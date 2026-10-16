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
#ifndef BAREOS_LIB_STAT_UTILS_H_
#define BAREOS_LIB_STAT_UTILS_H_

#include <cstdint>
#include <limits>

// A stat size/blocks value is unknown if it is negative (signed types) or
// the maximum value (unsigned types).
template <typename T> constexpr bool IsUnknownStatField(T value)
{
  if constexpr (std::numeric_limits<T>::is_signed) {
    return value < 0;
  } else {
    return value == std::numeric_limits<T>::max();
  }
}

// Number of 512-byte blocks needed to hold size bytes.
constexpr uint64_t BlocksFromSize(uint64_t size)
{
  return size / 512 + (size % 512 != 0);
}

#endif  // BAREOS_LIB_STAT_UTILS_H_
