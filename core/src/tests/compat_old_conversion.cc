/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2023-2023 Bareos GmbH & Co. KG

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
#include "compat_old_conversion.h"
#include <win32/include/winapi.h>
extern char* bstrncpy(char* dest, const char* src, int maxlen);
extern char* bstrncpy(char* dest, PoolMem& src, int maxlen);
namespace old_path_conversion {
void Dmsg0(...) {}
void Dmsg1(...) {}
void Dmsg2(...) {}
void Dmsg3(...) {}
void Dmsg4(...) {}
void ASSERT(...) {}
static const int debuglevel = 0;

inline bool IsPathSeparator(int ch) { return ch == '/' || ch == '\\'; }
thread_vss_path_convert global_tvpc{};
void Win32SetPathConvert(t_pVSSPathConvert convert, t_pVSSPathConvertW convertW)
{
  global_tvpc.pPathConvert = convert;
  global_tvpc.pPathConvertW = convertW;
}
static inline thread_vss_path_convert* Win32GetPathConvert()
{
  if (global_tvpc.pPathConvert) {
    return &global_tvpc;
  } else {
    return nullptr;
  }
}
static int UTF8_2_wchar(POOLMEM*& ppszUCS, const char* pszUTF)
{
  /* The return value is the number of wide characters written to the buffer.
   * convert null terminated string from utf-8 to ucs2, enlarge buffer if
   * necessary */
  if (p_MultiByteToWideChar) {
    // strlen of UTF8 +1 is enough
    DWORD cchSize = (strlen(pszUTF) + 1);
    ppszUCS = CheckPoolMemorySize(ppszUCS, cchSize * sizeof(wchar_t));

    int nRet = p_MultiByteToWideChar(CP_UTF8, 0, pszUTF, -1, (LPWSTR)ppszUCS,
                                     cchSize);
    ASSERT(nRet > 0);
    return nRet;
  } else {
    return 0;
  }
}
static inline void conv_unix_to_vss_win32_path(const char* name,
                                               char* win32_name,
                                               DWORD dwSize)
{
  const char* fname = name;
  char* tname = win32_name;
  thread_vss_path_convert* tvpc;

  Dmsg0(debuglevel, "Enter convert_unix_to_win32_path\n");

  tvpc = Win32GetPathConvert();
  if (IsPathSeparator(name[0]) && IsPathSeparator(name[1]) && name[2] == '.'
      && IsPathSeparator(name[3])) {
    *win32_name++ = '\\';
    *win32_name++ = '\\';
    *win32_name++ = '.';
    *win32_name++ = '\\';

    name += 4;
  } else if (!tvpc) {
    // FUTURE NOTE: removed g_platform_id; win95 does not matter anymore
    // Allow path to be 32767 bytes
    *win32_name++ = '\\';
    *win32_name++ = '\\';
    *win32_name++ = '?';
    *win32_name++ = '\\';
  }

  while (*name) {
    // Check for Unix separator and convert to Win32
    if (name[0] == '/' && name[1] == '/') { /* double slash? */
      name++;                               /* yes, skip first one */
    }
    if (*name == '/') {
      *win32_name++ = '\\'; /* convert char */
                            // If Win32 separator that is "quoted", remove quote
    } else if (*name == '\\' && name[1] == '\\') {
      *win32_name++ = '\\';
      name++; /* skip first \ */
    } else {
      *win32_name++ = *name; /* copy character */
    }
    name++;
  }

  /* Strip any trailing slash, if we stored something
   * but leave "c:\" with backslash (root directory case) */
  if (*fname != 0 && win32_name[-1] == '\\' && strlen(fname) != 3) {
    win32_name[-1] = 0;
  } else {
    *win32_name = 0;
  }

  /* Here we convert to VSS specific file name which
   * can get longer because VSS will make something like
   * \\\\?\\GLOBALROOT\\Device\\HarddiskVolumeShadowCopy1\\bareos\\uninstall.exe
   * from c:\bareos\uninstall.exe */
  Dmsg1(debuglevel, "path=%s\n", tname);
  if (tvpc) {
    POOLMEM* pszBuf = GetPoolMemory(PM_FNAME);

    pszBuf = CheckPoolMemorySize(pszBuf, dwSize);
    bstrncpy(pszBuf, tname, strlen(tname) + 1);
    tvpc->pPathConvert(pszBuf, tname, dwSize);
    FreePoolMemory(pszBuf);
  }

  Dmsg1(debuglevel, "Leave cvt_u_to_win32_path path=%s\n", tname);
}

void unix_name_to_win32(POOLMEM*& win32_name, const char* name)
{
  DWORD dwSize;

  /* One extra byte should suffice, but we double it
   * add MAX_PATH bytes for VSS shadow copy name. */
  dwSize = 2 * strlen(name) + MAX_PATH;
  win32_name = CheckPoolMemorySize(win32_name, dwSize);
  conv_unix_to_vss_win32_path(name, win32_name, dwSize);
}

static inline POOLMEM* make_wchar_win32_path(POOLMEM* pszUCSPath,
                                             BOOL* pBIsRawPath /*= NULL*/)
{
  Dmsg0(debuglevel, "Enter make_wchar_win32_path\n");
  if (pBIsRawPath) { *pBIsRawPath = FALSE; /* Initialize, set later */ }

  if (!p_GetCurrentDirectoryW) {
    Dmsg0(debuglevel, "Leave make_wchar_win32_path no change \n");
    return pszUCSPath;
  }

  wchar_t* name = (wchar_t*)pszUCSPath;

  // If it has already the desired form, exit without changes
  if (wcslen(name) > 3 && wcsncmp(name, L"\\\\?\\", 4) == 0) {
    Dmsg0(debuglevel, "Leave make_wchar_win32_path no change \n");
    return pszUCSPath;
  }

  wchar_t* pwszBuf = (wchar_t*)GetPoolMemory(PM_FNAME);
  wchar_t* pwszCurDirBuf = (wchar_t*)GetPoolMemory(PM_FNAME);
  DWORD dwCurDirPathSize = 0;

  // Get buffer with enough size (name+max 6. wchars+1 null terminator
  DWORD dwBufCharsNeeded = (wcslen(name) + 7);
  pwszBuf = (wchar_t*)CheckPoolMemorySize((POOLMEM*)pwszBuf,
                                          dwBufCharsNeeded * sizeof(wchar_t));

  /* Add \\?\ to support 32K long filepaths
   * it is important to make absolute paths, so we add drive and
   * current path if necessary */
  BOOL bAddDrive = TRUE;
  BOOL bAddCurrentPath = TRUE;
  BOOL bAddPrefix = TRUE;

  // Does path begin with drive? if yes, it is absolute
  if (iswalpha(name[0]) && name[1] == ':' && IsPathSeparator(name[2])) {
    bAddDrive = FALSE;
    bAddCurrentPath = FALSE;
  }

  // Is path absolute?
  if (IsPathSeparator(name[0])) bAddCurrentPath = FALSE;

  // Is path relative to itself?, if yes, skip ./
  if (name[0] == '.' && IsPathSeparator(name[1])) { name += 2; }

  // Is path of form '//./'?
  if (IsPathSeparator(name[0]) && IsPathSeparator(name[1]) && name[2] == '.'
      && IsPathSeparator(name[3])) {
    bAddDrive = FALSE;
    bAddCurrentPath = FALSE;
    bAddPrefix = FALSE;
    if (pBIsRawPath) { *pBIsRawPath = TRUE; }
  }

  int nParseOffset = 0;

  // Add 4 bytes header
  if (bAddPrefix) {
    nParseOffset = 4;
    wcscpy(pwszBuf, L"\\\\?\\");
  }

  // Get current path if needed
  if (bAddDrive || bAddCurrentPath) {
    dwCurDirPathSize = p_GetCurrentDirectoryW(0, NULL);
    if (dwCurDirPathSize > 0) {
      /* Get directory into own buffer as it may either return c:\... or
       * \\?\C:\.... */
      pwszCurDirBuf = (wchar_t*)CheckPoolMemorySize(
          (POOLMEM*)pwszCurDirBuf, (dwCurDirPathSize + 1) * sizeof(wchar_t));
      p_GetCurrentDirectoryW(dwCurDirPathSize, pwszCurDirBuf);
    } else {
      // We have no info for doing so
      bAddDrive = FALSE;
      bAddCurrentPath = FALSE;
    }
  }

  // Add drive if needed
  if (bAddDrive && !bAddCurrentPath) {
    wchar_t szDrive[3];

    if (IsPathSeparator(pwszCurDirBuf[0]) && IsPathSeparator(pwszCurDirBuf[1])
        && pwszCurDirBuf[2] == '?' && IsPathSeparator(pwszCurDirBuf[3])) {
      // Copy drive character
      szDrive[0] = pwszCurDirBuf[4];
    } else {
      // Copy drive character
      szDrive[0] = pwszCurDirBuf[0];
    }

    szDrive[1] = ':';
    szDrive[2] = 0;

    wcscat(pwszBuf, szDrive);
    nParseOffset += 2;
  }

  // Add path if needed
  if (bAddCurrentPath) {
    // The 1 additional character is for the eventually added backslash
    dwBufCharsNeeded += dwCurDirPathSize + 1;
    pwszBuf = (wchar_t*)CheckPoolMemorySize((POOLMEM*)pwszBuf,
                                            dwBufCharsNeeded * sizeof(wchar_t));

    /* Get directory into own buffer as it may either return c:\... or
     * \\?\C:\.... */
    if (IsPathSeparator(pwszCurDirBuf[0]) && IsPathSeparator(pwszCurDirBuf[1])
        && pwszCurDirBuf[2] == '?' && IsPathSeparator(pwszCurDirBuf[3])) {
      // Copy complete string
      wcscpy(pwszBuf, pwszCurDirBuf);
    } else {
      // Append path
      wcscat(pwszBuf, pwszCurDirBuf);
    }

    nParseOffset = wcslen((LPCWSTR)pwszBuf);

    // Check if path ends with backslash, if not, add one
    if (!IsPathSeparator(pwszBuf[nParseOffset - 1])) {
      wcscat(pwszBuf, L"\\");
      nParseOffset++;
    }
  }

  wchar_t* win32_name = &pwszBuf[nParseOffset];
  wchar_t* name_start = name;
  wchar_t previous_char = 0;
  wchar_t next_char = 0;

  while (*name) {
    /* Check for Unix separator and convert to Win32, eliminating duplicate
     * separators. */
    if (IsPathSeparator(*name)) {
      // Don't add a trailing slash.
      next_char = *(name + 1);
      if (previous_char != ':' && next_char == '\0') {
        name++;
        continue;
      }
      previous_char = '\\';
      *win32_name++ = '\\'; /* convert char */

      /* Eliminate consecutive slashes, but not at the start so that \\.\ still
       * works. */
      if (name_start != name && IsPathSeparator(name[1])) { name++; }
    } else {
      previous_char = *name;
      *win32_name++ = *name; /* copy character */
    }
    name++;
  }

  // NULL Terminate string
  *win32_name = 0;

  /* Here we convert to VSS specific file name which can get longer because VSS
   * will make something like
   * \\\\?\\GLOBALROOT\\Device\\HarddiskVolumeShadowCopy1\\bareos\\uninstall.exe
   * from c:\bareos\uninstall.exe */
  thread_vss_path_convert* tvpc = Win32GetPathConvert();
  if (tvpc) {
    // Is output buffer large enough?
    pwszBuf = (wchar_t*)CheckPoolMemorySize(
        (POOLMEM*)pwszBuf, (dwBufCharsNeeded + MAX_PATH) * sizeof(wchar_t));
    // Create temp. buffer
    wchar_t* pszBuf = (wchar_t*)GetPoolMemory(PM_FNAME);
    pszBuf = (wchar_t*)CheckPoolMemorySize(
        (POOLMEM*)pszBuf, (dwBufCharsNeeded + MAX_PATH) * sizeof(wchar_t));
    if (bAddPrefix) {
      nParseOffset = 4;
    } else {
      nParseOffset = 0;
    }

    wcsncpy(pszBuf, &pwszBuf[nParseOffset], wcslen(pwszBuf) + 1 - nParseOffset);
    tvpc->pPathConvertW(pszBuf, pwszBuf, dwBufCharsNeeded + MAX_PATH);
    FreePoolMemory((POOLMEM*)pszBuf);
  }

  FreePoolMemory(pszUCSPath);
  FreePoolMemory((POOLMEM*)pwszCurDirBuf);

  Dmsg1(debuglevel, "Leave make_wchar_win32_path=%s\n", pwszBuf);
  return (POOLMEM*)pwszBuf;
}

int make_win32_path_UTF8_2_wchar(POOLMEM*& pszUCS,
                                 const char* pszUTF,
                                 BOOL* pBIsRawPath /*= NULL*/)
{
  int nRet;
  // FUTURE NOTE: removed some caching logic here

  /* Helper to convert from utf-8 to UCS-2 and to complete a path for 32K path
   * syntax */
  nRet = UTF8_2_wchar(pszUCS, pszUTF);

  // FUTURE NOTE: always use 32K conversions
  // Add \\?\ to support 32K long filepaths
  pszUCS = make_wchar_win32_path(pszUCS, pBIsRawPath);

  // FUTURE NOTE: removed some caching logic here.

  return nRet;
}
}  // namespace old_path_conversion
