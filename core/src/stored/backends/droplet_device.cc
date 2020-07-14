/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2014-2017 Planets Communications B.V.
   Copyright (C) 2014-2020 Bareos GmbH & Co. KG

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

#include "include/bareos.h"

#include "stored/stored.h"
#include "stored/sd_backends.h"
#include "chunked_device.h"
#include "droplet_device.h"
#include "lib/edit.h"

#include <string>

namespace storagedaemon {

/**
 * Options that can be specified for this device type.
 */
enum device_option_type
{
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
  const char* name;
  enum device_option_type type;
  int compare_size;
};

static device_option device_options[] = {
    {"profile=", argument_profile, 8},
    {"location=", argument_location, 9},
    {"acl=", argument_canned_acl, 4},
    {"storageclass=", argument_storage_class, 13},
    {"bucket=", argument_bucket, 7},
    {"chunksize=", argument_chunksize, 10},
    {"iothreads=", argument_iothreads, 10},
    {"ioslots=", argument_ioslots, 8},
    {"retries=", argument_retries, 8},
    {"mmap", argument_mmap, 4},
    {NULL, argument_none}};

static int droplet_reference_count = 0;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

/**
 * Generic log function that glues libdroplet with BAREOS.
 */
static void DropletDeviceLogfunc(dpl_ctx_t* ctx,
                                 dpl_log_level_t level,
                                 const char* message)
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
static inline int DropletErrnoToSystemErrno(dpl_status_t status)
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
    case DPL_FAILURE: /**< General failure */
      errno = EIO;
      break;
    default:
      errno = EINVAL;
      break;
  }

  return errno;
}


/*
 * Callback for getting the total size of a chunked volume.
 */
static dpl_status_t chunked_volume_size_callback(dpl_sysmd_t* sysmd,
                                                 dpl_ctx_t* ctx,
                                                 const char* chunkpath,
                                                 void* data)
{
  dpl_status_t status = DPL_SUCCESS;
  ssize_t* volumesize = (ssize_t*)data;

  *volumesize = *volumesize + sysmd->size;

  return status;
}

/*
 * Callback for truncating a chunked volume.
 *
 * @return DPL_SUCCESS on success, on error: a dpl_status_t value that
 * represents the error.
 */
static dpl_status_t chunked_volume_truncate_callback(dpl_sysmd_t* sysmd,
                                                     dpl_ctx_t* ctx,
                                                     const char* chunkpath,
                                                     void* data)
{
  dpl_status_t status = DPL_SUCCESS;

  status = dpl_unlink(ctx, chunkpath);

  switch (status) {
    case DPL_SUCCESS:
      break;
    default:
      /* no error message here, as error will be set by calling function. */
      return status;
  }

  return status;
}


/*
 * Generic function that walks a dirname and calls the callback
 * function for each entry it finds in that directory.
 *
 * @return: true - if no error occured
 *          false - if an error has occured. Sets dev_errno and errmsg to the
 * first error.
 */
bool droplet_device::walk_chunks(const char* dirname,
                                 t_dpl_walk_chunks_call_back callback,
                                 void* data,
                                 bool ignore_gaps)
{
  bool retval = true;
  dpl_status_t status;
  dpl_status_t callback_status;
  dpl_sysmd_t* sysmd = NULL;
  PoolMem path(PM_NAME);

  sysmd = dpl_sysmd_dup(&sysmd_);
  bool found = true;
  int i = 0;
  while ((i < max_chunks_) && (found) && (retval)) {
    path.bsprintf("%s/%04d", dirname, i);

    status = dpl_getattr(ctx_,         /* context */
                         path.c_str(), /* locator */
                         NULL,         /* metadata */
                         sysmd);       /* sysmd */

    switch (status) {
      case DPL_SUCCESS:
        Dmsg1(100, "chunk %s exists. Calling callback.\n", path.c_str());
        callback_status = callback(sysmd, ctx_, path.c_str(), data);
        if (callback_status == DPL_SUCCESS) {
          i++;
        } else {
          Mmsg2(errmsg, _("Operation failed on chunk %s: ERR=%s."),
                path.c_str(), dpl_status_str(callback_status));
          dev_errno = DropletErrnoToSystemErrno(callback_status);
          /* exit loop */
          retval = false;
        }
        break;
      case DPL_ENOENT:
        if (ignore_gaps) {
          Dmsg1(1000, "chunk %s does not exists. Skipped.\n", path.c_str());
          i++;
        } else {
          Dmsg1(100, "chunk %s does not exists. Exiting.\n", path.c_str());
          found = false;
        }
        break;
      default:
        Dmsg2(100, "chunk %s failure: %s. Exiting.\n", path.c_str(),
              dpl_status_str(callback_status));
        found = false;
        break;
    }
  }

  if (sysmd) {
    dpl_sysmd_free(sysmd);
    sysmd = NULL;
  }

  return retval;
}

/**
 * Check if a specific path exists.
 * It uses dpl_getattr() for this.
 * However, dpl_getattr() results wrong results in a couple of situations,
 * espescially directoy names should not be checked using a prepended "/".
 *
 * Results in detail:
 *
 * path      | "name"  | "name/" | target reachable | target not reachable  |
 * target not reachable | wrong credentials | exists  | exists  | | (already
 * initialized) | (not initialized)    |
 * -------------------------------------------------------------------------------------------------------------------
 * ""        | -       | yes     | DPL_SUCCESS      | DPL_SUCCESS (!)       |
 * DPL_FAILURE          | DPL_EPERM
 * "/"       | yes     | -       | DPL_SUCCESS      | DPL_FAILURE           |
 * DPL_FAILURE          | DPL_EPERM
 *
 * "name"    | -       | -       | DPL_ENOENT       | DPL_FAILURE           |
 * DPL_FAILURE          | DPL_EPERM
 * "/name"   | -       | -       | DPL_ENOENT       | DPL_FAILURE           |
 * DPL_FAILURE          | DPL_EPERM "name/"   | -       | -       | DPL_ENOENT
 * | DPL_FAILURE           | DPL_FAILURE          | DPL_EPERM
 * "/name/"  | -       | -       | DPL_SUCCESS (!)  | DPL_SUCCESS (!)       |
 * DPL_SUCCESS (!)      | DPL_SUCCESS (!)
 *
 * "name"    | yes     | -       | DPL_SUCCESS      | DPL_FAILURE           |
 * DPL_FAILURE          | DPL_EPERM
 * "/name"   | yes     | -       | DPL_SUCCESS      | DPL_FAILURE           |
 * DPL_FAILURE          | DPL_EPERM "name/"   | yes     | -       | DPL_ENOTDIR
 * | DPL_FAILURE           | DPL_FAILURE          | DPL_EPERM
 * "/name/"  | yes     | -       | DPL_SUCCESS (!)  | DPL_SUCCESS (!)       |
 * DPL_SUCCESS (!)      | DPL_SUCCESS (!)
 *
 * "name"    | -       | yes     | DPL_SUCCESS      | DPL_FAILURE           |
 * DPL_FAILURE          | DPL_EPERM
 * "/name"   | -       | yes     | DPL_ENOENT  (!)  | DPL_FAILURE           |
 * DPL_FAILURE          | DPL_EPERM "name/"   | -       | yes     | DPL_SUCCESS
 * | DPL_FAILURE           | DPL_FAILURE          | DPL_EPERM
 * "/name/"  | -       | yes     | DPL_SUCCESS      | DPL_SUCCESS (!)       |
 * DPL_SUCCESS (!)      | DPL_SUCCESS (!)
 *
 * "name"    | yes     | yes     | DPL_SUCCESS      | DPL_FAILURE           |
 * DPL_FAILURE          | DPL_EPERM
 * "/name"   | yes     | yes     | DPL_SUCCESS      | DPL_FAILURE           |
 * DPL_FAILURE          | DPL_EPERM "name/"   | yes     | yes     | DPL_SUCCESS
 * | DPL_FAILURE           | DPL_FAILURE          | DPL_EPERM
 * "/name/"  | yes     | yes     | DPL_SUCCESS      | DPL_SUCCESS (!)       |
 * DPL_SUCCESS (!)      | DPL_SUCCESS (!)
 *
 * Best test for
 *   directories as "dir/"
 *   files as "file" or "/file".
 *
 * Returns DPL_SUCCESS     - if path exists and can be accessed
 *         DPL_* errorcode - otherwise
 */
dpl_status_t droplet_device::check_path(const char* path)
{
  dpl_status_t status;
  dpl_sysmd_t* sysmd = NULL;

  sysmd = dpl_sysmd_dup(&sysmd_);
  status = dpl_getattr(ctx_,   /* context */
                       path,   /* locator */
                       NULL,   /* metadata */
                       sysmd); /* sysmd */
  Dmsg4(100, "check_path: path=<%s> (device=%s, bucket=%s): Result %s\n", path,
        prt_name, ctx_->cur_bucket, dpl_status_str(status));
  dpl_sysmd_free(sysmd);

  return status;
}

/**
 * Checks if the connection to the backend storage system is possible.
 *
 * Returns true  - if connection can be established
 *         false - otherwise
 */
bool droplet_device::CheckRemote()
{
  if (!ctx_) {
    if (!initialize()) { return false; }
  }

  auto status = check_path("/");

  const char* h = dpl_addrlist_get(ctx_->addrlist);
  std::string hostaddr{h != nullptr ? h : "???"};

  switch (status) {
    case DPL_SUCCESS:
      Dmsg1(100, "Host is accessible: %s\n", hostaddr.c_str());
      return true;
    case DPL_ENOENT:
      Dmsg2(100,
            "Host is accessible: %s (%s), probably the host should be"
            " configured to accept virtual-host-style requests\n",
            hostaddr.c_str(), dpl_status_str(status));
      return true;
    default:
      Dmsg2(100, "Cannot reach host: %s (%s)\n ", hostaddr.c_str(),
            dpl_status_str(status));
      return false;
  }
}


bool droplet_device::remote_chunked_volume_exists()
{
  bool retval = false;
  dpl_status_t status;
  PoolMem chunk_dir(PM_FNAME);

  if (!CheckRemote()) { return false; }

  Mmsg(chunk_dir, "%s/", getVolCatName());
  status = check_path(chunk_dir.c_str());

  switch (status) {
    case DPL_SUCCESS:
      Dmsg1(100, "Remote chunked volume %s exists\n", chunk_dir.c_str());
      retval = true;
      break;
    case DPL_ENOENT:
    case DPL_FAILURE:
    default:
      Dmsg1(100, "Remote chunked volume %s does not exist\n",
            chunk_dir.c_str());
      break;
  }

  return retval;
}


/*
 * Internal method for flushing a chunk to the backing store.
 * This does the real work either by being called from a
 * io-thread or directly blocking the device.
 */
bool droplet_device::FlushRemoteChunk(chunk_io_request* request)
{
  bool retval = false;
  dpl_status_t status;
  dpl_option_t dpl_options;
  dpl_sysmd_t* sysmd = NULL;
  PoolMem chunk_dir(PM_FNAME), chunk_name(PM_FNAME);

  Mmsg(chunk_dir, "/%s", request->volname);
  Mmsg(chunk_name, "%s/%04d", chunk_dir.c_str(), request->chunk);

  /*
   * Set that we are uploading the chunk.
   */
  if (!SetInflightChunk(request)) { goto bail_out; }

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
  status = dpl_getattr(ctx_,               /* context */
                       chunk_name.c_str(), /* locator */
                       NULL,               /* metadata */
                       sysmd);             /* sysmd */

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
      status = dpl_getattr(ctx_,              /* context */
                           chunk_dir.c_str(), /* locator */
                           NULL,              /* metadata */
                           sysmd);            /* sysmd */

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
          status = dpl_mkdir(ctx_,              /* context */
                             chunk_dir.c_str(), /* locator */
                             NULL,              /* metadata */
                             sysmd);            /* sysmd */

          switch (status) {
            case DPL_SUCCESS:
              break;
            default:
              Mmsg2(errmsg,
                    _("Failed to create directory %s using dpl_mkdir(): "
                      "ERR=%s.\n"),
                    chunk_dir.c_str(), dpl_status_str(status));
              dev_errno = DropletErrnoToSystemErrno(status);
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
  status = dpl_fput(ctx_,                   /* context */
                    chunk_name.c_str(),     /* locator */
                    &dpl_options,           /* options */
                    NULL,                   /* condition */
                    NULL,                   /* range */
                    NULL,                   /* metadata */
                    sysmd,                  /* sysmd */
                    (char*)request->buffer, /* data_buf */
                    request->wbuflen);      /* data_len */

  switch (status) {
    case DPL_SUCCESS:
      break;
    default:
      Mmsg2(errmsg, _("Failed to flush %s using dpl_fput(): ERR=%s.\n"),
            chunk_name.c_str(), dpl_status_str(status));
      dev_errno = DropletErrnoToSystemErrno(status);
      goto bail_out;
  }

  retval = true;

bail_out:
  /*
   * Clear that we are uploading the chunk.
   */
  ClearInflightChunk(request);

  if (sysmd) { dpl_sysmd_free(sysmd); }

  return retval;
}

/*
 * Internal method for reading a chunk from the remote backing store.
 */
bool droplet_device::ReadRemoteChunk(chunk_io_request* request)
{
  bool retval = false;
  dpl_status_t status;
  dpl_option_t dpl_options;
  dpl_range_t dpl_range;
  dpl_sysmd_t* sysmd = NULL;
  PoolMem chunk_name(PM_FNAME);

  Mmsg(chunk_name, "/%s/%04d", request->volname, request->chunk);
  Dmsg1(100, "Reading chunk %s\n", chunk_name.c_str());

  /*
   * See if chunk exists.
   */
  sysmd = dpl_sysmd_dup(&sysmd_);
  status = dpl_getattr(ctx_,               /* context */
                       chunk_name.c_str(), /* locator */
                       NULL,               /* metadata */
                       sysmd);             /* sysmd */

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
    Mmsg3(
        errmsg,
        _("Failed to read %s (%ld) to big to fit in chunksize of %ld bytes\n"),
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
  status = dpl_fget(ctx_,                     /* context */
                    chunk_name.c_str(),       /* locator */
                    &dpl_options,             /* options */
                    NULL,                     /* condition */
                    &dpl_range,               /* range */
                    (char**)&request->buffer, /* data_bufp */
                    request->rbuflen,         /* data_lenp */
                    NULL,                     /* metadatap */
                    sysmd);                   /* sysmdp */

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
      dev_errno = DropletErrnoToSystemErrno(status);
      goto bail_out;
  }

  retval = true;

bail_out:
  if (sysmd) { dpl_sysmd_free(sysmd); }

  return retval;
}

/*
 * Internal method for truncating a chunked volume on the remote backing store.
 */
bool droplet_device::TruncateRemoteChunkedVolume(DeviceControlRecord* dcr)
{
  PoolMem chunk_dir(PM_FNAME);

  Dmsg1(100, "truncate_remote_chunked_volume(%s) start.\n", getVolCatName());
  Mmsg(chunk_dir, "/%s", getVolCatName());
  bool ignore_gaps = true;
  if (!walk_chunks(chunk_dir.c_str(), chunked_volume_truncate_callback, NULL,
                   ignore_gaps)) {
    /* errno already set in walk_chunks. */
    return false;
  }
  Dmsg1(100, "truncate_remote_chunked_volume(%s) finished.\n", getVolCatName());

  return true;
}


bool droplet_device::d_flush(DeviceControlRecord* dcr)
{
  return WaitUntilChunksWritten();
};

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
    dpl_set_log_func(DropletDeviceLogfunc);

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

    configstring_ = strdup(dev_options);

    bp = configstring_;
    while (bp) {
      next_option = strchr(bp, ',');
      if (next_option) { *next_option++ = '\0'; }

      done = false;
      for (int i = 0; !done && device_options[i].name; i++) {
        /*
         * Try to find a matching device option.
         */
        if (bstrncasecmp(bp, device_options[i].name,
                         device_options[i].compare_size)) {
          switch (device_options[i].type) {
            case argument_profile: {
              char* profile;

              /*
               * Strip any .profile prefix from the libdroplet profile name.
               */
              profile = bp + device_options[i].compare_size;
              len = strlen(profile);
              if (len > 8 && Bstrcasecmp(profile + (len - 8), ".profile")) {
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
    char* bp;
    PoolMem temp(PM_NAME);

    /*
     * Setup global sysmd settings which are cloned for each operation.
     */
    memset(&sysmd_, 0, sizeof(sysmd_));
    if (location_) {
      PmStrcpy(temp, location_);
      sysmd_.mask |= DPL_SYSMD_MASK_LOCATION_CONSTRAINT;
      sysmd_.location_constraint = dpl_location_constraint(temp.c_str());
      if (sysmd_.location_constraint == -1) {
        Mmsg2(errmsg, _("Illegal location argument %s for device %s%s\n"),
              temp.c_str(), dev_name);
        goto bail_out;
      }
    }

    if (canned_acl_) {
      PmStrcpy(temp, canned_acl_);
      sysmd_.mask |= DPL_SYSMD_MASK_CANNED_ACL;
      sysmd_.canned_acl = dpl_canned_acl(temp.c_str());
      if (sysmd_.canned_acl == -1) {
        Mmsg2(errmsg, _("Illegal canned_acl argument %s for device %s%s\n"),
              temp.c_str(), dev_name);
        goto bail_out;
      }
    }

    if (storage_class_) {
      PmStrcpy(temp, storage_class_);
      sysmd_.mask |= DPL_SYSMD_MASK_STORAGE_CLASS;
      sysmd_.storage_class = dpl_storage_class(temp.c_str());
      if (sysmd_.storage_class == -1) {
        Mmsg2(errmsg, _("Illegal storage_class argument %s for device %s%s\n"),
              temp.c_str(), dev_name);
        goto bail_out;
      }
    }

    /*
     * See if this is a path.
     */
    PmStrcpy(temp, profile_);
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
      Mmsg1(errmsg, _("Failed to create a new context using config %s\n"),
            dev_options);
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
        Mmsg2(errmsg,
              _("Failed to login for volume %s using dpl_login(): ERR=%s.\n"),
              getVolCatName(), dpl_status_str(status));
        Dmsg1(100, "%s", errmsg);
        goto bail_out;
    }

    /*
     * If a bucketname was defined set it in the context.
     */
    if (bucketname_) { ctx_->cur_bucket = strdup(bucketname_); }
  }

  return true;

bail_out:
  return false;
}

/*
 * Open a volume using libdroplet.
 */
int droplet_device::d_open(const char* pathname, int flags, int mode)
{
  if (!initialize()) { return -1; }

  return SetupChunk(pathname, flags, mode);
}

/*
 * Read data from a volume using libdroplet.
 */
ssize_t droplet_device::d_read(int fd, void* buffer, size_t count)
{
  return ReadChunked(fd, buffer, count);
}

/**
 * Write data to a volume using libdroplet.
 */
ssize_t droplet_device::d_write(int fd, const void* buffer, size_t count)
{
  return WriteChunked(fd, buffer, count);
}

int droplet_device::d_close(int fd) { return CloseChunk(); }

int droplet_device::d_ioctl(int fd, ioctl_req_t request, char* op)
{
  return -1;
}

/**
 * Open a directory on the backing store and find out size information for a
 * volume.
 */
ssize_t droplet_device::chunked_remote_volume_size()
{
  ssize_t volumesize = 0;
  dpl_sysmd_t* sysmd = NULL;
  PoolMem chunk_dir(PM_FNAME);

  Mmsg(chunk_dir, "/%s", getVolCatName());

  /*
   * FIXME: With the current version of libdroplet a dpl_getattr() on a
   * directory fails with DPL_ENOENT even when the directory does exist. All
   * other operations succeed and as walk_chunks() does a dpl_chdir() anyway
   *        that will fail if the directory doesn't exist for now we should be
   *        mostly fine.
   */

  Dmsg1(100, "get chunked_remote_volume_size(%s)\n", getVolCatName());
  if (!walk_chunks(chunk_dir.c_str(), chunked_volume_size_callback,
                   &volumesize)) {
    /* errno is already set in walk_chunks */
    volumesize = -1;
    goto bail_out;
  }

bail_out:
  if (sysmd) { dpl_sysmd_free(sysmd); }

  Dmsg2(100, "Size of volume %s: %lld\n", chunk_dir.c_str(), volumesize);

  return volumesize;
}

boffset_t droplet_device::d_lseek(DeviceControlRecord* dcr,
                                  boffset_t offset,
                                  int whence)
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

      volumesize = ChunkedVolumeSize();

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

  if (!LoadChunk()) { return -1; }

  return offset_;
}

bool droplet_device::d_truncate(DeviceControlRecord* dcr)
{
  return TruncateChunkedVolume(dcr);
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

  if (configstring_) { free(configstring_); }

  P(mutex);
  droplet_reference_count--;
  if (droplet_reference_count == 0) { dpl_free(); }
  V(mutex);
}

class Backend : public BackendInterface {
 public:
  Device* GetDevice(JobControlRecord* jcr, DeviceType device_type) override
  {
    switch (device_type) {
      case DeviceType::B_DROPLET_DEV:
        return new droplet_device;
      default:
        Jmsg(jcr, M_FATAL, 0, _("Request for unknown devicetype: %d\n"),
             device_type);
        return nullptr;
    }
  }
  void FlushDevice(void) override {}
};

#ifdef HAVE_DYNAMIC_SD_BACKENDS
extern "C" BackendInterface* GetBackend(void) { return new Backend; }
#endif
} /* namespace storagedaemon */
