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

function option(name, status, description, source = 'plugin-doc') {
  return {
    name,
    status,
    description,
    source,
  }
}

const manualBaseUrl = 'https://docs.bareos.org/master'

function manualUrl(path) {
  return `${manualBaseUrl}/${path}`
}

export const restorePluginHints = Object.freeze({
  bpipe: {
    displayName: 'BPipe',
    manualUrl: manualUrl('TasksAndConcepts/Plugins.html#bpipe'),
    optionSeparator: ':',
    aliases: ['bpipe'],
    options: [
      option('file', 'known', 'Pseudo path and filename shown in the restore tree.', 'plugin-doc'),
      option('reader', 'known', 'Program executed during backup to produce the data stream.', 'plugin-doc'),
      option('writer', 'known', 'Program executed during restore to consume the data stream.', 'plugin-doc'),
    ],
  },
  grpc: {
    displayName: 'gRPC FD plugin',
    manualUrl: manualUrl('TasksAndConcepts/Plugins.html#grpcplugin'),
    optionSeparator: ':',
    aliases: ['grpc', 'bareos-grpc-fd-plugin-bridge'],
    note: 'The grpc bridge itself does not define a restore schema. It forwards a normal Bareos plugin definition to the inferior plugin.',
    options: [
      option('module_name', 'known', 'Module name passed through to the wrapped plugin definition.', 'plugin-doc'),
      option('module_path', 'known', 'Optional plugin search path for the wrapped plugin.', 'plugin-doc'),
      option('filename', 'known', 'Example argument used by wrapped plugins such as the Python bridge examples.', 'plugin-example'),
      option('file', 'known', 'Example argument used by grpc test modules.', 'plugin-example'),
    ],
  },
  hyperV: {
    displayName: 'Hyper-V',
    manualUrl: manualUrl('TasksAndConcepts/Plugins.html#hypervplugin'),
    optionSeparator: ':',
    aliases: ['hyper-v'],
    options: [
      option('config_file', 'known', 'Path to the Hyper-V plugin configuration file.', 'plugin-example'),
    ],
  },
  ldap: {
    displayName: 'LDAP',
    manualUrl: manualUrl('TasksAndConcepts/Plugins.html#ldap-plugin'),
    optionSeparator: ':',
    aliases: ['ldap', 'bareos-fd-ldap'],
    note: 'The shipped example documents the LDAP option names more explicitly than the manual page.',
    options: [
      option('uri', 'required', 'LDAP server URI.', 'plugin-example'),
      option('basedn', 'required', 'Base DN used for the LDAP search.', 'plugin-example'),
      option('bind_dn', 'optional', 'Bind DN used for authenticated LDAP access.', 'plugin-example'),
      option('password', 'optional', 'Password for the bind DN.', 'plugin-example'),
      option('search_filter', 'optional', 'LDAP search filter for restricting returned entries.', 'plugin-source'),
    ],
  },
  libcloud: {
    displayName: 'Libcloud',
    manualUrl: manualUrl('TasksAndConcepts/Plugins.html#libcloudplugin'),
    optionSeparator: ':',
    aliases: ['libcloud', 'bareos-fd-libcloud'],
    note: 'The plugin string selects the module and bucket filters, while the connection details usually live in a config file.',
    options: [
      option('module_path', 'known', 'Optional path to Bareos plugin modules.', 'plugin-doc'),
      option('module_name', 'known', 'Plugin module name, usually bareos-fd-libcloud.', 'plugin-doc'),
      option('config_file', 'optional', 'Path to the libcloud configuration file.', 'plugin-doc'),
      option('buckets_include', 'optional', 'Comma-separated list of buckets to include.', 'plugin-doc'),
      option('buckets_exclude', 'optional', 'Comma-separated list of buckets to exclude.', 'plugin-doc'),
      option('hostname', 'required', 'Hostname or IP address of the storage backend.', 'plugin-doc'),
      option('port', 'required', 'Backend TCP port.', 'plugin-doc'),
      option('tls', 'required', 'Whether transport encryption should be used.', 'plugin-doc'),
      option('provider', 'required', 'Cloud provider string, for example S3.', 'plugin-doc'),
      option('username', 'required', 'Backend username or S3 access key.', 'plugin-doc'),
      option('password', 'required', 'Backend password or S3 secret key.', 'plugin-doc'),
      option('nb_worker', 'required', 'Number of worker threads preloading objects.', 'plugin-doc'),
      option('queue_size', 'required', 'Maximum number of queued objects between workers and plugin.', 'plugin-doc'),
      option('prefetch_size', 'required', 'Maximum object size downloaded in parallel by workers.', 'plugin-doc'),
      option('temporary_download_directory', 'required', 'Local directory used for temporary downloads.', 'plugin-doc'),
      option('fail_on_download_error', 'optional', 'Fail the backup on download errors instead of skipping files.', 'plugin-doc'),
      option('job_message_after_each_number_of_objects', 'optional', 'Write progress messages after this many objects.', 'plugin-doc'),
      option('prefetch_inmemory_size', 'optional', 'Maximum object size preloaded into memory.', 'plugin-doc'),
      option('global_timeout', 'optional', 'Timeout used to detect stuck worker threads.', 'plugin-doc'),
      option('libcloud_timeout', 'optional', 'Timeout in seconds for individual libcloud calls.', 'plugin-doc'),
    ],
  },
  mariabackup: {
    displayName: 'Mariabackup',
    manualUrl: manualUrl('TasksAndConcepts/Plugins.html#backup-mariadb-mariabackup'),
    optionSeparator: ':',
    aliases: ['mariabackup', 'bareos-fd-mariabackup'],
    options: [
      option('mycnf', 'optional', 'Path to the my.cnf file containing connection credentials.', 'plugin-doc'),
      option('dumpbinary', 'known', 'Override the mariabackup command binary.', 'plugin-doc'),
      option('dumpoptions', 'known', 'Override the mariabackup backup command options.', 'plugin-doc'),
      option('restorecommand', 'known', 'Override the restore command, defaulting to mbstream extraction.', 'plugin-doc'),
      option('strictIncremental', 'known', 'Skip writing data when the LSN did not change.', 'plugin-doc'),
      option('extradumpoptions', 'known', 'Additional dump options observed in plugin code, not explicitly documented.', 'plugin-source'),
      option('mysqlcmd', 'known', 'Auxiliary MySQL command override observed in plugin code.', 'plugin-source'),
      option('log', 'known', 'Plugin log setting observed in plugin code.', 'plugin-source'),
    ],
  },
  mariadbDump: {
    displayName: 'MariaDB dump',
    supportLevel: 'contrib',
    manualUrl: manualUrl('Appendix/Howtos/BackupOfThirdPartyDatabases.html#backup-mariadb-dump-python'),
    optionSeparator: ':',
    aliases: ['bareos-fd-mariadb-dump', 'mariadb-dump'],
    options: [
      option('db', 'known', 'Comma-separated list of databases to back up.', 'plugin-readme'),
      option('ignore_db', 'known', 'Comma-separated list of databases to exclude.', 'plugin-readme'),
      option('host', 'known', 'MariaDB host name, defaulting to localhost.', 'plugin-readme'),
      option('dumpoptions', 'known', 'Override the mariadb-dump option string.', 'plugin-readme'),
      option('drop_and_recreate', 'known', 'Disable drop/create statements when set to false.', 'plugin-readme'),
      option('defaultsfile', 'known', 'Path to a MariaDB defaults file for client utilities.', 'plugin-readme'),
      option('user', 'known', 'Database user for backup access.', 'plugin-readme'),
      option('password', 'known', 'Password for the configured database user.', 'plugin-readme'),
      option('dumpbinary', 'known', 'Override the mariadb-dump binary path.', 'plugin-readme'),
    ],
  },
  mysqlDump: {
    displayName: 'MySQL dump',
    supportLevel: 'contrib',
    manualUrl: manualUrl('Appendix/Howtos/BackupOfThirdPartyDatabases.html#backup-mysql-python'),
    optionSeparator: ':',
    aliases: ['bareos_mysql_dump', 'bareos-fd-mysql'],
    options: [
      option('db', 'known', 'Comma-separated list of databases to back up.', 'plugin-readme'),
      option('ignore_db', 'known', 'Comma-separated list of databases to exclude.', 'plugin-readme'),
      option('mysqlhost', 'known', 'MySQL host name, defaulting to localhost.', 'plugin-readme'),
      option('dumpoptions', 'known', 'Override the mysqldump option string.', 'plugin-readme'),
      option('drop_and_recreate', 'known', 'Disable drop/create statements when set to false.', 'plugin-readme'),
      option('defaultsfile', 'known', 'Path to a defaults file for mysql and mysqldump.', 'plugin-readme'),
      option('mysqluser', 'known', 'Database user for backup access.', 'plugin-readme'),
      option('mysqlpassword', 'known', 'Password for the configured database user.', 'plugin-readme'),
      option('dumpbinary', 'known', 'Override the mysqldump binary path.', 'plugin-readme'),
    ],
  },
  perconaXtrabackup: {
    displayName: 'Percona XtraBackup',
    manualUrl: manualUrl('TasksAndConcepts/Plugins.html#perconaxtrabackupplugin'),
    optionSeparator: ':',
    aliases: ['percona-xtrabackup', 'bareos-fd-percona-xtrabackup'],
    options: [
      option('mycnf', 'optional', 'Path to the my.cnf file containing connection credentials.', 'plugin-doc'),
      option('dumpbinary', 'known', 'Override the XtraBackup command binary.', 'plugin-doc'),
      option('dumpoptions', 'known', 'Override the XtraBackup backup command options.', 'plugin-doc'),
      option('restorecommand', 'known', 'Override the restore command, defaulting to xbstream extraction.', 'plugin-doc'),
      option('strictIncremental', 'known', 'Skip writing data when the LSN did not change.', 'plugin-doc'),
      option('extradumpoptions', 'known', 'Additional dump options observed in plugin code, not explicitly documented.', 'plugin-source'),
      option('mysqlcmd', 'known', 'Auxiliary MySQL command override observed in plugin code.', 'plugin-source'),
      option('log', 'known', 'Plugin log setting observed in plugin code.', 'plugin-source'),
    ],
  },
  postgresql: {
    displayName: 'PostgreSQL',
    manualUrl: manualUrl('TasksAndConcepts/Plugins.html#plugin-postgresql-fd'),
    optionSeparator: ':',
    aliases: ['postgresql', 'bareos-fd-postgresql'],
    options: [
      option('wal_archive_dir', 'required', 'Directory where PostgreSQL archives WAL files.', 'plugin-doc'),
      option('db_user', 'optional', 'Database role used for the PostgreSQL connection.', 'plugin-doc'),
      option('db_password', 'optional', 'Password for the database connection.', 'plugin-doc'),
      option('db_name', 'optional', 'Database name used to establish the connection.', 'plugin-doc'),
      option('db_host', 'optional', 'Database host or Unix socket directory.', 'plugin-doc'),
      option('db_port', 'optional', 'Database port or socket extension.', 'plugin-doc'),
      option('ignore_subdirs', 'optional', 'Comma-separated data-directory subdirectories to exclude.', 'plugin-doc'),
      option('switch_wal', 'optional', 'Force a WAL switch so all changes are archived.', 'plugin-doc'),
      option('switch_wal_timeout', 'optional', 'Timeout waiting for WAL archiving after switching.', 'plugin-doc'),
      option('role', 'optional', 'Set the SQL role after login before the first query.', 'plugin-doc'),
      option('start_fast', 'optional', 'Start the backup using an immediate checkpoint.', 'plugin-doc'),
      option('stop_wait_wal_archive', 'optional', 'Control whether the plugin waits for WAL archiving to finish.', 'plugin-doc'),
    ],
  },
  proxmox: {
    displayName: 'Proxmox VE',
    manualUrl: manualUrl('TasksAndConcepts/Plugins.html#proxmoxplugin'),
    optionSeparator: ':',
    aliases: ['proxmox', 'bareos-fd-proxmox'],
    note: 'pctstorage is required when restoring a container guest directly back into Proxmox.',
    options: [
      option('guestid', 'required', 'Guest ID to back up or recreate during restore.', 'plugin-doc'),
      option('force', 'optional', 'Overwrite an existing guest with the same ID during restore.', 'plugin-doc'),
      option('restoretodisk', 'optional', 'Restore to a local .vma dump file instead of directly restoring a guest.', 'plugin-doc'),
      option('restorepath', 'optional', 'Filesystem path used for local .vma restores.', 'plugin-doc'),
      option('pctstorage', 'optional', 'Target storage and size for container restore, for example local-lvm\\:8.', 'plugin-doc'),
    ],
  },
  qumulo: {
    displayName: 'Qumulo by Yuzuy',
    supportLevel: 'third-party',
    manualUrl: manualUrl('TasksAndConcepts/Plugins.html#section-yuzuy-qumulo-plugin'),
    optionSeparator: ':',
    aliases: ['qumulo', 'yuzuy-qumulo'],
    note: 'This is a third-party plugin. Bareos documents it, but the implementation and feature details come from Yuzuy.',
    options: [
      option('cluster', 'known', 'Qumulo cluster host name or address.', 'plugin-doc'),
      option('apikeyfile', 'known', 'Path to the Qumulo API key file.', 'plugin-doc'),
      option('secretfile', 'known', 'Path to the Qumulo API secret file.', 'plugin-doc'),
      option('snapdir', 'known', 'Snapshot directory or snapshot handling path.', 'plugin-doc'),
      option('path', 'known', 'Qumulo path to back up or restore.', 'plugin-doc'),
      option('exclude', 'optional', 'Exclude pattern or path filter.', 'plugin-doc'),
    ],
  },
  python: {
    displayName: 'Python plugin wrapper',
    manualUrl: manualUrl('TasksAndConcepts/Plugins.html#section-python-fd-plugin'),
    optionSeparator: ':',
    aliases: ['python', 'python3'],
    note: 'defaults_file provides defaults and overrides_file takes precedence over Fileset options.',
    options: [
      option('module_name', 'known', 'Plugin module name without the .py suffix.', 'plugin-doc'),
      option('module_path', 'known', 'Optional path to a non-default plugin directory.', 'plugin-doc'),
      option('defaults_file', 'known', 'Configuration file whose values act as defaults.', 'plugin-doc'),
      option('overrides_file', 'known', 'Configuration file whose values override Fileset options.', 'plugin-doc'),
    ],
  },
  tasksMariadb: {
    displayName: 'Tasks MariaDB',
    supportLevel: 'contrib',
    optionSeparator: ':',
    aliases: ['bareos_tasks.mariadb'],
    options: [
      option('folder', 'known', 'Virtual folder name used in the catalog.', 'plugin-readme'),
      option('mariadb', 'known', 'Command used to run the mariadb client.', 'plugin-readme'),
      option('mariadb_dump', 'known', 'Command used to run mariadb-dump.', 'plugin-readme'),
      option('mariadb_dump_options', 'known', 'Options passed to mariadb-dump.', 'plugin-readme'),
      option('user', 'known', 'System user running mariadb and mariadb-dump.', 'plugin-readme'),
      option('defaultsfile', 'known', 'Defaults file used by mariadb client utilities.', 'plugin-readme'),
      option('databases', 'known', 'Comma-separated list of databases to include.', 'plugin-readme'),
      option('exclude', 'known', 'Comma-separated list of databases to exclude.', 'plugin-readme'),
    ],
  },
  tasksMysql: {
    displayName: 'Tasks MySQL',
    supportLevel: 'contrib',
    optionSeparator: ':',
    aliases: ['bareos_tasks.mysql'],
    options: [
      option('folder', 'known', 'Virtual folder name used in the catalog.', 'plugin-readme'),
      option('mysql', 'known', 'Command used to run the mysql client.', 'plugin-readme'),
      option('mysql_dump', 'known', 'Command used to run mysqldump.', 'plugin-readme'),
      option('mysql_dump_options', 'known', 'Options passed to mysqldump.', 'plugin-readme'),
      option('user', 'known', 'System user running mysql and mysqldump.', 'plugin-readme'),
      option('defaultsfile', 'known', 'Defaults file used by MySQL client utilities.', 'plugin-readme'),
      option('databases', 'known', 'Comma-separated list of databases to include.', 'plugin-readme'),
      option('exclude', 'known', 'Comma-separated list of databases to exclude.', 'plugin-readme'),
    ],
  },
  tasksOracle: {
    displayName: 'Tasks Oracle',
    supportLevel: 'contrib',
    optionSeparator: ':',
    aliases: ['bareos_tasks.oracle'],
    options: [
      option('folder', 'known', 'Virtual folder name used in the catalog.', 'plugin-readme'),
      option('ora_exp', 'known', 'Command used to run Oracle exp.', 'plugin-readme'),
      option('ora_home', 'known', 'Oracle home path used by the export command.', 'plugin-readme'),
      option('ora_user', 'known', 'System user running the export command.', 'plugin-readme'),
      option('ora_exp_options', 'known', 'Options passed to the exp utility.', 'plugin-readme'),
      option('db_sid', 'known', 'Oracle SID to export.', 'plugin-readme'),
      option('db_user', 'known', 'Database user performing the export.', 'plugin-readme'),
      option('db_password', 'known', 'Password for the configured database user.', 'plugin-readme'),
    ],
  },
  tasksPgsql: {
    displayName: 'Tasks PostgreSQL',
    supportLevel: 'contrib',
    optionSeparator: ':',
    aliases: ['bareos_tasks.pgsql'],
    options: [
      option('folder', 'known', 'Virtual folder name used in the catalog.', 'plugin-readme'),
      option('psql', 'known', 'Command used to run psql.', 'plugin-readme'),
      option('pg_dump', 'known', 'Command used to run pg_dump.', 'plugin-readme'),
      option('pg_dump_options', 'known', 'Options passed to pg_dump.', 'plugin-readme'),
      option('pg_host', 'known', 'PostgreSQL host name or Unix socket directory.', 'plugin-readme'),
      option('pg_port', 'known', 'PostgreSQL TCP port or socket extension.', 'plugin-readme'),
      option('pg_user', 'known', 'User running psql and pg_dump.', 'plugin-readme'),
      option('databases', 'known', 'Comma-separated list of databases to include.', 'plugin-readme'),
      option('exclude', 'known', 'Comma-separated list of databases to exclude.', 'plugin-readme'),
    ],
  },
  vmware: {
    displayName: 'VMware',
    manualUrl: manualUrl('TasksAndConcepts/Plugins.html#vmwareplugin'),
    optionSeparator: ':',
    aliases: ['vmware', 'bareos-fd-vmware'],
    options: [
      option('vcserver', 'required', 'vCenter host name or IP address.', 'plugin-doc'),
      option('vcuser', 'required', 'vCenter API user name.', 'plugin-doc'),
      option('vcpass', 'required', 'Password for the vCenter API user.', 'plugin-doc'),
      option('dc', 'required', 'Datacenter name used to locate or recreate the VM.', 'plugin-doc'),
      option('folder', 'required', 'VM folder path, using / as separator.', 'plugin-doc'),
      option('vmname', 'required', 'Name of the VM to back up or recreate.', 'plugin-doc'),
      option('vcthumbprint', 'optional', 'SHA1 thumbprint of the vCenter SSL certificate.', 'plugin-doc'),
      option('transport', 'optional', 'Force a VDDK transport mode such as nbd or hotadd.', 'plugin-doc'),
      option('log_path', 'optional', 'Directory for bareos_vadp_dumper log files.', 'plugin-doc'),
      option('localvmdk', 'optional', 'Restore to local .vmdk files instead of directly to a VM.', 'plugin-doc'),
      option('vadp_dumper_verbose', 'optional', 'Enable verbose mode for bareos_vadp_dumper.', 'plugin-doc'),
      option('verifyssl', 'optional', 'Disable SSL certificate verification when set to no.', 'plugin-doc'),
      option('quiesce', 'optional', 'Control filesystem quiescing before the snapshot.', 'plugin-doc'),
      option('cleanup_tmpfiles', 'optional', 'Keep temporary files for debugging when set to no.', 'plugin-doc'),
      option('restore_esxhost', 'optional', 'Restore a recreated VM onto the given ESX host.', 'plugin-doc'),
      option('restore_cluster', 'optional', 'Restore a recreated VM into the given cluster.', 'plugin-doc'),
      option('restore_datastore', 'optional', 'Restore a recreated VM into the given datastore.', 'plugin-doc'),
      option('restore_resourcepool', 'optional', 'Restore a recreated VM into the given resource pool.', 'plugin-doc'),
      option('restore_powerstate', 'optional', 'Force the restored VM to stay off or power on after restore.', 'plugin-doc'),
      option('snapshot_retries', 'optional', 'Retry count for snapshot creation failures.', 'plugin-doc'),
      option('snapshot_retry_wait', 'optional', 'Delay between snapshot retries in seconds.', 'plugin-doc'),
      option('poweron_timeout', 'optional', 'Timeout waiting for the VM to power on after restore.', 'plugin-doc'),
      option('enable_cbt', 'optional', 'Enable changed block tracking automatically when possible.', 'plugin-doc'),
      option('do_io_in_core', 'optional', 'Process the VADP stream directly in the Bareos core.', 'plugin-doc'),
      option('vadp_dumper_multithreading', 'optional', 'Enable reader/writer multithreading in bareos_vadp_dumper.', 'plugin-doc'),
      option('vadp_dumper_sectors_per_call', 'optional', 'Tune sectors processed per VDDK read call.', 'plugin-doc'),
      option('vadp_dumper_query_allocated_blocks_chunk_size', 'optional', 'Tune the allocated-block query chunk size.', 'plugin-doc'),
      option('fallback_to_full_cbt', 'optional', 'Fail instead of falling back to full-CBT style incremental backup.', 'plugin-doc'),
      option('restore_allow_disks_mismatch', 'optional', 'Allow disk backing-path mismatches when recreating a VM.', 'plugin-doc'),
      option('nvram_connect_timeout', 'optional', 'HTTPS connect timeout for NVRAM backup and restore.', 'plugin-doc'),
      option('nvram_readwrite_timeout', 'optional', 'HTTPS read/write timeout for NVRAM backup and restore.', 'plugin-doc'),
      option('config_file', 'optional', 'Path to a configuration file with shared VMware plugin options.', 'plugin-doc'),
      option('uuid', 'known', 'Deprecated VM identifier that only supports restore to the same VM.', 'plugin-doc'),
    ],
  },
})
