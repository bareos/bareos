/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2023-2024 Bareos GmbH & Co. KG

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
#include "stored/stored_globals.h"
#include "stored/sd_backends.h"
#include "stored/device_control_record.h"
#include "lib/berrno.h"
#include "lib/util.h"
#include "lib/edit.h"
#include "lib/network_order.h"
#include "dedup_device.h"
#include "dedup/device_options.h"

#include <unistd.h>
#include <utility>
#include <stdexcept>
#include <cstring>

namespace storagedaemon {

namespace {
constexpr std::uint64_t block_number(std::uint32_t rfile, std::uint32_t rblock)
{
  std::uint64_t block_num = static_cast<std::uint64_t>(rfile) << 32 | rblock;
  return block_num;
}

constexpr bool check_open_mode(DeviceMode open_mode)
{
  switch (open_mode) {
    case DeviceMode::CREATE_READ_WRITE:
    case DeviceMode::OPEN_READ_WRITE:
    case DeviceMode::OPEN_READ_ONLY:
      return true;
      // write only is not possible
    case DeviceMode::OPEN_WRITE_ONLY:
    default:
      return false;
  }
}
};  // namespace

// Mount the device. Timeout is ignored.
bool dedup_device::MountBackend(DeviceControlRecord*, int)
{
  bool was_mounted = std::exchange(mounted, true);
  return !was_mounted;
}

// Unmount the device. Timeout is ignored.
bool dedup_device::UnmountBackend(DeviceControlRecord*, int)
{
  bool was_mounted = std::exchange(mounted, false);
  return was_mounted;
}

bool dedup_device::ScanForVolumeImpl(DeviceControlRecord* dcr)
{
  (void)dcr;
  return false;
}

int dedup_device::d_open(const char* path, int, int mode)
{
  if (openvol) {
    Emsg0(M_ERROR, 0, T_("Volume %s is opened already on this device.\n"),
          openvol->path());
    return -1;
  }

  if (!check_open_mode(open_mode)) {
    Emsg0(M_ABORT, 0, T_("Illegal mode given to open dev. (mode = %d)\n"),
          (int)open_mode);
    return -1;
  }

  bool read_only = open_mode == DeviceMode::OPEN_READ_ONLY;

  try {
    auto parsed = dedup::device_option_parser::parse(dev_options ?: "");

    for (auto& warning : parsed.warnings) {
      Emsg0(M_WARNING, 0, "Dedup device option warning: %s\n", warning.c_str());
    }

    auto& vol = (read_only)
                    ? openvol.emplace(dedup::volume::open_type::ReadOnly, mode,
                                      path, parsed.options.blocksize)
                    : openvol.emplace(dedup::volume::open_type::ReadWrite, mode,
                                      path, parsed.options.blocksize);

    return vol.fileno();
  } catch (const std::exception& ex) {
    Emsg0(M_ERROR, 0, T_("Could not open volume. ERR=%s\n"), ex.what());
    return -1;
  }
}

class chunked_reader {
 public:
  chunked_reader(const void* data, std::size_t size)
      : begin{(const char*)data}, end{begin + size}
  {
  }

  const char* get(std::size_t size)
  {
    ASSERT(begin < end);
    if (static_cast<std::size_t>(end - begin) < size) { return nullptr; }

    begin += size;
    return begin - size;
  }

  bool read(void* mem, std::size_t size)
  {
    ASSERT(begin < end);
    if (static_cast<std::size_t>(end - begin) < size) { return false; }

    std::memcpy(mem, begin, size);
    begin += size;

    return true;
  }

  bool finished() const { return begin == end; }
  std::size_t leftover() const { return end - begin; }

 private:
  const char* begin;
  const char* end;
};

namespace {
using net_u64 = network_order::network<std::uint64_t>;
using net_i64 = network_order::network<std::int64_t>;
using net_u32 = network_order::network<std::uint32_t>;
using net_i32 = network_order::network<std::int32_t>;
using net_u16 = network_order::network<std::uint16_t>;
using net_u8 = std::uint8_t;

struct block_header {
  net_u32 CheckSum;       /* Block check sum */
  net_u32 BlockSize;      /* Block byte size including the header */
  net_u32 BlockNumber;    /* Block number */
  char ID[4];             /* Identification and block level */
  net_u32 VolSessionId;   /* Session Id for Job */
  net_u32 VolSessionTime; /* Session Time for Job */

  std::size_t size() const { return BlockSize.load(); }
};

struct record_header {
  net_i32 FileIndex; /* File index supplied by File daemon */
  net_i32 Stream;    /* Stream number supplied by File daemon */
  net_u32 DataSize;  /* size of following data record in bytes */

  std::size_t size() const { return DataSize.load(); }
};

};  // namespace

ssize_t dedup_device::d_write(int fd, const void* data, size_t size)
{
  if (!openvol) {
    Emsg0(M_ERROR, 0, T_("Trying to write dedup volume when none are open.\n"));
    return -1;
  }

  if (openvol->fileno() != fd) {
    Emsg0(M_ERROR, 0,
          T_("Trying to write dedup volume that is not open "
             "(open = %d, trying to write = %d).\n"),
          openvol->fileno(), fd);
    return -1;
  }

  auto save = openvol->begin();

  try {
    chunked_reader stream{data, size};
    ssize_t datawritten = 0;

    while (!stream.finished()) {
      block_header block;
      if (!stream.read(&block, sizeof(block))) {
        throw std::runtime_error("Could not read block header from stream.");
      }

      // openvol->append_block(block);
      datawritten += sizeof(block);

      if (auto* block_data = stream.get(block.size()); !block_data) {
        throw std::runtime_error("Could not read block data from stream.");
      } else {
        chunked_reader records{block_data, block.size()};

        while (!records.finished()) {
          record_header record;
          if (!records.read(&record, sizeof(record))) {
            throw std::runtime_error(
                "Could not read record header from stream.");
          }

          // openvol->append_record(record);
          datawritten += sizeof(record);

          if (auto* record_data = records.get(record.size()); !record_data) {
            throw std::runtime_error("Could not read record data from stream.");
          } else {
            // openvol->append_data(record_data, record.size());
            datawritten += sizeof(record.size());
          }
        }
      }
    }


    openvol->commit(save);
    return datawritten;
  } catch (const std::exception& ex) {
    Emsg0(M_ERROR, 0,
          T_("Encountered error while trying to write to volume %s. ERR=%s\n"),
          openvol->path(), ex.what());
    openvol->abort(save);
    return -1;
  }
}

ssize_t dedup_device::d_read(int fd, void* data, size_t size)
{
  (void)fd;
  (void)data;
  (void)size;

  return -1;
}

int dedup_device::d_close(int fd)
{
  if (!openvol) {
    Emsg0(M_ERROR, 0, T_("Trying to close dedup volume when none are open.\n"));
    return -1;
  }

  if (openvol->fileno() != fd) {
    Emsg0(M_ERROR, 0,
          T_("Trying to close dedup volume that is not open "
             "(open = %d, trying to close = %d).\n"),
          openvol->fileno(), fd);
    return -1;
  }

  std::string volname = openvol->path();
  try {
    openvol.reset();
    return 0;
  } catch (const std::exception& ex) {
    Emsg0(M_ERROR, 0, T_("Could not close dedup device %s. ERR=%s\n"),
          volname.c_str(), ex.what());
    return -1;
  }
}

int dedup_device::d_ioctl(int, ioctl_req_t, char*) { return -1; }

boffset_t dedup_device::d_lseek(DeviceControlRecord*, boffset_t, int)
{
  return -1;
}

bool dedup_device::d_truncate(DeviceControlRecord* dcr)
{
  (void)dcr;
  return false;
}

std::size_t dedup_device::current_block()
{
  return block_number(file, block_num);
}

bool dedup_device::rewind(DeviceControlRecord* dcr)
{
  if (!openvol) {
    Emsg0(M_ERROR, 0,
          T_("Trying to rewind dedup volume when none are open.\n"));
    return false;
  }
  block_num = 0;
  file = 0;
  return UpdatePos(dcr);
}

bool dedup_device::UpdatePos(DeviceControlRecord*)
{
  // the volume itself is stateless
  // so we have nothing to do
  return true;
}

bool dedup_device::Reposition(DeviceControlRecord* dcr,
                              uint32_t rfile,
                              uint32_t rblock)
{
  if (!openvol) {
    Emsg0(M_ERROR, 0,
          T_("Trying to reposition dedup volume when none are open.\n"));
    return false;
  }
  Dmsg2(10, "file: %u -> %u; block: %u -> %u\n", file, rfile, block_num,
        rblock);
  block_num = rblock;
  file = rfile;
  return UpdatePos(dcr);
}

bool dedup_device::eod(DeviceControlRecord* dcr)
{
  (void)dcr;
  return false;
}

bool dedup_device::d_flush(DeviceControlRecord*) { return false; }

REGISTER_SD_BACKEND(dedup, dedup_device);

} /* namespace storagedaemon  */
