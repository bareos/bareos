/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2026-2026 Bareos GmbH & Co. KG

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
 * bareos-webui-proxy entry point.
 *
 * Reads configuration from an INI file with:
 *   [listen]
 *     address = <bind address>          (default: localhost)
 *     port = <bind port>                (default: 9104)
 *
 *   [<selector>]                        (at least one unique selector section
 *                                       is required)
 *     address = <director address>      (default: localhost)
 *     port = <director port>            (default: 9101)
 *     director_name = <director name>   (default: <selector>; multiple
 *                                       selectors may point to the same name)
 *     tls_psk_disable = yes|no          (default: no)
 *
 * CLI flags:
 *   --config <path>
 *   --log-file <path>
 *   --log-level <debug|info|warn|error>
 */

#include "proxy_log.h"
#include "proxy_server.h"
#include "proxy_auth_session.h"

#include <CLI/CLI.hpp>
#include <csignal>
#include <stdexcept>
#include <string>

namespace {

void HandleSignal(int /*sig*/) { g_proxy_shutdown_requested = 1; }

}  // namespace

int main(int argc, char* argv[])
{
  CLI::App app{"Bareos Director WebSocket Proxy"};
  std::string config_file = WEBUI_PROXY_DEFAULT_CONFIG_PATH;
  std::string log_file;
  std::string log_level = "info";
  app.add_option("--config", config_file, "Proxy config file")
      ->capture_default_str();
  app.add_option("--log-file", log_file, "Append proxy logs to this file");
  app.add_option("--log-level", log_level,
                 "Minimum proxy log level: debug, info, warn, error");

  CLI11_PARSE(app, argc, argv);

  try {
    ProxyLogLevel min_level;
    if (!ParseProxyLogLevel(log_level, min_level)) {
      throw CLI::ValidationError("--log-level",
                                 "must be one of: debug, info, warn, error");
    }

    ProxyLoggerConfig logger_cfg;
    logger_cfg.min_level = min_level;
    logger_cfg.log_file = log_file;
    ConfigureProxyLogger(logger_cfg);

    ProxyConfig cfg = LoadProxyConfigFile(config_file);

    // Apply session timeout configuration
    ProxyAuthSessionStore::Instance().SetSessionTimeouts(
        cfg.session_idle_timeout_minutes,
        cfg.session_absolute_lifetime_hours);

    g_proxy_shutdown_requested = 0;
    std::signal(SIGINT, HandleSignal);
    std::signal(SIGTERM, HandleSignal);
    std::signal(SIGPIPE, SIG_IGN);  // prevent crash on broken pipe

    ProxyServer server(cfg);
    server.Run();
  } catch (const std::exception& ex) {
    PROXY_LOG_ERROR("", "fatal: %s", ex.what());
    return 1;
  }

  return 0;
}
