/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2026 Bareos GmbH & Co. KG

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
#include "config_model.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>

#include <gtest/gtest.h>
#include <unistd.h>

namespace {
class TempConfigRoot {
 public:
  TempConfigRoot()
  {
    auto pattern = std::filesystem::temp_directory_path()
                   / "bareos-config-model-XXXXXX";
    auto mutable_path = pattern.string();
    path_ = mkdtemp(mutable_path.data());
    if (path_.empty()) throw std::runtime_error("mkdtemp failed");
  }

  ~TempConfigRoot() { std::filesystem::remove_all(path_); }

  const std::filesystem::path& path() const { return path_; }

 private:
  std::filesystem::path path_;
};
}  // namespace

TEST(BareosConfigModel, DiscoversDirectorAndDaemonResources)
{
  TempConfigRoot root;

  std::filesystem::create_directories(root.path() / "bareos-dir.d/client");
  std::filesystem::create_directories(root.path() / "bareos-dir.d/director");
  std::filesystem::create_directories(root.path() / "bareos-dir.d/storage");
  std::filesystem::create_directories(root.path() / "bareos-fd.d/client");
  std::filesystem::create_directories(root.path() / "bareos-fd.d/director");
  std::filesystem::create_directories(root.path() / "bareos-sd.d/device");
  std::filesystem::create_directories(root.path() / "bareos-sd.d/director");
  std::filesystem::create_directories(root.path() / "bareos-sd.d/storage");
  std::filesystem::create_directories(root.path() / "bareos-fd.d/fileset");
  std::filesystem::create_directories(root.path() / "bconsole.d/console");

  std::ofstream(root.path() / "bareos-dir.conf") << "# director\n";
  std::ofstream(root.path() / "bareos-sd.conf")
      << "Storage {\n  Name = example-sd\n}\n";
  std::ofstream(root.path() / "bareos-fd.conf")
      << "FileDaemon {\n  Name = example-fd\n}\n";
  std::ofstream(root.path() / "bconsole.conf")
      << "Director {\n  Name = bareos-dir\n}\n"
      << "Console {\n  Name = admin\n}\n";
  std::ofstream(root.path() / "bareos-dir.d/director/bareos-dir.conf")
      << "Director {\n  Name = bareos-dir\n}\n";
  std::ofstream(root.path() / "bareos-dir.d/client/example-fd.conf")
      << "Client {\n  Name = example-fd\n}\n";
  std::ofstream(root.path() / "bareos-fd.d/client/example-fd.conf")
      << "Client {\n  Name = example-fd\n}\n";
  std::ofstream(root.path() / "bareos-fd.d/director/bareos-dir.conf")
      << "Director {\n  Name = bareos-dir\n}\n";
  std::ofstream(root.path() / "bareos-dir.d/storage/example-sd.conf")
      << "Storage {\n  Name = example-sd\n}\n";
  std::ofstream(root.path() / "bareos-sd.d/director/bareos-dir.conf")
      << "Director {\n  Name = bareos-dir\n}\n";
  std::ofstream(root.path() / "bareos-sd.d/storage/example-sd.conf")
      << "Storage {\n  Name = example-sd\n}\n";
  std::ofstream(root.path() / "bareos-sd.d/device/tape.conf") << "Device {}\n";
  std::ofstream(root.path() / "bareos-fd.d/fileset/local.conf") << "FileSet {}\n";
  std::ofstream(root.path() / "bconsole.d/console/admin.conf")
      << "Console {\n  Name = admin\n}\n";

  const auto summary = DiscoverDatacenterSummary({root.path()});

  ASSERT_EQ(summary.directors.size(), 1U);
  EXPECT_EQ(summary.directors[0].name, "bareos-dir");
  ASSERT_GE(summary.directors[0].resources.size(), 2U);
  EXPECT_EQ(summary.directors[0].daemons.size(), 3U);
  EXPECT_EQ(summary.directors[0].daemons[0].kind, "storage-daemon");
  EXPECT_EQ(summary.directors[0].daemons[1].kind, "file-daemon");
  EXPECT_EQ(summary.directors[0].daemons[2].kind, "console");
  EXPECT_EQ(summary.directors[0].daemons[0].configured_name, "example-sd");
  EXPECT_EQ(summary.directors[0].daemons[1].configured_name, "example-fd");
  EXPECT_EQ(summary.directors[0].daemons[2].configured_name, "admin");
  EXPECT_EQ(summary.directors[0].daemons[0].resources[0].type, "configuration");
  EXPECT_EQ(summary.directors[0].daemons[0].resources[1].type, "device");
  EXPECT_EQ(summary.directors[0].daemons[0].resources[2].type, "director");
  EXPECT_EQ(summary.directors[0].daemons[0].resources[3].type, "storage");
  EXPECT_EQ(summary.directors[0].daemons[1].resources[0].type, "configuration");
  EXPECT_EQ(summary.directors[0].daemons[1].resources[1].type, "client");
  EXPECT_EQ(summary.directors[0].daemons[1].resources[2].type, "director");
  EXPECT_EQ(summary.directors[0].daemons[1].resources[3].type, "fileset");
  EXPECT_EQ(summary.directors[0].daemons[2].resources[0].type, "configuration");
  EXPECT_EQ(summary.directors[0].daemons[2].resources[1].type, "console");
  EXPECT_EQ(summary.directors[0].resources[0].id,
            "resource-0-director-main");
  EXPECT_EQ(summary.directors[0].daemons[0].id, "daemon-0-storage-daemon");
  EXPECT_EQ(summary.tree.kind, "datacenter");
  ASSERT_EQ(summary.tree.children.size(), 4U);
  EXPECT_EQ(summary.tree.children[0].kind, "director");
  EXPECT_EQ(summary.tree.children[0].id, "director-0");
  EXPECT_EQ(summary.tree.children[1].kind, "storage-daemon");
  EXPECT_EQ(summary.tree.children[1].id, "daemon-0-storage-daemon");
  EXPECT_EQ(summary.tree.children[2].kind, "file-daemon");
  EXPECT_EQ(summary.tree.children[2].id, "daemon-0-file-daemon");
  EXPECT_EQ(summary.tree.children[3].kind, "console");
  EXPECT_EQ(summary.tree.children[3].id, "daemon-0-console");
  EXPECT_TRUE(summary.tree.children[0].children.empty());
  EXPECT_EQ(summary.tree.children[0].resources[1].type, "client");
  EXPECT_EQ(summary.tree.children[0].resources[2].type, "director");
  EXPECT_EQ(summary.tree.children[0].resources[3].type, "storage");

  const auto* director = FindTreeNodeById(summary, "director-0");
  ASSERT_NE(director, nullptr);
  EXPECT_EQ(director->label, "bareos-dir");

  const auto* file_daemon = FindTreeNodeById(summary, "daemon-0-file-daemon");
  ASSERT_NE(file_daemon, nullptr);
  EXPECT_EQ(file_daemon->kind, "file-daemon");

  const auto* storage_reference = FindTreeNodeById(summary, "storage-0-0");
  EXPECT_EQ(storage_reference, nullptr);

  const auto director_relationships
      = FindRelationshipsForNode(summary, "director-0");
  ASSERT_EQ(director_relationships.size(), 5U);
  EXPECT_EQ(director_relationships[0].from_node_id, "director-0");
  EXPECT_EQ(director_relationships[0].to_node_id, "daemon-0-file-daemon");
  EXPECT_EQ(director_relationships[0].relation, "resource-name");
  EXPECT_EQ(director_relationships[0].endpoint_name, "example-fd");
  EXPECT_EQ(director_relationships[0].source_resource_type, "client");
  EXPECT_EQ(director_relationships[0].source_resource_name, "example-fd");
  EXPECT_EQ(director_relationships[0].target_resource_type, "client");
  EXPECT_EQ(director_relationships[0].target_resource_name, "example-fd");
  EXPECT_EQ(director_relationships[0].target_resource_path,
            (root.path() / "bareos-fd.d/client/example-fd.conf").string());
  EXPECT_NE(director_relationships[0].resolution.find("client resource example-fd"),
            std::string::npos);
  EXPECT_EQ(director_relationships[1].from_node_id, "daemon-0-file-daemon");
  EXPECT_EQ(director_relationships[1].to_node_id, "director-0");
  EXPECT_EQ(director_relationships[1].relation, "resource-name");
  EXPECT_EQ(director_relationships[1].endpoint_name, "bareos-dir");
  EXPECT_EQ(director_relationships[2].to_node_id, "daemon-0-storage-daemon");
  EXPECT_EQ(director_relationships[2].relation, "storage");
  EXPECT_EQ(director_relationships[2].source_resource_type, "storage");
  EXPECT_EQ(director_relationships[2].target_resource_type, "storage");
  EXPECT_EQ(director_relationships[3].from_node_id, "daemon-0-storage-daemon");
  EXPECT_EQ(director_relationships[3].to_node_id, "director-0");
  EXPECT_EQ(director_relationships[3].relation, "resource-name");
  EXPECT_EQ(director_relationships[4].from_node_id, "daemon-0-console");
  EXPECT_EQ(director_relationships[4].to_node_id, "director-0");
  EXPECT_EQ(director_relationships[4].relation, "resource-name");
  EXPECT_EQ(director_relationships[4].endpoint_name, "bareos-dir");
  EXPECT_EQ(director_relationships[4].source_resource_type, "director");
  EXPECT_EQ(director_relationships[4].target_resource_type, "director");

  const auto* resource
      = FindResourceById(summary, "resource-0-director-main");
  ASSERT_NE(resource, nullptr);
  EXPECT_EQ(resource->file_path,
            (root.path() / "bareos-dir.conf").string());

  const auto console_relationships = FindRelationshipsForNode(summary, "daemon-0-console");
  ASSERT_EQ(console_relationships.size(), 1U);
  EXPECT_EQ(console_relationships[0].to_node_id, "director-0");
  EXPECT_EQ(console_relationships[0].relation, "resource-name");
}

TEST(BareosConfigModel, UsesDirectorConfigStemAndNormalizesRoot)
{
  TempConfigRoot root;
  std::filesystem::create_directories(root.path() / "bareos-dir.d/client");
  std::ofstream(root.path() / "bareos-dir.conf")
      << "Director {\n  Name = main-director\n}\n";

  const auto summary
      = DiscoverDatacenterSummary({root.path().string() + "/"});

  ASSERT_EQ(summary.directors.size(), 1U);
  EXPECT_EQ(summary.directors[0].name, "bareos-dir");
  EXPECT_EQ(summary.tree.children[0].label, "bareos-dir");
}

TEST(BareosConfigModel, UsesDirectorConfigStemEvenWithOtherNames)
{
  TempConfigRoot root;
  std::filesystem::create_directories(root.path() / "bareos-dir.d/client");
  std::ofstream(root.path() / "bareos-dir.conf")
      << "Client {\n"
      << "  Name = bareos-fd\n"
      << "}\n"
      << "Director {\n"
      << "  Name = bareos\n"
      << "}\n";

  const auto summary = DiscoverDatacenterSummary({root.path()});

  ASSERT_EQ(summary.directors.size(), 1U);
  EXPECT_EQ(summary.directors[0].name, "bareos-dir");
  EXPECT_EQ(summary.tree.children[0].label, "bareos-dir");
}

TEST(BareosConfigModel, DetectsGenericReferenceAndSharedPasswordRelationships)
{
  TempConfigRoot root;
  std::filesystem::create_directories(root.path() / "bareos-dir.d/client");
  std::filesystem::create_directories(root.path() / "bareos-dir.d/director");
  std::filesystem::create_directories(root.path() / "bareos-dir.d/fileset");
  std::filesystem::create_directories(root.path() / "bareos-dir.d/job");
  std::filesystem::create_directories(root.path() / "bareos-dir.d/storage");
  std::filesystem::create_directories(root.path() / "bareos-sd.d/device");
  std::filesystem::create_directories(root.path() / "bconsole.d/console");
  std::filesystem::create_directories(root.path() / "bconsole.d/user");
  std::ofstream(root.path() / "bareos-dir.conf") << "Director {}\n";
  std::ofstream(root.path() / "bareos-fd.conf")
      << "FileDaemon {\n  Name = example-fd\n}\n";
  std::ofstream(root.path() / "bareos-sd.conf")
      << "Storage {\n  Name = example-sd\n}\n";
  const auto director_path = root.path() / "bareos-dir.d/director/bareos-dir.conf";
  const auto client_path = root.path() / "bareos-dir.d/client/example-fd.conf";
  const auto fileset_path = root.path() / "bareos-dir.d/fileset/SelfTest.conf";
  const auto job_path = root.path() / "bareos-dir.d/job/example-job.conf";
  const auto storage_path = root.path() / "bareos-dir.d/storage/example-sd.conf";
  const auto console_path = root.path() / "bconsole.d/console/admin.conf";
  const auto user_path = root.path() / "bconsole.d/user/api.conf";
  std::ofstream(director_path)
      << "Director {\n  Name = bareos-dir\n  Password = secret\n}\n";
  std::ofstream(client_path)
      << "Client {\n  Name = example-fd\n  Address = 10.0.0.10\n  Password = secret\n}\n";
  std::ofstream(fileset_path)
      << "FileSet {\n  Name = SelfTest\n  Include {\n    Options {}\n  }\n}\n";
  std::ofstream(job_path)
      << "Job {\n"
      << "  Name = BackupClient\n"
      << "  Client = example-fd\n"
      << "  FileSet = SelfTest\n"
      << "  Storage = example-sd\n"
      << "}\n";
  std::ofstream(storage_path)
      << "Storage {\n"
      << "  Name = example-sd\n"
      << "  Address = 10.0.0.20\n"
      << "  Device = tape-drive-0\n"
      << "  Password = secret\n"
      << "}\n";
  std::ofstream(root.path() / "bareos-sd.d/device/tape.conf")
      << "Device {\n  Name = tape-drive-0\n  ArchiveDevice = /tmp/tape\n  MediaType = File\n}\n";
  std::ofstream(root.path() / "bconsole.conf")
      << "Director {\n  Name = bareos-dir\n  Password = secret\n}\n"
      << "Console {\n  Password = secret\n}\n";
  std::ofstream(console_path)
      << "Console {\n  Name = admin\n  Password = secret\n}\n";
  std::ofstream(user_path)
      << "User {\n  Name = api\n  Password = secret\n}\n";

  const auto summary = DiscoverDatacenterSummary({root.path()});
  const auto relationships = FindRelationshipsForNode(summary, "director-0");

  const auto client_resource_relationship = std::find_if(
      relationships.begin(), relationships.end(),
      [&job_path, &client_path](const RelationshipSummary& relationship) {
        return relationship.relation == "resource-name"
               && relationship.source_resource_path == job_path.string()
                && relationship.endpoint_name == "example-fd"
               && relationship.target_resource_path == client_path.string();
      });
  ASSERT_NE(client_resource_relationship, relationships.end());
  EXPECT_EQ(client_resource_relationship->to_node_id, "director-0");
  EXPECT_EQ(std::count_if(
                relationships.begin(), relationships.end(),
                [&job_path](const RelationshipSummary& relationship) {
                  return relationship.relation == "daemon-name"
                         && relationship.source_resource_path == job_path.string()
                         && relationship.endpoint_name == "example-fd";
                }),
            0);

  const auto resource_name_relationship = std::find_if(
      relationships.begin(), relationships.end(),
      [&job_path, &fileset_path](const RelationshipSummary& relationship) {
        return relationship.relation == "resource-name"
               && relationship.source_resource_path == job_path.string()
               && relationship.endpoint_name == "SelfTest"
               && relationship.target_resource_path == fileset_path.string();
      });
  ASSERT_NE(resource_name_relationship, relationships.end());

  const auto shared_password_relationship = std::find_if(
      relationships.begin(), relationships.end(),
      [&director_path, &client_path, &storage_path](const RelationshipSummary& relationship) {
        if (relationship.relation != "shared-password") return false;
        const auto source_path = relationship.source_resource_path;
        const auto target_path = relationship.target_resource_path;
        return (source_path == director_path.string()
                && (target_path == client_path.string()
                    || target_path == storage_path.string()))
               || (target_path == director_path.string()
                   && (source_path == client_path.string()
                       || source_path == storage_path.string()));
      });
  ASSERT_NE(shared_password_relationship, relationships.end());
}

TEST(BareosConfigModel, BuildsNamedConsoleAuthenticationRelationships)
{
  TempConfigRoot root;
  std::filesystem::create_directories(root.path() / "bareos-dir.d/director");
  std::filesystem::create_directories(root.path() / "bareos-dir.d/console");
  std::filesystem::create_directories(root.path() / "bconsole.d/console");
  std::ofstream(root.path() / "bareos-dir.conf") << "Director {}\n";
  const auto director_path = root.path() / "bareos-dir.d/director/bareos-dir.conf";
  const auto director_console_path = root.path() / "bareos-dir.d/console/admin.conf";
  const auto bconsole_path = root.path() / "bconsole.conf";
  const auto bconsole_console_path = root.path() / "bconsole.d/console/admin.conf";
  std::ofstream(director_path)
      << "Director {\n  Name = bareos-dir\n}\n";
  std::ofstream(director_console_path)
      << "Console {\n  Name = admin\n  Password = secret\n}\n";
  std::ofstream(bconsole_path)
      << "Director {\n  Name = bareos-dir\n}\n";
  std::ofstream(bconsole_console_path)
      << "Console {\n  Name = admin\n  Password = secret\n}\n";

  const auto summary = DiscoverDatacenterSummary({root.path()});
  const auto relationships = FindRelationshipsForNode(summary, "daemon-0-console");

  const auto director_relationship = std::find_if(
      relationships.begin(), relationships.end(),
      [&bconsole_path, &director_path](const RelationshipSummary& relationship) {
        return relationship.relation == "resource-name"
               && relationship.source_resource_type == "director"
               && relationship.source_resource_path == bconsole_path.string()
               && relationship.target_resource_path == director_path.string();
      });
  ASSERT_NE(director_relationship, relationships.end());

  const auto console_relationship = std::find_if(
      relationships.begin(), relationships.end(),
      [&bconsole_console_path,
       &director_console_path](const RelationshipSummary& relationship) {
        return relationship.relation == "resource-name"
               && relationship.source_resource_type == "console"
               && relationship.source_resource_path == bconsole_console_path.string()
               && relationship.target_resource_path == director_console_path.string();
      });
  ASSERT_NE(console_relationship, relationships.end());

  const auto password_relationship = std::find_if(
      relationships.begin(), relationships.end(),
      [&bconsole_console_path,
       &director_console_path](const RelationshipSummary& relationship) {
        return relationship.relation == "shared-password"
               && relationship.source_resource_type == "console"
               && ((relationship.source_resource_path == bconsole_console_path.string()
                    && relationship.target_resource_path == director_console_path.string())
                   || (relationship.target_resource_path == bconsole_console_path.string()
                       && relationship.source_resource_path
                              == director_console_path.string()));
      });
  ASSERT_NE(password_relationship, relationships.end());
}

TEST(BareosConfigModel, BuildsRootConsoleAuthenticationRelationships)
{
  TempConfigRoot root;
  std::filesystem::create_directories(root.path() / "bareos-dir.d/director");
  std::ofstream(root.path() / "bareos-dir.conf") << "Director {}\n";
  const auto director_path = root.path() / "bareos-dir.d/director/bareos-dir.conf";
  const auto bconsole_path = root.path() / "bconsole.conf";
  std::ofstream(director_path)
      << "Director {\n  Name = bareos-dir\n  Password = secret\n}\n";
  std::ofstream(bconsole_path)
      << "Director {\n  Name = bareos-dir\n  Password = secret\n}\n";

  const auto summary = DiscoverDatacenterSummary({root.path()});
  const auto relationships = FindRelationshipsForNode(summary, "daemon-0-console");

  const auto director_relationship = std::find_if(
      relationships.begin(), relationships.end(),
      [&bconsole_path, &director_path](const RelationshipSummary& relationship) {
        return relationship.relation == "resource-name"
               && relationship.source_resource_type == "director"
               && relationship.source_resource_path == bconsole_path.string()
               && relationship.target_resource_path == director_path.string();
      });
  ASSERT_NE(director_relationship, relationships.end());

  const auto password_relationship = std::find_if(
      relationships.begin(), relationships.end(),
      [&bconsole_path, &director_path](const RelationshipSummary& relationship) {
        return relationship.relation == "shared-password"
               && relationship.source_resource_type == "director"
               && ((relationship.source_resource_path == bconsole_path.string()
                    && relationship.target_resource_path == director_path.string())
                   || (relationship.target_resource_path == bconsole_path.string()
                       && relationship.source_resource_path == director_path.string()));
      });
  ASSERT_NE(password_relationship, relationships.end());
}

TEST(BareosConfigModel, UsesDirectorDirectoryStemWithoutMainConfig)
{
  TempConfigRoot root;
  std::filesystem::create_directories(root.path() / "bareos-dir.d/client");
  std::ofstream(root.path() / "bareos-dir.d/client/example-fd.conf")
      << "Client {\n  Name = example-fd\n}\n";

  const auto summary = DiscoverDatacenterSummary({root.path()});

  ASSERT_EQ(summary.directors.size(), 1U);
  EXPECT_EQ(summary.directors[0].name, "bareos-dir");
  ASSERT_FALSE(summary.tree.children.empty());
  EXPECT_EQ(summary.tree.children[0].label, "bareos-dir");
}

TEST(BareosConfigModel, KeepsMessagesReferencesScopedToOwningDaemon)
{
  TempConfigRoot root;
  std::filesystem::create_directories(root.path() / "bareos-dir.d/jobdefs");
  std::filesystem::create_directories(root.path() / "bareos-dir.d/messages");
  std::filesystem::create_directories(root.path() / "bareos-fd.d/client");
  std::filesystem::create_directories(root.path() / "bareos-fd.d/messages");
  const auto director_messages_path
      = root.path() / "bareos-dir.d/messages/Standard.conf";
  const auto file_daemon_messages_path
      = root.path() / "bareos-fd.d/messages/Standard.conf";
  std::ofstream(root.path() / "bareos-dir.conf")
      << "Director {\n  Name = bareos-dir\n}\n";
  std::ofstream(root.path() / "bareos-fd.conf")
      << "FileDaemon {\n  Name = example-fd\n}\n";
  std::ofstream(director_messages_path)
      << "Messages {\n  Name = Standard\n}\n";
  std::ofstream(file_daemon_messages_path)
      << "Messages {\n  Name = Standard\n}\n";
  std::ofstream(root.path() / "bareos-dir.d/jobdefs/DefaultJob.conf")
      << "JobDefs {\n  Name = DefaultJob\n  Messages = Standard\n}\n";
  std::ofstream(root.path() / "bareos-fd.d/client/example-fd.conf")
      << "Client {\n  Name = example-fd\n  Messages = Standard\n}\n";

  const auto summary = DiscoverDatacenterSummary({root.path()});

  const auto director_relationships = FindRelationshipsForNode(summary, "director-0");
  const auto director_message_relationship = std::find_if(
      director_relationships.begin(), director_relationships.end(),
      [&director_messages_path](const RelationshipSummary& relationship) {
        return relationship.relation == "resource-name"
               && relationship.endpoint_name == "Standard"
               && relationship.target_resource_path == director_messages_path.string();
      });
  ASSERT_NE(director_message_relationship, director_relationships.end());
  EXPECT_EQ(director_message_relationship->to_node_id, "director-0");
  EXPECT_EQ(director_message_relationship->source_resource_type, "jobdefs");
  EXPECT_EQ(director_message_relationship->source_resource_name, "DefaultJob");

  const auto file_daemon_relationships
      = FindRelationshipsForNode(summary, "daemon-0-file-daemon");
  const auto file_daemon_message_relationship = std::find_if(
      file_daemon_relationships.begin(), file_daemon_relationships.end(),
      [&file_daemon_messages_path](const RelationshipSummary& relationship) {
        return relationship.relation == "resource-name"
               && relationship.endpoint_name == "Standard"
               && relationship.target_resource_path
                      == file_daemon_messages_path.string();
      });
  ASSERT_NE(file_daemon_message_relationship, file_daemon_relationships.end());
  EXPECT_EQ(file_daemon_message_relationship->to_node_id, "daemon-0-file-daemon");
  EXPECT_EQ(file_daemon_message_relationship->source_resource_type, "client");
  EXPECT_EQ(file_daemon_message_relationship->source_resource_name, "example-fd");
  EXPECT_EQ(std::count_if(
                file_daemon_relationships.begin(), file_daemon_relationships.end(),
                [&director_messages_path](const RelationshipSummary& relationship) {
                  return relationship.target_resource_path
                         == director_messages_path.string();
                }),
            0);
}

TEST(BareosConfigModel, KeepsKnownRemoteClientNodeWithoutRelationship)
{
  TempConfigRoot root;
  std::filesystem::create_directories(root.path() / "bareos-dir.d/client");
  std::filesystem::create_directories(root.path() / "bareos-fd.d/fileset");
  std::ofstream(root.path() / "bareos-dir.conf") << "Director {}\n";
  std::ofstream(root.path() / "bareos-fd.conf")
      << "FileDaemon {\n  Name = different-fd\n}\n";
  std::ofstream(root.path() / "bareos-dir.d/client/example-fd.conf")
      << "Client {\n  Name = example-fd\n}\n";
  std::ofstream(root.path() / "bareos-fd.d/fileset/local.conf") << "FileSet {}\n";

  const auto summary = DiscoverDatacenterSummary({root.path()});
  const auto relationships = FindRelationshipsForNode(summary, "director-0");

  EXPECT_TRUE(relationships.empty());

  const auto* remote_client = FindTreeNodeById(summary, "known-client-0-example-fd");
  ASSERT_NE(remote_client, nullptr);
  EXPECT_EQ(remote_client->kind, "client");
  EXPECT_EQ(remote_client->label, "example-fd");
}

TEST(BareosConfigModel, OmitsUnresolvedGenericResourceReferences)
{
  TempConfigRoot root;
  std::filesystem::create_directories(root.path() / "bareos-dir.d/job");
  std::ofstream(root.path() / "bareos-dir.conf") << "Director {}\n";
  std::ofstream(root.path() / "bareos-dir.d/job/example-job.conf")
      << "Job {\n"
      << "  Name = BackupClient\n"
      << "  FileSet = MissingFileSet\n"
      << "}\n";

  const auto summary = DiscoverDatacenterSummary({root.path()});
  const auto relationships = FindRelationshipsForNode(summary, "director-0");

  EXPECT_TRUE(relationships.empty());
}

TEST(BareosConfigModel, ResolvesRelationshipFromDaemonOwnedResourceName)
{
  TempConfigRoot root;
  std::filesystem::create_directories(root.path() / "bareos-dir.d/client");
  std::filesystem::create_directories(root.path() / "bareos-dir.d/director");
  std::filesystem::create_directories(root.path() / "bareos-dir.d/storage");
  std::filesystem::create_directories(root.path() / "bareos-fd.d/client");
  std::filesystem::create_directories(root.path() / "bareos-fd.d/director");
  std::filesystem::create_directories(root.path() / "bareos-sd.d/director");
  std::filesystem::create_directories(root.path() / "bareos-sd.d/storage");
  std::ofstream(root.path() / "bareos-dir.conf") << "Director {}\n";
  std::ofstream(root.path() / "bareos-fd.conf")
      << "FileDaemon {\n  Name = bareos-fd\n}\n";
  std::ofstream(root.path() / "bareos-sd.conf")
      << "Storage {\n  Name = File\n}\n";
  std::ofstream(root.path() / "bareos-dir.d/director/bareos-dir.conf")
      << "Director {\n  Name = bareos-dir\n}\n";
  std::ofstream(root.path() / "bareos-dir.d/client/bareos-fd.conf")
      << "Client {\n  Name = bareos-fd\n}\n";
  std::ofstream(root.path() / "bareos-dir.d/storage/File.conf")
      << "Storage {\n  Name = File\n}\n";
  std::ofstream(root.path() / "bareos-fd.d/client/myself.conf")
      << "Client {\n  Name = bareos-fd\n}\n";
  std::ofstream(root.path() / "bareos-fd.d/director/bareos-dir.conf")
      << "Director {\n  Name = bareos-dir\n}\n";
  std::ofstream(root.path() / "bareos-sd.d/storage/bareos-sd.conf")
      << "Storage {\n  Name = File\n}\n";
  std::ofstream(root.path() / "bareos-sd.d/director/bareos-dir.conf")
      << "Director {\n  Name = bareos-dir\n}\n";

  const auto summary = DiscoverDatacenterSummary({root.path()});
  const auto relationships = FindRelationshipsForNode(summary, "director-0");

  ASSERT_EQ(relationships.size(), 4U);
  EXPECT_TRUE(relationships[0].resolved);
  EXPECT_EQ(relationships[0].to_node_id, "daemon-0-file-daemon");
  EXPECT_TRUE(relationships[1].resolved);
  EXPECT_EQ(relationships[1].to_node_id, "director-0");
  EXPECT_TRUE(relationships[2].resolved);
  EXPECT_EQ(relationships[2].to_node_id, "daemon-0-storage-daemon");
  EXPECT_TRUE(relationships[3].resolved);
  EXPECT_EQ(relationships[3].to_node_id, "director-0");
}

TEST(BareosConfigModel, UsesParsedResourceNameInsteadOfFilenameStem)
{
  TempConfigRoot root;
  std::filesystem::create_directories(root.path() / "bareos-dir.d/client");
  std::filesystem::create_directories(root.path() / "bareos-fd.d/client");
  std::filesystem::create_directories(root.path() / "bareos-fd.d/director");
  std::ofstream(root.path() / "bareos-dir.conf") << "Director {}\n";
  std::ofstream(root.path() / "bareos-fd.conf")
      << "FileDaemon {\n  Name = example-fd\n}\n";
  std::ofstream(root.path() / "bareos-dir.d/client/not-the-name.conf")
      << "Client {\n  Name = example-fd\n}\n";
  std::ofstream(root.path() / "bareos-fd.d/client/myself.conf")
      << "Client {\n  Name = example-fd\n}\n";
  std::ofstream(root.path() / "bareos-fd.d/director/bareos-dir.conf")
      << "Director {\n  Name = bareos-dir\n}\n";

  const auto summary = DiscoverDatacenterSummary({root.path()});

  ASSERT_GE(summary.directors[0].resources.size(), 2U);
  EXPECT_EQ(summary.directors[0].resources[1].name, "example-fd");

  const auto relationships = FindRelationshipsForNode(summary, "director-0");
  ASSERT_EQ(relationships.size(), 1U);
  EXPECT_EQ(relationships[0].endpoint_name, "example-fd");
  EXPECT_TRUE(relationships[0].resolved);
}

TEST(BareosConfigModel, SplitsMultipleResourcesFromOneFile)
{
  TempConfigRoot root;
  std::filesystem::create_directories(root.path() / "bareos-dir.d/storage");
  std::ofstream(root.path() / "bareos-dir.conf") << "Director {}\n";
  std::ofstream(root.path() / "bareos-dir.d/storage/File.conf")
      << "Storage {\n"
      << "  Name = File\n"
      << "  Address = quadstorvtl\n"
      << "}\n\n"
      << "Storage {\n"
      << "  Name = File2\n"
      << "  Address = quadstorvtl\n"
      << "}\n";

  const auto summary = DiscoverDatacenterSummary({root.path()});
  std::vector<ResourceSummary> storages;
  for (const auto& resource : summary.directors[0].resources) {
    if (resource.type == "storage") storages.push_back(resource);
  }

  ASSERT_EQ(storages.size(), 2U);
  EXPECT_EQ(storages[0].name, "File");
  EXPECT_EQ(storages[1].name, "File2");
  EXPECT_EQ(storages[0].file_path, storages[1].file_path);

  const auto detail = LoadResourceDetail(storages[1]);
  EXPECT_NE(detail.content.find("Name = File2"), std::string::npos);
  EXPECT_EQ(detail.content.find("Name = File\n"), std::string::npos);
  for (const auto& message : detail.validation_messages) {
    EXPECT_NE(message.code, "duplicate-directive");
  }
}

TEST(BareosConfigModel, SerializesDatacenterSummaryAsJson)
{
  DatacenterSummary summary;
  summary.config_roots = {"/etc/bareos"};
  summary.directors.push_back({
      "director-id",
      "Main Director",
      "/etc/bareos",
      {{"resource-id", "client", "example-fd", "/etc/bareos/bareos-dir.d/client/example-fd.conf"}},
      {{"daemon-id", "file-daemon", "bareos-fd", "example-fd", "/etc/bareos/bareos-fd.d", {}}},
  });

  const auto json = SerializeDatacenterSummary(summary);

  EXPECT_NE(json.find("\"name\":\"Datacenter\""), std::string::npos);
  EXPECT_NE(json.find("\"name\":\"Main Director\""), std::string::npos);
  EXPECT_NE(json.find("\"kind\":\"file-daemon\""), std::string::npos);
  EXPECT_NE(json.find("\"type\":\"client\""), std::string::npos);
  EXPECT_NE(json.find("\"tree\":{"), std::string::npos);
  EXPECT_NE(json.find("\"kind\":\"datacenter\""), std::string::npos);
}

TEST(BareosConfigModel, LoadsResourceDetailContent)
{
  TempConfigRoot root;
  std::filesystem::create_directories(root.path() / "bareos-dir.d/client");

  const auto resource_path = root.path() / "bareos-dir.d/client/example-fd.conf";
  std::ofstream(resource_path) << "Client {\n  Name = example-fd\n}\n";

  ResourceSummary summary{"resource-1", "client", "example-fd",
                          resource_path.string()};

  const auto detail = LoadResourceDetail(summary);

  EXPECT_EQ(detail.summary.id, "resource-1");
  EXPECT_NE(detail.content.find("Name = example-fd"), std::string::npos);
  ASSERT_EQ(detail.directives.size(), 1U);
  EXPECT_EQ(detail.directives[0].key, "Name");
  EXPECT_EQ(detail.directives[0].value, "example-fd");
  EXPECT_EQ(detail.directives[0].line, 2U);
  EXPECT_EQ(detail.directives[0].nesting_level, 1U);
  ASSERT_EQ(detail.validation_messages.size(), 2U);
  EXPECT_EQ(detail.validation_messages[0].code, "missing-address");
  EXPECT_EQ(detail.validation_messages[1].code, "missing-required-directive");
  const auto has_required_hint = [&detail](const std::string& key) {
    const auto it = std::find_if(detail.field_hints.begin(), detail.field_hints.end(),
                                 [&key](const ResourceFieldHint& hint) {
                                   return hint.key == key && hint.required;
                                 });
    return it != detail.field_hints.end();
  };
  EXPECT_TRUE(has_required_hint("Name"));
  EXPECT_TRUE(has_required_hint("Address"));
  EXPECT_TRUE(has_required_hint("Password"));
  ASSERT_GE(detail.field_hints.size(), 3U);
  EXPECT_EQ(detail.field_hints[0].key, "Name");
  EXPECT_EQ(detail.field_hints[1].key, "Address");
  EXPECT_EQ(detail.field_hints[2].key, "Password");

  const auto json = SerializeResourceDetail(detail);
  EXPECT_NE(json.find("\"id\":\"resource-1\""), std::string::npos);
  EXPECT_NE(json.find("Name = example-fd"), std::string::npos);
  EXPECT_NE(json.find("\"directives\":[{"), std::string::npos);
  EXPECT_NE(json.find("\"validation_messages\":[{"), std::string::npos);
  EXPECT_NE(json.find("\"field_hints\":[{"), std::string::npos);
}

TEST(BareosConfigModel, ResolvesInheritedJobDefsDirectives)
{
  TempConfigRoot root;
  std::filesystem::create_directories(root.path() / "bareos-dir.d/job");
  std::filesystem::create_directories(root.path() / "bareos-dir.d/jobdefs");

  std::ofstream(root.path() / "bareos-dir.d/jobdefs/DefaultJob.conf")
      << "JobDefs {\n"
      << "  Name = DefaultJob\n"
      << "  Type = Backup\n"
      << "  Pool = Full\n"
      << "  Messages = Standard\n"
      << "}\n";
  const auto resource_path = root.path() / "bareos-dir.d/job/example.conf";
  std::ofstream(resource_path)
      << "Job {\n"
      << "  Name = ExampleJob\n"
      << "  JobDefs = DefaultJob\n"
      << "  Client = example-fd\n"
      << "  FileSet = SelfTest\n"
      << "}\n";

  ResourceSummary summary{"resource-1", "job", "ExampleJob",
                          resource_path.string()};
  const auto detail = LoadResourceDetail(summary);

  EXPECT_EQ(detail.inherited_directives.size(), 3U);
  EXPECT_EQ(detail.inherited_directives[0].source_resource_type, "jobdefs");
  EXPECT_EQ(detail.inherited_directives[0].source_resource_name, "DefaultJob");

  const auto has_inherited_key = [&detail](const std::string& key) {
    return std::any_of(detail.inherited_directives.begin(),
                       detail.inherited_directives.end(),
                       [&key](const InheritedDirective& directive) {
                         return directive.directive.key == key;
                       });
  };
  EXPECT_TRUE(has_inherited_key("Type"));
  EXPECT_TRUE(has_inherited_key("Pool"));
  EXPECT_TRUE(has_inherited_key("Messages"));

  const auto type_hint = std::find_if(
      detail.field_hints.begin(), detail.field_hints.end(),
      [](const ResourceFieldHint& hint) { return hint.key == "Type"; });
  ASSERT_NE(type_hint, detail.field_hints.end());
  EXPECT_FALSE(type_hint->present);
  EXPECT_TRUE(type_hint->inherited);
  EXPECT_EQ(type_hint->inherited_value, "Backup");
  EXPECT_EQ(type_hint->inherited_source_resource_type, "jobdefs");
  EXPECT_EQ(type_hint->inherited_source_resource_name, "DefaultJob");

  for (const auto& message : detail.validation_messages) {
    EXPECT_EQ(message.message.find("Type is required for this resource type."),
              std::string::npos);
    EXPECT_EQ(message.message.find("Pool is required for this resource type."),
              std::string::npos);
    EXPECT_EQ(message.message.find("Messages is required for this resource type."),
              std::string::npos);
  }
}

TEST(BareosConfigModel, UsesParserMetadataForReferenceFieldHints)
{
  TempConfigRoot root;
  std::filesystem::create_directories(root.path() / "bareos-dir.d/catalog");
  std::filesystem::create_directories(root.path() / "bareos-dir.d/client");

  std::ofstream(root.path() / "bareos-dir.d/catalog/MyCatalog.conf")
      << "Catalog {\n  Name = MyCatalog\n}\n";
  const auto resource_path = root.path() / "bareos-dir.d/client/example-fd.conf";
  std::ofstream(resource_path)
      << "Client {\n  Name = example-fd\n  Address = 10.0.0.20\n}\n";

  ResourceSummary summary{"resource-1", "client", "example-fd",
                          resource_path.string()};

  const auto detail = LoadResourceDetail(summary);
  const auto catalog_hint = std::find_if(
      detail.field_hints.begin(), detail.field_hints.end(),
      [](const ResourceFieldHint& hint) { return hint.key == "Catalog"; });

  ASSERT_NE(catalog_hint, detail.field_hints.end());
  EXPECT_EQ(catalog_hint->datatype, "RES");
  EXPECT_EQ(catalog_hint->related_resource_type, "catalog");
  EXPECT_NE(std::find(catalog_hint->allowed_values.begin(),
                      catalog_hint->allowed_values.end(), "MyCatalog"),
            catalog_hint->allowed_values.end());

  const auto file_retention_hint = std::find_if(
      detail.field_hints.begin(), detail.field_hints.end(),
      [](const ResourceFieldHint& hint) { return hint.key == "FileRetention"; });
  ASSERT_NE(file_retention_hint, detail.field_hints.end());
  EXPECT_TRUE(file_retention_hint->deprecated);
}

TEST(BareosConfigModel, AllowsRepeatableMessagesDestinations)
{
  TempConfigRoot root;
  std::filesystem::create_directories(root.path() / "bareos-dir.d/messages");

  const auto resource_path = root.path() / "bareos-dir.d/messages/daemon.conf";
  std::ofstream(resource_path)
      << "Messages {\n"
      << "  Name = Daemon\n"
      << "  Append = \"/var/log/bareos.log\" = all, !skipped\n"
      << "  Append = \"/var/log/bareos-errors.log\" = error, fatal\n"
      << "}\n";

  ResourceSummary summary{"resource-1", "messages", "Daemon",
                          resource_path.string()};
  const auto detail = LoadResourceDetail(summary);

  const auto append_hint = std::find_if(
      detail.field_hints.begin(), detail.field_hints.end(),
      [](const ResourceFieldHint& hint) { return hint.key == "Append"; });
  ASSERT_NE(append_hint, detail.field_hints.end());
  EXPECT_TRUE(append_hint->repeatable);

  for (const auto& message : detail.validation_messages) {
    EXPECT_NE(message.code, "duplicate-directive");
  }
}

TEST(BareosConfigModel, AllowsRepeatableProfileCommandAcl)
{
  TempConfigRoot root;
  std::filesystem::create_directories(root.path() / "bareos-dir.d/profile");

  const auto resource_path = root.path() / "bareos-dir.d/profile/operator.conf";
  std::ofstream(resource_path)
      << "Profile {\n"
      << "  Name = operator\n"
      << "  Command ACL = list, llist\n"
      << "  Command ACL = status, show\n"
      << "}\n";

  ResourceSummary summary{"resource-1", "profile", "operator",
                          resource_path.string()};
  const auto detail = LoadResourceDetail(summary);

  const auto command_acl_hint = std::find_if(
      detail.field_hints.begin(), detail.field_hints.end(),
      [](const ResourceFieldHint& hint) { return hint.key == "CommandAcl"; });
  ASSERT_NE(command_acl_hint, detail.field_hints.end());
  EXPECT_TRUE(command_acl_hint->repeatable);

  for (const auto& message : detail.validation_messages) {
    EXPECT_NE(message.code, "duplicate-directive");
  }
}

TEST(BareosConfigModel, NormalizesQuotedProfileAllowedValuesForConsole)
{
  TempConfigRoot root;
  std::filesystem::create_directories(root.path() / "bareos-dir.d/console");
  std::filesystem::create_directories(root.path() / "bareos-dir.d/profile");
  std::ofstream(root.path() / "bareos-dir.conf") << "Director {}\n";
  std::ofstream(root.path() / "bareos-dir.d/profile/operator.conf")
      << "Profile {\n  Name = \"operator\"\n}\n";
  std::ofstream(root.path() / "bareos-dir.d/profile/readonly.conf")
      << "Profile {\n  Name = readonly\n}\n";
  const auto resource_path = root.path() / "bareos-dir.d/console/admin.conf";
  std::ofstream(resource_path)
      << "Console {\n"
      << "  Name = admin\n"
      << "  Profile = \"operator\"\n"
      << "}\n";

  ResourceSummary summary{"resource-1", "console", "admin",
                          resource_path.string()};
  const auto detail = LoadResourceDetail(summary);
  const auto profile_hint = std::find_if(
      detail.field_hints.begin(), detail.field_hints.end(),
      [](const ResourceFieldHint& hint) { return hint.key == "Profile"; });

  ASSERT_NE(profile_hint, detail.field_hints.end());
  EXPECT_EQ(profile_hint->related_resource_type, "profile");
  EXPECT_NE(std::find(profile_hint->allowed_values.begin(),
                      profile_hint->allowed_values.end(), "operator"),
            profile_hint->allowed_values.end());
  EXPECT_NE(std::find(profile_hint->allowed_values.begin(),
                      profile_hint->allowed_values.end(), "readonly"),
            profile_hint->allowed_values.end());
  EXPECT_EQ(std::find(profile_hint->allowed_values.begin(),
                      profile_hint->allowed_values.end(), "\"operator\""),
            profile_hint->allowed_values.end());
  EXPECT_EQ(profile_hint->allowed_values.front(), "operator");
}

TEST(BareosConfigModel, NormalizesQuotedRepeatableProfileAllowedValuesForUser)
{
  TempConfigRoot root;
  std::filesystem::create_directories(root.path() / "bareos-dir.d/profile");
  std::filesystem::create_directories(root.path() / "bareos-dir.d/user");
  std::ofstream(root.path() / "bareos-dir.conf") << "Director {}\n";
  std::ofstream(root.path() / "bareos-dir.d/profile/webui-admin.conf")
      << "Profile {\n  Name = \"webui-admin\"\n}\n";
  std::ofstream(root.path() / "bareos-dir.d/profile/webui-readonly.conf")
      << "Profile {\n  Name = webui-readonly\n}\n";
  const auto resource_path = root.path() / "bareos-dir.d/user/admin.conf";
  std::ofstream(resource_path)
      << "User {\n"
      << "  Name = admin\n"
      << "  Password = secret\n"
      << "  Profile = \"webui-admin\"\n"
      << "  Profile = webui-readonly\n"
      << "}\n";

  ResourceSummary summary{"resource-1", "user", "admin", resource_path.string()};
  const auto detail = LoadResourceDetail(summary);
  const auto profile_hint = std::find_if(
      detail.field_hints.begin(), detail.field_hints.end(),
      [](const ResourceFieldHint& hint) { return hint.key == "Profile"; });

  ASSERT_NE(profile_hint, detail.field_hints.end());
  EXPECT_TRUE(profile_hint->repeatable);
  EXPECT_EQ(profile_hint->related_resource_type, "profile");
  EXPECT_NE(std::find(profile_hint->allowed_values.begin(),
                      profile_hint->allowed_values.end(), "webui-admin"),
            profile_hint->allowed_values.end());
  EXPECT_NE(std::find(profile_hint->allowed_values.begin(),
                      profile_hint->allowed_values.end(), "webui-readonly"),
            profile_hint->allowed_values.end());
  EXPECT_EQ(std::find(profile_hint->allowed_values.begin(),
                      profile_hint->allowed_values.end(), "\"webui-admin\""),
            profile_hint->allowed_values.end());
  EXPECT_EQ(std::find(profile_hint->allowed_values.begin(),
                      profile_hint->allowed_values.end(),
                      "\"webui-admin\"\nwebui-readonly"),
            profile_hint->allowed_values.end());
}

TEST(BareosConfigModel, BuildsFieldHintPreviewContent)
{
  TempConfigRoot root;
  std::filesystem::create_directories(root.path() / "bareos-dir.d/client");

  const auto resource_path = root.path() / "bareos-dir.d/client/example-fd.conf";
  std::ofstream(resource_path) << "Client {\n  Name = example-fd\n}\n";

  ResourceSummary summary{"resource-1", "client", "example-fd",
                          resource_path.string()};
  const auto detail = LoadResourceDetail(summary);

  auto updated_field_hints = detail.field_hints;
  for (auto& field : updated_field_hints) {
    if (field.key == "Address") {
      field.value = "10.0.0.20";
      field.present = true;
    }
    if (field.key == "Password") {
      field.value = "secret";
      field.present = true;
    }
  }

  const auto preview = BuildFieldHintEditPreview(detail, updated_field_hints);

  EXPECT_TRUE(preview.changed);
  EXPECT_LT(preview.updated_content.find("Name = example-fd"),
            preview.updated_content.find("Address = 10.0.0.20"));
  EXPECT_LT(preview.updated_content.find("Address = 10.0.0.20"),
            preview.updated_content.find("Password = secret"));
  EXPECT_NE(preview.updated_content.find("Address = 10.0.0.20"),
            std::string::npos);
  EXPECT_TRUE(preview.updated_validation_messages.empty());
}

TEST(BareosConfigModel, UpdatesPreviewSummaryNameAfterRename)
{
  TempConfigRoot root;
  std::filesystem::create_directories(root.path() / "bareos-dir.d/client");

  const auto resource_path = root.path() / "bareos-dir.d/client/example-fd.conf";
  std::ofstream(resource_path)
      << "Client {\n"
      << "  Name = example-fd\n"
      << "  Address = 10.0.0.20\n"
      << "  Password = secret\n"
      << "}\n";

  ResourceSummary summary{"resource-1", "client", "example-fd",
                          resource_path.string()};
  const auto detail = LoadResourceDetail(summary);
  const auto preview = BuildResourceEditPreview(
      detail,
      "Client {\n  Name = renamed-fd\n  Address = 10.0.0.20\n  Password = secret\n}\n");

  EXPECT_EQ(preview.summary.name, "renamed-fd");
  EXPECT_EQ(preview.summary.file_path,
            (root.path() / "bareos-dir.d/client/renamed-fd.conf").string());
}

TEST(BareosConfigModel, PreservesRepeatableScheduleRunDirectives)
{
  TempConfigRoot root;
  std::filesystem::create_directories(root.path() / "bareos-dir.d/schedule");

  const auto resource_path = root.path() / "bareos-dir.d/schedule/Nightly.conf";
  std::ofstream(resource_path)
      << "Schedule {\n"
      << "  Name = Nightly\n"
      << "  Run = Level=Full mon at 21:00\n"
      << "  Run = Level=Incremental tue at 21:00\n"
      << "}\n";

  ResourceSummary summary{"resource-1", "schedule", "Nightly",
                          resource_path.string()};
  const auto detail = LoadResourceDetail(summary);

  const auto run_hint = std::find_if(
      detail.field_hints.begin(), detail.field_hints.end(),
      [](const ResourceFieldHint& hint) { return hint.key == "Run"; });
  ASSERT_NE(run_hint, detail.field_hints.end());
  EXPECT_TRUE(run_hint->repeatable);
  EXPECT_EQ(run_hint->value,
            "Level=Full mon at 21:00\nLevel=Incremental tue at 21:00");

  for (const auto& message : detail.validation_messages) {
    EXPECT_NE(message.code, "duplicate-directive");
  }

  auto updated_field_hints = detail.field_hints;
  for (auto& field : updated_field_hints) {
    if (field.key == "Run") {
      field.value = "Level=Full mon at 21:00\nLevel=Differential wed at 21:00";
      field.present = true;
    }
  }

  const auto preview = BuildFieldHintEditPreview(detail, updated_field_hints);

  EXPECT_NE(preview.updated_content.find("Run = Level=Full mon at 21:00"),
            std::string::npos);
  EXPECT_NE(preview.updated_content.find("Run = Level=Differential wed at 21:00"),
            std::string::npos);
  EXPECT_EQ(
      preview.updated_content.find("Run = Level=Incremental tue at 21:00"),
      std::string::npos);
}

TEST(BareosConfigModel, NormalizesDirectiveNamesLikeParser)
{
  TempConfigRoot root;
  std::filesystem::create_directories(root.path() / "bareos-dir.d/fileset");

  const auto resource_path = root.path() / "bareos-dir.d/fileset/SelfTest.conf";
  std::ofstream(resource_path)
      << "FileSet {\n"
      << "  Name = SelfTest\n"
      << "  Enable VSS = No\n"
      << "}\n";

  ResourceSummary summary{"resource-1", "fileset", "SelfTest",
                          resource_path.string()};
  const auto detail = LoadResourceDetail(summary);

  const auto enable_vss_count = std::count_if(
      detail.field_hints.begin(), detail.field_hints.end(),
      [](const ResourceFieldHint& hint) { return hint.key == "EnableVSS"; });
  const auto spaced_enable_vss_count = std::count_if(
      detail.field_hints.begin(), detail.field_hints.end(),
      [](const ResourceFieldHint& hint) { return hint.key == "Enable VSS"; });

  EXPECT_EQ(enable_vss_count, 1);
  EXPECT_EQ(spaced_enable_vss_count, 0);

  const auto enable_vss = std::find_if(
      detail.field_hints.begin(), detail.field_hints.end(),
      [](const ResourceFieldHint& hint) { return hint.key == "EnableVSS"; });
  ASSERT_NE(enable_vss, detail.field_hints.end());
  EXPECT_EQ(enable_vss->datatype, "BOOLEAN");
  EXPECT_EQ(enable_vss->value, "no");
  EXPECT_EQ(enable_vss->default_value, "yes");
  EXPECT_EQ(enable_vss->allowed_values, (std::vector<std::string>{"yes", "no"}));

  auto updated_field_hints = detail.field_hints;
  for (auto& field : updated_field_hints) {
    if (field.key == "EnableVSS") {
      field.value = "yes";
      field.present = true;
    }
  }

  const auto preview = BuildFieldHintEditPreview(detail, updated_field_hints);
  EXPECT_EQ(preview.updated_content.find("EnableVSS = yes"), std::string::npos);
  EXPECT_NE(preview.updated_content.find("Enable VSS = yes"), std::string::npos);
}

TEST(BareosConfigModel, LimitsGuidedFieldsToTopLevelDirectives)
{
  TempConfigRoot root;
  std::filesystem::create_directories(root.path() / "bareos-dir.d/fileset");

  const auto resource_path = root.path() / "bareos-dir.d/fileset/SelfTest.conf";
  std::ofstream(resource_path)
      << "FileSet {\n"
      << "  Name = SelfTest\n"
      << "  Enable VSS = No\n"
      << "  Include {\n"
      << "    Options {\n"
      << "      Signature = XXH128\n"
      << "      HardLinks = Yes\n"
      << "    }\n"
      << "    File = /tmp/example\n"
      << "  }\n"
      << "}\n";

  ResourceSummary summary{"resource-1", "fileset", "SelfTest",
                          resource_path.string()};
  const auto detail = LoadResourceDetail(summary);

  EXPECT_TRUE(std::any_of(detail.directives.begin(), detail.directives.end(),
                          [](const ResourceDirective& directive) {
                            return directive.key == "Signature"
                                   && directive.nesting_level == 3;
                          }));
  EXPECT_TRUE(std::any_of(detail.directives.begin(), detail.directives.end(),
                          [](const ResourceDirective& directive) {
                            return directive.key == "File"
                                   && directive.nesting_level == 2;
                          }));

  EXPECT_FALSE(std::any_of(detail.field_hints.begin(), detail.field_hints.end(),
                           [](const ResourceFieldHint& hint) {
                             return hint.key == "Signature" || hint.key == "File"
                                    || hint.key == "HardLinks";
                           }));
  EXPECT_TRUE(std::any_of(detail.field_hints.begin(), detail.field_hints.end(),
                          [](const ResourceFieldHint& hint) {
                            return hint.key == "EnableVSS";
                          }));
}

TEST(BareosConfigModel, OrdersDisplayedDirectivesAndFieldHints)
{
  TempConfigRoot root;
  std::filesystem::create_directories(root.path() / "bareos-dir.d/client");

  const auto resource_path = root.path() / "bareos-dir.d/client/example-fd.conf";
  std::ofstream(resource_path)
      << "Client {\n"
      << "  Catalog = MyCatalog\n"
      << "  Password = secret\n"
      << "  Name = example-fd\n"
      << "  Address = 10.0.0.20\n"
      << "}\n";

  ResourceSummary summary{"resource-1", "client", "example-fd",
                          resource_path.string()};
  const auto detail = LoadResourceDetail(summary);

  ASSERT_GE(detail.directives.size(), 4U);
  EXPECT_EQ(detail.directives[0].key, "Name");
  EXPECT_EQ(detail.directives[1].key, "Address");
  EXPECT_EQ(detail.directives[2].key, "Password");
  EXPECT_EQ(detail.directives[3].key, "Catalog");

  ASSERT_GE(detail.field_hints.size(), 4U);
  const auto field_hint_position = [&](std::string_view key) {
    return std::distance(
        detail.field_hints.begin(),
        std::find_if(detail.field_hints.begin(), detail.field_hints.end(),
                     [&](const ResourceFieldHint& hint) { return hint.key == key; }));
  };
  EXPECT_EQ(detail.field_hints[0].key, "Name");
  EXPECT_EQ(field_hint_position("Address"), 1);
  EXPECT_LT(field_hint_position("Address"), field_hint_position("Password"));
  EXPECT_LT(field_hint_position("Password"), field_hint_position("AuthType"));
  EXPECT_LT(field_hint_position("AuthType"), field_hint_position("AutoPrune"));
  EXPECT_LT(field_hint_position("AutoPrune"), field_hint_position("Catalog"));
}
