/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2003-2010 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2023 Bareos GmbH & Co. KG

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
 * Windows APIs that are different for each system.
 * We use pointers to the entry points so that a
 * single binary will run on all Windows systems.
 *
 * Kern Sibbald MMIII
 */

#ifndef BAREOS_WIN32_INCLUDE_WINAPI_H_
#define BAREOS_WIN32_INCLUDE_WINAPI_H_

#if defined(HAVE_WIN32)
/*
 * Commented out native.h include statement, which is not distributed with the
 * free version of VC++, and which is not used in bareos.
 *
 * #if !defined(HAVE_MINGW) // native.h not present on mingw
 * #include <native.h>
 * #endif
 */
#  include <windef.h>
#  include <string>

#  ifndef POOLMEM
typedef char POOLMEM;
#  endif

// unicode enabling of win 32 needs some defines and functions

// using an average of 3 bytes per character is probably fine in
// practice but I believe that Windows actually uses UTF-16 encoding
// as opposed to UCS2 which means characters 0x10000-0x10ffff are
// valid and result in 4 byte UTF-8 encodings.
#  define MAX_PATH_UTF8 \
    MAX_PATH * 4  // strict upper bound on UTF-16 to UTF-8 conversion

// from
// http://msdn.microsoft.com/library/default.asp?url=/library/en-us/fileio/fs/getfileattributesex.asp
// In the ANSI version of this function, the name is limited to
// MAX_PATH characters. To extend this limit to 32,767 wide
// characters, call the Unicode version of the function and prepend
// "\\?\" to the path. For more information, see Naming a File.
#  define MAX_PATH_W 32767

std::wstring FromUtf8(std::string_view utf8);
std::string FromUtf16(std::wstring_view utf16);
int wchar_2_UTF8(POOLMEM*& pszUTF, const wchar_t* pszUCS);
int UTF8_2_wchar(POOLMEM*& pszUCS, const char* pszUTF);
int wchar_2_UTF8(char* pszUTF, const wchar_t* pszUCS, int cchChar);
BSTR str_2_BSTR(const char* pSrc);
char* BSTR_2_str(const BSTR pSrc);
std::wstring make_win32_path_UTF8_2_wchar(std::string_view utf8);

// init with win9x, but maybe set to NT in InitWinAPI
__declspec(dllimport) extern DWORD g_platform_id;
__declspec(dllimport) extern DWORD g_MinorVersion;
__declspec(dllimport) extern DWORD g_MajorVersion;

/* In ADVAPI32.DLL */
typedef BOOL(WINAPI* t_OpenProcessToken)(HANDLE, DWORD, PHANDLE);
typedef BOOL(WINAPI* t_AdjustTokenPrivileges)(HANDLE,
                                              BOOL,
                                              PTOKEN_PRIVILEGES,
                                              DWORD,
                                              PTOKEN_PRIVILEGES,
                                              PDWORD);
typedef BOOL(WINAPI* t_LookupPrivilegeValue)(LPCTSTR, LPCTSTR, PLUID);

__declspec(dllimport) extern t_OpenProcessToken p_OpenProcessToken;
__declspec(dllimport) extern t_AdjustTokenPrivileges p_AdjustTokenPrivileges;
__declspec(dllimport) extern t_LookupPrivilegeValue p_LookupPrivilegeValue;

/* In MSVCRT.DLL */
typedef int(__cdecl* t_wunlink)(const wchar_t*);
typedef int(__cdecl* t_wmkdir)(const wchar_t*);
typedef int(__cdecl* t_wopen)(const wchar_t*, int, ...);

__declspec(dllimport) extern t_wunlink p_wunlink;
__declspec(dllimport) extern t_wmkdir p_wmkdir;

/* In KERNEL32.DLL */
typedef BOOL(WINAPI* t_GetFileAttributesExA)(LPCSTR,
                                             GET_FILEEX_INFO_LEVELS,
                                             LPVOID);
typedef BOOL(WINAPI* t_GetFileAttributesExW)(LPCWSTR,
                                             GET_FILEEX_INFO_LEVELS,
                                             LPVOID);

#  if (_WIN32_WINNT >= 0x0600)
typedef DWORD(WINAPI* t_GetFileInformationByHandleEx)(HANDLE,
                                                      FILE_INFO_BY_HANDLE_CLASS,
                                                      LPVOID,
                                                      DWORD);
#  endif

typedef DWORD(WINAPI* t_GetFileAttributesA)(LPCSTR);
typedef DWORD(WINAPI* t_GetFileAttributesW)(LPCWSTR);
typedef BOOL(WINAPI* t_SetFileAttributesA)(LPCSTR, DWORD);
typedef BOOL(WINAPI* t_SetFileAttributesW)(LPCWSTR, DWORD);

typedef HANDLE(WINAPI* t_CreateFileA)(LPCSTR,
                                      DWORD,
                                      DWORD,
                                      LPSECURITY_ATTRIBUTES,
                                      DWORD,
                                      DWORD,
                                      HANDLE);
typedef HANDLE(WINAPI* t_CreateFileW)(LPCWSTR,
                                      DWORD,
                                      DWORD,
                                      LPSECURITY_ATTRIBUTES,
                                      DWORD,
                                      DWORD,
                                      HANDLE);

typedef DWORD(WINAPI* t_OpenEncryptedFileRawA)(LPCSTR, ULONG, PVOID);
typedef DWORD(WINAPI* t_OpenEncryptedFileRawW)(LPCWSTR, ULONG, PVOID);
typedef DWORD(WINAPI* t_ReadEncryptedFileRaw)(PFE_EXPORT_FUNC, PVOID, PVOID);
typedef DWORD(WINAPI* t_WriteEncryptedFileRaw)(PFE_IMPORT_FUNC, PVOID, PVOID);
typedef void(WINAPI* t_CloseEncryptedFileRaw)(PVOID);

typedef BOOL(WINAPI* t_CreateDirectoryA)(LPCSTR, LPSECURITY_ATTRIBUTES);
typedef BOOL(WINAPI* t_CreateDirectoryW)(LPCWSTR, LPSECURITY_ATTRIBUTES);

typedef BOOL(WINAPI* t_CreateSymbolicLinkA)(LPTSTR, LPTSTR, DWORD);
typedef BOOL(WINAPI* t_CreateSymbolicLinkW)(LPCWSTR, LPCWSTR, DWORD);

typedef BOOL(WINAPI* t_SetProcessShutdownParameters)(DWORD, DWORD);
typedef BOOL(
    WINAPI* t_BackupRead)(HANDLE, LPBYTE, DWORD, LPDWORD, BOOL, BOOL, LPVOID*);
typedef BOOL(
    WINAPI* t_BackupWrite)(HANDLE, LPBYTE, DWORD, LPDWORD, BOOL, BOOL, LPVOID*);

typedef int(WINAPI* t_WideCharToMultiByte)(UINT CodePage,
                                           DWORD,
                                           LPCWSTR,
                                           int,
                                           LPSTR,
                                           int,
                                           LPCSTR,
                                           LPBOOL);

typedef int(
    WINAPI* t_MultiByteToWideChar)(UINT, DWORD, LPCSTR, int, LPWSTR, int);
typedef HANDLE(WINAPI* t_FindFirstFileA)(LPCSTR, LPWIN32_FIND_DATAA);
typedef HANDLE(WINAPI* t_FindFirstFileW)(LPCWSTR, LPWIN32_FIND_DATAW);

typedef BOOL(WINAPI* t_FindNextFileA)(HANDLE, LPWIN32_FIND_DATAA);
typedef BOOL(WINAPI* t_FindNextFileW)(HANDLE, LPWIN32_FIND_DATAW);

typedef BOOL(WINAPI* t_SetCurrentDirectoryA)(LPCSTR);
typedef BOOL(WINAPI* t_SetCurrentDirectoryW)(LPCWSTR);

typedef DWORD(WINAPI* t_GetCurrentDirectoryA)(DWORD, LPSTR);
typedef DWORD(WINAPI* t_GetCurrentDirectoryW)(DWORD, LPWSTR);

typedef BOOL(WINAPI* t_GetVolumePathNameW)(LPCWSTR, LPWSTR, DWORD);
typedef BOOL(WINAPI* t_GetVolumeNameForVolumeMountPointW)(LPCWSTR,
                                                          LPWSTR,
                                                          DWORD);

typedef DWORD(WINAPI* t_GetLogicalDriveStringsA)(DWORD, LPSTR);
typedef DWORD(WINAPI* t_GetLogicalDriveStringsW)(DWORD, LPCWSTR);

typedef BOOL(WINAPI* t_AttachConsole)(DWORD);

typedef BOOL(WINAPI* t_CreateProcessA)(LPCSTR,
                                       LPSTR,
                                       LPSECURITY_ATTRIBUTES,
                                       LPSECURITY_ATTRIBUTES,
                                       BOOL,
                                       DWORD,
                                       PVOID,
                                       LPCSTR,
                                       LPSTARTUPINFOA,
                                       LPPROCESS_INFORMATION);
typedef BOOL(WINAPI* t_CreateProcessW)(LPCWSTR,
                                       LPWSTR,
                                       LPSECURITY_ATTRIBUTES,
                                       LPSECURITY_ATTRIBUTES,
                                       BOOL,
                                       DWORD,
                                       PVOID,
                                       LPCWSTR,
                                       LPSTARTUPINFOW,
                                       LPPROCESS_INFORMATION);

extern t_CreateProcessA p_CreateProcessA;
extern t_CreateProcessW p_CreateProcessW;

#  if (_WIN32_WINNT >= 0x0600)
extern t_GetFileInformationByHandleEx p_GetFileInformationByHandleEx;
#  endif

__declspec(dllimport) extern t_GetFileAttributesA p_GetFileAttributesA;
__declspec(dllimport) extern t_GetFileAttributesW p_GetFileAttributesW;

__declspec(dllimport) extern t_GetFileAttributesExA p_GetFileAttributesExA;
__declspec(dllimport) extern t_GetFileAttributesExW p_GetFileAttributesExW;

__declspec(dllimport) extern t_SetFileAttributesA p_SetFileAttributesA;
__declspec(dllimport) extern t_SetFileAttributesW p_SetFileAttributesW;

__declspec(dllimport) extern t_CreateFileA p_CreateFileA;
__declspec(dllimport) extern t_CreateFileW p_CreateFileW;

__declspec(dllimport) extern t_CreateDirectoryA p_CreateDirectoryA;
__declspec(dllimport) extern t_CreateDirectoryW p_CreateDirectoryW;

__declspec(dllimport) extern t_CreateSymbolicLinkA p_CreateSymbolicLinkA;
__declspec(dllimport) extern t_CreateSymbolicLinkW p_CreateSymbolicLinkW;

__declspec(dllimport) extern t_OpenEncryptedFileRawA p_OpenEncryptedFileRawA;
__declspec(dllimport) extern t_OpenEncryptedFileRawW p_OpenEncryptedFileRawW;
__declspec(dllimport) extern t_ReadEncryptedFileRaw p_ReadEncryptedFileRaw;
__declspec(dllimport) extern t_WriteEncryptedFileRaw p_WriteEncryptedFileRaw;
__declspec(dllimport) extern t_CloseEncryptedFileRaw p_CloseEncryptedFileRaw;

__declspec(dllimport) extern t_BackupRead p_BackupRead;
__declspec(dllimport) extern t_BackupWrite p_BackupWrite;

__declspec(dllimport) extern t_SetProcessShutdownParameters p_SetProcessShutdownParameters;

__declspec(dllimport) extern t_WideCharToMultiByte p_WideCharToMultiByte;
__declspec(dllimport) extern t_MultiByteToWideChar p_MultiByteToWideChar;

__declspec(dllimport) extern t_FindFirstFileA p_FindFirstFileA;
__declspec(dllimport) extern t_FindFirstFileW p_FindFirstFileW;

__declspec(dllimport) extern t_FindNextFileA p_FindNextFileA;
__declspec(dllimport) extern t_FindNextFileW p_FindNextFileW;

__declspec(dllimport) extern t_SetCurrentDirectoryA p_SetCurrentDirectoryA;
__declspec(dllimport) extern t_SetCurrentDirectoryW p_SetCurrentDirectoryW;

__declspec(dllimport) extern t_GetCurrentDirectoryA p_GetCurrentDirectoryA;
__declspec(dllimport) extern t_GetCurrentDirectoryW p_GetCurrentDirectoryW;

__declspec(dllimport) extern t_GetVolumePathNameW p_GetVolumePathNameW;
__declspec(dllimport) extern t_GetVolumeNameForVolumeMountPointW p_GetVolumeNameForVolumeMountPointW;

__declspec(dllimport) extern t_GetLogicalDriveStringsA p_GetLogicalDriveStringsA;
__declspec(dllimport) extern t_GetLogicalDriveStringsW p_GetLogicalDriveStringsW;

__declspec(dllimport) extern t_AttachConsole p_AttachConsole;

void InitWinAPIWrapper();

// In SHFOLDER.DLL on older systems, and now SHELL32.DLL
typedef BOOL(WINAPI* t_SHGetFolderPath)(HWND, int, HANDLE, DWORD, LPTSTR);
extern t_SHGetFolderPath p_SHGetFolderPath;

// In WS2_32.DLL
typedef INT(WSAAPI* t_InetPton)(INT Family,
                                PCTSTR pszAddrString,
                                PVOID pAddrBuf);

#endif
#endif  // BAREOS_WIN32_INCLUDE_WINAPI_H_
