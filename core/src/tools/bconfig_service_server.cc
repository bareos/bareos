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
  std::optional<std::string> address{};
  std::optional<uint16_t> port{};
  std::optional<std::vector<std::string>> allowed_script_dirs{};
  std::optional<std::vector<std::string>> allowed_job_commands{};
  std::optional<bool> tls_authenticate{};
  std::optional<bool> tls_enable{};
  std::optional<bool> tls_require{};
  std::optional<bool> tls_verify_peer{};
  std::optional<std::string> tls_cipher_list{};
  std::optional<std::string> tls_cipher_suites{};
  std::optional<std::string> tls_dh_file{};
  std::optional<std::string> tls_protocol{};
  std::optional<std::string> tls_ca_certificate_file{};
  std::optional<std::string> tls_ca_certificate_dir{};
  std::optional<std::string> tls_certificate_revocation_list{};
  std::optional<std::string> tls_certificate{};
  std::optional<std::string> tls_key{};
  std::optional<std::vector<std::string>> tls_allowed_cn{};
  std::optional<bool> connection_from_director_to_client{};
  std::optional<bool> connection_from_client_to_director{};
  std::optional<bool> monitor{};
  std::optional<uint64_t> maximum_bandwidth_per_job{};
};

struct DirectorClientRequestSpec {
  std::optional<std::string> address{};
  std::optional<uint16_t> port{};
  std::optional<std::string> password{};
  std::optional<bool> enabled{};
  std::optional<bool> passive{};
  std::optional<bool> connection_from_director_to_client{};
  std::optional<bool> connection_from_client_to_director{};
  std::optional<uint64_t> heartbeat_interval{};
  std::optional<uint64_t> maximum_bandwidth_per_job{};
  std::optional<std::string> description{};
};

struct DirectorStorageRequestSpec {
  std::optional<std::string> address{};
  std::optional<uint16_t> port{};
  std::optional<std::string> password{};
  std::optional<std::string> device{};
  std::optional<std::string> media_type{};
  std::optional<bool> enabled{};
  std::optional<bool> allow_compression{};
  std::optional<uint64_t> heartbeat_interval{};
  std::optional<uint64_t> maximum_bandwidth_per_job{};
  std::optional<std::string> archive_device{};
  std::optional<std::string> device_type{};
  std::optional<std::string> description{};
};

struct DirectorConsoleRequestSpec {
  std::optional<std::string> password{};
  std::optional<std::string> description{};
  std::optional<bool> use_pam_authentication{};
};

struct ConsoleConsoleRequestSpec {
  std::optional<std::string> director{};
  std::optional<std::string> password{};
  std::optional<std::string> description{};
  std::optional<std::string> rc_file{};
  std::optional<std::string> history_file{};
  std::optional<uint32_t> history_length{};
  std::optional<uint64_t> heartbeat_interval{};
  std::optional<bool> tls_authenticate{};
  std::optional<bool> tls_enable{};
  std::optional<bool> tls_require{};
  std::optional<bool> tls_verify_peer{};
  std::optional<std::string> tls_cipher_list{};
  std::optional<std::string> tls_cipher_suites{};
  std::optional<std::string> tls_dh_file{};
  std::optional<std::string> tls_protocol{};
  std::optional<std::string> tls_ca_certificate_file{};
  std::optional<std::string> tls_ca_certificate_dir{};
  std::optional<std::string> tls_certificate_revocation_list{};
  std::optional<std::string> tls_certificate{};
  std::optional<std::string> tls_key{};
  std::optional<std::vector<std::string>> tls_allowed_cn{};
};

struct ConsoleDirectorRequestSpec {
  std::optional<std::string> address{};
  std::optional<uint16_t> port{};
  std::optional<std::string> password{};
  std::optional<std::string> description{};
  std::optional<uint64_t> heartbeat_interval{};
  std::optional<bool> tls_authenticate{};
  std::optional<bool> tls_enable{};
  std::optional<bool> tls_require{};
  std::optional<bool> tls_verify_peer{};
  std::optional<std::string> tls_cipher_list{};
  std::optional<std::string> tls_cipher_suites{};
  std::optional<std::string> tls_dh_file{};
  std::optional<std::string> tls_protocol{};
  std::optional<std::string> tls_ca_certificate_file{};
  std::optional<std::string> tls_ca_certificate_dir{};
  std::optional<std::string> tls_certificate_revocation_list{};
  std::optional<std::string> tls_certificate{};
  std::optional<std::string> tls_key{};
  std::optional<std::vector<std::string>> tls_allowed_cn{};
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

struct DirectorMessagesRequestSpec {
  std::optional<std::string> description{};
  std::optional<std::vector<std::string>> entries{};
};

struct StorageDirectorRequestSpec {
  std::optional<std::string> password{};
  std::optional<std::string> description{};
  std::optional<bool> monitor{};
  std::optional<uint64_t> maximum_bandwidth_per_job{};
};

struct StorageDeviceRequestSpec {
  std::optional<std::string> media_type{};
  std::optional<std::string> archive_device{};
  std::optional<std::string> device_type{};
  std::optional<std::string> description{};
};

struct StorageNdmpRequestSpec {
  std::optional<std::string> username{};
  std::optional<std::string> password{};
  std::optional<std::string> auth_type{};
  std::optional<uint32_t> log_level{};
};

struct StorageAutochangerRequestSpec {
  std::optional<std::vector<std::string>> devices{};
  std::optional<std::string> changer_device{};
  std::optional<std::string> changer_command{};
  std::optional<std::string> description{};
};

struct StorageDaemonRequestSpec {
  std::optional<std::string> address{};
  std::optional<std::vector<std::string>> addresses{};
  std::optional<std::string> source_address{};
  std::optional<std::vector<std::string>> source_addresses{};
  std::optional<uint16_t> port{};
  std::optional<bool> just_in_time_reservation{};
  std::optional<uint32_t> maximum_concurrent_jobs{};
  std::optional<uint32_t> maximum_workers_per_job{};
  std::optional<uint32_t> absolute_job_timeout{};
  std::optional<bool> allow_bandwidth_bursting{};
  std::optional<bool> tls_authenticate{};
  std::optional<bool> tls_enable{};
  std::optional<bool> tls_require{};
  std::optional<bool> tls_verify_peer{};
  std::optional<std::string> tls_cipher_list{};
  std::optional<std::string> tls_cipher_suites{};
  std::optional<std::string> tls_dh_file{};
  std::optional<std::string> tls_protocol{};
  std::optional<std::string> tls_ca_certificate_file{};
  std::optional<std::string> tls_ca_certificate_dir{};
  std::optional<std::string> tls_certificate_revocation_list{};
  std::optional<std::string> tls_certificate{};
  std::optional<std::string> tls_key{};
  std::optional<std::vector<std::string>> tls_allowed_cn{};
  std::optional<bool> pki_signatures{};
  std::optional<bool> pki_encryption{};
  std::optional<std::string> pki_key_pair{};
  std::optional<std::vector<std::string>> pki_signers{};
  std::optional<std::vector<std::string>> pki_master_keys{};
  std::optional<std::string> pki_cipher{};
  std::optional<bool> always_use_lmdb{};
  std::optional<uint32_t> lmdb_threshold{};
  std::optional<bool> ndmp_enable{};
  std::optional<bool> ndmp_snooping{};
  std::optional<uint32_t> ndmp_log_level{};
  std::optional<std::string> ndmp_address{};
  std::optional<uint16_t> ndmp_port{};
  std::optional<std::vector<std::string>> ndmp_addresses{};
  std::optional<bool> autoxflate_on_replication{};
  std::optional<bool> collect_device_statistics{};
  std::optional<bool> collect_job_statistics{};
  std::optional<uint32_t> statistics_collect_interval{};
  std::optional<bool> device_reserve_by_media_type{};
  std::optional<bool> file_device_concurrent_read{};
  std::optional<std::string> ver_id{};
  std::optional<std::string> log_timestamp_format{};
  std::optional<uint64_t> maximum_bandwidth_per_job{};
  std::optional<std::string> secure_erase_command{};
  std::optional<std::string> grpc_module{};
  std::optional<bool> enable_ktls{};
  std::optional<uint64_t> sd_connect_timeout{};
  std::optional<uint64_t> fd_connect_timeout{};
  std::optional<uint64_t> heartbeat_interval{};
  std::optional<uint64_t> checkpoint_interval{};
  std::optional<uint64_t> client_connect_wait{};
  std::optional<uint32_t> maximum_network_buffer_size{};
  std::optional<std::string> description{};
  std::optional<std::string> working_directory{};
  std::optional<std::string> plugin_directory{};
  std::optional<std::vector<std::string>> plugin_names{};
#if defined(HAVE_DYNAMIC_SD_BACKENDS)
  std::optional<std::vector<std::string>> backend_directories{};
#endif
  std::optional<std::vector<std::string>> allowed_script_dirs{};
  std::optional<std::vector<std::string>> allowed_job_commands{};
  std::optional<std::string> scripts_directory{};
  std::optional<std::string> messages{};
};

struct DirectorScheduleRequestSpec {
  std::optional<std::string> description{};
  std::optional<bool> enabled{};
  std::optional<std::vector<std::string>> run_entries{};
};

struct DirectorCounterRequestSpec {
  std::optional<int32_t> minimum{};
  std::optional<uint32_t> maximum{};
  std::optional<std::string> wrap_counter{};
  std::optional<std::string> catalog{};
  std::optional<std::string> description{};
};

struct DirectorFilesetRequestSpec {
  std::optional<std::string> description{};
  std::optional<bool> ignore_fileset_changes{};
  std::optional<bool> enable_vss{};
  std::optional<std::vector<std::string>> include_blocks{};
  std::optional<std::vector<std::string>> exclude_blocks{};
};

struct DirectorJobRequestSpec {
  std::optional<std::string> description{};
  std::optional<std::string> type{};
  std::optional<std::string> level{};
  std::optional<std::string> messages{};
  std::optional<std::vector<std::string>> storages{};
  std::optional<std::string> pool{};
  std::optional<std::string> full_backup_pool{};
  std::optional<std::string> virtual_full_backup_pool{};
  std::optional<std::string> incremental_backup_pool{};
  std::optional<std::string> differential_backup_pool{};
  std::optional<std::string> next_pool{};
  std::optional<std::string> client{};
  std::optional<std::string> fileset{};
  std::optional<std::string> schedule{};
  std::optional<std::string> verify_job{};
  std::optional<std::string> catalog{};
  std::optional<std::string> jobdefs{};
  std::optional<std::string> where{};
  std::optional<int32_t> priority{};
  std::optional<bool> enabled{};
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
std::optional<ConsoleConsoleRequestSpec> ParseConsoleConsoleRequest(
    std::string_view body,
    std::string& error);
std::optional<ConsoleDirectorRequestSpec> ParseConsoleDirectorRequest(
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
std::optional<DirectorMessagesRequestSpec> ParseDirectorMessagesRequest(
    std::string_view body,
    std::string& error);
std::optional<StorageDirectorRequestSpec> ParseStorageDirectorRequest(
    std::string_view body,
    std::string& error);
std::optional<StorageDeviceRequestSpec> ParseStorageDeviceRequest(
    std::string_view body,
    std::string& error);
std::optional<StorageNdmpRequestSpec> ParseStorageNdmpRequest(
    std::string_view body,
    std::string& error);
std::optional<StorageAutochangerRequestSpec> ParseStorageAutochangerRequest(
    std::string_view body,
    std::string& error);
std::optional<StorageDaemonRequestSpec> ParseStorageDaemonRequest(
    std::string_view body,
    std::string& error);
std::optional<DirectorScheduleRequestSpec> ParseDirectorScheduleRequest(
    std::string_view body,
    std::string& error);
std::optional<DirectorCounterRequestSpec> ParseDirectorCounterRequest(
    std::string_view body,
    std::string& error);
std::optional<DirectorFilesetRequestSpec> ParseDirectorFilesetRequest(
    std::string_view body,
    std::string& error);
std::optional<DirectorJobRequestSpec> ParseDirectorJobRequest(
    std::string_view body,
    std::string& error);

JsonPtr MakeJson(json_t* value) { return JsonPtr(value, &json_decref); }

std::string MakeDebugTimestamp()
{
  const auto now = std::chrono::system_clock::now();
  const auto now_seconds = std::chrono::system_clock::to_time_t(now);
  const auto subseconds = std::chrono::duration_cast<std::chrono::microseconds>(
                              now.time_since_epoch())
                          % std::chrono::seconds(1);
  constexpr int kSubsecondDigits = 6;
  std::tm local{};
#if HAVE_WIN32
  localtime_s(&local, &now_seconds);
#else
  localtime_r(&now_seconds, &local);
#endif

  std::ostringstream stream;
  stream << std::put_time(&local, "%Y-%m-%dT%H:%M:%S");
  if constexpr (kSubsecondDigits > 0) {
    stream << '.' << std::setw(kSubsecondDigits) << std::setfill('0')
           << subseconds.count();
  }
  char offset_buffer[8]{};
  if (std::strftime(offset_buffer, sizeof(offset_buffer), "%z", &local) == 5) {
    stream << std::string_view{offset_buffer, 3} << ':'
           << std::string_view{offset_buffer + 3, 2};
  } else {
    stream << "+00:00";
  }
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

        <label for="client-stub-address">Address</label>
        <input id="client-stub-address" name="address"
               placeholder="bareos-dir.example.invalid">

        <label for="client-stub-port">Port</label>
        <input id="client-stub-port" name="port" type="number" min="1" max="65535"
               placeholder="9101">

        <label for="client-stub-allowed-script-dirs">Allowed script dirs</label>
        <textarea id="client-stub-allowed-script-dirs" name="allowed_script_dirs"
                  rows="3" placeholder="/usr/lib/bareos/scripts"></textarea>

        <label for="client-stub-allowed-job-commands">Allowed job commands</label>
        <textarea id="client-stub-allowed-job-commands" name="allowed_job_commands"
                  rows="3" placeholder="run-before-job-client"></textarea>

        <label class="checkbox-label" for="client-stub-tls-authenticate">
          <input id="client-stub-tls-authenticate" name="tls_authenticate"
                 type="checkbox">
          TLS authenticate
        </label>

        <label class="checkbox-label" for="client-stub-tls-enable">
          <input id="client-stub-tls-enable" name="tls_enable" type="checkbox">
          TLS enable
        </label>

        <label class="checkbox-label" for="client-stub-tls-require">
          <input id="client-stub-tls-require" name="tls_require" type="checkbox">
          TLS require
        </label>

        <label class="checkbox-label" for="client-stub-tls-verify-peer">
          <input id="client-stub-tls-verify-peer" name="tls_verify_peer"
                 type="checkbox">
          TLS verify peer
        </label>

        <label for="client-stub-tls-cipher-list">TLS cipher list</label>
        <input id="client-stub-tls-cipher-list" name="tls_cipher_list"
               placeholder="DEFAULT:@SECLEVEL=2">

        <label for="client-stub-tls-cipher-suites">TLS cipher suites</label>
        <input id="client-stub-tls-cipher-suites" name="tls_cipher_suites"
               placeholder="TLS_AES_256_GCM_SHA384">

        <label for="client-stub-tls-dh-file">TLS DH file</label>
        <input id="client-stub-tls-dh-file" name="tls_dh_file"
               placeholder="/etc/bareos/dh4096.pem">

        <label for="client-stub-tls-protocol">TLS protocol</label>
        <input id="client-stub-tls-protocol" name="tls_protocol"
               placeholder="MinProtocol = TLSv1.2">

        <label for="client-stub-tls-ca-certificate-file">TLS CA certificate file</label>
        <input id="client-stub-tls-ca-certificate-file"
               name="tls_ca_certificate_file"
               placeholder="/etc/bareos/ca.pem">

        <label for="client-stub-tls-ca-certificate-dir">TLS CA certificate dir</label>
        <input id="client-stub-tls-ca-certificate-dir"
               name="tls_ca_certificate_dir"
               placeholder="/etc/ssl/certs">

        <label for="client-stub-tls-certificate-revocation-list">TLS certificate revocation list</label>
        <input id="client-stub-tls-certificate-revocation-list"
               name="tls_certificate_revocation_list"
               placeholder="/etc/bareos/crl.pem">

        <label for="client-stub-tls-certificate">TLS certificate</label>
        <input id="client-stub-tls-certificate" name="tls_certificate"
               placeholder="/etc/bareos/client.crt">

        <label for="client-stub-tls-key">TLS key</label>
        <input id="client-stub-tls-key" name="tls_key"
               placeholder="/etc/bareos/client.key">

        <label for="client-stub-tls-allowed-cn">TLS allowed CN</label>
        <textarea id="client-stub-tls-allowed-cn" name="tls_allowed_cn"
                  rows="3" placeholder="bareos-dir.example.invalid"></textarea>

        <label class="checkbox-label" for="client-stub-connection-from-director-to-client">
          <input id="client-stub-connection-from-director-to-client"
                 name="connection_from_director_to_client" type="checkbox">
          Connection from director to client
        </label>

        <label class="checkbox-label" for="client-stub-connection-from-client-to-director">
          <input id="client-stub-connection-from-client-to-director"
                 name="connection_from_client_to_director" type="checkbox">
          Connection from client to director
        </label>

        <label class="checkbox-label" for="client-stub-monitor">
          <input id="client-stub-monitor" name="monitor" type="checkbox">
          Monitor
        </label>

        <label for="client-stub-maximum-bandwidth-per-job">Maximum bandwidth per job</label>
        <input id="client-stub-maximum-bandwidth-per-job"
               name="maximum_bandwidth_per_job" type="number" min="0"
               placeholder="0">

        <button type="submit">
          PUT /v1/deployments/{id}/clients/{client}/directors/{director}
        </button>
        <button type="button" id="client-stub-delete-button">
          DELETE /v1/deployments/{id}/clients/{client}/directors/{director}
        </button>
      </form>
    </section>

    <section class="card">
      <h2>Upsert client daemon resource</h2>
      <form id="client-daemon-form">
        <label for="client-daemon-deployment-id">Deployment ID</label>
        <input id="client-daemon-deployment-id" name="deployment_id" value="prod">

        <label for="client-daemon-client-name">Client name</label>
        <input id="client-daemon-client-name" name="client_name"
               value="backup-bareos-test-fd">

        <label for="client-daemon-address">Address</label>
        <input id="client-daemon-address" name="address"
               placeholder="client.example.com">

        <label for="client-daemon-addresses">Addresses</label>
        <textarea id="client-daemon-addresses" name="addresses"
                  rows="3"
                  placeholder="host[ipv4;192.0.2.10;9102]&#10;host[ipv6;::1;9102]"></textarea>

        <label for="client-daemon-source-address">SourceAddress</label>
        <input id="client-daemon-source-address" name="source_address"
               placeholder="192.0.2.10">

        <label for="client-daemon-source-addresses">Source addresses</label>
        <textarea id="client-daemon-source-addresses" name="source_addresses"
                  rows="3"
                  placeholder="192.0.2.10&#10;198.51.100.10"></textarea>

        <label for="client-daemon-port">Port</label>
        <input id="client-daemon-port" name="port" type="number"
               min="1" max="65535" placeholder="9102">

        <label for="client-daemon-maximum-concurrent-jobs">Maximum concurrent jobs</label>
        <input id="client-daemon-maximum-concurrent-jobs"
               name="maximum_concurrent_jobs" type="number" min="0"
               placeholder="1000">

        <label for="client-daemon-maximum-workers-per-job">Maximum workers per job</label>
        <input id="client-daemon-maximum-workers-per-job"
               name="maximum_workers_per_job" type="number" min="0"
               placeholder="2">

        <label for="client-daemon-absolute-job-timeout">Absolute job timeout</label>
        <input id="client-daemon-absolute-job-timeout"
               name="absolute_job_timeout" type="number" min="0"
               placeholder="0">

        <label for="client-daemon-allow-bandwidth-bursting">Allow bandwidth bursting</label>
        <select id="client-daemon-allow-bandwidth-bursting"
                name="allow_bandwidth_bursting">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="client-daemon-tls-authenticate">TLS authenticate only</label>
        <select id="client-daemon-tls-authenticate" name="tls_authenticate">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="client-daemon-tls-enable">TLS enabled</label>
        <select id="client-daemon-tls-enable" name="tls_enable">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="client-daemon-tls-require">TLS required</label>
        <select id="client-daemon-tls-require" name="tls_require">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="client-daemon-tls-verify-peer">TLS verify peer</label>
        <select id="client-daemon-tls-verify-peer" name="tls_verify_peer">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="client-daemon-tls-cipher-list">TLS cipher list</label>
        <input id="client-daemon-tls-cipher-list" name="tls_cipher_list"
               placeholder="ECDHE-RSA-AES256-GCM-SHA384">

        <label for="client-daemon-tls-cipher-suites">TLS cipher suites</label>
        <input id="client-daemon-tls-cipher-suites"
               name="tls_cipher_suites"
               placeholder="TLS_AES_256_GCM_SHA384">

        <label for="client-daemon-tls-dh-file">TLS DH file</label>
        <input id="client-daemon-tls-dh-file" name="tls_dh_file"
               placeholder="/etc/bareos/dh4096.pem">

        <label for="client-daemon-tls-protocol">TLS protocol</label>
        <input id="client-daemon-tls-protocol" name="tls_protocol"
               placeholder="MinProtocol = TLSv1.2">

        <label for="client-daemon-tls-ca-certificate-file">TLS CA certificate file</label>
        <input id="client-daemon-tls-ca-certificate-file"
               name="tls_ca_certificate_file"
               placeholder="/etc/bareos/ca.crt">

        <label for="client-daemon-tls-ca-certificate-dir">TLS CA certificate dir</label>
        <input id="client-daemon-tls-ca-certificate-dir"
               name="tls_ca_certificate_dir"
               placeholder="/etc/ssl/certs">

        <label for="client-daemon-tls-certificate-revocation-list">TLS certificate revocation list</label>
        <input id="client-daemon-tls-certificate-revocation-list"
               name="tls_certificate_revocation_list"
               placeholder="/etc/bareos/crl.pem">

        <label for="client-daemon-tls-certificate">TLS certificate</label>
        <input id="client-daemon-tls-certificate"
               name="tls_certificate"
               placeholder="/etc/bareos/client.crt">

        <label for="client-daemon-tls-key">TLS key</label>
        <input id="client-daemon-tls-key" name="tls_key"
               placeholder="/etc/bareos/client.key">

        <label for="client-daemon-tls-allowed-cn">TLS allowed CNs</label>
        <textarea id="client-daemon-tls-allowed-cn" name="tls_allowed_cn"
                  rows="3"
                  placeholder="client-cn-1&#10;client-cn-2"></textarea>

        <label for="client-daemon-pki-signatures">PKI signatures</label>
        <select id="client-daemon-pki-signatures" name="pki_signatures">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="client-daemon-pki-encryption">PKI encryption</label>
        <select id="client-daemon-pki-encryption" name="pki_encryption">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="client-daemon-pki-key-pair">PKI key pair</label>
        <input id="client-daemon-pki-key-pair" name="pki_key_pair"
               placeholder="/etc/bareos/client.pem">

        <label for="client-daemon-pki-signers">PKI signer paths</label>
        <textarea id="client-daemon-pki-signers" name="pki_signers"
                  rows="3"
                  placeholder="/etc/bareos/signer1.pem&#10;/etc/bareos/signer2.pem"></textarea>

        <label for="client-daemon-pki-master-keys">PKI master key paths</label>
        <textarea id="client-daemon-pki-master-keys" name="pki_master_keys"
                  rows="3"
                  placeholder="/etc/bareos/recipient1.pem&#10;/etc/bareos/recipient2.pem"></textarea>

        <label for="client-daemon-pki-cipher">PKI cipher</label>
        <input id="client-daemon-pki-cipher" name="pki_cipher"
               placeholder="aes256">

        <label for="client-daemon-always-use-lmdb">Always use LMDB</label>
        <select id="client-daemon-always-use-lmdb" name="always_use_lmdb">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="client-daemon-lmdb-threshold">LMDB threshold</label>
        <input id="client-daemon-lmdb-threshold" name="lmdb_threshold"
               type="number" min="0" placeholder="0">

        <label for="client-daemon-ver-id">VerId</label>
        <input id="client-daemon-ver-id" name="ver_id"
               placeholder="bareos-fd-custom">

        <label for="client-daemon-log-timestamp-format">Log timestamp format</label>
        <input id="client-daemon-log-timestamp-format"
               name="log_timestamp_format" placeholder="%d-%b %H:%M">

        <label for="client-daemon-maximum-bandwidth-per-job">Maximum bandwidth per job</label>
        <input id="client-daemon-maximum-bandwidth-per-job"
               name="maximum_bandwidth_per_job" type="number" min="0"
               placeholder="0">

        <label for="client-daemon-secure-erase-command">Secure erase command</label>
        <input id="client-daemon-secure-erase-command"
               name="secure_erase_command"
               placeholder="/usr/bin/shred -n 3 -u">

        <label for="client-daemon-grpc-module">gRPC module</label>
        <input id="client-daemon-grpc-module" name="grpc_module"
               placeholder="bareos-grpc-fd-plugin-bridge">

        <label for="client-daemon-enable-ktls">Enable kTLS</label>
        <select id="client-daemon-enable-ktls" name="enable_ktls">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="client-daemon-sd-connect-timeout">Storage connect timeout</label>
        <input id="client-daemon-sd-connect-timeout"
               name="sd_connect_timeout" type="number" min="0"
               placeholder="1800">

        <label for="client-daemon-heartbeat-interval">Heartbeat interval</label>
        <input id="client-daemon-heartbeat-interval" name="heartbeat_interval"
               type="number" min="0" placeholder="0">

        <label for="client-daemon-maximum-network-buffer-size">Maximum network buffer size</label>
        <input id="client-daemon-maximum-network-buffer-size"
               name="maximum_network_buffer_size" type="number" min="0"
               placeholder="0">

        <label for="client-daemon-description">Description</label>
        <input id="client-daemon-description" name="description"
               placeholder="Managed client daemon resource">

        <label for="client-daemon-working-directory">Working directory</label>
        <input id="client-daemon-working-directory" name="working_directory"
               placeholder="/var/lib/bareos/client">

        <label for="client-daemon-plugin-directory">Plugin directory</label>
        <input id="client-daemon-plugin-directory" name="plugin_directory"
               placeholder="/usr/lib/bareos/plugins">

        <label for="client-daemon-plugin-names">Plugin names</label>
        <textarea id="client-daemon-plugin-names" name="plugin_names"
                  rows="3"
                  placeholder="python&#10;grpc"></textarea>

        <label for="client-daemon-allowed-script-dirs">Allowed script dirs</label>
        <textarea id="client-daemon-allowed-script-dirs"
                  name="allowed_script_dirs" rows="3"
                  placeholder="/usr/lib/bareos/scripts&#10;/opt/bareos/scripts"></textarea>

        <label for="client-daemon-allowed-job-commands">Allowed job commands</label>
        <textarea id="client-daemon-allowed-job-commands"
                  name="allowed_job_commands" rows="3"
                  placeholder="RunBeforeJob&#10;Estimate listing"></textarea>

        <label for="client-daemon-scripts-directory">Scripts directory</label>
        <input id="client-daemon-scripts-directory" name="scripts_directory"
               placeholder="/usr/lib/bareos/scripts">

        <label for="client-daemon-messages">Messages</label>
        <input id="client-daemon-messages" name="messages" placeholder="Standard">

        <button type="submit">
          PUT /v1/deployments/{id}/clients/{client}
        </button>
      </form>
    </section>

    <section class="card">
      <h2>Upsert client messages resource</h2>
      <form id="client-messages-form">
        <label for="client-messages-deployment-id">Deployment ID</label>
        <input id="client-messages-deployment-id" name="deployment_id" value="prod">

        <label for="client-messages-client-name">Client name</label>
        <input id="client-messages-client-name" name="client_name"
               value="backup-bareos-test-fd">

        <label for="client-messages-messages-name">Messages name</label>
        <input id="client-messages-messages-name" name="messages_name" value="ManagedMessages">

        <label for="client-messages-description">Description</label>
        <input id="client-messages-description" name="description"
               placeholder="Managed client messages resource">

        <label for="client-messages-entries">Message entries</label>
        <textarea id="client-messages-entries" name="entries"
                  rows="4"
                  placeholder="Director = bareos-dir = all"></textarea>

        <button type="submit">
          PUT /v1/deployments/{id}/clients/{client}/messages/{messages}
        </button>
        <button type="button" id="client-messages-delete-button">
          DELETE /v1/deployments/{id}/clients/{client}/messages/{messages}
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

        <label class="checkbox-label" for="director-client-enabled">
          <input id="director-client-enabled" name="enabled" type="checkbox" checked>
          Enabled
        </label>

        <label class="checkbox-label" for="director-client-passive">
          <input id="director-client-passive" name="passive" type="checkbox">
          Passive
        </label>

        <label class="checkbox-label" for="director-client-connection-from-director-to-client">
          <input id="director-client-connection-from-director-to-client"
                 name="connection_from_director_to_client" type="checkbox">
          Connection from director to client
        </label>

        <label class="checkbox-label" for="director-client-connection-from-client-to-director">
          <input id="director-client-connection-from-client-to-director"
                 name="connection_from_client_to_director" type="checkbox">
          Connection from client to director
        </label>

        <label for="director-client-maximum-bandwidth-per-job">Maximum bandwidth per job</label>
        <input id="director-client-maximum-bandwidth-per-job"
               name="maximum_bandwidth_per_job" type="number" min="0"
               placeholder="0">

        <label for="director-client-heartbeat-interval">Heartbeat interval</label>
        <input id="director-client-heartbeat-interval" name="heartbeat_interval"
               type="number" min="0" placeholder="0">

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

        <label class="checkbox-label" for="director-storage-enabled">
          <input id="director-storage-enabled" name="enabled" type="checkbox" checked>
          Enabled
        </label>

        <label class="checkbox-label" for="director-storage-allow-compression">
          <input id="director-storage-allow-compression" name="allow_compression"
                 type="checkbox" checked>
          AllowCompression
        </label>

        <label for="director-storage-maximum-bandwidth-per-job">Maximum bandwidth per job</label>
        <input id="director-storage-maximum-bandwidth-per-job"
               name="maximum_bandwidth_per_job" type="number" min="0"
               placeholder="0">

        <label for="director-storage-heartbeat-interval">Heartbeat interval</label>
        <input id="director-storage-heartbeat-interval" name="heartbeat_interval"
               type="number" min="0" placeholder="0">

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

    <section class="card">
      <h2>Upsert director messages resource</h2>
      <form id="director-messages-form">
        <label for="director-messages-deployment-id">Deployment ID</label>
        <input id="director-messages-deployment-id" name="deployment_id" value="prod">

        <label for="director-messages-director-name">Director name</label>
        <input id="director-messages-director-name" name="director_name" value="bareos-dir">

        <label for="director-messages-messages-name">Messages name</label>
        <input id="director-messages-messages-name" name="messages_name" value="ManagedMessages">

        <label for="director-messages-description">Description</label>
        <input id="director-messages-description" name="description"
               placeholder="Managed messages resource">

        <label for="director-messages-entries">Message entries</label>
        <textarea id="director-messages-entries" name="entries"
                  rows="6"
                  placeholder="console = all, !skipped, !saved, !audit"></textarea>

        <button type="submit">
          PUT /v1/deployments/{id}/directors/{director}/messages/{messages}
        </button>
        <button type="button" id="director-messages-delete-button">
          DELETE /v1/deployments/{id}/directors/{director}/messages/{messages}
        </button>
      </form>
    </section>

    <section class="card">
      <h2>Upsert director counter resource</h2>
      <form id="director-counter-form">
        <label for="director-counter-deployment-id">Deployment ID</label>
        <input id="director-counter-deployment-id" name="deployment_id" value="prod">

        <label for="director-counter-director-name">Director name</label>
        <input id="director-counter-director-name" name="director_name" value="bareos-dir">

        <label for="director-counter-counter-name">Counter name</label>
        <input id="director-counter-counter-name" name="counter_name" value="ManagedCounter">

        <label for="director-counter-minimum">Minimum</label>
        <input id="director-counter-minimum" name="minimum" type="number">

        <label for="director-counter-maximum">Maximum</label>
        <input id="director-counter-maximum" name="maximum" type="number" min="0">

        <label for="director-counter-wrap-counter">WrapCounter</label>
        <input id="director-counter-wrap-counter" name="wrap_counter"
               placeholder="OtherCounter">

        <label for="director-counter-catalog">Catalog</label>
        <input id="director-counter-catalog" name="catalog"
               placeholder="MyCatalog">

        <label for="director-counter-description">Description</label>
        <input id="director-counter-description" name="description"
               placeholder="Managed counter resource">

        <button type="submit">
          PUT /v1/deployments/{id}/directors/{director}/counters/{counter}
        </button>
        <button type="button" id="director-counter-delete-button">
          DELETE /v1/deployments/{id}/directors/{director}/counters/{counter}
        </button>
      </form>
    </section>

    <section class="card">
      <h2>Upsert director fileset resource</h2>
      <form id="director-fileset-form">
        <label for="director-fileset-deployment-id">Deployment ID</label>
        <input id="director-fileset-deployment-id" name="deployment_id" value="prod">

        <label for="director-fileset-director-name">Director name</label>
        <input id="director-fileset-director-name" name="director_name" value="bareos-dir">

        <label for="director-fileset-fileset-name">FileSet name</label>
        <input id="director-fileset-fileset-name" name="fileset_name" value="ManagedFileSet">

        <label for="director-fileset-description">Description</label>
        <input id="director-fileset-description" name="description"
               placeholder="Managed fileset resource">

        <label class="checkbox-label" for="director-fileset-ignore-fileset-changes">
          <input id="director-fileset-ignore-fileset-changes"
                 name="ignore_fileset_changes" type="checkbox">
          IgnoreFileSetChanges
        </label>

        <label class="checkbox-label" for="director-fileset-enable-vss">
          <input id="director-fileset-enable-vss"
                 name="enable_vss" type="checkbox" checked>
          EnableVSS
        </label>

        <label for="director-fileset-include-blocks">Include blocks</label>
        <textarea id="director-fileset-include-blocks" name="include_blocks"
                  rows="8"
                  placeholder="Include {&#10;  Options {&#10;    Signature = XXH128&#10;  }&#10;  File = /tmp&#10;}"></textarea>

        <label for="director-fileset-exclude-blocks">Exclude blocks</label>
        <textarea id="director-fileset-exclude-blocks" name="exclude_blocks"
                  rows="6"
                  placeholder="Exclude {&#10;  File = /tmp/cache&#10;}"></textarea>

        <button type="submit">
          PUT /v1/deployments/{id}/directors/{director}/filesets/{fileset}
        </button>
        <button type="button" id="director-fileset-delete-button">
          DELETE /v1/deployments/{id}/directors/{director}/filesets/{fileset}
        </button>
      </form>
    </section>

    <section class="card">
      <h2>Upsert director job resource</h2>
      <form id="director-job-form">
        <label for="director-job-deployment-id">Deployment ID</label>
        <input id="director-job-deployment-id" name="deployment_id" value="prod">

        <label for="director-job-director-name">Director name</label>
        <input id="director-job-director-name" name="director_name" value="bareos-dir">

        <label for="director-job-job-name">Job name</label>
        <input id="director-job-job-name" name="job_name" value="ManagedJob">

        <label for="director-job-description">Description</label>
        <input id="director-job-description" name="description"
               placeholder="Managed job resource">

        <label for="director-job-type">Type</label>
        <input id="director-job-type" name="type" placeholder="Backup">

        <label for="director-job-level">Level</label>
        <input id="director-job-level" name="level" placeholder="Incremental">

        <label for="director-job-messages">Messages</label>
        <input id="director-job-messages" name="messages" placeholder="Standard">

        <label for="director-job-storages">Storage entries</label>
        <textarea id="director-job-storages" name="storages" rows="3"
                  placeholder="File"></textarea>

        <label for="director-job-pool">Pool</label>
        <input id="director-job-pool" name="pool" placeholder="Incremental">

        <label for="director-job-full-backup-pool">FullBackupPool</label>
        <input id="director-job-full-backup-pool" name="full_backup_pool"
               placeholder="Full">

        <label for="director-job-virtual-full-backup-pool">VirtualFullBackupPool</label>
        <input id="director-job-virtual-full-backup-pool"
               name="virtual_full_backup_pool">

        <label for="director-job-incremental-backup-pool">IncrementalBackupPool</label>
        <input id="director-job-incremental-backup-pool"
               name="incremental_backup_pool" placeholder="Incremental">

        <label for="director-job-differential-backup-pool">DifferentialBackupPool</label>
        <input id="director-job-differential-backup-pool"
               name="differential_backup_pool" placeholder="Differential">

        <label for="director-job-next-pool">NextPool</label>
        <input id="director-job-next-pool" name="next_pool">

        <label for="director-job-client">Client</label>
        <input id="director-job-client" name="client" placeholder="bareos-fd">

        <label for="director-job-fileset">FileSet</label>
        <input id="director-job-fileset" name="fileset" placeholder="SelfTest">

        <label for="director-job-schedule">Schedule</label>
        <input id="director-job-schedule" name="schedule" placeholder="WeeklyCycle">

        <label for="director-job-verify-job">JobToVerify</label>
        <input id="director-job-verify-job" name="verify_job">

        <label for="director-job-catalog">Catalog</label>
        <input id="director-job-catalog" name="catalog" placeholder="MyCatalog">

        <label for="director-job-jobdefs">JobDefs</label>
        <input id="director-job-jobdefs" name="jobdefs" placeholder="DefaultJob">

        <label for="director-job-where">Where</label>
        <input id="director-job-where" name="where">

        <label for="director-job-priority">Priority</label>
        <input id="director-job-priority" name="priority" type="number">

        <label class="checkbox-label" for="director-job-enabled">
          <input id="director-job-enabled" name="enabled" type="checkbox" checked>
          Enabled
        </label>

        <button type="submit">
          PUT /v1/deployments/{id}/directors/{director}/jobs/{job}
        </button>
        <button type="button" id="director-job-delete-button">
          DELETE /v1/deployments/{id}/directors/{director}/jobs/{job}
        </button>
      </form>
    </section>

    <section class="card">
      <h2>Upsert director jobdefs resource</h2>
      <form id="director-jobdefs-form">
        <label for="director-jobdefs-deployment-id">Deployment ID</label>
        <input id="director-jobdefs-deployment-id" name="deployment_id" value="prod">

        <label for="director-jobdefs-director-name">Director name</label>
        <input id="director-jobdefs-director-name" name="director_name" value="bareos-dir">

        <label for="director-jobdefs-jobdefs-name">JobDefs name</label>
        <input id="director-jobdefs-jobdefs-name" name="jobdefs_name" value="ManagedJobDefs">

        <label for="director-jobdefs-description">Description</label>
        <input id="director-jobdefs-description" name="description"
               placeholder="Managed jobdefs resource">

        <label for="director-jobdefs-type">Type</label>
        <input id="director-jobdefs-type" name="type" placeholder="Backup">

        <label for="director-jobdefs-level">Level</label>
        <input id="director-jobdefs-level" name="level" placeholder="Incremental">

        <label for="director-jobdefs-messages">Messages</label>
        <input id="director-jobdefs-messages" name="messages" placeholder="Standard">

        <label for="director-jobdefs-storages">Storage entries</label>
        <textarea id="director-jobdefs-storages" name="storages" rows="3"
                  placeholder="File"></textarea>

        <label for="director-jobdefs-pool">Pool</label>
        <input id="director-jobdefs-pool" name="pool" placeholder="Incremental">

        <label for="director-jobdefs-full-backup-pool">FullBackupPool</label>
        <input id="director-jobdefs-full-backup-pool" name="full_backup_pool"
               placeholder="Full">

        <label for="director-jobdefs-incremental-backup-pool">IncrementalBackupPool</label>
        <input id="director-jobdefs-incremental-backup-pool"
               name="incremental_backup_pool" placeholder="Incremental">

        <label for="director-jobdefs-differential-backup-pool">DifferentialBackupPool</label>
        <input id="director-jobdefs-differential-backup-pool"
               name="differential_backup_pool" placeholder="Differential">

        <label for="director-jobdefs-client">Client</label>
        <input id="director-jobdefs-client" name="client" placeholder="bareos-fd">

        <label for="director-jobdefs-fileset">FileSet</label>
        <input id="director-jobdefs-fileset" name="fileset" placeholder="SelfTest">

        <label for="director-jobdefs-schedule">Schedule</label>
        <input id="director-jobdefs-schedule" name="schedule" placeholder="WeeklyCycle">

        <label for="director-jobdefs-catalog">Catalog</label>
        <input id="director-jobdefs-catalog" name="catalog" placeholder="MyCatalog">

        <label for="director-jobdefs-priority">Priority</label>
        <input id="director-jobdefs-priority" name="priority" type="number">

        <label class="checkbox-label" for="director-jobdefs-enabled">
          <input id="director-jobdefs-enabled" name="enabled" type="checkbox" checked>
          Enabled
        </label>

        <button type="submit">
          PUT /v1/deployments/{id}/directors/{director}/jobdefs/{jobdefs}
        </button>
        <button type="button" id="director-jobdefs-delete-button">
          DELETE /v1/deployments/{id}/directors/{director}/jobdefs/{jobdefs}
        </button>
      </form>
    </section>

    <section class="card">
      <h2>Upsert storage-daemon storage resource</h2>
      <form id="storage-daemon-form">
        <label for="storage-daemon-deployment-id">Deployment ID</label>
        <input id="storage-daemon-deployment-id" name="deployment_id" value="prod">

        <label for="storage-daemon-storage-name">Storage name</label>
        <input id="storage-daemon-storage-name" name="storage_name" value="bareos-sd">

        <label for="storage-daemon-address">Address</label>
        <input id="storage-daemon-address" name="address"
               placeholder="storage.example.com">

        <label for="storage-daemon-addresses">Addresses</label>
        <textarea id="storage-daemon-addresses" name="addresses"
                  rows="3"
                  placeholder="host[ipv4;192.0.2.20;9103]&#10;host[ipv6;::1;9103]"></textarea>

        <label for="storage-daemon-source-address">SourceAddress</label>
        <input id="storage-daemon-source-address" name="source_address"
               placeholder="192.0.2.20">

        <label for="storage-daemon-source-addresses">Source addresses</label>
        <textarea id="storage-daemon-source-addresses" name="source_addresses"
                  rows="3"
                  placeholder="192.0.2.20&#10;198.51.100.20"></textarea>

        <label for="storage-daemon-port">Port</label>
        <input id="storage-daemon-port" name="port" type="number"
               min="1" max="65535" placeholder="9103">

        <label for="storage-daemon-maximum-concurrent-jobs">Maximum concurrent jobs</label>
        <input id="storage-daemon-maximum-concurrent-jobs"
               name="maximum_concurrent_jobs" type="number" min="0"
               placeholder="1000">

        <label for="storage-daemon-absolute-job-timeout">Absolute job timeout</label>
        <input id="storage-daemon-absolute-job-timeout"
               name="absolute_job_timeout" type="number" min="0"
               placeholder="0">

        <label for="storage-daemon-statistics-collect-interval">Statistics collect interval</label>
        <input id="storage-daemon-statistics-collect-interval"
               name="statistics_collect_interval" type="number" min="0"
               placeholder="0">

        <label for="storage-daemon-allow-bandwidth-bursting">Allow bandwidth bursting</label>
        <select id="storage-daemon-allow-bandwidth-bursting"
                name="allow_bandwidth_bursting">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="storage-daemon-collect-device-statistics">Collect device statistics</label>
        <select id="storage-daemon-collect-device-statistics"
                name="collect_device_statistics">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="storage-daemon-collect-job-statistics">Collect job statistics</label>
        <select id="storage-daemon-collect-job-statistics"
                name="collect_job_statistics">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="storage-daemon-device-reserve-by-media-type">Device reserve by media type</label>
        <select id="storage-daemon-device-reserve-by-media-type"
                name="device_reserve_by_media_type">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="storage-daemon-autoxflate-on-replication">AutoXFlate on replication</label>
        <select id="storage-daemon-autoxflate-on-replication"
                name="autoxflate_on_replication">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="storage-daemon-file-device-concurrent-read">File device concurrent read</label>
        <select id="storage-daemon-file-device-concurrent-read"
                name="file_device_concurrent_read">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="storage-daemon-ver-id">VerId</label>
        <input id="storage-daemon-ver-id" name="ver_id"
               placeholder="bareos-sd-custom">

        <label for="storage-daemon-log-timestamp-format">Log timestamp format</label>
        <input id="storage-daemon-log-timestamp-format"
               name="log_timestamp_format" placeholder="%d-%b %H:%M">

        <label for="storage-daemon-maximum-bandwidth-per-job">Maximum bandwidth per job</label>
        <input id="storage-daemon-maximum-bandwidth-per-job"
               name="maximum_bandwidth_per_job" type="number" min="0"
               placeholder="0">

        <label for="storage-daemon-secure-erase-command">Secure erase command</label>
        <input id="storage-daemon-secure-erase-command"
               name="secure_erase_command"
               placeholder="/usr/bin/shred -n 3 -u">

        <label for="storage-daemon-enable-ktls">Enable kTLS</label>
        <select id="storage-daemon-enable-ktls" name="enable_ktls">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="storage-daemon-tls-authenticate">TLS authenticate only</label>
        <select id="storage-daemon-tls-authenticate" name="tls_authenticate">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="storage-daemon-tls-enable">TLS enabled</label>
        <select id="storage-daemon-tls-enable" name="tls_enable">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="storage-daemon-tls-require">TLS required</label>
        <select id="storage-daemon-tls-require" name="tls_require">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="storage-daemon-tls-verify-peer">TLS verify peer</label>
        <select id="storage-daemon-tls-verify-peer" name="tls_verify_peer">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="storage-daemon-tls-cipher-list">TLS cipher list</label>
        <input id="storage-daemon-tls-cipher-list" name="tls_cipher_list"
               placeholder="ECDHE-RSA-AES256-GCM-SHA384">

        <label for="storage-daemon-tls-cipher-suites">TLS cipher suites</label>
        <input id="storage-daemon-tls-cipher-suites"
               name="tls_cipher_suites"
               placeholder="TLS_AES_256_GCM_SHA384">

        <label for="storage-daemon-tls-dh-file">TLS DH file</label>
        <input id="storage-daemon-tls-dh-file" name="tls_dh_file"
               placeholder="/etc/bareos/dh4096.pem">

        <label for="storage-daemon-tls-protocol">TLS protocol</label>
        <input id="storage-daemon-tls-protocol" name="tls_protocol"
               placeholder="MinProtocol = TLSv1.2">

        <label for="storage-daemon-tls-ca-certificate-file">TLS CA certificate file</label>
        <input id="storage-daemon-tls-ca-certificate-file"
               name="tls_ca_certificate_file"
               placeholder="/etc/bareos/ca.crt">

        <label for="storage-daemon-tls-ca-certificate-dir">TLS CA certificate dir</label>
        <input id="storage-daemon-tls-ca-certificate-dir"
               name="tls_ca_certificate_dir"
               placeholder="/etc/ssl/certs">

        <label for="storage-daemon-tls-certificate-revocation-list">TLS certificate revocation list</label>
        <input id="storage-daemon-tls-certificate-revocation-list"
               name="tls_certificate_revocation_list"
               placeholder="/etc/bareos/crl.pem">

        <label for="storage-daemon-tls-certificate">TLS certificate</label>
        <input id="storage-daemon-tls-certificate"
               name="tls_certificate"
               placeholder="/etc/bareos/storage.crt">

        <label for="storage-daemon-tls-key">TLS key</label>
        <input id="storage-daemon-tls-key" name="tls_key"
               placeholder="/etc/bareos/storage.key">

        <label for="storage-daemon-tls-allowed-cn">TLS allowed CNs</label>
        <textarea id="storage-daemon-tls-allowed-cn" name="tls_allowed_cn"
                  rows="3"
                  placeholder="storage-cn-1&#10;storage-cn-2"></textarea>

        <label for="storage-daemon-just-in-time-reservation">Just in time reservation</label>
        <select id="storage-daemon-just-in-time-reservation"
                name="just_in_time_reservation">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="storage-daemon-ndmp-enable">NDMP enabled</label>
        <select id="storage-daemon-ndmp-enable" name="ndmp_enable">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="storage-daemon-ndmp-snooping">NDMP snooping</label>
        <select id="storage-daemon-ndmp-snooping" name="ndmp_snooping">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="storage-daemon-ndmp-log-level">NDMP log level</label>
        <input id="storage-daemon-ndmp-log-level" name="ndmp_log_level"
               type="number" min="0" placeholder="4">

        <label for="storage-daemon-ndmp-address">NDMP address</label>
        <input id="storage-daemon-ndmp-address" name="ndmp_address"
               placeholder="192.0.2.30">

        <label for="storage-daemon-ndmp-port">NDMP port</label>
        <input id="storage-daemon-ndmp-port" name="ndmp_port" type="number"
               min="1" max="65535" placeholder="10000">

        <label for="storage-daemon-ndmp-addresses">NDMP addresses</label>
        <textarea id="storage-daemon-ndmp-addresses" name="ndmp_addresses"
                  rows="3"
                  placeholder="host[ipv4;192.0.2.30;10001]&#10;host[ipv6;::1;10001]"></textarea>

        <label for="storage-daemon-sd-connect-timeout">Storage connect timeout</label>
        <input id="storage-daemon-sd-connect-timeout"
               name="sd_connect_timeout" type="number" min="0"
               placeholder="1800">

        <label for="storage-daemon-fd-connect-timeout">Client connect timeout</label>
        <input id="storage-daemon-fd-connect-timeout"
               name="fd_connect_timeout" type="number" min="0"
               placeholder="1800">

        <label for="storage-daemon-heartbeat-interval">Heartbeat interval</label>
        <input id="storage-daemon-heartbeat-interval" name="heartbeat_interval"
               type="number" min="0" placeholder="0">

        <label for="storage-daemon-checkpoint-interval">Checkpoint interval</label>
        <input id="storage-daemon-checkpoint-interval"
               name="checkpoint_interval" type="number" min="0"
               placeholder="0">

        <label for="storage-daemon-client-connect-wait">Client connect wait</label>
        <input id="storage-daemon-client-connect-wait"
               name="client_connect_wait" type="number" min="0"
               placeholder="0">

        <label for="storage-daemon-maximum-network-buffer-size">Maximum network buffer size</label>
        <input id="storage-daemon-maximum-network-buffer-size"
               name="maximum_network_buffer_size" type="number" min="0"
               placeholder="0">

        <label for="storage-daemon-description">Description</label>
        <input id="storage-daemon-description" name="description"
               placeholder="Managed storage-daemon storage resource">

        <label for="storage-daemon-working-directory">Working directory</label>
        <input id="storage-daemon-working-directory" name="working_directory"
               placeholder="/var/lib/bareos/storage">

        <label for="storage-daemon-plugin-directory">Plugin directory</label>
        <input id="storage-daemon-plugin-directory" name="plugin_directory"
               placeholder="/usr/lib/bareos/plugins">

        <label for="storage-daemon-plugin-names">Plugin names</label>
        <textarea id="storage-daemon-plugin-names" name="plugin_names"
                  rows="3"
                  placeholder="autochanger&#10;python"></textarea>

        <label for="storage-daemon-backend-directories">Backend directories</label>
        <textarea id="storage-daemon-backend-directories"
                  name="backend_directories" rows="3"
                  placeholder="/usr/lib/bareos/backends&#10;/opt/bareos/backends"></textarea>
        <label for="storage-daemon-scripts-directory">Scripts directory</label>
        <input id="storage-daemon-scripts-directory" name="scripts_directory"
               placeholder="/usr/lib/bareos/scripts">

        <label for="storage-daemon-messages">Messages</label>
        <input id="storage-daemon-messages" name="messages" placeholder="Standard">

        <button type="submit">
          PUT /v1/deployments/{id}/storages/{storage}
        </button>
      </form>
    </section>

    <section class="card">
      <h2>Upsert director daemon resource</h2>
      <form id="director-daemon-form">
        <label for="director-daemon-deployment-id">Deployment ID</label>
        <input id="director-daemon-deployment-id" name="deployment_id" value="prod">

        <label for="director-daemon-name">Director name</label>
        <input id="director-daemon-name" name="director_name" value="bareos-dir">

        <label for="director-daemon-address">Address</label>
        <input id="director-daemon-address" name="address"
               placeholder="director.example.com">

        <label for="director-daemon-source-address">SourceAddress</label>
        <input id="director-daemon-source-address" name="source_address"
               placeholder="192.0.2.44">

        <label for="director-daemon-source-addresses">Source addresses</label>
        <textarea id="director-daemon-source-addresses" name="source_addresses"
                  rows="3"
                  placeholder="192.0.2.44&#10;198.51.100.44"></textarea>

        <label for="director-daemon-port">Port</label>
        <input id="director-daemon-port" name="port" type="number"
               min="1" max="65535" placeholder="9101">

        <label for="director-daemon-description">Description</label>
        <input id="director-daemon-description" name="description"
               placeholder="Managed director daemon resource">

        <label for="director-daemon-working-directory">Working directory</label>
        <input id="director-daemon-working-directory" name="working_directory"
               placeholder="/var/lib/bareos">

        <label for="director-daemon-plugin-directory">Plugin directory</label>
        <input id="director-daemon-plugin-directory" name="plugin_directory"
               placeholder="/usr/lib/bareos/plugins">

        <label for="director-daemon-scripts-directory">Scripts directory</label>
        <input id="director-daemon-scripts-directory" name="scripts_directory"
               placeholder="/usr/lib/bareos/scripts">

        <label for="director-daemon-messages">Messages resource</label>
        <input id="director-daemon-messages" name="messages"
               placeholder="Standard">

        <button type="submit">
          PUT /v1/deployments/{id}/directors/{director}
        </button>
      </form>
    </section>

    <section class="card">
      <h2>Upsert storage-daemon director resource</h2>
      <form id="storage-director-form">
        <label for="storage-director-deployment-id">Deployment ID</label>
        <input id="storage-director-deployment-id" name="deployment_id" value="prod">

        <label for="storage-director-storage-name">Storage name</label>
        <input id="storage-director-storage-name" name="storage_name" value="bareos-sd">

        <label for="storage-director-director-name">Director name</label>
        <input id="storage-director-director-name" name="director_name" value="bareos-dir">

        <label for="storage-director-password">Password</label>
        <input id="storage-director-password" name="password"
               placeholder="[md5]supersecret">

        <label for="storage-director-description">Description</label>
        <input id="storage-director-description" name="description"
               placeholder="Managed storage-daemon director resource">

        <label class="checkbox-label" for="storage-director-monitor">
          <input id="storage-director-monitor" name="monitor" type="checkbox">
          Monitor
        </label>

        <label for="storage-director-maximum-bandwidth-per-job">Maximum bandwidth per job</label>
        <input id="storage-director-maximum-bandwidth-per-job"
               name="maximum_bandwidth_per_job" type="number" min="0"
               placeholder="0">

        <button type="submit">
          PUT /v1/deployments/{id}/storages/{storage}/directors/{director}
        </button>
        <button type="button" id="storage-director-delete-button">
          DELETE /v1/deployments/{id}/storages/{storage}/directors/{director}
        </button>
      </form>
    </section>

    <section class="card">
      <h2>Upsert storage-daemon device resource</h2>
      <form id="storage-device-form">
        <label for="storage-device-deployment-id">Deployment ID</label>
        <input id="storage-device-deployment-id" name="deployment_id" value="prod">

        <label for="storage-device-storage-name">Storage name</label>
        <input id="storage-device-storage-name" name="storage_name" value="bareos-sd">

        <label for="storage-device-device-name">Device name</label>
        <input id="storage-device-device-name" name="device_name" value="ManagedDevice">

        <label for="storage-device-media-type">Media type</label>
        <input id="storage-device-media-type" name="media_type" value="File">

        <label for="storage-device-archive-device">Archive device</label>
        <input id="storage-device-archive-device" name="archive_device"
               value="/tmp/bareos-storage">

        <label for="storage-device-device-type">Device type</label>
        <input id="storage-device-device-type" name="device_type" value="file">

        <label for="storage-device-description">Description</label>
        <input id="storage-device-description" name="description"
               placeholder="Managed storage-daemon device resource">

        <button type="submit">
          PUT /v1/deployments/{id}/storages/{storage}/devices/{device}
        </button>
        <button type="button" id="storage-device-delete-button">
          DELETE /v1/deployments/{id}/storages/{storage}/devices/{device}
        </button>
      </form>
    </section>

    <section class="card">
      <h2>Upsert storage-daemon messages resource</h2>
      <form id="storage-messages-form">
        <label for="storage-messages-deployment-id">Deployment ID</label>
        <input id="storage-messages-deployment-id" name="deployment_id" value="prod">

        <label for="storage-messages-storage-name">Storage name</label>
        <input id="storage-messages-storage-name" name="storage_name" value="bareos-sd">

        <label for="storage-messages-messages-name">Messages name</label>
        <input id="storage-messages-messages-name" name="messages_name" value="ManagedMessages">

        <label for="storage-messages-description">Description</label>
        <input id="storage-messages-description" name="description"
               placeholder="Managed storage-daemon messages resource">

        <label for="storage-messages-entries">Message entries</label>
        <textarea id="storage-messages-entries" name="entries"
                  rows="4"
                  placeholder="Director = bareos-dir = all"></textarea>

        <button type="submit">
          PUT /v1/deployments/{id}/storages/{storage}/messages/{messages}
        </button>
        <button type="button" id="storage-messages-delete-button">
          DELETE /v1/deployments/{id}/storages/{storage}/messages/{messages}
        </button>
      </form>
    </section>

    <section class="card">
      <h2>Upsert storage-daemon NDMP resource</h2>
      <form id="storage-ndmp-form">
        <label for="storage-ndmp-deployment-id">Deployment ID</label>
        <input id="storage-ndmp-deployment-id" name="deployment_id" value="prod">

        <label for="storage-ndmp-storage-name">Storage name</label>
        <input id="storage-ndmp-storage-name" name="storage_name" value="bareos-sd">

        <label for="storage-ndmp-name">NDMP resource name</label>
        <input id="storage-ndmp-name" name="ndmp_name" value="ManagedNdmp">

        <label for="storage-ndmp-username">Username</label>
        <input id="storage-ndmp-username" name="username" value="ndmp-user">

        <label for="storage-ndmp-password">Password</label>
        <input id="storage-ndmp-password" name="password"
               placeholder="cleartext or [md5]hash">

        <label for="storage-ndmp-auth-type">Auth type</label>
        <input id="storage-ndmp-auth-type" name="auth_type" value="MD5">

        <label for="storage-ndmp-log-level">Log level</label>
        <input id="storage-ndmp-log-level" name="log_level" type="number" min="0"
               placeholder="4">

        <button type="submit">
          PUT /v1/deployments/{id}/storages/{storage}/ndmp/{ndmp}
        </button>
        <button type="button" id="storage-ndmp-delete-button">
          DELETE /v1/deployments/{id}/storages/{storage}/ndmp/{ndmp}
        </button>
      </form>
    </section>

    <section class="card">
      <h2>Upsert storage-daemon autochanger resource</h2>
      <form id="storage-autochanger-form">
        <label for="storage-autochanger-deployment-id">Deployment ID</label>
        <input id="storage-autochanger-deployment-id" name="deployment_id" value="prod">

        <label for="storage-autochanger-storage-name">Storage name</label>
        <input id="storage-autochanger-storage-name" name="storage_name" value="bareos-sd">

        <label for="storage-autochanger-name">Autochanger resource name</label>
        <input id="storage-autochanger-name" name="autochanger_name" value="ManagedAutochanger">

        <label for="storage-autochanger-devices">Device names</label>
        <textarea id="storage-autochanger-devices" name="devices"
                  rows="3"
                  placeholder="FileStorage"></textarea>

        <label for="storage-autochanger-changer-device">Changer device</label>
        <input id="storage-autochanger-changer-device" name="changer_device"
               value="/dev/null">

        <label for="storage-autochanger-changer-command">Changer command</label>
        <input id="storage-autochanger-changer-command" name="changer_command"
               value="/usr/lib/bareos/mtx-changer %c %o %S %a %d">

        <label for="storage-autochanger-description">Description</label>
        <input id="storage-autochanger-description" name="description"
               placeholder="Managed storage-daemon autochanger resource">

        <button type="submit">
          PUT /v1/deployments/{id}/storages/{storage}/autochangers/{autochanger}
        </button>
        <button type="button" id="storage-autochanger-delete-button">
          DELETE /v1/deployments/{id}/storages/{storage}/autochangers/{autochanger}
        </button>
      </form>
    </section>

    <section class="card">
      <h2>Upsert bconsole console resource</h2>
      <form id="console-console-form">
        <label for="console-console-deployment-id">Deployment ID</label>
        <input id="console-console-deployment-id" name="deployment_id" value="prod">

        <label for="console-console-config-name">Console config name</label>
        <input id="console-console-config-name" name="console_config_name" value="admin">

        <label for="console-console-name">Console resource name</label>
        <input id="console-console-name" name="console_name" value="managed-console">

        <label for="console-console-director">Director resource</label>
        <input id="console-console-director" name="director" value="bareos-dir">

        <label for="console-console-password">Password</label>
        <input id="console-console-password" name="password"
               placeholder="cleartext or [md5]hash">

         <label for="console-console-description">Description</label>
         <input id="console-console-description" name="description"
                placeholder="Managed bconsole resource">

         <label for="console-console-rc-file">RC file</label>
         <input id="console-console-rc-file" name="rc_file"
                placeholder="~/.bconsolerc">

         <label for="console-console-history-file">History file</label>
        <input id="console-console-history-file" name="history_file"
               placeholder="~/.bareos_history">

        <label for="console-console-history-length">History length</label>
        <input id="console-console-history-length" name="history_length"
               type="number" min="0" placeholder="100">

        <label for="console-console-heartbeat-interval">Heartbeat interval</label>
        <input id="console-console-heartbeat-interval" name="heartbeat_interval"
               type="number" min="0" placeholder="0">

        <label for="console-console-tls-authenticate">TLS authenticate only</label>
        <select id="console-console-tls-authenticate" name="tls_authenticate">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="console-console-tls-enable">TLS enabled</label>
        <select id="console-console-tls-enable" name="tls_enable">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="console-console-tls-require">TLS required</label>
        <select id="console-console-tls-require" name="tls_require">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

         <label for="console-console-tls-verify-peer">TLS verify peer</label>
         <select id="console-console-tls-verify-peer" name="tls_verify_peer">
           <option value="">Keep existing</option>
           <option value="true">Yes</option>
           <option value="false">No</option>
         </select>

         <label for="console-console-tls-cipher-list">TLS cipher list</label>
         <input id="console-console-tls-cipher-list" name="tls_cipher_list"
                placeholder="DEFAULT:@SECLEVEL=2">

         <label for="console-console-tls-cipher-suites">TLS cipher suites</label>
         <input id="console-console-tls-cipher-suites"
                name="tls_cipher_suites"
                placeholder="TLS_AES_256_GCM_SHA384">

         <label for="console-console-tls-dh-file">TLS DH file</label>
         <input id="console-console-tls-dh-file" name="tls_dh_file"
                placeholder="/etc/bareos/dh4096.pem">

         <label for="console-console-tls-protocol">TLS protocol</label>
         <input id="console-console-tls-protocol" name="tls_protocol"
                placeholder="+TLSv1.2:+TLSv1.3">

         <label for="console-console-tls-ca-certificate-file">TLS CA certificate file</label>
         <input id="console-console-tls-ca-certificate-file"
                name="tls_ca_certificate_file"
                placeholder="/etc/bareos/ca.pem">

         <label for="console-console-tls-ca-certificate-dir">TLS CA certificate dir</label>
         <input id="console-console-tls-ca-certificate-dir"
                name="tls_ca_certificate_dir"
                placeholder="/etc/ssl/certs">

         <label for="console-console-tls-certificate-revocation-list">TLS certificate revocation list</label>
         <input id="console-console-tls-certificate-revocation-list"
                name="tls_certificate_revocation_list"
                placeholder="/etc/bareos/crl.pem">

         <label for="console-console-tls-certificate">TLS certificate</label>
         <input id="console-console-tls-certificate" name="tls_certificate"
                placeholder="/etc/bareos/console-cert.pem">

         <label for="console-console-tls-key">TLS key</label>
         <input id="console-console-tls-key" name="tls_key"
                placeholder="/etc/bareos/console-key.pem">

         <label for="console-console-tls-allowed-cn">TLS allowed CNs</label>
         <textarea id="console-console-tls-allowed-cn" name="tls_allowed_cn"
                   placeholder="console.example.test&#10;director.example.test"></textarea>

        <button type="submit">
          PUT /v1/deployments/{id}/consoles/{console}/consoles/{resource}
        </button>
        <button type="button" id="console-console-delete-button">
          DELETE /v1/deployments/{id}/consoles/{console}/consoles/{resource}
        </button>
      </form>
    </section>

    <section class="card">
      <h2>Upsert bconsole director resource</h2>
      <form id="console-director-form">
        <label for="console-director-deployment-id">Deployment ID</label>
        <input id="console-director-deployment-id" name="deployment_id" value="prod">

        <label for="console-director-config-name">Console config name</label>
        <input id="console-director-config-name" name="console_config_name" value="admin">

        <label for="console-director-name">Director resource name</label>
        <input id="console-director-name" name="director_name" value="managed-dir">

        <label for="console-director-address">Address</label>
        <input id="console-director-address" name="address" value="localhost">

        <label for="console-director-port">Port</label>
        <input id="console-director-port" name="port" type="number" min="1" max="65535"
               placeholder="9101">

        <label for="console-director-password">Password</label>
        <input id="console-director-password" name="password"
               placeholder="cleartext or [md5]hash">

        <label for="console-director-description">Description</label>
        <input id="console-director-description" name="description"
               placeholder="Managed bconsole director resource">

        <label for="console-director-heartbeat-interval">Heartbeat interval</label>
        <input id="console-director-heartbeat-interval"
               name="heartbeat_interval" type="number" min="0"
               placeholder="0">

        <label for="console-director-tls-authenticate">TLS authenticate only</label>
        <select id="console-director-tls-authenticate" name="tls_authenticate">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="console-director-tls-enable">TLS enabled</label>
        <select id="console-director-tls-enable" name="tls_enable">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="console-director-tls-require">TLS required</label>
        <select id="console-director-tls-require" name="tls_require">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

         <label for="console-director-tls-verify-peer">TLS verify peer</label>
         <select id="console-director-tls-verify-peer" name="tls_verify_peer">
           <option value="">Keep existing</option>
           <option value="true">Yes</option>
           <option value="false">No</option>
         </select>

         <label for="console-director-tls-cipher-list">TLS cipher list</label>
         <input id="console-director-tls-cipher-list" name="tls_cipher_list"
                placeholder="DEFAULT:@SECLEVEL=2">

         <label for="console-director-tls-cipher-suites">TLS cipher suites</label>
         <input id="console-director-tls-cipher-suites"
                name="tls_cipher_suites"
                placeholder="TLS_AES_256_GCM_SHA384">

         <label for="console-director-tls-dh-file">TLS DH file</label>
         <input id="console-director-tls-dh-file" name="tls_dh_file"
                placeholder="/etc/bareos/dh4096.pem">

         <label for="console-director-tls-protocol">TLS protocol</label>
         <input id="console-director-tls-protocol" name="tls_protocol"
                placeholder="+TLSv1.2:+TLSv1.3">

         <label for="console-director-tls-ca-certificate-file">TLS CA certificate file</label>
         <input id="console-director-tls-ca-certificate-file"
                name="tls_ca_certificate_file"
                placeholder="/etc/bareos/ca.pem">

         <label for="console-director-tls-ca-certificate-dir">TLS CA certificate dir</label>
         <input id="console-director-tls-ca-certificate-dir"
                name="tls_ca_certificate_dir"
                placeholder="/etc/ssl/certs">

         <label for="console-director-tls-certificate-revocation-list">TLS certificate revocation list</label>
         <input id="console-director-tls-certificate-revocation-list"
                name="tls_certificate_revocation_list"
                placeholder="/etc/bareos/crl.pem">

         <label for="console-director-tls-certificate">TLS certificate</label>
         <input id="console-director-tls-certificate" name="tls_certificate"
                placeholder="/etc/bareos/director-cert.pem">

         <label for="console-director-tls-key">TLS key</label>
         <input id="console-director-tls-key" name="tls_key"
                placeholder="/etc/bareos/director-key.pem">

         <label for="console-director-tls-allowed-cn">TLS allowed CNs</label>
         <textarea id="console-director-tls-allowed-cn" name="tls_allowed_cn"
                   placeholder="director.example.test&#10;backup.example.test"></textarea>

        <button type="submit">
          PUT /v1/deployments/{id}/consoles/{console}/directors/{director}
        </button>
        <button type="button" id="console-director-delete-button">
          DELETE /v1/deployments/{id}/consoles/{console}/directors/{director}
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

    function buildServiceUrl(url) {
      if (!String(url).startsWith('/')) { return String(url); }

      const pathname = window.location.pathname.replace(/\/+$/, '');
      if (pathname === '' || pathname === '/ui') { return String(url); }
      if (pathname.endsWith('/ui')) {
        return pathname.slice(0, -3) + String(url);
      }
      return String(url);
    }

    async function request(method, url, body) {
      const options = { method, headers: {} };
      if (body !== undefined) {
        options.headers['Content-Type'] = 'application/json';
        options.body = JSON.stringify(body);
      }

      const candidates = [];
      const preferredUrl = buildServiceUrl(url);
      candidates.push(preferredUrl);
      if (preferredUrl !== String(url)) { candidates.push(String(url)); }

      let requestUrl = candidates[0];
      let response = null;
      let text = '';
      for (const candidate of candidates) {
        requestUrl = candidate;
        response = await fetch(candidate, options);
        text = await response.text();
        if (!(response.status === 404 && text.includes('"route not found."'))) {
          break;
        }
      }
      let payload = text;

      try {
        payload = JSON.stringify(JSON.parse(text), null, 2);
      } catch (_) {}

      responsePanel.textContent =
        `${method} ${requestUrl}\nHTTP ${response.status}\n\n${payload}`;

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
        const rawAllowedScriptDirs = String(form.get('allowed_script_dirs') ?? '');
        const allowedScriptDirs = rawAllowedScriptDirs.split('\n')
          .map((line) => line.trim())
          .filter((line) => line.length > 0);
        const rawAllowedJobCommands = String(form.get('allowed_job_commands') ?? '');
        const allowedJobCommands = rawAllowedJobCommands.split('\n')
          .map((line) => line.trim())
          .filter((line) => line.length > 0);
        const rawTlsAllowedCn = String(form.get('tls_allowed_cn') ?? '');
        const tlsAllowedCn = rawTlsAllowedCn.split('\n')
          .map((line) => line.trim())
          .filter((line) => line.length > 0);
        const payload = {
          description: String(form.get('description') ?? '').trim(),
          address: String(form.get('address') ?? '').trim(),
          port: String(form.get('port') ?? '').trim(),
          allowed_script_dirs: allowedScriptDirs,
          allowed_job_commands: allowedJobCommands,
          tls_authenticate: document.getElementById(
            'client-stub-tls-authenticate').checked,
          tls_enable: document.getElementById('client-stub-tls-enable').checked,
          tls_require: document.getElementById('client-stub-tls-require').checked,
          tls_verify_peer: document.getElementById(
            'client-stub-tls-verify-peer').checked,
          tls_cipher_list: String(form.get('tls_cipher_list') ?? '').trim(),
          tls_cipher_suites: String(form.get('tls_cipher_suites') ?? '').trim(),
          tls_dh_file: String(form.get('tls_dh_file') ?? '').trim(),
          tls_protocol: String(form.get('tls_protocol') ?? '').trim(),
          tls_ca_certificate_file: String(
            form.get('tls_ca_certificate_file') ?? '').trim(),
          tls_ca_certificate_dir: String(
            form.get('tls_ca_certificate_dir') ?? '').trim(),
          tls_certificate_revocation_list: String(
            form.get('tls_certificate_revocation_list') ?? '').trim(),
          tls_certificate: String(form.get('tls_certificate') ?? '').trim(),
          tls_key: String(form.get('tls_key') ?? '').trim(),
          tls_allowed_cn: tlsAllowedCn,
           connection_from_director_to_client: document.getElementById(
             'client-stub-connection-from-director-to-client').checked,
           connection_from_client_to_director: document.getElementById(
             'client-stub-connection-from-client-to-director').checked,
           monitor: document.getElementById('client-stub-monitor').checked,
           maximum_bandwidth_per_job: String(
             form.get('maximum_bandwidth_per_job') ?? '').trim(),
        };
        if (!payload.description) {
          delete payload.description;
        }
        if (!payload.address) {
          delete payload.address;
        }
        if (!payload.port) {
          delete payload.port;
        } else {
          payload.port = Number.parseInt(payload.port, 10);
        }
        if (payload.allowed_script_dirs.length === 0) {
          delete payload.allowed_script_dirs;
        }
        if (payload.allowed_job_commands.length === 0) {
          delete payload.allowed_job_commands;
        }
        if (!payload.tls_cipher_list) {
          delete payload.tls_cipher_list;
        }
        if (!payload.tls_cipher_suites) {
          delete payload.tls_cipher_suites;
        }
        if (!payload.tls_dh_file) {
          delete payload.tls_dh_file;
        }
        if (!payload.tls_protocol) {
          delete payload.tls_protocol;
        }
        if (!payload.tls_ca_certificate_file) {
          delete payload.tls_ca_certificate_file;
        }
        if (!payload.tls_ca_certificate_dir) {
          delete payload.tls_ca_certificate_dir;
        }
        if (!payload.tls_certificate_revocation_list) {
          delete payload.tls_certificate_revocation_list;
        }
        if (!payload.tls_certificate) {
          delete payload.tls_certificate;
        }
        if (!payload.tls_key) {
          delete payload.tls_key;
        }
        if (payload.tls_allowed_cn.length === 0) {
          delete payload.tls_allowed_cn;
        }
        if (!payload.maximum_bandwidth_per_job) {
          delete payload.maximum_bandwidth_per_job;
        } else {
          payload.maximum_bandwidth_per_job
            = Number.parseInt(payload.maximum_bandwidth_per_job, 10);
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
    document.getElementById('client-stub-delete-button').addEventListener(
      'click',
      async () => {
        const form = new FormData(document.getElementById('client-stub-form'));
        const deploymentId = String(form.get('deployment_id') ?? '').trim();
        const clientName = String(form.get('client_name') ?? '').trim();
        const directorName = String(form.get('director_name') ?? '').trim();
        const { response } = await request(
          'DELETE',
          `/v1/deployments/${encodeURIComponent(deploymentId)}/clients/${encodeURIComponent(clientName)}/directors/${encodeURIComponent(directorName)}`);
        if (response.ok) {
          document.getElementById('deployment-inspect-id').value = deploymentId;
          await loadDeploymentContents(deploymentId);
        }
      });
    document.getElementById('client-daemon-form').addEventListener(
      'submit',
      async (event) => {
        event.preventDefault();
        const form = new FormData(event.target);
        const deploymentId = String(form.get('deployment_id') ?? '').trim();
        const clientName = String(form.get('client_name') ?? '').trim();
        const rawAddresses = String(form.get('addresses') ?? '');
        const addresses = rawAddresses.split('\n')
          .map((line) => line.trim())
          .filter((line) => line.length > 0);
        const rawSourceAddresses = String(form.get('source_addresses') ?? '');
        const sourceAddresses = rawSourceAddresses.split('\n')
          .map((line) => line.trim())
          .filter((line) => line.length > 0);
        const rawTlsAllowedCn = String(form.get('tls_allowed_cn') ?? '');
        const tlsAllowedCn = rawTlsAllowedCn.split('\n')
          .map((line) => line.trim())
          .filter((line) => line.length > 0);
        const rawPkiSigners = String(form.get('pki_signers') ?? '');
        const pkiSigners = rawPkiSigners.split('\n')
          .map((line) => line.trim())
          .filter((line) => line.length > 0);
        const rawPkiMasterKeys = String(form.get('pki_master_keys') ?? '');
        const pkiMasterKeys = rawPkiMasterKeys.split('\n')
          .map((line) => line.trim())
          .filter((line) => line.length > 0);
        const rawPluginNames = String(form.get('plugin_names') ?? '');
        const pluginNames = rawPluginNames.split('\n')
          .map((line) => line.trim())
          .filter((line) => line.length > 0);
        const rawAllowedScriptDirs = String(form.get('allowed_script_dirs') ?? '');
        const allowedScriptDirs = rawAllowedScriptDirs.split('\n')
          .map((line) => line.trim())
          .filter((line) => line.length > 0);
        const rawAllowedJobCommands = String(form.get('allowed_job_commands') ?? '');
        const allowedJobCommands = rawAllowedJobCommands.split('\n')
          .map((line) => line.trim())
          .filter((line) => line.length > 0);
        const payload = {
          address: String(form.get('address') ?? '').trim(),
          addresses: addresses,
          source_address: String(form.get('source_address') ?? '').trim(),
          source_addresses: sourceAddresses,
          port: String(form.get('port') ?? '').trim(),
          maximum_concurrent_jobs: String(form.get('maximum_concurrent_jobs') ?? '').trim(),
          maximum_workers_per_job: String(form.get('maximum_workers_per_job') ?? '').trim(),
          absolute_job_timeout: String(form.get('absolute_job_timeout') ?? '').trim(),
          allow_bandwidth_bursting: String(form.get('allow_bandwidth_bursting') ?? '').trim(),
          tls_authenticate: String(form.get('tls_authenticate') ?? '').trim(),
          tls_enable: String(form.get('tls_enable') ?? '').trim(),
          tls_require: String(form.get('tls_require') ?? '').trim(),
          tls_verify_peer: String(form.get('tls_verify_peer') ?? '').trim(),
          tls_cipher_list: String(form.get('tls_cipher_list') ?? '').trim(),
          tls_cipher_suites: String(form.get('tls_cipher_suites') ?? '').trim(),
          tls_dh_file: String(form.get('tls_dh_file') ?? '').trim(),
          tls_protocol: String(form.get('tls_protocol') ?? '').trim(),
          tls_ca_certificate_file: String(form.get('tls_ca_certificate_file') ?? '').trim(),
          tls_ca_certificate_dir: String(form.get('tls_ca_certificate_dir') ?? '').trim(),
          tls_certificate_revocation_list: String(form.get('tls_certificate_revocation_list') ?? '').trim(),
          tls_certificate: String(form.get('tls_certificate') ?? '').trim(),
          tls_key: String(form.get('tls_key') ?? '').trim(),
          tls_allowed_cn: tlsAllowedCn,
          pki_signatures: String(form.get('pki_signatures') ?? '').trim(),
          pki_encryption: String(form.get('pki_encryption') ?? '').trim(),
          pki_key_pair: String(form.get('pki_key_pair') ?? '').trim(),
          pki_signers: pkiSigners,
          pki_master_keys: pkiMasterKeys,
          pki_cipher: String(form.get('pki_cipher') ?? '').trim(),
          always_use_lmdb: String(form.get('always_use_lmdb') ?? '').trim(),
          lmdb_threshold: String(form.get('lmdb_threshold') ?? '').trim(),
          ver_id: String(form.get('ver_id') ?? '').trim(),
          log_timestamp_format: String(form.get('log_timestamp_format') ?? '').trim(),
          maximum_bandwidth_per_job: String(form.get('maximum_bandwidth_per_job') ?? '').trim(),
          secure_erase_command: String(form.get('secure_erase_command') ?? '').trim(),
          grpc_module: String(form.get('grpc_module') ?? '').trim(),
          enable_ktls: String(form.get('enable_ktls') ?? '').trim(),
          sd_connect_timeout: String(form.get('sd_connect_timeout') ?? '').trim(),
          heartbeat_interval: String(form.get('heartbeat_interval') ?? '').trim(),
          maximum_network_buffer_size: String(form.get('maximum_network_buffer_size') ?? '').trim(),
          description: String(form.get('description') ?? '').trim(),
          working_directory: String(form.get('working_directory') ?? '').trim(),
          plugin_directory: String(form.get('plugin_directory') ?? '').trim(),
          plugin_names: pluginNames,
          allowed_script_dirs: allowedScriptDirs,
          allowed_job_commands: allowedJobCommands,
          scripts_directory: String(form.get('scripts_directory') ?? '').trim(),
          messages: String(form.get('messages') ?? '').trim(),
        };
        if (!payload.address) { delete payload.address; }
        if (payload.addresses.length === 0) { delete payload.addresses; }
        if (!payload.source_address) { delete payload.source_address; }
        if (payload.source_addresses.length === 0) {
          delete payload.source_addresses;
        }
        if (!payload.port) { delete payload.port; } else { payload.port = Number(payload.port); }
        if (!payload.maximum_concurrent_jobs) { delete payload.maximum_concurrent_jobs; } else { payload.maximum_concurrent_jobs = Number(payload.maximum_concurrent_jobs); }
        if (!payload.maximum_workers_per_job) { delete payload.maximum_workers_per_job; } else { payload.maximum_workers_per_job = Number(payload.maximum_workers_per_job); }
        if (!payload.absolute_job_timeout) { delete payload.absolute_job_timeout; } else { payload.absolute_job_timeout = Number(payload.absolute_job_timeout); }
        if (!payload.allow_bandwidth_bursting) { delete payload.allow_bandwidth_bursting; } else { payload.allow_bandwidth_bursting = payload.allow_bandwidth_bursting === 'true'; }
        if (!payload.tls_authenticate) { delete payload.tls_authenticate; } else { payload.tls_authenticate = payload.tls_authenticate === 'true'; }
        if (!payload.tls_enable) { delete payload.tls_enable; } else { payload.tls_enable = payload.tls_enable === 'true'; }
        if (!payload.tls_require) { delete payload.tls_require; } else { payload.tls_require = payload.tls_require === 'true'; }
        if (!payload.tls_verify_peer) { delete payload.tls_verify_peer; } else { payload.tls_verify_peer = payload.tls_verify_peer === 'true'; }
        if (!payload.tls_cipher_list) { delete payload.tls_cipher_list; }
        if (!payload.tls_cipher_suites) { delete payload.tls_cipher_suites; }
        if (!payload.tls_dh_file) { delete payload.tls_dh_file; }
        if (!payload.tls_protocol) { delete payload.tls_protocol; }
        if (!payload.tls_ca_certificate_file) { delete payload.tls_ca_certificate_file; }
        if (!payload.tls_ca_certificate_dir) { delete payload.tls_ca_certificate_dir; }
        if (!payload.tls_certificate_revocation_list) { delete payload.tls_certificate_revocation_list; }
        if (!payload.tls_certificate) { delete payload.tls_certificate; }
        if (!payload.tls_key) { delete payload.tls_key; }
        if (payload.tls_allowed_cn.length === 0) { delete payload.tls_allowed_cn; }
        if (!payload.pki_signatures) { delete payload.pki_signatures; } else { payload.pki_signatures = payload.pki_signatures === 'true'; }
        if (!payload.pki_encryption) { delete payload.pki_encryption; } else { payload.pki_encryption = payload.pki_encryption === 'true'; }
        if (!payload.pki_key_pair) { delete payload.pki_key_pair; }
        if (payload.pki_signers.length === 0) { delete payload.pki_signers; }
        if (payload.pki_master_keys.length === 0) { delete payload.pki_master_keys; }
        if (!payload.pki_cipher) { delete payload.pki_cipher; }
        if (!payload.always_use_lmdb) { delete payload.always_use_lmdb; } else { payload.always_use_lmdb = payload.always_use_lmdb === 'true'; }
        if (!payload.lmdb_threshold) { delete payload.lmdb_threshold; } else { payload.lmdb_threshold = Number(payload.lmdb_threshold); }
        if (!payload.ver_id) { delete payload.ver_id; }
        if (!payload.log_timestamp_format) { delete payload.log_timestamp_format; }
        if (!payload.maximum_bandwidth_per_job) { delete payload.maximum_bandwidth_per_job; } else { payload.maximum_bandwidth_per_job = Number(payload.maximum_bandwidth_per_job); }
        if (!payload.secure_erase_command) { delete payload.secure_erase_command; }
        if (!payload.grpc_module) { delete payload.grpc_module; }
        if (!payload.enable_ktls) { delete payload.enable_ktls; } else { payload.enable_ktls = payload.enable_ktls === 'true'; }
        if (!payload.sd_connect_timeout) { delete payload.sd_connect_timeout; } else { payload.sd_connect_timeout = Number(payload.sd_connect_timeout); }
        if (!payload.heartbeat_interval) { delete payload.heartbeat_interval; } else { payload.heartbeat_interval = Number(payload.heartbeat_interval); }
        if (!payload.maximum_network_buffer_size) { delete payload.maximum_network_buffer_size; } else { payload.maximum_network_buffer_size = Number(payload.maximum_network_buffer_size); }
        if (!payload.description) { delete payload.description; }
        if (!payload.working_directory) { delete payload.working_directory; }
        if (!payload.plugin_directory) { delete payload.plugin_directory; }
        if (payload.plugin_names.length === 0) { delete payload.plugin_names; }
        if (payload.allowed_script_dirs.length === 0) {
          delete payload.allowed_script_dirs;
        }
        if (payload.allowed_job_commands.length === 0) {
          delete payload.allowed_job_commands;
        }
        if (!payload.scripts_directory) { delete payload.scripts_directory; }
        if (!payload.messages) { delete payload.messages; }
        const { response } = await request(
          'PUT',
          `/v1/deployments/${encodeURIComponent(deploymentId)}/clients/${encodeURIComponent(clientName)}`,
          payload);
        if (response.ok) {
          document.getElementById('deployment-inspect-id').value = deploymentId;
          await loadDeploymentContents(deploymentId);
        }
      });
    document.getElementById('client-messages-form').addEventListener(
      'submit',
      async (event) => {
        event.preventDefault();
        const form = new FormData(event.target);
        const deploymentId = String(form.get('deployment_id') ?? '').trim();
        const clientName = String(form.get('client_name') ?? '').trim();
        const messagesName = String(form.get('messages_name') ?? '').trim();
        const rawEntries = String(form.get('entries') ?? '');
        const entries = rawEntries.split('\n')
          .map((line) => line.trim())
          .filter((line) => line.length > 0);
        const payload = {
          description: String(form.get('description') ?? '').trim(),
          entries,
        };
        if (!payload.description) { delete payload.description; }
        if (payload.entries.length === 0) { delete payload.entries; }
        const { response } = await request(
          'PUT',
          `/v1/deployments/${encodeURIComponent(deploymentId)}/clients/${encodeURIComponent(clientName)}/messages/${encodeURIComponent(messagesName)}`,
          payload);
        if (response.ok) {
          document.getElementById('deployment-inspect-id').value = deploymentId;
          await loadDeploymentContents(deploymentId);
        }
      });
    document.getElementById('client-messages-delete-button').addEventListener(
      'click',
      async () => {
        const form = new FormData(document.getElementById('client-messages-form'));
        const deploymentId = String(form.get('deployment_id') ?? '').trim();
        const clientName = String(form.get('client_name') ?? '').trim();
        const messagesName = String(form.get('messages_name') ?? '').trim();
        const { response } = await request(
          'DELETE',
          `/v1/deployments/${encodeURIComponent(deploymentId)}/clients/${encodeURIComponent(clientName)}/messages/${encodeURIComponent(messagesName)}`);
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
          enabled: document.getElementById('director-client-enabled').checked,
          passive: document.getElementById('director-client-passive').checked,
          connection_from_director_to_client: document.getElementById(
            'director-client-connection-from-director-to-client').checked,
          connection_from_client_to_director: document.getElementById(
            'director-client-connection-from-client-to-director').checked,
          maximum_bandwidth_per_job: String(
            form.get('maximum_bandwidth_per_job') ?? '').trim(),
          heartbeat_interval: String(form.get('heartbeat_interval') ?? '').trim(),
          description: String(form.get('description') ?? '').trim(),
        };
        if (!payload.address) {
          delete payload.address;
        }
        if (!payload.password) {
          delete payload.password;
        }
        if (!payload.maximum_bandwidth_per_job) {
          delete payload.maximum_bandwidth_per_job;
        } else {
          payload.maximum_bandwidth_per_job
            = Number.parseInt(payload.maximum_bandwidth_per_job, 10);
        }
        if (!payload.heartbeat_interval) {
          delete payload.heartbeat_interval;
        } else {
          payload.heartbeat_interval = Number.parseInt(payload.heartbeat_interval, 10);
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
          enabled: document.getElementById('director-storage-enabled').checked,
          allow_compression:
            document.getElementById('director-storage-allow-compression').checked,
          maximum_bandwidth_per_job: String(
            form.get('maximum_bandwidth_per_job') ?? '').trim(),
          heartbeat_interval: String(form.get('heartbeat_interval') ?? '').trim(),
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
        if (!payload.maximum_bandwidth_per_job) {
          delete payload.maximum_bandwidth_per_job;
        } else {
          payload.maximum_bandwidth_per_job
            = Number.parseInt(payload.maximum_bandwidth_per_job, 10);
        }
        if (!payload.heartbeat_interval) {
          delete payload.heartbeat_interval;
        } else {
          payload.heartbeat_interval = Number.parseInt(payload.heartbeat_interval, 10);
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
    document.getElementById('director-messages-form').addEventListener(
      'submit',
      async (event) => {
        event.preventDefault();
        const form = new FormData(event.target);
        const deploymentId = String(form.get('deployment_id') ?? '').trim();
        const directorName = String(form.get('director_name') ?? '').trim();
        const messagesName = String(form.get('messages_name') ?? '').trim();
        const rawEntries = String(form.get('entries') ?? '');
        const entries = rawEntries.split('\n')
          .map((line) => line.trim())
          .filter((line) => line.length > 0);
        const payload = {
          description: String(form.get('description') ?? '').trim(),
          entries,
        };
        if (!payload.description) {
          delete payload.description;
        }
        if (payload.entries.length === 0) {
          delete payload.entries;
        }
        const { response } = await request(
          'PUT',
          `/v1/deployments/${encodeURIComponent(deploymentId)}/directors/${encodeURIComponent(directorName)}/messages/${encodeURIComponent(messagesName)}`,
          payload);
        if (response.ok) {
          document.getElementById('deployment-inspect-id').value = deploymentId;
          await loadDeploymentContents(deploymentId);
        }
      });
    document.getElementById('director-messages-delete-button').addEventListener(
      'click',
      async () => {
        const form = new FormData(document.getElementById('director-messages-form'));
        const deploymentId = String(form.get('deployment_id') ?? '').trim();
        const directorName = String(form.get('director_name') ?? '').trim();
        const messagesName = String(form.get('messages_name') ?? '').trim();
        const { response } = await request(
          'DELETE',
          `/v1/deployments/${encodeURIComponent(deploymentId)}/directors/${encodeURIComponent(directorName)}/messages/${encodeURIComponent(messagesName)}`);
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
    document.getElementById('director-counter-form').addEventListener(
      'submit',
      async (event) => {
        event.preventDefault();
        const form = new FormData(event.target);
        const deploymentId = String(form.get('deployment_id') ?? '').trim();
        const directorName = String(form.get('director_name') ?? '').trim();
        const counterName = String(form.get('counter_name') ?? '').trim();
        const payload = {
          minimum: String(form.get('minimum') ?? '').trim(),
          maximum: String(form.get('maximum') ?? '').trim(),
          wrap_counter: String(form.get('wrap_counter') ?? '').trim(),
          catalog: String(form.get('catalog') ?? '').trim(),
          description: String(form.get('description') ?? '').trim(),
        };
        if (payload.minimum) {
          payload.minimum = Number.parseInt(payload.minimum, 10);
        } else {
          delete payload.minimum;
        }
        if (payload.maximum) {
          payload.maximum = Number.parseInt(payload.maximum, 10);
        } else {
          delete payload.maximum;
        }
        if (!payload.wrap_counter) {
          delete payload.wrap_counter;
        }
        if (!payload.catalog) {
          delete payload.catalog;
        }
        if (!payload.description) {
          delete payload.description;
        }
        const { response } = await request(
          'PUT',
          `/v1/deployments/${encodeURIComponent(deploymentId)}/directors/${encodeURIComponent(directorName)}/counters/${encodeURIComponent(counterName)}`,
          payload);
        if (response.ok) {
          document.getElementById('deployment-inspect-id').value = deploymentId;
          await loadDeploymentContents(deploymentId);
        }
      });
    document.getElementById('director-counter-delete-button').addEventListener(
      'click',
      async () => {
        const form = new FormData(document.getElementById('director-counter-form'));
        const deploymentId = String(form.get('deployment_id') ?? '').trim();
        const directorName = String(form.get('director_name') ?? '').trim();
        const counterName = String(form.get('counter_name') ?? '').trim();
        const { response } = await request(
          'DELETE',
          `/v1/deployments/${encodeURIComponent(deploymentId)}/directors/${encodeURIComponent(directorName)}/counters/${encodeURIComponent(counterName)}`);
        if (response.ok) {
          document.getElementById('deployment-inspect-id').value = deploymentId;
          await loadDeploymentContents(deploymentId);
        }
      });
    document.getElementById('director-fileset-form').addEventListener(
      'submit',
      async (event) => {
        event.preventDefault();
        const form = new FormData(event.target);
        const deploymentId = String(form.get('deployment_id') ?? '').trim();
        const directorName = String(form.get('director_name') ?? '').trim();
        const filesetName = String(form.get('fileset_name') ?? '').trim();
        const includeText = String(form.get('include_blocks') ?? '').trim();
        const excludeText = String(form.get('exclude_blocks') ?? '').trim();
        const payload = {
          description: String(form.get('description') ?? '').trim(),
          ignore_fileset_changes:
            document.getElementById('director-fileset-ignore-fileset-changes').checked,
          enable_vss: document.getElementById('director-fileset-enable-vss').checked,
          include_blocks: includeText ? [includeText] : [],
          exclude_blocks: excludeText ? [excludeText] : [],
        };
        if (!payload.description) {
          delete payload.description;
        }
        if (payload.include_blocks.length === 0) {
          delete payload.include_blocks;
        }
        if (payload.exclude_blocks.length === 0) {
          delete payload.exclude_blocks;
        }
        const { response } = await request(
          'PUT',
          `/v1/deployments/${encodeURIComponent(deploymentId)}/directors/${encodeURIComponent(directorName)}/filesets/${encodeURIComponent(filesetName)}`,
          payload);
        if (response.ok) {
          document.getElementById('deployment-inspect-id').value = deploymentId;
          await loadDeploymentContents(deploymentId);
        }
      });
    document.getElementById('director-fileset-delete-button').addEventListener(
      'click',
      async () => {
        const form = new FormData(document.getElementById('director-fileset-form'));
        const deploymentId = String(form.get('deployment_id') ?? '').trim();
        const directorName = String(form.get('director_name') ?? '').trim();
        const filesetName = String(form.get('fileset_name') ?? '').trim();
        const { response } = await request(
          'DELETE',
          `/v1/deployments/${encodeURIComponent(deploymentId)}/directors/${encodeURIComponent(directorName)}/filesets/${encodeURIComponent(filesetName)}`);
        if (response.ok) {
          document.getElementById('deployment-inspect-id').value = deploymentId;
          await loadDeploymentContents(deploymentId);
        }
      });
    document.getElementById('director-job-form').addEventListener(
      'submit',
      async (event) => {
        event.preventDefault();
        const form = new FormData(event.target);
        const deploymentId = String(form.get('deployment_id') ?? '').trim();
        const directorName = String(form.get('director_name') ?? '').trim();
        const jobName = String(form.get('job_name') ?? '').trim();
        const rawStorages = String(form.get('storages') ?? '');
        const storages = rawStorages.split('\n')
          .map((line) => line.trim())
          .filter((line) => line.length > 0);
        const payload = {
          description: String(form.get('description') ?? '').trim(),
          type: String(form.get('type') ?? '').trim(),
          level: String(form.get('level') ?? '').trim(),
          messages: String(form.get('messages') ?? '').trim(),
          storages,
          pool: String(form.get('pool') ?? '').trim(),
          full_backup_pool: String(form.get('full_backup_pool') ?? '').trim(),
          virtual_full_backup_pool:
            String(form.get('virtual_full_backup_pool') ?? '').trim(),
          incremental_backup_pool:
            String(form.get('incremental_backup_pool') ?? '').trim(),
          differential_backup_pool:
            String(form.get('differential_backup_pool') ?? '').trim(),
          next_pool: String(form.get('next_pool') ?? '').trim(),
          client: String(form.get('client') ?? '').trim(),
          fileset: String(form.get('fileset') ?? '').trim(),
          schedule: String(form.get('schedule') ?? '').trim(),
          verify_job: String(form.get('verify_job') ?? '').trim(),
          catalog: String(form.get('catalog') ?? '').trim(),
          jobdefs: String(form.get('jobdefs') ?? '').trim(),
          where: String(form.get('where') ?? '').trim(),
          priority: String(form.get('priority') ?? '').trim(),
          enabled: document.getElementById('director-job-enabled').checked,
        };
        if (!payload.description) { delete payload.description; }
        if (!payload.type) { delete payload.type; }
        if (!payload.level) { delete payload.level; }
        if (!payload.messages) { delete payload.messages; }
        if (payload.storages.length === 0) { delete payload.storages; }
        if (!payload.pool) { delete payload.pool; }
        if (!payload.full_backup_pool) { delete payload.full_backup_pool; }
        if (!payload.virtual_full_backup_pool) {
          delete payload.virtual_full_backup_pool;
        }
        if (!payload.incremental_backup_pool) {
          delete payload.incremental_backup_pool;
        }
        if (!payload.differential_backup_pool) {
          delete payload.differential_backup_pool;
        }
        if (!payload.next_pool) { delete payload.next_pool; }
        if (!payload.client) { delete payload.client; }
        if (!payload.fileset) { delete payload.fileset; }
        if (!payload.schedule) { delete payload.schedule; }
        if (!payload.verify_job) { delete payload.verify_job; }
        if (!payload.catalog) { delete payload.catalog; }
        if (!payload.jobdefs) { delete payload.jobdefs; }
        if (!payload.where) { delete payload.where; }
        if (!payload.priority) {
          delete payload.priority;
        } else {
          payload.priority = Number.parseInt(payload.priority, 10);
        }
        const { response } = await request(
          'PUT',
          `/v1/deployments/${encodeURIComponent(deploymentId)}/directors/${encodeURIComponent(directorName)}/jobs/${encodeURIComponent(jobName)}`,
          payload);
        if (response.ok) {
          document.getElementById('deployment-inspect-id').value = deploymentId;
          await loadDeploymentContents(deploymentId);
        }
      });
    document.getElementById('director-job-delete-button').addEventListener(
      'click',
      async () => {
        const form = new FormData(document.getElementById('director-job-form'));
        const deploymentId = String(form.get('deployment_id') ?? '').trim();
        const directorName = String(form.get('director_name') ?? '').trim();
        const jobName = String(form.get('job_name') ?? '').trim();
        const { response } = await request(
          'DELETE',
          `/v1/deployments/${encodeURIComponent(deploymentId)}/directors/${encodeURIComponent(directorName)}/jobs/${encodeURIComponent(jobName)}`);
        if (response.ok) {
          document.getElementById('deployment-inspect-id').value = deploymentId;
          await loadDeploymentContents(deploymentId);
        }
      });
    document.getElementById('director-jobdefs-form').addEventListener(
      'submit',
      async (event) => {
        event.preventDefault();
        const form = new FormData(event.target);
        const deploymentId = String(form.get('deployment_id') ?? '').trim();
        const directorName = String(form.get('director_name') ?? '').trim();
        const jobdefsName = String(form.get('jobdefs_name') ?? '').trim();
        const rawStorages = String(form.get('storages') ?? '');
        const storages = rawStorages.split('\n')
          .map((line) => line.trim())
          .filter((line) => line.length > 0);
        const payload = {
          description: String(form.get('description') ?? '').trim(),
          type: String(form.get('type') ?? '').trim(),
          level: String(form.get('level') ?? '').trim(),
          messages: String(form.get('messages') ?? '').trim(),
          storages,
          pool: String(form.get('pool') ?? '').trim(),
          full_backup_pool: String(form.get('full_backup_pool') ?? '').trim(),
          incremental_backup_pool:
            String(form.get('incremental_backup_pool') ?? '').trim(),
          differential_backup_pool:
            String(form.get('differential_backup_pool') ?? '').trim(),
          client: String(form.get('client') ?? '').trim(),
          fileset: String(form.get('fileset') ?? '').trim(),
          schedule: String(form.get('schedule') ?? '').trim(),
          catalog: String(form.get('catalog') ?? '').trim(),
          priority: String(form.get('priority') ?? '').trim(),
          enabled: document.getElementById('director-jobdefs-enabled').checked,
        };
        if (!payload.description) { delete payload.description; }
        if (!payload.type) { delete payload.type; }
        if (!payload.level) { delete payload.level; }
        if (!payload.messages) { delete payload.messages; }
        if (payload.storages.length === 0) { delete payload.storages; }
        if (!payload.pool) { delete payload.pool; }
        if (!payload.full_backup_pool) { delete payload.full_backup_pool; }
        if (!payload.incremental_backup_pool) {
          delete payload.incremental_backup_pool;
        }
        if (!payload.differential_backup_pool) {
          delete payload.differential_backup_pool;
        }
        if (!payload.client) { delete payload.client; }
        if (!payload.fileset) { delete payload.fileset; }
        if (!payload.schedule) { delete payload.schedule; }
        if (!payload.catalog) { delete payload.catalog; }
        if (!payload.priority) {
          delete payload.priority;
        } else {
          payload.priority = Number.parseInt(payload.priority, 10);
        }
        const { response } = await request(
          'PUT',
          `/v1/deployments/${encodeURIComponent(deploymentId)}/directors/${encodeURIComponent(directorName)}/jobdefs/${encodeURIComponent(jobdefsName)}`,
          payload);
        if (response.ok) {
          document.getElementById('deployment-inspect-id').value = deploymentId;
          await loadDeploymentContents(deploymentId);
        }
      });
    document.getElementById('director-jobdefs-delete-button').addEventListener(
      'click',
      async () => {
        const form = new FormData(document.getElementById('director-jobdefs-form'));
        const deploymentId = String(form.get('deployment_id') ?? '').trim();
        const directorName = String(form.get('director_name') ?? '').trim();
        const jobdefsName = String(form.get('jobdefs_name') ?? '').trim();
        const { response } = await request(
          'DELETE',
          `/v1/deployments/${encodeURIComponent(deploymentId)}/directors/${encodeURIComponent(directorName)}/jobdefs/${encodeURIComponent(jobdefsName)}`);
        if (response.ok) {
          document.getElementById('deployment-inspect-id').value = deploymentId;
          await loadDeploymentContents(deploymentId);
        }
      });
    document.getElementById('storage-daemon-form').addEventListener(
      'submit',
      async (event) => {
        event.preventDefault();
        const form = new FormData(event.target);
        const deploymentId = String(form.get('deployment_id') ?? '').trim();
        const storageName = String(form.get('storage_name') ?? '').trim();
        const rawAddresses = String(form.get('addresses') ?? '');
        const addresses = rawAddresses.split('\n')
          .map((line) => line.trim())
          .filter((line) => line.length > 0);
        const rawSourceAddresses = String(form.get('source_addresses') ?? '');
        const sourceAddresses = rawSourceAddresses.split('\n')
          .map((line) => line.trim())
          .filter((line) => line.length > 0);
        const rawTlsAllowedCn = String(form.get('tls_allowed_cn') ?? '');
        const tlsAllowedCn = rawTlsAllowedCn.split('\n')
          .map((line) => line.trim())
          .filter((line) => line.length > 0);
        const rawPluginNames = String(form.get('plugin_names') ?? '');
        const pluginNames = rawPluginNames.split('\n')
          .map((line) => line.trim())
          .filter((line) => line.length > 0);
        const rawNdmpAddresses = String(form.get('ndmp_addresses') ?? '');
        const ndmpAddresses = rawNdmpAddresses.split('\n')
          .map((line) => line.trim())
          .filter((line) => line.length > 0);
        const rawBackendDirectories = String(form.get('backend_directories') ?? '');
        const backendDirectories = rawBackendDirectories.split('\n')
          .map((line) => line.trim())
          .filter((line) => line.length > 0);
        const payload = {
          address: String(form.get('address') ?? '').trim(),
          addresses: addresses,
          source_address: String(form.get('source_address') ?? '').trim(),
          source_addresses: sourceAddresses,
          port: String(form.get('port') ?? '').trim(),
          just_in_time_reservation: String(form.get('just_in_time_reservation') ?? '').trim(),
          maximum_concurrent_jobs: String(form.get('maximum_concurrent_jobs') ?? '').trim(),
          absolute_job_timeout: String(form.get('absolute_job_timeout') ?? '').trim(),
          statistics_collect_interval: String(form.get('statistics_collect_interval') ?? '').trim(),
          allow_bandwidth_bursting: String(form.get('allow_bandwidth_bursting') ?? '').trim(),
          tls_authenticate: String(form.get('tls_authenticate') ?? '').trim(),
          tls_enable: String(form.get('tls_enable') ?? '').trim(),
          tls_require: String(form.get('tls_require') ?? '').trim(),
          tls_verify_peer: String(form.get('tls_verify_peer') ?? '').trim(),
          tls_cipher_list: String(form.get('tls_cipher_list') ?? '').trim(),
          tls_cipher_suites: String(form.get('tls_cipher_suites') ?? '').trim(),
          tls_dh_file: String(form.get('tls_dh_file') ?? '').trim(),
          tls_protocol: String(form.get('tls_protocol') ?? '').trim(),
          tls_ca_certificate_file: String(form.get('tls_ca_certificate_file') ?? '').trim(),
          tls_ca_certificate_dir: String(form.get('tls_ca_certificate_dir') ?? '').trim(),
          tls_certificate_revocation_list: String(form.get('tls_certificate_revocation_list') ?? '').trim(),
          tls_certificate: String(form.get('tls_certificate') ?? '').trim(),
          tls_key: String(form.get('tls_key') ?? '').trim(),
          tls_allowed_cn: tlsAllowedCn,
          ndmp_enable: String(form.get('ndmp_enable') ?? '').trim(),
          ndmp_snooping: String(form.get('ndmp_snooping') ?? '').trim(),
          ndmp_log_level: String(form.get('ndmp_log_level') ?? '').trim(),
          ndmp_address: String(form.get('ndmp_address') ?? '').trim(),
          ndmp_port: String(form.get('ndmp_port') ?? '').trim(),
          ndmp_addresses: ndmpAddresses,
          autoxflate_on_replication: String(form.get('autoxflate_on_replication') ?? '').trim(),
          collect_device_statistics: String(form.get('collect_device_statistics') ?? '').trim(),
          collect_job_statistics: String(form.get('collect_job_statistics') ?? '').trim(),
          device_reserve_by_media_type: String(form.get('device_reserve_by_media_type') ?? '').trim(),
          file_device_concurrent_read: String(form.get('file_device_concurrent_read') ?? '').trim(),
          ver_id: String(form.get('ver_id') ?? '').trim(),
          log_timestamp_format: String(form.get('log_timestamp_format') ?? '').trim(),
          maximum_bandwidth_per_job: String(form.get('maximum_bandwidth_per_job') ?? '').trim(),
          secure_erase_command: String(form.get('secure_erase_command') ?? '').trim(),
          enable_ktls: String(form.get('enable_ktls') ?? '').trim(),
          sd_connect_timeout: String(form.get('sd_connect_timeout') ?? '').trim(),
          fd_connect_timeout: String(form.get('fd_connect_timeout') ?? '').trim(),
          heartbeat_interval: String(form.get('heartbeat_interval') ?? '').trim(),
          checkpoint_interval: String(form.get('checkpoint_interval') ?? '').trim(),
          client_connect_wait: String(form.get('client_connect_wait') ?? '').trim(),
          maximum_network_buffer_size: String(form.get('maximum_network_buffer_size') ?? '').trim(),
          description: String(form.get('description') ?? '').trim(),
          working_directory: String(form.get('working_directory') ?? '').trim(),
          plugin_directory: String(form.get('plugin_directory') ?? '').trim(),
          plugin_names: pluginNames,
          backend_directories: backendDirectories,
          scripts_directory: String(form.get('scripts_directory') ?? '').trim(),
          messages: String(form.get('messages') ?? '').trim(),
        };
        if (!payload.address) { delete payload.address; }
        if (payload.addresses.length === 0) { delete payload.addresses; }
        if (!payload.source_address) { delete payload.source_address; }
        if (payload.source_addresses.length === 0) {
          delete payload.source_addresses;
        }
        if (!payload.port) { delete payload.port; } else { payload.port = Number(payload.port); }
        if (!payload.just_in_time_reservation) { delete payload.just_in_time_reservation; } else { payload.just_in_time_reservation = payload.just_in_time_reservation === 'true'; }
        if (!payload.maximum_concurrent_jobs) { delete payload.maximum_concurrent_jobs; } else { payload.maximum_concurrent_jobs = Number(payload.maximum_concurrent_jobs); }
        if (!payload.absolute_job_timeout) { delete payload.absolute_job_timeout; } else { payload.absolute_job_timeout = Number(payload.absolute_job_timeout); }
        if (!payload.statistics_collect_interval) { delete payload.statistics_collect_interval; } else { payload.statistics_collect_interval = Number(payload.statistics_collect_interval); }
        if (!payload.allow_bandwidth_bursting) { delete payload.allow_bandwidth_bursting; } else { payload.allow_bandwidth_bursting = payload.allow_bandwidth_bursting === 'true'; }
        if (!payload.tls_authenticate) { delete payload.tls_authenticate; } else { payload.tls_authenticate = payload.tls_authenticate === 'true'; }
        if (!payload.tls_enable) { delete payload.tls_enable; } else { payload.tls_enable = payload.tls_enable === 'true'; }
        if (!payload.tls_require) { delete payload.tls_require; } else { payload.tls_require = payload.tls_require === 'true'; }
        if (!payload.tls_verify_peer) { delete payload.tls_verify_peer; } else { payload.tls_verify_peer = payload.tls_verify_peer === 'true'; }
        if (!payload.tls_cipher_list) { delete payload.tls_cipher_list; }
        if (!payload.tls_cipher_suites) { delete payload.tls_cipher_suites; }
        if (!payload.tls_dh_file) { delete payload.tls_dh_file; }
        if (!payload.tls_protocol) { delete payload.tls_protocol; }
        if (!payload.tls_ca_certificate_file) { delete payload.tls_ca_certificate_file; }
        if (!payload.tls_ca_certificate_dir) { delete payload.tls_ca_certificate_dir; }
        if (!payload.tls_certificate_revocation_list) { delete payload.tls_certificate_revocation_list; }
        if (!payload.tls_certificate) { delete payload.tls_certificate; }
        if (!payload.tls_key) { delete payload.tls_key; }
        if (payload.tls_allowed_cn.length === 0) { delete payload.tls_allowed_cn; }
        if (!payload.ndmp_enable) { delete payload.ndmp_enable; } else { payload.ndmp_enable = payload.ndmp_enable === 'true'; }
        if (!payload.ndmp_snooping) { delete payload.ndmp_snooping; } else { payload.ndmp_snooping = payload.ndmp_snooping === 'true'; }
        if (!payload.ndmp_log_level) { delete payload.ndmp_log_level; } else { payload.ndmp_log_level = Number(payload.ndmp_log_level); }
        if (!payload.ndmp_address) { delete payload.ndmp_address; }
        if (!payload.ndmp_port) { delete payload.ndmp_port; } else { payload.ndmp_port = Number(payload.ndmp_port); }
        if (payload.ndmp_addresses.length === 0) { delete payload.ndmp_addresses; }
        if (!payload.autoxflate_on_replication) { delete payload.autoxflate_on_replication; } else { payload.autoxflate_on_replication = payload.autoxflate_on_replication === 'true'; }
        if (!payload.collect_device_statistics) { delete payload.collect_device_statistics; } else { payload.collect_device_statistics = payload.collect_device_statistics === 'true'; }
        if (!payload.collect_job_statistics) { delete payload.collect_job_statistics; } else { payload.collect_job_statistics = payload.collect_job_statistics === 'true'; }
        if (!payload.device_reserve_by_media_type) { delete payload.device_reserve_by_media_type; } else { payload.device_reserve_by_media_type = payload.device_reserve_by_media_type === 'true'; }
        if (!payload.file_device_concurrent_read) { delete payload.file_device_concurrent_read; } else { payload.file_device_concurrent_read = payload.file_device_concurrent_read === 'true'; }
        if (!payload.ver_id) { delete payload.ver_id; }
        if (!payload.log_timestamp_format) { delete payload.log_timestamp_format; }
        if (!payload.maximum_bandwidth_per_job) { delete payload.maximum_bandwidth_per_job; } else { payload.maximum_bandwidth_per_job = Number(payload.maximum_bandwidth_per_job); }
        if (!payload.secure_erase_command) { delete payload.secure_erase_command; }
        if (!payload.enable_ktls) { delete payload.enable_ktls; } else { payload.enable_ktls = payload.enable_ktls === 'true'; }
        if (!payload.sd_connect_timeout) { delete payload.sd_connect_timeout; } else { payload.sd_connect_timeout = Number(payload.sd_connect_timeout); }
        if (!payload.fd_connect_timeout) { delete payload.fd_connect_timeout; } else { payload.fd_connect_timeout = Number(payload.fd_connect_timeout); }
        if (!payload.heartbeat_interval) { delete payload.heartbeat_interval; } else { payload.heartbeat_interval = Number(payload.heartbeat_interval); }
        if (!payload.checkpoint_interval) { delete payload.checkpoint_interval; } else { payload.checkpoint_interval = Number(payload.checkpoint_interval); }
        if (!payload.client_connect_wait) { delete payload.client_connect_wait; } else { payload.client_connect_wait = Number(payload.client_connect_wait); }
        if (!payload.maximum_network_buffer_size) { delete payload.maximum_network_buffer_size; } else { payload.maximum_network_buffer_size = Number(payload.maximum_network_buffer_size); }
        if (!payload.description) { delete payload.description; }
        if (!payload.working_directory) { delete payload.working_directory; }
        if (!payload.plugin_directory) { delete payload.plugin_directory; }
        if (payload.plugin_names.length === 0) { delete payload.plugin_names; }
        if (payload.backend_directories.length === 0) { delete payload.backend_directories; }
        if (!payload.scripts_directory) { delete payload.scripts_directory; }
        if (!payload.messages) { delete payload.messages; }
        const { response } = await request(
          'PUT',
          `/v1/deployments/${encodeURIComponent(deploymentId)}/storages/${encodeURIComponent(storageName)}`,
          payload);
        if (response.ok) {
          document.getElementById('deployment-inspect-id').value = deploymentId;
          await loadDeploymentContents(deploymentId);
        }
      });
    document.getElementById('director-daemon-form').addEventListener(
      'submit',
      async (event) => {
        event.preventDefault();
        const form = new FormData(event.target);
        const deploymentId = String(form.get('deployment_id') ?? '').trim();
        const directorName = String(form.get('director_name') ?? '').trim();
        const rawSourceAddresses = String(form.get('source_addresses') ?? '');
        const sourceAddresses = rawSourceAddresses.split('\n')
          .map((line) => line.trim())
          .filter((line) => line.length > 0);
        const payload = {
          address: String(form.get('address') ?? '').trim(),
          source_address: String(form.get('source_address') ?? '').trim(),
          source_addresses: sourceAddresses,
          port: String(form.get('port') ?? '').trim(),
          description: String(form.get('description') ?? '').trim(),
          working_directory: String(form.get('working_directory') ?? '').trim(),
          plugin_directory: String(form.get('plugin_directory') ?? '').trim(),
          scripts_directory: String(form.get('scripts_directory') ?? '').trim(),
          messages: String(form.get('messages') ?? '').trim(),
        };
        if (!payload.address) { delete payload.address; }
        if (!payload.source_address) { delete payload.source_address; }
        if (payload.source_addresses.length === 0) {
          delete payload.source_addresses;
        }
        if (!payload.port) {
          delete payload.port;
        } else {
          payload.port = Number(payload.port);
        }
        if (!payload.description) { delete payload.description; }
        if (!payload.working_directory) { delete payload.working_directory; }
        if (!payload.plugin_directory) { delete payload.plugin_directory; }
        if (!payload.scripts_directory) { delete payload.scripts_directory; }
        if (!payload.messages) { delete payload.messages; }
        const { response } = await request(
          'PUT',
          `/v1/deployments/${encodeURIComponent(deploymentId)}/directors/${encodeURIComponent(directorName)}`,
          payload);
        if (response.ok) {
          document.getElementById('deployment-inspect-id').value = deploymentId;
          await loadDeploymentContents(deploymentId);
        }
      });
    document.getElementById('storage-director-form').addEventListener(
      'submit',
      async (event) => {
        event.preventDefault();
        const form = new FormData(event.target);
        const deploymentId = String(form.get('deployment_id') ?? '').trim();
        const storageName = String(form.get('storage_name') ?? '').trim();
        const directorName = String(form.get('director_name') ?? '').trim();
        const payload = {
          password: String(form.get('password') ?? '').trim(),
          description: String(form.get('description') ?? '').trim(),
          monitor: document.getElementById('storage-director-monitor').checked,
          maximum_bandwidth_per_job: String(
            form.get('maximum_bandwidth_per_job') ?? '').trim(),
        };
        if (!payload.password) { delete payload.password; }
        if (!payload.description) { delete payload.description; }
        if (!payload.maximum_bandwidth_per_job) {
          delete payload.maximum_bandwidth_per_job;
        } else {
          payload.maximum_bandwidth_per_job
            = Number.parseInt(payload.maximum_bandwidth_per_job, 10);
        }
        const { response } = await request(
          'PUT',
          `/v1/deployments/${encodeURIComponent(deploymentId)}/storages/${encodeURIComponent(storageName)}/directors/${encodeURIComponent(directorName)}`,
          payload);
        if (response.ok) {
          document.getElementById('deployment-inspect-id').value = deploymentId;
          await loadDeploymentContents(deploymentId);
        }
      });
    document.getElementById('storage-director-delete-button').addEventListener(
      'click',
      async () => {
        const form = new FormData(document.getElementById('storage-director-form'));
        const deploymentId = String(form.get('deployment_id') ?? '').trim();
        const storageName = String(form.get('storage_name') ?? '').trim();
        const directorName = String(form.get('director_name') ?? '').trim();
        const { response } = await request(
          'DELETE',
          `/v1/deployments/${encodeURIComponent(deploymentId)}/storages/${encodeURIComponent(storageName)}/directors/${encodeURIComponent(directorName)}`);
        if (response.ok) {
          document.getElementById('deployment-inspect-id').value = deploymentId;
          await loadDeploymentContents(deploymentId);
        }
      });
    document.getElementById('storage-device-form').addEventListener(
      'submit',
      async (event) => {
        event.preventDefault();
        const form = new FormData(event.target);
        const deploymentId = String(form.get('deployment_id') ?? '').trim();
        const storageName = String(form.get('storage_name') ?? '').trim();
        const deviceName = String(form.get('device_name') ?? '').trim();
        const payload = {
          media_type: String(form.get('media_type') ?? '').trim(),
          archive_device: String(form.get('archive_device') ?? '').trim(),
          device_type: String(form.get('device_type') ?? '').trim(),
          description: String(form.get('description') ?? '').trim(),
        };
        if (!payload.media_type) { delete payload.media_type; }
        if (!payload.archive_device) { delete payload.archive_device; }
        if (!payload.device_type) { delete payload.device_type; }
        if (!payload.description) { delete payload.description; }
        const { response } = await request(
          'PUT',
          `/v1/deployments/${encodeURIComponent(deploymentId)}/storages/${encodeURIComponent(storageName)}/devices/${encodeURIComponent(deviceName)}`,
          payload);
        if (response.ok) {
          document.getElementById('deployment-inspect-id').value = deploymentId;
          await loadDeploymentContents(deploymentId);
        }
      });
    document.getElementById('storage-device-delete-button').addEventListener(
      'click',
      async () => {
        const form = new FormData(document.getElementById('storage-device-form'));
        const deploymentId = String(form.get('deployment_id') ?? '').trim();
        const storageName = String(form.get('storage_name') ?? '').trim();
        const deviceName = String(form.get('device_name') ?? '').trim();
        const { response } = await request(
          'DELETE',
          `/v1/deployments/${encodeURIComponent(deploymentId)}/storages/${encodeURIComponent(storageName)}/devices/${encodeURIComponent(deviceName)}`);
        if (response.ok) {
          document.getElementById('deployment-inspect-id').value = deploymentId;
          await loadDeploymentContents(deploymentId);
        }
      });
    document.getElementById('storage-messages-form').addEventListener(
      'submit',
      async (event) => {
        event.preventDefault();
        const form = new FormData(event.target);
        const deploymentId = String(form.get('deployment_id') ?? '').trim();
        const storageName = String(form.get('storage_name') ?? '').trim();
        const messagesName = String(form.get('messages_name') ?? '').trim();
        const rawEntries = String(form.get('entries') ?? '');
        const entries = rawEntries.split('\n')
          .map((line) => line.trim())
          .filter((line) => line.length > 0);
        const payload = {
          description: String(form.get('description') ?? '').trim(),
          entries,
        };
        if (!payload.description) { delete payload.description; }
        if (payload.entries.length === 0) { delete payload.entries; }
        const { response } = await request(
          'PUT',
          `/v1/deployments/${encodeURIComponent(deploymentId)}/storages/${encodeURIComponent(storageName)}/messages/${encodeURIComponent(messagesName)}`,
          payload);
        if (response.ok) {
          document.getElementById('deployment-inspect-id').value = deploymentId;
          await loadDeploymentContents(deploymentId);
        }
      });
    document.getElementById('storage-messages-delete-button').addEventListener(
      'click',
      async () => {
        const form = new FormData(document.getElementById('storage-messages-form'));
        const deploymentId = String(form.get('deployment_id') ?? '').trim();
        const storageName = String(form.get('storage_name') ?? '').trim();
        const messagesName = String(form.get('messages_name') ?? '').trim();
        const { response } = await request(
          'DELETE',
          `/v1/deployments/${encodeURIComponent(deploymentId)}/storages/${encodeURIComponent(storageName)}/messages/${encodeURIComponent(messagesName)}`);
        if (response.ok) {
          document.getElementById('deployment-inspect-id').value = deploymentId;
          await loadDeploymentContents(deploymentId);
        }
      });
    document.getElementById('storage-ndmp-form').addEventListener(
      'submit',
      async (event) => {
        event.preventDefault();
        const form = new FormData(event.target);
        const deploymentId = String(form.get('deployment_id') ?? '').trim();
        const storageName = String(form.get('storage_name') ?? '').trim();
        const ndmpName = String(form.get('ndmp_name') ?? '').trim();
        const rawLogLevel = String(form.get('log_level') ?? '').trim();
        const payload = {
          username: String(form.get('username') ?? '').trim(),
          password: String(form.get('password') ?? '').trim(),
          auth_type: String(form.get('auth_type') ?? '').trim(),
        };
        if (!payload.username) { delete payload.username; }
        if (!payload.password) { delete payload.password; }
        if (!payload.auth_type) { delete payload.auth_type; }
        if (rawLogLevel) { payload.log_level = Number.parseInt(rawLogLevel, 10); }
        const { response } = await request(
          'PUT',
          `/v1/deployments/${encodeURIComponent(deploymentId)}/storages/${encodeURIComponent(storageName)}/ndmp/${encodeURIComponent(ndmpName)}`,
          payload);
        if (response.ok) {
          document.getElementById('deployment-inspect-id').value = deploymentId;
          await loadDeploymentContents(deploymentId);
        }
      });
    document.getElementById('storage-ndmp-delete-button').addEventListener(
      'click',
      async () => {
        const form = new FormData(document.getElementById('storage-ndmp-form'));
        const deploymentId = String(form.get('deployment_id') ?? '').trim();
        const storageName = String(form.get('storage_name') ?? '').trim();
        const ndmpName = String(form.get('ndmp_name') ?? '').trim();
        const { response } = await request(
          'DELETE',
          `/v1/deployments/${encodeURIComponent(deploymentId)}/storages/${encodeURIComponent(storageName)}/ndmp/${encodeURIComponent(ndmpName)}`);
        if (response.ok) {
          document.getElementById('deployment-inspect-id').value = deploymentId;
          await loadDeploymentContents(deploymentId);
        }
      });
    document.getElementById('storage-autochanger-form').addEventListener(
      'submit',
      async (event) => {
        event.preventDefault();
        const form = new FormData(event.target);
        const deploymentId = String(form.get('deployment_id') ?? '').trim();
        const storageName = String(form.get('storage_name') ?? '').trim();
        const autochangerName = String(form.get('autochanger_name') ?? '').trim();
        const rawDevices = String(form.get('devices') ?? '');
        const devices = rawDevices.split('\n')
          .map((line) => line.trim())
          .filter((line) => line.length > 0);
        const payload = {
          devices,
          changer_device: String(form.get('changer_device') ?? '').trim(),
          changer_command: String(form.get('changer_command') ?? '').trim(),
          description: String(form.get('description') ?? '').trim(),
        };
        if (payload.devices.length === 0) { delete payload.devices; }
        if (!payload.changer_device) { delete payload.changer_device; }
        if (!payload.changer_command) { delete payload.changer_command; }
        if (!payload.description) { delete payload.description; }
        const { response } = await request(
          'PUT',
          `/v1/deployments/${encodeURIComponent(deploymentId)}/storages/${encodeURIComponent(storageName)}/autochangers/${encodeURIComponent(autochangerName)}`,
          payload);
        if (response.ok) {
          document.getElementById('deployment-inspect-id').value = deploymentId;
          await loadDeploymentContents(deploymentId);
        }
      });
    document.getElementById('storage-autochanger-delete-button').addEventListener(
      'click',
      async () => {
        const form = new FormData(document.getElementById('storage-autochanger-form'));
        const deploymentId = String(form.get('deployment_id') ?? '').trim();
        const storageName = String(form.get('storage_name') ?? '').trim();
        const autochangerName = String(form.get('autochanger_name') ?? '').trim();
        const { response } = await request(
          'DELETE',
          `/v1/deployments/${encodeURIComponent(deploymentId)}/storages/${encodeURIComponent(storageName)}/autochangers/${encodeURIComponent(autochangerName)}`);
        if (response.ok) {
          document.getElementById('deployment-inspect-id').value = deploymentId;
          await loadDeploymentContents(deploymentId);
        }
      });
    document.getElementById('console-console-form').addEventListener(
      'submit',
      async (event) => {
        event.preventDefault();
        const form = new FormData(event.target);
        const deploymentId = String(form.get('deployment_id') ?? '').trim();
        const consoleConfigName = String(form.get('console_config_name') ?? '').trim();
        const consoleName = String(form.get('console_name') ?? '').trim();
        const rawHistoryLength = String(form.get('history_length') ?? '').trim();
        const rawHeartbeatInterval = String(form.get('heartbeat_interval') ?? '').trim();
        const rawTlsAllowedCn = String(form.get('tls_allowed_cn') ?? '');
        const tlsAllowedCn = rawTlsAllowedCn.split('\n')
          .map((value) => value.trim())
          .filter((value) => value.length > 0);
        const payload = {
          director: String(form.get('director') ?? '').trim(),
          password: String(form.get('password') ?? '').trim(),
          description: String(form.get('description') ?? '').trim(),
          rc_file: String(form.get('rc_file') ?? '').trim(),
          history_file: String(form.get('history_file') ?? '').trim(),
          tls_authenticate: String(form.get('tls_authenticate') ?? '').trim(),
          tls_enable: String(form.get('tls_enable') ?? '').trim(),
          tls_require: String(form.get('tls_require') ?? '').trim(),
          tls_verify_peer: String(form.get('tls_verify_peer') ?? '').trim(),
          tls_cipher_list: String(form.get('tls_cipher_list') ?? '').trim(),
          tls_cipher_suites: String(form.get('tls_cipher_suites') ?? '').trim(),
          tls_dh_file: String(form.get('tls_dh_file') ?? '').trim(),
          tls_protocol: String(form.get('tls_protocol') ?? '').trim(),
          tls_ca_certificate_file: String(form.get('tls_ca_certificate_file') ?? '').trim(),
          tls_ca_certificate_dir: String(form.get('tls_ca_certificate_dir') ?? '').trim(),
          tls_certificate_revocation_list: String(form.get('tls_certificate_revocation_list') ?? '').trim(),
          tls_certificate: String(form.get('tls_certificate') ?? '').trim(),
          tls_key: String(form.get('tls_key') ?? '').trim(),
          tls_allowed_cn: tlsAllowedCn,
        };
        if (!payload.director) { delete payload.director; }
        if (!payload.password) { delete payload.password; }
        if (!payload.description) { delete payload.description; }
        if (!payload.rc_file) { delete payload.rc_file; }
        if (!payload.history_file) { delete payload.history_file; }
        if (!payload.tls_cipher_list) { delete payload.tls_cipher_list; }
        if (!payload.tls_cipher_suites) { delete payload.tls_cipher_suites; }
        if (!payload.tls_dh_file) { delete payload.tls_dh_file; }
        if (!payload.tls_protocol) { delete payload.tls_protocol; }
        if (!payload.tls_ca_certificate_file) { delete payload.tls_ca_certificate_file; }
        if (!payload.tls_ca_certificate_dir) { delete payload.tls_ca_certificate_dir; }
        if (!payload.tls_certificate_revocation_list) { delete payload.tls_certificate_revocation_list; }
        if (!payload.tls_certificate) { delete payload.tls_certificate; }
        if (!payload.tls_key) { delete payload.tls_key; }
        if (payload.tls_allowed_cn.length === 0) { delete payload.tls_allowed_cn; }
        if (rawHistoryLength) { payload.history_length = Number(rawHistoryLength); }
        if (rawHeartbeatInterval) {
          payload.heartbeat_interval = Number(rawHeartbeatInterval);
        }
        if (!payload.tls_authenticate) { delete payload.tls_authenticate; } else { payload.tls_authenticate = payload.tls_authenticate === 'true'; }
        if (!payload.tls_enable) { delete payload.tls_enable; } else { payload.tls_enable = payload.tls_enable === 'true'; }
        if (!payload.tls_require) { delete payload.tls_require; } else { payload.tls_require = payload.tls_require === 'true'; }
        if (!payload.tls_verify_peer) { delete payload.tls_verify_peer; } else { payload.tls_verify_peer = payload.tls_verify_peer === 'true'; }
        const { response } = await request(
          'PUT',
          `/v1/deployments/${encodeURIComponent(deploymentId)}/consoles/${encodeURIComponent(consoleConfigName)}/consoles/${encodeURIComponent(consoleName)}`,
          payload);
        if (response.ok) {
          document.getElementById('deployment-inspect-id').value = deploymentId;
          await loadDeploymentContents(deploymentId);
        }
      });
    document.getElementById('console-console-delete-button').addEventListener(
      'click',
      async () => {
        const form = new FormData(document.getElementById('console-console-form'));
        const deploymentId = String(form.get('deployment_id') ?? '').trim();
        const consoleConfigName = String(form.get('console_config_name') ?? '').trim();
        const consoleName = String(form.get('console_name') ?? '').trim();
        const { response } = await request(
          'DELETE',
          `/v1/deployments/${encodeURIComponent(deploymentId)}/consoles/${encodeURIComponent(consoleConfigName)}/consoles/${encodeURIComponent(consoleName)}`);
        if (response.ok) {
          document.getElementById('deployment-inspect-id').value = deploymentId;
          await loadDeploymentContents(deploymentId);
        }
      });
    document.getElementById('console-director-form').addEventListener(
      'submit',
      async (event) => {
        event.preventDefault();
        const form = new FormData(event.target);
        const deploymentId = String(form.get('deployment_id') ?? '').trim();
        const consoleConfigName = String(form.get('console_config_name') ?? '').trim();
        const directorName = String(form.get('director_name') ?? '').trim();
        const rawPort = String(form.get('port') ?? '').trim();
        const rawHeartbeatInterval = String(form.get('heartbeat_interval') ?? '').trim();
        const rawTlsAllowedCn = String(form.get('tls_allowed_cn') ?? '');
        const tlsAllowedCn = rawTlsAllowedCn.split('\n')
          .map((value) => value.trim())
          .filter((value) => value.length > 0);
        const payload = {
          address: String(form.get('address') ?? '').trim(),
          password: String(form.get('password') ?? '').trim(),
          description: String(form.get('description') ?? '').trim(),
          tls_authenticate: String(form.get('tls_authenticate') ?? '').trim(),
          tls_enable: String(form.get('tls_enable') ?? '').trim(),
          tls_require: String(form.get('tls_require') ?? '').trim(),
          tls_verify_peer: String(form.get('tls_verify_peer') ?? '').trim(),
          tls_cipher_list: String(form.get('tls_cipher_list') ?? '').trim(),
          tls_cipher_suites: String(form.get('tls_cipher_suites') ?? '').trim(),
          tls_dh_file: String(form.get('tls_dh_file') ?? '').trim(),
          tls_protocol: String(form.get('tls_protocol') ?? '').trim(),
          tls_ca_certificate_file: String(form.get('tls_ca_certificate_file') ?? '').trim(),
          tls_ca_certificate_dir: String(form.get('tls_ca_certificate_dir') ?? '').trim(),
          tls_certificate_revocation_list: String(form.get('tls_certificate_revocation_list') ?? '').trim(),
          tls_certificate: String(form.get('tls_certificate') ?? '').trim(),
          tls_key: String(form.get('tls_key') ?? '').trim(),
          tls_allowed_cn: tlsAllowedCn,
        };
        if (!payload.address) { delete payload.address; }
        if (!payload.password) { delete payload.password; }
        if (!payload.description) { delete payload.description; }
        if (!payload.tls_cipher_list) { delete payload.tls_cipher_list; }
        if (!payload.tls_cipher_suites) { delete payload.tls_cipher_suites; }
        if (!payload.tls_dh_file) { delete payload.tls_dh_file; }
        if (!payload.tls_protocol) { delete payload.tls_protocol; }
        if (!payload.tls_ca_certificate_file) { delete payload.tls_ca_certificate_file; }
        if (!payload.tls_ca_certificate_dir) { delete payload.tls_ca_certificate_dir; }
        if (!payload.tls_certificate_revocation_list) { delete payload.tls_certificate_revocation_list; }
        if (!payload.tls_certificate) { delete payload.tls_certificate; }
        if (!payload.tls_key) { delete payload.tls_key; }
        if (payload.tls_allowed_cn.length === 0) { delete payload.tls_allowed_cn; }
        if (rawPort) { payload.port = Number(rawPort); }
        if (rawHeartbeatInterval) {
          payload.heartbeat_interval = Number(rawHeartbeatInterval);
        }
        if (!payload.tls_authenticate) { delete payload.tls_authenticate; } else { payload.tls_authenticate = payload.tls_authenticate === 'true'; }
        if (!payload.tls_enable) { delete payload.tls_enable; } else { payload.tls_enable = payload.tls_enable === 'true'; }
        if (!payload.tls_require) { delete payload.tls_require; } else { payload.tls_require = payload.tls_require === 'true'; }
        if (!payload.tls_verify_peer) { delete payload.tls_verify_peer; } else { payload.tls_verify_peer = payload.tls_verify_peer === 'true'; }
        const { response } = await request(
          'PUT',
          `/v1/deployments/${encodeURIComponent(deploymentId)}/consoles/${encodeURIComponent(consoleConfigName)}/directors/${encodeURIComponent(directorName)}`,
          payload);
        if (response.ok) {
          document.getElementById('deployment-inspect-id').value = deploymentId;
          await loadDeploymentContents(deploymentId);
        }
      });
    document.getElementById('console-director-delete-button').addEventListener(
      'click',
      async () => {
        const form = new FormData(document.getElementById('console-director-form'));
        const deploymentId = String(form.get('deployment_id') ?? '').trim();
        const consoleConfigName = String(form.get('console_config_name') ?? '').trim();
        const directorName = String(form.get('director_name') ?? '').trim();
        const { response } = await request(
          'DELETE',
          `/v1/deployments/${encodeURIComponent(deploymentId)}/consoles/${encodeURIComponent(consoleConfigName)}/directors/${encodeURIComponent(directorName)}`);
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

http::response<http::string_body> HandleDeploymentClientDaemonPutRequest(
    ServiceState& state,
    const http::request<http::string_body>& request,
    std::string_view deployment_id,
    std::string_view client_name)
{
  auto deployment = state.GetDeployment(deployment_id);
  if (!deployment) {
    return ErrorResponse(http::status::not_found, "deployment not found.");
  }

  std::string error;
  auto spec = ParseStorageDaemonRequest(request.body(), error);
  if (!spec) { return ErrorResponse(http::status::bad_request, error); }

  ClientDaemonResourceSpec resource_spec{
      .address = spec->address,
      .addresses = spec->addresses,
      .source_address = spec->source_address,
      .source_addresses = spec->source_addresses,
      .port = spec->port,
      .maximum_concurrent_jobs = spec->maximum_concurrent_jobs,
      .maximum_workers_per_job = spec->maximum_workers_per_job,
      .absolute_job_timeout = spec->absolute_job_timeout,
      .allow_bandwidth_bursting = spec->allow_bandwidth_bursting,
      .tls_authenticate = spec->tls_authenticate,
      .tls_enable = spec->tls_enable,
      .tls_require = spec->tls_require,
      .tls_verify_peer = spec->tls_verify_peer,
      .tls_cipher_list = spec->tls_cipher_list,
      .tls_cipher_suites = spec->tls_cipher_suites,
      .tls_dh_file = spec->tls_dh_file,
      .tls_protocol = spec->tls_protocol,
      .tls_ca_certificate_file = spec->tls_ca_certificate_file,
      .tls_ca_certificate_dir = spec->tls_ca_certificate_dir,
      .tls_certificate_revocation_list = spec->tls_certificate_revocation_list,
      .tls_certificate = spec->tls_certificate,
      .tls_key = spec->tls_key,
      .tls_allowed_cn = spec->tls_allowed_cn,
      .pki_signatures = spec->pki_signatures,
      .pki_encryption = spec->pki_encryption,
      .pki_key_pair = spec->pki_key_pair,
      .pki_signers = spec->pki_signers,
      .pki_master_keys = spec->pki_master_keys,
      .pki_cipher = spec->pki_cipher,
      .always_use_lmdb = spec->always_use_lmdb,
      .lmdb_threshold = spec->lmdb_threshold,
      .ver_id = spec->ver_id,
      .log_timestamp_format = spec->log_timestamp_format,
      .maximum_bandwidth_per_job = spec->maximum_bandwidth_per_job,
      .secure_erase_command = spec->secure_erase_command,
      .grpc_module = spec->grpc_module,
      .enable_ktls = spec->enable_ktls,
      .sd_connect_timeout = spec->sd_connect_timeout,
      .heartbeat_interval = spec->heartbeat_interval,
      .maximum_network_buffer_size = spec->maximum_network_buffer_size,
      .description = spec->description,
      .working_directory = spec->working_directory,
      .plugin_directory = spec->plugin_directory,
      .plugin_names = spec->plugin_names,
      .allowed_script_dirs = spec->allowed_script_dirs,
      .allowed_job_commands = spec->allowed_job_commands,
      .scripts_directory = spec->scripts_directory,
      .messages = spec->messages,
  };
  auto result = state.UpsertClientDaemonResource(deployment_id, client_name,
                                                 resource_spec);
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
  json_object_set_new(root.get(), "client_name",
                      json_string(std::string{client_name}.c_str()));
  json_object_set(root.get(), "client", client_json.get());
  return JsonResponse(http::status::ok, DumpJson(root.get()));
}

http::response<http::string_body> HandleDeploymentDirectorDaemonPutRequest(
    ServiceState& state,
    const http::request<http::string_body>& request,
    std::string_view deployment_id,
    std::string_view director_name)
{
  auto deployment = state.GetDeployment(deployment_id);
  if (!deployment) {
    return ErrorResponse(http::status::not_found, "deployment not found.");
  }

  std::string error;
  auto spec = ParseStorageDaemonRequest(request.body(), error);
  if (!spec) { return ErrorResponse(http::status::bad_request, error); }

  DirectorDaemonResourceSpec resource_spec{
      .address = spec->address,
      .addresses = spec->addresses,
      .source_address = spec->source_address,
      .source_addresses = spec->source_addresses,
      .port = spec->port,
      .maximum_concurrent_jobs = spec->maximum_concurrent_jobs,
      .absolute_job_timeout = spec->absolute_job_timeout,
      .tls_authenticate = spec->tls_authenticate,
      .tls_enable = spec->tls_enable,
      .tls_require = spec->tls_require,
      .tls_verify_peer = spec->tls_verify_peer,
      .tls_cipher_list = spec->tls_cipher_list,
      .tls_cipher_suites = spec->tls_cipher_suites,
      .tls_dh_file = spec->tls_dh_file,
      .tls_protocol = spec->tls_protocol,
      .tls_ca_certificate_file = spec->tls_ca_certificate_file,
      .tls_ca_certificate_dir = spec->tls_ca_certificate_dir,
      .tls_certificate_revocation_list = spec->tls_certificate_revocation_list,
      .tls_certificate = spec->tls_certificate,
      .tls_key = spec->tls_key,
      .tls_allowed_cn = spec->tls_allowed_cn,
      .ver_id = spec->ver_id,
      .log_timestamp_format = spec->log_timestamp_format,
      .secure_erase_command = spec->secure_erase_command,
      .enable_ktls = spec->enable_ktls,
      .fd_connect_timeout = spec->fd_connect_timeout,
      .sd_connect_timeout = spec->sd_connect_timeout,
      .heartbeat_interval = spec->heartbeat_interval,
      .description = spec->description,
      .working_directory = spec->working_directory,
      .plugin_directory = spec->plugin_directory,
      .plugin_names = spec->plugin_names,
      .scripts_directory = spec->scripts_directory,
      .messages = spec->messages,
  };
  auto result = state.UpsertDirectorDaemonResource(deployment_id, director_name,
                                                   resource_spec);
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
  json_object_set(root.get(), "director", director_json.get());
  return JsonResponse(http::status::ok, DumpJson(root.get()));
}

http::response<http::string_body> HandleDeploymentClientMessagesPutRequest(
    ServiceState& state,
    const http::request<http::string_body>& request,
    std::string_view deployment_id,
    std::string_view client_name,
    std::string_view messages_name)
{
  auto deployment = state.GetDeployment(deployment_id);
  if (!deployment) {
    return ErrorResponse(http::status::not_found, "deployment not found.");
  }

  std::string error;
  auto spec = ParseDirectorMessagesRequest(request.body(), error);
  if (!spec) { return ErrorResponse(http::status::bad_request, error); }

  ClientMessagesResourceSpec resource_spec{
      .description = spec->description,
      .entries = spec->entries,
  };
  auto result = state.UpsertClientMessagesResource(
      deployment_id, client_name, messages_name, resource_spec);
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
  json_object_set_new(root.get(), "client_name",
                      json_string(std::string{client_name}.c_str()));
  json_object_set_new(root.get(), "messages_name",
                      json_string(std::string{messages_name}.c_str()));
  json_object_set(root.get(), "client", client_json.get());
  return JsonResponse(http::status::ok, DumpJson(root.get()));
}

http::response<http::string_body> HandleDeploymentClientMessagesDeleteRequest(
    ServiceState& state,
    std::string_view deployment_id,
    std::string_view client_name,
    std::string_view messages_name)
{
  auto deployment = state.GetDeployment(deployment_id);
  if (!deployment) {
    return ErrorResponse(http::status::not_found, "deployment not found.");
  }

  auto result = state.DeleteClientMessagesResource(deployment_id, client_name,
                                                   messages_name);
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
  json_object_set_new(root.get(), "client_name",
                      json_string(std::string{client_name}.c_str()));
  json_object_set_new(root.get(), "messages_name",
                      json_string(std::string{messages_name}.c_str()));
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
      .address = spec->address,
      .port = spec->port,
      .allowed_script_dirs = spec->allowed_script_dirs,
      .allowed_job_commands = spec->allowed_job_commands,
      .tls_authenticate = spec->tls_authenticate,
      .tls_enable = spec->tls_enable,
      .tls_require = spec->tls_require,
      .tls_verify_peer = spec->tls_verify_peer,
      .tls_cipher_list = spec->tls_cipher_list,
      .tls_cipher_suites = spec->tls_cipher_suites,
      .tls_dh_file = spec->tls_dh_file,
      .tls_protocol = spec->tls_protocol,
      .tls_ca_certificate_file = spec->tls_ca_certificate_file,
      .tls_ca_certificate_dir = spec->tls_ca_certificate_dir,
      .tls_certificate_revocation_list = spec->tls_certificate_revocation_list,
      .tls_certificate = spec->tls_certificate,
      .tls_key = spec->tls_key,
      .tls_allowed_cn = spec->tls_allowed_cn,
      .connection_from_director_to_client
      = spec->connection_from_director_to_client,
      .connection_from_client_to_director
      = spec->connection_from_client_to_director,
      .monitor = spec->monitor,
      .maximum_bandwidth_per_job = spec->maximum_bandwidth_per_job,
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

http::response<http::string_body>
HandleDeploymentClientDirectorStubDeleteRequest(ServiceState& state,
                                                std::string_view deployment_id,
                                                std::string_view client_name,
                                                std::string_view director_name)
{
  auto deployment = state.GetDeployment(deployment_id);
  if (!deployment) {
    return ErrorResponse(http::status::not_found, "deployment not found.");
  }

  auto result = state.DeleteClientDirectorStub(deployment_id, client_name,
                                               director_name);
  if (!result) {
    return ErrorResponse(http::status::bad_request, result.error);
  }

  auto root = MakeJson(json_object());
  auto deployment_json = MakeJson(json_array());
  AppendDeployment(deployment_json.get(), *deployment);
  json_object_set(root.get(), "deployment",
                  json_array_get(deployment_json.get(), 0));
  json_object_set_new(root.get(), "client_name",
                      json_string(std::string{client_name}.c_str()));
  json_object_set_new(root.get(), "director_name",
                      json_string(std::string{director_name}.c_str()));
  json_object_set_new(root.get(), "deleted", json_true());
  json_object_set_new(root.get(), "client_removed",
                      result.value->has_value() ? json_false() : json_true());

  if (result.value->has_value()) {
    bool parser_initialized = false;
    auto client_json
        = BuildDeploymentConfigDocument(**result.value, parser_initialized);
    if (!parser_initialized) {
      return JsonResponse(http::status::bad_request,
                          DumpJson(client_json.get()));
    }
    json_object_set(root.get(), "client", client_json.get());
  } else {
    json_object_set_new(root.get(), "client", json_null());
  }

  return JsonResponse(http::status::ok, DumpJson(root.get()));
}

http::response<http::string_body> HandleDeploymentStorageDaemonPutRequest(
    ServiceState& state,
    const http::request<http::string_body>& request,
    std::string_view deployment_id,
    std::string_view storage_name)
{
  auto deployment = state.GetDeployment(deployment_id);
  if (!deployment) {
    return ErrorResponse(http::status::not_found, "deployment not found.");
  }

  std::string error;
  auto spec = ParseStorageDaemonRequest(request.body(), error);
  if (!spec) { return ErrorResponse(http::status::bad_request, error); }

  StorageDaemonResourceSpec resource_spec{
      .address = spec->address,
      .addresses = spec->addresses,
      .source_address = spec->source_address,
      .source_addresses = spec->source_addresses,
      .port = spec->port,
      .just_in_time_reservation = spec->just_in_time_reservation,
      .maximum_concurrent_jobs = spec->maximum_concurrent_jobs,
      .absolute_job_timeout = spec->absolute_job_timeout,
      .allow_bandwidth_bursting = spec->allow_bandwidth_bursting,
      .tls_authenticate = spec->tls_authenticate,
      .tls_enable = spec->tls_enable,
      .tls_require = spec->tls_require,
      .tls_verify_peer = spec->tls_verify_peer,
      .tls_cipher_list = spec->tls_cipher_list,
      .tls_cipher_suites = spec->tls_cipher_suites,
      .tls_dh_file = spec->tls_dh_file,
      .tls_protocol = spec->tls_protocol,
      .tls_ca_certificate_file = spec->tls_ca_certificate_file,
      .tls_ca_certificate_dir = spec->tls_ca_certificate_dir,
      .tls_certificate_revocation_list = spec->tls_certificate_revocation_list,
      .tls_certificate = spec->tls_certificate,
      .tls_key = spec->tls_key,
      .tls_allowed_cn = spec->tls_allowed_cn,
      .ndmp_enable = spec->ndmp_enable,
      .ndmp_snooping = spec->ndmp_snooping,
      .ndmp_log_level = spec->ndmp_log_level,
      .ndmp_address = spec->ndmp_address,
      .ndmp_port = spec->ndmp_port,
      .ndmp_addresses = spec->ndmp_addresses,
      .autoxflate_on_replication = spec->autoxflate_on_replication,
      .collect_device_statistics = spec->collect_device_statistics,
      .collect_job_statistics = spec->collect_job_statistics,
      .statistics_collect_interval = spec->statistics_collect_interval,
      .device_reserve_by_media_type = spec->device_reserve_by_media_type,
      .file_device_concurrent_read = spec->file_device_concurrent_read,
      .ver_id = spec->ver_id,
      .log_timestamp_format = spec->log_timestamp_format,
      .maximum_bandwidth_per_job = spec->maximum_bandwidth_per_job,
      .secure_erase_command = spec->secure_erase_command,
      .enable_ktls = spec->enable_ktls,
      .sd_connect_timeout = spec->sd_connect_timeout,
      .fd_connect_timeout = spec->fd_connect_timeout,
      .heartbeat_interval = spec->heartbeat_interval,
      .checkpoint_interval = spec->checkpoint_interval,
      .client_connect_wait = spec->client_connect_wait,
      .maximum_network_buffer_size = spec->maximum_network_buffer_size,
      .description = spec->description,
      .working_directory = spec->working_directory,
      .plugin_directory = spec->plugin_directory,
      .plugin_names = spec->plugin_names,
#if defined(HAVE_DYNAMIC_SD_BACKENDS)
      .backend_directories = spec->backend_directories,
#endif
      .scripts_directory = spec->scripts_directory,
      .messages = spec->messages,
  };
  auto result = state.UpsertStorageDaemonResource(deployment_id, storage_name,
                                                  resource_spec);
  if (!result) {
    return ErrorResponse(http::status::bad_request, result.error);
  }

  bool parser_initialized = false;
  auto storage_json
      = BuildDeploymentConfigDocument(*result.value, parser_initialized);
  if (!parser_initialized) {
    return JsonResponse(http::status::bad_request,
                        DumpJson(storage_json.get()));
  }

  auto root = MakeJson(json_object());
  auto deployment_json = MakeJson(json_array());
  AppendDeployment(deployment_json.get(), *deployment);
  json_object_set(root.get(), "deployment",
                  json_array_get(deployment_json.get(), 0));
  json_object_set_new(root.get(), "storage_name",
                      json_string(std::string{storage_name}.c_str()));
  json_object_set(root.get(), "storage", storage_json.get());
  return JsonResponse(http::status::ok, DumpJson(root.get()));
}

http::response<http::string_body> HandleDeploymentStorageMessagesPutRequest(
    ServiceState& state,
    const http::request<http::string_body>& request,
    std::string_view deployment_id,
    std::string_view storage_name,
    std::string_view messages_name)
{
  auto deployment = state.GetDeployment(deployment_id);
  if (!deployment) {
    return ErrorResponse(http::status::not_found, "deployment not found.");
  }

  std::string error;
  auto spec = ParseDirectorMessagesRequest(request.body(), error);
  if (!spec) { return ErrorResponse(http::status::bad_request, error); }

  StorageMessagesResourceSpec resource_spec{
      .description = spec->description,
      .entries = spec->entries,
  };
  auto result = state.UpsertStorageMessagesResource(
      deployment_id, storage_name, messages_name, resource_spec);
  if (!result) {
    return ErrorResponse(http::status::bad_request, result.error);
  }

  bool parser_initialized = false;
  auto storage_json
      = BuildDeploymentConfigDocument(*result.value, parser_initialized);
  if (!parser_initialized) {
    return JsonResponse(http::status::bad_request,
                        DumpJson(storage_json.get()));
  }

  auto root = MakeJson(json_object());
  auto deployment_json = MakeJson(json_array());
  AppendDeployment(deployment_json.get(), *deployment);
  json_object_set(root.get(), "deployment",
                  json_array_get(deployment_json.get(), 0));
  json_object_set_new(root.get(), "storage_name",
                      json_string(std::string{storage_name}.c_str()));
  json_object_set_new(root.get(), "messages_name",
                      json_string(std::string{messages_name}.c_str()));
  json_object_set(root.get(), "storage", storage_json.get());
  return JsonResponse(http::status::ok, DumpJson(root.get()));
}

http::response<http::string_body> HandleDeploymentStorageDirectorPutRequest(
    ServiceState& state,
    const http::request<http::string_body>& request,
    std::string_view deployment_id,
    std::string_view storage_name,
    std::string_view director_name)
{
  auto deployment = state.GetDeployment(deployment_id);
  if (!deployment) {
    return ErrorResponse(http::status::not_found, "deployment not found.");
  }

  std::string error;
  auto spec = ParseStorageDirectorRequest(request.body(), error);
  if (!spec) { return ErrorResponse(http::status::bad_request, error); }

  StorageDirectorResourceSpec resource_spec{
      .password = spec->password,
      .description = spec->description,
      .monitor = spec->monitor,
      .maximum_bandwidth_per_job = spec->maximum_bandwidth_per_job,
  };
  auto result = state.UpsertStorageDirectorResource(
      deployment_id, storage_name, director_name, resource_spec);
  if (!result) {
    return ErrorResponse(http::status::bad_request, result.error);
  }

  bool parser_initialized = false;
  auto storage_json
      = BuildDeploymentConfigDocument(*result.value, parser_initialized);
  if (!parser_initialized) {
    return JsonResponse(http::status::bad_request,
                        DumpJson(storage_json.get()));
  }

  auto root = MakeJson(json_object());
  auto deployment_json = MakeJson(json_array());
  AppendDeployment(deployment_json.get(), *deployment);
  json_object_set(root.get(), "deployment",
                  json_array_get(deployment_json.get(), 0));
  json_object_set_new(root.get(), "storage_name",
                      json_string(std::string{storage_name}.c_str()));
  json_object_set_new(root.get(), "director_name",
                      json_string(std::string{director_name}.c_str()));
  json_object_set(root.get(), "storage", storage_json.get());
  return JsonResponse(http::status::ok, DumpJson(root.get()));
}

http::response<http::string_body> HandleDeploymentStorageDirectorDeleteRequest(
    ServiceState& state,
    std::string_view deployment_id,
    std::string_view storage_name,
    std::string_view director_name)
{
  auto deployment = state.GetDeployment(deployment_id);
  if (!deployment) {
    return ErrorResponse(http::status::not_found, "deployment not found.");
  }

  auto result = state.DeleteStorageDirectorResource(deployment_id, storage_name,
                                                    director_name);
  if (!result) {
    return ErrorResponse(http::status::bad_request, result.error);
  }

  bool parser_initialized = false;
  auto storage_json
      = BuildDeploymentConfigDocument(*result.value, parser_initialized);
  if (!parser_initialized) {
    return JsonResponse(http::status::bad_request,
                        DumpJson(storage_json.get()));
  }

  auto root = MakeJson(json_object());
  auto deployment_json = MakeJson(json_array());
  AppendDeployment(deployment_json.get(), *deployment);
  json_object_set(root.get(), "deployment",
                  json_array_get(deployment_json.get(), 0));
  json_object_set_new(root.get(), "storage_name",
                      json_string(std::string{storage_name}.c_str()));
  json_object_set_new(root.get(), "director_name",
                      json_string(std::string{director_name}.c_str()));
  json_object_set(root.get(), "storage", storage_json.get());
  return JsonResponse(http::status::ok, DumpJson(root.get()));
}

http::response<http::string_body> HandleDeploymentStorageDevicePutRequest(
    ServiceState& state,
    const http::request<http::string_body>& request,
    std::string_view deployment_id,
    std::string_view storage_name,
    std::string_view device_name)
{
  auto deployment = state.GetDeployment(deployment_id);
  if (!deployment) {
    return ErrorResponse(http::status::not_found, "deployment not found.");
  }

  std::string error;
  auto spec = ParseStorageDeviceRequest(request.body(), error);
  if (!spec) { return ErrorResponse(http::status::bad_request, error); }

  StorageDeviceResourceSpec resource_spec{
      .media_type = spec->media_type,
      .archive_device = spec->archive_device,
      .device_type = spec->device_type,
      .description = spec->description,
  };
  auto result = state.UpsertStorageDeviceResource(deployment_id, storage_name,
                                                  device_name, resource_spec);
  if (!result) {
    return ErrorResponse(http::status::bad_request, result.error);
  }

  bool parser_initialized = false;
  auto storage_json
      = BuildDeploymentConfigDocument(*result.value, parser_initialized);
  if (!parser_initialized) {
    return JsonResponse(http::status::bad_request,
                        DumpJson(storage_json.get()));
  }

  auto root = MakeJson(json_object());
  auto deployment_json = MakeJson(json_array());
  AppendDeployment(deployment_json.get(), *deployment);
  json_object_set(root.get(), "deployment",
                  json_array_get(deployment_json.get(), 0));
  json_object_set_new(root.get(), "storage_name",
                      json_string(std::string{storage_name}.c_str()));
  json_object_set_new(root.get(), "device_name",
                      json_string(std::string{device_name}.c_str()));
  json_object_set(root.get(), "storage", storage_json.get());
  return JsonResponse(http::status::ok, DumpJson(root.get()));
}

http::response<http::string_body> HandleDeploymentStorageDeviceDeleteRequest(
    ServiceState& state,
    std::string_view deployment_id,
    std::string_view storage_name,
    std::string_view device_name)
{
  auto deployment = state.GetDeployment(deployment_id);
  if (!deployment) {
    return ErrorResponse(http::status::not_found, "deployment not found.");
  }

  auto result = state.DeleteStorageDeviceResource(deployment_id, storage_name,
                                                  device_name);
  if (!result) {
    return ErrorResponse(http::status::bad_request, result.error);
  }

  bool parser_initialized = false;
  auto storage_json
      = BuildDeploymentConfigDocument(*result.value, parser_initialized);
  if (!parser_initialized) {
    return JsonResponse(http::status::bad_request,
                        DumpJson(storage_json.get()));
  }

  auto root = MakeJson(json_object());
  auto deployment_json = MakeJson(json_array());
  AppendDeployment(deployment_json.get(), *deployment);
  json_object_set(root.get(), "deployment",
                  json_array_get(deployment_json.get(), 0));
  json_object_set_new(root.get(), "storage_name",
                      json_string(std::string{storage_name}.c_str()));
  json_object_set_new(root.get(), "device_name",
                      json_string(std::string{device_name}.c_str()));
  json_object_set(root.get(), "storage", storage_json.get());
  return JsonResponse(http::status::ok, DumpJson(root.get()));
}

http::response<http::string_body> HandleDeploymentStorageNdmpPutRequest(
    ServiceState& state,
    const http::request<http::string_body>& request,
    std::string_view deployment_id,
    std::string_view storage_name,
    std::string_view ndmp_name)
{
  auto deployment = state.GetDeployment(deployment_id);
  if (!deployment) {
    return ErrorResponse(http::status::not_found, "deployment not found.");
  }

  std::string error;
  auto spec = ParseStorageNdmpRequest(request.body(), error);
  if (!spec) { return ErrorResponse(http::status::bad_request, error); }

  StorageNdmpResourceSpec resource_spec{
      .username = spec->username,
      .password = spec->password,
      .auth_type = spec->auth_type,
      .log_level = spec->log_level,
  };
  auto result = state.UpsertStorageNdmpResource(deployment_id, storage_name,
                                                ndmp_name, resource_spec);
  if (!result) {
    return ErrorResponse(http::status::bad_request, result.error);
  }

  bool parser_initialized = false;
  auto storage_json
      = BuildDeploymentConfigDocument(*result.value, parser_initialized);
  if (!parser_initialized) {
    return JsonResponse(http::status::bad_request,
                        DumpJson(storage_json.get()));
  }

  auto root = MakeJson(json_object());
  auto deployment_json = MakeJson(json_array());
  AppendDeployment(deployment_json.get(), *deployment);
  json_object_set(root.get(), "deployment",
                  json_array_get(deployment_json.get(), 0));
  json_object_set_new(root.get(), "storage_name",
                      json_string(std::string{storage_name}.c_str()));
  json_object_set_new(root.get(), "ndmp_name",
                      json_string(std::string{ndmp_name}.c_str()));
  json_object_set(root.get(), "storage", storage_json.get());
  return JsonResponse(http::status::ok, DumpJson(root.get()));
}

http::response<http::string_body> HandleDeploymentStorageNdmpDeleteRequest(
    ServiceState& state,
    std::string_view deployment_id,
    std::string_view storage_name,
    std::string_view ndmp_name)
{
  auto deployment = state.GetDeployment(deployment_id);
  if (!deployment) {
    return ErrorResponse(http::status::not_found, "deployment not found.");
  }

  auto result
      = state.DeleteStorageNdmpResource(deployment_id, storage_name, ndmp_name);
  if (!result) {
    return ErrorResponse(http::status::bad_request, result.error);
  }

  bool parser_initialized = false;
  auto storage_json
      = BuildDeploymentConfigDocument(*result.value, parser_initialized);
  if (!parser_initialized) {
    return JsonResponse(http::status::bad_request,
                        DumpJson(storage_json.get()));
  }

  auto root = MakeJson(json_object());
  auto deployment_json = MakeJson(json_array());
  AppendDeployment(deployment_json.get(), *deployment);
  json_object_set(root.get(), "deployment",
                  json_array_get(deployment_json.get(), 0));
  json_object_set_new(root.get(), "storage_name",
                      json_string(std::string{storage_name}.c_str()));
  json_object_set_new(root.get(), "ndmp_name",
                      json_string(std::string{ndmp_name}.c_str()));
  json_object_set(root.get(), "storage", storage_json.get());
  return JsonResponse(http::status::ok, DumpJson(root.get()));
}

http::response<http::string_body> HandleDeploymentStorageAutochangerPutRequest(
    ServiceState& state,
    const http::request<http::string_body>& request,
    std::string_view deployment_id,
    std::string_view storage_name,
    std::string_view autochanger_name)
{
  auto deployment = state.GetDeployment(deployment_id);
  if (!deployment) {
    return ErrorResponse(http::status::not_found, "deployment not found.");
  }

  std::string error;
  auto spec = ParseStorageAutochangerRequest(request.body(), error);
  if (!spec) { return ErrorResponse(http::status::bad_request, error); }

  StorageAutochangerResourceSpec resource_spec{
      .devices = spec->devices,
      .changer_device = spec->changer_device,
      .changer_command = spec->changer_command,
      .description = spec->description,
  };
  auto result = state.UpsertStorageAutochangerResource(
      deployment_id, storage_name, autochanger_name, resource_spec);
  if (!result) {
    return ErrorResponse(http::status::bad_request, result.error);
  }

  bool parser_initialized = false;
  auto storage_json
      = BuildDeploymentConfigDocument(*result.value, parser_initialized);
  if (!parser_initialized) {
    return JsonResponse(http::status::bad_request,
                        DumpJson(storage_json.get()));
  }

  auto root = MakeJson(json_object());
  auto deployment_json = MakeJson(json_array());
  AppendDeployment(deployment_json.get(), *deployment);
  json_object_set(root.get(), "deployment",
                  json_array_get(deployment_json.get(), 0));
  json_object_set_new(root.get(), "storage_name",
                      json_string(std::string{storage_name}.c_str()));
  json_object_set_new(root.get(), "autochanger_name",
                      json_string(std::string{autochanger_name}.c_str()));
  json_object_set(root.get(), "storage", storage_json.get());
  return JsonResponse(http::status::ok, DumpJson(root.get()));
}

http::response<http::string_body>
HandleDeploymentStorageAutochangerDeleteRequest(
    ServiceState& state,
    std::string_view deployment_id,
    std::string_view storage_name,
    std::string_view autochanger_name)
{
  auto deployment = state.GetDeployment(deployment_id);
  if (!deployment) {
    return ErrorResponse(http::status::not_found, "deployment not found.");
  }

  auto result = state.DeleteStorageAutochangerResource(
      deployment_id, storage_name, autochanger_name);
  if (!result) {
    return ErrorResponse(http::status::bad_request, result.error);
  }

  bool parser_initialized = false;
  auto storage_json
      = BuildDeploymentConfigDocument(*result.value, parser_initialized);
  if (!parser_initialized) {
    return JsonResponse(http::status::bad_request,
                        DumpJson(storage_json.get()));
  }

  auto root = MakeJson(json_object());
  auto deployment_json = MakeJson(json_array());
  AppendDeployment(deployment_json.get(), *deployment);
  json_object_set(root.get(), "deployment",
                  json_array_get(deployment_json.get(), 0));
  json_object_set_new(root.get(), "storage_name",
                      json_string(std::string{storage_name}.c_str()));
  json_object_set_new(root.get(), "autochanger_name",
                      json_string(std::string{autochanger_name}.c_str()));
  json_object_set(root.get(), "storage", storage_json.get());
  return JsonResponse(http::status::ok, DumpJson(root.get()));
}

http::response<http::string_body> HandleDeploymentStorageMessagesDeleteRequest(
    ServiceState& state,
    std::string_view deployment_id,
    std::string_view storage_name,
    std::string_view messages_name)
{
  auto deployment = state.GetDeployment(deployment_id);
  if (!deployment) {
    return ErrorResponse(http::status::not_found, "deployment not found.");
  }

  auto result = state.DeleteStorageMessagesResource(deployment_id, storage_name,
                                                    messages_name);
  if (!result) {
    return ErrorResponse(http::status::bad_request, result.error);
  }

  bool parser_initialized = false;
  auto storage_json
      = BuildDeploymentConfigDocument(*result.value, parser_initialized);
  if (!parser_initialized) {
    return JsonResponse(http::status::bad_request,
                        DumpJson(storage_json.get()));
  }

  auto root = MakeJson(json_object());
  auto deployment_json = MakeJson(json_array());
  AppendDeployment(deployment_json.get(), *deployment);
  json_object_set(root.get(), "deployment",
                  json_array_get(deployment_json.get(), 0));
  json_object_set_new(root.get(), "storage_name",
                      json_string(std::string{storage_name}.c_str()));
  json_object_set_new(root.get(), "messages_name",
                      json_string(std::string{messages_name}.c_str()));
  json_object_set(root.get(), "storage", storage_json.get());
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
      .enabled = spec->enabled,
      .passive = spec->passive,
      .connection_from_director_to_client
      = spec->connection_from_director_to_client,
      .connection_from_client_to_director
      = spec->connection_from_client_to_director,
      .heartbeat_interval = spec->heartbeat_interval,
      .maximum_bandwidth_per_job = spec->maximum_bandwidth_per_job,
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
      .enabled = spec->enabled,
      .allow_compression = spec->allow_compression,
      .heartbeat_interval = spec->heartbeat_interval,
      .maximum_bandwidth_per_job = spec->maximum_bandwidth_per_job,
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

http::response<http::string_body> HandleDeploymentConsoleConsolePutRequest(
    ServiceState& state,
    const http::request<http::string_body>& request,
    std::string_view deployment_id,
    std::string_view console_config_name,
    std::string_view console_name)
{
  auto deployment = state.GetDeployment(deployment_id);
  if (!deployment) {
    return ErrorResponse(http::status::not_found, "deployment not found.");
  }

  std::string error;
  auto spec = ParseConsoleConsoleRequest(request.body(), error);
  if (!spec) { return ErrorResponse(http::status::bad_request, error); }

  ConsoleConsoleResourceSpec resource_spec{
      .director = spec->director,
      .password = spec->password,
      .description = spec->description,
      .rc_file = spec->rc_file,
      .history_file = spec->history_file,
      .history_length = spec->history_length,
      .heartbeat_interval = spec->heartbeat_interval,
      .tls_authenticate = spec->tls_authenticate,
      .tls_enable = spec->tls_enable,
      .tls_require = spec->tls_require,
      .tls_verify_peer = spec->tls_verify_peer,
      .tls_cipher_list = spec->tls_cipher_list,
      .tls_cipher_suites = spec->tls_cipher_suites,
      .tls_dh_file = spec->tls_dh_file,
      .tls_protocol = spec->tls_protocol,
      .tls_ca_certificate_file = spec->tls_ca_certificate_file,
      .tls_ca_certificate_dir = spec->tls_ca_certificate_dir,
      .tls_certificate_revocation_list = spec->tls_certificate_revocation_list,
      .tls_certificate = spec->tls_certificate,
      .tls_key = spec->tls_key,
      .tls_allowed_cn = spec->tls_allowed_cn,
  };
  auto result = state.UpsertConsoleConsoleResource(
      deployment_id, console_config_name, console_name, resource_spec);
  if (!result) {
    return ErrorResponse(http::status::bad_request, result.error);
  }

  bool parser_initialized = false;
  auto console_json
      = BuildDeploymentConfigDocument(*result.value, parser_initialized);
  if (!parser_initialized) {
    return JsonResponse(http::status::bad_request,
                        DumpJson(console_json.get()));
  }

  auto root = MakeJson(json_object());
  auto deployment_json = MakeJson(json_array());
  AppendDeployment(deployment_json.get(), *deployment);
  json_object_set(root.get(), "deployment",
                  json_array_get(deployment_json.get(), 0));
  json_object_set_new(root.get(), "console_config_name",
                      json_string(std::string{console_config_name}.c_str()));
  json_object_set_new(root.get(), "console_name",
                      json_string(std::string{console_name}.c_str()));
  json_object_set(root.get(), "console", console_json.get());
  return JsonResponse(http::status::ok, DumpJson(root.get()));
}

http::response<http::string_body> HandleDeploymentConsoleConsoleDeleteRequest(
    ServiceState& state,
    std::string_view deployment_id,
    std::string_view console_config_name,
    std::string_view console_name)
{
  auto deployment = state.GetDeployment(deployment_id);
  if (!deployment) {
    return ErrorResponse(http::status::not_found, "deployment not found.");
  }

  auto result = state.DeleteConsoleConsoleResource(
      deployment_id, console_config_name, console_name);
  if (!result) {
    return ErrorResponse(http::status::bad_request, result.error);
  }

  bool parser_initialized = false;
  auto console_json
      = BuildDeploymentConfigDocument(*result.value, parser_initialized);
  if (!parser_initialized) {
    return JsonResponse(http::status::bad_request,
                        DumpJson(console_json.get()));
  }

  auto root = MakeJson(json_object());
  auto deployment_json = MakeJson(json_array());
  AppendDeployment(deployment_json.get(), *deployment);
  json_object_set(root.get(), "deployment",
                  json_array_get(deployment_json.get(), 0));
  json_object_set_new(root.get(), "console_config_name",
                      json_string(std::string{console_config_name}.c_str()));
  json_object_set_new(root.get(), "console_name",
                      json_string(std::string{console_name}.c_str()));
  json_object_set(root.get(), "console", console_json.get());
  return JsonResponse(http::status::ok, DumpJson(root.get()));
}

http::response<http::string_body> HandleDeploymentConsoleDirectorPutRequest(
    ServiceState& state,
    const http::request<http::string_body>& request,
    std::string_view deployment_id,
    std::string_view console_config_name,
    std::string_view director_name)
{
  auto deployment = state.GetDeployment(deployment_id);
  if (!deployment) {
    return ErrorResponse(http::status::not_found, "deployment not found.");
  }

  std::string error;
  auto spec = ParseConsoleDirectorRequest(request.body(), error);
  if (!spec) { return ErrorResponse(http::status::bad_request, error); }

  ConsoleDirectorResourceSpec resource_spec{
      .address = spec->address,
      .port = spec->port,
      .password = spec->password,
      .description = spec->description,
      .heartbeat_interval = spec->heartbeat_interval,
      .tls_authenticate = spec->tls_authenticate,
      .tls_enable = spec->tls_enable,
      .tls_require = spec->tls_require,
      .tls_verify_peer = spec->tls_verify_peer,
      .tls_cipher_list = spec->tls_cipher_list,
      .tls_cipher_suites = spec->tls_cipher_suites,
      .tls_dh_file = spec->tls_dh_file,
      .tls_protocol = spec->tls_protocol,
      .tls_ca_certificate_file = spec->tls_ca_certificate_file,
      .tls_ca_certificate_dir = spec->tls_ca_certificate_dir,
      .tls_certificate_revocation_list = spec->tls_certificate_revocation_list,
      .tls_certificate = spec->tls_certificate,
      .tls_key = spec->tls_key,
      .tls_allowed_cn = spec->tls_allowed_cn,
  };
  auto result = state.UpsertConsoleDirectorResource(
      deployment_id, console_config_name, director_name, resource_spec);
  if (!result) {
    return ErrorResponse(http::status::bad_request, result.error);
  }

  bool parser_initialized = false;
  auto console_json
      = BuildDeploymentConfigDocument(*result.value, parser_initialized);
  if (!parser_initialized) {
    return JsonResponse(http::status::bad_request,
                        DumpJson(console_json.get()));
  }

  auto root = MakeJson(json_object());
  auto deployment_json = MakeJson(json_array());
  AppendDeployment(deployment_json.get(), *deployment);
  json_object_set(root.get(), "deployment",
                  json_array_get(deployment_json.get(), 0));
  json_object_set_new(root.get(), "console_config_name",
                      json_string(std::string{console_config_name}.c_str()));
  json_object_set_new(root.get(), "director_name",
                      json_string(std::string{director_name}.c_str()));
  json_object_set(root.get(), "director", console_json.get());
  return JsonResponse(http::status::ok, DumpJson(root.get()));
}

http::response<http::string_body> HandleDeploymentConsoleDirectorDeleteRequest(
    ServiceState& state,
    std::string_view deployment_id,
    std::string_view console_config_name,
    std::string_view director_name)
{
  auto deployment = state.GetDeployment(deployment_id);
  if (!deployment) {
    return ErrorResponse(http::status::not_found, "deployment not found.");
  }

  auto result = state.DeleteConsoleDirectorResource(
      deployment_id, console_config_name, director_name);
  if (!result) {
    return ErrorResponse(http::status::bad_request, result.error);
  }

  bool parser_initialized = false;
  auto console_json
      = BuildDeploymentConfigDocument(*result.value, parser_initialized);
  if (!parser_initialized) {
    return JsonResponse(http::status::bad_request,
                        DumpJson(console_json.get()));
  }

  auto root = MakeJson(json_object());
  auto deployment_json = MakeJson(json_array());
  AppendDeployment(deployment_json.get(), *deployment);
  json_object_set(root.get(), "deployment",
                  json_array_get(deployment_json.get(), 0));
  json_object_set_new(root.get(), "console_config_name",
                      json_string(std::string{console_config_name}.c_str()));
  json_object_set_new(root.get(), "director_name",
                      json_string(std::string{director_name}.c_str()));
  json_object_set(root.get(), "director", console_json.get());
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

http::response<http::string_body> HandleDeploymentDirectorMessagesPutRequest(
    ServiceState& state,
    const http::request<http::string_body>& request,
    std::string_view deployment_id,
    std::string_view director_name,
    std::string_view messages_name)
{
  auto deployment = state.GetDeployment(deployment_id);
  if (!deployment) {
    return ErrorResponse(http::status::not_found, "deployment not found.");
  }

  std::string error;
  auto spec = ParseDirectorMessagesRequest(request.body(), error);
  if (!spec) { return ErrorResponse(http::status::bad_request, error); }

  DirectorMessagesResourceSpec resource_spec{
      .description = spec->description,
      .entries = spec->entries,
  };
  auto result = state.UpsertDirectorMessagesResource(
      deployment_id, director_name, messages_name, resource_spec);
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
  json_object_set_new(root.get(), "messages_name",
                      json_string(std::string{messages_name}.c_str()));
  json_object_set(root.get(), "director", director_json.get());
  return JsonResponse(http::status::ok, DumpJson(root.get()));
}

http::response<http::string_body> HandleDeploymentDirectorMessagesDeleteRequest(
    ServiceState& state,
    std::string_view deployment_id,
    std::string_view director_name,
    std::string_view messages_name)
{
  auto deployment = state.GetDeployment(deployment_id);
  if (!deployment) {
    return ErrorResponse(http::status::not_found, "deployment not found.");
  }

  auto result = state.DeleteDirectorMessagesResource(
      deployment_id, director_name, messages_name);
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
  json_object_set_new(root.get(), "messages_name",
                      json_string(std::string{messages_name}.c_str()));
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

http::response<http::string_body> HandleDeploymentDirectorCounterPutRequest(
    ServiceState& state,
    const http::request<http::string_body>& request,
    std::string_view deployment_id,
    std::string_view director_name,
    std::string_view counter_name)
{
  auto deployment = state.GetDeployment(deployment_id);
  if (!deployment) {
    return ErrorResponse(http::status::not_found, "deployment not found.");
  }

  std::string error;
  auto spec = ParseDirectorCounterRequest(request.body(), error);
  if (!spec) { return ErrorResponse(http::status::bad_request, error); }

  DirectorCounterResourceSpec resource_spec{
      .minimum = spec->minimum,
      .maximum = spec->maximum,
      .wrap_counter = spec->wrap_counter,
      .catalog = spec->catalog,
      .description = spec->description,
  };
  auto result = state.UpsertDirectorCounterResource(
      deployment_id, director_name, counter_name, resource_spec);
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
  json_object_set_new(root.get(), "counter_name",
                      json_string(std::string{counter_name}.c_str()));
  json_object_set(root.get(), "director", director_json.get());
  return JsonResponse(http::status::ok, DumpJson(root.get()));
}

http::response<http::string_body> HandleDeploymentDirectorCounterDeleteRequest(
    ServiceState& state,
    std::string_view deployment_id,
    std::string_view director_name,
    std::string_view counter_name)
{
  auto deployment = state.GetDeployment(deployment_id);
  if (!deployment) {
    return ErrorResponse(http::status::not_found, "deployment not found.");
  }

  auto result = state.DeleteDirectorCounterResource(
      deployment_id, director_name, counter_name);
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
  json_object_set_new(root.get(), "counter_name",
                      json_string(std::string{counter_name}.c_str()));
  json_object_set(root.get(), "director", director_json.get());
  return JsonResponse(http::status::ok, DumpJson(root.get()));
}

http::response<http::string_body> HandleDeploymentDirectorFilesetPutRequest(
    ServiceState& state,
    const http::request<http::string_body>& request,
    std::string_view deployment_id,
    std::string_view director_name,
    std::string_view fileset_name)
{
  auto deployment = state.GetDeployment(deployment_id);
  if (!deployment) {
    return ErrorResponse(http::status::not_found, "deployment not found.");
  }

  std::string error;
  auto spec = ParseDirectorFilesetRequest(request.body(), error);
  if (!spec) { return ErrorResponse(http::status::bad_request, error); }

  DirectorFilesetResourceSpec resource_spec{
      .description = spec->description,
      .ignore_fileset_changes = spec->ignore_fileset_changes,
      .enable_vss = spec->enable_vss,
      .include_blocks = spec->include_blocks,
      .exclude_blocks = spec->exclude_blocks,
  };
  auto result = state.UpsertDirectorFilesetResource(
      deployment_id, director_name, fileset_name, resource_spec);
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
  json_object_set_new(root.get(), "fileset_name",
                      json_string(std::string{fileset_name}.c_str()));
  json_object_set(root.get(), "director", director_json.get());
  return JsonResponse(http::status::ok, DumpJson(root.get()));
}

http::response<http::string_body> HandleDeploymentDirectorFilesetDeleteRequest(
    ServiceState& state,
    std::string_view deployment_id,
    std::string_view director_name,
    std::string_view fileset_name)
{
  auto deployment = state.GetDeployment(deployment_id);
  if (!deployment) {
    return ErrorResponse(http::status::not_found, "deployment not found.");
  }

  auto result = state.DeleteDirectorFilesetResource(
      deployment_id, director_name, fileset_name);
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
  json_object_set_new(root.get(), "fileset_name",
                      json_string(std::string{fileset_name}.c_str()));
  json_object_set(root.get(), "director", director_json.get());
  return JsonResponse(http::status::ok, DumpJson(root.get()));
}

http::response<http::string_body> HandleDeploymentDirectorJobPutRequest(
    ServiceState& state,
    const http::request<http::string_body>& request,
    std::string_view deployment_id,
    std::string_view director_name,
    std::string_view job_name)
{
  auto deployment = state.GetDeployment(deployment_id);
  if (!deployment) {
    return ErrorResponse(http::status::not_found, "deployment not found.");
  }

  std::string error;
  auto spec = ParseDirectorJobRequest(request.body(), error);
  if (!spec) { return ErrorResponse(http::status::bad_request, error); }

  DirectorJobResourceSpec resource_spec{
      .description = spec->description,
      .type = spec->type,
      .level = spec->level,
      .messages = spec->messages,
      .storages = spec->storages,
      .pool = spec->pool,
      .full_backup_pool = spec->full_backup_pool,
      .virtual_full_backup_pool = spec->virtual_full_backup_pool,
      .incremental_backup_pool = spec->incremental_backup_pool,
      .differential_backup_pool = spec->differential_backup_pool,
      .next_pool = spec->next_pool,
      .client = spec->client,
      .fileset = spec->fileset,
      .schedule = spec->schedule,
      .verify_job = spec->verify_job,
      .catalog = spec->catalog,
      .jobdefs = spec->jobdefs,
      .where = spec->where,
      .priority = spec->priority,
      .enabled = spec->enabled,
  };
  auto result = state.UpsertDirectorJobResource(deployment_id, director_name,
                                                job_name, resource_spec);
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
  json_object_set_new(root.get(), "job_name",
                      json_string(std::string{job_name}.c_str()));
  json_object_set(root.get(), "director", director_json.get());
  return JsonResponse(http::status::ok, DumpJson(root.get()));
}

http::response<http::string_body> HandleDeploymentDirectorJobDeleteRequest(
    ServiceState& state,
    std::string_view deployment_id,
    std::string_view director_name,
    std::string_view job_name)
{
  auto deployment = state.GetDeployment(deployment_id);
  if (!deployment) {
    return ErrorResponse(http::status::not_found, "deployment not found.");
  }

  auto result
      = state.DeleteDirectorJobResource(deployment_id, director_name, job_name);
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
  json_object_set_new(root.get(), "job_name",
                      json_string(std::string{job_name}.c_str()));
  json_object_set(root.get(), "director", director_json.get());
  return JsonResponse(http::status::ok, DumpJson(root.get()));
}

http::response<http::string_body> HandleDeploymentDirectorJobDefsPutRequest(
    ServiceState& state,
    const http::request<http::string_body>& request,
    std::string_view deployment_id,
    std::string_view director_name,
    std::string_view jobdefs_name)
{
  auto deployment = state.GetDeployment(deployment_id);
  if (!deployment) {
    return ErrorResponse(http::status::not_found, "deployment not found.");
  }

  std::string error;
  auto spec = ParseDirectorJobRequest(request.body(), error);
  if (!spec) { return ErrorResponse(http::status::bad_request, error); }

  DirectorJobDefsResourceSpec resource_spec{
      .description = spec->description,
      .type = spec->type,
      .level = spec->level,
      .messages = spec->messages,
      .storages = spec->storages,
      .pool = spec->pool,
      .full_backup_pool = spec->full_backup_pool,
      .virtual_full_backup_pool = spec->virtual_full_backup_pool,
      .incremental_backup_pool = spec->incremental_backup_pool,
      .differential_backup_pool = spec->differential_backup_pool,
      .next_pool = spec->next_pool,
      .client = spec->client,
      .fileset = spec->fileset,
      .schedule = spec->schedule,
      .verify_job = spec->verify_job,
      .catalog = spec->catalog,
      .jobdefs = spec->jobdefs,
      .where = spec->where,
      .priority = spec->priority,
      .enabled = spec->enabled,
  };
  auto result = state.UpsertDirectorJobDefsResource(
      deployment_id, director_name, jobdefs_name, resource_spec);
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
  json_object_set_new(root.get(), "jobdefs_name",
                      json_string(std::string{jobdefs_name}.c_str()));
  json_object_set(root.get(), "director", director_json.get());
  return JsonResponse(http::status::ok, DumpJson(root.get()));
}

http::response<http::string_body> HandleDeploymentDirectorJobDefsDeleteRequest(
    ServiceState& state,
    std::string_view deployment_id,
    std::string_view director_name,
    std::string_view jobdefs_name)
{
  auto deployment = state.GetDeployment(deployment_id);
  if (!deployment) {
    return ErrorResponse(http::status::not_found, "deployment not found.");
  }

  auto result = state.DeleteDirectorJobDefsResource(
      deployment_id, director_name, jobdefs_name);
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
  json_object_set_new(root.get(), "jobdefs_name",
                      json_string(std::string{jobdefs_name}.c_str()));
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

std::vector<std::string_view> StripRoutePrefix(
    const std::vector<std::string_view>& path_parts)
{
  for (size_t index = 0; index < path_parts.size(); ++index) {
    if (path_parts[index] == "v1" || path_parts[index] == "ui") {
      return {path_parts.begin() + static_cast<std::ptrdiff_t>(index),
              path_parts.end()};
    }
  }
  return path_parts;
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
  auto* address = json_object_get(root.get(), "address");
  auto* port = json_object_get(root.get(), "port");
  auto* allowed_script_dirs
      = json_object_get(root.get(), "allowed_script_dirs");
  auto* allowed_job_commands
      = json_object_get(root.get(), "allowed_job_commands");
  auto* tls_authenticate = json_object_get(root.get(), "tls_authenticate");
  auto* tls_enable = json_object_get(root.get(), "tls_enable");
  auto* tls_require = json_object_get(root.get(), "tls_require");
  auto* tls_verify_peer = json_object_get(root.get(), "tls_verify_peer");
  auto* tls_cipher_list = json_object_get(root.get(), "tls_cipher_list");
  auto* tls_cipher_suites = json_object_get(root.get(), "tls_cipher_suites");
  auto* tls_dh_file = json_object_get(root.get(), "tls_dh_file");
  auto* tls_protocol = json_object_get(root.get(), "tls_protocol");
  auto* tls_ca_certificate_file
      = json_object_get(root.get(), "tls_ca_certificate_file");
  auto* tls_ca_certificate_dir
      = json_object_get(root.get(), "tls_ca_certificate_dir");
  auto* tls_certificate_revocation_list
      = json_object_get(root.get(), "tls_certificate_revocation_list");
  auto* tls_certificate = json_object_get(root.get(), "tls_certificate");
  auto* tls_key = json_object_get(root.get(), "tls_key");
  auto* tls_allowed_cn = json_object_get(root.get(), "tls_allowed_cn");
  auto* connection_from_director_to_client
      = json_object_get(root.get(), "connection_from_director_to_client");
  auto* connection_from_client_to_director
      = json_object_get(root.get(), "connection_from_client_to_director");
  auto* monitor = json_object_get(root.get(), "monitor");
  auto* maximum_bandwidth_per_job
      = json_object_get(root.get(), "maximum_bandwidth_per_job");
  auto require_string_array = [&error](json_t* value, const char* field) {
    if (!value || json_is_null(value)) { return true; }
    if (!json_is_array(value)) {
      error = std::string{"field '"} + field
              + "' must be an array of strings when provided.";
      return false;
    }
    for (size_t index = 0; index < json_array_size(value); ++index) {
      if (!json_is_string(json_array_get(value, index))) {
        error = std::string{"field '"} + field
                + "' must be an array of strings when provided.";
        return false;
      }
    }
    return true;
  };
  if (description && !json_is_null(description)
      && !json_is_string(description)) {
    error = "field 'description' must be a string when provided.";
    return std::nullopt;
  }
  if (address && !json_is_null(address) && !json_is_string(address)) {
    error = "field 'address' must be a string when provided.";
    return std::nullopt;
  }
  if (port && !json_is_null(port) && !json_is_integer(port)) {
    error = "field 'port' must be an integer when provided.";
    return std::nullopt;
  }
  if (!require_string_array(allowed_script_dirs, "allowed_script_dirs")) {
    return std::nullopt;
  }
  if (!require_string_array(allowed_job_commands, "allowed_job_commands")) {
    return std::nullopt;
  }
  if (tls_authenticate && !json_is_null(tls_authenticate)
      && !json_is_boolean(tls_authenticate)) {
    error = "field 'tls_authenticate' must be a boolean when provided.";
    return std::nullopt;
  }
  if (tls_enable && !json_is_null(tls_enable) && !json_is_boolean(tls_enable)) {
    error = "field 'tls_enable' must be a boolean when provided.";
    return std::nullopt;
  }
  if (tls_require && !json_is_null(tls_require)
      && !json_is_boolean(tls_require)) {
    error = "field 'tls_require' must be a boolean when provided.";
    return std::nullopt;
  }
  if (tls_verify_peer && !json_is_null(tls_verify_peer)
      && !json_is_boolean(tls_verify_peer)) {
    error = "field 'tls_verify_peer' must be a boolean when provided.";
    return std::nullopt;
  }
  if (tls_cipher_list && !json_is_null(tls_cipher_list)
      && !json_is_string(tls_cipher_list)) {
    error = "field 'tls_cipher_list' must be a string when provided.";
    return std::nullopt;
  }
  if (tls_cipher_suites && !json_is_null(tls_cipher_suites)
      && !json_is_string(tls_cipher_suites)) {
    error = "field 'tls_cipher_suites' must be a string when provided.";
    return std::nullopt;
  }
  if (tls_dh_file && !json_is_null(tls_dh_file)
      && !json_is_string(tls_dh_file)) {
    error = "field 'tls_dh_file' must be a string when provided.";
    return std::nullopt;
  }
  if (tls_protocol && !json_is_null(tls_protocol)
      && !json_is_string(tls_protocol)) {
    error = "field 'tls_protocol' must be a string when provided.";
    return std::nullopt;
  }
  if (tls_ca_certificate_file && !json_is_null(tls_ca_certificate_file)
      && !json_is_string(tls_ca_certificate_file)) {
    error = "field 'tls_ca_certificate_file' must be a string when provided.";
    return std::nullopt;
  }
  if (tls_ca_certificate_dir && !json_is_null(tls_ca_certificate_dir)
      && !json_is_string(tls_ca_certificate_dir)) {
    error = "field 'tls_ca_certificate_dir' must be a string when provided.";
    return std::nullopt;
  }
  if (tls_certificate_revocation_list
      && !json_is_null(tls_certificate_revocation_list)
      && !json_is_string(tls_certificate_revocation_list)) {
    error
        = "field 'tls_certificate_revocation_list' must be a string when "
          "provided.";
    return std::nullopt;
  }
  if (tls_certificate && !json_is_null(tls_certificate)
      && !json_is_string(tls_certificate)) {
    error = "field 'tls_certificate' must be a string when provided.";
    return std::nullopt;
  }
  if (tls_key && !json_is_null(tls_key) && !json_is_string(tls_key)) {
    error = "field 'tls_key' must be a string when provided.";
    return std::nullopt;
  }
  if (!require_string_array(tls_allowed_cn, "tls_allowed_cn")) {
    return std::nullopt;
  }
  if (connection_from_director_to_client
      && !json_is_null(connection_from_director_to_client)
      && !json_is_boolean(connection_from_director_to_client)) {
    error
        = "field 'connection_from_director_to_client' must be a boolean when "
          "provided.";
    return std::nullopt;
  }
  if (connection_from_client_to_director
      && !json_is_null(connection_from_client_to_director)
      && !json_is_boolean(connection_from_client_to_director)) {
    error
        = "field 'connection_from_client_to_director' must be a boolean when "
          "provided.";
    return std::nullopt;
  }
  if (monitor && !json_is_null(monitor) && !json_is_boolean(monitor)) {
    error = "field 'monitor' must be a boolean when provided.";
    return std::nullopt;
  }
  if (maximum_bandwidth_per_job && !json_is_null(maximum_bandwidth_per_job)
      && !json_is_integer(maximum_bandwidth_per_job)) {
    error
        = "field 'maximum_bandwidth_per_job' must be an integer when provided.";
    return std::nullopt;
  }

  ClientDirectorStubRequestSpec spec{};
  auto parse_string_array
      = [](json_t* value) -> std::optional<std::vector<std::string>> {
    if (!value || !json_is_array(value)) { return std::nullopt; }
    std::vector<std::string> result;
    result.reserve(json_array_size(value));
    for (size_t index = 0; index < json_array_size(value); ++index) {
      result.emplace_back(json_string_value(json_array_get(value, index)));
    }
    return result;
  };
  if (description && json_is_string(description)) {
    spec.description = std::string{json_string_value(description)};
  }
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
  spec.allowed_script_dirs = parse_string_array(allowed_script_dirs);
  spec.allowed_job_commands = parse_string_array(allowed_job_commands);
  if (tls_authenticate && json_is_boolean(tls_authenticate)) {
    spec.tls_authenticate = json_is_true(tls_authenticate);
  }
  if (tls_enable && json_is_boolean(tls_enable)) {
    spec.tls_enable = json_is_true(tls_enable);
  }
  if (tls_require && json_is_boolean(tls_require)) {
    spec.tls_require = json_is_true(tls_require);
  }
  if (tls_verify_peer && json_is_boolean(tls_verify_peer)) {
    spec.tls_verify_peer = json_is_true(tls_verify_peer);
  }
  if (tls_cipher_list && json_is_string(tls_cipher_list)) {
    spec.tls_cipher_list = std::string{json_string_value(tls_cipher_list)};
  }
  if (tls_cipher_suites && json_is_string(tls_cipher_suites)) {
    spec.tls_cipher_suites = std::string{json_string_value(tls_cipher_suites)};
  }
  if (tls_dh_file && json_is_string(tls_dh_file)) {
    spec.tls_dh_file = std::string{json_string_value(tls_dh_file)};
  }
  if (tls_protocol && json_is_string(tls_protocol)) {
    spec.tls_protocol = std::string{json_string_value(tls_protocol)};
  }
  if (tls_ca_certificate_file && json_is_string(tls_ca_certificate_file)) {
    spec.tls_ca_certificate_file
        = std::string{json_string_value(tls_ca_certificate_file)};
  }
  if (tls_ca_certificate_dir && json_is_string(tls_ca_certificate_dir)) {
    spec.tls_ca_certificate_dir
        = std::string{json_string_value(tls_ca_certificate_dir)};
  }
  if (tls_certificate_revocation_list
      && json_is_string(tls_certificate_revocation_list)) {
    spec.tls_certificate_revocation_list
        = std::string{json_string_value(tls_certificate_revocation_list)};
  }
  if (tls_certificate && json_is_string(tls_certificate)) {
    spec.tls_certificate = std::string{json_string_value(tls_certificate)};
  }
  if (tls_key && json_is_string(tls_key)) {
    spec.tls_key = std::string{json_string_value(tls_key)};
  }
  spec.tls_allowed_cn = parse_string_array(tls_allowed_cn);
  if (connection_from_director_to_client
      && json_is_boolean(connection_from_director_to_client)) {
    spec.connection_from_director_to_client
        = json_is_true(connection_from_director_to_client);
  }
  if (connection_from_client_to_director
      && json_is_boolean(connection_from_client_to_director)) {
    spec.connection_from_client_to_director
        = json_is_true(connection_from_client_to_director);
  }
  if (monitor && json_is_boolean(monitor)) {
    spec.monitor = json_is_true(monitor);
  }
  if (maximum_bandwidth_per_job && json_is_integer(maximum_bandwidth_per_job)) {
    const auto value = json_integer_value(maximum_bandwidth_per_job);
    if (value < 0) {
      error = "field 'maximum_bandwidth_per_job' must be non-negative.";
      return std::nullopt;
    }
    spec.maximum_bandwidth_per_job = static_cast<uint64_t>(value);
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
  auto* enabled = json_object_get(root.get(), "enabled");
  auto* passive = json_object_get(root.get(), "passive");
  auto* connection_from_director_to_client
      = json_object_get(root.get(), "connection_from_director_to_client");
  auto* connection_from_client_to_director
      = json_object_get(root.get(), "connection_from_client_to_director");
  auto* heartbeat_interval = json_object_get(root.get(), "heartbeat_interval");
  auto* maximum_bandwidth_per_job
      = json_object_get(root.get(), "maximum_bandwidth_per_job");
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
  if (enabled && !json_is_null(enabled) && !json_is_boolean(enabled)) {
    error = "field 'enabled' must be a boolean when provided.";
    return std::nullopt;
  }
  if (passive && !json_is_null(passive) && !json_is_boolean(passive)) {
    error = "field 'passive' must be a boolean when provided.";
    return std::nullopt;
  }
  if (connection_from_director_to_client
      && !json_is_null(connection_from_director_to_client)
      && !json_is_boolean(connection_from_director_to_client)) {
    error
        = "field 'connection_from_director_to_client' must be a boolean when "
          "provided.";
    return std::nullopt;
  }
  if (connection_from_client_to_director
      && !json_is_null(connection_from_client_to_director)
      && !json_is_boolean(connection_from_client_to_director)) {
    error
        = "field 'connection_from_client_to_director' must be a boolean when "
          "provided.";
    return std::nullopt;
  }
  if (heartbeat_interval && !json_is_null(heartbeat_interval)
      && !json_is_integer(heartbeat_interval)) {
    error = "field 'heartbeat_interval' must be an integer when provided.";
    return std::nullopt;
  }
  if (maximum_bandwidth_per_job && !json_is_null(maximum_bandwidth_per_job)
      && !json_is_integer(maximum_bandwidth_per_job)) {
    error
        = "field 'maximum_bandwidth_per_job' must be an integer when "
          "provided.";
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
  if (enabled && json_is_boolean(enabled)) {
    spec.enabled = json_is_true(enabled);
  }
  if (passive && json_is_boolean(passive)) {
    spec.passive = json_is_true(passive);
  }
  if (connection_from_director_to_client
      && json_is_boolean(connection_from_director_to_client)) {
    spec.connection_from_director_to_client
        = json_is_true(connection_from_director_to_client);
  }
  if (connection_from_client_to_director
      && json_is_boolean(connection_from_client_to_director)) {
    spec.connection_from_client_to_director
        = json_is_true(connection_from_client_to_director);
  }
  if (heartbeat_interval && json_is_integer(heartbeat_interval)) {
    const auto value = json_integer_value(heartbeat_interval);
    if (value < 0) {
      error = "field 'heartbeat_interval' must be non-negative.";
      return std::nullopt;
    }
    spec.heartbeat_interval = static_cast<uint64_t>(value);
  }
  if (maximum_bandwidth_per_job && json_is_integer(maximum_bandwidth_per_job)) {
    const auto value = json_integer_value(maximum_bandwidth_per_job);
    if (value < 0) {
      error = "field 'maximum_bandwidth_per_job' must be non-negative.";
      return std::nullopt;
    }
    spec.maximum_bandwidth_per_job = static_cast<uint64_t>(value);
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
  auto* enabled = json_object_get(root.get(), "enabled");
  auto* allow_compression = json_object_get(root.get(), "allow_compression");
  auto* heartbeat_interval = json_object_get(root.get(), "heartbeat_interval");
  auto* maximum_bandwidth_per_job
      = json_object_get(root.get(), "maximum_bandwidth_per_job");
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
  if (enabled && !json_is_null(enabled) && !json_is_boolean(enabled)) {
    error = "field 'enabled' must be a boolean when provided.";
    return std::nullopt;
  }
  if (allow_compression && !json_is_null(allow_compression)
      && !json_is_boolean(allow_compression)) {
    error = "field 'allow_compression' must be a boolean when provided.";
    return std::nullopt;
  }
  if (heartbeat_interval && !json_is_null(heartbeat_interval)
      && !json_is_integer(heartbeat_interval)) {
    error = "field 'heartbeat_interval' must be an integer when provided.";
    return std::nullopt;
  }
  if (maximum_bandwidth_per_job && !json_is_null(maximum_bandwidth_per_job)
      && !json_is_integer(maximum_bandwidth_per_job)) {
    error
        = "field 'maximum_bandwidth_per_job' must be an integer when "
          "provided.";
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
  if (enabled && json_is_boolean(enabled)) {
    spec.enabled = json_is_true(enabled);
  }
  if (allow_compression && json_is_boolean(allow_compression)) {
    spec.allow_compression = json_is_true(allow_compression);
  }
  if (heartbeat_interval && json_is_integer(heartbeat_interval)) {
    const auto value = json_integer_value(heartbeat_interval);
    if (value < 0) {
      error = "field 'heartbeat_interval' must be non-negative.";
      return std::nullopt;
    }
    spec.heartbeat_interval = static_cast<uint64_t>(value);
  }
  if (maximum_bandwidth_per_job && json_is_integer(maximum_bandwidth_per_job)) {
    const auto value = json_integer_value(maximum_bandwidth_per_job);
    if (value < 0) {
      error = "field 'maximum_bandwidth_per_job' must be non-negative.";
      return std::nullopt;
    }
    spec.maximum_bandwidth_per_job = static_cast<uint64_t>(value);
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

std::optional<ConsoleConsoleRequestSpec> ParseConsoleConsoleRequest(
    std::string_view body,
    std::string& error)
{
  json_error_t json_error{};
  auto root = MakeJson(json_loadb(body.data(), body.size(), 0, &json_error));
  if (!root) {
    error = "invalid JSON body: " + std::string{json_error.text};
    return std::nullopt;
  }

  auto* director = json_object_get(root.get(), "director");
  auto* password = json_object_get(root.get(), "password");
  auto* description = json_object_get(root.get(), "description");
  auto* rc_file = json_object_get(root.get(), "rc_file");
  auto* history_file = json_object_get(root.get(), "history_file");
  auto* history_length = json_object_get(root.get(), "history_length");
  auto* heartbeat_interval = json_object_get(root.get(), "heartbeat_interval");
  auto* tls_authenticate = json_object_get(root.get(), "tls_authenticate");
  auto* tls_enable = json_object_get(root.get(), "tls_enable");
  auto* tls_require = json_object_get(root.get(), "tls_require");
  auto* tls_verify_peer = json_object_get(root.get(), "tls_verify_peer");
  auto* tls_cipher_list = json_object_get(root.get(), "tls_cipher_list");
  auto* tls_cipher_suites = json_object_get(root.get(), "tls_cipher_suites");
  auto* tls_dh_file = json_object_get(root.get(), "tls_dh_file");
  auto* tls_protocol = json_object_get(root.get(), "tls_protocol");
  auto* tls_ca_certificate_file
      = json_object_get(root.get(), "tls_ca_certificate_file");
  auto* tls_ca_certificate_dir
      = json_object_get(root.get(), "tls_ca_certificate_dir");
  auto* tls_certificate_revocation_list
      = json_object_get(root.get(), "tls_certificate_revocation_list");
  auto* tls_certificate = json_object_get(root.get(), "tls_certificate");
  auto* tls_key = json_object_get(root.get(), "tls_key");
  auto* tls_allowed_cn = json_object_get(root.get(), "tls_allowed_cn");

  if (director && !json_is_null(director) && !json_is_string(director)) {
    error = "field 'director' must be a string when provided.";
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
  if (rc_file && !json_is_null(rc_file) && !json_is_string(rc_file)) {
    error = "field 'rc_file' must be a string when provided.";
    return std::nullopt;
  }
  if (history_file && !json_is_null(history_file)
      && !json_is_string(history_file)) {
    error = "field 'history_file' must be a string when provided.";
    return std::nullopt;
  }
  if (history_length && !json_is_null(history_length)
      && !json_is_integer(history_length)) {
    error = "field 'history_length' must be an integer when provided.";
    return std::nullopt;
  }
  if (heartbeat_interval && !json_is_null(heartbeat_interval)
      && !json_is_integer(heartbeat_interval)) {
    error = "field 'heartbeat_interval' must be an integer when provided.";
    return std::nullopt;
  }
  auto require_string = [&error](json_t* value, const char* field) {
    if (value && !json_is_null(value) && !json_is_string(value)) {
      error = std::string{"field '"} + field
              + "' must be a string when provided.";
      return false;
    }
    return true;
  };
  auto require_string_array = [&error](json_t* value, const char* field) {
    if (!value || json_is_null(value)) { return true; }
    if (!json_is_array(value)) {
      error = std::string{"field '"} + field
              + "' must be an array of strings when provided.";
      return false;
    }
    for (size_t index = 0; index < json_array_size(value); ++index) {
      if (!json_is_string(json_array_get(value, index))) {
        error = std::string{"field '"} + field
                + "' must be an array of strings when provided.";
        return false;
      }
    }
    return true;
  };
  if (!require_string(tls_cipher_list, "tls_cipher_list")
      || !require_string(tls_cipher_suites, "tls_cipher_suites")
      || !require_string(tls_dh_file, "tls_dh_file")
      || !require_string(tls_protocol, "tls_protocol")
      || !require_string(tls_ca_certificate_file, "tls_ca_certificate_file")
      || !require_string(tls_ca_certificate_dir, "tls_ca_certificate_dir")
      || !require_string(tls_certificate_revocation_list,
                         "tls_certificate_revocation_list")
      || !require_string(tls_certificate, "tls_certificate")
      || !require_string(tls_key, "tls_key")
      || !require_string_array(tls_allowed_cn, "tls_allowed_cn")) {
    return std::nullopt;
  }
  auto require_bool = [&error](json_t* value, const char* field) {
    if (value && !json_is_null(value) && !json_is_boolean(value)) {
      error = std::string{"field '"} + field
              + "' must be a boolean when provided.";
      return false;
    }
    return true;
  };
  if (!require_bool(tls_authenticate, "tls_authenticate")
      || !require_bool(tls_enable, "tls_enable")
      || !require_bool(tls_require, "tls_require")
      || !require_bool(tls_verify_peer, "tls_verify_peer")) {
    return std::nullopt;
  }

  ConsoleConsoleRequestSpec spec{};
  if (director && json_is_string(director)) {
    spec.director = std::string{json_string_value(director)};
  }
  if (password && json_is_string(password)) {
    spec.password = std::string{json_string_value(password)};
  }
  if (description && json_is_string(description)) {
    spec.description = std::string{json_string_value(description)};
  }
  if (rc_file && json_is_string(rc_file)) {
    spec.rc_file = std::string{json_string_value(rc_file)};
  }
  if (history_file && json_is_string(history_file)) {
    spec.history_file = std::string{json_string_value(history_file)};
  }
  if (history_length && json_is_integer(history_length)) {
    const auto value = json_integer_value(history_length);
    if (value < 0) {
      error = "field 'history_length' must be non-negative.";
      return std::nullopt;
    }
    spec.history_length = static_cast<uint32_t>(value);
  }
  if (heartbeat_interval && json_is_integer(heartbeat_interval)) {
    const auto value = json_integer_value(heartbeat_interval);
    if (value < 0) {
      error = "field 'heartbeat_interval' must be non-negative.";
      return std::nullopt;
    }
    spec.heartbeat_interval = static_cast<uint64_t>(value);
  }
  if (tls_authenticate && json_is_boolean(tls_authenticate)) {
    spec.tls_authenticate = json_is_true(tls_authenticate);
  }
  if (tls_enable && json_is_boolean(tls_enable)) {
    spec.tls_enable = json_is_true(tls_enable);
  }
  if (tls_require && json_is_boolean(tls_require)) {
    spec.tls_require = json_is_true(tls_require);
  }
  if (tls_verify_peer && json_is_boolean(tls_verify_peer)) {
    spec.tls_verify_peer = json_is_true(tls_verify_peer);
  }
  if (tls_cipher_list && json_is_string(tls_cipher_list)) {
    spec.tls_cipher_list = std::string{json_string_value(tls_cipher_list)};
  }
  if (tls_cipher_suites && json_is_string(tls_cipher_suites)) {
    spec.tls_cipher_suites = std::string{json_string_value(tls_cipher_suites)};
  }
  if (tls_dh_file && json_is_string(tls_dh_file)) {
    spec.tls_dh_file = std::string{json_string_value(tls_dh_file)};
  }
  if (tls_protocol && json_is_string(tls_protocol)) {
    spec.tls_protocol = std::string{json_string_value(tls_protocol)};
  }
  if (tls_ca_certificate_file && json_is_string(tls_ca_certificate_file)) {
    spec.tls_ca_certificate_file
        = std::string{json_string_value(tls_ca_certificate_file)};
  }
  if (tls_ca_certificate_dir && json_is_string(tls_ca_certificate_dir)) {
    spec.tls_ca_certificate_dir
        = std::string{json_string_value(tls_ca_certificate_dir)};
  }
  if (tls_certificate_revocation_list
      && json_is_string(tls_certificate_revocation_list)) {
    spec.tls_certificate_revocation_list
        = std::string{json_string_value(tls_certificate_revocation_list)};
  }
  if (tls_certificate && json_is_string(tls_certificate)) {
    spec.tls_certificate = std::string{json_string_value(tls_certificate)};
  }
  if (tls_key && json_is_string(tls_key)) {
    spec.tls_key = std::string{json_string_value(tls_key)};
  }
  if (tls_allowed_cn && json_is_array(tls_allowed_cn)) {
    std::vector<std::string> values;
    values.reserve(json_array_size(tls_allowed_cn));
    for (size_t index = 0; index < json_array_size(tls_allowed_cn); ++index) {
      values.emplace_back(
          json_string_value(json_array_get(tls_allowed_cn, index)));
    }
    spec.tls_allowed_cn = std::move(values);
  }
  return spec;
}

std::optional<ConsoleDirectorRequestSpec> ParseConsoleDirectorRequest(
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
  auto* heartbeat_interval = json_object_get(root.get(), "heartbeat_interval");
  auto* tls_authenticate = json_object_get(root.get(), "tls_authenticate");
  auto* tls_enable = json_object_get(root.get(), "tls_enable");
  auto* tls_require = json_object_get(root.get(), "tls_require");
  auto* tls_verify_peer = json_object_get(root.get(), "tls_verify_peer");
  auto* tls_cipher_list = json_object_get(root.get(), "tls_cipher_list");
  auto* tls_cipher_suites = json_object_get(root.get(), "tls_cipher_suites");
  auto* tls_dh_file = json_object_get(root.get(), "tls_dh_file");
  auto* tls_protocol = json_object_get(root.get(), "tls_protocol");
  auto* tls_ca_certificate_file
      = json_object_get(root.get(), "tls_ca_certificate_file");
  auto* tls_ca_certificate_dir
      = json_object_get(root.get(), "tls_ca_certificate_dir");
  auto* tls_certificate_revocation_list
      = json_object_get(root.get(), "tls_certificate_revocation_list");
  auto* tls_certificate = json_object_get(root.get(), "tls_certificate");
  auto* tls_key = json_object_get(root.get(), "tls_key");
  auto* tls_allowed_cn = json_object_get(root.get(), "tls_allowed_cn");

  if (address && !json_is_null(address) && !json_is_string(address)) {
    error = "field 'address' must be a string when provided.";
    return std::nullopt;
  }
  if (port && !json_is_null(port) && !json_is_integer(port)) {
    error = "field 'port' must be an integer when provided.";
    return std::nullopt;
  }
  if (port && json_is_integer(port)) {
    const auto parsed_port = json_integer_value(port);
    if (parsed_port < 1 || parsed_port > 65535) {
      error = "field 'port' must be between 1 and 65535 when provided.";
      return std::nullopt;
    }
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
  if (heartbeat_interval && !json_is_null(heartbeat_interval)
      && !json_is_integer(heartbeat_interval)) {
    error = "field 'heartbeat_interval' must be an integer when provided.";
    return std::nullopt;
  }
  auto require_string = [&error](json_t* value, const char* field) {
    if (value && !json_is_null(value) && !json_is_string(value)) {
      error = std::string{"field '"} + field
              + "' must be a string when provided.";
      return false;
    }
    return true;
  };
  auto require_string_array = [&error](json_t* value, const char* field) {
    if (!value || json_is_null(value)) { return true; }
    if (!json_is_array(value)) {
      error = std::string{"field '"} + field
              + "' must be an array of strings when provided.";
      return false;
    }
    for (size_t index = 0; index < json_array_size(value); ++index) {
      if (!json_is_string(json_array_get(value, index))) {
        error = std::string{"field '"} + field
                + "' must be an array of strings when provided.";
        return false;
      }
    }
    return true;
  };
  if (!require_string(tls_cipher_list, "tls_cipher_list")
      || !require_string(tls_cipher_suites, "tls_cipher_suites")
      || !require_string(tls_dh_file, "tls_dh_file")
      || !require_string(tls_protocol, "tls_protocol")
      || !require_string(tls_ca_certificate_file, "tls_ca_certificate_file")
      || !require_string(tls_ca_certificate_dir, "tls_ca_certificate_dir")
      || !require_string(tls_certificate_revocation_list,
                         "tls_certificate_revocation_list")
      || !require_string(tls_certificate, "tls_certificate")
      || !require_string(tls_key, "tls_key")
      || !require_string_array(tls_allowed_cn, "tls_allowed_cn")) {
    return std::nullopt;
  }
  auto require_bool = [&error](json_t* value, const char* field) {
    if (value && !json_is_null(value) && !json_is_boolean(value)) {
      error = std::string{"field '"} + field
              + "' must be a boolean when provided.";
      return false;
    }
    return true;
  };
  if (!require_bool(tls_authenticate, "tls_authenticate")
      || !require_bool(tls_enable, "tls_enable")
      || !require_bool(tls_require, "tls_require")
      || !require_bool(tls_verify_peer, "tls_verify_peer")) {
    return std::nullopt;
  }

  ConsoleDirectorRequestSpec spec{};
  if (address && json_is_string(address)) {
    spec.address = std::string{json_string_value(address)};
  }
  if (port && json_is_integer(port)) {
    spec.port = static_cast<uint16_t>(json_integer_value(port));
  }
  if (password && json_is_string(password)) {
    spec.password = std::string{json_string_value(password)};
  }
  if (description && json_is_string(description)) {
    spec.description = std::string{json_string_value(description)};
  }
  if (heartbeat_interval && json_is_integer(heartbeat_interval)) {
    const auto value = json_integer_value(heartbeat_interval);
    if (value < 0) {
      error = "field 'heartbeat_interval' must be non-negative.";
      return std::nullopt;
    }
    spec.heartbeat_interval = static_cast<uint64_t>(value);
  }
  if (tls_authenticate && json_is_boolean(tls_authenticate)) {
    spec.tls_authenticate = json_is_true(tls_authenticate);
  }
  if (tls_enable && json_is_boolean(tls_enable)) {
    spec.tls_enable = json_is_true(tls_enable);
  }
  if (tls_require && json_is_boolean(tls_require)) {
    spec.tls_require = json_is_true(tls_require);
  }
  if (tls_verify_peer && json_is_boolean(tls_verify_peer)) {
    spec.tls_verify_peer = json_is_true(tls_verify_peer);
  }
  if (tls_cipher_list && json_is_string(tls_cipher_list)) {
    spec.tls_cipher_list = std::string{json_string_value(tls_cipher_list)};
  }
  if (tls_cipher_suites && json_is_string(tls_cipher_suites)) {
    spec.tls_cipher_suites = std::string{json_string_value(tls_cipher_suites)};
  }
  if (tls_dh_file && json_is_string(tls_dh_file)) {
    spec.tls_dh_file = std::string{json_string_value(tls_dh_file)};
  }
  if (tls_protocol && json_is_string(tls_protocol)) {
    spec.tls_protocol = std::string{json_string_value(tls_protocol)};
  }
  if (tls_ca_certificate_file && json_is_string(tls_ca_certificate_file)) {
    spec.tls_ca_certificate_file
        = std::string{json_string_value(tls_ca_certificate_file)};
  }
  if (tls_ca_certificate_dir && json_is_string(tls_ca_certificate_dir)) {
    spec.tls_ca_certificate_dir
        = std::string{json_string_value(tls_ca_certificate_dir)};
  }
  if (tls_certificate_revocation_list
      && json_is_string(tls_certificate_revocation_list)) {
    spec.tls_certificate_revocation_list
        = std::string{json_string_value(tls_certificate_revocation_list)};
  }
  if (tls_certificate && json_is_string(tls_certificate)) {
    spec.tls_certificate = std::string{json_string_value(tls_certificate)};
  }
  if (tls_key && json_is_string(tls_key)) {
    spec.tls_key = std::string{json_string_value(tls_key)};
  }
  if (tls_allowed_cn && json_is_array(tls_allowed_cn)) {
    std::vector<std::string> values;
    values.reserve(json_array_size(tls_allowed_cn));
    for (size_t index = 0; index < json_array_size(tls_allowed_cn); ++index) {
      values.emplace_back(
          json_string_value(json_array_get(tls_allowed_cn, index)));
    }
    spec.tls_allowed_cn = std::move(values);
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

std::optional<DirectorMessagesRequestSpec> ParseDirectorMessagesRequest(
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
  auto* entries = json_object_get(root.get(), "entries");
  if (description && !json_is_null(description)
      && !json_is_string(description)) {
    error = "field 'description' must be a string when provided.";
    return std::nullopt;
  }
  if (entries && !json_is_null(entries) && !json_is_array(entries)) {
    error = "field 'entries' must be an array of strings when provided.";
    return std::nullopt;
  }

  DirectorMessagesRequestSpec spec{};
  if (description && json_is_string(description)) {
    spec.description = std::string{json_string_value(description)};
  }
  if (entries && json_is_array(entries)) {
    std::vector<std::string> parsed_entries;
    parsed_entries.reserve(json_array_size(entries));
    for (size_t index = 0; index < json_array_size(entries); ++index) {
      auto* entry = json_array_get(entries, index);
      if (!json_is_string(entry)) {
        error = "field 'entries' must contain only strings.";
        return std::nullopt;
      }
      parsed_entries.emplace_back(json_string_value(entry));
    }
    spec.entries = std::move(parsed_entries);
  }
  return spec;
}

std::optional<StorageDirectorRequestSpec> ParseStorageDirectorRequest(
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
  auto* monitor = json_object_get(root.get(), "monitor");
  auto* maximum_bandwidth_per_job
      = json_object_get(root.get(), "maximum_bandwidth_per_job");
  if (password && !json_is_null(password) && !json_is_string(password)) {
    error = "field 'password' must be a string when provided.";
    return std::nullopt;
  }
  if (description && !json_is_null(description)
      && !json_is_string(description)) {
    error = "field 'description' must be a string when provided.";
    return std::nullopt;
  }
  if (monitor && !json_is_null(monitor) && !json_is_boolean(monitor)) {
    error = "field 'monitor' must be a boolean when provided.";
    return std::nullopt;
  }
  if (maximum_bandwidth_per_job && !json_is_null(maximum_bandwidth_per_job)
      && !json_is_integer(maximum_bandwidth_per_job)) {
    error
        = "field 'maximum_bandwidth_per_job' must be an integer when provided.";
    return std::nullopt;
  }

  StorageDirectorRequestSpec spec{};
  if (password && json_is_string(password)) {
    spec.password = std::string{json_string_value(password)};
  }
  if (description && json_is_string(description)) {
    spec.description = std::string{json_string_value(description)};
  }
  if (monitor && json_is_boolean(monitor)) {
    spec.monitor = json_is_true(monitor);
  }
  if (maximum_bandwidth_per_job && json_is_integer(maximum_bandwidth_per_job)) {
    const auto value = json_integer_value(maximum_bandwidth_per_job);
    if (value < 0) {
      error = "field 'maximum_bandwidth_per_job' must be non-negative.";
      return std::nullopt;
    }
    spec.maximum_bandwidth_per_job = static_cast<uint64_t>(value);
  }
  return spec;
}

std::optional<StorageDeviceRequestSpec> ParseStorageDeviceRequest(
    std::string_view body,
    std::string& error)
{
  json_error_t json_error{};
  auto root = MakeJson(json_loadb(body.data(), body.size(), 0, &json_error));
  if (!root) {
    error = "invalid JSON body: " + std::string{json_error.text};
    return std::nullopt;
  }

  auto* media_type = json_object_get(root.get(), "media_type");
  auto* archive_device = json_object_get(root.get(), "archive_device");
  auto* device_type = json_object_get(root.get(), "device_type");
  auto* description = json_object_get(root.get(), "description");
  auto require_string = [&error](json_t* value, const char* field) {
    if (value && !json_is_null(value) && !json_is_string(value)) {
      error = std::string{"field '"} + field
              + "' must be a string when provided.";
      return false;
    }
    return true;
  };
  if (!require_string(media_type, "media_type")
      || !require_string(archive_device, "archive_device")
      || !require_string(device_type, "device_type")
      || !require_string(description, "description")) {
    return std::nullopt;
  }

  StorageDeviceRequestSpec spec{};
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

std::optional<StorageNdmpRequestSpec> ParseStorageNdmpRequest(
    std::string_view body,
    std::string& error)
{
  json_error_t json_error{};
  auto root = MakeJson(json_loadb(body.data(), body.size(), 0, &json_error));
  if (!root) {
    error = "invalid JSON body: " + std::string{json_error.text};
    return std::nullopt;
  }

  auto* username = json_object_get(root.get(), "username");
  auto* password = json_object_get(root.get(), "password");
  auto* auth_type = json_object_get(root.get(), "auth_type");
  auto* log_level = json_object_get(root.get(), "log_level");
  auto require_string = [&error](json_t* value, const char* field) {
    if (value && !json_is_null(value) && !json_is_string(value)) {
      error = std::string{"field '"} + field
              + "' must be a string when provided.";
      return false;
    }
    return true;
  };
  if (!require_string(username, "username")
      || !require_string(password, "password")
      || !require_string(auth_type, "auth_type")) {
    return std::nullopt;
  }
  if (log_level && !json_is_null(log_level) && !json_is_integer(log_level)) {
    error = "field 'log_level' must be an integer when provided.";
    return std::nullopt;
  }

  StorageNdmpRequestSpec spec{};
  if (username && json_is_string(username)) {
    spec.username = std::string{json_string_value(username)};
  }
  if (password && json_is_string(password)) {
    spec.password = std::string{json_string_value(password)};
  }
  if (auth_type && json_is_string(auth_type)) {
    spec.auth_type = std::string{json_string_value(auth_type)};
  }
  if (log_level && json_is_integer(log_level)) {
    const auto value = json_integer_value(log_level);
    if (value < 0) {
      error = "field 'log_level' must be non-negative.";
      return std::nullopt;
    }
    spec.log_level = static_cast<uint32_t>(value);
  }
  return spec;
}

std::optional<StorageAutochangerRequestSpec> ParseStorageAutochangerRequest(
    std::string_view body,
    std::string& error)
{
  json_error_t json_error{};
  auto root = MakeJson(json_loadb(body.data(), body.size(), 0, &json_error));
  if (!root) {
    error = "invalid JSON body: " + std::string{json_error.text};
    return std::nullopt;
  }

  auto* devices = json_object_get(root.get(), "devices");
  auto* changer_device = json_object_get(root.get(), "changer_device");
  auto* changer_command = json_object_get(root.get(), "changer_command");
  auto* description = json_object_get(root.get(), "description");
  auto require_string = [&error](json_t* value, const char* field) {
    if (value && !json_is_null(value) && !json_is_string(value)) {
      error = std::string{"field '"} + field
              + "' must be a string when provided.";
      return false;
    }
    return true;
  };
  if (!require_string(changer_device, "changer_device")
      || !require_string(changer_command, "changer_command")
      || !require_string(description, "description")) {
    return std::nullopt;
  }
  if (devices && !json_is_null(devices)) {
    if (!json_is_array(devices)) {
      error = "field 'devices' must be an array of strings when provided.";
      return std::nullopt;
    }
    size_t index = 0;
    json_t* entry = nullptr;
    json_array_foreach(devices, index, entry)
    {
      if (!json_is_string(entry)) {
        error = "field 'devices' must be an array of strings when provided.";
        return std::nullopt;
      }
    }
  }

  StorageAutochangerRequestSpec spec{};
  if (devices && json_is_array(devices)) {
    std::vector<std::string> device_names;
    const size_t size = json_array_size(devices);
    device_names.reserve(size);
    for (size_t index = 0; index < size; ++index) {
      auto* entry = json_array_get(devices, index);
      device_names.emplace_back(json_string_value(entry));
    }
    spec.devices = std::move(device_names);
  }
  if (changer_device && json_is_string(changer_device)) {
    spec.changer_device = std::string{json_string_value(changer_device)};
  }
  if (changer_command && json_is_string(changer_command)) {
    spec.changer_command = std::string{json_string_value(changer_command)};
  }
  if (description && json_is_string(description)) {
    spec.description = std::string{json_string_value(description)};
  }
  return spec;
}

std::optional<StorageDaemonRequestSpec> ParseStorageDaemonRequest(
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
  auto* addresses = json_object_get(root.get(), "addresses");
  auto* port = json_object_get(root.get(), "port");
  auto* just_in_time_reservation
      = json_object_get(root.get(), "just_in_time_reservation");
  auto* maximum_concurrent_jobs
      = json_object_get(root.get(), "maximum_concurrent_jobs");
  auto* maximum_workers_per_job
      = json_object_get(root.get(), "maximum_workers_per_job");
  auto* absolute_job_timeout
      = json_object_get(root.get(), "absolute_job_timeout");
  auto* source_address = json_object_get(root.get(), "source_address");
  auto* source_addresses = json_object_get(root.get(), "source_addresses");
  auto* tls_authenticate = json_object_get(root.get(), "tls_authenticate");
  auto* tls_enable = json_object_get(root.get(), "tls_enable");
  auto* tls_require = json_object_get(root.get(), "tls_require");
  auto* tls_verify_peer = json_object_get(root.get(), "tls_verify_peer");
  auto* tls_cipher_list = json_object_get(root.get(), "tls_cipher_list");
  auto* tls_cipher_suites = json_object_get(root.get(), "tls_cipher_suites");
  auto* tls_dh_file = json_object_get(root.get(), "tls_dh_file");
  auto* tls_protocol = json_object_get(root.get(), "tls_protocol");
  auto* tls_ca_certificate_file
      = json_object_get(root.get(), "tls_ca_certificate_file");
  auto* tls_ca_certificate_dir
      = json_object_get(root.get(), "tls_ca_certificate_dir");
  auto* tls_certificate_revocation_list
      = json_object_get(root.get(), "tls_certificate_revocation_list");
  auto* tls_certificate = json_object_get(root.get(), "tls_certificate");
  auto* tls_key = json_object_get(root.get(), "tls_key");
  auto* tls_allowed_cn = json_object_get(root.get(), "tls_allowed_cn");
  auto* pki_signatures = json_object_get(root.get(), "pki_signatures");
  auto* pki_encryption = json_object_get(root.get(), "pki_encryption");
  auto* pki_key_pair = json_object_get(root.get(), "pki_key_pair");
  auto* pki_signers = json_object_get(root.get(), "pki_signers");
  auto* pki_master_keys = json_object_get(root.get(), "pki_master_keys");
  auto* pki_cipher = json_object_get(root.get(), "pki_cipher");
  auto* always_use_lmdb = json_object_get(root.get(), "always_use_lmdb");
  auto* lmdb_threshold = json_object_get(root.get(), "lmdb_threshold");
  auto* ver_id = json_object_get(root.get(), "ver_id");
  auto* log_timestamp_format
      = json_object_get(root.get(), "log_timestamp_format");
  auto* maximum_bandwidth_per_job
      = json_object_get(root.get(), "maximum_bandwidth_per_job");
  auto* secure_erase_command
      = json_object_get(root.get(), "secure_erase_command");
  auto* grpc_module = json_object_get(root.get(), "grpc_module");
  auto* enable_ktls = json_object_get(root.get(), "enable_ktls");
  auto* statistics_collect_interval
      = json_object_get(root.get(), "statistics_collect_interval");
  auto* allow_bandwidth_bursting
      = json_object_get(root.get(), "allow_bandwidth_bursting");
  auto* ndmp_enable = json_object_get(root.get(), "ndmp_enable");
  auto* ndmp_snooping = json_object_get(root.get(), "ndmp_snooping");
  auto* ndmp_log_level = json_object_get(root.get(), "ndmp_log_level");
  auto* ndmp_address = json_object_get(root.get(), "ndmp_address");
  auto* ndmp_port = json_object_get(root.get(), "ndmp_port");
  auto* ndmp_addresses = json_object_get(root.get(), "ndmp_addresses");
  auto* autoxflate_on_replication
      = json_object_get(root.get(), "autoxflate_on_replication");
  auto* collect_device_statistics
      = json_object_get(root.get(), "collect_device_statistics");
  auto* collect_job_statistics
      = json_object_get(root.get(), "collect_job_statistics");
  auto* device_reserve_by_media_type
      = json_object_get(root.get(), "device_reserve_by_media_type");
  auto* file_device_concurrent_read
      = json_object_get(root.get(), "file_device_concurrent_read");
  auto* sd_connect_timeout = json_object_get(root.get(), "sd_connect_timeout");
  auto* fd_connect_timeout = json_object_get(root.get(), "fd_connect_timeout");
  auto* heartbeat_interval = json_object_get(root.get(), "heartbeat_interval");
  auto* checkpoint_interval
      = json_object_get(root.get(), "checkpoint_interval");
  auto* client_connect_wait
      = json_object_get(root.get(), "client_connect_wait");
  auto* maximum_network_buffer_size
      = json_object_get(root.get(), "maximum_network_buffer_size");
  auto* description = json_object_get(root.get(), "description");
  auto* working_directory = json_object_get(root.get(), "working_directory");
  auto* plugin_directory = json_object_get(root.get(), "plugin_directory");
  auto* plugin_names = json_object_get(root.get(), "plugin_names");
#if defined(HAVE_DYNAMIC_SD_BACKENDS)
  auto* backend_directories
      = json_object_get(root.get(), "backend_directories");
#endif
  auto* allowed_script_dirs
      = json_object_get(root.get(), "allowed_script_dirs");
  auto* allowed_job_commands
      = json_object_get(root.get(), "allowed_job_commands");
  auto* scripts_directory = json_object_get(root.get(), "scripts_directory");
  auto* messages = json_object_get(root.get(), "messages");
  auto require_string = [&error](json_t* value, const char* field) {
    if (value && !json_is_null(value) && !json_is_string(value)) {
      error = std::string{"field '"} + field
              + "' must be a string when provided.";
      return false;
    }
    return true;
  };
  auto require_string_array = [&error](json_t* value, const char* field) {
    if (!value || json_is_null(value)) { return true; }
    if (!json_is_array(value)) {
      error = std::string{"field '"} + field
              + "' must be an array of strings when provided.";
      return false;
    }
    for (size_t index = 0; index < json_array_size(value); ++index) {
      if (!json_is_string(json_array_get(value, index))) {
        error = std::string{"field '"} + field
                + "' must be an array of strings when provided.";
        return false;
      }
    }
    return true;
  };
  if (!require_string(address, "address")
      || !require_string_array(addresses, "addresses")
      || !require_string(source_address, "source_address")
      || !require_string_array(source_addresses, "source_addresses")
      || (port && !json_is_null(port) && !json_is_integer(port))
      || (maximum_concurrent_jobs && !json_is_null(maximum_concurrent_jobs)
          && !json_is_integer(maximum_concurrent_jobs))
      || (maximum_workers_per_job && !json_is_null(maximum_workers_per_job)
          && !json_is_integer(maximum_workers_per_job))
      || (absolute_job_timeout && !json_is_null(absolute_job_timeout)
          && !json_is_integer(absolute_job_timeout))
      || (lmdb_threshold && !json_is_null(lmdb_threshold)
          && !json_is_integer(lmdb_threshold))
      || (maximum_bandwidth_per_job && !json_is_null(maximum_bandwidth_per_job)
          && !json_is_integer(maximum_bandwidth_per_job))
      || !require_string(secure_erase_command, "secure_erase_command")
      || !require_string(grpc_module, "grpc_module")
      || !require_string(tls_cipher_list, "tls_cipher_list")
      || !require_string(tls_cipher_suites, "tls_cipher_suites")
      || !require_string(tls_dh_file, "tls_dh_file")
      || !require_string(tls_protocol, "tls_protocol")
      || !require_string(tls_ca_certificate_file, "tls_ca_certificate_file")
      || !require_string(tls_ca_certificate_dir, "tls_ca_certificate_dir")
      || !require_string(tls_certificate_revocation_list,
                         "tls_certificate_revocation_list")
      || !require_string(tls_certificate, "tls_certificate")
      || !require_string(tls_key, "tls_key")
      || !require_string_array(tls_allowed_cn, "tls_allowed_cn")
      || !require_string(pki_key_pair, "pki_key_pair")
      || !require_string_array(pki_signers, "pki_signers")
      || !require_string_array(pki_master_keys, "pki_master_keys")
      || !require_string(pki_cipher, "pki_cipher")
      || (statistics_collect_interval
          && !json_is_null(statistics_collect_interval)
          && !json_is_integer(statistics_collect_interval))
      || !require_string(ver_id, "ver_id")
      || !require_string(log_timestamp_format, "log_timestamp_format")
      || (just_in_time_reservation && !json_is_null(just_in_time_reservation)
          && !json_is_boolean(just_in_time_reservation))
      || (tls_authenticate && !json_is_null(tls_authenticate)
          && !json_is_boolean(tls_authenticate))
      || (tls_enable && !json_is_null(tls_enable)
          && !json_is_boolean(tls_enable))
      || (tls_require && !json_is_null(tls_require)
          && !json_is_boolean(tls_require))
      || (tls_verify_peer && !json_is_null(tls_verify_peer)
          && !json_is_boolean(tls_verify_peer))
      || (enable_ktls && !json_is_null(enable_ktls)
          && !json_is_boolean(enable_ktls))
      || (allow_bandwidth_bursting && !json_is_null(allow_bandwidth_bursting)
          && !json_is_boolean(allow_bandwidth_bursting))
      || (pki_signatures && !json_is_null(pki_signatures)
          && !json_is_boolean(pki_signatures))
      || (pki_encryption && !json_is_null(pki_encryption)
          && !json_is_boolean(pki_encryption))
      || (ndmp_enable && !json_is_null(ndmp_enable)
          && !json_is_boolean(ndmp_enable))
      || (ndmp_snooping && !json_is_null(ndmp_snooping)
          && !json_is_boolean(ndmp_snooping))
      || (ndmp_log_level && !json_is_null(ndmp_log_level)
          && !json_is_integer(ndmp_log_level))
      || !require_string(ndmp_address, "ndmp_address")
      || !require_string_array(ndmp_addresses, "ndmp_addresses")
      || (ndmp_port && !json_is_null(ndmp_port) && !json_is_integer(ndmp_port))
      || (always_use_lmdb && !json_is_null(always_use_lmdb)
          && !json_is_boolean(always_use_lmdb))
      || (autoxflate_on_replication && !json_is_null(autoxflate_on_replication)
          && !json_is_boolean(autoxflate_on_replication))
      || (collect_device_statistics && !json_is_null(collect_device_statistics)
          && !json_is_boolean(collect_device_statistics))
      || (collect_job_statistics && !json_is_null(collect_job_statistics)
          && !json_is_boolean(collect_job_statistics))
      || (device_reserve_by_media_type
          && !json_is_null(device_reserve_by_media_type)
          && !json_is_boolean(device_reserve_by_media_type))
      || (file_device_concurrent_read
          && !json_is_null(file_device_concurrent_read)
          && !json_is_boolean(file_device_concurrent_read))
      || (sd_connect_timeout && !json_is_null(sd_connect_timeout)
          && !json_is_integer(sd_connect_timeout))
      || (fd_connect_timeout && !json_is_null(fd_connect_timeout)
          && !json_is_integer(fd_connect_timeout))
      || (heartbeat_interval && !json_is_null(heartbeat_interval)
          && !json_is_integer(heartbeat_interval))
      || (checkpoint_interval && !json_is_null(checkpoint_interval)
          && !json_is_integer(checkpoint_interval))
      || (client_connect_wait && !json_is_null(client_connect_wait)
          && !json_is_integer(client_connect_wait))
      || (maximum_network_buffer_size
          && !json_is_null(maximum_network_buffer_size)
          && !json_is_integer(maximum_network_buffer_size))
      || !require_string(description, "description")
      || !require_string(working_directory, "working_directory")
      || !require_string(plugin_directory, "plugin_directory")
      || !require_string_array(plugin_names, "plugin_names")
#if defined(HAVE_DYNAMIC_SD_BACKENDS)
      || !require_string_array(backend_directories, "backend_directories")
#endif
      || !require_string_array(allowed_script_dirs, "allowed_script_dirs")
      || !require_string_array(allowed_job_commands, "allowed_job_commands")
      || !require_string(scripts_directory, "scripts_directory")
      || !require_string(messages, "messages")) {
    if (port && !json_is_null(port) && !json_is_integer(port)) {
      error = "field 'port' must be an integer when provided.";
    } else if (maximum_concurrent_jobs && !json_is_null(maximum_concurrent_jobs)
               && !json_is_integer(maximum_concurrent_jobs)) {
      error
          = "field 'maximum_concurrent_jobs' must be an integer when provided.";
    } else if (maximum_workers_per_job && !json_is_null(maximum_workers_per_job)
               && !json_is_integer(maximum_workers_per_job)) {
      error
          = "field 'maximum_workers_per_job' must be an integer when provided.";
    } else if (absolute_job_timeout && !json_is_null(absolute_job_timeout)
               && !json_is_integer(absolute_job_timeout)) {
      error = "field 'absolute_job_timeout' must be an integer when provided.";
    } else if (lmdb_threshold && !json_is_null(lmdb_threshold)
               && !json_is_integer(lmdb_threshold)) {
      error = "field 'lmdb_threshold' must be an integer when provided.";
    } else if (maximum_bandwidth_per_job
               && !json_is_null(maximum_bandwidth_per_job)
               && !json_is_integer(maximum_bandwidth_per_job)) {
      error
          = "field 'maximum_bandwidth_per_job' must be an integer when "
            "provided.";
    } else if (just_in_time_reservation
               && !json_is_null(just_in_time_reservation)
               && !json_is_boolean(just_in_time_reservation)) {
      error
          = "field 'just_in_time_reservation' must be a boolean when provided.";
    } else if (tls_authenticate && !json_is_null(tls_authenticate)
               && !json_is_boolean(tls_authenticate)) {
      error = "field 'tls_authenticate' must be a boolean when provided.";
    } else if (tls_enable && !json_is_null(tls_enable)
               && !json_is_boolean(tls_enable)) {
      error = "field 'tls_enable' must be a boolean when provided.";
    } else if (tls_require && !json_is_null(tls_require)
               && !json_is_boolean(tls_require)) {
      error = "field 'tls_require' must be a boolean when provided.";
    } else if (tls_verify_peer && !json_is_null(tls_verify_peer)
               && !json_is_boolean(tls_verify_peer)) {
      error = "field 'tls_verify_peer' must be a boolean when provided.";
    } else if (enable_ktls && !json_is_null(enable_ktls)
               && !json_is_boolean(enable_ktls)) {
      error = "field 'enable_ktls' must be a boolean when provided.";
    } else if (statistics_collect_interval
               && !json_is_null(statistics_collect_interval)
               && !json_is_integer(statistics_collect_interval)) {
      error
          = "field 'statistics_collect_interval' must be an integer when "
            "provided.";
    } else if (allow_bandwidth_bursting
               && !json_is_null(allow_bandwidth_bursting)
               && !json_is_boolean(allow_bandwidth_bursting)) {
      error
          = "field 'allow_bandwidth_bursting' must be a boolean when provided.";
    } else if (pki_signatures && !json_is_null(pki_signatures)
               && !json_is_boolean(pki_signatures)) {
      error = "field 'pki_signatures' must be a boolean when provided.";
    } else if (pki_encryption && !json_is_null(pki_encryption)
               && !json_is_boolean(pki_encryption)) {
      error = "field 'pki_encryption' must be a boolean when provided.";
    } else if (ndmp_enable && !json_is_null(ndmp_enable)
               && !json_is_boolean(ndmp_enable)) {
      error = "field 'ndmp_enable' must be a boolean when provided.";
    } else if (ndmp_snooping && !json_is_null(ndmp_snooping)
               && !json_is_boolean(ndmp_snooping)) {
      error = "field 'ndmp_snooping' must be a boolean when provided.";
    } else if (ndmp_log_level && !json_is_null(ndmp_log_level)
               && !json_is_integer(ndmp_log_level)) {
      error = "field 'ndmp_log_level' must be an integer when provided.";
    } else if (ndmp_port && !json_is_null(ndmp_port)
               && !json_is_integer(ndmp_port)) {
      error = "field 'ndmp_port' must be an integer when provided.";
    } else if (always_use_lmdb && !json_is_null(always_use_lmdb)
               && !json_is_boolean(always_use_lmdb)) {
      error = "field 'always_use_lmdb' must be a boolean when provided.";
    } else if (autoxflate_on_replication
               && !json_is_null(autoxflate_on_replication)
               && !json_is_boolean(autoxflate_on_replication)) {
      error
          = "field 'autoxflate_on_replication' must be a boolean when "
            "provided.";
    } else if (collect_device_statistics
               && !json_is_null(collect_device_statistics)
               && !json_is_boolean(collect_device_statistics)) {
      error
          = "field 'collect_device_statistics' must be a boolean when "
            "provided.";
    } else if (collect_job_statistics && !json_is_null(collect_job_statistics)
               && !json_is_boolean(collect_job_statistics)) {
      error = "field 'collect_job_statistics' must be a boolean when provided.";
    } else if (device_reserve_by_media_type
               && !json_is_null(device_reserve_by_media_type)
               && !json_is_boolean(device_reserve_by_media_type)) {
      error
          = "field 'device_reserve_by_media_type' must be a boolean when "
            "provided.";
    } else if (file_device_concurrent_read
               && !json_is_null(file_device_concurrent_read)
               && !json_is_boolean(file_device_concurrent_read)) {
      error
          = "field 'file_device_concurrent_read' must be a boolean when "
            "provided.";
    } else if (sd_connect_timeout && !json_is_null(sd_connect_timeout)
               && !json_is_integer(sd_connect_timeout)) {
      error = "field 'sd_connect_timeout' must be an integer when provided.";
    } else if (fd_connect_timeout && !json_is_null(fd_connect_timeout)
               && !json_is_integer(fd_connect_timeout)) {
      error = "field 'fd_connect_timeout' must be an integer when provided.";
    } else if (heartbeat_interval && !json_is_null(heartbeat_interval)
               && !json_is_integer(heartbeat_interval)) {
      error = "field 'heartbeat_interval' must be an integer when provided.";
    } else if (checkpoint_interval && !json_is_null(checkpoint_interval)
               && !json_is_integer(checkpoint_interval)) {
      error = "field 'checkpoint_interval' must be an integer when provided.";
    } else if (client_connect_wait && !json_is_null(client_connect_wait)
               && !json_is_integer(client_connect_wait)) {
      error = "field 'client_connect_wait' must be an integer when provided.";
    } else if (maximum_network_buffer_size
               && !json_is_null(maximum_network_buffer_size)
               && !json_is_integer(maximum_network_buffer_size)) {
      error
          = "field 'maximum_network_buffer_size' must be an integer when "
            "provided.";
    }
    return std::nullopt;
  }

  StorageDaemonRequestSpec spec{};
  auto parse_string_array
      = [](json_t* value) -> std::optional<std::vector<std::string>> {
    if (!value || !json_is_array(value)) { return std::nullopt; }
    std::vector<std::string> result;
    result.reserve(json_array_size(value));
    for (size_t index = 0; index < json_array_size(value); ++index) {
      result.emplace_back(json_string_value(json_array_get(value, index)));
    }
    return result;
  };
  if (address && json_is_string(address)) {
    spec.address = std::string{json_string_value(address)};
  }
  spec.addresses = parse_string_array(addresses);
  if (source_address && json_is_string(source_address)) {
    spec.source_address = std::string{json_string_value(source_address)};
  }
  spec.source_addresses = parse_string_array(source_addresses);
  if (port && json_is_integer(port)) {
    const auto value = json_integer_value(port);
    if (value <= 0 || value > 65535) {
      error = "field 'port' must be between 1 and 65535.";
      return std::nullopt;
    }
    spec.port = static_cast<uint16_t>(value);
  }
  if (ndmp_port && json_is_integer(ndmp_port)) {
    const auto value = json_integer_value(ndmp_port);
    if (value <= 0 || value > 65535) {
      error = "field 'ndmp_port' must be between 1 and 65535.";
      return std::nullopt;
    }
    spec.ndmp_port = static_cast<uint16_t>(value);
  }
  auto parse_u32 = [&error](json_t* value, const char* field,
                            std::optional<uint32_t>& target) -> bool {
    if (!value || !json_is_integer(value)) { return true; }
    const auto raw = json_integer_value(value);
    if (raw < 0 || raw > std::numeric_limits<uint32_t>::max()) {
      error = std::string{"field '"} + field + "' must be between 0 and "
              + std::to_string(std::numeric_limits<uint32_t>::max()) + ".";
      return false;
    }
    target = static_cast<uint32_t>(raw);
    return true;
  };
  if (!parse_u32(maximum_concurrent_jobs, "maximum_concurrent_jobs",
                 spec.maximum_concurrent_jobs)
      || !parse_u32(maximum_workers_per_job, "maximum_workers_per_job",
                    spec.maximum_workers_per_job)
      || !parse_u32(absolute_job_timeout, "absolute_job_timeout",
                    spec.absolute_job_timeout)
      || !parse_u32(lmdb_threshold, "lmdb_threshold", spec.lmdb_threshold)
      || !parse_u32(ndmp_log_level, "ndmp_log_level", spec.ndmp_log_level)
      || !parse_u32(statistics_collect_interval, "statistics_collect_interval",
                    spec.statistics_collect_interval)) {
    return std::nullopt;
  }
  if (ver_id && json_is_string(ver_id)) {
    spec.ver_id = std::string{json_string_value(ver_id)};
  }
  if (log_timestamp_format && json_is_string(log_timestamp_format)) {
    spec.log_timestamp_format
        = std::string{json_string_value(log_timestamp_format)};
  }
  if (secure_erase_command && json_is_string(secure_erase_command)) {
    spec.secure_erase_command
        = std::string{json_string_value(secure_erase_command)};
  }
  if (grpc_module && json_is_string(grpc_module)) {
    spec.grpc_module = std::string{json_string_value(grpc_module)};
  }
  if (tls_cipher_list && json_is_string(tls_cipher_list)) {
    spec.tls_cipher_list = std::string{json_string_value(tls_cipher_list)};
  }
  if (tls_cipher_suites && json_is_string(tls_cipher_suites)) {
    spec.tls_cipher_suites = std::string{json_string_value(tls_cipher_suites)};
  }
  if (tls_dh_file && json_is_string(tls_dh_file)) {
    spec.tls_dh_file = std::string{json_string_value(tls_dh_file)};
  }
  if (tls_protocol && json_is_string(tls_protocol)) {
    spec.tls_protocol = std::string{json_string_value(tls_protocol)};
  }
  if (tls_ca_certificate_file && json_is_string(tls_ca_certificate_file)) {
    spec.tls_ca_certificate_file
        = std::string{json_string_value(tls_ca_certificate_file)};
  }
  if (tls_ca_certificate_dir && json_is_string(tls_ca_certificate_dir)) {
    spec.tls_ca_certificate_dir
        = std::string{json_string_value(tls_ca_certificate_dir)};
  }
  if (tls_certificate_revocation_list
      && json_is_string(tls_certificate_revocation_list)) {
    spec.tls_certificate_revocation_list
        = std::string{json_string_value(tls_certificate_revocation_list)};
  }
  if (tls_certificate && json_is_string(tls_certificate)) {
    spec.tls_certificate = std::string{json_string_value(tls_certificate)};
  }
  if (tls_key && json_is_string(tls_key)) {
    spec.tls_key = std::string{json_string_value(tls_key)};
  }
  spec.tls_allowed_cn = parse_string_array(tls_allowed_cn);
  if (pki_key_pair && json_is_string(pki_key_pair)) {
    spec.pki_key_pair = std::string{json_string_value(pki_key_pair)};
  }
  spec.pki_signers = parse_string_array(pki_signers);
  spec.pki_master_keys = parse_string_array(pki_master_keys);
  if (pki_cipher && json_is_string(pki_cipher)) {
    spec.pki_cipher = std::string{json_string_value(pki_cipher)};
  }
  if (ndmp_address && json_is_string(ndmp_address)) {
    spec.ndmp_address = std::string{json_string_value(ndmp_address)};
  }
  spec.ndmp_addresses = parse_string_array(ndmp_addresses);
  auto parse_bool = [](json_t* value, std::optional<bool>& target) {
    if (!value || !json_is_boolean(value)) { return; }
    target = json_is_true(value);
  };
  parse_bool(just_in_time_reservation, spec.just_in_time_reservation);
  parse_bool(tls_authenticate, spec.tls_authenticate);
  parse_bool(tls_enable, spec.tls_enable);
  parse_bool(tls_require, spec.tls_require);
  parse_bool(tls_verify_peer, spec.tls_verify_peer);
  parse_bool(enable_ktls, spec.enable_ktls);
  parse_bool(allow_bandwidth_bursting, spec.allow_bandwidth_bursting);
  parse_bool(pki_signatures, spec.pki_signatures);
  parse_bool(pki_encryption, spec.pki_encryption);
  parse_bool(ndmp_enable, spec.ndmp_enable);
  parse_bool(ndmp_snooping, spec.ndmp_snooping);
  parse_bool(always_use_lmdb, spec.always_use_lmdb);
  parse_bool(autoxflate_on_replication, spec.autoxflate_on_replication);
  parse_bool(collect_device_statistics, spec.collect_device_statistics);
  parse_bool(collect_job_statistics, spec.collect_job_statistics);
  parse_bool(device_reserve_by_media_type, spec.device_reserve_by_media_type);
  parse_bool(file_device_concurrent_read, spec.file_device_concurrent_read);
  auto parse_non_negative_u64
      = [&error](json_t* value, const char* field,
                 std::optional<uint64_t>& target) -> bool {
    if (!value || !json_is_integer(value)) { return true; }
    const auto raw = json_integer_value(value);
    if (raw < 0) {
      error = std::string{"field '"} + field + "' must be non-negative.";
      return false;
    }
    target = static_cast<uint64_t>(raw);
    return true;
  };
  if (!parse_non_negative_u64(maximum_bandwidth_per_job,
                              "maximum_bandwidth_per_job",
                              spec.maximum_bandwidth_per_job)) {
    return std::nullopt;
  }
  if (!parse_non_negative_u64(sd_connect_timeout, "sd_connect_timeout",
                              spec.sd_connect_timeout)
      || !parse_non_negative_u64(fd_connect_timeout, "fd_connect_timeout",
                                 spec.fd_connect_timeout)
      || !parse_non_negative_u64(heartbeat_interval, "heartbeat_interval",
                                 spec.heartbeat_interval)
      || !parse_non_negative_u64(checkpoint_interval, "checkpoint_interval",
                                 spec.checkpoint_interval)
      || !parse_non_negative_u64(client_connect_wait, "client_connect_wait",
                                 spec.client_connect_wait)) {
    return std::nullopt;
  }
  if (maximum_network_buffer_size
      && json_is_integer(maximum_network_buffer_size)) {
    const auto value = json_integer_value(maximum_network_buffer_size);
    if (value < 0 || value > std::numeric_limits<uint32_t>::max()) {
      error
          = "field 'maximum_network_buffer_size' must be between 0 and "
            "4294967295.";
      return std::nullopt;
    }
    spec.maximum_network_buffer_size = static_cast<uint32_t>(value);
  }
  if (description && json_is_string(description)) {
    spec.description = std::string{json_string_value(description)};
  }
  if (working_directory && json_is_string(working_directory)) {
    spec.working_directory = std::string{json_string_value(working_directory)};
  }
  if (plugin_directory && json_is_string(plugin_directory)) {
    spec.plugin_directory = std::string{json_string_value(plugin_directory)};
  }
  spec.plugin_names = parse_string_array(plugin_names);
#if defined(HAVE_DYNAMIC_SD_BACKENDS)
  spec.backend_directories = parse_string_array(backend_directories);
#endif
  spec.allowed_script_dirs = parse_string_array(allowed_script_dirs);
  spec.allowed_job_commands = parse_string_array(allowed_job_commands);
  if (scripts_directory && json_is_string(scripts_directory)) {
    spec.scripts_directory = std::string{json_string_value(scripts_directory)};
  }
  if (messages && json_is_string(messages)) {
    spec.messages = std::string{json_string_value(messages)};
  }
  return spec;
}

std::optional<DirectorCounterRequestSpec> ParseDirectorCounterRequest(
    std::string_view body,
    std::string& error)
{
  json_error_t json_error{};
  auto root = MakeJson(json_loadb(body.data(), body.size(), 0, &json_error));
  if (!root) {
    error = "invalid JSON body: " + std::string{json_error.text};
    return std::nullopt;
  }

  auto* minimum = json_object_get(root.get(), "minimum");
  auto* maximum = json_object_get(root.get(), "maximum");
  auto* wrap_counter = json_object_get(root.get(), "wrap_counter");
  auto* catalog = json_object_get(root.get(), "catalog");
  auto* description = json_object_get(root.get(), "description");

  auto require_integer = [&error](json_t* value, const char* field) -> bool {
    if (!value || json_is_null(value) || json_is_integer(value)) {
      return true;
    }
    error = std::string{"field '"} + field
            + "' must be an integer when provided.";
    return false;
  };
  auto require_string = [&error](json_t* value, const char* field) -> bool {
    if (!value || json_is_null(value) || json_is_string(value)) { return true; }
    error
        = std::string{"field '"} + field + "' must be a string when provided.";
    return false;
  };
  if (!require_integer(minimum, "minimum")
      || !require_integer(maximum, "maximum")
      || !require_string(wrap_counter, "wrap_counter")
      || !require_string(catalog, "catalog")
      || !require_string(description, "description")) {
    return std::nullopt;
  }

  DirectorCounterRequestSpec spec{};
  if (minimum && json_is_integer(minimum)) {
    const auto raw = json_integer_value(minimum);
    if (raw < std::numeric_limits<int32_t>::min()
        || raw > std::numeric_limits<int32_t>::max()) {
      error = "field 'minimum' must be between -2147483648 and 2147483647.";
      return std::nullopt;
    }
    spec.minimum = static_cast<int32_t>(raw);
  }
  if (maximum && json_is_integer(maximum)) {
    const auto raw = json_integer_value(maximum);
    if (raw < 0 || raw > std::numeric_limits<uint32_t>::max()) {
      error = "field 'maximum' must be between 0 and 4294967295.";
      return std::nullopt;
    }
    spec.maximum = static_cast<uint32_t>(raw);
  }
  if (wrap_counter && json_is_string(wrap_counter)) {
    spec.wrap_counter = std::string{json_string_value(wrap_counter)};
  }
  if (catalog && json_is_string(catalog)) {
    spec.catalog = std::string{json_string_value(catalog)};
  }
  if (description && json_is_string(description)) {
    spec.description = std::string{json_string_value(description)};
  }
  return spec;
}

std::optional<DirectorFilesetRequestSpec> ParseDirectorFilesetRequest(
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
  auto* ignore_fileset_changes
      = json_object_get(root.get(), "ignore_fileset_changes");
  auto* enable_vss = json_object_get(root.get(), "enable_vss");
  auto* include_blocks = json_object_get(root.get(), "include_blocks");
  auto* exclude_blocks = json_object_get(root.get(), "exclude_blocks");

  auto require_string = [&error](json_t* value, const char* field) -> bool {
    if (!value || json_is_null(value) || json_is_string(value)) { return true; }
    error
        = std::string{"field '"} + field + "' must be a string when provided.";
    return false;
  };
  auto require_boolean = [&error](json_t* value, const char* field) -> bool {
    if (!value || json_is_null(value) || json_is_boolean(value)) {
      return true;
    }
    error
        = std::string{"field '"} + field + "' must be a boolean when provided.";
    return false;
  };
  auto require_string_array
      = [&error](json_t* value, const char* field) -> bool {
    if (!value || json_is_null(value)) { return true; }
    if (!json_is_array(value)) {
      error = std::string{"field '"} + field
              + "' must be an array of strings when provided.";
      return false;
    }
    for (size_t index = 0; index < json_array_size(value); ++index) {
      if (!json_is_string(json_array_get(value, index))) {
        error = std::string{"field '"} + field + "' must contain only strings.";
        return false;
      }
    }
    return true;
  };

  if (!require_string(description, "description")
      || !require_boolean(ignore_fileset_changes, "ignore_fileset_changes")
      || !require_boolean(enable_vss, "enable_vss")
      || !require_string_array(include_blocks, "include_blocks")
      || !require_string_array(exclude_blocks, "exclude_blocks")) {
    return std::nullopt;
  }

  auto parse_string_array
      = [](json_t* value) -> std::optional<std::vector<std::string>> {
    if (!value || !json_is_array(value)) { return std::nullopt; }
    std::vector<std::string> result;
    result.reserve(json_array_size(value));
    for (size_t index = 0; index < json_array_size(value); ++index) {
      result.emplace_back(json_string_value(json_array_get(value, index)));
    }
    return result;
  };

  DirectorFilesetRequestSpec spec{};
  if (description && json_is_string(description)) {
    spec.description = std::string{json_string_value(description)};
  }
  if (ignore_fileset_changes && json_is_boolean(ignore_fileset_changes)) {
    spec.ignore_fileset_changes = json_is_true(ignore_fileset_changes);
  }
  if (enable_vss && json_is_boolean(enable_vss)) {
    spec.enable_vss = json_is_true(enable_vss);
  }
  spec.include_blocks = parse_string_array(include_blocks);
  spec.exclude_blocks = parse_string_array(exclude_blocks);
  return spec;
}

std::optional<DirectorJobRequestSpec> ParseDirectorJobRequest(
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
  auto* type = json_object_get(root.get(), "type");
  auto* level = json_object_get(root.get(), "level");
  auto* messages = json_object_get(root.get(), "messages");
  auto* storages = json_object_get(root.get(), "storages");
  auto* pool = json_object_get(root.get(), "pool");
  auto* full_backup_pool = json_object_get(root.get(), "full_backup_pool");
  auto* virtual_full_backup_pool
      = json_object_get(root.get(), "virtual_full_backup_pool");
  auto* incremental_backup_pool
      = json_object_get(root.get(), "incremental_backup_pool");
  auto* differential_backup_pool
      = json_object_get(root.get(), "differential_backup_pool");
  auto* next_pool = json_object_get(root.get(), "next_pool");
  auto* client = json_object_get(root.get(), "client");
  auto* fileset = json_object_get(root.get(), "fileset");
  auto* schedule = json_object_get(root.get(), "schedule");
  auto* verify_job = json_object_get(root.get(), "verify_job");
  auto* catalog = json_object_get(root.get(), "catalog");
  auto* jobdefs = json_object_get(root.get(), "jobdefs");
  auto* where = json_object_get(root.get(), "where");
  auto* priority = json_object_get(root.get(), "priority");
  auto* enabled = json_object_get(root.get(), "enabled");

  auto require_string = [&error](json_t* value, const char* field) -> bool {
    if (!value || json_is_null(value) || json_is_string(value)) { return true; }
    error
        = std::string{"field '"} + field + "' must be a string when provided.";
    return false;
  };
  auto require_boolean = [&error](json_t* value, const char* field) -> bool {
    if (!value || json_is_null(value) || json_is_boolean(value)) {
      return true;
    }
    error
        = std::string{"field '"} + field + "' must be a boolean when provided.";
    return false;
  };
  auto require_integer = [&error](json_t* value, const char* field) -> bool {
    if (!value || json_is_null(value) || json_is_integer(value)) {
      return true;
    }
    error = std::string{"field '"} + field
            + "' must be an integer when provided.";
    return false;
  };
  auto require_string_array
      = [&error](json_t* value, const char* field) -> bool {
    if (!value || json_is_null(value)) { return true; }
    if (!json_is_array(value)) {
      error = std::string{"field '"} + field
              + "' must be an array of strings when provided.";
      return false;
    }
    for (size_t index = 0; index < json_array_size(value); ++index) {
      if (!json_is_string(json_array_get(value, index))) {
        error = std::string{"field '"} + field + "' must contain only strings.";
        return false;
      }
    }
    return true;
  };

  if (!require_string(description, "description")
      || !require_string(type, "type") || !require_string(level, "level")
      || !require_string(messages, "messages")
      || !require_string_array(storages, "storages")
      || !require_string(pool, "pool")
      || !require_string(full_backup_pool, "full_backup_pool")
      || !require_string(virtual_full_backup_pool, "virtual_full_backup_pool")
      || !require_string(incremental_backup_pool, "incremental_backup_pool")
      || !require_string(differential_backup_pool, "differential_backup_pool")
      || !require_string(next_pool, "next_pool")
      || !require_string(client, "client")
      || !require_string(fileset, "fileset")
      || !require_string(schedule, "schedule")
      || !require_string(verify_job, "verify_job")
      || !require_string(catalog, "catalog")
      || !require_string(jobdefs, "jobdefs") || !require_string(where, "where")
      || !require_integer(priority, "priority")
      || !require_boolean(enabled, "enabled")) {
    return std::nullopt;
  }

  auto parse_string_array
      = [](json_t* value) -> std::optional<std::vector<std::string>> {
    if (!value || !json_is_array(value)) { return std::nullopt; }
    std::vector<std::string> result;
    result.reserve(json_array_size(value));
    for (size_t index = 0; index < json_array_size(value); ++index) {
      result.emplace_back(json_string_value(json_array_get(value, index)));
    }
    return result;
  };

  DirectorJobRequestSpec spec{};
  if (description && json_is_string(description)) {
    spec.description = std::string{json_string_value(description)};
  }
  if (type && json_is_string(type)) {
    spec.type = std::string{json_string_value(type)};
  }
  if (level && json_is_string(level)) {
    spec.level = std::string{json_string_value(level)};
  }
  if (messages && json_is_string(messages)) {
    spec.messages = std::string{json_string_value(messages)};
  }
  spec.storages = parse_string_array(storages);
  if (pool && json_is_string(pool)) {
    spec.pool = std::string{json_string_value(pool)};
  }
  if (full_backup_pool && json_is_string(full_backup_pool)) {
    spec.full_backup_pool = std::string{json_string_value(full_backup_pool)};
  }
  if (virtual_full_backup_pool && json_is_string(virtual_full_backup_pool)) {
    spec.virtual_full_backup_pool
        = std::string{json_string_value(virtual_full_backup_pool)};
  }
  if (incremental_backup_pool && json_is_string(incremental_backup_pool)) {
    spec.incremental_backup_pool
        = std::string{json_string_value(incremental_backup_pool)};
  }
  if (differential_backup_pool && json_is_string(differential_backup_pool)) {
    spec.differential_backup_pool
        = std::string{json_string_value(differential_backup_pool)};
  }
  if (next_pool && json_is_string(next_pool)) {
    spec.next_pool = std::string{json_string_value(next_pool)};
  }
  if (client && json_is_string(client)) {
    spec.client = std::string{json_string_value(client)};
  }
  if (fileset && json_is_string(fileset)) {
    spec.fileset = std::string{json_string_value(fileset)};
  }
  if (schedule && json_is_string(schedule)) {
    spec.schedule = std::string{json_string_value(schedule)};
  }
  if (verify_job && json_is_string(verify_job)) {
    spec.verify_job = std::string{json_string_value(verify_job)};
  }
  if (catalog && json_is_string(catalog)) {
    spec.catalog = std::string{json_string_value(catalog)};
  }
  if (jobdefs && json_is_string(jobdefs)) {
    spec.jobdefs = std::string{json_string_value(jobdefs)};
  }
  if (where && json_is_string(where)) {
    spec.where = std::string{json_string_value(where)};
  }
  if (priority && json_is_integer(priority)) {
    const auto raw = json_integer_value(priority);
    if (raw < std::numeric_limits<int32_t>::min()
        || raw > std::numeric_limits<int32_t>::max()) {
      error = "field 'priority' must be between -2147483648 and 2147483647.";
      return std::nullopt;
    }
    spec.priority = static_cast<int32_t>(raw);
  }
  if (enabled && json_is_boolean(enabled)) {
    spec.enabled = json_is_true(enabled);
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

  if (path_parts.size() == 5 && path_parts[3] == "clients"
      && request.method() == http::verb::put) {
    return HandleDeploymentClientDaemonPutRequest(state, request, path_parts[2],
                                                  path_parts[4]);
  }

  if (path_parts.size() == 5 && path_parts[3] == "directors"
      && request.method() == http::verb::put) {
    return HandleDeploymentDirectorDaemonPutRequest(
        state, request, path_parts[2], path_parts[4]);
  }

  if (path_parts.size() == 5 && path_parts[3] == "storages"
      && request.method() == http::verb::put) {
    return HandleDeploymentStorageDaemonPutRequest(
        state, request, path_parts[2], path_parts[4]);
  }

  if (path_parts.size() == 7 && path_parts[3] == "clients"
      && path_parts[5] == "messages" && request.method() == http::verb::put) {
    return HandleDeploymentClientMessagesPutRequest(
        state, request, path_parts[2], path_parts[4], path_parts[6]);
  }
  if (path_parts.size() == 7 && path_parts[3] == "clients"
      && path_parts[5] == "messages"
      && request.method() == http::verb::delete_) {
    return HandleDeploymentClientMessagesDeleteRequest(
        state, path_parts[2], path_parts[4], path_parts[6]);
  }

  if (path_parts.size() == 7 && path_parts[3] == "clients"
      && path_parts[5] == "directors" && request.method() == http::verb::put) {
    return HandleDeploymentClientDirectorStubPutRequest(
        state, request, path_parts[2], path_parts[4], path_parts[6]);
  }
  if (path_parts.size() == 7 && path_parts[3] == "clients"
      && path_parts[5] == "directors"
      && request.method() == http::verb::delete_) {
    return HandleDeploymentClientDirectorStubDeleteRequest(
        state, path_parts[2], path_parts[4], path_parts[6]);
  }

  if (path_parts.size() == 7 && path_parts[3] == "storages"
      && path_parts[5] == "directors" && request.method() == http::verb::put) {
    return HandleDeploymentStorageDirectorPutRequest(
        state, request, path_parts[2], path_parts[4], path_parts[6]);
  }
  if (path_parts.size() == 7 && path_parts[3] == "storages"
      && path_parts[5] == "directors"
      && request.method() == http::verb::delete_) {
    return HandleDeploymentStorageDirectorDeleteRequest(
        state, path_parts[2], path_parts[4], path_parts[6]);
  }

  if (path_parts.size() == 7 && path_parts[3] == "storages"
      && path_parts[5] == "devices" && request.method() == http::verb::put) {
    return HandleDeploymentStorageDevicePutRequest(
        state, request, path_parts[2], path_parts[4], path_parts[6]);
  }
  if (path_parts.size() == 7 && path_parts[3] == "storages"
      && path_parts[5] == "devices"
      && request.method() == http::verb::delete_) {
    return HandleDeploymentStorageDeviceDeleteRequest(
        state, path_parts[2], path_parts[4], path_parts[6]);
  }

  if (path_parts.size() == 7 && path_parts[3] == "storages"
      && path_parts[5] == "messages" && request.method() == http::verb::put) {
    return HandleDeploymentStorageMessagesPutRequest(
        state, request, path_parts[2], path_parts[4], path_parts[6]);
  }
  if (path_parts.size() == 7 && path_parts[3] == "storages"
      && path_parts[5] == "messages"
      && request.method() == http::verb::delete_) {
    return HandleDeploymentStorageMessagesDeleteRequest(
        state, path_parts[2], path_parts[4], path_parts[6]);
  }
  if (path_parts.size() == 7 && path_parts[3] == "storages"
      && path_parts[5] == "ndmp" && request.method() == http::verb::put) {
    return HandleDeploymentStorageNdmpPutRequest(state, request, path_parts[2],
                                                 path_parts[4], path_parts[6]);
  }
  if (path_parts.size() == 7 && path_parts[3] == "storages"
      && path_parts[5] == "ndmp" && request.method() == http::verb::delete_) {
    return HandleDeploymentStorageNdmpDeleteRequest(
        state, path_parts[2], path_parts[4], path_parts[6]);
  }
  if (path_parts.size() == 7 && path_parts[3] == "storages"
      && path_parts[5] == "autochangers"
      && request.method() == http::verb::put) {
    return HandleDeploymentStorageAutochangerPutRequest(
        state, request, path_parts[2], path_parts[4], path_parts[6]);
  }
  if (path_parts.size() == 7 && path_parts[3] == "storages"
      && path_parts[5] == "autochangers"
      && request.method() == http::verb::delete_) {
    return HandleDeploymentStorageAutochangerDeleteRequest(
        state, path_parts[2], path_parts[4], path_parts[6]);
  }

  if (path_parts.size() == 7 && path_parts[3] == "consoles"
      && path_parts[5] == "consoles" && request.method() == http::verb::put) {
    return HandleDeploymentConsoleConsolePutRequest(
        state, request, path_parts[2], path_parts[4], path_parts[6]);
  }
  if (path_parts.size() == 7 && path_parts[3] == "consoles"
      && path_parts[5] == "consoles"
      && request.method() == http::verb::delete_) {
    return HandleDeploymentConsoleConsoleDeleteRequest(
        state, path_parts[2], path_parts[4], path_parts[6]);
  }
  if (path_parts.size() == 7 && path_parts[3] == "consoles"
      && path_parts[5] == "directors" && request.method() == http::verb::put) {
    return HandleDeploymentConsoleDirectorPutRequest(
        state, request, path_parts[2], path_parts[4], path_parts[6]);
  }
  if (path_parts.size() == 7 && path_parts[3] == "consoles"
      && path_parts[5] == "directors"
      && request.method() == http::verb::delete_) {
    return HandleDeploymentConsoleDirectorDeleteRequest(
        state, path_parts[2], path_parts[4], path_parts[6]);
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
      && path_parts[5] == "messages" && request.method() == http::verb::put) {
    return HandleDeploymentDirectorMessagesPutRequest(
        state, request, path_parts[2], path_parts[4], path_parts[6]);
  }
  if (path_parts.size() == 7 && path_parts[3] == "directors"
      && path_parts[5] == "messages"
      && request.method() == http::verb::delete_) {
    return HandleDeploymentDirectorMessagesDeleteRequest(
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

  if (path_parts.size() == 7 && path_parts[3] == "directors"
      && path_parts[5] == "counters" && request.method() == http::verb::put) {
    return HandleDeploymentDirectorCounterPutRequest(
        state, request, path_parts[2], path_parts[4], path_parts[6]);
  }
  if (path_parts.size() == 7 && path_parts[3] == "directors"
      && path_parts[5] == "counters"
      && request.method() == http::verb::delete_) {
    return HandleDeploymentDirectorCounterDeleteRequest(
        state, path_parts[2], path_parts[4], path_parts[6]);
  }

  if (path_parts.size() == 7 && path_parts[3] == "directors"
      && path_parts[5] == "filesets" && request.method() == http::verb::put) {
    return HandleDeploymentDirectorFilesetPutRequest(
        state, request, path_parts[2], path_parts[4], path_parts[6]);
  }
  if (path_parts.size() == 7 && path_parts[3] == "directors"
      && path_parts[5] == "filesets"
      && request.method() == http::verb::delete_) {
    return HandleDeploymentDirectorFilesetDeleteRequest(
        state, path_parts[2], path_parts[4], path_parts[6]);
  }

  if (path_parts.size() == 7 && path_parts[3] == "directors"
      && path_parts[5] == "jobs" && request.method() == http::verb::put) {
    return HandleDeploymentDirectorJobPutRequest(state, request, path_parts[2],
                                                 path_parts[4], path_parts[6]);
  }
  if (path_parts.size() == 7 && path_parts[3] == "directors"
      && path_parts[5] == "jobs" && request.method() == http::verb::delete_) {
    return HandleDeploymentDirectorJobDeleteRequest(
        state, path_parts[2], path_parts[4], path_parts[6]);
  }

  if (path_parts.size() == 7 && path_parts[3] == "directors"
      && path_parts[5] == "jobdefs" && request.method() == http::verb::put) {
    return HandleDeploymentDirectorJobDefsPutRequest(
        state, request, path_parts[2], path_parts[4], path_parts[6]);
  }
  if (path_parts.size() == 7 && path_parts[3] == "directors"
      && path_parts[5] == "jobdefs"
      && request.method() == http::verb::delete_) {
    return HandleDeploymentDirectorJobDefsDeleteRequest(
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
  const auto raw_path_parts = SplitPath(
      std::string_view{request.target().data(), request.target().size()});
  const auto path_parts = StripRoutePrefix(raw_path_parts);

  if (request.method() == http::verb::get
      && (target == "/" || (path_parts.size() == 1 && path_parts[0] == "ui"))) {
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
