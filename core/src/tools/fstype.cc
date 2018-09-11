/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2004-2006 Free Software Foundation Europe e.V.
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
 * Program for determining file system type
 */

#include "include/bareos.h"
#include "findlib/find.h"
#include "lib/mntent_cache.h"
#include "findlib/fstype.h"

static void usage()
{
   fprintf(stderr, _(
"\n"
"Usage: fstype [-v] path ...\n"
"\n"
"       Print the file system type a given file/directory is on.\n"
"       The following options are supported:\n"
"\n"
"       -v     print both path and file system type.\n"
"       -?     print this message.\n"
"\n"));

   exit(1);
}


int
main (int argc, char *const *argv)
{
   char fs[1000];
   int verbose = 0;
   int status = 0;
   int ch, i;

   setlocale(LC_ALL, "");
   bindtextdomain("bareos", LOCALEDIR);
   textdomain("bareos");

   while ((ch = getopt(argc, argv, "v?")) != -1) {
      switch (ch) {
         case 'v':
            verbose = 1;
            break;
         case '?':
         default:
            usage();

      }
   }
   argc -= optind;
   argv += optind;

   if (argc < 1) {
      usage();
   }

   OSDependentInit();

   for (i = 0; i < argc; --argc, ++argv) {
      if (fstype(*argv, fs, sizeof(fs))) {
         if (verbose) {
            printf("%s: %s\n", *argv, fs);
         } else {
            puts(fs);
         }
      } else {
         fprintf(stderr, _("%s: unknown\n"), *argv);
         status = 1;
      }
   }

   FlushMntentCache();

   exit(status);
}
