/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2020-2026 Bareos GmbH & Co. KG

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

#ifndef BAREOS_LIB_BOOL_STRING_H_
#define BAREOS_LIB_BOOL_STRING_H_

#include <lib/bsys.h>
#include <string_view>
#include <span>

/**
 * Parses a user provided string for a bool.
 *
 * This only works for ascii strings.
 */

enum class parse_bool_result
{
  True,
  False,
  Error,
};

static inline parse_bool_result parse_user_bool(std::string_view input)
{
  static constexpr std::string_view true_accepted[] = {"yes", "true"};

  static constexpr std::string_view false_accepted[] = {"no", "false"};

  using enum parse_bool_result;

  auto is_in_vec = [](std::span<const std::string_view> candidates,
                      std::string_view value) -> bool {
    for (auto candidate : candidates) {
      if (value.size() == candidate.size()
          && bstrncasecmp(value.data(), candidate.data(), value.size())) {
        return true;
      }
    }
    return false;
  };

  if (is_in_vec(true_accepted, input)) { return True; }

  if (is_in_vec(false_accepted, input)) { return False; }

  return Error;
}

#endif  // BAREOS_LIB_BOOL_STRING_H_
