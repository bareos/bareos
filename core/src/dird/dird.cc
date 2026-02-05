/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2016 Planets Communications B.V.
   Copyright (C) 2013-2026 Bareos GmbH & Co. KG

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
// Kern Sibbald, March MM
/**
 * @file
 * the main program
 */

#include "include/bareos.h"
#include "include/exit_codes.h"
#include "cats/sql.h"
#include "cats/sql_pooling.h"
#include "dird.h"
#include "dird_globals.h"
#include "dird/check_catalog.h"
#include "dird/job.h"
#include "dird/scheduler.h"
#include "dird/socket_server.h"
#include "dird/stats.h"
#include "lib/daemon.h"
#include "lib/berrno.h"
#include "lib/edit.h"
#include "lib/tls_openssl.h"
#include "lib/bsignal.h"
#include "lib/daemon.h"
#include "lib/parse_conf.h"
#include "lib/thread_specific_data.h"
#include "lib/util.h"
#include "lib/watchdog.h"
#include "lib/cli.h"

#include "dird/reload.h"

#include "lib/bregex.h"

#include <dirent.h>
#define NAMELEN(dirent) (strlen((dirent)->d_name))
#ifndef HAVE_READDIR_R
int Readdir_r(DIR* dirp, struct dirent* entry, struct dirent** result);
#endif

using namespace directordaemon;

/* Forward referenced subroutines */
namespace directordaemon {

#if !defined(HAVE_WIN32)
static
#endif
    void
    TerminateDird(int sig);
}  // namespace directordaemon

static void CleanUpOldFiles();
static bool InitSighandlerSighup();

/* Exported subroutines */
BAREOS_IMPORT bool ParseDirConfig(const char* configfile, int exit_code);
extern bool PrintMessage(void* sock, const char* fmt, ...);

/* Imported subroutines */
void StoreJobtype(lexer* lc, const ResourceItem* item, int index, int pass);
void StoreProtocoltype(lexer* lc,
                       const ResourceItem* item,
                       int index,
                       int pass);
void StoreLevel(lexer* lc, const ResourceItem* item, int index, int pass);
void StoreReplace(lexer* lc, const ResourceItem* item, int index, int pass);
void StoreMigtype(lexer* lc, const ResourceItem* item, int index, int pass);
void InitDeviceResources();

static bool test_config = false;

static std::string pidfile_path;

/* Globals Imported */
extern const ResourceItem job_items[];


/**
 * This allows the message handler to operate on the database by using a pointer
 * to this function. The pointer is needed because the other daemons do not have
 * access to the database. If the pointer is not defined (other daemons), then
 * writing the database is disabled.
 */
static bool DirDbLogInsert(JobControlRecord* jcr,
                           utime_t mtime,
                           const char* msg)
{
  int length;
  char ed1[50];
  char dt[MAX_TIME_LENGTH];
  PoolMem query(PM_MESSAGE), esc_msg(PM_MESSAGE);

  if (!jcr || !jcr->db || !jcr->db->IsConnected()) { return false; }
  length = strlen(msg);
  esc_msg.check_size(length * 2 + 1);
  jcr->db->EscapeString(jcr, esc_msg.c_str(), msg, length);

  bstrutime(dt, sizeof(dt), mtime);
  Mmsg(query, "INSERT INTO Log (JobId, Time, LogText) VALUES (%s,'%s','%s')",
       edit_int64(jcr->JobId, ed1), dt, esc_msg.c_str());

  return jcr->db->SqlExec(query.c_str());
}

/*********************************************************************
 *
 *         Main BAREOS Director Server program
 *
 */
#if defined(HAVE_WIN32)
#  define main BareosMain
#endif

int main(int argc, char* argv[])
{
  setlocale(LC_ALL, "");
  tzset();
  bindtextdomain("bareos", LOCALEDIR);
  textdomain("bareos");

  InitStackDump();
  MyNameIs(argc, argv, "bareos-dir");
  InitMsg(nullptr, nullptr); /* initialize message handler */
  daemon_start_time = time(nullptr);

  console_command = RunConsoleCommand;

  CLI::App dir_app;
  InitCLIApp(dir_app, "The Bareos Director Daemon.", 2000);

  dir_app
      .add_option(
          "-c,--config",
          [](std::vector<std::string> val) {
            if (configfile != nullptr) { free(configfile); }
            configfile = strdup(val.front().c_str());
            return true;
          },
          "Use <path> as configuration file or directory.")
      ->check(CLI::ExistingPath)
      ->type_name("<path>");

  AddDebugOptions(dir_app);

  bool foreground = false;
  [[maybe_unused]] CLI::Option* foreground_option
      = dir_app.add_flag("-f,--foreground", foreground, "Run in foreground.");

  std::string user;
  std::string group;
  AddUserAndGroupOptions(dir_app, user, group);

  dir_app.add_flag("-m,--print-kaboom", prt_kaboom,
                   "Print kaboom output (for debugging).");

  [[maybe_unused]] auto testconfig_option = dir_app.add_flag(
      "-t,--test-config", test_config, "Test - read configuration and exit.");

#ifndef HAVE_WIN32
  dir_app
      .add_option("-p,--pid-file", pidfile_path,
                  "Full path to pidfile (default: none).")
      ->excludes(foreground_option)
      ->excludes(testconfig_option)
      ->type_name("<file>");
#endif

  bool no_signals = false;
  dir_app.add_flag("-s,--no-signals", no_signals,
                   "No signals (for debugging).");

  AddVerboseOption(dir_app);

  CLI::Option_group* exp = dir_app.add_option_group(
      "configuration export", "various ways to export the configuration");

  bool export_config = false;
  bool export_config_json = false;
  std::string export_config_resourcetype;
  std::string export_config_resourcename;
  exp->add_option(
         "--xc,--export-config",
         [&export_config, &export_config_resourcetype,
          &export_config_resourcename](std::vector<std::string> val) {
           export_config = true;
           if (val.size() >= 1) {
             export_config_resourcetype = val[0];
             if (val.size() >= 2) { export_config_resourcename = val[1]; }
           }
           return true;
         },
         "Print all or specific configuration resources and exit.")
      ->type_name("[<resource_type> [<name>]]")
      ->expected(0, 2);

  bool export_config_schema = false;
  exp->add_flag("--xs,--export-schema", export_config_schema,
                "Print configuration schema in JSON format and exit.");

  exp->add_flag("--xj,--export-json", export_config_json,
                "Print all configuration resources in a json format.");

  exp->require_option(0, 1);

  AddDeprecatedExportOptionsHelp(dir_app);

  ParseBareosApp(dir_app, argc, argv);

  if (!no_signals) { InitSignals(TerminateDird); }

  int pidfile_fd = -1;
#if !defined(HAVE_WIN32)
  if (!test_config && !foreground && !pidfile_path.empty()) {
    pidfile_fd = CreatePidFile("bareos-dir", pidfile_path.c_str());
  }
#endif
  // See if we want to drop privs.
  char* uid = nullptr;
  if (!user.empty()) { uid = user.data(); }

  char* gid = nullptr;
  if (!group.empty()) { gid = group.data(); }

  if (geteuid() == 0) {
    drop(uid, gid, false);  // reduce privileges if requested
  } else if (uid || gid) {
    Emsg2(M_ERROR_TERM, 0,
          T_("The commandline options indicate to run as specified user/group, "
             "but program was not started with required root privileges.\n"));
  }

  my_config = InitDirConfig(configfile, M_CONFIG_ERROR);
  if (export_config_schema) {
    PoolMem buffer;

    PrintConfigSchemaJson(buffer);
    printf("%s\n", buffer.c_str());

    TerminateDird(BEXIT_SUCCESS);
    return BEXIT_SUCCESS;
  }

  my_config->ParseConfigOrExit();

  if (export_config_json) {
    my_config->PrintShape();
  } else if (export_config) {
    int rc = 0;
    if (!my_config->DumpResources(PrintMessage, nullptr,
                                  export_config_resourcetype,
                                  export_config_resourcename)) {
      rc = 1;
    }
    TerminateDird(rc);
    return BEXIT_SUCCESS;
  } else if (export_config_json) {
  }

  if (!CheckResources()) {
    Jmsg((JobControlRecord*)NULL, M_ERROR_TERM, 0,
         T_("Please correct the configuration in %s\n"),
         my_config->get_base_config_path().c_str());

    TerminateDird(BEXIT_SUCCESS);
    return BEXIT_SUCCESS;
  }

  if (my_config->HasWarnings()) {
    // messaging not initialized, so Jmsg with  M_WARNING doesn't work
    fprintf(stderr, T_("There are configuration warnings:\n"));
    for (auto& warning : my_config->GetWarnings()) {
      fprintf(stderr, " * %s\n", warning.c_str());
    }
  }

  if (!test_config && !foreground) {
    daemon_start("bareos-dir", pidfile_fd, pidfile_path);
    InitStackDump();  // grab new pid
  }

  if (InitCrypto() != 0) {
    Jmsg((JobControlRecord*)nullptr, M_ERROR_TERM, 0,
         T_("Cryptography library initialization failed.\n"));

    TerminateDird(BEXIT_SUCCESS);
    return BEXIT_SUCCESS;
  }

  if (!test_config) {
    ReadStateFile(me->working_directory, "bareos-dir",
                  GetFirstPortHostOrder(me->DIRaddrs));
  }

  SetJcrInThreadSpecificData(nullptr);

  LoadDirPlugins(me->plugin_directory, me->plugin_names);


  // If we are in testing mode, we don't try to fix the catalog
  cat_op mode = (test_config) ? CHECK_CONNECTION : UPDATE_AND_FIX;

  if (!CheckCatalog(mode)) {
    Jmsg((JobControlRecord*)nullptr, M_ERROR_TERM, 0,
         T_("Please correct the configuration in %s\n"),
         my_config->get_base_config_path().c_str());

    TerminateDird(BEXIT_SUCCESS);
    return BEXIT_SUCCESS;
  }

  if (test_config) { TerminateDird(0); }

  if (!InitializeSqlPooling()) {
    Jmsg((JobControlRecord*)nullptr, M_ERROR_TERM, 0,
         T_("Please correct the configuration in %s\n"),
         my_config->get_base_config_path().c_str());

    TerminateDird(BEXIT_SUCCESS);
    return BEXIT_SUCCESS;
  }

  MyNameIs(0, nullptr, me->resource_name_); /* set user defined name */

  CleanUpOldFiles();

  SetDbLogInsertCallback(DirDbLogInsert);

  InitSighandlerSighup();

  InitConsoleMsg(working_directory);

  StartWatchdog(); /* start network watchdog thread */

  LockJcrChain();
  InitJcrChain();
  UnlockJcrChain();
  if (me->jcr_watchdog_time) {
    InitJcrSubsystem(
        me->jcr_watchdog_time); /* start JobControlRecord watchdogs etc. */
  }

  InitJobServer(me->MaxConcurrentJobs);

  DbgJcrAddHook(
      DbDebugPrint); /* used to debug BareosDb connexion after fatal signal */

  StartStatisticsThread();

  Dmsg0(200, "Start UA server\n");
  if (!StartSocketServer(me->DIRaddrs)) { TerminateDird(0); }

  Dmsg0(200, "wait for next job\n");

  Scheduler::GetMainScheduler().Run();

  TerminateDird(BEXIT_SUCCESS);
  return BEXIT_SUCCESS;
}

/**
 * Cleanup and then exit
 *
 */
namespace directordaemon {
#if !defined(HAVE_WIN32)
static
#endif
    void
    TerminateDird(int sig)
{
  static bool is_reloading = false;

  if (is_reloading) {  /* avoid recursive termination problems */
    Bmicrosleep(2, 0); /* yield */
    exit(BEXIT_FAILURE);
  }

  is_reloading = true;
  debug_level = 0; /* turn off debug */

  DestroyConfigureUsageString();
  StopSocketServer();
  StopStatisticsThread();
  StopWatchdog();
  DbSqlPoolDestroy();
  UnloadDirPlugins();
  if (!test_config && me) { /* we don't need to do this block in test mode */
    WriteStateFile(me->working_directory, "bareos-dir",
                   GetFirstPortHostOrder(me->DIRaddrs));
    DeletePidFile(pidfile_path);
  }
  Scheduler::GetMainScheduler().Terminate();
  TermJobServer();
  TermMsg(); /* Terminate message handler */

  if (configfile != nullptr) { free(configfile); }
  if (my_config) {
    delete my_config;
    my_config = nullptr;
  }

  CleanupCrypto();

  exit(sig);
}
} /* namespace directordaemon */

/**
 * If we get here, we have received a SIGHUP, which means to reread our
 * configuration file.
 */
#if !defined(HAVE_WIN32)
extern "C" void SighandlerReloadConfig(int, siginfo_t*, void*)
{
  static bool is_reloading = false;

  if (is_reloading) {
    /* Note: don't use Jmsg here, as it could produce a race condition
     * on multiple parallel reloads */
    Qmsg(nullptr, M_ERROR, 0, T_("Already reloading. Request ignored.\n"));
    return;
  }
  is_reloading = true;
  DoReloadConfig();
  is_reloading = false;
}
#endif

static bool InitSighandlerSighup()
{
  bool retval = false;
#if !defined(HAVE_WIN32)
  sigset_t block_mask;
  struct sigaction action = {};

  /*  while handling SIGHUP signal,
   *  ignore further SIGHUP signals. */
  sigemptyset(&block_mask);
  sigaddset(&block_mask, SIGHUP);

  action.sa_sigaction = SighandlerReloadConfig;
  action.sa_mask = block_mask;
  action.sa_flags = SA_SIGINFO;
  sigaction(SIGHUP, &action, nullptr);

  retval = true;
#endif
  return retval;
}

static void CleanUpOldFiles()
{
  DIR* dp;
  struct dirent* result;
#ifdef USE_READDIR_R
  struct dirent* entry;
#endif
  int rc, name_max;
  int my_name_len = strlen(my_name);
  int len = strlen(me->working_directory);
  POOLMEM* cleanup = GetPoolMemory(PM_MESSAGE);
  POOLMEM* basename = GetPoolMemory(PM_MESSAGE);
  regex_t preg1{};
  char prbuf[500];

  /* Exclude spaces and look for .mail or .restore.xx.bsr files */
  const char* pat1 = "^[^ ]+\\.(restore\\.[^ ]+\\.bsr|mail)$";

  /* Setup working directory prefix */
  PmStrcpy(basename, me->working_directory);
  if (len > 0 && !IsPathSeparator(me->working_directory[len - 1])) {
    PmStrcat(basename, "/");
  }

  /* Compile regex expressions */
  rc = regcomp(&preg1, pat1, REG_EXTENDED);
  if (rc != 0) {
    regerror(rc, &preg1, prbuf, sizeof(prbuf));
    Pmsg2(000, T_("Could not compile regex pattern \"%s\" ERR=%s\n"), pat1,
          prbuf);
    goto get_out2;
  }

  name_max = pathconf(".", _PC_NAME_MAX);
  if (name_max < 1024) { name_max = 1024; }

  if (!(dp = opendir(me->working_directory))) {
    BErrNo be;
    Pmsg2(000, "Failed to open working dir %s for cleanup: ERR=%s\n",
          me->working_directory, be.bstrerror());
    goto get_out1;
    return;
  }

#ifdef USE_READDIR_R
  entry = (struct dirent*)malloc(sizeof(struct dirent) + name_max + 1000);
  while (1) {
    if ((Readdir_r(dp, entry, &result) != 0) || (result == nullptr)) {
#else
  while (1) {
    result = readdir(dp);
    if (result == nullptr) {
#endif

      break;
    }
    /* Exclude any name with ., .., not my_name or containing a space */
    if (strcmp(result->d_name, ".") == 0 || strcmp(result->d_name, "..") == 0
        || strncmp(result->d_name, my_name, my_name_len) != 0) {
      Dmsg1(500, "Skipped: %s\n", result->d_name);
      continue;
    }

    /* Unlink files that match regexes */
    if (regexec(&preg1, result->d_name, 0, nullptr, 0) == 0) {
      PmStrcpy(cleanup, basename);
      PmStrcat(cleanup, result->d_name);
      Dmsg1(100, "Unlink: %s\n", cleanup);
      SecureErase(nullptr, cleanup);
    }
  }

#ifdef USE_READDIR_R
  free(entry);
#endif
  closedir(dp);
/* Be careful to free up the correct resources */
get_out1:
  regfree(&preg1);
get_out2:
  FreePoolMemory(cleanup);
  FreePoolMemory(basename);
}
