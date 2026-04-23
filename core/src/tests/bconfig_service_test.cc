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
      {.description = std::string{"Initial stub"}});
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
