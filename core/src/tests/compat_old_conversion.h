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
#ifndef BAREOS_TESTS_COMPAT_OLD_CONVERSION_H_
#define BAREOS_TESTS_COMPAT_OLD_CONVERSION_H_
#include <lib/mem_pool.h>
#include <winsock2.h>
#include <windows.h>
namespace old_path_conversion {
typedef bool (*t_pVSSPathConvert)(const char* szFilePath,
                                  char* szShadowPath,
                                  int nBuflen);
typedef bool (*t_pVSSPathConvertW)(const wchar_t* szFilePath,
                                   wchar_t* szShadowPath,
                                   int nBuflen);
struct thread_vss_path_convert {
  t_pVSSPathConvert pPathConvert;
  t_pVSSPathConvertW pPathConvertW;
};

void Win32SetPathConvert(t_pVSSPathConvert, t_pVSSPathConvertW);
void unix_name_to_win32(POOLMEM*& win32_name, const char* name);
int make_win32_path_UTF8_2_wchar(POOLMEM*& pszUCS,
                                 const char* pszUTF,
                                 BOOL* pBIsRawPath = NULL);
};      // namespace old_path_conversion
#endif  // BAREOS_TESTS_COMPAT_OLD_CONVERSION_H_
