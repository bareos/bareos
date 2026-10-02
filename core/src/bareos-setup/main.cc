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
 *        bareos-setup --unattended [--override-repo-url URL] [--without-webui]
 *                     [--extra-package PACKAGE] [--dry]
 */
#include <array>
#include <cctype>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

#include <arpa/inet.h>
#include <CLI/CLI.hpp>
#include <netinet/in.h>
#include <unistd.h>

#include "command_runner.h"
#include "http_server.h"
#include "os_detector.h"
#include "setup_session.h"
#include "setup_steps.h"
#include "tui_wizard.h"

namespace {

constexpr char kIpv4LoopbackAddress[] = "127.0.0.1";
constexpr char kIpv6LoopbackAddress[] = "::1";

}  // namespace

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

static void OpenBrowser(SetupContext& context,
                        const std::string& url_host,
                        int port,
                        const std::string& token)
{
  std::string url = "http://" + url_host + ":" + std::to_string(port)
                    + "/?token=" + UrlEncode(token);
  const std::array<SetupCommand, 3> commands{XdgOpen({url}), Open({url}),
                                             SensibleBrowser({url})};
  for (const auto& command : commands) {
    if (!context.IsToolAvailable(command.tool)) continue;
    try {
      if (context.Run(command, false, [](std::string_view, std::string_view) {})
          == 0) {
        return;
      }
    } catch (const std::runtime_error& error) {
      std::cerr << "Could not launch browser: " << error.what() << "\n";
    }
  }
  std::cerr << "Could not open browser automatically.\n"
            << "Open this URL manually: " << url << "\n";
}

int main(int argc, char* argv[])
{
  CLI::App app{
      "Configure a Bareos installation using a temporary web UI "
      "or an interactive terminal wizard, or install unattended.",
      "bareos-setup"};
  app.set_version_flag("--version", BAREOS_FULL_VERSION);
  app.footer(std::string("Version: ") + BAREOS_FULL_VERSION);

  int port = 19101;
  app.add_option("--port,-p", port, "TCP port to listen on")
      ->default_val(19101)
      ->check(CLI::Range(1, 65535));

  std::string listen_address;
  const std::string listen_description
      = std::string("IPv4 or IPv6 address for the temporary setup web UI "
                      "listener. Defaults to ")
        + kIpv4LoopbackAddress
        + " (loopback only, the secure default). To reach the wizard from "
            "another host, prefer forwarding the port over SSH "
            "(ssh -L <port>:"
        + kIpv4LoopbackAddress
        + ":<port> root@host) rather than binding to a reachable interface: "
            "the setup wizard speaks plain HTTP and executes privileged "
            "commands.";
  CLI::Option* listen_option
      = app.add_option("--listen,-l", listen_address, listen_description)
            ->default_val(kIpv4LoopbackAddress);

  bool no_browser = false;
  app.add_flag("--no-browser", no_browser,
               "Do not open the browser automatically");

  bool dry_run = false;
  app.add_flag("--dry", dry_run,
               "Print the commands instead of executing them");

  bool tui = false;
  CLI::Option* tui_option = app.add_flag(
      "--tui", tui, "Run as interactive terminal wizard instead of web UI");
  listen_option->excludes(tui_option);
  tui_option->excludes(listen_option);

  bool unattended = false;
  auto* unattended_option = app.add_flag(
      "--unattended", unattended, "Install a Linux server without prompts");
  unattended_option->excludes(tui_option);
  unattended_option->excludes(listen_option);
  unattended_option->excludes("--port");
  unattended_option->excludes("--no-browser");
  UnattendedSetupOptions unattended_options;
  app.add_option("--override-repo-url",
                 unattended_options.override_repository_urls,
                 "Override the repository helper's URL (HTTPS, including "
                 "the distribution path); defaults to the Community repository")
      ->needs(unattended_option);
  app.add_option("--extra-package", unattended_options.extra_packages,
                 "Additional package to install (repeatable)")
      ->needs(unattended_option);
  bool without_webui = false;
  app.add_flag("--without-webui", without_webui,
               "Do not install or configure the WebUI")
      ->needs(unattended_option);
  bool without_tape_support = false;
  auto* without_tape_option
      = app.add_flag("--without-tape-support", without_tape_support,
                     "Do not install tape storage support");
  without_tape_option->needs(unattended_option);
  app.add_flag("--allow-tape-repositories",
               unattended_options.tape.allow_repositories,
               "Allow enabling approved vendor repositories for tape "
               "dependencies; requires vendor registration/entitlement")
      ->needs(unattended_option)
      ->excludes(without_tape_option);

  CLI11_PARSE(app, argc, argv);
  SetupContext setup_context(dry_run);

  std::cout << "Bareos Setup Wizard " << BAREOS_FULL_VERSION;
  if (dry_run) std::cout << " [dry-run]";
  std::cout << "\n";

  if (listen_address == "localhost") {
    // Give localhost a deterministic IPv4 meaning; pass ::1 to bind IPv6.
    listen_address = kIpv4LoopbackAddress;
  }

  in_addr ipv4_address{};
  in6_addr ipv6_address{};
  const bool is_ipv4
      = inet_pton(AF_INET, listen_address.c_str(), &ipv4_address) == 1;
  const bool is_ipv6
      = inet_pton(AF_INET6, listen_address.c_str(), &ipv6_address) == 1;
  if (!is_ipv4 && !is_ipv6) {
    std::cerr << "Fatal: invalid listen address: " << listen_address << "\n";
    return 1;
  }

  const bool listen_all_interfaces
      = (is_ipv4 && ipv4_address.s_addr == htonl(INADDR_ANY))
        || (is_ipv6 && IN6_IS_ADDR_UNSPECIFIED(&ipv6_address));
  const bool listen_loopback
      = (is_ipv4 && (ntohl(ipv4_address.s_addr) & 0xff000000U) == 0x7f000000U)
        || (is_ipv6 && IN6_IS_ADDR_LOOPBACK(&ipv6_address));
  if (!tui && !listen_loopback) {
    std::cerr << "Warning: listening on " << listen_address
              << " instead of the default " << kIpv4LoopbackAddress
              << ". The setup wizard executes privileged "
                 "commands as root and speaks plain HTTP, so the setup "
                 "token in the URL is the only access control; only do "
                 "this on a trusted network.\n"
                 "Consider forwarding the port over SSH instead, which "
                 "keeps the wizard on loopback and encrypts the "
                 "connection:\n"
                 "  ssh -L "
              << port << ":" << kIpv4LoopbackAddress << ":" << port
              << " root@<this-host>\n";
  }

  // Privileged steps (package install, service management, catalog
  // creation, ...) require root. Dry-run only prints commands and does not
  // need elevated privileges.
  if (!setup_context.dry_run() && !IsRoot()) {
    std::cerr << "Fatal: bareos-setup must be run as root. Use --dry to "
                 "print the commands without executing them.\n";
    return 1;
  }

  // Fail fast with a clear message if a required external tool (curl,
  // systemctl, the detected package manager, ...) is missing, instead
  // of surfacing a raw exec failure deep inside an install step later.
  // Dry-run only previews commands and never executes anything, so the
  // check is skipped in that mode.
  if (!setup_context.dry_run()) {
    const auto os = DetectOs();
    if (!IsSupportedPackageManager(os.pkg_mgr)) {
      std::cerr << "Fatal: no supported package manager (apt-get, dnf, yum, "
                   "zypper) was found.\n";
      return 1;
    }
    const auto missing = setup_context.MissingRequiredTools(os.pkg_mgr);
    if (!missing.empty()) {
      std::cerr << "Fatal: required tool(s) not found in PATH:";
      for (const auto& tool : missing) std::cerr << " " << tool;
      std::cerr << "\nInstall the missing tool(s) and try again.\n";
      return 1;
    }
  }

  if (unattended) {
    unattended_options.webui = !without_webui;
    unattended_options.tape.enabled = !without_tape_support;
    if (setenv("DEBIAN_FRONTEND", "noninteractive", 1) != 0) {
      std::cerr << "Fatal: cannot set noninteractive package installation.\n";
      return 1;
    }
    return RunUnattendedSetup(setup_context, unattended_options);
  }
  if (tui) return RunTuiWizard(setup_context);

  const std::string setup_token = GenerateSetupSecret(32);
  // Use a loopback URL for wildcard binds, in the same address family.
  // Wildcard addresses are not usable destinations in a browser URL.
  const std::string display_host
      = listen_all_interfaces
            ? (is_ipv6 ? kIpv6LoopbackAddress : kIpv4LoopbackAddress)
            : listen_address;
  const std::string url_host = display_host.find(':') == std::string::npos
                                   ? display_host
                                   : "[" + display_host + "]";
  const std::string setup_url = "http://" + url_host + ":"
                                + std::to_string(port)
                                + "/?token=" + UrlEncode(setup_token);

  if (no_browser) {
    std::cout << "Open this URL in your browser: " << setup_url << "\n"
              << std::flush;
  }

  // Fork before starting the server so the child opens the browser
  // after the parent has started listening.
  pid_t child = -1;
  if (!no_browser) {
    child = fork();
    if (child == 0) {
      // Child: wait briefly then open browser
      sleep(1);
      SetupContext browser_context;
      OpenBrowser(browser_context, url_host, port, setup_token);
      _exit(0);
    }
  }

  try {
    RunHttpServer(listen_address, port, setup_token,
                  [&setup_context](int fd, bool peer_is_loopback,
                                   std::string request_headers,
                                   std::string pending_input) {
                    RunSetupSession(fd, setup_context, peer_is_loopback,
                                    std::move(request_headers),
                                    std::move(pending_input));
                  });
  } catch (const std::exception& e) {
    std::cerr << "Fatal: " << e.what() << "\n";
    return 1;
  }
  return 0;
}
