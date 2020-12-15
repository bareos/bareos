/**
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
#if defined(HAVE_MINGW)
#  include "include/bareos.h"
#  include "gtest/gtest.h"
#else
#  include "gtest/gtest.h"
#  include "include/bareos.h"
#endif

#include "lib/bool_string.h"

#include <iostream>
#include <map>
#include <string>

TEST(BoolString, convert_supported_bool_strings)
{
  const std::map<std::string, bool> test_pattern{
      {"on", true},   {"true", true},   {"yes", true}, {"1", true},
      {"off", false}, {"false", false}, {"no", false}, {"0", false}};

  for (const auto& t : test_pattern) {
    bool value = false;
    try {
      BoolString s{t.first};
      value = s.get<bool>();
    } catch (const std::out_of_range& e) {
      // abort the test
      ASSERT_TRUE(false) << "Failed to create BoolString: " << e.what();
    }
    EXPECT_EQ(value, t.second) << "test pattern: '" << t.first << "'";
  }
}

TEST(BoolString, try_to_convert_not_supported_bool_strings)
{
  const std::set<std::string> test_pattern{
      {"o"}, {"tru"}, {"ye"}, {"12"}, {"offf"}, {"ffalse"}, {"0-no"}};

  for (const auto& t : test_pattern) {
    bool can_be_converted = true;
    try {
      BoolString s{t};
    } catch (const std::out_of_range& e) {
      can_be_converted = false;
    }
    EXPECT_FALSE(can_be_converted) << "test pattern: '" << t << "'";
  }
}
