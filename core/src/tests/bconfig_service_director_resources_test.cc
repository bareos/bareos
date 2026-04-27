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

  auto current = state.GetDirectorConsoleResourceSpec("prod", "bareos-dir",
                                                      "bareos-mon");
  ASSERT_TRUE(current) << current.error;
  ASSERT_TRUE(current.value->password.has_value());
  EXPECT_TRUE(current.value->password->starts_with("[md5]"));
  EXPECT_EQ(current.value->description, "Updated restricted console");
  ASSERT_TRUE(current.value->job_acl.has_value());
  EXPECT_EQ(*current.value->job_acl, (std::vector<std::string>{"*all*"}));
  ASSERT_TRUE(current.value->command_acl.has_value());
  EXPECT_EQ(*current.value->command_acl,
            (std::vector<std::string>{"status", ".status", "show"}));
  ASSERT_TRUE(current.value->profiles.has_value());
  EXPECT_EQ(*current.value->profiles, (std::vector<std::string>{"operator"}));
  EXPECT_EQ(current.value->tls_enable, false);
  EXPECT_EQ(current.value->tls_protocol, "TLSv1.2");
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

  auto created_spec
      = state.GetDirectorUserResourceSpec("prod", "bareos-dir", "managed-user");
  ASSERT_TRUE(created_spec) << created_spec.error;
  EXPECT_EQ(created_spec.value->description, "Managed user");
  ASSERT_TRUE(created_spec.value->command_acl);
  EXPECT_EQ(*created_spec.value->command_acl,
            (std::vector<std::string>{"status", ".status"}));
  ASSERT_TRUE(created_spec.value->profiles);
  EXPECT_EQ(*created_spec.value->profiles,
            (std::vector<std::string>{"operator"}));

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

  auto updated_spec = state.GetDirectorUserResourceSpec("prod", "bareos-dir",
                                                        "operator-user");
  ASSERT_TRUE(updated_spec) << updated_spec.error;
  EXPECT_EQ(updated_spec.value->description, "Updated operator user");
  ASSERT_TRUE(updated_spec.value->command_acl);
  EXPECT_EQ(*updated_spec.value->command_acl,
            (std::vector<std::string>{"list", "llist"}));
  ASSERT_TRUE(updated_spec.value->profiles);
  EXPECT_EQ(*updated_spec.value->profiles,
            (std::vector<std::string>{"operator"}));
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

  auto created_spec = state.GetDirectorProfileResourceSpec("prod", "bareos-dir",
                                                           "managed-profile");
  ASSERT_TRUE(created_spec) << created_spec.error;
  EXPECT_EQ(created_spec.value->description, "Managed profile");
  ASSERT_TRUE(created_spec.value->command_acl);
  EXPECT_EQ(*created_spec.value->command_acl,
            (std::vector<std::string>{"status", ".status"}));
  ASSERT_TRUE(created_spec.value->catalog_acl);
  EXPECT_EQ(*created_spec.value->catalog_acl,
            (std::vector<std::string>{"managed-catalog"}));

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

  auto updated_spec
      = state.GetDirectorProfileResourceSpec("prod", "bareos-dir", "operator");
  ASSERT_TRUE(updated_spec) << updated_spec.error;
  EXPECT_EQ(updated_spec.value->description, "Updated operator profile");
  ASSERT_TRUE(updated_spec.value->command_acl);
  EXPECT_EQ(*updated_spec.value->command_acl,
            (std::vector<std::string>{"list", "llist"}));
  ASSERT_TRUE(updated_spec.value->catalog_acl);
  EXPECT_EQ(*updated_spec.value->catalog_acl,
            (std::vector<std::string>{"operator-catalog"}));
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

  auto created_spec
      = state.GetDirectorPoolResourceSpec("prod", "bareos-dir", "managed-pool");
  ASSERT_TRUE(created_spec) << created_spec.error;
  EXPECT_EQ(created_spec.value->pool_type, "Scratch");
  EXPECT_EQ(created_spec.value->label_format, "Managed-");
  EXPECT_EQ(created_spec.value->cleaning_prefix, "Cleaner-");
  EXPECT_EQ(created_spec.value->label_type, "ansi");
  EXPECT_EQ(created_spec.value->maximum_volumes, 7);
  EXPECT_EQ(created_spec.value->maximum_volume_jobs, 8);
  EXPECT_EQ(created_spec.value->maximum_volume_files, 9);
  EXPECT_EQ(created_spec.value->maximum_volume_bytes, 123456);
  EXPECT_EQ(created_spec.value->volume_retention, 86400);
  EXPECT_EQ(created_spec.value->volume_use_duration, 43200);
  EXPECT_EQ(created_spec.value->migration_time, 3600);
  EXPECT_EQ(created_spec.value->migration_high_bytes, 654321);
  EXPECT_EQ(created_spec.value->migration_low_bytes, 12345);
  EXPECT_EQ(created_spec.value->next_pool, "Incremental");
  ASSERT_TRUE(created_spec.value->storages.has_value());
  EXPECT_EQ(*created_spec.value->storages, (std::vector<std::string>{"File"}));
  EXPECT_EQ(created_spec.value->use_catalog, true);
  EXPECT_EQ(created_spec.value->catalog_files, true);
  EXPECT_EQ(created_spec.value->purge_oldest_volume, true);
  EXPECT_EQ(created_spec.value->action_on_purge, "Truncate");
  EXPECT_EQ(created_spec.value->recycle_oldest_volume, true);
  EXPECT_EQ(created_spec.value->recycle_current_volume, false);
  EXPECT_EQ(created_spec.value->auto_prune, true);
  EXPECT_EQ(created_spec.value->recycle, false);
  EXPECT_EQ(created_spec.value->recycle_pool, "Scratch");
  EXPECT_EQ(created_spec.value->scratch_pool, "Scratch");
  EXPECT_EQ(created_spec.value->catalog, "MyCatalog");
  EXPECT_EQ(created_spec.value->file_retention, 172800);
  EXPECT_EQ(created_spec.value->job_retention, 259200);
  EXPECT_EQ(created_spec.value->minimum_block_size, 512);
  EXPECT_EQ(created_spec.value->maximum_block_size, 4096);
  EXPECT_EQ(created_spec.value->description, "Managed pool");

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

  auto updated_spec
      = state.GetDirectorPoolResourceSpec("prod", "bareos-dir", "Full");
  ASSERT_TRUE(updated_spec) << updated_spec.error;
  EXPECT_EQ(updated_spec.value->description, "Updated full pool");
  EXPECT_EQ(updated_spec.value->pool_type, "Backup");
  EXPECT_EQ(updated_spec.value->recycle, true);
  EXPECT_EQ(updated_spec.value->auto_prune, true);
  EXPECT_EQ(updated_spec.value->volume_retention, 31536000);
  EXPECT_EQ(updated_spec.value->maximum_volume_bytes, 53687091200);
  EXPECT_EQ(updated_spec.value->maximum_volumes, 100);
  EXPECT_EQ(updated_spec.value->label_format, "Full-");
  EXPECT_EQ(updated_spec.value->use_catalog, true);
  EXPECT_EQ(updated_spec.value->catalog_files, true);
  EXPECT_EQ(updated_spec.value->purge_oldest_volume, true);
  EXPECT_EQ(updated_spec.value->action_on_purge, "Truncate");
  EXPECT_EQ(updated_spec.value->recycle_oldest_volume, true);
  EXPECT_EQ(updated_spec.value->recycle_current_volume, false);
  EXPECT_EQ(updated_spec.value->maximum_volume_jobs, 11);
  EXPECT_EQ(updated_spec.value->maximum_volume_files, 12);
  EXPECT_EQ(updated_spec.value->volume_use_duration, 1800);
  EXPECT_EQ(updated_spec.value->migration_time, 2700);
  EXPECT_EQ(updated_spec.value->migration_high_bytes, 2000);
  EXPECT_EQ(updated_spec.value->migration_low_bytes, 1000);
  EXPECT_EQ(updated_spec.value->next_pool, "Incremental");
  ASSERT_TRUE(updated_spec.value->storages.has_value());
  EXPECT_EQ(*updated_spec.value->storages, (std::vector<std::string>{"File"}));
  EXPECT_EQ(updated_spec.value->cleaning_prefix, "clean-");
  EXPECT_EQ(updated_spec.value->label_type, "ansi");
  EXPECT_EQ(updated_spec.value->recycle_pool, "Scratch");
  EXPECT_EQ(updated_spec.value->scratch_pool, "Scratch");
  EXPECT_EQ(updated_spec.value->catalog, "MyCatalog");
  EXPECT_EQ(updated_spec.value->file_retention, 604800);
  EXPECT_EQ(updated_spec.value->job_retention, 1209600);
  EXPECT_EQ(updated_spec.value->minimum_block_size, 1024);
  EXPECT_EQ(updated_spec.value->maximum_block_size, 8192);
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

  auto created_spec = state.GetDirectorCatalogResourceSpec("prod", "bareos-dir",
                                                           "ManagedCatalog");
  ASSERT_TRUE(created_spec) << created_spec.error;
  EXPECT_EQ(created_spec.value->db_address, "127.0.0.1");
  EXPECT_EQ(created_spec.value->db_port, 5432);
  EXPECT_EQ(created_spec.value->db_password, "secret");
  EXPECT_EQ(created_spec.value->db_user, "bareos");
  EXPECT_EQ(created_spec.value->db_name, "bareos_catalog");
  EXPECT_EQ(created_spec.value->multiple_connections, true);
  EXPECT_EQ(created_spec.value->disable_batch_insert, true);
  EXPECT_EQ(created_spec.value->reconnect, true);
  EXPECT_EQ(created_spec.value->exit_on_fatal, false);
  EXPECT_EQ(created_spec.value->description, "Managed catalog");

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

  auto updated_spec
      = state.GetDirectorCatalogResourceSpec("prod", "bareos-dir", "MyCatalog");
  ASSERT_TRUE(updated_spec) << updated_spec.error;
  EXPECT_EQ(updated_spec.value->db_user, "regress");
  EXPECT_EQ(updated_spec.value->db_name, "regress_backup_bareos_test");
  EXPECT_TRUE(updated_spec.value->db_password.has_value());
  EXPECT_EQ(updated_spec.value->multiple_connections, false);
  EXPECT_EQ(updated_spec.value->disable_batch_insert, true);
  EXPECT_EQ(updated_spec.value->description, "Updated catalog");
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
  auto created_spec = state.GetDirectorScheduleResourceSpec(
      "prod", "bareos-dir", "ManagedSchedule");
  ASSERT_TRUE(created_spec) << created_spec.error;
  EXPECT_EQ(created_spec.value->description,
            std::optional<std::string>{"Managed schedule"});
  EXPECT_EQ(created_spec.value->enabled, std::optional<bool>{true});
  ASSERT_TRUE(created_spec.value->run_entries.has_value());
  EXPECT_EQ(*created_spec.value->run_entries,
            std::vector<std::string>{"Level=Full monthly 1st sat at 21:00"});

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
  auto updated_spec = state.GetDirectorScheduleResourceSpec(
      "prod", "bareos-dir", "Odd Weeks");
  ASSERT_TRUE(updated_spec) << updated_spec.error;
  EXPECT_EQ(updated_spec.value->description,
            std::optional<std::string>{"Updated odd weeks"});
  EXPECT_EQ(updated_spec.value->enabled, std::optional<bool>{true});
  ASSERT_TRUE(updated_spec.value->run_entries.has_value());
  EXPECT_EQ(*updated_spec.value->run_entries,
            std::vector<std::string>{"w01/w02 sun at 23:10"});
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
  EXPECT_NE(shared_text.find("Enabled = yes"), std::string::npos);
  EXPECT_NE(shared_text.find("Run = w01/w02 sun at 23:10"), std::string::npos);
  EXPECT_NE(shared_text.find("Name = \"OtherSchedule\""), std::string::npos);
  auto updated_spec = state.GetDirectorScheduleResourceSpec(
      "prod", "bareos-dir", "Odd Weeks");
  ASSERT_TRUE(updated_spec) << updated_spec.error;
  EXPECT_EQ(updated_spec.value->description,
            std::optional<std::string>{"Updated odd weeks"});
  EXPECT_EQ(updated_spec.value->enabled, std::optional<bool>{true});
  ASSERT_TRUE(updated_spec.value->run_entries.has_value());
  EXPECT_EQ(*updated_spec.value->run_entries,
            std::vector<std::string>{"w01/w02 sun at 23:10"});
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

  auto created_spec = state.GetDirectorCounterResourceSpec("prod", "bareos-dir",
                                                           "ManagedCounter");
  ASSERT_TRUE(created_spec) << created_spec.error;
  EXPECT_EQ(created_spec.value->minimum, 10);
  EXPECT_EQ(created_spec.value->maximum, 50);
  EXPECT_EQ(created_spec.value->wrap_counter, "WrapSeed");
  EXPECT_EQ(created_spec.value->catalog, "MyCatalog");
  EXPECT_EQ(created_spec.value->description, "Managed counter");

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

  auto updated_spec = state.GetDirectorCounterResourceSpec("prod", "bareos-dir",
                                                           "ExistingCounter");
  ASSERT_TRUE(updated_spec) << updated_spec.error;
  EXPECT_EQ(updated_spec.value->minimum, 7);
  EXPECT_EQ(updated_spec.value->maximum, 99);
  EXPECT_EQ(updated_spec.value->wrap_counter, "WrapSeed");
  EXPECT_EQ(updated_spec.value->catalog, "MyCatalog");
  EXPECT_EQ(updated_spec.value->description, "Updated counter");
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
  EXPECT_NE(shared_text.find("WrapCounter = WrapSeed"), std::string::npos);
  EXPECT_NE(shared_text.find("Catalog = MyCatalog"), std::string::npos);
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
  auto created_spec = state.GetDirectorFilesetResourceSpec("prod", "bareos-dir",
                                                           "ManagedFileSet");
  ASSERT_TRUE(created_spec) << created_spec.error;
  EXPECT_EQ(created_spec.value->description,
            std::optional<std::string>{"Managed fileset"});
  EXPECT_EQ(created_spec.value->ignore_fileset_changes,
            std::optional<bool>{true});
  EXPECT_EQ(created_spec.value->enable_vss, std::optional<bool>{false});
  ASSERT_TRUE(created_spec.value->include_blocks.has_value());
  EXPECT_EQ(*created_spec.value->include_blocks,
            (std::vector<std::string>{"  Include {\n"
                                      "    Options {\n"
                                      "      Signature = XXH128\n"
                                      "    }\n"
                                      "    File = "
                                      "/tmp/tests/backup-bareos-test/tmp\n"
                                      "  }\n"}));
  ASSERT_TRUE(created_spec.value->exclude_blocks.has_value());
  EXPECT_EQ(
      *created_spec.value->exclude_blocks,
      (std::vector<std::string>{"  Exclude {\n"
                                "    File = "
                                "/tmp/tests/backup-bareos-test/tmp/cache\n"
                                "  }\n"}));

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
  auto updated_spec
      = state.GetDirectorFilesetResourceSpec("prod", "bareos-dir", "LinuxAll");
  ASSERT_TRUE(updated_spec) << updated_spec.error;
  EXPECT_EQ(updated_spec.value->description,
            std::optional<std::string>{"Updated LinuxAll fileset"});
  EXPECT_EQ(updated_spec.value->ignore_fileset_changes,
            std::optional<bool>{false});
  EXPECT_EQ(updated_spec.value->enable_vss, std::optional<bool>{true});
  ASSERT_TRUE(updated_spec.value->include_blocks.has_value());
  ASSERT_EQ(updated_spec.value->include_blocks->size(), 1U);
  EXPECT_TRUE(
      updated_spec.value->include_blocks->front().starts_with("  Include {\n"));
  EXPECT_NE(
      updated_spec.value->include_blocks->front().find("Signature = XXH128"),
      std::string::npos);
  EXPECT_NE(updated_spec.value->include_blocks->front().find("FS Type = zfs"),
            std::string::npos);
  EXPECT_NE(updated_spec.value->include_blocks->front().find("File = /\n"),
            std::string::npos);
  ASSERT_TRUE(updated_spec.value->exclude_blocks.has_value());
  ASSERT_EQ(updated_spec.value->exclude_blocks->size(), 1U);
  EXPECT_TRUE(
      updated_spec.value->exclude_blocks->front().starts_with("  Exclude {\n"));
  EXPECT_NE(updated_spec.value->exclude_blocks->front().find(
                "File = /tmp/tests/backup-bareos-test/working"),
            std::string::npos);
  EXPECT_NE(updated_spec.value->exclude_blocks->front().find("File = /.fsck"),
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
  EXPECT_NE(
      shared_text.find("Include {\n    Options {\n      Signature = XXH128"),
      std::string::npos);
  EXPECT_NE(shared_text.find(
                "Exclude {\n    File = /tmp/tests/backup-bareos-test/working"),
            std::string::npos);
  EXPECT_NE(shared_text.find("Name = \"OtherFileSet\""), std::string::npos);
  auto updated_spec
      = state.GetDirectorFilesetResourceSpec("prod", "bareos-dir", "LinuxAll");
  ASSERT_TRUE(updated_spec) << updated_spec.error;
  EXPECT_EQ(updated_spec.value->description,
            std::optional<std::string>{"Updated LinuxAll fileset"});
  EXPECT_EQ(updated_spec.value->ignore_fileset_changes,
            std::optional<bool>{false});
  EXPECT_EQ(updated_spec.value->enable_vss, std::optional<bool>{true});
  ASSERT_TRUE(updated_spec.value->include_blocks.has_value());
  ASSERT_EQ(updated_spec.value->include_blocks->size(), 1U);
  EXPECT_TRUE(
      updated_spec.value->include_blocks->front().starts_with("  Include {\n"));
  EXPECT_NE(
      updated_spec.value->include_blocks->front().find("Signature = XXH128"),
      std::string::npos);
  EXPECT_NE(updated_spec.value->include_blocks->front().find("FS Type = zfs"),
            std::string::npos);
  EXPECT_NE(updated_spec.value->include_blocks->front().find("File = /\n"),
            std::string::npos);
  ASSERT_TRUE(updated_spec.value->exclude_blocks.has_value());
  ASSERT_EQ(updated_spec.value->exclude_blocks->size(), 1U);
  EXPECT_TRUE(
      updated_spec.value->exclude_blocks->front().starts_with("  Exclude {\n"));
  EXPECT_NE(updated_spec.value->exclude_blocks->front().find(
                "File = /tmp/tests/backup-bareos-test/working"),
            std::string::npos);
  EXPECT_NE(updated_spec.value->exclude_blocks->front().find("File = /.fsck"),
            std::string::npos);
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
       .protocol = std::string{"ndmp"},
       .client = std::string{"bareos-fd"},
       .jobdefs = std::string{"DefaultJob"},
       .run_entries = std::vector<std::string>{"catalog-backup"},
       .run_before_job_entries
       = std::vector<std::string>{"/usr/lib/bareos/scripts/job-before"},
       .run_after_job_entries
       = std::vector<std::string>{"/usr/lib/bareos/scripts/job-after"},
       .run_after_failed_job_entries
       = std::vector<std::string>{"/usr/lib/bareos/scripts/job-failed"},
       .client_run_before_job_entries
       = std::vector<std::string>{"/usr/lib/bareos/scripts/client-before"},
       .client_run_after_job_entries
       = std::vector<std::string>{"/usr/lib/bareos/scripts/client-after"},
       .runscript_blocks
       = std::vector<std::string>{"  RunScript {\n"
                                  "    Console = \"status dir\"\n"
                                  "    RunsWhen = After\n"
                                  "    RunsOnFailure = yes\n"
                                  "  }\n"},
       .replace = std::string{"ifolder"},
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
       .selection_type = std::string{"pooltime"},
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
       .always_incremental_keep_number = 11,
       .always_incremental_max_full_age = 86400,
       .max_full_consolidations = 12,
       .run_on_incoming_connect_interval = 1800,
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
  EXPECT_NE(created_text.find("Protocol = NDMP_BAREOS"), std::string::npos);
  EXPECT_NE(created_text.find("JobDefs = DefaultJob"), std::string::npos);
  EXPECT_NE(created_text.find("Client = bareos-fd"), std::string::npos);
  EXPECT_NE(created_text.find("Run = catalog-backup"), std::string::npos);
  EXPECT_NE(created_text.find(
                "RunBeforeJob = \"/usr/lib/bareos/scripts/job-before\""),
            std::string::npos);
  EXPECT_NE(
      created_text.find("RunAfterJob = \"/usr/lib/bareos/scripts/job-after\""),
      std::string::npos);
  EXPECT_NE(created_text.find(
                "RunAfterFailedJob = \"/usr/lib/bareos/scripts/job-failed\""),
            std::string::npos);
  EXPECT_NE(
      created_text.find(
          "ClientRunBeforeJob = \"/usr/lib/bareos/scripts/client-before\""),
      std::string::npos);
  EXPECT_NE(created_text.find(
                "ClientRunAfterJob = \"/usr/lib/bareos/scripts/client-after\""),
            std::string::npos);
  EXPECT_NE(created_text.find("RunScript {\n"
                              "    Console = \"status dir\"\n"
                              "    RunsWhen = After\n"
                              "    RunsOnFailure = yes\n"
                              "  }"),
            std::string::npos);
  EXPECT_NE(created_text.find("Replace = IfOlder"), std::string::npos);
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
  EXPECT_NE(created_text.find("SelectionType = PoolTime"), std::string::npos);
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
  EXPECT_NE(created_text.find("AlwaysIncrementalKeepNumber = 11"),
            std::string::npos);
  EXPECT_NE(created_text.find("AlwaysIncrementalMaxFullAge = 86400"),
            std::string::npos);
  EXPECT_NE(created_text.find("MaxFullConsolidations = 12"), std::string::npos);
  EXPECT_NE(created_text.find("RunOnIncomingConnectInterval = 1800"),
            std::string::npos);
  EXPECT_NE(created_text.find("Enabled = yes"), std::string::npos);

  auto created_spec
      = state.GetDirectorJobResourceSpec("prod", "bareos-dir", "ManagedJob");
  ASSERT_TRUE(created_spec) << created_spec.error;
  EXPECT_EQ(created_spec.value->description, "Managed job");
  EXPECT_EQ(created_spec.value->protocol, "NDMP_BAREOS");
  EXPECT_EQ(created_spec.value->jobdefs, "DefaultJob");
  ASSERT_TRUE(created_spec.value->run_before_job_entries);
  EXPECT_EQ(*created_spec.value->run_before_job_entries,
            (std::vector<std::string>{"/usr/lib/bareos/scripts/job-before"}));
  ASSERT_TRUE(created_spec.value->runscript_blocks);
  EXPECT_EQ(created_spec.value->runscript_blocks->size(), 1U);
  EXPECT_EQ(created_spec.value->replace, "IfOlder");
  EXPECT_EQ(created_spec.value->maximum_concurrent_jobs, 3);
  ASSERT_TRUE(created_spec.value->fd_plugin_options);
  EXPECT_EQ(*created_spec.value->fd_plugin_options,
            (std::vector<std::string>{"fd=one"}));
  EXPECT_EQ(created_spec.value->always_incremental, true);

  const auto backup_catalog_path
      = created.value->path / "bareos-dir.d/job/BackupCatalog.conf";
  auto backup_catalog_text = ReadTextFile(backup_catalog_path);
  const std::string backup_catalog_insertion
      = "  BackupFormat = \"Portable\"\n"
        "  Protocol = NDMP_NATIVE\n"
        "  Run = backup-catalog-now\n"
        "  RunBeforeJob = \"/tmp/scripts/make_catalog_backup MyCatalog\"\n"
        "  RunAfterJob = \"/tmp/scripts/delete_catalog_backup\"\n"
        "  RunAfterFailedJob = "
        "\"/tmp/scripts/report_catalog_backup_failure\"\n"
        "  ClientRunBeforeJob = "
        "\"/tmp/scripts/client_prepare_catalog_backup\"\n"
        "  ClientRunAfterJob = "
        "\"/tmp/scripts/client_finalize_catalog_backup\"\n"
        "  RunScript {\n"
        "    Console = \"llist jobid=1\"\n"
        "    RunsWhen = After\n"
        "    RunsOnFailure = yes\n"
        "  }\n"
        "  Replace = Never\n"
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
        "  SelectionType = Volume\n"
        "  Accurate = yes\n"
        "  AllowDuplicateJobs = no\n"
        "  SaveFileHistory = yes\n"
        "  FileHistorySize = 87654\n"
        "  MaxConcurrentCopies = 8\n"
        "  AlwaysIncremental = yes\n"
        "  AlwaysIncrementalJobRetention = 14400\n"
        "  AlwaysIncrementalKeepNumber = 21\n"
        "  AlwaysIncrementalMaxFullAge = 172800\n"
        "  MaxFullConsolidations = 22\n"
        "  RunOnIncomingConnectInterval = 3600\n";
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
  EXPECT_NE(
      updated_text.find("RunAfterJob = \"/tmp/scripts/delete_catalog_backup\""),
      std::string::npos);
  EXPECT_NE(updated_text.find("WriteBootstrap = "), std::string::npos);
  EXPECT_NE(updated_text.find("BackupFormat = \"Portable\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("Protocol = NDMP_NATIVE"), std::string::npos);
  EXPECT_NE(updated_text.find("Run = backup-catalog-now"), std::string::npos);
  EXPECT_NE(updated_text.find("RunAfterFailedJob = "
                              "\"/tmp/scripts/report_catalog_backup_failure\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("ClientRunBeforeJob = "
                              "\"/tmp/scripts/client_prepare_catalog_backup\""),
            std::string::npos);
  EXPECT_NE(
      updated_text.find("ClientRunAfterJob = "
                        "\"/tmp/scripts/client_finalize_catalog_backup\""),
      std::string::npos);
  EXPECT_NE(updated_text.find("RunScript {\n"
                              "    Console = \"llist jobid=1\"\n"
                              "    RunsWhen = After\n"
                              "    RunsOnFailure = yes\n"
                              "  }\n"),
            std::string::npos);
  EXPECT_NE(updated_text.find("Replace = Never"), std::string::npos);
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
  EXPECT_NE(updated_text.find("SelectionType = Volume"), std::string::npos);
  EXPECT_NE(updated_text.find("Accurate = yes"), std::string::npos);
  EXPECT_NE(updated_text.find("AllowDuplicateJobs = no"), std::string::npos);
  EXPECT_NE(updated_text.find("SaveFileHistory = yes"), std::string::npos);
  EXPECT_NE(updated_text.find("FileHistorySize = 87654"), std::string::npos);
  EXPECT_NE(updated_text.find("MaxConcurrentCopies = 8"), std::string::npos);
  EXPECT_NE(updated_text.find("AlwaysIncremental = yes"), std::string::npos);
  EXPECT_NE(updated_text.find("AlwaysIncrementalJobRetention = 14400"),
            std::string::npos);
  EXPECT_NE(updated_text.find("AlwaysIncrementalKeepNumber = 21"),
            std::string::npos);
  EXPECT_NE(updated_text.find("AlwaysIncrementalMaxFullAge = 172800"),
            std::string::npos);
  EXPECT_NE(updated_text.find("MaxFullConsolidations = 22"), std::string::npos);
  EXPECT_NE(updated_text.find("RunOnIncomingConnectInterval = 3600"),
            std::string::npos);

  auto updated_spec
      = state.GetDirectorJobResourceSpec("prod", "bareos-dir", "BackupCatalog");
  ASSERT_TRUE(updated_spec) << updated_spec.error;
  EXPECT_EQ(updated_spec.value->description, "Updated backup catalog job");
  EXPECT_EQ(updated_spec.value->backup_format, "Portable");
  EXPECT_EQ(updated_spec.value->protocol, "NDMP_NATIVE");
  ASSERT_TRUE(updated_spec.value->run_after_failed_job_entries);
  EXPECT_EQ(
      *updated_spec.value->run_after_failed_job_entries,
      (std::vector<std::string>{"/tmp/scripts/report_catalog_backup_failure"}));
  ASSERT_TRUE(updated_spec.value->runscript_blocks);
  EXPECT_EQ(updated_spec.value->runscript_blocks->size(), 1U);
  EXPECT_EQ(updated_spec.value->replace, "Never");
  EXPECT_EQ(updated_spec.value->selection_type, "Volume");
  ASSERT_TRUE(updated_spec.value->dir_plugin_options);
  EXPECT_EQ(*updated_spec.value->dir_plugin_options,
            (std::vector<std::string>{"dir=imported"}));
  EXPECT_EQ(updated_spec.value->max_concurrent_copies, 8);
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
  auto original_text = ReadTextFile(original_path);
  const auto original_brace = original_text.rfind("}\n");
  ASSERT_NE(original_brace, std::string::npos);
  original_text.insert(original_brace,
                       "  runafterfailedjob = "
                       "\"/tmp/scripts/shared-job-failed\"\n"
                       "  clientrunbeforejob = "
                       "\"/tmp/scripts/shared-client-before\"\n"
                       "  clientrunafterjob = "
                       "\"/tmp/scripts/shared-client-after\"\n"
                       "  runscript {\n"
                       "    console = \"status dir\"\n"
                       "    runswhen = after\n"
                       "  }\n");
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
  EXPECT_NE(shared_text.find(
                "RunAfterFailedJob = \"/tmp/scripts/shared-job-failed\""),
            std::string::npos);
  EXPECT_NE(shared_text.find(
                "ClientRunBeforeJob = \"/tmp/scripts/shared-client-before\""),
            std::string::npos);
  EXPECT_NE(shared_text.find(
                "ClientRunAfterJob = \"/tmp/scripts/shared-client-after\""),
            std::string::npos);
  EXPECT_NE(shared_text.find("runscript {\n"
                             "    console = \"status dir\"\n"
                             "    runswhen = after\n"
                             "  }\n"),
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
       .protocol = std::string{"ndmp"},
       .messages = std::string{"Standard"},
       .pool = std::string{"Incremental"},
       .client = std::string{"bareos-fd"},
       .fileset = std::string{"SelfTest"},
       .schedule = std::string{"WeeklyCycle"},
       .run_entries = std::vector<std::string>{"managed-jobdefs-run"},
       .run_before_job_entries
       = std::vector<std::string>{"/usr/lib/bareos/scripts/jobdefs-before"},
       .run_after_job_entries
       = std::vector<std::string>{"/usr/lib/bareos/scripts/jobdefs-after"},
       .run_after_failed_job_entries
       = std::vector<std::string>{"/usr/lib/bareos/scripts/jobdefs-failed"},
       .client_run_before_job_entries = std::vector<
           std::string>{"/usr/lib/bareos/scripts/jobdefs-client-before"},
       .client_run_after_job_entries = std::vector<
           std::string>{"/usr/lib/bareos/scripts/jobdefs-client-after"},
       .runscript_blocks
       = std::vector<std::string>{"  RunScript {\n"
                                  "    Command = \"/bin/true\"\n"
                                  "    Target = bareos-fd\n"
                                  "    RunsWhen = Before\n"
                                  "  }\n"},
       .replace = std::string{"ifolder"},
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
       .selection_type = std::string{"pooltime"},
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
       .always_incremental_keep_number = 13,
       .always_incremental_max_full_age = 90000,
       .max_full_consolidations = 14,
       .run_on_incoming_connect_interval = 2400,
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
  EXPECT_NE(created_text.find("Protocol = NDMP_BAREOS"), std::string::npos);
  EXPECT_NE(created_text.find("Client = bareos-fd"), std::string::npos);
  EXPECT_NE(created_text.find("Run = managed-jobdefs-run"), std::string::npos);
  EXPECT_NE(created_text.find(
                "RunBeforeJob = \"/usr/lib/bareos/scripts/jobdefs-before\""),
            std::string::npos);
  EXPECT_NE(created_text.find(
                "RunAfterJob = \"/usr/lib/bareos/scripts/jobdefs-after\""),
            std::string::npos);
  EXPECT_NE(
      created_text.find(
          "RunAfterFailedJob = \"/usr/lib/bareos/scripts/jobdefs-failed\""),
      std::string::npos);
  EXPECT_NE(
      created_text.find("ClientRunBeforeJob = "
                        "\"/usr/lib/bareos/scripts/jobdefs-client-before\""),
      std::string::npos);
  EXPECT_NE(
      created_text.find("ClientRunAfterJob = "
                        "\"/usr/lib/bareos/scripts/jobdefs-client-after\""),
      std::string::npos);
  EXPECT_NE(created_text.find("RunScript {\n"
                              "    Command = \"/bin/true\"\n"
                              "    Target = bareos-fd\n"
                              "    RunsWhen = Before\n"
                              "  }\n"),
            std::string::npos);
  EXPECT_NE(created_text.find("Replace = IfOlder"), std::string::npos);
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
  EXPECT_NE(created_text.find("SelectionType = PoolTime"), std::string::npos);
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
  EXPECT_NE(created_text.find("AlwaysIncrementalKeepNumber = 13"),
            std::string::npos);
  EXPECT_NE(created_text.find("AlwaysIncrementalMaxFullAge = 90000"),
            std::string::npos);
  EXPECT_NE(created_text.find("MaxFullConsolidations = 14"), std::string::npos);
  EXPECT_NE(created_text.find("RunOnIncomingConnectInterval = 2400"),
            std::string::npos);
  EXPECT_NE(created_text.find("Enabled = yes"), std::string::npos);

  auto created_spec = state.GetDirectorJobDefsResourceSpec("prod", "bareos-dir",
                                                           "ManagedJobDefs");
  ASSERT_TRUE(created_spec) << created_spec.error;
  EXPECT_EQ(created_spec.value->description, "Managed jobdefs");
  EXPECT_EQ(created_spec.value->type, "Backup");
  EXPECT_EQ(created_spec.value->protocol, "NDMP_BAREOS");
  ASSERT_TRUE(created_spec.value->run_before_job_entries);
  EXPECT_EQ(
      *created_spec.value->run_before_job_entries,
      (std::vector<std::string>{"/usr/lib/bareos/scripts/jobdefs-before"}));
  ASSERT_TRUE(created_spec.value->runscript_blocks);
  EXPECT_EQ(created_spec.value->runscript_blocks->size(), 1U);
  EXPECT_EQ(created_spec.value->replace, "IfOlder");
  EXPECT_EQ(created_spec.value->maximum_concurrent_jobs, 4);
  ASSERT_TRUE(created_spec.value->fd_plugin_options);
  EXPECT_EQ(*created_spec.value->fd_plugin_options,
            (std::vector<std::string>{"fd=defs"}));
  EXPECT_EQ(created_spec.value->always_incremental, true);

  const auto default_jobdefs_path
      = created.value->path / "bareos-dir.d/jobdefs/DefaultJob.conf";
  auto default_jobdefs_text = ReadTextFile(default_jobdefs_path);
  const std::string default_jobdefs_insertion
      = "  BackupFormat = \"Portable\"\n"
        "  Protocol = NDMP_NATIVE\n"
        "  Run = imported-jobdefs-run\n"
        "  RunBeforeJob = \"/tmp/scripts/prepare_jobdefs\"\n"
        "  RunAfterJob = \"/tmp/scripts/cleanup_jobdefs\"\n"
        "  RunAfterFailedJob = \"/tmp/scripts/report_jobdefs_failure\"\n"
        "  ClientRunBeforeJob = \"/tmp/scripts/jobdefs_client_prepare\"\n"
        "  ClientRunAfterJob = \"/tmp/scripts/jobdefs_client_cleanup\"\n"
        "  RunScript {\n"
        "    Command = \"/usr/lib/bareos/scripts/jobdefs-run\"\n"
        "    RunsWhen = After\n"
        "  }\n"
        "  Replace = Never\n"
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
        "  SelectionType = Volume\n"
        "  Accurate = yes\n"
        "  AllowDuplicateJobs = no\n"
        "  SaveFileHistory = yes\n"
        "  FileHistorySize = 76543\n"
        "  MaxConcurrentCopies = 9\n"
        "  AlwaysIncremental = yes\n"
        "  AlwaysIncrementalJobRetention = 21600\n"
        "  AlwaysIncrementalKeepNumber = 23\n"
        "  AlwaysIncrementalMaxFullAge = 180000\n"
        "  MaxFullConsolidations = 24\n"
        "  RunOnIncomingConnectInterval = 4800\n";
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
  EXPECT_NE(updated_text.find("Protocol = NDMP_NATIVE"), std::string::npos);
  EXPECT_NE(updated_text.find("Run = imported-jobdefs-run"), std::string::npos);
  EXPECT_NE(
      updated_text.find("RunBeforeJob = \"/tmp/scripts/prepare_jobdefs\""),
      std::string::npos);
  EXPECT_NE(updated_text.find("RunAfterJob = \"/tmp/scripts/cleanup_jobdefs\""),
            std::string::npos);
  EXPECT_NE(updated_text.find(
                "RunAfterFailedJob = \"/tmp/scripts/report_jobdefs_failure\""),
            std::string::npos);
  EXPECT_NE(updated_text.find(
                "ClientRunBeforeJob = \"/tmp/scripts/jobdefs_client_prepare\""),
            std::string::npos);
  EXPECT_NE(updated_text.find(
                "ClientRunAfterJob = \"/tmp/scripts/jobdefs_client_cleanup\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("RunScript {\n"
                              "    Command = "
                              "\"/usr/lib/bareos/scripts/jobdefs-run\"\n"
                              "    RunsWhen = After\n"
                              "  }\n"),
            std::string::npos);
  EXPECT_NE(updated_text.find("Replace = Never"), std::string::npos);
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
  EXPECT_NE(updated_text.find("SelectionType = Volume"), std::string::npos);
  EXPECT_NE(updated_text.find("Accurate = yes"), std::string::npos);
  EXPECT_NE(updated_text.find("AllowDuplicateJobs = no"), std::string::npos);
  EXPECT_NE(updated_text.find("SaveFileHistory = yes"), std::string::npos);
  EXPECT_NE(updated_text.find("FileHistorySize = 76543"), std::string::npos);
  EXPECT_NE(updated_text.find("MaxConcurrentCopies = 9"), std::string::npos);
  EXPECT_NE(updated_text.find("AlwaysIncremental = yes"), std::string::npos);
  EXPECT_NE(updated_text.find("AlwaysIncrementalJobRetention = 21600"),
            std::string::npos);
  EXPECT_NE(updated_text.find("AlwaysIncrementalKeepNumber = 23"),
            std::string::npos);
  EXPECT_NE(updated_text.find("AlwaysIncrementalMaxFullAge = 180000"),
            std::string::npos);
  EXPECT_NE(updated_text.find("MaxFullConsolidations = 24"), std::string::npos);
  EXPECT_NE(updated_text.find("RunOnIncomingConnectInterval = 4800"),
            std::string::npos);

  auto updated_spec = state.GetDirectorJobDefsResourceSpec("prod", "bareos-dir",
                                                           "DefaultJob");
  ASSERT_TRUE(updated_spec) << updated_spec.error;
  EXPECT_EQ(updated_spec.value->description, "Updated default jobdefs");
  EXPECT_EQ(updated_spec.value->full_backup_pool, "Full");
  EXPECT_EQ(updated_spec.value->differential_backup_pool, "Differential");
  ASSERT_TRUE(updated_spec.value->run_after_failed_job_entries);
  EXPECT_EQ(*updated_spec.value->run_after_failed_job_entries,
            (std::vector<std::string>{"/tmp/scripts/report_jobdefs_failure"}));
  ASSERT_TRUE(updated_spec.value->runscript_blocks);
  EXPECT_EQ(updated_spec.value->runscript_blocks->size(), 1U);
  EXPECT_EQ(updated_spec.value->replace, "Never");
  EXPECT_EQ(updated_spec.value->selection_type, "Volume");
  ASSERT_TRUE(updated_spec.value->dir_plugin_options);
  EXPECT_EQ(*updated_spec.value->dir_plugin_options,
            (std::vector<std::string>{"dir=imported-defs"}));
  EXPECT_EQ(updated_spec.value->max_concurrent_copies, 9);
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
  auto original_text = ReadTextFile(original_path);
  const auto original_brace = original_text.rfind("}\n");
  ASSERT_NE(original_brace, std::string::npos);
  original_text.insert(original_brace,
                       "  runafterfailedjob = "
                       "\"/tmp/scripts/shared-jobdefs-failed\"\n"
                       "  clientrunbeforejob = "
                       "\"/tmp/scripts/shared-jobdefs-client-before\"\n"
                       "  clientrunafterjob = "
                       "\"/tmp/scripts/shared-jobdefs-client-after\"\n"
                       "  runscript {\n"
                       "    command = \"/bin/true\"\n"
                       "    runswhen = before\n"
                       "  }\n");
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
  EXPECT_NE(shared_text.find(
                "RunAfterFailedJob = \"/tmp/scripts/shared-jobdefs-failed\""),
            std::string::npos);
  EXPECT_NE(shared_text.find("ClientRunBeforeJob = "
                             "\"/tmp/scripts/shared-jobdefs-client-before\""),
            std::string::npos);
  EXPECT_NE(shared_text.find("ClientRunAfterJob = "
                             "\"/tmp/scripts/shared-jobdefs-client-after\""),
            std::string::npos);
  EXPECT_NE(shared_text.find("runscript {\n"
                             "    command = \"/bin/true\"\n"
                             "    runswhen = before\n"
                             "  }\n"),
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
       .mail_command = std::string{"/usr/lib/bareos/mail-managed %r"},
       .operator_command = std::string{"/usr/lib/bareos/operator-managed %r"},
       .timestamp_format = std::string{"%Y-%m-%d %H:%M:%S"},
       .syslog_entries = std::vector<std::string>{"mail = all, !skipped"},
       .console_entries
       = std::vector<std::string>{"all, !skipped, !saved, !audit"}});
  ASSERT_TRUE(created) << created.error;
  EXPECT_EQ(created.value->name, "bareos-dir");

  const auto messages_path
      = created.value->path / "bareos-dir.d/messages/ManagedMessages.conf";
  const auto created_text = ReadTextFile(messages_path);
  EXPECT_NE(created_text.find("Messages {"), std::string::npos);
  EXPECT_NE(created_text.find("Name = \"ManagedMessages\""), std::string::npos);
  EXPECT_NE(created_text.find("Description = \"Managed messages\""),
            std::string::npos);
  EXPECT_NE(
      created_text.find("MailCommand = \"/usr/lib/bareos/mail-managed %r\""),
      std::string::npos);
  EXPECT_NE(created_text.find(
                "OperatorCommand = \"/usr/lib/bareos/operator-managed %r\""),
            std::string::npos);
  EXPECT_NE(created_text.find("TimestampFormat = \"%Y-%m-%d %H:%M:%S\""),
            std::string::npos);
  EXPECT_NE(created_text.find("Console = all, !skipped, !saved, !audit"),
            std::string::npos);
  EXPECT_NE(created_text.find("Syslog = mail = all, !skipped"),
            std::string::npos);

  const auto standard_path
      = created.value->path / "bareos-dir.d/messages/Standard.conf";
  auto standard_text = ReadTextFile(standard_path);
  auto standard_brace = standard_text.rfind("}\n");
  ASSERT_NE(standard_brace, std::string::npos);
  standard_text.insert(standard_brace,
                       "  timestampformat = \"%Y-%m-%d %H:%M:%S\"\n"
                       "  syslog = mail = all, !skipped\n");
  WriteTextFile(standard_path, standard_text);

  auto updated = state.UpsertDirectorMessagesResource(
      "prod", "bareos-dir", "Standard",
      {.description = std::string{"Updated standard messages"}});
  ASSERT_TRUE(updated) << updated.error;

  const auto updated_text = ReadTextFile(standard_path);
  EXPECT_NE(updated_text.find("Description = \"Updated standard messages\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("OperatorCommand = "), std::string::npos);
  EXPECT_NE(updated_text.find("MailCommand = "), std::string::npos);
  EXPECT_NE(updated_text.find("TimestampFormat = \"%Y-%m-%d %H:%M:%S\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("Syslog = mail = all, !skipped"),
            std::string::npos);
  EXPECT_NE(updated_text.find("Append = "), std::string::npos);

  auto current
      = state.GetDirectorMessagesResourceSpec("prod", "bareos-dir", "Standard");
  ASSERT_TRUE(current) << current.error;
  EXPECT_EQ(current.value->description, "Updated standard messages");
  EXPECT_EQ(current.value->timestamp_format, "%Y-%m-%d %H:%M:%S");
  ASSERT_TRUE(current.value->syslog_entries.has_value());
  EXPECT_EQ(*current.value->syslog_entries,
            (std::vector<std::string>{"mail = all, !skipped"}));
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
  EXPECT_NE(shared_text.find("console = all, !skipped, !saved, !audit"),
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

TEST(BconfigService, ServesDirectorMessagesPrefillRouteOverHttp)
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

    auto messages = state.UpsertDirectorMessagesResource(
        "prod", "bareos-dir", "ManagedMessages",
        {.description = std::string{"HTTP-managed director messages"},
         .mail_command = std::string{"/usr/lib/bareos/mail-http %r"},
         .timestamp_format = std::string{"%Y-%m-%d %H:%M:%S"},
         .syslog_entries = std::vector<std::string>{"mail = all, !skipped"},
         .console_entries
         = std::vector<std::string>{"all, !skipped, !saved, !audit"}});
    ASSERT_TRUE(messages) << messages.error;
  }

  ScopedBconfigServiceServer server{state_root.path()};
  ASSERT_TRUE(server.ready()) << server.startup_error();

  const auto messages_response = server.Get(
      "/v1/deployments/prod/directors/bareos-dir/messages/ManagedMessages/"
      "prefill");
  ASSERT_EQ(messages_response.status_code, 200u) << messages_response.body;
  auto messages_json = ParseJson(messages_response.body);
  ASSERT_NE(messages_json.get(), nullptr) << messages_response.body;
  auto* messages_deployment
      = json_object_get(messages_json.get(), "deployment");
  ASSERT_TRUE(json_is_object(messages_deployment));
  auto* messages_deployment_id = json_object_get(messages_deployment, "id");
  ASSERT_TRUE(json_is_string(messages_deployment_id));
  EXPECT_STREQ(json_string_value(messages_deployment_id), "prod");
  auto* messages_deployment_name = json_object_get(messages_deployment, "name");
  ASSERT_TRUE(json_is_string(messages_deployment_name));
  EXPECT_STREQ(json_string_value(messages_deployment_name), "Production");
  auto* messages_spec = json_object_get(messages_json.get(), "spec");
  ASSERT_TRUE(json_is_object(messages_spec));
  auto* messages_description = json_object_get(messages_spec, "description");
  ASSERT_TRUE(json_is_string(messages_description));
  EXPECT_STREQ(json_string_value(messages_description),
               "HTTP-managed director messages");
  auto* messages_mail_command = json_object_get(messages_spec, "mail_command");
  ASSERT_TRUE(json_is_string(messages_mail_command));
  EXPECT_STREQ(json_string_value(messages_mail_command),
               "/usr/lib/bareos/mail-http %r");
  auto* messages_timestamp = json_object_get(messages_spec, "timestamp_format");
  ASSERT_TRUE(json_is_string(messages_timestamp));
  EXPECT_STREQ(json_string_value(messages_timestamp), "%Y-%m-%d %H:%M:%S");
  auto* messages_syslog = json_object_get(messages_spec, "syslog_entries");
  ASSERT_TRUE(json_is_array(messages_syslog));
  ASSERT_EQ(json_array_size(messages_syslog), 1u);
  EXPECT_STREQ(json_string_value(json_array_get(messages_syslog, 0)),
               "mail = all, !skipped");
  auto* messages_console = json_object_get(messages_spec, "console_entries");
  ASSERT_TRUE(json_is_array(messages_console));
  ASSERT_EQ(json_array_size(messages_console), 1u);
  EXPECT_STREQ(json_string_value(json_array_get(messages_console, 0)),
               "all, !skipped, !saved, !audit");

  const auto missing_messages_response = server.Get(
      "/v1/deployments/prod/directors/bareos-dir/messages/MissingMessages/"
      "prefill");
  EXPECT_EQ(missing_messages_response.status_code, 400u);
  EXPECT_NE(missing_messages_response.body.find(
                "director messages 'MissingMessages' not found."),
            std::string::npos);

  const auto missing_deployment_response = server.Get(
      "/v1/deployments/missing/directors/bareos-dir/messages/ManagedMessages/"
      "prefill");
  EXPECT_EQ(missing_deployment_response.status_code, 404u);
  EXPECT_NE(missing_deployment_response.body.find("deployment not found."),
            std::string::npos);
#endif
}

TEST(BconfigService, ServesDirectorConsolePrefillRouteOverHttp)
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

    auto console = state.UpsertDirectorConsoleResource(
        "prod", "bareos-dir", "ManagedConsole",
        {.password = std::string{"[md5]abcdef0123456789abcdef0123456789"},
         .description = std::string{"HTTP-managed director console"},
         .job_acl = std::vector<std::string>{"ManagedJob"},
         .command_acl = std::vector<std::string>{"status", ".status", "show"},
         .profiles = std::vector<std::string>{"operator"},
         .use_pam_authentication = true,
         .tls_enable = true,
         .tls_require = true,
         .tls_cipher_list = std::string{"HIGH"},
         .tls_allowed_cn = std::vector<std::string>{"backup-admin"}});
    ASSERT_TRUE(console) << console.error;
  }

  ScopedBconfigServiceServer server{state_root.path()};
  ASSERT_TRUE(server.ready()) << server.startup_error();

  const auto console_response = server.Get(
      "/v1/deployments/prod/directors/bareos-dir/consoles/ManagedConsole/"
      "prefill");
  ASSERT_EQ(console_response.status_code, 200u) << console_response.body;
  auto console_json = ParseJson(console_response.body);
  ASSERT_NE(console_json.get(), nullptr) << console_response.body;
  auto* console_deployment = json_object_get(console_json.get(), "deployment");
  ASSERT_TRUE(json_is_object(console_deployment));
  auto* console_deployment_id = json_object_get(console_deployment, "id");
  ASSERT_TRUE(json_is_string(console_deployment_id));
  EXPECT_STREQ(json_string_value(console_deployment_id), "prod");
  auto* console_deployment_name = json_object_get(console_deployment, "name");
  ASSERT_TRUE(json_is_string(console_deployment_name));
  EXPECT_STREQ(json_string_value(console_deployment_name), "Production");
  auto* console_spec = json_object_get(console_json.get(), "spec");
  ASSERT_TRUE(json_is_object(console_spec));
  auto* console_description = json_object_get(console_spec, "description");
  ASSERT_TRUE(json_is_string(console_description));
  EXPECT_STREQ(json_string_value(console_description),
               "HTTP-managed director console");
  auto* console_job_acl = json_object_get(console_spec, "job_acl");
  ASSERT_TRUE(json_is_array(console_job_acl));
  ASSERT_EQ(json_array_size(console_job_acl), 1u);
  EXPECT_STREQ(json_string_value(json_array_get(console_job_acl, 0)),
               "ManagedJob");
  auto* console_command_acl = json_object_get(console_spec, "command_acl");
  ASSERT_TRUE(json_is_array(console_command_acl));
  ASSERT_EQ(json_array_size(console_command_acl), 3u);
  EXPECT_STREQ(json_string_value(json_array_get(console_command_acl, 1)),
               ".status");
  auto* console_profiles = json_object_get(console_spec, "profiles");
  ASSERT_TRUE(json_is_array(console_profiles));
  ASSERT_EQ(json_array_size(console_profiles), 1u);
  EXPECT_STREQ(json_string_value(json_array_get(console_profiles, 0)),
               "operator");
  auto* console_use_pam
      = json_object_get(console_spec, "use_pam_authentication");
  ASSERT_TRUE(json_is_boolean(console_use_pam));
  EXPECT_TRUE(json_is_true(console_use_pam));
  auto* console_tls_enable = json_object_get(console_spec, "tls_enable");
  ASSERT_TRUE(json_is_boolean(console_tls_enable));
  EXPECT_TRUE(json_is_true(console_tls_enable));
  auto* console_tls_require = json_object_get(console_spec, "tls_require");
  ASSERT_TRUE(json_is_boolean(console_tls_require));
  EXPECT_TRUE(json_is_true(console_tls_require));
  auto* console_tls_cipher = json_object_get(console_spec, "tls_cipher_list");
  ASSERT_TRUE(json_is_string(console_tls_cipher));
  EXPECT_STREQ(json_string_value(console_tls_cipher), "HIGH");
  auto* console_tls_allowed_cn
      = json_object_get(console_spec, "tls_allowed_cn");
  ASSERT_TRUE(json_is_array(console_tls_allowed_cn));
  ASSERT_EQ(json_array_size(console_tls_allowed_cn), 1u);
  EXPECT_STREQ(json_string_value(json_array_get(console_tls_allowed_cn, 0)),
               "backup-admin");

  const auto missing_console_response = server.Get(
      "/v1/deployments/prod/directors/bareos-dir/consoles/MissingConsole/"
      "prefill");
  EXPECT_EQ(missing_console_response.status_code, 400u);
  EXPECT_NE(missing_console_response.body.find(
                "director console 'MissingConsole' not found."),
            std::string::npos);

  const auto missing_deployment_response = server.Get(
      "/v1/deployments/missing/directors/bareos-dir/consoles/ManagedConsole/"
      "prefill");
  EXPECT_EQ(missing_deployment_response.status_code, 404u);
  EXPECT_NE(missing_deployment_response.body.find("deployment not found."),
            std::string::npos);
#endif
}

TEST(BconfigService, ServesDirectorUserPrefillRouteOverHttp)
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

    auto user = state.UpsertDirectorUserResource(
        "prod", "bareos-dir", "ManagedUser",
        {.description = std::string{"HTTP-managed director user"},
         .job_acl = std::vector<std::string>{"ManagedJob", "NightlyJob"},
         .command_acl = std::vector<std::string>{"status", ".status"},
         .profiles = std::vector<std::string>{"operator"}});
    ASSERT_TRUE(user) << user.error;
  }

  ScopedBconfigServiceServer server{state_root.path()};
  ASSERT_TRUE(server.ready()) << server.startup_error();

  const auto user_response = server.Get(
      "/v1/deployments/prod/directors/bareos-dir/users/ManagedUser/prefill");
  ASSERT_EQ(user_response.status_code, 200u) << user_response.body;
  auto user_json = ParseJson(user_response.body);
  ASSERT_NE(user_json.get(), nullptr) << user_response.body;
  auto* user_deployment = json_object_get(user_json.get(), "deployment");
  ASSERT_TRUE(json_is_object(user_deployment));
  auto* user_deployment_id = json_object_get(user_deployment, "id");
  ASSERT_TRUE(json_is_string(user_deployment_id));
  EXPECT_STREQ(json_string_value(user_deployment_id), "prod");
  auto* user_deployment_name = json_object_get(user_deployment, "name");
  ASSERT_TRUE(json_is_string(user_deployment_name));
  EXPECT_STREQ(json_string_value(user_deployment_name), "Production");
  auto* user_spec = json_object_get(user_json.get(), "spec");
  ASSERT_TRUE(json_is_object(user_spec));
  auto* user_description = json_object_get(user_spec, "description");
  ASSERT_TRUE(json_is_string(user_description));
  EXPECT_STREQ(json_string_value(user_description),
               "HTTP-managed director user");
  auto* user_job_acl = json_object_get(user_spec, "job_acl");
  ASSERT_TRUE(json_is_array(user_job_acl));
  ASSERT_EQ(json_array_size(user_job_acl), 2u);
  EXPECT_STREQ(json_string_value(json_array_get(user_job_acl, 0)),
               "ManagedJob");
  EXPECT_STREQ(json_string_value(json_array_get(user_job_acl, 1)),
               "NightlyJob");
  auto* user_command_acl = json_object_get(user_spec, "command_acl");
  ASSERT_TRUE(json_is_array(user_command_acl));
  ASSERT_EQ(json_array_size(user_command_acl), 2u);
  EXPECT_STREQ(json_string_value(json_array_get(user_command_acl, 1)),
               ".status");
  auto* user_profiles = json_object_get(user_spec, "profiles");
  ASSERT_TRUE(json_is_array(user_profiles));
  ASSERT_EQ(json_array_size(user_profiles), 1u);
  EXPECT_STREQ(json_string_value(json_array_get(user_profiles, 0)), "operator");

  const auto missing_user_response = server.Get(
      "/v1/deployments/prod/directors/bareos-dir/users/MissingUser/prefill");
  EXPECT_EQ(missing_user_response.status_code, 400u);
  EXPECT_NE(
      missing_user_response.body.find("director user 'MissingUser' not found."),
      std::string::npos);

  const auto missing_deployment_response = server.Get(
      "/v1/deployments/missing/directors/bareos-dir/users/ManagedUser/prefill");
  EXPECT_EQ(missing_deployment_response.status_code, 404u);
  EXPECT_NE(missing_deployment_response.body.find("deployment not found."),
            std::string::npos);
#endif
}

TEST(BconfigService, ServesDirectorProfilePrefillRouteOverHttp)
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

    auto profile = state.UpsertDirectorProfileResource(
        "prod", "bareos-dir", "ManagedProfile",
        {.description = std::string{"HTTP-managed director profile"},
         .command_acl = std::vector<std::string>{"status", ".status"},
         .catalog_acl = std::vector<std::string>{"managed-catalog"}});
    ASSERT_TRUE(profile) << profile.error;
  }

  ScopedBconfigServiceServer server{state_root.path()};
  ASSERT_TRUE(server.ready()) << server.startup_error();

  const auto profile_response = server.Get(
      "/v1/deployments/prod/directors/bareos-dir/profiles/ManagedProfile/"
      "prefill");
  ASSERT_EQ(profile_response.status_code, 200u) << profile_response.body;
  auto profile_json = ParseJson(profile_response.body);
  ASSERT_NE(profile_json.get(), nullptr) << profile_response.body;
  auto* profile_deployment = json_object_get(profile_json.get(), "deployment");
  ASSERT_TRUE(json_is_object(profile_deployment));
  auto* profile_deployment_id = json_object_get(profile_deployment, "id");
  ASSERT_TRUE(json_is_string(profile_deployment_id));
  EXPECT_STREQ(json_string_value(profile_deployment_id), "prod");
  auto* profile_deployment_name = json_object_get(profile_deployment, "name");
  ASSERT_TRUE(json_is_string(profile_deployment_name));
  EXPECT_STREQ(json_string_value(profile_deployment_name), "Production");
  auto* profile_spec = json_object_get(profile_json.get(), "spec");
  ASSERT_TRUE(json_is_object(profile_spec));
  auto* profile_description = json_object_get(profile_spec, "description");
  ASSERT_TRUE(json_is_string(profile_description));
  EXPECT_STREQ(json_string_value(profile_description),
               "HTTP-managed director profile");
  auto* profile_command_acl = json_object_get(profile_spec, "command_acl");
  ASSERT_TRUE(json_is_array(profile_command_acl));
  ASSERT_EQ(json_array_size(profile_command_acl), 2u);
  EXPECT_STREQ(json_string_value(json_array_get(profile_command_acl, 0)),
               "status");
  EXPECT_STREQ(json_string_value(json_array_get(profile_command_acl, 1)),
               ".status");
  auto* profile_catalog_acl = json_object_get(profile_spec, "catalog_acl");
  ASSERT_TRUE(json_is_array(profile_catalog_acl));
  ASSERT_EQ(json_array_size(profile_catalog_acl), 1u);
  EXPECT_STREQ(json_string_value(json_array_get(profile_catalog_acl, 0)),
               "managed-catalog");

  const auto missing_profile_response = server.Get(
      "/v1/deployments/prod/directors/bareos-dir/profiles/MissingProfile/"
      "prefill");
  EXPECT_EQ(missing_profile_response.status_code, 400u);
  EXPECT_NE(missing_profile_response.body.find(
                "director profile 'MissingProfile' not found."),
            std::string::npos);

  const auto missing_deployment_response = server.Get(
      "/v1/deployments/missing/directors/bareos-dir/profiles/ManagedProfile/"
      "prefill");
  EXPECT_EQ(missing_deployment_response.status_code, 404u);
  EXPECT_NE(missing_deployment_response.body.find("deployment not found."),
            std::string::npos);
#endif
}

TEST(BconfigService, ServesDirectorPoolPrefillRouteOverHttp)
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

    auto pool = state.UpsertDirectorPoolResource(
        "prod", "bareos-dir", "ManagedPool",
        {.pool_type = std::string{"Scratch"},
         .label_format = std::string{"Managed-"},
         .maximum_volumes = 7,
         .maximum_volume_bytes = 123456,
         .storages = std::vector<std::string>{"File"},
         .use_catalog = true,
         .recycle = false,
         .file_retention = 172800,
         .description = std::string{"HTTP-managed director pool"}});
    ASSERT_TRUE(pool) << pool.error;
  }

  ScopedBconfigServiceServer server{state_root.path()};
  ASSERT_TRUE(server.ready()) << server.startup_error();

  const auto pool_response = server.Get(
      "/v1/deployments/prod/directors/bareos-dir/pools/ManagedPool/prefill");
  ASSERT_EQ(pool_response.status_code, 200u) << pool_response.body;
  auto pool_json = ParseJson(pool_response.body);
  ASSERT_NE(pool_json.get(), nullptr) << pool_response.body;
  auto* pool_deployment = json_object_get(pool_json.get(), "deployment");
  ASSERT_TRUE(json_is_object(pool_deployment));
  auto* pool_deployment_id = json_object_get(pool_deployment, "id");
  ASSERT_TRUE(json_is_string(pool_deployment_id));
  EXPECT_STREQ(json_string_value(pool_deployment_id), "prod");
  auto* pool_deployment_name = json_object_get(pool_deployment, "name");
  ASSERT_TRUE(json_is_string(pool_deployment_name));
  EXPECT_STREQ(json_string_value(pool_deployment_name), "Production");
  auto* pool_spec = json_object_get(pool_json.get(), "spec");
  ASSERT_TRUE(json_is_object(pool_spec));
  auto* pool_description = json_object_get(pool_spec, "description");
  ASSERT_TRUE(json_is_string(pool_description));
  EXPECT_STREQ(json_string_value(pool_description),
               "HTTP-managed director pool");
  auto* pool_type = json_object_get(pool_spec, "pool_type");
  ASSERT_TRUE(json_is_string(pool_type));
  EXPECT_STREQ(json_string_value(pool_type), "Scratch");
  auto* pool_label_format = json_object_get(pool_spec, "label_format");
  ASSERT_TRUE(json_is_string(pool_label_format));
  EXPECT_STREQ(json_string_value(pool_label_format), "Managed-");
  auto* pool_maximum_volumes = json_object_get(pool_spec, "maximum_volumes");
  ASSERT_TRUE(json_is_integer(pool_maximum_volumes));
  EXPECT_EQ(json_integer_value(pool_maximum_volumes), 7);
  auto* pool_maximum_volume_bytes
      = json_object_get(pool_spec, "maximum_volume_bytes");
  ASSERT_TRUE(json_is_integer(pool_maximum_volume_bytes));
  EXPECT_EQ(json_integer_value(pool_maximum_volume_bytes), 123456);
  auto* pool_storages = json_object_get(pool_spec, "storages");
  ASSERT_TRUE(json_is_array(pool_storages));
  ASSERT_EQ(json_array_size(pool_storages), 1u);
  EXPECT_STREQ(json_string_value(json_array_get(pool_storages, 0)), "File");
  auto* pool_use_catalog = json_object_get(pool_spec, "use_catalog");
  ASSERT_TRUE(json_is_boolean(pool_use_catalog));
  EXPECT_TRUE(json_is_true(pool_use_catalog));
  auto* pool_recycle = json_object_get(pool_spec, "recycle");
  ASSERT_TRUE(json_is_boolean(pool_recycle));
  EXPECT_TRUE(json_is_false(pool_recycle));
  auto* pool_file_retention = json_object_get(pool_spec, "file_retention");
  ASSERT_TRUE(json_is_integer(pool_file_retention));
  EXPECT_EQ(json_integer_value(pool_file_retention), 172800);

  const auto missing_pool_response = server.Get(
      "/v1/deployments/prod/directors/bareos-dir/pools/MissingPool/prefill");
  EXPECT_EQ(missing_pool_response.status_code, 400u);
  EXPECT_NE(
      missing_pool_response.body.find("director pool 'MissingPool' not found."),
      std::string::npos);

  const auto missing_deployment_response = server.Get(
      "/v1/deployments/missing/directors/bareos-dir/pools/ManagedPool/prefill");
  EXPECT_EQ(missing_deployment_response.status_code, 404u);
  EXPECT_NE(missing_deployment_response.body.find("deployment not found."),
            std::string::npos);
#endif
}

TEST(BconfigService, ServesDirectorCatalogPrefillRouteOverHttp)
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

    auto catalog = state.UpsertDirectorCatalogResource(
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
         .description = std::string{"HTTP-managed director catalog"}});
    ASSERT_TRUE(catalog) << catalog.error;
  }

  ScopedBconfigServiceServer server{state_root.path()};
  ASSERT_TRUE(server.ready()) << server.startup_error();

  const auto catalog_response = server.Get(
      "/v1/deployments/prod/directors/bareos-dir/catalogs/ManagedCatalog/"
      "prefill");
  ASSERT_EQ(catalog_response.status_code, 200u) << catalog_response.body;
  auto catalog_json = ParseJson(catalog_response.body);
  ASSERT_NE(catalog_json.get(), nullptr) << catalog_response.body;
  auto* catalog_deployment = json_object_get(catalog_json.get(), "deployment");
  ASSERT_TRUE(json_is_object(catalog_deployment));
  auto* catalog_deployment_id = json_object_get(catalog_deployment, "id");
  ASSERT_TRUE(json_is_string(catalog_deployment_id));
  EXPECT_STREQ(json_string_value(catalog_deployment_id), "prod");
  auto* catalog_deployment_name = json_object_get(catalog_deployment, "name");
  ASSERT_TRUE(json_is_string(catalog_deployment_name));
  EXPECT_STREQ(json_string_value(catalog_deployment_name), "Production");
  auto* catalog_spec = json_object_get(catalog_json.get(), "spec");
  ASSERT_TRUE(json_is_object(catalog_spec));
  auto* catalog_description = json_object_get(catalog_spec, "description");
  ASSERT_TRUE(json_is_string(catalog_description));
  EXPECT_STREQ(json_string_value(catalog_description),
               "HTTP-managed director catalog");
  auto* catalog_db_address = json_object_get(catalog_spec, "db_address");
  ASSERT_TRUE(json_is_string(catalog_db_address));
  EXPECT_STREQ(json_string_value(catalog_db_address), "127.0.0.1");
  auto* catalog_db_port = json_object_get(catalog_spec, "db_port");
  ASSERT_TRUE(json_is_integer(catalog_db_port));
  EXPECT_EQ(json_integer_value(catalog_db_port), 5432);
  auto* catalog_db_password = json_object_get(catalog_spec, "db_password");
  ASSERT_TRUE(json_is_string(catalog_db_password));
  EXPECT_STREQ(json_string_value(catalog_db_password), "secret");
  auto* catalog_db_user = json_object_get(catalog_spec, "db_user");
  ASSERT_TRUE(json_is_string(catalog_db_user));
  EXPECT_STREQ(json_string_value(catalog_db_user), "bareos");
  auto* catalog_db_name = json_object_get(catalog_spec, "db_name");
  ASSERT_TRUE(json_is_string(catalog_db_name));
  EXPECT_STREQ(json_string_value(catalog_db_name), "bareos_catalog");
  auto* catalog_multiple_connections
      = json_object_get(catalog_spec, "multiple_connections");
  ASSERT_TRUE(json_is_boolean(catalog_multiple_connections));
  EXPECT_TRUE(json_is_true(catalog_multiple_connections));
  auto* catalog_disable_batch_insert
      = json_object_get(catalog_spec, "disable_batch_insert");
  ASSERT_TRUE(json_is_boolean(catalog_disable_batch_insert));
  EXPECT_TRUE(json_is_true(catalog_disable_batch_insert));
  auto* catalog_reconnect = json_object_get(catalog_spec, "reconnect");
  ASSERT_TRUE(json_is_boolean(catalog_reconnect));
  EXPECT_TRUE(json_is_true(catalog_reconnect));
  auto* catalog_exit_on_fatal = json_object_get(catalog_spec, "exit_on_fatal");
  ASSERT_TRUE(json_is_boolean(catalog_exit_on_fatal));
  EXPECT_TRUE(json_is_false(catalog_exit_on_fatal));

  const auto missing_catalog_response = server.Get(
      "/v1/deployments/prod/directors/bareos-dir/catalogs/MissingCatalog/"
      "prefill");
  EXPECT_EQ(missing_catalog_response.status_code, 400u);
  EXPECT_NE(missing_catalog_response.body.find(
                "director catalog 'MissingCatalog' not found."),
            std::string::npos);

  const auto missing_deployment_response = server.Get(
      "/v1/deployments/missing/directors/bareos-dir/catalogs/ManagedCatalog/"
      "prefill");
  EXPECT_EQ(missing_deployment_response.status_code, 404u);
  EXPECT_NE(missing_deployment_response.body.find("deployment not found."),
            std::string::npos);
#endif
}

TEST(BconfigService, ServesDirectorSchedulePrefillRouteOverHttp)
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

    auto schedule = state.UpsertDirectorScheduleResource(
        "prod", "bareos-dir", "ManagedSchedule",
        {.description = std::string{"HTTP-managed director schedule"},
         .enabled = false,
         .run_entries
         = std::vector<std::string>{"Level=Full monthly 1st sat at 21:00",
                                    "Level=Incremental mon-fri at 21:00"}});
    ASSERT_TRUE(schedule) << schedule.error;
  }

  ScopedBconfigServiceServer server{state_root.path()};
  ASSERT_TRUE(server.ready()) << server.startup_error();

  const auto schedule_response = server.Get(
      "/v1/deployments/prod/directors/bareos-dir/schedules/ManagedSchedule/"
      "prefill");
  ASSERT_EQ(schedule_response.status_code, 200u) << schedule_response.body;
  auto schedule_json = ParseJson(schedule_response.body);
  ASSERT_NE(schedule_json.get(), nullptr) << schedule_response.body;
  auto* schedule_deployment
      = json_object_get(schedule_json.get(), "deployment");
  ASSERT_TRUE(json_is_object(schedule_deployment));
  auto* schedule_deployment_id = json_object_get(schedule_deployment, "id");
  ASSERT_TRUE(json_is_string(schedule_deployment_id));
  EXPECT_STREQ(json_string_value(schedule_deployment_id), "prod");
  auto* schedule_deployment_name = json_object_get(schedule_deployment, "name");
  ASSERT_TRUE(json_is_string(schedule_deployment_name));
  EXPECT_STREQ(json_string_value(schedule_deployment_name), "Production");
  auto* schedule_spec = json_object_get(schedule_json.get(), "spec");
  ASSERT_TRUE(json_is_object(schedule_spec));
  auto* schedule_description = json_object_get(schedule_spec, "description");
  ASSERT_TRUE(json_is_string(schedule_description));
  EXPECT_STREQ(json_string_value(schedule_description),
               "HTTP-managed director schedule");
  auto* schedule_enabled = json_object_get(schedule_spec, "enabled");
  ASSERT_TRUE(json_is_boolean(schedule_enabled));
  EXPECT_TRUE(json_is_false(schedule_enabled));
  auto* schedule_run_entries = json_object_get(schedule_spec, "run_entries");
  ASSERT_TRUE(json_is_array(schedule_run_entries));
  ASSERT_EQ(json_array_size(schedule_run_entries), 2u);
  EXPECT_STREQ(json_string_value(json_array_get(schedule_run_entries, 0)),
               "Level=Full monthly 1st sat at 21:00");
  EXPECT_STREQ(json_string_value(json_array_get(schedule_run_entries, 1)),
               "Level=Incremental mon-fri at 21:00");

  const auto missing_schedule_response = server.Get(
      "/v1/deployments/prod/directors/bareos-dir/schedules/MissingSchedule/"
      "prefill");
  EXPECT_EQ(missing_schedule_response.status_code, 400u);
  EXPECT_NE(missing_schedule_response.body.find(
                "director schedule 'MissingSchedule' not found."),
            std::string::npos);

  const auto missing_deployment_response = server.Get(
      "/v1/deployments/missing/directors/bareos-dir/schedules/ManagedSchedule/"
      "prefill");
  EXPECT_EQ(missing_deployment_response.status_code, 404u);
  EXPECT_NE(missing_deployment_response.body.find("deployment not found."),
            std::string::npos);
#endif
}

TEST(BconfigService, ServesDirectorCounterPrefillRouteOverHttp)
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
    AddCounterFixtures(source_root.path());

    auto import_job
        = state.CreateJob({.type = "import_configuration",
                           .deployment_id = std::string{"prod"},
                           .source_path = source_root.path().string()});
    ASSERT_TRUE(import_job);
    auto imported = WaitForJobTerminal(state, import_job.value->id);
    ASSERT_TRUE(imported.has_value());
    ASSERT_EQ(imported->status, JobStatus::kSucceeded);

    auto counter = state.UpsertDirectorCounterResource(
        "prod", "bareos-dir", "ManagedCounter",
        {.minimum = 10,
         .maximum = 50,
         .wrap_counter = std::string{"WrapSeed"},
         .catalog = std::string{"MyCatalog"},
         .description = std::string{"HTTP-managed director counter"}});
    ASSERT_TRUE(counter) << counter.error;
  }

  ScopedBconfigServiceServer server{state_root.path()};
  ASSERT_TRUE(server.ready()) << server.startup_error();

  const auto counter_response = server.Get(
      "/v1/deployments/prod/directors/bareos-dir/counters/ManagedCounter/"
      "prefill");
  ASSERT_EQ(counter_response.status_code, 200u) << counter_response.body;
  auto counter_json = ParseJson(counter_response.body);
  ASSERT_NE(counter_json.get(), nullptr) << counter_response.body;
  auto* counter_deployment = json_object_get(counter_json.get(), "deployment");
  ASSERT_TRUE(json_is_object(counter_deployment));
  auto* counter_deployment_id = json_object_get(counter_deployment, "id");
  ASSERT_TRUE(json_is_string(counter_deployment_id));
  EXPECT_STREQ(json_string_value(counter_deployment_id), "prod");
  auto* counter_deployment_name = json_object_get(counter_deployment, "name");
  ASSERT_TRUE(json_is_string(counter_deployment_name));
  EXPECT_STREQ(json_string_value(counter_deployment_name), "Production");
  auto* counter_spec = json_object_get(counter_json.get(), "spec");
  ASSERT_TRUE(json_is_object(counter_spec));
  auto* counter_minimum = json_object_get(counter_spec, "minimum");
  ASSERT_TRUE(json_is_integer(counter_minimum));
  EXPECT_EQ(json_integer_value(counter_minimum), 10);
  auto* counter_maximum = json_object_get(counter_spec, "maximum");
  ASSERT_TRUE(json_is_integer(counter_maximum));
  EXPECT_EQ(json_integer_value(counter_maximum), 50);
  auto* counter_wrap = json_object_get(counter_spec, "wrap_counter");
  ASSERT_TRUE(json_is_string(counter_wrap));
  EXPECT_STREQ(json_string_value(counter_wrap), "WrapSeed");
  auto* counter_catalog = json_object_get(counter_spec, "catalog");
  ASSERT_TRUE(json_is_string(counter_catalog));
  EXPECT_STREQ(json_string_value(counter_catalog), "MyCatalog");
  auto* counter_description = json_object_get(counter_spec, "description");
  ASSERT_TRUE(json_is_string(counter_description));
  EXPECT_STREQ(json_string_value(counter_description),
               "HTTP-managed director counter");

  const auto missing_counter_response = server.Get(
      "/v1/deployments/prod/directors/bareos-dir/counters/MissingCounter/"
      "prefill");
  EXPECT_EQ(missing_counter_response.status_code, 400u);
  EXPECT_NE(missing_counter_response.body.find(
                "director counter 'MissingCounter' not found."),
            std::string::npos);

  const auto missing_deployment_response = server.Get(
      "/v1/deployments/missing/directors/bareos-dir/counters/ManagedCounter/"
      "prefill");
  EXPECT_EQ(missing_deployment_response.status_code, 404u);
  EXPECT_NE(missing_deployment_response.body.find("deployment not found."),
            std::string::npos);
#endif
}

TEST(BconfigService, ServesDirectorFilesetPrefillRouteOverHttp)
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

    auto fileset = state.UpsertDirectorFilesetResource(
        "prod", "bareos-dir", "ManagedFileSet",
        {.description = std::string{"HTTP-managed director fileset"},
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
    ASSERT_TRUE(fileset) << fileset.error;
  }

  ScopedBconfigServiceServer server{state_root.path()};
  ASSERT_TRUE(server.ready()) << server.startup_error();

  const auto fileset_response = server.Get(
      "/v1/deployments/prod/directors/bareos-dir/filesets/ManagedFileSet/"
      "prefill");
  ASSERT_EQ(fileset_response.status_code, 200u) << fileset_response.body;
  auto fileset_json = ParseJson(fileset_response.body);
  ASSERT_NE(fileset_json.get(), nullptr) << fileset_response.body;
  auto* fileset_deployment = json_object_get(fileset_json.get(), "deployment");
  ASSERT_TRUE(json_is_object(fileset_deployment));
  auto* fileset_deployment_id = json_object_get(fileset_deployment, "id");
  ASSERT_TRUE(json_is_string(fileset_deployment_id));
  EXPECT_STREQ(json_string_value(fileset_deployment_id), "prod");
  auto* fileset_deployment_name = json_object_get(fileset_deployment, "name");
  ASSERT_TRUE(json_is_string(fileset_deployment_name));
  EXPECT_STREQ(json_string_value(fileset_deployment_name), "Production");
  auto* fileset_spec = json_object_get(fileset_json.get(), "spec");
  ASSERT_TRUE(json_is_object(fileset_spec));
  auto* fileset_description = json_object_get(fileset_spec, "description");
  ASSERT_TRUE(json_is_string(fileset_description));
  EXPECT_STREQ(json_string_value(fileset_description),
               "HTTP-managed director fileset");
  auto* fileset_ignore
      = json_object_get(fileset_spec, "ignore_fileset_changes");
  ASSERT_TRUE(json_is_boolean(fileset_ignore));
  EXPECT_TRUE(json_is_true(fileset_ignore));
  auto* fileset_enable_vss = json_object_get(fileset_spec, "enable_vss");
  ASSERT_TRUE(json_is_boolean(fileset_enable_vss));
  EXPECT_TRUE(json_is_false(fileset_enable_vss));
  auto* fileset_includes = json_object_get(fileset_spec, "include_blocks");
  ASSERT_TRUE(json_is_array(fileset_includes));
  ASSERT_EQ(json_array_size(fileset_includes), 1u);
  EXPECT_NE(
      std::string{json_string_value(json_array_get(fileset_includes, 0))}.find(
          "Signature = XXH128"),
      std::string::npos);
  auto* fileset_excludes = json_object_get(fileset_spec, "exclude_blocks");
  ASSERT_TRUE(json_is_array(fileset_excludes));
  ASSERT_EQ(json_array_size(fileset_excludes), 1u);
  EXPECT_NE(
      std::string{json_string_value(json_array_get(fileset_excludes, 0))}.find(
          "/tmp/tests/backup-bareos-test/tmp/cache"),
      std::string::npos);

  const auto missing_fileset_response = server.Get(
      "/v1/deployments/prod/directors/bareos-dir/filesets/MissingFileSet/"
      "prefill");
  EXPECT_EQ(missing_fileset_response.status_code, 400u);
  EXPECT_NE(missing_fileset_response.body.find(
                "director fileset 'MissingFileSet' not found."),
            std::string::npos);

  const auto missing_deployment_response = server.Get(
      "/v1/deployments/missing/directors/bareos-dir/filesets/ManagedFileSet/"
      "prefill");
  EXPECT_EQ(missing_deployment_response.status_code, 404u);
  EXPECT_NE(missing_deployment_response.body.find("deployment not found."),
            std::string::npos);
#endif
}

TEST(BconfigService, ServesDirectorJobDefsPrefillRouteOverHttp)
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

    auto jobdefs = state.UpsertDirectorJobDefsResource(
        "prod", "bareos-dir", "ManagedJobDefs",
        {.description = std::string{"HTTP-managed director jobdefs"},
         .type = std::string{"Backup"},
         .backup_format = std::string{"Portable"},
         .protocol = std::string{"ndmp"},
         .client = std::string{"bareos-fd"},
         .fileset = std::string{"SelfTest"},
         .run_entries = std::vector<std::string>{"managed-jobdefs-run"},
         .run_before_job_entries
         = std::vector<std::string>{"/usr/lib/bareos/scripts/jobdefs-before"},
         .write_bootstrap = std::string{"/tmp/http-managed-jobdefs.bsr"},
         .maximum_concurrent_jobs = 4,
         .enabled = false});
    ASSERT_TRUE(jobdefs) << jobdefs.error;
  }

  ScopedBconfigServiceServer server{state_root.path()};
  ASSERT_TRUE(server.ready()) << server.startup_error();

  const auto jobdefs_response = server.Get(
      "/v1/deployments/prod/directors/bareos-dir/jobdefs/ManagedJobDefs/"
      "prefill");
  ASSERT_EQ(jobdefs_response.status_code, 200u) << jobdefs_response.body;
  auto jobdefs_json = ParseJson(jobdefs_response.body);
  ASSERT_NE(jobdefs_json.get(), nullptr) << jobdefs_response.body;
  auto* jobdefs_deployment = json_object_get(jobdefs_json.get(), "deployment");
  ASSERT_TRUE(json_is_object(jobdefs_deployment));
  auto* jobdefs_deployment_id = json_object_get(jobdefs_deployment, "id");
  ASSERT_TRUE(json_is_string(jobdefs_deployment_id));
  EXPECT_STREQ(json_string_value(jobdefs_deployment_id), "prod");
  auto* jobdefs_deployment_name = json_object_get(jobdefs_deployment, "name");
  ASSERT_TRUE(json_is_string(jobdefs_deployment_name));
  EXPECT_STREQ(json_string_value(jobdefs_deployment_name), "Production");
  auto* jobdefs_spec = json_object_get(jobdefs_json.get(), "spec");
  ASSERT_TRUE(json_is_object(jobdefs_spec));
  auto* jobdefs_description = json_object_get(jobdefs_spec, "description");
  ASSERT_TRUE(json_is_string(jobdefs_description));
  EXPECT_STREQ(json_string_value(jobdefs_description),
               "HTTP-managed director jobdefs");
  auto* jobdefs_type = json_object_get(jobdefs_spec, "type");
  ASSERT_TRUE(json_is_string(jobdefs_type));
  EXPECT_STREQ(json_string_value(jobdefs_type), "Backup");
  auto* jobdefs_backup_format = json_object_get(jobdefs_spec, "backup_format");
  ASSERT_TRUE(json_is_string(jobdefs_backup_format));
  EXPECT_STREQ(json_string_value(jobdefs_backup_format), "Portable");
  auto* jobdefs_protocol = json_object_get(jobdefs_spec, "protocol");
  ASSERT_TRUE(json_is_string(jobdefs_protocol));
  EXPECT_STREQ(json_string_value(jobdefs_protocol), "NDMP_BAREOS");
  auto* jobdefs_client = json_object_get(jobdefs_spec, "client");
  ASSERT_TRUE(json_is_string(jobdefs_client));
  EXPECT_STREQ(json_string_value(jobdefs_client), "bareos-fd");
  auto* jobdefs_fileset = json_object_get(jobdefs_spec, "fileset");
  ASSERT_TRUE(json_is_string(jobdefs_fileset));
  EXPECT_STREQ(json_string_value(jobdefs_fileset), "SelfTest");
  auto* jobdefs_runs = json_object_get(jobdefs_spec, "run_entries");
  ASSERT_TRUE(json_is_array(jobdefs_runs));
  ASSERT_EQ(json_array_size(jobdefs_runs), 1u);
  EXPECT_STREQ(json_string_value(json_array_get(jobdefs_runs, 0)),
               "managed-jobdefs-run");
  auto* jobdefs_run_before
      = json_object_get(jobdefs_spec, "run_before_job_entries");
  ASSERT_TRUE(json_is_array(jobdefs_run_before));
  ASSERT_EQ(json_array_size(jobdefs_run_before), 1u);
  EXPECT_STREQ(json_string_value(json_array_get(jobdefs_run_before, 0)),
               "/usr/lib/bareos/scripts/jobdefs-before");
  auto* jobdefs_write_bootstrap
      = json_object_get(jobdefs_spec, "write_bootstrap");
  ASSERT_TRUE(json_is_string(jobdefs_write_bootstrap));
  EXPECT_STREQ(json_string_value(jobdefs_write_bootstrap),
               "/tmp/http-managed-jobdefs.bsr");
  auto* jobdefs_maximum_concurrent_jobs
      = json_object_get(jobdefs_spec, "maximum_concurrent_jobs");
  ASSERT_TRUE(json_is_integer(jobdefs_maximum_concurrent_jobs));
  EXPECT_EQ(json_integer_value(jobdefs_maximum_concurrent_jobs), 4);
  auto* jobdefs_enabled = json_object_get(jobdefs_spec, "enabled");
  ASSERT_TRUE(json_is_boolean(jobdefs_enabled));
  EXPECT_TRUE(json_is_false(jobdefs_enabled));

  const auto missing_jobdefs_response = server.Get(
      "/v1/deployments/prod/directors/bareos-dir/jobdefs/MissingJobDefs/"
      "prefill");
  EXPECT_EQ(missing_jobdefs_response.status_code, 400u);
  EXPECT_NE(missing_jobdefs_response.body.find(
                "director jobdefs 'MissingJobDefs' not found."),
            std::string::npos);

  const auto missing_deployment_response = server.Get(
      "/v1/deployments/missing/directors/bareos-dir/jobdefs/ManagedJobDefs/"
      "prefill");
  EXPECT_EQ(missing_deployment_response.status_code, 404u);
  EXPECT_NE(missing_deployment_response.body.find("deployment not found."),
            std::string::npos);
#endif
}

TEST(BconfigService, ServesDirectorJobPrefillRouteOverHttp)
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

    auto job = state.UpsertDirectorJobResource(
        "prod", "bareos-dir", "ManagedJob",
        {.description = std::string{"HTTP-managed director job"},
         .type = std::string{"Backup"},
         .protocol = std::string{"ndmp"},
         .client = std::string{"bareos-fd"},
         .jobdefs = std::string{"DefaultJob"},
         .run_entries = std::vector<std::string>{"managed-job-run"},
         .run_before_job_entries
         = std::vector<std::string>{"/usr/lib/bareos/scripts/job-before"},
         .write_bootstrap = std::string{"/tmp/http-managed-job.bsr"},
         .maximum_concurrent_jobs = 2,
         .enabled = false});
    ASSERT_TRUE(job) << job.error;
  }

  ScopedBconfigServiceServer server{state_root.path()};
  ASSERT_TRUE(server.ready()) << server.startup_error();

  const auto job_response = server.Get(
      "/v1/deployments/prod/directors/bareos-dir/jobs/ManagedJob/prefill");
  ASSERT_EQ(job_response.status_code, 200u) << job_response.body;
  auto job_json = ParseJson(job_response.body);
  ASSERT_NE(job_json.get(), nullptr) << job_response.body;
  auto* job_deployment = json_object_get(job_json.get(), "deployment");
  ASSERT_TRUE(json_is_object(job_deployment));
  auto* job_deployment_id = json_object_get(job_deployment, "id");
  ASSERT_TRUE(json_is_string(job_deployment_id));
  EXPECT_STREQ(json_string_value(job_deployment_id), "prod");
  auto* job_deployment_name = json_object_get(job_deployment, "name");
  ASSERT_TRUE(json_is_string(job_deployment_name));
  EXPECT_STREQ(json_string_value(job_deployment_name), "Production");
  auto* job_spec = json_object_get(job_json.get(), "spec");
  ASSERT_TRUE(json_is_object(job_spec));
  auto* job_description = json_object_get(job_spec, "description");
  ASSERT_TRUE(json_is_string(job_description));
  EXPECT_STREQ(json_string_value(job_description), "HTTP-managed director job");
  auto* job_type = json_object_get(job_spec, "type");
  ASSERT_TRUE(json_is_string(job_type));
  EXPECT_STREQ(json_string_value(job_type), "Backup");
  auto* job_protocol = json_object_get(job_spec, "protocol");
  ASSERT_TRUE(json_is_string(job_protocol));
  EXPECT_STREQ(json_string_value(job_protocol), "NDMP_BAREOS");
  auto* job_client = json_object_get(job_spec, "client");
  ASSERT_TRUE(json_is_string(job_client));
  EXPECT_STREQ(json_string_value(job_client), "bareos-fd");
  auto* job_jobdefs = json_object_get(job_spec, "jobdefs");
  ASSERT_TRUE(json_is_string(job_jobdefs));
  EXPECT_STREQ(json_string_value(job_jobdefs), "DefaultJob");
  auto* job_runs = json_object_get(job_spec, "run_entries");
  ASSERT_TRUE(json_is_array(job_runs));
  ASSERT_EQ(json_array_size(job_runs), 1u);
  EXPECT_STREQ(json_string_value(json_array_get(job_runs, 0)),
               "managed-job-run");
  auto* job_run_before = json_object_get(job_spec, "run_before_job_entries");
  ASSERT_TRUE(json_is_array(job_run_before));
  ASSERT_EQ(json_array_size(job_run_before), 1u);
  EXPECT_STREQ(json_string_value(json_array_get(job_run_before, 0)),
               "/usr/lib/bareos/scripts/job-before");
  auto* job_write_bootstrap = json_object_get(job_spec, "write_bootstrap");
  ASSERT_TRUE(json_is_string(job_write_bootstrap));
  EXPECT_STREQ(json_string_value(job_write_bootstrap),
               "/tmp/http-managed-job.bsr");
  auto* job_maximum_concurrent_jobs
      = json_object_get(job_spec, "maximum_concurrent_jobs");
  ASSERT_TRUE(json_is_integer(job_maximum_concurrent_jobs));
  EXPECT_EQ(json_integer_value(job_maximum_concurrent_jobs), 2);
  auto* job_enabled = json_object_get(job_spec, "enabled");
  ASSERT_TRUE(json_is_boolean(job_enabled));
  EXPECT_TRUE(json_is_false(job_enabled));

  const auto missing_job_response = server.Get(
      "/v1/deployments/prod/directors/bareos-dir/jobs/MissingJob/prefill");
  EXPECT_EQ(missing_job_response.status_code, 400u);
  EXPECT_NE(
      missing_job_response.body.find("director job 'MissingJob' not found."),
      std::string::npos);

  const auto missing_deployment_response = server.Get(
      "/v1/deployments/missing/directors/bareos-dir/jobs/ManagedJob/prefill");
  EXPECT_EQ(missing_deployment_response.status_code, 404u);
  EXPECT_NE(missing_deployment_response.body.find("deployment not found."),
            std::string::npos);
#endif
}

}  // namespace
}  // namespace bconfig::service
