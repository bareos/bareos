/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2003-2008 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2024 Bareos GmbH & Co. KG

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

// Kern Sibbald MMIII

/* @file
 * Windows APIs that are different for each system.
 * We use pointers to the entry points so that a
 * single binary will run on all Windows systems.
 */

#include "include/bareos.h"
#include "fill_proc_address.h"
#include "winapi.h"

#define SET_API_POINTER_EX(name, symbol) \
  BareosFillProcAddress(p_##name, hLib, #symbol)

#define SET_API_POINTER(symbol) BareosFillProcAddress(p_##symbol, hLib, #symbol)

// Init with win9x, but maybe set to NT in InitWinAPI
DWORD g_platform_id = VER_PLATFORM_WIN32_WINDOWS;
DWORD g_MinorVersion = 0;
DWORD g_MajorVersion = 0;

// API Pointers
t_OpenProcessToken p_OpenProcessToken = NULL;
t_AdjustTokenPrivileges p_AdjustTokenPrivileges = NULL;
t_LookupPrivilegeValue p_LookupPrivilegeValue = NULL;

t_SetProcessShutdownParameters p_SetProcessShutdownParameters = NULL;

t_CreateFileA p_CreateFileA = NULL;
t_CreateFileW p_CreateFileW = NULL;

t_CreateDirectoryA p_CreateDirectoryA = NULL;
t_CreateDirectoryW p_CreateDirectoryW = NULL;

t_CreateSymbolicLinkA p_CreateSymbolicLinkA = NULL;
t_CreateSymbolicLinkW p_CreateSymbolicLinkW = NULL;

t_OpenEncryptedFileRawA p_OpenEncryptedFileRawA = NULL;
t_OpenEncryptedFileRawW p_OpenEncryptedFileRawW = NULL;
t_ReadEncryptedFileRaw p_ReadEncryptedFileRaw = NULL;
t_WriteEncryptedFileRaw p_WriteEncryptedFileRaw = NULL;
t_CloseEncryptedFileRaw p_CloseEncryptedFileRaw = NULL;

t_wunlink p_wunlink = NULL;
t_wmkdir p_wmkdir = NULL;

#if (_WIN32_WINNT >= 0x0600)
t_GetFileInformationByHandleEx p_GetFileInformationByHandleEx = NULL;
#endif

t_GetFileAttributesA p_GetFileAttributesA = NULL;
t_GetFileAttributesW p_GetFileAttributesW = NULL;

t_GetFileAttributesExA p_GetFileAttributesExA = NULL;
t_GetFileAttributesExW p_GetFileAttributesExW = NULL;

t_SetFileAttributesA p_SetFileAttributesA = NULL;
t_SetFileAttributesW p_SetFileAttributesW = NULL;

t_BackupRead p_BackupRead = NULL;
t_BackupWrite p_BackupWrite = NULL;

t_WideCharToMultiByte p_WideCharToMultiByte = NULL;
t_MultiByteToWideChar p_MultiByteToWideChar = NULL;

t_AttachConsole p_AttachConsole = NULL;

t_FindFirstFileA p_FindFirstFileA = NULL;
t_FindFirstFileW p_FindFirstFileW = NULL;

t_FindNextFileA p_FindNextFileA = NULL;
t_FindNextFileW p_FindNextFileW = NULL;

t_SetCurrentDirectoryA p_SetCurrentDirectoryA = NULL;
t_SetCurrentDirectoryW p_SetCurrentDirectoryW = NULL;

t_GetCurrentDirectoryA p_GetCurrentDirectoryA = NULL;
t_GetCurrentDirectoryW p_GetCurrentDirectoryW = NULL;

t_GetVolumePathNameW p_GetVolumePathNameW = NULL;
t_GetVolumeNameForVolumeMountPointW p_GetVolumeNameForVolumeMountPointW = NULL;

t_SHGetFolderPath p_SHGetFolderPath = NULL;

t_CreateProcessA p_CreateProcessA = NULL;
t_CreateProcessW p_CreateProcessW = NULL;

t_GetLogicalDriveStringsA p_GetLogicalDriveStringsA = NULL;
t_GetLogicalDriveStringsW p_GetLogicalDriveStringsW = NULL;


void InitWinAPIWrapper()
{
  OSVERSIONINFO osversioninfo = {sizeof(OSVERSIONINFO), 0, 0, 0, 0, 0};

  // Get the current OS version
  if (!GetVersionEx(&osversioninfo)) {
    g_platform_id = 0;
  } else {
    g_platform_id = osversioninfo.dwPlatformId;
    g_MinorVersion = osversioninfo.dwMinorVersion;
    g_MajorVersion = osversioninfo.dwMajorVersion;
  }

  HMODULE hLib = LoadLibraryA("KERNEL32.DLL");
  if (hLib) {
    // Get logical drive calls
    SET_API_POINTER(GetLogicalDriveStringsA);
    SET_API_POINTER(GetLogicalDriveStringsW);

    // Create process calls
    SET_API_POINTER(CreateProcessA);
    SET_API_POINTER(CreateProcessW);

    // Create file calls
    SET_API_POINTER(CreateFileA);
    SET_API_POINTER(CreateDirectoryA);
    SET_API_POINTER(CreateSymbolicLinkA);

#if (_WIN32_WINNT >= 0x0600)
    // File Information calls
    SET_API_POINTER(GetFileInformationByHandleEx);
#endif

    // Attribute calls
    SET_API_POINTER(GetFileAttributesA);
    SET_API_POINTER(GetFileAttributesExA);
    SET_API_POINTER(SetFileAttributesA);

    // Process calls
    SET_API_POINTER(SetProcessShutdownParameters);

    // Char conversion calls
    SET_API_POINTER(WideCharToMultiByte);
    SET_API_POINTER(MultiByteToWideChar);

    // Find files
    SET_API_POINTER(FindFirstFileA);
    SET_API_POINTER(FindNextFileA);

    // Get and set directory
    SET_API_POINTER(GetCurrentDirectoryA);
    SET_API_POINTER(SetCurrentDirectoryA);

    if (g_platform_id != VER_PLATFORM_WIN32_WINDOWS) {
      SET_API_POINTER(CreateFileW);
      SET_API_POINTER(CreateDirectoryW);
      SET_API_POINTER(CreateSymbolicLinkW);

      // Backup calls
      SET_API_POINTER(BackupRead);
      SET_API_POINTER(BackupWrite);

      SET_API_POINTER(GetFileAttributesW);
      SET_API_POINTER(GetFileAttributesExW);
      SET_API_POINTER(SetFileAttributesW);
      SET_API_POINTER(FindFirstFileW);
      SET_API_POINTER(FindNextFileW);
      SET_API_POINTER(GetCurrentDirectoryW);
      SET_API_POINTER(SetCurrentDirectoryW);

      /* Some special stuff we need for VSS
       * but static linkage doesn't work on Win 9x */
      SET_API_POINTER(GetVolumePathNameW);
      SET_API_POINTER(GetVolumeNameForVolumeMountPointW);

      SET_API_POINTER(AttachConsole);
    }
  }

  if (g_platform_id != VER_PLATFORM_WIN32_WINDOWS) {
    hLib = LoadLibraryA("MSVCRT.DLL");
    if (hLib) {
      SET_API_POINTER_EX(wunlink, _wunlink);
      SET_API_POINTER_EX(wmkdir, _wmkdir);
    }

    hLib = LoadLibraryA("ADVAPI32.DLL");
    if (hLib) {
      SET_API_POINTER(OpenProcessToken);
      SET_API_POINTER(AdjustTokenPrivileges);
      SET_API_POINTER_EX(LookupPrivilegeValue, LookupPrivilegeValueA);

      // EFS calls
      SET_API_POINTER(OpenEncryptedFileRawA);
      SET_API_POINTER(OpenEncryptedFileRawW);
      SET_API_POINTER(ReadEncryptedFileRaw);
      SET_API_POINTER(WriteEncryptedFileRaw);
      SET_API_POINTER(CloseEncryptedFileRaw);
    }
  }

  hLib = LoadLibraryA("SHELL32.DLL");
  if (hLib) {
    SET_API_POINTER_EX(SHGetFolderPath, SHGetFolderPathA);
  } else {
    // If SHELL32 isn't found try SHFOLDER for older systems
    hLib = LoadLibraryA("SHFOLDER.DLL");
    if (hLib) { SET_API_POINTER_EX(SHGetFolderPath, SHGetFolderPathA); }
  }

  dyn::LoadDynamicFunctions();
}

namespace dyn {
dynamic_function::dynamic_function(function_registry& registry, const char* lib)
{
  registry[lib].emplace_back(this);
}

void LoadDynamicFunctions()
{
  for (auto& [lib, funs] : dynamic_functions) {
    auto library = LoadLibraryA(lib.c_str());

    if (!library) continue;

    for (auto* f : funs) { f->load(library); }
  }
}
};  // namespace dyn
