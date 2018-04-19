/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2003-2008 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2016 Bareos GmbH & Co. KG

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
 * Kern Sibbald MMIII
 */

/* @file
 * Windows APIs that are different for each system.
 * We use pointers to the entry points so that a
 * single binary will run on all Windows systems.
 */

#include "include/bareos.h"

/*
 * Init with win9x, but maybe set to NT in InitWinAPI
 */
DWORD g_platform_id = VER_PLATFORM_WIN32_WINDOWS;
DWORD g_MinorVersion = 0;
DWORD g_MajorVersion = 0;

/*
 * API Pointers
 */
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

t_InetPton p_InetPton = NULL;

void InitWinAPIWrapper()
{
   OSVERSIONINFO osversioninfo = { sizeof(OSVERSIONINFO) };

   /*
    * Get the current OS version
    */
   if (!GetVersionEx(&osversioninfo)) {
      g_platform_id = 0;
   } else {
      g_platform_id = osversioninfo.dwPlatformId;
      g_MinorVersion = osversioninfo.dwMinorVersion;
      g_MajorVersion = osversioninfo.dwMajorVersion;
   }

   HMODULE hLib = LoadLibraryA("KERNEL32.DLL");
   if (hLib) {
      /*
       * Get logical drive calls
       */
      p_GetLogicalDriveStringsA = (t_GetLogicalDriveStringsA) GetProcAddress(hLib, "GetLogicalDriveStringsA");
      p_GetLogicalDriveStringsW = (t_GetLogicalDriveStringsW) GetProcAddress(hLib, "GetLogicalDriveStringsW");

      /*
       * Create process calls
       */
      p_CreateProcessA = (t_CreateProcessA) GetProcAddress(hLib, "CreateProcessA");
      p_CreateProcessW = (t_CreateProcessW) GetProcAddress(hLib, "CreateProcessW");

      /*
       * Create file calls
       */
      p_CreateFileA = (t_CreateFileA) GetProcAddress(hLib, "CreateFileA");
      p_CreateDirectoryA = (t_CreateDirectoryA) GetProcAddress(hLib, "CreateDirectoryA");
      p_CreateSymbolicLinkA = (t_CreateSymbolicLinkA) GetProcAddress(hLib, "CreateSymbolicLinkA");

#if (_WIN32_WINNT >= 0x0600)
      /*
       * File Information calls
       */
      p_GetFileInformationByHandleEx = (t_GetFileInformationByHandleEx) GetProcAddress(hLib, "GetFileInformationByHandleEx");
#endif

      /*
       * Attribute calls
       */
      p_GetFileAttributesA = (t_GetFileAttributesA) GetProcAddress(hLib, "GetFileAttributesA");
      p_GetFileAttributesExA = (t_GetFileAttributesExA) GetProcAddress(hLib, "GetFileAttributesExA");
      p_SetFileAttributesA = (t_SetFileAttributesA) GetProcAddress(hLib, "SetFileAttributesA");

      /*
       * Process calls
       */
      p_SetProcessShutdownParameters = (t_SetProcessShutdownParameters) GetProcAddress(hLib, "SetProcessShutdownParameters");

      /*
       * Char conversion calls
       */
      p_WideCharToMultiByte = (t_WideCharToMultiByte) GetProcAddress(hLib, "WideCharToMultiByte");
      p_MultiByteToWideChar = (t_MultiByteToWideChar) GetProcAddress(hLib, "MultiByteToWideChar");

      /*
       * Find files
       */
      p_FindFirstFileA = (t_FindFirstFileA) GetProcAddress(hLib, "FindFirstFileA");
      p_FindNextFileA = (t_FindNextFileA) GetProcAddress(hLib, "FindNextFileA");

      /*
       * Get and set directory
       */
      p_GetCurrentDirectoryA = (t_GetCurrentDirectoryA) GetProcAddress(hLib, "GetCurrentDirectoryA");
      p_SetCurrentDirectoryA = (t_SetCurrentDirectoryA) GetProcAddress(hLib, "SetCurrentDirectoryA");

      if (g_platform_id != VER_PLATFORM_WIN32_WINDOWS) {
         p_CreateFileW = (t_CreateFileW) GetProcAddress(hLib, "CreateFileW");
         p_CreateDirectoryW = (t_CreateDirectoryW) GetProcAddress(hLib, "CreateDirectoryW");
         p_CreateSymbolicLinkW = (t_CreateSymbolicLinkW) GetProcAddress(hLib, "CreateSymbolicLinkW");

         /*
          * Backup calls
          */
         p_BackupRead = (t_BackupRead) GetProcAddress(hLib, "BackupRead");
         p_BackupWrite = (t_BackupWrite) GetProcAddress(hLib, "BackupWrite");

         p_GetFileAttributesW = (t_GetFileAttributesW) GetProcAddress(hLib, "GetFileAttributesW");
         p_GetFileAttributesExW = (t_GetFileAttributesExW) GetProcAddress(hLib, "GetFileAttributesExW");
         p_SetFileAttributesW = (t_SetFileAttributesW) GetProcAddress(hLib, "SetFileAttributesW");
         p_FindFirstFileW = (t_FindFirstFileW) GetProcAddress(hLib, "FindFirstFileW");
         p_FindNextFileW = (t_FindNextFileW) GetProcAddress(hLib, "FindNextFileW");
         p_GetCurrentDirectoryW = (t_GetCurrentDirectoryW) GetProcAddress(hLib, "GetCurrentDirectoryW");
         p_SetCurrentDirectoryW = (t_SetCurrentDirectoryW) GetProcAddress(hLib, "SetCurrentDirectoryW");

         /*
          * Some special stuff we need for VSS
          * but static linkage doesn't work on Win 9x
          */
         p_GetVolumePathNameW = (t_GetVolumePathNameW) GetProcAddress(hLib, "GetVolumePathNameW");
         p_GetVolumeNameForVolumeMountPointW = (t_GetVolumeNameForVolumeMountPointW) GetProcAddress(hLib, "GetVolumeNameForVolumeMountPointW");

         p_AttachConsole = (t_AttachConsole) GetProcAddress(hLib, "AttachConsole");
      }
   }

   if (g_platform_id != VER_PLATFORM_WIN32_WINDOWS) {
      hLib = LoadLibraryA("MSVCRT.DLL");
      if (hLib) {
         /* unlink */
         p_wunlink = (t_wunlink) GetProcAddress(hLib, "_wunlink");
         /* wmkdir */
         p_wmkdir = (t_wmkdir) GetProcAddress(hLib, "_wmkdir");
      }

      hLib = LoadLibraryA("ADVAPI32.DLL");
      if (hLib) {
         p_OpenProcessToken = (t_OpenProcessToken) GetProcAddress(hLib, "OpenProcessToken");
         p_AdjustTokenPrivileges = (t_AdjustTokenPrivileges) GetProcAddress(hLib, "AdjustTokenPrivileges");
         p_LookupPrivilegeValue = (t_LookupPrivilegeValue) GetProcAddress(hLib, "LookupPrivilegeValueA");

         /*
          * EFS calls
          */
         p_OpenEncryptedFileRawA = (t_OpenEncryptedFileRawA) GetProcAddress(hLib, "OpenEncryptedFileRawA");
         p_OpenEncryptedFileRawW = (t_OpenEncryptedFileRawW) GetProcAddress(hLib, "OpenEncryptedFileRawW");
         p_ReadEncryptedFileRaw = (t_ReadEncryptedFileRaw) GetProcAddress(hLib, "ReadEncryptedFileRaw");
         p_WriteEncryptedFileRaw = (t_WriteEncryptedFileRaw) GetProcAddress(hLib, "WriteEncryptedFileRaw");
         p_CloseEncryptedFileRaw = (t_CloseEncryptedFileRaw) GetProcAddress(hLib, "CloseEncryptedFileRaw");
      }
   }

   hLib = LoadLibraryA("SHELL32.DLL");
   if (hLib) {
      p_SHGetFolderPath = (t_SHGetFolderPath) GetProcAddress(hLib, "SHGetFolderPathA");
   } else {
      /*
       * If SHELL32 isn't found try SHFOLDER for older systems
       */
      hLib = LoadLibraryA("SHFOLDER.DLL");
      if (hLib) {
         p_SHGetFolderPath = (t_SHGetFolderPath) GetProcAddress(hLib, "SHGetFolderPathA");
      }
   }

   hLib = LoadLibraryA("WS2_32.DLL");
   if (hLib) {
       p_InetPton = (t_InetPton) GetProcAddress(hLib, "InetPtonA");
   }

   atexit(Win32TSDCleanup);
}
