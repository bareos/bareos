/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2020-2025 Bareos GmbH & Co. KG

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

#include <fstream>
#include <string>
#include <optional>

#include "include/config.h"
#include "osinfo.h"
#include "util.h"

struct os_release {
  os_release()
  {
    if (auto file = std::ifstream("/etc/os-release")) {
      std::string line;
      std::optional<std::string> name, version, pretty_name;
      while (std::getline(file, line)) {
        // we expect the lines to be of the form key=value
        // and ignore all non matching lines

        if (std::optional split = split_first(line, '=')) {
          auto [key, value] = *split;

          // we expect that value is surrounded by double quotes
          if (value.size() > 0 && value.front() == '"') {
            value.remove_prefix(1);
          }
          if (value.size() > 0 && value.back() == '"') {
            value.remove_suffix(1);
          }

          // we dont want empty names
          if (value.size() == 0) { continue; }

          if (key == "NAME") {
            name.emplace(value);
          } else if (key == "VERSION") {
            version.emplace(value);
          } else if (key == "PRETTY_NAME") {
            pretty_name.emplace(value);
          }
        }
      }

      if (pretty_name) {
        os_version = std::move(pretty_name);
      } else if (name && version) {
        os_version.emplace(*name + " " + *version);
      }
    }
  }

  std::optional<std::string> os_version;
};

const char* GetOsInfoString()
{
  // output priority
  // 1) pretty_name
  // 2) name + version (both have to be there)
  // 3) DISTVER

  static os_release info{};

  if (info.os_version) { return info.os_version->c_str(); }

  return DISTVER;
}
