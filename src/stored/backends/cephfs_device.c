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
 * Gluster Filesystem API device abstraction.
 *
 * Marco van Wieringen, May 2014
 */

#include "bareos.h"

#ifdef HAVE_CEPHFS
#include "stored.h"
#include "backends/cephfs_device.h"

/*
 * Open a volume using libcephfs.
 */
int cephfs_device::d_open(const char *pathname, int flags, int mode)
{
   int status;
   berrno be;

   if (!m_cephfs_configstring) {
      char *bp;

      m_cephfs_configstring = bstrdup(dev_name);
      bp = strchr(m_cephfs_configstring, ':');
      if (!bp) {
         Mmsg1(errmsg, _("Unable to parse device %s.\n"), dev_name);
         Emsg0(M_FATAL, 0, errmsg);
         goto bail_out;
      }

      *bp++ = '\0';
      m_cephfs_conffile = m_cephfs_configstring;
      m_basedir = bp;
   }

   if (!m_cmount) {
      status = ceph_create(&m_cmount, NULL);
      if (status < 0) {
         Mmsg1(errmsg, _("Unable to create CEPHFS mount: ERR=%s\n"), be.bstrerror(-status));
         Emsg0(M_FATAL, 0, errmsg);
         goto bail_out;
      }

      status = ceph_conf_read_file(m_cmount, m_cephfs_conffile);
      if (status < 0) {
         Mmsg2(errmsg, _("Unable to read CEPHFS config %s: ERR=%s\n"), m_cephfs_conffile, be.bstrerror(-status));
         Emsg0(M_FATAL, 0, errmsg);
         goto bail_out;
      }

      status = ceph_mount(m_cmount, NULL);
      if (status < 0) {
         Mmsg1(errmsg, _("Unable to mount CEPHFS: ERR=%s\n"), be.bstrerror(-status));
         Emsg0(M_FATAL, 0, errmsg);
         goto bail_out;
      }
   }

   /*
    * See if we don't have a file open already.
    */
   if (m_fd >= 0) {
      ceph_close(m_cmount, m_fd);
      m_fd = -1;
   }

   /*
    * See if we store in an explicit directory.
    */
   if (m_basedir) {
      struct stat st;

      /*
       * Make sure the dir exists if one is defined.
       */
      status = ceph_stat(m_cmount, m_basedir, &st);
      if (status < 0) {
         switch (status) {
         case -ENOENT:
            status = ceph_mkdirs(m_cmount, m_basedir, 0750);
            if (status < 0) {
               Mmsg1(errmsg, _("Specified CEPHFS direcory %s cannot be created.\n"), m_basedir);
               Emsg0(M_FATAL, 0, errmsg);
               goto bail_out;
            }
            break;
         default:
            goto bail_out;
         }
      } else {
         if (!S_ISDIR(st.st_mode)) {
            Mmsg1(errmsg, _("Specified CEPHFS direcory %s is not a directory.\n"), m_basedir);
            Emsg0(M_FATAL, 0, errmsg);
            goto bail_out;
         }
      }

      Mmsg(m_virtual_filename, "%s/%s", m_basedir, getVolCatName());
   } else {
      Mmsg(m_virtual_filename, "%s", getVolCatName());
   }

   m_fd = ceph_open(m_cmount, m_virtual_filename, flags, mode);
   if (m_fd < 0) {
      goto bail_out;
   }

   return 0;

bail_out:
   /*
    * Cleanup the CEPHFS context.
    */
   if (m_cmount) {
      ceph_shutdown(m_cmount);
      m_cmount = NULL;
   }
   m_fd = -1;

   return -1;
}

/*
 * Read data from a volume using libcephfs.
 */
ssize_t cephfs_device::d_read(int fd, void *buffer, size_t count)
{
   if (m_fd >= 0) {
      return ceph_read(m_cmount, m_fd, (char *)buffer, count, -1);
   } else {
      errno = EBADF;
      return -1;
   }
}

/*
 * Write data to a volume using libcephfs.
 */
ssize_t cephfs_device::d_write(int fd, const void *buffer, size_t count)
{
   if (m_fd >= 0) {
      return ceph_write(m_cmount, m_fd, (char *)buffer, count, -1);
   } else {
      errno = EBADF;
      return -1;
   }
}

int cephfs_device::d_close(int fd)
{
   if (m_fd >= 0) {
      int status;

      status = ceph_close(m_cmount, m_fd);
      m_fd = -1;
      return (status < 0) ? -1: 0;
   } else {
      errno = EBADF;
      return -1;
   }
}

int cephfs_device::d_ioctl(int fd, ioctl_req_t request, char *op)
{
   return -1;
}

boffset_t cephfs_device::d_lseek(DCR *dcr, boffset_t offset, int whence)
{
   if (m_fd >= 0) {
      boffset_t status;

      status = ceph_lseek(m_cmount, m_fd, offset, whence);
      if (status >= 0) {
         return status;
      } else {
         errno = -status;
         status = -1;
         return status;
      }
   } else {
      errno = EBADF;
      return -1;
   }
}

bool cephfs_device::d_truncate(DCR *dcr)
{
   int status;
   struct stat st;

   if (m_fd >= 0) {
      status = ceph_ftruncate(m_cmount, m_fd, 0);
      if (status < 0) {
         berrno be;

         Mmsg2(errmsg, _("Unable to truncate device %s. ERR=%s\n"), prt_name, be.bstrerror(-status));
         Emsg0(M_FATAL, 0, errmsg);
         return false;
      }

      /*
       * Check for a successful ceph_ftruncate() and issue work-around when truncation doesn't work.
       *
       * 1. close file
       * 2. delete file
       * 3. open new file with same mode
       * 4. change ownership to original
       */
      status = ceph_fstat(m_cmount, m_fd, &st);
      if (status < 0) {
         berrno be;

         Mmsg2(errmsg, _("Unable to stat device %s. ERR=%s\n"), prt_name, be.bstrerror(-status));
         return false;
      }

      if (st.st_size != 0) {             /* ceph_ftruncate() didn't work */
         ceph_close(m_cmount, m_fd);
         ceph_unlink(m_cmount, m_virtual_filename);

         /*
          * Recreate the file -- of course, empty
          */
         oflags = O_CREAT | O_RDWR | O_BINARY;
         m_fd = ceph_open(m_cmount, m_virtual_filename, oflags, st.st_mode);
         if (m_fd < 0) {
            berrno be;

            dev_errno = -m_fd;;
            Mmsg2(errmsg, _("Could not reopen: %s, ERR=%s\n"), m_virtual_filename, be.bstrerror(-m_fd));
            Emsg0(M_FATAL, 0, errmsg);
            m_fd = -1;

            return false;
         }

         /*
          * Reset proper owner
          */
         ceph_chown(m_cmount, m_virtual_filename, st.st_uid, st.st_gid);
      }
   }

   return true;
}

cephfs_device::~cephfs_device()
{
   if (m_cmount && m_fd >= 0) {
      ceph_close(m_cmount, m_fd);
      m_fd = -1;
   }

   if (!m_cmount) {
      ceph_shutdown(m_cmount);
      m_cmount = NULL;
   }

   if (m_cephfs_configstring) {
      free(m_cephfs_configstring);
      m_cephfs_configstring = NULL;
   }

   free_pool_memory(m_virtual_filename);
}

cephfs_device::cephfs_device()
{
   m_fd = -1;
   m_cephfs_configstring = NULL;
   m_cephfs_conffile = NULL;
   m_basedir = NULL;
   m_cmount = NULL;
   m_virtual_filename = get_pool_memory(PM_FNAME);
}

#ifdef HAVE_DYNAMIC_SD_BACKENDS
extern "C" DEVICE SD_IMP_EXP *backend_instantiate(JCR *jcr, int device_type)
{
   DEVICE *dev = NULL;

   switch (device_type) {
   case B_CEPHFS_DEV:
      dev = New(cephfs_device);
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
#endif /* HAVE_CEPHFS */
