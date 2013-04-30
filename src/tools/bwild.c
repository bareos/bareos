/*
 * Test program for testing wild card expressions
 * 
 *  Kern Sibbald, MMVI
 *
 */
/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2006-2006 Free Software Foundation Europe e.V.

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

#include "bacula.h"
#include "lib/fnmatch.h"

static void usage()
{
   fprintf(stderr,
"\n"
"Usage: bwild [-d debug_level] -f <data-file>\n"
"       -f          specify file of data to be matched\n"
"       -i          use case insenitive match\n"
"       -l          suppress line numbers\n"
"       -n          print lines that do not match\n"
"       -?          print this message.\n"
"\n\n");

   exit(1);
}

int main(int argc, char *const *argv)
{
   char *fname = NULL;
   int rc, ch;
   char data[1000];
   char pat[500];
   FILE *fd;
   bool match_only = true;
   int lineno;
   bool no_linenos = false;
   int ic = 0;
   

   setlocale(LC_ALL, "");
   bindtextdomain("bacula", LOCALEDIR);
   textdomain("bacula");

   while ((ch = getopt(argc, argv, "d:f:in?")) != -1) {
      switch (ch) {
      case 'd':                       /* set debug level */
         debug_level = atoi(optarg);
         if (debug_level <= 0) {
            debug_level = 1;
         }
         break;

      case 'f':                       /* data */
         fname = optarg;
         break;

      case 'i':                       /* ignore case */
         ic = FNM_CASEFOLD;
         break;

      case 'l':
         no_linenos = true;
         break;

      case 'n':
         match_only = false;
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

   OSDependentInit();

   for ( ;; ) {
      printf("Enter a wild-card: ");
      if (fgets(pat, sizeof(pat)-1, stdin) == NULL) {
         break;
      }
      strip_trailing_newline(pat);
      if (pat[0] == 0) {
         exit(0);
      }
      fd = fopen(fname, "r");
      if (!fd) {
         printf(_("Could not open data file: %s\n"), fname);
         exit(1);
      }
      lineno = 0;
      while (fgets(data, sizeof(data)-1, fd)) {
         strip_trailing_newline(data);
         lineno++;
         rc = fnmatch(pat, data, ic);
         if ((match_only && rc == 0) || (!match_only && rc != 0)) {
            if (no_linenos) {
               printf("%s\n", data);
            } else {
               printf("%5d: %s\n", lineno, data);
            }
         }
      }
      fclose(fd);
   }
   exit(0);
}
