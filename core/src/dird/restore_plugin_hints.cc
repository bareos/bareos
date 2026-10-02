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

#include "include/bareos.h"
#include "dird/restore_plugin_hints.h"
#include "cats/cats.h"
#include "lib/edit.h"

#include <algorithm>
#include <cctype>
#include <optional>

namespace directordaemon::restore_plugin_hints {

namespace {

// clang-format off
constexpr PluginOptionHint kBarriOptions[] = {
    {"files", "required", "Comma-separated target devices or files to restore the BARRI image to; mutually exclusive with directory and copy.", "plugin-doc", PluginOptionType::kPath},
    {"directory", "optional", "Restore disks into generated files below this directory; mutually exclusive with files and copy.", "plugin-doc", PluginOptionType::kPath},
    {"copy", "optional", "Copy the BARRI image to this file for manual recovery with barri-cli; mutually exclusive with files and directory.", "plugin-doc", PluginOptionType::kPath},
    {"save-unreferenced-disks", "optional", "Try to save disks that contain no snapshotted data. Default: yes.", "plugin-doc", PluginOptionType::kBoolean},
    {"save-unreferenced-partitions", "optional", "Try to save partitions that contain no snapshotted data. Default: yes.", "plugin-doc", PluginOptionType::kBoolean},
    {"save-unreferenced-extents", "optional", "Try to save even unsnapshotted parts of partitions. Default: yes.", "plugin-doc", PluginOptionType::kBoolean},
    {"ignore-disks", "optional", "Comma-separated list of disks to ignore.", "plugin-doc"},
};

constexpr PluginOptionHint kBpipeOptions[] = {
    {"file", "known", "Pseudo path and filename shown in the restore tree.", "plugin-doc", PluginOptionType::kPath},
    {"reader", "known", "Program executed during backup to produce the data stream.", "plugin-doc"},
    {"writer", "known", "Program executed during restore to consume the data stream.", "plugin-doc"},
};

constexpr PluginOptionHint kGrpcOptions[] = {
    {"module_name", "known", "Module name passed through to the wrapped plugin definition.", "plugin-doc"},
    {"module_path", "known", "Optional plugin search path for the wrapped plugin.", "plugin-doc", PluginOptionType::kPath},
    {"filename", "known", "Example argument used by wrapped plugins such as the Python bridge examples.", "plugin-example", PluginOptionType::kPath},
    {"file", "known", "Example argument used by grpc test modules.", "plugin-example", PluginOptionType::kPath},
};

constexpr PluginOptionHint kHyperVOptions[] = {
    {"config_file", "known", "Path to the Hyper-V plugin configuration file.", "plugin-example", PluginOptionType::kPath, true},
};

constexpr PluginOptionHint kIncusOptions[] = {
    {"instance", "required", "Instance name to back up; can be overridden on restore with restore_instance.", "plugin-doc"},
    {"project", "optional", "Incus project containing the instance.", "plugin-doc"},
    {"remote", "optional", "Incus remote containing the instance.", "plugin-doc"},
    {"allow_disk_resize", "optional", "Reserved option that currently must stay unset or no.", "plugin-doc", PluginOptionType::kBoolean},
    {"chunk_size", "optional", "Virtual-machine disk chunk size, e.g. 64MiB.", "plugin-doc"},
    {"chunk_id_length", "optional", "Number of digits used for chunk identifiers.", "plugin-doc", PluginOptionType::kInteger},
    {"compression", "optional", "Tarball compression algorithm, e.g. none, lzma, gzip, bzip2, or zstd where supported.", "plugin-doc"},
    {"path_prefix", "optional", "Bareos storage path prefix for backup files.", "plugin-doc"},
    {"queue_depth", "optional", "Maximum number of chunks buffered in RAM during backup.", "plugin-doc", PluginOptionType::kInteger},
    {"max_file_size", "optional", "Maximum file size allowed in the backup buffer.", "plugin-doc"},
    {"backup_poll_timeout", "optional", "Delay after which a quiet Incus export is considered stalled.", "plugin-doc"},
    {"hash", "optional", "Hash algorithm used to detect changes in each chunk.", "plugin-doc"},
    {"hijacked_stat_fields", "optional", "Comma-separated stat fields used to store chunk digests.", "plugin-doc"},
    {"restore_buffer_depth", "optional", "Maximum number of restore chunks buffered in RAM.", "plugin-doc", PluginOptionType::kInteger},
    {"restore_config_override", "optional", "Repeatable <key>=<value> configuration override applied on restore.", "plugin-doc"},
    {"restore_device_override", "optional", "Repeatable <device>,<key>=<value> device override applied on restore.", "plugin-doc"},
    {"restore_instance", "optional", "Target instance name for restore; defaults to instance.", "plugin-doc"},
    {"restore_path", "optional", "Write the restore tarball to this filesystem path instead of importing it.", "plugin-doc", PluginOptionType::kPath},
    {"restore_project", "optional", "Target project for restore; defaults to project.", "plugin-doc"},
    {"restore_remote", "optional", "Target remote for restore; defaults to remote.", "plugin-doc"},
    {"restore_storage", "optional", "Target storage pool for restore.", "plugin-doc"},
    {"temp_dir", "optional", "Temporary directory used for restore chunks.", "plugin-doc", PluginOptionType::kPath},
};

constexpr PluginOptionHint kLdapOptions[] = {
    {"uri", "required", "LDAP server URI.", "plugin-example"},
    {"basedn", "required", "Base DN used for the LDAP search.", "plugin-example"},
    {"bind_dn", "optional", "Bind DN used for authenticated LDAP access.", "plugin-example"},
    {"password", "optional", "Password for the bind DN.", "plugin-example"},
    {"search_filter", "optional", "LDAP search filter for restricting returned entries.", "plugin-source"},
};

constexpr PluginOptionHint kLibcloudOptions[] = {
    {"module_path", "known", "Optional path to Bareos plugin modules.", "plugin-doc", PluginOptionType::kPath},
    {"module_name", "known", "Plugin module name, usually bareos-fd-libcloud.", "plugin-doc"},
    {"config_file", "optional", "Path to the libcloud configuration file.", "plugin-doc", PluginOptionType::kPath, true},
    {"buckets_include", "optional", "Comma-separated list of buckets to include.", "plugin-doc"},
    {"buckets_exclude", "optional", "Comma-separated list of buckets to exclude.", "plugin-doc"},
    {"hostname", "required", "Hostname or IP address of the storage backend.", "plugin-doc"},
    {"port", "required", "Backend TCP port.", "plugin-doc", PluginOptionType::kInteger},
    {"tls", "required", "Whether transport encryption should be used.", "plugin-doc", PluginOptionType::kBoolean},
    {"provider", "required", "Cloud provider string, for example S3.", "plugin-doc"},
    {"username", "required", "Backend username or S3 access key.", "plugin-doc"},
    {"password", "required", "Backend password or S3 secret key.", "plugin-doc"},
    {"nb_worker", "required", "Number of worker threads preloading objects.", "plugin-doc", PluginOptionType::kInteger},
    {"queue_size", "required", "Maximum number of queued objects between workers and plugin.", "plugin-doc", PluginOptionType::kInteger},
    {"prefetch_size", "required", "Maximum object size downloaded in parallel by workers.", "plugin-doc", PluginOptionType::kInteger},
    {"temporary_download_directory", "required", "Local directory used for temporary downloads.", "plugin-doc", PluginOptionType::kPath},
    {"fail_on_download_error", "optional", "Fail the backup on download errors instead of skipping files.", "plugin-doc", PluginOptionType::kBoolean},
    {"job_message_after_each_number_of_objects", "optional", "Write progress messages after this many objects.", "plugin-doc", PluginOptionType::kInteger},
    {"prefetch_inmemory_size", "optional", "Maximum object size preloaded into memory.", "plugin-doc", PluginOptionType::kInteger},
    {"global_timeout", "optional", "Timeout used to detect stuck worker threads.", "plugin-doc", PluginOptionType::kInteger},
    {"libcloud_timeout", "optional", "Timeout in seconds for individual libcloud calls.", "plugin-doc", PluginOptionType::kInteger},
};

constexpr PluginOptionHint kMariabackupOptions[] = {
    {"mycnf", "optional", "Path to the my.cnf file containing connection credentials.", "plugin-doc", PluginOptionType::kPath, true},
    {"dumpbinary", "known", "Override the mariabackup command binary.", "plugin-doc", PluginOptionType::kPath},
    {"dumpoptions", "known", "Override the mariabackup backup command options.", "plugin-doc"},
    {"restorecommand", "known", "Override the restore command, defaulting to mbstream extraction.", "plugin-doc"},
    {"strictIncremental", "known", "Skip writing data when the LSN did not change.", "plugin-doc", PluginOptionType::kBoolean},
    {"extradumpoptions", "known", "Additional dump options observed in plugin code, not explicitly documented.", "plugin-source"},
    {"mysqlcmd", "known", "Auxiliary MySQL command override observed in plugin code.", "plugin-source"},
    {"log", "known", "Plugin log setting observed in plugin code.", "plugin-source"},
};

constexpr PluginOptionHint kMariadbDumpOptions[] = {
    {"db", "known", "Comma-separated list of databases to back up.", "plugin-readme"},
    {"ignore_db", "known", "Comma-separated list of databases to exclude.", "plugin-readme"},
    {"host", "known", "MariaDB host name, defaulting to localhost.", "plugin-readme"},
    {"dumpoptions", "known", "Override the mariadb-dump option string.", "plugin-readme"},
    {"drop_and_recreate", "known", "Disable drop/create statements when set to false.", "plugin-readme", PluginOptionType::kBoolean},
    {"defaultsfile", "known", "Path to a MariaDB defaults file for client utilities.", "plugin-readme", PluginOptionType::kPath, true},
    {"user", "known", "Database user for backup access.", "plugin-readme"},
    {"password", "known", "Password for the configured database user.", "plugin-readme"},
    {"dumpbinary", "known", "Override the mariadb-dump binary path.", "plugin-readme", PluginOptionType::kPath},
};

constexpr PluginOptionHint kMysqlDumpOptions[] = {
    {"db", "known", "Comma-separated list of databases to back up.", "plugin-readme"},
    {"ignore_db", "known", "Comma-separated list of databases to exclude.", "plugin-readme"},
    {"mysqlhost", "known", "MySQL host name, defaulting to localhost.", "plugin-readme"},
    {"dumpoptions", "known", "Override the mysqldump option string.", "plugin-readme"},
    {"drop_and_recreate", "known", "Disable drop/create statements when set to false.", "plugin-readme", PluginOptionType::kBoolean},
    {"defaultsfile", "known", "Path to a defaults file for mysql and mysqldump.", "plugin-readme", PluginOptionType::kPath, true},
    {"mysqluser", "known", "Database user for backup access.", "plugin-readme"},
    {"mysqlpassword", "known", "Password for the configured database user.", "plugin-readme"},
    {"dumpbinary", "known", "Override the mysqldump binary path.", "plugin-readme", PluginOptionType::kPath},
};

constexpr PluginOptionHint kPerconaXtrabackupOptions[] = {
    {"mycnf", "optional", "Path to the my.cnf file containing connection credentials.", "plugin-doc", PluginOptionType::kPath, true},
    {"dumpbinary", "known", "Override the XtraBackup command binary.", "plugin-doc", PluginOptionType::kPath},
    {"dumpoptions", "known", "Override the XtraBackup backup command options.", "plugin-doc"},
    {"restorecommand", "known", "Override the restore command, defaulting to xbstream extraction.", "plugin-doc"},
    {"strictIncremental", "known", "Skip writing data when the LSN did not change.", "plugin-doc", PluginOptionType::kBoolean},
    {"extradumpoptions", "known", "Additional dump options observed in plugin code, not explicitly documented.", "plugin-source"},
    {"mysqlcmd", "known", "Auxiliary MySQL command override observed in plugin code.", "plugin-source"},
    {"log", "known", "Plugin log setting observed in plugin code.", "plugin-source"},
};

constexpr PluginOptionHint kPostgresqlOptions[] = {
    {"wal_archive_dir", "required", "Directory where PostgreSQL archives WAL files.", "plugin-doc", PluginOptionType::kPath},
    {"db_user", "optional", "Database role used for the PostgreSQL connection.", "plugin-doc"},
    {"db_password", "optional", "Password for the database connection.", "plugin-doc"},
    {"db_name", "optional", "Database name used to establish the connection.", "plugin-doc"},
    {"db_host", "optional", "Database host or Unix socket directory.", "plugin-doc"},
    {"db_port", "optional", "Database port or socket extension.", "plugin-doc"},
    {"ignore_subdirs", "optional", "Comma-separated data-directory subdirectories to exclude.", "plugin-doc"},
    {"switch_wal", "optional", "Force a WAL switch so all changes are archived.", "plugin-doc", PluginOptionType::kBoolean},
    {"switch_wal_timeout", "optional", "Timeout waiting for WAL archiving after switching.", "plugin-doc", PluginOptionType::kInteger},
    {"role", "optional", "Set the SQL role after login before the first query.", "plugin-doc"},
    {"start_fast", "optional", "Start the backup using an immediate checkpoint.", "plugin-doc", PluginOptionType::kBoolean},
    {"stop_wait_wal_archive", "optional", "Control whether the plugin waits for WAL archiving to finish.", "plugin-doc", PluginOptionType::kBoolean},
};

constexpr PluginOptionHint kProxmoxOptions[] = {
    {"guestid", "optional", "Guest ID to create or restore; required when restoring directly into Proxmox.", "plugin-doc", PluginOptionType::kInteger},
    {"force", "optional", "Overwrite an existing guest with the same ID during restore.", "plugin-doc", PluginOptionType::kBoolean},
    {"restoretodisk", "optional", "Restore to a local .vma dump file instead of directly restoring a guest.", "plugin-doc", PluginOptionType::kBoolean},
    {"restorepath", "optional", "Filesystem path used for local .vma restores.", "plugin-doc", PluginOptionType::kPath},
    {"pctstorage", "optional", "Target storage and size for container restore, for example local-lvm\\\\:8.", "plugin-doc"},
};

constexpr PluginOptionHint kQumuloOptions[] = {
    {"host", "required", "Qumulo cluster host name or address.", "plugin-source"},
    {"port", "required", "Qumulo API TCP port.", "plugin-source", PluginOptionType::kInteger},
    {"user", "required", "Qumulo API user name.", "plugin-source"},
    {"password", "required", "Qumulo API password.", "plugin-source"},
    {"mount_point", "required", "Local mount point used by the plugin.", "plugin-source", PluginOptionType::kPath},
    {"snapshot_name", "required", "Qumulo snapshot name used for the backup.", "plugin-source"},
    {"share_name", "optional", "Qumulo share to back up; either share_name or path must be set.", "plugin-source"},
    {"path", "optional", "Qumulo filesystem path to back up; either share_name or path must be set.", "plugin-source", PluginOptionType::kPath},
    {"config_file", "optional", "Path to a Qumulo plugin configuration file.", "plugin-source", PluginOptionType::kPath, true},
    {"api_limit", "optional", "Maximum number of entries requested per Qumulo API call.", "plugin-source", PluginOptionType::kInteger},
    {"max_workers", "optional", "Maximum number of worker processes.", "plugin-source", PluginOptionType::kInteger},
    {"exclusions", "optional", "Path to a file containing glob patterns to exclude.", "plugin-source", PluginOptionType::kPath},
    {"exclusions_report", "optional", "Whether to report excluded objects.", "plugin-source", PluginOptionType::kBoolean},
    {"injections", "optional", "Path to a file containing objects to inject into the backup.", "plugin-source", PluginOptionType::kPath},
    {"timeout", "optional", "Qumulo API request timeout in seconds.", "plugin-source", PluginOptionType::kInteger},
    {"perf_interval", "optional", "Performance logging interval; 0 disables reporting.", "plugin-source", PluginOptionType::kInteger},
    {"io_type", "optional", "Where file I/O is performed: core or plugin.", "plugin-source"},
};

constexpr PluginOptionHint kPythonOptions[] = {
    {"module_name", "known", "Plugin module name without the .py suffix.", "plugin-doc"},
    {"module_path", "known", "Optional path to a non-default plugin directory.", "plugin-doc", PluginOptionType::kPath},
    {"defaults_file", "known", "Configuration file whose values act as defaults.", "plugin-doc", PluginOptionType::kPath, true},
    {"overrides_file", "known", "Configuration file whose values override Fileset options.", "plugin-doc", PluginOptionType::kPath, true},
};

constexpr PluginOptionHint kTasksMariadbOptions[] = {
    {"folder", "known", "Virtual folder name used in the catalog.", "plugin-readme"},
    {"mariadb", "known", "Command used to run the mariadb client.", "plugin-readme", PluginOptionType::kPath},
    {"mariadb_dump", "known", "Command used to run mariadb-dump.", "plugin-readme", PluginOptionType::kPath},
    {"mariadb_dump_options", "known", "Options passed to mariadb-dump.", "plugin-readme"},
    {"user", "known", "System user running mariadb and mariadb-dump.", "plugin-readme"},
    {"defaultsfile", "known", "Defaults file used by mariadb client utilities.", "plugin-readme", PluginOptionType::kPath, true},
    {"databases", "known", "Comma-separated list of databases to include.", "plugin-readme"},
    {"exclude", "known", "Comma-separated list of databases to exclude.", "plugin-readme"},
};

constexpr PluginOptionHint kTasksMysqlOptions[] = {
    {"folder", "known", "Virtual folder name used in the catalog.", "plugin-readme"},
    {"mysql", "known", "Command used to run the mysql client.", "plugin-readme", PluginOptionType::kPath},
    {"mysql_dump", "known", "Command used to run mysqldump.", "plugin-readme", PluginOptionType::kPath},
    {"mysql_dump_options", "known", "Options passed to mysqldump.", "plugin-readme"},
    {"user", "known", "System user running mysql and mysqldump.", "plugin-readme"},
    {"defaultsfile", "known", "Defaults file used by MySQL client utilities.", "plugin-readme", PluginOptionType::kPath, true},
    {"databases", "known", "Comma-separated list of databases to include.", "plugin-readme"},
    {"exclude", "known", "Comma-separated list of databases to exclude.", "plugin-readme"},
};

constexpr PluginOptionHint kTasksOracleOptions[] = {
    {"folder", "known", "Virtual folder name used in the catalog.", "plugin-readme"},
    {"ora_exp", "known", "Command used to run Oracle exp.", "plugin-readme", PluginOptionType::kPath},
    {"ora_home", "known", "Oracle home path used by the export command.", "plugin-readme", PluginOptionType::kPath},
    {"ora_user", "known", "System user running the export command.", "plugin-readme"},
    {"ora_exp_options", "known", "Options passed to the exp utility.", "plugin-readme"},
    {"db_sid", "known", "Oracle SID to export.", "plugin-readme"},
    {"db_user", "known", "Database user performing the export.", "plugin-readme"},
    {"db_password", "known", "Password for the configured database user.", "plugin-readme"},
};

constexpr PluginOptionHint kTasksPgsqlOptions[] = {
    {"folder", "known", "Virtual folder name used in the catalog.", "plugin-readme"},
    {"psql", "known", "Command used to run psql.", "plugin-readme", PluginOptionType::kPath},
    {"pg_dump", "known", "Command used to run pg_dump.", "plugin-readme", PluginOptionType::kPath},
    {"pg_dump_options", "known", "Options passed to pg_dump.", "plugin-readme"},
    {"pg_host", "known", "PostgreSQL host name or Unix socket directory.", "plugin-readme"},
    {"pg_port", "known", "PostgreSQL TCP port or socket extension.", "plugin-readme"},
    {"pg_user", "known", "User running psql and pg_dump.", "plugin-readme"},
    {"databases", "known", "Comma-separated list of databases to include.", "plugin-readme"},
    {"exclude", "known", "Comma-separated list of databases to exclude.", "plugin-readme"},
};

constexpr PluginOptionHint kVmwareOptions[] = {
    {"vcserver", "required", "vCenter host name or IP address.", "plugin-doc"},
    {"vcuser", "required", "vCenter API user name.", "plugin-doc"},
    {"vcpass", "required", "Password for the vCenter API user.", "plugin-doc"},
    {"dc", "required", "Datacenter name used to locate or recreate the VM.", "plugin-doc"},
    {"folder", "required", "VM folder path, using / as separator.", "plugin-doc"},
    {"vmname", "required", "Name of the VM to back up or recreate.", "plugin-doc"},
    {"vcthumbprint", "optional", "SHA1 thumbprint of the vCenter SSL certificate.", "plugin-doc"},
    {"transport", "optional", "Force a VDDK transport mode such as nbd or hotadd.", "plugin-doc"},
    {"log_path", "optional", "Directory for bareos_vadp_dumper log files.", "plugin-doc", PluginOptionType::kPath},
    {"localvmdk", "optional", "Restore to local .vmdk files instead of directly to a VM.", "plugin-doc", PluginOptionType::kBoolean},
    {"vadp_dumper_verbose", "optional", "Enable verbose mode for bareos_vadp_dumper.", "plugin-doc", PluginOptionType::kBoolean},
    {"verifyssl", "optional", "Disable SSL certificate verification when set to no.", "plugin-doc", PluginOptionType::kBoolean},
    {"quiesce", "optional", "Control filesystem quiescing before the snapshot.", "plugin-doc", PluginOptionType::kBoolean},
    {"cleanup_tmpfiles", "optional", "Keep temporary files for debugging when set to no.", "plugin-doc", PluginOptionType::kBoolean},
    {"restore_esxhost", "optional", "Restore a recreated VM onto the given ESX host.", "plugin-doc"},
    {"restore_cluster", "optional", "Restore a recreated VM into the given cluster.", "plugin-doc"},
    {"restore_datastore", "optional", "Restore a recreated VM into the given datastore.", "plugin-doc"},
    {"restore_resourcepool", "optional", "Restore a recreated VM into the given resource pool.", "plugin-doc"},
    {"restore_powerstate", "optional", "Power state after restore: on, off, or previous.", "plugin-doc"},
    {"snapshot_retries", "optional", "Retry count for snapshot creation failures.", "plugin-doc", PluginOptionType::kInteger},
    {"snapshot_retry_wait", "optional", "Delay between snapshot retries in seconds.", "plugin-doc", PluginOptionType::kInteger},
    {"poweron_timeout", "optional", "Timeout waiting for the VM to power on after restore.", "plugin-doc", PluginOptionType::kInteger},
    {"enable_cbt", "optional", "Enable changed block tracking automatically when possible.", "plugin-doc", PluginOptionType::kBoolean},
    {"do_io_in_core", "optional", "Process the VADP stream directly in the Bareos core.", "plugin-doc", PluginOptionType::kBoolean},
    {"vadp_dumper_multithreading", "optional", "Enable reader/writer multithreading in bareos_vadp_dumper.", "plugin-doc", PluginOptionType::kBoolean},
    {"vadp_dumper_sectors_per_call", "optional", "Tune sectors processed per VDDK read call.", "plugin-doc", PluginOptionType::kInteger},
    {"vadp_dumper_query_allocated_blocks_chunk_size", "optional", "Tune the allocated-block query chunk size.", "plugin-doc", PluginOptionType::kInteger},
    {"fallback_to_full_cbt", "optional", "Fail instead of falling back to full-CBT style incremental backup.", "plugin-doc", PluginOptionType::kBoolean},
    {"restore_allow_disks_mismatch", "optional", "Allow disk backing-path mismatches when recreating a VM.", "plugin-doc", PluginOptionType::kBoolean},
    {"nvram_connect_timeout", "optional", "HTTPS connect timeout for NVRAM backup and restore.", "plugin-doc", PluginOptionType::kInteger},
    {"nvram_readwrite_timeout", "optional", "HTTPS read/write timeout for NVRAM backup and restore.", "plugin-doc", PluginOptionType::kInteger},
    {"config_file", "optional", "Path to a configuration file with shared VMware plugin options.", "plugin-doc", PluginOptionType::kPath, true},
    {"uuid", "known", "Deprecated VM identifier that only supports restore to the same VM.", "plugin-doc"},
};

constexpr std::string_view kBarriAliases[] = {"barri"};
constexpr std::string_view kBpipeAliases[] = {"bpipe"};
constexpr std::string_view kGrpcAliases[] = {"grpc", "bareos-grpc-fd-plugin-bridge"};
constexpr std::string_view kHyperVAliases[] = {"hyper-v"};
constexpr std::string_view kIncusAliases[] = {"incus", "bareos-fd-incus"};
constexpr std::string_view kLdapAliases[] = {"ldap", "bareos-fd-ldap"};
constexpr std::string_view kLibcloudAliases[] = {"libcloud", "bareos-fd-libcloud"};
constexpr std::string_view kMariabackupAliases[] = {"mariabackup", "bareos-fd-mariabackup"};
constexpr std::string_view kMariadbDumpAliases[] = {"bareos-fd-mariadb-dump", "mariadb-dump"};
constexpr std::string_view kMysqlDumpAliases[] = {"bareos_mysql_dump", "bareos-fd-mysql"};
constexpr std::string_view kPerconaXtrabackupAliases[] = {"percona-xtrabackup", "bareos-fd-percona-xtrabackup"};
constexpr std::string_view kPostgresqlAliases[] = {"postgresql", "bareos-fd-postgresql"};
constexpr std::string_view kProxmoxAliases[] = {"proxmox", "bareos-fd-proxmox"};
constexpr std::string_view kQumuloAliases[] = {"qumulo", "yuzuy-qumulo"};
constexpr std::string_view kPythonAliases[] = {"python", "python3"};
constexpr std::string_view kTasksMariadbAliases[] = {"bareos_tasks.mariadb"};
constexpr std::string_view kTasksMysqlAliases[] = {"bareos_tasks.mysql"};
constexpr std::string_view kTasksOracleAliases[] = {"bareos_tasks.oracle"};
constexpr std::string_view kTasksPgsqlAliases[] = {"bareos_tasks.pgsql"};
constexpr std::string_view kVmwareAliases[] = {"vmware", "bareos-fd-vmware"};

constexpr PluginRestoreHint kPluginRestoreHints[] = {
    {"barri", "BARRI (Bareos Recovery Imager)", "TasksAndConcepts/Plugins.html#barriplugin",
     ":", "", "bareos",
     kBarriAliases, kBarriOptions},
    {"bpipe", "BPipe", "TasksAndConcepts/Plugins.html#bpipe",
     ":", "", "bareos",
     kBpipeAliases, kBpipeOptions},
    {"grpc", "gRPC FD plugin", "TasksAndConcepts/Plugins.html#grpcplugin",
     ":", "The grpc bridge itself does not define a restore schema. It forwards a normal Bareos plugin definition to the inferior plugin.", "bareos",
     kGrpcAliases, kGrpcOptions},
    {"hyperV", "Hyper-V", "TasksAndConcepts/Plugins.html#hypervplugin",
     ":", "", "bareos",
     kHyperVAliases, kHyperVOptions},
    {"incus", "Incus", "TasksAndConcepts/Plugins.html#incusplugin",
     ":", "Options used at backup time are passed back on restore; restore_* options override the backup-time values.", "bareos",
     kIncusAliases, kIncusOptions},
    {"ldap", "LDAP", "TasksAndConcepts/Plugins.html#ldap-plugin",
     ":", "The shipped example documents the LDAP option names more explicitly than the manual page.", "bareos",
     kLdapAliases, kLdapOptions},
    {"libcloud", "Libcloud", "TasksAndConcepts/Plugins.html#libcloudplugin",
     ":", "The plugin string selects the module and bucket filters, while the connection details usually live in a config file.", "bareos",
     kLibcloudAliases, kLibcloudOptions},
    {"mariabackup", "Mariabackup", "TasksAndConcepts/Plugins.html#backup-mariadb-mariabackup",
     ":", "", "bareos",
     kMariabackupAliases, kMariabackupOptions},
    {"mariadbDump", "MariaDB dump", "Appendix/Howtos/BackupOfThirdPartyDatabases.html#backup-mariadb-dump-python",
     ":", "", "contrib",
     kMariadbDumpAliases, kMariadbDumpOptions},
    {"mysqlDump", "MySQL dump", "Appendix/Howtos/BackupOfThirdPartyDatabases.html#backup-mysql-python",
     ":", "", "contrib",
     kMysqlDumpAliases, kMysqlDumpOptions},
    {"perconaXtrabackup", "Percona XtraBackup", "TasksAndConcepts/Plugins.html#perconaxtrabackupplugin",
     ":", "", "bareos",
     kPerconaXtrabackupAliases, kPerconaXtrabackupOptions},
    {"postgresql", "PostgreSQL", "TasksAndConcepts/Plugins.html#plugin-postgresql-fd",
     ":", "", "bareos",
     kPostgresqlAliases, kPostgresqlOptions},
    {"proxmox", "Proxmox VE", "TasksAndConcepts/Plugins.html#proxmoxplugin",
     ":", "pctstorage is required when restoring a container guest directly back into Proxmox.", "bareos",
     kProxmoxAliases, kProxmoxOptions},
    {"qumulo", "Qumulo by Yuzuy", "TasksAndConcepts/Plugins.html#section-yuzuy-qumulo-plugin",
     ":", "This is a third-party plugin. Either share_name or path must be set; config_file can provide the same values.", "third-party",
     kQumuloAliases, kQumuloOptions},
    {"python", "Python plugin wrapper", "TasksAndConcepts/Plugins.html#section-python-fd-plugin",
     ":", "defaults_file provides defaults and overrides_file takes precedence over Fileset options.", "bareos",
     kPythonAliases, kPythonOptions},
    {"tasksMariadb", "Tasks MariaDB", "",
     ":", "", "contrib",
     kTasksMariadbAliases, kTasksMariadbOptions},
    {"tasksMysql", "Tasks MySQL", "",
     ":", "", "contrib",
     kTasksMysqlAliases, kTasksMysqlOptions},
    {"tasksOracle", "Tasks Oracle", "",
     ":", "", "contrib",
     kTasksOracleAliases, kTasksOracleOptions},
    {"tasksPgsql", "Tasks PostgreSQL", "",
     ":", "", "contrib",
     kTasksPgsqlAliases, kTasksPgsqlOptions},
    {"vmware", "VMware", "TasksAndConcepts/Plugins.html#vmwareplugin",
     ":", "", "bareos",
     kVmwareAliases, kVmwareOptions},
};
// clang-format on
std::string ToLower(std::string_view text)
{
  std::string result(text);
  std::transform(result.begin(), result.end(), result.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return result;
}

std::string_view TrimAscii(std::string_view text)
{
  size_t begin = 0;
  while (begin < text.size()
         && std::isspace(static_cast<unsigned char>(text[begin]))) {
    ++begin;
  }
  size_t end = text.size();
  while (end > begin
         && std::isspace(static_cast<unsigned char>(text[end - 1]))) {
    --end;
  }
  return text.substr(begin, end - begin);
}

// Parses "..." at the very start of text, returning the content between
// the quotes if a closing quote is present.
std::optional<std::string> ParseLeadingQuoted(std::string_view text)
{
  if (text.empty() || text.front() != '"') { return std::nullopt; }
  size_t end = text.find('"', 1);
  if (end == std::string_view::npos) { return std::nullopt; }
  return std::string(text.substr(1, end - 1));
}

// Matches (after trimming leading whitespace) "Plugin" [ws] "=" [ws]
// "<value>", returning <value> if the whole prefix matches.
std::optional<std::string> MatchPluginLine(std::string_view line)
{
  std::string_view trimmed = TrimAscii(line);
  constexpr std::string_view kKeyword = "Plugin";
  if (!trimmed.starts_with(kKeyword)) { return std::nullopt; }
  trimmed.remove_prefix(kKeyword.size());
  trimmed = TrimAscii(trimmed);
  if (trimmed.empty() || trimmed.front() != '=') { return std::nullopt; }
  trimmed.remove_prefix(1);
  trimmed = TrimAscii(trimmed);
  return ParseLeadingQuoted(trimmed);
}

// Matches (after trimming leading whitespace) a bare "<value>" -- a
// FileSet continuation line for a multi-line Plugin definition.
std::optional<std::string> MatchContinuationLine(std::string_view line)
{
  return ParseLeadingQuoted(TrimAscii(line));
}

bool IsOptionKeyCharacter(char c)
{
  return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '.'
         || c == '-';
}

// Extracts every unique "key" from "...:key=..." occurrences in text.
std::vector<std::string> ExtractOptionKeys(std::string_view text)
{
  std::vector<std::string> keys;
  for (size_t i = 0; i < text.size(); ++i) {
    if (text[i] != ':') { continue; }
    size_t start = i + 1;
    size_t end = start;
    while (end < text.size() && IsOptionKeyCharacter(text[end])) { ++end; }
    if (end > start && end < text.size() && text[end] == '=') {
      std::string key(text.substr(start, end - start));
      if (std::find(keys.begin(), keys.end(), key) == keys.end()) {
        keys.push_back(std::move(key));
      }
    }
    i = end > i ? end - 1 : i;
  }
  return keys;
}

// Case-insensitive substring search, mirroring the webui's /i regex flag.
size_t FindCaseInsensitive(std::string_view haystack, std::string_view needle)
{
  if (needle.empty() || needle.size() > haystack.size()) {
    return std::string_view::npos;
  }
  for (size_t i = 0; i + needle.size() <= haystack.size(); ++i) {
    bool match = true;
    for (size_t j = 0; j < needle.size(); ++j) {
      if (std::tolower(static_cast<unsigned char>(haystack[i + j]))
          != std::tolower(static_cast<unsigned char>(needle[j]))) {
        match = false;
        break;
      }
    }
    if (match) { return i; }
  }
  return std::string_view::npos;
}

// Extracts the value of "...:module_name=<value>..." (up to the next ':'
// or the end of the string), case-insensitively.
std::optional<std::string> ExtractModuleNameOption(std::string_view raw)
{
  constexpr std::string_view kNeedle = ":module_name=";
  size_t pos = FindCaseInsensitive(raw, kNeedle);
  if (pos == std::string_view::npos) { return std::nullopt; }
  size_t value_start = pos + kNeedle.size();
  size_t value_end = raw.find(':', value_start);
  if (value_end == std::string_view::npos) { value_end = raw.size(); }
  return std::string(
      TrimAscii(raw.substr(value_start, value_end - value_start)));
}

}  // namespace

std::string_view PluginOptionTypeName(PluginOptionType type)
{
  switch (type) {
    case PluginOptionType::kBoolean:
      return "boolean";
    case PluginOptionType::kInteger:
      return "integer";
    case PluginOptionType::kPath:
      return "path";
    case PluginOptionType::kString:
      break;
  }
  return "string";
}

std::span<const PluginRestoreHint> AllPluginRestoreHints()
{
  return kPluginRestoreHints;
}

std::vector<const PluginRestoreHint*> SortedPluginRestoreHints()
{
  std::vector<const PluginRestoreHint*> hints;
  hints.reserve(std::size(kPluginRestoreHints));
  for (const PluginRestoreHint& hint : kPluginRestoreHints) {
    hints.push_back(&hint);
  }
  std::sort(hints.begin(), hints.end(),
            [](const PluginRestoreHint* left, const PluginRestoreHint* right) {
              return left->display_name < right->display_name;
            });
  return hints;
}

const PluginRestoreHint* FindPluginRestoreHint(std::string_view id_or_alias)
{
  std::string needle = ToLower(id_or_alias);
  for (const PluginRestoreHint& hint : kPluginRestoreHints) {
    if (ToLower(hint.id) == needle) { return &hint; }
    for (std::string_view alias : hint.aliases) {
      if (ToLower(alias) == needle) { return &hint; }
    }
  }
  return nullptr;
}

std::vector<FileSetPluginDefinition> ExtractFileSetPluginDefinitions(
    std::string_view fileset_text)
{
  std::vector<FileSetPluginDefinition> definitions;
  std::optional<std::string> current;

  auto finish_current = [&]() {
    if (!current) { return; }
    FileSetPluginDefinition definition;
    definition.raw = *current;
    size_t separator = definition.raw.find(':');
    definition.plugin_name = separator == std::string::npos
                                 ? definition.raw
                                 : definition.raw.substr(0, separator);
    definition.option_keys = ExtractOptionKeys(definition.raw);
    definitions.push_back(std::move(definition));
    current.reset();
  };

  size_t line_start = 0;
  while (line_start <= fileset_text.size()) {
    size_t line_end = fileset_text.find('\n', line_start);
    if (line_end == std::string_view::npos) { line_end = fileset_text.size(); }
    std::string_view line
        = fileset_text.substr(line_start, line_end - line_start);

    if (std::optional<std::string> value = MatchPluginLine(line)) {
      finish_current();
      current = std::move(value);
    } else if (current) {
      if (std::optional<std::string> continuation
          = MatchContinuationLine(line)) {
        *current += *continuation;
      } else {
        finish_current();
      }
    }

    if (line_end >= fileset_text.size()) { break; }
    line_start = line_end + 1;
  }
  finish_current();

  return definitions;
}

const PluginRestoreHint* ResolvePluginRestoreHint(
    const FileSetPluginDefinition& definition)
{
  if (std::optional<std::string> module_name
      = ExtractModuleNameOption(definition.raw)) {
    if (!module_name->empty()) {
      if (const PluginRestoreHint* hint = FindPluginRestoreHint(*module_name)) {
        return hint;
      }
    }
  }

  return FindPluginRestoreHint(definition.plugin_name);
}

const FileSetPluginDefinition* FindMatchingPluginDefinition(
    const PluginRestoreHint& hint,
    const std::vector<FileSetPluginDefinition>& definitions)
{
  for (const FileSetPluginDefinition& definition : definitions) {
    if (const PluginRestoreHint* resolved
        = ResolvePluginRestoreHint(definition);
        resolved && resolved->id == hint.id) {
      return &definition;
    }
  }
  return nullptr;
}

PluginOptionsBlock BuildInitialPluginOptionsBlock(
    const FileSetPluginDefinition& definition)
{
  PluginOptionsBlock block;
  block.plugin_name = definition.plugin_name;
  if (std::optional<std::string> module_name
      = ExtractModuleNameOption(definition.raw);
      module_name && !module_name->empty()) {
    block.options.emplace_back("module_name", *module_name);
  }
  return block;
}

const PluginRestoreHint* ResolvePluginOptionsBlockHint(
    const PluginOptionsBlock& block)
{
  FileSetPluginDefinition definition;
  definition.plugin_name = block.plugin_name;
  definition.raw = BuildPluginOptionsBlock(block, ":");
  return ResolvePluginRestoreHint(definition);
}

CategorizedPluginOptions CategorizePluginOptions(
    const PluginRestoreHint& hint,
    const std::vector<std::string>& fileset_option_keys)
{
  CategorizedPluginOptions result;
  for (const PluginOptionHint& option : hint.options) {
    bool already_in_fileset = std::find(fileset_option_keys.begin(),
                                        fileset_option_keys.end(), option.name)
                              != fileset_option_keys.end();
    if (already_in_fileset) {
      result.already_in_fileset.push_back(&option);
    } else if (option.status == "required") {
      result.required.push_back(&option);
    } else {
      result.optional.push_back(&option);
    }
  }
  return result;
}

bool HintProvidesDefaultsElsewhere(const PluginRestoreHint& hint)
{
  for (const PluginOptionHint& option : hint.options) {
    if (option.provides_defaults) { return true; }
  }
  return false;
}

std::string BuildPluginOptionExample(const PluginRestoreHint& hint)
{
  std::vector<const PluginOptionHint*> preferred;
  for (const PluginOptionHint& option : hint.options) {
    if (option.status == "required") { preferred.push_back(&option); }
  }

  const std::vector<const PluginOptionHint*>* source = &preferred;
  std::vector<const PluginOptionHint*> all_options;
  if (preferred.empty()) {
    for (const PluginOptionHint& option : hint.options) {
      all_options.push_back(&option);
    }
    source = &all_options;
  }

  std::string example;
  size_t count = std::min<size_t>(2, source->size());
  for (size_t i = 0; i < count; ++i) {
    if (i > 0) { example += hint.option_separator; }
    example += (*source)[i]->name;
    example += "=...";
  }
  return example;
}

PluginOptionsBlock ParsePluginOptionsBlock(std::string_view block)
{
  PluginOptionsBlock result;

  size_t separator = block.find(':');
  std::string_view name_part = separator == std::string_view::npos
                                   ? block
                                   : block.substr(0, separator);
  result.plugin_name = std::string(name_part);
  if (separator == std::string_view::npos) { return result; }

  std::string_view rest = block.substr(separator + 1);
  size_t pos = 0;
  while (pos <= rest.size()) {
    size_t next = rest.find(':', pos);
    std::string_view part
        = rest.substr(pos, next == std::string_view::npos ? next : next - pos);
    if (!part.empty()) {
      size_t eq = part.find('=');
      if (eq == std::string_view::npos) {
        result.options.emplace_back(std::string(part), std::string());
      } else {
        result.options.emplace_back(std::string(part.substr(0, eq)),
                                    std::string(part.substr(eq + 1)));
      }
    }
    if (next == std::string_view::npos) { break; }
    pos = next + 1;
  }
  return result;
}

std::string BuildPluginOptionsBlock(const PluginOptionsBlock& block,
                                    std::string_view separator)
{
  std::string out = block.plugin_name;
  for (const auto& [key, value] : block.options) {
    out += separator;
    out += key;
    if (!value.empty()) {
      out += '=';
      out += value;
    }
  }
  return out;
}

std::vector<PluginOptionsBlock> ParsePluginOptionsDocument(
    std::string_view document)
{
  std::vector<PluginOptionsBlock> blocks;
  size_t line_start = 0;
  while (line_start <= document.size()) {
    size_t line_end = document.find('\n', line_start);
    std::string_view line = document.substr(
        line_start,
        line_end == std::string_view::npos ? line_end : line_end - line_start);
    if (!line.empty()) { blocks.push_back(ParsePluginOptionsBlock(line)); }
    if (line_end == std::string_view::npos) { break; }
    line_start = line_end + 1;
  }
  return blocks;
}

std::string BuildPluginOptionsDocument(
    const std::vector<PluginOptionsBlock>& blocks,
    std::string_view separator)
{
  std::string out;
  for (size_t i = 0; i < blocks.size(); ++i) {
    if (i > 0) { out += '\n'; }
    out += BuildPluginOptionsBlock(blocks[i], separator);
  }
  return out;
}

bool AllPluginOptionsBlocksAuthorized(
    std::string_view document,
    const std::function<bool(const std::string&)>& is_block_authorized)
{
  for (const PluginOptionsBlock& block : ParsePluginOptionsDocument(document)) {
    if (!is_block_authorized(BuildPluginOptionsBlock(block))) { return false; }
  }
  return true;
}

namespace {
int FileSetTextHandler(void* ctx, int, char** row)
{
  auto* text = static_cast<std::string*>(ctx);
  if (row[0]) { *text = row[0]; }
  return 0;
}
}  // namespace

bool GetFileSetTextByFileSetId(BareosDb* db,
                               DBId_t fileset_id,
                               std::string* text)
{
  PoolMem query;
  char ed1[50];

  Mmsg(query, "SELECT FileSetText FROM FileSet WHERE FileSetId=%s",
       edit_int64(fileset_id, ed1));

  DbLocker _{db};
  return db->SqlQuery(query.c_str(), FileSetTextHandler, text);
}

}  // namespace directordaemon::restore_plugin_hints
