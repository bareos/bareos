/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2025 Bareos GmbH & Co. KG

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
 * Second generation Storage daemon.
 *
 * It accepts a number of simple commands from the File daemon
 * and acts on them. When a request to append data is made,
 * it opens a data channel and accepts data from the
 * File daemon.
 */

#include "include/bareos.h"
#include "include/exit_codes.h"
#include "stored/stored.h"
#include "lib/crypto_cache.h"
#include "stored/acquire.h"
#include "stored/autochanger.h"
#include "stored/bsr.h"
#include "stored/device.h"
#include "stored/stored_jcr_impl.h"
#include "stored/job.h"
#include "stored/label.h"
#include "stored/ndmp_tape.h"
#include "stored/sd_backends.h"
#include "stored/sd_device_control_record.h"
#include "stored/sd_stats.h"
#include "stored/socket_server.h"
#include "stored/stored_globals.h"
#include "stored/wait.h"
#include "lib/berrno.h"
#include "lib/bsock.h"
#include "lib/bnet_network_dump.h"
#include "lib/cli.h"
#include "lib/daemon.h"
#include "lib/bsignal.h"
#include "lib/parse_conf.h"
#include "lib/thread_specific_data.h"
#include "lib/util.h"
#include "lib/watchdog.h"
#include "include/jcr.h"
#if !defined(HAVE_WIN32)
#  include "lib/priv.h"
#endif

namespace storagedaemon {
extern bool ParseSdConfig(const char* configfile, int exit_code);
}

using namespace storagedaemon;

/* Forward referenced functions */
namespace storagedaemon {
#if !defined(HAVE_WIN32)
static
#endif
    void
    TerminateStored(int sig);
}  // namespace storagedaemon
static int CheckResources();
static void CleanUpOldFiles();

extern "C" void* device_initialization(void* arg);

/* Global static variables */
static std::string pidfile_path;


/*********************************************************************
 *
 *  Main Bareos Storage Daemon
 *
 */
#if defined(HAVE_WIN32)
#  define main BareosMain
#endif

int main(int argc, char* argv[])
{
  pthread_t thid;

  setlocale(LC_ALL, "");
  tzset();
  bindtextdomain("bareos", LOCALEDIR);
  textdomain("bareos");

  InitStackDump();
  MyNameIs(argc, argv, "bareos-sd");
  InitMsg(nullptr, nullptr);
  daemon_start_time = time(nullptr);

  // Sanity checks
  if (TAPE_BSIZE % B_DEV_BSIZE != 0 || TAPE_BSIZE / B_DEV_BSIZE == 0) {
    Emsg2(M_ABORT, 0,
          T_("Tape block size (%d) not multiple of system size (%d)\n"),
          TAPE_BSIZE, B_DEV_BSIZE);
  }
  if (TAPE_BSIZE != (1 << (ffs(TAPE_BSIZE) - 1))) {
    Emsg1(M_ABORT, 0, T_("Tape block size (%d) is not a power of 2\n"),
          TAPE_BSIZE);
  }

  CLI::App sd_app;
  InitCLIApp(sd_app, "The Bareos Storage Daemon.", 2000);

  sd_app
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

  AddDebugOptions(sd_app);

  bool foreground = false;
  [[maybe_unused]] CLI::Option* foreground_option = sd_app.add_flag(
      "-f,--foreground", foreground, "Run in foreground (for debugging).");

  std::string user{};
  std::string group{};
  AddUserAndGroupOptions(sd_app, user, group);


  sd_app.add_flag("-m,--print-kaboom", prt_kaboom,
                  "Print kaboom output (for debugging).");

  sd_app.add_flag("-i,--ignore-io-errors", forge_on, "Ignore IO errors.");

  bool test_config = false;
  [[maybe_unused]] auto testconfig_option = sd_app.add_flag(
      "-t,--test-config", test_config, "Test - read configuration and exit.");

#ifndef HAVE_WIN32
  sd_app
      .add_option("-p,--pid-file", pidfile_path,
                  "Full path to pidfile (default: none)")
      ->excludes(foreground_option)
      ->excludes(testconfig_option)
      ->type_name("<file>");
#endif

  bool no_signals = false;
  sd_app.add_flag("-s,--no-signals", no_signals, "No signals (for debugging).");

  AddVerboseOption(sd_app);

  bool export_config = false;
  CLI::Option* xc
      = sd_app.add_flag("--xc,--export-config", export_config,
                        "Print all configuration resources and exit.");


  bool export_config_schema = false;
  sd_app
      .add_flag("--xs,--export-schema", export_config_schema,
                "Print configuration schema in JSON format and exit.")
      ->excludes(xc);

  AddDeprecatedExportOptionsHelp(sd_app);

  ParseBareosApp(sd_app, argc, argv);

  if (!no_signals) { InitSignals(TerminateStored); }

  int pidfile_fd = -1;
#if !defined(HAVE_WIN32)
  if (!foreground && !test_config && !pidfile_path.empty()) {
    pidfile_fd = CreatePidFile("bareos-sd", pidfile_path.c_str());
  }

  // See if we want to drop privs.
  char* uid = nullptr;
  if (!user.empty()) { uid = user.data(); }

  char* gid = nullptr;
  if (!group.empty()) { gid = group.data(); }

  if (geteuid() == 0) {
    drop(uid, gid, false);
  } else if (uid || gid) {
    Emsg2(M_ERROR_TERM, 0,
          T_("The commandline options indicate to run as specified user/group, "
             "but program was not started with required root privileges.\n"));
  }
#endif

  if (export_config_schema) {
    PoolMem buffer;

    my_config = InitSdConfig(configfile, M_CONFIG_ERROR);
    PrintConfigSchemaJson(buffer);
    printf("%s\n", buffer.c_str());

    return BEXIT_SUCCESS;
  }

  my_config = InitSdConfig(configfile, M_CONFIG_ERROR);
  ParseSdConfig(configfile, M_CONFIG_ERROR);

  if (forge_on) {
    my_config->AddWarning(
        "Running with '-i' is for testing and emergency recovery purposes "
        "only");
  }

  if (export_config) {
    my_config->DumpResources(PrintMessage, nullptr);

    return BEXIT_SUCCESS;
  }

  if (!CheckResources()) {
    Jmsg((JobControlRecord*)NULL, M_ERROR_TERM, 0,
         T_("Please correct the configuration in %s\n"),
         my_config->get_base_config_path().c_str());
  }

  if (my_config->HasWarnings()) {
    // messaging not initialized, so Jmsg with  M_WARNING doesn't work
    fprintf(stderr, T_("There are configuration warnings:\n"));
    for (auto& warning : my_config->GetWarnings()) {
      fprintf(stderr, " * %s\n", warning.c_str());
    }
  }

  if (!foreground && !test_config) {
    daemon_start("bareos-sd", pidfile_fd, pidfile_path); /* become daemon */
    InitStackDump();                                     /* pick up new pid */
  }

  if (InitCrypto() != 0) {
    Jmsg((JobControlRecord*)nullptr, M_ERROR_TERM, 0,
         T_("Cryptography library initialization failed.\n"));
  }

  InitReservationsLock();

  if (test_config) { TerminateStored(0); }

  MyNameIs(0, (char**)nullptr, me->resource_name_); /* Set our real name */

  ReadStateFile(me->working_directory, "bareos-sd",
                GetFirstPortHostOrder(me->SDaddrs));
  ReadCryptoCache(me->working_directory, "bareos-sd",
                  GetFirstPortHostOrder(me->SDaddrs));

  SetJcrInThreadSpecificData(nullptr);

  LoadSdPlugins(me->plugin_directory, me->plugin_names);

  CleanUpOldFiles();

  /* Ensure that Volume Session Time and Id are both
   * set and are both non-zero.
   */
  vol_session_time = (uint32_t)daemon_start_time;
  if (vol_session_time == 0) { /* paranoid */
    Jmsg0(nullptr, M_ABORT, 0, T_("Volume Session Time is ZERO!\n"));
  }

  // Start the device allocation thread
  CreateVolumeLists(); /* do before device_init */
  if (pthread_create(&thid, nullptr, device_initialization, nullptr) != 0) {
    BErrNo be;
    Emsg1(M_ABORT, 0, T_("Unable to create thread. ERR=%s\n"), be.bstrerror());
  }

  LockJcrChain();
  // prevent race condition with device_initialisation
  InitJcrChain();
  UnlockJcrChain();
  StartWatchdog(); /* start watchdog thread */
  if (me->jcr_watchdog_time) {
    InitJcrSubsystem(
        me->jcr_watchdog_time); /* start JobControlRecord watchdogs etc. */
  }

  StartStatisticsThread();

#if HAVE_NDMP
  // Separate thread that handles NDMP connections
  if (me->ndmp_enable) { StartNdmpThreadServer(me->NDMPaddrs); }
#endif

  // Single server used for Director/Storage and File daemon
  StartSocketServer(me->SDaddrs);

  /* to keep compiler quiet */
  TerminateStored(BEXIT_SUCCESS);

  return BEXIT_SUCCESS;
}

/* Check Configuration file for necessary info */
static int CheckResources()
{
  bool OK = true;
  const std::string& configfile_name = my_config->get_base_config_path();

  if (my_config->GetNextRes(R_STORAGE, (BareosResource*)me) != nullptr) {
    Jmsg1(nullptr, M_ERROR, 0,
          T_("Only one Storage resource permitted in %s\n"),
          configfile_name.c_str());
    OK = false;
  }

  if (my_config->GetNextRes(R_DIRECTOR, nullptr) == nullptr) {
    Jmsg1(nullptr, M_ERROR, 0,
          T_("No Director resource defined in %s. Cannot continue.\n"),
          configfile_name.c_str());
    OK = false;
  }

  if (my_config->GetNextRes(R_DEVICE, nullptr) == nullptr) {
    Jmsg1(nullptr, M_ERROR, 0,
          T_("No Device resource defined in %s. Cannot continue.\n"),
          configfile_name.c_str());
    OK = false;
  }

  if (!me->messages) {
    me->messages = (MessagesResource*)my_config->GetNextRes(R_MSGS, nullptr);
    if (!me->messages) {
      Jmsg1(nullptr, M_ERROR, 0,
            T_("No Messages resource defined in %s. Cannot continue.\n"),
            configfile_name.c_str());
      OK = false;
    }
  }

  if (!me->working_directory) {
    Jmsg1(nullptr, M_ERROR, 0,
          T_("No Working Directory defined in %s. Cannot continue.\n"),
          configfile_name.c_str());
    OK = false;
  }

  DeviceResource* device_resource = nullptr;
  foreach_res (device_resource, R_DEVICE) {
    if (device_resource->drive_crypto_enabled
        && BitIsSet(CAP_LABEL, device_resource->cap_bits)) {
      Jmsg(nullptr, M_FATAL, 0,
           T_("LabelMedia enabled is incompatible with tape crypto on Device "
              "\"%s\" in %s.\n"),
           device_resource->resource_name_, configfile_name.c_str());
      OK = false;
    }
  }

  if (OK) { OK = InitAutochangers(); }

  if (OK) {
    CloseMsg(nullptr);              /* close temp message handler */
    InitMsg(nullptr, me->messages); /* open daemon message handler */
    SetWorkingDirectory(me->working_directory);
    if (me->secure_erase_cmdline) {
      SetSecureEraseCmdline(me->secure_erase_cmdline);
    }
    if (me->log_timestamp_format) {
      SetLogTimestampFormat(me->log_timestamp_format);
    }
  }

  return OK;
}

// Remove old .spool files written by me from the working directory.
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

  /* Look for .spool files but don't allow spaces */
  const char* pat1 = "^[^ ]+\\.spool$";

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
    BErrNo be2;
    Pmsg2(000, "Failed to open working dir %s for cleanup: ERR=%s\n",
          me->working_directory, be2.bstrerror());
    goto get_out1;
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

    /* Unlink files that match regex */
    if (regexec(&preg1, result->d_name, 0, nullptr, 0) == 0) {
      PmStrcpy(cleanup, basename);
      PmStrcat(cleanup, result->d_name);
      Dmsg1(500, "Unlink: %s\n", cleanup);
      SecureErase(nullptr, cleanup);
    }
  }
#ifdef USE_READDIR_R
  free(entry);
#endif
  closedir(dp);

get_out1:
  regfree(&preg1);
get_out2:
  FreePoolMemory(cleanup);
  FreePoolMemory(basename);
}


/**
 * Here we attempt to init and open each device. This is done once at startup in
 * a separate thread.
 */
extern "C" void* device_initialization(void*)
{
  DeviceResource* device_resource = nullptr;
  DeviceControlRecord* dcr;
  JobControlRecord* jcr;
  Device* dev;
  int errstat;

  ResLocker _{my_config};

  pthread_detach(pthread_self());
  jcr = NewStoredJcr();
  NewPlugins(jcr); /* instantiate plugins */
  jcr->setJobType(JT_SYSTEM);

  // Initialize job end condition variable
  errstat = pthread_cond_init(&jcr->sd_impl->job_end_wait, nullptr);
  if (errstat != 0) {
    BErrNo be;
    Jmsg1(jcr, M_ABORT, 0,
          T_("Unable to init job endstart cond variable: ERR=%s\n"),
          be.bstrerror(errstat));
  }

  foreach_res (device_resource, R_DEVICE) {
    Dmsg1(90, "calling FactoryCreateDevice %s\n",
          device_resource->archive_device_string);
    dev = FactoryCreateDevice(nullptr, device_resource);
    Dmsg1(10, "SD init done %s\n", device_resource->archive_device_string);
    if (!dev) {
      Jmsg1(nullptr, M_ERROR, 0, T_("Could not initialize %s\n"),
            device_resource->archive_device_string);
      continue;
    }

    dcr = new StorageDaemonDeviceControlRecord;
    jcr->sd_impl->dcr = dcr;
    SetupNewDcrDevice(jcr, dcr, dev, nullptr);
    jcr->sd_impl->dcr->SetWillWrite();
    GeneratePluginEvent(jcr, bSdEventDeviceInit, dcr);
    if (dev->AttachedToAutochanger()) {
      // If autochanger set slot in dev structure
      GetAutochangerLoadedSlot(dcr);
    }

    if (BitIsSet(CAP_ALWAYSOPEN, device_resource->cap_bits)) {
      Dmsg1(20, "calling FirstOpenDevice %s\n", dev->print_name());
      if (!FirstOpenDevice(dcr)) {
        Jmsg1(nullptr, M_ERROR, 0, T_("Could not open device %s\n"),
              dev->print_name());
        Dmsg1(20, "Could not open device %s\n", dev->print_name());
        FreeDeviceControlRecord(dcr);
        jcr->sd_impl->dcr = nullptr;
        continue;
      }
    }

    if (BitIsSet(CAP_AUTOMOUNT, device_resource->cap_bits) && dev->IsOpen()) {
      switch (ReadDevVolumeLabel(dcr)) {
        case VOL_OK:
          memcpy(&dev->VolCatInfo, &dcr->VolCatInfo, sizeof(dev->VolCatInfo));
          VolumeUnused(dcr); /* mark volume "released" */
          break;
        default:
          Jmsg1(nullptr, M_WARNING, 0, T_("Could not mount device %s\n"),
                dev->print_name());
          break;
      }
    }
    FreeDeviceControlRecord(dcr);
    jcr->sd_impl->dcr = nullptr;
  }
  FreeJcr(jcr);
  init_done = true;
  return nullptr;
}

// Clean up and then exit
namespace storagedaemon {

#if !defined(HAVE_WIN32)
static
#endif
    void
    TerminateStored(int sig)
{
  static bool in_here = false;
  DeviceResource* device_resource = nullptr;
  JobControlRecord* jcr;

  if (in_here) {       /* prevent loops */
    Bmicrosleep(2, 0); /* yield */
    exit(BEXIT_FAILURE);
  }
  in_here = true;
  debug_level = 0; /* turn off any debug */
  StopStatisticsThread();
#if HAVE_NDMP
  if (me->ndmp_enable) { StopNdmpThreadServer(); }
#endif
  StopSocketServer();

  StopWatchdog();

  if (sig == SIGTERM) { /* normal shutdown request? */
    /* This is a normal shutdown request. We wiffle through
     *   all open jobs canceling them and trying to wake
     *   them up so that they will report back the correct
     *   volume status. */
    foreach_jcr (jcr) {
      BareosSocket* fd;
      if (jcr->JobId == 0) { continue; /* ignore console */ }
      jcr->setJobStatusWithPriorityCheck(JS_Canceled);
      fd = jcr->file_bsock;
      if (fd) {
        fd->SetTimedOut();
        jcr->MyThreadSendSignal(kTimeoutSignal);
        Dmsg1(100, "term_stored killing JobId=%d\n", jcr->JobId);
        /* ***FIXME*** wiffle through all dcrs */
        if (jcr->sd_impl->dcr && jcr->sd_impl->dcr->dev
            && jcr->sd_impl->dcr->dev->blocked()) {
          pthread_cond_broadcast(&jcr->sd_impl->dcr->dev->wait_next_vol);
          Dmsg1(100, "JobId=%u broadcast wait_device_release\n",
                (uint32_t)jcr->JobId);
          ReleaseDeviceCond();
        }
        if (jcr->sd_impl->read_dcr && jcr->sd_impl->read_dcr->dev
            && jcr->sd_impl->read_dcr->dev->blocked()) {
          pthread_cond_broadcast(&jcr->sd_impl->read_dcr->dev->wait_next_vol);
          Dmsg1(100, "JobId=%u broadcast wait_device_release\n",
                (uint32_t)jcr->JobId);
          ReleaseDeviceCond();
        }
        Bmicrosleep(0, 50000);
      }
      FreeJcr(jcr);
    }
    endeach_jcr(jcr);
    Bmicrosleep(0, 500000); /* give them 1/2 sec to clean up */
  }

  WriteStateFile(me->working_directory, "bareos-sd",
                 GetFirstPortHostOrder(me->SDaddrs));
  DeletePidFile(pidfile_path);

  Dmsg1(200, "In TerminateStored() sig=%d\n", sig);

  UnloadSdPlugins();
  FlushCryptoCache();
  FreeVolumeLists();

  foreach_res (device_resource, R_DEVICE) {
    Dmsg1(10, "Term device %s\n", device_resource->archive_device_string);
    if (device_resource->dev) {
      device_resource->dev->ClearVolhdr();
      delete device_resource->dev;
      device_resource->dev = nullptr;
    } else {
      Dmsg1(10, "No dev structure %s\n",
            device_resource->archive_device_string);
    }
  }

  if (configfile) {
    free(configfile);
    configfile = nullptr;
  }
  if (my_config) {
    delete my_config;
    my_config = nullptr;
  }

  TermMsg();
  CleanupCrypto();
  TermReservationsLock();

  exit(sig);
}

} /* namespace storagedaemon */
