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

#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <sys/utsname.h>
#include <unistd.h>

/** Strip leading/trailing whitespace and optional surrounding quotes,
 * handling the shell-style escaping used by os-release values. */
static std::string Unquote(std::string s)
{
  const auto trim = [](std::string value) {
    const auto begin = value.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) return std::string{};
    const auto end = value.find_last_not_of(" \t\r\n");
    return value.substr(begin, end - begin + 1);
  };

  s = trim(s);
  if (s.empty()) return {};

  const char quote = s.front();
  if ((quote == '"' || quote == '\'') && s.size() >= 2 && s.back() == quote)
    s = s.substr(1, s.size() - 2);

  std::string unescaped;
  unescaped.reserve(s.size());
  bool escaping = false;
  for (const char ch : s) {
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
static std::vector<std::string> SplitWords(const std::string& value)
{
  std::vector<std::string> words;
  std::istringstream stream(value);
  std::string word;
  while (stream >> word) words.push_back(word);
  return words;
}

OsInfo ParseOsRelease(const std::string& content)
{
  OsInfo info;
  std::istringstream stream(content);
  std::string line;
  while (std::getline(stream, line)) {
    auto eq = line.find('=');
    if (eq == std::string::npos) continue;
    auto key = Unquote(line.substr(0, eq));
    auto val = Unquote(line.substr(eq + 1));
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
    info.pkg_mgr = "apt";
  } else if (access("/usr/bin/dnf", X_OK) == 0) {
    info.pkg_mgr = "dnf";
  } else if (access("/usr/bin/yum", X_OK) == 0) {
    info.pkg_mgr = "yum";
  } else if (access("/usr/bin/zypper", X_OK) == 0) {
    info.pkg_mgr = "zypper";
  } else {
    info.pkg_mgr = "unknown";
  }

  return info;
}
