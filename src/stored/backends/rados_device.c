/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2014-2014 Bareos GmbH & Co. KG

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   General Public License for more details.

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

#include "bareos.h"

#ifdef HAVE_RADOS
#include "stored.h"
#include "rados_device.h"

/*
 * Open a volume using librados.
 */
int rados_device::d_open(const char *pathname, int flags, int mode)
{
   if (!m_rados_configstring) {
      char *bp;

      m_rados_configstring = bstrdup(dev_name);
      bp = strchr(m_rados_configstring, ':');
      if (!bp) {
         Mmsg1(errmsg, _("Unable to parse device %s.\n"), dev_name);
         Emsg0(M_FATAL, 0, errmsg);
         goto bail_out;
      }

      *bp++ = '\0';
      m_rados_conffile = m_rados_configstring;
      m_rados_poolname = bp;
   }

   if (!m_cluster_initialized) {
      if (rados_create(&m_cluster, NULL) < 0) {
         berrno be;

         Mmsg1(errmsg, _("Unable to create RADOS cluster: ERR=%s\n"), be.bstrerror());
         Emsg0(M_FATAL, 0, errmsg);
         goto bail_out;
      }

      if (rados_conf_read_file(m_cluster, m_rados_conffile) < 0) {
         berrno be;

         Mmsg2(errmsg, _("Unable to read RADOS config %s: ERR=%s\n"), m_rados_conffile, be.bstrerror());
         Emsg0(M_FATAL, 0, errmsg);
         rados_shutdown(m_cluster);
         goto bail_out;
      }

      if (rados_connect(m_cluster) < 0) {
         berrno be;

         Mmsg1(errmsg, _("Unable to connect to RADOS cluster: ERR=%s\n"), be.bstrerror());
         Emsg0(M_FATAL, 0, errmsg);
         rados_shutdown(m_cluster);
         goto bail_out;
      }

      m_cluster_initialized = true;
   }

   if (!m_ctx) {
      if (rados_ioctx_create(m_cluster, m_rados_poolname, &m_ctx) < 0) {
         berrno be;

         Mmsg2(errmsg, _("Unable to create RADOS IO context for pool %s: ERR=%s\n"), m_rados_poolname, be.bstrerror());
         Emsg0(M_FATAL, 0, errmsg);
         goto bail_out;
      }
   }

   m_offset = 0;

   return 0;

bail_out:
   return -1;
}

/*
 * Read data from a volume using librados.
 */
ssize_t rados_device::d_read(int fd, void *buffer, size_t count)
{
   if (m_ctx) {
      int nr_read;

      nr_read = rados_read(m_ctx, getVolCatName(), (char *)buffer, count, m_offset);
      if (nr_read > 0) {
         m_offset += nr_read;
      }
      return nr_read;
   } else {
      errno = EBADF;
      return -1;
   }
}

/*
 * Write data to a volume using librados.
 */
ssize_t rados_device::d_write(int fd, const void *buffer, size_t count)
{
   if (m_ctx) {
      int nr_written;

      nr_written = rados_write(m_ctx, getVolCatName(), (char *)buffer, count, m_offset);
      if (nr_written > 0) {
         m_offset += nr_written;
      }
      return nr_written;
   } else {
      errno = EBADF;
      return -1;
   }
}

int rados_device::d_close(int fd)
{
   /*
    * Destroy the IOcontext.
    */
   if (m_ctx) {
      rados_ioctx_destroy(m_ctx);
      m_ctx = NULL;
   } else {
      errno = EBADF;
      return -1;
   }

   return 0;
}

int rados_device::d_ioctl(int fd, ioctl_req_t request, char *op)
{
   return -1;
}

boffset_t rados_device::d_lseek(DCR *dcr, boffset_t offset, int whence)
{
   switch (whence) {
   case SEEK_SET:
      m_offset = offset;
      break;
   case SEEK_CUR:
      m_offset += offset;
      break;
   case SEEK_END: {
      uint64_t object_size;
      time_t object_mtime;

      if (rados_stat(m_ctx, getVolCatName(), &object_size, &object_mtime) == 0) {
         m_offset = object_size + offset;
      } else {
         return -1;
      }
      break;
   }
   default:
      return -1;
   }

   return m_offset;
}

bool rados_device::d_truncate(DCR *dcr)
{
   if (m_ctx) {
      uint64_t object_size;
      time_t object_mtime;

      if (rados_trunc(m_ctx, getVolCatName(), 0) != 0) {
         berrno be;

         Mmsg2(errmsg, _("Unable to truncate device %s. ERR=%s\n"),
               print_name(), be.bstrerror());
         Emsg0(M_FATAL, 0, errmsg);
         return false;
      }

      if (rados_stat(m_ctx, getVolCatName(), &object_size, &object_mtime) == 0) {
         berrno be;

         Mmsg2(errmsg, _("Unable to stat volume %s. ERR=%s\n"), getVolCatName(), be.bstrerror());
         return false;
      }

      if (object_size != 0) { /* rados_trunc() didn't work. */
         if (rados_remove(m_ctx, getVolCatName()) != 0) {
            berrno be;

            Mmsg2(errmsg, _("Unable to remove volume %s. ERR=%s\n"), getVolCatName(), be.bstrerror());
            return false;
         }
      }

      m_offset = 0;
   }

   return true;
}

rados_device::~rados_device()
{
   if (m_ctx) {
      rados_ioctx_destroy(m_ctx);
      m_ctx = NULL;
   }

   if (m_cluster_initialized) {
      rados_shutdown(&m_cluster);
      m_cluster_initialized = false;
   }

   if (m_rados_configstring) {
      free(m_rados_configstring);
   }
}

rados_device::rados_device()
{
   m_fd = -1;
   m_rados_configstring = NULL;
   m_rados_conffile = NULL;
   m_rados_poolname = NULL;
   m_cluster_initialized = false;
   m_ctx = NULL;
}
#endif
