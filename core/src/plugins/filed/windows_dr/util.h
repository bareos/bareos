/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2025-2025 Bareos GmbH & Co. KG

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

#ifndef BAREOS_PLUGINS_FILED_WINDOWS_DR_UTIL_H_
#define BAREOS_PLUGINS_FILED_WINDOWS_DR_UTIL_H_

#include <string>
#include <string_view>

namespace {

#if defined(HAVE_WIN32)

static inline std::string FromUtf16(std::wstring_view utf16)
{
  // WideCharToMultiByte does not handle empty strings
  if (utf16.size() == 0) { return {}; }

  // if the buffer is to small (or not supplied) the function returns
  // the number of bytes required
  DWORD required = WideCharToMultiByte(CP_UTF8, 0, utf16.data(), utf16.size(),
                                       nullptr, 0, nullptr, nullptr);
  if (required == 0) { return "<impossible conversion>"; }
  std::string utf8(required, '\0');

  // if the buffer is big enough the function returns the number of
  // bytes written
  DWORD written
      = WideCharToMultiByte(CP_UTF8, 0, utf16.data(), utf16.size(), utf8.data(),
                            utf8.size(), nullptr, nullptr);

  if (written != required) { return "<impossible conversion>"; }

  utf8.resize(written);
  return utf8;
}

static inline std::string FromUtf16(const wchar_t* utf16p)
{
  if (!utf16p) { return {}; }
  return FromUtf16(std::wstring_view{utf16p});
}

#endif

static inline bool GetBit(unsigned char* data, std::size_t index)
{
  return (data[index / CHAR_BIT] >> (index % CHAR_BIT)) & 0x1;
}

template <class... Ts> struct overloads : Ts... {
  using Ts::operator()...;
};
}  // namespace

#endif  // BAREOS_PLUGINS_FILED_WINDOWS_DR_UTIL_H_
