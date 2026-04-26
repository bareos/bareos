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
TEST(BconfigService, UpsertsDirectorConsoleResources)
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

  auto created = state.UpsertDirectorConsoleResource(
      "prod", "bareos-dir", "managed-console",
      {.password = std::string{"[md5]abcdef0123456789abcdef0123456789"},
       .description = std::string{"Managed console"},
       .job_acl = std::vector<std::string>{"ManagedJob"},
       .profiles = std::vector<std::string>{"operator"},
       .use_pam_authentication = true,
       .tls_enable = true,
       .tls_require = true,
       .tls_cipher_list = std::string{"HIGH"},
       .tls_allowed_cn = std::vector<std::string>{"backup-admin"}});
  ASSERT_TRUE(created) << created.error;
  EXPECT_EQ(created.value->name, "bareos-dir");

  const auto console_path
      = created.value->path / "bareos-dir.d/console/managed-console.conf";
  const auto created_text = ReadTextFile(console_path);
  EXPECT_NE(created_text.find("Name = \"managed-console\""), std::string::npos);
  EXPECT_NE(
      created_text.find("Password = \"[md5]abcdef0123456789abcdef0123456789\""),
      std::string::npos);
  EXPECT_NE(created_text.find("Description = \"Managed console\""),
            std::string::npos);
  EXPECT_NE(created_text.find("JobACL = ManagedJob"), std::string::npos);
  EXPECT_NE(created_text.find("Profile = operator"), std::string::npos);
  EXPECT_NE(created_text.find("UsePamAuthentication = yes"), std::string::npos);
  EXPECT_NE(created_text.find("TlsEnable = yes"), std::string::npos);
  EXPECT_NE(created_text.find("TlsRequire = yes"), std::string::npos);
  EXPECT_NE(created_text.find("TlsCipherList = \"HIGH\""), std::string::npos);
  EXPECT_NE(created_text.find("TlsAllowedCn = \"backup-admin\""),
            std::string::npos);

  auto updated = state.UpsertDirectorConsoleResource(
      "prod", "bareos-dir", "bareos-mon",
      {.description = std::string{"Updated restricted console"},
       .command_acl = std::vector<std::string>{"status", ".status", "show"},
       .profiles = std::vector<std::string>{"operator"},
       .tls_enable = false,
       .tls_protocol = std::string{"TLSv1.2"}});
  ASSERT_TRUE(updated) << updated.error;

  const auto updated_text = ReadTextFile(
      updated.value->path / "bareos-dir.d/console/bareos-mon.conf");
  EXPECT_NE(updated_text.find("Description = \"Updated restricted console\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("Password = "), std::string::npos);
  EXPECT_NE(updated_text.find("CommandACL = status, .status, show"),
            std::string::npos);
  EXPECT_NE(updated_text.find("Profile = operator"), std::string::npos);
  EXPECT_NE(updated_text.find("TlsEnable = no"), std::string::npos);
  EXPECT_NE(updated_text.find("TlsProtocol = \"TLSv1.2\""), std::string::npos);
}

TEST(BconfigService, UpsertsDirectorConsoleResourcesInSharedFiles)
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

  const auto console_directory
      = RepositoryLayout::DirectorsDirectory(repo_path.path())
        / "bareos-dir/bareos-dir.d/console";
  const auto original_path = console_directory / "bareos-mon.conf";
  const auto shared_path = console_directory / "shared.conf";
  const auto original_text = ReadTextFile(original_path);
  WriteTextFile(shared_path,
                original_text
                    + "\nConsole {\n"
                      "  Name = \"other-console\"\n"
                      "  Password = \"other_password\"\n"
                      "}\n");
  std::filesystem::remove(original_path);

  auto updated = state.UpsertDirectorConsoleResource(
      "prod", "bareos-dir", "bareos-mon",
      {.description = std::string{"Updated restricted console"}});
  ASSERT_TRUE(updated) << updated.error;
  EXPECT_FALSE(std::filesystem::exists(original_path));
  EXPECT_TRUE(std::filesystem::exists(shared_path));
  const auto shared_text = ReadTextFile(shared_path);
  EXPECT_NE(shared_text.find("Description = \"Updated restricted console\""),
            std::string::npos);
  EXPECT_NE(shared_text.find("Name = \"other-console\""), std::string::npos);
}

TEST(BconfigService, DeletesDirectorConsoleResources)
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

  auto created = state.UpsertDirectorConsoleResource(
      "prod", "bareos-dir", "managed-console",
      {.password = std::string{"[md5]abcdef0123456789abcdef0123456789"}});
  ASSERT_TRUE(created) << created.error;

  const auto console_path
      = created.value->path / "bareos-dir.d/console/managed-console.conf";
  ASSERT_TRUE(std::filesystem::exists(console_path));

  auto deleted = state.DeleteDirectorConsoleResource("prod", "bareos-dir",
                                                     "managed-console");
  ASSERT_TRUE(deleted) << deleted.error;
  EXPECT_FALSE(std::filesystem::exists(console_path));
}

TEST(BconfigService, DeletesDirectorConsoleResourcesFromSharedFiles)
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

  const auto console_directory
      = RepositoryLayout::DirectorsDirectory(repo_path.path())
        / "bareos-dir/bareos-dir.d/console";
  const auto original_path = console_directory / "bareos-mon.conf";
  const auto shared_path = console_directory / "shared.conf";
  const auto original_text = ReadTextFile(original_path);
  WriteTextFile(shared_path,
                original_text
                    + "\nConsole {\n"
                      "  Name = \"other-console\"\n"
                      "  Password = \"other_password\"\n"
                      "}\n");
  std::filesystem::remove(original_path);

  auto deleted = state.DeleteDirectorConsoleResource("prod", "bareos-dir",
                                                     "other-console");
  ASSERT_TRUE(deleted) << deleted.error;
  EXPECT_FALSE(std::filesystem::exists(original_path));
  EXPECT_TRUE(std::filesystem::exists(shared_path));
  const auto shared_text = ReadTextFile(shared_path);
  EXPECT_NE(shared_text.find("CommandACL = status, .status"),
            std::string::npos);
  EXPECT_EQ(shared_text.find("Name = \"other-console\""), std::string::npos);
}

TEST(BconfigService, UpsertsDirectorUserResources)
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
  WriteTextFile(source_root.path() / "bareos-dir.d/user/operator-user.conf",
                "User {\n"
                "  Name = \"operator-user\"\n"
                "  Description = \"Fixture operator user\"\n"
                "  CommandACL = status, .status\n"
                "  Profile = operator\n"
                "}\n");

  auto import_job
      = state.CreateJob({.type = "import_configuration",
                         .deployment_id = std::string{"prod"},
                         .source_path = source_root.path().string()});
  ASSERT_TRUE(import_job);
  auto imported = WaitForJobTerminal(state, import_job.value->id);
  ASSERT_TRUE(imported.has_value());
  ASSERT_EQ(imported->status, JobStatus::kSucceeded);

  auto created = state.UpsertDirectorUserResource(
      "prod", "bareos-dir", "managed-user",
      {.description = std::string{"Managed user"},
       .command_acl = std::vector<std::string>{"status", ".status"},
       .profiles = std::vector<std::string>{"operator"}});
  ASSERT_TRUE(created) << created.error;
  EXPECT_EQ(created.value->name, "bareos-dir");

  const auto user_path
      = created.value->path / "bareos-dir.d/user/managed-user.conf";
  const auto created_text = ReadTextFile(user_path);
  EXPECT_NE(created_text.find("Name = \"managed-user\""), std::string::npos);
  EXPECT_NE(created_text.find("Description = \"Managed user\""),
            std::string::npos);
  EXPECT_NE(created_text.find("CommandACL = status, .status"),
            std::string::npos);
  EXPECT_NE(created_text.find("Profile = operator"), std::string::npos);

  auto updated = state.UpsertDirectorUserResource(
      "prod", "bareos-dir", "operator-user",
      {.description = std::string{"Updated operator user"},
       .command_acl = std::vector<std::string>{"list", "llist"},
       .profiles = std::vector<std::string>{"operator"}});
  ASSERT_TRUE(updated) << updated.error;

  const auto updated_text = ReadTextFile(
      updated.value->path / "bareos-dir.d/user/operator-user.conf");
  EXPECT_NE(updated_text.find("Description = \"Updated operator user\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("CommandACL = list, llist"), std::string::npos);
  EXPECT_NE(updated_text.find("Profile = operator"), std::string::npos);
}

TEST(BconfigService, UpsertsDirectorUserResourcesInSharedFiles)
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
  WriteTextFile(source_root.path() / "bareos-dir.d/user/operator-user.conf",
                "User {\n"
                "  Name = \"operator-user\"\n"
                "  Description = \"Fixture operator user\"\n"
                "}\n");

  auto import_job
      = state.CreateJob({.type = "import_configuration",
                         .deployment_id = std::string{"prod"},
                         .source_path = source_root.path().string()});
  ASSERT_TRUE(import_job);
  auto imported = WaitForJobTerminal(state, import_job.value->id);
  ASSERT_TRUE(imported.has_value());
  ASSERT_EQ(imported->status, JobStatus::kSucceeded);

  const auto user_directory
      = RepositoryLayout::DirectorsDirectory(repo_path.path())
        / "bareos-dir/bareos-dir.d/user";
  const auto original_path = user_directory / "operator-user.conf";
  const auto shared_path = user_directory / "shared.conf";
  const auto original_text = ReadTextFile(original_path);
  WriteTextFile(shared_path,
                original_text
                    + "\nUser {\n"
                      "  Name = \"other-user\"\n"
                      "}\n");
  std::filesystem::remove(original_path);

  auto updated = state.UpsertDirectorUserResource(
      "prod", "bareos-dir", "operator-user",
      {.description = std::string{"Updated operator user"}});
  ASSERT_TRUE(updated) << updated.error;
  EXPECT_FALSE(std::filesystem::exists(original_path));
  EXPECT_TRUE(std::filesystem::exists(shared_path));
  const auto shared_text = ReadTextFile(shared_path);
  EXPECT_NE(shared_text.find("Description = \"Updated operator user\""),
            std::string::npos);
  EXPECT_NE(shared_text.find("Name = \"other-user\""), std::string::npos);
}

TEST(BconfigService, DeletesDirectorUserResources)
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

  auto created = state.UpsertDirectorUserResource(
      "prod", "bareos-dir", "managed-user",
      {.description = std::string{"Managed user"}});
  ASSERT_TRUE(created) << created.error;

  const auto user_path
      = created.value->path / "bareos-dir.d/user/managed-user.conf";
  ASSERT_TRUE(std::filesystem::exists(user_path));

  auto deleted
      = state.DeleteDirectorUserResource("prod", "bareos-dir", "managed-user");
  ASSERT_TRUE(deleted) << deleted.error;
  EXPECT_FALSE(std::filesystem::exists(user_path));
}

TEST(BconfigService, DeletesDirectorUserResourcesFromSharedFiles)
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
  WriteTextFile(source_root.path() / "bareos-dir.d/user/operator-user.conf",
                "User {\n"
                "  Name = \"operator-user\"\n"
                "  Description = \"Fixture operator user\"\n"
                "}\n");

  auto import_job
      = state.CreateJob({.type = "import_configuration",
                         .deployment_id = std::string{"prod"},
                         .source_path = source_root.path().string()});
  ASSERT_TRUE(import_job);
  auto imported = WaitForJobTerminal(state, import_job.value->id);
  ASSERT_TRUE(imported.has_value());
  ASSERT_EQ(imported->status, JobStatus::kSucceeded);

  const auto user_directory
      = RepositoryLayout::DirectorsDirectory(repo_path.path())
        / "bareos-dir/bareos-dir.d/user";
  const auto original_path = user_directory / "operator-user.conf";
  const auto shared_path = user_directory / "shared.conf";
  const auto original_text = ReadTextFile(original_path);
  WriteTextFile(shared_path,
                original_text
                    + "\nUser {\n"
                      "  Name = \"other-user\"\n"
                      "}\n");
  std::filesystem::remove(original_path);

  auto deleted
      = state.DeleteDirectorUserResource("prod", "bareos-dir", "other-user");
  ASSERT_TRUE(deleted) << deleted.error;
  EXPECT_FALSE(std::filesystem::exists(original_path));
  EXPECT_TRUE(std::filesystem::exists(shared_path));
  const auto shared_text = ReadTextFile(shared_path);
  EXPECT_NE(shared_text.find("Name = \"operator-user\""), std::string::npos);
  EXPECT_EQ(shared_text.find("Name = \"other-user\""), std::string::npos);
}

TEST(BconfigService, UpsertsDirectorProfileResources)
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

  auto created = state.UpsertDirectorProfileResource(
      "prod", "bareos-dir", "managed-profile",
      {.description = std::string{"Managed profile"},
       .command_acl = std::vector<std::string>{"status", ".status"},
       .catalog_acl = std::vector<std::string>{"managed-catalog"}});
  ASSERT_TRUE(created) << created.error;
  EXPECT_EQ(created.value->name, "bareos-dir");

  const auto profile_path
      = created.value->path / "bareos-dir.d/profile/managed-profile.conf";
  const auto created_text = ReadTextFile(profile_path);
  EXPECT_NE(created_text.find("Name = \"managed-profile\""), std::string::npos);
  EXPECT_NE(created_text.find("Description = \"Managed profile\""),
            std::string::npos);
  EXPECT_NE(created_text.find("CommandACL = status, .status"),
            std::string::npos);
  EXPECT_NE(created_text.find("CatalogACL = managed-catalog"),
            std::string::npos);

  auto updated = state.UpsertDirectorProfileResource(
      "prod", "bareos-dir", "operator",
      {.description = std::string{"Updated operator profile"},
       .command_acl = std::vector<std::string>{"list", "llist"},
       .catalog_acl = std::vector<std::string>{"operator-catalog"}});
  ASSERT_TRUE(updated) << updated.error;

  const auto updated_text = ReadTextFile(
      updated.value->path / "bareos-dir.d/profile/operator.conf");
  EXPECT_NE(updated_text.find("Description = \"Updated operator profile\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("CommandACL = list, llist"), std::string::npos);
  EXPECT_NE(updated_text.find("CatalogACL = operator-catalog"),
            std::string::npos);
}

TEST(BconfigService, UpsertsDirectorProfileResourcesInSharedFiles)
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

  const auto profile_directory
      = RepositoryLayout::DirectorsDirectory(repo_path.path())
        / "bareos-dir/bareos-dir.d/profile";
  const auto original_path = profile_directory / "operator.conf";
  const auto shared_path = profile_directory / "shared.conf";
  const auto original_text = ReadTextFile(original_path);
  WriteTextFile(shared_path,
                original_text
                    + "\nProfile {\n"
                      "  Name = \"other-profile\"\n"
                      "}\n");
  std::filesystem::remove(original_path);

  auto updated = state.UpsertDirectorProfileResource(
      "prod", "bareos-dir", "operator",
      {.description = std::string{"Updated operator profile"}});
  ASSERT_TRUE(updated) << updated.error;
  EXPECT_FALSE(std::filesystem::exists(original_path));
  EXPECT_TRUE(std::filesystem::exists(shared_path));
  const auto shared_text = ReadTextFile(shared_path);
  EXPECT_NE(shared_text.find("Description = \"Updated operator profile\""),
            std::string::npos);
  EXPECT_NE(shared_text.find("Name = \"other-profile\""), std::string::npos);
}

TEST(BconfigService, DeletesDirectorProfileResources)
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

  auto created = state.UpsertDirectorProfileResource(
      "prod", "bareos-dir", "managed-profile",
      {.description = std::string{"Managed profile"}});
  ASSERT_TRUE(created) << created.error;

  const auto profile_path
      = created.value->path / "bareos-dir.d/profile/managed-profile.conf";
  ASSERT_TRUE(std::filesystem::exists(profile_path));

  auto deleted = state.DeleteDirectorProfileResource("prod", "bareos-dir",
                                                     "managed-profile");
  ASSERT_TRUE(deleted) << deleted.error;
  EXPECT_FALSE(std::filesystem::exists(profile_path));
}

TEST(BconfigService, DeletesDirectorProfileResourcesFromSharedFiles)
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

  const auto profile_directory
      = RepositoryLayout::DirectorsDirectory(repo_path.path())
        / "bareos-dir/bareos-dir.d/profile";
  const auto original_path = profile_directory / "operator.conf";
  const auto shared_path = profile_directory / "shared.conf";
  const auto original_text = ReadTextFile(original_path);
  WriteTextFile(shared_path,
                original_text
                    + "\nProfile {\n"
                      "  Name = \"other-profile\"\n"
                      "}\n");
  std::filesystem::remove(original_path);

  auto deleted = state.DeleteDirectorProfileResource("prod", "bareos-dir",
                                                     "other-profile");
  ASSERT_TRUE(deleted) << deleted.error;
  EXPECT_FALSE(std::filesystem::exists(original_path));
  EXPECT_TRUE(std::filesystem::exists(shared_path));
  const auto shared_text = ReadTextFile(shared_path);
  EXPECT_NE(shared_text.find("Name = operator"), std::string::npos);
  EXPECT_EQ(shared_text.find("Name = \"other-profile\""), std::string::npos);
}

TEST(BconfigService, UpsertsDirectorPoolResources)
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

  auto created = state.UpsertDirectorPoolResource(
      "prod", "bareos-dir", "managed-pool",
      {.pool_type = std::string{"Scratch"},
       .label_format = std::string{"Managed-"},
       .cleaning_prefix = std::string{"Cleaner-"},
       .label_type = std::string{"ansi"},
       .maximum_volumes = 7,
       .maximum_volume_jobs = 8,
       .maximum_volume_files = 9,
       .maximum_volume_bytes = 123456,
       .volume_retention = 86400,
       .volume_use_duration = 43200,
       .migration_time = 3600,
       .migration_high_bytes = 654321,
       .migration_low_bytes = 12345,
       .next_pool = std::string{"Incremental"},
       .storages = std::vector<std::string>{"File"},
       .use_catalog = true,
       .catalog_files = true,
       .purge_oldest_volume = true,
       .action_on_purge = std::string{"Truncate"},
       .recycle_oldest_volume = true,
       .recycle_current_volume = false,
       .auto_prune = true,
       .recycle = false,
       .recycle_pool = std::string{"Scratch"},
       .scratch_pool = std::string{"Scratch"},
       .catalog = std::string{"MyCatalog"},
       .file_retention = 172800,
       .job_retention = 259200,
       .minimum_block_size = 512,
       .maximum_block_size = 4096,
       .description = std::string{"Managed pool"}});
  ASSERT_TRUE(created) << created.error;
  EXPECT_EQ(created.value->name, "bareos-dir");

  const auto pool_path
      = created.value->path / "bareos-dir.d/pool/managed-pool.conf";
  const auto created_text = ReadTextFile(pool_path);
  EXPECT_NE(created_text.find("Name = \"managed-pool\""), std::string::npos);
  EXPECT_NE(created_text.find("Description = \"Managed pool\""),
            std::string::npos);
  EXPECT_NE(created_text.find("PoolType = Scratch"), std::string::npos);
  EXPECT_NE(created_text.find("LabelFormat = \"Managed-\""), std::string::npos);
  EXPECT_NE(created_text.find("CleaningPrefix = \"Cleaner-\""),
            std::string::npos);
  EXPECT_NE(created_text.find("LabelType = ansi"), std::string::npos);
  EXPECT_NE(created_text.find("MaximumVolumes = 7"), std::string::npos);
  EXPECT_NE(created_text.find("MaximumVolumeJobs = 8"), std::string::npos);
  EXPECT_NE(created_text.find("MaximumVolumeFiles = 9"), std::string::npos);
  EXPECT_NE(created_text.find("MaximumVolumeBytes = 123456"),
            std::string::npos);
  EXPECT_NE(created_text.find("VolumeRetention = 86400"), std::string::npos);
  EXPECT_NE(created_text.find("VolumeUseDuration = 43200"), std::string::npos);
  EXPECT_NE(created_text.find("MigrationTime = 3600"), std::string::npos);
  EXPECT_NE(created_text.find("MigrationHighBytes = 654321"),
            std::string::npos);
  EXPECT_NE(created_text.find("MigrationLowBytes = 12345"), std::string::npos);
  EXPECT_NE(created_text.find("NextPool = Incremental"), std::string::npos);
  EXPECT_NE(created_text.find("Storage = File"), std::string::npos);
  EXPECT_NE(created_text.find("UseCatalog = yes"), std::string::npos);
  EXPECT_NE(created_text.find("CatalogFiles = yes"), std::string::npos);
  EXPECT_NE(created_text.find("PurgeOldestVolume = yes"), std::string::npos);
  EXPECT_NE(created_text.find("ActionOnPurge = Truncate"), std::string::npos);
  EXPECT_NE(created_text.find("RecycleOldestVolume = yes"), std::string::npos);
  EXPECT_NE(created_text.find("RecycleCurrentVolume = no"), std::string::npos);
  EXPECT_NE(created_text.find("AutoPrune = yes"), std::string::npos);
  EXPECT_NE(created_text.find("Recycle = no"), std::string::npos);
  EXPECT_NE(created_text.find("RecyclePool = Scratch"), std::string::npos);
  EXPECT_NE(created_text.find("ScratchPool = Scratch"), std::string::npos);
  EXPECT_NE(created_text.find("Catalog = MyCatalog"), std::string::npos);
  EXPECT_NE(created_text.find("FileRetention = 172800"), std::string::npos);
  EXPECT_NE(created_text.find("JobRetention = 259200"), std::string::npos);
  EXPECT_NE(created_text.find("MinimumBlockSize = 512"), std::string::npos);
  EXPECT_NE(created_text.find("MaximumBlockSize = 4096"), std::string::npos);

  const auto full_path = created.value->path / "bareos-dir.d/pool/Full.conf";
  auto full_text = ReadTextFile(full_path);
  const std::string insertion
      = "  UseCatalog = yes\n"
        "  CatalogFiles = yes\n"
        "  PurgeOldestVolume = yes\n"
        "  ActionOnPurge = Truncate\n"
        "  RecycleOldestVolume = yes\n"
        "  RecycleCurrentVolume = no\n"
        "  MaximumVolumeJobs = 11\n"
        "  MaximumVolumeFiles = 12\n"
        "  VolumeUseDuration = 1800\n"
        "  MigrationTime = 2700\n"
        "  MigrationHighBytes = 2000\n"
        "  MigrationLowBytes = 1000\n"
        "  NextPool = Incremental\n"
        "  Storage = File\n"
        "  CleaningPrefix = \"clean-\"\n"
        "  LabelType = ansi\n"
        "  RecyclePool = Scratch\n"
        "  ScratchPool = Scratch\n"
        "  Catalog = MyCatalog\n"
        "  FileRetention = 604800\n"
        "  JobRetention = 1209600\n"
        "  MinimumBlockSize = 1024\n"
        "  MaximumBlockSize = 8192\n";
  auto brace = full_text.rfind("}\n");
  ASSERT_NE(brace, std::string::npos);
  full_text.insert(brace, insertion);
  WriteTextFile(full_path, full_text);

  auto updated = state.UpsertDirectorPoolResource(
      "prod", "bareos-dir", "Full",
      {.description = std::string{"Updated full pool"}});
  ASSERT_TRUE(updated) << updated.error;

  const auto updated_text = ReadTextFile(full_path);
  EXPECT_NE(updated_text.find("Description = \"Updated full pool\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("PoolType = Backup"), std::string::npos);
  EXPECT_NE(updated_text.find("Recycle = yes"), std::string::npos);
  EXPECT_NE(updated_text.find("AutoPrune = yes"), std::string::npos);
  EXPECT_NE(updated_text.find("VolumeRetention = 31536000"), std::string::npos);
  EXPECT_NE(updated_text.find("MaximumVolumeBytes = 53687091200"),
            std::string::npos);
  EXPECT_NE(updated_text.find("MaximumVolumes = 100"), std::string::npos);
  EXPECT_NE(updated_text.find("LabelFormat = \"Full-\""), std::string::npos);
  EXPECT_NE(updated_text.find("UseCatalog = yes"), std::string::npos);
  EXPECT_NE(updated_text.find("CatalogFiles = yes"), std::string::npos);
  EXPECT_NE(updated_text.find("PurgeOldestVolume = yes"), std::string::npos);
  EXPECT_NE(updated_text.find("ActionOnPurge = Truncate"), std::string::npos);
  EXPECT_NE(updated_text.find("RecycleOldestVolume = yes"), std::string::npos);
  EXPECT_NE(updated_text.find("RecycleCurrentVolume = no"), std::string::npos);
  EXPECT_NE(updated_text.find("MaximumVolumeJobs = 11"), std::string::npos);
  EXPECT_NE(updated_text.find("MaximumVolumeFiles = 12"), std::string::npos);
  EXPECT_NE(updated_text.find("VolumeUseDuration = 1800"), std::string::npos);
  EXPECT_NE(updated_text.find("MigrationTime = 2700"), std::string::npos);
  EXPECT_NE(updated_text.find("MigrationHighBytes = 2000"), std::string::npos);
  EXPECT_NE(updated_text.find("MigrationLowBytes = 1000"), std::string::npos);
  EXPECT_NE(updated_text.find("NextPool = Incremental"), std::string::npos);
  EXPECT_NE(updated_text.find("Storage = File"), std::string::npos);
  EXPECT_NE(updated_text.find("CleaningPrefix = \"clean-\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("LabelType = ansi"), std::string::npos);
  EXPECT_NE(updated_text.find("RecyclePool = Scratch"), std::string::npos);
  EXPECT_NE(updated_text.find("ScratchPool = Scratch"), std::string::npos);
  EXPECT_NE(updated_text.find("Catalog = MyCatalog"), std::string::npos);
  EXPECT_NE(updated_text.find("FileRetention = 604800"), std::string::npos);
  EXPECT_NE(updated_text.find("JobRetention = 1209600"), std::string::npos);
  EXPECT_NE(updated_text.find("MinimumBlockSize = 1024"), std::string::npos);
  EXPECT_NE(updated_text.find("MaximumBlockSize = 8192"), std::string::npos);
}

TEST(BconfigService, UpsertsDirectorPoolResourcesInSharedFiles)
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

  const auto pool_directory
      = RepositoryLayout::DirectorsDirectory(repo_path.path())
        / "bareos-dir/bareos-dir.d/pool";
  const auto original_path = pool_directory / "Full.conf";
  const auto shared_path = pool_directory / "shared.conf";
  const auto original_text = ReadTextFile(original_path);
  WriteTextFile(shared_path,
                original_text
                    + "\nPool {\n"
                      "  Name = \"OtherPool\"\n"
                      "  PoolType = Backup\n"
                      "}\n");
  std::filesystem::remove(original_path);

  auto updated = state.UpsertDirectorPoolResource(
      "prod", "bareos-dir", "Full",
      {.description = std::string{"Updated full pool"}});
  ASSERT_TRUE(updated) << updated.error;
  EXPECT_FALSE(std::filesystem::exists(original_path));
  EXPECT_TRUE(std::filesystem::exists(shared_path));
  const auto shared_text = ReadTextFile(shared_path);
  EXPECT_NE(shared_text.find("Description = \"Updated full pool\""),
            std::string::npos);
  EXPECT_NE(shared_text.find("Name = \"OtherPool\""), std::string::npos);
}

TEST(BconfigService, DeletesDirectorPoolResources)
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

  auto created = state.UpsertDirectorPoolResource(
      "prod", "bareos-dir", "managed-pool",
      {.pool_type = std::string{"Scratch"},
       .description = std::string{"Managed pool"}});
  ASSERT_TRUE(created) << created.error;

  const auto pool_path
      = created.value->path / "bareos-dir.d/pool/managed-pool.conf";
  ASSERT_TRUE(std::filesystem::exists(pool_path));

  auto deleted
      = state.DeleteDirectorPoolResource("prod", "bareos-dir", "managed-pool");
  ASSERT_TRUE(deleted) << deleted.error;
  EXPECT_FALSE(std::filesystem::exists(pool_path));
}

TEST(BconfigService, DeletesDirectorPoolResourcesFromSharedFiles)
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

  const auto pool_directory
      = RepositoryLayout::DirectorsDirectory(repo_path.path())
        / "bareos-dir/bareos-dir.d/pool";
  const auto original_path = pool_directory / "Full.conf";
  const auto shared_path = pool_directory / "shared.conf";
  const auto original_text = ReadTextFile(original_path);
  WriteTextFile(shared_path,
                original_text
                    + "\nPool {\n"
                      "  Name = \"OtherPool\"\n"
                      "  PoolType = Backup\n"
                      "}\n");
  std::filesystem::remove(original_path);

  auto deleted
      = state.DeleteDirectorPoolResource("prod", "bareos-dir", "OtherPool");
  ASSERT_TRUE(deleted) << deleted.error;
  EXPECT_FALSE(std::filesystem::exists(original_path));
  EXPECT_TRUE(std::filesystem::exists(shared_path));
  const auto shared_text = ReadTextFile(shared_path);
  EXPECT_NE(shared_text.find("Name = Full"), std::string::npos);
  EXPECT_EQ(shared_text.find("Name = \"OtherPool\""), std::string::npos);
}

TEST(BconfigService, UpsertsDirectorCatalogResources)
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

  auto created = state.UpsertDirectorCatalogResource(
      "prod", "bareos-dir", "ManagedCatalog",
      {.db_address = std::string{"127.0.0.1"},
       .db_port = 5432,
       .db_password = std::string{"secret"},
       .db_user = std::string{"bareos"},
       .db_name = std::string{"bareos_catalog"},
       .multiple_connections = true,
       .disable_batch_insert = true,
       .reconnect = true,
       .exit_on_fatal = false,
       .description = std::string{"Managed catalog"}});
  ASSERT_TRUE(created) << created.error;
  EXPECT_EQ(created.value->name, "bareos-dir");

  const auto catalog_path
      = created.value->path / "bareos-dir.d/catalog/ManagedCatalog.conf";
  const auto created_text = ReadTextFile(catalog_path);
  EXPECT_NE(created_text.find("Name = \"ManagedCatalog\""), std::string::npos);
  EXPECT_NE(created_text.find("Description = \"Managed catalog\""),
            std::string::npos);
  EXPECT_NE(created_text.find("DbAddress = \"127.0.0.1\""), std::string::npos);
  EXPECT_NE(created_text.find("DbPort = 5432"), std::string::npos);
  EXPECT_NE(created_text.find("DbPassword = \"secret\""), std::string::npos);
  EXPECT_NE(created_text.find("DbUser = \"bareos\""), std::string::npos);
  EXPECT_NE(created_text.find("DbName = \"bareos_catalog\""),
            std::string::npos);
  EXPECT_NE(created_text.find("MultipleConnections = yes"), std::string::npos);
  EXPECT_NE(created_text.find("DisableBatchInsert = yes"), std::string::npos);
  EXPECT_NE(created_text.find("Reconnect = yes"), std::string::npos);
  EXPECT_NE(created_text.find("ExitOnFatal = no"), std::string::npos);

  auto updated = state.UpsertDirectorCatalogResource(
      "prod", "bareos-dir", "MyCatalog",
      {.multiple_connections = false,
       .disable_batch_insert = true,
       .description = std::string{"Updated catalog"}});
  ASSERT_TRUE(updated) << updated.error;

  const auto updated_text = ReadTextFile(
      updated.value->path / "bareos-dir.d/catalog/MyCatalog.conf");
  EXPECT_NE(updated_text.find("Description = \"Updated catalog\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("DbUser = \"regress\""), std::string::npos);
  EXPECT_NE(updated_text.find("DbName = \"regress_backup_bareos_test\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("DbPassword = \"\""), std::string::npos);
  EXPECT_NE(updated_text.find("MultipleConnections = no"), std::string::npos);
  EXPECT_NE(updated_text.find("DisableBatchInsert = yes"), std::string::npos);
}

TEST(BconfigService, UpsertsDirectorCatalogResourcesInSharedFiles)
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

  const auto catalog_directory
      = RepositoryLayout::DirectorsDirectory(repo_path.path())
        / "bareos-dir/bareos-dir.d/catalog";
  const auto original_path = catalog_directory / "MyCatalog.conf";
  const auto shared_path = catalog_directory / "shared.conf";
  const auto original_text = ReadTextFile(original_path);
  WriteTextFile(shared_path,
                original_text
                    + "\nCatalog {\n"
                      "  Name = \"OtherCatalog\"\n"
                      "  DbUser = \"other\"\n"
                      "  DbName = \"other\"\n"
                      "}\n");
  std::filesystem::remove(original_path);

  auto updated = state.UpsertDirectorCatalogResource(
      "prod", "bareos-dir", "MyCatalog",
      {.description = std::string{"Updated catalog"}});
  ASSERT_TRUE(updated) << updated.error;
  EXPECT_FALSE(std::filesystem::exists(original_path));
  EXPECT_TRUE(std::filesystem::exists(shared_path));
  const auto shared_text = ReadTextFile(shared_path);
  EXPECT_NE(shared_text.find("Description = \"Updated catalog\""),
            std::string::npos);
  EXPECT_NE(shared_text.find("Name = \"OtherCatalog\""), std::string::npos);
}

TEST(BconfigService, DeletesDirectorCatalogResources)
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

  auto created = state.UpsertDirectorCatalogResource(
      "prod", "bareos-dir", "ManagedCatalog",
      {.db_user = std::string{"bareos"},
       .db_name = std::string{"bareos_catalog"},
       .description = std::string{"Managed catalog"}});
  ASSERT_TRUE(created) << created.error;

  const auto catalog_path
      = created.value->path / "bareos-dir.d/catalog/ManagedCatalog.conf";
  ASSERT_TRUE(std::filesystem::exists(catalog_path));

  auto deleted = state.DeleteDirectorCatalogResource("prod", "bareos-dir",
                                                     "ManagedCatalog");
  ASSERT_TRUE(deleted) << deleted.error;
  EXPECT_FALSE(std::filesystem::exists(catalog_path));
}

TEST(BconfigService, DeletesDirectorCatalogResourcesFromSharedFiles)
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

  const auto catalog_directory
      = RepositoryLayout::DirectorsDirectory(repo_path.path())
        / "bareos-dir/bareos-dir.d/catalog";
  const auto original_path = catalog_directory / "MyCatalog.conf";
  const auto shared_path = catalog_directory / "shared.conf";
  const auto original_text = ReadTextFile(original_path);
  WriteTextFile(shared_path,
                original_text
                    + "\nCatalog {\n"
                      "  Name = \"OtherCatalog\"\n"
                      "  DbUser = \"other\"\n"
                      "  DbName = \"other\"\n"
                      "}\n");
  std::filesystem::remove(original_path);

  auto deleted
      = state.DeleteDirectorCatalogResource("prod", "bareos-dir", "MyCatalog");
  ASSERT_TRUE(deleted) << deleted.error;
  EXPECT_FALSE(std::filesystem::exists(original_path));
  EXPECT_TRUE(std::filesystem::exists(shared_path));
  const auto shared_text = ReadTextFile(shared_path);
  EXPECT_EQ(shared_text.find("Name = \"MyCatalog\""), std::string::npos);
  EXPECT_NE(shared_text.find("Name = \"OtherCatalog\""), std::string::npos);
}

TEST(BconfigService, UpsertsDirectorScheduleResources)
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

  auto created = state.UpsertDirectorScheduleResource(
      "prod", "bareos-dir", "ManagedSchedule",
      {.description = std::string{"Managed schedule"},
       .enabled = true,
       .run_entries
       = std::vector<std::string>{"Level=Full monthly 1st sat at 21:00"}});
  ASSERT_TRUE(created) << created.error;
  EXPECT_EQ(created.value->name, "bareos-dir");

  const auto schedule_path
      = created.value->path / "bareos-dir.d/schedule/ManagedSchedule.conf";
  const auto created_text = ReadTextFile(schedule_path);
  EXPECT_NE(created_text.find("Name = \"ManagedSchedule\""), std::string::npos);
  EXPECT_NE(created_text.find("Description = \"Managed schedule\""),
            std::string::npos);
  EXPECT_NE(created_text.find("Enabled = yes"), std::string::npos);
  EXPECT_NE(created_text.find("Run = Level=Full monthly 1st sat at 21:00"),
            std::string::npos);

  auto updated = state.UpsertDirectorScheduleResource(
      "prod", "bareos-dir", "Odd Weeks",
      {.description = std::string{"Updated odd weeks"}});
  ASSERT_TRUE(updated) << updated.error;

  const auto updated_text = ReadTextFile(
      updated.value->path / "bareos-dir.d/schedule/OddWeeks.conf");
  EXPECT_NE(updated_text.find("Description = \"Updated odd weeks\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("Enabled = yes"), std::string::npos);
  EXPECT_NE(updated_text.find("Run = w01/w02 sun at 23:10"), std::string::npos);
}

TEST(BconfigService, UpsertsDirectorScheduleResourcesInSharedFiles)
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

  const auto schedule_directory
      = RepositoryLayout::DirectorsDirectory(repo_path.path())
        / "bareos-dir/bareos-dir.d/schedule";
  const auto original_path = schedule_directory / "OddWeeks.conf";
  const auto shared_path = schedule_directory / "shared.conf";
  const auto original_text = ReadTextFile(original_path);
  WriteTextFile(shared_path,
                original_text
                    + "\nSchedule {\n"
                      "  Name = \"OtherSchedule\"\n"
                      "  Run = daily at 20:00\n"
                      "}\n");
  std::filesystem::remove(original_path);

  auto updated = state.UpsertDirectorScheduleResource(
      "prod", "bareos-dir", "Odd Weeks",
      {.description = std::string{"Updated odd weeks"}});
  ASSERT_TRUE(updated) << updated.error;
  EXPECT_FALSE(std::filesystem::exists(original_path));
  EXPECT_TRUE(std::filesystem::exists(shared_path));
  const auto shared_text = ReadTextFile(shared_path);
  EXPECT_NE(shared_text.find("Description = \"Updated odd weeks\""),
            std::string::npos);
  EXPECT_NE(shared_text.find("Name = \"OtherSchedule\""), std::string::npos);
}

TEST(BconfigService, DeletesDirectorScheduleResources)
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

  auto created = state.UpsertDirectorScheduleResource(
      "prod", "bareos-dir", "ManagedSchedule",
      {.run_entries = std::vector<std::string>{"daily at 20:00"}});
  ASSERT_TRUE(created) << created.error;

  const auto schedule_path
      = created.value->path / "bareos-dir.d/schedule/ManagedSchedule.conf";
  ASSERT_TRUE(std::filesystem::exists(schedule_path));

  auto deleted = state.DeleteDirectorScheduleResource("prod", "bareos-dir",
                                                      "ManagedSchedule");
  ASSERT_TRUE(deleted) << deleted.error;
  EXPECT_FALSE(std::filesystem::exists(schedule_path));
}

TEST(BconfigService, DeletesDirectorScheduleResourcesFromSharedFiles)
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

  const auto schedule_directory
      = RepositoryLayout::DirectorsDirectory(repo_path.path())
        / "bareos-dir/bareos-dir.d/schedule";
  const auto original_path = schedule_directory / "OddWeeks.conf";
  const auto shared_path = schedule_directory / "shared.conf";
  const auto original_text = ReadTextFile(original_path);
  WriteTextFile(shared_path,
                original_text
                    + "\nSchedule {\n"
                      "  Name = \"OtherSchedule\"\n"
                      "  Run = daily at 20:00\n"
                      "}\n");
  std::filesystem::remove(original_path);

  auto deleted = state.DeleteDirectorScheduleResource("prod", "bareos-dir",
                                                      "OtherSchedule");
  ASSERT_TRUE(deleted) << deleted.error;
  EXPECT_FALSE(std::filesystem::exists(original_path));
  EXPECT_TRUE(std::filesystem::exists(shared_path));
  const auto shared_text = ReadTextFile(shared_path);
  EXPECT_NE(shared_text.find("Run = w01/w02 sun at 23:10"), std::string::npos);
  EXPECT_EQ(shared_text.find("Name = \"OtherSchedule\""), std::string::npos);
}

TEST(BconfigService, UpsertsDirectorCounterResources)
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
  AddCounterFixtures(source_root.path());

  auto import_job
      = state.CreateJob({.type = "import_configuration",
                         .deployment_id = std::string{"prod"},
                         .source_path = source_root.path().string()});
  ASSERT_TRUE(import_job);
  auto imported = WaitForJobTerminal(state, import_job.value->id);
  ASSERT_TRUE(imported.has_value());
  ASSERT_EQ(imported->status, JobStatus::kSucceeded);

  auto created = state.UpsertDirectorCounterResource(
      "prod", "bareos-dir", "ManagedCounter",
      {.minimum = 10,
       .maximum = 50,
       .wrap_counter = std::string{"WrapSeed"},
       .catalog = std::string{"MyCatalog"},
       .description = std::string{"Managed counter"}});
  ASSERT_TRUE(created) << created.error;
  EXPECT_EQ(created.value->name, "bareos-dir");

  const auto counter_path
      = created.value->path / "bareos-dir.d/counter/ManagedCounter.conf";
  const auto created_text = ReadTextFile(counter_path);
  EXPECT_NE(created_text.find("Name = \"ManagedCounter\""), std::string::npos);
  EXPECT_NE(created_text.find("Description = \"Managed counter\""),
            std::string::npos);
  EXPECT_NE(created_text.find("Minimum = 10"), std::string::npos);
  EXPECT_NE(created_text.find("Maximum = 50"), std::string::npos);
  EXPECT_NE(created_text.find("WrapCounter = WrapSeed"), std::string::npos);
  EXPECT_NE(created_text.find("Catalog = MyCatalog"), std::string::npos);

  auto updated = state.UpsertDirectorCounterResource(
      "prod", "bareos-dir", "ExistingCounter",
      {.description = std::string{"Updated counter"}});
  ASSERT_TRUE(updated) << updated.error;

  const auto updated_text = ReadTextFile(
      updated.value->path / "bareos-dir.d/counter/ExistingCounter.conf");
  EXPECT_NE(updated_text.find("Description = \"Updated counter\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("Minimum = 7"), std::string::npos);
  EXPECT_NE(updated_text.find("Maximum = 99"), std::string::npos);
  EXPECT_NE(updated_text.find("WrapCounter = WrapSeed"), std::string::npos);
  EXPECT_NE(updated_text.find("Catalog = MyCatalog"), std::string::npos);
}

TEST(BconfigService, UpsertsDirectorCounterResourcesInSharedFiles)
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
  AddCounterFixtures(source_root.path());

  auto import_job
      = state.CreateJob({.type = "import_configuration",
                         .deployment_id = std::string{"prod"},
                         .source_path = source_root.path().string()});
  ASSERT_TRUE(import_job);
  auto imported = WaitForJobTerminal(state, import_job.value->id);
  ASSERT_TRUE(imported.has_value());
  ASSERT_EQ(imported->status, JobStatus::kSucceeded);

  const auto counter_directory
      = RepositoryLayout::DirectorsDirectory(repo_path.path())
        / "bareos-dir/bareos-dir.d/counter";
  const auto original_path = counter_directory / "ExistingCounter.conf";
  const auto shared_path = counter_directory / "shared.conf";
  const auto original_text = ReadTextFile(original_path);
  WriteTextFile(shared_path,
                original_text
                    + "\nCounter {\n"
                      "  Name = \"OtherCounter\"\n"
                      "  Minimum = 1\n"
                      "  Maximum = 2\n"
                      "}\n");
  std::filesystem::remove(original_path);

  auto updated = state.UpsertDirectorCounterResource(
      "prod", "bareos-dir", "ExistingCounter",
      {.description = std::string{"Updated counter"}});
  ASSERT_TRUE(updated) << updated.error;
  EXPECT_FALSE(std::filesystem::exists(original_path));
  EXPECT_TRUE(std::filesystem::exists(shared_path));
  const auto shared_text = ReadTextFile(shared_path);
  EXPECT_NE(shared_text.find("Description = \"Updated counter\""),
            std::string::npos);
  EXPECT_NE(shared_text.find("Name = \"OtherCounter\""), std::string::npos);
}

TEST(BconfigService, DeletesDirectorCounterResources)
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
  AddCounterFixtures(source_root.path());

  auto import_job
      = state.CreateJob({.type = "import_configuration",
                         .deployment_id = std::string{"prod"},
                         .source_path = source_root.path().string()});
  ASSERT_TRUE(import_job);
  auto imported = WaitForJobTerminal(state, import_job.value->id);
  ASSERT_TRUE(imported.has_value());
  ASSERT_EQ(imported->status, JobStatus::kSucceeded);

  auto created = state.UpsertDirectorCounterResource(
      "prod", "bareos-dir", "ManagedCounter", {.minimum = 10, .maximum = 50});
  ASSERT_TRUE(created) << created.error;

  const auto counter_path
      = created.value->path / "bareos-dir.d/counter/ManagedCounter.conf";
  ASSERT_TRUE(std::filesystem::exists(counter_path));

  auto deleted = state.DeleteDirectorCounterResource("prod", "bareos-dir",
                                                     "ManagedCounter");
  ASSERT_TRUE(deleted) << deleted.error;
  EXPECT_FALSE(std::filesystem::exists(counter_path));
}

TEST(BconfigService, DeletesDirectorCounterResourcesFromSharedFiles)
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
  AddCounterFixtures(source_root.path());

  auto import_job
      = state.CreateJob({.type = "import_configuration",
                         .deployment_id = std::string{"prod"},
                         .source_path = source_root.path().string()});
  ASSERT_TRUE(import_job);
  auto imported = WaitForJobTerminal(state, import_job.value->id);
  ASSERT_TRUE(imported.has_value());
  ASSERT_EQ(imported->status, JobStatus::kSucceeded);

  const auto counter_directory
      = RepositoryLayout::DirectorsDirectory(repo_path.path())
        / "bareos-dir/bareos-dir.d/counter";
  const auto original_path = counter_directory / "ExistingCounter.conf";
  const auto shared_path = counter_directory / "shared.conf";
  const auto original_text = ReadTextFile(original_path);
  WriteTextFile(shared_path,
                original_text
                    + "\nCounter {\n"
                      "  Name = \"OtherCounter\"\n"
                      "  Minimum = 1\n"
                      "  Maximum = 2\n"
                      "}\n");
  std::filesystem::remove(original_path);

  auto deleted = state.DeleteDirectorCounterResource("prod", "bareos-dir",
                                                     "OtherCounter");
  ASSERT_TRUE(deleted) << deleted.error;
  EXPECT_FALSE(std::filesystem::exists(original_path));
  EXPECT_TRUE(std::filesystem::exists(shared_path));
  const auto shared_text = ReadTextFile(shared_path);
  EXPECT_NE(shared_text.find("Description = \"Existing counter\""),
            std::string::npos);
  EXPECT_EQ(shared_text.find("Name = \"OtherCounter\""), std::string::npos);
}

TEST(BconfigService, UpsertsDirectorFilesetResources)
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

  auto created = state.UpsertDirectorFilesetResource(
      "prod", "bareos-dir", "ManagedFileSet",
      {.description = std::string{"Managed fileset"},
       .ignore_fileset_changes = true,
       .enable_vss = false,
       .include_blocks = std::vector<
           std::string>{"  Include {\n"
                        "    Options {\n"
                        "      Signature = XXH128\n"
                        "    }\n"
                        "    File = /tmp/tests/backup-bareos-test/tmp\n"
                        "  }\n"},
       .exclude_blocks = std::vector<std::string>{
           "  Exclude {\n"
           "    File = /tmp/tests/backup-bareos-test/tmp/cache\n"
           "  }\n"}});
  ASSERT_TRUE(created) << created.error;
  EXPECT_EQ(created.value->name, "bareos-dir");

  const auto fileset_path
      = created.value->path / "bareos-dir.d/fileset/ManagedFileSet.conf";
  const auto created_text = ReadTextFile(fileset_path);
  EXPECT_NE(created_text.find("Name = \"ManagedFileSet\""), std::string::npos);
  EXPECT_NE(created_text.find("Description = \"Managed fileset\""),
            std::string::npos);
  EXPECT_NE(created_text.find("IgnoreFileSetChanges = yes"), std::string::npos);
  EXPECT_NE(created_text.find("EnableVSS = no"), std::string::npos);
  EXPECT_NE(
      created_text.find("Include {\n    Options {\n      Signature = XXH128"),
      std::string::npos);
  EXPECT_NE(
      created_text.find(
          "Exclude {\n    File = /tmp/tests/backup-bareos-test/tmp/cache"),
      std::string::npos);

  auto updated = state.UpsertDirectorFilesetResource(
      "prod", "bareos-dir", "LinuxAll",
      {.description = std::string{"Updated LinuxAll fileset"}});
  ASSERT_TRUE(updated) << updated.error;

  const auto updated_text = ReadTextFile(
      updated.value->path / "bareos-dir.d/fileset/LinuxAll.conf");
  EXPECT_NE(updated_text.find("Description = \"Updated LinuxAll fileset\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("IgnoreFileSetChanges = no"), std::string::npos);
  EXPECT_NE(updated_text.find("EnableVSS = yes"), std::string::npos);
  EXPECT_NE(
      updated_text.find("Include {\n    Options {\n      Signature = XXH128"),
      std::string::npos);
  EXPECT_NE(updated_text.find(
                "Exclude {\n    File = /tmp/tests/backup-bareos-test/working"),
            std::string::npos);
}

TEST(BconfigService, UpsertsDirectorFilesetResourcesInSharedFiles)
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

  const auto fileset_directory
      = RepositoryLayout::DirectorsDirectory(repo_path.path())
        / "bareos-dir/bareos-dir.d/fileset";
  const auto original_path = fileset_directory / "LinuxAll.conf";
  const auto shared_path = fileset_directory / "shared.conf";
  const auto original_text = ReadTextFile(original_path);
  WriteTextFile(shared_path,
                original_text
                    + "\nFileSet {\n"
                      "  Name = \"OtherFileSet\"\n"
                      "  Include {\n"
                      "    File = /tmp/other\n"
                      "  }\n"
                      "}\n");
  std::filesystem::remove(original_path);

  auto updated = state.UpsertDirectorFilesetResource(
      "prod", "bareos-dir", "LinuxAll",
      {.description = std::string{"Updated LinuxAll fileset"}});
  ASSERT_TRUE(updated) << updated.error;
  EXPECT_FALSE(std::filesystem::exists(original_path));
  EXPECT_TRUE(std::filesystem::exists(shared_path));
  const auto shared_text = ReadTextFile(shared_path);
  EXPECT_NE(shared_text.find("Description = \"Updated LinuxAll fileset\""),
            std::string::npos);
  EXPECT_NE(shared_text.find("Name = \"OtherFileSet\""), std::string::npos);
}

TEST(BconfigService, DeletesDirectorFilesetResources)
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

  auto created = state.UpsertDirectorFilesetResource(
      "prod", "bareos-dir", "ManagedFileSet",
      {.include_blocks = std::vector<std::string>{
           "  Include {\n"
           "    File = /tmp/tests/backup-bareos-test/tmp\n"
           "  }\n"}});
  ASSERT_TRUE(created) << created.error;

  const auto fileset_path
      = created.value->path / "bareos-dir.d/fileset/ManagedFileSet.conf";
  ASSERT_TRUE(std::filesystem::exists(fileset_path));

  auto deleted = state.DeleteDirectorFilesetResource("prod", "bareos-dir",
                                                     "ManagedFileSet");
  ASSERT_TRUE(deleted) << deleted.error;
  EXPECT_FALSE(std::filesystem::exists(fileset_path));
}

TEST(BconfigService, DeletesDirectorFilesetResourcesFromSharedFiles)
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

  const auto fileset_directory
      = RepositoryLayout::DirectorsDirectory(repo_path.path())
        / "bareos-dir/bareos-dir.d/fileset";
  const auto original_path = fileset_directory / "LinuxAll.conf";
  const auto shared_path = fileset_directory / "shared.conf";
  const auto original_text = ReadTextFile(original_path);
  WriteTextFile(shared_path,
                original_text
                    + "\nFileSet {\n"
                      "  Name = \"OtherFileSet\"\n"
                      "  Include {\n"
                      "    File = /tmp/other\n"
                      "  }\n"
                      "}\n");
  std::filesystem::remove(original_path);

  auto deleted = state.DeleteDirectorFilesetResource("prod", "bareos-dir",
                                                     "OtherFileSet");
  ASSERT_TRUE(deleted) << deleted.error;
  EXPECT_FALSE(std::filesystem::exists(original_path));
  EXPECT_TRUE(std::filesystem::exists(shared_path));
  const auto shared_text = ReadTextFile(shared_path);
  EXPECT_NE(shared_text.find("Signature = XXH128"), std::string::npos);
  EXPECT_EQ(shared_text.find("Name = \"OtherFileSet\""), std::string::npos);
}

TEST(BconfigService, UpsertsDirectorJobResources)
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

  auto created = state.UpsertDirectorJobResource(
      "prod", "bareos-dir", "ManagedJob",
      {.description = std::string{"Managed job"},
       .backup_format = std::string{"Portable"},
       .client = std::string{"bareos-fd"},
       .jobdefs = std::string{"DefaultJob"},
       .run_entries = std::vector<std::string>{"catalog-backup"},
       .write_bootstrap = std::string{"/tmp/managed-job.bsr"},
       .maximum_bandwidth = 12345,
       .max_run_sched_time = 60,
       .max_run_time = 120,
       .max_full_interval = 3600,
       .prefix_links = true,
       .prune_jobs = true,
       .spool_attributes = true,
       .spool_data = false,
       .spool_size = 45678,
       .maximum_concurrent_jobs = 3,
       .reschedule_on_error = true,
       .reschedule_interval = 300,
       .reschedule_times = 4,
       .allow_mixed_priority = true,
       .accurate = true,
       .allow_duplicate_jobs = false,
       .save_file_history = true,
       .file_history_size = 98765,
       .fd_plugin_options = std::vector<std::string>{"fd=one"},
       .sd_plugin_options = std::vector<std::string>{"sd=two"},
       .dir_plugin_options = std::vector<std::string>{"dir=three"},
       .max_concurrent_copies = 5,
       .always_incremental = true,
       .always_incremental_job_retention = 7200,
       .enabled = true});
  ASSERT_TRUE(created) << created.error;
  EXPECT_EQ(created.value->name, "bareos-dir");

  const auto job_path
      = created.value->path / "bareos-dir.d/job/ManagedJob.conf";
  const auto created_text = ReadTextFile(job_path);
  EXPECT_NE(created_text.find("Name = \"ManagedJob\""), std::string::npos);
  EXPECT_NE(created_text.find("Description = \"Managed job\""),
            std::string::npos);
  EXPECT_NE(created_text.find("BackupFormat = \"Portable\""),
            std::string::npos);
  EXPECT_NE(created_text.find("JobDefs = DefaultJob"), std::string::npos);
  EXPECT_NE(created_text.find("Client = bareos-fd"), std::string::npos);
  EXPECT_NE(created_text.find("Run = catalog-backup"), std::string::npos);
  EXPECT_NE(created_text.find("WriteBootstrap = \"/tmp/managed-job.bsr\""),
            std::string::npos);
  EXPECT_NE(created_text.find("MaximumBandwidth = 12345"), std::string::npos);
  EXPECT_NE(created_text.find("MaxRunSchedTime = 60"), std::string::npos);
  EXPECT_NE(created_text.find("MaxRunTime = 120"), std::string::npos);
  EXPECT_NE(created_text.find("MaxFullInterval = 3600"), std::string::npos);
  EXPECT_NE(created_text.find("PrefixLinks = yes"), std::string::npos);
  EXPECT_NE(created_text.find("PruneJobs = yes"), std::string::npos);
  EXPECT_NE(created_text.find("SpoolAttributes = yes"), std::string::npos);
  EXPECT_NE(created_text.find("SpoolData = no"), std::string::npos);
  EXPECT_NE(created_text.find("SpoolSize = 45678"), std::string::npos);
  EXPECT_NE(created_text.find("MaximumConcurrentJobs = 3"), std::string::npos);
  EXPECT_NE(created_text.find("RescheduleOnError = yes"), std::string::npos);
  EXPECT_NE(created_text.find("RescheduleInterval = 300"), std::string::npos);
  EXPECT_NE(created_text.find("RescheduleTimes = 4"), std::string::npos);
  EXPECT_NE(created_text.find("AllowMixedPriority = yes"), std::string::npos);
  EXPECT_NE(created_text.find("Accurate = yes"), std::string::npos);
  EXPECT_NE(created_text.find("AllowDuplicateJobs = no"), std::string::npos);
  EXPECT_NE(created_text.find("SaveFileHistory = yes"), std::string::npos);
  EXPECT_NE(created_text.find("FileHistorySize = 98765"), std::string::npos);
  EXPECT_NE(created_text.find("FdPluginOptions = \"fd=one\""),
            std::string::npos);
  EXPECT_NE(created_text.find("SdPluginOptions = \"sd=two\""),
            std::string::npos);
  EXPECT_NE(created_text.find("DirPluginOptions = \"dir=three\""),
            std::string::npos);
  EXPECT_NE(created_text.find("MaxConcurrentCopies = 5"), std::string::npos);
  EXPECT_NE(created_text.find("AlwaysIncremental = yes"), std::string::npos);
  EXPECT_NE(created_text.find("AlwaysIncrementalJobRetention = 7200"),
            std::string::npos);
  EXPECT_NE(created_text.find("Enabled = yes"), std::string::npos);

  const auto backup_catalog_path
      = created.value->path / "bareos-dir.d/job/BackupCatalog.conf";
  auto backup_catalog_text = ReadTextFile(backup_catalog_path);
  const std::string backup_catalog_insertion
      = "  BackupFormat = \"Portable\"\n"
        "  Run = backup-catalog-now\n"
        "  FdPluginOptions = \"fd=imported\"\n"
        "  SdPluginOptions = \"sd=imported\"\n"
        "  DirPluginOptions = \"dir=imported\"\n"
        "  MaximumBandwidth = 23456\n"
        "  MaxRunSchedTime = 90\n"
        "  MaxRunTime = 180\n"
        "  MaxFullInterval = 5400\n"
        "  PrefixLinks = yes\n"
        "  PruneJobs = yes\n"
        "  SpoolAttributes = yes\n"
        "  SpoolData = no\n"
        "  SpoolSize = 65432\n"
        "  MaximumConcurrentJobs = 6\n"
        "  RescheduleOnError = yes\n"
        "  RescheduleInterval = 600\n"
        "  RescheduleTimes = 7\n"
        "  AllowMixedPriority = yes\n"
        "  Accurate = yes\n"
        "  AllowDuplicateJobs = no\n"
        "  SaveFileHistory = yes\n"
        "  FileHistorySize = 87654\n"
        "  MaxConcurrentCopies = 8\n"
        "  AlwaysIncremental = yes\n"
        "  AlwaysIncrementalJobRetention = 14400\n";
  auto backup_catalog_brace = backup_catalog_text.rfind("}\n");
  ASSERT_NE(backup_catalog_brace, std::string::npos);
  backup_catalog_text.insert(backup_catalog_brace, backup_catalog_insertion);
  WriteTextFile(backup_catalog_path, backup_catalog_text);

  auto updated = state.UpsertDirectorJobResource(
      "prod", "bareos-dir", "BackupCatalog",
      {.description = std::string{"Updated backup catalog job"}});
  ASSERT_TRUE(updated) << updated.error;

  const auto updated_text = ReadTextFile(
      updated.value->path / "bareos-dir.d/job/BackupCatalog.conf");
  EXPECT_NE(updated_text.find("Description = \"Updated backup catalog job\""),
            std::string::npos);
  EXPECT_NE(
      updated_text.find(
          "RunBeforeJob = \"/tmp/scripts/make_catalog_backup MyCatalog\""),
      std::string::npos);
  EXPECT_NE(updated_text.find(
                "RunAfterJob  = \"/tmp/scripts/delete_catalog_backup\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("WriteBootstrap = "), std::string::npos);
  EXPECT_NE(updated_text.find("BackupFormat = \"Portable\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("Run = backup-catalog-now"), std::string::npos);
  EXPECT_NE(updated_text.find("FdPluginOptions = \"fd=imported\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("SdPluginOptions = \"sd=imported\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("DirPluginOptions = \"dir=imported\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("MaximumBandwidth = 23456"), std::string::npos);
  EXPECT_NE(updated_text.find("MaxRunSchedTime = 90"), std::string::npos);
  EXPECT_NE(updated_text.find("MaxRunTime = 180"), std::string::npos);
  EXPECT_NE(updated_text.find("MaxFullInterval = 5400"), std::string::npos);
  EXPECT_NE(updated_text.find("PrefixLinks = yes"), std::string::npos);
  EXPECT_NE(updated_text.find("PruneJobs = yes"), std::string::npos);
  EXPECT_NE(updated_text.find("SpoolAttributes = yes"), std::string::npos);
  EXPECT_NE(updated_text.find("SpoolData = no"), std::string::npos);
  EXPECT_NE(updated_text.find("SpoolSize = 65432"), std::string::npos);
  EXPECT_NE(updated_text.find("MaximumConcurrentJobs = 6"), std::string::npos);
  EXPECT_NE(updated_text.find("RescheduleOnError = yes"), std::string::npos);
  EXPECT_NE(updated_text.find("RescheduleInterval = 600"), std::string::npos);
  EXPECT_NE(updated_text.find("RescheduleTimes = 7"), std::string::npos);
  EXPECT_NE(updated_text.find("AllowMixedPriority = yes"), std::string::npos);
  EXPECT_NE(updated_text.find("Accurate = yes"), std::string::npos);
  EXPECT_NE(updated_text.find("AllowDuplicateJobs = no"), std::string::npos);
  EXPECT_NE(updated_text.find("SaveFileHistory = yes"), std::string::npos);
  EXPECT_NE(updated_text.find("FileHistorySize = 87654"), std::string::npos);
  EXPECT_NE(updated_text.find("MaxConcurrentCopies = 8"), std::string::npos);
  EXPECT_NE(updated_text.find("AlwaysIncremental = yes"), std::string::npos);
  EXPECT_NE(updated_text.find("AlwaysIncrementalJobRetention = 14400"),
            std::string::npos);
}

TEST(BconfigService, UpsertsDirectorJobResourcesInSharedFiles)
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

  const auto job_directory
      = RepositoryLayout::DirectorsDirectory(repo_path.path())
        / "bareos-dir/bareos-dir.d/job";
  const auto original_path = job_directory / "BackupCatalog.conf";
  const auto shared_path = job_directory / "shared.conf";
  const auto original_text = ReadTextFile(original_path);
  WriteTextFile(shared_path,
                original_text
                    + "\nJob {\n"
                      "  Name = \"OtherJob\"\n"
                      "  JobDefs = DefaultJob\n"
                      "  Client = bareos-fd\n"
                      "}\n");
  std::filesystem::remove(original_path);

  auto updated = state.UpsertDirectorJobResource(
      "prod", "bareos-dir", "BackupCatalog",
      {.description = std::string{"Updated backup catalog job"}});
  ASSERT_TRUE(updated) << updated.error;
  EXPECT_FALSE(std::filesystem::exists(original_path));
  EXPECT_TRUE(std::filesystem::exists(shared_path));
  const auto shared_text = ReadTextFile(shared_path);
  EXPECT_NE(shared_text.find("Description = \"Updated backup catalog job\""),
            std::string::npos);
  EXPECT_NE(shared_text.find("Name = \"OtherJob\""), std::string::npos);
}

TEST(BconfigService, DeletesDirectorJobResources)
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

  auto created
      = state.UpsertDirectorJobResource("prod", "bareos-dir", "ManagedJob",
                                        {.client = std::string{"bareos-fd"},
                                         .jobdefs = std::string{"DefaultJob"}});
  ASSERT_TRUE(created) << created.error;

  const auto job_path
      = created.value->path / "bareos-dir.d/job/ManagedJob.conf";
  ASSERT_TRUE(std::filesystem::exists(job_path));

  auto deleted
      = state.DeleteDirectorJobResource("prod", "bareos-dir", "ManagedJob");
  ASSERT_TRUE(deleted) << deleted.error;
  EXPECT_FALSE(std::filesystem::exists(job_path));
}

TEST(BconfigService, DeletesDirectorJobResourcesFromSharedFiles)
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

  const auto job_directory
      = RepositoryLayout::DirectorsDirectory(repo_path.path())
        / "bareos-dir/bareos-dir.d/job";
  const auto original_path = job_directory / "BackupCatalog.conf";
  const auto shared_path = job_directory / "shared.conf";
  const auto original_text = ReadTextFile(original_path);
  WriteTextFile(shared_path,
                original_text
                    + "\nJob {\n"
                      "  Name = \"OtherJob\"\n"
                      "  JobDefs = DefaultJob\n"
                      "  Client = bareos-fd\n"
                      "}\n");
  std::filesystem::remove(original_path);

  auto deleted
      = state.DeleteDirectorJobResource("prod", "bareos-dir", "OtherJob");
  ASSERT_TRUE(deleted) << deleted.error;
  EXPECT_FALSE(std::filesystem::exists(original_path));
  EXPECT_TRUE(std::filesystem::exists(shared_path));
  const auto shared_text = ReadTextFile(shared_path);
  EXPECT_NE(shared_text.find("RunBeforeJob = "), std::string::npos);
  EXPECT_EQ(shared_text.find("Name = \"OtherJob\""), std::string::npos);
}

TEST(BconfigService, UpsertsDirectorJobDefsResources)
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

  auto created = state.UpsertDirectorJobDefsResource(
      "prod", "bareos-dir", "ManagedJobDefs",
      {.description = std::string{"Managed jobdefs"},
       .type = std::string{"Backup"},
       .backup_format = std::string{"Portable"},
       .messages = std::string{"Standard"},
       .pool = std::string{"Incremental"},
       .client = std::string{"bareos-fd"},
       .fileset = std::string{"SelfTest"},
       .schedule = std::string{"WeeklyCycle"},
       .run_entries = std::vector<std::string>{"managed-jobdefs-run"},
       .write_bootstrap = std::string{"/tmp/managed-jobdefs.bsr"},
       .maximum_bandwidth = 23456,
       .max_run_sched_time = 75,
       .max_run_time = 150,
       .max_full_interval = 4200,
       .prefix_links = true,
       .prune_jobs = true,
       .spool_attributes = true,
       .spool_data = false,
       .spool_size = 56789,
       .maximum_concurrent_jobs = 4,
       .reschedule_on_error = true,
       .reschedule_interval = 450,
       .reschedule_times = 5,
       .allow_mixed_priority = true,
       .accurate = true,
       .allow_duplicate_jobs = false,
       .save_file_history = true,
       .file_history_size = 87654,
       .fd_plugin_options = std::vector<std::string>{"fd=defs"},
       .sd_plugin_options = std::vector<std::string>{"sd=defs"},
       .dir_plugin_options = std::vector<std::string>{"dir=defs"},
       .max_concurrent_copies = 6,
       .always_incremental = true,
       .always_incremental_job_retention = 10800,
       .enabled = true});
  ASSERT_TRUE(created) << created.error;
  EXPECT_EQ(created.value->name, "bareos-dir");

  const auto jobdefs_path
      = created.value->path / "bareos-dir.d/jobdefs/ManagedJobDefs.conf";
  const auto created_text = ReadTextFile(jobdefs_path);
  EXPECT_NE(created_text.find("JobDefs {"), std::string::npos);
  EXPECT_NE(created_text.find("Name = \"ManagedJobDefs\""), std::string::npos);
  EXPECT_NE(created_text.find("Description = \"Managed jobdefs\""),
            std::string::npos);
  EXPECT_NE(created_text.find("Type = Backup"), std::string::npos);
  EXPECT_NE(created_text.find("BackupFormat = \"Portable\""),
            std::string::npos);
  EXPECT_NE(created_text.find("Client = bareos-fd"), std::string::npos);
  EXPECT_NE(created_text.find("Run = managed-jobdefs-run"), std::string::npos);
  EXPECT_NE(created_text.find("WriteBootstrap = \"/tmp/managed-jobdefs.bsr\""),
            std::string::npos);
  EXPECT_NE(created_text.find("MaximumBandwidth = 23456"), std::string::npos);
  EXPECT_NE(created_text.find("MaxRunSchedTime = 75"), std::string::npos);
  EXPECT_NE(created_text.find("MaxRunTime = 150"), std::string::npos);
  EXPECT_NE(created_text.find("MaxFullInterval = 4200"), std::string::npos);
  EXPECT_NE(created_text.find("PrefixLinks = yes"), std::string::npos);
  EXPECT_NE(created_text.find("PruneJobs = yes"), std::string::npos);
  EXPECT_NE(created_text.find("SpoolAttributes = yes"), std::string::npos);
  EXPECT_NE(created_text.find("SpoolData = no"), std::string::npos);
  EXPECT_NE(created_text.find("SpoolSize = 56789"), std::string::npos);
  EXPECT_NE(created_text.find("MaximumConcurrentJobs = 4"), std::string::npos);
  EXPECT_NE(created_text.find("RescheduleOnError = yes"), std::string::npos);
  EXPECT_NE(created_text.find("RescheduleInterval = 450"), std::string::npos);
  EXPECT_NE(created_text.find("RescheduleTimes = 5"), std::string::npos);
  EXPECT_NE(created_text.find("AllowMixedPriority = yes"), std::string::npos);
  EXPECT_NE(created_text.find("Accurate = yes"), std::string::npos);
  EXPECT_NE(created_text.find("AllowDuplicateJobs = no"), std::string::npos);
  EXPECT_NE(created_text.find("SaveFileHistory = yes"), std::string::npos);
  EXPECT_NE(created_text.find("FileHistorySize = 87654"), std::string::npos);
  EXPECT_NE(created_text.find("FdPluginOptions = \"fd=defs\""),
            std::string::npos);
  EXPECT_NE(created_text.find("SdPluginOptions = \"sd=defs\""),
            std::string::npos);
  EXPECT_NE(created_text.find("DirPluginOptions = \"dir=defs\""),
            std::string::npos);
  EXPECT_NE(created_text.find("MaxConcurrentCopies = 6"), std::string::npos);
  EXPECT_NE(created_text.find("AlwaysIncremental = yes"), std::string::npos);
  EXPECT_NE(created_text.find("AlwaysIncrementalJobRetention = 10800"),
            std::string::npos);
  EXPECT_NE(created_text.find("Enabled = yes"), std::string::npos);

  const auto default_jobdefs_path
      = created.value->path / "bareos-dir.d/jobdefs/DefaultJob.conf";
  auto default_jobdefs_text = ReadTextFile(default_jobdefs_path);
  const std::string default_jobdefs_insertion
      = "  BackupFormat = \"Portable\"\n"
        "  Run = imported-jobdefs-run\n"
        "  FdPluginOptions = \"fd=imported-defs\"\n"
        "  SdPluginOptions = \"sd=imported-defs\"\n"
        "  DirPluginOptions = \"dir=imported-defs\"\n"
        "  MaximumBandwidth = 34567\n"
        "  MaxRunSchedTime = 95\n"
        "  MaxRunTime = 210\n"
        "  MaxFullInterval = 6300\n"
        "  PrefixLinks = yes\n"
        "  PruneJobs = yes\n"
        "  SpoolAttributes = yes\n"
        "  SpoolData = no\n"
        "  SpoolSize = 76543\n"
        "  MaximumConcurrentJobs = 7\n"
        "  RescheduleOnError = yes\n"
        "  RescheduleInterval = 900\n"
        "  RescheduleTimes = 8\n"
        "  AllowMixedPriority = yes\n"
        "  Accurate = yes\n"
        "  AllowDuplicateJobs = no\n"
        "  SaveFileHistory = yes\n"
        "  FileHistorySize = 76543\n"
        "  MaxConcurrentCopies = 9\n"
        "  AlwaysIncremental = yes\n"
        "  AlwaysIncrementalJobRetention = 21600\n";
  auto default_jobdefs_brace = default_jobdefs_text.rfind("}\n");
  ASSERT_NE(default_jobdefs_brace, std::string::npos);
  default_jobdefs_text.insert(default_jobdefs_brace, default_jobdefs_insertion);
  WriteTextFile(default_jobdefs_path, default_jobdefs_text);

  auto updated = state.UpsertDirectorJobDefsResource(
      "prod", "bareos-dir", "DefaultJob",
      {.description = std::string{"Updated default jobdefs"}});
  ASSERT_TRUE(updated) << updated.error;

  const auto updated_text = ReadTextFile(
      updated.value->path / "bareos-dir.d/jobdefs/DefaultJob.conf");
  EXPECT_NE(updated_text.find("Description = \"Updated default jobdefs\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("WriteBootstrap = "), std::string::npos);
  EXPECT_NE(updated_text.find("FullBackupPool = Full"), std::string::npos);
  EXPECT_NE(updated_text.find("DifferentialBackupPool = Differential"),
            std::string::npos);
  EXPECT_NE(updated_text.find("IncrementalBackupPool = Incremental"),
            std::string::npos);
  EXPECT_NE(updated_text.find("BackupFormat = \"Portable\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("Run = imported-jobdefs-run"), std::string::npos);
  EXPECT_NE(updated_text.find("FdPluginOptions = \"fd=imported-defs\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("SdPluginOptions = \"sd=imported-defs\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("DirPluginOptions = \"dir=imported-defs\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("MaximumBandwidth = 34567"), std::string::npos);
  EXPECT_NE(updated_text.find("MaxRunSchedTime = 95"), std::string::npos);
  EXPECT_NE(updated_text.find("MaxRunTime = 210"), std::string::npos);
  EXPECT_NE(updated_text.find("MaxFullInterval = 6300"), std::string::npos);
  EXPECT_NE(updated_text.find("PrefixLinks = yes"), std::string::npos);
  EXPECT_NE(updated_text.find("PruneJobs = yes"), std::string::npos);
  EXPECT_NE(updated_text.find("SpoolAttributes = yes"), std::string::npos);
  EXPECT_NE(updated_text.find("SpoolData = no"), std::string::npos);
  EXPECT_NE(updated_text.find("SpoolSize = 76543"), std::string::npos);
  EXPECT_NE(updated_text.find("MaximumConcurrentJobs = 7"), std::string::npos);
  EXPECT_NE(updated_text.find("RescheduleOnError = yes"), std::string::npos);
  EXPECT_NE(updated_text.find("RescheduleInterval = 900"), std::string::npos);
  EXPECT_NE(updated_text.find("RescheduleTimes = 8"), std::string::npos);
  EXPECT_NE(updated_text.find("AllowMixedPriority = yes"), std::string::npos);
  EXPECT_NE(updated_text.find("Accurate = yes"), std::string::npos);
  EXPECT_NE(updated_text.find("AllowDuplicateJobs = no"), std::string::npos);
  EXPECT_NE(updated_text.find("SaveFileHistory = yes"), std::string::npos);
  EXPECT_NE(updated_text.find("FileHistorySize = 76543"), std::string::npos);
  EXPECT_NE(updated_text.find("MaxConcurrentCopies = 9"), std::string::npos);
  EXPECT_NE(updated_text.find("AlwaysIncremental = yes"), std::string::npos);
  EXPECT_NE(updated_text.find("AlwaysIncrementalJobRetention = 21600"),
            std::string::npos);
}

TEST(BconfigService, UpsertsDirectorJobDefsResourcesInSharedFiles)
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

  const auto jobdefs_directory
      = RepositoryLayout::DirectorsDirectory(repo_path.path())
        / "bareos-dir/bareos-dir.d/jobdefs";
  const auto original_path = jobdefs_directory / "DefaultJob.conf";
  const auto shared_path = jobdefs_directory / "shared.conf";
  const auto original_text = ReadTextFile(original_path);
  WriteTextFile(shared_path,
                original_text
                    + "\nJobDefs {\n"
                      "  Name = \"OtherJobDefs\"\n"
                      "  Client = bareos-fd\n"
                      "}\n");
  std::filesystem::remove(original_path);

  auto updated = state.UpsertDirectorJobDefsResource(
      "prod", "bareos-dir", "DefaultJob",
      {.description = std::string{"Updated default jobdefs"}});
  ASSERT_TRUE(updated) << updated.error;
  EXPECT_FALSE(std::filesystem::exists(original_path));
  EXPECT_TRUE(std::filesystem::exists(shared_path));
  const auto shared_text = ReadTextFile(shared_path);
  EXPECT_NE(shared_text.find("Description = \"Updated default jobdefs\""),
            std::string::npos);
  EXPECT_NE(shared_text.find("Name = \"OtherJobDefs\""), std::string::npos);
}

TEST(BconfigService, DeletesDirectorJobDefsResources)
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

  auto created = state.UpsertDirectorJobDefsResource(
      "prod", "bareos-dir", "ManagedJobDefs",
      {.client = std::string{"bareos-fd"},
       .fileset = std::string{"SelfTest"},
       .schedule = std::string{"WeeklyCycle"}});
  ASSERT_TRUE(created) << created.error;

  const auto jobdefs_path
      = created.value->path / "bareos-dir.d/jobdefs/ManagedJobDefs.conf";
  ASSERT_TRUE(std::filesystem::exists(jobdefs_path));

  auto deleted = state.DeleteDirectorJobDefsResource("prod", "bareos-dir",
                                                     "ManagedJobDefs");
  ASSERT_TRUE(deleted) << deleted.error;
  EXPECT_FALSE(std::filesystem::exists(jobdefs_path));
}

TEST(BconfigService, DeletesDirectorJobDefsResourcesFromSharedFiles)
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

  const auto jobdefs_directory
      = RepositoryLayout::DirectorsDirectory(repo_path.path())
        / "bareos-dir/bareos-dir.d/jobdefs";
  const auto original_path = jobdefs_directory / "DefaultJob.conf";
  const auto shared_path = jobdefs_directory / "shared.conf";
  const auto original_text = ReadTextFile(original_path);
  WriteTextFile(shared_path,
                original_text
                    + "\nJobDefs {\n"
                      "  Name = \"OtherJobDefs\"\n"
                      "  Client = bareos-fd\n"
                      "}\n");
  std::filesystem::remove(original_path);

  auto deleted = state.DeleteDirectorJobDefsResource("prod", "bareos-dir",
                                                     "OtherJobDefs");
  ASSERT_TRUE(deleted) << deleted.error;
  EXPECT_FALSE(std::filesystem::exists(original_path));
  EXPECT_TRUE(std::filesystem::exists(shared_path));
  const auto shared_text = ReadTextFile(shared_path);
  EXPECT_NE(shared_text.find("Write Bootstrap = "), std::string::npos);
  EXPECT_EQ(shared_text.find("Name = \"OtherJobDefs\""), std::string::npos);
}

TEST(BconfigService, UpsertsDirectorMessagesResources)
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

  auto created = state.UpsertDirectorMessagesResource(
      "prod", "bareos-dir", "ManagedMessages",
      {.description = std::string{"Managed messages"},
       .entries = std::vector<std::string>{
           "  console = all, !skipped, !saved, !audit"}});
  ASSERT_TRUE(created) << created.error;
  EXPECT_EQ(created.value->name, "bareos-dir");

  const auto messages_path
      = created.value->path / "bareos-dir.d/messages/ManagedMessages.conf";
  const auto created_text = ReadTextFile(messages_path);
  EXPECT_NE(created_text.find("Messages {"), std::string::npos);
  EXPECT_NE(created_text.find("Name = \"ManagedMessages\""), std::string::npos);
  EXPECT_NE(created_text.find("Description = \"Managed messages\""),
            std::string::npos);
  EXPECT_NE(created_text.find("console = all, !skipped, !saved, !audit"),
            std::string::npos);

  auto updated = state.UpsertDirectorMessagesResource(
      "prod", "bareos-dir", "Standard",
      {.description = std::string{"Updated standard messages"}});
  ASSERT_TRUE(updated) << updated.error;

  const auto updated_text = ReadTextFile(
      updated.value->path / "bareos-dir.d/messages/Standard.conf");
  EXPECT_NE(updated_text.find("Description = \"Updated standard messages\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("operatorcommand = "), std::string::npos);
  EXPECT_NE(updated_text.find("mailcommand = "), std::string::npos);
  EXPECT_NE(updated_text.find("append = "), std::string::npos);
}

TEST(BconfigService, UpsertsDirectorMessagesResourcesInSharedFiles)
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

  const auto messages_directory
      = RepositoryLayout::DirectorsDirectory(repo_path.path())
        / "bareos-dir/bareos-dir.d/messages";
  const auto original_path = messages_directory / "Standard.conf";
  const auto shared_path = messages_directory / "shared.conf";
  const auto original_text = ReadTextFile(original_path);
  WriteTextFile(shared_path,
                original_text
                    + "\nMessages {\n"
                      "  Name = \"OtherMessages\"\n"
                      "  console = all, !skipped, !saved, !audit\n"
                      "}\n");
  std::filesystem::remove(original_path);

  auto updated = state.UpsertDirectorMessagesResource(
      "prod", "bareos-dir", "Standard",
      {.description = std::string{"Updated standard messages"}});
  ASSERT_TRUE(updated) << updated.error;
  EXPECT_FALSE(std::filesystem::exists(original_path));
  EXPECT_TRUE(std::filesystem::exists(shared_path));
  const auto shared_text = ReadTextFile(shared_path);
  EXPECT_NE(shared_text.find("Description = \"Updated standard messages\""),
            std::string::npos);
  EXPECT_NE(shared_text.find("Name = \"OtherMessages\""), std::string::npos);
}

TEST(BconfigService, DeletesDirectorMessagesResources)
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

  auto created = state.UpsertDirectorMessagesResource(
      "prod", "bareos-dir", "ManagedMessages",
      {.entries = std::vector<std::string>{
           "  console = all, !skipped, !saved, !audit"}});
  ASSERT_TRUE(created) << created.error;

  const auto messages_path
      = created.value->path / "bareos-dir.d/messages/ManagedMessages.conf";
  ASSERT_TRUE(std::filesystem::exists(messages_path));

  auto deleted = state.DeleteDirectorMessagesResource("prod", "bareos-dir",
                                                      "ManagedMessages");
  ASSERT_TRUE(deleted) << deleted.error;
  EXPECT_FALSE(std::filesystem::exists(messages_path));
}

TEST(BconfigService, DeletesDirectorMessagesResourcesFromSharedFiles)
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

  const auto messages_directory
      = RepositoryLayout::DirectorsDirectory(repo_path.path())
        / "bareos-dir/bareos-dir.d/messages";
  const auto original_path = messages_directory / "Standard.conf";
  const auto shared_path = messages_directory / "shared.conf";
  const auto original_text = ReadTextFile(original_path);
  WriteTextFile(shared_path,
                original_text
                    + "\nMessages {\n"
                      "  Name = \"OtherMessages\"\n"
                      "  console = all, !skipped, !saved, !audit\n"
                      "}\n");
  std::filesystem::remove(original_path);

  auto deleted = state.DeleteDirectorMessagesResource("prod", "bareos-dir",
                                                      "OtherMessages");
  ASSERT_TRUE(deleted) << deleted.error;
  EXPECT_FALSE(std::filesystem::exists(original_path));
  EXPECT_TRUE(std::filesystem::exists(shared_path));
  const auto shared_text = ReadTextFile(shared_path);
  EXPECT_NE(shared_text.find("console = all, !skipped, !saved, !audit"),
            std::string::npos);
  EXPECT_EQ(shared_text.find("Name = \"OtherMessages\""), std::string::npos);
}

}  // namespace
}  // namespace bconfig::service
