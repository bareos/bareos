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
#include "dedup_device.h"
#include "dedup/device_options.h"

#include <unistd.h>
#include <utility>
#include <stdexcept>

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

ssize_t dedup_device::d_write(int fd, const void* data, size_t size)
{
  (void)fd;
  (void)data;
  (void)size;
  return -1;
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
    Emsg0(M_ERROR, 0, T_("Trying to close dedup volume when non are open.\n"));
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

bool dedup_device::rewind(DeviceControlRecord* dcr)
{
  (void)dcr;
  return false;
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
  (void)dcr;
  (void)rfile;
  (void)rblock;
  return false;
}

bool dedup_device::eod(DeviceControlRecord* dcr)
{
  (void)dcr;
  return false;
}

bool dedup_device::d_flush(DeviceControlRecord*) { return false; }

REGISTER_SD_BACKEND(dedup, dedup_device);

} /* namespace storagedaemon  */
