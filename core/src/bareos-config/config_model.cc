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
#include <cctype>
#include <fstream>
#include <map>
#include <memory>
#include <set>
#include <sstream>

#include "console/console_conf.h"
#include "console/console_globals.h"
#include "dird/dird_conf.h"
#include "dird/dird_globals.h"
#include "filed/filed_conf.h"
#include "filed/filed_globals.h"
#include "include/bareos.h"
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

struct ParsedResourceBlock;

std::map<std::string, DirectiveMetadata> BuildDirectiveMetadata(
    const ResourceSummary& summary);
const ResourceTable* LookupResourceTable(const ResourceSummary& summary);
const ResourceItem* LookupDirectiveResourceItem(const ResourceSummary& summary,
                                                const ResourceTable* table,
                                                const std::string& key);
std::string TrimWhitespace(std::string value);
std::filesystem::path FindConfigRootForPath(const std::filesystem::path& path);
std::vector<ParsedResourceBlock> ParseNamedResourceBlocks(
    const std::filesystem::path& file_path, const std::string& block_name);
std::vector<ResourceDirective> ParseResourceDirectives(const std::string& content);

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

ConfigComponent DetectComponentForPath(const std::filesystem::path& path)
{
  const auto path_string = path.string();
  if (path_string.find("bareos-dir.d") != std::string::npos
      || path.filename() == "bareos-dir.conf") {
    return ConfigComponent::kDirector;
  }
  if (path_string.find("bareos-fd.d") != std::string::npos
      || path.filename() == "bareos-fd.conf") {
    return ConfigComponent::kFileDaemon;
  }
  if (path_string.find("bareos-sd.d") != std::string::npos
      || path.filename() == "bareos-sd.conf") {
    return ConfigComponent::kStorageDaemon;
  }
  if (path.filename() == "bconsole.conf") { return ConfigComponent::kConsole; }
  return ConfigComponent::kUnknown;
}

std::filesystem::path FindConfigRootForPath(const std::filesystem::path& path)
{
  for (auto current = path; !current.empty();) {
    const auto name = current.filename().string();
    if (name == "bareos-dir.d" || name == "bareos-fd.d" || name == "bareos-sd.d") {
      return current.parent_path();
    }
    const auto parent = current.parent_path();
    if (parent == current) break;
    current = parent;
  }

  const auto filename = path.filename().string();
  if (filename == "bareos-dir.conf" || filename == "bareos-fd.conf"
      || filename == "bareos-sd.conf" || filename == "bconsole.conf") {
    return path.parent_path();
  }

  return {};
}

std::string ParserResourceTypeForSummary(const ResourceSummary& summary)
{
  if (summary.type == "autochanger") return "Autochanger";
  if (summary.type == "catalog") return "Catalog";
  if (summary.type == "client") return "Client";
  if (summary.type == "console") return "Console";
  if (summary.type == "counter") return "Counter";
  if (summary.type == "device") return "Device";
  if (summary.type == "director") return "Director";
  if (summary.type == "fileset") return "FileSet";
  if (summary.type == "job") return "Job";
  if (summary.type == "jobdefs") return "JobDefs";
  if (summary.type == "messages") return "Messages";
  if (summary.type == "ndmp") return "Ndmp";
  if (summary.type == "pool") return "Pool";
  if (summary.type == "profile") return "Profile";
  if (summary.type == "schedule") return "Schedule";
  if (summary.type == "storage") return "Storage";
  if (summary.type == "user") return "User";
  return "";
}

const ResourceTable* LookupResourceTable(const ResourceSummary& summary)
{
  const auto parser_resource_type = ParserResourceTypeForSummary(summary);
  if (parser_resource_type.empty()) return nullptr;

  const auto binding = GetParserBinding(DetectComponentForPath(summary.file_path));
  if (!binding.parser) return nullptr;
  if (binding.assign_global) { binding.assign_global(binding.parser); }
  return binding.parser->GetResourceTable(parser_resource_type.c_str());
}

const ResourceItem* LookupDirectiveResourceItem(const ResourceSummary& summary,
                                                const ResourceTable* table,
                                                const std::string& key)
{
  if (!table) return nullptr;

  const auto binding = GetParserBinding(DetectComponentForPath(summary.file_path));
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

  const auto binding = GetParserBinding(DetectComponentForPath(summary.file_path));
  if (!binding.parser) return "";
  if (binding.assign_global) { binding.assign_global(binding.parser); }

  const auto* resource_type = binding.parser->ResToStr(item.code);
  if (!resource_type) return "";
  return NormalizeParserResourceType(resource_type);
}

std::vector<std::string> CollectAllowedValues(const ResourceSummary& summary,
                                              const DirectiveMetadata& metadata)
{
  if (metadata.datatype == "BOOLEAN") {
    return {"yes", "no"};
  }

  if (metadata.related_resource_type.empty()) return {};

  const auto config_root = FindConfigRootForPath(summary.file_path);
  if (config_root.empty()) return {};

  const auto source_component = DetectComponentForPath(summary.file_path);
  const auto datacenter = DiscoverDatacenterSummary({config_root});

  std::set<std::string> values;
  auto collect = [&](const std::vector<ResourceSummary>& resources) {
    for (const auto& resource : resources) {
      if (DetectComponentForPath(resource.file_path) != source_component) continue;
      if (resource.type != metadata.related_resource_type) continue;
      if (resource.name.empty()) continue;
      values.insert(resource.name);
    }
  };

  for (const auto& director : datacenter.directors) {
    collect(director.resources);
    for (const auto& daemon : director.daemons) {
      collect(daemon.resources);
    }
  }

  return {values.begin(), values.end()};
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
                                    item.type == CFG_TYPE_ALIST_RES
                                        || item.type == CFG_TYPE_ALIST_STR
                                        || item.type == CFG_TYPE_ALIST_DIR
                                        || item.type == CFG_TYPE_ACL
                                        || item.type == CFG_TYPE_MSGS
                                        || item.type == CFG_TYPE_RUN
                                        || item.type == CFG_TYPE_STR_VECTOR
                                        || item.type == CFG_TYPE_STR_VECTOR_OF_DIRS,
                                    item.is_deprecated};
    metadata.emplace(entry.key, std::move(entry));
  }

  return metadata;
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

std::string ExtractDirectiveFromNamedBlock(const std::filesystem::path& path,
                                           const std::string& block_name,
                                           const std::string& directive_key)
{
  std::ifstream input(path);
  if (!input) return "";

  std::string line;
  bool in_block = false;
  while (std::getline(input, line)) {
    auto trimmed = TrimWhitespace(line);
    if (!in_block) {
      if (trimmed.rfind(block_name, 0) == 0
          && trimmed.find('{') != std::string::npos) {
        in_block = true;
      }
      continue;
    }

    if (trimmed == "}") break;
    const auto separator = trimmed.find('=');
    if (separator == std::string::npos) continue;

    const auto key = TrimWhitespace(trimmed.substr(0, separator));
    if (key != directive_key) continue;

    return TrimWhitespace(trimmed.substr(separator + 1));
  }

  return "";
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

size_t CountChar(const std::string& text, char needle)
{
  return static_cast<size_t>(
      std::count(text.begin(), text.end(), needle));
}

struct ParsedResourceBlock {
  std::string name;
  std::string content;
};

std::vector<ParsedResourceBlock> ParseNamedResourceBlocks(
    const std::filesystem::path& path, const std::string& block_name)
{
  std::ifstream input(path);
  if (!input) return {};

  std::vector<ParsedResourceBlock> blocks;
  std::string line;
  bool in_block = false;
  size_t brace_depth = 0;
  std::ostringstream content;
  std::string current_name;

  while (std::getline(input, line)) {
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
      brace_depth = CountChar(parsed_line, '{') - CountChar(parsed_line, '}');
      content.str("");
      content.clear();
      content << line << "\n";
      current_name.clear();
      continue;
    }

    content << line << "\n";

    const auto separator = trimmed.find('=');
    if (separator != std::string::npos) {
      const auto key = TrimWhitespace(trimmed.substr(0, separator));
      if (key == "Name" && current_name.empty()) {
        current_name = TrimWhitespace(trimmed.substr(separator + 1));
      }
    }

    brace_depth += CountChar(parsed_line, '{');
    brace_depth -= CountChar(parsed_line, '}');
    if (brace_depth == 0) {
      blocks.push_back({current_name, content.str()});
      in_block = false;
    }
  }

  return blocks;
}

std::vector<ResourceDirective> ParseResourceDirectives(
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

  const auto config_root = FindConfigRootForPath(summary.file_path);
  const auto jobdefs_dir = config_root / "bareos-dir.d" / "jobdefs";
  if (jobdefs_dir.empty() || !std::filesystem::exists(jobdefs_dir)) return {};

  const auto normalized_jobdefs_name
      = NormalizeDirectiveNameForComparison(inherited_jobdefs_name);
  for (const auto& entry : std::filesystem::recursive_directory_iterator(jobdefs_dir)) {
    if (!entry.is_regular_file() || entry.path().extension() != ".conf") continue;

    const auto blocks = ParseNamedResourceBlocks(entry.path(), "JobDefs");
    for (const auto& block : blocks) {
      if (NormalizeDirectiveNameForComparison(
              StripSurroundingQuotes(block.name))
          != normalized_jobdefs_name) {
        continue;
      }

      std::vector<InheritedDirective> inherited_directives;
      for (const auto& inherited_directive : ParseResourceDirectives(block.content)) {
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
            StripSurroundingQuotes(block.name),
            entry.path().string(),
        });
      }
      return inherited_directives;
    }
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
    if (present && !hint.allowed_values.empty()
        && std::find(hint.allowed_values.begin(), hint.allowed_values.end(),
                     hint.value)
               == hint.allowed_values.end()) {
      hint.allowed_values.insert(hint.allowed_values.begin(), hint.value);
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
    const std::filesystem::path& path, const std::string& id_prefix)
{
  std::vector<ResourceSummary> resources;
  if (!std::filesystem::exists(path)) return resources;

  size_t resource_index = 0;
  for (const auto& entry : std::filesystem::recursive_directory_iterator(path)) {
    if (!entry.is_regular_file() || entry.path().extension() != ".conf") continue;

    const auto type = entry.path().parent_path().filename().string();
    const auto block_name = ResourceBlockNameForTypeImpl(type);
    if (!block_name.empty()) {
      const auto parsed_blocks = ParseNamedResourceBlocks(entry.path(), block_name);
      if (!parsed_blocks.empty()) {
        const auto fallback_stem = StemWithoutExtension(entry.path());
        for (size_t block_index = 0; block_index < parsed_blocks.size();
             ++block_index) {
          auto name = parsed_blocks[block_index].name;
          if (name.empty()) {
            name = parsed_blocks.size() == 1
                       ? fallback_stem
                       : fallback_stem + "-" + std::to_string(block_index);
          }
          resources.push_back({
              id_prefix + "-" + std::to_string(resource_index++),
              type.empty() ? "resource" : type,
              name,
              entry.path().string(),
          });
        }
        continue;
      }
    }

    auto name = StemWithoutExtension(entry.path());
    resources.push_back({
        id_prefix + "-" + std::to_string(resource_index++),
        type.empty() ? "resource" : type,
        name,
        entry.path().string(),
    });
  }

  std::sort(resources.begin(), resources.end(),
            [](const ResourceSummary& lhs, const ResourceSummary& rhs) {
              if (lhs.type != rhs.type) return lhs.type < rhs.type;
              return lhs.name < rhs.name;
            });
  return resources;
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
  std::string block_name;
  std::string resource_type;
  std::string resource_block;
  if (kind == "file-daemon") {
    block_name = "FileDaemon";
    resource_type = "client";
    resource_block = "Client";
  } else if (kind == "storage-daemon") {
    block_name = "Storage";
    resource_type = "storage";
    resource_block = "Storage";
  } else if (kind == "console") {
    block_name = "Console";
    resource_type = "console";
    resource_block = "Console";
  } else {
    return "";
  }

  if (std::filesystem::exists(daemon_conf)) {
    const auto configured_name
        = ExtractDirectiveFromNamedBlock(daemon_conf, block_name, "Name");
    if (!configured_name.empty()) return configured_name;
  }

  for (const auto& resource : resources) {
    if (resource.type != resource_type) continue;
    const auto configured_name
        = ExtractDirectiveFromNamedBlock(resource.file_path, resource_block, "Name");
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
      conf_dir, "resource-" + std::to_string(director_index) + "-" + kind);
  const auto daemon_conf = root / (daemon_name + ".conf");
  const auto configured_name
      = ConfiguredNameForDaemon(kind, daemon_conf, resources);
  if (std::filesystem::exists(daemon_conf)) {
    resources.insert(resources.begin(),
                     {"resource-" + std::to_string(director_index) + "-" + kind
                          + "-main",
                       "configuration", daemon_conf.filename().string(),
                       daemon_conf.string()});
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
    const auto director_name = ExtractDirectiveFromNamedBlock(
        resource.file_path, "Director", "Name");
    if (!director_name.empty()) return StripSurroundingQuotes(director_name);
  }

  const auto configuration = std::find_if(
      console_daemon.resources.begin(), console_daemon.resources.end(),
      [](const ResourceSummary& resource) { return resource.type == "configuration"; });
  if (configuration == console_daemon.resources.end()) return "";

  return StripSurroundingQuotes(ExtractDirectiveFromNamedBlock(
      configuration->file_path, "Director", "Name"));
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

    for (const auto& block_name : std::array<std::string, 3>{"Director", "Console",
                                                             "User"}) {
      const auto blocks = ParseNamedResourceBlocks(entry.resource->file_path, block_name);
      for (size_t block_index = 0; block_index < blocks.size(); ++block_index) {
        auto summary = *entry.resource;
        summary.type = Lowercase(block_name);
        if (blocks[block_index].name.empty()) {
          summary.name = Lowercase(block_name) + "-" + std::to_string(block_index);
        } else {
          summary.name = StripSurroundingQuotes(blocks[block_index].name);
        }
        auto detail = BuildResourceDetail(summary, blocks[block_index].content);
        subjects.push_back({std::move(summary),
                            *entry.resource,
                            std::move(detail),
                            entry.owner_node_id,
                            entry.owner_label,
                            entry.owner_kind});
      }
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
                ExtractDirectiveFromNamedBlock(target_resource->file_path, "Director",
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
            ExtractDirectiveFromNamedBlock(target_resource->file_path, "Console",
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

        std::vector<const RelationshipResourceEntry*> matches;
        for (const auto& entry : resource_entries) {
          if (!entry.resource) continue;
          if (entry.resource->type != metadata_it->second.related_resource_type) continue;
          if (subject.owner_kind == "director"
              && (metadata_it->second.related_resource_type == "client"
                  || metadata_it->second.related_resource_type == "storage")
              && entry.owner_kind != "director") {
            continue;
          }
          if (IsOwnerScopedRelationshipType(metadata_it->second.related_resource_type)
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

        if (matches.empty()) continue;

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
        director_dir, "resource-" + std::to_string(director_index) + "-director");

    if (std::filesystem::exists(director_conf)) {
      director.resources.insert(
          director.resources.begin(),
          {"resource-" + std::to_string(director_index) + "-director-main",
           "configuration", director_conf.filename().string(),
           director_conf.string()});
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
  const auto all_relationships = BuildRelationshipSummaries(summary);
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
  const auto block_name = ResourceBlockNameForType(resource.type);
  if (!block_name.empty()) {
    const auto parsed_blocks
        = ParseNamedResourceBlocks(resource.file_path, block_name);
    const auto it = std::find_if(
        parsed_blocks.begin(), parsed_blocks.end(),
        [&resource](const ParsedResourceBlock& block) {
          return block.name == resource.name;
        });
    if (it != parsed_blocks.end()) {
      return BuildResourceDetail(resource, it->content);
    }
  }

  std::ifstream input(resource.file_path);
  std::ostringstream content;
  content << input.rdbuf();
  return BuildResourceDetail(resource, content.str());
}

ResourceDetail BuildResourceDetail(const ResourceSummary& resource,
                                   const std::string& content)
{
  const auto parsed_directives = ParseResourceDirectives(content);
  const auto inherited_directives
      = ResolveInheritedDirectives(resource, parsed_directives);
  auto display_directives = parsed_directives;
  SortResourceDirectivesForDisplay(resource, display_directives);
  return {resource, content, display_directives, inherited_directives,
          ValidateResourceContent(resource, content, parsed_directives,
                                  inherited_directives),
          BuildResourceFieldHints(resource, parsed_directives,
                                  inherited_directives)};
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
  const auto parsed_updated_directives = ParseResourceDirectives(updated_content);
  const auto updated_inherited_directives
      = ResolveInheritedDirectives(resource.summary, parsed_updated_directives);
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
