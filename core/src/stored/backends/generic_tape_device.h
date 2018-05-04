/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2014-2014 Planets Communications B.V.
   Copyright (C) 2014-2014 Bareos GmbH & Co. KG

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

class generic_tape_device: public Device {
public:
   generic_tape_device() {};
   virtual ~generic_tape_device() {};

   /*
    * Interface from Device
    */
   virtual void OpenDevice(DeviceControlRecord *dcr, int omode);
   virtual char *StatusDev();
   virtual bool eod(DeviceControlRecord *dcr);
   virtual void SetAteof();
   virtual void SetAteot();
   virtual bool offline();
   virtual bool weof(int num);
   virtual bool fsf(int num);
   virtual bool bsf(int num);
   virtual bool fsr(int num);
   virtual bool bsr(int num);
   virtual bool LoadDev();
   virtual void LockDoor();
   virtual void UnlockDoor();
   virtual void clrerror(int func);
   virtual void SetOsDeviceParameters(DeviceControlRecord *dcr);
   virtual int32_t GetOsTapeFile();
   virtual bool rewind(DeviceControlRecord *dcr);
   virtual bool UpdatePos(DeviceControlRecord *dcr);
   virtual bool reposition(DeviceControlRecord *dcr, uint32_t rfile, uint32_t rblock);
   virtual bool MountBackend(DeviceControlRecord *dcr, int timeout);
   virtual bool UnmountBackend(DeviceControlRecord *dcr, int timeout);
   virtual int d_close(int);
   virtual int d_open(const char *pathname, int flags, int mode);
   virtual int d_ioctl(int fd, ioctl_req_t request, char *mt = NULL);
   virtual boffset_t d_lseek(DeviceControlRecord *dcr, boffset_t offset, int whence);
   virtual ssize_t d_read(int fd, void *buffer, size_t count);
   virtual ssize_t d_write(int fd, const void *buffer, size_t count);
   virtual bool d_truncate(DeviceControlRecord *dcr);
};
#endif /* BAREOS_STORED_BACKENDS_GENERIC_TAPE_DEVICE_H_ */
