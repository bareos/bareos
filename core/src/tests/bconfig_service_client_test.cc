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
TEST(BconfigService, ListsAndFindsDeploymentClientConfigs)
{
  ScopedDirectory source_root{MakeTempPath()};
  ScopedDirectory repo_path{MakeTempPath()};
  ServiceState state;

  auto deployment
      = state.CreateDeployment({.id = "prod",
                                .name = "Production",
                                .repository_path = repo_path.path()});
  ASSERT_TRUE(deployment);

  const auto source_fixture_root = FindFixtureRoot();
  ASSERT_FALSE(source_fixture_root.empty());
  std::filesystem::create_directories(source_root.path());
  std::filesystem::copy(source_fixture_root / "bareos-dir.d",
                        source_root.path() / "bareos-dir.d",
                        std::filesystem::copy_options::recursive);
  std::filesystem::copy(source_fixture_root / "bareos-fd.d",
                        source_root.path() / "bareos-fd.d",
                        std::filesystem::copy_options::recursive);

  auto import_job
      = state.CreateJob({.type = "import_configuration",
                         .deployment_id = std::string{"prod"},
                         .source_path = source_root.path().string()});
  ASSERT_TRUE(import_job);
  auto imported = WaitForJobTerminal(state, import_job.value->id);
  ASSERT_TRUE(imported.has_value());
  ASSERT_EQ(imported->status, JobStatus::kSucceeded);

  auto clients
      = state.ListDeploymentConfigs("prod", bconfig::Component::kClient);
  ASSERT_TRUE(clients);
  ASSERT_EQ(clients.value->size(), 5u);
  EXPECT_EQ(clients.value->at(0).name, "backup-bareos-test-fd");
  EXPECT_EQ(clients.value->at(1).name, "bareos-fd");
  EXPECT_EQ(clients.value->at(2).name, "bareos-fd-duplicate-interface");
  EXPECT_EQ(clients.value->at(3).name, "bareos-fd2");
  EXPECT_EQ(clients.value->at(4).name, "bareos-fd3");

  auto client = state.GetDeploymentConfig("prod", bconfig::Component::kClient,
                                          "bareos-fd2");
  ASSERT_TRUE(client);
  EXPECT_EQ(client.value->component, bconfig::Component::kClient);
  EXPECT_EQ(client.value->name, "bareos-fd2");
  EXPECT_TRUE(std::filesystem::exists(
      client.value->path / "bareos-fd.d/director/bareos-dir.conf"));

  auto missing = state.GetDeploymentConfig("prod", bconfig::Component::kClient,
                                           "missing-client");
  ASSERT_FALSE(missing);
  EXPECT_EQ(missing.error, "deployment config not found.");
}

TEST(BconfigService, UpsertsClientMessagesResources)
{
  ScopedDirectory source_root{MakeTempPath()};
  ScopedDirectory repo_path{MakeTempPath()};
  ServiceState state;

  auto deployment
      = state.CreateDeployment({.id = "prod",
                                .name = "Production",
                                .repository_path = repo_path.path()});
  ASSERT_TRUE(deployment);

  const auto source_fixture_root = FindFixtureRoot();
  ASSERT_FALSE(source_fixture_root.empty());
  std::filesystem::create_directories(source_root.path());
  std::filesystem::copy(source_fixture_root / "bareos-fd.d",
                        source_root.path() / "bareos-fd.d",
                        std::filesystem::copy_options::recursive);

  auto import_job
      = state.CreateJob({.type = "import_configuration",
                         .deployment_id = std::string{"prod"},
                         .source_path = source_root.path().string()});
  ASSERT_TRUE(import_job);
  auto imported = WaitForJobTerminal(state, import_job.value->id);
  ASSERT_TRUE(imported.has_value());
  ASSERT_EQ(imported->status, JobStatus::kSucceeded);

  auto created = state.UpsertClientMessagesResource(
      "prod", "backup-bareos-test-fd", "ManagedMessages",
      {.description = std::string{"Managed client messages"},
       .mail_command = std::string{"/usr/lib/bareos/client-mail %r"},
       .operator_command = std::string{"/usr/lib/bareos/client-operator %r"},
       .timestamp_format = std::string{"%H:%M:%S"},
       .stdout_entries = std::vector<std::string>{"all, !restored"},
       .director_entries
       = std::vector<std::string>{"bareos-dir = all, !skipped"}});
  ASSERT_TRUE(created) << created.error;
  EXPECT_EQ(created.value->name, "backup-bareos-test-fd");

  const auto messages_path
      = created.value->path / "bareos-fd.d/messages/ManagedMessages.conf";
  const auto created_text = ReadTextFile(messages_path);
  EXPECT_NE(created_text.find("Messages {"), std::string::npos);
  EXPECT_NE(created_text.find("Name = \"ManagedMessages\""), std::string::npos);
  EXPECT_NE(created_text.find("Description = \"Managed client messages\""),
            std::string::npos);
  EXPECT_NE(
      created_text.find("MailCommand = \"/usr/lib/bareos/client-mail %r\""),
      std::string::npos);
  EXPECT_NE(created_text.find("OperatorCommand = "
                              "\"/usr/lib/bareos/client-operator %r\""),
            std::string::npos);
  EXPECT_NE(created_text.find("TimestampFormat = \"%H:%M:%S\""),
            std::string::npos);
  EXPECT_NE(created_text.find("Director = bareos-dir = all, !skipped"),
            std::string::npos);
  EXPECT_NE(created_text.find("Stdout = all, !restored"), std::string::npos);

  const auto standard_path
      = created.value->path / "bareos-fd.d/messages/Standard.conf";
  auto standard_text = ReadTextFile(standard_path);
  auto standard_brace = standard_text.rfind("}\n");
  ASSERT_NE(standard_brace, std::string::npos);
  standard_text.insert(standard_brace,
                       "  mailcommand = \"/usr/lib/bareos/client-mail %r\"\n"
                       "  operatorcommand = "
                       "\"/usr/lib/bareos/client-operator %r\"\n"
                       "  timestampformat = \"%H:%M:%S\"\n"
                       "  stdout = all, !skipped\n");
  WriteTextFile(standard_path, standard_text);

  auto updated = state.UpsertClientMessagesResource(
      "prod", "backup-bareos-test-fd", "Standard",
      {.description = std::string{"Updated client messages"}});
  ASSERT_TRUE(updated) << updated.error;

  const auto updated_text = ReadTextFile(standard_path);
  EXPECT_NE(updated_text.find("Description = \"Updated client messages\""),
            std::string::npos);
  EXPECT_NE(
      updated_text.find("MailCommand = \"/usr/lib/bareos/client-mail %r\""),
      std::string::npos);
  EXPECT_NE(updated_text.find("OperatorCommand = "
                              "\"/usr/lib/bareos/client-operator %r\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("TimestampFormat = \"%H:%M:%S\""),
            std::string::npos);
  EXPECT_NE(
      updated_text.find("Director = bareos-dir = all, !skipped, !restored"),
      std::string::npos);
  EXPECT_NE(updated_text.find("Stdout = all, !skipped"), std::string::npos);

  auto current = state.GetClientMessagesResourceSpec(
      "prod", "backup-bareos-test-fd", "Standard");
  ASSERT_TRUE(current) << current.error;
  EXPECT_EQ(current.value->description, "Updated client messages");
  EXPECT_EQ(current.value->mail_command, "/usr/lib/bareos/client-mail %r");
  EXPECT_EQ(current.value->operator_command,
            "/usr/lib/bareos/client-operator %r");
  EXPECT_EQ(current.value->timestamp_format, "%H:%M:%S");
  ASSERT_TRUE(current.value->director_entries.has_value());
  EXPECT_EQ(
      *current.value->director_entries,
      (std::vector<std::string>{"bareos-dir = all, !skipped, !restored"}));
  ASSERT_TRUE(current.value->stdout_entries.has_value());
  EXPECT_EQ(*current.value->stdout_entries,
            (std::vector<std::string>{"all, !skipped"}));
}

TEST(BconfigService, UpsertsClientMessagesResourcesInSharedFiles)
{
  ScopedDirectory source_root{MakeTempPath()};
  ScopedDirectory repo_path{MakeTempPath()};
  ServiceState state;

  auto deployment
      = state.CreateDeployment({.id = "prod",
                                .name = "Production",
                                .repository_path = repo_path.path()});
  ASSERT_TRUE(deployment);

  const auto source_fixture_root = FindFixtureRoot();
  ASSERT_FALSE(source_fixture_root.empty());
  std::filesystem::create_directories(source_root.path());
  std::filesystem::copy(source_fixture_root / "bareos-fd.d",
                        source_root.path() / "bareos-fd.d",
                        std::filesystem::copy_options::recursive);

  auto import_job
      = state.CreateJob({.type = "import_configuration",
                         .deployment_id = std::string{"prod"},
                         .source_path = source_root.path().string()});
  ASSERT_TRUE(import_job);
  auto imported = WaitForJobTerminal(state, import_job.value->id);
  ASSERT_TRUE(imported.has_value());
  ASSERT_EQ(imported->status, JobStatus::kSucceeded);

  const auto messages_directory
      = RepositoryLayout::ClientsDirectory(repo_path.path())
        / "backup-bareos-test-fd/bareos-fd.d/messages";
  const auto original_path = messages_directory / "Standard.conf";
  const auto shared_path = messages_directory / "shared.conf";
  const auto original_text = ReadTextFile(original_path);
  WriteTextFile(shared_path,
                original_text
                    + "\nMessages {\n"
                      "  Name = \"OtherMessages\"\n"
                      "  Director = bareos-dir = all\n"
                      "}\n");
  std::filesystem::remove(original_path);

  auto updated = state.UpsertClientMessagesResource(
      "prod", "backup-bareos-test-fd", "Standard",
      {.description = std::string{"Updated client messages"}});
  ASSERT_TRUE(updated) << updated.error;
  EXPECT_FALSE(std::filesystem::exists(original_path));
  EXPECT_TRUE(std::filesystem::exists(shared_path));
  const auto shared_text = ReadTextFile(shared_path);
  EXPECT_NE(shared_text.find("Description = \"Updated client messages\""),
            std::string::npos);
  EXPECT_NE(
      shared_text.find("Director = bareos-dir = all, !skipped, !restored"),
      std::string::npos);
  EXPECT_NE(shared_text.find("Name = \"OtherMessages\""), std::string::npos);
}

TEST(BconfigService, DeletesClientMessagesResources)
{
  ScopedDirectory source_root{MakeTempPath()};
  ScopedDirectory repo_path{MakeTempPath()};
  ServiceState state;

  auto deployment
      = state.CreateDeployment({.id = "prod",
                                .name = "Production",
                                .repository_path = repo_path.path()});
  ASSERT_TRUE(deployment);

  const auto source_fixture_root = FindFixtureRoot();
  ASSERT_FALSE(source_fixture_root.empty());
  std::filesystem::create_directories(source_root.path());
  std::filesystem::copy(source_fixture_root / "bareos-fd.d",
                        source_root.path() / "bareos-fd.d",
                        std::filesystem::copy_options::recursive);

  auto import_job
      = state.CreateJob({.type = "import_configuration",
                         .deployment_id = std::string{"prod"},
                         .source_path = source_root.path().string()});
  ASSERT_TRUE(import_job);
  auto imported = WaitForJobTerminal(state, import_job.value->id);
  ASSERT_TRUE(imported.has_value());
  ASSERT_EQ(imported->status, JobStatus::kSucceeded);

  auto created = state.UpsertClientMessagesResource(
      "prod", "backup-bareos-test-fd", "ManagedMessages",
      {.entries = std::vector<std::string>{"  Director = bareos-dir = all"}});
  ASSERT_TRUE(created) << created.error;

  const auto messages_path
      = created.value->path / "bareos-fd.d/messages/ManagedMessages.conf";
  ASSERT_TRUE(std::filesystem::exists(messages_path));

  auto deleted = state.DeleteClientMessagesResource(
      "prod", "backup-bareos-test-fd", "ManagedMessages");
  ASSERT_TRUE(deleted) << deleted.error;
  EXPECT_FALSE(std::filesystem::exists(messages_path));
}

TEST(BconfigService, DeletesClientMessagesResourcesFromSharedFiles)
{
  ScopedDirectory source_root{MakeTempPath()};
  ScopedDirectory repo_path{MakeTempPath()};
  ServiceState state;

  auto deployment
      = state.CreateDeployment({.id = "prod",
                                .name = "Production",
                                .repository_path = repo_path.path()});
  ASSERT_TRUE(deployment);

  const auto source_fixture_root = FindFixtureRoot();
  ASSERT_FALSE(source_fixture_root.empty());
  std::filesystem::create_directories(source_root.path());
  std::filesystem::copy(source_fixture_root / "bareos-fd.d",
                        source_root.path() / "bareos-fd.d",
                        std::filesystem::copy_options::recursive);

  auto import_job
      = state.CreateJob({.type = "import_configuration",
                         .deployment_id = std::string{"prod"},
                         .source_path = source_root.path().string()});
  ASSERT_TRUE(import_job);
  auto imported = WaitForJobTerminal(state, import_job.value->id);
  ASSERT_TRUE(imported.has_value());
  ASSERT_EQ(imported->status, JobStatus::kSucceeded);

  const auto messages_directory
      = RepositoryLayout::ClientsDirectory(repo_path.path())
        / "backup-bareos-test-fd/bareos-fd.d/messages";
  const auto original_path = messages_directory / "Standard.conf";
  const auto shared_path = messages_directory / "shared.conf";
  const auto original_text = ReadTextFile(original_path);
  WriteTextFile(shared_path,
                original_text
                    + "\nMessages {\n"
                      "  Name = \"OtherMessages\"\n"
                      "  Director = bareos-dir = all\n"
                      "}\n");
  std::filesystem::remove(original_path);

  auto deleted = state.DeleteClientMessagesResource(
      "prod", "backup-bareos-test-fd", "OtherMessages");
  ASSERT_TRUE(deleted) << deleted.error;
  EXPECT_FALSE(std::filesystem::exists(original_path));
  EXPECT_TRUE(std::filesystem::exists(shared_path));
  const auto shared_text = ReadTextFile(shared_path);
  EXPECT_NE(
      shared_text.find("Director = bareos-dir = all, !skipped, !restored"),
      std::string::npos);
  EXPECT_EQ(shared_text.find("Name = \"OtherMessages\""), std::string::npos);
}

TEST(BconfigService, UpsertsClientDaemonResources)
{
  ScopedDirectory source_root{MakeTempPath()};
  ScopedDirectory repo_path{MakeTempPath()};
  ServiceState state;

  auto deployment
      = state.CreateDeployment({.id = "prod",
                                .name = "Production",
                                .repository_path = repo_path.path()});
  ASSERT_TRUE(deployment);

  const auto source_fixture_root = FindFixtureRoot();
  ASSERT_FALSE(source_fixture_root.empty());
  std::filesystem::create_directories(source_root.path());
  std::filesystem::copy(source_fixture_root / "bareos-fd.d",
                        source_root.path() / "bareos-fd.d",
                        std::filesystem::copy_options::recursive);

  auto import_job
      = state.CreateJob({.type = "import_configuration",
                         .deployment_id = std::string{"prod"},
                         .source_path = source_root.path().string()});
  ASSERT_TRUE(import_job);
  auto imported = WaitForJobTerminal(state, import_job.value->id);
  ASSERT_TRUE(imported.has_value());
  ASSERT_EQ(imported->status, JobStatus::kSucceeded);

  auto updated = state.UpsertClientDaemonResource(
      "prod", "backup-bareos-test-fd",
      {.addresses = std::vector<std::string>{"host[ipv4;192.0.2.10;42102]",
                                             "host[ipv6;::1;42102]"},
       .source_address = std::string{"192.0.2.10"},
       .maximum_concurrent_jobs = 42,
       .maximum_workers_per_job = 3,
       .absolute_job_timeout = 900,
       .allow_bandwidth_bursting = true,
       .tls_authenticate = false,
       .tls_enable = true,
       .tls_require = false,
       .tls_verify_peer = true,
       .tls_cipher_list = std::string{"ECDHE-RSA-AES256-GCM-SHA384"},
       .tls_cipher_suites = std::string{"TLS_AES_256_GCM_SHA384"},
       .tls_dh_file = std::string{"/etc/bareos/dh4096.pem"},
       .tls_protocol = std::string{"MinProtocol = TLSv1.2"},
       .tls_ca_certificate_file = std::string{"/etc/bareos/ca.crt"},
       .tls_ca_certificate_dir = std::string{"/etc/ssl/certs"},
       .tls_certificate_revocation_list = std::string{"/etc/bareos/crl.pem"},
       .tls_certificate = std::string{"/etc/bareos/client.crt"},
       .tls_key = std::string{"/etc/bareos/client.key"},
       .tls_allowed_cn = std::vector<std::string>{"client-cn-1", "client-cn-2"},
       .pki_signatures = true,
       .pki_encryption = false,
       .pki_key_pair = std::string{"/etc/bareos/client.pem"},
       .pki_signers = std::vector<std::string>{"/etc/bareos/signer1.pem",
                                               "/etc/bareos/signer2.pem"},
       .pki_master_keys
       = std::vector<std::string>{"/etc/bareos/recipient1.pem",
                                  "/etc/bareos/recipient2.pem"},
       .pki_cipher = std::string{"aes256"},
       .always_use_lmdb = false,
       .lmdb_threshold = 17,
       .ver_id = std::string{"client-ver"},
       .log_timestamp_format = std::string{"%Y-%m-%d %H:%M:%S"},
       .maximum_bandwidth_per_job = 2048,
       .secure_erase_command = std::string{"/usr/bin/shred -n 3 -u"},
       .grpc_module = std::string{"grpc-fd-module"},
       .enable_ktls = true,
       .sd_connect_timeout = 1800,
       .heartbeat_interval = 60,
       .maximum_network_buffer_size = 1048576,
       .description = std::string{"Managed client daemon"},
       .working_directory = std::string{"/var/lib/bareos/client"},
       .plugin_directory = std::string{"/usr/lib/bareos/plugins"},
       .plugin_names = std::vector<std::string>{"python", "grpc"},
       .allowed_script_dirs
       = std::vector<std::string>{"/usr/lib/bareos/scripts",
                                  "/opt/bareos/scripts"},
       .allowed_job_commands
       = std::vector<std::string>{"RunBeforeJob", "Estimate listing"},
       .scripts_directory = std::string{"/usr/lib/bareos/scripts"},
       .messages = std::string{"Standard"}});
  ASSERT_TRUE(updated) << updated.error;
  EXPECT_EQ(updated.value->name, "backup-bareos-test-fd");

  const auto client_path
      = updated.value->path / "bareos-fd.d/client/myself.conf";
  const auto updated_text = ReadTextFile(client_path);
  EXPECT_NE(updated_text.find("Client {"), std::string::npos);
  EXPECT_NE(updated_text.find("Name = \"backup-bareos-test-fd\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("Addresses = {"), std::string::npos);
  EXPECT_NE(updated_text.find("ipv4 = { addr = 192.0.2.10 ; port = 42102 }"),
            std::string::npos);
  EXPECT_NE(updated_text.find("ipv6 = { addr = ::1 ; port = 42102 }"),
            std::string::npos);
  EXPECT_NE(updated_text.find("SourceAddress = 192.0.2.10"), std::string::npos);
  EXPECT_NE(updated_text.find("MaximumConcurrentJobs = 42"), std::string::npos);
  EXPECT_NE(updated_text.find("MaximumWorkersPerJob = 3"), std::string::npos);
  EXPECT_NE(updated_text.find("AbsoluteJobTimeout = 900"), std::string::npos);
  EXPECT_NE(updated_text.find("AllowBandwidthBursting = yes"),
            std::string::npos);
  EXPECT_NE(updated_text.find("TlsAuthenticate = no"), std::string::npos);
  EXPECT_NE(updated_text.find("TlsEnable = yes"), std::string::npos);
  EXPECT_NE(updated_text.find("TlsRequire = no"), std::string::npos);
  EXPECT_NE(updated_text.find("TlsVerifyPeer = yes"), std::string::npos);
  EXPECT_NE(
      updated_text.find("TlsCipherList = \"ECDHE-RSA-AES256-GCM-SHA384\""),
      std::string::npos);
  EXPECT_NE(updated_text.find("TlsCipherSuites = \"TLS_AES_256_GCM_SHA384\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("TlsDhFile = \"/etc/bareos/dh4096.pem\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("TlsProtocol = \"MinProtocol = TLSv1.2\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("TlsCaCertificateFile = \"/etc/bareos/ca.crt\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("TlsCaCertificateDir = \"/etc/ssl/certs\""),
            std::string::npos);
  EXPECT_NE(updated_text.find(
                "TlsCertificateRevocationList = \"/etc/bareos/crl.pem\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("TlsCertificate = \"/etc/bareos/client.crt\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("TlsKey = \"/etc/bareos/client.key\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("TlsAllowedCn = \"client-cn-1\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("TlsAllowedCn = \"client-cn-2\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("PkiSignatures = yes"), std::string::npos);
  EXPECT_NE(updated_text.find("PkiEncryption = no"), std::string::npos);
  EXPECT_NE(updated_text.find("PkiKeyPair = \"/etc/bareos/client.pem\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("PkiSigner = \"/etc/bareos/signer1.pem\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("PkiSigner = \"/etc/bareos/signer2.pem\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("PkiMasterKey = \"/etc/bareos/recipient1.pem\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("PkiMasterKey = \"/etc/bareos/recipient2.pem\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("PkiCipher = aes256"), std::string::npos);
  EXPECT_NE(updated_text.find("AlwaysUseLmdb = no"), std::string::npos);
  EXPECT_NE(updated_text.find("LmdbThreshold = 17"), std::string::npos);
  EXPECT_NE(updated_text.find("VerId = \"client-ver\""), std::string::npos);
  EXPECT_NE(updated_text.find("LogTimestampFormat = \"%Y-%m-%d %H:%M:%S\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("MaximumBandwidthPerJob = 2048"),
            std::string::npos);
  EXPECT_NE(
      updated_text.find("SecureEraseCommand = \"/usr/bin/shred -n 3 -u\""),
      std::string::npos);
  EXPECT_NE(updated_text.find("GrpcModule = \"grpc-fd-module\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("EnableKtls = yes"), std::string::npos);
  EXPECT_NE(updated_text.find("SdConnectTimeout = 1800"), std::string::npos);
  EXPECT_NE(updated_text.find("HeartbeatInterval = 60"), std::string::npos);
  EXPECT_NE(updated_text.find("MaximumNetworkBufferSize = 1048576"),
            std::string::npos);
  EXPECT_NE(updated_text.find("Description = \"Managed client daemon\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("WorkingDirectory = \"/var/lib/bareos/client\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("PluginDirectory = \"/usr/lib/bareos/plugins\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("PluginNames = python"), std::string::npos);
  EXPECT_NE(updated_text.find("PluginNames = grpc"), std::string::npos);
  EXPECT_NE(updated_text.find("AllowedScriptDir = \"/usr/lib/bareos/scripts\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("AllowedScriptDir = \"/opt/bareos/scripts\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("AllowedJobCommand = \"RunBeforeJob\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("AllowedJobCommand = \"Estimate listing\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("ScriptsDirectory = \"/usr/lib/bareos/scripts\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("Messages = Standard"), std::string::npos);

  auto current
      = state.GetClientDaemonResourceSpec("prod", "backup-bareos-test-fd");
  ASSERT_TRUE(current) << current.error;
  ASSERT_TRUE(current.value->addresses.has_value());
  EXPECT_EQ(*current.value->addresses,
            (std::vector<std::string>{"host[ipv4;192.0.2.10;42102]",
                                      "host[ipv6;::1;42102]"}));
  EXPECT_EQ(current.value->source_address, "192.0.2.10");
  EXPECT_EQ(current.value->tls_enable, true);
  ASSERT_TRUE(current.value->tls_allowed_cn.has_value());
  EXPECT_EQ(*current.value->tls_allowed_cn,
            (std::vector<std::string>{"client-cn-1", "client-cn-2"}));
  ASSERT_TRUE(current.value->pki_signers.has_value());
  EXPECT_EQ(*current.value->pki_signers,
            (std::vector<std::string>{"/etc/bareos/signer1.pem",
                                      "/etc/bareos/signer2.pem"}));
  ASSERT_TRUE(current.value->plugin_names.has_value());
  EXPECT_EQ(*current.value->plugin_names,
            (std::vector<std::string>{"python", "grpc"}));
  ASSERT_TRUE(current.value->allowed_script_dirs.has_value());
  EXPECT_EQ(*current.value->allowed_script_dirs,
            (std::vector<std::string>{"/usr/lib/bareos/scripts",
                                      "/opt/bareos/scripts"}));
  EXPECT_EQ(current.value->messages, "Standard");
}

TEST(BconfigService, UpsertsClientDaemonResourcesInSharedFiles)
{
  ScopedDirectory source_root{MakeTempPath()};
  ScopedDirectory repo_path{MakeTempPath()};
  ServiceState state;

  auto deployment
      = state.CreateDeployment({.id = "prod",
                                .name = "Production",
                                .repository_path = repo_path.path()});
  ASSERT_TRUE(deployment);

  const auto source_fixture_root = FindFixtureRoot();
  ASSERT_FALSE(source_fixture_root.empty());
  std::filesystem::create_directories(source_root.path());
  std::filesystem::copy(source_fixture_root / "bareos-fd.d",
                        source_root.path() / "bareos-fd.d",
                        std::filesystem::copy_options::recursive);

  auto import_job
      = state.CreateJob({.type = "import_configuration",
                         .deployment_id = std::string{"prod"},
                         .source_path = source_root.path().string()});
  ASSERT_TRUE(import_job);
  auto imported = WaitForJobTerminal(state, import_job.value->id);
  ASSERT_TRUE(imported.has_value());
  ASSERT_EQ(imported->status, JobStatus::kSucceeded);

  const auto client_directory
      = RepositoryLayout::ClientsDirectory(repo_path.path())
        / "backup-bareos-test-fd/bareos-fd.d/client";
  const auto original_path = client_directory / "myself.conf";
  const auto shared_path = client_directory / "shared.conf";
  const auto original_text = ReadTextFile(original_path);
  WriteTextFile(shared_path,
                original_text
                    + "\nClient {\n"
                      "  Name = \"OtherClient\"\n"
                      "}\n");
  std::filesystem::remove(original_path);

  auto updated = state.UpsertClientDaemonResource(
      "prod", "backup-bareos-test-fd",
      {.description = std::string{"Managed client daemon"}});
  ASSERT_TRUE(updated) << updated.error;
  EXPECT_FALSE(std::filesystem::exists(original_path));
  EXPECT_TRUE(std::filesystem::exists(shared_path));
  const auto shared_text = ReadTextFile(shared_path);
  EXPECT_NE(shared_text.find("Description = \"Managed client daemon\""),
            std::string::npos);
  EXPECT_NE(shared_text.find("Name = \"OtherClient\""), std::string::npos);
}

TEST(BconfigService, UpsertsClientDaemonResourcesWithMultipleSourceAddresses)
{
  ScopedDirectory source_root{MakeTempPath()};
  ScopedDirectory repo_path{MakeTempPath()};
  ServiceState state;

  auto deployment
      = state.CreateDeployment({.id = "prod",
                                .name = "Production",
                                .repository_path = repo_path.path()});
  ASSERT_TRUE(deployment);

  const auto source_fixture_root = FindFixtureRoot();
  ASSERT_FALSE(source_fixture_root.empty());
  std::filesystem::create_directories(source_root.path());
  std::filesystem::copy(source_fixture_root / "bareos-fd.d",
                        source_root.path() / "bareos-fd.d",
                        std::filesystem::copy_options::recursive);

  auto import_job
      = state.CreateJob({.type = "import_configuration",
                         .deployment_id = std::string{"prod"},
                         .source_path = source_root.path().string()});
  ASSERT_TRUE(import_job);
  auto imported = WaitForJobTerminal(state, import_job.value->id);
  ASSERT_TRUE(imported.has_value());
  ASSERT_EQ(imported->status, JobStatus::kSucceeded);

  const auto client_path
      = RepositoryLayout::ClientsDirectory(repo_path.path())
        / "backup-bareos-test-fd/bareos-fd.d/client/myself.conf";
  const auto original_text = ReadTextFile(client_path);
  const auto closing = original_text.rfind("}\n");
  ASSERT_NE(closing, std::string::npos);
  WriteTextFile(client_path,
                original_text.substr(0, closing)
                    + "  SourceAddress = 192.0.2.10\n"
                      "  SourceAddress = 198.51.100.10\n"
                      "}\n");

  auto updated = state.UpsertClientDaemonResource(
      "prod", "backup-bareos-test-fd",
      {.description = std::string{"Updated client daemon"}});
  ASSERT_TRUE(updated) << updated.error;

  const auto updated_text = ReadTextFile(client_path);
  EXPECT_NE(updated_text.find("Description = \"Updated client daemon\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("SourceAddress = 192.0.2.10"), std::string::npos);
  EXPECT_NE(updated_text.find("SourceAddress = 198.51.100.10"),
            std::string::npos);
}

TEST(BconfigService, UpsertsClientDaemonResourcesPreserveUnknownPkiCipher)
{
  ScopedDirectory source_root{MakeTempPath()};
  ScopedDirectory repo_path{MakeTempPath()};
  ServiceState state;

  auto deployment
      = state.CreateDeployment({.id = "prod",
                                .name = "Production",
                                .repository_path = repo_path.path()});
  ASSERT_TRUE(deployment);

  const auto source_fixture_root = FindFixtureRoot();
  ASSERT_FALSE(source_fixture_root.empty());
  std::filesystem::create_directories(source_root.path());
  std::filesystem::copy(source_fixture_root / "bareos-fd.d",
                        source_root.path() / "bareos-fd.d",
                        std::filesystem::copy_options::recursive);

  auto import_job
      = state.CreateJob({.type = "import_configuration",
                         .deployment_id = std::string{"prod"},
                         .source_path = source_root.path().string()});
  ASSERT_TRUE(import_job);
  auto imported = WaitForJobTerminal(state, import_job.value->id);
  ASSERT_TRUE(imported.has_value());
  ASSERT_EQ(imported->status, JobStatus::kSucceeded);

  const auto client_path
      = RepositoryLayout::ClientsDirectory(repo_path.path())
        / "backup-bareos-test-fd/bareos-fd.d/client/myself.conf";
  const auto original_text = ReadTextFile(client_path);
  const auto closing = original_text.rfind("}\n");
  ASSERT_NE(closing, std::string::npos);
  WriteTextFile(client_path,
                original_text.substr(0, closing)
                    + "  PkiCipher = legacycipher\n"
                      "}\n");

  auto updated = state.UpsertClientDaemonResource(
      "prod", "backup-bareos-test-fd",
      {.description = std::string{"Updated client daemon"}});
  ASSERT_TRUE(updated) << updated.error;

  const auto updated_text = ReadTextFile(client_path);
  EXPECT_NE(updated_text.find("Description = \"Updated client daemon\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("PkiCipher = legacycipher"), std::string::npos);
}

TEST(BconfigService, UpsertsDirectorDaemonResources)
{
  ScopedDirectory source_root{MakeTempPath()};
  ScopedDirectory repo_path{MakeTempPath()};
  ServiceState state;

  auto deployment
      = state.CreateDeployment({.id = "prod",
                                .name = "Production",
                                .repository_path = repo_path.path()});
  ASSERT_TRUE(deployment);

  const auto source_fixture_root = FindFixtureRoot();
  ASSERT_FALSE(source_fixture_root.empty());
  std::filesystem::create_directories(source_root.path());
  std::filesystem::copy(source_fixture_root / "bareos-dir.d",
                        source_root.path() / "bareos-dir.d",
                        std::filesystem::copy_options::recursive);

  auto import_job
      = state.CreateJob({.type = "import_configuration",
                         .deployment_id = std::string{"prod"},
                         .source_path = source_root.path().string()});
  ASSERT_TRUE(import_job);
  auto imported = WaitForJobTerminal(state, import_job.value->id);
  ASSERT_TRUE(imported.has_value());
  ASSERT_EQ(imported->status, JobStatus::kSucceeded);

  auto updated = state.UpsertDirectorDaemonResource(
      "prod", "bareos-dir",
      {.address = std::string{"192.0.2.44"},
       .source_addresses
       = std::vector<std::string>{"192.0.2.54", "198.51.100.54"},
       .port = 19101,
       .query_file = std::string{"/etc/bareos/query.sql"},
       .subscriptions = 7,
       .maximum_concurrent_jobs = 23,
       .maximum_console_connections = 31,
       .password = std::string{"updated-director-password"},
       .absolute_job_timeout = 3600,
       .tls_authenticate = false,
       .tls_enable = true,
       .tls_require = false,
       .tls_verify_peer = true,
       .tls_cipher_list = std::string{"ECDHE-RSA-AES256-GCM-SHA384"},
       .tls_cipher_suites = std::string{"TLS_AES_256_GCM_SHA384"},
       .tls_dh_file = std::string{"/etc/bareos/dh4096.pem"},
       .tls_protocol = std::string{"MinProtocol = TLSv1.2"},
       .tls_ca_certificate_file = std::string{"/etc/bareos/ca.crt"},
       .tls_ca_certificate_dir = std::string{"/etc/ssl/certs"},
       .tls_certificate_revocation_list = std::string{"/etc/bareos/crl.pem"},
       .tls_certificate = std::string{"/etc/bareos/director.crt"},
       .tls_key = std::string{"/etc/bareos/director.key"},
       .tls_allowed_cn
       = std::vector<std::string>{"director-cn-1", "director-cn-2"},
       .ver_id = std::string{"director-ver"},
       .log_timestamp_format = std::string{"%Y-%m-%d %H:%M:%S"},
       .secure_erase_command = std::string{"/usr/bin/shred -n 3 -u"},
       .enable_ktls = true,
       .fd_connect_timeout = 300,
       .sd_connect_timeout = 1800,
       .heartbeat_interval = 60,
       .statistics_retention = 86400,
       .statistics_collect_interval = 42,
       .description = std::string{"Managed director daemon"},
       .key_encryption_key = std::string{"director-kek"},
       .ndmp_snooping = true,
       .ndmp_log_level = 6,
       .ndmp_namelist_fhinfo_set_zero_for_invalid_uquad = true,
       .auditing = true,
       .audit_events = std::vector<std::string>{"item01", "item02"},
       .working_directory = std::string{"/var/lib/bareos/director"},
       .plugin_directory = std::string{"/usr/lib/bareos/plugins"},
       .plugin_names = std::vector<std::string>{"python"},
       .scripts_directory = std::string{"/usr/lib/bareos/scripts"},
       .messages = std::string{"Standard"}});
  ASSERT_TRUE(updated) << updated.error;

  const auto director_path
      = updated.value->path / "bareos-dir.d/director/bareos-dir.conf";
  const auto updated_text = ReadTextFile(director_path);
  EXPECT_NE(updated_text.find("Director {"), std::string::npos);
  EXPECT_NE(updated_text.find("Name = \"bareos-dir\""), std::string::npos);
  EXPECT_NE(updated_text.find("Password = "), std::string::npos);
  EXPECT_NE(updated_text.find("Address = 192.0.2.44"), std::string::npos);
  EXPECT_NE(updated_text.find("SourceAddress = 192.0.2.54"), std::string::npos);
  EXPECT_NE(updated_text.find("SourceAddress = 198.51.100.54"),
            std::string::npos);
  EXPECT_NE(updated_text.find("Port = 19101"), std::string::npos);
  EXPECT_NE(updated_text.find("QueryFile = \"/etc/bareos/query.sql\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("Subscriptions = 7"), std::string::npos);
  EXPECT_NE(updated_text.find("MaximumConcurrentJobs = 23"), std::string::npos);
  EXPECT_NE(updated_text.find("MaximumConsoleConnections = 31"),
            std::string::npos);
  EXPECT_NE(updated_text.find("Password = \"updated-director-password\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("AbsoluteJobTimeout = 3600"), std::string::npos);
  EXPECT_NE(updated_text.find("TlsAuthenticate = no"), std::string::npos);
  EXPECT_NE(updated_text.find("TlsEnable = yes"), std::string::npos);
  EXPECT_NE(updated_text.find("TlsRequire = no"), std::string::npos);
  EXPECT_NE(updated_text.find("TlsVerifyPeer = yes"), std::string::npos);
  EXPECT_NE(
      updated_text.find("TlsCipherList = \"ECDHE-RSA-AES256-GCM-SHA384\""),
      std::string::npos);
  EXPECT_NE(updated_text.find("TlsCipherSuites = \"TLS_AES_256_GCM_SHA384\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("TlsDhFile = \"/etc/bareos/dh4096.pem\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("TlsProtocol = \"MinProtocol = TLSv1.2\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("TlsCaCertificateFile = \"/etc/bareos/ca.crt\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("TlsCaCertificateDir = \"/etc/ssl/certs\""),
            std::string::npos);
  EXPECT_NE(updated_text.find(
                "TlsCertificateRevocationList = \"/etc/bareos/crl.pem\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("TlsCertificate = \"/etc/bareos/director.crt\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("TlsKey = \"/etc/bareos/director.key\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("TlsAllowedCn = \"director-cn-1\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("TlsAllowedCn = \"director-cn-2\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("VerId = \"director-ver\""), std::string::npos);
  EXPECT_NE(updated_text.find("LogTimestampFormat = \"%Y-%m-%d %H:%M:%S\""),
            std::string::npos);
  EXPECT_NE(
      updated_text.find("SecureEraseCommand = \"/usr/bin/shred -n 3 -u\""),
      std::string::npos);
  EXPECT_NE(updated_text.find("EnableKtls = yes"), std::string::npos);
  EXPECT_NE(updated_text.find("FdConnectTimeout = 300"), std::string::npos);
  EXPECT_NE(updated_text.find("SdConnectTimeout = 1800"), std::string::npos);
  EXPECT_NE(updated_text.find("HeartbeatInterval = 60"), std::string::npos);
  EXPECT_NE(updated_text.find("StatisticsRetention = 86400"),
            std::string::npos);
  EXPECT_NE(updated_text.find("StatisticsCollectInterval = 42"),
            std::string::npos);
  EXPECT_NE(updated_text.find("Description = \"Managed director daemon\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("KeyEncryptionKey = \"director-kek\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("NdmpSnooping = yes"), std::string::npos);
  EXPECT_NE(updated_text.find("NdmpLogLevel = 6"), std::string::npos);
  EXPECT_NE(updated_text.find("NdmpNamelistFhinfoSetZeroForInvalidUquad = yes"),
            std::string::npos);
  EXPECT_NE(updated_text.find("Auditing = yes"), std::string::npos);
  EXPECT_NE(updated_text.find("AuditEvents = item01"), std::string::npos);
  EXPECT_NE(updated_text.find("AuditEvents = item02"), std::string::npos);
  EXPECT_NE(
      updated_text.find("WorkingDirectory = \"/var/lib/bareos/director\""),
      std::string::npos);
  EXPECT_NE(updated_text.find("PluginDirectory = \"/usr/lib/bareos/plugins\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("PluginNames = python"), std::string::npos);
  EXPECT_NE(updated_text.find("ScriptsDirectory = \"/usr/lib/bareos/scripts\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("Messages = Standard"), std::string::npos);

  auto current = state.GetDirectorDaemonResourceSpec("prod", "bareos-dir");
  ASSERT_TRUE(current) << current.error;
  EXPECT_EQ(current.value->address, "192.0.2.44");
  ASSERT_TRUE(current.value->source_addresses.has_value());
  EXPECT_EQ(*current.value->source_addresses,
            (std::vector<std::string>{"192.0.2.54", "198.51.100.54"}));
  ASSERT_TRUE(current.value->password.has_value());
  EXPECT_FALSE(current.value->password->empty());
  EXPECT_EQ(current.value->tls_protocol, "MinProtocol = TLSv1.2");
  ASSERT_TRUE(current.value->tls_allowed_cn.has_value());
  EXPECT_EQ(*current.value->tls_allowed_cn,
            (std::vector<std::string>{"director-cn-1", "director-cn-2"}));
  ASSERT_TRUE(current.value->audit_events.has_value());
  EXPECT_EQ(*current.value->audit_events,
            (std::vector<std::string>{"item01", "item02"}));
  ASSERT_TRUE(current.value->plugin_names.has_value());
  EXPECT_EQ(*current.value->plugin_names, (std::vector<std::string>{"python"}));
  EXPECT_EQ(current.value->messages, "Standard");
}

TEST(BconfigService, UpsertsDirectorDaemonResourcesInSharedFiles)
{
  ScopedDirectory source_root{MakeTempPath()};
  ScopedDirectory repo_path{MakeTempPath()};
  ServiceState state;

  auto deployment
      = state.CreateDeployment({.id = "prod",
                                .name = "Production",
                                .repository_path = repo_path.path()});
  ASSERT_TRUE(deployment);

  const auto source_fixture_root = FindFixtureRoot();
  ASSERT_FALSE(source_fixture_root.empty());
  std::filesystem::create_directories(source_root.path());
  std::filesystem::copy(source_fixture_root / "bareos-dir.d",
                        source_root.path() / "bareos-dir.d",
                        std::filesystem::copy_options::recursive);

  auto import_job
      = state.CreateJob({.type = "import_configuration",
                         .deployment_id = std::string{"prod"},
                         .source_path = source_root.path().string()});
  ASSERT_TRUE(import_job);
  auto imported = WaitForJobTerminal(state, import_job.value->id);
  ASSERT_TRUE(imported.has_value());
  ASSERT_EQ(imported->status, JobStatus::kSucceeded);

  const auto director_directory
      = RepositoryLayout::DirectorsDirectory(repo_path.path())
        / "bareos-dir/bareos-dir.d/director";
  const auto original_path = director_directory / "bareos-dir.conf";
  const auto shared_path = director_directory / "shared.conf";
  const auto original_text = ReadTextFile(original_path);
  WriteTextFile(shared_path,
                original_text
                    + "\nDirector {\n"
                      "  Name = \"OtherDirector\"\n"
                      "  Password = \"other-password\"\n"
                      "}\n");
  std::filesystem::remove(original_path);

  auto updated = state.UpsertDirectorDaemonResource(
      "prod", "bareos-dir", {.description = std::string{"Managed director"}});
  ASSERT_TRUE(updated) << updated.error;
  EXPECT_FALSE(std::filesystem::exists(original_path));
  EXPECT_TRUE(std::filesystem::exists(shared_path));
  const auto shared_text = ReadTextFile(shared_path);
  EXPECT_NE(shared_text.find("Description = \"Managed director\""),
            std::string::npos);
  EXPECT_NE(shared_text.find("Name = \"OtherDirector\""), std::string::npos);
}

TEST(BconfigService, UpsertsDirectorDaemonResourcesWithMultipleSourceAddresses)
{
  ScopedDirectory source_root{MakeTempPath()};
  ScopedDirectory repo_path{MakeTempPath()};
  ServiceState state;

  auto deployment
      = state.CreateDeployment({.id = "prod",
                                .name = "Production",
                                .repository_path = repo_path.path()});
  ASSERT_TRUE(deployment);

  const auto source_fixture_root = FindFixtureRoot();
  ASSERT_FALSE(source_fixture_root.empty());
  std::filesystem::create_directories(source_root.path());
  std::filesystem::copy(source_fixture_root / "bareos-dir.d",
                        source_root.path() / "bareos-dir.d",
                        std::filesystem::copy_options::recursive);

  auto import_job
      = state.CreateJob({.type = "import_configuration",
                         .deployment_id = std::string{"prod"},
                         .source_path = source_root.path().string()});
  ASSERT_TRUE(import_job);
  auto imported = WaitForJobTerminal(state, import_job.value->id);
  ASSERT_TRUE(imported.has_value());
  ASSERT_EQ(imported->status, JobStatus::kSucceeded);

  const auto director_path
      = RepositoryLayout::DirectorsDirectory(repo_path.path())
        / "bareos-dir/bareos-dir.d/director/bareos-dir.conf";
  const auto original_text = ReadTextFile(director_path);
  const auto closing = original_text.rfind("}\n");
  ASSERT_NE(closing, std::string::npos);
  WriteTextFile(director_path,
                original_text.substr(0, closing)
                    + "  SourceAddress = 192.0.2.44\n"
                      "  SourceAddress = 198.51.100.44\n"
                      "}\n");

  auto updated = state.UpsertDirectorDaemonResource(
      "prod", "bareos-dir",
      {.description = std::string{"Updated director daemon"}});
  ASSERT_TRUE(updated) << updated.error;

  const auto updated_text = ReadTextFile(director_path);
  EXPECT_NE(updated_text.find("Description = \"Updated director daemon\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("SourceAddress = 192.0.2.44"), std::string::npos);
  EXPECT_NE(updated_text.find("SourceAddress = 198.51.100.44"),
            std::string::npos);
}

TEST(BconfigService, GeneratesClientStubsForDirectorOnlyImports)
{
  ScopedDirectory source_root{MakeTempPath()};
  ScopedDirectory repo_path{MakeTempPath()};
  ServiceState state;

  auto deployment
      = state.CreateDeployment({.id = "prod",
                                .name = "Production",
                                .repository_path = repo_path.path()});
  ASSERT_TRUE(deployment);

  const auto source_fixture_root = FindFixtureRoot();
  ASSERT_FALSE(source_fixture_root.empty());
  std::filesystem::create_directories(source_root.path());
  std::filesystem::copy(source_fixture_root / "bareos-dir.d",
                        source_root.path() / "bareos-dir.d",
                        std::filesystem::copy_options::recursive);

  auto import_job
      = state.CreateJob({.type = "import_configuration",
                         .deployment_id = std::string{"prod"},
                         .source_path = source_root.path().string()});
  ASSERT_TRUE(import_job);
  auto imported = WaitForJobTerminal(state, import_job.value->id);
  ASSERT_TRUE(imported.has_value());
  ASSERT_EQ(imported->status, JobStatus::kSucceeded);

  const auto client_root
      = RepositoryLayout::ClientsDirectory(repo_path.path()) / "bareos-fd";
  const auto stub_path = client_root / "bareos-fd.d/director/bareos-dir.conf";
  EXPECT_TRUE(std::filesystem::exists(stub_path));

  std::ifstream stub_file{stub_path};
  ASSERT_TRUE(stub_file.good());
  const std::string stub_text((std::istreambuf_iterator<char>(stub_file)),
                              std::istreambuf_iterator<char>());
  EXPECT_NE(stub_text.find("Name = \"bareos-dir\""), std::string::npos);
  EXPECT_NE(stub_text.find("Password = \"[md5]"), std::string::npos);

  auto validate_job = state.CreateJob({.type = "validate_deployment_repo",
                                       .deployment_id = std::string{"prod"}});
  ASSERT_TRUE(validate_job);
  auto validated = WaitForJobTerminal(state, validate_job.value->id);
  ASSERT_TRUE(validated.has_value());
  EXPECT_EQ(validated->status, JobStatus::kSucceeded);
  EXPECT_NE(std::find(validated->logs.begin(), validated->logs.end(),
                      "validated client 'bareos-fd'"),
            validated->logs.end());
}

TEST(BconfigService, UpsertsClientDirectorStubs)
{
  ScopedDirectory source_root{MakeTempPath()};
  ScopedDirectory repo_path{MakeTempPath()};
  ServiceState state;

  auto deployment
      = state.CreateDeployment({.id = "prod",
                                .name = "Production",
                                .repository_path = repo_path.path()});
  ASSERT_TRUE(deployment);

  const auto source_fixture_root = FindFixtureRoot();
  ASSERT_FALSE(source_fixture_root.empty());
  std::filesystem::create_directories(source_root.path());
  std::filesystem::copy(source_fixture_root / "bareos-dir.d",
                        source_root.path() / "bareos-dir.d",
                        std::filesystem::copy_options::recursive);

  auto import_job
      = state.CreateJob({.type = "import_configuration",
                         .deployment_id = std::string{"prod"},
                         .source_path = source_root.path().string()});
  ASSERT_TRUE(import_job);
  auto imported = WaitForJobTerminal(state, import_job.value->id);
  ASSERT_TRUE(imported.has_value());
  ASSERT_EQ(imported->status, JobStatus::kSucceeded);

  auto created = state.UpsertClientDirectorStub(
      "prod", "bareos-fd", "bareos-dir",
      {.description = std::string{"Initial stub"},
       .password = std::string{"[md5]11111111111111111111111111111111"},
       .address = std::string{"127.0.0.1"},
       .port = 9101,
       .allowed_script_dirs
       = std::vector<std::string>{"/usr/lib/bareos/scripts",
                                  "/opt/bareos/hooks"},
       .allowed_job_commands = std::vector<std::string>{"run-before-job-client",
                                                        "run-after-job-client"},
       .tls_authenticate = true,
       .tls_enable = true,
       .tls_require = true,
       .tls_verify_peer = true,
       .tls_cipher_list = std::string{"DEFAULT:@SECLEVEL=2"},
       .tls_cipher_suites = std::string{"TLS_AES_256_GCM_SHA384"},
       .tls_dh_file = std::string{"/etc/bareos/dh4096.pem"},
       .tls_protocol = std::string{"MinProtocol = TLSv1.2"},
       .tls_ca_certificate_file = std::string{"/etc/bareos/ca.pem"},
       .tls_ca_certificate_dir = std::string{"/etc/ssl/certs"},
       .tls_certificate_revocation_list = std::string{"/etc/bareos/crl.pem"},
       .tls_certificate = std::string{"/etc/bareos/client.crt"},
       .tls_key = std::string{"/etc/bareos/client.key"},
       .tls_allowed_cn = std::vector<std::string>{"bareos-dir.example.invalid",
                                                  "backup-dir.internal"},
       .connection_from_director_to_client = false,
       .connection_from_client_to_director = false,
       .monitor = true,
       .maximum_bandwidth_per_job = 2048});
  ASSERT_TRUE(created);
  EXPECT_EQ(created.value->name, "bareos-fd");

  const auto stub_path
      = created.value->path / "bareos-fd.d/director/bareos-dir.conf";
  std::ifstream created_file{stub_path};
  ASSERT_TRUE(created_file.good());
  const std::string created_text((std::istreambuf_iterator<char>(created_file)),
                                 std::istreambuf_iterator<char>());
  EXPECT_NE(
      created_text.find("Password = \"[md5]11111111111111111111111111111111\""),
      std::string::npos);
  EXPECT_NE(created_text.find("Description = \"Initial stub\""),
            std::string::npos);
  EXPECT_NE(created_text.find("Address = 127.0.0.1"), std::string::npos);
  EXPECT_NE(created_text.find("Port = 9101"), std::string::npos);
  EXPECT_NE(created_text.find("AllowedScriptDir = \"/usr/lib/bareos/scripts\""),
            std::string::npos);
  EXPECT_NE(created_text.find("AllowedScriptDir = \"/opt/bareos/hooks\""),
            std::string::npos);
  EXPECT_NE(created_text.find("AllowedJobCommand = \"run-before-job-client\""),
            std::string::npos);
  EXPECT_NE(created_text.find("AllowedJobCommand = \"run-after-job-client\""),
            std::string::npos);
  EXPECT_NE(created_text.find("TlsAuthenticate = yes"), std::string::npos);
  EXPECT_NE(created_text.find("TlsEnable = yes"), std::string::npos);
  EXPECT_NE(created_text.find("TlsRequire = yes"), std::string::npos);
  EXPECT_NE(created_text.find("TlsVerifyPeer = yes"), std::string::npos);
  EXPECT_NE(created_text.find("TlsCipherList = \"DEFAULT:@SECLEVEL=2\""),
            std::string::npos);
  EXPECT_NE(created_text.find("TlsCipherSuites = \"TLS_AES_256_GCM_SHA384\""),
            std::string::npos);
  EXPECT_NE(created_text.find("TlsDhFile = \"/etc/bareos/dh4096.pem\""),
            std::string::npos);
  EXPECT_NE(created_text.find("TlsProtocol = \"MinProtocol = TLSv1.2\""),
            std::string::npos);
  EXPECT_NE(created_text.find("TlsCaCertificateFile = \"/etc/bareos/ca.pem\""),
            std::string::npos);
  EXPECT_NE(created_text.find("TlsCaCertificateDir = \"/etc/ssl/certs\""),
            std::string::npos);
  EXPECT_NE(created_text.find(
                "TlsCertificateRevocationList = \"/etc/bareos/crl.pem\""),
            std::string::npos);
  EXPECT_NE(created_text.find("TlsCertificate = \"/etc/bareos/client.crt\""),
            std::string::npos);
  EXPECT_NE(created_text.find("TlsKey = \"/etc/bareos/client.key\""),
            std::string::npos);
  EXPECT_NE(created_text.find("TlsAllowedCn = \"bareos-dir.example.invalid\""),
            std::string::npos);
  EXPECT_NE(created_text.find("TlsAllowedCn = \"backup-dir.internal\""),
            std::string::npos);
  EXPECT_NE(created_text.find("ConnectionFromDirectorToClient = no"),
            std::string::npos);
  EXPECT_NE(created_text.find("ConnectionFromClientToDirector = no"),
            std::string::npos);
  EXPECT_NE(created_text.find("Monitor = yes"), std::string::npos);
  EXPECT_NE(created_text.find("MaximumBandwidthPerJob = 2048"),
            std::string::npos);

  auto updated = state.UpsertClientDirectorStub(
      "prod", "bareos-fd", "bareos-dir",
      {.description = std::string{"Updated stub"},
       .password = std::string{"[md5]22222222222222222222222222222222"}});
  ASSERT_TRUE(updated);

  std::ifstream updated_file{stub_path};
  ASSERT_TRUE(updated_file.good());
  const std::string updated_text((std::istreambuf_iterator<char>(updated_file)),
                                 std::istreambuf_iterator<char>());
  EXPECT_NE(
      updated_text.find("Password = \"[md5]22222222222222222222222222222222\""),
      std::string::npos);
  EXPECT_NE(updated_text.find("Description = \"Updated stub\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("Address = 127.0.0.1"), std::string::npos);
  EXPECT_NE(updated_text.find("Port = 9101"), std::string::npos);
  EXPECT_NE(updated_text.find("AllowedScriptDir = \"/usr/lib/bareos/scripts\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("AllowedScriptDir = \"/opt/bareos/hooks\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("AllowedJobCommand = \"run-before-job-client\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("AllowedJobCommand = \"run-after-job-client\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("TlsAuthenticate = yes"), std::string::npos);
  EXPECT_NE(updated_text.find("TlsEnable = yes"), std::string::npos);
  EXPECT_NE(updated_text.find("TlsRequire = yes"), std::string::npos);
  EXPECT_NE(updated_text.find("TlsVerifyPeer = yes"), std::string::npos);
  EXPECT_NE(updated_text.find("TlsCipherList = \"DEFAULT:@SECLEVEL=2\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("TlsCipherSuites = \"TLS_AES_256_GCM_SHA384\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("TlsDhFile = \"/etc/bareos/dh4096.pem\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("TlsProtocol = \"MinProtocol = TLSv1.2\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("TlsCaCertificateFile = \"/etc/bareos/ca.pem\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("TlsCaCertificateDir = \"/etc/ssl/certs\""),
            std::string::npos);
  EXPECT_NE(updated_text.find(
                "TlsCertificateRevocationList = \"/etc/bareos/crl.pem\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("TlsCertificate = \"/etc/bareos/client.crt\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("TlsKey = \"/etc/bareos/client.key\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("TlsAllowedCn = \"bareos-dir.example.invalid\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("TlsAllowedCn = \"backup-dir.internal\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("ConnectionFromDirectorToClient = no"),
            std::string::npos);
  EXPECT_NE(updated_text.find("ConnectionFromClientToDirector = no"),
            std::string::npos);
  EXPECT_NE(updated_text.find("Monitor = yes"), std::string::npos);
  EXPECT_NE(updated_text.find("MaximumBandwidthPerJob = 2048"),
            std::string::npos);

  auto current
      = state.GetClientDirectorStubSpec("prod", "bareos-fd", "bareos-dir");
  ASSERT_TRUE(current) << current.error;
  EXPECT_EQ(current.value->description, "Updated stub");
  EXPECT_EQ(current.value->password, "[md5]22222222222222222222222222222222");
  EXPECT_EQ(current.value->address, "127.0.0.1");
  ASSERT_TRUE(current.value->port.has_value());
  EXPECT_EQ(*current.value->port, 9101);
  ASSERT_TRUE(current.value->allowed_script_dirs.has_value());
  EXPECT_EQ(*current.value->allowed_script_dirs,
            (std::vector<std::string>{"/usr/lib/bareos/scripts",
                                      "/opt/bareos/hooks"}));
  ASSERT_TRUE(current.value->allowed_job_commands.has_value());
  EXPECT_EQ(*current.value->allowed_job_commands,
            (std::vector<std::string>{"run-before-job-client",
                                      "run-after-job-client"}));
  EXPECT_EQ(current.value->tls_authenticate, true);
  EXPECT_EQ(current.value->tls_enable, true);
  EXPECT_EQ(current.value->tls_require, true);
  EXPECT_EQ(current.value->tls_verify_peer, true);
  EXPECT_EQ(current.value->tls_cipher_list, "DEFAULT:@SECLEVEL=2");
  EXPECT_EQ(current.value->tls_cipher_suites, "TLS_AES_256_GCM_SHA384");
  EXPECT_EQ(current.value->tls_dh_file, "/etc/bareos/dh4096.pem");
  EXPECT_EQ(current.value->tls_protocol, "MinProtocol = TLSv1.2");
  EXPECT_EQ(current.value->tls_ca_certificate_file, "/etc/bareos/ca.pem");
  EXPECT_EQ(current.value->tls_ca_certificate_dir, "/etc/ssl/certs");
  EXPECT_EQ(current.value->tls_certificate_revocation_list,
            "/etc/bareos/crl.pem");
  EXPECT_EQ(current.value->tls_certificate, "/etc/bareos/client.crt");
  EXPECT_EQ(current.value->tls_key, "/etc/bareos/client.key");
  ASSERT_TRUE(current.value->tls_allowed_cn.has_value());
  EXPECT_EQ(*current.value->tls_allowed_cn,
            (std::vector<std::string>{"bareos-dir.example.invalid",
                                      "backup-dir.internal"}));
  EXPECT_EQ(current.value->connection_from_director_to_client, false);
  EXPECT_EQ(current.value->connection_from_client_to_director, false);
  EXPECT_EQ(current.value->monitor, true);
  EXPECT_EQ(current.value->maximum_bandwidth_per_job, 2048);

  auto rejected = state.UpsertClientDirectorStub("prod", "missing-client",
                                                 "bareos-dir", {});
  ASSERT_FALSE(rejected);
  EXPECT_NE(rejected.error.find("does not define client 'missing-client'"),
            std::string::npos);

  std::ifstream reverted_file{stub_path};
  ASSERT_TRUE(reverted_file.good());
  const std::string reverted_text(
      (std::istreambuf_iterator<char>(reverted_file)),
      std::istreambuf_iterator<char>());
  EXPECT_EQ(reverted_text, updated_text);
}

TEST(BconfigService, UpsertsClientDirectorStubsInSharedFiles)
{
  ScopedDirectory source_root{MakeTempPath()};
  ScopedDirectory repo_path{MakeTempPath()};
  ServiceState state;

  auto deployment
      = state.CreateDeployment({.id = "prod",
                                .name = "Production",
                                .repository_path = repo_path.path()});
  ASSERT_TRUE(deployment);

  const auto source_fixture_root = FindFixtureRoot();
  ASSERT_FALSE(source_fixture_root.empty());
  std::filesystem::create_directories(source_root.path());
  std::filesystem::copy(source_fixture_root / "bareos-dir.d",
                        source_root.path() / "bareos-dir.d",
                        std::filesystem::copy_options::recursive);

  auto import_job
      = state.CreateJob({.type = "import_configuration",
                         .deployment_id = std::string{"prod"},
                         .source_path = source_root.path().string()});
  ASSERT_TRUE(import_job);
  auto imported = WaitForJobTerminal(state, import_job.value->id);
  ASSERT_TRUE(imported.has_value());
  ASSERT_EQ(imported->status, JobStatus::kSucceeded);

  const auto stub_directory
      = RepositoryLayout::ClientsDirectory(repo_path.path())
        / "bareos-fd/bareos-fd.d/director";
  const auto original_path = stub_directory / "bareos-dir.conf";
  const auto shared_path = stub_directory / "shared.conf";
  const auto original_text = ReadTextFile(original_path);
  WriteTextFile(shared_path,
                original_text
                    + "\nDirector {\n"
                      "  Name = \"other-dir\"\n"
                      "  Password = \"other-password\"\n"
                      "}\n");
  std::filesystem::remove(original_path);

  auto updated = state.UpsertClientDirectorStub(
      "prod", "bareos-fd", "bareos-dir",
      {.description = std::string{"Updated shared stub"},
       .password = std::string{"[md5]33333333333333333333333333333333"}});
  ASSERT_TRUE(updated) << updated.error;
  EXPECT_FALSE(std::filesystem::exists(original_path));
  EXPECT_TRUE(std::filesystem::exists(shared_path));
  const auto shared_text = ReadTextFile(shared_path);
  EXPECT_NE(
      shared_text.find("Password = \"[md5]33333333333333333333333333333333\""),
      std::string::npos);
  EXPECT_NE(shared_text.find("Description = \"Updated shared stub\""),
            std::string::npos);
  EXPECT_NE(shared_text.find("Name = \"other-dir\""), std::string::npos);
}

TEST(BconfigService, UpsertsClientDirectorStubsPreserveLargeImportedPort)
{
  ScopedDirectory source_root{MakeTempPath()};
  ScopedDirectory repo_path{MakeTempPath()};
  ServiceState state;

  auto deployment
      = state.CreateDeployment({.id = "prod",
                                .name = "Production",
                                .repository_path = repo_path.path()});
  ASSERT_TRUE(deployment);

  const auto source_fixture_root = FindFixtureRoot();
  ASSERT_FALSE(source_fixture_root.empty());
  std::filesystem::create_directories(source_root.path());
  std::filesystem::copy(source_fixture_root / "bareos-dir.d",
                        source_root.path() / "bareos-dir.d",
                        std::filesystem::copy_options::recursive);

  auto import_job
      = state.CreateJob({.type = "import_configuration",
                         .deployment_id = std::string{"prod"},
                         .source_path = source_root.path().string()});
  ASSERT_TRUE(import_job);
  auto imported = WaitForJobTerminal(state, import_job.value->id);
  ASSERT_TRUE(imported.has_value());
  ASSERT_EQ(imported->status, JobStatus::kSucceeded);

  const auto stub_path = RepositoryLayout::ClientsDirectory(repo_path.path())
                         / "bareos-fd/bareos-fd.d/director/bareos-dir.conf";
  WriteTextFile(stub_path,
                "Director {\n"
                "  Name = \"bareos-dir\"\n"
                "  Password = \"[md5]abcdef0123456789abcdef0123456789\"\n"
                "  Description = \"Imported stub\"\n"
                "  Address = localhost\n"
                "  Port = 70000\n"
                "}\n");

  auto updated = state.UpsertClientDirectorStub(
      "prod", "bareos-fd", "bareos-dir",
      {.description = std::string{"Updated imported stub"}});
  ASSERT_TRUE(updated) << updated.error;

  const auto updated_text = ReadTextFile(stub_path);
  EXPECT_NE(updated_text.find("Address = localhost"), std::string::npos);
  EXPECT_NE(updated_text.find("Port = 70000"), std::string::npos);
  EXPECT_NE(updated_text.find("Description = \"Updated imported stub\""),
            std::string::npos);

  auto current
      = state.GetClientDirectorStubSpec("prod", "bareos-fd", "bareos-dir");
  ASSERT_TRUE(current) << current.error;
  EXPECT_EQ(current.value->description, "Updated imported stub");
  EXPECT_EQ(current.value->address, "localhost");
  EXPECT_FALSE(current.value->port.has_value());
}

TEST(BconfigService, ServesClientPrefillRoutesOverHttp)
{
#if HAVE_WIN32
  GTEST_SKIP() << "HTTP prefill route coverage is only implemented on POSIX.";
#else
  ScopedDirectory source_root{MakeTempPath()};
  ScopedDirectory repo_path{MakeTempPath()};
  ScopedDirectory state_root{MakeTempPath()};

  {
    ServiceState state{state_root.path()};

    auto deployment
        = state.CreateDeployment({.id = "prod",
                                  .name = "Production",
                                  .repository_path = repo_path.path()});
    ASSERT_TRUE(deployment);

    const auto source_fixture_root = FindFixtureRoot();
    ASSERT_FALSE(source_fixture_root.empty());
    std::filesystem::create_directories(source_root.path());
    std::filesystem::copy(source_fixture_root / "bareos-dir.d",
                          source_root.path() / "bareos-dir.d",
                          std::filesystem::copy_options::recursive);

    auto import_job
        = state.CreateJob({.type = "import_configuration",
                           .deployment_id = std::string{"prod"},
                           .source_path = source_root.path().string()});
    ASSERT_TRUE(import_job);
    auto imported = WaitForJobTerminal(state, import_job.value->id);
    ASSERT_TRUE(imported.has_value());
    ASSERT_EQ(imported->status, JobStatus::kSucceeded);

    auto messages = state.UpsertClientMessagesResource(
        "prod", "bareos-fd", "ManagedMessages",
        {.description = std::string{"HTTP-managed client messages"},
         .stdout_entries = std::vector<std::string>{"all, !restored"},
         .director_entries
         = std::vector<std::string>{"bareos-dir = all, !skipped"}});
    ASSERT_TRUE(messages) << messages.error;

    auto daemon = state.UpsertClientDaemonResource(
        "prod", "bareos-fd",
        {.addresses = std::vector<std::string>{"host[ipv4;192.0.2.10;42102]",
                                               "host[ipv6;::1;42102]"},
         .allow_bandwidth_bursting = true,
         .description = std::string{"HTTP-managed client daemon"},
         .allowed_job_commands
         = std::vector<std::string>{"RunBeforeJob", "Estimate listing"},
         .messages = std::string{"ManagedMessages"}});
    ASSERT_TRUE(daemon) << daemon.error;

    auto director = state.UpsertClientDirectorStub(
        "prod", "bareos-fd", "bareos-dir",
        {.description = std::string{"HTTP-managed client director"},
         .password = std::string{"[md5]44444444444444444444444444444444"},
         .address = std::string{"127.0.0.1"},
         .port = 9101,
         .allowed_script_dirs
         = std::vector<std::string>{"/usr/lib/bareos/scripts",
                                    "/opt/bareos/hooks"},
         .monitor = true,
         .maximum_bandwidth_per_job = 2048});
    ASSERT_TRUE(director) << director.error;
  }

  ScopedBconfigServiceServer server{state_root.path()};
  ASSERT_TRUE(server.ready()) << server.startup_error();

  const auto daemon_response
      = server.Get("/v1/deployments/prod/clients/bareos-fd/prefill");
  ASSERT_EQ(daemon_response.status_code, 200u) << daemon_response.body;
  auto daemon_json = ParseJson(daemon_response.body);
  ASSERT_NE(daemon_json.get(), nullptr) << daemon_response.body;
  auto* daemon_deployment = json_object_get(daemon_json.get(), "deployment");
  ASSERT_TRUE(json_is_object(daemon_deployment));
  auto* daemon_deployment_id = json_object_get(daemon_deployment, "id");
  ASSERT_TRUE(json_is_string(daemon_deployment_id));
  EXPECT_STREQ(json_string_value(daemon_deployment_id), "prod");
  auto* daemon_deployment_name = json_object_get(daemon_deployment, "name");
  ASSERT_TRUE(json_is_string(daemon_deployment_name));
  EXPECT_STREQ(json_string_value(daemon_deployment_name), "Production");
  auto* daemon_spec = json_object_get(daemon_json.get(), "spec");
  ASSERT_TRUE(json_is_object(daemon_spec));
  auto* daemon_description = json_object_get(daemon_spec, "description");
  ASSERT_TRUE(json_is_string(daemon_description));
  EXPECT_STREQ(json_string_value(daemon_description),
               "HTTP-managed client daemon");
  auto* daemon_messages = json_object_get(daemon_spec, "messages");
  ASSERT_TRUE(json_is_string(daemon_messages));
  EXPECT_STREQ(json_string_value(daemon_messages), "ManagedMessages");
  auto* daemon_allow_burst
      = json_object_get(daemon_spec, "allow_bandwidth_bursting");
  ASSERT_TRUE(json_is_boolean(daemon_allow_burst));
  EXPECT_TRUE(json_is_true(daemon_allow_burst));
  auto* daemon_addresses = json_object_get(daemon_spec, "addresses");
  ASSERT_TRUE(json_is_array(daemon_addresses));
  ASSERT_EQ(json_array_size(daemon_addresses), 2u);
  EXPECT_STREQ(json_string_value(json_array_get(daemon_addresses, 0)),
               "host[ipv4;192.0.2.10;42102]");
  auto* daemon_job_commands
      = json_object_get(daemon_spec, "allowed_job_commands");
  ASSERT_TRUE(json_is_array(daemon_job_commands));
  ASSERT_EQ(json_array_size(daemon_job_commands), 2u);
  EXPECT_STREQ(json_string_value(json_array_get(daemon_job_commands, 1)),
               "Estimate listing");

  const auto messages_response = server.Get(
      "/v1/deployments/prod/clients/bareos-fd/messages/ManagedMessages/"
      "prefill");
  ASSERT_EQ(messages_response.status_code, 200u) << messages_response.body;
  auto messages_json = ParseJson(messages_response.body);
  ASSERT_NE(messages_json.get(), nullptr) << messages_response.body;
  auto* messages_spec = json_object_get(messages_json.get(), "spec");
  ASSERT_TRUE(json_is_object(messages_spec));
  auto* messages_description = json_object_get(messages_spec, "description");
  ASSERT_TRUE(json_is_string(messages_description));
  EXPECT_STREQ(json_string_value(messages_description),
               "HTTP-managed client messages");
  auto* messages_stdout = json_object_get(messages_spec, "stdout_entries");
  ASSERT_TRUE(json_is_array(messages_stdout));
  ASSERT_EQ(json_array_size(messages_stdout), 1u);
  EXPECT_STREQ(json_string_value(json_array_get(messages_stdout, 0)),
               "all, !restored");
  auto* messages_directors = json_object_get(messages_spec, "director_entries");
  ASSERT_TRUE(json_is_array(messages_directors));
  ASSERT_EQ(json_array_size(messages_directors), 1u);
  EXPECT_STREQ(json_string_value(json_array_get(messages_directors, 0)),
               "bareos-dir = all, !skipped");

  const auto director_response = server.Get(
      "/v1/deployments/prod/clients/bareos-fd/directors/bareos-dir/prefill");
  ASSERT_EQ(director_response.status_code, 200u) << director_response.body;
  auto director_json = ParseJson(director_response.body);
  ASSERT_NE(director_json.get(), nullptr) << director_response.body;
  auto* director_spec = json_object_get(director_json.get(), "spec");
  ASSERT_TRUE(json_is_object(director_spec));
  auto* director_description = json_object_get(director_spec, "description");
  ASSERT_TRUE(json_is_string(director_description));
  EXPECT_STREQ(json_string_value(director_description),
               "HTTP-managed client director");
  auto* director_address = json_object_get(director_spec, "address");
  ASSERT_TRUE(json_is_string(director_address));
  EXPECT_STREQ(json_string_value(director_address), "127.0.0.1");
  auto* director_port = json_object_get(director_spec, "port");
  ASSERT_TRUE(json_is_integer(director_port));
  EXPECT_EQ(json_integer_value(director_port), 9101);
  auto* director_script_dirs
      = json_object_get(director_spec, "allowed_script_dirs");
  ASSERT_TRUE(json_is_array(director_script_dirs));
  ASSERT_EQ(json_array_size(director_script_dirs), 2u);
  EXPECT_STREQ(json_string_value(json_array_get(director_script_dirs, 0)),
               "/usr/lib/bareos/scripts");
  auto* director_monitor = json_object_get(director_spec, "monitor");
  ASSERT_TRUE(json_is_boolean(director_monitor));
  EXPECT_TRUE(json_is_true(director_monitor));
  auto* director_bandwidth
      = json_object_get(director_spec, "maximum_bandwidth_per_job");
  ASSERT_TRUE(json_is_integer(director_bandwidth));
  EXPECT_EQ(json_integer_value(director_bandwidth), 2048);

  const auto missing_director_response = server.Get(
      "/v1/deployments/prod/clients/bareos-fd/directors/MissingDirector/"
      "prefill");
  EXPECT_EQ(missing_director_response.status_code, 400u);
  EXPECT_NE(missing_director_response.body.find(
                "client director 'MissingDirector' not found."),
            std::string::npos);

  const auto missing_deployment_response
      = server.Get("/v1/deployments/missing/clients/bareos-fd/prefill");
  EXPECT_EQ(missing_deployment_response.status_code, 404u);
  EXPECT_NE(missing_deployment_response.body.find("deployment not found."),
            std::string::npos);
#endif
}

TEST(BconfigService, DeletesClientDirectorStubs)
{
  ScopedDirectory source_root{MakeTempPath()};
  ScopedDirectory repo_path{MakeTempPath()};
  ServiceState state;

  auto deployment
      = state.CreateDeployment({.id = "prod",
                                .name = "Production",
                                .repository_path = repo_path.path()});
  ASSERT_TRUE(deployment);

  const auto source_fixture_root = FindFixtureRoot();
  ASSERT_FALSE(source_fixture_root.empty());
  std::filesystem::create_directories(source_root.path());
  std::filesystem::copy(source_fixture_root / "bareos-fd.d",
                        source_root.path() / "bareos-fd.d",
                        std::filesystem::copy_options::recursive);

  auto import_job
      = state.CreateJob({.type = "import_configuration",
                         .deployment_id = std::string{"prod"},
                         .source_path = source_root.path().string()});
  ASSERT_TRUE(import_job);
  auto imported = WaitForJobTerminal(state, import_job.value->id);
  ASSERT_TRUE(imported.has_value());
  ASSERT_EQ(imported->status, JobStatus::kSucceeded);

  const auto stub_path = RepositoryLayout::ClientsDirectory(repo_path.path())
                         / "backup-bareos-test-fd/bareos-fd.d/director/"
                           "bareos-dir.conf";
  ASSERT_TRUE(std::filesystem::exists(stub_path));

  auto deleted = state.DeleteClientDirectorStub("prod", "backup-bareos-test-fd",
                                                "bareos-dir");
  ASSERT_TRUE(deleted) << deleted.error;
  ASSERT_TRUE(deleted.value->has_value());
  EXPECT_FALSE(std::filesystem::exists(stub_path));

  auto client = state.GetDeploymentConfig("prod", bconfig::Component::kClient,
                                          "backup-bareos-test-fd");
  ASSERT_TRUE(client);
}

TEST(BconfigService, DeletesClientDirectorStubsFromSharedFiles)
{
  ScopedDirectory source_root{MakeTempPath()};
  ScopedDirectory repo_path{MakeTempPath()};
  ServiceState state;

  auto deployment
      = state.CreateDeployment({.id = "prod",
                                .name = "Production",
                                .repository_path = repo_path.path()});
  ASSERT_TRUE(deployment);

  const auto source_fixture_root = FindFixtureRoot();
  ASSERT_FALSE(source_fixture_root.empty());
  std::filesystem::create_directories(source_root.path());
  std::filesystem::copy(source_fixture_root / "bareos-fd.d",
                        source_root.path() / "bareos-fd.d",
                        std::filesystem::copy_options::recursive);

  auto import_job
      = state.CreateJob({.type = "import_configuration",
                         .deployment_id = std::string{"prod"},
                         .source_path = source_root.path().string()});
  ASSERT_TRUE(import_job);
  auto imported = WaitForJobTerminal(state, import_job.value->id);
  ASSERT_TRUE(imported.has_value());
  ASSERT_EQ(imported->status, JobStatus::kSucceeded);

  const auto stub_directory
      = RepositoryLayout::ClientsDirectory(repo_path.path())
        / "backup-bareos-test-fd/bareos-fd.d/director";
  const auto original_path = stub_directory / "bareos-dir.conf";
  const auto shared_path = stub_directory / "shared.conf";
  const auto original_text = ReadTextFile(original_path);
  WriteTextFile(shared_path,
                original_text
                    + "\nDirector {\n"
                      "  Name = \"other-dir\"\n"
                      "  Password = \"other-password\"\n"
                      "}\n");
  std::filesystem::remove(original_path);

  auto deleted = state.DeleteClientDirectorStub("prod", "backup-bareos-test-fd",
                                                "other-dir");
  ASSERT_TRUE(deleted) << deleted.error;
  ASSERT_TRUE(deleted.value->has_value());
  EXPECT_FALSE(std::filesystem::exists(original_path));
  EXPECT_TRUE(std::filesystem::exists(shared_path));
  const auto shared_text = ReadTextFile(shared_path);
  EXPECT_NE(shared_text.find("Name = bareos-dir"), std::string::npos);
  EXPECT_EQ(shared_text.find("Name = \"other-dir\""), std::string::npos);
}

TEST(BconfigService, RejectsMissingClientDirectorStubDeletes)
{
  ScopedDirectory source_root{MakeTempPath()};
  ScopedDirectory repo_path{MakeTempPath()};
  ServiceState state;

  auto deployment
      = state.CreateDeployment({.id = "prod",
                                .name = "Production",
                                .repository_path = repo_path.path()});
  ASSERT_TRUE(deployment);

  const auto source_fixture_root = FindFixtureRoot();
  ASSERT_FALSE(source_fixture_root.empty());
  std::filesystem::create_directories(source_root.path());
  std::filesystem::copy(source_fixture_root / "bareos-fd.d",
                        source_root.path() / "bareos-fd.d",
                        std::filesystem::copy_options::recursive);

  auto import_job
      = state.CreateJob({.type = "import_configuration",
                         .deployment_id = std::string{"prod"},
                         .source_path = source_root.path().string()});
  ASSERT_TRUE(import_job);
  auto imported = WaitForJobTerminal(state, import_job.value->id);
  ASSERT_TRUE(imported.has_value());
  ASSERT_EQ(imported->status, JobStatus::kSucceeded);

  auto rejected = state.DeleteClientDirectorStub(
      "prod", "backup-bareos-test-fd", "missing-dir");
  ASSERT_FALSE(rejected);
  EXPECT_NE(rejected.error.find("does not define director"), std::string::npos);
}

TEST(BconfigService, DeletesGeneratedClientDirectorStubs)
{
  ScopedDirectory source_root{MakeTempPath()};
  ScopedDirectory repo_path{MakeTempPath()};
  ServiceState state;

  auto deployment
      = state.CreateDeployment({.id = "prod",
                                .name = "Production",
                                .repository_path = repo_path.path()});
  ASSERT_TRUE(deployment);

  const auto source_fixture_root = FindFixtureRoot();
  ASSERT_FALSE(source_fixture_root.empty());
  std::filesystem::create_directories(source_root.path());
  std::filesystem::copy(source_fixture_root / "bareos-dir.d",
                        source_root.path() / "bareos-dir.d",
                        std::filesystem::copy_options::recursive);

  auto import_job
      = state.CreateJob({.type = "import_configuration",
                         .deployment_id = std::string{"prod"},
                         .source_path = source_root.path().string()});
  ASSERT_TRUE(import_job);
  auto imported = WaitForJobTerminal(state, import_job.value->id);
  ASSERT_TRUE(imported.has_value());
  ASSERT_EQ(imported->status, JobStatus::kSucceeded);

  const auto client_root
      = RepositoryLayout::ClientsDirectory(repo_path.path()) / "bareos-fd";
  const auto stub_path = client_root / "bareos-fd.d/director/bareos-dir.conf";
  ASSERT_TRUE(std::filesystem::exists(stub_path));

  auto deleted
      = state.DeleteClientDirectorStub("prod", "bareos-fd", "bareos-dir");
  if (!deleted) { FAIL() << deleted.error; }
  ASSERT_FALSE(deleted.value->has_value());
  EXPECT_FALSE(std::filesystem::exists(stub_path));
  EXPECT_FALSE(std::filesystem::exists(client_root));

  auto client = state.GetDeploymentConfig("prod", bconfig::Component::kClient,
                                          "bareos-fd");
  ASSERT_FALSE(client);
}

}  // namespace
}  // namespace bconfig::service
