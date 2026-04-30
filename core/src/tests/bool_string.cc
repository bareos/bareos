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

using namespace bool_parsing::internal;


static inline std::pair<parse_bool_result, bool> deprecated(parse_bool_result x)
{
  return {x, true};
}

static inline std::pair<parse_bool_result, bool> allowed(parse_bool_result x)
{
  return {x, false};
}

TEST(ParseBool, simple_yes)
{
  EXPECT_EQ(parse_bool("yes", true), allowed(parse_bool_result::True));
  EXPECT_EQ(parse_bool("true", true), deprecated(parse_bool_result::True));
  EXPECT_EQ(parse_bool("1", true), deprecated(parse_bool_result::True));
}

TEST(ParseBool, deprecation_true)
{
  EXPECT_EQ(parse_bool("yes"), allowed(parse_bool_result::True));
  EXPECT_EQ(parse_bool("true"), deprecated(parse_bool_result::Error));
  EXPECT_EQ(parse_bool("1"), deprecated(parse_bool_result::Error));
}

TEST(ParseBool, simple_no)
{
  EXPECT_EQ(parse_bool("no", true), allowed(parse_bool_result::False));
  EXPECT_EQ(parse_bool("false", true), deprecated(parse_bool_result::False));
  EXPECT_EQ(parse_bool("0", true), deprecated(parse_bool_result::False));
}

TEST(ParseBool, deprecation_false)
{
  EXPECT_EQ(parse_bool("no"), allowed(parse_bool_result::False));
  EXPECT_EQ(parse_bool("false"), deprecated(parse_bool_result::Error));
  EXPECT_EQ(parse_bool("0"), deprecated(parse_bool_result::Error));
}

TEST(ParseBool, random_capitalisation)
{
  EXPECT_EQ(parse_bool("YeS", true), allowed(parse_bool_result::True));
  EXPECT_EQ(parse_bool("nO", true), allowed(parse_bool_result::False));

  EXPECT_EQ(parse_bool("TrUe", true), deprecated(parse_bool_result::True));
  EXPECT_EQ(parse_bool("fAlSe", true), deprecated(parse_bool_result::False));
}

TEST(ParseBool, bad_numbers)
{
  EXPECT_EQ(parse_bool("2"), allowed(parse_bool_result::Error));
  EXPECT_EQ(parse_bool("01"), allowed(parse_bool_result::Error));
  EXPECT_EQ(parse_bool("10"), allowed(parse_bool_result::Error));
}

TEST(ParseBool, whitespace)
{
  // parse_bool does _not_ handle trimming
  EXPECT_EQ(parse_bool(" yes"), allowed(parse_bool_result::Error));
  EXPECT_EQ(parse_bool("yes "), allowed(parse_bool_result::Error));
  EXPECT_EQ(parse_bool("false "), allowed(parse_bool_result::Error));
  EXPECT_EQ(parse_bool(" false"), allowed(parse_bool_result::Error));

  EXPECT_EQ(parse_bool(""), allowed(parse_bool_result::Error));
  EXPECT_EQ(parse_bool(" "), allowed(parse_bool_result::Error));
}
