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
/*
 * Human readable description of a Windows installation.
 *
 * This header is deliberately free of any windows.h dependency so that the
 * formatting logic can be unit tested on every platform.  The values that
 * describe the machine are gathered in osinfo_win32.cc and passed in as a
 * VersionInfo.
 */

#ifndef BAREOS_LIB_WINDOWS_VERSION_H_
#define BAREOS_LIB_WINDOWS_VERSION_H_

#include <cstdint>
#include <string>
#include <string_view>

namespace windows_version {

enum class ProductType
{
  kUnknown,
  kWorkstation,
  kServer,
};

enum class Architecture
{
  kUnknown,
  kX86,
  kX64,
  kArm64,
  kIa64,
};

// All string_views point into storage owned by the caller; they only have to
// stay alive until FormatVersion() returns.
struct VersionInfo {
  std::uint32_t major{};
  std::uint32_t minor{};
  std::uint32_t build{};
  std::uint32_t ubr{};

  std::string_view product_name;       // registry ProductName
  std::string_view display_version;    // DisplayVersion, else ReleaseId
  std::string_view edition_id;         // EditionID
  std::string_view installation_type;  // Client/Server/Server Core/WindowsPE
  std::string_view csd_version;        // "Service Pack 1"

  ProductType product_type{ProductType::kUnknown};
  Architecture arch{Architecture::kUnknown};
  bool is_winpe{false};
};

// The first Windows 10 build that is marketed as Windows 11.
inline constexpr std::uint32_t kFirstWindows11Build = 22000;

constexpr std::string_view ArchitectureSuffix(Architecture arch)
{
  switch (arch) {
    case Architecture::kX86:
      return ", 32-bit";
    case Architecture::kX64:
      return ", 64-bit";
    case Architecture::kArm64:
      return ", ARM64";
    case Architecture::kIa64:
      return ", IA64";
    case Architecture::kUnknown:
      break;
  }
  return {};
}

/* Windows 11 keeps reporting a ProductName of "Windows 10 ..." in the
 * registry, so the marketing name has to be derived from the build number. */
constexpr bool IsWindows11(std::uint32_t major,
                           std::uint32_t build,
                           ProductType type)
{
  return major >= 10 && build >= kFirstWindows11Build
         && type != ProductType::kServer;
}

// Used when the registry does not provide a usable ProductName.
constexpr std::string_view MarketingName(std::uint32_t major,
                                         std::uint32_t minor,
                                         std::uint32_t build,
                                         ProductType type)
{
  bool server = type == ProductType::kServer;

  if (major == 6) {
    switch (minor) {
      case 0:
        return server ? "Windows Server 2008" : "Windows Vista";
      case 1:
        return server ? "Windows Server 2008 R2" : "Windows 7";
      case 2:
        return server ? "Windows Server 2012" : "Windows 8";
      case 3:
        return server ? "Windows Server 2012 R2" : "Windows 8.1";
      default:
        return server ? "Windows Server" : "Windows";
    }
  }

  if (major >= 10) {
    if (!server) {
      return IsWindows11(major, build, type) ? "Windows 11" : "Windows 10";
    }
    if (build >= 26100) { return "Windows Server 2025"; }
    if (build >= 25398) { return "Windows Server, version 23H2"; }
    if (build >= 20348) { return "Windows Server 2022"; }
    if (build >= 17763) { return "Windows Server 2019"; }
    if (build >= 14393) { return "Windows Server 2016"; }
    return "Windows Server";
  }

  return server ? "Windows Server" : "Windows";
}

/* The LTSC/LTSB editions are only distinguishable by their EditionID; the
 * release year has to be derived from the build number. */
constexpr bool IsLongTermServicingEdition(std::string_view edition_id)
{
  return edition_id == "EnterpriseS" || edition_id == "EnterpriseSN"
         || edition_id == "IoTEnterpriseS" || edition_id == "IoTEnterpriseSK";
}

constexpr std::string_view LtscDesignation(std::uint32_t build)
{
  if (build >= 26100) { return "LTSC 2024"; }
  if (build >= 19044) { return "LTSC 2021"; }
  if (build >= 17763) { return "LTSC 2019"; }
  if (build >= 14393) { return "LTSB 2016"; }
  if (build >= 10240) { return "LTSB 2015"; }
  return "LTSC";
}

/* "Service Pack 3" becomes "SP3"; anything else is used verbatim.  Returns an
 * empty view when the input is not of the expected shape, in which case the
 * caller uses the original string. */
constexpr std::string_view ServicePackShortName(std::string_view csd_version)
{
  constexpr std::string_view prefix = "Service Pack ";
  if (csd_version.size() <= prefix.size()) { return {}; }
  if (csd_version.substr(0, prefix.size()) != prefix) { return {}; }

  std::string_view number = csd_version.substr(prefix.size());
  for (char c : number) {
    if (c < '0' || c > '9') { return {}; }
  }
  return number;
}

/* EditionID is written as "ServerStandard"/"ServerDatacenter" on server
 * systems; the redundant "Server" prefix is stripped when the edition is
 * appended to a name that already contains "Windows Server". */
constexpr std::string_view EditionDisplayName(std::string_view edition_id)
{
  constexpr std::string_view prefix = "Server";
  if (edition_id.size() > prefix.size()
      && edition_id.substr(0, prefix.size()) == prefix) {
    return edition_id.substr(prefix.size());
  }
  return edition_id;
}

constexpr bool IsServerCore(std::string_view installation_type)
{
  return installation_type.find("Core") != std::string_view::npos;
}

// Build the human readable description, e.g.
// "Microsoft Windows 11 Pro 24H2 (build 26100.4652), 64-bit"
std::string FormatVersion(const VersionInfo& info);

}  // namespace windows_version

#endif  // BAREOS_LIB_WINDOWS_VERSION_H_
