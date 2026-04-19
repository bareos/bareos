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
#include "config_service.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>

#include <gtest/gtest.h>
#include <unistd.h>

namespace {
class TempConfigRoot {
 public:
  TempConfigRoot()
  {
    auto pattern = std::filesystem::temp_directory_path()
                   / "bareos-config-service-XXXXXX";
    auto mutable_path = pattern.string();
    path_ = mkdtemp(mutable_path.data());
    if (path_.empty()) throw std::runtime_error("mkdtemp failed");
  }

  ~TempConfigRoot() { std::filesystem::remove_all(path_); }

  const std::filesystem::path& path() const { return path_; }

 private:
  std::filesystem::path path_;
};

ConfigServiceOptions MakeServiceOptions(
    const std::filesystem::path& root,
    const std::string& bareos_dir_binary = "/bin/true",
    const std::string& bareos_fd_binary = "/bin/true",
    const std::string& bareos_sd_binary = "/bin/true")
{
  return {{root},
          bareos_dir_binary,
          bareos_fd_binary,
          bareos_sd_binary,
          root / "generated"};
}

std::vector<std::string> ExtractNodeIdsFromBootstrap(const std::string& body)
{
  std::vector<std::string> ids;
  const std::regex node_regex("\"id\":\"([^\"]+)\",\"kind\":\"");
  for (std::sregex_iterator it(body.begin(), body.end(), node_regex), end;
       it != end; ++it) {
    ids.push_back((*it)[1].str());
  }
  return ids;
}
}  // namespace

TEST(BareosConfigService, ReturnsEditableResourcePayload)
{
  TempConfigRoot root;
  std::filesystem::create_directories(root.path() / "bareos-dir.d/client");
  std::filesystem::create_directories(root.path() / "bareos-dir.d/catalog");
  std::ofstream(root.path() / "bareos-dir.conf") << "Director {}\n";
  std::ofstream(root.path() / "bareos-dir.d/catalog/MyCatalog.conf")
      << "Catalog {\n  Name = MyCatalog\n}\n";
  std::ofstream(root.path() / "bareos-dir.d/client/example-fd.conf")
      << "Client {\n  Name = example-fd\n  Catalog = MyCatalog\n}\n";

  const ConfigServiceOptions options = MakeServiceOptions(root.path());
  const HttpRequest request{
      "GET", "/api/v1/resources/resource-0-director-0/editor", "", ""};

  const auto response = HandleConfigServiceRequest(options, request);

  EXPECT_EQ(response.status_code, 200);
  EXPECT_NE(response.body.find("\"editable\":true"), std::string::npos);
  EXPECT_NE(response.body.find("\"save_mode\":\"dry-run\""), std::string::npos);
  EXPECT_NE(response.body.find("\"field_hints\":[{"), std::string::npos);
  EXPECT_NE(response.body.find("\"key\":\"Address\""), std::string::npos);
  EXPECT_NE(response.body.find("\"key\":\"Name\""), std::string::npos);
  EXPECT_NE(response.body.find("\"default_value\":"), std::string::npos);
  EXPECT_NE(response.body.find("\"related_resource_type\":\"catalog\""),
            std::string::npos);
  EXPECT_NE(response.body.find("\"allowed_values\":[\"MyCatalog\"]"),
            std::string::npos);
  EXPECT_NE(response.body.find("\"code\":\"missing-address\""), std::string::npos);
  EXPECT_NE(response.body.find("example-fd"), std::string::npos);
}

TEST(BareosConfigService, ReturnsInheritedJobDefsInEditablePayload)
{
  TempConfigRoot root;
  std::filesystem::create_directories(root.path() / "bareos-dir.d/job");
  std::filesystem::create_directories(root.path() / "bareos-dir.d/jobdefs");
  std::ofstream(root.path() / "bareos-dir.conf") << "Director {}\n";
  std::ofstream(root.path() / "bareos-dir.d/jobdefs/DefaultJob.conf")
      << "JobDefs {\n"
      << "  Name = DefaultJob\n"
      << "  Type = Backup\n"
      << "  Pool = Full\n"
      << "  Messages = Standard\n"
      << "}\n";
  std::ofstream(root.path() / "bareos-dir.d/job/example.conf")
      << "Job {\n"
      << "  Name = ExampleJob\n"
      << "  JobDefs = DefaultJob\n"
      << "  Client = example-fd\n"
      << "  FileSet = SelfTest\n"
      << "}\n";

  const ConfigServiceOptions options = MakeServiceOptions(root.path());
  const HttpRequest request{
      "GET", "/api/v1/resources/resource-0-director-0/editor", "", ""};

  const auto response = HandleConfigServiceRequest(options, request);

  EXPECT_EQ(response.status_code, 200);
  EXPECT_NE(response.body.find("\"inherited_directives\":[{"),
            std::string::npos);
  EXPECT_NE(response.body.find("\"source_resource_type\":\"jobdefs\""),
            std::string::npos);
  EXPECT_NE(response.body.find("\"source_resource_name\":\"DefaultJob\""),
            std::string::npos);
  EXPECT_NE(response.body.find("\"inherited\":true"), std::string::npos);
  EXPECT_NE(response.body.find("\"inherited_value\":\"Backup\""),
            std::string::npos);
  EXPECT_NE(response.body.find("\"inherited_source_resource_name\":\"DefaultJob\""),
            std::string::npos);
}

TEST(BareosConfigService, NormalizesQuotedProfileOptionsInEditablePayload)
{
  TempConfigRoot root;
  std::filesystem::create_directories(root.path() / "bareos-dir.d/console");
  std::filesystem::create_directories(root.path() / "bareos-dir.d/profile");
  std::ofstream(root.path() / "bareos-dir.conf") << "Director {}\n";
  std::ofstream(root.path() / "bareos-dir.d/profile/webui-admin.conf")
      << "Profile {\n  Name = \"webui-admin\"\n}\n";
  std::ofstream(root.path() / "bareos-dir.d/profile/webui-readonly.conf")
      << "Profile {\n  Name = \"webui-readonly\"\n}\n";
  std::ofstream(root.path() / "bareos-dir.d/console/admin.conf")
      << "Console {\n"
      << "  Name = admin\n"
      << "  Profile = \"webui-admin\"\n"
      << "}\n";

  const auto model = DiscoverDatacenterSummary({root.path()});
  const auto resource = std::find_if(
      model.directors[0].resources.begin(), model.directors[0].resources.end(),
      [](const ResourceSummary& summary) { return summary.type == "console"; });
  ASSERT_NE(resource, model.directors[0].resources.end());

  const ConfigServiceOptions options = MakeServiceOptions(root.path());
  const HttpRequest request{
      "GET", "/api/v1/resources/" + resource->id + "/editor", "", ""};

  const auto response = HandleConfigServiceRequest(options, request);

  EXPECT_EQ(response.status_code, 200);
  EXPECT_NE(
      response.body.find("\"allowed_values\":[\"webui-admin\",\"webui-readonly\"]"),
      std::string::npos);
  EXPECT_EQ(response.body.find("\"allowed_values\":[\"\\\"webui-admin\\\"\""),
            std::string::npos);
}

TEST(BareosConfigService, NormalizesQuotedRepeatableProfileOptionsInEditablePayload)
{
  TempConfigRoot root;
  std::filesystem::create_directories(root.path() / "bareos-dir.d/profile");
  std::filesystem::create_directories(root.path() / "bareos-dir.d/user");
  std::ofstream(root.path() / "bareos-dir.conf") << "Director {}\n";
  std::ofstream(root.path() / "bareos-dir.d/profile/webui-admin.conf")
      << "Profile {\n  Name = \"webui-admin\"\n}\n";
  std::ofstream(root.path() / "bareos-dir.d/profile/webui-readonly.conf")
      << "Profile {\n  Name = \"webui-readonly\"\n}\n";
  std::ofstream(root.path() / "bareos-dir.d/user/admin.conf")
      << "User {\n"
      << "  Name = admin\n"
      << "  Password = secret\n"
      << "  Profile = \"webui-admin\"\n"
      << "  Profile = webui-readonly\n"
      << "}\n";

  const auto model = DiscoverDatacenterSummary({root.path()});
  const auto resource = std::find_if(
      model.directors[0].resources.begin(), model.directors[0].resources.end(),
      [](const ResourceSummary& summary) { return summary.type == "user"; });
  ASSERT_NE(resource, model.directors[0].resources.end());

  const ConfigServiceOptions options = MakeServiceOptions(root.path());
  const HttpRequest request{
      "GET", "/api/v1/resources/" + resource->id + "/editor", "", ""};

  const auto response = HandleConfigServiceRequest(options, request);

  EXPECT_EQ(response.status_code, 200);
  EXPECT_NE(
      response.body.find("\"allowed_values\":[\"webui-admin\",\"webui-readonly\"]"),
      std::string::npos);
  EXPECT_EQ(response.body.find(
                "\"allowed_values\":[\"\\\"webui-admin\\\"\\nwebui-readonly\"]"),
            std::string::npos);
}

TEST(BareosConfigService, ReturnsNewResourceEditorPayload)
{
  TempConfigRoot root;
  std::filesystem::create_directories(root.path() / "bareos-dir.d/client");
  std::ofstream(root.path() / "bareos-dir.conf") << "Director {}\n";
  std::ofstream(root.path() / "bareos-dir.d/client/example-fd.conf")
      << "Client {\n  Name = example-fd\n}\n";

  const ConfigServiceOptions options = MakeServiceOptions(root.path());
  const HttpRequest request{
      "GET", "/api/v1/nodes/director-0/new-resource/client/editor", "", ""};

  const auto response = HandleConfigServiceRequest(options, request);

  EXPECT_EQ(response.status_code, 200);
  EXPECT_NE(response.body.find("\"editable\":true"), std::string::npos);
  EXPECT_NE(response.body.find("\"type\":\"client\""), std::string::npos);
  EXPECT_NE(response.body.find("new-client.conf"), std::string::npos);
  EXPECT_NE(response.body.find("Client {\\n}\\n"), std::string::npos);
}

TEST(BareosConfigService, PreviewsRepeatableScheduleRunFields)
{
  TempConfigRoot root;
  std::filesystem::create_directories(root.path() / "bareos-dir.d/client");
  std::ofstream(root.path() / "bareos-dir.conf") << "Director {}\n";
  std::ofstream(root.path() / "bareos-dir.d/client/example-fd.conf")
      << "Client {\n  Name = example-fd\n}\n";

  const ConfigServiceOptions options = MakeServiceOptions(root.path());
  const HttpRequest request{
      "POST",
      "/api/v1/nodes/director-0/new-resource/schedule/preview-fields",
      "text/plain; charset=utf-8",
      "Name=Nightly\nRun=Level=Full mon at 21:00\nRun=Level=Incremental tue at 21:00\n"};

  const auto response = HandleConfigServiceRequest(options, request);

  EXPECT_EQ(response.status_code, 200);
  EXPECT_NE(response.body.find("\"key\":\"Run\""), std::string::npos);
  EXPECT_NE(response.body.find("\"repeatable\":true"), std::string::npos);
  EXPECT_NE(
      response.body.find("Run = Level=Full mon at 21:00\\n  Run = Level=Incremental tue at 21:00"),
      std::string::npos);
}

TEST(BareosConfigService, ReturnsScheduleRunOverrideRelationships)
{
  TempConfigRoot root;
  std::filesystem::create_directories(root.path() / "bareos-dir.d/schedule");
  std::filesystem::create_directories(root.path() / "bareos-dir.d/pool");
  std::filesystem::create_directories(root.path() / "bareos-dir.d/storage");
  std::filesystem::create_directories(root.path() / "bareos-dir.d/messages");
  std::ofstream(root.path() / "bareos-dir.conf") << "Director {}\n";
  std::ofstream(root.path() / "bareos-dir.d/pool/full.conf")
      << "Pool {\n  Name = FullPool\n}\n";
  std::ofstream(root.path() / "bareos-dir.d/storage/file.conf")
      << "Storage {\n  Name = FileStorage\n}\n";
  std::ofstream(root.path() / "bareos-dir.d/messages/standard.conf")
      << "Messages {\n  Name = Standard\n}\n";
  std::ofstream(root.path() / "bareos-dir.d/schedule/Nightly.conf")
      << "Schedule {\n"
      << "  Name = Nightly\n"
      << "  Run = Incremental FullPool=FullPool Storage=FileStorage Messages=Standard mon at 21:00\n"
      << "}\n";

  const ConfigServiceOptions options = MakeServiceOptions(root.path());
  const HttpRequest request{
      "GET", "/api/v1/nodes/director-0/relationships", "", ""};

  const auto response = HandleConfigServiceRequest(options, request);

  EXPECT_EQ(response.status_code, 200);
  EXPECT_NE(response.body.find("\"source_resource_type\":\"schedule\""),
            std::string::npos);
  EXPECT_NE(response.body.find("\"source_resource_name\":\"Nightly\""),
            std::string::npos);
  EXPECT_NE(response.body.find("\"target_resource_name\":\"FullPool\""),
            std::string::npos);
  EXPECT_NE(response.body.find("\"target_resource_name\":\"FileStorage\""),
            std::string::npos);
  EXPECT_NE(response.body.find("\"target_resource_name\":\"Standard\""),
            std::string::npos);
  EXPECT_NE(response.body.find("Directive FullPool in schedule Nightly"),
            std::string::npos);
  EXPECT_NE(response.body.find("Directive Storage in schedule Nightly"),
            std::string::npos);
  EXPECT_NE(response.body.find("Directive Messages in schedule Nightly"),
            std::string::npos);
}

TEST(BareosConfigService, ExposesCreatableStorageDaemonResourceTypes)
{
  TempConfigRoot root;
  std::filesystem::create_directories(root.path() / "bareos-dir.d/storage");
  std::filesystem::create_directories(root.path() / "bareos-sd.d/storage");
  std::filesystem::create_directories(root.path() / "bareos-sd.d/device");
  std::ofstream(root.path() / "bareos-dir.conf") << "Director {}\n";
  std::ofstream(root.path() / "bareos-sd.conf")
      << "Storage {\n  Name = example-sd\n}\n";
  std::ofstream(root.path() / "bareos-dir.d/storage/example-sd.conf")
      << "Storage {\n  Name = example-sd\n}\n";
  std::ofstream(root.path() / "bareos-sd.d/device/tape.conf")
      << "Device {\n  Name = tape-drive-0\n}\n";

  const ConfigServiceOptions options = MakeServiceOptions(root.path());
  const HttpRequest request{"GET", "/api/v1/nodes/daemon-0-storage-daemon", "", ""};

  const auto response = HandleConfigServiceRequest(options, request);

  EXPECT_EQ(response.status_code, 200);
  EXPECT_NE(response.body.find("\"creatable_resource_types\":["),
            std::string::npos);
  EXPECT_NE(response.body.find("\"autochanger\""), std::string::npos);
  EXPECT_NE(response.body.find("\"device\""), std::string::npos);
  EXPECT_NE(response.body.find("\"storage\""), std::string::npos);
}

TEST(BareosConfigService, ReturnsConsoleNodeAndResources)
{
  TempConfigRoot root;
  std::filesystem::create_directories(root.path() / "bareos-dir.d/client");
  std::filesystem::create_directories(root.path() / "bconsole.d/console");
  std::ofstream(root.path() / "bareos-dir.conf") << "Director {}\n";
  std::ofstream(root.path() / "bconsole.conf")
      << "Director {\n  Name = bareos-dir\n}\n"
      << "Console {\n  Name = admin\n}\n";
  std::ofstream(root.path() / "bconsole.d/console/admin.conf")
      << "Console {\n  Name = admin\n}\n";

  const ConfigServiceOptions options = MakeServiceOptions(root.path());
  const auto bootstrap
      = HandleConfigServiceRequest(options, {"GET", "/api/v1/bootstrap", "", ""});
  ASSERT_EQ(bootstrap.status_code, 200);
  EXPECT_NE(bootstrap.body.find("\"id\":\"daemon-0-console\""), std::string::npos);
  EXPECT_NE(bootstrap.body.find("\"kind\":\"console\""), std::string::npos);

  const HttpRequest request{"GET", "/api/v1/nodes/daemon-0-console/resources", "", ""};
  const auto response = HandleConfigServiceRequest(options, request);

  EXPECT_EQ(response.status_code, 200);
  EXPECT_NE(response.body.find("\"type\":\"configuration\""), std::string::npos);
  EXPECT_NE(response.body.find("\"type\":\"console\""), std::string::npos);
  EXPECT_NE(response.body.find((root.path() / "bconsole.conf").string()),
            std::string::npos);
}

TEST(BareosConfigService, ReturnsConsoleRelationships)
{
  TempConfigRoot root;
  std::filesystem::create_directories(root.path() / "bareos-dir.d/director");
  std::filesystem::create_directories(root.path() / "bareos-dir.d/console");
  std::filesystem::create_directories(root.path() / "bconsole.d/console");
  std::ofstream(root.path() / "bareos-dir.conf") << "Director {}\n";
  std::ofstream(root.path() / "bareos-dir.d/director/bareos-dir.conf")
      << "Director {\n  Name = bareos-dir\n}\n";
  const auto director_console_path = root.path() / "bareos-dir.d/console/admin.conf";
  std::ofstream(director_console_path)
      << "Console {\n  Name = admin\n  Password = secret\n}\n";
  std::ofstream(root.path() / "bconsole.conf")
      << "Director {\n  Name = bareos-dir\n}\n"
      << "Console {\n  Name = admin\n}\n";
  const auto bconsole_console_path = root.path() / "bconsole.d/console/admin.conf";
  std::ofstream(bconsole_console_path)
      << "Console {\n  Name = admin\n  Password = secret\n}\n";

  const ConfigServiceOptions options = MakeServiceOptions(root.path());
  const HttpRequest request{
      "GET", "/api/v1/nodes/daemon-0-console/relationships", "", ""};

  const auto response = HandleConfigServiceRequest(options, request);

  EXPECT_EQ(response.status_code, 200);
  EXPECT_NE(response.body.find("\"relation\":\"resource-name\""), std::string::npos);
  EXPECT_NE(response.body.find("\"relation\":\"shared-password\""),
            std::string::npos);
  EXPECT_NE(response.body.find("\"from_node_id\":\"daemon-0-console\""),
            std::string::npos);
  EXPECT_NE(response.body.find("\"to_node_id\":\"director-0\""),
            std::string::npos);
  EXPECT_NE(response.body.find("\"endpoint_name\":\"bareos-dir\""),
            std::string::npos);
  EXPECT_NE(response.body.find("\"endpoint_name\":\"admin\""), std::string::npos);
  EXPECT_NE(response.body.find(bconsole_console_path.string()), std::string::npos);
  EXPECT_NE(response.body.find(director_console_path.string()), std::string::npos);
}

TEST(BareosConfigService, ReturnsRootConsoleAuthenticationRelationships)
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

  const ConfigServiceOptions options = MakeServiceOptions(root.path());
  const HttpRequest request{
      "GET", "/api/v1/nodes/daemon-0-console/relationships", "", ""};

  const auto response = HandleConfigServiceRequest(options, request);

  EXPECT_EQ(response.status_code, 200);
  EXPECT_NE(response.body.find("\"relation\":\"resource-name\""), std::string::npos);
  EXPECT_NE(response.body.find("\"relation\":\"shared-password\""),
            std::string::npos);
  EXPECT_NE(response.body.find("\"source_resource_type\":\"director\""),
            std::string::npos);
  EXPECT_NE(response.body.find(bconsole_path.string()), std::string::npos);
  EXPECT_NE(response.body.find(director_path.string()), std::string::npos);
}

TEST(BareosConfigService, LoadsResourcesAndRelationshipsForAllBootstrapNodes)
{
  TempConfigRoot root;
  std::filesystem::create_directories(root.path() / "bareos-dir.d/client");
  std::filesystem::create_directories(root.path() / "bareos-dir.d/director");
  std::filesystem::create_directories(root.path() / "bareos-dir.d/job");
  std::filesystem::create_directories(root.path() / "bareos-fd.d/client");
  std::filesystem::create_directories(root.path() / "bareos-fd.d/director");
  std::filesystem::create_directories(root.path() / "bareos-sd.d/director");
  std::filesystem::create_directories(root.path() / "bconsole.d/console");
  std::ofstream(root.path() / "bareos-dir.conf")
      << "Director {\n  Name = bareos-dir\n}\n";
  std::ofstream(root.path() / "bareos-fd.conf")
      << "FileDaemon {\n  Name = example-fd\n}\n";
  std::ofstream(root.path() / "bareos-sd.conf")
      << "Storage {\n  Name = example-sd\n}\n";
  std::ofstream(root.path() / "bconsole.conf")
      << "Director {\n  Name = bareos-dir\n}\n"
      << "Console {\n  Name = bconsole\n}\n";
  std::ofstream(root.path() / "bareos-dir.d/director/bareos-dir.conf")
      << "Director {\n  Name = bareos-dir\n}\n";
  std::ofstream(root.path() / "bareos-dir.d/client/example-fd.conf")
      << "Client {\n  Name = example-fd\n}\n";
  std::ofstream(root.path() / "bareos-dir.d/client/remote-fd.conf")
      << "Client {\n  Name = remote-fd\n}\n";
  std::ofstream(root.path() / "bareos-dir.d/job/backup.conf")
      << "Job {\n  Name = BackupClient\n  Client = example-fd\n}\n";
  std::ofstream(root.path() / "bareos-fd.d/client/myself.conf")
      << "Client {\n  Name = example-fd\n}\n";
  std::ofstream(root.path() / "bareos-fd.d/director/bareos-dir.conf")
      << "Director {\n  Name = bareos-dir\n}\n";
  std::ofstream(root.path() / "bareos-sd.d/director/bareos-dir.conf")
      << "Director {\n  Name = bareos-dir\n}\n";
  std::ofstream(root.path() / "bconsole.d/console/admin.conf")
      << "Console {\n  Name = admin\n}\n";

  const auto generated_path = root.path()
                              / "generated/clients/remote-fd/etc/bareos/bareos-fd.d/director/bareos-dir.conf";
  std::filesystem::create_directories(generated_path.parent_path());
  std::ofstream(generated_path)
      << "Director {\n  Name = bareos-dir\n}\n";

  const ConfigServiceOptions options = MakeServiceOptions(root.path());
  const auto bootstrap
      = HandleConfigServiceRequest(options, {"GET", "/api/v1/bootstrap", "", ""});
  ASSERT_EQ(bootstrap.status_code, 200);

  const auto node_ids = ExtractNodeIdsFromBootstrap(bootstrap.body);
  ASSERT_FALSE(node_ids.empty());

  for (const auto& node_id : node_ids) {
    const auto resources = HandleConfigServiceRequest(
        options, {"GET", "/api/v1/nodes/" + node_id + "/resources", "", ""});
    EXPECT_EQ(resources.status_code, 200) << node_id << "\n" << resources.body;
    EXPECT_EQ(resources.mime_type, "application/json; charset=utf-8");

    const auto relationships = HandleConfigServiceRequest(
        options, {"GET", "/api/v1/nodes/" + node_id + "/relationships", "", ""});
    EXPECT_EQ(relationships.status_code, 200)
        << node_id << "\n" << relationships.body;
    EXPECT_EQ(relationships.mime_type, "application/json; charset=utf-8");
  }
}

TEST(BareosConfigService, ReturnsGenericReferenceAndSharedPasswordRelationships)
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
  const auto client_path = root.path() / "bareos-dir.d/client/example-fd.conf";
  const auto fileset_path = root.path() / "bareos-dir.d/fileset/SelfTest.conf";
  const auto job_path = root.path() / "bareos-dir.d/job/example-job.conf";
  const auto console_path = root.path() / "bconsole.d/console/admin.conf";
  const auto user_path = root.path() / "bconsole.d/user/api.conf";
  std::ofstream(root.path() / "bareos-dir.d/director/bareos-dir.conf")
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
  std::ofstream(root.path() / "bareos-dir.d/storage/example-sd.conf")
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

  const ConfigServiceOptions options = MakeServiceOptions(root.path());
  const HttpRequest request{
      "GET", "/api/v1/nodes/director-0/relationships", "", ""};

  const auto response = HandleConfigServiceRequest(options, request);

  EXPECT_EQ(response.status_code, 200);
  EXPECT_NE(response.body.find("\"relation\":\"resource-name\""), std::string::npos);
  EXPECT_NE(response.body.find("\"relation\":\"shared-password\""),
            std::string::npos);
  EXPECT_NE(response.body.find("\"endpoint_name\":\"example-fd\""),
            std::string::npos);
  EXPECT_NE(response.body.find("\"endpoint_name\":\"SelfTest\""),
            std::string::npos);
  EXPECT_NE(response.body.find(client_path.string()), std::string::npos);
  EXPECT_NE(response.body.find(fileset_path.string()), std::string::npos);
  EXPECT_EQ(response.body.find(console_path.string()), std::string::npos);
  EXPECT_EQ(response.body.find(user_path.string()), std::string::npos);
}

TEST(BareosConfigService, KeepsMessagesRelationshipsScopedToOwningDaemon)
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

  const ConfigServiceOptions options = MakeServiceOptions(root.path());
  const HttpRequest request{
      "GET", "/api/v1/nodes/daemon-0-file-daemon/relationships", "", ""};

  const auto response = HandleConfigServiceRequest(options, request);

  EXPECT_EQ(response.status_code, 200);
  EXPECT_NE(response.body.find(file_daemon_messages_path.string()), std::string::npos);
  EXPECT_EQ(response.body.find(director_messages_path.string()), std::string::npos);
}

TEST(BareosConfigService, ReturnsNewAutochangerEditorPayloadForStorageDaemon)
{
  TempConfigRoot root;
  std::filesystem::create_directories(root.path() / "bareos-dir.d/storage");
  std::filesystem::create_directories(root.path() / "bareos-sd.d/device");
  std::ofstream(root.path() / "bareos-dir.conf") << "Director {}\n";
  std::ofstream(root.path() / "bareos-sd.conf")
      << "Storage {\n  Name = example-sd\n}\n";
  std::ofstream(root.path() / "bareos-dir.d/storage/example-sd.conf")
      << "Storage {\n  Name = example-sd\n}\n";
  std::ofstream(root.path() / "bareos-sd.d/device/tape.conf")
      << "Device {\n  Name = tape-drive-0\n}\n";

  const ConfigServiceOptions options = MakeServiceOptions(root.path());
  const HttpRequest request{
      "GET",
      "/api/v1/nodes/daemon-0-storage-daemon/new-resource/autochanger/editor",
      "",
      ""};

  const auto response = HandleConfigServiceRequest(options, request);

  EXPECT_EQ(response.status_code, 200);
  EXPECT_NE(response.body.find("\"type\":\"autochanger\""),
            std::string::npos);
  EXPECT_NE(response.body.find("new-autochanger.conf"), std::string::npos);
  EXPECT_NE(response.body.find("Autochanger {\\n}\\n"), std::string::npos);
}

TEST(BareosConfigService, ServesDatacenterScopedWizardUi)
{
  const ConfigServiceOptions options{{}};
  const HttpRequest request{"GET", "/", "", ""};

  const auto response = HandleConfigServiceRequest(options, request);

  EXPECT_EQ(response.status_code, 200);
  EXPECT_NE(response.body.find("Target director"), std::string::npos);
  EXPECT_NE(response.body.find("node.kind === 'datacenter'"),
            std::string::npos);
  EXPECT_NE(response.body.find("loadNodeById(node.id);"), std::string::npos);
  EXPECT_NE(response.body.find("grid-template-columns: 420px 1fr"),
            std::string::npos);
  EXPECT_NE(response.body.find("--setup-primary: #0075be;"),
            std::string::npos);
  EXPECT_NE(response.body.find("--setup-accent: #f5a623;"),
            std::string::npos);
  EXPECT_NE(response.body.find("background: linear-gradient(180deg, #eef6fb 0%, #f5f7fa 160px);"),
            std::string::npos);
  EXPECT_NE(response.body.find("border-radius: 16px;"),
            std::string::npos);
  EXPECT_EQ(response.body.find("<h3>Resources</h3>"), std::string::npos);
  EXPECT_NE(response.body.find("Resources are shown in the tree on the left"),
            std::string::npos);
  EXPECT_NE(response.body.find("const expandedTreeIds = new Set(["),
            std::string::npos);
  EXPECT_NE(response.body.find("function iconMarkup(name, options = {})"),
            std::string::npos);
  EXPECT_NE(response.body.find("function iconForGroup(groupId)"),
            std::string::npos);
  EXPECT_NE(response.body.find("function iconForNode(node)"),
            std::string::npos);
  EXPECT_NE(response.body.find("function iconForResource(resource)"),
            std::string::npos);
  EXPECT_NE(response.body.find("function clientManagementState(node)"),
            std::string::npos);
  EXPECT_NE(response.body.find("function renderClientManagementBadge(node)"),
            std::string::npos);
  EXPECT_NE(response.body.find("class=\"app-title\""),
            std::string::npos);
  EXPECT_NE(response.body.find("class=\"section-heading\""),
            std::string::npos);
  EXPECT_NE(response.body.find(".tree-status-badge.managed"),
            std::string::npos);
  EXPECT_NE(response.body.find(".tree-status-badge.remote"),
            std::string::npos);
  EXPECT_NE(response.body.find("label: 'Directors'"), std::string::npos);
  EXPECT_NE(response.body.find("label: 'Storages'"), std::string::npos);
  EXPECT_NE(response.body.find("label: 'Clients'"), std::string::npos);
  EXPECT_NE(response.body.find("label: 'Consoles'"), std::string::npos);
  EXPECT_NE(response.body.find("function groupedTreeEntries(root)"),
            std::string::npos);
  EXPECT_NE(response.body.find("function normalizeNodeLabelForComparison(label)"),
            std::string::npos);
  EXPECT_NE(response.body.find("function deduplicateClientGroupNodes(nodes)"),
            std::string::npos);
  EXPECT_NE(response.body.find("group.id === 'tree-group-clients'"),
            std::string::npos);
  EXPECT_NE(response.body.find("Management: ${escapeHtml(managementState.description)}<br>"),
            std::string::npos);
  EXPECT_NE(response.body.find("function renderGroup(group, parentEl)"),
            std::string::npos);
  EXPECT_NE(response.body.find("function renderDatacenterTree(root, parentEl)"),
            std::string::npos);
  EXPECT_NE(response.body.find("const nodeExpansionId = `tree-node-${node.id}`;"),
            std::string::npos);
  EXPECT_NE(response.body.find("isEffectivelyEmptyDirectiveValue"),
            std::string::npos);
  EXPECT_NE(response.body.find("hideEmptyOptional: true"),
            std::string::npos);
  EXPECT_NE(response.body.find("allowEmptyOptionalToggle: true"),
            std::string::npos);
  EXPECT_NE(response.body.find("textarea.repeatable-field"),
            std::string::npos);
  EXPECT_NE(response.body.find("select[multiple]"),
            std::string::npos);
  EXPECT_NE(response.body.find("function renderRelationshipGraph(relationships, options = {})"),
            std::string::npos);
  EXPECT_NE(response.body.find("class=\"relationship-view\""),
            std::string::npos);
  EXPECT_NE(response.body.find("Internal relations graph"), std::string::npos);
  EXPECT_NE(response.body.find("Relations between entities graph"),
            std::string::npos);
  EXPECT_NE(response.body.find("Each daemon or director definition appears once as an object."),
            std::string::npos);
  EXPECT_NE(response.body.find("This graph shows only relations that stay inside the same entity"),
            std::string::npos);
  EXPECT_NE(response.body.find("This graph shows only relations that connect one entity"),
            std::string::npos);
  EXPECT_NE(response.body.find("the arrows show which resource links to which other resource"),
            std::string::npos);
  EXPECT_NE(response.body.find("function isInternalRelationship(relationship)"),
            std::string::npos);
  EXPECT_NE(response.body.find("path.includes('/bareos-sd.d/')"),
            std::string::npos);
  EXPECT_NE(response.body.find("splitObjectResources: true"),
            std::string::npos);
  EXPECT_NE(response.body.find("Resources are distributed into two lanes: resources with outgoing relations stay on the left"),
            std::string::npos);
  EXPECT_NE(response.body.find("const normalizedTypeLabel = (resource) =>"),
            std::string::npos);
  EXPECT_NE(response.body.find("const typeOrder = normalizedTypeLabel(left).localeCompare(normalizedTypeLabel(right));"),
            std::string::npos);
  EXPECT_NE(response.body.find("const splitLaneCount = splitObjectResources ? 2 : 1;"),
            std::string::npos);
  EXPECT_NE(response.body.find("const preferredLaneForResource = (resource) => {"),
            std::string::npos);
  EXPECT_NE(response.body.find("if (resource.outgoing > 0) return 0;"),
            std::string::npos);
  EXPECT_NE(response.body.find("const laneOrderForResource = (resource) => {"),
            std::string::npos);
  EXPECT_NE(response.body.find("const laneResourcesForObject = (object, lane) =>"),
            std::string::npos);
  EXPECT_NE(response.body.find("object.resourceLanes = Array.from("),
            std::string::npos);
  EXPECT_NE(response.body.find("const laneOffset = splitObjectResources"),
            std::string::npos);
  EXPECT_NE(response.body.find("const resourceBoxHeight = 23;"),
            std::string::npos);
  EXPECT_NE(response.body.find("const resourceRowHeight = resourceBoxHeight * 2;"),
            std::string::npos);
  EXPECT_NE(response.body.find("const resourceLaneGap = splitObjectResources"),
            std::string::npos);
  EXPECT_NE(response.body.find("? (resourceBoxHeight + objectPaddingSide) * 3.75"),
            std::string::npos);
  EXPECT_NE(response.body.find("const outerColumnLinkMargin = 96;"),
            std::string::npos);
  EXPECT_NE(response.body.find("const outerColumnLinkPadding = outerColumnLinkMargin + 64;"),
            std::string::npos);
  EXPECT_NE(response.body.find("const graphPaddingLeft = graphPaddingX + outerColumnLinkPadding;"),
            std::string::npos);
  EXPECT_NE(response.body.find("const rowTop = (resource.y - object.y) - (resourceBoxHeight / 2);"),
            std::string::npos);
  EXPECT_NE(response.body.find("overflow: visible;"),
            std::string::npos);
  EXPECT_NE(response.body.find("function attachRelationshipGraphHover()"),
            std::string::npos);
  EXPECT_NE(response.body.find("class=\"relationship-graph-object\""),
            std::string::npos);
  EXPECT_NE(response.body.find("class=\"relationship-graph-edge\""),
            std::string::npos);
  EXPECT_NE(response.body.find("${objectResourceMarkup}\n            ${edgeMarkup}"),
            std::string::npos);
  EXPECT_NE(response.body.find("class=\"relationship-graph-edge-source\""),
            std::string::npos);
  EXPECT_NE(response.body.find("orient=\"auto\""),
            std::string::npos);
  EXPECT_NE(response.body.find("function relationshipDirectiveLabel(relationship)"),
            std::string::npos);
  EXPECT_NE(response.body.find("const referencedType = `${relationship?.target_resource_type || ''}`.trim().toLowerCase();"),
            std::string::npos);
  EXPECT_NE(response.body.find("if (!normalizedLabel || normalizedLabel.includes('-')) return '';"),
            std::string::npos);
  EXPECT_NE(response.body.find("return normalizedLabel.toLowerCase() === referencedType ? '' : normalizedLabel;"),
            std::string::npos);
  EXPECT_NE(response.body.find("const rawDirectiveLabel = relationshipDirectiveLabel(relationship);"),
            std::string::npos);
  EXPECT_NE(response.body.find("class=\"relationship-graph-edge-label\""),
            std::string::npos);
  EXPECT_NE(response.body.find("Directive (.+?) in "),
            std::string::npos);
  EXPECT_NE(response.body.find("const sameColumn = !sameObject && sourceObject.column === targetObject.column;"),
            std::string::npos);
  EXPECT_NE(response.body.find("const routeOnLeft = sourceObject.column === 0;"),
            std::string::npos);
  EXPECT_NE(response.body.find("const lanePortX = (resource, direction) =>"),
            std::string::npos);
  EXPECT_NE(response.body.find("const sameResource = sourceResource.key === targetResource.key;"),
            std::string::npos);
  EXPECT_NE(response.body.find("const sameLaneLoopDirection = (lane) => (lane === 0 ? -1 : 1);"),
            std::string::npos);
  EXPECT_NE(response.body.find("const laneBoundaryX = (object, leftLane) =>"),
            std::string::npos);
  EXPECT_NE(response.body.find("laneResources.sort((left, right) => {"),
            std::string::npos);
  EXPECT_NE(response.body.find("const boundaryX = laneBoundaryX("),
            std::string::npos);
  EXPECT_NE(response.body.find("const selfLoopHeight = resourceRowHeight + Math.abs(laneOffset);"),
            std::string::npos);
  EXPECT_NE(response.body.find("const selfLoopEndY = startY + 14;"),
            std::string::npos);
  EXPECT_NE(response.body.find("const selfLoopApproachY = selfLoopEndY - 10;"),
            std::string::npos);
  EXPECT_NE(response.body.find("const selfLoopLabelY = startY + ((selfLoopEndY - startY) / 2) - 2;"),
            std::string::npos);
  EXPECT_NE(response.body.find("? sourceObject.x - (outerColumnLinkMargin / 2) - Math.abs(laneOffset)"),
            std::string::npos);
  EXPECT_NE(response.body.find("const branchX = routeOnLeft ? columnStartX - 26 : columnStartX + 26;"),
            std::string::npos);
  EXPECT_NE(response.body.find("const graphEdgeDataAttributes = ("),
            std::string::npos);
  EXPECT_NE(response.body.find("const graphEdgeTitle = (relationship) =>"),
            std::string::npos);
  EXPECT_NE(response.body.find("const graphEndApproachY = (startY, endY) => {"),
            std::string::npos);
  EXPECT_NE(response.body.find("const renderRelationshipLegend = (entries) => `"),
            std::string::npos);
  EXPECT_NE(response.body.find("C ${outerX} ${endY}, ${branchEndX} ${endApproachY}, ${columnEndX} ${endY}"),
            std::string::npos);
  EXPECT_NE(response.body.find("C ${loopX} ${endY}, ${branchEndX} ${endApproachY}, ${sameLaneEndX} ${endY}"),
            std::string::npos);
  EXPECT_NE(response.body.find("has-active-highlight"),
            std::string::npos);
  EXPECT_NE(response.body.find("Jump to resolved node"),
            std::string::npos);
  EXPECT_NE(response.body.find("size=\"${Math.min(Math.max(optionValues.length, 2), 4)}\""),
            std::string::npos);
  EXPECT_NE(response.body.find("rows=\"2\""),
            std::string::npos);
  EXPECT_NE(response.body.find("stripSurroundingQuotes"),
            std::string::npos);
  EXPECT_NE(response.body.find("escapeHtml(stripSurroundingQuotes(resource.name))"),
            std::string::npos);
  EXPECT_NE(response.body.find("if (node.kind === 'datacenter') {"),
            std::string::npos);
  EXPECT_NE(response.body.find("return [];"),
            std::string::npos);
  EXPECT_NE(response.body.find("renderInheritedDirectives"),
            std::string::npos);
  EXPECT_NE(response.body.find("updated_inherited_directives"),
            std::string::npos);
  EXPECT_EQ(response.body.find("<h4>Parsed fields</h4>"), std::string::npos);
  EXPECT_EQ(response.body.find("<h5>Parsed fields</h5>"), std::string::npos);
  EXPECT_NE(response.body.find("const sortedResources = [...resources].sort"),
            std::string::npos);
  EXPECT_NE(response.body.find("const preferredResourceTypeByKind = {"),
            std::string::npos);
  EXPECT_NE(response.body.find("director: 'director'"),
            std::string::npos);
  EXPECT_NE(response.body.find("'file-daemon': 'client'"),
            std::string::npos);
  EXPECT_NE(response.body.find("'storage-daemon': 'storage'"),
            std::string::npos);
  EXPECT_NE(response.body.find("const preferredResourceType = preferredResourceTypeByKind[node.kind] || '';"),
            std::string::npos);
  EXPECT_NE(response.body.find("stripSurroundingQuotes(left.name || '') === normalizedNodeLabel"),
            std::string::npos);
  EXPECT_NE(response.body.find("left.type.localeCompare(right.type)"),
            std::string::npos);
  EXPECT_NE(response.body.find("const groupedResources = sortedResources.reduce"),
            std::string::npos);
  EXPECT_NE(response.body.find("Object.entries(groupedResources).map"),
            std::string::npos);
  EXPECT_NE(response.body.find("Inherited value: ${escapeHtml(stripSurroundingQuotes(field.inherited_value))}"),
            std::string::npos);
  EXPECT_NE(response.body.find("inherited from ${escapeHtml(field.inherited_source_resource_type)}"),
            std::string::npos);
  EXPECT_NE(response.body.find("restoreQuotedFieldValue"),
            std::string::npos);
  EXPECT_NE(response.body.find("data-field-quote-char"),
            std::string::npos);
  EXPECT_NE(response.body.find("renderRenameImpacts"),
            std::string::npos);
  EXPECT_NE(response.body.find("resource-rename-impacts"),
            std::string::npos);
  EXPECT_NE(response.body.find("<th>Target resource</th>"),
            std::string::npos);
  EXPECT_NE(response.body.find("field.default_value ? ` · default ${escapeHtml(stripSurroundingQuotes(field.default_value))}` : ''"),
            std::string::npos);
  EXPECT_NE(response.body.find("field.present && !isEffectivelyEmptyDirectiveValue(field.value)"),
            std::string::npos);
  EXPECT_NE(response.body.find("? ['', ...field.allowed_values]"),
            std::string::npos);
  EXPECT_NE(response.body.find("Show empty and unset fields"),
            std::string::npos);
  EXPECT_NE(response.body.find("Hide empty and unset fields"),
            std::string::npos);
  EXPECT_NE(response.body.find("synchronizeFieldHintsFromEditor"),
            std::string::npos);
  EXPECT_NE(response.body.find("splitRepeatableFieldValues"),
            std::string::npos);
  EXPECT_NE(response.body.find("detectRepeatableFieldQuote"),
            std::string::npos);
  EXPECT_NE(response.body.find("normalizeRepeatableFieldValues"),
            std::string::npos);
  EXPECT_NE(response.body.find(".map((entry) => stripSurroundingQuotes(entry))"),
            std::string::npos);
  EXPECT_NE(response.body.find("getRepeatableFieldValues"),
            std::string::npos);
  EXPECT_NE(response.body.find(".relationship-graph-resource[data-resource-id]"),
            std::string::npos);
  EXPECT_NE(response.body.find(
                "detailsEl.querySelectorAll('.relationship-graph-resource[data-resource-id]')"),
            std::string::npos);
  EXPECT_NE(response.body.find("function isInternalRelationship(relationship) {"),
            std::string::npos);
  EXPECT_NE(response.body.find("Internal relations"), std::string::npos);
  EXPECT_NE(response.body.find("Relations between entities"),
            std::string::npos);
  EXPECT_NE(response.body.find("No internal relations discovered for this node."),
            std::string::npos);
  EXPECT_NE(response.body.find("No cross-entity relations discovered for this node."),
            std::string::npos);
  EXPECT_NE(response.body.find("Block colors"), std::string::npos);
  EXPECT_NE(response.body.find("resource with outgoing relations"),
            std::string::npos);
  EXPECT_NE(response.body.find("resource with incoming and outgoing relations"),
            std::string::npos);
  EXPECT_NE(response.body.find("resource mainly targeted by relations"),
            std::string::npos);
  EXPECT_NE(response.body.find("unresolved object"), std::string::npos);
  EXPECT_NE(response.body.find("Arrow colors"), std::string::npos);
  EXPECT_NE(response.body.find("field.datatype === 'RESOURCE_LIST'"),
            std::string::npos);
  EXPECT_NE(response.body.find("multiple"),
            std::string::npos);
  EXPECT_NE(response.body.find("if (!payload.saved) {"),
            std::string::npos);
  EXPECT_NE(response.body.find("return fetch('/api/v1/bootstrap')"),
            std::string::npos);
  EXPECT_NE(response.body.find("renderTree(data);"),
            std::string::npos);
  EXPECT_NE(response.body.find("loadNodeById('datacenter');"),
            std::string::npos);
  EXPECT_NE(response.body.find("No non-empty directive fields parsed yet."),
            std::string::npos);
  EXPECT_NE(response.body.find("Verification output:"), std::string::npos);
  EXPECT_NE(response.body.find("verify:"), std::string::npos);
  EXPECT_EQ(response.body.find("node.kind === 'director'"), std::string::npos);
}

TEST(BareosConfigService, ReturnsDryRunPreviewForUpdatedContent)
{
  TempConfigRoot root;
  std::filesystem::create_directories(root.path() / "bareos-dir.d/client");
  std::ofstream(root.path() / "bareos-dir.conf") << "Director {}\n";
  std::ofstream(root.path() / "bareos-dir.d/client/example-fd.conf")
      << "Client {\n  Name = example-fd\n}\n";

  const ConfigServiceOptions options = MakeServiceOptions(root.path());
  const HttpRequest request{"POST",
                            "/api/v1/resources/resource-0-director-0/preview",
                            "text/plain; charset=utf-8",
                            "Client {\n  Name = renamed-fd\n}\n"};

  const auto response = HandleConfigServiceRequest(options, request);

  EXPECT_EQ(response.status_code, 200);
  EXPECT_NE(response.body.find("\"changed\":true"), std::string::npos);
  EXPECT_NE(response.body.find("renamed-fd"), std::string::npos);
  EXPECT_NE(response.body.find("\"updated_field_hints\":[{"), std::string::npos);
  EXPECT_NE(response.body.find("\"updated_directives\":[{"), std::string::npos);
  EXPECT_NE(response.body.find("\"updated_validation_messages\":[{"),
            std::string::npos);
  EXPECT_NE(response.body.find("\"code\":\"missing-address\""), std::string::npos);
  EXPECT_NE(response.body.find("\"updated_line_count\":3"), std::string::npos);
}

TEST(BareosConfigService, ReportsRenameReferenceImpactsInPreview)
{
  TempConfigRoot root;
  std::filesystem::create_directories(root.path() / "bareos-dir.d/client");
  std::filesystem::create_directories(root.path() / "bareos-dir.d/job");
  std::ofstream(root.path() / "bareos-dir.conf") << "Director {}\n";
  std::ofstream(root.path() / "bareos-dir.d/client/example-fd.conf")
      << "Client {\n"
      << "  Name = example-fd\n"
      << "  Address = 10.0.0.10\n"
      << "  Password = secret\n"
      << "}\n";
  const auto job_path = root.path() / "bareos-dir.d/job/backup.conf";
  std::ofstream(job_path)
      << "Job {\n"
      << "  Name = BackupClient\n"
      << "  Client = example-fd\n"
      << "}\n";

  const auto model = DiscoverDatacenterSummary({root.path()});
  const auto resource = std::find_if(
      model.directors[0].resources.begin(), model.directors[0].resources.end(),
      [](const ResourceSummary& summary) { return summary.name == "example-fd"; });
  ASSERT_NE(resource, model.directors[0].resources.end());

  const ConfigServiceOptions options = MakeServiceOptions(root.path());
  const HttpRequest request{
      "POST",
      "/api/v1/resources/" + resource->id + "/preview",
      "text/plain; charset=utf-8",
      "Client {\n  Name = renamed-fd\n  Address = 10.0.0.10\n  Password = secret\n}\n"};

  const auto response = HandleConfigServiceRequest(options, request);

  EXPECT_EQ(response.status_code, 200);
  EXPECT_NE(response.body.find("\"code\":\"rename-reference-update\""),
            std::string::npos);
  EXPECT_NE(response.body.find("\"rename_impacts\":[{"), std::string::npos);
  EXPECT_NE(response.body.find("\"resource_type\":\"job\""), std::string::npos);
  EXPECT_NE(response.body.find("\"directive_key\":\"Client\""), std::string::npos);
  EXPECT_NE(response.body.find("Client in job BackupClient"), std::string::npos);
  EXPECT_NE(response.body.find(job_path.string()), std::string::npos);
  EXPECT_NE(response.body.find("\"new_value\":\"renamed-fd\""), std::string::npos);
}

TEST(BareosConfigService, ReturnsFieldBasedPreviewForUpdatedContent)
{
  TempConfigRoot root;
  std::filesystem::create_directories(root.path() / "bareos-dir.d/client");
  std::ofstream(root.path() / "bareos-dir.conf") << "Director {}\n";
  std::ofstream(root.path() / "bareos-dir.d/client/example-fd.conf")
      << "Client {\n  Name = example-fd\n}\n";

  const ConfigServiceOptions options = MakeServiceOptions(root.path());
  const HttpRequest request{"POST",
                            "/api/v1/resources/resource-0-director-0/preview-fields",
                            "text/plain; charset=utf-8",
                            "Name=example-fd\nAddress=10.0.0.20\nPassword=secret\n"};

  const auto response = HandleConfigServiceRequest(options, request);

  EXPECT_EQ(response.status_code, 200);
  EXPECT_NE(response.body.find("Address = 10.0.0.20"), std::string::npos);
  EXPECT_NE(response.body.find("\"updated_field_hints\":[{"), std::string::npos);
  EXPECT_NE(response.body.find("\"updated_validation_messages\":[]"),
            std::string::npos);
}

TEST(BareosConfigService, ReturnsFieldBasedPreviewForNewResource)
{
  TempConfigRoot root;
  std::filesystem::create_directories(root.path() / "bareos-dir.d/client");
  std::ofstream(root.path() / "bareos-dir.conf") << "Director {}\n";
  std::ofstream(root.path() / "bareos-dir.d/client/example-fd.conf")
      << "Client {\n  Name = example-fd\n}\n";

  const ConfigServiceOptions options = MakeServiceOptions(root.path());
  const HttpRequest request{
      "POST",
      "/api/v1/nodes/director-0/new-resource/client/preview-fields",
      "text/plain; charset=utf-8",
      "Name=wizard-fd\nAddress=10.0.0.20\nPassword=secret\n"};

  const auto response = HandleConfigServiceRequest(options, request);

  EXPECT_EQ(response.status_code, 200);
  EXPECT_NE(response.body.find("Name = wizard-fd"), std::string::npos);
  EXPECT_NE(response.body.find("Address = 10.0.0.20"), std::string::npos);
  EXPECT_NE(response.body.find("Password = secret"), std::string::npos);
  EXPECT_NE(response.body.find("\"updated_validation_messages\":[]"),
            std::string::npos);
}

TEST(BareosConfigService, SavesUpdatedResourceContent)
{
  TempConfigRoot root;
  std::filesystem::create_directories(root.path() / "bareos-dir.d/client");
  std::ofstream(root.path() / "bareos-dir.conf") << "Director {}\n";
  const auto resource_path = root.path() / "bareos-dir.d/client/example-fd.conf";
  std::ofstream(resource_path)
      << "Client {\n  Name = example-fd\n  Address = 10.0.0.10\n}\n";

  const ConfigServiceOptions options = MakeServiceOptions(root.path());
  const HttpRequest request{
      "POST",
      "/api/v1/resources/resource-0-director-0/save",
      "text/plain; charset=utf-8",
      "Client {\n  Name = example-fd\n  Address = 10.0.0.20\n  Password = secret\n}\n"};

  const auto response = HandleConfigServiceRequest(options, request);

  EXPECT_EQ(response.status_code, 200);
  EXPECT_NE(response.body.find("\"saved\":true"), std::string::npos);
  EXPECT_NE(response.body.find("\"action\":\"update\""), std::string::npos);
  EXPECT_NE(response.body.find("\"backup_path\":\""), std::string::npos);
  EXPECT_NE(response.body.find("\"follow_up_component\":\"director\""),
            std::string::npos);
  EXPECT_NE(response.body.find("\"follow_up_action\":\"reload\""),
            std::string::npos);

  const auto saved_content = [&resource_path] {
    std::ifstream input(resource_path);
    std::ostringstream output;
    output << input.rdbuf();
    return output.str();
  }();
  EXPECT_NE(saved_content.find("Address = 10.0.0.20"), std::string::npos);
  EXPECT_NE(saved_content.find("Password = secret"), std::string::npos);
}

TEST(BareosConfigService, SavesRenamedResourceAndUpdatesReferences)
{
  TempConfigRoot root;
  std::filesystem::create_directories(root.path() / "bareos-dir.d/client");
  std::filesystem::create_directories(root.path() / "bareos-dir.d/job");
  std::ofstream(root.path() / "bareos-dir.conf") << "Director {}\n";
  const auto client_path = root.path() / "bareos-dir.d/client/example-fd.conf";
  const auto job_path = root.path() / "bareos-dir.d/job/backup.conf";
  std::ofstream(client_path)
      << "Client {\n"
      << "  Name = example-fd\n"
      << "  Address = 10.0.0.10\n"
      << "  Password = secret\n"
      << "}\n";
  std::ofstream(job_path)
      << "Job {\n"
      << "  Name = BackupClient\n"
      << "  Client = example-fd\n"
      << "}\n";

  const auto model = DiscoverDatacenterSummary({root.path()});
  const auto resource = std::find_if(
      model.directors[0].resources.begin(), model.directors[0].resources.end(),
      [](const ResourceSummary& summary) { return summary.name == "example-fd"; });
  ASSERT_NE(resource, model.directors[0].resources.end());

  const ConfigServiceOptions options = MakeServiceOptions(root.path());
  const HttpRequest request{
      "POST",
      "/api/v1/resources/" + resource->id + "/save",
      "text/plain; charset=utf-8",
      "Client {\n  Name = renamed-fd\n  Address = 10.0.0.10\n  Password = secret\n}\n"};

  const auto response = HandleConfigServiceRequest(options, request);

  EXPECT_EQ(response.status_code, 200);
  EXPECT_NE(response.body.find("\"saved\":true"), std::string::npos);
  EXPECT_NE(response.body.find("\"affected_updates\":[{"), std::string::npos);
  EXPECT_NE(response.body.find(job_path.string()), std::string::npos);
  const auto renamed_client_path = root.path() / "bareos-dir.d/client/renamed-fd.conf";
  ASSERT_FALSE(std::filesystem::exists(client_path));
  ASSERT_TRUE(std::filesystem::exists(renamed_client_path));

  const auto client_content = [&renamed_client_path] {
    std::ifstream input(renamed_client_path);
    std::ostringstream output;
    output << input.rdbuf();
    return output.str();
  }();
  const auto job_content = [&job_path] {
    std::ifstream input(job_path);
    std::ostringstream output;
    output << input.rdbuf();
    return output.str();
  }();

  EXPECT_NE(client_content.find("Name = renamed-fd"), std::string::npos);
  EXPECT_NE(job_content.find("Client = renamed-fd"), std::string::npos);
  EXPECT_EQ(job_content.find("Client = example-fd"), std::string::npos);
}

TEST(BareosConfigService,
     SavesRenamedPoolAndUpdatesSelfAndPoolReferenceForms)
{
  TempConfigRoot root;
  std::filesystem::create_directories(root.path() / "bareos-dir.d/pool");
  std::filesystem::create_directories(root.path() / "bareos-dir.d/job");
  std::filesystem::create_directories(root.path() / "bareos-dir.d/schedule");
  std::ofstream(root.path() / "bareos-dir.conf") << "Director {}\n";
  const auto pool_path = root.path() / "bareos-dir.d/pool/scratch.conf";
  const auto job_path = root.path() / "bareos-dir.d/job/backup.conf";
  const auto schedule_path = root.path() / "bareos-dir.d/schedule/nightly.conf";
  std::ofstream(pool_path)
      << "Pool {\n"
      << "  Name = ScratchPool\n"
      << "  RecyclePool = ScratchPool\n"
      << "}\n";
  std::ofstream(job_path)
      << "Job {\n"
      << "  Name = BackupJob\n"
      << "  Pool = ScratchPool\n"
      << "  FullBackupPool = ScratchPool\n"
      << "}\n";
  std::ofstream(schedule_path)
      << "Schedule {\n"
      << "  Name = Nightly\n"
      << "  Run = Incremental FullPool=ScratchPool mon at 21:00\n"
      << "}\n";

  const auto model = DiscoverDatacenterSummary({root.path()});
  const auto resource = std::find_if(
      model.directors[0].resources.begin(), model.directors[0].resources.end(),
      [](const ResourceSummary& summary) {
        return summary.type == "pool" && summary.name == "ScratchPool";
      });
  ASSERT_NE(resource, model.directors[0].resources.end());

  const ConfigServiceOptions options = MakeServiceOptions(root.path());
  const HttpRequest request{
      "POST",
      "/api/v1/resources/" + resource->id + "/save",
      "text/plain; charset=utf-8",
      "Pool {\n  Name = RenamedScratchPool\n  RecyclePool = ScratchPool\n}\n"};

  const auto response = HandleConfigServiceRequest(options, request);

  EXPECT_EQ(response.status_code, 200) << response.body;
  EXPECT_NE(response.body.find("\"saved\":true"), std::string::npos);
  EXPECT_NE(response.body.find("\"directive_key\":\"RecyclePool\""),
            std::string::npos);
  EXPECT_NE(response.body.find("\"directive_key\":\"Pool\""), std::string::npos);
  EXPECT_NE(response.body.find("\"directive_key\":\"FullBackupPool\""),
            std::string::npos);
  EXPECT_NE(response.body.find("\"directive_key\":\"FullPool\""),
            std::string::npos);

  const auto pool_content = [&pool_path] {
    std::ifstream input(pool_path);
    std::ostringstream output;
    output << input.rdbuf();
    return output.str();
  }();
  const auto job_content = [&job_path] {
    std::ifstream input(job_path);
    std::ostringstream output;
    output << input.rdbuf();
    return output.str();
  }();
  const auto schedule_content = [&schedule_path] {
    std::ifstream input(schedule_path);
    std::ostringstream output;
    output << input.rdbuf();
    return output.str();
  }();

  EXPECT_NE(pool_content.find("Name = RenamedScratchPool"), std::string::npos);
  EXPECT_NE(pool_content.find("RecyclePool = RenamedScratchPool"),
            std::string::npos);
  EXPECT_EQ(pool_content.find("RecyclePool = ScratchPool"), std::string::npos);
  EXPECT_NE(job_content.find("Pool = RenamedScratchPool"), std::string::npos);
  EXPECT_NE(job_content.find("FullBackupPool = RenamedScratchPool"),
            std::string::npos);
  EXPECT_EQ(job_content.find("FullBackupPool = ScratchPool"), std::string::npos);
  EXPECT_NE(schedule_content.find("Run = Incremental FullPool=RenamedScratchPool mon at 21:00"),
            std::string::npos);
  EXPECT_EQ(schedule_content.find("Run = Incremental FullPool=ScratchPool mon at 21:00"),
            std::string::npos);
}

TEST(BareosConfigService, ReportsPasswordReferenceImpactsInPreview)
{
  TempConfigRoot root;
  std::filesystem::create_directories(root.path() / "bareos-dir.d/client");
  std::filesystem::create_directories(root.path() / "bareos-fd.d/director");
  std::ofstream(root.path() / "bareos-dir.conf") << "Director {}\n";
  std::ofstream(root.path() / "bareos-fd.conf")
      << "FileDaemon {\n  Name = example-fd\n}\n";
  std::ofstream(root.path() / "bareos-dir.d/client/example-fd.conf")
      << "Client {\n"
      << "  Name = example-fd\n"
      << "  Address = 10.0.0.10\n"
      << "  Password = secret\n"
      << "}\n";
  const auto counterpart_path = root.path() / "bareos-fd.d/director/bareos-dir.conf";
  std::ofstream(counterpart_path)
      << "Director {\n"
      << "  Name = bareos-dir\n"
      << "  Password = secret\n"
      << "}\n";

  const auto model = DiscoverDatacenterSummary({root.path()});
  const auto resource = std::find_if(
      model.directors[0].resources.begin(), model.directors[0].resources.end(),
      [](const ResourceSummary& summary) { return summary.name == "example-fd"; });
  ASSERT_NE(resource, model.directors[0].resources.end());

  const ConfigServiceOptions options = MakeServiceOptions(root.path());
  const HttpRequest request{
      "POST",
      "/api/v1/resources/" + resource->id + "/preview",
      "text/plain; charset=utf-8",
      "Client {\n  Name = example-fd\n  Address = 10.0.0.10\n  Password = newersecret\n}\n"};

  const auto response = HandleConfigServiceRequest(options, request);

  EXPECT_EQ(response.status_code, 200);
  EXPECT_NE(response.body.find("\"code\":\"password-reference-update\""),
            std::string::npos);
  EXPECT_NE(response.body.find("Password in director bareos-dir"),
            std::string::npos);
  EXPECT_NE(response.body.find(counterpart_path.string()), std::string::npos);
  EXPECT_NE(response.body.find("\"directive_key\":\"Password\""), std::string::npos);
  EXPECT_NE(response.body.find("\"new_value\":\"newersecret\""), std::string::npos);
}

TEST(BareosConfigService, ReportsStorageDeviceReferenceImpactsInPreview)
{
  TempConfigRoot root;
  std::filesystem::create_directories(root.path() / "bareos-dir.d/storage");
  std::filesystem::create_directories(root.path() / "bareos-sd.d/storage");
  std::filesystem::create_directories(root.path() / "bareos-sd.d/device");
  std::ofstream(root.path() / "bareos-dir.conf") << "Director {}\n";
  std::ofstream(root.path() / "bareos-sd.conf")
      << "Storage {\n  Name = example-sd\n}\n";
  const auto director_storage_path
      = root.path() / "bareos-dir.d/storage/example-sd.conf";
  std::ofstream(director_storage_path)
      << "Storage {\n"
      << "  Name = example-sd\n"
      << "  Address = 10.0.0.20\n"
      << "  Password = secret\n"
      << "  Device = tape-drive-0\n"
      << "  MediaType = File\n"
      << "}\n";
  std::ofstream(root.path() / "bareos-sd.d/storage/example-sd.conf")
      << "Storage {\n"
      << "  Name = example-sd\n"
      << "  Device = tape-drive-0\n"
      << "  MediaType = File\n"
      << "}\n";
  std::ofstream(root.path() / "bareos-sd.d/device/tape.conf")
      << "Device {\n"
      << "  Name = tape-drive-0\n"
      << "  ArchiveDevice = /tmp/tape-drive-0\n"
      << "  MediaType = File\n"
      << "}\n";

  const auto model = DiscoverDatacenterSummary({root.path()});
  const auto& daemon_resources = model.directors[0].daemons[0].resources;
  const auto resource = std::find_if(
      daemon_resources.begin(), daemon_resources.end(),
      [](const ResourceSummary& summary) {
        return summary.type == "device" && summary.name == "tape-drive-0";
      });
  ASSERT_NE(resource, daemon_resources.end());

  const ConfigServiceOptions options = MakeServiceOptions(root.path());
  const HttpRequest request{
      "POST",
      "/api/v1/resources/" + resource->id + "/preview",
      "text/plain; charset=utf-8",
      "Device {\n  Name = tape-drive-1\n  ArchiveDevice = /tmp/tape-drive-0\n  MediaType = File\n}\n"};

  const auto response = HandleConfigServiceRequest(options, request);

  EXPECT_EQ(response.status_code, 200);
  EXPECT_NE(response.body.find("\"code\":\"rename-reference-update\""),
            std::string::npos);
  EXPECT_NE(response.body.find("Device in storage example-sd"),
            std::string::npos);
  EXPECT_NE(response.body.find(director_storage_path.string()), std::string::npos);
  EXPECT_NE(response.body.find("\"directive_key\":\"Device\""), std::string::npos);
  EXPECT_NE(response.body.find("\"new_value\":\"tape-drive-1\""), std::string::npos);
}

TEST(BareosConfigService, ReportsDirectorDeviceRenameImpactsInPreview)
{
  TempConfigRoot root;
  std::filesystem::create_directories(root.path() / "bareos-dir.d/storage");
  std::filesystem::create_directories(root.path() / "bareos-sd.d/storage");
  std::filesystem::create_directories(root.path() / "bareos-sd.d/device");
  std::ofstream(root.path() / "bareos-dir.conf") << "Director {}\n";
  std::ofstream(root.path() / "bareos-sd.conf")
      << "Storage {\n  Name = example-sd\n}\n";
  const auto director_storage_path
      = root.path() / "bareos-dir.d/storage/example-sd.conf";
  const auto device_path = root.path() / "bareos-sd.d/device/tape.conf";
  std::ofstream(director_storage_path)
      << "Storage {\n"
      << "  Name = example-sd\n"
      << "  Address = 10.0.0.20\n"
      << "  Password = secret\n"
      << "  Device = tape-drive-0\n"
      << "  MediaType = File\n"
      << "}\n";
  std::ofstream(root.path() / "bareos-sd.d/storage/example-sd.conf")
      << "Storage {\n"
      << "  Name = example-sd\n"
      << "  Device = tape-drive-0\n"
      << "  MediaType = File\n"
      << "}\n";
  std::ofstream(device_path)
      << "Device {\n"
      << "  Name = tape-drive-0\n"
      << "  ArchiveDevice = /tmp/tape-drive-0\n"
      << "  MediaType = File\n"
      << "}\n";

  const auto model = DiscoverDatacenterSummary({root.path()});
  const auto resource = std::find_if(
      model.directors[0].resources.begin(), model.directors[0].resources.end(),
      [](const ResourceSummary& summary) { return summary.type == "storage"; });
  ASSERT_NE(resource, model.directors[0].resources.end());

  const ConfigServiceOptions options = MakeServiceOptions(root.path());
  const HttpRequest request{
      "POST",
      "/api/v1/resources/" + resource->id + "/preview",
      "text/plain; charset=utf-8",
      "Storage {\n  Name = example-sd\n  Address = 10.0.0.20\n  Password = secret\n  Device = tape-drive-1\n  MediaType = File\n}\n"};

  const auto response = HandleConfigServiceRequest(options, request);

  EXPECT_EQ(response.status_code, 200);
  EXPECT_NE(response.body.find("\"code\":\"rename-reference-update\""),
            std::string::npos);
  EXPECT_NE(response.body.find("Name in device tape-drive-0"),
            std::string::npos);
  EXPECT_NE(response.body.find(device_path.string()), std::string::npos);
  EXPECT_NE(response.body.find("\"directive_key\":\"Name\""), std::string::npos);
  EXPECT_NE(response.body.find("\"new_value\":\"tape-drive-1\""), std::string::npos);
}

TEST(BareosConfigService, ReportsDirectorRenameImpactsInPreview)
{
  TempConfigRoot root;
  std::filesystem::create_directories(root.path() / "bareos-dir.d/director");
  std::filesystem::create_directories(root.path() / "bareos-fd.d/director");
  std::filesystem::create_directories(root.path() / "bareos-sd.d/director");
  std::filesystem::create_directories(root.path() / "bconsole.d/console");
  std::filesystem::create_directories(root.path() / "bconsole.d/user");
  std::ofstream(root.path() / "bareos-dir.conf") << "Director {}\n";
  const auto director_path = root.path() / "bareos-dir.d/director/bareos-dir.conf";
  const auto fd_director_path = root.path() / "bareos-fd.d/director/bareos-dir.conf";
  const auto sd_director_path = root.path() / "bareos-sd.d/director/bareos-dir.conf";
  const auto console_config_path = root.path() / "bconsole.conf";
  const auto console_path = root.path() / "bconsole.d/console/admin.conf";
  const auto user_path = root.path() / "bconsole.d/user/api.conf";
  std::ofstream(director_path)
      << "Director {\n"
      << "  Name = bareos-dir\n"
      << "  Password = secret\n"
      << "}\n";
  std::ofstream(fd_director_path)
      << "Director {\n"
      << "  Name = bareos-dir\n"
      << "  Password = secret\n"
      << "}\n";
  std::ofstream(sd_director_path)
      << "Director {\n"
      << "  Name = bareos-dir\n"
      << "  Password = secret\n"
      << "}\n";
  std::ofstream(console_config_path)
      << "Director {\n"
      << "  Name = bareos-dir\n"
      << "}\n"
      << "Console {\n"
      << "  Name = admin\n"
      << "}\n";
  std::ofstream(console_path)
      << "Console {\n  Name = admin\n  Password = secret\n}\n";

  const auto model = DiscoverDatacenterSummary({root.path()});
  const auto resource = std::find_if(
      model.directors[0].resources.begin(), model.directors[0].resources.end(),
      [](const ResourceSummary& summary) {
        return summary.type == "director" && summary.name == "bareos-dir";
      });
  ASSERT_NE(resource, model.directors[0].resources.end());

  const ConfigServiceOptions options = MakeServiceOptions(root.path());
  const HttpRequest request{
      "POST",
      "/api/v1/resources/" + resource->id + "/preview",
      "text/plain; charset=utf-8",
      "Director {\n  Name = renamed-dir\n  Password = secret\n}\n"};

  const auto response = HandleConfigServiceRequest(options, request);

  EXPECT_EQ(response.status_code, 200);
  EXPECT_NE(response.body.find("\"code\":\"rename-reference-update\""),
            std::string::npos);
  EXPECT_NE(response.body.find("Name in director bareos-dir"), std::string::npos);
  EXPECT_NE(response.body.find(fd_director_path.string()), std::string::npos);
  EXPECT_NE(response.body.find(sd_director_path.string()), std::string::npos);
  EXPECT_NE(response.body.find(console_config_path.string()), std::string::npos);
  EXPECT_NE(response.body.find("\"new_value\":\"renamed-dir\""), std::string::npos);
}

TEST(BareosConfigService, ReportsDirectorPasswordImpactsInPreview)
{
  TempConfigRoot root;
  std::filesystem::create_directories(root.path() / "bareos-dir.d/director");
  std::filesystem::create_directories(root.path() / "bareos-fd.d/director");
  std::filesystem::create_directories(root.path() / "bareos-sd.d/director");
  std::filesystem::create_directories(root.path() / "bconsole.d/console");
  std::filesystem::create_directories(root.path() / "bconsole.d/user");
  std::ofstream(root.path() / "bareos-dir.conf") << "Director {}\n";
  const auto director_path = root.path() / "bareos-dir.d/director/bareos-dir.conf";
  const auto fd_director_path = root.path() / "bareos-fd.d/director/bareos-dir.conf";
  const auto sd_director_path = root.path() / "bareos-sd.d/director/bareos-dir.conf";
  const auto console_config_path = root.path() / "bconsole.conf";
  const auto console_path = root.path() / "bconsole.d/console/admin.conf";
  const auto user_path = root.path() / "bconsole.d/user/api.conf";
  std::ofstream(director_path)
      << "Director {\n"
      << "  Name = bareos-dir\n"
      << "  Password = secret\n"
      << "}\n";
  std::ofstream(fd_director_path)
      << "Director {\n"
      << "  Name = bareos-dir\n"
      << "  Password = secret\n"
      << "}\n";
  std::ofstream(sd_director_path)
      << "Director {\n"
      << "  Name = bareos-dir\n"
      << "  Password = secret\n"
      << "}\n";
  std::ofstream(console_config_path)
      << "Director {\n"
      << "  Name = bareos-dir\n"
      << "}\n"
      << "Console {\n"
      << "  Password = secret\n"
      << "}\n";
  std::ofstream(console_path)
      << "Console {\n  Name = admin\n  Password = secret\n}\n";
  std::ofstream(user_path)
      << "User {\n  Name = api\n  Password = secret\n}\n";

  const auto model = DiscoverDatacenterSummary({root.path()});
  const auto resource = std::find_if(
      model.directors[0].resources.begin(), model.directors[0].resources.end(),
      [](const ResourceSummary& summary) {
        return summary.type == "director" && summary.name == "bareos-dir";
      });
  ASSERT_NE(resource, model.directors[0].resources.end());

  const ConfigServiceOptions options = MakeServiceOptions(root.path());
  const HttpRequest request{
      "POST",
      "/api/v1/resources/" + resource->id + "/preview",
      "text/plain; charset=utf-8",
      "Director {\n  Name = bareos-dir\n  Password = newersecret\n}\n"};

  const auto response = HandleConfigServiceRequest(options, request);

  EXPECT_EQ(response.status_code, 200);
  EXPECT_NE(response.body.find("\"code\":\"password-reference-update\""),
            std::string::npos);
  EXPECT_NE(response.body.find("Password in director bareos-dir"),
            std::string::npos);
  EXPECT_NE(response.body.find(fd_director_path.string()), std::string::npos);
  EXPECT_NE(response.body.find(sd_director_path.string()), std::string::npos);
  EXPECT_NE(response.body.find(console_config_path.string()), std::string::npos);
  EXPECT_NE(response.body.find(console_path.string()), std::string::npos);
  EXPECT_NE(response.body.find(user_path.string()), std::string::npos);
  EXPECT_NE(response.body.find("\"directive_key\":\"Password\""), std::string::npos);
  EXPECT_NE(response.body.find("\"new_value\":\"newersecret\""), std::string::npos);
}

TEST(BareosConfigService, ReportsRootDirectorConfigRenameImpactsInPreview)
{
  TempConfigRoot root;
  std::filesystem::create_directories(root.path() / "bareos-fd.d/director");
  std::filesystem::create_directories(root.path() / "bareos-sd.d/director");
  std::filesystem::create_directories(root.path() / "bconsole.d/console");
  std::filesystem::create_directories(root.path() / "bconsole.d/user");
  const auto director_config_path = root.path() / "bareos-dir.conf";
  const auto fd_director_path = root.path() / "bareos-fd.d/director/bareos-dir.conf";
  const auto sd_director_path = root.path() / "bareos-sd.d/director/bareos-dir.conf";
  const auto console_config_path = root.path() / "bconsole.conf";
  const auto console_path = root.path() / "bconsole.d/console/admin.conf";
  const auto user_path = root.path() / "bconsole.d/user/api.conf";
  std::ofstream(director_config_path)
      << "Director {\n"
      << "  Name = bareos-dir\n"
      << "  Password = secret\n"
      << "}\n";
  std::ofstream(fd_director_path)
      << "Director {\n"
      << "  Name = bareos-dir\n"
      << "  Password = secret\n"
      << "}\n";
  std::ofstream(sd_director_path)
      << "Director {\n"
      << "  Name = bareos-dir\n"
      << "  Password = secret\n"
      << "}\n";
  std::ofstream(console_config_path)
      << "Director {\n"
      << "  Name = bareos-dir\n"
      << "}\n"
      << "Console {\n"
      << "  Password = secret\n"
      << "}\n";
  std::ofstream(console_path)
      << "Console {\n  Name = admin\n  Password = secret\n}\n";
  std::ofstream(user_path)
      << "User {\n  Name = api\n  Password = secret\n}\n";

  const auto model = DiscoverDatacenterSummary({root.path()});
  const auto resource = std::find_if(
      model.directors[0].resources.begin(), model.directors[0].resources.end(),
      [&director_config_path](const ResourceSummary& summary) {
        return summary.type == "configuration"
               && summary.file_path == director_config_path.string();
      });
  ASSERT_NE(resource, model.directors[0].resources.end());

  const ConfigServiceOptions options = MakeServiceOptions(root.path());
  const HttpRequest request{
      "POST",
      "/api/v1/resources/" + resource->id + "/preview",
      "text/plain; charset=utf-8",
      "Director {\n  Name = renamed-dir\n  Password = secret\n}\n"};

  const auto response = HandleConfigServiceRequest(options, request);

  EXPECT_EQ(response.status_code, 200) << response.body;
  EXPECT_NE(response.body.find("\"code\":\"rename-reference-update\""),
            std::string::npos)
      << response.body;
  EXPECT_NE(response.body.find(fd_director_path.string()), std::string::npos)
      << response.body;
  EXPECT_NE(response.body.find(sd_director_path.string()), std::string::npos)
      << response.body;
  EXPECT_NE(response.body.find(console_config_path.string()), std::string::npos)
      << response.body;
  EXPECT_NE(response.body.find("\"new_value\":\"renamed-dir\""), std::string::npos)
      << response.body;
}

TEST(BareosConfigService, ReportsRootDirectorConfigPasswordImpactsInPreview)
{
  TempConfigRoot root;
  std::filesystem::create_directories(root.path() / "bareos-fd.d/director");
  std::filesystem::create_directories(root.path() / "bareos-sd.d/director");
  std::filesystem::create_directories(root.path() / "bconsole.d/console");
  std::filesystem::create_directories(root.path() / "bconsole.d/user");
  const auto director_config_path = root.path() / "bareos-dir.conf";
  const auto fd_director_path = root.path() / "bareos-fd.d/director/bareos-dir.conf";
  const auto sd_director_path = root.path() / "bareos-sd.d/director/bareos-dir.conf";
  const auto console_config_path = root.path() / "bconsole.conf";
  const auto console_path = root.path() / "bconsole.d/console/admin.conf";
  const auto user_path = root.path() / "bconsole.d/user/api.conf";
  std::ofstream(director_config_path)
      << "Director {\n"
      << "  Name = bareos-dir\n"
      << "  Password = secret\n"
      << "}\n";
  std::ofstream(fd_director_path)
      << "Director {\n"
      << "  Name = bareos-dir\n"
      << "  Password = secret\n"
      << "}\n";
  std::ofstream(sd_director_path)
      << "Director {\n"
      << "  Name = bareos-dir\n"
      << "  Password = secret\n"
      << "}\n";
  std::ofstream(console_config_path)
      << "Director {\n"
      << "  Name = bareos-dir\n"
      << "}\n"
      << "Console {\n"
      << "  Password = secret\n"
      << "}\n";
  std::ofstream(console_path)
      << "Console {\n  Name = admin\n  Password = secret\n}\n";
  std::ofstream(user_path)
      << "User {\n  Name = api\n  Password = secret\n}\n";

  const auto model = DiscoverDatacenterSummary({root.path()});
  const auto resource = std::find_if(
      model.directors[0].resources.begin(), model.directors[0].resources.end(),
      [&director_config_path](const ResourceSummary& summary) {
        return summary.type == "configuration"
               && summary.file_path == director_config_path.string();
      });
  ASSERT_NE(resource, model.directors[0].resources.end());

  const ConfigServiceOptions options = MakeServiceOptions(root.path());
  const HttpRequest request{
      "POST",
      "/api/v1/resources/" + resource->id + "/preview",
      "text/plain; charset=utf-8",
      "Director {\n  Name = bareos-dir\n  Password = newersecret\n}\n"};

  const auto response = HandleConfigServiceRequest(options, request);

  EXPECT_EQ(response.status_code, 200) << response.body;
  EXPECT_NE(response.body.find("\"code\":\"password-reference-update\""),
            std::string::npos)
      << response.body;
  EXPECT_NE(response.body.find(fd_director_path.string()), std::string::npos)
      << response.body;
  EXPECT_NE(response.body.find(sd_director_path.string()), std::string::npos)
      << response.body;
  EXPECT_NE(response.body.find(console_config_path.string()), std::string::npos)
      << response.body;
  EXPECT_NE(response.body.find(console_path.string()), std::string::npos)
      << response.body;
  EXPECT_NE(response.body.find(user_path.string()), std::string::npos)
      << response.body;
  EXPECT_NE(response.body.find("\"new_value\":\"newersecret\""),
            std::string::npos)
      << response.body;
}

TEST(BareosConfigService, SavesUpdatedPasswordAndUpdatesCounterpart)
{
  TempConfigRoot root;
  std::filesystem::create_directories(root.path() / "bareos-dir.d/client");
  std::filesystem::create_directories(root.path() / "bareos-fd.d/director");
  std::ofstream(root.path() / "bareos-dir.conf") << "Director {}\n";
  std::ofstream(root.path() / "bareos-fd.conf")
      << "FileDaemon {\n  Name = example-fd\n}\n";
  const auto client_path = root.path() / "bareos-dir.d/client/example-fd.conf";
  const auto counterpart_path = root.path() / "bareos-fd.d/director/bareos-dir.conf";
  std::ofstream(client_path)
      << "Client {\n"
      << "  Name = example-fd\n"
      << "  Address = 10.0.0.10\n"
      << "  Password = secret\n"
      << "}\n";
  std::ofstream(counterpart_path)
      << "Director {\n"
      << "  Name = bareos-dir\n"
      << "  Password = secret\n"
      << "}\n";

  const auto model = DiscoverDatacenterSummary({root.path()});
  const auto resource = std::find_if(
      model.directors[0].resources.begin(), model.directors[0].resources.end(),
      [](const ResourceSummary& summary) { return summary.name == "example-fd"; });
  ASSERT_NE(resource, model.directors[0].resources.end());

  const ConfigServiceOptions options = MakeServiceOptions(root.path());
  const HttpRequest request{
      "POST",
      "/api/v1/resources/" + resource->id + "/save",
      "text/plain; charset=utf-8",
      "Client {\n  Name = example-fd\n  Address = 10.0.0.10\n  Password = newersecret\n}\n"};

  const auto response = HandleConfigServiceRequest(options, request);

  EXPECT_EQ(response.status_code, 200);
  EXPECT_NE(response.body.find("\"saved\":true"), std::string::npos);
  EXPECT_NE(response.body.find(counterpart_path.string()), std::string::npos);

  const auto client_content = [&client_path] {
    std::ifstream input(client_path);
    std::ostringstream output;
    output << input.rdbuf();
    return output.str();
  }();
  const auto counterpart_content = [&counterpart_path] {
    std::ifstream input(counterpart_path);
    std::ostringstream output;
    output << input.rdbuf();
    return output.str();
  }();

  EXPECT_NE(client_content.find("Password = newersecret"), std::string::npos);
  EXPECT_NE(counterpart_content.find("Password = newersecret"), std::string::npos);
  EXPECT_EQ(counterpart_content.find("Password = secret"), std::string::npos);
}

TEST(BareosConfigService, SavesUpdatedDirectorPasswordAndUpdatesBconsoleConfig)
{
  TempConfigRoot root;
  std::filesystem::create_directories(root.path() / "bareos-dir.d/director");
  std::filesystem::create_directories(root.path() / "bareos-fd.d/director");
  std::filesystem::create_directories(root.path() / "bareos-sd.d/director");
  std::filesystem::create_directories(root.path() / "bconsole.d/console");
  std::filesystem::create_directories(root.path() / "bconsole.d/user");
  std::ofstream(root.path() / "bareos-dir.conf") << "Director {}\n";
  std::ofstream(root.path() / "bareos-fd.conf")
      << "FileDaemon {\n  Name = example-fd\n}\n";
  std::ofstream(root.path() / "bareos-sd.conf")
      << "Storage {\n  Name = example-sd\n}\n";
  std::ofstream(root.path() / "bconsole.conf")
      << "Director {\n  Name = bareos-dir\n}\n"
      << "Console {\n  Password = secret\n}\n";
  const auto console_path = root.path() / "bconsole.d/console/admin.conf";
  std::ofstream(console_path)
      << "Console {\n  Name = admin\n  Password = secret\n}\n";
  const auto user_path = root.path() / "bconsole.d/user/api.conf";
  std::ofstream(user_path)
      << "User {\n  Name = api\n  Password = secret\n}\n";
  const auto director_path = root.path() / "bareos-dir.d/director/bareos-dir.conf";
  const auto fd_director_path = root.path() / "bareos-fd.d/director/bareos-dir.conf";
  const auto sd_director_path = root.path() / "bareos-sd.d/director/bareos-dir.conf";
  const auto console_config_path = root.path() / "bconsole.conf";
  std::ofstream(director_path)
      << "Director {\n"
      << "  Name = bareos-dir\n"
      << "  Password = secret\n"
      << "}\n";
  std::ofstream(fd_director_path)
      << "Director {\n"
      << "  Name = bareos-dir\n"
      << "  Password = secret\n"
      << "}\n";
  std::ofstream(sd_director_path)
      << "Director {\n"
      << "  Name = bareos-dir\n"
      << "  Password = secret\n"
      << "}\n";

  const auto model = DiscoverDatacenterSummary({root.path()});
  const auto resource = std::find_if(
      model.directors[0].resources.begin(), model.directors[0].resources.end(),
      [](const ResourceSummary& summary) {
        return summary.type == "director" && summary.name == "bareos-dir";
      });
  ASSERT_NE(resource, model.directors[0].resources.end());

  const ConfigServiceOptions options = MakeServiceOptions(root.path());
  const HttpRequest request{
      "POST",
      "/api/v1/resources/" + resource->id + "/save",
      "text/plain; charset=utf-8",
      "Director {\n  Name = bareos-dir\n  Password = newersecret\n}\n"};

  const auto response = HandleConfigServiceRequest(options, request);

  EXPECT_EQ(response.status_code, 200);
  EXPECT_NE(response.body.find("\"saved\":true"), std::string::npos);
  EXPECT_NE(response.body.find(fd_director_path.string()), std::string::npos);
  EXPECT_NE(response.body.find(sd_director_path.string()), std::string::npos);
  EXPECT_NE(response.body.find(console_config_path.string()),
            std::string::npos);
  EXPECT_NE(response.body.find(console_path.string()), std::string::npos);
  EXPECT_NE(response.body.find(user_path.string()), std::string::npos);

  const auto read_file = [](const std::filesystem::path& path) {
    std::ifstream input(path);
    std::ostringstream output;
    output << input.rdbuf();
    return output.str();
  };

  EXPECT_NE(read_file(director_path).find("Password = newersecret"),
            std::string::npos);
  EXPECT_NE(read_file(fd_director_path).find("Password = newersecret"),
            std::string::npos);
  EXPECT_NE(read_file(sd_director_path).find("Password = newersecret"),
            std::string::npos);
  EXPECT_NE(read_file(console_config_path).find("Password = newersecret"),
            std::string::npos);
  EXPECT_NE(read_file(console_path).find("Password = newersecret"),
            std::string::npos);
  EXPECT_NE(read_file(user_path).find("Password = newersecret"),
            std::string::npos);
  EXPECT_EQ(read_file(console_config_path).find("Password = secret"),
            std::string::npos);
}

TEST(BareosConfigService,
     SavesUpdatedRootDirectorConfigPasswordAndUpdatesBconsoleConfig)
{
  TempConfigRoot root;
  std::filesystem::create_directories(root.path() / "bareos-fd.d/director");
  std::filesystem::create_directories(root.path() / "bareos-sd.d/director");
  std::filesystem::create_directories(root.path() / "bconsole.d/console");
  std::filesystem::create_directories(root.path() / "bconsole.d/user");
  const auto director_config_path = root.path() / "bareos-dir.conf";
  const auto fd_director_path = root.path() / "bareos-fd.d/director/bareos-dir.conf";
  const auto sd_director_path = root.path() / "bareos-sd.d/director/bareos-dir.conf";
  const auto console_config_path = root.path() / "bconsole.conf";
  const auto console_path = root.path() / "bconsole.d/console/admin.conf";
  const auto user_path = root.path() / "bconsole.d/user/api.conf";
  std::ofstream(director_config_path)
      << "Director {\n"
      << "  Name = bareos-dir\n"
      << "  Password = secret\n"
      << "}\n";
  std::ofstream(fd_director_path)
      << "Director {\n"
      << "  Name = bareos-dir\n"
      << "  Password = secret\n"
      << "}\n";
  std::ofstream(sd_director_path)
      << "Director {\n"
      << "  Name = bareos-dir\n"
      << "  Password = secret\n"
      << "}\n";
  std::ofstream(console_config_path)
      << "Director {\n"
      << "  Name = bareos-dir\n"
      << "}\n"
      << "Console {\n"
      << "  Password = secret\n"
      << "}\n";
  std::ofstream(console_path)
      << "Console {\n  Name = admin\n  Password = secret\n}\n";
  std::ofstream(user_path)
      << "User {\n  Name = api\n  Password = secret\n}\n";

  const auto model = DiscoverDatacenterSummary({root.path()});
  const auto resource = std::find_if(
      model.directors[0].resources.begin(), model.directors[0].resources.end(),
      [&director_config_path](const ResourceSummary& summary) {
        return summary.type == "configuration"
               && summary.file_path == director_config_path.string();
      });
  ASSERT_NE(resource, model.directors[0].resources.end());

  const ConfigServiceOptions options = MakeServiceOptions(root.path());
  const HttpRequest request{
      "POST",
      "/api/v1/resources/" + resource->id + "/save",
      "text/plain; charset=utf-8",
      "Director {\n  Name = bareos-dir\n  Password = newersecret\n}\n"};

  const auto response = HandleConfigServiceRequest(options, request);

  EXPECT_EQ(response.status_code, 200) << response.body;
  EXPECT_NE(response.body.find("\"saved\":true"), std::string::npos)
      << response.body;
  EXPECT_NE(response.body.find("\"code\":\"password-reference-update\""),
            std::string::npos)
      << response.body;
  EXPECT_NE(response.body.find(console_config_path.string()), std::string::npos)
      << response.body;
  EXPECT_NE(response.body.find(console_path.string()), std::string::npos)
      << response.body;
  EXPECT_NE(response.body.find(user_path.string()), std::string::npos)
      << response.body;

  const auto read_file = [](const std::filesystem::path& path) {
    std::ifstream input(path);
    std::ostringstream output;
    output << input.rdbuf();
    return output.str();
  };

  EXPECT_NE(read_file(director_config_path).find("Password = newersecret"),
            std::string::npos);
  EXPECT_NE(read_file(fd_director_path).find("Password = newersecret"),
            std::string::npos);
  EXPECT_NE(read_file(sd_director_path).find("Password = newersecret"),
            std::string::npos);
  EXPECT_NE(read_file(console_config_path).find("Password = newersecret"),
            std::string::npos);
  EXPECT_NE(read_file(console_path).find("Password = newersecret"),
            std::string::npos);
  EXPECT_NE(read_file(user_path).find("Password = newersecret"),
            std::string::npos);
  EXPECT_EQ(read_file(console_config_path).find("Password = secret"),
            std::string::npos);
}

TEST(BareosConfigService,
     SavesUpdatedBconsoleConfigPasswordAndUpdatesDirectorAuthResources)
{
  TempConfigRoot root;
  std::filesystem::create_directories(root.path() / "bareos-dir.d/director");
  std::filesystem::create_directories(root.path() / "bareos-fd.d/director");
  std::filesystem::create_directories(root.path() / "bareos-sd.d/director");
  std::filesystem::create_directories(root.path() / "bconsole.d/console");
  std::filesystem::create_directories(root.path() / "bconsole.d/user");
  const auto director_path = root.path() / "bareos-dir.d/director/bareos-dir.conf";
  const auto fd_director_path = root.path() / "bareos-fd.d/director/bareos-dir.conf";
  const auto sd_director_path = root.path() / "bareos-sd.d/director/bareos-dir.conf";
  const auto console_config_path = root.path() / "bconsole.conf";
  const auto console_path = root.path() / "bconsole.d/console/admin.conf";
  const auto user_path = root.path() / "bconsole.d/user/api.conf";
  std::ofstream(root.path() / "bareos-dir.conf") << "Director {}\n";
  std::ofstream(director_path)
      << "Director {\n"
      << "  Name = bareos-dir\n"
      << "  Password = secret\n"
      << "}\n";
  std::ofstream(fd_director_path)
      << "Director {\n"
      << "  Name = bareos-dir\n"
      << "  Password = secret\n"
      << "}\n";
  std::ofstream(sd_director_path)
      << "Director {\n"
      << "  Name = bareos-dir\n"
      << "  Password = secret\n"
      << "}\n";
  std::ofstream(console_config_path)
      << "Director {\n"
      << "  Name = bareos-dir\n"
      << "  Password = secret\n"
      << "}\n"
      << "Console {\n"
      << "  Password = secret\n"
      << "}\n";
  std::ofstream(console_path)
      << "Console {\n  Name = admin\n  Password = secret\n}\n";
  std::ofstream(user_path)
      << "User {\n  Name = api\n  Password = secret\n}\n";

  const auto model = DiscoverDatacenterSummary({root.path()});
  const auto resource = std::find_if(
      model.directors[0].daemons[2].resources.begin(),
      model.directors[0].daemons[2].resources.end(),
      [&console_config_path](const ResourceSummary& summary) {
        return summary.type == "configuration"
               && summary.file_path == console_config_path.string();
      });
  ASSERT_NE(resource, model.directors[0].daemons[2].resources.end());

  const ConfigServiceOptions options = MakeServiceOptions(root.path());
  const HttpRequest request{
      "POST",
      "/api/v1/resources/" + resource->id + "/save",
      "text/plain; charset=utf-8",
      "Director {\n"
      "  Name = bareos-dir\n"
      "  Password = secret\n"
      "}\n"
      "Console {\n"
      "  Password = newersecret\n"
      "}\n"};

  const auto response = HandleConfigServiceRequest(options, request);

  EXPECT_EQ(response.status_code, 200) << response.body;
  EXPECT_NE(response.body.find("\"saved\":true"), std::string::npos)
      << response.body;
  EXPECT_NE(response.body.find(director_path.string()), std::string::npos)
      << response.body;
  EXPECT_NE(response.body.find(fd_director_path.string()), std::string::npos)
      << response.body;
  EXPECT_NE(response.body.find(sd_director_path.string()), std::string::npos)
      << response.body;
  EXPECT_NE(response.body.find(console_config_path.string()), std::string::npos)
      << response.body;
  EXPECT_NE(response.body.find(console_path.string()), std::string::npos)
      << response.body;
  EXPECT_NE(response.body.find(user_path.string()), std::string::npos)
      << response.body;

  const auto read_file = [](const std::filesystem::path& path) {
    std::ifstream input(path);
    std::ostringstream output;
    output << input.rdbuf();
    return output.str();
  };

  EXPECT_NE(read_file(director_path).find("Password = newersecret"),
            std::string::npos);
  EXPECT_NE(read_file(fd_director_path).find("Password = newersecret"),
            std::string::npos);
  EXPECT_NE(read_file(sd_director_path).find("Password = newersecret"),
            std::string::npos);
  EXPECT_NE(read_file(console_config_path).find("Password = newersecret"),
            std::string::npos);
  EXPECT_NE(read_file(console_path).find("Password = newersecret"),
            std::string::npos);
  EXPECT_NE(read_file(user_path).find("Password = newersecret"),
            std::string::npos);
  EXPECT_EQ(read_file(console_config_path).find("Password = secret"),
            std::string::npos);
}

TEST(BareosConfigService, SavesRenamedStorageDeviceAndUpdatesDirectorStorage)
{
  TempConfigRoot root;
  std::filesystem::create_directories(root.path() / "bareos-dir.d/storage");
  std::filesystem::create_directories(root.path() / "bareos-sd.d/storage");
  std::filesystem::create_directories(root.path() / "bareos-sd.d/device");
  std::ofstream(root.path() / "bareos-dir.conf") << "Director {}\n";
  std::ofstream(root.path() / "bareos-sd.conf")
      << "Storage {\n  Name = example-sd\n}\n";
  const auto director_storage_path
      = root.path() / "bareos-dir.d/storage/example-sd.conf";
  const auto device_path = root.path() / "bareos-sd.d/device/tape.conf";
  std::ofstream(director_storage_path)
      << "Storage {\n"
      << "  Name = example-sd\n"
      << "  Address = 10.0.0.20\n"
      << "  Password = secret\n"
      << "  Device = tape-drive-0\n"
      << "  MediaType = File\n"
      << "}\n";
  std::ofstream(root.path() / "bareos-sd.d/storage/example-sd.conf")
      << "Storage {\n"
      << "  Name = example-sd\n"
      << "  Device = tape-drive-0\n"
      << "  MediaType = File\n"
      << "}\n";
  std::ofstream(device_path)
      << "Device {\n"
      << "  Name = tape-drive-0\n"
      << "  ArchiveDevice = /tmp/tape-drive-0\n"
      << "  MediaType = File\n"
      << "}\n";

  const auto model = DiscoverDatacenterSummary({root.path()});
  const auto& daemon_resources = model.directors[0].daemons[0].resources;
  const auto resource = std::find_if(
      daemon_resources.begin(), daemon_resources.end(),
      [](const ResourceSummary& summary) {
        return summary.type == "device" && summary.name == "tape-drive-0";
      });
  ASSERT_NE(resource, daemon_resources.end());

  const ConfigServiceOptions options = MakeServiceOptions(root.path());
  const HttpRequest request{
      "POST",
      "/api/v1/resources/" + resource->id + "/save",
      "text/plain; charset=utf-8",
      "Device {\n  Name = tape-drive-1\n  ArchiveDevice = /tmp/tape-drive-0\n  MediaType = File\n}\n"};

  const auto response = HandleConfigServiceRequest(options, request);

  EXPECT_EQ(response.status_code, 200);
  EXPECT_NE(response.body.find("\"saved\":true"), std::string::npos);
  EXPECT_NE(response.body.find(director_storage_path.string()), std::string::npos);

  const auto director_storage_content = [&director_storage_path] {
    std::ifstream input(director_storage_path);
    std::ostringstream output;
    output << input.rdbuf();
    return output.str();
  }();
  const auto device_content = [&device_path] {
    std::ifstream input(device_path);
    std::ostringstream output;
    output << input.rdbuf();
    return output.str();
  }();

  EXPECT_NE(device_content.find("Name = tape-drive-1"), std::string::npos);
  EXPECT_NE(director_storage_content.find("Device = tape-drive-1"),
            std::string::npos);
  EXPECT_EQ(director_storage_content.find("Device = tape-drive-0"),
            std::string::npos);
}

TEST(BareosConfigService, SavesRenamedDirectorDeviceAndUpdatesStorageDaemon)
{
  TempConfigRoot root;
  std::filesystem::create_directories(root.path() / "bareos-dir.d/storage");
  std::filesystem::create_directories(root.path() / "bareos-sd.d/storage");
  std::filesystem::create_directories(root.path() / "bareos-sd.d/device");
  std::ofstream(root.path() / "bareos-dir.conf") << "Director {}\n";
  std::ofstream(root.path() / "bareos-sd.conf")
      << "Storage {\n  Name = example-sd\n}\n";
  const auto director_storage_path
      = root.path() / "bareos-dir.d/storage/example-sd.conf";
  const auto device_path = root.path() / "bareos-sd.d/device/tape.conf";
  std::ofstream(director_storage_path)
      << "Storage {\n"
      << "  Name = example-sd\n"
      << "  Address = 10.0.0.20\n"
      << "  Password = secret\n"
      << "  Device = tape-drive-0\n"
      << "  MediaType = File\n"
      << "}\n";
  std::ofstream(root.path() / "bareos-sd.d/storage/example-sd.conf")
      << "Storage {\n"
      << "  Name = example-sd\n"
      << "  Device = tape-drive-0\n"
      << "  MediaType = File\n"
      << "}\n";
  std::ofstream(device_path)
      << "Device {\n"
      << "  Name = tape-drive-0\n"
      << "  ArchiveDevice = /tmp/tape-drive-0\n"
      << "  MediaType = File\n"
      << "}\n";

  const auto model = DiscoverDatacenterSummary({root.path()});
  const auto resource = std::find_if(
      model.directors[0].resources.begin(), model.directors[0].resources.end(),
      [](const ResourceSummary& summary) { return summary.type == "storage"; });
  ASSERT_NE(resource, model.directors[0].resources.end());

  const ConfigServiceOptions options = MakeServiceOptions(root.path());
  const HttpRequest request{
      "POST",
      "/api/v1/resources/" + resource->id + "/save",
      "text/plain; charset=utf-8",
      "Storage {\n  Name = example-sd\n  Address = 10.0.0.20\n  Password = secret\n  Device = tape-drive-1\n  MediaType = File\n}\n"};

  const auto response = HandleConfigServiceRequest(options, request);

  EXPECT_EQ(response.status_code, 200);
  EXPECT_NE(response.body.find("\"saved\":true"), std::string::npos);
  EXPECT_NE(response.body.find(device_path.string()), std::string::npos);

  const auto director_storage_content = [&director_storage_path] {
    std::ifstream input(director_storage_path);
    std::ostringstream output;
    output << input.rdbuf();
    return output.str();
  }();
  const auto device_content = [&device_path] {
    std::ifstream input(device_path);
    std::ostringstream output;
    output << input.rdbuf();
    return output.str();
  }();

  EXPECT_NE(director_storage_content.find("Device = tape-drive-1"),
            std::string::npos);
  EXPECT_NE(device_content.find("Name = tape-drive-1"), std::string::npos);
  EXPECT_EQ(device_content.find("Name = tape-drive-0"), std::string::npos);
}

TEST(BareosConfigService, SavesRenamedDirectorAndUpdatesDaemonConfigs)
{
  TempConfigRoot root;
  std::filesystem::create_directories(root.path() / "bareos-dir.d/director");
  std::filesystem::create_directories(root.path() / "bareos-fd.d/director");
  std::filesystem::create_directories(root.path() / "bareos-sd.d/director");
  std::filesystem::create_directories(root.path() / "bconsole.d/console");
  std::ofstream(root.path() / "bareos-dir.conf") << "Director {}\n";
  const auto director_path = root.path() / "bareos-dir.d/director/bareos-dir.conf";
  const auto fd_director_path = root.path() / "bareos-fd.d/director/bareos-dir.conf";
  const auto sd_director_path = root.path() / "bareos-sd.d/director/bareos-dir.conf";
  const auto console_config_path = root.path() / "bconsole.conf";
  std::ofstream(director_path)
      << "Director {\n"
      << "  Name = bareos-dir\n"
      << "  Password = secret\n"
      << "}\n";
  std::ofstream(fd_director_path)
      << "Director {\n"
      << "  Name = bareos-dir\n"
      << "  Password = secret\n"
      << "}\n";
  std::ofstream(sd_director_path)
      << "Director {\n"
      << "  Name = bareos-dir\n"
      << "  Password = secret\n"
      << "}\n";
  std::ofstream(console_config_path)
      << "Director {\n"
      << "  Name = bareos-dir\n"
      << "}\n"
      << "Console {\n"
      << "  Name = admin\n"
      << "}\n";
  std::ofstream(root.path() / "bconsole.d/console/admin.conf")
      << "Console {\n  Name = admin\n}\n";

  const auto model = DiscoverDatacenterSummary({root.path()});
  const auto resource = std::find_if(
      model.directors[0].resources.begin(), model.directors[0].resources.end(),
      [](const ResourceSummary& summary) {
        return summary.type == "director" && summary.name == "bareos-dir";
      });
  ASSERT_NE(resource, model.directors[0].resources.end());

  const ConfigServiceOptions options = MakeServiceOptions(root.path());
  const HttpRequest request{
      "POST",
      "/api/v1/resources/" + resource->id + "/save",
      "text/plain; charset=utf-8",
      "Director {\n  Name = renamed-dir\n  Password = secret\n}\n"};

  const auto response = HandleConfigServiceRequest(options, request);

  EXPECT_EQ(response.status_code, 200);
  EXPECT_NE(response.body.find("\"saved\":true"), std::string::npos);
  const auto renamed_director_path
      = root.path() / "bareos-dir.d/director/renamed-dir.conf";
  const auto renamed_fd_director_path
      = root.path() / "bareos-fd.d/director/renamed-dir.conf";
  const auto renamed_sd_director_path
      = root.path() / "bareos-sd.d/director/renamed-dir.conf";
  EXPECT_NE(response.body.find(renamed_fd_director_path.string()), std::string::npos);
  EXPECT_NE(response.body.find(renamed_sd_director_path.string()), std::string::npos);
  EXPECT_NE(response.body.find(console_config_path.string()), std::string::npos);

  const auto read_file = [](const std::filesystem::path& path) {
    std::ifstream input(path);
    std::ostringstream output;
    output << input.rdbuf();
    return output.str();
  };

  ASSERT_FALSE(std::filesystem::exists(director_path));
  ASSERT_FALSE(std::filesystem::exists(fd_director_path));
  ASSERT_FALSE(std::filesystem::exists(sd_director_path));
  ASSERT_TRUE(std::filesystem::exists(renamed_director_path));
  ASSERT_TRUE(std::filesystem::exists(renamed_fd_director_path));
  ASSERT_TRUE(std::filesystem::exists(renamed_sd_director_path));

  const auto director_content = read_file(renamed_director_path);
  const auto fd_director_content = read_file(renamed_fd_director_path);
  const auto sd_director_content = read_file(renamed_sd_director_path);
  const auto console_content = read_file(console_config_path);

  EXPECT_NE(director_content.find("Name = renamed-dir"), std::string::npos);
  EXPECT_NE(fd_director_content.find("Name = renamed-dir"), std::string::npos);
  EXPECT_NE(sd_director_content.find("Name = renamed-dir"), std::string::npos);
  EXPECT_NE(console_content.find("Name = renamed-dir"), std::string::npos);
  EXPECT_EQ(fd_director_content.find("Name = bareos-dir"), std::string::npos);
  EXPECT_EQ(sd_director_content.find("Name = bareos-dir"), std::string::npos);
  EXPECT_EQ(console_content.find("Name = bareos-dir"), std::string::npos);
}

TEST(BareosConfigService,
     SavesRenamedRootDirectorConfigAndUpdatesDaemonConfigsAndBconsole)
{
  TempConfigRoot root;
  std::filesystem::create_directories(root.path() / "bareos-fd.d/director");
  std::filesystem::create_directories(root.path() / "bareos-sd.d/director");
  std::filesystem::create_directories(root.path() / "bconsole.d/console");
  const auto director_config_path = root.path() / "bareos-dir.conf";
  const auto fd_director_path = root.path() / "bareos-fd.d/director/bareos-dir.conf";
  const auto sd_director_path = root.path() / "bareos-sd.d/director/bareos-dir.conf";
  const auto console_config_path = root.path() / "bconsole.conf";
  std::ofstream(director_config_path)
      << "Director {\n"
      << "  Name = bareos-dir\n"
      << "  Password = secret\n"
      << "}\n";
  std::ofstream(fd_director_path)
      << "Director {\n"
      << "  Name = bareos-dir\n"
      << "  Password = secret\n"
      << "}\n";
  std::ofstream(sd_director_path)
      << "Director {\n"
      << "  Name = bareos-dir\n"
      << "  Password = secret\n"
      << "}\n";
  std::ofstream(console_config_path)
      << "Director {\n"
      << "  Name = bareos-dir\n"
      << "  Password = secret\n"
      << "}\n"
      << "Console {\n"
      << "  Name = admin\n"
      << "}\n";
  std::ofstream(root.path() / "bconsole.d/console/admin.conf")
      << "Console {\n  Name = admin\n}\n";

  const auto model = DiscoverDatacenterSummary({root.path()});
  const auto resource = std::find_if(
      model.directors[0].resources.begin(), model.directors[0].resources.end(),
      [&director_config_path](const ResourceSummary& summary) {
        return summary.type == "configuration"
               && summary.file_path == director_config_path.string();
      });
  ASSERT_NE(resource, model.directors[0].resources.end());

  const ConfigServiceOptions options = MakeServiceOptions(root.path());
  const HttpRequest request{
      "POST",
      "/api/v1/resources/" + resource->id + "/save",
      "text/plain; charset=utf-8",
      "Director {\n  Name = renamed-dir\n  Password = secret\n}\n"};

  const auto response = HandleConfigServiceRequest(options, request);

  EXPECT_EQ(response.status_code, 200) << response.body;
  EXPECT_NE(response.body.find("\"saved\":true"), std::string::npos)
      << response.body;
  EXPECT_NE(response.body.find(console_config_path.string()), std::string::npos)
      << response.body;

  const auto renamed_fd_director_path
      = root.path() / "bareos-fd.d/director/renamed-dir.conf";
  const auto renamed_sd_director_path
      = root.path() / "bareos-sd.d/director/renamed-dir.conf";
  const auto renamed_director_config_path = root.path() / "renamed-dir.conf";

  const auto read_file = [](const std::filesystem::path& path) {
    std::ifstream input(path);
    std::ostringstream output;
    output << input.rdbuf();
    return output.str();
  };

  ASSERT_FALSE(std::filesystem::exists(director_config_path));
  ASSERT_FALSE(std::filesystem::exists(fd_director_path));
  ASSERT_FALSE(std::filesystem::exists(sd_director_path));
  ASSERT_TRUE(std::filesystem::exists(renamed_director_config_path));
  ASSERT_TRUE(std::filesystem::exists(renamed_fd_director_path));
  ASSERT_TRUE(std::filesystem::exists(renamed_sd_director_path));

  EXPECT_NE(read_file(renamed_director_config_path).find("Name = renamed-dir"),
            std::string::npos);
  EXPECT_NE(read_file(renamed_fd_director_path).find("Name = renamed-dir"),
            std::string::npos);
  EXPECT_NE(read_file(renamed_sd_director_path).find("Name = renamed-dir"),
            std::string::npos);
  EXPECT_NE(read_file(console_config_path).find("Name = renamed-dir"),
            std::string::npos);
  EXPECT_EQ(read_file(console_config_path).find("Name = bareos-dir"),
            std::string::npos);
}

TEST(BareosConfigService, SavesRenamedStorageResourceToRenamedFile)
{
  TempConfigRoot root;
  std::filesystem::create_directories(root.path() / "bareos-dir.d/storage");
  std::ofstream(root.path() / "bareos-dir.conf") << "Director {}\n";
  const auto storage_path = root.path() / "bareos-dir.d/storage/example-sd.conf";
  std::ofstream(storage_path)
      << "Storage {\n"
      << "  Name = example-sd\n"
      << "  Address = 10.0.0.20\n"
      << "  Password = secret\n"
      << "  Device = tape-drive-0\n"
      << "  MediaType = File\n"
      << "}\n";

  const auto model = DiscoverDatacenterSummary({root.path()});
  const auto resource = std::find_if(
      model.directors[0].resources.begin(), model.directors[0].resources.end(),
      [](const ResourceSummary& summary) { return summary.type == "storage"; });
  ASSERT_NE(resource, model.directors[0].resources.end());

  const ConfigServiceOptions options = MakeServiceOptions(root.path());
  const HttpRequest request{
      "POST",
      "/api/v1/resources/" + resource->id + "/save",
      "text/plain; charset=utf-8",
      "Storage {\n  Name = renamed-sd\n  Address = 10.0.0.20\n  Password = secret\n  Device = tape-drive-0\n  MediaType = File\n}\n"};

  const auto response = HandleConfigServiceRequest(options, request);

  EXPECT_EQ(response.status_code, 200);
  EXPECT_NE(response.body.find("\"saved\":true"), std::string::npos);

  const auto renamed_storage_path
      = root.path() / "bareos-dir.d/storage/renamed-sd.conf";
  ASSERT_FALSE(std::filesystem::exists(storage_path));
  ASSERT_TRUE(std::filesystem::exists(renamed_storage_path));

  std::ifstream input(renamed_storage_path);
  std::ostringstream output;
  output << input.rdbuf();
  EXPECT_NE(output.str().find("Name = renamed-sd"), std::string::npos);
}

TEST(BareosConfigService, SavesOnlySelectedBlockFromMultiResourceFile)
{
  TempConfigRoot root;
  std::filesystem::create_directories(root.path() / "bareos-dir.d/client");
  std::ofstream(root.path() / "bareos-dir.conf") << "Director {}\n";
  const auto resource_path = root.path() / "bareos-dir.d/client/two.conf";
  std::ofstream(resource_path)
      << "Client {\n  Name = first-fd\n  Address = 10.0.0.10\n}\n"
      << "Client {\n  Name = second-fd\n  Address = 10.0.0.11\n}\n";

  const ConfigServiceOptions options = MakeServiceOptions(root.path());
  const auto model = DiscoverDatacenterSummary({root.path()});
  const auto resource = std::find_if(
      model.directors[0].resources.begin(), model.directors[0].resources.end(),
      [](const ResourceSummary& summary) { return summary.name == "second-fd"; });
  ASSERT_NE(resource, model.directors[0].resources.end());
  const HttpRequest request{
      "POST",
      "/api/v1/resources/" + resource->id + "/save",
      "text/plain; charset=utf-8",
      "Client {\n  Name = second-fd\n  Address = 10.0.0.22\n  Password = secret\n}\n"};

  const auto response = HandleConfigServiceRequest(options, request);

  EXPECT_EQ(response.status_code, 200);

  const auto saved_content = [&resource_path] {
    std::ifstream input(resource_path);
    std::ostringstream output;
    output << input.rdbuf();
    return output.str();
  }();
  EXPECT_NE(saved_content.find("Name = first-fd\n  Address = 10.0.0.10"),
            std::string::npos);
  EXPECT_NE(saved_content.find("Name = second-fd\n  Address = 10.0.0.22"),
            std::string::npos);
  EXPECT_NE(saved_content.find("Password = secret"), std::string::npos);
}

TEST(BareosConfigService, SavesNewResourceContent)
{
  TempConfigRoot root;
  std::filesystem::create_directories(root.path() / "bareos-dir.d/client");
  std::ofstream(root.path() / "bareos-dir.conf") << "Director {}\n";
  std::ofstream(root.path() / "bareos-dir.d/client/example-fd.conf")
      << "Client {\n  Name = example-fd\n}\n";

  const ConfigServiceOptions options = MakeServiceOptions(root.path());
  const HttpRequest request{
      "POST",
      "/api/v1/nodes/director-0/new-resource/client/save",
      "text/plain; charset=utf-8",
      "Client {\n  Name = new-fd\n  Address = 10.0.0.30\n  Password = secret\n}\n"};

  const auto response = HandleConfigServiceRequest(options, request);

  EXPECT_EQ(response.status_code, 200);
  EXPECT_NE(response.body.find("\"saved\":true"), std::string::npos);
  EXPECT_NE(response.body.find("\"action\":\"create\""), std::string::npos);

  const auto saved_path = root.path() / "bareos-dir.d/client/new-fd.conf";
  ASSERT_TRUE(std::filesystem::exists(saved_path));
  const auto saved_content = [&saved_path] {
    std::ifstream input(saved_path);
    std::ostringstream output;
    output << input.rdbuf();
    return output.str();
  }();
  EXPECT_NE(saved_content.find("Name = new-fd"), std::string::npos);
  EXPECT_NE(saved_content.find("Address = 10.0.0.30"), std::string::npos);
}

TEST(BareosConfigService, RollsBackSaveWhenDaemonVerificationFails)
{
  TempConfigRoot root;
  std::filesystem::create_directories(root.path() / "bareos-dir.d/client");
  std::ofstream(root.path() / "bareos-dir.conf") << "Director {}\n";
  const auto resource_path = root.path() / "bareos-dir.d/client/example-fd.conf";
  std::ofstream(resource_path)
      << "Client {\n  Name = example-fd\n  Address = 10.0.0.10\n}\n";

  const ConfigServiceOptions options
      = MakeServiceOptions(root.path(), "/bin/false", "/bin/true", "/bin/true");
  const HttpRequest request{
      "POST",
      "/api/v1/resources/resource-0-director-0/save",
      "text/plain; charset=utf-8",
      "Client {\n  Name = example-fd\n  Address = 10.0.0.20\n  Password = secret\n}\n"};

  const auto response = HandleConfigServiceRequest(options, request);

  EXPECT_EQ(response.status_code, 409);
  EXPECT_NE(response.body.find("\"saved\":false"), std::string::npos);
  EXPECT_NE(response.body.find("\"verification_command\":\"/bin/false -t -c "),
            std::string::npos);
  EXPECT_NE(response.body.find("Daemon configuration verification failed"),
            std::string::npos);

  const auto saved_content = [&resource_path] {
    std::ifstream input(resource_path);
    std::ostringstream output;
    output << input.rdbuf();
    return output.str();
  }();
  EXPECT_NE(saved_content.find("Address = 10.0.0.10"), std::string::npos);
  EXPECT_EQ(saved_content.find("Password = secret"), std::string::npos);
}

TEST(BareosConfigService, ReturnsNodeRelationships)
{
  TempConfigRoot root;
  std::filesystem::create_directories(root.path() / "bareos-dir.d/client");
  std::filesystem::create_directories(root.path() / "bareos-dir.d/storage");
  std::filesystem::create_directories(root.path() / "bareos-fd.d/client");
  std::filesystem::create_directories(root.path() / "bareos-fd.d/director");
  std::filesystem::create_directories(root.path() / "bareos-sd.d/director");
  std::filesystem::create_directories(root.path() / "bareos-sd.d/storage");
  std::filesystem::create_directories(root.path() / "bareos-fd.d/fileset");
  std::filesystem::create_directories(root.path() / "bareos-sd.d/device");
  std::ofstream(root.path() / "bareos-dir.conf") << "Director {}\n";
  std::ofstream(root.path() / "bareos-fd.conf")
      << "FileDaemon {\n  Name = example-fd\n}\n";
  std::ofstream(root.path() / "bareos-sd.conf")
      << "Storage {\n  Name = example-sd\n}\n";
  std::ofstream(root.path() / "bareos-dir.d/client/not-the-name.conf")
      << "Client {\n  Name = example-fd\n}\n";
  std::ofstream(root.path() / "bareos-dir.d/storage/not-the-storage-name.conf")
      << "Storage {\n  Name = example-sd\n}\n";
  std::ofstream(root.path() / "bareos-fd.d/client/example-fd.conf")
      << "Client {\n  Name = example-fd\n}\n";
  std::ofstream(root.path() / "bareos-fd.d/director/bareos-dir.conf")
      << "Director {\n  Name = bareos-dir\n}\n";
  std::ofstream(root.path() / "bareos-sd.d/director/bareos-dir.conf")
      << "Director {\n  Name = bareos-dir\n}\n";
  std::ofstream(root.path() / "bareos-sd.d/storage/example-sd.conf")
      << "Storage {\n  Name = example-sd\n}\n";
  std::ofstream(root.path() / "bareos-fd.d/fileset/local.conf") << "FileSet {}\n";
  std::ofstream(root.path() / "bareos-sd.d/device/tape.conf") << "Device {}\n";

  const ConfigServiceOptions options = MakeServiceOptions(root.path());
  const HttpRequest request{
      "GET", "/api/v1/nodes/director-0/relationships", "", ""};

  const auto response = HandleConfigServiceRequest(options, request);

  EXPECT_EQ(response.status_code, 200);
  EXPECT_NE(response.body.find("\"relation\":\"resource-name\""), std::string::npos);
  EXPECT_NE(response.body.find("\"from_node_id\":\"director-0\""),
            std::string::npos);
  EXPECT_NE(response.body.find("\"to_node_id\":\"daemon-0-file-daemon\""),
            std::string::npos);
  EXPECT_NE(response.body.find("\"to_node_id\":\"daemon-0-storage-daemon\""),
            std::string::npos);
  EXPECT_NE(response.body.find("\"endpoint_name\":\"example-fd\""),
            std::string::npos);
  EXPECT_NE(response.body.find("\"source_resource_type\":\"client\""),
            std::string::npos);
  EXPECT_NE(response.body.find("\"source_resource_name\":\"example-fd\""),
            std::string::npos);
  EXPECT_NE(response.body.find("\"target_resource_type\":\"client\""),
            std::string::npos);
  EXPECT_NE(response.body.find("\"target_resource_name\":\"example-fd\""),
            std::string::npos);
  EXPECT_NE(response.body.find((root.path() / "bareos-fd.d/client/example-fd.conf").string()),
            std::string::npos);
  EXPECT_NE(response.body.find("\"resolution\":\"Resolved to client resource example-fd in bareos-fd.\""),
            std::string::npos);
}

TEST(BareosConfigService, ReturnsStorageDeviceRelationships)
{
  TempConfigRoot root;
  std::filesystem::create_directories(root.path() / "bareos-dir.d/storage");
  std::filesystem::create_directories(root.path() / "bareos-sd.d/storage");
  std::filesystem::create_directories(root.path() / "bareos-sd.d/device");
  std::ofstream(root.path() / "bareos-dir.conf") << "Director {}\n";
  std::ofstream(root.path() / "bareos-sd.conf")
      << "Storage {\n  Name = example-sd\n}\n";
  const auto director_storage_path
      = root.path() / "bareos-dir.d/storage/example-sd.conf";
  const auto device_path = root.path() / "bareos-sd.d/device/tape.conf";
  std::ofstream(director_storage_path)
      << "Storage {\n"
      << "  Name = example-sd\n"
      << "  Address = 10.0.0.20\n"
      << "  Password = secret\n"
      << "  Device = tape-drive-0\n"
      << "  MediaType = File\n"
      << "}\n";
  std::ofstream(root.path() / "bareos-sd.d/storage/example-sd.conf")
      << "Storage {\n"
      << "  Name = example-sd\n"
      << "  Device = tape-drive-0\n"
      << "  MediaType = File\n"
      << "}\n";
  std::ofstream(device_path)
      << "Device {\n"
      << "  Name = tape-drive-0\n"
      << "  ArchiveDevice = /tmp/tape-drive-0\n"
      << "  MediaType = File\n"
      << "}\n";

  const ConfigServiceOptions options = MakeServiceOptions(root.path());
  const HttpRequest request{
      "GET", "/api/v1/nodes/daemon-0-storage-daemon/relationships", "", ""};

  const auto response = HandleConfigServiceRequest(options, request);

  EXPECT_EQ(response.status_code, 200);
  EXPECT_NE(response.body.find("\"relation\":\"device\""), std::string::npos);
  EXPECT_NE(response.body.find("\"endpoint_name\":\"tape-drive-0\""),
            std::string::npos);
  EXPECT_NE(response.body.find("\"source_resource_path\":"
                               + std::string("\"")
                               + director_storage_path.string()),
            std::string::npos);
  EXPECT_NE(response.body.find("\"source_resource_type\":\"storage\""),
            std::string::npos);
  EXPECT_NE(response.body.find("\"source_resource_name\":\"example-sd\""),
            std::string::npos);
  EXPECT_NE(response.body.find("\"from_node_id\":\"director-0\""),
            std::string::npos);
  EXPECT_NE(response.body.find("\"target_resource_path\":"
                               + std::string("\"")
                               + device_path.string()),
            std::string::npos);
  EXPECT_NE(response.body.find("\"target_resource_type\":\"device\""),
            std::string::npos);
  EXPECT_NE(response.body.find("\"target_resource_name\":\"tape-drive-0\""),
            std::string::npos);
  EXPECT_NE(response.body.find("\"to_node_id\":\"daemon-0-storage-daemon\""),
            std::string::npos);
  EXPECT_NE(response.body.find("\"target_resource_id\":\"resource-0-storage-daemon"),
            std::string::npos);
}

TEST(BareosConfigService, ShowsKnownRemoteClientsInDatacenterResources)
{
  TempConfigRoot root;
  std::filesystem::create_directories(root.path() / "bareos-dir.d/client");
  std::filesystem::create_directories(root.path() / "bareos-fd.d/director");
  std::ofstream(root.path() / "bareos-dir.conf") << "Director {}\n";
  std::ofstream(root.path() / "bareos-fd.conf")
      << "FileDaemon {\n  Name = local-fd\n}\n";
  std::ofstream(root.path() / "bareos-dir.d/client/local-fd.conf")
      << "Client {\n  Name = local-fd\n}\n";
  std::ofstream(root.path() / "bareos-dir.d/client/remote-fd.conf")
      << "Client {\n  Name = remote-fd\n  Password = secret\n}\n";
  const auto generated_path = root.path()
                              / "generated/clients/remote-fd/etc/bareos/bareos-fd.d/director/bareos-dir.conf";
  std::filesystem::create_directories(generated_path.parent_path());
  std::ofstream(generated_path)
      << "Director {\n  Name = bareos-dir\n  Password = secret\n}\n";

  const ConfigServiceOptions options = MakeServiceOptions(root.path());
  const HttpRequest request{"GET", "/api/v1/nodes/datacenter/resources", "", ""};

  const auto response = HandleConfigServiceRequest(options, request);

  EXPECT_EQ(response.status_code, 200);
  EXPECT_NE(response.body.find("\"name\":\"remote-fd\""), std::string::npos);
  EXPECT_NE(response.body.find("\"name\":\"remote-fd generated client config\""),
            std::string::npos);
  EXPECT_NE(response.body.find(generated_path.string()), std::string::npos);
  EXPECT_EQ(response.body.find("\"name\":\"local-fd\""), std::string::npos);

  const HttpRequest editor_request{
      "GET",
      "/api/v1/resources/generated-client-config-director-0-remote-fd/editor",
      "",
      ""};
  const auto editor_response = HandleConfigServiceRequest(options, editor_request);
  EXPECT_EQ(editor_response.status_code, 200);
  EXPECT_NE(editor_response.body.find(generated_path.string()), std::string::npos);
  EXPECT_NE(editor_response.body.find("Name = bareos-dir"), std::string::npos);

  const HttpRequest node_resources_request{
      "GET", "/api/v1/nodes/known-client-0-remote-fd/resources", "", ""};
  const auto node_resources_response
      = HandleConfigServiceRequest(options, node_resources_request);
  EXPECT_EQ(node_resources_response.status_code, 200);
  EXPECT_NE(node_resources_response.body.find("\"name\":\"remote-fd\""),
            std::string::npos);
  EXPECT_NE(node_resources_response.body.find(
                "\"name\":\"remote-fd generated client config\""),
            std::string::npos);
  EXPECT_NE(node_resources_response.body.find(generated_path.string()),
            std::string::npos);
}

TEST(BareosConfigService, KeepsKnownClientRelationshipsAndOmitsMissingReferences)
{
  TempConfigRoot root;
  std::filesystem::create_directories(root.path() / "bareos-dir.d/client");
  std::filesystem::create_directories(root.path() / "bareos-dir.d/job");
  std::ofstream(root.path() / "bareos-dir.conf") << "Director {}\n";
  std::ofstream(root.path() / "bareos-fd.conf")
      << "FileDaemon {\n  Name = different-fd\n}\n";
  std::ofstream(root.path() / "bareos-dir.d/client/example-fd.conf")
      << "Client {\n  Name = example-fd\n}\n";
  std::ofstream(root.path() / "bareos-dir.d/job/example-job.conf")
      << "Job {\n"
      << "  Name = BackupClient\n"
      << "  FileSet = MissingFileSet\n"
      << "}\n";

  const ConfigServiceOptions options = MakeServiceOptions(root.path());
  const HttpRequest request{
      "GET", "/api/v1/nodes/director-0/relationships", "", ""};

  const auto response = HandleConfigServiceRequest(options, request);

  EXPECT_EQ(response.status_code, 200);
  EXPECT_NE(response.body.find("\"relation\":\"resource-name\""),
            std::string::npos);
  EXPECT_NE(response.body.find("\"endpoint_name\":\"example-fd\""),
            std::string::npos);
  EXPECT_NE(response.body.find("\"to_node_id\":\"known-client-0-example-fd\""),
            std::string::npos);
  EXPECT_EQ(response.body.find("\"endpoint_name\":\"MissingFileSet\""),
            std::string::npos);
}

TEST(BareosConfigService, RemovesRemoteClientAndGeneratedConfig)
{
  TempConfigRoot root;
  std::filesystem::create_directories(root.path() / "bareos-dir.d/client");
  std::ofstream(root.path() / "bareos-dir.conf") << "Director {}\n";
  const auto client_path = root.path() / "bareos-dir.d/client/remote-fd.conf";
  std::ofstream(client_path)
      << "Client {\n  Name = remote-fd\n  Address = 10.0.0.40\n}\n";
  const auto generated_path = root.path()
                              / "generated/clients/remote-fd/etc/bareos/bareos-fd.d/director/bareos-dir.conf";
  std::filesystem::create_directories(generated_path.parent_path());
  std::ofstream(generated_path)
      << "Director {\n  Name = bareos-dir\n  Password = secret\n}\n";

  const ConfigServiceOptions options = MakeServiceOptions(root.path());
  const HttpRequest request{"POST", "/api/v1/nodes/known-client-0-remote-fd/remove",
                            "", ""};

  const auto response = HandleConfigServiceRequest(options, request);

  EXPECT_EQ(response.status_code, 200);
  EXPECT_NE(response.body.find("\"removed\":true"), std::string::npos);
  EXPECT_NE(response.body.find("\"action\":\"delete\""), std::string::npos);
  EXPECT_FALSE(std::filesystem::exists(client_path));
  EXPECT_FALSE(std::filesystem::exists(generated_path));

  const auto model = DiscoverDatacenterSummary({root.path()});
  EXPECT_EQ(FindTreeNodeById(model, "known-client-0-remote-fd"), nullptr);
}

TEST(BareosConfigService, ReturnsAddClientWizardPreview)
{
  TempConfigRoot root;
  std::filesystem::create_directories(root.path() / "bareos-dir.d/client");
  std::ofstream(root.path() / "bareos-dir.conf") << "Director {}\n";

  const ConfigServiceOptions options = MakeServiceOptions(root.path());
  const HttpRequest request{"POST",
                            "/api/v1/nodes/datacenter/add-client/preview/director-0",
                            "text/plain; charset=utf-8",
                            "Name=wizard-fd\nAddress=10.0.0.40\nPassword=secret\nCatalog=MyCatalog\n"};

  const auto response = HandleConfigServiceRequest(options, request);

  EXPECT_EQ(response.status_code, 200);
  EXPECT_NE(response.body.find("\"wizard\":\"add-client\""), std::string::npos);
  EXPECT_NE(response.body.find("\"operations\":[{"), std::string::npos);
  EXPECT_NE(response.body.find("\"role\":\"director-client\""), std::string::npos);
  EXPECT_NE(response.body.find("\"role\":\"client-file-daemon-director\""),
            std::string::npos);
  EXPECT_NE(response.body.find("\"action\":\"create\""), std::string::npos);
  EXPECT_EQ(response.body.find("\"action\":\"create-or-update\""),
            std::string::npos);
  EXPECT_NE(response.body.find("\"type\":\"client\""), std::string::npos);
  EXPECT_NE(response.body.find("\"type\":\"director\""), std::string::npos);
  EXPECT_NE(response.body.find("\"director_name\":\"bareos-dir\""),
            std::string::npos);
  EXPECT_NE(response.body.find("Name = bareos-dir"), std::string::npos);
  EXPECT_NE(response.body.find("wizard-fd.conf"), std::string::npos);
  EXPECT_NE(response.body.find("Address = 10.0.0.40"), std::string::npos);
  EXPECT_NE(response.body.find(
                (root.path() / "generated/directors/bareos-dir/etc/bareos/bareos-dir.d/client/wizard-fd.conf")
                    .string()),
            std::string::npos);
  EXPECT_NE(response.body.find(
                (root.path() / "generated/clients/wizard-fd/etc/bareos/bareos-fd.d/director/bareos-dir.conf")
                    .string()),
            std::string::npos);
  EXPECT_NE(response.body.find("\"validation_messages\":[]"), std::string::npos);
}

TEST(BareosConfigService, SavesAddClientWizardArtifactsBelowStagingRoot)
{
  TempConfigRoot root;
  std::filesystem::create_directories(root.path() / "bareos-dir.d/client");
  std::filesystem::create_directories(root.path() / "bareos-fd.d/director");
  std::filesystem::create_directories(root.path() / "bareos-dir.d/messages");
  std::filesystem::create_directories(root.path() / "bareos-fd.d/messages");
  std::ofstream(root.path() / "bareos-dir.conf") << "Director {}\n";
  std::ofstream(root.path() / "bareos-fd.conf")
      << "FileDaemon {\n  Name = wizard-fd\n}\n";
  std::ofstream(root.path() / "bareos-dir.d/messages/Standard.conf")
      << "Messages {\n  Name = Standard\n}\n";
  std::ofstream(root.path() / "bareos-fd.d/messages/Standard.conf")
      << "Messages {\n  Name = Standard\n}\n";

  const ConfigServiceOptions options = MakeServiceOptions(root.path());
  const HttpRequest request{"POST",
                            "/api/v1/nodes/datacenter/add-client/save/director-0",
                            "text/plain; charset=utf-8",
                            "Name=wizard-fd\nAddress=10.0.0.40\nPassword=secret\n"};

  const auto response = HandleConfigServiceRequest(options, request);

  EXPECT_EQ(response.status_code, 200);
  EXPECT_NE(response.body.find("\"saved\":true"), std::string::npos);
  EXPECT_NE(response.body.find("\"role\":\"director-client\""),
            std::string::npos);
  EXPECT_NE(response.body.find("\"role\":\"client-file-daemon-director\""),
            std::string::npos);

  const auto director_path = root.path()
                             / "generated/directors/bareos-dir/etc/bareos/bareos-dir.d/client/wizard-fd.conf";
  const auto file_daemon_path = root.path()
                                / "generated/clients/wizard-fd/etc/bareos/bareos-fd.d/director/bareos-dir.conf";
  const auto staged_director_main
      = root.path() / "generated/directors/bareos-dir/etc/bareos/bareos-dir.conf";
  const auto staged_director_messages = root.path()
                                        / "generated/directors/bareos-dir/etc/bareos/bareos-dir.d/messages/Standard.conf";
  const auto staged_file_daemon_main
      = root.path() / "generated/clients/wizard-fd/etc/bareos/bareos-fd.conf";
  const auto staged_file_daemon_messages = root.path()
                                           / "generated/clients/wizard-fd/etc/bareos/bareos-fd.d/messages/Standard.conf";
  ASSERT_TRUE(std::filesystem::exists(director_path));
  ASSERT_TRUE(std::filesystem::exists(file_daemon_path));
  ASSERT_TRUE(std::filesystem::exists(staged_director_main));
  ASSERT_TRUE(std::filesystem::exists(staged_director_messages));
  ASSERT_TRUE(std::filesystem::exists(staged_file_daemon_main));
  ASSERT_TRUE(std::filesystem::exists(staged_file_daemon_messages));

  const auto director_content = [&director_path] {
    std::ifstream input(director_path);
    std::ostringstream output;
    output << input.rdbuf();
    return output.str();
  }();
  const auto file_daemon_content = [&file_daemon_path] {
    std::ifstream input(file_daemon_path);
    std::ostringstream output;
    output << input.rdbuf();
    return output.str();
  }();

  EXPECT_NE(director_content.find("Name = wizard-fd"), std::string::npos);
  EXPECT_NE(director_content.find("Address = 10.0.0.40"), std::string::npos);
  EXPECT_NE(file_daemon_content.find("Name = bareos-dir"), std::string::npos);
  EXPECT_NE(file_daemon_content.find("Password = secret"), std::string::npos);
}

TEST(BareosConfigService, CopiesDirectorConfigTreeIntoStagingForRemoteClient)
{
  TempConfigRoot root;
  std::filesystem::create_directories(root.path() / "bareos-dir.d/client");
  std::filesystem::create_directories(root.path() / "bareos-dir.d/job");
  std::ofstream(root.path() / "bareos-dir.conf") << "Director {}\n";
  std::ofstream(root.path() / "bareos-dir.d/client/existing-fd.conf")
      << "Client {\n  Name = existing-fd\n}\n";
  std::ofstream(root.path() / "bareos-dir.d/job/backup.conf")
      << "Job {\n  Name = BackupClient\n}\n";

  const ConfigServiceOptions options = MakeServiceOptions(root.path());
  const HttpRequest request{"POST",
                            "/api/v1/nodes/datacenter/add-client/save/director-0",
                            "text/plain; charset=utf-8",
                            "Name=remote-fd\nAddress=10.0.0.50\nPassword=secret\n"};

  const auto response = HandleConfigServiceRequest(options, request);

  EXPECT_EQ(response.status_code, 200);
  EXPECT_TRUE(std::filesystem::exists(
      root.path() / "generated/directors/bareos-dir/etc/bareos/bareos-dir.conf"));
  EXPECT_TRUE(std::filesystem::exists(
      root.path()
      / "generated/directors/bareos-dir/etc/bareos/bareos-dir.d/job/backup.conf"));

  const auto bootstrap
      = HandleConfigServiceRequest(options, {"GET", "/api/v1/bootstrap", "", ""});
  EXPECT_EQ(bootstrap.status_code, 200);
  EXPECT_EQ(bootstrap.body.find("\"id\":\"staged-client-bareos-dir-existing-fd\""),
            std::string::npos);
}

TEST(BareosConfigService, ShowsStagedClientInBootstrapTreeAndResources)
{
  TempConfigRoot root;
  std::filesystem::create_directories(root.path() / "bareos-dir.d/client");
  std::ofstream(root.path() / "bareos-dir.conf") << "Director {}\n";

  const ConfigServiceOptions options = MakeServiceOptions(root.path());
  const HttpRequest save_request{"POST",
                                 "/api/v1/nodes/datacenter/add-client/save/director-0",
                                 "text/plain; charset=utf-8",
                                 "Name=wizard-fd\nAddress=10.0.0.40\nPassword=secret\n"};
  const auto save_response = HandleConfigServiceRequest(options, save_request);
  ASSERT_EQ(save_response.status_code, 200);

  const auto bootstrap = HandleConfigServiceRequest(
      options, {"GET", "/api/v1/bootstrap", "", ""});
  EXPECT_EQ(bootstrap.status_code, 200);
  EXPECT_NE(bootstrap.body.find("\"id\":\"staged-client-bareos-dir-wizard-fd\""),
            std::string::npos);

  const auto resources = HandleConfigServiceRequest(
      options,
      {"GET", "/api/v1/nodes/staged-client-bareos-dir-wizard-fd/resources", "", ""});
  EXPECT_EQ(resources.status_code, 200);
  EXPECT_NE(resources.body.find("\"name\":\"wizard-fd\""), std::string::npos);
  EXPECT_NE(resources.body.find("\"name\":\"wizard-fd generated client config\""),
            std::string::npos);
}

TEST(BareosConfigService, MarksStagedAddClientWizardPreviewAsUpdate)
{
  TempConfigRoot root;
  std::filesystem::create_directories(root.path() / "bareos-dir.d/client");
  std::filesystem::create_directories(
      root.path()
      / "generated/directors/bareos-dir/etc/bareos/bareos-dir.d/client");
  std::filesystem::create_directories(
      root.path()
      / "generated/clients/wizard-fd/etc/bareos/bareos-fd.d/director");
  std::ofstream(root.path() / "bareos-dir.conf") << "Director {}\n";
  std::ofstream(root.path()
                / "generated/directors/bareos-dir/etc/bareos/bareos-dir.d/client/wizard-fd.conf")
      << "Client {\n  Name = wizard-fd\n}\n";
  std::ofstream(root.path()
                / "generated/clients/wizard-fd/etc/bareos/bareos-fd.d/director/bareos-dir.conf")
      << "Director {\n  Name = bareos-dir\n}\n";

  const ConfigServiceOptions options = MakeServiceOptions(root.path());
  const HttpRequest request{"POST",
                            "/api/v1/nodes/datacenter/add-client/preview/director-0",
                            "text/plain; charset=utf-8",
                            "Name=wizard-fd\nAddress=10.0.0.40\nPassword=secret\n"};

  const auto response = HandleConfigServiceRequest(options, request);

  EXPECT_EQ(response.status_code, 200);
  EXPECT_NE(response.body.find("\"action\":\"update\""), std::string::npos);
  EXPECT_NE(response.body.find("\"role\":\"director-client\""),
            std::string::npos);
  EXPECT_NE(response.body.find("\"role\":\"client-file-daemon-director\""),
            std::string::npos);
}

TEST(BareosConfigService, ReturnsAddClientWizardSchema)
{
  TempConfigRoot root;
  std::filesystem::create_directories(root.path() / "bareos-dir.d/client");
  std::filesystem::create_directories(root.path() / "bareos-dir.d/catalog");
  std::ofstream(root.path() / "bareos-dir.conf") << "Director {}\n";
  std::ofstream(root.path() / "bareos-dir.d/catalog/MyCatalog.conf")
      << "Catalog {\n  Name = MyCatalog\n}\n";

  const ConfigServiceOptions options = MakeServiceOptions(root.path());
  const HttpRequest request{
      "GET", "/api/v1/nodes/datacenter/add-client/schema/director-0", "", ""};

  const auto response = HandleConfigServiceRequest(options, request);

  EXPECT_EQ(response.status_code, 200);
  EXPECT_NE(response.body.find("\"wizard\":\"add-client\""), std::string::npos);
  EXPECT_NE(response.body.find("\"field_hints\":[{"), std::string::npos);
  EXPECT_NE(response.body.find("\"key\":\"Name\""), std::string::npos);
  EXPECT_NE(response.body.find("\"key\":\"Address\""), std::string::npos);
  EXPECT_NE(response.body.find("\"key\":\"Password\""), std::string::npos);
  EXPECT_EQ(response.body.find("\"key\":\"Catalog\""), std::string::npos);
  EXPECT_EQ(response.body.find("FileRetention"), std::string::npos);
  EXPECT_EQ(response.body.find("JobRetention"), std::string::npos);
}

TEST(BareosConfigService, ReturnsAddClientWizardSchemaWithCatalogChoice)
{
  TempConfigRoot root;
  std::filesystem::create_directories(root.path() / "bareos-dir.d/client");
  std::filesystem::create_directories(root.path() / "bareos-dir.d/catalog");
  std::ofstream(root.path() / "bareos-dir.conf") << "Director {}\n";
  std::ofstream(root.path() / "bareos-dir.d/catalog/MyCatalog.conf")
      << "Catalog {\n  Name = MyCatalog\n}\n";
  std::ofstream(root.path() / "bareos-dir.d/catalog/OtherCatalog.conf")
      << "Catalog {\n  Name = OtherCatalog\n}\n";

  const ConfigServiceOptions options = MakeServiceOptions(root.path());
  const HttpRequest request{
      "GET", "/api/v1/nodes/datacenter/add-client/schema/director-0", "", ""};

  const auto response = HandleConfigServiceRequest(options, request);

  EXPECT_EQ(response.status_code, 200);
  EXPECT_NE(response.body.find("\"key\":\"Catalog\""), std::string::npos);
  EXPECT_NE(response.body.find("\"related_resource_type\":\"catalog\""),
            std::string::npos);
  EXPECT_NE(response.body.find("\"allowed_values\":[\"MyCatalog\",\"OtherCatalog\"]"),
            std::string::npos);
}

TEST(BareosConfigService, OmitsSingleCatalogInAddClientWizardPreview)
{
  TempConfigRoot root;
  std::filesystem::create_directories(root.path() / "bareos-dir.d/client");
  std::filesystem::create_directories(root.path() / "bareos-dir.d/catalog");
  std::ofstream(root.path() / "bareos-dir.conf") << "Director {}\n";
  std::ofstream(root.path() / "bareos-dir.d/catalog/MyCatalog.conf")
      << "Catalog {\n  Name = MyCatalog\n}\n";

  const ConfigServiceOptions options = MakeServiceOptions(root.path());
  const HttpRequest request{"POST",
                            "/api/v1/nodes/datacenter/add-client/preview/director-0",
                            "text/plain; charset=utf-8",
                            "Name=wizard-fd\nAddress=10.0.0.40\nPassword=secret\n"};

  const auto response = HandleConfigServiceRequest(options, request);

  EXPECT_EQ(response.status_code, 200);
  EXPECT_EQ(response.body.find("Catalog = MyCatalog"), std::string::npos);
  EXPECT_NE(response.body.find("\"validation_messages\":[]"), std::string::npos);
}

TEST(BareosConfigService, MarksExistingClientWizardPreviewAsUpdate)
{
  TempConfigRoot root;
  std::filesystem::create_directories(root.path() / "bareos-dir.d/client");
  std::ofstream(root.path() / "bareos-dir.conf") << "Director {}\n";
  std::filesystem::create_directories(
      root.path()
      / "generated/directors/bareos-dir/etc/bareos/bareos-dir.d/client");
  std::ofstream(root.path()
                / "generated/directors/bareos-dir/etc/bareos/bareos-dir.d/client/wizard-fd.conf")
      << "Client {\n  Name = wizard-fd\n}\n";

  const ConfigServiceOptions options = MakeServiceOptions(root.path());
  const HttpRequest request{"POST",
                            "/api/v1/nodes/datacenter/add-client/preview/director-0",
                            "text/plain; charset=utf-8",
                            "Name=wizard-fd\nAddress=10.0.0.40\nPassword=secret\n"};

  const auto response = HandleConfigServiceRequest(options, request);

  EXPECT_EQ(response.status_code, 200);
  EXPECT_NE(response.body.find("\"action\":\"update\""), std::string::npos);
}

TEST(BareosConfigService, StagesClientFileDaemonConfigEvenWhenManagedLocally)
{
  TempConfigRoot root;
  std::filesystem::create_directories(root.path() / "bareos-dir.d/client");
  std::filesystem::create_directories(root.path() / "bareos-fd.d/director");
  std::ofstream(root.path() / "bareos-dir.conf") << "Director {}\n";
  std::ofstream(root.path() / "bareos-fd.conf")
      << "FileDaemon {\n  Name = local-fd\n}\n";
  std::ofstream(root.path() / "bareos-fd.d/director/bareos-dir.conf")
      << "Director {\n  Name = bareos-dir\n}\n";

  const ConfigServiceOptions options = MakeServiceOptions(root.path());
  const HttpRequest request{"POST",
                            "/api/v1/nodes/datacenter/add-client/preview/director-0",
                            "text/plain; charset=utf-8",
                            "Name=wizard-fd\nAddress=10.0.0.40\nPassword=secret\n"};

  const auto response = HandleConfigServiceRequest(options, request);

  EXPECT_EQ(response.status_code, 200);
  EXPECT_NE(response.body.find(
                (root.path()
                 / "generated/clients/wizard-fd/etc/bareos/bareos-fd.d/director/bareos-dir.conf")
                    .string()),
            std::string::npos);
  EXPECT_NE(response.body.find(
                (root.path()
                 / "generated/directors/bareos-dir/etc/bareos/bareos-dir.d/client/wizard-fd.conf")
                    .string()),
            std::string::npos);
}
