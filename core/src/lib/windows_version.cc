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

#include "windows_version.h"

namespace windows_version {

namespace {

constexpr std::string_view kWhitespace = " \t\r\n";

std::string_view Trim(std::string_view value)
{
  auto begin = value.find_first_not_of(kWhitespace);
  if (begin == std::string_view::npos) { return {}; }
  auto end = value.find_last_not_of(kWhitespace);
  return value.substr(begin, end - begin + 1);
}

bool Contains(std::string_view haystack, std::string_view needle)
{
  return haystack.find(needle) != std::string_view::npos;
}

void Append(std::string& out, std::string_view value)
{
  if (value.empty()) { return; }
  if (!out.empty() && out.back() != ' ') { out += ' '; }
  out += value;
}

std::string BuildString(const VersionInfo& info)
{
  std::string result = " (build " + std::to_string(info.build);
  if (info.ubr != 0) { result += '.' + std::to_string(info.ubr); }
  result += ')';
  return result;
}

/* Windows 11 and Windows Server 2019 and later report a ProductName of
 * "Windows 10 ..." resp. an outdated name, so the registry value cannot be
 * trusted verbatim. */
std::string BaseName(const VersionInfo& info)
{
  std::string_view product_name = Trim(info.product_name);
  std::string_view marketing_name = MarketingName(info.major, info.minor,
                                                  info.build, info.product_type);

  if (product_name.empty()) {
    std::string name{marketing_name};
    Append(name, EditionDisplayName(Trim(info.edition_id)));
    return name;
  }

  std::string name{product_name};
  if (IsWindows11(info.major, info.build, info.product_type)) {
    if (auto pos = name.find("Windows 10"); pos != std::string::npos) {
      name.replace(pos, std::string_view{"Windows 10"}.size(), "Windows 11");
    } else if (!Contains(name, "Windows 11")) {
      name = std::string{marketing_name};
      Append(name, EditionDisplayName(Trim(info.edition_id)));
    }
  }
  return name;
}

}  // namespace

std::string FormatVersion(const VersionInfo& info)
{
  if (info.is_winpe) {
    std::string result = "Windows PE";
    result += BuildString(info);
    result += ArchitectureSuffix(info.arch);
    return result;
  }

  std::string result = BaseName(info);
  if (Trim(result).empty()) { return "Unknown Windows version"; }

  std::string_view edition_id = Trim(info.edition_id);
  if (IsLongTermServicingEdition(edition_id) && !Contains(result, "LTSC")
      && !Contains(result, "LTSB")) {
    Append(result, LtscDesignation(info.build));
  }

  if (IsServerCore(Trim(info.installation_type))
      && !Contains(result, "Core installation")) {
    Append(result, "(Core installation)");
  }

  if (std::string_view display_version = Trim(info.display_version);
      !display_version.empty() && !Contains(result, display_version)) {
    Append(result, display_version);
  }

  if (std::string_view csd_version = Trim(info.csd_version);
      !csd_version.empty()) {
    if (std::string_view number = ServicePackShortName(csd_version);
        !number.empty()) {
      Append(result, "SP");
      result += number;
    } else {
      Append(result, csd_version);
    }
  }

  result += BuildString(info);
  result += ArchitectureSuffix(info.arch);

  if (result.compare(0, 10, "Microsoft ") != 0) {
    result.insert(0, "Microsoft ");
  }

  return result;
}

}  // namespace windows_version
