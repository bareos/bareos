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
#include "tools/bconfig_lib.h"

#include "include/bareos.h"

#include <jansson.h>

#include <algorithm>
#include <chrono>
#include <cctype>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <system_error>

namespace bconfig::service {
namespace {

using JsonPtr = std::unique_ptr<json_t, decltype(&json_decref)>;

JsonPtr MakeJson(json_t* value) { return JsonPtr(value, &json_decref); }

bool WriteFile(const std::filesystem::path& path, const std::string& content)
{
  std::ofstream stream(path, std::ios::out | std::ios::trunc);
  if (!stream) { return false; }
  stream << content;
  return static_cast<bool>(stream);
}

template <typename T>
std::vector<T> SortedValues(const std::unordered_map<std::string, T>& values)
{
  std::vector<T> result;
  result.reserve(values.size());
  for (const auto& [_, value] : values) {
    result.emplace_back(value);
  }

  std::sort(result.begin(), result.end(),
            [](const auto& lhs, const auto& rhs) { return lhs.id < rhs.id; });
  return result;
}

struct ImportedComponent {
  bconfig::Component component;
  std::string resource_name{};
  std::filesystem::path destination_root{};
};

struct DeploymentConfigRoot {
  bconfig::Component component;
  std::string name{};
  std::filesystem::path path{};
};

std::filesystem::path ComponentConfigDirectoryName(bconfig::Component component)
{
  switch (component) {
    case bconfig::Component::kDirector:
      return "bareos-dir.d";
    case bconfig::Component::kStorage:
      return "bareos-sd.d";
    case bconfig::Component::kClient:
      return "bareos-fd.d";
#ifdef BCONFIG_HAVE_CONSOLE
    case bconfig::Component::kConsole:
      return {};
#endif
#ifdef BCONFIG_HAVE_TRAYMONITOR
    case bconfig::Component::kTrayMonitor:
      return {};
#endif
  }

  return {};
}

const char* PrimaryResourceType(bconfig::Component component)
{
  switch (component) {
    case bconfig::Component::kDirector:
      return "Director";
    case bconfig::Component::kStorage:
      return "Storage";
    case bconfig::Component::kClient:
      return "Client";
#ifdef BCONFIG_HAVE_CONSOLE
    case bconfig::Component::kConsole:
      return "Console";
#endif
#ifdef BCONFIG_HAVE_TRAYMONITOR
    case bconfig::Component::kTrayMonitor:
      return "TrayMonitor";
#endif
  }

  return "unknown";
}

std::filesystem::path ComponentBucketDirectory(
    const std::filesystem::path& repository_root, bconfig::Component component)
{
  switch (component) {
    case bconfig::Component::kDirector:
      return RepositoryLayout::DirectorsDirectory(repository_root);
    case bconfig::Component::kStorage:
      return RepositoryLayout::StoragesDirectory(repository_root);
    case bconfig::Component::kClient:
      return RepositoryLayout::ClientsDirectory(repository_root);
#ifdef BCONFIG_HAVE_CONSOLE
    case bconfig::Component::kConsole:
      return RepositoryLayout::ConsolesDirectory(repository_root);
#endif
#ifdef BCONFIG_HAVE_TRAYMONITOR
    case bconfig::Component::kTrayMonitor:
      return {};
#endif
  }

  return {};
}

void AppendParseMessages(std::vector<std::string>& logs,
                         const bconfig::ParseMessages& messages,
                         std::string_view prefix)
{
  for (const auto& warning : messages.warnings) {
    logs.emplace_back(std::string{prefix} + "warning: " + warning);
  }
  for (const auto& error : messages.errors) {
    logs.emplace_back(std::string{prefix} + "error: " + error);
  }
}

std::vector<bconfig::Component> SupportedImportComponents()
{
  return {bconfig::Component::kDirector,
          bconfig::Component::kStorage,
          bconfig::Component::kClient};
}

std::optional<std::filesystem::path> ResolveImportRoot(
    const std::filesystem::path& source_path, std::string& error)
{
  std::error_code error_code;
  const auto normalized
      = std::filesystem::weakly_canonical(source_path, error_code);
  if (error_code || !std::filesystem::exists(normalized)) {
    error = "source path does not exist: " + source_path.string();
    return std::nullopt;
  }

  if (!std::filesystem::is_directory(normalized)) {
    error = "source path must be a directory.";
    return std::nullopt;
  }

  for (const auto component : SupportedImportComponents()) {
    const auto config_directory_name = ComponentConfigDirectoryName(component);
    if (!config_directory_name.empty()
        && normalized.filename() == config_directory_name) {
      return normalized.parent_path();
    }
  }

  return normalized;
}

std::optional<std::filesystem::path> FindComponentConfigDirectory(
    bconfig::Component component, const std::filesystem::path& source_root)
{
  const auto config_directory_name = ComponentConfigDirectoryName(component);
  if (config_directory_name.empty()) { return std::nullopt; }

  const auto config_directory = source_root / config_directory_name;
  if (std::filesystem::is_directory(config_directory)) {
    return config_directory;
  }

  return std::nullopt;
}

std::vector<DeploymentConfigRoot> DiscoverDeploymentConfigRoots(
    const std::filesystem::path& repository_root)
{
  std::vector<DeploymentConfigRoot> configs;

  for (const auto component : SupportedImportComponents()) {
    const auto bucket = ComponentBucketDirectory(repository_root, component);
    const auto config_directory_name = ComponentConfigDirectoryName(component);
    if (bucket.empty() || config_directory_name.empty()
        || !std::filesystem::is_directory(bucket)) {
      continue;
    }

    for (const auto& entry : std::filesystem::directory_iterator(bucket)) {
      if (!entry.is_directory()) { continue; }
      if (!std::filesystem::is_directory(entry.path() / config_directory_name)) {
        continue;
      }

      configs.push_back(DeploymentConfigRoot{
          .component = component,
          .name = entry.path().filename().string(),
          .path = entry.path(),
      });
    }
  }

  std::sort(configs.begin(), configs.end(),
            [](const auto& lhs, const auto& rhs) {
              if (lhs.component != rhs.component) {
                return std::string{bconfig::ComponentToString(lhs.component)}
                       < std::string{bconfig::ComponentToString(rhs.component)};
              }
              return lhs.name < rhs.name;
            });
  return configs;
}

std::optional<std::string> DetectPrimaryResourceName(
    bconfig::Component component, bconfig::LoadedConfig& loaded)
{
  const auto primary_type = std::string{PrimaryResourceType(component)};
  for (const auto& resource : bconfig::CollectResources(*loaded.parser)) {
    if (resource.type == primary_type && !resource.name.empty()) {
      return resource.name;
    }
  }

  return std::nullopt;
}

std::optional<std::string> CopyDirectoryTree(const std::filesystem::path& source,
                                             const std::filesystem::path& target)
{
  std::error_code error_code;
  std::filesystem::create_directories(target.parent_path(), error_code);
  if (error_code) {
    return "failed to create directory '" + target.parent_path().string()
           + "': " + error_code.message();
  }

  std::filesystem::copy(source, target, std::filesystem::copy_options::recursive,
                        error_code);
  if (error_code) {
    return "failed to copy '" + source.string() + "' to '" + target.string()
           + "': " + error_code.message();
  }

  return std::nullopt;
}

void RemoveTreeQuietly(const std::filesystem::path& path)
{
  std::error_code error_code;
  std::filesystem::remove_all(path, error_code);
}

std::optional<std::string> UpdateImportState(
    const std::filesystem::path& repository_root,
    const JobRecord& job,
    const std::vector<ImportedComponent>& imported_items)
{
  const auto import_state_path
      = RepositoryLayout::ImportStatePath(repository_root);
  json_error_t json_error{};
  JsonPtr root = MakeJson(json_object());
  if (std::filesystem::exists(import_state_path)) {
    root = MakeJson(json_load_file(import_state_path.c_str(), 0, &json_error));
    if (!root) {
      return "failed to parse import state '" + import_state_path.string()
             + "': " + json_error.text;
    }
    if (!json_is_object(root.get())) {
      return "import state file '" + import_state_path.string()
             + "' must contain a JSON object.";
    }
  }

  auto* imports_json = json_object_get(root.get(), "imports");
  if (!imports_json) {
    imports_json = json_array();
    json_object_set_new(root.get(), "imports", imports_json);
  } else if (!json_is_array(imports_json)) {
    return "import state file '" + import_state_path.string()
           + "' contains a non-array 'imports' field.";
  }

  for (const auto& imported : imported_items) {
    auto import_entry = MakeJson(json_object());
    json_object_set_new(import_entry.get(), "job_id",
                        json_string(job.id.c_str()));
    json_object_set_new(
        import_entry.get(), "component",
        json_string(bconfig::ComponentToString(imported.component)));
    json_object_set_new(import_entry.get(), "resource_name",
                        json_string(imported.resource_name.c_str()));
    if (job.source_path) {
      json_object_set_new(import_entry.get(), "source_path",
                          json_string(job.source_path->c_str()));
    } else {
      json_object_set_new(import_entry.get(), "source_path", json_null());
    }

    std::error_code error_code;
    auto relative_destination = std::filesystem::relative(
        imported.destination_root, repository_root, error_code);
    const auto destination_text
        = error_code ? imported.destination_root.generic_string()
                     : relative_destination.generic_string();
    json_object_set_new(import_entry.get(), "destination_path",
                        json_string(destination_text.c_str()));
    json_object_set_new(import_entry.get(), "imported_at",
                        json_string(job.updated_at.c_str()));
    json_array_append_new(imports_json, import_entry.release());
  }

  char* dump = json_dumps(root.get(), JSON_INDENT(2));
  if (!dump) {
    return "failed to serialize import state '" + import_state_path.string()
           + "'.";
  }

  std::string content{dump};
  std::free(dump);
  content.push_back('\n');
  if (!WriteFile(import_state_path, content)) {
    return "failed to write import state file '" + import_state_path.string()
           + "'.";
  }

  return std::nullopt;
}

OperationResult<std::vector<DeploymentImportRecord>> LoadDeploymentImports(
    const std::filesystem::path& repository_root)
{
  const auto import_state_path = RepositoryLayout::ImportStatePath(repository_root);
  if (!std::filesystem::exists(import_state_path)) {
    return {.value = std::vector<DeploymentImportRecord>{}};
  }

  json_error_t json_error{};
  auto root = MakeJson(json_load_file(import_state_path.c_str(), 0, &json_error));
  if (!root) {
    return {.error = "failed to parse import state '" + import_state_path.string()
                     + "': " + json_error.text};
  }
  if (!json_is_object(root.get())) {
    return {.error = "import state file '" + import_state_path.string()
                     + "' must contain a JSON object."};
  }

  auto* imports_json = json_object_get(root.get(), "imports");
  if (!imports_json) {
    return {.value = std::vector<DeploymentImportRecord>{}};
  }
  if (!json_is_array(imports_json)) {
    return {.error = "import state file '" + import_state_path.string()
                     + "' contains a non-array 'imports' field."};
  }

  std::vector<DeploymentImportRecord> imports;
  imports.reserve(json_array_size(imports_json));

  size_t index = 0;
  json_t* entry = nullptr;
  json_array_foreach(imports_json, index, entry)
  {
    if (!json_is_object(entry)) {
      return {.error = "import state file '" + import_state_path.string()
                       + "' contains a non-object import entry."};
    }

    auto* job_id = json_object_get(entry, "job_id");
    auto* component = json_object_get(entry, "component");
    auto* resource_name = json_object_get(entry, "resource_name");
    auto* source_path = json_object_get(entry, "source_path");
    auto* destination_path = json_object_get(entry, "destination_path");
    auto* imported_at = json_object_get(entry, "imported_at");

    if (!json_is_string(job_id) || !json_is_string(component)
        || !json_is_string(resource_name) || !json_is_string(destination_path)
        || !json_is_string(imported_at)) {
      return {.error = "import state file '" + import_state_path.string()
                       + "' contains an import entry with invalid fields."};
    }
    if (source_path && !json_is_null(source_path) && !json_is_string(source_path)) {
      return {.error = "import state file '" + import_state_path.string()
                       + "' contains an import entry with an invalid source_path."};
    }

    DeploymentImportRecord record{
        .job_id = json_string_value(job_id),
        .component = json_string_value(component),
        .resource_name = json_string_value(resource_name),
        .destination_path = json_string_value(destination_path),
        .imported_at = json_string_value(imported_at),
    };
    if (source_path && json_is_string(source_path)) {
      record.source_path = std::string{json_string_value(source_path)};
    }
    imports.emplace_back(std::move(record));
  }

  return {.value = std::move(imports)};
}

OperationResult<ImportedComponent> ImportComponentIntoDeployment(
    bconfig::Component component,
    const std::filesystem::path& source_root,
    const std::filesystem::path& deployment_repository_root,
    std::vector<std::string>& logs)
{
  const auto source_config_directory
      = FindComponentConfigDirectory(component, source_root);
  if (!source_config_directory) { return {}; }

  auto loaded = bconfig::LoadConfig(component, source_root.string(), true);
  if (!loaded.parser) {
    AppendParseMessages(
        logs, loaded.messages,
        std::string{bconfig::ComponentToString(component)} + " source ");
    return {.error = "failed to initialize parser for "
                     + std::string{bconfig::ComponentToString(component)}
                     + " import."};
  }

  AppendParseMessages(logs, loaded.messages,
                      std::string{bconfig::ComponentToString(component)}
                          + " source ");
  if (!loaded.parse_ok) {
    return {.error = std::string{bconfig::ComponentToString(component)}
                     + " source config did not parse cleanly."};
  }

  auto resource_name = DetectPrimaryResourceName(component, loaded);
  if (!resource_name) {
    logs.emplace_back("skipping "
                      + std::string{bconfig::ComponentToString(component)}
                      + " import because no primary "
                      + std::string{PrimaryResourceType(component)}
                      + " resource was found");
    return {};
  }

  const auto target_bucket
      = ComponentBucketDirectory(deployment_repository_root, component);
  if (target_bucket.empty()) {
    return {.error = "component import target is not configured."};
  }

  const auto final_root = target_bucket / *resource_name;
  if (std::filesystem::exists(final_root)) {
    return {.error = "import target already exists: " + final_root.string()};
  }

  const auto temp_root = target_bucket / (*resource_name + ".tmp-import");
  RemoveTreeQuietly(temp_root);
  const auto temp_config_directory
      = temp_root / ComponentConfigDirectoryName(component);
  if (auto error
      = CopyDirectoryTree(*source_config_directory, temp_config_directory);
      error) {
    RemoveTreeQuietly(temp_root);
    return {.error = *error};
  }

  auto imported = bconfig::LoadConfig(component, temp_root.string(), true);
  if (!imported.parser) {
    RemoveTreeQuietly(temp_root);
    AppendParseMessages(
        logs, imported.messages,
        std::string{bconfig::ComponentToString(component)} + " imported ");
    return {.error = "failed to initialize parser for imported "
                     + std::string{bconfig::ComponentToString(component)}
                     + " config."};
  }
  AppendParseMessages(logs, imported.messages,
                      std::string{bconfig::ComponentToString(component)}
                          + " imported ");
  if (!imported.parse_ok) {
    RemoveTreeQuietly(temp_root);
    return {.error = std::string{bconfig::ComponentToString(component)}
                     + " imported config did not validate cleanly."};
  }

  std::error_code error_code;
  std::filesystem::rename(temp_root, final_root, error_code);
  if (error_code) {
    RemoveTreeQuietly(temp_root);
    return {.error = "failed to finalize import into '" + final_root.string()
                     + "': " + error_code.message()};
  }

  logs.emplace_back("imported "
                    + std::string{bconfig::ComponentToString(component)} + " '"
                    + *resource_name + "' into " + final_root.string());
  return {.value = ImportedComponent{component, *resource_name, final_root}};
}

}  // namespace

ServiceState::ServiceState(std::filesystem::path state_directory)
    : state_directory_{std::move(state_directory)}
{
  if (!state_directory_.empty()) {
    std::error_code error_code;
    std::filesystem::create_directories(state_directory_, error_code);
    if (error_code) {
      throw std::runtime_error{"failed to create service state directory '"
                               + state_directory_.string() + "': "
                               + error_code.message()};
    }

    if (auto error = LoadState(); error) { throw std::runtime_error{*error}; }
  }

  worker_thread_ = std::thread{&ServiceState::WorkerLoop, this};
}

ServiceState::~ServiceState()
{
  {
    std::lock_guard guard{mutex_};
    stop_worker_ = true;
  }
  worker_condition_.notify_all();
  if (worker_thread_.joinable()) { worker_thread_.join(); }
}

std::filesystem::path RepositoryLayout::ManifestPath(
    const std::filesystem::path& repository_root)
{
  return repository_root / "manifest.json";
}

std::filesystem::path RepositoryLayout::ServiceDirectory(
    const std::filesystem::path& repository_root)
{
  return repository_root / "service";
}

std::filesystem::path RepositoryLayout::OwnershipPath(
    const std::filesystem::path& repository_root)
{
  return ServiceDirectory(repository_root) / "ownership.json";
}

std::filesystem::path RepositoryLayout::ImportStatePath(
    const std::filesystem::path& repository_root)
{
  return ServiceDirectory(repository_root) / "import-state.json";
}

std::filesystem::path RepositoryLayout::DirectorsDirectory(
    const std::filesystem::path& repository_root)
{
  return repository_root / "directors";
}

std::filesystem::path RepositoryLayout::StoragesDirectory(
    const std::filesystem::path& repository_root)
{
  return repository_root / "storages";
}

std::filesystem::path RepositoryLayout::ClientsDirectory(
    const std::filesystem::path& repository_root)
{
  return repository_root / "clients";
}

std::filesystem::path RepositoryLayout::ConsolesDirectory(
    const std::filesystem::path& repository_root)
{
  return repository_root / "consoles";
}

std::vector<std::filesystem::path> RepositoryLayout::Directories(
    const std::filesystem::path& repository_root)
{
  return {repository_root,
          ServiceDirectory(repository_root),
          DirectorsDirectory(repository_root),
          StoragesDirectory(repository_root),
          ClientsDirectory(repository_root),
          ConsolesDirectory(repository_root)};
}

std::string_view ToString(WorkflowMode mode)
{
  switch (mode) {
    case WorkflowMode::kDirectCommit:
      return "direct_commit";
    case WorkflowMode::kReview:
      return "review";
  }

  return "unknown";
}

std::optional<WorkflowMode> ParseWorkflowMode(std::string_view value)
{
  if (value == "direct_commit") { return WorkflowMode::kDirectCommit; }
  if (value == "review") { return WorkflowMode::kReview; }
  return std::nullopt;
}

std::string_view ToString(JobStatus status)
{
  switch (status) {
    case JobStatus::kQueued:
      return "queued";
    case JobStatus::kRunning:
      return "running";
    case JobStatus::kSucceeded:
      return "succeeded";
    case JobStatus::kFailed:
      return "failed";
  }

  return "unknown";
}

std::optional<JobStatus> ParseJobStatus(std::string_view value)
{
  if (value == "queued") { return JobStatus::kQueued; }
  if (value == "running") { return JobStatus::kRunning; }
  if (value == "succeeded") { return JobStatus::kSucceeded; }
  if (value == "failed") { return JobStatus::kFailed; }
  return std::nullopt;
}

OperationResult<DeploymentRecord> ServiceState::CreateDeployment(
    const DeploymentSpec& spec)
{
  if (!IsValidIdentifier(spec.id)) {
    return {.error = "deployment id must contain only letters, digits, '-' "
                      "or '_'."};
  }

  if (spec.name.empty()) {
    return {.error = "deployment name must not be empty."};
  }

  if (spec.repository_path.empty()) {
    return {.error = "repository_path must not be empty."};
  }

  std::lock_guard guard{mutex_};

  if (deployments_.find(spec.id) != deployments_.end()) {
    return {.error = "deployment id already exists."};
  }

  DeploymentRecord record{.id = spec.id,
                            .name = spec.name,
                            .repository_path = spec.repository_path,
                            .workflow_mode = spec.workflow_mode,
                            .created_at = MakeTimestamp()};

  if (auto error = InitializeRepositoryLayout(record); error) {
    return {.error = *error};
  }

  deployments_.emplace(record.id, record);
  if (auto error = SaveStateLocked(); error) {
    deployments_.erase(record.id);
    return {.error = *error};
  }
  return {.value = record};
}

std::vector<DeploymentRecord> ServiceState::ListDeployments() const
{
  std::lock_guard guard{mutex_};
  return SortedValues(deployments_);
}

std::optional<DeploymentRecord> ServiceState::GetDeployment(
    std::string_view id) const
{
  std::lock_guard guard{mutex_};
  auto it = deployments_.find(std::string{id});
  if (it == deployments_.end()) { return std::nullopt; }
  return it->second;
}

OperationResult<std::vector<DeploymentImportRecord>>
ServiceState::ListDeploymentImports(std::string_view deployment_id) const
{
  std::filesystem::path repository_path;
  {
    std::lock_guard guard{mutex_};
    auto it = deployments_.find(std::string{deployment_id});
    if (it == deployments_.end()) {
      return {.error = "deployment not found."};
    }
    repository_path = it->second.repository_path;
  }

  return LoadDeploymentImports(repository_path);
}

OperationResult<JobRecord> ServiceState::CreateJob(const JobSpec& spec)
{
  if (spec.type.empty()) { return {.error = "job type must not be empty."}; }

  std::lock_guard guard{mutex_};

  if (spec.deployment_id
      && deployments_.find(*spec.deployment_id) == deployments_.end()) {
    return {.error = "deployment id does not exist."};
  }

  const auto timestamp = MakeTimestamp();
  JobRecord record{.id = "job-" + std::to_string(next_job_id_++),
                   .type = spec.type,
                   .deployment_id = spec.deployment_id,
                   .source_component = spec.source_component,
                   .source_path = spec.source_path,
                   .status = JobStatus::kQueued,
                   .created_at = timestamp,
                   .updated_at = timestamp};

  jobs_.emplace(record.id, record);
  if (auto error = SaveStateLocked(); error) {
    jobs_.erase(record.id);
    --next_job_id_;
    return {.error = *error};
  }
  worker_condition_.notify_one();
  return {.value = record};
}

std::vector<JobRecord> ServiceState::ListJobs() const
{
  std::lock_guard guard{mutex_};
  return SortedValues(jobs_);
}

std::optional<JobRecord> ServiceState::GetJob(std::string_view id) const
{
  std::lock_guard guard{mutex_};
  auto it = jobs_.find(std::string{id});
  if (it == jobs_.end()) { return std::nullopt; }
  return it->second;
}

const std::filesystem::path& ServiceState::GetStateDirectory() const
{
  return state_directory_;
}

bool ServiceState::HasPersistentState() const
{
  return !state_directory_.empty();
}

void ServiceState::WorkerLoop()
{
  for (;;) {
    std::optional<JobRecord> queued_job;

    {
      std::unique_lock lock{mutex_};
      worker_condition_.wait(lock, [this] {
        if (stop_worker_) { return true; }
        return std::any_of(jobs_.begin(), jobs_.end(), [](const auto& item) {
          return item.second.status == JobStatus::kQueued;
        });
      });

      if (stop_worker_) { return; }
      queued_job = GetNextQueuedJobLocked();
      if (!queued_job) { continue; }
      MarkJobRunning(queued_job->id, "job execution started");
      queued_job = jobs_.at(queued_job->id);
    }

    auto [status, logs] = ExecuteJob(*queued_job);
    std::optional<std::string> last_error{};
    if (status == JobStatus::kFailed && !logs.empty()) {
      last_error = logs.back();
    }

    std::string final_log
        = status == JobStatus::kSucceeded ? "job execution finished"
                                          : "job execution failed";
    if (!logs.empty()) {
      final_log += ": ";
      final_log += logs.back();
    }

    {
      std::lock_guard guard{mutex_};
      auto it = jobs_.find(queued_job->id);
      if (it == jobs_.end()) { continue; }
      it->second.logs.insert(it->second.logs.end(), logs.begin(), logs.end());
      MarkJobFinished(queued_job->id, status, last_error, final_log);
    }
  }
}

std::optional<JobRecord> ServiceState::GetNextQueuedJobLocked()
{
  auto it = std::find_if(jobs_.begin(), jobs_.end(), [](const auto& item) {
    return item.second.status == JobStatus::kQueued;
  });
  if (it == jobs_.end()) { return std::nullopt; }
  return it->second;
}

void ServiceState::MarkJobRunning(const std::string& id, std::string log_message)
{
  auto it = jobs_.find(id);
  if (it == jobs_.end()) { return; }

  const auto timestamp = MakeTimestamp();
  it->second.status = JobStatus::kRunning;
  it->second.updated_at = timestamp;
  it->second.started_at = timestamp;
  it->second.finished_at.reset();
  it->second.last_error.reset();
  it->second.logs.emplace_back(std::move(log_message));
  static_cast<void>(SaveStateLocked());
}

void ServiceState::MarkJobFinished(const std::string& id,
                                   JobStatus status,
                                   std::optional<std::string> last_error,
                                   std::string log_message)
{
  auto it = jobs_.find(id);
  if (it == jobs_.end()) { return; }

  const auto timestamp = MakeTimestamp();
  it->second.status = status;
  it->second.updated_at = timestamp;
  it->second.finished_at = timestamp;
  it->second.last_error = std::move(last_error);
  it->second.logs.emplace_back(std::move(log_message));
  static_cast<void>(SaveStateLocked());
}

void ServiceState::RequeueRunningJobsLocked()
{
  for (auto& [_, job] : jobs_) {
    if (job.status == JobStatus::kRunning) {
      job.status = JobStatus::kQueued;
      job.updated_at = MakeTimestamp();
      job.finished_at.reset();
      job.last_error.reset();
      job.logs.emplace_back("job re-queued after service restart");
    }
  }
}

std::pair<JobStatus, std::vector<std::string>> ServiceState::ExecuteJob(
    const JobRecord& job_snapshot) const
{
  std::vector<std::string> logs;

  if (job_snapshot.type == "validate_deployment_repo") {
    if (!job_snapshot.deployment_id) {
      logs.emplace_back("deployment_id is required");
      return {JobStatus::kFailed, logs};
    }

    std::optional<DeploymentRecord> deployment;
    {
      std::lock_guard guard{mutex_};
      auto it = deployments_.find(*job_snapshot.deployment_id);
      if (it != deployments_.end()) { deployment = it->second; }
    }
    if (!deployment) {
      logs.emplace_back("deployment not found");
      return {JobStatus::kFailed, logs};
    }

    const auto& root = deployment->repository_path;
    if (!std::filesystem::exists(RepositoryLayout::ManifestPath(root))) {
      logs.emplace_back("manifest.json is missing");
      return {JobStatus::kFailed, logs};
    }
    for (const auto& directory : RepositoryLayout::Directories(root)) {
      if (!std::filesystem::is_directory(directory)) {
        logs.emplace_back("missing required directory: " + directory.string());
        return {JobStatus::kFailed, logs};
      }
    }

    const auto configs = DiscoverDeploymentConfigRoots(root);
    if (configs.empty()) {
      logs.emplace_back("repository layout is valid");
      logs.emplace_back("no imported config roots found in deployment repo");
      return {JobStatus::kSucceeded, logs};
    }

    for (const auto& config : configs) {
      auto loaded = bconfig::LoadConfig(config.component, config.path.string(), true);
      if (!loaded.parser) {
        AppendParseMessages(
            logs, loaded.messages,
            std::string{bconfig::ComponentToString(config.component)} + " '"
                + config.name + "' ");
        logs.emplace_back("failed to initialize parser for "
                          + std::string{bconfig::ComponentToString(config.component)}
                          + " '" + config.name + "'");
        return {JobStatus::kFailed, logs};
      }

      AppendParseMessages(
          logs, loaded.messages,
          std::string{bconfig::ComponentToString(config.component)} + " '"
              + config.name + "' ");
      if (!loaded.parse_ok) {
        logs.emplace_back("config root failed validation: "
                          + std::string{bconfig::ComponentToString(config.component)}
                          + " '" + config.name + "'");
        return {JobStatus::kFailed, logs};
      }

      logs.emplace_back("validated "
                        + std::string{bconfig::ComponentToString(config.component)}
                        + " '" + config.name + "'");
    }

    logs.emplace_back("validated " + std::to_string(configs.size())
                      + " imported config root(s)");
    return {JobStatus::kSucceeded, logs};
  }

  if (job_snapshot.type == "import_configuration") {
    if (!job_snapshot.deployment_id) {
      logs.emplace_back("deployment_id is required");
      return {JobStatus::kFailed, logs};
    }
    if (!job_snapshot.source_path) {
      logs.emplace_back("source_path is required");
      return {JobStatus::kFailed, logs};
    }

    std::optional<DeploymentRecord> deployment;
    {
      std::lock_guard guard{mutex_};
      auto it = deployments_.find(*job_snapshot.deployment_id);
      if (it != deployments_.end()) { deployment = it->second; }
    }
    if (!deployment) {
      logs.emplace_back("deployment not found");
      return {JobStatus::kFailed, logs};
    }

    std::string root_error;
    auto source_root = ResolveImportRoot(*job_snapshot.source_path, root_error);
    if (!source_root) {
      logs.emplace_back(std::move(root_error));
      return {JobStatus::kFailed, logs};
    }

    std::vector<ImportedComponent> imported_components;
    imported_components.reserve(SupportedImportComponents().size());
    for (const auto component : SupportedImportComponents()) {
      auto imported = ImportComponentIntoDeployment(component, *source_root,
                                                   deployment->repository_path,
                                                   logs);
      if (!imported.value && imported.error.empty()) { continue; }
      if (!imported) {
        for (const auto& finished_import : imported_components) {
          RemoveTreeQuietly(finished_import.destination_root);
        }
        logs.emplace_back(imported.error);
        return {JobStatus::kFailed, logs};
      }

      imported_components.emplace_back(std::move(*imported.value));
    }

    if (imported_components.empty()) {
      logs.emplace_back("no supported Bareos config trees found under "
                        + source_root->string());
      return {JobStatus::kFailed, logs};
    }

    JobRecord finished_job = job_snapshot;
    finished_job.updated_at = MakeTimestamp();
    if (auto error = UpdateImportState(deployment->repository_path, finished_job,
                                       imported_components);
        error) {
      for (const auto& finished_import : imported_components) {
        RemoveTreeQuietly(finished_import.destination_root);
      }
      logs.emplace_back(*error);
      return {JobStatus::kFailed, logs};
    }

    logs.emplace_back("imported "
                      + std::to_string(imported_components.size())
                      + " component tree(s) from "
                      + source_root->string());
    return {JobStatus::kSucceeded, logs};
  }

  logs.emplace_back("unsupported job type: " + job_snapshot.type);
  return {JobStatus::kFailed, logs};
}

std::string ServiceState::MakeTimestamp()
{
  const auto now = std::chrono::system_clock::to_time_t(
      std::chrono::system_clock::now());
  std::tm utc{};
#if HAVE_WIN32
  gmtime_s(&utc, &now);
#else
  gmtime_r(&now, &utc);
#endif

  std::ostringstream stream;
  stream << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
  return stream.str();
}

bool ServiceState::IsValidIdentifier(std::string_view id)
{
  if (id.empty()) { return false; }

  return std::all_of(id.begin(), id.end(), [](unsigned char ch) {
    return std::isalnum(ch) || ch == '-' || ch == '_';
  });
}

std::string ServiceState::JsonEscape(std::string_view value)
{
  std::string escaped;
  escaped.reserve(value.size());

  for (unsigned char ch : value) {
    switch (ch) {
      case '\\':
        escaped += "\\\\";
        break;
      case '"':
        escaped += "\\\"";
        break;
      case '\b':
        escaped += "\\b";
        break;
      case '\f':
        escaped += "\\f";
        break;
      case '\n':
        escaped += "\\n";
        break;
      case '\r':
        escaped += "\\r";
        break;
      case '\t':
        escaped += "\\t";
        break;
      default:
        if (ch < 0x20) {
          std::ostringstream stream;
          stream << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                 << static_cast<int>(ch);
          escaped += stream.str();
        } else {
          escaped += static_cast<char>(ch);
        }
        break;
    }
  }

  return escaped;
}

std::string ServiceState::ManifestJson(const DeploymentRecord& record)
{
  std::ostringstream stream;
  stream << "{\n"
         << "  \"id\": \"" << JsonEscape(record.id) << "\",\n"
         << "  \"name\": \"" << JsonEscape(record.name) << "\",\n"
         << "  \"repository_path\": \""
         << JsonEscape(record.repository_path.generic_string()) << "\",\n"
         << "  \"workflow_mode\": \"" << ToString(record.workflow_mode)
         << "\",\n"
         << "  \"created_at\": \"" << JsonEscape(record.created_at) << "\"\n"
         << "}\n";
  return stream.str();
}

std::string ServiceState::EmptyObjectJson()
{
  return "{}\n";
}

std::string ServiceState::SerializeState(
    uint64_t next_job_id,
    const std::vector<DeploymentRecord>& deployments,
    const std::vector<JobRecord>& jobs)
{
  std::ostringstream stream;
  stream << "{\n"
         << "  \"next_job_id\": " << next_job_id << ",\n"
         << "  \"deployments\": [\n";

  for (size_t i = 0; i < deployments.size(); ++i) {
    const auto& deployment = deployments[i];
    stream << "    {\n"
           << "      \"id\": \"" << JsonEscape(deployment.id) << "\",\n"
           << "      \"name\": \"" << JsonEscape(deployment.name) << "\",\n"
           << "      \"repository_path\": \""
           << JsonEscape(deployment.repository_path.generic_string())
           << "\",\n"
           << "      \"workflow_mode\": \""
           << ToString(deployment.workflow_mode) << "\",\n"
           << "      \"created_at\": \""
           << JsonEscape(deployment.created_at) << "\"\n"
           << "    }";
    if (i + 1 != deployments.size()) { stream << ","; }
    stream << "\n";
  }

  stream << "  ],\n"
         << "  \"jobs\": [\n";

  for (size_t i = 0; i < jobs.size(); ++i) {
    const auto& job = jobs[i];
    stream << "    {\n"
           << "      \"id\": \"" << JsonEscape(job.id) << "\",\n"
           << "      \"type\": \"" << JsonEscape(job.type) << "\",\n"
           << "      \"deployment_id\": ";
    if (job.deployment_id) {
      stream << "\"" << JsonEscape(*job.deployment_id) << "\"";
    } else {
      stream << "null";
    }
    stream << ",\n"
           << "      \"source_component\": ";
    if (job.source_component) {
      stream << "\"" << JsonEscape(*job.source_component) << "\"";
    } else {
      stream << "null";
    }
    stream << ",\n"
           << "      \"source_path\": ";
    if (job.source_path) {
      stream << "\"" << JsonEscape(*job.source_path) << "\"";
    } else {
      stream << "null";
    }
    stream << ",\n"
           << "      \"status\": \"" << ToString(job.status) << "\",\n"
           << "      \"created_at\": \"" << JsonEscape(job.created_at)
           << "\",\n"
           << "      \"updated_at\": \"" << JsonEscape(job.updated_at)
           << "\",\n"
           << "      \"started_at\": ";
    if (job.started_at) {
      stream << "\"" << JsonEscape(*job.started_at) << "\"";
    } else {
      stream << "null";
    }
    stream << ",\n"
           << "      \"finished_at\": ";
    if (job.finished_at) {
      stream << "\"" << JsonEscape(*job.finished_at) << "\"";
    } else {
      stream << "null";
    }
    stream << ",\n"
           << "      \"last_error\": ";
    if (job.last_error) {
      stream << "\"" << JsonEscape(*job.last_error) << "\"";
    } else {
      stream << "null";
    }
    stream << ",\n"
           << "      \"logs\": [";
    for (size_t log_index = 0; log_index < job.logs.size(); ++log_index) {
      if (log_index > 0) { stream << ", "; }
      stream << "\"" << JsonEscape(job.logs[log_index]) << "\"";
    }
    stream << "]\n"
           << "    }";
    if (i + 1 != jobs.size()) { stream << ","; }
    stream << "\n";
  }

  stream << "  ]\n"
         << "}\n";
  return stream.str();
}

std::optional<std::string> ServiceState::InitializeRepositoryLayout(
    const DeploymentRecord& record)
{
  std::error_code error_code;
  for (const auto& directory :
       RepositoryLayout::Directories(record.repository_path)) {
    if (!std::filesystem::create_directories(directory, error_code)
        && error_code) {
      return "failed to create repository directory '" + directory.string()
             + "': " + error_code.message();
    }
  }

  const auto manifest_path = RepositoryLayout::ManifestPath(record.repository_path);
  if (std::filesystem::exists(manifest_path)) {
    return "repository manifest already exists at '" + manifest_path.string()
           + "'.";
  }

  if (!WriteFile(manifest_path, ManifestJson(record))) {
    return "failed to write manifest '" + manifest_path.string() + "'.";
  }

  const auto ownership_path
      = RepositoryLayout::OwnershipPath(record.repository_path);
  if (!std::filesystem::exists(ownership_path)
      && !WriteFile(ownership_path, EmptyObjectJson())) {
    return "failed to write ownership file '" + ownership_path.string() + "'.";
  }

  const auto import_state_path
      = RepositoryLayout::ImportStatePath(record.repository_path);
  if (!std::filesystem::exists(import_state_path)
      && !WriteFile(import_state_path, EmptyObjectJson())) {
    return "failed to write import state file '" + import_state_path.string()
           + "'.";
  }

  return std::nullopt;
}

std::optional<std::string> ServiceState::LoadState()
{
  const auto state_file = state_directory_ / "service-state.json";
  if (!std::filesystem::exists(state_file)) { return std::nullopt; }

  json_error_t json_error{};
  auto root = MakeJson(json_load_file(state_file.c_str(), 0, &json_error));
  if (!root) {
    return "failed to parse service state '" + state_file.string()
           + "': " + json_error.text;
  }

  auto* next_job_id = json_object_get(root.get(), "next_job_id");
  auto* deployments = json_object_get(root.get(), "deployments");
  auto* jobs = json_object_get(root.get(), "jobs");
  if (!json_is_integer(next_job_id) || !json_is_array(deployments)
      || !json_is_array(jobs)) {
    return "service state file '" + state_file.string()
           + "' is missing required fields.";
  }

  deployments_.clear();
  jobs_.clear();
  next_job_id_ = static_cast<uint64_t>(json_integer_value(next_job_id));

  size_t index = 0;
  json_t* entry = nullptr;
  json_array_foreach(deployments, index, entry)
  {
    auto* id = json_object_get(entry, "id");
    auto* name = json_object_get(entry, "name");
    auto* repository_path = json_object_get(entry, "repository_path");
    auto* workflow_mode = json_object_get(entry, "workflow_mode");
    auto* created_at = json_object_get(entry, "created_at");

    if (!json_is_string(id) || !json_is_string(name)
        || !json_is_string(repository_path) || !json_is_string(workflow_mode)
        || !json_is_string(created_at)) {
      return "service state file '" + state_file.string()
             + "' contains an invalid deployment entry.";
    }

    auto parsed_workflow = ParseWorkflowMode(json_string_value(workflow_mode));
    if (!parsed_workflow) {
      return "service state file '" + state_file.string()
             + "' contains an unknown workflow mode.";
    }

    DeploymentRecord record{
        .id = json_string_value(id),
        .name = json_string_value(name),
        .repository_path = json_string_value(repository_path),
        .workflow_mode = *parsed_workflow,
        .created_at = json_string_value(created_at)};
    deployments_.emplace(record.id, std::move(record));
  }

  json_array_foreach(jobs, index, entry)
  {
    auto* id = json_object_get(entry, "id");
    auto* type = json_object_get(entry, "type");
    auto* deployment_id = json_object_get(entry, "deployment_id");
    auto* source_component = json_object_get(entry, "source_component");
    auto* source_path = json_object_get(entry, "source_path");
    auto* status = json_object_get(entry, "status");
    auto* created_at = json_object_get(entry, "created_at");
    auto* updated_at = json_object_get(entry, "updated_at");
    auto* started_at = json_object_get(entry, "started_at");
    auto* finished_at = json_object_get(entry, "finished_at");
    auto* last_error = json_object_get(entry, "last_error");
    auto* logs = json_object_get(entry, "logs");

    if (!json_is_string(id) || !json_is_string(type)
        || (deployment_id && !json_is_null(deployment_id)
            && !json_is_string(deployment_id))
        || (source_component && !json_is_null(source_component)
            && !json_is_string(source_component))
        || (source_path && !json_is_null(source_path)
            && !json_is_string(source_path))
        || !json_is_string(status) || !json_is_string(created_at)
        || !json_is_string(updated_at)
        || (started_at && !json_is_null(started_at) && !json_is_string(started_at))
        || (finished_at && !json_is_null(finished_at)
            && !json_is_string(finished_at))
        || (last_error && !json_is_null(last_error) && !json_is_string(last_error))
        || (logs && !json_is_array(logs))) {
      return "service state file '" + state_file.string()
             + "' contains an invalid job entry.";
    }

    auto parsed_status = ParseJobStatus(json_string_value(status));
    if (!parsed_status) {
      return "service state file '" + state_file.string()
             + "' contains an unknown job status.";
    }

    JobRecord record{.id = json_string_value(id),
                     .type = json_string_value(type),
                     .deployment_id = std::nullopt,
                     .source_component = std::nullopt,
                     .source_path = std::nullopt,
                     .status = *parsed_status,
                     .created_at = json_string_value(created_at),
                     .updated_at = json_string_value(updated_at)};
    if (deployment_id && json_is_string(deployment_id)) {
      record.deployment_id = std::string{json_string_value(deployment_id)};
    }
    if (source_component && json_is_string(source_component)) {
      record.source_component = std::string{json_string_value(source_component)};
    }
    if (source_path && json_is_string(source_path)) {
      record.source_path = std::string{json_string_value(source_path)};
    }
    if (started_at && json_is_string(started_at)) {
      record.started_at = std::string{json_string_value(started_at)};
    }
    if (finished_at && json_is_string(finished_at)) {
      record.finished_at = std::string{json_string_value(finished_at)};
    }
    if (last_error && json_is_string(last_error)) {
      record.last_error = std::string{json_string_value(last_error)};
    }
    if (logs) {
      size_t log_index = 0;
      json_t* log_entry = nullptr;
      json_array_foreach(logs, log_index, log_entry)
      {
        if (!json_is_string(log_entry)) {
          return "service state file '" + state_file.string()
                 + "' contains a non-string job log entry.";
        }
        record.logs.emplace_back(json_string_value(log_entry));
      }
    }

    jobs_.emplace(record.id, std::move(record));
  }

  RequeueRunningJobsLocked();
  if (next_job_id_ == 0) { next_job_id_ = 1; }
  return std::nullopt;
}

std::optional<std::string> ServiceState::SaveStateLocked() const
{
  if (state_directory_.empty()) { return std::nullopt; }

  const auto state_file = state_directory_ / "service-state.json";
  const auto content = SerializeState(next_job_id_, SortedValues(deployments_),
                                      SortedValues(jobs_));
  if (!WriteFile(state_file, content)) {
    return "failed to write service state '" + state_file.string() + "'.";
  }

  return std::nullopt;
}

}  // namespace bconfig::service
