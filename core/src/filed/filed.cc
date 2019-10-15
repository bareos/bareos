/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2010 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2019 Bareos GmbH & Co. KG

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
 * Kern Sibbald, March MM
 */
/**
 * @file
 * Bareos File Daemon
 */

#include "include/bareos.h"
#include "filed/filed.h"
#include "filed/filed_globals.h"
#include "filed/dir_cmd.h"
#include "filed/socket_server.h"
#include "lib/mntent_cache.h"
#include "lib/daemon.h"
#include "lib/bnet_network_dump.h"
#include "lib/bsignal.h"
#include "lib/parse_conf.h"
#include "lib/watchdog.h"
#include "lib/util.h"
#include "lib/address_conf.h"

using namespace filedaemon;

/* Imported Functions */
extern void* handle_connection_request(void* dir_sock);
extern void PrintMessage(void* sock, const char* fmt, ...);

/* Forward referenced functions */
static bool CheckResources();

static bool foreground = false;

static void usage()
{
  kBareosVersion.printCopyright(stderr, 2000);
  fprintf(
      stderr,
      _("Usage: bareos-fd [options]\n"
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
        "        -xs         print configuration file schema in JSON format "
        "and exit\n"
        "        -?          print this message.\n"
        "\n"));

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

int main(int argc, char* argv[])
{
  int ch;
  bool test_config = false;
  bool export_config = false;
  bool export_config_schema = false;
  bool keep_readall_caps = false;
  char* uid = NULL;
  char* gid = NULL;

  setlocale(LC_ALL, "");
  tzset();
  bindtextdomain("bareos", LOCALEDIR);
  textdomain("bareos");

  InitStackDump();
  MyNameIs(argc, argv, "bareos-fd");
  InitMsg(NULL, NULL);
  daemon_start_time = time(NULL);

  while ((ch = getopt(argc, argv, "bc:d:fg:kmrstu:vx:z:?")) != -1) {
    switch (ch) {
      case 'b':
        backup_only_mode = true;
        break;

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

      case 'g': /* set group */
        gid = optarg;
        break;

      case 'k':
        keep_readall_caps = true;
        break;

      case 'm': /* print kaboom output */
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

      case 'u': /* set userid */
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

  if (argc) {
    if (configfile != NULL) free(configfile);
    configfile = strdup(*argv);
    argc--;
    argv++;
  }
  if (argc) { usage(); }

  if (!uid && keep_readall_caps) {
    Emsg0(M_ERROR_TERM, 0, _("-k option has no meaning without -u option.\n"));
  }

  /*
   * See if we want to drop privs.
   */
  if (geteuid() == 0) { drop(uid, gid, keep_readall_caps); }

  if (!no_signals) {
    InitSignals(TerminateFiled);
  } else {
    /*
     * This reduces the number of signals facilitating debugging
     */
    watchdog_sleep_time = 120; /* long timeout for debugging */
  }

  if (export_config_schema) {
    PoolMem buffer;

    my_config = InitFdConfig(configfile, M_ERROR_TERM);
    PrintConfigSchemaJson(buffer);
    printf("%s\n", buffer.c_str());
    goto bail_out;
  }

  my_config = InitFdConfig(configfile, M_ERROR_TERM);
  my_config->ParseConfig();

  if (export_config) {
    my_config->DumpResources(PrintMessage, NULL);
    goto bail_out;
  }

  if (!foreground && !test_config) {
    daemon_start();
    InitStackDump(); /* set new pid */
  }

  if (InitCrypto() != 0) {
    Emsg0(M_ERROR, 0, _("Cryptography library initialization failed.\n"));
    TerminateFiled(1);
  }

  if (!CheckResources()) {
    Emsg1(M_ERROR, 0, _("Please correct configuration file: %s\n"),
          my_config->get_base_config_path().c_str());
    TerminateFiled(1);
  }

  SetWorkingDirectory(me->working_directory);

#if defined(HAVE_WIN32)
  if (me->compatible) {
    Win32SetCompatible();
  } else {
    Win32ClearCompatible();
  }
#endif

  if (test_config) { TerminateFiled(0); }

  SetThreadConcurrency(me->MaxConcurrentJobs * 2 + 10);

  /* Maximum 1 daemon at a time */
  CreatePidFile(me->pid_directory, "bareos-fd",
                GetFirstPortHostOrder(me->FDaddrs));
  ReadStateFile(me->working_directory, "bareos-fd",
                GetFirstPortHostOrder(me->FDaddrs));

  LoadFdPlugins(me->plugin_directory, me->plugin_names);

  InitJcrChain();
  if (!no_signals) {
    StartWatchdog(); /* start watchdog thread */
    if (me->jcr_watchdog_time) {
      InitJcrSubsystem(
          me->jcr_watchdog_time); /* start JobControlRecord watchdogs etc. */
    }
  }

  /*
   * if configured, start threads and connect to Director.
   */
  StartConnectToDirectorThreads();

  /*
   * start socket server to listen for new connections.
   */
  StartSocketServer(me->FDaddrs);

  TerminateFiled(0);

bail_out:
  exit(0);
}

namespace filedaemon {

void TerminateFiled(int sig)
{
  static bool already_here = false;

  if (already_here) {
    Bmicrosleep(2, 0); /* yield */
    exit(1);           /* prevent loops */
  }
  already_here = true;
  debug_level = 0; /* turn off debug */
  StopWatchdog();

  StopConnectToDirectorThreads(true);
  StopSocketServer(true);

  UnloadFdPlugins();
  FlushMntentCache();
  WriteStateFile(me->working_directory, "bareos-fd",
                 GetFirstPortHostOrder(me->FDaddrs));
  DeletePidFile(me->pid_directory, "bareos-fd",
                GetFirstPortHostOrder(me->FDaddrs));

  if (configfile != NULL) { free(configfile); }

  if (debug_level > 0) { PrintMemoryPoolStats(); }
  if (my_config) {
    delete my_config;
    my_config = NULL;
  }
  TermMsg();
  CleanupCrypto();
  CloseMemoryPool(); /* release free memory in pool */
  exit(sig);
}
} /* namespace filedaemon */

/**
 * Make a quick check to see that we have all the
 * resources needed.
 */
static bool CheckResources()
{
  bool OK = true;
  DirectorResource* director;
  const std::string& configfile = my_config->get_base_config_path();

  LockRes(my_config);

  me = (ClientResource*)my_config->GetNextRes(R_CLIENT, NULL);
  my_config->own_resource_ = me;
  if (!me) {
    Emsg1(M_FATAL, 0,
          _("No File daemon resource defined in %s\n"
            "Without that I don't know who I am :-(\n"),
          configfile.c_str());
    OK = false;
  } else {
    /*
     * Sanity check.
     */
    if (me->MaxConnections < (2 * me->MaxConcurrentJobs)) {
      me->MaxConnections = (2 * me->MaxConcurrentJobs) + 2;
    }

    if (my_config->GetNextRes(R_CLIENT, (BareosResource*)me) != NULL) {
      Emsg1(M_FATAL, 0, _("Only one Client resource permitted in %s\n"),
            configfile.c_str());
      OK = false;
    }
    MyNameIs(0, NULL, me->resource_name_);
    if (!me->messages) {
      me->messages = (MessagesResource*)my_config->GetNextRes(R_MSGS, NULL);
      if (!me->messages) {
        Emsg1(M_FATAL, 0, _("No Messages resource defined in %s\n"),
              configfile.c_str());
        OK = false;
      }
    }
    if (me->pki_encrypt || me->pki_sign) {
#ifndef HAVE_CRYPTO
      Jmsg(NULL, M_FATAL, 0,
           _("PKI encryption/signing enabled but not compiled into Bareos.\n"));
      OK = false;
#endif
    }

    /* pki_encrypt implies pki_sign */
    if (me->pki_encrypt) { me->pki_sign = true; }

    if ((me->pki_encrypt || me->pki_sign) && !me->pki_keypair_file) {
      Emsg2(M_FATAL, 0,
            _("\"PKI Key Pair\" must be defined for File"
              " daemon \"%s\" in %s if either \"PKI Sign\" or"
              " \"PKI Encrypt\" are enabled.\n"),
            me->resource_name_, configfile.c_str());
      OK = false;
    }

    /* If everything is well, attempt to initialize our public/private keys */
    if (OK && (me->pki_encrypt || me->pki_sign)) {
      char* filepath = nullptr;
      /* Load our keypair */
      me->pki_keypair = crypto_keypair_new();
      if (!me->pki_keypair) {
        Emsg0(M_FATAL, 0, _("Failed to allocate a new keypair object.\n"));
        OK = false;
      } else {
        if (!CryptoKeypairLoadCert(me->pki_keypair, me->pki_keypair_file)) {
          Emsg2(M_FATAL, 0,
                _("Failed to load public certificate for File"
                  " daemon \"%s\" in %s.\n"),
                me->resource_name_, configfile.c_str());
          OK = false;
        }

        if (!CryptoKeypairLoadKey(me->pki_keypair, me->pki_keypair_file, NULL,
                                  NULL)) {
          Emsg2(M_FATAL, 0,
                _("Failed to load private key for File"
                  " daemon \"%s\" in %s.\n"),
                me->resource_name_, configfile.c_str());
          OK = false;
        }
      }

      /*
       * Trusted Signers. We're always trusted.
       */
      me->pki_signers = new alist(10, not_owned_by_alist);
      if (me->pki_keypair) {
        me->pki_signers->append(crypto_keypair_dup(me->pki_keypair));
      }

      /* If additional signing public keys have been specified, load them up */
      if (me->pki_signing_key_files) {
        foreach_alist (filepath, me->pki_signing_key_files) {
          X509_KEYPAIR* keypair;

          keypair = crypto_keypair_new();
          if (!keypair) {
            Emsg0(M_FATAL, 0, _("Failed to allocate a new keypair object.\n"));
            OK = false;
          } else {
            if (CryptoKeypairLoadCert(keypair, filepath)) {
              me->pki_signers->append(keypair);

              /* Attempt to load a private key, if available */
              if (CryptoKeypairHasKey(filepath)) {
                if (!CryptoKeypairLoadKey(keypair, filepath, NULL, NULL)) {
                  Emsg3(M_FATAL, 0,
                        _("Failed to load private key from file %s for File"
                          " daemon \"%s\" in %s.\n"),
                        filepath, me->resource_name_, configfile.c_str());
                  OK = false;
                }
              }

            } else {
              Emsg3(M_FATAL, 0,
                    _("Failed to load trusted signer certificate"
                      " from file %s for File daemon \"%s\" in %s.\n"),
                    filepath, me->resource_name_, configfile.c_str());
              OK = false;
            }
          }
        }
      }

      /*
       * Crypto recipients. We're always included as a recipient.
       * The symmetric session key will be encrypted for each of these readers.
       */
      me->pki_recipients = new alist(10, not_owned_by_alist);
      if (me->pki_keypair) {
        me->pki_recipients->append(crypto_keypair_dup(me->pki_keypair));
      }


      /* If additional keys have been specified, load them up */
      if (me->pki_master_key_files) {
        foreach_alist (filepath, me->pki_master_key_files) {
          X509_KEYPAIR* keypair;

          keypair = crypto_keypair_new();
          if (!keypair) {
            Emsg0(M_FATAL, 0, _("Failed to allocate a new keypair object.\n"));
            OK = false;
          } else {
            if (CryptoKeypairLoadCert(keypair, filepath)) {
              me->pki_recipients->append(keypair);
            } else {
              Emsg3(M_FATAL, 0,
                    _("Failed to load master key certificate"
                      " from file %s for File daemon \"%s\" in %s.\n"),
                    filepath, me->resource_name_, configfile.c_str());
              OK = false;
            }
          }
        }
      }
    }
  }


  /* Verify that a director record exists */
  LockRes(my_config);
  director = (DirectorResource*)my_config->GetNextRes(R_DIRECTOR, NULL);
  UnlockRes(my_config);
  if (!director) {
    Emsg1(M_FATAL, 0, _("No Director resource defined in %s\n"),
          configfile.c_str());
    OK = false;
  }

  UnlockRes(my_config);

  if (OK) {
    CloseMsg(NULL);              /* close temp message handler */
    InitMsg(NULL, me->messages); /* open user specified message handler */
    if (me->secure_erase_cmdline) {
      SetSecureEraseCmdline(me->secure_erase_cmdline);
    }
    if (me->log_timestamp_format) {
      SetLogTimestampFormat(me->log_timestamp_format);
    }
  }

  return OK;
}
