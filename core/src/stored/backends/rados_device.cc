/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2014-2016 Planets Communications B.V.
   Copyright (C) 2014-2021 Bareos GmbH & Co. KG

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
// Marco van Wieringen, February 2014
/**
 * @file
 * CEPH (librados) API device abstraction.
 */

#include "include/bareos.h"

#ifdef HAVE_RADOS
#  include "stored/stored.h"
#  include "stored/sd_backends.h"
#  include "rados_device.h"
#  include "lib/berrno.h"
#  include "lib/edit.h"

namespace storagedaemon {

// Options that can be specified for this device type.
enum device_option_type
{
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
  const char* name;
  enum device_option_type type;
  int compare_size;
};

static device_option device_options[]
    = {{"conffile=", argument_conffile, 9},
       {"poolname=", argument_poolname, 9},
#  if LIBRADOS_VERSION_CODE < 17408
       {"clientid=", argument_clientid, 9},
#  else
       {"clustername=", argument_clustername, 12},
       {"username=", argument_username, 9},
#  endif
#  ifdef HAVE_RADOS_STRIPER
       {"striped", argument_striped, 7},
       {"stripe_unit=", argument_stripe_unit, 12},
       {"object_size=", argument_object_size, 12},
       {"stripe_count=", argument_stripe_count, 13},
#  endif
       {NULL, argument_none}};

// Open a volume using librados.
int rados_device::d_open(const char* pathname, int flags, int mode)
{
  int status;
  uint64_t object_size;
  time_t object_mtime;
  BErrNo be;

  if (!rados_configstring_) {
    char *bp, *next_option;
    bool done;

    if (!dev_options) {
      Mmsg0(errmsg, _("No device options configured\n"));
      Emsg0(M_FATAL, 0, errmsg);
      goto bail_out;
    }

    rados_configstring_ = strdup(dev_options);

    bp = rados_configstring_;
    while (bp) {
      next_option = strchr(bp, ',');
      if (next_option) { *next_option++ = '\0'; }

      done = false;
      for (int i = 0; !done && device_options[i].name; i++) {
        // Try to find a matching device option.
        if (bstrncasecmp(bp, device_options[i].name,
                         device_options[i].compare_size)) {
          switch (device_options[i].type) {
            case argument_conffile:
              rados_conffile_ = bp + device_options[i].compare_size;
              done = true;
              break;
#  if LIBRADOS_VERSION_CODE < 17408
            case argument_clientid:
              rados_clientid_ = bp + device_options[i].compare_size;
              done = true;
              break;
#  else
            case argument_clustername:
              rados_clustername_ = bp + device_options[i].compare_size;
              done = true;
              break;
            case argument_username:
              rados_username_ = bp + device_options[i].compare_size;
              done = true;
              break;
#  endif
            case argument_poolname:
              rados_poolname_ = bp + device_options[i].compare_size;
              done = true;
              break;
#  ifdef HAVE_RADOS_STRIPER
            case argument_striped:
              stripe_volume_ = true;
              done = true;
              break;
            case argument_stripe_unit:
              size_to_uint64(bp + device_options[i].compare_size,
                             &stripe_unit_);
              done = true;
              break;
            case argument_object_size:
              size_to_uint64(bp + device_options[i].compare_size,
                             &object_size_);
              done = true;
              break;
            case argument_stripe_count:
              stripe_count_ = str_to_int64(bp + device_options[i].compare_size);
              done = true;
              break;
#  endif
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

    if (!rados_conffile_) {
      Mmsg0(errmsg, _("No rados config file configured\n"));
      Emsg0(M_FATAL, 0, errmsg);
      goto bail_out;
    }

#  if LIBRADOS_VERSION_CODE < 17408
    if (!rados_clientid_) {
      Mmsg1(errmsg, _("No client id configured defaulting to %s\n"),
            DEFAULT_CLIENTID);
      rados_clientid_ = strdup(DEFAULT_CLIENTID);
    }
#  else
    if (!rados_clustername_) {
      rados_clustername_ = strdup(DEFAULT_CLUSTERNAME);
    }
    if (!rados_username_) {
      Mmsg1(errmsg, _("No username configured defaulting to %s\n"),
            DEFAULT_USERNAME);
      rados_username_ = strdup(DEFAULT_USERNAME);
    }
#  endif

    if (!rados_poolname_) {
      Mmsg0(errmsg, _("No rados pool configured\n"));
      Emsg0(M_FATAL, 0, errmsg);
      goto bail_out;
    }
  }

  if (!cluster_initialized_) {
#  if LIBRADOS_VERSION_CODE >= 17408
    uint64_t rados_flags = 0;
#  endif

    /*
     * Use for versions lower then 0.69.1 the old rados_create() and
     * for later version rados_create2() calls.
     */
#  if LIBRADOS_VERSION_CODE < 17408
    status = rados_create(&cluster_, rados_clientid_);
#  else
    status = rados_create2(&cluster_, rados_clustername_, rados_username_,
                           rados_flags);
#  endif
    if (status < 0) {
      Mmsg1(errmsg, _("Unable to create RADOS cluster: ERR=%s\n"),
            be.bstrerror(-status));
      Emsg0(M_FATAL, 0, errmsg);
      goto bail_out;
    }

    status = rados_conf_read_file(cluster_, rados_conffile_);
    if (status < 0) {
      Mmsg2(errmsg, _("Unable to read RADOS config %s: ERR=%s\n"),
            rados_conffile_, be.bstrerror(-status));
      Emsg0(M_FATAL, 0, errmsg);
      rados_shutdown(cluster_);
      goto bail_out;
    }

    status = rados_connect(cluster_);
    if (status < 0) {
      Mmsg1(errmsg, _("Unable to connect to RADOS cluster: ERR=%s\n"),
            be.bstrerror(-status));
      Emsg0(M_FATAL, 0, errmsg);
      rados_shutdown(cluster_);
      goto bail_out;
    }

    cluster_initialized_ = true;
  }

  if (!ctx_) {
    status = rados_ioctx_create(cluster_, rados_poolname_, &ctx_);
    if (status < 0) {
      Mmsg2(errmsg,
            _("Unable to create RADOS IO context for pool %s: ERR=%s\n"),
            rados_poolname_, be.bstrerror(-status));
      Emsg0(M_FATAL, 0, errmsg);
      goto bail_out;
    }

#  ifdef HAVE_RADOS_STRIPER
    if (stripe_volume_) {
      status = rados_striper_create(ctx_, &striper_);
      if (status < 0) {
        Mmsg2(errmsg,
              _("Unable to create RADOS striper object for pool %s: ERR=%s\n"),
              rados_poolname_, be.bstrerror(-status));
        Emsg0(M_FATAL, 0, errmsg);
        goto bail_out;
      }

      status
          = rados_striper_set_object_layout_stripe_unit(striper_, stripe_unit_);
      if (status < 0) {
        Mmsg3(errmsg,
              _("Unable to set RADOS striper unit size to %d  for pool %s: "
                "ERR=%s\n"),
              stripe_unit_, rados_poolname_, be.bstrerror(-status));
        Emsg0(M_FATAL, 0, errmsg);
        goto bail_out;
      }

      status = rados_striper_set_object_layout_stripe_count(striper_,
                                                            stripe_count_);
      if (status < 0) {
        Mmsg3(errmsg,
              _("Unable to set RADOS striper stripe count to %d  for pool %s: "
                "ERR=%s\n"),
              stripe_count_, rados_poolname_, be.bstrerror(-status));
        Emsg0(M_FATAL, 0, errmsg);
        goto bail_out;
      }

      status
          = rados_striper_set_object_layout_object_size(striper_, object_size_);
      if (status < 0) {
        Mmsg3(errmsg,
              _("Unable to set RADOS striper object size to %d  for pool %s: "
                "ERR=%s\n"),
              object_size_, rados_poolname_, be.bstrerror(-status));
        Emsg0(M_FATAL, 0, errmsg);
        goto bail_out;
      }
    }
#  endif
  }

  Mmsg(virtual_filename_, "%s", getVolCatName());

  // See if the object already exists.
#  ifdef HAVE_RADOS_STRIPER
  if (stripe_volume_) {
    status = rados_striper_stat(striper_, virtual_filename_, &object_size,
                                &object_mtime);
  } else {
    status = rados_stat(ctx_, virtual_filename_, &object_size, &object_mtime);
  }
#  else
  status = rados_stat(ctx_, virtual_filename_, &object_size, &object_mtime);
#  endif

  // See if the O_CREAT flag is set.
  if (flags & O_CREAT) {
    if (status < 0) {
      switch (status) {
        case -ENOENT:
          /*
           * Create an empty object.
           * e.g. write one byte and then truncate it to zero bytes.
           */
#  ifdef HAVE_RADOS_STRIPER
          if (stripe_volume_) {
            rados_striper_write(striper_, virtual_filename_, " ", 1, 0);
            rados_striper_trunc(striper_, virtual_filename_, 0);
          } else {
#  endif
            rados_write(ctx_, virtual_filename_, " ", 1, 0);
            rados_trunc(ctx_, virtual_filename_, 0);
#  ifdef HAVE_RADOS_STRIPER
          }
#  endif
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

  offset_ = 0;

  return 0;

bail_out:
  if (cluster_initialized_) {
    rados_shutdown(cluster_);
    cluster_initialized_ = false;
  }

  return -1;
}

// Read some data from an object at a given offset.
ssize_t rados_device::ReadObjectData(boffset_t offset,
                                     char* buffer,
                                     size_t count)
{
#  ifdef HAVE_RADOS_STRIPER
  if (stripe_volume_) {
    return rados_striper_read(striper_, virtual_filename_, buffer, count,
                              offset);
  } else {
#  endif
    return rados_read(ctx_, virtual_filename_, buffer, count, offset);
#  ifdef HAVE_RADOS_STRIPER
  }
#  endif
}

// Read data from a volume using librados.
ssize_t rados_device::d_read(int fd, void* buffer, size_t count)
{
  if (ctx_) {
    size_t nr_read = 0;

    nr_read = ReadObjectData(offset_, (char*)buffer, count);
    if (nr_read >= 0) {
      offset_ += nr_read;
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
ssize_t rados_device::WriteObjectData(boffset_t offset,
                                      char* buffer,
                                      size_t count)
{
#  if LIBRADOS_VERSION_CODE <= 17408
  return = rados_write(ctx_, virtual_filename_, buffer, count, offset);
#  else
  int status;

#    ifdef HAVE_RADOS_STRIPER
  if (striper_) {
    status = rados_striper_write(striper_, virtual_filename_, buffer, count,
                                 offset);
  } else {
#    endif
    status = rados_write(ctx_, virtual_filename_, buffer, count, offset);
#    ifdef HAVE_RADOS_STRIPER
  }
#    endif

  if (status == 0) {
    return count;
  } else {
    errno = -status;
    return -1;
  }
#  endif
}

// Write data to a volume using librados.
ssize_t rados_device::d_write(int fd, const void* buffer, size_t count)
{
  if (ctx_) {
    size_t nr_written = 0;

    nr_written = WriteObjectData(offset_, (char*)buffer, count);
    if (nr_written >= 0) { offset_ += nr_written; }

    return nr_written;
  } else {
    errno = EBADF;
    return -1;
  }
}

int rados_device::d_close(int fd)
{
  // Destroy the IOcontext.
  if (ctx_) {
#  ifdef HAVE_RADOS_STRIPER
    if (striper_) {
      rados_striper_destroy(striper_);
      striper_ = NULL;
    }
#  endif
    rados_ioctx_destroy(ctx_);
    ctx_ = NULL;
  } else {
    errno = EBADF;
    return -1;
  }

  return 0;
}

int rados_device::d_ioctl(int fd, ioctl_req_t request, char* op) { return -1; }

#  ifdef HAVE_RADOS_STRIPER
ssize_t rados_device::StriperVolumeSize()
{
  uint64_t object_size;
  time_t object_mtime;

  if (rados_striper_stat(striper_, virtual_filename_, &object_size,
                         &object_mtime)
      == 0) {
    return object_size;
  } else {
    return -1;
  }
}
#  endif

ssize_t rados_device::VolumeSize()
{
  uint64_t object_size;
  time_t object_mtime;

  if (rados_stat(ctx_, virtual_filename_, &object_size, &object_mtime) == 0) {
    return object_size;
  } else {
    return -1;
  }
}

boffset_t rados_device::d_lseek(DeviceControlRecord* dcr,
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
      ssize_t filesize;

#  ifdef HAVE_RADOS_STRIPER
      if (stripe_volume_) {
        filesize = StriperVolumeSize();
      } else {
        filesize = VolumeSize();
      }
#  else
      filesize = VolumeSize();
#  endif

      if (filesize >= 0) {
        offset_ = filesize + offset;
      } else {
        return -1;
      }
      break;
    }
    default:
      return -1;
  }

  return offset_;
}

#  ifdef HAVE_RADOS_STRIPER
bool rados_device::TruncateStriperVolume(DeviceControlRecord* dcr)
{
  int status;
  uint64_t object_size;
  time_t object_mtime;
  BErrNo be;

  status = rados_striper_trunc(striper_, virtual_filename_, 0);
  if (status < 0) {
    Mmsg2(errmsg, _("Unable to truncate device %s. ERR=%s\n"), prt_name,
          be.bstrerror(-status));
    Emsg0(M_FATAL, 0, errmsg);
    return false;
  }

  status = rados_striper_stat(striper_, virtual_filename_, &object_size,
                              &object_mtime);
  if (status < 0) {
    Mmsg2(errmsg, _("Unable to stat volume %s. ERR=%s\n"), virtual_filename_,
          be.bstrerror(-status));
    Dmsg1(100, "%s", errmsg);
    return false;
  }

  if (object_size != 0) { /* rados_trunc() didn't work. */
    status = rados_striper_remove(striper_, virtual_filename_);
    if (status < 0) {
      Mmsg2(errmsg, _("Unable to remove volume %s. ERR=%s\n"),
            virtual_filename_, be.bstrerror(-status));
      Dmsg1(100, "%s", errmsg);
      return false;
    }
  }

  offset_ = 0;
  return true;
}
#  endif

bool rados_device::TruncateVolume(DeviceControlRecord* dcr)
{
  int status;
  uint64_t object_size;
  time_t object_mtime;
  BErrNo be;

  status = rados_trunc(ctx_, virtual_filename_, 0);
  if (status < 0) {
    Mmsg2(errmsg, _("Unable to truncate device %s. ERR=%s\n"), prt_name,
          be.bstrerror(-status));
    Emsg0(M_FATAL, 0, errmsg);
    return false;
  }

  status = rados_stat(ctx_, virtual_filename_, &object_size, &object_mtime);
  if (status < 0) {
    Mmsg2(errmsg, _("Unable to stat volume %s. ERR=%s\n"), virtual_filename_,
          be.bstrerror(-status));
    Dmsg1(100, "%s", errmsg);
    return false;
  }

  if (object_size != 0) { /* rados_trunc() didn't work. */
    status = rados_remove(ctx_, virtual_filename_);
    if (status < 0) {
      Mmsg2(errmsg, _("Unable to remove volume %s. ERR=%s\n"),
            virtual_filename_, be.bstrerror(-status));
      Dmsg1(100, "%s", errmsg);
      return false;
    }
  }

  offset_ = 0;
  return true;
}

bool rados_device::d_truncate(DeviceControlRecord* dcr)
{
  if (ctx_) {
#  ifdef HAVE_RADOS_STRIPER
    if (stripe_volume_) {
      return TruncateStriperVolume(dcr);
    } else {
      return TruncateVolume(dcr);
    }
#  else
    return TruncateVolume(dcr);
#  endif
  }

  return true;
}

rados_device::~rados_device()
{
  if (ctx_) {
    rados_ioctx_destroy(ctx_);
    ctx_ = NULL;
  }

  if (cluster_initialized_) {
    rados_shutdown(cluster_);
    cluster_initialized_ = false;
  }

#  if LIBRADOS_VERSION_CODE < 17408
  if (rados_clientid_) { free(rados_clientid_); }
#  else
  if (rados_clustername_) { free(rados_clustername_); }
  if (rados_username_) { free(rados_username_); }
#  endif

  if (rados_configstring_) { free(rados_configstring_); }

  FreePoolMemory(virtual_filename_);
  close(nullptr);
}

rados_device::rados_device()
{
  rados_configstring_ = NULL;
  rados_conffile_ = NULL;
  rados_poolname_ = NULL;
#  if LIBRADOS_VERSION_CODE < 17408
  rados_clientid_ = NULL;
#  else
  rados_clustername_ = NULL;
  rados_username_ = NULL;
#  endif
  cluster_initialized_ = false;
  ctx_ = NULL;
#  ifdef HAVE_RADOS_STRIPER
  stripe_volume_ = false;
  stripe_unit_ = 4194304;
  stripe_count_ = 1;
  object_size_ = 4194304;
  striper_ = NULL;
#  endif
  virtual_filename_ = GetPoolMemory(PM_FNAME);
}

class Backend : public BackendInterface {
 public:
  Device* GetDevice(JobControlRecord* jcr, DeviceType device_type) override
  {
    switch (device_type) {
      case DeviceType::B_RADOS_DEV:
        return new rados_device;
      default:
        Jmsg(jcr, M_FATAL, 0, _("Request for unknown devicetype: %d\n"),
             device_type);
        return nullptr;
    }
  }
  void FlushDevice(void) override {}
};

#  ifdef HAVE_DYNAMIC_SD_BACKENDS
extern "C" BackendInterface* GetBackend(void) { return new Backend; }
#  endif

} /* namespace storagedaemon */
#endif /* HAVE_RADOS */
