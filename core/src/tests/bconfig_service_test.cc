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

#include "tools/bconfig_service.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <thread>

namespace bconfig::service {
namespace {

std::filesystem::path MakeTempPath()
{
  static std::atomic<uint64_t> counter{0};
  return std::filesystem::temp_directory_path()
         / ("bconfig-service-test-" + std::to_string(++counter));
}

class ScopedDirectory {
 public:
  explicit ScopedDirectory(std::filesystem::path path) : path_{std::move(path)}
  {
  }

  ~ScopedDirectory() { std::filesystem::remove_all(path_); }

  const std::filesystem::path& path() const { return path_; }

 private:
  std::filesystem::path path_;
};

class ScopedEnvironmentVariable {
 public:
  ScopedEnvironmentVariable(const char* name, const std::string& value)
      : name_{name}
  {
    if (const char* current = std::getenv(name_)) { previous_value_ = current; }
    had_previous_value_ = previous_value_.has_value();
    Set(value);
  }

  ~ScopedEnvironmentVariable()
  {
    if (had_previous_value_) {
      Set(*previous_value_);
    } else {
#if HAVE_WIN32
      _putenv_s(name_, "");
#else
      unsetenv(name_);
#endif
    }
  }

 private:
  void Set(const std::string& value)
  {
#if HAVE_WIN32
    _putenv_s(name_, value.c_str());
#else
    setenv(name_, value.c_str(), 1);
#endif
  }

  const char* name_;
  std::optional<std::string> previous_value_{};
  bool had_previous_value_{false};
};

std::filesystem::path FindFixtureRoot()
{
  auto cursor = std::filesystem::current_path();
  for (;;) {
    const auto build_tree_path = cursor / "configs/bareos-configparser-tests";
    if (std::filesystem::is_directory(build_tree_path)) {
      return build_tree_path;
    }

    const auto source_tree_path
        = cursor / "core/src/tests/configs/bareos-configparser-tests";
    if (std::filesystem::is_directory(source_tree_path)) {
      return source_tree_path;
    }

    if (cursor == cursor.root_path()) { break; }
    cursor = cursor.parent_path();
  }

  return {};
}

std::string ReadTextFile(const std::filesystem::path& path)
{
  std::ifstream file{path};
  EXPECT_TRUE(file.good());
  return std::string((std::istreambuf_iterator<char>(file)),
                     std::istreambuf_iterator<char>());
}

void WriteTextFile(const std::filesystem::path& path, std::string_view content)
{
  std::filesystem::create_directories(path.parent_path());
  std::ofstream file{path};
  ASSERT_TRUE(file.good());
  file << content;
  ASSERT_TRUE(file.good());
}

void AddCounterFixtures(const std::filesystem::path& source_root)
{
  WriteTextFile(source_root / "bareos-dir.d/counter/WrapSeed.conf",
                "Counter {\n"
                "  Name = \"WrapSeed\"\n"
                "  Minimum = 1\n"
                "  Maximum = 5\n"
                "}\n");
  WriteTextFile(source_root / "bareos-dir.d/counter/ExistingCounter.conf",
                "Counter {\n"
                "  Name = \"ExistingCounter\"\n"
                "  Description = \"Existing counter\"\n"
                "  Minimum = 7\n"
                "  Maximum = 99\n"
                "  WrapCounter = WrapSeed\n"
                "  Catalog = MyCatalog\n"
                "}\n");
}

std::optional<JobRecord> WaitForJobTerminal(ServiceState& state,
                                            std::string_view id)
{
  for (int attempt = 0; attempt < 100; ++attempt) {
    auto job = state.GetJob(id);
    if (job
        && (job->status == JobStatus::kSucceeded
            || job->status == JobStatus::kFailed)) {
      return job;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }

  return state.GetJob(id);
}

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
       .entries
       = std::vector<std::string>{"  Director = bareos-dir = all, !skipped"}});
  ASSERT_TRUE(created) << created.error;
  EXPECT_EQ(created.value->name, "backup-bareos-test-fd");

  const auto messages_path
      = created.value->path / "bareos-fd.d/messages/ManagedMessages.conf";
  const auto created_text = ReadTextFile(messages_path);
  EXPECT_NE(created_text.find("Messages {"), std::string::npos);
  EXPECT_NE(created_text.find("Name = \"ManagedMessages\""), std::string::npos);
  EXPECT_NE(created_text.find("Description = \"Managed client messages\""),
            std::string::npos);
  EXPECT_NE(created_text.find("Director = bareos-dir = all, !skipped"),
            std::string::npos);

  auto updated = state.UpsertClientMessagesResource(
      "prod", "backup-bareos-test-fd", "Standard",
      {.description = std::string{"Updated client messages"}});
  ASSERT_TRUE(updated) << updated.error;

  const auto updated_text = ReadTextFile(
      updated.value->path / "bareos-fd.d/messages/Standard.conf");
  EXPECT_NE(updated_text.find("Description = \"Updated client messages\""),
            std::string::npos);
  EXPECT_NE(
      updated_text.find("Director = bareos-dir = all, !skipped, !restored"),
      std::string::npos);
}

TEST(BconfigService, RejectsClientMessagesUpdatesForSharedFiles)
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

  auto rejected = state.UpsertClientMessagesResource(
      "prod", "backup-bareos-test-fd", "Standard",
      {.description = std::string{"Updated client messages"}});
  ASSERT_FALSE(rejected);
  EXPECT_NE(rejected.error.find("standalone file"), std::string::npos);
  EXPECT_FALSE(std::filesystem::exists(original_path));
  EXPECT_TRUE(std::filesystem::exists(shared_path));
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

TEST(BconfigService, RejectsClientMessagesDeletesForSharedFiles)
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

  auto rejected = state.DeleteClientMessagesResource(
      "prod", "backup-bareos-test-fd", "Standard");
  ASSERT_FALSE(rejected);
  EXPECT_NE(rejected.error.find("standalone file"), std::string::npos);
  EXPECT_FALSE(std::filesystem::exists(original_path));
  EXPECT_TRUE(std::filesystem::exists(shared_path));
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
}

TEST(BconfigService, RejectsClientDaemonUpdatesForSharedFiles)
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

  auto rejected = state.UpsertClientDaemonResource(
      "prod", "backup-bareos-test-fd",
      {.description = std::string{"Managed client daemon"}});
  ASSERT_FALSE(rejected);
  EXPECT_NE(rejected.error.find("standalone file"), std::string::npos);
  EXPECT_FALSE(std::filesystem::exists(original_path));
  EXPECT_TRUE(std::filesystem::exists(shared_path));
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
       .connection_from_director_to_client = false,
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
  EXPECT_NE(created_text.find("Password = \"[md5]"), std::string::npos);
  EXPECT_NE(created_text.find("Description = \"Initial stub\""),
            std::string::npos);
  EXPECT_NE(created_text.find("ConnectionFromDirectorToClient = no"),
            std::string::npos);
  EXPECT_NE(created_text.find("Monitor = yes"), std::string::npos);
  EXPECT_NE(created_text.find("MaximumBandwidthPerJob = 2048"),
            std::string::npos);

  auto updated = state.UpsertClientDirectorStub(
      "prod", "bareos-fd", "bareos-dir",
      {.description = std::string{"Updated stub"}});
  ASSERT_TRUE(updated);

  std::ifstream updated_file{stub_path};
  ASSERT_TRUE(updated_file.good());
  const std::string updated_text((std::istreambuf_iterator<char>(updated_file)),
                                 std::istreambuf_iterator<char>());
  EXPECT_NE(updated_text.find("Password = \"[md5]"), std::string::npos);
  EXPECT_NE(updated_text.find("Description = \"Updated stub\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("ConnectionFromDirectorToClient = no"),
            std::string::npos);
  EXPECT_NE(updated_text.find("Monitor = yes"), std::string::npos);
  EXPECT_NE(updated_text.find("MaximumBandwidthPerJob = 2048"),
            std::string::npos);

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
       .password = std::string{"[md5]0123456789abcdef0123456789abcdef"},
       .description = std::string{"Managed by service"}});
  ASSERT_TRUE(created) << created.error;
  EXPECT_EQ(created.value->name, "bareos-dir");

  const auto director_client_path
      = created.value->path / "bareos-dir.d/client/client1-fd.conf";
  const auto created_text = ReadTextFile(director_client_path);
  EXPECT_NE(created_text.find("Name = \"client1-fd\""), std::string::npos);
  EXPECT_NE(created_text.find("Address = client1-fd.example.com"),
            std::string::npos);
  EXPECT_NE(
      created_text.find("Password = \"[md5]0123456789abcdef0123456789abcdef\""),
      std::string::npos);
  EXPECT_NE(created_text.find("Port = 9102"), std::string::npos);
  EXPECT_NE(created_text.find("Description = \"Managed by service\""),
            std::string::npos);

  const auto stub_path = RepositoryLayout::ClientsDirectory(repo_path.path())
                         / "client1-fd/bareos-fd.d/director/bareos-dir.conf";
  const auto stub_text = ReadTextFile(stub_path);
  EXPECT_NE(
      stub_text.find("Password = \"[md5]0123456789abcdef0123456789abcdef\""),
      std::string::npos);

  auto updated = state.UpsertDirectorClientResource(
      "prod", "bareos-dir", "client1-fd",
      {.address = std::string{"client1-alt.example.com"}});
  ASSERT_TRUE(updated) << updated.error;

  const auto updated_text = ReadTextFile(director_client_path);
  EXPECT_NE(updated_text.find("Address = client1-alt.example.com"),
            std::string::npos);
  EXPECT_NE(
      updated_text.find("Password = \"[md5]0123456789abcdef0123456789abcdef\""),
      std::string::npos);
  EXPECT_NE(updated_text.find("Port = 9102"), std::string::npos);
  EXPECT_NE(updated_text.find("Description = \"Managed by service\""),
            std::string::npos);
}

TEST(BconfigService, RejectsDirectorClientUpdatesForSharedFiles)
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

  auto rejected = state.UpsertDirectorClientResource(
      "prod", "bareos-dir", "bareos-fd",
      {.address = std::string{"bareos-fd-alt.example.com"}});
  ASSERT_FALSE(rejected);
  EXPECT_NE(rejected.error.find("standalone file"), std::string::npos);
  EXPECT_FALSE(std::filesystem::exists(original_path));
  EXPECT_TRUE(std::filesystem::exists(shared_path));
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

TEST(BconfigService, RejectsDirectorClientDeletesForSharedFiles)
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

  auto rejected
      = state.DeleteDirectorClientResource("prod", "bareos-dir", "bareos-fd");
  ASSERT_FALSE(rejected);
  EXPECT_NE(rejected.error.find("standalone file"), std::string::npos);
  EXPECT_FALSE(std::filesystem::exists(original_path));
  EXPECT_TRUE(std::filesystem::exists(shared_path));
}

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
       .use_pam_authentication = true});
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
  EXPECT_NE(created_text.find("UsePamAuthentication = yes"), std::string::npos);

  auto updated = state.UpsertDirectorConsoleResource(
      "prod", "bareos-dir", "bareos-mon",
      {.description = std::string{"Updated restricted console"}});
  ASSERT_TRUE(updated) << updated.error;

  const auto updated_text = ReadTextFile(
      updated.value->path / "bareos-dir.d/console/bareos-mon.conf");
  EXPECT_NE(updated_text.find("Description = \"Updated restricted console\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("Password = "), std::string::npos);
  EXPECT_NE(updated_text.find("CommandACL = status, .status"),
            std::string::npos);
  EXPECT_NE(updated_text.find("JobACL = *all*"), std::string::npos);
}

TEST(BconfigService, RejectsDirectorConsoleUpdatesForSharedFiles)
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

  auto rejected = state.UpsertDirectorConsoleResource(
      "prod", "bareos-dir", "bareos-mon",
      {.description = std::string{"Updated restricted console"}});
  ASSERT_FALSE(rejected);
  EXPECT_NE(rejected.error.find("standalone file"), std::string::npos);
  EXPECT_FALSE(std::filesystem::exists(original_path));
  EXPECT_TRUE(std::filesystem::exists(shared_path));
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

TEST(BconfigService, RejectsDirectorConsoleDeletesForSharedFiles)
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

  auto rejected
      = state.DeleteDirectorConsoleResource("prod", "bareos-dir", "bareos-mon");
  ASSERT_FALSE(rejected);
  EXPECT_NE(rejected.error.find("standalone file"), std::string::npos);
  EXPECT_FALSE(std::filesystem::exists(original_path));
  EXPECT_TRUE(std::filesystem::exists(shared_path));
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
      {.description = std::string{"Managed user"}});
  ASSERT_TRUE(created) << created.error;
  EXPECT_EQ(created.value->name, "bareos-dir");

  const auto user_path
      = created.value->path / "bareos-dir.d/user/managed-user.conf";
  const auto created_text = ReadTextFile(user_path);
  EXPECT_NE(created_text.find("Name = \"managed-user\""), std::string::npos);
  EXPECT_NE(created_text.find("Description = \"Managed user\""),
            std::string::npos);

  auto updated = state.UpsertDirectorUserResource(
      "prod", "bareos-dir", "operator-user",
      {.description = std::string{"Updated operator user"}});
  ASSERT_TRUE(updated) << updated.error;

  const auto updated_text = ReadTextFile(
      updated.value->path / "bareos-dir.d/user/operator-user.conf");
  EXPECT_NE(updated_text.find("Description = \"Updated operator user\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("CommandACL = status, .status"),
            std::string::npos);
  EXPECT_NE(updated_text.find("Profile = operator"), std::string::npos);
}

TEST(BconfigService, RejectsDirectorUserUpdatesForSharedFiles)
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

  auto rejected = state.UpsertDirectorUserResource(
      "prod", "bareos-dir", "operator-user",
      {.description = std::string{"Updated operator user"}});
  ASSERT_FALSE(rejected);
  EXPECT_NE(rejected.error.find("standalone file"), std::string::npos);
  EXPECT_FALSE(std::filesystem::exists(original_path));
  EXPECT_TRUE(std::filesystem::exists(shared_path));
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

TEST(BconfigService, RejectsDirectorUserDeletesForSharedFiles)
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

  auto rejected
      = state.DeleteDirectorUserResource("prod", "bareos-dir", "operator-user");
  ASSERT_FALSE(rejected);
  EXPECT_NE(rejected.error.find("standalone file"), std::string::npos);
  EXPECT_FALSE(std::filesystem::exists(original_path));
  EXPECT_TRUE(std::filesystem::exists(shared_path));
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
      {.description = std::string{"Managed profile"}});
  ASSERT_TRUE(created) << created.error;
  EXPECT_EQ(created.value->name, "bareos-dir");

  const auto profile_path
      = created.value->path / "bareos-dir.d/profile/managed-profile.conf";
  const auto created_text = ReadTextFile(profile_path);
  EXPECT_NE(created_text.find("Name = \"managed-profile\""), std::string::npos);
  EXPECT_NE(created_text.find("Description = \"Managed profile\""),
            std::string::npos);

  auto updated = state.UpsertDirectorProfileResource(
      "prod", "bareos-dir", "operator",
      {.description = std::string{"Updated operator profile"}});
  ASSERT_TRUE(updated) << updated.error;

  const auto updated_text = ReadTextFile(
      updated.value->path / "bareos-dir.d/profile/operator.conf");
  EXPECT_NE(updated_text.find("Description = \"Updated operator profile\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("CommandACL = !.bvfs_clear_cache, !.exit, !.sql"),
            std::string::npos);
  EXPECT_NE(updated_text.find("*all*"), std::string::npos);
  EXPECT_NE(updated_text.find("CatalogACL = *all*"), std::string::npos);
}

TEST(BconfigService, RejectsDirectorProfileUpdatesForSharedFiles)
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

  auto rejected = state.UpsertDirectorProfileResource(
      "prod", "bareos-dir", "operator",
      {.description = std::string{"Updated operator profile"}});
  ASSERT_FALSE(rejected);
  EXPECT_NE(rejected.error.find("standalone file"), std::string::npos);
  EXPECT_FALSE(std::filesystem::exists(original_path));
  EXPECT_TRUE(std::filesystem::exists(shared_path));
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

TEST(BconfigService, RejectsDirectorProfileDeletesForSharedFiles)
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

  auto rejected
      = state.DeleteDirectorProfileResource("prod", "bareos-dir", "operator");
  ASSERT_FALSE(rejected);
  EXPECT_NE(rejected.error.find("standalone file"), std::string::npos);
  EXPECT_FALSE(std::filesystem::exists(original_path));
  EXPECT_TRUE(std::filesystem::exists(shared_path));
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
       .maximum_volumes = 7,
       .maximum_volume_bytes = 123456,
       .volume_retention = 86400,
       .auto_prune = true,
       .recycle = false,
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
  EXPECT_NE(created_text.find("MaximumVolumes = 7"), std::string::npos);
  EXPECT_NE(created_text.find("MaximumVolumeBytes = 123456"),
            std::string::npos);
  EXPECT_NE(created_text.find("VolumeRetention = 86400"), std::string::npos);
  EXPECT_NE(created_text.find("AutoPrune = yes"), std::string::npos);
  EXPECT_NE(created_text.find("Recycle = no"), std::string::npos);

  auto updated = state.UpsertDirectorPoolResource(
      "prod", "bareos-dir", "Full",
      {.description = std::string{"Updated full pool"}});
  ASSERT_TRUE(updated) << updated.error;

  const auto updated_text
      = ReadTextFile(updated.value->path / "bareos-dir.d/pool/Full.conf");
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
}

TEST(BconfigService, RejectsDirectorPoolUpdatesForSharedFiles)
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

  auto rejected = state.UpsertDirectorPoolResource(
      "prod", "bareos-dir", "Full",
      {.description = std::string{"Updated full pool"}});
  ASSERT_FALSE(rejected);
  EXPECT_NE(rejected.error.find("standalone file"), std::string::npos);
  EXPECT_FALSE(std::filesystem::exists(original_path));
  EXPECT_TRUE(std::filesystem::exists(shared_path));
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

TEST(BconfigService, RejectsDirectorPoolDeletesForSharedFiles)
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

  auto rejected
      = state.DeleteDirectorPoolResource("prod", "bareos-dir", "Full");
  ASSERT_FALSE(rejected);
  EXPECT_NE(rejected.error.find("standalone file"), std::string::npos);
  EXPECT_FALSE(std::filesystem::exists(original_path));
  EXPECT_TRUE(std::filesystem::exists(shared_path));
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
  EXPECT_NE(created_text.find("Reconnect = yes"), std::string::npos);
  EXPECT_NE(created_text.find("ExitOnFatal = no"), std::string::npos);

  auto updated = state.UpsertDirectorCatalogResource(
      "prod", "bareos-dir", "MyCatalog",
      {.description = std::string{"Updated catalog"}});
  ASSERT_TRUE(updated) << updated.error;

  const auto updated_text = ReadTextFile(
      updated.value->path / "bareos-dir.d/catalog/MyCatalog.conf");
  EXPECT_NE(updated_text.find("Description = \"Updated catalog\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("DbUser = \"regress\""), std::string::npos);
  EXPECT_NE(updated_text.find("DbName = \"regress_backup_bareos_test\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("DbPassword = \"\""), std::string::npos);
}

TEST(BconfigService, RejectsDirectorCatalogUpdatesForSharedFiles)
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

  auto rejected = state.UpsertDirectorCatalogResource(
      "prod", "bareos-dir", "MyCatalog",
      {.description = std::string{"Updated catalog"}});
  ASSERT_FALSE(rejected);
  EXPECT_NE(rejected.error.find("standalone file"), std::string::npos);
  EXPECT_FALSE(std::filesystem::exists(original_path));
  EXPECT_TRUE(std::filesystem::exists(shared_path));
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

TEST(BconfigService, RejectsDirectorCatalogDeletesForSharedFiles)
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

  auto rejected
      = state.DeleteDirectorCatalogResource("prod", "bareos-dir", "MyCatalog");
  ASSERT_FALSE(rejected);
  EXPECT_NE(rejected.error.find("standalone file"), std::string::npos);
  EXPECT_FALSE(std::filesystem::exists(original_path));
  EXPECT_TRUE(std::filesystem::exists(shared_path));
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

TEST(BconfigService, RejectsDirectorScheduleUpdatesForSharedFiles)
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

  auto rejected = state.UpsertDirectorScheduleResource(
      "prod", "bareos-dir", "Odd Weeks",
      {.description = std::string{"Updated odd weeks"}});
  ASSERT_FALSE(rejected);
  EXPECT_NE(rejected.error.find("standalone file"), std::string::npos);
  EXPECT_FALSE(std::filesystem::exists(original_path));
  EXPECT_TRUE(std::filesystem::exists(shared_path));
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

TEST(BconfigService, RejectsDirectorScheduleDeletesForSharedFiles)
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

  auto rejected
      = state.DeleteDirectorScheduleResource("prod", "bareos-dir", "Odd Weeks");
  ASSERT_FALSE(rejected);
  EXPECT_NE(rejected.error.find("standalone file"), std::string::npos);
  EXPECT_FALSE(std::filesystem::exists(original_path));
  EXPECT_TRUE(std::filesystem::exists(shared_path));
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

TEST(BconfigService, RejectsDirectorCounterUpdatesForSharedFiles)
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

  auto rejected = state.UpsertDirectorCounterResource(
      "prod", "bareos-dir", "ExistingCounter",
      {.description = std::string{"Updated counter"}});
  ASSERT_FALSE(rejected);
  EXPECT_NE(rejected.error.find("standalone file"), std::string::npos);
  EXPECT_FALSE(std::filesystem::exists(original_path));
  EXPECT_TRUE(std::filesystem::exists(shared_path));
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

TEST(BconfigService, RejectsDirectorCounterDeletesForSharedFiles)
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

  auto rejected = state.DeleteDirectorCounterResource("prod", "bareos-dir",
                                                      "ExistingCounter");
  ASSERT_FALSE(rejected);
  EXPECT_NE(rejected.error.find("standalone file"), std::string::npos);
  EXPECT_FALSE(std::filesystem::exists(original_path));
  EXPECT_TRUE(std::filesystem::exists(shared_path));
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

TEST(BconfigService, RejectsDirectorFilesetUpdatesForSharedFiles)
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

  auto rejected = state.UpsertDirectorFilesetResource(
      "prod", "bareos-dir", "LinuxAll",
      {.description = std::string{"Updated LinuxAll fileset"}});
  ASSERT_FALSE(rejected);
  EXPECT_NE(rejected.error.find("standalone file"), std::string::npos);
  EXPECT_FALSE(std::filesystem::exists(original_path));
  EXPECT_TRUE(std::filesystem::exists(shared_path));
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

TEST(BconfigService, RejectsDirectorFilesetDeletesForSharedFiles)
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

  auto rejected
      = state.DeleteDirectorFilesetResource("prod", "bareos-dir", "LinuxAll");
  ASSERT_FALSE(rejected);
  EXPECT_NE(rejected.error.find("standalone file"), std::string::npos);
  EXPECT_FALSE(std::filesystem::exists(original_path));
  EXPECT_TRUE(std::filesystem::exists(shared_path));
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
       .client = std::string{"bareos-fd"},
       .jobdefs = std::string{"DefaultJob"},
       .enabled = true});
  ASSERT_TRUE(created) << created.error;
  EXPECT_EQ(created.value->name, "bareos-dir");

  const auto job_path
      = created.value->path / "bareos-dir.d/job/ManagedJob.conf";
  const auto created_text = ReadTextFile(job_path);
  EXPECT_NE(created_text.find("Name = \"ManagedJob\""), std::string::npos);
  EXPECT_NE(created_text.find("Description = \"Managed job\""),
            std::string::npos);
  EXPECT_NE(created_text.find("JobDefs = DefaultJob"), std::string::npos);
  EXPECT_NE(created_text.find("Client = bareos-fd"), std::string::npos);
  EXPECT_NE(created_text.find("Enabled = yes"), std::string::npos);

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
  EXPECT_NE(updated_text.find("Write Bootstrap = "), std::string::npos);
}

TEST(BconfigService, RejectsDirectorJobUpdatesForSharedFiles)
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

  auto rejected = state.UpsertDirectorJobResource(
      "prod", "bareos-dir", "BackupCatalog",
      {.description = std::string{"Updated backup catalog job"}});
  ASSERT_FALSE(rejected);
  EXPECT_NE(rejected.error.find("standalone file"), std::string::npos);
  EXPECT_FALSE(std::filesystem::exists(original_path));
  EXPECT_TRUE(std::filesystem::exists(shared_path));
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

TEST(BconfigService, RejectsDirectorJobDeletesForSharedFiles)
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

  auto rejected
      = state.DeleteDirectorJobResource("prod", "bareos-dir", "BackupCatalog");
  ASSERT_FALSE(rejected);
  EXPECT_NE(rejected.error.find("standalone file"), std::string::npos);
  EXPECT_FALSE(std::filesystem::exists(original_path));
  EXPECT_TRUE(std::filesystem::exists(shared_path));
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
       .messages = std::string{"Standard"},
       .pool = std::string{"Incremental"},
       .client = std::string{"bareos-fd"},
       .fileset = std::string{"SelfTest"},
       .schedule = std::string{"WeeklyCycle"},
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
  EXPECT_NE(created_text.find("Client = bareos-fd"), std::string::npos);
  EXPECT_NE(created_text.find("Enabled = yes"), std::string::npos);

  auto updated = state.UpsertDirectorJobDefsResource(
      "prod", "bareos-dir", "DefaultJob",
      {.description = std::string{"Updated default jobdefs"}});
  ASSERT_TRUE(updated) << updated.error;

  const auto updated_text = ReadTextFile(
      updated.value->path / "bareos-dir.d/jobdefs/DefaultJob.conf");
  EXPECT_NE(updated_text.find("Description = \"Updated default jobdefs\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("Write Bootstrap = "), std::string::npos);
  EXPECT_NE(updated_text.find("FullBackupPool = Full"), std::string::npos);
  EXPECT_NE(updated_text.find("DifferentialBackupPool = Differential"),
            std::string::npos);
  EXPECT_NE(updated_text.find("IncrementalBackupPool = Incremental"),
            std::string::npos);
}

TEST(BconfigService, RejectsDirectorJobDefsUpdatesForSharedFiles)
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

  auto rejected = state.UpsertDirectorJobDefsResource(
      "prod", "bareos-dir", "DefaultJob",
      {.description = std::string{"Updated default jobdefs"}});
  ASSERT_FALSE(rejected);
  EXPECT_NE(rejected.error.find("standalone file"), std::string::npos);
  EXPECT_FALSE(std::filesystem::exists(original_path));
  EXPECT_TRUE(std::filesystem::exists(shared_path));
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

TEST(BconfigService, RejectsDirectorJobDefsDeletesForSharedFiles)
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

  auto rejected
      = state.DeleteDirectorJobDefsResource("prod", "bareos-dir", "DefaultJob");
  ASSERT_FALSE(rejected);
  EXPECT_NE(rejected.error.find("standalone file"), std::string::npos);
  EXPECT_FALSE(std::filesystem::exists(original_path));
  EXPECT_TRUE(std::filesystem::exists(shared_path));
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

TEST(BconfigService, RejectsDirectorMessagesUpdatesForSharedFiles)
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

  auto rejected = state.UpsertDirectorMessagesResource(
      "prod", "bareos-dir", "Standard",
      {.description = std::string{"Updated standard messages"}});
  ASSERT_FALSE(rejected);
  EXPECT_NE(rejected.error.find("standalone file"), std::string::npos);
  EXPECT_FALSE(std::filesystem::exists(original_path));
  EXPECT_TRUE(std::filesystem::exists(shared_path));
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

TEST(BconfigService, RejectsDirectorMessagesDeletesForSharedFiles)
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

  auto rejected
      = state.DeleteDirectorMessagesResource("prod", "bareos-dir", "Standard");
  ASSERT_FALSE(rejected);
  EXPECT_NE(rejected.error.find("standalone file"), std::string::npos);
  EXPECT_FALSE(std::filesystem::exists(original_path));
  EXPECT_TRUE(std::filesystem::exists(shared_path));
}

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
       .entries = std::vector<std::string>{"  Director = bareos-dir = all"}});
  ASSERT_TRUE(created) << created.error;
  EXPECT_EQ(created.value->name, "bareos-sd");

  const auto messages_path
      = created.value->path / "bareos-sd.d/messages/ManagedMessages.conf";
  const auto created_text = ReadTextFile(messages_path);
  EXPECT_NE(created_text.find("Messages {"), std::string::npos);
  EXPECT_NE(created_text.find("Name = \"ManagedMessages\""), std::string::npos);
  EXPECT_NE(created_text.find("Description = \"Managed storage messages\""),
            std::string::npos);
  EXPECT_NE(created_text.find("Director = bareos-dir = all"),
            std::string::npos);

  auto updated = state.UpsertStorageMessagesResource(
      "prod", "bareos-sd", "Standard",
      {.description = std::string{"Updated storage messages"}});
  ASSERT_TRUE(updated) << updated.error;

  const auto updated_text = ReadTextFile(
      updated.value->path / "bareos-sd.d/messages/Standard.conf");
  EXPECT_NE(updated_text.find("Description = \"Updated storage messages\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("Director = bareos-dir = all"),
            std::string::npos);
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
}

TEST(BconfigService, RejectsStorageDaemonUpdatesForSharedFiles)
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

  auto rejected = state.UpsertStorageDaemonResource(
      "prod", "bareos-sd",
      {.description = std::string{"Managed storage daemon"}});
  ASSERT_FALSE(rejected);
  EXPECT_NE(rejected.error.find("standalone file"), std::string::npos);
  EXPECT_FALSE(std::filesystem::exists(original_path));
  EXPECT_TRUE(std::filesystem::exists(shared_path));
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
       .maximum_bandwidth_per_job = 2048});
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

  const auto ownership_path = RepositoryLayout::OwnershipPath(repo_path.path());
  const auto ownership_text = ReadTextFile(ownership_path);
  EXPECT_NE(ownership_text.find(
                "storages/bareos-sd/bareos-sd.d/director/ManagedDirector.conf"),
            std::string::npos);

  auto updated = state.UpsertStorageDirectorResource(
      "prod", "bareos-sd", "bareos-dir",
      {.password = std::string{"[md5]abcdef0123456789abcdef0123456789"},
       .description = std::string{"Updated storage director"},
       .monitor = false,
       .maximum_bandwidth_per_job = 4096});
  ASSERT_TRUE(updated) << updated.error;

  const auto updated_text = ReadTextFile(
      updated.value->path / "bareos-sd.d/director/bareos-dir.conf");
  EXPECT_NE(updated_text.find("Description = \"Updated storage director\""),
            std::string::npos);
  EXPECT_NE(
      updated_text.find("Password = \"[md5]abcdef0123456789abcdef0123456789\""),
      std::string::npos);
  EXPECT_NE(updated_text.find("Monitor = no"), std::string::npos);
  EXPECT_NE(updated_text.find("MaximumBandwidthPerJob = 4096"),
            std::string::npos);

  const auto updated_ownership_text = ReadTextFile(ownership_path);
  EXPECT_NE(updated_ownership_text.find(
                "storages/bareos-sd/bareos-sd.d/director/ManagedDirector.conf"),
            std::string::npos);
  EXPECT_NE(updated_ownership_text.find(
                "storages/bareos-sd/bareos-sd.d/director/bareos-dir.conf"),
            std::string::npos);
}

TEST(BconfigService, RejectsStorageDirectorUpdatesForSharedFiles)
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

  auto rejected = state.UpsertStorageDirectorResource(
      "prod", "bareos-sd", "bareos-dir",
      {.password = std::string{"[md5]abcdef0123456789abcdef0123456789"}});
  ASSERT_FALSE(rejected);
  EXPECT_NE(rejected.error.find("standalone file"), std::string::npos);
  EXPECT_FALSE(std::filesystem::exists(original_path));
  EXPECT_TRUE(std::filesystem::exists(shared_path));
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

TEST(BconfigService, RejectsStorageDirectorDeletesForSharedFiles)
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

  auto rejected
      = state.DeleteStorageDirectorResource("prod", "bareos-sd", "bareos-dir");
  ASSERT_FALSE(rejected);
  EXPECT_NE(rejected.error.find("standalone file"), std::string::npos);
  EXPECT_FALSE(std::filesystem::exists(original_path));
  EXPECT_TRUE(std::filesystem::exists(shared_path));
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

  const auto ownership_path = RepositoryLayout::OwnershipPath(repo_path.path());
  const auto ownership_text = ReadTextFile(ownership_path);
  EXPECT_NE(ownership_text.find(
                "storages/bareos-sd/bareos-sd.d/device/ManagedDevice.conf"),
            std::string::npos);

  auto updated = state.UpsertStorageDeviceResource(
      "prod", "bareos-sd", "FileStorage",
      {.archive_device = std::string{"/tmp/updated-storage"},
       .description = std::string{"Updated storage device"}});
  ASSERT_TRUE(updated) << updated.error;

  const auto updated_text = ReadTextFile(
      updated.value->path / "bareos-sd.d/device/FileStorage.conf");
  EXPECT_NE(updated_text.find("Description = \"Updated storage device\""),
            std::string::npos);
  EXPECT_NE(updated_text.find("Media Type = File"), std::string::npos);
  EXPECT_NE(updated_text.find("Archive Device = /tmp/updated-storage"),
            std::string::npos);
  EXPECT_NE(updated_text.find("Device Type = \"File\""), std::string::npos);

  const auto updated_ownership_text = ReadTextFile(ownership_path);
  EXPECT_NE(updated_ownership_text.find(
                "storages/bareos-sd/bareos-sd.d/device/FileStorage.conf"),
            std::string::npos);
}

TEST(BconfigService, RejectsStorageDeviceUpdatesForSharedFiles)
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

  auto rejected = state.UpsertStorageDeviceResource(
      "prod", "bareos-sd", "FileStorage",
      {.archive_device = std::string{"/tmp/updated-storage"}});
  ASSERT_FALSE(rejected);
  EXPECT_NE(rejected.error.find("standalone file"), std::string::npos);
  EXPECT_FALSE(std::filesystem::exists(original_path));
  EXPECT_TRUE(std::filesystem::exists(shared_path));
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

TEST(BconfigService, RejectsStorageDeviceDeletesForSharedFiles)
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

  auto rejected
      = state.DeleteStorageDeviceResource("prod", "bareos-sd", "FileStorage");
  ASSERT_FALSE(rejected);
  EXPECT_NE(rejected.error.find("standalone file"), std::string::npos);
  EXPECT_FALSE(std::filesystem::exists(original_path));
  EXPECT_TRUE(std::filesystem::exists(shared_path));
}

TEST(BconfigService, RejectsStorageMessagesUpdatesForSharedFiles)
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

  auto rejected = state.UpsertStorageMessagesResource(
      "prod", "bareos-sd", "Standard",
      {.description = std::string{"Updated storage messages"}});
  ASSERT_FALSE(rejected);
  EXPECT_NE(rejected.error.find("standalone file"), std::string::npos);
  EXPECT_FALSE(std::filesystem::exists(original_path));
  EXPECT_TRUE(std::filesystem::exists(shared_path));
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

TEST(BconfigService, RejectsStorageMessagesDeletesForSharedFiles)
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

  auto rejected
      = state.DeleteStorageMessagesResource("prod", "bareos-sd", "Standard");
  ASSERT_FALSE(rejected);
  EXPECT_NE(rejected.error.find("standalone file"), std::string::npos);
  EXPECT_FALSE(std::filesystem::exists(original_path));
  EXPECT_TRUE(std::filesystem::exists(shared_path));
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
       .password = std::string{"[md5]abcdef0123456789abcdef0123456789"},
       .device = std::string{"FileStorage"},
       .media_type = std::string{"File"},
       .description = std::string{"Managed storage"}});
  ASSERT_TRUE(created) << created.error;
  EXPECT_EQ(created.value->name, "bareos-dir");

  const auto storage_path
      = created.value->path / "bareos-dir.d/storage/FileManaged.conf";
  const auto created_text = ReadTextFile(storage_path);
  EXPECT_NE(created_text.find("Name = \"FileManaged\""), std::string::npos);
  EXPECT_NE(created_text.find("Address = localhost"), std::string::npos);
  EXPECT_NE(
      created_text.find("Password = \"[md5]abcdef0123456789abcdef0123456789\""),
      std::string::npos);
  EXPECT_NE(created_text.find("Device = FileStorage"), std::string::npos);
  EXPECT_NE(created_text.find("Media Type = File"), std::string::npos);
  EXPECT_NE(created_text.find("Port = 9103"), std::string::npos);
  EXPECT_NE(created_text.find("Description = \"Managed storage\""),
            std::string::npos);

  auto updated = state.UpsertDirectorStorageResource(
      "prod", "bareos-dir", "FileManaged",
      {.address = std::string{"storage.example.com"}});
  ASSERT_TRUE(updated) << updated.error;

  const auto updated_text = ReadTextFile(storage_path);
  EXPECT_NE(updated_text.find("Address = storage.example.com"),
            std::string::npos);
  EXPECT_NE(
      updated_text.find("Password = \"[md5]abcdef0123456789abcdef0123456789\""),
      std::string::npos);
  EXPECT_NE(updated_text.find("Device = FileStorage"), std::string::npos);
  EXPECT_NE(updated_text.find("Media Type = File"), std::string::npos);
  EXPECT_NE(updated_text.find("Port = 9103"), std::string::npos);
  EXPECT_NE(updated_text.find("Description = \"Managed storage\""),
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
       .password = std::string{"sd_password"},
       .device = std::string{"FileStorage"},
       .media_type = std::string{"File"}});
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

TEST(BconfigService, RejectsDirectorStorageUpdatesForSharedFiles)
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

  auto rejected = state.UpsertDirectorStorageResource(
      "prod", "bareos-dir", "File",
      {.address = std::string{"storage-alt.example.com"}});
  ASSERT_FALSE(rejected);
  EXPECT_NE(rejected.error.find("standalone file"), std::string::npos);
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

TEST(BconfigService, RejectsDirectorStorageDeletesForSharedFiles)
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

  auto rejected
      = state.DeleteDirectorStorageResource("prod", "bareos-dir", "File");
  ASSERT_FALSE(rejected);
  EXPECT_NE(rejected.error.find("standalone file"), std::string::npos);
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

  const auto director_text = ReadTextFile(director_sync_path);
  EXPECT_NE(director_text.find("Name = \"bareos-dir\""), std::string::npos);
  EXPECT_NE(director_text.find(
                "Password = \"[md5]abcdef0123456789abcdef0123456789\""),
            std::string::npos);

  const auto device_text = ReadTextFile(device_sync_path);
  EXPECT_NE(device_text.find("Name = \"FileManagedDevice\""),
            std::string::npos);
  EXPECT_NE(device_text.find("Media Type = File"), std::string::npos);
  EXPECT_NE(device_text.find("Archive Device = /tmp/bareos-storage"),
            std::string::npos);
  EXPECT_NE(device_text.find("Device Type = \"file\""), std::string::npos);

  const auto ownership_text
      = ReadTextFile(RepositoryLayout::OwnershipPath(repo_path.path()));
  EXPECT_NE(ownership_text.find("storages/bareos-sd/bareos-sd.d/director/"
                                "bareos-dir.conf"),
            std::string::npos);
  EXPECT_NE(ownership_text.find("storages/bareos-sd/bareos-sd.d/device/"
                                "FileManagedDevice.conf"),
            std::string::npos);
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
