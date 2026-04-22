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
#include "include/exit_codes.h"
#include "lib/cli.h"

#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <jansson.h>

#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace bconfig::service {
namespace {

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = net::ip::tcp;

using JsonPtr = std::unique_ptr<json_t, decltype(&json_decref)>;

struct PeerLoadSpec {
  bconfig::Component component;
  std::string path;
};

struct InspectRequestSpec {
  bconfig::Component component;
  std::string path;
  std::vector<PeerLoadSpec> peers{};
};

struct DeploymentConfigSpec {
  bconfig::Component component;
  std::string name;
  std::filesystem::path path;
};

JsonPtr MakeJson(json_t* value) { return JsonPtr(value, &json_decref); }

std::vector<bconfig::Component> SupportedDeploymentInspectComponents()
{
  return {bconfig::Component::kDirector,
          bconfig::Component::kStorage,
          bconfig::Component::kClient};
}

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

const char* kTestUiHtml = R"HTML(
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <title>bconfig-service test UI</title>
  <style>
    body {
      font-family: sans-serif;
      margin: 0;
      padding: 1.5rem;
      background: #f5f7fb;
      color: #1f2937;
    }
    h1, h2 {
      margin-top: 0;
    }
    .layout {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(320px, 1fr));
      gap: 1rem;
    }
    .card {
      background: white;
      border-radius: 10px;
      padding: 1rem;
      box-shadow: 0 1px 3px rgba(0, 0, 0, 0.08);
    }
    label {
      display: block;
      font-weight: 600;
      margin-top: 0.75rem;
      margin-bottom: 0.25rem;
    }
    input, select, button {
      width: 100%;
      box-sizing: border-box;
      padding: 0.55rem 0.7rem;
      border: 1px solid #cbd5e1;
      border-radius: 6px;
      font: inherit;
    }
    button {
      cursor: pointer;
      background: #2563eb;
      color: white;
      border: none;
      margin-top: 1rem;
    }
    button.secondary {
      background: #475569;
    }
    pre {
      background: #0f172a;
      color: #e2e8f0;
      padding: 0.75rem;
      border-radius: 8px;
      overflow: auto;
      min-height: 12rem;
    }
    .actions {
      display: flex;
      gap: 0.75rem;
    }
    .actions button {
      margin-top: 0.75rem;
    }
    .contents-panel {
      min-height: 12rem;
    }
    .contents-empty {
      color: #475569;
      margin: 0;
    }
    .contents-meta {
      margin: 0 0 1rem 0;
      color: #475569;
    }
    .config-list {
      display: grid;
      gap: 0.75rem;
    }
    .config-item {
      border: 1px solid #dbe3ef;
      border-radius: 8px;
      padding: 0.75rem;
      background: #f8fafc;
    }
    .config-item h3 {
      margin: 0 0 0.35rem 0;
      font-size: 1rem;
    }
    .config-item p {
      margin: 0.2rem 0;
      color: #475569;
    }
    .message-list {
      margin: 0.5rem 0 0 1rem;
      padding: 0;
    }
    .message-list li {
      margin: 0.15rem 0;
    }
    .resource-item {
      margin: 0.35rem 0;
    }
    .resource-item details {
      background: #ffffff;
      border: 1px solid #e2e8f0;
      border-radius: 6px;
      padding: 0.45rem 0.6rem;
    }
    .resource-item summary {
      cursor: pointer;
      font-weight: 600;
    }
    .resource-meta {
      margin: 0.35rem 0;
      color: #475569;
    }
    .directive-list {
      margin: 0.35rem 0 0 1rem;
      padding: 0;
    }
    .directive-list li {
      margin: 0.15rem 0;
    }
    .relation-list {
      margin: 0.35rem 0 0 1rem;
      padding: 0;
    }
    .relation-list li {
      margin: 0.15rem 0;
    }
    .job-list {
      display: grid;
      gap: 0.75rem;
    }
    .import-list {
      display: grid;
      gap: 0.75rem;
    }
    .import-item {
      border: 1px solid #dbe3ef;
      border-radius: 8px;
      padding: 0.75rem;
      background: #f8fafc;
    }
    .import-item h3 {
      margin: 0 0 0.35rem 0;
      font-size: 1rem;
    }
    .import-item p {
      margin: 0.2rem 0;
      color: #475569;
    }
    .job-item {
      border: 1px solid #dbe3ef;
      border-radius: 8px;
      padding: 0.75rem;
      background: #f8fafc;
    }
    .job-item h3 {
      margin: 0 0 0.35rem 0;
      font-size: 1rem;
    }
    .job-item p {
      margin: 0.2rem 0;
      color: #475569;
    }
    .job-logs {
      margin: 0.5rem 0 0 1rem;
      padding: 0;
    }
    .job-logs li {
      margin: 0.15rem 0;
    }
    .resource-list {
      margin: 0.5rem 0 0 1rem;
      padding: 0;
    }
    .resource-list li {
      margin: 0.15rem 0;
    }
  </style>
</head>
<body>
  <h1>bconfig-service test UI</h1>
  <p>Use this page to exercise the service API directly.</p>

  <div class="layout">
    <section class="card">
      <h2>Create deployment</h2>
      <form id="deployment-form">
        <label for="deployment-id">ID</label>
        <input id="deployment-id" name="id" value="prod">

        <label for="deployment-name">Name</label>
        <input id="deployment-name" name="name" value="Production">

        <label for="deployment-repo">Repository path</label>
        <input id="deployment-repo" name="repository_path"
               value="/tmp/bconfig-service-prod">

        <label for="deployment-workflow">Workflow mode</label>
        <select id="deployment-workflow" name="workflow_mode">
          <option value="direct_commit">direct_commit</option>
          <option value="review" selected>review</option>
        </select>

        <button type="submit">POST /v1/deployments</button>
      </form>
    </section>

    <section class="card">
      <h2>Create job</h2>
      <form id="job-form">
        <label for="job-type">Type</label>
        <select id="job-type" name="type">
          <option value="validate_deployment_repo" selected>
            validate_deployment_repo
          </option>
          <option value="import_configuration">
            import_configuration
          </option>
        </select>

        <label for="job-deployment-id">Deployment ID</label>
        <input id="job-deployment-id" name="deployment_id" value="prod">

        <label for="job-source-path">Source Bareos config root</label>
        <input id="job-source-path" name="source_path"
               placeholder="/etc/bareos" autocomplete="off" spellcheck="false">
        <p class="contents-meta">
          Entering a config root here automatically switches the job type to
          <code>import_configuration</code>.
        </p>

        <button type="submit">POST /v1/jobs</button>
      </form>
    </section>

    <section class="card">
      <h2>Schema</h2>
      <form id="schema-form">
        <label for="schema-component">Component</label>
        <select id="schema-component" name="component">
          <option value="director" selected>director</option>
          <option value="storage">storage</option>
          <option value="client">client</option>
          <option value="console">console</option>
        </select>

        <button type="submit">GET /v1/schema/{component}</button>
      </form>
    </section>

    <section class="card">
      <h2>Inspect config</h2>
      <form id="inspect-form">
        <label for="inspect-component">Component</label>
        <select id="inspect-component" name="component">
          <option value="director" selected>director</option>
          <option value="storage">storage</option>
          <option value="client">client</option>
          <option value="console">console</option>
        </select>

        <label for="inspect-path">Config path</label>
        <input id="inspect-path" name="path" value="/etc/bareos/bareos-dir.d/">

        <button type="submit">POST /v1/inspect</button>
      </form>
    </section>

    <section class="card">
      <h2>Inspect deployment</h2>
      <form id="deployment-inspect-form">
        <label for="deployment-inspect-id">Deployment ID</label>
        <input id="deployment-inspect-id" name="deployment_id" value="prod">

        <button type="submit">GET /v1/deployments/{id}/inspect</button>
      </form>
    </section>
  </div>

  <div class="layout" style="margin-top: 1rem;">
    <section class="card">
      <h2>Deployment contents</h2>
      <p class="contents-meta">
        Reads the current deployment repo via
        <code>GET /v1/deployments/{id}/inspect</code>.
      </p>
      <div id="deployment-contents" class="contents-panel">
        <p class="contents-empty">Load a deployment to see its current contents.</p>
      </div>
    </section>

    <section class="card">
      <h2>Deployment jobs</h2>
      <p class="contents-meta">
        Shows recent jobs for the selected deployment.
      </p>
      <div id="deployment-jobs" class="contents-panel">
        <p class="contents-empty">Load a deployment to see its recent jobs.</p>
      </div>
    </section>

    <section class="card">
      <h2>Deployment imports</h2>
      <p class="contents-meta">
        Shows persisted import metadata from
        <code>service/import-state.json</code>.
      </p>
      <div id="deployment-imports" class="contents-panel">
        <p class="contents-empty">Load a deployment to see its imports.</p>
      </div>
    </section>

    <section class="card">
      <h2>API browser</h2>
      <div class="actions">
        <button type="button" class="secondary" id="health-button">
          GET /v1/health
        </button>
        <button type="button" class="secondary" id="deployments-button">
          GET /v1/deployments
        </button>
        <button type="button" class="secondary" id="jobs-button">
          GET /v1/jobs
        </button>
      </div>
      <pre id="response-panel"></pre>
    </section>

    <section class="card">
      <h2>Quick notes</h2>
      <ul>
        <li>Deployments and jobs are persisted in the service state directory.</li>
        <li>Creating a deployment scaffolds the repo layout on disk.</li>
        <li>The current layout uses <code>directors/</code> and
            <code>storages/</code> so one deployment can contain many of each.
        </li>
        <li>Jobs currently execute asynchronously and update their status/logs.</li>
        <li><code>import_configuration</code> scans a Bareos config root like
            <code>/etc/bareos</code>, imports supported component trees it
            finds, and records them in
            <code>service/import-state.json</code>.
        </li>
      </ul>
    </section>
  </div>

  <script>
    const responsePanel = document.getElementById('response-panel');
    const deploymentContentsPanel = document.getElementById('deployment-contents');
    const deploymentJobsPanel = document.getElementById('deployment-jobs');
    const deploymentImportsPanel = document.getElementById('deployment-imports');
    const jobTypeField = document.getElementById('job-type');
    const jobSourcePathField = document.getElementById('job-source-path');

    function escapeHtml(value) {
      return String(value)
        .replaceAll('&', '&amp;')
        .replaceAll('<', '&lt;')
        .replaceAll('>', '&gt;')
        .replaceAll('"', '&quot;');
    }

    function sleep(milliseconds) {
      return new Promise((resolve) => setTimeout(resolve, milliseconds));
    }

    async function request(method, url, body) {
      const options = { method, headers: {} };
      if (body !== undefined) {
        options.headers['Content-Type'] = 'application/json';
        options.body = JSON.stringify(body);
      }

      const response = await fetch(url, options);
      const text = await response.text();
      let payload = text;

      try {
        payload = JSON.stringify(JSON.parse(text), null, 2);
      } catch (_) {}

      responsePanel.textContent =
        `${method} ${url}\nHTTP ${response.status}\n\n${payload}`;

      return { response, text, payload };
    }

    function renderDeploymentContents(document) {
      const configs = document.configs ?? [];
      const deployment = document.deployment;
      if (!configs.length) {
        deploymentContentsPanel.innerHTML = `
          <p class="contents-meta"><strong>${escapeHtml(deployment.id)}</strong>:
          no imported config roots found.</p>`;
        return;
      }

      const configItems = configs.map((config) => {
        const warnings = config.messages?.warnings?.length ?? 0;
        const errors = config.messages?.errors?.length ?? 0;
        const warningItems = (config.messages?.warnings ?? []).map((warning) =>
          `<li>${escapeHtml(warning)}</li>`
        ).join('');
        const errorItems = (config.messages?.errors ?? []).map((error) =>
          `<li>${escapeHtml(error)}</li>`
        ).join('');
        const resources = (config.resources ?? []).map((resource) => {
          const directives = (resource.directives ?? []).map((directive) =>
            `<li>${escapeHtml(directive.name)}</li>`
          ).join('');
          const relations = (resource.relations ?? []).map((relation) =>
            `<li>${escapeHtml(relation.directive)} → <code>${escapeHtml(relation.target_type)}</code>: ${escapeHtml(relation.target_name)}</li>`
          ).join('');
          const externalRelations = (resource.external_relations ?? []).map((relation) =>
            `<li>${escapeHtml(relation.relation)} → <code>${escapeHtml(relation.target_component)}</code>/<code>${escapeHtml(relation.target_type)}</code>: ${escapeHtml(relation.target_name)} (${relation.matched ? 'matched' : 'missing'})</li>`
          ).join('');
          const source = resource.source
            ? `<p class="resource-meta"><strong>Source:</strong> <code>${escapeHtml(resource.source.file)}</code>:${resource.source.line}</p>`
            : '';
          const relationsBlock = relations
            ? `<p class="resource-meta"><strong>Relations</strong></p><ul class="relation-list">${relations}</ul>`
            : '';
          const externalRelationsBlock = externalRelations
            ? `<p class="resource-meta"><strong>External relations</strong></p><ul class="relation-list">${externalRelations}</ul>`
            : '';
          return `
            <li class="resource-item">
              <details>
                <summary><code>${escapeHtml(resource.type)}</code>: ${escapeHtml(resource.name)}</summary>
                ${source}
                <p class="resource-meta">
                  Directives: ${resource.directives?.length ?? 0};
                  Relations: ${resource.relations?.length ?? 0};
                  External relations: ${resource.external_relations?.length ?? 0}
                </p>
                <ul class="directive-list">${directives}</ul>
                ${relationsBlock}
                ${externalRelationsBlock}
              </details>
            </li>`;
        }).join('');
        const warningBlock = warningItems
          ? `<p class="resource-meta"><strong>Warnings</strong></p><ul class="message-list">${warningItems}</ul>`
          : '';
        const errorBlock = errorItems
          ? `<p class="resource-meta"><strong>Errors</strong></p><ul class="message-list">${errorItems}</ul>`
          : '';
        return `
          <div class="config-item">
            <h3>${escapeHtml(config.component)} / ${escapeHtml(config.name)}</h3>
            <p><strong>Path:</strong> <code>${escapeHtml(config.path)}</code></p>
            <p><strong>Parse:</strong> ${config.parse_ok ? 'ok' : 'failed'}; 
               <strong>Resources:</strong> ${config.resources?.length ?? 0};
               <strong>Warnings:</strong> ${warnings};
               <strong>Errors:</strong> ${errors}</p>
            ${warningBlock}
            ${errorBlock}
            <ul class="resource-list">${resources}</ul>
          </div>`;
      }).join('');

      deploymentContentsPanel.innerHTML = `
        <p class="contents-meta"><strong>${escapeHtml(deployment.id)}</strong>:
        ${configs.length} config root(s) in
        <code>${escapeHtml(deployment.repository_path)}</code></p>
        <div class="config-list">${configItems}</div>`;
    }

    function renderDeploymentJobs(deploymentId, jobs) {
      if (!deploymentId) {
        deploymentJobsPanel.innerHTML =
          '<p class="contents-empty">Enter a deployment ID first.</p>';
        return;
      }

      if (!jobs.length) {
        deploymentJobsPanel.innerHTML = `
          <p class="contents-meta"><strong>${escapeHtml(deploymentId)}</strong>:
          no jobs found.</p>`;
        return;
      }

      const items = jobs.map((job) => {
        const logs = (job.logs ?? []).map((entry) =>
          `<li>${escapeHtml(entry)}</li>`
        ).join('');
        return `
          <div class="job-item">
            <h3>${escapeHtml(job.type)}</h3>
            <p><strong>Status:</strong> ${escapeHtml(job.status)}; 
               <strong>ID:</strong> <code>${escapeHtml(job.id)}</code></p>
            <p><strong>Updated:</strong> ${escapeHtml(job.updated_at)}</p>
            <ul class="job-logs">${logs}</ul>
          </div>`;
      }).join('');

      deploymentJobsPanel.innerHTML = `
        <p class="contents-meta"><strong>${escapeHtml(deploymentId)}</strong>:
        ${jobs.length} recent job(s)</p>
        <div class="job-list">${items}</div>`;
    }

    function renderDeploymentImports(deploymentId, imports) {
      if (!deploymentId) {
        deploymentImportsPanel.innerHTML =
          '<p class="contents-empty">Enter a deployment ID first.</p>';
        return;
      }

      if (!imports.length) {
        deploymentImportsPanel.innerHTML = `
          <p class="contents-meta"><strong>${escapeHtml(deploymentId)}</strong>:
          no imports found.</p>`;
        return;
      }

      const items = imports.map((entry) => `
        <div class="import-item">
          <h3>${escapeHtml(entry.component)} / ${escapeHtml(entry.resource_name)}</h3>
          <p><strong>Imported:</strong> ${escapeHtml(entry.imported_at)}; 
             <strong>Job:</strong> <code>${escapeHtml(entry.job_id)}</code></p>
          <p><strong>Source:</strong> <code>${escapeHtml(entry.source_path ?? '')}</code></p>
          <p><strong>Destination:</strong> <code>${escapeHtml(entry.destination_path)}</code></p>
        </div>`).join('');

      deploymentImportsPanel.innerHTML = `
        <p class="contents-meta"><strong>${escapeHtml(deploymentId)}</strong>:
        ${imports.length} import record(s)</p>
        <div class="import-list">${items}</div>`;
    }

    async function loadDeploymentJobs(deploymentId) {
      if (!deploymentId) {
        deploymentJobsPanel.innerHTML =
          '<p class="contents-empty">Enter a deployment ID first.</p>';
        return;
      }

      const { response, text } = await request('GET', '/v1/jobs');
      if (!response.ok) {
        deploymentJobsPanel.innerHTML =
          '<p class="contents-empty">Could not load deployment jobs.</p>';
        return;
      }

      const document = JSON.parse(text);
      const jobs = (document.jobs ?? [])
        .filter((job) => job.deployment_id === deploymentId)
        .sort((lhs, rhs) => rhs.id.localeCompare(lhs.id))
        .slice(0, 10);
      renderDeploymentJobs(deploymentId, jobs);
    }

    async function loadDeploymentImports(deploymentId) {
      if (!deploymentId) {
        deploymentImportsPanel.innerHTML =
          '<p class="contents-empty">Enter a deployment ID first.</p>';
        return;
      }

      const { response, text } = await request(
        'GET', `/v1/deployments/${deploymentId}/imports`);
      if (!response.ok) {
        deploymentImportsPanel.innerHTML =
          '<p class="contents-empty">Could not load deployment imports.</p>';
        return;
      }

      const document = JSON.parse(text);
      const imports = (document.imports ?? [])
        .slice()
        .sort((lhs, rhs) => rhs.imported_at.localeCompare(lhs.imported_at));
      renderDeploymentImports(deploymentId, imports);
    }

    async function loadDeploymentContents(deploymentId) {
      if (!deploymentId) {
        deploymentContentsPanel.innerHTML =
          '<p class="contents-empty">Enter a deployment ID first.</p>';
        return;
      }

      const { response, text } = await request(
        'GET', `/v1/deployments/${deploymentId}/inspect`);
      if (!response.ok) {
        deploymentContentsPanel.innerHTML =
          `<p class="contents-empty">Could not load deployment contents.</p>`;
        return;
      }

      renderDeploymentContents(JSON.parse(text));
      await loadDeploymentJobs(deploymentId);
      await loadDeploymentImports(deploymentId);
    }

    async function followJob(jobId, deploymentId) {
      deploymentContentsPanel.innerHTML =
        `<p class="contents-empty">Waiting for job <code>${escapeHtml(jobId)}</code> to finish...</p>`;
      deploymentJobsPanel.innerHTML =
        `<p class="contents-empty">Waiting for job <code>${escapeHtml(jobId)}</code> to finish...</p>`;
      deploymentImportsPanel.innerHTML =
        `<p class="contents-empty">Waiting for job <code>${escapeHtml(jobId)}</code> to finish...</p>`;

      for (let attempt = 0; attempt < 30; ++attempt) {
        await sleep(1000);
        const { response, text } = await request('GET', `/v1/jobs/${jobId}`);
        if (!response.ok) { return; }

        const document = JSON.parse(text);
        const status = document.job?.status;
        if (status === 'succeeded' || status === 'failed') {
          if (deploymentId) {
            await loadDeploymentContents(deploymentId);
          }
          return;
        }
      }

      deploymentContentsPanel.innerHTML =
        '<p class="contents-empty">Job is still running. Refresh deployment contents in a moment.</p>';
      deploymentJobsPanel.innerHTML =
        '<p class="contents-empty">Job is still running. Refresh deployment jobs in a moment.</p>';
      deploymentImportsPanel.innerHTML =
        '<p class="contents-empty">Job is still running. Refresh deployment imports in a moment.</p>';
    }

    document.getElementById('deployment-form').addEventListener(
      'submit',
      async (event) => {
        event.preventDefault();
        const form = new FormData(event.target);
        const payload = Object.fromEntries(form);
        document.getElementById('job-deployment-id').value = payload.id;
        document.getElementById('deployment-inspect-id').value = payload.id;
        await request('POST', '/v1/deployments', payload);
        await loadDeploymentContents(payload.id);
      });

    document.getElementById('job-form').addEventListener(
      'submit',
      async (event) => {
        event.preventDefault();
        const form = new FormData(event.target);
        const payload = Object.fromEntries(form);
        if (!payload.deployment_id) {
          delete payload.deployment_id;
        }
        payload.source_path = (payload.source_path ?? '').trim();
        if (payload.source_path) {
          payload.type = 'import_configuration';
          jobTypeField.value = 'import_configuration';
        }
        if (payload.type !== 'import_configuration') {
          delete payload.source_path;
        } else {
          if (!payload.source_path) {
            payload.source_path = '/etc/bareos';
          }
        }
        const { response, text } = await request('POST', '/v1/jobs', payload);
        if (payload.deployment_id) {
          deploymentContentsPanel.innerHTML =
            '<p class="contents-empty">Job submitted. Waiting for completion...</p>';
        }
        if (response.ok) {
          const document = JSON.parse(text);
          const jobId = document.job?.id;
          if (jobId && payload.deployment_id) {
            await followJob(jobId, payload.deployment_id);
          }
        }
      });
    jobSourcePathField.addEventListener('input', () => {
      if (jobSourcePathField.value.trim()) {
        jobTypeField.value = 'import_configuration';
      }
    });

    document.getElementById('health-button').addEventListener(
      'click', () => request('GET', '/v1/health'));
    document.getElementById('deployments-button').addEventListener(
      'click', () => request('GET', '/v1/deployments'));
    document.getElementById('jobs-button').addEventListener(
      'click', () => request('GET', '/v1/jobs'));
    document.getElementById('schema-form').addEventListener(
      'submit',
      async (event) => {
        event.preventDefault();
        const form = new FormData(event.target);
        await request('GET', `/v1/schema/${form.get('component')}`);
      });
    document.getElementById('inspect-form').addEventListener(
      'submit',
      async (event) => {
        event.preventDefault();
        const form = new FormData(event.target);
        await request('POST', '/v1/inspect', Object.fromEntries(form));
      });
    document.getElementById('deployment-inspect-form').addEventListener(
      'submit',
      async (event) => {
        event.preventDefault();
        const form = new FormData(event.target);
        await loadDeploymentContents(form.get('deployment_id'));
      });

    request('GET', '/v1/health');
  </script>
</body>
</html>
)HTML";

std::string DumpJson(json_t* value)
{
  char* dump = json_dumps(value, JSON_INDENT(2));
  if (!dump) { return "{}\n"; }
  std::string json_text{dump};
  std::free(dump);
  json_text.push_back('\n');
  return json_text;
}

http::response<http::string_body> JsonResponse(http::status status,
                                               std::string body)
{
  http::response<http::string_body> response{status, 11};
  response.set(http::field::content_type, "application/json");
  response.keep_alive(false);
  response.body() = std::move(body);
  response.prepare_payload();
  return response;
}

http::response<http::string_body> HtmlResponse(http::status status,
                                               std::string body)
{
  http::response<http::string_body> response{status, 11};
  response.set(http::field::content_type, "text/html; charset=utf-8");
  response.keep_alive(false);
  response.body() = std::move(body);
  response.prepare_payload();
  return response;
}

http::response<http::string_body> ErrorResponse(http::status status,
                                                const std::string& message)
{
  auto root = MakeJson(json_object());
  json_object_set_new(root.get(), "error", json_string(message.c_str()));
  return JsonResponse(status, DumpJson(root.get()));
}

void AppendDeployment(json_t* array, const DeploymentRecord& record)
{
  auto object = MakeJson(json_object());
  json_object_set_new(object.get(), "id", json_string(record.id.c_str()));
  json_object_set_new(object.get(), "name", json_string(record.name.c_str()));
  json_object_set_new(object.get(), "repository_path",
                      json_string(record.repository_path.string().c_str()));
  json_object_set_new(object.get(), "workflow_mode",
                      json_string(ToString(record.workflow_mode).data()));
  json_object_set_new(object.get(), "created_at",
                      json_string(record.created_at.c_str()));
  json_array_append_new(array, object.release());
}

void AppendJob(json_t* array, const JobRecord& record)
{
  auto object = MakeJson(json_object());
  json_object_set_new(object.get(), "id", json_string(record.id.c_str()));
  json_object_set_new(object.get(), "type", json_string(record.type.c_str()));
  if (record.deployment_id) {
    json_object_set_new(object.get(), "deployment_id",
                        json_string(record.deployment_id->c_str()));
  } else {
    json_object_set_new(object.get(), "deployment_id", json_null());
  }
  if (record.source_component) {
    json_object_set_new(object.get(), "source_component",
                        json_string(record.source_component->c_str()));
  } else {
    json_object_set_new(object.get(), "source_component", json_null());
  }
  if (record.source_path) {
    json_object_set_new(object.get(), "source_path",
                        json_string(record.source_path->c_str()));
  } else {
    json_object_set_new(object.get(), "source_path", json_null());
  }
  json_object_set_new(object.get(), "status",
                      json_string(ToString(record.status).data()));
  json_object_set_new(object.get(), "created_at",
                      json_string(record.created_at.c_str()));
  json_object_set_new(object.get(), "updated_at",
                      json_string(record.updated_at.c_str()));
  if (record.started_at) {
    json_object_set_new(object.get(), "started_at",
                        json_string(record.started_at->c_str()));
  } else {
    json_object_set_new(object.get(), "started_at", json_null());
  }
  if (record.finished_at) {
    json_object_set_new(object.get(), "finished_at",
                        json_string(record.finished_at->c_str()));
  } else {
    json_object_set_new(object.get(), "finished_at", json_null());
  }
  if (record.last_error) {
    json_object_set_new(object.get(), "last_error",
                        json_string(record.last_error->c_str()));
  } else {
    json_object_set_new(object.get(), "last_error", json_null());
  }
  auto logs = MakeJson(json_array());
  for (const auto& log : record.logs) {
    json_array_append_new(logs.get(), json_string(log.c_str()));
  }
  json_object_set_new(object.get(), "logs", logs.release());
  json_array_append_new(array, object.release());
}

void AppendDeploymentImport(json_t* array, const DeploymentImportRecord& record)
{
  auto object = MakeJson(json_object());
  json_object_set_new(object.get(), "job_id", json_string(record.job_id.c_str()));
  json_object_set_new(object.get(), "component",
                      json_string(record.component.c_str()));
  json_object_set_new(object.get(), "resource_name",
                      json_string(record.resource_name.c_str()));
  if (record.source_path) {
    json_object_set_new(object.get(), "source_path",
                        json_string(record.source_path->c_str()));
  } else {
    json_object_set_new(object.get(), "source_path", json_null());
  }
  json_object_set_new(object.get(), "destination_path",
                      json_string(record.destination_path.c_str()));
  json_object_set_new(object.get(), "imported_at",
                      json_string(record.imported_at.c_str()));
  json_array_append_new(array, object.release());
}

void SetSource(json_t* object,
               const char* key,
               const std::optional<BareosResource::SourceLocation>& source)
{
  if (!source) {
    json_object_set_new(object, key, json_null());
    return;
  }

  auto source_json = MakeJson(json_object());
  json_object_set_new(source_json.get(), "file",
                      json_string(source->file.c_str()));
  json_object_set_new(source_json.get(), "line", json_integer(source->line));
  json_object_set_new(object, key, source_json.release());
}

void SetMessages(json_t* object, const bconfig::ParseMessages& messages)
{
  auto messages_json = MakeJson(json_object());
  auto errors = MakeJson(json_array());
  auto warnings = MakeJson(json_array());
  for (const auto& error : messages.errors) {
    json_array_append_new(errors.get(), json_string(error.c_str()));
  }
  for (const auto& warning : messages.warnings) {
    json_array_append_new(warnings.get(), json_string(warning.c_str()));
  }
  json_object_set_new(messages_json.get(), "errors", errors.release());
  json_object_set_new(messages_json.get(), "warnings", warnings.release());
  json_object_set_new(object, "messages", messages_json.release());
}

json_t* DirectiveUseToJson(const bconfig::DirectiveUseEntry& directive)
{
  auto object = MakeJson(json_object());
  json_object_set_new(object.get(), "name", json_string(directive.name.c_str()));
  SetSource(object.get(), "source", directive.source);
  return object.release();
}

json_t* RelationToJson(const bconfig::RelationEntry& relation)
{
  auto object = MakeJson(json_object());
  json_object_set_new(object.get(), "directive",
                      json_string(relation.directive.c_str()));
  json_object_set_new(object.get(), "target_type",
                      json_string(relation.target_type.c_str()));
  json_object_set_new(object.get(), "target_name",
                      json_string(relation.target_name.c_str()));
  SetSource(object.get(), "source", relation.source);
  return object.release();
}

json_t* ExternalRelationToJson(const bconfig::ExternalRelationEntry& relation)
{
  auto object = MakeJson(json_object());
  json_object_set_new(object.get(), "relation",
                      json_string(relation.relation.c_str()));
  json_object_set_new(object.get(), "target_component",
                      json_string(relation.target_component.c_str()));
  json_object_set_new(object.get(), "target_type",
                      json_string(relation.target_type.c_str()));
  json_object_set_new(object.get(), "target_name",
                      json_string(relation.target_name.c_str()));
  json_object_set_new(object.get(), "matched", json_boolean(relation.matched));
  if (relation.detail) {
    json_object_set_new(object.get(), "detail",
                        json_string(relation.detail->c_str()));
  } else {
    json_object_set_new(object.get(), "detail", json_null());
  }
  SetSource(object.get(), "source", relation.source);
  return object.release();
}

json_t* DetailValueToJson(const bconfig::DetailValueEntry& value)
{
  auto object = MakeJson(json_object());
  json_object_set_new(object.get(), "name", json_string(value.name.c_str()));
  json_object_set_new(object.get(), "value", json_string(value.value.c_str()));
  SetSource(object.get(), "source", value.source);
  return object.release();
}

json_t* NestedDetailToJson(const bconfig::NestedDetailEntry& detail)
{
  auto object = MakeJson(json_object());
  auto values = MakeJson(json_array());
  json_object_set_new(object.get(), "kind", json_string(detail.kind.c_str()));
  if (detail.summary) {
    json_object_set_new(object.get(), "summary",
                        json_string(detail.summary->c_str()));
  } else {
    json_object_set_new(object.get(), "summary", json_null());
  }
  SetSource(object.get(), "source", detail.source);
  for (const auto& value : detail.values) {
    json_array_append_new(values.get(), DetailValueToJson(value));
  }
  json_object_set_new(object.get(), "values", values.release());
  return object.release();
}

json_t* InspectionEntryToJson(const bconfig::ResourceInspectionEntry& entry)
{
  auto object = MakeJson(json_object());
  auto directives = MakeJson(json_array());
  auto relations = MakeJson(json_array());
  auto external_relations = MakeJson(json_array());
  auto nested_details = MakeJson(json_array());

  json_object_set_new(object.get(), "type", json_string(entry.type.c_str()));
  json_object_set_new(object.get(), "name", json_string(entry.name.c_str()));
  json_object_set_new(object.get(), "internal", json_boolean(entry.internal));
  SetSource(object.get(), "source", entry.source);

  for (const auto& directive : entry.directives) {
    json_array_append_new(directives.get(), DirectiveUseToJson(directive));
  }
  for (const auto& relation : entry.relations) {
    json_array_append_new(relations.get(), RelationToJson(relation));
  }
  for (const auto& relation : entry.external_relations) {
    json_array_append_new(external_relations.get(),
                          ExternalRelationToJson(relation));
  }
  for (const auto& detail : entry.nested_details) {
    json_array_append_new(nested_details.get(), NestedDetailToJson(detail));
  }

  json_object_set_new(object.get(), "directives", directives.release());
  json_object_set_new(object.get(), "relations", relations.release());
  json_object_set_new(object.get(), "external_relations",
                      external_relations.release());
  json_object_set_new(object.get(), "nested_details", nested_details.release());
  return object.release();
}

std::optional<InspectRequestSpec> ParseInspectRequest(std::string_view body,
                                                      std::string& error)
{
  json_error_t json_error{};
  auto root = MakeJson(json_loadb(body.data(), body.size(), 0, &json_error));
  if (!root) {
    error = "invalid JSON body: " + std::string{json_error.text};
    return std::nullopt;
  }

  auto* component = json_object_get(root.get(), "component");
  auto* path = json_object_get(root.get(), "path");
  auto* peers = json_object_get(root.get(), "peers");

  if (!json_is_string(component) || !json_is_string(path)) {
    error = "fields 'component' and 'path' must be strings.";
    return std::nullopt;
  }

  auto parsed_component = bconfig::ParseComponent(json_string_value(component));
  if (!parsed_component) {
    error = "unknown component.";
    return std::nullopt;
  }

  InspectRequestSpec spec{.component = *parsed_component,
                          .path = json_string_value(path)};

  if (!peers) { return spec; }
  if (!json_is_array(peers)) {
    error = "field 'peers' must be an array when provided.";
    return std::nullopt;
  }

  size_t index = 0;
  json_t* peer = nullptr;
  json_array_foreach(peers, index, peer)
  {
    auto* peer_component = json_object_get(peer, "component");
    auto* peer_path = json_object_get(peer, "path");
    if (!json_is_string(peer_component) || !json_is_string(peer_path)) {
      error = "every peer requires string fields 'component' and 'path'.";
      return std::nullopt;
    }

    auto parsed_peer_component
        = bconfig::ParseComponent(json_string_value(peer_component));
    if (!parsed_peer_component) {
      error = "peer component is unknown.";
      return std::nullopt;
    }

    spec.peers.emplace_back(
        PeerLoadSpec{.component = *parsed_peer_component,
                     .path = json_string_value(peer_path)});
  }

  return spec;
}

JsonPtr BuildInspectionDocument(const InspectRequestSpec& spec,
                                bool& parser_initialized)
{
  auto loaded = bconfig::LoadConfig(spec.component, spec.path, true);
  parser_initialized = static_cast<bool>(loaded.parser);

  std::vector<bconfig::LoadedConfig> peer_configs;
  std::vector<const bconfig::LoadedConfig*> peer_refs;
  peer_configs.reserve(spec.peers.size());
  peer_refs.reserve(spec.peers.size());
  for (const auto& peer : spec.peers) {
    peer_configs.emplace_back(bconfig::LoadConfig(peer.component, peer.path, true));
  }
  for (const auto& peer : peer_configs) { peer_refs.emplace_back(&peer); }

  auto root = MakeJson(json_object());
  auto resources = MakeJson(json_array());
  auto peers = MakeJson(json_array());
  json_object_set_new(root.get(), "component",
                      json_string(bconfig::ComponentToString(spec.component)));
  json_object_set_new(root.get(), "path", json_string(spec.path.c_str()));
  json_object_set_new(root.get(), "parse_ok", json_boolean(loaded.parse_ok));
  SetMessages(root.get(), loaded.messages);

  for (const auto& peer : peer_configs) {
    auto peer_json = MakeJson(json_object());
    json_object_set_new(
        peer_json.get(), "component",
        json_string(bconfig::ComponentToString(peer.component)));
    json_object_set_new(peer_json.get(), "path",
                        json_string(peer.parser
                                        ? peer.parser->get_base_config_path()
                                              .c_str()
                                        : ""));
    json_object_set_new(peer_json.get(), "parse_ok", json_boolean(peer.parse_ok));
    SetMessages(peer_json.get(), peer.messages);
    json_array_append_new(peers.get(), peer_json.release());
  }

  if (!loaded.parser) {
    json_object_set_new(root.get(), "error",
                        json_string("failed to initialize parser"));
  } else {
    for (const auto& resource : bconfig::CollectResources(loaded, peer_refs)) {
      json_array_append_new(resources.get(), InspectionEntryToJson(resource));
    }
  }

  json_object_set_new(root.get(), "peers", peers.release());
  json_object_set_new(root.get(), "resources", resources.release());
  return root;
}

std::vector<DeploymentConfigSpec> DiscoverDeploymentConfigs(
    const DeploymentRecord& deployment)
{
  std::vector<DeploymentConfigSpec> configs;

  for (const auto component : SupportedDeploymentInspectComponents()) {
    const auto bucket
        = ComponentBucketDirectory(deployment.repository_path, component);
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

      configs.push_back(DeploymentConfigSpec{
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

http::response<http::string_body> HandleSchemaRequest(
    const std::vector<std::string_view>& path_parts)
{
  if (path_parts.size() != 3) {
    return ErrorResponse(http::status::not_found, "unknown schema route.");
  }

  auto component = bconfig::ParseComponent(path_parts[2]);
  if (!component) {
    return ErrorResponse(http::status::bad_request, "unknown component.");
  }

  auto loaded = bconfig::LoadConfig(*component, "", false);
  if (!loaded.parser) {
    auto root = MakeJson(json_object());
    const std::string component_name{path_parts[2]};
    json_object_set_new(root.get(), "component",
                        json_string(component_name.c_str()));
    SetMessages(root.get(), loaded.messages);
    json_object_set_new(root.get(), "error",
                        json_string("failed to initialize parser"));
    return JsonResponse(http::status::bad_request, DumpJson(root.get()));
  }

  auto root = MakeJson(json_object());
  auto resources = MakeJson(json_array());
  json_object_set_new(root.get(), "component",
                      json_string(bconfig::ComponentToString(*component)));

  for (const auto& resource : bconfig::CollectSchema(*loaded.parser)) {
    auto resource_json = MakeJson(json_object());
    auto directives = MakeJson(json_array());
    json_object_set_new(resource_json.get(), "name",
                        json_string(resource.name.c_str()));
    for (const auto& directive : resource.directives) {
      auto directive_json = MakeJson(json_object());
      auto aliases = MakeJson(json_array());
      json_object_set_new(directive_json.get(), "name",
                          json_string(directive.name.c_str()));
      json_object_set_new(directive_json.get(), "datatype",
                          json_string(directive.datatype.c_str()));
      if (directive.default_value) {
        json_object_set_new(directive_json.get(), "default_value",
                            json_string(directive.default_value->c_str()));
      } else {
        json_object_set_new(directive_json.get(), "default_value", json_null());
      }
      json_object_set_new(directive_json.get(), "required",
                          json_boolean(directive.required));
      json_object_set_new(directive_json.get(), "deprecated",
                          json_boolean(directive.deprecated));
      for (const auto& alias : directive.aliases) {
        json_array_append_new(aliases.get(), json_string(alias.c_str()));
      }
      json_object_set_new(directive_json.get(), "aliases", aliases.release());
      if (directive.description) {
        json_object_set_new(directive_json.get(), "description",
                            json_string(directive.description->c_str()));
      } else {
        json_object_set_new(directive_json.get(), "description", json_null());
      }
      json_array_append_new(directives.get(), directive_json.release());
    }
    json_object_set_new(resource_json.get(), "directives", directives.release());
    json_array_append_new(resources.get(), resource_json.release());
  }

  json_object_set_new(root.get(), "resources", resources.release());
  return JsonResponse(http::status::ok, DumpJson(root.get()));
}

http::response<http::string_body> HandleInspectRequest(
    const http::request<http::string_body>& request)
{
  std::string error;
  auto spec = ParseInspectRequest(request.body(), error);
  if (!spec) { return ErrorResponse(http::status::bad_request, error); }

  bool parser_initialized = false;
  auto root = BuildInspectionDocument(*spec, parser_initialized);
  if (!parser_initialized) {
    return JsonResponse(http::status::bad_request, DumpJson(root.get()));
  }
  return JsonResponse(http::status::ok, DumpJson(root.get()));
}

http::response<http::string_body> HandleDeploymentInspectRequest(
    ServiceState& state, std::string_view deployment_id)
{
  auto deployment = state.GetDeployment(deployment_id);
  if (!deployment) {
    return ErrorResponse(http::status::not_found, "deployment not found.");
  }

  auto root = MakeJson(json_object());
  auto deployment_json = MakeJson(json_array());
  auto configs_json = MakeJson(json_array());
  AppendDeployment(deployment_json.get(), *deployment);
  json_object_set(root.get(), "deployment",
                  json_array_get(deployment_json.get(), 0));

  for (const auto& config : DiscoverDeploymentConfigs(*deployment)) {
    InspectRequestSpec spec{.component = config.component,
                            .path = config.path.string(),
                            .peers = {}};
    bool parser_initialized = false;
    auto inspection = BuildInspectionDocument(spec, parser_initialized);
    json_object_set_new(inspection.get(), "name", json_string(config.name.c_str()));
    json_array_append_new(configs_json.get(), inspection.release());
  }

  json_object_set_new(root.get(), "configs", configs_json.release());
  return JsonResponse(http::status::ok, DumpJson(root.get()));
}

http::response<http::string_body> HandleDeploymentImportsRequest(
    ServiceState& state, std::string_view deployment_id)
{
  auto deployment = state.GetDeployment(deployment_id);
  if (!deployment) {
    return ErrorResponse(http::status::not_found, "deployment not found.");
  }

  auto imports = state.ListDeploymentImports(deployment_id);
  if (!imports) {
    return ErrorResponse(http::status::bad_request, imports.error);
  }

  auto root = MakeJson(json_object());
  auto deployment_json = MakeJson(json_array());
  auto imports_json = MakeJson(json_array());
  AppendDeployment(deployment_json.get(), *deployment);
  json_object_set(root.get(), "deployment",
                  json_array_get(deployment_json.get(), 0));
  for (const auto& record : *imports.value) {
    AppendDeploymentImport(imports_json.get(), record);
  }
  json_object_set_new(root.get(), "imports", imports_json.release());
  return JsonResponse(http::status::ok, DumpJson(root.get()));
}

std::vector<std::string_view> SplitPath(std::string_view target)
{
  auto path = target.substr(0, target.find('?'));
  std::vector<std::string_view> parts;

  while (!path.empty()) {
    const auto slash = path.find('/');
    const auto part = path.substr(0, slash);
    if (!part.empty()) { parts.emplace_back(part); }

    if (slash == std::string_view::npos) { break; }
    path.remove_prefix(slash + 1);
  }

  return parts;
}

std::optional<DeploymentSpec> ParseDeploymentSpec(std::string_view body,
                                                      std::string& error)
{
  json_error_t json_error{};
  auto root = MakeJson(json_loadb(body.data(), body.size(), 0, &json_error));
  if (!root) {
    error = "invalid JSON body: " + std::string{json_error.text};
    return std::nullopt;
  }

  auto* id = json_object_get(root.get(), "id");
  auto* name = json_object_get(root.get(), "name");
  auto* repository_path = json_object_get(root.get(), "repository_path");
  auto* workflow_mode = json_object_get(root.get(), "workflow_mode");

  if (!json_is_string(id) || !json_is_string(name)
      || !json_is_string(repository_path)) {
    error = "fields 'id', 'name', and 'repository_path' must be strings.";
    return std::nullopt;
  }

  DeploymentSpec spec{};
  spec.id = json_string_value(id);
  spec.name = json_string_value(name);
  spec.repository_path = json_string_value(repository_path);

  if (workflow_mode) {
    if (!json_is_string(workflow_mode)) {
      error = "field 'workflow_mode' must be a string when provided.";
      return std::nullopt;
    }

    auto parsed = ParseWorkflowMode(json_string_value(workflow_mode));
    if (!parsed) {
      error
          = "workflow_mode must be 'direct_commit' or 'review'.";
      return std::nullopt;
    }
    spec.workflow_mode = *parsed;
  }

  return spec;
}

std::optional<JobSpec> ParseJobSpec(std::string_view body, std::string& error)
{
  json_error_t json_error{};
  auto root = MakeJson(json_loadb(body.data(), body.size(), 0, &json_error));
  if (!root) {
    error = "invalid JSON body: " + std::string{json_error.text};
    return std::nullopt;
  }

  auto* type = json_object_get(root.get(), "type");
  auto* deployment_id = json_object_get(root.get(), "deployment_id");
  auto* source_component = json_object_get(root.get(), "source_component");
  auto* source_path = json_object_get(root.get(), "source_path");
  if (!json_is_string(type)) {
    error = "field 'type' must be a string.";
    return std::nullopt;
  }

  if (deployment_id && !json_is_null(deployment_id)
      && !json_is_string(deployment_id)) {
    error = "field 'deployment_id' must be a string when provided.";
    return std::nullopt;
  }
  if (source_component && !json_is_null(source_component)
      && !json_is_string(source_component)) {
    error = "field 'source_component' must be a string when provided.";
    return std::nullopt;
  }
  if (source_path && !json_is_null(source_path) && !json_is_string(source_path)) {
    error = "field 'source_path' must be a string when provided.";
    return std::nullopt;
  }

  JobSpec spec{};
  spec.type = json_string_value(type);
  if (deployment_id && json_is_string(deployment_id)) {
    spec.deployment_id = std::string{json_string_value(deployment_id)};
  }
  if (source_component && json_is_string(source_component)) {
    spec.source_component = std::string{json_string_value(source_component)};
  }
  if (source_path && json_is_string(source_path)) {
    spec.source_path = std::string{json_string_value(source_path)};
  }

  return spec;
}

http::response<http::string_body> HandleDeploymentsRequest(
    ServiceState& state,
    const http::request<http::string_body>& request,
    const std::vector<std::string_view>& path_parts)
{
  if (path_parts.size() == 2 && request.method() == http::verb::get) {
    auto root = MakeJson(json_object());
    auto deployments = MakeJson(json_array());
    for (const auto& deployment : state.ListDeployments()) {
      AppendDeployment(deployments.get(), deployment);
    }
    json_object_set_new(root.get(), "deployments", deployments.release());
    return JsonResponse(http::status::ok, DumpJson(root.get()));
  }

  if (path_parts.size() == 2 && request.method() == http::verb::post) {
    std::string error;
    auto spec = ParseDeploymentSpec(request.body(), error);
    if (!spec) { return ErrorResponse(http::status::bad_request, error); }

    auto result = state.CreateDeployment(*spec);
    if (!result) {
      return ErrorResponse(http::status::bad_request, result.error);
    }

    auto root = MakeJson(json_object());
    auto deployments = MakeJson(json_array());
    AppendDeployment(deployments.get(), *result.value);
    auto* deployment = json_array_get(deployments.get(), 0);
    json_object_set(root.get(), "deployment", deployment);
    return JsonResponse(http::status::created, DumpJson(root.get()));
  }

  if (path_parts.size() == 3 && request.method() == http::verb::get) {
    auto deployment = state.GetDeployment(path_parts[2]);
    if (!deployment) {
      return ErrorResponse(http::status::not_found, "deployment not found.");
    }

    auto root = MakeJson(json_object());
    auto array = MakeJson(json_array());
    AppendDeployment(array.get(), *deployment);
    auto* item = json_array_get(array.get(), 0);
    json_object_set(root.get(), "deployment", item);
    return JsonResponse(http::status::ok, DumpJson(root.get()));
  }

  if (path_parts.size() == 4 && path_parts[3] == "inspect"
      && request.method() == http::verb::get) {
    return HandleDeploymentInspectRequest(state, path_parts[2]);
  }

  if (path_parts.size() == 4 && path_parts[3] == "imports"
      && request.method() == http::verb::get) {
    return HandleDeploymentImportsRequest(state, path_parts[2]);
  }

  return ErrorResponse(http::status::not_found, "unknown deployments route.");
}

http::response<http::string_body> HandleJobsRequest(
    ServiceState& state,
    const http::request<http::string_body>& request,
    const std::vector<std::string_view>& path_parts)
{
  if (path_parts.size() == 2 && request.method() == http::verb::get) {
    auto root = MakeJson(json_object());
    auto jobs = MakeJson(json_array());
    for (const auto& job : state.ListJobs()) {
      AppendJob(jobs.get(), job);
    }
    json_object_set_new(root.get(), "jobs", jobs.release());
    return JsonResponse(http::status::ok, DumpJson(root.get()));
  }

  if (path_parts.size() == 2 && request.method() == http::verb::post) {
    std::string error;
    auto spec = ParseJobSpec(request.body(), error);
    if (!spec) { return ErrorResponse(http::status::bad_request, error); }

    auto result = state.CreateJob(*spec);
    if (!result) {
      return ErrorResponse(http::status::bad_request, result.error);
    }

    auto root = MakeJson(json_object());
    auto jobs = MakeJson(json_array());
    AppendJob(jobs.get(), *result.value);
    auto* job = json_array_get(jobs.get(), 0);
    json_object_set(root.get(), "job", job);
    return JsonResponse(http::status::created, DumpJson(root.get()));
  }

  if (path_parts.size() == 3 && request.method() == http::verb::get) {
    auto job = state.GetJob(path_parts[2]);
    if (!job) { return ErrorResponse(http::status::not_found, "job not found."); }

    auto root = MakeJson(json_object());
    auto jobs = MakeJson(json_array());
    AppendJob(jobs.get(), *job);
    auto* item = json_array_get(jobs.get(), 0);
    json_object_set(root.get(), "job", item);
    return JsonResponse(http::status::ok, DumpJson(root.get()));
  }

  return ErrorResponse(http::status::not_found, "unknown jobs route.");
}

http::response<http::string_body> HandleRequest(
    ServiceState& state, const http::request<http::string_body>& request)
{
  const auto target
      = std::string_view{request.target().data(), request.target().size()};
  const auto path_parts = SplitPath(std::string_view{request.target().data(),
                                                     request.target().size()});

  if (request.method() == http::verb::get
      && (target == "/" || target == "/ui" || target == "/ui/")) {
    return HtmlResponse(http::status::ok, kTestUiHtml);
  }

  if (path_parts.empty()) {
    return ErrorResponse(http::status::not_found, "route not found.");
  }

  if (request.method() != http::verb::get
      && request.method() != http::verb::post) {
    return ErrorResponse(http::status::method_not_allowed,
                         "only GET and POST are currently supported.");
  }

  if (path_parts.size() == 2 && path_parts[0] == "v1"
      && path_parts[1] == "health" && request.method() == http::verb::get) {
    auto root = MakeJson(json_object());
    json_object_set_new(root.get(), "status", json_string("ok"));
    json_object_set_new(root.get(), "service", json_string("bconfig-service"));
    if (state.HasPersistentState()) {
      json_object_set_new(
          root.get(), "state_directory",
          json_string(state.GetStateDirectory().string().c_str()));
    } else {
      json_object_set_new(root.get(), "state_directory", json_null());
    }
    return JsonResponse(http::status::ok, DumpJson(root.get()));
  }

  if (path_parts.size() >= 2 && path_parts[0] == "v1"
      && path_parts[1] == "schema" && request.method() == http::verb::get) {
    return HandleSchemaRequest(path_parts);
  }

  if (path_parts.size() == 2 && path_parts[0] == "v1"
      && path_parts[1] == "inspect" && request.method() == http::verb::post) {
    return HandleInspectRequest(request);
  }

  if (path_parts.size() >= 2 && path_parts[0] == "v1"
      && path_parts[1] == "deployments") {
    return HandleDeploymentsRequest(state, request, path_parts);
  }

  if (path_parts.size() >= 2 && path_parts[0] == "v1"
      && path_parts[1] == "jobs") {
    return HandleJobsRequest(state, request, path_parts);
  }

  return ErrorResponse(http::status::not_found, "route not found.");
}

void RunServer(const std::string& address, uint16_t port, ServiceState& state)
{
  net::io_context io_context{1};
  tcp::acceptor acceptor{
      io_context,
      tcp::endpoint{net::ip::make_address(address), port}};

  std::cout << "bconfig-service listening on " << address << ":" << port
            << std::endl;

  for (;;) {
    tcp::socket socket{io_context};
    acceptor.accept(socket);

    beast::flat_buffer buffer;
    http::request<http::string_body> request;
    http::read(socket, buffer, request);

    auto response = HandleRequest(state, request);
    http::write(socket, response);

    beast::error_code error_code;
    socket.shutdown(tcp::socket::shutdown_send, error_code);
  }
}

}  // namespace
}  // namespace bconfig::service

int main(int argc, char** argv)
{
  setlocale(LC_ALL, "");
  tzset();
  bindtextdomain("bareos", LOCALEDIR);
  textdomain("bareos");

  CLI::App application;
  InitCLIApp(application, "The Bareos configuration service.");

  AddDebugOptions(application);

  std::string address{"127.0.0.1"};
  uint16_t port = 8080;
  std::string state_dir{
      (std::filesystem::temp_directory_path() / "bconfig-service-state")
          .string()};
  application.add_option("--address", address,
                         "Address to listen on for HTTP requests.");
  application.add_option("--port", port, "Port to listen on for HTTP requests.");
  application.add_option("--state-dir", state_dir,
                         "Directory for persistent service state.");

  ParseBareosApp(application, argc, argv);
  OSDependentInit();

  try {
    bconfig::service::ServiceState state{state_dir};
    bconfig::service::RunServer(address, port, state);
  } catch (const std::exception& error) {
    std::cerr << "bconfig-service failed: " << error.what() << std::endl;
    return BEXIT_FAILURE;
  }

  return BEXIT_SUCCESS;
}
