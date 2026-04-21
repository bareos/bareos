/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2026-2026 Bareos GmbH & Co. KG

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

#include "tools/bconfig_lib.h"

#include <algorithm>

#include "gtest/gtest.h"

namespace {

TEST(Bconfig, CollectsDirectorResourceSources)
{
  auto loaded = bconfig::LoadConfig(bconfig::Component::kDirector,
                                    "configs/bareos-configparser-tests");

  ASSERT_TRUE(loaded.parser);
  ASSERT_TRUE(loaded.parse_ok);

  auto resources = bconfig::CollectResources(*loaded.parser);
  auto it = std::find_if(resources.begin(), resources.end(),
                         [](const auto& resource) {
                           return resource.type == "Director"
                                  && resource.name == "bareos-dir";
                         });
  ASSERT_NE(it, resources.end());
  ASSERT_TRUE(it->source.has_value());
  EXPECT_EQ(it->source->line, 1);
  EXPECT_TRUE(it->source->file.ends_with(
      "configs/bareos-configparser-tests/bareos-dir.d/director/"
      "bareos-dir.conf"));

  auto directive = std::find_if(it->directives.begin(), it->directives.end(),
                                [](const auto& entry) {
                                  return entry.name == "Messages";
                                });
  ASSERT_NE(directive, it->directives.end());
  ASSERT_TRUE(directive->source.has_value());
  EXPECT_EQ(directive->source->line, 6);
}

TEST(Bconfig, CollectsResolvedInternalRelations)
{
  auto loaded = bconfig::LoadConfig(bconfig::Component::kDirector,
                                    "configs/bareos-configparser-tests");

  ASSERT_TRUE(loaded.parser);
  ASSERT_TRUE(loaded.parse_ok);

  auto resources = bconfig::CollectResources(*loaded.parser);
  auto it = std::find_if(resources.begin(), resources.end(),
                         [](const auto& resource) {
                           return resource.type == "Job"
                                  && resource.name == "backup-bareos-fd";
                         });
  ASSERT_NE(it, resources.end());

  auto has_relation = [&it](const char* directive,
                            const char* target_type,
                            const char* target_name) {
    return std::any_of(it->relations.begin(), it->relations.end(),
                       [directive, target_type, target_name](const auto& rel) {
                         return rel.directive == directive
                                && rel.target_type == target_type
                                && rel.target_name == target_name;
                       });
  };

  EXPECT_TRUE(has_relation("JobDefs", "JobDefs", "DefaultJob"));
  EXPECT_TRUE(has_relation("Client", "Client", "bareos-fd"));
}

TEST(Bconfig, CollectsSchemaMetadata)
{
  auto loaded
      = bconfig::LoadConfig(bconfig::Component::kDirector, "", false);

  ASSERT_TRUE(loaded.parser);

  auto schema = bconfig::CollectSchema(*loaded.parser);
  auto director = std::find_if(schema.begin(), schema.end(), [](const auto& r) {
    return r.name == "Director";
  });
  ASSERT_NE(director, schema.end());

  auto password = std::find_if(director->directives.begin(),
                               director->directives.end(), [](const auto& d) {
                                 return d.name == "Password";
                               });
  ASSERT_NE(password, director->directives.end());
  EXPECT_TRUE(password->required);
  EXPECT_EQ(password->datatype, "AUTOPASSWORD");
}

}  // namespace
