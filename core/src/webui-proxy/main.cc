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
 * Reads configuration from an INI config file.
 *
 * CLI flags:
 *   --config <path>
 */

#include "bconfig_http_proxy.h"
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
  CLI::App app{"Bareos Director WebSocket Proxy"};
  std::string config_file = WEBUI_PROXY_DEFAULT_CONFIG_PATH;
  BconfigHttpProxyConfig bconfig_cfg;
  bconfig_cfg.upstream_host = EnvOr("BAREOS_BCONFIG_HOST", "127.0.0.1");
  bconfig_cfg.upstream_port = EnvIntOr("BAREOS_BCONFIG_PORT", 8080);
  app.add_option("--config", config_file,
                 "Proxy config file (default: " WEBUI_PROXY_DEFAULT_CONFIG_PATH
                 ")");

  app.add_option("--bconfig-host", bconfig_cfg.upstream_host,
                 "bconfig-service upstream host (env: BAREOS_BCONFIG_HOST)");
  app.add_option("--bconfig-port", bconfig_cfg.upstream_port,
                 "bconfig-service upstream port "
                 "(env: BAREOS_BCONFIG_PORT, default: 8080)");

  CLI11_PARSE(app, argc, argv);

  ProxyConfig cfg;
  LoadProxyConfigFile(config_file, cfg);

  std::signal(SIGINT, HandleSignal);
  std::signal(SIGTERM, HandleSignal);
  std::signal(SIGPIPE, SIG_IGN);  // prevent crash on broken pipe

  ProxyServer server(cfg, bconfig_cfg);
  g_server = &server;

  try {
    server.Run();
  } catch (const std::exception& ex) {
    fprintf(stderr, "[proxy] fatal: %s\n", ex.what());
    return 1;
  }

  return 0;
}
