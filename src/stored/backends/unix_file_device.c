/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.
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
 * UNIX FILE API device abstraction.
 *
 * Kern Sibbald, MM
 *
 * Extracted from other source files Marco van Wieringen, December 2013
 */

#include "bareos.h"
#include "stored.h"
#include "unix_file_device.h"

/*
 * (Un)mount the device (For a FILE device)
 */
static bool do_mount(DCR *dcr, bool mount, int dotimeout)
{
   DEVRES *device = dcr->dev->device;
   POOL_MEM ocmd(PM_FNAME);
   POOLMEM *results;
   DIR* dp;
   char *icmd;
   struct dirent *entry, *result;
   int status, tries, name_max, count;
   berrno be;

   Dsm_check(200);
   if (mount) {
      icmd = device->mount_command;
   } else {
      icmd = device->unmount_command;
   }

   dcr->dev->edit_mount_codes(ocmd, icmd);

   Dmsg2(100, "do_mount: cmd=%s mounted=%d\n", ocmd.c_str(), dcr->dev->is_mounted());

   if (dotimeout) {
      /* Try at most 10 times to (un)mount the device. This should perhaps be configurable. */
      tries = 10;
   } else {
      tries = 1;
   }
   results = get_memory(4000);

   /* If busy retry each second */
   Dmsg1(100, "do_mount run_prog=%s\n", ocmd.c_str());
   while ((status = run_program_full_output(ocmd.c_str(), dcr->dev->max_open_wait / 2, results)) != 0) {
      /* Doesn't work with internationalization (This is not a problem) */
      if (mount && fnmatch("*is already mounted on*", results, 0) == 0) {
         break;
      }
      if (!mount && fnmatch("* not mounted*", results, 0) == 0) {
         break;
      }
      if (tries-- > 0) {
         /* Sometimes the device cannot be mounted because it is already mounted.
          * Try to unmount it, then remount it */
         if (mount) {
            Dmsg1(400, "Trying to unmount the device %s...\n", dcr->dev->print_name());
            do_mount(dcr, 0, 0);
         }
         bmicrosleep(1, 0);
         continue;
      }
      Dmsg5(100, "Device %s cannot be %smounted. status=%d result=%s ERR=%s\n", dcr->dev->print_name(),
           (mount ? "" : "un"), status, results, be.bstrerror(status));
      Mmsg(dcr->dev->errmsg, _("Device %s cannot be %smounted. ERR=%s\n"),
           dcr->dev->print_name(), (mount ? "" : "un"), be.bstrerror(status));

      /*
       * Now, just to be sure it is not mounted, try to read the filesystem.
       */
      name_max = pathconf(".", _PC_NAME_MAX);
      if (name_max < 1024) {
         name_max = 1024;
      }

      if (!(dp = opendir(device->mount_point))) {
         berrno be;
         dcr->dev->dev_errno = errno;
         Dmsg3(100, "do_mount: failed to open dir %s (dev=%s), ERR=%s\n",
               device->mount_point, dcr->dev->print_name(), be.bstrerror());
         goto get_out;
      }

      entry = (struct dirent *)malloc(sizeof(struct dirent) + name_max + 1000);
      count = 0;
      while (1) {
         if ((readdir_r(dp, entry, &result) != 0) || (result == NULL)) {
            dcr->dev->dev_errno = EIO;
            Dmsg2(129, "do_mount: failed to find suitable file in dir %s (dev=%s)\n",
                  device->mount_point, dcr->dev->print_name());
            break;
         }
         if (!bstrcmp(result->d_name, ".") && !bstrcmp(result->d_name, "..") && !bstrcmp(result->d_name, ".keep")) {
            count++; /* result->d_name != ., .. or .keep (Gentoo-specific) */
            break;
         } else {
            Dmsg2(129, "do_mount: ignoring %s in %s\n", result->d_name, device->mount_point);
         }
      }
      free(entry);
      closedir(dp);

      Dmsg1(100, "do_mount: got %d files in the mount point (not counting ., .. and .keep)\n", count);

      if (count > 0) {
         /* If we got more than ., .. and .keep */
         /*   there must be something mounted */
         if (mount) {
            Dmsg1(100, "Did Mount by count=%d\n", count);
            break;
         } else {
            /* An unmount request. We failed to unmount - report an error */
            free_pool_memory(results);
            Dmsg0(200, "== error mount=1 wanted unmount\n");
            return false;
         }
      }
get_out:
      free_pool_memory(results);
      Dmsg0(200, "============ mount=0\n");
      Dsm_check(200);
      return false;
   }

   free_pool_memory(results);
   Dmsg1(200, "============ mount=%d\n", mount);
   return true;
}

/*
 * Mount the device.
 *
 * If timeout, wait until the mount command returns 0.
 * If !timeout, try to mount the device only once.
 */
bool unix_file_device::mount_backend(DCR *dcr, int timeout)
{
   bool retval = true;

   if (requires_mount() && device->mount_command) {
      retval = do_mount(dcr, true, timeout);
   }

   return retval;
}

/*
 * Unmount the device
 *
 * If timeout, wait until the unmount command returns 0.
 * If !timeout, try to unmount the device only once.
 */
bool unix_file_device::unmount_backend(DCR *dcr, int timeout)
{
   bool retval = true;

   if (requires_mount() && device->unmount_command) {
      retval = do_mount(dcr, false, timeout);
   }

   return retval;
}

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

      Mmsg2(errmsg, _("Unable to truncate device %s. ERR=%s\n"), prt_name, be.bstrerror());
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

      Mmsg2(errmsg, _("Unable to stat device %s. ERR=%s\n"), prt_name, be.bstrerror());
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
            prt_name, archive_name.c_str());

      /*
       * Close file and blow it away
       */
      ::close(m_fd);
      ::unlink(archive_name.c_str());

      /*
       * Recreate the file -- of course, empty
       */
      oflags = O_CREAT | O_RDWR | O_BINARY;
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
}
