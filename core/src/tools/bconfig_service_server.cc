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
  std::optional<std::string> password{};
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
  std::optional<std::string> lan_address{};
  std::optional<uint16_t> port{};
  std::optional<std::string> protocol{};
  std::optional<std::string> auth_type{};
  std::optional<std::string> catalog{};
  std::optional<std::string> username{};
  std::optional<std::string> password{};
  std::optional<bool> enabled{};
  std::optional<bool> passive{};
  std::optional<bool> strict_quotas{};
  std::optional<bool> quota_include_failed_jobs{};
  std::optional<uint64_t> soft_quota{};
  std::optional<uint64_t> hard_quota{};
  std::optional<uint64_t> soft_quota_grace_period{};
  std::optional<uint64_t> file_retention{};
  std::optional<uint64_t> job_retention{};
  std::optional<uint32_t> ndmp_log_level{};
  std::optional<uint32_t> ndmp_block_size{};
  std::optional<bool> ndmp_use_lmdb{};
  std::optional<bool> auto_prune{};
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
  std::optional<uint32_t> maximum_concurrent_jobs{};
  std::optional<uint64_t> heartbeat_interval{};
  std::optional<uint64_t> maximum_bandwidth_per_job{};
  std::optional<std::string> description{};
};

struct DirectorStorageRequestSpec {
  std::optional<std::string> address{};
  std::optional<std::string> lan_address{};
  std::optional<uint16_t> port{};
  std::optional<std::string> protocol{};
  std::optional<std::string> auth_type{};
  std::optional<std::string> username{};
  std::optional<std::string> password{};
  std::optional<std::string> device{};
  std::optional<std::string> media_type{};
  std::optional<bool> autochanger{};
  std::optional<bool> enabled{};
  std::optional<bool> allow_compression{};
  std::optional<uint64_t> heartbeat_interval{};
  std::optional<uint64_t> cache_status_interval{};
  std::optional<uint32_t> maximum_concurrent_jobs{};
  std::optional<uint32_t> maximum_concurrent_read_jobs{};
  std::optional<std::string> paired_storage{};
  std::optional<uint64_t> maximum_bandwidth_per_job{};
  std::optional<bool> collect_statistics{};
  std::optional<std::string> ndmp_changer_device{};
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
  std::optional<std::string> archive_device{};
  std::optional<std::string> device_type{};
  std::optional<std::string> description{};
};

struct DirectorConsoleRequestSpec {
  std::optional<std::string> password{};
  std::optional<std::string> description{};
  std::optional<std::vector<std::string>> job_acl{};
  std::optional<std::vector<std::string>> client_acl{};
  std::optional<std::vector<std::string>> storage_acl{};
  std::optional<std::vector<std::string>> schedule_acl{};
  std::optional<std::vector<std::string>> pool_acl{};
  std::optional<std::vector<std::string>> command_acl{};
  std::optional<std::vector<std::string>> fileset_acl{};
  std::optional<std::vector<std::string>> catalog_acl{};
  std::optional<std::vector<std::string>> where_acl{};
  std::optional<std::vector<std::string>> plugin_options_acl{};
  std::optional<std::vector<std::string>> profiles{};
  std::optional<bool> use_pam_authentication{};
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
  std::optional<std::vector<std::string>> job_acl{};
  std::optional<std::vector<std::string>> client_acl{};
  std::optional<std::vector<std::string>> storage_acl{};
  std::optional<std::vector<std::string>> schedule_acl{};
  std::optional<std::vector<std::string>> pool_acl{};
  std::optional<std::vector<std::string>> command_acl{};
  std::optional<std::vector<std::string>> fileset_acl{};
  std::optional<std::vector<std::string>> catalog_acl{};
  std::optional<std::vector<std::string>> where_acl{};
  std::optional<std::vector<std::string>> plugin_options_acl{};
  std::optional<std::vector<std::string>> profiles{};
};

struct DirectorProfileRequestSpec {
  std::optional<std::string> description{};
  std::optional<std::vector<std::string>> job_acl{};
  std::optional<std::vector<std::string>> client_acl{};
  std::optional<std::vector<std::string>> storage_acl{};
  std::optional<std::vector<std::string>> schedule_acl{};
  std::optional<std::vector<std::string>> pool_acl{};
  std::optional<std::vector<std::string>> command_acl{};
  std::optional<std::vector<std::string>> fileset_acl{};
  std::optional<std::vector<std::string>> catalog_acl{};
  std::optional<std::vector<std::string>> where_acl{};
  std::optional<std::vector<std::string>> plugin_options_acl{};
};

struct DirectorPoolRequestSpec {
  std::optional<std::string> pool_type{};
  std::optional<std::string> label_format{};
  std::optional<std::string> cleaning_prefix{};
  std::optional<std::string> label_type{};
  std::optional<uint32_t> maximum_volumes{};
  std::optional<uint32_t> maximum_volume_jobs{};
  std::optional<uint32_t> maximum_volume_files{};
  std::optional<uint64_t> maximum_volume_bytes{};
  std::optional<uint64_t> volume_retention{};
  std::optional<uint64_t> volume_use_duration{};
  std::optional<uint64_t> migration_time{};
  std::optional<uint64_t> migration_high_bytes{};
  std::optional<uint64_t> migration_low_bytes{};
  std::optional<std::string> next_pool{};
  std::optional<std::vector<std::string>> storages{};
  std::optional<bool> use_catalog{};
  std::optional<bool> catalog_files{};
  std::optional<bool> purge_oldest_volume{};
  std::optional<std::string> action_on_purge{};
  std::optional<bool> recycle_oldest_volume{};
  std::optional<bool> recycle_current_volume{};
  std::optional<bool> auto_prune{};
  std::optional<bool> recycle{};
  std::optional<std::string> recycle_pool{};
  std::optional<std::string> scratch_pool{};
  std::optional<std::string> catalog{};
  std::optional<uint64_t> file_retention{};
  std::optional<uint64_t> job_retention{};
  std::optional<uint32_t> minimum_block_size{};
  std::optional<uint32_t> maximum_block_size{};
  std::optional<std::string> description{};
};

struct DirectorCatalogRequestSpec {
  std::optional<std::string> db_address{};
  std::optional<uint32_t> db_port{};
  std::optional<std::string> db_socket{};
  std::optional<std::string> db_password{};
  std::optional<std::string> db_user{};
  std::optional<std::string> db_name{};
  std::optional<bool> multiple_connections{};
  std::optional<bool> disable_batch_insert{};
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
  std::optional<std::string> mail_command{};
  std::optional<std::string> operator_command{};
  std::optional<std::string> timestamp_format{};
  std::optional<std::vector<std::string>> syslog_entries{};
  std::optional<std::vector<std::string>> mail_entries{};
  std::optional<std::vector<std::string>> mail_on_error_entries{};
  std::optional<std::vector<std::string>> mail_on_success_entries{};
  std::optional<std::vector<std::string>> file_entries{};
  std::optional<std::vector<std::string>> append_entries{};
  std::optional<std::vector<std::string>> stdout_entries{};
  std::optional<std::vector<std::string>> stderr_entries{};
  std::optional<std::vector<std::string>> director_entries{};
  std::optional<std::vector<std::string>> console_entries{};
  std::optional<std::vector<std::string>> operator_entries{};
  std::optional<std::vector<std::string>> catalog_entries{};
  std::optional<std::vector<std::string>> entries{};
};

struct StorageDirectorRequestSpec {
  std::optional<std::string> password{};
  std::optional<std::string> description{};
  std::optional<bool> monitor{};
  std::optional<uint64_t> maximum_bandwidth_per_job{};
  std::optional<std::string> key_encryption_key{};
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

struct StorageDeviceRequestSpec {
  std::optional<std::string> media_type{};
  std::optional<std::string> archive_device{};
  std::optional<std::string> device_type{};
  std::optional<std::string> access_mode{};
  std::optional<std::string> device_options{};
  std::optional<std::string> diagnostic_device{};
  std::optional<bool> hardware_end_of_file{};
  std::optional<bool> hardware_end_of_medium{};
  std::optional<bool> backward_space_record{};
  std::optional<bool> backward_space_file{};
  std::optional<bool> bsf_at_eom{};
  std::optional<bool> two_eof{};
  std::optional<bool> forward_space_record{};
  std::optional<bool> forward_space_file{};
  std::optional<bool> fast_forward_space_file{};
  std::optional<bool> removable_media{};
  std::optional<bool> random_access{};
  std::optional<bool> automatic_mount{};
  std::optional<bool> label_media{};
  std::optional<bool> always_open{};
  std::optional<bool> autochanger{};
  std::optional<bool> close_on_poll{};
  std::optional<bool> block_positioning{};
  std::optional<bool> use_mtiocget{};
  std::optional<bool> check_labels{};
  std::optional<bool> requires_mount{};
  std::optional<bool> offline_on_unmount{};
  std::optional<bool> block_checksum{};
  std::optional<bool> auto_select{};
  std::optional<std::string> changer_device{};
  std::optional<std::string> changer_command{};
  std::optional<std::string> alert_command{};
  std::optional<uint64_t> maximum_changer_wait{};
  std::optional<uint64_t> maximum_open_wait{};
  std::optional<uint32_t> maximum_open_volumes{};
  std::optional<uint32_t> maximum_network_buffer_size{};
  std::optional<uint64_t> volume_poll_interval{};
  std::optional<uint64_t> maximum_rewind_wait{};
  std::optional<uint32_t> label_block_size{};
  std::optional<uint32_t> minimum_block_size{};
  std::optional<uint32_t> maximum_block_size{};
  std::optional<uint64_t> maximum_file_size{};
  std::optional<uint64_t> volume_capacity{};
  std::optional<uint32_t> maximum_concurrent_jobs{};
  std::optional<std::string> spool_directory{};
  std::optional<uint64_t> maximum_spool_size{};
  std::optional<uint64_t> maximum_job_spool_size{};
  std::optional<uint16_t> drive_index{};
  std::optional<std::string> mount_point{};
  std::optional<std::string> mount_command{};
  std::optional<std::string> unmount_command{};
  std::optional<std::string> label_type{};
  std::optional<bool> no_rewind_on_close{};
  std::optional<bool> drive_tape_alert_enabled{};
  std::optional<bool> drive_crypto_enabled{};
  std::optional<bool> query_crypto_status{};
  std::optional<std::string> auto_deflate{};
  std::optional<std::string> auto_deflate_algorithm{};
  std::optional<uint16_t> auto_deflate_level{};
  std::optional<std::string> auto_inflate{};
  std::optional<bool> collect_statistics{};
  std::optional<bool> eof_on_error_is_eot{};
  std::optional<uint32_t> count{};
  std::optional<std::string> description{};
};

struct StorageNdmpRequestSpec {
  std::optional<std::string> description{};
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
  std::optional<std::string> query_file{};
  std::optional<uint32_t> subscriptions{};
  std::optional<bool> just_in_time_reservation{};
  std::optional<uint32_t> maximum_concurrent_jobs{};
  std::optional<uint32_t> maximum_workers_per_job{};
  std::optional<uint32_t> maximum_console_connections{};
  std::optional<std::string> password{};
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
  std::optional<uint64_t> statistics_retention{};
  std::optional<uint64_t> checkpoint_interval{};
  std::optional<uint64_t> client_connect_wait{};
  std::optional<uint32_t> maximum_network_buffer_size{};
  std::optional<std::string> description{};
  std::optional<std::string> key_encryption_key{};
  std::optional<bool> ndmp_namelist_fhinfo_set_zero_for_invalid_uquad{};
  std::optional<bool> auditing{};
  std::optional<std::vector<std::string>> audit_events{};
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
  std::optional<std::string> backup_format{};
  std::optional<std::string> protocol{};
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
  std::optional<std::vector<std::string>> run_entries{};
  std::optional<std::vector<std::string>> run_before_job_entries{};
  std::optional<std::vector<std::string>> run_after_job_entries{};
  std::optional<std::vector<std::string>> run_after_failed_job_entries{};
  std::optional<std::vector<std::string>> client_run_before_job_entries{};
  std::optional<std::vector<std::string>> client_run_after_job_entries{};
  std::optional<std::vector<std::string>> runscript_blocks{};
  std::optional<std::string> where{};
  std::optional<std::string> replace{};
  std::optional<std::string> regex_where{};
  std::optional<std::string> strip_prefix{};
  std::optional<std::string> add_prefix{};
  std::optional<std::string> add_suffix{};
  std::optional<std::string> bootstrap{};
  std::optional<std::string> write_bootstrap{};
  std::optional<std::string> write_verify_list{};
  std::optional<uint64_t> maximum_bandwidth{};
  std::optional<uint64_t> max_run_sched_time{};
  std::optional<uint64_t> max_run_time{};
  std::optional<uint64_t> full_max_runtime{};
  std::optional<uint64_t> incremental_max_runtime{};
  std::optional<uint64_t> differential_max_runtime{};
  std::optional<uint64_t> max_wait_time{};
  std::optional<uint64_t> max_start_delay{};
  std::optional<uint64_t> max_full_interval{};
  std::optional<uint64_t> max_virtual_full_interval{};
  std::optional<uint64_t> max_diff_interval{};
  std::optional<bool> prefix_links{};
  std::optional<bool> prune_jobs{};
  std::optional<bool> prune_files{};
  std::optional<bool> prune_volumes{};
  std::optional<bool> purge_migration_job{};
  std::optional<bool> spool_attributes{};
  std::optional<bool> spool_data{};
  std::optional<uint64_t> spool_size{};
  std::optional<bool> rerun_failed_levels{};
  std::optional<bool> prefer_mounted_volumes{};
  std::optional<uint32_t> maximum_concurrent_jobs{};
  std::optional<bool> reschedule_on_error{};
  std::optional<uint64_t> reschedule_interval{};
  std::optional<uint32_t> reschedule_times{};
  std::optional<int32_t> priority{};
  std::optional<bool> allow_mixed_priority{};
  std::optional<std::string> selection_type{};
  std::optional<std::string> selection_pattern{};
  std::optional<bool> accurate{};
  std::optional<bool> allow_duplicate_jobs{};
  std::optional<bool> allow_higher_duplicates{};
  std::optional<bool> cancel_lower_level_duplicates{};
  std::optional<bool> cancel_queued_duplicates{};
  std::optional<bool> cancel_running_duplicates{};
  std::optional<bool> save_file_history{};
  std::optional<uint64_t> file_history_size{};
  std::optional<std::vector<std::string>> fd_plugin_options{};
  std::optional<std::vector<std::string>> sd_plugin_options{};
  std::optional<std::vector<std::string>> dir_plugin_options{};
  std::optional<uint32_t> max_concurrent_copies{};
  std::optional<bool> always_incremental{};
  std::optional<uint64_t> always_incremental_job_retention{};
  std::optional<uint32_t> always_incremental_keep_number{};
  std::optional<uint64_t> always_incremental_max_full_age{};
  std::optional<uint32_t> max_full_consolidations{};
  std::optional<uint64_t> run_on_incoming_connect_interval{};
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

        <label for="client-stub-tls-authenticate">TlsAuthenticate</label>
        <select id="client-stub-tls-authenticate" name="tls_authenticate">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="client-stub-tls-enable">TlsEnable</label>
        <select id="client-stub-tls-enable" name="tls_enable">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="client-stub-tls-require">TlsRequire</label>
        <select id="client-stub-tls-require" name="tls_require">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="client-stub-tls-verify-peer">TlsVerifyPeer</label>
        <select id="client-stub-tls-verify-peer" name="tls_verify_peer">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

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

        <label for="client-stub-connection-from-director-to-client">ConnectionFromDirectorToClient</label>
        <select id="client-stub-connection-from-director-to-client"
                name="connection_from_director_to_client">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="client-stub-connection-from-client-to-director">ConnectionFromClientToDirector</label>
        <select id="client-stub-connection-from-client-to-director"
                name="connection_from_client_to_director">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="client-stub-monitor">Monitor</label>
        <select id="client-stub-monitor" name="monitor">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

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

        <label for="client-messages-mail-command">MailCommand</label>
        <input id="client-messages-mail-command" name="mail_command"
               placeholder="/usr/lib/bareos/mail-client %r">

        <label for="client-messages-operator-command">OperatorCommand</label>
        <input id="client-messages-operator-command" name="operator_command"
               placeholder="/usr/lib/bareos/operator-client %r">

        <label for="client-messages-timestamp-format">TimestampFormat</label>
        <input id="client-messages-timestamp-format" name="timestamp_format"
               placeholder="%Y-%m-%d %H:%M:%S">

        <label for="client-messages-syslog-entries">Syslog entries</label>
        <textarea id="client-messages-syslog-entries" name="syslog_entries"
                  rows="2" placeholder="mail = all, !skipped"></textarea>

        <label for="client-messages-mail-entries">Mail entries</label>
        <textarea id="client-messages-mail-entries" name="mail_entries"
                  rows="2" placeholder="ops@example.test = all"></textarea>

        <label for="client-messages-mail-on-error-entries">MailOnError entries</label>
        <textarea id="client-messages-mail-on-error-entries"
                  name="mail_on_error_entries"
                  rows="2" placeholder="ops@example.test = all"></textarea>

        <label for="client-messages-mail-on-success-entries">MailOnSuccess entries</label>
        <textarea id="client-messages-mail-on-success-entries"
                  name="mail_on_success_entries"
                  rows="2" placeholder="ops@example.test = all"></textarea>

        <label for="client-messages-file-entries">File entries</label>
        <textarea id="client-messages-file-entries" name="file_entries"
                  rows="2" placeholder="\"/var/log/bareos/client.log\" = all"></textarea>

        <label for="client-messages-append-entries">Append entries</label>
        <textarea id="client-messages-append-entries" name="append_entries"
                  rows="2" placeholder="\"/var/log/bareos/client.log\" = all"></textarea>

        <label for="client-messages-stdout-entries">Stdout entries</label>
        <textarea id="client-messages-stdout-entries" name="stdout_entries"
                  rows="2" placeholder="all, !skipped"></textarea>

        <label for="client-messages-stderr-entries">Stderr entries</label>
        <textarea id="client-messages-stderr-entries" name="stderr_entries"
                  rows="2" placeholder="all, !skipped"></textarea>

        <label for="client-messages-director-entries">Director entries</label>
        <textarea id="client-messages-director-entries" name="director_entries"
                  rows="2" placeholder="bareos-dir = all"></textarea>

        <label for="client-messages-console-entries">Console entries</label>
        <textarea id="client-messages-console-entries" name="console_entries"
                  rows="2" placeholder="all, !skipped"></textarea>

        <label for="client-messages-operator-entries">Operator entries</label>
        <textarea id="client-messages-operator-entries" name="operator_entries"
                  rows="2" placeholder="all"></textarea>

        <label for="client-messages-catalog-entries">Catalog entries</label>
        <textarea id="client-messages-catalog-entries" name="catalog_entries"
                  rows="2" placeholder="all"></textarea>

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
        <input id="director-client-address" name="address"
               placeholder="client1-fd.example.com">

        <label for="director-client-lan-address">LanAddress</label>
        <input id="director-client-lan-address" name="lan_address">

        <label for="director-client-port">Port</label>
        <input id="director-client-port" name="port" type="number"
               min="1" max="65535" placeholder="9102">

        <label for="director-client-protocol">Protocol</label>
        <input id="director-client-protocol" name="protocol"
               placeholder="Native">

        <label for="director-client-auth-type">AuthType</label>
        <input id="director-client-auth-type" name="auth_type"
               placeholder="None">

        <label for="director-client-catalog">Catalog</label>
        <input id="director-client-catalog" name="catalog"
               placeholder="MyCatalog">

        <label for="director-client-username">Username</label>
        <input id="director-client-username" name="username"
               placeholder="bareos">

        <label for="director-client-password">Password</label>
        <input id="director-client-password" name="password"
               placeholder="cleartext or [md5]hash">

        <label for="director-client-enabled">Enabled</label>
        <select id="director-client-enabled" name="enabled">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="director-client-passive">Passive</label>
        <select id="director-client-passive" name="passive">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="director-client-strict-quotas">StrictQuotas</label>
        <select id="director-client-strict-quotas" name="strict_quotas">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="director-client-quota-include-failed-jobs">QuotaIncludeFailedJobs</label>
        <select id="director-client-quota-include-failed-jobs"
                name="quota_include_failed_jobs">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="director-client-connection-from-director-to-client">ConnectionFromDirectorToClient</label>
        <select id="director-client-connection-from-director-to-client"
                name="connection_from_director_to_client">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="director-client-connection-from-client-to-director">ConnectionFromClientToDirector</label>
        <select id="director-client-connection-from-client-to-director"
                name="connection_from_client_to_director">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="director-client-maximum-bandwidth-per-job">Maximum bandwidth per job</label>
        <input id="director-client-maximum-bandwidth-per-job"
               name="maximum_bandwidth_per_job" type="number" min="0"
               placeholder="0">

        <label for="director-client-soft-quota">SoftQuota</label>
        <input id="director-client-soft-quota" name="soft_quota" type="number"
               min="0" placeholder="0">

        <label for="director-client-hard-quota">HardQuota</label>
        <input id="director-client-hard-quota" name="hard_quota" type="number"
               min="0" placeholder="0">

        <label for="director-client-soft-quota-grace-period">SoftQuotaGracePeriod</label>
        <input id="director-client-soft-quota-grace-period"
               name="soft_quota_grace_period" type="number" min="0"
               placeholder="0">

        <label for="director-client-file-retention">FileRetention</label>
        <input id="director-client-file-retention" name="file_retention"
               type="number" min="0" placeholder="0">

        <label for="director-client-job-retention">JobRetention</label>
        <input id="director-client-job-retention" name="job_retention"
               type="number" min="0" placeholder="0">

        <label for="director-client-ndmp-log-level">NdmpLogLevel</label>
        <input id="director-client-ndmp-log-level" name="ndmp_log_level"
               type="number" min="0" placeholder="0">

        <label for="director-client-ndmp-block-size">NdmpBlockSize</label>
        <input id="director-client-ndmp-block-size" name="ndmp_block_size"
               type="number" min="0" placeholder="0">

        <label for="director-client-ndmp-use-lmdb">NdmpUseLmdb</label>
        <select id="director-client-ndmp-use-lmdb" name="ndmp_use_lmdb">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="director-client-auto-prune">AutoPrune</label>
        <select id="director-client-auto-prune" name="auto_prune">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="director-client-tls-authenticate">TlsAuthenticate</label>
        <select id="director-client-tls-authenticate" name="tls_authenticate">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="director-client-tls-enable">TlsEnable</label>
        <select id="director-client-tls-enable" name="tls_enable">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="director-client-tls-require">TlsRequire</label>
        <select id="director-client-tls-require" name="tls_require">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="director-client-tls-verify-peer">TlsVerifyPeer</label>
        <select id="director-client-tls-verify-peer" name="tls_verify_peer">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="director-client-tls-cipher-list">TlsCipherList</label>
        <input id="director-client-tls-cipher-list" name="tls_cipher_list"
               placeholder="HIGH:!aNULL">

        <label for="director-client-tls-cipher-suites">TlsCipherSuites</label>
        <input id="director-client-tls-cipher-suites" name="tls_cipher_suites"
               placeholder="TLS_AES_256_GCM_SHA384">

        <label for="director-client-tls-dh-file">TlsDhFile</label>
        <input id="director-client-tls-dh-file" name="tls_dh_file"
               placeholder="/etc/bareos/dh4096.pem">

        <label for="director-client-tls-protocol">TlsProtocol</label>
        <input id="director-client-tls-protocol" name="tls_protocol"
               placeholder="TLSv1.2">

        <label for="director-client-tls-ca-certificate-file">TlsCaCertificateFile</label>
        <input id="director-client-tls-ca-certificate-file"
               name="tls_ca_certificate_file"
               placeholder="/etc/bareos/ca.pem">

        <label for="director-client-tls-ca-certificate-dir">TlsCaCertificateDir</label>
        <input id="director-client-tls-ca-certificate-dir"
               name="tls_ca_certificate_dir"
               placeholder="/etc/ssl/certs">

        <label for="director-client-tls-certificate-revocation-list">TlsCertificateRevocationList</label>
        <input id="director-client-tls-certificate-revocation-list"
               name="tls_certificate_revocation_list"
               placeholder="/etc/bareos/crl.pem">

        <label for="director-client-tls-certificate">TlsCertificate</label>
        <input id="director-client-tls-certificate" name="tls_certificate"
               placeholder="/etc/bareos/client.crt">

        <label for="director-client-tls-key">TlsKey</label>
        <input id="director-client-tls-key" name="tls_key"
               placeholder="/etc/bareos/client.key">

        <label for="director-client-tls-allowed-cn">TlsAllowedCn</label>
        <textarea id="director-client-tls-allowed-cn" name="tls_allowed_cn"
                  rows="3"
                  placeholder="backup-dir.example.test&#10;backup-dir.internal"></textarea>

        <label for="director-client-maximum-concurrent-jobs">MaximumConcurrentJobs</label>
        <input id="director-client-maximum-concurrent-jobs"
               name="maximum_concurrent_jobs" type="number" min="0"
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
        <input id="director-storage-address" name="address"
               placeholder="localhost">

        <label for="director-storage-lan-address">LanAddress</label>
        <input id="director-storage-lan-address" name="lan_address">

        <label for="director-storage-port">Port</label>
        <input id="director-storage-port" name="port" type="number"
               min="1" max="65535" placeholder="9103">

        <label for="director-storage-protocol">Protocol</label>
        <input id="director-storage-protocol" name="protocol"
               placeholder="NDMPV4">

        <label for="director-storage-auth-type">AuthType</label>
        <input id="director-storage-auth-type" name="auth_type"
               placeholder="Clear">

        <label for="director-storage-username">Username</label>
        <input id="director-storage-username" name="username"
               placeholder="ndmp-user">

        <label for="director-storage-password">Password</label>
        <input id="director-storage-password" name="password"
               placeholder="cleartext or [md5]hash">

        <label for="director-storage-device">Device</label>
        <input id="director-storage-device" name="device"
               placeholder="FileManaged">

        <label for="director-storage-media-type">Media Type</label>
        <input id="director-storage-media-type" name="media_type"
               placeholder="File">

        <label for="director-storage-autochanger">AutoChanger</label>
        <select id="director-storage-autochanger" name="autochanger">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="director-storage-enabled">Enabled</label>
        <select id="director-storage-enabled" name="enabled">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="director-storage-allow-compression">AllowCompression</label>
        <select id="director-storage-allow-compression"
                name="allow_compression">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="director-storage-maximum-bandwidth-per-job">Maximum bandwidth per job</label>
        <input id="director-storage-maximum-bandwidth-per-job"
               name="maximum_bandwidth_per_job" type="number" min="0"
               placeholder="0">

        <label for="director-storage-heartbeat-interval">Heartbeat interval</label>
        <input id="director-storage-heartbeat-interval" name="heartbeat_interval"
               type="number" min="0" placeholder="0">

        <label for="director-storage-cache-status-interval">Cache status interval</label>
        <input id="director-storage-cache-status-interval"
               name="cache_status_interval" type="number" min="0"
               placeholder="0">

        <label for="director-storage-maximum-concurrent-jobs">MaximumConcurrentJobs</label>
        <input id="director-storage-maximum-concurrent-jobs"
               name="maximum_concurrent_jobs" type="number" min="0"
               placeholder="0">

        <label for="director-storage-maximum-concurrent-read-jobs">MaximumConcurrentReadJobs</label>
        <input id="director-storage-maximum-concurrent-read-jobs"
               name="maximum_concurrent_read_jobs" type="number" min="0"
               placeholder="0">

        <label for="director-storage-paired-storage">PairedStorage</label>
        <input id="director-storage-paired-storage" name="paired_storage"
               placeholder="File">

        <label for="director-storage-archive-device">Archive Device</label>
        <input id="director-storage-archive-device" name="archive_device"
               placeholder="/tmp/bareos-storage">

        <label for="director-storage-device-type">Device Type</label>
        <input id="director-storage-device-type" name="device_type"
               placeholder="file">

        <label for="director-storage-collect-statistics">CollectStatistics</label>
        <select id="director-storage-collect-statistics"
                name="collect_statistics">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="director-storage-ndmp-changer-device">NdmpChangerDevice</label>
        <input id="director-storage-ndmp-changer-device"
               name="ndmp_changer_device" placeholder="changer0">

        <label for="director-storage-tls-authenticate">TlsAuthenticate</label>
        <select id="director-storage-tls-authenticate"
                name="tls_authenticate">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="director-storage-tls-enable">TlsEnable</label>
        <select id="director-storage-tls-enable" name="tls_enable">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="director-storage-tls-require">TlsRequire</label>
        <select id="director-storage-tls-require" name="tls_require">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="director-storage-tls-verify-peer">TlsVerifyPeer</label>
        <select id="director-storage-tls-verify-peer"
                name="tls_verify_peer">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="director-storage-tls-cipher-list">TLS cipher list</label>
        <input id="director-storage-tls-cipher-list" name="tls_cipher_list"
               placeholder="ECDHE-RSA-AES256-GCM-SHA384">

        <label for="director-storage-tls-cipher-suites">TLS cipher suites</label>
        <input id="director-storage-tls-cipher-suites" name="tls_cipher_suites"
               placeholder="TLS_AES_256_GCM_SHA384">

        <label for="director-storage-tls-dh-file">TLS DH file</label>
        <input id="director-storage-tls-dh-file" name="tls_dh_file"
               placeholder="/etc/bareos/dh4096.pem">

        <label for="director-storage-tls-protocol">TLS protocol</label>
        <input id="director-storage-tls-protocol" name="tls_protocol"
               placeholder="MinProtocol = TLSv1.2">

        <label for="director-storage-tls-ca-certificate-file">TLS CA certificate file</label>
        <input id="director-storage-tls-ca-certificate-file"
               name="tls_ca_certificate_file"
               placeholder="/etc/bareos/ca.crt">

        <label for="director-storage-tls-ca-certificate-dir">TLS CA certificate dir</label>
        <input id="director-storage-tls-ca-certificate-dir"
               name="tls_ca_certificate_dir"
               placeholder="/etc/ssl/certs">

        <label for="director-storage-tls-certificate-revocation-list">TLS certificate revocation list</label>
        <input id="director-storage-tls-certificate-revocation-list"
               name="tls_certificate_revocation_list"
               placeholder="/etc/bareos/crl.pem">

        <label for="director-storage-tls-certificate">TLS certificate</label>
        <input id="director-storage-tls-certificate" name="tls_certificate"
               placeholder="/etc/bareos/storage.crt">

        <label for="director-storage-tls-key">TLS key</label>
        <input id="director-storage-tls-key" name="tls_key"
               placeholder="/etc/bareos/storage.key">

        <label for="director-storage-tls-allowed-cn">TLS allowed CNs</label>
        <textarea id="director-storage-tls-allowed-cn" name="tls_allowed_cn"
                  placeholder="storage-cn-1&#10;storage-cn-2"></textarea>

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

        <label for="director-console-use-pam">UsePamAuthentication</label>
        <select id="director-console-use-pam" name="use_pam_authentication">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="director-console-job-acl">Job ACL</label>
        <textarea id="director-console-job-acl" name="job_acl"
                  placeholder="ManagedJob&#10;RestoreJob"></textarea>

        <label for="director-console-client-acl">Client ACL</label>
        <textarea id="director-console-client-acl" name="client_acl"
                  placeholder="bareos-fd"></textarea>

        <label for="director-console-storage-acl">Storage ACL</label>
        <textarea id="director-console-storage-acl" name="storage_acl"
                  placeholder="FileStorage"></textarea>

        <label for="director-console-schedule-acl">Schedule ACL</label>
        <textarea id="director-console-schedule-acl" name="schedule_acl"
                  placeholder="WeeklyCycle"></textarea>

        <label for="director-console-pool-acl">Pool ACL</label>
        <textarea id="director-console-pool-acl" name="pool_acl"
                  placeholder="Full"></textarea>

        <label for="director-console-command-acl">Command ACL</label>
        <textarea id="director-console-command-acl" name="command_acl"
                  placeholder="status&#10;.status&#10;show"></textarea>

        <label for="director-console-fileset-acl">FileSet ACL</label>
        <textarea id="director-console-fileset-acl" name="fileset_acl"
                  placeholder="LinuxAll"></textarea>

        <label for="director-console-catalog-acl">Catalog ACL</label>
        <textarea id="director-console-catalog-acl" name="catalog_acl"
                  placeholder="MyCatalog"></textarea>

        <label for="director-console-where-acl">Where ACL</label>
        <textarea id="director-console-where-acl" name="where_acl"
                  placeholder="/tmp/restore"></textarea>

        <label for="director-console-plugin-options-acl">PluginOptions ACL</label>
        <textarea id="director-console-plugin-options-acl"
                  name="plugin_options_acl"
                  placeholder="python:module_path"></textarea>

        <label for="director-console-profiles">Profiles</label>
        <textarea id="director-console-profiles" name="profiles"
                  placeholder="operator"></textarea>

        <label for="director-console-tls-authenticate">TlsAuthenticate</label>
        <select id="director-console-tls-authenticate"
                name="tls_authenticate">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="director-console-tls-enable">TlsEnable</label>
        <select id="director-console-tls-enable" name="tls_enable">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="director-console-tls-require">TlsRequire</label>
        <select id="director-console-tls-require" name="tls_require">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="director-console-tls-verify-peer">TlsVerifyPeer</label>
        <select id="director-console-tls-verify-peer"
                name="tls_verify_peer">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="director-console-tls-cipher-list">TLS cipher list</label>
        <input id="director-console-tls-cipher-list" name="tls_cipher_list"
               placeholder="HIGH">

        <label for="director-console-tls-cipher-suites">TLS cipher suites</label>
        <input id="director-console-tls-cipher-suites"
               name="tls_cipher_suites"
               placeholder="TLS_AES_256_GCM_SHA384">

        <label for="director-console-tls-dh-file">TLS DH file</label>
        <input id="director-console-tls-dh-file" name="tls_dh_file"
               placeholder="/etc/bareos/dh4096.pem">

        <label for="director-console-tls-protocol">TLS protocol</label>
        <input id="director-console-tls-protocol" name="tls_protocol"
               placeholder="+TLSv1.2:+TLSv1.3">

        <label for="director-console-tls-ca-certificate-file">TLS CA certificate file</label>
        <input id="director-console-tls-ca-certificate-file"
               name="tls_ca_certificate_file"
               placeholder="/etc/bareos/ca.pem">

        <label for="director-console-tls-ca-certificate-dir">TLS CA certificate dir</label>
        <input id="director-console-tls-ca-certificate-dir"
               name="tls_ca_certificate_dir"
               placeholder="/etc/ssl/certs">

        <label for="director-console-tls-certificate-revocation-list">TLS certificate revocation list</label>
        <input id="director-console-tls-certificate-revocation-list"
               name="tls_certificate_revocation_list"
               placeholder="/etc/bareos/crl.pem">

        <label for="director-console-tls-certificate">TLS certificate</label>
        <input id="director-console-tls-certificate" name="tls_certificate"
               placeholder="/etc/bareos/console-cert.pem">

        <label for="director-console-tls-key">TLS key</label>
        <input id="director-console-tls-key" name="tls_key"
               placeholder="/etc/bareos/console-key.pem">

        <label for="director-console-tls-allowed-cn">TLS allowed CNs</label>
        <textarea id="director-console-tls-allowed-cn" name="tls_allowed_cn"
                  placeholder="backup-dir.example.test&#10;operator.example.test"></textarea>

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

        <label for="director-user-job-acl">Job ACL</label>
        <textarea id="director-user-job-acl" name="job_acl"
                  placeholder="ManagedJob&#10;RestoreJob"></textarea>

        <label for="director-user-client-acl">Client ACL</label>
        <textarea id="director-user-client-acl" name="client_acl"
                  placeholder="bareos-fd"></textarea>

        <label for="director-user-storage-acl">Storage ACL</label>
        <textarea id="director-user-storage-acl" name="storage_acl"
                  placeholder="FileStorage"></textarea>

        <label for="director-user-schedule-acl">Schedule ACL</label>
        <textarea id="director-user-schedule-acl" name="schedule_acl"
                  placeholder="WeeklyCycle"></textarea>

        <label for="director-user-pool-acl">Pool ACL</label>
        <textarea id="director-user-pool-acl" name="pool_acl"
                  placeholder="Full"></textarea>

        <label for="director-user-command-acl">Command ACL</label>
        <textarea id="director-user-command-acl" name="command_acl"
                  placeholder="status&#10;.status&#10;show"></textarea>

        <label for="director-user-fileset-acl">FileSet ACL</label>
        <textarea id="director-user-fileset-acl" name="fileset_acl"
                  placeholder="LinuxAll"></textarea>

        <label for="director-user-catalog-acl">Catalog ACL</label>
        <textarea id="director-user-catalog-acl" name="catalog_acl"
                  placeholder="MyCatalog"></textarea>

        <label for="director-user-where-acl">Where ACL</label>
        <textarea id="director-user-where-acl" name="where_acl"
                  placeholder="/tmp/restore"></textarea>

        <label for="director-user-plugin-options-acl">PluginOptions ACL</label>
        <textarea id="director-user-plugin-options-acl"
                  name="plugin_options_acl"
                  placeholder="python:module_path"></textarea>

        <label for="director-user-profiles">Profiles</label>
        <textarea id="director-user-profiles" name="profiles"
                  placeholder="operator"></textarea>

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

        <label for="director-profile-job-acl">Job ACL</label>
        <textarea id="director-profile-job-acl" name="job_acl"
                  placeholder="ManagedJob&#10;RestoreJob"></textarea>

        <label for="director-profile-client-acl">Client ACL</label>
        <textarea id="director-profile-client-acl" name="client_acl"
                  placeholder="bareos-fd"></textarea>

        <label for="director-profile-storage-acl">Storage ACL</label>
        <textarea id="director-profile-storage-acl" name="storage_acl"
                  placeholder="FileStorage"></textarea>

        <label for="director-profile-schedule-acl">Schedule ACL</label>
        <textarea id="director-profile-schedule-acl" name="schedule_acl"
                  placeholder="WeeklyCycle"></textarea>

        <label for="director-profile-pool-acl">Pool ACL</label>
        <textarea id="director-profile-pool-acl" name="pool_acl"
                  placeholder="Full"></textarea>

        <label for="director-profile-command-acl">Command ACL</label>
        <textarea id="director-profile-command-acl" name="command_acl"
                  placeholder="status&#10;.status&#10;show"></textarea>

        <label for="director-profile-fileset-acl">FileSet ACL</label>
        <textarea id="director-profile-fileset-acl" name="fileset_acl"
                  placeholder="LinuxAll"></textarea>

        <label for="director-profile-catalog-acl">Catalog ACL</label>
        <textarea id="director-profile-catalog-acl" name="catalog_acl"
                  placeholder="MyCatalog"></textarea>

        <label for="director-profile-where-acl">Where ACL</label>
        <textarea id="director-profile-where-acl" name="where_acl"
                  placeholder="/tmp/restore"></textarea>

        <label for="director-profile-plugin-options-acl">PluginOptions ACL</label>
        <textarea id="director-profile-plugin-options-acl"
                  name="plugin_options_acl"
                  placeholder="python:module_path"></textarea>

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
        <input id="director-pool-pool-type" name="pool_type"
               placeholder="Backup">

        <label for="director-pool-label-format">Label format</label>
        <input id="director-pool-label-format" name="label_format"
               placeholder="Managed-">

        <label for="director-pool-cleaning-prefix">CleaningPrefix</label>
        <input id="director-pool-cleaning-prefix" name="cleaning_prefix"
               placeholder="Cleaner-">

        <label for="director-pool-label-type">LabelType</label>
        <input id="director-pool-label-type" name="label_type"
               placeholder="ansi">

        <label for="director-pool-maximum-volumes">Maximum volumes</label>
        <input id="director-pool-maximum-volumes" name="maximum_volumes"
               type="number" min="0">

        <label for="director-pool-maximum-volume-jobs">Maximum volume jobs</label>
        <input id="director-pool-maximum-volume-jobs"
               name="maximum_volume_jobs" type="number" min="0">

        <label for="director-pool-maximum-volume-files">Maximum volume files</label>
        <input id="director-pool-maximum-volume-files"
               name="maximum_volume_files" type="number" min="0">

        <label for="director-pool-maximum-volume-bytes">Maximum volume bytes</label>
        <input id="director-pool-maximum-volume-bytes"
               name="maximum_volume_bytes" type="number" min="0">

        <label for="director-pool-volume-retention">Volume retention (seconds)</label>
        <input id="director-pool-volume-retention" name="volume_retention"
               type="number" min="0">

        <label for="director-pool-volume-use-duration">Volume use duration</label>
        <input id="director-pool-volume-use-duration"
               name="volume_use_duration" type="number" min="0">

        <label for="director-pool-migration-time">Migration time</label>
        <input id="director-pool-migration-time" name="migration_time"
               type="number" min="0">

        <label for="director-pool-migration-high-bytes">Migration high bytes</label>
        <input id="director-pool-migration-high-bytes"
               name="migration_high_bytes" type="number" min="0">

        <label for="director-pool-migration-low-bytes">Migration low bytes</label>
        <input id="director-pool-migration-low-bytes"
               name="migration_low_bytes" type="number" min="0">

        <label for="director-pool-next-pool">NextPool</label>
        <input id="director-pool-next-pool" name="next_pool"
               placeholder="Incremental">

        <label for="director-pool-storages">Storages</label>
        <textarea id="director-pool-storages" name="storages"
                  rows="3" placeholder="File&#10;Tape"></textarea>

        <label for="director-pool-use-catalog">UseCatalog</label>
        <select id="director-pool-use-catalog" name="use_catalog">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="director-pool-catalog-files">CatalogFiles</label>
        <select id="director-pool-catalog-files" name="catalog_files">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="director-pool-purge-oldest-volume">PurgeOldestVolume</label>
        <select id="director-pool-purge-oldest-volume"
                name="purge_oldest_volume">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="director-pool-action-on-purge">ActionOnPurge</label>
        <input id="director-pool-action-on-purge" name="action_on_purge"
               placeholder="Truncate">

        <label for="director-pool-recycle-oldest-volume">RecycleOldestVolume</label>
        <select id="director-pool-recycle-oldest-volume"
                name="recycle_oldest_volume">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="director-pool-recycle-current-volume">RecycleCurrentVolume</label>
        <select id="director-pool-recycle-current-volume"
                name="recycle_current_volume">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="director-pool-auto-prune">AutoPrune</label>
        <select id="director-pool-auto-prune" name="auto_prune">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="director-pool-recycle">Recycle</label>
        <select id="director-pool-recycle" name="recycle">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="director-pool-recycle-pool">RecyclePool</label>
        <input id="director-pool-recycle-pool" name="recycle_pool"
               placeholder="Scratch">

        <label for="director-pool-scratch-pool">ScratchPool</label>
        <input id="director-pool-scratch-pool" name="scratch_pool"
               placeholder="Scratch">

        <label for="director-pool-catalog">Catalog</label>
        <input id="director-pool-catalog" name="catalog"
               placeholder="MyCatalog">

        <label for="director-pool-file-retention">FileRetention</label>
        <input id="director-pool-file-retention" name="file_retention"
               type="number" min="0">

        <label for="director-pool-job-retention">JobRetention</label>
        <input id="director-pool-job-retention" name="job_retention"
               type="number" min="0">

        <label for="director-pool-minimum-block-size">MinimumBlockSize</label>
        <input id="director-pool-minimum-block-size" name="minimum_block_size"
               type="number" min="0">

        <label for="director-pool-maximum-block-size">MaximumBlockSize</label>
        <input id="director-pool-maximum-block-size" name="maximum_block_size"
               type="number" min="0">

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
               placeholder="bareos_catalog">

        <label for="director-catalog-db-user">DbUser</label>
        <input id="director-catalog-db-user" name="db_user"
               placeholder="bareos">

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

        <label for="director-catalog-multiple-connections">MultipleConnections</label>
        <select id="director-catalog-multiple-connections"
                name="multiple_connections">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="director-catalog-disable-batch-insert">DisableBatchInsert</label>
        <select id="director-catalog-disable-batch-insert"
                name="disable_batch_insert">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="director-catalog-reconnect">Reconnect</label>
        <select id="director-catalog-reconnect" name="reconnect">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="director-catalog-exit-on-fatal">ExitOnFatal</label>
        <select id="director-catalog-exit-on-fatal" name="exit_on_fatal">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

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

        <label for="director-schedule-enabled">Enabled</label>
        <select id="director-schedule-enabled" name="enabled">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

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

        <label for="director-messages-mail-command">MailCommand</label>
        <input id="director-messages-mail-command" name="mail_command"
               placeholder="/usr/lib/bareos/mail-managed %r">

        <label for="director-messages-operator-command">OperatorCommand</label>
        <input id="director-messages-operator-command" name="operator_command"
               placeholder="/usr/lib/bareos/operator-managed %r">

        <label for="director-messages-timestamp-format">TimestampFormat</label>
        <input id="director-messages-timestamp-format" name="timestamp_format"
               placeholder="%Y-%m-%d %H:%M:%S">

        <label for="director-messages-syslog-entries">Syslog entries</label>
        <textarea id="director-messages-syslog-entries" name="syslog_entries"
                  rows="2" placeholder="mail = all, !skipped"></textarea>

        <label for="director-messages-mail-entries">Mail entries</label>
        <textarea id="director-messages-mail-entries" name="mail_entries"
                  rows="2" placeholder="ops@example.test = all"></textarea>

        <label for="director-messages-mail-on-error-entries">MailOnError entries</label>
        <textarea id="director-messages-mail-on-error-entries"
                  name="mail_on_error_entries"
                  rows="2" placeholder="ops@example.test = all"></textarea>

        <label for="director-messages-mail-on-success-entries">MailOnSuccess entries</label>
        <textarea id="director-messages-mail-on-success-entries"
                  name="mail_on_success_entries"
                  rows="2" placeholder="ops@example.test = all"></textarea>

        <label for="director-messages-file-entries">File entries</label>
        <textarea id="director-messages-file-entries" name="file_entries"
                  rows="2" placeholder="\"/var/log/bareos/dir.log\" = all"></textarea>

        <label for="director-messages-append-entries">Append entries</label>
        <textarea id="director-messages-append-entries" name="append_entries"
                  rows="2" placeholder="\"/var/log/bareos/dir.log\" = all"></textarea>

        <label for="director-messages-stdout-entries">Stdout entries</label>
        <textarea id="director-messages-stdout-entries" name="stdout_entries"
                  rows="2" placeholder="all, !skipped"></textarea>

        <label for="director-messages-stderr-entries">Stderr entries</label>
        <textarea id="director-messages-stderr-entries" name="stderr_entries"
                  rows="2" placeholder="all, !skipped"></textarea>

        <label for="director-messages-director-entries">Director entries</label>
        <textarea id="director-messages-director-entries" name="director_entries"
                  rows="2" placeholder="bareos-dir = all"></textarea>

        <label for="director-messages-console-entries">Console entries</label>
        <textarea id="director-messages-console-entries" name="console_entries"
                  rows="2"
                  placeholder="all, !skipped, !saved, !audit"></textarea>

        <label for="director-messages-operator-entries">Operator entries</label>
        <textarea id="director-messages-operator-entries" name="operator_entries"
                  rows="2" placeholder="all"></textarea>

        <label for="director-messages-catalog-entries">Catalog entries</label>
        <textarea id="director-messages-catalog-entries" name="catalog_entries"
                  rows="2" placeholder="all"></textarea>

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

        <label for="director-fileset-ignore-fileset-changes">IgnoreFileSetChanges</label>
        <select id="director-fileset-ignore-fileset-changes"
                name="ignore_fileset_changes">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="director-fileset-enable-vss">EnableVSS</label>
        <select id="director-fileset-enable-vss" name="enable_vss">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

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

        <label for="director-job-backup-format">BackupFormat</label>
        <input id="director-job-backup-format" name="backup_format"
               placeholder="Native">

        <label for="director-job-protocol">Protocol</label>
        <input id="director-job-protocol" name="protocol" placeholder="Native">

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

        <label for="director-job-replace">Replace</label>
        <input id="director-job-replace" name="replace" placeholder="always">

        <label for="director-job-regex-where">RegexWhere</label>
        <input id="director-job-regex-where" name="regex_where">

        <label for="director-job-strip-prefix">StripPrefix</label>
        <input id="director-job-strip-prefix" name="strip_prefix">

        <label for="director-job-add-prefix">AddPrefix</label>
        <input id="director-job-add-prefix" name="add_prefix">

        <label for="director-job-add-suffix">AddSuffix</label>
        <input id="director-job-add-suffix" name="add_suffix">

        <label for="director-job-bootstrap">Bootstrap</label>
        <input id="director-job-bootstrap" name="bootstrap">

        <label for="director-job-write-bootstrap">WriteBootstrap</label>
        <input id="director-job-write-bootstrap" name="write_bootstrap">

        <label for="director-job-write-verify-list">WriteVerifyList</label>
        <input id="director-job-write-verify-list" name="write_verify_list">

        <label for="director-job-run-entries">Run entries</label>
        <textarea id="director-job-run-entries" name="run_entries" rows="3"
                  placeholder="Level=Full monthly 1st sat at 21:00"></textarea>

        <label for="director-job-run-before-job-entries">RunBeforeJob entries</label>
        <textarea id="director-job-run-before-job-entries"
                  name="run_before_job_entries" rows="2"
                  placeholder="/usr/lib/bareos/scripts/pre-job.sh"></textarea>

        <label for="director-job-run-after-job-entries">RunAfterJob entries</label>
        <textarea id="director-job-run-after-job-entries"
                  name="run_after_job_entries" rows="2"
                  placeholder="/usr/lib/bareos/scripts/post-job.sh"></textarea>

        <label for="director-job-run-after-failed-job-entries">RunAfterFailedJob entries</label>
        <textarea id="director-job-run-after-failed-job-entries"
                  name="run_after_failed_job_entries" rows="2"
                  placeholder="/usr/lib/bareos/scripts/fail-job.sh"></textarea>

        <label for="director-job-client-run-before-job-entries">ClientRunBeforeJob entries</label>
        <textarea id="director-job-client-run-before-job-entries"
                  name="client_run_before_job_entries" rows="2"
                  placeholder="/usr/lib/bareos/scripts/client-pre.sh"></textarea>

        <label for="director-job-client-run-after-job-entries">ClientRunAfterJob entries</label>
        <textarea id="director-job-client-run-after-job-entries"
                  name="client_run_after_job_entries" rows="2"
                  placeholder="/usr/lib/bareos/scripts/client-post.sh"></textarea>

        <label for="director-job-runscript-blocks">RunScript blocks</label>
        <textarea id="director-job-runscript-blocks" name="runscript_blocks"
                  rows="8"
                  placeholder="RunScript {&#10;  Console = &quot;status dir&quot;&#10;  RunsWhen = After&#10;  RunsOnFailure = yes&#10;}"></textarea>

        <label for="director-job-maximum-bandwidth">MaximumBandwidth</label>
        <input id="director-job-maximum-bandwidth" name="maximum_bandwidth"
               type="number" min="0">

        <label for="director-job-max-run-sched-time">MaxRunSchedTime</label>
        <input id="director-job-max-run-sched-time" name="max_run_sched_time"
               type="number" min="0">

        <label for="director-job-max-run-time">MaxRunTime</label>
        <input id="director-job-max-run-time" name="max_run_time"
               type="number" min="0">

        <label for="director-job-full-max-runtime">FullMaxRunTime</label>
        <input id="director-job-full-max-runtime" name="full_max_runtime"
               type="number" min="0">

        <label for="director-job-incremental-max-runtime">IncrementalMaxRunTime</label>
        <input id="director-job-incremental-max-runtime"
               name="incremental_max_runtime" type="number" min="0">

        <label for="director-job-differential-max-runtime">DifferentialMaxRunTime</label>
        <input id="director-job-differential-max-runtime"
               name="differential_max_runtime" type="number" min="0">

        <label for="director-job-max-wait-time">MaxWaitTime</label>
        <input id="director-job-max-wait-time" name="max_wait_time"
               type="number" min="0">

        <label for="director-job-max-start-delay">MaxStartDelay</label>
        <input id="director-job-max-start-delay" name="max_start_delay"
               type="number" min="0">

        <label for="director-job-max-full-interval">MaxFullInterval</label>
        <input id="director-job-max-full-interval" name="max_full_interval"
               type="number" min="0">

        <label for="director-job-max-virtual-full-interval">MaxVirtualFullInterval</label>
        <input id="director-job-max-virtual-full-interval"
               name="max_virtual_full_interval" type="number" min="0">

        <label for="director-job-max-diff-interval">MaxDiffInterval</label>
        <input id="director-job-max-diff-interval" name="max_diff_interval"
               type="number" min="0">

        <label for="director-job-spool-size">SpoolSize</label>
        <input id="director-job-spool-size" name="spool_size"
               type="number" min="0">

        <label for="director-job-maximum-concurrent-jobs">MaximumConcurrentJobs</label>
        <input id="director-job-maximum-concurrent-jobs"
               name="maximum_concurrent_jobs" type="number" min="0">

        <label for="director-job-reschedule-interval">RescheduleInterval</label>
        <input id="director-job-reschedule-interval" name="reschedule_interval"
               type="number" min="0">

        <label for="director-job-reschedule-times">RescheduleTimes</label>
        <input id="director-job-reschedule-times" name="reschedule_times"
               type="number" min="0">

        <label for="director-job-priority">Priority</label>
        <input id="director-job-priority" name="priority" type="number">

        <label for="director-job-selection-type">SelectionType</label>
        <input id="director-job-selection-type" name="selection_type"
               placeholder="Matching">

        <label for="director-job-selection-pattern">SelectionPattern</label>
        <input id="director-job-selection-pattern" name="selection_pattern">

        <label for="director-job-file-history-size">FileHistorySize</label>
        <input id="director-job-file-history-size" name="file_history_size"
               type="number" min="0">

        <label for="director-job-fd-plugin-options">FdPluginOptions</label>
        <textarea id="director-job-fd-plugin-options" name="fd_plugin_options"
                  rows="2" placeholder="python:module_path=/opt/bareos"></textarea>

        <label for="director-job-sd-plugin-options">SdPluginOptions</label>
        <textarea id="director-job-sd-plugin-options" name="sd_plugin_options"
                  rows="2" placeholder="compression=on"></textarea>

        <label for="director-job-dir-plugin-options">DirPluginOptions</label>
        <textarea id="director-job-dir-plugin-options" name="dir_plugin_options"
                  rows="2" placeholder="audit=verbose"></textarea>

        <label for="director-job-max-concurrent-copies">MaxConcurrentCopies</label>
        <input id="director-job-max-concurrent-copies"
               name="max_concurrent_copies" type="number" min="0">

        <label for="director-job-always-incremental-job-retention">AlwaysIncrementalJobRetention</label>
        <input id="director-job-always-incremental-job-retention"
               name="always_incremental_job_retention" type="number" min="0">

        <label for="director-job-always-incremental-keep-number">AlwaysIncrementalKeepNumber</label>
        <input id="director-job-always-incremental-keep-number"
               name="always_incremental_keep_number" type="number" min="0">

        <label for="director-job-always-incremental-max-full-age">AlwaysIncrementalMaxFullAge</label>
        <input id="director-job-always-incremental-max-full-age"
               name="always_incremental_max_full_age" type="number" min="0">

        <label for="director-job-max-full-consolidations">MaxFullConsolidations</label>
        <input id="director-job-max-full-consolidations"
               name="max_full_consolidations" type="number" min="0">

        <label for="director-job-run-on-incoming-connect-interval">RunOnIncomingConnectInterval</label>
        <input id="director-job-run-on-incoming-connect-interval"
               name="run_on_incoming_connect_interval" type="number" min="0">

        <label for="director-job-enabled">Enabled</label>
        <select id="director-job-enabled" name="enabled">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="director-job-prefix-links">PrefixLinks</label>
        <select id="director-job-prefix-links" name="prefix_links">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="director-job-prune-jobs">PruneJobs</label>
        <select id="director-job-prune-jobs" name="prune_jobs">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="director-job-prune-files">PruneFiles</label>
        <select id="director-job-prune-files" name="prune_files">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="director-job-prune-volumes">PruneVolumes</label>
        <select id="director-job-prune-volumes" name="prune_volumes">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="director-job-purge-migration-job">PurgeMigrationJob</label>
        <select id="director-job-purge-migration-job"
                name="purge_migration_job">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="director-job-spool-attributes">SpoolAttributes</label>
        <select id="director-job-spool-attributes" name="spool_attributes">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="director-job-spool-data">SpoolData</label>
        <select id="director-job-spool-data" name="spool_data">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="director-job-rerun-failed-levels">RerunFailedLevels</label>
        <select id="director-job-rerun-failed-levels"
                name="rerun_failed_levels">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="director-job-prefer-mounted-volumes">PreferMountedVolumes</label>
        <select id="director-job-prefer-mounted-volumes"
                name="prefer_mounted_volumes">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="director-job-reschedule-on-error">RescheduleOnError</label>
        <select id="director-job-reschedule-on-error"
                name="reschedule_on_error">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="director-job-allow-mixed-priority">AllowMixedPriority</label>
        <select id="director-job-allow-mixed-priority"
                name="allow_mixed_priority">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="director-job-accurate">Accurate</label>
        <select id="director-job-accurate" name="accurate">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="director-job-allow-duplicate-jobs">AllowDuplicateJobs</label>
        <select id="director-job-allow-duplicate-jobs"
                name="allow_duplicate_jobs">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="director-job-allow-higher-duplicates">AllowHigherDuplicates</label>
        <select id="director-job-allow-higher-duplicates"
                name="allow_higher_duplicates">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="director-job-cancel-lower-level-duplicates">CancelLowerLevelDuplicates</label>
        <select id="director-job-cancel-lower-level-duplicates"
                name="cancel_lower_level_duplicates">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="director-job-cancel-queued-duplicates">CancelQueuedDuplicates</label>
        <select id="director-job-cancel-queued-duplicates"
                name="cancel_queued_duplicates">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="director-job-cancel-running-duplicates">CancelRunningDuplicates</label>
        <select id="director-job-cancel-running-duplicates"
                name="cancel_running_duplicates">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="director-job-save-file-history">SaveFileHistory</label>
        <select id="director-job-save-file-history" name="save_file_history">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="director-job-always-incremental">AlwaysIncremental</label>
        <select id="director-job-always-incremental"
                name="always_incremental">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

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

        <label for="director-jobdefs-backup-format">BackupFormat</label>
        <input id="director-jobdefs-backup-format" name="backup_format"
               placeholder="Native">

        <label for="director-jobdefs-protocol">Protocol</label>
        <input id="director-jobdefs-protocol" name="protocol"
               placeholder="Native">

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

        <label for="director-jobdefs-verify-job">JobToVerify</label>
        <input id="director-jobdefs-verify-job" name="verify_job">

        <label for="director-jobdefs-jobdefs">JobDefs</label>
        <input id="director-jobdefs-jobdefs" name="jobdefs"
               placeholder="DefaultJob">

        <label for="director-jobdefs-virtual-full-backup-pool">VirtualFullBackupPool</label>
        <input id="director-jobdefs-virtual-full-backup-pool"
               name="virtual_full_backup_pool">

        <label for="director-jobdefs-next-pool">NextPool</label>
        <input id="director-jobdefs-next-pool" name="next_pool">

        <label for="director-jobdefs-where">Where</label>
        <input id="director-jobdefs-where" name="where">

        <label for="director-jobdefs-replace">Replace</label>
        <input id="director-jobdefs-replace" name="replace"
               placeholder="always">

        <label for="director-jobdefs-regex-where">RegexWhere</label>
        <input id="director-jobdefs-regex-where" name="regex_where">

        <label for="director-jobdefs-strip-prefix">StripPrefix</label>
        <input id="director-jobdefs-strip-prefix" name="strip_prefix">

        <label for="director-jobdefs-add-prefix">AddPrefix</label>
        <input id="director-jobdefs-add-prefix" name="add_prefix">

        <label for="director-jobdefs-add-suffix">AddSuffix</label>
        <input id="director-jobdefs-add-suffix" name="add_suffix">

        <label for="director-jobdefs-bootstrap">Bootstrap</label>
        <input id="director-jobdefs-bootstrap" name="bootstrap">

        <label for="director-jobdefs-write-bootstrap">WriteBootstrap</label>
        <input id="director-jobdefs-write-bootstrap" name="write_bootstrap">

        <label for="director-jobdefs-write-verify-list">WriteVerifyList</label>
        <input id="director-jobdefs-write-verify-list" name="write_verify_list">

        <label for="director-jobdefs-run-entries">Run entries</label>
        <textarea id="director-jobdefs-run-entries" name="run_entries" rows="3"
                  placeholder="Level=Full monthly 1st sat at 21:00"></textarea>

        <label for="director-jobdefs-run-before-job-entries">RunBeforeJob entries</label>
        <textarea id="director-jobdefs-run-before-job-entries"
                  name="run_before_job_entries" rows="2"
                  placeholder="/usr/lib/bareos/scripts/pre-job.sh"></textarea>

        <label for="director-jobdefs-run-after-job-entries">RunAfterJob entries</label>
        <textarea id="director-jobdefs-run-after-job-entries"
                  name="run_after_job_entries" rows="2"
                  placeholder="/usr/lib/bareos/scripts/post-job.sh"></textarea>

        <label for="director-jobdefs-run-after-failed-job-entries">RunAfterFailedJob entries</label>
        <textarea id="director-jobdefs-run-after-failed-job-entries"
                  name="run_after_failed_job_entries" rows="2"
                  placeholder="/usr/lib/bareos/scripts/fail-job.sh"></textarea>

        <label for="director-jobdefs-client-run-before-job-entries">ClientRunBeforeJob entries</label>
        <textarea id="director-jobdefs-client-run-before-job-entries"
                  name="client_run_before_job_entries" rows="2"
                  placeholder="/usr/lib/bareos/scripts/client-pre.sh"></textarea>

        <label for="director-jobdefs-client-run-after-job-entries">ClientRunAfterJob entries</label>
        <textarea id="director-jobdefs-client-run-after-job-entries"
                  name="client_run_after_job_entries" rows="2"
                  placeholder="/usr/lib/bareos/scripts/client-post.sh"></textarea>

        <label for="director-jobdefs-runscript-blocks">RunScript blocks</label>
        <textarea id="director-jobdefs-runscript-blocks"
                  name="runscript_blocks" rows="8"
                  placeholder="RunScript {&#10;  Console = &quot;status dir&quot;&#10;  RunsWhen = After&#10;  RunsOnFailure = yes&#10;}"></textarea>

        <label for="director-jobdefs-maximum-bandwidth">MaximumBandwidth</label>
        <input id="director-jobdefs-maximum-bandwidth"
               name="maximum_bandwidth" type="number" min="0">

        <label for="director-jobdefs-max-run-sched-time">MaxRunSchedTime</label>
        <input id="director-jobdefs-max-run-sched-time"
               name="max_run_sched_time" type="number" min="0">

        <label for="director-jobdefs-max-run-time">MaxRunTime</label>
        <input id="director-jobdefs-max-run-time" name="max_run_time"
               type="number" min="0">

        <label for="director-jobdefs-full-max-runtime">FullMaxRunTime</label>
        <input id="director-jobdefs-full-max-runtime" name="full_max_runtime"
               type="number" min="0">

        <label for="director-jobdefs-incremental-max-runtime">IncrementalMaxRunTime</label>
        <input id="director-jobdefs-incremental-max-runtime"
               name="incremental_max_runtime" type="number" min="0">

        <label for="director-jobdefs-differential-max-runtime">DifferentialMaxRunTime</label>
        <input id="director-jobdefs-differential-max-runtime"
               name="differential_max_runtime" type="number" min="0">

        <label for="director-jobdefs-max-wait-time">MaxWaitTime</label>
        <input id="director-jobdefs-max-wait-time" name="max_wait_time"
               type="number" min="0">

        <label for="director-jobdefs-max-start-delay">MaxStartDelay</label>
        <input id="director-jobdefs-max-start-delay" name="max_start_delay"
               type="number" min="0">

        <label for="director-jobdefs-max-full-interval">MaxFullInterval</label>
        <input id="director-jobdefs-max-full-interval"
               name="max_full_interval" type="number" min="0">

        <label for="director-jobdefs-max-virtual-full-interval">MaxVirtualFullInterval</label>
        <input id="director-jobdefs-max-virtual-full-interval"
               name="max_virtual_full_interval" type="number" min="0">

        <label for="director-jobdefs-max-diff-interval">MaxDiffInterval</label>
        <input id="director-jobdefs-max-diff-interval" name="max_diff_interval"
               type="number" min="0">

        <label for="director-jobdefs-spool-size">SpoolSize</label>
        <input id="director-jobdefs-spool-size" name="spool_size"
               type="number" min="0">

        <label for="director-jobdefs-maximum-concurrent-jobs">MaximumConcurrentJobs</label>
        <input id="director-jobdefs-maximum-concurrent-jobs"
               name="maximum_concurrent_jobs" type="number" min="0">

        <label for="director-jobdefs-reschedule-interval">RescheduleInterval</label>
        <input id="director-jobdefs-reschedule-interval"
               name="reschedule_interval" type="number" min="0">

        <label for="director-jobdefs-reschedule-times">RescheduleTimes</label>
        <input id="director-jobdefs-reschedule-times" name="reschedule_times"
               type="number" min="0">

        <label for="director-jobdefs-priority">Priority</label>
        <input id="director-jobdefs-priority" name="priority" type="number">

        <label for="director-jobdefs-selection-type">SelectionType</label>
        <input id="director-jobdefs-selection-type" name="selection_type"
               placeholder="Matching">

        <label for="director-jobdefs-selection-pattern">SelectionPattern</label>
        <input id="director-jobdefs-selection-pattern"
               name="selection_pattern">

        <label for="director-jobdefs-file-history-size">FileHistorySize</label>
        <input id="director-jobdefs-file-history-size"
               name="file_history_size" type="number" min="0">

        <label for="director-jobdefs-fd-plugin-options">FdPluginOptions</label>
        <textarea id="director-jobdefs-fd-plugin-options"
                  name="fd_plugin_options" rows="2"
                  placeholder="python:module_path=/opt/bareos"></textarea>

        <label for="director-jobdefs-sd-plugin-options">SdPluginOptions</label>
        <textarea id="director-jobdefs-sd-plugin-options"
                  name="sd_plugin_options" rows="2"
                  placeholder="compression=on"></textarea>

        <label for="director-jobdefs-dir-plugin-options">DirPluginOptions</label>
        <textarea id="director-jobdefs-dir-plugin-options"
                  name="dir_plugin_options" rows="2"
                  placeholder="audit=verbose"></textarea>

        <label for="director-jobdefs-max-concurrent-copies">MaxConcurrentCopies</label>
        <input id="director-jobdefs-max-concurrent-copies"
               name="max_concurrent_copies" type="number" min="0">

        <label for="director-jobdefs-always-incremental-job-retention">AlwaysIncrementalJobRetention</label>
        <input id="director-jobdefs-always-incremental-job-retention"
               name="always_incremental_job_retention" type="number" min="0">

        <label for="director-jobdefs-always-incremental-keep-number">AlwaysIncrementalKeepNumber</label>
        <input id="director-jobdefs-always-incremental-keep-number"
               name="always_incremental_keep_number" type="number" min="0">

        <label for="director-jobdefs-always-incremental-max-full-age">AlwaysIncrementalMaxFullAge</label>
        <input id="director-jobdefs-always-incremental-max-full-age"
               name="always_incremental_max_full_age" type="number" min="0">

        <label for="director-jobdefs-max-full-consolidations">MaxFullConsolidations</label>
        <input id="director-jobdefs-max-full-consolidations"
               name="max_full_consolidations" type="number" min="0">

        <label for="director-jobdefs-run-on-incoming-connect-interval">RunOnIncomingConnectInterval</label>
        <input id="director-jobdefs-run-on-incoming-connect-interval"
               name="run_on_incoming_connect_interval" type="number" min="0">

        <label for="director-jobdefs-enabled">Enabled</label>
        <select id="director-jobdefs-enabled" name="enabled">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="director-jobdefs-prefix-links">PrefixLinks</label>
        <select id="director-jobdefs-prefix-links" name="prefix_links">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="director-jobdefs-prune-jobs">PruneJobs</label>
        <select id="director-jobdefs-prune-jobs" name="prune_jobs">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="director-jobdefs-prune-files">PruneFiles</label>
        <select id="director-jobdefs-prune-files" name="prune_files">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="director-jobdefs-prune-volumes">PruneVolumes</label>
        <select id="director-jobdefs-prune-volumes" name="prune_volumes">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="director-jobdefs-purge-migration-job">PurgeMigrationJob</label>
        <select id="director-jobdefs-purge-migration-job"
                name="purge_migration_job">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="director-jobdefs-spool-attributes">SpoolAttributes</label>
        <select id="director-jobdefs-spool-attributes"
                name="spool_attributes">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="director-jobdefs-spool-data">SpoolData</label>
        <select id="director-jobdefs-spool-data" name="spool_data">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="director-jobdefs-rerun-failed-levels">RerunFailedLevels</label>
        <select id="director-jobdefs-rerun-failed-levels"
                name="rerun_failed_levels">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="director-jobdefs-prefer-mounted-volumes">PreferMountedVolumes</label>
        <select id="director-jobdefs-prefer-mounted-volumes"
                name="prefer_mounted_volumes">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="director-jobdefs-reschedule-on-error">RescheduleOnError</label>
        <select id="director-jobdefs-reschedule-on-error"
                name="reschedule_on_error">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="director-jobdefs-allow-mixed-priority">AllowMixedPriority</label>
        <select id="director-jobdefs-allow-mixed-priority"
                name="allow_mixed_priority">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="director-jobdefs-accurate">Accurate</label>
        <select id="director-jobdefs-accurate" name="accurate">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="director-jobdefs-allow-duplicate-jobs">AllowDuplicateJobs</label>
        <select id="director-jobdefs-allow-duplicate-jobs"
                name="allow_duplicate_jobs">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="director-jobdefs-allow-higher-duplicates">AllowHigherDuplicates</label>
        <select id="director-jobdefs-allow-higher-duplicates"
                name="allow_higher_duplicates">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="director-jobdefs-cancel-lower-level-duplicates">CancelLowerLevelDuplicates</label>
        <select id="director-jobdefs-cancel-lower-level-duplicates"
                name="cancel_lower_level_duplicates">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="director-jobdefs-cancel-queued-duplicates">CancelQueuedDuplicates</label>
        <select id="director-jobdefs-cancel-queued-duplicates"
                name="cancel_queued_duplicates">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="director-jobdefs-cancel-running-duplicates">CancelRunningDuplicates</label>
        <select id="director-jobdefs-cancel-running-duplicates"
                name="cancel_running_duplicates">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="director-jobdefs-save-file-history">SaveFileHistory</label>
        <select id="director-jobdefs-save-file-history"
                name="save_file_history">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="director-jobdefs-always-incremental">AlwaysIncremental</label>
        <select id="director-jobdefs-always-incremental"
                name="always_incremental">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

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

        <label for="director-daemon-addresses">Addresses</label>
        <textarea id="director-daemon-addresses" name="addresses"
                  rows="3"
                  placeholder="host[ipv4;192.0.2.44;9101]&#10;host[ipv6;::1;9101]"></textarea>

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

        <label for="director-daemon-query-file">QueryFile</label>
        <input id="director-daemon-query-file" name="query_file"
               placeholder="/tmp/scripts/query.sql">

        <label for="director-daemon-subscriptions">Subscriptions</label>
        <input id="director-daemon-subscriptions" name="subscriptions"
               type="number" min="0" placeholder="0">

        <label for="director-daemon-maximum-concurrent-jobs">Maximum concurrent jobs</label>
        <input id="director-daemon-maximum-concurrent-jobs"
               name="maximum_concurrent_jobs" type="number" min="0"
               placeholder="10">

        <label for="director-daemon-maximum-console-connections">Maximum console connections</label>
        <input id="director-daemon-maximum-console-connections"
               name="maximum_console_connections" type="number" min="0"
               placeholder="0">

        <label for="director-daemon-password">Password</label>
        <input id="director-daemon-password" name="password"
               placeholder="[md5]supersecret">

        <label for="director-daemon-absolute-job-timeout">Absolute job timeout</label>
        <input id="director-daemon-absolute-job-timeout"
               name="absolute_job_timeout" type="number" min="0"
               placeholder="0">

        <label for="director-daemon-tls-authenticate">TlsAuthenticate</label>
        <select id="director-daemon-tls-authenticate"
                name="tls_authenticate">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="director-daemon-tls-enable">TlsEnable</label>
        <select id="director-daemon-tls-enable" name="tls_enable">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="director-daemon-tls-require">TlsRequire</label>
        <select id="director-daemon-tls-require" name="tls_require">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="director-daemon-tls-verify-peer">TlsVerifyPeer</label>
        <select id="director-daemon-tls-verify-peer"
                name="tls_verify_peer">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="director-daemon-tls-cipher-list">TLS cipher list</label>
        <input id="director-daemon-tls-cipher-list" name="tls_cipher_list"
               placeholder="HIGH">

        <label for="director-daemon-tls-cipher-suites">TLS cipher suites</label>
        <input id="director-daemon-tls-cipher-suites" name="tls_cipher_suites"
               placeholder="TLS_AES_256_GCM_SHA384">

        <label for="director-daemon-tls-dh-file">TLS DH file</label>
        <input id="director-daemon-tls-dh-file" name="tls_dh_file"
               placeholder="/etc/bareos/dh4096.pem">

        <label for="director-daemon-tls-protocol">TLS protocol</label>
        <input id="director-daemon-tls-protocol" name="tls_protocol"
               placeholder="+TLSv1.2:+TLSv1.3">

        <label for="director-daemon-tls-ca-certificate-file">TLS CA certificate file</label>
        <input id="director-daemon-tls-ca-certificate-file"
               name="tls_ca_certificate_file"
               placeholder="/etc/bareos/ca.pem">

        <label for="director-daemon-tls-ca-certificate-dir">TLS CA certificate dir</label>
        <input id="director-daemon-tls-ca-certificate-dir"
               name="tls_ca_certificate_dir"
               placeholder="/etc/ssl/certs">

        <label for="director-daemon-tls-certificate-revocation-list">TLS certificate revocation list</label>
        <input id="director-daemon-tls-certificate-revocation-list"
               name="tls_certificate_revocation_list"
               placeholder="/etc/bareos/crl.pem">

        <label for="director-daemon-tls-certificate">TLS certificate</label>
        <input id="director-daemon-tls-certificate" name="tls_certificate"
               placeholder="/etc/bareos/director-cert.pem">

        <label for="director-daemon-tls-key">TLS key</label>
        <input id="director-daemon-tls-key" name="tls_key"
               placeholder="/etc/bareos/director-key.pem">

        <label for="director-daemon-tls-allowed-cn">TLS allowed CNs</label>
        <textarea id="director-daemon-tls-allowed-cn" name="tls_allowed_cn"
                  rows="3"
                  placeholder="director.example.test&#10;backup.example.test"></textarea>

        <label for="director-daemon-ver-id">VerId</label>
        <input id="director-daemon-ver-id" name="ver_id"
               placeholder="bareos-dir-custom">

        <label for="director-daemon-log-timestamp-format">Log timestamp format</label>
        <input id="director-daemon-log-timestamp-format"
               name="log_timestamp_format" placeholder="%d-%b %H:%M">

        <label for="director-daemon-secure-erase-command">Secure erase command</label>
        <input id="director-daemon-secure-erase-command"
               name="secure_erase_command"
               placeholder="/usr/bin/shred -n 3 -u">

        <label for="director-daemon-enable-ktls">EnableKtls</label>
        <select id="director-daemon-enable-ktls" name="enable_ktls">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="director-daemon-fd-connect-timeout">FD connect timeout</label>
        <input id="director-daemon-fd-connect-timeout"
               name="fd_connect_timeout" type="number" min="0"
               placeholder="1800">

        <label for="director-daemon-sd-connect-timeout">SD connect timeout</label>
        <input id="director-daemon-sd-connect-timeout"
               name="sd_connect_timeout" type="number" min="0"
               placeholder="1800">

        <label for="director-daemon-heartbeat-interval">Heartbeat interval</label>
        <input id="director-daemon-heartbeat-interval"
               name="heartbeat_interval" type="number" min="0"
               placeholder="0">

        <label for="director-daemon-statistics-retention">Statistics retention</label>
        <input id="director-daemon-statistics-retention"
               name="statistics_retention" type="number" min="0"
               placeholder="0">

        <label for="director-daemon-statistics-collect-interval">Statistics collect interval</label>
        <input id="director-daemon-statistics-collect-interval"
               name="statistics_collect_interval" type="number" min="0"
               placeholder="0">

        <label for="director-daemon-description">Description</label>
        <input id="director-daemon-description" name="description"
               placeholder="Managed director daemon resource">

        <label for="director-daemon-key-encryption-key">Key encryption key</label>
        <input id="director-daemon-key-encryption-key"
               name="key_encryption_key" placeholder="managed-key">

        <label for="director-daemon-ndmp-snooping">NdmpSnooping</label>
        <select id="director-daemon-ndmp-snooping" name="ndmp_snooping">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="director-daemon-ndmp-log-level">NDMP log level</label>
        <input id="director-daemon-ndmp-log-level" name="ndmp_log_level"
               type="number" min="0" placeholder="4">

        <label for="director-daemon-ndmp-namelist-fhinfo-set-zero-for-invalid-uquad">NdmpNamelistFhinfoSetZeroForInvalidUquad</label>
        <select id="director-daemon-ndmp-namelist-fhinfo-set-zero-for-invalid-uquad"
                name="ndmp_namelist_fhinfo_set_zero_for_invalid_uquad">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="director-daemon-auditing">Auditing</label>
        <select id="director-daemon-auditing" name="auditing">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="director-daemon-audit-events">Audit events</label>
        <textarea id="director-daemon-audit-events" name="audit_events"
                  rows="3" placeholder="all&#10;jobstart&#10;jobend"></textarea>

        <label for="director-daemon-working-directory">Working directory</label>
        <input id="director-daemon-working-directory" name="working_directory"
               placeholder="/var/lib/bareos">

        <label for="director-daemon-plugin-directory">Plugin directory</label>
        <input id="director-daemon-plugin-directory" name="plugin_directory"
               placeholder="/usr/lib/bareos/plugins">

        <label for="director-daemon-plugin-names">Plugin names</label>
        <textarea id="director-daemon-plugin-names" name="plugin_names"
                  rows="3" placeholder="python&#10;grpc"></textarea>

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

        <label for="storage-director-monitor">Monitor</label>
        <select id="storage-director-monitor" name="monitor">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="storage-director-maximum-bandwidth-per-job">Maximum bandwidth per job</label>
        <input id="storage-director-maximum-bandwidth-per-job"
               name="maximum_bandwidth_per_job" type="number" min="0"
               placeholder="0">

        <label for="storage-director-key-encryption-key">Key encryption key</label>
        <input id="storage-director-key-encryption-key"
               name="key_encryption_key"
               placeholder="managed-key">

        <label for="storage-director-tls-authenticate">TlsAuthenticate</label>
        <select id="storage-director-tls-authenticate"
                name="tls_authenticate">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="storage-director-tls-enable">TlsEnable</label>
        <select id="storage-director-tls-enable" name="tls_enable">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="storage-director-tls-require">TlsRequire</label>
        <select id="storage-director-tls-require" name="tls_require">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="storage-director-tls-verify-peer">TlsVerifyPeer</label>
        <select id="storage-director-tls-verify-peer"
                name="tls_verify_peer">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="storage-director-tls-cipher-list">TLS cipher list</label>
        <input id="storage-director-tls-cipher-list" name="tls_cipher_list"
               placeholder="HIGH">

        <label for="storage-director-tls-cipher-suites">TLS cipher suites</label>
        <input id="storage-director-tls-cipher-suites" name="tls_cipher_suites"
               placeholder="TLS_AES_256_GCM_SHA384">

        <label for="storage-director-tls-dh-file">TLS DH file</label>
        <input id="storage-director-tls-dh-file" name="tls_dh_file"
               placeholder="/etc/bareos/dh4096.pem">

        <label for="storage-director-tls-protocol">TLS protocol</label>
        <input id="storage-director-tls-protocol" name="tls_protocol"
               placeholder="+TLSv1.2:+TLSv1.3">

        <label for="storage-director-tls-ca-certificate-file">TLS CA certificate file</label>
        <input id="storage-director-tls-ca-certificate-file"
               name="tls_ca_certificate_file"
               placeholder="/etc/bareos/ca.pem">

        <label for="storage-director-tls-ca-certificate-dir">TLS CA certificate dir</label>
        <input id="storage-director-tls-ca-certificate-dir"
               name="tls_ca_certificate_dir"
               placeholder="/etc/ssl/certs">

        <label for="storage-director-tls-certificate-revocation-list">TLS certificate revocation list</label>
        <input id="storage-director-tls-certificate-revocation-list"
               name="tls_certificate_revocation_list"
               placeholder="/etc/bareos/crl.pem">

        <label for="storage-director-tls-certificate">TLS certificate</label>
        <input id="storage-director-tls-certificate" name="tls_certificate"
               placeholder="/etc/bareos/director-cert.pem">

        <label for="storage-director-tls-key">TLS key</label>
        <input id="storage-director-tls-key" name="tls_key"
               placeholder="/etc/bareos/director-key.pem">

        <label for="storage-director-tls-allowed-cn">TLS allowed CNs</label>
        <textarea id="storage-director-tls-allowed-cn" name="tls_allowed_cn"
                  placeholder="director.example.test&#10;backup.example.test"></textarea>

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
        <input id="storage-device-media-type" name="media_type"
               placeholder="File">

        <label for="storage-device-archive-device">Archive device</label>
        <input id="storage-device-archive-device" name="archive_device"
               placeholder="/tmp/bareos-storage">

        <label for="storage-device-device-type">Device type</label>
        <input id="storage-device-device-type" name="device_type"
               placeholder="file">

        <label for="storage-device-access-mode">AccessMode</label>
        <input id="storage-device-access-mode" name="access_mode"
               placeholder="readonly">

        <label for="storage-device-device-options">DeviceOptions</label>
        <input id="storage-device-device-options" name="device_options"
               placeholder="Block Size = 64k">

        <label for="storage-device-diagnostic-device">DiagnosticDevice</label>
        <input id="storage-device-diagnostic-device" name="diagnostic_device"
               placeholder="/tmp/managed-storage.diag">

        <label for="storage-device-hardware-end-of-file">HardwareEndOfFile</label>
        <select id="storage-device-hardware-end-of-file"
                name="hardware_end_of_file">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="storage-device-hardware-end-of-medium">HardwareEndOfMedium</label>
        <select id="storage-device-hardware-end-of-medium"
                name="hardware_end_of_medium">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="storage-device-backward-space-record">BackwardSpaceRecord</label>
        <select id="storage-device-backward-space-record"
                name="backward_space_record">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="storage-device-backward-space-file">BackwardSpaceFile</label>
        <select id="storage-device-backward-space-file"
                name="backward_space_file">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="storage-device-bsf-at-eom">BsfAtEom</label>
        <select id="storage-device-bsf-at-eom" name="bsf_at_eom">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="storage-device-two-eof">TwoEof</label>
        <select id="storage-device-two-eof" name="two_eof">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="storage-device-forward-space-record">ForwardSpaceRecord</label>
        <select id="storage-device-forward-space-record"
                name="forward_space_record">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="storage-device-forward-space-file">ForwardSpaceFile</label>
        <select id="storage-device-forward-space-file"
                name="forward_space_file">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="storage-device-fast-forward-space-file">FastForwardSpaceFile</label>
        <select id="storage-device-fast-forward-space-file"
                name="fast_forward_space_file">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="storage-device-removable-media">RemovableMedia</label>
        <select id="storage-device-removable-media" name="removable_media">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="storage-device-random-access">RandomAccess</label>
        <select id="storage-device-random-access" name="random_access">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="storage-device-automatic-mount">AutomaticMount</label>
        <select id="storage-device-automatic-mount" name="automatic_mount">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="storage-device-label-media">LabelMedia</label>
        <select id="storage-device-label-media" name="label_media">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="storage-device-always-open">AlwaysOpen</label>
        <select id="storage-device-always-open" name="always_open">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="storage-device-autochanger">Autochanger</label>
        <select id="storage-device-autochanger" name="autochanger">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="storage-device-close-on-poll">CloseOnPoll</label>
        <select id="storage-device-close-on-poll" name="close_on_poll">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="storage-device-block-positioning">BlockPositioning</label>
        <select id="storage-device-block-positioning" name="block_positioning">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="storage-device-use-mtiocget">UseMtiocget</label>
        <select id="storage-device-use-mtiocget" name="use_mtiocget">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="storage-device-check-labels">CheckLabels</label>
        <select id="storage-device-check-labels" name="check_labels">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="storage-device-requires-mount">RequiresMount</label>
        <select id="storage-device-requires-mount" name="requires_mount">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="storage-device-offline-on-unmount">OfflineOnUnmount</label>
        <select id="storage-device-offline-on-unmount"
                name="offline_on_unmount">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="storage-device-block-checksum">BlockChecksum</label>
        <select id="storage-device-block-checksum" name="block_checksum">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="storage-device-auto-select">AutoSelect</label>
        <select id="storage-device-auto-select" name="auto_select">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="storage-device-changer-device">ChangerDevice</label>
        <input id="storage-device-changer-device" name="changer_device"
               placeholder="/dev/sg0">

        <label for="storage-device-changer-command">ChangerCommand</label>
        <input id="storage-device-changer-command" name="changer_command"
               placeholder="/usr/lib/bareos/mtx-changer %c %o">

        <label for="storage-device-alert-command">AlertCommand</label>
        <input id="storage-device-alert-command" name="alert_command"
               placeholder="/usr/lib/bareos/alert.sh">

        <label for="storage-device-maximum-changer-wait">MaximumChangerWait</label>
        <input id="storage-device-maximum-changer-wait"
               name="maximum_changer_wait" type="number" min="0"
               placeholder="0">

        <label for="storage-device-maximum-open-wait">MaximumOpenWait</label>
        <input id="storage-device-maximum-open-wait"
               name="maximum_open_wait" type="number" min="0"
               placeholder="0">

        <label for="storage-device-maximum-open-volumes">MaximumOpenVolumes</label>
        <input id="storage-device-maximum-open-volumes"
               name="maximum_open_volumes" type="number" min="0"
               placeholder="0">

        <label for="storage-device-maximum-network-buffer-size">MaximumNetworkBufferSize</label>
        <input id="storage-device-maximum-network-buffer-size"
               name="maximum_network_buffer_size" type="number" min="0"
               placeholder="0">

        <label for="storage-device-volume-poll-interval">VolumePollInterval</label>
        <input id="storage-device-volume-poll-interval"
               name="volume_poll_interval" type="number" min="0"
               placeholder="0">

        <label for="storage-device-maximum-rewind-wait">MaximumRewindWait</label>
        <input id="storage-device-maximum-rewind-wait"
               name="maximum_rewind_wait" type="number" min="0"
               placeholder="0">

        <label for="storage-device-label-block-size">LabelBlockSize</label>
        <input id="storage-device-label-block-size"
               name="label_block_size" type="number" min="0"
               placeholder="0">

        <label for="storage-device-minimum-block-size">MinimumBlockSize</label>
        <input id="storage-device-minimum-block-size"
               name="minimum_block_size" type="number" min="0"
               placeholder="0">

        <label for="storage-device-maximum-block-size">MaximumBlockSize</label>
        <input id="storage-device-maximum-block-size"
               name="maximum_block_size" type="number" min="0"
               placeholder="0">

        <label for="storage-device-maximum-file-size">MaximumFileSize</label>
        <input id="storage-device-maximum-file-size"
               name="maximum_file_size" type="number" min="0"
               placeholder="0">

        <label for="storage-device-volume-capacity">VolumeCapacity</label>
        <input id="storage-device-volume-capacity"
               name="volume_capacity" type="number" min="0"
               placeholder="0">

        <label for="storage-device-maximum-concurrent-jobs">MaximumConcurrentJobs</label>
        <input id="storage-device-maximum-concurrent-jobs"
               name="maximum_concurrent_jobs" type="number" min="0"
               placeholder="0">

        <label for="storage-device-spool-directory">SpoolDirectory</label>
        <input id="storage-device-spool-directory" name="spool_directory"
               placeholder="/var/spool/bareos">

        <label for="storage-device-maximum-spool-size">MaximumSpoolSize</label>
        <input id="storage-device-maximum-spool-size"
               name="maximum_spool_size" type="number" min="0"
               placeholder="0">

        <label for="storage-device-maximum-job-spool-size">MaximumJobSpoolSize</label>
        <input id="storage-device-maximum-job-spool-size"
               name="maximum_job_spool_size" type="number" min="0"
               placeholder="0">

        <label for="storage-device-drive-index">DriveIndex</label>
        <input id="storage-device-drive-index" name="drive_index"
               type="number" min="0" max="65535" placeholder="0">

        <label for="storage-device-mount-point">MountPoint</label>
        <input id="storage-device-mount-point" name="mount_point"
               placeholder="/mnt/tape">

        <label for="storage-device-mount-command">MountCommand</label>
        <input id="storage-device-mount-command" name="mount_command"
               placeholder="/usr/bin/mount /mnt/tape">

        <label for="storage-device-unmount-command">UnmountCommand</label>
        <input id="storage-device-unmount-command" name="unmount_command"
               placeholder="/usr/bin/umount /mnt/tape">

        <label for="storage-device-label-type">LabelType</label>
        <input id="storage-device-label-type" name="label_type"
               placeholder="ibm">

        <label for="storage-device-no-rewind-on-close">NoRewindOnClose</label>
        <select id="storage-device-no-rewind-on-close"
                name="no_rewind_on_close">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="storage-device-drive-tape-alert-enabled">DriveTapeAlertEnabled</label>
        <select id="storage-device-drive-tape-alert-enabled"
                name="drive_tape_alert_enabled">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="storage-device-drive-crypto-enabled">DriveCryptoEnabled</label>
        <select id="storage-device-drive-crypto-enabled"
                name="drive_crypto_enabled">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="storage-device-query-crypto-status">QueryCryptoStatus</label>
        <select id="storage-device-query-crypto-status"
                name="query_crypto_status">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="storage-device-auto-deflate">AutoDeflate</label>
        <input id="storage-device-auto-deflate" name="auto_deflate"
               placeholder="writeonly">

        <label for="storage-device-auto-deflate-algorithm">AutoDeflateAlgorithm</label>
        <input id="storage-device-auto-deflate-algorithm"
               name="auto_deflate_algorithm" placeholder="lz4hc">

        <label for="storage-device-auto-deflate-level">AutoDeflateLevel</label>
        <input id="storage-device-auto-deflate-level"
               name="auto_deflate_level" type="number" min="0" max="65535"
               placeholder="0">

        <label for="storage-device-auto-inflate">AutoInflate</label>
        <input id="storage-device-auto-inflate" name="auto_inflate"
               placeholder="in">

        <label for="storage-device-collect-statistics">CollectStatistics</label>
        <select id="storage-device-collect-statistics"
                name="collect_statistics">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="storage-device-eof-on-error-is-eot">EofOnErrorIsEot</label>
        <select id="storage-device-eof-on-error-is-eot"
                name="eof_on_error_is_eot">
          <option value="">Keep existing</option>
          <option value="true">Yes</option>
          <option value="false">No</option>
        </select>

        <label for="storage-device-count">Count</label>
        <input id="storage-device-count" name="count" type="number" min="0"
               placeholder="0">

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

        <label for="storage-messages-mail-command">MailCommand</label>
        <input id="storage-messages-mail-command" name="mail_command"
               placeholder="/usr/lib/bareos/mail-storage %r">

        <label for="storage-messages-operator-command">OperatorCommand</label>
        <input id="storage-messages-operator-command" name="operator_command"
               placeholder="/usr/lib/bareos/operator-storage %r">

        <label for="storage-messages-timestamp-format">TimestampFormat</label>
        <input id="storage-messages-timestamp-format" name="timestamp_format"
               placeholder="%Y-%m-%d %H:%M:%S">

        <label for="storage-messages-syslog-entries">Syslog entries</label>
        <textarea id="storage-messages-syslog-entries" name="syslog_entries"
                  rows="2" placeholder="mail = all, !skipped"></textarea>

        <label for="storage-messages-mail-entries">Mail entries</label>
        <textarea id="storage-messages-mail-entries" name="mail_entries"
                  rows="2" placeholder="ops@example.test = all"></textarea>

        <label for="storage-messages-mail-on-error-entries">MailOnError entries</label>
        <textarea id="storage-messages-mail-on-error-entries"
                  name="mail_on_error_entries"
                  rows="2" placeholder="ops@example.test = all"></textarea>

        <label for="storage-messages-mail-on-success-entries">MailOnSuccess entries</label>
        <textarea id="storage-messages-mail-on-success-entries"
                  name="mail_on_success_entries"
                  rows="2" placeholder="ops@example.test = all"></textarea>

        <label for="storage-messages-file-entries">File entries</label>
        <textarea id="storage-messages-file-entries" name="file_entries"
                  rows="2" placeholder="\"/var/log/bareos/storage.log\" = all"></textarea>

        <label for="storage-messages-append-entries">Append entries</label>
        <textarea id="storage-messages-append-entries" name="append_entries"
                  rows="2" placeholder="\"/var/log/bareos/storage.log\" = all"></textarea>

        <label for="storage-messages-stdout-entries">Stdout entries</label>
        <textarea id="storage-messages-stdout-entries" name="stdout_entries"
                  rows="2" placeholder="all, !skipped"></textarea>

        <label for="storage-messages-stderr-entries">Stderr entries</label>
        <textarea id="storage-messages-stderr-entries" name="stderr_entries"
                  rows="2" placeholder="all, !skipped"></textarea>

        <label for="storage-messages-director-entries">Director entries</label>
        <textarea id="storage-messages-director-entries" name="director_entries"
                  rows="2" placeholder="bareos-dir = all"></textarea>

        <label for="storage-messages-console-entries">Console entries</label>
        <textarea id="storage-messages-console-entries" name="console_entries"
                  rows="2" placeholder="all, !skipped"></textarea>

        <label for="storage-messages-operator-entries">Operator entries</label>
        <textarea id="storage-messages-operator-entries" name="operator_entries"
                  rows="2" placeholder="all"></textarea>

        <label for="storage-messages-catalog-entries">Catalog entries</label>
        <textarea id="storage-messages-catalog-entries" name="catalog_entries"
                  rows="2" placeholder="all"></textarea>

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
        <input id="storage-ndmp-username" name="username"
               placeholder="ndmp-user">

        <label for="storage-ndmp-password">Password</label>
        <input id="storage-ndmp-password" name="password"
               placeholder="cleartext or [md5]hash">

        <label for="storage-ndmp-auth-type">Auth type</label>
        <input id="storage-ndmp-auth-type" name="auth_type"
               placeholder="MD5">

        <label for="storage-ndmp-log-level">Log level</label>
        <input id="storage-ndmp-log-level" name="log_level" type="number" min="0"
               placeholder="4">

        <label for="storage-ndmp-description">Description</label>
        <input id="storage-ndmp-description" name="description"
               placeholder="Managed NDMP resource">

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
               placeholder="/dev/null">

        <label for="storage-autochanger-changer-command">Changer command</label>
        <input id="storage-autochanger-changer-command" name="changer_command"
               placeholder="/usr/lib/bareos/mtx-changer %c %o %S %a %d">

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
        <input id="console-console-director" name="director"
               placeholder="bareos-dir">

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
        <input id="console-director-address" name="address"
               placeholder="localhost">

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

    function setPrefillControlValue(control, value) {
      if (Array.isArray(value)) {
        control.value = value.join('\n');
      } else if (typeof value === 'boolean') {
        control.value = value ? 'true' : 'false';
      } else if (value === null || value === undefined) {
        control.value = '';
      } else {
        control.value = String(value);
      }
    }

    function applyFormPrefill(formId, identifiers, spec) {
      const form = document.getElementById(formId);
      if (!form) { return; }

      for (const element of Array.from(form.elements)) {
        if (!(element instanceof HTMLInputElement
            || element instanceof HTMLTextAreaElement
            || element instanceof HTMLSelectElement)) {
          continue;
        }
        if (!element.name || element.type === 'submit' || element.type === 'button') {
          continue;
        }
        if (Object.hasOwn(identifiers, element.name)) { continue; }
        element.value = '';
      }

      for (const [name, value] of Object.entries(identifiers)) {
        const element = form.elements.namedItem(name);
        if (!element || element instanceof RadioNodeList) { continue; }
        setPrefillControlValue(element, value);
      }

      for (const [name, value] of Object.entries(spec ?? {})) {
        if (value === null || value === undefined) { continue; }
        const element = form.elements.namedItem(name);
        if (!element || element instanceof RadioNodeList) { continue; }
        setPrefillControlValue(element, value);
      }

      form.scrollIntoView({ behavior: 'smooth', block: 'start' });
    }

    function buildPeerPrefillTarget(
      deploymentId, component, configName, resourceType, resourceName
    ) {
      const normalizedComponent = String(component ?? '').toLowerCase();
      const normalizedType = String(resourceType ?? '').toLowerCase();
      const encodedDeployment = encodeURIComponent(String(deploymentId ?? ''));
      const encodedConfig = encodeURIComponent(String(configName ?? ''));
      const encodedResource = encodeURIComponent(String(resourceName ?? ''));

      if (normalizedComponent === 'client' && normalizedType === 'director') {
        return {
          formId: 'client-stub-form',
          identifiers: {
            deployment_id: String(deploymentId ?? ''),
            client_name: String(configName ?? ''),
            director_name: String(resourceName ?? ''),
          },
          endpoint:
            `/v1/deployments/${encodedDeployment}/clients/${encodedConfig}/directors/${encodedResource}/prefill`,
        };
      }
      if (normalizedComponent === 'director' && normalizedType === 'client') {
        return {
          formId: 'director-client-form',
          identifiers: {
            deployment_id: String(deploymentId ?? ''),
            director_name: String(configName ?? ''),
            client_name: String(resourceName ?? ''),
          },
          endpoint:
            `/v1/deployments/${encodedDeployment}/directors/${encodedConfig}/clients/${encodedResource}/prefill`,
        };
      }
      if (normalizedComponent === 'director' && normalizedType === 'storage') {
        return {
          formId: 'director-storage-form',
          identifiers: {
            deployment_id: String(deploymentId ?? ''),
            director_name: String(configName ?? ''),
            storage_name: String(resourceName ?? ''),
          },
          endpoint:
            `/v1/deployments/${encodedDeployment}/directors/${encodedConfig}/storages/${encodedResource}/prefill`,
        };
      }
      if (normalizedComponent === 'storage' && normalizedType === 'director') {
        return {
          formId: 'storage-director-form',
          identifiers: {
            deployment_id: String(deploymentId ?? ''),
            storage_name: String(configName ?? ''),
            director_name: String(resourceName ?? ''),
          },
          endpoint:
            `/v1/deployments/${encodedDeployment}/storages/${encodedConfig}/directors/${encodedResource}/prefill`,
        };
      }
      return null;
    }

    async function loadPeerPrefill(formId, identifiers, endpoint) {
      const { response, text } = await request('GET', endpoint);
      if (!response.ok) { return; }

      const payloadDocument = JSON.parse(text);
      if (payloadDocument.deployment?.id) {
        document.getElementById('deployment-inspect-id').value
          = payloadDocument.deployment.id;
      }
      applyFormPrefill(formId, identifiers, payloadDocument.spec ?? {});
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
          const prefillTarget = buildPeerPrefillTarget(
            deployment.id, config.component, config.name, resource.type, resource.name
          );
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
          const prefillButton = prefillTarget
            ? `<p class="resource-meta"><button type="button" class="prefill-button"
                 data-form-id="${escapeHtml(prefillTarget.formId)}"
                 data-endpoint="${escapeHtml(prefillTarget.endpoint)}"
                 data-identifiers="${escapeHtml(encodeURIComponent(JSON.stringify(prefillTarget.identifiers)))}">Prefill editor</button></p>`
            : '';
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
                ${prefillButton}
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

      for (const button of deploymentContentsPanel.querySelectorAll('.prefill-button')) {
        button.addEventListener('click', async () => {
          const formId = String(button.dataset.formId ?? '');
          const endpoint = String(button.dataset.endpoint ?? '');
          const identifiers = JSON.parse(
            decodeURIComponent(String(button.dataset.identifiers ?? ''))
          );
          await loadPeerPrefill(formId, identifiers, endpoint);
        });
      }
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
          tls_authenticate: String(form.get('tls_authenticate') ?? '').trim(),
          tls_enable: String(form.get('tls_enable') ?? '').trim(),
          tls_require: String(form.get('tls_require') ?? '').trim(),
          tls_verify_peer: String(form.get('tls_verify_peer') ?? '').trim(),
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
          connection_from_director_to_client: String(
            form.get('connection_from_director_to_client') ?? '').trim(),
          connection_from_client_to_director: String(
            form.get('connection_from_client_to_director') ?? '').trim(),
          monitor: String(form.get('monitor') ?? '').trim(),
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
        if (!payload.tls_authenticate) {
          delete payload.tls_authenticate;
        } else {
          payload.tls_authenticate = payload.tls_authenticate === 'true';
        }
        if (!payload.tls_enable) {
          delete payload.tls_enable;
        } else {
          payload.tls_enable = payload.tls_enable === 'true';
        }
        if (!payload.tls_require) {
          delete payload.tls_require;
        } else {
          payload.tls_require = payload.tls_require === 'true';
        }
        if (!payload.tls_verify_peer) {
          delete payload.tls_verify_peer;
        } else {
          payload.tls_verify_peer = payload.tls_verify_peer === 'true';
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
        if (!payload.connection_from_director_to_client) {
          delete payload.connection_from_director_to_client;
        } else {
          payload.connection_from_director_to_client
            = payload.connection_from_director_to_client === 'true';
        }
        if (!payload.connection_from_client_to_director) {
          delete payload.connection_from_client_to_director;
        } else {
          payload.connection_from_client_to_director
            = payload.connection_from_client_to_director === 'true';
        }
        if (!payload.monitor) {
          delete payload.monitor;
        } else {
          payload.monitor = payload.monitor === 'true';
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
        const payload = {};
        const stringFields = [
          'description',
          'mail_command',
          'operator_command',
          'timestamp_format',
        ];
        for (const field of stringFields) {
          const value = String(form.get(field) ?? '').trim();
          if (value) { payload[field] = value; }
        }
        const arrayFields = [
          'syslog_entries',
          'mail_entries',
          'mail_on_error_entries',
          'mail_on_success_entries',
          'file_entries',
          'append_entries',
          'stdout_entries',
          'stderr_entries',
          'director_entries',
          'console_entries',
          'operator_entries',
          'catalog_entries',
          'entries',
        ];
        for (const field of arrayFields) {
          const values = String(form.get(field) ?? '').split('\n')
            .map((line) => line.trim())
            .filter((line) => line.length > 0);
          if (values.length > 0) { payload[field] = values; }
        }
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
        const rawTlsAllowedCn = String(form.get('tls_allowed_cn') ?? '');
        const tlsAllowedCn = rawTlsAllowedCn.split('\n')
          .map((line) => line.trim())
          .filter((line) => line.length > 0);
        const payload = {
          address: String(form.get('address') ?? '').trim(),
          lan_address: String(form.get('lan_address') ?? '').trim(),
          port: String(form.get('port') ?? '').trim(),
          protocol: String(form.get('protocol') ?? '').trim(),
          auth_type: String(form.get('auth_type') ?? '').trim(),
          catalog: String(form.get('catalog') ?? '').trim(),
          username: String(form.get('username') ?? '').trim(),
          password: String(form.get('password') ?? '').trim(),
          enabled: String(form.get('enabled') ?? '').trim(),
          passive: String(form.get('passive') ?? '').trim(),
          strict_quotas: String(form.get('strict_quotas') ?? '').trim(),
          quota_include_failed_jobs: String(
            form.get('quota_include_failed_jobs') ?? '').trim(),
          connection_from_director_to_client: String(
            form.get('connection_from_director_to_client') ?? '').trim(),
          connection_from_client_to_director: String(
            form.get('connection_from_client_to_director') ?? '').trim(),
          soft_quota: String(form.get('soft_quota') ?? '').trim(),
          hard_quota: String(form.get('hard_quota') ?? '').trim(),
          soft_quota_grace_period: String(
            form.get('soft_quota_grace_period') ?? '').trim(),
          file_retention: String(form.get('file_retention') ?? '').trim(),
          job_retention: String(form.get('job_retention') ?? '').trim(),
          ndmp_log_level: String(form.get('ndmp_log_level') ?? '').trim(),
          ndmp_block_size: String(form.get('ndmp_block_size') ?? '').trim(),
          ndmp_use_lmdb: String(form.get('ndmp_use_lmdb') ?? '').trim(),
          auto_prune: String(form.get('auto_prune') ?? '').trim(),
          tls_authenticate: String(form.get('tls_authenticate') ?? '').trim(),
          tls_enable: String(form.get('tls_enable') ?? '').trim(),
          tls_require: String(form.get('tls_require') ?? '').trim(),
          tls_verify_peer: String(form.get('tls_verify_peer') ?? '').trim(),
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
          maximum_concurrent_jobs: String(
            form.get('maximum_concurrent_jobs') ?? '').trim(),
          maximum_bandwidth_per_job: String(
            form.get('maximum_bandwidth_per_job') ?? '').trim(),
          heartbeat_interval: String(form.get('heartbeat_interval') ?? '').trim(),
          description: String(form.get('description') ?? '').trim(),
        };
        if (!payload.address) {
          delete payload.address;
        }
        if (!payload.lan_address) {
          delete payload.lan_address;
        }
        if (!payload.port) {
          delete payload.port;
        } else {
          payload.port = Number(payload.port);
        }
        if (!payload.protocol) {
          delete payload.protocol;
        }
        if (!payload.auth_type) {
          delete payload.auth_type;
        }
        if (!payload.catalog) {
          delete payload.catalog;
        }
        if (!payload.username) {
          delete payload.username;
        }
        if (!payload.password) {
          delete payload.password;
        }
        if (!payload.enabled) {
          delete payload.enabled;
        } else {
          payload.enabled = payload.enabled === 'true';
        }
        if (!payload.passive) {
          delete payload.passive;
        } else {
          payload.passive = payload.passive === 'true';
        }
        if (!payload.strict_quotas) {
          delete payload.strict_quotas;
        } else {
          payload.strict_quotas = payload.strict_quotas === 'true';
        }
        if (!payload.quota_include_failed_jobs) {
          delete payload.quota_include_failed_jobs;
        } else {
          payload.quota_include_failed_jobs
            = payload.quota_include_failed_jobs === 'true';
        }
        if (!payload.connection_from_director_to_client) {
          delete payload.connection_from_director_to_client;
        } else {
          payload.connection_from_director_to_client
            = payload.connection_from_director_to_client === 'true';
        }
        if (!payload.connection_from_client_to_director) {
          delete payload.connection_from_client_to_director;
        } else {
          payload.connection_from_client_to_director
            = payload.connection_from_client_to_director === 'true';
        }
        if (!payload.ndmp_use_lmdb) {
          delete payload.ndmp_use_lmdb;
        } else {
          payload.ndmp_use_lmdb = payload.ndmp_use_lmdb === 'true';
        }
        if (!payload.auto_prune) {
          delete payload.auto_prune;
        } else {
          payload.auto_prune = payload.auto_prune === 'true';
        }
        if (!payload.tls_authenticate) {
          delete payload.tls_authenticate;
        } else {
          payload.tls_authenticate = payload.tls_authenticate === 'true';
        }
        if (!payload.tls_enable) {
          delete payload.tls_enable;
        } else {
          payload.tls_enable = payload.tls_enable === 'true';
        }
        if (!payload.tls_require) {
          delete payload.tls_require;
        } else {
          payload.tls_require = payload.tls_require === 'true';
        }
        if (!payload.tls_verify_peer) {
          delete payload.tls_verify_peer;
        } else {
          payload.tls_verify_peer = payload.tls_verify_peer === 'true';
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
        if (!payload.soft_quota) {
          delete payload.soft_quota;
        } else {
          payload.soft_quota = Number.parseInt(payload.soft_quota, 10);
        }
        if (!payload.hard_quota) {
          delete payload.hard_quota;
        } else {
          payload.hard_quota = Number.parseInt(payload.hard_quota, 10);
        }
        if (!payload.soft_quota_grace_period) {
          delete payload.soft_quota_grace_period;
        } else {
          payload.soft_quota_grace_period
            = Number.parseInt(payload.soft_quota_grace_period, 10);
        }
        if (!payload.file_retention) {
          delete payload.file_retention;
        } else {
          payload.file_retention = Number.parseInt(payload.file_retention, 10);
        }
        if (!payload.job_retention) {
          delete payload.job_retention;
        } else {
          payload.job_retention = Number.parseInt(payload.job_retention, 10);
        }
        if (!payload.ndmp_log_level) {
          delete payload.ndmp_log_level;
        } else {
          payload.ndmp_log_level = Number.parseInt(payload.ndmp_log_level, 10);
        }
        if (!payload.ndmp_block_size) {
          delete payload.ndmp_block_size;
        } else {
          payload.ndmp_block_size = Number.parseInt(payload.ndmp_block_size, 10);
        }
        if (!payload.maximum_concurrent_jobs) {
          delete payload.maximum_concurrent_jobs;
        } else {
          payload.maximum_concurrent_jobs
            = Number.parseInt(payload.maximum_concurrent_jobs, 10);
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
        const rawTlsAllowedCn = String(form.get('tls_allowed_cn') ?? '');
        const tlsAllowedCn = rawTlsAllowedCn
          .split(/\r?\n/)
          .map((value) => value.trim())
          .filter((value) => value.length > 0);
        const payload = {
          address: String(form.get('address') ?? '').trim(),
          lan_address: String(form.get('lan_address') ?? '').trim(),
          port: String(form.get('port') ?? '').trim(),
          protocol: String(form.get('protocol') ?? '').trim(),
          auth_type: String(form.get('auth_type') ?? '').trim(),
          username: String(form.get('username') ?? '').trim(),
          password: String(form.get('password') ?? '').trim(),
          device: String(form.get('device') ?? '').trim(),
          media_type: String(form.get('media_type') ?? '').trim(),
          autochanger: String(form.get('autochanger') ?? '').trim(),
          enabled: String(form.get('enabled') ?? '').trim(),
          allow_compression: String(
            form.get('allow_compression') ?? '').trim(),
          maximum_bandwidth_per_job: String(
            form.get('maximum_bandwidth_per_job') ?? '').trim(),
          heartbeat_interval: String(form.get('heartbeat_interval') ?? '').trim(),
          cache_status_interval: String(
            form.get('cache_status_interval') ?? '').trim(),
          maximum_concurrent_jobs: String(
            form.get('maximum_concurrent_jobs') ?? '').trim(),
          maximum_concurrent_read_jobs: String(
            form.get('maximum_concurrent_read_jobs') ?? '').trim(),
          paired_storage: String(form.get('paired_storage') ?? '').trim(),
          archive_device: String(form.get('archive_device') ?? '').trim(),
          device_type: String(form.get('device_type') ?? '').trim(),
          collect_statistics: String(
            form.get('collect_statistics') ?? '').trim(),
          ndmp_changer_device: String(
            form.get('ndmp_changer_device') ?? '').trim(),
          tls_authenticate: String(form.get('tls_authenticate') ?? '').trim(),
          tls_enable: String(form.get('tls_enable') ?? '').trim(),
          tls_require: String(form.get('tls_require') ?? '').trim(),
          tls_verify_peer: String(form.get('tls_verify_peer') ?? '').trim(),
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
          description: String(form.get('description') ?? '').trim(),
        };
        if (!payload.address) {
          delete payload.address;
        }
        if (!payload.lan_address) {
          delete payload.lan_address;
        }
        if (!payload.port) {
          delete payload.port;
        } else {
          payload.port = Number(payload.port);
        }
        if (!payload.protocol) {
          delete payload.protocol;
        }
        if (!payload.auth_type) {
          delete payload.auth_type;
        }
        if (!payload.username) {
          delete payload.username;
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
        if (!payload.autochanger) {
          delete payload.autochanger;
        } else {
          payload.autochanger = payload.autochanger === 'true';
        }
        if (!payload.enabled) {
          delete payload.enabled;
        } else {
          payload.enabled = payload.enabled === 'true';
        }
        if (!payload.allow_compression) {
          delete payload.allow_compression;
        } else {
          payload.allow_compression = payload.allow_compression === 'true';
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
        if (!payload.cache_status_interval) {
          delete payload.cache_status_interval;
        } else {
          payload.cache_status_interval
            = Number.parseInt(payload.cache_status_interval, 10);
        }
        if (!payload.maximum_concurrent_jobs) {
          delete payload.maximum_concurrent_jobs;
        } else {
          payload.maximum_concurrent_jobs
            = Number.parseInt(payload.maximum_concurrent_jobs, 10);
        }
        if (!payload.maximum_concurrent_read_jobs) {
          delete payload.maximum_concurrent_read_jobs;
        } else {
          payload.maximum_concurrent_read_jobs
            = Number.parseInt(payload.maximum_concurrent_read_jobs, 10);
        }
        if (!payload.paired_storage) {
          delete payload.paired_storage;
        }
        if (!payload.archive_device) {
          delete payload.archive_device;
        }
        if (!payload.device_type) {
          delete payload.device_type;
        }
        if (!payload.collect_statistics) {
          delete payload.collect_statistics;
        } else {
          payload.collect_statistics = payload.collect_statistics === 'true';
        }
        if (!payload.ndmp_changer_device) {
          delete payload.ndmp_changer_device;
        }
        if (!payload.tls_authenticate) {
          delete payload.tls_authenticate;
        } else {
          payload.tls_authenticate = payload.tls_authenticate === 'true';
        }
        if (!payload.tls_enable) {
          delete payload.tls_enable;
        } else {
          payload.tls_enable = payload.tls_enable === 'true';
        }
        if (!payload.tls_require) {
          delete payload.tls_require;
        } else {
          payload.tls_require = payload.tls_require === 'true';
        }
        if (!payload.tls_verify_peer) {
          delete payload.tls_verify_peer;
        } else {
          payload.tls_verify_peer = payload.tls_verify_peer === 'true';
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
        const parseLines = (name) => String(form.get(name) ?? '')
          .split(/\r?\n/)
          .map((value) => value.trim())
          .filter((value) => value.length > 0);
        const payload = {
          password: String(form.get('password') ?? '').trim(),
          description: String(form.get('description') ?? '').trim(),
          job_acl: parseLines('job_acl'),
          client_acl: parseLines('client_acl'),
          storage_acl: parseLines('storage_acl'),
          schedule_acl: parseLines('schedule_acl'),
          pool_acl: parseLines('pool_acl'),
          command_acl: parseLines('command_acl'),
          fileset_acl: parseLines('fileset_acl'),
          catalog_acl: parseLines('catalog_acl'),
          where_acl: parseLines('where_acl'),
          plugin_options_acl: parseLines('plugin_options_acl'),
          profiles: parseLines('profiles'),
          use_pam_authentication: String(
            form.get('use_pam_authentication') ?? '').trim(),
          tls_authenticate: String(form.get('tls_authenticate') ?? '').trim(),
          tls_enable: String(form.get('tls_enable') ?? '').trim(),
          tls_require: String(form.get('tls_require') ?? '').trim(),
          tls_verify_peer: String(form.get('tls_verify_peer') ?? '').trim(),
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
          tls_allowed_cn: parseLines('tls_allowed_cn'),
        };
        if (!payload.password) {
          delete payload.password;
        }
        if (!payload.description) {
          delete payload.description;
        }
        if (payload.job_acl.length === 0) { delete payload.job_acl; }
        if (payload.client_acl.length === 0) { delete payload.client_acl; }
        if (payload.storage_acl.length === 0) { delete payload.storage_acl; }
        if (payload.schedule_acl.length === 0) { delete payload.schedule_acl; }
        if (payload.pool_acl.length === 0) { delete payload.pool_acl; }
        if (payload.command_acl.length === 0) { delete payload.command_acl; }
        if (payload.fileset_acl.length === 0) { delete payload.fileset_acl; }
        if (payload.catalog_acl.length === 0) { delete payload.catalog_acl; }
        if (payload.where_acl.length === 0) { delete payload.where_acl; }
        if (payload.plugin_options_acl.length === 0) {
          delete payload.plugin_options_acl;
        }
        if (payload.profiles.length === 0) { delete payload.profiles; }
        if (!payload.use_pam_authentication) {
          delete payload.use_pam_authentication;
        } else {
          payload.use_pam_authentication = payload.use_pam_authentication === 'true';
        }
        if (!payload.tls_authenticate) {
          delete payload.tls_authenticate;
        } else {
          payload.tls_authenticate = payload.tls_authenticate === 'true';
        }
        if (!payload.tls_enable) {
          delete payload.tls_enable;
        } else {
          payload.tls_enable = payload.tls_enable === 'true';
        }
        if (!payload.tls_require) {
          delete payload.tls_require;
        } else {
          payload.tls_require = payload.tls_require === 'true';
        }
        if (!payload.tls_verify_peer) {
          delete payload.tls_verify_peer;
        } else {
          payload.tls_verify_peer = payload.tls_verify_peer === 'true';
        }
        if (!payload.tls_cipher_list) { delete payload.tls_cipher_list; }
        if (!payload.tls_cipher_suites) { delete payload.tls_cipher_suites; }
        if (!payload.tls_dh_file) { delete payload.tls_dh_file; }
        if (!payload.tls_protocol) { delete payload.tls_protocol; }
        if (!payload.tls_ca_certificate_file) {
          delete payload.tls_ca_certificate_file;
        }
        if (!payload.tls_ca_certificate_dir) {
          delete payload.tls_ca_certificate_dir;
        }
        if (!payload.tls_certificate_revocation_list) {
          delete payload.tls_certificate_revocation_list;
        }
        if (!payload.tls_certificate) { delete payload.tls_certificate; }
        if (!payload.tls_key) { delete payload.tls_key; }
        if (payload.tls_allowed_cn.length === 0) {
          delete payload.tls_allowed_cn;
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
        const parseLines = (name) => String(form.get(name) ?? '')
          .split(/\r?\n/)
          .map((value) => value.trim())
          .filter((value) => value.length > 0);
        const payload = {
          description: String(form.get('description') ?? '').trim(),
          job_acl: parseLines('job_acl'),
          client_acl: parseLines('client_acl'),
          storage_acl: parseLines('storage_acl'),
          schedule_acl: parseLines('schedule_acl'),
          pool_acl: parseLines('pool_acl'),
          command_acl: parseLines('command_acl'),
          fileset_acl: parseLines('fileset_acl'),
          catalog_acl: parseLines('catalog_acl'),
          where_acl: parseLines('where_acl'),
          plugin_options_acl: parseLines('plugin_options_acl'),
          profiles: parseLines('profiles'),
        };
        if (!payload.description) {
          delete payload.description;
        }
        if (payload.job_acl.length === 0) { delete payload.job_acl; }
        if (payload.client_acl.length === 0) { delete payload.client_acl; }
        if (payload.storage_acl.length === 0) { delete payload.storage_acl; }
        if (payload.schedule_acl.length === 0) { delete payload.schedule_acl; }
        if (payload.pool_acl.length === 0) { delete payload.pool_acl; }
        if (payload.command_acl.length === 0) { delete payload.command_acl; }
        if (payload.fileset_acl.length === 0) { delete payload.fileset_acl; }
        if (payload.catalog_acl.length === 0) { delete payload.catalog_acl; }
        if (payload.where_acl.length === 0) { delete payload.where_acl; }
        if (payload.plugin_options_acl.length === 0) {
          delete payload.plugin_options_acl;
        }
        if (payload.profiles.length === 0) { delete payload.profiles; }
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
        const parseLines = (name) => String(form.get(name) ?? '')
          .split(/\r?\n/)
          .map((value) => value.trim())
          .filter((value) => value.length > 0);
        const payload = {
          description: String(form.get('description') ?? '').trim(),
          job_acl: parseLines('job_acl'),
          client_acl: parseLines('client_acl'),
          storage_acl: parseLines('storage_acl'),
          schedule_acl: parseLines('schedule_acl'),
          pool_acl: parseLines('pool_acl'),
          command_acl: parseLines('command_acl'),
          fileset_acl: parseLines('fileset_acl'),
          catalog_acl: parseLines('catalog_acl'),
          where_acl: parseLines('where_acl'),
          plugin_options_acl: parseLines('plugin_options_acl'),
        };
        if (!payload.description) {
          delete payload.description;
        }
        if (payload.job_acl.length === 0) { delete payload.job_acl; }
        if (payload.client_acl.length === 0) { delete payload.client_acl; }
        if (payload.storage_acl.length === 0) { delete payload.storage_acl; }
        if (payload.schedule_acl.length === 0) { delete payload.schedule_acl; }
        if (payload.pool_acl.length === 0) { delete payload.pool_acl; }
        if (payload.command_acl.length === 0) { delete payload.command_acl; }
        if (payload.fileset_acl.length === 0) { delete payload.fileset_acl; }
        if (payload.catalog_acl.length === 0) { delete payload.catalog_acl; }
        if (payload.where_acl.length === 0) { delete payload.where_acl; }
        if (payload.plugin_options_acl.length === 0) {
          delete payload.plugin_options_acl;
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
        const storages = String(form.get('storages') ?? '').split('\n')
          .map((line) => line.trim())
          .filter((line) => line.length > 0);
        const payload = {
          pool_type: String(form.get('pool_type') ?? '').trim(),
          label_format: String(form.get('label_format') ?? '').trim(),
          cleaning_prefix: String(form.get('cleaning_prefix') ?? '').trim(),
          label_type: String(form.get('label_type') ?? '').trim(),
          maximum_volumes: String(form.get('maximum_volumes') ?? '').trim(),
          maximum_volume_jobs: String(
            form.get('maximum_volume_jobs') ?? '').trim(),
          maximum_volume_files: String(
            form.get('maximum_volume_files') ?? '').trim(),
          maximum_volume_bytes: String(form.get('maximum_volume_bytes') ?? '').trim(),
          volume_retention: String(form.get('volume_retention') ?? '').trim(),
          volume_use_duration: String(
            form.get('volume_use_duration') ?? '').trim(),
          migration_time: String(form.get('migration_time') ?? '').trim(),
          migration_high_bytes: String(
            form.get('migration_high_bytes') ?? '').trim(),
          migration_low_bytes: String(
            form.get('migration_low_bytes') ?? '').trim(),
          next_pool: String(form.get('next_pool') ?? '').trim(),
          storages: storages,
          use_catalog: String(form.get('use_catalog') ?? '').trim(),
          catalog_files: String(form.get('catalog_files') ?? '').trim(),
          purge_oldest_volume: String(
            form.get('purge_oldest_volume') ?? '').trim(),
          action_on_purge: String(form.get('action_on_purge') ?? '').trim(),
          recycle_oldest_volume: String(
            form.get('recycle_oldest_volume') ?? '').trim(),
          recycle_current_volume: String(
            form.get('recycle_current_volume') ?? '').trim(),
          auto_prune: String(form.get('auto_prune') ?? '').trim(),
          recycle: String(form.get('recycle') ?? '').trim(),
          recycle_pool: String(form.get('recycle_pool') ?? '').trim(),
          scratch_pool: String(form.get('scratch_pool') ?? '').trim(),
          catalog: String(form.get('catalog') ?? '').trim(),
          file_retention: String(form.get('file_retention') ?? '').trim(),
          job_retention: String(form.get('job_retention') ?? '').trim(),
          minimum_block_size: String(
            form.get('minimum_block_size') ?? '').trim(),
          maximum_block_size: String(
            form.get('maximum_block_size') ?? '').trim(),
          description: String(form.get('description') ?? '').trim(),
        };
        if (!payload.pool_type) {
          delete payload.pool_type;
        }
        if (!payload.label_format) {
          delete payload.label_format;
        }
        if (!payload.cleaning_prefix) {
          delete payload.cleaning_prefix;
        }
        if (!payload.label_type) {
          delete payload.label_type;
        }
        if (!payload.maximum_volumes) {
          delete payload.maximum_volumes;
        } else {
          payload.maximum_volumes = Number(payload.maximum_volumes);
        }
        if (!payload.maximum_volume_jobs) {
          delete payload.maximum_volume_jobs;
        } else {
          payload.maximum_volume_jobs = Number(payload.maximum_volume_jobs);
        }
        if (!payload.maximum_volume_files) {
          delete payload.maximum_volume_files;
        } else {
          payload.maximum_volume_files = Number(payload.maximum_volume_files);
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
        if (!payload.volume_use_duration) {
          delete payload.volume_use_duration;
        } else {
          payload.volume_use_duration = Number(payload.volume_use_duration);
        }
        if (!payload.migration_time) {
          delete payload.migration_time;
        } else {
          payload.migration_time = Number(payload.migration_time);
        }
        if (!payload.migration_high_bytes) {
          delete payload.migration_high_bytes;
        } else {
          payload.migration_high_bytes = Number(payload.migration_high_bytes);
        }
        if (!payload.migration_low_bytes) {
          delete payload.migration_low_bytes;
        } else {
          payload.migration_low_bytes = Number(payload.migration_low_bytes);
        }
        if (!payload.next_pool) {
          delete payload.next_pool;
        }
        if (payload.storages.length === 0) {
          delete payload.storages;
        }
        if (!payload.use_catalog) {
          delete payload.use_catalog;
        } else {
          payload.use_catalog = payload.use_catalog === 'true';
        }
        if (!payload.catalog_files) {
          delete payload.catalog_files;
        } else {
          payload.catalog_files = payload.catalog_files === 'true';
        }
        if (!payload.purge_oldest_volume) {
          delete payload.purge_oldest_volume;
        } else {
          payload.purge_oldest_volume = payload.purge_oldest_volume === 'true';
        }
        if (!payload.action_on_purge) {
          delete payload.action_on_purge;
        }
        if (!payload.recycle_oldest_volume) {
          delete payload.recycle_oldest_volume;
        } else {
          payload.recycle_oldest_volume
            = payload.recycle_oldest_volume === 'true';
        }
        if (!payload.recycle_current_volume) {
          delete payload.recycle_current_volume;
        } else {
          payload.recycle_current_volume
            = payload.recycle_current_volume === 'true';
        }
        if (!payload.auto_prune) {
          delete payload.auto_prune;
        } else {
          payload.auto_prune = payload.auto_prune === 'true';
        }
        if (!payload.recycle) {
          delete payload.recycle;
        } else {
          payload.recycle = payload.recycle === 'true';
        }
        if (!payload.recycle_pool) {
          delete payload.recycle_pool;
        }
        if (!payload.scratch_pool) {
          delete payload.scratch_pool;
        }
        if (!payload.catalog) {
          delete payload.catalog;
        }
        if (!payload.file_retention) {
          delete payload.file_retention;
        } else {
          payload.file_retention = Number(payload.file_retention);
        }
        if (!payload.job_retention) {
          delete payload.job_retention;
        } else {
          payload.job_retention = Number(payload.job_retention);
        }
        if (!payload.minimum_block_size) {
          delete payload.minimum_block_size;
        } else {
          payload.minimum_block_size = Number(payload.minimum_block_size);
        }
        if (!payload.maximum_block_size) {
          delete payload.maximum_block_size;
        } else {
          payload.maximum_block_size = Number(payload.maximum_block_size);
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
          multiple_connections: String(
            form.get('multiple_connections') ?? '').trim(),
          disable_batch_insert: String(
            form.get('disable_batch_insert') ?? '').trim(),
          reconnect: String(form.get('reconnect') ?? '').trim(),
          exit_on_fatal: String(form.get('exit_on_fatal') ?? '').trim(),
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
        if (!payload.multiple_connections) {
          delete payload.multiple_connections;
        } else {
          payload.multiple_connections = payload.multiple_connections === 'true';
        }
        if (!payload.disable_batch_insert) {
          delete payload.disable_batch_insert;
        } else {
          payload.disable_batch_insert = payload.disable_batch_insert === 'true';
        }
        if (!payload.reconnect) {
          delete payload.reconnect;
        } else {
          payload.reconnect = payload.reconnect === 'true';
        }
        if (!payload.exit_on_fatal) {
          delete payload.exit_on_fatal;
        } else {
          payload.exit_on_fatal = payload.exit_on_fatal === 'true';
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
        const payload = {};
        const stringFields = [
          'description',
          'mail_command',
          'operator_command',
          'timestamp_format',
        ];
        for (const field of stringFields) {
          const value = String(form.get(field) ?? '').trim();
          if (value) { payload[field] = value; }
        }
        const arrayFields = [
          'syslog_entries',
          'mail_entries',
          'mail_on_error_entries',
          'mail_on_success_entries',
          'file_entries',
          'append_entries',
          'stdout_entries',
          'stderr_entries',
          'director_entries',
          'console_entries',
          'operator_entries',
          'catalog_entries',
          'entries',
        ];
        for (const field of arrayFields) {
          const values = String(form.get(field) ?? '').split('\n')
            .map((line) => line.trim())
            .filter((line) => line.length > 0);
          if (values.length > 0) { payload[field] = values; }
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
          enabled: String(form.get('enabled') ?? '').trim(),
          run_entries: runEntries,
        };
        if (!payload.description) {
          delete payload.description;
        }
        if (!payload.enabled) {
          delete payload.enabled;
        } else {
          payload.enabled = payload.enabled === 'true';
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
          ignore_fileset_changes: String(
            form.get('ignore_fileset_changes') ?? '').trim(),
          enable_vss: String(form.get('enable_vss') ?? '').trim(),
          include_blocks: includeText ? [includeText] : [],
          exclude_blocks: excludeText ? [excludeText] : [],
        };
        if (!payload.description) {
          delete payload.description;
        }
        if (!payload.ignore_fileset_changes) {
          delete payload.ignore_fileset_changes;
        } else {
          payload.ignore_fileset_changes
            = payload.ignore_fileset_changes === 'true';
        }
        if (!payload.enable_vss) {
          delete payload.enable_vss;
        } else {
          payload.enable_vss = payload.enable_vss === 'true';
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
    const buildDirectorJobPayload = (form) => {
      const payload = {};
      const stringFields = [
        'description',
        'type',
        'backup_format',
        'protocol',
        'level',
        'messages',
        'pool',
        'full_backup_pool',
        'virtual_full_backup_pool',
        'incremental_backup_pool',
        'differential_backup_pool',
        'next_pool',
        'client',
        'fileset',
        'schedule',
        'verify_job',
        'catalog',
        'jobdefs',
        'where',
        'replace',
        'regex_where',
        'strip_prefix',
        'add_prefix',
        'add_suffix',
        'bootstrap',
        'write_bootstrap',
        'write_verify_list',
        'selection_type',
        'selection_pattern',
      ];
      for (const field of stringFields) {
        const value = String(form.get(field) ?? '').trim();
        if (value) { payload[field] = value; }
      }
      const integerFields = [
        'maximum_bandwidth',
        'max_run_sched_time',
        'max_run_time',
        'full_max_runtime',
        'incremental_max_runtime',
        'differential_max_runtime',
        'max_wait_time',
        'max_start_delay',
        'max_full_interval',
        'max_virtual_full_interval',
        'max_diff_interval',
        'spool_size',
        'maximum_concurrent_jobs',
        'reschedule_interval',
        'reschedule_times',
        'priority',
        'file_history_size',
        'max_concurrent_copies',
        'always_incremental_job_retention',
        'always_incremental_keep_number',
        'always_incremental_max_full_age',
        'max_full_consolidations',
        'run_on_incoming_connect_interval',
      ];
      for (const field of integerFields) {
        const value = String(form.get(field) ?? '').trim();
        if (value) { payload[field] = Number.parseInt(value, 10); }
      }
      const booleanFields = [
        'enabled',
        'prefix_links',
        'prune_jobs',
        'prune_files',
        'prune_volumes',
        'purge_migration_job',
        'spool_attributes',
        'spool_data',
        'rerun_failed_levels',
        'prefer_mounted_volumes',
        'reschedule_on_error',
        'allow_mixed_priority',
        'accurate',
        'allow_duplicate_jobs',
        'allow_higher_duplicates',
        'cancel_lower_level_duplicates',
        'cancel_queued_duplicates',
        'cancel_running_duplicates',
        'save_file_history',
        'always_incremental',
      ];
      for (const field of booleanFields) {
        const value = String(form.get(field) ?? '').trim();
        if (value) { payload[field] = value === 'true'; }
      }
      const arrayFields = [
        'storages',
        'run_entries',
        'run_before_job_entries',
        'run_after_job_entries',
        'run_after_failed_job_entries',
        'client_run_before_job_entries',
        'client_run_after_job_entries',
        'fd_plugin_options',
        'sd_plugin_options',
        'dir_plugin_options',
      ];
      for (const field of arrayFields) {
        const values = String(form.get(field) ?? '').split('\n')
          .map((line) => line.trim())
          .filter((line) => line.length > 0);
        if (values.length > 0) { payload[field] = values; }
      }
      const runscriptBlocks = String(form.get('runscript_blocks') ?? '').trim();
      if (runscriptBlocks) { payload.runscript_blocks = [runscriptBlocks]; }
      return payload;
    };
    document.getElementById('director-job-form').addEventListener(
      'submit',
      async (event) => {
        event.preventDefault();
        const form = new FormData(event.target);
        const deploymentId = String(form.get('deployment_id') ?? '').trim();
        const directorName = String(form.get('director_name') ?? '').trim();
        const jobName = String(form.get('job_name') ?? '').trim();
        const payload = buildDirectorJobPayload(form);
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
        const payload = buildDirectorJobPayload(form);
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
        const rawAuditEvents = String(form.get('audit_events') ?? '');
        const auditEvents = rawAuditEvents.split('\n')
          .map((line) => line.trim())
          .filter((line) => line.length > 0);
        const rawPluginNames = String(form.get('plugin_names') ?? '');
        const pluginNames = rawPluginNames.split('\n')
          .map((line) => line.trim())
          .filter((line) => line.length > 0);
        const payload = {
          address: String(form.get('address') ?? '').trim(),
          addresses: addresses,
          source_address: String(form.get('source_address') ?? '').trim(),
          source_addresses: sourceAddresses,
          port: String(form.get('port') ?? '').trim(),
          query_file: String(form.get('query_file') ?? '').trim(),
          subscriptions: String(form.get('subscriptions') ?? '').trim(),
          maximum_concurrent_jobs: String(form.get('maximum_concurrent_jobs') ?? '').trim(),
          maximum_console_connections: String(form.get('maximum_console_connections') ?? '').trim(),
          password: String(form.get('password') ?? '').trim(),
          absolute_job_timeout: String(form.get('absolute_job_timeout') ?? '').trim(),
          tls_authenticate: String(form.get('tls_authenticate') ?? '').trim(),
          tls_enable: String(form.get('tls_enable') ?? '').trim(),
          tls_require: String(form.get('tls_require') ?? '').trim(),
          tls_verify_peer: String(form.get('tls_verify_peer') ?? '').trim(),
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
          ver_id: String(form.get('ver_id') ?? '').trim(),
          log_timestamp_format: String(form.get('log_timestamp_format') ?? '').trim(),
          secure_erase_command: String(form.get('secure_erase_command') ?? '').trim(),
          enable_ktls: String(form.get('enable_ktls') ?? '').trim(),
          fd_connect_timeout: String(form.get('fd_connect_timeout') ?? '').trim(),
          sd_connect_timeout: String(form.get('sd_connect_timeout') ?? '').trim(),
          heartbeat_interval: String(form.get('heartbeat_interval') ?? '').trim(),
          statistics_retention: String(form.get('statistics_retention') ?? '').trim(),
          statistics_collect_interval: String(
            form.get('statistics_collect_interval') ?? '').trim(),
          description: String(form.get('description') ?? '').trim(),
          key_encryption_key: String(form.get('key_encryption_key') ?? '').trim(),
          ndmp_snooping: String(form.get('ndmp_snooping') ?? '').trim(),
          ndmp_log_level: String(form.get('ndmp_log_level') ?? '').trim(),
          ndmp_namelist_fhinfo_set_zero_for_invalid_uquad: String(
            form.get('ndmp_namelist_fhinfo_set_zero_for_invalid_uquad')
              ?? '').trim(),
          auditing: String(form.get('auditing') ?? '').trim(),
          audit_events: auditEvents,
          working_directory: String(form.get('working_directory') ?? '').trim(),
          plugin_directory: String(form.get('plugin_directory') ?? '').trim(),
          plugin_names: pluginNames,
          scripts_directory: String(form.get('scripts_directory') ?? '').trim(),
          messages: String(form.get('messages') ?? '').trim(),
        };
        if (!payload.address) { delete payload.address; }
        if (payload.addresses.length === 0) { delete payload.addresses; }
        if (!payload.source_address) { delete payload.source_address; }
        if (payload.source_addresses.length === 0) {
          delete payload.source_addresses;
        }
        if (!payload.port) {
          delete payload.port;
        } else {
          payload.port = Number(payload.port);
        }
        if (!payload.query_file) { delete payload.query_file; }
        if (!payload.subscriptions) {
          delete payload.subscriptions;
        } else {
          payload.subscriptions = Number(payload.subscriptions);
        }
        if (!payload.maximum_concurrent_jobs) {
          delete payload.maximum_concurrent_jobs;
        } else {
          payload.maximum_concurrent_jobs = Number(payload.maximum_concurrent_jobs);
        }
        if (!payload.maximum_console_connections) {
          delete payload.maximum_console_connections;
        } else {
          payload.maximum_console_connections
            = Number(payload.maximum_console_connections);
        }
        if (!payload.password) { delete payload.password; }
        if (!payload.absolute_job_timeout) {
          delete payload.absolute_job_timeout;
        } else {
          payload.absolute_job_timeout = Number(payload.absolute_job_timeout);
        }
        if (!payload.tls_authenticate) {
          delete payload.tls_authenticate;
        } else {
          payload.tls_authenticate = payload.tls_authenticate === 'true';
        }
        if (!payload.tls_enable) {
          delete payload.tls_enable;
        } else {
          payload.tls_enable = payload.tls_enable === 'true';
        }
        if (!payload.tls_require) {
          delete payload.tls_require;
        } else {
          payload.tls_require = payload.tls_require === 'true';
        }
        if (!payload.tls_verify_peer) {
          delete payload.tls_verify_peer;
        } else {
          payload.tls_verify_peer = payload.tls_verify_peer === 'true';
        }
        if (!payload.tls_cipher_list) { delete payload.tls_cipher_list; }
        if (!payload.tls_cipher_suites) { delete payload.tls_cipher_suites; }
        if (!payload.tls_dh_file) { delete payload.tls_dh_file; }
        if (!payload.tls_protocol) { delete payload.tls_protocol; }
        if (!payload.tls_ca_certificate_file) {
          delete payload.tls_ca_certificate_file;
        }
        if (!payload.tls_ca_certificate_dir) {
          delete payload.tls_ca_certificate_dir;
        }
        if (!payload.tls_certificate_revocation_list) {
          delete payload.tls_certificate_revocation_list;
        }
        if (!payload.tls_certificate) { delete payload.tls_certificate; }
        if (!payload.tls_key) { delete payload.tls_key; }
        if (payload.tls_allowed_cn.length === 0) { delete payload.tls_allowed_cn; }
        if (!payload.ver_id) { delete payload.ver_id; }
        if (!payload.log_timestamp_format) { delete payload.log_timestamp_format; }
        if (!payload.secure_erase_command) { delete payload.secure_erase_command; }
        if (!payload.enable_ktls) {
          delete payload.enable_ktls;
        } else {
          payload.enable_ktls = payload.enable_ktls === 'true';
        }
        if (!payload.fd_connect_timeout) {
          delete payload.fd_connect_timeout;
        } else {
          payload.fd_connect_timeout = Number(payload.fd_connect_timeout);
        }
        if (!payload.sd_connect_timeout) {
          delete payload.sd_connect_timeout;
        } else {
          payload.sd_connect_timeout = Number(payload.sd_connect_timeout);
        }
        if (!payload.heartbeat_interval) {
          delete payload.heartbeat_interval;
        } else {
          payload.heartbeat_interval = Number(payload.heartbeat_interval);
        }
        if (!payload.statistics_retention) {
          delete payload.statistics_retention;
        } else {
          payload.statistics_retention = Number(payload.statistics_retention);
        }
        if (!payload.statistics_collect_interval) {
          delete payload.statistics_collect_interval;
        } else {
          payload.statistics_collect_interval
            = Number(payload.statistics_collect_interval);
        }
        if (!payload.description) { delete payload.description; }
        if (!payload.key_encryption_key) { delete payload.key_encryption_key; }
        if (!payload.ndmp_snooping) {
          delete payload.ndmp_snooping;
        } else {
          payload.ndmp_snooping = payload.ndmp_snooping === 'true';
        }
        if (!payload.ndmp_log_level) {
          delete payload.ndmp_log_level;
        } else {
          payload.ndmp_log_level = Number(payload.ndmp_log_level);
        }
        if (!payload.ndmp_namelist_fhinfo_set_zero_for_invalid_uquad) {
          delete payload.ndmp_namelist_fhinfo_set_zero_for_invalid_uquad;
        } else {
          payload.ndmp_namelist_fhinfo_set_zero_for_invalid_uquad
            = payload.ndmp_namelist_fhinfo_set_zero_for_invalid_uquad === 'true';
        }
        if (!payload.auditing) {
          delete payload.auditing;
        } else {
          payload.auditing = payload.auditing === 'true';
        }
        if (payload.audit_events.length === 0) { delete payload.audit_events; }
        if (!payload.working_directory) { delete payload.working_directory; }
        if (!payload.plugin_directory) { delete payload.plugin_directory; }
        if (payload.plugin_names.length === 0) { delete payload.plugin_names; }
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
        const rawTlsAllowedCn = String(form.get('tls_allowed_cn') ?? '');
        const tlsAllowedCn = rawTlsAllowedCn
          .split(/\r?\n/)
          .map((value) => value.trim())
          .filter((value) => value.length > 0);
        const payload = {
          password: String(form.get('password') ?? '').trim(),
          description: String(form.get('description') ?? '').trim(),
          monitor: String(form.get('monitor') ?? '').trim(),
          maximum_bandwidth_per_job: String(
            form.get('maximum_bandwidth_per_job') ?? '').trim(),
          key_encryption_key: String(
            form.get('key_encryption_key') ?? '').trim(),
          tls_authenticate: String(form.get('tls_authenticate') ?? '').trim(),
          tls_enable: String(form.get('tls_enable') ?? '').trim(),
          tls_require: String(form.get('tls_require') ?? '').trim(),
          tls_verify_peer: String(form.get('tls_verify_peer') ?? '').trim(),
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
        };
        if (!payload.password) { delete payload.password; }
        if (!payload.description) { delete payload.description; }
        if (!payload.monitor) {
          delete payload.monitor;
        } else {
          payload.monitor = payload.monitor === 'true';
        }
        if (!payload.maximum_bandwidth_per_job) {
          delete payload.maximum_bandwidth_per_job;
        } else {
          payload.maximum_bandwidth_per_job
            = Number.parseInt(payload.maximum_bandwidth_per_job, 10);
        }
        if (!payload.key_encryption_key) { delete payload.key_encryption_key; }
        if (!payload.tls_authenticate) {
          delete payload.tls_authenticate;
        } else {
          payload.tls_authenticate = payload.tls_authenticate === 'true';
        }
        if (!payload.tls_enable) {
          delete payload.tls_enable;
        } else {
          payload.tls_enable = payload.tls_enable === 'true';
        }
        if (!payload.tls_require) {
          delete payload.tls_require;
        } else {
          payload.tls_require = payload.tls_require === 'true';
        }
        if (!payload.tls_verify_peer) {
          delete payload.tls_verify_peer;
        } else {
          payload.tls_verify_peer = payload.tls_verify_peer === 'true';
        }
        if (!payload.tls_cipher_list) { delete payload.tls_cipher_list; }
        if (!payload.tls_cipher_suites) { delete payload.tls_cipher_suites; }
        if (!payload.tls_dh_file) { delete payload.tls_dh_file; }
        if (!payload.tls_protocol) { delete payload.tls_protocol; }
        if (!payload.tls_ca_certificate_file) {
          delete payload.tls_ca_certificate_file;
        }
        if (!payload.tls_ca_certificate_dir) {
          delete payload.tls_ca_certificate_dir;
        }
        if (!payload.tls_certificate_revocation_list) {
          delete payload.tls_certificate_revocation_list;
        }
        if (!payload.tls_certificate) { delete payload.tls_certificate; }
        if (!payload.tls_key) { delete payload.tls_key; }
        if (payload.tls_allowed_cn.length === 0) {
          delete payload.tls_allowed_cn;
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
        const payload = {};
        const stringFields = [
          'media_type',
          'archive_device',
          'device_type',
          'access_mode',
          'device_options',
          'diagnostic_device',
          'changer_device',
          'changer_command',
          'alert_command',
          'spool_directory',
          'mount_point',
          'mount_command',
          'unmount_command',
          'label_type',
          'auto_deflate',
          'auto_deflate_algorithm',
          'auto_inflate',
          'description',
        ];
        for (const field of stringFields) {
          const value = String(form.get(field) ?? '').trim();
          if (value) { payload[field] = value; }
        }

        const booleanFields = [
          'hardware_end_of_file',
          'hardware_end_of_medium',
          'backward_space_record',
          'backward_space_file',
          'bsf_at_eom',
          'two_eof',
          'forward_space_record',
          'forward_space_file',
          'fast_forward_space_file',
          'removable_media',
          'random_access',
          'automatic_mount',
          'label_media',
          'always_open',
          'autochanger',
          'close_on_poll',
          'block_positioning',
          'use_mtiocget',
          'check_labels',
          'requires_mount',
          'offline_on_unmount',
          'block_checksum',
          'auto_select',
          'no_rewind_on_close',
          'drive_tape_alert_enabled',
          'drive_crypto_enabled',
          'query_crypto_status',
          'collect_statistics',
          'eof_on_error_is_eot',
        ];
        for (const field of booleanFields) {
          const value = String(form.get(field) ?? '').trim();
          if (value) { payload[field] = value === 'true'; }
        }

        const integerFields = [
          'maximum_changer_wait',
          'maximum_open_wait',
          'maximum_open_volumes',
          'maximum_network_buffer_size',
          'volume_poll_interval',
          'maximum_rewind_wait',
          'label_block_size',
          'minimum_block_size',
          'maximum_block_size',
          'maximum_file_size',
          'volume_capacity',
          'maximum_concurrent_jobs',
          'maximum_spool_size',
          'maximum_job_spool_size',
          'drive_index',
          'auto_deflate_level',
          'count',
        ];
        for (const field of integerFields) {
          const value = String(form.get(field) ?? '').trim();
          if (value) { payload[field] = Number.parseInt(value, 10); }
        }
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
        const payload = {};
        const stringFields = [
          'description',
          'mail_command',
          'operator_command',
          'timestamp_format',
        ];
        for (const field of stringFields) {
          const value = String(form.get(field) ?? '').trim();
          if (value) { payload[field] = value; }
        }
        const arrayFields = [
          'syslog_entries',
          'mail_entries',
          'mail_on_error_entries',
          'mail_on_success_entries',
          'file_entries',
          'append_entries',
          'stdout_entries',
          'stderr_entries',
          'director_entries',
          'console_entries',
          'operator_entries',
          'catalog_entries',
          'entries',
        ];
        for (const field of arrayFields) {
          const values = String(form.get(field) ?? '').split('\n')
            .map((line) => line.trim())
            .filter((line) => line.length > 0);
          if (values.length > 0) { payload[field] = values; }
        }
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
          description: String(form.get('description') ?? '').trim(),
          username: String(form.get('username') ?? '').trim(),
          password: String(form.get('password') ?? '').trim(),
          auth_type: String(form.get('auth_type') ?? '').trim(),
        };
        if (!payload.description) { delete payload.description; }
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

void SetOptionalString(json_t* object,
                       const char* key,
                       const std::optional<std::string>& value)
{
  if (value) {
    json_object_set_new(object, key, json_string(value->c_str()));
  } else {
    json_object_set_new(object, key, json_null());
  }
}

void SetOptionalBool(json_t* object,
                     const char* key,
                     const std::optional<bool>& value)
{
  if (value) {
    json_object_set_new(object, key, json_boolean(*value));
  } else {
    json_object_set_new(object, key, json_null());
  }
}

template <typename Integer>
void SetOptionalInteger(json_t* object,
                        const char* key,
                        const std::optional<Integer>& value)
{
  if (value) {
    json_object_set_new(object, key,
                        json_integer(static_cast<json_int_t>(*value)));
  } else {
    json_object_set_new(object, key, json_null());
  }
}

void SetOptionalStringArray(
    json_t* object,
    const char* key,
    const std::optional<std::vector<std::string>>& value)
{
  if (!value) {
    json_object_set_new(object, key, json_null());
    return;
  }

  auto array = MakeJson(json_array());
  for (const auto& entry : *value) {
    json_array_append_new(array.get(), json_string(entry.c_str()));
  }
  json_object_set_new(object, key, array.release());
}

json_t* ClientDirectorStubSpecToJson(const ClientDirectorStubSpec& spec)
{
  auto object = MakeJson(json_object());
  SetOptionalString(object.get(), "description", spec.description);
  SetOptionalString(object.get(), "password", spec.password);
  SetOptionalString(object.get(), "address", spec.address);
  SetOptionalInteger(object.get(), "port", spec.port);
  SetOptionalStringArray(object.get(), "allowed_script_dirs",
                         spec.allowed_script_dirs);
  SetOptionalStringArray(object.get(), "allowed_job_commands",
                         spec.allowed_job_commands);
  SetOptionalBool(object.get(), "tls_authenticate", spec.tls_authenticate);
  SetOptionalBool(object.get(), "tls_enable", spec.tls_enable);
  SetOptionalBool(object.get(), "tls_require", spec.tls_require);
  SetOptionalBool(object.get(), "tls_verify_peer", spec.tls_verify_peer);
  SetOptionalString(object.get(), "tls_cipher_list", spec.tls_cipher_list);
  SetOptionalString(object.get(), "tls_cipher_suites", spec.tls_cipher_suites);
  SetOptionalString(object.get(), "tls_dh_file", spec.tls_dh_file);
  SetOptionalString(object.get(), "tls_protocol", spec.tls_protocol);
  SetOptionalString(object.get(), "tls_ca_certificate_file",
                    spec.tls_ca_certificate_file);
  SetOptionalString(object.get(), "tls_ca_certificate_dir",
                    spec.tls_ca_certificate_dir);
  SetOptionalString(object.get(), "tls_certificate_revocation_list",
                    spec.tls_certificate_revocation_list);
  SetOptionalString(object.get(), "tls_certificate", spec.tls_certificate);
  SetOptionalString(object.get(), "tls_key", spec.tls_key);
  SetOptionalStringArray(object.get(), "tls_allowed_cn", spec.tls_allowed_cn);
  SetOptionalBool(object.get(), "connection_from_director_to_client",
                  spec.connection_from_director_to_client);
  SetOptionalBool(object.get(), "connection_from_client_to_director",
                  spec.connection_from_client_to_director);
  SetOptionalBool(object.get(), "monitor", spec.monitor);
  SetOptionalInteger(object.get(), "maximum_bandwidth_per_job",
                     spec.maximum_bandwidth_per_job);
  return object.release();
}

json_t* DirectorClientResourceSpecToJson(const DirectorClientResourceSpec& spec)
{
  auto object = MakeJson(json_object());
  SetOptionalString(object.get(), "address", spec.address);
  SetOptionalString(object.get(), "lan_address", spec.lan_address);
  SetOptionalInteger(object.get(), "port", spec.port);
  SetOptionalString(object.get(), "protocol", spec.protocol);
  SetOptionalString(object.get(), "auth_type", spec.auth_type);
  SetOptionalString(object.get(), "catalog", spec.catalog);
  SetOptionalString(object.get(), "username", spec.username);
  SetOptionalString(object.get(), "password", spec.password);
  SetOptionalBool(object.get(), "enabled", spec.enabled);
  SetOptionalBool(object.get(), "passive", spec.passive);
  SetOptionalBool(object.get(), "strict_quotas", spec.strict_quotas);
  SetOptionalBool(object.get(), "quota_include_failed_jobs",
                  spec.quota_include_failed_jobs);
  SetOptionalInteger(object.get(), "soft_quota", spec.soft_quota);
  SetOptionalInteger(object.get(), "hard_quota", spec.hard_quota);
  SetOptionalInteger(object.get(), "soft_quota_grace_period",
                     spec.soft_quota_grace_period);
  SetOptionalInteger(object.get(), "file_retention", spec.file_retention);
  SetOptionalInteger(object.get(), "job_retention", spec.job_retention);
  SetOptionalInteger(object.get(), "ndmp_log_level", spec.ndmp_log_level);
  SetOptionalInteger(object.get(), "ndmp_block_size", spec.ndmp_block_size);
  SetOptionalBool(object.get(), "ndmp_use_lmdb", spec.ndmp_use_lmdb);
  SetOptionalBool(object.get(), "auto_prune", spec.auto_prune);
  SetOptionalBool(object.get(), "tls_authenticate", spec.tls_authenticate);
  SetOptionalBool(object.get(), "tls_enable", spec.tls_enable);
  SetOptionalBool(object.get(), "tls_require", spec.tls_require);
  SetOptionalBool(object.get(), "tls_verify_peer", spec.tls_verify_peer);
  SetOptionalString(object.get(), "tls_cipher_list", spec.tls_cipher_list);
  SetOptionalString(object.get(), "tls_cipher_suites", spec.tls_cipher_suites);
  SetOptionalString(object.get(), "tls_dh_file", spec.tls_dh_file);
  SetOptionalString(object.get(), "tls_protocol", spec.tls_protocol);
  SetOptionalString(object.get(), "tls_ca_certificate_file",
                    spec.tls_ca_certificate_file);
  SetOptionalString(object.get(), "tls_ca_certificate_dir",
                    spec.tls_ca_certificate_dir);
  SetOptionalString(object.get(), "tls_certificate_revocation_list",
                    spec.tls_certificate_revocation_list);
  SetOptionalString(object.get(), "tls_certificate", spec.tls_certificate);
  SetOptionalString(object.get(), "tls_key", spec.tls_key);
  SetOptionalStringArray(object.get(), "tls_allowed_cn", spec.tls_allowed_cn);
  SetOptionalBool(object.get(), "connection_from_director_to_client",
                  spec.connection_from_director_to_client);
  SetOptionalBool(object.get(), "connection_from_client_to_director",
                  spec.connection_from_client_to_director);
  SetOptionalInteger(object.get(), "maximum_concurrent_jobs",
                     spec.maximum_concurrent_jobs);
  SetOptionalInteger(object.get(), "heartbeat_interval",
                     spec.heartbeat_interval);
  SetOptionalInteger(object.get(), "maximum_bandwidth_per_job",
                     spec.maximum_bandwidth_per_job);
  SetOptionalString(object.get(), "description", spec.description);
  return object.release();
}

json_t* DirectorStorageResourceSpecToJson(
    const DirectorStorageResourceSpec& spec)
{
  auto object = MakeJson(json_object());
  SetOptionalString(object.get(), "address", spec.address);
  SetOptionalString(object.get(), "lan_address", spec.lan_address);
  SetOptionalInteger(object.get(), "port", spec.port);
  SetOptionalString(object.get(), "protocol", spec.protocol);
  SetOptionalString(object.get(), "auth_type", spec.auth_type);
  SetOptionalString(object.get(), "username", spec.username);
  SetOptionalString(object.get(), "password", spec.password);
  SetOptionalString(object.get(), "device", spec.device);
  SetOptionalString(object.get(), "media_type", spec.media_type);
  SetOptionalBool(object.get(), "autochanger", spec.autochanger);
  SetOptionalBool(object.get(), "enabled", spec.enabled);
  SetOptionalBool(object.get(), "allow_compression", spec.allow_compression);
  SetOptionalInteger(object.get(), "heartbeat_interval",
                     spec.heartbeat_interval);
  SetOptionalInteger(object.get(), "cache_status_interval",
                     spec.cache_status_interval);
  SetOptionalInteger(object.get(), "maximum_concurrent_jobs",
                     spec.maximum_concurrent_jobs);
  SetOptionalInteger(object.get(), "maximum_concurrent_read_jobs",
                     spec.maximum_concurrent_read_jobs);
  SetOptionalString(object.get(), "paired_storage", spec.paired_storage);
  SetOptionalInteger(object.get(), "maximum_bandwidth_per_job",
                     spec.maximum_bandwidth_per_job);
  SetOptionalBool(object.get(), "collect_statistics", spec.collect_statistics);
  SetOptionalString(object.get(), "ndmp_changer_device",
                    spec.ndmp_changer_device);
  SetOptionalBool(object.get(), "tls_authenticate", spec.tls_authenticate);
  SetOptionalBool(object.get(), "tls_enable", spec.tls_enable);
  SetOptionalBool(object.get(), "tls_require", spec.tls_require);
  SetOptionalBool(object.get(), "tls_verify_peer", spec.tls_verify_peer);
  SetOptionalString(object.get(), "tls_cipher_list", spec.tls_cipher_list);
  SetOptionalString(object.get(), "tls_cipher_suites", spec.tls_cipher_suites);
  SetOptionalString(object.get(), "tls_dh_file", spec.tls_dh_file);
  SetOptionalString(object.get(), "tls_protocol", spec.tls_protocol);
  SetOptionalString(object.get(), "tls_ca_certificate_file",
                    spec.tls_ca_certificate_file);
  SetOptionalString(object.get(), "tls_ca_certificate_dir",
                    spec.tls_ca_certificate_dir);
  SetOptionalString(object.get(), "tls_certificate_revocation_list",
                    spec.tls_certificate_revocation_list);
  SetOptionalString(object.get(), "tls_certificate", spec.tls_certificate);
  SetOptionalString(object.get(), "tls_key", spec.tls_key);
  SetOptionalStringArray(object.get(), "tls_allowed_cn", spec.tls_allowed_cn);
  SetOptionalString(object.get(), "archive_device", spec.archive_device);
  SetOptionalString(object.get(), "device_type", spec.device_type);
  SetOptionalString(object.get(), "description", spec.description);
  return object.release();
}

json_t* StorageDirectorResourceSpecToJson(
    const StorageDirectorResourceSpec& spec)
{
  auto object = MakeJson(json_object());
  SetOptionalString(object.get(), "password", spec.password);
  SetOptionalString(object.get(), "description", spec.description);
  SetOptionalBool(object.get(), "monitor", spec.monitor);
  SetOptionalInteger(object.get(), "maximum_bandwidth_per_job",
                     spec.maximum_bandwidth_per_job);
  SetOptionalString(object.get(), "key_encryption_key",
                    spec.key_encryption_key);
  SetOptionalBool(object.get(), "tls_authenticate", spec.tls_authenticate);
  SetOptionalBool(object.get(), "tls_enable", spec.tls_enable);
  SetOptionalBool(object.get(), "tls_require", spec.tls_require);
  SetOptionalBool(object.get(), "tls_verify_peer", spec.tls_verify_peer);
  SetOptionalString(object.get(), "tls_cipher_list", spec.tls_cipher_list);
  SetOptionalString(object.get(), "tls_cipher_suites", spec.tls_cipher_suites);
  SetOptionalString(object.get(), "tls_dh_file", spec.tls_dh_file);
  SetOptionalString(object.get(), "tls_protocol", spec.tls_protocol);
  SetOptionalString(object.get(), "tls_ca_certificate_file",
                    spec.tls_ca_certificate_file);
  SetOptionalString(object.get(), "tls_ca_certificate_dir",
                    spec.tls_ca_certificate_dir);
  SetOptionalString(object.get(), "tls_certificate_revocation_list",
                    spec.tls_certificate_revocation_list);
  SetOptionalString(object.get(), "tls_certificate", spec.tls_certificate);
  SetOptionalString(object.get(), "tls_key", spec.tls_key);
  SetOptionalStringArray(object.get(), "tls_allowed_cn", spec.tls_allowed_cn);
  return object.release();
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

http::response<http::string_body> HandleClientDirectorStubPrefillRequest(
    ServiceState& state,
    std::string_view deployment_id,
    std::string_view client_name,
    std::string_view director_name)
{
  auto deployment = state.GetDeployment(deployment_id);
  if (!deployment) {
    return ErrorResponse(http::status::not_found, "deployment not found.");
  }

  auto spec = state.GetClientDirectorStubSpec(deployment_id, client_name,
                                              director_name);
  if (!spec) { return ErrorResponse(http::status::bad_request, spec.error); }

  auto root = MakeJson(json_object());
  auto deployment_json = MakeJson(json_array());
  AppendDeployment(deployment_json.get(), *deployment);
  json_object_set(root.get(), "deployment",
                  json_array_get(deployment_json.get(), 0));
  json_object_set_new(root.get(), "spec",
                      ClientDirectorStubSpecToJson(*spec.value));
  return JsonResponse(http::status::ok, DumpJson(root.get()));
}

http::response<http::string_body> HandleDirectorClientPrefillRequest(
    ServiceState& state,
    std::string_view deployment_id,
    std::string_view director_name,
    std::string_view client_name)
{
  auto deployment = state.GetDeployment(deployment_id);
  if (!deployment) {
    return ErrorResponse(http::status::not_found, "deployment not found.");
  }

  auto spec = state.GetDirectorClientResourceSpec(deployment_id, director_name,
                                                  client_name);
  if (!spec) { return ErrorResponse(http::status::bad_request, spec.error); }

  auto root = MakeJson(json_object());
  auto deployment_json = MakeJson(json_array());
  AppendDeployment(deployment_json.get(), *deployment);
  json_object_set(root.get(), "deployment",
                  json_array_get(deployment_json.get(), 0));
  json_object_set_new(root.get(), "spec",
                      DirectorClientResourceSpecToJson(*spec.value));
  return JsonResponse(http::status::ok, DumpJson(root.get()));
}

http::response<http::string_body> HandleDirectorStoragePrefillRequest(
    ServiceState& state,
    std::string_view deployment_id,
    std::string_view director_name,
    std::string_view storage_name)
{
  auto deployment = state.GetDeployment(deployment_id);
  if (!deployment) {
    return ErrorResponse(http::status::not_found, "deployment not found.");
  }

  auto spec = state.GetDirectorStorageResourceSpec(deployment_id, director_name,
                                                   storage_name);
  if (!spec) { return ErrorResponse(http::status::bad_request, spec.error); }

  auto root = MakeJson(json_object());
  auto deployment_json = MakeJson(json_array());
  AppendDeployment(deployment_json.get(), *deployment);
  json_object_set(root.get(), "deployment",
                  json_array_get(deployment_json.get(), 0));
  json_object_set_new(root.get(), "spec",
                      DirectorStorageResourceSpecToJson(*spec.value));
  return JsonResponse(http::status::ok, DumpJson(root.get()));
}

http::response<http::string_body> HandleStorageDirectorPrefillRequest(
    ServiceState& state,
    std::string_view deployment_id,
    std::string_view storage_name,
    std::string_view director_name)
{
  auto deployment = state.GetDeployment(deployment_id);
  if (!deployment) {
    return ErrorResponse(http::status::not_found, "deployment not found.");
  }

  auto spec = state.GetStorageDirectorResourceSpec(deployment_id, storage_name,
                                                   director_name);
  if (!spec) { return ErrorResponse(http::status::bad_request, spec.error); }

  auto root = MakeJson(json_object());
  auto deployment_json = MakeJson(json_array());
  AppendDeployment(deployment_json.get(), *deployment);
  json_object_set(root.get(), "deployment",
                  json_array_get(deployment_json.get(), 0));
  json_object_set_new(root.get(), "spec",
                      StorageDirectorResourceSpecToJson(*spec.value));
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
  if (spec->query_file || spec->subscriptions
      || spec->maximum_console_connections || spec->password
      || spec->statistics_retention || spec->key_encryption_key
      || spec->ndmp_namelist_fhinfo_set_zero_for_invalid_uquad || spec->auditing
      || spec->audit_events || spec->just_in_time_reservation
      || spec->ndmp_enable || spec->ndmp_snooping || spec->ndmp_log_level
      || spec->ndmp_address || spec->ndmp_port || spec->ndmp_addresses
      || spec->autoxflate_on_replication || spec->collect_device_statistics
      || spec->collect_job_statistics || spec->statistics_collect_interval
      || spec->device_reserve_by_media_type || spec->file_device_concurrent_read
      || spec->fd_connect_timeout || spec->checkpoint_interval
      || spec->client_connect_wait
#if defined(HAVE_DYNAMIC_SD_BACKENDS)
      || spec->backend_directories
#endif
  ) {
    return ErrorResponse(
        http::status::bad_request,
        "client-daemon singleton updates do not support query_file, "
        "subscriptions, maximum_console_connections, password, "
        "statistics_retention, key_encryption_key, "
        "ndmp_namelist_fhinfo_set_zero_for_invalid_uquad, auditing, "
        "audit_events, storage-only reservation and NDMP fields, "
        "statistics_collect_interval, fd_connect_timeout, "
        "checkpoint_interval, client_connect_wait, or backend_directories.");
  }

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
  if (spec->just_in_time_reservation || spec->maximum_workers_per_job
      || spec->allow_bandwidth_bursting || spec->pki_signatures
      || spec->pki_encryption || spec->pki_key_pair || spec->pki_signers
      || spec->pki_master_keys || spec->pki_cipher || spec->always_use_lmdb
      || spec->lmdb_threshold || spec->maximum_bandwidth_per_job
      || spec->grpc_module || spec->ndmp_enable || spec->ndmp_address
      || spec->ndmp_port || spec->ndmp_addresses
      || spec->autoxflate_on_replication || spec->collect_device_statistics
      || spec->collect_job_statistics || spec->device_reserve_by_media_type
      || spec->file_device_concurrent_read || spec->checkpoint_interval
      || spec->client_connect_wait || spec->maximum_network_buffer_size
      || spec->allowed_script_dirs || spec->allowed_job_commands
#if defined(HAVE_DYNAMIC_SD_BACKENDS)
      || spec->backend_directories
#endif
  ) {
    return ErrorResponse(
        http::status::bad_request,
        "director-daemon singleton updates do not support "
        "storage-only reservation, PKI, LMDB, maximum_bandwidth_per_job, "
        "grpc_module, storage NDMP address/device fields, "
        "device-statistics fields, checkpoint_interval, "
        "client_connect_wait, maximum_network_buffer_size, "
        "allowed_script_dirs, allowed_job_commands, or "
        "backend_directories.");
  }

  DirectorDaemonResourceSpec resource_spec{
      .address = spec->address,
      .addresses = spec->addresses,
      .source_address = spec->source_address,
      .source_addresses = spec->source_addresses,
      .port = spec->port,
      .query_file = spec->query_file,
      .subscriptions = spec->subscriptions,
      .maximum_concurrent_jobs = spec->maximum_concurrent_jobs,
      .maximum_console_connections = spec->maximum_console_connections,
      .password = spec->password,
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
      .statistics_retention = spec->statistics_retention,
      .statistics_collect_interval = spec->statistics_collect_interval,
      .description = spec->description,
      .key_encryption_key = spec->key_encryption_key,
      .ndmp_snooping = spec->ndmp_snooping,
      .ndmp_log_level = spec->ndmp_log_level,
      .ndmp_namelist_fhinfo_set_zero_for_invalid_uquad
      = spec->ndmp_namelist_fhinfo_set_zero_for_invalid_uquad,
      .auditing = spec->auditing,
      .audit_events = spec->audit_events,
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
      .mail_command = spec->mail_command,
      .operator_command = spec->operator_command,
      .timestamp_format = spec->timestamp_format,
      .syslog_entries = spec->syslog_entries,
      .mail_entries = spec->mail_entries,
      .mail_on_error_entries = spec->mail_on_error_entries,
      .mail_on_success_entries = spec->mail_on_success_entries,
      .file_entries = spec->file_entries,
      .append_entries = spec->append_entries,
      .stdout_entries = spec->stdout_entries,
      .stderr_entries = spec->stderr_entries,
      .director_entries = spec->director_entries,
      .console_entries = spec->console_entries,
      .operator_entries = spec->operator_entries,
      .catalog_entries = spec->catalog_entries,
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
      .password = spec->password,
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
  if (spec->maximum_workers_per_job || spec->pki_signatures
      || spec->pki_encryption || spec->pki_key_pair || spec->pki_signers
      || spec->pki_master_keys || spec->pki_cipher || spec->always_use_lmdb
      || spec->lmdb_threshold || spec->grpc_module || spec->allowed_script_dirs
      || spec->allowed_job_commands || spec->query_file || spec->subscriptions
      || spec->maximum_console_connections || spec->password
      || spec->statistics_retention || spec->key_encryption_key
      || spec->ndmp_namelist_fhinfo_set_zero_for_invalid_uquad || spec->auditing
      || spec->audit_events) {
    return ErrorResponse(
        http::status::bad_request,
        "storage-daemon singleton updates do not support "
        "maximum_workers_per_job, PKI, always_use_lmdb, lmdb_threshold, "
        "grpc_module, allowed_script_dirs, allowed_job_commands, "
        "query_file, subscriptions, maximum_console_connections, password, "
        "statistics_retention, key_encryption_key, "
        "ndmp_namelist_fhinfo_set_zero_for_invalid_uquad, auditing, or "
        "audit_events.");
  }

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
      .mail_command = spec->mail_command,
      .operator_command = spec->operator_command,
      .timestamp_format = spec->timestamp_format,
      .syslog_entries = spec->syslog_entries,
      .mail_entries = spec->mail_entries,
      .mail_on_error_entries = spec->mail_on_error_entries,
      .mail_on_success_entries = spec->mail_on_success_entries,
      .file_entries = spec->file_entries,
      .append_entries = spec->append_entries,
      .stdout_entries = spec->stdout_entries,
      .stderr_entries = spec->stderr_entries,
      .director_entries = spec->director_entries,
      .console_entries = spec->console_entries,
      .operator_entries = spec->operator_entries,
      .catalog_entries = spec->catalog_entries,
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
      .key_encryption_key = spec->key_encryption_key,
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
      .access_mode = spec->access_mode,
      .device_options = spec->device_options,
      .diagnostic_device = spec->diagnostic_device,
      .hardware_end_of_file = spec->hardware_end_of_file,
      .hardware_end_of_medium = spec->hardware_end_of_medium,
      .backward_space_record = spec->backward_space_record,
      .backward_space_file = spec->backward_space_file,
      .bsf_at_eom = spec->bsf_at_eom,
      .two_eof = spec->two_eof,
      .forward_space_record = spec->forward_space_record,
      .forward_space_file = spec->forward_space_file,
      .fast_forward_space_file = spec->fast_forward_space_file,
      .removable_media = spec->removable_media,
      .random_access = spec->random_access,
      .automatic_mount = spec->automatic_mount,
      .label_media = spec->label_media,
      .always_open = spec->always_open,
      .autochanger = spec->autochanger,
      .close_on_poll = spec->close_on_poll,
      .block_positioning = spec->block_positioning,
      .use_mtiocget = spec->use_mtiocget,
      .check_labels = spec->check_labels,
      .requires_mount = spec->requires_mount,
      .offline_on_unmount = spec->offline_on_unmount,
      .block_checksum = spec->block_checksum,
      .auto_select = spec->auto_select,
      .changer_device = spec->changer_device,
      .changer_command = spec->changer_command,
      .alert_command = spec->alert_command,
      .maximum_changer_wait = spec->maximum_changer_wait,
      .maximum_open_wait = spec->maximum_open_wait,
      .maximum_open_volumes = spec->maximum_open_volumes,
      .maximum_network_buffer_size = spec->maximum_network_buffer_size,
      .volume_poll_interval = spec->volume_poll_interval,
      .maximum_rewind_wait = spec->maximum_rewind_wait,
      .label_block_size = spec->label_block_size,
      .minimum_block_size = spec->minimum_block_size,
      .maximum_block_size = spec->maximum_block_size,
      .maximum_file_size = spec->maximum_file_size,
      .volume_capacity = spec->volume_capacity,
      .maximum_concurrent_jobs = spec->maximum_concurrent_jobs,
      .spool_directory = spec->spool_directory,
      .maximum_spool_size = spec->maximum_spool_size,
      .maximum_job_spool_size = spec->maximum_job_spool_size,
      .drive_index = spec->drive_index,
      .mount_point = spec->mount_point,
      .mount_command = spec->mount_command,
      .unmount_command = spec->unmount_command,
      .label_type = spec->label_type,
      .no_rewind_on_close = spec->no_rewind_on_close,
      .drive_tape_alert_enabled = spec->drive_tape_alert_enabled,
      .drive_crypto_enabled = spec->drive_crypto_enabled,
      .query_crypto_status = spec->query_crypto_status,
      .auto_deflate = spec->auto_deflate,
      .auto_deflate_algorithm = spec->auto_deflate_algorithm,
      .auto_deflate_level = spec->auto_deflate_level,
      .auto_inflate = spec->auto_inflate,
      .collect_statistics = spec->collect_statistics,
      .eof_on_error_is_eot = spec->eof_on_error_is_eot,
      .count = spec->count,
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
      .description = spec->description,
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
      .lan_address = spec->lan_address,
      .port = spec->port,
      .protocol = spec->protocol,
      .auth_type = spec->auth_type,
      .catalog = spec->catalog,
      .username = spec->username,
      .password = spec->password,
      .enabled = spec->enabled,
      .passive = spec->passive,
      .strict_quotas = spec->strict_quotas,
      .quota_include_failed_jobs = spec->quota_include_failed_jobs,
      .soft_quota = spec->soft_quota,
      .hard_quota = spec->hard_quota,
      .soft_quota_grace_period = spec->soft_quota_grace_period,
      .file_retention = spec->file_retention,
      .job_retention = spec->job_retention,
      .ndmp_log_level = spec->ndmp_log_level,
      .ndmp_block_size = spec->ndmp_block_size,
      .ndmp_use_lmdb = spec->ndmp_use_lmdb,
      .auto_prune = spec->auto_prune,
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
      .maximum_concurrent_jobs = spec->maximum_concurrent_jobs,
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
      .lan_address = spec->lan_address,
      .port = spec->port,
      .protocol = spec->protocol,
      .auth_type = spec->auth_type,
      .username = spec->username,
      .password = spec->password,
      .device = spec->device,
      .media_type = spec->media_type,
      .autochanger = spec->autochanger,
      .enabled = spec->enabled,
      .allow_compression = spec->allow_compression,
      .heartbeat_interval = spec->heartbeat_interval,
      .cache_status_interval = spec->cache_status_interval,
      .maximum_concurrent_jobs = spec->maximum_concurrent_jobs,
      .maximum_concurrent_read_jobs = spec->maximum_concurrent_read_jobs,
      .paired_storage = spec->paired_storage,
      .maximum_bandwidth_per_job = spec->maximum_bandwidth_per_job,
      .collect_statistics = spec->collect_statistics,
      .ndmp_changer_device = spec->ndmp_changer_device,
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
      .job_acl = spec->job_acl,
      .client_acl = spec->client_acl,
      .storage_acl = spec->storage_acl,
      .schedule_acl = spec->schedule_acl,
      .pool_acl = spec->pool_acl,
      .command_acl = spec->command_acl,
      .fileset_acl = spec->fileset_acl,
      .catalog_acl = spec->catalog_acl,
      .where_acl = spec->where_acl,
      .plugin_options_acl = spec->plugin_options_acl,
      .profiles = spec->profiles,
      .use_pam_authentication = spec->use_pam_authentication,
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
      .job_acl = spec->job_acl,
      .client_acl = spec->client_acl,
      .storage_acl = spec->storage_acl,
      .schedule_acl = spec->schedule_acl,
      .pool_acl = spec->pool_acl,
      .command_acl = spec->command_acl,
      .fileset_acl = spec->fileset_acl,
      .catalog_acl = spec->catalog_acl,
      .where_acl = spec->where_acl,
      .plugin_options_acl = spec->plugin_options_acl,
      .profiles = spec->profiles,
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
      .job_acl = spec->job_acl,
      .client_acl = spec->client_acl,
      .storage_acl = spec->storage_acl,
      .schedule_acl = spec->schedule_acl,
      .pool_acl = spec->pool_acl,
      .command_acl = spec->command_acl,
      .fileset_acl = spec->fileset_acl,
      .catalog_acl = spec->catalog_acl,
      .where_acl = spec->where_acl,
      .plugin_options_acl = spec->plugin_options_acl,
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
      .cleaning_prefix = spec->cleaning_prefix,
      .label_type = spec->label_type,
      .maximum_volumes = spec->maximum_volumes,
      .maximum_volume_jobs = spec->maximum_volume_jobs,
      .maximum_volume_files = spec->maximum_volume_files,
      .maximum_volume_bytes = spec->maximum_volume_bytes,
      .volume_retention = spec->volume_retention,
      .volume_use_duration = spec->volume_use_duration,
      .migration_time = spec->migration_time,
      .migration_high_bytes = spec->migration_high_bytes,
      .migration_low_bytes = spec->migration_low_bytes,
      .next_pool = spec->next_pool,
      .storages = spec->storages,
      .use_catalog = spec->use_catalog,
      .catalog_files = spec->catalog_files,
      .purge_oldest_volume = spec->purge_oldest_volume,
      .action_on_purge = spec->action_on_purge,
      .recycle_oldest_volume = spec->recycle_oldest_volume,
      .recycle_current_volume = spec->recycle_current_volume,
      .auto_prune = spec->auto_prune,
      .recycle = spec->recycle,
      .recycle_pool = spec->recycle_pool,
      .scratch_pool = spec->scratch_pool,
      .catalog = spec->catalog,
      .file_retention = spec->file_retention,
      .job_retention = spec->job_retention,
      .minimum_block_size = spec->minimum_block_size,
      .maximum_block_size = spec->maximum_block_size,
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
      .multiple_connections = spec->multiple_connections,
      .disable_batch_insert = spec->disable_batch_insert,
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
      .mail_command = spec->mail_command,
      .operator_command = spec->operator_command,
      .timestamp_format = spec->timestamp_format,
      .syslog_entries = spec->syslog_entries,
      .mail_entries = spec->mail_entries,
      .mail_on_error_entries = spec->mail_on_error_entries,
      .mail_on_success_entries = spec->mail_on_success_entries,
      .file_entries = spec->file_entries,
      .append_entries = spec->append_entries,
      .stdout_entries = spec->stdout_entries,
      .stderr_entries = spec->stderr_entries,
      .director_entries = spec->director_entries,
      .console_entries = spec->console_entries,
      .operator_entries = spec->operator_entries,
      .catalog_entries = spec->catalog_entries,
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
      .backup_format = spec->backup_format,
      .protocol = spec->protocol,
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
      .run_entries = spec->run_entries,
      .run_before_job_entries = spec->run_before_job_entries,
      .run_after_job_entries = spec->run_after_job_entries,
      .run_after_failed_job_entries = spec->run_after_failed_job_entries,
      .client_run_before_job_entries = spec->client_run_before_job_entries,
      .client_run_after_job_entries = spec->client_run_after_job_entries,
      .runscript_blocks = spec->runscript_blocks,
      .where = spec->where,
      .replace = spec->replace,
      .regex_where = spec->regex_where,
      .strip_prefix = spec->strip_prefix,
      .add_prefix = spec->add_prefix,
      .add_suffix = spec->add_suffix,
      .bootstrap = spec->bootstrap,
      .write_bootstrap = spec->write_bootstrap,
      .write_verify_list = spec->write_verify_list,
      .maximum_bandwidth = spec->maximum_bandwidth,
      .max_run_sched_time = spec->max_run_sched_time,
      .max_run_time = spec->max_run_time,
      .full_max_runtime = spec->full_max_runtime,
      .incremental_max_runtime = spec->incremental_max_runtime,
      .differential_max_runtime = spec->differential_max_runtime,
      .max_wait_time = spec->max_wait_time,
      .max_start_delay = spec->max_start_delay,
      .max_full_interval = spec->max_full_interval,
      .max_virtual_full_interval = spec->max_virtual_full_interval,
      .max_diff_interval = spec->max_diff_interval,
      .prefix_links = spec->prefix_links,
      .prune_jobs = spec->prune_jobs,
      .prune_files = spec->prune_files,
      .prune_volumes = spec->prune_volumes,
      .purge_migration_job = spec->purge_migration_job,
      .spool_attributes = spec->spool_attributes,
      .spool_data = spec->spool_data,
      .spool_size = spec->spool_size,
      .rerun_failed_levels = spec->rerun_failed_levels,
      .prefer_mounted_volumes = spec->prefer_mounted_volumes,
      .maximum_concurrent_jobs = spec->maximum_concurrent_jobs,
      .reschedule_on_error = spec->reschedule_on_error,
      .reschedule_interval = spec->reschedule_interval,
      .reschedule_times = spec->reschedule_times,
      .priority = spec->priority,
      .allow_mixed_priority = spec->allow_mixed_priority,
      .selection_type = spec->selection_type,
      .selection_pattern = spec->selection_pattern,
      .accurate = spec->accurate,
      .allow_duplicate_jobs = spec->allow_duplicate_jobs,
      .allow_higher_duplicates = spec->allow_higher_duplicates,
      .cancel_lower_level_duplicates = spec->cancel_lower_level_duplicates,
      .cancel_queued_duplicates = spec->cancel_queued_duplicates,
      .cancel_running_duplicates = spec->cancel_running_duplicates,
      .save_file_history = spec->save_file_history,
      .file_history_size = spec->file_history_size,
      .fd_plugin_options = spec->fd_plugin_options,
      .sd_plugin_options = spec->sd_plugin_options,
      .dir_plugin_options = spec->dir_plugin_options,
      .max_concurrent_copies = spec->max_concurrent_copies,
      .always_incremental = spec->always_incremental,
      .always_incremental_job_retention
      = spec->always_incremental_job_retention,
      .always_incremental_keep_number = spec->always_incremental_keep_number,
      .always_incremental_max_full_age = spec->always_incremental_max_full_age,
      .max_full_consolidations = spec->max_full_consolidations,
      .run_on_incoming_connect_interval
      = spec->run_on_incoming_connect_interval,
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
      .backup_format = spec->backup_format,
      .protocol = spec->protocol,
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
      .run_entries = spec->run_entries,
      .run_before_job_entries = spec->run_before_job_entries,
      .run_after_job_entries = spec->run_after_job_entries,
      .run_after_failed_job_entries = spec->run_after_failed_job_entries,
      .client_run_before_job_entries = spec->client_run_before_job_entries,
      .client_run_after_job_entries = spec->client_run_after_job_entries,
      .runscript_blocks = spec->runscript_blocks,
      .where = spec->where,
      .replace = spec->replace,
      .regex_where = spec->regex_where,
      .strip_prefix = spec->strip_prefix,
      .add_prefix = spec->add_prefix,
      .add_suffix = spec->add_suffix,
      .bootstrap = spec->bootstrap,
      .write_bootstrap = spec->write_bootstrap,
      .write_verify_list = spec->write_verify_list,
      .maximum_bandwidth = spec->maximum_bandwidth,
      .max_run_sched_time = spec->max_run_sched_time,
      .max_run_time = spec->max_run_time,
      .full_max_runtime = spec->full_max_runtime,
      .incremental_max_runtime = spec->incremental_max_runtime,
      .differential_max_runtime = spec->differential_max_runtime,
      .max_wait_time = spec->max_wait_time,
      .max_start_delay = spec->max_start_delay,
      .max_full_interval = spec->max_full_interval,
      .max_virtual_full_interval = spec->max_virtual_full_interval,
      .max_diff_interval = spec->max_diff_interval,
      .prefix_links = spec->prefix_links,
      .prune_jobs = spec->prune_jobs,
      .prune_files = spec->prune_files,
      .prune_volumes = spec->prune_volumes,
      .purge_migration_job = spec->purge_migration_job,
      .spool_attributes = spec->spool_attributes,
      .spool_data = spec->spool_data,
      .spool_size = spec->spool_size,
      .rerun_failed_levels = spec->rerun_failed_levels,
      .prefer_mounted_volumes = spec->prefer_mounted_volumes,
      .maximum_concurrent_jobs = spec->maximum_concurrent_jobs,
      .reschedule_on_error = spec->reschedule_on_error,
      .reschedule_interval = spec->reschedule_interval,
      .reschedule_times = spec->reschedule_times,
      .priority = spec->priority,
      .allow_mixed_priority = spec->allow_mixed_priority,
      .selection_type = spec->selection_type,
      .selection_pattern = spec->selection_pattern,
      .accurate = spec->accurate,
      .allow_duplicate_jobs = spec->allow_duplicate_jobs,
      .allow_higher_duplicates = spec->allow_higher_duplicates,
      .cancel_lower_level_duplicates = spec->cancel_lower_level_duplicates,
      .cancel_queued_duplicates = spec->cancel_queued_duplicates,
      .cancel_running_duplicates = spec->cancel_running_duplicates,
      .save_file_history = spec->save_file_history,
      .file_history_size = spec->file_history_size,
      .fd_plugin_options = spec->fd_plugin_options,
      .sd_plugin_options = spec->sd_plugin_options,
      .dir_plugin_options = spec->dir_plugin_options,
      .max_concurrent_copies = spec->max_concurrent_copies,
      .always_incremental = spec->always_incremental,
      .always_incremental_job_retention
      = spec->always_incremental_job_retention,
      .always_incremental_keep_number = spec->always_incremental_keep_number,
      .always_incremental_max_full_age = spec->always_incremental_max_full_age,
      .max_full_consolidations = spec->max_full_consolidations,
      .run_on_incoming_connect_interval
      = spec->run_on_incoming_connect_interval,
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
  auto* password = json_object_get(root.get(), "password");
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
  if (password && !json_is_null(password) && !json_is_string(password)) {
    error = "field 'password' must be a string when provided.";
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
  if (password && json_is_string(password)) {
    spec.password = std::string{json_string_value(password)};
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
  auto* lan_address = json_object_get(root.get(), "lan_address");
  auto* port = json_object_get(root.get(), "port");
  auto* protocol = json_object_get(root.get(), "protocol");
  auto* auth_type = json_object_get(root.get(), "auth_type");
  auto* catalog = json_object_get(root.get(), "catalog");
  auto* username = json_object_get(root.get(), "username");
  auto* password = json_object_get(root.get(), "password");
  auto* enabled = json_object_get(root.get(), "enabled");
  auto* passive = json_object_get(root.get(), "passive");
  auto* strict_quotas = json_object_get(root.get(), "strict_quotas");
  auto* quota_include_failed_jobs
      = json_object_get(root.get(), "quota_include_failed_jobs");
  auto* soft_quota = json_object_get(root.get(), "soft_quota");
  auto* hard_quota = json_object_get(root.get(), "hard_quota");
  auto* soft_quota_grace_period
      = json_object_get(root.get(), "soft_quota_grace_period");
  auto* file_retention = json_object_get(root.get(), "file_retention");
  auto* job_retention = json_object_get(root.get(), "job_retention");
  auto* ndmp_log_level = json_object_get(root.get(), "ndmp_log_level");
  auto* ndmp_block_size = json_object_get(root.get(), "ndmp_block_size");
  auto* ndmp_use_lmdb = json_object_get(root.get(), "ndmp_use_lmdb");
  auto* auto_prune = json_object_get(root.get(), "auto_prune");
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
  auto* maximum_concurrent_jobs
      = json_object_get(root.get(), "maximum_concurrent_jobs");
  auto* heartbeat_interval = json_object_get(root.get(), "heartbeat_interval");
  auto* maximum_bandwidth_per_job
      = json_object_get(root.get(), "maximum_bandwidth_per_job");
  auto* description = json_object_get(root.get(), "description");
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

  if (address && !json_is_null(address) && !json_is_string(address)) {
    error = "field 'address' must be a string when provided.";
    return std::nullopt;
  }
  if (lan_address && !json_is_null(lan_address)
      && !json_is_string(lan_address)) {
    error = "field 'lan_address' must be a string when provided.";
    return std::nullopt;
  }
  if (port && !json_is_null(port) && !json_is_integer(port)) {
    error = "field 'port' must be an integer when provided.";
    return std::nullopt;
  }
  if (protocol && !json_is_null(protocol) && !json_is_string(protocol)) {
    error = "field 'protocol' must be a string when provided.";
    return std::nullopt;
  }
  if (auth_type && !json_is_null(auth_type) && !json_is_string(auth_type)) {
    error = "field 'auth_type' must be a string when provided.";
    return std::nullopt;
  }
  if (catalog && !json_is_null(catalog) && !json_is_string(catalog)) {
    error = "field 'catalog' must be a string when provided.";
    return std::nullopt;
  }
  if (username && !json_is_null(username) && !json_is_string(username)) {
    error = "field 'username' must be a string when provided.";
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
  if (strict_quotas && !json_is_null(strict_quotas)
      && !json_is_boolean(strict_quotas)) {
    error = "field 'strict_quotas' must be a boolean when provided.";
    return std::nullopt;
  }
  if (quota_include_failed_jobs && !json_is_null(quota_include_failed_jobs)
      && !json_is_boolean(quota_include_failed_jobs)) {
    error
        = "field 'quota_include_failed_jobs' must be a boolean when provided.";
    return std::nullopt;
  }
  if (soft_quota && !json_is_null(soft_quota) && !json_is_integer(soft_quota)) {
    error = "field 'soft_quota' must be an integer when provided.";
    return std::nullopt;
  }
  if (hard_quota && !json_is_null(hard_quota) && !json_is_integer(hard_quota)) {
    error = "field 'hard_quota' must be an integer when provided.";
    return std::nullopt;
  }
  if (soft_quota_grace_period && !json_is_null(soft_quota_grace_period)
      && !json_is_integer(soft_quota_grace_period)) {
    error = "field 'soft_quota_grace_period' must be an integer when provided.";
    return std::nullopt;
  }
  if (file_retention && !json_is_null(file_retention)
      && !json_is_integer(file_retention)) {
    error = "field 'file_retention' must be an integer when provided.";
    return std::nullopt;
  }
  if (job_retention && !json_is_null(job_retention)
      && !json_is_integer(job_retention)) {
    error = "field 'job_retention' must be an integer when provided.";
    return std::nullopt;
  }
  if (ndmp_log_level && !json_is_null(ndmp_log_level)
      && !json_is_integer(ndmp_log_level)) {
    error = "field 'ndmp_log_level' must be an integer when provided.";
    return std::nullopt;
  }
  if (ndmp_block_size && !json_is_null(ndmp_block_size)
      && !json_is_integer(ndmp_block_size)) {
    error = "field 'ndmp_block_size' must be an integer when provided.";
    return std::nullopt;
  }
  if (ndmp_use_lmdb && !json_is_null(ndmp_use_lmdb)
      && !json_is_boolean(ndmp_use_lmdb)) {
    error = "field 'ndmp_use_lmdb' must be a boolean when provided.";
    return std::nullopt;
  }
  if (auto_prune && !json_is_null(auto_prune) && !json_is_boolean(auto_prune)) {
    error = "field 'auto_prune' must be a boolean when provided.";
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
  if (maximum_concurrent_jobs && !json_is_null(maximum_concurrent_jobs)
      && !json_is_integer(maximum_concurrent_jobs)) {
    error = "field 'maximum_concurrent_jobs' must be an integer when provided.";
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
  if (port && json_is_integer(port)) {
    const auto value = json_integer_value(port);
    if (value <= 0 || value > 65535) {
      error = "field 'port' must be between 1 and 65535.";
      return std::nullopt;
    }
    spec.port = static_cast<uint16_t>(value);
  }
  if (protocol && json_is_string(protocol)) {
    spec.protocol = std::string{json_string_value(protocol)};
  }
  if (auth_type && json_is_string(auth_type)) {
    spec.auth_type = std::string{json_string_value(auth_type)};
  }
  if (catalog && json_is_string(catalog)) {
    spec.catalog = std::string{json_string_value(catalog)};
  }
  if (username && json_is_string(username)) {
    spec.username = std::string{json_string_value(username)};
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
  if (strict_quotas && json_is_boolean(strict_quotas)) {
    spec.strict_quotas = json_is_true(strict_quotas);
  }
  if (quota_include_failed_jobs && json_is_boolean(quota_include_failed_jobs)) {
    spec.quota_include_failed_jobs = json_is_true(quota_include_failed_jobs);
  }
  if (soft_quota && json_is_integer(soft_quota)) {
    const auto value = json_integer_value(soft_quota);
    if (value < 0) {
      error = "field 'soft_quota' must be non-negative.";
      return std::nullopt;
    }
    spec.soft_quota = static_cast<uint64_t>(value);
  }
  if (hard_quota && json_is_integer(hard_quota)) {
    const auto value = json_integer_value(hard_quota);
    if (value < 0) {
      error = "field 'hard_quota' must be non-negative.";
      return std::nullopt;
    }
    spec.hard_quota = static_cast<uint64_t>(value);
  }
  if (soft_quota_grace_period && json_is_integer(soft_quota_grace_period)) {
    const auto value = json_integer_value(soft_quota_grace_period);
    if (value < 0) {
      error = "field 'soft_quota_grace_period' must be non-negative.";
      return std::nullopt;
    }
    spec.soft_quota_grace_period = static_cast<uint64_t>(value);
  }
  if (file_retention && json_is_integer(file_retention)) {
    const auto value = json_integer_value(file_retention);
    if (value < 0) {
      error = "field 'file_retention' must be non-negative.";
      return std::nullopt;
    }
    spec.file_retention = static_cast<uint64_t>(value);
  }
  if (job_retention && json_is_integer(job_retention)) {
    const auto value = json_integer_value(job_retention);
    if (value < 0) {
      error = "field 'job_retention' must be non-negative.";
      return std::nullopt;
    }
    spec.job_retention = static_cast<uint64_t>(value);
  }
  if (ndmp_log_level && json_is_integer(ndmp_log_level)) {
    const auto value = json_integer_value(ndmp_log_level);
    if (value < 0 || value > std::numeric_limits<uint32_t>::max()) {
      error = "field 'ndmp_log_level' must be between 0 and "
              + std::to_string(std::numeric_limits<uint32_t>::max()) + ".";
      return std::nullopt;
    }
    spec.ndmp_log_level = static_cast<uint32_t>(value);
  }
  if (ndmp_block_size && json_is_integer(ndmp_block_size)) {
    const auto value = json_integer_value(ndmp_block_size);
    if (value < 0 || value > std::numeric_limits<uint32_t>::max()) {
      error = "field 'ndmp_block_size' must be between 0 and "
              + std::to_string(std::numeric_limits<uint32_t>::max()) + ".";
      return std::nullopt;
    }
    spec.ndmp_block_size = static_cast<uint32_t>(value);
  }
  if (ndmp_use_lmdb && json_is_boolean(ndmp_use_lmdb)) {
    spec.ndmp_use_lmdb = json_is_true(ndmp_use_lmdb);
  }
  if (auto_prune && json_is_boolean(auto_prune)) {
    spec.auto_prune = json_is_true(auto_prune);
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
  spec.tls_allowed_cn = parse_string_array(tls_allowed_cn);
  if (maximum_concurrent_jobs && json_is_integer(maximum_concurrent_jobs)) {
    const auto value = json_integer_value(maximum_concurrent_jobs);
    if (value < 0 || value > std::numeric_limits<uint32_t>::max()) {
      error = "field 'maximum_concurrent_jobs' must be between 0 and "
              + std::to_string(std::numeric_limits<uint32_t>::max()) + ".";
      return std::nullopt;
    }
    spec.maximum_concurrent_jobs = static_cast<uint32_t>(value);
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
  auto* lan_address = json_object_get(root.get(), "lan_address");
  auto* port = json_object_get(root.get(), "port");
  auto* protocol = json_object_get(root.get(), "protocol");
  auto* auth_type = json_object_get(root.get(), "auth_type");
  auto* username = json_object_get(root.get(), "username");
  auto* password = json_object_get(root.get(), "password");
  auto* device = json_object_get(root.get(), "device");
  auto* media_type = json_object_get(root.get(), "media_type");
  auto* autochanger = json_object_get(root.get(), "autochanger");
  auto* enabled = json_object_get(root.get(), "enabled");
  auto* allow_compression = json_object_get(root.get(), "allow_compression");
  auto* heartbeat_interval = json_object_get(root.get(), "heartbeat_interval");
  auto* cache_status_interval
      = json_object_get(root.get(), "cache_status_interval");
  auto* maximum_concurrent_jobs
      = json_object_get(root.get(), "maximum_concurrent_jobs");
  auto* maximum_concurrent_read_jobs
      = json_object_get(root.get(), "maximum_concurrent_read_jobs");
  auto* paired_storage = json_object_get(root.get(), "paired_storage");
  auto* maximum_bandwidth_per_job
      = json_object_get(root.get(), "maximum_bandwidth_per_job");
  auto* collect_statistics = json_object_get(root.get(), "collect_statistics");
  auto* ndmp_changer_device
      = json_object_get(root.get(), "ndmp_changer_device");
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
  auto require_bool = [&error](json_t* value, const char* field) {
    if (value && !json_is_null(value) && !json_is_boolean(value)) {
      error = std::string{"field '"} + field
              + "' must be a boolean when provided.";
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

  if (!require_string(address, "address")) {
    error = "field 'address' must be a string when provided.";
    return std::nullopt;
  }
  if (!require_string(lan_address, "lan_address")
      || !require_string(protocol, "protocol")
      || !require_string(auth_type, "auth_type")
      || !require_string(username, "username")
      || !require_string(password, "password")
      || !require_string(device, "device")
      || !require_string(media_type, "media_type")
      || !require_string(paired_storage, "paired_storage")
      || !require_string(ndmp_changer_device, "ndmp_changer_device")
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
      || !require_string(archive_device, "archive_device")
      || !require_string(device_type, "device_type")
      || !require_string(description, "description")
      || !require_string_array(tls_allowed_cn, "tls_allowed_cn")) {
    return std::nullopt;
  }
  if (port && !json_is_null(port) && !json_is_integer(port)) {
    error = "field 'port' must be an integer when provided.";
    return std::nullopt;
  }
  if (!require_bool(autochanger, "autochanger")
      || !require_bool(enabled, "enabled")
      || !require_bool(allow_compression, "allow_compression")
      || !require_bool(collect_statistics, "collect_statistics")
      || !require_bool(tls_authenticate, "tls_authenticate")
      || !require_bool(tls_enable, "tls_enable")
      || !require_bool(tls_require, "tls_require")
      || !require_bool(tls_verify_peer, "tls_verify_peer")) {
    return std::nullopt;
  }
  if (heartbeat_interval && !json_is_null(heartbeat_interval)
      && !json_is_integer(heartbeat_interval)) {
    error = "field 'heartbeat_interval' must be an integer when provided.";
    return std::nullopt;
  }
  if (cache_status_interval && !json_is_null(cache_status_interval)
      && !json_is_integer(cache_status_interval)) {
    error = "field 'cache_status_interval' must be an integer when provided.";
    return std::nullopt;
  }
  if (maximum_bandwidth_per_job && !json_is_null(maximum_bandwidth_per_job)
      && !json_is_integer(maximum_bandwidth_per_job)) {
    error
        = "field 'maximum_bandwidth_per_job' must be an integer when "
          "provided.";
    return std::nullopt;
  }
  if (maximum_concurrent_jobs && !json_is_null(maximum_concurrent_jobs)
      && !json_is_integer(maximum_concurrent_jobs)) {
    error = "field 'maximum_concurrent_jobs' must be an integer when provided.";
    return std::nullopt;
  }
  if (maximum_concurrent_read_jobs
      && !json_is_null(maximum_concurrent_read_jobs)
      && !json_is_integer(maximum_concurrent_read_jobs)) {
    error
        = "field 'maximum_concurrent_read_jobs' must be an integer when "
          "provided.";
    return std::nullopt;
  }

  DirectorStorageRequestSpec spec{};
  if (address && json_is_string(address)) {
    spec.address = std::string{json_string_value(address)};
  }
  if (lan_address && json_is_string(lan_address)) {
    spec.lan_address = std::string{json_string_value(lan_address)};
  }
  if (port && json_is_integer(port)) {
    const auto value = json_integer_value(port);
    if (value <= 0 || value > 65535) {
      error = "field 'port' must be between 1 and 65535.";
      return std::nullopt;
    }
    spec.port = static_cast<uint16_t>(value);
  }
  if (protocol && json_is_string(protocol)) {
    spec.protocol = std::string{json_string_value(protocol)};
  }
  if (auth_type && json_is_string(auth_type)) {
    spec.auth_type = std::string{json_string_value(auth_type)};
  }
  if (username && json_is_string(username)) {
    spec.username = std::string{json_string_value(username)};
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
  if (autochanger && json_is_boolean(autochanger)) {
    spec.autochanger = json_is_true(autochanger);
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
  if (cache_status_interval && json_is_integer(cache_status_interval)) {
    const auto value = json_integer_value(cache_status_interval);
    if (value < 0) {
      error = "field 'cache_status_interval' must be non-negative.";
      return std::nullopt;
    }
    spec.cache_status_interval = static_cast<uint64_t>(value);
  }
  if (maximum_concurrent_jobs && json_is_integer(maximum_concurrent_jobs)) {
    const auto value = json_integer_value(maximum_concurrent_jobs);
    if (value < 0 || value > std::numeric_limits<uint32_t>::max()) {
      error = "field 'maximum_concurrent_jobs' must be between 0 and "
              + std::to_string(std::numeric_limits<uint32_t>::max()) + ".";
      return std::nullopt;
    }
    spec.maximum_concurrent_jobs = static_cast<uint32_t>(value);
  }
  if (maximum_concurrent_read_jobs
      && json_is_integer(maximum_concurrent_read_jobs)) {
    const auto value = json_integer_value(maximum_concurrent_read_jobs);
    if (value < 0 || value > std::numeric_limits<uint32_t>::max()) {
      error = "field 'maximum_concurrent_read_jobs' must be between 0 and "
              + std::to_string(std::numeric_limits<uint32_t>::max()) + ".";
      return std::nullopt;
    }
    spec.maximum_concurrent_read_jobs = static_cast<uint32_t>(value);
  }
  if (paired_storage && json_is_string(paired_storage)) {
    spec.paired_storage = std::string{json_string_value(paired_storage)};
  }
  if (maximum_bandwidth_per_job && json_is_integer(maximum_bandwidth_per_job)) {
    const auto value = json_integer_value(maximum_bandwidth_per_job);
    if (value < 0) {
      error = "field 'maximum_bandwidth_per_job' must be non-negative.";
      return std::nullopt;
    }
    spec.maximum_bandwidth_per_job = static_cast<uint64_t>(value);
  }
  if (collect_statistics && json_is_boolean(collect_statistics)) {
    spec.collect_statistics = json_is_true(collect_statistics);
  }
  if (ndmp_changer_device && json_is_string(ndmp_changer_device)) {
    spec.ndmp_changer_device
        = std::string{json_string_value(ndmp_changer_device)};
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
  auto* job_acl = json_object_get(root.get(), "job_acl");
  auto* client_acl = json_object_get(root.get(), "client_acl");
  auto* storage_acl = json_object_get(root.get(), "storage_acl");
  auto* schedule_acl = json_object_get(root.get(), "schedule_acl");
  auto* pool_acl = json_object_get(root.get(), "pool_acl");
  auto* command_acl = json_object_get(root.get(), "command_acl");
  auto* fileset_acl = json_object_get(root.get(), "fileset_acl");
  auto* catalog_acl = json_object_get(root.get(), "catalog_acl");
  auto* where_acl = json_object_get(root.get(), "where_acl");
  auto* plugin_options_acl = json_object_get(root.get(), "plugin_options_acl");
  auto* profiles = json_object_get(root.get(), "profiles");
  auto* use_pam_authentication
      = json_object_get(root.get(), "use_pam_authentication");
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
  auto require_string = [&error](json_t* value, const char* field) {
    if (value && !json_is_null(value) && !json_is_string(value)) {
      error = std::string{"field '"} + field
              + "' must be a string when provided.";
      return false;
    }
    return true;
  };
  auto require_bool = [&error](json_t* value, const char* field) {
    if (value && !json_is_null(value) && !json_is_boolean(value)) {
      error = std::string{"field '"} + field
              + "' must be a boolean when provided.";
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
  if (!require_string_array(job_acl, "job_acl")
      || !require_string_array(client_acl, "client_acl")
      || !require_string_array(storage_acl, "storage_acl")
      || !require_string_array(schedule_acl, "schedule_acl")
      || !require_string_array(pool_acl, "pool_acl")
      || !require_string_array(command_acl, "command_acl")
      || !require_string_array(fileset_acl, "fileset_acl")
      || !require_string_array(catalog_acl, "catalog_acl")
      || !require_string_array(where_acl, "where_acl")
      || !require_string_array(plugin_options_acl, "plugin_options_acl")
      || !require_string_array(profiles, "profiles")
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
      || !require_string_array(tls_allowed_cn, "tls_allowed_cn")) {
    return std::nullopt;
  }
  if (!require_bool(tls_authenticate, "tls_authenticate")
      || !require_bool(tls_enable, "tls_enable")
      || !require_bool(tls_require, "tls_require")
      || !require_bool(tls_verify_peer, "tls_verify_peer")) {
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
  spec.job_acl = parse_string_array(job_acl);
  spec.client_acl = parse_string_array(client_acl);
  spec.storage_acl = parse_string_array(storage_acl);
  spec.schedule_acl = parse_string_array(schedule_acl);
  spec.pool_acl = parse_string_array(pool_acl);
  spec.command_acl = parse_string_array(command_acl);
  spec.fileset_acl = parse_string_array(fileset_acl);
  spec.catalog_acl = parse_string_array(catalog_acl);
  spec.where_acl = parse_string_array(where_acl);
  spec.plugin_options_acl = parse_string_array(plugin_options_acl);
  spec.profiles = parse_string_array(profiles);
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
  auto* job_acl = json_object_get(root.get(), "job_acl");
  auto* client_acl = json_object_get(root.get(), "client_acl");
  auto* storage_acl = json_object_get(root.get(), "storage_acl");
  auto* schedule_acl = json_object_get(root.get(), "schedule_acl");
  auto* pool_acl = json_object_get(root.get(), "pool_acl");
  auto* command_acl = json_object_get(root.get(), "command_acl");
  auto* fileset_acl = json_object_get(root.get(), "fileset_acl");
  auto* catalog_acl = json_object_get(root.get(), "catalog_acl");
  auto* where_acl = json_object_get(root.get(), "where_acl");
  auto* plugin_options_acl = json_object_get(root.get(), "plugin_options_acl");
  auto* profiles = json_object_get(root.get(), "profiles");
  if (description && !json_is_null(description)
      && !json_is_string(description)) {
    error = "field 'description' must be a string when provided.";
    return std::nullopt;
  }
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
  if (!require_string_array(job_acl, "job_acl")
      || !require_string_array(client_acl, "client_acl")
      || !require_string_array(storage_acl, "storage_acl")
      || !require_string_array(schedule_acl, "schedule_acl")
      || !require_string_array(pool_acl, "pool_acl")
      || !require_string_array(command_acl, "command_acl")
      || !require_string_array(fileset_acl, "fileset_acl")
      || !require_string_array(catalog_acl, "catalog_acl")
      || !require_string_array(where_acl, "where_acl")
      || !require_string_array(plugin_options_acl, "plugin_options_acl")
      || !require_string_array(profiles, "profiles")) {
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

  DirectorUserRequestSpec spec{};
  if (description && json_is_string(description)) {
    spec.description = std::string{json_string_value(description)};
  }
  spec.job_acl = parse_string_array(job_acl);
  spec.client_acl = parse_string_array(client_acl);
  spec.storage_acl = parse_string_array(storage_acl);
  spec.schedule_acl = parse_string_array(schedule_acl);
  spec.pool_acl = parse_string_array(pool_acl);
  spec.command_acl = parse_string_array(command_acl);
  spec.fileset_acl = parse_string_array(fileset_acl);
  spec.catalog_acl = parse_string_array(catalog_acl);
  spec.where_acl = parse_string_array(where_acl);
  spec.plugin_options_acl = parse_string_array(plugin_options_acl);
  spec.profiles = parse_string_array(profiles);
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
  auto* job_acl = json_object_get(root.get(), "job_acl");
  auto* client_acl = json_object_get(root.get(), "client_acl");
  auto* storage_acl = json_object_get(root.get(), "storage_acl");
  auto* schedule_acl = json_object_get(root.get(), "schedule_acl");
  auto* pool_acl = json_object_get(root.get(), "pool_acl");
  auto* command_acl = json_object_get(root.get(), "command_acl");
  auto* fileset_acl = json_object_get(root.get(), "fileset_acl");
  auto* catalog_acl = json_object_get(root.get(), "catalog_acl");
  auto* where_acl = json_object_get(root.get(), "where_acl");
  auto* plugin_options_acl = json_object_get(root.get(), "plugin_options_acl");
  if (description && !json_is_null(description)
      && !json_is_string(description)) {
    error = "field 'description' must be a string when provided.";
    return std::nullopt;
  }
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
  if (!require_string_array(job_acl, "job_acl")
      || !require_string_array(client_acl, "client_acl")
      || !require_string_array(storage_acl, "storage_acl")
      || !require_string_array(schedule_acl, "schedule_acl")
      || !require_string_array(pool_acl, "pool_acl")
      || !require_string_array(command_acl, "command_acl")
      || !require_string_array(fileset_acl, "fileset_acl")
      || !require_string_array(catalog_acl, "catalog_acl")
      || !require_string_array(where_acl, "where_acl")
      || !require_string_array(plugin_options_acl, "plugin_options_acl")) {
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

  DirectorProfileRequestSpec spec{};
  if (description && json_is_string(description)) {
    spec.description = std::string{json_string_value(description)};
  }
  spec.job_acl = parse_string_array(job_acl);
  spec.client_acl = parse_string_array(client_acl);
  spec.storage_acl = parse_string_array(storage_acl);
  spec.schedule_acl = parse_string_array(schedule_acl);
  spec.pool_acl = parse_string_array(pool_acl);
  spec.command_acl = parse_string_array(command_acl);
  spec.fileset_acl = parse_string_array(fileset_acl);
  spec.catalog_acl = parse_string_array(catalog_acl);
  spec.where_acl = parse_string_array(where_acl);
  spec.plugin_options_acl = parse_string_array(plugin_options_acl);
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
  auto* cleaning_prefix = json_object_get(root.get(), "cleaning_prefix");
  auto* label_type = json_object_get(root.get(), "label_type");
  auto* maximum_volumes = json_object_get(root.get(), "maximum_volumes");
  auto* maximum_volume_jobs
      = json_object_get(root.get(), "maximum_volume_jobs");
  auto* maximum_volume_files
      = json_object_get(root.get(), "maximum_volume_files");
  auto* maximum_volume_bytes
      = json_object_get(root.get(), "maximum_volume_bytes");
  auto* volume_retention = json_object_get(root.get(), "volume_retention");
  auto* volume_use_duration
      = json_object_get(root.get(), "volume_use_duration");
  auto* migration_time = json_object_get(root.get(), "migration_time");
  auto* migration_high_bytes
      = json_object_get(root.get(), "migration_high_bytes");
  auto* migration_low_bytes
      = json_object_get(root.get(), "migration_low_bytes");
  auto* next_pool = json_object_get(root.get(), "next_pool");
  auto* storages = json_object_get(root.get(), "storages");
  auto* use_catalog = json_object_get(root.get(), "use_catalog");
  auto* catalog_files = json_object_get(root.get(), "catalog_files");
  auto* purge_oldest_volume
      = json_object_get(root.get(), "purge_oldest_volume");
  auto* action_on_purge = json_object_get(root.get(), "action_on_purge");
  auto* recycle_oldest_volume
      = json_object_get(root.get(), "recycle_oldest_volume");
  auto* recycle_current_volume
      = json_object_get(root.get(), "recycle_current_volume");
  auto* auto_prune = json_object_get(root.get(), "auto_prune");
  auto* recycle = json_object_get(root.get(), "recycle");
  auto* recycle_pool = json_object_get(root.get(), "recycle_pool");
  auto* scratch_pool = json_object_get(root.get(), "scratch_pool");
  auto* catalog = json_object_get(root.get(), "catalog");
  auto* file_retention = json_object_get(root.get(), "file_retention");
  auto* job_retention = json_object_get(root.get(), "job_retention");
  auto* minimum_block_size = json_object_get(root.get(), "minimum_block_size");
  auto* maximum_block_size = json_object_get(root.get(), "maximum_block_size");
  auto* description = json_object_get(root.get(), "description");

  auto require_string = [&error](json_t* value, const char* field) {
    if (value && !json_is_null(value) && !json_is_string(value)) {
      error = std::string{"field '"} + field
              + "' must be a string when provided.";
      return false;
    }
    return true;
  };
  auto require_bool = [&error](json_t* value, const char* field) {
    if (value && !json_is_null(value) && !json_is_boolean(value)) {
      error = std::string{"field '"} + field
              + "' must be a boolean when provided.";
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
  if (!require_string(pool_type, "pool_type")
      || !require_string(label_format, "label_format")
      || !require_string(cleaning_prefix, "cleaning_prefix")
      || !require_string(label_type, "label_type")
      || !require_integer(maximum_volumes, "maximum_volumes")
      || !require_integer(maximum_volume_jobs, "maximum_volume_jobs")
      || !require_integer(maximum_volume_files, "maximum_volume_files")
      || !require_integer(maximum_volume_bytes, "maximum_volume_bytes")
      || !require_integer(volume_retention, "volume_retention")
      || !require_integer(volume_use_duration, "volume_use_duration")
      || !require_integer(migration_time, "migration_time")
      || !require_integer(migration_high_bytes, "migration_high_bytes")
      || !require_integer(migration_low_bytes, "migration_low_bytes")
      || !require_string(next_pool, "next_pool")
      || !require_string_array(storages, "storages")
      || !require_bool(use_catalog, "use_catalog")
      || !require_bool(catalog_files, "catalog_files")
      || !require_bool(purge_oldest_volume, "purge_oldest_volume")
      || !require_string(action_on_purge, "action_on_purge")
      || !require_bool(recycle_oldest_volume, "recycle_oldest_volume")
      || !require_bool(recycle_current_volume, "recycle_current_volume")
      || !require_bool(auto_prune, "auto_prune")
      || !require_bool(recycle, "recycle")
      || !require_string(recycle_pool, "recycle_pool")
      || !require_string(scratch_pool, "scratch_pool")
      || !require_string(catalog, "catalog")
      || !require_integer(file_retention, "file_retention")
      || !require_integer(job_retention, "job_retention")
      || !require_integer(minimum_block_size, "minimum_block_size")
      || !require_integer(maximum_block_size, "maximum_block_size")
      || !require_string(description, "description")) {
    return std::nullopt;
  }

  DirectorPoolRequestSpec spec{};
  if (pool_type && json_is_string(pool_type)) {
    spec.pool_type = std::string{json_string_value(pool_type)};
  }
  if (label_format && json_is_string(label_format)) {
    spec.label_format = std::string{json_string_value(label_format)};
  }
  if (cleaning_prefix && json_is_string(cleaning_prefix)) {
    spec.cleaning_prefix = std::string{json_string_value(cleaning_prefix)};
  }
  if (label_type && json_is_string(label_type)) {
    spec.label_type = std::string{json_string_value(label_type)};
  }
  if (maximum_volumes && json_is_integer(maximum_volumes)) {
    const auto value = json_integer_value(maximum_volumes);
    if (value < 0 || value > std::numeric_limits<uint32_t>::max()) {
      error = "field 'maximum_volumes' must be between 0 and 4294967295.";
      return std::nullopt;
    }
    spec.maximum_volumes = static_cast<uint32_t>(value);
  }
  if (maximum_volume_jobs && json_is_integer(maximum_volume_jobs)) {
    const auto value = json_integer_value(maximum_volume_jobs);
    if (value < 0 || value > std::numeric_limits<uint32_t>::max()) {
      error = "field 'maximum_volume_jobs' must be between 0 and 4294967295.";
      return std::nullopt;
    }
    spec.maximum_volume_jobs = static_cast<uint32_t>(value);
  }
  if (maximum_volume_files && json_is_integer(maximum_volume_files)) {
    const auto value = json_integer_value(maximum_volume_files);
    if (value < 0 || value > std::numeric_limits<uint32_t>::max()) {
      error = "field 'maximum_volume_files' must be between 0 and 4294967295.";
      return std::nullopt;
    }
    spec.maximum_volume_files = static_cast<uint32_t>(value);
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
  auto parse_uint64 = [&error](json_t* value, const char* field,
                               std::optional<uint64_t>& target) {
    if (!value || !json_is_integer(value)) { return true; }
    const auto parsed = json_integer_value(value);
    if (parsed < 0) {
      error = std::string{"field '"} + field + "' must be non-negative.";
      return false;
    }
    target = static_cast<uint64_t>(parsed);
    return true;
  };
  auto parse_uint32 = [&error](json_t* value, const char* field,
                               std::optional<uint32_t>& target) {
    if (!value || !json_is_integer(value)) { return true; }
    const auto parsed = json_integer_value(value);
    if (parsed < 0 || parsed > std::numeric_limits<uint32_t>::max()) {
      error = std::string{"field '"} + field
              + "' must be between 0 and 4294967295.";
      return false;
    }
    target = static_cast<uint32_t>(parsed);
    return true;
  };
  if (!parse_uint64(volume_use_duration, "volume_use_duration",
                    spec.volume_use_duration)
      || !parse_uint64(migration_time, "migration_time", spec.migration_time)
      || !parse_uint64(migration_high_bytes, "migration_high_bytes",
                       spec.migration_high_bytes)
      || !parse_uint64(migration_low_bytes, "migration_low_bytes",
                       spec.migration_low_bytes)
      || !parse_uint64(file_retention, "file_retention", spec.file_retention)
      || !parse_uint64(job_retention, "job_retention", spec.job_retention)
      || !parse_uint32(minimum_block_size, "minimum_block_size",
                       spec.minimum_block_size)
      || !parse_uint32(maximum_block_size, "maximum_block_size",
                       spec.maximum_block_size)) {
    return std::nullopt;
  }
  if (next_pool && json_is_string(next_pool)) {
    spec.next_pool = std::string{json_string_value(next_pool)};
  }
  if (storages && json_is_array(storages)) {
    std::vector<std::string> values;
    values.reserve(json_array_size(storages));
    for (size_t index = 0; index < json_array_size(storages); ++index) {
      values.emplace_back(json_string_value(json_array_get(storages, index)));
    }
    spec.storages = std::move(values);
  }
  if (use_catalog && json_is_boolean(use_catalog)) {
    spec.use_catalog = json_is_true(use_catalog);
  }
  if (catalog_files && json_is_boolean(catalog_files)) {
    spec.catalog_files = json_is_true(catalog_files);
  }
  if (purge_oldest_volume && json_is_boolean(purge_oldest_volume)) {
    spec.purge_oldest_volume = json_is_true(purge_oldest_volume);
  }
  if (action_on_purge && json_is_string(action_on_purge)) {
    spec.action_on_purge = std::string{json_string_value(action_on_purge)};
  }
  if (recycle_oldest_volume && json_is_boolean(recycle_oldest_volume)) {
    spec.recycle_oldest_volume = json_is_true(recycle_oldest_volume);
  }
  if (recycle_current_volume && json_is_boolean(recycle_current_volume)) {
    spec.recycle_current_volume = json_is_true(recycle_current_volume);
  }
  if (auto_prune && json_is_boolean(auto_prune)) {
    spec.auto_prune = json_is_true(auto_prune);
  }
  if (recycle && json_is_boolean(recycle)) {
    spec.recycle = json_is_true(recycle);
  }
  if (recycle_pool && json_is_string(recycle_pool)) {
    spec.recycle_pool = std::string{json_string_value(recycle_pool)};
  }
  if (scratch_pool && json_is_string(scratch_pool)) {
    spec.scratch_pool = std::string{json_string_value(scratch_pool)};
  }
  if (catalog && json_is_string(catalog)) {
    spec.catalog = std::string{json_string_value(catalog)};
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
  auto* multiple_connections
      = json_object_get(root.get(), "multiple_connections");
  auto* disable_batch_insert
      = json_object_get(root.get(), "disable_batch_insert");
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
      || !require_boolean(multiple_connections, "multiple_connections")
      || !require_boolean(disable_batch_insert, "disable_batch_insert")
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
  if (multiple_connections && json_is_boolean(multiple_connections)) {
    spec.multiple_connections = json_is_true(multiple_connections);
  }
  if (disable_batch_insert && json_is_boolean(disable_batch_insert)) {
    spec.disable_batch_insert = json_is_true(disable_batch_insert);
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
  auto* mail_command = json_object_get(root.get(), "mail_command");
  auto* operator_command = json_object_get(root.get(), "operator_command");
  auto* timestamp_format = json_object_get(root.get(), "timestamp_format");
  auto* syslog_entries = json_object_get(root.get(), "syslog_entries");
  auto* mail_entries = json_object_get(root.get(), "mail_entries");
  auto* mail_on_error_entries
      = json_object_get(root.get(), "mail_on_error_entries");
  auto* mail_on_success_entries
      = json_object_get(root.get(), "mail_on_success_entries");
  auto* file_entries = json_object_get(root.get(), "file_entries");
  auto* append_entries = json_object_get(root.get(), "append_entries");
  auto* stdout_entries = json_object_get(root.get(), "stdout_entries");
  auto* stderr_entries = json_object_get(root.get(), "stderr_entries");
  auto* director_entries = json_object_get(root.get(), "director_entries");
  auto* console_entries = json_object_get(root.get(), "console_entries");
  auto* operator_entries = json_object_get(root.get(), "operator_entries");
  auto* catalog_entries = json_object_get(root.get(), "catalog_entries");
  auto* entries = json_object_get(root.get(), "entries");
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
  if (description && !json_is_null(description)
      && !json_is_string(description)) {
    error = "field 'description' must be a string when provided.";
    return std::nullopt;
  }
  if (mail_command && !json_is_null(mail_command)
      && !json_is_string(mail_command)) {
    error = "field 'mail_command' must be a string when provided.";
    return std::nullopt;
  }
  if (operator_command && !json_is_null(operator_command)
      && !json_is_string(operator_command)) {
    error = "field 'operator_command' must be a string when provided.";
    return std::nullopt;
  }
  if (timestamp_format && !json_is_null(timestamp_format)
      && !json_is_string(timestamp_format)) {
    error = "field 'timestamp_format' must be a string when provided.";
    return std::nullopt;
  }
  if (!require_string_array(syslog_entries, "syslog_entries")
      || !require_string_array(mail_entries, "mail_entries")
      || !require_string_array(mail_on_error_entries, "mail_on_error_entries")
      || !require_string_array(mail_on_success_entries,
                               "mail_on_success_entries")
      || !require_string_array(file_entries, "file_entries")
      || !require_string_array(append_entries, "append_entries")
      || !require_string_array(stdout_entries, "stdout_entries")
      || !require_string_array(stderr_entries, "stderr_entries")
      || !require_string_array(director_entries, "director_entries")
      || !require_string_array(console_entries, "console_entries")
      || !require_string_array(operator_entries, "operator_entries")
      || !require_string_array(catalog_entries, "catalog_entries")
      || !require_string_array(entries, "entries")) {
    return std::nullopt;
  }

  DirectorMessagesRequestSpec spec{};
  auto parse_string_array
      = [](json_t* value) -> std::optional<std::vector<std::string>> {
    if (!value || !json_is_array(value)) { return std::nullopt; }
    std::vector<std::string> parsed;
    parsed.reserve(json_array_size(value));
    for (size_t index = 0; index < json_array_size(value); ++index) {
      parsed.emplace_back(json_string_value(json_array_get(value, index)));
    }
    return parsed;
  };
  if (description && json_is_string(description)) {
    spec.description = std::string{json_string_value(description)};
  }
  if (mail_command && json_is_string(mail_command)) {
    spec.mail_command = std::string{json_string_value(mail_command)};
  }
  if (operator_command && json_is_string(operator_command)) {
    spec.operator_command = std::string{json_string_value(operator_command)};
  }
  if (timestamp_format && json_is_string(timestamp_format)) {
    spec.timestamp_format = std::string{json_string_value(timestamp_format)};
  }
  spec.syslog_entries = parse_string_array(syslog_entries);
  spec.mail_entries = parse_string_array(mail_entries);
  spec.mail_on_error_entries = parse_string_array(mail_on_error_entries);
  spec.mail_on_success_entries = parse_string_array(mail_on_success_entries);
  spec.file_entries = parse_string_array(file_entries);
  spec.append_entries = parse_string_array(append_entries);
  spec.stdout_entries = parse_string_array(stdout_entries);
  spec.stderr_entries = parse_string_array(stderr_entries);
  spec.director_entries = parse_string_array(director_entries);
  spec.console_entries = parse_string_array(console_entries);
  spec.operator_entries = parse_string_array(operator_entries);
  spec.catalog_entries = parse_string_array(catalog_entries);
  spec.entries = parse_string_array(entries);
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
  auto* key_encryption_key = json_object_get(root.get(), "key_encryption_key");
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
  auto require_string = [&error](json_t* value, const char* field) {
    if (value && !json_is_null(value) && !json_is_string(value)) {
      error = std::string{"field '"} + field
              + "' must be a string when provided.";
      return false;
    }
    return true;
  };
  auto require_bool = [&error](json_t* value, const char* field) {
    if (value && !json_is_null(value) && !json_is_boolean(value)) {
      error = std::string{"field '"} + field
              + "' must be a boolean when provided.";
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
  if (!require_string(key_encryption_key, "key_encryption_key")
      || !require_bool(tls_authenticate, "tls_authenticate")
      || !require_bool(tls_enable, "tls_enable")
      || !require_bool(tls_require, "tls_require")
      || !require_bool(tls_verify_peer, "tls_verify_peer")
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
      || !require_string_array(tls_allowed_cn, "tls_allowed_cn")) {
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
  if (key_encryption_key && json_is_string(key_encryption_key)) {
    spec.key_encryption_key
        = std::string{json_string_value(key_encryption_key)};
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
  auto* access_mode = json_object_get(root.get(), "access_mode");
  auto* device_options = json_object_get(root.get(), "device_options");
  auto* diagnostic_device = json_object_get(root.get(), "diagnostic_device");
  auto* hardware_end_of_file
      = json_object_get(root.get(), "hardware_end_of_file");
  auto* hardware_end_of_medium
      = json_object_get(root.get(), "hardware_end_of_medium");
  auto* backward_space_record
      = json_object_get(root.get(), "backward_space_record");
  auto* backward_space_file
      = json_object_get(root.get(), "backward_space_file");
  auto* bsf_at_eom = json_object_get(root.get(), "bsf_at_eom");
  auto* two_eof = json_object_get(root.get(), "two_eof");
  auto* forward_space_record
      = json_object_get(root.get(), "forward_space_record");
  auto* forward_space_file = json_object_get(root.get(), "forward_space_file");
  auto* fast_forward_space_file
      = json_object_get(root.get(), "fast_forward_space_file");
  auto* removable_media = json_object_get(root.get(), "removable_media");
  auto* random_access = json_object_get(root.get(), "random_access");
  auto* automatic_mount = json_object_get(root.get(), "automatic_mount");
  auto* label_media = json_object_get(root.get(), "label_media");
  auto* always_open = json_object_get(root.get(), "always_open");
  auto* autochanger = json_object_get(root.get(), "autochanger");
  auto* close_on_poll = json_object_get(root.get(), "close_on_poll");
  auto* block_positioning = json_object_get(root.get(), "block_positioning");
  auto* use_mtiocget = json_object_get(root.get(), "use_mtiocget");
  auto* check_labels = json_object_get(root.get(), "check_labels");
  auto* requires_mount = json_object_get(root.get(), "requires_mount");
  auto* offline_on_unmount = json_object_get(root.get(), "offline_on_unmount");
  auto* block_checksum = json_object_get(root.get(), "block_checksum");
  auto* auto_select = json_object_get(root.get(), "auto_select");
  auto* changer_device = json_object_get(root.get(), "changer_device");
  auto* changer_command = json_object_get(root.get(), "changer_command");
  auto* alert_command = json_object_get(root.get(), "alert_command");
  auto* maximum_changer_wait
      = json_object_get(root.get(), "maximum_changer_wait");
  auto* maximum_open_wait = json_object_get(root.get(), "maximum_open_wait");
  auto* maximum_open_volumes
      = json_object_get(root.get(), "maximum_open_volumes");
  auto* maximum_network_buffer_size
      = json_object_get(root.get(), "maximum_network_buffer_size");
  auto* volume_poll_interval
      = json_object_get(root.get(), "volume_poll_interval");
  auto* maximum_rewind_wait
      = json_object_get(root.get(), "maximum_rewind_wait");
  auto* label_block_size = json_object_get(root.get(), "label_block_size");
  auto* minimum_block_size = json_object_get(root.get(), "minimum_block_size");
  auto* maximum_block_size = json_object_get(root.get(), "maximum_block_size");
  auto* maximum_file_size = json_object_get(root.get(), "maximum_file_size");
  auto* volume_capacity = json_object_get(root.get(), "volume_capacity");
  auto* maximum_concurrent_jobs
      = json_object_get(root.get(), "maximum_concurrent_jobs");
  auto* spool_directory = json_object_get(root.get(), "spool_directory");
  auto* maximum_spool_size = json_object_get(root.get(), "maximum_spool_size");
  auto* maximum_job_spool_size
      = json_object_get(root.get(), "maximum_job_spool_size");
  auto* drive_index = json_object_get(root.get(), "drive_index");
  auto* mount_point = json_object_get(root.get(), "mount_point");
  auto* mount_command = json_object_get(root.get(), "mount_command");
  auto* unmount_command = json_object_get(root.get(), "unmount_command");
  auto* label_type = json_object_get(root.get(), "label_type");
  auto* no_rewind_on_close = json_object_get(root.get(), "no_rewind_on_close");
  auto* drive_tape_alert_enabled
      = json_object_get(root.get(), "drive_tape_alert_enabled");
  auto* drive_crypto_enabled
      = json_object_get(root.get(), "drive_crypto_enabled");
  auto* query_crypto_status
      = json_object_get(root.get(), "query_crypto_status");
  auto* auto_deflate = json_object_get(root.get(), "auto_deflate");
  auto* auto_deflate_algorithm
      = json_object_get(root.get(), "auto_deflate_algorithm");
  auto* auto_deflate_level = json_object_get(root.get(), "auto_deflate_level");
  auto* auto_inflate = json_object_get(root.get(), "auto_inflate");
  auto* collect_statistics = json_object_get(root.get(), "collect_statistics");
  auto* eof_on_error_is_eot
      = json_object_get(root.get(), "eof_on_error_is_eot");
  auto* count = json_object_get(root.get(), "count");
  auto* description = json_object_get(root.get(), "description");
  auto require_string = [&error](json_t* value, const char* field) {
    if (value && !json_is_null(value) && !json_is_string(value)) {
      error = std::string{"field '"} + field
              + "' must be a string when provided.";
      return false;
    }
    return true;
  };
  auto require_bool = [&error](json_t* value, const char* field) {
    if (value && !json_is_null(value) && !json_is_boolean(value)) {
      error = std::string{"field '"} + field
              + "' must be a boolean when provided.";
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
  if (!require_string(media_type, "media_type")
      || !require_string(archive_device, "archive_device")
      || !require_string(device_type, "device_type")
      || !require_string(access_mode, "access_mode")
      || !require_string(device_options, "device_options")
      || !require_string(diagnostic_device, "diagnostic_device")
      || !require_string(changer_device, "changer_device")
      || !require_string(changer_command, "changer_command")
      || !require_string(alert_command, "alert_command")
      || !require_string(spool_directory, "spool_directory")
      || !require_string(mount_point, "mount_point")
      || !require_string(mount_command, "mount_command")
      || !require_string(unmount_command, "unmount_command")
      || !require_string(label_type, "label_type")
      || !require_string(auto_deflate, "auto_deflate")
      || !require_string(auto_deflate_algorithm, "auto_deflate_algorithm")
      || !require_string(auto_inflate, "auto_inflate")
      || !require_string(description, "description")) {
    return std::nullopt;
  }
  if (!require_bool(hardware_end_of_file, "hardware_end_of_file")
      || !require_bool(hardware_end_of_medium, "hardware_end_of_medium")
      || !require_bool(backward_space_record, "backward_space_record")
      || !require_bool(backward_space_file, "backward_space_file")
      || !require_bool(bsf_at_eom, "bsf_at_eom")
      || !require_bool(two_eof, "two_eof")
      || !require_bool(forward_space_record, "forward_space_record")
      || !require_bool(forward_space_file, "forward_space_file")
      || !require_bool(fast_forward_space_file, "fast_forward_space_file")
      || !require_bool(removable_media, "removable_media")
      || !require_bool(random_access, "random_access")
      || !require_bool(automatic_mount, "automatic_mount")
      || !require_bool(label_media, "label_media")
      || !require_bool(always_open, "always_open")
      || !require_bool(autochanger, "autochanger")
      || !require_bool(close_on_poll, "close_on_poll")
      || !require_bool(block_positioning, "block_positioning")
      || !require_bool(use_mtiocget, "use_mtiocget")
      || !require_bool(check_labels, "check_labels")
      || !require_bool(requires_mount, "requires_mount")
      || !require_bool(offline_on_unmount, "offline_on_unmount")
      || !require_bool(block_checksum, "block_checksum")
      || !require_bool(auto_select, "auto_select")
      || !require_bool(no_rewind_on_close, "no_rewind_on_close")
      || !require_bool(drive_tape_alert_enabled, "drive_tape_alert_enabled")
      || !require_bool(drive_crypto_enabled, "drive_crypto_enabled")
      || !require_bool(query_crypto_status, "query_crypto_status")
      || !require_bool(collect_statistics, "collect_statistics")
      || !require_bool(eof_on_error_is_eot, "eof_on_error_is_eot")
      || !require_integer(maximum_changer_wait, "maximum_changer_wait")
      || !require_integer(maximum_open_wait, "maximum_open_wait")
      || !require_integer(maximum_open_volumes, "maximum_open_volumes")
      || !require_integer(maximum_network_buffer_size,
                          "maximum_network_buffer_size")
      || !require_integer(volume_poll_interval, "volume_poll_interval")
      || !require_integer(maximum_rewind_wait, "maximum_rewind_wait")
      || !require_integer(label_block_size, "label_block_size")
      || !require_integer(minimum_block_size, "minimum_block_size")
      || !require_integer(maximum_block_size, "maximum_block_size")
      || !require_integer(maximum_file_size, "maximum_file_size")
      || !require_integer(volume_capacity, "volume_capacity")
      || !require_integer(maximum_concurrent_jobs, "maximum_concurrent_jobs")
      || !require_integer(maximum_spool_size, "maximum_spool_size")
      || !require_integer(maximum_job_spool_size, "maximum_job_spool_size")
      || !require_integer(drive_index, "drive_index")
      || !require_integer(auto_deflate_level, "auto_deflate_level")
      || !require_integer(count, "count")) {
    return std::nullopt;
  }

  StorageDeviceRequestSpec spec{};
  auto assign_u64 = [&error](json_t* value, const char* field,
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
  auto assign_u32 = [&error](json_t* value, const char* field,
                             std::optional<uint32_t>& target) -> bool {
    if (!value || !json_is_integer(value)) { return true; }
    const auto raw = json_integer_value(value);
    if (raw < 0 || raw > std::numeric_limits<uint32_t>::max()) {
      error = std::string{"field '"} + field
              + "' must fit in an unsigned 32-bit integer.";
      return false;
    }
    target = static_cast<uint32_t>(raw);
    return true;
  };
  auto assign_u16 = [&error](json_t* value, const char* field,
                             std::optional<uint16_t>& target) -> bool {
    if (!value || !json_is_integer(value)) { return true; }
    const auto raw = json_integer_value(value);
    if (raw < 0 || raw > std::numeric_limits<uint16_t>::max()) {
      error = std::string{"field '"} + field
              + "' must fit in an unsigned 16-bit integer.";
      return false;
    }
    target = static_cast<uint16_t>(raw);
    return true;
  };
  if (media_type && json_is_string(media_type)) {
    spec.media_type = std::string{json_string_value(media_type)};
  }
  if (archive_device && json_is_string(archive_device)) {
    spec.archive_device = std::string{json_string_value(archive_device)};
  }
  if (device_type && json_is_string(device_type)) {
    spec.device_type = std::string{json_string_value(device_type)};
  }
  if (access_mode && json_is_string(access_mode)) {
    spec.access_mode = std::string{json_string_value(access_mode)};
  }
  if (device_options && json_is_string(device_options)) {
    spec.device_options = std::string{json_string_value(device_options)};
  }
  if (diagnostic_device && json_is_string(diagnostic_device)) {
    spec.diagnostic_device = std::string{json_string_value(diagnostic_device)};
  }
  if (hardware_end_of_file && json_is_boolean(hardware_end_of_file)) {
    spec.hardware_end_of_file = json_is_true(hardware_end_of_file);
  }
  if (hardware_end_of_medium && json_is_boolean(hardware_end_of_medium)) {
    spec.hardware_end_of_medium = json_is_true(hardware_end_of_medium);
  }
  if (backward_space_record && json_is_boolean(backward_space_record)) {
    spec.backward_space_record = json_is_true(backward_space_record);
  }
  if (backward_space_file && json_is_boolean(backward_space_file)) {
    spec.backward_space_file = json_is_true(backward_space_file);
  }
  if (bsf_at_eom && json_is_boolean(bsf_at_eom)) {
    spec.bsf_at_eom = json_is_true(bsf_at_eom);
  }
  if (two_eof && json_is_boolean(two_eof)) {
    spec.two_eof = json_is_true(two_eof);
  }
  if (forward_space_record && json_is_boolean(forward_space_record)) {
    spec.forward_space_record = json_is_true(forward_space_record);
  }
  if (forward_space_file && json_is_boolean(forward_space_file)) {
    spec.forward_space_file = json_is_true(forward_space_file);
  }
  if (fast_forward_space_file && json_is_boolean(fast_forward_space_file)) {
    spec.fast_forward_space_file = json_is_true(fast_forward_space_file);
  }
  if (removable_media && json_is_boolean(removable_media)) {
    spec.removable_media = json_is_true(removable_media);
  }
  if (random_access && json_is_boolean(random_access)) {
    spec.random_access = json_is_true(random_access);
  }
  if (automatic_mount && json_is_boolean(automatic_mount)) {
    spec.automatic_mount = json_is_true(automatic_mount);
  }
  if (label_media && json_is_boolean(label_media)) {
    spec.label_media = json_is_true(label_media);
  }
  if (always_open && json_is_boolean(always_open)) {
    spec.always_open = json_is_true(always_open);
  }
  if (autochanger && json_is_boolean(autochanger)) {
    spec.autochanger = json_is_true(autochanger);
  }
  if (close_on_poll && json_is_boolean(close_on_poll)) {
    spec.close_on_poll = json_is_true(close_on_poll);
  }
  if (block_positioning && json_is_boolean(block_positioning)) {
    spec.block_positioning = json_is_true(block_positioning);
  }
  if (use_mtiocget && json_is_boolean(use_mtiocget)) {
    spec.use_mtiocget = json_is_true(use_mtiocget);
  }
  if (check_labels && json_is_boolean(check_labels)) {
    spec.check_labels = json_is_true(check_labels);
  }
  if (requires_mount && json_is_boolean(requires_mount)) {
    spec.requires_mount = json_is_true(requires_mount);
  }
  if (offline_on_unmount && json_is_boolean(offline_on_unmount)) {
    spec.offline_on_unmount = json_is_true(offline_on_unmount);
  }
  if (block_checksum && json_is_boolean(block_checksum)) {
    spec.block_checksum = json_is_true(block_checksum);
  }
  if (auto_select && json_is_boolean(auto_select)) {
    spec.auto_select = json_is_true(auto_select);
  }
  if (changer_device && json_is_string(changer_device)) {
    spec.changer_device = std::string{json_string_value(changer_device)};
  }
  if (changer_command && json_is_string(changer_command)) {
    spec.changer_command = std::string{json_string_value(changer_command)};
  }
  if (alert_command && json_is_string(alert_command)) {
    spec.alert_command = std::string{json_string_value(alert_command)};
  }
  if (!assign_u64(maximum_changer_wait, "maximum_changer_wait",
                  spec.maximum_changer_wait)
      || !assign_u64(maximum_open_wait, "maximum_open_wait",
                     spec.maximum_open_wait)
      || !assign_u32(maximum_open_volumes, "maximum_open_volumes",
                     spec.maximum_open_volumes)
      || !assign_u32(maximum_network_buffer_size, "maximum_network_buffer_size",
                     spec.maximum_network_buffer_size)
      || !assign_u64(volume_poll_interval, "volume_poll_interval",
                     spec.volume_poll_interval)
      || !assign_u64(maximum_rewind_wait, "maximum_rewind_wait",
                     spec.maximum_rewind_wait)
      || !assign_u32(label_block_size, "label_block_size",
                     spec.label_block_size)
      || !assign_u32(minimum_block_size, "minimum_block_size",
                     spec.minimum_block_size)
      || !assign_u32(maximum_block_size, "maximum_block_size",
                     spec.maximum_block_size)
      || !assign_u64(maximum_file_size, "maximum_file_size",
                     spec.maximum_file_size)
      || !assign_u64(volume_capacity, "volume_capacity", spec.volume_capacity)
      || !assign_u32(maximum_concurrent_jobs, "maximum_concurrent_jobs",
                     spec.maximum_concurrent_jobs)
      || !assign_u64(maximum_spool_size, "maximum_spool_size",
                     spec.maximum_spool_size)
      || !assign_u64(maximum_job_spool_size, "maximum_job_spool_size",
                     spec.maximum_job_spool_size)
      || !assign_u16(drive_index, "drive_index", spec.drive_index)
      || !assign_u32(count, "count", spec.count)) {
    return std::nullopt;
  }
  if (spool_directory && json_is_string(spool_directory)) {
    spec.spool_directory = std::string{json_string_value(spool_directory)};
  }
  if (mount_point && json_is_string(mount_point)) {
    spec.mount_point = std::string{json_string_value(mount_point)};
  }
  if (mount_command && json_is_string(mount_command)) {
    spec.mount_command = std::string{json_string_value(mount_command)};
  }
  if (unmount_command && json_is_string(unmount_command)) {
    spec.unmount_command = std::string{json_string_value(unmount_command)};
  }
  if (label_type && json_is_string(label_type)) {
    spec.label_type = std::string{json_string_value(label_type)};
  }
  if (no_rewind_on_close && json_is_boolean(no_rewind_on_close)) {
    spec.no_rewind_on_close = json_is_true(no_rewind_on_close);
  }
  if (drive_tape_alert_enabled && json_is_boolean(drive_tape_alert_enabled)) {
    spec.drive_tape_alert_enabled = json_is_true(drive_tape_alert_enabled);
  }
  if (drive_crypto_enabled && json_is_boolean(drive_crypto_enabled)) {
    spec.drive_crypto_enabled = json_is_true(drive_crypto_enabled);
  }
  if (query_crypto_status && json_is_boolean(query_crypto_status)) {
    spec.query_crypto_status = json_is_true(query_crypto_status);
  }
  if (auto_deflate && json_is_string(auto_deflate)) {
    spec.auto_deflate = std::string{json_string_value(auto_deflate)};
  }
  if (auto_deflate_algorithm && json_is_string(auto_deflate_algorithm)) {
    spec.auto_deflate_algorithm
        = std::string{json_string_value(auto_deflate_algorithm)};
  }
  if (!assign_u16(auto_deflate_level, "auto_deflate_level",
                  spec.auto_deflate_level)) {
    return std::nullopt;
  }
  if (auto_inflate && json_is_string(auto_inflate)) {
    spec.auto_inflate = std::string{json_string_value(auto_inflate)};
  }
  if (collect_statistics && json_is_boolean(collect_statistics)) {
    spec.collect_statistics = json_is_true(collect_statistics);
  }
  if (eof_on_error_is_eot && json_is_boolean(eof_on_error_is_eot)) {
    spec.eof_on_error_is_eot = json_is_true(eof_on_error_is_eot);
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
  auto* description = json_object_get(root.get(), "description");
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
      || !require_string(auth_type, "auth_type")
      || !require_string(description, "description")) {
    return std::nullopt;
  }
  if (log_level && !json_is_null(log_level) && !json_is_integer(log_level)) {
    error = "field 'log_level' must be an integer when provided.";
    return std::nullopt;
  }

  StorageNdmpRequestSpec spec{};
  if (description && json_is_string(description)) {
    spec.description = std::string{json_string_value(description)};
  }
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
  auto* query_file = json_object_get(root.get(), "query_file");
  auto* subscriptions = json_object_get(root.get(), "subscriptions");
  auto* just_in_time_reservation
      = json_object_get(root.get(), "just_in_time_reservation");
  auto* maximum_concurrent_jobs
      = json_object_get(root.get(), "maximum_concurrent_jobs");
  auto* maximum_workers_per_job
      = json_object_get(root.get(), "maximum_workers_per_job");
  auto* maximum_console_connections
      = json_object_get(root.get(), "maximum_console_connections");
  auto* password = json_object_get(root.get(), "password");
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
  auto* statistics_retention
      = json_object_get(root.get(), "statistics_retention");
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
  auto* key_encryption_key = json_object_get(root.get(), "key_encryption_key");
  auto* ndmp_namelist_fhinfo_set_zero_for_invalid_uquad = json_object_get(
      root.get(), "ndmp_namelist_fhinfo_set_zero_for_invalid_uquad");
  auto* auditing = json_object_get(root.get(), "auditing");
  auto* audit_events = json_object_get(root.get(), "audit_events");
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
      || !require_string(query_file, "query_file")
      || !require_string(source_address, "source_address")
      || !require_string_array(source_addresses, "source_addresses")
      || (port && !json_is_null(port) && !json_is_integer(port))
      || (subscriptions && !json_is_null(subscriptions)
          && !json_is_integer(subscriptions))
      || (maximum_concurrent_jobs && !json_is_null(maximum_concurrent_jobs)
          && !json_is_integer(maximum_concurrent_jobs))
      || (maximum_workers_per_job && !json_is_null(maximum_workers_per_job)
          && !json_is_integer(maximum_workers_per_job))
      || (maximum_console_connections
          && !json_is_null(maximum_console_connections)
          && !json_is_integer(maximum_console_connections))
      || !require_string(password, "password")
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
      || (statistics_retention && !json_is_null(statistics_retention)
          && !json_is_integer(statistics_retention))
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
      || (ndmp_namelist_fhinfo_set_zero_for_invalid_uquad
          && !json_is_null(ndmp_namelist_fhinfo_set_zero_for_invalid_uquad)
          && !json_is_boolean(ndmp_namelist_fhinfo_set_zero_for_invalid_uquad))
      || (auditing && !json_is_null(auditing) && !json_is_boolean(auditing))
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
      || !require_string(key_encryption_key, "key_encryption_key")
      || !require_string_array(audit_events, "audit_events")
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
    } else if (subscriptions && !json_is_null(subscriptions)
               && !json_is_integer(subscriptions)) {
      error = "field 'subscriptions' must be an integer when provided.";
    } else if (maximum_concurrent_jobs && !json_is_null(maximum_concurrent_jobs)
               && !json_is_integer(maximum_concurrent_jobs)) {
      error
          = "field 'maximum_concurrent_jobs' must be an integer when provided.";
    } else if (maximum_workers_per_job && !json_is_null(maximum_workers_per_job)
               && !json_is_integer(maximum_workers_per_job)) {
      error
          = "field 'maximum_workers_per_job' must be an integer when provided.";
    } else if (maximum_console_connections
               && !json_is_null(maximum_console_connections)
               && !json_is_integer(maximum_console_connections)) {
      error
          = "field 'maximum_console_connections' must be an integer when "
            "provided.";
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
    } else if (statistics_retention && !json_is_null(statistics_retention)
               && !json_is_integer(statistics_retention)) {
      error = "field 'statistics_retention' must be an integer when provided.";
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
    } else if (ndmp_namelist_fhinfo_set_zero_for_invalid_uquad
               && !json_is_null(ndmp_namelist_fhinfo_set_zero_for_invalid_uquad)
               && !json_is_boolean(
                   ndmp_namelist_fhinfo_set_zero_for_invalid_uquad)) {
      error
          = "field 'ndmp_namelist_fhinfo_set_zero_for_invalid_uquad' must be a "
            "boolean when provided.";
    } else if (auditing && !json_is_null(auditing)
               && !json_is_boolean(auditing)) {
      error = "field 'auditing' must be a boolean when provided.";
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
  if (query_file && json_is_string(query_file)) {
    spec.query_file = std::string{json_string_value(query_file)};
  }
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
  if (!parse_u32(subscriptions, "subscriptions", spec.subscriptions)
      || !parse_u32(maximum_concurrent_jobs, "maximum_concurrent_jobs",
                    spec.maximum_concurrent_jobs)
      || !parse_u32(maximum_workers_per_job, "maximum_workers_per_job",
                    spec.maximum_workers_per_job)
      || !parse_u32(maximum_console_connections, "maximum_console_connections",
                    spec.maximum_console_connections)
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
  if (password && json_is_string(password)) {
    spec.password = std::string{json_string_value(password)};
  }
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
  parse_bool(ndmp_namelist_fhinfo_set_zero_for_invalid_uquad,
             spec.ndmp_namelist_fhinfo_set_zero_for_invalid_uquad);
  parse_bool(auditing, spec.auditing);
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
      || !parse_non_negative_u64(statistics_retention, "statistics_retention",
                                 spec.statistics_retention)
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
  if (key_encryption_key && json_is_string(key_encryption_key)) {
    spec.key_encryption_key
        = std::string{json_string_value(key_encryption_key)};
  }
  spec.audit_events = parse_string_array(audit_events);
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
  auto* backup_format = json_object_get(root.get(), "backup_format");
  auto* protocol = json_object_get(root.get(), "protocol");
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
  auto* run_entries = json_object_get(root.get(), "run_entries");
  auto* run_before_job_entries
      = json_object_get(root.get(), "run_before_job_entries");
  auto* run_after_job_entries
      = json_object_get(root.get(), "run_after_job_entries");
  auto* run_after_failed_job_entries
      = json_object_get(root.get(), "run_after_failed_job_entries");
  auto* client_run_before_job_entries
      = json_object_get(root.get(), "client_run_before_job_entries");
  auto* client_run_after_job_entries
      = json_object_get(root.get(), "client_run_after_job_entries");
  auto* runscript_blocks = json_object_get(root.get(), "runscript_blocks");
  auto* where = json_object_get(root.get(), "where");
  auto* replace = json_object_get(root.get(), "replace");
  auto* regex_where = json_object_get(root.get(), "regex_where");
  auto* strip_prefix = json_object_get(root.get(), "strip_prefix");
  auto* add_prefix = json_object_get(root.get(), "add_prefix");
  auto* add_suffix = json_object_get(root.get(), "add_suffix");
  auto* bootstrap = json_object_get(root.get(), "bootstrap");
  auto* write_bootstrap = json_object_get(root.get(), "write_bootstrap");
  auto* write_verify_list = json_object_get(root.get(), "write_verify_list");
  auto* maximum_bandwidth = json_object_get(root.get(), "maximum_bandwidth");
  auto* max_run_sched_time = json_object_get(root.get(), "max_run_sched_time");
  auto* max_run_time = json_object_get(root.get(), "max_run_time");
  auto* full_max_runtime = json_object_get(root.get(), "full_max_runtime");
  auto* incremental_max_runtime
      = json_object_get(root.get(), "incremental_max_runtime");
  auto* differential_max_runtime
      = json_object_get(root.get(), "differential_max_runtime");
  auto* max_wait_time = json_object_get(root.get(), "max_wait_time");
  auto* max_start_delay = json_object_get(root.get(), "max_start_delay");
  auto* max_full_interval = json_object_get(root.get(), "max_full_interval");
  auto* max_virtual_full_interval
      = json_object_get(root.get(), "max_virtual_full_interval");
  auto* max_diff_interval = json_object_get(root.get(), "max_diff_interval");
  auto* prefix_links = json_object_get(root.get(), "prefix_links");
  auto* prune_jobs = json_object_get(root.get(), "prune_jobs");
  auto* prune_files = json_object_get(root.get(), "prune_files");
  auto* prune_volumes = json_object_get(root.get(), "prune_volumes");
  auto* purge_migration_job
      = json_object_get(root.get(), "purge_migration_job");
  auto* spool_attributes = json_object_get(root.get(), "spool_attributes");
  auto* spool_data = json_object_get(root.get(), "spool_data");
  auto* spool_size = json_object_get(root.get(), "spool_size");
  auto* rerun_failed_levels
      = json_object_get(root.get(), "rerun_failed_levels");
  auto* prefer_mounted_volumes
      = json_object_get(root.get(), "prefer_mounted_volumes");
  auto* maximum_concurrent_jobs
      = json_object_get(root.get(), "maximum_concurrent_jobs");
  auto* reschedule_on_error
      = json_object_get(root.get(), "reschedule_on_error");
  auto* reschedule_interval
      = json_object_get(root.get(), "reschedule_interval");
  auto* reschedule_times = json_object_get(root.get(), "reschedule_times");
  auto* priority = json_object_get(root.get(), "priority");
  auto* allow_mixed_priority
      = json_object_get(root.get(), "allow_mixed_priority");
  auto* selection_type = json_object_get(root.get(), "selection_type");
  auto* selection_pattern = json_object_get(root.get(), "selection_pattern");
  auto* accurate = json_object_get(root.get(), "accurate");
  auto* allow_duplicate_jobs
      = json_object_get(root.get(), "allow_duplicate_jobs");
  auto* allow_higher_duplicates
      = json_object_get(root.get(), "allow_higher_duplicates");
  auto* cancel_lower_level_duplicates
      = json_object_get(root.get(), "cancel_lower_level_duplicates");
  auto* cancel_queued_duplicates
      = json_object_get(root.get(), "cancel_queued_duplicates");
  auto* cancel_running_duplicates
      = json_object_get(root.get(), "cancel_running_duplicates");
  auto* save_file_history = json_object_get(root.get(), "save_file_history");
  auto* file_history_size = json_object_get(root.get(), "file_history_size");
  auto* fd_plugin_options = json_object_get(root.get(), "fd_plugin_options");
  auto* sd_plugin_options = json_object_get(root.get(), "sd_plugin_options");
  auto* dir_plugin_options = json_object_get(root.get(), "dir_plugin_options");
  auto* max_concurrent_copies
      = json_object_get(root.get(), "max_concurrent_copies");
  auto* always_incremental = json_object_get(root.get(), "always_incremental");
  auto* always_incremental_job_retention
      = json_object_get(root.get(), "always_incremental_job_retention");
  auto* always_incremental_keep_number
      = json_object_get(root.get(), "always_incremental_keep_number");
  auto* always_incremental_max_full_age
      = json_object_get(root.get(), "always_incremental_max_full_age");
  auto* max_full_consolidations
      = json_object_get(root.get(), "max_full_consolidations");
  auto* run_on_incoming_connect_interval
      = json_object_get(root.get(), "run_on_incoming_connect_interval");
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
      || !require_string(type, "type")
      || !require_string(backup_format, "backup_format")
      || !require_string(protocol, "protocol")
      || !require_string(level, "level")
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
      || !require_string(jobdefs, "jobdefs")
      || !require_string_array(run_entries, "run_entries")
      || !require_string_array(run_before_job_entries, "run_before_job_entries")
      || !require_string_array(run_after_job_entries, "run_after_job_entries")
      || !require_string_array(run_after_failed_job_entries,
                               "run_after_failed_job_entries")
      || !require_string_array(client_run_before_job_entries,
                               "client_run_before_job_entries")
      || !require_string_array(client_run_after_job_entries,
                               "client_run_after_job_entries")
      || !require_string_array(runscript_blocks, "runscript_blocks")
      || !require_string(where, "where") || !require_string(replace, "replace")
      || !require_string(regex_where, "regex_where")
      || !require_string(strip_prefix, "strip_prefix")
      || !require_string(add_prefix, "add_prefix")
      || !require_string(add_suffix, "add_suffix")
      || !require_string(bootstrap, "bootstrap")
      || !require_string(write_bootstrap, "write_bootstrap")
      || !require_string(write_verify_list, "write_verify_list")
      || !require_integer(maximum_bandwidth, "maximum_bandwidth")
      || !require_integer(max_run_sched_time, "max_run_sched_time")
      || !require_integer(max_run_time, "max_run_time")
      || !require_integer(full_max_runtime, "full_max_runtime")
      || !require_integer(incremental_max_runtime, "incremental_max_runtime")
      || !require_integer(differential_max_runtime, "differential_max_runtime")
      || !require_integer(max_wait_time, "max_wait_time")
      || !require_integer(max_start_delay, "max_start_delay")
      || !require_integer(max_full_interval, "max_full_interval")
      || !require_integer(max_virtual_full_interval,
                          "max_virtual_full_interval")
      || !require_integer(max_diff_interval, "max_diff_interval")
      || !require_boolean(prefix_links, "prefix_links")
      || !require_boolean(prune_jobs, "prune_jobs")
      || !require_boolean(prune_files, "prune_files")
      || !require_boolean(prune_volumes, "prune_volumes")
      || !require_boolean(purge_migration_job, "purge_migration_job")
      || !require_boolean(spool_attributes, "spool_attributes")
      || !require_boolean(spool_data, "spool_data")
      || !require_integer(spool_size, "spool_size")
      || !require_boolean(rerun_failed_levels, "rerun_failed_levels")
      || !require_boolean(prefer_mounted_volumes, "prefer_mounted_volumes")
      || !require_integer(maximum_concurrent_jobs, "maximum_concurrent_jobs")
      || !require_boolean(reschedule_on_error, "reschedule_on_error")
      || !require_integer(reschedule_interval, "reschedule_interval")
      || !require_integer(reschedule_times, "reschedule_times")
      || !require_integer(priority, "priority")
      || !require_boolean(allow_mixed_priority, "allow_mixed_priority")
      || !require_string(selection_type, "selection_type")
      || !require_string(selection_pattern, "selection_pattern")
      || !require_boolean(accurate, "accurate")
      || !require_boolean(allow_duplicate_jobs, "allow_duplicate_jobs")
      || !require_boolean(allow_higher_duplicates, "allow_higher_duplicates")
      || !require_boolean(cancel_lower_level_duplicates,
                          "cancel_lower_level_duplicates")
      || !require_boolean(cancel_queued_duplicates, "cancel_queued_duplicates")
      || !require_boolean(cancel_running_duplicates,
                          "cancel_running_duplicates")
      || !require_boolean(save_file_history, "save_file_history")
      || !require_integer(file_history_size, "file_history_size")
      || !require_string_array(fd_plugin_options, "fd_plugin_options")
      || !require_string_array(sd_plugin_options, "sd_plugin_options")
      || !require_string_array(dir_plugin_options, "dir_plugin_options")
      || !require_integer(max_concurrent_copies, "max_concurrent_copies")
      || !require_boolean(always_incremental, "always_incremental")
      || !require_integer(always_incremental_job_retention,
                          "always_incremental_job_retention")
      || !require_integer(always_incremental_keep_number,
                          "always_incremental_keep_number")
      || !require_integer(always_incremental_max_full_age,
                          "always_incremental_max_full_age")
      || !require_integer(max_full_consolidations, "max_full_consolidations")
      || !require_integer(run_on_incoming_connect_interval,
                          "run_on_incoming_connect_interval")
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
  auto parse_uint64 = [&error](json_t* value, const char* field,
                               std::optional<uint64_t>& target) {
    if (!value || !json_is_integer(value)) { return true; }
    const auto raw = json_integer_value(value);
    if (raw < 0) {
      error = std::string{"field '"} + field + "' must be non-negative.";
      return false;
    }
    target = static_cast<uint64_t>(raw);
    return true;
  };
  auto parse_uint32 = [&error](json_t* value, const char* field,
                               std::optional<uint32_t>& target) {
    if (!value || !json_is_integer(value)) { return true; }
    const auto raw = json_integer_value(value);
    if (raw < 0 || raw > std::numeric_limits<uint32_t>::max()) {
      error = std::string{"field '"} + field
              + "' must be between 0 and 4294967295.";
      return false;
    }
    target = static_cast<uint32_t>(raw);
    return true;
  };

  DirectorJobRequestSpec spec{};
  if (description && json_is_string(description)) {
    spec.description = std::string{json_string_value(description)};
  }
  if (type && json_is_string(type)) {
    spec.type = std::string{json_string_value(type)};
  }
  if (backup_format && json_is_string(backup_format)) {
    spec.backup_format = std::string{json_string_value(backup_format)};
  }
  if (protocol && json_is_string(protocol)) {
    spec.protocol = std::string{json_string_value(protocol)};
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
  spec.run_entries = parse_string_array(run_entries);
  spec.run_before_job_entries = parse_string_array(run_before_job_entries);
  spec.run_after_job_entries = parse_string_array(run_after_job_entries);
  spec.run_after_failed_job_entries
      = parse_string_array(run_after_failed_job_entries);
  spec.client_run_before_job_entries
      = parse_string_array(client_run_before_job_entries);
  spec.client_run_after_job_entries
      = parse_string_array(client_run_after_job_entries);
  spec.runscript_blocks = parse_string_array(runscript_blocks);
  if (where && json_is_string(where)) {
    spec.where = std::string{json_string_value(where)};
  }
  if (replace && json_is_string(replace)) {
    spec.replace = std::string{json_string_value(replace)};
  }
  if (regex_where && json_is_string(regex_where)) {
    spec.regex_where = std::string{json_string_value(regex_where)};
  }
  if (strip_prefix && json_is_string(strip_prefix)) {
    spec.strip_prefix = std::string{json_string_value(strip_prefix)};
  }
  if (add_prefix && json_is_string(add_prefix)) {
    spec.add_prefix = std::string{json_string_value(add_prefix)};
  }
  if (add_suffix && json_is_string(add_suffix)) {
    spec.add_suffix = std::string{json_string_value(add_suffix)};
  }
  if (bootstrap && json_is_string(bootstrap)) {
    spec.bootstrap = std::string{json_string_value(bootstrap)};
  }
  if (write_bootstrap && json_is_string(write_bootstrap)) {
    spec.write_bootstrap = std::string{json_string_value(write_bootstrap)};
  }
  if (write_verify_list && json_is_string(write_verify_list)) {
    spec.write_verify_list = std::string{json_string_value(write_verify_list)};
  }
  if (!parse_uint64(maximum_bandwidth, "maximum_bandwidth",
                    spec.maximum_bandwidth)
      || !parse_uint64(max_run_sched_time, "max_run_sched_time",
                       spec.max_run_sched_time)
      || !parse_uint64(max_run_time, "max_run_time", spec.max_run_time)
      || !parse_uint64(full_max_runtime, "full_max_runtime",
                       spec.full_max_runtime)
      || !parse_uint64(incremental_max_runtime, "incremental_max_runtime",
                       spec.incremental_max_runtime)
      || !parse_uint64(differential_max_runtime, "differential_max_runtime",
                       spec.differential_max_runtime)
      || !parse_uint64(max_wait_time, "max_wait_time", spec.max_wait_time)
      || !parse_uint64(max_start_delay, "max_start_delay", spec.max_start_delay)
      || !parse_uint64(max_full_interval, "max_full_interval",
                       spec.max_full_interval)
      || !parse_uint64(max_virtual_full_interval, "max_virtual_full_interval",
                       spec.max_virtual_full_interval)
      || !parse_uint64(max_diff_interval, "max_diff_interval",
                       spec.max_diff_interval)
      || !parse_uint64(spool_size, "spool_size", spec.spool_size)
      || !parse_uint32(maximum_concurrent_jobs, "maximum_concurrent_jobs",
                       spec.maximum_concurrent_jobs)
      || !parse_uint64(reschedule_interval, "reschedule_interval",
                       spec.reschedule_interval)
      || !parse_uint32(reschedule_times, "reschedule_times",
                       spec.reschedule_times)
      || !parse_uint64(file_history_size, "file_history_size",
                       spec.file_history_size)
      || !parse_uint32(max_concurrent_copies, "max_concurrent_copies",
                       spec.max_concurrent_copies)
      || !parse_uint64(always_incremental_job_retention,
                       "always_incremental_job_retention",
                       spec.always_incremental_job_retention)
      || !parse_uint32(always_incremental_keep_number,
                       "always_incremental_keep_number",
                       spec.always_incremental_keep_number)
      || !parse_uint64(always_incremental_max_full_age,
                       "always_incremental_max_full_age",
                       spec.always_incremental_max_full_age)
      || !parse_uint32(max_full_consolidations, "max_full_consolidations",
                       spec.max_full_consolidations)
      || !parse_uint64(run_on_incoming_connect_interval,
                       "run_on_incoming_connect_interval",
                       spec.run_on_incoming_connect_interval)) {
    return std::nullopt;
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
  if (prefix_links && json_is_boolean(prefix_links)) {
    spec.prefix_links = json_is_true(prefix_links);
  }
  if (prune_jobs && json_is_boolean(prune_jobs)) {
    spec.prune_jobs = json_is_true(prune_jobs);
  }
  if (prune_files && json_is_boolean(prune_files)) {
    spec.prune_files = json_is_true(prune_files);
  }
  if (prune_volumes && json_is_boolean(prune_volumes)) {
    spec.prune_volumes = json_is_true(prune_volumes);
  }
  if (purge_migration_job && json_is_boolean(purge_migration_job)) {
    spec.purge_migration_job = json_is_true(purge_migration_job);
  }
  if (spool_attributes && json_is_boolean(spool_attributes)) {
    spec.spool_attributes = json_is_true(spool_attributes);
  }
  if (spool_data && json_is_boolean(spool_data)) {
    spec.spool_data = json_is_true(spool_data);
  }
  if (rerun_failed_levels && json_is_boolean(rerun_failed_levels)) {
    spec.rerun_failed_levels = json_is_true(rerun_failed_levels);
  }
  if (prefer_mounted_volumes && json_is_boolean(prefer_mounted_volumes)) {
    spec.prefer_mounted_volumes = json_is_true(prefer_mounted_volumes);
  }
  if (reschedule_on_error && json_is_boolean(reschedule_on_error)) {
    spec.reschedule_on_error = json_is_true(reschedule_on_error);
  }
  if (allow_mixed_priority && json_is_boolean(allow_mixed_priority)) {
    spec.allow_mixed_priority = json_is_true(allow_mixed_priority);
  }
  if (selection_type && json_is_string(selection_type)) {
    spec.selection_type = std::string{json_string_value(selection_type)};
  }
  if (selection_pattern && json_is_string(selection_pattern)) {
    spec.selection_pattern = std::string{json_string_value(selection_pattern)};
  }
  if (accurate && json_is_boolean(accurate)) {
    spec.accurate = json_is_true(accurate);
  }
  if (allow_duplicate_jobs && json_is_boolean(allow_duplicate_jobs)) {
    spec.allow_duplicate_jobs = json_is_true(allow_duplicate_jobs);
  }
  if (allow_higher_duplicates && json_is_boolean(allow_higher_duplicates)) {
    spec.allow_higher_duplicates = json_is_true(allow_higher_duplicates);
  }
  if (cancel_lower_level_duplicates
      && json_is_boolean(cancel_lower_level_duplicates)) {
    spec.cancel_lower_level_duplicates
        = json_is_true(cancel_lower_level_duplicates);
  }
  if (cancel_queued_duplicates && json_is_boolean(cancel_queued_duplicates)) {
    spec.cancel_queued_duplicates = json_is_true(cancel_queued_duplicates);
  }
  if (cancel_running_duplicates && json_is_boolean(cancel_running_duplicates)) {
    spec.cancel_running_duplicates = json_is_true(cancel_running_duplicates);
  }
  if (save_file_history && json_is_boolean(save_file_history)) {
    spec.save_file_history = json_is_true(save_file_history);
  }
  if (always_incremental && json_is_boolean(always_incremental)) {
    spec.always_incremental = json_is_true(always_incremental);
  }
  spec.fd_plugin_options = parse_string_array(fd_plugin_options);
  spec.sd_plugin_options = parse_string_array(sd_plugin_options);
  spec.dir_plugin_options = parse_string_array(dir_plugin_options);
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
      && path_parts[5] == "directors" && path_parts[6] != "prefill"
      && request.method() == http::verb::put) {
    return HandleDeploymentClientDirectorStubPutRequest(
        state, request, path_parts[2], path_parts[4], path_parts[6]);
  }
  if (path_parts.size() == 8 && path_parts[3] == "clients"
      && path_parts[5] == "directors" && path_parts[7] == "prefill"
      && request.method() == http::verb::get) {
    return HandleClientDirectorStubPrefillRequest(state, path_parts[2],
                                                  path_parts[4], path_parts[6]);
  }
  if (path_parts.size() == 7 && path_parts[3] == "clients"
      && path_parts[5] == "directors"
      && request.method() == http::verb::delete_) {
    return HandleDeploymentClientDirectorStubDeleteRequest(
        state, path_parts[2], path_parts[4], path_parts[6]);
  }

  if (path_parts.size() == 8 && path_parts[3] == "storages"
      && path_parts[5] == "directors" && path_parts[7] == "prefill"
      && request.method() == http::verb::get) {
    return HandleStorageDirectorPrefillRequest(state, path_parts[2],
                                               path_parts[4], path_parts[6]);
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

  if (path_parts.size() == 8 && path_parts[3] == "directors"
      && path_parts[5] == "clients" && path_parts[7] == "prefill"
      && request.method() == http::verb::get) {
    return HandleDirectorClientPrefillRequest(state, path_parts[2],
                                              path_parts[4], path_parts[6]);
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

  if (path_parts.size() == 8 && path_parts[3] == "directors"
      && path_parts[5] == "storages" && path_parts[7] == "prefill"
      && request.method() == http::verb::get) {
    return HandleDirectorStoragePrefillRequest(state, path_parts[2],
                                               path_parts[4], path_parts[6]);
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
