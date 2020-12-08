/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2020-2020 Bareos GmbH & Co. KG

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
  BoolString(const std::string& s) : str_value(s)
  {
    if (true_values.find(str_value) == true_values.end()
        && false_values.find(str_value) == false_values.end()) {
      std::string err = {"Wrong parameter: "};
      throw std::out_of_range(err + str_value);
    }
  }
  template <typename T = std::string>
  T get() const
  {
    return str_value;
  }

 private:
  std::string str_value;
  static const std::set<std::string> true_values;
  static const std::set<std::string> false_values;
};

template <>
inline bool BoolString::get<bool>() const
{
  return true_values.find(str_value) != true_values.end();
}

#endif  // BAREOS_LIB_BOOL_STRING_H_
