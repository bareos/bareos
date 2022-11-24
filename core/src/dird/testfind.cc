/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2008 Free Software Foundation Europe e.V.
   Copyright (C) 2016-2023 Bareos GmbH & Co. KG

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
// Kern Sibbald, MM
/**
 * @file
 * Test program for find files
 */


#include "include/bareos.h"
#include "include/ch.h"

#include "dird/dird_conf.h"
#include "dird/dird_globals.h"
#include "dird/jcr_util.h"
#include "dird/testfind_jcr.h"
#include "filed/fileset.h"

#include "lib/mntent_cache.h"
#include "lib/parse_conf.h"
#include "dird/jcr_util.h"
#include "dird/dird_globals.h"
#include "dird/dird_conf.h"
#include "dird/director_jcr_impl.h"
#include "lib/recent_job_results_list.h"
#include "lib/tree.h"
#include "findlib/find.h"
#include "findlib/attribs.h"

#if defined(HAVE_WIN32)
#  define isatty(fd) (fd == 0)
#endif


using namespace directordaemon;

void TestfindFreeJcr(JobControlRecord* jcr)
{
  Dmsg0(200, "Start testfind FreeJcr\n");

  if (jcr->dir_impl) {
    delete jcr->dir_impl;
    jcr->dir_impl = nullptr;
  }

  Dmsg0(200, "End testfind FreeJcr\n");
}


extern bool ParseDirConfig(const char* configfile, int exit_code);

/* Global variables */

static int attrs = 0;

static void usage()
{
  fprintf(
      stderr,
      _("\n"
        "Usage: testfind [-d debug_level] [-] [pattern1 ...]\n"
        "       -a          print extended attributes (Win32 debug)\n"
        "       -d <nn>     set debug level to <nn>\n"
        "       -dt         print timestamp in debug output\n"
        "       -c          specify config file containing FileSet resources\n"
        "       -f          specify which FileSet to use\n"
        "       -?          print this message.\n"
        "\n"
        "Patterns are used for file inclusion -- normally directories.\n"
        "Debug level >= 1 prints each file found.\n"
        "Debug level >= 10 prints path/file for catalog.\n"
        "Errors are always printed.\n"
        "Files/paths truncated is the number of files/paths with len > 255.\n"
        "Truncation is only in the catalog.\n"
        "\n"));

  exit(1);
}


int main(int argc, char* const* argv)
{
  const char* configfile = ConfigurationParser::GetDefaultConfigDir();
  const char* fileset_name = "SelfTest";
  int ch;

  OSDependentInit();

  setlocale(LC_ALL, "");
  tzset();
  bindtextdomain("bareos", LOCALEDIR);
  textdomain("bareos");

  while ((ch = getopt(argc, argv, "ac:d:f:?")) != -1) {
    switch (ch) {
      case 'a': /* print extended attributes *debug* */
        attrs = 1;
        break;

      case 'c': /* set debug level */
        configfile = optarg;
        break;

      case 'd': /* set debug level */
        if (*optarg == 't') {
          dbg_timestamp = true;
        } else {
          debug_level = atoi(optarg);
          if (debug_level <= 0) { debug_level = 1; }
        }
        break;

      case 'f': /* exclude patterns */
        fileset_name = optarg;
        break;

      case '?':
      default:
        usage();
    }
  }

  argc -= optind;
  argv += optind;

  directordaemon::my_config = InitDirConfig(configfile, M_ERROR_TERM);
  directordaemon::my_config->ParseConfig();


  MessagesResource* msg;

  foreach_res (msg, R_MSGS) { InitMsg(NULL, msg); }

  JobControlRecord* jcr;
  jcr = NewDirectorJcr(TestfindFreeJcr);

  FilesetResource* dir_fileset
      = (FilesetResource*)my_config->GetResWithName(R_FILESET, fileset_name);

  if (dir_fileset == NULL) {
    fprintf(stderr, "%s: Fileset not found\n", fileset_name);

    FilesetResource* var;

    fprintf(stderr, "Valid FileSets:\n");

    foreach_res (var, R_FILESET) {
      fprintf(stderr, "    %s\n", var->resource_name_);
    }

    exit(1);
  }

  launchFileDaemonLogic(dir_fileset, configfile, attrs);

  FreeJcr(jcr);
  if (my_config) {
    delete my_config;
    my_config = NULL;
  }

  RecentJobResultsList::Cleanup();
  CleanupJcrChain();


  FlushMntentCache();

  TermMsg();

  exit(0);
}
