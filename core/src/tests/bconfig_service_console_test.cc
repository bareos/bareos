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

#include "bconfig_service_test_utils.h"

namespace bconfig::service {
namespace {
TEST(BconfigService, ImportsConsoleRootsIntoSingleConfigFile)
{
  ScopedDirectory source_root{MakeTempPath()};
  ScopedDirectory repo_path{MakeTempPath()};
  ServiceState state;

  auto deployment
      = state.CreateDeployment({.id = "prod",
                                .name = "Production",
                                .repository_path = repo_path.path()});
  ASSERT_TRUE(deployment);

  AddConsoleImportFixture(source_root.path());

  auto import_job
      = state.CreateJob({.type = "import_configuration",
                         .deployment_id = std::string{"prod"},
                         .source_path = source_root.path().string()});
  ASSERT_TRUE(import_job);
  auto imported = WaitForJobTerminal(state, import_job.value->id);
  ASSERT_TRUE(imported.has_value());
  ASSERT_EQ(imported->status, JobStatus::kSucceeded);

  const auto imported_root
      = RepositoryLayout::ConsolesDirectory(repo_path.path()) / "admin";
  const auto root_config = imported_root / "bconsole.conf";
  ASSERT_TRUE(std::filesystem::exists(root_config));
  EXPECT_FALSE(std::filesystem::exists(imported_root / "bconsole.d"));

  const auto root_text = ReadTextFile(root_config);
  EXPECT_NE(root_text.find("Console {"), std::string::npos);
  EXPECT_NE(root_text.find("Name = \"admin\""), std::string::npos);
  EXPECT_NE(root_text.find("bareos-dir"), std::string::npos);
  EXPECT_NE(root_text.find("Director {"), std::string::npos);
  EXPECT_NE(root_text.find("Name = \"bareos-dir\""), std::string::npos);

  auto imports = state.ListDeploymentImports("prod");
  ASSERT_TRUE(imports);
  ASSERT_EQ(imports.value->size(), 1u);
  EXPECT_EQ(imports.value->at(0).component, "console");
  EXPECT_EQ(imports.value->at(0).resource_name, "admin");
  EXPECT_EQ(imports.value->at(0).destination_path, "consoles/admin");

  auto validate_job = state.CreateJob({.type = "validate_deployment_repo",
                                       .deployment_id = std::string{"prod"}});
  ASSERT_TRUE(validate_job);
  auto validated = WaitForJobTerminal(state, validate_job.value->id);
  ASSERT_TRUE(validated.has_value());
  EXPECT_EQ(validated->status, JobStatus::kSucceeded);
  EXPECT_NE(std::find(validated->logs.begin(), validated->logs.end(),
                      "validated console 'admin'"),
            validated->logs.end());
}

TEST(BconfigService, UpsertsConsoleComponentConsoleResources)
{
  ScopedDirectory source_root{MakeTempPath()};
  ScopedDirectory repo_path{MakeTempPath()};
  ServiceState state;

  auto deployment
      = state.CreateDeployment({.id = "prod",
                                .name = "Production",
                                .repository_path = repo_path.path()});
  ASSERT_TRUE(deployment);

  AddConsoleImportFixture(source_root.path());

  auto import_job
      = state.CreateJob({.type = "import_configuration",
                         .deployment_id = std::string{"prod"},
                         .source_path = source_root.path().string()});
  ASSERT_TRUE(import_job);
  auto imported = WaitForJobTerminal(state, import_job.value->id);
  ASSERT_TRUE(imported.has_value());
  ASSERT_EQ(imported->status, JobStatus::kSucceeded);

  auto created = state.UpsertConsoleConsoleResource(
      "prod", "admin", "managed-console",
      {.director = std::string{"bareos-dir"},
       .password = std::string{"[md5]abcdef0123456789abcdef0123456789"},
       .description = std::string{"Managed bconsole resource"},
       .rc_file = std::string{"/var/lib/bareos/managed.rc"},
       .history_file = std::string{"/var/lib/bareos/managed.history"},
       .history_length = 123,
       .heartbeat_interval = 33,
       .tls_authenticate = false,
       .tls_enable = true,
       .tls_require = false,
       .tls_verify_peer = true,
       .tls_cipher_list = std::string{"DEFAULT:@SECLEVEL=2"},
       .tls_cipher_suites = std::string{"TLS_AES_256_GCM_SHA384"},
       .tls_dh_file = std::string{"/etc/bareos/console-dh.pem"},
       .tls_protocol = std::string{"+TLSv1.2:+TLSv1.3"},
       .tls_ca_certificate_file = std::string{"/etc/bareos/ca.pem"},
       .tls_ca_certificate_dir = std::string{"/etc/ssl/certs"},
       .tls_certificate_revocation_list = std::string{"/etc/bareos/crl.pem"},
       .tls_certificate = std::string{"/etc/bareos/console-cert.pem"},
       .tls_key = std::string{"/etc/bareos/console-key.pem"},
       .tls_allowed_cn = std::vector<std::string>{"console.example.test",
                                                  "director.example.test"}});
  ASSERT_TRUE(created) << created.error;
  EXPECT_EQ(created.value->name, "admin");

  const auto created_text = ReadTextFile(created.value->path / "bconsole.conf");
  EXPECT_NE(created_text.find("Name = \"managed-console\""), std::string::npos);
  EXPECT_NE(created_text.find("Director = bareos-dir"), std::string::npos);
  EXPECT_NE(
      created_text.find("Password = \"[md5]abcdef0123456789abcdef0123456789\""),
      std::string::npos);
  EXPECT_NE(created_text.find("Description = \"Managed bconsole resource\""),
            std::string::npos);
  EXPECT_NE(created_text.find("RcFile = \"/var/lib/bareos/managed.rc\""),
            std::string::npos);
  EXPECT_NE(
      created_text.find("HistoryFile = \"/var/lib/bareos/managed.history\""),
      std::string::npos);
  EXPECT_NE(created_text.find("HistoryLength = 123"), std::string::npos);
  EXPECT_NE(created_text.find("HeartbeatInterval = 33"), std::string::npos);
  EXPECT_NE(created_text.find("TlsAuthenticate = no"), std::string::npos);
  EXPECT_NE(created_text.find("TlsEnable = yes"), std::string::npos);
  EXPECT_NE(created_text.find("TlsRequire = no"), std::string::npos);
  EXPECT_NE(created_text.find("TlsVerifyPeer = yes"), std::string::npos);
  EXPECT_NE(created_text.find("TlsCipherList = \"DEFAULT:@SECLEVEL=2\""),
            std::string::npos);
  EXPECT_NE(created_text.find("TlsCipherSuites = \"TLS_AES_256_GCM_SHA384\""),
            std::string::npos);
  EXPECT_NE(created_text.find("TlsDhFile = \"/etc/bareos/console-dh.pem\""),
            std::string::npos);
  EXPECT_NE(created_text.find("TlsProtocol = \"+TLSv1.2:+TLSv1.3\""),
            std::string::npos);
  EXPECT_NE(created_text.find("TlsCaCertificateFile = \"/etc/bareos/ca.pem\""),
            std::string::npos);
  EXPECT_NE(created_text.find("TlsCaCertificateDir = \"/etc/ssl/certs\""),
            std::string::npos);
  EXPECT_NE(created_text.find(
                "TlsCertificateRevocationList = \"/etc/bareos/crl.pem\""),
            std::string::npos);
  EXPECT_NE(
      created_text.find("TlsCertificate = \"/etc/bareos/console-cert.pem\""),
      std::string::npos);
  EXPECT_NE(created_text.find("TlsKey = \"/etc/bareos/console-key.pem\""),
            std::string::npos);
  EXPECT_NE(created_text.find("TlsAllowedCn = \"console.example.test\""),
            std::string::npos);
  EXPECT_NE(created_text.find("TlsAllowedCn = \"director.example.test\""),
            std::string::npos);

  auto updated = state.UpsertConsoleConsoleResource(
      "prod", "admin", "admin",
      {.description = std::string{"Updated imported console"}});
  ASSERT_TRUE(updated) << updated.error;

  const auto updated_text = ReadTextFile(updated.value->path / "bconsole.conf");
  EXPECT_NE(updated_text.find("Description = \"Updated imported console\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("Password = "), std::string::npos);
  EXPECT_NE(updated_text.find("Director = bareos-dir"), std::string::npos);
  EXPECT_NE(updated_text.find("Name = \"managed-console\""), std::string::npos);

  auto current = state.GetConsoleConsoleResourceSpec("prod", "admin", "admin");
  ASSERT_TRUE(current) << current.error;
  EXPECT_EQ(current.value->director, "bareos-dir");
  EXPECT_EQ(current.value->description, "Updated imported console");
  ASSERT_TRUE(current.value->password.has_value());
  EXPECT_FALSE(current.value->password->empty());
}

TEST(BconfigService, UpsertsConsoleComponentConsoleHistoryFields)
{
  ScopedDirectory source_root{MakeTempPath()};
  ScopedDirectory repo_path{MakeTempPath()};
  ServiceState state;

  auto deployment
      = state.CreateDeployment({.id = "prod",
                                .name = "Production",
                                .repository_path = repo_path.path()});
  ASSERT_TRUE(deployment);

  WriteTextFile(source_root.path() / "bconsole.conf",
                "#\n"
                "# Bareos User Agent (or Console) Configuration File\n"
                "#\n"
                "\n"
                "Console {\n"
                "  Name = admin\n"
                "  Description = \"Imported Console\"\n"
                "  Password = \"secret\"\n"
                "  Director = bareos-dir\n"
                "  HistoryFile = \"/var/lib/bareos/admin.history\"\n"
                "  HistoryLength = 250\n"
                "  HeartbeatInterval = 45\n"
                "}\n"
                "\n"
                "Director {\n"
                "  Name = bareos-dir\n"
                "  Description = \"Imported Director\"\n"
                "  Address = localhost\n"
                "  Password = \"secret\"\n"
                "}\n");

  auto import_job
      = state.CreateJob({.type = "import_configuration",
                         .deployment_id = std::string{"prod"},
                         .source_path = source_root.path().string()});
  ASSERT_TRUE(import_job);
  auto imported = WaitForJobTerminal(state, import_job.value->id);
  ASSERT_TRUE(imported.has_value());
  ASSERT_EQ(imported->status, JobStatus::kSucceeded);

  auto updated = state.UpsertConsoleConsoleResource(
      "prod", "admin", "admin",
      {.description = std::string{"Updated imported console"},
       .history_file = std::string{"/var/lib/bareos/admin.updated.history"},
       .history_length = 500,
       .heartbeat_interval = 90});
  ASSERT_TRUE(updated) << updated.error;

  const auto updated_text = ReadTextFile(updated.value->path / "bconsole.conf");
  EXPECT_NE(updated_text.find("Description = \"Updated imported console\""),
            std::string::npos);
  EXPECT_NE(updated_text.find(
                "HistoryFile = \"/var/lib/bareos/admin.updated.history\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("HistoryLength = 500"), std::string::npos);
  EXPECT_NE(updated_text.find("HeartbeatInterval = 90"), std::string::npos);
}

TEST(BconfigService, UpsertsConsoleComponentConsoleRcFile)
{
  ScopedDirectory source_root{MakeTempPath()};
  ScopedDirectory repo_path{MakeTempPath()};
  ServiceState state;

  auto deployment
      = state.CreateDeployment({.id = "prod",
                                .name = "Production",
                                .repository_path = repo_path.path()});
  ASSERT_TRUE(deployment);

  WriteTextFile(source_root.path() / "bconsole.conf",
                "#\n"
                "# Bareos User Agent (or Console) Configuration File\n"
                "#\n"
                "\n"
                "Console {\n"
                "  Name = admin\n"
                "  Description = \"Imported Console\"\n"
                "  Password = \"secret\"\n"
                "  Director = bareos-dir\n"
                "  RcFile = \"/var/lib/bareos/admin.rc\"\n"
                "}\n"
                "\n"
                "Director {\n"
                "  Name = bareos-dir\n"
                "  Description = \"Imported Director\"\n"
                "  Address = localhost\n"
                "  Password = \"secret\"\n"
                "}\n");

  auto import_job
      = state.CreateJob({.type = "import_configuration",
                         .deployment_id = std::string{"prod"},
                         .source_path = source_root.path().string()});
  ASSERT_TRUE(import_job);
  auto imported = WaitForJobTerminal(state, import_job.value->id);
  ASSERT_TRUE(imported.has_value());
  ASSERT_EQ(imported->status, JobStatus::kSucceeded);

  auto updated = state.UpsertConsoleConsoleResource(
      "prod", "admin", "admin",
      {.description = std::string{"Updated imported console"},
       .rc_file = std::string{"/var/lib/bareos/admin.updated.rc"}});
  ASSERT_TRUE(updated) << updated.error;

  const auto updated_text = ReadTextFile(updated.value->path / "bconsole.conf");
  EXPECT_NE(updated_text.find("Description = \"Updated imported console\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("RcFile = \"/var/lib/bareos/admin.updated.rc\""),
            std::string::npos);
}

TEST(BconfigService, UpsertsConsoleComponentConsoleTlsBooleans)
{
  ScopedDirectory source_root{MakeTempPath()};
  ScopedDirectory repo_path{MakeTempPath()};
  ServiceState state;

  auto deployment
      = state.CreateDeployment({.id = "prod",
                                .name = "Production",
                                .repository_path = repo_path.path()});
  ASSERT_TRUE(deployment);

  WriteTextFile(source_root.path() / "bconsole.conf",
                "#\n"
                "# Bareos User Agent (or Console) Configuration File\n"
                "#\n"
                "\n"
                "Console {\n"
                "  Name = admin\n"
                "  Description = \"Imported Console\"\n"
                "  Password = \"secret\"\n"
                "  Director = bareos-dir\n"
                "  TlsAuthenticate = yes\n"
                "  TlsEnable = no\n"
                "  TlsRequire = yes\n"
                "  TlsVerifyPeer = no\n"
                "}\n"
                "\n"
                "Director {\n"
                "  Name = bareos-dir\n"
                "  Description = \"Imported Director\"\n"
                "  Address = localhost\n"
                "  Password = \"secret\"\n"
                "}\n");

  auto import_job
      = state.CreateJob({.type = "import_configuration",
                         .deployment_id = std::string{"prod"},
                         .source_path = source_root.path().string()});
  ASSERT_TRUE(import_job);
  auto imported = WaitForJobTerminal(state, import_job.value->id);
  ASSERT_TRUE(imported.has_value());
  ASSERT_EQ(imported->status, JobStatus::kSucceeded);

  auto updated = state.UpsertConsoleConsoleResource(
      "prod", "admin", "admin",
      {.description = std::string{"Updated imported console"},
       .tls_authenticate = false,
       .tls_enable = true,
       .tls_require = false,
       .tls_verify_peer = true});
  ASSERT_TRUE(updated) << updated.error;

  const auto updated_text = ReadTextFile(updated.value->path / "bconsole.conf");
  EXPECT_NE(updated_text.find("Description = \"Updated imported console\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("TlsAuthenticate = no"), std::string::npos);
  EXPECT_NE(updated_text.find("TlsEnable = yes"), std::string::npos);
  EXPECT_NE(updated_text.find("TlsRequire = no"), std::string::npos);
  EXPECT_NE(updated_text.find("TlsVerifyPeer = yes"), std::string::npos);
}

TEST(BconfigService, UpsertsConsoleComponentConsoleTlsStrings)
{
  ScopedDirectory source_root{MakeTempPath()};
  ScopedDirectory repo_path{MakeTempPath()};
  ServiceState state;

  auto deployment
      = state.CreateDeployment({.id = "prod",
                                .name = "Production",
                                .repository_path = repo_path.path()});
  ASSERT_TRUE(deployment);

  WriteTextFile(source_root.path() / "bconsole.conf",
                "#\n"
                "# Bareos User Agent (or Console) Configuration File\n"
                "#\n"
                "\n"
                "Console {\n"
                "  Name = admin\n"
                "  Description = \"Imported Console\"\n"
                "  Password = \"secret\"\n"
                "  Director = bareos-dir\n"
                "  TlsCipherList = \"DEFAULT\"\n"
                "  TlsCipherSuites = \"TLS_AES_128_GCM_SHA256\"\n"
                "  TlsDhFile = \"/etc/bareos/old-console-dh.pem\"\n"
                "  TlsProtocol = \"+TLSv1.2\"\n"
                "}\n"
                "\n"
                "Director {\n"
                "  Name = bareos-dir\n"
                "  Description = \"Imported Director\"\n"
                "  Address = localhost\n"
                "  Password = \"secret\"\n"
                "}\n");

  auto import_job
      = state.CreateJob({.type = "import_configuration",
                         .deployment_id = std::string{"prod"},
                         .source_path = source_root.path().string()});
  ASSERT_TRUE(import_job);
  auto imported = WaitForJobTerminal(state, import_job.value->id);
  ASSERT_TRUE(imported.has_value());
  ASSERT_EQ(imported->status, JobStatus::kSucceeded);

  auto updated = state.UpsertConsoleConsoleResource(
      "prod", "admin", "admin",
      {.description = std::string{"Updated imported console"},
       .tls_cipher_list = std::string{"DEFAULT:@SECLEVEL=2"},
       .tls_cipher_suites = std::string{"TLS_AES_256_GCM_SHA384"},
       .tls_dh_file = std::string{"/etc/bareos/new-console-dh.pem"},
       .tls_protocol = std::string{"+TLSv1.2:+TLSv1.3"}});
  ASSERT_TRUE(updated) << updated.error;

  const auto updated_text = ReadTextFile(updated.value->path / "bconsole.conf");
  EXPECT_NE(updated_text.find("Description = \"Updated imported console\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("TlsCipherList = \"DEFAULT:@SECLEVEL=2\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("TlsCipherSuites = \"TLS_AES_256_GCM_SHA384\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("TlsDhFile = \"/etc/bareos/new-console-dh.pem\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("TlsProtocol = \"+TLSv1.2:+TLSv1.3\""),
            std::string::npos);
}

TEST(BconfigService, UpsertsConsoleComponentConsoleTlsCertificatePaths)
{
  ScopedDirectory source_root{MakeTempPath()};
  ScopedDirectory repo_path{MakeTempPath()};
  ServiceState state;

  auto deployment
      = state.CreateDeployment({.id = "prod",
                                .name = "Production",
                                .repository_path = repo_path.path()});
  ASSERT_TRUE(deployment);

  WriteTextFile(source_root.path() / "bconsole.conf",
                "#\n"
                "# Bareos User Agent (or Console) Configuration File\n"
                "#\n"
                "\n"
                "Console {\n"
                "  Name = admin\n"
                "  Description = \"Imported Console\"\n"
                "  Password = \"secret\"\n"
                "  Director = bareos-dir\n"
                "  TlsCaCertificateFile = \"/etc/bareos/old-ca.pem\"\n"
                "  TlsCaCertificateDir = \"/etc/old-ssl/certs\"\n"
                "  TlsCertificateRevocationList = \"/etc/bareos/old-crl.pem\"\n"
                "  TlsCertificate = \"/etc/bareos/old-console-cert.pem\"\n"
                "  TlsKey = \"/etc/bareos/old-console-key.pem\"\n"
                "}\n"
                "\n"
                "Director {\n"
                "  Name = bareos-dir\n"
                "  Description = \"Imported Director\"\n"
                "  Address = localhost\n"
                "  Password = \"secret\"\n"
                "}\n");

  auto import_job
      = state.CreateJob({.type = "import_configuration",
                         .deployment_id = std::string{"prod"},
                         .source_path = source_root.path().string()});
  ASSERT_TRUE(import_job);
  auto imported = WaitForJobTerminal(state, import_job.value->id);
  ASSERT_TRUE(imported.has_value());
  ASSERT_EQ(imported->status, JobStatus::kSucceeded);

  auto updated = state.UpsertConsoleConsoleResource(
      "prod", "admin", "admin",
      {.description = std::string{"Updated imported console"},
       .tls_ca_certificate_file = std::string{"/etc/bareos/new-ca.pem"},
       .tls_ca_certificate_dir = std::string{"/etc/ssl/certs"},
       .tls_certificate_revocation_list
       = std::string{"/etc/bareos/new-crl.pem"},
       .tls_certificate = std::string{"/etc/bareos/new-console-cert.pem"},
       .tls_key = std::string{"/etc/bareos/new-console-key.pem"}});
  ASSERT_TRUE(updated) << updated.error;

  const auto updated_text = ReadTextFile(updated.value->path / "bconsole.conf");
  EXPECT_NE(updated_text.find("Description = \"Updated imported console\""),
            std::string::npos);
  EXPECT_NE(
      updated_text.find("TlsCaCertificateFile = \"/etc/bareos/new-ca.pem\""),
      std::string::npos);
  EXPECT_NE(updated_text.find("TlsCaCertificateDir = \"/etc/ssl/certs\""),
            std::string::npos);
  EXPECT_NE(updated_text.find(
                "TlsCertificateRevocationList = \"/etc/bareos/new-crl.pem\""),
            std::string::npos);
  EXPECT_NE(updated_text.find(
                "TlsCertificate = \"/etc/bareos/new-console-cert.pem\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("TlsKey = \"/etc/bareos/new-console-key.pem\""),
            std::string::npos);
}

TEST(BconfigService, UpsertsConsoleComponentConsoleTlsAllowedCn)
{
  ScopedDirectory source_root{MakeTempPath()};
  ScopedDirectory repo_path{MakeTempPath()};
  ServiceState state;

  auto deployment
      = state.CreateDeployment({.id = "prod",
                                .name = "Production",
                                .repository_path = repo_path.path()});
  ASSERT_TRUE(deployment);

  WriteTextFile(source_root.path() / "bconsole.conf",
                "#\n"
                "# Bareos User Agent (or Console) Configuration File\n"
                "#\n"
                "\n"
                "Console {\n"
                "  Name = admin\n"
                "  Description = \"Imported Console\"\n"
                "  Password = \"secret\"\n"
                "  Director = bareos-dir\n"
                "  TlsAllowedCn = \"old-console.example.test\"\n"
                "}\n"
                "\n"
                "Director {\n"
                "  Name = bareos-dir\n"
                "  Description = \"Imported Director\"\n"
                "  Address = localhost\n"
                "  Password = \"secret\"\n"
                "}\n");

  auto import_job
      = state.CreateJob({.type = "import_configuration",
                         .deployment_id = std::string{"prod"},
                         .source_path = source_root.path().string()});
  ASSERT_TRUE(import_job);
  auto imported = WaitForJobTerminal(state, import_job.value->id);
  ASSERT_TRUE(imported.has_value());
  ASSERT_EQ(imported->status, JobStatus::kSucceeded);

  auto updated = state.UpsertConsoleConsoleResource(
      "prod", "admin", "admin",
      {.description = std::string{"Updated imported console"},
       .tls_allowed_cn = std::vector<std::string>{"console.example.test",
                                                  "director.example.test"}});
  ASSERT_TRUE(updated) << updated.error;

  const auto updated_text = ReadTextFile(updated.value->path / "bconsole.conf");
  EXPECT_NE(updated_text.find("Description = \"Updated imported console\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("TlsAllowedCn = \"console.example.test\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("TlsAllowedCn = \"director.example.test\""),
            std::string::npos);
  EXPECT_EQ(updated_text.find("TlsAllowedCn = \"old-console.example.test\""),
            std::string::npos);
}

TEST(BconfigService,
     RewritesConsoleComponentConsoleResourcesWithoutDroppingOthers)
{
  ScopedDirectory source_root{MakeTempPath()};
  ScopedDirectory repo_path{MakeTempPath()};
  ServiceState state;

  auto deployment
      = state.CreateDeployment({.id = "prod",
                                .name = "Production",
                                .repository_path = repo_path.path()});
  ASSERT_TRUE(deployment);

  AddConsoleImportFixture(source_root.path());

  auto import_job
      = state.CreateJob({.type = "import_configuration",
                         .deployment_id = std::string{"prod"},
                         .source_path = source_root.path().string()});
  ASSERT_TRUE(import_job);
  auto imported = WaitForJobTerminal(state, import_job.value->id);
  ASSERT_TRUE(imported.has_value());
  ASSERT_EQ(imported->status, JobStatus::kSucceeded);

  auto created = state.UpsertConsoleConsoleResource(
      "prod", "admin", "managed-console",
      {.director = std::string{"bareos-dir"},
       .password = std::string{"[md5]abcdef0123456789abcdef0123456789"},
       .description = std::string{"Managed bconsole resource"}});
  ASSERT_TRUE(created) << created.error;

  auto updated = state.UpsertConsoleConsoleResource(
      "prod", "admin", "admin",
      {.description = std::string{"Updated imported console"}});
  ASSERT_TRUE(updated) << updated.error;

  const auto rewritten_text
      = ReadTextFile(updated.value->path / "bconsole.conf");
  EXPECT_NE(rewritten_text.find("Description = \"Updated imported console\""),
            std::string::npos);
  EXPECT_NE(rewritten_text.find("Name = \"managed-console\""),
            std::string::npos);
  EXPECT_NE(rewritten_text.find("Description = \"Managed bconsole resource\""),
            std::string::npos);
  EXPECT_NE(rewritten_text.find("Name = \"bareos-dir\""), std::string::npos);
}

TEST(BconfigService, DeletesConsoleComponentConsoleResources)
{
  ScopedDirectory source_root{MakeTempPath()};
  ScopedDirectory repo_path{MakeTempPath()};
  ServiceState state;

  auto deployment
      = state.CreateDeployment({.id = "prod",
                                .name = "Production",
                                .repository_path = repo_path.path()});
  ASSERT_TRUE(deployment);

  AddConsoleImportFixture(source_root.path());

  auto import_job
      = state.CreateJob({.type = "import_configuration",
                         .deployment_id = std::string{"prod"},
                         .source_path = source_root.path().string()});
  ASSERT_TRUE(import_job);
  auto imported = WaitForJobTerminal(state, import_job.value->id);
  ASSERT_TRUE(imported.has_value());
  ASSERT_EQ(imported->status, JobStatus::kSucceeded);

  auto created = state.UpsertConsoleConsoleResource(
      "prod", "admin", "managed-console",
      {.director = std::string{"bareos-dir"},
       .password = std::string{"[md5]abcdef0123456789abcdef0123456789"}});
  ASSERT_TRUE(created) << created.error;

  const auto created_text = ReadTextFile(created.value->path / "bconsole.conf");
  EXPECT_NE(created_text.find("Name = \"managed-console\""), std::string::npos);

  auto deleted
      = state.DeleteConsoleConsoleResource("prod", "admin", "managed-console");
  ASSERT_TRUE(deleted) << deleted.error;
  const auto deleted_text = ReadTextFile(deleted.value->path / "bconsole.conf");
  EXPECT_EQ(deleted_text.find("Name = \"managed-console\""), std::string::npos);
  EXPECT_NE(deleted_text.find("Name = \"admin\""), std::string::npos);
  EXPECT_NE(deleted_text.find("Name = \"bareos-dir\""), std::string::npos);
}

TEST(BconfigService,
     DeletesConsoleComponentConsoleResourcesWithoutDroppingOthers)
{
  ScopedDirectory source_root{MakeTempPath()};
  ScopedDirectory repo_path{MakeTempPath()};
  ServiceState state;

  auto deployment
      = state.CreateDeployment({.id = "prod",
                                .name = "Production",
                                .repository_path = repo_path.path()});
  ASSERT_TRUE(deployment);

  AddConsoleImportFixture(source_root.path());

  auto import_job
      = state.CreateJob({.type = "import_configuration",
                         .deployment_id = std::string{"prod"},
                         .source_path = source_root.path().string()});
  ASSERT_TRUE(import_job);
  auto imported = WaitForJobTerminal(state, import_job.value->id);
  ASSERT_TRUE(imported.has_value());
  ASSERT_EQ(imported->status, JobStatus::kSucceeded);

  auto created = state.UpsertConsoleConsoleResource(
      "prod", "admin", "managed-console",
      {.director = std::string{"bareos-dir"},
       .password = std::string{"[md5]abcdef0123456789abcdef0123456789"}});
  ASSERT_TRUE(created) << created.error;

  auto deleted = state.DeleteConsoleConsoleResource("prod", "admin", "admin");
  ASSERT_TRUE(deleted) << deleted.error;
  const auto deleted_text = ReadTextFile(deleted.value->path / "bconsole.conf");
  EXPECT_EQ(deleted_text.find("Name = \"admin\""), std::string::npos);
  EXPECT_NE(deleted_text.find("Name = \"managed-console\""), std::string::npos);
  EXPECT_NE(deleted_text.find("Name = \"bareos-dir\""), std::string::npos);
}

TEST(BconfigService, UpsertsConsoleComponentDirectorResources)
{
  ScopedDirectory source_root{MakeTempPath()};
  ScopedDirectory repo_path{MakeTempPath()};
  ServiceState state;

  auto deployment
      = state.CreateDeployment({.id = "prod",
                                .name = "Production",
                                .repository_path = repo_path.path()});
  ASSERT_TRUE(deployment);

  AddConsoleImportFixture(source_root.path());

  auto import_job
      = state.CreateJob({.type = "import_configuration",
                         .deployment_id = std::string{"prod"},
                         .source_path = source_root.path().string()});
  ASSERT_TRUE(import_job);
  auto imported = WaitForJobTerminal(state, import_job.value->id);
  ASSERT_TRUE(imported.has_value());
  ASSERT_EQ(imported->status, JobStatus::kSucceeded);

  auto created = state.UpsertConsoleDirectorResource(
      "prod", "admin", "managed-dir",
      {.address = std::string{"localhost"},
       .port = 9101,
       .password = std::string{"managed_password"},
       .description = std::string{"Managed bconsole director"},
       .heartbeat_interval = 25,
       .tls_authenticate = false,
       .tls_enable = true,
       .tls_require = false,
       .tls_verify_peer = true,
       .tls_cipher_list = std::string{"DEFAULT:@SECLEVEL=2"},
       .tls_cipher_suites = std::string{"TLS_AES_256_GCM_SHA384"},
       .tls_dh_file = std::string{"/etc/bareos/director-dh.pem"},
       .tls_protocol = std::string{"+TLSv1.2:+TLSv1.3"},
       .tls_ca_certificate_file = std::string{"/etc/bareos/ca.pem"},
       .tls_ca_certificate_dir = std::string{"/etc/ssl/certs"},
       .tls_certificate_revocation_list = std::string{"/etc/bareos/crl.pem"},
       .tls_certificate = std::string{"/etc/bareos/director-cert.pem"},
       .tls_key = std::string{"/etc/bareos/director-key.pem"},
       .tls_allowed_cn = std::vector<std::string>{"director.example.test",
                                                  "backup.example.test"}});
  ASSERT_TRUE(created) << created.error;
  EXPECT_EQ(created.value->name, "admin");

  const auto created_text = ReadTextFile(created.value->path / "bconsole.conf");
  EXPECT_NE(created_text.find("Name = \"managed-dir\""), std::string::npos);
  EXPECT_NE(created_text.find("Address = localhost"), std::string::npos);
  EXPECT_NE(created_text.find("Port = 9101"), std::string::npos);
  EXPECT_NE(created_text.find("Password = \"managed_password\""),
            std::string::npos);
  EXPECT_NE(created_text.find("Description = \"Managed bconsole director\""),
            std::string::npos);
  EXPECT_NE(created_text.find("HeartbeatInterval = 25"), std::string::npos);
  EXPECT_NE(created_text.find("TlsAuthenticate = no"), std::string::npos);
  EXPECT_NE(created_text.find("TlsEnable = yes"), std::string::npos);
  EXPECT_NE(created_text.find("TlsRequire = no"), std::string::npos);
  EXPECT_NE(created_text.find("TlsVerifyPeer = yes"), std::string::npos);
  EXPECT_NE(created_text.find("TlsCipherList = \"DEFAULT:@SECLEVEL=2\""),
            std::string::npos);
  EXPECT_NE(created_text.find("TlsCipherSuites = \"TLS_AES_256_GCM_SHA384\""),
            std::string::npos);
  EXPECT_NE(created_text.find("TlsDhFile = \"/etc/bareos/director-dh.pem\""),
            std::string::npos);
  EXPECT_NE(created_text.find("TlsProtocol = \"+TLSv1.2:+TLSv1.3\""),
            std::string::npos);
  EXPECT_NE(created_text.find("TlsCaCertificateFile = \"/etc/bareos/ca.pem\""),
            std::string::npos);
  EXPECT_NE(created_text.find("TlsCaCertificateDir = \"/etc/ssl/certs\""),
            std::string::npos);
  EXPECT_NE(created_text.find(
                "TlsCertificateRevocationList = \"/etc/bareos/crl.pem\""),
            std::string::npos);
  EXPECT_NE(
      created_text.find("TlsCertificate = \"/etc/bareos/director-cert.pem\""),
      std::string::npos);
  EXPECT_NE(created_text.find("TlsKey = \"/etc/bareos/director-key.pem\""),
            std::string::npos);
  EXPECT_NE(created_text.find("TlsAllowedCn = \"director.example.test\""),
            std::string::npos);
  EXPECT_NE(created_text.find("TlsAllowedCn = \"backup.example.test\""),
            std::string::npos);

  auto updated = state.UpsertConsoleDirectorResource(
      "prod", "admin", "bareos-dir",
      {.address = std::string{"director.example.test"},
       .description = std::string{"Updated imported director"}});
  ASSERT_TRUE(updated) << updated.error;

  const auto updated_text = ReadTextFile(updated.value->path / "bconsole.conf");
  EXPECT_NE(updated_text.find("Name = \"bareos-dir\""), std::string::npos);
  EXPECT_NE(updated_text.find("Address = director.example.test"),
            std::string::npos);
  EXPECT_NE(updated_text.find("Description = \"Updated imported director\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("Name = \"managed-dir\""), std::string::npos);
  EXPECT_NE(updated_text.find("Name = \"admin\""), std::string::npos);

  auto current
      = state.GetConsoleDirectorResourceSpec("prod", "admin", "bareos-dir");
  ASSERT_TRUE(current) << current.error;
  EXPECT_EQ(current.value->address, "director.example.test");
  EXPECT_EQ(current.value->description, "Updated imported director");
  ASSERT_TRUE(current.value->password.has_value());
  EXPECT_FALSE(current.value->password->empty());
}

TEST(BconfigService, UpsertsConsoleComponentDirectorHeartbeatInterval)
{
  ScopedDirectory source_root{MakeTempPath()};
  ScopedDirectory repo_path{MakeTempPath()};
  ServiceState state;

  auto deployment
      = state.CreateDeployment({.id = "prod",
                                .name = "Production",
                                .repository_path = repo_path.path()});
  ASSERT_TRUE(deployment);

  WriteTextFile(source_root.path() / "bconsole.conf",
                "#\n"
                "# Bareos User Agent (or Console) Configuration File\n"
                "#\n"
                "\n"
                "Console {\n"
                "  Name = admin\n"
                "  Description = \"Imported Console\"\n"
                "  Password = \"secret\"\n"
                "  Director = bareos-dir\n"
                "}\n"
                "\n"
                "Director {\n"
                "  Name = bareos-dir\n"
                "  Description = \"Imported Director\"\n"
                "  Address = localhost\n"
                "  Password = \"secret\"\n"
                "  HeartbeatInterval = 45\n"
                "}\n");

  auto import_job
      = state.CreateJob({.type = "import_configuration",
                         .deployment_id = std::string{"prod"},
                         .source_path = source_root.path().string()});
  ASSERT_TRUE(import_job);
  auto imported = WaitForJobTerminal(state, import_job.value->id);
  ASSERT_TRUE(imported.has_value());
  ASSERT_EQ(imported->status, JobStatus::kSucceeded);

  auto updated = state.UpsertConsoleDirectorResource(
      "prod", "admin", "bareos-dir",
      {.address = std::string{"director.example.test"},
       .description = std::string{"Updated imported director"},
       .heartbeat_interval = 90});
  ASSERT_TRUE(updated) << updated.error;

  const auto updated_text = ReadTextFile(updated.value->path / "bconsole.conf");
  EXPECT_NE(updated_text.find("Address = director.example.test"),
            std::string::npos);
  EXPECT_NE(updated_text.find("HeartbeatInterval = 90"), std::string::npos);
  EXPECT_NE(updated_text.find("Description = \"Updated imported director\""),
            std::string::npos);
}

TEST(BconfigService, UpsertsConsoleComponentDirectorPreservesLargeImportedPort)
{
  ScopedDirectory source_root{MakeTempPath()};
  ScopedDirectory repo_path{MakeTempPath()};
  ServiceState state;

  auto deployment
      = state.CreateDeployment({.id = "prod",
                                .name = "Production",
                                .repository_path = repo_path.path()});
  ASSERT_TRUE(deployment);

  WriteTextFile(source_root.path() / "bconsole.conf",
                "#\n"
                "# Bareos User Agent (or Console) Configuration File\n"
                "#\n"
                "\n"
                "Console {\n"
                "  Name = admin\n"
                "  Description = \"Imported Console\"\n"
                "  Password = \"secret\"\n"
                "  Director = bareos-dir\n"
                "}\n"
                "\n"
                "Director {\n"
                "  Name = bareos-dir\n"
                "  Description = \"Imported Director\"\n"
                "  Address = localhost\n"
                "  Port = 70000\n"
                "  Password = \"secret\"\n"
                "}\n");

  auto import_job
      = state.CreateJob({.type = "import_configuration",
                         .deployment_id = std::string{"prod"},
                         .source_path = source_root.path().string()});
  ASSERT_TRUE(import_job);
  auto imported = WaitForJobTerminal(state, import_job.value->id);
  ASSERT_TRUE(imported.has_value());
  ASSERT_EQ(imported->status, JobStatus::kSucceeded);

  auto updated = state.UpsertConsoleDirectorResource(
      "prod", "admin", "bareos-dir",
      {.description = std::string{"Updated imported director"}});
  ASSERT_TRUE(updated) << updated.error;

  const auto updated_text = ReadTextFile(updated.value->path / "bconsole.conf");
  EXPECT_NE(updated_text.find("Address = localhost"), std::string::npos);
  EXPECT_NE(updated_text.find("Port = 70000"), std::string::npos);
  EXPECT_NE(updated_text.find("Description = \"Updated imported director\""),
            std::string::npos);
}

TEST(BconfigService, UpsertsConsoleComponentDirectorTlsBooleans)
{
  ScopedDirectory source_root{MakeTempPath()};
  ScopedDirectory repo_path{MakeTempPath()};
  ServiceState state;

  auto deployment
      = state.CreateDeployment({.id = "prod",
                                .name = "Production",
                                .repository_path = repo_path.path()});
  ASSERT_TRUE(deployment);

  WriteTextFile(source_root.path() / "bconsole.conf",
                "#\n"
                "# Bareos User Agent (or Console) Configuration File\n"
                "#\n"
                "\n"
                "Console {\n"
                "  Name = admin\n"
                "  Description = \"Imported Console\"\n"
                "  Password = \"secret\"\n"
                "  Director = bareos-dir\n"
                "}\n"
                "\n"
                "Director {\n"
                "  Name = bareos-dir\n"
                "  Description = \"Imported Director\"\n"
                "  Address = localhost\n"
                "  Password = \"secret\"\n"
                "  TlsAuthenticate = yes\n"
                "  TlsEnable = no\n"
                "  TlsRequire = yes\n"
                "  TlsVerifyPeer = no\n"
                "}\n");

  auto import_job
      = state.CreateJob({.type = "import_configuration",
                         .deployment_id = std::string{"prod"},
                         .source_path = source_root.path().string()});
  ASSERT_TRUE(import_job);
  auto imported = WaitForJobTerminal(state, import_job.value->id);
  ASSERT_TRUE(imported.has_value());
  ASSERT_EQ(imported->status, JobStatus::kSucceeded);

  auto updated = state.UpsertConsoleDirectorResource(
      "prod", "admin", "bareos-dir",
      {.address = std::string{"director.example.test"},
       .description = std::string{"Updated imported director"},
       .tls_authenticate = false,
       .tls_enable = true,
       .tls_require = false,
       .tls_verify_peer = true});
  ASSERT_TRUE(updated) << updated.error;

  const auto updated_text = ReadTextFile(updated.value->path / "bconsole.conf");
  EXPECT_NE(updated_text.find("Address = director.example.test"),
            std::string::npos);
  EXPECT_NE(updated_text.find("Description = \"Updated imported director\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("TlsAuthenticate = no"), std::string::npos);
  EXPECT_NE(updated_text.find("TlsEnable = yes"), std::string::npos);
  EXPECT_NE(updated_text.find("TlsRequire = no"), std::string::npos);
  EXPECT_NE(updated_text.find("TlsVerifyPeer = yes"), std::string::npos);
}

TEST(BconfigService, UpsertsConsoleComponentDirectorTlsStrings)
{
  ScopedDirectory source_root{MakeTempPath()};
  ScopedDirectory repo_path{MakeTempPath()};
  ServiceState state;

  auto deployment
      = state.CreateDeployment({.id = "prod",
                                .name = "Production",
                                .repository_path = repo_path.path()});
  ASSERT_TRUE(deployment);

  WriteTextFile(source_root.path() / "bconsole.conf",
                "#\n"
                "# Bareos User Agent (or Console) Configuration File\n"
                "#\n"
                "\n"
                "Console {\n"
                "  Name = admin\n"
                "  Description = \"Imported Console\"\n"
                "  Password = \"secret\"\n"
                "  Director = bareos-dir\n"
                "}\n"
                "\n"
                "Director {\n"
                "  Name = bareos-dir\n"
                "  Description = \"Imported Director\"\n"
                "  Address = localhost\n"
                "  Password = \"secret\"\n"
                "  TlsCipherList = \"DEFAULT\"\n"
                "  TlsCipherSuites = \"TLS_AES_128_GCM_SHA256\"\n"
                "  TlsDhFile = \"/etc/bareos/old-director-dh.pem\"\n"
                "  TlsProtocol = \"+TLSv1.2\"\n"
                "}\n");

  auto import_job
      = state.CreateJob({.type = "import_configuration",
                         .deployment_id = std::string{"prod"},
                         .source_path = source_root.path().string()});
  ASSERT_TRUE(import_job);
  auto imported = WaitForJobTerminal(state, import_job.value->id);
  ASSERT_TRUE(imported.has_value());
  ASSERT_EQ(imported->status, JobStatus::kSucceeded);

  auto updated = state.UpsertConsoleDirectorResource(
      "prod", "admin", "bareos-dir",
      {.address = std::string{"director.example.test"},
       .description = std::string{"Updated imported director"},
       .tls_cipher_list = std::string{"DEFAULT:@SECLEVEL=2"},
       .tls_cipher_suites = std::string{"TLS_AES_256_GCM_SHA384"},
       .tls_dh_file = std::string{"/etc/bareos/new-director-dh.pem"},
       .tls_protocol = std::string{"+TLSv1.2:+TLSv1.3"}});
  ASSERT_TRUE(updated) << updated.error;

  const auto updated_text = ReadTextFile(updated.value->path / "bconsole.conf");
  EXPECT_NE(updated_text.find("Address = director.example.test"),
            std::string::npos);
  EXPECT_NE(updated_text.find("Description = \"Updated imported director\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("TlsCipherList = \"DEFAULT:@SECLEVEL=2\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("TlsCipherSuites = \"TLS_AES_256_GCM_SHA384\""),
            std::string::npos);
  EXPECT_NE(
      updated_text.find("TlsDhFile = \"/etc/bareos/new-director-dh.pem\""),
      std::string::npos);
  EXPECT_NE(updated_text.find("TlsProtocol = \"+TLSv1.2:+TLSv1.3\""),
            std::string::npos);
}

TEST(BconfigService, UpsertsConsoleComponentDirectorTlsCertificatePaths)
{
  ScopedDirectory source_root{MakeTempPath()};
  ScopedDirectory repo_path{MakeTempPath()};
  ServiceState state;

  auto deployment
      = state.CreateDeployment({.id = "prod",
                                .name = "Production",
                                .repository_path = repo_path.path()});
  ASSERT_TRUE(deployment);

  WriteTextFile(source_root.path() / "bconsole.conf",
                "#\n"
                "# Bareos User Agent (or Console) Configuration File\n"
                "#\n"
                "\n"
                "Console {\n"
                "  Name = admin\n"
                "  Description = \"Imported Console\"\n"
                "  Password = \"secret\"\n"
                "  Director = bareos-dir\n"
                "}\n"
                "\n"
                "Director {\n"
                "  Name = bareos-dir\n"
                "  Description = \"Imported Director\"\n"
                "  Address = localhost\n"
                "  Password = \"secret\"\n"
                "  TlsCaCertificateFile = \"/etc/bareos/old-ca.pem\"\n"
                "  TlsCaCertificateDir = \"/etc/old-ssl/certs\"\n"
                "  TlsCertificateRevocationList = \"/etc/bareos/old-crl.pem\"\n"
                "  TlsCertificate = \"/etc/bareos/old-director-cert.pem\"\n"
                "  TlsKey = \"/etc/bareos/old-director-key.pem\"\n"
                "}\n");

  auto import_job
      = state.CreateJob({.type = "import_configuration",
                         .deployment_id = std::string{"prod"},
                         .source_path = source_root.path().string()});
  ASSERT_TRUE(import_job);
  auto imported = WaitForJobTerminal(state, import_job.value->id);
  ASSERT_TRUE(imported.has_value());
  ASSERT_EQ(imported->status, JobStatus::kSucceeded);

  auto updated = state.UpsertConsoleDirectorResource(
      "prod", "admin", "bareos-dir",
      {.address = std::string{"director.example.test"},
       .description = std::string{"Updated imported director"},
       .tls_ca_certificate_file = std::string{"/etc/bareos/new-ca.pem"},
       .tls_ca_certificate_dir = std::string{"/etc/ssl/certs"},
       .tls_certificate_revocation_list
       = std::string{"/etc/bareos/new-crl.pem"},
       .tls_certificate = std::string{"/etc/bareos/new-director-cert.pem"},
       .tls_key = std::string{"/etc/bareos/new-director-key.pem"}});
  ASSERT_TRUE(updated) << updated.error;

  const auto updated_text = ReadTextFile(updated.value->path / "bconsole.conf");
  EXPECT_NE(updated_text.find("Address = director.example.test"),
            std::string::npos);
  EXPECT_NE(updated_text.find("Description = \"Updated imported director\""),
            std::string::npos);
  EXPECT_NE(
      updated_text.find("TlsCaCertificateFile = \"/etc/bareos/new-ca.pem\""),
      std::string::npos);
  EXPECT_NE(updated_text.find("TlsCaCertificateDir = \"/etc/ssl/certs\""),
            std::string::npos);
  EXPECT_NE(updated_text.find(
                "TlsCertificateRevocationList = \"/etc/bareos/new-crl.pem\""),
            std::string::npos);
  EXPECT_NE(updated_text.find(
                "TlsCertificate = \"/etc/bareos/new-director-cert.pem\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("TlsKey = \"/etc/bareos/new-director-key.pem\""),
            std::string::npos);
}

TEST(BconfigService, UpsertsConsoleComponentDirectorTlsAllowedCn)
{
  ScopedDirectory source_root{MakeTempPath()};
  ScopedDirectory repo_path{MakeTempPath()};
  ServiceState state;

  auto deployment
      = state.CreateDeployment({.id = "prod",
                                .name = "Production",
                                .repository_path = repo_path.path()});
  ASSERT_TRUE(deployment);

  WriteTextFile(source_root.path() / "bconsole.conf",
                "#\n"
                "# Bareos User Agent (or Console) Configuration File\n"
                "#\n"
                "\n"
                "Console {\n"
                "  Name = admin\n"
                "  Description = \"Imported Console\"\n"
                "  Password = \"secret\"\n"
                "  Director = bareos-dir\n"
                "}\n"
                "\n"
                "Director {\n"
                "  Name = bareos-dir\n"
                "  Description = \"Imported Director\"\n"
                "  Address = localhost\n"
                "  Password = \"secret\"\n"
                "  TlsAllowedCn = \"old-director.example.test\"\n"
                "}\n");

  auto import_job
      = state.CreateJob({.type = "import_configuration",
                         .deployment_id = std::string{"prod"},
                         .source_path = source_root.path().string()});
  ASSERT_TRUE(import_job);
  auto imported = WaitForJobTerminal(state, import_job.value->id);
  ASSERT_TRUE(imported.has_value());
  ASSERT_EQ(imported->status, JobStatus::kSucceeded);

  auto updated = state.UpsertConsoleDirectorResource(
      "prod", "admin", "bareos-dir",
      {.address = std::string{"director.example.test"},
       .description = std::string{"Updated imported director"},
       .tls_allowed_cn = std::vector<std::string>{"director.example.test",
                                                  "backup.example.test"}});
  ASSERT_TRUE(updated) << updated.error;

  const auto updated_text = ReadTextFile(updated.value->path / "bconsole.conf");
  EXPECT_NE(updated_text.find("Address = director.example.test"),
            std::string::npos);
  EXPECT_NE(updated_text.find("Description = \"Updated imported director\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("TlsAllowedCn = \"director.example.test\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("TlsAllowedCn = \"backup.example.test\""),
            std::string::npos);
  EXPECT_EQ(updated_text.find("TlsAllowedCn = \"old-director.example.test\""),
            std::string::npos);
}

TEST(BconfigService,
     RewritesConsoleComponentDirectorResourcesWithoutDroppingOthers)
{
  ScopedDirectory source_root{MakeTempPath()};
  ScopedDirectory repo_path{MakeTempPath()};
  ServiceState state;

  auto deployment
      = state.CreateDeployment({.id = "prod",
                                .name = "Production",
                                .repository_path = repo_path.path()});
  ASSERT_TRUE(deployment);

  AddConsoleImportFixture(source_root.path());

  auto import_job
      = state.CreateJob({.type = "import_configuration",
                         .deployment_id = std::string{"prod"},
                         .source_path = source_root.path().string()});
  ASSERT_TRUE(import_job);
  auto imported = WaitForJobTerminal(state, import_job.value->id);
  ASSERT_TRUE(imported.has_value());
  ASSERT_EQ(imported->status, JobStatus::kSucceeded);

  auto created = state.UpsertConsoleDirectorResource(
      "prod", "admin", "managed-dir",
      {.address = std::string{"localhost"},
       .description = std::string{"Managed bconsole director"}});
  ASSERT_TRUE(created) << created.error;

  auto updated = state.UpsertConsoleDirectorResource(
      "prod", "admin", "bareos-dir",
      {.address = std::string{"director.example.test"},
       .description = std::string{"Updated imported director"}});
  ASSERT_TRUE(updated) << updated.error;

  const auto rewritten_text
      = ReadTextFile(updated.value->path / "bconsole.conf");
  EXPECT_NE(rewritten_text.find("Name = \"managed-dir\""), std::string::npos);
  EXPECT_NE(rewritten_text.find("Description = \"Managed bconsole director\""),
            std::string::npos);
  EXPECT_NE(rewritten_text.find("Name = \"admin\""), std::string::npos);
  EXPECT_NE(rewritten_text.find("Name = \"bareos-dir\""), std::string::npos);
  EXPECT_NE(rewritten_text.find("Address = director.example.test"),
            std::string::npos);
}

TEST(BconfigService, DeletesConsoleComponentDirectorResources)
{
  ScopedDirectory source_root{MakeTempPath()};
  ScopedDirectory repo_path{MakeTempPath()};
  ServiceState state;

  auto deployment
      = state.CreateDeployment({.id = "prod",
                                .name = "Production",
                                .repository_path = repo_path.path()});
  ASSERT_TRUE(deployment);

  AddConsoleImportFixture(source_root.path());

  auto import_job
      = state.CreateJob({.type = "import_configuration",
                         .deployment_id = std::string{"prod"},
                         .source_path = source_root.path().string()});
  ASSERT_TRUE(import_job);
  auto imported = WaitForJobTerminal(state, import_job.value->id);
  ASSERT_TRUE(imported.has_value());
  ASSERT_EQ(imported->status, JobStatus::kSucceeded);

  auto created = state.UpsertConsoleDirectorResource(
      "prod", "admin", "managed-dir",
      {.address = std::string{"localhost"},
       .password = std::string{"managed_password"}});
  ASSERT_TRUE(created) << created.error;

  const auto created_text = ReadTextFile(created.value->path / "bconsole.conf");
  EXPECT_NE(created_text.find("Name = \"managed-dir\""), std::string::npos);

  auto deleted
      = state.DeleteConsoleDirectorResource("prod", "admin", "managed-dir");
  ASSERT_TRUE(deleted) << deleted.error;

  const auto deleted_text = ReadTextFile(deleted.value->path / "bconsole.conf");
  EXPECT_EQ(deleted_text.find("Name = \"managed-dir\""), std::string::npos);
  EXPECT_NE(deleted_text.find("Name = \"bareos-dir\""), std::string::npos);
  EXPECT_NE(deleted_text.find("Name = \"admin\""), std::string::npos);
}

TEST(BconfigService,
     DeletesConsoleComponentDirectorResourcesWithoutDroppingOthers)
{
  ScopedDirectory source_root{MakeTempPath()};
  ScopedDirectory repo_path{MakeTempPath()};
  ServiceState state;

  auto deployment
      = state.CreateDeployment({.id = "prod",
                                .name = "Production",
                                .repository_path = repo_path.path()});
  ASSERT_TRUE(deployment);

  AddConsoleImportFixture(source_root.path());

  auto import_job
      = state.CreateJob({.type = "import_configuration",
                         .deployment_id = std::string{"prod"},
                         .source_path = source_root.path().string()});
  ASSERT_TRUE(import_job);
  auto imported = WaitForJobTerminal(state, import_job.value->id);
  ASSERT_TRUE(imported.has_value());
  ASSERT_EQ(imported->status, JobStatus::kSucceeded);

  auto created = state.UpsertConsoleDirectorResource(
      "prod", "admin", "managed-dir",
      {.address = std::string{"localhost"},
       .description = std::string{"Managed bconsole director"}});
  ASSERT_TRUE(created) << created.error;

  auto deleted
      = state.DeleteConsoleDirectorResource("prod", "admin", "managed-dir");
  ASSERT_TRUE(deleted) << deleted.error;

  const auto deleted_text = ReadTextFile(deleted.value->path / "bconsole.conf");
  EXPECT_EQ(deleted_text.find("Name = \"managed-dir\""), std::string::npos);
  EXPECT_NE(deleted_text.find("Name = \"bareos-dir\""), std::string::npos);
  EXPECT_NE(deleted_text.find("Name = \"admin\""), std::string::npos);
}

}  // namespace
}  // namespace bconfig::service
