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

#include "lib/hello.h"
#include <fmt/format.h>

struct bashed_printer {
  std::string_view to_print;
};

template <>
struct fmt::formatter<bashed_printer> : fmt::formatter<std::string_view> {
  constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.begin(); }

  auto format(const bashed_printer& s, fmt::format_context& ctx) const
  {
    auto out = ctx.out();

    for (char c : s.to_print) { *out++ = c == ' ' ? 0x1 : c; }

    return out;
  }
};

using global_resource::Type;

std::string hello_formatter<Type::Director, Type::Storage>::format(
    std::string_view name)
{
  auto& version = kBareosVersion;
  return fmt::format("Hello Director {} calling Version=\"{}.{}.{}\"\n",
                     bashed_printer{name}, version.Major, version.Minor,
                     version.Patch);
}
std::string hello_formatter<Type::Director, Type::Client>::format(
    std::string_view name)
{
  auto& version = kBareosVersion;
  return fmt::format("Hello Director {} calling Version=\"{}.{}.{}\"\n",
                     bashed_printer{name}, version.Major, version.Minor,
                     version.Patch);
}

std::string hello_formatter<Type::Storage, Type::Storage>::format(
    std::string_view name)
{
  auto& version = kBareosVersion;
  return fmt::format("Hello Start Storage Job {} Version=\"{}.{}.{}\"\n",
                     bashed_printer{name}, version.Major, version.Minor,
                     version.Patch);
}
std::string hello_formatter<Type::Storage, Type::Client>::format(
    std::string_view name)
{
  auto& version = kBareosVersion;
  return fmt::format(
      "Hello Storage calling Start Job {} Version=\"{}.{}.{}\"\n",
      bashed_printer{name}, version.Major, version.Minor, version.Patch);
}

std::string hello_formatter<Type::Client, Type::Storage>::format(
    std::string_view name)
{
  auto& version = kBareosVersion;
  return fmt::format("Hello Start Job {} Version=\"{}.{}.{}\"\n",
                     bashed_printer{name}, version.Major, version.Minor,
                     version.Patch);
}
std::string hello_formatter<Type::Client, Type::Director>::format(
    std::string_view name)
{
  auto& version = kBareosVersion;
  return fmt::format(
      "Hello Client {} FdProtocolVersion=54 calling Version=\"{}.{}.{}\"\n",
      bashed_printer{name}, version.Major, version.Minor, version.Patch);
}

std::string hello_formatter<Type::Console, Type::Director>::format(
    std::string_view name)
{
  auto& version = kBareosVersion;
  return fmt::format("Hello {} calling version {} Version=\"{}.{}.{}\"\n",
                     bashed_printer{name}, kBareosVersionStrings.Full,
                     version.Major, version.Minor, version.Patch);
}
