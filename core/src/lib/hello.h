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

/* A connection that happens during e.g. a backup from fd to storage, will set
 * from = Client, to = Storage, type = Job (!),
 * as the client will authenticate as "job".
 * Name will always be the name used during authentication, so in this case
 * its the unique name of the job (jcr->JobName).
 */

struct connection_triplet {
  global_resource::Type to;    // this should be equal to "my type"
  global_resource::Type from;  // this is the actual type of other end
  global_resource::Type type;  // this is the type used for auth

  auto operator<=>(const connection_triplet&) const = default;
};

struct ParsedHello {
  connection_triplet triplet;
  std::string name;

  int fd_protocol_version;  // only for fd -> dir connections
  bool old_console;         // only for console -> dir connections

  uint32_t bareos_version;  // = 0 means < 26.0.0
};

std::optional<ParsedHello> parse_hello(std::string_view hello_msg);

#endif  // BAREOS_LIB_HELLO_H_
