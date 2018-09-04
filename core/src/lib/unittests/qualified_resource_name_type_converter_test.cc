/**
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2018-2018 Bareos GmbH & Co. KG

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


#include "gtest/gtest.h"
#define private public
#include "lib/qualified_resource_name_type_converter.h"
#undef private
#include "include/bareos.h"

enum
{
  kOne                = 1,
  kTwo                = 2,
  kThree              = 3,
  kNotInsertedIntoMap = 4
};

static std::map<int, std::string> create_test_map()
{
  std::map<int, std::string> map;
  map.insert(std::make_pair(kOne, "kOne"));
  map.insert(std::make_pair(kTwo, "kTwo"));
  map.insert(std::make_pair(kThree, "kThree"));
  return map;
}

TEST(QualifiedResourceNameTypeConverter, ResourceTypeToString)
{
  QualifiedResourceNameTypeConverter c(create_test_map());

  EXPECT_STREQ(c.ResourceTypeToString(kOne).c_str(), "kOne");
  EXPECT_STREQ(c.ResourceTypeToString(kTwo).c_str(), "kTwo");
  EXPECT_STREQ(c.ResourceTypeToString(kThree).c_str(), "kThree");
  EXPECT_STREQ(c.ResourceTypeToString(kNotInsertedIntoMap).c_str(), "");

  EXPECT_EQ(c.StringToResourceType("kOne"), kOne);
  EXPECT_EQ(c.StringToResourceType("kTwo"), kTwo);
  EXPECT_EQ(c.StringToResourceType("kThree"), kThree);
  EXPECT_EQ(c.StringToResourceType("kNotInsertedIntoMap"), -1);

  std::string result_str;
  bool ok = c.ResourceToString("ResourceName", kTwo, result_str);
  EXPECT_EQ(ok, true);
  EXPECT_STREQ(result_str.c_str(), "kTwo:ResourceName");

  ok = c.ResourceToString("ResourceName", kNotInsertedIntoMap, result_str);
  EXPECT_EQ(ok, false);

  std::string name;
  int r_type;
  ok = c.StringToResource(name, r_type, "kOne:Developer");
  EXPECT_EQ(ok, true);
  EXPECT_EQ(r_type, kOne);
  EXPECT_STREQ(name.c_str(), "Developer");

  ok = c.StringToResource(name, r_type, "kOneDeveloper");
  EXPECT_EQ(ok, false);
}
