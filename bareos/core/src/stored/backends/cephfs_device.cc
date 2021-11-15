/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2014-2014 Planets Communications B.V.
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
// Marco van Wieringen, May 2014
/**
 * @file
 * Gluster Filesystem API device abstraction.
 */

#include "include/bareos.h"

#ifdef HAVE_CEPHFS
#  include "stored/stored.h"
#  include "stored/sd_backends.h"
#  include "stored/backends/cephfs_device.h"
#  include "lib/berrno.h"

namespace storagedaemon {

// Options that can be specified for this device type.
enum device_option_type
{
  argument_none = 0,
  argument_conffile,
  argument_basedir
};

struct device_option {
  const char* name;
  enum device_option_type type;
  int compare_size;
};

static device_option device_options[] = {{"conffile=", argument_conffile, 9},
                                         {"basedir=", argument_basedir, 9},
                                         {NULL, argument_none}};

// Open a volume using libcephfs.
int cephfs_device::d_open(const char* pathname, int flags, int mode)
{
  int status;
  BErrNo be;

  if (!cephfs_configstring_) {
    char *bp, *next_option;
    bool done;

    if (!dev_options) {
      Mmsg0(errmsg, _("No device options configured\n"));
      Emsg0(M_FATAL, 0, errmsg);
      goto bail_out;
    }

    cephfs_configstring_ = strdup(dev_options);

    bp = cephfs_configstring_;
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
              cephfs_conffile_ = bp + device_options[i].compare_size;
              done = true;
              break;
            case argument_basedir:
              basedir_ = bp + device_options[i].compare_size;
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
  }

  if (!cephfs_conffile_) {
    Mmsg0(errmsg, _("No cephfs config file configured\n"));
    Emsg0(M_FATAL, 0, errmsg);
    goto bail_out;
  }


  if (!cmount_) {
    status = ceph_create(&cmount_, NULL);
    if (status < 0) {
      Mmsg1(errmsg, _("Unable to create CEPHFS mount: ERR=%s\n"),
            be.bstrerror(-status));
      Emsg0(M_FATAL, 0, errmsg);
      goto bail_out;
    }

    status = ceph_conf_read_file(cmount_, cephfs_conffile_);
    if (status < 0) {
      Mmsg2(errmsg, _("Unable to read CEPHFS config %s: ERR=%s\n"),
            cephfs_conffile_, be.bstrerror(-status));
      Emsg0(M_FATAL, 0, errmsg);
      goto bail_out;
    }

    status = ceph_mount(cmount_, NULL);
    if (status < 0) {
      Mmsg1(errmsg, _("Unable to mount CEPHFS: ERR=%s\n"),
            be.bstrerror(-status));
      Emsg0(M_FATAL, 0, errmsg);
      goto bail_out;
    }
  }

  // See if we don't have a file open already.
  if (fd >= 0) {
    ceph_close(cmount_, fd);
    fd = -1;
  }

  // See if we store in an explicit directory.
  if (basedir_) {
#  if HAVE_CEPH_STATX
    struct ceph_statx stx;
#  else
    struct stat st;
#  endif

    // Make sure the dir exists if one is defined.
#  if HAVE_CEPH_STATX
    status = ceph_statx(cmount_, basedir_, &stx, CEPH_STATX_SIZE,
                        AT_SYMLINK_NOFOLLOW);
#  else
    status = ceph_stat(cmount_, basedir_, &st);
#  endif
    if (status < 0) {
      switch (status) {
        case -ENOENT:
          status = ceph_mkdirs(cmount_, basedir_, 0750);
          if (status < 0) {
            Mmsg1(errmsg,
                  _("Specified CEPHFS directory %s cannot be created.\n"),
                  basedir_);
            Emsg0(M_FATAL, 0, errmsg);
            goto bail_out;
          }
          break;
        default:
          goto bail_out;
      }
    } else {
#  if HAVE_CEPH_STATX
      if (!S_ISDIR(stx.stx_mode)) {
#  else
      if (!S_ISDIR(st.st_mode)) {
#  endif
        Mmsg1(errmsg, _("Specified CEPHFS directory %s is not a directory.\n"),
              basedir_);
        Emsg0(M_FATAL, 0, errmsg);
        goto bail_out;
      }
    }

    Mmsg(virtual_filename_, "%s/%s", basedir_, getVolCatName());
  } else {
    Mmsg(virtual_filename_, "%s", getVolCatName());
  }

  fd = ceph_open(cmount_, virtual_filename_, flags, mode);
  if (fd < 0) { goto bail_out; }

  return 0;

bail_out:
  // Cleanup the CEPHFS context.
  if (cmount_) {
    ceph_shutdown(cmount_);
    cmount_ = NULL;
  }
  fd = -1;

  return -1;
}

// Read data from a volume using libcephfs.
ssize_t cephfs_device::d_read(int fd, void* buffer, size_t count)
{
  if (fd >= 0) {
    return ceph_read(cmount_, fd, (char*)buffer, count, -1);
  } else {
    errno = EBADF;
    return -1;
  }
}

// Write data to a volume using libcephfs.
ssize_t cephfs_device::d_write(int fd, const void* buffer, size_t count)
{
  if (fd >= 0) {
    return ceph_write(cmount_, fd, (char*)buffer, count, -1);
  } else {
    errno = EBADF;
    return -1;
  }
}

int cephfs_device::d_close(int fd)
{
  if (fd >= 0) {
    int status;

    status = ceph_close(cmount_, fd);
    fd = -1;
    return (status < 0) ? -1 : 0;
  } else {
    errno = EBADF;
    return -1;
  }
}

int cephfs_device::d_ioctl(int fd, ioctl_req_t request, char* op) { return -1; }

boffset_t cephfs_device::d_lseek(DeviceControlRecord* dcr,
                                 boffset_t offset,
                                 int whence)
{
  if (fd >= 0) {
    boffset_t status;

    status = ceph_lseek(cmount_, fd, offset, whence);
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

bool cephfs_device::d_truncate(DeviceControlRecord* dcr)
{
  int status;
#  if HAVE_CEPH_STATX
  struct ceph_statx stx;
#  else
  struct stat st;
#  endif

  if (fd >= 0) {
    status = ceph_ftruncate(cmount_, fd, 0);
    if (status < 0) {
      BErrNo be;

      Mmsg2(errmsg, _("Unable to truncate device %s. ERR=%s\n"), prt_name,
            be.bstrerror(-status));
      Emsg0(M_FATAL, 0, errmsg);
      return false;
    }

    /*
     * Check for a successful ceph_ftruncate() and issue work-around when
     * truncation doesn't work.
     *
     * 1. close file
     * 2. delete file
     * 3. open new file with same mode
     * 4. change ownership to original
     */
#  if HAVE_CEPH_STATX
    status = ceph_fstatx(cmount_, fd, &stx, CEPH_STATX_MODE, 0);
#  else
    status = ceph_fstat(cmount_, fd, &st);
#  endif
    if (status < 0) {
      BErrNo be;

#  if HAVE_CEPH_STATX
      Mmsg2(errmsg, _("Unable to ceph_statx device %s. ERR=%s\n"), prt_name,
            be.bstrerror(-status));
#  else
      Mmsg2(errmsg, _("Unable to stat device %s. ERR=%s\n"), prt_name,
            be.bstrerror(-status));
#  endif
      Dmsg1(100, "%s", errmsg);
      return false;
    }

#  if HAVE_CEPH_STATX
    if (stx.stx_size != 0) { /* ceph_ftruncate() didn't work */
#  else
    if (st.st_size != 0) { /* ceph_ftruncate() didn't work */
#  endif
      ceph_close(cmount_, fd);
      ceph_unlink(cmount_, virtual_filename_);

      // Recreate the file -- of course, empty
      oflags = O_CREAT | O_RDWR | O_BINARY;
#  if HAVE_CEPH_STATX
      fd = ceph_open(cmount_, virtual_filename_, oflags, stx.stx_mode);
#  else
      fd = ceph_open(cmount_, virtual_filename_, oflags, st.st_mode);
#  endif
      if (fd < 0) {
        BErrNo be;

        dev_errno = -fd;
        ;
        Mmsg2(errmsg, _("Could not reopen: %s, ERR=%s\n"), virtual_filename_,
              be.bstrerror(-fd));
        Emsg0(M_FATAL, 0, errmsg);
        fd = -1;

        return false;
      }

      // Reset proper owner
#  if HAVE_CEPH_STATX
      ceph_chown(cmount_, virtual_filename_, stx.stx_uid, stx.stx_gid);
#  else
      ceph_chown(cmount_, virtual_filename_, st.st_uid, st.st_gid);
#  endif
    }
  }

  return true;
}

cephfs_device::~cephfs_device()
{
  if (cmount_ && fd >= 0) {
    ceph_close(cmount_, fd);
    fd = -1;
  }

  if (!cmount_) {
    ceph_shutdown(cmount_);
    cmount_ = NULL;
  }

  if (cephfs_configstring_) {
    free(cephfs_configstring_);
    cephfs_configstring_ = NULL;
  }

  FreePoolMemory(virtual_filename_);
  close(nullptr);
}

cephfs_device::cephfs_device()
{
  cephfs_configstring_ = NULL;
  cephfs_conffile_ = NULL;
  basedir_ = NULL;
  cmount_ = NULL;
  virtual_filename_ = GetPoolMemory(PM_FNAME);
}

class Backend : public BackendInterface {
 public:
  Device* GetDevice(JobControlRecord* jcr, DeviceType device_type) override
  {
    switch (device_type) {
      case DeviceType::B_CEPHFS_DEV:
        return new cephfs_device;
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

#endif /* HAVE_CEPHFS */
