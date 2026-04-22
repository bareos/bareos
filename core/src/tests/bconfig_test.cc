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

bool HasMatchedExternalRelation(
    const bconfig::ResourceInspectionEntry& resource,
    const char* relation,
    const char* component,
    const char* target_type,
    const char* target_name)
{
  return std::any_of(
      resource.external_relations.begin(), resource.external_relations.end(),
      [relation, component, target_type, target_name](const auto& rel) {
        return rel.relation == relation && rel.target_component == component
               && rel.target_type == target_type
               && rel.target_name == target_name && rel.matched;
      });
}

const bconfig::NestedDetailEntry* FindNestedDetail(
    const bconfig::ResourceInspectionEntry& resource,
    const char* kind,
    const char* summary)
{
  auto it = std::find_if(
      resource.nested_details.begin(), resource.nested_details.end(),
      [kind, summary](const auto& detail) {
        return detail.kind == kind && detail.summary.has_value()
               && *detail.summary == summary;
      });
  return it == resource.nested_details.end() ? nullptr : &*it;
}

const bconfig::DetailValueEntry* FindDetailValue(
    const bconfig::NestedDetailEntry& detail,
    const char* name)
{
  auto it
      = std::find_if(detail.values.begin(), detail.values.end(),
                     [name](const auto& value) { return value.name == name; });
  return it == detail.values.end() ? nullptr : &*it;
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

TEST(Bconfig, CollectsRunscriptNestedDetails)
{
  auto config_path = WriteTempConfig("bconfig-runscript-details.conf",
                                     R"(Director {
  Name = bareos-dir
  Password = "dir_password"
  Messages = Standard
}

Messages {
  Name = Standard
  Console = all
}

Client {
  Name = bareos-fd
  Address = localhost
  Password = "client_password"
}

FileSet {
  Name = TestFileset
  Include {
    File = /tmp
  }
}

Schedule {
  Name = Nightly
  Run = Full at 01:00
}

Pool {
  Name = Full
  Pool Type = Backup
}

Storage {
  Name = MainStorage
  Address = localhost
  Password = "storage_password"
  Device = FileStorage
  Media Type = File
}

JobDefs {
  Name = DefaultJob
  Type = Backup
  Level = Full
  Client = bareos-fd
  FileSet = TestFileset
  Schedule = Nightly
  Storage = MainStorage
  Pool = Full
  Messages = Standard
  RunScript {
    Command = "/bin/true"
    Target = bareos-fd
    RunsWhen = before
    FailJobOnError = no
  }
}

Job {
  Name = BackupJob
  JobDefs = DefaultJob
  ClientRunBeforeJob = "/bin/echo job-client"
  RunScript {
    Console = "status dir"
    RunsWhen = after
    RunsOnFailure = yes
  }
}
)");

  auto loaded = bconfig::LoadConfig(bconfig::Component::kDirector,
                                    config_path.string());

  ASSERT_TRUE(loaded.parser);
  ASSERT_TRUE(loaded.parse_ok);

  auto resources = bconfig::CollectResources(*loaded.parser);
  auto jobdefs = std::find_if(
      resources.begin(), resources.end(), [](const auto& resource) {
        return resource.type == "JobDefs" && resource.name == "DefaultJob";
      });
  ASSERT_NE(jobdefs, resources.end());

  auto* jobdefs_script = FindNestedDetail(*jobdefs, "RunScript", "/bin/true");
  ASSERT_NE(jobdefs_script, nullptr);
  ASSERT_TRUE(jobdefs_script->source.has_value());
  EXPECT_EQ(jobdefs_script->source->line, 53);

  auto* command = FindDetailValue(*jobdefs_script, "Command");
  ASSERT_NE(command, nullptr);
  EXPECT_EQ(command->value, "/bin/true");
  ASSERT_TRUE(command->source.has_value());
  EXPECT_EQ(command->source->line, 54);

  auto* target = FindDetailValue(*jobdefs_script, "Target");
  ASSERT_NE(target, nullptr);
  EXPECT_EQ(target->value, "bareos-fd");
  ASSERT_TRUE(target->source.has_value());
  EXPECT_EQ(target->source->line, 55);

  auto* fail_on_error = FindDetailValue(*jobdefs_script, "FailJobOnError");
  ASSERT_NE(fail_on_error, nullptr);
  EXPECT_EQ(fail_on_error->value, "no");
  ASSERT_TRUE(fail_on_error->source.has_value());
  EXPECT_EQ(fail_on_error->source->line, 57);

  auto job = std::find_if(
      resources.begin(), resources.end(), [](const auto& resource) {
        return resource.type == "Job" && resource.name == "BackupJob";
      });
  ASSERT_NE(job, resources.end());

  auto* short_script
      = FindNestedDetail(*job, "RunScript", "/bin/echo job-client");
  ASSERT_NE(short_script, nullptr);
  ASSERT_TRUE(short_script->source.has_value());
  EXPECT_EQ(short_script->source->line, 64);

  auto* short_target = FindDetailValue(*short_script, "Target");
  ASSERT_NE(short_target, nullptr);
  EXPECT_EQ(short_target->value, "job-client");
  ASSERT_TRUE(short_target->source.has_value());
  EXPECT_EQ(short_target->source->line, 64);

  auto* short_form = FindDetailValue(*short_script, "Form");
  ASSERT_NE(short_form, nullptr);
  EXPECT_EQ(short_form->value, "short");
  EXPECT_FALSE(short_form->source.has_value());

  auto* console_script = FindNestedDetail(*job, "RunScript", "status dir");
  ASSERT_NE(console_script, nullptr);
  auto* command_type = FindDetailValue(*console_script, "CommandType");
  ASSERT_NE(command_type, nullptr);
  EXPECT_EQ(command_type->value, "Console");
  ASSERT_TRUE(command_type->source.has_value());
  EXPECT_EQ(command_type->source->line, 66);
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

TEST(Bconfig, CollectsDirectorStorageAutochangerPeerRelations)
{
  auto director_path = WriteTempConfig("bconfig-director-peer-autochanger.conf",
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
  Name = TapeStorage
  Address = localhost
  Password = "storage-password"
  Device = ll2-lto7
  Media Type = LTO
}
)");
  auto storage_path = WriteTempConfig("bconfig-storage-peer-autochanger.conf",
                                      R"(Director {
  Name = bareos-dir
  Password = "storage-password"
}

Storage {
  Name = bareos-sd
}

Device {
  Name = ll2-drive-1
  Media Type = LTO
  Archive Device = /tmp/bconfig-storage-autochanger-1
  Device Type = File
}

Autochanger {
  Name = ll2-lto7
  Device = ll2-drive-1
  Changer Device = /dev/null
  Changer Command = "/bin/true"
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
        return resource.type == "Storage" && resource.name == "TapeStorage";
      });
  ASSERT_NE(entry, resources.end());

  EXPECT_TRUE(std::any_of(entry->external_relations.begin(),
                          entry->external_relations.end(), [](const auto& rel) {
                            return rel.relation == "Device"
                                   && rel.target_component == "storage"
                                   && rel.target_type == "Autochanger"
                                   && rel.target_name == "ll2-lto7"
                                   && rel.matched;
                          }));
}

TEST(Bconfig, CollectsClientPrimaryPeerRelations)
{
  auto client_path = WriteTempConfig("bconfig-client-primary-peer.conf",
                                     R"(Director {
  Name = bareos-dir
  Password = "client-password"
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
  auto director_path
      = WriteTempConfig("bconfig-director-for-client-primary.conf",
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

  auto client
      = bconfig::LoadConfig(bconfig::Component::kClient, client_path.string());
  auto director = bconfig::LoadConfig(bconfig::Component::kDirector,
                                      director_path.string());

  ASSERT_TRUE(client.parser);
  ASSERT_TRUE(director.parser);
  ASSERT_TRUE(client.parse_ok);
  ASSERT_TRUE(director.parse_ok);

  auto resources = bconfig::CollectResources(client, {&director});

  auto client_entry = std::find_if(
      resources.begin(), resources.end(), [](const auto& resource) {
        return resource.type == "Client" && resource.name == "bareos-fd";
      });
  ASSERT_NE(client_entry, resources.end());
  EXPECT_TRUE(HasMatchedExternalRelation(*client_entry, "Peer.Client",
                                         "director", "Client", "bareos-fd"));

  auto director_entry = std::find_if(
      resources.begin(), resources.end(), [](const auto& resource) {
        return resource.type == "Director" && resource.name == "bareos-dir";
      });
  ASSERT_NE(director_entry, resources.end());
  EXPECT_TRUE(HasMatchedExternalRelation(*director_entry, "Peer.Director",
                                         "director", "Director", "bareos-dir"));
  EXPECT_TRUE(HasMatchedExternalRelation(*director_entry, "Peer.ClientAuth",
                                         "director", "Client", "bareos-fd"));
}

TEST(Bconfig, CollectsStoragePrimaryPeerRelations)
{
  auto storage_path = WriteTempConfig("bconfig-storage-primary-peer.conf",
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
  Archive Device = /tmp/bconfig-storage-primary-device
  Device Type = File
}

Device {
  Name = ll2-drive-1
  Media Type = LTO
  Archive Device = /tmp/bconfig-storage-primary-drive
  Device Type = File
}

Autochanger {
  Name = ll2-lto7
  Device = ll2-drive-1
  Changer Device = /dev/null
  Changer Command = "/bin/true"
}
)");
  auto director_path
      = WriteTempConfig("bconfig-director-for-storage-primary.conf",
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
  Name = FileStorageRef
  Address = localhost
  Password = "storage-password"
  Device = FileStorage
  Media Type = File
}

Storage {
  Name = AutochangerRef
  Address = localhost
  Password = "storage-password"
  Device = ll2-lto7
  Media Type = LTO
}
)");

  auto storage = bconfig::LoadConfig(bconfig::Component::kStorage,
                                     storage_path.string());
  auto director = bconfig::LoadConfig(bconfig::Component::kDirector,
                                      director_path.string());

  ASSERT_TRUE(storage.parser);
  ASSERT_TRUE(director.parser);
  ASSERT_TRUE(storage.parse_ok);
  ASSERT_TRUE(director.parse_ok);

  auto resources = bconfig::CollectResources(storage, {&director});

  auto director_entry = std::find_if(
      resources.begin(), resources.end(), [](const auto& resource) {
        return resource.type == "Director" && resource.name == "bareos-dir";
      });
  ASSERT_NE(director_entry, resources.end());
  EXPECT_TRUE(HasMatchedExternalRelation(*director_entry, "Peer.Director",
                                         "director", "Director", "bareos-dir"));
  EXPECT_TRUE(HasMatchedExternalRelation(*director_entry, "Peer.StorageAuth",
                                         "director", "Storage",
                                         "FileStorageRef"));
  EXPECT_TRUE(HasMatchedExternalRelation(*director_entry, "Peer.StorageAuth",
                                         "director", "Storage",
                                         "AutochangerRef"));

  auto device_entry = std::find_if(
      resources.begin(), resources.end(), [](const auto& resource) {
        return resource.type == "Device" && resource.name == "FileStorage";
      });
  ASSERT_NE(device_entry, resources.end());
  EXPECT_TRUE(HasMatchedExternalRelation(
      *device_entry, "Peer.Storage", "director", "Storage", "FileStorageRef"));

  auto autochanger_entry = std::find_if(
      resources.begin(), resources.end(), [](const auto& resource) {
        return resource.type == "Autochanger" && resource.name == "ll2-lto7";
      });
  ASSERT_NE(autochanger_entry, resources.end());
  EXPECT_TRUE(HasMatchedExternalRelation(*autochanger_entry, "Peer.Storage",
                                         "director", "Storage",
                                         "AutochangerRef"));
}

#ifdef BCONFIG_HAVE_CONSOLE
TEST(Bconfig, CollectsConsolePrimaryPeerRelations)
{
  auto console_path = WriteTempConfig("bconfig-console-primary-peer.conf",
                                      R"(Director {
  Name = bareos-dir
  Address = localhost
  Password = "director-password"
}

Console {
  Name = admin
  Password = "console-password"
  Director = bareos-dir
}
)");
  auto director_path
      = WriteTempConfig("bconfig-director-for-console-primary.conf",
                        R"(Director {
  Name = bareos-dir
  Password = "director-password"
  Messages = Standard
}

Messages {
  Name = Standard
  Console = all
}

Console {
  Name = admin
  Password = "console-password"
  CommandACL = status
}
)");

  auto console = bconfig::LoadConfig(bconfig::Component::kConsole,
                                     console_path.string());
  auto director = bconfig::LoadConfig(bconfig::Component::kDirector,
                                      director_path.string());

  ASSERT_TRUE(console.parser);
  ASSERT_TRUE(director.parser);
  ASSERT_TRUE(console.parse_ok);
  ASSERT_TRUE(director.parse_ok);

  auto resources = bconfig::CollectResources(console, {&director});

  auto director_entry = std::find_if(
      resources.begin(), resources.end(), [](const auto& resource) {
        return resource.type == "Director" && resource.name == "bareos-dir";
      });
  ASSERT_NE(director_entry, resources.end());
  EXPECT_TRUE(HasMatchedExternalRelation(*director_entry, "Peer.DefaultConsole",
                                         "director", "Director", "bareos-dir"));

  auto console_entry = std::find_if(
      resources.begin(), resources.end(), [](const auto& resource) {
        return resource.type == "Console" && resource.name == "admin";
      });
  ASSERT_NE(console_entry, resources.end());
  EXPECT_TRUE(HasMatchedExternalRelation(*console_entry, "Peer.Director",
                                         "director", "Director", "bareos-dir"));
  EXPECT_TRUE(HasMatchedExternalRelation(*console_entry, "Peer.Console",
                                         "director", "Console", "admin"));
}
#endif

#ifdef BCONFIG_HAVE_TRAYMONITOR
TEST(Bconfig, CollectsTrayMonitorPrimaryPeerRelations)
{
  auto tray_path = WriteTempConfig("bconfig-traymonitor-primary-peer.conf",
                                   R"(Monitor {
  Name = tray-monitor
  Password = "console-password"
}

Director {
  Name = bareos-dir
  Address = localhost
}

Client {
  Name = bareos-fd
  Address = localhost
  Password = "client-password"
}

Storage {
  Name = bareos-sd
  Address = localhost
  Password = "storage-password"
}
)");
  auto director_path = WriteTempConfig("bconfig-director-for-traymonitor.conf",
                                       R"(Director {
  Name = bareos-dir
  Password = "director-password"
  Messages = Standard
}

Messages {
  Name = Standard
  Console = all
}

Console {
  Name = tray-monitor
  Password = "console-password"
  CommandACL = status
}
)");
  auto client_path = WriteTempConfig("bconfig-client-for-traymonitor.conf",
                                     R"(Director {
  Name = tray-monitor
  Password = "client-password"
}

Client {
  Name = bareos-fd
}
)");
  auto storage_path = WriteTempConfig("bconfig-storage-for-traymonitor.conf",
                                      R"(Director {
  Name = tray-monitor
  Password = "storage-password"
}

Storage {
  Name = bareos-sd
}
)");

  auto tray = bconfig::LoadConfig(bconfig::Component::kTrayMonitor,
                                  tray_path.string());
  auto director = bconfig::LoadConfig(bconfig::Component::kDirector,
                                      director_path.string());
  auto client
      = bconfig::LoadConfig(bconfig::Component::kClient, client_path.string());
  auto storage = bconfig::LoadConfig(bconfig::Component::kStorage,
                                     storage_path.string());

  ASSERT_TRUE(tray.parser);
  ASSERT_TRUE(director.parser);
  ASSERT_TRUE(client.parser);
  ASSERT_TRUE(storage.parser);
  ASSERT_TRUE(tray.parse_ok);
  ASSERT_TRUE(director.parse_ok);
  ASSERT_TRUE(client.parse_ok);
  ASSERT_TRUE(storage.parse_ok);

  auto resources
      = bconfig::CollectResources(tray, {&director, &client, &storage});

  auto director_entry = std::find_if(
      resources.begin(), resources.end(), [](const auto& resource) {
        return resource.type == "Director" && resource.name == "bareos-dir";
      });
  ASSERT_NE(director_entry, resources.end());
  EXPECT_TRUE(HasMatchedExternalRelation(*director_entry, "Peer.Director",
                                         "director", "Director", "bareos-dir"));

  auto monitor_entry = std::find_if(
      resources.begin(), resources.end(), [](const auto& resource) {
        return resource.type == "Monitor" && resource.name == "tray-monitor";
      });
  ASSERT_NE(monitor_entry, resources.end());
  EXPECT_TRUE(HasMatchedExternalRelation(
      *monitor_entry, "Peer.Console", "director", "Console", "tray-monitor"));

  auto client_entry = std::find_if(
      resources.begin(), resources.end(), [](const auto& resource) {
        return resource.type == "Client" && resource.name == "bareos-fd";
      });
  ASSERT_NE(client_entry, resources.end());
  EXPECT_TRUE(HasMatchedExternalRelation(*client_entry, "Peer.DirectorAuth",
                                         "client", "Director", "tray-monitor"));

  auto storage_entry = std::find_if(
      resources.begin(), resources.end(), [](const auto& resource) {
        return resource.type == "Storage" && resource.name == "bareos-sd";
      });
  ASSERT_NE(storage_entry, resources.end());
  EXPECT_TRUE(HasMatchedExternalRelation(*storage_entry, "Peer.DirectorAuth",
                                         "storage", "Director",
                                         "tray-monitor"));
}
#endif

}  // namespace
