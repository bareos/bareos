/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2026 Bareos GmbH & Co. KG

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
#include "config_service.h"

#include <algorithm>
#include <cctype>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <array>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <sys/wait.h>
#include <unistd.h>

#include "config_model.h"

namespace {
std::vector<std::string> SplitPath(const std::string& path)
{
  std::vector<std::string> parts;
  std::string current;
  for (char c : path) {
    if (c == '/') {
      if (!current.empty()) {
        parts.push_back(current);
        current.clear();
      }
      continue;
    }
    current += c;
  }
  if (!current.empty()) parts.push_back(current);
  return parts;
}

std::string TrimWhitespace(std::string value)
{
  while (!value.empty()
         && std::isspace(static_cast<unsigned char>(value.front()))) {
    value.erase(value.begin());
  }
  while (!value.empty()
         && std::isspace(static_cast<unsigned char>(value.back()))) {
    value.pop_back();
  }
  return value;
}

std::string JsonEscape(const std::string& text)
{
  std::string escaped;
  escaped.reserve(text.size() + 8);
  for (const char c : text) {
    switch (c) {
      case '\\':
        escaped += "\\\\";
        break;
      case '"':
        escaped += "\\\"";
        break;
      case '\n':
        escaped += "\\n";
        break;
      default:
        escaped += c;
        break;
    }
  }
  return escaped;
}

std::string JsonString(const std::string& text)
{
  return "\"" + JsonEscape(text) + "\"";
}

const TreeNodeSummary* FindDatacenterDirectorNode(const TreeNodeSummary& datacenter,
                                                  const std::string& director_id)
{
  const auto it = std::find_if(
      datacenter.children.begin(), datacenter.children.end(),
      [&director_id](const TreeNodeSummary& child) {
        return child.kind == "director" && child.id == director_id;
      });
  return it == datacenter.children.end() ? nullptr : &(*it);
}

bool SupportsManualResourceCreation(const TreeNodeSummary& node)
{
  return node.kind == "director" || node.kind == "file-daemon"
         || node.kind == "storage-daemon";
}

std::vector<std::string> CreatableResourceTypesForNodeKind(
    const std::string& kind)
{
  if (kind == "director") {
    return {"catalog", "client", "console", "counter", "director", "fileset",
            "job", "jobdefs", "messages", "pool", "profile", "schedule",
            "storage", "user"};
  }
  if (kind == "file-daemon") { return {"client", "director", "messages"}; }
  if (kind == "storage-daemon") {
    return {"autochanger", "device", "director", "messages", "ndmp",
            "storage"};
  }
  return {};
}

std::string SerializeStringArray(const std::vector<std::string>& values)
{
  std::ostringstream output;
  output << "[";
  for (size_t i = 0; i < values.size(); ++i) {
    if (i > 0) output << ",";
    output << JsonString(values[i]);
  }
  output << "]";
  return output.str();
}

std::string SerializeNodeDetail(const TreeNodeSummary& node)
{
  auto body = SerializeTreeNodeSummary(node);
  if (body.empty() || body.back() != '}') return body;
  body.pop_back();
  body += ",\"creatable_resource_types\":"
          + SerializeStringArray(CreatableResourceTypesForNodeKind(node.kind))
          + "}";
  return body;
}

std::string ConfigIncludeDirForNodeKind(const std::string& kind)
{
  if (kind == "director") return "bareos-dir.d";
  if (kind == "file-daemon") return "bareos-fd.d";
  if (kind == "storage-daemon") return "bareos-sd.d";
  return "";
}

std::vector<std::string> CreatableResourceTypesForNode(const TreeNodeSummary& node)
{
  auto types = CreatableResourceTypesForNodeKind(node.kind);
  types.erase(
      std::remove_if(types.begin(), types.end(),
                     [](const std::string& type) {
                       return ResourceBlockNameForType(type).empty();
                     }),
      types.end());
  return types;
}

ResourceDetail BuildNewResourceDetail(const TreeNodeSummary& node,
                                      const std::string& resource_type)
{
  const auto include_dir = ConfigIncludeDirForNodeKind(node.kind);
  const auto block_name = ResourceBlockNameForType(resource_type);
  const auto file_path = std::filesystem::path(node.description) / include_dir
                         / resource_type / ("new-" + resource_type + ".conf");
  const ResourceSummary summary{"new-resource-" + node.id + "-" + resource_type,
                                resource_type, "new-" + resource_type,
                                file_path.string()};
  return BuildResourceDetail(summary, block_name + " {\n}\n");
}

struct AddClientTargetPlan {
  std::string file_path;
  std::string action;
  bool exists_local = false;
};

std::string SanitizePathComponent(std::string value)
{
  value = TrimWhitespace(value);
  for (auto& ch : value) {
    if (ch == '/' || ch == '\\' || std::iscntrl(static_cast<unsigned char>(ch))) {
      ch = '_';
    }
  }
  if (value.empty()) return "unnamed";
  return value;
}

std::filesystem::path BuildGeneratedRemoteConfigRoot(
    const ConfigServiceOptions& options,
    const std::string& target_kind,
    const std::string& target_name)
{
  const auto bucket = target_kind == "storage" ? "storages" : "clients";
  return options.generated_config_root / bucket
         / SanitizePathComponent(target_name) / "etc" / "bareos";
}

bool IsGeneratedRemotePath(const std::string& path)
{
  return path.find("/generated/clients/") != std::string::npos
         || path.find("/generated/storages/") != std::string::npos
         || path.find("/var/lib/bareos-config/generated/clients/")
                != std::string::npos
         || path.find("/var/lib/bareos-config/generated/storages/")
                != std::string::npos;
}

std::string MakeGeneratedClientConfigId(const std::string& director_id,
                                        const std::string& client_name)
{
  return "generated-client-config-" + director_id + "-"
         + SanitizePathComponent(client_name);
}

struct PersistResult;
struct VerificationResult;
std::vector<ResourceSummary> CollectDatacenterSyntheticResources(
    const ConfigServiceOptions& options, const DatacenterSummary& model);
std::optional<ResourceSummary> ResolveResourceSummary(
    const ConfigServiceOptions& options,
    const DatacenterSummary& model,
    const std::string& resource_id);
bool DeleteFileWithBackup(const std::filesystem::path& path,
                          std::string& action,
                          std::string& backup_path,
                          std::string& error);
bool RemoveExistingResource(const ResourceSummary& summary,
                            std::string& action,
                            std::string& backup_path,
                            std::string& error);
void PopulateFollowUpAction(PersistResult& result);
void RollbackPersistedResource(const PersistResult& result);
VerificationResult VerifySavedConfig(const ConfigServiceOptions& options,
                                     const std::string& file_path);

struct NamedResourceBlockRange {
  std::string name;
  size_t index = 0;
  size_t start_line = 0;
  size_t end_line = 0;
};

struct RenameImpact {
  ResourceSummary resource;
  std::string directive_key;
  size_t line = 0;
  std::string old_value;
  std::string new_value;
};

struct RenameAffectedUpdate {
  ResourceDetail detail;
  ResourceEditPreview preview;
  std::vector<RenameImpact> impacts;
};

struct RenameAwarePreview {
  ResourceEditPreview preview;
  std::vector<RenameImpact> rename_impacts;
  std::vector<RenameAffectedUpdate> affected_updates;
};

struct PersistedAffectedUpdate {
  ResourceSummary resource;
  std::string action;
  std::string path;
  std::string backup_path;
};

struct PersistResult {
  bool saved = false;
  int status_code = 200;
  std::string action = "unchanged";
  std::string backup_path;
  std::string error;
  ResourceEditPreview preview;
  std::string verification_command;
  std::string verification_output;
  std::string follow_up_component;
  std::string follow_up_action;
  std::string follow_up_command;
  std::string follow_up_message;
  std::vector<RenameImpact> rename_impacts;
  std::vector<PersistedAffectedUpdate> affected_updates;
};

struct RemoteClientRemovalResult {
  bool removed = false;
  int status_code = 200;
  std::string error;
  PersistResult client_resource;
  PersistResult generated_config;
};

struct WizardPersistResult {
  bool saved = false;
  int status_code = 200;
  std::string error;
  PersistResult director_client;
  PersistResult client_file_daemon_director;
};

enum class DaemonComponent {
  kUnknown,
  kDirector,
  kFileDaemon,
  kStorageDaemon,
};

struct VerificationResult {
  bool success = false;
  std::string command;
  std::string output;
  std::string error;
};

std::string StemWithoutExtension(const std::filesystem::path& path)
{
  return path.stem().string();
}

std::vector<std::string> SplitLines(const std::string& content)
{
  std::vector<std::string> lines;
  std::istringstream input(content);
  std::string line;
  while (std::getline(input, line)) {
    lines.push_back(line);
  }
  return lines;
}

std::string JoinLines(const std::vector<std::string>& lines)
{
  if (lines.empty()) return "";

  std::ostringstream output;
  for (size_t i = 0; i < lines.size(); ++i) {
    if (i > 0) output << "\n";
    output << lines[i];
  }
  output << "\n";
  return output.str();
}

std::string Lowercase(std::string value)
{
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return value;
}

std::string NormalizeDirectiveNameForComparison(std::string value)
{
  value = Lowercase(std::move(value));
  value.erase(std::remove_if(value.begin(), value.end(),
                             [](unsigned char c) { return std::isspace(c); }),
              value.end());
  return value;
}

std::string StripSurroundingQuotes(std::string value)
{
  value = TrimWhitespace(std::move(value));
  if (value.size() < 2) return value;
  const auto first = value.front();
  const auto last = value.back();
  if ((first == '"' || first == '\'') && first == last) {
    return value.substr(1, value.size() - 2);
  }
  return value;
}

std::vector<std::string> SplitRepeatableFieldValue(const std::string& value)
{
  std::vector<std::string> values;
  std::istringstream input(value);
  std::string line;
  while (std::getline(input, line)) {
    auto trimmed = TrimWhitespace(line);
    if (!trimmed.empty()) values.push_back(std::move(trimmed));
  }
  return values;
}

std::string JoinRepeatableFieldValues(const std::vector<std::string>& values)
{
  std::ostringstream output;
  bool first = true;
  for (const auto& value : values) {
    if (value.empty()) continue;
    if (!first) output << '\n';
    output << value;
    first = false;
  }
  return output.str();
}

std::string PreserveReferenceQuoting(const std::string& original_value,
                                     const std::string& replacement)
{
  const auto trimmed = TrimWhitespace(original_value);
  if (trimmed.size() >= 2 && (trimmed.front() == '"' || trimmed.front() == '\'')
      && trimmed.front() == trimmed.back()) {
    return std::string(1, trimmed.front()) + replacement
           + std::string(1, trimmed.front());
  }
  return replacement;
}

bool IsTopLevelDirective(const ResourceDirective& directive)
{
  return directive.nesting_level == 1;
}

std::string ExtractResourceName(const ResourceSummary& summary,
                                const std::vector<ResourceDirective>& directives)
{
  for (const auto& directive : directives) {
    if (!IsTopLevelDirective(directive)) continue;
    if (NormalizeDirectiveNameForComparison(directive.key) != "name") continue;
    const auto value = StripSurroundingQuotes(directive.value);
    if (!value.empty()) return value;
  }
  return StripSurroundingQuotes(summary.name);
}

size_t FindDirectiveLine(const std::vector<ResourceDirective>& directives,
                         const std::string& key)
{
  const auto normalized_key = NormalizeDirectiveNameForComparison(key);
  for (const auto& directive : directives) {
    if (!IsTopLevelDirective(directive)) continue;
    if (NormalizeDirectiveNameForComparison(directive.key) != normalized_key) continue;
    return directive.line;
  }
  return 0;
}

std::string ExtractDirectiveValue(const std::vector<ResourceDirective>& directives,
                                  const std::string& key)
{
  const auto normalized_key = NormalizeDirectiveNameForComparison(key);
  for (const auto& directive : directives) {
    if (!IsTopLevelDirective(directive)) continue;
    if (NormalizeDirectiveNameForComparison(directive.key) != normalized_key) continue;
    return directive.value;
  }
  return "";
}

std::vector<ResourceSummary> CollectAllResources(const DatacenterSummary& summary)
{
  std::vector<ResourceSummary> resources;
  for (const auto& director : summary.directors) {
    resources.insert(resources.end(), director.resources.begin(),
                     director.resources.end());
    for (const auto& daemon : director.daemons) {
      resources.insert(resources.end(), daemon.resources.begin(),
                       daemon.resources.end());
    }
  }
  return resources;
}

const DirectorSummary* FindOwningDirectorForResource(
    const DatacenterSummary& summary, const std::string& resource_id)
{
  for (const auto& director : summary.directors) {
    if (std::any_of(director.resources.begin(), director.resources.end(),
                    [&resource_id](const ResourceSummary& resource) {
                      return resource.id == resource_id;
                    })) {
      return &director;
    }
  }
  return nullptr;
}

const DirectorSummary* FindOwningDirectorForDaemonResource(
    const DatacenterSummary& summary,
    const std::string& resource_id,
    const DaemonSummary** daemon_out = nullptr)
{
  for (const auto& director : summary.directors) {
    for (const auto& daemon : director.daemons) {
      if (std::any_of(daemon.resources.begin(), daemon.resources.end(),
                      [&resource_id](const ResourceSummary& resource) {
                        return resource.id == resource_id;
                      })) {
        if (daemon_out) *daemon_out = &daemon;
        return &director;
      }
    }
  }
  if (daemon_out) *daemon_out = nullptr;
  return nullptr;
}

const DaemonSummary* FindDaemonById(const DatacenterSummary& summary,
                                    const std::string& daemon_id)
{
  for (const auto& director : summary.directors) {
    const auto it = std::find_if(
        director.daemons.begin(), director.daemons.end(),
        [&daemon_id](const DaemonSummary& daemon) { return daemon.id == daemon_id; });
    if (it != director.daemons.end()) return &(*it);
  }
  return nullptr;
}

const DaemonSummary* FindDirectorDaemonByKind(const DirectorSummary& director,
                                              const std::string& kind)
{
  const auto it = std::find_if(director.daemons.begin(), director.daemons.end(),
                               [&kind](const DaemonSummary& daemon) {
                                 return daemon.kind == kind;
                               });
  return it == director.daemons.end() ? nullptr : &(*it);
}

const DirectorSummary* FindDirectorById(const DatacenterSummary& summary,
                                        const std::string& director_id)
{
  const auto it = std::find_if(summary.directors.begin(), summary.directors.end(),
                               [&director_id](const DirectorSummary& director) {
                                 return director.id == director_id;
                               });
  return it == summary.directors.end() ? nullptr : &(*it);
}

std::vector<ResourceSummary> CollectDatacenterSyntheticResources(
    const ConfigServiceOptions& options, const DatacenterSummary& model)
{
  std::vector<ResourceSummary> resources;
  for (const auto& director : model.directors) {
    const auto* file_daemon = FindDirectorDaemonByKind(director, "file-daemon");
    const auto local_file_daemon_name
        = file_daemon ? StripSurroundingQuotes(file_daemon->configured_name) : "";
    for (const auto& resource : director.resources) {
      if (resource.type != "client") continue;

      const auto client_name = StripSurroundingQuotes(resource.name);
      if (client_name.empty()) continue;
      if (!local_file_daemon_name.empty() && client_name == local_file_daemon_name) {
        continue;
      }

      resources.push_back(resource);

      const auto generated_path
          = BuildGeneratedRemoteConfigRoot(options, "client", client_name)
            / "bareos-fd.d" / "director" / (director.name + ".conf");
      if (!std::filesystem::exists(generated_path)) continue;

      resources.push_back(
          {MakeGeneratedClientConfigId(director.id, client_name),
           "configuration", client_name + " generated client config",
           generated_path.string()});
    }
  }

  std::sort(resources.begin(), resources.end(),
            [](const ResourceSummary& lhs, const ResourceSummary& rhs) {
              if (lhs.type != rhs.type) return lhs.type < rhs.type;
              return lhs.name < rhs.name;
            });
  return resources;
}

std::optional<ResourceSummary> ResolveResourceSummary(
    const ConfigServiceOptions& options,
    const DatacenterSummary& model,
    const std::string& resource_id)
{
  if (const auto* resource = FindResourceById(model, resource_id)) {
    return *resource;
  }

  const auto resources = CollectDatacenterSyntheticResources(options, model);
  const auto it = std::find_if(
      resources.begin(), resources.end(),
      [&resource_id](const ResourceSummary& resource) {
        return resource.id == resource_id;
      });
  if (it == resources.end()) return std::nullopt;
  return *it;
}

std::vector<ResourceSummary> CollectNodeResources(
    const ConfigServiceOptions& options,
    const DatacenterSummary& model,
    const TreeNodeSummary& node)
{
  if (node.kind == "datacenter") {
    return CollectDatacenterSyntheticResources(options, model);
  }

  auto resources = node.resources;
  if (node.kind != "client" || node.resources.empty()) return resources;

  const auto* director = FindOwningDirectorForResource(model, node.resources.front().id);
  if (!director) return resources;

  const auto generated_path
      = BuildGeneratedRemoteConfigRoot(options, "client", node.label)
        / "bareos-fd.d" / "director" / (director->name + ".conf");
  if (!std::filesystem::exists(generated_path)) return resources;

  resources.push_back({MakeGeneratedClientConfigId(director->id, node.label),
                       "configuration",
                       node.label + " generated client config",
                       generated_path.string()});
  return resources;
}

RemoteClientRemovalResult RemoveRemoteClient(
    const ConfigServiceOptions& options,
    const DatacenterSummary& model,
    const TreeNodeSummary& node)
{
  auto result = RemoteClientRemovalResult{};
  if (node.kind != "client" || node.resources.empty()) {
    result.status_code = 404;
    result.error = "Remote client not found.";
    return result;
  }

  const auto client_resource = node.resources.front();
  const auto* director = FindOwningDirectorForResource(model, client_resource.id);
  if (!director) {
    result.status_code = 404;
    result.error = "Owning director for remote client not found.";
    return result;
  }

  auto client_detail = LoadResourceDetail(client_resource);
  result.client_resource.preview = BuildResourceEditPreview(client_detail,
                                                            client_detail.content);
  result.client_resource.preview.summary = client_resource;

  std::string remove_error;
  if (!RemoveExistingResource(client_resource, result.client_resource.action,
                              result.client_resource.backup_path, remove_error)) {
    result.status_code = 409;
    result.error = remove_error;
    result.client_resource.status_code = 409;
    result.client_resource.error = remove_error;
    return result;
  }
  result.client_resource.saved = true;

  const auto generated_path
      = BuildGeneratedRemoteConfigRoot(options, "client", node.label)
        / "bareos-fd.d" / "director" / (director->name + ".conf");
  const ResourceSummary generated_summary{
      MakeGeneratedClientConfigId(director->id, node.label),
      "configuration",
      node.label + " generated client config",
      generated_path.string()};
  result.generated_config.preview.summary = generated_summary;
  if (std::filesystem::exists(generated_path)) {
    auto generated_detail = LoadResourceDetail(generated_summary);
    result.generated_config.preview = BuildResourceEditPreview(generated_detail,
                                                               generated_detail.content);
    result.generated_config.preview.summary = generated_summary;
  }

  if (!DeleteFileWithBackup(generated_path, result.generated_config.action,
                            result.generated_config.backup_path, remove_error)) {
    RollbackPersistedResource(result.client_resource);
    result.client_resource.saved = false;
    result.client_resource.error = "Rolled back after generated config removal failed.";
    result.status_code = 409;
    result.error = remove_error;
    result.generated_config.status_code = 409;
    result.generated_config.error = remove_error;
    return result;
  }
  result.generated_config.saved = true;

  const auto verification = VerifySavedConfig(options, client_resource.file_path);
  result.client_resource.verification_command = verification.command;
  result.client_resource.verification_output = verification.output;
  if (!verification.success) {
    RollbackPersistedResource(result.generated_config);
    RollbackPersistedResource(result.client_resource);
    result.client_resource.saved = false;
    result.generated_config.saved = false;
    result.status_code = 409;
    result.error = verification.error.empty()
                       ? "Daemon configuration verification failed."
                       : verification.error;
    result.client_resource.error = result.error;
    return result;
  }

  PopulateFollowUpAction(result.client_resource);
  result.removed = true;
  return result;
}

DaemonComponent DetectComponentForPath(const std::filesystem::path& path)
{
  const auto path_string = path.string();
  if (path_string.find("bareos-dir.d") != std::string::npos
      || path.filename() == "bareos-dir.conf") {
    return DaemonComponent::kDirector;
  }
  if (path_string.find("bareos-fd.d") != std::string::npos
      || path.filename() == "bareos-fd.conf") {
    return DaemonComponent::kFileDaemon;
  }
  if (path_string.find("bareos-sd.d") != std::string::npos
      || path.filename() == "bareos-sd.conf") {
    return DaemonComponent::kStorageDaemon;
  }
  return DaemonComponent::kUnknown;
}

std::filesystem::path FindConfigRootForPath(const std::filesystem::path& path)
{
  for (auto current = path; !current.empty(); current = current.parent_path()) {
    const auto name = current.filename().string();
    if (name == "bareos-dir.d" || name == "bareos-fd.d" || name == "bareos-sd.d") {
      return current.parent_path();
    }
  }

  const auto filename = path.filename().string();
  if (filename == "bareos-dir.conf" || filename == "bareos-fd.conf"
      || filename == "bareos-sd.conf") {
    return path.parent_path();
  }

  return {};
}

std::string VerificationBinaryForComponent(const ConfigServiceOptions& options,
                                           DaemonComponent component)
{
  switch (component) {
    case DaemonComponent::kDirector:
      return options.bareos_dir_binary;
    case DaemonComponent::kFileDaemon:
      return options.bareos_fd_binary;
    case DaemonComponent::kStorageDaemon:
      return options.bareos_sd_binary;
    case DaemonComponent::kUnknown:
      return "";
  }
  return "";
}

VerificationResult RunVerificationCommand(const std::vector<std::string>& args)
{
  auto result = VerificationResult{};
  if (args.empty()) {
    result.error = "Verification command is empty.";
    return result;
  }

  for (size_t i = 0; i < args.size(); ++i) {
    if (i > 0) result.command += " ";
    result.command += args[i];
  }

  int pipe_fd[2];
  if (pipe(pipe_fd) != 0) {
    result.error = "Failed to create pipe for verification command.";
    return result;
  }

  const pid_t pid = fork();
  if (pid < 0) {
    close(pipe_fd[0]);
    close(pipe_fd[1]);
    result.error = "Failed to fork verification command.";
    return result;
  }

  if (pid == 0) {
    dup2(pipe_fd[1], STDOUT_FILENO);
    dup2(pipe_fd[1], STDERR_FILENO);
    close(pipe_fd[0]);
    close(pipe_fd[1]);

    std::vector<char*> argv;
    argv.reserve(args.size() + 1);
    for (const auto& arg : args) {
      argv.push_back(const_cast<char*>(arg.c_str()));
    }
    argv.push_back(nullptr);
    execvp(argv[0], argv.data());
    _exit(127);
  }

  close(pipe_fd[1]);
  std::array<char, 4096> buffer{};
  ssize_t read_count = 0;
  while ((read_count = read(pipe_fd[0], buffer.data(), buffer.size())) > 0) {
    result.output.append(buffer.data(), static_cast<size_t>(read_count));
  }
  close(pipe_fd[0]);

  int status = 0;
  if (waitpid(pid, &status, 0) < 0) {
    result.error = "Failed waiting for verification command.";
    return result;
  }

  result.success = WIFEXITED(status) && WEXITSTATUS(status) == 0;
  if (!result.success && result.output.empty()) {
    result.error = "Verification command failed without output.";
  }
  return result;
}

std::vector<NamedResourceBlockRange> ParseNamedResourceBlockRanges(
    const std::filesystem::path& path, const std::string& block_name)
{
  std::ifstream input(path);
  if (!input) return {};

  std::vector<NamedResourceBlockRange> blocks;
  std::string line;
  bool in_block = false;
  size_t brace_depth = 0;
  size_t line_number = 0;
  size_t start_line = 0;
  std::string current_name;

  while (std::getline(input, line)) {
    ++line_number;
    auto parsed_line = line;
    const auto comment_start = parsed_line.find('#');
    if (comment_start != std::string::npos) {
      parsed_line = parsed_line.substr(0, comment_start);
    }
    const auto trimmed = TrimWhitespace(parsed_line);

    if (!in_block) {
      if (trimmed.rfind(block_name, 0) != 0
          || trimmed.find('{') == std::string::npos) {
        continue;
      }

      in_block = true;
      brace_depth = std::count(parsed_line.begin(), parsed_line.end(), '{')
                    - std::count(parsed_line.begin(), parsed_line.end(), '}');
      start_line = line_number - 1;
      current_name.clear();
      continue;
    }

    const auto separator = trimmed.find('=');
    if (separator != std::string::npos) {
      const auto key = TrimWhitespace(trimmed.substr(0, separator));
      if (key == "Name" && current_name.empty()) {
        current_name = TrimWhitespace(trimmed.substr(separator + 1));
      }
    }

    brace_depth += std::count(parsed_line.begin(), parsed_line.end(), '{');
    brace_depth -= std::min(brace_depth,
                            static_cast<size_t>(std::count(parsed_line.begin(),
                                                           parsed_line.end(),
                                                           '}')));
    if (brace_depth == 0) {
      blocks.push_back({current_name, blocks.size(), start_line, line_number - 1});
      in_block = false;
    }
  }

  const auto fallback_stem = StemWithoutExtension(path);
  for (auto& block : blocks) {
    if (!block.name.empty()) continue;
    block.name = blocks.size() == 1 ? fallback_stem
                                    : fallback_stem + "-"
                                          + std::to_string(block.index);
  }

  return blocks;
}

bool HasValidationErrors(const std::vector<ValidationMessage>& messages)
{
  return std::any_of(messages.begin(), messages.end(),
                     [](const ValidationMessage& message) {
                       return message.level == "error";
                     });
}

std::string ReadFileToString(const std::filesystem::path& path)
{
  std::ifstream input(path);
  std::ostringstream output;
  output << input.rdbuf();
  return output.str();
}

std::string BuildBackupPath(const std::filesystem::path& path)
{
  const auto now = std::time(nullptr);
  std::tm local_time{};
  localtime_r(&now, &local_time);
  char timestamp[32];
  std::strftime(timestamp, sizeof(timestamp), "%Y%m%d%H%M%S", &local_time);
  return path.string() + "." + timestamp + ".bak";
}

bool WriteFileWithBackup(const std::filesystem::path& path,
                         const std::string& content,
                         std::string& action,
                         std::string& backup_path,
                         std::string& error)
{
  std::error_code ec;
  const auto parent = path.parent_path();
  if (!parent.empty()) {
    std::filesystem::create_directories(parent, ec);
    if (ec) {
      error = "Failed to create parent directory for "
              + path.string() + ".";
      return false;
    }
  }

  const auto file_exists = std::filesystem::exists(path);
  if (file_exists) {
    const auto current_content = ReadFileToString(path);
    if (current_content == content) {
      action = "unchanged";
      backup_path.clear();
      return true;
    }

    backup_path = BuildBackupPath(path);
    std::filesystem::copy_file(path, backup_path,
                               std::filesystem::copy_options::overwrite_existing,
                               ec);
    if (ec) {
      error = "Failed to create backup for " + path.string() + ".";
      return false;
    }
    action = "update";
  } else {
    action = "create";
    backup_path.clear();
  }

  std::ofstream output(path, std::ios::trunc);
  if (!output) {
    error = "Failed to open " + path.string() + " for writing.";
    return false;
  }
  output << content;
  if (!output) {
    error = "Failed to write " + path.string() + ".";
    return false;
  }
  return true;
}

bool DeleteFileWithBackup(const std::filesystem::path& path,
                          std::string& action,
                          std::string& backup_path,
                          std::string& error)
{
  if (!std::filesystem::exists(path)) {
    action = "unchanged";
    backup_path.clear();
    return true;
  }

  std::error_code ec;
  backup_path = BuildBackupPath(path);
  std::filesystem::copy_file(path, backup_path,
                             std::filesystem::copy_options::overwrite_existing,
                             ec);
  if (ec) {
    error = "Failed to create backup for " + path.string() + ".";
    return false;
  }

  std::filesystem::remove(path, ec);
  if (ec) {
    error = "Failed to remove " + path.string() + ".";
    return false;
  }

  action = "delete";
  return true;
}

bool SaveExistingResourceContent(const ResourceSummary& summary,
                                 const std::string& updated_content,
                                 std::string& action,
                                 std::string& backup_path,
                                 std::string& error)
{
  const auto path = std::filesystem::path(summary.file_path);
  const auto block_name = ResourceBlockNameForType(summary.type);
  if (block_name.empty() || !std::filesystem::exists(path)) {
    return WriteFileWithBackup(path, updated_content, action, backup_path, error);
  }

  const auto original_content = ReadFileToString(path);
  const auto block_ranges = ParseNamedResourceBlockRanges(path, block_name);
  std::vector<NamedResourceBlockRange> matches;
  for (const auto& block : block_ranges) {
    if (block.name == summary.name) matches.push_back(block);
  }
  if (matches.empty()) {
    error = "Resource block could not be located in "
            + summary.file_path + ".";
    return false;
  }
  if (matches.size() > 1) {
    error = "Resource block name is ambiguous in " + summary.file_path + ".";
    return false;
  }

  auto lines = SplitLines(original_content);
  const auto replacement_lines = SplitLines(updated_content);
  const auto& match = matches.front();
  lines.erase(lines.begin() + static_cast<std::ptrdiff_t>(match.start_line),
              lines.begin() + static_cast<std::ptrdiff_t>(match.end_line + 1));
  lines.insert(lines.begin() + static_cast<std::ptrdiff_t>(match.start_line),
               replacement_lines.begin(), replacement_lines.end());

  return WriteFileWithBackup(path, JoinLines(lines), action, backup_path, error);
}

bool RemoveExistingResource(const ResourceSummary& summary,
                            std::string& action,
                            std::string& backup_path,
                            std::string& error)
{
  const auto path = std::filesystem::path(summary.file_path);
  const auto block_name = ResourceBlockNameForType(summary.type);
  if (block_name.empty() || !std::filesystem::exists(path)) {
    return DeleteFileWithBackup(path, action, backup_path, error);
  }

  const auto original_content = ReadFileToString(path);
  const auto block_ranges = ParseNamedResourceBlockRanges(path, block_name);
  std::vector<NamedResourceBlockRange> matches;
  for (const auto& block : block_ranges) {
    if (block.name == summary.name) matches.push_back(block);
  }
  if (matches.empty()) {
    error = "Resource block could not be located in " + summary.file_path + ".";
    return false;
  }
  if (matches.size() > 1) {
    error = "Resource block name is ambiguous in " + summary.file_path + ".";
    return false;
  }

  auto lines = SplitLines(original_content);
  const auto& match = matches.front();
  lines.erase(lines.begin() + static_cast<std::ptrdiff_t>(match.start_line),
              lines.begin() + static_cast<std::ptrdiff_t>(match.end_line + 1));

  bool has_content = false;
  for (const auto& line : lines) {
    if (!TrimWhitespace(line).empty()) {
      has_content = true;
      break;
    }
  }

  if (!has_content) {
    return DeleteFileWithBackup(path, action, backup_path, error);
  }

  return WriteFileWithBackup(path, JoinLines(lines), action, backup_path, error);
}

std::vector<RenameImpact> CollectRenameImpacts(const ResourceDetail& detail,
                                               const std::string& directive_key,
                                               const std::string& old_name,
                                               const std::string& new_name)
{
  std::vector<RenameImpact> impacts;
  const auto normalized_key = NormalizeDirectiveNameForComparison(directive_key);
  for (const auto& directive : detail.directives) {
    if (!IsTopLevelDirective(directive)) continue;
    if (NormalizeDirectiveNameForComparison(directive.key) != normalized_key) {
      continue;
    }
    if (StripSurroundingQuotes(directive.value) != old_name) continue;
    impacts.push_back(
        {detail.summary, directive.key, directive.line, directive.value,
         PreserveReferenceQuoting(directive.value, new_name)});
  }
  return impacts;
}

std::string SummarizeRenameImpacts(const std::vector<RenameImpact>& rename_impacts);

std::vector<ResourceSummary> FindPasswordPropagationTargets(
    const DatacenterSummary& summary, const ResourceDetail& detail)
{
  std::vector<ResourceSummary> targets;
  std::set<std::string> seen;
  const auto add_target = [&](const ResourceSummary& resource) {
    if (seen.insert(resource.id).second) targets.push_back(resource);
  };

  if (detail.summary.type == "client" || detail.summary.type == "storage") {
    const auto* director = FindOwningDirectorForResource(summary, detail.summary.id);
    if (!director) return targets;
    const auto relationships = FindRelationshipsForNode(summary, director->id);
    for (const auto& relationship : relationships) {
      if (relationship.source_resource_id != detail.summary.id || !relationship.resolved
          || relationship.to_node_id.empty()) {
        continue;
      }
      const auto* daemon = FindDaemonById(summary, relationship.to_node_id);
      if (!daemon) continue;
      for (const auto& resource : daemon->resources) {
        if (resource.type != "director") continue;
        if (StripSurroundingQuotes(resource.name) != director->name) continue;
        add_target(resource);
      }
    }
    return targets;
  }

  if (detail.summary.type != "director") return targets;

  const DaemonSummary* daemon = nullptr;
  const auto* director
      = FindOwningDirectorForDaemonResource(summary, detail.summary.id, &daemon);
  if (!director || !daemon) return targets;

  const auto relationships = FindRelationshipsForNode(summary, daemon->id);
  for (const auto& relationship : relationships) {
    if (relationship.to_node_id != daemon->id || !relationship.resolved) continue;
    if (StripSurroundingQuotes(detail.summary.name)
        != StripSurroundingQuotes(relationship.from_label)) {
      continue;
    }
    const auto* resource = FindResourceById(summary, relationship.source_resource_id);
    if (!resource) continue;
    if (resource->type != "client" && resource->type != "storage") continue;
    add_target(*resource);
  }

  return targets;
}

std::vector<ResourceSummary> FindDeviceRenameTargets(
    const DatacenterSummary& summary, const ResourceDetail& detail)
{
  std::vector<ResourceSummary> targets;
  if (detail.summary.type != "device") return targets;

  const DaemonSummary* daemon = nullptr;
  const auto* director
      = FindOwningDirectorForDaemonResource(summary, detail.summary.id, &daemon);
  if (!director || !daemon || daemon->kind != "storage-daemon") return targets;

  std::set<std::string> seen;
  const auto relationships = FindRelationshipsForNode(summary, daemon->id);
  for (const auto& relationship : relationships) {
    if (relationship.to_node_id != daemon->id || !relationship.resolved
        || relationship.relation != "storage") {
      continue;
    }
    const auto* resource = FindResourceById(summary, relationship.source_resource_id);
    if (!resource || resource->type != "storage") continue;
    if (seen.insert(resource->id).second) targets.push_back(*resource);
  }

  return targets;
}

std::vector<ResourceSummary> FindReverseDeviceRenameTargets(
    const DatacenterSummary& summary,
    const ResourceDetail& detail,
    const std::string& device_name)
{
  std::vector<ResourceSummary> targets;
  if (detail.summary.type != "storage" || device_name.empty()) return targets;

  const auto* director = FindOwningDirectorForResource(summary, detail.summary.id);
  if (!director) return targets;

  std::set<std::string> seen;
  const auto relationships = FindRelationshipsForNode(summary, director->id);
  for (const auto& relationship : relationships) {
    if (relationship.source_resource_id != detail.summary.id || !relationship.resolved
        || relationship.relation != "storage" || relationship.to_node_id.empty()) {
      continue;
    }

    const auto* daemon = FindDaemonById(summary, relationship.to_node_id);
    if (!daemon || daemon->kind != "storage-daemon") continue;

    const auto device_resource = std::find_if(
        daemon->resources.begin(), daemon->resources.end(),
        [&device_name](const ResourceSummary& candidate) {
          return candidate.type == "device"
                 && StripSurroundingQuotes(candidate.name) == device_name;
        });
    if (device_resource == daemon->resources.end()) continue;
    if (seen.insert(device_resource->id).second) targets.push_back(*device_resource);
  }

  return targets;
}

std::vector<ResourceSummary> FindDirectorRenameTargets(
    const DatacenterSummary& summary,
    const ResourceDetail& detail,
    const std::string& director_name)
{
  std::vector<ResourceSummary> targets;
  if (detail.summary.type != "director" || director_name.empty()) return targets;

  std::set<std::string> seen;
  const auto add_target = [&](const ResourceSummary& resource) {
    if (resource.id == detail.summary.id) return;
    if (resource.type != "director") return;
    if (StripSurroundingQuotes(resource.name) != director_name) return;
    if (seen.insert(resource.id).second) targets.push_back(resource);
  };

  if (const auto* director
      = FindOwningDirectorForResource(summary, detail.summary.id)) {
    for (const auto& resource : director->resources) add_target(resource);
    for (const auto& daemon : director->daemons) {
      for (const auto& resource : daemon.resources) add_target(resource);
    }
    return targets;
  }

  const DaemonSummary* daemon = nullptr;
  const auto* director
      = FindOwningDirectorForDaemonResource(summary, detail.summary.id, &daemon);
  if (!director || !daemon) return targets;

  for (const auto& resource : director->resources) add_target(resource);
  for (const auto& candidate_daemon : director->daemons) {
    for (const auto& resource : candidate_daemon.resources) add_target(resource);
  }

  return targets;
}

RenameAwarePreview BuildRenameAwarePreview(const ResourceDetail& detail,
                                           const std::string& updated_content)
{
  auto result = RenameAwarePreview{BuildResourceEditPreview(detail, updated_content),
                                   {}, {}};
  const auto old_name = ExtractResourceName(detail.summary, detail.directives);
  const auto new_name
      = ExtractResourceName(result.preview.summary, result.preview.updated_directives);
  const bool name_changed
      = !old_name.empty() && !new_name.empty() && old_name != new_name;

  const auto config_root
      = FindConfigRootForPath(std::filesystem::path(detail.summary.file_path));
  if (config_root.empty()) return result;

  const auto model = DiscoverDatacenterSummary({config_root});
  if (name_changed) {
    for (const auto& resource : CollectAllResources(model)) {
      if (resource.id == detail.summary.id) continue;

      auto related_detail = LoadResourceDetail(resource);
      auto updated_field_hints = related_detail.field_hints;
      std::vector<RenameImpact> resource_impacts;
      bool changed = false;

      for (auto& field : updated_field_hints) {
        if (!field.present || field.related_resource_type != detail.summary.type) {
          continue;
        }

        if (field.repeatable) {
          auto values = SplitRepeatableFieldValue(field.value);
          bool field_changed = false;
          for (auto& value : values) {
            if (StripSurroundingQuotes(value) != old_name) continue;
            value = PreserveReferenceQuoting(value, new_name);
            field_changed = true;
            changed = true;
          }
          if (!field_changed) continue;
          field.value = JoinRepeatableFieldValues(values);
          field.present = true;
          auto impacts = CollectRenameImpacts(related_detail, field.key, old_name,
                                              new_name);
          resource_impacts.insert(resource_impacts.end(), impacts.begin(),
                                  impacts.end());
          continue;
        }

        if (StripSurroundingQuotes(field.value) != old_name) continue;
        field.value = PreserveReferenceQuoting(field.value, new_name);
        field.present = true;
        changed = true;
        auto impacts
            = CollectRenameImpacts(related_detail, field.key, old_name, new_name);
        resource_impacts.insert(resource_impacts.end(), impacts.begin(),
                                impacts.end());
      }

      if (!changed) continue;
      auto preview = BuildFieldHintEditPreview(related_detail, updated_field_hints);
      result.rename_impacts.insert(result.rename_impacts.end(),
                                   resource_impacts.begin(),
                                   resource_impacts.end());
      result.affected_updates.push_back(
          {std::move(related_detail), std::move(preview), std::move(resource_impacts)});
    }

    for (const auto& target : FindDeviceRenameTargets(model, detail)) {
      const auto existing_update = std::find_if(
          result.affected_updates.begin(), result.affected_updates.end(),
          [&target](const RenameAffectedUpdate& update) {
            return update.detail.summary.id == target.id;
          });

      ResourceDetail related_detail;
      std::vector<ResourceFieldHint> updated_field_hints;
      std::vector<RenameImpact> existing_impacts;
      if (existing_update != result.affected_updates.end()) {
        related_detail = existing_update->detail;
        updated_field_hints = existing_update->preview.updated_field_hints;
        existing_impacts = existing_update->impacts;
      } else {
        related_detail = LoadResourceDetail(target);
        updated_field_hints = related_detail.field_hints;
      }

      bool changed = false;
      for (auto& field : updated_field_hints) {
        if (!field.present || NormalizeDirectiveNameForComparison(field.key) != "device") {
          continue;
        }

        if (field.repeatable) {
          auto values = SplitRepeatableFieldValue(field.value);
          bool field_changed = false;
          for (auto& value : values) {
            if (StripSurroundingQuotes(value) != old_name) continue;
            value = PreserveReferenceQuoting(value, new_name);
            field_changed = true;
            changed = true;
          }
          if (!field_changed) continue;
          field.value = JoinRepeatableFieldValues(values);
          field.present = true;
        } else {
          if (StripSurroundingQuotes(field.value) != old_name) continue;
          field.value = PreserveReferenceQuoting(field.value, new_name);
          field.present = true;
          changed = true;
        }

        auto impacts = CollectRenameImpacts(related_detail, field.key, old_name,
                                            new_name);
        existing_impacts.insert(existing_impacts.end(), impacts.begin(),
                                impacts.end());
        result.rename_impacts.insert(result.rename_impacts.end(), impacts.begin(),
                                     impacts.end());
      }

      if (!changed) continue;
      auto preview = BuildFieldHintEditPreview(related_detail, updated_field_hints);
      if (existing_update != result.affected_updates.end()) {
        existing_update->preview = std::move(preview);
        existing_update->impacts = std::move(existing_impacts);
      } else {
        result.affected_updates.push_back(
            {std::move(related_detail), std::move(preview), std::move(existing_impacts)});
      }
    }

    for (const auto& target : FindDirectorRenameTargets(model, detail, old_name)) {
      const auto existing_update = std::find_if(
          result.affected_updates.begin(), result.affected_updates.end(),
          [&target](const RenameAffectedUpdate& update) {
            return update.detail.summary.id == target.id;
          });

      ResourceDetail related_detail;
      std::vector<ResourceFieldHint> updated_field_hints;
      std::vector<RenameImpact> existing_impacts;
      if (existing_update != result.affected_updates.end()) {
        related_detail = existing_update->detail;
        updated_field_hints = existing_update->preview.updated_field_hints;
        existing_impacts = existing_update->impacts;
      } else {
        related_detail = LoadResourceDetail(target);
        updated_field_hints = related_detail.field_hints;
      }

      auto name_field = std::find_if(
          updated_field_hints.begin(), updated_field_hints.end(),
          [](const ResourceFieldHint& field) {
            return NormalizeDirectiveNameForComparison(field.key) == "name";
          });
      if (name_field == updated_field_hints.end() || !name_field->present
          || StripSurroundingQuotes(name_field->value) != old_name) {
        continue;
      }

      name_field->value = PreserveReferenceQuoting(name_field->value, new_name);
      name_field->present = true;

      auto impacts = CollectRenameImpacts(related_detail, "Name", old_name,
                                          new_name);
      existing_impacts.insert(existing_impacts.end(), impacts.begin(), impacts.end());
      result.rename_impacts.insert(result.rename_impacts.end(), impacts.begin(),
                                   impacts.end());

      auto preview = BuildFieldHintEditPreview(related_detail, updated_field_hints);
      if (existing_update != result.affected_updates.end()) {
        existing_update->preview = std::move(preview);
        existing_update->impacts = std::move(existing_impacts);
      } else {
        result.affected_updates.push_back(
            {std::move(related_detail), std::move(preview), std::move(existing_impacts)});
      }
    }
  }

  const auto old_device = StripSurroundingQuotes(
      ExtractDirectiveValue(detail.directives, "Device"));
  const auto new_device = StripSurroundingQuotes(
      ExtractDirectiveValue(result.preview.updated_directives, "Device"));
  if (detail.summary.type == "storage" && !old_device.empty() && !new_device.empty()
      && old_device != new_device) {
    for (const auto& target : FindReverseDeviceRenameTargets(model, detail, old_device)) {
      const auto existing_update = std::find_if(
          result.affected_updates.begin(), result.affected_updates.end(),
          [&target](const RenameAffectedUpdate& update) {
            return update.detail.summary.id == target.id;
          });

      ResourceDetail related_detail;
      std::vector<ResourceFieldHint> updated_field_hints;
      std::vector<RenameImpact> existing_impacts;
      if (existing_update != result.affected_updates.end()) {
        related_detail = existing_update->detail;
        updated_field_hints = existing_update->preview.updated_field_hints;
        existing_impacts = existing_update->impacts;
      } else {
        related_detail = LoadResourceDetail(target);
        updated_field_hints = related_detail.field_hints;
      }

      auto name_field = std::find_if(
          updated_field_hints.begin(), updated_field_hints.end(),
          [](const ResourceFieldHint& field) {
            return NormalizeDirectiveNameForComparison(field.key) == "name";
          });
      if (name_field == updated_field_hints.end() || !name_field->present
          || StripSurroundingQuotes(name_field->value) != old_device) {
        continue;
      }

      name_field->value = PreserveReferenceQuoting(name_field->value, new_device);
      name_field->present = true;

      auto impacts = CollectRenameImpacts(related_detail, "Name", old_device,
                                          new_device);
      existing_impacts.insert(existing_impacts.end(), impacts.begin(), impacts.end());
      result.rename_impacts.insert(result.rename_impacts.end(), impacts.begin(),
                                   impacts.end());

      auto preview = BuildFieldHintEditPreview(related_detail, updated_field_hints);
      if (existing_update != result.affected_updates.end()) {
        existing_update->preview = std::move(preview);
        existing_update->impacts = std::move(existing_impacts);
      } else {
        result.affected_updates.push_back(
            {std::move(related_detail), std::move(preview), std::move(existing_impacts)});
      }
    }
  }

  std::vector<RenameImpact> name_impacts;
  for (const auto& impact : result.rename_impacts) {
    if (NormalizeDirectiveNameForComparison(impact.directive_key) == "password") {
      continue;
    }
    name_impacts.push_back(impact);
  }
  if (!name_impacts.empty()) {
    std::set<std::string> resource_ids;
    for (const auto& impact : name_impacts) {
      resource_ids.insert(impact.resource.id);
    }
    auto warning = ValidationMessage{};
    warning.level = "warning";
    warning.code = "rename-reference-update";
    warning.message = "Renaming this resource will also update "
                      + std::to_string(name_impacts.size())
                      + " reference(s) in "
                      + std::to_string(resource_ids.size())
                      + " other resource(s): "
                      + SummarizeRenameImpacts(name_impacts) + ".";
    warning.line = FindDirectiveLine(result.preview.updated_directives, "Name");
    result.preview.updated_validation_messages.push_back(std::move(warning));
  }

  const auto old_password = StripSurroundingQuotes(
      ExtractDirectiveValue(detail.directives, "Password"));
  const auto new_password = StripSurroundingQuotes(
      ExtractDirectiveValue(result.preview.updated_directives, "Password"));
  if (!old_password.empty() && !new_password.empty() && old_password != new_password) {
    for (const auto& target : FindPasswordPropagationTargets(model, detail)) {
      const auto existing_update = std::find_if(
          result.affected_updates.begin(), result.affected_updates.end(),
          [&target](const RenameAffectedUpdate& update) {
            return update.detail.summary.id == target.id;
          });

      ResourceDetail related_detail;
      std::vector<ResourceFieldHint> updated_field_hints;
      std::vector<RenameImpact> existing_impacts;
      if (existing_update != result.affected_updates.end()) {
        related_detail = existing_update->detail;
        updated_field_hints = existing_update->preview.updated_field_hints;
        existing_impacts = existing_update->impacts;
      } else {
        related_detail = LoadResourceDetail(target);
        updated_field_hints = related_detail.field_hints;
      }

      bool changed = false;
      auto password_field = std::find_if(
          updated_field_hints.begin(), updated_field_hints.end(),
          [](const ResourceFieldHint& field) { return field.key == "Password"; });
      if (password_field == updated_field_hints.end() || !password_field->present
          || StripSurroundingQuotes(password_field->value) != old_password) {
        continue;
      }

      password_field->value
          = PreserveReferenceQuoting(password_field->value, new_password);
      password_field->present = true;
      changed = true;
      auto impacts = CollectRenameImpacts(related_detail, "Password", old_password,
                                          new_password);
      existing_impacts.insert(existing_impacts.end(), impacts.begin(), impacts.end());
      result.rename_impacts.insert(result.rename_impacts.end(), impacts.begin(),
                                   impacts.end());

      if (!changed) continue;
      auto preview = BuildFieldHintEditPreview(related_detail, updated_field_hints);
      if (existing_update != result.affected_updates.end()) {
        existing_update->preview = std::move(preview);
        existing_update->impacts = std::move(existing_impacts);
      } else {
        result.affected_updates.push_back(
            {std::move(related_detail), std::move(preview), std::move(existing_impacts)});
      }
    }

    if (!result.rename_impacts.empty()) {
      std::set<std::string> password_resource_ids;
      std::vector<RenameImpact> password_impacts;
      for (const auto& impact : result.rename_impacts) {
        if (NormalizeDirectiveNameForComparison(impact.directive_key) != "password") {
          continue;
        }
        password_resource_ids.insert(impact.resource.id);
        password_impacts.push_back(impact);
      }
      if (!password_impacts.empty()) {
        auto warning = ValidationMessage{};
        warning.level = "warning";
        warning.code = "password-reference-update";
        warning.message = "Changing this password will also update "
                          + std::to_string(password_impacts.size())
                          + " password reference(s) in "
                          + std::to_string(password_resource_ids.size())
                          + " other resource(s): "
                          + SummarizeRenameImpacts(password_impacts) + ".";
        warning.line = FindDirectiveLine(result.preview.updated_directives, "Password");
        result.preview.updated_validation_messages.push_back(std::move(warning));
      }
    }
  }

  return result;
}

std::string SerializeRenameImpacts(const std::vector<RenameImpact>& rename_impacts)
{
  std::ostringstream output;
  output << "[";
  for (size_t i = 0; i < rename_impacts.size(); ++i) {
    if (i > 0) output << ",";
    const auto& impact = rename_impacts[i];
    output << "{"
           << "\"resource_id\":" << JsonString(impact.resource.id) << ","
           << "\"resource_type\":" << JsonString(impact.resource.type) << ","
           << "\"resource_name\":" << JsonString(impact.resource.name) << ","
           << "\"file_path\":" << JsonString(impact.resource.file_path) << ","
           << "\"directive_key\":" << JsonString(impact.directive_key) << ","
           << "\"line\":" << impact.line << ","
           << "\"old_value\":" << JsonString(impact.old_value) << ","
           << "\"new_value\":" << JsonString(impact.new_value) << "}";
  }
  output << "]";
  return output.str();
}

std::string SummarizeRenameImpacts(const std::vector<RenameImpact>& rename_impacts)
{
  std::vector<std::string> descriptions;
  std::set<std::string> seen;
  for (const auto& impact : rename_impacts) {
    auto description = impact.directive_key + " in " + impact.resource.type + " "
                       + StripSurroundingQuotes(impact.resource.name);
    if (impact.line > 0) {
      description += " (line " + std::to_string(impact.line) + ")";
    }
    if (!impact.resource.file_path.empty()) {
      description += " in " + impact.resource.file_path;
    }
    if (!seen.insert(description).second) continue;
    descriptions.push_back(std::move(description));
  }

  std::ostringstream output;
  for (size_t i = 0; i < descriptions.size(); ++i) {
    if (i > 0) output << "; ";
    output << descriptions[i];
  }
  return output.str();
}

std::string SerializeRenameAwarePreview(const RenameAwarePreview& preview)
{
  auto body = SerializeResourceEditPreview(preview.preview);
  if (body.empty() || body.back() != '}') return body;
  body.pop_back();
  body += ",\"rename_impacts\":" + SerializeRenameImpacts(preview.rename_impacts)
          + "}";
  return body;
}

std::string SerializePersistedAffectedUpdates(
    const std::vector<PersistedAffectedUpdate>& affected_updates)
{
  std::ostringstream output;
  output << "[";
  for (size_t i = 0; i < affected_updates.size(); ++i) {
    if (i > 0) output << ",";
    const auto& update = affected_updates[i];
    output << "{"
           << "\"resource_id\":" << JsonString(update.resource.id) << ","
           << "\"resource_type\":" << JsonString(update.resource.type) << ","
           << "\"resource_name\":" << JsonString(update.resource.name) << ","
           << "\"path\":" << JsonString(update.path) << ","
           << "\"action\":" << JsonString(update.action) << ","
           << "\"backup_path\":" << JsonString(update.backup_path) << "}";
  }
  output << "]";
  return output.str();
}

std::string SerializePersistResult(const PersistResult& result)
{
  std::ostringstream output;
  output << "{"
         << "\"saved\":" << (result.saved ? "true" : "false") << ","
         << "\"action\":" << JsonString(result.action) << ","
         << "\"path\":" << JsonString(result.preview.summary.file_path) << ","
         << "\"backup_path\":" << JsonString(result.backup_path) << ","
         << "\"error\":" << JsonString(result.error) << ","
         << "\"verification_command\":"
         << JsonString(result.verification_command) << ","
         << "\"verification_output\":"
         << JsonString(result.verification_output) << ","
         << "\"follow_up_component\":"
         << JsonString(result.follow_up_component) << ","
         << "\"follow_up_action\":" << JsonString(result.follow_up_action) << ","
         << "\"follow_up_command\":"
         << JsonString(result.follow_up_command) << ","
         << "\"follow_up_message\":"
         << JsonString(result.follow_up_message) << ","
         << "\"affected_updates\":"
         << SerializePersistedAffectedUpdates(result.affected_updates) << ","
         << "\"preview\":";
  output << SerializeRenameAwarePreview(
      {result.preview, result.rename_impacts, {}});
  output << "}";
  return output.str();
}

void PopulateFollowUpAction(PersistResult& result);
void RollbackPersistedResource(const PersistResult& result);
VerificationResult VerifySavedConfig(const ConfigServiceOptions& options,
                                     const std::string& file_path);

PersistResult PersistResourceDetail(const ConfigServiceOptions& options,
                                    const ResourceDetail& detail,
                                    const std::string& updated_content,
                                    bool new_resource)
{
  auto result = PersistResult{};
  const auto rename_preview = BuildRenameAwarePreview(detail, updated_content);
  result.preview = rename_preview.preview;
  result.rename_impacts = rename_preview.rename_impacts;
  if (HasValidationErrors(result.preview.updated_validation_messages)) {
    result.status_code = 409;
    result.error = "Validation errors prevent saving this resource.";
    return result;
  }

  for (const auto& affected_update : rename_preview.affected_updates) {
    if (HasValidationErrors(affected_update.preview.updated_validation_messages)) {
      result.status_code = 409;
      result.error = "Validation errors prevent updating referenced resources.";
      return result;
    }
  }

  struct PendingWrite {
    ResourceSummary summary;
    std::string content;
    bool new_resource = false;
    std::string action;
    std::string backup_path;
  };

  std::vector<PendingWrite> writes;
  writes.push_back(
      {detail.summary, result.preview.updated_content, new_resource, "", ""});
  for (const auto& affected_update : rename_preview.affected_updates) {
    writes.push_back({affected_update.detail.summary,
                      affected_update.preview.updated_content, false, "", ""});
  }

  for (auto& write : writes) {
    std::string error;
    const auto saved
        = write.new_resource
              ? WriteFileWithBackup(write.summary.file_path, write.content,
                                    write.action, write.backup_path, error)
              : SaveExistingResourceContent(write.summary, write.content, write.action,
                                            write.backup_path, error);
    if (!saved) {
      for (auto written = writes.rbegin(); written != writes.rend(); ++written) {
        if (written->action.empty() || written->action == "unchanged") continue;
        PersistResult rollback_result;
        rollback_result.saved = true;
        rollback_result.action = written->action;
        rollback_result.backup_path = written->backup_path;
        rollback_result.preview.summary = written->summary;
        RollbackPersistedResource(rollback_result);
      }
      result.status_code = 409;
      result.error = error;
      return result;
    }
  }

  result.saved = true;
  result.action = writes.front().action;
  result.backup_path = writes.front().backup_path;
  for (size_t i = 1; i < writes.size(); ++i) {
    result.affected_updates.push_back(
        {writes[i].summary, writes[i].action, writes[i].summary.file_path,
         writes[i].backup_path});
  }

  std::set<std::string> verified_targets;
  for (const auto& write : writes) {
    if (IsGeneratedRemotePath(write.summary.file_path)) continue;
    const auto component = DetectComponentForPath(write.summary.file_path);
    if (component == DaemonComponent::kUnknown) continue;
    const auto config_root = FindConfigRootForPath(write.summary.file_path);
    if (config_root.empty()) continue;
    const auto verification_key
        = std::to_string(static_cast<int>(component)) + ":" + config_root.string();
    if (!verified_targets.insert(verification_key).second) continue;

    const auto verification = VerifySavedConfig(options, write.summary.file_path);
    if (!result.verification_command.empty() && !verification.command.empty()) {
      result.verification_command += "\n";
    }
    if (!result.verification_output.empty() && !verification.output.empty()) {
      result.verification_output += "\n";
    }
    result.verification_command += verification.command;
    result.verification_output += verification.output;
    if (!verification.success) {
      for (auto written = writes.rbegin(); written != writes.rend(); ++written) {
        if (written->action.empty() || written->action == "unchanged") continue;
        PersistResult rollback_result;
        rollback_result.saved = true;
        rollback_result.action = written->action;
        rollback_result.backup_path = written->backup_path;
        rollback_result.preview.summary = written->summary;
        RollbackPersistedResource(rollback_result);
      }
      result.saved = false;
      result.status_code = 409;
      result.error = verification.error.empty()
                         ? "Daemon configuration verification failed."
                         : verification.error;
      return result;
    }
  }
  PopulateFollowUpAction(result);
  return result;
}

bool IsManagedLocalPath(const std::string& path) { return !IsGeneratedRemotePath(path); }

void PopulateFollowUpAction(PersistResult& result)
{
  if (!result.saved) return;

  const auto& path = result.preview.summary.file_path;
  if (!IsManagedLocalPath(path)) return;

  const bool default_root = path.rfind("/etc/bareos/", 0) == 0;
  auto set_follow_up = [&](std::string component, std::string action,
                           std::string command, std::string message) {
    result.follow_up_component = std::move(component);
    result.follow_up_action = std::move(action);
    result.follow_up_command = std::move(command);
    result.follow_up_message = std::move(message);
  };

  const auto is_component_path = [&](const std::string& stem) {
    return path.find("/" + stem + ".conf") != std::string::npos
           || path.find("/" + stem + ".d/") != std::string::npos;
  };

  if (is_component_path("bareos-fd")) {
    set_follow_up(
        "file-daemon", "restart",
        default_root ? "systemctl restart bareos-filedaemon" : "",
        default_root
            ? "Restart the Bareos File Daemon to apply these changes."
            : "Restart the matching Bareos File Daemon instance for this non-default config root.");
    return;
  }
  if (is_component_path("bareos-sd")) {
    set_follow_up(
        "storage-daemon", "restart",
        default_root ? "systemctl restart bareos-storage" : "",
        default_root
            ? "Restart the Bareos Storage Daemon to apply these changes."
            : "Restart the matching Bareos Storage Daemon instance for this non-default config root.");
    return;
  }
  if (is_component_path("bareos-dir")) {
    set_follow_up(
        "director", "reload",
        default_root ? "systemctl reload bareos-director" : "",
        default_root
            ? "Reload the Bareos Director to apply these changes."
            : "Reload the matching Bareos Director instance for this non-default config root.");
  }
}

VerificationResult VerifySavedConfig(const ConfigServiceOptions& options,
                                     const std::string& file_path)
{
  const auto component = DetectComponentForPath(file_path);
  if (component == DaemonComponent::kUnknown) {
    return {true, "", "", ""};
  }

  const auto config_root = FindConfigRootForPath(file_path);
  if (config_root.empty()) {
    return {false, "", "",
            "Could not determine config root for daemon verification."};
  }

  const auto binary = VerificationBinaryForComponent(options, component);
  if (binary.empty()) {
    return {false, "", "",
            "No daemon verification binary configured for this component."};
  }

  auto result = RunVerificationCommand(
      {binary, "-t", "-c", config_root.string()});
  if (!result.success) {
    const auto detail = result.output.empty() ? result.error : result.output;
    result.error = detail.empty()
                       ? "Daemon configuration verification failed."
                       : "Daemon configuration verification failed.\n" + detail;
  }
  return result;
}

void RollbackPersistedResource(const PersistResult& result)
{
  if (!result.saved || result.action == "unchanged") return;

  std::error_code ec;
  const auto path = std::filesystem::path(result.preview.summary.file_path);
  if (result.action == "create") {
    std::filesystem::remove(path, ec);
    return;
  }

  if ((result.action == "update" || result.action == "delete")
      && !result.backup_path.empty()) {
    std::filesystem::copy_file(result.backup_path, path,
                               std::filesystem::copy_options::overwrite_existing,
                               ec);
  }
}

std::string SerializeWizardPersistResult(const WizardPersistResult& result)
{
  std::ostringstream output;
  output << "{"
         << "\"saved\":" << (result.saved ? "true" : "false") << ","
         << "\"error\":" << JsonString(result.error) << ","
         << "\"results\":["
         << "{\"role\":\"director-client\",\"result\":"
         << SerializePersistResult(result.director_client) << "},"
         << "{\"role\":\"client-file-daemon-director\",\"result\":"
         << SerializePersistResult(result.client_file_daemon_director) << "}"
         << "]}";
  return output.str();
}

std::string SerializeRemoteClientRemovalResult(
    const RemoteClientRemovalResult& result)
{
  std::ostringstream output;
  output << "{"
         << "\"removed\":" << (result.removed ? "true" : "false") << ","
         << "\"error\":" << JsonString(result.error) << ","
         << "\"results\":["
         << "{\"role\":\"director-client\",\"result\":"
         << SerializePersistResult(result.client_resource) << "},"
         << "{\"role\":\"generated-client-config\",\"result\":"
         << SerializePersistResult(result.generated_config) << "}"
         << "]}";
  return output.str();
}

std::vector<ResourceFieldHint> ParseFieldHintUpdateBody(
    const std::string& body,
    const std::vector<ResourceFieldHint>& base_field_hints)
{
  auto field_hints = base_field_hints;
  std::map<std::string, size_t> field_index_by_key;
  for (size_t i = 0; i < field_hints.size(); ++i) {
    field_index_by_key[field_hints[i].key] = i;
  }

  std::istringstream input(body);
  std::string line;
  while (std::getline(input, line)) {
    auto trimmed = TrimWhitespace(line);
    if (trimmed.empty()) continue;

    const auto separator = trimmed.find('=');
    if (separator == std::string::npos) continue;

    const auto key = TrimWhitespace(trimmed.substr(0, separator));
    const auto value = TrimWhitespace(trimmed.substr(separator + 1));
    if (key.empty()) continue;

    const auto it = field_index_by_key.find(key);
    if (it != field_index_by_key.end()) {
      auto& field = field_hints[it->second];
      if (field.repeatable && !field.value.empty() && !value.empty()) {
        field.value += "\n" + value;
      } else {
        field.value = value;
      }
      field.present = !value.empty();
      continue;
    }

    field_hints.push_back(
        {key, key, false, false, false, !value.empty(), false, value, "", "", "",
         "", "", "", "", {}});
    field_index_by_key[key] = field_hints.size() - 1;
  }

  return field_hints;
}

std::map<std::string, std::string> ParseKeyValueBody(const std::string& body)
{
  std::map<std::string, std::string> values;
  std::istringstream input(body);
  std::string line;
  while (std::getline(input, line)) {
    auto trimmed = TrimWhitespace(line);
    if (trimmed.empty()) continue;

    const auto separator = trimmed.find('=');
    if (separator == std::string::npos) continue;

    const auto key = TrimWhitespace(trimmed.substr(0, separator));
    const auto value = TrimWhitespace(trimmed.substr(separator + 1));
    if (!key.empty()) values[key] = value;
  }
  return values;
}

ResourceDetail BuildAddClientDirectorPreview(
    const TreeNodeSummary& director_node,
    const std::map<std::string, std::string>& values);

ResourceDetail BuildAddClientDirectorTemplate(
    const TreeNodeSummary& director_node,
    const std::map<std::string, std::string>& values)
{
  std::string resource_name = "new-client";
  if (const auto name_it = values.find("Name");
      name_it != values.end() && !name_it->second.empty()) {
    resource_name = name_it->second;
  }

  const ResourceSummary summary{
      "wizard-add-client-preview", "client", resource_name,
      director_node.description + "/bareos-dir.d/client/" + resource_name + ".conf"};
  return BuildResourceDetail(summary, "Client {\n}\n");
}

std::vector<ResourceFieldHint> SelectAddClientWizardFieldHints(
    const ResourceDetail& detail)
{
  std::vector<ResourceFieldHint> field_hints;
  for (const auto& hint : detail.field_hints) {
    if (hint.deprecated) continue;
    if (hint.key == "Catalog" && hint.related_resource_type == "catalog"
        && hint.allowed_values.size() <= 1) {
      continue;
    }
    if (hint.key == "Name" || hint.required || !hint.related_resource_type.empty()) {
      field_hints.push_back(hint);
    }
  }

  const auto priority = [](const ResourceFieldHint& hint) {
    if (hint.key == "Name") return 0;
    if (hint.required) return 1;
    if (!hint.related_resource_type.empty()) return 2;
    return 3;
  };

  std::sort(field_hints.begin(), field_hints.end(),
            [&priority](const ResourceFieldHint& left,
                        const ResourceFieldHint& right) {
              const auto left_priority = priority(left);
              const auto right_priority = priority(right);
              if (left_priority != right_priority) {
                return left_priority < right_priority;
              }
              return left.key < right.key;
            });
  return field_hints;
}

std::vector<ResourceFieldHint> ApplyAddClientWizardValues(
    std::vector<ResourceFieldHint> field_hints,
    const std::map<std::string, std::string>& values)
{
  for (auto& field : field_hints) {
    const auto it = values.find(field.key);
    if (it == values.end()) continue;
    field.value = it->second;
    field.present = !it->second.empty();
  }
  return field_hints;
}

std::string SerializeAddClientWizardSchema(const TreeNodeSummary& director_node)
{
  const auto director_template = BuildAddClientDirectorTemplate(director_node, {});
  const auto field_hints = SelectAddClientWizardFieldHints(director_template);

  std::ostringstream output;
  output << "{"
         << "\"wizard\":\"add-client\","
         << "\"director_id\":" << JsonString(director_node.id) << ","
         << "\"director_name\":" << JsonString(director_node.label) << ","
         << "\"field_hints\":" << SerializeResourceFieldHints(field_hints)
         << "}";
  return output.str();
}

ResourceDetail BuildAddClientDirectorPreview(
    const TreeNodeSummary& director_node,
    const std::map<std::string, std::string>& values)
{
  std::string resource_name = "new-client";
  if (const auto name_it = values.find("Name");
      name_it != values.end() && !name_it->second.empty()) {
    resource_name = name_it->second;
  }

  const ResourceSummary summary{
      "wizard-add-client-preview",
      "client",
      resource_name,
      director_node.description + "/bareos-dir.d/client/" + resource_name + ".conf"};

  auto director_template = BuildAddClientDirectorTemplate(director_node, values);
  director_template.summary = summary;
  const auto field_hints = ApplyAddClientWizardValues(
      SelectAddClientWizardFieldHints(director_template), values);
  const auto preview = BuildFieldHintEditPreview(director_template, field_hints);

  return {preview.summary,
          preview.updated_content,
          preview.updated_directives,
          preview.updated_inherited_directives,
          preview.updated_validation_messages,
          preview.updated_field_hints};
}

std::string BuildClientSideDirectorPreviewContent(
    const TreeNodeSummary& director_node,
    const std::map<std::string, std::string>& values)
{
  std::ostringstream output;
  output << "Director {\n"
         << "  Name = " << director_node.label << "\n";
  if (const auto password_it = values.find("Password");
      password_it != values.end() && !password_it->second.empty()) {
    output << "  Password = " << password_it->second << "\n";
  }
  output << "}\n";
  return output.str();
}

AddClientTargetPlan BuildClientFileDaemonTargetPlan(
    const ConfigServiceOptions& options,
    const TreeNodeSummary& director_node,
    const std::string& client_name)
{
  const auto config_root = std::filesystem::path(director_node.description);
  const auto local_fd_conf = config_root / "bareos-fd.conf";
  const auto local_fd_dir = config_root / "bareos-fd.d";
  const auto local_target
      = local_fd_dir / "director" / (director_node.label + ".conf");
  bool use_local_target = false;

  if ((std::filesystem::exists(local_fd_conf) || std::filesystem::exists(local_fd_dir))
      && !client_name.empty()) {
    const auto model = DiscoverDatacenterSummary(options.config_roots);
    const auto* director = FindDirectorById(model, director_node.id);
    if (director) {
      const auto daemon_it = std::find_if(
          director->daemons.begin(), director->daemons.end(),
          [](const DaemonSummary& daemon) { return daemon.kind == "file-daemon"; });
      if (daemon_it != director->daemons.end()
          && StripSurroundingQuotes(daemon_it->configured_name) == client_name) {
        use_local_target = true;
      }
    }
  }

  if (use_local_target) {
    const bool target_exists = std::filesystem::exists(local_target);
    return {local_target.string(), target_exists ? "update" : "create",
            target_exists};
  }

  const auto staged_root
      = BuildGeneratedRemoteConfigRoot(options, "client", client_name);
  const auto staged_target
      = staged_root / "bareos-fd.d" / "director" / (director_node.label + ".conf");
  const bool target_exists = std::filesystem::exists(staged_target);
  return {staged_target.string(), target_exists ? "update" : "create",
          target_exists};
}

ResourceDetail BuildAddClientFileDaemonPreview(
    const ConfigServiceOptions& options,
    const TreeNodeSummary& director_node,
    const std::map<std::string, std::string>& values)
{
  const auto client_name_it = values.find("Name");
  const auto client_name
      = client_name_it != values.end() ? client_name_it->second : "";
  const auto target_plan = BuildClientFileDaemonTargetPlan(options, director_node,
                                                           client_name);

  const ResourceSummary summary{
      "wizard-add-client-file-daemon",
      "director",
      director_node.label,
      target_plan.file_path};
  return BuildResourceDetail(summary,
                             BuildClientSideDirectorPreviewContent(director_node,
                                                                    values));
}

std::string SerializeAddClientWizardPreview(
    const ConfigServiceOptions& options,
    const TreeNodeSummary& director_node,
    const std::map<std::string, std::string>& values)
{
  const auto director_preview = BuildAddClientDirectorPreview(director_node, values);
  const auto file_daemon_preview
      = BuildAddClientFileDaemonPreview(options, director_node, values);
  const auto file_daemon_target_plan
      = BuildClientFileDaemonTargetPlan(
          options,
          director_node, values.contains("Name") ? values.at("Name") : "");
  const bool director_file_exists
      = std::filesystem::exists(director_preview.summary.file_path);

  std::ostringstream output;
  output << "{"
         << "\"wizard\":\"add-client\","
         << "\"director_id\":" << JsonString(director_node.id) << ","
         << "\"director_name\":" << JsonString(director_node.label) << ","
         << "\"operations\":["
         << "{"
         << "\"role\":\"director-client\","
         << "\"target_scope\":\"director\","
         << "\"action\":"
         << JsonString(director_file_exists ? "update" : "create") << ","
         << "\"path\":" << JsonString(director_preview.summary.file_path) << ","
         << "\"exists_local\":" << (director_file_exists ? "true" : "false")
         << "},"
          << "{"
          << "\"role\":\"client-file-daemon-director\","
          << "\"target_scope\":\"client\","
          << "\"action\":"
          << JsonString(file_daemon_target_plan.action) << ","
          << "\"path\":" << JsonString(file_daemon_preview.summary.file_path) << ","
          << "\"exists_local\":"
          << (file_daemon_target_plan.exists_local ? "true" : "false")
          << "}],"
         << "\"resources\":["
         << "{\"role\":\"director-client\",\"resource\":"
         << SerializeEditableResourceDetail(director_preview) << "},"
         << "{\"role\":\"client-file-daemon-director\",\"resource\":"
         << SerializeEditableResourceDetail(file_daemon_preview) << "}"
         << "]}";
  return output.str();
}

WizardPersistResult PersistAddClientWizard(
    const ConfigServiceOptions& options,
    const TreeNodeSummary& director_node,
    const std::map<std::string, std::string>& values)
{
  auto result = WizardPersistResult{};
  const auto director_preview = BuildAddClientDirectorPreview(director_node, values);
  const auto file_daemon_preview
      = BuildAddClientFileDaemonPreview(options, director_node, values);

  result.director_client.preview
      = BuildResourceEditPreview(director_preview, director_preview.content);
  result.client_file_daemon_director.preview
      = BuildResourceEditPreview(file_daemon_preview, file_daemon_preview.content);

  result.director_client = PersistResourceDetail(
      options, director_preview, director_preview.content,
      !std::filesystem::exists(director_preview.summary.file_path));
  if (!result.director_client.saved) {
    result.status_code = result.director_client.status_code;
    result.error = result.director_client.error;
    return result;
  }

  result.client_file_daemon_director = PersistResourceDetail(
      options, file_daemon_preview, file_daemon_preview.content,
      !std::filesystem::exists(file_daemon_preview.summary.file_path));
  if (!result.client_file_daemon_director.saved) {
    RollbackPersistedResource(result.director_client);
    result.director_client.saved = false;
    result.director_client.error
        = "Rolled back after client-side save failed.";
    result.status_code = result.client_file_daemon_director.status_code;
    result.error = result.client_file_daemon_director.error;
    return result;
  }

  result.saved = true;
  return result;
}

std::string BuildIndexHtml()
{
  return R"HTML(<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Bareos Config</title>
  <style>
    body { font-family: system-ui, sans-serif; margin: 0; color: #1f2937; }
    header { padding: 12px 16px; background: #111827; color: white; }
    .layout { display: grid; grid-template-columns: 320px 360px 1fr; min-height: calc(100vh - 52px); }
    .pane { border-right: 1px solid #d1d5db; padding: 16px; overflow: auto; }
    .pane:last-child { border-right: 0; }
    .node { cursor: pointer; padding: 4px 0; }
    .children { padding-left: 18px; }
    .resource { padding: 8px 0; border-bottom: 1px solid #e5e7eb; cursor: pointer; }
    .resource small { color: #6b7280; display: block; }
    .muted { color: #6b7280; }
    textarea { width: 100%; min-height: 320px; box-sizing: border-box; font: 13px/1.4 ui-monospace, monospace; padding: 12px; border: 1px solid #d1d5db; border-radius: 6px; }
    button { margin-top: 12px; padding: 8px 12px; border: 1px solid #111827; background: #111827; color: white; border-radius: 6px; cursor: pointer; }
    button:disabled { opacity: 0.6; cursor: progress; }
    .link-button { margin-top: 0; padding: 0; border: 0; background: none; color: #1d4ed8; border-radius: 0; }
    .link-button:hover { text-decoration: underline; }
    .directive-list { margin: 12px 0; padding: 0; list-style: none; }
    .directive-list li { padding: 6px 0; border-bottom: 1px solid #e5e7eb; }
    .field-hint-list { display: grid; gap: 10px; margin: 12px 0; }
    .field-hint-row { display: grid; gap: 4px; }
    .field-hint-row input { width: 100%; box-sizing: border-box; padding: 8px; border: 1px solid #d1d5db; border-radius: 6px; }
    .validation-list { margin: 12px 0; padding: 0; list-style: none; }
    .validation-list li { padding: 6px 8px; border-radius: 6px; margin-bottom: 8px; }
     .validation-list li.warning { background: #fef3c7; color: #92400e; }
     .validation-list li.error { background: #fee2e2; color: #991b1b; }
     table { width: 100%; border-collapse: collapse; margin: 12px 0; }
     th, td { padding: 8px; border-bottom: 1px solid #e5e7eb; text-align: left; vertical-align: top; }
     pre { white-space: pre-wrap; word-break: break-word; background: #f3f4f6; padding: 12px; border-radius: 6px; }
   </style>
</head>
<body>
  <header>
    <strong>Bareos Config</strong> — standalone configuration service bootstrap
  </header>
  <div class="layout">
    <section class="pane">
      <h3>Datacenter</h3>
      <div id="tree" class="muted">Loading...</div>
    </section>
    <section class="pane">
      <h3>Resources</h3>
      <div id="resources" class="muted">Select a director or daemon.</div>
    </section>
    <section class="pane">
      <h3>Details</h3>
      <div id="details" class="muted">Select a resource to inspect its source path.</div>
    </section>
  </div>
  <script>
    const treeEl = document.getElementById('tree');
    const resourcesEl = document.getElementById('resources');
    const detailsEl = document.getElementById('details');

    function escapeHtml(text) {
      return text
        .replaceAll('&', '&amp;')
        .replaceAll('<', '&lt;')
        .replaceAll('>', '&gt;')
        .replaceAll('"', '&quot;');
    }

    function isEffectivelyEmptyDirectiveValue(value) {
      if (value === null || value === undefined) {
        return true;
      }
      const trimmed = `${value}`.trim();
      if (!trimmed) {
        return true;
      }
      return trimmed === '""' || trimmed === "''";
    }

    function detectSurroundingQuote(value) {
      if (value === null || value === undefined) {
        return '';
      }
      const text = `${value}`;
      if (text.length < 2) {
        return '';
      }
      const first = text[0];
      const last = text[text.length - 1];
      if ((first === '"' || first === "'") && first === last) {
        return first;
      }
      return '';
    }

    function stripSurroundingQuotes(value) {
      const quote = detectSurroundingQuote(value);
      if (!quote) {
        return value ?? '';
      }
      return `${value}`.slice(1, -1);
    }

    function restoreQuotedFieldValue(fieldEl) {
      const value = fieldEl.value || '';
      const quote = fieldEl.dataset.fieldQuoteChar || '';
      if (!quote || !value || detectSurroundingQuote(value)) {
        return value;
      }
      return `${quote}${value}${quote}`;
    }

    function splitRepeatableFieldValues(value) {
      return `${value || ''}`
        .split('\n')
        .map((entry) => entry.trim())
        .filter(Boolean);
    }

    function getRepeatableFieldValues(fieldEl) {
      if (fieldEl.tagName === 'SELECT' && fieldEl.multiple) {
        return Array.from(fieldEl.selectedOptions).map((option) => option.value);
      }
      return splitRepeatableFieldValues(fieldEl.value);
    }

    function renderDirectives(directives) {
      const visibleDirectives = (directives || []).filter(
        (directive) => !isEffectivelyEmptyDirectiveValue(directive.value),
      );
      if (!visibleDirectives.length) {
        return '<div class="muted">No non-empty directive fields parsed yet.</div>';
      }

      return `<ul class="directive-list">${visibleDirectives.map((directive) => `
        <li style="margin-left: ${Math.max((directive.nesting_level || 1) - 1, 0) * 16}px;"><strong>${escapeHtml(directive.key)}</strong> = ${escapeHtml(stripSurroundingQuotes(directive.value))}
          <small class="muted">line ${directive.line}${directive.nesting_level ? `, level ${directive.nesting_level}` : ''}</small></li>`).join('')}</ul>`;
    }

    function renderInheritedDirectives(inheritedDirectives) {
      const visibleDirectives = (inheritedDirectives || []).filter(
        (directive) => !isEffectivelyEmptyDirectiveValue(directive.value),
      );
      if (!visibleDirectives.length) {
        return '<div class="muted">No inherited directives.</div>';
      }

      return `<ul class="directive-list">${visibleDirectives.map((directive) => `
        <li style="margin-left: ${Math.max((directive.nesting_level || 1) - 1, 0) * 16}px;"><strong>${escapeHtml(directive.key)}</strong> = ${escapeHtml(stripSurroundingQuotes(directive.value))}
          <small class="muted">from ${escapeHtml(directive.source_resource_type)} ${escapeHtml(stripSurroundingQuotes(directive.source_resource_name || '-'))}${directive.line ? `, line ${directive.line}` : ''}${directive.nesting_level ? `, level ${directive.nesting_level}` : ''}</small></li>`).join('')}</ul>`;
    }

    function renderValidation(validationMessages) {
      if (!validationMessages || !validationMessages.length) {
        return '<div class="muted">No validation messages.</div>';
      }

      return `<ul class="validation-list">${validationMessages.map((validation) => `
        <li class="${escapeHtml(validation.level)}">
          <strong>${escapeHtml(validation.level.toUpperCase())}</strong>
          ${escapeHtml(validation.message)}
          <small class="muted">${escapeHtml(validation.code)}${validation.line ? `, line ${validation.line}` : ''}</small>
        </li>`).join('')}</ul>`;
    }

    function renderRenameImpacts(renameImpacts) {
      if (!renameImpacts || !renameImpacts.length) {
        return '<div class="muted">No cross-resource reference updates are needed.</div>';
      }

      return `<ul class="directive-list">${renameImpacts.map((impact) => `
        <li>
          <strong>${escapeHtml(impact.resource_type)} ${escapeHtml(stripSurroundingQuotes(impact.resource_name || '-'))}</strong>
          <small>${escapeHtml(impact.directive_key)}${impact.line ? `, line ${impact.line}` : ''}</small>
          <div class="muted">${escapeHtml(impact.file_path || '-')}</div>
          <div class="muted">${escapeHtml(stripSurroundingQuotes(impact.old_value || ''))} → ${escapeHtml(stripSurroundingQuotes(impact.new_value || ''))}</div>
        </li>`).join('')}</ul>`;
    }

    function renderWizardOperations(operations) {
      if (!operations || !operations.length) {
        return '<div class="muted">No planned operations.</div>';
      }

      return `<ul class="directive-list">${operations.map((operation) => `
        <li>
          <strong>${escapeHtml(operation.role)}</strong>
          <small>${escapeHtml(operation.action)} · ${escapeHtml(operation.target_scope)}</small>
          <div class="muted">${escapeHtml(operation.path || '(no direct file path)')}</div>
        </li>`).join('')}</ul>`;
    }

    function renderWizardSaveResults(results) {
      if (!results || !results.length) {
        return '<div class="muted">No save results.</div>';
      }

      return `<ul class="directive-list">${results.map((entry) => `
        <li>
          <strong>${escapeHtml(entry.role)}</strong>
          <small>${escapeHtml(entry.result.action || 'unchanged')}</small>
          <div class="muted">${escapeHtml(entry.result.path || '-')}</div>
           ${entry.result.backup_path ? `<div class="muted">backup: ${escapeHtml(entry.result.backup_path)}</div>` : ''}
           ${entry.result.verification_command ? `<div class="muted">verify: ${escapeHtml(entry.result.verification_command)}</div>` : ''}
           ${entry.result.error ? `<div class="muted">${escapeHtml(entry.result.error)}</div>` : ''}
           ${entry.result.verification_output ? `<pre>${escapeHtml(entry.result.verification_output)}</pre>` : ''}
         </li>`).join('')}</ul>`;
    }

    function renderFollowUpAction(result) {
      if (!result || !result.follow_up_component) {
        return '<div class="muted">No daemon follow-up required.</div>';
      }

      return `<div style="margin-top: 8px;">
        <strong>${escapeHtml(result.follow_up_component)}</strong>
        <small>${escapeHtml(result.follow_up_action)}</small>
        <div class="muted">${escapeHtml(result.follow_up_message || '')}</div>
        ${result.follow_up_command
          ? `<pre>${escapeHtml(result.follow_up_command)}</pre>`
          : ''}
      </div>`;
    }

    function renderWizardPreview(preview, datacenterNode, directorNode, relationships, wizardBody) {
      const resourcesHtml = (preview.resources || []).map((entry) => `
        <section style="margin-bottom: 24px;">
          <h4>${escapeHtml(entry.role)}</h4>
          <div class="muted">Target file: ${escapeHtml(entry.resource.file_path)}</div>
          <h5>Validation</h5>
          <div>${renderValidation(entry.resource.validation_messages)}</div>
          <h5>Generated config</h5>
          <pre>${escapeHtml(entry.resource.content)}</pre>
        </section>`).join('');

      detailsEl.innerHTML = `<strong>Add client preview</strong>
        <div class="muted">Director: ${escapeHtml(directorNode.label)}</div>
        <h4>Relationships</h4>
        <div>${renderRelationships(relationships)}</div>
        <h4>Planned operations</h4>
        <div>${renderWizardOperations(preview.operations)}</div>
        <div>
          <button id="save-add-client">Save generated config</button>
          <div id="save-add-client-status" class="muted" style="margin-top: 8px;"></div>
          <div id="save-add-client-results"></div>
        </div>
        ${resourcesHtml}
        <div><button id="back-to-wizard">Back to add client wizard</button></div>`;

      document.getElementById('save-add-client').addEventListener('click', () => {
        const saveButton = document.getElementById('save-add-client');
        const statusEl = document.getElementById('save-add-client-status');
        const resultsEl = document.getElementById('save-add-client-results');
        saveButton.disabled = true;
        statusEl.textContent = 'Saving generated config...';
        resultsEl.innerHTML = '';

        fetch(`/api/v1/nodes/${datacenterNode.id}/add-client/save/${directorNode.id}`, {
          method: 'POST',
          headers: { 'Content-Type': 'text/plain; charset=utf-8' },
          body: wizardBody,
        })
          .then(async (response) => ({
            ok: response.ok,
            payload: await response.json(),
          }))
          .then(({ ok, payload }) => {
            statusEl.textContent = payload.saved
              ? 'Saved generated config.'
              : (payload.error || (ok ? 'Save incomplete.' : 'Save blocked.'));
            resultsEl.innerHTML = renderWizardSaveResults(payload.results || [])
              + (payload.results || []).map((entry) => renderFollowUpAction(entry.result)).join('');
          })
          .catch((error) => {
            statusEl.textContent = `Failed to save generated config: ${error}`;
          })
          .finally(() => {
            saveButton.disabled = false;
          });
      });

      document.getElementById('back-to-wizard').addEventListener('click', () => {
        renderAddClientWizard(datacenterNode, directorNode.id);
      });
      attachRelationshipActions();
    }

    function renderRelationships(relationships) {
      if (!relationships || !relationships.length) {
        return '<div class="muted">No daemon relationships discovered for this node yet.</div>';
      }

        return `<table>
        <thead>
          <tr>
            <th>From</th>
            <th>To</th>
            <th>Relation</th>
            <th>Endpoint</th>
            <th>Resolution</th>
            <th>Source resource</th>
            <th>Target resource</th>
          </tr>
        </thead>
        <tbody>${relationships.map((relationship) => `
          <tr>
            <td>${relationship.from_node_id
                ? `<button class="link-button relationship-node" data-node-id="${escapeHtml(relationship.from_node_id)}">${escapeHtml(relationship.from_label)}</button>`
                : escapeHtml(relationship.from_label)}</td>
            <td>${relationship.to_node_id
                ? `<button class="link-button relationship-node" data-node-id="${escapeHtml(relationship.to_node_id)}">${escapeHtml(relationship.to_label)}</button>`
                : escapeHtml(relationship.to_label)}</td>
            <td>${escapeHtml(relationship.relation)}${relationship.resolved ? '' : ' (unresolved)'}</td>
            <td>${escapeHtml(relationship.endpoint_name || '-')}</td>
            <td><small>${escapeHtml(relationship.resolution || '-')}</small></td>
            <td>${relationship.source_resource_id
                ? `<button class="link-button relationship-resource" data-resource-id="${escapeHtml(relationship.source_resource_id)}"><small>${escapeHtml(relationship.source_resource_path || '-')}</small></button>`
                : `<small>${escapeHtml(relationship.source_resource_path || '-')}</small>`}</td>
            <td>${relationship.target_resource_id
                ? `<button class="link-button relationship-resource" data-resource-id="${escapeHtml(relationship.target_resource_id)}"><small>${escapeHtml(relationship.target_resource_path || '-')}</small></button>`
                : `<small>${escapeHtml(relationship.target_resource_path || '-')}</small>`}</td>
          </tr>`).join('')}</tbody>
      </table>`;
    }

    function attachRelationshipActions() {
      detailsEl.querySelectorAll('.relationship-node').forEach((buttonEl) => {
        buttonEl.addEventListener('click', () => {
          loadNodeById(buttonEl.dataset.nodeId);
        });
      });

      detailsEl.querySelectorAll('.relationship-resource').forEach((buttonEl) => {
        buttonEl.addEventListener('click', () => {
          showResourceDetails(buttonEl.dataset.resourceId);
        });
      });
    }

    function renderAddClientWizard(node, selectedDirectorId) {
      const directors = (node.children || []).filter((child) => child.kind === 'director');
      if (!directors.length) {
        detailsEl.innerHTML = `<strong>${escapeHtml(node.label)}</strong>
          <div class="muted">No directors discovered in this datacenter.</div>`;
        return;
      }

      const initialDirectorId = selectedDirectorId || directors[0].id;
      detailsEl.innerHTML = `<strong>${escapeHtml(node.label)}</strong>
        <div class="muted">Kind: ${escapeHtml(node.kind)}<br>Location: ${escapeHtml(node.description || '-')}</div>
        <h4>Add client wizard</h4>
        <label class="field-hint-row">
          <span><strong>Target director</strong></span>
          <select id="wizard-director">${directors.map((director) => `<option value="${escapeHtml(director.id)}"${director.id === initialDirectorId ? ' selected' : ''}>${escapeHtml(director.label)}</option>`).join('')}</select>
        </label>
        <h4>Relationships</h4>
        <div id="wizard-relationships"><div class="muted">Loading relationships...</div></div>
        <div id="wizard-fields" class="field-hint-list"><div class="muted">Loading directive metadata...</div></div>
        <div><button id="preview-add-client" disabled>Preview client config</button></div>
        <div id="wizard-status" class="muted" style="margin-top: 12px;"></div>`;

      const directorSelectEl = document.getElementById('wizard-director');
      const relationshipsEl = document.getElementById('wizard-relationships');
      const previewButton = document.getElementById('preview-add-client');
      const statusEl = document.getElementById('wizard-status');
      const wizardFieldsEl = document.getElementById('wizard-fields');
      let currentDirector = directors.find((director) => director.id === initialDirectorId) || directors[0];
      let currentRelationships = [];

      const loadWizardContext = () => {
        currentDirector = directors.find((director) => director.id === directorSelectEl.value) || directors[0];
        previewButton.disabled = true;
        relationshipsEl.innerHTML = '<div class="muted">Loading relationships...</div>';
        wizardFieldsEl.innerHTML = '<div class="muted">Loading directive metadata...</div>';
        statusEl.textContent = '';

        Promise.all([
          fetch(`/api/v1/nodes/${node.id}/add-client/schema/${currentDirector.id}`).then((response) => response.json()),
          fetch(`/api/v1/nodes/${currentDirector.id}/relationships`).then((response) => response.json()),
        ])
          .then(([schema, relationships]) => {
            currentRelationships = relationships;
            relationshipsEl.innerHTML = renderRelationships(relationships);
            wizardFieldsEl.innerHTML = renderFieldHints(schema.field_hints || []);
            previewButton.disabled = false;
            attachRelationshipActions();
          })
          .catch((error) => {
            relationshipsEl.innerHTML = '<div class="muted">No daemon relationships discovered for this node yet.</div>';
            wizardFieldsEl.innerHTML = '<div class="muted">No guided fields available for this wizard yet.</div>';
            statusEl.textContent = `Failed to load wizard metadata: ${error}`;
          });
      };

      directorSelectEl.addEventListener('change', loadWizardContext);
      previewButton.addEventListener('click', () => {
        previewButton.disabled = true;
        statusEl.textContent = 'Generating add-client preview...';
        const body = serializeFieldHints(wizardFieldsEl);

        fetch(`/api/v1/nodes/${node.id}/add-client/preview/${currentDirector.id}`, {
          method: 'POST',
          headers: { 'Content-Type': 'text/plain; charset=utf-8' },
          body,
        })
          .then((response) => response.json())
          .then((preview) => {
            renderWizardPreview(
              preview,
              node,
              currentDirector,
              currentRelationships,
              body,
            );
          })
          .catch((error) => {
            statusEl.textContent = `Failed to preview client: ${error}`;
          })
          .finally(() => {
            previewButton.disabled = false;
          });
      });
      loadWizardContext();
    }

    function renderFieldHints(fieldHints, options = {}) {
      const visibleFieldHints = (fieldHints || []).filter((field) => {
        if (!options.hideEmptyOptional) {
          return true;
        }
        if (field.required) {
          return true;
        }
        return field.present && !isEffectivelyEmptyDirectiveValue(field.value);
      });
      if (!visibleFieldHints.length) {
        return '<div class="muted">No guided fields available for this resource type yet.</div>';
      }

      const renderValueInput = (field, index) => {
        if (field.repeatable && field.allowed_values && field.allowed_values.length
            && field.datatype === 'RESOURCE_LIST') {
          const selectedValues = new Set(
            splitRepeatableFieldValues(field.present ? field.value : ''),
          );
          const optionValues = Array.from(
            new Set([
              ...field.allowed_values,
              ...Array.from(selectedValues),
            ]),
          );
          return `<select
              multiple
              size="${Math.min(Math.max(optionValues.length, 2), 8)}"
              data-field-key="${escapeHtml(field.key)}"
              data-field-required="${field.required ? 'true' : 'false'}"
              data-field-repeatable="true"
              data-field-quote-char="${escapeHtml(detectSurroundingQuote(field.value || ''))}"
            >${optionValues.map((value) => `<option value="${escapeHtml(value)}"${selectedValues.has(value) ? ' selected' : ''}>${escapeHtml(value)}</option>`).join('')}</select>`;
        }
        if (field.repeatable) {
          return `<textarea
              data-field-key="${escapeHtml(field.key)}"
              data-field-required="${field.required ? 'true' : 'false'}"
              data-field-repeatable="true"
              data-field-quote-char="${escapeHtml(detectSurroundingQuote(field.value || ''))}"
              placeholder="${escapeHtml(stripSurroundingQuotes((!field.present && field.inherited_value) ? field.inherited_value : ''))}"
              rows="3"
            >${escapeHtml(stripSurroundingQuotes(field.value || ''))}</textarea>`;
        }
        if (field.allowed_values && field.allowed_values.length) {
          if (field.datatype === 'BOOLEAN') {
            const optionValues = (!field.required && !field.present)
              ? ['', ...field.allowed_values]
              : field.allowed_values;
            return `<select
                data-field-key="${escapeHtml(field.key)}"
                data-field-required="${field.required ? 'true' : 'false'}"
              >${optionValues.map((value) => {
                const selected = value === (field.value || '');
                if (value === '' && field.inherited && !field.present && field.inherited_value) {
                  return `<option value="" selected>${escapeHtml(`Inherited: ${stripSurroundingQuotes(field.inherited_value)}`)}</option>`;
                }
                return `<option value="${escapeHtml(value)}"${selected ? ' selected' : ''}>${escapeHtml(value)}</option>`;
              }).join('')}</select>`;
          }
          return `<div>
              <input
                list="field-hints-${index}"
                data-field-key="${escapeHtml(field.key)}"
                data-field-required="${field.required ? 'true' : 'false'}"
                data-field-quote-char="${escapeHtml(detectSurroundingQuote(field.value || ''))}"
                placeholder="${escapeHtml(stripSurroundingQuotes((!field.present && field.inherited_value) ? field.inherited_value : ''))}"
                value="${escapeHtml(stripSurroundingQuotes(field.value || ''))}"
              >
              <datalist id="field-hints-${index}">${field.allowed_values.map((value) => `<option value="${escapeHtml(value)}"></option>`).join('')}</datalist>
            </div>`;
        }
        return `<input
            data-field-key="${escapeHtml(field.key)}"
            data-field-required="${field.required ? 'true' : 'false'}"
            data-field-quote-char="${escapeHtml(detectSurroundingQuote(field.value || ''))}"
            placeholder="${escapeHtml(stripSurroundingQuotes((!field.present && field.inherited_value) ? field.inherited_value : ''))}"
            value="${escapeHtml(stripSurroundingQuotes(field.value || ''))}"
          >`;
      };

      return `<div class="field-hint-list">${visibleFieldHints.map((field, index) => `
        <label class="field-hint-row">
          <span><strong>${escapeHtml(field.label)}</strong> <small class="muted">${escapeHtml(field.key)}${field.required ? ' · required' : ''}${field.repeatable ? ' · repeatable' : ''}${field.deprecated ? ' · deprecated' : ''}${field.datatype ? ` · ${escapeHtml(field.datatype)}` : ''}${field.default_value ? ` · default ${escapeHtml(stripSurroundingQuotes(field.default_value))}` : ''}${field.inherited && field.inherited_source_resource_name ? ` · inherited from ${escapeHtml(field.inherited_source_resource_type)} ${escapeHtml(stripSurroundingQuotes(field.inherited_source_resource_name))}` : ''}${field.related_resource_type ? ` · ${escapeHtml(field.related_resource_type)} reference` : ''}</small>${field.description ? `<br><small class="muted">${escapeHtml(field.description)}</small>` : ''}${field.inherited && !field.present && field.inherited_value ? `<br><small class="muted">Inherited value: ${escapeHtml(stripSurroundingQuotes(field.inherited_value))}</small>` : ''}</span>
          ${renderValueInput(field, index)}
        </label>`).join('')}</div>`;
    }

    function serializeFieldHints(fieldHintsEl) {
      return Array.from(fieldHintsEl.querySelectorAll('[data-field-key]'))
        .flatMap((field) => {
          if (field.dataset.fieldRepeatable !== 'true') {
            return `${field.dataset.fieldKey}=${restoreQuotedFieldValue(field)}`;
          }
          const quote = field.dataset.fieldQuoteChar || '';
          return getRepeatableFieldValues(field)
            .map((value) => {
              if (!quote || detectSurroundingQuote(value)) {
                return `${field.dataset.fieldKey}=${value}`;
              }
              return `${field.dataset.fieldKey}=${quote}${value}${quote}`;
            });
        })
        .join('\n');
    }

    function renderEditableResource(resource, previewUrl, previewFieldsUrl, saveUrl, options = {}) {
      detailsEl.innerHTML = `<strong id="resource-title">${escapeHtml(stripSurroundingQuotes(resource.name))}</strong>
        <div id="resource-meta" class="muted">Type: ${escapeHtml(resource.type)}<br>File: ${escapeHtml(resource.file_path)}</div>
        <h4>Guided fields</h4>
        <div id="resource-fields"></div>
        <div>
          <button id="apply-fields">Apply fields to raw config</button>
          ${options.allowEmptyOptionalToggle
            ? '<button id="toggle-empty-fields" style="margin-left: 8px;">Show empty and unset fields</button>'
            : ''}
        </div>
        <h4>Inherited fields</h4>
        <div id="resource-inherited-directives">${renderInheritedDirectives(resource.inherited_directives)}</div>
        <h4>Reference updates</h4>
        <div id="resource-rename-impacts">${renderRenameImpacts(resource.rename_impacts || [])}</div>
        <h4>Validation</h4>
        <div id="resource-validation">${renderValidation(resource.validation_messages)}</div>
        <h4>Raw config</h4>
        <textarea id="resource-editor">${escapeHtml(resource.content)}</textarea>
        <div>
          <button id="preview-changes">Preview changes</button>
          <button id="save-resource" style="margin-left: 8px;">Save resource</button>
        </div>
        <div id="preview-status" class="muted" style="margin-top: 12px;"></div>
        <pre id="preview-output"></pre>`;

      const editorEl = document.getElementById('resource-editor');
      const previewButton = document.getElementById('preview-changes');
      const saveButton = document.getElementById('save-resource');
      const previewStatusEl = document.getElementById('preview-status');
      const previewOutputEl = document.getElementById('preview-output');
      const resourceTitleEl = document.getElementById('resource-title');
      const resourceMetaEl = document.getElementById('resource-meta');
      const fieldHintsEl = document.getElementById('resource-fields');
      const applyFieldsButton = document.getElementById('apply-fields');
      const toggleEmptyFieldsButton = document.getElementById('toggle-empty-fields');
      const inheritedDirectivesEl = document.getElementById('resource-inherited-directives');
      const renameImpactsEl = document.getElementById('resource-rename-impacts');
      const validationEl = document.getElementById('resource-validation');
      let currentFieldHints = resource.field_hints || [];
      let hideEmptyOptional = !!options.hideEmptyOptional;
      previewOutputEl.textContent = resource.content;
      previewStatusEl.textContent = 'Preview changes first if you want to inspect the generated writeback before saving.';

      const synchronizeFieldHintsFromEditor = () => {
        const valuesByKey = new Map(
          Array.from(fieldHintsEl.querySelectorAll('[data-field-key]')).map((field) => [
            field.dataset.fieldKey,
            field.dataset.fieldRepeatable === 'true'
              ? getRepeatableFieldValues(field).join('\n')
              : restoreQuotedFieldValue(field),
          ]),
        );
        currentFieldHints = currentFieldHints.map((field) => {
          if (!valuesByKey.has(field.key)) {
            return field;
          }
          const value = valuesByKey.get(field.key);
          return {
            ...field,
            value,
            present: !isEffectivelyEmptyDirectiveValue(value),
          };
        });
      };

      const renderCurrentFieldHints = () => {
        fieldHintsEl.dataset.hideEmptyOptional = hideEmptyOptional ? 'true' : 'false';
        fieldHintsEl.innerHTML = renderFieldHints(currentFieldHints, {
          ...options,
          hideEmptyOptional,
        });
        if (toggleEmptyFieldsButton) {
          toggleEmptyFieldsButton.textContent = hideEmptyOptional
            ? 'Show empty and unset fields'
            : 'Hide empty and unset fields';
        }
      };

      const applyPreviewToEditor = (preview) => {
        currentFieldHints = preview.updated_field_hints;
        resourceTitleEl.textContent = stripSurroundingQuotes(preview.name || resource.name || '');
        resourceMetaEl.innerHTML = `Type: ${escapeHtml(preview.type || resource.type)}<br>File: ${escapeHtml(preview.file_path || resource.file_path)}`;
        editorEl.value = preview.updated_content;
        renderCurrentFieldHints();
        inheritedDirectivesEl.innerHTML = renderInheritedDirectives(preview.updated_inherited_directives);
        renameImpactsEl.innerHTML = renderRenameImpacts(preview.rename_impacts || []);
        validationEl.innerHTML = renderValidation(preview.updated_validation_messages);
        previewOutputEl.textContent = preview.updated_content;
      };

      renderCurrentFieldHints();

      applyFieldsButton.addEventListener('click', () => {
        applyFieldsButton.disabled = true;
        previewStatusEl.textContent = 'Generating field-based dry-run preview...';
        synchronizeFieldHintsFromEditor();
        fetch(previewFieldsUrl, {
          method: 'POST',
          headers: { 'Content-Type': 'text/plain; charset=utf-8' },
          body: serializeFieldHints(fieldHintsEl),
        })
          .then((response) => response.json())
          .then((preview) => {
            applyPreviewToEditor(preview);
            previewStatusEl.textContent =
              `Guided field preview generated — ${preview.original_line_count} -> ${preview.updated_line_count} lines`;
          })
          .catch((error) => {
            previewStatusEl.textContent = `Failed to preview guided fields: ${error}`;
          })
          .finally(() => {
            applyFieldsButton.disabled = false;
          });
      });

      previewButton.addEventListener('click', () => {
        previewButton.disabled = true;
        previewStatusEl.textContent = 'Generating dry-run preview...';
        fetch(previewUrl, {
          method: 'POST',
          headers: { 'Content-Type': 'text/plain; charset=utf-8' },
          body: editorEl.value,
        })
          .then((response) => response.json())
          .then((preview) => {
            applyPreviewToEditor(preview);
            previewStatusEl.textContent =
              `${preview.changed ? 'Changes detected' : 'No changes detected'} — ` +
              `${preview.original_line_count} -> ${preview.updated_line_count} lines` +
              ((preview.rename_impacts || []).length
                ? ` — updates ${(preview.rename_impacts || []).length} reference(s)`
                : '');
          })
          .catch((error) => {
            previewStatusEl.textContent = `Failed to preview resource: ${error}`;
          })
          .finally(() => {
            previewButton.disabled = false;
          });
      });

      saveButton.addEventListener('click', () => {
        saveButton.disabled = true;
        previewStatusEl.textContent = 'Saving resource...';
        fetch(saveUrl, {
          method: 'POST',
          headers: { 'Content-Type': 'text/plain; charset=utf-8' },
          body: editorEl.value,
        })
          .then(async (response) => ({
            ok: response.ok,
            payload: await response.json(),
          }))
          .then(({ ok, payload }) => {
            if (payload.preview) {
              applyPreviewToEditor(payload.preview);
            }

            const statusParts = [
              payload.saved ? `Saved resource (${payload.action})` : 'Save blocked',
              payload.path || '',
              payload.backup_path ? `backup: ${payload.backup_path}` : '',
              !ok && payload.error ? payload.error : '',
            ].filter(Boolean);
            previewStatusEl.textContent = statusParts.join(' — ');
            let outputText = payload.preview.updated_content;
            if (payload.verification_command) {
              outputText += `\n# Verification: ${payload.verification_command}`;
            }
            if (payload.verification_output) {
              outputText += `\n# Verification output:\n${payload.verification_output}`;
            }
            if ((payload.affected_updates || []).length) {
              outputText += '\n# Updated referenced resources:';
              (payload.affected_updates || []).forEach((update) => {
                outputText += `\n# ${update.action}: ${update.path}`;
                if (update.backup_path) {
                  outputText += ` (backup: ${update.backup_path})`;
                }
              });
            }
            if (payload.follow_up_message) {
              outputText += `\n# Follow-up: ${payload.follow_up_message}`;
            }
            if (payload.follow_up_command) {
              outputText += `\n# Command: ${payload.follow_up_command}`;
            }
            previewOutputEl.textContent = outputText;
          })
          .catch((error) => {
            previewStatusEl.textContent = `Failed to save resource: ${error}`;
          })
          .finally(() => {
            saveButton.disabled = false;
          });
      });

      if (toggleEmptyFieldsButton) {
        toggleEmptyFieldsButton.addEventListener('click', () => {
          synchronizeFieldHintsFromEditor();
          hideEmptyOptional = !hideEmptyOptional;
          renderCurrentFieldHints();
        });
      }
    }

    function showResourceDetails(resourceId) {
      fetch(`/api/v1/resources/${resourceId}/editor`)
        .then((response) => response.json())
        .then((resource) => {
          renderEditableResource(
            resource,
            `/api/v1/resources/${resource.id}/preview`,
            `/api/v1/resources/${resource.id}/preview-fields`,
            `/api/v1/resources/${resource.id}/save`,
            { hideEmptyOptional: true, allowEmptyOptionalToggle: true },
          );
        })
        .catch((error) => {
          detailsEl.textContent = `Failed to load resource: ${error}`;
        });
    }

    function showNewResourceEditor(nodeId, resourceType) {
      fetch(`/api/v1/nodes/${nodeId}/new-resource/${resourceType}/editor`)
        .then((response) => response.json())
        .then((resource) => {
          renderEditableResource(
            resource,
            `/api/v1/nodes/${nodeId}/new-resource/${resourceType}/preview`,
            `/api/v1/nodes/${nodeId}/new-resource/${resourceType}/preview-fields`,
            `/api/v1/nodes/${nodeId}/new-resource/${resourceType}/save`,
            { hideEmptyOptional: false },
          );
        })
        .catch((error) => {
          detailsEl.textContent = `Failed to initialize resource template: ${error}`;
        });
    }

    function loadNodeById(nodeId) {
      fetch(`/api/v1/nodes/${nodeId}`)
        .then((response) => response.json())
        .then((node) => {
          loadNodeResources(node);
        })
        .catch((error) => {
          detailsEl.textContent = `Failed to load node: ${error}`;
        });
    }

    function showResources(node, title, resources) {
      const creatableTypes = Array.isArray(node.creatable_resource_types)
        ? node.creatable_resource_types
        : [];
      const preferredResourceTypeByKind = {
        director: 'director',
        'file-daemon': 'client',
        'storage-daemon': 'storage',
      };
      const preferredResourceType = preferredResourceTypeByKind[node.kind] || '';
      const normalizedNodeLabel = stripSurroundingQuotes(node.label || '');
      const sortedResources = [...resources].sort((left, right) => {
        const leftTypePriority = left.type === preferredResourceType ? 0 : 1;
        const rightTypePriority = right.type === preferredResourceType ? 0 : 1;
        if (leftTypePriority !== rightTypePriority) {
          return leftTypePriority - rightTypePriority;
        }
        if (left.type !== right.type) {
          return left.type.localeCompare(right.type);
        }
        const leftNamePriority = left.type === preferredResourceType
            && stripSurroundingQuotes(left.name || '') === normalizedNodeLabel
          ? 0
          : 1;
        const rightNamePriority = right.type === preferredResourceType
            && stripSurroundingQuotes(right.name || '') === normalizedNodeLabel
          ? 0
          : 1;
        if (leftNamePriority !== rightNamePriority) {
          return leftNamePriority - rightNamePriority;
        }
        return (left.name || '').localeCompare(right.name || '');
      });
      const groupedResources = sortedResources.reduce((groups, resource) => {
        if (!groups[resource.type]) {
          groups[resource.type] = [];
        }
        groups[resource.type].push(resource);
        return groups;
      }, {});

      if (!resources.length) {
        resourcesEl.innerHTML = `<strong>${title}</strong>` +
          (creatableTypes.length
            ? `<div style="margin: 12px 0;">
                <select id="new-resource-type">${creatableTypes.map((type) => `<option value="${escapeHtml(type)}">${escapeHtml(type)}</option>`).join('')}</select>
                <button id="create-resource" style="margin-top: 0; margin-left: 8px;">Add resource</button>
              </div>`
            : '') +
          `<div class="muted">No resources discovered yet.</div>`;
        if (creatableTypes.length) {
          resourcesEl.querySelector('#create-resource').addEventListener('click', () => {
            showNewResourceEditor(node.id, resourcesEl.querySelector('#new-resource-type').value);
          });
        }
        return;
      }
      resourcesEl.innerHTML = `<strong>${title}</strong>` +
        (creatableTypes.length
          ? `<div style="margin: 12px 0;">
              <select id="new-resource-type">${creatableTypes.map((type) => `<option value="${escapeHtml(type)}">${escapeHtml(type)}</option>`).join('')}</select>
              <button id="create-resource" style="margin-top: 0; margin-left: 8px;">Add resource</button>
            </div>`
          : '') +
        Object.entries(groupedResources).map(([type, typeResources]) => `
        <div style="margin: 12px 0 4px;">
          <strong>${escapeHtml(type)}</strong>
        </div>
        ${typeResources.map((resource) => `
          <div class="resource" data-id="${resource.id}">
            ${escapeHtml(stripSurroundingQuotes(resource.name))}
          </div>`).join('')}`).join('');

      if (creatableTypes.length) {
        resourcesEl.querySelector('#create-resource').addEventListener('click', () => {
          showNewResourceEditor(node.id, resourcesEl.querySelector('#new-resource-type').value);
        });
      }
      resourcesEl.querySelectorAll('.resource').forEach((resourceEl) => {
        resourceEl.addEventListener('click', () => {
          showResourceDetails(resourceEl.dataset.id);
        });
      });
    }

    function loadNodeResources(node) {
      Promise.all([
        fetch(`/api/v1/nodes/${node.id}/resources`).then((response) => response.json()),
        fetch(`/api/v1/nodes/${node.id}/relationships`).then((response) => response.json()),
      ])
        .then(([resources, relationships]) => {
          showResources(node, `${node.label} resources`, resources);
          if (node.kind === 'datacenter') {
            renderAddClientWizard(node);
            return;
          }
          detailsEl.innerHTML = `<pre>Kind: ${node.kind}
Location: ${node.description || '-'}</pre>
            ${node.kind === 'client'
              ? `<div><button id="remove-remote-client">Remove remote client</button></div>
                 <div id="remove-remote-client-status" class="muted" style="margin-top: 8px;"></div>`
              : ''}
            <h4>Relationships</h4>
            <div>${renderRelationships(relationships)}</div>`;
          if (node.kind === 'client') {
            document.getElementById('remove-remote-client').addEventListener('click', () => {
              const removeButton = document.getElementById('remove-remote-client');
              const statusEl = document.getElementById('remove-remote-client-status');
              if (!window.confirm(`Remove remote client ${node.label}?`)) {
                return;
              }
              removeButton.disabled = true;
              statusEl.textContent = 'Removing remote client...';
              fetch(`/api/v1/nodes/${node.id}/remove`, { method: 'POST' })
                .then((response) => response.json())
                .then((payload) => {
                  if (!payload.removed) {
                    throw new Error(payload.error || 'Removal failed');
                  }
                  statusEl.textContent = 'Remote client removed.';
                  return fetch('/api/v1/bootstrap')
                    .then((response) => response.json())
                    .then((data) => {
                      renderTree(data);
                      loadNodeById('datacenter');
                    });
                })
                .catch((error) => {
                  statusEl.textContent = `Failed to remove remote client: ${error}`;
                })
                .finally(() => {
                  removeButton.disabled = false;
                });
            });
          }
          attachRelationshipActions();
        })
        .catch((error) => {
          detailsEl.textContent = `Failed to load node resources: ${error}`;
        });
    }

    function renderNode(node, parentEl) {
      const wrapper = document.createElement('div');
      wrapper.innerHTML = `
        <div class="node"><strong>${node.label}</strong> <small class="muted">${node.kind}</small></div>
        <div class="children"></div>`;

      wrapper.querySelector(':scope > .node').addEventListener('click', () => {
        loadNodeById(node.id);
      });

      const childrenEl = wrapper.querySelector(':scope > .children');
      (node.children || []).forEach((child) => renderNode(child, childrenEl));
      parentEl.appendChild(wrapper);
    }

    function renderTree(data) {
      const root = data.tree;
      if (!root || !(root.children || []).length) {
        treeEl.innerHTML = '<div class="muted">No Bareos directors discovered.</div>';
        return;
      }
      treeEl.innerHTML = '';
      renderNode(root, treeEl);
    }

    fetch('/api/v1/bootstrap')
      .then((response) => response.json())
      .then((data) => renderTree(data))
      .catch((error) => {
        treeEl.textContent = `Failed to load bootstrap data: ${error}`;
      });
  </script>
</body>
</html>)HTML";
}
}  // namespace

HttpResponse HandleConfigServiceRequest(const ConfigServiceOptions& options,
                                        const HttpRequest& request)
{
  if (request.method != "GET" && request.method != "POST") {
    return {405, "text/plain; charset=utf-8", "Method Not Allowed"};
  }

  if (request.method == "GET"
      && (request.path == "/" || request.path == "/index.html")) {
    return {200, "text/html; charset=utf-8", BuildIndexHtml()};
  }

  if (request.method == "GET" && request.path == "/api/v1/healthz") {
    return {200, "application/json; charset=utf-8", "{\"status\":\"ok\"}"};
  }

  if (request.method == "GET" && request.path == "/api/v1/bootstrap") {
    const auto model = DiscoverDatacenterSummary(options.config_roots);
    return {200, "application/json; charset=utf-8",
            SerializeDatacenterSummary(model)};
  }

  if (request.method == "GET" && request.path == "/api/v1/tree") {
    const auto model = DiscoverDatacenterSummary(options.config_roots);
    return {200, "application/json; charset=utf-8", SerializeTreeNodeSummary(model.tree)};
  }

  const auto path_parts = SplitPath(request.path);
  if (request.method == "GET" && path_parts.size() == 4 && path_parts[0] == "api"
      && path_parts[1] == "v1"
      && path_parts[2] == "nodes") {
    const auto model = DiscoverDatacenterSummary(options.config_roots);
    const auto* node = FindTreeNodeById(model, path_parts[3]);
    if (!node) return {404, "text/plain; charset=utf-8", "Node Not Found"};
    return {200, "application/json; charset=utf-8", SerializeNodeDetail(*node)};
  }

  if (request.method == "GET" && path_parts.size() == 5
      && path_parts[0] == "api" && path_parts[1] == "v1"
      && path_parts[2] == "nodes" && path_parts[4] == "resources") {
    const auto model = DiscoverDatacenterSummary(options.config_roots);
    const auto* node = FindTreeNodeById(model, path_parts[3]);
    if (!node) return {404, "text/plain; charset=utf-8", "Node Not Found"};
    return {200, "application/json; charset=utf-8",
            SerializeResourceSummaries(
                CollectNodeResources(options, model, *node))};
  }

  if (request.method == "GET" && path_parts.size() == 5
      && path_parts[0] == "api" && path_parts[1] == "v1"
      && path_parts[2] == "nodes" && path_parts[4] == "relationships") {
    const auto model = DiscoverDatacenterSummary(options.config_roots);
    const auto* node = FindTreeNodeById(model, path_parts[3]);
    if (!node) return {404, "text/plain; charset=utf-8", "Node Not Found"};
    return {200, "application/json; charset=utf-8",
             SerializeRelationshipSummaries(
                 FindRelationshipsForNode(model, path_parts[3]))};
  }

  if (request.method == "POST" && path_parts.size() == 5
      && path_parts[0] == "api" && path_parts[1] == "v1"
      && path_parts[2] == "nodes" && path_parts[4] == "remove") {
    const auto model = DiscoverDatacenterSummary(options.config_roots);
    const auto* node = FindTreeNodeById(model, path_parts[3]);
    if (!node) return {404, "text/plain; charset=utf-8", "Node Not Found"};
    const auto result = RemoveRemoteClient(options, model, *node);
    return {result.status_code, "application/json; charset=utf-8",
            SerializeRemoteClientRemovalResult(result)};
  }

  if (request.method == "GET" && path_parts.size() == 7
      && path_parts[0] == "api" && path_parts[1] == "v1"
      && path_parts[2] == "nodes" && path_parts[4] == "new-resource"
      && path_parts[6] == "editor") {
    const auto model = DiscoverDatacenterSummary(options.config_roots);
    const auto* node = FindTreeNodeById(model, path_parts[3]);
    if (!node || !SupportsManualResourceCreation(*node)) {
      return {404, "text/plain; charset=utf-8", "Node Not Found"};
    }

    const auto creatable_types = CreatableResourceTypesForNode(*node);
    if (std::find(creatable_types.begin(), creatable_types.end(), path_parts[5])
        == creatable_types.end()) {
      return {404, "text/plain; charset=utf-8", "Resource Type Not Found"};
    }

    return {200, "application/json; charset=utf-8",
            SerializeEditableResourceDetail(
                BuildNewResourceDetail(*node, path_parts[5]))};
  }

  if (request.method == "POST" && path_parts.size() == 7
      && path_parts[0] == "api" && path_parts[1] == "v1"
      && path_parts[2] == "nodes" && path_parts[4] == "new-resource"
      && path_parts[6] == "preview") {
    const auto model = DiscoverDatacenterSummary(options.config_roots);
    const auto* node = FindTreeNodeById(model, path_parts[3]);
    if (!node || !SupportsManualResourceCreation(*node)) {
      return {404, "text/plain; charset=utf-8", "Node Not Found"};
    }

    const auto creatable_types = CreatableResourceTypesForNode(*node);
    if (std::find(creatable_types.begin(), creatable_types.end(), path_parts[5])
        == creatable_types.end()) {
      return {404, "text/plain; charset=utf-8", "Resource Type Not Found"};
    }

    const auto detail = BuildNewResourceDetail(*node, path_parts[5]);
    return {200, "application/json; charset=utf-8",
            SerializeRenameAwarePreview(
                BuildRenameAwarePreview(detail, request.body))};
  }

  if (request.method == "POST" && path_parts.size() == 7
      && path_parts[0] == "api" && path_parts[1] == "v1"
      && path_parts[2] == "nodes" && path_parts[4] == "new-resource"
      && path_parts[6] == "preview-fields") {
    const auto model = DiscoverDatacenterSummary(options.config_roots);
    const auto* node = FindTreeNodeById(model, path_parts[3]);
    if (!node || !SupportsManualResourceCreation(*node)) {
      return {404, "text/plain; charset=utf-8", "Node Not Found"};
    }

    const auto creatable_types = CreatableResourceTypesForNode(*node);
    if (std::find(creatable_types.begin(), creatable_types.end(), path_parts[5])
        == creatable_types.end()) {
      return {404, "text/plain; charset=utf-8", "Resource Type Not Found"};
    }

    const auto detail = BuildNewResourceDetail(*node, path_parts[5]);
    return {200, "application/json; charset=utf-8",
            SerializeRenameAwarePreview(BuildRenameAwarePreview(
                detail, BuildFieldHintEditPreview(
                            detail,
                            ParseFieldHintUpdateBody(request.body,
                                                     detail.field_hints))
                            .updated_content))};
  }

  if (request.method == "POST" && path_parts.size() == 7
      && path_parts[0] == "api" && path_parts[1] == "v1"
      && path_parts[2] == "nodes" && path_parts[4] == "new-resource"
      && path_parts[6] == "save") {
    const auto model = DiscoverDatacenterSummary(options.config_roots);
    const auto* node = FindTreeNodeById(model, path_parts[3]);
    if (!node || !SupportsManualResourceCreation(*node)) {
      return {404, "text/plain; charset=utf-8", "Node Not Found"};
    }

    const auto creatable_types = CreatableResourceTypesForNode(*node);
    if (std::find(creatable_types.begin(), creatable_types.end(), path_parts[5])
        == creatable_types.end()) {
      return {404, "text/plain; charset=utf-8", "Resource Type Not Found"};
    }

    const auto result
        = PersistResourceDetail(options,
                                BuildNewResourceDetail(*node, path_parts[5]),
                                request.body, true);
    return {result.status_code, "application/json; charset=utf-8",
            SerializePersistResult(result)};
  }

  if (request.method == "GET" && path_parts.size() == 4
      && path_parts[0] == "api" && path_parts[1] == "v1"
      && path_parts[2] == "resources") {
    const auto model = DiscoverDatacenterSummary(options.config_roots);
    const auto resource = ResolveResourceSummary(options, model, path_parts[3]);
    if (!resource) return {404, "text/plain; charset=utf-8", "Resource Not Found"};
    return {200, "application/json; charset=utf-8",
            SerializeResourceDetail(LoadResourceDetail(*resource))};
  }

  if (request.method == "GET" && path_parts.size() == 5
      && path_parts[0] == "api" && path_parts[1] == "v1"
      && path_parts[2] == "resources" && path_parts[4] == "editor") {
    const auto model = DiscoverDatacenterSummary(options.config_roots);
    const auto resource = ResolveResourceSummary(options, model, path_parts[3]);
    if (!resource) return {404, "text/plain; charset=utf-8", "Resource Not Found"};
    return {200, "application/json; charset=utf-8",
            SerializeEditableResourceDetail(LoadResourceDetail(*resource))};
  }

  if (request.method == "POST" && path_parts.size() == 5
      && path_parts[0] == "api" && path_parts[1] == "v1"
      && path_parts[2] == "resources" && path_parts[4] == "preview") {
    const auto model = DiscoverDatacenterSummary(options.config_roots);
    const auto resource = ResolveResourceSummary(options, model, path_parts[3]);
    if (!resource) return {404, "text/plain; charset=utf-8", "Resource Not Found"};

    const auto detail = LoadResourceDetail(*resource);
    return {200, "application/json; charset=utf-8",
            SerializeRenameAwarePreview(
                BuildRenameAwarePreview(detail, request.body))};
  }

  if (request.method == "POST" && path_parts.size() == 5
      && path_parts[0] == "api" && path_parts[1] == "v1"
      && path_parts[2] == "resources" && path_parts[4] == "preview-fields") {
    const auto model = DiscoverDatacenterSummary(options.config_roots);
    const auto resource = ResolveResourceSummary(options, model, path_parts[3]);
    if (!resource) return {404, "text/plain; charset=utf-8", "Resource Not Found"};

    const auto detail = LoadResourceDetail(*resource);
    return {200, "application/json; charset=utf-8",
            SerializeRenameAwarePreview(BuildRenameAwarePreview(
                detail, BuildFieldHintEditPreview(
                            detail,
                            ParseFieldHintUpdateBody(request.body,
                                                     detail.field_hints))
                            .updated_content))};
  }

  if (request.method == "POST" && path_parts.size() == 5
      && path_parts[0] == "api" && path_parts[1] == "v1"
      && path_parts[2] == "resources" && path_parts[4] == "save") {
    const auto model = DiscoverDatacenterSummary(options.config_roots);
    const auto resource = ResolveResourceSummary(options, model, path_parts[3]);
    if (!resource) return {404, "text/plain; charset=utf-8", "Resource Not Found"};

    const auto result = PersistResourceDetail(options,
                                              LoadResourceDetail(*resource),
                                              request.body, false);
    return {result.status_code, "application/json; charset=utf-8",
            SerializePersistResult(result)};
  }

  if (request.method == "GET" && path_parts.size() == 6
      && path_parts[0] == "api" && path_parts[1] == "v1"
      && path_parts[2] == "directors" && path_parts[4] == "add-client"
      && path_parts[5] == "schema") {
    const auto model = DiscoverDatacenterSummary(options.config_roots);
    const auto* node = FindTreeNodeById(model, path_parts[3]);
    if (!node || node->kind != "director") {
      return {404, "text/plain; charset=utf-8", "Director Not Found"};
    }

    return {200, "application/json; charset=utf-8",
            SerializeAddClientWizardSchema(*node)};
  }

  if (request.method == "GET" && path_parts.size() == 7
      && path_parts[0] == "api" && path_parts[1] == "v1"
      && path_parts[2] == "nodes" && path_parts[4] == "add-client"
      && path_parts[5] == "schema") {
    const auto model = DiscoverDatacenterSummary(options.config_roots);
    const auto* node = FindTreeNodeById(model, path_parts[3]);
    if (!node || node->kind != "datacenter") {
      return {404, "text/plain; charset=utf-8", "Datacenter Not Found"};
    }

    const auto* director = FindDatacenterDirectorNode(*node, path_parts[6]);
    if (!director) {
      return {404, "text/plain; charset=utf-8", "Director Not Found"};
    }

    return {200, "application/json; charset=utf-8",
            SerializeAddClientWizardSchema(*director)};
  }

  if (request.method == "POST" && path_parts.size() == 6
      && path_parts[0] == "api" && path_parts[1] == "v1"
      && path_parts[2] == "directors" && path_parts[4] == "add-client"
      && path_parts[5] == "preview") {
    const auto model = DiscoverDatacenterSummary(options.config_roots);
    const auto* node = FindTreeNodeById(model, path_parts[3]);
    if (!node || node->kind != "director") {
      return {404, "text/plain; charset=utf-8", "Director Not Found"};
    }

    return {200, "application/json; charset=utf-8",
            SerializeAddClientWizardPreview(options, *node,
                                            ParseKeyValueBody(request.body))};
  }

  if (request.method == "POST" && path_parts.size() == 7
      && path_parts[0] == "api" && path_parts[1] == "v1"
      && path_parts[2] == "nodes" && path_parts[4] == "add-client"
      && path_parts[5] == "preview") {
    const auto model = DiscoverDatacenterSummary(options.config_roots);
    const auto* node = FindTreeNodeById(model, path_parts[3]);
    if (!node || node->kind != "datacenter") {
      return {404, "text/plain; charset=utf-8", "Datacenter Not Found"};
    }

    const auto* director = FindDatacenterDirectorNode(*node, path_parts[6]);
    if (!director) {
      return {404, "text/plain; charset=utf-8", "Director Not Found"};
    }

    return {200, "application/json; charset=utf-8",
            SerializeAddClientWizardPreview(options, *director,
                                            ParseKeyValueBody(request.body))};
  }

  if (request.method == "POST" && path_parts.size() == 7
      && path_parts[0] == "api" && path_parts[1] == "v1"
      && path_parts[2] == "nodes" && path_parts[4] == "add-client"
      && path_parts[5] == "save") {
    const auto model = DiscoverDatacenterSummary(options.config_roots);
    const auto* node = FindTreeNodeById(model, path_parts[3]);
    if (!node || node->kind != "datacenter") {
      return {404, "text/plain; charset=utf-8", "Datacenter Not Found"};
    }

    const auto* director = FindDatacenterDirectorNode(*node, path_parts[6]);
    if (!director) {
      return {404, "text/plain; charset=utf-8", "Director Not Found"};
    }

    const auto result = PersistAddClientWizard(options, *director,
                                               ParseKeyValueBody(request.body));
    return {result.status_code, "application/json; charset=utf-8",
            SerializeWizardPersistResult(result)};
  }

  return {404, "text/plain; charset=utf-8", "Not Found"};
}
