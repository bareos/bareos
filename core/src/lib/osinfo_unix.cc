/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2020-2025 Bareos GmbH & Co. KG

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

#include <fstream>
#include <string>
#include <optional>

#include "include/config.h"
#include "osinfo.h"
#include "util.h"

#include <cctype>

#include <sys/utsname.h>
namespace {
constexpr std::string_view unquote(std::string_view value)
{
  if (value.size() == 0) { return value; }

  // if its not quoted, then we have nothing to do
  if (value.front() != '\'' && value.front() != '"') { return value; }

  auto quoted_char = value.front();

  auto current = 1;

  for (;;) {
    auto pos = value.find_first_of(quoted_char, current);
    if (pos == value.npos) {
      // no matching quote was found
      break;
    }

    // we now know that value[pos] == quoted_char,
    // but we cannot stop here since this value might be escaped!

    size_t num_backslashes = 0;
    for (size_t i = 0; i < pos; ++i) {
      if (value[pos - 1 - i] == '\\') {
        num_backslashes += 1;
      } else {
        // we only want to count the number of consecutive backslashes
        // starting at pos-1, so we can quit now
        break;
      }
    }

    // if we have an _even_ amount of backslashes, then the backslashes all
    // cancel each other out (each pair corresponds to one unescaped literal
    // backslash); we found the end of the quoted string
    // if we have an _odd_ amount of backslashes, then the quote is escaped
    // and we need to continue on
    if ((num_backslashes & 1) == 0) { return value.substr(1, pos - 1); }

    current = pos + 1;
  }

  // if we did not find matching quotes, then something is wrong,
  // but since we are not the shell police, we just treat it as unquoted
  // and continue
  return value;
}

static_assert(unquote(R"(abcd)") == R"(abcd)");
static_assert(unquote(R"('abcd)") == R"('abcd)");
static_assert(unquote(R"('abcd')") == R"(abcd)");
static_assert(unquote(R"("abcd")") == R"(abcd)");
static_assert(unquote(R"("ab\"cd")") == R"(ab\"cd)");
static_assert(unquote(R"('ab\'cd')") == R"(ab\'cd)");

std::string parse_value(std::string_view value)
{
  // the only thing we have to do here is to remove escaping backslashes

  std::string result;
  result.reserve(value.size());

  for (size_t i = 0; i < value.size(); ++i) {
    if (value[i] == '\\' && i < value.size() - 1) {
      // only shell builtins are allowed to be escaped
      // and those are always one byte long, so we do not need to
      // utf-8 decode/encode here
      result.push_back(value[i + 1]);
      i += 1;
    } else {
      result.push_back(value[i]);
    }
  }

  return result;
}

std::string_view trim_left(std::string_view in)
{
  for (size_t i = 0; i < in.size(); ++i) {
    if (!std::isspace(in[i])) {
      in.remove_prefix(i);
      return in;
    }
  }

  return {};
}
}  // namespace

struct os_release {
  // see man 5 os-release

  void parse_file(std::ifstream& input)
  {
    std::string line;
    std::optional<std::string> name = std::nullopt;
    std::optional<std::string> version = std::nullopt;
    std::optional<std::string> pretty_name = std::nullopt;
    while (std::getline(input, line)) {
      // we expect the lines to be of the form key=value
      // and ignore all non matching lines

      auto trimmed = trim_left(line);

      if (trimmed.empty()) {
        // blank lines are ignored
        continue;
      }

      if (trimmed.front() == '#') {
        // comments get ignored
        continue;
      }

      if (std::optional split = split_first(line, '=')) {
        auto [key, value] = *split;

        std::optional<std::string>* place{};

        if (key == "NAME") {
          place = &name;
        } else if (key == "VERSION") {
          place = &name;
        } else if (key == "PRETTY_NAME") {
          place = &pretty_name;
        } else {
          // we do not care about you ...
          continue;
        }

        // the value may be either given directly or be enclosed in
        // quotes, i.e.
        //    A=B
        // or
        //    A="B" / A='B'
        auto unquoted = unquote(value);

        auto real_value = parse_value(unquoted);

        // we dont want empty names
        if (real_value.size() == 0) { continue; }

        place->emplace(real_value);
      }
    }

    if (pretty_name) {
      os_version = std::move(pretty_name);
    } else if (name && version) {
      os_version.emplace(*name + " " + *version);
    }
  }


  os_release()
  {
    if (auto loc1 = std::ifstream("/etc/os-release")) {
      parse_file(loc1);
    } else if (auto loc2 = std::ifstream("/usr/lib/os-release")) {
      parse_file(loc2);
    } else if (auto loc3 = std::ifstream("/etc/initrd-release")) {
      parse_file(loc3);
    } else {
      utsname name;
      if (uname(&name) == 0) {
        // example:
        // Linux 6.8.0-1036-aws (#38~22.04.1-Ubuntu SMP Fri Aug 22 15:44:33 UTC
        // 2025)
        std::string pretty_name;
        pretty_name += name.sysname;
        pretty_name += " ";
        pretty_name += name.release;
        pretty_name += " (";
        pretty_name += name.version;
        pretty_name += ")";
        os_version.emplace(std::move(pretty_name));
      }
    }
  }

  std::optional<std::string> os_version;
};

const char* GetOsInfoString()
{
  // output priority
  // 1) pretty_name
  // 2) name + version (both have to be there)
  // 3) uname
  // 4) DISTVER

  static os_release info{};

  if (info.os_version) { return info.os_version->c_str(); }

  return "built on " DISTVER;
}
