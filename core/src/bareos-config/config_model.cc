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
  std::string related_resource_type;
  bool required = false;
  bool repeatable = false;
  bool deprecated = false;
};

std::map<std::string, DirectiveMetadata> BuildDirectiveMetadata(
    const ResourceSummary& summary);
const ResourceTable* LookupResourceTable(const ResourceSummary& summary);
const ResourceItem* LookupDirectiveResourceItem(const ResourceSummary& summary,
                                                const ResourceTable* table,
                                                const std::string& key);

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

bool IsTopLevelDirective(const ResourceDirective& directive)
{
  return directive.nesting_level == 1;
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
  for (auto current = path; !current.empty(); current = current.parent_path()) {
    const auto name = current.filename().string();
    if (name == "bareos-dir.d" || name == "bareos-fd.d" || name == "bareos-sd.d") {
      return current.parent_path();
    }
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
                                   RelatedResourceType(summary, item),
                                   item.is_required,
                                   item.type == CFG_TYPE_ALIST_RES
                                       || item.type == CFG_TYPE_ALIST_STR
                                       || item.type == CFG_TYPE_ALIST_DIR
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

std::vector<ValidationMessage> ValidateResourceContent(
    const ResourceSummary& summary,
    const std::string& content,
    const std::vector<ResourceDirective>& directives)
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
    if (top_level) { present_keys.insert(canonical_key); }

    if (directive.value.empty()) {
      messages.push_back({"warning", "empty-value",
                          "Directive has an empty value.", directive.line});
    }

    if (!top_level) continue;

    const auto insert_result
        = first_occurrence.emplace(canonical_key, directive.line);
    if (!insert_result.second) {
      messages.push_back({"warning", "duplicate-directive",
                          "Directive appears multiple times in this resource.",
                          directive.line});
    }
  }

  for (const auto& [key, metadata] : directive_metadata) {
    if (!metadata.required || present_keys.contains(key)) continue;

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
    const std::vector<ResourceDirective>& directives)
{
  const auto* table = LookupResourceTable(summary);
  const auto directive_metadata = BuildDirectiveMetadata(summary);

  std::map<std::string, std::string> first_values;
  std::map<std::string, size_t> counts;
  for (const auto& directive : directives) {
    if (!IsTopLevelDirective(directive)) continue;
    auto canonical_key = directive.key;
    if (const auto* item = LookupDirectiveResourceItem(summary, table, directive.key)) {
      canonical_key = item->name;
    }
    ++counts[canonical_key];
    first_values.emplace(canonical_key, directive.value);
  }

  std::vector<ResourceFieldHint> hints;
  std::map<std::string, bool> known_keys;
  for (const auto& [key, metadata] : directive_metadata) {
    const auto value_it = first_values.find(key);
    const bool present = value_it != first_values.end();
    const auto value = present ? value_it->second : "";
    auto hint = ResourceFieldHint{key,
                                  key,
                                  metadata.required,
                                  metadata.repeatable || counts[key] > 1,
                                  metadata.deprecated,
                                  present,
                                  metadata.datatype == "BOOLEAN"
                                      ? NormalizeBooleanValue(value)
                                      : value,
                                  metadata.datatype,
                                  metadata.description,
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
                     counts[canonical_key] > 1, false, true, directive.value, "",
                     "", "", {}});
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
  std::map<std::string, size_t> directive_index_by_key;
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
    if (!directive_index_by_key.contains(key)) {
      directive_index_by_key.emplace(key, i);
    }
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
    for (auto& [key, index] : directive_index_by_key) {
      if (index > removed_index) --index;
    }
  };
  auto adjust_indices_after_insert = [&](size_t inserted_index) {
    for (auto& [key, index] : directive_index_by_key) {
      if (index >= inserted_index) ++index;
    }
  };

  for (const auto& field : updated_field_hints) {
    const auto desired_value = TrimWhitespace(field.value);
    const auto directive_it = directive_index_by_key.find(field.key);
    if (directive_it != directive_index_by_key.end()) {
      const auto line_index = directive_it->second;
      const auto rendered_key_it = rendered_key_by_canonical_key.find(field.key);
      const auto& rendered_key
          = rendered_key_it != rendered_key_by_canonical_key.end()
                ? rendered_key_it->second
                : field.key;
      const auto replacement = "  " + rendered_key + " = " + desired_value;
      if (!desired_value.empty() || field.required) {
        lines[line_index] = replacement;
      } else {
        lines.erase(lines.begin() + static_cast<std::ptrdiff_t>(line_index));
        directive_index_by_key.erase(directive_it);
        rendered_key_by_canonical_key.erase(field.key);
        adjust_indices_after_removal(line_index);
        if (closing_index > line_index) --closing_index;
      }
      continue;
    }

    if (desired_value.empty() && !field.required) continue;
    const auto replacement = "  " + field.key + " = " + desired_value;
    const auto insert_index = std::min(closing_index, lines.size());
    adjust_indices_after_insert(insert_index);
    lines.insert(lines.begin() + static_cast<std::ptrdiff_t>(insert_index),
                 replacement);
    directive_index_by_key[field.key] = insert_index;
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
  const auto block_name = kind == "file-daemon" ? "FileDaemon" : "Storage";
  if (std::filesystem::exists(daemon_conf)) {
    const auto configured_name
        = ExtractDirectiveFromNamedBlock(daemon_conf, block_name, "Name");
    if (!configured_name.empty()) return configured_name;
  }

  const auto resource_type = kind == "file-daemon" ? "client" : "storage";
  const auto resource_block = kind == "file-daemon" ? "Client" : "Storage";
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
           << "\"value\":" << JsonString(field.value) << ","
           << "\"datatype\":" << JsonString(field.datatype) << ","
           << "\"description\":" << JsonString(field.description) << ","
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

std::vector<RelationshipSummary> BuildRelationshipSummaries(
    const DatacenterSummary& summary)
{
  std::vector<RelationshipSummary> relationships;
  for (size_t director_index = 0; director_index < summary.directors.size();
       ++director_index) {
    const auto& director = summary.directors[director_index];
    const auto* file_daemon = FindDirectorDaemonByKind(director, "file-daemon");
    const auto* storage_daemon
        = FindDirectorDaemonByKind(director, "storage-daemon");
    size_t relationship_index = 0;

    for (const auto& resource : director.resources) {
      const bool is_client = resource.type == "client";
      const bool is_storage = resource.type == "storage";
      if (!is_client && !is_storage) continue;

      const auto* target = is_client ? file_daemon : storage_daemon;
      std::string to_node_id;
      std::string to_label;
      std::string resolution;
      bool resolved = false;

      if (!target) {
        to_label = is_client ? "unresolved file-daemon"
                             : "unresolved storage-daemon";
        resolution = is_client
                         ? "No parsed local file-daemon node is available for this client reference yet."
                         : "No parsed local storage-daemon node is available for this storage reference yet.";
      } else if (target->configured_name.empty()) {
        to_node_id = target->id;
        to_label = target->name;
        resolved = true;
        resolution = "Parsed local " + target->kind + " node " + target->name
                     + " is linked, but no configured Name directive was found to compare with endpoint "
                     + resource.name + ".";
      } else if (target->configured_name != resource.name) {
        to_node_id = target->id;
        to_label = target->name;
        resolved = true;
        resolution = "Resolved to parsed local " + target->kind + " node "
                     + target->name + ", but its configured name "
                     + target->configured_name + " does not match endpoint "
                     + resource.name + ".";
      } else {
        to_node_id = target->id;
        to_label = target->name;
        resolved = true;
        resolution = "Resolved to parsed local " + target->kind + " node "
                     + target->name + " with configured name "
                     + target->configured_name + ".";
      }

      relationships.push_back({
          MakeRelationshipId(director_index, relationship_index++),
          is_client ? "client" : "storage",
          resource.name,
          director.id,
          director.name,
          to_node_id,
          to_label,
          resource.id,
          resource.file_path,
          resolved,
          resolution,
      });
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
  auto display_directives = parsed_directives;
  SortResourceDirectivesForDisplay(resource, display_directives);
  return {resource, content, display_directives,
          ValidateResourceContent(resource, content, parsed_directives),
          BuildResourceFieldHints(resource, parsed_directives)};
}

ResourceEditPreview BuildResourceEditPreview(
    const ResourceDetail& resource, const std::string& updated_content)
{
  const auto parsed_updated_directives = ParseResourceDirectives(updated_content);
  auto display_updated_directives = parsed_updated_directives;
  SortResourceDirectivesForDisplay(resource.summary, display_updated_directives);
  return {resource.summary,
          resource.content,
          updated_content,
          resource.directives,
          display_updated_directives,
          resource.validation_messages,
          ValidateResourceContent(resource.summary, updated_content,
                                  parsed_updated_directives),
          resource.field_hints,
          BuildResourceFieldHints(resource.summary, parsed_updated_directives),
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
