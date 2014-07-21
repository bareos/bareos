/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2013-2014 Bareos GmbH & Co. KG

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
 * Windows File API device abstraction.
 *
 * Marco van Wieringen, December 2013
 */

#ifndef WIN32_FILE_DEVICE_H
#define WIN32_FILE_DEVICE_H

class win32_file_device: public DEVICE {
public:
   win32_file_device();
   ~win32_file_device();

   /*
    * Interface from DEVICE
    */
   bool mount_backend(DCR *dcr, int timeout);
   bool unmount_backend(DCR *dcr, int timeout);
   int d_close(int);
   int d_open(const char *pathname, int flags, int mode);
   int d_ioctl(int fd, ioctl_req_t request, char *mt = NULL);
   ssize_t d_read(int fd, void *buffer, size_t count);
   ssize_t d_write(int fd, const void *buffer, size_t count);
   boffset_t d_lseek(DCR *dcr, boffset_t offset, int whence);
   bool d_truncate(DCR *dcr);
};
#endif /* WIN32_FILE_DEVICE_H */
