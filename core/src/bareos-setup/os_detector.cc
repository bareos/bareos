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
#include "os_detector.h"

#include <cctype>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include <sys/utsname.h>
#include <unistd.h>

const char* PackageManagerName(PackageManager package_manager)
{
  switch (package_manager) {
    case PackageManager::Unknown:
      return "unknown";
    case PackageManager::Apt:
      return "apt";
    case PackageManager::Dnf:
      return "dnf";
    case PackageManager::Yum:
      return "yum";
    case PackageManager::Zypper:
      return "zypper";
  }
  return "unknown";
}

static std::string_view Trim(std::string_view value)
{
  const auto is_space = [](unsigned char ch) { return std::isspace(ch) != 0; };
  while (!value.empty() && is_space(value.front())) value.remove_prefix(1);
  while (!value.empty() && is_space(value.back())) value.remove_suffix(1);
  return value;
}

/** Remove os-release quotes and interpret backslash escapes. */
static std::string Unquote(std::string_view value)
{
  value = Trim(value);
  if (value.size() >= 2 && (value.front() == '"' || value.front() == '\'')
      && value.back() == value.front()) {
    value.remove_prefix(1);
    value.remove_suffix(1);
  }

  std::string unescaped;
  unescaped.reserve(value.size());
  bool escaping = false;
  for (const char ch : value) {
    if (escaping) {
      unescaped.push_back(ch);
      escaping = false;
      continue;
    }
    if (ch == '\\') {
      escaping = true;
      continue;
    }
    unescaped.push_back(ch);
  }
  if (escaping) unescaped.push_back('\\');

  return unescaped;
}

/** Split a whitespace separated os-release value list into its entries. */
static std::vector<std::string> SplitWords(std::string_view value)
{
  std::vector<std::string> words;
  size_t offset = 0;
  while (offset < value.size()) {
    while (offset < value.size()
           && std::isspace(static_cast<unsigned char>(value[offset])) != 0) {
      ++offset;
    }
    const size_t word_begin = offset;
    while (offset < value.size()
           && std::isspace(static_cast<unsigned char>(value[offset])) == 0) {
      ++offset;
    }
    if (word_begin != offset) {
      words.emplace_back(value.substr(word_begin, offset - word_begin));
    }
  }
  return words;
}

OsInfo ParseOsRelease(const std::string& content)
{
  OsInfo info;
  std::istringstream stream(content);
  std::string line;
  while (std::getline(stream, line)) {
    const std::string_view trimmed_line = Trim(line);
    // os-release comment lines begin with '#'; blank lines have no '='.
    if (trimmed_line.empty() || trimmed_line.front() == '#') continue;
    const auto eq = trimmed_line.find('=');
    if (eq == std::string::npos) continue;
    auto key = Unquote(trimmed_line.substr(0, eq));
    auto val = Unquote(trimmed_line.substr(eq + 1));
    if (key == "ID")
      info.distro = val;
    else if (key == "ID_LIKE")
      info.id_like = SplitWords(val);
    else if (key == "VERSION_ID")
      info.version = val;
    else if (key == "VERSION_CODENAME")
      info.codename = val;
    else if (key == "PRETTY_NAME")
      info.pretty_name = val;
  }
  return info;
}

OsInfo DetectOs()
{
  OsInfo info;

  // A missing or unreadable os-release leaves the distribution fields empty
  // instead of failing.  The wizard then offers a manual repository choice.
  std::ifstream f("/etc/os-release");
  if (f) {
    std::ostringstream content;
    content << f.rdbuf();
    info = ParseOsRelease(content.str());
  }

  // Architecture via uname
  struct utsname uts{};
  if (uname(&uts) == 0) info.arch = uts.machine;

  // Detect package manager
  if (access("/usr/bin/apt-get", X_OK) == 0
      || access("/bin/apt-get", X_OK) == 0) {
    info.pkg_mgr = PackageManager::Apt;
  } else if (access("/usr/bin/dnf", X_OK) == 0) {
    info.pkg_mgr = PackageManager::Dnf;
  } else if (access("/usr/bin/yum", X_OK) == 0) {
    info.pkg_mgr = PackageManager::Yum;
  } else if (access("/usr/bin/zypper", X_OK) == 0) {
    info.pkg_mgr = PackageManager::Zypper;
  }

  return info;
}
