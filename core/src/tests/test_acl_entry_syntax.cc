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
#endif

#include "include/bareos.h"
#include "lib/edit.h"

POOLMEM* msg = GetPoolMemory(PM_FNAME);

TEST(acl_entry_syntax_test, acl_entry_syntax_test)
{
  EXPECT_EQ(true, IsAclEntryValid("list", msg));

  EXPECT_EQ(false, IsAclEntryValid("list,add", msg));
  EXPECT_STREQ("Illegal character \",\" in acl.\n", msg);

  EXPECT_EQ(true, IsAclEntryValid("STRING.CONTAINING.ALLOWED.CHARS!*.", msg));

  std::string string_maximum_length(MAX_NAME_LENGTH - 1, '.');
  EXPECT_EQ(true, IsAclEntryValid(string_maximum_length.c_str(), msg));

  EXPECT_EQ(false, IsAclEntryValid("illegalch@racter", msg));
  EXPECT_STREQ("Illegal character \"@\" in acl.\n", msg);

  EXPECT_EQ(false, IsAclEntryValid("", msg));
  EXPECT_STREQ("Acl must be at least one character long.\n", msg);

  EXPECT_EQ(false, IsAclEntryValid(nullptr, msg));
  EXPECT_STREQ("Empty acl not allowed.\n", msg);

  std::string string_too_long(MAX_NAME_LENGTH, '.');
  EXPECT_EQ(false, IsAclEntryValid(string_too_long.c_str(), msg));

  EXPECT_STREQ("Acl too long.\n", msg);
}
