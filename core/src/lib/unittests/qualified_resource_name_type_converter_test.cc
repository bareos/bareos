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

static constexpr char record_separator_ = 0x1e;

enum
{
  kOne                = 1,
  kTwo                = 2,
  kThree              = 3,
  kNotInsertedIntoMap = 4
};

static const std::map<int, std::string> create_test_map()
{
  const std::map<int, std::string> map{{kOne, "kOne"}, {kTwo, "kTwo"}, {kThree, "kThree"}};
  return map;
}

TEST(QualifiedResourceNameTypeConverter, StringToType)
{
  bool ok;
  int job_id;
  int r_type;
  std::string name;
  std::string result_str;

  QualifiedResourceNameTypeConverter c(create_test_map());

  EXPECT_EQ(c.StringToResourceType("kOne"), kOne);
  EXPECT_EQ(c.StringToResourceType("kTwo"), kTwo);
  EXPECT_EQ(c.StringToResourceType("kThree"), kThree);
  EXPECT_EQ(c.StringToResourceType("kNotInsertedIntoMap"), -1);

  job_id = -1; /* job_id should be unchanged */
  std::string tmp;
  tmp = std::string("kOne") + record_separator_ + std::string("Developer");
  ok  = c.StringToResource(name, r_type, job_id, tmp);
  EXPECT_EQ(ok, true);
  EXPECT_EQ(job_id, -1);

  job_id            = -1; /* job_id will be changed in this test */
  std::string test1 = std::string("kOne") + record_separator_ + std::string("Developer") + record_separator_ +
                      std::string("123");
  ok = c.StringToResource(name, r_type, job_id, test1.c_str());
  EXPECT_EQ(ok, true);
  EXPECT_EQ(r_type, kOne);
  EXPECT_EQ(job_id, 123);
  EXPECT_STREQ(name.c_str(), "Developer");

  /* try invalid string */
  ok = c.StringToResource(name, r_type, job_id, "foobar");
  EXPECT_EQ(ok, false);

  /* try invalid job_id (not a number) */
  job_id = -2; /* job_id should be unchanged in this test */
  std::string test2 = std::string("kOne") + record_separator_ + std::string("Developer") + record_separator_ +
                      std::string("foo");
  ok     = c.StringToResource(name, r_type, job_id, test2.c_str());
  EXPECT_EQ(ok, false);
  EXPECT_EQ(job_id, -2);
}

TEST(QualifiedResourceNameTypeConverter, TypeToString)
{
  bool ok;
  int job_id = 0;
  std::string name;
  std::string result_str;

  QualifiedResourceNameTypeConverter c(create_test_map());

  EXPECT_STREQ(c.ResourceTypeToString(kOne).c_str(), "kOne");
  EXPECT_STREQ(c.ResourceTypeToString(kTwo).c_str(), "kTwo");
  EXPECT_STREQ(c.ResourceTypeToString(kThree).c_str(), "kThree");
  EXPECT_STREQ(c.ResourceTypeToString(kNotInsertedIntoMap).c_str(), "");

  /* resource without job_id */
  ok = c.ResourceToString("ResourceName", kTwo, result_str);
  EXPECT_EQ(ok, true);
  std::string test1 = std::string("kTwo") + record_separator_ + std::string("ResourceName");
  EXPECT_STREQ(result_str.c_str(), test1.c_str());

  /* resource with job_id */
  job_id = 456;
  ok     = c.ResourceToString("ResourceName2", kThree, job_id, result_str);
  EXPECT_EQ(ok, true);
  std::string test2 = std::string("kThree") + record_separator_ + std::string("ResourceName2") + record_separator_ +
                      std::string("456");
  EXPECT_STREQ(result_str.c_str(), test2.c_str());

  /* try invalid resource type */
  ok = c.ResourceToString("ResourceName", kNotInsertedIntoMap, result_str);
  EXPECT_EQ(ok, false);
}
