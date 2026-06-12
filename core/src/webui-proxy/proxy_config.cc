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
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

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

std::string FormatConfigError(size_t line_number, const std::string& message)
{
  return "Proxy config line " + std::to_string(line_number) + ": " + message;
}

std::string FormatUnknownKeyError(std::string_view section,
                                  const std::string& key,
                                  size_t line_number)
{
  return FormatConfigError(
      line_number,
      "unknown key in [" + std::string(section) + "]: '" + key + "'");
}

std::string ParseValue(std::string value, size_t line_number)
{
  value = Trim(std::move(value));
  const bool starts_quoted = !value.empty() && value.front() == '"';
  const bool ends_quoted = !value.empty() && value.back() == '"';
  if (starts_quoted != ends_quoted) {
    throw ProxyConfigQuotedValueError(FormatConfigError(
        line_number, "quoted value must use matching quotes"));
  }
  if (!starts_quoted) { return value; }

  std::string parsed;
  parsed.reserve(value.size() - 2);
  for (size_t i = 1; i + 1 < value.size(); ++i) {
    if (value[i] != '\\') {
      parsed.push_back(value[i]);
      continue;
    }
    ++i;
    if (i + 1 >= value.size()) {
      throw ProxyConfigQuotedValueError(FormatConfigError(
          line_number, "quoted value ends with an incomplete escape"));
    }
    switch (value[i]) {
      case '"':
      case '\\':
        parsed.push_back(value[i]);
        break;
      default:
        throw ProxyConfigQuotedValueError(FormatConfigError(
            line_number, "quoted value contains an invalid escape"));
    }
  }
  return parsed;
}

void EnsurePrintableAscii(std::string_view value,
                          std::string_view what,
                          size_t line_number)
{
  const auto is_printable_ascii
      = [](unsigned char ch) { return ch >= 0x20 && ch <= 0x7e; };
  if (std::all_of(value.begin(), value.end(), is_printable_ascii)) { return; }
  throw ProxyConfigInvalidCharacterError(FormatConfigError(
      line_number, std::string(what) + " must contain only printable ASCII"));
}

bool ParseBool(const std::string& value,
               const std::string& key,
               size_t line_number)
{
  if (Bstrcasecmp(value.c_str(), "yes")) { return true; }
  if (Bstrcasecmp(value.c_str(), "no")) { return false; }
  throw ProxyConfigInvalidBooleanError(
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
  } catch (const std::exception&) {
    throw ProxyConfigInvalidIntegerError(
        FormatConfigError(line_number, "'" + key + "' must be an integer"));
  }
}

void ApplyProxySetting(ProxyConfig& cfg,
                       const std::string& key,
                       const std::string& value,
                       size_t line_number)
{
  if (key == "address") {
    cfg.bind_address = value;
  } else if (key == "port") {
    cfg.port = ParseInteger(value, key, line_number);
  } else if (key == "session_idle_timeout_minutes") {
    cfg.session_idle_timeout_minutes
        = ParseInteger(value, key, line_number);
  } else if (key == "session_absolute_lifetime_hours") {
    cfg.session_absolute_lifetime_hours
        = ParseInteger(value, key, line_number);
  } else {
    throw ProxyConfigUnknownKeyError(
        FormatUnknownKeyError("listen", key, line_number));
  }
}

void ApplyDirectorSetting(DirectorTargetConfig& cfg,
                          std::string_view section_name,
                          const std::string& key,
                          const std::string& value,
                          size_t line_number)
{
  if (key == "address") {
    cfg.address = value;
  } else if (key == "port") {
    cfg.port = ParseInteger(value, key, line_number);
  } else if (key == "director_name") {
    cfg.name = value;
  } else if (key == "tls_psk_disable") {
    cfg.tls_psk_disable = ParseBool(value, key, line_number);
  } else {
    throw ProxyConfigUnknownKeyError(
        FormatUnknownKeyError(section_name, key, line_number));
  }
}

void ParseAndApplyProxyConfig(const std::string& ini, ProxyConfig& cfg)
{
  enum class SectionKind
  {
    kUnknown,
    kListen,
    kDirector
  };

  std::istringstream input(ini);
  std::string line;
  std::string current_section;
  std::string current_director_selector;
  SectionKind current_section_kind = SectionKind::kUnknown;
  size_t line_number = 0;
  std::map<std::string, size_t> seen_sections;
  std::map<std::string, DirectorTargetConfig> parsed_directors;

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
      EnsurePrintableAscii(current_section, "section name", line_number);
      if (current_section.empty()) {
        throw ProxyConfigParseError(
            FormatConfigError(line_number, "section name must not be empty"));
      }
      if (!seen_sections.emplace(current_section, line_number).second) {
        throw ProxyConfigDuplicateSectionError(FormatConfigError(
            line_number, "duplicate section '" + current_section + "'"));
      }

      current_director_selector.clear();
      if (Bstrcasecmp(current_section.c_str(), "listen")) {
        current_section_kind = SectionKind::kListen;
      } else if (current_section.rfind("director:", 0) == 0) {
        throw ProxyConfigUnknownSectionError(
            FormatConfigError(line_number,
                              "director sections no longer use the "
                              "'director:' prefix"));
      } else {
        current_section_kind = SectionKind::kDirector;
        current_director_selector = current_section;
      }
      continue;
    }

    const auto eq_pos = line.find('=');
    if (eq_pos == std::string::npos) {
      throw ProxyConfigParseError(
          FormatConfigError(line_number, "invalid line"));
    }

    const std::string key = Trim(line.substr(0, eq_pos));
    const std::string value = ParseValue(line.substr(eq_pos + 1), line_number);
    EnsurePrintableAscii(key, "key", line_number);
    EnsurePrintableAscii(value, "value", line_number);

    switch (current_section_kind) {
      case SectionKind::kListen:
        ApplyProxySetting(cfg, key, value, line_number);
        continue;
      case SectionKind::kDirector: {
        auto [it, inserted] = parsed_directors.emplace(
            current_director_selector, DirectorTargetConfig{});
        if (inserted) { it->second.name = current_director_selector; }
        ApplyDirectorSetting(it->second, current_section, key, value,
                             line_number);
        continue;
      }
      case SectionKind::kUnknown:
        break;
    }

    throw ProxyConfigUnknownSectionError(FormatConfigError(
        line_number, "unknown section '" + current_section + "'"));
  }

  if (parsed_directors.empty()) {
    throw ProxyConfigMissingDirectorSectionError(
        "Proxy config: at least one director section is required");
  }

  cfg.configured_directors = std::move(parsed_directors);
}

}  // namespace

void LoadProxyConfigFile(const std::string& path, ProxyConfig& cfg)
{
  std::ifstream input(path);
  if (!input) {
    throw ProxyConfigFileError("Proxy config: cannot load '" + path + "'");
  }
  std::ostringstream contents;
  contents << input.rdbuf();
  ParseAndApplyProxyConfig(contents.str(), cfg);
}

void LoadProxyConfigFromString(const std::string& ini, ProxyConfig& cfg)
{
  ParseAndApplyProxyConfig(ini, cfg);
}
