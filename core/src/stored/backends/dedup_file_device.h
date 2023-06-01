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
  // this only works if only complete blocks are written/read to this device
  //   - ansi labels ?
  //   - this still does not address the issue of record headers
  // a header block looks like this:
  // offset to next header block (0 if no next header block)
  // offset to prev header block (0 if no prev header block)
  // some padding
  // header-1
  // header-2
  // ...
  // header-n
  // similary a data block looks like this:
  // offset to next data block (0 if no next data block)
  // offset to prev data block (0 if no prev data block)
  // some padding
  // data-1
  // data-2
  // ...
  // data-n

  // the file will then look something like this:
  //     +--------------+
  //  +- | header-block |
  //  |  | data-block   |-+
  //  |  | data-block   |-+
  //  +- | header-block | |
  //     | data-block   |-+
  //     | unallocated  |
  //     | ...          |
  //     +--------------+
 public:
  dedup_file_device() = default;
  ~dedup_file_device() { close(nullptr); }

  // Interface from Device
  SeekMode GetSeekMode() const override { return SeekMode::BYTES; }
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
 private:
  bool mounted{false};
  bool open{false};
  size_t blocksize{0};
};

} /* namespace storagedaemon */

#endif  // BAREOS_STORED_BACKENDS_DEDUP_FILE_DEVICE_H_
