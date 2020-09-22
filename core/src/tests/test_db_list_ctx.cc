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

#if defined(HAVE_MINGW)
#include "include/bareos.h"
#include "gtest/gtest.h"
#else
#include "gtest/gtest.h"
#include "include/bareos.h"
#endif

#include <iostream>
#include "cats/cats.h"

TEST(db_list_ctx, ConstructorsTest)
{
  db_list_ctx list1;
  EXPECT_TRUE(list1.empty());

  list1.add("Test123");
  EXPECT_EQ(0, list1.at(0).compare(std::string("Test123")));

  BStringList list2(list1);
  EXPECT_EQ(1, list2.size());
  EXPECT_EQ(0, list2.front().compare(std::string("Test123")));
}

TEST(db_list_ctx, AddJobId)
{
  db_list_ctx list1;
  EXPECT_TRUE(list1.empty());

  JobId_t jobid = 3;

  list1.add("Test1");
  list1.add("Test2");
  list1.add(jobid);

  EXPECT_EQ(3, list1.size());
  EXPECT_EQ(0, list1.at(2).compare(std::to_string(jobid)));
}

TEST(db_list_ctx, AddList)
{
  db_list_ctx list1;
  EXPECT_TRUE(list1.empty());

  JobId_t jobid1 = 13;

  list1.add("Test11");
  list1.add("Test12");
  list1.add(jobid1);

  EXPECT_EQ(3, list1.size());

  db_list_ctx list2;

  JobId_t jobid2 = 23;

  list2.add("Test21");
  list2.add("Test22");
  list2.add(jobid2);

  list1.add(list2);

  EXPECT_EQ(3, list2.size());

  EXPECT_EQ(6, list1.size());
  EXPECT_EQ(0, list1.at(1).compare(std::string("Test12")));
  EXPECT_EQ(0, list1.at(4).compare(std::string("Test22")));
}

TEST(db_list_ctx, list)
{
  db_list_ctx list1;

  list1.add(11);
  list1.add(12);
  list1.add(13);

  EXPECT_EQ(3, list1.size());
  EXPECT_EQ(0, list1.GetAsString().compare(std::string("11,12,13")));
  // std::cerr << "list: " << list1.list() << "\n";
  EXPECT_TRUE(bstrcmp(list1.GetAsString().c_str(), "11,12,13"));
  EXPECT_STREQ(list1.GetAsString().c_str(), "11,12,13");
}

TEST(db_list_ctx, GetFrontAsInteger)
{
  db_list_ctx nb;

  EXPECT_EQ(0, nb.GetFrontAsInteger());

  nb.add(11);
  EXPECT_EQ(11, nb.GetFrontAsInteger());

  nb.add(12);
  EXPECT_EQ(11, nb.GetFrontAsInteger());
}
