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

#ifndef BAREOS_TOOLS_BCONFIG_SERVICE_H_
#define BAREOS_TOOLS_BCONFIG_SERVICE_H_

#include "tools/bconfig_lib.h"

#include <cstdint>
#include <filesystem>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace bconfig::service {

enum class WorkflowMode
{
  kDirectCommit,
  kReview
};

enum class JobStatus
{
  kQueued,
  kRunning,
  kSucceeded,
  kFailed
};

template <typename T> struct OperationResult {
  std::optional<T> value{};
  std::string error{};

  explicit operator bool() const { return value.has_value(); }
};

struct DeploymentSpec {
  std::string id{};
  std::string name{};
  std::filesystem::path repository_path{};
  WorkflowMode workflow_mode{WorkflowMode::kDirectCommit};
};

struct DeploymentRecord {
  std::string id{};
  std::string name{};
  std::filesystem::path repository_path{};
  WorkflowMode workflow_mode{WorkflowMode::kDirectCommit};
  std::string created_at{};
};

struct DeploymentImportRecord {
  std::string job_id{};
  std::string component{};
  std::string resource_name{};
  std::optional<std::string> source_path{};
  std::string destination_path{};
  std::string imported_at{};
};

struct DeploymentGitStatusRecord {
  bool initialized{false};
  std::string branch{};
  bool clean{true};
  bool has_staged_changes{false};
  bool has_untracked_files{false};
  std::vector<std::string> entries{};
};

struct DeploymentDiffPreviewRecord {
  bool initialized{false};
  bool has_changes{false};
  std::string diff{};
  std::vector<std::string> untracked_files{};
};

struct DeploymentConfigRecord {
  bconfig::Component component{bconfig::Component::kDirector};
  std::string name{};
  std::filesystem::path path{};
};

struct ClientDirectorStubSpec {
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

struct ClientMessagesResourceSpec {
  std::optional<std::string> description{};
  std::optional<std::vector<std::string>> entries{};
};

struct ClientDaemonResourceSpec {
  std::optional<std::string> address{};
  std::optional<std::vector<std::string>> addresses{};
  std::optional<std::string> source_address{};
  std::optional<std::vector<std::string>> source_addresses{};
  std::optional<uint16_t> port{};
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
  std::optional<std::string> ver_id{};
  std::optional<std::string> log_timestamp_format{};
  std::optional<uint64_t> maximum_bandwidth_per_job{};
  std::optional<std::string> secure_erase_command{};
  std::optional<std::string> grpc_module{};
  std::optional<bool> enable_ktls{};
  std::optional<uint64_t> sd_connect_timeout{};
  std::optional<uint64_t> heartbeat_interval{};
  std::optional<uint32_t> maximum_network_buffer_size{};
  std::optional<std::string> description{};
  std::optional<std::string> working_directory{};
  std::optional<std::string> plugin_directory{};
  std::optional<std::vector<std::string>> plugin_names{};
  std::optional<std::vector<std::string>> allowed_script_dirs{};
  std::optional<std::vector<std::string>> allowed_job_commands{};
  std::optional<std::string> scripts_directory{};
  std::optional<std::string> messages{};
};

struct DirectorDaemonResourceSpec {
  std::optional<std::string> address{};
  std::optional<std::vector<std::string>> addresses{};
  std::optional<std::string> source_address{};
  std::optional<std::vector<std::string>> source_addresses{};
  std::optional<uint16_t> port{};
  std::optional<uint32_t> maximum_concurrent_jobs{};
  std::optional<uint32_t> absolute_job_timeout{};
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
  std::optional<std::string> ver_id{};
  std::optional<std::string> log_timestamp_format{};
  std::optional<std::string> secure_erase_command{};
  std::optional<bool> enable_ktls{};
  std::optional<uint64_t> fd_connect_timeout{};
  std::optional<uint64_t> sd_connect_timeout{};
  std::optional<uint64_t> heartbeat_interval{};
  std::optional<std::string> description{};
  std::optional<std::string> working_directory{};
  std::optional<std::string> plugin_directory{};
  std::optional<std::vector<std::string>> plugin_names{};
  std::optional<std::string> scripts_directory{};
  std::optional<std::string> messages{};
};

struct DirectorClientResourceSpec {
  std::optional<std::string> address{};
  std::optional<std::string> lan_address{};
  std::optional<uint16_t> port{};
  std::optional<std::string> password{};
  std::optional<bool> enabled{};
  std::optional<bool> passive{};
  std::optional<bool> strict_quotas{};
  std::optional<bool> quota_include_failed_jobs{};
  std::optional<uint64_t> soft_quota{};
  std::optional<uint64_t> hard_quota{};
  std::optional<bool> connection_from_director_to_client{};
  std::optional<bool> connection_from_client_to_director{};
  std::optional<uint64_t> heartbeat_interval{};
  std::optional<uint64_t> maximum_bandwidth_per_job{};
  std::optional<std::string> description{};
};

struct DirectorStorageResourceSpec {
  std::optional<std::string> address{};
  std::optional<std::string> lan_address{};
  std::optional<uint16_t> port{};
  std::optional<std::string> password{};
  std::optional<std::string> device{};
  std::optional<std::string> media_type{};
  std::optional<bool> enabled{};
  std::optional<bool> allow_compression{};
  std::optional<uint64_t> heartbeat_interval{};
  std::optional<uint64_t> cache_status_interval{};
  std::optional<uint64_t> maximum_bandwidth_per_job{};
  std::optional<std::string> archive_device{};
  std::optional<std::string> device_type{};
  std::optional<std::string> description{};
};

struct DirectorConsoleResourceSpec {
  std::optional<std::string> password{};
  std::optional<std::string> description{};
  std::optional<bool> use_pam_authentication{};
};

struct ConsoleConsoleResourceSpec {
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

struct ConsoleDirectorResourceSpec {
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

struct DirectorUserResourceSpec {
  std::optional<std::string> description{};
};

struct DirectorProfileResourceSpec {
  std::optional<std::string> description{};
};

struct DirectorPoolResourceSpec {
  std::optional<std::string> pool_type{};
  std::optional<std::string> label_format{};
  std::optional<uint32_t> maximum_volumes{};
  std::optional<uint64_t> maximum_volume_bytes{};
  std::optional<uint64_t> volume_retention{};
  std::optional<bool> auto_prune{};
  std::optional<bool> recycle{};
  std::optional<std::string> description{};
};

struct DirectorCatalogResourceSpec {
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

struct DirectorMessagesResourceSpec {
  std::optional<std::string> description{};
  std::optional<std::vector<std::string>> entries{};
};

struct DirectorScheduleResourceSpec {
  std::optional<std::string> description{};
  std::optional<bool> enabled{};
  std::optional<std::vector<std::string>> run_entries{};
};

struct DirectorCounterResourceSpec {
  std::optional<int32_t> minimum{};
  std::optional<uint32_t> maximum{};
  std::optional<std::string> wrap_counter{};
  std::optional<std::string> catalog{};
  std::optional<std::string> description{};
};

struct DirectorFilesetResourceSpec {
  std::optional<std::string> description{};
  std::optional<bool> ignore_fileset_changes{};
  std::optional<bool> enable_vss{};
  std::optional<std::vector<std::string>> include_blocks{};
  std::optional<std::vector<std::string>> exclude_blocks{};
};

struct DirectorJobResourceSpec {
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

struct DirectorJobDefsResourceSpec {
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

struct StorageMessagesResourceSpec {
  std::optional<std::string> description{};
  std::optional<std::vector<std::string>> entries{};
};

struct StorageDirectorResourceSpec {
  std::optional<std::string> password{};
  std::optional<std::string> description{};
  std::optional<bool> monitor{};
  std::optional<uint64_t> maximum_bandwidth_per_job{};
};

struct StorageDeviceResourceSpec {
  std::optional<std::string> media_type{};
  std::optional<std::string> archive_device{};
  std::optional<std::string> device_type{};
  std::optional<std::string> description{};
};

struct StorageNdmpResourceSpec {
  std::optional<std::string> username{};
  std::optional<std::string> password{};
  std::optional<std::string> auth_type{};
  std::optional<uint32_t> log_level{};
};

struct StorageAutochangerResourceSpec {
  std::optional<std::vector<std::string>> devices{};
  std::optional<std::string> changer_device{};
  std::optional<std::string> changer_command{};
  std::optional<std::string> description{};
};

struct StorageDaemonResourceSpec {
  std::optional<std::string> address{};
  std::optional<std::vector<std::string>> addresses{};
  std::optional<std::string> source_address{};
  std::optional<std::vector<std::string>> source_addresses{};
  std::optional<uint16_t> port{};
  std::optional<bool> just_in_time_reservation{};
  std::optional<uint32_t> maximum_concurrent_jobs{};
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
  std::optional<std::string> scripts_directory{};
  std::optional<std::string> messages{};
};

struct JobSpec {
  std::string type{};
  std::optional<std::string> deployment_id{};
  std::optional<std::string> source_component{};
  std::optional<std::string> source_path{};
  std::optional<std::string> commit_message{};
};

struct JobRecord {
  std::string id{};
  std::string type{};
  std::optional<std::string> deployment_id{};
  std::optional<std::string> source_component{};
  std::optional<std::string> source_path{};
  std::optional<std::string> commit_message{};
  JobStatus status{JobStatus::kQueued};
  std::string created_at{};
  std::string updated_at{};
  std::optional<std::string> started_at{};
  std::optional<std::string> finished_at{};
  std::optional<std::string> last_error{};
  std::vector<std::string> logs{};
};

class RepositoryLayout {
 public:
  static std::filesystem::path ManifestPath(
      const std::filesystem::path& repository_root);
  static std::filesystem::path ServiceDirectory(
      const std::filesystem::path& repository_root);
  static std::filesystem::path OwnershipPath(
      const std::filesystem::path& repository_root);
  static std::filesystem::path ImportStatePath(
      const std::filesystem::path& repository_root);
  static std::filesystem::path DirectorsDirectory(
      const std::filesystem::path& repository_root);
  static std::filesystem::path StoragesDirectory(
      const std::filesystem::path& repository_root);
  static std::filesystem::path ClientsDirectory(
      const std::filesystem::path& repository_root);
  static std::filesystem::path ConsolesDirectory(
      const std::filesystem::path& repository_root);
  static std::vector<std::filesystem::path> Directories(
      const std::filesystem::path& repository_root);
};

std::filesystem::path ExpandUserPath(const std::filesystem::path& path);
std::filesystem::path DefaultStorageBasePath();
std::filesystem::path DefaultDeploymentRepositoryPath(
    std::string_view deployment_id);

std::string_view ToString(WorkflowMode mode);
std::optional<WorkflowMode> ParseWorkflowMode(std::string_view value);
std::string_view ToString(JobStatus status);
std::optional<JobStatus> ParseJobStatus(std::string_view value);

class ServiceState {
 public:
  explicit ServiceState(std::filesystem::path state_directory = {});
  ~ServiceState();

  OperationResult<DeploymentRecord> CreateDeployment(
      const DeploymentSpec& spec);
  std::vector<DeploymentRecord> ListDeployments() const;
  std::optional<DeploymentRecord> GetDeployment(std::string_view id) const;
  OperationResult<std::vector<DeploymentConfigRecord>> ListDeploymentConfigs(
      std::string_view deployment_id,
      bconfig::Component component) const;
  OperationResult<DeploymentConfigRecord> GetDeploymentConfig(
      std::string_view deployment_id,
      bconfig::Component component,
      std::string_view name) const;
  OperationResult<DeploymentConfigRecord> UpsertClientDirectorStub(
      std::string_view deployment_id,
      std::string_view client_name,
      std::string_view director_name,
      const ClientDirectorStubSpec& spec) const;
  OperationResult<std::optional<DeploymentConfigRecord>>
  DeleteClientDirectorStub(std::string_view deployment_id,
                           std::string_view client_name,
                           std::string_view director_name) const;
  OperationResult<DeploymentConfigRecord> UpsertClientMessagesResource(
      std::string_view deployment_id,
      std::string_view client_name,
      std::string_view messages_name,
      const ClientMessagesResourceSpec& spec) const;
  OperationResult<DeploymentConfigRecord> DeleteClientMessagesResource(
      std::string_view deployment_id,
      std::string_view client_name,
      std::string_view messages_name) const;
  OperationResult<DeploymentConfigRecord> UpsertClientDaemonResource(
      std::string_view deployment_id,
      std::string_view client_name,
      const ClientDaemonResourceSpec& spec) const;
  OperationResult<DeploymentConfigRecord> UpsertDirectorDaemonResource(
      std::string_view deployment_id,
      std::string_view director_name,
      const DirectorDaemonResourceSpec& spec) const;
  OperationResult<DeploymentConfigRecord> UpsertDirectorClientResource(
      std::string_view deployment_id,
      std::string_view director_name,
      std::string_view client_name,
      const DirectorClientResourceSpec& spec) const;
  OperationResult<DeploymentConfigRecord> DeleteDirectorClientResource(
      std::string_view deployment_id,
      std::string_view director_name,
      std::string_view client_name) const;
  OperationResult<DeploymentConfigRecord> UpsertDirectorStorageResource(
      std::string_view deployment_id,
      std::string_view director_name,
      std::string_view storage_name,
      const DirectorStorageResourceSpec& spec) const;
  OperationResult<DeploymentConfigRecord> DeleteDirectorStorageResource(
      std::string_view deployment_id,
      std::string_view director_name,
      std::string_view storage_name) const;
  OperationResult<DeploymentConfigRecord> UpsertDirectorConsoleResource(
      std::string_view deployment_id,
      std::string_view director_name,
      std::string_view console_name,
      const DirectorConsoleResourceSpec& spec) const;
  OperationResult<DeploymentConfigRecord> DeleteDirectorConsoleResource(
      std::string_view deployment_id,
      std::string_view director_name,
      std::string_view console_name) const;
  OperationResult<DeploymentConfigRecord> UpsertConsoleConsoleResource(
      std::string_view deployment_id,
      std::string_view console_config_name,
      std::string_view console_name,
      const ConsoleConsoleResourceSpec& spec) const;
  OperationResult<DeploymentConfigRecord> DeleteConsoleConsoleResource(
      std::string_view deployment_id,
      std::string_view console_config_name,
      std::string_view console_name) const;
  OperationResult<DeploymentConfigRecord> UpsertConsoleDirectorResource(
      std::string_view deployment_id,
      std::string_view console_config_name,
      std::string_view director_name,
      const ConsoleDirectorResourceSpec& spec) const;
  OperationResult<DeploymentConfigRecord> DeleteConsoleDirectorResource(
      std::string_view deployment_id,
      std::string_view console_config_name,
      std::string_view director_name) const;
  OperationResult<DeploymentConfigRecord> UpsertDirectorUserResource(
      std::string_view deployment_id,
      std::string_view director_name,
      std::string_view user_name,
      const DirectorUserResourceSpec& spec) const;
  OperationResult<DeploymentConfigRecord> DeleteDirectorUserResource(
      std::string_view deployment_id,
      std::string_view director_name,
      std::string_view user_name) const;
  OperationResult<DeploymentConfigRecord> UpsertDirectorProfileResource(
      std::string_view deployment_id,
      std::string_view director_name,
      std::string_view profile_name,
      const DirectorProfileResourceSpec& spec) const;
  OperationResult<DeploymentConfigRecord> DeleteDirectorProfileResource(
      std::string_view deployment_id,
      std::string_view director_name,
      std::string_view profile_name) const;
  OperationResult<DeploymentConfigRecord> UpsertDirectorPoolResource(
      std::string_view deployment_id,
      std::string_view director_name,
      std::string_view pool_name,
      const DirectorPoolResourceSpec& spec) const;
  OperationResult<DeploymentConfigRecord> DeleteDirectorPoolResource(
      std::string_view deployment_id,
      std::string_view director_name,
      std::string_view pool_name) const;
  OperationResult<DeploymentConfigRecord> UpsertDirectorCatalogResource(
      std::string_view deployment_id,
      std::string_view director_name,
      std::string_view catalog_name,
      const DirectorCatalogResourceSpec& spec) const;
  OperationResult<DeploymentConfigRecord> DeleteDirectorCatalogResource(
      std::string_view deployment_id,
      std::string_view director_name,
      std::string_view catalog_name) const;
  OperationResult<DeploymentConfigRecord> UpsertDirectorMessagesResource(
      std::string_view deployment_id,
      std::string_view director_name,
      std::string_view messages_name,
      const DirectorMessagesResourceSpec& spec) const;
  OperationResult<DeploymentConfigRecord> DeleteDirectorMessagesResource(
      std::string_view deployment_id,
      std::string_view director_name,
      std::string_view messages_name) const;
  OperationResult<DeploymentConfigRecord> UpsertDirectorScheduleResource(
      std::string_view deployment_id,
      std::string_view director_name,
      std::string_view schedule_name,
      const DirectorScheduleResourceSpec& spec) const;
  OperationResult<DeploymentConfigRecord> DeleteDirectorScheduleResource(
      std::string_view deployment_id,
      std::string_view director_name,
      std::string_view schedule_name) const;
  OperationResult<DeploymentConfigRecord> UpsertDirectorCounterResource(
      std::string_view deployment_id,
      std::string_view director_name,
      std::string_view counter_name,
      const DirectorCounterResourceSpec& spec) const;
  OperationResult<DeploymentConfigRecord> DeleteDirectorCounterResource(
      std::string_view deployment_id,
      std::string_view director_name,
      std::string_view counter_name) const;
  OperationResult<DeploymentConfigRecord> UpsertDirectorFilesetResource(
      std::string_view deployment_id,
      std::string_view director_name,
      std::string_view fileset_name,
      const DirectorFilesetResourceSpec& spec) const;
  OperationResult<DeploymentConfigRecord> DeleteDirectorFilesetResource(
      std::string_view deployment_id,
      std::string_view director_name,
      std::string_view fileset_name) const;
  OperationResult<DeploymentConfigRecord> UpsertDirectorJobResource(
      std::string_view deployment_id,
      std::string_view director_name,
      std::string_view job_name,
      const DirectorJobResourceSpec& spec) const;
  OperationResult<DeploymentConfigRecord> DeleteDirectorJobResource(
      std::string_view deployment_id,
      std::string_view director_name,
      std::string_view job_name) const;
  OperationResult<DeploymentConfigRecord> UpsertDirectorJobDefsResource(
      std::string_view deployment_id,
      std::string_view director_name,
      std::string_view jobdefs_name,
      const DirectorJobDefsResourceSpec& spec) const;
  OperationResult<DeploymentConfigRecord> DeleteDirectorJobDefsResource(
      std::string_view deployment_id,
      std::string_view director_name,
      std::string_view jobdefs_name) const;
  OperationResult<DeploymentConfigRecord> UpsertStorageMessagesResource(
      std::string_view deployment_id,
      std::string_view storage_name,
      std::string_view messages_name,
      const StorageMessagesResourceSpec& spec) const;
  OperationResult<DeploymentConfigRecord> DeleteStorageMessagesResource(
      std::string_view deployment_id,
      std::string_view storage_name,
      std::string_view messages_name) const;
  OperationResult<DeploymentConfigRecord> UpsertStorageDirectorResource(
      std::string_view deployment_id,
      std::string_view storage_name,
      std::string_view director_name,
      const StorageDirectorResourceSpec& spec) const;
  OperationResult<DeploymentConfigRecord> DeleteStorageDirectorResource(
      std::string_view deployment_id,
      std::string_view storage_name,
      std::string_view director_name) const;
  OperationResult<DeploymentConfigRecord> UpsertStorageDeviceResource(
      std::string_view deployment_id,
      std::string_view storage_name,
      std::string_view device_name,
      const StorageDeviceResourceSpec& spec) const;
  OperationResult<DeploymentConfigRecord> DeleteStorageDeviceResource(
      std::string_view deployment_id,
      std::string_view storage_name,
      std::string_view device_name) const;
  OperationResult<DeploymentConfigRecord> UpsertStorageNdmpResource(
      std::string_view deployment_id,
      std::string_view storage_name,
      std::string_view ndmp_name,
      const StorageNdmpResourceSpec& spec) const;
  OperationResult<DeploymentConfigRecord> DeleteStorageNdmpResource(
      std::string_view deployment_id,
      std::string_view storage_name,
      std::string_view ndmp_name) const;
  OperationResult<DeploymentConfigRecord> UpsertStorageAutochangerResource(
      std::string_view deployment_id,
      std::string_view storage_name,
      std::string_view autochanger_name,
      const StorageAutochangerResourceSpec& spec) const;
  OperationResult<DeploymentConfigRecord> DeleteStorageAutochangerResource(
      std::string_view deployment_id,
      std::string_view storage_name,
      std::string_view autochanger_name) const;
  OperationResult<DeploymentConfigRecord> UpsertStorageDaemonResource(
      std::string_view deployment_id,
      std::string_view storage_name,
      const StorageDaemonResourceSpec& spec) const;
  OperationResult<std::vector<DeploymentImportRecord>> ListDeploymentImports(
      std::string_view deployment_id) const;
  OperationResult<DeploymentGitStatusRecord> GetDeploymentGitStatus(
      std::string_view deployment_id) const;
  OperationResult<DeploymentDiffPreviewRecord> GetDeploymentDiffPreview(
      std::string_view deployment_id) const;

  OperationResult<JobRecord> CreateJob(const JobSpec& spec);
  std::vector<JobRecord> ListJobs() const;
  std::optional<JobRecord> GetJob(std::string_view id) const;
  const std::filesystem::path& GetStateDirectory() const;
  bool HasPersistentState() const;

 private:
  static std::string MakeTimestamp();
  static bool IsValidIdentifier(std::string_view id);
  static std::string JsonEscape(std::string_view value);
  static std::string ManifestJson(const DeploymentRecord& record);
  static std::string EmptyObjectJson();
  static std::string SerializeState(
      uint64_t next_job_id,
      const std::vector<DeploymentRecord>& deployments,
      const std::vector<JobRecord>& jobs);
  static std::optional<std::string> InitializeRepositoryLayout(
      const DeploymentRecord& record);
  void WorkerLoop();
  std::optional<JobRecord> GetNextQueuedJobLocked();
  void MarkJobRunning(const std::string& id, std::string log_message);
  void MarkJobFinished(const std::string& id,
                       JobStatus status,
                       std::optional<std::string> last_error,
                       std::string log_message);
  void RequeueRunningJobsLocked();
  std::pair<JobStatus, std::vector<std::string>> ExecuteJob(
      const JobRecord& job_snapshot) const;
  std::optional<std::string> LoadState();
  std::optional<std::string> SaveStateLocked() const;

  mutable std::mutex mutex_{};
  std::condition_variable worker_condition_{};
  std::thread worker_thread_{};
  bool stop_worker_{false};
  std::filesystem::path state_directory_{};
  std::unordered_map<std::string, DeploymentRecord> deployments_{};
  std::unordered_map<std::string, JobRecord> jobs_{};
  uint64_t next_job_id_{1};
};

}  // namespace bconfig::service

#endif  // BAREOS_TOOLS_BCONFIG_SERVICE_H_
