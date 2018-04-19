/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2013-2013 Planets Communications B.V.
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
 * Kern Sibbald, MM
 * Extracted from other source files Marco van Wieringen, December 2013
 */
/**
 * @file
 * UNIX FIFO API device abstraction.
 *
 */

#include "include/bareos.h"
#include "stored/stored.h"
#include "win32_fifo_device.h"
#include "stored/autochanger.h"


/**
 * Open a fifo device
 */
void win32_fifo_device::open_device(DeviceControlRecord *dcr, int omode)
{
   file_size = 0;
   int timeout = max_open_wait;
   utime_t start_time = time(NULL);

   mount(dcr, 1);                     /* do mount if required */

   Dmsg0(100, "Open dev: device is fifo\n");

   get_autochanger_loaded_slot(dcr);

   open_mode = omode;
   set_mode(omode);

   if (timeout < 1) {
      timeout = 1;
   }
   errno = 0;

   if (timeout) {
      /*
       * Set open timer
       */
      tid = start_thread_timer(dcr->jcr, pthread_self(), timeout);
   }

   Dmsg2(100, "Try open %s mode=%s\n", prt_name, mode_to_str(omode));

   /*
    * If busy retry each second for max_open_wait seconds
    */
   for ( ;; ) {
      /*
       * Try non-blocking open
       */
      fd_ = d_open(dev_name, oflags | O_NONBLOCK, 0);
      if (fd_ < 0) {
         berrno be;
         dev_errno = errno;
         Dmsg5(100, "Open error on %s omode=%d oflags=%x errno=%d: ERR=%s\n",
               prt_name, omode, oflags, errno, be.bstrerror());
      } else {
         d_close(fd_);
         fd_ = d_open(dev_name, oflags, 0); /* open normally */
         if (fd_ < 0) {
            berrno be;
            dev_errno = errno;
            Dmsg5(100, "Open error on %s omode=%d oflags=%x errno=%d: ERR=%s\n",
                  prt_name, omode, oflags, errno, be.bstrerror());
            break;
         }
         dev_errno = 0;
         lock_door();
         break;                               /* Successfully opened and rewound */
      }
      bmicrosleep(5, 0);

      /*
       * Exceed wait time ?
       */
      if (time(NULL) - start_time >= max_open_wait) {
         break;                       /* yes, get out */
      }
   }

   if (!is_open()) {
      berrno be;
      Mmsg2(errmsg, _("Unable to open device %s: ERR=%s\n"),
            prt_name, be.bstrerror(dev_errno));
      Dmsg1(100, "%s", errmsg);
   }

   /*
    * Stop any open() timer we started
    */
   if (tid) {
      stop_thread_timer(tid);
      tid = 0;
   }

   Dmsg1(100, "open dev: fifo %d opened\n", fd_);
}

bool win32_fifo_device::eod(DeviceControlRecord *dcr)
{
   if (fd_ < 0) {
      dev_errno = EBADF;
      Mmsg1(errmsg, _("Bad call to eod. Device %s not open\n"), prt_name);
      return false;
   }

   Dmsg0(100, "Enter eod\n");
   if (at_eot()) {
      return true;
   }

   clear_eof();         /* remove EOF flag */

   block_num = file = 0;
   file_size = 0;
   file_addr = 0;
   return true;
}

/**
 * (Un)mount the device (For a FILE device)
 */
static bool do_mount(DeviceControlRecord *dcr, bool mount, int dotimeout)
{
   DeviceResource *device = dcr->dev->device;
   PoolMem ocmd(PM_FNAME);
   POOLMEM *results;
   DIR* dp;
   char *icmd;
   struct dirent *result;
#ifdef USE_READDIR_R
   struct dirent *entry;
#endif
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
#ifdef USE_READDIR_R
      entry = (struct dirent *)malloc(sizeof(struct dirent) + name_max + 1000);
#endif
      count = 0;
      while (1) {
#ifdef USE_READDIR_R
         if ((readdir_r(dp, entry, &result) != 0) || (result == NULL)) {
#else
         result = readdir(dp);
         if (result == NULL) {
#endif
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
#ifdef USE_READDIR_R
      free(entry);
#endif
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

/**
 * Mount the device.
 *
 * If timeout, wait until the mount command returns 0.
 * If !timeout, try to mount the device only once.
 */
bool win32_fifo_device::mount_backend(DeviceControlRecord *dcr, int timeout)
{
   bool retval = true;

   if (requires_mount() && device->mount_command) {
      retval = do_mount(dcr, true, timeout);
   }

   return retval;
}

/**
 * Unmount the device
 *
 * If timeout, wait until the unmount command returns 0.
 * If !timeout, try to unmount the device only once.
 */
bool win32_fifo_device::unmount_backend(DeviceControlRecord *dcr, int timeout)
{
   bool retval = true;

   if (requires_mount() && device->unmount_command) {
      retval = do_mount(dcr, false, timeout);
   }

   return retval;
}

int win32_fifo_device::d_open(const char *pathname, int flags, int mode)
{
   return ::open(pathname, flags, mode);
}

ssize_t win32_fifo_device::d_read(int fd, void *buffer, size_t count)
{
   return ::read(fd, buffer, count);
}

ssize_t win32_fifo_device::d_write(int fd, const void *buffer, size_t count)
{
   return ::write(fd, buffer, count);
}

int win32_fifo_device::d_close(int fd)
{
   return ::close(fd);
}

int win32_fifo_device::d_ioctl(int fd, ioctl_req_t request, char *op)
{
   return -1;
}

boffset_t win32_fifo_device::d_lseek(DeviceControlRecord *dcr, boffset_t offset, int whence)
{
   return -1;
}

bool win32_fifo_device::d_truncate(DeviceControlRecord *dcr)
{
   return true;
}

win32_fifo_device::~win32_fifo_device()
{
}

win32_fifo_device::win32_fifo_device()
{
}
