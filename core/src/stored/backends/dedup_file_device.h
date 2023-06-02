/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2023-2023 Bareos GmbH & Co. KG

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation, which is
   listed in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   Affero General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
*/

#ifndef BAREOS_STORED_BACKENDS_DEDUP_FILE_DEVICE_H_
#define BAREOS_STORED_BACKENDS_DEDUP_FILE_DEVICE_H_

#include "stored/dev.h"

namespace storagedaemon {
class dedup_file_device : public Device {
 public:
  dedup_file_device() = default;
  ~dedup_file_device() { close(nullptr); }

  // Interface from Device
  SeekMode GetSeekMode() const override { return SeekMode::FILE_BLOCK; }
  bool CanReadConcurrently() const override { return true; }
  bool MountBackend(DeviceControlRecord* dcr, int timeout) override;
  bool UnmountBackend(DeviceControlRecord* dcr, int timeout) override;
  bool ScanForVolumeImpl(DeviceControlRecord* dcr) override;
  int d_close(int) override;
  int d_open(const char* pathname, int flags, int mode) override;
  int d_ioctl(int fd, ioctl_req_t request, char* mt = NULL) override;
  boffset_t d_lseek(DeviceControlRecord* dcr,
                    boffset_t offset,
                    int whence) override;
  ssize_t d_read(int fd, void* buffer, size_t count) override;
  ssize_t d_write(int fd, const void* buffer, size_t count) override;
  bool d_truncate(DeviceControlRecord* dcr) override;
  bool rewind(DeviceControlRecord* dcr) override;
  bool UpdatePos(DeviceControlRecord* dcr) override;
  bool Reposition(DeviceControlRecord* dcr, uint32_t rfile, uint32_t rblock) override;
 private:
  bool mounted{false};

  int block_fd;
  int record_fd;
  int data_fd;
};

} /* namespace storagedaemon */

#endif  // BAREOS_STORED_BACKENDS_DEDUP_FILE_DEVICE_H_
