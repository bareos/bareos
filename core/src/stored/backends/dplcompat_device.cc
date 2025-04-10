/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2024-2024 Bareos GmbH & Co. KG

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
*/

#include "include/bareos.h"

#include "stored/stored.h"
#include "stored/sd_backends.h"
#include "chunked_device.h"
#include "lib/edit.h"
#include "dplcompat_device.h"

#include <string>
#include <optional>
#include <fmt/format.h>
#include <gsl/gsl>
#include "util.h"

namespace utl = backends::util;
using namespace std::literals::string_literals;

namespace {
static constexpr int debug_info = 100;
static constexpr int debug_trace = 120;

std::string get_chunk_name(storagedaemon::chunk_io_request* request)
{
  return fmt::format(FMT_STRING("{:04d}"), request->chunk);
}
bool is_chunk_name(const std::string& name)
{
  if (name.length() != 4) { return false; }
  for (char c : name) {
    if (c < '0' || c > '9') { return false; }
  }
  return true;
}

static const utl::options option_defaults{
    {"chunksize", "10 MB"}, {"iothreads", "0"},       {"ioslots", "10"},
    {"retries", "0"},       {"program_timeout", "0"},  // default in
                                                       // crud_storage
};

unsigned long long stoull_notrailing(const std::string& str)
{
  size_t pos;
  unsigned long long val = std::stoull(str, &pos);
  if (!std::all_of(str.begin() + pos, str.end(), b_isjunkchar)) {
    throw std::invalid_argument{"unparseable trailing characters"};
  }
  return val;
}

unsigned long stoul_notrailing(const std::string& str)
{
  size_t pos;
  unsigned long val = std::stoul(str, &pos);
  if (!std::all_of(str.begin() + pos, str.end(), b_isjunkchar)) {
    throw std::invalid_argument{"unparseable trailing characters"};
  }
  return val;
}

// delete this, so only specializations will be considered
template <typename T> void convert_value(T&, const std::string&) = delete;

template <>
[[maybe_unused]] void convert_value<>(unsigned long long& to,
                                      const std::string& from)
{
  to = stoull_notrailing(from);
}

template <>
[[maybe_unused]] void convert_value<>(unsigned long& to,
                                      const std::string& from)
{
  to = stoul_notrailing(from);
}

template <> void convert_value<>(uint8_t& to, const std::string& from)
{
  to = gsl::narrow<uint8_t>(stoul_notrailing(from));
}

template <> void convert_value<>(uint32_t& to, const std::string& from)
{
  to = gsl::narrow<uint32_t>(stoul_notrailing(from));
}


template <> void convert_value<>(std::string& to, const std::string& from)
{
  to = from;
}

void convert_size(uint64_t& to, const std::string& from)
{
  if (!size_to_uint64(from.c_str(), &to)) {
    throw std::invalid_argument("Hello, World!");
  }
}

template <typename T>
tl::expected<utl::options*, std::string> convert(
    utl::options* options,
    const std::string& key,
    T& target,
    std::function<void(T&, const std::string&)> converter = convert_value<T>)
{
  auto node_handle = options->extract(key);
  if (node_handle.empty()) {
    return tl::unexpected(
        fmt::format("no value provided for option '{}'\n", key));
  }
  auto value = node_handle.mapped();

  try {
    converter(target, value);
  } catch (std::invalid_argument& e) {
    return tl::unexpected(fmt::format(
        "invalid argument '{}' for option '{}': {}\n", value, key, e.what()));
  } catch (std::out_of_range& e) {
    return tl::unexpected(
        fmt::format("value '{}' for option '{}' is out of range: {}\n", value,
                    key, e.what()));
  } catch (gsl::narrowing_error& e) {
    return tl::unexpected(
        fmt::format("value '{}' for option '{}' would be truncated: {}\n",
                    value, key, e.what()));
  }
  return options;
}

std::optional<std::string> fetch_value(utl::options& options,
                                       const std::string& key)
{
  auto node_handle = options.extract(key);
  if (node_handle.empty()) { return std::nullopt; }
  return node_handle.mapped();
}

template <typename T>
auto get_value_converter(const std::string& key, T& target)
{
  return [&key, &target](utl::options* options) {
    return convert(options, key, target);
  };
}

template <typename T> auto get_size_converter(const std::string& key, T& target)
{
  return [&key, &target](utl::options* options) {
    return convert(options, key, target, std::function{convert_size});
  };
}


}  // namespace

namespace storagedaemon {

bool DropletCompatibleDevice::setup()
{
  if (m_setup_succeeded) { return true; }
  if (auto result = setup_impl()) {
    return m_setup_succeeded = true;
  } else {
    PmStrcpy(errmsg, result.error().c_str());
    Emsg0(M_FATAL, 0, errmsg);
    return false;
  }
}

tl::expected<void, std::string> DropletCompatibleDevice::setup_impl()
{
  auto res = utl::parse_options(dev_options);
  if (std::holds_alternative<utl::error>(res)) {
    return tl::unexpected(
        fmt::format("device option error: {}\n", std::get<utl::error>(res)));
  }
  auto options = std::get<utl::options>(res);

  // apply default values
  options.merge(utl::options(option_defaults));

  Dmsg0(debug_info, "dev_options: %s\n", dev_options);
  for (const auto& [key, value] : options) {
    Dmsg0(debug_trace, "'%s' = '%s'\n", key.c_str(), value.c_str());
  }

  std::string program;
  uint32_t program_timeout{0};

  if (auto conversion_result
      = tl::expected<utl::options*, std::string>{&options}
            .and_then(get_value_converter("iothreads", io_threads_))
            .and_then(get_value_converter("ioslots", io_slots_))
            .and_then(get_value_converter("retries", retries_))
            .and_then(get_size_converter("chunksize", chunk_size_))
            .and_then(get_value_converter("program", program))
            .and_then(get_value_converter("program_timeout", program_timeout));
      !conversion_result) {
    return tl::unexpected(conversion_result.error());
  }

  if (program.empty()) {
    return tl::unexpected("Option 'program' is required\n"s);
  }

  Dmsg0(debug_trace, "configured chunksize in bytes: %llu\n", chunk_size_);

  if (auto result = m_storage.set_program(program); !result) { return result; }

  if (program_timeout > 0) {
    m_storage.set_program_timeout(std::chrono::seconds{program_timeout});
  }

  if (auto supported_options = m_storage.get_supported_options()) {
    for (const auto& option_name : *supported_options) {
      if (auto value = fetch_value(options, option_name);
          value && !m_storage.set_option(option_name, *value)) {
        return tl::unexpected(fmt::format("Error setting option '{}' to '{}'\n",
                                          option_name, *value));
      }
    }
  } else {
    return tl::unexpected(
        fmt::format("Cannot get supported options.\nCause: {}\n",
                    supported_options.error()));
  }

  // OptionConsumer should have consumed all options at this point
  if (!options.empty()) {
    BStringList option_names;
    for (const auto& [name, value] : options) { option_names.push_back(name); }
    return tl::unexpected(fmt::format("Unknown options encountered: {}\n",
                                      option_names.Join(", ")));
  }
  return {};
}

bool DropletCompatibleDevice::CheckRemoteConnection()
{
  Dmsg0(debug_trace, "CheckRemoteConnection called\n");
  return setup() && m_storage.test_connection();
}

bool DropletCompatibleDevice::FlushRemoteChunk(chunk_io_request* request)
{
  const std::string_view obj_name{request->volname};
  const std::string obj_chunk = get_chunk_name(request);
  if (request->wbuflen == 0) {
    Dmsg1(debug_info, "Not flushing empty chunk %s/%s\n", obj_name.data(),
          obj_chunk.c_str());
    return true;
  }
  Dmsg1(debug_trace, "Flushing chunk %s/%s\n", obj_name.data(),
        obj_chunk.c_str());

  auto inflight_lease = getInflightLease(request);
  if (!inflight_lease) {
    Dmsg0(debug_info, "Could not acquire inflight lease for %s %s\n",
          obj_name.data(), obj_chunk.c_str());
    return false;
  }


  /* Check on the remote backing store if the chunk already exists.
   * We only upload this chunk if it is bigger then the chunk that exists
   * on the remote backing store. When using io-threads it could happen
   * that there are multiple flush requests for the same chunk when a
   * chunk is reused in a next backup job. We only want the chunk with
   * the biggest amount of valid data to persist as we only append to
   * chunks. */
  auto obj_stat = m_storage.stat(obj_name, obj_chunk);

  if (obj_stat && obj_stat->size > request->wbuflen) {
    Dmsg1(debug_info,
          "Not uploading chunk %s with size %d, as chunk with size %d is "
          "already present\n",
          obj_name.data(), obj_stat->size, request->wbuflen);
    return true;
  }

  auto obj_data = gsl::span{request->buffer, request->wbuflen};
  Dmsg1(debug_info, "Uploading %zu bytes of data\n", request->wbuflen);
  if (auto result = m_storage.upload(obj_name, obj_chunk, obj_data)) {
    return true;
  } else {
    PmStrcpy(errmsg, result.error().c_str());
    dev_errno = EIO;
    return false;
  }
}

// Internal method for reading a chunk from the remote backing store.
bool DropletCompatibleDevice::ReadRemoteChunk(chunk_io_request* request)
{
  const std::string_view obj_name{request->volname};
  const std::string obj_chunk = get_chunk_name(request);
  Dmsg1(debug_trace, "Reading chunk %s/%s\n", obj_name.data(),
        obj_chunk.data());

  // check object metadata
  auto obj_stat = m_storage.stat(obj_name, obj_chunk);
  if (!obj_stat) {
    PmStrcpy(errmsg, obj_stat.error().c_str());
    Dmsg1(debug_info, "%s", errmsg);
    dev_errno = EIO;
    return false;
  } else if (obj_stat->size > request->wbuflen) {
    Mmsg3(errmsg,
          T_("Failed to read %s (%ld) to big to fit in chunksize of %ld "
             "bytes\n"),
          obj_name.data(), obj_stat->size, request->wbuflen);
    Dmsg1(debug_info, "%s", errmsg);
    dev_errno = EINVAL;
    return false;
  }

  if (auto obj_data = m_storage.download(obj_name, obj_chunk,
                                         {request->buffer, obj_stat->size})) {
    *request->rbuflen = obj_data->size_bytes();
    return true;
  } else {
    PmStrcpy(errmsg, obj_data.error().c_str());
    Dmsg1(debug_info, "%s", errmsg);
    dev_errno = EIO;
    return false;
  }
}

bool DropletCompatibleDevice::TruncateRemoteVolume(DeviceControlRecord*)
{
  const char* vol_name = getVolCatName();
  const auto chunk_map = m_storage.list(vol_name);
  if (!chunk_map) {
    PmStrcpy(errmsg, chunk_map.error().c_str());
    dev_errno = EIO;
    return false;
  }
  for (const auto& [chunk_name, stat] : *chunk_map) {
    if (is_chunk_name(chunk_name)) {
      if (auto res = m_storage.remove(vol_name, chunk_name); !res) {
        PmStrcpy(errmsg, chunk_map.error().c_str());
        dev_errno = EIO;
        return false;
      }
    }
  }
  return true;
}

ssize_t DropletCompatibleDevice::RemoteVolumeSize()
{
  const auto chunk_map = m_storage.list(getVolCatName());
  if (!chunk_map) {
    PmStrcpy(errmsg, chunk_map.error().c_str());
    dev_errno = EIO;
    return false;
  }
  if (chunk_map->empty()) { return -1; }
  ssize_t total_size{0};
  for (const auto& [name, stat] : *chunk_map) {
    if (is_chunk_name(name)) { total_size += stat.size; }
  }
  return total_size;
}


bool DropletCompatibleDevice::d_flush(DeviceControlRecord*)
{
  return WaitUntilChunksWritten();
};

int DropletCompatibleDevice::d_open(const char* pathname, int flags, int mode)
{
  if (!setup()) {
    dev_errno = EIO;
    Emsg0(M_FATAL, 0, "%s", errmsg);
  }
  return SetupChunk(pathname, flags, mode);
}

ssize_t DropletCompatibleDevice::d_read(int t_fd, void* buffer, size_t count)
{
  return ReadChunked(t_fd, buffer, count);
}

ssize_t DropletCompatibleDevice::d_write(int t_fd,
                                         const void* buffer,
                                         size_t count)
{
  return WriteChunked(t_fd, buffer, count);
}

int DropletCompatibleDevice::d_close(int) { return CloseChunk(); }

int DropletCompatibleDevice::d_ioctl(int, ioctl_req_t, char*) { return -1; }

boffset_t DropletCompatibleDevice::d_lseek(DeviceControlRecord*,
                                           boffset_t offset,
                                           int whence)
{
  switch (whence) {
    case SEEK_SET:
      offset_ = offset;
      break;
    case SEEK_CUR:
      offset_ += offset;
      break;
    case SEEK_END: {
      ssize_t volumesize;

      volumesize = ChunkedVolumeSize();

      Dmsg1(debug_info, "Current volumesize: %lld\n", volumesize);

      if (volumesize >= 0) {
        offset_ = volumesize + offset;
      } else {
        return -1;
      }
      break;
    }
    default:
      return -1;
  }

  if (!LoadChunk()) { return -1; }

  return offset_;
}

bool DropletCompatibleDevice::d_truncate(DeviceControlRecord* dcr)
{
  return TruncateChunkedVolume(dcr);
}

REGISTER_SD_BACKEND(dplcompat, DropletCompatibleDevice)

} /* namespace storagedaemon */
