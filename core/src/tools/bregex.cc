/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2006-2006 Free Software Foundation Europe e.V.
   Copyright (C) 2016-2025 Bareos GmbH & Co. KG

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
// Kern Sibbald, MMVI
/**
 * @file
 * Test program for testing regular expressions.
 */

#include "include/bareos.h"
#include "include/exit_codes.h"
#include "lib/cli.h"

#if __has_include(<regex.h>)
#  include <regex.h>
#else
#  include "lib/bregex.h"
#endif

int main(int argc, char** argv)
{
  setlocale(LC_ALL, "");
  tzset();
  bindtextdomain("bareos", LOCALEDIR);
  textdomain("bareos");

  CLI::App bregex_app;
  InitCLIApp(bregex_app, "The Bareos Regular Expression tool.");

  AddDebugOptions(bregex_app);

  std::string fname{};
  bregex_app
      .add_option("-f,--filename", fname, "Specify file or data to be matched.")
      ->required();

  bool no_linenos = false;
  bregex_app.add_flag("-l,--suppress-linenumbers", no_linenos,
                      "Suppress line numbers.");

  bool match_only = true;
  bregex_app.add_flag(
      "-n,--not-match", [&match_only](bool) { match_only = false; },
      "Print line that do not match.");

  ParseBareosApp(bregex_app, argc, argv);

  OSDependentInit();

  regex_t preg{};
  char prbuf[500];
  char data[1000];
  char pat[500];
  FILE* fd;
  int rc;
  int lineno;

  for (;;) {
    printf("Enter regex pattern: ");
    if (fgets(pat, sizeof(pat) - 1, stdin) == NULL) { break; }
    StripTrailingNewline(pat);
    if (pat[0] == 0) { exit(0); }
    rc = regcomp(&preg, pat, REG_EXTENDED);
    if (rc != 0) {
      regerror(rc, &preg, prbuf, sizeof(prbuf));
      printf("Regex compile error: %s\n", prbuf);
      continue;
    }
    fd = fopen(fname.c_str(), "r");
    if (!fd) {
      printf(T_("Could not open data file: %s\n"), fname.c_str());
      exit(BEXIT_FAILURE);
    }
    lineno = 0;
    while (fgets(data, sizeof(data) - 1, fd)) {
      StripTrailingNewline(data);
      lineno++;
      rc = regexec(&preg, data, 0, NULL, 0);
      if ((match_only && rc == 0) || (!match_only && rc != 0)) {
        if (no_linenos) {
          printf("%s\n", data);
        } else {
          printf("%5d: %s\n", lineno, data);
        }
      }
    }
    fclose(fd);
    regfree(&preg);
  }
  return BEXIT_SUCCESS;
}
