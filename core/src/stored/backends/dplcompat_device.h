/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2024-2024 Bareos GmbH & Co. KG

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

#ifndef BAREOS_STORED_BACKENDS_DPLCOMPAT_DEVICE_H_
#define BAREOS_STORED_BACKENDS_DPLCOMPAT_DEVICE_H_

#include "chunked_device.h"
#include <gsl/span>
#include <map>
#include "crud_storage.h"

namespace storagedaemon {

// refactor to own file

// end refactor to own file

class DropletCompatibleDevice : public ChunkedDevice {
 private:
  /* maximun number of chunks in a volume (0000 to 9999) */
  static constexpr int max_chunks_ = 10000;
  CrudStorage m_storage;

  // Interface from ChunkedDevice
  bool CheckRemoteConnection() override;
  bool FlushRemoteChunk(chunk_io_request* request) override;
  bool ReadRemoteChunk(chunk_io_request* request) override;
  ssize_t RemoteVolumeSize() override;
  bool TruncateRemoteVolume(DeviceControlRecord* dcr) override;

 public:

  // Interface from Device
  SeekMode GetSeekMode() const override { return SeekMode::BYTES; }
  bool CanReadConcurrently() const override { return true; }
  int d_close(int fd) override;
  int d_open(const char* pathname, int flags, int mode) override;
  int d_ioctl(int fd, ioctl_req_t request, char* mt = NULL) override;
  boffset_t d_lseek(DeviceControlRecord* dcr,
                    boffset_t offset,
                    int whence) override;
  ssize_t d_read(int fd, void* buffer, size_t count) override;
  ssize_t d_write(int fd, const void* buffer, size_t count) override;
  bool d_truncate(DeviceControlRecord* dcr) override;
  bool d_flush(DeviceControlRecord* dcr) override;
};
} /* namespace storagedaemon */
#endif  // BAREOS_STORED_BACKENDS_DPLCOMPAT_DEVICE_H_
