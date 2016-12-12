/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2004-2007 Free Software Foundation Europe e.V.
   Copyright (C) 2016-2016 Bareos GmbH & Co. KG

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   Affero General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
*/
/*
 * Written by Preben 'Peppe' Guldberg, December MMIV
 */
/**
 * @file
 * Implement routines to determine file system types.
 */

#include "bareos.h"
#include "find.h"

/**
 * These functions should be implemented for each OS
 *
 * bool fstype(const char *fname, char *fs, int fslen);
 */
#if defined(HAVE_DARWIN_OS) || \
    defined(HAVE_FREEBSD_OS) || \
    defined(HAVE_OPENBSD_OS)

#include <sys/param.h>
#include <sys/mount.h>

bool fstype(const char *fname, char *fs, int fslen)
{
   struct statfs st;

   if (statfs(fname, &st) == 0) {
      bstrncpy(fs, st.f_fstypename, fslen);
      return true;
   }

   Dmsg1(50, "statfs() failed for \"%s\"\n", fname);
   return false;
}

#elif defined(HAVE_NETBSD_OS)
#include <sys/param.h>
#include <sys/mount.h>
#ifdef HAVE_SYS_STATVFS_H
#include <sys/statvfs.h>
#else
#define statvfs statfs
#endif

bool fstype(const char *fname, char *fs, int fslen)
{
   struct statvfs st;

   if (statvfs(fname, &st) == 0) {
      bstrncpy(fs, st.f_fstypename, fslen);
      return true;
   }

   Dmsg1(50, "statfs() failed for \"%s\"\n", fname);
   return false;
}

#elif defined(HAVE_HPUX_OS) || \
      defined(HAVE_IRIX_OS)

#include <sys/types.h>
#include <sys/statvfs.h>

bool fstype(const char *fname, char *fs, int fslen)
{
   struct statvfs st;

   if (statvfs(fname, &st) == 0) {
      bstrncpy(fs, st.f_basetype, fslen);
      return true;
   }

   Dmsg1(50, "statfs() failed for \"%s\"\n", fname);
   return false;
}

#elif defined(HAVE_LINUX_OS) || \
      defined(HAVE_OSF1_OS)

#include <sys/stat.h>
#include "lib/mntent_cache.h"

bool fstype(const char *fname, char *fs, int fslen)
{
   struct stat st;
   mntent_cache_entry_t *mce;

   if (lstat(fname, &st) == 0) {
      if ((mce = find_mntent_mapping(st.st_dev)) != NULL) {
         bstrncpy(fs, mce->fstype, fslen);
         release_mntent_mapping(mce);
         return true;
      }
      return false;
   }

   Dmsg1(50, "lstat() failed for \"%s\"\n", fname);
   return false;
}

#elif defined(HAVE_SUN_OS)

#include <sys/types.h>
#include <sys/stat.h>

bool fstype(const char *fname, char *fs, int fslen)
{
   struct stat st;

   if (lstat(fname, &st) == 0) {
      bstrncpy(fs, st.st_fstype, fslen);
      return true;
   }

   Dmsg1(50, "lstat() failed for \"%s\"\n", fname);
   return false;
}

#elif defined (HAVE_WIN32)
/* Windows */

bool fstype(const char *fname, char *fs, int fslen)
{
   DWORD componentlength;
   DWORD fsflags;
   CHAR rootpath[4];
   UINT oldmode;
   BOOL result;

   /* Copy Drive Letter, colon, and backslash to rootpath */
   bstrncpy(rootpath, fname, sizeof(rootpath));

   /* We don't want any popups if there isn't any media in the drive */
   oldmode = SetErrorMode(SEM_FAILCRITICALERRORS);

   result = GetVolumeInformation(rootpath, NULL, 0, NULL, &componentlength, &fsflags, fs, fslen);

   SetErrorMode(oldmode);

   if (result) {
      /* Windows returns NTFS, FAT, etc.  Make it lowercase to be consistent with other OSes */
      lcase(fs);
   } else {
      Dmsg2(10, "GetVolumeInformation() failed for \"%s\", Error = %d.\n", rootpath, GetLastError());
   }

   return result != 0;
}

/* Windows */

#else    /* No recognised OS */

bool fstype(const char *fname, char *fs, int fslen)
{
   Dmsg0(10, "!!! fstype() not implemented for this OS. !!!\n");
   return false;
}
#endif

/**
 * Compare function build on top of fstype, OS independent.
 *
 * bool fstype_equals(const char *fname, const char *fstypename);
 */
bool fstype_equals(const char *fname, const char *fstypename)
{
   char fs_typename[128];

   if (fstype(fname, fs_typename, sizeof(fs_typename))) {
      return bstrcmp(fs_typename, fstypename);
   }

   return false;
}
