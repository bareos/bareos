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
#ifndef BAREOS_WEBUI_PROXY_PROXY_LOG_H_
#define BAREOS_WEBUI_PROXY_PROXY_LOG_H_

#include <chrono>
#include <cstdio>
#include <string>
#include <string_view>
#include <utility>

#include "lib/source_location.h"
#include "lib/util.h"

enum class ProxyLogLevel
{
  Debug = 0,
  Info,
  Warn,
  Error,
};

struct ProxyLoggerConfig {
  ProxyLogLevel min_level{ProxyLogLevel::Info};
  std::string log_file;
  bool log_to_stderr{true};
};

bool ParseProxyLogLevel(std::string_view input, ProxyLogLevel& level);
std::string_view ProxyLogLevelName(ProxyLogLevel level);
std::string FormatProxyLogTimestamp(
    std::chrono::system_clock::time_point timestamp);
void ConfigureProxyLogger(const ProxyLoggerConfig& config);
void ProxyLogWrite(ProxyLogLevel level,
                   std::string_view peer,
                   std::string_view message,
                   libbareos::source_location loc
                   = libbareos::source_location::current());

inline void ProxyLogFormat(ProxyLogLevel level,
                           std::string_view peer,
                           libbareos::source_location loc,
                           const char* message)
{
  ProxyLogWrite(level, peer, message, loc);
}

template <typename First, typename... Rest>
void ProxyLogFormat(ProxyLogLevel level,
                    std::string_view peer,
                    libbareos::source_location loc,
                    const char* format,
                    First&& first,
                    Rest&&... rest)
{
  printf_check(format, std::forward<First>(first),
               std::forward<Rest>(rest)...);
  const int rendered_size
      = std::snprintf(nullptr, 0, format, std::forward<First>(first),
                      std::forward<Rest>(rest)...);
  if (rendered_size < 0) {
    ProxyLogWrite(level, peer, "log formatting failed", loc);
    return;
  }

  std::string rendered(static_cast<size_t>(rendered_size), '\0');
  std::snprintf(rendered.data(), rendered.size() + 1, format,
                std::forward<First>(first), std::forward<Rest>(rest)...);
  ProxyLogWrite(level, peer, rendered, loc);
}

#define PROXY_LOG_DEBUG(peer, format, ...)              \
  ProxyLogFormat(ProxyLogLevel::Debug, peer,            \
                 libbareos::source_location::current(), \
                 format __VA_OPT__(, ) __VA_ARGS__)

#define PROXY_LOG_INFO(peer, format, ...)               \
  ProxyLogFormat(ProxyLogLevel::Info, peer,             \
                 libbareos::source_location::current(), \
                 format __VA_OPT__(, ) __VA_ARGS__)

#define PROXY_LOG_WARN(peer, format, ...)               \
  ProxyLogFormat(ProxyLogLevel::Warn, peer,             \
                 libbareos::source_location::current(), \
                 format __VA_OPT__(, ) __VA_ARGS__)

#define PROXY_LOG_ERROR(peer, format, ...)              \
  ProxyLogFormat(ProxyLogLevel::Error, peer,            \
                 libbareos::source_location::current(), \
                 format __VA_OPT__(, ) __VA_ARGS__)

#endif  // BAREOS_WEBUI_PROXY_PROXY_LOG_H_
