/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2013 Bareos GmbH & Co. KG

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
 * Second generation Storage daemon.
 *
 * Kern Sibbald, MM
 *
 * It accepts a number of simple commands from the File daemon
 * and acts on them. When a request to append data is made,
 * it opens a data channel and accepts data from the
 * File daemon.
 */

#include "bareos.h"
#include "stored.h"
#include "lib/crypto_cache.h"

/* Imported functions */
extern bool parse_sd_config(CONFIG *config, const char *configfile, int exit_code);

/* Forward referenced functions */
void terminate_stored(int sig);
static int check_resources();
static void cleanup_old_files();

extern "C" void *device_initialization(void *arg);

#define CONFIG_FILE "bareos-sd.conf"  /* Default config file */

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
static workq_t dird_workq;            /* queue for processing connections */
#if HAVE_NDMP
static workq_t ndmp_workq;            /* queue for processing NDMP connections */
#endif
static alist *sock_fds;

static void usage()
{
   fprintf(stderr, _(
PROG_COPYRIGHT
"\nVersion: %s (%s)\n\n"
"Usage: bareos-sd [options] [-c config_file] [config_file]\n"
"        -c <file>   use <file> as configuration file\n"
"        -d <nn>     set debug level to <nn>\n"
"        -dt         print timestamp in debug output\n"
"        -f          run in foreground (for debugging)\n"
"        -g <group>  set groupid to group\n"
"        -m          print kaboom output (for debugging)\n"
"        -p          proceed despite I/O errors\n"
"        -s          no signals (for debugging)\n"
"        -t          test - read config and exit\n"
"        -u <user>   userid to <user>\n"
"        -v          verbose user messages\n"
"        -?          print this message.\n"
"\n"), 2000, VERSION, BDATE);

   exit(1);
}

/*********************************************************************
 *
 *  Main Bareos Unix Storage Daemon
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

   /* Sanity checks */
   if (TAPE_BSIZE % B_DEV_BSIZE != 0 || TAPE_BSIZE / B_DEV_BSIZE == 0) {
      Emsg2(M_ABORT, 0, _("Tape block size (%d) not multiple of system size (%d)\n"),
         TAPE_BSIZE, B_DEV_BSIZE);
   }
   if (TAPE_BSIZE != (1 << (ffs(TAPE_BSIZE)-1))) {
      Emsg1(M_ABORT, 0, _("Tape block size (%d) is not a power of 2\n"), TAPE_BSIZE);
   }

   while ((ch = getopt(argc, argv, "c:d:fg:mpstu:v?")) != -1) {
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

      case '?':
      default:
         usage();
         break;
      }
   }
   argc -= optind;
   argv += optind;

   if (argc) {
      if (configfile != NULL) {
         free(configfile);
      }
      configfile = bstrdup(*argv);
      argc--;
      argv++;
   }
   if (argc)
      usage();

   /*
    * See if we want to drop privs.
    */
   if (geteuid() == 0) {
      drop(uid, gid, false);
   }

   if (!no_signals) {
      init_signals(terminate_stored);
   }

   if (configfile == NULL) {
      configfile = bstrdup(CONFIG_FILE);
   }

   my_config = new_config_parser();
   parse_sd_config(my_config, configfile, M_ERROR_TERM);

   if (init_crypto() != 0) {
      Jmsg((JCR *)NULL, M_ERROR_TERM, 0, _("Cryptography library initialization failed.\n"));
   }

   if (!check_resources()) {
      Jmsg((JCR *)NULL, M_ERROR_TERM, 0, _("Please correct configuration file: %s\n"), configfile);
   }

   init_reservations_lock();

   if (test_config) {
      terminate_stored(0);
   }

   my_name_is(0, (char **)NULL, me->hdr.name);     /* Set our real name */

   if (!foreground) {
      daemon_start();                 /* become daemon */
      init_stack_dump();              /* pick up new pid */
   }

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
   set_thread_concurrency(me->max_concurrent_jobs * 2 + 4);
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
      init_jcr_subsystem(me->jcr_watchdog_time); /* start JCR watchdogs etc. */
   }

   start_statistics_thread();

#if HAVE_NDMP
   /* Seperate thread that handles NDMP connections */
   if (me->ndmp_enable) {
      start_ndmp_thread_server(me->NDMPaddrs,
                               me->max_concurrent_jobs * 2 + 1,
                               &ndmp_workq);
   }
#endif

   /* Single server used for Director/Storage and File daemon */
   sock_fds = New(alist(10, not_owned_by_alist));
   bnet_thread_server_tcp(me->SDaddrs,
                      me->max_concurrent_jobs * 2 + 1,
                      sock_fds,
                      &dird_workq,
                      me->nokeepalive,
                      handle_connection_request);
   exit(1);                           /* to keep compiler quiet */
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

   if (GetNextRes(R_STORAGE, (RES *)me) != NULL) {
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

   if (!me->messages) {
      me->messages = (MSGSRES *)GetNextRes(R_MSGS, NULL);
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

   STORES *store;
   foreach_res(store, R_STORAGE) {
      /* tls_require implies tls_enable */
      if (store->tls_require) {
         if (have_tls) {
            store->tls_enable = true;
         } else {
            Jmsg(NULL, M_FATAL, 0, _("TLS required but not configured in Bareos.\n"));
            OK = false;
            continue;
         }
      }

      tls_needed = store->tls_enable || store->tls_authenticate;

      if (!store->tls_certfile && tls_needed) {
         Jmsg(NULL, M_FATAL, 0, _("\"TLS Certificate\" file not defined for Storage \"%s\" in %s.\n"),
              store->hdr.name, configfile);
         OK = false;
      }

      if (!store->tls_keyfile && tls_needed) {
         Jmsg(NULL, M_FATAL, 0, _("\"TLS Key\" file not defined for Storage \"%s\" in %s.\n"),
              store->hdr.name, configfile);
         OK = false;
      }

      if ((!store->tls_ca_certfile && !store->tls_ca_certdir) && tls_needed && store->tls_verify_peer) {
         Jmsg(NULL, M_FATAL, 0, _("Neither \"TLS CA Certificate\""
              " or \"TLS CA Certificate Dir\" are defined for Storage \"%s\" in %s."
              " At least one CA certificate store is required"
              " when using \"TLS Verify Peer\".\n"),
              store->hdr.name, configfile);
         OK = false;
      }

      /* If everything is well, attempt to initialize our per-resource TLS context */
      if (OK && (tls_needed || store->tls_require)) {
         /* Initialize TLS context:
          * Args: CA certfile, CA certdir, Certfile, Keyfile,
          * Keyfile PEM Callback, Keyfile CB Userdata, DHfile, Verify Peer */
         store->tls_ctx = new_tls_context(store->tls_ca_certfile,
                                          store->tls_ca_certdir,
                                          store->tls_crlfile,
                                          store->tls_certfile,
                                          store->tls_keyfile,
                                          NULL,
                                          NULL,
                                          store->tls_dhfile,
                                          store->tls_verify_peer);

         if (!store->tls_ctx) {
            Jmsg(NULL, M_FATAL, 0, _("Failed to initialize TLS context for Storage \"%s\" in %s.\n"),
                 store->hdr.name, configfile);
            OK = false;
         }

         set_tls_enable(store->tls_ctx, tls_needed);
         set_tls_require(store->tls_ctx, store->tls_require);
      }
   }

   DIRRES *director;
   foreach_res(director, R_DIRECTOR) {
      /* tls_require implies tls_enable */
      if (director->tls_require) {
         director->tls_enable = true;
      }

      tls_needed = director->tls_enable || director->tls_authenticate;

      if (!director->tls_certfile && tls_needed) {
         Jmsg(NULL, M_FATAL, 0, _("\"TLS Certificate\" file not defined for Director \"%s\" in %s.\n"),
              director->hdr.name, configfile);
         OK = false;
      }

      if (!director->tls_keyfile && tls_needed) {
         Jmsg(NULL, M_FATAL, 0, _("\"TLS Key\" file not defined for Director \"%s\" in %s.\n"),
              director->hdr.name, configfile);
         OK = false;
      }

      if ((!director->tls_ca_certfile && !director->tls_ca_certdir) && tls_needed && director->tls_verify_peer) {
         Jmsg(NULL, M_FATAL, 0, _("Neither \"TLS CA Certificate\""
              " or \"TLS CA Certificate Dir\" are defined for Director \"%s\" in %s."
              " At least one CA certificate store is required"
              " when using \"TLS Verify Peer\".\n"),
              director->hdr.name, configfile);
         OK = false;
      }

      /* If everything is well, attempt to initialize our per-resource TLS context */
      if (OK && (tls_needed || director->tls_require)) {
         /* Initialize TLS context:
          * Args: CA certfile, CA certdir, Certfile, Keyfile,
          * Keyfile PEM Callback, Keyfile CB Userdata, DHfile, Verify Peer */
         director->tls_ctx = new_tls_context(director->tls_ca_certfile,
                                             director->tls_ca_certdir,
                                             director->tls_crlfile,
                                             director->tls_certfile,
                                             director->tls_keyfile,
                                             NULL,
                                             NULL,
                                             director->tls_dhfile,
                                             director->tls_verify_peer);

         if (!director->tls_ctx) {
            Jmsg(NULL, M_FATAL, 0, _("Failed to initialize TLS context for Director \"%s\" in %s.\n"),
                 director->hdr.name, configfile);
            OK = false;
         }

         set_tls_enable(director->tls_ctx, tls_needed);
         set_tls_require(director->tls_ctx, director->tls_require);
      }
   }

   DEVRES *device;
   foreach_res(device, R_DEVICE) {
      if (device->drive_crypto_enabled && device->cap_bits & CAP_LABEL) {
         Jmsg(NULL, M_FATAL, 0, _("LabelMedia enabled is incompatible with tape crypto on Device \"%s\" in %s.\n"),
              device->hdr.name, configfile);
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
   }

   return OK;
}

/*
 * Remove old .spool files written by me from the working directory.
 */
static void cleanup_old_files()
{
   DIR* dp;
   struct dirent *entry, *result;
   int rc, name_max;
   int my_name_len = strlen(my_name);
   int len = strlen(me->working_directory);
   POOLMEM *cleanup = get_pool_memory(PM_MESSAGE);
   POOLMEM *basename = get_pool_memory(PM_MESSAGE);
   regex_t preg1;
   char prbuf[500];
   const int nmatch = 30;
   regmatch_t pmatch[nmatch];
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

   entry = (struct dirent *)malloc(sizeof(struct dirent) + name_max + 1000);
   while (1) {
      if ((readdir_r(dp, entry, &result) != 0) || (result == NULL)) {
         break;
      }
      /* Exclude any name with ., .., not my_name or containing a space */
      if (strcmp(result->d_name, ".") == 0 || strcmp(result->d_name, "..") == 0 ||
          strncmp(result->d_name, my_name, my_name_len) != 0) {
         Dmsg1(500, "Skipped: %s\n", result->d_name);
         continue;
      }

      /* Unlink files that match regex */
      if (regexec(&preg1, result->d_name, nmatch, pmatch,  0) == 0) {
         pm_strcpy(cleanup, basename);
         pm_strcat(cleanup, result->d_name);
         Dmsg1(500, "Unlink: %s\n", cleanup);
         unlink(cleanup);
      }
   }
   free(entry);
   closedir(dp);

get_out1:
   regfree(&preg1);
get_out2:
   free_pool_memory(cleanup);
   free_pool_memory(basename);
}


/*
 * Here we attempt to init and open each device. This is done once at startup in a separate thread.
 */
extern "C"
void *device_initialization(void *arg)
{
   DEVRES *device;
   DCR *dcr;
   JCR *jcr;
   DEVICE *dev;
   int errstat;

   LockRes();

   pthread_detach(pthread_self());
   jcr = new_jcr(sizeof(JCR), stored_free_jcr);
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

      dcr = New(SD_DCR);
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

      if (device->cap_bits & CAP_ALWAYSOPEN) {
         Dmsg1(20, "calling first_open_device %s\n", dev->print_name());
         if (!first_open_device(dcr)) {
            Jmsg1(NULL, M_ERROR, 0, _("Could not open device %s\n"), dev->print_name());
            Dmsg1(20, "Could not open device %s\n", dev->print_name());
            free_dcr(dcr);
            jcr->dcr = NULL;
            continue;
         }
      }

      if (device->cap_bits & CAP_AUTOMOUNT && dev->is_open()) {
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

/*
 * Clean up and then exit
 */
void terminate_stored(int sig)
{
   static bool in_here = false;
   DEVRES *device;
   JCR *jcr;

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

   cleanup_bnet_thread_server_tcp(sock_fds, &dird_workq);
   delete sock_fds;
   sock_fds = NULL;

   if (sig == SIGTERM) {              /* normal shutdown request? */
      /*
       * This is a normal shutdown request. We wiffle through
       *   all open jobs canceling them and trying to wake
       *   them up so that they will report back the correct
       *   volume status.
       */
      foreach_jcr(jcr) {
         BSOCK *fd;
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

   sm_dump(false);                    /* dump orphaned buffers */
   exit(sig);
}
