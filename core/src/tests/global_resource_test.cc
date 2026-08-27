/**
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2018-2026 Bareos GmbH & Co. KG

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
#include "include/bareos.h"

#define private public
#include "lib/global_resource.h"
#undef private

TEST(GlobalResource, GetNameFromType)
{
  using namespace global_resource;
  for (auto [type, name] : type_names) {
    EXPECT_EQ(GetNameFromType(type), name);
  }
}

TEST(GlobalResource, GetTypeFromName)
{
  using namespace global_resource;
  for (auto [type, name] : type_names) {
    EXPECT_EQ(GetTypeFromName(name), type);
  }
}

TEST(GlobalResource, QualifiedNameWorks)
{
  using namespace global_resource;
  for (auto [type, _] : type_names) {
    // QualifiedName returns an empty string on error
    EXPECT_NE(QualifiedName(type, ""), std::string{});
  }
}

TEST(GlobalResource, QualifiedNameRoundtrip)
{
  std::string_view resource_name = "abcd";
  using namespace global_resource;
  for (auto [type, _] : type_names) {
    auto qualified_name = QualifiedName(type, resource_name);
    ASSERT_NE(qualified_name, std::string{});

    auto [parsed_type, parsed_name] = ParseQualifiedName(qualified_name);
    EXPECT_EQ(parsed_type, type);
    EXPECT_EQ(parsed_name, resource_name);
  }
}
