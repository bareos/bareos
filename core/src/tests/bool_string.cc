/**
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
#if defined(HAVE_MINGW)
#  include "include/bareos.h"
#  include "gtest/gtest.h"
#else
#  include "gtest/gtest.h"
#  include "include/bareos.h"
#endif

#include "lib/bool_string.h"

TEST(ParseBool, simple_yes)
{
  EXPECT_EQ(parse_user_bool("yes"), parse_bool_result::True);
  EXPECT_EQ(parse_user_bool("true"), parse_bool_result::True);
  EXPECT_EQ(parse_user_bool("1"), parse_bool_result::True);
}

TEST(ParseBool, simple_no)
{
  EXPECT_EQ(parse_user_bool("no"), parse_bool_result::False);
  EXPECT_EQ(parse_user_bool("false"), parse_bool_result::False);
  EXPECT_EQ(parse_user_bool("0"), parse_bool_result::False);
}

TEST(ParseBool, random_capitalisation)
{
  EXPECT_EQ(parse_user_bool("YeS"), parse_bool_result::True);
  EXPECT_EQ(parse_user_bool("fAlSe"), parse_bool_result::False);
}

TEST(ParseBool, bad_numbers)
{
  EXPECT_EQ(parse_user_bool("2"), parse_bool_result::Error);
  EXPECT_EQ(parse_user_bool("01"), parse_bool_result::Error);
  EXPECT_EQ(parse_user_bool("10"), parse_bool_result::Error);
}

TEST(ParseBool, whitespace)
{
  // parse_user_bool does _not_ handle trimming
  EXPECT_EQ(parse_user_bool(" yes"), parse_bool_result::Error);
  EXPECT_EQ(parse_user_bool("yes "), parse_bool_result::Error);
  EXPECT_EQ(parse_user_bool("false "), parse_bool_result::Error);
  EXPECT_EQ(parse_user_bool(" false"), parse_bool_result::Error);

  EXPECT_EQ(parse_user_bool(""), parse_bool_result::Error);
  EXPECT_EQ(parse_user_bool(" "), parse_bool_result::Error);
}
