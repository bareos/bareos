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

#include "gtest/gtest.h"
#include "include/bareos.h"
#include "lib/edit.h"

POOLMEM* msg = GetPoolMemory(PM_FNAME);

TEST(acl_entry_syntax_test, acl_entry_syntax_test)
{
  EXPECT_EQ(true, IsAclEntryValid("list", msg));

  EXPECT_EQ(false, IsAclEntryValid("list,add", msg));
  EXPECT_STREQ("Illegal character \",\" in acl.\n", msg);

  EXPECT_EQ(true, IsAclEntryValid("STRING.CONTAINING.ALLOWED.CHARS!*.", msg));
  EXPECT_EQ(
      true,
      IsAclEntryValid(
          "STRING.WITH.MAX.ALLOWED.LENGTH......................................"
          "...........................................................",
          msg));
  EXPECT_EQ(false, IsAclEntryValid("illegalch@racter", msg));
  EXPECT_STREQ("Illegal character \"@\" in acl.\n", msg);

  EXPECT_EQ(false, IsAclEntryValid("", msg));
  EXPECT_STREQ("Acl must be at least one character long.\n", msg);

  EXPECT_EQ(false, IsAclEntryValid(nullptr, msg));
  EXPECT_STREQ("Empty acl not allowed.\n", msg);


  EXPECT_EQ(
      false,
      IsAclEntryValid(
          "STRING.WITH.MAX.ALLOWED.LENGTH.PLUS.ONE............................."
          "............................................................",
          msg));
  EXPECT_STREQ("Acl too long.\n", msg);
}
