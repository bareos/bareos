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
#include "include/bareos.h"
#include "gtest/gtest.h"
#else
#include "gtest/gtest.h"
#include "include/bareos.h"
#endif

#include "lib/util.h"

TEST(sort_stringvector, sort_ascending)
{
  std::vector<std::string> v{"Zfs", "EXT2", "ext3", "ext4", "xfs", "AFS"};
  SortCaseInsensitive(v);

  EXPECT_STREQ(v[0].c_str(), "AFS");
  EXPECT_STREQ(v[1].c_str(), "EXT2");
  EXPECT_STREQ(v[2].c_str(), "ext3");
  EXPECT_STREQ(v[3].c_str(), "ext4");
  EXPECT_STREQ(v[4].c_str(), "xfs");
  EXPECT_STREQ(v[5].c_str(), "Zfs");
}
