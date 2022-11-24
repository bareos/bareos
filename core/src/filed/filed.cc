/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2010 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2023 Bareos GmbH & Co. KG

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
 * Bareos File Daemon
 */

#include "include/bareos.h"
#include "filed/filed.h"
#include "filed/filed_globals.h"
#include "filed/dir_cmd.h"
#include "filed/socket_server.h"
#include "lib/cli.h"
#include "lib/mntent_cache.h"
#include "lib/daemon.h"
#include "lib/bnet_network_dump.h"
#include "lib/bsignal.h"
#include "lib/parse_conf.h"
#include "lib/watchdog.h"
#include "lib/util.h"
#include "lib/address_conf.h"
#include "lib/alist.h"
#include "filed/filed_utils.h"

using namespace filedaemon;

/* Imported Functions */
extern void* handle_connection_request(void* dir_sock);
extern bool PrintMessage(void* sock, const char* fmt, ...);


static std::string pidfile_path{};

/*********************************************************************
 *
 *  Main Bareos Unix Client Program
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
  MyNameIs(argc, argv, "bareos-fd");
  InitMsg(nullptr, nullptr);
  daemon_start_time = time(nullptr);

  CLI::App fd_app;
  InitCLIApp(fd_app, "The Bareos File Daemon.", 2000);

  fd_app.add_flag("-b,--backup-only", backup_only_mode, "Backup only mode.");

  fd_app
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

  AddDebugOptions(fd_app);

  bool foreground = false;
  CLI::Option* foreground_option = fd_app.add_flag(
      "-f,--foreground", foreground, "Run in foreground (for debugging).");

  std::string user;
  std::string group;
  AddUserAndGroupOptions(fd_app, user, group);

  bool keep_readall_caps = false;
  fd_app.add_flag("-k,--keep-readall", keep_readall_caps,
                  "Keep readall capabilities.");


  fd_app.add_flag("-m,--print-kaboom", prt_kaboom,
                  "Print kaboom output (for debugging)");

  bool test_config = false;
  auto testconfig_option = fd_app.add_flag(
      "-t,--test-config", test_config, "Test - read configuration and exit.");

#if !defined(HAVE_WIN32)
  fd_app
      .add_option("-p,--pid-file", pidfile_path,
                  "Full path to pidfile (default: none)")
      ->excludes(foreground_option)
      ->excludes(testconfig_option)
      ->type_name("<file>");
#else
  // to silence unused variable error on windows
  (void)testconfig_option;
  (void)foreground_option;
#endif

  fd_app.add_flag("-r,--restore-only", restore_only_mode, "Restore only mode.");

  fd_app.add_flag("-s,--no-signals", no_signals, "No signals (for debugging).");

  AddVerboseOption(fd_app);

  bool export_config = false;
  CLI::Option* xc = fd_app.add_flag("--xc,--export-config", export_config,
                                    "Print configuration resources and exit.");


  bool export_config_schema = false;
  fd_app
      .add_flag("--xs,--export-schema", export_config_schema,
                "Print configuration schema in JSON format and exit")
      ->excludes(xc);

  AddDeprecatedExportOptionsHelp(fd_app);

  AddNetworkDebuggingOption(fd_app);

  CLI11_PARSE(fd_app, argc, argv);

  if (user.empty() && keep_readall_caps) {
    Emsg0(M_ERROR_TERM, 0, _("-k option has no meaning without -u option.\n"));
  }

  int pidfile_fd = -1;
#if !defined(HAVE_WIN32)
  if (!foreground && !test_config && !pidfile_path.empty()) {
    pidfile_fd = CreatePidFile("bareos-fd", pidfile_path.c_str());
  }
#endif

  // See if we want to drop privs.
  char* uid = nullptr;
  if (!user.empty()) { uid = user.data(); }

  char* gid = nullptr;
  if (!group.empty()) { gid = group.data(); }

  if (geteuid() == 0) {
    drop(uid, gid, keep_readall_caps);
  } else if (uid || gid) {
    Emsg2(M_ERROR_TERM, 0,
          _("The commandline options indicate to run as specified user/group, "
            "but program was not started with required root privileges.\n"));
  }

  if (!no_signals) {
    InitSignals(TerminateFiled);
  } else {
    // This reduces the number of signals facilitating debugging
    watchdog_sleep_time = 120; /* long timeout for debugging */
  }

  if (export_config_schema) {
    PoolMem buffer;

    my_config = InitFdConfig(configfile, M_ERROR_TERM);
    PrintConfigSchemaJson(buffer);
    printf("%s\n", buffer.c_str());

    exit(0);
  }

  my_config = InitFdConfig(configfile, M_ERROR_TERM);
  my_config->ParseConfig();

  if (export_config) {
    my_config->DumpResources(PrintMessage, nullptr);

    exit(0);
  }

  if (!CheckResources()) {
    Emsg1(M_ERROR, 0, _("Please correct configuration file: %s\n"),
          my_config->get_base_config_path().c_str());
    TerminateFiled(1);
  }

  if (my_config->HasWarnings()) {
    // messaging not initialized, so Jmsg with  M_WARNING doesn't work
    fprintf(stderr, _("There are configuration warnings:\n"));
    for (auto& warning : my_config->GetWarnings()) {
      fprintf(stderr, " * %s\n", warning.c_str());
    }
  }

  if (!foreground && !test_config) {
    daemon_start("bareos-fd", pidfile_fd, pidfile_path);
    InitStackDump(); /* set new pid */
  }

  if (InitCrypto() != 0) {
    Emsg0(M_ERROR, 0, _("Cryptography library initialization failed.\n"));
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

  /* Maximum 1 daemon at a time */
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

  // if configured, start threads and connect to Director.
  StartConnectToDirectorThreads();

  // start socket server to listen for new connections.
  StartSocketServer(me->FDaddrs);

  TerminateFiled(0);

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
  DeletePidFile(pidfile_path);

  if (configfile != nullptr) { free(configfile); }

  if (my_config) {
    delete my_config;
    my_config = nullptr;
  }
  TermMsg();
  CleanupCrypto();
  exit(sig);
}


} /* namespace filedaemon */
/**
 * Make a quick check to see that we have all the
 * resources needed.
 */
bool CheckResources()
{
  bool OK = true;
  DirectorResource* director;
  const std::string& configfile = my_config->get_base_config_path();

  ResLocker _{my_config};

  me = (ClientResource*)my_config->GetNextRes(R_CLIENT, nullptr);
  my_config->own_resource_ = me;
  if (!me) {
    Emsg1(M_FATAL, 0,
          _("No File daemon resource defined in %s\n"
            "Without that I don't know who I am :-(\n"),
          configfile.c_str());
    OK = false;
  } else {
    if (my_config->GetNextRes(R_CLIENT, (BareosResource*)me) != nullptr) {
      Emsg1(M_FATAL, 0, _("Only one Client resource permitted in %s\n"),
            configfile.c_str());
      OK = false;
    }
    MyNameIs(0, nullptr, me->resource_name_);
    if (!me->messages) {
      me->messages = (MessagesResource*)my_config->GetNextRes(R_MSGS, nullptr);
      if (!me->messages) {
        Emsg1(M_FATAL, 0, _("No Messages resource defined in %s\n"),
              configfile.c_str());
        OK = false;
      }
    }
    if (me->pki_encrypt || me->pki_sign) {
#ifndef HAVE_CRYPTO
      Jmsg(nullptr, M_FATAL, 0,
           _("PKI encryption/signing enabled but not compiled into Bareos.\n"));
      OK = false;
#endif
    }

    /* pki_encrypt fd_implies pki_sign */
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
      const char* filepath = nullptr;
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

        if (!CryptoKeypairLoadKey(me->pki_keypair, me->pki_keypair_file,
                                  nullptr, nullptr)) {
          Emsg2(M_FATAL, 0,
                _("Failed to load private key for File"
                  " daemon \"%s\" in %s.\n"),
                me->resource_name_, configfile.c_str());
          OK = false;
        }
      }

      // Trusted Signers. We're always trusted.
      me->pki_signers = new alist<X509_KEYPAIR*>(10, not_owned_by_alist);
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
                if (!CryptoKeypairLoadKey(keypair, filepath, nullptr,
                                          nullptr)) {
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

      /* Crypto recipients. We're always included as a recipient.
       * The symmetric session key will be encrypted for each of these readers.
       */
      me->pki_recipients = new alist<X509_KEYPAIR*>(10, not_owned_by_alist);
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
  director = (DirectorResource*)my_config->GetNextRes(R_DIRECTOR, nullptr);

  if (!director) {
    Emsg1(M_FATAL, 0, _("No Director resource defined in %s\n"),
          configfile.c_str());
    OK = false;
  }

  if (OK) {
    CloseMsg(nullptr);              /* close temp message handler */
    InitMsg(nullptr, me->messages); /* open user specified message handler */
    if (me->secure_erase_cmdline) {
      SetSecureEraseCmdline(me->secure_erase_cmdline);
    }
    if (me->log_timestamp_format) {
      SetLogTimestampFormat(me->log_timestamp_format);
    }
  }

  return OK;
}
