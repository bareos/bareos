/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2010 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
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
 * Bareos File Daemon
 */

#if !defined(HAVE_MSVC)
#  include <fcntl.h>
#  include <unistd.h>
#endif
#include "include/bareos.h"
#include "include/exit_codes.h"
#include "filed/dir_cmd.h"
#include "filed/filed.h"
#include "filed/filed_globals.h"
#include "filed/filed_utils.h"
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
#include "lib/berrno.h"

using namespace filedaemon;

/* Imported Functions */
extern void* handle_connection_request(void* dir_sock);

static bool use_signal_pipe_termination = false;
#if !defined(HAVE_WIN32)
static int termination_pipe_fds[2] = {-1, -1};
static volatile sig_atomic_t termination_signal = 0;
#endif

static void CloseTerminationPipe()
{
#if !defined(HAVE_WIN32)
  if (termination_pipe_fds[0] >= 0) {
    close(termination_pipe_fds[0]);
    termination_pipe_fds[0] = -1;
  }
  if (termination_pipe_fds[1] >= 0) {
    close(termination_pipe_fds[1]);
    termination_pipe_fds[1] = -1;
  }
#endif
}

#if !defined(HAVE_WIN32)
static bool SetupTerminationPipe()
{
  if (pipe(termination_pipe_fds) != 0) {
    BErrNo be;
    Emsg1(M_WARNING, 0,
          T_("Failed to create termination notification pipe: %s\n"),
          be.bstrerror());
    return false;
  }

  int flags = fcntl(termination_pipe_fds[1], F_GETFL);
  if (flags < 0
      || fcntl(termination_pipe_fds[1], F_SETFL, flags | O_NONBLOCK) != 0) {
    BErrNo be;
    Emsg1(M_WARNING, 0,
          T_("Failed to configure termination notification pipe: %s\n"),
          be.bstrerror());
    CloseTerminationPipe();
    return false;
  }

  return true;
}

static void NotifyTerminationViaPipe(int sig)
{
  termination_signal = sig;
  if (termination_pipe_fds[1] >= 0) {
    const unsigned char signal_byte = 1;
    [[maybe_unused]] auto ignored
        = write(termination_pipe_fds[1], &signal_byte, sizeof(signal_byte));
  }
}
#endif

static bool IsClientInitiatedOnlyModeConfigured()
{
  BareosResource* resource = nullptr;
  bool has_outbound_director = false;
  bool has_inbound_director = false;

  while ((resource = my_config->GetNextRes(R_DIRECTOR, resource)) != nullptr) {
    auto* director = dynamic_cast<DirectorResource*>(resource);
    if (!director) { continue; }

    if (director->conn_from_fd_to_dir) { has_outbound_director = true; }
    if (director->conn_from_dir_to_fd) { has_inbound_director = true; }
  }

  return has_outbound_director && !has_inbound_director;
}

static void WaitUntilTerminated()
{
  // Without a listening socket server we still need to keep the daemon process
  // alive until the process receives a termination signal.
#if !defined(HAVE_WIN32)
  if (use_signal_pipe_termination && termination_pipe_fds[0] >= 0) {
    unsigned char signal_byte;
    while (read(termination_pipe_fds[0], &signal_byte, sizeof(signal_byte))
           < 0) {
      if (errno != EINTR) { break; }
    }
  } else {
    for (;;) { pause(); }
  }
#else
  for (;;) { Bmicrosleep(30, 0); }
#endif
}


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
            if (g_filed_configfile != nullptr) { free(g_filed_configfile); }
            g_filed_configfile = strdup(val.front().c_str());
            return true;
          },
          "Use <path> as configuration file or directory.")
      ->check(CLI::ExistingPath)
      ->type_name("<path>");

  AddDebugOptions(fd_app);

  bool foreground = false;
  [[maybe_unused]] CLI::Option* foreground_option = fd_app.add_flag(
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
  [[maybe_unused]] auto testconfig_option = fd_app.add_flag(
      "-t,--test-config", test_config, "Test - read configuration and exit.");
#ifndef HAVE_WIN32
  fd_app
      .add_option("-p,--pid-file", pidfile_path,
                  "Full path to pidfile (default: none)")
      ->excludes(foreground_option)
      ->excludes(testconfig_option)
      ->type_name("<file>");
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

  ParseBareosApp(fd_app, argc, argv);

  if (user.empty() && keep_readall_caps) {
    Emsg0(M_ERROR_TERM, 0, T_("-k option has no meaning without -u option.\n"));
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
          T_("The commandline options indicate to run as specified user/group, "
             "but program was not started with required root privileges.\n"));
  }

  if (export_config_schema) {
    PoolMem buffer;

    my_config = InitFdConfig(g_filed_configfile, M_ERROR_TERM);
    PrintConfigSchemaJson(buffer);
    printf("%s\n", buffer.c_str());

    exit(BEXIT_SUCCESS);
  }

  my_config = InitFdConfig(g_filed_configfile, M_CONFIG_ERROR);
  my_config->ParseConfigOrExit();

  if (export_config) {
    my_config->DumpResources(PrintMessage, nullptr);

    exit(BEXIT_SUCCESS);
  }

  if (!CheckResources()) {
    Emsg1(M_ERROR, 0, T_("Please correct configuration file: %s\n"),
          my_config->get_base_config_path().c_str());
    TerminateFiled(BEXIT_CONFIG_ERROR);
  }

  if (my_config->HasWarnings()) {
    // messaging not initialized, so Jmsg with  M_WARNING doesn't work
    fprintf(stderr, T_("There are configuration warnings:\n"));
    for (auto& warning : my_config->GetWarnings()) {
      fprintf(stderr, " * %s\n", warning.c_str());
    }
  }

  const bool client_initiated_only_mode = IsClientInitiatedOnlyModeConfigured();

  if (!no_signals) {
#if !defined(HAVE_WIN32)
    if (client_initiated_only_mode && SetupTerminationPipe()) {
      InitSignals(NotifyTerminationViaPipe);
      use_signal_pipe_termination = true;
    } else {
      InitSignals(TerminateFiled);
    }
#else
    InitSignals(TerminateFiled);
#endif
  }

  if (!foreground && !test_config) {
    daemon_start("bareos-fd", pidfile_fd, pidfile_path);
    InitStackDump(); /* set new pid */
  }

  if (InitCrypto() != 0) {
    Emsg0(M_ERROR, 0, T_("Cryptography library initialization failed.\n"));
    TerminateFiled(1);
  }

  SetWorkingDirectory(me->working_directory);

  if (test_config) { TerminateFiled(0); }

  /* Maximum 1 daemon at a time */
  ReadStateFile(me->working_directory, "bareos-fd",
                GetFirstPortHostOrder(me->FDaddrs));
  LoadFdPlugins(me->plugin_directory, me->plugin_names);

  LockJcrChain();
  InitJcrChain();
  UnlockJcrChain();
  if (!no_signals) {
    StartWatchdog(); /* start watchdog thread */
    if (me->jcr_watchdog_time) {
      InitJcrSubsystem(
          me->jcr_watchdog_time); /* start JobControlRecord watchdogs etc. */
    }
  }

  // if configured, start threads and connect to Director.
  StartConnectToDirectorThreads();

  // Start socket server only when inbound director connections are enabled.
  if (client_initiated_only_mode) {
    Pmsg0(000, T_("Client-initiated-only mode detected; "
                  "skipping socket listener startup.\n"));
    WaitUntilTerminated();
    if (use_signal_pipe_termination) {
#if !defined(HAVE_WIN32)
      TerminateFiled(termination_signal ? termination_signal : BEXIT_SUCCESS);
#else
      TerminateFiled(BEXIT_SUCCESS);
#endif
    }
  } else {
    StartSocketServer(me->FDaddrs);
  }

  TerminateFiled(BEXIT_SUCCESS);
  return BEXIT_SUCCESS;
}

namespace filedaemon {

void TerminateFiled(int sig)
{
  static bool already_here = false;

  if (already_here) {
    Bmicrosleep(2, 0);   /* yield */
    exit(BEXIT_FAILURE); /* prevent loops */
  }
  already_here = true;
  debug_level = 0; /* turn off debug */
  StopWatchdog();

  StopConnectToDirectorThreads(true);
  StopSocketServer();

  UnloadFdPlugins();
  FlushMntentCache();
  if (me) {
    WriteStateFile(me->working_directory, "bareos-fd",
                   GetFirstPortHostOrder(me->FDaddrs));
  }
  CloseTerminationPipe();
  DeletePidFile(pidfile_path);

  if (g_filed_configfile != nullptr) { free(g_filed_configfile); }

  if (my_config) {
    delete my_config;
    my_config = nullptr;
  }
  TermMsg();
  CleanupCrypto();
  exit(sig);
}


} /* namespace filedaemon */
