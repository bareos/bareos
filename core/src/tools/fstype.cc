/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2004-2006 Free Software Foundation Europe e.V.
   Copyright (C) 2016-2024 Bareos GmbH & Co. KG

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
// Written by Preben 'Peppe' Guldberg, December MMIV
/**
 * @file
 * Program for determining file system type
 */

#if !defined(_MSC_VER)
#include <unistd.h>
#endif
#include "include/bareos.h"
#include "include/exit_codes.h"
#include "findlib/find.h"
#include "lib/mntent_cache.h"
#include "findlib/fstype.h"

#if _MSC_VER
char* optarg {};
#endif

static void usage(int exit_status)
{
  fprintf(stderr,
          T_("\n"
             "Usage: fstype [-v] path ...\n"
             "\n"
             "       Print the file system type a given file/directory is on.\n"
             "       The following options are supported:\n"
             "\n"
             "       -v     print both path and file system type.\n"
             "       -?     print this message.\n"
             "\n"));

  exit(exit_status);
}


int main(int argc, char* const* argv)
{
  char fs[1000];
  bool verbose_flag = false;
  int exit_status = BEXIT_SUCCESS;
  int ch, i;

  setlocale(LC_ALL, "");
  tzset();
  bindtextdomain("bareos", LOCALEDIR);
  textdomain("bareos");

  while ((ch = getopt(argc, argv, "v?")) != -1) {
    switch (ch) {
      case 'v':
        verbose_flag = true;
        break;
      case '?':
        usage(BEXIT_SUCCESS);
        break;
      default:
        usage(BEXIT_FAILURE);
    }
  }
  argc -= optind;
  argv += optind;

  if (argc < 1) { usage(BEXIT_FAILURE); }

  OSDependentInit();

  for (i = 0; i < argc; --argc, ++argv) {
    if (fstype(*argv, fs, sizeof(fs))) {
      if (verbose_flag) {
        printf("%s: %s\n", *argv, fs);
      } else {
        puts(fs);
      }
    } else {
      fprintf(stderr, T_("%s: unknown\n"), *argv);
      exit_status = BEXIT_FAILURE;
    }
  }

  FlushMntentCache();

  exit(exit_status);
}
