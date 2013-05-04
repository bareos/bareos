/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2006-2008 Free Software Foundation Europe e.V.

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation, which is
   listed in the file LICENSE.

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
 * Test program for testing regular expressions.
 *
 * Kern Sibbald, MMVI
 */

/*
 *  If you define BAREOS_REGEX, bregex will be built with the
 *  Bareos bregex library, which is the same code that we
 *  use on Win32, thus using Linux, you can test your Win32
 *  expressions. Otherwise, this program will link with the
 *  system library routines.
 */
//#define BAREOS_REGEX

#include "bareos.h"
#include <stdio.h>
#include "lib/breg.h"

static void usage()
{
   fprintf(stderr,
"\n"
"Usage: bregtest [-d debug_level] [-s] -f <data-file> -e /test/test2/\n"
"       -f          specify file of data to be matched\n"
"       -e          specify expression\n"
"       -s          sed output\n"
"       -d <nn>     set debug level to <nn>\n"
"       -dt         print timestamp in debug output\n"
"       -?          print this message.\n"
"\n");

   exit(1);
}


int main(int argc, char *const *argv)
{
   char *fname = NULL;
   char *expr = NULL;
   int ch;
   bool sed=false;
   char data[1000];
   FILE *fd;

   setlocale(LC_ALL, "");
   bindtextdomain("bareos", LOCALEDIR);
   textdomain("bareos");

   while ((ch = getopt(argc, argv, "sd:f:e:")) != -1) {
      switch (ch) {
      case 'd':                       /* set debug level */
         if (*optarg == 't') {
            dbg_timestamp = true;
         } else {
            debug_level = atoi(optarg);
            if (debug_level <= 0) {
               debug_level = 1;
            }
         }
         break;

      case 'f':                       /* data */
         fname = optarg;
         break;

      case 'e':
         expr = optarg;
         break;

      case 's':
         sed=true;
         break;

      case '?':
      default:
         usage();

      }
   }
   argc -= optind;
   argv += optind;

   if (!fname) {
      printf("A data file must be specified.\n");
      usage();
   }

   if (!expr) {
      printf("An expression must be specified.\n");
      usage();
   }

   OSDependentInit();

   alist *list;
   char *p;

   list = get_bregexps(expr);

   if (!list) {
      printf("Can't use %s as 'sed' expression\n", expr);
      exit (1);
   }

   fd = fopen(fname, "r");
   if (!fd) {
      printf(_("Could not open data file: %s\n"), fname);
      exit(1);
   }

   while (fgets(data, sizeof(data)-1, fd)) {
      strip_trailing_newline(data);
      apply_bregexps(data, list, &p);
      if (sed) {
         printf("%s\n", p);
      } else {
         printf("%s => %s\n", data, p);
      }
   }
   fclose(fd);
   free_bregexps(list);
   delete list;
   exit(0);
}
/*
  TODO:
   - ajout /g

   - tests
   * test avec /i
   * test avec un sed et faire un diff
   * test avec une boucle pour voir les fuites

*/
