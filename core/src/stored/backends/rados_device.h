/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2014-2016 Planets Communications B.V.
   Copyright (C) 2014-2016 Bareos GmbH & Co. KG

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
 * CEPH (librados) API device abstraction.
 *
 * Marco van Wieringen, February 2014
 */

#ifndef RADOS_DEVICE_H
#define RADOS_DEVICE_H

#include <rados/librados.h>

#ifdef HAVE_RADOS_STRIPER
#include <radosstriper/libradosstriper.h>
#endif

/*
 * Use for versions lower then 0.68.0 of the API the old format and otherwise the new one.
 */
#if LIBRADOS_VERSION_CODE < 17408
#define DEFAULT_CLIENTID "admin"
#else
#define DEFAULT_CLUSTERNAME "ceph"
#define DEFAULT_USERNAME "client.admin"
#endif

class rados_device: public DEVICE {
private:
   /*
    * Private Members
    */
   char *m_rados_configstring;
   char *m_rados_conffile;
   char *m_rados_poolname;
#if LIBRADOS_VERSION_CODE < 17408
   char *m_rados_clientid;
#else
   char *m_rados_clustername;
   char *m_rados_username;
#endif
   bool m_cluster_initialized;
#ifdef HAVE_RADOS_STRIPER
   bool m_stripe_volume;
   uint64_t m_stripe_unit;
   uint32_t m_stripe_count;
   uint64_t m_object_size;
#endif
   rados_t m_cluster;
   rados_ioctx_t m_ctx;
#ifdef HAVE_RADOS_STRIPER
   rados_striper_t m_striper;
#endif
   boffset_t m_offset;
   POOLMEM *m_virtual_filename;

   /*
    * Private Methods
    */
   ssize_t read_object_data(boffset_t offset, char *buffer, size_t count);
   ssize_t write_object_data(boffset_t offset, char *buffer, size_t count);
#ifdef HAVE_RADOS_STRIPER
   ssize_t striper_volume_size();
#endif
   ssize_t volume_size();
#ifdef HAVE_RADOS_STRIPER
   bool truncate_striper_volume(DCR *dcr);
#endif
   bool truncate_volume(DCR *dcr);

public:
   /*
    * Public Methods
    */
   rados_device();
   ~rados_device();

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
#endif /* RADOS_DEVICE_H */
