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
#include <unordered_map>
#include <fmt/format.h>
#include <gsl/gsl>
#include "util.h"

namespace utl = backends::util;

namespace {
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
    {"chunksize", "10485760"},  // 10 MB
    {"iothreads", "0"},
    {"ioslots", "10"},
    {"retries", "0"},
    //{"mmap", "0"},
};

namespace detail {
class class_with_a_really_long_name_that_you_should_not_use {};
}  // namespace detail

template <class T> struct dependent_false : std::false_type {};

template <>
struct dependent_false<
    detail::class_with_a_really_long_name_that_you_should_not_use>
    : std::true_type {};

class OptionConsumer {
 private:
  utl::options& options;
  POOLMEM*& errmsg;

  template <typename T> void convert_value(T&, const std::string&)
  {
    static_assert(dependent_false<T>::value,
                  "missing specialization for this type");
  }

 public:
  OptionConsumer(utl::options& t_options, POOLMEM*& t_errmsg)
      : options(t_options), errmsg(t_errmsg)
  {
  }

  std::optional<std::string> fetch_value(const std::string& key)
  {
    auto node_handle = options.extract(key);
    if (node_handle.empty()) { return std::nullopt; }
    return node_handle.mapped();
  }

  template <typename T> bool convert(const std::string& key, T& target)
  {
    auto node_handle = options.extract(key);
    if (node_handle.empty()) {
      Mmsg0(errmsg, "no value provided for option '%s'\n", key.c_str());
      return false;
    }
    auto value = node_handle.mapped();

    try {
      convert_value(target, value);
    } catch (std::invalid_argument& e) {
      Mmsg0(errmsg, "invalid argument '%s' for option '%s': %s\n",
            value.c_str(), key.c_str(), e.what());
      return false;
    } catch (std::out_of_range& e) {
      Mmsg0(errmsg, "value '%s' for option '%s' is out of range: %s\n",
            value.c_str(), key.c_str(), e.what());
      return false;
    } catch (gsl::narrowing_error& e) {
      Mmsg0(errmsg, "value '%s' for option '%s' would be truncated: %s\n",
            value.c_str(), key.c_str(), e.what());
      return false;
    }
    return true;
  }
};

template <>
void OptionConsumer::convert_value<>(unsigned long& to, const std::string& from)
{
  to = std::stoul(from);
}

template <>
void OptionConsumer::convert_value<>(uint8_t& to, const std::string& from)
{
  to = gsl::narrow<uint8_t>(std::stoul(from));
}

template <>
void OptionConsumer::convert_value<>(std::string& to, const std::string& from)
{
  to = from;
}


}  // namespace

namespace storagedaemon {

bool DropletCompatibleDevice::setup()
{
  if (m_setup_succeeded) { return true; }
  auto res = utl::parse_options(dev_options);
  if (std::holds_alternative<utl::error>(res)) {
    Mmsg0(errmsg, "device option error: %s\n",
          std::get<utl::error>(res).c_str());
    Emsg0(M_FATAL, 0, errmsg);
    return false;
  }
  auto options = std::get<utl::options>(res);

  // apply default values
  options.merge(utl::options(option_defaults));

  Dmsg0(200, "dev_options: %s\n", dev_options);
  for (const auto& [key, value] : options) {
    Dmsg0(200, "'%s' = '%s'\n", key.c_str(), value.c_str());
  }

  OptionConsumer oc{options, errmsg};
  std::string program;
  if (!oc.convert("iothreads", io_threads_) || !oc.convert("ioslots", io_slots_)
      || !oc.convert("retries", retries_)
      || !oc.convert("chunksize", chunk_size_)
      || !oc.convert("program", program)) {
    Emsg0(M_FATAL, 0, errmsg);
    return false;
  }
  if (program.empty()) {
    Mmsg0(errmsg, "option 'program' is required");
    Emsg0(M_FATAL, 0, errmsg);
    return false;
  }

  if (!m_storage.set_program(program)) {
    Mmsg0(errmsg,
          "program '%s' is not usable, provide the absolute path or make sure "
          "it is in your Scripts Directory.\n",
          program.c_str());
    Emsg0(M_FATAL, 0, errmsg);
    return false;
  }

  auto supported_options = m_storage.get_supported_options();
  for (const auto& option_name : supported_options) {
    if (auto value = oc.fetch_value(option_name)) {
      if (!m_storage.set_option(option_name, *value)) {
        Mmsg0(errmsg, "error setting option '%s' to '%s'\n",
              option_name.c_str(), value->c_str());
        Emsg0(M_FATAL, 0, errmsg);
        return false;
      }
    }
  }

  // OptionConsumer should have consumed all options at this point
  if (!options.empty()) {
    std::vector<std::string> option_names;
    for ( const auto& [name, value]: options) {
      option_names.push_back(name);
    }
    auto excess_options = fmt::format("{}", fmt::join(option_names, ", "));
    Mmsg0(errmsg, "unknown options encountered: %s\n", excess_options.c_str());
    Emsg0(M_FATAL, 0, errmsg);
    return false;
  }

  return m_setup_succeeded = true;
}

bool DropletCompatibleDevice::CheckRemoteConnection()
{
  Dmsg0(200, "CheckRemoteConnection called\n");
  return setup() && m_storage.test_connection();
}

bool DropletCompatibleDevice::FlushRemoteChunk(chunk_io_request* request)
{
  const std::string obj_name = request->volname;
  const std::string obj_chunk = get_chunk_name(request);
  Dmsg1(100, "Flushing chunk %s/%s\n", obj_name.c_str(), obj_chunk.c_str());

  auto inflight_lease = getInflightLease(request);
  if (!inflight_lease) {
    Dmsg0(100, "Could not acquire inflight lease for %s %s\n", obj_name.c_str(),
          obj_chunk.c_str());
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
    Dmsg1(100,
          "Not uploading chunk %s with size %d, as chunk with size %d is "
          "already present\n",
          obj_name.c_str(), obj_stat->size, request->wbuflen);
    return true;
  }
  // FIXME more error handling here!
  auto obj_data = gsl::span{request->buffer, request->wbuflen};
  Dmsg1(5, "Uploading %zu bytes of data\n", request->wbuflen);
  if (m_storage.upload(obj_name, obj_chunk, obj_data)) { return true; }
  return false;
}

// Internal method for reading a chunk from the remote backing store.
bool DropletCompatibleDevice::ReadRemoteChunk(chunk_io_request* request)
{
  const std::string obj_name = request->volname;
  const std::string obj_chunk = get_chunk_name(request);
  Dmsg1(100, "Reading chunk %s\n", obj_name.c_str());

  // check object metadata
  auto obj_stat = m_storage.stat(obj_name, obj_chunk);
  if (!obj_stat) {
    Mmsg1(errmsg, T_("Failed to open %s/%s doesn't exist\n"), obj_name.c_str(),
          obj_chunk.c_str());
    Dmsg1(100, "%s", errmsg);
    dev_errno = EIO;
    return false;
  } else if (obj_stat->size > request->wbuflen) {
    Mmsg3(errmsg,
          T_("Failed to read %s (%ld) to big to fit in chunksize of %ld "
             "bytes\n"),
          obj_name.c_str(), obj_stat->size, request->wbuflen);
    Dmsg1(100, "%s", errmsg);
    dev_errno = EINVAL;
    return false;
  }

  if (auto obj_data = m_storage.download(obj_name, obj_chunk,
                                         {request->buffer, obj_stat->size})) {
    *request->rbuflen = obj_data->size_bytes();
    return true;
  } else {
    Mmsg1(errmsg, T_("Failed to read %s/%s\n"), obj_name.c_str(),
          obj_chunk.c_str());
    Dmsg1(100, "%s", errmsg);
    dev_errno = EIO;
    return false;
  }
}

bool DropletCompatibleDevice::TruncateRemoteVolume(DeviceControlRecord*)
{
  const char* vol_name = getVolCatName();
  const auto list = m_storage.list(vol_name);
  for (const auto& [chunk_name, stat] : list) {
    if (is_chunk_name(chunk_name)) {
      if (!m_storage.remove(vol_name, chunk_name)) { return false; }
    }
  }
  return true;
}

ssize_t DropletCompatibleDevice::RemoteVolumeSize()
{
  const auto list = m_storage.list(getVolCatName());
  if (list.empty()) { return -1; }
  ssize_t total_size{0};
  for (const auto& [name, stat] : list) {
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
    Emsg0(M_FATAL, 0, errmsg);
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

      Dmsg1(100, "Current volumesize: %lld\n", volumesize);

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
