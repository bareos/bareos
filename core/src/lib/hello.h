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

#ifndef BAREOS_LIB_HELLO_H_
#define BAREOS_LIB_HELLO_H_


#include <string_view>

#include "lib/global_resource.h"
#include "lib/version.h"

template <global_resource::Type my_type, global_resource::Type target_type>
struct hello_formatter;

template <>
struct hello_formatter<global_resource::Type::Director,
                       global_resource::Type::Storage> {
  static constexpr global_resource::Type auth_type
      = global_resource::Type::Director;
  static std::string format(std::string_view name);
};
template <>
struct hello_formatter<global_resource::Type::Director,
                       global_resource::Type::Client> {
  static constexpr global_resource::Type auth_type
      = global_resource::Type::Director;
  static std::string format(std::string_view name);
};

template <>
struct hello_formatter<global_resource::Type::Storage,
                       global_resource::Type::Storage> {
  static constexpr global_resource::Type auth_type = global_resource::Type::Job;
  static std::string format(std::string_view name);
};
template <>
struct hello_formatter<global_resource::Type::Storage,
                       global_resource::Type::Client> {
  static constexpr global_resource::Type auth_type = global_resource::Type::Job;
  static std::string format(std::string_view name);
};

template <>
struct hello_formatter<global_resource::Type::Client,
                       global_resource::Type::Storage> {
  static constexpr global_resource::Type auth_type = global_resource::Type::Job;
  static std::string format(std::string_view name);
};
template <>
struct hello_formatter<global_resource::Type::Client,
                       global_resource::Type::Director> {
  static constexpr global_resource::Type auth_type
      = global_resource::Type::Client;
  static std::string format(std::string_view name);
};

template <>
struct hello_formatter<global_resource::Type::Console,
                       global_resource::Type::Director> {
  static constexpr global_resource::Type auth_type
      = global_resource::Type::Console;
  static std::string format(std::string_view name);
};


#endif  // BAREOS_LIB_HELLO_H_
