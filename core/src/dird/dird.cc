/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2016 Planets Communications B.V.
   Copyright (C) 2013-2021 Bareos GmbH & Co. KG

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
#include "cats/cats_backends.h"
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

#ifndef HAVE_REGEX_H
#  include "lib/bregex.h"
#else
#  include <regex.h>
#endif
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

static bool CheckResources();
static bool InitializeSqlPooling(void);
static void CleanUpOldFiles();
static bool InitSighandlerSighup();
static JobControlRecord* PrepareJobToRun(const char* job_name);

/* Exported subroutines */
extern bool ParseDirConfig(const char* configfile, int exit_code);
extern bool PrintMessage(void* sock, const char* fmt, ...);

/* Imported subroutines */
void StoreJobtype(LEX* lc, ResourceItem* item, int index, int pass);
void StoreProtocoltype(LEX* lc, ResourceItem* item, int index, int pass);
void StoreLevel(LEX* lc, ResourceItem* item, int index, int pass);
void StoreReplace(LEX* lc, ResourceItem* item, int index, int pass);
void StoreMigtype(LEX* lc, ResourceItem* item, int index, int pass);
void InitDeviceResources();

static char* runjob = nullptr;
static bool background = true;
static bool test_config = false;

static char* pidfile_path = nullptr;

/* Globals Imported */
extern ResourceItem job_items[];


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

  return jcr->db->SqlQuery(query.c_str());
}

static void usage()
{
  kBareosVersionStrings.PrintCopyrightWithFsfAndPlanets(stderr, 2000);
  fprintf(
      stderr,
      _("Usage: bareos-dir [options]\n"
        "        -c <path>   use <path> as configuration file or directory\n"
        "        -d <nn>     set debug level to <nn>\n"
        "        -dt         print timestamp in debug output\n"
        "        -f          run in foreground (for debugging)\n"
        "        -g <group>  run as group <group>\n"
        "        -m          print kaboom output (for debugging)\n"
#if !defined(HAVE_WIN32)
        "        -p <file>   full path to pidfile (default: none)\n"
#endif
        "        -r <job>    run <job> now\n"
        "        -s          no signals (for debugging)\n"
        "        -t          test - read configuration and exit\n"
        "        -u <user>   run as user <user>\n"
        "        -v          verbose user messages\n"
        "        -xc         print configuration and exit\n"
        "        -xs         print configuration file schema in JSON format "
        "and exit\n"
        "        -?          print this message.\n"
        "\n"));

  exit(1);
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
  int ch;
  cat_op mode;
  bool no_signals = false;
  bool export_config = false;
  bool export_config_schema = false;
  char* uid = nullptr;
  char* gid = nullptr;

  setlocale(LC_ALL, "");
  tzset();
  bindtextdomain("bareos", LOCALEDIR);
  textdomain("bareos");

  InitStackDump();
  MyNameIs(argc, argv, "bareos-dir");
  InitMsg(nullptr, nullptr); /* initialize message handler */
  daemon_start_time = time(nullptr);

  console_command = RunConsoleCommand;

#if HAVE_WIN32
  std::string allowed_parameters("c:d:fg:mr:stu:vx:z:?");
#else
  std::string allowed_parameters("c:d:fg:mr:p:stu:vx:z:?");
#endif

  while ((ch = getopt(argc, argv, allowed_parameters.c_str())) != -1) {
    switch (ch) {
      case 'c': /* specify config file */
        if (configfile != nullptr) { free(configfile); }
        configfile = strdup(optarg);
        break;

      case 'd': /* set debug level */
        if (*optarg == 't') {
          dbg_timestamp = true;
        } else {
          debug_level = atoi(optarg);
          if (debug_level <= 0) { debug_level = 1; }
        }
        Dmsg1(10, "Debug level = %d\n", debug_level);
        break;

      case 'f': /* run in foreground */
        background = false;
        break;

      case 'g': /* set group id */
        gid = optarg;
        break;

      case 'm': /* print kaboom output */
        prt_kaboom = true;
        break;

      case 'p':
        pidfile_path = strdup(optarg);
        break;

      case 'r': /* run job */
        if (runjob != nullptr) { free(runjob); }
        if (optarg) { runjob = strdup(optarg); }
        break;

      case 's': /* turn off signals */
        no_signals = true;
        break;

      case 't': /* test config */
        test_config = true;
        break;

      case 'u': /* set uid */
        uid = optarg;
        break;

      case 'v': /* verbose */
        verbose++;
        break;

      case 'x': /* export configuration/schema and exit */
        if (*optarg == 's') {
          export_config_schema = true;
        } else if (*optarg == 'c') {
          export_config = true;
        } else {
          usage();
        }
        break;

      case 'z': /* switch network debugging on */
        if (!BnetDump::EvaluateCommandLineArgs(optarg)) { exit(1); }
        break;

      case '?':
      default:
        usage();
        break;
    }
  }
  argc -= optind;
  argv += optind;

  if (!background && pidfile_path) {
    Emsg0(M_WARNING, 0,
          "Options -p and -f cannot be used together. You cannot create a "
          "pid file when in foreground mode.\n");

    exit(0);
  }

  if (!no_signals) { InitSignals(TerminateDird); }

  if (argc) {
    if (configfile != nullptr) { free(configfile); }
    configfile = strdup(*argv);
    argc--;
    argv++;
  }
  if (argc) { usage(); }

  int pidfile_fd = -1;
#if !defined(HAVE_WIN32)
  if (!test_config && background && pidfile_path) {
    pidfile_fd = CreatePidFile("bareos-dir", pidfile_path);
  }
#endif
  if (geteuid() == 0) {
    drop(uid, gid, false);  // reduce privileges if requested
  }

  if (export_config_schema) {
    PoolMem buffer;

    my_config = InitDirConfig(configfile, M_ERROR_TERM);
    PrintConfigSchemaJson(buffer);
    printf("%s\n", buffer.c_str());

    TerminateDird(0);
    return 0;
  }

  my_config = InitDirConfig(configfile, M_ERROR_TERM);
  my_config->ParseConfig();

  if (export_config) {
    my_config->DumpResources(PrintMessage, nullptr);

    TerminateDird(0);
    return 0;
  }

  if (!CheckResources()) {
    Jmsg((JobControlRecord*)NULL, M_ERROR_TERM, 0,
         _("Please correct the configuration in %s\n"),
         my_config->get_base_config_path().c_str());

    TerminateDird(0);
    return 0;
  }

  if (!test_config) { /* we don't need to do this block in test mode */
    if (background) {
      daemon_start("bareos-dir", pidfile_fd, pidfile_path);
      InitStackDump(); /* grab new pid */
    }
  }

  if (InitCrypto() != 0) {
    Jmsg((JobControlRecord*)nullptr, M_ERROR_TERM, 0,
         _("Cryptography library initialization failed.\n"));

    TerminateDird(0);
    return 0;
  }

  if (!test_config) { /* we don't need to do this block in test mode */
    /* Create pid must come after we are a daemon -- so we have our final pid */
    ReadStateFile(me->working_directory, "bareos-dir",
                  GetFirstPortHostOrder(me->DIRaddrs));
  }

  SetJcrInThreadSpecificData(nullptr);

#if defined(HAVE_DYNAMIC_CATS_BACKENDS)
  for (const auto& backend_dir : me->backend_directories) {
    Dmsg1(100, "backend path: %s\n", backend_dir.c_str());
  }

  DbSetBackendDirs(me->backend_directories);
#endif
  LoadDirPlugins(me->plugin_directory, me->plugin_names);

  // If we are in testing mode, we don't try to fix the catalog
  mode = (test_config) ? CHECK_CONNECTION : UPDATE_AND_FIX;

  if (!CheckCatalog(mode)) {
    Jmsg((JobControlRecord*)nullptr, M_ERROR_TERM, 0,
         _("Please correct the configuration in %s\n"),
         my_config->get_base_config_path().c_str());

    TerminateDird(0);
    return 0;
  }

  if (test_config) {
    if (my_config->HasWarnings()) {
      /* messaging not initialized, so Jmsg with  M_WARNING doesn't work */
      fprintf(stderr, _("There are configuration warnings:\n"));
      for (auto& warning : my_config->GetWarnings()) {
        fprintf(stderr, " * %s\n", warning.c_str());
      }
    }
    TerminateDird(0);
  }

  if (!InitializeSqlPooling()) {
    Jmsg((JobControlRecord*)nullptr, M_ERROR_TERM, 0,
         _("Please correct the configuration in %s\n"),
         my_config->get_base_config_path().c_str());

    TerminateDird(0);
    return 0;
  }

  MyNameIs(0, nullptr, me->resource_name_); /* set user defined name */

  CleanUpOldFiles();

  SetDbLogInsertCallback(DirDbLogInsert);

  InitSighandlerSighup();

  InitConsoleMsg(working_directory);

  StartWatchdog(); /* start network watchdog thread */

  InitJcrChain();
  if (me->jcr_watchdog_time) {
    InitJcrSubsystem(
        me->jcr_watchdog_time); /* start JobControlRecord watchdogs etc. */
  }

  InitJobServer(me->MaxConcurrentJobs);

  DbgJcrAddHook(
      DbDebugPrint); /* used to debug BareosDb connexion after fatal signal */

  //   InitDeviceResources();

  StartStatisticsThread();

  Dmsg0(200, "Start UA server\n");
  if (!StartSocketServer(me->DIRaddrs)) { TerminateDird(0); }

  Dmsg0(200, "wait for next job\n");

  if (runjob) {
    JobControlRecord* jcr = PrepareJobToRun(runjob);
    if (jcr) { ExecuteJob(jcr); }
  } else {
    Scheduler::GetMainScheduler().Run();
  }

  TerminateDird(0);
  return 0;
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
    exit(1);
  }

  is_reloading = true;
  debug_level = 0; /* turn off debug */

  DestroyConfigureUsageString();
  StopSocketServer();
  StopStatisticsThread();
  StopWatchdog();
  DbSqlPoolDestroy();
  DbFlushBackends();
  UnloadDirPlugins();
  if (!test_config && me) { /* we don't need to do this block in test mode */
    WriteStateFile(me->working_directory, "bareos-dir",
                   GetFirstPortHostOrder(me->DIRaddrs));
    DeletePidFile(pidfile_path);
  }
  Scheduler::GetMainScheduler().Terminate();
  TermJobServer();

  if (runjob) { free(runjob); }
  if (configfile != nullptr) { free(configfile); }
  if (my_config) {
    delete my_config;
    my_config = nullptr;
  }

  TermMsg(); /* Terminate message handler */
  CleanupCrypto();

  exit(sig);
}
} /* namespace directordaemon */

/**
 * If we get here, we have received a SIGHUP, which means to reread our
 * configuration file.
 */
#if !defined(HAVE_WIN32)
extern "C" void SighandlerReloadConfig(int sig, siginfo_t* siginfo, void* ptr)
{
  static bool is_reloading = false;

  if (is_reloading) {
    /*
     * Note: don't use Jmsg here, as it could produce a race condition
     * on multiple parallel reloads
     */
    Qmsg(nullptr, M_ERROR, 0, _("Already reloading. Request ignored.\n"));
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

  /*
   *  while handling SIGHUP signal,
   *  ignore further SIGHUP signals.
   */
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

namespace directordaemon {
bool DoReloadConfig()
{
  static bool is_reloading = false;
  bool reloaded = false;


  if (is_reloading) {
    /*
     * Note: don't use Jmsg here, as it could produce a race condition
     * on multiple parallel reloads
     */
    Qmsg(nullptr, M_ERROR, 0, _("Already reloading. Request ignored.\n"));
    return false;
  }
  is_reloading = true;

  StopStatisticsThread();

  LockJobs();
  LockRes(my_config);

  DbSqlPoolFlush();

  my_config->BackupResourceTable();
  Dmsg0(100, "Reloading config file\n");


  my_config->err_type_ = M_ERROR;
  my_config->ClearWarnings();
  bool ok = my_config->ParseConfig();

  // parse config successful
  if (ok && CheckResources() && CheckCatalog(UPDATE_CATALOG)
      && InitializeSqlPooling()) {
    Scheduler::GetMainScheduler().ClearQueue();

    reloaded = true;

    SetWorkingDirectory(me->working_directory);
    Dmsg0(10, "Director's configuration file reread successfully.\n");
    Dmsg0(10, "Releasing previous configuration resource table.\n");
    my_config->ReleasePreviousResourceTable();

    StartStatisticsThread();

  } else {  // parse config failed
    Jmsg(nullptr, M_ERROR, 0, _("Please correct the configuration in %s\n"),
         my_config->get_base_config_path().c_str());

    Jmsg(nullptr, M_ERROR, 0, _("Resetting to previous configuration.\n"));
    my_config->RestoreResourceTable();
    // me is changed above by CheckResources()
    me = (DirectorResource*)my_config->GetNextRes(R_DIRECTOR, nullptr);
    assert(me);
    my_config->own_resource_ = me;

    StartStatisticsThread();
  }

  UnlockRes(my_config);
  UnlockJobs();
  is_reloading = false;
  return reloaded;
}

} /* namespace directordaemon */

/*
 * See if two storage definitions point to the same Storage Daemon.
 *
 * We compare:
 *  - address
 *  - SDport
 *  - password
 */
static inline bool IsSameStorageDaemon(StorageResource* store1,
                                       StorageResource* store2)
{
  return store1->SDport == store2->SDport
         && Bstrcasecmp(store1->address, store2->address)
         && Bstrcasecmp(store1->password_.value, store2->password_.value);
}

/**
 * Make a quick check to see that we have all the
 * resources needed.
 *
 *  **** FIXME **** this routine could be a lot more
 *   intelligent and comprehensive.
 */
static bool CheckResources()
{
  bool OK = true;
  JobResource* job;
  const std::string& configfile = my_config->get_base_config_path();

  LockRes(my_config);

  job = (JobResource*)my_config->GetNextRes(R_JOB, nullptr);
  me = (DirectorResource*)my_config->GetNextRes(R_DIRECTOR, nullptr);
  my_config->own_resource_ = me;
  if (!me) {
    Jmsg(nullptr, M_FATAL, 0,
         _("No Director resource defined in %s\n"
           "Without that I don't know who I am :-(\n"),
         configfile.c_str());
    OK = false;
    goto bail_out;
  } else {
    // Sanity check.
    if (me->MaxConsoleConnections > me->MaxConnections) {
      me->MaxConnections = me->MaxConsoleConnections + 10;
    }

    my_config->omit_defaults_ = true;
    SetWorkingDirectory(me->working_directory);

    // See if message resource is specified.
    if (!me->messages) {
      me->messages = (MessagesResource*)my_config->GetNextRes(R_MSGS, nullptr);
      if (!me->messages) {
        Jmsg(nullptr, M_FATAL, 0, _("No Messages resource defined in %s\n"),
             configfile.c_str());
        OK = false;
        goto bail_out;
      }
    }

    // When the user didn't force us we optimize for size.
    if (!me->optimize_for_size && !me->optimize_for_speed) {
      me->optimize_for_size = true;
    } else if (me->optimize_for_size && me->optimize_for_speed) {
      Jmsg(nullptr, M_FATAL, 0,
           _("Cannot optimize for speed and size define only one in %s\n"),
           configfile.c_str());
      OK = false;
      goto bail_out;
    }

    if (my_config->GetNextRes(R_DIRECTOR, (BareosResource*)me) != nullptr) {
      Jmsg(nullptr, M_FATAL, 0,
           _("Only one Director resource permitted in %s\n"),
           configfile.c_str());
      OK = false;
      goto bail_out;
    }

    if (me->IsTlsConfigured()) {
      if (!have_tls) {
        Jmsg(nullptr, M_FATAL, 0,
             _("TLS required but not compiled into BAREOS.\n"));
        OK = false;
        goto bail_out;
      }
    }
  }

  if (!job) {
    Jmsg(nullptr, M_FATAL, 0, _("No Job records defined in %s\n"),
         configfile.c_str());
    OK = false;
    goto bail_out;
  }

  if (!PopulateDefs()) {
    OK = false;
    goto bail_out;
  }

  // Loop over Jobs
  foreach_res (job, R_JOB) {
    if (job->MaxFullConsolidations && job->JobType != JT_CONSOLIDATE) {
      Jmsg(nullptr, M_FATAL, 0,
           _("MaxFullConsolidations configured in job %s which is not of job "
             "type \"consolidate\" in file %s\n"),
           job->resource_name_, configfile.c_str());
      OK = false;
      goto bail_out;
    }

    if (job->JobType != JT_BACKUP
        && (job->AlwaysIncremental || job->AlwaysIncrementalJobRetention
            || job->AlwaysIncrementalKeepNumber
            || job->AlwaysIncrementalMaxFullAge)) {
      Jmsg(nullptr, M_FATAL, 0,
           _("AlwaysIncremental configured in job %s which is not of job type "
             "\"backup\" in file %s\n"),
           job->resource_name_, configfile.c_str());
      OK = false;
      goto bail_out;
    }
  }

  ConsoleResource* cons;
  foreach_res (cons, R_CONSOLE) {
    if (cons->IsTlsConfigured()) {
      if (!have_tls) {
        Jmsg(nullptr, M_FATAL, 0,
             _("TLS required but not configured in BAREOS.\n"));
        OK = false;
        goto bail_out;
      }
    }
  }

  ClientResource* client;
  foreach_res (client, R_CLIENT) {
    if (client->IsTlsConfigured()) {
      if (!have_tls) {
        Jmsg(nullptr, M_FATAL, 0, _("TLS required but not configured.\n"));
        OK = false;
        goto bail_out;
      }
    }
  }

  StorageResource *store, *nstore;
  foreach_res (store, R_STORAGE) {
    if (store->IsTlsConfigured()) {
      if (!have_tls) {
        Jmsg(nullptr, M_FATAL, 0, _("TLS required but not configured.\n"));
        OK = false;
        goto bail_out;
      }
    }

    /*
     * If we collect statistics on this SD make sure any other entry pointing to
     * the same SD does not collect statistics otherwise we collect the same
     * data multiple times.
     */
    if (store->collectstats) {
      nstore = store;
      while ((nstore = (StorageResource*)my_config->GetNextRes(
                  R_STORAGE, (BareosResource*)nstore))) {
        if (IsSameStorageDaemon(store, nstore) && nstore->collectstats) {
          nstore->collectstats = false;
          Dmsg1(200,
                _("Disabling collectstats for storage \"%s\""
                  " as other storage already collects from this SD.\n"),
                nstore->resource_name_);
        }
      }
    }
  }

  if (OK) {
    CloseMsg(nullptr);              /* close temp message handler */
    InitMsg(nullptr, me->messages); /* open daemon message handler */
    if (me->secure_erase_cmdline) {
      SetSecureEraseCmdline(me->secure_erase_cmdline);
    }
    if (me->log_timestamp_format) {
      SetLogTimestampFormat(me->log_timestamp_format);
    }
  }

bail_out:
  UnlockRes(my_config);
  return OK;
}

// Initialize the sql pooling.
static bool InitializeSqlPooling(void)
{
  bool retval = true;
  CatalogResource* catalog;

  foreach_res (catalog, R_CATALOG) {
    if (!db_sql_pool_initialize(
            catalog->db_driver, catalog->db_name, catalog->db_user,
            catalog->db_password.value, catalog->db_address, catalog->db_port,
            catalog->db_socket, catalog->disable_batch_insert,
            catalog->try_reconnect, catalog->exit_on_fatal,
            catalog->pooling_min_connections, catalog->pooling_max_connections,
            catalog->pooling_increment_connections,
            catalog->pooling_idle_timeout, catalog->pooling_validate_timeout)) {
      Jmsg(nullptr, M_FATAL, 0,
           _("Could not setup sql pooling for Catalog \"%s\", database "
             "\"%s\".\n"),
           catalog->resource_name_, catalog->db_name);
      retval = false;
      goto bail_out;
    }
  }

bail_out:
  return retval;
}

static JobControlRecord* PrepareJobToRun(const char* job_name)
{
  JobResource* job
      = static_cast<JobResource*>(my_config->GetResWithName(R_JOB, job_name));
  if (!job) {
    Emsg1(M_ERROR, 0, _("Job %s not found\n"), job_name);
    return nullptr;
  }
  Dmsg1(5, "Found job_name %s\n", job_name);
  JobControlRecord* jcr = NewDirectorJcr();
  SetJcrDefaults(jcr, job);
  return jcr;
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
  BErrNo be;

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
    Pmsg2(000, _("Could not compile regex pattern \"%s\" ERR=%s\n"), pat1,
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
