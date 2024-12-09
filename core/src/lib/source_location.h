/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2024-2024 Bareos GmbH & Co. KG

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

#ifndef BAREOS_LIB_SOURCE_LOCATION_H_
#define BAREOS_LIB_SOURCE_LOCATION_H_

#include "include/config.h"

#if defined(HAVE_SOURCE_LOCATION)
#  include <source_location>
namespace libbareos {
using source_location = std::source_location;
};
#elif defined(HAVE_BUILTIN_LOCATION)
#  include <cstdint>
namespace libbareos {
class source_location {
public:
  constexpr static source_location current(const char* function
                                           = __builtin_FUNCTION(),
                                           const char* file = __builtin_FILE(),
                                           std::uint_least32_t line
                                           = __builtin_LINE()) noexcept
  {
    return source_location{ function, file, line };
  }

  constexpr const char* file_name() const noexcept { return file_; }
  constexpr const char* function_name() const noexcept { return function_; }
  constexpr std::uint_least32_t line() const noexcept { return line_; }
  constexpr std::uint_least32_t column() const noexcept { return 0; }

  constexpr source_location() = default;

private:
  constexpr source_location(const char* function,
                  const char* file,
                  std::uint_least32_t line)
    : function_{function}
    , file_{file}
    , line_{line}
  {}

  const char* function_{"unset"};
  const char* file_{"unset"};
  std::uint_least32_t line_{0};
};
}  // namespace brs
#else
#error "No source_location implementation available"
#endif

#include <string_view>
// make sure this works at all
static_assert(std::string_view{libbareos::source_location::current().file_name()}
              == std::string_view{__FILE__});
static_assert(libbareos::source_location::current().line() == __LINE__);

namespace {
  // make sure this works as intended, when used as a defaulted parameter
  // This was bugged on e.g. clang 15
  constexpr auto make_source_loc(libbareos::source_location loc = libbareos::source_location::current()) {
    return loc;
  }

static_assert(make_source_loc().line() == __LINE__);
  static_assert(std::string_view{make_source_loc().file_name()}
                == std::string_view{__FILE__});
};  // namespace

#endif  // BAREOS_LIB_SOURCE_LOCATION_H_
