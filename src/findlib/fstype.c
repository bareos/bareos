/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2004-2007 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from
   many others, a complete list can be found in the file AUTHORS.
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

   Bacula® is a registered trademark of Kern Sibbald.
   The licensor of Bacula is the Free Software Foundation Europe
   (FSFE), Fiduciary Program, Sumatrastrasse 25, 8006 Zürich,
   Switzerland, email:ftf@fsfeurope.org.
*/
/*
 *  Implement routines to determine file system types.
 *
 *   Written by Preben 'Peppe' Guldberg, December MMIV
 *
 *   Version $Id$
 */


#ifndef TEST_PROGRAM

#include "bacula.h"
#include "find.h"

#else /* Set up for testing a stand alone program */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define SUPPORTEDOSES \
   "HAVE_DARWIN_OS\n" \
   "HAVE_FREEBSD_OS\n" \
   "HAVE_HPUX_OS\n" \
   "HAVE_IRIX_OS\n" \
   "HAVE_LINUX_OS\n" \
   "HAVE_NETBSD_OS\n" \
   "HAVE_OPENBSD_OS\n" \
   "HAVE_SUN_OS\n" \
   "HAVE_OSF1_OS\n" \
   "HAVE_WIN32\n"
#define false              0
#define true               1
#define bstrncpy           strncpy
#define bstrcmp(s1, s2)    !strcmp(s1, s2)
#define Dmsg0(n,s)         fprintf(stderr, s)
#define Dmsg1(n,s,a1)      fprintf(stderr, s, a1)
#define Dmsg2(n,s,a1,a2)   fprintf(stderr, s, a1, a2)
#endif

/*
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
#ifdef TEST_PROGRAM
   Dmsg1(10, "Please define one of the following when compiling:\n\n%s\n",
         SUPPORTEDOSES);
   exit(EXIT_FAILURE);
#endif

   return false;
}
#endif

/*
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

#ifdef TEST_PROGRAM
int main(int argc, char **argv)
{
   char *p;
   char fs[1000];
   int status = 0;

   if (argc < 2) {
      p = (argc < 1) ? "fstype" : argv[0];
      printf("usage:\t%s path ...\n"
            "\t%s prints the file system type and pathname of the paths.\n",
            p, p);
      return EXIT_FAILURE;
   }
   while (*++argv) {
      if (!fstype(*argv, fs, sizeof(fs))) {
         status = EXIT_FAILURE;
      } else {
         printf("%s\t%s\n", fs, *argv);
      }
   }
   return status;
}
#endif
