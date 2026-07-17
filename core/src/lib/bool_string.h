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

#include <set>
#include <stdexcept>
#include <string>

class BoolString {
 public:
  explicit BoolString(const std::string& s) : str_value(s)
  {
    if (true_values.find(str_value) == true_values.end()
        && false_values.find(str_value) == false_values.end()) {
      std::string err = {"Wrong parameter: "};
      throw std::out_of_range(err + str_value);
    }
  }
  template <typename T = std::string> T get() const { return str_value; }

 private:
  std::string str_value;
  const std::set<std::string> true_values{"true", "yes", "1"};
  const std::set<std::string> false_values{"false", "no", "0"};
};

template <> inline bool BoolString::get<bool>() const
{
  return true_values.find(str_value) != true_values.end();
}

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

namespace bool_parsing::internal {
template <size_t N>
inline bool is_in_vec(const std::string_view (&candidates)[N],
                      std::string_view value)
{
  for (auto candidate : candidates) {
    if (value.size() == candidate.size()
        && strncasecmp(value.data(), candidate.data(), value.size()) == 0) {
      return true;
    }
  }
  return false;
}

inline std::pair<parse_bool_result, bool> parse_bool(std::string_view input,
                                                     bool allow_deprecated
                                                     = false)
{
  static constexpr std::string_view true_accepted[] = {"yes"};
  static constexpr std::string_view true_deprecated[] = {"true", "1"};

  static constexpr std::string_view false_accepted[] = {"no"};
  static constexpr std::string_view false_deprecated[] = {"false", "0"};

  bool deprecated = false;

  if (is_in_vec(true_accepted, input)) {
    return {parse_bool_result::True, deprecated};
  }
  if (is_in_vec(false_accepted, input)) {
    return {parse_bool_result::False, deprecated};
  }

  deprecated = true;
  if (is_in_vec(true_deprecated, input)) {
    if (allow_deprecated) {
      return {parse_bool_result::True, deprecated};
    } else {
      return {parse_bool_result::Error, deprecated};
    }
  }
  if (is_in_vec(false_deprecated, input)) {
    if (allow_deprecated) {
      return {parse_bool_result::False, deprecated};
    } else {
      return {parse_bool_result::Error, deprecated};
    }
  }

  return {parse_bool_result::Error, false};
}
}  // namespace bool_parsing::internal

static inline parse_bool_result parse_user_bool(std::string_view input)
{
  using namespace bool_parsing::internal;

  auto [result, deprecated] = parse_bool(input);

  if (deprecated) {
    // we do not accept deprecated options in user contexts

    Dmsg0(100, "User passed deprecated option %.*s\n", (int)input.size(),
          input.data());

    return parse_bool_result::Error;
  }
  return result;
}

#endif  // BAREOS_LIB_BOOL_STRING_H_
