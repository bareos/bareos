/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2023-2023 Bareos GmbH & Co. KG

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
#include "dedup_file_device.h"
#include "lib/berrno.h"
#include "lib/util.h"

#include <utility>

namespace storagedaemon {

/**
 * Mount the device.
 *
 * If timeout, wait until the mount command returns 0.
 * If !timeout, try to mount the device only once.
 */
bool dedup_file_device::MountBackend(DeviceControlRecord*, int)
{
  bool was_mounted = std::exchange(mounted, true);
  return !was_mounted;
}

/**
 * Unmount the device
 *
 * If timeout, wait until the unmount command returns 0.
 * If !timeout, try to unmount the device only once.
 */
bool dedup_file_device::UnmountBackend(DeviceControlRecord*, int)
{
  bool was_mounted = std::exchange(mounted, false);
  return was_mounted;
}

bool dedup_file_device::ScanForVolumeImpl(DeviceControlRecord* dcr)
{
  return ScanDirectoryForVolume(dcr);
}

int dedup_file_device::d_open(const char*, int, int)
{
  bool was_open = std::exchange(open, true);
  if (was_open) return -1;

  return 5;
}

ssize_t dedup_file_device::d_read(int fd, void*, size_t)
{
  if (fd != 5) return -1;
  return 0;
}

ssize_t dedup_file_device::d_write(int fd, const void*, size_t size)
{
  if (fd != 5) return -1;
  if (blocksize != size) {
    Dmsg2(10, "Blocksize was %lu; now its %lu\n", blocksize, size);
    blocksize = size;
  } else {
    Dmsg2(10, "Blocksize staying at %lu\n", blocksize);
  }
  return 0;
}

int dedup_file_device::d_close(int fd) {
  if (fd != 5) return -1;
  bool was_open = std::exchange(open, false);
  return was_open;
}

int dedup_file_device::d_ioctl(int, ioctl_req_t, char*) { return -1; }

boffset_t dedup_file_device::d_lseek(DeviceControlRecord*,
				     boffset_t,
				     int)
{
  return -1;
}

bool dedup_file_device::d_truncate(DeviceControlRecord*)
{
  return true;
}

REGISTER_SD_BACKEND(dedup, dedup_file_device);

} /* namespace storagedaemon  */
