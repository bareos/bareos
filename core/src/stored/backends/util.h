/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2024-2025 Bareos GmbH & Co. KG

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

#ifndef BAREOS_STORED_BACKENDS_UTIL_H_
#define BAREOS_STORED_BACKENDS_UTIL_H_

#include <string_view>
#include <string>
#include <map>
#include <variant>
#include <fmt/format.h>
#include <lib/source_location.h>

namespace backends::util {

int key_compare(std::string_view l, std::string_view r);

struct key_comparator {
  bool operator()(std::string_view l, std::string_view r) const
  {
    return key_compare(l, r) == -1;
  }
};

using options = std::map<std::string, std::string, key_comparator>;
using error = std::string;

std::variant<options, error> parse_options(std::string_view v);


/* wrap an integer with its source location for logging */
struct LevelAndLocation {
  int level;
  libbareos::source_location loc;

  constexpr LevelAndLocation(int t_level,
                             const libbareos::source_location& t_loc
                             = libbareos::source_location::current())
      : level(t_level), loc(t_loc)
  {
  }
};

/* emit a fmt-formatted debug message */
template <typename Fmt, typename... Args>
void Dfmt(LevelAndLocation lal, Fmt fmt, Args&&... args)
{
  const auto formatted
      = fmt::format(std::forward<Fmt>(fmt), std::forward<Args>(args)...);
  d_msg(lal.loc.file_name(), lal.loc.line(), lal.level, "%s\n",
        formatted.c_str());
}

};  // namespace backends::util

#endif  // BAREOS_STORED_BACKENDS_UTIL_H_
