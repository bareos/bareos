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

std::filesystem::path FindFixtureRoot()
{
  auto cursor = std::filesystem::current_path();
  for (;;) {
    const auto build_tree_path = cursor / "configs/bareos-configparser-tests";
    if (std::filesystem::is_directory(build_tree_path)) { return build_tree_path; }

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

std::optional<JobRecord> WaitForJobTerminal(ServiceState& state,
                                            std::string_view id)
{
  for (int attempt = 0; attempt < 50; ++attempt) {
    auto job = state.GetJob(id);
    if (job && (job->status == JobStatus::kSucceeded
                || job->status == JobStatus::kFailed)) {
      return job;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }

  return state.GetJob(id);
}

TEST(BconfigService, CreatesDeploymentScaffold)
{
  ScopedDirectory repo_path{MakeTempPath()};
  ServiceState state;

  auto result = state.CreateDeployment(
      {.id = "prod",
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

  std::ifstream manifest{
      RepositoryLayout::ManifestPath(repo_path.path())};
  ASSERT_TRUE(manifest.good());
  const std::string manifest_text((std::istreambuf_iterator<char>(manifest)),
                                  std::istreambuf_iterator<char>());
  EXPECT_NE(manifest_text.find("\"id\": \"prod\""), std::string::npos);
  EXPECT_NE(manifest_text.find("\"workflow_mode\": \"review\""),
            std::string::npos);
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

TEST(BconfigService, RunsValidationJobsForKnownDeployments)
{
  ScopedDirectory repo_path{MakeTempPath()};
  ServiceState state;

  auto deployment = state.CreateDeployment(
      {.id = "prod", .name = "Production", .repository_path = repo_path.path()});
  ASSERT_TRUE(deployment);

  auto job = state.CreateJob(
      {.type = "validate_deployment_repo",
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

  auto deployment = state.CreateDeployment(
      {.id = "prod", .name = "Production", .repository_path = repo_path.path()});
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

  auto import_job = state.CreateJob({.type = "import_configuration",
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
}

TEST(BconfigService, PersistsDeploymentsAndJobsAcrossRestart)
{
  ScopedDirectory state_path{MakeTempPath()};
  ScopedDirectory repo_path{MakeTempPath()};

  {
    ServiceState state{state_path.path()};
    auto deployment = state.CreateDeployment(
        {.id = "prod",
         .name = "Production",
         .repository_path = repo_path.path(),
         .workflow_mode = WorkflowMode::kReview});
    ASSERT_TRUE(deployment);

    auto job = state.CreateJob(
        {.type = "validate_deployment_repo",
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

    auto next_job = reloaded.CreateJob(
        {.type = "validate_deployment_repo",
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

  auto deployment = state.CreateDeployment(
      {.id = "prod", .name = "Production", .repository_path = repo_path.path()});
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
  EXPECT_TRUE(std::filesystem::exists(imported_root / "bareos-dir.d/director/"
                                      "bareos-dir.conf"));
  const auto imported_client_root
      = RepositoryLayout::ClientsDirectory(repo_path.path())
        / "backup-bareos-test-fd";
  EXPECT_TRUE(std::filesystem::is_directory(imported_client_root / "bareos-fd.d"));
  EXPECT_TRUE(std::filesystem::exists(imported_client_root / "bareos-fd.d/client/"
                                      "myself.conf"));

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
  EXPECT_NE(import_state_text.find("\"resource_name\": \"backup-bareos-test-fd\""),
            std::string::npos);

  auto imports = state.ListDeploymentImports("prod");
  ASSERT_TRUE(imports);
  ASSERT_EQ(imports.value->size(), 2u);
  EXPECT_EQ(imports.value->at(0).job_id, job.value->id);
  EXPECT_EQ(imports.value->at(0).component, "director");
  EXPECT_EQ(imports.value->at(0).resource_name, "bareos-dir");
  EXPECT_EQ(imports.value->at(0).source_path,
            std::optional<std::string>{source_root.path().string()});
  EXPECT_EQ(imports.value->at(0).destination_path,
            "directors/bareos-dir");
  EXPECT_EQ(imports.value->at(1).component, "client");
  EXPECT_EQ(imports.value->at(1).resource_name, "backup-bareos-test-fd");
}

}  // namespace
}  // namespace bconfig::service
