/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2014-2014 Planets Communications B.V.
   Copyright (C) 2014-2021 Bareos GmbH & Co. KG

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
 * GENERIC Tape API device abstraction.
 *
 * Marco van Wieringen, June 2014
 */

#ifndef BAREOS_STORED_BACKENDS_GENERIC_TAPE_DEVICE_H_
#define BAREOS_STORED_BACKENDS_GENERIC_TAPE_DEVICE_H_

#include "stored/dev.h"

namespace storagedaemon {

class generic_tape_device : public Device {
 public:
  generic_tape_device() = default;
  virtual ~generic_tape_device() { close(nullptr); }

  // Interface from Device
  virtual void OpenDevice(DeviceControlRecord* dcr, DeviceMode omode) override;
  virtual char* StatusDev() override;
  virtual bool eod(DeviceControlRecord* dcr) override;
  virtual void SetAteof() override;
  virtual void SetAteot() override;
  virtual bool offline() override;
  virtual bool weof(int num) override;
  virtual bool fsf(int num) override;
  virtual bool bsf(int num) override;
  virtual bool fsr(int num) override;
  virtual bool bsr(int num) override;
  virtual bool LoadDev() override;
  virtual void LockDoor() override;
  virtual void UnlockDoor() override;
  virtual void clrerror(int func) override;
  virtual void SetOsDeviceParameters(DeviceControlRecord* dcr) override;
  virtual int32_t GetOsTapeFile() override;
  virtual bool rewind(DeviceControlRecord* dcr) override;
  virtual bool UpdatePos(DeviceControlRecord* dcr) override;
  virtual bool Reposition(DeviceControlRecord* dcr,
                          uint32_t rfile,
                          uint32_t rblock) override;
  virtual bool MountBackend(DeviceControlRecord* dcr, int timeout) override;
  virtual bool UnmountBackend(DeviceControlRecord* dcr, int timeout) override;
  virtual int d_close(int) override;
  virtual int d_open(const char* pathname, int flags, int mode) override;
  virtual int d_ioctl(int fd, ioctl_req_t request, char* mt = NULL) override;
  virtual boffset_t d_lseek(DeviceControlRecord* dcr,
                            boffset_t offset,
                            int whence) override;
  virtual ssize_t d_read(int fd, void* buffer, size_t count) override;
  virtual ssize_t d_write(int fd, const void* buffer, size_t count) override;
  virtual bool d_truncate(DeviceControlRecord* dcr) override;

 private:
  void OsClrError();
  void HandleError(int func);
};

} /* namespace storagedaemon */

#endif  // BAREOS_STORED_BACKENDS_GENERIC_TAPE_DEVICE_H_
