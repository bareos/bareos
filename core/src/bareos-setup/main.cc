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
 * Usage: bareos-setup [--port PORT] [--listen ADDRESS] [--no-browser]
 *                     [--tui] [--dry]
 */
#include <cctype>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

#include <CLI/CLI.hpp>
#include <unistd.h>

#include "http_server.h"
#include "setup_session.h"
#include "setup_steps.h"
#include "tui_wizard.h"
#include "command_runner.h"
#include "os_detector.h"

/** Percent-encode a string for safe use as a URL query value. The setup
 * token alphabet includes characters (e.g. '#', '@') that are otherwise
 * reserved in URLs, so the raw token must never be embedded verbatim. */
static std::string UrlEncode(const std::string& value)
{
  std::ostringstream encoded;
  encoded.fill('0');
  encoded << std::hex;
  for (unsigned char c : value) {
    if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      encoded << c;
    } else {
      encoded << '%' << std::setw(2) << std::uppercase << int(c)
              << std::nouppercase;
    }
  }
  return encoded.str();
}

static void OpenBrowser(const std::string& display_host,
                        int port,
                        const std::string& token)
{
  std::string url = "http://" + display_host + ":" + std::to_string(port)
                    + "/?token=" + UrlEncode(token);
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
  app.footer(std::string("Version: ") + BAREOS_FULL_VERSION);

  int port = 19101;
  app.add_option("--port,-p", port, "TCP port to listen on")
      ->default_val(19101);

  std::string listen_address = "127.0.0.1";
  app.add_option("--listen,-l", listen_address,
                 "IPv4 address to listen on. Defaults to 127.0.0.1 "
                 "(loopback only, the secure default). Use 0.0.0.0 to "
                 "listen on all interfaces, or a specific interface "
                 "address, so the wizard can be reached from other "
                 "hosts -- only do this on a trusted network, since "
                 "the wizard executes privileged commands.")
      ->default_val("127.0.0.1");

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

  std::cout << "Bareos Setup Wizard " << BAREOS_FULL_VERSION;
  if (dry_run) std::cout << " [dry-run]";
  std::cout << "\n";

  const bool listen_all_interfaces = (listen_address == "0.0.0.0");
  if (!tui && listen_address != "127.0.0.1" && listen_address != "localhost") {
    std::cerr << "Warning: listening on " << listen_address
              << " instead of the "
              << "default 127.0.0.1. The setup wizard executes privileged "
                 "commands as root; only do this on a trusted network. The "
                 "setup token in the URL remains the only access control.\n";
  }
  if (listen_address == "localhost") listen_address = "127.0.0.1";

  // Fail fast with a clear message if a required external tool (curl,
  // systemctl, the detected package manager, ...) is missing, instead
  // of surfacing a raw exec failure deep inside an install step later.
  // Dry-run only previews commands and never executes anything, so the
  // check is skipped in that mode.
  if (!dry_run) {
    const auto os = DetectOs();
    const auto missing = MissingRequiredTools(os.pkg_mgr);
    if (!missing.empty()) {
      std::cerr << "Fatal: required tool(s) not found in PATH:";
      for (const auto& tool : missing) std::cerr << " " << tool;
      std::cerr << "\nInstall the missing tool(s) and try again.\n";
      return 1;
    }
  }

  // Privileged steps (package install, service management, catalog
  // creation, ...) always execute as root. When not already root, obtain
  // a sudo ticket once here -- interactively, on the real terminal -- so
  // the user is prompted for their password exactly once, instead of
  // requiring sudoers configuration (e.g. NOPASSWD) up front. A background
  // thread then keeps the ticket alive for the rest of the run.
  if (!dry_run && !IsRoot()) {
    std::cout << "Administrator privileges are required to continue.\n";
    if (!PrimeSudoTicket()) {
      std::cerr << "Fatal: could not obtain sudo privileges. Run "
                   "bareos-setup as root, or as a user allowed to "
                   "authenticate with sudo from this terminal.\n";
      return 1;
    }
    StartSudoKeepAlive();
  }

  // TUI mode: run interactively in the terminal, no HTTP server.
  if (tui) return RunTuiWizard(dry_run);

  const std::string setup_token = GenerateSetupSecret(32);
  // When listening on all interfaces, "0.0.0.0" itself is not a URL a
  // browser can open -- use "localhost" for the displayed/opened URL,
  // since the server also always accepts connections on loopback.
  // Likewise, keep showing the familiar "localhost" for the default
  // loopback-only setup, matching prior behavior.
  const std::string display_host
      = (listen_all_interfaces || listen_address == "127.0.0.1")
          ? "localhost"
          : listen_address;
  const std::string setup_url = "http://" + display_host + ":"
                                + std::to_string(port)
                                + "/?token=" + UrlEncode(setup_token);

  if (no_browser) {
    std::cout << "Open this URL in your browser: " << setup_url << "\n";
  }

  // Fork before starting the server so the child opens the browser
  // after the parent has started listening.
  pid_t child = -1;
  if (!no_browser) {
    child = fork();
    if (child == 0) {
      // Child: wait briefly then open browser
      sleep(1);
      OpenBrowser(display_host, port, setup_token);
      _exit(0);
    }
  }

  try {
    RunHttpServer(listen_address, port, setup_token,
                  [dry_run](int fd) { RunSetupSession(fd, dry_run); });
  } catch (const std::exception& e) {
    std::cerr << "Fatal: " << e.what() << "\n";
    return 1;
  }
  return 0;
}
