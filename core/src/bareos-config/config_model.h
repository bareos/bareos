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
#ifndef BAREOS_BAREOS_CONFIG_CONFIG_MODEL_H_
#define BAREOS_BAREOS_CONFIG_CONFIG_MODEL_H_

#include <filesystem>
#include <string>
#include <vector>

struct ResourceSummary {
  std::string id;
  std::string type;
  std::string name;
  std::string file_path;
};

struct ResourceDirective {
  std::string key;
  std::string value;
  size_t line = 0;
  size_t nesting_level = 0;
};

struct ValidationMessage {
  std::string level;
  std::string code;
  std::string message;
  size_t line = 0;
};

struct ResourceFieldHint {
  std::string key;
  std::string label;
  bool required = false;
  bool repeatable = false;
  bool deprecated = false;
  bool present = false;
  bool inherited = false;
  std::string value;
  std::string inherited_value;
  std::string datatype;
  std::string description;
  std::string default_value;
  std::string inherited_source_resource_type;
  std::string inherited_source_resource_name;
  std::string related_resource_type;
  std::vector<std::string> allowed_values;
};

struct InheritedDirective {
  ResourceDirective directive;
  std::string source_resource_type;
  std::string source_resource_name;
  std::string source_resource_path;
};

struct ResourceDetail {
  ResourceSummary summary;
  std::string content;
  std::vector<ResourceDirective> directives;
  std::vector<InheritedDirective> inherited_directives;
  std::vector<ValidationMessage> validation_messages;
  std::vector<ResourceFieldHint> field_hints;
};

struct ResourceEditPreview {
  ResourceSummary summary;
  std::string original_content;
  std::string updated_content;
  std::vector<ResourceDirective> original_directives;
  std::vector<ResourceDirective> updated_directives;
  std::vector<InheritedDirective> original_inherited_directives;
  std::vector<InheritedDirective> updated_inherited_directives;
  std::vector<ValidationMessage> original_validation_messages;
  std::vector<ValidationMessage> updated_validation_messages;
  std::vector<ResourceFieldHint> original_field_hints;
  std::vector<ResourceFieldHint> updated_field_hints;
  bool changed = false;
  size_t original_line_count = 0;
  size_t updated_line_count = 0;
};

struct DaemonSummary {
  std::string id;
  std::string kind;
  std::string name;
  std::string configured_name;
  std::string config_root;
  std::vector<ResourceSummary> resources;
};

struct TreeNodeSummary {
  std::string id;
  std::string kind;
  std::string label;
  std::string description;
  std::vector<ResourceSummary> resources;
  std::vector<TreeNodeSummary> children;
};

struct RelationshipSummary {
  std::string id;
  std::string relation;
  std::string endpoint_name;
  std::string from_node_id;
  std::string from_label;
  std::string to_node_id;
  std::string to_label;
  std::string source_resource_id;
  std::string source_resource_path;
  std::string source_resource_type;
  std::string source_resource_name;
  std::string target_resource_id;
  std::string target_resource_path;
  std::string target_resource_type;
  std::string target_resource_name;
  bool resolved = false;
  std::string resolution;
};

struct DirectorSummary {
  std::string id;
  std::string name;
  std::string config_root;
  std::vector<ResourceSummary> resources;
  std::vector<DaemonSummary> daemons;
};

struct DatacenterSummary {
  std::string id = "datacenter";
  std::string name = "Datacenter";
  std::vector<std::string> config_roots;
  std::vector<DirectorSummary> directors;
  TreeNodeSummary tree{"datacenter", "datacenter", "Datacenter", "", {}, {}};
};

std::vector<std::filesystem::path> DefaultConfigRoots();
DatacenterSummary DiscoverDatacenterSummary(
    const std::vector<std::filesystem::path>& config_roots);
std::string SerializeDatacenterSummary(const DatacenterSummary& summary);
std::string SerializeTreeNodeSummary(const TreeNodeSummary& node);
std::string SerializeResourceSummaries(
    const std::vector<ResourceSummary>& resources);
std::string SerializeRelationshipSummaries(
    const std::vector<RelationshipSummary>& relationships);
std::string ResourceBlockNameForType(const std::string& type);
std::string SerializeResourceDetail(const ResourceDetail& resource);
std::string SerializeEditableResourceDetail(const ResourceDetail& resource);
std::string SerializeResourceFieldHints(
    const std::vector<ResourceFieldHint>& field_hints);
std::string SerializeResourceEditPreview(const ResourceEditPreview& preview);
std::vector<RelationshipSummary> FindRelationshipsForNode(
    const DatacenterSummary& summary, const std::string& node_id);
const TreeNodeSummary* FindTreeNodeById(const DatacenterSummary& summary,
                                        const std::string& node_id);
const ResourceSummary* FindResourceById(const DatacenterSummary& summary,
                                        const std::string& resource_id);
ResourceDetail BuildResourceDetail(const ResourceSummary& resource,
                                   const std::string& content);
ResourceDetail LoadResourceDetail(const ResourceSummary& resource);
ResourceEditPreview BuildResourceEditPreview(
    const ResourceDetail& resource, const std::string& updated_content);
ResourceEditPreview BuildFieldHintEditPreview(
    const ResourceDetail& resource,
    const std::vector<ResourceFieldHint>& updated_field_hints);

#endif  // BAREOS_BAREOS_CONFIG_CONFIG_MODEL_H_
