/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2016 Bareos GmbH & Co. KG

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
/*
 * Kern Sibbald, MM
 */
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
#include "stored/stored.h"
#include "lib/crypto_cache.h"
#include "stored/acquire.h"
#include "stored/autochanger.h"
#include "stored/bsr.h"
#include "stored/device.h"
#include "stored/job.h"
#include "stored/label.h"
#include "stored/ndmp_tape.h"
#include "stored/sd_backends.h"
#include "stored/sd_stats.h"
#include "stored/socket_server.h"
#include "stored/stored_globals.h"
#include "stored/wait.h"
#include "lib/berrno.h"
#include "lib/bsock.h"
#include "lib/bnet_network_dump.h"
#include "lib/daemon.h"
#include "lib/bsignal.h"
#include "lib/parse_conf.h"
#include "lib/util.h"
#include "lib/watchdog.h"
#include "include/jcr.h"


namespace storagedaemon {
extern bool ParseSdConfig(const char* configfile, int exit_code);
}

using namespace storagedaemon;

/* Imported functions */
extern void PrintMessage(void* sock, const char* fmt, ...);

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
static bool foreground = 0;

static void usage()
{
  fprintf(
      stderr,
      _(PROG_COPYRIGHT
        "\nVersion: %s (%s)\n\n"
        "Usage: bareos-sd [options]\n"
        "        -c <path>   use <path> as configuration file or directory\n"
        "        -d <nn>     set debug level to <nn>\n"
        "        -dt         print timestamp in debug output\n"
        "        -f          run in foreground (for debugging)\n"
        "        -g <group>  run as group <group>\n"
        "        -m          print kaboom output (for debugging)\n"
        "        -p          proceed despite I/O errors\n"
        "        -s          no signals (for debugging)\n"
        "        -t          test - read configuration and exit\n"
        "        -u <user>   run as user <user>\n"
        "        -v          verbose user messages\n"
        "        -xc         print configuration and exit\n"
        "        -xs         print configuration file schema in JSON format "
        "and exit\n"
        "        -?          print this message.\n"
        "\n"),
      2000, VERSION, BDATE);

  exit(1);
}

/*********************************************************************
 *
 *  Main Bareos Storage Daemon
 *
 */
#if defined(HAVE_WIN32)
#define main BareosMain
#endif

int main(int argc, char* argv[])
{
  int ch;
  bool no_signals = false;
  bool test_config = false;
  bool export_config = false;
  bool export_config_schema = false;
  pthread_t thid;
  char* uid = NULL;
  char* gid = NULL;

  start_heap = sbrk(0);
  setlocale(LC_ALL, "");
  bindtextdomain("bareos", LOCALEDIR);
  textdomain("bareos");

  InitStackDump();
  MyNameIs(argc, argv, "bareos-sd");
  InitMsg(NULL, NULL);
  daemon_start_time = time(NULL);

  /*
   * Sanity checks
   */
  if (TAPE_BSIZE % B_DEV_BSIZE != 0 || TAPE_BSIZE / B_DEV_BSIZE == 0) {
    Emsg2(M_ABORT, 0,
          _("Tape block size (%d) not multiple of system size (%d)\n"),
          TAPE_BSIZE, B_DEV_BSIZE);
  }
  if (TAPE_BSIZE != (1 << (ffs(TAPE_BSIZE) - 1))) {
    Emsg1(M_ABORT, 0, _("Tape block size (%d) is not a power of 2\n"),
          TAPE_BSIZE);
  }

  while ((ch = getopt(argc, argv, "c:d:fg:mpstu:vx:z:?")) != -1) {
    switch (ch) {
      case 'c': /* configuration file */
        if (configfile != NULL) { free(configfile); }
        configfile = strdup(optarg);
        break;

      case 'd': /* debug level */
        if (*optarg == 't') {
          dbg_timestamp = true;
        } else {
          debug_level = atoi(optarg);
          if (debug_level <= 0) { debug_level = 1; }
        }
        break;

      case 'f': /* run in foreground */
        foreground = true;
        break;

      case 'g': /* set group id */
        gid = optarg;
        break;

      case 'm': /* print kaboom output */
        prt_kaboom = true;
        break;

      case 'p': /* proceed in spite of I/O errors */
        forge_on = true;
        break;

      case 's': /* no signals */
        no_signals = true;
        break;

      case 't':
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

  if (!no_signals) { InitSignals(TerminateStored); }

  if (argc) {
    if (configfile != NULL) { free(configfile); }
    configfile = strdup(*argv);
    argc--;
    argv++;
  }
  if (argc) { usage(); }

  /*
   * See if we want to drop privs.
   */
  if (geteuid() == 0) { drop(uid, gid, false); }

  if (export_config_schema) {
    PoolMem buffer;

    my_config = InitSdConfig(configfile, M_ERROR_TERM);
    PrintConfigSchemaJson(buffer);
    printf("%s\n", buffer.c_str());
    goto bail_out;
  }

  my_config = InitSdConfig(configfile, M_ERROR_TERM);
  ParseSdConfig(configfile, M_ERROR_TERM);

  if (export_config) {
    my_config->DumpResources(PrintMessage, NULL);
    goto bail_out;
  }

  if (!foreground && !test_config) {
    daemon_start();  /* become daemon */
    InitStackDump(); /* pick up new pid */
  }

  if (InitCrypto() != 0) {
    Jmsg((JobControlRecord*)NULL, M_ERROR_TERM, 0,
         _("Cryptography library initialization failed.\n"));
  }

  if (!CheckResources()) {
    Jmsg((JobControlRecord*)NULL, M_ERROR_TERM, 0,
         _("Please correct the configuration in %s\n"),
         my_config->get_base_config_path().c_str());
  }

  InitReservationsLock();

  if (test_config) { TerminateStored(0); }

  MyNameIs(0, (char**)NULL, me->resource_name_); /* Set our real name */

  CreatePidFile(me->pid_directory, "bareos-sd",
                GetFirstPortHostOrder(me->SDaddrs));
  ReadStateFile(me->working_directory, "bareos-sd",
                GetFirstPortHostOrder(me->SDaddrs));
  ReadCryptoCache(me->working_directory, "bareos-sd",
                  GetFirstPortHostOrder(me->SDaddrs));

  SetJcrInTsd(INVALID_JCR);

  /*
   * Make sure on Solaris we can run concurrent, watch dog + servers + misc
   */
  SetThreadConcurrency(me->MaxConcurrentJobs * 2 + 4);

  LoadSdPlugins(me->plugin_directory, me->plugin_names);

  CleanUpOldFiles();

  /* Ensure that Volume Session Time and Id are both
   * set and are both non-zero.
   */
  vol_session_time = (uint32_t)daemon_start_time;
  if (vol_session_time == 0) { /* paranoid */
    Jmsg0(NULL, M_ABORT, 0, _("Volume Session Time is ZERO!\n"));
  }

  /*
   * Start the device allocation thread
   */
  CreateVolumeLists(); /* do before device_init */
  if (pthread_create(&thid, NULL, device_initialization, NULL) != 0) {
    BErrNo be;
    Emsg1(M_ABORT, 0, _("Unable to create thread. ERR=%s\n"), be.bstrerror());
  }

  StartWatchdog(); /* start watchdog thread */
  if (me->jcr_watchdog_time) {
    InitJcrSubsystem(
        me->jcr_watchdog_time); /* start JobControlRecord watchdogs etc. */
  }

  StartStatisticsThread();

#if HAVE_NDMP
  /*
   * Separate thread that handles NDMP connections
   */
  if (me->ndmp_enable) {
    StartNdmpThreadServer(me->NDMPaddrs, me->MaxConnections);
  }
#endif

  /*
   * Single server used for Director/Storage and File daemon
   */
  StartSocketServer(me->SDaddrs);

  /* to keep compiler quiet */
  TerminateStored(0);

bail_out:
  return 0;
}

/* Check Configuration file for necessary info */
static int CheckResources()
{
  bool OK = true;
  const std::string& configfile = my_config->get_base_config_path();

  if (my_config->GetNextRes(R_STORAGE, (BareosResource*)me) != NULL) {
    Jmsg1(NULL, M_ERROR, 0, _("Only one Storage resource permitted in %s\n"),
          configfile.c_str());
    OK = false;
  }

  if (my_config->GetNextRes(R_DIRECTOR, NULL) == NULL) {
    Jmsg1(NULL, M_ERROR, 0,
          _("No Director resource defined in %s. Cannot continue.\n"),
          configfile.c_str());
    OK = false;
  }

  if (my_config->GetNextRes(R_DEVICE, NULL) == NULL) {
    Jmsg1(NULL, M_ERROR, 0,
          _("No Device resource defined in %s. Cannot continue.\n"),
          configfile.c_str());
    OK = false;
  }

  /*
   * Sanity check.
   */
  if (me->MaxConnections < ((2 * me->MaxConcurrentJobs) + 2)) {
    me->MaxConnections = (2 * me->MaxConcurrentJobs) + 2;
  }

  if (!me->messages) {
    me->messages = (MessagesResource*)my_config->GetNextRes(R_MSGS, NULL);
    if (!me->messages) {
      Jmsg1(NULL, M_ERROR, 0,
            _("No Messages resource defined in %s. Cannot continue.\n"),
            configfile.c_str());
      OK = false;
    }
  }

  if (!me->working_directory) {
    Jmsg1(NULL, M_ERROR, 0,
          _("No Working Directory defined in %s. Cannot continue.\n"),
          configfile.c_str());
    OK = false;
  }

  StorageResource* store = me;
  if (store->IsTlsConfigured()) {
    if (!have_tls) {
      Jmsg(NULL, M_FATAL, 0, _("TLS required but not compiled into Bareos.\n"));
      OK = false;
    }
  }

  DeviceResource* device;
  foreach_res (device, R_DEVICE) {
    if (device->drive_crypto_enabled && BitIsSet(CAP_LABEL, device->cap_bits)) {
      Jmsg(NULL, M_FATAL, 0,
           _("LabelMedia enabled is incompatible with tape crypto on Device "
             "\"%s\" in %s.\n"),
           device->resource_name_, configfile.c_str());
      OK = false;
    }
  }

  if (OK) { OK = InitAutochangers(); }

  if (OK) {
    CloseMsg(NULL);              /* close temp message handler */
    InitMsg(NULL, me->messages); /* open daemon message handler */
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

/**
 * Remove old .spool files written by me from the working directory.
 */
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
  regex_t preg1;
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
  }

#ifdef USE_READDIR_R
  entry = (struct dirent*)malloc(sizeof(struct dirent) + name_max + 1000);
  while (1) {
    if ((Readdir_r(dp, entry, &result) != 0) || (result == NULL)) {
#else
  while (1) {
    result = readdir(dp);
    if (result == NULL) {
#endif
      break;
    }

    /* Exclude any name with ., .., not my_name or containing a space */
    if (strcmp(result->d_name, ".") == 0 || strcmp(result->d_name, "..") == 0 ||
        strncmp(result->d_name, my_name, my_name_len) != 0) {
      Dmsg1(500, "Skipped: %s\n", result->d_name);
      continue;
    }

    /* Unlink files that match regex */
    if (regexec(&preg1, result->d_name, 0, NULL, 0) == 0) {
      PmStrcpy(cleanup, basename);
      PmStrcat(cleanup, result->d_name);
      Dmsg1(500, "Unlink: %s\n", cleanup);
      SecureErase(NULL, cleanup);
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
extern "C" void* device_initialization(void* arg)
{
  DeviceResource* device;
  DeviceControlRecord* dcr;
  JobControlRecord* jcr;
  Device* dev;
  int errstat;

  LockRes(my_config);

  pthread_detach(pthread_self());
  jcr = new_jcr(sizeof(JobControlRecord), StoredFreeJcr);
  NewPlugins(jcr); /* instantiate plugins */
  jcr->setJobType(JT_SYSTEM);

  /*
   * Initialize job start condition variable
   */
  errstat = pthread_cond_init(&jcr->job_start_wait, NULL);
  if (errstat != 0) {
    BErrNo be;
    Jmsg1(jcr, M_ABORT, 0,
          _("Unable to init job start cond variable: ERR=%s\n"),
          be.bstrerror(errstat));
  }

  /*
   * Initialize job end condition variable
   */
  errstat = pthread_cond_init(&jcr->job_end_wait, NULL);
  if (errstat != 0) {
    BErrNo be;
    Jmsg1(jcr, M_ABORT, 0,
          _("Unable to init job endstart cond variable: ERR=%s\n"),
          be.bstrerror(errstat));
  }

  foreach_res (device, R_DEVICE) {
    Dmsg1(90, "calling InitDev %s\n", device->device_name);
    dev = InitDev(NULL, device);
    Dmsg1(10, "SD init done %s\n", device->device_name);
    if (!dev) {
      Jmsg1(NULL, M_ERROR, 0, _("Could not initialize %s\n"),
            device->device_name);
      continue;
    }

    dcr = new StorageDaemonDeviceControlRecord;
    jcr->dcr = dcr;
    SetupNewDcrDevice(jcr, dcr, dev, NULL);
    jcr->dcr->SetWillWrite();
    GeneratePluginEvent(jcr, bsdEventDeviceInit, dcr);
    if (dev->IsAutochanger()) {
      /*
       * If autochanger set slot in dev structure
       */
      GetAutochangerLoadedSlot(dcr);
    }

    if (BitIsSet(CAP_ALWAYSOPEN, device->cap_bits)) {
      Dmsg1(20, "calling FirstOpenDevice %s\n", dev->print_name());
      if (!FirstOpenDevice(dcr)) {
        Jmsg1(NULL, M_ERROR, 0, _("Could not open device %s\n"),
              dev->print_name());
        Dmsg1(20, "Could not open device %s\n", dev->print_name());
        FreeDeviceControlRecord(dcr);
        jcr->dcr = NULL;
        continue;
      }
    }

    if (BitIsSet(CAP_AUTOMOUNT, device->cap_bits) && dev->IsOpen()) {
      switch (ReadDevVolumeLabel(dcr)) {
        case VOL_OK:
          memcpy(&dev->VolCatInfo, &dcr->VolCatInfo, sizeof(dev->VolCatInfo));
          VolumeUnused(dcr); /* mark volume "released" */
          break;
        default:
          Jmsg1(NULL, M_WARNING, 0, _("Could not mount device %s\n"),
                dev->print_name());
          break;
      }
    }
    FreeDeviceControlRecord(dcr);
    jcr->dcr = NULL;
  }
  FreeJcr(jcr);
  init_done = true;
  UnlockRes(my_config);
  return NULL;
}

/**
 * Clean up and then exit
 */
namespace storagedaemon {

#if !defined(HAVE_WIN32)
static
#endif
    void
    TerminateStored(int sig)
{
  static bool in_here = false;
  DeviceResource* device;
  JobControlRecord* jcr;

  if (in_here) {       /* prevent loops */
    Bmicrosleep(2, 0); /* yield */
    exit(1);
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
    /*
     * This is a normal shutdown request. We wiffle through
     *   all open jobs canceling them and trying to wake
     *   them up so that they will report back the correct
     *   volume status.
     */
    foreach_jcr (jcr) {
      BareosSocket* fd;
      if (jcr->JobId == 0) { continue; /* ignore console */ }
      jcr->setJobStatus(JS_Canceled);
      fd = jcr->file_bsock;
      if (fd) {
        fd->SetTimedOut();
        jcr->MyThreadSendSignal(TIMEOUT_SIGNAL);
        Dmsg1(100, "term_stored killing JobId=%d\n", jcr->JobId);
        /* ***FIXME*** wiffle through all dcrs */
        if (jcr->dcr && jcr->dcr->dev && jcr->dcr->dev->blocked()) {
          pthread_cond_broadcast(&jcr->dcr->dev->wait_next_vol);
          Dmsg1(100, "JobId=%u broadcast wait_device_release\n",
                (uint32_t)jcr->JobId);
          ReleaseDeviceCond();
        }
        if (jcr->read_dcr && jcr->read_dcr->dev &&
            jcr->read_dcr->dev->blocked()) {
          pthread_cond_broadcast(&jcr->read_dcr->dev->wait_next_vol);
          Dmsg1(100, "JobId=%u broadcast wait_device_release\n",
                (uint32_t)jcr->JobId);
          ReleaseDeviceCond();
        }
        Bmicrosleep(0, 50000);
      }
      FreeJcr(jcr);
    }
    Bmicrosleep(0, 500000); /* give them 1/2 sec to clean up */
  }

  WriteStateFile(me->working_directory, "bareos-sd",
                 GetFirstPortHostOrder(me->SDaddrs));
  DeletePidFile(me->pid_directory, "bareos-sd",
                GetFirstPortHostOrder(me->SDaddrs));

  Dmsg1(200, "In TerminateStored() sig=%d\n", sig);

  UnloadSdPlugins();
  FlushCryptoCache();
  FreeVolumeLists();

  foreach_res (device, R_DEVICE) {
    Dmsg1(10, "Term device %s\n", device->device_name);
    if (device->dev) {
      device->dev->ClearVolhdr();
      device->dev->term();
      device->dev = NULL;
    } else {
      Dmsg1(10, "No dev structure %s\n", device->device_name);
    }
  }

#if defined(HAVE_DYNAMIC_SD_BACKENDS)
  DevFlushBackends();
#endif

  if (configfile) {
    free(configfile);
    configfile = NULL;
  }
  if (my_config) {
    delete my_config;
    my_config = NULL;
  }

  if (debug_level > 10) { PrintMemoryPoolStats(); }
  TermMsg();
  CleanupCrypto();
  TermReservationsLock();
  CloseMemoryPool();

  exit(sig);
}

} /* namespace storagedaemon */
