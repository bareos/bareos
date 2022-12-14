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
#include "lib/cli.h"
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

int main(int argc, char* const* argv)
{
  OSDependentInit();

  setlocale(LC_ALL, "");
  tzset();
  bindtextdomain("bareos", LOCALEDIR);
  textdomain("bareos");

  CLI::App testfind_app;
  InitCLIApp(testfind_app, "The Bareos Testfind Tool.", 2000);

  bool attrs = false;
  testfind_app.add_flag("-a,--print-attributes", attrs,
                        "Print extended attributes (Win32 debug).");

  std::string configfile = ConfigurationParser::GetDefaultConfigDir();
  testfind_app
      .add_option("-c,--config", configfile,
                  "Use <path> as configuration file or directory.")
      ->check(CLI::ExistingPath)
      ->type_name("<path>");

  AddDebugOptions(testfind_app);

  std::string fileset_name = "SelfTest";
  testfind_app.add_option("-f,--fileset", fileset_name,
                          "Specify which FileSet to use.");

  CLI11_PARSE(testfind_app, argc, argv);

  directordaemon::my_config = InitDirConfig(configfile.c_str(), M_ERROR_TERM);
  directordaemon::my_config->ParseConfig();


  MessagesResource* msg;

  foreach_res (msg, R_MSGS) { InitMsg(NULL, msg); }

  JobControlRecord* jcr;
  jcr = NewDirectorJcr(TestfindFreeJcr);

  FilesetResource* dir_fileset = (FilesetResource*)my_config->GetResWithName(
      R_FILESET, fileset_name.c_str());

  if (dir_fileset == NULL) {
    fprintf(stderr, "%s: Fileset not found\n", fileset_name.c_str());

    FilesetResource* var;

    fprintf(stderr, "Valid FileSets:\n");

    foreach_res (var, R_FILESET) {
      fprintf(stderr, "    %s\n", var->resource_name_);
    }

    exit(1);
  }

  launchFileDaemonLogic(dir_fileset, configfile.c_str(), attrs);

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
