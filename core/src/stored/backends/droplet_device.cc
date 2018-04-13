/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2014-2017 Planets Communications B.V.
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
 *
 * Object Storage API device abstraction.
 *
 * Stacking is the following:
 *
 *   droplet_device::
 *         |
 *         v
 *   chunked_device::
 *         |
 *         v
 *       Device::
 *
 */
/**
 * @file
 * Object Storage API device abstraction.
 */

#include "bareos.h"

#ifdef HAVE_DROPLET
#include "stored.h"
#include "chunked_device.h"
#include "droplet_device.h"

/**
 * Options that can be specified for this device type.
 */
enum device_option_type {
   argument_none = 0,
   argument_profile,
   argument_location,
   argument_canned_acl,
   argument_storage_class,
   argument_bucket,
   argument_chunksize,
   argument_iothreads,
   argument_ioslots,
   argument_retries,
   argument_mmap
};

struct device_option {
   const char *name;
   enum device_option_type type;
   int compare_size;
};

static device_option device_options[] = {
   { "profile=", argument_profile, 8 },
   { "location=", argument_location, 9 },
   { "acl=", argument_canned_acl, 4 },
   { "storageclass=", argument_storage_class, 13 },
   { "bucket=", argument_bucket, 7 },
   { "chunksize=", argument_chunksize, 10 },
   { "iothreads=", argument_iothreads, 10 },
   { "ioslots=", argument_ioslots, 8 },
   { "retries=", argument_retries, 8 },
   { "mmap", argument_mmap, 4 },
   { NULL, argument_none }
};

static int droplet_reference_count = 0;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

/**
 * Generic log function that glues libdroplet with BAREOS.
 */
static void droplet_device_logfunc(dpl_ctx_t *ctx, dpl_log_level_t level, const char *message)
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
   default:
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
   case DPL_ETIMEOUT:
      errno = ETIMEDOUT;
   case DPL_ENOMEM:
      errno = ENOMEM;
      break;
   case DPL_EIO:
      errno = EIO;
      break;
   case DPL_ENAMETOOLONG:
      errno = ENAMETOOLONG;
      break;
   case DPL_ENOTDIR:
      errno = ENOTDIR;
      break;
   case DPL_ENOTEMPTY:
      errno = ENOTEMPTY;
      break;
   case DPL_EISDIR:
      errno = EISDIR;
      break;
   case DPL_EEXIST:
      errno = EEXIST;
      break;
   case DPL_EPERM:
      errno = EPERM;
      break;
   default:
      errno = EINVAL;
      break;
   }

   return errno;
}

/**
 * Generic callback for the walk_dpl_directory() function.
 *
 * Returns true - abort loop
 *         false - continue loop
 */
typedef bool (*t_call_back)(dpl_dirent_t *dirent, dpl_ctx_t *ctx,
                            const char *dirname, void *data);

/*
 * Callback for getting the total size of a chunked volume.
 */
static bool chunked_volume_size_callback(dpl_dirent_t *dirent, dpl_ctx_t *ctx,
                                         const char *dirname, void *data)
{
   ssize_t *volumesize = (ssize_t *)data;

   /*
    * Make sure it starts with [0-9] e.g. a volume chunk.
    */
   if (*dirent->name >= '0' && *dirent->name <= '9') {
      *volumesize = *volumesize + dirent->size;
   }

   return false;
}

/*
 * Callback for truncating a chunked volume.
 */
static bool chunked_volume_truncate_callback(dpl_dirent_t *dirent, dpl_ctx_t *ctx,
                                             const char *dirname, void *data)
{
   dpl_status_t status;

   /*
    * Make sure it starts with [0-9] e.g. a volume chunk.
    */
   if (*dirent->name >= '0' && *dirent->name <= '9') {
      status = dpl_unlink(ctx, dirent->name);

      switch (status) {
      case DPL_SUCCESS:
         break;
      default:
         return true;
      }
   }

   return false;
}

/*
 * Generic function that walks a dirname and calls the callback
 * function for each entry it finds in that directory.
 */
static bool walk_dpl_directory(dpl_ctx_t *ctx, const char *dirname, t_call_back callback, void *data)
{
   void *dir_hdl;
   dpl_status_t status;
   dpl_dirent_t dirent;

   if (dirname) {
      status = dpl_chdir(ctx, dirname);

      switch (status) {
      case DPL_SUCCESS:
         break;
      default:
         return false;
      }
   }

   status = dpl_opendir(ctx, ".", &dir_hdl);

   switch (status) {
   case DPL_SUCCESS:
      break;
   default:
      return false;
   }

   while (!dpl_eof(dir_hdl)) {
      status = dpl_readdir(dir_hdl, &dirent);

      switch (status) {
      case DPL_SUCCESS:
         break;
      default:
         dpl_closedir(dir_hdl);
         return false;
      }

      /*
       * Skip '.' and '..'
       */
      if (bstrcmp(dirent.name, ".") ||
          bstrcmp(dirent.name, "..")) {
         continue;
      }

      if (callback(&dirent, ctx, dirname, data)) {
         break;
      }
   }

   dpl_closedir(dir_hdl);

   if (dirname) {
      status = dpl_chdir(ctx, "/");

      switch (status) {
      case DPL_SUCCESS:
         break;
      default:
         return false;
      }
   }

   return true;
}

/*
 * Internal method for flushing a chunk to the backing store.
 * This does the real work either by being called from a
 * io-thread or directly blocking the device.
 */
bool droplet_device::flush_remote_chunk(chunk_io_request *request)
{
   bool retval = false;
   dpl_status_t status;
   dpl_option_t dpl_options;
   dpl_sysmd_t *sysmd = NULL;
   PoolMem chunk_dir(PM_FNAME),
            chunk_name(PM_FNAME);

   Mmsg(chunk_dir, "/%s", request->volname);
   Mmsg(chunk_name, "%s/%04d", chunk_dir.c_str(), request->chunk);

   /*
    * Set that we are uploading the chunk.
    */
   if (!set_inflight_chunk(request)) {
      goto bail_out;
   }

   Dmsg1(100, "Flushing chunk %s\n", chunk_name.c_str());

   /*
    * Check on the remote backing store if the chunk already exists.
    * We only upload this chunk if it is bigger then the chunk that exists
    * on the remote backing store. When using io-threads it could happen
    * that there are multiple flush requests for the same chunk when a
    * chunk is reused in a next backup job. We only want the chunk with
    * the biggest amount of valid data to persist as we only append to
    * chunks.
    */
   sysmd = dpl_sysmd_dup(&sysmd_);
   status = dpl_getattr(ctx_, /* context */
                        chunk_name.c_str(), /* locator */
                        NULL, /* metadata */
                        sysmd); /* sysmd */

   switch (status) {
   case DPL_SUCCESS:
      if (sysmd->size > request->wbuflen) {
         retval = true;
         goto bail_out;
      }
      break;
   default:
      /*
       * Check on the remote backing store if the chunkdir exists.
       */
      dpl_sysmd_free(sysmd);
      sysmd = dpl_sysmd_dup(&sysmd_);
      status = dpl_getattr(ctx_, /* context */
                           chunk_dir.c_str(), /* locator */
                           NULL, /* metadata */
                           sysmd); /* sysmd */

      switch (status) {
      case DPL_SUCCESS:
         break;
      case DPL_ENOENT:
      case DPL_FAILURE:
         /*
          * Make sure the chunk directory with the name of the volume exists.
          */
         dpl_sysmd_free(sysmd);
         sysmd = dpl_sysmd_dup(&sysmd_);
         status = dpl_mkdir(ctx_, /* context */
                            chunk_dir.c_str(), /* locator */
                            NULL, /* metadata */
                            sysmd);/* sysmd */

         switch (status) {
         case DPL_SUCCESS:
            break;
         default:
            Mmsg2(errmsg, _("Failed to create direcory %s using dpl_mkdir(): ERR=%s.\n"),
                  chunk_dir.c_str(), dpl_status_str(status));
            dev_errno = droplet_errno_to_system_errno(status);
            goto bail_out;
         }
         break;
      default:
         break;
      }
      break;
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

   dpl_sysmd_free(sysmd);
   sysmd = dpl_sysmd_dup(&sysmd_);
   status = dpl_fput(ctx_, /* context */
                     chunk_name.c_str(), /* locator */
                     &dpl_options, /* options */
                     NULL, /* condition */
                     NULL, /* range */
                     NULL, /* metadata */
                     sysmd, /* sysmd */
                     (char *)request->buffer, /* data_buf */
                     request->wbuflen); /* data_len */

   switch (status) {
   case DPL_SUCCESS:
      break;
   default:
      Mmsg2(errmsg, _("Failed to flush %s using dpl_fput(): ERR=%s.\n"),
            chunk_name.c_str(), dpl_status_str(status));
      dev_errno = droplet_errno_to_system_errno(status);
      goto bail_out;
   }

   retval = true;

bail_out:
   /*
    * Clear that we are uploading the chunk.
    */
   clear_inflight_chunk(request);

   if (sysmd) {
      dpl_sysmd_free(sysmd);
   }

   return retval;
}

/*
 * Internal method for reading a chunk from the remote backing store.
 */
bool droplet_device::read_remote_chunk(chunk_io_request *request)
{
   bool retval = false;
   dpl_status_t status;
   dpl_option_t dpl_options;
   dpl_range_t dpl_range;
   dpl_sysmd_t *sysmd = NULL;
   PoolMem chunk_name(PM_FNAME);

   Mmsg(chunk_name, "/%s/%04d", request->volname, request->chunk);
   Dmsg1(100, "Reading chunk %s\n", chunk_name.c_str());

   /*
    * See if chunk exists.
    */
   sysmd = dpl_sysmd_dup(&sysmd_);
   status = dpl_getattr(ctx_, /* context */
                        chunk_name.c_str(), /* locator */
                        NULL, /* metadata */
                        sysmd); /* sysmd */

   switch (status) {
   case DPL_SUCCESS:
      break;
   default:
      Mmsg1(errmsg, _("Failed to open %s doesn't exist\n"), chunk_name.c_str());
      Dmsg1(100, "%s", errmsg);
      dev_errno = EIO;
      goto bail_out;
   }

   if (sysmd->size > request->wbuflen) {
      Mmsg3(errmsg, _("Failed to read %s (%ld) to big to fit in chunksize of %ld bytes\n"),
            chunk_name.c_str(), sysmd->size, request->wbuflen);
      Dmsg1(100, "%s", errmsg);
      dev_errno = EINVAL;
      goto bail_out;
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

   dpl_range.start = 0;
   dpl_range.end = sysmd->size;
   *request->rbuflen = sysmd->size;
   dpl_sysmd_free(sysmd);
   sysmd = dpl_sysmd_dup(&sysmd_);
   status = dpl_fget(ctx_, /* context */
                     chunk_name.c_str(), /* locator */
                     &dpl_options, /* options */
                     NULL, /* condition */
                     &dpl_range, /* range */
                     (char **)&request->buffer, /* data_bufp */
                     request->rbuflen, /* data_lenp */
                     NULL, /* metadatap */
                     sysmd); /* sysmdp */

   switch (status) {
   case DPL_SUCCESS:
      break;
   case DPL_ENOENT:
      Mmsg1(errmsg, _("Failed to open %s doesn't exist\n"), chunk_name.c_str());
      Dmsg1(100, "%s", errmsg);
      dev_errno = EIO;
      goto bail_out;
   default:
      Mmsg2(errmsg, _("Failed to read %s using dpl_fget(): ERR=%s.\n"),
            chunk_name.c_str(), dpl_status_str(status));
      dev_errno = droplet_errno_to_system_errno(status);
      goto bail_out;
   }

   retval = true;

bail_out:
   if (sysmd) {
      dpl_sysmd_free(sysmd);
   }

   return retval;
}

/*
 * Internal method for truncating a chunked volume on the remote backing store.
 */
bool droplet_device::truncate_remote_chunked_volume(DeviceControlRecord *dcr)
{
   PoolMem chunk_dir(PM_FNAME);

   Mmsg(chunk_dir, "/%s", getVolCatName());
   if (!walk_dpl_directory(ctx_, chunk_dir.c_str(), chunked_volume_truncate_callback, NULL)) {
      return false;
   }

   return true;
}

/*
 * Initialize backend.
 */
bool droplet_device::initialize()
{
   dpl_status_t status;

   /*
    * Initialize the droplet library when its not done previously.
    */
   P(mutex);
   if (droplet_reference_count == 0) {
      dpl_set_log_func(droplet_device_logfunc);

      status = dpl_init();
      switch (status) {
      case DPL_SUCCESS:
         break;
      default:
         V(mutex);
         goto bail_out;
      }
   }
   droplet_reference_count++;
   V(mutex);

   if (!configstring_) {
      int len;
      bool done;
      uint64_t value;
      char *bp, *next_option;

      if (!dev_options) {
         Mmsg0(errmsg, _("No device options configured\n"));
         Emsg0(M_FATAL, 0, errmsg);
         return -1;
      }

      configstring_ = bstrdup(dev_options);

      bp = configstring_;
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
               case argument_profile: {
                  char *profile;

                  /*
                   * Strip any .profile prefix from the libdroplet profile name.
                   */
                  profile = bp + device_options[i].compare_size;
                  len = strlen(profile);
                  if (len > 8 && bstrcasecmp(profile + (len - 8), ".profile")) {
                     profile[len - 8] = '\0';
                  }
                  profile_ = profile;
                  done = true;
                  break;
               }
               case argument_location:
                  location_ = bp + device_options[i].compare_size;
                  done = true;
                  break;
               case argument_canned_acl:
                  canned_acl_ = bp + device_options[i].compare_size;
                  done = true;
                  break;
               case argument_storage_class:
                  storage_class_ = bp + device_options[i].compare_size;
                  done = true;
                  break;
               case argument_bucket:
                  bucketname_ = bp + device_options[i].compare_size;
                  done = true;
                  break;
               case argument_chunksize:
                  size_to_uint64(bp + device_options[i].compare_size, &value);
                  chunk_size_ = value;
                  done = true;
                  break;
               case argument_iothreads:
                  size_to_uint64(bp + device_options[i].compare_size, &value);
                  io_threads_ = value & 0xFF;
                  done = true;
                  break;
               case argument_ioslots:
                  size_to_uint64(bp + device_options[i].compare_size, &value);
                  io_slots_ = value & 0xFF;
                  done = true;
                  break;
               case argument_retries:
                  size_to_uint64(bp + device_options[i].compare_size, &value);
                  retries_ = value & 0xFF;
                  done = true;
                  break;
               case argument_mmap:
                  use_mmap_ = true;
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

      if (!profile_) {
         Mmsg0(errmsg, _("No droplet profile configured\n"));
         Emsg0(M_FATAL, 0, errmsg);
         goto bail_out;
      }
   }

   /*
    * See if we need to setup a new context for this device.
    */
   if (!ctx_) {
      char *bp;
      PoolMem temp(PM_NAME);

      /*
       * Setup global sysmd settings which are cloned for each operation.
       */
      memset(&sysmd_, 0, sizeof(sysmd_));
      if (location_) {
         pm_strcpy(temp, location_);
         sysmd_.mask |= DPL_SYSMD_MASK_LOCATION_CONSTRAINT;
         sysmd_.location_constraint = dpl_location_constraint(temp.c_str());
         if (sysmd_.location_constraint == -1) {
            Mmsg2(errmsg, _("Illegal location argument %s for device %s%s\n"), temp.c_str(), dev_name);
            goto bail_out;
         }
      }

      if (canned_acl_) {
         pm_strcpy(temp, canned_acl_);
         sysmd_.mask |= DPL_SYSMD_MASK_CANNED_ACL;
         sysmd_.canned_acl = dpl_canned_acl(temp.c_str());
         if (sysmd_.canned_acl == -1) {
            Mmsg2(errmsg, _("Illegal canned_acl argument %s for device %s%s\n"), temp.c_str(), dev_name);
            goto bail_out;
         }
      }

      if (storage_class_) {
         pm_strcpy(temp, storage_class_);
         sysmd_.mask |= DPL_SYSMD_MASK_STORAGE_CLASS;
         sysmd_.storage_class = dpl_storage_class(temp.c_str());
         if (sysmd_.storage_class == -1) {
            Mmsg2(errmsg, _("Illegal storage_class argument %s for device %s%s\n"), temp.c_str(), dev_name);
            goto bail_out;
         }
      }

      /*
       * See if this is a path.
       */
      pm_strcpy(temp, profile_);
      bp = strrchr(temp.c_str(), '/');
      if (!bp) {
         /*
          * Only a profile name.
          */
         ctx_ = dpl_ctx_new(NULL, temp.c_str());
      } else {
         if (bp == temp.c_str()) {
            /*
             * Profile in root of filesystem
             */
            ctx_ = dpl_ctx_new("/", bp + 1);
         } else {
            /*
             * Profile somewhere else.
             */
            *bp++ = '\0';
            ctx_ = dpl_ctx_new(temp.c_str(), bp);
         }
      }

      /*
       * If we failed to allocate a new context fail the open.
       */
      if (!ctx_) {
         Mmsg1(errmsg, _("Failed to create a new context using config %s\n"), dev_options);
         Dmsg1(100, "%s", errmsg);
         goto bail_out;
      }

      /*
       * Login if that is needed for this backend.
       */
      status = dpl_login(ctx_);

      switch (status) {
      case DPL_SUCCESS:
         break;
      case DPL_ENOTSUPP:
         /*
          * Backend doesn't support login which is fine.
          */
         break;
      default:
         Mmsg2(errmsg, _("Failed to login for volume %s using dpl_login(): ERR=%s.\n"),
               getVolCatName(), dpl_status_str(status));
         Dmsg1(100, "%s", errmsg);
         goto bail_out;
      }

      /*
       * If a bucketname was defined set it in the context.
       */
      if (bucketname_) {
         ctx_->cur_bucket = bstrdup(bucketname_);
      }
   }

   return true;

bail_out:
   return false;
}

/*
 * Open a volume using libdroplet.
 */
int droplet_device::d_open(const char *pathname, int flags, int mode)
{
   if (!initialize()) {
      return -1;
   }

   return setup_chunk(pathname, flags, mode);
}

/*
 * Read data from a volume using libdroplet.
 */
ssize_t droplet_device::d_read(int fd, void *buffer, size_t count)
{
   return read_chunked(fd, buffer, count);
}

/**
 * Write data to a volume using libdroplet.
 */
ssize_t droplet_device::d_write(int fd, const void *buffer, size_t count)
{
   return write_chunked(fd, buffer, count);
}

int droplet_device::d_close(int fd)
{
   return close_chunk();
}

int droplet_device::d_ioctl(int fd, ioctl_req_t request, char *op)
{
   return -1;
}

/**
 * Open a directory on the backing store and find out size information for a volume.
 */
ssize_t droplet_device::chunked_remote_volume_size()
{
   dpl_status_t status;
   ssize_t volumesize = 0;
   dpl_sysmd_t *sysmd = NULL;
   PoolMem chunk_dir(PM_FNAME);

   Mmsg(chunk_dir, "/%s", getVolCatName());

   /*
    * FIXME: With the current version of libdroplet a dpl_getattr() on a directory
    *        fails with DPL_ENOENT even when the directory does exist. All other
    *        operations succeed and as walk_dpl_directory() does a dpl_chdir() anyway
    *        that will fail if the directory doesn't exist for now we should be
    *        mostly fine.
    */

#if 0
   /*
    * First make sure that the chunkdir exists otherwise it makes little sense to scan it.
    */
   sysmd = dpl_sysmd_dup(&sysmd_);
   status = dpl_getattr(ctx_, /* context */
                        chunk_dir.c_str(), /* locator */
                        NULL, /* metadata */
                        sysmd); /* sysmd */

   switch (status) {
   case DPL_SUCCESS:
      /*
       * Make sure the filetype is a directory and not a file.
       */
      if (sysmd->ftype != DPL_FTYPE_DIR) {
         volumesize = -1;
         goto bail_out;
      }
      break;
   case DPL_ENOENT:
      volumesize = -1;
      goto bail_out;
   default:
      break;
   }
#endif

   if (!walk_dpl_directory(ctx_, chunk_dir.c_str(), chunked_volume_size_callback, &volumesize)) {
      volumesize = -1;
      goto bail_out;
   }

bail_out:
   if (sysmd) {
      dpl_sysmd_free(sysmd);
   }

   Dmsg2(100, "Volume size of volume %s, %lld\n", chunk_dir.c_str(), volumesize);

   return volumesize;
}

boffset_t droplet_device::d_lseek(DeviceControlRecord *dcr, boffset_t offset, int whence)
{
   switch (whence) {
   case SEEK_SET:
      offset_ = offset;
      break;
   case SEEK_CUR:
      offset_ += offset;
      break;
   case SEEK_END: {
      ssize_t volumesize;

      volumesize = chunked_volume_size();

      Dmsg1(100, "Current volumesize: %lld\n", volumesize);

      if (volumesize >= 0) {
         offset_ = volumesize + offset;
      } else {
         return -1;
      }
      break;
   }
   default:
      return -1;
   }

   if (!load_chunk()) {
      return -1;
   }

   return offset_;
}

bool droplet_device::d_truncate(DeviceControlRecord *dcr)
{
   return truncate_chunked_volume(dcr);
}

droplet_device::~droplet_device()
{
   if (ctx_) {
      if (bucketname_ && ctx_->cur_bucket) {
         free(ctx_->cur_bucket);
         ctx_->cur_bucket = NULL;
      }
      dpl_ctx_free(ctx_);
      ctx_ = NULL;
   }

   if (configstring_) {
      free(configstring_);
   }

   P(mutex);
   droplet_reference_count--;
   if (droplet_reference_count == 0) {
      dpl_free();
   }
   V(mutex);
}

droplet_device::droplet_device()
{
   configstring_ = NULL;
   bucketname_ = NULL;
   location_ = NULL;
   canned_acl_ = NULL;
   storage_class_ = NULL;
   ctx_ = NULL;
}

#ifdef HAVE_DYNAMIC_SD_BACKENDS
extern "C" Device SD_IMP_EXP *backend_instantiate(JobControlRecord *jcr, int device_type)
{
   Device *dev = NULL;

   switch (device_type) {
   case B_DROPLET_DEV:
      dev = New(droplet_device);
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
