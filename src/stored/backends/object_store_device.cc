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
 * Marco van Wieringen, February 2014
 */
/**
 * @file
 * Object Storage API device abstraction.
 */

#include "bareos.h"

#ifdef HAVE_DROPLET
#include "stored.h"
#include "object_store_device.h"

/**
 * Options that can be specified for this device type.
 */
enum device_option_type {
   argument_none = 0,
   argument_profile,
   argument_bucket
};

struct device_option {
   const char *name;
   enum device_option_type type;
   int compare_size;
};

static device_option device_options[] = {
   { "profile=", argument_profile, 8 },
   { "bucket=", argument_bucket, 7 },
   { NULL, argument_none }
};

static int droplet_reference_count = 0;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

/**
 * Generic log function that glues libdroplet with BAREOS.
 */
static void object_store_logfunc(dpl_ctx_t *ctx, dpl_log_level_t level, const char *message)
{
   switch (level) {
   case DPL_DEBUG:
      Dmsg1(100, "%s\n", message);
      break;
   case DPL_INFO:
      Emsg1(M_INFO, 0, "%s\n", message);
      break;
   case DPL_WARNING:
      Emsg1(M_WARNING, 0, "%s\n", message);
      break;
   case DPL_ERROR:
      Emsg1(M_ERROR, 0, "%s\n", message);
      break;
   }
}

/**
 * Map the droplet errno's to system ones.
 */
static inline int droplet_errno_to_system_errno(dpl_status_t status)
{
   switch (status) {
   case DPL_ENOENT:
      errno = ENOENT;
      break;
   case DPL_EIO:
      errno = EIO;
      break;
   case DPL_ENAMETOOLONG:
      errno = ENAMETOOLONG;
      break;
   case DPL_EEXIST:
      errno = EEXIST;
      break;
   case DPL_EPERM:
      errno = EPERM;
      break;
   default:
      break;
   }

   return -1;
}

/**
 * Open a volume using libdroplet.
 */
int object_store_device::d_open(const char *pathname, int flags, int mode)
{
   dpl_status_t status;
   dpl_vfile_flag_t dpl_flags;
   dpl_option_t dpl_options;

#if 1
   Mmsg1(errmsg, _("Object Storage devices are not yet supported, please disable %s\n"), dev_name);
   return -1;
#endif

   /*
    * Initialize the droplet library when its not done previously.
    */
   P(mutex);
   if (droplet_reference_count == 0) {
      status = dpl_init();
      if (status != DPL_SUCCESS) {
         V(mutex);
         return -1;
      }

      dpl_set_log_func(object_store_logfunc);
      droplet_reference_count++;
   }
   V(mutex);

   if (!m_object_configstring) {
      int len;
      char *bp, *next_option;
      bool done;

      if (!dev_options) {
         Mmsg0(errmsg, _("No device options configured\n"));
         Emsg0(M_FATAL, 0, errmsg);
         return -1;
      }

      m_object_configstring = bstrdup(dev_options);

      bp = m_object_configstring;
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
               case argument_profile:
                  m_profile = bp + device_options[i].compare_size;
                  done = true;
                  break;
               case argument_bucket:
                  m_object_bucketname = bp + device_options[i].compare_size;
                  done = true;
                  break;
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

      if (!m_profile) {
         Mmsg0(errmsg, _("No droplet profile configured\n"));
         Emsg0(M_FATAL, 0, errmsg);
         goto bail_out;
      }

      /*
       * Strip any .profile prefix from the libdroplet profile name.
       */
      len = strlen(m_profile);
      if (len > 8 && bstrcasecmp(m_profile + (len - 8), ".profile")) {
         m_profile[len - 8] = '\0';
      }
   }

   /*
    * See if we need to setup a new context for this device.
    */
   if (!m_ctx) {
      char *bp;

      /*
       * See if this is a path.
       */
      bp = strrchr(m_object_configstring, '/');
      if (!bp) {
         /*
          * Only a profile name.
          */
         m_ctx = dpl_ctx_new(NULL, m_object_configstring);
      } else {
         if (bp == m_object_configstring) {
            /*
             * Profile in root of filesystem
             */
            m_ctx = dpl_ctx_new("/", bp + 1);
         } else {
            /*
             * Profile somewhere else.
             */
            *bp++ = '\0';
            m_ctx = dpl_ctx_new(m_object_configstring, bp);
         }
      }

      /*
       * If we failed to allocate a new context fail the open.
       */
      if (!m_ctx) {
         Mmsg1(errmsg, _("Failed to create a new context using config %s\n"), dev_options);
         return -1;
      }

      /*
       * Login if that is needed for this backend.
       */
      status = dpl_login(m_ctx);
      switch (status) {
      case DPL_SUCCESS:
         break;
      case DPL_ENOTSUPP:
         /*
          * Backend doesn't support login which is fine.
          */
         break;
      default:
         Mmsg2(errmsg, _("Failed to login for voume %s using dpl_login(): ERR=%s.\n"),
               getVolCatName(), dpl_status_str(status));
         return -1;
      }

      /*
       * If a bucketname was defined set it in the context.
       */
      if (m_object_bucketname) {
         m_ctx->cur_bucket = m_object_bucketname;
      }
   }

   /*
    * See if we don't have a file open already.
    */
   if (m_vfd) {
      dpl_close(m_vfd);
      m_vfd = NULL;
   }

   /*
    * Create some options for libdroplet.
    *
    * DPL_OPTION_NOALLOC - we provide the buffer to copy the data into
    *                      no need to let the library allocate memory we
    *                      need to free after copying the data.
    */
   memset(&dpl_options, 0, sizeof(dpl_options));
   dpl_options.mask |= DPL_OPTION_NOALLOC;

   if (flags & O_CREAT) {
      dpl_flags = DPL_VFILE_FLAG_CREAT | DPL_VFILE_FLAG_RDWR;
      status = dpl_open(m_ctx, /* context */
                        getVolCatName(), /* locator */
                        dpl_flags, /* flags */
                        &dpl_options, /* options */
                        NULL, /* condition */
                        NULL, /* metadata */
                        NULL, /* sysmd */
                        NULL, /* query_params */
                        NULL, /* stream_status */
                        &m_vfd);
   } else {
      dpl_flags = DPL_VFILE_FLAG_RDWR;
      status = dpl_open(m_ctx, /* context */
                        getVolCatName(), /* locator */
                        dpl_flags, /* flags */
                        &dpl_options, /* options */
                        NULL, /* condition */
                        NULL, /* metadata */
                        NULL, /* sysmd */
                        NULL, /* query_params */
                        NULL, /* stream_status */
                        &m_vfd);
   }

   switch (status) {
   case DPL_SUCCESS:
      m_offset = 0;
      return 0;
   default:
      Mmsg2(errmsg, _("Failed to open %s using dpl_open(): ERR=%s.\n"),
            getVolCatName(), dpl_status_str(status));
      m_vfd = NULL;
      return droplet_errno_to_system_errno(status);
   }

bail_out:
   return -1;
}

/**
 * Read data from a volume using libdroplet.
 */
ssize_t object_store_device::d_read(int fd, void *buffer, size_t count)
{
   if (m_vfd) {
      unsigned int buflen;
      dpl_status_t status;

      buflen = count;
      status = dpl_pread(m_vfd, count, m_offset, (char **)&buffer, &buflen);

      switch (status) {
      case DPL_SUCCESS:
         m_offset += buflen;
         return buflen;
      default:
         Mmsg2(errmsg, _("Failed to read %s using dpl_read(): ERR=%s.\n"),
               getVolCatName(), dpl_status_str(status));
         return droplet_errno_to_system_errno(status);
      }
   } else {
      errno = EBADF;
      return -1;
   }
}

/**
 * Write data to a volume using libdroplet.
 */
ssize_t object_store_device::d_write(int fd, const void *buffer, size_t count)
{
   if (m_vfd) {
      dpl_status_t status;

      status = dpl_pwrite(m_vfd, (char *)buffer, count, m_offset);
      switch (status) {
      case DPL_SUCCESS:
         m_offset += count;
         return count;
      default:
         Mmsg2(errmsg, _("Failed to write %s using dpl_write(): ERR=%s.\n"),
               getVolCatName(), dpl_status_str(status));
         return droplet_errno_to_system_errno(status);
      }
   } else {
      errno = EBADF;
      return -1;
   }
}

int object_store_device::d_close(int fd)
{
   if (m_vfd) {
      dpl_status_t status;

      status = dpl_close(m_vfd);
      switch (status) {
      case DPL_SUCCESS:
         m_vfd = NULL;
         return 0;
      default:
         m_vfd = NULL;
         return droplet_errno_to_system_errno(status);
      }
   } else {
      errno = EBADF;
      return -1;
   }
}

int object_store_device::d_ioctl(int fd, ioctl_req_t request, char *op)
{
   return -1;
}

/**
 * Open a directory on the object store and find out size information for a file.
 */
static inline size_t object_store_get_file_size(dpl_ctx_t *ctx, const char *filename)
{
   void *dir_hdl;
   dpl_status_t status;
   dpl_dirent_t dirent;
   size_t filesize = -1;

   status = dpl_opendir(ctx, ".", &dir_hdl);
   switch (status) {
   case DPL_SUCCESS:
      break;
   default:
      return -1;
   }

   while (!dpl_eof(dir_hdl)) {
      if (bstrcasecmp(dirent.name, filename)) {
         filesize = dirent.size;
         break;
      }
   }

   dpl_closedir(dir_hdl);

   return filesize;
}

boffset_t object_store_device::d_lseek(DCR *dcr, boffset_t offset, int whence)
{
   switch (whence) {
   case SEEK_SET:
      m_offset = offset;
      break;
   case SEEK_CUR:
      m_offset += offset;
      break;
   case SEEK_END: {
      size_t filesize;

      filesize = object_store_get_file_size(m_ctx, getVolCatName());
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

bool object_store_device::d_truncate(DCR *dcr)
{
   /*
    * libdroplet doesn't have a truncate function so unlink the volume and create a new empty one.
    */
   if (m_vfd) {
      dpl_status_t status;
      dpl_vfile_flag_t dpl_flags;
      dpl_option_t dpl_options;

      status = dpl_close(m_vfd);
      switch (status) {
      case DPL_SUCCESS:
         m_vfd = NULL;
         break;
      default:
         Mmsg2(errmsg, _("Failed to close %s using dpl_close(): ERR=%s.\n"),
               getVolCatName(), dpl_status_str(status));
         return false;
      }

      status = dpl_unlink(m_ctx, getVolCatName());
      switch (status) {
      case DPL_SUCCESS:
         break;
      default:
         Mmsg2(errmsg, _("Failed to unlink %s using dpl_unlink(): ERR=%s.\n"),
               getVolCatName(), dpl_status_str(status));
         return false;
      }

      /*
       * Create some options for libdroplet.
       *
       * DPL_OPTION_NOALLOC - we provide the buffer to copy the data into
       *                      no need to let the library allocate memory we
       *                      need to free after copying the data.
       */
      memset(&dpl_options, 0, sizeof(dpl_options));
      dpl_options.mask |= DPL_OPTION_NOALLOC;

      dpl_flags = DPL_VFILE_FLAG_CREAT | DPL_VFILE_FLAG_RDWR;
      status = dpl_open(m_ctx, /* context */
                        getVolCatName(), /* locator */
                        dpl_flags, /* flags */
                        &dpl_options, /* options */
                        NULL, /* condition */
                        NULL, /* metadata */
                        NULL, /* sysmd */
                        NULL, /* query_params */
                        NULL, /* stream_status */
                        &m_vfd);

      switch (status) {
      case DPL_SUCCESS:
         break;
      default:
         Mmsg2(errmsg, _("Failed to open %s using dpl_open(): ERR=%s.\n"),
               getVolCatName(), dpl_status_str(status));
         return false;
      }
   }

   return true;
}

object_store_device::~object_store_device()
{
   if (m_ctx) {
      dpl_ctx_free(m_ctx);
      m_ctx = NULL;
   }

   if (m_object_configstring) {
      free(m_object_configstring);
   }

   P(mutex);
   droplet_reference_count--;
   if (droplet_reference_count == 0) {
      dpl_free();
   }
   V(mutex);
}

object_store_device::object_store_device()
{
   m_object_configstring = NULL;
   m_object_bucketname = NULL;
   m_ctx = NULL;
}

#ifdef HAVE_DYNAMIC_SD_BACKENDS
extern "C" DEVICE SD_IMP_EXP *backend_instantiate(JCR *jcr, int device_type)
{
   DEVICE *dev = NULL;

   switch (device_type) {
   case B_OBJECT_STORE_DEV:
      dev = New(object_store_device);
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
#endif /* HAVE_DROPLET */
