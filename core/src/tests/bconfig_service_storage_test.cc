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
TEST(BconfigService, UpsertsStorageMessagesResources)
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
  std::filesystem::copy(source_fixture_root / "bareos-sd.d",
                        source_root.path() / "bareos-sd.d",
                        std::filesystem::copy_options::recursive);
  WriteTextFile(source_root.path() / "bareos-sd.d/storage/bareos-sd.conf",
                "Storage {\n"
                "  Name = \"bareos-sd\"\n"
                "}\n");

  auto import_job
      = state.CreateJob({.type = "import_configuration",
                         .deployment_id = std::string{"prod"},
                         .source_path = source_root.path().string()});
  ASSERT_TRUE(import_job);
  auto imported = WaitForJobTerminal(state, import_job.value->id);
  ASSERT_TRUE(imported.has_value());
  ASSERT_EQ(imported->status, JobStatus::kSucceeded);

  auto created = state.UpsertStorageMessagesResource(
      "prod", "bareos-sd", "ManagedMessages",
      {.description = std::string{"Managed storage messages"},
       .mail_command = std::string{"/usr/lib/bareos/storage-mail %r"},
       .operator_command = std::string{"/usr/lib/bareos/storage-operator %r"},
       .timestamp_format = std::string{"%d-%b %H:%M"},
       .append_entries
       = std::vector<std::string>{"\"/var/log/bareos/storage.log\" = all"},
       .director_entries = std::vector<std::string>{"bareos-dir = all"}});
  ASSERT_TRUE(created) << created.error;
  EXPECT_EQ(created.value->name, "bareos-sd");

  const auto messages_path
      = created.value->path / "bareos-sd.d/messages/ManagedMessages.conf";
  const auto created_text = ReadTextFile(messages_path);
  EXPECT_NE(created_text.find("Messages {"), std::string::npos);
  EXPECT_NE(created_text.find("Name = \"ManagedMessages\""), std::string::npos);
  EXPECT_NE(created_text.find("Description = \"Managed storage messages\""),
            std::string::npos);
  EXPECT_NE(
      created_text.find("MailCommand = \"/usr/lib/bareos/storage-mail %r\""),
      std::string::npos);
  EXPECT_NE(created_text.find("OperatorCommand = "
                              "\"/usr/lib/bareos/storage-operator %r\""),
            std::string::npos);
  EXPECT_NE(created_text.find("TimestampFormat = \"%d-%b %H:%M\""),
            std::string::npos);
  EXPECT_NE(created_text.find("Director = bareos-dir = all"),
            std::string::npos);
  EXPECT_NE(created_text.find("Append = \"/var/log/bareos/storage.log\" = all"),
            std::string::npos);

  const auto standard_path
      = created.value->path / "bareos-sd.d/messages/Standard.conf";
  auto standard_text = ReadTextFile(standard_path);
  auto standard_brace = standard_text.rfind("}\n");
  ASSERT_NE(standard_brace, std::string::npos);
  standard_text.insert(standard_brace,
                       "  mailcommand = \"/usr/lib/bareos/storage-mail %r\"\n"
                       "  operatorcommand = "
                       "\"/usr/lib/bareos/storage-operator %r\"\n"
                       "  timestampformat = \"%d-%b %H:%M\"\n"
                       "  append = \"/var/log/bareos/storage.log\" = all\n");
  WriteTextFile(standard_path, standard_text);

  auto updated = state.UpsertStorageMessagesResource(
      "prod", "bareos-sd", "Standard",
      {.description = std::string{"Updated storage messages"}});
  ASSERT_TRUE(updated) << updated.error;

  const auto updated_text = ReadTextFile(standard_path);
  EXPECT_NE(updated_text.find("Description = \"Updated storage messages\""),
            std::string::npos);
  EXPECT_NE(
      updated_text.find("MailCommand = \"/usr/lib/bareos/storage-mail %r\""),
      std::string::npos);
  EXPECT_NE(updated_text.find("OperatorCommand = "
                              "\"/usr/lib/bareos/storage-operator %r\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("TimestampFormat = \"%d-%b %H:%M\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("Director = bareos-dir = all"),
            std::string::npos);
  EXPECT_NE(updated_text.find("Append = \"/var/log/bareos/storage.log\" = all"),
            std::string::npos);

  auto current
      = state.GetStorageMessagesResourceSpec("prod", "bareos-sd", "Standard");
  ASSERT_TRUE(current) << current.error;
  EXPECT_EQ(current.value->description, "Updated storage messages");
  EXPECT_EQ(current.value->mail_command, "/usr/lib/bareos/storage-mail %r");
  EXPECT_EQ(current.value->operator_command,
            "/usr/lib/bareos/storage-operator %r");
  EXPECT_EQ(current.value->timestamp_format, "%d-%b %H:%M");
  ASSERT_TRUE(current.value->append_entries.has_value());
  EXPECT_EQ(
      *current.value->append_entries,
      (std::vector<std::string>{"\"/var/log/bareos/storage.log\" = all"}));
  ASSERT_TRUE(current.value->director_entries.has_value());
  EXPECT_EQ(*current.value->director_entries,
            (std::vector<std::string>{"bareos-dir = all"}));
}

TEST(BconfigService, UpsertsStorageDaemonResources)
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
  std::filesystem::copy(source_fixture_root / "bareos-sd.d",
                        source_root.path() / "bareos-sd.d",
                        std::filesystem::copy_options::recursive);
  WriteTextFile(source_root.path() / "bareos-sd.d/storage/bareos-sd.conf",
                "Storage {\n"
                "  Name = \"bareos-sd\"\n"
                "}\n");

  auto import_job
      = state.CreateJob({.type = "import_configuration",
                         .deployment_id = std::string{"prod"},
                         .source_path = source_root.path().string()});
  ASSERT_TRUE(import_job);
  auto imported = WaitForJobTerminal(state, import_job.value->id);
  ASSERT_TRUE(imported.has_value());
  ASSERT_EQ(imported->status, JobStatus::kSucceeded);

  auto updated = state.UpsertStorageDaemonResource(
      "prod", "bareos-sd",
      {.addresses = std::vector<std::string>{"host[ipv4;192.0.2.20;42103]",
                                             "host[ipv6;::1;42103]"},
       .source_address = std::string{"192.0.2.20"},
       .just_in_time_reservation = true,
       .maximum_concurrent_jobs = 84,
       .absolute_job_timeout = 1200,
       .allow_bandwidth_bursting = false,
       .tls_authenticate = true,
       .tls_enable = true,
       .tls_require = false,
       .tls_verify_peer = false,
       .tls_cipher_list = std::string{"ECDHE-RSA-AES256-GCM-SHA384"},
       .tls_cipher_suites = std::string{"TLS_AES_256_GCM_SHA384"},
       .tls_dh_file = std::string{"/etc/bareos/dh4096.pem"},
       .tls_protocol = std::string{"MinProtocol = TLSv1.2"},
       .tls_ca_certificate_file = std::string{"/etc/bareos/ca.crt"},
       .tls_ca_certificate_dir = std::string{"/etc/ssl/certs"},
       .tls_certificate_revocation_list = std::string{"/etc/bareos/crl.pem"},
       .tls_certificate = std::string{"/etc/bareos/storage.crt"},
       .tls_key = std::string{"/etc/bareos/storage.key"},
       .tls_allowed_cn
       = std::vector<std::string>{"storage-cn-1", "storage-cn-2"},
       .ndmp_enable = true,
       .ndmp_snooping = false,
       .ndmp_log_level = 6,
       .ndmp_addresses = std::vector<std::string>{"host[ipv4;192.0.2.30;10001]",
                                                  "host[ipv6;::1;10001]"},
       .autoxflate_on_replication = true,
       .collect_device_statistics = true,
       .collect_job_statistics = false,
       .statistics_collect_interval = 15,
       .device_reserve_by_media_type = true,
       .file_device_concurrent_read = false,
       .ver_id = std::string{"storage-ver"},
       .log_timestamp_format = std::string{"%d-%b %H:%M"},
       .maximum_bandwidth_per_job = 4096,
       .secure_erase_command = std::string{"/usr/bin/wipefs --all"},
       .enable_ktls = false,
       .sd_connect_timeout = 1800,
       .fd_connect_timeout = 5400,
       .heartbeat_interval = 120,
       .checkpoint_interval = 300,
       .client_connect_wait = 30,
       .maximum_network_buffer_size = 2097152,
       .description = std::string{"Managed storage daemon"},
       .working_directory = std::string{"/var/lib/bareos/storage"},
       .plugin_directory = std::string{"/usr/lib/bareos/plugins"},
       .plugin_names = std::vector<std::string>{"autochanger", "python"},
#if defined(HAVE_DYNAMIC_SD_BACKENDS)
       .backend_directories
       = std::vector<std::string>{"/usr/lib/bareos/backends",
                                  "/opt/bareos/backends"},
#endif
       .scripts_directory = std::string{"/usr/lib/bareos/scripts"},
       .messages = std::string{"Standard"}});
  ASSERT_TRUE(updated) << updated.error;
  EXPECT_EQ(updated.value->name, "bareos-sd");

  const auto storage_path
      = updated.value->path / "bareos-sd.d/storage/bareos-sd.conf";
  const auto updated_text = ReadTextFile(storage_path);
  EXPECT_NE(updated_text.find("Storage {"), std::string::npos);
  EXPECT_NE(updated_text.find("Name = \"bareos-sd\""), std::string::npos);
  EXPECT_NE(updated_text.find("Addresses = {"), std::string::npos);
  EXPECT_NE(updated_text.find("ipv4 = { addr = 192.0.2.20 ; port = 42103 }"),
            std::string::npos);
  EXPECT_NE(updated_text.find("ipv6 = { addr = ::1 ; port = 42103 }"),
            std::string::npos);
  EXPECT_NE(updated_text.find("SourceAddress = 192.0.2.20"), std::string::npos);
  EXPECT_NE(updated_text.find("JustInTimeReservation = yes"),
            std::string::npos);
  EXPECT_NE(updated_text.find("MaximumConcurrentJobs = 84"), std::string::npos);
  EXPECT_NE(updated_text.find("AbsoluteJobTimeout = 1200"), std::string::npos);
  EXPECT_NE(updated_text.find("AllowBandwidthBursting = no"),
            std::string::npos);
  EXPECT_NE(updated_text.find("TlsAuthenticate = yes"), std::string::npos);
  EXPECT_NE(updated_text.find("TlsEnable = yes"), std::string::npos);
  EXPECT_NE(updated_text.find("TlsRequire = no"), std::string::npos);
  EXPECT_NE(updated_text.find("TlsVerifyPeer = no"), std::string::npos);
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
  EXPECT_NE(updated_text.find("TlsCertificate = \"/etc/bareos/storage.crt\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("TlsKey = \"/etc/bareos/storage.key\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("TlsAllowedCn = \"storage-cn-1\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("TlsAllowedCn = \"storage-cn-2\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("NdmpEnable = yes"), std::string::npos);
  EXPECT_NE(updated_text.find("NdmpSnooping = no"), std::string::npos);
  EXPECT_NE(updated_text.find("NdmpLogLevel = 6"), std::string::npos);
  EXPECT_NE(updated_text.find("NdmpAddresses = {"), std::string::npos);
  EXPECT_NE(updated_text.find("ipv4 = { addr = 192.0.2.30 ; port = 10001 }"),
            std::string::npos);
  EXPECT_NE(updated_text.find("ipv6 = { addr = ::1 ; port = 10001 }"),
            std::string::npos);
  EXPECT_NE(updated_text.find("AutoXFlateOnReplication = yes"),
            std::string::npos);
  EXPECT_NE(updated_text.find("CollectDeviceStatistics = yes"),
            std::string::npos);
  EXPECT_NE(updated_text.find("CollectJobStatistics = no"), std::string::npos);
  EXPECT_NE(updated_text.find("StatisticsCollectInterval = 15"),
            std::string::npos);
  EXPECT_NE(updated_text.find("DeviceReserveByMediaType = yes"),
            std::string::npos);
  EXPECT_NE(updated_text.find("FileDeviceConcurrentRead = no"),
            std::string::npos);
  EXPECT_NE(updated_text.find("VerId = \"storage-ver\""), std::string::npos);
  EXPECT_NE(updated_text.find("LogTimestampFormat = \"%d-%b %H:%M\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("MaximumBandwidthPerJob = 4096"),
            std::string::npos);
  EXPECT_NE(updated_text.find("SecureEraseCommand = \"/usr/bin/wipefs --all\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("EnableKtls = no"), std::string::npos);
  EXPECT_NE(updated_text.find("SdConnectTimeout = 1800"), std::string::npos);
  EXPECT_NE(updated_text.find("FdConnectTimeout = 5400"), std::string::npos);
  EXPECT_NE(updated_text.find("HeartbeatInterval = 120"), std::string::npos);
  EXPECT_NE(updated_text.find("CheckpointInterval = 300"), std::string::npos);
  EXPECT_NE(updated_text.find("ClientConnectWait = 30"), std::string::npos);
  EXPECT_NE(updated_text.find("MaximumNetworkBufferSize = 2097152"),
            std::string::npos);
  EXPECT_NE(updated_text.find("Description = \"Managed storage daemon\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("WorkingDirectory = \"/var/lib/bareos/storage\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("PluginDirectory = \"/usr/lib/bareos/plugins\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("PluginNames = autochanger"), std::string::npos);
  EXPECT_NE(updated_text.find("PluginNames = python"), std::string::npos);
#if defined(HAVE_DYNAMIC_SD_BACKENDS)
  EXPECT_NE(
      updated_text.find("BackendDirectory = \"/usr/lib/bareos/backends\""),
      std::string::npos);
  EXPECT_NE(updated_text.find("BackendDirectory = \"/opt/bareos/backends\""),
            std::string::npos);
#endif
  EXPECT_NE(updated_text.find("ScriptsDirectory = \"/usr/lib/bareos/scripts\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("Messages = Standard"), std::string::npos);

  auto current = state.GetStorageDaemonResourceSpec("prod", "bareos-sd");
  ASSERT_TRUE(current) << current.error;
  ASSERT_TRUE(current.value->addresses.has_value());
  EXPECT_EQ(*current.value->addresses,
            (std::vector<std::string>{"host[ipv4;192.0.2.20;42103]",
                                      "host[ipv6;::1;42103]"}));
  EXPECT_EQ(current.value->just_in_time_reservation, true);
  ASSERT_TRUE(current.value->tls_allowed_cn.has_value());
  EXPECT_EQ(*current.value->tls_allowed_cn,
            (std::vector<std::string>{"storage-cn-1", "storage-cn-2"}));
  ASSERT_TRUE(current.value->ndmp_addresses.has_value());
  EXPECT_EQ(*current.value->ndmp_addresses,
            (std::vector<std::string>{"host[ipv4;192.0.2.30;10001]",
                                      "host[ipv6;::1;10001]"}));
  ASSERT_TRUE(current.value->plugin_names.has_value());
  EXPECT_EQ(*current.value->plugin_names,
            (std::vector<std::string>{"autochanger", "python"}));
#if defined(HAVE_DYNAMIC_SD_BACKENDS)
  ASSERT_TRUE(current.value->backend_directories.has_value());
  EXPECT_EQ(*current.value->backend_directories,
            (std::vector<std::string>{"/usr/lib/bareos/backends",
                                      "/opt/bareos/backends"}));
#endif
  EXPECT_EQ(current.value->messages, "Standard");
}

TEST(BconfigService, UpsertsStorageDaemonResourcesInSharedFiles)
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
  std::filesystem::copy(source_fixture_root / "bareos-sd.d",
                        source_root.path() / "bareos-sd.d",
                        std::filesystem::copy_options::recursive);
  WriteTextFile(source_root.path() / "bareos-sd.d/storage/bareos-sd.conf",
                "Storage {\n"
                "  Name = \"bareos-sd\"\n"
                "}\n");

  auto import_job
      = state.CreateJob({.type = "import_configuration",
                         .deployment_id = std::string{"prod"},
                         .source_path = source_root.path().string()});
  ASSERT_TRUE(import_job);
  auto imported = WaitForJobTerminal(state, import_job.value->id);
  ASSERT_TRUE(imported.has_value());
  ASSERT_EQ(imported->status, JobStatus::kSucceeded);

  const auto storage_directory
      = RepositoryLayout::StoragesDirectory(repo_path.path())
        / "bareos-sd/bareos-sd.d/storage";
  const auto original_path = storage_directory / "bareos-sd.conf";
  const auto shared_path = storage_directory / "shared.conf";
  const auto original_text = ReadTextFile(original_path);
  WriteTextFile(shared_path,
                original_text
                    + "\nStorage {\n"
                      "  Name = \"OtherStorage\"\n"
                      "}\n");
  std::filesystem::remove(original_path);

  auto updated = state.UpsertStorageDaemonResource(
      "prod", "bareos-sd",
      {.description = std::string{"Managed storage daemon"}});
  ASSERT_TRUE(updated) << updated.error;
  EXPECT_FALSE(std::filesystem::exists(original_path));
  EXPECT_TRUE(std::filesystem::exists(shared_path));
  const auto shared_text = ReadTextFile(shared_path);
  EXPECT_NE(shared_text.find("Description = \"Managed storage daemon\""),
            std::string::npos);
  EXPECT_NE(shared_text.find("Name = \"OtherStorage\""), std::string::npos);
}

TEST(BconfigService, UpsertsStorageDaemonResourcesWithMultipleSourceAddresses)
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
  std::filesystem::copy(source_fixture_root / "bareos-sd.d",
                        source_root.path() / "bareos-sd.d",
                        std::filesystem::copy_options::recursive);
  WriteTextFile(source_root.path() / "bareos-sd.d/storage/bareos-sd.conf",
                "Storage {\n"
                "  Name = \"bareos-sd\"\n"
                "}\n");

  auto import_job
      = state.CreateJob({.type = "import_configuration",
                         .deployment_id = std::string{"prod"},
                         .source_path = source_root.path().string()});
  ASSERT_TRUE(import_job);
  auto imported = WaitForJobTerminal(state, import_job.value->id);
  ASSERT_TRUE(imported.has_value());
  ASSERT_EQ(imported->status, JobStatus::kSucceeded);

  const auto storage_path
      = RepositoryLayout::StoragesDirectory(repo_path.path())
        / "bareos-sd/bareos-sd.d/storage/bareos-sd.conf";
  const auto original_text = ReadTextFile(storage_path);
  const auto closing = original_text.rfind("}\n");
  ASSERT_NE(closing, std::string::npos);
  WriteTextFile(storage_path,
                original_text.substr(0, closing)
                    + "  SourceAddress = 192.0.2.20\n"
                      "  SourceAddress = 198.51.100.20\n"
                      "}\n");

  auto updated = state.UpsertStorageDaemonResource(
      "prod", "bareos-sd",
      {.description = std::string{"Updated storage daemon"}});
  ASSERT_TRUE(updated) << updated.error;

  const auto updated_text = ReadTextFile(storage_path);
  EXPECT_NE(updated_text.find("Description = \"Updated storage daemon\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("SourceAddress = 192.0.2.20"), std::string::npos);
  EXPECT_NE(updated_text.find("SourceAddress = 198.51.100.20"),
            std::string::npos);
}

TEST(BconfigService, UpsertsStorageDaemonResourcesWithScalarNdmpAddress)
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
  std::filesystem::copy(source_fixture_root / "bareos-sd.d",
                        source_root.path() / "bareos-sd.d",
                        std::filesystem::copy_options::recursive);
  WriteTextFile(source_root.path() / "bareos-sd.d/storage/bareos-sd.conf",
                "Storage {\n"
                "  Name = \"bareos-sd\"\n"
                "  NdmpAddress = 192.0.2.30\n"
                "  NdmpPort = 10001\n"
                "  Description = \"Imported storage daemon\"\n"
                "}\n");

  auto import_job
      = state.CreateJob({.type = "import_configuration",
                         .deployment_id = std::string{"prod"},
                         .source_path = source_root.path().string()});
  ASSERT_TRUE(import_job);
  auto imported = WaitForJobTerminal(state, import_job.value->id);
  ASSERT_TRUE(imported.has_value());
  ASSERT_EQ(imported->status, JobStatus::kSucceeded);

  auto updated = state.UpsertStorageDaemonResource(
      "prod", "bareos-sd",
      {.description = std::string{"Updated storage daemon"}});
  ASSERT_TRUE(updated) << updated.error;

  const auto storage_path
      = updated.value->path / "bareos-sd.d/storage/bareos-sd.conf";
  const auto updated_text = ReadTextFile(storage_path);
  EXPECT_NE(updated_text.find("Description = \"Updated storage daemon\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("NdmpAddress = 192.0.2.30"), std::string::npos);
  EXPECT_NE(updated_text.find("NdmpPort = 10001"), std::string::npos);
}

TEST(BconfigService, UpsertsStorageDirectorResources)
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
  std::filesystem::copy(source_fixture_root / "bareos-sd.d",
                        source_root.path() / "bareos-sd.d",
                        std::filesystem::copy_options::recursive);
  WriteTextFile(source_root.path() / "bareos-sd.d/storage/bareos-sd.conf",
                "Storage {\n"
                "  Name = \"bareos-sd\"\n"
                "}\n");

  auto import_job
      = state.CreateJob({.type = "import_configuration",
                         .deployment_id = std::string{"prod"},
                         .source_path = source_root.path().string()});
  ASSERT_TRUE(import_job);
  auto imported = WaitForJobTerminal(state, import_job.value->id);
  ASSERT_TRUE(imported.has_value());
  ASSERT_EQ(imported->status, JobStatus::kSucceeded);

  auto created = state.UpsertStorageDirectorResource(
      "prod", "bareos-sd", "ManagedDirector",
      {.password = std::string{"[md5]0123456789abcdef0123456789abcdef"},
       .description = std::string{"Managed storage director"},
       .monitor = true,
       .maximum_bandwidth_per_job = 2048,
       .key_encryption_key = std::string{"managed-key"},
       .tls_enable = true,
       .tls_require = true,
       .tls_cipher_list = std::string{"HIGH"},
       .tls_allowed_cn = std::vector<std::string>{"bareos-dir"}});
  ASSERT_TRUE(created) << created.error;
  EXPECT_EQ(created.value->name, "bareos-sd");

  const auto director_path
      = created.value->path / "bareos-sd.d/director/ManagedDirector.conf";
  const auto created_text = ReadTextFile(director_path);
  EXPECT_NE(created_text.find("Director {"), std::string::npos);
  EXPECT_NE(created_text.find("Name = \"ManagedDirector\""), std::string::npos);
  EXPECT_NE(created_text.find("Description = \"Managed storage director\""),
            std::string::npos);
  EXPECT_NE(
      created_text.find("Password = \"[md5]0123456789abcdef0123456789abcdef\""),
      std::string::npos);
  EXPECT_NE(created_text.find("Monitor = yes"), std::string::npos);
  EXPECT_NE(created_text.find("MaximumBandwidthPerJob = 2048"),
            std::string::npos);
  EXPECT_NE(created_text.find("KeyEncryptionKey = \"managed-key\""),
            std::string::npos);
  EXPECT_NE(created_text.find("TlsEnable = yes"), std::string::npos);
  EXPECT_NE(created_text.find("TlsRequire = yes"), std::string::npos);
  EXPECT_NE(created_text.find("TlsCipherList = \"HIGH\""), std::string::npos);
  EXPECT_NE(created_text.find("TlsAllowedCn = \"bareos-dir\""),
            std::string::npos);

  const auto ownership_path = RepositoryLayout::OwnershipPath(repo_path.path());
  const auto ownership_text = ReadTextFile(ownership_path);
  EXPECT_NE(ownership_text.find(
                "storages/bareos-sd/bareos-sd.d/director/ManagedDirector.conf"),
            std::string::npos);

  auto updated = state.UpsertStorageDirectorResource(
      "prod", "bareos-sd", "ManagedDirector",
      {.password = std::string{"[md5]abcdef0123456789abcdef0123456789"},
       .description = std::string{"Updated storage director"},
       .monitor = false,
       .maximum_bandwidth_per_job = 4096,
       .key_encryption_key = std::string{"updated-key"},
       .tls_enable = false,
       .tls_protocol = std::string{"TLSv1.2"}});
  ASSERT_TRUE(updated) << updated.error;

  const auto updated_text = ReadTextFile(
      updated.value->path / "bareos-sd.d/director/ManagedDirector.conf");
  EXPECT_NE(updated_text.find("Description = \"Updated storage director\""),
            std::string::npos);
  EXPECT_NE(
      updated_text.find("Password = \"[md5]abcdef0123456789abcdef0123456789\""),
      std::string::npos);
  EXPECT_NE(updated_text.find("Monitor = no"), std::string::npos);
  EXPECT_NE(updated_text.find("MaximumBandwidthPerJob = 4096"),
            std::string::npos);
  EXPECT_NE(updated_text.find("KeyEncryptionKey = \"updated-key\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("TlsEnable = no"), std::string::npos);
  EXPECT_NE(updated_text.find("TlsRequire = yes"), std::string::npos);
  EXPECT_NE(updated_text.find("TlsCipherList = \"HIGH\""), std::string::npos);
  EXPECT_NE(updated_text.find("TlsProtocol = \"TLSv1.2\""), std::string::npos);
  EXPECT_NE(updated_text.find("TlsAllowedCn = \"bareos-dir\""),
            std::string::npos);

  auto current = state.GetStorageDirectorResourceSpec("prod", "bareos-sd",
                                                      "ManagedDirector");
  ASSERT_TRUE(current) << current.error;
  EXPECT_EQ(current.value->password, "[md5]abcdef0123456789abcdef0123456789");
  EXPECT_EQ(current.value->description, "Updated storage director");
  EXPECT_EQ(current.value->monitor, false);
  EXPECT_EQ(current.value->maximum_bandwidth_per_job, 4096);
  EXPECT_EQ(current.value->key_encryption_key, "updated-key");
  EXPECT_EQ(current.value->tls_enable, false);
  EXPECT_EQ(current.value->tls_require, true);
  EXPECT_EQ(current.value->tls_cipher_list, "HIGH");
  EXPECT_EQ(current.value->tls_protocol, "TLSv1.2");
  ASSERT_TRUE(current.value->tls_allowed_cn.has_value());
  EXPECT_EQ(*current.value->tls_allowed_cn,
            (std::vector<std::string>{"bareos-dir"}));

  const auto updated_ownership_text = ReadTextFile(ownership_path);
  EXPECT_NE(updated_ownership_text.find(
                "storages/bareos-sd/bareos-sd.d/director/ManagedDirector.conf"),
            std::string::npos);
}

TEST(BconfigService, UpsertsStorageDirectorResourcesInSharedFiles)
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
  std::filesystem::copy(source_fixture_root / "bareos-sd.d",
                        source_root.path() / "bareos-sd.d",
                        std::filesystem::copy_options::recursive);
  WriteTextFile(source_root.path() / "bareos-sd.d/storage/bareos-sd.conf",
                "Storage {\n"
                "  Name = \"bareos-sd\"\n"
                "}\n");

  auto import_job
      = state.CreateJob({.type = "import_configuration",
                         .deployment_id = std::string{"prod"},
                         .source_path = source_root.path().string()});
  ASSERT_TRUE(import_job);
  auto imported = WaitForJobTerminal(state, import_job.value->id);
  ASSERT_TRUE(imported.has_value());
  ASSERT_EQ(imported->status, JobStatus::kSucceeded);

  const auto director_directory
      = RepositoryLayout::StoragesDirectory(repo_path.path())
        / "bareos-sd/bareos-sd.d/director";
  const auto original_path = director_directory / "bareos-dir.conf";
  const auto shared_path = director_directory / "shared.conf";
  const auto original_text = ReadTextFile(original_path);
  WriteTextFile(shared_path,
                original_text
                    + "\nDirector {\n"
                      "  Name = \"OtherDirector\"\n"
                      "  Password = \"secret\"\n"
                      "}\n");
  std::filesystem::remove(original_path);

  auto updated = state.UpsertStorageDirectorResource(
      "prod", "bareos-sd", "bareos-dir",
      {.password = std::string{"[md5]abcdef0123456789abcdef0123456789"}});
  ASSERT_TRUE(updated) << updated.error;
  EXPECT_FALSE(std::filesystem::exists(original_path));
  EXPECT_TRUE(std::filesystem::exists(shared_path));
  const auto shared_text = ReadTextFile(shared_path);
  EXPECT_NE(
      shared_text.find("Password = \"[md5]abcdef0123456789abcdef0123456789\""),
      std::string::npos);
  EXPECT_NE(shared_text.find("Name = \"OtherDirector\""), std::string::npos);
}

TEST(BconfigService, DeletesStorageDirectorResources)
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
  std::filesystem::copy(source_fixture_root / "bareos-sd.d",
                        source_root.path() / "bareos-sd.d",
                        std::filesystem::copy_options::recursive);
  WriteTextFile(source_root.path() / "bareos-sd.d/storage/bareos-sd.conf",
                "Storage {\n"
                "  Name = \"bareos-sd\"\n"
                "}\n");

  auto import_job
      = state.CreateJob({.type = "import_configuration",
                         .deployment_id = std::string{"prod"},
                         .source_path = source_root.path().string()});
  ASSERT_TRUE(import_job);
  auto imported = WaitForJobTerminal(state, import_job.value->id);
  ASSERT_TRUE(imported.has_value());
  ASSERT_EQ(imported->status, JobStatus::kSucceeded);

  auto created = state.UpsertStorageDirectorResource(
      "prod", "bareos-sd", "ManagedDirector",
      {.password = std::string{"[md5]0123456789abcdef0123456789abcdef"}});
  ASSERT_TRUE(created) << created.error;

  const auto director_path
      = created.value->path / "bareos-sd.d/director/ManagedDirector.conf";
  ASSERT_TRUE(std::filesystem::exists(director_path));

  const auto ownership_path = RepositoryLayout::OwnershipPath(repo_path.path());
  const auto ownership_text = ReadTextFile(ownership_path);
  ASSERT_NE(ownership_text.find(
                "storages/bareos-sd/bareos-sd.d/director/ManagedDirector.conf"),
            std::string::npos);

  auto deleted = state.DeleteStorageDirectorResource("prod", "bareos-sd",
                                                     "ManagedDirector");
  ASSERT_TRUE(deleted) << deleted.error;
  EXPECT_FALSE(std::filesystem::exists(director_path));

  const auto updated_ownership_text = ReadTextFile(ownership_path);
  EXPECT_EQ(updated_ownership_text.find(
                "storages/bareos-sd/bareos-sd.d/director/ManagedDirector.conf"),
            std::string::npos);
}

TEST(BconfigService, DeletesStorageDirectorResourcesFromSharedFiles)
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
  std::filesystem::copy(source_fixture_root / "bareos-sd.d",
                        source_root.path() / "bareos-sd.d",
                        std::filesystem::copy_options::recursive);
  WriteTextFile(source_root.path() / "bareos-sd.d/storage/bareos-sd.conf",
                "Storage {\n"
                "  Name = \"bareos-sd\"\n"
                "}\n");

  auto import_job
      = state.CreateJob({.type = "import_configuration",
                         .deployment_id = std::string{"prod"},
                         .source_path = source_root.path().string()});
  ASSERT_TRUE(import_job);
  auto imported = WaitForJobTerminal(state, import_job.value->id);
  ASSERT_TRUE(imported.has_value());
  ASSERT_EQ(imported->status, JobStatus::kSucceeded);

  const auto director_directory
      = RepositoryLayout::StoragesDirectory(repo_path.path())
        / "bareos-sd/bareos-sd.d/director";
  const auto original_path = director_directory / "bareos-dir.conf";
  const auto shared_path = director_directory / "shared.conf";
  const auto original_text = ReadTextFile(original_path);
  WriteTextFile(shared_path,
                original_text
                    + "\nDirector {\n"
                      "  Name = \"OtherDirector\"\n"
                      "  Password = \"secret\"\n"
                      "}\n");
  std::filesystem::remove(original_path);

  auto deleted = state.DeleteStorageDirectorResource("prod", "bareos-sd",
                                                     "OtherDirector");
  ASSERT_TRUE(deleted) << deleted.error;
  EXPECT_FALSE(std::filesystem::exists(original_path));
  EXPECT_TRUE(std::filesystem::exists(shared_path));
  const auto shared_text = ReadTextFile(shared_path);
  EXPECT_NE(shared_text.find("Name = bareos-dir"), std::string::npos);
  EXPECT_EQ(shared_text.find("Name = \"OtherDirector\""), std::string::npos);
}

TEST(BconfigService, UpsertsStorageDeviceResources)
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
  std::filesystem::copy(source_fixture_root / "bareos-sd.d",
                        source_root.path() / "bareos-sd.d",
                        std::filesystem::copy_options::recursive);
  WriteTextFile(source_root.path() / "bareos-sd.d/storage/bareos-sd.conf",
                "Storage {\n"
                "  Name = \"bareos-sd\"\n"
                "}\n");

  auto import_job
      = state.CreateJob({.type = "import_configuration",
                         .deployment_id = std::string{"prod"},
                         .source_path = source_root.path().string()});
  ASSERT_TRUE(import_job);
  auto imported = WaitForJobTerminal(state, import_job.value->id);
  ASSERT_TRUE(imported.has_value());
  ASSERT_EQ(imported->status, JobStatus::kSucceeded);

  auto created = state.UpsertStorageDeviceResource(
      "prod", "bareos-sd", "ManagedDevice",
      {.media_type = std::string{"File"},
       .archive_device = std::string{"/tmp/managed-storage"},
       .device_type = std::string{"file"},
       .access_mode = std::string{"readonly"},
       .device_options = std::string{"Block Size = 64k"},
       .diagnostic_device = std::string{"/tmp/managed-storage.diag"},
       .hardware_end_of_file = false,
       .hardware_end_of_medium = false,
       .backward_space_record = false,
       .backward_space_file = false,
       .bsf_at_eom = true,
       .two_eof = true,
       .forward_space_record = false,
       .forward_space_file = false,
       .fast_forward_space_file = false,
       .removable_media = false,
       .random_access = true,
       .automatic_mount = true,
       .label_media = true,
       .always_open = false,
       .autochanger = true,
       .close_on_poll = true,
       .block_positioning = false,
       .use_mtiocget = false,
       .check_labels = true,
       .requires_mount = true,
       .offline_on_unmount = true,
       .block_checksum = false,
       .auto_select = false,
       .changer_device = std::string{"/dev/sg0"},
       .changer_command = std::string{"/usr/lib/bareos/mtx-changer %c %o"},
       .alert_command = std::string{"/usr/lib/bareos/alert.sh"},
       .maximum_changer_wait = 301,
       .maximum_open_wait = 302,
       .maximum_open_volumes = 4,
       .maximum_network_buffer_size = 1048576,
       .volume_poll_interval = 303,
       .maximum_rewind_wait = 304,
       .label_block_size = 64513,
       .minimum_block_size = 4096,
       .maximum_block_size = 1048577,
       .maximum_file_size = 1000000001,
       .volume_capacity = 2000000002,
       .maximum_concurrent_jobs = 5,
       .spool_directory = std::string{"/var/spool/bareos"},
       .maximum_spool_size = 3000000003,
       .maximum_job_spool_size = 4000000004,
       .drive_index = 2,
       .mount_point = std::string{"/mnt/tape"},
       .mount_command = std::string{"/usr/bin/mount /mnt/tape"},
       .unmount_command = std::string{"/usr/bin/umount /mnt/tape"},
       .label_type = std::string{"ibm"},
       .no_rewind_on_close = false,
       .drive_tape_alert_enabled = true,
       .drive_crypto_enabled = true,
       .query_crypto_status = true,
       .auto_deflate = std::string{"writeonly"},
       .auto_deflate_algorithm = std::string{"lz4hc"},
       .auto_deflate_level = 7,
       .auto_inflate = std::string{"in"},
       .collect_statistics = false,
       .eof_on_error_is_eot = true,
       .count = 5,
       .description = std::string{"Managed storage device"}});
  ASSERT_TRUE(created) << created.error;
  EXPECT_EQ(created.value->name, "bareos-sd");

  const auto device_path
      = created.value->path / "bareos-sd.d/device/ManagedDevice.conf";
  const auto created_text = ReadTextFile(device_path);
  EXPECT_NE(created_text.find("Device {"), std::string::npos);
  EXPECT_NE(created_text.find("Name = \"ManagedDevice\""), std::string::npos);
  EXPECT_NE(created_text.find("Description = \"Managed storage device\""),
            std::string::npos);
  EXPECT_NE(created_text.find("Media Type = File"), std::string::npos);
  EXPECT_NE(created_text.find("Archive Device = /tmp/managed-storage"),
            std::string::npos);
  EXPECT_NE(created_text.find("Device Type = \"file\""), std::string::npos);
  EXPECT_NE(created_text.find("AccessMode = read"), std::string::npos);
  EXPECT_NE(created_text.find("DeviceOptions = \"Block Size = 64k\""),
            std::string::npos);
  EXPECT_NE(
      created_text.find("DiagnosticDevice = \"/tmp/managed-storage.diag\""),
      std::string::npos);
  EXPECT_NE(created_text.find("HardwareEndOfFile = no"), std::string::npos);
  EXPECT_NE(created_text.find("HardwareEndOfMedium = no"), std::string::npos);
  EXPECT_NE(created_text.find("BackwardSpaceRecord = no"), std::string::npos);
  EXPECT_NE(created_text.find("BackwardSpaceFile = no"), std::string::npos);
  EXPECT_NE(created_text.find("BsfAtEom = yes"), std::string::npos);
  EXPECT_NE(created_text.find("TwoEof = yes"), std::string::npos);
  EXPECT_NE(created_text.find("ForwardSpaceRecord = no"), std::string::npos);
  EXPECT_NE(created_text.find("ForwardSpaceFile = no"), std::string::npos);
  EXPECT_NE(created_text.find("FastForwardSpaceFile = no"), std::string::npos);
  EXPECT_NE(created_text.find("RemovableMedia = no"), std::string::npos);
  EXPECT_NE(created_text.find("RandomAccess = yes"), std::string::npos);
  EXPECT_NE(created_text.find("AutomaticMount = yes"), std::string::npos);
  EXPECT_NE(created_text.find("LabelMedia = yes"), std::string::npos);
  EXPECT_NE(created_text.find("AlwaysOpen = no"), std::string::npos);
  EXPECT_NE(created_text.find("Autochanger = yes"), std::string::npos);
  EXPECT_NE(created_text.find("CloseOnPoll = yes"), std::string::npos);
  EXPECT_NE(created_text.find("BlockPositioning = no"), std::string::npos);
  EXPECT_NE(created_text.find("UseMtiocget = no"), std::string::npos);
  EXPECT_NE(created_text.find("CheckLabels = yes"), std::string::npos);
  EXPECT_NE(created_text.find("RequiresMount = yes"), std::string::npos);
  EXPECT_NE(created_text.find("OfflineOnUnmount = yes"), std::string::npos);
  EXPECT_NE(created_text.find("BlockChecksum = no"), std::string::npos);
  EXPECT_NE(created_text.find("AutoSelect = no"), std::string::npos);
  EXPECT_NE(created_text.find("ChangerDevice = \"/dev/sg0\""),
            std::string::npos);
  EXPECT_NE(created_text.find(
                "ChangerCommand = \"/usr/lib/bareos/mtx-changer %c %o\""),
            std::string::npos);
  EXPECT_NE(created_text.find("AlertCommand = \"/usr/lib/bareos/alert.sh\""),
            std::string::npos);
  EXPECT_NE(created_text.find("MaximumChangerWait = 301"), std::string::npos);
  EXPECT_NE(created_text.find("MaximumOpenWait = 302"), std::string::npos);
  EXPECT_NE(created_text.find("MaximumOpenVolumes = 4"), std::string::npos);
  EXPECT_NE(created_text.find("MaximumNetworkBufferSize = 1048576"),
            std::string::npos);
  EXPECT_NE(created_text.find("VolumePollInterval = 303"), std::string::npos);
  EXPECT_NE(created_text.find("MaximumRewindWait = 304"), std::string::npos);
  EXPECT_NE(created_text.find("LabelBlockSize = 64513"), std::string::npos);
  EXPECT_NE(created_text.find("MinimumBlockSize = 4096"), std::string::npos);
  EXPECT_NE(created_text.find("MaximumBlockSize = 1048577"), std::string::npos);
  EXPECT_NE(created_text.find("MaximumFileSize = 1000000001"),
            std::string::npos);
  EXPECT_NE(created_text.find("VolumeCapacity = 2000000002"),
            std::string::npos);
  EXPECT_NE(created_text.find("MaximumConcurrentJobs = 5"), std::string::npos);
  EXPECT_NE(created_text.find("SpoolDirectory = \"/var/spool/bareos\""),
            std::string::npos);
  EXPECT_NE(created_text.find("MaximumSpoolSize = 3000000003"),
            std::string::npos);
  EXPECT_NE(created_text.find("MaximumJobSpoolSize = 4000000004"),
            std::string::npos);
  EXPECT_NE(created_text.find("DriveIndex = 2"), std::string::npos);
  EXPECT_NE(created_text.find("MountPoint = \"/mnt/tape\""), std::string::npos);
  EXPECT_NE(created_text.find("MountCommand = \"/usr/bin/mount /mnt/tape\""),
            std::string::npos);
  EXPECT_NE(created_text.find("UnmountCommand = \"/usr/bin/umount /mnt/tape\""),
            std::string::npos);
  EXPECT_NE(created_text.find("LabelType = ibm"), std::string::npos);
  EXPECT_NE(created_text.find("NoRewindOnClose = no"), std::string::npos);
  EXPECT_NE(created_text.find("DriveTapeAlertEnabled = yes"),
            std::string::npos);
  EXPECT_NE(created_text.find("DriveCryptoEnabled = yes"), std::string::npos);
  EXPECT_NE(created_text.find("QueryCryptoStatus = yes"), std::string::npos);
  EXPECT_NE(created_text.find("AutoDeflate = write"), std::string::npos);
  EXPECT_NE(created_text.find("AutoDeflateAlgorithm = lz4hc"),
            std::string::npos);
  EXPECT_NE(created_text.find("AutoDeflateLevel = 7"), std::string::npos);
  EXPECT_NE(created_text.find("AutoInflate = read"), std::string::npos);
  EXPECT_NE(created_text.find("CollectStatistics = no"), std::string::npos);
  EXPECT_NE(created_text.find("EofOnErrorIsEot = yes"), std::string::npos);
  EXPECT_NE(created_text.find("Count = 5"), std::string::npos);

  const auto ownership_path = RepositoryLayout::OwnershipPath(repo_path.path());
  const auto ownership_text = ReadTextFile(ownership_path);
  EXPECT_NE(ownership_text.find(
                "storages/bareos-sd/bareos-sd.d/device/ManagedDevice.conf"),
            std::string::npos);

  auto updated = state.UpsertStorageDeviceResource(
      "prod", "bareos-sd", "ManagedDevice",
      {.archive_device = std::string{"/tmp/updated-storage"},
       .description = std::string{"Updated storage device"}});
  ASSERT_TRUE(updated) << updated.error;

  const auto updated_text = ReadTextFile(
      updated.value->path / "bareos-sd.d/device/ManagedDevice.conf");
  EXPECT_NE(updated_text.find("Description = \"Updated storage device\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("Media Type = File"), std::string::npos);
  EXPECT_NE(updated_text.find("Archive Device = /tmp/updated-storage"),
            std::string::npos);
  EXPECT_NE(updated_text.find("Device Type = \"file\""), std::string::npos);
  EXPECT_NE(updated_text.find("AccessMode = read"), std::string::npos);
  EXPECT_NE(updated_text.find("DeviceOptions = \"Block Size = 64k\""),
            std::string::npos);
  EXPECT_NE(
      updated_text.find("DiagnosticDevice = \"/tmp/managed-storage.diag\""),
      std::string::npos);
  EXPECT_NE(updated_text.find("HardwareEndOfFile = no"), std::string::npos);
  EXPECT_NE(updated_text.find("HardwareEndOfMedium = no"), std::string::npos);
  EXPECT_NE(updated_text.find("BackwardSpaceRecord = no"), std::string::npos);
  EXPECT_NE(updated_text.find("BackwardSpaceFile = no"), std::string::npos);
  EXPECT_NE(updated_text.find("BsfAtEom = yes"), std::string::npos);
  EXPECT_NE(updated_text.find("TwoEof = yes"), std::string::npos);
  EXPECT_NE(updated_text.find("ForwardSpaceRecord = no"), std::string::npos);
  EXPECT_NE(updated_text.find("ForwardSpaceFile = no"), std::string::npos);
  EXPECT_NE(updated_text.find("FastForwardSpaceFile = no"), std::string::npos);
  EXPECT_NE(updated_text.find("RemovableMedia = no"), std::string::npos);
  EXPECT_NE(updated_text.find("RandomAccess = yes"), std::string::npos);
  EXPECT_NE(updated_text.find("AutomaticMount = yes"), std::string::npos);
  EXPECT_NE(updated_text.find("LabelMedia = yes"), std::string::npos);
  EXPECT_NE(updated_text.find("AlwaysOpen = no"), std::string::npos);
  EXPECT_NE(updated_text.find("Autochanger = yes"), std::string::npos);
  EXPECT_NE(updated_text.find("CloseOnPoll = yes"), std::string::npos);
  EXPECT_NE(updated_text.find("BlockPositioning = no"), std::string::npos);
  EXPECT_NE(updated_text.find("UseMtiocget = no"), std::string::npos);
  EXPECT_NE(updated_text.find("CheckLabels = yes"), std::string::npos);
  EXPECT_NE(updated_text.find("RequiresMount = yes"), std::string::npos);
  EXPECT_NE(updated_text.find("OfflineOnUnmount = yes"), std::string::npos);
  EXPECT_NE(updated_text.find("BlockChecksum = no"), std::string::npos);
  EXPECT_NE(updated_text.find("AutoSelect = no"), std::string::npos);
  EXPECT_NE(updated_text.find("ChangerDevice = \"/dev/sg0\""),
            std::string::npos);
  EXPECT_NE(updated_text.find(
                "ChangerCommand = \"/usr/lib/bareos/mtx-changer %c %o\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("AlertCommand = \"/usr/lib/bareos/alert.sh\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("MaximumChangerWait = 301"), std::string::npos);
  EXPECT_NE(updated_text.find("MaximumOpenWait = 302"), std::string::npos);
  EXPECT_NE(updated_text.find("MaximumOpenVolumes = 4"), std::string::npos);
  EXPECT_NE(updated_text.find("MaximumNetworkBufferSize = 1048576"),
            std::string::npos);
  EXPECT_NE(updated_text.find("VolumePollInterval = 303"), std::string::npos);
  EXPECT_NE(updated_text.find("MaximumRewindWait = 304"), std::string::npos);
  EXPECT_NE(updated_text.find("LabelBlockSize = 64513"), std::string::npos);
  EXPECT_NE(updated_text.find("MinimumBlockSize = 4096"), std::string::npos);
  EXPECT_NE(updated_text.find("MaximumBlockSize = 1048577"), std::string::npos);
  EXPECT_NE(updated_text.find("MaximumFileSize = 1000000001"),
            std::string::npos);
  EXPECT_NE(updated_text.find("VolumeCapacity = 2000000002"),
            std::string::npos);
  EXPECT_NE(updated_text.find("MaximumConcurrentJobs = 5"), std::string::npos);
  EXPECT_NE(updated_text.find("SpoolDirectory = \"/var/spool/bareos\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("MaximumSpoolSize = 3000000003"),
            std::string::npos);
  EXPECT_NE(updated_text.find("MaximumJobSpoolSize = 4000000004"),
            std::string::npos);
  EXPECT_NE(updated_text.find("DriveIndex = 2"), std::string::npos);
  EXPECT_NE(updated_text.find("MountPoint = \"/mnt/tape\""), std::string::npos);
  EXPECT_NE(updated_text.find("MountCommand = \"/usr/bin/mount /mnt/tape\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("UnmountCommand = \"/usr/bin/umount /mnt/tape\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("LabelType = ibm"), std::string::npos);
  EXPECT_NE(updated_text.find("NoRewindOnClose = no"), std::string::npos);
  EXPECT_NE(updated_text.find("DriveTapeAlertEnabled = yes"),
            std::string::npos);
  EXPECT_NE(updated_text.find("DriveCryptoEnabled = yes"), std::string::npos);
  EXPECT_NE(updated_text.find("QueryCryptoStatus = yes"), std::string::npos);
  EXPECT_NE(updated_text.find("AutoDeflate = write"), std::string::npos);
  EXPECT_NE(updated_text.find("AutoDeflateAlgorithm = lz4hc"),
            std::string::npos);
  EXPECT_NE(updated_text.find("AutoDeflateLevel = 7"), std::string::npos);
  EXPECT_NE(updated_text.find("AutoInflate = read"), std::string::npos);
  EXPECT_NE(updated_text.find("CollectStatistics = no"), std::string::npos);
  EXPECT_NE(updated_text.find("EofOnErrorIsEot = yes"), std::string::npos);
  EXPECT_NE(updated_text.find("Count = 5"), std::string::npos);

  const auto updated_ownership_text = ReadTextFile(ownership_path);
  EXPECT_NE(updated_ownership_text.find(
                "storages/bareos-sd/bareos-sd.d/device/ManagedDevice.conf"),
            std::string::npos);
}

TEST(BconfigService, UpsertsStorageDeviceResourcesInSharedFiles)
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
  std::filesystem::copy(source_fixture_root / "bareos-sd.d",
                        source_root.path() / "bareos-sd.d",
                        std::filesystem::copy_options::recursive);
  WriteTextFile(source_root.path() / "bareos-sd.d/storage/bareos-sd.conf",
                "Storage {\n"
                "  Name = \"bareos-sd\"\n"
                "}\n");

  auto import_job
      = state.CreateJob({.type = "import_configuration",
                         .deployment_id = std::string{"prod"},
                         .source_path = source_root.path().string()});
  ASSERT_TRUE(import_job);
  auto imported = WaitForJobTerminal(state, import_job.value->id);
  ASSERT_TRUE(imported.has_value());
  ASSERT_EQ(imported->status, JobStatus::kSucceeded);

  const auto device_directory
      = RepositoryLayout::StoragesDirectory(repo_path.path())
        / "bareos-sd/bareos-sd.d/device";
  const auto original_path = device_directory / "FileStorage.conf";
  const auto shared_path = device_directory / "shared.conf";
  const auto original_text = ReadTextFile(original_path);
  auto shared_base_text = original_text;
  const auto original_brace = shared_base_text.rfind("}\n");
  ASSERT_NE(original_brace, std::string::npos);
  shared_base_text.insert(original_brace,
                          "  accessmode = writeonly\n"
                          "  hardwareendoffile = no\n"
                          "  randomaccess = yes\n"
                          "  autochanger = yes\n"
                          "  checklabels = yes\n"
                          "  maximumnetworkbuffersize = 2097152\n"
                          "  maximumrewindwait = 305\n"
                          "  autodeflate = both\n"
                          "  autodeflatealgorithm = lz4\n"
                          "  autodeflatelevel = 6\n"
                          "  autoinflate = readonly\n"
                          "  labeltype = ansi\n"
                          "  count = 5\n");
  WriteTextFile(shared_path,
                shared_base_text
                    + "\nDevice {\n"
                      "  Name = \"OtherDevice\"\n"
                      "  Media Type = File\n"
                      "  Archive Device = /tmp/other-storage\n"
                      "  Device Type = \"file\"\n"
                      "}\n");
  std::filesystem::remove(original_path);

  auto updated = state.UpsertStorageDeviceResource(
      "prod", "bareos-sd", "FileStorage",
      {.archive_device = std::string{"/tmp/updated-storage"}});
  ASSERT_TRUE(updated) << updated.error;
  EXPECT_FALSE(std::filesystem::exists(original_path));
  EXPECT_TRUE(std::filesystem::exists(shared_path));
  const auto shared_text = ReadTextFile(shared_path);
  EXPECT_NE(shared_text.find("Archive Device = /tmp/updated-storage"),
            std::string::npos);
  EXPECT_NE(shared_text.find("AccessMode = write"), std::string::npos);
  EXPECT_NE(shared_text.find("HardwareEndOfFile = no"), std::string::npos);
  EXPECT_NE(shared_text.find("RandomAccess = yes"), std::string::npos);
  EXPECT_NE(shared_text.find("Autochanger = yes"), std::string::npos);
  EXPECT_NE(shared_text.find("CheckLabels = yes"), std::string::npos);
  EXPECT_NE(shared_text.find("MaximumNetworkBufferSize = 2097152"),
            std::string::npos);
  EXPECT_NE(shared_text.find("MaximumRewindWait = 305"), std::string::npos);
  EXPECT_NE(shared_text.find("AutoDeflate = readwrite"), std::string::npos);
  EXPECT_NE(shared_text.find("AutoDeflateAlgorithm = lz4"), std::string::npos);
  EXPECT_NE(shared_text.find("AutoDeflateLevel = 6"), std::string::npos);
  EXPECT_NE(shared_text.find("AutoInflate = read"), std::string::npos);
  EXPECT_NE(shared_text.find("LabelType = ansi"), std::string::npos);
  EXPECT_NE(shared_text.find("Count = 5"), std::string::npos);
  EXPECT_NE(shared_text.find("Name = \"OtherDevice\""), std::string::npos);
}

TEST(BconfigService, DeletesStorageDeviceResources)
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
  std::filesystem::copy(source_fixture_root / "bareos-sd.d",
                        source_root.path() / "bareos-sd.d",
                        std::filesystem::copy_options::recursive);
  WriteTextFile(source_root.path() / "bareos-sd.d/storage/bareos-sd.conf",
                "Storage {\n"
                "  Name = \"bareos-sd\"\n"
                "}\n");

  auto import_job
      = state.CreateJob({.type = "import_configuration",
                         .deployment_id = std::string{"prod"},
                         .source_path = source_root.path().string()});
  ASSERT_TRUE(import_job);
  auto imported = WaitForJobTerminal(state, import_job.value->id);
  ASSERT_TRUE(imported.has_value());
  ASSERT_EQ(imported->status, JobStatus::kSucceeded);

  auto created = state.UpsertStorageDeviceResource(
      "prod", "bareos-sd", "ManagedDevice",
      {.media_type = std::string{"File"},
       .archive_device = std::string{"/tmp/managed-storage"},
       .device_type = std::string{"file"}});
  ASSERT_TRUE(created) << created.error;

  const auto device_path
      = created.value->path / "bareos-sd.d/device/ManagedDevice.conf";
  ASSERT_TRUE(std::filesystem::exists(device_path));

  const auto ownership_path = RepositoryLayout::OwnershipPath(repo_path.path());
  const auto ownership_text = ReadTextFile(ownership_path);
  ASSERT_NE(ownership_text.find(
                "storages/bareos-sd/bareos-sd.d/device/ManagedDevice.conf"),
            std::string::npos);

  auto deleted
      = state.DeleteStorageDeviceResource("prod", "bareos-sd", "ManagedDevice");
  ASSERT_TRUE(deleted) << deleted.error;
  EXPECT_FALSE(std::filesystem::exists(device_path));

  const auto updated_ownership_text = ReadTextFile(ownership_path);
  EXPECT_EQ(updated_ownership_text.find(
                "storages/bareos-sd/bareos-sd.d/device/ManagedDevice.conf"),
            std::string::npos);
}

TEST(BconfigService, DeletesStorageDeviceResourcesFromSharedFiles)
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
  std::filesystem::copy(source_fixture_root / "bareos-sd.d",
                        source_root.path() / "bareos-sd.d",
                        std::filesystem::copy_options::recursive);
  WriteTextFile(source_root.path() / "bareos-sd.d/storage/bareos-sd.conf",
                "Storage {\n"
                "  Name = \"bareos-sd\"\n"
                "}\n");

  auto import_job
      = state.CreateJob({.type = "import_configuration",
                         .deployment_id = std::string{"prod"},
                         .source_path = source_root.path().string()});
  ASSERT_TRUE(import_job);
  auto imported = WaitForJobTerminal(state, import_job.value->id);
  ASSERT_TRUE(imported.has_value());
  ASSERT_EQ(imported->status, JobStatus::kSucceeded);

  const auto device_directory
      = RepositoryLayout::StoragesDirectory(repo_path.path())
        / "bareos-sd/bareos-sd.d/device";
  const auto original_path = device_directory / "FileStorage.conf";
  const auto shared_path = device_directory / "shared.conf";
  const auto original_text = ReadTextFile(original_path);
  WriteTextFile(shared_path,
                original_text
                    + "\nDevice {\n"
                      "  Name = \"OtherDevice\"\n"
                      "  Media Type = File\n"
                      "  Archive Device = /tmp/other-storage\n"
                      "  Device Type = \"file\"\n"
                      "}\n");
  std::filesystem::remove(original_path);

  auto deleted
      = state.DeleteStorageDeviceResource("prod", "bareos-sd", "OtherDevice");
  ASSERT_TRUE(deleted) << deleted.error;
  EXPECT_FALSE(std::filesystem::exists(original_path));
  EXPECT_TRUE(std::filesystem::exists(shared_path));
  const auto shared_text = ReadTextFile(shared_path);
  EXPECT_NE(shared_text.find("Name = FileStorage"), std::string::npos);
  EXPECT_EQ(shared_text.find("Name = \"OtherDevice\""), std::string::npos);
}

TEST(BconfigService, UpsertsStorageNdmpResources)
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
  std::filesystem::copy(source_fixture_root / "bareos-sd.d",
                        source_root.path() / "bareos-sd.d",
                        std::filesystem::copy_options::recursive);
  std::filesystem::create_directories(source_root.path() / "bareos-sd.d/ndmp");
  WriteTextFile(source_root.path() / "bareos-sd.d/storage/bareos-sd.conf",
                "Storage {\n"
                "  Name = \"bareos-sd\"\n"
                "}\n");
  WriteTextFile(source_root.path() / "bareos-sd.d/ndmp/DefaultNdmp.conf",
                "Ndmp {\n"
                "  Name = \"DefaultNdmp\"\n"
                "  Username = \"imported-user\"\n"
                "  Password = \"imported-password\"\n"
                "  AuthType = Clear\n"
                "  LogLevel = 7\n"
                "}\n");

  auto import_job
      = state.CreateJob({.type = "import_configuration",
                         .deployment_id = std::string{"prod"},
                         .source_path = source_root.path().string()});
  ASSERT_TRUE(import_job);
  auto imported = WaitForJobTerminal(state, import_job.value->id);
  ASSERT_TRUE(imported.has_value());
  ASSERT_EQ(imported->status, JobStatus::kSucceeded);

  auto created = state.UpsertStorageNdmpResource(
      "prod", "bareos-sd", "ManagedNdmp",
      {.description = std::string{"Managed NDMP"},
       .username = std::string{"managed-user"},
       .password = std::string{"[md5]0123456789abcdef0123456789abcdef"},
       .auth_type = std::string{"MD5"},
       .log_level = 9});
  ASSERT_TRUE(created) << created.error;
  EXPECT_EQ(created.value->name, "bareos-sd");

  const auto ndmp_path
      = created.value->path / "bareos-sd.d/ndmp/ManagedNdmp.conf";
  const auto created_text = ReadTextFile(ndmp_path);
  EXPECT_NE(created_text.find("Ndmp {"), std::string::npos);
  EXPECT_NE(created_text.find("Name = \"ManagedNdmp\""), std::string::npos);
  EXPECT_NE(created_text.find("Description = \"Managed NDMP\""),
            std::string::npos);
  EXPECT_NE(created_text.find("Username = \"managed-user\""),
            std::string::npos);
  EXPECT_NE(
      created_text.find("Password = \"[md5]0123456789abcdef0123456789abcdef\""),
      std::string::npos);
  EXPECT_NE(created_text.find("AuthType = MD5"), std::string::npos);
  EXPECT_NE(created_text.find("LogLevel = 9"), std::string::npos);

  const auto ownership_path = RepositoryLayout::OwnershipPath(repo_path.path());
  const auto ownership_text = ReadTextFile(ownership_path);
  EXPECT_NE(ownership_text.find(
                "storages/bareos-sd/bareos-sd.d/ndmp/ManagedNdmp.conf"),
            std::string::npos);

  auto updated = state.UpsertStorageNdmpResource(
      "prod", "bareos-sd", "DefaultNdmp",
      {.description = std::string{"Updated NDMP"},
       .username = std::string{"updated-user"},
       .password = std::string{"updated-password"},
       .auth_type = std::string{"None"},
       .log_level = 3});
  ASSERT_TRUE(updated) << updated.error;

  const auto updated_text
      = ReadTextFile(updated.value->path / "bareos-sd.d/ndmp/DefaultNdmp.conf");
  EXPECT_NE(updated_text.find("Username = \"updated-user\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("Description = \"Updated NDMP\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("Password = \"updated-password\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("AuthType = None"), std::string::npos);
  EXPECT_NE(updated_text.find("LogLevel = 3"), std::string::npos);

  const auto updated_ownership_text = ReadTextFile(ownership_path);
  EXPECT_NE(updated_ownership_text.find(
                "storages/bareos-sd/bareos-sd.d/ndmp/ManagedNdmp.conf"),
            std::string::npos);
  EXPECT_NE(updated_ownership_text.find(
                "storages/bareos-sd/bareos-sd.d/ndmp/DefaultNdmp.conf"),
            std::string::npos);
}

TEST(BconfigService, UpsertsStorageNdmpResourcesInSharedFiles)
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
  std::filesystem::copy(source_fixture_root / "bareos-sd.d",
                        source_root.path() / "bareos-sd.d",
                        std::filesystem::copy_options::recursive);
  std::filesystem::create_directories(source_root.path() / "bareos-sd.d/ndmp");
  WriteTextFile(source_root.path() / "bareos-sd.d/storage/bareos-sd.conf",
                "Storage {\n"
                "  Name = \"bareos-sd\"\n"
                "}\n");
  WriteTextFile(source_root.path() / "bareos-sd.d/ndmp/DefaultNdmp.conf",
                "Ndmp {\n"
                "  Name = \"DefaultNdmp\"\n"
                "  Username = \"imported-user\"\n"
                "  Password = \"imported-password\"\n"
                "}\n");

  auto import_job
      = state.CreateJob({.type = "import_configuration",
                         .deployment_id = std::string{"prod"},
                         .source_path = source_root.path().string()});
  ASSERT_TRUE(import_job);
  auto imported = WaitForJobTerminal(state, import_job.value->id);
  ASSERT_TRUE(imported.has_value());
  ASSERT_EQ(imported->status, JobStatus::kSucceeded);

  const auto ndmp_directory
      = RepositoryLayout::StoragesDirectory(repo_path.path())
        / "bareos-sd/bareos-sd.d/ndmp";
  const auto original_path = ndmp_directory / "DefaultNdmp.conf";
  const auto shared_path = ndmp_directory / "shared.conf";
  const auto original_text = ReadTextFile(original_path);
  WriteTextFile(shared_path,
                original_text
                    + "\nNdmp {\n"
                      "  Name = \"OtherNdmp\"\n"
                      "  Username = \"other-user\"\n"
                      "  Password = \"other-password\"\n"
                      "}\n");
  std::filesystem::remove(original_path);

  auto updated = state.UpsertStorageNdmpResource(
      "prod", "bareos-sd", "DefaultNdmp",
      {.username = std::string{"updated-user"}});
  ASSERT_TRUE(updated) << updated.error;
  EXPECT_FALSE(std::filesystem::exists(original_path));
  EXPECT_TRUE(std::filesystem::exists(shared_path));
  const auto shared_text = ReadTextFile(shared_path);
  EXPECT_NE(shared_text.find("Username = \"updated-user\""), std::string::npos);
  EXPECT_NE(shared_text.find("Name = \"OtherNdmp\""), std::string::npos);
}

TEST(BconfigService, DeletesStorageNdmpResources)
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
  std::filesystem::copy(source_fixture_root / "bareos-sd.d",
                        source_root.path() / "bareos-sd.d",
                        std::filesystem::copy_options::recursive);
  WriteTextFile(source_root.path() / "bareos-sd.d/storage/bareos-sd.conf",
                "Storage {\n"
                "  Name = \"bareos-sd\"\n"
                "}\n");

  auto import_job
      = state.CreateJob({.type = "import_configuration",
                         .deployment_id = std::string{"prod"},
                         .source_path = source_root.path().string()});
  ASSERT_TRUE(import_job);
  auto imported = WaitForJobTerminal(state, import_job.value->id);
  ASSERT_TRUE(imported.has_value());
  ASSERT_EQ(imported->status, JobStatus::kSucceeded);

  auto created = state.UpsertStorageNdmpResource(
      "prod", "bareos-sd", "ManagedNdmp",
      {.username = std::string{"managed-user"},
       .password = std::string{"managed-password"}});
  ASSERT_TRUE(created) << created.error;

  const auto ndmp_path
      = created.value->path / "bareos-sd.d/ndmp/ManagedNdmp.conf";
  ASSERT_TRUE(std::filesystem::exists(ndmp_path));

  const auto ownership_path = RepositoryLayout::OwnershipPath(repo_path.path());
  const auto ownership_text = ReadTextFile(ownership_path);
  ASSERT_NE(ownership_text.find(
                "storages/bareos-sd/bareos-sd.d/ndmp/ManagedNdmp.conf"),
            std::string::npos);

  auto deleted
      = state.DeleteStorageNdmpResource("prod", "bareos-sd", "ManagedNdmp");
  ASSERT_TRUE(deleted) << deleted.error;
  EXPECT_FALSE(std::filesystem::exists(ndmp_path));

  const auto updated_ownership_text = ReadTextFile(ownership_path);
  EXPECT_EQ(updated_ownership_text.find(
                "storages/bareos-sd/bareos-sd.d/ndmp/ManagedNdmp.conf"),
            std::string::npos);
}

TEST(BconfigService, DeletesStorageNdmpResourcesFromSharedFiles)
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
  std::filesystem::copy(source_fixture_root / "bareos-sd.d",
                        source_root.path() / "bareos-sd.d",
                        std::filesystem::copy_options::recursive);
  std::filesystem::create_directories(source_root.path() / "bareos-sd.d/ndmp");
  WriteTextFile(source_root.path() / "bareos-sd.d/storage/bareos-sd.conf",
                "Storage {\n"
                "  Name = \"bareos-sd\"\n"
                "}\n");
  WriteTextFile(source_root.path() / "bareos-sd.d/ndmp/DefaultNdmp.conf",
                "Ndmp {\n"
                "  Name = \"DefaultNdmp\"\n"
                "  Username = \"imported-user\"\n"
                "  Password = \"imported-password\"\n"
                "}\n");

  auto import_job
      = state.CreateJob({.type = "import_configuration",
                         .deployment_id = std::string{"prod"},
                         .source_path = source_root.path().string()});
  ASSERT_TRUE(import_job);
  auto imported = WaitForJobTerminal(state, import_job.value->id);
  ASSERT_TRUE(imported.has_value());
  ASSERT_EQ(imported->status, JobStatus::kSucceeded);

  const auto ndmp_directory
      = RepositoryLayout::StoragesDirectory(repo_path.path())
        / "bareos-sd/bareos-sd.d/ndmp";
  const auto original_path = ndmp_directory / "DefaultNdmp.conf";
  const auto shared_path = ndmp_directory / "shared.conf";
  const auto original_text = ReadTextFile(original_path);
  WriteTextFile(shared_path,
                original_text
                    + "\nNdmp {\n"
                      "  Name = \"OtherNdmp\"\n"
                      "  Username = \"other-user\"\n"
                      "  Password = \"other-password\"\n"
                      "}\n");
  std::filesystem::remove(original_path);

  auto deleted
      = state.DeleteStorageNdmpResource("prod", "bareos-sd", "OtherNdmp");
  ASSERT_TRUE(deleted) << deleted.error;
  EXPECT_FALSE(std::filesystem::exists(original_path));
  EXPECT_TRUE(std::filesystem::exists(shared_path));
  const auto shared_text = ReadTextFile(shared_path);
  EXPECT_NE(shared_text.find("Username = \"imported-user\""),
            std::string::npos);
  EXPECT_EQ(shared_text.find("Name = \"OtherNdmp\""), std::string::npos);
}

TEST(BconfigService, UpsertsStorageAutochangerResources)
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
  std::filesystem::copy(source_fixture_root / "bareos-sd.d",
                        source_root.path() / "bareos-sd.d",
                        std::filesystem::copy_options::recursive);
  std::filesystem::create_directories(source_root.path()
                                      / "bareos-sd.d/autochanger");
  WriteTextFile(source_root.path() / "bareos-sd.d/storage/bareos-sd.conf",
                "Storage {\n"
                "  Name = \"bareos-sd\"\n"
                "}\n");
  WriteTextFile(source_root.path() / "bareos-sd.d/autochanger/Default.conf",
                "Autochanger {\n"
                "  Name = \"Default\"\n"
                "  Device = FileStorage\n"
                "  Changer Device = \"/dev/default-changer\"\n"
                "  Changer Command = \"/usr/lib/bareos/default-changer\"\n"
                "  Description = \"Imported autochanger\"\n"
                "}\n");

  auto import_job
      = state.CreateJob({.type = "import_configuration",
                         .deployment_id = std::string{"prod"},
                         .source_path = source_root.path().string()});
  ASSERT_TRUE(import_job);
  auto imported = WaitForJobTerminal(state, import_job.value->id);
  ASSERT_TRUE(imported.has_value());
  ASSERT_EQ(imported->status, JobStatus::kSucceeded);

  auto created = state.UpsertStorageAutochangerResource(
      "prod", "bareos-sd", "ManagedAutochanger",
      {.devices = std::vector<std::string>{"FileStorage"},
       .changer_device = std::string{"/dev/managed-changer"},
       .changer_command
       = std::string{"/usr/lib/bareos/mtx-changer %c %o %S %a %d"},
       .description = std::string{"Managed autochanger"}});
  ASSERT_TRUE(created) << created.error;

  const auto autochanger_path
      = created.value->path / "bareos-sd.d/autochanger/ManagedAutochanger.conf";
  const auto created_text = ReadTextFile(autochanger_path);
  EXPECT_NE(created_text.find("Autochanger {"), std::string::npos);
  EXPECT_NE(created_text.find("Name = \"ManagedAutochanger\""),
            std::string::npos);
  EXPECT_NE(created_text.find("Device = FileStorage"), std::string::npos);
  EXPECT_NE(created_text.find("Changer Device = \"/dev/managed-changer\""),
            std::string::npos);
  EXPECT_NE(
      created_text.find(
          "Changer Command = \"/usr/lib/bareos/mtx-changer %c %o %S %a %d\""),
      std::string::npos);
  EXPECT_NE(created_text.find("Description = \"Managed autochanger\""),
            std::string::npos);

  const auto ownership_path = RepositoryLayout::OwnershipPath(repo_path.path());
  const auto ownership_text = ReadTextFile(ownership_path);
  EXPECT_NE(
      ownership_text.find(
          "storages/bareos-sd/bareos-sd.d/autochanger/ManagedAutochanger.conf"),
      std::string::npos);

  auto updated = state.UpsertStorageAutochangerResource(
      "prod", "bareos-sd", "Default",
      {.devices = std::vector<std::string>{"FileStorage"},
       .changer_device = std::string{"/dev/updated-changer"},
       .changer_command = std::string{"/usr/lib/bareos/updated-changer"},
       .description = std::string{"Updated autochanger"}});
  ASSERT_TRUE(updated) << updated.error;

  const auto updated_text = ReadTextFile(
      updated.value->path / "bareos-sd.d/autochanger/Default.conf");
  EXPECT_NE(updated_text.find("Device = FileStorage"), std::string::npos);
  EXPECT_NE(updated_text.find("Changer Device = \"/dev/updated-changer\""),
            std::string::npos);
  EXPECT_NE(updated_text.find(
                "Changer Command = \"/usr/lib/bareos/updated-changer\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("Description = \"Updated autochanger\""),
            std::string::npos);
}

TEST(BconfigService, UpsertsStorageAutochangerResourcesInSharedFiles)
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
  std::filesystem::copy(source_fixture_root / "bareos-sd.d",
                        source_root.path() / "bareos-sd.d",
                        std::filesystem::copy_options::recursive);
  std::filesystem::create_directories(source_root.path()
                                      / "bareos-sd.d/autochanger");
  WriteTextFile(source_root.path() / "bareos-sd.d/storage/bareos-sd.conf",
                "Storage {\n"
                "  Name = \"bareos-sd\"\n"
                "}\n");
  WriteTextFile(source_root.path() / "bareos-sd.d/autochanger/Default.conf",
                "Autochanger {\n"
                "  Name = \"Default\"\n"
                "  Device = FileStorage\n"
                "  Changer Device = \"/dev/default-changer\"\n"
                "  Changer Command = \"/usr/lib/bareos/default-changer\"\n"
                "}\n");

  auto import_job
      = state.CreateJob({.type = "import_configuration",
                         .deployment_id = std::string{"prod"},
                         .source_path = source_root.path().string()});
  ASSERT_TRUE(import_job);
  auto imported = WaitForJobTerminal(state, import_job.value->id);
  ASSERT_TRUE(imported.has_value());
  ASSERT_EQ(imported->status, JobStatus::kSucceeded);

  const auto autochanger_directory
      = RepositoryLayout::StoragesDirectory(repo_path.path())
        / "bareos-sd/bareos-sd.d/autochanger";
  const auto original_path = autochanger_directory / "Default.conf";
  const auto shared_path = autochanger_directory / "shared.conf";
  const auto original_text = ReadTextFile(original_path);
  WriteTextFile(shared_path,
                original_text
                    + "\nAutochanger {\n"
                      "  Name = \"Other\"\n"
                      "  Device = FileStorage\n"
                      "}\n");
  std::filesystem::remove(original_path);

  auto updated = state.UpsertStorageAutochangerResource(
      "prod", "bareos-sd", "Default",
      {.devices = std::vector<std::string>{"FileStorage"}});
  ASSERT_TRUE(updated) << updated.error;
  EXPECT_FALSE(std::filesystem::exists(original_path));
  EXPECT_TRUE(std::filesystem::exists(shared_path));
  const auto shared_text = ReadTextFile(shared_path);
  EXPECT_NE(shared_text.find("Changer Device = \"/dev/default-changer\""),
            std::string::npos);
  EXPECT_NE(shared_text.find("Name = \"Other\""), std::string::npos);
}

TEST(BconfigService, UpsertsStorageAutochangerResourcesWithCountedDevices)
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
  std::filesystem::copy(source_fixture_root / "bareos-sd.d",
                        source_root.path() / "bareos-sd.d",
                        std::filesystem::copy_options::recursive);
  std::filesystem::create_directories(source_root.path()
                                      / "bareos-sd.d/autochanger");
  std::filesystem::create_directories(source_root.path()
                                      / "bareos-sd.d/device");
  WriteTextFile(source_root.path() / "bareos-sd.d/storage/bareos-sd.conf",
                "Storage {\n"
                "  Name = \"bareos-sd\"\n"
                "}\n");
  WriteTextFile(
      source_root.path() / "bareos-sd.d/autochanger/CountedAutochanger.conf",
      "Autochanger {\n"
      "  Name = \"CountedAutochanger\"\n"
      "  Device = MultipliedDeviceResource\n"
      "  Changer Device = \"/dev/counting-changer\"\n"
      "  Changer Command = \"/usr/lib/bareos/counting-changer\"\n"
      "  Description = \"Imported counted autochanger\"\n"
      "}\n");
  WriteTextFile(
      source_root.path() / "bareos-sd.d/device/MultipliedDeviceResource.conf",
      "Device {\n"
      "  Name = \"MultipliedDeviceResource\"\n"
      "  Media Type = File\n"
      "  Archive Device = \"/var/lib/bareos/storage/volumes\"\n"
      "  Device Type = File\n"
      "  LabelMedia = yes\n"
      "  Random Access = yes\n"
      "  AutomaticMount = yes\n"
      "  RemovableMedia = no\n"
      "  AlwaysOpen = no\n"
      "  Count = 5\n"
      "}\n");

  auto import_job
      = state.CreateJob({.type = "import_configuration",
                         .deployment_id = std::string{"prod"},
                         .source_path = source_root.path().string()});
  ASSERT_TRUE(import_job);
  auto imported = WaitForJobTerminal(state, import_job.value->id);
  ASSERT_TRUE(imported.has_value());
  ASSERT_EQ(imported->status, JobStatus::kSucceeded);

  auto updated = state.UpsertStorageAutochangerResource(
      "prod", "bareos-sd", "CountedAutochanger",
      {.description = std::string{"Updated counted autochanger"}});
  ASSERT_TRUE(updated) << updated.error;

  const auto updated_text = ReadTextFile(
      updated.value->path / "bareos-sd.d/autochanger/CountedAutochanger.conf");
  EXPECT_NE(updated_text.find("Device = MultipliedDeviceResource"),
            std::string::npos);
  EXPECT_NE(updated_text.find("Description = \"Updated counted autochanger\""),
            std::string::npos);
}

TEST(BconfigService, UpsertsSharedStorageAutochangerResourcesWithCountedDevices)
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
  std::filesystem::copy(source_fixture_root / "bareos-sd.d",
                        source_root.path() / "bareos-sd.d",
                        std::filesystem::copy_options::recursive);
  std::filesystem::create_directories(source_root.path()
                                      / "bareos-sd.d/autochanger");
  std::filesystem::create_directories(source_root.path()
                                      / "bareos-sd.d/device");
  WriteTextFile(source_root.path() / "bareos-sd.d/storage/bareos-sd.conf",
                "Storage {\n"
                "  Name = \"bareos-sd\"\n"
                "}\n");
  WriteTextFile(source_root.path() / "bareos-sd.d/autochanger/shared.conf",
                "Autochanger {\n"
                "  Name = \"CountedAutochanger\"\n"
                "  Device = MultipliedDeviceResource\n"
                "  Changer Device = \"/dev/counting-changer\"\n"
                "  Changer Command = \"/usr/lib/bareos/counting-changer\"\n"
                "  Description = \"Imported counted autochanger\"\n"
                "}\n"
                "\n"
                "Autochanger {\n"
                "  Name = \"Other\"\n"
                "  Device = FileStorage\n"
                "}\n");
  WriteTextFile(
      source_root.path() / "bareos-sd.d/device/MultipliedDeviceResource.conf",
      "Device {\n"
      "  Name = \"MultipliedDeviceResource\"\n"
      "  Media Type = File\n"
      "  Archive Device = \"/var/lib/bareos/storage/volumes\"\n"
      "  Device Type = File\n"
      "  LabelMedia = yes\n"
      "  Random Access = yes\n"
      "  AutomaticMount = yes\n"
      "  RemovableMedia = no\n"
      "  AlwaysOpen = no\n"
      "  Count = 5\n"
      "}\n");

  auto import_job
      = state.CreateJob({.type = "import_configuration",
                         .deployment_id = std::string{"prod"},
                         .source_path = source_root.path().string()});
  ASSERT_TRUE(import_job);
  auto imported = WaitForJobTerminal(state, import_job.value->id);
  ASSERT_TRUE(imported.has_value());
  ASSERT_EQ(imported->status, JobStatus::kSucceeded);

  auto updated = state.UpsertStorageAutochangerResource(
      "prod", "bareos-sd", "CountedAutochanger",
      {.description = std::string{"Updated counted autochanger"}});
  ASSERT_TRUE(updated) << updated.error;

  const auto updated_text = ReadTextFile(
      updated.value->path / "bareos-sd.d/autochanger/shared.conf");
  EXPECT_NE(updated_text.find("Device = MultipliedDeviceResource"),
            std::string::npos);
  EXPECT_NE(updated_text.find("Description = \"Updated counted autochanger\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("Name = \"Other\""), std::string::npos);
}

TEST(BconfigService, DeletesStorageAutochangerResources)
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
  std::filesystem::copy(source_fixture_root / "bareos-sd.d",
                        source_root.path() / "bareos-sd.d",
                        std::filesystem::copy_options::recursive);
  WriteTextFile(source_root.path() / "bareos-sd.d/storage/bareos-sd.conf",
                "Storage {\n"
                "  Name = \"bareos-sd\"\n"
                "}\n");

  auto import_job
      = state.CreateJob({.type = "import_configuration",
                         .deployment_id = std::string{"prod"},
                         .source_path = source_root.path().string()});
  ASSERT_TRUE(import_job);
  auto imported = WaitForJobTerminal(state, import_job.value->id);
  ASSERT_TRUE(imported.has_value());
  ASSERT_EQ(imported->status, JobStatus::kSucceeded);

  auto created = state.UpsertStorageAutochangerResource(
      "prod", "bareos-sd", "ManagedAutochanger",
      {.devices = std::vector<std::string>{"FileStorage"},
       .changer_device = std::string{"/dev/managed-changer"},
       .changer_command
       = std::string{"/usr/lib/bareos/mtx-changer %c %o %S %a %d"}});
  ASSERT_TRUE(created) << created.error;

  const auto autochanger_path
      = created.value->path / "bareos-sd.d/autochanger/ManagedAutochanger.conf";
  ASSERT_TRUE(std::filesystem::exists(autochanger_path));

  const auto ownership_path = RepositoryLayout::OwnershipPath(repo_path.path());
  const auto ownership_text = ReadTextFile(ownership_path);
  ASSERT_NE(
      ownership_text.find(
          "storages/bareos-sd/bareos-sd.d/autochanger/ManagedAutochanger.conf"),
      std::string::npos);

  auto deleted = state.DeleteStorageAutochangerResource("prod", "bareos-sd",
                                                        "ManagedAutochanger");
  ASSERT_TRUE(deleted) << deleted.error;
  EXPECT_FALSE(std::filesystem::exists(autochanger_path));

  const auto updated_ownership_text = ReadTextFile(ownership_path);
  EXPECT_EQ(
      updated_ownership_text.find(
          "storages/bareos-sd/bareos-sd.d/autochanger/ManagedAutochanger.conf"),
      std::string::npos);
}

TEST(BconfigService, DeletesStorageAutochangerResourcesFromSharedFiles)
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
  std::filesystem::copy(source_fixture_root / "bareos-sd.d",
                        source_root.path() / "bareos-sd.d",
                        std::filesystem::copy_options::recursive);
  std::filesystem::create_directories(source_root.path()
                                      / "bareos-sd.d/autochanger");
  WriteTextFile(source_root.path() / "bareos-sd.d/storage/bareos-sd.conf",
                "Storage {\n"
                "  Name = \"bareos-sd\"\n"
                "}\n");
  WriteTextFile(source_root.path() / "bareos-sd.d/autochanger/Default.conf",
                "Autochanger {\n"
                "  Name = \"Default\"\n"
                "  Device = FileStorage\n"
                "  Changer Device = \"/dev/default-changer\"\n"
                "  Changer Command = \"/usr/lib/bareos/default-changer\"\n"
                "}\n");

  auto import_job
      = state.CreateJob({.type = "import_configuration",
                         .deployment_id = std::string{"prod"},
                         .source_path = source_root.path().string()});
  ASSERT_TRUE(import_job);
  auto imported = WaitForJobTerminal(state, import_job.value->id);
  ASSERT_TRUE(imported.has_value());
  ASSERT_EQ(imported->status, JobStatus::kSucceeded);

  const auto autochanger_directory
      = RepositoryLayout::StoragesDirectory(repo_path.path())
        / "bareos-sd/bareos-sd.d/autochanger";
  const auto original_path = autochanger_directory / "Default.conf";
  const auto shared_path = autochanger_directory / "shared.conf";
  const auto original_text = ReadTextFile(original_path);
  WriteTextFile(shared_path,
                original_text
                    + "\nAutochanger {\n"
                      "  Name = \"Other\"\n"
                      "  Device = FileStorage\n"
                      "}\n");
  std::filesystem::remove(original_path);

  auto deleted
      = state.DeleteStorageAutochangerResource("prod", "bareos-sd", "Other");
  ASSERT_TRUE(deleted) << deleted.error;
  EXPECT_FALSE(std::filesystem::exists(original_path));
  EXPECT_TRUE(std::filesystem::exists(shared_path));
  const auto shared_text = ReadTextFile(shared_path);
  EXPECT_NE(shared_text.find("Device = FileStorage"), std::string::npos);
  EXPECT_EQ(shared_text.find("Name = \"Other\""), std::string::npos);
}

TEST(BconfigService, UpsertsStorageMessagesResourcesInSharedFiles)
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
  std::filesystem::copy(source_fixture_root / "bareos-sd.d",
                        source_root.path() / "bareos-sd.d",
                        std::filesystem::copy_options::recursive);
  WriteTextFile(source_root.path() / "bareos-sd.d/storage/bareos-sd.conf",
                "Storage {\n"
                "  Name = \"bareos-sd\"\n"
                "}\n");

  auto import_job
      = state.CreateJob({.type = "import_configuration",
                         .deployment_id = std::string{"prod"},
                         .source_path = source_root.path().string()});
  ASSERT_TRUE(import_job);
  auto imported = WaitForJobTerminal(state, import_job.value->id);
  ASSERT_TRUE(imported.has_value());
  ASSERT_EQ(imported->status, JobStatus::kSucceeded);

  const auto messages_directory
      = RepositoryLayout::StoragesDirectory(repo_path.path())
        / "bareos-sd/bareos-sd.d/messages";
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

  auto updated = state.UpsertStorageMessagesResource(
      "prod", "bareos-sd", "Standard",
      {.description = std::string{"Updated storage messages"}});
  ASSERT_TRUE(updated) << updated.error;
  EXPECT_FALSE(std::filesystem::exists(original_path));
  EXPECT_TRUE(std::filesystem::exists(shared_path));
  const auto shared_text = ReadTextFile(shared_path);
  EXPECT_NE(shared_text.find("Description = \"Updated storage messages\""),
            std::string::npos);
  EXPECT_NE(shared_text.find("Director = bareos-dir = all"), std::string::npos);
  EXPECT_NE(shared_text.find("Name = \"OtherMessages\""), std::string::npos);
}

TEST(BconfigService, DeletesStorageMessagesResources)
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
  std::filesystem::copy(source_fixture_root / "bareos-sd.d",
                        source_root.path() / "bareos-sd.d",
                        std::filesystem::copy_options::recursive);
  WriteTextFile(source_root.path() / "bareos-sd.d/storage/bareos-sd.conf",
                "Storage {\n"
                "  Name = \"bareos-sd\"\n"
                "}\n");

  auto import_job
      = state.CreateJob({.type = "import_configuration",
                         .deployment_id = std::string{"prod"},
                         .source_path = source_root.path().string()});
  ASSERT_TRUE(import_job);
  auto imported = WaitForJobTerminal(state, import_job.value->id);
  ASSERT_TRUE(imported.has_value());
  ASSERT_EQ(imported->status, JobStatus::kSucceeded);

  auto created = state.UpsertStorageMessagesResource(
      "prod", "bareos-sd", "ManagedMessages",
      {.entries = std::vector<std::string>{"  Director = bareos-dir = all"}});
  ASSERT_TRUE(created) << created.error;

  const auto messages_path
      = created.value->path / "bareos-sd.d/messages/ManagedMessages.conf";
  ASSERT_TRUE(std::filesystem::exists(messages_path));

  auto deleted = state.DeleteStorageMessagesResource("prod", "bareos-sd",
                                                     "ManagedMessages");
  ASSERT_TRUE(deleted) << deleted.error;
  EXPECT_FALSE(std::filesystem::exists(messages_path));
}

TEST(BconfigService, DeletesStorageMessagesResourcesFromSharedFiles)
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
  std::filesystem::copy(source_fixture_root / "bareos-sd.d",
                        source_root.path() / "bareos-sd.d",
                        std::filesystem::copy_options::recursive);
  WriteTextFile(source_root.path() / "bareos-sd.d/storage/bareos-sd.conf",
                "Storage {\n"
                "  Name = \"bareos-sd\"\n"
                "}\n");

  auto import_job
      = state.CreateJob({.type = "import_configuration",
                         .deployment_id = std::string{"prod"},
                         .source_path = source_root.path().string()});
  ASSERT_TRUE(import_job);
  auto imported = WaitForJobTerminal(state, import_job.value->id);
  ASSERT_TRUE(imported.has_value());
  ASSERT_EQ(imported->status, JobStatus::kSucceeded);

  const auto messages_directory
      = RepositoryLayout::StoragesDirectory(repo_path.path())
        / "bareos-sd/bareos-sd.d/messages";
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

  auto deleted = state.DeleteStorageMessagesResource("prod", "bareos-sd",
                                                     "OtherMessages");
  ASSERT_TRUE(deleted) << deleted.error;
  EXPECT_FALSE(std::filesystem::exists(original_path));
  EXPECT_TRUE(std::filesystem::exists(shared_path));
  const auto shared_text = ReadTextFile(shared_path);
  EXPECT_NE(shared_text.find("Director = bareos-dir = all"), std::string::npos);
  EXPECT_EQ(shared_text.find("Name = \"OtherMessages\""), std::string::npos);
}

}  // namespace
}  // namespace bconfig::service
