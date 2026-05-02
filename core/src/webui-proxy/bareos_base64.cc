/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2024-2026 Bareos GmbH & Co. KG

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

// clang-format off
static const char kBase64Digits[64] = {
    'A','B','C','D','E','F','G','H','I','J','K','L','M',
    'N','O','P','Q','R','S','T','U','V','W','X','Y','Z',
    'a','b','c','d','e','f','g','h','i','j','k','l','m',
    'n','o','p','q','r','s','t','u','v','w','x','y','z',
    '0','1','2','3','4','5','6','7','8','9','+','/'
};
// clang-format on

std::string BareosBase64Encode(const uint8_t* data, int len, bool compatible)
{
  std::string result;
  result.reserve((len * 4 + 2) / 3);

  uint32_t reg = 0;
  int rem = 0;

  for (int i = 0; i < len;) {
    if (rem < 6) {
      reg <<= 8;
      if (compatible) {
        reg |= static_cast<uint32_t>(static_cast<uint8_t>(data[i++]));
      } else {
        // compatible=false: signed sign-extension into the 32-bit register
        reg |= static_cast<uint32_t>(static_cast<int8_t>(data[i++]));
      }
      rem += 8;
    }
    uint32_t save = reg;
    reg >>= (rem - 6);
    result += kBase64Digits[reg & 0x3Fu];
    reg = save;
    rem -= 6;
  }

  if (rem) {
    uint32_t mask = (1u << rem) - 1u;
    if (compatible) {
      result += kBase64Digits[(reg & mask) << (6 - rem)];
    } else {
      result += kBase64Digits[reg & mask];
    }
  }

  return result;
}
