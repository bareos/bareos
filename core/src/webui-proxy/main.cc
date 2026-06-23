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
 * bareos-webui-proxy entry point and main() function.
 */

#include "proxy_log.h"
#include "proxy_server.h"
#include "proxy_auth_session.h"

#include <CLI/CLI.hpp>
#include <csignal>
#include <filesystem>
#include <stdexcept>
#include <string>

namespace {

void HandleSignal(int /*sig*/) { g_proxy_shutdown_requested = 1; }

}  // namespace

int main(int argc, char* argv[])
{
  CLI::App app{"Bareos Director WebSocket Proxy"};
  std::string config_file = WEBUI_PROXY_DEFAULT_CONFIG_PATH;
  CLI::Option* config_option = nullptr;
  std::string log_file;
  ProxyLogLevel log_level = ProxyLogLevel::Info;

  config_option = app.add_option("--config", config_file, "Proxy config file")
                      ->capture_default_str();
  app.add_option("--log-file", log_file, "Append proxy logs to this file");
  app.add_option("--log-level", log_level,
                 "Minimum proxy log level: debug, info, warn, error")
      ->transform(CLI::CheckedTransformer(std::map<std::string, ProxyLogLevel>{
          {"debug", ProxyLogLevel::Debug},
          {"info", ProxyLogLevel::Info},
          {"warn", ProxyLogLevel::Warn},
          {"warning", ProxyLogLevel::Warn},
          {"error", ProxyLogLevel::Error},
      },
          CLI::ignore_case));

  CLI11_PARSE(app, argc, argv);

  try {
    ProxyLoggerConfig logger_cfg;
    logger_cfg.min_level = log_level;
    logger_cfg.log_file = log_file;
    ConfigureProxyLogger(logger_cfg);

    ProxyConfig cfg;
    if (config_option->count() > 0) {
      cfg = LoadProxyConfigFile(config_file);
      PROXY_LOG_INFO("", "loaded proxy config from '%s' (--config)",
                     config_file.c_str());
    } else {
      std::error_code fs_error;
      const bool has_default_config
          = std::filesystem::exists(config_file, fs_error);
      if (fs_error) {
        throw ProxyConfigFileError("Proxy config: cannot check '" + config_file
                                   + "'");
      }
      if (has_default_config) {
        cfg = LoadProxyConfigFile(config_file);
        PROXY_LOG_INFO("", "loaded proxy config from '%s' (default path)",
                       config_file.c_str());
      } else {
        cfg = LoadBuiltInDefaultProxyConfig();
        PROXY_LOG_INFO("", "proxy config file '%s' not found; using built-in "
                           "defaults",
                       config_file.c_str());
      }
    }

    // Apply session timeout configuration
    ProxyAuthSessionStore::SetSessionTimeouts(
        cfg.session_idle_timeout_minutes,
        cfg.session_absolute_lifetime_hours);

    g_proxy_shutdown_requested = 0;
    std::signal(SIGINT, HandleSignal);
    std::signal(SIGTERM, HandleSignal);
    std::signal(SIGPIPE, SIG_IGN);  // prevent crash on broken pipe

    RunProxyServer(cfg);
  } catch (const std::exception& ex) {
    PROXY_LOG_ERROR("", "fatal: %s", ex.what());
    return 1;
  }

  return 0;
}
