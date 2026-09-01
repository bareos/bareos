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
#include "include/version_numbers.h"
#include "lib/util.h"
#include <fmt/format.h>
#include "include/version_hex.h"

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

std::optional<ParsedHello> parse_hello(global_resource::Type my_type,
                                       std::string_view hello_msg)
{
  std::string cpy{hello_msg};

  unsigned major{}, minor{}, patch{};
  char name[128] = {};
  int fd_protocol_version = {};
  Type type{}, from = Type::Unknown;
  bool old_console{};
  char version[128] = {};

  switch (my_type) {
    case global_resource::Type::Client: {
      if (sscanf(cpy.c_str(),
                 "Hello Storage calling Start Job %127s Version=\"%u.%u.%u\"",
                 name, &major, &minor, &patch)
              == 4
          || sscanf(cpy.c_str(), "Hello Storage calling Start Job %127s", name)
                 == 1) {
        from = Type::Storage;
        type = Type::Job;
      } else if (sscanf(cpy.c_str(),
                        "Hello Director %127s calling Version=\"%u.%u.%u\"",
                        name, &major, &minor, &patch)
                     == 4
                 || sscanf(cpy.c_str(), "Hello Director %127s calling", name)
                        == 1) {
        from = Type::Director;
        type = Type::Director;
      }
    } break;

    case global_resource::Type::Director: {
      if (sscanf(cpy.c_str(),
                 "Hello Client %127s FdProtocolVersion=%d calling "
                 "Version=\"%u.%u.%u\"",
                 name, &fd_protocol_version, &major, &minor, &patch)
              == 5
          || sscanf(cpy.c_str(),
                    "Hello Client %127s FdProtocolVersion=%d calling", name,
                    &fd_protocol_version)
                 == 2
          || sscanf(cpy.c_str(), "Hello Client %127s calling", name) == 1) {
        from = Type::Client;
        type = Type::Client;
      } else if (sscanf(
                     cpy.c_str(),
                     "Hello %127s calling version %127s Version=\"%u.%u.%u\"",
                     name, version, &major, &minor, &patch)
                     == 4
                 || sscanf(cpy.c_str(), "Hello %127s calling version %127s",
                           name, version)
                        == 4
                 || sscanf(cpy.c_str(), "Hello %127s calling", name) == 1) {
        from = Type::Console;
        type = Type::Console;

        if (version[0] != 0) {
          auto parsed_version = parse_version(version);
          old_console = parsed_version < BareosVersionNumber::kRelease_18_2;
        } else {
          old_console = true;
        }
      }
    } break;

    case global_resource::Type::Storage: {
      if (sscanf(cpy.c_str(), "Hello Start Job %127s Version=\"%u.%u.%u\"",
                 name, &major, &minor, &patch)
              == 4
          || sscanf(cpy.c_str(), "Hello Start Job %127s", name) == 1) {
        from = Type::Client;
        type = Type::Job;
      } else if (sscanf(cpy.c_str(),
                        "Hello Start Storage Job %127s Version=\"%u.%u.%u\"",
                        name, &major, &minor, &patch)
                     == 4
                 || sscanf(cpy.c_str(), "Hello Start Storage Job %127s", name)
                        == 1) {
        from = Type::Storage;
        type = Type::Job;
      } else if (sscanf(cpy.c_str(),
                        "Hello Director %127s calling Version=\"%u.%u.%u\"",
                        name, &major, &minor, &patch)
                     == 4
                 || sscanf(cpy.c_str(), "Hello Director %127s calling", name)
                        == 1) {
        type = Type::Director;
        from = Type::Director;
      }
    } break;

    default: {
    } break;
  }

  if (from == Type::Unknown) { return std::nullopt; }

  return ParsedHello{
      .from = from,
      .type = type,
      .name = name,
      .fd_protocol_version = fd_protocol_version,
      .old_console = old_console,
      .bareos_version = VERSION_HEX(major, minor, patch),
  };
}
