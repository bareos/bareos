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
#include "lib/message.h"
#include "lib/thread_specific_data.h"

#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <jansson.h>

#include <chrono>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <set>
#include <source_location>
#include <sstream>
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
constexpr int kServiceDebugLevel = 10;

struct PeerLoadSpec {
  bconfig::Component component;
  std::string path;
};

struct InspectRequestSpec {
  bconfig::Component component;
  std::string path;
  std::vector<PeerLoadSpec> peers{};
};

struct ClientDirectorStubRequestSpec {
  std::optional<std::string> description{};
};

struct DirectorClientRequestSpec {
  std::optional<std::string> address{};
  std::optional<uint16_t> port{};
  std::optional<std::string> password{};
  std::optional<std::string> description{};
};

struct DirectorStorageRequestSpec {
  std::optional<std::string> address{};
  std::optional<uint16_t> port{};
  std::optional<std::string> password{};
  std::optional<std::string> device{};
  std::optional<std::string> media_type{};
  std::optional<std::string> archive_device{};
  std::optional<std::string> device_type{};
  std::optional<std::string> description{};
};

struct DirectorConsoleRequestSpec {
  std::optional<std::string> password{};
  std::optional<std::string> description{};
  std::optional<bool> use_pam_authentication{};
};

struct DirectorUserRequestSpec {
  std::optional<std::string> description{};
};

struct DirectorProfileRequestSpec {
  std::optional<std::string> description{};
};

struct DirectorPoolRequestSpec {
  std::optional<std::string> pool_type{};
  std::optional<std::string> label_format{};
  std::optional<uint32_t> maximum_volumes{};
  std::optional<uint64_t> maximum_volume_bytes{};
  std::optional<uint64_t> volume_retention{};
  std::optional<bool> auto_prune{};
  std::optional<bool> recycle{};
  std::optional<std::string> description{};
};

struct DirectorCatalogRequestSpec {
  std::optional<std::string> db_address{};
  std::optional<uint32_t> db_port{};
  std::optional<std::string> db_socket{};
  std::optional<std::string> db_password{};
  std::optional<std::string> db_user{};
  std::optional<std::string> db_name{};
  std::optional<bool> reconnect{};
  std::optional<bool> exit_on_fatal{};
  std::optional<uint32_t> min_connections{};
  std::optional<uint32_t> max_connections{};
  std::optional<uint32_t> inc_connections{};
  std::optional<uint32_t> idle_timeout{};
  std::optional<uint32_t> validate_timeout{};
  std::optional<std::string> description{};
};

struct DirectorScheduleRequestSpec {
  std::optional<std::string> description{};
  std::optional<bool> enabled{};
  std::optional<std::vector<std::string>> run_entries{};
};

std::optional<ClientDirectorStubRequestSpec> ParseClientDirectorStubRequest(
    std::string_view body,
    std::string& error);
std::optional<DirectorClientRequestSpec> ParseDirectorClientRequest(
    std::string_view body,
    std::string& error);
std::optional<DirectorStorageRequestSpec> ParseDirectorStorageRequest(
    std::string_view body,
    std::string& error);
std::optional<DirectorConsoleRequestSpec> ParseDirectorConsoleRequest(
    std::string_view body,
    std::string& error);
std::optional<DirectorUserRequestSpec> ParseDirectorUserRequest(
    std::string_view body,
    std::string& error);
std::optional<DirectorProfileRequestSpec> ParseDirectorProfileRequest(
    std::string_view body,
    std::string& error);
std::optional<DirectorPoolRequestSpec> ParseDirectorPoolRequest(
    std::string_view body,
    std::string& error);
std::optional<DirectorCatalogRequestSpec> ParseDirectorCatalogRequest(
    std::string_view body,
    std::string& error);
std::optional<DirectorScheduleRequestSpec> ParseDirectorScheduleRequest(
    std::string_view body,
    std::string& error);

JsonPtr MakeJson(json_t* value) { return JsonPtr(value, &json_decref); }

std::string MakeDebugTimestamp()
{
  const auto now = std::chrono::system_clock::now();
  const auto now_seconds = std::chrono::system_clock::to_time_t(now);
  const auto subseconds = now.time_since_epoch() % std::chrono::seconds(1);
  constexpr auto kSubsecondTicksPerSecond
      = std::chrono::system_clock::duration::period::den
        / std::chrono::system_clock::duration::period::num;
  constexpr int kSubsecondDigits = [] {
    auto ticks = kSubsecondTicksPerSecond;
    int digits = 0;
    while (ticks > 1) {
      ticks /= 10;
      ++digits;
    }
    return digits;
  }();
  std::tm utc{};
#if HAVE_WIN32
  gmtime_s(&utc, &now_seconds);
#else
  gmtime_r(&now_seconds, &utc);
#endif

  std::ostringstream stream;
  stream << std::put_time(&utc, "%Y-%m-%dT%H:%M:%S");
  if constexpr (kSubsecondDigits > 0) {
    stream << '.' << std::setw(kSubsecondDigits) << std::setfill('0')
           << subseconds.count();
  }
  stream << 'Z';
  return stream.str();
}

void DebugLog(std::string_view message,
              const std::source_location& location
              = std::source_location::current())
{
  if (kServiceDebugLevel > debug_level) { return; }

  const auto line = MakeDebugTimestamp() + " " + std::string{my_name} + " ("
                    + std::to_string(kServiceDebugLevel)
                    + "): " + get_basename(location.file_name()) + ":"
                    + std::to_string(location.line()) + "-"
                    + std::to_string(GetJobIdFromThreadSpecificData()) + " "
                    + std::string{message};
  p_msg(__FILE__, __LINE__, -1, "%s\n", line.c_str());
}

std::vector<bconfig::Component> SupportedDeploymentInspectComponents()
{
  return {bconfig::Component::kDirector, bconfig::Component::kStorage,
          bconfig::Component::kClient};
}

const char* kTestUiHtmlTemplate = R"HTML(
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
    input, select, button, textarea {
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
    .detail-list {
      margin: 0.35rem 0 0 1rem;
      padding: 0;
    }
    .detail-list li {
      margin: 0.25rem 0;
    }
    .detail-value-list {
      margin: 0.25rem 0 0 1rem;
      padding: 0;
    }
    .detail-value-list li {
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
    .git-status-list {
      margin: 0.5rem 0 0 1rem;
      padding: 0;
    }
    .git-status-list li {
      margin: 0.15rem 0;
      font-family: monospace;
    }
    .diff-preview {
      background: #0f172a;
      color: #e2e8f0;
      padding: 0.75rem;
      border-radius: 8px;
      overflow: auto;
      min-height: 12rem;
      white-space: pre-wrap;
      word-break: break-word;
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
               value="__DEFAULT_DEPLOYMENT_REPOSITORY_PATH__">

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
          <option value="commit_deployment_repo">
            commit_deployment_repo
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

        <label for="job-commit-message">Commit message</label>
        <input id="job-commit-message" name="commit_message"
               placeholder="Import Bareos config root">

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

    <section class="card">
      <h2>Upsert client director stub</h2>
      <form id="client-stub-form">
        <label for="client-stub-deployment-id">Deployment ID</label>
        <input id="client-stub-deployment-id" name="deployment_id" value="prod">

        <label for="client-stub-client-name">Client name</label>
        <input id="client-stub-client-name" name="client_name" value="bareos-fd">

        <label for="client-stub-director-name">Director name</label>
        <input id="client-stub-director-name" name="director_name" value="bareos-dir">

        <p class="contents-meta">
          The password is taken from the matching director-side
          <code>Client</code> resource.
        </p>

        <label for="client-stub-description">Description</label>
        <input id="client-stub-description" name="description"
               placeholder="Managed director stub for client">

        <button type="submit">
          PUT /v1/deployments/{id}/clients/{client}/directors/{director}
        </button>
      </form>
    </section>

    <section class="card">
      <h2>Upsert director client resource</h2>
      <form id="director-client-form">
        <label for="director-client-deployment-id">Deployment ID</label>
        <input id="director-client-deployment-id" name="deployment_id" value="prod">

        <label for="director-client-director-name">Director name</label>
        <input id="director-client-director-name" name="director_name" value="bareos-dir">

        <label for="director-client-client-name">Client name</label>
        <input id="director-client-client-name" name="client_name" value="client1-fd">

        <label for="director-client-address">Address</label>
        <input id="director-client-address" name="address" value="client1-fd.example.com">

        <label for="director-client-password">Password</label>
        <input id="director-client-password" name="password"
               placeholder="cleartext or [md5]hash">

        <label for="director-client-description">Description</label>
        <input id="director-client-description" name="description"
               placeholder="Managed client resource">

        <button type="submit">
          PUT /v1/deployments/{id}/directors/{director}/clients/{client}
        </button>
        <button type="button" id="director-client-delete-button">
          DELETE /v1/deployments/{id}/directors/{director}/clients/{client}
        </button>
      </form>
    </section>

    <section class="card">
      <h2>Upsert director storage resource</h2>
      <form id="director-storage-form">
        <label for="director-storage-deployment-id">Deployment ID</label>
        <input id="director-storage-deployment-id" name="deployment_id" value="prod">

        <label for="director-storage-director-name">Director name</label>
        <input id="director-storage-director-name" name="director_name" value="bareos-dir">

        <label for="director-storage-storage-name">Storage name</label>
        <input id="director-storage-storage-name" name="storage_name" value="FileManaged">

        <label for="director-storage-address">Address</label>
        <input id="director-storage-address" name="address" value="localhost">

        <label for="director-storage-port">Port</label>
        <input id="director-storage-port" name="port" type="number" min="1" max="65535" value="9103">

        <label for="director-storage-password">Password</label>
        <input id="director-storage-password" name="password"
               placeholder="cleartext or [md5]hash">

        <label for="director-storage-device">Device</label>
        <input id="director-storage-device" name="device" value="FileManaged">

        <label for="director-storage-media-type">Media Type</label>
        <input id="director-storage-media-type" name="media_type" value="File">

        <label for="director-storage-archive-device">Archive Device</label>
        <input id="director-storage-archive-device" name="archive_device"
               value="/tmp/bareos-storage">

        <label for="director-storage-device-type">Device Type</label>
        <input id="director-storage-device-type" name="device_type" value="file">

        <label for="director-storage-description">Description</label>
        <input id="director-storage-description" name="description"
               placeholder="Managed storage resource">

        <button type="submit">
          PUT /v1/deployments/{id}/directors/{director}/storages/{storage}
        </button>
        <button type="button" id="director-storage-delete-button">
          DELETE /v1/deployments/{id}/directors/{director}/storages/{storage}
        </button>
      </form>
    </section>

    <section class="card">
      <h2>Upsert director console resource</h2>
      <form id="director-console-form">
        <label for="director-console-deployment-id">Deployment ID</label>
        <input id="director-console-deployment-id" name="deployment_id" value="prod">

        <label for="director-console-director-name">Director name</label>
        <input id="director-console-director-name" name="director_name" value="bareos-dir">

        <label for="director-console-console-name">Console name</label>
        <input id="director-console-console-name" name="console_name" value="managed-console">

        <label for="director-console-password">Password</label>
        <input id="director-console-password" name="password"
               placeholder="cleartext or [md5]hash">

        <label for="director-console-description">Description</label>
        <input id="director-console-description" name="description"
               placeholder="Managed console resource">

        <label class="checkbox-label" for="director-console-use-pam">
          <input id="director-console-use-pam" name="use_pam_authentication"
                 type="checkbox">
          Use PAM authentication
        </label>

        <button type="submit">
          PUT /v1/deployments/{id}/directors/{director}/consoles/{console}
        </button>
        <button type="button" id="director-console-delete-button">
          DELETE /v1/deployments/{id}/directors/{director}/consoles/{console}
        </button>
      </form>
    </section>

    <section class="card">
      <h2>Upsert director user resource</h2>
      <form id="director-user-form">
        <label for="director-user-deployment-id">Deployment ID</label>
        <input id="director-user-deployment-id" name="deployment_id" value="prod">

        <label for="director-user-director-name">Director name</label>
        <input id="director-user-director-name" name="director_name" value="bareos-dir">

        <label for="director-user-user-name">User name</label>
        <input id="director-user-user-name" name="user_name" value="managed-user">

        <label for="director-user-description">Description</label>
        <input id="director-user-description" name="description"
               placeholder="Managed user resource">

        <button type="submit">
          PUT /v1/deployments/{id}/directors/{director}/users/{user}
        </button>
        <button type="button" id="director-user-delete-button">
          DELETE /v1/deployments/{id}/directors/{director}/users/{user}
        </button>
      </form>
    </section>

    <section class="card">
      <h2>Upsert director profile resource</h2>
      <form id="director-profile-form">
        <label for="director-profile-deployment-id">Deployment ID</label>
        <input id="director-profile-deployment-id" name="deployment_id" value="prod">

        <label for="director-profile-director-name">Director name</label>
        <input id="director-profile-director-name" name="director_name" value="bareos-dir">

        <label for="director-profile-profile-name">Profile name</label>
        <input id="director-profile-profile-name" name="profile_name" value="managed-profile">

        <label for="director-profile-description">Description</label>
        <input id="director-profile-description" name="description"
               placeholder="Managed profile resource">

        <button type="submit">
          PUT /v1/deployments/{id}/directors/{director}/profiles/{profile}
        </button>
        <button type="button" id="director-profile-delete-button">
          DELETE /v1/deployments/{id}/directors/{director}/profiles/{profile}
        </button>
      </form>
    </section>

    <section class="card">
      <h2>Upsert director pool resource</h2>
      <form id="director-pool-form">
        <label for="director-pool-deployment-id">Deployment ID</label>
        <input id="director-pool-deployment-id" name="deployment_id" value="prod">

        <label for="director-pool-director-name">Director name</label>
        <input id="director-pool-director-name" name="director_name" value="bareos-dir">

        <label for="director-pool-pool-name">Pool name</label>
        <input id="director-pool-pool-name" name="pool_name" value="managed-pool">

        <label for="director-pool-pool-type">Pool type</label>
        <input id="director-pool-pool-type" name="pool_type" value="Backup">

        <label for="director-pool-label-format">Label format</label>
        <input id="director-pool-label-format" name="label_format"
               placeholder="Managed-">

        <label for="director-pool-maximum-volumes">Maximum volumes</label>
        <input id="director-pool-maximum-volumes" name="maximum_volumes"
               type="number" min="0">

        <label for="director-pool-maximum-volume-bytes">Maximum volume bytes</label>
        <input id="director-pool-maximum-volume-bytes"
               name="maximum_volume_bytes" type="number" min="0">

        <label for="director-pool-volume-retention">Volume retention (seconds)</label>
        <input id="director-pool-volume-retention" name="volume_retention"
               type="number" min="0">

        <label class="checkbox-label" for="director-pool-auto-prune">
          <input id="director-pool-auto-prune" name="auto_prune"
                 type="checkbox">
          Auto prune
        </label>

        <label class="checkbox-label" for="director-pool-recycle">
          <input id="director-pool-recycle" name="recycle" type="checkbox">
          Recycle
        </label>

        <label for="director-pool-description">Description</label>
        <input id="director-pool-description" name="description"
               placeholder="Managed pool resource">

        <button type="submit">
          PUT /v1/deployments/{id}/directors/{director}/pools/{pool}
        </button>
        <button type="button" id="director-pool-delete-button">
          DELETE /v1/deployments/{id}/directors/{director}/pools/{pool}
        </button>
      </form>
    </section>

    <section class="card">
      <h2>Upsert director catalog resource</h2>
      <form id="director-catalog-form">
        <label for="director-catalog-deployment-id">Deployment ID</label>
        <input id="director-catalog-deployment-id" name="deployment_id" value="prod">

        <label for="director-catalog-director-name">Director name</label>
        <input id="director-catalog-director-name" name="director_name" value="bareos-dir">

        <label for="director-catalog-catalog-name">Catalog name</label>
        <input id="director-catalog-catalog-name" name="catalog_name" value="managed-catalog">

        <label for="director-catalog-db-name">DbName</label>
        <input id="director-catalog-db-name" name="db_name"
               value="bareos_catalog">

        <label for="director-catalog-db-user">DbUser</label>
        <input id="director-catalog-db-user" name="db_user" value="bareos">

        <label for="director-catalog-db-password">DbPassword</label>
        <input id="director-catalog-db-password" name="db_password"
               placeholder="cleartext or [md5]hash">

        <label for="director-catalog-db-address">DbAddress</label>
        <input id="director-catalog-db-address" name="db_address"
               placeholder="localhost">

        <label for="director-catalog-db-port">DbPort</label>
        <input id="director-catalog-db-port" name="db_port" type="number" min="0">

        <label for="director-catalog-db-socket">DbSocket</label>
        <input id="director-catalog-db-socket" name="db_socket">

        <label class="checkbox-label" for="director-catalog-reconnect">
          <input id="director-catalog-reconnect" name="reconnect"
                 type="checkbox" checked>
          Reconnect
        </label>

        <label class="checkbox-label" for="director-catalog-exit-on-fatal">
          <input id="director-catalog-exit-on-fatal" name="exit_on_fatal"
                 type="checkbox">
          Exit on fatal
        </label>

        <label for="director-catalog-min-connections">MinConnections</label>
        <input id="director-catalog-min-connections" name="min_connections"
               type="number" min="0">

        <label for="director-catalog-max-connections">MaxConnections</label>
        <input id="director-catalog-max-connections" name="max_connections"
               type="number" min="0">

        <label for="director-catalog-inc-connections">IncConnections</label>
        <input id="director-catalog-inc-connections" name="inc_connections"
               type="number" min="0">

        <label for="director-catalog-idle-timeout">IdleTimeout</label>
        <input id="director-catalog-idle-timeout" name="idle_timeout"
               type="number" min="0">

        <label for="director-catalog-validate-timeout">ValidateTimeout</label>
        <input id="director-catalog-validate-timeout" name="validate_timeout"
               type="number" min="0">

        <label for="director-catalog-description">Description</label>
        <input id="director-catalog-description" name="description"
               placeholder="Managed catalog resource">

        <button type="submit">
          PUT /v1/deployments/{id}/directors/{director}/catalogs/{catalog}
        </button>
        <button type="button" id="director-catalog-delete-button">
          DELETE /v1/deployments/{id}/directors/{director}/catalogs/{catalog}
        </button>
      </form>
    </section>

    <section class="card">
      <h2>Upsert director schedule resource</h2>
      <form id="director-schedule-form">
        <label for="director-schedule-deployment-id">Deployment ID</label>
        <input id="director-schedule-deployment-id" name="deployment_id" value="prod">

        <label for="director-schedule-director-name">Director name</label>
        <input id="director-schedule-director-name" name="director_name" value="bareos-dir">

        <label for="director-schedule-schedule-name">Schedule name</label>
        <input id="director-schedule-schedule-name" name="schedule_name" value="managed-schedule">

        <label for="director-schedule-description">Description</label>
        <input id="director-schedule-description" name="description"
               placeholder="Managed schedule resource">

        <label class="checkbox-label" for="director-schedule-enabled">
          <input id="director-schedule-enabled" name="enabled"
                 type="checkbox" checked>
          Enabled
        </label>

        <label for="director-schedule-runs">Run entries</label>
        <textarea id="director-schedule-runs" name="run_entries"
                  rows="4"
                  placeholder="Level=Full monthly 1st sat at 21:00"></textarea>

        <button type="submit">
          PUT /v1/deployments/{id}/directors/{director}/schedules/{schedule}
        </button>
        <button type="button" id="director-schedule-delete-button">
          DELETE /v1/deployments/{id}/directors/{director}/schedules/{schedule}
        </button>
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
      <h2>Deployment clients</h2>
      <p class="contents-meta">
        Shows imported client config roots from
        <code>GET /v1/deployments/{id}/clients</code>.
      </p>
      <div id="deployment-clients" class="contents-panel">
        <p class="contents-empty">Load a deployment to see its clients.</p>
      </div>
    </section>

    <section class="card">
      <h2>Deployment git status</h2>
      <p class="contents-meta">
        Shows repository state for the selected deployment.
      </p>
      <div id="deployment-git-status" class="contents-panel">
        <p class="contents-empty">Load a deployment to see repository status.</p>
      </div>
    </section>

    <section class="card">
      <h2>Deployment diff preview</h2>
      <p class="contents-meta">
        Shows the current git diff and untracked files for the selected deployment.
      </p>
      <div id="deployment-diff-preview" class="contents-panel">
        <p class="contents-empty">Load a deployment to see repository changes.</p>
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
         <li>Default persisted service data lives under
             <code>__DEFAULT_STORAGE_BASE_PATH__</code>.
         </li>
       </ul>
     </section>
   </div>

  <script>
    const responsePanel = document.getElementById('response-panel');
    const deploymentContentsPanel = document.getElementById('deployment-contents');
    const deploymentJobsPanel = document.getElementById('deployment-jobs');
    const deploymentImportsPanel = document.getElementById('deployment-imports');
    const deploymentClientsPanel = document.getElementById('deployment-clients');
    const deploymentGitStatusPanel = document.getElementById('deployment-git-status');
    const deploymentDiffPreviewPanel = document.getElementById('deployment-diff-preview');
    const jobTypeField = document.getElementById('job-type');
    const jobSourcePathField = document.getElementById('job-source-path');
    const jobCommitMessageField = document.getElementById('job-commit-message');

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
          const nestedDetails = (resource.nested_details ?? []).map((detail) => {
            const summary = detail.summary
              ? ` "${escapeHtml(detail.summary)}"`
              : '';
            const source = detail.source
              ? ` <code>${escapeHtml(detail.source.file)}</code>:${detail.source.line}`
              : '';
            const values = (detail.values ?? []).map((value) => {
              const valueSource = value.source
                ? ` <code>${escapeHtml(value.source.file)}</code>:${value.source.line}`
                : '';
              return `<li><strong>${escapeHtml(value.name)}:</strong> ${escapeHtml(value.value)}${valueSource}</li>`;
            }).join('');
            const valuesBlock = values
              ? `<ul class="detail-value-list">${values}</ul>`
              : '';
            return `<li><strong>${escapeHtml(detail.kind)}</strong>${summary}${source}${valuesBlock}</li>`;
          }).join('');
          const relations = (resource.relations ?? []).map((relation) =>
            `<li>${escapeHtml(relation.directive)} → <code>${escapeHtml(relation.target_type)}</code>: ${escapeHtml(relation.target_name)}</li>`
          ).join('');
          const externalRelations = (resource.external_relations ?? []).map((relation) =>
            `<li>${escapeHtml(relation.relation)} → <code>${escapeHtml(relation.target_component)}</code>/<code>${escapeHtml(relation.target_type)}</code>: ${escapeHtml(relation.target_name)} (${relation.matched ? 'matched' : 'missing'})</li>`
          ).join('');
          const source = resource.source
            ? `<p class="resource-meta"><strong>Source:</strong> <code>${escapeHtml(resource.source.file)}</code>:${resource.source.line}</p>`
            : '';
          const ownership = `<p class="resource-meta"><strong>Ownership:</strong> ${resource.managed ? 'service-managed' : 'imported-only'}</p>`;
          const nestedDetailsBlock = nestedDetails
            ? `<p class="resource-meta"><strong>Nested details</strong></p><ul class="detail-list">${nestedDetails}</ul>`
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
                ${ownership}
                <p class="resource-meta">
                  Directives: ${resource.directives?.length ?? 0};
                  Nested details: ${resource.nested_details?.length ?? 0};
                  Relations: ${resource.relations?.length ?? 0};
                  External relations: ${resource.external_relations?.length ?? 0}
                </p>
                <ul class="directive-list">${directives}</ul>
                ${nestedDetailsBlock}
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
               <strong>Managed resources:</strong> ${config.managed_resource_count ?? 0};
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
        const commitMessage = job.commit_message
          ? `<p><strong>Commit:</strong> ${escapeHtml(job.commit_message)}</p>`
          : '';
        return `
          <div class="job-item">
            <h3>${escapeHtml(job.type)}</h3>
            <p><strong>Status:</strong> ${escapeHtml(job.status)}; 
               <strong>ID:</strong> <code>${escapeHtml(job.id)}</code></p>
            <p><strong>Updated:</strong> ${escapeHtml(job.updated_at)}</p>
            ${commitMessage}
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

    function renderDeploymentClients(deploymentId, clients) {
      if (!deploymentId) {
        deploymentClientsPanel.innerHTML =
          '<p class="contents-empty">Enter a deployment ID first.</p>';
        return;
      }

      if (!clients.length) {
        deploymentClientsPanel.innerHTML = `
          <p class="contents-meta"><strong>${escapeHtml(deploymentId)}</strong>:
          no client config roots found.</p>`;
        return;
      }

      const items = clients.map((client) => `
        <div class="import-item">
          <h3>${escapeHtml(client.name)}</h3>
          <p><strong>Component:</strong> ${escapeHtml(client.component)}</p>
          <p><strong>Path:</strong> <code>${escapeHtml(client.path)}</code></p>
          <p><strong>Detail:</strong> <code>/v1/deployments/${encodeURIComponent(deploymentId)}/clients/${encodeURIComponent(client.name)}</code></p>
        </div>`).join('');

      deploymentClientsPanel.innerHTML = `
        <p class="contents-meta"><strong>${escapeHtml(deploymentId)}</strong>:
        ${clients.length} client config root(s)</p>
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

    async function loadDeploymentClients(deploymentId) {
      if (!deploymentId) {
        deploymentClientsPanel.innerHTML =
          '<p class="contents-empty">Enter a deployment ID first.</p>';
        return;
      }

      const { response, text } = await request(
        'GET', `/v1/deployments/${deploymentId}/clients`);
      if (!response.ok) {
        deploymentClientsPanel.innerHTML =
          '<p class="contents-empty">Could not load deployment clients.</p>';
        return;
      }

      const document = JSON.parse(text);
      renderDeploymentClients(deploymentId, document.clients ?? []);
    }

    function renderDeploymentGitStatus(deploymentId, status) {
      if (!deploymentId) {
        deploymentGitStatusPanel.innerHTML =
          '<p class="contents-empty">Enter a deployment ID first.</p>';
        return;
      }

      if (!status.initialized) {
        deploymentGitStatusPanel.innerHTML = `
          <p class="contents-meta"><strong>${escapeHtml(deploymentId)}</strong>:
          repository is not initialized.</p>`;
        return;
      }

      const entries = (status.entries ?? []).map((entry) =>
        `<li>${escapeHtml(entry)}</li>`
      ).join('');
      const entryBlock = entries
        ? `<ul class="git-status-list">${entries}</ul>`
        : '<p class="contents-meta">Working tree is clean.</p>';
      deploymentGitStatusPanel.innerHTML = `
        <p class="contents-meta"><strong>${escapeHtml(deploymentId)}</strong>:
        branch <code>${escapeHtml(status.branch || 'unknown')}</code>;
        clean: ${status.clean ? 'yes' : 'no'};
        staged changes: ${status.has_staged_changes ? 'yes' : 'no'};
        untracked files: ${status.has_untracked_files ? 'yes' : 'no'}</p>
        ${entryBlock}`;
    }

    async function loadDeploymentGitStatus(deploymentId) {
      if (!deploymentId) {
        deploymentGitStatusPanel.innerHTML =
          '<p class="contents-empty">Enter a deployment ID first.</p>';
        return;
      }

      const { response, text } = await request(
        'GET', `/v1/deployments/${deploymentId}/git-status`);
      if (!response.ok) {
        deploymentGitStatusPanel.innerHTML =
          '<p class="contents-empty">Could not load deployment git status.</p>';
        return;
      }

      const document = JSON.parse(text);
      renderDeploymentGitStatus(deploymentId, document.git_status ?? {});
    }

    function renderDeploymentDiffPreview(deploymentId, preview) {
      if (!deploymentId) {
        deploymentDiffPreviewPanel.innerHTML =
          '<p class="contents-empty">Enter a deployment ID first.</p>';
        return;
      }

      if (!preview.initialized) {
        deploymentDiffPreviewPanel.innerHTML = `
          <p class="contents-meta"><strong>${escapeHtml(deploymentId)}</strong>:
          repository is not initialized.</p>`;
        return;
      }

      const untracked = (preview.untracked_files ?? []).map((path) =>
        `<li>${escapeHtml(path)}</li>`
      ).join('');
      const untrackedBlock = untracked
        ? `<p class="contents-meta"><strong>Untracked files</strong></p><ul class="git-status-list">${untracked}</ul>`
        : '';
      const diffText = preview.diff
        ? escapeHtml(preview.diff)
        : 'Working tree matches HEAD.';
      deploymentDiffPreviewPanel.innerHTML = `
        <p class="contents-meta"><strong>${escapeHtml(deploymentId)}</strong>:
        changes present: ${preview.has_changes ? 'yes' : 'no'}</p>
        ${untrackedBlock}
        <pre class="diff-preview">${diffText}</pre>`;
    }

    async function loadDeploymentDiffPreview(deploymentId) {
      if (!deploymentId) {
        deploymentDiffPreviewPanel.innerHTML =
          '<p class="contents-empty">Enter a deployment ID first.</p>';
        return;
      }

      const { response, text } = await request(
        'GET', `/v1/deployments/${deploymentId}/diff`);
      if (!response.ok) {
        deploymentDiffPreviewPanel.innerHTML =
          '<p class="contents-empty">Could not load deployment diff preview.</p>';
        return;
      }

      const document = JSON.parse(text);
      renderDeploymentDiffPreview(deploymentId, document.diff_preview ?? {});
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
      await loadDeploymentClients(deploymentId);
      await loadDeploymentGitStatus(deploymentId);
      await loadDeploymentDiffPreview(deploymentId);
    }

    async function followJob(jobId, deploymentId) {
      deploymentContentsPanel.innerHTML =
        `<p class="contents-empty">Waiting for job <code>${escapeHtml(jobId)}</code> to finish...</p>`;
      deploymentJobsPanel.innerHTML =
        `<p class="contents-empty">Waiting for job <code>${escapeHtml(jobId)}</code> to finish...</p>`;
      deploymentImportsPanel.innerHTML =
        `<p class="contents-empty">Waiting for job <code>${escapeHtml(jobId)}</code> to finish...</p>`;
      deploymentClientsPanel.innerHTML =
        `<p class="contents-empty">Waiting for job <code>${escapeHtml(jobId)}</code> to finish...</p>`;
      deploymentGitStatusPanel.innerHTML =
        `<p class="contents-empty">Waiting for job <code>${escapeHtml(jobId)}</code> to finish...</p>`;
      deploymentDiffPreviewPanel.innerHTML =
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
      deploymentClientsPanel.innerHTML =
        '<p class="contents-empty">Job is still running. Refresh deployment clients in a moment.</p>';
      deploymentGitStatusPanel.innerHTML =
        '<p class="contents-empty">Job is still running. Refresh deployment git status in a moment.</p>';
      deploymentDiffPreviewPanel.innerHTML =
        '<p class="contents-empty">Job is still running. Refresh deployment diff preview in a moment.</p>';
    }

    document.getElementById('deployment-form').addEventListener(
      'submit',
      async (event) => {
        event.preventDefault();
        const form = new FormData(event.target);
        const payload = Object.fromEntries(form);
        document.getElementById('job-deployment-id').value = payload.id;
        document.getElementById('deployment-inspect-id').value = payload.id;
        document.getElementById('client-stub-deployment-id').value = payload.id;
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
        payload.commit_message = (payload.commit_message ?? '').trim();
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
        if (payload.type !== 'commit_deployment_repo') {
          delete payload.commit_message;
        } else if (!payload.commit_message) {
          payload.commit_message = 'Update deployment repository';
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
    jobCommitMessageField.addEventListener('input', () => {
      if (jobCommitMessageField.value.trim() && !jobSourcePathField.value.trim()) {
        jobTypeField.value = 'commit_deployment_repo';
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
    document.getElementById('client-stub-form').addEventListener(
      'submit',
      async (event) => {
        event.preventDefault();
        const form = new FormData(event.target);
        const deploymentId = String(form.get('deployment_id') ?? '').trim();
        const clientName = String(form.get('client_name') ?? '').trim();
        const directorName = String(form.get('director_name') ?? '').trim();
        const payload = {
          description: String(form.get('description') ?? '').trim(),
        };
        if (!payload.description) {
          delete payload.description;
        }
        const { response } = await request(
          'PUT',
          `/v1/deployments/${encodeURIComponent(deploymentId)}/clients/${encodeURIComponent(clientName)}/directors/${encodeURIComponent(directorName)}`,
          payload);
        if (response.ok) {
          document.getElementById('deployment-inspect-id').value = deploymentId;
          await loadDeploymentContents(deploymentId);
        }
      });
    document.getElementById('director-client-form').addEventListener(
      'submit',
      async (event) => {
        event.preventDefault();
        const form = new FormData(event.target);
        const deploymentId = String(form.get('deployment_id') ?? '').trim();
        const directorName = String(form.get('director_name') ?? '').trim();
        const clientName = String(form.get('client_name') ?? '').trim();
        const payload = {
          address: String(form.get('address') ?? '').trim(),
          password: String(form.get('password') ?? '').trim(),
          description: String(form.get('description') ?? '').trim(),
        };
        if (!payload.address) {
          delete payload.address;
        }
        if (!payload.password) {
          delete payload.password;
        }
        if (!payload.description) {
          delete payload.description;
        }
        const { response } = await request(
          'PUT',
          `/v1/deployments/${encodeURIComponent(deploymentId)}/directors/${encodeURIComponent(directorName)}/clients/${encodeURIComponent(clientName)}`,
          payload);
        if (response.ok) {
          document.getElementById('deployment-inspect-id').value = deploymentId;
          document.getElementById('client-stub-client-name').value = clientName;
          document.getElementById('client-stub-director-name').value = directorName;
          await loadDeploymentContents(deploymentId);
        }
      });
    document.getElementById('director-client-delete-button').addEventListener(
      'click',
      async () => {
        const form = new FormData(document.getElementById('director-client-form'));
        const deploymentId = String(form.get('deployment_id') ?? '').trim();
        const directorName = String(form.get('director_name') ?? '').trim();
        const clientName = String(form.get('client_name') ?? '').trim();
        const { response } = await request(
          'DELETE',
          `/v1/deployments/${encodeURIComponent(deploymentId)}/directors/${encodeURIComponent(directorName)}/clients/${encodeURIComponent(clientName)}`);
        if (response.ok) {
          document.getElementById('deployment-inspect-id').value = deploymentId;
          await loadDeploymentContents(deploymentId);
        }
      });
    document.getElementById('director-storage-form').addEventListener(
      'submit',
      async (event) => {
        event.preventDefault();
        const form = new FormData(event.target);
        const deploymentId = String(form.get('deployment_id') ?? '').trim();
        const directorName = String(form.get('director_name') ?? '').trim();
        const storageName = String(form.get('storage_name') ?? '').trim();
        const payload = {
          address: String(form.get('address') ?? '').trim(),
          port: String(form.get('port') ?? '').trim(),
          password: String(form.get('password') ?? '').trim(),
          device: String(form.get('device') ?? '').trim(),
          media_type: String(form.get('media_type') ?? '').trim(),
          archive_device: String(form.get('archive_device') ?? '').trim(),
          device_type: String(form.get('device_type') ?? '').trim(),
          description: String(form.get('description') ?? '').trim(),
        };
        if (!payload.address) {
          delete payload.address;
        }
        if (!payload.port) {
          delete payload.port;
        } else {
          payload.port = Number(payload.port);
        }
        if (!payload.password) {
          delete payload.password;
        }
        if (!payload.device) {
          delete payload.device;
        }
        if (!payload.media_type) {
          delete payload.media_type;
        }
        if (!payload.archive_device) {
          delete payload.archive_device;
        }
        if (!payload.device_type) {
          delete payload.device_type;
        }
        if (!payload.description) {
          delete payload.description;
        }
        const { response } = await request(
          'PUT',
          `/v1/deployments/${encodeURIComponent(deploymentId)}/directors/${encodeURIComponent(directorName)}/storages/${encodeURIComponent(storageName)}`,
          payload);
        if (response.ok) {
          document.getElementById('deployment-inspect-id').value = deploymentId;
          await loadDeploymentContents(deploymentId);
        }
      });
    {
      const storageNameInput = document.getElementById('director-storage-storage-name');
      const deviceInput = document.getElementById('director-storage-device');
      let autoDeviceName = deviceInput.value;
      storageNameInput.addEventListener('input', () => {
        if (deviceInput.value === autoDeviceName || !deviceInput.value.trim()) {
          autoDeviceName = storageNameInput.value.trim();
          deviceInput.value = autoDeviceName;
        }
      });
      deviceInput.addEventListener('input', () => {
        if (!deviceInput.value.trim()) {
          autoDeviceName = storageNameInput.value.trim();
        }
      });
    }
    document.getElementById('director-storage-delete-button').addEventListener(
      'click',
      async () => {
        const form = new FormData(document.getElementById('director-storage-form'));
        const deploymentId = String(form.get('deployment_id') ?? '').trim();
        const directorName = String(form.get('director_name') ?? '').trim();
        const storageName = String(form.get('storage_name') ?? '').trim();
        const { response } = await request(
          'DELETE',
          `/v1/deployments/${encodeURIComponent(deploymentId)}/directors/${encodeURIComponent(directorName)}/storages/${encodeURIComponent(storageName)}`);
        if (response.ok) {
          document.getElementById('deployment-inspect-id').value = deploymentId;
          await loadDeploymentContents(deploymentId);
        }
      });
    document.getElementById('director-console-form').addEventListener(
      'submit',
      async (event) => {
        event.preventDefault();
        const form = new FormData(event.target);
        const deploymentId = String(form.get('deployment_id') ?? '').trim();
        const directorName = String(form.get('director_name') ?? '').trim();
        const consoleName = String(form.get('console_name') ?? '').trim();
        const payload = {
          password: String(form.get('password') ?? '').trim(),
          description: String(form.get('description') ?? '').trim(),
          use_pam_authentication: document.getElementById('director-console-use-pam').checked,
        };
        if (!payload.password) {
          delete payload.password;
        }
        if (!payload.description) {
          delete payload.description;
        }
        const { response } = await request(
          'PUT',
          `/v1/deployments/${encodeURIComponent(deploymentId)}/directors/${encodeURIComponent(directorName)}/consoles/${encodeURIComponent(consoleName)}`,
          payload);
        if (response.ok) {
          document.getElementById('deployment-inspect-id').value = deploymentId;
          await loadDeploymentContents(deploymentId);
        }
      });
    document.getElementById('director-console-delete-button').addEventListener(
      'click',
      async () => {
        const form = new FormData(document.getElementById('director-console-form'));
        const deploymentId = String(form.get('deployment_id') ?? '').trim();
        const directorName = String(form.get('director_name') ?? '').trim();
        const consoleName = String(form.get('console_name') ?? '').trim();
        const { response } = await request(
          'DELETE',
          `/v1/deployments/${encodeURIComponent(deploymentId)}/directors/${encodeURIComponent(directorName)}/consoles/${encodeURIComponent(consoleName)}`);
        if (response.ok) {
          document.getElementById('deployment-inspect-id').value = deploymentId;
          await loadDeploymentContents(deploymentId);
        }
      });
    document.getElementById('director-user-form').addEventListener(
      'submit',
      async (event) => {
        event.preventDefault();
        const form = new FormData(event.target);
        const deploymentId = String(form.get('deployment_id') ?? '').trim();
        const directorName = String(form.get('director_name') ?? '').trim();
        const userName = String(form.get('user_name') ?? '').trim();
        const payload = {
          description: String(form.get('description') ?? '').trim(),
        };
        if (!payload.description) {
          delete payload.description;
        }
        const { response } = await request(
          'PUT',
          `/v1/deployments/${encodeURIComponent(deploymentId)}/directors/${encodeURIComponent(directorName)}/users/${encodeURIComponent(userName)}`,
          payload);
        if (response.ok) {
          document.getElementById('deployment-inspect-id').value = deploymentId;
          await loadDeploymentContents(deploymentId);
        }
      });
    document.getElementById('director-user-delete-button').addEventListener(
      'click',
      async () => {
        const form = new FormData(document.getElementById('director-user-form'));
        const deploymentId = String(form.get('deployment_id') ?? '').trim();
        const directorName = String(form.get('director_name') ?? '').trim();
        const userName = String(form.get('user_name') ?? '').trim();
        const { response } = await request(
          'DELETE',
          `/v1/deployments/${encodeURIComponent(deploymentId)}/directors/${encodeURIComponent(directorName)}/users/${encodeURIComponent(userName)}`);
        if (response.ok) {
          document.getElementById('deployment-inspect-id').value = deploymentId;
          await loadDeploymentContents(deploymentId);
        }
      });
    document.getElementById('director-profile-form').addEventListener(
      'submit',
      async (event) => {
        event.preventDefault();
        const form = new FormData(event.target);
        const deploymentId = String(form.get('deployment_id') ?? '').trim();
        const directorName = String(form.get('director_name') ?? '').trim();
        const profileName = String(form.get('profile_name') ?? '').trim();
        const payload = {
          description: String(form.get('description') ?? '').trim(),
        };
        if (!payload.description) {
          delete payload.description;
        }
        const { response } = await request(
          'PUT',
          `/v1/deployments/${encodeURIComponent(deploymentId)}/directors/${encodeURIComponent(directorName)}/profiles/${encodeURIComponent(profileName)}`,
          payload);
        if (response.ok) {
          document.getElementById('deployment-inspect-id').value = deploymentId;
          await loadDeploymentContents(deploymentId);
        }
      });
    document.getElementById('director-profile-delete-button').addEventListener(
      'click',
      async () => {
        const form = new FormData(document.getElementById('director-profile-form'));
        const deploymentId = String(form.get('deployment_id') ?? '').trim();
        const directorName = String(form.get('director_name') ?? '').trim();
        const profileName = String(form.get('profile_name') ?? '').trim();
        const { response } = await request(
          'DELETE',
          `/v1/deployments/${encodeURIComponent(deploymentId)}/directors/${encodeURIComponent(directorName)}/profiles/${encodeURIComponent(profileName)}`);
        if (response.ok) {
          document.getElementById('deployment-inspect-id').value = deploymentId;
          await loadDeploymentContents(deploymentId);
        }
      });
    document.getElementById('director-pool-form').addEventListener(
      'submit',
      async (event) => {
        event.preventDefault();
        const form = new FormData(event.target);
        const deploymentId = String(form.get('deployment_id') ?? '').trim();
        const directorName = String(form.get('director_name') ?? '').trim();
        const poolName = String(form.get('pool_name') ?? '').trim();
        const payload = {
          pool_type: String(form.get('pool_type') ?? '').trim(),
          label_format: String(form.get('label_format') ?? '').trim(),
          maximum_volumes: String(form.get('maximum_volumes') ?? '').trim(),
          maximum_volume_bytes: String(form.get('maximum_volume_bytes') ?? '').trim(),
          volume_retention: String(form.get('volume_retention') ?? '').trim(),
          auto_prune: document.getElementById('director-pool-auto-prune').checked,
          recycle: document.getElementById('director-pool-recycle').checked,
          description: String(form.get('description') ?? '').trim(),
        };
        if (!payload.pool_type) {
          delete payload.pool_type;
        }
        if (!payload.label_format) {
          delete payload.label_format;
        }
        if (!payload.maximum_volumes) {
          delete payload.maximum_volumes;
        } else {
          payload.maximum_volumes = Number(payload.maximum_volumes);
        }
        if (!payload.maximum_volume_bytes) {
          delete payload.maximum_volume_bytes;
        } else {
          payload.maximum_volume_bytes = Number(payload.maximum_volume_bytes);
        }
        if (!payload.volume_retention) {
          delete payload.volume_retention;
        } else {
          payload.volume_retention = Number(payload.volume_retention);
        }
        if (!payload.description) {
          delete payload.description;
        }
        const { response } = await request(
          'PUT',
          `/v1/deployments/${encodeURIComponent(deploymentId)}/directors/${encodeURIComponent(directorName)}/pools/${encodeURIComponent(poolName)}`,
          payload);
        if (response.ok) {
          document.getElementById('deployment-inspect-id').value = deploymentId;
          await loadDeploymentContents(deploymentId);
        }
      });
    document.getElementById('director-pool-delete-button').addEventListener(
      'click',
      async () => {
        const form = new FormData(document.getElementById('director-pool-form'));
        const deploymentId = String(form.get('deployment_id') ?? '').trim();
        const directorName = String(form.get('director_name') ?? '').trim();
        const poolName = String(form.get('pool_name') ?? '').trim();
        const { response } = await request(
          'DELETE',
          `/v1/deployments/${encodeURIComponent(deploymentId)}/directors/${encodeURIComponent(directorName)}/pools/${encodeURIComponent(poolName)}`);
        if (response.ok) {
          document.getElementById('deployment-inspect-id').value = deploymentId;
          await loadDeploymentContents(deploymentId);
        }
      });
    document.getElementById('director-catalog-form').addEventListener(
      'submit',
      async (event) => {
        event.preventDefault();
        const form = new FormData(event.target);
        const deploymentId = String(form.get('deployment_id') ?? '').trim();
        const directorName = String(form.get('director_name') ?? '').trim();
        const catalogName = String(form.get('catalog_name') ?? '').trim();
        const payload = {
          db_address: String(form.get('db_address') ?? '').trim(),
          db_port: String(form.get('db_port') ?? '').trim(),
          db_socket: String(form.get('db_socket') ?? '').trim(),
          db_password: String(form.get('db_password') ?? '').trim(),
          db_user: String(form.get('db_user') ?? '').trim(),
          db_name: String(form.get('db_name') ?? '').trim(),
          reconnect: document.getElementById('director-catalog-reconnect').checked,
          exit_on_fatal: document.getElementById('director-catalog-exit-on-fatal').checked,
          min_connections: String(form.get('min_connections') ?? '').trim(),
          max_connections: String(form.get('max_connections') ?? '').trim(),
          inc_connections: String(form.get('inc_connections') ?? '').trim(),
          idle_timeout: String(form.get('idle_timeout') ?? '').trim(),
          validate_timeout: String(form.get('validate_timeout') ?? '').trim(),
          description: String(form.get('description') ?? '').trim(),
        };
        if (!payload.db_address) {
          delete payload.db_address;
        }
        if (!payload.db_port) {
          delete payload.db_port;
        } else {
          payload.db_port = Number(payload.db_port);
        }
        if (!payload.db_socket) {
          delete payload.db_socket;
        }
        if (!payload.db_password) {
          delete payload.db_password;
        }
        if (!payload.db_user) {
          delete payload.db_user;
        }
        if (!payload.db_name) {
          delete payload.db_name;
        }
        if (!payload.min_connections) {
          delete payload.min_connections;
        } else {
          payload.min_connections = Number(payload.min_connections);
        }
        if (!payload.max_connections) {
          delete payload.max_connections;
        } else {
          payload.max_connections = Number(payload.max_connections);
        }
        if (!payload.inc_connections) {
          delete payload.inc_connections;
        } else {
          payload.inc_connections = Number(payload.inc_connections);
        }
        if (!payload.idle_timeout) {
          delete payload.idle_timeout;
        } else {
          payload.idle_timeout = Number(payload.idle_timeout);
        }
        if (!payload.validate_timeout) {
          delete payload.validate_timeout;
        } else {
          payload.validate_timeout = Number(payload.validate_timeout);
        }
        if (!payload.description) {
          delete payload.description;
        }
        const { response } = await request(
          'PUT',
          `/v1/deployments/${encodeURIComponent(deploymentId)}/directors/${encodeURIComponent(directorName)}/catalogs/${encodeURIComponent(catalogName)}`,
          payload);
        if (response.ok) {
          document.getElementById('deployment-inspect-id').value = deploymentId;
          await loadDeploymentContents(deploymentId);
        }
      });
    document.getElementById('director-catalog-delete-button').addEventListener(
      'click',
      async () => {
        const form = new FormData(document.getElementById('director-catalog-form'));
        const deploymentId = String(form.get('deployment_id') ?? '').trim();
        const directorName = String(form.get('director_name') ?? '').trim();
        const catalogName = String(form.get('catalog_name') ?? '').trim();
        const { response } = await request(
          'DELETE',
          `/v1/deployments/${encodeURIComponent(deploymentId)}/directors/${encodeURIComponent(directorName)}/catalogs/${encodeURIComponent(catalogName)}`);
        if (response.ok) {
          document.getElementById('deployment-inspect-id').value = deploymentId;
          await loadDeploymentContents(deploymentId);
        }
      });
    document.getElementById('director-schedule-form').addEventListener(
      'submit',
      async (event) => {
        event.preventDefault();
        const form = new FormData(event.target);
        const deploymentId = String(form.get('deployment_id') ?? '').trim();
        const directorName = String(form.get('director_name') ?? '').trim();
        const scheduleName = String(form.get('schedule_name') ?? '').trim();
        const rawRuns = String(form.get('run_entries') ?? '');
        const runEntries = rawRuns.split('\n')
          .map((line) => line.trim())
          .filter((line) => line.length > 0);
        const payload = {
          description: String(form.get('description') ?? '').trim(),
          enabled: document.getElementById('director-schedule-enabled').checked,
          run_entries: runEntries,
        };
        if (!payload.description) {
          delete payload.description;
        }
        if (payload.run_entries.length === 0) {
          delete payload.run_entries;
        }
        const { response } = await request(
          'PUT',
          `/v1/deployments/${encodeURIComponent(deploymentId)}/directors/${encodeURIComponent(directorName)}/schedules/${encodeURIComponent(scheduleName)}`,
          payload);
        if (response.ok) {
          document.getElementById('deployment-inspect-id').value = deploymentId;
          await loadDeploymentContents(deploymentId);
        }
      });
    document.getElementById('director-schedule-delete-button').addEventListener(
      'click',
      async () => {
        const form = new FormData(document.getElementById('director-schedule-form'));
        const deploymentId = String(form.get('deployment_id') ?? '').trim();
        const directorName = String(form.get('director_name') ?? '').trim();
        const scheduleName = String(form.get('schedule_name') ?? '').trim();
        const { response } = await request(
          'DELETE',
          `/v1/deployments/${encodeURIComponent(deploymentId)}/directors/${encodeURIComponent(directorName)}/schedules/${encodeURIComponent(scheduleName)}`);
        if (response.ok) {
          document.getElementById('deployment-inspect-id').value = deploymentId;
          await loadDeploymentContents(deploymentId);
        }
      });

    request('GET', '/v1/health');
  </script>
</body>
</html>
)HTML";

std::string ReplaceAll(std::string text,
                       std::string_view needle,
                       std::string_view replacement)
{
  size_t position = 0;
  while ((position = text.find(needle, position)) != std::string::npos) {
    text.replace(position, needle.size(), replacement);
    position += replacement.size();
  }
  return text;
}

std::string BuildTestUiHtml()
{
  auto html = std::string{kTestUiHtmlTemplate};
  html = ReplaceAll(html, "__DEFAULT_DEPLOYMENT_REPOSITORY_PATH__",
                    DefaultDeploymentRepositoryPath("prod").string());
  html = ReplaceAll(html, "__DEFAULT_STORAGE_BASE_PATH__",
                    DefaultStorageBasePath().string());
  return html;
}

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
  if (record.commit_message) {
    json_object_set_new(object.get(), "commit_message",
                        json_string(record.commit_message->c_str()));
  } else {
    json_object_set_new(object.get(), "commit_message", json_null());
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
  json_object_set_new(object.get(), "job_id",
                      json_string(record.job_id.c_str()));
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

void SetDeploymentGitStatus(json_t* object,
                            const DeploymentGitStatusRecord& status)
{
  auto git_status = MakeJson(json_object());
  auto entries = MakeJson(json_array());
  json_object_set_new(git_status.get(), "initialized",
                      json_boolean(status.initialized));
  json_object_set_new(git_status.get(), "branch",
                      json_string(status.branch.c_str()));
  json_object_set_new(git_status.get(), "clean", json_boolean(status.clean));
  json_object_set_new(git_status.get(), "has_staged_changes",
                      json_boolean(status.has_staged_changes));
  json_object_set_new(git_status.get(), "has_untracked_files",
                      json_boolean(status.has_untracked_files));
  for (const auto& entry : status.entries) {
    json_array_append_new(entries.get(), json_string(entry.c_str()));
  }
  json_object_set_new(git_status.get(), "entries", entries.release());
  json_object_set_new(object, "git_status", git_status.release());
}

void SetDeploymentDiffPreview(json_t* object,
                              const DeploymentDiffPreviewRecord& preview)
{
  auto diff_preview = MakeJson(json_object());
  auto untracked = MakeJson(json_array());
  json_object_set_new(diff_preview.get(), "initialized",
                      json_boolean(preview.initialized));
  json_object_set_new(diff_preview.get(), "has_changes",
                      json_boolean(preview.has_changes));
  json_object_set_new(diff_preview.get(), "diff",
                      json_string(preview.diff.c_str()));
  for (const auto& entry : preview.untracked_files) {
    json_array_append_new(untracked.get(), json_string(entry.c_str()));
  }
  json_object_set_new(diff_preview.get(), "untracked_files",
                      untracked.release());
  json_object_set_new(object, "diff_preview", diff_preview.release());
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
  json_object_set_new(object.get(), "name",
                      json_string(directive.name.c_str()));
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

std::filesystem::path RepositoryRootFromConfigPath(
    const std::filesystem::path& config_root)
{
  return config_root.parent_path().parent_path();
}

OperationResult<std::set<std::string>> LoadManagedPathsForRepository(
    const std::filesystem::path& repository_root)
{
  const auto ownership_path = RepositoryLayout::OwnershipPath(repository_root);
  if (!std::filesystem::exists(ownership_path)) {
    return {.value = std::set<std::string>{}};
  }

  json_error_t json_error{};
  auto root = MakeJson(json_load_file(ownership_path.c_str(), 0, &json_error));
  if (!root) {
    return {.error = "failed to parse ownership file '"
                     + ownership_path.string() + "': " + json_error.text};
  }
  if (!json_is_object(root.get())) {
    return {.error = "ownership file '" + ownership_path.string()
                     + "' must contain a JSON object."};
  }

  std::set<std::string> managed_paths;
  auto* managed_files = json_object_get(root.get(), "managed_files");
  if (!managed_files) { return {.value = std::move(managed_paths)}; }
  if (!json_is_array(managed_files)) {
    return {.error = "ownership file '" + ownership_path.string()
                     + "' contains a non-array 'managed_files' field."};
  }

  size_t index = 0;
  json_t* entry = nullptr;
  json_array_foreach(managed_files, index, entry)
  {
    if (!json_is_string(entry)) {
      return {.error = "ownership file '" + ownership_path.string()
                       + "' contains a non-string managed file entry."};
    }
    managed_paths.emplace(json_string_value(entry));
  }

  return {.value = std::move(managed_paths)};
}

bool IsManagedSourcePath(const std::set<std::string>& managed_paths,
                         const std::filesystem::path& repository_root,
                         const std::filesystem::path& source_path)
{
  std::error_code error_code;
  const auto relative
      = std::filesystem::relative(source_path, repository_root, error_code);
  if (error_code) { return false; }
  return managed_paths.contains(relative.generic_string());
}

void AnnotateInspectionOwnership(json_t* inspection,
                                 const std::filesystem::path& config_root)
{
  const auto repository_root = RepositoryRootFromConfigPath(config_root);
  auto managed_paths = LoadManagedPathsForRepository(repository_root);
  if (!managed_paths) {
    json_object_set_new(inspection, "ownership_error",
                        json_string(managed_paths.error.c_str()));
    return;
  }

  auto* resources = json_object_get(inspection, "resources");
  if (!json_is_array(resources)) {
    json_object_set_new(inspection, "managed_resource_count", json_integer(0));
    json_object_set_new(inspection, "has_managed_resources", json_false());
    return;
  }

  size_t managed_resource_count = 0;
  size_t index = 0;
  json_t* resource = nullptr;
  json_array_foreach(resources, index, resource)
  {
    bool managed = false;
    auto* source = json_object_get(resource, "source");
    if (json_is_object(source)) {
      auto* file = json_object_get(source, "file");
      if (json_is_string(file)) {
        managed = IsManagedSourcePath(*managed_paths.value, repository_root,
                                      json_string_value(file));
      }
    }
    if (managed) { ++managed_resource_count; }
    json_object_set_new(resource, "managed", json_boolean(managed));
  }

  json_object_set_new(inspection, "managed_resource_count",
                      json_integer(managed_resource_count));
  json_object_set_new(inspection, "has_managed_resources",
                      json_boolean(managed_resource_count > 0));
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

    spec.peers.emplace_back(PeerLoadSpec{.component = *parsed_peer_component,
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
    peer_configs.emplace_back(
        bconfig::LoadConfig(peer.component, peer.path, true));
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
    json_object_set_new(
        peer_json.get(), "path",
        json_string(peer.parser ? peer.parser->get_base_config_path().c_str()
                                : ""));
    json_object_set_new(peer_json.get(), "parse_ok",
                        json_boolean(peer.parse_ok));
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
    json_object_set_new(resource_json.get(), "directives",
                        directives.release());
    json_array_append_new(resources.get(), resource_json.release());
  }

  json_object_set_new(root.get(), "resources", resources.release());
  return JsonResponse(http::status::ok, DumpJson(root.get()));
}

void AppendDeploymentConfig(json_t* array, const DeploymentConfigRecord& record)
{
  auto object = MakeJson(json_object());
  json_object_set_new(
      object.get(), "component",
      json_string(bconfig::ComponentToString(record.component)));
  json_object_set_new(object.get(), "name", json_string(record.name.c_str()));
  json_object_set_new(object.get(), "path",
                      json_string(record.path.string().c_str()));
  json_array_append_new(array, object.release());
}

JsonPtr BuildDeploymentConfigDocument(const DeploymentConfigRecord& config,
                                      bool& parser_initialized)
{
  InspectRequestSpec spec{
      .component = config.component, .path = config.path.string(), .peers = {}};
  auto inspection = BuildInspectionDocument(spec, parser_initialized);
  json_object_set_new(inspection.get(), "name",
                      json_string(config.name.c_str()));
  AnnotateInspectionOwnership(inspection.get(), config.path);
  return inspection;
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
    ServiceState& state,
    std::string_view deployment_id)
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

  for (const auto component : SupportedDeploymentInspectComponents()) {
    auto configs = state.ListDeploymentConfigs(deployment_id, component);
    if (!configs) {
      return ErrorResponse(http::status::bad_request, configs.error);
    }

    for (const auto& config : *configs.value) {
      bool parser_initialized = false;
      auto inspection
          = BuildDeploymentConfigDocument(config, parser_initialized);
      json_array_append_new(configs_json.get(), inspection.release());
    }
  }

  json_object_set_new(root.get(), "configs", configs_json.release());
  return JsonResponse(http::status::ok, DumpJson(root.get()));
}

http::response<http::string_body> HandleDeploymentImportsRequest(
    ServiceState& state,
    std::string_view deployment_id)
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

http::response<http::string_body> HandleDeploymentClientsRequest(
    ServiceState& state,
    std::string_view deployment_id)
{
  auto deployment = state.GetDeployment(deployment_id);
  if (!deployment) {
    return ErrorResponse(http::status::not_found, "deployment not found.");
  }

  auto clients
      = state.ListDeploymentConfigs(deployment_id, bconfig::Component::kClient);
  if (!clients) {
    return ErrorResponse(http::status::bad_request, clients.error);
  }

  auto root = MakeJson(json_object());
  auto deployment_json = MakeJson(json_array());
  auto clients_json = MakeJson(json_array());
  AppendDeployment(deployment_json.get(), *deployment);
  json_object_set(root.get(), "deployment",
                  json_array_get(deployment_json.get(), 0));
  for (const auto& client : *clients.value) {
    AppendDeploymentConfig(clients_json.get(), client);
  }
  json_object_set_new(root.get(), "clients", clients_json.release());
  return JsonResponse(http::status::ok, DumpJson(root.get()));
}

http::response<http::string_body> HandleDeploymentClientRequest(
    ServiceState& state,
    std::string_view deployment_id,
    std::string_view client_name)
{
  auto deployment = state.GetDeployment(deployment_id);
  if (!deployment) {
    return ErrorResponse(http::status::not_found, "deployment not found.");
  }

  auto client = state.GetDeploymentConfig(
      deployment_id, bconfig::Component::kClient, client_name);
  if (!client) { return ErrorResponse(http::status::not_found, client.error); }

  bool parser_initialized = false;
  auto client_json
      = BuildDeploymentConfigDocument(*client.value, parser_initialized);
  if (!parser_initialized) {
    return JsonResponse(http::status::bad_request, DumpJson(client_json.get()));
  }

  auto root = MakeJson(json_object());
  auto deployment_json = MakeJson(json_array());
  AppendDeployment(deployment_json.get(), *deployment);
  json_object_set(root.get(), "deployment",
                  json_array_get(deployment_json.get(), 0));
  json_object_set(root.get(), "client", client_json.get());
  return JsonResponse(http::status::ok, DumpJson(root.get()));
}

http::response<http::string_body> HandleDeploymentClientDirectorStubPutRequest(
    ServiceState& state,
    const http::request<http::string_body>& request,
    std::string_view deployment_id,
    std::string_view client_name,
    std::string_view director_name)
{
  auto deployment = state.GetDeployment(deployment_id);
  if (!deployment) {
    return ErrorResponse(http::status::not_found, "deployment not found.");
  }

  std::string error;
  auto spec = ParseClientDirectorStubRequest(request.body(), error);
  if (!spec) { return ErrorResponse(http::status::bad_request, error); }

  ClientDirectorStubSpec stub_spec{
      .description = spec->description,
  };
  auto result = state.UpsertClientDirectorStub(deployment_id, client_name,
                                               director_name, stub_spec);
  if (!result) {
    return ErrorResponse(http::status::bad_request, result.error);
  }

  bool parser_initialized = false;
  auto client_json
      = BuildDeploymentConfigDocument(*result.value, parser_initialized);
  if (!parser_initialized) {
    return JsonResponse(http::status::bad_request, DumpJson(client_json.get()));
  }

  auto root = MakeJson(json_object());
  auto deployment_json = MakeJson(json_array());
  AppendDeployment(deployment_json.get(), *deployment);
  json_object_set(root.get(), "deployment",
                  json_array_get(deployment_json.get(), 0));
  json_object_set_new(root.get(), "director_name",
                      json_string(std::string{director_name}.c_str()));
  json_object_set(root.get(), "client", client_json.get());
  return JsonResponse(http::status::ok, DumpJson(root.get()));
}

http::response<http::string_body> HandleDeploymentDirectorClientPutRequest(
    ServiceState& state,
    const http::request<http::string_body>& request,
    std::string_view deployment_id,
    std::string_view director_name,
    std::string_view client_name)
{
  auto deployment = state.GetDeployment(deployment_id);
  if (!deployment) {
    return ErrorResponse(http::status::not_found, "deployment not found.");
  }

  std::string error;
  auto spec = ParseDirectorClientRequest(request.body(), error);
  if (!spec) { return ErrorResponse(http::status::bad_request, error); }

  DirectorClientResourceSpec resource_spec{
      .address = spec->address,
      .port = spec->port,
      .password = spec->password,
      .description = spec->description,
  };
  auto result = state.UpsertDirectorClientResource(deployment_id, director_name,
                                                   client_name, resource_spec);
  if (!result) {
    return ErrorResponse(http::status::bad_request, result.error);
  }

  bool parser_initialized = false;
  auto director_json
      = BuildDeploymentConfigDocument(*result.value, parser_initialized);
  if (!parser_initialized) {
    return JsonResponse(http::status::bad_request,
                        DumpJson(director_json.get()));
  }

  auto client = state.GetDeploymentConfig(
      deployment_id, bconfig::Component::kClient, client_name);

  auto root = MakeJson(json_object());
  auto deployment_json = MakeJson(json_array());
  AppendDeployment(deployment_json.get(), *deployment);
  json_object_set(root.get(), "deployment",
                  json_array_get(deployment_json.get(), 0));
  json_object_set_new(root.get(), "director_name",
                      json_string(std::string{director_name}.c_str()));
  json_object_set_new(root.get(), "client_name",
                      json_string(std::string{client_name}.c_str()));
  json_object_set(root.get(), "director", director_json.get());
  if (client) {
    bool client_parser_initialized = false;
    auto client_json = BuildDeploymentConfigDocument(*client.value,
                                                     client_parser_initialized);
    if (!client_parser_initialized) {
      return JsonResponse(http::status::bad_request,
                          DumpJson(client_json.get()));
    }
    json_object_set(root.get(), "client", client_json.get());
  } else {
    json_object_set_new(root.get(), "client", json_null());
  }
  return JsonResponse(http::status::ok, DumpJson(root.get()));
}

http::response<http::string_body> HandleDeploymentDirectorClientDeleteRequest(
    ServiceState& state,
    std::string_view deployment_id,
    std::string_view director_name,
    std::string_view client_name)
{
  auto deployment = state.GetDeployment(deployment_id);
  if (!deployment) {
    return ErrorResponse(http::status::not_found, "deployment not found.");
  }

  auto result = state.DeleteDirectorClientResource(deployment_id, director_name,
                                                   client_name);
  if (!result) {
    return ErrorResponse(http::status::bad_request, result.error);
  }

  bool parser_initialized = false;
  auto director_json
      = BuildDeploymentConfigDocument(*result.value, parser_initialized);
  if (!parser_initialized) {
    return JsonResponse(http::status::bad_request,
                        DumpJson(director_json.get()));
  }

  auto client = state.GetDeploymentConfig(
      deployment_id, bconfig::Component::kClient, client_name);

  auto root = MakeJson(json_object());
  auto deployment_json = MakeJson(json_array());
  AppendDeployment(deployment_json.get(), *deployment);
  json_object_set(root.get(), "deployment",
                  json_array_get(deployment_json.get(), 0));
  json_object_set_new(root.get(), "director_name",
                      json_string(std::string{director_name}.c_str()));
  json_object_set_new(root.get(), "client_name",
                      json_string(std::string{client_name}.c_str()));
  json_object_set(root.get(), "director", director_json.get());
  if (client) {
    bool client_parser_initialized = false;
    auto client_json = BuildDeploymentConfigDocument(*client.value,
                                                     client_parser_initialized);
    if (!client_parser_initialized) {
      return JsonResponse(http::status::bad_request,
                          DumpJson(client_json.get()));
    }
    json_object_set(root.get(), "client", client_json.get());
  } else {
    json_object_set_new(root.get(), "client", json_null());
  }
  return JsonResponse(http::status::ok, DumpJson(root.get()));
}

http::response<http::string_body> HandleDeploymentDirectorStoragePutRequest(
    ServiceState& state,
    const http::request<http::string_body>& request,
    std::string_view deployment_id,
    std::string_view director_name,
    std::string_view storage_name)
{
  auto deployment = state.GetDeployment(deployment_id);
  if (!deployment) {
    return ErrorResponse(http::status::not_found, "deployment not found.");
  }

  std::string error;
  auto spec = ParseDirectorStorageRequest(request.body(), error);
  if (!spec) { return ErrorResponse(http::status::bad_request, error); }

  DirectorStorageResourceSpec resource_spec{
      .address = spec->address,
      .port = spec->port,
      .password = spec->password,
      .device = spec->device,
      .media_type = spec->media_type,
      .archive_device = spec->archive_device,
      .device_type = spec->device_type,
      .description = spec->description,
  };
  auto result = state.UpsertDirectorStorageResource(
      deployment_id, director_name, storage_name, resource_spec);
  if (!result) {
    return ErrorResponse(http::status::bad_request, result.error);
  }

  bool parser_initialized = false;
  auto director_json
      = BuildDeploymentConfigDocument(*result.value, parser_initialized);
  if (!parser_initialized) {
    return JsonResponse(http::status::bad_request,
                        DumpJson(director_json.get()));
  }

  auto root = MakeJson(json_object());
  auto deployment_json = MakeJson(json_array());
  AppendDeployment(deployment_json.get(), *deployment);
  json_object_set(root.get(), "deployment",
                  json_array_get(deployment_json.get(), 0));
  json_object_set_new(root.get(), "director_name",
                      json_string(std::string{director_name}.c_str()));
  json_object_set_new(root.get(), "storage_name",
                      json_string(std::string{storage_name}.c_str()));
  json_object_set(root.get(), "director", director_json.get());
  return JsonResponse(http::status::ok, DumpJson(root.get()));
}

http::response<http::string_body> HandleDeploymentDirectorStorageDeleteRequest(
    ServiceState& state,
    std::string_view deployment_id,
    std::string_view director_name,
    std::string_view storage_name)
{
  auto deployment = state.GetDeployment(deployment_id);
  if (!deployment) {
    return ErrorResponse(http::status::not_found, "deployment not found.");
  }

  auto result = state.DeleteDirectorStorageResource(
      deployment_id, director_name, storage_name);
  if (!result) {
    return ErrorResponse(http::status::bad_request, result.error);
  }

  bool parser_initialized = false;
  auto director_json
      = BuildDeploymentConfigDocument(*result.value, parser_initialized);
  if (!parser_initialized) {
    return JsonResponse(http::status::bad_request,
                        DumpJson(director_json.get()));
  }

  auto root = MakeJson(json_object());
  auto deployment_json = MakeJson(json_array());
  AppendDeployment(deployment_json.get(), *deployment);
  json_object_set(root.get(), "deployment",
                  json_array_get(deployment_json.get(), 0));
  json_object_set_new(root.get(), "director_name",
                      json_string(std::string{director_name}.c_str()));
  json_object_set_new(root.get(), "storage_name",
                      json_string(std::string{storage_name}.c_str()));
  json_object_set(root.get(), "director", director_json.get());
  return JsonResponse(http::status::ok, DumpJson(root.get()));
}

http::response<http::string_body> HandleDeploymentDirectorConsolePutRequest(
    ServiceState& state,
    const http::request<http::string_body>& request,
    std::string_view deployment_id,
    std::string_view director_name,
    std::string_view console_name)
{
  auto deployment = state.GetDeployment(deployment_id);
  if (!deployment) {
    return ErrorResponse(http::status::not_found, "deployment not found.");
  }

  std::string error;
  auto spec = ParseDirectorConsoleRequest(request.body(), error);
  if (!spec) { return ErrorResponse(http::status::bad_request, error); }

  DirectorConsoleResourceSpec resource_spec{
      .password = spec->password,
      .description = spec->description,
      .use_pam_authentication = spec->use_pam_authentication,
  };
  auto result = state.UpsertDirectorConsoleResource(
      deployment_id, director_name, console_name, resource_spec);
  if (!result) {
    return ErrorResponse(http::status::bad_request, result.error);
  }

  bool parser_initialized = false;
  auto director_json
      = BuildDeploymentConfigDocument(*result.value, parser_initialized);
  if (!parser_initialized) {
    return JsonResponse(http::status::bad_request,
                        DumpJson(director_json.get()));
  }

  auto root = MakeJson(json_object());
  auto deployment_json = MakeJson(json_array());
  AppendDeployment(deployment_json.get(), *deployment);
  json_object_set(root.get(), "deployment",
                  json_array_get(deployment_json.get(), 0));
  json_object_set_new(root.get(), "director_name",
                      json_string(std::string{director_name}.c_str()));
  json_object_set_new(root.get(), "console_name",
                      json_string(std::string{console_name}.c_str()));
  json_object_set(root.get(), "director", director_json.get());
  return JsonResponse(http::status::ok, DumpJson(root.get()));
}

http::response<http::string_body> HandleDeploymentDirectorConsoleDeleteRequest(
    ServiceState& state,
    std::string_view deployment_id,
    std::string_view director_name,
    std::string_view console_name)
{
  auto deployment = state.GetDeployment(deployment_id);
  if (!deployment) {
    return ErrorResponse(http::status::not_found, "deployment not found.");
  }

  auto result = state.DeleteDirectorConsoleResource(
      deployment_id, director_name, console_name);
  if (!result) {
    return ErrorResponse(http::status::bad_request, result.error);
  }

  bool parser_initialized = false;
  auto director_json
      = BuildDeploymentConfigDocument(*result.value, parser_initialized);
  if (!parser_initialized) {
    return JsonResponse(http::status::bad_request,
                        DumpJson(director_json.get()));
  }

  auto root = MakeJson(json_object());
  auto deployment_json = MakeJson(json_array());
  AppendDeployment(deployment_json.get(), *deployment);
  json_object_set(root.get(), "deployment",
                  json_array_get(deployment_json.get(), 0));
  json_object_set_new(root.get(), "director_name",
                      json_string(std::string{director_name}.c_str()));
  json_object_set_new(root.get(), "console_name",
                      json_string(std::string{console_name}.c_str()));
  json_object_set(root.get(), "director", director_json.get());
  return JsonResponse(http::status::ok, DumpJson(root.get()));
}

http::response<http::string_body> HandleDeploymentDirectorUserPutRequest(
    ServiceState& state,
    const http::request<http::string_body>& request,
    std::string_view deployment_id,
    std::string_view director_name,
    std::string_view user_name)
{
  auto deployment = state.GetDeployment(deployment_id);
  if (!deployment) {
    return ErrorResponse(http::status::not_found, "deployment not found.");
  }

  std::string error;
  auto spec = ParseDirectorUserRequest(request.body(), error);
  if (!spec) { return ErrorResponse(http::status::bad_request, error); }

  DirectorUserResourceSpec resource_spec{
      .description = spec->description,
  };
  auto result = state.UpsertDirectorUserResource(deployment_id, director_name,
                                                 user_name, resource_spec);
  if (!result) {
    return ErrorResponse(http::status::bad_request, result.error);
  }

  bool parser_initialized = false;
  auto director_json
      = BuildDeploymentConfigDocument(*result.value, parser_initialized);
  if (!parser_initialized) {
    return JsonResponse(http::status::bad_request,
                        DumpJson(director_json.get()));
  }

  auto root = MakeJson(json_object());
  auto deployment_json = MakeJson(json_array());
  AppendDeployment(deployment_json.get(), *deployment);
  json_object_set(root.get(), "deployment",
                  json_array_get(deployment_json.get(), 0));
  json_object_set_new(root.get(), "director_name",
                      json_string(std::string{director_name}.c_str()));
  json_object_set_new(root.get(), "user_name",
                      json_string(std::string{user_name}.c_str()));
  json_object_set(root.get(), "director", director_json.get());
  return JsonResponse(http::status::ok, DumpJson(root.get()));
}

http::response<http::string_body> HandleDeploymentDirectorUserDeleteRequest(
    ServiceState& state,
    std::string_view deployment_id,
    std::string_view director_name,
    std::string_view user_name)
{
  auto deployment = state.GetDeployment(deployment_id);
  if (!deployment) {
    return ErrorResponse(http::status::not_found, "deployment not found.");
  }

  auto result = state.DeleteDirectorUserResource(deployment_id, director_name,
                                                 user_name);
  if (!result) {
    return ErrorResponse(http::status::bad_request, result.error);
  }

  bool parser_initialized = false;
  auto director_json
      = BuildDeploymentConfigDocument(*result.value, parser_initialized);
  if (!parser_initialized) {
    return JsonResponse(http::status::bad_request,
                        DumpJson(director_json.get()));
  }

  auto root = MakeJson(json_object());
  auto deployment_json = MakeJson(json_array());
  AppendDeployment(deployment_json.get(), *deployment);
  json_object_set(root.get(), "deployment",
                  json_array_get(deployment_json.get(), 0));
  json_object_set_new(root.get(), "director_name",
                      json_string(std::string{director_name}.c_str()));
  json_object_set_new(root.get(), "user_name",
                      json_string(std::string{user_name}.c_str()));
  json_object_set(root.get(), "director", director_json.get());
  return JsonResponse(http::status::ok, DumpJson(root.get()));
}

http::response<http::string_body> HandleDeploymentDirectorProfilePutRequest(
    ServiceState& state,
    const http::request<http::string_body>& request,
    std::string_view deployment_id,
    std::string_view director_name,
    std::string_view profile_name)
{
  auto deployment = state.GetDeployment(deployment_id);
  if (!deployment) {
    return ErrorResponse(http::status::not_found, "deployment not found.");
  }

  std::string error;
  auto spec = ParseDirectorProfileRequest(request.body(), error);
  if (!spec) { return ErrorResponse(http::status::bad_request, error); }

  DirectorProfileResourceSpec resource_spec{
      .description = spec->description,
  };
  auto result = state.UpsertDirectorProfileResource(
      deployment_id, director_name, profile_name, resource_spec);
  if (!result) {
    return ErrorResponse(http::status::bad_request, result.error);
  }

  bool parser_initialized = false;
  auto director_json
      = BuildDeploymentConfigDocument(*result.value, parser_initialized);
  if (!parser_initialized) {
    return JsonResponse(http::status::bad_request,
                        DumpJson(director_json.get()));
  }

  auto root = MakeJson(json_object());
  auto deployment_json = MakeJson(json_array());
  AppendDeployment(deployment_json.get(), *deployment);
  json_object_set(root.get(), "deployment",
                  json_array_get(deployment_json.get(), 0));
  json_object_set_new(root.get(), "director_name",
                      json_string(std::string{director_name}.c_str()));
  json_object_set_new(root.get(), "profile_name",
                      json_string(std::string{profile_name}.c_str()));
  json_object_set(root.get(), "director", director_json.get());
  return JsonResponse(http::status::ok, DumpJson(root.get()));
}

http::response<http::string_body> HandleDeploymentDirectorProfileDeleteRequest(
    ServiceState& state,
    std::string_view deployment_id,
    std::string_view director_name,
    std::string_view profile_name)
{
  auto deployment = state.GetDeployment(deployment_id);
  if (!deployment) {
    return ErrorResponse(http::status::not_found, "deployment not found.");
  }

  auto result = state.DeleteDirectorProfileResource(
      deployment_id, director_name, profile_name);
  if (!result) {
    return ErrorResponse(http::status::bad_request, result.error);
  }

  bool parser_initialized = false;
  auto director_json
      = BuildDeploymentConfigDocument(*result.value, parser_initialized);
  if (!parser_initialized) {
    return JsonResponse(http::status::bad_request,
                        DumpJson(director_json.get()));
  }

  auto root = MakeJson(json_object());
  auto deployment_json = MakeJson(json_array());
  AppendDeployment(deployment_json.get(), *deployment);
  json_object_set(root.get(), "deployment",
                  json_array_get(deployment_json.get(), 0));
  json_object_set_new(root.get(), "director_name",
                      json_string(std::string{director_name}.c_str()));
  json_object_set_new(root.get(), "profile_name",
                      json_string(std::string{profile_name}.c_str()));
  json_object_set(root.get(), "director", director_json.get());
  return JsonResponse(http::status::ok, DumpJson(root.get()));
}

http::response<http::string_body> HandleDeploymentDirectorPoolPutRequest(
    ServiceState& state,
    const http::request<http::string_body>& request,
    std::string_view deployment_id,
    std::string_view director_name,
    std::string_view pool_name)
{
  auto deployment = state.GetDeployment(deployment_id);
  if (!deployment) {
    return ErrorResponse(http::status::not_found, "deployment not found.");
  }

  std::string error;
  auto spec = ParseDirectorPoolRequest(request.body(), error);
  if (!spec) { return ErrorResponse(http::status::bad_request, error); }

  DirectorPoolResourceSpec resource_spec{
      .pool_type = spec->pool_type,
      .label_format = spec->label_format,
      .maximum_volumes = spec->maximum_volumes,
      .maximum_volume_bytes = spec->maximum_volume_bytes,
      .volume_retention = spec->volume_retention,
      .auto_prune = spec->auto_prune,
      .recycle = spec->recycle,
      .description = spec->description,
  };
  auto result = state.UpsertDirectorPoolResource(deployment_id, director_name,
                                                 pool_name, resource_spec);
  if (!result) {
    return ErrorResponse(http::status::bad_request, result.error);
  }

  bool parser_initialized = false;
  auto director_json
      = BuildDeploymentConfigDocument(*result.value, parser_initialized);
  if (!parser_initialized) {
    return JsonResponse(http::status::bad_request,
                        DumpJson(director_json.get()));
  }

  auto root = MakeJson(json_object());
  auto deployment_json = MakeJson(json_array());
  AppendDeployment(deployment_json.get(), *deployment);
  json_object_set(root.get(), "deployment",
                  json_array_get(deployment_json.get(), 0));
  json_object_set_new(root.get(), "director_name",
                      json_string(std::string{director_name}.c_str()));
  json_object_set_new(root.get(), "pool_name",
                      json_string(std::string{pool_name}.c_str()));
  json_object_set(root.get(), "director", director_json.get());
  return JsonResponse(http::status::ok, DumpJson(root.get()));
}

http::response<http::string_body> HandleDeploymentDirectorPoolDeleteRequest(
    ServiceState& state,
    std::string_view deployment_id,
    std::string_view director_name,
    std::string_view pool_name)
{
  auto deployment = state.GetDeployment(deployment_id);
  if (!deployment) {
    return ErrorResponse(http::status::not_found, "deployment not found.");
  }

  auto result = state.DeleteDirectorPoolResource(deployment_id, director_name,
                                                 pool_name);
  if (!result) {
    return ErrorResponse(http::status::bad_request, result.error);
  }

  bool parser_initialized = false;
  auto director_json
      = BuildDeploymentConfigDocument(*result.value, parser_initialized);
  if (!parser_initialized) {
    return JsonResponse(http::status::bad_request,
                        DumpJson(director_json.get()));
  }

  auto root = MakeJson(json_object());
  auto deployment_json = MakeJson(json_array());
  AppendDeployment(deployment_json.get(), *deployment);
  json_object_set(root.get(), "deployment",
                  json_array_get(deployment_json.get(), 0));
  json_object_set_new(root.get(), "director_name",
                      json_string(std::string{director_name}.c_str()));
  json_object_set_new(root.get(), "pool_name",
                      json_string(std::string{pool_name}.c_str()));
  json_object_set(root.get(), "director", director_json.get());
  return JsonResponse(http::status::ok, DumpJson(root.get()));
}

http::response<http::string_body> HandleDeploymentDirectorCatalogPutRequest(
    ServiceState& state,
    const http::request<http::string_body>& request,
    std::string_view deployment_id,
    std::string_view director_name,
    std::string_view catalog_name)
{
  auto deployment = state.GetDeployment(deployment_id);
  if (!deployment) {
    return ErrorResponse(http::status::not_found, "deployment not found.");
  }

  std::string error;
  auto spec = ParseDirectorCatalogRequest(request.body(), error);
  if (!spec) { return ErrorResponse(http::status::bad_request, error); }

  DirectorCatalogResourceSpec resource_spec{
      .db_address = spec->db_address,
      .db_port = spec->db_port,
      .db_socket = spec->db_socket,
      .db_password = spec->db_password,
      .db_user = spec->db_user,
      .db_name = spec->db_name,
      .reconnect = spec->reconnect,
      .exit_on_fatal = spec->exit_on_fatal,
      .min_connections = spec->min_connections,
      .max_connections = spec->max_connections,
      .inc_connections = spec->inc_connections,
      .idle_timeout = spec->idle_timeout,
      .validate_timeout = spec->validate_timeout,
      .description = spec->description,
  };
  auto result = state.UpsertDirectorCatalogResource(
      deployment_id, director_name, catalog_name, resource_spec);
  if (!result) {
    return ErrorResponse(http::status::bad_request, result.error);
  }

  bool parser_initialized = false;
  auto director_json
      = BuildDeploymentConfigDocument(*result.value, parser_initialized);
  if (!parser_initialized) {
    return JsonResponse(http::status::bad_request,
                        DumpJson(director_json.get()));
  }

  auto root = MakeJson(json_object());
  auto deployment_json = MakeJson(json_array());
  AppendDeployment(deployment_json.get(), *deployment);
  json_object_set(root.get(), "deployment",
                  json_array_get(deployment_json.get(), 0));
  json_object_set_new(root.get(), "director_name",
                      json_string(std::string{director_name}.c_str()));
  json_object_set_new(root.get(), "catalog_name",
                      json_string(std::string{catalog_name}.c_str()));
  json_object_set(root.get(), "director", director_json.get());
  return JsonResponse(http::status::ok, DumpJson(root.get()));
}

http::response<http::string_body> HandleDeploymentDirectorCatalogDeleteRequest(
    ServiceState& state,
    std::string_view deployment_id,
    std::string_view director_name,
    std::string_view catalog_name)
{
  auto deployment = state.GetDeployment(deployment_id);
  if (!deployment) {
    return ErrorResponse(http::status::not_found, "deployment not found.");
  }

  auto result = state.DeleteDirectorCatalogResource(
      deployment_id, director_name, catalog_name);
  if (!result) {
    return ErrorResponse(http::status::bad_request, result.error);
  }

  bool parser_initialized = false;
  auto director_json
      = BuildDeploymentConfigDocument(*result.value, parser_initialized);
  if (!parser_initialized) {
    return JsonResponse(http::status::bad_request,
                        DumpJson(director_json.get()));
  }

  auto root = MakeJson(json_object());
  auto deployment_json = MakeJson(json_array());
  AppendDeployment(deployment_json.get(), *deployment);
  json_object_set(root.get(), "deployment",
                  json_array_get(deployment_json.get(), 0));
  json_object_set_new(root.get(), "director_name",
                      json_string(std::string{director_name}.c_str()));
  json_object_set_new(root.get(), "catalog_name",
                      json_string(std::string{catalog_name}.c_str()));
  json_object_set(root.get(), "director", director_json.get());
  return JsonResponse(http::status::ok, DumpJson(root.get()));
}

http::response<http::string_body> HandleDeploymentDirectorSchedulePutRequest(
    ServiceState& state,
    const http::request<http::string_body>& request,
    std::string_view deployment_id,
    std::string_view director_name,
    std::string_view schedule_name)
{
  auto deployment = state.GetDeployment(deployment_id);
  if (!deployment) {
    return ErrorResponse(http::status::not_found, "deployment not found.");
  }

  std::string error;
  auto spec = ParseDirectorScheduleRequest(request.body(), error);
  if (!spec) { return ErrorResponse(http::status::bad_request, error); }

  DirectorScheduleResourceSpec resource_spec{
      .description = spec->description,
      .enabled = spec->enabled,
      .run_entries = spec->run_entries,
  };
  auto result = state.UpsertDirectorScheduleResource(
      deployment_id, director_name, schedule_name, resource_spec);
  if (!result) {
    return ErrorResponse(http::status::bad_request, result.error);
  }

  bool parser_initialized = false;
  auto director_json
      = BuildDeploymentConfigDocument(*result.value, parser_initialized);
  if (!parser_initialized) {
    return JsonResponse(http::status::bad_request,
                        DumpJson(director_json.get()));
  }

  auto root = MakeJson(json_object());
  auto deployment_json = MakeJson(json_array());
  AppendDeployment(deployment_json.get(), *deployment);
  json_object_set(root.get(), "deployment",
                  json_array_get(deployment_json.get(), 0));
  json_object_set_new(root.get(), "director_name",
                      json_string(std::string{director_name}.c_str()));
  json_object_set_new(root.get(), "schedule_name",
                      json_string(std::string{schedule_name}.c_str()));
  json_object_set(root.get(), "director", director_json.get());
  return JsonResponse(http::status::ok, DumpJson(root.get()));
}

http::response<http::string_body> HandleDeploymentDirectorScheduleDeleteRequest(
    ServiceState& state,
    std::string_view deployment_id,
    std::string_view director_name,
    std::string_view schedule_name)
{
  auto deployment = state.GetDeployment(deployment_id);
  if (!deployment) {
    return ErrorResponse(http::status::not_found, "deployment not found.");
  }

  auto result = state.DeleteDirectorScheduleResource(
      deployment_id, director_name, schedule_name);
  if (!result) {
    return ErrorResponse(http::status::bad_request, result.error);
  }

  bool parser_initialized = false;
  auto director_json
      = BuildDeploymentConfigDocument(*result.value, parser_initialized);
  if (!parser_initialized) {
    return JsonResponse(http::status::bad_request,
                        DumpJson(director_json.get()));
  }

  auto root = MakeJson(json_object());
  auto deployment_json = MakeJson(json_array());
  AppendDeployment(deployment_json.get(), *deployment);
  json_object_set(root.get(), "deployment",
                  json_array_get(deployment_json.get(), 0));
  json_object_set_new(root.get(), "director_name",
                      json_string(std::string{director_name}.c_str()));
  json_object_set_new(root.get(), "schedule_name",
                      json_string(std::string{schedule_name}.c_str()));
  json_object_set(root.get(), "director", director_json.get());
  return JsonResponse(http::status::ok, DumpJson(root.get()));
}

http::response<http::string_body> HandleDeploymentGitStatusRequest(
    ServiceState& state,
    std::string_view deployment_id)
{
  auto deployment = state.GetDeployment(deployment_id);
  if (!deployment) {
    return ErrorResponse(http::status::not_found, "deployment not found.");
  }

  auto git_status = state.GetDeploymentGitStatus(deployment_id);
  if (!git_status) {
    return ErrorResponse(http::status::bad_request, git_status.error);
  }

  auto root = MakeJson(json_object());
  auto deployment_json = MakeJson(json_array());
  AppendDeployment(deployment_json.get(), *deployment);
  json_object_set(root.get(), "deployment",
                  json_array_get(deployment_json.get(), 0));
  SetDeploymentGitStatus(root.get(), *git_status.value);
  return JsonResponse(http::status::ok, DumpJson(root.get()));
}

http::response<http::string_body> HandleDeploymentDiffPreviewRequest(
    ServiceState& state,
    std::string_view deployment_id)
{
  auto deployment = state.GetDeployment(deployment_id);
  if (!deployment) {
    return ErrorResponse(http::status::not_found, "deployment not found.");
  }

  auto diff_preview = state.GetDeploymentDiffPreview(deployment_id);
  if (!diff_preview) {
    return ErrorResponse(http::status::bad_request, diff_preview.error);
  }

  auto root = MakeJson(json_object());
  auto deployment_json = MakeJson(json_array());
  AppendDeployment(deployment_json.get(), *deployment);
  json_object_set(root.get(), "deployment",
                  json_array_get(deployment_json.get(), 0));
  SetDeploymentDiffPreview(root.get(), *diff_preview.value);
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
      error = "workflow_mode must be 'direct_commit' or 'review'.";
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
  auto* commit_message = json_object_get(root.get(), "commit_message");
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
  if (source_path && !json_is_null(source_path)
      && !json_is_string(source_path)) {
    error = "field 'source_path' must be a string when provided.";
    return std::nullopt;
  }
  if (commit_message && !json_is_null(commit_message)
      && !json_is_string(commit_message)) {
    error = "field 'commit_message' must be a string when provided.";
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
  if (commit_message && json_is_string(commit_message)) {
    spec.commit_message = std::string{json_string_value(commit_message)};
  }

  return spec;
}

std::optional<ClientDirectorStubRequestSpec> ParseClientDirectorStubRequest(
    std::string_view body,
    std::string& error)
{
  json_error_t json_error{};
  auto root = MakeJson(json_loadb(body.data(), body.size(), 0, &json_error));
  if (!root) {
    error = "invalid JSON body: " + std::string{json_error.text};
    return std::nullopt;
  }

  auto* description = json_object_get(root.get(), "description");
  if (description && !json_is_null(description)
      && !json_is_string(description)) {
    error = "field 'description' must be a string when provided.";
    return std::nullopt;
  }

  ClientDirectorStubRequestSpec spec{};
  if (description && json_is_string(description)) {
    spec.description = std::string{json_string_value(description)};
  }
  return spec;
}

std::optional<DirectorClientRequestSpec> ParseDirectorClientRequest(
    std::string_view body,
    std::string& error)
{
  json_error_t json_error{};
  auto root = MakeJson(json_loadb(body.data(), body.size(), 0, &json_error));
  if (!root) {
    error = "invalid JSON body: " + std::string{json_error.text};
    return std::nullopt;
  }

  auto* address = json_object_get(root.get(), "address");
  auto* port = json_object_get(root.get(), "port");
  auto* password = json_object_get(root.get(), "password");
  auto* description = json_object_get(root.get(), "description");

  if (address && !json_is_null(address) && !json_is_string(address)) {
    error = "field 'address' must be a string when provided.";
    return std::nullopt;
  }
  if (port && !json_is_null(port) && !json_is_integer(port)) {
    error = "field 'port' must be an integer when provided.";
    return std::nullopt;
  }
  if (password && !json_is_null(password) && !json_is_string(password)) {
    error = "field 'password' must be a string when provided.";
    return std::nullopt;
  }
  if (description && !json_is_null(description)
      && !json_is_string(description)) {
    error = "field 'description' must be a string when provided.";
    return std::nullopt;
  }

  DirectorClientRequestSpec spec{};
  if (address && json_is_string(address)) {
    spec.address = std::string{json_string_value(address)};
  }
  if (port && json_is_integer(port)) {
    const auto value = json_integer_value(port);
    if (value <= 0 || value > 65535) {
      error = "field 'port' must be between 1 and 65535.";
      return std::nullopt;
    }
    spec.port = static_cast<uint16_t>(value);
  }
  if (password && json_is_string(password)) {
    spec.password = std::string{json_string_value(password)};
  }
  if (description && json_is_string(description)) {
    spec.description = std::string{json_string_value(description)};
  }
  return spec;
}

std::optional<DirectorStorageRequestSpec> ParseDirectorStorageRequest(
    std::string_view body,
    std::string& error)
{
  json_error_t json_error{};
  auto root = MakeJson(json_loadb(body.data(), body.size(), 0, &json_error));
  if (!root) {
    error = "invalid JSON body: " + std::string{json_error.text};
    return std::nullopt;
  }

  auto* address = json_object_get(root.get(), "address");
  auto* port = json_object_get(root.get(), "port");
  auto* password = json_object_get(root.get(), "password");
  auto* device = json_object_get(root.get(), "device");
  auto* media_type = json_object_get(root.get(), "media_type");
  auto* archive_device = json_object_get(root.get(), "archive_device");
  auto* device_type = json_object_get(root.get(), "device_type");
  auto* description = json_object_get(root.get(), "description");

  if (address && !json_is_null(address) && !json_is_string(address)) {
    error = "field 'address' must be a string when provided.";
    return std::nullopt;
  }
  if (port && !json_is_null(port) && !json_is_integer(port)) {
    error = "field 'port' must be an integer when provided.";
    return std::nullopt;
  }
  if (password && !json_is_null(password) && !json_is_string(password)) {
    error = "field 'password' must be a string when provided.";
    return std::nullopt;
  }
  if (device && !json_is_null(device) && !json_is_string(device)) {
    error = "field 'device' must be a string when provided.";
    return std::nullopt;
  }
  if (media_type && !json_is_null(media_type) && !json_is_string(media_type)) {
    error = "field 'media_type' must be a string when provided.";
    return std::nullopt;
  }
  if (archive_device && !json_is_null(archive_device)
      && !json_is_string(archive_device)) {
    error = "field 'archive_device' must be a string when provided.";
    return std::nullopt;
  }
  if (device_type && !json_is_null(device_type)
      && !json_is_string(device_type)) {
    error = "field 'device_type' must be a string when provided.";
    return std::nullopt;
  }
  if (description && !json_is_null(description)
      && !json_is_string(description)) {
    error = "field 'description' must be a string when provided.";
    return std::nullopt;
  }

  DirectorStorageRequestSpec spec{};
  if (address && json_is_string(address)) {
    spec.address = std::string{json_string_value(address)};
  }
  if (port && json_is_integer(port)) {
    const auto value = json_integer_value(port);
    if (value <= 0 || value > 65535) {
      error = "field 'port' must be between 1 and 65535.";
      return std::nullopt;
    }
    spec.port = static_cast<uint16_t>(value);
  }
  if (password && json_is_string(password)) {
    spec.password = std::string{json_string_value(password)};
  }
  if (device && json_is_string(device)) {
    spec.device = std::string{json_string_value(device)};
  }
  if (media_type && json_is_string(media_type)) {
    spec.media_type = std::string{json_string_value(media_type)};
  }
  if (archive_device && json_is_string(archive_device)) {
    spec.archive_device = std::string{json_string_value(archive_device)};
  }
  if (device_type && json_is_string(device_type)) {
    spec.device_type = std::string{json_string_value(device_type)};
  }
  if (description && json_is_string(description)) {
    spec.description = std::string{json_string_value(description)};
  }
  return spec;
}

std::optional<DirectorConsoleRequestSpec> ParseDirectorConsoleRequest(
    std::string_view body,
    std::string& error)
{
  json_error_t json_error{};
  auto root = MakeJson(json_loadb(body.data(), body.size(), 0, &json_error));
  if (!root) {
    error = "invalid JSON body: " + std::string{json_error.text};
    return std::nullopt;
  }

  auto* password = json_object_get(root.get(), "password");
  auto* description = json_object_get(root.get(), "description");
  auto* use_pam_authentication
      = json_object_get(root.get(), "use_pam_authentication");

  if (password && !json_is_null(password) && !json_is_string(password)) {
    error = "field 'password' must be a string when provided.";
    return std::nullopt;
  }
  if (description && !json_is_null(description)
      && !json_is_string(description)) {
    error = "field 'description' must be a string when provided.";
    return std::nullopt;
  }
  if (use_pam_authentication && !json_is_null(use_pam_authentication)
      && !json_is_boolean(use_pam_authentication)) {
    error = "field 'use_pam_authentication' must be a boolean when provided.";
    return std::nullopt;
  }

  DirectorConsoleRequestSpec spec{};
  if (password && json_is_string(password)) {
    spec.password = std::string{json_string_value(password)};
  }
  if (description && json_is_string(description)) {
    spec.description = std::string{json_string_value(description)};
  }
  if (use_pam_authentication && json_is_boolean(use_pam_authentication)) {
    spec.use_pam_authentication = json_is_true(use_pam_authentication);
  }
  return spec;
}

std::optional<DirectorUserRequestSpec> ParseDirectorUserRequest(
    std::string_view body,
    std::string& error)
{
  json_error_t json_error{};
  auto root = MakeJson(json_loadb(body.data(), body.size(), 0, &json_error));
  if (!root) {
    error = "invalid JSON body: " + std::string{json_error.text};
    return std::nullopt;
  }

  auto* description = json_object_get(root.get(), "description");
  if (description && !json_is_null(description)
      && !json_is_string(description)) {
    error = "field 'description' must be a string when provided.";
    return std::nullopt;
  }

  DirectorUserRequestSpec spec{};
  if (description && json_is_string(description)) {
    spec.description = std::string{json_string_value(description)};
  }
  return spec;
}

std::optional<DirectorProfileRequestSpec> ParseDirectorProfileRequest(
    std::string_view body,
    std::string& error)
{
  json_error_t json_error{};
  auto root = MakeJson(json_loadb(body.data(), body.size(), 0, &json_error));
  if (!root) {
    error = "invalid JSON body: " + std::string{json_error.text};
    return std::nullopt;
  }

  auto* description = json_object_get(root.get(), "description");
  if (description && !json_is_null(description)
      && !json_is_string(description)) {
    error = "field 'description' must be a string when provided.";
    return std::nullopt;
  }

  DirectorProfileRequestSpec spec{};
  if (description && json_is_string(description)) {
    spec.description = std::string{json_string_value(description)};
  }
  return spec;
}

std::optional<DirectorPoolRequestSpec> ParseDirectorPoolRequest(
    std::string_view body,
    std::string& error)
{
  json_error_t json_error{};
  auto root = MakeJson(json_loadb(body.data(), body.size(), 0, &json_error));
  if (!root) {
    error = "invalid JSON body: " + std::string{json_error.text};
    return std::nullopt;
  }

  auto* pool_type = json_object_get(root.get(), "pool_type");
  auto* label_format = json_object_get(root.get(), "label_format");
  auto* maximum_volumes = json_object_get(root.get(), "maximum_volumes");
  auto* maximum_volume_bytes
      = json_object_get(root.get(), "maximum_volume_bytes");
  auto* volume_retention = json_object_get(root.get(), "volume_retention");
  auto* auto_prune = json_object_get(root.get(), "auto_prune");
  auto* recycle = json_object_get(root.get(), "recycle");
  auto* description = json_object_get(root.get(), "description");

  if (pool_type && !json_is_null(pool_type) && !json_is_string(pool_type)) {
    error = "field 'pool_type' must be a string when provided.";
    return std::nullopt;
  }
  if (label_format && !json_is_null(label_format)
      && !json_is_string(label_format)) {
    error = "field 'label_format' must be a string when provided.";
    return std::nullopt;
  }
  if (maximum_volumes && !json_is_null(maximum_volumes)
      && !json_is_integer(maximum_volumes)) {
    error = "field 'maximum_volumes' must be an integer when provided.";
    return std::nullopt;
  }
  if (maximum_volume_bytes && !json_is_null(maximum_volume_bytes)
      && !json_is_integer(maximum_volume_bytes)) {
    error = "field 'maximum_volume_bytes' must be an integer when provided.";
    return std::nullopt;
  }
  if (volume_retention && !json_is_null(volume_retention)
      && !json_is_integer(volume_retention)) {
    error = "field 'volume_retention' must be an integer when provided.";
    return std::nullopt;
  }
  if (auto_prune && !json_is_null(auto_prune) && !json_is_boolean(auto_prune)) {
    error = "field 'auto_prune' must be a boolean when provided.";
    return std::nullopt;
  }
  if (recycle && !json_is_null(recycle) && !json_is_boolean(recycle)) {
    error = "field 'recycle' must be a boolean when provided.";
    return std::nullopt;
  }
  if (description && !json_is_null(description)
      && !json_is_string(description)) {
    error = "field 'description' must be a string when provided.";
    return std::nullopt;
  }

  DirectorPoolRequestSpec spec{};
  if (pool_type && json_is_string(pool_type)) {
    spec.pool_type = std::string{json_string_value(pool_type)};
  }
  if (label_format && json_is_string(label_format)) {
    spec.label_format = std::string{json_string_value(label_format)};
  }
  if (maximum_volumes && json_is_integer(maximum_volumes)) {
    const auto value = json_integer_value(maximum_volumes);
    if (value < 0 || value > std::numeric_limits<uint32_t>::max()) {
      error = "field 'maximum_volumes' must be between 0 and 4294967295.";
      return std::nullopt;
    }
    spec.maximum_volumes = static_cast<uint32_t>(value);
  }
  if (maximum_volume_bytes && json_is_integer(maximum_volume_bytes)) {
    const auto value = json_integer_value(maximum_volume_bytes);
    if (value < 0) {
      error = "field 'maximum_volume_bytes' must be non-negative.";
      return std::nullopt;
    }
    spec.maximum_volume_bytes = static_cast<uint64_t>(value);
  }
  if (volume_retention && json_is_integer(volume_retention)) {
    const auto value = json_integer_value(volume_retention);
    if (value < 0) {
      error = "field 'volume_retention' must be non-negative.";
      return std::nullopt;
    }
    spec.volume_retention = static_cast<uint64_t>(value);
  }
  if (auto_prune && json_is_boolean(auto_prune)) {
    spec.auto_prune = json_is_true(auto_prune);
  }
  if (recycle && json_is_boolean(recycle)) {
    spec.recycle = json_is_true(recycle);
  }
  if (description && json_is_string(description)) {
    spec.description = std::string{json_string_value(description)};
  }
  return spec;
}

std::optional<DirectorCatalogRequestSpec> ParseDirectorCatalogRequest(
    std::string_view body,
    std::string& error)
{
  json_error_t json_error{};
  auto root = MakeJson(json_loadb(body.data(), body.size(), 0, &json_error));
  if (!root) {
    error = "invalid JSON body: " + std::string{json_error.text};
    return std::nullopt;
  }

  auto* db_address = json_object_get(root.get(), "db_address");
  auto* db_port = json_object_get(root.get(), "db_port");
  auto* db_socket = json_object_get(root.get(), "db_socket");
  auto* db_password = json_object_get(root.get(), "db_password");
  auto* db_user = json_object_get(root.get(), "db_user");
  auto* db_name = json_object_get(root.get(), "db_name");
  auto* reconnect = json_object_get(root.get(), "reconnect");
  auto* exit_on_fatal = json_object_get(root.get(), "exit_on_fatal");
  auto* min_connections = json_object_get(root.get(), "min_connections");
  auto* max_connections = json_object_get(root.get(), "max_connections");
  auto* inc_connections = json_object_get(root.get(), "inc_connections");
  auto* idle_timeout = json_object_get(root.get(), "idle_timeout");
  auto* validate_timeout = json_object_get(root.get(), "validate_timeout");
  auto* description = json_object_get(root.get(), "description");

  auto require_string = [&error](json_t* value, const char* field) {
    if (value && !json_is_null(value) && !json_is_string(value)) {
      error = std::string{"field '"} + field
              + "' must be a string when provided.";
      return false;
    }
    return true;
  };
  auto require_integer = [&error](json_t* value, const char* field) {
    if (value && !json_is_null(value) && !json_is_integer(value)) {
      error = std::string{"field '"} + field
              + "' must be an integer when provided.";
      return false;
    }
    return true;
  };
  auto require_boolean = [&error](json_t* value, const char* field) {
    if (value && !json_is_null(value) && !json_is_boolean(value)) {
      error = std::string{"field '"} + field
              + "' must be a boolean when provided.";
      return false;
    }
    return true;
  };

  if (!require_string(db_address, "db_address")
      || !require_integer(db_port, "db_port")
      || !require_string(db_socket, "db_socket")
      || !require_string(db_password, "db_password")
      || !require_string(db_user, "db_user")
      || !require_string(db_name, "db_name")
      || !require_boolean(reconnect, "reconnect")
      || !require_boolean(exit_on_fatal, "exit_on_fatal")
      || !require_integer(min_connections, "min_connections")
      || !require_integer(max_connections, "max_connections")
      || !require_integer(inc_connections, "inc_connections")
      || !require_integer(idle_timeout, "idle_timeout")
      || !require_integer(validate_timeout, "validate_timeout")
      || !require_string(description, "description")) {
    return std::nullopt;
  }

  auto parse_u32 = [&error](json_t* value, const char* field,
                            std::optional<uint32_t>& out) -> bool {
    if (!value || !json_is_integer(value)) { return true; }
    const auto raw = json_integer_value(value);
    if (raw < 0 || raw > std::numeric_limits<uint32_t>::max()) {
      error = std::string{"field '"} + field
              + "' must be between 0 and 4294967295.";
      return false;
    }
    out = static_cast<uint32_t>(raw);
    return true;
  };

  DirectorCatalogRequestSpec spec{};
  if (db_address && json_is_string(db_address)) {
    spec.db_address = std::string{json_string_value(db_address)};
  }
  if (!parse_u32(db_port, "db_port", spec.db_port)
      || !parse_u32(min_connections, "min_connections", spec.min_connections)
      || !parse_u32(max_connections, "max_connections", spec.max_connections)
      || !parse_u32(inc_connections, "inc_connections", spec.inc_connections)
      || !parse_u32(idle_timeout, "idle_timeout", spec.idle_timeout)
      || !parse_u32(validate_timeout, "validate_timeout",
                    spec.validate_timeout)) {
    return std::nullopt;
  }
  if (db_socket && json_is_string(db_socket)) {
    spec.db_socket = std::string{json_string_value(db_socket)};
  }
  if (db_password && json_is_string(db_password)) {
    spec.db_password = std::string{json_string_value(db_password)};
  }
  if (db_user && json_is_string(db_user)) {
    spec.db_user = std::string{json_string_value(db_user)};
  }
  if (db_name && json_is_string(db_name)) {
    spec.db_name = std::string{json_string_value(db_name)};
  }
  if (reconnect && json_is_boolean(reconnect)) {
    spec.reconnect = json_is_true(reconnect);
  }
  if (exit_on_fatal && json_is_boolean(exit_on_fatal)) {
    spec.exit_on_fatal = json_is_true(exit_on_fatal);
  }
  if (description && json_is_string(description)) {
    spec.description = std::string{json_string_value(description)};
  }
  return spec;
}

std::optional<DirectorScheduleRequestSpec> ParseDirectorScheduleRequest(
    std::string_view body,
    std::string& error)
{
  json_error_t json_error{};
  auto root = MakeJson(json_loadb(body.data(), body.size(), 0, &json_error));
  if (!root) {
    error = "invalid JSON body: " + std::string{json_error.text};
    return std::nullopt;
  }

  auto* description = json_object_get(root.get(), "description");
  auto* enabled = json_object_get(root.get(), "enabled");
  auto* run_entries = json_object_get(root.get(), "run_entries");
  if (description && !json_is_null(description)
      && !json_is_string(description)) {
    error = "field 'description' must be a string when provided.";
    return std::nullopt;
  }
  if (enabled && !json_is_null(enabled) && !json_is_boolean(enabled)) {
    error = "field 'enabled' must be a boolean when provided.";
    return std::nullopt;
  }
  if (run_entries && !json_is_null(run_entries)
      && !json_is_array(run_entries)) {
    error = "field 'run_entries' must be an array of strings when provided.";
    return std::nullopt;
  }

  DirectorScheduleRequestSpec spec{};
  if (description && json_is_string(description)) {
    spec.description = std::string{json_string_value(description)};
  }
  if (enabled && json_is_boolean(enabled)) {
    spec.enabled = json_is_true(enabled);
  }
  if (run_entries && json_is_array(run_entries)) {
    std::vector<std::string> entries;
    entries.reserve(json_array_size(run_entries));
    for (size_t index = 0; index < json_array_size(run_entries); ++index) {
      auto* entry = json_array_get(run_entries, index);
      if (!json_is_string(entry)) {
        error = "field 'run_entries' must contain only strings.";
        return std::nullopt;
      }
      entries.emplace_back(json_string_value(entry));
    }
    spec.run_entries = std::move(entries);
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

  if (path_parts.size() == 4 && path_parts[3] == "clients"
      && request.method() == http::verb::get) {
    return HandleDeploymentClientsRequest(state, path_parts[2]);
  }

  if (path_parts.size() == 5 && path_parts[3] == "clients"
      && request.method() == http::verb::get) {
    return HandleDeploymentClientRequest(state, path_parts[2], path_parts[4]);
  }

  if (path_parts.size() == 7 && path_parts[3] == "clients"
      && path_parts[5] == "directors" && request.method() == http::verb::put) {
    return HandleDeploymentClientDirectorStubPutRequest(
        state, request, path_parts[2], path_parts[4], path_parts[6]);
  }

  if (path_parts.size() == 7 && path_parts[3] == "directors"
      && path_parts[5] == "clients" && request.method() == http::verb::put) {
    return HandleDeploymentDirectorClientPutRequest(
        state, request, path_parts[2], path_parts[4], path_parts[6]);
  }
  if (path_parts.size() == 7 && path_parts[3] == "directors"
      && path_parts[5] == "clients"
      && request.method() == http::verb::delete_) {
    return HandleDeploymentDirectorClientDeleteRequest(
        state, path_parts[2], path_parts[4], path_parts[6]);
  }

  if (path_parts.size() == 7 && path_parts[3] == "directors"
      && path_parts[5] == "storages" && request.method() == http::verb::put) {
    return HandleDeploymentDirectorStoragePutRequest(
        state, request, path_parts[2], path_parts[4], path_parts[6]);
  }
  if (path_parts.size() == 7 && path_parts[3] == "directors"
      && path_parts[5] == "storages"
      && request.method() == http::verb::delete_) {
    return HandleDeploymentDirectorStorageDeleteRequest(
        state, path_parts[2], path_parts[4], path_parts[6]);
  }

  if (path_parts.size() == 7 && path_parts[3] == "directors"
      && path_parts[5] == "consoles" && request.method() == http::verb::put) {
    return HandleDeploymentDirectorConsolePutRequest(
        state, request, path_parts[2], path_parts[4], path_parts[6]);
  }
  if (path_parts.size() == 7 && path_parts[3] == "directors"
      && path_parts[5] == "consoles"
      && request.method() == http::verb::delete_) {
    return HandleDeploymentDirectorConsoleDeleteRequest(
        state, path_parts[2], path_parts[4], path_parts[6]);
  }

  if (path_parts.size() == 7 && path_parts[3] == "directors"
      && path_parts[5] == "users" && request.method() == http::verb::put) {
    return HandleDeploymentDirectorUserPutRequest(state, request, path_parts[2],
                                                  path_parts[4], path_parts[6]);
  }
  if (path_parts.size() == 7 && path_parts[3] == "directors"
      && path_parts[5] == "users" && request.method() == http::verb::delete_) {
    return HandleDeploymentDirectorUserDeleteRequest(
        state, path_parts[2], path_parts[4], path_parts[6]);
  }

  if (path_parts.size() == 7 && path_parts[3] == "directors"
      && path_parts[5] == "profiles" && request.method() == http::verb::put) {
    return HandleDeploymentDirectorProfilePutRequest(
        state, request, path_parts[2], path_parts[4], path_parts[6]);
  }
  if (path_parts.size() == 7 && path_parts[3] == "directors"
      && path_parts[5] == "profiles"
      && request.method() == http::verb::delete_) {
    return HandleDeploymentDirectorProfileDeleteRequest(
        state, path_parts[2], path_parts[4], path_parts[6]);
  }

  if (path_parts.size() == 7 && path_parts[3] == "directors"
      && path_parts[5] == "pools" && request.method() == http::verb::put) {
    return HandleDeploymentDirectorPoolPutRequest(state, request, path_parts[2],
                                                  path_parts[4], path_parts[6]);
  }
  if (path_parts.size() == 7 && path_parts[3] == "directors"
      && path_parts[5] == "pools" && request.method() == http::verb::delete_) {
    return HandleDeploymentDirectorPoolDeleteRequest(
        state, path_parts[2], path_parts[4], path_parts[6]);
  }

  if (path_parts.size() == 7 && path_parts[3] == "directors"
      && path_parts[5] == "catalogs" && request.method() == http::verb::put) {
    return HandleDeploymentDirectorCatalogPutRequest(
        state, request, path_parts[2], path_parts[4], path_parts[6]);
  }
  if (path_parts.size() == 7 && path_parts[3] == "directors"
      && path_parts[5] == "catalogs"
      && request.method() == http::verb::delete_) {
    return HandleDeploymentDirectorCatalogDeleteRequest(
        state, path_parts[2], path_parts[4], path_parts[6]);
  }

  if (path_parts.size() == 7 && path_parts[3] == "directors"
      && path_parts[5] == "schedules" && request.method() == http::verb::put) {
    return HandleDeploymentDirectorSchedulePutRequest(
        state, request, path_parts[2], path_parts[4], path_parts[6]);
  }
  if (path_parts.size() == 7 && path_parts[3] == "directors"
      && path_parts[5] == "schedules"
      && request.method() == http::verb::delete_) {
    return HandleDeploymentDirectorScheduleDeleteRequest(
        state, path_parts[2], path_parts[4], path_parts[6]);
  }

  if (path_parts.size() == 4 && path_parts[3] == "git-status"
      && request.method() == http::verb::get) {
    return HandleDeploymentGitStatusRequest(state, path_parts[2]);
  }

  if (path_parts.size() == 4 && path_parts[3] == "diff"
      && request.method() == http::verb::get) {
    return HandleDeploymentDiffPreviewRequest(state, path_parts[2]);
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
    for (const auto& job : state.ListJobs()) { AppendJob(jobs.get(), job); }
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
    if (!job) {
      return ErrorResponse(http::status::not_found, "job not found.");
    }

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
    ServiceState& state,
    const http::request<http::string_body>& request)
{
  DebugLog("handling " + std::string{request.method_string()} + " "
           + std::string{request.target()});
  const auto target
      = std::string_view{request.target().data(), request.target().size()};
  const auto path_parts = SplitPath(
      std::string_view{request.target().data(), request.target().size()});

  if (request.method() == http::verb::get
      && (target == "/" || target == "/ui" || target == "/ui/")) {
    return HtmlResponse(http::status::ok, BuildTestUiHtml());
  }

  if (path_parts.empty()) {
    return ErrorResponse(http::status::not_found, "route not found.");
  }

  if (request.method() != http::verb::get
      && request.method() != http::verb::post
      && request.method() != http::verb::put
      && request.method() != http::verb::delete_) {
    return ErrorResponse(
        http::status::method_not_allowed,
        "only GET, POST, PUT, and DELETE are currently supported.");
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
  tcp::acceptor acceptor{io_context,
                         tcp::endpoint{net::ip::make_address(address), port}};

  std::cout << "bconfig-service listening on " << address << ":" << port
            << std::endl;
  DebugLog("server listening on " + address + ":" + std::to_string(port));

  for (;;) {
    tcp::socket socket{io_context};
    acceptor.accept(socket);

    beast::flat_buffer buffer;
    http::request<http::string_body> request;
    http::read(socket, buffer, request);

    auto response = HandleRequest(state, request);
    DebugLog("responding with status "
             + std::to_string(static_cast<unsigned>(response.result_int()))
             + " to " + std::string{request.method_string()} + " "
             + std::string{request.target()});
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
  std::string state_dir
      = (bconfig::service::DefaultStorageBasePath() / "service-state").string();
  application.add_option("--address", address,
                         "Address to listen on for HTTP requests.");
  application.add_option("--port", port,
                         "Port to listen on for HTTP requests.");
  application.add_option("--state-dir", state_dir,
                         "Directory for persistent service state.");

  ParseBareosApp(application, argc, argv);
  OSDependentInit();
  state_dir = bconfig::service::ExpandUserPath(state_dir).string();

  try {
    bconfig::service::ServiceState state{state_dir};
    bconfig::service::RunServer(address, port, state);
  } catch (const std::exception& error) {
    std::cerr << "bconfig-service failed: " << error.what() << std::endl;
    return BEXIT_FAILURE;
  }

  return BEXIT_SUCCESS;
}
