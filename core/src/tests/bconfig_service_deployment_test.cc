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
TEST(BconfigService, CreatesDeploymentScaffold)
{
  ScopedDirectory repo_path{MakeTempPath()};
  ServiceState state;

  auto result
      = state.CreateDeployment({.id = "prod",
                                .name = "Production",
                                .repository_path = repo_path.path(),
                                .workflow_mode = WorkflowMode::kReview});

  ASSERT_TRUE(result);
  ASSERT_TRUE(std::filesystem::exists(
      RepositoryLayout::ManifestPath(repo_path.path())));
  EXPECT_TRUE(std::filesystem::is_directory(
      RepositoryLayout::ServiceDirectory(repo_path.path())));
  EXPECT_TRUE(std::filesystem::is_directory(
      RepositoryLayout::DirectorsDirectory(repo_path.path())));
  EXPECT_TRUE(std::filesystem::is_directory(
      RepositoryLayout::StoragesDirectory(repo_path.path())));
  EXPECT_TRUE(std::filesystem::is_directory(
      RepositoryLayout::ClientsDirectory(repo_path.path())));
  EXPECT_TRUE(std::filesystem::is_directory(
      RepositoryLayout::ConsolesDirectory(repo_path.path())));
  EXPECT_TRUE(std::filesystem::is_directory(repo_path.path() / ".git"));

  std::ifstream manifest{RepositoryLayout::ManifestPath(repo_path.path())};
  ASSERT_TRUE(manifest.good());
  const std::string manifest_text((std::istreambuf_iterator<char>(manifest)),
                                  std::istreambuf_iterator<char>());
  EXPECT_NE(manifest_text.find("\"id\": \"prod\""), std::string::npos);
  EXPECT_NE(manifest_text.find("\"workflow_mode\": \"review\""),
            std::string::npos);

  auto git_status = state.GetDeploymentGitStatus("prod");
  ASSERT_TRUE(git_status);
  EXPECT_TRUE(git_status.value->initialized);
  EXPECT_TRUE(git_status.value->clean);
  EXPECT_FALSE(git_status.value->has_staged_changes);
  EXPECT_FALSE(git_status.value->has_untracked_files);

  auto diff_preview = state.GetDeploymentDiffPreview("prod");
  ASSERT_TRUE(diff_preview);
  EXPECT_TRUE(diff_preview.value->initialized);
  EXPECT_FALSE(diff_preview.value->has_changes);
  EXPECT_TRUE(diff_preview.value->diff.empty());
  EXPECT_TRUE(diff_preview.value->untracked_files.empty());
}

TEST(BconfigService, BootstrapsConfigRootsWithoutImport)
{
  ScopedDirectory repo_path{MakeTempPath()};
  ScopedDirectory runtime_path{MakeTempPath()};
  ScopedDirectory installed_config_root{MakeTempPath()};
  const auto systemctl_log = runtime_path.path() / "systemctl.log";
#if defined(HAVE_DYNAMIC_SD_BACKENDS)
  ScopedDirectory backend_path{MakeTempPath()};
  ScopedEnvironmentVariable backend_override{"BCONFIG_BAREOS_SD_BACKEND_DIR",
                                             backend_path.path().string()};
  (void)backend_override;
#endif
#if !HAVE_WIN32
  const auto make_fake_daemon = [](const std::filesystem::path& path) {
    WriteTextFile(path,
                  "#!/bin/sh\n"
                  "trap 'exit 0' TERM INT\n"
                  "sleep 30 &\n"
                  "wait $!\n");
    std::filesystem::permissions(path,
                                 std::filesystem::perms::owner_read
                                     | std::filesystem::perms::owner_write
                                     | std::filesystem::perms::owner_exec,
                                 std::filesystem::perm_options::replace);
  };
  const auto fake_dir_binary = runtime_path.path() / "bareos-dir";
  const auto fake_sd_binary = runtime_path.path() / "bareos-sd";
  const auto fake_fd_binary = runtime_path.path() / "bareos-fd";
  const auto fake_systemctl = runtime_path.path() / "systemctl";
  make_fake_daemon(fake_dir_binary);
  make_fake_daemon(fake_sd_binary);
  make_fake_daemon(fake_fd_binary);
  WriteTextFile(installed_config_root.path() / "bareos-dir.d/stale.conf",
                "stale director config\n");
  WriteTextFile(installed_config_root.path() / "bareos-fd.d/stale.conf",
                "stale client config\n");
  WriteTextFile(installed_config_root.path() / "bareos-sd.d/stale.conf",
                "stale storage config\n");
  WriteTextFile(installed_config_root.path() / "bconsole.conf",
                "stale console config\n");
  WriteTextFile(
      fake_systemctl,
      "#!/bin/sh\n"
      "printf '%s\\n' \"$*\" >> \"$BCONFIG_TEST_SYSTEMCTL_LOG\"\n"
      "case \"$1 $2\" in\n"
      "  'reset-failed bareos-fd.service'|'reset-failed "
      "bareos-sd.service'|'reset-failed bareos-dir.service')\n"
      "    printf 'Failed to reset failed state of unit %s: Unit %s not "
      "loaded.\\n' \"$2\" \"$2\"\n"
      "    exit 1 ;;\n"
      "  'start bareos-fd.service') test -f "
      "\"$BCONFIG_TEST_INSTALLED_CONFIG_ROOT/bareos-fd.d/client/"
      "bareos-fd.conf\" "
      "&& exit 0 ;;\n"
      "  'start bareos-sd.service') test -f "
      "\"$BCONFIG_TEST_INSTALLED_CONFIG_ROOT/bareos-sd.d/storage/"
      "bareos-sd.conf\" "
      "&& exit 0 ;;\n"
      "  'start bareos-dir.service') test -f "
      "\"$BCONFIG_TEST_INSTALLED_CONFIG_ROOT/bareos-dir.d/director/"
      "bareos-dir.conf\" "
      "&& test -f \"$BCONFIG_TEST_INSTALLED_CONFIG_ROOT/bconsole.conf\" "
      "&& exit 0 ;;\n"
      "  '--quiet is-active') exit 0 ;;\n"
      "  '--no-pager --full') printf 'active (%s)\\n' \"$4\"; exit 0 ;;\n"
      "esac\n"
      "exit 1\n");
  std::filesystem::permissions(fake_systemctl,
                               std::filesystem::perms::owner_read
                                   | std::filesystem::perms::owner_write
                                   | std::filesystem::perms::owner_exec,
                               std::filesystem::perm_options::replace);
  ScopedEnvironmentVariable dir_binary_override{"BCONFIG_BAREOS_DIR_BINARY",
                                                fake_dir_binary.string()};
  ScopedEnvironmentVariable sd_binary_override{"BCONFIG_BAREOS_SD_BINARY",
                                               fake_sd_binary.string()};
  ScopedEnvironmentVariable fd_binary_override{"BCONFIG_BAREOS_FD_BINARY",
                                               fake_fd_binary.string()};
  ScopedEnvironmentVariable systemctl_override{"BCONFIG_SYSTEMCTL_BINARY",
                                               fake_systemctl.string()};
  ScopedEnvironmentVariable systemctl_log_override{"BCONFIG_TEST_SYSTEMCTL_LOG",
                                                   systemctl_log.string()};
  ScopedEnvironmentVariable installed_config_root_override{
      "BCONFIG_BAREOS_CONFIG_ROOT", installed_config_root.path().string()};
  ScopedEnvironmentVariable installed_config_root_log_override{
      "BCONFIG_TEST_INSTALLED_CONFIG_ROOT",
      installed_config_root.path().string()};
  (void)dir_binary_override;
  (void)sd_binary_override;
  (void)fd_binary_override;
  (void)systemctl_override;
  (void)systemctl_log_override;
  (void)installed_config_root_override;
  (void)installed_config_root_log_override;
#endif
  ServiceState state;

  auto deployment
      = state.CreateDeployment({.id = "prod",
                                .name = "Production",
                                .repository_path = repo_path.path()});
  ASSERT_TRUE(deployment);

  auto director_messages = state.UpsertDirectorMessagesResource(
      "prod", "bareos-dir", "Daemon",
      {.description = std::string{"Managed daemon messages"},
       .console_entries = std::vector<std::string>{"all"}});
  ASSERT_TRUE(director_messages) << director_messages.error;

  auto director = state.UpsertDirectorDaemonResource(
      "prod", "bareos-dir",
      {.address = std::string{"127.0.0.1"},
       .port = 9101,
       .password = std::string{"director-secret"},
       .working_directory = (runtime_path.path() / "director").string(),
       .messages = std::string{"Daemon"}});
  ASSERT_TRUE(director) << director.error;

  auto storage_messages = state.UpsertStorageMessagesResource(
      "prod", "bareos-sd", "Standard",
      {.description = std::string{"Managed storage messages"},
       .director_entries = std::vector<std::string>{"bareos-dir = all"}});
  ASSERT_TRUE(storage_messages) << storage_messages.error;

  auto storage = state.UpsertStorageDaemonResource(
      "prod", "bareos-sd",
      {.address = std::string{"127.0.0.1"},
       .port = 9103,
       .working_directory = (runtime_path.path() / "storage").string(),
       .messages = std::string{"Standard"}});
  ASSERT_TRUE(storage) << storage.error;

  auto client_messages = state.UpsertClientMessagesResource(
      "prod", "bareos-fd", "Standard",
      {.description = std::string{"Managed client messages"},
       .director_entries
       = std::vector<std::string>{"bareos-dir = all, !skipped, !restored"}});
  ASSERT_TRUE(client_messages) << client_messages.error;

  auto client = state.UpsertClientDaemonResource(
      "prod", "bareos-fd",
      {.address = std::string{"127.0.0.1"},
       .port = 9102,
       .working_directory = (runtime_path.path() / "client").string(),
       .messages = std::string{"Standard"}});
  ASSERT_TRUE(client) << client.error;

  auto director_client = state.UpsertDirectorClientResource(
      "prod", "bareos-dir", "bareos-fd",
      {.address = std::string{"127.0.0.1"},
       .password = std::string{"director-secret"}});
  ASSERT_TRUE(director_client) << director_client.error;

  auto director_storage = state.UpsertDirectorStorageResource(
      "prod", "bareos-dir", "bareos-sd",
      {.address = std::string{"127.0.0.1"},
       .password = std::string{"director-secret"},
       .device = std::string{"bareos-sd"},
       .media_type = std::string{"File"},
       .archive_device = std::string{"storage"},
       .device_type = std::string{"File"}});
  ASSERT_TRUE(director_storage) << director_storage.error;

  auto director_pool = state.UpsertDirectorPoolResource(
      "prod", "bareos-dir", "Default",
      {.pool_type = std::string{"Backup"},
       .label_format = std::string{"Default-"},
       .storages = std::vector<std::string>{"bareos-sd"},
       .use_catalog = false});
  ASSERT_TRUE(director_pool) << director_pool.error;

  auto director_schedule = state.UpsertDirectorScheduleResource(
      "prod", "bareos-dir", "Default",
      {.enabled = true,
       .run_entries = std::vector<std::string>{"Level=Full 1st sun at 21:00"}});
  ASSERT_TRUE(director_schedule) << director_schedule.error;

  auto director_fileset = state.UpsertDirectorFilesetResource(
      "prod", "bareos-dir", "Default",
      {.include_blocks
       = std::vector<std::string>{"  Include {\n    File = /tmp\n  }\n"}});
  ASSERT_TRUE(director_fileset) << director_fileset.error;

  auto director_job = state.UpsertDirectorJobResource(
      "prod", "bareos-dir", "DefaultBackup",
      {.type = std::string{"Backup"},
       .level = std::string{"Full"},
       .messages = std::string{"Daemon"},
       .storages = std::vector<std::string>{"bareos-sd"},
       .pool = std::string{"Default"},
       .client = std::string{"bareos-fd"},
       .fileset = std::string{"Default"},
       .schedule = std::string{"Default"}});
  ASSERT_TRUE(director_job) << director_job.error;

  auto console_director = state.UpsertConsoleDirectorResource(
      "prod", "admin", "bareos-dir",
      {.address = std::string{"127.0.0.1"},
       .port = 9101,
       .password = std::string{"console-secret"}});
  ASSERT_TRUE(console_director) << console_director.error;

  auto console_resource = state.UpsertConsoleConsoleResource(
      "prod", "admin", "admin",
      {.director = std::string{"bareos-dir"},
       .password = std::string{"console-secret"}});
  ASSERT_TRUE(console_resource) << console_resource.error;

  auto webui_profile = state.UpsertDirectorProfileResource(
      "prod", "bareos-dir", "webui-admin",
      {.job_acl = std::vector<std::string>{"*all*"},
       .client_acl = std::vector<std::string>{"*all*"},
       .storage_acl = std::vector<std::string>{"*all*"},
       .schedule_acl = std::vector<std::string>{"*all*"},
       .pool_acl = std::vector<std::string>{"*all*"},
       .command_acl
       = std::vector<std::string>{"!.bvfs_clear_cache", "!.exit", "!.sql",
                                  "!configure", "!create", "!delete", "!purge",
                                  "!prune", "!sqlquery", "!umount", "!unmount",
                                  "*all*"},
       .fileset_acl = std::vector<std::string>{"*all*"},
       .catalog_acl = std::vector<std::string>{"*all*"},
       .where_acl = std::vector<std::string>{"*all*"},
       .plugin_options_acl = std::vector<std::string>{"*all*"}});
  ASSERT_TRUE(webui_profile) << webui_profile.error;

  auto director_console = state.UpsertDirectorConsoleResource(
      "prod", "bareos-dir", "admin",
      {.password = std::string{"console-secret"},
       .profiles = std::vector<std::string>{"webui-admin"}});
  ASSERT_TRUE(director_console) << director_console.error;

  auto validate_job = state.CreateJob({.type = "validate_deployment_repo",
                                       .deployment_id = std::string{"prod"}});
  ASSERT_TRUE(validate_job);
  auto validated = WaitForJobTerminal(state, validate_job.value->id);
  ASSERT_TRUE(validated.has_value());
  EXPECT_EQ(validated->status, JobStatus::kSucceeded);

  const auto profile_text = ReadTextFile(
      repo_path.path()
      / "directors/bareos-dir/bareos-dir.d/profile/webui-admin.conf");
  EXPECT_NE(profile_text.find("CommandACL = !.bvfs_clear_cache, !.exit, !.sql, "
                              "!configure, !create, !delete, !purge, !prune, "
                              "!sqlquery, !umount, !unmount, *all*"),
            std::string::npos);

  const auto console_text
      = ReadTextFile(repo_path.path()
                     / "directors/bareos-dir/bareos-dir.d/console/admin.conf");
  EXPECT_NE(console_text.find("Profile = webui-admin"), std::string::npos);

#if HAVE_WIN32
  auto smoke_job = state.CreateJob(
      {.type = "smoke_test_deployment", .deployment_id = std::string{"prod"}});
  ASSERT_TRUE(smoke_job);
  auto smoked = WaitForJobTerminal(state, smoke_job.value->id);
  ASSERT_TRUE(smoked.has_value());
  EXPECT_EQ(smoked->status, JobStatus::kFailed);
#else
  const auto format_logs = [](const JobRecord& job) {
    std::string joined;
    for (const auto& log : job.logs) {
      if (!joined.empty()) { joined += '\n'; }
      joined += log;
    }
    return joined;
  };

  auto smoke_job = state.CreateJob(
      {.type = "smoke_test_deployment", .deployment_id = std::string{"prod"}});
  ASSERT_TRUE(smoke_job);
  auto smoked = WaitForJobTerminal(state, smoke_job.value->id);
  ASSERT_TRUE(smoked.has_value());
  EXPECT_EQ(smoked->status, JobStatus::kSucceeded) << format_logs(*smoked);
  EXPECT_NE(format_logs(*smoked).find("assigned archive directory ownership"),
            std::string::npos);

  WriteTextFile(runtime_path.path() / "director" / "bareos-dir.conmsg",
                "stale console messages\n");
  ASSERT_TRUE(std::filesystem::exists(runtime_path.path() / "director"
                                      / "bareos-dir.conmsg"));

  auto start_job = state.CreateJob({.type = "start_deployment_daemons",
                                    .deployment_id = std::string{"prod"}});
  ASSERT_TRUE(start_job);
  auto started = WaitForJobTerminal(state, start_job.value->id);
  ASSERT_TRUE(started.has_value());
  EXPECT_EQ(started->status, JobStatus::kSucceeded) << format_logs(*started);
  EXPECT_TRUE(std::filesystem::is_directory(runtime_path.path() / "storage"));
  EXPECT_TRUE(std::filesystem::exists(runtime_path.path() / "director"
                                      / "bareos-dir.conmsg"));
  const auto systemctl_calls = ReadTextFile(systemctl_log);
  EXPECT_NE(systemctl_calls.find("reset-failed bareos-fd.service"),
            std::string::npos);
  EXPECT_NE(systemctl_calls.find("start bareos-fd.service"), std::string::npos);
  EXPECT_NE(systemctl_calls.find("--quiet is-active bareos-fd.service"),
            std::string::npos);
  EXPECT_NE(systemctl_calls.find("reset-failed bareos-sd.service"),
            std::string::npos);
  EXPECT_NE(systemctl_calls.find("start bareos-sd.service"), std::string::npos);
  EXPECT_NE(systemctl_calls.find("--quiet is-active bareos-sd.service"),
            std::string::npos);
  EXPECT_NE(systemctl_calls.find("reset-failed bareos-dir.service"),
            std::string::npos);
  EXPECT_NE(systemctl_calls.find("start bareos-dir.service"),
            std::string::npos);
  EXPECT_NE(systemctl_calls.find("--quiet is-active bareos-dir.service"),
            std::string::npos);
  EXPECT_TRUE(std::filesystem::is_directory(installed_config_root.path()
                                            / "bareos-dir.d"));
  EXPECT_TRUE(std::filesystem::is_directory(installed_config_root.path()
                                            / "bareos-fd.d"));
  EXPECT_TRUE(std::filesystem::is_directory(installed_config_root.path()
                                            / "bareos-sd.d"));
  EXPECT_TRUE(std::filesystem::is_regular_file(installed_config_root.path()
                                               / "bconsole.conf"));
  EXPECT_FALSE(std::filesystem::exists(installed_config_root.path()
                                       / "bareos-dir.d/stale.conf"));
  EXPECT_FALSE(std::filesystem::exists(installed_config_root.path()
                                       / "bareos-fd.d/stale.conf"));
  EXPECT_FALSE(std::filesystem::exists(installed_config_root.path()
                                       / "bareos-sd.d/stale.conf"));
  const auto installed_console
      = ReadTextFile(installed_config_root.path() / "bconsole.conf");
  EXPECT_NE(installed_console.find("Console {"), std::string::npos);
#endif
}

TEST(BconfigService, RejectsDuplicateDeploymentIds)
{
  ScopedDirectory repo_path_a{MakeTempPath()};
  ScopedDirectory repo_path_b{MakeTempPath()};
  ServiceState state;

  auto first = state.CreateDeployment({.id = "prod",
                                       .name = "Production",
                                       .repository_path = repo_path_a.path()});
  ASSERT_TRUE(first);

  auto second = state.CreateDeployment({.id = "prod",
                                        .name = "Production Copy",
                                        .repository_path = repo_path_b.path()});
  ASSERT_FALSE(second);
  EXPECT_EQ(second.error, "deployment id already exists.");
}

#if !HAVE_WIN32
TEST(BconfigService, InitializesCatalogDatabaseWithHelperScripts)
{
  ScopedDirectory repo_path{MakeTempPath()};
  ScopedDirectory runtime_path{MakeTempPath()};
  ScopedDirectory tools_path{MakeTempPath()};
  const auto psql_log = tools_path.path() / "psql.log";
  const auto script_log = tools_path.path() / "scripts.log";

  const auto make_executable
      = [](const std::filesystem::path& path, std::string_view content) {
          WriteTextFile(path, content);
          std::filesystem::permissions(path,
                                       std::filesystem::perms::owner_read
                                           | std::filesystem::perms::owner_write
                                           | std::filesystem::perms::owner_exec,
                                       std::filesystem::perm_options::replace);
        };

  make_executable(tools_path.path() / "psql",
                  "#!/bin/sh\n"
                  "printf '%s\\n' \"$*\" >> \"$BCONFIG_TEST_PSQL_LOG\"\n"
                  "case \"$*\" in\n"
                  "  *pg_catalog.pg_roles*) exit 0 ;;\n"
                  "  *pg_catalog.pg_database*) exit 0 ;;\n"
                  "  *information_schema.tables*) exit 0 ;;\n"
                  "esac\n"
                  "exit 0\n");
  make_executable(
      tools_path.path() / "create_bareos_database",
      "#!/bin/sh\n"
      "echo create_bareos_database >> \"$BCONFIG_TEST_SCRIPT_LOG\"\n"
      "exit 0\n");
  make_executable(tools_path.path() / "make_bareos_tables",
                  "#!/bin/sh\n"
                  "echo make_bareos_tables >> \"$BCONFIG_TEST_SCRIPT_LOG\"\n"
                  "exit 0\n");
  make_executable(
      tools_path.path() / "grant_bareos_privileges",
      "#!/bin/sh\n"
      "echo grant_bareos_privileges >> \"$BCONFIG_TEST_SCRIPT_LOG\"\n"
      "exit 0\n");

  ScopedEnvironmentVariable psql_override{
      "BCONFIG_PSQL_BINARY", (tools_path.path() / "psql").string()};
  ScopedEnvironmentVariable create_override{
      "BCONFIG_BAREOS_CREATE_DATABASE_SCRIPT",
      (tools_path.path() / "create_bareos_database").string()};
  ScopedEnvironmentVariable tables_override{
      "BCONFIG_BAREOS_MAKE_TABLES_SCRIPT",
      (tools_path.path() / "make_bareos_tables").string()};
  ScopedEnvironmentVariable grant_override{
      "BCONFIG_BAREOS_GRANT_PRIVILEGES_SCRIPT",
      (tools_path.path() / "grant_bareos_privileges").string()};
  ScopedEnvironmentVariable psql_log_override{"BCONFIG_TEST_PSQL_LOG",
                                              psql_log.string()};
  ScopedEnvironmentVariable script_log_override{"BCONFIG_TEST_SCRIPT_LOG",
                                                script_log.string()};
  (void)psql_override;
  (void)create_override;
  (void)tables_override;
  (void)grant_override;
  (void)psql_log_override;
  (void)script_log_override;

  ServiceState state;

  auto deployment
      = state.CreateDeployment({.id = "prod",
                                .name = "Production",
                                .repository_path = repo_path.path()});
  ASSERT_TRUE(deployment);

  auto messages = state.UpsertDirectorMessagesResource(
      "prod", "bareos-dir", "Daemon",
      {.description = std::string{"Managed daemon messages"},
       .console_entries = std::vector<std::string>{"all"}});
  ASSERT_TRUE(messages) << messages.error;

  auto director = state.UpsertDirectorDaemonResource(
      "prod", "bareos-dir",
      {.address = std::string{"127.0.0.1"},
       .port = 39101,
       .password = std::string{"director-secret"},
       .working_directory = (runtime_path.path() / "director").string(),
       .messages = std::string{"Daemon"}});
  ASSERT_TRUE(director) << director.error;

  auto catalog
      = state.UpsertDirectorCatalogResource("prod", "bareos-dir", "MyCatalog",
                                            {.db_password = std::string{""},
                                             .db_user = std::string{"bareos"},
                                             .db_name = std::string{"bareos"}});
  ASSERT_TRUE(catalog) << catalog.error;

  auto job = state.CreateJob({.type = "initialize_catalog_database",
                              .deployment_id = std::string{"prod"}});
  ASSERT_TRUE(job);

  auto finished = WaitForJobTerminal(state, job.value->id);
  ASSERT_TRUE(finished.has_value());
  EXPECT_EQ(finished->status, JobStatus::kSucceeded);
  EXPECT_NE(std::find(finished->logs.begin(), finished->logs.end(),
                      "created database 'bareos'"),
            finished->logs.end());
  EXPECT_NE(std::find(finished->logs.begin(), finished->logs.end(),
                      "created catalog tables in 'bareos'"),
            finished->logs.end());
  EXPECT_NE(std::find(finished->logs.begin(), finished->logs.end(),
                      "created database user 'bareos' and granted catalog "
                      "privileges"),
            finished->logs.end());

  EXPECT_EQ(ReadTextFile(script_log),
            "create_bareos_database\nmake_bareos_tables\n"
            "grant_bareos_privileges\n");
  const auto psql_calls = ReadTextFile(psql_log);
  EXPECT_NE(psql_calls.find("pg_catalog.pg_roles"), std::string::npos);
  EXPECT_NE(psql_calls.find("pg_catalog.pg_database"), std::string::npos);
  EXPECT_NE(psql_calls.find("information_schema.tables"), std::string::npos);
}
#endif

TEST(BconfigService, ExpandsTildeRepositoryPaths)
{
  ScopedDirectory home_path{MakeTempPath()};
  ScopedEnvironmentVariable home{"HOME", home_path.path().string()};
  ServiceState state;

  auto deployment = state.CreateDeployment(
      {.id = "prod", .name = "Production", .repository_path = "~/repo"});
  ASSERT_TRUE(deployment);
  EXPECT_EQ(deployment.value->repository_path, home_path.path() / "repo");
  EXPECT_TRUE(std::filesystem::exists(
      RepositoryLayout::ManifestPath(home_path.path() / "repo")));
}

TEST(BconfigService, RunsValidationJobsForKnownDeployments)
{
  ScopedDirectory repo_path{MakeTempPath()};
  ServiceState state;

  auto deployment
      = state.CreateDeployment({.id = "prod",
                                .name = "Production",
                                .repository_path = repo_path.path()});
  ASSERT_TRUE(deployment);

  auto job = state.CreateJob({.type = "validate_deployment_repo",
                              .deployment_id = std::string{"prod"}});
  ASSERT_TRUE(job);
  auto finished = WaitForJobTerminal(state, job.value->id);
  ASSERT_TRUE(finished.has_value());
  EXPECT_EQ(finished->deployment_id, std::optional<std::string>{"prod"});
  EXPECT_EQ(finished->id, "job-1");
  EXPECT_EQ(finished->status, JobStatus::kSucceeded);
  EXPECT_FALSE(finished->logs.empty());
}

TEST(BconfigService, ValidatesImportedDeploymentConfigs)
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

  auto validate_job = state.CreateJob({.type = "validate_deployment_repo",
                                       .deployment_id = std::string{"prod"}});
  ASSERT_TRUE(validate_job);
  auto validated = WaitForJobTerminal(state, validate_job.value->id);
  ASSERT_TRUE(validated.has_value());
  EXPECT_EQ(validated->status, JobStatus::kSucceeded);
  EXPECT_NE(std::find(validated->logs.begin(), validated->logs.end(),
                      "validated director 'bareos-dir'"),
            validated->logs.end());
  EXPECT_NE(std::find(validated->logs.begin(), validated->logs.end(),
                      "validated client 'backup-bareos-test-fd'"),
            validated->logs.end());
  EXPECT_NE(std::find(validated->logs.begin(), validated->logs.end(),
                      "validated client 'bareos-fd'"),
            validated->logs.end());
}

TEST(BconfigService, PersistsDeploymentsAndJobsAcrossRestart)
{
  ScopedDirectory state_path{MakeTempPath()};
  ScopedDirectory repo_path{MakeTempPath()};

  {
    ServiceState state{state_path.path()};
    auto deployment
        = state.CreateDeployment({.id = "prod",
                                  .name = "Production",
                                  .repository_path = repo_path.path(),
                                  .workflow_mode = WorkflowMode::kReview});
    ASSERT_TRUE(deployment);

    auto job = state.CreateJob({.type = "validate_deployment_repo",
                                .deployment_id = std::string{"prod"}});
    ASSERT_TRUE(job);
    auto finished = WaitForJobTerminal(state, job.value->id);
    ASSERT_TRUE(finished.has_value());
    EXPECT_EQ(finished->id, "job-1");
  }

  {
    ServiceState reloaded{state_path.path()};
    auto deployment = reloaded.GetDeployment("prod");
    ASSERT_TRUE(deployment.has_value());
    EXPECT_EQ(deployment->workflow_mode, WorkflowMode::kReview);
    EXPECT_EQ(reloaded.ListDeployments().size(), 1u);
    EXPECT_EQ(reloaded.ListJobs().size(), 1u);

    auto next_job = reloaded.CreateJob({.type = "validate_deployment_repo",
                                        .deployment_id = std::string{"prod"}});
    ASSERT_TRUE(next_job);
    auto finished = WaitForJobTerminal(reloaded, next_job.value->id);
    ASSERT_TRUE(finished.has_value());
    EXPECT_EQ(finished->id, "job-2");
    EXPECT_EQ(finished->status, JobStatus::kSucceeded);
  }
}

TEST(BconfigService, CreatesStorageBootstrapSessions)
{
  ScopedDirectory repo_path{MakeTempPath()};
  ServiceState state;

  auto deployment
      = state.CreateDeployment({.id = "prod",
                                .name = "Production",
                                .repository_path = repo_path.path(),
                                .workflow_mode = WorkflowMode::kReview});
  ASSERT_TRUE(deployment);

  auto session = state.CreateStorageBootstrapSession(
      {.deployment_id = "prod",
       .storage_name = std::string{"bareos-sd"},
       .ttl_seconds = 600});
  ASSERT_TRUE(session) << session.error;
  EXPECT_EQ(session.value->id, "storage-bootstrap-1");
  EXPECT_EQ(session.value->deployment_id, "prod");
  ASSERT_TRUE(session.value->storage_name.has_value());
  EXPECT_EQ(*session.value->storage_name, "bareos-sd");
  EXPECT_EQ(session.value->status, StorageBootstrapSessionStatus::kPending);
  EXPECT_EQ(session.value->bootstrap_token.size(), 32U);
  EXPECT_GT(session.value->expires_at_epoch_seconds, 0U);

  auto loaded = state.GetStorageBootstrapSession(session.value->id);
  ASSERT_TRUE(loaded.has_value());
  EXPECT_EQ(loaded->bootstrap_token, session.value->bootstrap_token);

  auto authenticated = state.AuthenticateStorageBootstrapSession(
      session.value->id, session.value->bootstrap_token);
  ASSERT_TRUE(authenticated) << authenticated.error;
  EXPECT_EQ(authenticated.value->id, session.value->id);

  auto rejected = state.AuthenticateStorageBootstrapSession(session.value->id,
                                                            "bad-token");
  ASSERT_FALSE(rejected);
  EXPECT_EQ(state.ListStorageBootstrapSessions().size(), 1U);
}

TEST(BconfigService, ValidatesStorageBootstrapSessionTransitions)
{
  ScopedDirectory repo_path{MakeTempPath()};
  ServiceState state;

  auto deployment
      = state.CreateDeployment({.id = "prod",
                                .name = "Production",
                                .repository_path = repo_path.path()});
  ASSERT_TRUE(deployment);

  auto session = state.CreateStorageBootstrapSession({.deployment_id = "prod"});
  ASSERT_TRUE(session) << session.error;

  auto invalid = state.TransitionStorageBootstrapSession(
      session.value->id, StorageBootstrapSessionStatus::kSelected);
  ASSERT_FALSE(invalid);

  auto registered = state.TransitionStorageBootstrapSession(
      session.value->id, StorageBootstrapSessionStatus::kRegistered,
      std::nullopt, std::string{"sdhost"}, std::string{"sdhost.example.com"});
  ASSERT_TRUE(registered) << registered.error;
  EXPECT_EQ(registered.value->status,
            StorageBootstrapSessionStatus::kRegistered);
  ASSERT_TRUE(registered.value->hostname.has_value());
  EXPECT_EQ(*registered.value->hostname, "sdhost");
  ASSERT_TRUE(registered.value->fqdn.has_value());
  EXPECT_EQ(*registered.value->fqdn, "sdhost.example.com");

  auto discovered = state.TransitionStorageBootstrapSession(
      session.value->id, StorageBootstrapSessionStatus::kDiscovered);
  ASSERT_TRUE(discovered) << discovered.error;
  EXPECT_EQ(discovered.value->status,
            StorageBootstrapSessionStatus::kDiscovered);

  auto failed = state.TransitionStorageBootstrapSession(
      session.value->id, StorageBootstrapSessionStatus::kFailed,
      std::string{"discovery upload failed"});
  ASSERT_TRUE(failed) << failed.error;
  EXPECT_EQ(failed.value->status, StorageBootstrapSessionStatus::kFailed);
  ASSERT_TRUE(failed.value->last_error.has_value());
  EXPECT_EQ(*failed.value->last_error, "discovery upload failed");

  auto terminal = state.TransitionStorageBootstrapSession(
      session.value->id, StorageBootstrapSessionStatus::kSelected);
  ASSERT_FALSE(terminal);
}

TEST(BconfigService, PersistsAndExpiresStorageBootstrapSessionsAcrossRestart)
{
  ScopedDirectory state_path{MakeTempPath()};
  ScopedDirectory repo_path{MakeTempPath()};

  std::string bootstrap_token;
  {
    ServiceState state{state_path.path()};
    auto deployment
        = state.CreateDeployment({.id = "prod",
                                  .name = "Production",
                                  .repository_path = repo_path.path()});
    ASSERT_TRUE(deployment);

    auto session = state.CreateStorageBootstrapSession(
        {.deployment_id = "prod", .ttl_seconds = 0});
    ASSERT_TRUE(session) << session.error;
    bootstrap_token = session.value->bootstrap_token;
  }

  {
    ServiceState reloaded{state_path.path()};
    auto session = reloaded.GetStorageBootstrapSession("storage-bootstrap-1");
    ASSERT_TRUE(session.has_value());
    EXPECT_EQ(session->status, StorageBootstrapSessionStatus::kExpired);

    auto rejected = reloaded.AuthenticateStorageBootstrapSession(
        session->id, bootstrap_token);
    ASSERT_FALSE(rejected);

    auto next = reloaded.CreateStorageBootstrapSession(
        {.deployment_id = "prod", .ttl_seconds = 600});
    ASSERT_TRUE(next) << next.error;
    EXPECT_EQ(next.value->id, "storage-bootstrap-2");
  }
}

TEST(BconfigService, ServesStorageBootstrapSessionApi)
{
  ScopedDirectory state_path{MakeTempPath()};
  ScopedDirectory repo_path{MakeTempPath()};

  {
    ServiceState state{state_path.path()};
    auto deployment
        = state.CreateDeployment({.id = "prod",
                                  .name = "Production",
                                  .repository_path = repo_path.path()});
    ASSERT_TRUE(deployment);

    auto storage_messages = state.UpsertStorageMessagesResource(
        "prod", "bareos-sd", "Standard",
        {.description = std::string{"Managed storage messages"},
         .director_entries = std::vector<std::string>{"bareos-dir = all"}});
    ASSERT_TRUE(storage_messages) << storage_messages.error;

    auto storage = state.UpsertStorageDaemonResource(
        "prod", "bareos-sd",
        {.address = std::string{"127.0.0.1"},
         .port = 9103,
         .working_directory = std::string{"/var/lib/bareos"},
         .messages = std::string{"Standard"}});
    ASSERT_TRUE(storage) << storage.error;

    auto storage_director = state.UpsertStorageDirectorResource(
        "prod", "bareos-sd", "bareos-dir",
        {.password = std::string{"storage-director-secret"},
         .description = std::string{
             "Director, who is permitted to contact this storage daemon."}});
    ASSERT_TRUE(storage_director) << storage_director.error;

    auto storage_device = state.UpsertStorageDeviceResource(
        "prod", "bareos-sd", "FileStorage",
        {.media_type = std::string{"File"},
         .archive_device = std::string{"/var/lib/bareos/storage"},
         .device_type = std::string{"File"},
         .removable_media = false,
         .random_access = true,
         .automatic_mount = true,
         .label_media = true,
         .always_open = false,
         .description = std::string{
             "File device. A connecting Director must have the same Name and "
             "MediaType."}});
    ASSERT_TRUE(storage_device) << storage_device.error;
  }

#if !HAVE_WIN32
  ScopedBconfigServiceServer server{state_path.path()};
  ASSERT_TRUE(server.ready()) << server.startup_error();

  const auto create_response = server.Post(
      "/v1/bootstrap/storage-sessions",
      R"({"deployment_id":"prod","storage_name":"bareos-sd","ttl_seconds":600})");
  ASSERT_EQ(create_response.status_code, 201u) << create_response.body;
  auto create_json = ParseJson(create_response.body);
  ASSERT_NE(create_json.get(), nullptr) << create_response.body;
  auto* created
      = json_object_get(create_json.get(), "storage_bootstrap_session");
  ASSERT_TRUE(json_is_object(created));
  auto* session_id = json_object_get(created, "id");
  auto* bootstrap_token = json_object_get(created, "bootstrap_token");
  auto* created_status = json_object_get(created, "status");
  ASSERT_TRUE(json_is_string(session_id));
  ASSERT_TRUE(json_is_string(bootstrap_token));
  ASSERT_TRUE(json_is_string(created_status));
  EXPECT_STREQ(json_string_value(created_status), "pending");

  const std::string session_id_text{json_string_value(session_id)};
  const std::string bootstrap_token_text{json_string_value(bootstrap_token)};
  const auto session_base_path
      = std::string{"/v1/bootstrap/storage-sessions/"} + session_id_text;

  const auto list_response = server.Get("/v1/bootstrap/storage-sessions");
  ASSERT_EQ(list_response.status_code, 200u) << list_response.body;
  auto list_json = ParseJson(list_response.body);
  ASSERT_NE(list_json.get(), nullptr) << list_response.body;
  auto* sessions
      = json_object_get(list_json.get(), "storage_bootstrap_sessions");
  ASSERT_TRUE(json_is_array(sessions));
  ASSERT_EQ(json_array_size(sessions), 1u);

  const auto register_response = server.Post(
      session_base_path + "/register",
      "{\"bootstrap_token\":\"" + bootstrap_token_text
          + "\",\"hostname\":\"sdhost\",\"fqdn\":\"sdhost.example.com\"}");
  ASSERT_EQ(register_response.status_code, 200u) << register_response.body;
  auto register_json = ParseJson(register_response.body);
  ASSERT_NE(register_json.get(), nullptr) << register_response.body;
  auto* registered
      = json_object_get(register_json.get(), "storage_bootstrap_session");
  ASSERT_TRUE(json_is_object(registered));
  EXPECT_STREQ(json_string_value(json_object_get(registered, "status")),
               "registered");

  const auto discovery_response = server.Post(
      session_base_path + "/discovery",
      "{\"bootstrap_token\":\"" + bootstrap_token_text
          + "\",\"hostname\":\"sdhost\",\"fqdn\":\"sdhost.example.com\","
            "\"report\":{\"hostname\":\"sdhost\",\"filesystems\":[]}}");
  ASSERT_EQ(discovery_response.status_code, 200u) << discovery_response.body;
  auto discovery_json = ParseJson(discovery_response.body);
  ASSERT_NE(discovery_json.get(), nullptr) << discovery_response.body;
  auto* discovered
      = json_object_get(discovery_json.get(), "storage_bootstrap_session");
  ASSERT_TRUE(json_is_object(discovered));
  EXPECT_STREQ(json_string_value(json_object_get(discovered, "status")),
               "discovered");
  auto* discovery_report = json_object_get(discovered, "discovery_report");
  ASSERT_TRUE(json_is_object(discovery_report));
  auto* discovery_hostname = json_object_get(discovery_report, "hostname");
  ASSERT_TRUE(json_is_string(discovery_hostname));
  EXPECT_STREQ(json_string_value(discovery_hostname), "sdhost");

  const auto selection_response = server.Post(
      session_base_path + "/selection",
      R"({"selection":{"filesystem_mountpoint":"/srv/storage","archive_path":"/srv/storage/bareos/storage"}})");
  ASSERT_EQ(selection_response.status_code, 200u) << selection_response.body;
  auto selection_json = ParseJson(selection_response.body);
  ASSERT_NE(selection_json.get(), nullptr) << selection_response.body;
  auto* selected
      = json_object_get(selection_json.get(), "storage_bootstrap_session");
  ASSERT_TRUE(json_is_object(selected));
  EXPECT_STREQ(json_string_value(json_object_get(selected, "status")),
               "selected");
  auto* selection = json_object_get(selected, "selection");
  ASSERT_TRUE(json_is_object(selection));
  auto* archive_path = json_object_get(selection, "archive_path");
  ASSERT_TRUE(json_is_string(archive_path));
  EXPECT_STREQ(json_string_value(archive_path), "/srv/storage/bareos/storage");

  const auto get_response = server.Get(session_base_path);
  ASSERT_EQ(get_response.status_code, 200u) << get_response.body;
  auto get_json = ParseJson(get_response.body);
  ASSERT_NE(get_json.get(), nullptr) << get_response.body;
  auto* loaded = json_object_get(get_json.get(), "storage_bootstrap_session");
  ASSERT_TRUE(json_is_object(loaded));
  EXPECT_STREQ(json_string_value(json_object_get(loaded, "status")),
               "selected");

  const auto config_response = server.Get(
      session_base_path + "/config-bundle?token=" + bootstrap_token_text);
  ASSERT_EQ(config_response.status_code, 200u) << config_response.body;
  auto config_json = ParseJson(config_response.body);
  ASSERT_NE(config_json.get(), nullptr) << config_response.body;
  auto* config_session
      = json_object_get(config_json.get(), "storage_bootstrap_session");
  ASSERT_TRUE(json_is_object(config_session));
  EXPECT_STREQ(json_string_value(json_object_get(config_session, "status")),
               "config_ready");
  auto* config_bundle = json_object_get(config_json.get(), "config_bundle");
  ASSERT_TRUE(json_is_object(config_bundle));
  auto* selected_archive_path
      = json_object_get(config_bundle, "selected_archive_path");
  ASSERT_TRUE(json_is_string(selected_archive_path));
  EXPECT_STREQ(json_string_value(selected_archive_path),
               "/srv/storage/bareos/storage");
  auto* files = json_object_get(config_bundle, "files");
  ASSERT_TRUE(json_is_array(files));

  json_t* file_entry = nullptr;
  size_t file_index = 0;
  json_array_foreach(files, file_index, file_entry)
  {
    auto* path = json_object_get(file_entry, "path");
    if (json_is_string(path)
        && std::string_view{json_string_value(path)}
               == "bareos-sd.d/device/FileStorage.conf") {
      break;
    }
    file_entry = nullptr;
  }
  ASSERT_NE(file_entry, nullptr);
  auto* device_content = json_object_get(file_entry, "content");
  ASSERT_TRUE(json_is_string(device_content));
  EXPECT_NE(std::string_view{json_string_value(device_content)}.find(
                "/srv/storage/bareos/storage"),
            std::string_view::npos);

  const auto applied_response
      = server.Post(session_base_path + "/applied",
                    "{\"bootstrap_token\":\"" + bootstrap_token_text
                        + "\",\"success\":false,\"error\":\"apply failed\"}");
  ASSERT_EQ(applied_response.status_code, 200u) << applied_response.body;
  auto applied_json = ParseJson(applied_response.body);
  ASSERT_NE(applied_json.get(), nullptr) << applied_response.body;
  auto* applied
      = json_object_get(applied_json.get(), "storage_bootstrap_session");
  ASSERT_TRUE(json_is_object(applied));
  EXPECT_STREQ(json_string_value(json_object_get(applied, "status")), "failed");
  auto* last_error = json_object_get(applied, "last_error");
  ASSERT_TRUE(json_is_string(last_error));
  EXPECT_STREQ(json_string_value(last_error), "apply failed");
#endif
}

TEST(BconfigService, ImportsDetectedComponentTreesFromConfigRoot)
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

  auto job = state.CreateJob({.type = "import_configuration",
                              .deployment_id = std::string{"prod"},
                              .source_path = source_root.path().string()});
  ASSERT_TRUE(job);

  auto finished = WaitForJobTerminal(state, job.value->id);
  ASSERT_TRUE(finished.has_value());
  EXPECT_EQ(finished->status, JobStatus::kSucceeded);

  const auto imported_root
      = RepositoryLayout::DirectorsDirectory(repo_path.path()) / "bareos-dir";
  EXPECT_TRUE(std::filesystem::is_directory(imported_root / "bareos-dir.d"));
  EXPECT_TRUE(std::filesystem::exists(imported_root
                                      / "bareos-dir.d/director/"
                                        "bareos-dir.conf"));
  const auto imported_client_root
      = RepositoryLayout::ClientsDirectory(repo_path.path())
        / "backup-bareos-test-fd";
  EXPECT_TRUE(
      std::filesystem::is_directory(imported_client_root / "bareos-fd.d"));
  EXPECT_TRUE(std::filesystem::exists(imported_client_root
                                      / "bareos-fd.d/client/"
                                        "myself.conf"));
  const auto generated_stub_root
      = RepositoryLayout::ClientsDirectory(repo_path.path()) / "bareos-fd";
  EXPECT_TRUE(
      std::filesystem::is_directory(generated_stub_root / "bareos-fd.d"));
  EXPECT_TRUE(std::filesystem::exists(generated_stub_root
                                      / "bareos-fd.d/director/"
                                        "bareos-dir.conf"));

  std::ifstream import_state{
      RepositoryLayout::ImportStatePath(repo_path.path())};
  ASSERT_TRUE(import_state.good());
  const std::string import_state_text(
      (std::istreambuf_iterator<char>(import_state)),
      std::istreambuf_iterator<char>());
  EXPECT_NE(import_state_text.find("\"component\": \"director\""),
            std::string::npos);
  EXPECT_NE(import_state_text.find("\"resource_name\": \"bareos-dir\""),
            std::string::npos);
  EXPECT_NE(import_state_text.find("\"component\": \"client\""),
            std::string::npos);
  EXPECT_NE(
      import_state_text.find("\"resource_name\": \"backup-bareos-test-fd\""),
      std::string::npos);
  EXPECT_NE(import_state_text.find("\"resource_name\": \"bareos-fd\""),
            std::string::npos);

  auto imports = state.ListDeploymentImports("prod");
  ASSERT_TRUE(imports);
  ASSERT_EQ(imports.value->size(), 6u);
  EXPECT_EQ(imports.value->at(0).job_id, job.value->id);
  EXPECT_EQ(imports.value->at(0).component, "director");
  EXPECT_EQ(imports.value->at(0).resource_name, "bareos-dir");
  EXPECT_EQ(imports.value->at(0).source_path,
            std::optional<std::string>{source_root.path().string()});
  EXPECT_EQ(imports.value->at(0).destination_path, "directors/bareos-dir");
  EXPECT_EQ(imports.value->at(1).component, "client");
  EXPECT_EQ(imports.value->at(1).resource_name, "backup-bareos-test-fd");
  EXPECT_EQ(imports.value->at(2).component, "client");
  EXPECT_EQ(imports.value->at(2).resource_name, "bareos-fd");

  auto git_status = state.GetDeploymentGitStatus("prod");
  ASSERT_TRUE(git_status);
  EXPECT_TRUE(git_status.value->initialized);
  EXPECT_FALSE(git_status.value->clean);
  EXPECT_FALSE(git_status.value->has_staged_changes);
  EXPECT_TRUE(git_status.value->has_untracked_files);
  EXPECT_NE(
      std::find(
          git_status.value->entries.begin(), git_status.value->entries.end(),
          "?? clients/backup-bareos-test-fd/bareos-fd.d/client/myself.conf"),
      git_status.value->entries.end());
  EXPECT_NE(
      std::find(git_status.value->entries.begin(),
                git_status.value->entries.end(),
                "?? clients/bareos-fd/bareos-fd.d/director/bareos-dir.conf"),
      git_status.value->entries.end());

  auto diff_preview = state.GetDeploymentDiffPreview("prod");
  ASSERT_TRUE(diff_preview);
  EXPECT_TRUE(diff_preview.value->initialized);
  EXPECT_TRUE(diff_preview.value->has_changes);
  EXPECT_NE(diff_preview.value->diff.find("service/import-state.json"),
            std::string::npos);
  EXPECT_NE(
      std::find(diff_preview.value->untracked_files.begin(),
                diff_preview.value->untracked_files.end(),
                "clients/backup-bareos-test-fd/bareos-fd.d/client/myself.conf"),
      diff_preview.value->untracked_files.end());
  EXPECT_NE(std::find(diff_preview.value->untracked_files.begin(),
                      diff_preview.value->untracked_files.end(),
                      "clients/bareos-fd/bareos-fd.d/director/bareos-dir.conf"),
            diff_preview.value->untracked_files.end());
}
TEST(BconfigService, CommitsDeploymentRepositoryChanges)
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

  auto commit_job = state.CreateJob(
      {.type = "commit_deployment_repo",
       .deployment_id = std::string{"prod"},
       .commit_message = std::string{"Import Bareos config root"}});
  ASSERT_TRUE(commit_job);
  auto committed = WaitForJobTerminal(state, commit_job.value->id);
  ASSERT_TRUE(committed.has_value());
  EXPECT_EQ(committed->status, JobStatus::kSucceeded);
  EXPECT_EQ(committed->commit_message,
            std::optional<std::string>{"Import Bareos config root"});
  EXPECT_NE(std::find_if(committed->logs.begin(), committed->logs.end(),
                         [](const std::string& log) {
                           return log.rfind("created commit ", 0) == 0;
                         }),
            committed->logs.end());

  auto git_status = state.GetDeploymentGitStatus("prod");
  ASSERT_TRUE(git_status);
  EXPECT_TRUE(git_status.value->initialized);
  EXPECT_TRUE(git_status.value->clean);
  EXPECT_FALSE(git_status.value->has_staged_changes);
  EXPECT_FALSE(git_status.value->has_untracked_files);

  auto diff_preview = state.GetDeploymentDiffPreview("prod");
  ASSERT_TRUE(diff_preview);
  EXPECT_TRUE(diff_preview.value->initialized);
  EXPECT_FALSE(diff_preview.value->has_changes);
  EXPECT_TRUE(diff_preview.value->diff.empty());
  EXPECT_TRUE(diff_preview.value->untracked_files.empty());
}

}  // namespace
}  // namespace bconfig::service
