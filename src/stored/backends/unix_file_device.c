/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2013-2013 Bareos GmbH & Co. KG

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
 * UNIX File API device abstraction.
 *
 * Marco van Wieringen, December 2013
 */

#include "bareos.h"
#include "stored.h"
#include "unix_file_device.h"

int unix_file_device::d_open(const char *pathname, int flags, int mode)
{
   return ::open(pathname, flags, mode);
}

ssize_t unix_file_device::d_read(int fd, void *buffer, size_t count)
{
   return ::read(fd, buffer, count);
}

ssize_t unix_file_device::d_write(int fd, const void *buffer, size_t count)
{
   return ::write(fd, buffer, count);
}

int unix_file_device::d_close(int fd)
{
   return ::close(fd);
}

int unix_file_device::d_ioctl(int fd, ioctl_req_t request, char *op)
{
   return -1;
}

boffset_t unix_file_device::d_lseek(DCR *dcr, boffset_t offset, int whence)
{
   return ::lseek(m_fd, offset, whence);
}

bool unix_file_device::d_truncate(DCR *dcr)
{
  struct stat st;

  if (ftruncate(m_fd, 0) != 0) {
     berrno be;

     Mmsg2(errmsg, _("Unable to truncate device %s. ERR=%s\n"), print_name(), be.bstrerror());
     return false;
  }

  /*
   * Check for a successful ftruncate() and issue a work-around for devices
   * (mostly cheap NAS) that don't support truncation.
   * Workaround supplied by Martin Schmid as a solution to bug #1011.
   * 1. close file
   * 2. delete file
   * 3. open new file with same mode
   * 4. change ownership to original
   */
  if (fstat(m_fd, &st) != 0) {
     berrno be;

     Mmsg2(errmsg, _("Unable to stat device %s. ERR=%s\n"), print_name(), be.bstrerror());
     return false;
  }

  if (st.st_size != 0) {             /* ftruncate() didn't work */
     POOL_MEM archive_name(PM_FNAME);

     pm_strcpy(archive_name, dev_name);
     if (!IsPathSeparator(archive_name.c_str()[strlen(archive_name.c_str())-1])) {
        pm_strcat(archive_name, "/");
     }
     pm_strcat(archive_name, dcr->VolumeName);

     Mmsg2(errmsg, _("Device %s doesn't support ftruncate(). Recreating file %s.\n"),
           print_name(), archive_name.c_str());

     /*
      * Close file and blow it away
      */
     ::close(m_fd);
     ::unlink(archive_name.c_str());

     /*
      * Recreate the file -- of course, empty
      */
     set_mode(CREATE_READ_WRITE);
     if ((m_fd = ::open(archive_name.c_str(), oflags, st.st_mode)) < 0) {
        berrno be;

        dev_errno = errno;
        Mmsg2(errmsg, _("Could not reopen: %s, ERR=%s\n"), archive_name.c_str(), be.bstrerror());
        Dmsg1(100, "reopen failed: %s", errmsg);
        Emsg0(M_FATAL, 0, errmsg);
        return false;
     }

     /*
      * Reset proper owner
      */
     chown(archive_name.c_str(), st.st_uid, st.st_gid);
  }

  return true;
}

unix_file_device::~unix_file_device()
{
}

unix_file_device::unix_file_device()
{
   m_fd = -1;
}
