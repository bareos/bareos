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
#include <mutex>
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
                                file_path.string(),
                                node.kind,
                                node.description};
  return BuildResourceDetail(summary, block_name + " {\n}\n");
}

struct AddClientTargetPlan {
  std::string file_path;
  std::string action;
  bool exists_local = false;
  std::filesystem::path staged_root;
  std::optional<std::filesystem::path> source_root;
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

std::filesystem::path BuildGeneratedDirectorConfigRoot(
    const ConfigServiceOptions& options, const std::string& director_name)
{
  return options.generated_config_root / "directors"
         / SanitizePathComponent(director_name) / "etc" / "bareos";
}

bool IsGeneratedRemotePath(const std::string& path)
{
  return path.find("/generated/clients/") != std::string::npos
         || path.find("/generated/directors/") != std::string::npos
         || path.find("/generated/storages/") != std::string::npos
         || path.find("/var/lib/bareos-config/generated/clients/")
                != std::string::npos
         || path.find("/var/lib/bareos-config/generated/directors/")
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

std::string MakeStagedDirectorClientNodeId(const std::string& director_name,
                                           const std::string& client_name)
{
  return "staged-client-" + SanitizePathComponent(director_name) + "-"
         + SanitizePathComponent(client_name);
}

struct PersistResult;
struct VerificationResult;
std::vector<TreeNodeSummary> CollectStagedDirectorClientNodes(
    const ConfigServiceOptions& options, const DatacenterSummary& model);
std::optional<TreeNodeSummary> ResolveNodeSummary(const ConfigServiceOptions& options,
                                                  const DatacenterSummary& model,
                                                  const std::string& node_id);
TreeNodeSummary BuildAugmentedTree(const ConfigServiceOptions& options,
                                   const DatacenterSummary& model);
std::string SerializeBootstrapSummary(const ConfigServiceOptions& options,
                                      const DatacenterSummary& model);
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
bool MirrorConfigTreeIntoStage(const std::filesystem::path& source_root,
                               const std::filesystem::path& staged_root,
                               const std::filesystem::path& excluded_root,
                               std::string& error);
void PopulateFollowUpAction(PersistResult& result);
void RollbackPersistedResource(const PersistResult& result);
VerificationResult VerifySavedConfig(const ConfigServiceOptions& options,
                                     const ResourceSummary& summary);

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

struct PasswordChange {
  std::string old_password;
  std::string new_password;
};

struct PasswordSourceDetails {
  ResourceDetail original;
  ResourceDetail updated;
};

struct PersistedAffectedUpdate {
  ResourceSummary resource;
  std::string action;
  std::string path;
  std::string backup_path;
  std::string original_path;
};

struct PersistResult {
  bool saved = false;
  int status_code = 200;
  std::string action = "unchanged";
  std::string backup_path;
  std::string original_path;
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

DaemonComponent ComponentForSummary(const ResourceSummary& summary)
{
  if (summary.component == "director") return DaemonComponent::kDirector;
  if (summary.component == "file-daemon") return DaemonComponent::kFileDaemon;
  if (summary.component == "storage-daemon") {
    return DaemonComponent::kStorageDaemon;
  }
  return DaemonComponent::kUnknown;
}

struct VerificationResult {
  bool success = false;
  std::string command;
  std::string output;
  std::string error;
};

std::string MaybeRenameConfigFilePath(const ResourceSummary& summary,
                                      const std::string& old_name,
                                      const std::string& new_name)
{
  if (summary.type != "configuration" || old_name.empty() || new_name.empty()
      || old_name == new_name) {
    return summary.file_path;
  }

  const auto path = std::filesystem::path(summary.file_path);
  if (path.extension() != ".conf" || path.stem() != old_name) {
    return summary.file_path;
  }
  return (path.parent_path() / (new_name + path.extension().string())).string();
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

size_t StartLineIndex(const ResourceSummary& resource)
{
  return resource.source_line_start == 0 ? 0 : resource.source_line_start - 1;
}

size_t EndLineIndex(const ResourceSummary& resource)
{
  return resource.source_line_end == 0 ? 0 : resource.source_line_end - 1;
}

std::vector<ResourceSummary> DiscoverContainedResources(
    const ResourceSummary& resource,
    const std::string& content,
    const std::string& resource_type = {})
{
  auto resources = DiscoverResourceSummariesFromContent(resource, content);
  if (!resource_type.empty()) {
    resources.erase(
        std::remove_if(resources.begin(), resources.end(),
                       [&resource_type](const ResourceSummary& summary) {
                         return summary.type != resource_type;
                       }),
        resources.end());
  }
  return resources;
}

std::optional<ResourceDetail> BuildContainedResourceDetail(
    const ResourceSummary& container,
    const ResourceSummary& resource,
    const std::string& content)
{
  if (resource.source_line_start == 0 || resource.source_line_end == 0) {
    return std::nullopt;
  }

  const auto lines = SplitLines(content);
  const auto start = StartLineIndex(resource);
  const auto end = EndLineIndex(resource);
  if (start >= lines.size() || end >= lines.size() || start > end) {
    return std::nullopt;
  }

  std::vector<std::string> block_lines(
      lines.begin() + static_cast<std::ptrdiff_t>(start),
      lines.begin() + static_cast<std::ptrdiff_t>(end + 1));
  auto summary = resource;
  summary.file_path = container.file_path;
  summary.component = container.component;
  summary.config_root = container.config_root;
  return BuildResourceDetail(summary, JoinLines(block_lines));
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

std::optional<std::string> ExtractGeneratedDirectorName(
    const ConfigServiceOptions& options, const std::string& file_path)
{
  if (options.generated_config_root.empty()) return std::nullopt;

  const auto absolute_path = std::filesystem::path(file_path).lexically_normal();
  const auto generated_root = options.generated_config_root.lexically_normal();
  const auto relative = absolute_path.lexically_relative(generated_root);
  if (relative.empty() || relative == absolute_path) return std::nullopt;

  auto it = relative.begin();
  if (it == relative.end() || *it != "directors") return std::nullopt;
  ++it;
  if (it == relative.end()) return std::nullopt;
  return it->string();
}

std::vector<TreeNodeSummary> CollectStagedDirectorClientNodes(
    const ConfigServiceOptions& options, const DatacenterSummary& model)
{
  std::vector<TreeNodeSummary> nodes;
  const auto generated_root = options.generated_config_root / "directors";
  if (!std::filesystem::exists(generated_root)) return nodes;

  for (std::filesystem::recursive_directory_iterator it(generated_root), end;
       it != end; ++it) {
    if (!it->is_regular_file()) continue;
    if (it->path().extension() != ".conf") continue;
    if (it->path().parent_path().filename() != "client") continue;
    if (it->path().parent_path().parent_path().filename() != "bareos-dir.d") {
      continue;
    }

    const auto director_name = ExtractGeneratedDirectorName(
        options, it->path().string());
    if (!director_name || director_name->empty()) continue;

    const auto client_name = it->path().stem().string();
    if (client_name.empty()) continue;
    const auto live_director = std::find_if(
        model.directors.begin(), model.directors.end(),
        [&director_name](const DirectorSummary& director) {
          return director.name == *director_name;
        });
    if (live_director != model.directors.end()) {
      const auto existing_client = std::find_if(
          live_director->resources.begin(), live_director->resources.end(),
          [&client_name](const ResourceSummary& resource) {
            return resource.type == "client"
                   && StripSurroundingQuotes(resource.name) == client_name;
          });
      if (existing_client != live_director->resources.end()) continue;
    }

    const ResourceSummary resource{
        "staged-director-client-" + SanitizePathComponent(*director_name) + "-"
            + SanitizePathComponent(client_name),
        "client",
        client_name,
        it->path().string(),
        "director",
        std::filesystem::path(it->path()).parent_path().parent_path().string()};
    nodes.push_back({MakeStagedDirectorClientNodeId(*director_name, client_name),
                     "client",
                     client_name,
                     it->path().string(),
                     {resource},
                     {}});
  }

  std::sort(nodes.begin(), nodes.end(),
            [](const TreeNodeSummary& lhs, const TreeNodeSummary& rhs) {
              return lhs.label < rhs.label;
            });
  return nodes;
}

std::optional<TreeNodeSummary> ResolveNodeSummary(const ConfigServiceOptions& options,
                                                  const DatacenterSummary& model,
                                                  const std::string& node_id)
{
  if (const auto* node = FindTreeNodeById(model, node_id)) {
    return *node;
  }

  const auto staged_nodes = CollectStagedDirectorClientNodes(options, model);
  const auto it = std::find_if(staged_nodes.begin(), staged_nodes.end(),
                               [&node_id](const TreeNodeSummary& node) {
                                 return node.id == node_id;
                               });
  if (it == staged_nodes.end()) return std::nullopt;
  return *it;
}

TreeNodeSummary BuildAugmentedTree(const ConfigServiceOptions& options,
                                   const DatacenterSummary& model)
{
  auto tree = model.tree;
  auto staged_nodes = CollectStagedDirectorClientNodes(options, model);
  for (auto& node : staged_nodes) {
    const auto duplicate = std::find_if(
        tree.children.begin(), tree.children.end(),
        [&node](const TreeNodeSummary& existing) { return existing.id == node.id; });
    if (duplicate == tree.children.end()) {
      tree.children.push_back(std::move(node));
    }
  }

  return tree;
}

std::string SerializeBootstrapSummary(const ConfigServiceOptions& options,
                                      const DatacenterSummary& model)
{
  return "{\"tree\":"
         + SerializeTreeNodeSummary(BuildAugmentedTree(options, model)) + "}";
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
           generated_path.string(),
           "file-daemon",
           director.config_root});
    }
  }

  for (const auto& node : CollectStagedDirectorClientNodes(options, model)) {
    resources.insert(resources.end(), node.resources.begin(), node.resources.end());

    const auto director_name
        = ExtractGeneratedDirectorName(options, node.description).value_or("");
    const auto generated_path
        = BuildGeneratedRemoteConfigRoot(options, "client", node.label)
          / "bareos-fd.d" / "director" / (director_name + ".conf");
    if (!std::filesystem::exists(generated_path)) continue;

    resources.push_back(
        {MakeGeneratedClientConfigId(node.id, node.label),
         "configuration",
         node.label + " generated client config",
         generated_path.string(),
         "file-daemon",
         node.description});
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

  std::string director_name;
  std::string director_id;
  if (const auto* director
      = FindOwningDirectorForResource(model, node.resources.front().id)) {
    director_name = director->name;
    director_id = director->id;
  } else if (const auto staged_director_name
             = ExtractGeneratedDirectorName(options, node.resources.front().file_path)) {
    director_name = *staged_director_name;
    director_id = node.id;
  } else {
    return resources;
  }

  const auto generated_path
      = BuildGeneratedRemoteConfigRoot(options, "client", node.label)
        / "bareos-fd.d" / "director" / (director_name + ".conf");
  if (!std::filesystem::exists(generated_path)) return resources;

  resources.push_back({MakeGeneratedClientConfigId(director_id, node.label),
                       "configuration",
                       node.label + " generated client config",
                       generated_path.string(),
                       "file-daemon",
                       node.description});
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
  std::string director_name;
  bool verify_director = true;
  if (const auto* director = FindOwningDirectorForResource(model, client_resource.id)) {
    director_name = director->name;
  } else if (const auto staged_director_name
             = ExtractGeneratedDirectorName(options, client_resource.file_path)) {
    director_name = *staged_director_name;
    verify_director = false;
  } else {
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
        / "bareos-fd.d" / "director" / (director_name + ".conf");
  const ResourceSummary generated_summary{
      MakeGeneratedClientConfigId(node.id, node.label),
      "configuration",
      node.label + " generated client config",
      generated_path.string(),
      "file-daemon",
      std::filesystem::path(node.description).string()};
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

  if (verify_director) {
    const auto verification = VerifySavedConfig(options, client_resource);
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
  }

  PopulateFollowUpAction(result.client_resource);
  result.removed = true;
  return result;
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

std::optional<ResourceDetail> BuildDirectorBlockDetail(
    const ResourceSummary& resource,
    const std::string& content,
    const std::string& preferred_name = "")
{
  auto blocks = DiscoverContainedResources(resource, content, "director");
  if (blocks.empty()) return std::nullopt;

  auto match = blocks.begin();
  if (!preferred_name.empty()) {
    match = std::find_if(blocks.begin(), blocks.end(),
                         [&preferred_name](const ResourceSummary& block) {
                           return StripSurroundingQuotes(block.name)
                                  == preferred_name;
                         });
    if (match == blocks.end()) {
      if (blocks.size() != 1) return std::nullopt;
      match = blocks.begin();
    }
  }

  auto detail = BuildContainedResourceDetail(resource, *match, content);
  if (detail) { detail->summary.id = resource.id; }
  return detail;
}

std::optional<ResourceDetail> BuildSyntheticDirectorPasswordDetail(
    const ResourceSummary& resource,
    const std::string& director_name,
    const std::string& password)
{
  if (director_name.empty() || password.empty()) return std::nullopt;

  auto summary = resource;
  summary.type = "director";
  summary.name = director_name;

  std::ostringstream content;
  content << "Director {\n"
          << "  Name = " << director_name << "\n"
          << "  Password = " << password << "\n"
          << "}\n";
  return BuildResourceDetail(summary, content.str());
}

std::optional<PasswordSourceDetails> BuildConsoleAuthPasswordSourceDetails(
    const DatacenterSummary& summary,
    const ResourceDetail& detail,
    const ResourceEditPreview& preview)
{
  const DaemonSummary* daemon = nullptr;
  const auto* director
      = FindOwningDirectorForDaemonResource(summary, detail.summary.id, &daemon);
  if (!director || !daemon || daemon->kind != "console") return std::nullopt;

  const auto director_name = StripSurroundingQuotes(director->name);
  if (director_name.empty()) return std::nullopt;

  if (detail.summary.type == "console" || detail.summary.type == "user") {
    const auto old_password = StripSurroundingQuotes(
        ExtractDirectiveValue(detail.directives, "Password"));
    const auto new_password = StripSurroundingQuotes(
        ExtractDirectiveValue(preview.updated_directives, "Password"));
    if (old_password.empty() || new_password.empty() || old_password == new_password) {
      return std::nullopt;
    }

    const auto original = BuildSyntheticDirectorPasswordDetail(
        detail.summary, director_name, old_password);
    const auto updated = BuildSyntheticDirectorPasswordDetail(
        preview.summary, director_name, new_password);
    if (!original || !updated) return std::nullopt;
    return PasswordSourceDetails{*original, *updated};
  }

  if (detail.summary.type != "configuration") return std::nullopt;

  const auto find_changed_password = [&](const std::string& block_name)
      -> std::optional<PasswordChange> {
    const auto resource_type = NormalizeDirectiveNameForComparison(block_name);
    const auto old_blocks
        = DiscoverContainedResources(detail.summary, detail.content, resource_type);
    const auto new_blocks = DiscoverContainedResources(preview.summary,
                                                       preview.updated_content,
                                                       resource_type);
    const auto block_count = std::min(old_blocks.size(), new_blocks.size());
    for (size_t index = 0; index < block_count; ++index) {
      const auto& old_block = old_blocks[index];
      const auto& new_block = new_blocks[index];
      const auto old_detail
          = BuildContainedResourceDetail(detail.summary, old_block, detail.content);
      const auto new_detail = BuildContainedResourceDetail(preview.summary, new_block,
                                                           preview.updated_content);
      if (!old_detail || !new_detail) continue;
      const auto old_password = StripSurroundingQuotes(
          ExtractDirectiveValue(old_detail->directives, "Password"));
      const auto new_password = StripSurroundingQuotes(
          ExtractDirectiveValue(new_detail->directives, "Password"));
      if (old_password.empty() || new_password.empty() || old_password == new_password) {
        continue;
      }
      return PasswordChange{old_password, new_password};
    }
    return std::nullopt;
  };

  for (const auto& block_name : std::array<std::string, 3>{"Director", "Console",
                                                           "User"}) {
    const auto changed_password = find_changed_password(block_name);
    if (!changed_password) continue;

    const auto original = BuildSyntheticDirectorPasswordDetail(
        detail.summary, director_name, changed_password->old_password);
    const auto updated = BuildSyntheticDirectorPasswordDetail(
        preview.summary, director_name, changed_password->new_password);
    if (!original || !updated) return std::nullopt;
    return PasswordSourceDetails{*original, *updated};
  }

  return std::nullopt;
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

bool MirrorConfigTreeIntoStage(const std::filesystem::path& source_root,
                               const std::filesystem::path& staged_root,
                               const std::filesystem::path& excluded_root,
                               std::string& error)
{
  if (source_root.empty() || !std::filesystem::exists(source_root)) return true;

  std::error_code ec;
  std::filesystem::create_directories(staged_root, ec);
  if (ec) {
    error = "Failed to create staging root " + staged_root.string() + ".";
    return false;
  }

  const auto normalized_excluded_root = excluded_root.lexically_normal();
  for (std::filesystem::recursive_directory_iterator it(source_root), end;
       it != end; ++it) {
    const auto current_path = it->path().lexically_normal();
    if (!normalized_excluded_root.empty()) {
      const auto excluded_relative
          = current_path.lexically_relative(normalized_excluded_root);
      if ((current_path == normalized_excluded_root)
          || (!excluded_relative.empty() && excluded_relative != current_path
              && *excluded_relative.begin() != "..")) {
        if (it->is_directory()) it.disable_recursion_pending();
        continue;
      }
    }

    const auto relative = it->path().lexically_relative(source_root);
    const auto target = staged_root / relative;

    if (it->is_directory()) {
      std::filesystem::create_directories(target, ec);
      if (ec) {
        error = "Failed to create staging directory " + target.string() + ".";
        return false;
      }
      continue;
    }

    if (!it->is_regular_file()) continue;
    std::filesystem::create_directories(target.parent_path(), ec);
    if (ec) {
      error = "Failed to create staging directory " + target.parent_path().string()
              + ".";
      return false;
    }
    if (std::filesystem::exists(target)) continue;

    std::filesystem::copy_file(it->path(), target,
                               std::filesystem::copy_options::skip_existing, ec);
    if (ec) {
      error = "Failed to copy " + it->path().string() + " into staging.";
      return false;
    }
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

bool SaveExistingResourceContent(const ResourceSummary& original_summary,
                                 const ResourceSummary& updated_summary,
                                 const std::string& updated_content,
                                 std::string& action,
                                 std::string& backup_path,
                                 std::string& error)
{
  const auto original_path = std::filesystem::path(original_summary.file_path);
  const auto updated_path = std::filesystem::path(updated_summary.file_path);
  const auto block_name = ResourceBlockNameForType(original_summary.type);
  if (updated_path != original_path) {
    if (std::filesystem::exists(updated_path)) {
      error = "Target path already exists: " + updated_path.string() + ".";
      return false;
    }

    if (!WriteFileWithBackup(updated_path, updated_content, action, backup_path,
                             error)) {
      return false;
    }

    std::string delete_action;
    std::string original_backup_path;
    if (!DeleteFileWithBackup(original_path, delete_action, original_backup_path,
                              error)) {
      std::error_code cleanup_error;
      std::filesystem::remove(updated_path, cleanup_error);
      return false;
    }
    action = "move";
    backup_path = std::move(original_backup_path);
    return true;
  }

  if (block_name.empty() || !std::filesystem::exists(updated_path)) {
    return WriteFileWithBackup(updated_path, updated_content, action, backup_path,
                               error);
  }

  const auto original_content = ReadFileToString(updated_path);
  const auto block_ranges = DiscoverContainedResources(
      original_summary, original_content,
      NormalizeDirectiveNameForComparison(block_name));
  std::vector<ResourceSummary> matches;
  for (const auto& block : block_ranges) {
    if (StripSurroundingQuotes(block.name)
        == StripSurroundingQuotes(original_summary.name)) {
      matches.push_back(block);
    }
  }
  if (matches.empty() && block_ranges.size() == 1) {
    matches.push_back(block_ranges.front());
  }
  if (matches.empty()) {
    error = "Resource block could not be located in "
            + original_summary.file_path + ".";
    return false;
  }
  if (matches.size() > 1) {
    error = "Resource block name is ambiguous in " + original_summary.file_path + ".";
    return false;
  }

  auto lines = SplitLines(original_content);
  const auto replacement_lines = SplitLines(updated_content);
  const auto& match = matches.front();
  lines.erase(lines.begin() + static_cast<std::ptrdiff_t>(StartLineIndex(match)),
              lines.begin() + static_cast<std::ptrdiff_t>(EndLineIndex(match) + 1));
  lines.insert(lines.begin() + static_cast<std::ptrdiff_t>(StartLineIndex(match)),
               replacement_lines.begin(), replacement_lines.end());

  return WriteFileWithBackup(updated_path, JoinLines(lines), action, backup_path,
                             error);
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
  const auto block_ranges = DiscoverContainedResources(
      summary, original_content, NormalizeDirectiveNameForComparison(block_name));
  std::vector<ResourceSummary> matches;
  for (const auto& block : block_ranges) {
    if (StripSurroundingQuotes(block.name)
        == StripSurroundingQuotes(summary.name)) {
      matches.push_back(block);
    }
  }
  if (matches.empty() && block_ranges.size() == 1) {
    matches.push_back(block_ranges.front());
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
  lines.erase(lines.begin() + static_cast<std::ptrdiff_t>(StartLineIndex(match)),
              lines.begin() + static_cast<std::ptrdiff_t>(EndLineIndex(match) + 1));

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

  if (detail.summary.type == "schedule") {
    for (const auto& reference : ExtractScheduleRunOverrideReferences(detail)) {
      if (NormalizeDirectiveNameForComparison(reference.directive_key)
              != normalized_key
          || reference.referenced_name != old_name) {
        continue;
      }
      impacts.push_back({detail.summary, reference.directive_key, reference.line,
                         reference.raw_value,
                         PreserveReferenceQuoting(reference.raw_value, new_name)});
    }
  }

  return impacts;
}

std::optional<std::string> ExtractDirectiveKeyFromRelationship(
    const RelationshipSummary& relationship)
{
  static const std::string prefix = "Directive ";
  static const std::string suffix = " in ";
  if (!relationship.resolution.starts_with(prefix)) {
    return std::nullopt;
  }

  const auto suffix_pos = relationship.resolution.find(suffix, prefix.size());
  if (suffix_pos == std::string::npos || suffix_pos <= prefix.size()) {
    return std::nullopt;
  }

  const auto directive_key = TrimWhitespace(
      relationship.resolution.substr(prefix.size(),
                                     suffix_pos - prefix.size()));
  if (directive_key.empty()) return std::nullopt;
  return directive_key;
}

bool MatchesRelationshipTarget(const RelationshipSummary& relationship,
                               const ResourceSummary& target)
{
  if (!relationship.target_resource_id.empty() && !target.id.empty()
      && relationship.target_resource_id == target.id) {
    return true;
  }

  if (!relationship.target_resource_path.empty() && !target.file_path.empty()
      && relationship.target_resource_path == target.file_path
      && relationship.target_resource_type == target.type
      && NormalizeDirectiveNameForComparison(relationship.target_resource_name)
             == NormalizeDirectiveNameForComparison(
                 StripSurroundingQuotes(target.name))) {
    return true;
  }

  return false;
}

bool MatchesRelationshipSource(const RelationshipSummary& relationship,
                               const ResourceSummary& source)
{
  if (!relationship.source_resource_id.empty() && !source.id.empty()
      && relationship.source_resource_id == source.id) {
    return true;
  }

  if (!relationship.source_resource_path.empty() && !source.file_path.empty()
      && relationship.source_resource_path == source.file_path
      && relationship.source_resource_type == source.type
      && NormalizeDirectiveNameForComparison(relationship.source_resource_name)
             == NormalizeDirectiveNameForComparison(
                 StripSurroundingQuotes(source.name))) {
    return true;
  }

  return false;
}

bool ApplyRenameToFieldHints(std::vector<ResourceFieldHint>& field_hints,
                             const std::string& directive_key,
                             const std::string& old_name,
                             const std::string& new_name)
{
  const auto normalized_key = NormalizeDirectiveNameForComparison(directive_key);
  bool changed = false;
  for (auto& field : field_hints) {
    if (!field.present
        || NormalizeDirectiveNameForComparison(field.key) != normalized_key) {
      continue;
    }

    if (field.repeatable) {
      auto values = SplitRepeatableFieldValue(field.value);
      bool field_changed = false;
      for (auto& value : values) {
        if (StripSurroundingQuotes(value) != old_name) continue;
        value = PreserveReferenceQuoting(value, new_name);
        field_changed = true;
      }
      if (!field_changed) continue;
      field.value = JoinRepeatableFieldValues(values);
      changed = true;
      continue;
    }

    if (StripSurroundingQuotes(field.value) != old_name) continue;
    field.value = PreserveReferenceQuoting(field.value, new_name);
    changed = true;
  }

  if (normalized_key == "run") return changed;

  for (auto& field : field_hints) {
    if (!field.present || NormalizeDirectiveNameForComparison(field.key) != "run") {
      continue;
    }

    auto run_lines = SplitRepeatableFieldValue(field.value);
    bool field_changed = false;
    for (size_t run_index = 0; run_index < run_lines.size(); ++run_index) {
      ResourceDetail run_detail;
      run_detail.summary.type = "schedule";
      run_detail.directives.push_back(
          {"Run", run_lines[run_index], run_index + 1, 1});

      auto references = ExtractScheduleRunOverrideReferences(run_detail);
      for (auto it = references.rbegin(); it != references.rend(); ++it) {
        if (NormalizeDirectiveNameForComparison(it->directive_key) != normalized_key
            || it->referenced_name != old_name) {
          continue;
        }
        run_lines[run_index].replace(
            it->value_offset, it->value_length,
            PreserveReferenceQuoting(it->raw_value, new_name));
        field_changed = true;
      }
    }

    if (!field_changed) continue;
    field.value = JoinRepeatableFieldValues(run_lines);
    changed = true;
  }

  return changed;
}

void AppendUniqueRenameImpacts(std::vector<RenameImpact>& impacts,
                               const std::vector<RenameImpact>& additions)
{
  for (const auto& addition : additions) {
    const auto duplicate = std::find_if(
        impacts.begin(), impacts.end(),
        [&addition](const RenameImpact& existing) {
          return existing.resource.id == addition.resource.id
                 && existing.directive_key == addition.directive_key
                 && existing.line == addition.line
                 && existing.old_value == addition.old_value
                 && existing.new_value == addition.new_value;
        });
    if (duplicate == impacts.end()) impacts.push_back(addition);
  }
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

  if (detail.summary.type == "director") {
    const auto director_name = StripSurroundingQuotes(detail.summary.name);
    if (director_name.empty()) return targets;

    const auto add_matching_director_targets = [&](const DirectorSummary& director) {
      for (const auto& resource : director.resources) {
        if (resource.id == detail.summary.id || resource.type != "director") continue;
        if (StripSurroundingQuotes(resource.name) != director_name) continue;
        add_target(resource);
      }
      for (const auto& daemon : director.daemons) {
        for (const auto& resource : daemon.resources) {
          if (resource.id == detail.summary.id || resource.type != "director") continue;
          if (StripSurroundingQuotes(resource.name) != director_name) continue;
          add_target(resource);
        }
      }
    };

    if (const auto* director = FindOwningDirectorForResource(summary, detail.summary.id)) {
      add_matching_director_targets(*director);
      return targets;
    }

    const DaemonSummary* daemon = nullptr;
    const auto* director
        = FindOwningDirectorForDaemonResource(summary, detail.summary.id, &daemon);
    if (!director || !daemon) return targets;
    add_matching_director_targets(*director);
    return targets;
  }

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

std::vector<ResourceSummary> FindConsoleConfigurationDirectorTargets(
    const DatacenterSummary& summary, const ResourceDetail& detail)
{
  std::vector<ResourceSummary> targets;
  if (detail.summary.type != "director") return targets;

  const auto director_name = StripSurroundingQuotes(detail.summary.name);
  if (director_name.empty()) return targets;

  const auto collect = [&](const DirectorSummary& director) {
      for (const auto& daemon : director.daemons) {
        if (daemon.kind != "console") continue;
        for (const auto& resource : daemon.resources) {
          if (resource.type != "configuration") continue;
          const auto content = ReadFileToString(resource.file_path);
          const auto blocks
              = DiscoverContainedResources(resource, content, "director");
          const auto match = std::find_if(
              blocks.begin(), blocks.end(),
              [&director_name](const ResourceSummary& block) {
                return StripSurroundingQuotes(block.name) == director_name;
              });
          if (match != blocks.end()) targets.push_back(resource);
        }
      }
  };

  if (const auto* director = FindOwningDirectorForResource(summary, detail.summary.id)) {
    collect(*director);
    return targets;
  }

  const auto* director = FindOwningDirectorForDaemonResource(summary, detail.summary.id);
  if (!director) return targets;
  collect(*director);
  return targets;
}

std::vector<ResourceSummary> FindConsoleAuthPasswordTargets(
    const DatacenterSummary& summary, const ResourceDetail& detail)
{
  std::vector<ResourceSummary> targets;
  if (detail.summary.type != "director") return targets;

  std::set<std::string> seen;
  const auto add_target = [&](const ResourceSummary& resource) {
    if (resource.id == detail.summary.id) return;
    if (resource.type != "console" && resource.type != "user") return;
    if (seen.insert(resource.id).second) targets.push_back(resource);
  };

  const auto collect = [&](const DirectorSummary& director) {
    for (const auto& daemon : director.daemons) {
      if (daemon.kind != "console") continue;
      for (const auto& resource : daemon.resources) add_target(resource);
    }
  };

  if (const auto* director = FindOwningDirectorForResource(summary, detail.summary.id)) {
    collect(*director);
    return targets;
  }

  const auto* director = FindOwningDirectorForDaemonResource(summary, detail.summary.id);
  if (!director) return targets;
  collect(*director);
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
  const auto source_director_detail
      = detail.summary.type == "configuration"
            ? BuildDirectorBlockDetail(detail.summary, detail.content)
            : std::nullopt;
  const auto updated_director_detail
      = source_director_detail
            ? BuildDirectorBlockDetail(result.preview.summary,
                                       result.preview.updated_content,
                                       source_director_detail->summary.name)
            : std::nullopt;
  const auto& name_source_directives
      = source_director_detail ? source_director_detail->directives
                               : detail.directives;
  const auto& name_updated_directives
      = updated_director_detail ? updated_director_detail->directives
                                : result.preview.updated_directives;
  const auto name_source_summary
      = source_director_detail ? source_director_detail->summary : detail.summary;
  const auto name_updated_summary
      = updated_director_detail ? updated_director_detail->summary
                                : result.preview.summary;
  const auto& rename_source_detail
      = source_director_detail ? *source_director_detail : detail;
  const auto old_name
      = ExtractResourceName(name_source_summary, name_source_directives);
  const auto new_name
      = ExtractResourceName(name_updated_summary, name_updated_directives);
  const bool name_changed
      = !old_name.empty() && !new_name.empty() && old_name != new_name;
  if (detail.summary.type == "configuration" && name_changed) {
    result.preview.summary.file_path
        = MaybeRenameConfigFilePath(detail.summary, old_name, new_name);
    result.preview.summary.name
        = std::filesystem::path(result.preview.summary.file_path).filename().string();
  }

  const auto config_root = std::filesystem::path(detail.summary.config_root);
  if (config_root.empty()) return result;

  const auto model = DiscoverDatacenterSummary({config_root});
  if (name_changed) {
    auto updated_source_field_hints = result.preview.updated_field_hints;
    std::vector<RenameImpact> source_impacts;
    bool source_changed = false;

    for (const auto& relationship : FindAllRelationships(model)) {
      if (!MatchesRelationshipTarget(relationship, name_source_summary)) continue;

      const auto directive_key = ExtractDirectiveKeyFromRelationship(relationship);
      if (!directive_key) continue;

      if (MatchesRelationshipSource(relationship, rename_source_detail.summary)) {
        if (rename_source_detail.summary.type == "configuration") continue;
        if (!ApplyRenameToFieldHints(updated_source_field_hints, *directive_key,
                                     old_name, new_name)) {
          continue;
        }
        AppendUniqueRenameImpacts(
            source_impacts,
            CollectRenameImpacts(rename_source_detail, *directive_key, old_name,
                                 new_name));
        source_changed = true;
        continue;
      }

      const auto* source_resource
          = FindResourceById(model, relationship.source_resource_id);
      if (!source_resource) continue;

      const auto existing_update = std::find_if(
          result.affected_updates.begin(), result.affected_updates.end(),
          [source_resource](const RenameAffectedUpdate& update) {
            return update.detail.summary.id == source_resource->id;
          });

      ResourceDetail related_detail;
      std::vector<ResourceFieldHint> updated_field_hints;
      std::vector<RenameImpact> resource_impacts;
      if (existing_update != result.affected_updates.end()) {
        related_detail = existing_update->detail;
        updated_field_hints = existing_update->preview.updated_field_hints;
        resource_impacts = existing_update->impacts;
      } else {
        related_detail = LoadResourceDetail(*source_resource);
        updated_field_hints = related_detail.field_hints;
      }

      if (!ApplyRenameToFieldHints(updated_field_hints, *directive_key, old_name,
                                   new_name)) {
        continue;
      }

      AppendUniqueRenameImpacts(
          resource_impacts,
          CollectRenameImpacts(related_detail, *directive_key, old_name,
                               new_name));
      auto preview = BuildFieldHintEditPreview(related_detail, updated_field_hints);
      AppendUniqueRenameImpacts(result.rename_impacts, resource_impacts);
      if (existing_update != result.affected_updates.end()) {
        existing_update->preview = std::move(preview);
        existing_update->impacts = std::move(resource_impacts);
      } else {
        result.affected_updates.push_back(
            {std::move(related_detail), std::move(preview),
             std::move(resource_impacts)});
      }
    }

    if (source_changed) {
      result.preview
          = BuildFieldHintEditPreview(rename_source_detail, updated_source_field_hints);
      AppendUniqueRenameImpacts(result.rename_impacts, source_impacts);
    }

    for (const auto& target : FindDeviceRenameTargets(model, rename_source_detail)) {
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

    for (const auto& target
         : FindDirectorRenameTargets(model, rename_source_detail, old_name)) {
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

    for (const auto& target : FindConsoleConfigurationDirectorTargets(
             model, rename_source_detail)) {
      const auto existing_update = std::find_if(
          result.affected_updates.begin(), result.affected_updates.end(),
          [&target](const RenameAffectedUpdate& update) {
            return update.detail.summary.id == target.id;
          });
      if (existing_update != result.affected_updates.end()) continue;

      auto related_detail = LoadResourceDetail(target);
      const auto block_ranges
          = DiscoverContainedResources(target, related_detail.content, "director");
      const auto block = std::find_if(
          block_ranges.begin(), block_ranges.end(),
          [&old_name](const ResourceSummary& range) {
            return StripSurroundingQuotes(range.name) == old_name;
          });
      if (block == block_ranges.end()) continue;

      auto content_lines = SplitLines(related_detail.content);
      const auto block_detail
          = BuildContainedResourceDetail(target, *block, related_detail.content);
      if (!block_detail) continue;
      auto updated_field_hints = block_detail->field_hints;
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

      auto block_preview
          = BuildFieldHintEditPreview(*block_detail, updated_field_hints);
      auto impacts = CollectRenameImpacts(*block_detail, "Name", old_name, new_name);
      if (impacts.empty()) {
        impacts.push_back({target, "Name", 0, old_name, new_name});
      }
      result.rename_impacts.insert(result.rename_impacts.end(), impacts.begin(),
                                   impacts.end());

      content_lines.erase(
          content_lines.begin() + static_cast<std::ptrdiff_t>(StartLineIndex(*block)),
          content_lines.begin()
              + static_cast<std::ptrdiff_t>(EndLineIndex(*block) + 1));
      const auto replacement_lines = SplitLines(block_preview.updated_content);
      content_lines.insert(
          content_lines.begin() + static_cast<std::ptrdiff_t>(StartLineIndex(*block)),
          replacement_lines.begin(), replacement_lines.end());

      auto preview = BuildResourceEditPreview(related_detail, JoinLines(content_lines));
      result.affected_updates.push_back(
          {std::move(related_detail), std::move(preview), std::move(impacts)});
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
                      + " resource(s): "
                      + SummarizeRenameImpacts(name_impacts) + ".";
    warning.line = FindDirectiveLine(result.preview.updated_directives, "Name");
    result.preview.updated_validation_messages.push_back(std::move(warning));
  }

  std::optional<ResourceDetail> console_source_password_detail;
  std::optional<ResourceDetail> console_updated_password_detail;
  if (detail.summary.type == "configuration" || detail.summary.type == "console"
      || detail.summary.type == "user") {
    const auto console_password_source
        = BuildConsoleAuthPasswordSourceDetails(model, detail, result.preview);
    if (console_password_source) {
      console_source_password_detail = console_password_source->original;
      console_updated_password_detail = console_password_source->updated;
    }
  }

  const ResourceDetail* source_password_detail
      = console_source_password_detail
            ? &*console_source_password_detail
            : (source_director_detail ? &*source_director_detail : nullptr);
  const ResourceDetail* updated_password_detail
      = console_updated_password_detail
            ? &*console_updated_password_detail
            : (updated_director_detail ? &*updated_director_detail : nullptr);
  const auto& password_source_directives
      = source_password_detail ? source_password_detail->directives
                               : detail.directives;
  const auto& password_updated_directives
      = updated_password_detail ? updated_password_detail->directives
                                : result.preview.updated_directives;
  const auto password_source_summary
      = source_password_detail ? source_password_detail->summary : detail.summary;

  const auto old_password = StripSurroundingQuotes(
      ExtractDirectiveValue(password_source_directives, "Password"));
  const auto new_password = StripSurroundingQuotes(
      ExtractDirectiveValue(password_updated_directives, "Password"));
  if (!old_password.empty() && !new_password.empty() && old_password != new_password) {
    auto password_detail = detail;
    password_detail.summary = password_source_summary;
    password_detail.directives = password_source_directives;
    if (source_password_detail) {
      password_detail.content = source_password_detail->content;
      password_detail.inherited_directives
          = source_password_detail->inherited_directives;
      password_detail.validation_messages
          = source_password_detail->validation_messages;
      password_detail.field_hints = source_password_detail->field_hints;
    }
    const auto console_configuration_targets
        = FindConsoleConfigurationDirectorTargets(model, password_detail);
    for (const auto& target : FindPasswordPropagationTargets(model, password_detail)) {
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

    for (const auto& target : FindConsoleAuthPasswordTargets(model, password_detail)) {
      const bool handled_by_configuration = std::any_of(
          console_configuration_targets.begin(), console_configuration_targets.end(),
          [&target](const ResourceSummary& configuration_target) {
            return configuration_target.file_path == target.file_path;
          });
      if (handled_by_configuration) continue;

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

      auto password_field = std::find_if(
          updated_field_hints.begin(), updated_field_hints.end(),
          [](const ResourceFieldHint& field) {
            return NormalizeDirectiveNameForComparison(field.key) == "password";
          });
      if (password_field == updated_field_hints.end() || !password_field->present
          || StripSurroundingQuotes(password_field->value) != old_password) {
        continue;
      }

      password_field->value
          = PreserveReferenceQuoting(password_field->value, new_password);
      password_field->present = true;

      auto impacts = CollectRenameImpacts(related_detail, "Password", old_password,
                                          new_password);
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

    for (const auto& target : console_configuration_targets) {
      const auto existing_update = std::find_if(
          result.affected_updates.begin(), result.affected_updates.end(),
          [&target](const RenameAffectedUpdate& update) {
            return update.detail.summary.id == target.id;
          });

      if (existing_update != result.affected_updates.end()) continue;

      auto related_detail = LoadResourceDetail(target);
      auto updated_config_content = related_detail.content;
      std::vector<RenameImpact> impacts;
      bool changed = false;

      const auto update_matching_blocks
          = [&](const std::string& resource_type,
                const auto& matcher) {
              const auto block_ranges = DiscoverContainedResources(
                  target, updated_config_content, resource_type);
              for (const auto& block : block_ranges) {
                if (!matcher(block)) continue;

                auto content_lines = SplitLines(updated_config_content);
                auto block_detail
                    = BuildContainedResourceDetail(target, block, updated_config_content);
                if (!block_detail) continue;
                auto updated_field_hints = block_detail->field_hints;
                auto password_field = std::find_if(
                    updated_field_hints.begin(), updated_field_hints.end(),
                    [](const ResourceFieldHint& field) {
                      return NormalizeDirectiveNameForComparison(field.key)
                             == "password";
                    });
                if (password_field == updated_field_hints.end()) continue;

                const bool had_password = password_field->present;
                if (had_password
                    && StripSurroundingQuotes(password_field->value)
                           != old_password) {
                  continue;
                }

                password_field->value = PreserveReferenceQuoting(
                    password_field->value, new_password);
                password_field->present = true;

                auto block_preview = BuildFieldHintEditPreview(*block_detail,
                                                               updated_field_hints);
                auto block_impacts = CollectRenameImpacts(
                    *block_detail, "Password", had_password ? old_password : "",
                    new_password);
                if (block_impacts.empty()) {
                  block_impacts.push_back(
                      {block_detail->summary, "Password", 0,
                       had_password ? old_password : "", new_password});
                }
                impacts.insert(impacts.end(), block_impacts.begin(),
                               block_impacts.end());

                content_lines.erase(
                    content_lines.begin()
                        + static_cast<std::ptrdiff_t>(StartLineIndex(block)),
                    content_lines.begin()
                        + static_cast<std::ptrdiff_t>(EndLineIndex(block) + 1));
                const auto replacement_lines
                    = SplitLines(block_preview.updated_content);
                content_lines.insert(
                    content_lines.begin()
                        + static_cast<std::ptrdiff_t>(StartLineIndex(block)),
                    replacement_lines.begin(), replacement_lines.end());
                updated_config_content = JoinLines(content_lines);
                changed = true;
              }
            };

      update_matching_blocks(
          "director",
          [&password_detail](const ResourceSummary& range) {
            return StripSurroundingQuotes(range.name)
                   == StripSurroundingQuotes(password_detail.summary.name);
          });
      update_matching_blocks("console",
                             [](const ResourceSummary&) { return true; });
      update_matching_blocks("user",
                             [](const ResourceSummary&) { return true; });

      if (!changed) continue;
      result.rename_impacts.insert(result.rename_impacts.end(), impacts.begin(),
                                   impacts.end());
      auto preview = BuildResourceEditPreview(related_detail, updated_config_content);
      result.affected_updates.push_back(
          {std::move(related_detail), std::move(preview), std::move(impacts)});
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
           << "\"backup_path\":" << JsonString(update.backup_path) << ","
           << "\"original_path\":" << JsonString(update.original_path) << "}";
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
         << "\"original_path\":" << JsonString(result.original_path) << ","
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
                                     const ResourceSummary& summary);

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
    ResourceSummary original_summary;
    ResourceSummary summary;
    std::string content;
    bool new_resource = false;
    std::string action;
    std::string backup_path;
  };

  std::vector<PendingWrite> writes;
  writes.push_back(
      {detail.summary, result.preview.summary, result.preview.updated_content,
       new_resource, "", ""});
  for (const auto& affected_update : rename_preview.affected_updates) {
    writes.push_back({affected_update.detail.summary,
                      affected_update.preview.summary,
                      affected_update.preview.updated_content, false, "", ""});
  }

  for (auto& write : writes) {
    std::string error;
    const auto saved
        = write.new_resource
              ? WriteFileWithBackup(write.summary.file_path, write.content,
                                    write.action, write.backup_path, error)
              : SaveExistingResourceContent(write.original_summary, write.summary,
                                            write.content, write.action,
                                            write.backup_path, error);
    if (!saved) {
      for (auto written = writes.rbegin(); written != writes.rend(); ++written) {
        if (written->action.empty() || written->action == "unchanged") continue;
        PersistResult rollback_result;
        rollback_result.saved = true;
        rollback_result.action = written->action;
        rollback_result.backup_path = written->backup_path;
        rollback_result.preview.summary = written->summary;
        rollback_result.original_path = written->original_summary.file_path;
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
  result.original_path = writes.front().original_summary.file_path;
  for (size_t i = 1; i < writes.size(); ++i) {
    result.affected_updates.push_back(
        {writes[i].summary, writes[i].action, writes[i].summary.file_path,
         writes[i].backup_path, writes[i].original_summary.file_path});
  }

  std::set<std::string> verified_targets;
  for (const auto& write : writes) {
    if (IsGeneratedRemotePath(write.summary.file_path)) continue;
    const auto component = ComponentForSummary(write.summary);
    if (component == DaemonComponent::kUnknown) continue;
    const auto config_root = std::filesystem::path(write.summary.config_root);
    if (config_root.empty()) continue;
    const auto verification_key
        = std::to_string(static_cast<int>(component)) + ":" + config_root.string();
    if (!verified_targets.insert(verification_key).second) continue;

    const auto verification = VerifySavedConfig(options, write.summary);
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
        rollback_result.original_path = written->original_summary.file_path;
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

  const auto& summary = result.preview.summary;
  const auto& path = summary.file_path;
  if (!IsManagedLocalPath(path)) return;

  const bool default_root = summary.config_root == "/etc/bareos";
  auto set_follow_up = [&](std::string component, std::string action,
                           std::string command, std::string message) {
    result.follow_up_component = std::move(component);
    result.follow_up_action = std::move(action);
    result.follow_up_command = std::move(command);
    result.follow_up_message = std::move(message);
  };

  if (summary.component == "file-daemon") {
    set_follow_up(
        "file-daemon", "restart",
        default_root ? "systemctl restart bareos-filedaemon" : "",
        default_root
            ? "Restart the Bareos File Daemon to apply these changes."
            : "Restart the matching Bareos File Daemon instance for this non-default config root.");
    return;
  }
  if (summary.component == "storage-daemon") {
    set_follow_up(
        "storage-daemon", "restart",
        default_root ? "systemctl restart bareos-storage" : "",
        default_root
            ? "Restart the Bareos Storage Daemon to apply these changes."
            : "Restart the matching Bareos Storage Daemon instance for this non-default config root.");
    return;
  }
  if (summary.component == "director") {
    set_follow_up(
        "director", "reload",
        default_root ? "systemctl reload bareos-director" : "",
        default_root
            ? "Reload the Bareos Director to apply these changes."
            : "Reload the matching Bareos Director instance for this non-default config root.");
  }
}

VerificationResult VerifySavedConfig(const ConfigServiceOptions& options,
                                     const ResourceSummary& summary)
{
  const auto component = ComponentForSummary(summary);
  if (component == DaemonComponent::kUnknown) {
    return {true, "", "", ""};
  }

  const auto config_root = std::filesystem::path(summary.config_root);
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

  if (result.action == "move") {
    std::filesystem::remove(path, ec);
    if (!result.backup_path.empty() && !result.original_path.empty()) {
      std::filesystem::copy_file(
          result.backup_path, result.original_path,
          std::filesystem::copy_options::overwrite_existing, ec);
    }
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
  std::set<std::string> updated_keys_from_request;
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
      if (field.repeatable && updated_keys_from_request.contains(key)
          && !field.value.empty() && !value.empty()) {
        field.value += "\n" + value;
      } else {
        field.value = value;
      }
      field.present = !value.empty();
      updated_keys_from_request.insert(key);
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
    const ConfigServiceOptions& options,
    const TreeNodeSummary& director_node,
    const std::map<std::string, std::string>& values);

ResourceDetail BuildAddClientDirectorTemplate(
    const ConfigServiceOptions& options,
    const TreeNodeSummary& director_node,
    const std::map<std::string, std::string>& values)
{
  std::string resource_name = "new-client";
  if (const auto name_it = values.find("Name");
      name_it != values.end() && !name_it->second.empty()) {
    resource_name = name_it->second;
  }

  const auto staged_path = BuildGeneratedDirectorConfigRoot(options, director_node.label)
                           / "bareos-dir.d" / "client" / (resource_name + ".conf");
  const ResourceSummary live_summary{
      "wizard-add-client-preview",
      "client",
      resource_name,
      (std::filesystem::path(director_node.description) / "bareos-dir.d" / "client"
        / (resource_name + ".conf"))
          .string(),
      "director",
      director_node.description};
  auto detail = BuildResourceDetail(live_summary, "Client {\n}\n");
  detail.summary.file_path = staged_path.string();
  return detail;
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

std::string SerializeAddClientWizardSchema(const ConfigServiceOptions& options,
                                           const TreeNodeSummary& director_node)
{
  const auto director_template
      = BuildAddClientDirectorTemplate(options, director_node, {});
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
    const ConfigServiceOptions& options,
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
      (BuildGeneratedDirectorConfigRoot(options, director_node.label) / "bareos-dir.d"
        / "client" / (resource_name + ".conf"))
          .string(),
      "director",
      director_node.description};

  auto director_template
      = BuildAddClientDirectorTemplate(options, director_node, values);
  director_template.summary = summary;
  const auto field_hints = ApplyAddClientWizardValues(
      SelectAddClientWizardFieldHints(director_template), values);
  const auto preview = BuildFieldHintEditPreview(director_template, field_hints);

  return {preview.summary,
          preview.updated_content,
          preview.updated_directives,
          preview.updated_inherited_directives,
          preview.updated_validation_messages,
          preview.updated_field_hints,
          {}};
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

bool IsManagedLocalClient(const ConfigServiceOptions& options,
                          const TreeNodeSummary& director_node,
                          const std::string& client_name)
{
  if (client_name.empty()) return false;

  const auto config_root = std::filesystem::path(director_node.description);
  const auto local_fd_conf = config_root / "bareos-fd.conf";
  const auto local_fd_dir = config_root / "bareos-fd.d";
  if (!std::filesystem::exists(local_fd_conf)
      && !std::filesystem::exists(local_fd_dir)) {
    return false;
  }

  const auto model = DiscoverDatacenterSummary(options.config_roots);
  const auto director = std::find_if(
      model.directors.begin(), model.directors.end(),
      [&director_node](const DirectorSummary& summary) {
        return summary.id == director_node.id;
      });
  if (director == model.directors.end()) return false;

  const auto daemon_it = std::find_if(
      director->daemons.begin(), director->daemons.end(),
      [](const DaemonSummary& daemon) { return daemon.kind == "file-daemon"; });
  if (daemon_it == director->daemons.end()) return false;

  return StripSurroundingQuotes(daemon_it->configured_name) == client_name;
}

AddClientTargetPlan BuildClientFileDaemonTargetPlan(
    const ConfigServiceOptions& options,
    const TreeNodeSummary& director_node,
    const std::string& client_name)
{
  const auto staged_root
      = BuildGeneratedRemoteConfigRoot(options, "client", client_name);
  const auto staged_target
      = staged_root / "bareos-fd.d" / "director" / (director_node.label + ".conf");
  const bool target_exists = std::filesystem::exists(staged_target);
  AddClientTargetPlan plan{staged_target.string(),
                           target_exists ? "update" : "create",
                           target_exists,
                           staged_root,
                           std::nullopt};
  if (IsManagedLocalClient(options, director_node, client_name)) {
    plan.source_root = std::filesystem::path(director_node.description);
  }
  return plan;
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
      target_plan.file_path,
      "file-daemon",
      director_node.description};
  return BuildResourceDetail(summary,
                             BuildClientSideDirectorPreviewContent(director_node,
                                                                    values));
}

std::string SerializeAddClientWizardPreview(
    const ConfigServiceOptions& options,
    const TreeNodeSummary& director_node,
    const std::map<std::string, std::string>& values)
{
  const auto director_preview
      = BuildAddClientDirectorPreview(options, director_node, values);
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
  const auto client_name = values.contains("Name") ? values.at("Name") : "";
  const auto director_staged_root
      = BuildGeneratedDirectorConfigRoot(options, director_node.label);
  const auto file_daemon_target_plan
      = BuildClientFileDaemonTargetPlan(options, director_node, client_name);
  const auto director_preview
      = BuildAddClientDirectorPreview(options, director_node, values);
  const auto file_daemon_preview
      = BuildAddClientFileDaemonPreview(options, director_node, values);

  result.director_client.preview
      = BuildResourceEditPreview(director_preview, director_preview.content);
  result.client_file_daemon_director.preview
      = BuildResourceEditPreview(file_daemon_preview, file_daemon_preview.content);

  std::string stage_error;
  if (!MirrorConfigTreeIntoStage(std::filesystem::path(director_node.description),
                                 director_staged_root,
                                 options.generated_config_root, stage_error)) {
    result.status_code = 409;
    result.error = stage_error;
    return result;
  }
  if (file_daemon_target_plan.source_root
      && !MirrorConfigTreeIntoStage(*file_daemon_target_plan.source_root,
                                    file_daemon_target_plan.staged_root,
                                    options.generated_config_root,
                                    stage_error)) {
    result.status_code = 409;
    result.error = stage_error;
    return result;
  }

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
    :root {
      --setup-primary: #0075be;
      --setup-secondary: #5a6773;
      --setup-accent: #f5a623;
      --setup-surface: #ffffff;
      --setup-surface-muted: #f5f7fa;
      --setup-border: #d9e2ec;
      --setup-text: #22303c;
      --setup-text-muted: #5a6773;
      --setup-shadow: 0 10px 30px rgba(0, 117, 190, 0.08);
    }
    body {
      font-family: system-ui, sans-serif;
      margin: 0;
      color: var(--setup-text);
      background: linear-gradient(180deg, #eef6fb 0%, #f5f7fa 160px);
    }
    header {
      padding: 14px 24px;
      background: var(--setup-primary);
      color: white;
      box-shadow: 0 2px 12px rgba(0, 117, 190, 0.25);
    }
    .layout {
      display: grid;
      grid-template-columns: 420px 1fr;
      gap: 24px;
      max-width: 1480px;
      margin: 0 auto;
      padding: 24px;
      min-height: calc(100vh - 60px);
      box-sizing: border-box;
      align-items: start;
    }
    .pane {
      background: var(--setup-surface);
      border: 1px solid var(--setup-border);
      border-radius: 16px;
      padding: 20px;
      overflow: auto;
      box-shadow: var(--setup-shadow);
    }
    .pane h3 {
      margin: 0 0 16px;
      color: var(--setup-primary);
      font-size: 1.2rem;
      font-weight: 700;
      padding-bottom: 12px;
      border-bottom: 1px solid #e7edf3;
    }
    .app-title, .section-heading, .tree-icon-label {
      display: inline-flex;
      align-items: center;
      gap: 8px;
    }
    .icon {
      display: inline-flex;
      width: 18px;
      height: 18px;
      color: currentColor;
      flex: 0 0 18px;
    }
    .icon svg {
      width: 18px;
      height: 18px;
      stroke: currentColor;
      fill: none;
      stroke-width: 1.8;
      stroke-linecap: round;
      stroke-linejoin: round;
    }
    .icon.fill svg { fill: currentColor; stroke: none; }
    .tree-entry { display: flex; align-items: center; gap: 6px; }
    .tree-toggle {
      margin: 0;
      padding: 0;
      width: 18px;
      min-width: 18px;
      height: 18px;
      border: 0;
      background: none;
      color: var(--setup-secondary);
      border-radius: 4px;
    }
    .tree-toggle:hover { background: #e7f2fa; }
    .tree-toggle.placeholder { cursor: default; }
    .tree-label { flex: 1; min-width: 0; }
    .tree-group {
      padding: 8px 10px;
      border-radius: 10px;
      font-weight: 700;
      color: var(--setup-primary);
      background: #f2f8fc;
      border: 1px solid #d8eaf6;
      margin-top: 8px;
    }
    .node {
      cursor: pointer;
      padding: 6px 8px;
      border-radius: 10px;
      transition: background 0.15s ease, box-shadow 0.15s ease;
    }
    .node:hover { background: #f5f9fc; }
    .node.selected {
      background: #e6f2fb;
      box-shadow: inset 3px 0 0 var(--setup-primary);
    }
    .children { padding-left: 18px; }
    .resource {
      padding: 8px 10px;
      border-radius: 10px;
      cursor: pointer;
      border: 1px solid transparent;
      transition: background 0.15s ease, border-color 0.15s ease;
    }
    .resource:hover {
      background: #f8fbfd;
      border-color: #e0edf7;
    }
    .resource.selected {
      background: #e9f5ff;
      border-color: #b7daf4;
    }
    .resource small { color: var(--setup-text-muted); display: block; }
    .tree-status-badge {
      display: inline-flex;
      align-items: center;
      margin-left: 8px;
      padding: 2px 8px;
      border-radius: 999px;
      font-size: 0.72rem;
      font-weight: 700;
      letter-spacing: 0.02em;
      text-transform: uppercase;
      border: 1px solid #d8eaf6;
      background: rgba(255, 255, 255, 0.92);
      color: var(--setup-text-muted);
    }
    .tree-status-badge.managed {
      border-color: #bde2ca;
      background: #edf9f1;
      color: #22663c;
    }
    .tree-status-badge.remote {
      border-color: #f0d7a9;
      background: #fff6e6;
      color: #8a6200;
    }
    .muted { color: var(--setup-text-muted); }
    textarea {
      width: 100%;
      min-height: 320px;
      box-sizing: border-box;
      font: 13px/1.5 ui-monospace, monospace;
      padding: 12px;
      border: 1px solid var(--setup-border);
      border-radius: 10px;
      background: #fcfdff;
    }
    input, select {
      width: 100%;
      box-sizing: border-box;
      padding: 10px 12px;
      border: 1px solid var(--setup-border);
      border-radius: 10px;
      background: white;
      color: var(--setup-text);
    }
    select[multiple],
    textarea.repeatable-field {
      min-height: 0;
      padding: 8px 10px;
      font-size: 0.92rem;
      line-height: 1.35;
    }
    select[multiple] {
      max-height: 9.5rem;
    }
    textarea.repeatable-field {
      resize: vertical;
    }
    button {
      margin-top: 12px;
      padding: 9px 14px;
      border: 1px solid var(--setup-primary);
      background: var(--setup-primary);
      color: white;
      border-radius: 10px;
      cursor: pointer;
      font-weight: 600;
      box-shadow: 0 4px 12px rgba(0, 117, 190, 0.18);
    }
    button:hover { background: #0068a9; }
    button:disabled { opacity: 0.6; cursor: progress; }
    .link-button {
      margin-top: 0;
      padding: 0;
      border: 0;
      background: none;
      color: var(--setup-primary);
      border-radius: 0;
      box-shadow: none;
    }
    .link-button:hover { text-decoration: underline; }
    .directive-list { margin: 12px 0; padding: 0; list-style: none; }
    .directive-list li { padding: 8px 0; border-bottom: 1px solid #e7edf3; }
    .field-hint-list { display: grid; gap: 10px; margin: 12px 0; }
    .field-hint-row { display: grid; gap: 4px; }
    .schedule-run-editor {
      display: grid;
      gap: 10px;
      margin-top: 6px;
    }
    .schedule-run-toolbar {
      display: flex;
      justify-content: space-between;
      align-items: center;
      gap: 8px;
    }
    .schedule-run-rows {
      display: grid;
      gap: 10px;
    }
    .schedule-run-row {
      border: 1px solid #d8eaf6;
      border-radius: 12px;
      background: #fbfdff;
      padding: 10px;
      display: grid;
      gap: 10px;
    }
    .schedule-run-row-header {
      display: flex;
      justify-content: space-between;
      align-items: center;
      gap: 8px;
      flex-wrap: wrap;
    }
    .schedule-run-row-actions {
      display: flex;
      gap: 6px;
      flex-wrap: wrap;
    }
    .schedule-run-grid {
      display: grid;
      gap: 10px;
      grid-template-columns: repeat(auto-fit, minmax(150px, 1fr));
    }
    .schedule-run-grid label {
      display: grid;
      gap: 4px;
    }
    .schedule-run-grid label > span,
    .schedule-run-advanced label > span {
      font-size: 0.82rem;
      font-weight: 600;
      color: var(--setup-text-muted);
    }
    .schedule-run-advanced {
      display: grid;
      gap: 6px;
    }
    .schedule-run-advanced textarea {
      min-height: 96px;
    }
    .schedule-run-chip {
      display: inline-flex;
      align-items: center;
      padding: 2px 8px;
      border-radius: 999px;
      border: 1px solid #d8eaf6;
      background: rgba(255, 255, 255, 0.92);
      color: var(--setup-text-muted);
      font-size: 0.72rem;
      font-weight: 700;
      text-transform: uppercase;
    }
    .schedule-run-chip.advanced {
      border-color: #f0d7a9;
      background: #fff6e6;
      color: #8a6200;
    }
    .schedule-run-preview {
      border: 1px solid #e2ebf2;
      border-radius: 12px;
      background: #f8fbfd;
      padding: 10px 12px;
      display: grid;
      gap: 8px;
    }
    .schedule-run-preview h5 {
      margin: 0;
      color: var(--setup-primary);
      font-size: 0.92rem;
    }
    .schedule-run-preview-grid {
      display: grid;
      gap: 8px;
      grid-template-columns: repeat(auto-fit, minmax(220px, 1fr));
    }
    .schedule-run-preview-card {
      border: 1px solid #d8eaf6;
      border-radius: 10px;
      background: rgba(255, 255, 255, 0.92);
      padding: 8px 10px;
    }
    .schedule-run-preview-card ul,
    .schedule-run-timeline ul {
      margin: 8px 0 0;
      padding-left: 18px;
    }
    .schedule-run-preview-card li,
    .schedule-run-timeline li {
      margin: 4px 0;
    }
    .schedule-run-overlap {
      color: #8a6200;
      font-weight: 600;
    }
    .schedule-run-when-builder {
      grid-column: 1 / -1;
      border: 1px solid #d8eaf6;
      border-radius: 10px;
      background: rgba(255, 255, 255, 0.88);
      padding: 10px;
      display: grid;
      gap: 10px;
    }
    .schedule-run-when-grid {
      display: grid;
      gap: 10px;
      grid-template-columns: repeat(auto-fit, minmax(180px, 1fr));
    }
    .schedule-run-when-group {
      display: grid;
      gap: 6px;
    }
    .schedule-run-when-group > span {
      font-size: 0.82rem;
      font-weight: 600;
      color: var(--setup-text-muted);
    }
    .schedule-run-chip-grid {
      display: flex;
      flex-wrap: wrap;
      gap: 6px;
    }
    .schedule-run-chip-toggle {
      position: relative;
      display: inline-flex;
      align-items: center;
    }
    .schedule-run-chip-toggle input {
      position: absolute;
      opacity: 0;
      pointer-events: none;
    }
    .schedule-run-chip-toggle span {
      display: inline-flex;
      align-items: center;
      padding: 4px 10px;
      border-radius: 999px;
      border: 1px solid #d8eaf6;
      background: #ffffff;
      color: var(--setup-text);
      font-size: 0.8rem;
      cursor: pointer;
      user-select: none;
    }
    .schedule-run-chip-toggle input:checked + span {
      border-color: #7db6de;
      background: #e8f4fc;
      color: var(--setup-primary);
      font-weight: 600;
    }
    .schedule-run-summary {
      font-size: 0.83rem;
      color: var(--setup-text-muted);
    }
    .schedule-run-when-fallback {
      grid-column: 1 / -1;
      display: grid;
      gap: 6px;
    }
    .validation-list { margin: 12px 0; padding: 0; list-style: none; }
    .validation-list li { padding: 8px 10px; border-radius: 10px; margin-bottom: 8px; }
     .validation-list li.warning { background: #fff5d6; color: #8a6200; }
     .validation-list li.error { background: #fde7ea; color: #a12638; }
     .relationship-view { display: grid; gap: 16px; }
     .relationship-graph-card {
        border: 1px solid #d8eaf6;
        border-radius: 14px;
        background: linear-gradient(180deg, #f8fcff 0%, #f2f8fc 100%);
        padding: 10px 12px;
      }
      .relationship-graph-card h5 {
        margin: 0 0 4px;
        color: var(--setup-primary);
        font-size: 0.92rem;
      }
       .relationship-graph {
         width: 100%;
         overflow-x: auto;
         margin-top: 8px;
       }
        .relationship-graph svg {
          display: block;
          width: auto;
          max-width: 100%;
          height: auto;
          overflow: visible;
        }
        .relationship-graph-object,
        .relationship-graph-resource,
        .relationship-graph-edge {
          transition: opacity 140ms ease;
        }
        .relationship-graph.has-active-highlight .relationship-graph-object,
        .relationship-graph.has-active-highlight .relationship-graph-resource,
        .relationship-graph.has-active-highlight .relationship-graph-edge {
          opacity: 0.22;
        }
        .relationship-graph.has-active-highlight .relationship-graph-object.is-highlighted,
        .relationship-graph.has-active-highlight .relationship-graph-resource.is-highlighted,
        .relationship-graph.has-active-highlight .relationship-graph-edge.is-highlighted {
          opacity: 1;
        }
        .relationship-graph-object.is-highlighted rect {
          stroke: var(--setup-primary);
          stroke-width: 2.1;
        }
        .relationship-graph-resource.is-highlighted rect {
          stroke: var(--setup-accent);
          stroke-width: 1.9;
        }
        .relationship-graph-resource[data-resource-id] {
          cursor: pointer;
        }
        .relationship-graph-edge.is-highlighted path {
          stroke-width: 3.2;
        }
        .relationship-graph-edge-source {
          opacity: 0.96;
        }
        .relationship-graph-edge-label rect {
          fill: rgba(255, 255, 255, 0.94);
          stroke: #d8eaf6;
          stroke-width: 1;
        }
        .relationship-graph-edge-label text {
          fill: #22303c;
          font-size: 8.5px;
          font-weight: 600;
          text-anchor: middle;
          dominant-baseline: middle;
        }
        .relationship-graph-note {
          margin-top: 6px;
        }
       .relationship-graph-list {
         display: flex;
         flex-wrap: wrap;
         gap: 6px;
         margin-top: 6px;
       }
       .relationship-graph-node-link {
         margin-top: 0;
         padding: 4px 8px;
         border: 1px solid #d8eaf6;
         border-radius: 999px;
         background: rgba(255, 255, 255, 0.9);
         color: var(--setup-primary);
         box-shadow: none;
         font-size: 0.83rem;
       }
       .relationship-legend {
         display: flex;
         flex-wrap: wrap;
         gap: 6px;
        margin-top: 8px;
      }
      .relationship-chip {
        display: inline-flex;
        align-items: center;
        gap: 5px;
        padding: 3px 8px;
        border-radius: 999px;
        background: rgba(255, 255, 255, 0.82);
        border: 1px solid #d8eaf6;
        font-size: 0.8rem;
        color: var(--setup-text);
      }
       .relationship-chip-swatch {
         width: 8px;
         height: 8px;
         border-radius: 999px;
         flex: 0 0 8px;
       }
      .table-scroll { overflow-x: auto; }
     table { width: 100%; border-collapse: collapse; margin: 12px 0; background: #fbfdff; border-radius: 10px; overflow: hidden; }
     th, td { padding: 10px; border-bottom: 1px solid #e7edf3; text-align: left; vertical-align: top; }
     th { background: #f2f8fc; color: var(--setup-primary); font-weight: 700; }
     pre {
       white-space: pre-wrap;
       word-break: break-word;
       background: #f7fafc;
       border: 1px solid #e2ebf2;
       padding: 12px;
       border-radius: 10px;
     }
    </style>
</head>
<body>
  <header>
    <div class="app-title"><span class="icon fill" aria-hidden="true"><svg viewBox="0 0 24 24"><path d="M4 7a2 2 0 0 1 2-2h4l2 2h6a2 2 0 0 1 2 2v1H4V7Z"></path><path d="M4 10h16v7a2 2 0 0 1-2 2H6a2 2 0 0 1-2-2v-7Z"></path></svg></span><strong>Bareos Config</strong></div> — standalone configuration service bootstrap
  </header>
  <div class="layout">
    <section class="pane">
      <h3><span class="section-heading"><span class="icon" aria-hidden="true"><svg viewBox="0 0 24 24"><path d="M3 21h18"></path><path d="M5 21V7l7-4 7 4v14"></path><path d="M9 10h.01"></path><path d="M9 14h.01"></path><path d="M15 10h.01"></path><path d="M15 14h.01"></path></svg></span>Datacenter</span></h3>
      <div id="tree" class="muted">Loading...</div>
    </section>
    <section class="pane">
      <h3><span class="section-heading"><span class="icon" aria-hidden="true"><svg viewBox="0 0 24 24"><path d="M14 3h-4a2 2 0 0 0-2 2v14l4-2 4 2V5a2 2 0 0 0-2-2Z"></path></svg></span>Details</span></h3>
      <div id="details" class="muted">Select a node or resource from the tree.</div>
    </section>
  </div>
  <script>
    const treeEl = document.getElementById('tree');
    const detailsEl = document.getElementById('details');
    let currentTreeData = null;
    let selectedNodeId = 'datacenter';
    let selectedResourceId = '';
    const loadedNodeResources = new Map();
    const expandedTreeIds = new Set([
      'tree-group-directors',
      'tree-group-storages',
      'tree-group-clients',
      'tree-group-consoles',
    ]);

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

    function iconMarkup(name, options = {}) {
      const kind = options.fill ? 'icon fill' : 'icon';
      const icons = {
        datacenter: '<svg viewBox="0 0 24 24"><path d="M3 21h18"></path><path d="M5 21V7l7-4 7 4v14"></path><path d="M9 10h.01"></path><path d="M9 14h.01"></path><path d="M15 10h.01"></path><path d="M15 14h.01"></path></svg>',
        directors: '<svg viewBox="0 0 24 24"><path d="M4 20h16"></path><path d="M6 20V8h12v12"></path><path d="M9 4h6v4H9z"></path></svg>',
        storages: '<svg viewBox="0 0 24 24"><ellipse cx="12" cy="6" rx="7" ry="3"></ellipse><path d="M5 6v12c0 1.7 3.1 3 7 3s7-1.3 7-3V6"></path><path d="M5 12c0 1.7 3.1 3 7 3s7-1.3 7-3"></path></svg>',
        clients: '<svg viewBox="0 0 24 24"><rect x="3" y="5" width="18" height="12" rx="2"></rect><path d="M8 21h8"></path><path d="M12 17v4"></path></svg>',
        consoles: '<svg viewBox="0 0 24 24"><rect x="3" y="4" width="18" height="14" rx="2"></rect><path d="m7 9 2 2-2 2"></path><path d="M11 13h4"></path></svg>',
        director: '<svg viewBox="0 0 24 24"><path d="M4 20h16"></path><path d="M6 20V8h12v12"></path><path d="M9 4h6v4H9z"></path></svg>',
        'storage-daemon': '<svg viewBox="0 0 24 24"><ellipse cx="12" cy="6" rx="7" ry="3"></ellipse><path d="M5 6v12c0 1.7 3.1 3 7 3s7-1.3 7-3V6"></path><path d="M5 12c0 1.7 3.1 3 7 3s7-1.3 7-3"></path></svg>',
        'file-daemon': '<svg viewBox="0 0 24 24"><rect x="3" y="5" width="18" height="12" rx="2"></rect><path d="M8 21h8"></path><path d="M12 17v4"></path></svg>',
        client: '<svg viewBox="0 0 24 24"><rect x="3" y="5" width="18" height="12" rx="2"></rect><path d="M8 21h8"></path><path d="M12 17v4"></path></svg>',
        console: '<svg viewBox="0 0 24 24"><rect x="3" y="4" width="18" height="14" rx="2"></rect><path d="m7 9 2 2-2 2"></path><path d="M11 13h4"></path></svg>',
        resource: '<svg viewBox="0 0 24 24"><path d="M8 3h6l5 5v13H8z"></path><path d="M14 3v5h5"></path></svg>',
        configuration: '<svg viewBox="0 0 24 24"><path d="M8 3h6l5 5v13H8z"></path><path d="M14 3v5h5"></path></svg>',
      };
      return `<span class="${kind}" aria-hidden="true">${icons[name] || icons.resource}</span>`;
    }

    function iconForGroup(groupId) {
      const byGroup = {
        'tree-group-directors': 'directors',
        'tree-group-storages': 'storages',
        'tree-group-clients': 'clients',
        'tree-group-consoles': 'consoles',
      };
      return iconMarkup(byGroup[groupId] || 'resource');
    }

    function iconForNode(node) {
      return iconMarkup(node.kind || 'resource');
    }

    function iconForResource(resource) {
      return iconMarkup(resource.type || 'resource');
    }

    function clientManagementState(node) {
      if (node.kind === 'file-daemon') {
        return {
          badgeClass: 'managed',
          badgeLabel: 'managed',
          description: 'fully managed client',
        };
      }
      if (node.kind === 'client') {
        return {
          badgeClass: 'remote',
          badgeLabel: 'remote',
          description: 'remote client only',
        };
      }
      return null;
    }

    function renderClientManagementBadge(node) {
      const state = clientManagementState(node);
      if (!state) {
        return '';
      }
      return `<span class="tree-status-badge ${escapeHtml(state.badgeClass)}">${escapeHtml(state.badgeLabel)}</span>`;
    }

    function splitRepeatableFieldValues(value) {
      return `${value || ''}`
        .split('\n')
        .map((entry) => entry.trim())
        .filter(Boolean);
    }

    function detectRepeatableFieldQuote(value) {
      const firstValue = splitRepeatableFieldValues(value)[0] || '';
      return detectSurroundingQuote(firstValue);
    }

    function normalizeRepeatableFieldValues(value) {
      return splitRepeatableFieldValues(value)
        .map((entry) => stripSurroundingQuotes(entry));
    }

    const scheduleRunOverrideKeys = [
      'Pool',
      'FullPool',
      'IncrementalPool',
      'DifferentialPool',
      'NextPool',
      'Storage',
      'Messages',
      'Priority',
      'SpoolData',
      'Accurate',
      'MaxRunSchedTime',
    ];

    const scheduleRunBooleanOverrideKeys = new Set([
      'Accurate',
      'SpoolData',
    ]);

    const scheduleRunKnownLevels = new Map([
      ['full', 'Full'],
      ['differential', 'Differential'],
      ['incremental', 'Incremental'],
      ['virtualfull', 'VirtualFull'],
      ['base', 'Base'],
      ['since', 'Since'],
      ['verifycatalog', 'VerifyCatalog'],
      ['verifydata', 'VerifyData'],
      ['verifyvolume', 'VerifyVolume'],
      ['initcatalog', 'InitCatalog'],
      ['volumetocatalog', 'VolumeToCatalog'],
      ['disktocatalog', 'DiskToCatalog'],
    ]);

    const scheduleRunLevelOptions = [
      'Full',
      'Differential',
      'Incremental',
      'VirtualFull',
      'Base',
      'Since',
      'VerifyCatalog',
      'VerifyData',
      'VerifyVolume',
      'InitCatalog',
      'VolumeToCatalog',
      'DiskToCatalog',
    ];

    const scheduleWeekdayTokens = new Map([
      ['sun', 0], ['sunday', 0],
      ['mon', 1], ['monday', 1],
      ['tue', 2], ['tuesday', 2],
      ['wed', 3], ['wednesday', 3],
      ['thu', 4], ['thursday', 4],
      ['fri', 5], ['friday', 5],
      ['sat', 6], ['saturday', 6],
    ]);

    const scheduleMonthTokens = new Map([
      ['jan', 0], ['january', 0],
      ['feb', 1], ['february', 1],
      ['mar', 2], ['march', 2],
      ['apr', 3], ['april', 3],
      ['may', 4],
      ['jun', 5], ['june', 5],
      ['jul', 6], ['july', 6],
      ['aug', 7], ['august', 7],
      ['sep', 8], ['september', 8],
      ['oct', 9], ['october', 9],
      ['nov', 10], ['november', 10],
      ['dec', 11], ['december', 11],
    ]);

    const scheduleOrdinalTokens = new Map([
      ['1st', 0], ['first', 0],
      ['2nd', 1], ['second', 1],
      ['3rd', 2], ['third', 2],
      ['4th', 3], ['fourth', 3],
      ['5th', 4], ['fifth', 4],
    ]);

    const scheduleWeekdayOrder = ['sun', 'mon', 'tue', 'wed', 'thu', 'fri', 'sat'];
    const scheduleWeekdayLabels = {
      sun: 'Sun',
      mon: 'Mon',
      tue: 'Tue',
      wed: 'Wed',
      thu: 'Thu',
      fri: 'Fri',
      sat: 'Sat',
    };
    const scheduleMonthOrder = ['jan', 'feb', 'mar', 'apr', 'may', 'jun',
      'jul', 'aug', 'sep', 'oct', 'nov', 'dec'];
    const scheduleMonthLabels = {
      jan: 'Jan',
      feb: 'Feb',
      mar: 'Mar',
      apr: 'Apr',
      may: 'May',
      jun: 'Jun',
      jul: 'Jul',
      aug: 'Aug',
      sep: 'Sep',
      oct: 'Oct',
      nov: 'Nov',
      dec: 'Dec',
    };
    const scheduleOrdinalOptions = ['1st', '2nd', '3rd', '4th', '5th', 'last'];

    function createEmptyScheduleRunOverrides() {
      return scheduleRunOverrideKeys.reduce((result, key) => {
        result[key] = '';
        return result;
      }, {});
    }

    function createDefaultScheduleRunRow() {
      return {
        mode: 'structured',
        raw: '',
        dirty: false,
        level: '',
        levelStyle: 'keyword',
        scheduleExpression: '',
        overrides: createEmptyScheduleRunOverrides(),
      };
    }

    function stripScheduleRunQuotes(value) {
      return stripSurroundingQuotes((value || '').trim());
    }

    function quoteScheduleRunValue(value) {
      const trimmed = `${value || ''}`.trim();
      if (!trimmed) {
        return '';
      }
      if (detectSurroundingQuote(trimmed)) {
        return trimmed;
      }
      return /[\s,]/.test(trimmed) ? `"${trimmed}"` : trimmed;
    }

    function tokenizeScheduleRunLine(line) {
      const tokens = [];
      const source = `${line || ''}`;
      let current = '';
      let quote = '';
      for (let index = 0; index < source.length; index += 1) {
        const character = source[index];
        if (quote) {
          current += character;
          if (character === quote) {
            quote = '';
          }
          continue;
        }
        if (character === '"' || character === '\'') {
          quote = character;
          current += character;
          continue;
        }
        if (/\s|,/.test(character)) {
          if (current) {
            tokens.push(current);
            current = '';
          }
          continue;
        }
        current += character;
      }
      if (current) {
        tokens.push(current);
      }
      return tokens;
    }

    function parseScheduleRunAssignment(token) {
      const separator = token.indexOf('=');
      if (separator < 0) {
        return null;
      }
      return {
        key: token.slice(0, separator).trim(),
        value: token.slice(separator + 1).trim(),
      };
    }

    function normalizeScheduleRunToken(token) {
      return stripScheduleRunQuotes(token).trim().toLowerCase();
    }

    function normalizeScheduleDirectiveName(name) {
      return `${name || ''}`
        .trim()
        .replace(/[^A-Za-z0-9]/g, '')
        .toLowerCase();
    }

    function scheduleCanonicalWeekday(token) {
      const normalized = normalizeScheduleRunToken(token);
      return scheduleWeekdayOrder.find(
        (weekday) => scheduleWeekdayTokens.get(normalized) === scheduleWeekdayTokens.get(weekday),
      ) || '';
    }

    function scheduleCanonicalMonth(token) {
      const normalized = normalizeScheduleRunToken(token);
      return scheduleMonthOrder.find(
        (month) => scheduleMonthTokens.get(normalized) === scheduleMonthTokens.get(month),
      ) || '';
    }

    function formatScheduleTimeValue(hour, minute) {
      return `${String(hour).padStart(2, '0')}:${String(minute).padStart(2, '0')}`;
    }

    function createDefaultScheduleWhenState() {
      return {
        supported: true,
        unsupportedReason: '',
        frequency: 'daily',
        weekdays: [],
        months: [],
        monthDays: [],
        ordinal: '',
        ordinalWeekday: '',
        time: '',
        hourlyMinute: '0',
        hasExplicitTime: false,
      };
    }

    function sortedUniqueNumbers(values, minimum, maximum) {
      return Array.from(new Set((values || [])
        .map((value) => Number(value))
        .filter((value) => Number.isInteger(value) && value >= minimum && value <= maximum)))
        .sort((left, right) => left - right);
    }

    function parseMonthDaysInput(value) {
      return sortedUniqueNumbers(`${value || ''}`.split(/[,\s]+/), 1, 31);
    }

    function parseScheduleWhenState(expression) {
      const tokens = tokenizeScheduleRunLine(expression || '');
      const state = createDefaultScheduleWhenState();
      let explicitFrequency = '';
      for (let index = 0; index < tokens.length; index += 1) {
        const token = normalizeScheduleRunToken(tokens[index]);
        if (!token || token === 'on' || token === 'pooltime') {
          continue;
        }
        if (token === 'at') {
          const parsedTime = parseScheduleTimeToken(tokens[index + 1]);
          if (!parsedTime) {
            state.supported = false;
            state.unsupportedReason = 'This schedule time cannot be represented by the guided editor yet.';
            return state;
          }
          state.time = formatScheduleTimeValue(parsedTime.hour, parsedTime.minute);
          state.hourlyMinute = String(parsedTime.minute);
          state.hasExplicitTime = true;
          index += 1;
          continue;
        }
        if (token === 'hourly') {
          explicitFrequency = 'hourly';
          continue;
        }
        if (token === 'daily' || token === 'weekly' || token === 'monthly') {
          explicitFrequency = token;
          continue;
        }
        if (token === 'last') {
          const weekday = scheduleCanonicalWeekday(tokens[index + 1]);
          if (!weekday) {
            state.supported = false;
            state.unsupportedReason = 'Use raw mode for this `last` expression.';
            return state;
          }
          state.ordinal = 'last';
          state.ordinalWeekday = weekday;
          index += 1;
          continue;
        }
        if (scheduleOrdinalTokens.has(token)) {
          const weekday = scheduleCanonicalWeekday(tokens[index + 1]);
          if (!weekday) {
            state.supported = false;
            state.unsupportedReason = 'Use raw mode for this ordinal expression.';
            return state;
          }
          const ordinalIndex = scheduleOrdinalTokens.get(token);
          state.ordinal = ['1st', '2nd', '3rd', '4th', '5th'][ordinalIndex] || '';
          state.ordinalWeekday = weekday;
          index += 1;
          continue;
        }
        if (/^w\d{1,2}$/.test(token) || token.includes('/')) {
          state.supported = false;
          state.unsupportedReason = 'Use raw mode for week-of-year or modulo expressions.';
          return state;
        }
        if (token.includes('-')) {
          const [startToken, endToken] = token.split('-', 2).map((part) => part.trim().toLowerCase());
          const weekdayRange = expandScheduleTokenRange(startToken, endToken, scheduleWeekdayTokens);
          if (weekdayRange) {
            weekdayRange.forEach((weekdayValue) => {
              const weekday = scheduleWeekdayOrder.find(
                (candidate) => scheduleWeekdayTokens.get(candidate) === weekdayValue,
              );
              if (weekday) state.weekdays.push(weekday);
            });
            continue;
          }
          const monthRange = expandScheduleTokenRange(startToken, endToken, scheduleMonthTokens);
          if (monthRange) {
            monthRange.forEach((monthValue) => {
              const month = scheduleMonthOrder.find(
                (candidate) => scheduleMonthTokens.get(candidate) === monthValue,
              );
              if (month) state.months.push(month);
            });
            continue;
          }
          if (/^\d{1,2}$/.test(startToken) && /^\d{1,2}$/.test(endToken)) {
            const start = Number(startToken);
            const end = Number(endToken);
            for (let current = start; current <= end; current += 1) {
              state.monthDays.push(current);
            }
            continue;
          }
          state.supported = false;
          state.unsupportedReason = 'Use raw mode for this range expression.';
          return state;
        }
        if (/^\d{1,2}$/.test(token)) {
          state.monthDays.push(Number(token));
          continue;
        }
        const weekday = scheduleCanonicalWeekday(token);
        if (weekday) {
          state.weekdays.push(weekday);
          continue;
        }
        const month = scheduleCanonicalMonth(token);
        if (month) {
          state.months.push(month);
          continue;
        }
        state.supported = false;
        state.unsupportedReason = `Use raw mode for \`${stripScheduleRunQuotes(tokens[index])}\`.`;
        return state;
      }

      state.weekdays = Array.from(new Set(state.weekdays));
      state.months = Array.from(new Set(state.months));
      state.monthDays = sortedUniqueNumbers(state.monthDays, 1, 31);

      if (explicitFrequency) {
        state.frequency = explicitFrequency;
      } else if (state.months.length) {
        state.frequency = 'yearly';
      } else if (state.ordinal || state.monthDays.length) {
        state.frequency = 'monthly';
      } else if (state.weekdays.length) {
        state.frequency = 'weekly';
      } else {
        state.frequency = 'daily';
      }

      return state;
    }

    function serializeScheduleWhenState(state) {
      const parts = [];
      const frequency = `${state.frequency || 'daily'}`.trim();
      const weekdays = (state.weekdays || []).filter(Boolean)
        .sort((left, right) => scheduleWeekdayOrder.indexOf(left) - scheduleWeekdayOrder.indexOf(right));
      const months = (state.months || []).filter(Boolean)
        .sort((left, right) => scheduleMonthOrder.indexOf(left) - scheduleMonthOrder.indexOf(right));
      const monthDays = sortedUniqueNumbers(state.monthDays, 1, 31);

      if (frequency === 'hourly') {
        parts.push('hourly');
        const minute = Number(state.hourlyMinute);
        if (state.hasExplicitTime || (Number.isInteger(minute) && minute !== 0)) {
          parts.push('at', `00:${String(Number.isInteger(minute) ? minute : 0).padStart(2, '0')}`);
        }
        return parts.join(' ').trim();
      }

      if (frequency === 'daily') {
        parts.push('daily');
      } else if (frequency === 'weekly') {
        if (weekdays.length) {
          parts.push(...weekdays);
        } else {
          parts.push('weekly');
        }
      } else if (frequency === 'monthly') {
        if (state.ordinal && state.ordinalWeekday) {
          parts.push(state.ordinal, state.ordinalWeekday);
        } else if (monthDays.length) {
          parts.push(...monthDays.map((value) => String(value)));
        } else {
          parts.push('monthly');
        }
      } else if (frequency === 'yearly') {
        if (months.length) parts.push(...months);
        if (state.ordinal && state.ordinalWeekday) {
          parts.push(state.ordinal, state.ordinalWeekday);
        } else if (monthDays.length) {
          parts.push(...monthDays.map((value) => String(value)));
        }
      } else {
        if (months.length) parts.push(...months);
        if (state.ordinal && state.ordinalWeekday) {
          parts.push(state.ordinal, state.ordinalWeekday);
        } else {
          if (monthDays.length) parts.push(...monthDays.map((value) => String(value)));
          if (weekdays.length) parts.push(...weekdays);
        }
      }

      if (state.hasExplicitTime && state.time) {
        parts.push('at', state.time);
      }
      return parts.join(' ').trim();
    }

    function parseScheduleRunLine(line) {
      const raw = `${line || ''}`.trim();
      const row = createDefaultScheduleRunRow();
      row.raw = raw;
      if (!raw) {
        return row;
      }

      const scheduleTokens = [];
      const tokens = tokenizeScheduleRunLine(raw);
      for (const token of tokens) {
        const assignment = parseScheduleRunAssignment(token);
        if (assignment) {
          const normalizedKey = normalizeScheduleDirectiveName(assignment.key);
          if (normalizedKey === 'level') {
            row.level = stripScheduleRunQuotes(assignment.value);
            row.levelStyle = 'keyword';
            continue;
          }
          const overrideKey = scheduleRunOverrideKeys.find(
            (key) => normalizeScheduleDirectiveName(key) === normalizedKey,
          );
          if (overrideKey) {
            row.overrides[overrideKey] = stripScheduleRunQuotes(assignment.value);
            continue;
          }
          return {
            ...row,
            mode: 'advanced',
            raw,
          };
        }

        const normalizedToken = normalizeScheduleRunToken(token);
        if (!row.level && scheduleRunKnownLevels.has(normalizedToken)) {
          row.level = scheduleRunKnownLevels.get(normalizedToken);
          row.levelStyle = 'legacy';
          continue;
        }
        scheduleTokens.push(token);
      }

      row.scheduleExpression = scheduleTokens.join(' ').trim();
      return row;
    }

    function serializeScheduleRunLine(row) {
      if (!row) {
        return '';
      }
      if (row.mode === 'advanced') {
        return `${row.raw || ''}`.trim();
      }
      if (!row.dirty && row.raw) {
        return `${row.raw}`.trim();
      }
      const parts = [];
      const level = `${row.level || ''}`.trim();
      if (level) {
        parts.push(
          row.levelStyle === 'legacy'
            ? quoteScheduleRunValue(level)
            : `Level=${quoteScheduleRunValue(level)}`,
        );
      }
      scheduleRunOverrideKeys.forEach((key) => {
        const value = `${row.overrides?.[key] || ''}`.trim();
        if (value) {
          parts.push(`${key}=${quoteScheduleRunValue(value)}`);
        }
      });
      const expression = `${row.scheduleExpression || ''}`.trim();
      if (expression) {
        parts.push(expression);
      }
      return parts.join(' ').trim();
    }

    function normalizeScheduleBooleanValue(value) {
      const normalized = `${value || ''}`.trim().toLowerCase();
      if (normalized === 'yes' || normalized === 'true' || normalized === '1') {
        return 'yes';
      }
      if (normalized === 'no' || normalized === 'false' || normalized === '0') {
        return 'no';
      }
      return '';
    }

    function parseScheduleTimeToken(token) {
      const match = `${token || ''}`.trim().match(/^(\d{1,2}):(\d{2})(am|pm)?$/i);
      if (!match) {
        return null;
      }
      let hour = Number(match[1]);
      const minute = Number(match[2]);
      const suffix = (match[3] || '').toLowerCase();
      if (!Number.isInteger(hour) || !Number.isInteger(minute) || minute < 0 || minute > 59) {
        return null;
      }
      if (suffix === 'pm' && hour !== 12) {
        hour += 12;
      } else if (suffix === 'am' && hour === 12) {
        hour = 0;
      }
      if (hour < 0 || hour > 23) {
        return null;
      }
      return { hour, minute };
    }

    function expandScheduleTokenRange(startToken, endToken, tokenMap) {
      const start = tokenMap.get(startToken);
      const end = tokenMap.get(endToken);
      if (start === undefined || end === undefined) {
        return null;
      }
      const values = [];
      const wrap = Math.max(...tokenMap.values()) + 1;
      for (let current = start; ; current = (current + 1) % wrap) {
        values.push(current);
        if (current === end) {
          break;
        }
      }
      return values;
    }

    function getScheduleWeekOfMonth(date) {
      return Math.floor((date.getDate() - 1) / 7);
    }

    function isScheduleLastSevenDaysOfMonth(date) {
      const lastDay = new Date(date.getFullYear(), date.getMonth() + 1, 0).getDate();
      return date.getDate() > lastDay - 7;
    }

    function getScheduleWeekOfYear(date) {
      const start = new Date(date.getFullYear(), 0, 1);
      const startMidnight = new Date(start.getFullYear(), start.getMonth(), start.getDate());
      const currentMidnight = new Date(date.getFullYear(), date.getMonth(), date.getDate());
      const days = Math.floor((currentMidnight - startMidnight) / 86400000);
      return Math.floor((days + startMidnight.getDay()) / 7);
    }

    function parseScheduleExpression(expression) {
      const tokens = tokenizeScheduleRunLine(expression || '');
      const spec = {
        hourly: false,
        minute: 0,
        hours: null,
        weekdays: null,
        months: null,
        monthDays: null,
        weeksOfMonth: null,
        weeksOfYear: null,
        lastSevenDays: false,
        valid: true,
        unsupportedReason: '',
      };

      const addToSet = (name, value) => {
        if (!spec[name]) {
          spec[name] = new Set();
        }
        spec[name].add(value);
      };

      for (let index = 0; index < tokens.length; index += 1) {
        const token = normalizeScheduleRunToken(tokens[index]);
        if (!token || token === 'on' || token === 'pooltime') {
          continue;
        }
        if (token === 'at') {
          const parsedTime = parseScheduleTimeToken(tokens[index + 1]);
          if (!parsedTime) {
            spec.valid = false;
            spec.unsupportedReason = 'Preview needs an `at HH:MM` time.';
            return spec;
          }
          spec.minute = parsedTime.minute;
          spec.hours = new Set([parsedTime.hour]);
          index += 1;
          continue;
        }
        if (token === 'hourly') {
          spec.hourly = true;
          spec.hours = new Set(Array.from({ length: 24 }, (_, hour) => hour));
          continue;
        }
        if (token === 'daily' || token === 'weekly' || token === 'monthly') {
          continue;
        }
        if (token === 'last') {
          const nextToken = normalizeScheduleRunToken(tokens[index + 1]);
          if (!scheduleWeekdayTokens.has(nextToken)) {
            spec.valid = false;
            spec.unsupportedReason = 'Preview currently supports `last` only with a weekday.';
            return spec;
          }
          spec.lastSevenDays = true;
          addToSet('weekdays', scheduleWeekdayTokens.get(nextToken));
          index += 1;
          continue;
        }
        if (scheduleOrdinalTokens.has(token)) {
          const nextToken = normalizeScheduleRunToken(tokens[index + 1]);
          if (!scheduleWeekdayTokens.has(nextToken)) {
            spec.valid = false;
            spec.unsupportedReason = 'Preview needs an ordinal like `1st mon`.';
            return spec;
          }
          addToSet('weeksOfMonth', scheduleOrdinalTokens.get(token));
          addToSet('weekdays', scheduleWeekdayTokens.get(nextToken));
          index += 1;
          continue;
        }
        if (/^w\d{1,2}$/.test(token)) {
          addToSet('weeksOfYear', Number(token.slice(1)));
          continue;
        }
        if (/^\d{1,2}$/.test(token)) {
          const day = Number(token);
          if (day < 1 || day > 31) {
            spec.valid = false;
            spec.unsupportedReason = 'Preview only supports month days between 1 and 31.';
            return spec;
          }
          addToSet('monthDays', day);
          continue;
        }
        if (scheduleWeekdayTokens.has(token)) {
          addToSet('weekdays', scheduleWeekdayTokens.get(token));
          continue;
        }
        if (scheduleMonthTokens.has(token)) {
          addToSet('months', scheduleMonthTokens.get(token));
          continue;
        }
        if (token.includes('/')) {
          spec.valid = false;
          spec.unsupportedReason = 'Preview is not available yet for modulo expressions.';
          return spec;
        }
        if (token.includes('-')) {
          const [startToken, endToken] = token.split('-', 2).map((part) => part.trim().toLowerCase());
          if (/^\d{1,2}$/.test(startToken) && /^\d{1,2}$/.test(endToken)) {
            const start = Number(startToken);
            const end = Number(endToken);
            if (start < 1 || start > 31 || end < 1 || end > 31) {
              spec.valid = false;
              spec.unsupportedReason = 'Preview only supports day ranges between 1 and 31.';
              return spec;
            }
            for (let current = start; ; current = current === 31 ? 1 : current + 1) {
              addToSet('monthDays', current);
              if (current === end) {
                break;
              }
            }
            continue;
          }
          const weekdayRange = expandScheduleTokenRange(startToken, endToken, scheduleWeekdayTokens);
          if (weekdayRange) {
            weekdayRange.forEach((weekday) => addToSet('weekdays', weekday));
            continue;
          }
          const monthRange = expandScheduleTokenRange(startToken, endToken, scheduleMonthTokens);
          if (monthRange) {
            monthRange.forEach((month) => addToSet('months', month));
            continue;
          }
          const ordinalRange = expandScheduleTokenRange(startToken, endToken, scheduleOrdinalTokens);
          if (ordinalRange) {
            ordinalRange.forEach((ordinal) => addToSet('weeksOfMonth', ordinal));
            continue;
          }
        }
        spec.valid = false;
        spec.unsupportedReason = `Preview does not yet understand \`${stripScheduleRunQuotes(tokens[index])}\`.`;
        return spec;
      }

      if (!spec.hours) {
        spec.hours = new Set([0]);
      }
      return spec;
    }

    function scheduleDateMatchesSpec(date, spec) {
      if (spec.months && !spec.months.has(date.getMonth())) {
        return false;
      }
      if (spec.monthDays && !spec.monthDays.has(date.getDate())) {
        return false;
      }
      if (spec.weekdays && !spec.weekdays.has(date.getDay())) {
        return false;
      }
      if (spec.weeksOfMonth && spec.weeksOfMonth.size > 0) {
        const weekOfMonth = getScheduleWeekOfMonth(date);
        if (!spec.weeksOfMonth.has(weekOfMonth)
            && !(spec.lastSevenDays && isScheduleLastSevenDaysOfMonth(date))) {
          return false;
        }
      } else if (spec.lastSevenDays && !isScheduleLastSevenDaysOfMonth(date)) {
        return false;
      }
      if (spec.weeksOfYear && !spec.weeksOfYear.has(getScheduleWeekOfYear(date))) {
        return false;
      }
      return true;
    }

    function buildScheduleOccurrence(date, spec, hour) {
      return new Date(
        date.getFullYear(),
        date.getMonth(),
        date.getDate(),
        hour,
        spec.minute,
        0,
        0,
      );
    }

    function getNextScheduleRunOccurrences(row, limit = 5) {
      if (!row || row.mode === 'advanced') {
        return { occurrences: [], unsupportedReason: 'Advanced rows stay in raw mode.' };
      }
      const spec = parseScheduleExpression(`${row.scheduleExpression || ''}`.trim());
      if (!spec.valid) {
        return { occurrences: [], unsupportedReason: spec.unsupportedReason };
      }

      const occurrences = [];
      const now = new Date();
      if (spec.hourly) {
        let candidate = new Date(now);
        candidate.setSeconds(0, 0);
        if (candidate.getMinutes() > spec.minute
            || (candidate.getMinutes() === spec.minute && candidate <= now)) {
          candidate.setHours(candidate.getHours() + 1);
        }
        candidate.setMinutes(spec.minute, 0, 0);
        for (let guard = 0; guard < 24 * 120 && occurrences.length < limit; guard += 1) {
          if (scheduleDateMatchesSpec(candidate, spec)) {
            occurrences.push(new Date(candidate));
          }
          candidate = new Date(candidate.getTime() + 3600000);
        }
        return {
          occurrences,
          unsupportedReason: occurrences.length ? '' : 'No run time found in the preview window.',
        };
      }

      let candidateDay = new Date(now.getFullYear(), now.getMonth(), now.getDate());
      for (let guard = 0; guard < 370 && occurrences.length < limit; guard += 1) {
        if (scheduleDateMatchesSpec(candidateDay, spec)) {
          const dayOccurrences = Array.from(spec.hours)
            .sort((left, right) => left - right)
            .map((hour) => buildScheduleOccurrence(candidateDay, spec, hour))
            .filter((occurrence) => occurrence > now);
          occurrences.push(...dayOccurrences.slice(0, limit - occurrences.length));
        }
        candidateDay = new Date(candidateDay.getTime() + 86400000);
      }
      return {
        occurrences,
        unsupportedReason: occurrences.length ? '' : 'No run time found in the preview window.',
      };
    }

    function formatScheduleOccurrence(occurrence) {
      return occurrence.toLocaleString([], {
        weekday: 'short',
        month: 'short',
        day: 'numeric',
        hour: '2-digit',
        minute: '2-digit',
      });
    }

    function renderScheduleRunOverrideField(row, key, allowedValuesByKey) {
      const currentValue = `${row.overrides?.[key] || ''}`.trim();
      if (scheduleRunBooleanOverrideKeys.has(key)) {
        return `<label>
          <span>${escapeHtml(key)}</span>
          <input
            type="checkbox"
            data-schedule-run-field="${escapeHtml(key)}"
            ${normalizeScheduleBooleanValue(currentValue) === 'yes' ? 'checked' : ''}
          >
        </label>`;
      }

      const allowedValues = Array.isArray(allowedValuesByKey?.[key])
        ? allowedValuesByKey[key]
        : [];
      if (allowedValues.length) {
        const optionValues = Array.from(new Set([
          '',
          ...allowedValues.map((value) => stripSurroundingQuotes(value)),
          currentValue,
        ]));
        return `<label>
          <span>${escapeHtml(key)}</span>
          <select data-schedule-run-field="${escapeHtml(key)}">
            ${optionValues.map((value) => `<option value="${escapeHtml(value)}"${value === currentValue ? ' selected' : ''}>${escapeHtml(value || '—')}</option>`).join('')}
          </select>
        </label>`;
      }

      if (key === 'Priority') {
        return `<label>
          <span>${escapeHtml(key)}</span>
          <input
            type="number"
            min="0"
            step="1"
            data-schedule-run-field="${escapeHtml(key)}"
            value="${escapeHtml(currentValue)}"
          >
        </label>`;
      }

      return `<label>
        <span>${escapeHtml(key)}</span>
        <input data-schedule-run-field="${escapeHtml(key)}" value="${escapeHtml(currentValue)}">
      </label>`;
    }

    function renderScheduleRunLevelField(row) {
      const currentValue = `${row.level || ''}`.trim();
      const optionValues = Array.from(new Set(['', ...scheduleRunLevelOptions, currentValue]));
      return `<label>
        <span>Level</span>
        <select data-schedule-run-field="level">
          ${optionValues.map((value) => `<option value="${escapeHtml(value)}"${value === currentValue ? ' selected' : ''}>${escapeHtml(value || '—')}</option>`).join('')}
        </select>
      </label>`;
    }

    function renderScheduleChipGroup(options, selectedValues, fieldName) {
      return `<div class="schedule-run-chip-grid">${options.map((value) => `<label class="schedule-run-chip-toggle">
        <input
          type="checkbox"
          data-schedule-run-when-field="${escapeHtml(fieldName)}"
          value="${escapeHtml(value)}"
          ${selectedValues.includes(value) ? 'checked' : ''}
        >
        <span>${escapeHtml(scheduleWeekdayLabels[value] || scheduleMonthLabels[value] || value)}</span>
      </label>`).join('')}</div>`;
    }

    function renderScheduleWhenBuilder(row) {
      const whenState = parseScheduleWhenState(row.scheduleExpression || '');
      if (!whenState.supported) {
        return `<div class="schedule-run-when-fallback">
          <span><strong>When</strong> <small class="muted">This expression stays in text mode for now.</small></span>
          <input data-schedule-run-field="scheduleExpression" placeholder="sun at 12:34" value="${escapeHtml(row.scheduleExpression || '')}">
          <div class="muted">${escapeHtml(whenState.unsupportedReason)}</div>
        </div>`;
      }

      return `<div class="schedule-run-when-builder">
        <div class="schedule-run-when-grid">
          <label>
            <span>Frequency</span>
            <select data-schedule-run-when-field="frequency">
              ${['hourly', 'daily', 'weekly', 'monthly', 'yearly', 'custom'].map((value) => `<option value="${value}"${value === whenState.frequency ? ' selected' : ''}>${escapeHtml(value.charAt(0).toUpperCase() + value.slice(1))}</option>`).join('')}
            </select>
          </label>
          ${whenState.frequency === 'hourly'
            ? `<label>
                <span>Minute</span>
                <input
                  type="number"
                  min="0"
                  max="59"
                  step="1"
                  data-schedule-run-when-field="hourlyMinute"
                  value="${escapeHtml(whenState.hourlyMinute || '0')}"
                >
              </label>`
            : `<label>
                <span>Time</span>
                <input
                  type="time"
                  data-schedule-run-when-field="time"
                  value="${escapeHtml(whenState.time || '')}"
                >
              </label>`
          }
          <label>
            <span>Month days</span>
            <input
              data-schedule-run-when-field="monthDays"
              placeholder="1,15,31"
              value="${escapeHtml((whenState.monthDays || []).join(', '))}"
            >
          </label>
          <label>
            <span>Ordinal weekday</span>
            <select data-schedule-run-when-field="ordinal">
              <option value="">None</option>
              ${scheduleOrdinalOptions.map((value) => `<option value="${value}"${value === whenState.ordinal ? ' selected' : ''}>${escapeHtml(value)}</option>`).join('')}
            </select>
          </label>
          <label>
            <span>Ordinal day</span>
            <select data-schedule-run-when-field="ordinalWeekday">
              <option value="">-</option>
              ${scheduleWeekdayOrder.map((value) => `<option value="${value}"${value === whenState.ordinalWeekday ? ' selected' : ''}>${escapeHtml(scheduleWeekdayLabels[value])}</option>`).join('')}
            </select>
          </label>
        </div>
        <div class="schedule-run-when-group">
          <span>Weekdays</span>
          ${renderScheduleChipGroup(scheduleWeekdayOrder, whenState.weekdays || [], 'weekdays')}
        </div>
        <div class="schedule-run-when-group">
          <span>Months</span>
          ${renderScheduleChipGroup(scheduleMonthOrder, whenState.months || [], 'months')}
        </div>
        <div class="schedule-run-summary">Summary: ${escapeHtml(serializeScheduleWhenState(whenState) || 'No schedule yet')}</div>
      </div>`;
    }

    function renderScheduleRunRow(row, index, allowedValuesByKey = {}) {
      const modeLabel = row.mode === 'advanced' ? 'Advanced' : 'Structured';
      const preview = getNextScheduleRunOccurrences(row, 4);
      const previewMarkup = row.mode === 'advanced'
        ? '<div class="muted">This row stays in raw mode. Switch back when it fits the structured editor again.</div>'
        : (preview.occurrences.length
            ? `<ul>${preview.occurrences.map((occurrence) => `<li>${escapeHtml(formatScheduleOccurrence(occurrence))}</li>`).join('')}</ul>`
            : `<div class="muted">${escapeHtml(preview.unsupportedReason || 'No upcoming run found.')}</div>`);
      return `<div class="schedule-run-row" data-row-index="${index}">
        <div class="schedule-run-row-header">
          <div>
            <strong>Run ${index + 1}</strong>
            <span class="schedule-run-chip${row.mode === 'advanced' ? ' advanced' : ''}">${escapeHtml(modeLabel)}</span>
          </div>
          <div class="schedule-run-row-actions">
            <button type="button" class="link-button" data-schedule-run-action="move-up"${index === 0 ? ' disabled' : ''}>Up</button>
            <button type="button" class="link-button" data-schedule-run-action="move-down">Down</button>
            <button type="button" class="link-button" data-schedule-run-action="duplicate">Duplicate</button>
            <button type="button" class="link-button" data-schedule-run-action="toggle-mode">${row.mode === 'advanced' ? 'Use fields' : 'Use raw line'}</button>
            <button type="button" class="link-button" data-schedule-run-action="remove">Remove</button>
          </div>
        </div>
        ${row.mode === 'advanced'
          ? `<label class="schedule-run-advanced">
              <span>Raw Run entry</span>
              <textarea data-schedule-run-field="raw" rows="3" placeholder="Incremental FullPool=Scratch Storage=File Messages=Standard sun at 12:34">${escapeHtml(row.raw || '')}</textarea>
            </label>`
          : `<div class="schedule-run-grid">
              ${renderScheduleRunLevelField(row)}
              ${renderScheduleWhenBuilder(row)}
              ${scheduleRunOverrideKeys.map((key) => renderScheduleRunOverrideField(
                row,
                key,
                allowedValuesByKey,
              )).join('')}
            </div>`
        }
        <div class="schedule-run-preview-card">
          <strong>Next runs</strong>
          ${previewMarkup}
        </div>
      </div>`;
    }

    function renderScheduleRunPreview(rows) {
      const rowSummaries = rows.map((row, index) => {
        const preview = getNextScheduleRunOccurrences(row, 5);
        return { index, preview };
      });
      const mergedTimeline = rowSummaries
        .flatMap(({ index, preview }) => preview.occurrences.map((occurrence) => ({
          index,
          occurrence,
        })))
        .sort((left, right) => left.occurrence - right.occurrence)
        .slice(0, 10);
      const overlapCounts = mergedTimeline.reduce((result, entry) => {
        const key = entry.occurrence.toISOString();
        result.set(key, (result.get(key) || 0) + 1);
        return result;
      }, new Map());
      return `<div class="schedule-run-preview">
        <h5>Combined schedule preview</h5>
        <div class="muted">Each Run line is evaluated independently, so overlapping matches still queue separately.</div>
        <div class="schedule-run-preview-grid">
          ${rowSummaries.map(({ index, preview }) => `<div class="schedule-run-preview-card">
            <strong>Run ${index + 1}</strong>
            ${preview.occurrences.length
              ? `<ul>${preview.occurrences.slice(0, 3).map((occurrence) => `<li>${escapeHtml(formatScheduleOccurrence(occurrence))}</li>`).join('')}</ul>`
              : `<div class="muted">${escapeHtml(preview.unsupportedReason || 'No preview available.')}</div>`}
          </div>`).join('')}
        </div>
        <div class="schedule-run-timeline">
          <strong>Merged timeline</strong>
          ${mergedTimeline.length
            ? `<ul>${mergedTimeline.map((entry) => {
              const count = overlapCounts.get(entry.occurrence.toISOString()) || 0;
              return `<li>${escapeHtml(formatScheduleOccurrence(entry.occurrence))} · Run ${entry.index + 1}${count > 1 ? ` <span class="schedule-run-overlap">(${count} overlapping entries)</span>` : ''}</li>`;
            }).join('')}</ul>`
            : '<div class="muted">Add a schedulable Run entry to preview the upcoming queue.</div>'}
        </div>
      </div>`;
    }

    function renderScheduleRunField(field, options = {}) {
      const rows = normalizeRepeatableFieldValues(field.value || '')
        .map((line) => parseScheduleRunLine(line));
      const effectiveRows = rows.length ? rows : [createDefaultScheduleRunRow()];
      const allowedValuesByKey = options.scheduleRunAllowedValues || {};
      const hiddenValue = effectiveRows
        .map((row) => serializeScheduleRunLine(row))
        .filter((line) => line)
        .join('\n');
      return `<div
        class="schedule-run-editor"
        data-schedule-run-allowed-values="${escapeHtml(JSON.stringify(allowedValuesByKey))}"
      >
        <textarea
          class="repeatable-field schedule-run-storage"
          data-field-key="${escapeHtml(field.key)}"
          data-field-required="${field.required ? 'true' : 'false'}"
          data-field-repeatable="true"
          data-field-quote-char=""
          hidden
        >${escapeHtml(hiddenValue)}</textarea>
        <div class="schedule-run-toolbar">
          <div class="muted">Edit Run entries row by row while keeping embedded pool, storage, and message overrides visible.</div>
          <button type="button" class="link-button" data-schedule-run-action="add">Add Run entry</button>
        </div>
        <div class="schedule-run-rows">${effectiveRows.map((row, index) => renderScheduleRunRow(row, index, allowedValuesByKey)).join('')}</div>
        ${renderScheduleRunPreview(effectiveRows)}
      </div>`;
    }

    function readScheduleRunRowFromElement(rowEl, previousRow) {
      if (!rowEl) {
        return createDefaultScheduleRunRow();
      }
      if (rowEl.querySelector('[data-schedule-run-field="raw"]')) {
        return {
          ...(previousRow || createDefaultScheduleRunRow()),
          mode: 'advanced',
          dirty: true,
          raw: rowEl.querySelector('[data-schedule-run-field="raw"]').value,
        };
      }
      const row = {
        ...(previousRow || createDefaultScheduleRunRow()),
        mode: 'structured',
        dirty: true,
        level: rowEl.querySelector('[data-schedule-run-field="level"]')?.value || '',
        scheduleExpression: '',
        overrides: createEmptyScheduleRunOverrides(),
      };
      if (rowEl.querySelector('[data-schedule-run-field="scheduleExpression"]')) {
        row.scheduleExpression
          = rowEl.querySelector('[data-schedule-run-field="scheduleExpression"]').value || '';
      } else {
        const frequency = rowEl.querySelector('[data-schedule-run-when-field="frequency"]')?.value || 'daily';
        const time = rowEl.querySelector('[data-schedule-run-when-field="time"]')?.value || '';
        const hourlyMinute = rowEl.querySelector('[data-schedule-run-when-field="hourlyMinute"]')?.value || '0';
        const monthDays = parseMonthDaysInput(
          rowEl.querySelector('[data-schedule-run-when-field="monthDays"]')?.value || '',
        );
        const ordinal = rowEl.querySelector('[data-schedule-run-when-field="ordinal"]')?.value || '';
        const ordinalWeekday
          = rowEl.querySelector('[data-schedule-run-when-field="ordinalWeekday"]')?.value || '';
        const weekdays = Array.from(
          rowEl.querySelectorAll('[data-schedule-run-when-field="weekdays"]:checked'),
        ).map((fieldEl) => fieldEl.value);
        const months = Array.from(
          rowEl.querySelectorAll('[data-schedule-run-when-field="months"]:checked'),
        ).map((fieldEl) => fieldEl.value);
        row.scheduleExpression = serializeScheduleWhenState({
          frequency,
          time,
          hourlyMinute,
          monthDays,
          ordinal,
          ordinalWeekday,
          weekdays,
          months,
          hasExplicitTime: frequency === 'hourly' ? true : !!time,
        });
      }
      scheduleRunOverrideKeys.forEach((key) => {
        const fieldEl = rowEl.querySelector(`[data-schedule-run-field="${key}"]`);
        if (!fieldEl) {
          row.overrides[key] = '';
          return;
        }
        if (scheduleRunBooleanOverrideKeys.has(key) && fieldEl.type === 'checkbox') {
          row.overrides[key] = fieldEl.checked ? 'yes' : '';
          return;
        }
        row.overrides[key] = fieldEl.value || '';
      });
      return row;
    }

    function initializeScheduleRunEditors(containerEl) {
      containerEl.querySelectorAll('.schedule-run-editor').forEach((editorEl) => {
        if (editorEl.dataset.initialized === 'true') {
          return;
        }
        editorEl.dataset.initialized = 'true';
        const storageEl = editorEl.querySelector('.schedule-run-storage');
        const rowsEl = editorEl.querySelector('.schedule-run-rows');
        const allowedValuesByKey = editorEl.dataset.scheduleRunAllowedValues
          ? JSON.parse(editorEl.dataset.scheduleRunAllowedValues)
          : {};

        const refreshEditorPreview = () => {
          const rows = editorEl._scheduleRunRows && editorEl._scheduleRunRows.length
            ? editorEl._scheduleRunRows
            : [createDefaultScheduleRunRow()];
          storageEl.value = rows
            .map((row) => serializeScheduleRunLine(row))
            .filter((line) => line)
            .join('\n');
          rowsEl.querySelectorAll('.schedule-run-row').forEach((rowEl, index) => {
            const row = rows[index] || createDefaultScheduleRunRow();
            const preview = getNextScheduleRunOccurrences(row, 4);
            const summaryEl = rowEl.querySelector('.schedule-run-summary');
            if (summaryEl) {
              summaryEl.textContent = `Summary: ${serializeScheduleWhenState(
                parseScheduleWhenState(row.scheduleExpression || ''),
              ) || 'No schedule yet'}`;
            }
            const previewCard = rowEl.querySelector('.schedule-run-preview-card');
            if (!previewCard) {
              return;
            }
            previewCard.innerHTML = `<strong>Next runs</strong>${row.mode === 'advanced'
              ? '<div class="muted">This row stays in raw mode. Switch back when it fits the structured editor again.</div>'
              : (preview.occurrences.length
                  ? `<ul>${preview.occurrences.map((occurrence) => `<li>${escapeHtml(formatScheduleOccurrence(occurrence))}</li>`).join('')}</ul>`
                  : `<div class="muted">${escapeHtml(preview.unsupportedReason || 'No upcoming run found.')}</div>`)}`;
          });
          const oldPreview = editorEl.querySelector('.schedule-run-preview');
          if (oldPreview) {
            oldPreview.remove();
          }
          editorEl.insertAdjacentHTML('beforeend', renderScheduleRunPreview(rows));
        };

        const renderEditor = () => {
          const rows = editorEl._scheduleRunRows && editorEl._scheduleRunRows.length
            ? editorEl._scheduleRunRows
            : [createDefaultScheduleRunRow()];
          rowsEl.innerHTML = rows.map((row, index) => renderScheduleRunRow(
            row,
            index,
            allowedValuesByKey,
          )).join('');
          refreshEditorPreview();
        };

        editorEl._scheduleRunRows = normalizeRepeatableFieldValues(storageEl.value || '')
          .map((line) => parseScheduleRunLine(line));
        if (!editorEl._scheduleRunRows.length) {
          editorEl._scheduleRunRows = [createDefaultScheduleRunRow()];
        }
        renderEditor();

        const handleScheduleRunRowUpdate = (event) => {
          const rowEl = event.target.closest('.schedule-run-row');
          if (!rowEl) {
            return;
          }
          const rowIndex = Number(rowEl.dataset.rowIndex);
          editorEl._scheduleRunRows[rowIndex] = readScheduleRunRowFromElement(
            rowEl,
            editorEl._scheduleRunRows[rowIndex],
          );
          if (event.type === 'change'
              && event.target.matches('[data-schedule-run-when-field="frequency"], [data-schedule-run-when-field="ordinal"]')) {
            renderEditor();
            return;
          }
          refreshEditorPreview();
        };
        editorEl.addEventListener('input', handleScheduleRunRowUpdate);
        editorEl.addEventListener('change', handleScheduleRunRowUpdate);

        editorEl.addEventListener('click', (event) => {
          const actionEl = event.target.closest('[data-schedule-run-action]');
          if (!actionEl) {
            return;
          }
          event.preventDefault();
          const action = actionEl.dataset.scheduleRunAction;
          const rowEl = actionEl.closest('.schedule-run-row');
          const rowIndex = rowEl ? Number(rowEl.dataset.rowIndex) : -1;

          if (rowIndex >= 0) {
            editorEl._scheduleRunRows[rowIndex] = readScheduleRunRowFromElement(
              rowEl,
              editorEl._scheduleRunRows[rowIndex],
            );
          }

          switch (action) {
            case 'add':
              editorEl._scheduleRunRows.push(createDefaultScheduleRunRow());
              break;
            case 'remove':
              editorEl._scheduleRunRows.splice(rowIndex, 1);
              if (!editorEl._scheduleRunRows.length) {
                editorEl._scheduleRunRows.push(createDefaultScheduleRunRow());
              }
              break;
            case 'duplicate': {
              const sourceRow = editorEl._scheduleRunRows[rowIndex] || createDefaultScheduleRunRow();
              const serialized = serializeScheduleRunLine({
                ...sourceRow,
                dirty: true,
              });
              const parsedDuplicate = parseScheduleRunLine(serialized);
              editorEl._scheduleRunRows.splice(rowIndex + 1, 0, {
                ...parsedDuplicate,
                raw: serialized,
                dirty: true,
                overrides: { ...(parsedDuplicate.overrides || createEmptyScheduleRunOverrides()) },
              });
              break;
            }
            case 'move-up':
              if (rowIndex > 0) {
                [editorEl._scheduleRunRows[rowIndex - 1], editorEl._scheduleRunRows[rowIndex]]
                  = [editorEl._scheduleRunRows[rowIndex], editorEl._scheduleRunRows[rowIndex - 1]];
              }
              break;
            case 'move-down':
              if (rowIndex >= 0 && rowIndex < editorEl._scheduleRunRows.length - 1) {
                [editorEl._scheduleRunRows[rowIndex], editorEl._scheduleRunRows[rowIndex + 1]]
                  = [editorEl._scheduleRunRows[rowIndex + 1], editorEl._scheduleRunRows[rowIndex]];
              }
              break;
            case 'toggle-mode':
              if (rowIndex >= 0) {
                const currentRow = editorEl._scheduleRunRows[rowIndex];
                if (currentRow.mode === 'advanced') {
                  const parsedRow = parseScheduleRunLine(currentRow.raw);
                  editorEl._scheduleRunRows[rowIndex] = parsedRow.mode === 'advanced'
                    ? currentRow
                    : {
                      ...parsedRow,
                      dirty: true,
                    };
                } else {
                  editorEl._scheduleRunRows[rowIndex] = {
                    ...currentRow,
                    mode: 'advanced',
                    raw: serializeScheduleRunLine({
                      ...currentRow,
                      dirty: true,
                    }),
                    dirty: true,
                  };
                }
              }
              break;
            default:
              break;
          }
          renderEditor();
        });
      });
    }

    function getRepeatableFieldValues(fieldEl) {
      if (fieldEl.tagName === 'SELECT' && fieldEl.multiple) {
        return Array.from(fieldEl.selectedOptions).map((option) => option.value);
      }
      return splitRepeatableFieldValues(fieldEl.value);
    }

    function sortResourcesForNode(node, resources) {
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
      return sortedResources;
    }

    function getNodeResourcesForTree(node) {
      if (node.kind === 'datacenter') {
        return [];
      }
      const resources = loadedNodeResources.get(node.id) || node.resources || [];
      return sortResourcesForNode(node, resources);
    }

    function treeGroupForNodeKind(kind) {
      if (kind === 'director') {
        return { id: 'tree-group-directors', label: 'Directors' };
      }
      if (kind === 'storage-daemon') {
        return { id: 'tree-group-storages', label: 'Storages' };
      }
      if (kind === 'file-daemon' || kind === 'client') {
        return { id: 'tree-group-clients', label: 'Clients' };
      }
      if (kind === 'console') {
        return { id: 'tree-group-consoles', label: 'Consoles' };
      }
      return null;
    }

    function sortTreeNodes(nodes) {
      return [...nodes].sort((left, right) =>
        stripSurroundingQuotes(left.label || '').localeCompare(
          stripSurroundingQuotes(right.label || ''),
        ));
    }

    function normalizeNodeLabelForComparison(label) {
      return `${stripSurroundingQuotes(label || '')}`.trim().toLowerCase();
    }

    function deduplicateClientGroupNodes(nodes) {
      const fileDaemonLabels = new Set(
        nodes
          .filter((node) => node.kind === 'file-daemon')
          .map((node) => normalizeNodeLabelForComparison(node.label)),
      );
      return nodes.filter((node) => {
        if (node.kind !== 'client') {
          return true;
        }
        const label = normalizeNodeLabelForComparison(node.label);
        return !label || !fileDaemonLabels.has(label);
      });
    }

    function groupedTreeEntries(root) {
      const treeGroupDefinitions = [
        { id: 'tree-group-directors', label: 'Directors', nodes: [] },
        { id: 'tree-group-storages', label: 'Storages', nodes: [] },
        { id: 'tree-group-clients', label: 'Clients', nodes: [] },
        { id: 'tree-group-consoles', label: 'Consoles', nodes: [] },
      ];
      const groupsById = new Map(
        treeGroupDefinitions.map((group) => [group.id, group]),
      );
      (root.children || []).forEach((child) => {
        const group = treeGroupForNodeKind(child.kind);
        if (!group || !groupsById.has(group.id)) {
          return;
        }
        groupsById.get(group.id).nodes.push(child);
      });
      return treeGroupDefinitions
        .map((group) => ({
          ...group,
          nodes: sortTreeNodes(
            group.id === 'tree-group-clients'
              ? deduplicateClientGroupNodes(group.nodes)
              : group.nodes,
          ),
        }))
        .filter((group) => group.nodes.length);
    }

    function renderTreeToggle(isExpanded, onToggle, isVisible = true) {
      const toggleEl = document.createElement('button');
      toggleEl.type = 'button';
      toggleEl.className = `tree-toggle${isVisible ? '' : ' placeholder'}`;
      toggleEl.textContent = isExpanded ? '▾' : '▸';
      if (isVisible) {
        toggleEl.addEventListener('click', (event) => {
          event.stopPropagation();
          onToggle();
        });
      } else {
        toggleEl.disabled = true;
      }
      return toggleEl;
    }

    function toggleTreeExpansion(entryId) {
      if (expandedTreeIds.has(entryId)) {
        expandedTreeIds.delete(entryId);
      } else {
        expandedTreeIds.add(entryId);
      }
      renderTree(currentTreeData);
    }

    function isTreeExpanded(entryId) {
      return expandedTreeIds.has(entryId);
    }

    function findNodeContainingResource(node, resourceId) {
      const resources = loadedNodeResources.get(node.id) || node.resources || [];
      if (resources.some((resource) => resource.id === resourceId)) {
        return node.id;
      }
      for (const child of (node.children || [])) {
        const match = findNodeContainingResource(child, resourceId);
        if (match) {
          return match;
        }
      }
      return '';
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
            if (!payload.saved) {
              return;
            }
            return fetch('/api/v1/bootstrap')
              .then((response) => response.json())
              .then((data) => {
                loadedNodeResources.clear();
                renderTree(data);
                loadNodeById('datacenter');
              });
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

    function relationshipColor(relation) {
      switch ((relation || '').toLowerCase()) {
        case 'daemon-name':
          return '#0075be';
        case 'resource-name':
          return '#3f8f5c';
        case 'shared-password':
          return '#f5a623';
        case 'client':
          return '#0075be';
        case 'storage':
          return '#8a55d7';
        default:
          return '#5a6773';
      }
    }

    function truncateGraphLabel(value, limit = 26) {
      if (!value) {
        return '-';
      }
      if (value.length <= limit) {
        return value;
      }
      return `${value.slice(0, Math.max(limit - 1, 1))}\u2026`;
    }

    function resourceDisplayLabel(type, name, fallback) {
      const normalizedType = `${type || ''}`.trim();
      const normalizedName = `${name || ''}`.trim();
      if (normalizedType && normalizedName) {
        return `${normalizedType} ${normalizedName}`;
      }
      if (normalizedType) {
        return normalizedType;
      }
      if (normalizedName) {
        return normalizedName;
      }
      return fallback || 'unresolved';
    }

    function graphResourceTypeLabel(type) {
      const normalizedType = `${type || ''}`.trim().toLowerCase();
      const labels = {
        'autochanger': 'Autochanger',
        'client': 'Client',
        'configuration': 'Configuration',
        'console': 'Console',
        'device': 'Device',
        'director': 'Director',
        'file-daemon': 'FileDaemon',
        'fileset': 'FileSet',
        'job': 'Job',
        'jobdefs': 'JobDefs',
        'messages': 'Messages',
        'pool': 'Pool',
        'profile': 'Profile',
        'schedule': 'Schedule',
        'storage': 'Storage',
        'user': 'User',
      };
      if (labels[normalizedType]) {
        return labels[normalizedType];
      }
      return normalizedType
        ? normalizedType.charAt(0).toUpperCase() + normalizedType.slice(1)
        : '';
    }

    function graphResourceLabelParts(type, name, fallback) {
      const normalizedName = `${name || ''}`.trim();
      const typeLabel = graphResourceTypeLabel(type);
      if (typeLabel && normalizedName) {
        return {
          typeLabel,
          nameLabel: normalizedName,
          fullLabel: `${typeLabel} ${normalizedName}`,
        };
      }
      if (typeLabel) {
        return {
          typeLabel,
          nameLabel: '',
          fullLabel: typeLabel,
        };
      }
      return {
        typeLabel: '',
        nameLabel: '',
        fullLabel: fallback || 'unresolved',
      };
    }

    function relationshipDirectiveLabel(relationship) {
      const referencedType = `${relationship?.target_resource_type || ''}`.trim().toLowerCase();
      const resolution = `${relationship?.resolution || ''}`.trim();
      const directiveMatch = resolution.match(/^Directive (.+?) in /);
      let label = '';
      if (directiveMatch) {
        label = directiveMatch[1];
      } else {
        switch (`${relationship?.relation || ''}`.trim().toLowerCase()) {
          case 'shared-password':
            label = 'Password';
            break;
          case 'device':
            label = 'Device';
            break;
          case 'storage':
            label = 'Storage';
            break;
          default:
            label = '';
            break;
        }
      }
      if (!label) return '';
      const normalizedLabel = label.trim();
      if (!normalizedLabel || normalizedLabel.includes('-')) return '';
      return normalizedLabel.toLowerCase() === referencedType ? '' : normalizedLabel;
    }

    function isInternalRelationship(relationship) {
      const fromNodeId = `${relationship.from_node_id || ''}`.trim();
      const toNodeId = `${relationship.to_node_id || ''}`.trim();
      const sourcePath = `${relationship.source_resource_path || ''}`.trim();
      const targetPath = `${relationship.target_resource_path || ''}`.trim();
      const relationshipPathDomain = (path) => {
        if (!path) {
          return '';
        }
        if (path.includes('/bareos-dir.d/') || path.endsWith('/bareos-dir.conf')) {
          return 'director';
        }
        if (path.includes('/bareos-sd.d/') || path.endsWith('/bareos-sd.conf')) {
          return 'storage-daemon';
        }
        if (path.includes('/bareos-fd.d/') || path.endsWith('/bareos-fd.conf')) {
          return 'file-daemon';
        }
        if (path.includes('/bconsole.d/') || path.endsWith('/bconsole.conf')) {
          return 'console';
        }
        return '';
      };
      const sourceDomain = relationshipPathDomain(sourcePath);
      const targetDomain = relationshipPathDomain(targetPath);
      if (sourceDomain && targetDomain && sourceDomain !== targetDomain) {
        return false;
      }
      if (fromNodeId && toNodeId) {
        return fromNodeId === toNodeId;
      }
      if (sourceDomain && targetDomain) {
        return sourceDomain === targetDomain;
      }

      return Boolean(sourcePath && targetPath && sourcePath === targetPath);
    }

    function renderRelationshipGraph(relationships, options = {}) {
      const title = options.title || 'Graph view';
      const description = options.description
        || 'Each daemon or director definition appears once as an object. The nested rows are the concrete config resources participating in relationships, with the resource type and name styled differently, and the arrows show which resource links to which other resource.';
      const emptyMessage = options.emptyMessage || 'No relationships discovered for this graph.';
      const graphId = `${options.graphId || 'relationships'}`
        .replace(/[^a-z0-9_-]+/gi, '-')
        .toLowerCase();
      const markerId = `relationship-arrow-${graphId}`;
      if (!relationships || !relationships.length) {
        return `<div class="relationship-graph-card">
          <h5>${escapeHtml(title)}</h5>
          <div class="muted">${escapeHtml(description)}</div>
          <div class="muted">${escapeHtml(emptyMessage)}</div>
        </div>`;
      }

      const legendCounts = [];
      const objectIndex = new Map();
      const objects = [];
      const ensureObject = (key, label, options = {}) => {
        if (!objectIndex.has(key)) {
          const object = {
            key,
            label: label || '-',
            nodeId: options.nodeId || '',
            unresolved: Boolean(options.unresolved),
            resourceIndex: new Map(),
            resources: [],
          };
          objectIndex.set(key, object);
          objects.push(object);
        }
        return objectIndex.get(key);
      };
      const ensureResource = (object, key, label, path, options = {}) => {
        if (!object.resourceIndex.has(key)) {
          const resource = {
            key,
            label,
            path,
            resourceId: options.resourceId || '',
            typeLabel: options.typeLabel || '',
            nameLabel: options.nameLabel || '',
            incoming: 0,
            outgoing: 0,
          };
          object.resourceIndex.set(key, resource);
          object.resources.push(resource);
        }
        return object.resourceIndex.get(key);
      };
      const objectKeyForRelationshipEnd = (nodeId, label, unresolvedPrefix) =>
        nodeId ? `node:${nodeId}` : `${unresolvedPrefix}:${label || '-'}`;

      relationships.forEach((relationship) => {
        const relation = relationship.relation || 'other';
        const existing = legendCounts.find((entry) => entry.relation === relation);
        if (existing) {
          existing.count += 1;
        } else {
          legendCounts.push({ relation, count: 1 });
        }

        const sourceLabel = relationship.from_label || '-';
        const targetLabel = relationship.to_label || relationship.endpoint_name || '-';
        const sourceObject = ensureObject(
          objectKeyForRelationshipEnd(relationship.from_node_id, sourceLabel, 'source'),
          sourceLabel,
          { nodeId: relationship.from_node_id },
        );
        const targetObject = ensureObject(
          objectKeyForRelationshipEnd(relationship.to_node_id, targetLabel, 'unresolved'),
          targetLabel,
          {
            nodeId: relationship.to_node_id,
            unresolved: !relationship.to_node_id,
          },
        );

        const sourceResourceLabel = graphResourceLabelParts(
          relationship.source_resource_type,
          relationship.source_resource_name,
          relationship.endpoint_name || relationship.relation || 'source',
        );
        const sourceResource = ensureResource(
          sourceObject,
          relationship.source_resource_id || `source:${relationship.source_resource_path || ''}:${relationship.relation}:${relationship.endpoint_name || ''}`,
          sourceResourceLabel.fullLabel,
          relationship.source_resource_path || '',
          {
            resourceId: relationship.source_resource_id,
            typeLabel: sourceResourceLabel.typeLabel,
            nameLabel: sourceResourceLabel.nameLabel,
          },
        );
        const targetResourceLabel = graphResourceLabelParts(
          relationship.target_resource_type,
          relationship.target_resource_name,
          targetLabel || relationship.endpoint_name || relationship.relation || 'target',
        );
        const targetResource = ensureResource(
          targetObject,
          relationship.target_resource_id || `target:${relationship.target_resource_path || ''}:${relationship.relation}:${targetLabel}`,
          targetResourceLabel.fullLabel,
          relationship.target_resource_path || '',
          {
            resourceId: relationship.target_resource_id,
            typeLabel: targetResourceLabel.typeLabel,
            nameLabel: targetResourceLabel.nameLabel,
          },
        );
        sourceResource.outgoing += 1;
        targetResource.incoming += 1;
        relationship._graphSourceObjectKey = sourceObject.key;
        relationship._graphTargetObjectKey = targetObject.key;
        relationship._graphSourceResourceKey = sourceResource.key;
        relationship._graphTargetResourceKey = targetResource.key;
      });

      const splitObjectResources = Boolean(options.splitObjectResources);
      const splitLaneCount = splitObjectResources ? 2 : 1;
      const preferredLaneForResource = (resource) => {
        if (resource.outgoing > 0) return 0;
        return 1;
      };
      const laneOrderForResource = (resource) => {
        if (resource.outgoing > 0 && resource.incoming === 0) return 0;
        if (resource.incoming > 0 && resource.outgoing > 0) return 1;
        return 2;
      };
      const laneResourcesForObject = (object, lane) =>
        splitObjectResources ? object.resourceLanes[lane] : object.resources;
      const lanePortX = (resource, direction) =>
        direction >= 0 ? resource.x + resource.width : resource.x;
      const sameLaneLoopDirection = (lane) => (lane === 0 ? -1 : 1);
      const laneBoundaryX = (object, leftLane) =>
        object.x + objectPaddingSide
          + ((leftLane + 1) * resourceBoxWidth)
          + (leftLane * resourceLaneGap)
          + (resourceLaneGap / 2);
      objects.forEach((object) => {
        const normalizedTypeLabel = (resource) =>
          `${resource.typeLabel || ''}`.trim().toLowerCase();
        object.resources.sort((left, right) => {
          const typeOrder = normalizedTypeLabel(left).localeCompare(normalizedTypeLabel(right));
          if (typeOrder !== 0) {
            return typeOrder;
          }
          return left.label.localeCompare(right.label);
        });
        object.column = object.nodeId && object.nodeId.startsWith('director-') ? 0 : 1;
        object.resourceLanes = Array.from(
          { length: splitLaneCount },
          () => [],
        );
        object.resources.forEach((resource, resourceIndex) => {
          let lane = 0;
          if (splitObjectResources) {
            lane = preferredLaneForResource(resource);
          }
          resource.lane = lane;
          if (splitObjectResources) {
            const laneResources = laneResourcesForObject(object, lane);
            resource.laneIndex = laneResources.length;
            laneResources.push(resource);
          } else {
            resource.laneIndex = resourceIndex;
          }
        });
        if (splitObjectResources) {
          object.resourceLanes.forEach((laneResources) => {
            laneResources.sort((left, right) => {
              const order = laneOrderForResource(left) - laneOrderForResource(right);
              if (order !== 0) return order;
              const typeOrder = normalizedTypeLabel(left).localeCompare(
                normalizedTypeLabel(right),
              );
              if (typeOrder !== 0) return typeOrder;
              return left.label.localeCompare(right.label);
            });
            laneResources.forEach((resource, index) => {
              resource.laneIndex = index;
            });
          });
        }
      });

      const compactGraph = !splitObjectResources;
      const baseObjectWidth = compactGraph ? 182 : 208;
      const columnGap = compactGraph ? 68 : 92;
      const headerHeight = compactGraph ? 12 : 40;
      const resourceBoxHeight = 23;
      const resourceRowHeight = compactGraph
        ? resourceBoxHeight + 4
        : resourceBoxHeight * 2;
      const objectPaddingTop = compactGraph ? 2 : 8;
      const objectPaddingBottom = compactGraph ? 2 : 8;
      const objectPaddingSide = compactGraph ? 6 : 8;
      const resourceLaneGap = splitObjectResources
        ? (resourceBoxHeight + objectPaddingSide) * 3.75
        : 0;
      const objectWidth = splitObjectResources
        ? (objectPaddingSide * 2)
          + (baseObjectWidth * splitLaneCount)
          + (resourceLaneGap * (splitLaneCount - 1))
        : baseObjectWidth;
      const resourceBoxWidth = splitObjectResources
        ? baseObjectWidth
        : objectWidth - (objectPaddingSide * 2);
      const topPadding = compactGraph ? 14 : 30;
      const portInset = 10;
      objects.forEach((object) => {
        const visibleRowCount = splitObjectResources
          ? Math.max(...object.resourceLanes.map((lane) => lane.length), 0)
          : object.resources.length;
        object.height = objectPaddingTop + headerHeight
          + (visibleRowCount * resourceRowHeight) + objectPaddingBottom;
      });

      const verticalGap = compactGraph ? 6 : 14;
      const graphPaddingX = 18;
      const outerColumnLinkMargin = 96;
      const outerColumnLinkPadding = outerColumnLinkMargin + 64;
      const graphPaddingLeft = graphPaddingX + outerColumnLinkPadding;
      const graphPaddingRight = graphPaddingX + outerColumnLinkPadding;
      const columns = [0, 1].map((column) =>
        objects
          .filter((object) => object.column === column)
          .sort((left, right) => left.label.localeCompare(right.label)),
      );
      const visibleColumns = [0, 1].filter((column) => columns[column].length > 0);
      const graphWidth = visibleColumns.length === 1
        ? graphPaddingLeft + graphPaddingRight + objectWidth
        : graphPaddingLeft + graphPaddingRight + objectWidth * 2 + columnGap;
      const columnX = {
        0: visibleColumns.length === 1
          ? Math.max((graphWidth - objectWidth) / 2, graphPaddingLeft)
          : graphPaddingLeft,
        1: visibleColumns.length === 1
          ? Math.max((graphWidth - objectWidth) / 2, graphPaddingLeft)
          : graphWidth - graphPaddingRight - objectWidth,
      };
      const columnHeight = Math.max(
        ...columns.map((entries) =>
          entries.reduce((sum, object) => sum + object.height, 0)
          + Math.max(entries.length - 1, 0) * verticalGap,
        ),
        1,
      );
      const centerOffset = (entries) => {
        const entriesHeight = entries.reduce((sum, object) => sum + object.height, 0)
          + Math.max(entries.length - 1, 0) * verticalGap;
        return topPadding + Math.max((columnHeight - entriesHeight) / 2, 0);
      };
      let maxObjectHeight = 0;
      visibleColumns.forEach((column) => {
        let currentY = centerOffset(columns[column]);
        columns[column].forEach((object) => {
          const maxLaneLength = splitObjectResources
            ? Math.max(...object.resourceLanes.map((lane) => lane.length), 0)
            : object.resources.length;
          object.x = columnX[column];
          object.y = currentY;
          object.resources.forEach((resource) => {
            const laneX = splitObjectResources
              ? object.x + objectPaddingSide
                + (resource.lane * (resourceBoxWidth + resourceLaneGap))
              : object.x + objectPaddingSide;
            const laneLength = splitObjectResources
              ? laneResourcesForObject(object, resource.lane).length
              : object.resources.length;
            const laneOffset = splitObjectResources
              ? ((maxLaneLength - laneLength) * resourceRowHeight) / 2
              : 0;
            resource.x = laneX;
            resource.width = resourceBoxWidth;
            resource.y = object.y + objectPaddingTop + headerHeight
              + laneOffset
              + (resource.laneIndex * resourceRowHeight) + (resourceRowHeight / 2);
          });
          currentY += object.height + verticalGap;
          maxObjectHeight = Math.max(maxObjectHeight, object.height);
        });
      });

      const directedCounts = new Map();
      relationships.forEach((relationship) => {
        const key = `${relationship._graphSourceObjectKey}:${relationship._graphSourceResourceKey}->${relationship._graphTargetObjectKey}:${relationship._graphTargetResourceKey}`;
        directedCounts.set(key, (directedCounts.get(key) || 0) + 1);
      });
      const directedSeen = new Map();
      const graphResourceRef = (object, resource) => `${object.key}|${resource.key}`;
      const graphEdgeDataAttributes = (
        sourceObject,
        targetObject,
        sourceResourceRef,
        targetResourceRef,
      ) => `data-source-object-key="${escapeHtml(sourceObject.key)}"
                data-target-object-key="${escapeHtml(targetObject.key)}"
                data-source-resource-ref="${escapeHtml(sourceResourceRef)}"
                data-target-resource-ref="${escapeHtml(targetResourceRef)}"`;
      const graphEdgeTitle = (relationship) =>
        `${relationship.from_label || '-'} → ${relationship.to_label || relationship.endpoint_name || '-'} (${relationship.relation || 'related'})${relationship.resolution ? ` — ${relationship.resolution}` : ''}`;
      const graphEndApproachY = (startY, endY) => {
        const endApproachOffset = Math.min(
          Math.max(Math.abs(endY - startY) / 3, 10),
          18,
        );
        return endY - (Math.sign(endY - startY || 1) * endApproachOffset);
      };
      const renderEdgeMarkup = (relationship, edgeDataAttributes, pathD, title, stroke, startX, startY, labelX, labelY) => {
        const rawDirectiveLabel = relationshipDirectiveLabel(relationship);
        const directiveLabel = rawDirectiveLabel
          ? truncateGraphLabel(rawDirectiveLabel, 18)
          : '';
        const labelWidth = Math.max((directiveLabel.length * 5.8) + 10, 24);
        return `<g class="relationship-graph-edge" ${edgeDataAttributes}>
            <path d="${pathD}"
              fill="none"
              stroke="${stroke}"
              stroke-width="2.1"
              stroke-linecap="round"
              ${relationship.resolved ? '' : 'stroke-dasharray="7 6"'}
              opacity="${relationship.resolved ? '0.92' : '0.72'}"
              marker-end="url(#${markerId})">
              <title>${escapeHtml(title)}</title>
            </path>
            <circle class="relationship-graph-edge-source" cx="${startX}" cy="${startY}" r="2.4" fill="${stroke}" opacity="${relationship.resolved ? '0.96' : '0.8'}"></circle>${directiveLabel ? `
            <g class="relationship-graph-edge-label" transform="translate(${labelX} ${labelY})">
              <rect x="${-(labelWidth / 2)}" y="-6.5" width="${labelWidth}" height="13" rx="6"></rect>
              <text x="0" y="0">${escapeHtml(directiveLabel)}</text>
            </g>` : ''}
          </g>`;
      };
      const renderRelationshipLegend = (entries) => `
        <div class="relationship-legend">${entries.map((entry) => `
          <span class="relationship-chip">
            <span class="relationship-chip-swatch" style="background:${entry.color}"></span>
            ${escapeHtml(entry.label)}
          </span>`).join('')}</div>`;

      const edgeMarkup = relationships.map((relationship) => {
        const sourceObject = objectIndex.get(relationship._graphSourceObjectKey);
        const targetObject = objectIndex.get(relationship._graphTargetObjectKey);
        const sourceResource = sourceObject?.resourceIndex.get(relationship._graphSourceResourceKey);
        const targetResource = targetObject?.resourceIndex.get(relationship._graphTargetResourceKey);
        if (!sourceObject || !targetObject || !sourceResource || !targetResource) {
          return '';
        }

        const sameObject = sourceObject.key === targetObject.key;
        const sameColumn = !sameObject && sourceObject.column === targetObject.column;
        const forward = sourceObject.x <= targetObject.x;
        const startX = sameObject
          ? (sourceResource.lane === 0
              ? sourceResource.x + sourceResource.width
              : sourceResource.x)
          : sourceObject.x + (forward ? objectWidth - portInset : portInset);
        const endX = sameObject
          ? (targetResource.lane === 0
              ? targetResource.x + targetResource.width
              : targetResource.x)
          : targetObject.x + (forward ? portInset : objectWidth - portInset);
        const startY = sourceResource.y;
        const endY = targetResource.y;
        const pairKey = `${relationship._graphSourceObjectKey}:${relationship._graphSourceResourceKey}->${relationship._graphTargetObjectKey}:${relationship._graphTargetResourceKey}`;
        const reverseKey = `${relationship._graphTargetObjectKey}:${relationship._graphTargetResourceKey}->${relationship._graphSourceObjectKey}:${relationship._graphSourceResourceKey}`;
        const pairCount = directedCounts.get(pairKey) || 1;
        const pairIndex = directedSeen.get(pairKey) || 0;
        directedSeen.set(pairKey, pairIndex + 1);
        const reverseCount = directedCounts.get(reverseKey) || 0;
        const sourceResourceRef = graphResourceRef(sourceObject, sourceResource);
        const targetResourceRef = graphResourceRef(targetObject, targetResource);
        const edgeDataAttributes = graphEdgeDataAttributes(
          sourceObject,
          targetObject,
          sourceResourceRef,
          targetResourceRef,
        );
        const laneOffset = (pairIndex - ((pairCount - 1) / 2)) * 8;
        const stroke = relationshipColor(relationship.relation);
        const title = graphEdgeTitle(relationship);
        if (sameObject) {
          if (splitObjectResources) {
            const sameLane = sourceResource.lane === targetResource.lane;
            if (!sameLane) {
              const laneDirection = targetResource.lane > sourceResource.lane ? 1 : -1;
              const crossLaneStartX = lanePortX(sourceResource, laneDirection);
              const crossLaneEndX = lanePortX(targetResource, -laneDirection);
              const boundaryX = laneBoundaryX(
                sourceObject,
                Math.min(sourceResource.lane, targetResource.lane),
              );
              const branchStartX = crossLaneStartX + (laneDirection * 14);
              const branchEndX = crossLaneEndX - (laneDirection * 14);
              const midY = ((startY + endY) / 2) + laneOffset;
              const endApproachY = graphEndApproachY(startY, endY);
              return renderEdgeMarkup(
                relationship,
                edgeDataAttributes,
                `M ${crossLaneStartX} ${startY} C ${branchStartX} ${startY}, ${boundaryX} ${startY}, ${boundaryX} ${midY}
                C ${boundaryX} ${endY}, ${branchEndX} ${endApproachY}, ${crossLaneEndX} ${endY}`,
                title,
                stroke,
                crossLaneStartX,
                startY,
                boundaryX,
                midY - 10,
              );
            }
            const sameResource = sourceResource.key === targetResource.key;
            const loopDirection = sameLaneLoopDirection(sourceResource.lane);
            const sameLaneStartX = lanePortX(sourceResource, loopDirection);
            const sameLaneEndX = lanePortX(targetResource, loopDirection);
            const loopX = loopDirection < 0
              ? sourceObject.x - (outerColumnLinkMargin / 2) - Math.abs(laneOffset)
              : sourceObject.x + objectWidth + (outerColumnLinkMargin / 2) + Math.abs(laneOffset);
            if (sameResource) {
              const branchX = sameLaneStartX + (loopDirection * 22);
              const selfLoopHeight = resourceRowHeight + Math.abs(laneOffset);
              const selfLoopEndY = startY + 14;
              const selfLoopApproachY = selfLoopEndY - 10;
              const selfLoopLabelY = startY + ((selfLoopEndY - startY) / 2) - 2;
              return renderEdgeMarkup(
                relationship,
                edgeDataAttributes,
                `M ${sameLaneStartX} ${startY} C ${branchX} ${startY}, ${loopX} ${startY - selfLoopHeight}, ${loopX} ${startY}
                C ${loopX} ${startY + selfLoopHeight}, ${branchX} ${selfLoopApproachY}, ${sameLaneStartX} ${selfLoopEndY}`,
                title,
                stroke,
                sameLaneStartX,
                startY,
                loopX,
                selfLoopLabelY,
              );
            }
            const branchX = sameLaneStartX + (loopDirection * 22);
            const branchEndX = sameLaneEndX + (loopDirection * 22);
            const midY = ((startY + endY) / 2) + laneOffset;
            const endApproachY = graphEndApproachY(startY, endY);
            return renderEdgeMarkup(
              relationship,
              edgeDataAttributes,
              `M ${sameLaneStartX} ${startY} C ${branchX} ${startY}, ${loopX} ${startY}, ${loopX} ${midY}
              C ${loopX} ${endY}, ${branchEndX} ${endApproachY}, ${sameLaneEndX} ${endY}`,
              title,
              stroke,
              sameLaneStartX,
              startY,
              loopX,
              midY - 10,
            );
          }
          const loopX = sourceObject.x + objectWidth + 30 + Math.abs(laneOffset);
          const controlY1 = startY + laneOffset - 10;
          const controlY2 = endY + laneOffset + 10;
          return renderEdgeMarkup(
            relationship,
            edgeDataAttributes,
            `M ${startX} ${startY} C ${loopX} ${controlY1}, ${loopX} ${controlY2}, ${endX} ${endY}`,
            title,
            stroke,
            startX,
            startY,
            loopX,
            ((startY + endY) / 2) + laneOffset - 10,
          );
        }

        const bendOffset = laneOffset + (reverseCount ? (forward ? -18 : 18) : 0);
        if (sameColumn) {
          const routeOnLeft = sourceObject.column === 0;
          const outerX = routeOnLeft
            ? Math.min(sourceObject.x, targetObject.x) - outerColumnLinkMargin - Math.abs(bendOffset)
            : Math.max(
                sourceObject.x + objectWidth,
                targetObject.x + objectWidth,
              ) + outerColumnLinkMargin + Math.abs(bendOffset);
          const columnStartX = sourceObject.x + (routeOnLeft ? portInset : objectWidth - portInset);
          const columnEndX = targetObject.x + (routeOnLeft ? portInset : objectWidth - portInset);
          const branchX = routeOnLeft ? columnStartX - 26 : columnStartX + 26;
          const branchEndX = routeOnLeft ? columnEndX - 26 : columnEndX + 26;
          const midY = ((startY + endY) / 2) + bendOffset;
          const endApproachY = graphEndApproachY(startY, endY);
          return renderEdgeMarkup(
            relationship,
            edgeDataAttributes,
            `M ${columnStartX} ${startY} C ${branchX} ${startY}, ${outerX} ${startY}, ${outerX} ${midY}
              C ${outerX} ${endY}, ${branchEndX} ${endApproachY}, ${columnEndX} ${endY}`,
            title,
            stroke,
            columnStartX,
            startY,
            outerX,
            midY - 10,
          );
        }
        const deltaX = endX - startX;
        const controlX1 = startX + (deltaX * 0.3);
        const controlX2 = startX + (deltaX * 0.7);
        const controlY1 = startY + bendOffset;
        const controlY2 = endY + bendOffset;
        return renderEdgeMarkup(
          relationship,
          edgeDataAttributes,
          `M ${startX} ${startY} C ${controlX1} ${controlY1}, ${controlX2} ${controlY2}, ${endX} ${endY}`,
          title,
          stroke,
          startX,
          startY,
          startX + (deltaX / 2),
          ((startY + endY) / 2) + bendOffset - 10,
        );
      }).join('');

      const objectFill = (object) => {
        if (object.unresolved) return '#fff6f6';
        return '#ffffff';
      };
      const objectStroke = (object) => {
        if (object.unresolved) return '#e2b6b6';
        return '#8fc5e6';
      };
      const resourceFill = (resource) => {
        if (resource.incoming > 0 && resource.outgoing > 0) return '#fff2d8';
        if (resource.outgoing > 0) return '#e8f4fc';
        return '#f5f7fa';
      };
      const resourceStroke = (resource) => {
        if (resource.incoming > 0 && resource.outgoing > 0) return '#f0c36a';
        if (resource.outgoing > 0) return '#8fc5e6';
        return '#d3dde6';
      };

      const objectBackgroundMarkup = objects.map((object) => `
        <g class="relationship-graph-object" data-object-key="${escapeHtml(object.key)}" transform="translate(${object.x}, ${object.y})">
          <rect width="${objectWidth}" height="${object.height}" rx="10"
            fill="${objectFill(object)}"
            stroke="${objectStroke(object)}"
            stroke-width="1.3"></rect>
          <text x="10" y="${compactGraph ? 12 : 18}" fill="#22303c" font-size="${compactGraph ? 12 : 12}" font-weight="700">${escapeHtml(truncateGraphLabel(object.label, compactGraph ? 20 : 22))}</text>
          ${compactGraph ? '' : `<text x="10" y="32" fill="#5a6773" font-size="9">${escapeHtml(object.unresolved ? 'unresolved object' : 'daemon / definition object')}</text>`}
          <title>${escapeHtml(object.label)}</title>
        </g>`).join('');
      const objectResourceMarkup = objects.map((object) => `
        <g transform="translate(${object.x}, ${object.y})">
          ${object.resources.map((resource) => {
            const rowTop = (resource.y - object.y) - (resourceBoxHeight / 2);
            const rowLeft = resource.x - object.x;
            const resourceRef = graphResourceRef(object, resource);
            const typeLabel = truncateGraphLabel((resource.typeLabel || '').toUpperCase(), 10);
            const nameLabel = truncateGraphLabel(resource.nameLabel, 14);
            const fallbackLabel = truncateGraphLabel(resource.label, 21);
            return `<g class="relationship-graph-resource" data-object-key="${escapeHtml(object.key)}" data-resource-ref="${escapeHtml(resourceRef)}"${resource.resourceId ? ` data-resource-id="${escapeHtml(resource.resourceId)}"` : ''} transform="translate(${rowLeft}, ${rowTop})">
              <rect width="${resource.width}" height="${resourceBoxHeight}" rx="6"
                fill="${resourceFill(resource)}"
                stroke="${resourceStroke(resource)}"
                stroke-width="1.1"></rect>
              <text x="8" y="12" fill="#22303c" font-size="10">${resource.typeLabel
                  ? `<tspan font-size="8" font-weight="700" letter-spacing="0.8" fill="#2b6b8f">${escapeHtml(typeLabel)}</tspan>${resource.nameLabel
                      ? `<tspan dx="6" font-size="10.5" font-style="italic" font-weight="500" fill="#22303c">${escapeHtml(nameLabel)}</tspan>`
                      : ''}`
                  : `<tspan font-weight="600">${escapeHtml(fallbackLabel)}</tspan>`}</text>
              ${resource.incoming > 0 ? `<circle cx="3" cy="${resourceBoxHeight / 2}" r="2.7" fill="${resourceStroke(resource)}"></circle>` : ''}
              ${resource.outgoing > 0 ? `<circle cx="${resource.width - 3}" cy="${resourceBoxHeight / 2}" r="2.7" fill="${resourceStroke(resource)}"></circle>` : ''}
              <title>${escapeHtml(resource.label)}${resource.path ? ` — ${resource.path}` : ''}</title>
            </g>`;
          }).join('')}
        </g>`).join('');

      const graphHeight = topPadding + columnHeight + 24;
      const clickableNodes = objects
        .filter((object) => object.nodeId)
        .sort((left, right) => left.label.localeCompare(right.label));

      return `<div class="relationship-graph-card">
        <h5>${escapeHtml(title)}</h5>
        <div class="muted">${escapeHtml(description)} The table below stays the detailed source of truth.</div>
        <div class="relationship-graph">
          <svg width="${graphWidth}" height="${graphHeight}" viewBox="0 0 ${graphWidth} ${graphHeight}" role="img" aria-label="Relationship graph">
            <defs>
              <marker id="${markerId}" viewBox="0 0 10 10" refX="8" refY="5" markerWidth="7" markerHeight="7" orient="auto">
                <path d="M 0 0 L 10 5 L 0 10 z" fill="context-stroke"></path>
              </marker>
            </defs>
            ${objectBackgroundMarkup}
            ${objectResourceMarkup}
            ${edgeMarkup}
          </svg>
        </div>
        ${clickableNodes.length ? `<div class="relationship-graph-note muted">Jump to resolved node</div>
        <div class="relationship-graph-list">${clickableNodes.map((node) => `
          <button class="relationship-node relationship-graph-node-link" data-node-id="${escapeHtml(node.nodeId)}">${escapeHtml(node.label)}</button>`).join('')}</div>` : ''}
        <div class="relationship-graph-note muted">Block colors</div>
        ${renderRelationshipLegend([
          { color: '#e8f4fc', label: 'resource with outgoing relations' },
          { color: '#fff2d8', label: 'resource with incoming and outgoing relations' },
          { color: '#f5f7fa', label: 'resource mainly targeted by relations' },
          { color: '#fff6f6', label: 'unresolved object' },
        ])}
        <div class="relationship-graph-note muted">Arrow colors</div>
        ${renderRelationshipLegend(legendCounts.map((entry) => ({
          color: relationshipColor(entry.relation),
          label: `${entry.relation} · ${entry.count}`,
        })))}
      </div>`;
    }

    function renderRelationshipsTable(relationships) {
      const visibleRelationships = relationships.filter(
        (relationship) => `${relationship.target_resource_path || ''}`.trim(),
      );
      const relationshipResourceLabel = (relationship, side) => {
        const type = `${relationship[`${side}_resource_type`] || ''}`.trim();
        const name = `${relationship[`${side}_resource_name`] || ''}`.trim();
        if (type && name) {
          return `${type} ${name}`;
        }
        if (type) {
          return type;
        }
        if (name) {
          return name;
        }
        return '-';
      };
      const sortedRelationships = [...visibleRelationships].sort((left, right) => {
        const fromComparison = `${left.from_label || ''}`.localeCompare(
          `${right.from_label || ''}`,
          undefined,
          { sensitivity: 'base' },
        );
        if (fromComparison !== 0) {
          return fromComparison;
        }
        const toComparison = `${left.to_label || ''}`.localeCompare(
          `${right.to_label || ''}`,
          undefined,
          { sensitivity: 'base' },
        );
        if (toComparison !== 0) {
          return toComparison;
        }
        return `${left.relation || ''}`.localeCompare(
          `${right.relation || ''}`,
          undefined,
          { sensitivity: 'base' },
        );
      });
      const renderRelationshipTableSection = (title, sectionRelationships, emptyMessage) => `
        <div class="relationship-table-section">
          <h5>${escapeHtml(title)}</h5>
          ${sectionRelationships.length ? `<div class="table-scroll"><table>
        <thead>
          <tr>
            <th>From</th>
            <th>To</th>
            <th>Relation</th>
            <th>Endpoint</th>
            <th>Source resource</th>
            <th>Source defined in</th>
            <th>Target resource</th>
            <th>Target defined in</th>
            <th>Resolution</th>
          </tr>
        </thead>
        <tbody>${sectionRelationships.map((relationship) => `
          <tr>
            <td>${relationship.from_node_id
                ? `<button class="link-button relationship-node" data-node-id="${escapeHtml(relationship.from_node_id)}">${escapeHtml(relationship.from_label)}</button>`
                : escapeHtml(relationship.from_label)}</td>
            <td>${relationship.to_node_id
                ? `<button class="link-button relationship-node" data-node-id="${escapeHtml(relationship.to_node_id)}">${escapeHtml(relationship.to_label)}</button>`
                : escapeHtml(relationship.to_label)}</td>
            <td>${escapeHtml(relationship.relation)}${relationship.resolved ? '' : ' (unresolved)'}</td>
            <td>${escapeHtml(relationship.endpoint_name || '-')}</td>
            <td>${relationship.source_resource_id
                ? `<button class="link-button relationship-resource" data-resource-id="${escapeHtml(relationship.source_resource_id)}">${escapeHtml(relationshipResourceLabel(relationship, 'source'))}</button>`
                : escapeHtml(relationshipResourceLabel(relationship, 'source'))}</td>
            <td>${relationship.source_resource_id
                ? `<button class="link-button relationship-resource" data-resource-id="${escapeHtml(relationship.source_resource_id)}"><small>${escapeHtml(relationship.source_resource_path || '-')}</small></button>`
                : `<small>${escapeHtml(relationship.source_resource_path || '-')}</small>`}</td>
            <td>${relationship.target_resource_id
                ? `<button class="link-button relationship-resource" data-resource-id="${escapeHtml(relationship.target_resource_id)}">${escapeHtml(relationshipResourceLabel(relationship, 'target'))}</button>`
                : escapeHtml(relationshipResourceLabel(relationship, 'target'))}</td>
            <td>${relationship.target_resource_id
                ? `<button class="link-button relationship-resource" data-resource-id="${escapeHtml(relationship.target_resource_id)}"><small>${escapeHtml(relationship.target_resource_path || '-')}</small></button>`
                : `<small>${escapeHtml(relationship.target_resource_path || '-')}</small>`}</td>
            <td><small>${escapeHtml(relationship.resolution || '-')}</small></td>
          </tr>`).join('')}</tbody>
      </table></div>` : `<div class="muted">${escapeHtml(emptyMessage)}</div>`}
        </div>`;

      const internalRelationships = sortedRelationships.filter((relationship) =>
        isInternalRelationship(relationship),
      );
      const crossEntityRelationships = sortedRelationships.filter(
        (relationship) => !isInternalRelationship(relationship),
      );

      return `${renderRelationshipTableSection(
        'Internal relations',
        internalRelationships,
        'No internal relations discovered for this node.',
      )}${renderRelationshipTableSection(
        'Relations between entities',
        crossEntityRelationships,
        'No cross-entity relations discovered for this node.',
      )}`;
    }

    function renderRelationships(relationships) {
      const visibleRelationships = (relationships || []).filter(
        (relationship) => `${relationship.target_resource_path || ''}`.trim(),
      );
      if (!visibleRelationships.length) {
        return '<div class="muted">No resolved resource relationships discovered for this node yet.</div>';
      }

      const internalRelationships = visibleRelationships.filter((relationship) =>
        isInternalRelationship(relationship),
      );
      const crossEntityRelationships = visibleRelationships.filter(
        (relationship) => !isInternalRelationship(relationship),
      );

      return `<div class="relationship-view">
        ${renderRelationshipGraph(internalRelationships, {
          title: 'Internal relations graph',
          description: 'This graph shows only relations that stay inside the same entity or configuration object. Resources are distributed into two lanes: resources with outgoing relations stay on the left, with the mixed incoming/outgoing resources placed at the bottom of that lane, while mainly targeted resources stay on the right so internal arrows remain easier to follow and the type and name remain visually distinct.',
          emptyMessage: 'No internal relations discovered for this node.',
          graphId: 'internal',
          splitObjectResources: true,
        })}
        ${renderRelationshipGraph(crossEntityRelationships, {
          title: 'Relations between entities graph',
          description: 'This graph shows only relations that connect one entity or configuration object to another. The nested rows are resource entries with the type and name styled separately, so you can see exactly which resource in one configuration links to which resource in another configuration.',
          emptyMessage: 'No cross-entity relations discovered for this node.',
          graphId: 'cross-entity',
        })}
        ${renderRelationshipsTable(visibleRelationships)}
      </div>`;
    }

    function clearRelationshipGraphHighlight(graphEl) {
      graphEl.classList.remove('has-active-highlight');
      graphEl.querySelectorAll('.relationship-graph-object, .relationship-graph-resource, .relationship-graph-edge')
        .forEach((element) => {
          element.classList.remove('is-highlighted');
        });
    }

    function applyRelationshipGraphHighlight(graphEl, highlightedEdges, highlightedResourceRefs, highlightedObjectKeys) {
      graphEl.classList.add('has-active-highlight');
      graphEl.querySelectorAll('.relationship-graph-edge').forEach((edgeEl) => {
        edgeEl.classList.toggle('is-highlighted', highlightedEdges.has(edgeEl));
      });
      graphEl.querySelectorAll('.relationship-graph-resource').forEach((resourceEl) => {
        resourceEl.classList.toggle(
          'is-highlighted',
          highlightedResourceRefs.has(resourceEl.dataset.resourceRef),
        );
      });
      graphEl.querySelectorAll('.relationship-graph-object').forEach((objectEl) => {
        objectEl.classList.toggle(
          'is-highlighted',
          highlightedObjectKeys.has(objectEl.dataset.objectKey),
        );
      });
    }

    function attachRelationshipGraphHover() {
      detailsEl.querySelectorAll('.relationship-graph').forEach((graphEl) => {
        const highlightSelection = (highlightedEdges, highlightedResourceRefs, highlightedObjectKeys) => {
          if (!highlightedEdges.size && !highlightedResourceRefs.size && !highlightedObjectKeys.size) {
            clearRelationshipGraphHighlight(graphEl);
            return;
          }
          applyRelationshipGraphHighlight(
            graphEl,
            highlightedEdges,
            highlightedResourceRefs,
            highlightedObjectKeys,
          );
        };

        graphEl.querySelectorAll('.relationship-graph-resource').forEach((resourceEl) => {
          resourceEl.addEventListener('mouseenter', () => {
            const highlightedEdges = new Set();
            const highlightedResourceRefs = new Set([resourceEl.dataset.resourceRef]);
            const highlightedObjectKeys = new Set([resourceEl.dataset.objectKey]);
            graphEl.querySelectorAll('.relationship-graph-edge').forEach((edgeEl) => {
              if (edgeEl.dataset.sourceResourceRef === resourceEl.dataset.resourceRef
                  || edgeEl.dataset.targetResourceRef === resourceEl.dataset.resourceRef) {
                highlightedEdges.add(edgeEl);
                highlightedResourceRefs.add(edgeEl.dataset.sourceResourceRef);
                highlightedResourceRefs.add(edgeEl.dataset.targetResourceRef);
                highlightedObjectKeys.add(edgeEl.dataset.sourceObjectKey);
                highlightedObjectKeys.add(edgeEl.dataset.targetObjectKey);
              }
            });
            highlightSelection(highlightedEdges, highlightedResourceRefs, highlightedObjectKeys);
          });
          resourceEl.addEventListener('mouseleave', () => {
            clearRelationshipGraphHighlight(graphEl);
          });
        });

        graphEl.querySelectorAll('.relationship-graph-object').forEach((objectEl) => {
          objectEl.addEventListener('mouseenter', () => {
            const highlightedEdges = new Set();
            const highlightedResourceRefs = new Set();
            const highlightedObjectKeys = new Set([objectEl.dataset.objectKey]);
            graphEl.querySelectorAll('.relationship-graph-resource').forEach((resourceEl) => {
              if (resourceEl.dataset.objectKey === objectEl.dataset.objectKey) {
                highlightedResourceRefs.add(resourceEl.dataset.resourceRef);
              }
            });
            graphEl.querySelectorAll('.relationship-graph-edge').forEach((edgeEl) => {
              if (edgeEl.dataset.sourceObjectKey === objectEl.dataset.objectKey
                  || edgeEl.dataset.targetObjectKey === objectEl.dataset.objectKey) {
                highlightedEdges.add(edgeEl);
                highlightedResourceRefs.add(edgeEl.dataset.sourceResourceRef);
                highlightedResourceRefs.add(edgeEl.dataset.targetResourceRef);
                highlightedObjectKeys.add(edgeEl.dataset.sourceObjectKey);
                highlightedObjectKeys.add(edgeEl.dataset.targetObjectKey);
              }
            });
            highlightSelection(highlightedEdges, highlightedResourceRefs, highlightedObjectKeys);
          });
          objectEl.addEventListener('mouseleave', () => {
            clearRelationshipGraphHighlight(graphEl);
          });
        });

        graphEl.querySelectorAll('.relationship-graph-edge').forEach((edgeEl) => {
          edgeEl.addEventListener('mouseenter', () => {
            const highlightedEdges = new Set([edgeEl]);
            const highlightedResourceRefs = new Set([
              edgeEl.dataset.sourceResourceRef,
              edgeEl.dataset.targetResourceRef,
            ]);
            const highlightedObjectKeys = new Set([
              edgeEl.dataset.sourceObjectKey,
              edgeEl.dataset.targetObjectKey,
            ]);
            highlightSelection(highlightedEdges, highlightedResourceRefs, highlightedObjectKeys);
          });
          edgeEl.addEventListener('mouseleave', () => {
            clearRelationshipGraphHighlight(graphEl);
          });
        });
      });
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
      detailsEl.querySelectorAll('.relationship-graph-resource[data-resource-id]').forEach((resourceEl) => {
        resourceEl.addEventListener('click', () => {
          showResourceDetails(resourceEl.dataset.resourceId);
        });
      });
      attachRelationshipGraphHover();
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
            initializeScheduleRunEditors(wizardFieldsEl);
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
        if ((options.resourceType || '') === 'schedule' && field.key === 'Run' && field.repeatable) {
          return renderScheduleRunField(field, options);
        }
        if (field.repeatable && field.allowed_values && field.allowed_values.length
            && field.datatype === 'RESOURCE_LIST') {
          const selectedValues = new Set(
            normalizeRepeatableFieldValues(field.present ? field.value : ''),
          );
          const optionValues = Array.from(
            new Set([
              ...field.allowed_values.map((value) => stripSurroundingQuotes(value)),
              ...Array.from(selectedValues),
            ]),
          );
          return `<select
              class="repeatable-field"
              multiple
              size="${Math.min(Math.max(optionValues.length, 2), 4)}"
              data-field-key="${escapeHtml(field.key)}"
              data-field-required="${field.required ? 'true' : 'false'}"
              data-field-repeatable="true"
              data-field-quote-char="${escapeHtml(detectRepeatableFieldQuote(field.value || ''))}"
            >${optionValues.map((value) => `<option value="${escapeHtml(value)}"${selectedValues.has(value) ? ' selected' : ''}>${escapeHtml(value)}</option>`).join('')}</select>`;
        }
        if (field.repeatable) {
          return `<textarea
              class="repeatable-field"
              data-field-key="${escapeHtml(field.key)}"
              data-field-required="${field.required ? 'true' : 'false'}"
              data-field-repeatable="true"
              data-field-quote-char="${escapeHtml(detectRepeatableFieldQuote(field.value || ''))}"
              placeholder="${escapeHtml(stripSurroundingQuotes((!field.present && field.inherited_value) ? field.inherited_value : ''))}"
              rows="2"
            >${escapeHtml(normalizeRepeatableFieldValues(field.value || '').join('\n'))}</textarea>`;
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
      let currentScheduleRunAllowedValues = resource.schedule_run_override_allowed_values || {};
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
          resourceType: resource.type,
          scheduleRunAllowedValues: currentScheduleRunAllowedValues,
        });
        initializeScheduleRunEditors(fieldHintsEl);
        if (toggleEmptyFieldsButton) {
          toggleEmptyFieldsButton.textContent = hideEmptyOptional
            ? 'Show empty and unset fields'
            : 'Hide empty and unset fields';
        }
      };

      const applyPreviewToEditor = (preview) => {
        currentFieldHints = preview.updated_field_hints;
        currentScheduleRunAllowedValues = preview.schedule_run_override_allowed_values
          || currentScheduleRunAllowedValues;
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
            if (payload.saved) {
              fetch('/api/v1/bootstrap')
                .then((response) => response.json())
                .then((data) => {
                  loadedNodeResources.clear();
                  renderTree(data);
                })
                .catch(() => {});
            }
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
      if (currentTreeData && currentTreeData.tree) {
        const ownerNodeId = findNodeContainingResource(currentTreeData.tree, resourceId);
        if (ownerNodeId) {
          selectedNodeId = ownerNodeId;
          expandedTreeIds.add(`tree-node-${ownerNodeId}`);
        }
      }
      selectedResourceId = resourceId;
      if (currentTreeData) {
        renderTree(currentTreeData);
      }
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

    function fetchJson(url, options) {
      return fetch(url, options).then(async (response) => {
        const body = await response.text();
        if (!response.ok) {
          const detail = body || response.statusText || 'empty response';
          throw new Error(`HTTP ${response.status}: ${detail}`);
        }
        if (!body) {
          throw new Error(`Empty response from ${url}`);
        }
        try {
          return JSON.parse(body);
        } catch (error) {
          throw new Error(`Invalid JSON from ${url}: ${error}`);
        }
      });
    }

    function showNewResourceEditor(nodeId, resourceType) {
      selectedNodeId = nodeId;
      selectedResourceId = '';
      if (currentTreeData) {
        renderTree(currentTreeData);
      }
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
      selectedNodeId = nodeId;
      selectedResourceId = '';
      if (currentTreeData) {
        renderTree(currentTreeData);
      }
      fetchJson(`/api/v1/nodes/${nodeId}`)
        .then((node) => {
          loadNodeResources(node);
        })
        .catch((error) => {
          detailsEl.textContent = `Failed to load node: ${error}`;
        });
    }

    function renderNodeDetails(node, resources, relationships) {
      const creatableTypes = Array.isArray(node.creatable_resource_types)
        ? node.creatable_resource_types
        : [];
      const managementState = clientManagementState(node);
      detailsEl.innerHTML = `
        <h4 style="margin-top: 0;">${escapeHtml(node.label)}</h4>
        <div class="muted" style="margin-bottom: 12px;">
          Kind: ${escapeHtml(node.kind)}<br>
          ${managementState ? `Management: ${escapeHtml(managementState.description)}<br>` : ''}
          Location: ${escapeHtml(node.description || '-')}<br>
          Resources are shown in the tree on the left (${resources.length} total).
        </div>
        ${creatableTypes.length
          ? `<div style="margin: 12px 0 16px;">
              <label for="new-resource-type"><strong>Add resource</strong></label><br>
              <select id="new-resource-type" style="margin-top: 8px;">${creatableTypes.map((type) => `<option value="${escapeHtml(type)}">${escapeHtml(type)}</option>`).join('')}</select>
              <button id="create-resource" style="margin-top: 0; margin-left: 8px;">Add resource</button>
            </div>`
          : ''}
        ${node.kind === 'client'
          ? `<div><button id="remove-remote-client">Remove remote client</button></div>
             <div id="remove-remote-client-status" class="muted" style="margin-top: 8px;"></div>`
          : ''}
        <h4>Relationships</h4>
        <div>${renderRelationships(relationships)}</div>`;

      if (creatableTypes.length) {
        document.getElementById('create-resource').addEventListener('click', () => {
          showNewResourceEditor(
            node.id,
            document.getElementById('new-resource-type').value,
          );
        });
      }

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
                  loadedNodeResources.clear();
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
        fetchJson(`/api/v1/nodes/${node.id}/resources`),
        fetchJson(`/api/v1/nodes/${node.id}/relationships`),
      ])
        .then(([resources, relationships]) => {
          loadedNodeResources.set(node.id, resources);
          renderTree(currentTreeData);
          if (node.kind === 'datacenter') {
            renderAddClientWizard(node);
            return;
          }
          renderNodeDetails(node, resources, relationships);
        })
        .catch((error) => {
          detailsEl.textContent = `Failed to load node resources: ${error}`;
        });
    }

    function renderGroup(group, parentEl) {
      const wrapper = document.createElement('div');
      const expanded = isTreeExpanded(group.id);
      const rowEl = document.createElement('div');
      rowEl.className = 'tree-group';
      const entryEl = document.createElement('div');
      entryEl.className = 'tree-entry';
      entryEl.appendChild(renderTreeToggle(expanded, () => toggleTreeExpansion(group.id)));
      const labelEl = document.createElement('div');
      labelEl.className = 'tree-label';
      labelEl.innerHTML = `<span class="tree-icon-label">${iconForGroup(group.id)}<span>${escapeHtml(group.label)}</span></span>`;
      entryEl.appendChild(labelEl);
      rowEl.appendChild(entryEl);
      wrapper.appendChild(rowEl);

      const childrenEl = document.createElement('div');
      childrenEl.className = 'children';
      if (expanded) {
        group.nodes.forEach((node) => renderNode(node, childrenEl));
      }
      wrapper.appendChild(childrenEl);
      parentEl.appendChild(wrapper);
    }

    function renderNode(node, parentEl) {
      const resources = getNodeResourcesForTree(node);
      const nodeExpansionId = `tree-node-${node.id}`;
      const hasExpandableContent = resources.length || (node.children || []).length;
      const expanded = hasExpandableContent && isTreeExpanded(nodeExpansionId);
      const wrapper = document.createElement('div');
      const rowEl = document.createElement('div');
      rowEl.className = `node${selectedNodeId === node.id && !selectedResourceId ? ' selected' : ''}`;
      const entryEl = document.createElement('div');
      entryEl.className = 'tree-entry';
      entryEl.appendChild(
        renderTreeToggle(expanded, () => toggleTreeExpansion(nodeExpansionId), hasExpandableContent),
      );
      const labelEl = document.createElement('div');
      labelEl.className = 'tree-label';
      labelEl.innerHTML = `<span class="tree-icon-label">${iconForNode(node)}<strong>${escapeHtml(node.label)}</strong>${renderClientManagementBadge(node)}</span>`;
      entryEl.appendChild(labelEl);
      rowEl.appendChild(entryEl);
      rowEl.addEventListener('click', () => {
        loadNodeById(node.id);
      });
      wrapper.appendChild(rowEl);

      const childrenEl = document.createElement('div');
      childrenEl.className = 'children';
      if (expanded) {
        resources.forEach((resource) => {
          const resourceEl = document.createElement('div');
          const isSelectedResource = selectedResourceId === resource.id;
          resourceEl.className = `resource${isSelectedResource ? ' selected' : ''}`;
          resourceEl.innerHTML = `<span class="tree-icon-label">${iconForResource(resource)}<span>${escapeHtml(stripSurroundingQuotes(resource.name))}</span></span>
            <small>${escapeHtml(resource.type)}</small>`;
          resourceEl.addEventListener('click', (event) => {
            event.stopPropagation();
            selectedNodeId = node.id;
            showResourceDetails(resource.id);
          });
          childrenEl.appendChild(resourceEl);
        });
        (node.children || []).forEach((child) => renderNode(child, childrenEl));
      }
      wrapper.appendChild(childrenEl);
      parentEl.appendChild(wrapper);
    }

    function renderDatacenterTree(root, parentEl) {
      const wrapper = document.createElement('div');
      const rowEl = document.createElement('div');
      rowEl.className = `node${selectedNodeId === root.id && !selectedResourceId ? ' selected' : ''}`;
      const entryEl = document.createElement('div');
      entryEl.className = 'tree-entry';
      entryEl.appendChild(renderTreeToggle(true, () => {}, false));
      const labelEl = document.createElement('div');
      labelEl.className = 'tree-label';
      labelEl.innerHTML = `<span class="tree-icon-label">${iconMarkup('datacenter')}<span><strong>${escapeHtml(root.label)}</strong> <small class="muted">${escapeHtml(root.kind)}</small></span></span>`;
      entryEl.appendChild(labelEl);
      rowEl.appendChild(entryEl);
      rowEl.addEventListener('click', () => {
        loadNodeById(root.id);
      });
      wrapper.appendChild(rowEl);

      const childrenEl = document.createElement('div');
      childrenEl.className = 'children';
      groupedTreeEntries(root).forEach((group) => renderGroup(group, childrenEl));
      wrapper.appendChild(childrenEl);
      parentEl.appendChild(wrapper);
    }

    function renderTree(data) {
      currentTreeData = data;
      const root = data.tree;
      if (!root || !(root.children || []).length) {
        treeEl.innerHTML = '<div class="muted">No Bareos directors discovered.</div>';
        return;
      }
      treeEl.innerHTML = '';
      renderDatacenterTree(root, treeEl);
    }

    fetch('/api/v1/bootstrap')
      .then((response) => response.json())
      .then((data) => {
        renderTree(data);
        loadNodeById('datacenter');
      })
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
  static std::mutex request_mutex;
  const std::lock_guard<std::mutex> lock(request_mutex);

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
            SerializeBootstrapSummary(options, model)};
  }

  if (request.method == "GET" && request.path == "/api/v1/tree") {
    const auto model = DiscoverDatacenterSummary(options.config_roots);
    return {200, "application/json; charset=utf-8",
            SerializeTreeNodeSummary(BuildAugmentedTree(options, model))};
  }

  const auto path_parts = SplitPath(request.path);
  if (request.method == "GET" && path_parts.size() == 4 && path_parts[0] == "api"
      && path_parts[1] == "v1"
      && path_parts[2] == "nodes") {
    const auto model = DiscoverDatacenterSummary(options.config_roots);
    const auto node = ResolveNodeSummary(options, model, path_parts[3]);
    if (!node) return {404, "text/plain; charset=utf-8", "Node Not Found"};
    return {200, "application/json; charset=utf-8", SerializeNodeDetail(*node)};
  }

  if (request.method == "GET" && path_parts.size() == 5
      && path_parts[0] == "api" && path_parts[1] == "v1"
      && path_parts[2] == "nodes" && path_parts[4] == "resources") {
    const auto model = DiscoverDatacenterSummary(options.config_roots);
    const auto node = ResolveNodeSummary(options, model, path_parts[3]);
    if (!node) return {404, "text/plain; charset=utf-8", "Node Not Found"};
    return {200, "application/json; charset=utf-8",
            SerializeResourceSummaries(
                CollectNodeResources(options, model, *node))};
  }

  if (request.method == "GET" && path_parts.size() == 5
      && path_parts[0] == "api" && path_parts[1] == "v1"
      && path_parts[2] == "nodes" && path_parts[4] == "relationships") {
    const auto model = DiscoverDatacenterSummary(options.config_roots);
    const auto node = ResolveNodeSummary(options, model, path_parts[3]);
    if (!node) return {404, "text/plain; charset=utf-8", "Node Not Found"};
    return {200, "application/json; charset=utf-8",
              SerializeRelationshipSummaries(
                  FindRelationshipsForNode(model, path_parts[3]))};
  }

  if (request.method == "POST" && path_parts.size() == 5
      && path_parts[0] == "api" && path_parts[1] == "v1"
      && path_parts[2] == "nodes" && path_parts[4] == "remove") {
    const auto model = DiscoverDatacenterSummary(options.config_roots);
    const auto node = ResolveNodeSummary(options, model, path_parts[3]);
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
            SerializeAddClientWizardSchema(options, *node)};
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
            SerializeAddClientWizardSchema(options, *director)};
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
