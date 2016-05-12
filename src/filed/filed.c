/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2010 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2014 Bareos GmbH & Co. KG

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
 * Bareos File Daemon
 *
 * Kern Sibbald, March MM
 */

#include "bareos.h"
#include "filed.h"
#include "lib/mntent_cache.h"

/* Imported Functions */
extern void *handle_connection_request(void *dir_sock);
extern bool parse_fd_config(CONFIG *config, const char *configfile, int exit_code);
extern void prtmsg(void *sock, const char *fmt, ...);

/* Forward referenced functions */
static bool check_resources();

/* Exported variables */
CLIENTRES *me = NULL;                 /* Our Global resource */
CONFIG *my_config = NULL;             /* Our Global config */

bool no_signals = false;
bool backup_only_mode = false;
bool restore_only_mode = false;
void *start_heap;

char *configfile = NULL;
static bool foreground = false;

static void usage()
{
   fprintf(stderr, _(
PROG_COPYRIGHT
"\nVersion: %s (%s)\n\n"
"Usage: bareos-fd [options] [-c config_file]\n"
"        -b          backup only mode\n"
"        -c <path>   use <path> as configuration file or directory\n"
"        -d <nn>     set debug level to <nn>\n"
"        -dt         print timestamp in debug output\n"
"        -f          run in foreground (for debugging)\n"
"        -g <group>  run as group <group>\n"
"        -k          keep readall capabilities\n"
"        -m          print kaboom output (for debugging)\n"
"        -r          restore only mode\n"
"        -s          no signals (for debugging)\n"
"        -t          test configuration file and exit\n"
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
 *  Main Bareos Unix Client Program
 *
 */
#if defined(HAVE_WIN32)
#define main BareosMain
#endif

int main (int argc, char *argv[])
{
   int ch;
   bool test_config = false;
   bool export_config = false;
   bool export_config_schema = false;
   bool keep_readall_caps = false;
   char *uid = NULL;
   char *gid = NULL;

   start_heap = sbrk(0);
   setlocale(LC_ALL, "");
   bindtextdomain("bareos", LOCALEDIR);
   textdomain("bareos");

   init_stack_dump();
   my_name_is(argc, argv, "bareos-fd");
   init_msg(NULL, NULL);
   daemon_start_time = time(NULL);

   while ((ch = getopt(argc, argv, "bc:d:fg:kmrstu:vx:?")) != -1) {
      switch (ch) {
      case 'b':
         backup_only_mode = true;
         break;

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

      case 'g':                    /* set group */
         gid = optarg;
         break;

      case 'k':
         keep_readall_caps = true;
         break;

      case 'm':                    /* print kaboom output */
         prt_kaboom = true;
         break;

      case 'r':
         restore_only_mode = true;
         break;

      case 's':
         no_signals = true;
         break;

      case 't':
         test_config = true;
         break;

      case 'u':                    /* set userid */
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

   if (argc) {
      if (configfile != NULL)
         free(configfile);
      configfile = bstrdup(*argv);
      argc--;
      argv++;
   }
   if (argc) {
      usage();
   }

   if (!uid && keep_readall_caps) {
      Emsg0(M_ERROR_TERM, 0, _("-k option has no meaning without -u option.\n"));
   }

   /*
    * See if we want to drop privs.
    */
   if (geteuid() == 0) {
      drop(uid, gid, keep_readall_caps);
   }

   if (!no_signals) {
      init_signals(terminate_filed);
   } else {
      /*
       * This reduces the number of signals facilitating debugging
       */
      watchdog_sleep_time = 120;      /* long timeout for debugging */
   }

   if (export_config_schema) {
      POOL_MEM buffer;

      my_config = new_config_parser();
      init_fd_config(my_config, configfile, M_ERROR_TERM);
      print_config_schema_json(buffer);
      printf("%s\n", buffer.c_str());
      goto bail_out;
   }

   my_config = new_config_parser();
   parse_fd_config(my_config, configfile, M_ERROR_TERM);

   if (export_config) {
      my_config->dump_resources(prtmsg, NULL);
      goto bail_out;
   }

   if (!foreground && !test_config) {
      daemon_start();
      init_stack_dump();              /* set new pid */
   }

   if (init_crypto() != 0) {
      Emsg0(M_ERROR, 0, _("Cryptography library initialization failed.\n"));
      terminate_filed(1);
   }

   if (!check_resources()) {
      Emsg1(M_ERROR, 0, _("Please correct configuration file: %s\n"), configfile);
      terminate_filed(1);
   }

   set_working_directory(me->working_directory);

#if defined(HAVE_WIN32)
   if (me->compatible) {
      Win32SetCompatible();
   } else {
      Win32ClearCompatible();
   }
#endif

   if (test_config) {
      terminate_filed(0);
   }

   set_thread_concurrency(me->MaxConcurrentJobs * 2 + 10);
   lmgr_init_thread(); /* initialize the lockmanager stack */

   /* Maximum 1 daemon at a time */
   create_pid_file(me->pid_directory, "bareos-fd",
                   get_first_port_host_order(me->FDaddrs));
   read_state_file(me->working_directory, "bareos-fd",
                   get_first_port_host_order(me->FDaddrs));

   load_fd_plugins(me->plugin_directory, me->plugin_names);

   if (!no_signals) {
      start_watchdog();               /* start watchdog thread */
      if (me->jcr_watchdog_time) {
         init_jcr_subsystem(me->jcr_watchdog_time); /* start JCR watchdogs etc. */
      }
   }

   /*
    * if configured, start threads and connect to Director.
    */
   start_connect_to_director_threads();

   /*
    * start socket server to listen for new connections.
    */
   start_socket_server(me->FDaddrs);

   terminate_filed(0);

bail_out:
   exit(0);
}

void terminate_filed(int sig)
{
   static bool already_here = false;

   if (already_here) {
      bmicrosleep(2, 0);              /* yield */
      exit(1);                        /* prevent loops */
   }
   already_here = true;
   debug_level = 0;                   /* turn off debug */
   stop_watchdog();

   stop_connect_to_director_threads(true);
   stop_socket_server(true);

   unload_fd_plugins();
   flush_mntent_cache();
   write_state_file(me->working_directory, "bareos-fd", get_first_port_host_order(me->FDaddrs));
   delete_pid_file(me->pid_directory, "bareos-fd", get_first_port_host_order(me->FDaddrs));

   if (configfile != NULL) {
      free(configfile);
   }

   if (debug_level > 0) {
      print_memory_pool_stats();
   }
   if (my_config) {
      my_config->free_resources();
      free(my_config);
      my_config = NULL;
   }
   term_msg();
   cleanup_crypto();
   close_memory_pool();               /* release free memory in pool */
   lmgr_cleanup_main();
   sm_dump(false, false);             /* dump orphaned buffers */
   exit(sig);
}

/*
* Make a quick check to see that we have all the
* resources needed.
*/
static bool check_resources()
{
   bool OK = true;
   DIRRES *director;
   bool need_tls;

   LockRes();

   me = (CLIENTRES *)GetNextRes(R_CLIENT, NULL);
   if (!me) {
      Emsg1(M_FATAL, 0, _("No File daemon resource defined in %s\n"
            "Without that I don't know who I am :-(\n"), configfile);
      OK = false;
   } else {
      /*
       * Sanity check.
       */
      if (me->MaxConnections < (2 * me->MaxConcurrentJobs)) {
         me->MaxConnections = (2 * me->MaxConcurrentJobs) + 2;
      }

      if (GetNextRes(R_CLIENT, (RES *) me) != NULL) {
         Emsg1(M_FATAL, 0, _("Only one Client resource permitted in %s\n"),
              configfile);
         OK = false;
      }
      my_name_is(0, NULL, me->name());
      if (!me->messages) {
         me->messages = (MSGSRES *)GetNextRes(R_MSGS, NULL);
         if (!me->messages) {
             Emsg1(M_FATAL, 0, _("No Messages resource defined in %s\n"), configfile);
             OK = false;
         }
      }
      /* tls_require implies tls_enable */
      if (me->tls.require) {
#ifndef HAVE_TLS
         Jmsg(NULL, M_FATAL, 0, _("TLS required but not configured in Bareos.\n"));
         OK = false;
#else
         me->tls.enable = true;
#endif
      }
      need_tls = me->tls.enable || me->tls.authenticate;

      if ((!me->tls.ca_certfile && !me->tls.ca_certdir) && need_tls) {
         Emsg1(M_FATAL, 0, _("Neither \"TLS CA Certificate\""
            " or \"TLS CA Certificate Dir\" are defined for File daemon in %s.\n"),
                            configfile);
        OK = false;
      }

      /* If everything is well, attempt to initialize our per-resource TLS context */
      if (OK && (need_tls || me->tls.require)) {
         /* Initialize TLS context:
          * Args: CA certfile, CA certdir, Certfile, Keyfile,
          * Keyfile PEM Callback, Keyfile CB Userdata, DHfile, Verify Peer */
         me->tls.ctx = new_tls_context(me->tls.ca_certfile,
                                       me->tls.ca_certdir,
                                       me->tls.crlfile,
                                       me->tls.certfile,
                                       me->tls.keyfile,
                                       NULL,
                                       NULL,
                                       NULL,
                                       me->tls.cipherlist,
                                       me->tls.verify_peer);

         if (!me->tls.ctx) {
            Emsg2(M_FATAL, 0, _("Failed to initialize TLS context for File daemon \"%s\" in %s.\n"),
                                me->name(), configfile);
            OK = false;
         }

         set_tls_enable(me->tls.ctx, need_tls);
         set_tls_require(me->tls.ctx, me->tls.require);
      }

      if (me->pki_encrypt || me->pki_sign) {
#ifndef HAVE_CRYPTO
         Jmsg(NULL, M_FATAL, 0, _("PKI encryption/signing enabled but not compiled into Bareos.\n"));
         OK = false;
#endif
      }

      /* pki_encrypt implies pki_sign */
      if (me->pki_encrypt) {
         me->pki_sign = true;
      }

      if ((me->pki_encrypt || me->pki_sign) && !me->pki_keypair_file) {
         Emsg2(M_FATAL, 0, _("\"PKI Key Pair\" must be defined for File"
            " daemon \"%s\" in %s if either \"PKI Sign\" or"
            " \"PKI Encrypt\" are enabled.\n"), me->name(), configfile);
         OK = false;
      }

      /* If everything is well, attempt to initialize our public/private keys */
      if (OK && (me->pki_encrypt || me->pki_sign)) {
         char *filepath;
         /* Load our keypair */
         me->pki_keypair = crypto_keypair_new();
         if (!me->pki_keypair) {
            Emsg0(M_FATAL, 0, _("Failed to allocate a new keypair object.\n"));
            OK = false;
         } else {
            if (!crypto_keypair_load_cert(me->pki_keypair, me->pki_keypair_file)) {
               Emsg2(M_FATAL, 0, _("Failed to load public certificate for File"
                     " daemon \"%s\" in %s.\n"), me->name(), configfile);
               OK = false;
            }

            if (!crypto_keypair_load_key(me->pki_keypair, me->pki_keypair_file, NULL, NULL)) {
               Emsg2(M_FATAL, 0, _("Failed to load private key for File"
                     " daemon \"%s\" in %s.\n"), me->name(), configfile);
               OK = false;
            }
         }

         /*
          * Trusted Signers. We're always trusted.
          */
         me->pki_signers = New(alist(10, not_owned_by_alist));
         if (me->pki_keypair) {
            me->pki_signers->append(crypto_keypair_dup(me->pki_keypair));
         }

         /* If additional signing public keys have been specified, load them up */
         if (me->pki_signing_key_files) {
            foreach_alist(filepath, me->pki_signing_key_files) {
               X509_KEYPAIR *keypair;

               keypair = crypto_keypair_new();
               if (!keypair) {
                  Emsg0(M_FATAL, 0, _("Failed to allocate a new keypair object.\n"));
                  OK = false;
               } else {
                  if (crypto_keypair_load_cert(keypair, filepath)) {
                     me->pki_signers->append(keypair);

                     /* Attempt to load a private key, if available */
                     if (crypto_keypair_has_key(filepath)) {
                        if (!crypto_keypair_load_key(keypair, filepath, NULL, NULL)) {
                           Emsg3(M_FATAL, 0, _("Failed to load private key from file %s for File"
                              " daemon \"%s\" in %s.\n"), filepath, me->name(), configfile);
                           OK = false;
                        }
                     }

                  } else {
                     Emsg3(M_FATAL, 0, _("Failed to load trusted signer certificate"
                        " from file %s for File daemon \"%s\" in %s.\n"), filepath, me->name(), configfile);
                     OK = false;
                  }
               }
            }
         }

         /*
          * Crypto recipients. We're always included as a recipient.
          * The symmetric session key will be encrypted for each of these readers.
          */
         me->pki_recipients = New(alist(10, not_owned_by_alist));
         if (me->pki_keypair) {
            me->pki_recipients->append(crypto_keypair_dup(me->pki_keypair));
         }


         /* If additional keys have been specified, load them up */
         if (me->pki_master_key_files) {
            foreach_alist(filepath, me->pki_master_key_files) {
               X509_KEYPAIR *keypair;

               keypair = crypto_keypair_new();
               if (!keypair) {
                  Emsg0(M_FATAL, 0, _("Failed to allocate a new keypair object.\n"));
                  OK = false;
               } else {
                  if (crypto_keypair_load_cert(keypair, filepath)) {
                     me->pki_recipients->append(keypair);
                  } else {
                     Emsg3(M_FATAL, 0, _("Failed to load master key certificate"
                        " from file %s for File daemon \"%s\" in %s.\n"), filepath, me->name(), configfile);
                     OK = false;
                  }
               }
            }
         }
      }
   }


   /* Verify that a director record exists */
   LockRes();
   director = (DIRRES *)GetNextRes(R_DIRECTOR, NULL);
   UnlockRes();
   if (!director) {
      Emsg1(M_FATAL, 0, _("No Director resource defined in %s\n"),
            configfile);
      OK = false;
   }

   foreach_res(director, R_DIRECTOR) {
      /* tls_require implies tls_enable */
      if (director->tls.require) {
#ifndef HAVE_TLS
         Jmsg(NULL, M_FATAL, 0, _("TLS required but not configured in Bareos.\n"));
         OK = false;
         continue;
#else
         director->tls.enable = true;
#endif
      }
      need_tls = director->tls.enable || director->tls.authenticate;

      if (!director->tls.certfile && need_tls) {
         Emsg2(M_FATAL, 0, _("\"TLS Certificate\" file not defined for Director \"%s\" in %s.\n"),
               director->name(), configfile);
         OK = false;
      }

      if (!director->tls.keyfile && need_tls) {
         Emsg2(M_FATAL, 0, _("\"TLS Key\" file not defined for Director \"%s\" in %s.\n"),
               director->name(), configfile);
         OK = false;
      }

      if ((!director->tls.ca_certfile && !director->tls.ca_certdir) && need_tls && director->tls.verify_peer) {
         Emsg2(M_FATAL, 0, _("Neither \"TLS CA Certificate\""
                             " or \"TLS CA Certificate Dir\" are defined for Director \"%s\" in %s."
                             " At least one CA certificate store is required"
                             " when using \"TLS Verify Peer\".\n"),
                             director->name(), configfile);
         OK = false;
      }

      /* If everything is well, attempt to initialize our per-resource TLS context */
      if (OK && (need_tls || director->tls.require)) {
         /* Initialize TLS context:
          * Args: CA certfile, CA certdir, Certfile, Keyfile,
          * Keyfile PEM Callback, Keyfile CB Userdata, DHfile, Verify Peer */
         director->tls.ctx = new_tls_context(director->tls.ca_certfile,
                                             director->tls.ca_certdir,
                                             director->tls.crlfile,
                                             director->tls.certfile,
                                             director->tls.keyfile,
                                             NULL,
                                             NULL,
                                             director->tls.dhfile,
                                             director->tls.cipherlist,
                                             director->tls.verify_peer);

         if (!director->tls.ctx) {
            Emsg2(M_FATAL, 0, _("Failed to initialize TLS context for Director \"%s\" in %s.\n"),
                                director->name(), configfile);
            OK = false;
         }

         set_tls_enable(director->tls.ctx, need_tls);
         set_tls_require(director->tls.ctx, director->tls.require);
      }
   }

   UnlockRes();

   if (OK) {
      close_msg(NULL);                /* close temp message handler */
      init_msg(NULL, me->messages);   /* open user specified message handler */
      if (me->secure_erase_cmdline) {
         set_secure_erase_cmdline(me->secure_erase_cmdline);
      }
      if (me->log_timestamp_format) {
         set_log_timestamp_format(me->log_timestamp_format);
      }
   }

   return OK;
}
