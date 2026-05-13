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
#include "proxy_config.h"

#include "lib/bsys.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {

std::string Trim(std::string value)
{
  auto is_space = [](unsigned char ch) { return std::isspace(ch) != 0; };
  value.erase(value.begin(),
              std::find_if_not(value.begin(), value.end(), is_space));
  value.erase(std::find_if_not(value.rbegin(), value.rend(), is_space).base(),
              value.end());
  return value;
}

std::string StripQuotes(std::string value)
{
  value = Trim(std::move(value));
  if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
    return value.substr(1, value.size() - 2);
  }
  return value;
}

std::string FormatConfigError(size_t line_number, const std::string& message)
{
  return "Proxy config line " + std::to_string(line_number) + ": " + message;
}

bool ParseBool(const std::string& value,
               const std::string& key,
               size_t line_number)
{
  if (Bstrcasecmp(value.c_str(), "yes")) { return true; }
  if (Bstrcasecmp(value.c_str(), "no")) { return false; }
  throw std::runtime_error(
      FormatConfigError(line_number, "'" + key + "' must be a boolean"));
}

int ParseInteger(const std::string& value,
                 const std::string& key,
                 size_t line_number)
{
  try {
    size_t pos = 0;
    const int parsed = std::stoi(value, &pos, 10);
    if (pos != value.size()) { throw std::invalid_argument("trailing"); }
    return parsed;
  } catch (...) {
    throw std::runtime_error(
        FormatConfigError(line_number, "'" + key + "' must be an integer"));
  }
}

void ApplyProxySetting(ProxyConfig& cfg,
                       const std::string& key,
                       const std::string& value,
                       size_t line_number)
{
  if (key == "ws_host") {
    cfg.bind_host = value;
  } else if (key == "ws_port") {
    cfg.port = ParseInteger(value, key, line_number);
  } else {
    throw std::runtime_error(FormatConfigError(
        line_number, "unknown key in [listen]: '" + key + "'"));
  }
}

void ApplyDirectorSetting(DirectorTargetConfig& cfg,
                          const std::string& key,
                          const std::string& value,
                          size_t line_number)
{
  if (key == "host") {
    cfg.host = value;
  } else if (key == "port") {
    cfg.port = ParseInteger(value, key, line_number);
  } else if (key == "director_name") {
    cfg.name = value;
  } else if (key == "tls_psk_disable") {
    cfg.tls_psk_disable = ParseBool(value, key, line_number);
  } else {
    throw std::runtime_error(
        FormatConfigError(line_number, "unknown director key '" + key + "'"));
  }
}

void ParseAndApplyProxyConfig(const std::string& ini, ProxyConfig& cfg)
{
  std::istringstream input(ini);
  std::string line;
  std::string current_section;
  size_t line_number = 0;

  while (std::getline(input, line)) {
    ++line_number;
    const auto comment_pos = line.find_first_of("#;");
    if (comment_pos != std::string::npos) {
      line = line.substr(0, comment_pos);
    }
    line = Trim(std::move(line));
    if (line.empty()) { continue; }

    if (line.front() == '[' && line.back() == ']') {
      current_section = Trim(line.substr(1, line.size() - 2));
      continue;
    }

    const auto eq_pos = line.find('=');
    if (eq_pos == std::string::npos) {
      throw std::runtime_error(FormatConfigError(line_number, "invalid line"));
    }

    const std::string key = Trim(line.substr(0, eq_pos));
    const std::string value = StripQuotes(line.substr(eq_pos + 1));

    if (current_section == "listen") {
      ApplyProxySetting(cfg, key, value, line_number);
      continue;
    }
    if (current_section.rfind("director:", 0) == 0) {
      const std::string director_id = current_section.substr(9);
      auto [it, inserted]
          = cfg.allowed_directors.emplace(director_id, DirectorTargetConfig{});
      if (inserted) { it->second.name = director_id; }
      ApplyDirectorSetting(it->second, key, value, line_number);
      continue;
    }

    throw std::runtime_error(FormatConfigError(
        line_number, "unknown section '" + current_section + "'"));
  }

  if (cfg.allowed_directors.empty()) {
    throw std::runtime_error(
        "Proxy config: at least one [director:<id>] section is required");
  }
}

}  // namespace

void LoadProxyConfigFile(const std::string& path, ProxyConfig& cfg)
{
  std::ifstream input(path);
  if (!input) {
    throw std::runtime_error("Proxy config: cannot load '" + path + "'");
  }
  std::ostringstream contents;
  contents << input.rdbuf();
  ParseAndApplyProxyConfig(contents.str(), cfg);
}

void LoadProxyConfigFromString(const std::string& ini, ProxyConfig& cfg)
{
  ParseAndApplyProxyConfig(ini, cfg);
}
