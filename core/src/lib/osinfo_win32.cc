/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2007-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2016-2026 Bareos GmbH & Co. KG

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
 * Kern Sibbald, August 2007
 *
 * Note, some of the original Bareos Windows startup and service handling code
 * was derived from VNC code that was used in apcupsd then ported to
 * Bareos.  However, since then the code has been significantly enhanced
 * and largely rewritten.
 *
 * Evidently due to the nature of Windows startup code and service
 * handling code, certain similarities remain. Thanks to the original
 * VNC authors.
 *
 * This is a generic main routine, which is used by all three
 * of the daemons. Each one compiles it with slightly different
 * #defines.
 */

#include <cstdint>
#include <cstring>
#include <mutex>
#include <string>
#include <string_view>
#include <windows.h>
#include <tchar.h>
#include <stdio.h>

#include "fill_proc_address.h"
#include "osinfo.h"
#include "windows_version.h"

static char win_os[300];
static bool win_os_initialized = false;
static std::mutex init_mutex;

static bool GetWindowsVersionString(LPTSTR osbuf, int maxsiz);

const char* GetOsInfoString()
{
  if (!win_os_initialized) {
    const std::lock_guard<std::mutex> lock(init_mutex);
    if (!win_os_initialized) {
      GetWindowsVersionString(win_os, sizeof(win_os) - 1);
      win_os_initialized = true;
    }
  }
  return win_os;
}

namespace {

using windows_version::Architecture;
using windows_version::ProductType;
using windows_version::VersionInfo;

typedef void(WINAPI* PGNSI)(LPSYSTEM_INFO);

/* RTL_OSVERSIONINFOEXW is not declared by every SDK/mingw header set, so the
 * layout (which is ABI stable and matches OSVERSIONINFOEXW) is reproduced
 * here. RtlGetVersion() is used instead of GetVersionEx()/VerifyVersionInfo()
 * because those lie about the OS version unless the calling process carries
 * an application manifest declaring compatibility with the running Windows
 * release. */
struct RtlOsVersionInfoExW {
  ULONG dwOSVersionInfoSize;
  ULONG dwMajorVersion;
  ULONG dwMinorVersion;
  ULONG dwBuildNumber;
  ULONG dwPlatformId;
  WCHAR szCSDVersion[128];
  USHORT wServicePackMajor;
  USHORT wServicePackMinor;
  USHORT wSuiteMask;
  UCHAR wProductType;
  UCHAR wReserved;
};

using RtlGetVersionFn = LONG(WINAPI*)(RtlOsVersionInfoExW*);

bool GetTrueOsVersion(RtlOsVersionInfoExW* info)
{
  memset(info, 0, sizeof(*info));
  info->dwOSVersionInfoSize = sizeof(*info);

  RtlGetVersionFn RtlGetVersion;
  if (!BareosFillProcAddress(RtlGetVersion, GetModuleHandle(TEXT("ntdll.dll")),
                             "RtlGetVersion")) {
    return false;
  }
  // STATUS_SUCCESS
  return RtlGetVersion(info) == 0;
}

std::string RegGetString(HKEY key, const char* value)
{
  DWORD type;
  DWORD size;
  if (RegGetValueA(key, "", value, RRF_RT_REG_SZ, &type, nullptr, &size)
      != ERROR_SUCCESS) {
    return {};
  }
  if (type != REG_SZ || size == 0) { return {}; }

  // size includes the terminating 0
  std::string result;
  result.resize(size - 1);
  if (RegGetValueA(key, "", value, RRF_RT_REG_SZ, &type, result.data(), &size)
      != ERROR_SUCCESS) {
    return {};
  }
  return result;
}

std::uint32_t RegGetDword(HKEY key, const char* value)
{
  DWORD type;
  DWORD data = 0;
  DWORD size = sizeof(data);
  if (RegGetValueA(key, "", value, RRF_RT_REG_DWORD, &type, &data, &size)
      != ERROR_SUCCESS) {
    return 0;
  }
  return data;
}

std::uint32_t ParseUint32(std::string_view value)
{
  std::uint32_t result = 0;
  for (char c : value) {
    if (c < '0' || c > '9') { break; }
    result = (result * 10) + static_cast<std::uint32_t>(c - '0');
  }
  return result;
}

Architecture ArchitectureFromProcessorArchitecture(WORD arch)
{
  switch (arch) {
    case PROCESSOR_ARCHITECTURE_AMD64:
      return Architecture::kX64;
    case PROCESSOR_ARCHITECTURE_INTEL:
      return Architecture::kX86;
    case PROCESSOR_ARCHITECTURE_ARM64:
      return Architecture::kArm64;
    case PROCESSOR_ARCHITECTURE_IA64:
      return Architecture::kIa64;
    default:
      return Architecture::kUnknown;
  }
}

// Backing storage for the string_views referenced by VersionInfo; VersionInfo
// itself only borrows these values, so they have to outlive its use.
struct RegistryStrings {
  std::string product_name;
  std::string display_version;
  std::string edition_id;
  std::string installation_type;
  std::string csd_version;
  std::string current_version;
};

/* Reads HKLM\Software\Microsoft\Windows NT\CurrentVersion. Returns false if
 * the key cannot be opened or no usable build number was found, in which
 * case the caller falls back to RtlGetVersion() alone. */
bool FillFromRegistry(RegistryStrings* strings, VersionInfo* info)
{
  HKEY key;
  auto ret = RegOpenKeyExA(HKEY_LOCAL_MACHINE,
                           "Software\\Microsoft\\Windows NT\\CurrentVersion", 0,
                           KEY_READ, &key);
  if (ret != ERROR_SUCCESS) { return false; }

  strings->product_name = RegGetString(key, "ProductName");
  // DisplayVersion (e.g. "24H2") replaced ReleaseId in Windows 10 1909+.
  strings->display_version = RegGetString(key, "DisplayVersion");
  if (strings->display_version.empty()) {
    strings->display_version = RegGetString(key, "ReleaseId");
  }
  strings->edition_id = RegGetString(key, "EditionID");
  strings->installation_type = RegGetString(key, "InstallationType");
  strings->csd_version = RegGetString(key, "CSDVersion");
  strings->current_version = RegGetString(key, "CurrentVersion");

  info->build = ParseUint32(RegGetString(key, "CurrentBuildNumber"));
  info->ubr = RegGetDword(key, "UBR");

  // Windows 10+ exposes the version as separate DWORDs; older releases only
  // have the "6.1"-style CurrentVersion string.
  info->major = RegGetDword(key, "CurrentMajorVersionNumber");
  info->minor = RegGetDword(key, "CurrentMinorVersionNumber");
  if (info->major == 0 && !strings->current_version.empty()) {
    std::string_view version = strings->current_version;
    auto dot = version.find('.');
    info->major = ParseUint32(version.substr(0, dot));
    if (dot != std::string_view::npos) {
      info->minor = ParseUint32(version.substr(dot + 1));
    }
  }

  RegCloseKey(key);
  return info->build != 0;
}

}  // namespace

// Get Windows version display string
static bool GetWindowsVersionString(LPTSTR osbuf, int maxsiz)
{
  RegistryStrings strings;
  VersionInfo info{};

  bool have_registry_values = FillFromRegistry(&strings, &info);

  RtlOsVersionInfoExW true_version;
  bool have_true_version = GetTrueOsVersion(&true_version);

  if (!have_registry_values && !have_true_version) {
    strncpy(osbuf, "Unknown Windows version.", maxsiz);
    osbuf[maxsiz - 1] = '\0';
    return true;
  }

  info.product_name = strings.product_name;
  info.display_version = strings.display_version;
  info.edition_id = strings.edition_id;
  info.installation_type = strings.installation_type;
  info.csd_version = strings.csd_version;
  info.is_winpe = strings.installation_type == "WindowsPE";

  if (have_true_version) {
    info.product_type = (true_version.wProductType == VER_NT_WORKSTATION)
                            ? ProductType::kWorkstation
                            : ProductType::kServer;
    // RtlGetVersion() is authoritative; use it if the registry did not
    // provide major/build values.
    if (info.major == 0) { info.major = true_version.dwMajorVersion; }
    if (info.minor == 0) { info.minor = true_version.dwMinorVersion; }
    if (info.build == 0) { info.build = true_version.dwBuildNumber; }
  } else {
    info.product_type = ProductType::kWorkstation;
  }

  SYSTEM_INFO si;
  memset(&si, 0, sizeof(si));
  if (PGNSI pGNSI;
      BareosFillProcAddress(pGNSI, GetModuleHandle(TEXT("kernel32.dll")),
                            "GetNativeSystemInfo")) {
    pGNSI(&si);
  } else {
    GetSystemInfo(&si);
  }
  info.arch = ArchitectureFromProcessorArchitecture(si.wProcessorArchitecture);

  std::string formatted = windows_version::FormatVersion(info);
  strncpy(osbuf, formatted.c_str(), maxsiz);
  osbuf[maxsiz - 1] = '\0';
  return true;
}
