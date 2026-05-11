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
#include "bareos_base64.h"

#include "base64.h"

std::string BareosBase64Encode(const uint8_t* data, int len)
{
  std::string result(BASE64_SIZE(len), '\0');
  const auto encoded = BinToBase64(
      result.data(), static_cast<int>(result.size()),
      const_cast<char*>(reinterpret_cast<const char*>(data)), len, true);
  result.resize(static_cast<size_t>(encoded));
  return result;
}
