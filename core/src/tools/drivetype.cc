/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2006-2006 Free Software Foundation Europe e.V.
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
 * Written by Robert Nelson, June 2006
 */
/**
 * @file
 * Program for determining drive type
 */

#include "bareos.h"
#include "findlib/find.h"
#include "findlib/drivetype.h"

static void usage()
{
   fprintf(stderr, _(
"\n"
"Usage: drivetype [-v] path ...\n"
"\n"
"       Print the drive type a given file/directory is on.\n"
"       The following options are supported:\n"
"\n"
"       -l     print local fixed hard drive\n"
"       -a     display information on all drives\n"
"       -v     print both path and file system type.\n"
"       -?     print this message.\n"
"\n"));

   exit(1);
}

int display_drive(char *drive, bool display_local, int verbose)
{
   char dt[100];
   int status = 0;

   if (drivetype(drive, dt, sizeof(dt))) {
      if (display_local) {      /* in local mode, display only harddrive */
         if (bstrcmp(dt, "fixed")) {
            printf("%s\n", drive);
         }
      } else if (verbose) {
         printf("%s: %s\n", drive, dt);
      } else {
         puts(dt);
      }
   } else if (!display_local) { /* local mode is used by FileSet scripts */
      fprintf(stderr, _("%s: unknown\n"), drive);
      status = 1;
   }
   return status;
}

int
main (int argc, char *const *argv)
{
   int verbose = 0;
   int status = 0;
   int ch, i;
   bool display_local = false;
   bool display_all = false;
   char drive='A';
   char buf[16];

   setlocale(LC_ALL, "");
   bindtextdomain("bareos", LOCALEDIR);
   textdomain("bareos");

   while ((ch = getopt(argc, argv, "alv?")) != -1) {
      switch (ch) {
         case 'v':
            verbose = 1;
            break;
         case 'l':
            display_local = true;
            break;
         case 'a':
            display_all = true;
            break;
         case '?':
         default:
            usage();

      }
   }
   argc -= optind;
   argv += optind;

   OSDependentInit();

   if (argc < 1 && display_all) {
      /* Try all letters */
      for (drive = 'A'; drive <= 'Z'; drive++) {
         bsnprintf(buf, sizeof(buf), "%c:/", drive);
         display_drive(buf, display_local, verbose);
      }
      exit(status);
   }

   if (argc < 1) {
      usage();
   }

   for (i = 0; i < argc; --argc, ++argv) {
      status += display_drive(*argv, display_local, verbose);
   }
   exit(status);
}
