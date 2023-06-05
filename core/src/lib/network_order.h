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

namespace network_order {

template <typename T> constexpr T byteswap(T);

template <> constexpr std::uint64_t byteswap(std::uint64_t x)
{
  return __builtin_bswap64(x);
}
template <> constexpr std::uint32_t byteswap(std::uint32_t x)
{
  return __builtin_bswap32(x);
}
template <> constexpr std::uint16_t byteswap(std::uint16_t x)
{
  return __builtin_bswap16(x);
}
template <> constexpr std::uint8_t byteswap(std::uint8_t x) { return x; }

template <typename T> constexpr T byteswap(T val)
{
  using nosign = std::make_unsigned_t<T>;
  return static_cast<T>(byteswap<nosign>(static_cast<nosign>(val)));
}

#if !defined(BYTE_ORDER) || !defined(LITTLE_ENDIAN)
#  error "Could not determine endianess."
#elif (BYTE_ORDER == LITTLE_ENDIAN)
template <typename T> T to_network(T val) { return byteswap(val); }
template <typename T> T to_native(T val) { return byteswap(val); }
#else
template <typename T> T to_network(T val) { return val; }
template <typename T> T to_native(T val) { return val; }
#endif

struct from_network_order {
} from_network_order_v;

struct from_native_order {
} from_native_order_v;

template <typename T> struct network_value {
  T as_network;
  T as_native() const { return to_native(as_network); }
  operator T() const { return as_native(); }

  network_value() = default;
  network_value(from_network_order, T val) : as_network{val} {}
  network_value(from_native_order, T val) : as_network{to_network(val)} {}
  network_value(T val) : network_value{from_native_order_v, val} {}
};

// TODO: can we do this on instantiaton of every template
//       and check that P(network_value<T>) == P(T) ?
static_assert(std::is_standard_layout_v<network_value<int>>);
static_assert(std::is_pod_v<network_value<int>>);
static_assert(std::has_unique_object_representations_v<network_value<int>>);

template <typename U> static network_value<U> of_network(U network)
{
  return network_value<U>{from_network_order_v, network};
}

template <typename U> static network_value<U> of_native(U native)
{
  return network_value<U>{from_native_order_v, native};
}
}
#endif  // BAREOS_LIB_NETWORK_ORDER_H_
