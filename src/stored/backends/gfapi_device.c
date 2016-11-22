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
 * Gluster Filesystem API device abstraction.
 *
 * Marco van Wieringen, February 2014
 */

#include "bareos.h"

#ifdef HAVE_GFAPI
#include "stored.h"
#include "backends/gfapi_device.h"

/*
 * Options that can be specified for this device type.
 */
enum device_option_type {
   argument_none = 0,
   argument_uri
};

struct device_option {
   const char *name;
   enum device_option_type type;
   int compare_size;
};

static device_option device_options[] = {
   { "uri=", argument_uri, 4 },
   { NULL, argument_none }
};

/*
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
static inline bool parse_gfapi_devicename(char *devicename,
                                          char **transport,
                                          char **servername,
                                          char **volumename,
                                          char **dir,
                                          int *serverport)
{
   char *bp;

   /*
    * Make sure its a URI that starts with gluster.
    */
   if (!bstrncasecmp(devicename, "gluster", 7)) {
      return false;
   }

   /*
    * Parse any explicit protocol.
    */
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
      if (!bp) {
         goto bail_out;
      }
   }

   /*
    * When protocol is not UNIX parse servername and portnr.
    */
   if (!*transport || !bstrcasecmp(*transport, "unix")) {
      /*
       * Parse servername of gluster management server.
       */
      bp = strchr(bp, '/');

      /*
       * Validate URI.
       */
      if (!bp || !(*bp == '/')) {
         goto bail_out;
      }

      /*
       * Skip the two //
       */
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
         char *port;

         *bp++ = '\0';
         port = bp;
         bp = strchr(bp, '/');
         if (!bp) {
            goto bail_out;
         }
         *bp++ = '\0';
         *serverport = str_to_int64(port);
         *volumename = bp;

         /*
          * See if there is a dir specified.
          */
         bp = strchr(bp, '/');
         if (bp) {
            *bp++ = '\0';
            *dir = bp;
         }
      } else {
         *serverport = 0;
         bp = *servername;

         /*
          * Parse the volume name.
          */
         bp = strchr(bp, '/');
         if (!bp) {
            goto bail_out;
         }
         *bp++ = '\0';
         *volumename = bp;

         /*
          * See if there is a dir specified.
          */
         bp = strchr(bp, '/');
         if (bp) {
            *bp++ = '\0';
            *dir = bp;
         }
      }
   } else {
      /*
       * For UNIX serverport is zero.
       */
      *serverport = 0;

      /*
       * Validate URI.
       */
      if (*bp != '/' || *(bp + 1) != '/') {
         goto bail_out;
      }

      /*
       * Skip the two //
       */
      *bp++ = '\0';
      bp++;

      /*
       * For UNIX URIs the server part of the URI needs to be empty.
       */
      if (*bp++ != '/') {
         goto bail_out;
      }
      *volumename = bp;

      /*
       * See if there is a dir specified.
       */
      bp = strchr(bp, '/');
      if (bp) {
         *bp++ = '\0';
         *dir = bp;
      }

      /*
       * Parse any socket parameters.
       */
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

/*
 * Create a parent directory using the gfapi.
 */
static inline bool gfapi_makedirs(glfs_t *glfs, const char *directory)
{
   int len;
   char *bp;
   struct stat st;
   bool retval = false;
   POOL_MEM new_directory(PM_FNAME);

   pm_strcpy(new_directory, directory);
   len = strlen(new_directory.c_str());

   /*
    * Strip any trailing slashes.
    */
   for (char *p = new_directory.c_str() + (len - 1);
        (p >= new_directory.c_str()) && *p == '/';
        p--) {
      *p = '\0';
   }

   if (strlen(new_directory.c_str()) &&
       glfs_stat(glfs, new_directory.c_str(), &st) != 0) {
      /*
       * See if the parent exists.
       */
      switch (errno) {
         case ENOENT:
            bp = strrchr(new_directory.c_str(), '/');
            if (bp) {
               /*
                * Make sure our parent exists.
                */
               *bp = '\0';
               retval = gfapi_makedirs(glfs, new_directory.c_str());
               if (!retval) {
                  return false;
               }

               /*
                * Create the directory.
                */
               if (glfs_mkdir(glfs, directory, 0750) == 0) {
                  retval = true;
               }
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

/*
 * Open a volume using gfapi.
 */
int gfapi_device::d_open(const char *pathname, int flags, int mode)
{
   int status;

   /*
    * Parse the gluster URI.
    */
   if (!m_gfapi_configstring) {
      char *bp, *next_option;
      bool done;

      if (!dev_options) {
         Mmsg0(errmsg, _("No device options configured\n"));
         Emsg0(M_FATAL, 0, errmsg);
         goto bail_out;
      }

      m_gfapi_configstring = bstrdup(dev_options);

      bp = m_gfapi_configstring;
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
               case argument_uri:
                  m_gfapi_uri = bp + device_options[i].compare_size;
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

      if (!m_gfapi_uri) {
         Mmsg0(errmsg, _("No GFAPI URI configured\n"));
         Emsg0(M_FATAL, 0, errmsg);
         goto bail_out;
      }

      if (!parse_gfapi_devicename(m_gfapi_uri,
                                  &m_transport,
                                  &m_servername,
                                  &m_volumename,
                                  &m_basedir,
                                  &m_serverport)) {
         Mmsg1(errmsg, _("Unable to parse device URI %s.\n"), dev_options);
         Emsg0(M_FATAL, 0, errmsg);
         goto bail_out;
      }
   }

   /*
    * See if we need to setup a Gluster context.
    */
   if (!m_glfs) {
      m_glfs = glfs_new(m_volumename);
      if (!m_glfs) {
         Mmsg1(errmsg, _("Unable to create new Gluster context for volumename %s.\n"), m_volumename);
         Emsg0(M_FATAL, 0, errmsg);
         goto bail_out;
      }

      status = glfs_set_volfile_server(m_glfs, (m_transport) ? m_transport : "tcp", m_servername, m_serverport);
      if (status < 0) {
         Mmsg3(errmsg, _("Unable to initialize Gluster management server for transport %s, servername %s, serverport %d\n"),
               (m_transport) ? m_transport : "tcp", m_servername, m_serverport);
         Emsg0(M_FATAL, 0, errmsg);
         goto bail_out;
      }

      status = glfs_init(m_glfs);
      if (status < 0) {
         Mmsg1(errmsg, _("Unable to initialize Gluster for volumename %s.\n"), m_volumename);
         Emsg0(M_FATAL, 0, errmsg);
         goto bail_out;
      }
   }

   /*
    * See if we don't have a file open already.
    */
   if (m_gfd) {
      glfs_close(m_gfd);
      m_gfd = NULL;
   }

   /*
    * See if we store in an explicit directory.
    */
   if (m_basedir) {
      struct stat st;

      /*
       * Make sure the dir exists if one is defined.
       */
      Mmsg(m_virtual_filename, "/%s", m_basedir);
      if (glfs_stat(m_glfs, m_virtual_filename, &st) != 0) {
         switch (errno) {
         case ENOENT:
            if (!gfapi_makedirs(m_glfs, m_virtual_filename)) {
               Mmsg1(errmsg, _("Specified glusterfs directory %s cannot be created.\n"), m_virtual_filename);
               Emsg0(M_FATAL, 0, errmsg);
               goto bail_out;
            }
            break;
         default:
            goto bail_out;
         }
      } else {
         if (!S_ISDIR(st.st_mode)) {
            Mmsg1(errmsg, _("Specified glusterfs directory %s is not a directory.\n"), m_virtual_filename);
            Emsg0(M_FATAL, 0, errmsg);
            goto bail_out;
         }
      }

      Mmsg(m_virtual_filename, "/%s/%s", m_basedir, getVolCatName());
   } else {
      Mmsg(m_virtual_filename, "%s", getVolCatName());
   }

   /*
    * See if the O_CREAT flag is set as glfs_open doesn't support that flag and you have to call glfs_creat then.
    */
   if (flags & O_CREAT) {
      m_gfd = glfs_creat(m_glfs, m_virtual_filename, flags, mode);
   } else {
      m_gfd = glfs_open(m_glfs, m_virtual_filename, flags);
   }

   if (!m_gfd) {
      goto bail_out;
   }

   return 0;

bail_out:
   /*
    * Cleanup the Gluster context.
    */
   if (m_glfs) {
      glfs_fini(m_glfs);
      m_glfs = NULL;
   }

   return -1;
}

/*
 * Read data from a volume using gfapi.
 */
ssize_t gfapi_device::d_read(int fd, void *buffer, size_t count)
{
   if (m_gfd) {
      return glfs_read(m_gfd, buffer, count, 0);
   } else {
      errno = EBADF;
      return -1;
   }
}

/*
 * Write data to a volume using gfapi.
 */
ssize_t gfapi_device::d_write(int fd, const void *buffer, size_t count)
{
   if (m_gfd) {
      return glfs_write(m_gfd, buffer, count, 0);
   } else {
      errno = EBADF;
      return -1;
   }
}

int gfapi_device::d_close(int fd)
{
   if (m_gfd) {
      int status;

      status = glfs_close(m_gfd);
      m_gfd = NULL;
      return status;
   } else {
      errno = EBADF;
      return -1;
   }
}

int gfapi_device::d_ioctl(int fd, ioctl_req_t request, char *op)
{
   return -1;
}

boffset_t gfapi_device::d_lseek(DCR *dcr, boffset_t offset, int whence)
{
   if (m_gfd) {
      return glfs_lseek(m_gfd, offset, whence);
   } else {
      errno = EBADF;
      return -1;
   }
}

bool gfapi_device::d_truncate(DCR *dcr)
{
   struct stat st;

   if (m_gfd) {
      if (glfs_ftruncate(m_gfd, 0) != 0) {
         berrno be;

         Mmsg2(errmsg, _("Unable to truncate device %s. ERR=%s\n"), prt_name, be.bstrerror());
         Emsg0(M_FATAL, 0, errmsg);
         return false;
      }

      /*
       * Check for a successful glfs_truncate() and issue work-around when truncation doesn't work.
       *
       * 1. close file
       * 2. delete file
       * 3. open new file with same mode
       * 4. change ownership to original
       */
      if (glfs_fstat(m_gfd, &st) != 0) {
         berrno be;

         Mmsg2(errmsg, _("Unable to stat device %s. ERR=%s\n"), prt_name, be.bstrerror());
         Dmsg1(100, "%s", errmsg);
         return false;
      }

      if (st.st_size != 0) {             /* glfs_truncate() didn't work */
         glfs_close(m_gfd);
         glfs_unlink(m_glfs, m_virtual_filename);

         /*
          * Recreate the file -- of course, empty
          */
         oflags = O_CREAT | O_RDWR | O_BINARY;
         m_gfd = glfs_creat(m_glfs, m_virtual_filename, oflags, st.st_mode);
         if (!m_gfd) {
            berrno be;

            dev_errno = errno;
            Mmsg2(errmsg, _("Could not reopen: %s, ERR=%s\n"), m_virtual_filename, be.bstrerror());
            Emsg0(M_FATAL, 0, errmsg);

            return false;
         }

         /*
          * Reset proper owner
          */
         glfs_chown(m_glfs, m_virtual_filename, st.st_uid, st.st_gid);
      }
   }

   return true;
}

gfapi_device::~gfapi_device()
{
   if (m_gfd) {
      glfs_close(m_gfd);
      m_gfd = NULL;
   }

   if (!m_glfs) {
      glfs_fini(m_glfs);
      m_glfs = NULL;
   }

   if (m_gfapi_configstring) {
      free(m_gfapi_configstring);
      m_gfapi_configstring = NULL;
   }

   free_pool_memory(m_virtual_filename);
}

gfapi_device::gfapi_device()
{
   m_gfapi_configstring = NULL;
   m_transport = NULL;
   m_servername = NULL;
   m_volumename = NULL;
   m_basedir = NULL;
   m_serverport = 0;
   m_glfs = NULL;
   m_gfd = NULL;
   m_virtual_filename = get_pool_memory(PM_FNAME);
}

#ifdef HAVE_DYNAMIC_SD_BACKENDS
extern "C" DEVICE SD_IMP_EXP *backend_instantiate(JCR *jcr, int device_type)
{
   DEVICE *dev = NULL;

   switch (device_type) {
   case B_GFAPI_DEV:
      dev = New(gfapi_device);
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
#endif /* HAVE_GFAPI */
