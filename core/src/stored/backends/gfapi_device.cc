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
// Marco van Wieringen, February 2014
/**
 * @file
 * Gluster Filesystem API device abstraction.
 */

#include "include/bareos.h"

#ifdef HAVE_GFAPI
#  include "stored/stored.h"
#  include "stored/sd_backends.h"
#  include "stored/backends/gfapi_device.h"
#  include "lib/edit.h"
#  include "lib/berrno.h"

namespace storagedaemon {

// Options that can be specified for this device type.
enum device_option_type
{
  argument_none = 0,
  argument_uri,
  argument_logfile,
  argument_loglevel
};

struct device_option {
  const char* name;
  enum device_option_type type;
  int compare_size;
};

static device_option device_options[] = {{"uri=", argument_uri, 4},
                                         {"logfile=", argument_logfile, 8},
                                         {"loglevel=", argument_loglevel, 9},
                                         {NULL, argument_none}};

/**
 * Parse a gluster definition into something we can use for setting
 * up the right connection to a gluster management server and get access
 * to a gluster volume.
 *
 * Syntax:
 *
 * gluster[+transport]://[server[:port]]/volname[/dir][?socket=...]
 *
 * 'gluster' is the protocol.
 *
 * 'transport' specifies the transport type used to connect to gluster
 * management daemon (glusterd). Valid transport types are tcp, unix
 * and rdma. If a transport type isn't specified, then tcp type is assumed.
 *
 * 'server' specifies the server where the volume file specification for
 * the given volume resides. This can be either hostname, ipv4 address
 * or ipv6 address. ipv6 address needs to be within square brackets [ ].
 * If transport type is 'unix', then 'server' field should not be specifed.
 * The 'socket' field needs to be populated with the path to unix domain
 * socket.
 *
 * 'port' is the port number on which glusterd is listening. This is optional
 * and if not specified, QEMU will send 0 which will make gluster to use the
 * default port. If the transport type is unix, then 'port' should not be
 * specified.
 *
 * 'volname' is the name of the gluster volume which contains the backup images.
 *
 * 'dir' is an optional directory on the 'volname'
 *
 * Examples:
 *
 * gluster://1.2.3.4/testvol[/dir]
 * gluster+tcp://1.2.3.4/testvol[/dir]
 * gluster+tcp://1.2.3.4:24007/testvol[/dir]
 * gluster+tcp://[1:2:3:4:5:6:7:8]/testvol[/dir]
 * gluster+tcp://[1:2:3:4:5:6:7:8]:24007/testvol[/dir]
 * gluster+tcp://server.domain.com:24007/testvol[/dir]
 * gluster+unix:///testvol[/dir]?socket=/tmp/glusterd.socket
 * gluster+rdma://1.2.3.4:24007/testvol[/dir]
 */
static inline bool parse_gfapi_devicename(char* devicename,
                                          char** transport,
                                          char** servername,
                                          char** volumename,
                                          char** dir,
                                          int* serverport)
{
  char* bp;

  // Make sure its a URI that starts with gluster.
  if (!bstrncasecmp(devicename, "gluster", 7)) { return false; }

  // Parse any explicit protocol.
  bp = strchr(devicename, '+');
  if (bp) {
    *transport = ++bp;
    bp = strchr(bp, ':');
    if (bp) {
      *bp++ = '\0';
    } else {
      goto bail_out;
    }
  } else {
    *transport = NULL;
    bp = strchr(devicename, ':');
    if (!bp) { goto bail_out; }
  }

  // When protocol is not UNIX parse servername and portnr.
  if (!*transport || !Bstrcasecmp(*transport, "unix")) {
    // Parse servername of gluster management server.
    bp = strchr(bp, '/');

    // Validate URI.
    if (!bp || !(*bp == '/')) { goto bail_out; }

    // Skip the two //
    *bp++ = '\0';
    bp++;
    *servername = bp;

    /*
     * Parse any explicit server portnr.
     * We search reverse in the string for a : what indicates
     * a port specification but in that string there may not contain a ']'
     * because then we searching in a IPv6 string.
     */
    bp = strrchr(bp, ':');
    if (bp && !strchr(bp, ']')) {
      char* port;

      *bp++ = '\0';
      port = bp;
      bp = strchr(bp, '/');
      if (!bp) { goto bail_out; }
      *bp++ = '\0';
      *serverport = str_to_int64(port);
      *volumename = bp;

      // See if there is a dir specified.
      bp = strchr(bp, '/');
      if (bp) {
        *bp++ = '\0';
        *dir = bp;
      }
    } else {
      *serverport = 0;
      bp = *servername;

      // Parse the volume name.
      bp = strchr(bp, '/');
      if (!bp) { goto bail_out; }
      *bp++ = '\0';
      *volumename = bp;

      // See if there is a dir specified.
      bp = strchr(bp, '/');
      if (bp) {
        *bp++ = '\0';
        *dir = bp;
      }
    }
  } else {
    // For UNIX serverport is zero.
    *serverport = 0;

    // Validate URI.
    if (*bp != '/' || *(bp + 1) != '/') { goto bail_out; }

    // Skip the two //
    *bp++ = '\0';
    bp++;

    // For UNIX URIs the server part of the URI needs to be empty.
    if (*bp++ != '/') { goto bail_out; }
    *volumename = bp;

    // See if there is a dir specified.
    bp = strchr(bp, '/');
    if (bp) {
      *bp++ = '\0';
      *dir = bp;
    }

    // Parse any socket parameters.
    bp = strchr(bp, '?');
    if (bp) {
      if (bstrncasecmp(bp + 1, "socket=", 7)) {
        *bp = '\0';
        *servername = bp + 8;
      }
    }
  }

  return true;

bail_out:
  return false;
}

// Create a parent directory using the gfapi.
static inline bool GfapiMakedirs(glfs_t* glfs, const char* directory)
{
  int len;
  char* bp;
  struct stat st;
  bool retval = false;
  PoolMem new_directory(PM_FNAME);

  PmStrcpy(new_directory, directory);
  len = strlen(new_directory.c_str());

  // Strip any trailing slashes.
  for (char* p = new_directory.c_str() + (len - 1);
       (p >= new_directory.c_str()) && *p == '/'; p--) {
    *p = '\0';
  }

  if (strlen(new_directory.c_str())
      && glfs_stat(glfs, new_directory.c_str(), &st) != 0) {
    // See if the parent exists.
    switch (errno) {
      case ENOENT:
        bp = strrchr(new_directory.c_str(), '/');
        if (bp) {
          // Make sure our parent exists.
          *bp = '\0';
          retval = GfapiMakedirs(glfs, new_directory.c_str());
          if (!retval) { return false; }

          // Create the directory.
          if (glfs_mkdir(glfs, directory, 0750) == 0) { retval = true; }
        }
        break;
      default:
        break;
    }
  } else {
    retval = true;
  }

  return retval;
}

// Open a volume using gfapi.
int gfapi_device::d_open(const char* pathname, int flags, int mode)
{
  int status;

  // Parse the gluster URI.
  if (!gfapi_configstring_) {
    char *bp, *next_option;
    bool done;

    if (!dev_options) {
      Mmsg0(errmsg, _("No device options configured\n"));
      Emsg0(M_FATAL, 0, errmsg);
      goto bail_out;
    }

    gfapi_configstring_ = strdup(dev_options);

    bp = gfapi_configstring_;
    while (bp) {
      next_option = strchr(bp, ',');
      if (next_option) { *next_option++ = '\0'; }

      done = false;
      for (int i = 0; !done && device_options[i].name; i++) {
        // Try to find a matching device option.
        if (bstrncasecmp(bp, device_options[i].name,
                         device_options[i].compare_size)) {
          switch (device_options[i].type) {
            case argument_uri:
              gfapi_uri_ = bp + device_options[i].compare_size;
              break;
            case argument_logfile:
              gfapi_logfile_ = bp + device_options[i].compare_size;
              break;
            case argument_loglevel:
              gfapi_loglevel_
                  = strtol(bp + device_options[i].compare_size, NULL, 10);
              break;
            default:
              Mmsg1(errmsg, _("Unable to parse device option: %s\n"), bp);
              Emsg0(M_FATAL, 0, errmsg);
              goto bail_out;
              break;
          }
          done = true;
        }
      }
      bp = next_option;
    }

    if (!gfapi_uri_) {
      Mmsg0(errmsg, _("No GFAPI URI configured\n"));
      Emsg0(M_FATAL, 0, errmsg);
      goto bail_out;
    }

    if (!parse_gfapi_devicename(gfapi_uri_, &transport_, &servername_,
                                &volumename_, &basedir_, &serverport_)) {
      Mmsg1(errmsg, _("Unable to parse device URI %s.\n"), dev_options);
      Emsg0(M_FATAL, 0, errmsg);
      goto bail_out;
    }
  }

  // See if we need to setup a Gluster context.
  if (!glfs_) {
    glfs_ = glfs_new(volumename_);
    if (!glfs_) {
      Mmsg1(errmsg,
            _("Unable to create new Gluster context for volumename %s.\n"),
            volumename_);
      Emsg0(M_FATAL, 0, errmsg);
      goto bail_out;
    }

    if (gfapi_logfile_ != nullptr) {
      if (glfs_set_logging(glfs_, gfapi_logfile_, gfapi_loglevel_) < 0) {
        Mmsg3(errmsg,
              _("Unable to initialize Gluster logging file=%s level=%d\n"),
              gfapi_logfile_, gfapi_loglevel_);
        Emsg0(M_FATAL, 0, errmsg);
        goto bail_out;
      }
    }

    status = glfs_set_volfile_server(glfs_, (transport_) ? transport_ : "tcp",
                                     servername_, serverport_);
    if (status < 0) {
      Mmsg3(errmsg,
            _("Unable to initialize Gluster management server for transport "
              "%s, servername %s, serverport %d\n"),
            (transport_) ? transport_ : "tcp", servername_, serverport_);
      Emsg0(M_FATAL, 0, errmsg);
      goto bail_out;
    }

    status = glfs_init(glfs_);
    if (status < 0) {
      Mmsg1(errmsg, _("Unable to initialize Gluster for volumename %s.\n"),
            volumename_);
      Emsg0(M_FATAL, 0, errmsg);
      goto bail_out;
    }
  }

  // See if we don't have a file open already.
  if (gfd_) {
    glfs_close(gfd_);
    gfd_ = NULL;
  }

  // See if we store in an explicit directory.
  if (basedir_) {
    struct stat st;

    // Make sure the dir exists if one is defined.
    Mmsg(virtual_filename_, "/%s", basedir_);
    if (glfs_stat(glfs_, virtual_filename_, &st) != 0) {
      switch (errno) {
        case ENOENT:
          if (!GfapiMakedirs(glfs_, virtual_filename_)) {
            Mmsg1(errmsg,
                  _("Specified glusterfs directory %s cannot be created.\n"),
                  virtual_filename_);
            Emsg0(M_FATAL, 0, errmsg);
            goto bail_out;
          }
          break;
        default:
          goto bail_out;
      }
    } else {
      if (!S_ISDIR(st.st_mode)) {
        Mmsg1(errmsg,
              _("Specified glusterfs directory %s is not a directory.\n"),
              virtual_filename_);
        Emsg0(M_FATAL, 0, errmsg);
        goto bail_out;
      }
    }

    Mmsg(virtual_filename_, "/%s/%s", basedir_, getVolCatName());
  } else {
    Mmsg(virtual_filename_, "%s", getVolCatName());
  }

  /*
   * See if the O_CREAT flag is set as glfs_open doesn't support that flag and
   * you have to call glfs_creat then.
   */
  if (flags & O_CREAT) {
    gfd_ = glfs_creat(glfs_, virtual_filename_, flags, mode);
  } else {
    gfd_ = glfs_open(glfs_, virtual_filename_, flags);
  }

  if (!gfd_) { goto bail_out; }

  return 0;

bail_out:
  // Cleanup the Gluster context.
  if (glfs_) {
    glfs_fini(glfs_);
    glfs_ = NULL;
  }

  return -1;
}

// Read data from a volume using gfapi.
ssize_t gfapi_device::d_read(int fd, void* buffer, size_t count)
{
  if (gfd_) {
    return glfs_read(gfd_, buffer, count, 0);
  } else {
    errno = EBADF;
    return -1;
  }
}

// Write data to a volume using gfapi.
ssize_t gfapi_device::d_write(int fd, const void* buffer, size_t count)
{
  if (gfd_) {
    return glfs_write(gfd_, buffer, count, 0);
  } else {
    errno = EBADF;
    return -1;
  }
}

int gfapi_device::d_close(int fd)
{
  if (gfd_) {
    int status;

    status = glfs_close(gfd_);
    gfd_ = NULL;
    return status;
  } else {
    errno = EBADF;
    return -1;
  }
}

int gfapi_device::d_ioctl(int fd, ioctl_req_t request, char* op) { return -1; }

boffset_t gfapi_device::d_lseek(DeviceControlRecord* dcr,
                                boffset_t offset,
                                int whence)
{
  if (gfd_) {
    return glfs_lseek(gfd_, offset, whence);
  } else {
    errno = EBADF;
    return -1;
  }
}

bool gfapi_device::d_truncate(DeviceControlRecord* dcr)
{
  struct stat st;

  if (gfd_) {
    if (glfs_ftruncate(gfd_, 0) != 0) {
      BErrNo be;

      Mmsg2(errmsg, _("Unable to truncate device %s. ERR=%s\n"), prt_name,
            be.bstrerror());
      Emsg0(M_FATAL, 0, errmsg);
      return false;
    }

    /*
     * Check for a successful glfs_truncate() and issue work-around when
     * truncation doesn't work.
     *
     * 1. close file
     * 2. delete file
     * 3. open new file with same mode
     * 4. change ownership to original
     */
    if (glfs_fstat(gfd_, &st) != 0) {
      BErrNo be;

      Mmsg2(errmsg, _("Unable to stat device %s. ERR=%s\n"), prt_name,
            be.bstrerror());
      Dmsg1(100, "%s", errmsg);
      return false;
    }

    if (st.st_size != 0) { /* glfs_truncate() didn't work */
      glfs_close(gfd_);
      glfs_unlink(glfs_, virtual_filename_);

      // Recreate the file -- of course, empty
      oflags = O_CREAT | O_RDWR | O_BINARY;
      gfd_ = glfs_creat(glfs_, virtual_filename_, oflags, st.st_mode);
      if (!gfd_) {
        BErrNo be;

        dev_errno = errno;
        Mmsg2(errmsg, _("Could not reopen: %s, ERR=%s\n"), virtual_filename_,
              be.bstrerror());
        Emsg0(M_FATAL, 0, errmsg);

        return false;
      }

      // Reset proper owner
      glfs_chown(glfs_, virtual_filename_, st.st_uid, st.st_gid);
    }
  }

  return true;
}

gfapi_device::~gfapi_device()
{
  if (gfd_) {
    glfs_close(gfd_);
    gfd_ = NULL;
  }

  if (!glfs_) {
    glfs_fini(glfs_);
    glfs_ = NULL;
  }

  if (gfapi_configstring_) {
    free(gfapi_configstring_);
    gfapi_configstring_ = NULL;
  }

  FreePoolMemory(virtual_filename_);
  close(nullptr);
}

gfapi_device::gfapi_device()
{
  gfapi_configstring_ = NULL;
  transport_ = NULL;
  servername_ = NULL;
  volumename_ = NULL;
  basedir_ = NULL;
  serverport_ = 0;
  glfs_ = NULL;
  gfd_ = NULL;
  virtual_filename_ = GetPoolMemory(PM_FNAME);
}

class Backend : public BackendInterface {
 public:
  Device* GetDevice(JobControlRecord* jcr, DeviceType device_type) override
  {
    switch (device_type) {
      case DeviceType::B_GFAPI_DEV:
        return new gfapi_device;
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
#endif /* HAVE_GFAPI */
