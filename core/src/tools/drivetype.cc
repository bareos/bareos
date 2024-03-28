/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2006-2006 Free Software Foundation Europe e.V.
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
// Written by Robert Nelson, June 2006
/**
 * @file
 * Program for determining drive type
 */

#if !defined(_MSC_VER)
#include <unistd.h>
#endif
#include "include/bareos.h"
#include "include/exit_codes.h"
#include "findlib/find.h"
#include "findlib/drivetype.h"

#if _MSC_VER
char* optarg {};
#endif

static void usage(int exit_code)
{
  fprintf(stderr,
          T_("\n"
             "Usage: Drivetype [-v] path ...\n"
             "\n"
             "       Print the drive type a given file/directory is on.\n"
             "       The following options are supported:\n"
             "\n"
             "       -l     print local fixed hard drive\n"
             "       -a     display information on all drives\n"
             "       -v     print both path and file system type.\n"
             "       -?     print this message.\n"
             "\n"));

  exit(exit_code);
}

int DisplayDrive(char* drive, bool display_local, bool verbose)
{
  char dt[100];
  int status = BEXIT_SUCCESS;

  if (Drivetype(drive, dt, sizeof(dt))) {
    if (display_local) { /* in local mode, display only harddrive */
      if (bstrcmp(dt, "fixed")) { printf("%s\n", drive); }
    } else if (verbose) {
      printf("%s: %s\n", drive, dt);
    } else {
      puts(dt);
    }
  } else if (!display_local) { /* local mode is used by FileSet scripts */
    fprintf(stderr, T_("%s: unknown\n"), drive);
    status = BEXIT_FAILURE;
  }
  return status;
}

int main(int argc, char* const* argv)
{
  bool verbose_flag = false;
  int ch, i;
  bool display_local = false;
  bool display_all = false;
  char drive = 'A';
  char buf[16];

  setlocale(LC_ALL, "");
  tzset();
  bindtextdomain("bareos", LOCALEDIR);
  textdomain("bareos");

  while ((ch = getopt(argc, argv, "alv?")) != -1) {
    switch (ch) {
      case 'v':
        verbose_flag = true;
        break;
      case 'l':
        display_local = true;
        break;
      case 'a':
        display_all = true;
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

  OSDependentInit();

  if (display_all) {
    /* Try all letters */
    for (drive = 'A'; drive <= 'Z'; drive++) {
      Bsnprintf(buf, sizeof(buf), "%c:/", drive);
      DisplayDrive(buf, display_local, verbose_flag);
    }
    exit(BEXIT_SUCCESS);
  }

  if (argc < 1) { usage(BEXIT_FAILURE); }

  int exit_status = BEXIT_SUCCESS;
  for (i = 0; i < argc; --argc, ++argv) {
    exit_status = DisplayDrive(*argv, display_local, verbose_flag);
  }
  exit(exit_status);
}
