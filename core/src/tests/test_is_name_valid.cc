/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2019-2019 Bareos GmbH & Co. KG

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation, which is
   listed in the file LICENSE.

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
#define _S_IFDIR S_IFDIR
#define _stat stat
#include "minwindef.h"
#include "include/bareos.h"
#include "gtest/gtest.h"
#else
#include "gtest/gtest.h"
#include "include/bareos.h"
#endif

#include "lib/edit.h"

POOLMEM* msg = GetPoolMemory(PM_FNAME);

TEST(is_name_valid, is_name_valid)
{
  EXPECT_EQ(true, IsNameValid("STRING_CONTAINING_ALLOWED_CHARS :.-_/", msg));
  EXPECT_EQ(true, IsNameValid("A name_has_space_at_second_place"));
  EXPECT_EQ(true, IsNameValid("name_has_space_as_second_last Z"));

  std::string string_maximum_length(MAX_NAME_LENGTH - 1, '.');
  EXPECT_EQ(true, IsNameValid(string_maximum_length.c_str(), msg));

  EXPECT_EQ(false, IsNameValid("illegalch@racter", msg));
  EXPECT_STREQ("Illegal character \"@\" in name.\n", msg);

  EXPECT_EQ(false, IsNameValid("", msg));
  EXPECT_STREQ("Name must be at least one character long.\n", msg);

  EXPECT_EQ(false, IsNameValid(nullptr, msg));
  EXPECT_STREQ("Empty name not allowed.\n", msg);

  EXPECT_EQ(false, IsNameValid(" name_starts_with_space", msg));
  EXPECT_STREQ("Name cannot start with space.\n", msg);

  EXPECT_EQ(false, IsNameValid("name_ends_with_space ", msg));
  EXPECT_STREQ("Name cannot end with space.\n", msg);

  EXPECT_EQ(false, IsNameValid(" ", msg));
  EXPECT_STREQ("Name cannot start with space.\n", msg);

  std::string string_too_long(MAX_NAME_LENGTH, '.');
  EXPECT_EQ(false, IsNameValid(string_too_long.c_str(), msg));
  EXPECT_STREQ("Name too long.\n", msg);
}
