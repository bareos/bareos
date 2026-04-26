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
TEST(BconfigService, UpsertsDirectorClientResources)
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

  auto created = state.UpsertDirectorClientResource(
      "prod", "bareos-dir", "client1-fd",
      {.address = std::string{"client1-fd.example.com"},
       .lan_address = std::string{"client1-fd-lan.example.com"},
       .protocol = std::string{"NDMPV4"},
       .auth_type = std::string{"MD5"},
       .catalog = std::string{"MyCatalog"},
       .username = std::string{"managed-user"},
       .password = std::string{"[md5]0123456789abcdef0123456789abcdef"},
       .enabled = false,
       .passive = true,
       .strict_quotas = true,
       .quota_include_failed_jobs = false,
       .soft_quota = 4096,
       .hard_quota = 8192,
       .soft_quota_grace_period = 120,
       .file_retention = 3600,
       .job_retention = 7200,
       .ndmp_log_level = 7,
       .ndmp_block_size = 64512,
       .ndmp_use_lmdb = false,
       .auto_prune = true,
       .tls_authenticate = true,
       .tls_enable = true,
       .tls_require = true,
       .tls_verify_peer = true,
       .tls_cipher_list = std::string{"HIGH:!aNULL"},
       .tls_cipher_suites = std::string{"TLS_AES_256_GCM_SHA384"},
       .tls_dh_file = std::string{"/etc/bareos/dh4096.pem"},
       .tls_protocol = std::string{"TLSv1.3"},
       .tls_ca_certificate_file = std::string{"/etc/bareos/ca.pem"},
       .tls_ca_certificate_dir = std::string{"/etc/ssl/certs"},
       .tls_certificate_revocation_list = std::string{"/etc/bareos/crl.pem"},
       .tls_certificate = std::string{"/etc/bareos/client.crt"},
       .tls_key = std::string{"/etc/bareos/client.key"},
       .tls_allowed_cn = std::vector<std::string>{"backup-dir.example.test",
                                                  "backup-dir.internal"},
       .connection_from_director_to_client = false,
       .connection_from_client_to_director = true,
       .maximum_concurrent_jobs = 9,
       .heartbeat_interval = 60,
       .maximum_bandwidth_per_job = 2048,
       .description = std::string{"Managed by service"}});
  ASSERT_TRUE(created) << created.error;
  EXPECT_EQ(created.value->name, "bareos-dir");

  const auto director_client_path
      = created.value->path / "bareos-dir.d/client/client1-fd.conf";
  const auto created_text = ReadTextFile(director_client_path);
  EXPECT_NE(created_text.find("Name = \"client1-fd\""), std::string::npos);
  EXPECT_NE(created_text.find("Address = client1-fd.example.com"),
            std::string::npos);
  EXPECT_NE(created_text.find("LanAddress = client1-fd-lan.example.com"),
            std::string::npos);
  EXPECT_NE(created_text.find("Protocol = NDMPV4"), std::string::npos);
  EXPECT_NE(created_text.find("AuthType = MD5"), std::string::npos);
  EXPECT_NE(created_text.find("Catalog = MyCatalog"), std::string::npos);
  EXPECT_NE(created_text.find("Username = \"managed-user\""),
            std::string::npos);
  EXPECT_NE(
      created_text.find("Password = \"[md5]0123456789abcdef0123456789abcdef\""),
      std::string::npos);
  EXPECT_NE(created_text.find("Port = 9102"), std::string::npos);
  EXPECT_NE(created_text.find("Enabled = no"), std::string::npos);
  EXPECT_NE(created_text.find("Passive = yes"), std::string::npos);
  EXPECT_NE(created_text.find("StrictQuotas = yes"), std::string::npos);
  EXPECT_NE(created_text.find("QuotaIncludeFailedJobs = no"),
            std::string::npos);
  EXPECT_NE(created_text.find("SoftQuota = 4096"), std::string::npos);
  EXPECT_NE(created_text.find("HardQuota = 8192"), std::string::npos);
  EXPECT_NE(created_text.find("SoftQuotaGracePeriod = 120"), std::string::npos);
  EXPECT_NE(created_text.find("FileRetention = 3600"), std::string::npos);
  EXPECT_NE(created_text.find("JobRetention = 7200"), std::string::npos);
  EXPECT_NE(created_text.find("NdmpLogLevel = 7"), std::string::npos);
  EXPECT_NE(created_text.find("NdmpBlockSize = 64512"), std::string::npos);
  EXPECT_NE(created_text.find("NdmpUseLmdb = no"), std::string::npos);
  EXPECT_NE(created_text.find("AutoPrune = yes"), std::string::npos);
  EXPECT_NE(created_text.find("TlsAuthenticate = yes"), std::string::npos);
  EXPECT_NE(created_text.find("TlsEnable = yes"), std::string::npos);
  EXPECT_NE(created_text.find("TlsRequire = yes"), std::string::npos);
  EXPECT_NE(created_text.find("TlsVerifyPeer = yes"), std::string::npos);
  EXPECT_NE(created_text.find("TlsCipherList = \"HIGH:!aNULL\""),
            std::string::npos);
  EXPECT_NE(created_text.find("TlsCipherSuites = \"TLS_AES_256_GCM_SHA384\""),
            std::string::npos);
  EXPECT_NE(created_text.find("TlsDhFile = \"/etc/bareos/dh4096.pem\""),
            std::string::npos);
  EXPECT_NE(created_text.find("TlsProtocol = \"TLSv1.3\""), std::string::npos);
  EXPECT_NE(created_text.find("TlsCaCertificateFile = \"/etc/bareos/ca.pem\""),
            std::string::npos);
  EXPECT_NE(created_text.find("TlsCaCertificateDir = \"/etc/ssl/certs\""),
            std::string::npos);
  EXPECT_NE(created_text.find("TlsCertificateRevocationList = "
                              "\"/etc/bareos/crl.pem\""),
            std::string::npos);
  EXPECT_NE(created_text.find("TlsCertificate = \"/etc/bareos/client.crt\""),
            std::string::npos);
  EXPECT_NE(created_text.find("TlsKey = \"/etc/bareos/client.key\""),
            std::string::npos);
  EXPECT_NE(created_text.find("TlsAllowedCn = \"backup-dir.example.test\""),
            std::string::npos);
  EXPECT_NE(created_text.find("TlsAllowedCn = \"backup-dir.internal\""),
            std::string::npos);
  EXPECT_NE(created_text.find("ConnectionFromDirectorToClient = no"),
            std::string::npos);
  EXPECT_NE(created_text.find("ConnectionFromClientToDirector = yes"),
            std::string::npos);
  EXPECT_NE(created_text.find("MaximumConcurrentJobs = 9"), std::string::npos);
  EXPECT_NE(created_text.find("HeartbeatInterval = 60"), std::string::npos);
  EXPECT_NE(created_text.find("MaximumBandwidthPerJob = 2048"),
            std::string::npos);
  EXPECT_NE(created_text.find("Description = \"Managed by service\""),
            std::string::npos);

  const auto stub_path = RepositoryLayout::ClientsDirectory(repo_path.path())
                         / "client1-fd/bareos-fd.d/director/bareos-dir.conf";
  const auto stub_text = ReadTextFile(stub_path);
  EXPECT_NE(
      stub_text.find("Password = \"[md5]0123456789abcdef0123456789abcdef\""),
      std::string::npos);
  EXPECT_EQ(stub_text.find("LanAddress = client1-fd-lan.example.com"),
            std::string::npos);
  EXPECT_EQ(stub_text.find("Protocol = NDMPV4"), std::string::npos);
  EXPECT_EQ(stub_text.find("AuthType = MD5"), std::string::npos);
  EXPECT_EQ(stub_text.find("Catalog = MyCatalog"), std::string::npos);
  EXPECT_EQ(stub_text.find("Username = \"managed-user\""), std::string::npos);
  EXPECT_EQ(stub_text.find("Passive = yes"), std::string::npos);
  EXPECT_EQ(stub_text.find("StrictQuotas = yes"), std::string::npos);
  EXPECT_EQ(stub_text.find("QuotaIncludeFailedJobs = no"), std::string::npos);
  EXPECT_EQ(stub_text.find("SoftQuota = 4096"), std::string::npos);
  EXPECT_EQ(stub_text.find("HardQuota = 8192"), std::string::npos);
  EXPECT_EQ(stub_text.find("SoftQuotaGracePeriod = 120"), std::string::npos);
  EXPECT_EQ(stub_text.find("FileRetention = 3600"), std::string::npos);
  EXPECT_EQ(stub_text.find("JobRetention = 7200"), std::string::npos);
  EXPECT_EQ(stub_text.find("NdmpLogLevel = 7"), std::string::npos);
  EXPECT_EQ(stub_text.find("NdmpBlockSize = 64512"), std::string::npos);
  EXPECT_EQ(stub_text.find("NdmpUseLmdb = no"), std::string::npos);
  EXPECT_EQ(stub_text.find("AutoPrune = yes"), std::string::npos);
  EXPECT_EQ(stub_text.find("TlsAuthenticate = yes"), std::string::npos);
  EXPECT_EQ(stub_text.find("TlsEnable = yes"), std::string::npos);
  EXPECT_EQ(stub_text.find("TlsRequire = yes"), std::string::npos);
  EXPECT_EQ(stub_text.find("TlsVerifyPeer = yes"), std::string::npos);
  EXPECT_EQ(stub_text.find("TlsCipherList = \"HIGH:!aNULL\""),
            std::string::npos);
  EXPECT_EQ(stub_text.find("TlsCipherSuites = \"TLS_AES_256_GCM_SHA384\""),
            std::string::npos);
  EXPECT_EQ(stub_text.find("TlsDhFile = \"/etc/bareos/dh4096.pem\""),
            std::string::npos);
  EXPECT_EQ(stub_text.find("TlsProtocol = \"TLSv1.3\""), std::string::npos);
  EXPECT_EQ(stub_text.find("TlsCaCertificateFile = \"/etc/bareos/ca.pem\""),
            std::string::npos);
  EXPECT_EQ(stub_text.find("TlsCaCertificateDir = \"/etc/ssl/certs\""),
            std::string::npos);
  EXPECT_EQ(stub_text.find("TlsCertificateRevocationList = "
                           "\"/etc/bareos/crl.pem\""),
            std::string::npos);
  EXPECT_EQ(stub_text.find("TlsCertificate = \"/etc/bareos/client.crt\""),
            std::string::npos);
  EXPECT_EQ(stub_text.find("TlsKey = \"/etc/bareos/client.key\""),
            std::string::npos);
  EXPECT_EQ(stub_text.find("TlsAllowedCn = \"backup-dir.example.test\""),
            std::string::npos);
  EXPECT_EQ(stub_text.find("TlsAllowedCn = \"backup-dir.internal\""),
            std::string::npos);
  EXPECT_NE(stub_text.find("ConnectionFromDirectorToClient = no"),
            std::string::npos);
  EXPECT_NE(stub_text.find("ConnectionFromClientToDirector = yes"),
            std::string::npos);
  EXPECT_EQ(stub_text.find("MaximumConcurrentJobs = 9"), std::string::npos);
  EXPECT_NE(stub_text.find("MaximumBandwidthPerJob = 2048"), std::string::npos);

  auto updated = state.UpsertDirectorClientResource(
      "prod", "bareos-dir", "client1-fd",
      {.address = std::string{"client1-alt.example.com"}});
  ASSERT_TRUE(updated) << updated.error;

  const auto updated_text = ReadTextFile(director_client_path);
  EXPECT_NE(updated_text.find("Address = client1-alt.example.com"),
            std::string::npos);
  EXPECT_NE(updated_text.find("LanAddress = client1-fd-lan.example.com"),
            std::string::npos);
  EXPECT_NE(updated_text.find("Protocol = NDMPV4"), std::string::npos);
  EXPECT_NE(updated_text.find("AuthType = MD5"), std::string::npos);
  EXPECT_NE(updated_text.find("Catalog = MyCatalog"), std::string::npos);
  EXPECT_NE(updated_text.find("Username = \"managed-user\""),
            std::string::npos);
  EXPECT_NE(
      updated_text.find("Password = \"[md5]0123456789abcdef0123456789abcdef\""),
      std::string::npos);
  EXPECT_NE(updated_text.find("Port = 9102"), std::string::npos);
  EXPECT_NE(updated_text.find("Enabled = no"), std::string::npos);
  EXPECT_NE(updated_text.find("Passive = yes"), std::string::npos);
  EXPECT_NE(updated_text.find("StrictQuotas = yes"), std::string::npos);
  EXPECT_NE(updated_text.find("QuotaIncludeFailedJobs = no"),
            std::string::npos);
  EXPECT_NE(updated_text.find("SoftQuota = 4096"), std::string::npos);
  EXPECT_NE(updated_text.find("HardQuota = 8192"), std::string::npos);
  EXPECT_NE(updated_text.find("SoftQuotaGracePeriod = 120"), std::string::npos);
  EXPECT_NE(updated_text.find("FileRetention = 3600"), std::string::npos);
  EXPECT_NE(updated_text.find("JobRetention = 7200"), std::string::npos);
  EXPECT_NE(updated_text.find("NdmpLogLevel = 7"), std::string::npos);
  EXPECT_NE(updated_text.find("NdmpBlockSize = 64512"), std::string::npos);
  EXPECT_NE(updated_text.find("NdmpUseLmdb = no"), std::string::npos);
  EXPECT_NE(updated_text.find("AutoPrune = yes"), std::string::npos);
  EXPECT_NE(updated_text.find("TlsAuthenticate = yes"), std::string::npos);
  EXPECT_NE(updated_text.find("TlsEnable = yes"), std::string::npos);
  EXPECT_NE(updated_text.find("TlsRequire = yes"), std::string::npos);
  EXPECT_NE(updated_text.find("TlsVerifyPeer = yes"), std::string::npos);
  EXPECT_NE(updated_text.find("TlsCipherList = \"HIGH:!aNULL\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("TlsCipherSuites = \"TLS_AES_256_GCM_SHA384\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("TlsDhFile = \"/etc/bareos/dh4096.pem\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("TlsProtocol = \"TLSv1.3\""), std::string::npos);
  EXPECT_NE(updated_text.find("TlsCaCertificateFile = \"/etc/bareos/ca.pem\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("TlsCaCertificateDir = \"/etc/ssl/certs\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("TlsCertificateRevocationList = "
                              "\"/etc/bareos/crl.pem\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("TlsCertificate = \"/etc/bareos/client.crt\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("TlsKey = \"/etc/bareos/client.key\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("TlsAllowedCn = \"backup-dir.example.test\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("TlsAllowedCn = \"backup-dir.internal\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("ConnectionFromDirectorToClient = no"),
            std::string::npos);
  EXPECT_NE(updated_text.find("ConnectionFromClientToDirector = yes"),
            std::string::npos);
  EXPECT_NE(updated_text.find("MaximumConcurrentJobs = 9"), std::string::npos);
  EXPECT_NE(updated_text.find("HeartbeatInterval = 60"), std::string::npos);
  EXPECT_NE(updated_text.find("MaximumBandwidthPerJob = 2048"),
            std::string::npos);
  EXPECT_NE(updated_text.find("Description = \"Managed by service\""),
            std::string::npos);

  const auto updated_stub_text = ReadTextFile(stub_path);
  EXPECT_NE(updated_stub_text.find("ConnectionFromDirectorToClient = no"),
            std::string::npos);
  EXPECT_NE(updated_stub_text.find("ConnectionFromClientToDirector = yes"),
            std::string::npos);
  EXPECT_NE(updated_stub_text.find("MaximumBandwidthPerJob = 2048"),
            std::string::npos);
}

TEST(BconfigService, UpsertsDirectorClientResourcesPreserveLargeImportedPort)
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
  WriteTextFile(
      source_root.path() / "bareos-dir.d/client/bareos-fd.conf",
      "Client {\n"
      "  Name = \"bareos-fd\"\n"
      "  Description = \"Imported client\"\n"
      "  Address = localhost\n"
      "  LanAddress = imported-client-lan.example.com\n"
      "  Protocol = NDMPV3\n"
      "  AuthType = Clear\n"
      "  Catalog = MyCatalog\n"
      "  Username = \"imported-user\"\n"
      "  Password = \"secret\"\n"
      "  Port = 70000\n"
      "  Enabled = no\n"
      "  Passive = yes\n"
      "  StrictQuotas = yes\n"
      "  QuotaIncludeFailedJobs = no\n"
      "  SoftQuota = 16384\n"
      "  HardQuota = 32768\n"
      "  SoftQuotaGracePeriod = 240\n"
      "  FileRetention = 7200\n"
      "  JobRetention = 14400\n"
      "  NdmpLogLevel = 5\n"
      "  NdmpBlockSize = 32768\n"
      "  NdmpUseLmdb = no\n"
      "  AutoPrune = yes\n"
      "  TlsAuthenticate = yes\n"
      "  TlsEnable = yes\n"
      "  TlsRequire = yes\n"
      "  TlsVerifyPeer = yes\n"
      "  TlsCipherList = \"DEFAULT\"\n"
      "  TlsCipherSuites = \"TLS_AES_128_GCM_SHA256\"\n"
      "  TlsDhFile = \"/etc/bareos/dh2048.pem\"\n"
      "  TlsProtocol = \"TLSv1.2\"\n"
      "  TlsCaCertificateFile = \"/etc/bareos/import-ca.pem\"\n"
      "  TlsCaCertificateDir = \"/etc/bareos/ca-dir\"\n"
      "  TlsCertificateRevocationList = \"/etc/bareos/import-crl.pem\"\n"
      "  TlsCertificate = \"/etc/bareos/import-client.crt\"\n"
      "  TlsKey = \"/etc/bareos/import-client.key\"\n"
      "  TlsAllowedCn = \"import-backup-dir.example.test\"\n"
      "  TlsAllowedCn = \"import-backup-dir.internal\"\n"
      "  ConnectionFromDirectorToClient = no\n"
      "  ConnectionFromClientToDirector = yes\n"
      "  MaximumConcurrentJobs = 4\n"
      "  HeartbeatInterval = 45\n"
      "  MaximumBandwidthPerJob = 8192\n"
      "}\n");

  auto import_job
      = state.CreateJob({.type = "import_configuration",
                         .deployment_id = std::string{"prod"},
                         .source_path = source_root.path().string()});
  ASSERT_TRUE(import_job);
  auto imported = WaitForJobTerminal(state, import_job.value->id);
  ASSERT_TRUE(imported.has_value());
  ASSERT_EQ(imported->status, JobStatus::kSucceeded);

  auto updated = state.UpsertDirectorClientResource(
      "prod", "bareos-dir", "bareos-fd",
      {.description = std::string{"Updated imported client"}});
  ASSERT_TRUE(updated) << updated.error;

  const auto updated_text = ReadTextFile(
      updated.value->path / "bareos-dir.d/client/bareos-fd.conf");
  EXPECT_NE(updated_text.find("Address = localhost"), std::string::npos);
  EXPECT_NE(updated_text.find("LanAddress = imported-client-lan.example.com"),
            std::string::npos);
  EXPECT_NE(updated_text.find("Protocol = NDMPV3"), std::string::npos);
  EXPECT_NE(updated_text.find("AuthType = Clear"), std::string::npos);
  EXPECT_NE(updated_text.find("Catalog = MyCatalog"), std::string::npos);
  EXPECT_NE(updated_text.find("Username = \"imported-user\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("Port = 70000"), std::string::npos);
  EXPECT_NE(updated_text.find("Enabled = no"), std::string::npos);
  EXPECT_NE(updated_text.find("Passive = yes"), std::string::npos);
  EXPECT_NE(updated_text.find("StrictQuotas = yes"), std::string::npos);
  EXPECT_NE(updated_text.find("QuotaIncludeFailedJobs = no"),
            std::string::npos);
  EXPECT_NE(updated_text.find("SoftQuota = 16384"), std::string::npos);
  EXPECT_NE(updated_text.find("HardQuota = 32768"), std::string::npos);
  EXPECT_NE(updated_text.find("SoftQuotaGracePeriod = 240"), std::string::npos);
  EXPECT_NE(updated_text.find("FileRetention = 7200"), std::string::npos);
  EXPECT_NE(updated_text.find("JobRetention = 14400"), std::string::npos);
  EXPECT_NE(updated_text.find("NdmpLogLevel = 5"), std::string::npos);
  EXPECT_NE(updated_text.find("NdmpBlockSize = 32768"), std::string::npos);
  EXPECT_NE(updated_text.find("NdmpUseLmdb = no"), std::string::npos);
  EXPECT_NE(updated_text.find("AutoPrune = yes"), std::string::npos);
  EXPECT_NE(updated_text.find("TlsAuthenticate = yes"), std::string::npos);
  EXPECT_NE(updated_text.find("TlsEnable = yes"), std::string::npos);
  EXPECT_NE(updated_text.find("TlsRequire = yes"), std::string::npos);
  EXPECT_NE(updated_text.find("TlsVerifyPeer = yes"), std::string::npos);
  EXPECT_NE(updated_text.find("TlsCipherList = \"DEFAULT\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("TlsCipherSuites = \"TLS_AES_128_GCM_SHA256\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("TlsDhFile = \"/etc/bareos/dh2048.pem\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("TlsProtocol = \"TLSv1.2\""), std::string::npos);
  EXPECT_NE(
      updated_text.find("TlsCaCertificateFile = \"/etc/bareos/import-ca.pem\""),
      std::string::npos);
  EXPECT_NE(updated_text.find("TlsCaCertificateDir = \"/etc/bareos/ca-dir\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("TlsCertificateRevocationList = "
                              "\"/etc/bareos/import-crl.pem\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("TlsCertificate = "
                              "\"/etc/bareos/import-client.crt\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("TlsKey = \"/etc/bareos/import-client.key\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("TlsAllowedCn = "
                              "\"import-backup-dir.example.test\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("TlsAllowedCn = "
                              "\"import-backup-dir.internal\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("ConnectionFromDirectorToClient = no"),
            std::string::npos);
  EXPECT_NE(updated_text.find("ConnectionFromClientToDirector = yes"),
            std::string::npos);
  EXPECT_NE(updated_text.find("MaximumConcurrentJobs = 4"), std::string::npos);
  EXPECT_NE(updated_text.find("HeartbeatInterval = 45"), std::string::npos);
  EXPECT_NE(updated_text.find("MaximumBandwidthPerJob = 8192"),
            std::string::npos);
  EXPECT_NE(updated_text.find("Description = \"Updated imported client\""),
            std::string::npos);

  const auto stub_path = RepositoryLayout::ClientsDirectory(repo_path.path())
                         / "bareos-fd/bareos-fd.d/director/bareos-dir.conf";
  const auto stub_text = ReadTextFile(stub_path);
  EXPECT_EQ(stub_text.find("Passive = yes"), std::string::npos);
  EXPECT_EQ(stub_text.find("StrictQuotas = yes"), std::string::npos);
  EXPECT_EQ(stub_text.find("QuotaIncludeFailedJobs = no"), std::string::npos);
  EXPECT_EQ(stub_text.find("SoftQuota = 16384"), std::string::npos);
  EXPECT_EQ(stub_text.find("HardQuota = 32768"), std::string::npos);
  EXPECT_EQ(stub_text.find("SoftQuotaGracePeriod = 240"), std::string::npos);
  EXPECT_EQ(stub_text.find("FileRetention = 7200"), std::string::npos);
  EXPECT_EQ(stub_text.find("JobRetention = 14400"), std::string::npos);
  EXPECT_EQ(stub_text.find("NdmpLogLevel = 5"), std::string::npos);
  EXPECT_EQ(stub_text.find("NdmpBlockSize = 32768"), std::string::npos);
  EXPECT_EQ(stub_text.find("NdmpUseLmdb = no"), std::string::npos);
  EXPECT_EQ(stub_text.find("AutoPrune = yes"), std::string::npos);
  EXPECT_EQ(stub_text.find("TlsAuthenticate = yes"), std::string::npos);
  EXPECT_EQ(stub_text.find("TlsEnable = yes"), std::string::npos);
  EXPECT_EQ(stub_text.find("TlsRequire = yes"), std::string::npos);
  EXPECT_EQ(stub_text.find("TlsVerifyPeer = yes"), std::string::npos);
  EXPECT_EQ(stub_text.find("TlsCipherList = \"DEFAULT\""), std::string::npos);
  EXPECT_EQ(stub_text.find("TlsCipherSuites = \"TLS_AES_128_GCM_SHA256\""),
            std::string::npos);
  EXPECT_EQ(stub_text.find("TlsDhFile = \"/etc/bareos/dh2048.pem\""),
            std::string::npos);
  EXPECT_EQ(stub_text.find("TlsProtocol = \"TLSv1.2\""), std::string::npos);
  EXPECT_EQ(
      stub_text.find("TlsCaCertificateFile = \"/etc/bareos/import-ca.pem\""),
      std::string::npos);
  EXPECT_EQ(stub_text.find("TlsCaCertificateDir = \"/etc/bareos/ca-dir\""),
            std::string::npos);
  EXPECT_NE(stub_text.find("ConnectionFromDirectorToClient = no"),
            std::string::npos);
  EXPECT_NE(stub_text.find("ConnectionFromClientToDirector = yes"),
            std::string::npos);
  EXPECT_EQ(stub_text.find("MaximumConcurrentJobs = 4"), std::string::npos);
  EXPECT_NE(stub_text.find("MaximumBandwidthPerJob = 8192"), std::string::npos);
}

TEST(BconfigService, UpsertsDirectorClientResourcesInSharedFiles)
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

  const auto client_directory
      = RepositoryLayout::DirectorsDirectory(repo_path.path())
        / "bareos-dir/bareos-dir.d/client";
  const auto original_path = client_directory / "bareos-fd.conf";
  const auto shared_path = client_directory / "shared.conf";
  std::filesystem::rename(original_path, shared_path);

  auto updated = state.UpsertDirectorClientResource(
      "prod", "bareos-dir", "bareos-fd",
      {.address = std::string{"bareos-fd-alt.example.com"}});
  ASSERT_TRUE(updated) << updated.error;
  EXPECT_FALSE(std::filesystem::exists(original_path));
  EXPECT_TRUE(std::filesystem::exists(shared_path));
  const auto shared_text = ReadTextFile(shared_path);
  EXPECT_NE(shared_text.find("Address = bareos-fd-alt.example.com"),
            std::string::npos);
}

TEST(BconfigService, DeletesDirectorClientResources)
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

  auto created = state.UpsertDirectorClientResource(
      "prod", "bareos-dir", "client1-fd",
      {.address = std::string{"client1-fd.example.com"},
       .password = std::string{"[md5]0123456789abcdef0123456789abcdef"}});
  ASSERT_TRUE(created) << created.error;

  const auto director_client_path
      = created.value->path / "bareos-dir.d/client/client1-fd.conf";
  const auto stub_path = RepositoryLayout::ClientsDirectory(repo_path.path())
                         / "client1-fd/bareos-fd.d/director/bareos-dir.conf";
  ASSERT_TRUE(std::filesystem::exists(director_client_path));
  ASSERT_TRUE(std::filesystem::exists(stub_path));

  auto deleted
      = state.DeleteDirectorClientResource("prod", "bareos-dir", "client1-fd");
  ASSERT_TRUE(deleted) << deleted.error;
  EXPECT_FALSE(std::filesystem::exists(director_client_path));
  EXPECT_FALSE(std::filesystem::exists(stub_path));
}

TEST(BconfigService, DeletesDirectorClientResourcesFromSharedFiles)
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

  const auto client_directory
      = RepositoryLayout::DirectorsDirectory(repo_path.path())
        / "bareos-dir/bareos-dir.d/client";
  const auto original_path = client_directory / "bareos-fd.conf";
  const auto shared_path = client_directory / "shared.conf";
  const auto original_text = ReadTextFile(original_path);
  WriteTextFile(shared_path,
                original_text
                    + "\nClient {\n"
                      "  Name = \"other-client\"\n"
                      "  Address = other-client.example.com\n"
                      "  Password = \"other_password\"\n"
                      "}\n");
  std::filesystem::remove(original_path);

  auto deleted = state.DeleteDirectorClientResource("prod", "bareos-dir",
                                                    "other-client");
  ASSERT_TRUE(deleted) << deleted.error;
  EXPECT_FALSE(std::filesystem::exists(original_path));
  EXPECT_TRUE(std::filesystem::exists(shared_path));
  const auto shared_text = ReadTextFile(shared_path);
  EXPECT_NE(shared_text.find("Name = bareos-fd"), std::string::npos);
  EXPECT_EQ(shared_text.find("Name = \"other-client\""), std::string::npos);
}
TEST(BconfigService, UpsertsDirectorStorageResources)
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

  auto created = state.UpsertDirectorStorageResource(
      "prod", "bareos-dir", "FileManaged",
      {.address = std::string{"localhost"},
       .lan_address = std::string{"storage-lan.example.com"},
       .protocol = std::string{"NDMPV4"},
       .auth_type = std::string{"Clear"},
       .username = std::string{"ndmp-user"},
       .password = std::string{"[md5]abcdef0123456789abcdef0123456789"},
       .device = std::string{"FileStorage"},
       .media_type = std::string{"File"},
       .autochanger = true,
       .enabled = false,
       .allow_compression = false,
       .heartbeat_interval = 60,
       .cache_status_interval = 90,
       .maximum_concurrent_jobs = 11,
       .maximum_concurrent_read_jobs = 3,
       .paired_storage = std::string{"File"},
       .maximum_bandwidth_per_job = 2048,
       .collect_statistics = true,
       .ndmp_changer_device = std::string{"changer0"},
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
       .tls_certificate = std::string{"/etc/bareos/storage.crt"},
       .tls_key = std::string{"/etc/bareos/storage.key"},
       .tls_allowed_cn
       = std::vector<std::string>{"storage-cn-1", "storage-cn-2"},
       .description = std::string{"Managed storage"}});
  ASSERT_TRUE(created) << created.error;
  EXPECT_EQ(created.value->name, "bareos-dir");

  const auto storage_path
      = created.value->path / "bareos-dir.d/storage/FileManaged.conf";
  const auto created_text = ReadTextFile(storage_path);
  EXPECT_NE(created_text.find("Name = \"FileManaged\""), std::string::npos);
  EXPECT_NE(created_text.find("Address = localhost"), std::string::npos);
  EXPECT_NE(created_text.find("LanAddress = storage-lan.example.com"),
            std::string::npos);
  EXPECT_NE(created_text.find("Protocol = NDMPV4"), std::string::npos);
  EXPECT_NE(created_text.find("AuthType = Clear"), std::string::npos);
  EXPECT_NE(created_text.find("Username = \"ndmp-user\""), std::string::npos);
  EXPECT_NE(
      created_text.find("Password = \"[md5]abcdef0123456789abcdef0123456789\""),
      std::string::npos);
  EXPECT_NE(created_text.find("Device = FileStorage"), std::string::npos);
  EXPECT_NE(created_text.find("Media Type = File"), std::string::npos);
  EXPECT_NE(created_text.find("Port = 9103"), std::string::npos);
  EXPECT_NE(created_text.find("AutoChanger = yes"), std::string::npos);
  EXPECT_NE(created_text.find("Enabled = no"), std::string::npos);
  EXPECT_NE(created_text.find("AllowCompression = no"), std::string::npos);
  EXPECT_NE(created_text.find("HeartbeatInterval = 60"), std::string::npos);
  EXPECT_NE(created_text.find("CacheStatusInterval = 90"), std::string::npos);
  EXPECT_NE(created_text.find("MaximumConcurrentJobs = 11"), std::string::npos);
  EXPECT_NE(created_text.find("MaximumConcurrentReadJobs = 3"),
            std::string::npos);
  EXPECT_NE(created_text.find("PairedStorage = File"), std::string::npos);
  EXPECT_NE(created_text.find("MaximumBandwidthPerJob = 2048"),
            std::string::npos);
  EXPECT_NE(created_text.find("CollectStatistics = yes"), std::string::npos);
  EXPECT_NE(created_text.find("NdmpChangerDevice = changer0"),
            std::string::npos);
  EXPECT_NE(created_text.find("TlsAuthenticate = no"), std::string::npos);
  EXPECT_NE(created_text.find("TlsEnable = yes"), std::string::npos);
  EXPECT_NE(created_text.find("TlsRequire = no"), std::string::npos);
  EXPECT_NE(created_text.find("TlsVerifyPeer = yes"), std::string::npos);
  EXPECT_NE(
      created_text.find("TlsCipherList = \"ECDHE-RSA-AES256-GCM-SHA384\""),
      std::string::npos);
  EXPECT_NE(created_text.find("TlsCipherSuites = \"TLS_AES_256_GCM_SHA384\""),
            std::string::npos);
  EXPECT_NE(created_text.find("TlsDhFile = \"/etc/bareos/dh4096.pem\""),
            std::string::npos);
  EXPECT_NE(created_text.find("TlsProtocol = \"MinProtocol = TLSv1.2\""),
            std::string::npos);
  EXPECT_NE(created_text.find("TlsCaCertificateFile = \"/etc/bareos/ca.crt\""),
            std::string::npos);
  EXPECT_NE(created_text.find("TlsCaCertificateDir = \"/etc/ssl/certs\""),
            std::string::npos);
  EXPECT_NE(created_text.find(
                "TlsCertificateRevocationList = \"/etc/bareos/crl.pem\""),
            std::string::npos);
  EXPECT_NE(created_text.find("TlsCertificate = \"/etc/bareos/storage.crt\""),
            std::string::npos);
  EXPECT_NE(created_text.find("TlsKey = \"/etc/bareos/storage.key\""),
            std::string::npos);
  EXPECT_NE(created_text.find("TlsAllowedCn = \"storage-cn-1\""),
            std::string::npos);
  EXPECT_NE(created_text.find("TlsAllowedCn = \"storage-cn-2\""),
            std::string::npos);
  EXPECT_NE(created_text.find("Description = \"Managed storage\""),
            std::string::npos);

  auto updated = state.UpsertDirectorStorageResource(
      "prod", "bareos-dir", "FileManaged",
      {.address = std::string{"storage.example.com"}});
  ASSERT_TRUE(updated) << updated.error;

  const auto updated_text = ReadTextFile(storage_path);
  EXPECT_NE(updated_text.find("Address = storage.example.com"),
            std::string::npos);
  EXPECT_NE(updated_text.find("LanAddress = storage-lan.example.com"),
            std::string::npos);
  EXPECT_NE(updated_text.find("Protocol = NDMPV4"), std::string::npos);
  EXPECT_NE(updated_text.find("AuthType = Clear"), std::string::npos);
  EXPECT_NE(updated_text.find("Username = \"ndmp-user\""), std::string::npos);
  EXPECT_NE(
      updated_text.find("Password = \"[md5]abcdef0123456789abcdef0123456789\""),
      std::string::npos);
  EXPECT_NE(updated_text.find("Device = FileStorage"), std::string::npos);
  EXPECT_NE(updated_text.find("Media Type = File"), std::string::npos);
  EXPECT_NE(updated_text.find("Port = 9103"), std::string::npos);
  EXPECT_NE(updated_text.find("AutoChanger = yes"), std::string::npos);
  EXPECT_NE(updated_text.find("Enabled = no"), std::string::npos);
  EXPECT_NE(updated_text.find("AllowCompression = no"), std::string::npos);
  EXPECT_NE(updated_text.find("HeartbeatInterval = 60"), std::string::npos);
  EXPECT_NE(updated_text.find("CacheStatusInterval = 90"), std::string::npos);
  EXPECT_NE(updated_text.find("MaximumConcurrentJobs = 11"), std::string::npos);
  EXPECT_NE(updated_text.find("MaximumConcurrentReadJobs = 3"),
            std::string::npos);
  EXPECT_NE(updated_text.find("PairedStorage = File"), std::string::npos);
  EXPECT_NE(updated_text.find("MaximumBandwidthPerJob = 2048"),
            std::string::npos);
  EXPECT_NE(updated_text.find("CollectStatistics = yes"), std::string::npos);
  EXPECT_NE(updated_text.find("NdmpChangerDevice = changer0"),
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
  EXPECT_NE(updated_text.find("TlsCertificate = \"/etc/bareos/storage.crt\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("TlsKey = \"/etc/bareos/storage.key\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("TlsAllowedCn = \"storage-cn-1\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("TlsAllowedCn = \"storage-cn-2\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("Description = \"Managed storage\""),
            std::string::npos);
}

TEST(BconfigService, UpsertsDirectorStorageResourcesPreserveLargeImportedPort)
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
  WriteTextFile(source_root.path() / "bareos-dir.d/storage/File.conf",
                "Storage {\n"
                "  Name = \"File\"\n"
                "  Description = \"Imported storage\"\n"
                "  Address = localhost\n"
                "  LanAddress = imported-storage-lan.example.com\n"
                "  Password = \"secret\"\n"
                "  Device = FileStorage\n"
                "  Media Type = File\n"
                "  Port = 70000\n"
                "  Enabled = no\n"
                "  AllowCompression = no\n"
                "  HeartbeatInterval = 45\n"
                "  CacheStatusInterval = 75\n"
                "  MaximumBandwidthPerJob = 8192\n"
                "}\n");

  auto import_job
      = state.CreateJob({.type = "import_configuration",
                         .deployment_id = std::string{"prod"},
                         .source_path = source_root.path().string()});
  ASSERT_TRUE(import_job);
  auto imported = WaitForJobTerminal(state, import_job.value->id);
  ASSERT_TRUE(imported.has_value());
  ASSERT_EQ(imported->status, JobStatus::kSucceeded);

  auto updated = state.UpsertDirectorStorageResource(
      "prod", "bareos-dir", "File",
      {.description = std::string{"Updated imported storage"}});
  ASSERT_TRUE(updated) << updated.error;

  const auto updated_text
      = ReadTextFile(updated.value->path / "bareos-dir.d/storage/File.conf");
  EXPECT_NE(updated_text.find("Address = localhost"), std::string::npos);
  EXPECT_NE(updated_text.find("LanAddress = imported-storage-lan.example.com"),
            std::string::npos);
  EXPECT_NE(updated_text.find("Port = 70000"), std::string::npos);
  EXPECT_NE(updated_text.find("Enabled = no"), std::string::npos);
  EXPECT_NE(updated_text.find("AllowCompression = no"), std::string::npos);
  EXPECT_NE(updated_text.find("HeartbeatInterval = 45"), std::string::npos);
  EXPECT_NE(updated_text.find("CacheStatusInterval = 75"), std::string::npos);
  EXPECT_NE(updated_text.find("MaximumBandwidthPerJob = 8192"),
            std::string::npos);
  EXPECT_NE(updated_text.find("Description = \"Updated imported storage\""),
            std::string::npos);
}

TEST(BconfigService,
     UpsertsDirectorStorageResourcesWithMatchingImportedStorageDaemonFiles)
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

  const auto storage_root
      = RepositoryLayout::StoragesDirectory(repo_path.path()) / "bareos-sd";
  const auto original_director_text
      = ReadTextFile(storage_root / "bareos-sd.d/director/bareos-dir.conf");
  const auto original_device_text
      = ReadTextFile(storage_root / "bareos-sd.d/device/FileStorage.conf");

  auto created = state.UpsertDirectorStorageResource(
      "prod", "bareos-dir", "FileManaged",
      {.address = std::string{"localhost"},
       .lan_address = std::string{"storage-lan.example.com"},
       .password = std::string{"sd_password"},
       .device = std::string{"FileStorage"},
       .media_type = std::string{"File"},
       .allow_compression = false,
       .cache_status_interval = 120});
  ASSERT_TRUE(created) << created.error;

  EXPECT_EQ(ReadTextFile(storage_root / "bareos-sd.d/director/bareos-dir.conf"),
            original_director_text);
  EXPECT_EQ(ReadTextFile(storage_root / "bareos-sd.d/device/FileStorage.conf"),
            original_device_text);
}

TEST(BconfigService, DefaultsNewDirectorStorageDeviceToStorageName)
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

  auto created = state.UpsertDirectorStorageResource(
      "prod", "bareos-dir", "FileManaged",
      {.address = std::string{"localhost"},
       .password = std::string{"[md5]abcdef0123456789abcdef0123456789"},
       .media_type = std::string{"File"},
       .archive_device = std::string{"/tmp/bareos-storage"},
       .device_type = std::string{"file"}});
  ASSERT_TRUE(created) << created.error;

  const auto director_storage_path
      = created.value->path / "bareos-dir.d/storage/FileManaged.conf";
  EXPECT_NE(ReadTextFile(director_storage_path).find("Device = FileManaged"),
            std::string::npos);

  const auto storage_root
      = RepositoryLayout::StoragesDirectory(repo_path.path()) / "bareos-sd";
  EXPECT_TRUE(std::filesystem::exists(storage_root
                                      / "bareos-sd.d/device/FileManaged.conf"));
}

TEST(BconfigService, UpsertsDirectorStorageResourcesInSharedFiles)
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

  auto updated = state.UpsertDirectorStorageResource(
      "prod", "bareos-dir", "File",
      {.address = std::string{"storage-alt.example.com"}});
  ASSERT_TRUE(updated) << updated.error;
  const auto shared_text
      = ReadTextFile(RepositoryLayout::DirectorsDirectory(repo_path.path())
                     / "bareos-dir/bareos-dir.d/storage/File.conf");
  EXPECT_NE(shared_text.find("Address = storage-alt.example.com"),
            std::string::npos);
  EXPECT_NE(shared_text.find("Name = File2"), std::string::npos);
}

TEST(BconfigService, UpsertsDirectorStorageResourcesWithMultipleDevices)
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
  WriteTextFile(source_root.path() / "bareos-dir.d/storage/MultiDevice.conf",
                "Storage {\n"
                "  Name = \"MultiDevice\"\n"
                "  Description = \"Imported storage\"\n"
                "  Address = localhost\n"
                "  Password = \"secret\"\n"
                "  Device = FileStorage\n"
                "  Device = FileStorage2\n"
                "  Media Type = File\n"
                "  Port = 9103\n"
                "}\n");

  auto import_job
      = state.CreateJob({.type = "import_configuration",
                         .deployment_id = std::string{"prod"},
                         .source_path = source_root.path().string()});
  ASSERT_TRUE(import_job);
  auto imported = WaitForJobTerminal(state, import_job.value->id);
  ASSERT_TRUE(imported.has_value());
  ASSERT_EQ(imported->status, JobStatus::kSucceeded);

  auto updated = state.UpsertDirectorStorageResource(
      "prod", "bareos-dir", "MultiDevice",
      {.address = std::string{"storage.example.com"},
       .description = std::string{"Updated storage"}});
  ASSERT_TRUE(updated) << updated.error;

  const auto updated_text = ReadTextFile(
      updated.value->path / "bareos-dir.d/storage/MultiDevice.conf");
  EXPECT_NE(updated_text.find("Address = storage.example.com"),
            std::string::npos);
  EXPECT_NE(updated_text.find("Description = \"Updated storage\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("Device = FileStorage"), std::string::npos);
  EXPECT_NE(updated_text.find("Device = FileStorage2"), std::string::npos);
}

TEST(BconfigService, UpsertsSharedDirectorStorageResourcesWithMultipleDevices)
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
  WriteTextFile(source_root.path() / "bareos-dir.d/storage/shared-multi.conf",
                "Storage {\n"
                "  Name = \"MultiDevice\"\n"
                "  Description = \"Imported storage\"\n"
                "  Address = localhost\n"
                "  Password = \"secret\"\n"
                "  Device = FileStorage\n"
                "  Device = FileStorage2\n"
                "  Media Type = File\n"
                "  Port = 9103\n"
                "}\n"
                "\n"
                "Storage {\n"
                "  Name = \"Other\"\n"
                "  Address = other.example.test\n"
                "  Password = \"other-secret\"\n"
                "  Device = OtherDevice\n"
                "  Media Type = File\n"
                "  Port = 9103\n"
                "}\n");

  auto import_job
      = state.CreateJob({.type = "import_configuration",
                         .deployment_id = std::string{"prod"},
                         .source_path = source_root.path().string()});
  ASSERT_TRUE(import_job);
  auto imported = WaitForJobTerminal(state, import_job.value->id);
  ASSERT_TRUE(imported.has_value());
  ASSERT_EQ(imported->status, JobStatus::kSucceeded);

  auto updated = state.UpsertDirectorStorageResource(
      "prod", "bareos-dir", "MultiDevice",
      {.address = std::string{"storage.example.com"},
       .description = std::string{"Updated storage"}});
  ASSERT_TRUE(updated) << updated.error;

  const auto updated_text = ReadTextFile(
      updated.value->path / "bareos-dir.d/storage/shared-multi.conf");
  EXPECT_NE(updated_text.find("Address = storage.example.com"),
            std::string::npos);
  EXPECT_NE(updated_text.find("Description = \"Updated storage\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("Device = FileStorage"), std::string::npos);
  EXPECT_NE(updated_text.find("Device = FileStorage2"), std::string::npos);
  EXPECT_NE(updated_text.find("Name = \"Other\""), std::string::npos);
}

TEST(BconfigService, DeletesDirectorStorageResources)
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

  auto created = state.UpsertDirectorStorageResource(
      "prod", "bareos-dir", "FileManaged",
      {.address = std::string{"localhost"},
       .password = std::string{"[md5]abcdef0123456789abcdef0123456789"},
       .device = std::string{"FileManagedDevice"},
       .media_type = std::string{"File"},
       .archive_device = std::string{"/tmp/bareos-storage"},
       .device_type = std::string{"file"}});
  ASSERT_TRUE(created) << created.error;

  const auto director_storage_path
      = created.value->path / "bareos-dir.d/storage/FileManaged.conf";
  const auto storage_root
      = RepositoryLayout::StoragesDirectory(repo_path.path()) / "bareos-sd";
  const auto director_sync_path
      = storage_root / "bareos-sd.d/director/bareos-dir.conf";
  const auto device_sync_path
      = storage_root / "bareos-sd.d/device/FileManagedDevice.conf";
  ASSERT_TRUE(std::filesystem::exists(director_storage_path));
  ASSERT_TRUE(std::filesystem::exists(director_sync_path));
  ASSERT_TRUE(std::filesystem::exists(device_sync_path));

  auto deleted = state.DeleteDirectorStorageResource("prod", "bareos-dir",
                                                     "FileManaged");
  ASSERT_TRUE(deleted) << deleted.error;

  EXPECT_FALSE(std::filesystem::exists(director_storage_path));
  EXPECT_TRUE(std::filesystem::exists(director_sync_path));
  EXPECT_FALSE(std::filesystem::exists(device_sync_path));

  const auto ownership_text
      = ReadTextFile(RepositoryLayout::OwnershipPath(repo_path.path()));
  EXPECT_NE(ownership_text.find("storages/bareos-sd/bareos-sd.d/director/"
                                "bareos-dir.conf"),
            std::string::npos);
  EXPECT_EQ(ownership_text.find("storages/bareos-sd/bareos-sd.d/device/"
                                "FileManagedDevice.conf"),
            std::string::npos);
}

TEST(BconfigService, DeletesDirectorStorageResourcesFromSharedFiles)
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

  auto deleted
      = state.DeleteDirectorStorageResource("prod", "bareos-dir", "File2");
  ASSERT_TRUE(deleted) << deleted.error;
  const auto shared_text
      = ReadTextFile(RepositoryLayout::DirectorsDirectory(repo_path.path())
                     / "bareos-dir/bareos-dir.d/storage/File.conf");
  EXPECT_NE(shared_text.find("Name = File"), std::string::npos);
  EXPECT_EQ(shared_text.find("Name = File2"), std::string::npos);
}

TEST(BconfigService, SyncsDirectorStorageResourcesIntoStorageDaemonFiles)
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

  auto created = state.UpsertDirectorStorageResource(
      "prod", "bareos-dir", "FileManaged",
      {.address = std::string{"localhost"},
       .password = std::string{"[md5]abcdef0123456789abcdef0123456789"},
       .device = std::string{"FileManagedDevice"},
       .media_type = std::string{"File"},
       .heartbeat_interval = 60,
       .maximum_bandwidth_per_job = 4096,
       .archive_device = std::string{"/tmp/bareos-storage"},
       .device_type = std::string{"file"},
       .description = std::string{"Managed storage"}});
  ASSERT_TRUE(created) << created.error;

  const auto storage_root
      = RepositoryLayout::StoragesDirectory(repo_path.path()) / "bareos-sd";
  const auto director_sync_path
      = storage_root / "bareos-sd.d/director/bareos-dir.conf";
  const auto device_sync_path
      = storage_root / "bareos-sd.d/device/FileManagedDevice.conf";

  auto updated_device = state.UpsertStorageDeviceResource(
      "prod", "bareos-sd", "FileManagedDevice",
      {.device_options = std::string{"Block Size = 32k"},
       .maximum_open_wait = 305,
       .collect_statistics = false,
       .description = std::string{"Managed synced device"}});
  ASSERT_TRUE(updated_device) << updated_device.error;

  auto updated_storage = state.UpsertDirectorStorageResource(
      "prod", "bareos-dir", "FileManaged",
      {.address = std::string{"storage.example.com"}});
  ASSERT_TRUE(updated_storage) << updated_storage.error;

  const auto director_text = ReadTextFile(director_sync_path);
  EXPECT_NE(director_text.find("Name = \"bareos-dir\""), std::string::npos);
  EXPECT_NE(director_text.find(
                "Password = \"[md5]abcdef0123456789abcdef0123456789\""),
            std::string::npos);
  EXPECT_NE(director_text.find("MaximumBandwidthPerJob = 4096"),
            std::string::npos);

  const auto device_text = ReadTextFile(device_sync_path);
  EXPECT_NE(device_text.find("Name = \"FileManagedDevice\""),
            std::string::npos);
  EXPECT_NE(device_text.find("Media Type = File"), std::string::npos);
  EXPECT_NE(device_text.find("Archive Device = /tmp/bareos-storage"),
            std::string::npos);
  EXPECT_NE(device_text.find("Device Type = \"file\""), std::string::npos);
  EXPECT_NE(device_text.find("DeviceOptions = \"Block Size = 32k\""),
            std::string::npos);
  EXPECT_NE(device_text.find("MaximumOpenWait = 305"), std::string::npos);
  EXPECT_NE(device_text.find("CollectStatistics = no"), std::string::npos);
  EXPECT_NE(device_text.find("Description = \"Managed synced device\""),
            std::string::npos);

  const auto ownership_text
      = ReadTextFile(RepositoryLayout::OwnershipPath(repo_path.path()));
  EXPECT_NE(ownership_text.find("storages/bareos-sd/bareos-sd.d/director/"
                                "bareos-dir.conf"),
            std::string::npos);
  EXPECT_NE(ownership_text.find("storages/bareos-sd/bareos-sd.d/device/"
                                "FileManagedDevice.conf"),
            std::string::npos);
}

}  // namespace
}  // namespace bconfig::service
