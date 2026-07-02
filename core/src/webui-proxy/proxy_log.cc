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
#include "proxy_log.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <mutex>
#include <stdexcept>
#include <string>

namespace {

std::mutex logger_mutex;
ProxyLoggerConfig logger_config;
std::ofstream logger_stream;

std::string_view Basename(std::string_view path)
{
  const auto slash = path.find_last_of("/\\");
  return slash == std::string_view::npos ? path : path.substr(slash + 1);
}

std::string NormalizeTimezoneOffset(std::string_view raw_offset)
{
  // strftime("%z") returns +HHMM/-HHMM, but the proxy log format uses
  // RFC 3339 style offsets with a colon: +HH:MM/-HH:MM.
  if (raw_offset.size() == 5
      && (raw_offset.front() == '+' || raw_offset.front() == '-')) {
    std::string normalized;
    normalized.reserve(6);
    normalized.append(raw_offset.substr(0, 3));
    normalized.push_back(':');
    normalized.append(raw_offset.substr(3));
    return normalized;
  }

  return std::string(raw_offset);
}

std::string BuildLogLine(ProxyLogLevel level,
                         std::string_view peer,
                         std::string_view message,
                         libbareos::source_location loc)
{
  std::string line = FormatProxyLogTimestamp(std::chrono::system_clock::now());
  line.append(" [proxy] [");
  line.append(ProxyLogLevelName(level));
  line.append("] [");
  line.append(Basename(loc.file_name()));
  line.push_back(':');
  line.append(std::to_string(loc.line()));
  line.push_back(']');
  if (!peer.empty()) {
    line.append(" [");
    line.append(peer);
    line.push_back(']');
  }
  line.push_back(' ');
  line.append(message);
  line.push_back('\n');
  return line;
}

bool ShouldLog(ProxyLogLevel level)
{
  return static_cast<int>(level) >= static_cast<int>(logger_config.min_level);
}

}  // namespace

bool ParseProxyLogLevel(std::string_view input, ProxyLogLevel& level)
{
  std::string normalized(input);
  std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                 [](unsigned char ch) { return std::tolower(ch); });

  if (normalized == "debug") {
    level = ProxyLogLevel::Debug;
    return true;
  }
  if (normalized == "info") {
    level = ProxyLogLevel::Info;
    return true;
  }
  if (normalized == "warn" || normalized == "warning") {
    level = ProxyLogLevel::Warn;
    return true;
  }
  if (normalized == "error") {
    level = ProxyLogLevel::Error;
    return true;
  }

  return false;
}

std::string_view ProxyLogLevelName(ProxyLogLevel level)
{
  switch (level) {
    case ProxyLogLevel::Debug:
      return "DEBUG";
    case ProxyLogLevel::Info:
      return "INFO";
    case ProxyLogLevel::Warn:
      return "WARN";
    case ProxyLogLevel::Error:
      return "ERROR";
  }

  return "UNKNOWN";
}

std::string FormatProxyLogTimestamp(
    std::chrono::system_clock::time_point timestamp)
{
  using namespace std::chrono;

  const auto seconds = floor<std::chrono::seconds>(timestamp);
  const auto subseconds = duration_cast<microseconds>(timestamp - seconds);
  const std::time_t wall_clock = system_clock::to_time_t(seconds);

  std::tm local_tm{};
  localtime_r(&wall_clock, &local_tm);

  std::array<char, 32> date_buffer{};
  if (std::strftime(date_buffer.data(), date_buffer.size(), "%Y-%m-%dT%H:%M:%S",
                    &local_tm)
      == 0) {
    throw std::runtime_error("Proxy logger: failed to format timestamp");
  }

  std::array<char, 8> offset_buffer{};
  if (std::strftime(offset_buffer.data(), offset_buffer.size(), "%z", &local_tm)
      == 0) {
    throw std::runtime_error("Proxy logger: failed to format timezone");
  }

  char formatted[64];
  std::snprintf(formatted, sizeof(formatted), "%s.%06lld%s", date_buffer.data(),
                static_cast<long long>(subseconds.count()),
                NormalizeTimezoneOffset(offset_buffer.data()).c_str());
  return formatted;
}

void ConfigureProxyLogger(const ProxyLoggerConfig& config)
{
  std::ofstream new_stream;
  if (!config.log_file.empty()) {
    new_stream.open(config.log_file, std::ios::out | std::ios::app);
    if (!new_stream) {
      throw std::runtime_error("Proxy logger: cannot open log file '"
                               + config.log_file + "'");
    }
  }

  std::lock_guard<std::mutex> lock(logger_mutex);
  logger_config = config;
  logger_stream = std::move(new_stream);
}

void ProxyLogWrite(ProxyLogLevel level,
                   std::string_view peer,
                   std::string_view message,
                   libbareos::source_location loc)
{
  std::lock_guard<std::mutex> lock(logger_mutex);
  if (!ShouldLog(level)) { return; }

  const std::string line = BuildLogLine(level, peer, message, loc);
  if (logger_config.log_to_stderr) {
    std::fwrite(line.data(), 1, line.size(), stderr);
    std::fflush(stderr);
  }
  if (logger_stream.is_open()) {
    logger_stream << line;
    logger_stream.flush();
  }
}
