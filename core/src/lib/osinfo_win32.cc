/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2007-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2016-2022 Bareos GmbH & Co. KG

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

#include <mutex>
#include <windows.h>
#include <tchar.h>
#include <stdio.h>

#include "fill_proc_address.h"
#include "osinfo.h"

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


#ifndef PRODUCT_UNLICENSED
#  define PRODUCT_UNLICENSED 0xABCDABCD
#  define PRODUCT_BUSINESS 0x00000006
#  define PRODUCT_BUSINESS_N 0x00000010
#  define PRODUCT_CLUSTER_SERVER 0x00000012
#  define PRODUCT_DATACENTER_SERVER 0x00000008
#  define PRODUCT_DATACENTER_SERVER_CORE 0x0000000C
#  define PRODUCT_DATACENTER_SERVER_CORE_V 0x00000027
#  define PRODUCT_DATACENTER_SERVER_V 0x00000025
#  define PRODUCT_ENTERPRISE 0x00000004
#  define PRODUCT_ENTERPRISE_E 0x00000046
#  define PRODUCT_ENTERPRISE_N 0x0000001B
#  define PRODUCT_ENTERPRISE_SERVER 0x0000000A
#  define PRODUCT_ENTERPRISE_SERVER_CORE 0x0000000E
#  define PRODUCT_ENTERPRISE_SERVER_CORE_V 0x00000029
#  define PRODUCT_ENTERPRISE_SERVER_IA64 0x0000000F
#  define PRODUCT_ENTERPRISE_SERVER_V 0x00000026
#  define PRODUCT_HOME_BASIC 0x00000002
#  define PRODUCT_HOME_BASIC_E 0x00000043
#  define PRODUCT_HOME_BASIC_N 0x00000005
#  define PRODUCT_HOME_PREMIUM 0x00000003
#  define PRODUCT_HOME_PREMIUM_E 0x00000044
#  define PRODUCT_HOME_PREMIUM_N 0x0000001A
#  define PRODUCT_HYPERV 0x0000002A
#  define PRODUCT_MEDIUMBUSINESS_SERVER_MANAGEMENT 0x0000001E
#  define PRODUCT_MEDIUMBUSINESS_SERVER_MESSAGING 0x00000020
#  define PRODUCT_MEDIUMBUSINESS_SERVER_SECURITY 0x0000001F
#  define PRODUCT_PROFESSIONAL 0x00000030
#  define PRODUCT_PROFESSIONAL_E 0x00000045
#  define PRODUCT_PROFESSIONAL_N 0x00000031
#  define PRODUCT_SERVER_FOR_SMALLBUSINESS 0x00000018
#  define PRODUCT_SERVER_FOR_SMALLBUSINESS_V 0x00000023
#  define PRODUCT_SERVER_FOUNDATION 0x00000021
#  define PRODUCT_SMALLBUSINESS_SERVER 0x00000009
#  define PRODUCT_SOLUTION_EMBEDDEDSERVER 0x00000038
#  define PRODUCT_STANDARD_SERVER 0x00000007
#  define PRODUCT_STANDARD_SERVER_CORE 0x0000000D
#  define PRODUCT_STANDARD_SERVER_CORE_V 0x00000028
#  define PRODUCT_STANDARD_SERVER_V 0x00000024
#  define PRODUCT_STARTER 0x0000000B
#  define PRODUCT_STARTER_E 0x00000042
#  define PRODUCT_STARTER_N 0x0000002F
#  define PRODUCT_STORAGE_ENTERPRISE_SERVER 0x00000017
#  define PRODUCT_STORAGE_EXPRESS_SERVER 0x00000014
#  define PRODUCT_STORAGE_STANDARD_SERVER 0x00000015
#  define PRODUCT_STORAGE_WORKGROUP_SERVER 0x00000016
#  define PRODUCT_UNDEFINED 0x00000000
#  define PRODUCT_ULTIMATE 0x00000001
#  define PRODUCT_ULTIMATE_E 0x00000047
#  define PRODUCT_ULTIMATE_N 0x0000001C
#  define PRODUCT_WEB_SERVER 0x00000011
#  define PRODUCT_WEB_SERVER_CORE 0x0000001D

#  define PRODUCT_SMALLBUSINESS_SERVER_PREMIUM 0x19
#  define SM_SERVERR2 89
#  define VER_SERVER_NT 0x80000000

#endif

#ifndef PRODUCT_PROFESSIONAL
#  define PRODUCT_PROFESSIONAL 0x00000030
#endif
#ifndef VER_SUITE_STORAGE_SERVER
#  define VER_SUITE_STORAGE_SERVER 0x00002000
#endif
#ifndef VER_SUITE_COMPUTE_SERVER
#  define VER_SUITE_COMPUTE_SERVER 0x00004000
#endif

/* Unknown value */
#undef VER_SUITE_WH_SERVER
#define VER_SUITE_WH_SERVER -1


typedef void(WINAPI* PGNSI)(LPSYSTEM_INFO);
typedef BOOL(WINAPI* PGPI)(DWORD, DWORD, DWORD, DWORD, PDWORD);

// Get Windows version display string
static bool GetWindowsVersionString(LPTSTR osbuf, int maxsiz)
{
  OSVERSIONINFOEX osvi;
  SYSTEM_INFO si;
  BOOL bOsVersionInfoEx;
  DWORD dwType;

  memset(&si, 0, sizeof(SYSTEM_INFO));
  memset(&osvi, 0, sizeof(OSVERSIONINFOEX));

  osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);

  if (!(bOsVersionInfoEx = GetVersionEx((OSVERSIONINFO*)&osvi))) return 1;

  // Call GetNativeSystemInfo if supported or GetSystemInfo otherwise.

  if (PGNSI pGNSI;
      BareosFillProcAddress(pGNSI, GetModuleHandle(TEXT("kernel32.dll")),
                            "GetNativeSystemInfo")) {
    pGNSI(&si);
  } else {
    GetSystemInfo(&si);
  }

  if (osvi.dwPlatformId == VER_PLATFORM_WIN32_NT && osvi.dwMajorVersion > 4) {
    strncpy(osbuf, TEXT("Microsoft "), maxsiz);

    // Test for the specific product.
    switch (osvi.dwMajorVersion) {
      case 6:
        switch (osvi.dwMinorVersion) {
          case 0:
            if (osvi.wProductType == VER_NT_WORKSTATION) {
              strncat(osbuf, TEXT("Windows Vista "), maxsiz);
            } else {
              strncat(osbuf, TEXT("Windows Server 2008 "), maxsiz);
            }
            break;
          case 1:
            if (osvi.wProductType == VER_NT_WORKSTATION) {
              strncat(osbuf, TEXT("Windows 7 "), maxsiz);
            } else {
              strncat(osbuf, TEXT("Windows Server 2008 R2 "), maxsiz);
            }
            break;
          case 2:
            if (osvi.wProductType == VER_NT_WORKSTATION) {
              strncat(osbuf, TEXT("Windows 8 "), maxsiz);
            } else {
              strncat(osbuf, TEXT("Windows Server 2012 "), maxsiz);
            }
            break;
          default:
            strncat(osbuf, TEXT("Windows Unknown Release "), maxsiz);
            break;
        }

        if (PGPI pGPI;
            BareosFillProcAddress(pGPI, GetModuleHandle(TEXT("kernel32.dll")),
                                  "GetProductInfo")) {
          pGPI(osvi.dwMajorVersion, osvi.dwMinorVersion, 0, 0, &dwType);
        } else {
          dwType = PRODUCT_HOME_BASIC;
        }

        switch (dwType) {
          case PRODUCT_ULTIMATE:
            strncat(osbuf, TEXT("Ultimate Edition"), maxsiz);
            break;
          case PRODUCT_PROFESSIONAL:
            strncat(osbuf, TEXT("Professional"), maxsiz);
            break;
          case PRODUCT_HOME_PREMIUM:
            strncat(osbuf, TEXT("Home Premium Edition"), maxsiz);
            break;
          case PRODUCT_HOME_BASIC:
            strncat(osbuf, TEXT("Home Basic Edition"), maxsiz);
            break;
          case PRODUCT_ENTERPRISE:
            strncat(osbuf, TEXT("Enterprise Edition"), maxsiz);
            break;
          case PRODUCT_BUSINESS:
            strncat(osbuf, TEXT("Business Edition"), maxsiz);
            break;
          case PRODUCT_STARTER:
            strncat(osbuf, TEXT("Starter Edition"), maxsiz);
            break;
          case PRODUCT_CLUSTER_SERVER:
            strncat(osbuf, TEXT("Cluster Server Edition"), maxsiz);
            break;
          case PRODUCT_DATACENTER_SERVER:
            strncat(osbuf, TEXT("Datacenter Edition"), maxsiz);
            break;
          case PRODUCT_DATACENTER_SERVER_CORE:
            strncat(osbuf, TEXT("Datacenter Edition (core installation)"),
                    maxsiz);
            break;
          case PRODUCT_ENTERPRISE_SERVER:
            strncat(osbuf, TEXT("Enterprise Edition"), maxsiz);
            break;
          case PRODUCT_ENTERPRISE_SERVER_CORE:
            strncat(osbuf, TEXT("Enterprise Edition (core installation)"),
                    maxsiz);
            break;
          case PRODUCT_ENTERPRISE_SERVER_IA64:
            strncat(osbuf, TEXT("Enterprise Edition for Itanium-based Systems"),
                    maxsiz);
            break;
          case PRODUCT_SMALLBUSINESS_SERVER:
            strncat(osbuf, TEXT("Small Business Server"), maxsiz);
            break;
          case PRODUCT_SMALLBUSINESS_SERVER_PREMIUM:
            strncat(osbuf, TEXT("Small Business Server Premium Edition"),
                    maxsiz);
            break;
          case PRODUCT_STANDARD_SERVER:
            strncat(osbuf, TEXT("Standard Edition"), maxsiz);
            break;
          case PRODUCT_STANDARD_SERVER_CORE:
            strncat(osbuf, TEXT("Standard Edition (core installation)"),
                    maxsiz);
            break;
          case PRODUCT_WEB_SERVER:
            strncat(osbuf, TEXT("Web Server Edition"), maxsiz);
            break;
        }
        break;
      case 5:
        switch (osvi.dwMinorVersion) {
          case 2:
            if (GetSystemMetrics(SM_SERVERR2)) {
              strncat(osbuf, TEXT("Windows Server 2003 R2 "), maxsiz);
            } else if (osvi.wSuiteMask & VER_SUITE_STORAGE_SERVER) {
              strncat(osbuf, TEXT("Windows Storage Server 2003"), maxsiz);
            } else if (osvi.wSuiteMask & VER_SUITE_WH_SERVER) {
              strncat(osbuf, TEXT("Windows Home Server"), maxsiz);
            } else if (osvi.wProductType == VER_NT_WORKSTATION
                       && si.wProcessorArchitecture
                              == PROCESSOR_ARCHITECTURE_AMD64) {
              strncat(osbuf, TEXT("Windows XP Professional x64 Edition"),
                      maxsiz);
            } else {
              strncat(osbuf, TEXT("Windows Server 2003 "), maxsiz);
            }

            // Test for the server type.
            if (osvi.wProductType != VER_NT_WORKSTATION) {
              if (si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_IA64) {
                if (osvi.wSuiteMask & VER_SUITE_DATACENTER) {
                  strncat(osbuf,
                          TEXT("Datacenter Edition for Itanium-based Systems"),
                          maxsiz);
                } else if (osvi.wSuiteMask & VER_SUITE_ENTERPRISE) {
                  strncat(osbuf,
                          TEXT("Enterprise Edition for Itanium-based Systems"),
                          maxsiz);
                }
              } else if (si.wProcessorArchitecture
                         == PROCESSOR_ARCHITECTURE_AMD64) {
                if (osvi.wSuiteMask & VER_SUITE_DATACENTER) {
                  strncat(osbuf, TEXT("Datacenter x64 Edition"), maxsiz);
                } else if (osvi.wSuiteMask & VER_SUITE_ENTERPRISE) {
                  strncat(osbuf, TEXT("Enterprise x64 Edition"), maxsiz);
                } else {
                  strncat(osbuf, TEXT("Standard x64 Edition"), maxsiz);
                }
              } else {
                if (osvi.wSuiteMask & VER_SUITE_COMPUTE_SERVER) {
                  strncat(osbuf, TEXT("Compute Cluster Edition"), maxsiz);
                } else if (osvi.wSuiteMask & VER_SUITE_DATACENTER) {
                  strncat(osbuf, TEXT("Datacenter Edition"), maxsiz);
                } else if (osvi.wSuiteMask & VER_SUITE_ENTERPRISE) {
                  strncat(osbuf, TEXT("Enterprise Edition"), maxsiz);
                } else if (osvi.wSuiteMask & VER_SUITE_BLADE) {
                  strncat(osbuf, TEXT("Web Edition"), maxsiz);
                } else {
                  strncat(osbuf, TEXT("Standard Edition"), maxsiz);
                }
              }
            }
            break;
          case 1:
            strncat(osbuf, TEXT("Windows XP "), maxsiz);
            if (osvi.wSuiteMask & VER_SUITE_PERSONAL) {
              strncat(osbuf, TEXT("Home Edition"), maxsiz);
            } else {
              strncat(osbuf, TEXT("Professional"), maxsiz);
            }
            break;
          case 0:
            strncat(osbuf, TEXT("Windows 2000 "), maxsiz);
            if (osvi.wProductType == VER_NT_WORKSTATION) {
              strncat(osbuf, TEXT("Professional"), maxsiz);
            } else {
              if (osvi.wSuiteMask & VER_SUITE_DATACENTER) {
                strncat(osbuf, TEXT("Datacenter Server"), maxsiz);
              } else if (osvi.wSuiteMask & VER_SUITE_ENTERPRISE) {
                strncat(osbuf, TEXT("Advanced Server"), maxsiz);
              } else {
                strncat(osbuf, TEXT("Server"), maxsiz);
              }
            }
            break;
        }
        break;
      default:
        break;
    }

    // Include service pack (if any) and build number.
    if (_tcslen(osvi.szCSDVersion) > 0) {
      strncat(osbuf, TEXT(" "), maxsiz);
      strncat(osbuf, osvi.szCSDVersion, maxsiz);
    }

    char buf[80];

    snprintf(buf, 80, " (build %d)", (int)osvi.dwBuildNumber);
    strncat(osbuf, buf, maxsiz);

    if (osvi.dwMajorVersion >= 6) {
      if (si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64)
        strncat(osbuf, TEXT(", 64-bit"), maxsiz);
      else if (si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_INTEL)
        strncat(osbuf, TEXT(", 32-bit"), maxsiz);
    }

    return true;
  } else {
    strncpy(osbuf, "Unknown Windows version.", maxsiz);
    return true;
  }
}
