/**
   BAREOS® - Backup Archiving REcovery Open Sourced

Copyright (C) 2023-2023 Bareos GmbH & Co. KG

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

#include "lib/breg.h"

static const char regexp_sep = '!';

TEST(RegularExpressions, BuildRegexWhere)
{
  PoolMem strip_prefix("a");
  std::string dest = BuildRegexWhere(strip_prefix.addr(), nullptr, nullptr);
  EXPECT_STREQ(dest.c_str(), "!a!!i");

  PoolMem add_prefix("b");
  dest = BuildRegexWhere(strip_prefix.addr(), add_prefix.addr(), nullptr);
  EXPECT_STREQ(dest.c_str(), "!a!!i,!^!b!");

  PoolMem add_suffix("c");
  dest = BuildRegexWhere(strip_prefix.addr(), add_prefix.addr(),
                         add_suffix.addr());
  EXPECT_STREQ(dest.c_str(), "!a!!i,!([^/])$!$1c!,!^!b!");
}

TEST(RegularExpressions, bregexp_escape_string)
{
  PoolMem dest;

  PoolMem strip_prefix("a");
  bregexp_escape_string(dest.addr(), strip_prefix.addr(), regexp_sep);
  EXPECT_STREQ(dest.c_str(), "a");

  strip_prefix.strcpy("a\\");
  bregexp_escape_string(dest.addr(), strip_prefix.addr(), regexp_sep);
  EXPECT_STREQ(dest.c_str(), "a\\\\");
}
