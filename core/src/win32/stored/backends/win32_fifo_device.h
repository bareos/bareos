/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2015-2015 Planets Communications B.V.
   Copyright (C) 2015-2020 Bareos GmbH & Co. KG

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
/*
 * WIN32 FIFO API device abstraction.
 *
 * Marco van Wieringen, June 2014
 */

#ifndef BAREOS_WIN32_STORED_BACKENDS_WIN32_FIFO_DEVICE_H_
#define BAREOS_WIN32_STORED_BACKENDS_WIN32_FIFO_DEVICE_H_

#include "stored/dev.h"
#include "lib/btimers.h"
#include "lib/util.h"

namespace storagedaemon {

class DeviceControlRecord;

class win32_fifo_device : public Device {
 public:
  win32_fifo_device() = default;
  ~win32_fifo_device() = default;

  /*
   * Interface from Device
   */
  void OpenDevice(DeviceControlRecord* dcr, DeviceMode omode) override;
  bool eod(DeviceControlRecord* dcr) override;
  bool MountBackend(DeviceControlRecord* dcr, int timeout) override;
  bool UnmountBackend(DeviceControlRecord* dcr, int timeout) override;
  int d_close(int) override;
  int d_open(const char* pathname, int flags, int mode) override;
  int d_ioctl(int fd, ioctl_req_t request, char* mt = NULL) override;
  boffset_t d_lseek(DeviceControlRecord* dcr,
                    boffset_t offset,
                    int whence) override;
  ssize_t d_read(int fd, void* buffer, size_t count) override;
  ssize_t d_write(int fd, const void* buffer, size_t count) override;
  bool d_truncate(DeviceControlRecord* dcr) override;
};

} /* namespace storagedaemon */
#endif /* BAREOS_WIN32_STORED_BACKENDS_WIN32_FIFO_DEVICE_H_ */
