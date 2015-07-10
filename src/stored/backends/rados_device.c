/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2014-2014 Planets Communications B.V.
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
 * Options that can be specified for this device type.
 */
enum device_option_type {
   argument_none = 0,
   argument_conffile,
   argument_poolname,
   argument_striped,
   argument_stripe_unit,
   argument_stripe_count
};

struct device_option {
   const char *name;
   enum device_option_type type;
   int compare_size;
};

static device_option device_options[] = {
   { "conffile=", argument_conffile, 9 },
   { "poolname=", argument_poolname, 9 },
#ifdef HAVE_RADOS_STRIPER
   { "striped", argument_striped, 7 },
   { "stripe_unit=", argument_stripe_unit, 11 },
   { "stripe_count=", argument_stripe_count, 12 },
#endif
   { NULL, argument_none }
};

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
      char *bp, *next_option;
      bool done;

      if (!dev_options) {
         Mmsg0(errmsg, _("No device options configured\n"));
         Emsg0(M_FATAL, 0, errmsg);
         goto bail_out;
      }

      m_rados_configstring = bstrdup(dev_options);

      bp = m_rados_configstring;
      while (bp) {
         next_option = strchr(bp, ',');
         if (next_option) {
            *next_option++ = '\0';
         }

         done = false;
         for (int i = 0; !done && device_options[i].name; i++) {
            /*
             * Try to find a matching device option.
             */
            if (bstrncasecmp(bp, device_options[i].name, device_options[i].compare_size)) {
               switch (device_options[i].type) {
               case argument_conffile:
                  m_rados_conffile = bp + device_options[i].compare_size;
                  done = true;
                  break;
               case argument_poolname:
                  m_rados_poolname = bp + device_options[i].compare_size;
                  done = true;
                  break;
#ifdef HAVE_RADOS_STRIPER
               case argument_striped:
                  m_stripe_volume = true;
                  done = true;
                  break;
               case argument_stripe_unit:
                  m_stripe_unit = str_to_int64(bp + device_options[i].compare_size);
                  done = true;
                  break;
               case argument_stripe_count:
                  m_stripe_count = str_to_int64(bp + device_options[i].compare_size);
                  done = true;
                  break;
#endif
               default:
                  break;
               }
            }
         }

         if (!done) {
            Mmsg1(errmsg, _("Unable to parse device option: %s\n"), bp);
            Emsg0(M_FATAL, 0, errmsg);
            goto bail_out;
         }

         bp = next_option;
      }

      if (!m_rados_conffile) {
         Mmsg0(errmsg, _("No rados config file configured\n"));
         Emsg0(M_FATAL, 0, errmsg);
         goto bail_out;
      }

      if (!m_rados_poolname) {
         Mmsg0(errmsg, _("No rados pool configured\n"));
         Emsg0(M_FATAL, 0, errmsg);
         goto bail_out;
      }
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

#ifdef HAVE_RADOS_STRIPER
      if (m_stripe_volume) {
         status = rados_striper_create(m_ctx, &m_striper);
         if (status < 0) {
            Mmsg2(errmsg, _("Unable to create RADOS striper object for pool %s: ERR=%s\n"), m_rados_poolname, be.bstrerror(-status));
            Emsg0(M_FATAL, 0, errmsg);
            goto bail_out;
         }

         status = rados_striper_set_object_layout_stripe_unit(m_striper, m_stripe_unit);
         if (status < 0) {
            Mmsg3(errmsg, _("Unable to set RADOS striper unit size to %d  for pool %s: ERR=%s\n"), m_stripe_unit, m_rados_poolname, be.bstrerror(-status));
            Emsg0(M_FATAL, 0, errmsg);
            goto bail_out;
         }

         status = rados_striper_set_object_layout_stripe_count(m_striper, m_stripe_count);
         if (status < 0) {
            Mmsg3(errmsg, _("Unable to set RADOS striper stripe count to %d  for pool %s: ERR=%s\n"), m_stripe_count, m_rados_poolname, be.bstrerror(-status));
            Emsg0(M_FATAL, 0, errmsg);
            goto bail_out;
         }
      }
#endif
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
#ifdef HAVE_RADOS_STRIPER
            if (m_stripe_volume) {
               rados_striper_write(m_ctx, getVolCatName(), " ", 1, 0);
               rados_striper_trunc(m_ctx, getVolCatName(), 0);
            } else {
#endif
               rados_write(m_ctx, getVolCatName(), " ", 1, 0);
               rados_trunc(m_ctx, getVolCatName(), 0);
#ifdef HAVE_RADOS_STRIPER
            }
#endif
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

#ifdef HAVE_RADOS_STRIPER
      if (m_striper) {
         nr_read = rados_striper_read(m_striper, getVolCatName(), (char *)buffer, count, m_offset);
      } else {
#endif
         nr_read = rados_read(m_ctx, getVolCatName(), (char *)buffer, count, m_offset);
#ifdef HAVE_RADOS_STRIPER
      }
#endif

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
 *
 * Seems the API changed everything earlier then 0.69 returns bytes written.
 */
#if LIBRADOS_VERSION_CODE <= 17408
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
#else
ssize_t rados_device::d_write(int fd, const void *buffer, size_t count)
{
   if (m_ctx) {
      int status;

#ifdef HAVE_RADOS_STRIPER
      if (m_striper) {
         status = rados_striper_write(m_striper, getVolCatName(), (char *)buffer, count, m_offset);
      } else {
#endif
         status = rados_write(m_ctx, getVolCatName(), (char *)buffer, count, m_offset);
#ifdef HAVE_RADOS_STRIPER
      }
#endif

      if (status == 0) {
         m_offset += count;
         return count;
      } else {
         errno = -status;
         return -1;
      }
   } else {
      errno = EBADF;
      return -1;
   }
}
#endif

int rados_device::d_close(int fd)
{
   /*
    * Destroy the IOcontext.
    */
   if (m_ctx) {
#ifdef HAVE_RADOS_STRIPER
      if (m_striper) {
         rados_striper_destroy(m_striper);
         m_striper = NULL;
      }
#endif
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

#ifdef HAVE_RADOS_STRIPER
      if (m_stripe_volume) {
         status = rados_striper_trunc(m_ctx, getVolCatName(), 0);
      } else {
#endif
         status = rados_trunc(m_ctx, getVolCatName(), 0);
#ifdef HAVE_RADOS_STRIPER
      }
#endif

      if (status < 0) {
         Mmsg2(errmsg, _("Unable to truncate device %s. ERR=%s\n"), prt_name, be.bstrerror(-status));
         Emsg0(M_FATAL, 0, errmsg);
         return false;
      }

      status = rados_stat(m_ctx, getVolCatName(), &object_size, &object_mtime);
      if (status < 0) {
         Mmsg2(errmsg, _("Unable to stat volume %s. ERR=%s\n"), getVolCatName(), be.bstrerror(-status));
         Dmsg1(100, "%s", errmsg);
         return false;
      }

      if (object_size != 0) { /* rados_trunc() didn't work. */
         status = rados_remove(m_ctx, getVolCatName());
         if (status < 0) {
            Mmsg2(errmsg, _("Unable to remove volume %s. ERR=%s\n"), getVolCatName(), be.bstrerror(-status));
            Dmsg1(100, "%s", errmsg);
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
#ifdef HAVE_RADOS_STRIPER
   m_stripe_volume = false;
   m_stripe_unit = 0;
   m_stripe_count = 0;
   m_striper = NULL;
#endif
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
