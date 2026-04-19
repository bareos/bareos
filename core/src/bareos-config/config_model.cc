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
#include "config_model.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <fstream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <utility>
#include <unistd.h>

#define NEED_JANSSON_NAMESPACE 1
#include <jansson.h>
#include "console/console_conf.h"
#include "console/console_globals.h"
#include "dird/dird_conf.h"
#include "dird/dird_globals.h"
#include "filed/filed_conf.h"
#include "filed/filed_globals.h"
#include "include/bareos.h"
#include "lib/bareos_resource.h"
#include "lib/output_formatter.h"
#include "lib/output_formatter_resource.h"
#include "lib/parse_conf.h"
#include "lib/resource_item.h"
#include "stored/stored_conf.h"
#include "stored/stored_globals.h"

namespace {
enum class ConfigComponent {
  kUnknown,
  kDirector,
  kFileDaemon,
  kStorageDaemon,
  kConsole,
};

struct ParserBinding {
  ConfigurationParser* parser = nullptr;
  void (*assign_global)(ConfigurationParser*) = nullptr;
};

struct DirectiveMetadata {
  std::string key;
  std::string datatype;
  std::string description;
  std::string default_value;
  std::string related_resource_type;
  bool required = false;
  bool repeatable = false;
  bool deprecated = false;
};

struct PrintedDirective {
  ResourceDirective directive;
  bool inherited = false;
};

struct ParsedResourceDump {
  std::string content;
  std::vector<PrintedDirective> directives;
};

std::map<std::string, DirectiveMetadata> BuildDirectiveMetadata(
    const ResourceSummary& summary);
std::vector<ResourceSummary> DiscoverResourcesInDirectory(
    const std::filesystem::path& path,
    const std::string& id_prefix,
    const std::string& component,
    const std::string& config_root);
bool StageEditedContentInConfigRoot(const ResourceSummary& resource,
                                    const std::string& content,
                                    const std::filesystem::path& staged_root,
                                    std::filesystem::path& staged_file);
const ResourceTable* FindResourceTableByNormalizedType(
    const ConfigurationParser& parser, const std::string& normalized_type);
const ResourceTable* LookupResourceTable(const ResourceSummary& summary);
const ResourceItem* LookupDirectiveResourceItem(const ResourceSummary& summary,
                                                 const ResourceTable* table,
                                                 const std::string& key);
std::string TrimWhitespace(std::string value);
ParsedResourceDump DumpResourceFromParser(const ResourceSummary& summary);
thread_local ConfigurationParser* standalone_parser = nullptr;

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

struct ScheduleRunOverrideMetadata {
  const char* directive_key;
  const char* related_resource_type;
};

const ScheduleRunOverrideMetadata* LookupScheduleRunOverrideMetadata(
    const std::string& key)
{
  static const std::map<std::string, ScheduleRunOverrideMetadata> kMetadata{
      {"pool", {"Pool", "pool"}},
      {"fullpool", {"FullPool", "pool"}},
      {"incrementalpool", {"IncrementalPool", "pool"}},
      {"differentialpool", {"DifferentialPool", "pool"}},
      {"nextpool", {"NextPool", "pool"}},
      {"storage", {"Storage", "storage"}},
      {"messages", {"Messages", "messages"}},
  };

  const auto it = kMetadata.find(NormalizeDirectiveNameForComparison(key));
  return it == kMetadata.end() ? nullptr : &it->second;
}

std::string NormalizeBooleanValue(const std::string& value)
{
  const auto normalized = NormalizeDirectiveNameForComparison(value);
  if (normalized == "yes" || normalized == "true") return "yes";
  if (normalized == "no" || normalized == "false") return "no";
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

std::vector<ScheduleRunOverrideReference> ParseScheduleRunOverrideReferences(
    const std::string& run_value, size_t line, size_t run_index)
{
  std::vector<ScheduleRunOverrideReference> references;
  size_t position = 0;

  while (position < run_value.size()) {
    while (position < run_value.size()
           && std::isspace(static_cast<unsigned char>(run_value[position]))) {
      ++position;
    }
    if (position >= run_value.size()) break;

    const auto key_start = position;
    while (position < run_value.size() && run_value[position] != '='
           && !std::isspace(static_cast<unsigned char>(run_value[position]))) {
      ++position;
    }

    const auto key_end = position;
    while (position < run_value.size()
           && std::isspace(static_cast<unsigned char>(run_value[position]))) {
      ++position;
    }

    if (position >= run_value.size() || run_value[position] != '=') {
      continue;
    }

    auto key = run_value.substr(key_start, key_end - key_start);
    ++position;

    while (position < run_value.size()
           && std::isspace(static_cast<unsigned char>(run_value[position]))) {
      ++position;
    }
    if (position >= run_value.size()) break;

    const auto value_start = position;
    if (run_value[position] == '"' || run_value[position] == '\'') {
      const auto quote = run_value[position];
      ++position;
      while (position < run_value.size() && run_value[position] != quote) {
        ++position;
      }
      if (position < run_value.size()) ++position;
    } else {
      while (position < run_value.size()
             && !std::isspace(static_cast<unsigned char>(run_value[position]))) {
        ++position;
      }
    }
    const auto value_end = position;

    const auto* metadata = LookupScheduleRunOverrideMetadata(key);
    if (!metadata) continue;

    const auto raw_value = run_value.substr(value_start, value_end - value_start);
    const auto referenced_name = StripSurroundingQuotes(raw_value);
    if (referenced_name.empty()) continue;

    references.push_back({metadata->directive_key,
                          metadata->related_resource_type,
                          referenced_name,
                          raw_value,
                          line,
                          run_index,
                          value_start,
                          value_end - value_start});
  }

  return references;
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
  return summary.name;
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

int DirectiveSortGroup(const std::string& key, bool required)
{
  if (NormalizeDirectiveNameForComparison(key) == "name") return 0;
  return required ? 1 : 2;
}

void SortResourceFieldHints(std::vector<ResourceFieldHint>& hints)
{
  std::sort(hints.begin(), hints.end(),
            [](const ResourceFieldHint& left, const ResourceFieldHint& right) {
              const auto left_group = DirectiveSortGroup(left.key, left.required);
              const auto right_group
                  = DirectiveSortGroup(right.key, right.required);
              if (left_group != right_group) return left_group < right_group;
              return left.key < right.key;
            });
}

void SortResourceDirectivesForDisplay(
    const ResourceSummary& summary, std::vector<ResourceDirective>& directives)
{
  const auto* table = LookupResourceTable(summary);
  const auto directive_metadata = BuildDirectiveMetadata(summary);

  std::sort(directives.begin(), directives.end(),
            [&](const ResourceDirective& left, const ResourceDirective& right) {
              const auto left_top_level = IsTopLevelDirective(left);
              const auto right_top_level = IsTopLevelDirective(right);
              if (left_top_level != right_top_level) return left_top_level;

              if (!left_top_level && !right_top_level) {
                if (left.nesting_level != right.nesting_level) {
                  return left.nesting_level < right.nesting_level;
                }
                return left.line < right.line;
              }

              auto left_key = left.key;
              if (const auto* item
                  = LookupDirectiveResourceItem(summary, table, left.key)) {
                left_key = item->name;
              }
              auto right_key = right.key;
              if (const auto* item
                  = LookupDirectiveResourceItem(summary, table, right.key)) {
                right_key = item->name;
              }

              const auto left_metadata = directive_metadata.find(left_key);
              const auto right_metadata = directive_metadata.find(right_key);
              const auto left_required
                  = left_metadata != directive_metadata.end()
                        ? left_metadata->second.required
                        : false;
              const auto right_required
                  = right_metadata != directive_metadata.end()
                        ? right_metadata->second.required
                        : false;
              const auto left_group
                  = DirectiveSortGroup(left_key, left_required);
              const auto right_group
                  = DirectiveSortGroup(right_key, right_required);
              if (left_group != right_group) return left_group < right_group;
              if (left_key != right_key) return left_key < right_key;
              return left.line < right.line;
            });
}

std::string NormalizeParserResourceType(const std::string& value)
{
  return Lowercase(value);
}

void AssignDirectorParser(ConfigurationParser* parser)
{
  directordaemon::my_config = parser;
}

void AssignFileDaemonParser(ConfigurationParser* parser)
{
  filedaemon::my_config = parser;
}

void AssignStorageDaemonParser(ConfigurationParser* parser)
{
  storagedaemon::my_config = parser;
}

void AssignConsoleParser(ConfigurationParser* parser) { console::my_config = parser; }

ParserBinding GetParserBinding(ConfigComponent component)
{
  switch (component) {
    case ConfigComponent::kDirector: {
      static auto parser = std::unique_ptr<ConfigurationParser>(
          directordaemon::InitDirConfig(nullptr, M_INFO));
      AssignDirectorParser(parser.get());
      return {parser.get(), &AssignDirectorParser};
    }
    case ConfigComponent::kFileDaemon: {
      static auto parser = std::unique_ptr<ConfigurationParser>(
          filedaemon::InitFdConfig(nullptr, M_INFO));
      AssignFileDaemonParser(parser.get());
      return {parser.get(), &AssignFileDaemonParser};
    }
    case ConfigComponent::kStorageDaemon: {
      static auto parser = std::unique_ptr<ConfigurationParser>(
          storagedaemon::InitSdConfig(nullptr, M_INFO));
      AssignStorageDaemonParser(parser.get());
      return {parser.get(), &AssignStorageDaemonParser};
    }
    case ConfigComponent::kConsole: {
      static auto parser = std::unique_ptr<ConfigurationParser>(
          console::InitConsConfig(nullptr, M_INFO));
      AssignConsoleParser(parser.get());
      return {parser.get(), &AssignConsoleParser};
    }
    case ConfigComponent::kUnknown:
      return {};
  }

  return {};
}

ConfigComponent ComponentForSummary(const ResourceSummary& summary)
{
  if (summary.component == "director") {
    return ConfigComponent::kDirector;
  }
  if (summary.component == "file-daemon") {
    return ConfigComponent::kFileDaemon;
  }
  if (summary.component == "storage-daemon") {
    return ConfigComponent::kStorageDaemon;
  }
  if (summary.component == "console") { return ConfigComponent::kConsole; }
  return ConfigComponent::kUnknown;
}

ConfigComponent ComponentFromString(const std::string& component)
{
  return ComponentForSummary(
      ResourceSummary{"", "", "", "", component, ""});
}

std::unique_ptr<ConfigurationParser> CreateParserForComponent(
    ConfigComponent component, const char* config_file)
{
  switch (component) {
    case ConfigComponent::kDirector:
      return std::unique_ptr<ConfigurationParser>(
          directordaemon::InitDirConfig(config_file, M_INFO));
    case ConfigComponent::kFileDaemon:
      return std::unique_ptr<ConfigurationParser>(
          filedaemon::InitFdConfig(config_file, M_INFO));
    case ConfigComponent::kStorageDaemon:
      return std::unique_ptr<ConfigurationParser>(
          storagedaemon::InitSdConfig(config_file, M_INFO));
    case ConfigComponent::kConsole:
      return std::unique_ptr<ConfigurationParser>(
          console::InitConsConfig(config_file, M_INFO));
    case ConfigComponent::kUnknown:
      return nullptr;
  }
  return nullptr;
}

std::string DefaultConfigIncludeDirForComponent(ConfigComponent component)
{
  switch (component) {
    case ConfigComponent::kDirector:
      return "bareos-dir.d";
    case ConfigComponent::kFileDaemon:
      return "bareos-fd.d";
    case ConfigComponent::kStorageDaemon:
      return "bareos-sd.d";
    case ConfigComponent::kConsole:
      return "bconsole.d";
    case ConfigComponent::kUnknown:
      break;
  }
  return "";
}

std::vector<ConfigComponent> CandidateComponentsForType(
    const std::string& resource_type, ConfigComponent preferred)
{
  std::vector<ConfigComponent> candidates;
  if (preferred != ConfigComponent::kUnknown) {
    auto parser = CreateParserForComponent(preferred, nullptr);
    if (parser && FindResourceTableByNormalizedType(*parser, resource_type)) {
      candidates.push_back(preferred);
    }
  }

  for (const auto component : {ConfigComponent::kDirector,
                               ConfigComponent::kFileDaemon,
                               ConfigComponent::kStorageDaemon,
                               ConfigComponent::kConsole}) {
    if (std::find(candidates.begin(), candidates.end(), component)
        != candidates.end()) {
      continue;
    }
    auto parser = CreateParserForComponent(component, nullptr);
    if (!parser) continue;
    if (FindResourceTableByNormalizedType(*parser, resource_type)) {
      candidates.push_back(component);
    }
  }

  if (candidates.empty()) {
    candidates = {ConfigComponent::kDirector, ConfigComponent::kFileDaemon,
                  ConfigComponent::kStorageDaemon, ConfigComponent::kConsole};
  }
  return candidates;
}

bool SaveStandaloneDiscoveryResource(int type, const ResourceItem* items, int pass)
{
  if (pass != 1 && pass != 2) {
    return true;
  }
  if (!standalone_parser || !items || !items->allocated_resource) {
    return true;
  }

  auto* resource = *items->allocated_resource;
  if (!resource) return true;

  if (resource->resource_name_) {
    for (auto* existing = standalone_parser->GetNextRes(type, nullptr); existing;
         existing = standalone_parser->GetNextRes(type, existing)) {
      if (bstrcmp(existing->resource_name_, resource->resource_name_)) {
        return true;
      }
    }
  }

  if (!standalone_parser->AppendToResourcesChain(resource, type)) {
    return false;
  }

  *items->allocated_resource = nullptr;
  return true;
}

bool HasParsedResources(const ConfigurationParser& parser)
{
  if (const auto* tables = parser.GetResourceTables()) {
    for (const auto* table = tables; table->name; ++table) {
      if (parser.GetNextRes(table->rcode, nullptr) != nullptr) return true;
    }
  }
  return false;
}

bool ParseStandaloneConfigFile(ConfigurationParser& parser,
                               const std::filesystem::path& config_file,
                               bool lenient)
{
  std::map<int, std::string> resource_type_names;
  if (const auto* tables = parser.GetResourceTables()) {
    for (const auto* table = tables; table->name; ++table) {
      resource_type_names.emplace(table->rcode, table->name);
    }
  }
  parser.InitializeQualifiedResourceNameTypeConverter(resource_type_names);
  parser.allow_unnamed_resources_ = lenient;
  if (lenient) {
    standalone_parser = &parser;
    parser.SaveResourceCb_ = &SaveStandaloneDiscoveryResource;
  }
  const auto parsed = parser.ParseConfigFile(config_file.c_str(), nullptr);
  if (lenient) { standalone_parser = nullptr; }
  return parsed || (lenient && HasParsedResources(parser));
}

bool ParseConfigIncludeTree(ConfigurationParser& parser,
                            const std::filesystem::path& config_root,
                            ConfigComponent component,
                            bool lenient)
{
  const auto include_dir = DefaultConfigIncludeDirForComponent(component);
  if (include_dir.empty() || config_root.empty()) return false;
  return ParseStandaloneConfigFile(
      parser, config_root / include_dir / "*" / "*.conf", lenient);
}

const ResourceTable* FindResourceTableByNormalizedType(
    const ConfigurationParser& parser, const std::string& normalized_type)
{
  const auto* tables = parser.GetResourceTables();
  if (!tables) return nullptr;

  for (const auto* table = tables; table->name; ++table) {
    if (NormalizeParserResourceType(table->name) == normalized_type) {
      return table;
    }
  }

  return nullptr;
}

const ResourceTable* LookupResourceTable(const ResourceSummary& summary)
{
  for (const auto candidate :
       CandidateComponentsForType(summary.type, ComponentForSummary(summary))) {
    const auto binding = GetParserBinding(candidate);
    if (!binding.parser) continue;
    if (binding.assign_global) { binding.assign_global(binding.parser); }
    if (const auto* table
        = FindResourceTableByNormalizedType(*binding.parser, summary.type)) {
      return table;
    }
  }
  return nullptr;
}

const ResourceItem* LookupDirectiveResourceItem(const ResourceSummary& summary,
                                                const ResourceTable* table,
                                                const std::string& key)
{
  if (!table) return nullptr;

  const auto binding = GetParserBinding(ComponentForSummary(summary));
  if (!binding.parser) return nullptr;
  if (binding.assign_global) { binding.assign_global(binding.parser); }

  if (const auto* item = binding.parser->GetResourceItem(table->items, key.c_str())) {
    return item;
  }

  const auto normalized_key = NormalizeDirectiveNameForComparison(key);
  for (int i = 0; table->items[i].name; ++i) {
    const auto& item = table->items[i];
    if (NormalizeDirectiveNameForComparison(item.name) == normalized_key) {
      return &item;
    }
    for (const auto& alias : item.aliases) {
      if (NormalizeDirectiveNameForComparison(alias) == normalized_key) {
        return &item;
      }
    }
  }

  return nullptr;
}

std::string DatatypeString(int type) { return DatatypeToString(type); }

std::string RelatedResourceType(const ResourceSummary& summary,
                                const ResourceItem& item)
{
  if (item.type != CFG_TYPE_RES && item.type != CFG_TYPE_ALIST_RES) return "";

  for (const auto candidate :
       CandidateComponentsForType(summary.type, ComponentForSummary(summary))) {
    const auto binding = GetParserBinding(candidate);
    if (!binding.parser) continue;
    if (binding.assign_global) { binding.assign_global(binding.parser); }
    if (!FindResourceTableByNormalizedType(*binding.parser, summary.type)) continue;

    const auto* resource_type = binding.parser->ResToStr(item.code);
    if (!resource_type) return "";
    return NormalizeParserResourceType(resource_type);
  }
  return "";
}

std::vector<std::string> CollectAllowedValues(const ResourceSummary& summary,
                                              const DirectiveMetadata& metadata)
{
  if (metadata.datatype == "BOOLEAN") {
    return {"yes", "no"};
  }

  if (metadata.related_resource_type.empty()) return {};

  const auto config_root = std::filesystem::path(summary.config_root);
  if (config_root.empty()) return {};

  std::set<std::string> values;
  std::filesystem::path component_dir;
  if (summary.component == "director") {
    component_dir = config_root / "bareos-dir.d";
  } else if (summary.component == "file-daemon") {
    component_dir = config_root / "bareos-fd.d";
  } else if (summary.component == "storage-daemon") {
    component_dir = config_root / "bareos-sd.d";
  } else if (summary.component == "console") {
    component_dir = config_root / "bconsole.d";
  }
  if (component_dir.empty()) return {};

  const auto related_dir = component_dir / metadata.related_resource_type;
  for (const auto& resource : DiscoverResourcesInDirectory(
           related_dir, "allowed-values", summary.component, summary.config_root)) {
    if (resource.type != metadata.related_resource_type) continue;
    const auto normalized_name = StripSurroundingQuotes(resource.name);
    if (normalized_name.empty()) continue;
    values.insert(normalized_name);
  }

  return {values.begin(), values.end()};
}

std::vector<std::string> CollectAllowedValuesForRelatedResourceType(
    const ResourceSummary& summary, const std::string& related_resource_type)
{
  if (related_resource_type.empty()) return {};
  return CollectAllowedValues(summary, DirectiveMetadata{"",
                                                        "",
                                                        "",
                                                        "",
                                                        related_resource_type,
                                                        false,
                                                        false,
                                                        false});
}

std::map<std::string, std::vector<std::string>>
BuildScheduleRunOverrideAllowedValues(const ResourceSummary& summary)
{
  if (summary.type != "schedule") return {};

  return {
      {"Pool", CollectAllowedValuesForRelatedResourceType(summary, "pool")},
      {"FullPool", CollectAllowedValuesForRelatedResourceType(summary, "pool")},
      {"IncrementalPool",
       CollectAllowedValuesForRelatedResourceType(summary, "pool")},
      {"DifferentialPool",
       CollectAllowedValuesForRelatedResourceType(summary, "pool")},
      {"NextPool", CollectAllowedValuesForRelatedResourceType(summary, "pool")},
      {"Storage", CollectAllowedValuesForRelatedResourceType(summary, "storage")},
      {"Messages",
       CollectAllowedValuesForRelatedResourceType(summary, "messages")},
  };
}

std::map<std::string, DirectiveMetadata> BuildDirectiveMetadata(
    const ResourceSummary& summary)
{
  const auto* table = LookupResourceTable(summary);
  if (!table) return {};

  std::map<std::string, DirectiveMetadata> metadata;
  for (int i = 0; table->items[i].name; ++i) {
    const auto& item = table->items[i];
    auto entry = DirectiveMetadata{item.name,
                                   DatatypeString(item.type),
                                   item.description ? item.description : "",
                                   item.default_value ? item.default_value : "",
                                   RelatedResourceType(summary, item),
                                   item.is_required,
                                   IsRepeatableDatatype(item.type),
                                   item.is_deprecated};
    metadata.emplace(entry.key, std::move(entry));
  }

  return metadata;
}

bool AppendFormattedOutput(void* context, const char* fmt, ...)
{
  if (!context || !fmt) return false;

  va_list args;
  va_start(args, fmt);
  char* rendered = nullptr;
  const int length = vasprintf(&rendered, fmt, args);
  va_end(args);
  if (length < 0 || !rendered) return false;

  static_cast<std::string*>(context)->append(rendered,
                                             static_cast<size_t>(length));
  free(rendered);
  return true;
}

std::string RenderResourceContent(ConfigurationParser& parser,
                                  BareosResource& resource)
{
  std::string output;
  OutputFormatter formatter(&AppendFormattedOutput, &output, nullptr, nullptr);
  OutputFormatterResource formatter_resource(&formatter);
  resource.PrintConfig(formatter_resource, parser, false, false);
  formatter.FinalizeResult(true);
  return output;
}

json_t* RenderResourceJson(ConfigurationParser& parser, BareosResource& resource)
{
  std::string output;
  OutputFormatter formatter(&AppendFormattedOutput, &output, nullptr, nullptr,
                            API_MODE_JSON);
  OutputFormatterResource formatter_resource(&formatter);
  resource.PrintConfig(formatter_resource, parser, false, false);
  formatter.FinalizeResult(true);

  json_error_t error;
  json_t* root = json_loads(output.c_str(), 0, &error);
  if (!root) return nullptr;

  json_t* result = json_object_get(root, "result");
  if (!result) {
    json_decref(root);
    return nullptr;
  }

  json_incref(result);
  json_decref(root);
  return result;
}

bool PathIsWithin(const std::filesystem::path& path,
                  const std::filesystem::path& root)
{
  if (root.empty()) return false;

  const auto normalized_path = path.lexically_normal();
  const auto normalized_root = root.lexically_normal();
  if (normalized_path == normalized_root) return true;

  const auto relative = normalized_path.lexically_relative(normalized_root);
  return !relative.empty() && relative != normalized_path
         && *relative.begin() != "..";
}

std::vector<ResourceSummary> CollectResourceSummariesFromParser(
    ConfigurationParser& parser,
    const std::string& id_prefix,
    const std::string& component,
    const std::string& config_root,
    const std::filesystem::path& source_root_filter)
{
  std::vector<ResourceSummary> resources;
  size_t resource_index = 0;

  const auto* tables = parser.GetResourceTables();
  if (!tables) return resources;

  for (const auto* table = tables; table->name; ++table) {
    for (auto* resource = parser.GetNextRes(table->rcode, nullptr); resource;
         resource = parser.GetNextRes(table->rcode, resource)) {
      if (resource->internal_) continue;

      const std::filesystem::path source_file = resource->source_file_;
      if (!source_root_filter.empty() && !PathIsWithin(source_file, source_root_filter)) {
        continue;
      }

      resources.emplace_back(
          id_prefix + "-" + std::to_string(resource_index++),
          NormalizeParserResourceType(parser.ResToStr(table->rcode)),
          resource->resource_name_ ? resource->resource_name_ : "",
          source_file.string(), component, config_root, resource->source_line_start_,
          resource->source_line_end_);
    }
  }

  std::sort(resources.begin(), resources.end(),
            [](const ResourceSummary& lhs, const ResourceSummary& rhs) {
              if (lhs.file_path != rhs.file_path) return lhs.file_path < rhs.file_path;
              if (lhs.source_line_start != rhs.source_line_start) {
                return lhs.source_line_start < rhs.source_line_start;
              }
              if (lhs.type != rhs.type) return lhs.type < rhs.type;
              return lhs.name < rhs.name;
            });
  return resources;
}

BareosResource* FindParsedResource(ConfigurationParser& parser,
                                   const ResourceSummary& summary)
{
  const auto* tables = parser.GetResourceTables();
  if (!tables) return nullptr;

  for (const auto* table = tables; table->name; ++table) {
    for (auto* resource = parser.GetNextRes(table->rcode, nullptr); resource;
         resource = parser.GetNextRes(table->rcode, resource)) {
      if (resource->internal_) continue;
      if (NormalizeParserResourceType(parser.ResToStr(table->rcode)) != summary.type) {
        continue;
      }
      if ((resource->resource_name_ ? resource->resource_name_ : "") != summary.name) {
        continue;
      }
      if (!summary.file_path.empty() && resource->source_file_ != summary.file_path) {
        continue;
      }
      if (summary.source_line_start != 0
          && resource->source_line_start_ != summary.source_line_start) {
        continue;
      }
      return resource;
    }
  }

  return nullptr;
}

std::string ReadFileLineRange(const std::filesystem::path& path,
                              size_t start_line,
                              size_t end_line)
{
  std::ifstream input(path);
  if (!input) return {};

  std::ostringstream output;
  std::string line;
  size_t line_number = 0;
  while (std::getline(input, line)) {
    ++line_number;
    if (line_number < start_line) continue;
    if (end_line != 0 && line_number > end_line) break;
    output << line << "\n";
  }
  return output.str();
}

std::string JsonScalarToDirectiveValue(const json_t* value)
{
  if (json_is_string(value)) return json_string_value(value);
  if (json_is_boolean(value)) return json_is_true(value) ? "yes" : "no";
  if (json_is_integer(value)) return std::to_string(json_integer_value(value));
  if (json_is_real(value)) return std::to_string(json_real_value(value));
  return "";
}

std::string CanonicalDirectiveKey(const ResourceItem* items, const std::string& key)
{
  if (!items) return key;
  const auto normalized_key = NormalizeDirectiveNameForComparison(key);
  for (int index = 0; items[index].name; ++index) {
    const auto& item = items[index];
    if (NormalizeDirectiveNameForComparison(item.name) == normalized_key) {
      return item.name;
    }
    for (const auto& alias : item.aliases) {
      if (NormalizeDirectiveNameForComparison(alias) == normalized_key) {
        return item.name;
      }
    }
  }
  return key;
}

size_t CountChar(const std::string& text, char needle)
{
  return static_cast<size_t>(std::count(text.begin(), text.end(), needle));
}

std::vector<ResourceDirective> ParseDirectivesFromContent(
    const std::string& content)
{
  std::vector<ResourceDirective> directives;
  std::istringstream input(content);
  std::string line;
  size_t line_number = 0;
  size_t brace_depth = 0;

  while (std::getline(input, line)) {
    ++line_number;
    const auto comment_start = line.find('#');
    if (comment_start != std::string::npos) {
      line = line.substr(0, comment_start);
    }

    auto trimmed = TrimWhitespace(line);
    const auto open_braces = CountChar(line, '{');
    const auto close_braces = CountChar(line, '}');
    const auto current_depth = brace_depth;

    if (!trimmed.empty() && trimmed != "{" && trimmed != "}"
        && !trimmed.ends_with('{')) {
      const auto separator = trimmed.find('=');
      if (separator != std::string::npos) {
        auto key = TrimWhitespace(trimmed.substr(0, separator));
        auto value = TrimWhitespace(trimmed.substr(separator + 1));
        if (!key.empty()) {
          directives.push_back(
              {std::move(key), std::move(value), line_number, current_depth});
        }
      }
    }

    brace_depth += open_braces;
    brace_depth -= std::min(brace_depth, close_braces);
  }

  return directives;
}

size_t AppendJsonValueDirectives(const ResourceItem* items,
                                 const std::string& key,
                                 json_t* value,
                                 size_t nesting_level,
                                 size_t line,
                                 std::vector<PrintedDirective>& directives)
{
  const auto canonical_key = CanonicalDirectiveKey(items, key);
  if (json_is_object(value)) {
    ++line;
    const char* child_key = nullptr;
    json_t* child_value = nullptr;
    json_object_foreach(value, child_key, child_value)
    {
      line = AppendJsonValueDirectives(items, child_key, child_value,
                                       nesting_level + 1, line, directives);
    }
    return line + 1;
  }

  if (json_is_array(value)) {
    const size_t array_size = json_array_size(value);
    bool contains_object = false;
    for (size_t index = 0; index < array_size; ++index) {
      json_t* item = json_array_get(value, index);
      if (json_is_object(item) || json_is_array(item)) {
        contains_object = true;
        break;
      }
    }

    if (!contains_object) {
      for (size_t index = 0; index < array_size; ++index) {
        directives.push_back(
            {{canonical_key,
              JsonScalarToDirectiveValue(json_array_get(value, index)), line,
              nesting_level},
             false});
        ++line;
      }
      return line;
    }

    ++line;
    for (size_t index = 0; index < array_size; ++index) {
      line = AppendJsonValueDirectives(items, canonical_key,
                                       json_array_get(value, index),
                                       nesting_level + 1, line, directives);
    }
    return line + 1;
  }

  directives.push_back(
      {{canonical_key, JsonScalarToDirectiveValue(value), line, nesting_level},
       false});
  return line + 1;
}

std::vector<PrintedDirective> ExtractPrintedDirectives(ConfigurationParser& parser,
                                                       BareosResource& resource)
{
  std::vector<PrintedDirective> directives;
  json_t* result = RenderResourceJson(parser, resource);
  if (!result) return directives;

  const auto group_name = Lowercase(parser.ResGroupToStr(resource.rcode_));
  json_t* group = json_object_get(result, group_name.c_str());
  json_t* resource_object
      = group ? json_object_get(group, resource.resource_name_) : nullptr;
  if (!resource_object || !json_is_object(resource_object)) {
    json_decref(result);
    return directives;
  }

  const auto* table = parser.GetResourceTable(parser.ResToStr(resource.rcode_));
  const auto* items = table ? table->items : nullptr;
  size_t line = 2;
  const char* key = nullptr;
  json_t* value = nullptr;
  json_object_foreach(resource_object, key, value)
  {
    line = AppendJsonValueDirectives(items, key, value, 1, line, directives);
  }

  json_decref(result);
  return directives;
}

ParsedResourceDump DumpParsedResource(ConfigurationParser& parser,
                                      const ResourceSummary& summary)
{
  ParsedResourceDump dump;
  auto* resource = FindParsedResource(parser, summary);
  if (!resource) return dump;

  if (summary.source_line_start != 0 && summary.source_line_end != 0) {
    dump.content = ReadFileLineRange(summary.file_path, summary.source_line_start,
                                     summary.source_line_end);
  } else if (!summary.file_path.empty()
             && std::filesystem::exists(summary.file_path)) {
    std::ifstream input(summary.file_path);
    std::ostringstream content;
    content << input.rdbuf();
    dump.content = content.str();
  } else {
    dump.content = RenderResourceContent(parser, *resource);
  }
  dump.directives = ExtractPrintedDirectives(parser, *resource);
  return dump;
}

ParsedResourceDump DumpResourceFromParser(const ResourceSummary& summary)
{
  const auto preferred_component = ComponentForSummary(summary);

  const auto parse_with = [&](ConfigComponent component,
                              const std::filesystem::path& config_file)
      -> ParsedResourceDump {
    auto parser = CreateParserForComponent(component, nullptr);
    if (!parser) return {};

    const auto binding = GetParserBinding(component);
    if (binding.assign_global) { binding.assign_global(parser.get()); }
    if (!ParseStandaloneConfigFile(*parser, config_file, true)) return {};
    return DumpParsedResource(*parser, summary);
  };

  const auto file_path = std::filesystem::path(summary.file_path);
  if (!file_path.empty() && std::filesystem::exists(file_path)) {
    for (const auto candidate :
         CandidateComponentsForType(summary.type, preferred_component)) {
      auto dump = parse_with(candidate, file_path);
      if (!dump.content.empty() || !dump.directives.empty()) return dump;
    }
  }

  const auto config_root = std::filesystem::path(summary.config_root);
  for (const auto candidate :
       CandidateComponentsForType(summary.type, preferred_component)) {
    if (!config_root.empty() && std::filesystem::exists(config_root)) {
      auto parser = CreateParserForComponent(candidate, nullptr);
      if (!parser) continue;

      const auto binding = GetParserBinding(candidate);
      if (binding.assign_global) { binding.assign_global(parser.get()); }
      if (!ParseConfigIncludeTree(*parser, config_root, candidate, true)) continue;
      auto dump = DumpParsedResource(*parser, summary);
      if (!dump.content.empty() || !dump.directives.empty()) return dump;
    }
  }

  return {};
}

std::string MakeDirectorId(size_t director_index)
{
  return "director-" + std::to_string(director_index);
}

std::string MakeDaemonId(size_t director_index, const std::string& daemon_kind)
{
  return "daemon-" + std::to_string(director_index) + "-" + daemon_kind;
}

std::string MakeRelationshipId(size_t director_index, size_t relationship_index)
{
  return "relationship-" + std::to_string(director_index) + "-"
         + std::to_string(relationship_index);
}

std::string SanitizeNodeIdComponent(std::string value)
{
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
    if (std::isalnum(c)) return static_cast<char>(std::tolower(c));
    return '-';
  });
  value.erase(std::unique(value.begin(), value.end(),
                          [](char left, char right) {
                            return left == '-' && right == '-';
                          }),
              value.end());
  while (!value.empty() && value.front() == '-') value.erase(value.begin());
  while (!value.empty() && value.back() == '-') value.pop_back();
  return value.empty() ? "unnamed" : value;
}

std::string MakeKnownClientNodeId(size_t director_index, const std::string& name)
{
  return "known-client-" + std::to_string(director_index) + "-"
         + SanitizeNodeIdComponent(StripSurroundingQuotes(name));
}

const DaemonSummary* FindDirectorDaemonByKind(const DirectorSummary& director,
                                              const std::string& kind)
{
  const auto it = std::find_if(
      director.daemons.begin(), director.daemons.end(),
      [&kind](const DaemonSummary& daemon) {
        return daemon.kind == kind && !daemon.resources.empty();
      });
  return it == director.daemons.end() ? nullptr : &(*it);
}

void AppendJsonDirectives(std::ostringstream& output,
                          const std::vector<ResourceDirective>& directives);
void AppendJsonInheritedDirectives(
    std::ostringstream& output,
    const std::vector<InheritedDirective>& inherited_directives);
void AppendJsonValidationMessages(
    std::ostringstream& output,
    const std::vector<ValidationMessage>& validation_messages);
void AppendJsonFieldHints(std::ostringstream& output,
                          const std::vector<ResourceFieldHint>& field_hints);

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

std::string JsonBool(bool value) { return value ? "true" : "false"; }

std::string StemWithoutExtension(const std::filesystem::path& path)
{
  auto stem = path.stem().string();
  if (stem == ".conf") return path.filename().string();
  return stem;
}

size_t CountLines(const std::string& content)
{
  if (content.empty()) return 0;

  size_t lines = 1;
  for (char c : content) {
    if (c == '\n') ++lines;
  }
  if (content.back() == '\n') --lines;
  return lines;
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

std::string ResourceBlockNameForTypeImpl(const std::string& type)
{
  if (type == "autochanger") return "Autochanger";
  if (type == "catalog") return "Catalog";
  if (type == "client") return "Client";
  if (type == "console") return "Console";
  if (type == "counter") return "Counter";
  if (type == "device") return "Device";
  if (type == "director") return "Director";
  if (type == "fileset") return "FileSet";
  if (type == "job") return "Job";
  if (type == "jobdefs") return "JobDefs";
  if (type == "messages") return "Messages";
  if (type == "ndmp") return "Ndmp";
  if (type == "pool") return "Pool";
  if (type == "profile") return "Profile";
  if (type == "schedule") return "Schedule";
  if (type == "storage") return "Storage";
  if (type == "user") return "User";
  return "";
}

std::vector<InheritedDirective> ResolveInheritedDirectives(
    const ResourceSummary& summary,
    const std::vector<ResourceDirective>& directives)
{
  if (summary.type != "job") return {};

  std::string inherited_jobdefs_name;
  for (const auto& directive : directives) {
    if (!IsTopLevelDirective(directive)) continue;
    if (NormalizeDirectiveNameForComparison(directive.key) != "jobdefs") continue;
    inherited_jobdefs_name = StripSurroundingQuotes(directive.value);
    break;
  }
  if (inherited_jobdefs_name.empty()) return {};

  std::set<std::string> direct_keys;
  for (const auto& directive : directives) {
    if (!IsTopLevelDirective(directive)) continue;
    direct_keys.insert(NormalizeDirectiveNameForComparison(directive.key));
  }

  const auto config_root = std::filesystem::path(summary.config_root);
  const auto jobdefs_dir = config_root / "bareos-dir.d" / "jobdefs";
  if (jobdefs_dir.empty() || !std::filesystem::exists(jobdefs_dir)) return {};

  const auto normalized_jobdefs_name
      = NormalizeDirectiveNameForComparison(inherited_jobdefs_name);
  const auto jobdefs_resources = DiscoverResourcesInDirectory(
      jobdefs_dir, "inherited-jobdefs", "director", summary.config_root);
  for (const auto& resource : jobdefs_resources) {
    if (resource.type != "jobdefs") continue;
    if (NormalizeDirectiveNameForComparison(StripSurroundingQuotes(resource.name))
        != normalized_jobdefs_name) {
      continue;
    }

    const auto detail = LoadResourceDetail(resource);
    std::vector<InheritedDirective> inherited_directives;
    for (const auto& inherited_directive : detail.directives) {
      if (!IsTopLevelDirective(inherited_directive)) continue;
      const auto normalized_key
          = NormalizeDirectiveNameForComparison(inherited_directive.key);
      if (normalized_key == "name" || normalized_key == "jobdefs"
          || direct_keys.contains(normalized_key)) {
        continue;
      }

      inherited_directives.push_back(InheritedDirective{
          inherited_directive,
          "jobdefs",
          StripSurroundingQuotes(resource.name),
          resource.file_path,
      });
    }
    return inherited_directives;
  }

  return {};
}

std::vector<ValidationMessage> ValidateResourceContent(
    const ResourceSummary& summary,
    const std::string& content,
    const std::vector<ResourceDirective>& directives,
    const std::vector<InheritedDirective>& inherited_directives)
{
  std::vector<ValidationMessage> messages;
  const auto* table = LookupResourceTable(summary);
  const auto directive_metadata = BuildDirectiveMetadata(summary);

  if (directives.empty()) {
    messages.push_back(
        {"warning", "no-directives",
         "No directive fields were parsed from this resource.", 0});
  }

  std::map<std::string, size_t> first_occurrence;
  std::set<std::string> present_keys;
  for (const auto& directive : directives) {
    const auto top_level = IsTopLevelDirective(directive);
    auto canonical_key = directive.key;
    if (const auto* item = LookupDirectiveResourceItem(summary, table, directive.key)) {
      canonical_key = item->name;
    }
    if (top_level) {
      present_keys.insert(NormalizeDirectiveNameForComparison(canonical_key));
    }

    if (directive.value.empty()) {
      messages.push_back({"warning", "empty-value",
                          "Directive has an empty value.", directive.line});
    }

    if (!top_level) continue;

    const auto insert_result
        = first_occurrence.emplace(canonical_key, directive.line);
    const auto metadata_it = directive_metadata.find(canonical_key);
    const bool repeatable = metadata_it != directive_metadata.end()
                            && metadata_it->second.repeatable;
    if (!insert_result.second && !repeatable) {
      messages.push_back({"warning", "duplicate-directive",
                          "Directive appears multiple times in this resource.",
                          directive.line});
    }
  }

  for (const auto& inherited_directive : inherited_directives) {
    if (!IsTopLevelDirective(inherited_directive.directive)) continue;
    present_keys.insert(NormalizeDirectiveNameForComparison(
        inherited_directive.directive.key));
  }

  for (const auto& [key, metadata] : directive_metadata) {
    if (!metadata.required
        || present_keys.contains(NormalizeDirectiveNameForComparison(key))) {
      continue;
    }

    if (key == "Name") {
      messages.push_back({"error", "missing-name",
                          "Resource does not define a Name directive.", 0});
      continue;
    }
    if (key == "Address" && summary.type == "client") {
      messages.push_back({"warning", "missing-address",
                          "Client resource does not define an Address directive.",
                          0});
      continue;
    }
    messages.push_back({"error", "missing-required-directive",
                        key + " is required for this resource type.", 0});
  }

  std::istringstream input(content);
  std::string line;
  size_t line_number = 0;
  while (std::getline(input, line)) {
    ++line_number;
    const auto comment_start = line.find('#');
    if (comment_start != std::string::npos) {
      line = line.substr(0, comment_start);
    }

    const auto trimmed = TrimWhitespace(line);
    if (trimmed.empty() || trimmed == "{" || trimmed == "}") continue;
    if (trimmed.back() == '{') continue;
    if (trimmed.find('=') == std::string::npos) {
      messages.push_back({"warning", "unparsed-line",
                          "Non-empty line could not be parsed as a directive.",
                          line_number});
    }
  }

  return messages;
}

std::vector<ResourceFieldHint> BuildResourceFieldHints(
    const ResourceSummary& summary,
    const std::vector<ResourceDirective>& directives,
    const std::vector<InheritedDirective>& inherited_directives)
{
  const auto* table = LookupResourceTable(summary);
  const auto directive_metadata = BuildDirectiveMetadata(summary);

  std::map<std::string, std::vector<std::string>> values_by_key;
  std::map<std::string, InheritedDirective> inherited_by_key;
  std::map<std::string, size_t> counts;
  for (const auto& directive : directives) {
    if (!IsTopLevelDirective(directive)) continue;
    auto canonical_key = directive.key;
    if (const auto* item = LookupDirectiveResourceItem(summary, table, directive.key)) {
      canonical_key = item->name;
    }
    ++counts[canonical_key];
    values_by_key[canonical_key].push_back(directive.value);
  }
  for (const auto& inherited_directive : inherited_directives) {
    if (!IsTopLevelDirective(inherited_directive.directive)) continue;
    const auto normalized_key = NormalizeDirectiveNameForComparison(
        inherited_directive.directive.key);
    if (!inherited_by_key.contains(normalized_key)) {
      inherited_by_key.emplace(normalized_key, inherited_directive);
    }
  }

  std::vector<ResourceFieldHint> hints;
  std::map<std::string, bool> known_keys;
  for (const auto& [key, metadata] : directive_metadata) {
    const auto value_it = values_by_key.find(key);
    const bool present = value_it != values_by_key.end();
    const auto inherited_it
        = inherited_by_key.find(NormalizeDirectiveNameForComparison(key));
    const bool inherited = !present && inherited_it != inherited_by_key.end();
    const auto value = !present
                           ? std::string{}
                           : metadata.repeatable
                                 ? JoinRepeatableFieldValues(value_it->second)
                                 : value_it->second.front();
    const auto inherited_value = !inherited
                                     ? std::string{}
                                     : inherited_it->second.directive.value;
    auto hint = ResourceFieldHint{key,
                                  key,
                                  metadata.required,
                                  metadata.repeatable || counts[key] > 1,
                                  metadata.deprecated,
                                  present,
                                  inherited,
                                  metadata.datatype == "BOOLEAN"
                                      ? NormalizeBooleanValue(value)
                                      : value,
                                  metadata.datatype == "BOOLEAN"
                                      ? NormalizeBooleanValue(inherited_value)
                                      : inherited_value,
                                  metadata.datatype,
                                  metadata.description,
                                  metadata.datatype == "BOOLEAN"
                                      ? NormalizeBooleanValue(metadata.default_value)
                                      : metadata.default_value,
                                  inherited
                                      ? inherited_it->second.source_resource_type
                                      : "",
                                  inherited
                                      ? inherited_it->second.source_resource_name
                                      : "",
                                  metadata.related_resource_type,
                                  CollectAllowedValues(summary, metadata)};
    std::vector<std::string> normalized_hint_values;
    if (metadata.related_resource_type.empty()) {
      normalized_hint_values.push_back(hint.value);
    } else if (hint.repeatable) {
      normalized_hint_values = SplitRepeatableFieldValue(hint.value);
      for (auto& normalized_value : normalized_hint_values) {
        normalized_value = StripSurroundingQuotes(std::move(normalized_value));
      }
    } else {
      normalized_hint_values.push_back(StripSurroundingQuotes(hint.value));
    }
    if (present && !hint.allowed_values.empty()) {
      for (const auto& normalized_hint_value : normalized_hint_values) {
        if (normalized_hint_value.empty()) continue;
        if (std::find(hint.allowed_values.begin(), hint.allowed_values.end(),
                      normalized_hint_value)
            != hint.allowed_values.end()) {
          continue;
        }
        hint.allowed_values.insert(hint.allowed_values.begin(),
                                   normalized_hint_value);
      }
    }
    hints.push_back(std::move(hint));
    known_keys[key] = true;
  }

  for (const auto& directive : directives) {
    if (!IsTopLevelDirective(directive)) continue;
    auto canonical_key = directive.key;
    if (const auto* item = LookupDirectiveResourceItem(summary, table, directive.key)) {
      canonical_key = item->name;
    }
    if (known_keys[canonical_key]) continue;
    hints.push_back({canonical_key, canonical_key, false,
                     counts[canonical_key] > 1, false, true, false, directive.value,
                     "", "", "", "", "", "", "", {}});
    known_keys[canonical_key] = true;
  }

  SortResourceFieldHints(hints);
  return hints;
}

std::vector<std::string> SplitLinesPreserveTrailing(const std::string& content)
{
  std::vector<std::string> lines;
  std::string current;
  for (char c : content) {
    if (c == '\n') {
      lines.push_back(current);
      current.clear();
      continue;
    }
    current += c;
  }
  lines.push_back(current);
  return lines;
}

std::string JoinLines(const std::vector<std::string>& lines)
{
  std::ostringstream output;
  for (size_t i = 0; i < lines.size(); ++i) {
    if (i > 0) output << '\n';
    output << lines[i];
  }
  return output.str();
}

std::string ApplyFieldHintsToContent(
    const ResourceDetail& resource,
    const std::vector<ResourceFieldHint>& updated_field_hints)
{
  auto lines = SplitLinesPreserveTrailing(resource.content);
  const auto* table = LookupResourceTable(resource.summary);
  std::map<std::string, std::vector<size_t>> directive_indexes_by_key;
  std::map<std::string, std::string> rendered_key_by_canonical_key;
  for (size_t i = 0; i < lines.size(); ++i) {
    auto trimmed = TrimWhitespace(lines[i]);
    const auto separator = trimmed.find('=');
    if (trimmed.empty() || separator == std::string::npos) continue;
    auto rendered_key = TrimWhitespace(trimmed.substr(0, separator));
    auto key = rendered_key;
    if (const auto* item = LookupDirectiveResourceItem(resource.summary, table, key)) {
      key = item->name;
    }
    directive_indexes_by_key[key].push_back(i);
    rendered_key_by_canonical_key[key] = std::move(rendered_key);
  }

  size_t closing_index = lines.size();
  for (size_t i = lines.size(); i > 0; --i) {
    if (TrimWhitespace(lines[i - 1]) == "}") {
      closing_index = i - 1;
      break;
    }
  }

  auto adjust_indices_after_removal = [&](size_t removed_index) {
    for (auto& [key, indexes] : directive_indexes_by_key) {
      for (auto& index : indexes) {
        if (index > removed_index) --index;
      }
    }
  };
  auto adjust_indices_after_insert = [&](size_t inserted_index) {
    for (auto& [key, indexes] : directive_indexes_by_key) {
      for (auto& index : indexes) {
        if (index >= inserted_index) ++index;
      }
    }
  };
  auto render_line = [](const std::string& key, const std::string& value) {
    return "  " + key + " = " + value;
  };

  for (const auto& field : updated_field_hints) {
    const auto desired_value = TrimWhitespace(field.value);
    const auto directive_it = directive_indexes_by_key.find(field.key);
    const auto rendered_key_it = rendered_key_by_canonical_key.find(field.key);
    const auto& rendered_key
        = rendered_key_it != rendered_key_by_canonical_key.end()
              ? rendered_key_it->second
              : field.key;
    if (field.repeatable) {
      const auto desired_values = SplitRepeatableFieldValue(field.value);
      if (directive_it != directive_indexes_by_key.end()) {
        auto existing_indexes = directive_it->second;
        std::sort(existing_indexes.rbegin(), existing_indexes.rend());
        for (const auto line_index : existing_indexes) {
          lines.erase(lines.begin() + static_cast<std::ptrdiff_t>(line_index));
          adjust_indices_after_removal(line_index);
          if (closing_index > line_index) --closing_index;
        }
        directive_indexes_by_key.erase(directive_it);
      }
      if (desired_values.empty()) {
        rendered_key_by_canonical_key.erase(field.key);
        continue;
      }

      const auto insert_index = std::min(closing_index, lines.size());
      for (size_t value_index = 0; value_index < desired_values.size();
           ++value_index) {
        const auto current_index = insert_index + value_index;
        adjust_indices_after_insert(current_index);
        lines.insert(lines.begin() + static_cast<std::ptrdiff_t>(current_index),
                     render_line(rendered_key, desired_values[value_index]));
        directive_indexes_by_key[field.key].push_back(current_index);
        if (closing_index >= current_index) ++closing_index;
      }
      rendered_key_by_canonical_key[field.key] = rendered_key;
      continue;
    }
    if (directive_it != directive_indexes_by_key.end()) {
      const auto line_index = directive_it->second.front();
      const auto replacement = render_line(rendered_key, desired_value);
      if (!desired_value.empty() || field.required) {
        lines[line_index] = replacement;
      } else {
        lines.erase(lines.begin() + static_cast<std::ptrdiff_t>(line_index));
        directive_indexes_by_key.erase(directive_it);
        rendered_key_by_canonical_key.erase(field.key);
        adjust_indices_after_removal(line_index);
        if (closing_index > line_index) --closing_index;
      }
      continue;
    }

    if (desired_value.empty() && !field.required) continue;
    const auto replacement = render_line(field.key, desired_value);
    const auto insert_index = std::min(closing_index, lines.size());
    adjust_indices_after_insert(insert_index);
    lines.insert(lines.begin() + static_cast<std::ptrdiff_t>(insert_index),
                 replacement);
    directive_indexes_by_key[field.key] = {insert_index};
    rendered_key_by_canonical_key[field.key] = field.key;
    if (closing_index >= insert_index) ++closing_index;
  }

  return JoinLines(lines);
}

std::vector<ResourceSummary> DiscoverResourcesInDirectory(
    const std::filesystem::path& path,
    const std::string& id_prefix,
    const std::string& component,
    const std::string& config_root)
{
  if (!std::filesystem::exists(path)) return {};

  const auto component_type = ComponentFromString(component);
  const auto root_path = std::filesystem::path(config_root);
  std::vector<ResourceSummary> root_resources;
  if (component_type != ConfigComponent::kUnknown && !root_path.empty()
      && std::filesystem::exists(root_path)) {
    auto parser = CreateParserForComponent(component_type, nullptr);
    if (parser) {
      const auto binding = GetParserBinding(component_type);
      if (binding.assign_global) { binding.assign_global(parser.get()); }
      if (ParseConfigIncludeTree(*parser, root_path, component_type, true)) {
        root_resources = CollectResourceSummariesFromParser(
            *parser, id_prefix, component, config_root, path);
      }
    }
  }

  auto same_resource = [](const ResourceSummary& lhs, const ResourceSummary& rhs) {
    return lhs.file_path == rhs.file_path
           && lhs.source_line_start == rhs.source_line_start
           && lhs.source_line_end == rhs.source_line_end && lhs.type == rhs.type
           && lhs.name == rhs.name;
  };
  auto append_unique = [&](std::vector<ResourceSummary>& destination,
                           ResourceSummary resource) {
    const auto duplicate = std::find_if(
        destination.begin(), destination.end(),
        [&](const ResourceSummary& existing) {
          return same_resource(existing, resource);
        });
    if (duplicate == destination.end()) {
      destination.push_back(std::move(resource));
    }
  };

  std::vector<ResourceSummary> parsed_resources;
  for (const auto& entry : std::filesystem::recursive_directory_iterator(path)) {
    if (!entry.is_regular_file() || entry.path().extension() != ".conf") continue;
    const auto hinted_type = entry.path().parent_path().filename().string();
    const auto parsed_count_before = parsed_resources.size();
    for (const auto candidate :
         CandidateComponentsForType(hinted_type, component_type)) {
      auto parser = CreateParserForComponent(candidate, nullptr);
      if (!parser) continue;
      const auto binding = GetParserBinding(candidate);
      if (binding.assign_global) { binding.assign_global(parser.get()); }
      if (!ParseStandaloneConfigFile(*parser, entry.path(), true)) continue;

      auto file_resources = CollectResourceSummariesFromParser(
          *parser, id_prefix, component, config_root, entry.path());
      if (file_resources.empty()) continue;
      for (auto& summary : file_resources) {
        append_unique(parsed_resources, std::move(summary));
      }
      break;
    }

    if (parsed_resources.size() == parsed_count_before
        && !ResourceBlockNameForType(hinted_type).empty()) {
      std::ifstream input(entry.path());
      std::ostringstream content;
      content << input.rdbuf();
      auto discovered = DiscoverResourceSummariesFromContent(
          {id_prefix + "-" + std::to_string(parsed_resources.size()),
           hinted_type,
           "",
           entry.path().string(),
           component,
           config_root},
          content.str());
      for (auto& summary : discovered) {
        append_unique(parsed_resources, std::move(summary));
      }
    }
  }

  for (auto& summary : root_resources) {
    append_unique(parsed_resources, std::move(summary));
  }

  for (size_t index = 0; index < parsed_resources.size(); ++index) {
    parsed_resources[index].id = id_prefix + "-" + std::to_string(index);
  }
  return parsed_resources;
}

bool StageEditedContentInConfigRoot(const ResourceSummary& resource,
                                    const std::string& content,
                                    const std::filesystem::path& staged_root,
                                    std::filesystem::path& staged_file)
{
  const auto config_root = std::filesystem::path(resource.config_root);
  const auto resource_path = std::filesystem::path(resource.file_path);
  if (config_root.empty() || resource_path.empty()
      || !std::filesystem::exists(config_root)) {
    return false;
  }

  const auto relative_path = resource_path.lexically_relative(config_root);
  if (relative_path.empty() || relative_path == resource_path
      || *relative_path.begin() == "..") {
    return false;
  }

  std::error_code ec;
  for (const auto& entry : std::filesystem::recursive_directory_iterator(config_root)) {
    const auto entry_relative = entry.path().lexically_relative(config_root);
    const auto staged_path = staged_root / entry_relative;
    if (entry.is_directory()) {
      std::filesystem::create_directories(staged_path, ec);
      if (ec) return false;
      continue;
    }
    if (!entry.is_regular_file()) continue;

    std::filesystem::create_directories(staged_path.parent_path(), ec);
    if (ec) return false;
    std::filesystem::copy_file(entry.path(), staged_path,
                               std::filesystem::copy_options::overwrite_existing,
                               ec);
    if (ec) return false;
  }

  staged_file = staged_root / relative_path;
  std::filesystem::create_directories(staged_file.parent_path(), ec);
  if (ec) return false;

  std::ofstream output(staged_file, std::ios::trunc);
  if (!output) return false;
  output << content;
  return static_cast<bool>(output);
}

std::string DirectorNameForRoot(const std::filesystem::path& root)
{
  const auto explicit_conf = root / "bareos-dir.conf";
  if (std::filesystem::exists(explicit_conf)) {
    return StemWithoutExtension(explicit_conf);
  }

  const auto explicit_conf_dir = root / "bareos-dir.d";
  if (std::filesystem::exists(explicit_conf_dir)) {
    return "bareos-dir";
  }

  const auto normalized_root = root.lexically_normal();
  auto filename = normalized_root.filename().string();
  if (!filename.empty()) return filename;

  filename = normalized_root.parent_path().filename().string();
  if (!filename.empty()) return filename;

  return normalized_root.string();
}

std::string ConfiguredNameForDaemon(
    const std::string& kind,
    const std::filesystem::path& daemon_conf,
    const std::vector<ResourceSummary>& resources)
{
  std::string resource_type;
  if (kind == "file-daemon") {
    resource_type = "client";
  } else if (kind == "storage-daemon") {
    resource_type = "storage";
  } else if (kind == "console") {
    resource_type = "console";
  } else {
    return "";
  }

  if (std::filesystem::exists(daemon_conf)) {
    auto parser = CreateParserForComponent(ComponentFromString(kind), nullptr);
    if (parser) {
      const auto binding = GetParserBinding(ComponentFromString(kind));
      if (binding.assign_global) { binding.assign_global(parser.get()); }
      if (ParseStandaloneConfigFile(*parser, daemon_conf, false)
          || ParseStandaloneConfigFile(*parser, daemon_conf, true)) {
        const auto main_resources = CollectResourceSummariesFromParser(
            *parser, "daemon-name-main", kind, daemon_conf.parent_path().string(),
            daemon_conf);
        const auto it = std::find_if(main_resources.begin(), main_resources.end(),
                                     [&](const ResourceSummary& resource) {
                                       return resource.type == resource_type;
                                     });
        if (it != main_resources.end()) {
          const auto configured_name
              = ExtractDirectiveValue(LoadResourceDetail(*it).directives, "Name");
          if (!configured_name.empty()) return configured_name;
        }
      }
    }
  }

  for (const auto& resource : resources) {
    if (resource.type != resource_type) continue;
    const auto configured_name
        = ExtractDirectiveValue(LoadResourceDetail(resource).directives, "Name");
    if (!configured_name.empty()) return configured_name;
  }

  return "";
}

DaemonSummary DiscoverDaemon(const std::filesystem::path& root,
                             size_t director_index,
                             const std::string& kind,
                             const std::string& conf_dir_name,
                             const std::string& daemon_name)
{
  const auto conf_dir = root / conf_dir_name;
  auto resources = DiscoverResourcesInDirectory(
      conf_dir,
      "resource-" + std::to_string(director_index) + "-" + kind,
      kind,
      root.string());
  const auto daemon_conf = root / (daemon_name + ".conf");
  const auto configured_name
      = ConfiguredNameForDaemon(kind, daemon_conf, resources);
  if (std::filesystem::exists(daemon_conf)) {
    resources.insert(resources.begin(),
                     {"resource-" + std::to_string(director_index) + "-" + kind
                           + "-main",
                        "configuration", daemon_conf.filename().string(),
                        daemon_conf.string(), kind, root.string()});
  }

  return {
      MakeDaemonId(director_index, kind),
      kind,
      daemon_name,
      configured_name,
      conf_dir.string(),
      std::move(resources),
  };
}

void AppendJsonResourceArray(std::ostringstream& output,
                             const std::vector<ResourceSummary>& resources)
{
  output << "[";
  for (size_t i = 0; i < resources.size(); ++i) {
    if (i > 0) output << ",";
    const auto& resource = resources[i];
    output << "{"
           << "\"id\":" << JsonString(resource.id) << ","
           << "\"type\":" << JsonString(resource.type) << ","
           << "\"name\":" << JsonString(resource.name) << ","
           << "\"file_path\":" << JsonString(resource.file_path) << "}";
  }
  output << "]";
}

void AppendJsonRelationshipArray(
    std::ostringstream& output,
    const std::vector<RelationshipSummary>& relationships)
{
  output << "[";
  for (size_t i = 0; i < relationships.size(); ++i) {
    if (i > 0) output << ",";
    const auto& relationship = relationships[i];
    output << "{"
           << "\"id\":" << JsonString(relationship.id) << ","
           << "\"relation\":" << JsonString(relationship.relation) << ","
           << "\"endpoint_name\":"
           << JsonString(relationship.endpoint_name) << ","
           << "\"from_node_id\":"
           << JsonString(relationship.from_node_id) << ","
           << "\"from_label\":" << JsonString(relationship.from_label) << ","
           << "\"to_node_id\":" << JsonString(relationship.to_node_id) << ","
           << "\"to_label\":" << JsonString(relationship.to_label) << ","
           << "\"source_resource_id\":"
           << JsonString(relationship.source_resource_id) << ","
           << "\"source_resource_path\":"
           << JsonString(relationship.source_resource_path) << ","
           << "\"source_resource_type\":"
           << JsonString(relationship.source_resource_type) << ","
           << "\"source_resource_name\":"
           << JsonString(relationship.source_resource_name) << ","
           << "\"target_resource_id\":"
           << JsonString(relationship.target_resource_id) << ","
           << "\"target_resource_path\":"
           << JsonString(relationship.target_resource_path) << ","
           << "\"target_resource_type\":"
           << JsonString(relationship.target_resource_type) << ","
           << "\"target_resource_name\":"
           << JsonString(relationship.target_resource_name) << ","
           << "\"resolved\":" << JsonBool(relationship.resolved) << ","
           << "\"resolution\":"
           << JsonString(relationship.resolution) << "}";
  }
  output << "]";
}

void AppendJsonResourceDetail(std::ostringstream& output,
                              const ResourceDetail& resource)
{
  output << "{"
         << "\"id\":" << JsonString(resource.summary.id) << ","
         << "\"type\":" << JsonString(resource.summary.type) << ","
         << "\"name\":" << JsonString(resource.summary.name) << ","
         << "\"file_path\":" << JsonString(resource.summary.file_path) << ","
         << "\"content\":" << JsonString(resource.content) << ","
           << "\"directives\":";
  AppendJsonDirectives(output, resource.directives);
  output << ",\"inherited_directives\":";
  AppendJsonInheritedDirectives(output, resource.inherited_directives);
  output << ",\"validation_messages\":";
  AppendJsonValidationMessages(output, resource.validation_messages);
  output << ",\"field_hints\":";
  AppendJsonFieldHints(output, resource.field_hints);
  output << "}";
}

void AppendJsonDirectives(std::ostringstream& output,
                          const std::vector<ResourceDirective>& directives)
{
  output << "[";
  for (size_t i = 0; i < directives.size(); ++i) {
    if (i > 0) output << ",";
    const auto& directive = directives[i];
    output << "{"
           << "\"key\":" << JsonString(directive.key) << ","
           << "\"value\":" << JsonString(directive.value) << ","
           << "\"line\":" << std::to_string(directive.line) << ","
           << "\"nesting_level\":" << std::to_string(directive.nesting_level)
           << "}";
  }
  output << "]";
}

void AppendJsonInheritedDirectives(
    std::ostringstream& output,
    const std::vector<InheritedDirective>& inherited_directives)
{
  output << "[";
  for (size_t i = 0; i < inherited_directives.size(); ++i) {
    if (i > 0) output << ",";
    const auto& inherited = inherited_directives[i];
    output << "{"
           << "\"key\":" << JsonString(inherited.directive.key) << ","
           << "\"value\":" << JsonString(inherited.directive.value) << ","
           << "\"line\":" << std::to_string(inherited.directive.line) << ","
           << "\"nesting_level\":"
           << std::to_string(inherited.directive.nesting_level) << ","
           << "\"source_resource_type\":"
           << JsonString(inherited.source_resource_type) << ","
           << "\"source_resource_name\":"
           << JsonString(inherited.source_resource_name) << ","
           << "\"source_resource_path\":"
           << JsonString(inherited.source_resource_path) << "}";
  }
  output << "]";
}

void AppendJsonValidationMessages(
    std::ostringstream& output,
    const std::vector<ValidationMessage>& validation_messages)
{
  output << "[";
  for (size_t i = 0; i < validation_messages.size(); ++i) {
    if (i > 0) output << ",";
    const auto& validation = validation_messages[i];
    output << "{"
           << "\"level\":" << JsonString(validation.level) << ","
           << "\"code\":" << JsonString(validation.code) << ","
           << "\"message\":" << JsonString(validation.message) << ","
           << "\"line\":" << std::to_string(validation.line) << "}";
  }
  output << "]";
}

void AppendJsonFieldHints(std::ostringstream& output,
                          const std::vector<ResourceFieldHint>& field_hints)
{
  output << "[";
  for (size_t i = 0; i < field_hints.size(); ++i) {
    if (i > 0) output << ",";
    const auto& field = field_hints[i];
    output << "{"
           << "\"key\":" << JsonString(field.key) << ","
           << "\"label\":" << JsonString(field.label) << ","
           << "\"required\":" << JsonBool(field.required) << ","
           << "\"repeatable\":" << JsonBool(field.repeatable) << ","
           << "\"deprecated\":" << JsonBool(field.deprecated) << ","
            << "\"present\":" << JsonBool(field.present) << ","
           << "\"inherited\":" << JsonBool(field.inherited) << ","
           << "\"value\":" << JsonString(field.value) << ","
           << "\"inherited_value\":" << JsonString(field.inherited_value) << ","
           << "\"datatype\":" << JsonString(field.datatype) << ","
           << "\"description\":" << JsonString(field.description) << ","
           << "\"default_value\":" << JsonString(field.default_value) << ","
           << "\"inherited_source_resource_type\":"
           << JsonString(field.inherited_source_resource_type) << ","
           << "\"inherited_source_resource_name\":"
           << JsonString(field.inherited_source_resource_name) << ","
           << "\"related_resource_type\":"
           << JsonString(field.related_resource_type) << ","
           << "\"allowed_values\":[";
    for (size_t value_index = 0; value_index < field.allowed_values.size();
         ++value_index) {
      if (value_index > 0) output << ",";
      output << JsonString(field.allowed_values[value_index]);
    }
    output << "]}";
  }
  output << "]";
}

void AppendJsonScheduleRunOverrideAllowedValues(
    std::ostringstream& output,
    const std::map<std::string, std::vector<std::string>>& allowed_values_by_key)
{
  output << "{";
  bool first_schedule_override = true;
  for (const auto& [directive_key, allowed_values] : allowed_values_by_key) {
    if (!first_schedule_override) output << ",";
    first_schedule_override = false;
    output << JsonString(directive_key) << ":" << "[";
    for (size_t value_index = 0; value_index < allowed_values.size();
         ++value_index) {
      if (value_index > 0) output << ",";
      output << JsonString(allowed_values[value_index]);
    }
    output << "]";
  }
  output << "}";
}

void AppendJsonEditableResourceDetail(std::ostringstream& output,
                                      const ResourceDetail& resource)
{
  output << "{"
         << "\"id\":" << JsonString(resource.summary.id) << ","
         << "\"type\":" << JsonString(resource.summary.type) << ","
         << "\"name\":" << JsonString(resource.summary.name) << ","
         << "\"file_path\":" << JsonString(resource.summary.file_path) << ","
         << "\"content\":" << JsonString(resource.content) << ","
         << "\"directives\":";
  AppendJsonDirectives(output, resource.directives);
  output << ",\"inherited_directives\":";
  AppendJsonInheritedDirectives(output, resource.inherited_directives);
  output << ",\"validation_messages\":";
  AppendJsonValidationMessages(output, resource.validation_messages);
  output << ",\"field_hints\":";
  AppendJsonFieldHints(output, resource.field_hints);
  output << ",\"schedule_run_override_allowed_values\":";
  AppendJsonScheduleRunOverrideAllowedValues(
      output, resource.schedule_run_override_allowed_values);
  output << ","
         << "\"editable\":true,"
         << "\"save_mode\":\"dry-run\"}";
}

void AppendJsonResourceEditPreview(std::ostringstream& output,
                                   const ResourceEditPreview& preview)
{
  output << "{"
         << "\"id\":" << JsonString(preview.summary.id) << ","
         << "\"type\":" << JsonString(preview.summary.type) << ","
         << "\"name\":" << JsonString(preview.summary.name) << ","
         << "\"file_path\":" << JsonString(preview.summary.file_path) << ","
         << "\"original_content\":"
         << JsonString(preview.original_content) << ","
         << "\"updated_content\":"
         << JsonString(preview.updated_content) << ","
         << "\"original_directives\":";
  AppendJsonDirectives(output, preview.original_directives);
  output << ",\"updated_directives\":";
  AppendJsonDirectives(output, preview.updated_directives);
  output << ",\"original_inherited_directives\":";
  AppendJsonInheritedDirectives(output, preview.original_inherited_directives);
  output << ",\"updated_inherited_directives\":";
  AppendJsonInheritedDirectives(output, preview.updated_inherited_directives);
  output << ",\"original_validation_messages\":";
  AppendJsonValidationMessages(output, preview.original_validation_messages);
  output << ",\"updated_validation_messages\":";
  AppendJsonValidationMessages(output, preview.updated_validation_messages);
  output << ",\"original_field_hints\":";
  AppendJsonFieldHints(output, preview.original_field_hints);
  output << ",\"updated_field_hints\":";
  AppendJsonFieldHints(output, preview.updated_field_hints);
  output << ",\"schedule_run_override_allowed_values\":";
  AppendJsonScheduleRunOverrideAllowedValues(
      output,
      BuildScheduleRunOverrideAllowedValues(preview.summary));
  output << ","
         << "\"changed\":" << JsonBool(preview.changed) << ","
         << "\"original_line_count\":"
         << std::to_string(preview.original_line_count) << ","
         << "\"updated_line_count\":"
         << std::to_string(preview.updated_line_count) << "}";
}

void AppendJsonTreeNode(std::ostringstream& output, const TreeNodeSummary& node)
{
  output << "{"
         << "\"id\":" << JsonString(node.id) << ","
         << "\"kind\":" << JsonString(node.kind) << ","
         << "\"label\":" << JsonString(node.label) << ","
         << "\"description\":" << JsonString(node.description) << ","
         << "\"resources\":";
  AppendJsonResourceArray(output, node.resources);
  output << ",\"children\":[";
  for (size_t i = 0; i < node.children.size(); ++i) {
    if (i > 0) output << ",";
    AppendJsonTreeNode(output, node.children[i]);
  }
  output << "]}";
}

TreeNodeSummary BuildDirectorTreeNode(const DirectorSummary& director,
                                      size_t director_index)
{
  return {
      MakeDirectorId(director_index),
      "director",
      director.name,
      director.config_root,
      director.resources,
      {},
  };
}

std::vector<TreeNodeSummary> BuildKnownClientTreeNodes(
    const DirectorSummary& director, size_t director_index)
{
  std::vector<TreeNodeSummary> nodes;
  const auto* file_daemon = FindDirectorDaemonByKind(director, "file-daemon");
  const auto local_client_name
      = file_daemon ? StripSurroundingQuotes(file_daemon->configured_name) : "";

  for (const auto& resource : director.resources) {
    if (resource.type != "client") continue;
    const auto client_name = StripSurroundingQuotes(resource.name);
    if (client_name.empty()) continue;
    if (!local_client_name.empty() && client_name == local_client_name) continue;

    nodes.push_back({MakeKnownClientNodeId(director_index, client_name), "client",
                     client_name, resource.file_path, {resource}, {}});
  }

  return nodes;
}

std::string FindConsoleDirectorReference(const DaemonSummary& console_daemon)
{
  for (const auto& resource : console_daemon.resources) {
    if (resource.type != "director") continue;
    const auto director_name
        = ExtractDirectiveValue(LoadResourceDetail(resource).directives, "Name");
    if (!director_name.empty()) return StripSurroundingQuotes(director_name);
  }

  const auto configuration = std::find_if(
      console_daemon.resources.begin(), console_daemon.resources.end(),
      [](const ResourceSummary& resource) { return resource.type == "configuration"; });
  if (configuration == console_daemon.resources.end()) return "";

  auto parser = CreateParserForComponent(ConfigComponent::kConsole,
                                         nullptr);
  if (!parser) return "";

  const auto binding = GetParserBinding(ConfigComponent::kConsole);
  if (binding.assign_global) { binding.assign_global(parser.get()); }
  if (!ParseStandaloneConfigFile(*parser, configuration->file_path, false)
      && !ParseStandaloneConfigFile(*parser, configuration->file_path, true)) {
    return "";
  }

  const auto resources = CollectResourceSummariesFromParser(
      *parser, "console-main", "console", configuration->config_root,
      configuration->file_path);
  const auto it = std::find_if(resources.begin(), resources.end(),
                               [](const ResourceSummary& resource) {
                                 return resource.type == "director";
                               });
  if (it == resources.end()) return "";

  const auto director_name
      = ExtractDirectiveValue(LoadResourceDetail(*it).directives, "Name");
  if (!director_name.empty()) {
    return StripSurroundingQuotes(director_name);
  }
  return "";
}

bool HasNamedResourceDirective(const ResourceDetail& detail, const std::string& key)
{
  return !StripSurroundingQuotes(ExtractDirectiveValue(detail.directives, key)).empty();
}

struct RelationshipResourceEntry {
  const ResourceSummary* resource = nullptr;
  std::string owner_node_id;
  std::string owner_label;
  std::string owner_kind;
};

struct RelationshipSubject {
  ResourceSummary summary;
  ResourceSummary source_resource;
  ResourceDetail detail;
  std::string owner_node_id;
  std::string owner_label;
  std::string owner_kind;
};

std::vector<RelationshipResourceEntry> CollectRelationshipResourceEntries(
    const DirectorSummary& director)
{
  std::vector<RelationshipResourceEntry> entries;
  for (const auto& resource : director.resources) {
    entries.push_back(
        {&resource, director.id, director.name, "director"});
  }
  for (const auto& daemon : director.daemons) {
    for (const auto& resource : daemon.resources) {
      entries.push_back({&resource, daemon.id, daemon.name, daemon.kind});
    }
  }
  return entries;
}

bool IsPrimaryDaemonResourceType(const std::string& owner_kind,
                                 const std::string& resource_type)
{
  if (owner_kind == "director") return resource_type == "director";
  if (owner_kind == "file-daemon") return resource_type == "client";
  if (owner_kind == "storage-daemon") return resource_type == "storage";
  if (owner_kind == "console") return resource_type == "console";
  return false;
}

bool IsOwnerScopedRelationshipType(const std::string& resource_type)
{
  return resource_type == "messages";
}

std::vector<RelationshipSubject> CollectRelationshipSubjects(
    const std::vector<RelationshipResourceEntry>& resource_entries)
{
  std::vector<RelationshipSubject> subjects;
  for (const auto& entry : resource_entries) {
    subjects.push_back({*entry.resource,
                        *entry.resource,
                        LoadResourceDetail(*entry.resource),
                        entry.owner_node_id,
                        entry.owner_label,
                        entry.owner_kind});

    if (entry.resource->type != "configuration" || entry.owner_kind != "console") {
      continue;
    }

    auto parser = CreateParserForComponent(ConfigComponent::kConsole,
                                           entry.resource->file_path.c_str());
    if (!parser) continue;
    const auto binding = GetParserBinding(ConfigComponent::kConsole);
    if (binding.assign_global) { binding.assign_global(parser.get()); }
    if (!parser->ParseConfig()) continue;

    for (const auto& summary : CollectResourceSummariesFromParser(
             *parser, "configuration-subject", entry.owner_kind,
             entry.resource->config_root, entry.resource->file_path)) {
      auto detail = LoadResourceDetail(summary);
      subjects.push_back({summary, *entry.resource, std::move(detail),
                          entry.owner_node_id, entry.owner_label,
                          entry.owner_kind});
    }
  }
  return subjects;
}

std::string CanonicalDirectiveKey(const ResourceSummary& summary,
                                  const ResourceDirective& directive)
{
  auto canonical_key = directive.key;
  const auto* table = LookupResourceTable(summary);
  if (const auto* item = LookupDirectiveResourceItem(summary, table, directive.key)) {
    canonical_key = item->name;
  }
  return canonical_key;
}

std::string RelationshipTargetLabel(const RelationshipResourceEntry& target)
{
  if (!target.resource) return "";
  if (IsPrimaryDaemonResourceType(target.owner_kind, target.resource->type)) {
    return target.owner_label;
  }
  return StripSurroundingQuotes(target.resource->name);
}

std::string RelationshipResourceDisplayName(const ResourceSummary& resource)
{
  const auto name = StripSurroundingQuotes(resource.name);
  return name.empty() ? resource.type : name;
}

std::string RelationshipResolutionForReference(
    const RelationshipSubject& source,
    const std::string& relation,
    const std::string& directive_key,
    const std::string& referenced_name,
    const RelationshipResourceEntry* target)
{
  const auto source_name = StripSurroundingQuotes(source.detail.summary.name).empty()
                               ? source.detail.summary.type
                               : StripSurroundingQuotes(source.detail.summary.name);
  if (!target || !target->resource) {
    return "Directive " + directive_key + " in " + source.detail.summary.type + " "
           + source_name + " references " + referenced_name
           + ", but no matching " + relation + " target was found.";
  }

  if (relation == "daemon-name") {
    return "Directive " + directive_key + " in " + source.detail.summary.type + " "
           + source_name + " resolves to daemon node " + target->owner_label + ".";
  }

  return "Directive " + directive_key + " in " + source.detail.summary.type + " "
         + source_name + " resolves to resource "
         + StripSurroundingQuotes(target->resource->name) + ".";
}

std::vector<RelationshipSummary> BuildRelationshipSummaries(
    const DatacenterSummary& summary)
{
  std::vector<RelationshipSummary> relationships;
  for (size_t director_index = 0; director_index < summary.directors.size();
       ++director_index) {
    const auto& director = summary.directors[director_index];
    const auto resource_entries = CollectRelationshipResourceEntries(director);
    const auto subjects = CollectRelationshipSubjects(resource_entries);
    const auto* file_daemon = FindDirectorDaemonByKind(director, "file-daemon");
    const auto* storage_daemon
        = FindDirectorDaemonByKind(director, "storage-daemon");
    const auto* console_daemon = FindDirectorDaemonByKind(director, "console");
    const auto known_client_nodes = BuildKnownClientTreeNodes(director, director_index);
    size_t relationship_index = 0;
    const auto director_resource = std::find_if(
        director.resources.begin(), director.resources.end(),
        [&director](const ResourceSummary& candidate) {
          return candidate.type == "director"
                 && NormalizeDirectiveNameForComparison(
                        StripSurroundingQuotes(candidate.name))
                        == NormalizeDirectiveNameForComparison(director.name);
        });
    const auto find_named_resource = [](const DaemonSummary* daemon,
                                        const std::string& resource_type,
                                        const std::string& resource_name) {
      return daemon ? std::find_if(
                          daemon->resources.begin(), daemon->resources.end(),
                          [&resource_type, &resource_name](const ResourceSummary& candidate) {
                            return candidate.type == resource_type
                                   && NormalizeDirectiveNameForComparison(
                                          StripSurroundingQuotes(candidate.name))
                                          == NormalizeDirectiveNameForComparison(resource_name);
                          })
                    : std::vector<ResourceSummary>::const_iterator{};
    };
    const auto find_known_client_node = [&known_client_nodes](
                                            const std::string& client_name) {
      return std::find_if(
          known_client_nodes.begin(), known_client_nodes.end(),
          [&client_name](const TreeNodeSummary& node) {
            return node.kind == "client"
                   && NormalizeDirectiveNameForComparison(node.label)
                           == NormalizeDirectiveNameForComparison(client_name);
          });
    };
    const auto add_reference_relationships
        = [&](const RelationshipSubject& subject, const std::string& canonical_key,
              const std::string& related_resource_type,
              const std::string& referenced_name) {
            if (related_resource_type.empty() || referenced_name.empty()) return;

            std::vector<const RelationshipResourceEntry*> matches;
            for (const auto& entry : resource_entries) {
              if (!entry.resource) continue;
              if (entry.resource->type != related_resource_type) continue;
              if (subject.owner_kind == "director"
                  && (related_resource_type == "client"
                      || related_resource_type == "storage")
                  && entry.owner_kind != "director") {
                continue;
              }
              if (IsOwnerScopedRelationshipType(related_resource_type)
                  && entry.owner_node_id != subject.owner_node_id) {
                continue;
              }
              if (NormalizeDirectiveNameForComparison(
                      StripSurroundingQuotes(entry.resource->name))
                  != NormalizeDirectiveNameForComparison(referenced_name)) {
                continue;
              }
              matches.push_back(&entry);
            }

            for (const auto* match : matches) {
              const auto relation
                  = IsPrimaryDaemonResourceType(match->owner_kind, match->resource->type)
                        ? "daemon-name"
                        : "resource-name";
              relationships.push_back({
                  MakeRelationshipId(director_index, relationship_index++),
                  relation,
                  referenced_name,
                  subject.owner_node_id,
                  subject.owner_label,
                  match->owner_node_id,
                  RelationshipTargetLabel(*match),
                  subject.source_resource.id,
                  subject.source_resource.file_path,
                  subject.detail.summary.type,
                  RelationshipResourceDisplayName(subject.detail.summary),
                  match->resource->id,
                  match->resource->file_path,
                  match->resource->type,
                  RelationshipResourceDisplayName(*match->resource),
                  true,
                  RelationshipResolutionForReference(subject, relation, canonical_key,
                                                     referenced_name, match),
              });
            }
          };

    for (const auto& resource : director.resources) {
      const bool is_client = resource.type == "client";
      const bool is_storage = resource.type == "storage";
      if (!is_client && !is_storage) continue;

      const auto* target = is_client ? file_daemon : storage_daemon;
      const auto target_resource = is_client
                                       ? find_named_resource(target, "client",
                                                             StripSurroundingQuotes(resource.name))
                                       : find_named_resource(target, "storage",
                                                             StripSurroundingQuotes(resource.name));
      if (!target || target_resource == target->resources.end()
          || target->configured_name.empty()
          || NormalizeDirectiveNameForComparison(target->configured_name)
                 != NormalizeDirectiveNameForComparison(resource.name)) {
        if (is_client) {
          const auto known_client = find_known_client_node(
              StripSurroundingQuotes(resource.name));
          if (known_client != known_client_nodes.end()) {
            relationships.push_back({
                MakeRelationshipId(director_index, relationship_index++),
                "resource-name",
                resource.name,
                director.id,
                director.name,
                known_client->id,
                known_client->label,
                resource.id,
                resource.file_path,
                resource.type,
                RelationshipResourceDisplayName(resource),
                resource.id,
                resource.file_path,
                resource.type,
                RelationshipResourceDisplayName(resource),
                true,
                "Resolved to known client node " + known_client->label + ".",
            });
            const auto client_password = StripSurroundingQuotes(
                ExtractDirectiveValue(LoadResourceDetail(resource).directives,
                                      "Password"));
            if (!client_password.empty()) {
              relationships.push_back({
                  MakeRelationshipId(director_index, relationship_index++),
                  "shared-password",
                  "common password",
                  director.id,
                  director.name,
                  known_client->id,
                  known_client->label,
                  resource.id,
                  resource.file_path,
                  resource.type,
                  RelationshipResourceDisplayName(resource),
                  resource.id,
                  resource.file_path,
                  resource.type,
                  RelationshipResourceDisplayName(resource),
                  true,
                  "Known client node " + known_client->label
                      + " uses the configured client password.",
              });
            }
          }
        }
        continue;
      }

      const auto to_node_id = target->id;
      const auto to_label = target->name;
      const auto target_resource_id = target_resource->id;
      const auto target_resource_path = target_resource->file_path;
      const auto target_resource_type = target_resource->type;
      const auto target_resource_name
          = StripSurroundingQuotes(target_resource->name);
      const auto resolution = "Resolved to "
                              + std::string(is_client ? "client" : "storage")
                              + " resource " + target_resource->name + " in "
                              + target->name + ".";

      relationships.push_back({
          MakeRelationshipId(director_index, relationship_index++),
          is_client ? "resource-name" : "storage",
          resource.name,
          director.id,
          director.name,
          to_node_id,
          to_label,
          resource.id,
          resource.file_path,
          resource.type,
          RelationshipResourceDisplayName(resource),
          target_resource_id,
          target_resource_path,
          target_resource_type,
          target_resource_name,
          true,
          resolution,
      });

      if (is_client && director_resource != director.resources.end()) {
        const auto client_director_resource
            = find_named_resource(target, "director", director.name);
        if (client_director_resource != target->resources.end()) {
          relationships.push_back({
              MakeRelationshipId(director_index, relationship_index++),
              "resource-name",
              director.name,
              target->id,
              target->name,
              director.id,
              director.name,
              client_director_resource->id,
              client_director_resource->file_path,
              client_director_resource->type,
              RelationshipResourceDisplayName(*client_director_resource),
              director_resource->id,
              director_resource->file_path,
              director_resource->type,
              RelationshipResourceDisplayName(*director_resource),
              true,
              "Resolved to director resource "
                  + director_resource->name + " from the client director resource "
                  + client_director_resource->name + ".",
          });
        }
      }

      if (is_storage && director_resource != director.resources.end()) {
        const auto storage_director_resource
            = find_named_resource(target, "director", director.name);
        if (storage_director_resource != target->resources.end()) {
          relationships.push_back({
              MakeRelationshipId(director_index, relationship_index++),
              "resource-name",
              director.name,
              target->id,
              target->name,
              director.id,
              director.name,
              storage_director_resource->id,
              storage_director_resource->file_path,
              storage_director_resource->type,
              RelationshipResourceDisplayName(*storage_director_resource),
              director_resource->id,
              director_resource->file_path,
              director_resource->type,
              RelationshipResourceDisplayName(*director_resource),
              true,
              "Resolved to director resource "
                  + director_resource->name + " from the storage director resource "
                  + storage_director_resource->name + ".",
          });
        }
      }

      if (!is_storage || !target) continue;

      const auto storage_detail = LoadResourceDetail(resource);
      for (const auto& directive : storage_detail.directives) {
        if (!IsTopLevelDirective(directive)) continue;
        if (NormalizeDirectiveNameForComparison(directive.key) != "device") {
          continue;
        }

        const auto device_name = StripSurroundingQuotes(directive.value);
        const auto device_resource = std::find_if(
            target->resources.begin(), target->resources.end(),
            [&device_name](const ResourceSummary& candidate) {
              return candidate.type == "device"
                     && StripSurroundingQuotes(candidate.name) == device_name;
            });

        if (device_resource == target->resources.end()) continue;

        relationships.push_back({
            MakeRelationshipId(director_index, relationship_index++),
            "device",
            device_name,
            director.id,
            director.name,
            target->id,
            target->name,
            resource.id,
            resource.file_path,
            resource.type,
            RelationshipResourceDisplayName(resource),
            device_resource->id,
            device_resource->file_path,
            device_resource->type,
            RelationshipResourceDisplayName(*device_resource),
            true,
            "Resolved to storage-daemon device resource "
                + device_resource->name + ".",
        });
      }
    }

    if (console_daemon) {
      const auto find_console_subject
          = [&subjects](const std::string& resource_type, const std::string& resource_name) {
              return std::find_if(
                  subjects.begin(), subjects.end(),
                  [&resource_type, &resource_name](const RelationshipSubject& subject) {
                    return subject.owner_kind == "console"
                           && subject.detail.summary.type == resource_type
                           && NormalizeDirectiveNameForComparison(
                                  StripSurroundingQuotes(subject.detail.summary.name))
                                  == NormalizeDirectiveNameForComparison(resource_name);
                  });
            };
      const auto has_named_console_subject = [&subjects]() {
        return std::any_of(
            subjects.begin(), subjects.end(),
            [](const RelationshipSubject& subject) {
              return subject.owner_kind == "console"
                     && subject.detail.summary.type == "console"
                     && HasNamedResourceDirective(subject.detail, "Name");
            });
      };

      const auto referenced_director = FindConsoleDirectorReference(*console_daemon);
      if (!referenced_director.empty()) {
        const auto console_director_subject
            = find_console_subject("director", referenced_director);
        const auto target_resource = std::find_if(
            director.resources.begin(), director.resources.end(),
            [&referenced_director](const ResourceSummary& resource) {
              return resource.type == "director"
                     && NormalizeDirectiveNameForComparison(
                            StripSurroundingQuotes(resource.name))
                            == NormalizeDirectiveNameForComparison(referenced_director);
            });
        if (console_director_subject != subjects.end()
            && target_resource != director.resources.end()) {
          relationships.push_back({
              MakeRelationshipId(director_index, relationship_index++),
              "resource-name",
              referenced_director,
              console_daemon->id,
              console_daemon->name,
              director.id,
              director.name,
              console_director_subject->source_resource.id,
              console_director_subject->source_resource.file_path,
              console_director_subject->detail.summary.type,
              RelationshipResourceDisplayName(console_director_subject->detail.summary),
              target_resource->id,
              target_resource->file_path,
              target_resource->type,
              RelationshipResourceDisplayName(*target_resource),
              true,
              "Resolved to director resource "
                  + RelationshipResourceDisplayName(*target_resource)
                  + " from the bconsole Director resource "
                  + RelationshipResourceDisplayName(console_director_subject->detail.summary)
                  + ".",
          });

          if (!has_named_console_subject()) {
            const auto bconsole_password = StripSurroundingQuotes(
                ExtractDirectiveValue(console_director_subject->detail.directives,
                                      "Password"));
            const auto director_password = StripSurroundingQuotes(
                ExtractDirectiveValue(LoadResourceDetail(*target_resource).directives,
                                      "Password"));
            if (!bconsole_password.empty() && bconsole_password == director_password) {
              relationships.push_back({
                  MakeRelationshipId(director_index, relationship_index++),
                  "shared-password",
                  "common password",
                  console_daemon->id,
                  console_daemon->name,
                  director.id,
                  director.name,
                  console_director_subject->source_resource.id,
                  console_director_subject->source_resource.file_path,
                  console_director_subject->detail.summary.type,
                  RelationshipResourceDisplayName(console_director_subject->detail.summary),
                  target_resource->id,
                  target_resource->file_path,
                  target_resource->type,
                  RelationshipResourceDisplayName(*target_resource),
                  true,
                  "The default bconsole authentication uses the same password as "
                  "the director resource.",
              });
            }
          }
        }
      }

      for (const auto& subject : subjects) {
        if (subject.owner_kind != "console" || subject.detail.summary.type != "console"
            || !HasNamedResourceDirective(subject.detail, "Name")) {
          continue;
        }

        const auto console_name = StripSurroundingQuotes(subject.detail.summary.name);
        const auto target_resource = std::find_if(
            director.resources.begin(), director.resources.end(),
            [&console_name](const ResourceSummary& resource) {
              return resource.type == "console"
                     && NormalizeDirectiveNameForComparison(
                            StripSurroundingQuotes(resource.name))
                            == NormalizeDirectiveNameForComparison(console_name);
            });
        if (target_resource == director.resources.end()) continue;

        relationships.push_back({
            MakeRelationshipId(director_index, relationship_index++),
            "resource-name",
            console_name,
            console_daemon->id,
            console_daemon->name,
            director.id,
            director.name,
            subject.source_resource.id,
            subject.source_resource.file_path,
            subject.detail.summary.type,
            RelationshipResourceDisplayName(subject.detail.summary),
            target_resource->id,
            target_resource->file_path,
            target_resource->type,
            RelationshipResourceDisplayName(*target_resource),
            true,
            "Resolved to director console resource "
                + RelationshipResourceDisplayName(*target_resource)
                + " from the bconsole Console resource "
                + RelationshipResourceDisplayName(subject.detail.summary) + ".",
        });

        const auto console_password = StripSurroundingQuotes(
            ExtractDirectiveValue(subject.detail.directives, "Password"));
        const auto director_console_password = StripSurroundingQuotes(
            ExtractDirectiveValue(LoadResourceDetail(*target_resource).directives,
                                  "Password"));
        if (!console_password.empty() && console_password == director_console_password) {
          relationships.push_back({
              MakeRelationshipId(director_index, relationship_index++),
              "shared-password",
              "common password",
              console_daemon->id,
              console_daemon->name,
              director.id,
              director.name,
              subject.source_resource.id,
              subject.source_resource.file_path,
              subject.detail.summary.type,
              RelationshipResourceDisplayName(subject.detail.summary),
              target_resource->id,
              target_resource->file_path,
              target_resource->type,
              RelationshipResourceDisplayName(*target_resource),
              true,
              "The bconsole Console resource and the director Console resource "
              "share the authentication password.",
          });
        }
      }
    }

    for (const auto& subject : subjects) {
      const auto directive_metadata = BuildDirectiveMetadata(subject.detail.summary);
      for (const auto& directive : subject.detail.directives) {
        if (!IsTopLevelDirective(directive)) continue;

        const auto canonical_key = CanonicalDirectiveKey(subject.detail.summary, directive);
        const auto metadata_it = directive_metadata.find(canonical_key);
        if (metadata_it == directive_metadata.end()
            || metadata_it->second.related_resource_type.empty()) {
          continue;
        }

        const auto normalized_key
            = NormalizeDirectiveNameForComparison(canonical_key);
        if ((subject.detail.summary.type == "storage" && normalized_key == "device")
            || (subject.owner_kind == "console"
                && metadata_it->second.related_resource_type == "director")) {
          continue;
        }

        const auto referenced_name = StripSurroundingQuotes(directive.value);
        if (referenced_name.empty()) continue;
        add_reference_relationships(subject, canonical_key,
                                    metadata_it->second.related_resource_type,
                                    referenced_name);
      }

      if (subject.detail.summary.type == "schedule") {
        for (const auto& run_reference
             : ExtractScheduleRunOverrideReferences(subject.detail)) {
          add_reference_relationships(subject, run_reference.directive_key,
                                      run_reference.related_resource_type,
                                      run_reference.referenced_name);
        }
      }
    }

    std::vector<const RelationshipSubject*> password_subjects;
    for (const auto& subject : subjects) {
      const auto password = StripSurroundingQuotes(
          ExtractDirectiveValue(subject.detail.directives, "Password"));
      if (password.empty()) continue;
      password_subjects.push_back(&subject);
    }
    for (size_t left = 0; left < password_subjects.size(); ++left) {
      const auto left_password = StripSurroundingQuotes(
          ExtractDirectiveValue(password_subjects[left]->detail.directives, "Password"));
      for (size_t right = left + 1; right < password_subjects.size(); ++right) {
        const auto right_password = StripSurroundingQuotes(
            ExtractDirectiveValue(password_subjects[right]->detail.directives,
                                  "Password"));
        if (left_password.empty() || left_password != right_password) continue;
        if (password_subjects[left]->owner_kind == "console"
            || password_subjects[right]->owner_kind == "console") {
          continue;
        }

        relationships.push_back({
            MakeRelationshipId(director_index, relationship_index++),
            "shared-password",
            "common password",
            password_subjects[left]->owner_node_id,
            password_subjects[left]->owner_label,
            password_subjects[right]->owner_node_id,
            password_subjects[right]->owner_label,
            password_subjects[left]->source_resource.id,
            password_subjects[left]->source_resource.file_path,
            password_subjects[left]->detail.summary.type,
            RelationshipResourceDisplayName(password_subjects[left]->detail.summary),
            password_subjects[right]->source_resource.id,
            password_subjects[right]->source_resource.file_path,
            password_subjects[right]->detail.summary.type,
            RelationshipResourceDisplayName(password_subjects[right]->detail.summary),
            true,
            "Resources share a common password value.",
        });
      }
    }
  }

  return relationships;
}

const TreeNodeSummary* FindTreeNodeById(const TreeNodeSummary& node,
                                        const std::string& node_id)
{
  if (node.id == node_id) return &node;
  for (const auto& child : node.children) {
    if (const auto* match = FindTreeNodeById(child, node_id)) return match;
  }
  return nullptr;
}

const ResourceSummary* FindResourceById(const std::vector<ResourceSummary>& resources,
                                        const std::string& resource_id)
{
  const auto it = std::find_if(resources.begin(), resources.end(),
                               [&resource_id](const ResourceSummary& resource) {
                                 return resource.id == resource_id;
                               });
  return it == resources.end() ? nullptr : &(*it);
}

const ResourceSummary* FindResourceById(const TreeNodeSummary& node,
                                        const std::string& resource_id)
{
  if (const auto* resource = FindResourceById(node.resources, resource_id)) {
    return resource;
  }
  for (const auto& child : node.children) {
    if (const auto* match = FindResourceById(child, resource_id)) return match;
  }
  return nullptr;
}
}  // namespace

std::vector<std::filesystem::path> DefaultConfigRoots()
{
  return {"/etc/bareos"};
}

std::string ResourceBlockNameForType(const std::string& type)
{
  return ResourceBlockNameForTypeImpl(type);
}

DatacenterSummary DiscoverDatacenterSummary(
    const std::vector<std::filesystem::path>& config_roots)
{
  DatacenterSummary summary;
  summary.tree = {summary.id, "datacenter", summary.name, "", {}, {}};

  std::vector<std::filesystem::path> roots = config_roots;
  if (roots.empty()) roots = DefaultConfigRoots();

  for (size_t director_index = 0; director_index < roots.size();
       ++director_index) {
    const auto& root = roots[director_index];
    summary.config_roots.push_back(root.string());

    const auto director_dir = root / "bareos-dir.d";
    const auto director_conf = root / "bareos-dir.conf";
    if (!std::filesystem::exists(director_dir)
        && !std::filesystem::exists(director_conf)) {
      continue;
    }

    DirectorSummary director;
    director.id = MakeDirectorId(summary.directors.size());
    director.name = DirectorNameForRoot(root);
    director.config_root = root.string();
    director.resources = DiscoverResourcesInDirectory(
        director_dir,
        "resource-" + std::to_string(director_index) + "-director",
        "director",
        root.string());

    if (std::filesystem::exists(director_conf)) {
      director.resources.insert(
          director.resources.begin(),
          {"resource-" + std::to_string(director_index) + "-director-main",
           "configuration", director_conf.filename().string(),
           director_conf.string(), "director", root.string()});
    }

    director.daemons.push_back(DiscoverDaemon(root, director_index,
                                              "storage-daemon", "bareos-sd.d",
                                              "bareos-sd"));
    director.daemons.push_back(DiscoverDaemon(root, director_index,
                                              "file-daemon", "bareos-fd.d",
                                              "bareos-fd"));
    director.daemons.push_back(DiscoverDaemon(root, director_index,
                                              "console", "bconsole.d",
                                              "bconsole"));
    summary.directors.push_back(std::move(director));
    summary.tree.children.push_back(
        BuildDirectorTreeNode(summary.directors.back(), director_index));

    for (const auto& daemon : summary.directors.back().daemons) {
      if (daemon.resources.empty()) continue;
      summary.tree.children.push_back({
          daemon.id,
          daemon.kind,
          daemon.name,
          daemon.config_root,
          daemon.resources,
          {},
      });
    }

    for (auto& node : BuildKnownClientTreeNodes(summary.directors.back(),
                                                director_index)) {
      summary.tree.children.push_back(std::move(node));
    }
  }

  return summary;
}

std::string SerializeDatacenterSummary(const DatacenterSummary& summary)
{
  std::ostringstream output;
  output << "{"
         << "\"id\":" << JsonString(summary.id) << ","
         << "\"name\":" << JsonString(summary.name) << ","
         << "\"config_roots\":[";

  for (size_t i = 0; i < summary.config_roots.size(); ++i) {
    if (i > 0) output << ",";
    output << JsonString(summary.config_roots[i]);
  }

  output << "],\"tree\":";
  AppendJsonTreeNode(output, summary.tree);
  output << ",\"directors\":[";
  for (size_t i = 0; i < summary.directors.size(); ++i) {
    if (i > 0) output << ",";
    const auto& director = summary.directors[i];
    output << "{"
           << "\"id\":" << JsonString(director.id) << ","
           << "\"name\":" << JsonString(director.name) << ","
           << "\"config_root\":" << JsonString(director.config_root) << ","
           << "\"resources\":";
    AppendJsonResourceArray(output, director.resources);
    output << ",\"daemons\":[";
    for (size_t daemon_index = 0; daemon_index < director.daemons.size();
         ++daemon_index) {
      if (daemon_index > 0) output << ",";
      const auto& daemon = director.daemons[daemon_index];
      output << "{"
             << "\"id\":" << JsonString(daemon.id) << ","
             << "\"kind\":" << JsonString(daemon.kind) << ","
             << "\"name\":" << JsonString(daemon.name) << ","
             << "\"configured_name\":"
             << JsonString(daemon.configured_name) << ","
             << "\"config_root\":" << JsonString(daemon.config_root) << ","
             << "\"resources\":";
      AppendJsonResourceArray(output, daemon.resources);
      output << "}";
    }
    output << "]}";
  }

  output << "]}";
  return output.str();
}

std::string SerializeTreeNodeSummary(const TreeNodeSummary& node)
{
  std::ostringstream output;
  AppendJsonTreeNode(output, node);
  return output.str();
}

std::string SerializeResourceSummaries(
    const std::vector<ResourceSummary>& resources)
{
  std::ostringstream output;
  AppendJsonResourceArray(output, resources);
  return output.str();
}

std::string SerializeRelationshipSummaries(
    const std::vector<RelationshipSummary>& relationships)
{
  std::ostringstream output;
  AppendJsonRelationshipArray(output, relationships);
  return output.str();
}

std::string SerializeResourceDetail(const ResourceDetail& resource)
{
  std::ostringstream output;
  AppendJsonResourceDetail(output, resource);
  return output.str();
}

std::string SerializeEditableResourceDetail(const ResourceDetail& resource)
{
  std::ostringstream output;
  AppendJsonEditableResourceDetail(output, resource);
  return output.str();
}

std::string SerializeResourceFieldHints(
    const std::vector<ResourceFieldHint>& field_hints)
{
  std::ostringstream output;
  AppendJsonFieldHints(output, field_hints);
  return output.str();
}

std::string SerializeResourceEditPreview(const ResourceEditPreview& preview)
{
  std::ostringstream output;
  AppendJsonResourceEditPreview(output, preview);
  return output.str();
}

std::vector<RelationshipSummary> FindAllRelationships(
    const DatacenterSummary& summary)
{
  return BuildRelationshipSummaries(summary);
}

std::vector<ScheduleRunOverrideReference> ExtractScheduleRunOverrideReferences(
    const ResourceDetail& detail)
{
  std::vector<ScheduleRunOverrideReference> references;
  size_t run_index = 0;
  for (const auto& directive : detail.directives) {
    if (!IsTopLevelDirective(directive)
        || NormalizeDirectiveNameForComparison(directive.key) != "run") {
      continue;
    }
    auto run_references = ParseScheduleRunOverrideReferences(directive.value,
                                                             directive.line,
                                                             run_index++);
    references.insert(references.end(), run_references.begin(), run_references.end());
  }
  return references;
}

const TreeNodeSummary* FindTreeNodeById(const DatacenterSummary& summary,
                                        const std::string& node_id)
{
  return FindTreeNodeById(summary.tree, node_id);
}

const ResourceSummary* FindResourceById(const DatacenterSummary& summary,
                                        const std::string& resource_id)
{
  return FindResourceById(summary.tree, resource_id);
}

std::vector<RelationshipSummary> FindRelationshipsForNode(
    const DatacenterSummary& summary, const std::string& node_id)
{
  const auto all_relationships = FindAllRelationships(summary);
  std::vector<RelationshipSummary> matches;
  for (const auto& relationship : all_relationships) {
    if (relationship.from_node_id == node_id
        || relationship.to_node_id == node_id) {
      matches.push_back(relationship);
    }
  }
  return matches;
}

ResourceDetail LoadResourceDetail(const ResourceSummary& resource)
{
  if (resource.type == "configuration") {
    std::ifstream input(resource.file_path);
    std::ostringstream content;
    content << input.rdbuf();
    return BuildResourceDetail(resource, content.str());
  }

  if (!resource.file_path.empty() && std::filesystem::exists(resource.file_path)) {
    if (resource.source_line_start != 0 && resource.source_line_end != 0) {
      return BuildResourceDetail(resource,
                                 ReadFileLineRange(resource.file_path,
                                                   resource.source_line_start,
                                                   resource.source_line_end));
    }

    std::ifstream input(resource.file_path);
    std::ostringstream content;
    content << input.rdbuf();
    return BuildResourceDetail(resource, content.str());
  }

  const auto dump = DumpResourceFromParser(resource);
  if (!dump.content.empty() || !dump.directives.empty()) {
    auto directives = ParseDirectivesFromContent(dump.content);
    const auto* table = LookupResourceTable(resource);
    for (auto& directive : directives) {
      if (const auto* item
          = LookupDirectiveResourceItem(resource, table, directive.key)) {
        directive.key = item->name;
      }
    }

    const auto inherited_directives = ResolveInheritedDirectives(resource, directives);
    auto display_directives = directives;
    SortResourceDirectivesForDisplay(resource, display_directives);
    return {resource,
            dump.content,
            display_directives,
            inherited_directives,
            ValidateResourceContent(resource, dump.content, directives,
                                    inherited_directives),
            BuildResourceFieldHints(resource, directives, inherited_directives),
            BuildScheduleRunOverrideAllowedValues(resource)};
  }

  return {resource, {}, {}, {}, {}, {}, {}};
}

std::vector<ResourceSummary> DiscoverResourceSummariesFromContent(
    const ResourceSummary& resource, const std::string& content)
{
  const auto preferred_component = ComponentForSummary(resource);
  if (preferred_component == ConfigComponent::kUnknown) return {};

  static std::atomic<size_t> temp_counter{0};
  const auto temp_root = std::filesystem::temp_directory_path()
                         / ("bareos-config-" + std::to_string(::getpid()) + "-"
                            + std::to_string(temp_counter.fetch_add(1)));
  std::error_code ec;
  std::filesystem::create_directories(temp_root, ec);
  if (ec) return {};

  const auto file_name = std::filesystem::path(resource.file_path).filename().empty()
                             ? std::filesystem::path("resource.conf")
                             : std::filesystem::path(resource.file_path).filename();
  auto temp_file = temp_root / file_name;
  const bool staged_in_root
      = StageEditedContentInConfigRoot(resource, content, temp_root, temp_file);
  if (!staged_in_root) {
    std::ofstream output(temp_file, std::ios::trunc);
    if (!output) {
      std::filesystem::remove_all(temp_root, ec);
      return {};
    }
    output << content;
    if (!output) {
      std::filesystem::remove_all(temp_root, ec);
      return {};
    }
  }

  std::vector<ResourceSummary> summaries;
  const auto candidates = resource.type == "configuration"
                              ? std::vector<ConfigComponent>{preferred_component}
                              : CandidateComponentsForType(resource.type,
                                                           preferred_component);
  for (const auto candidate : candidates) {
    const auto make_parser = [&]() {
      auto parser = CreateParserForComponent(candidate, nullptr);
      if (!parser) return parser;
      const auto binding = GetParserBinding(candidate);
      if (binding.assign_global) { binding.assign_global(parser.get()); }
      return parser;
    };

    auto parser = make_parser();
    if (!parser) continue;

    if (staged_in_root && resource.type != "configuration") {
      if (ParseStandaloneConfigFile(*parser, temp_file, false)
          || ParseStandaloneConfigFile(*parser, temp_file, true)) {
        summaries = CollectResourceSummariesFromParser(
            *parser, resource.id.empty() ? "content" : resource.id,
            resource.component, resource.config_root, temp_file);
      }
      if (summaries.empty()) {
        parser = make_parser();
        if (!parser
            || !ParseConfigIncludeTree(*parser, temp_root, candidate, true)) {
          continue;
        }
      }
    } else if (!ParseStandaloneConfigFile(*parser, temp_file, false)
               && !ParseStandaloneConfigFile(*parser, temp_file, true)) {
      continue;
    }

    if (summaries.empty()) {
      summaries = CollectResourceSummariesFromParser(
          *parser, resource.id.empty() ? "content" : resource.id,
          resource.component, resource.config_root, temp_file);
    }
    if (!summaries.empty()) break;
  }

  for (auto& summary : summaries) {
    summary.file_path = resource.file_path;
    summary.component = resource.component;
    summary.config_root = resource.config_root;
  }

  if (summaries.empty() && resource.type == "user") {
    // bareos-config intentionally keeps compatibility for password-bearing
    // user files even though parser-backed rediscovery is still incomplete.
    auto fallback = resource;
    const auto line_count
        = static_cast<size_t>(std::count(content.begin(), content.end(), '\n'));
    const auto content_end_line = line_count == 0 ? 1 : line_count;
    if (resource.source_line_start != 0 && resource.source_line_end != 0
        && content_end_line >= resource.source_line_end) {
      fallback.source_line_start = resource.source_line_start;
      fallback.source_line_end = resource.source_line_end;
    } else {
      fallback.source_line_start = 1;
      fallback.source_line_end = content_end_line;
    }
    summaries.push_back(std::move(fallback));
  }

  std::filesystem::remove_all(temp_root, ec);
  return summaries;
}

ResourceDetail BuildResourceDetail(const ResourceSummary& resource,
                                   const std::string& content)
{
  if (resource.type == "configuration") {
    return {resource, content, {}, {}, {}, {}, {}};
  }

  ParsedResourceDump dump;
  if (content.empty()) {
    dump = DumpResourceFromParser(resource);
  } else {
    dump.content = content;
  }
  if (dump.content.empty()) dump.content = content;

  auto directives = ParseDirectivesFromContent(dump.content);
  const auto* table = LookupResourceTable(resource);
  for (auto& directive : directives) {
    if (const auto* item
        = LookupDirectiveResourceItem(resource, table, directive.key)) {
      directive.key = item->name;
    }
  }

  const auto inherited_directives = ResolveInheritedDirectives(resource, directives);
  auto display_directives = directives;
  SortResourceDirectivesForDisplay(resource, display_directives);
  return {resource,
          dump.content,
          display_directives,
          inherited_directives,
          ValidateResourceContent(resource, dump.content, directives,
                                  inherited_directives),
          BuildResourceFieldHints(resource, directives, inherited_directives),
          BuildScheduleRunOverrideAllowedValues(resource)};
}

std::string MaybeRenameResourceFilePath(const ResourceSummary& summary,
                                        const std::string& old_name,
                                        const std::string& new_name)
{
  if (old_name.empty() || new_name.empty() || old_name == new_name) {
    return summary.file_path;
  }

  const auto path = std::filesystem::path(summary.file_path);
  if (path.extension() != ".conf") return summary.file_path;
  if (path.stem() != old_name) return summary.file_path;
  return (path.parent_path() / (new_name + path.extension().string())).string();
}

ResourceEditPreview BuildResourceEditPreview(
    const ResourceDetail& resource, const std::string& updated_content)
{
  const auto updated_detail = BuildResourceDetail(resource.summary, updated_content);
  const auto parsed_updated_directives = updated_detail.directives;
  const auto updated_inherited_directives = updated_detail.inherited_directives;
  auto display_updated_directives = parsed_updated_directives;
  SortResourceDirectivesForDisplay(resource.summary, display_updated_directives);
  auto updated_summary = resource.summary;
  const auto old_name = ExtractResourceName(resource.summary, resource.directives);
  updated_summary.name = ExtractResourceName(updated_summary,
                                             parsed_updated_directives);
  updated_summary.file_path = MaybeRenameResourceFilePath(
      updated_summary, old_name, updated_summary.name);
  return {updated_summary,
          resource.content,
          updated_content,
          resource.directives,
          display_updated_directives,
          resource.inherited_directives,
          updated_inherited_directives,
          resource.validation_messages,
          ValidateResourceContent(resource.summary, updated_content,
                                  parsed_updated_directives,
                                  updated_inherited_directives),
          resource.field_hints,
          BuildResourceFieldHints(resource.summary, parsed_updated_directives,
                                  updated_inherited_directives),
          resource.content != updated_content,
          CountLines(resource.content),
          CountLines(updated_content)};
}

ResourceEditPreview BuildFieldHintEditPreview(
    const ResourceDetail& resource,
    const std::vector<ResourceFieldHint>& updated_field_hints)
{
  return BuildResourceEditPreview(
      resource, ApplyFieldHintsToContent(resource, updated_field_hints));
}
