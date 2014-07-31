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
   int status;
   uint64_t object_size;
   time_t object_mtime;
   berrno be;

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
      status = rados_create(&m_cluster, NULL);
      if (status < 0) {
         Mmsg1(errmsg, _("Unable to create RADOS cluster: ERR=%s\n"), be.bstrerror(-status));
         Emsg0(M_FATAL, 0, errmsg);
         goto bail_out;
      }

      status = rados_conf_read_file(m_cluster, m_rados_conffile);
      if (status < 0) {
         Mmsg2(errmsg, _("Unable to read RADOS config %s: ERR=%s\n"), m_rados_conffile, be.bstrerror(-status));
         Emsg0(M_FATAL, 0, errmsg);
         rados_shutdown(m_cluster);
         goto bail_out;
      }

      status = rados_connect(m_cluster);
      if (status < 0) {
         Mmsg1(errmsg, _("Unable to connect to RADOS cluster: ERR=%s\n"), be.bstrerror(-status));
         Emsg0(M_FATAL, 0, errmsg);
         rados_shutdown(m_cluster);
         goto bail_out;
      }

      m_cluster_initialized = true;
   }

   if (!m_ctx) {
      status = rados_ioctx_create(m_cluster, m_rados_poolname, &m_ctx);
      if (status < 0) {
         Mmsg2(errmsg, _("Unable to create RADOS IO context for pool %s: ERR=%s\n"), m_rados_poolname, be.bstrerror(-status));
         Emsg0(M_FATAL, 0, errmsg);
         goto bail_out;
      }
   }

   /*
    * See if the object already exists.
    */
   status = rados_stat(m_ctx, getVolCatName(), &object_size, &object_mtime);

   /*
    * See if the O_CREAT flag is set.
    */
   if (flags & O_CREAT) {
      if (status < 0) {
         switch (status) {
         case -ENOENT:
            /*
             * Create an empty object.
             * e.g. write one byte and then truncate it to zero bytes.
             */
            rados_write(m_ctx, getVolCatName(), " ", 1, 0);
            rados_trunc(m_ctx, getVolCatName(), 0);
            break;
         default:
            errno = -status;
            return -1;
         }
      }
   } else {
      if (status < 0) {
         errno = -status;
         return -1;
      }
   }

   m_offset = 0;

   return 0;

bail_out:
   if (m_cluster_initialized) {
      rados_shutdown(&m_cluster);
      m_cluster_initialized = false;
   }

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
      if (nr_read >= 0) {
         m_offset += nr_read;
         return nr_read;
      } else {
         errno = -nr_read;
         return -1;
      }
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
      if (nr_written >= 0) {
         m_offset += nr_written;
         return nr_written;
      } else {
         errno = -nr_written;
         return -1;
      }
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
      int status;
      uint64_t object_size;
      time_t object_mtime;
      berrno be;

      status = rados_trunc(m_ctx, getVolCatName(), 0);
      if (status < 0) {
         Mmsg2(errmsg, _("Unable to truncate device %s. ERR=%s\n"), prt_name, be.bstrerror(-status));
         Emsg0(M_FATAL, 0, errmsg);
         return false;
      }

      status = rados_stat(m_ctx, getVolCatName(), &object_size, &object_mtime);
      if (status < 0) {
         Mmsg2(errmsg, _("Unable to stat volume %s. ERR=%s\n"), getVolCatName(), be.bstrerror(-status));
         return false;
      }

      if (object_size != 0) { /* rados_trunc() didn't work. */
         status = rados_remove(m_ctx, getVolCatName());
         if (status < 0) {
            Mmsg2(errmsg, _("Unable to remove volume %s. ERR=%s\n"), getVolCatName(), be.bstrerror(-status));
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
   m_rados_configstring = NULL;
   m_rados_conffile = NULL;
   m_rados_poolname = NULL;
   m_cluster_initialized = false;
   m_ctx = NULL;
}

#ifdef HAVE_DYNAMIC_SD_BACKENDS
extern "C" DEVICE SD_IMP_EXP *backend_instantiate(JCR *jcr, int device_type)
{
   DEVICE *dev = NULL;

   switch (device_type) {
   case B_RADOS_DEV:
      dev = New(rados_device);
      break;
   default:
      Jmsg(jcr, M_FATAL, 0, _("Request for unknown devicetype: %d\n"), device_type);
      break;
   }

   return dev;
}

extern "C" void SD_IMP_EXP flush_backend(void)
{
}
#endif
#endif /* HAVE_RADOS */
