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
 * Object Storage API device abstraction.
 *
 * Marco van Wieringen, February 2014
 */

#ifndef OBJECTSTORAGE_DEVICE_H
#define OBJECTSTORAGE_DEVICE_H

#include <droplet.h>
#include <droplet/vfs.h>

class object_store_device: public DEVICE {
private:
   char *m_object_configstring;
   char *m_profile;
   char *m_object_bucketname;
   dpl_ctx_t *m_ctx;
   dpl_vfile_t *m_vfd;
   boffset_t m_offset;

public:
   object_store_device();
   ~object_store_device();

   /*
    * Interface from DEVICE
    */
   int d_close(int);
   int d_open(const char *pathname, int flags, int mode);
   int d_ioctl(int fd, ioctl_req_t request, char *mt = NULL);
   boffset_t d_lseek(DCR *dcr, boffset_t offset, int whence);
   ssize_t d_read(int fd, void *buffer, size_t count);
   ssize_t d_write(int fd, const void *buffer, size_t count);
   bool d_truncate(DCR *dcr);
};
#endif /* OBJECTSTORE_DEVICE_H */
