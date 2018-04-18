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

#include "bareos.h"
#include "stored.h"
#include "lib/crypto_cache.h"
#include "stored/acquire.h"
#include "stored/autochanger.h"
#include "stored/device.h"
#include "stored/job.h"
#include "stored/label.h"
#include "stored/ndmp_tape.h"
#include "stored/sd_backends.h"
#include "stored/sd_stats.h"
#include "stored/socket_server.h"
#include "stored/wait.h"

/* Imported functions */
extern bool parse_sd_config(ConfigurationParser *config, const char *configfile, int exit_code);
extern void prtmsg(void *sock, const char *fmt, ...);

/* Forward referenced functions */
#if !defined(HAVE_WIN32)
static
#endif
void terminate_stored(int sig);
static int check_resources();
static void cleanup_old_files();

extern "C" void *device_initialization(void *arg);

/* Global variables exported */
char OK_msg[]   = "3000 OK\n";
char TERM_msg[] = "3999 Terminate\n";

void *start_heap;

static uint32_t VolSessionId = 0;
uint32_t VolSessionTime;
bool init_done = false;

/* Global static variables */
static bool foreground = 0;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

static void usage()
{
   fprintf(stderr, _(
PROG_COPYRIGHT
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
"        -xs         print configuration file schema in JSON format and exit\n"
"        -?          print this message.\n"
"\n"), 2000, VERSION, BDATE);

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

int main (int argc, char *argv[])
{
   int ch;
   bool no_signals = false;
   bool test_config = false;
   bool export_config = false;
   bool export_config_schema = false;
   pthread_t thid;
   char *uid = NULL;
   char *gid = NULL;

   start_heap = sbrk(0);
   setlocale(LC_ALL, "");
   bindtextdomain("bareos", LOCALEDIR);
   textdomain("bareos");

   init_stack_dump();
   my_name_is(argc, argv, "bareos-sd");
   init_msg(NULL, NULL);
   daemon_start_time = time(NULL);

   /*
    * Sanity checks
    */
   if (TAPE_BSIZE % B_DEV_BSIZE != 0 || TAPE_BSIZE / B_DEV_BSIZE == 0) {
      Emsg2(M_ABORT, 0, _("Tape block size (%d) not multiple of system size (%d)\n"),
         TAPE_BSIZE, B_DEV_BSIZE);
   }
   if (TAPE_BSIZE != (1 << (ffs(TAPE_BSIZE)-1))) {
      Emsg1(M_ABORT, 0, _("Tape block size (%d) is not a power of 2\n"), TAPE_BSIZE);
   }

   while ((ch = getopt(argc, argv, "c:d:fg:mpstu:vx:?")) != -1) {
      switch (ch) {
      case 'c':                    /* configuration file */
         if (configfile != NULL) {
            free(configfile);
         }
         configfile = bstrdup(optarg);
         break;

      case 'd':                    /* debug level */
         if (*optarg == 't') {
            dbg_timestamp = true;
         } else {
            debug_level = atoi(optarg);
            if (debug_level <= 0) {
               debug_level = 1;
            }
         }
         break;

      case 'f':                    /* run in foreground */
         foreground = true;
         break;

      case 'g':                    /* set group id */
         gid = optarg;
         break;

      case 'm':                    /* print kaboom output */
         prt_kaboom = true;
         break;

      case 'p':                    /* proceed in spite of I/O errors */
         forge_on = true;
         break;

      case 's':                    /* no signals */
         no_signals = true;
         break;

      case 't':
         test_config = true;
         break;

      case 'u':                    /* set uid */
         uid = optarg;
         break;

      case 'v':                    /* verbose */
         verbose++;
         break;

      case 'x':                    /* export configuration/schema and exit */
         if (*optarg == 's') {
            export_config_schema = true;
         } else if (*optarg == 'c') {
            export_config = true;
         } else {
            usage();
         }
         break;

      case '?':
      default:
         usage();
         break;
      }
   }
   argc -= optind;
   argv += optind;

   if (!no_signals) {
      init_signals(terminate_stored);
   }

   if (argc) {
      if (configfile != NULL) {
         free(configfile);
      }
      configfile = bstrdup(*argv);
      argc--;
      argv++;
   }
   if (argc) {
      usage();
   }

   /*
    * See if we want to drop privs.
    */
   if (geteuid() == 0) {
      drop(uid, gid, false);
   }

  if (export_config_schema) {
      PoolMem buffer;

      my_config = new_config_parser();
      init_sd_config(my_config, configfile, M_ERROR_TERM);
      print_config_schema_json(buffer);
      printf("%s\n", buffer.c_str());
      goto bail_out;
   }

   my_config = new_config_parser();
   parse_sd_config(my_config, configfile, M_ERROR_TERM);

   if (export_config) {
      my_config->dump_resources(prtmsg, NULL);
      goto bail_out;
   }

   if (!foreground && !test_config) {
      daemon_start();                 /* become daemon */
      init_stack_dump();              /* pick up new pid */
   }

   if (init_crypto() != 0) {
      Jmsg((JobControlRecord *)NULL, M_ERROR_TERM, 0, _("Cryptography library initialization failed.\n"));
   }

   if (!check_resources()) {
      Jmsg((JobControlRecord *)NULL, M_ERROR_TERM, 0, _("Please correct the configuration in %s\n"), my_config->get_base_config_path());
   }

   init_reservations_lock();

   if (test_config) {
      terminate_stored(0);
   }

   my_name_is(0, (char **)NULL, me->name());     /* Set our real name */

   create_pid_file(me->pid_directory, "bareos-sd",
                   get_first_port_host_order(me->SDaddrs));
   read_state_file(me->working_directory, "bareos-sd",
                   get_first_port_host_order(me->SDaddrs));
   read_crypto_cache(me->working_directory, "bareos-sd",
                     get_first_port_host_order(me->SDaddrs));

   set_jcr_in_tsd(INVALID_JCR);

   /*
    * Make sure on Solaris we can run concurrent, watch dog + servers + misc
    */
   set_thread_concurrency(me->MaxConcurrentJobs * 2 + 4);
   lmgr_init_thread(); /* initialize the lockmanager stack */

   load_sd_plugins(me->plugin_directory, me->plugin_names);

   cleanup_old_files();

   /* Ensure that Volume Session Time and Id are both
    * set and are both non-zero.
    */
   VolSessionTime = (uint32_t)daemon_start_time;
   if (VolSessionTime == 0) { /* paranoid */
      Jmsg0(NULL, M_ABORT, 0, _("Volume Session Time is ZERO!\n"));
   }

   /*
    * Start the device allocation thread
    */
   create_volume_lists();             /* do before device_init */
   if (pthread_create(&thid, NULL, device_initialization, NULL) != 0) {
      berrno be;
      Emsg1(M_ABORT, 0, _("Unable to create thread. ERR=%s\n"), be.bstrerror());
   }

   start_watchdog();                  /* start watchdog thread */
   if (me->jcr_watchdog_time) {
      init_jcr_subsystem(me->jcr_watchdog_time); /* start JobControlRecord watchdogs etc. */
   }

   start_statistics_thread();

#if HAVE_NDMP
   /*
    * Separate thread that handles NDMP connections
    */
   if (me->ndmp_enable) {
      start_ndmp_thread_server(me->NDMPaddrs, me->MaxConnections);
   }
#endif

   /*
    * Single server used for Director/Storage and File daemon
    */
   start_socket_server(me->SDaddrs);

   /* to keep compiler quiet */
   terminate_stored(0);

bail_out:
   return 0;
}

/* Return a new Session Id */
uint32_t newVolSessionId()
{
   uint32_t Id;

   P(mutex);
   VolSessionId++;
   Id = VolSessionId;
   V(mutex);
   return Id;
}

/* Check Configuration file for necessary info */
static int check_resources()
{
   bool OK = true;
   bool tls_needed;
   const char *configfile = my_config->get_base_config_path();

   if (GetNextRes(R_STORAGE, (CommonResourceHeader *)me) != NULL) {
      Jmsg1(NULL, M_ERROR, 0, _("Only one Storage resource permitted in %s\n"),
         configfile);
      OK = false;
   }

   if (GetNextRes(R_DIRECTOR, NULL) == NULL) {
      Jmsg1(NULL, M_ERROR, 0, _("No Director resource defined in %s. Cannot continue.\n"),
         configfile);
      OK = false;
   }

   if (GetNextRes(R_DEVICE, NULL) == NULL){
      Jmsg1(NULL, M_ERROR, 0, _("No Device resource defined in %s. Cannot continue.\n"),
           configfile);
      OK = false;
   }

   /*
    * Sanity check.
    */
   if (me->MaxConnections < ((2 * me->MaxConcurrentJobs) + 2)) {
      me->MaxConnections = (2 * me->MaxConcurrentJobs) + 2;
   }

   if (!me->messages) {
      me->messages = (MessagesResource *)GetNextRes(R_MSGS, NULL);
      if (!me->messages) {
         Jmsg1(NULL, M_ERROR, 0, _("No Messages resource defined in %s. Cannot continue.\n"),
            configfile);
         OK = false;
      }
   }

   if (!me->working_directory) {
      Jmsg1(NULL, M_ERROR, 0, _("No Working Directory defined in %s. Cannot continue.\n"),
         configfile);
      OK = false;
   }

   StorageResource *store = me;
   /* tls_require implies tls_enable */
   if (store->tls_cert.require) {
      if (have_tls) {
         store->tls_cert.enable = true;
      } else {
         Jmsg(NULL, M_FATAL, 0, _("TLS required but not configured in Bareos.\n"));
         OK = false;
      }
   }

   tls_needed = store->tls_cert.enable || store->tls_cert.authenticate;

   if ((store->tls_cert.certfile == nullptr || store->tls_cert.certfile->empty()) && tls_needed) {
      Jmsg(NULL,
           M_FATAL,
           0,
           _("\"TLS Certificate\" file not defined for Storage \"%s\" in %s.\n"),
           store->name(),
           configfile);
      OK = false;
   }

   if ((store->tls_cert.keyfile == nullptr || store->tls_cert.keyfile->empty()) && tls_needed) {
      Jmsg(NULL,
           M_FATAL,
           0,
           _("\"TLS Key\" file not defined for Storage \"%s\" in %s.\n"),
           store->name(),
           configfile);
      OK = false;
   }

   if (((store->tls_cert.ca_certfile == nullptr || store->tls_cert.ca_certfile->empty()) &&
        (store->tls_cert.ca_certdir == nullptr || store->tls_cert.ca_certdir->empty())) &&
       tls_needed && store->tls_cert.verify_peer) {
      Jmsg(NULL,
           M_FATAL,
           0,
           _("Neither \"TLS CA Certificate\""
             " or \"TLS CA Certificate Dir\" are defined for Storage \"%s\" in %s."
             " At least one CA certificate store is required"
             " when using \"TLS Verify Peer\".\n"),
           store->name(),
           configfile);
      OK = false;
   }

   DirectorResource *director;
   foreach_res(director, R_DIRECTOR) {
      /* tls_require implies tls_enable */
      if (director->tls_cert.require) {
         director->tls_cert.enable = true;
      }

      tls_needed = director->tls_cert.enable || director->tls_cert.authenticate;

      if ((director->tls_cert.certfile == nullptr || director->tls_cert.certfile->empty()) &&
          tls_needed) {
         Jmsg(NULL, M_FATAL, 0, _("\"TLS Certificate\" file not defined for Director \"%s\" in %s.\n"),
              director->name(), configfile);
         OK = false;
      }

      if ((director->tls_cert.keyfile == nullptr || director->tls_cert.keyfile->empty()) && tls_needed) {
         Jmsg(NULL, M_FATAL, 0, _("\"TLS Key\" file not defined for Director \"%s\" in %s.\n"),
              director->name(), configfile);
         OK = false;
      }

      if (((director->tls_cert.ca_certfile == nullptr || director->tls_cert.ca_certfile->empty()) &&
           (director->tls_cert.ca_certdir == nullptr || director->tls_cert.ca_certdir->empty())) &&
          tls_needed && director->tls_cert.verify_peer) {
         Jmsg(NULL, M_FATAL, 0, _("Neither \"TLS CA Certificate\""
              " or \"TLS CA Certificate Dir\" are defined for Director \"%s\" in %s."
              " At least one CA certificate store is required"
              " when using \"TLS Verify Peer\".\n"),
              director->name(), configfile);
         OK = false;
      }
    }

   DeviceResource *device;
   foreach_res(device, R_DEVICE) {
      if (device->drive_crypto_enabled && bit_is_set(CAP_LABEL, device->cap_bits)) {
         Jmsg(NULL, M_FATAL, 0, _("LabelMedia enabled is incompatible with tape crypto on Device \"%s\" in %s.\n"),
              device->name(), configfile);
         OK = false;
      }
   }

   if (OK) {
      OK = init_autochangers();
   }

   if (OK) {
      close_msg(NULL);                   /* close temp message handler */
      init_msg(NULL, me->messages);      /* open daemon message handler */
      set_working_directory(me->working_directory);
      if (me->secure_erase_cmdline) {
         set_secure_erase_cmdline(me->secure_erase_cmdline);
      }
      if (me->log_timestamp_format) {
         set_log_timestamp_format(me->log_timestamp_format);
      }
   }

   return OK;
}

/**
 * Remove old .spool files written by me from the working directory.
 */
static void cleanup_old_files()
{
   DIR* dp;
   struct dirent *result;
#ifdef USE_READDIR_R
   struct dirent *entry;
#endif
   int rc, name_max;
   int my_name_len = strlen(my_name);
   int len = strlen(me->working_directory);
   POOLMEM *cleanup = get_pool_memory(PM_MESSAGE);
   POOLMEM *basename = get_pool_memory(PM_MESSAGE);
   regex_t preg1;
   char prbuf[500];
   berrno be;

   /* Look for .spool files but don't allow spaces */
   const char *pat1 = "^[^ ]+\\.spool$";

   /* Setup working directory prefix */
   pm_strcpy(basename, me->working_directory);
   if (len > 0 && !IsPathSeparator(me->working_directory[len-1])) {
      pm_strcat(basename, "/");
   }

   /* Compile regex expressions */
   rc = regcomp(&preg1, pat1, REG_EXTENDED);
   if (rc != 0) {
      regerror(rc, &preg1, prbuf, sizeof(prbuf));
      Pmsg2(000,  _("Could not compile regex pattern \"%s\" ERR=%s\n"),
           pat1, prbuf);
      goto get_out2;
   }

   name_max = pathconf(".", _PC_NAME_MAX);
   if (name_max < 1024) {
      name_max = 1024;
   }

   if (!(dp = opendir(me->working_directory))) {
      berrno be;
      Pmsg2(000, "Failed to open working dir %s for cleanup: ERR=%s\n",
            me->working_directory, be.bstrerror());
      goto get_out1;
   }

#ifdef USE_READDIR_R
   entry = (struct dirent *)malloc(sizeof(struct dirent) + name_max + 1000);
   while (1) {
      if ((readdir_r(dp, entry, &result) != 0) || (result == NULL)) {
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
         pm_strcpy(cleanup, basename);
         pm_strcat(cleanup, result->d_name);
         Dmsg1(500, "Unlink: %s\n", cleanup);
         secure_erase(NULL, cleanup);
      }
   }
#ifdef USE_READDIR_R
   free(entry);
#endif
   closedir(dp);

get_out1:
   regfree(&preg1);
get_out2:
   free_pool_memory(cleanup);
   free_pool_memory(basename);
}


/**
 * Here we attempt to init and open each device. This is done once at startup in a separate thread.
 */
extern "C"
void *device_initialization(void *arg)
{
   DeviceResource *device;
   DeviceControlRecord *dcr;
   JobControlRecord *jcr;
   Device *dev;
   int errstat;

   LockRes();

   pthread_detach(pthread_self());
   jcr = new_jcr(sizeof(JobControlRecord), stored_free_jcr);
   new_plugins(jcr);  /* instantiate plugins */
   jcr->setJobType(JT_SYSTEM);

   /*
    * Initialize job start condition variable
    */
   errstat = pthread_cond_init(&jcr->job_start_wait, NULL);
   if (errstat != 0) {
      berrno be;
      Jmsg1(jcr, M_ABORT, 0, _("Unable to init job start cond variable: ERR=%s\n"), be.bstrerror(errstat));
   }

   /*
    * Initialize job end condition variable
    */
   errstat = pthread_cond_init(&jcr->job_end_wait, NULL);
   if (errstat != 0) {
      berrno be;
      Jmsg1(jcr, M_ABORT, 0, _("Unable to init job endstart cond variable: ERR=%s\n"), be.bstrerror(errstat));
   }

   foreach_res(device, R_DEVICE) {
      Dmsg1(90, "calling init_dev %s\n", device->device_name);
      dev = init_dev(NULL, device);
      Dmsg1(10, "SD init done %s\n", device->device_name);
      if (!dev) {
         Jmsg1(NULL, M_ERROR, 0, _("Could not initialize %s\n"), device->device_name);
         continue;
      }

      dcr = New(StorageDaemonDeviceControlRecord);
      jcr->dcr = dcr;
      setup_new_dcr_device(jcr, dcr, dev, NULL);
      jcr->dcr->set_will_write();
      generate_plugin_event(jcr, bsdEventDeviceInit, dcr);
      if (dev->is_autochanger()) {
         /*
          * If autochanger set slot in dev structure
          */
         get_autochanger_loaded_slot(dcr);
      }

      if (bit_is_set(CAP_ALWAYSOPEN, device->cap_bits)) {
         Dmsg1(20, "calling first_open_device %s\n", dev->print_name());
         if (!first_open_device(dcr)) {
            Jmsg1(NULL, M_ERROR, 0, _("Could not open device %s\n"), dev->print_name());
            Dmsg1(20, "Could not open device %s\n", dev->print_name());
            free_dcr(dcr);
            jcr->dcr = NULL;
            continue;
         }
      }

      if (bit_is_set(CAP_AUTOMOUNT, device->cap_bits) && dev->is_open()) {
         switch (read_dev_volume_label(dcr)) {
         case VOL_OK:
            memcpy(&dev->VolCatInfo, &dcr->VolCatInfo, sizeof(dev->VolCatInfo));
            volume_unused(dcr);             /* mark volume "released" */
            break;
         default:
            Jmsg1(NULL, M_WARNING, 0, _("Could not mount device %s\n"), dev->print_name());
            break;
         }
      }
      free_dcr(dcr);
      jcr->dcr = NULL;
   }
#ifdef xxx
   if (jcr->dcr) {
      Dmsg1(000, "free_dcr=%p\n", jcr->dcr);
      free_dcr(jcr->dcr);
      jcr->dcr = NULL;
   }
#endif
   free_jcr(jcr);
   init_done = true;
   UnlockRes();
   return NULL;
}

/**
 * Clean up and then exit
 */
#if !defined(HAVE_WIN32)
static
#endif
void terminate_stored(int sig)
{
   static bool in_here = false;
   DeviceResource *device;
   JobControlRecord *jcr;

   if (in_here) {                     /* prevent loops */
      bmicrosleep(2, 0);              /* yield */
      exit(1);
   }
   in_here = true;
   debug_level = 0;                   /* turn off any debug */
   stop_statistics_thread();
#if HAVE_NDMP
   if (me->ndmp_enable) {
      stop_ndmp_thread_server();
   }
#endif
   stop_watchdog();

   stop_socket_server();

   if (sig == SIGTERM) {              /* normal shutdown request? */
      /*
       * This is a normal shutdown request. We wiffle through
       *   all open jobs canceling them and trying to wake
       *   them up so that they will report back the correct
       *   volume status.
       */
      foreach_jcr(jcr) {
         BareosSocket *fd;
         if (jcr->JobId == 0) {
            free_jcr(jcr);
            continue;                 /* ignore console */
         }
         jcr->setJobStatus(JS_Canceled);
         fd = jcr->file_bsock;
         if (fd) {
            fd->set_timed_out();
            jcr->my_thread_send_signal(TIMEOUT_SIGNAL);
            Dmsg1(100, "term_stored killing JobId=%d\n", jcr->JobId);
            /* ***FIXME*** wiffle through all dcrs */
            if (jcr->dcr && jcr->dcr->dev && jcr->dcr->dev->blocked()) {
               pthread_cond_broadcast(&jcr->dcr->dev->wait_next_vol);
               Dmsg1(100, "JobId=%u broadcast wait_device_release\n", (uint32_t)jcr->JobId);
               release_device_cond();
            }
            if (jcr->read_dcr && jcr->read_dcr->dev && jcr->read_dcr->dev->blocked()) {
               pthread_cond_broadcast(&jcr->read_dcr->dev->wait_next_vol);
               Dmsg1(100, "JobId=%u broadcast wait_device_release\n", (uint32_t)jcr->JobId);
               release_device_cond();
            }
            bmicrosleep(0, 50000);
         }
         free_jcr(jcr);
      }
      bmicrosleep(0, 500000);         /* give them 1/2 sec to clean up */
   }

   write_state_file(me->working_directory, "bareos-sd", get_first_port_host_order(me->SDaddrs));
   delete_pid_file(me->pid_directory, "bareos-sd", get_first_port_host_order(me->SDaddrs));

   Dmsg1(200, "In terminate_stored() sig=%d\n", sig);

   unload_sd_plugins();
   flush_crypto_cache();
   free_volume_lists();

   foreach_res(device, R_DEVICE) {
      Dmsg1(10, "Term device %s\n", device->device_name);
      if (device->dev) {
         device->dev->clear_volhdr();
         device->dev->term();
         device->dev = NULL;
      } else {
         Dmsg1(10, "No dev structure %s\n", device->device_name);
      }
   }

#if defined(HAVE_DYNAMIC_SD_BACKENDS)
   dev_flush_backends();
#endif

   if (configfile) {
      free(configfile);
      configfile = NULL;
   }
   if (my_config) {
      my_config->free_resources();
      free(my_config);
      my_config = NULL;
   }

   if (debug_level > 10) {
      print_memory_pool_stats();
   }
   term_msg();
   cleanup_crypto();
   term_reservations_lock();
   close_memory_pool();
   lmgr_cleanup_main();

   sm_dump(false, false);             /* dump orphaned buffers */
   exit(sig);
}
