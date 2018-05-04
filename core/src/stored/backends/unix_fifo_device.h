/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2013-2013 Planets Communications B.V.
   Copyright (C) 2013-2013 Bareos GmbH & Co. KG

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
 * UNIX FIFO API device abstraction.
 *
 * Marco van Wieringen, June 2014
 */

#ifndef BAREOS_STORED_BACKENDS_UNIX_FIFO_DEVICE_H_
#define BAREOS_STORED_BACKENDS_UNIX_FIFO_DEVICE_H_

class unix_fifo_device: public Device {
public:
   unix_fifo_device();
   ~unix_fifo_device();

   /*
    * Interface from Device
    */
   void OpenDevice(DeviceControlRecord *dcr, int omode);
   bool eod(DeviceControlRecord *dcr);
   bool MountBackend(DeviceControlRecord *dcr, int timeout);
   bool UnmountBackend(DeviceControlRecord *dcr, int timeout);
   int d_close(int);
   int d_open(const char *pathname, int flags, int mode);
   int d_ioctl(int fd, ioctl_req_t request, char *mt = NULL);
   boffset_t d_lseek(DeviceControlRecord *dcr, boffset_t offset, int whence);
   ssize_t d_read(int fd, void *buffer, size_t count);
   ssize_t d_write(int fd, const void *buffer, size_t count);
   bool d_truncate(DeviceControlRecord *dcr);
};
#endif /* BAREOS_STORED_BACKENDS_UNIX_FIFO_DEVICE_H_ */
