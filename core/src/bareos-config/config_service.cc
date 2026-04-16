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
#include <filesystem>
#include <map>
#include <sstream>
#include <string>

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
      field.value = value;
      field.present = !value.empty();
      continue;
    }

    field_hints.push_back(
        {key, key, false, false, false, !value.empty(), value, "", "", "", {}});
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

  return {preview.summary, preview.updated_content, preview.updated_directives,
          preview.updated_validation_messages, preview.updated_field_hints};
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
    const TreeNodeSummary& director_node)
{
  const auto config_root = std::filesystem::path(director_node.description);
  const auto local_fd_conf = config_root / "bareos-fd.conf";
  const auto local_fd_dir = config_root / "bareos-fd.d";
  const auto local_target
      = local_fd_dir / "director" / (director_node.label + ".conf");

  if (std::filesystem::exists(local_fd_conf)
      || std::filesystem::exists(local_fd_dir)) {
    const bool target_exists = std::filesystem::exists(local_target);
    return {local_target.string(), target_exists ? "update" : "create",
            target_exists};
  }

  return {"client:/etc/bareos/bareos-fd.d/director/" + director_node.label
              + ".conf",
          "create-or-update", false};
}

ResourceDetail BuildAddClientFileDaemonPreview(
    const TreeNodeSummary& director_node,
    const std::map<std::string, std::string>& values)
{
  const auto target_plan = BuildClientFileDaemonTargetPlan(director_node);

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
    const TreeNodeSummary& director_node,
    const std::map<std::string, std::string>& values)
{
  const auto director_preview = BuildAddClientDirectorPreview(director_node, values);
  const auto file_daemon_preview
      = BuildAddClientFileDaemonPreview(director_node, values);
  const auto file_daemon_target_plan
      = BuildClientFileDaemonTargetPlan(director_node);
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

    function renderDirectives(directives) {
      if (!directives || !directives.length) {
        return '<div class="muted">No directive fields parsed yet.</div>';
      }

      return `<ul class="directive-list">${directives.map((directive) => `
        <li style="margin-left: ${Math.max((directive.nesting_level || 1) - 1, 0) * 16}px;"><strong>${escapeHtml(directive.key)}</strong> = ${escapeHtml(directive.value)}
          <small class="muted">line ${directive.line}${directive.nesting_level ? `, level ${directive.nesting_level}` : ''}</small></li>`).join('')}</ul>`;
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

    function renderWizardPreview(preview, datacenterNode, directorNode, relationships) {
      const resourcesHtml = (preview.resources || []).map((entry) => `
        <section style="margin-bottom: 24px;">
          <h4>${escapeHtml(entry.role)}</h4>
          <div class="muted">Target file: ${escapeHtml(entry.resource.file_path)}</div>
          <h5>Parsed fields</h5>
          <div>${renderDirectives(entry.resource.directives)}</div>
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
        ${resourcesHtml}
        <div><button id="back-to-wizard">Back to add client wizard</button></div>`;

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
            renderWizardPreview(preview, node, currentDirector, currentRelationships);
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

    function renderFieldHints(fieldHints) {
      if (!fieldHints || !fieldHints.length) {
        return '<div class="muted">No guided fields available for this resource type yet.</div>';
      }

      return `<div class="field-hint-list">${fieldHints.map((field, index) => `
        <label class="field-hint-row">
          <span><strong>${escapeHtml(field.label)}</strong> <small class="muted">${escapeHtml(field.key)}${field.required ? ' · required' : ''}${field.repeatable ? ' · repeatable' : ''}${field.deprecated ? ' · deprecated' : ''}${field.datatype ? ` · ${escapeHtml(field.datatype)}` : ''}${field.related_resource_type ? ` · ${escapeHtml(field.related_resource_type)} reference` : ''}</small>${field.description ? `<br><small class="muted">${escapeHtml(field.description)}</small>` : ''}</span>
          ${field.allowed_values && field.allowed_values.length
            ? (field.datatype === 'BOOLEAN'
              ? `<select
                  data-field-key="${escapeHtml(field.key)}"
                  data-field-required="${field.required ? 'true' : 'false'}"
                >${field.allowed_values.map((value) => `<option value="${escapeHtml(value)}"${value === (field.value || '') ? ' selected' : ''}>${escapeHtml(value)}</option>`).join('')}</select>`
              : `<div>
                  <input
                    list="field-hints-${index}"
                    data-field-key="${escapeHtml(field.key)}"
                    data-field-required="${field.required ? 'true' : 'false'}"
                    value="${escapeHtml(field.value || '')}"
                  >
                  <datalist id="field-hints-${index}">${field.allowed_values.map((value) => `<option value="${escapeHtml(value)}"></option>`).join('')}</datalist>
                </div>`)
            : `<input
                data-field-key="${escapeHtml(field.key)}"
                data-field-required="${field.required ? 'true' : 'false'}"
                value="${escapeHtml(field.value || '')}"
              >`}
        </label>`).join('')}</div>`;
    }

    function serializeFieldHints(fieldHintsEl) {
      return Array.from(fieldHintsEl.querySelectorAll('[data-field-key]'))
        .map((field) => `${field.dataset.fieldKey}=${field.value}`)
        .join('\n');
    }

    function renderEditableResource(resource, previewUrl, previewFieldsUrl) {
      detailsEl.innerHTML = `<strong>${resource.name}</strong>
        <div class="muted">Type: ${resource.type}<br>File: ${resource.file_path}<br>Mode: ${resource.save_mode}</div>
        <h4>Guided fields</h4>
        <div id="resource-fields">${renderFieldHints(resource.field_hints)}</div>
        <div><button id="apply-fields">Apply fields to raw config</button></div>
        <h4>Parsed fields</h4>
        <div id="resource-directives">${renderDirectives(resource.directives)}</div>
        <h4>Validation</h4>
        <div id="resource-validation">${renderValidation(resource.validation_messages)}</div>
        <h4>Raw config</h4>
        <textarea id="resource-editor">${escapeHtml(resource.content)}</textarea>
        <div><button id="preview-changes">Preview changes</button></div>
        <div id="preview-status" class="muted" style="margin-top: 12px;"></div>
        <pre id="preview-output"></pre>`;

      const editorEl = document.getElementById('resource-editor');
      const previewButton = document.getElementById('preview-changes');
      const previewStatusEl = document.getElementById('preview-status');
      const previewOutputEl = document.getElementById('preview-output');
      const fieldHintsEl = document.getElementById('resource-fields');
      const applyFieldsButton = document.getElementById('apply-fields');
      const directivesEl = document.getElementById('resource-directives');
      const validationEl = document.getElementById('resource-validation');
      previewOutputEl.textContent = resource.content;
      previewStatusEl.textContent = 'Editing is currently preview-only; writeback is not implemented yet.';

      applyFieldsButton.addEventListener('click', () => {
        applyFieldsButton.disabled = true;
        previewStatusEl.textContent = 'Generating field-based dry-run preview...';
        fetch(previewFieldsUrl, {
          method: 'POST',
          headers: { 'Content-Type': 'text/plain; charset=utf-8' },
          body: serializeFieldHints(fieldHintsEl),
        })
          .then((response) => response.json())
          .then((preview) => {
            editorEl.value = preview.updated_content;
            fieldHintsEl.innerHTML = renderFieldHints(preview.updated_field_hints);
            directivesEl.innerHTML = renderDirectives(preview.updated_directives);
            validationEl.innerHTML = renderValidation(preview.updated_validation_messages);
            previewOutputEl.textContent = preview.updated_content;
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
            previewStatusEl.textContent =
              `${preview.changed ? 'Changes detected' : 'No changes detected'} — ` +
              `${preview.original_line_count} -> ${preview.updated_line_count} lines`;
            fieldHintsEl.innerHTML = renderFieldHints(preview.updated_field_hints);
            directivesEl.innerHTML = renderDirectives(preview.updated_directives);
            validationEl.innerHTML = renderValidation(preview.updated_validation_messages);
            previewOutputEl.textContent = preview.updated_content;
          })
          .catch((error) => {
            previewStatusEl.textContent = `Failed to preview resource: ${error}`;
          })
          .finally(() => {
            previewButton.disabled = false;
          });
      });
    }

    function showResourceDetails(resourceId) {
      fetch(`/api/v1/resources/${resourceId}/editor`)
        .then((response) => response.json())
        .then((resource) => {
          renderEditableResource(
            resource,
            `/api/v1/resources/${resource.id}/preview`,
            `/api/v1/resources/${resource.id}/preview-fields`,
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
        resources.map((resource) => `
        <div class="resource" data-id="${resource.id}">
          ${resource.name}
          <small>${resource.type}</small>
        </div>`).join('');

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
            <h4>Relationships</h4>
            <div>${renderRelationships(relationships)}</div>`;
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
            SerializeResourceSummaries(node->resources)};
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
            SerializeResourceEditPreview(
                BuildResourceEditPreview(detail, request.body))};
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
            SerializeResourceEditPreview(BuildFieldHintEditPreview(
                detail,
                ParseFieldHintUpdateBody(request.body, detail.field_hints)))};
  }

  if (request.method == "GET" && path_parts.size() == 4
      && path_parts[0] == "api" && path_parts[1] == "v1"
      && path_parts[2] == "resources") {
    const auto model = DiscoverDatacenterSummary(options.config_roots);
    const auto* resource = FindResourceById(model, path_parts[3]);
    if (!resource) return {404, "text/plain; charset=utf-8", "Resource Not Found"};
    return {200, "application/json; charset=utf-8",
            SerializeResourceDetail(LoadResourceDetail(*resource))};
  }

  if (request.method == "GET" && path_parts.size() == 5
      && path_parts[0] == "api" && path_parts[1] == "v1"
      && path_parts[2] == "resources" && path_parts[4] == "editor") {
    const auto model = DiscoverDatacenterSummary(options.config_roots);
    const auto* resource = FindResourceById(model, path_parts[3]);
    if (!resource) return {404, "text/plain; charset=utf-8", "Resource Not Found"};
    return {200, "application/json; charset=utf-8",
            SerializeEditableResourceDetail(LoadResourceDetail(*resource))};
  }

  if (request.method == "POST" && path_parts.size() == 5
      && path_parts[0] == "api" && path_parts[1] == "v1"
      && path_parts[2] == "resources" && path_parts[4] == "preview") {
    const auto model = DiscoverDatacenterSummary(options.config_roots);
    const auto* resource = FindResourceById(model, path_parts[3]);
    if (!resource) return {404, "text/plain; charset=utf-8", "Resource Not Found"};

    const auto detail = LoadResourceDetail(*resource);
    return {200, "application/json; charset=utf-8",
            SerializeResourceEditPreview(
                BuildResourceEditPreview(detail, request.body))};
  }

  if (request.method == "POST" && path_parts.size() == 5
      && path_parts[0] == "api" && path_parts[1] == "v1"
      && path_parts[2] == "resources" && path_parts[4] == "preview-fields") {
    const auto model = DiscoverDatacenterSummary(options.config_roots);
    const auto* resource = FindResourceById(model, path_parts[3]);
    if (!resource) return {404, "text/plain; charset=utf-8", "Resource Not Found"};

    const auto detail = LoadResourceDetail(*resource);
    return {200, "application/json; charset=utf-8",
            SerializeResourceEditPreview(BuildFieldHintEditPreview(
                detail,
                ParseFieldHintUpdateBody(request.body, detail.field_hints)))};
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
            SerializeAddClientWizardPreview(*node, ParseKeyValueBody(request.body))};
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
            SerializeAddClientWizardPreview(*director, ParseKeyValueBody(request.body))};
  }

  return {404, "text/plain; charset=utf-8", "Not Found"};
}
