/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2024-2026 Bareos GmbH & Co. KG

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
/**
 * @file
 * bareos-setup: single-binary installation wizard.
 *
 * Usage: bareos-setup [--port PORT] [--no-browser] [--tui] [--dry]
 */
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

#include <CLI/CLI.hpp>
#include <unistd.h>

#include "http_server.h"
#include "setup_session.h"
#include "tui_wizard.h"

static void OpenBrowser(int port)
{
  std::string url = "http://localhost:" + std::to_string(port) + "/";
  // Try common browser launchers in order
  for (const char* cmd : {"xdg-open", "open", "sensible-browser"}) {
    if (execlp(cmd, cmd, url.c_str(), nullptr) == 0) return;
    // execlp only returns on failure
  }
  std::cerr << "Could not open browser automatically.\n"
            << "Open this URL manually: " << url << "\n";
}

int main(int argc, char* argv[])
{
  CLI::App app{"Bareos Setup Wizard", "bareos-setup"};
  app.set_version_flag("--version", BAREOS_FULL_VERSION);

  int port = 19101;
  app.add_option("--port,-p", port, "TCP port to listen on")
      ->default_val(19101);

  bool no_browser = false;
  app.add_flag("--no-browser", no_browser,
               "Do not open the browser automatically");

  bool dry_run = false;
  app.add_flag("--dry", dry_run,
               "Dry-run mode: print commands instead of executing them");

  bool tui = false;
  app.add_flag("--tui", tui,
               "Run as interactive terminal wizard instead of web UI");

  CLI11_PARSE(app, argc, argv);

  if (port < 1 || port > 65535) {
    std::cerr << "Fatal: --port must be in the range 1..65535\n";
    return 1;
  }

  std::cout << "Bareos Setup Wizard " << BAREOS_FULL_VERSION;
  if (dry_run) std::cout << " [dry-run]";
  std::cout << "\n";

  // TUI mode: run interactively in the terminal, no HTTP server.
  if (tui) return RunTuiWizard(dry_run);

  // Fork before starting the server so the child opens the browser
  // after the parent has started listening.
  pid_t child = -1;
  if (!no_browser) {
    child = fork();
    if (child == 0) {
      // Child: wait briefly then open browser
      sleep(1);
      OpenBrowser(port);
      _exit(0);
    }
  }

  try {
    RunHttpServer(port, [dry_run](int fd) { RunSetupSession(fd, dry_run); });
  } catch (const std::exception& e) {
    std::cerr << "Fatal: " << e.what() << "\n";
    return 1;
  }
  return 0;
}
