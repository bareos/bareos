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

#include "proxy_log.h"
#include "lib/bsys.h"

#include <algorithm>
#include <cerrno>
#include <filesystem>
#include <iomanip>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace {

struct ConfigFd {
  int fd;
  ~ConfigFd()
  {
    if (fd >= 0) { close(fd); }
  }
};

void CheckConfigPermissions(int fd, const std::string& path, bool directory)
{
  struct stat metadata{};
  if (fstat(fd, &metadata) != 0) {
    throw ProxyConfigFileError(
        "Proxy config: cannot inspect permissions of '" + path
        + "': " + std::error_code(errno, std::generic_category()).message());
  }
  const bool safe_owner = metadata.st_uid == 0 || metadata.st_uid == geteuid();
  const mode_t forbidden = S_IRWXO | S_IWGRP
                           | (directory ? 0 : S_IXUSR | S_IXGRP) | S_ISUID
                           | S_ISGID | S_ISVTX;
  const bool safe_group
      = !(metadata.st_mode & S_IRWXG) || metadata.st_gid == getegid();
  const bool right_type
      = directory ? S_ISDIR(metadata.st_mode) : S_ISREG(metadata.st_mode);
  if (!safe_owner || !safe_group || !right_type
      || (metadata.st_mode & forbidden)) {
    const std::string recommended_mode = directory ? "0750" : "0640";
    std::ostringstream error;
    error << "Proxy config: unsafe configuration "
          << (directory ? "directory" : "file") << " '" << path << "' (mode "
          << std::oct << std::setw(4) << std::setfill('0')
          << (metadata.st_mode & 07777) << std::dec
          << ", uid=" << metadata.st_uid << ", gid=" << metadata.st_gid
          << "): ";
    if (!right_type) {
      error << "expected " << (directory ? "a directory" : "a regular file");
    } else if (!safe_owner) {
      error << "owner must be root (uid 0) or service uid " << geteuid();
    } else if (!safe_group) {
      error << "group access requires service gid " << getegid();
    } else {
      error << "other-user access, group write, "
            << (directory ? "" : "file execute permissions, ")
            << "and special mode bits are forbidden";
    }
    error << "; use mode " << recommended_mode << " or stricter (chmod "
          << recommended_mode << "), root or service uid " << geteuid()
          << " as owner, and service gid " << getegid()
          << " when group access is enabled";
    throw ProxyConfigFileError(error.str());
  }
}

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

std::string StripComment(std::string_view line)
{
  bool quoted = false;
  bool escaped = false;
  for (size_t i = 0; i < line.size(); ++i) {
    if (escaped) {
      escaped = false;
    } else if (quoted && line[i] == '\\') {
      escaped = true;
    } else if (line[i] == '"') {
      quoted = !quoted;
    } else if (!quoted && (line[i] == '#' || line[i] == ';')) {
      return std::string(line.substr(0, i));
    }
  }
  return std::string(line);
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
    cfg.session_idle_timeout_minutes = ParseInteger(value, key, line_number);
  } else if (key == "session_absolute_lifetime_hours") {
    cfg.session_absolute_lifetime_hours = ParseInteger(value, key, line_number);
  } else if (key == "max_unauthenticated_connections") {
    cfg.max_unauthenticated_connections = ParseInteger(value, key, line_number);
  } else if (key == "log_level") {
    if (!ParseProxyLogLevel(value, cfg.log_level)) {
      throw ProxyConfigParseError("line " + std::to_string(line_number)
                                  + ": invalid log_level value '" + value
                                  + "' (expected: debug, info, warn, error)");
    }
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
  } else if (key == "pam_console_name" || key == "pam_console_password") {
    if (!cfg.pam_console) { cfg.pam_console.emplace(); }
    if (key == "pam_console_name") {
      cfg.pam_console->name = value;
    } else {
      cfg.pam_console->password = value;
    }
  } else {
    throw ProxyConfigUnknownKeyError(
        FormatUnknownKeyError(section_name, key, line_number));
  }
}

ProxyConfig ParseProxyConfig(const std::string& ini)
{
  ProxyConfig cfg;
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
  bool seen_listen = false;

  while (std::getline(input, line)) {
    ++line_number;
    line = Trim(StripComment(line));
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
        seen_listen = true;
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

  if (!seen_listen) {
    throw ProxyConfigMissingListenSectionError(
        "Proxy config: a [listen] section is required");
  }

  if (parsed_directors.empty()) {
    throw ProxyConfigMissingDirectorSectionError(
        "Proxy config: at least one director section is required");
  }

  for (const auto& [selector, director] : parsed_directors) {
    if (!director.pam_console) { continue; }
    if (director.pam_console->name.empty()
        || director.pam_console->password.empty()) {
      throw ProxyConfigParseError(
          "Proxy config [" + selector
          + "]: PAM requires nonempty pam_console_name and pam_console_password");
    }
    if (director.tls_psk_disable) {
      throw ProxyConfigParseError("Proxy config [" + selector
                                  + "]: PAM requires TLS-PSK");
    }
  }
  cfg.configured_directors = std::move(parsed_directors);
  return cfg;
}

}  // namespace

ProxyConfig LoadProxyConfigFile(const std::string& path)
{
  const std::filesystem::path config_path(path);
  const auto parent = config_path.has_parent_path()
                          ? config_path.parent_path()
                          : std::filesystem::path(".");
  ConfigFd directory{
      open(parent.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW)};
  if (directory.fd < 0) {
    throw ProxyConfigFileError(
        "Proxy config: cannot open configuration directory '" + parent.string()
        + "': " + std::error_code(errno, std::generic_category()).message()
        + "; check service directory read/search permissions; the immediate "
          "directory must not be a symbolic link");
  }
  ConfigFd input{openat(directory.fd, config_path.filename().c_str(),
                        O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK)};
  if (input.fd < 0) {
    throw ProxyConfigFileError(
        "Proxy config: cannot load '" + path + "': "
        + std::error_code(errno, std::generic_category()).message()
        + "; check service file read permissions; symbolic links are not allowed");
  }
  CheckConfigPermissions(input.fd, path, false);
  CheckConfigPermissions(directory.fd, parent.string(), true);
  std::string contents;
  char buffer[4096];
  while (true) {
    const auto count = read(input.fd, buffer, sizeof(buffer));
    if (count < 0 && errno == EINTR) { continue; }
    if (count < 0) {
      throw ProxyConfigFileError(
          "Proxy config: cannot read '" + path
          + "': " + std::error_code(errno, std::generic_category()).message());
    }
    if (count == 0) { break; }
    contents.append(buffer, static_cast<size_t>(count));
  }
  return ParseProxyConfig(contents);
}

ProxyConfig LoadProxyConfigFromString(const std::string& ini)
{
  return ParseProxyConfig(ini);
}

ProxyConfig LoadBuiltInDefaultProxyConfig()
{
  ProxyConfig cfg;
  DirectorTargetConfig director_cfg;
  director_cfg.name = "bareos-dir";
  cfg.configured_directors.emplace("bareos-dir", std::move(director_cfg));
  return cfg;
}
