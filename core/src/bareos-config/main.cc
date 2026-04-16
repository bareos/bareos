/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2026 Bareos GmbH & Co. KG

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
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include <CLI/CLI.hpp>
#include <unistd.h>

#include "config_model.h"
#include "config_service.h"
#include "http_server.h"

namespace {
void OpenBrowser(const std::string& bind_address, int port)
{
  const std::string url = "http://" + bind_address + ":" + std::to_string(port)
                          + "/";
  for (const char* command : {"xdg-open", "open", "sensible-browser"}) {
    if (execlp(command, command, url.c_str(), nullptr) == 0) return;
  }
  std::cerr << "Could not open browser automatically.\n"
            << "Open this URL manually: " << url << "\n";
}
}  // namespace

int main(int argc, char* argv[])
{
  CLI::App app{"Bareos Config Service", "bareos-config"};
  app.set_version_flag("--version", BAREOS_FULL_VERSION);

  int port = 19111;
  std::string bind_address = "127.0.0.1";
  bool no_browser = false;
  std::vector<std::filesystem::path> config_roots;

  app.add_option("--port,-p", port, "TCP port to listen on")->default_val(19111);
  app.add_option("--bind", bind_address, "IPv4 address to bind to")
      ->default_val("127.0.0.1");
  app.add_flag("--no-browser", no_browser,
               "Do not open the browser automatically");
  app.add_option("--config-root", config_roots,
                 "Bareos configuration root to manage (repeatable)");

  CLI11_PARSE(app, argc, argv);

  if (port < 1 || port > 65535) {
    std::cerr << "Fatal: --port must be in the range 1..65535\n";
    return 1;
  }

  if (config_roots.empty()) config_roots = DefaultConfigRoots();

  if (!no_browser) {
    const pid_t child = fork();
    if (child == 0) {
      sleep(1);
      OpenBrowser(bind_address, port);
      _exit(0);
    }
  }

  try {
    const ConfigServiceOptions options{config_roots};
    RunHttpServer(bind_address, port, [&options](const HttpRequest& request) {
      return HandleConfigServiceRequest(options, request);
    });
  } catch (const std::exception& e) {
    std::cerr << "Fatal: " << e.what() << "\n";
    return 1;
  }

  return 0;
}
