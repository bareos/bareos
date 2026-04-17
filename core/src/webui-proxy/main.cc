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
 * bareos-webui-proxy — C++ drop-in replacement for director_ws_proxy.py.
 *
 * Reads configuration from the same environment variables as the Python proxy:
 *   WS_HOST               bind address        (default: localhost)
 *   WS_PORT               WebSocket port      (default: 8765)
 *   BAREOS_DIRECTOR_HOST  director hostname   (default: localhost)
 *   BAREOS_DIRECTOR_PORT  director port       (default: 9101)
 *   BAREOS_DIRECTOR_NAME  director name       (default: bareos-dir)
 *
 * The proxy can use TLS-PSK for its connection to the Director when enabled.
 *
 * CLI flags (override env vars):
 *   --ws-host <addr>
 *   --ws-port <port>
 *   --director-host <addr>
 *   --director-port <port>
 *   --director-name <name>
 */

#include "proxy_server.h"

#include <CLI/CLI.hpp>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>

static ProxyServer* g_server = nullptr;

static void HandleSignal(int /*sig*/)
{
  if (g_server) { g_server->Stop(); }
}

static std::string EnvOr(const char* var, const char* def)
{
  const char* val = std::getenv(var);
  return val ? val : def;
}

static int EnvIntOr(const char* var, int def)
{
  const char* val = std::getenv(var);
  return val ? std::atoi(val) : def;
}

int main(int argc, char* argv[])
{
  ServerConfig cfg;
  cfg.bind_host = EnvOr("WS_HOST", "localhost");
  cfg.port = EnvIntOr("WS_PORT", 8765);
  cfg.director.host = EnvOr("BAREOS_DIRECTOR_HOST", "localhost");
  cfg.director.port = EnvIntOr("BAREOS_DIRECTOR_PORT", 9101);
  cfg.director.name = EnvOr("BAREOS_DIRECTOR_NAME", "bareos-dir");

  CLI::App app{"Bareos Director WebSocket Proxy"};
  app.add_option("--ws-host", cfg.bind_host,
                 "Bind address (env: WS_HOST, default: localhost)");
  app.add_option("--ws-port", cfg.port,
                 "WebSocket port (env: WS_PORT, default: 8765)");
  app.add_option("--director-host", cfg.director.host,
                 "Director hostname (env: BAREOS_DIRECTOR_HOST)");
  app.add_option("--director-port", cfg.director.port,
                 "Director port (env: BAREOS_DIRECTOR_PORT, default: 9101)");
  app.add_option("--director-name", cfg.director.name,
                 "Director name (env: BAREOS_DIRECTOR_NAME)");

  CLI11_PARSE(app, argc, argv);

  std::signal(SIGINT, HandleSignal);
  std::signal(SIGTERM, HandleSignal);
  std::signal(SIGPIPE, SIG_IGN);  // prevent crash on broken pipe

  ProxyServer server(cfg);
  g_server = &server;

  try {
    server.Run();
  } catch (const std::exception& ex) {
    fprintf(stderr, "[proxy] fatal: %s\n", ex.what());
    return 1;
  }

  return 0;
}
