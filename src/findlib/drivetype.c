/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2006-2007 Free Software Foundation Europe e.V.

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
 *  Implement routines to determine drive type (Windows specific).
 *
 *   Written by Robert Nelson, June 2006
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
   "HAVE_WIN32\n"
#define false              0
#define true               1
#define bstrncpy           strncpy
#define Dmsg0(n,s)         fprintf(stderr, s)
#define Dmsg1(n,s,a1)      fprintf(stderr, s, a1)
#define Dmsg2(n,s,a1,a2)   fprintf(stderr, s, a1, a2)
#endif

/*
 * These functions should be implemented for each OS
 *
 *       bool drivetype(const char *fname, char *dt, int dtlen);
 */

#if defined (HAVE_WIN32)
/* Windows */

bool drivetype(const char *fname, char *dt, int dtlen)
{
   CHAR rootpath[4];
   UINT type;

   /* Copy Drive Letter, colon, and backslash to rootpath */
   bstrncpy(rootpath, fname, 3);
   rootpath[3] = '\0';

   type = GetDriveType(rootpath);

   switch (type) {
   case DRIVE_REMOVABLE:   bstrncpy(dt, "removable", dtlen);   return true;
   case DRIVE_FIXED:       bstrncpy(dt, "fixed", dtlen);       return true;
   case DRIVE_REMOTE:      bstrncpy(dt, "remote", dtlen);      return true;
   case DRIVE_CDROM:       bstrncpy(dt, "cdrom", dtlen);       return true;
   case DRIVE_RAMDISK:     bstrncpy(dt, "ramdisk", dtlen);     return true;
   case DRIVE_UNKNOWN:
   case DRIVE_NO_ROOT_DIR:
   default:
      return false;
   }
}
/* Windows */

#else    /* No recognised OS */

bool drivetype(const char *fname, char *dt, int dtlen)
{
   Dmsg0(10, "!!! drivetype() not implemented for this OS. !!!\n");
#ifdef TEST_PROGRAM
   Dmsg1(10, "Please define one of the following when compiling:\n\n%s\n",
         SUPPORTEDOSES);
   exit(EXIT_FAILURE);
#endif

   return false;
}
#endif

#ifdef TEST_PROGRAM
int main(int argc, char **argv)
{
   char *p;
   char dt[1000];
   int status = 0;

   if (argc < 2) {
      p = (argc < 1) ? "drivetype" : argv[0];
      printf("usage:\t%s path ...\n"
            "\t%s prints the drive type and pathname of the paths.\n",
            p, p);
      return EXIT_FAILURE;
   }
   while (*++argv) {
      if (!drivetype(*argv, dt, sizeof(dt))) {
         status = EXIT_FAILURE;
      } else {
         printf("%s\t%s\n", dt, *argv);
      }
   }
   return status;
}
#endif
