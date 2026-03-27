/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2024-2025 Bareos GmbH & Co. KG

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
#include <stdexcept>
#include <string>

#include <sys/utsname.h>
#include <unistd.h>

/** Strip leading/trailing whitespace and optional surrounding quotes. */
static std::string Unquote(std::string s)
{
  // trim whitespace
  auto b = s.find_first_not_of(" \t\r\n");
  auto e = s.find_last_not_of(" \t\r\n");
  if (b == std::string::npos) return {};
  s = s.substr(b, e - b + 1);
  // strip quotes
  if (s.size() >= 2 && s.front() == '"' && s.back() == '"')
    s = s.substr(1, s.size() - 2);
  return s;
}

OsInfo DetectOs()
{
  OsInfo info;

  // Parse /etc/os-release
  std::ifstream f("/etc/os-release");
  if (!f) throw std::runtime_error("Cannot open /etc/os-release");

  std::string line;
  while (std::getline(f, line)) {
    auto eq = line.find('=');
    if (eq == std::string::npos) continue;
    auto key = line.substr(0, eq);
    auto val = Unquote(line.substr(eq + 1));
    if (key == "ID")                info.distro      = val;
    else if (key == "VERSION_ID")   info.version     = val;
    else if (key == "VERSION_CODENAME") info.codename = val;
    else if (key == "PRETTY_NAME")  info.pretty_name = val;
  }

  // Fallback: derive codename from PRETTY_NAME for Ubuntu-like ("24.04 LTS")
  // Leave it empty if not found; the frontend handles that.

  // Architecture via uname
  struct utsname uts {};
  if (uname(&uts) == 0) info.arch = uts.machine;

  // Detect package manager
  if (access("/usr/bin/apt-get", X_OK) == 0 ||
      access("/bin/apt-get", X_OK) == 0) {
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
