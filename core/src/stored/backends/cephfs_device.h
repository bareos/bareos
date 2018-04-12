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
 * CEPHFS API device abstraction.
 *
 * Marco van Wieringen, May 2014
 */

#ifndef CEPHFS_DEVICE_H
#define CEPHFS_DEVICE_H

#include <cephfs/libcephfs.h>

class cephfs_device: public DEVICE {
private:
   char *m_cephfs_configstring;
   char *m_cephfs_conffile;
   char *m_basedir;
   struct ceph_mount_info *m_cmount;
   POOLMEM *m_virtual_filename;

public:
   cephfs_device();
   ~cephfs_device();

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
#endif /* CEPHFS_DEVICE_H */
