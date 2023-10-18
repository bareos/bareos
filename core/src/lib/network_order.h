/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2023-2023 Bareos GmbH & Co. KG

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation, which is
   listed in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   Affero General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
*/

#ifndef BAREOS_LIB_NETWORK_ORDER_H_
#define BAREOS_LIB_NETWORK_ORDER_H_

#include <cstdint>
#include <cstring>
#include <type_traits>

#include "config.h"

namespace network_order {

template <typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
constexpr T byteswap(T i)
{
  using U = std::make_unsigned_t<T>;
  static_assert(sizeof(U) == sizeof(T));

  U u;
  std::memcpy(&u, &i, sizeof(U));
  if constexpr (sizeof(T) == 1) {
    u = u;
  } else if constexpr (sizeof(T) == 2) {
    u = ((u & 0x00ff) << 8) | ((u & 0xff00) >> 8);
  } else if constexpr (sizeof(T) == 4) {
    u = ((u & 0x000000ff) << 24) | ((u & 0x0000ff00) << 8)
        | ((u & 0x00ff0000) >> 8) | ((u & 0xff000000) >> 24);
  } else if constexpr (sizeof(T) == 8) {
    u = ((u & 0x00000000000000ff) << 56) | ((u & 0x000000000000ff00) << 40)
        | ((u & 0x0000000000ff0000) << 24) | ((u & 0x00000000ff000000) << 8)
        | ((u & 0x000000ff00000000) >> 8) | ((u & 0x0000ff0000000000) >> 24)
        | ((u & 0x00ff000000000000) >> 40) | ((u & 0xff00000000000000) >> 56);
  } else {
    static_assert(false, "Unsupported integer-width");
  }

  std::memcpy(&i, &u, sizeof(T));
  return i;
}

namespace {
#if defined(HAVE_BIG_ENDIAN)
inline constexpr bool needs_flip = false;
#else
inline constexpr bool needs_flip = true;
#endif
};  // namespace

template <typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
struct network {
  [[implicit]] constexpr network(T value) noexcept
  {
    // since we have user provided constructors, we are not a pod type
    // but we should ensure that we meet the other criteria
    static_assert(sizeof(*this) == sizeof(T));
    static_assert(alignof(*this) == alignof(T));
    store(value);
  }

  constexpr network() noexcept : network{T{}} {}

  constexpr inline T load() const noexcept
  {
    if constexpr (needs_flip) {
      return byteswap(storage);
    } else {
      return storage;
    }
  }

  constexpr inline void store(T value) noexcept
  {
    if constexpr (needs_flip) {
      storage = byteswap(value);
    } else {
      storage = value;
    }
  }

  constexpr operator T() const noexcept { return load(); }

 private:
  T storage;
};

} /* namespace network_order */
#endif  // BAREOS_LIB_NETWORK_ORDER_H_
