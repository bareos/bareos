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
#include "config_service.h"

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

  const ConfigServiceOptions options{{root.path()}};
  const HttpRequest request{
      "GET", "/api/v1/resources/resource-0-director-0/editor", "", ""};

  const auto response = HandleConfigServiceRequest(options, request);

  EXPECT_EQ(response.status_code, 200);
  EXPECT_NE(response.body.find("\"editable\":true"), std::string::npos);
  EXPECT_NE(response.body.find("\"save_mode\":\"dry-run\""), std::string::npos);
  EXPECT_NE(response.body.find("\"field_hints\":[{"), std::string::npos);
  EXPECT_NE(response.body.find("\"key\":\"Address\""), std::string::npos);
  EXPECT_NE(response.body.find("\"key\":\"Name\""), std::string::npos);
  EXPECT_NE(response.body.find("\"related_resource_type\":\"catalog\""),
            std::string::npos);
  EXPECT_NE(response.body.find("\"allowed_values\":[\"MyCatalog\"]"),
            std::string::npos);
  EXPECT_NE(response.body.find("\"code\":\"missing-address\""), std::string::npos);
  EXPECT_NE(response.body.find("example-fd"), std::string::npos);
}

TEST(BareosConfigService, ReturnsNewResourceEditorPayload)
{
  TempConfigRoot root;
  std::filesystem::create_directories(root.path() / "bareos-dir.d/client");
  std::ofstream(root.path() / "bareos-dir.conf") << "Director {}\n";
  std::ofstream(root.path() / "bareos-dir.d/client/example-fd.conf")
      << "Client {\n  Name = example-fd\n}\n";

  const ConfigServiceOptions options{{root.path()}};
  const HttpRequest request{
      "GET", "/api/v1/nodes/director-0/new-resource/client/editor", "", ""};

  const auto response = HandleConfigServiceRequest(options, request);

  EXPECT_EQ(response.status_code, 200);
  EXPECT_NE(response.body.find("\"editable\":true"), std::string::npos);
  EXPECT_NE(response.body.find("\"type\":\"client\""), std::string::npos);
  EXPECT_NE(response.body.find("new-client.conf"), std::string::npos);
  EXPECT_NE(response.body.find("Client {\\n}\\n"), std::string::npos);
}

TEST(BareosConfigService, ExposesCreatableStorageDaemonResourceTypes)
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

  const ConfigServiceOptions options{{root.path()}};
  const HttpRequest request{"GET", "/api/v1/nodes/daemon-0-storage-daemon", "", ""};

  const auto response = HandleConfigServiceRequest(options, request);

  EXPECT_EQ(response.status_code, 200);
  EXPECT_NE(response.body.find("\"creatable_resource_types\":["),
            std::string::npos);
  EXPECT_NE(response.body.find("\"autochanger\""), std::string::npos);
  EXPECT_NE(response.body.find("\"device\""), std::string::npos);
  EXPECT_NE(response.body.find("\"storage\""), std::string::npos);
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

  const ConfigServiceOptions options{{root.path()}};
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
  EXPECT_EQ(response.body.find("node.kind === 'director'"), std::string::npos);
}

TEST(BareosConfigService, ReturnsDryRunPreviewForUpdatedContent)
{
  TempConfigRoot root;
  std::filesystem::create_directories(root.path() / "bareos-dir.d/client");
  std::ofstream(root.path() / "bareos-dir.conf") << "Director {}\n";
  std::ofstream(root.path() / "bareos-dir.d/client/example-fd.conf")
      << "Client {\n  Name = example-fd\n}\n";

  const ConfigServiceOptions options{{root.path()}};
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

TEST(BareosConfigService, ReturnsFieldBasedPreviewForUpdatedContent)
{
  TempConfigRoot root;
  std::filesystem::create_directories(root.path() / "bareos-dir.d/client");
  std::ofstream(root.path() / "bareos-dir.conf") << "Director {}\n";
  std::ofstream(root.path() / "bareos-dir.d/client/example-fd.conf")
      << "Client {\n  Name = example-fd\n}\n";

  const ConfigServiceOptions options{{root.path()}};
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

  const ConfigServiceOptions options{{root.path()}};
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

TEST(BareosConfigService, ReturnsNodeRelationships)
{
  TempConfigRoot root;
  std::filesystem::create_directories(root.path() / "bareos-dir.d/client");
  std::filesystem::create_directories(root.path() / "bareos-dir.d/storage");
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
  std::ofstream(root.path() / "bareos-fd.d/fileset/local.conf") << "FileSet {}\n";
  std::ofstream(root.path() / "bareos-sd.d/device/tape.conf") << "Device {}\n";

  const ConfigServiceOptions options{{root.path()}};
  const HttpRequest request{
      "GET", "/api/v1/nodes/director-0/relationships", "", ""};

  const auto response = HandleConfigServiceRequest(options, request);

  EXPECT_EQ(response.status_code, 200);
  EXPECT_NE(response.body.find("\"relation\":\"client\""), std::string::npos);
  EXPECT_NE(response.body.find("\"relation\":\"storage\""), std::string::npos);
  EXPECT_NE(response.body.find("\"from_node_id\":\"director-0\""),
            std::string::npos);
  EXPECT_NE(response.body.find("\"to_node_id\":\"daemon-0-file-daemon\""),
            std::string::npos);
  EXPECT_NE(response.body.find("\"to_node_id\":\"daemon-0-storage-daemon\""),
            std::string::npos);
  EXPECT_NE(response.body.find("\"endpoint_name\":\"example-fd\""),
            std::string::npos);
  EXPECT_NE(response.body.find("\"resolution\":\"Resolved to parsed local file-daemon node bareos-fd with configured name example-fd.\""),
            std::string::npos);
}

TEST(BareosConfigService, ReturnsAddClientWizardPreview)
{
  TempConfigRoot root;
  std::filesystem::create_directories(root.path() / "bareos-dir.d/client");
  std::ofstream(root.path() / "bareos-dir.conf") << "Director {}\n";

  const ConfigServiceOptions options{{root.path()}};
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
  EXPECT_NE(response.body.find("\"action\":\"create-or-update\""),
            std::string::npos);
  EXPECT_NE(response.body.find("\"type\":\"client\""), std::string::npos);
  EXPECT_NE(response.body.find("\"type\":\"director\""), std::string::npos);
  EXPECT_NE(response.body.find("\"director_name\":\"bareos-dir\""),
            std::string::npos);
  EXPECT_NE(response.body.find("Name = bareos-dir"), std::string::npos);
  EXPECT_NE(response.body.find("wizard-fd.conf"), std::string::npos);
  EXPECT_NE(response.body.find("Address = 10.0.0.40"), std::string::npos);
  EXPECT_NE(response.body.find("client:/etc/bareos/bareos-fd.d/director/bareos-dir.conf"),
            std::string::npos);
  EXPECT_NE(response.body.find("\"validation_messages\":[]"), std::string::npos);
}

TEST(BareosConfigService, ReturnsAddClientWizardSchema)
{
  TempConfigRoot root;
  std::filesystem::create_directories(root.path() / "bareos-dir.d/client");
  std::filesystem::create_directories(root.path() / "bareos-dir.d/catalog");
  std::ofstream(root.path() / "bareos-dir.conf") << "Director {}\n";
  std::ofstream(root.path() / "bareos-dir.d/catalog/MyCatalog.conf")
      << "Catalog {\n  Name = MyCatalog\n}\n";

  const ConfigServiceOptions options{{root.path()}};
  const HttpRequest request{
      "GET", "/api/v1/nodes/datacenter/add-client/schema/director-0", "", ""};

  const auto response = HandleConfigServiceRequest(options, request);

  EXPECT_EQ(response.status_code, 200);
  EXPECT_NE(response.body.find("\"wizard\":\"add-client\""), std::string::npos);
  EXPECT_NE(response.body.find("\"field_hints\":[{"), std::string::npos);
  EXPECT_NE(response.body.find("\"key\":\"Name\""), std::string::npos);
  EXPECT_NE(response.body.find("\"key\":\"Address\""), std::string::npos);
  EXPECT_NE(response.body.find("\"key\":\"Password\""), std::string::npos);
  EXPECT_NE(response.body.find("\"key\":\"Catalog\""), std::string::npos);
  EXPECT_NE(response.body.find("\"required\":true"), std::string::npos);
  EXPECT_NE(response.body.find("\"related_resource_type\":\"catalog\""),
            std::string::npos);
  EXPECT_NE(response.body.find("\"allowed_values\":[\"MyCatalog\"]"),
            std::string::npos);
  EXPECT_EQ(response.body.find("FileRetention"), std::string::npos);
  EXPECT_EQ(response.body.find("JobRetention"), std::string::npos);
}

TEST(BareosConfigService, MarksExistingClientWizardPreviewAsUpdate)
{
  TempConfigRoot root;
  std::filesystem::create_directories(root.path() / "bareos-dir.d/client");
  std::ofstream(root.path() / "bareos-dir.conf") << "Director {}\n";
  std::ofstream(root.path() / "bareos-dir.d/client/wizard-fd.conf")
      << "Client {\n  Name = wizard-fd\n}\n";

  const ConfigServiceOptions options{{root.path()}};
  const HttpRequest request{"POST",
                            "/api/v1/nodes/datacenter/add-client/preview/director-0",
                            "text/plain; charset=utf-8",
                            "Name=wizard-fd\nAddress=10.0.0.40\nPassword=secret\n"};

  const auto response = HandleConfigServiceRequest(options, request);

  EXPECT_EQ(response.status_code, 200);
  EXPECT_NE(response.body.find("\"action\":\"update\""), std::string::npos);
}

TEST(BareosConfigService, UsesLocalFileDaemonTargetWhenManagedLocally)
{
  TempConfigRoot root;
  std::filesystem::create_directories(root.path() / "bareos-dir.d/client");
  std::filesystem::create_directories(root.path() / "bareos-fd.d/director");
  std::ofstream(root.path() / "bareos-dir.conf") << "Director {}\n";
  std::ofstream(root.path() / "bareos-fd.conf") << "FileDaemon {}\n";
  std::ofstream(root.path() / "bareos-fd.d/director/bareos-dir.conf")
      << "Director {\n  Name = bareos-dir\n}\n";

  const ConfigServiceOptions options{{root.path()}};
  const HttpRequest request{"POST",
                            "/api/v1/nodes/datacenter/add-client/preview/director-0",
                            "text/plain; charset=utf-8",
                            "Name=wizard-fd\nAddress=10.0.0.40\nPassword=secret\n"};

  const auto response = HandleConfigServiceRequest(options, request);

  EXPECT_EQ(response.status_code, 200);
  EXPECT_NE(response.body.find("\"role\":\"client-file-daemon-director\""),
            std::string::npos);
  EXPECT_NE(response.body.find("\"action\":\"update\""), std::string::npos);
  EXPECT_NE(response.body.find("\"exists_local\":true"), std::string::npos);
  EXPECT_NE(response.body.find(
                (root.path() / "bareos-fd.d/director/bareos-dir.conf").string()),
            std::string::npos);
  EXPECT_EQ(response.body.find("client:/etc/bareos/bareos-fd.d/director"),
            std::string::npos);
}
