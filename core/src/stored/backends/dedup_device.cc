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
#include "dedup/util.h"

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

    if (open_mode == DeviceMode::CREATE_READ_WRITE) {
      // if the folder already exists, it will do nothing
      dedup::volume::create_new(mode, path, parsed.options.blocksize);
    }

    auto& vol
        = (read_only)
              ? openvol.emplace(dedup::volume::open_type::ReadOnly, path)
              : openvol.emplace(dedup::volume::open_type::ReadWrite, path);

    return vol.fileno();
  } catch (const std::exception& ex) {
    Emsg0(M_ERROR, 0, T_("Could not open volume. ERR=%s\n"), ex.what());
    return -1;
  }
}

ssize_t dedup_device::d_write(int fd, const void* data, size_t size)
{
  using block_header = dedup::block_header;
  using record_header = dedup::record_header;
  using chunked_reader = dedup::chunked_reader;

  struct raii_save_state {
    dedup::volume* vol;
    dedup::save_state save;

    raii_save_state(dedup::volume& vol) : vol{&vol}, save{vol.BeginBlock()} {}

    void commit(block_header hdr)
    {
      vol->CommitBlock(save, hdr);
      vol = nullptr;
    }

    ~raii_save_state()
    {
      if (vol) {
        vol->AbortBlock(save);
        vol = nullptr;
      }
    }
  };
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

  if (current_block() == 0) {
    if (!ResetOpenVolume()) { return -1; }
  }

  std::size_t datawritten = 0;
  try {
    chunked_reader stream{data, size};

    while (!stream.finished()) {
      // when save goes out of scope without being commited, it will get
      // aborted
      auto save = raii_save_state(openvol.value());

      block_header block;
      if (!stream.read(&block, sizeof(block))) {
        throw std::runtime_error("Could not read block header from stream.");
      }

      auto blocksize = sizeof(block);

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

          blocksize += sizeof(record);

          if (auto* record_data = records.get(record.size()); !record_data) {
            throw std::runtime_error("Could not read record data from stream.");
          } else {
            openvol->PushRecord(record, record_data, record.size());
            blocksize += record.size();
          }
        }
      }

      save.commit(block);
      datawritten += blocksize;
    }

    return datawritten;
  } catch (const std::exception& ex) {
    Emsg0(M_ERROR, 0,
          T_("Encountered error while trying to write to volume %s. ERR=%s\n"),
          openvol->path(), ex.what());
    return datawritten;
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

bool dedup_device::d_flush(DeviceControlRecord*)
{
  if (!openvol) {
    Emsg0(M_ERROR, 0, T_("Trying to flush dedup volume when none are open.\n"));
    return false;
  }

  try {
    openvol->flush();
    return true;
  } catch (const std::exception& ex) {
    Emsg0(M_ERROR, 0, T_("Could not flush volume %s. ERR=%s\n"),
          openvol->path(), ex.what());
    (void)ex;
    return false;
  }
}

bool dedup_device::ResetOpenVolume()
{
  ASSERT(!!openvol);
  // FIXME(ssura): support secure erase command
  try {
    openvol->reset();
    return true;
  } catch (const std::exception& ex) {
    Emsg0(M_ERROR, 0,
          T_("Encountered error while trying to reset volume %s. ERR=%s\n"),
          openvol->path(), ex.what());
    return false;
  }
}

REGISTER_SD_BACKEND(dedup, dedup_device);

} /* namespace storagedaemon  */
