/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2014-2016 Planets Communications B.V.
   Copyright (C) 2014-2016 Bareos GmbH & Co. KG

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
 * Marco van Wieringen, February 2014
 */
/**
 * @file
 * CEPH (librados) API device abstraction.
 */

#include "bareos.h"

#ifdef HAVE_RADOS
#include "stored.h"
#include "rados_device.h"

/**
 * Options that can be specified for this device type.
 */
enum device_option_type {
   argument_none = 0,
   argument_conffile,
   argument_poolname,
   argument_clientid,
   argument_clustername,
   argument_username,
   argument_striped,
   argument_stripe_unit,
   argument_object_size,
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
#if LIBRADOS_VERSION_CODE < 17408
   { "clientid=", argument_clientid, 9 },
#else
   { "clustername=", argument_clustername, 12 },
   { "username=", argument_username, 9 },
#endif
#ifdef HAVE_RADOS_STRIPER
   { "striped", argument_striped, 7 },
   { "stripe_unit=", argument_stripe_unit, 12 },
   { "object_size=", argument_object_size, 12 },
   { "stripe_count=", argument_stripe_count, 13 },
#endif
   { NULL, argument_none }
};

/**
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
#if LIBRADOS_VERSION_CODE < 17408
               case argument_clientid:
                  m_rados_clientid = bp + device_options[i].compare_size;
                  done = true;
                  break;
#else
               case argument_clustername:
                  m_rados_clustername = bp + device_options[i].compare_size;
                  done = true;
                  break;
               case argument_username:
                  m_rados_username = bp + device_options[i].compare_size;
                  done = true;
                  break;
#endif
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
                  size_to_uint64(bp + device_options[i].compare_size, &m_stripe_unit);
                  done = true;
                  break;
               case argument_object_size:
                  size_to_uint64(bp + device_options[i].compare_size, &m_object_size);
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

#if LIBRADOS_VERSION_CODE < 17408
      if (!m_rados_clientid) {
         Mmsg1(errmsg, _("No client id configured defaulting to %s\n"), DEFAULT_CLIENTID);
         m_rados_clientid = bstrdup(DEFAULT_CLIENTID);
      }
#else
      if (!m_rados_clustername) {
         m_rados_clustername = bstrdup(DEFAULT_CLUSTERNAME);
      }
      if (!m_rados_username) {
         Mmsg1(errmsg, _("No username configured defaulting to %s\n"), DEFAULT_USERNAME);
         m_rados_username = bstrdup(DEFAULT_USERNAME);
      }
#endif

      if (!m_rados_poolname) {
         Mmsg0(errmsg, _("No rados pool configured\n"));
         Emsg0(M_FATAL, 0, errmsg);
         goto bail_out;
      }
   }

   if (!m_cluster_initialized) {
#if LIBRADOS_VERSION_CODE >= 17408
      uint64_t rados_flags = 0;
#endif

      /*
       * Use for versions lower then 0.69.1 the old rados_create() and
       * for later version rados_create2() calls.
       */
#if LIBRADOS_VERSION_CODE < 17408
      status = rados_create(&m_cluster, m_rados_clientid);
#else
      status = rados_create2(&m_cluster, m_rados_clustername, m_rados_username, rados_flags);
#endif
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

         status = rados_striper_set_object_layout_object_size(m_striper, m_object_size);
         if (status < 0) {
            Mmsg3(errmsg, _("Unable to set RADOS striper object size to %d  for pool %s: ERR=%s\n"), m_object_size, m_rados_poolname, be.bstrerror(-status));
             Emsg0(M_FATAL, 0, errmsg);
             goto bail_out;
         }
      }
#endif
   }

   Mmsg(m_virtual_filename, "%s", getVolCatName());

   /*
    * See if the object already exists.
    */
#ifdef HAVE_RADOS_STRIPER
   if (m_stripe_volume) {
      status = rados_striper_stat(m_striper, m_virtual_filename, &object_size, &object_mtime);
   } else {
      status = rados_stat(m_ctx, m_virtual_filename, &object_size, &object_mtime);
   }
#else
   status = rados_stat(m_ctx, m_virtual_filename, &object_size, &object_mtime);
#endif

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
               rados_striper_write(m_striper, m_virtual_filename, " ", 1, 0);
               rados_striper_trunc(m_striper, m_virtual_filename, 0);
            } else {
#endif
               rados_write(m_ctx, m_virtual_filename, " ", 1, 0);
               rados_trunc(m_ctx, m_virtual_filename, 0);
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
      rados_shutdown(m_cluster);
      m_cluster_initialized = false;
   }

   return -1;
}

/**
 * Read some data from an object at a given offset.
 */
ssize_t rados_device::read_object_data(boffset_t offset, char *buffer, size_t count)
{
#ifdef HAVE_RADOS_STRIPER
   if (m_stripe_volume) {
      return rados_striper_read(m_striper, m_virtual_filename, buffer, count, offset);
   } else {
#endif
      return rados_read(m_ctx, m_virtual_filename, buffer, count, offset);
#ifdef HAVE_RADOS_STRIPER
   }
#endif
}

/**
 * Read data from a volume using librados.
 */
ssize_t rados_device::d_read(int fd, void *buffer, size_t count)
{
   if (m_ctx) {
      size_t nr_read = 0;

      nr_read = read_object_data(m_offset, (char *)buffer, count);
      if (nr_read >= 0) {
         m_offset += nr_read;
      } else {
         errno = -nr_read;
         nr_read = -1;
      }

      return nr_read;
   } else {
      errno = EBADF;
      return -1;
   }
}

/**
 * Write some data to an object at a given offset.
 * Seems the API changed everything earlier then 0.69 returns bytes written.
 */
ssize_t rados_device::write_object_data(boffset_t offset, char *buffer, size_t count)
{
#if LIBRADOS_VERSION_CODE <= 17408
   return = rados_write(m_ctx, m_virtual_filename, buffer, count, offset);
#else
   int status;

#ifdef HAVE_RADOS_STRIPER
   if (m_striper) {
      status = rados_striper_write(m_striper, m_virtual_filename, buffer, count, offset);
   } else {
#endif
      status = rados_write(m_ctx, m_virtual_filename, buffer, count, offset);
#ifdef HAVE_RADOS_STRIPER
   }
#endif

   if (status == 0) {
      return count;
   } else {
      errno = -status;
      return -1;
   }
#endif
}

/**
 * Write data to a volume using librados.
 */
ssize_t rados_device::d_write(int fd, const void *buffer, size_t count)
{
   if (m_ctx) {
      size_t nr_written = 0;

      nr_written = write_object_data(m_offset, (char *)buffer, count);
      if (nr_written >= 0) {
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

#ifdef HAVE_RADOS_STRIPER
ssize_t rados_device::striper_volume_size()
{
   uint64_t object_size;
   time_t object_mtime;

   if (rados_striper_stat(m_striper, m_virtual_filename, &object_size, &object_mtime) == 0) {
      return object_size;
   } else {
      return -1;
   }
}
#endif

ssize_t rados_device::volume_size()
{
   uint64_t object_size;
   time_t object_mtime;

   if (rados_stat(m_ctx, m_virtual_filename, &object_size, &object_mtime) == 0) {
      return object_size;
   } else {
      return -1;
   }
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
      ssize_t filesize;

#ifdef HAVE_RADOS_STRIPER
      if (m_stripe_volume) {
         filesize = striper_volume_size();
      } else {
         filesize = volume_size();
      }
#else
      filesize = volume_size();
#endif

      if (filesize >= 0) {
         m_offset = filesize + offset;
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

#ifdef HAVE_RADOS_STRIPER
bool rados_device::truncate_striper_volume(DCR *dcr)
{
   int status;
   uint64_t object_size;
   time_t object_mtime;
   berrno be;

   status = rados_striper_trunc(m_striper, m_virtual_filename, 0);
   if (status < 0) {
      Mmsg2(errmsg, _("Unable to truncate device %s. ERR=%s\n"), prt_name, be.bstrerror(-status));
      Emsg0(M_FATAL, 0, errmsg);
      return false;
   }

   status = rados_striper_stat(m_striper, m_virtual_filename, &object_size, &object_mtime);
   if (status < 0) {
      Mmsg2(errmsg, _("Unable to stat volume %s. ERR=%s\n"), m_virtual_filename, be.bstrerror(-status));
      Dmsg1(100, "%s", errmsg);
      return false;
   }

   if (object_size != 0) { /* rados_trunc() didn't work. */
      status = rados_striper_remove(m_striper, m_virtual_filename);
      if (status < 0) {
         Mmsg2(errmsg, _("Unable to remove volume %s. ERR=%s\n"), m_virtual_filename, be.bstrerror(-status));
         Dmsg1(100, "%s", errmsg);
         return false;
      }
   }

   m_offset = 0;
   return true;
}
#endif

bool rados_device::truncate_volume(DCR *dcr)
{
   int status;
   uint64_t object_size;
   time_t object_mtime;
   berrno be;

   status = rados_trunc(m_ctx, m_virtual_filename, 0);
   if (status < 0) {
      Mmsg2(errmsg, _("Unable to truncate device %s. ERR=%s\n"), prt_name, be.bstrerror(-status));
      Emsg0(M_FATAL, 0, errmsg);
      return false;
   }

   status = rados_stat(m_ctx, m_virtual_filename, &object_size, &object_mtime);
   if (status < 0) {
      Mmsg2(errmsg, _("Unable to stat volume %s. ERR=%s\n"), m_virtual_filename, be.bstrerror(-status));
      Dmsg1(100, "%s", errmsg);
      return false;
   }

   if (object_size != 0) { /* rados_trunc() didn't work. */
      status = rados_remove(m_ctx, m_virtual_filename);
      if (status < 0) {
         Mmsg2(errmsg, _("Unable to remove volume %s. ERR=%s\n"), m_virtual_filename, be.bstrerror(-status));
         Dmsg1(100, "%s", errmsg);
         return false;
      }
   }

   m_offset = 0;
   return true;
}

bool rados_device::d_truncate(DCR *dcr)
{
   if (m_ctx) {
#ifdef HAVE_RADOS_STRIPER
      if (m_stripe_volume) {
         return truncate_striper_volume(dcr);
      } else {
         return truncate_volume(dcr);
      }
#else
      return truncate_volume(dcr);
#endif
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
      rados_shutdown(m_cluster);
      m_cluster_initialized = false;
   }

#if LIBRADOS_VERSION_CODE < 17408
   if (m_rados_clientid) {
      free(m_rados_clientid);
   }
#else
   if (m_rados_clustername) {
      free(m_rados_clustername);
   }
   if (m_rados_username) {
      free(m_rados_username);
   }
#endif

   if (m_rados_configstring) {
      free(m_rados_configstring);
   }

   free_pool_memory(m_virtual_filename);
}

rados_device::rados_device()
{
   m_rados_configstring = NULL;
   m_rados_conffile = NULL;
   m_rados_poolname = NULL;
#if LIBRADOS_VERSION_CODE < 17408
   m_rados_clientid = NULL;
#else
   m_rados_clustername = NULL;
   m_rados_username = NULL;
#endif
   m_cluster_initialized = false;
   m_ctx = NULL;
#ifdef HAVE_RADOS_STRIPER
   m_stripe_volume = false;
   m_stripe_unit = 4194304;
   m_stripe_count = 1;
   m_object_size = 4194304;
   m_striper = NULL;
#endif
   m_virtual_filename = get_pool_memory(PM_FNAME);
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
