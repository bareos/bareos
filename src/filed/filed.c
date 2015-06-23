/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2010 Free Software Foundation Europe e.V.
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

/* Forward referenced functions */
static bool check_resources();

/* Exported variables */
CLIENTRES *me = NULL;                 /* Our Global resource */
CONFIG *my_config = NULL;             /* Our Global config */

bool no_signals = false;
bool backup_only_mode = false;
bool restore_only_mode = false;
void *start_heap;

#define CONFIG_FILE "bareos-fd.conf"  /* default config file */

char *configfile = NULL;
static bool foreground = false;
static workq_t dir_workq;             /* queue of work from Director */
static alist *sock_fds;
static pthread_t tcp_server_tid;

static void usage()
{
   fprintf(stderr, _(
PROG_COPYRIGHT
"\nVersion: %s (%s)\n\n"
"Usage: bareos-fd [-f -s] [-c config_file] [-d debug_level]\n"
"        -b          backup only mode\n"
"        -c <file>   use <file> as configuration file\n"
"        -d <nn>     set debug level to <nn>\n"
"        -dt         print a timestamp in debug output\n"
"        -f          run in foreground (for debugging)\n"
"        -g          groupid\n"
"        -k          keep readall capabilities\n"
"        -m          print kaboom output (for debugging)\n"
"        -r          restore only mode\n"
"        -s          no signals (for debugging)\n"
"        -t          test configuration file and exit\n"
"        -u          userid\n"
"        -v          verbose user messages\n"
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

   while ((ch = getopt(argc, argv, "bc:d:fg:kmrstu:v?")) != -1) {
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

      case '?':
      default:
         usage();

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

   tcp_server_tid = pthread_self();
   if (!no_signals) {
      init_signals(terminate_filed);
   } else {
      /* This reduces the number of signals facilitating debugging */
      watchdog_sleep_time = 120;      /* long timeout for debugging */
   }

   if (configfile == NULL) {
      configfile = bstrdup(CONFIG_FILE);
   }

   my_config = new_config_parser();
   parse_fd_config(my_config, configfile, M_ERROR_TERM);

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

   set_thread_concurrency(me->MaxConcurrentJobs + 10);
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
   tcp_server_tid = pthread_self();

   /* Become server, and handle requests */
   IPADDR *p;
   foreach_dlist(p, me->FDaddrs) {
      Dmsg1(10, "filed: listening on port %d\n", p->get_port_host_order());
   }

   sock_fds = New(alist(10, not_owned_by_alist));
   bnet_thread_server_tcp(me->FDaddrs,
                          me->MaxConcurrentJobs,
                          sock_fds,
                          &dir_workq,
                          me->nokeepalive,
                          handle_connection_request);

   terminate_filed(0);

   exit(0);                           /* should never get here */
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

   bnet_stop_thread_server_tcp(tcp_server_tid);
   cleanup_bnet_thread_server_tcp(sock_fds, &dir_workq);
   delete sock_fds;
   sock_fds = NULL;

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
   sm_dump(false);                    /* dump orphaned buffers */
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
      if (GetNextRes(R_CLIENT, (RES *) me) != NULL) {
         Emsg1(M_FATAL, 0, _("Only one Client resource permitted in %s\n"),
              configfile);
         OK = false;
      }
      my_name_is(0, NULL, me->hdr.name);
      if (!me->messages) {
         me->messages = (MSGSRES *)GetNextRes(R_MSGS, NULL);
         if (!me->messages) {
             Emsg1(M_FATAL, 0, _("No Messages resource defined in %s\n"), configfile);
             OK = false;
         }
      }
      /* tls_require implies tls_enable */
      if (me->tls_require) {
#ifndef HAVE_TLS
         Jmsg(NULL, M_FATAL, 0, _("TLS required but not configured in Bareos.\n"));
         OK = false;
#else
         me->tls_enable = true;
#endif
      }
      need_tls = me->tls_enable || me->tls_authenticate;

      if ((!me->tls_ca_certfile && !me->tls_ca_certdir) && need_tls) {
         Emsg1(M_FATAL, 0, _("Neither \"TLS CA Certificate\""
            " or \"TLS CA Certificate Dir\" are defined for File daemon in %s.\n"),
                            configfile);
        OK = false;
      }

      /* If everything is well, attempt to initialize our per-resource TLS context */
      if (OK && (need_tls || me->tls_require)) {
         /* Initialize TLS context:
          * Args: CA certfile, CA certdir, Certfile, Keyfile,
          * Keyfile PEM Callback, Keyfile CB Userdata, DHfile, Verify Peer */
         me->tls_ctx = new_tls_context(me->tls_ca_certfile,
                                       me->tls_ca_certdir,
                                       me->tls_crlfile,
                                       me->tls_certfile,
                                       me->tls_keyfile,
                                       NULL,
                                       NULL,
                                       NULL,
                                       me->tls_verify_peer);

         if (!me->tls_ctx) {
            Emsg2(M_FATAL, 0, _("Failed to initialize TLS context for File daemon \"%s\" in %s.\n"),
                                me->hdr.name, configfile);
            OK = false;
         }

         set_tls_enable(me->tls_ctx, need_tls);
         set_tls_require(me->tls_ctx, me->tls_require);
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
            " \"PKI Encrypt\" are enabled.\n"), me->hdr.name, configfile);
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
                     " daemon \"%s\" in %s.\n"), me->hdr.name, configfile);
               OK = false;
            }

            if (!crypto_keypair_load_key(me->pki_keypair, me->pki_keypair_file, NULL, NULL)) {
               Emsg2(M_FATAL, 0, _("Failed to load private key for File"
                     " daemon \"%s\" in %s.\n"), me->hdr.name, configfile);
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
                              " daemon \"%s\" in %s.\n"), filepath, me->hdr.name, configfile);
                           OK = false;
                        }
                     }

                  } else {
                     Emsg3(M_FATAL, 0, _("Failed to load trusted signer certificate"
                        " from file %s for File daemon \"%s\" in %s.\n"), filepath, me->hdr.name, configfile);
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
                        " from file %s for File daemon \"%s\" in %s.\n"), filepath, me->hdr.name, configfile);
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
      if (director->tls_require) {
#ifndef HAVE_TLS
         Jmsg(NULL, M_FATAL, 0, _("TLS required but not configured in Bareos.\n"));
         OK = false;
         continue;
#else
         director->tls_enable = true;
#endif
      }
      need_tls = director->tls_enable || director->tls_authenticate;

      if (!director->tls_certfile && need_tls) {
         Emsg2(M_FATAL, 0, _("\"TLS Certificate\" file not defined for Director \"%s\" in %s.\n"),
               director->hdr.name, configfile);
         OK = false;
      }

      if (!director->tls_keyfile && need_tls) {
         Emsg2(M_FATAL, 0, _("\"TLS Key\" file not defined for Director \"%s\" in %s.\n"),
               director->hdr.name, configfile);
         OK = false;
      }

      if ((!director->tls_ca_certfile && !director->tls_ca_certdir) && need_tls && director->tls_verify_peer) {
         Emsg2(M_FATAL, 0, _("Neither \"TLS CA Certificate\""
                             " or \"TLS CA Certificate Dir\" are defined for Director \"%s\" in %s."
                             " At least one CA certificate store is required"
                             " when using \"TLS Verify Peer\".\n"),
                             director->hdr.name, configfile);
         OK = false;
      }

      /* If everything is well, attempt to initialize our per-resource TLS context */
      if (OK && (need_tls || director->tls_require)) {
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
            Emsg2(M_FATAL, 0, _("Failed to initialize TLS context for Director \"%s\" in %s.\n"),
                                director->hdr.name, configfile);
            OK = false;
         }

         set_tls_enable(director->tls_ctx, need_tls);
         set_tls_require(director->tls_ctx, director->tls_require);
      }
   }

   UnlockRes();

   if (OK) {
      close_msg(NULL);                /* close temp message handler */
      init_msg(NULL, me->messages);   /* open user specified message handler */
   }

   return OK;
}
