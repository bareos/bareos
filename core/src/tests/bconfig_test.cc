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
#include <filesystem>
#include <fstream>

#include "gtest/gtest.h"

namespace {

std::filesystem::path WriteTempConfig(const char* filename,
                                      const std::string& content)
{
  auto path = std::filesystem::temp_directory_path() / filename;
  std::ofstream stream(path);
  stream << content;
  stream.close();
  return path;
}

TEST(Bconfig, CollectsDirectorResourceSources)
{
  auto loaded = bconfig::LoadConfig(bconfig::Component::kDirector,
                                    "configs/bareos-configparser-tests");

  ASSERT_TRUE(loaded.parser);
  ASSERT_TRUE(loaded.parse_ok);

  auto resources = bconfig::CollectResources(*loaded.parser);
  auto it = std::find_if(
      resources.begin(), resources.end(), [](const auto& resource) {
        return resource.type == "Director" && resource.name == "bareos-dir";
      });
  ASSERT_NE(it, resources.end());
  ASSERT_TRUE(it->source.has_value());
  EXPECT_EQ(it->source->line, 1);
  EXPECT_TRUE(it->source->file.ends_with(
      "configs/bareos-configparser-tests/bareos-dir.d/director/"
      "bareos-dir.conf"));

  auto directive = std::find_if(
      it->directives.begin(), it->directives.end(),
      [](const auto& entry) { return entry.name == "Messages"; });
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
  auto it = std::find_if(
      resources.begin(), resources.end(), [](const auto& resource) {
        return resource.type == "Job" && resource.name == "backup-bareos-fd";
      });
  ASSERT_NE(it, resources.end());

  auto has_relation = [&it](const char* directive, const char* target_type,
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
  auto loaded = bconfig::LoadConfig(bconfig::Component::kDirector, "", false);

  ASSERT_TRUE(loaded.parser);

  auto schema = bconfig::CollectSchema(*loaded.parser);
  auto director = std::find_if(schema.begin(), schema.end(), [](const auto& r) {
    return r.name == "Director";
  });
  ASSERT_NE(director, schema.end());

  auto password
      = std::find_if(director->directives.begin(), director->directives.end(),
                     [](const auto& d) { return d.name == "Password"; });
  ASSERT_NE(password, director->directives.end());
  EXPECT_TRUE(password->required);
  EXPECT_EQ(password->datatype, "AUTOPASSWORD");
}

TEST(Bconfig, CollectsClientInternalRelations)
{
  auto config_path = WriteTempConfig("bconfig-client-relations.conf",
                                     R"(Director {
  Name = bareos-dir
  Password = "dir_password"
}

Client {
  Name = bareos-fd
  Messages = Standard
}

Messages {
  Name = Standard
  Director = bareos-dir = all
}
)");

  auto loaded
      = bconfig::LoadConfig(bconfig::Component::kClient, config_path.string());

  ASSERT_TRUE(loaded.parser);
  ASSERT_TRUE(loaded.parse_ok);

  auto resources = bconfig::CollectResources(*loaded.parser);
  auto it = std::find_if(
      resources.begin(), resources.end(), [](const auto& resource) {
        return resource.type == "Client" && resource.name == "bareos-fd";
      });
  ASSERT_NE(it, resources.end());
  ASSERT_EQ(it->relations.size(), 1U);
  EXPECT_EQ(it->relations[0].directive, "Messages");
  EXPECT_EQ(it->relations[0].target_type, "Messages");
  EXPECT_EQ(it->relations[0].target_name, "Standard");
}

TEST(Bconfig, CollectsStorageInternalRelations)
{
  auto config_path = WriteTempConfig("bconfig-storage-relations.conf",
                                     R"(Director {
  Name = bareos-dir
  Password = "dir_password"
}

Storage {
  Name = bareos-sd
  Messages = Standard
}

Messages {
  Name = Standard
  Director = bareos-dir = all
}

Device {
  Name = FileStorage
  Media Type = File
  Archive Device = /tmp/bconfig-storage-relations
  Device Type = File
}

Autochanger {
  Name = Autochanger1
  Device = FileStorage
  Changer Device = /dev/null
  Changer Command = "/bin/true"
}
)");

  auto loaded
      = bconfig::LoadConfig(bconfig::Component::kStorage, config_path.string());

  ASSERT_TRUE(loaded.parser);
  ASSERT_TRUE(loaded.parse_ok);

  auto resources = bconfig::CollectResources(*loaded.parser);
  auto storage = std::find_if(
      resources.begin(), resources.end(), [](const auto& resource) {
        return resource.type == "Storage" && resource.name == "bareos-sd";
      });
  ASSERT_NE(storage, resources.end());
  ASSERT_EQ(storage->relations.size(), 1U);
  EXPECT_EQ(storage->relations[0].directive, "Messages");
  EXPECT_EQ(storage->relations[0].target_type, "Messages");
  EXPECT_EQ(storage->relations[0].target_name, "Standard");

  auto autochanger = std::find_if(resources.begin(), resources.end(),
                                  [](const auto& resource) {
                                    return resource.type == "Autochanger"
                                           && resource.name == "Autochanger1";
                                  });
  ASSERT_NE(autochanger, resources.end());
  ASSERT_EQ(autochanger->relations.size(), 1U);
  EXPECT_EQ(autochanger->relations[0].directive, "Device");
  EXPECT_EQ(autochanger->relations[0].target_type, "Device");
  EXPECT_EQ(autochanger->relations[0].target_name, "FileStorage");
}

TEST(Bconfig, SurfacesBrokenReferences)
{
  auto config_path = WriteTempConfig("bconfig-broken-reference.conf",
                                     R"(Director {
  Name = broken-dir
  Password = "dir_password"
  Messages = MissingMessages
}
)");

  auto loaded = bconfig::LoadConfig(bconfig::Component::kDirector,
                                    config_path.string());

  ASSERT_TRUE(loaded.parser);
  ASSERT_FALSE(loaded.parse_ok);
  EXPECT_FALSE(loaded.messages.errors.empty());
  EXPECT_TRUE(std::any_of(loaded.messages.errors.begin(),
                          loaded.messages.errors.end(), [](const auto& error) {
                            return error.find("Could not find config resource")
                                   != std::string::npos;
                          }));
}

TEST(Bconfig, DetectsScheduleRunOverrideRelations)
{
  auto config_path = WriteTempConfig("bconfig-schedule-run-overrides.conf",
                                     R"(Schedule {
  Name = Nightly
  Run = pool=Full messages=Standard storage=MainStorage at 01:00
  Run = differentialpool=Diff nextpool=Next messages=AltMessages at 02:00
}

Pool {
  Name = Full
  Pool Type = Backup
}

Pool {
  Name = Diff
  Pool Type = Backup
}

Pool {
  Name = Next
  Pool Type = Backup
}

Messages {
  Name = Standard
  Console = all
}

Messages {
  Name = AltMessages
  Console = all
}

Storage {
  Name = MainStorage
  Address = localhost
  Password = "secret"
  Device = FileStorage
  Media Type = File
}
)");

  auto loaded = bconfig::LoadConfig(bconfig::Component::kDirector,
                                    config_path.string());

  ASSERT_TRUE(loaded.parser);
  ASSERT_TRUE(loaded.parse_ok);

  auto resources = bconfig::CollectResources(*loaded.parser);
  auto schedule = std::find_if(
      resources.begin(), resources.end(), [](const auto& resource) {
        return resource.type == "Schedule" && resource.name == "Nightly";
      });
  ASSERT_NE(schedule, resources.end());

  auto has_relation = [&schedule](const char* directive,
                                  const char* target_type,
                                  const char* target_name, int source_line) {
    return std::any_of(
        schedule->relations.begin(), schedule->relations.end(),
        [directive, target_type, target_name, source_line](const auto& rel) {
          return rel.directive == directive && rel.target_type == target_type
                 && rel.target_name == target_name && rel.source.has_value()
                 && rel.source->line == source_line;
        });
  };

  EXPECT_TRUE(has_relation("Run.Pool", "Pool", "Full", 3));
  EXPECT_TRUE(has_relation("Run.Messages", "Messages", "Standard", 3));
  EXPECT_TRUE(has_relation("Run.Storage", "Storage", "MainStorage", 3));
  EXPECT_TRUE(has_relation("Run.DifferentialPool", "Pool", "Diff", 4));
  EXPECT_TRUE(has_relation("Run.NextPool", "Pool", "Next", 4));
  EXPECT_TRUE(has_relation("Run.Messages", "Messages", "AltMessages", 4));
}

TEST(Bconfig, SkipsHostnameValidationWhenInspecting)
{
  auto config_path = WriteTempConfig("bconfig-unresolved-hostname.conf",
                                     R"(Director {
  Name = bareos-dir
  Password = "dir_password"
  Address = bareos-dir
  Messages = Standard
}

Messages {
  Name = Standard
  Console = all
}
)");

  auto loaded = bconfig::LoadConfig(bconfig::Component::kDirector,
                                    config_path.string());

  ASSERT_TRUE(loaded.parser);
  EXPECT_TRUE(loaded.parse_ok);
  EXPECT_TRUE(loaded.messages.errors.empty());

  auto resources = bconfig::CollectResources(*loaded.parser);
  auto director = std::find_if(
      resources.begin(), resources.end(), [](const auto& resource) {
        return resource.type == "Director" && resource.name == "bareos-dir";
      });
  ASSERT_NE(director, resources.end());
  EXPECT_TRUE(std::any_of(
      director->directives.begin(), director->directives.end(),
      [](const auto& directive) { return directive.name == "Address"; }));
}

TEST(Bconfig, CollectsDirectorClientPeerRelations)
{
  auto director_path = WriteTempConfig("bconfig-director-peer-client.conf",
                                       R"(Director {
  Name = bareos-dir
  Password = "director-password"
  Messages = Standard
}

Messages {
  Name = Standard
  Console = all
}

Client {
  Name = bareos-fd
  Address = localhost
  Password = "client-password"
}
)");
  auto client_path = WriteTempConfig("bconfig-client-peer.conf",
                                     R"(Director {
  Name = bareos-dir
  Password = "client-password"
}

Client {
  Name = bareos-fd
}
)");

  auto director = bconfig::LoadConfig(bconfig::Component::kDirector,
                                      director_path.string());
  auto client
      = bconfig::LoadConfig(bconfig::Component::kClient, client_path.string());

  ASSERT_TRUE(director.parser);
  ASSERT_TRUE(client.parser);
  ASSERT_TRUE(director.parse_ok);
  ASSERT_TRUE(client.parse_ok);

  auto resources = bconfig::CollectResources(director, {&client});
  auto entry = std::find_if(
      resources.begin(), resources.end(), [](const auto& resource) {
        return resource.type == "Client" && resource.name == "bareos-fd";
      });
  ASSERT_NE(entry, resources.end());

  auto has_external = [&entry](const char* relation, const char* component,
                               const char* target_type,
                               const char* target_name) {
    return std::any_of(
        entry->external_relations.begin(), entry->external_relations.end(),
        [relation, component, target_type, target_name](const auto& rel) {
          return rel.relation == relation && rel.target_component == component
                 && rel.target_type == target_type
                 && rel.target_name == target_name && rel.matched;
        });
  };

  EXPECT_TRUE(has_external("Peer.Client", "client", "Client", "bareos-fd"));
  EXPECT_TRUE(
      has_external("Peer.DirectorAuth", "client", "Director", "bareos-dir"));
}

TEST(Bconfig, CollectsDirectorStoragePeerRelations)
{
  auto director_path = WriteTempConfig("bconfig-director-peer-storage.conf",
                                       R"(Director {
  Name = bareos-dir
  Password = "director-password"
  Messages = Standard
}

Messages {
  Name = Standard
  Console = all
}

Storage {
  Name = MainStorage
  Address = localhost
  Password = "storage-password"
  Device = FileStorage
  Media Type = File
}
)");
  auto storage_path = WriteTempConfig("bconfig-storage-peer.conf",
                                      R"(Director {
  Name = bareos-dir
  Password = "storage-password"
}

Storage {
  Name = bareos-sd
}

Device {
  Name = FileStorage
  Media Type = File
  Archive Device = /tmp/bconfig-storage-peer
  Device Type = File
}
)");

  auto director = bconfig::LoadConfig(bconfig::Component::kDirector,
                                      director_path.string());
  auto storage = bconfig::LoadConfig(bconfig::Component::kStorage,
                                     storage_path.string());

  ASSERT_TRUE(director.parser);
  ASSERT_TRUE(storage.parser);
  ASSERT_TRUE(director.parse_ok);
  ASSERT_TRUE(storage.parse_ok);

  auto resources = bconfig::CollectResources(director, {&storage});
  auto entry = std::find_if(
      resources.begin(), resources.end(), [](const auto& resource) {
        return resource.type == "Storage" && resource.name == "MainStorage";
      });
  ASSERT_NE(entry, resources.end());

  auto has_external = [&entry](const char* relation, const char* component,
                               const char* target_type,
                               const char* target_name) {
    return std::any_of(
        entry->external_relations.begin(), entry->external_relations.end(),
        [relation, component, target_type, target_name](const auto& rel) {
          return rel.relation == relation && rel.target_component == component
                 && rel.target_type == target_type
                 && rel.target_name == target_name && rel.matched;
        });
  };

  EXPECT_TRUE(
      has_external("Peer.DirectorAuth", "storage", "Director", "bareos-dir"));
  EXPECT_TRUE(has_external("Device", "storage", "Device", "FileStorage"));
}

}  // namespace
