/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2014 Bareos GmbH & Co. KG

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
/*
 * Main configuration file parser for BAREOS Directors,
 * some parts may be split into separate files such as
 * the schedule configuration (run_config.c).
 *
 * Note, the configuration file parser consists of three parts
 *
 * 1. The generic lexical scanner in lib/lex.c and lib/lex.h
 *
 * 2. The generic config  scanner in lib/parse_config.c and
 *    lib/parse_config.h.
 *
 *    These files contain the parser code, some utility
 *    routines, and the common store routines (name, int,
 *    string).
 *
 * 3. The daemon specific file, which contains the Resource
 *    definitions as well as any specific store routines
 *    for the resource records.
 *
 * Kern Sibbald, January MM
 */

#include "bareos.h"
#include "dird.h"

/*
 * Define the first and last resource ID record
 * types. Note, these should be unique for each
 * daemon though not a requirement.
 */
static RES *sres_head[R_LAST - R_FIRST + 1];
static RES **res_head = sres_head;

/*
 * Set default indention e.g. 2 spaces.
 */
#define DEFAULT_INDENT_STRING "  "

/*
 * Imported subroutines
 */
extern void store_inc(LEX *lc, RES_ITEM *item, int index, int pass);
extern void store_run(LEX *lc, RES_ITEM *item, int index, int pass);

/*
 * Forward referenced subroutines
 */

/*
 * We build the current resource here as we are
 * scanning the resource configuration definition,
 * then move it to allocated memory when the resource
 * scan is complete.
 */
static URES res_all;
static int32_t res_all_size = sizeof(res_all);

/*
 * Definition of records permitted within each
 * resource with the routine to process the record
 * information. NOTE! quoted names must be in lower case.
 *
 * Director Resource
 *
 * name handler value code flags default_value
 */
static RES_ITEM dir_items[] = {
   { "Name", CFG_TYPE_NAME, ITEM(res_dir.hdr.name), 0, CFG_ITEM_REQUIRED, NULL },
   { "Description", CFG_TYPE_STR, ITEM(res_dir.hdr.desc), 0, 0, NULL },
   { "Messages", CFG_TYPE_RES, ITEM(res_dir.messages), R_MSGS, 0, NULL },
   { "DirPort", CFG_TYPE_ADDRESSES_PORT, ITEM(res_dir.DIRaddrs), 0, CFG_ITEM_DEFAULT, DIR_DEFAULT_PORT },
   { "DirAddress", CFG_TYPE_ADDRESSES_ADDRESS, ITEM(res_dir.DIRaddrs), 0, CFG_ITEM_DEFAULT, DIR_DEFAULT_PORT },
   { "DirAddresses", CFG_TYPE_ADDRESSES, ITEM(res_dir.DIRaddrs), 0, CFG_ITEM_DEFAULT, DIR_DEFAULT_PORT },
   { "DirSourceAddress", CFG_TYPE_ADDRESSES_ADDRESS, ITEM(res_dir.DIRsrc_addr), 0, CFG_ITEM_DEFAULT, "0" },
   { "QueryFile", CFG_TYPE_DIR, ITEM(res_dir.query_file), 0, CFG_ITEM_REQUIRED, NULL },
   { "WorkingDirectory", CFG_TYPE_DIR, ITEM(res_dir.working_directory), 0, CFG_ITEM_DEFAULT | CFG_ITEM_PLATFORM_SPECIFIC, _PATH_BAREOS_WORKINGDIR },
   { "PidDirectory", CFG_TYPE_DIR, ITEM(res_dir.pid_directory), 0, CFG_ITEM_DEFAULT | CFG_ITEM_PLATFORM_SPECIFIC, _PATH_BAREOS_PIDDIR },
   { "PluginDirectory", CFG_TYPE_DIR, ITEM(res_dir.plugin_directory), 0, 0, NULL },
   { "PluginNames", CFG_TYPE_PLUGIN_NAMES, ITEM(res_dir.plugin_names), 0, 0, NULL },
   { "ScriptsDirectory", CFG_TYPE_DIR, ITEM(res_dir.scripts_directory), 0, 0, NULL },
#if defined(HAVE_DYNAMIC_CATS_BACKENDS)
   { "BackendDirectory", CFG_TYPE_ALIST_DIR, ITEM(res_dir.backend_directories), 0, CFG_ITEM_DEFAULT | CFG_ITEM_PLATFORM_SPECIFIC, _PATH_BAREOS_BACKENDDIR },
#endif
   { "Subscriptions", CFG_TYPE_PINT32, ITEM(res_dir.subscriptions), 0, CFG_ITEM_DEFAULT, "0" },
   { "SubSysDirectory", CFG_TYPE_DIR, ITEM(res_dir.subsys_directory), 0, 0, NULL },
   { "MaximumConcurrentJobs", CFG_TYPE_PINT32, ITEM(res_dir.MaxConcurrentJobs), 0, CFG_ITEM_DEFAULT, "1" },
   { "MaximumConsoleConnections", CFG_TYPE_PINT32, ITEM(res_dir.MaxConsoleConnect), 0, CFG_ITEM_DEFAULT, "20" },
   { "Password", CFG_TYPE_AUTOPASSWORD, ITEM(res_dir.password), 0, CFG_ITEM_REQUIRED, NULL },
   { "FdConnectTimeout", CFG_TYPE_TIME, ITEM(res_dir.FDConnectTimeout), 0, CFG_ITEM_DEFAULT, "180" /* 3 minutes */ },
   { "SdConnectTimeout", CFG_TYPE_TIME, ITEM(res_dir.SDConnectTimeout), 0, CFG_ITEM_DEFAULT, "1800" /* 30 minutes */ },
   { "HeartbeatInterval", CFG_TYPE_TIME, ITEM(res_dir.heartbeat_interval), 0, CFG_ITEM_DEFAULT, "0" },
   { "TlsAuthenticate", CFG_TYPE_BOOL, ITEM(res_dir.tls_authenticate), 0, 0, NULL },
   { "TlsEnable", CFG_TYPE_BOOL, ITEM(res_dir.tls_enable), 0, 0, NULL },
   { "TlsRequire", CFG_TYPE_BOOL, ITEM(res_dir.tls_require), 0, 0, NULL },
   { "TlsVerifyPeer", CFG_TYPE_BOOL, ITEM(res_dir.tls_verify_peer), 0, CFG_ITEM_DEFAULT, "true" },
   { "TlsCaCertificateFile", CFG_TYPE_DIR, ITEM(res_dir.tls_ca_certfile), 0, 0, NULL },
   { "TlsCaCertificateDir", CFG_TYPE_DIR, ITEM(res_dir.tls_ca_certdir), 0, 0, NULL },
   { "TlsCertificateRevocationList", CFG_TYPE_DIR, ITEM(res_dir.tls_crlfile), 0, 0, NULL },
   { "TlsCertificate", CFG_TYPE_DIR, ITEM(res_dir.tls_certfile), 0, 0, NULL },
   { "TlsKey", CFG_TYPE_DIR, ITEM(res_dir.tls_keyfile), 0, 0, NULL },
   { "TlsDhFile", CFG_TYPE_DIR, ITEM(res_dir.tls_dhfile), 0, 0, NULL },
   { "TlsAllowedCN", CFG_TYPE_ALIST_STR, ITEM(res_dir.tls_allowed_cns), 0, 0, NULL },
   { "StatisticsRetention", CFG_TYPE_TIME, ITEM(res_dir.stats_retention), 0, CFG_ITEM_DEFAULT, "160704000" /* 5 years */ },
   { "StatisticsCollectInterval", CFG_TYPE_PINT32, ITEM(res_dir.stats_collect_interval), 0, CFG_ITEM_DEFAULT, "150" },
   { "VerId", CFG_TYPE_STR, ITEM(res_dir.verid), 0, 0, NULL },
   { "OptimizeForSize", CFG_TYPE_BOOL, ITEM(res_dir.optimize_for_size), 0, CFG_ITEM_DEFAULT, "false" },
   { "OptimizeForSpeed", CFG_TYPE_BOOL, ITEM(res_dir.optimize_for_speed), 0, CFG_ITEM_DEFAULT, "false" },
   { "OmitDefaults", CFG_TYPE_BOOL, ITEM(res_dir.omit_defaults), 0, CFG_ITEM_DEFAULT, "true" },
   { "KeyEncryptionKey", CFG_TYPE_AUTOPASSWORD, ITEM(res_dir.keyencrkey), 1, 0, NULL },
   { "NdmpSnooping", CFG_TYPE_BOOL, ITEM(res_dir.ndmp_snooping), 0, 0, NULL },
   { "NdmpLogLevel", CFG_TYPE_PINT32, ITEM(res_dir.ndmp_loglevel), 0, CFG_ITEM_DEFAULT, "4" },
   { "AbsoluteJobTimeout", CFG_TYPE_PINT32, ITEM(res_dir.jcr_watchdog_time), 0, 0, NULL },
   { "Auditing", CFG_TYPE_BOOL, ITEM(res_dir.auditing), 0, CFG_ITEM_DEFAULT, "false" },
   { "AuditEvents", CFG_TYPE_AUDIT, ITEM(res_dir.audit_events), 0, 0, NULL },
   { NULL, 0, { 0 }, 0, 0, NULL }
};

/*
 * Console Resource
 *
 * name handler value code flags default_value
 */
static RES_ITEM con_items[] = {
   { "Name", CFG_TYPE_NAME, ITEM(res_con.hdr.name), 0, CFG_ITEM_REQUIRED, NULL },
   { "Description", CFG_TYPE_STR, ITEM(res_con.hdr.desc), 0, 0, NULL },
   { "Password", CFG_TYPE_AUTOPASSWORD, ITEM(res_con.password), 0, CFG_ITEM_REQUIRED, NULL },
   { "JobACL", CFG_TYPE_ACL, ITEM(res_con.ACL_lists), Job_ACL, 0, NULL },
   { "ClientACL", CFG_TYPE_ACL, ITEM(res_con.ACL_lists), Client_ACL, 0, NULL },
   { "StorageACL", CFG_TYPE_ACL, ITEM(res_con.ACL_lists), Storage_ACL, 0, NULL },
   { "ScheduleACL", CFG_TYPE_ACL, ITEM(res_con.ACL_lists), Schedule_ACL, 0, NULL },
   { "RunACL", CFG_TYPE_ACL, ITEM(res_con.ACL_lists), Run_ACL, 0, NULL },
   { "PoolACL", CFG_TYPE_ACL, ITEM(res_con.ACL_lists), Pool_ACL, 0, NULL },
   { "CommandACL", CFG_TYPE_ACL, ITEM(res_con.ACL_lists), Command_ACL, 0, NULL },
   { "FilesetACL", CFG_TYPE_ACL, ITEM(res_con.ACL_lists), FileSet_ACL, 0, NULL },
   { "CatalogACL", CFG_TYPE_ACL, ITEM(res_con.ACL_lists), Catalog_ACL, 0, NULL },
   { "WhereACL", CFG_TYPE_ACL, ITEM(res_con.ACL_lists), Where_ACL, 0, NULL },
   { "PluginoptionsACL", CFG_TYPE_ACL, ITEM(res_con.ACL_lists), PluginOptions_ACL, 0, NULL },
   { "TlsAuthenticate", CFG_TYPE_BOOL, ITEM(res_con.tls_authenticate), 0, 0, NULL },
   { "TlsEnable", CFG_TYPE_BOOL, ITEM(res_con.tls_enable), 0, 0, NULL },
   { "TlsRequire", CFG_TYPE_BOOL, ITEM(res_con.tls_require), 0, 0, NULL },
   { "TlsVerifyPeer", CFG_TYPE_BOOL, ITEM(res_con.tls_verify_peer), 0, CFG_ITEM_DEFAULT, "true" },
   { "TlsCaCertificateFile", CFG_TYPE_DIR, ITEM(res_con.tls_ca_certfile), 0, 0, NULL },
   { "TlsCaCertificateDir", CFG_TYPE_DIR, ITEM(res_con.tls_ca_certdir), 0, 0, NULL },
   { "TlsCertificateRevocationList", CFG_TYPE_DIR, ITEM(res_con.tls_crlfile), 0, 0, NULL },
   { "TlsCertificate", CFG_TYPE_DIR, ITEM(res_con.tls_certfile), 0, 0, NULL },
   { "TlsKey", CFG_TYPE_DIR, ITEM(res_con.tls_keyfile), 0, 0, NULL },
   { "TlsDhFile", CFG_TYPE_DIR, ITEM(res_con.tls_dhfile), 0, 0, NULL },
   { "TlsAllowedCN", CFG_TYPE_ALIST_STR, ITEM(res_con.tls_allowed_cns), 0, 0, NULL },
   { NULL, 0, { 0 }, 0, 0, NULL }
};

/*
 * Client or File daemon resource
 *
 * name handler value code flags default_value
 */
static RES_ITEM cli_items[] = {
   { "Name", CFG_TYPE_NAME, ITEM(res_client.hdr.name), 0, CFG_ITEM_REQUIRED, NULL },
   { "Description", CFG_TYPE_STR, ITEM(res_client.hdr.desc), 0, 0, NULL },
   { "Protocol", CFG_TYPE_AUTHPROTOCOLTYPE, ITEM(res_client.Protocol), 0, CFG_ITEM_DEFAULT, "Native" },
   { "Authtype", CFG_TYPE_AUTHTYPE, ITEM(res_client.AuthType), 0, CFG_ITEM_DEFAULT, "None" },
   { "Address", CFG_TYPE_STR, ITEM(res_client.address), 0, CFG_ITEM_REQUIRED, NULL },
   { "FdAddress", CFG_TYPE_STR, ITEM(res_client.address), 0, CFG_ITEM_ALIAS, NULL },
   { "Port", CFG_TYPE_PINT32, ITEM(res_client.FDport), 0, CFG_ITEM_DEFAULT, FD_DEFAULT_PORT },
   { "FdPort", CFG_TYPE_PINT32, ITEM(res_client.FDport), 0, CFG_ITEM_DEFAULT | CFG_ITEM_ALIAS, FD_DEFAULT_PORT },
   { "Username", CFG_TYPE_STR, ITEM(res_client.username), 0, 0, NULL },
   { "Password", CFG_TYPE_AUTOPASSWORD, ITEM(res_client.password), 0, CFG_ITEM_REQUIRED, NULL },
   { "FdPassword", CFG_TYPE_AUTOPASSWORD, ITEM(res_client.password), 0, CFG_ITEM_ALIAS, NULL },
   { "Catalog", CFG_TYPE_RES, ITEM(res_client.catalog), R_CATALOG, 0, NULL },
   { "Passive", CFG_TYPE_BOOL, ITEM(res_client.passive), 0, CFG_ITEM_DEFAULT, "false" },
   { "Enabled", CFG_TYPE_BOOL, ITEM(res_client.enabled), 0, CFG_ITEM_DEFAULT, "true" },
   { "HardQuota", CFG_TYPE_SIZE64, ITEM(res_client.HardQuota), 0, CFG_ITEM_DEFAULT, "0" },
   { "SoftQuota", CFG_TYPE_SIZE64, ITEM(res_client.SoftQuota), 0, CFG_ITEM_DEFAULT, "0" },
   { "SoftQuotaGracePeriod", CFG_TYPE_TIME, ITEM(res_client.SoftQuotaGracePeriod), 0, CFG_ITEM_DEFAULT, "0" },
   { "StrictQuotas", CFG_TYPE_BOOL, ITEM(res_client.StrictQuotas), 0, CFG_ITEM_DEFAULT, "false" },
   { "QuotaIncludeFailedJobs", CFG_TYPE_BOOL, ITEM(res_client.QuotaIncludeFailedJobs), 0, CFG_ITEM_DEFAULT, "true" },
   { "FileRetention", CFG_TYPE_TIME, ITEM(res_client.FileRetention), 0, CFG_ITEM_DEFAULT, "5184000" /* 60 days */ },
   { "JobRetention", CFG_TYPE_TIME, ITEM(res_client.JobRetention), 0, CFG_ITEM_DEFAULT, "15552000" /* 180 days */ },
   { "HeartbeatInterval", CFG_TYPE_TIME, ITEM(res_client.heartbeat_interval), 0, CFG_ITEM_DEFAULT, "0" },
   { "Autoprune", CFG_TYPE_BOOL, ITEM(res_client.AutoPrune), 0, CFG_ITEM_DEFAULT, "false" },
   { "MaximumConcurrentJobs", CFG_TYPE_PINT32, ITEM(res_client.MaxConcurrentJobs), 0, CFG_ITEM_DEFAULT, "1" },
   { "TlsAuthenticate", CFG_TYPE_BOOL, ITEM(res_client.tls_authenticate), 0, 0, NULL },
   { "TlsEnable", CFG_TYPE_BOOL, ITEM(res_client.tls_enable), 0, 0, NULL },
   { "TlsRequire", CFG_TYPE_BOOL, ITEM(res_client.tls_require), 0, 0, NULL },
   { "TlsCaCertificateFile", CFG_TYPE_DIR, ITEM(res_client.tls_ca_certfile), 0, 0, NULL },
   { "TlsCaCertificateDir", CFG_TYPE_DIR, ITEM(res_client.tls_ca_certdir), 0, 0, NULL },
   { "TlsCertificateRevocationList", CFG_TYPE_DIR, ITEM(res_client.tls_crlfile), 0, 0, NULL },
   { "TlsCertificate", CFG_TYPE_DIR, ITEM(res_client.tls_certfile), 0, 0, NULL },
   { "TlsKey", CFG_TYPE_DIR, ITEM(res_client.tls_keyfile), 0, 0, NULL },
   { "TlsAllowedCN", CFG_TYPE_ALIST_STR, ITEM(res_client.tls_allowed_cns), 0, 0, NULL },
   { "MaximumBandwidthPerJob", CFG_TYPE_SPEED, ITEM(res_client.max_bandwidth), 0, 0, NULL },
   { "NdmpLogLevel", CFG_TYPE_PINT32, ITEM(res_client.ndmp_loglevel), 0, CFG_ITEM_DEFAULT, "4" },
   { "NdmpBlockSize", CFG_TYPE_PINT32, ITEM(res_client.ndmp_blocksize), 0, CFG_ITEM_DEFAULT, "64512" },
   { NULL, 0, { 0 }, 0, 0, NULL }
};

/* Storage daemon resource
 *
 * name handler value code flags default_value
 */
static RES_ITEM store_items[] = {
   { "Name", CFG_TYPE_NAME, ITEM(res_store.hdr.name), 0, CFG_ITEM_REQUIRED, NULL },
   { "Description", CFG_TYPE_STR, ITEM(res_store.hdr.desc), 0, 0, NULL },
   { "Protocol", CFG_TYPE_AUTHPROTOCOLTYPE, ITEM(res_store.Protocol), 0, CFG_ITEM_DEFAULT, "Native" },
   { "Authtype", CFG_TYPE_AUTHTYPE, ITEM(res_store.AuthType), 0, CFG_ITEM_DEFAULT, "None" },
   { "Address", CFG_TYPE_STR, ITEM(res_store.address), 0, CFG_ITEM_REQUIRED, NULL },
   { "SdAddress", CFG_TYPE_STR, ITEM(res_store.address), 0, CFG_ITEM_ALIAS, NULL },
   { "Port", CFG_TYPE_PINT32, ITEM(res_store.SDport), 0, CFG_ITEM_DEFAULT, SD_DEFAULT_PORT },
   { "SdPort", CFG_TYPE_PINT32, ITEM(res_store.SDport), 0, CFG_ITEM_DEFAULT | CFG_ITEM_ALIAS, SD_DEFAULT_PORT },
   { "Username", CFG_TYPE_STR, ITEM(res_store.username), 0, 0, NULL },
   { "Password", CFG_TYPE_AUTOPASSWORD, ITEM(res_store.password), 0, CFG_ITEM_REQUIRED, NULL },
   { "SdPassword", CFG_TYPE_AUTOPASSWORD, ITEM(res_store.password), 0, CFG_ITEM_ALIAS, NULL },
   { "Device", CFG_TYPE_DEVICE, ITEM(res_store.device), R_DEVICE, CFG_ITEM_REQUIRED, NULL },
   { "MediaType", CFG_TYPE_STRNAME, ITEM(res_store.media_type), 0, CFG_ITEM_REQUIRED, NULL },
   { "AutoChanger", CFG_TYPE_BOOL, ITEM(res_store.autochanger), 0, CFG_ITEM_DEFAULT, "false" },
   { "Enabled", CFG_TYPE_BOOL, ITEM(res_store.enabled), 0, CFG_ITEM_DEFAULT, "true" },
   { "AllowCompression", CFG_TYPE_BOOL, ITEM(res_store.AllowCompress), 0, CFG_ITEM_DEFAULT, "true" },
   { "HeartbeatInterval", CFG_TYPE_TIME, ITEM(res_store.heartbeat_interval), 0, CFG_ITEM_DEFAULT, "0" },
   { "MaximumConcurrentJobs", CFG_TYPE_PINT32, ITEM(res_store.MaxConcurrentJobs), 0, CFG_ITEM_DEFAULT, "1" },
   { "MaximumConcurrentReadJobs", CFG_TYPE_PINT32, ITEM(res_store.MaxConcurrentReadJobs), 0, CFG_ITEM_DEFAULT, "0" },
   { "SddPort", CFG_TYPE_PINT32, ITEM(res_store.SDDport), 0, CFG_ITEM_DEPRECATED, NULL },
   { "TlsAuthenticate", CFG_TYPE_BOOL, ITEM(res_store.tls_authenticate), 0, 0, NULL },
   { "TlsEnable", CFG_TYPE_BOOL, ITEM(res_store.tls_enable), 0, 0, NULL },
   { "TlsRequire", CFG_TYPE_BOOL, ITEM(res_store.tls_require), 0, 0, NULL },
   { "TlsCaCertificateFile", CFG_TYPE_DIR, ITEM(res_store.tls_ca_certfile), 0, 0, NULL },
   { "TlsCacertificateDir", CFG_TYPE_DIR, ITEM(res_store.tls_ca_certdir), 0, 0, NULL },
   { "TlsCertificateRevocationList", CFG_TYPE_DIR, ITEM(res_store.tls_crlfile), 0, 0, NULL },
   { "TlsCertificate", CFG_TYPE_DIR, ITEM(res_store.tls_certfile), 0, 0, NULL },
   { "TlsKey", CFG_TYPE_DIR, ITEM(res_store.tls_keyfile), 0, 0, NULL },
   { "PairedStorage", CFG_TYPE_RES, ITEM(res_store.paired_storage), R_STORAGE, 0, NULL },
   { "MaximumBandwidthPerJob", CFG_TYPE_SPEED, ITEM(res_store.max_bandwidth), 0, 0, NULL },
   { "CollectStatistics", CFG_TYPE_BOOL, ITEM(res_store.collectstats), 0, CFG_ITEM_DEFAULT, "false" },
   { NULL, 0, { 0 }, 0, 0, NULL }
};

/*
 * Catalog Resource Directives
 *
 * name handler value code flags default_value
 */
static RES_ITEM cat_items[] = {
   { "Name", CFG_TYPE_NAME, ITEM(res_cat.hdr.name), 0, CFG_ITEM_REQUIRED, NULL },
   { "Description", CFG_TYPE_STR, ITEM(res_cat.hdr.desc), 0, 0, NULL },
   { "Address", CFG_TYPE_STR, ITEM(res_cat.db_address), 0, CFG_ITEM_ALIAS, NULL },
   { "DbAddress", CFG_TYPE_STR, ITEM(res_cat.db_address), 0, 0, NULL },
   { "DbPort", CFG_TYPE_PINT32, ITEM(res_cat.db_port), 0, 0, NULL },
   { "Password", CFG_TYPE_AUTOPASSWORD, ITEM(res_cat.db_password), 0, CFG_ITEM_ALIAS, NULL },
   { "DbPassword", CFG_TYPE_AUTOPASSWORD, ITEM(res_cat.db_password), 0, 0, NULL },
   { "DbUser", CFG_TYPE_STR, ITEM(res_cat.db_user), 0, 0, NULL },
   { "User", CFG_TYPE_STR, ITEM(res_cat.db_user), 0, CFG_ITEM_ALIAS, NULL },
   { "DbName", CFG_TYPE_STR, ITEM(res_cat.db_name), 0, CFG_ITEM_REQUIRED, NULL },
#ifdef HAVE_DYNAMIC_CATS_BACKENDS
   { "DbDriver", CFG_TYPE_STR, ITEM(res_cat.db_driver), 0, CFG_ITEM_REQUIRED, NULL },
#else
   { "DbDriver", CFG_TYPE_STR, ITEM(res_cat.db_driver), 0, 0, NULL },
#endif
   { "DbSocket", CFG_TYPE_STR, ITEM(res_cat.db_socket), 0, 0, NULL },
   /* Turned off for the moment */
   { "MultipleConnections", CFG_TYPE_BIT, ITEM(res_cat.mult_db_connections), 0, 0, NULL },
   { "DisableBatchInsert", CFG_TYPE_BOOL, ITEM(res_cat.disable_batch_insert), 0, CFG_ITEM_DEFAULT, "false" },
   { "MinConnections", CFG_TYPE_PINT32, ITEM(res_cat.pooling_min_connections), 0, CFG_ITEM_DEFAULT, "1" },
   { "MaxConnections", CFG_TYPE_PINT32, ITEM(res_cat.pooling_max_connections), 0, CFG_ITEM_DEFAULT, "5" },
   { "IncConnections", CFG_TYPE_PINT32, ITEM(res_cat.pooling_increment_connections), 0, CFG_ITEM_DEFAULT, "1" },
   { "IdleTimeout", CFG_TYPE_PINT32, ITEM(res_cat.pooling_idle_timeout), 0, CFG_ITEM_DEFAULT, "30" },
   { "ValidateTimeout", CFG_TYPE_PINT32, ITEM(res_cat.pooling_validate_timeout), 0, CFG_ITEM_DEFAULT, "120" },
   { NULL, 0, { 0 }, 0, 0, NULL }
};

/*
 * Job Resource Directives
 *
 * name handler value code flags default_value
 */
RES_ITEM job_items[] = {
   { "Name", CFG_TYPE_NAME, ITEM(res_job.hdr.name), 0, CFG_ITEM_REQUIRED, NULL },
   { "Description", CFG_TYPE_STR, ITEM(res_job.hdr.desc), 0, 0, NULL },
   { "Type", CFG_TYPE_JOBTYPE, ITEM(res_job.JobType), 0, CFG_ITEM_REQUIRED, NULL },
   { "Protocol", CFG_TYPE_PROTOCOLTYPE, ITEM(res_job.Protocol), 0, CFG_ITEM_DEFAULT, "Native" },
   { "BackupFormat", CFG_TYPE_STR, ITEM(res_job.backup_format), 0, CFG_ITEM_DEFAULT, "Native" },
   { "Level", CFG_TYPE_LEVEL, ITEM(res_job.JobLevel), 0, 0, NULL },
   { "Messages", CFG_TYPE_RES, ITEM(res_job.messages), R_MSGS, CFG_ITEM_REQUIRED, NULL },
   { "Storage", CFG_TYPE_ALIST_RES, ITEM(res_job.storage), R_STORAGE, 0, NULL },
   { "Pool", CFG_TYPE_RES, ITEM(res_job.pool), R_POOL, CFG_ITEM_REQUIRED, NULL },
   { "FullBackupPool", CFG_TYPE_RES, ITEM(res_job.full_pool), R_POOL, 0, NULL },
   { "IncrementalBackupPool", CFG_TYPE_RES, ITEM(res_job.inc_pool), R_POOL, 0, NULL },
   { "DifferentialBackupPool", CFG_TYPE_RES, ITEM(res_job.diff_pool), R_POOL, 0, NULL },
   { "NextPool", CFG_TYPE_RES, ITEM(res_job.next_pool), R_POOL, 0, NULL },
   { "Client", CFG_TYPE_RES, ITEM(res_job.client), R_CLIENT, 0, NULL },
   { "Fileset", CFG_TYPE_RES, ITEM(res_job.fileset), R_FILESET, 0, NULL },
   { "Schedule", CFG_TYPE_RES, ITEM(res_job.schedule), R_SCHEDULE, 0, NULL },
   { "VerifyJob", CFG_TYPE_RES, ITEM(res_job.verify_job), R_JOB, CFG_ITEM_ALIAS, NULL },
   { "JobToVerify", CFG_TYPE_RES, ITEM(res_job.verify_job), R_JOB, 0, NULL },
   { "Catalog", CFG_TYPE_RES, ITEM(res_job.catalog), R_CATALOG, 0, NULL },
   { "JobDefs", CFG_TYPE_RES, ITEM(res_job.jobdefs), R_JOBDEFS, 0, NULL },
   { "Run", CFG_TYPE_ALIST_STR, ITEM(res_job.run_cmds), 0, 0, NULL },
   /* Root of where to restore files */
   { "Where", CFG_TYPE_DIR, ITEM(res_job.RestoreWhere), 0, 0, NULL },
   { "RegexWhere", CFG_TYPE_STR, ITEM(res_job.RegexWhere), 0, 0, NULL },
   { "StripPrefix", CFG_TYPE_STR, ITEM(res_job.strip_prefix), 0, 0, NULL },
   { "AddPrefix", CFG_TYPE_STR, ITEM(res_job.add_prefix), 0, 0, NULL },
   { "AddSuffix", CFG_TYPE_STR, ITEM(res_job.add_suffix), 0, 0, NULL },
   /* Where to find bootstrap during restore */
   { "Bootstrap", CFG_TYPE_DIR, ITEM(res_job.RestoreBootstrap), 0, 0, NULL },
   /* Where to write bootstrap file during backup */
   { "WriteBootstrap", CFG_TYPE_DIR, ITEM(res_job.WriteBootstrap), 0, 0, NULL },
   { "WriteVerifyList", CFG_TYPE_DIR, ITEM(res_job.WriteVerifyList), 0, 0, NULL },
   { "Replace", CFG_TYPE_REPLACE, ITEM(res_job.replace), 0, CFG_ITEM_DEFAULT, "Always" },
   { "MaximumBandwidth", CFG_TYPE_SPEED, ITEM(res_job.max_bandwidth), 0, 0, NULL },
   { "MaxrunSchedTime", CFG_TYPE_TIME, ITEM(res_job.MaxRunSchedTime), 0, 0, NULL },
   { "MaxRunTime", CFG_TYPE_TIME, ITEM(res_job.MaxRunTime), 0, 0, NULL },
   { "FullMaxWaitTime", CFG_TYPE_TIME, ITEM(res_job.FullMaxRunTime), 0, CFG_ITEM_DEPRECATED, NULL },
   { "IncrementalMaxWaitTime", CFG_TYPE_TIME, ITEM(res_job.IncMaxRunTime), 0, CFG_ITEM_DEPRECATED, NULL },
   { "DifferentialMaxWaitTime", CFG_TYPE_TIME, ITEM(res_job.DiffMaxRunTime), 0, CFG_ITEM_DEPRECATED, NULL },
   { "FullMaxRuntime", CFG_TYPE_TIME, ITEM(res_job.FullMaxRunTime), 0, 0, NULL },
   { "IncrementalMaxRuntime", CFG_TYPE_TIME, ITEM(res_job.IncMaxRunTime), 0, 0, NULL },
   { "DifferentialMaxRuntime", CFG_TYPE_TIME, ITEM(res_job.DiffMaxRunTime), 0, 0, NULL },
   { "MaxWaitTime", CFG_TYPE_TIME, ITEM(res_job.MaxWaitTime), 0, 0, NULL },
   { "MaxStartDelay", CFG_TYPE_TIME, ITEM(res_job.MaxStartDelay), 0, 0, NULL },
   { "MaxFullInterval", CFG_TYPE_TIME, ITEM(res_job.MaxFullInterval), 0, 0, NULL },
   { "MaxDiffInterval", CFG_TYPE_TIME, ITEM(res_job.MaxDiffInterval), 0, 0, NULL },
   { "PrefixLinks", CFG_TYPE_BOOL, ITEM(res_job.PrefixLinks), 0, CFG_ITEM_DEFAULT, "false" },
   { "PruneJobs", CFG_TYPE_BOOL, ITEM(res_job.PruneJobs), 0, CFG_ITEM_DEFAULT, "false" },
   { "PruneFiles", CFG_TYPE_BOOL, ITEM(res_job.PruneFiles), 0, CFG_ITEM_DEFAULT, "false" },
   { "PruneVolumes", CFG_TYPE_BOOL, ITEM(res_job.PruneVolumes), 0, CFG_ITEM_DEFAULT, "false" },
   { "PurgeMigrationJob", CFG_TYPE_BOOL, ITEM(res_job.PurgeMigrateJob), 0, CFG_ITEM_DEFAULT, "false" },
   { "Enabled", CFG_TYPE_BOOL, ITEM(res_job.enabled), 0, CFG_ITEM_DEFAULT, "true" },
   { "SpoolAttributes", CFG_TYPE_BOOL, ITEM(res_job.SpoolAttributes), 0, CFG_ITEM_DEFAULT, "false" },
   { "SpoolData", CFG_TYPE_BOOL, ITEM(res_job.spool_data), 0, CFG_ITEM_DEFAULT, "false" },
   { "SpoolSize", CFG_TYPE_SIZE64, ITEM(res_job.spool_size), 0, 0, NULL },
   { "RerunFailedLevels", CFG_TYPE_BOOL, ITEM(res_job.rerun_failed_levels), 0, CFG_ITEM_DEFAULT, "false" },
   { "PreferMountedVolumes", CFG_TYPE_BOOL, ITEM(res_job.PreferMountedVolumes), 0, CFG_ITEM_DEFAULT, "true" },
   { "RunBeforeJob", CFG_TYPE_SHRTRUNSCRIPT, ITEM(res_job.RunScripts), 0, 0, NULL },
   { "RunAfterJob", CFG_TYPE_SHRTRUNSCRIPT, ITEM(res_job.RunScripts), 0, 0, NULL },
   { "RunAfterFailedJob", CFG_TYPE_SHRTRUNSCRIPT, ITEM(res_job.RunScripts), 0, 0, NULL },
   { "ClientRunBeforeJob", CFG_TYPE_SHRTRUNSCRIPT, ITEM(res_job.RunScripts), 0, 0, NULL },
   { "ClientRunAfterJob", CFG_TYPE_SHRTRUNSCRIPT, ITEM(res_job.RunScripts), 0, 0, NULL },
   { "MaximumConcurrentJobs", CFG_TYPE_PINT32, ITEM(res_job.MaxConcurrentJobs), 0, CFG_ITEM_DEFAULT, "1" },
   { "RescheduleOnError", CFG_TYPE_BOOL, ITEM(res_job.RescheduleOnError), 0, CFG_ITEM_DEFAULT, "false" },
   { "RescheduleInterval", CFG_TYPE_TIME, ITEM(res_job.RescheduleInterval), 0, CFG_ITEM_DEFAULT, "1800" /* 30 minutes */ },
   { "RescheduleTimes", CFG_TYPE_PINT32, ITEM(res_job.RescheduleTimes), 0, CFG_ITEM_DEFAULT, "5" },
   { "Priority", CFG_TYPE_PINT32, ITEM(res_job.Priority), 0, CFG_ITEM_DEFAULT, "10" },
   { "AllowMixedPriority", CFG_TYPE_BOOL, ITEM(res_job.allow_mixed_priority), 0, CFG_ITEM_DEFAULT, "false" },
   { "WritePartAfterJob", CFG_TYPE_BOOL, ITEM(res_job.write_part_after_job), 0, CFG_ITEM_DEPRECATED, NULL },
   { "SelectionPattern", CFG_TYPE_STR, ITEM(res_job.selection_pattern), 0, 0, NULL },
   { "RunScript", CFG_TYPE_RUNSCRIPT, ITEM(res_job.RunScripts), 0, CFG_ITEM_NO_EQUALS, NULL },
   { "SelectionType", CFG_TYPE_MIGTYPE, ITEM(res_job.selection_type), 0, 0, NULL },
   { "Accurate", CFG_TYPE_BOOL, ITEM(res_job.accurate), 0, CFG_ITEM_DEFAULT, "false" },
   { "AllowDuplicateJobs", CFG_TYPE_BOOL, ITEM(res_job.AllowDuplicateJobs), 0, CFG_ITEM_DEFAULT, "true" },
   { "AllowHigherDuplicates", CFG_TYPE_BOOL, ITEM(res_job.AllowHigherDuplicates), 0, CFG_ITEM_DEFAULT, "true" },
   { "CancelLowerLevelDuplicates", CFG_TYPE_BOOL, ITEM(res_job.CancelLowerLevelDuplicates), 0, CFG_ITEM_DEFAULT, "false" },
   { "CancelQueuedDuplicates", CFG_TYPE_BOOL, ITEM(res_job.CancelQueuedDuplicates), 0, CFG_ITEM_DEFAULT, "false" },
   { "CancelRunningDuplicates", CFG_TYPE_BOOL, ITEM(res_job.CancelRunningDuplicates), 0, CFG_ITEM_DEFAULT, "false" },
   { "SaveFileHistory", CFG_TYPE_BOOL, ITEM(res_job.SaveFileHist), 0, CFG_ITEM_DEFAULT, "true" },
   { "PluginOptions", CFG_TYPE_STR, ITEM(res_job.PluginOptions), 0, 0, NULL },
   { "Base", CFG_TYPE_ALIST_RES, ITEM(res_job.base), R_JOB, 0, NULL },
   { NULL, 0, { 0 }, 0, 0, NULL }
};

/*
 * FileSet resource
 *
 * name handler value code flags default_value
 */
static RES_ITEM fs_items[] = {
   { "Name", CFG_TYPE_NAME, ITEM(res_fs.hdr.name), 0, CFG_ITEM_REQUIRED, NULL },
   { "Description", CFG_TYPE_STR, ITEM(res_fs.hdr.desc), 0, 0, NULL },
   { "Include", CFG_TYPE_INCEXC, { 0 }, 0, CFG_ITEM_NO_EQUALS, NULL },
   { "Exclude", CFG_TYPE_INCEXC, { 0 }, 1, CFG_ITEM_NO_EQUALS, NULL },
   { "IgnoreFilesetChanges", CFG_TYPE_BOOL, ITEM(res_fs.ignore_fs_changes), 0, CFG_ITEM_DEFAULT, "false" },
   { "EnableVSS", CFG_TYPE_BOOL, ITEM(res_fs.enable_vss), 0, CFG_ITEM_DEFAULT, "true" },
   { NULL, 0, { 0 }, 0, 0, NULL }
};

/*
 * Schedule -- see run_conf.c
 *
 * name handler value code flags default_value
 */
static RES_ITEM sch_items[] = {
   { "Name", CFG_TYPE_NAME, ITEM(res_sch.hdr.name), 0, CFG_ITEM_REQUIRED, NULL },
   { "Description", CFG_TYPE_STR, ITEM(res_sch.hdr.desc), 0, 0, NULL },
   { "Run", CFG_TYPE_RUN, ITEM(res_sch.run), 0, 0, NULL },
   { "Enabled", CFG_TYPE_BOOL, ITEM(res_sch.enabled), 0, CFG_ITEM_DEFAULT, "true" },
   { NULL, 0, { 0 }, 0, 0, NULL }
};

/*
 * Pool resource
 *
 * name handler value code flags default_value
 */
static RES_ITEM pool_items[] = {
   { "Name", CFG_TYPE_NAME, ITEM(res_pool.hdr.name), 0, CFG_ITEM_REQUIRED, NULL },
   { "Description", CFG_TYPE_STR, ITEM(res_pool.hdr.desc), 0, 0, NULL },
   { "PoolType", CFG_TYPE_STRNAME, ITEM(res_pool.pool_type), 0, CFG_ITEM_REQUIRED, NULL },
   { "LabelFormat", CFG_TYPE_STRNAME, ITEM(res_pool.label_format), 0, 0, NULL },
   { "LabelType", CFG_TYPE_LABEL, ITEM(res_pool.LabelType), 0, 0, NULL },
   { "CleaningPrefix", CFG_TYPE_STRNAME, ITEM(res_pool.cleaning_prefix), 0, CFG_ITEM_DEFAULT, "CLN" },
   { "UseCatalog", CFG_TYPE_BOOL, ITEM(res_pool.use_catalog), 0, CFG_ITEM_DEFAULT, "true" },
   { "UseVolumeOnce", CFG_TYPE_BOOL, ITEM(res_pool.use_volume_once), 0, CFG_ITEM_DEPRECATED, NULL },
   { "PurgeOldestVolume", CFG_TYPE_BOOL, ITEM(res_pool.purge_oldest_volume), 0, 0, NULL },
   { "ActionOnPurge", CFG_TYPE_ACTIONONPURGE, ITEM(res_pool.action_on_purge), 0, 0, NULL },
   { "RecycleOldestVolume", CFG_TYPE_BOOL, ITEM(res_pool.recycle_oldest_volume), 0, 0, NULL },
   { "RecycleCurrentVolume", CFG_TYPE_BOOL, ITEM(res_pool.recycle_current_volume), 0, 0, NULL },
   { "MaximumVolumes", CFG_TYPE_PINT32, ITEM(res_pool.max_volumes), 0, 0, NULL },
   { "MaximumVolumeJobs", CFG_TYPE_PINT32, ITEM(res_pool.MaxVolJobs), 0, 0, NULL },
   { "MaximumVolumeFiles", CFG_TYPE_PINT32, ITEM(res_pool.MaxVolFiles), 0, 0, NULL },
   { "MaximumVolumeBytes", CFG_TYPE_SIZE64, ITEM(res_pool.MaxVolBytes), 0, 0, NULL },
   { "CatalogFiles", CFG_TYPE_BOOL, ITEM(res_pool.catalog_files), 0, CFG_ITEM_DEFAULT, "true" },
   { "VolumeRetention", CFG_TYPE_TIME, ITEM(res_pool.VolRetention), 0, CFG_ITEM_DEFAULT, "31536000" /* 365 days */ },
   { "VolumeUseDuration", CFG_TYPE_TIME, ITEM(res_pool.VolUseDuration), 0, 0, NULL },
   { "MigrationTime", CFG_TYPE_TIME, ITEM(res_pool.MigrationTime), 0, 0, NULL },
   { "MigrationHighBytes", CFG_TYPE_SIZE64, ITEM(res_pool.MigrationHighBytes), 0, 0, NULL },
   { "MigrationLowBytes", CFG_TYPE_SIZE64, ITEM(res_pool.MigrationLowBytes), 0, 0, NULL },
   { "NextPool", CFG_TYPE_RES, ITEM(res_pool.NextPool), R_POOL, 0, NULL },
   { "Storage", CFG_TYPE_ALIST_RES, ITEM(res_pool.storage), R_STORAGE, 0, NULL },
   { "Autoprune", CFG_TYPE_BOOL, ITEM(res_pool.AutoPrune), 0, CFG_ITEM_DEFAULT, "true" },
   { "Recycle", CFG_TYPE_BOOL, ITEM(res_pool.Recycle), 0, CFG_ITEM_DEFAULT, "true" },
   { "RecyclePool", CFG_TYPE_RES, ITEM(res_pool.RecyclePool), R_POOL, 0, NULL },
   { "ScratchPool", CFG_TYPE_RES, ITEM(res_pool.ScratchPool), R_POOL, 0, NULL },
   { "CopyPool", CFG_TYPE_ALIST_RES, ITEM(res_pool.CopyPool), R_POOL, 0, NULL },
   { "Catalog", CFG_TYPE_RES, ITEM(res_pool.catalog), R_CATALOG, 0, NULL },
   { "FileRetention", CFG_TYPE_TIME, ITEM(res_pool.FileRetention), 0, 0, NULL },
   { "JobRetention", CFG_TYPE_TIME, ITEM(res_pool.JobRetention), 0, 0, NULL },
   { "MinimumBlocksize", CFG_TYPE_PINT32, ITEM(res_pool.MinBlocksize), 0, 0, NULL },
   { "MaximumBlocksize", CFG_TYPE_PINT32, ITEM(res_pool.MaxBlocksize), 0, 0, NULL },
   { NULL, 0, { 0 }, 0, 0, NULL }
};

/*
 * Counter Resource
 *
 * name handler value code flags default_value
 */
static RES_ITEM counter_items[] = {
   { "Name", CFG_TYPE_NAME, ITEM(res_counter.hdr.name), 0, CFG_ITEM_REQUIRED, NULL },
   { "Description", CFG_TYPE_STR, ITEM(res_counter.hdr.desc), 0, 0, NULL },
   { "Minimum", CFG_TYPE_INT32, ITEM(res_counter.MinValue), 0, CFG_ITEM_DEFAULT, "0" },
   { "Maximum", CFG_TYPE_PINT32, ITEM(res_counter.MaxValue), 0, CFG_ITEM_DEFAULT, "2147483647" /* INT32_MAX */ },
   { "WrapCounter", CFG_TYPE_RES, ITEM(res_counter.WrapCounter), R_COUNTER, 0, NULL },
   { "Catalog", CFG_TYPE_RES, ITEM(res_counter.Catalog), R_CATALOG, 0, NULL },
   { NULL, 0, { 0 }, 0, 0, NULL }
};

/*
 * Message resource
 */
#include "lib/msg_res.h"

/*
 * This is the master resource definition.
 * It must have one item for each of the resources.
 *
 *  NOTE!!! keep it in the same order as the R_codes
 *    or eliminate all resources[rindex].name
 *
 * name handler value code flags default_value
 */
static RES_TABLE resources[] = {
   { "Director", dir_items, R_DIRECTOR, sizeof(DIRRES) },
   { "Client", cli_items, R_CLIENT, sizeof(CLIENTRES) },
   { "JobDefs", job_items, R_JOBDEFS, sizeof(JOBRES) },
   { "Job", job_items, R_JOB, sizeof(JOBRES) },
   { "Storage", store_items, R_STORAGE, sizeof(STORERES) },
   { "Catalog", cat_items, R_CATALOG, sizeof(CATRES) },
   { "Schedule", sch_items, R_SCHEDULE, sizeof(SCHEDRES) },
   { "Fileset", fs_items, R_FILESET, sizeof(FILESETRES) },
   { "Pool", pool_items, R_POOL, sizeof(POOLRES) },
   { "Messages", msgs_items, R_MSGS, sizeof(MSGSRES) },
   { "Counter", counter_items, R_COUNTER, sizeof(COUNTERRES) },
   { "Console", con_items, R_CONSOLE, sizeof(CONRES) },
   { "Device", NULL, R_DEVICE, sizeof(DEVICERES) }, /* info obtained from SD */
   { NULL, NULL, 0, 0 }
};

/*
 * Note, when this resource is used, we are inside a Job
 * resource. We treat the RunScript like a sort of
 * mini-resource within the Job resource. As such we
 * don't use the URES union because that contains
 * a Job resource (and it would corrupt the Job resource)
 * but use a global separate resource for holding the
 * runscript data.
 */
static RUNSCRIPT res_runscript;

/*
 * new RunScript items
 * name handler value code flags default_value
 */
static RES_ITEM runscript_items[] = {
 { "Command", CFG_TYPE_RUNSCRIPT_CMD, { (char **)&res_runscript }, SHELL_CMD, 0, NULL },
 { "Console", CFG_TYPE_RUNSCRIPT_CMD, { (char **)&res_runscript }, CONSOLE_CMD, 0, NULL },
 { "Target", CFG_TYPE_RUNSCRIPT_TARGET, { (char **)&res_runscript }, 0, 0, NULL },
 { "RunsOnSuccess", CFG_TYPE_RUNSCRIPT_BOOL, { (char **)&res_runscript.on_success }, 0, 0, NULL },
 { "RunsOnFailure", CFG_TYPE_RUNSCRIPT_BOOL, { (char **)&res_runscript.on_failure }, 0, 0, NULL },
 { "FailJobOnError", CFG_TYPE_RUNSCRIPT_BOOL, { (char **)&res_runscript.fail_on_error }, 0, 0, NULL },
 { "AbortJobOnError", CFG_TYPE_RUNSCRIPT_BOOL, { (char **)&res_runscript.fail_on_error }, 0, 0, NULL },
 { "RunsWhen", CFG_TYPE_RUNSCRIPT_WHEN, { (char **)&res_runscript.when }, 0, 0, NULL },
 { "RunsOnClient", CFG_TYPE_RUNSCRIPT_TARGET, { (char **)&res_runscript }, 0, 0, NULL }, /* TODO */
 { NULL, 0, { 0 }, 0, 0, NULL }
};

/*
 * The following arrays are referenced from else where and
 * used for display to the user so the keyword are pretty
 * printed with additional capitals. As the code uses
 * strcasecmp anyhow this doesn't matter.
 */

/*
 * Keywords (RHS) permitted in Job Level records
 *
 * name level job_type
 */
struct s_jl joblevels[] = {
   { "Full", L_FULL, JT_BACKUP },
   { "Base", L_BASE, JT_BACKUP },
   { "Incremental", L_INCREMENTAL, JT_BACKUP },
   { "Differential", L_DIFFERENTIAL, JT_BACKUP },
   { "Since", L_SINCE, JT_BACKUP },
   { "VirtualFull", L_VIRTUAL_FULL, JT_BACKUP },
   { "Catalog", L_VERIFY_CATALOG, JT_VERIFY },
   { "InitCatalog", L_VERIFY_INIT, JT_VERIFY },
   { "VolumeToCatalog", L_VERIFY_VOLUME_TO_CATALOG, JT_VERIFY },
   { "DiskToCatalog", L_VERIFY_DISK_TO_CATALOG, JT_VERIFY },
   { "Data", L_VERIFY_DATA, JT_VERIFY },
   { "Full", L_FULL, JT_COPY },
   { "Incremental", L_INCREMENTAL, JT_COPY },
   { "Differential", L_DIFFERENTIAL, JT_COPY },
   { "Full", L_FULL, JT_MIGRATE },
   { "Incremental", L_INCREMENTAL, JT_MIGRATE },
   { "Differential", L_DIFFERENTIAL, JT_MIGRATE },
   { " ", L_NONE, JT_ADMIN },
   { " ", L_NONE, JT_RESTORE },
   { NULL, 0, 0 }
};

/* Keywords (RHS) permitted in Job type records
 *
 * type_name job_type
 */
struct s_jt jobtypes[] = {
   { "Backup", JT_BACKUP },
   { "Admin", JT_ADMIN },
   { "Verify", JT_VERIFY },
   { "Restore", JT_RESTORE },
   { "Migrate",JT_MIGRATE },
   { "Copy", JT_COPY },
   { NULL, 0 }
};

/* Keywords (RHS) permitted in Protocol type records
 *
 * name token
 */
static struct s_kw backupprotocols[] = {
   { "Native", PT_NATIVE },
   { "NDMP", PT_NDMP },
   { NULL, 0 }
};

/* Keywords (RHS) permitted in AuthProtocol type records
 *
 * name token
 */
static struct s_kw authprotocols[] = {
   { "Native", APT_NATIVE },
   { "NDMPV2", APT_NDMPV2 },
   { "NDMPV3", APT_NDMPV3 },
   { "NDMPV4", APT_NDMPV4 },
   { NULL, 0 }
};

/* Keywords (RHS) permitted in Authentication type records
 *
 * name token
 */
static struct s_kw authmethods[] = {
   { "None", AT_NONE },
   { "Clear", AT_CLEAR },
   { "MD5", AT_MD5 },
   { NULL, 0 }
};

/*
 * Keywords (RHS) permitted in Selection type records
 *
 * type_name job_type
 */
static struct s_jt migtypes[] = {
   { "SmallestVolume", MT_SMALLEST_VOL },
   { "OldestVolume", MT_OLDEST_VOL },
   { "PoolOccupancy", MT_POOL_OCCUPANCY },
   { "PoolTime", MT_POOL_TIME },
   { "PoolUncopiedJobs", MT_POOL_UNCOPIED_JOBS },
   { "Client", MT_CLIENT },
   { "Volume", MT_VOLUME },
   { "Job", MT_JOB },
   { "SqlQuery", MT_SQLQUERY },
   { NULL, 0 }
};

/*
 * Options permitted in Restore replace=
 */
struct s_kw ReplaceOptions[] = {
   { "Always", REPLACE_ALWAYS },
   { "IfNewer", REPLACE_IFNEWER },
   { "IfOlder", REPLACE_IFOLDER },
   { "Never", REPLACE_NEVER },
   { NULL, 0 }
};

/*
 * Options permited in ActionOnPurge=
 */
struct s_kw ActionOnPurgeOptions[] = {
   { "None", ON_PURGE_NONE },
   { "Truncate", ON_PURGE_TRUNCATE },
   { NULL, 0 }
};

/*
 * Volume status allowed for a Volume.
 */
struct s_kw VolumeStatus[] = {
   { "Append", 0 },
   { "Full", 0 },
   { "Used", 0 },
   { "Recycle", 0 },
   { "Purged", 0 },
   { "Cleaning", 0 },
   { "Error", 0 },
   { NULL, 0 }
};

char *CATRES::display(POOLMEM *dst)
{
   Mmsg(dst, "catalog=%s\ndb_name=%s\ndb_driver=%s\ndb_user=%s\n"
             "db_password=%s\ndb_address=%s\ndb_port=%i\n"
             "db_socket=%s\n",
        name(), NPRTB(db_name),
        NPRTB(db_driver), NPRTB(db_user), NPRTB(db_password.value),
        NPRTB(db_address), db_port, NPRTB(db_socket));

   return dst;
}

static void indent_config_item(POOL_MEM &cfg_str, int level, const char *config_item)
{
   for (int i = 0; i < level; i++) {
      pm_strcat(cfg_str, DEFAULT_INDENT_STRING);
   }
   pm_strcat(cfg_str, config_item);
}

static inline void print_config_runscript(RES_ITEM *item, POOL_MEM &cfg_str)
{
   POOL_MEM temp;
   RUNSCRIPT* runscript;
   alist *list;

   list = *item->alistvalue;
   if (bstrcmp(item->name, "runscript")) {
      if (list != NULL) {
         foreach_alist(runscript, list) {
            int len;
            POOLMEM *cmdbuf;

            len = strlen(runscript->command);
            cmdbuf = get_pool_memory(PM_NAME);
            cmdbuf = check_pool_memory_size(cmdbuf, len * 2);
            escape_string(cmdbuf, runscript->command, len);

            /*
             * Don't print runscript when its inherited from a JobDef.
             */
            if (runscript->from_jobdef) {
               continue;
            }

            /*
             * Check if runscript must be written as short runscript
             */
            if (runscript->short_form) {
               if (runscript->when == SCRIPT_Before &&           /* runbeforejob */
                  (bstrcmp(runscript->target, ""))) {
                     Mmsg(temp, "run before job = \"%s\"\n", cmdbuf);
               } else if (runscript->when == SCRIPT_After &&     /* runafterjob */
                          runscript->on_success &&
                         !runscript->on_failure &&
                         !runscript->fail_on_error &&
                          bstrcmp(runscript->target, "")) {
                  Mmsg(temp, "run after job = \"%s\"\n", cmdbuf);
               } else if (runscript->when == SCRIPT_After &&     /* client run after job */
                          runscript->on_success &&
                         !runscript->on_failure &&
                         !runscript->fail_on_error &&
                          !bstrcmp(runscript->target, "")) {
                  Mmsg(temp, "client run after job = \"%s\"\n", cmdbuf);
               } else if (runscript->when == SCRIPT_Before &&      /* client run before job */
                          !bstrcmp(runscript->target, "")) {
                  Mmsg(temp, "before job = \"%s\"\n", cmdbuf);
               } else if (runscript->when == SCRIPT_After &&      /* run after failed job */
                          runscript->on_failure &&
                         !runscript->on_success &&
                         !runscript->fail_on_error &&
                          bstrcmp(runscript->target, "")) {
                  Mmsg(temp, "run after failed job = \"%s\"\n", cmdbuf);
               }
               indent_config_item(cfg_str, 1, temp.c_str());
            } else {
               Mmsg(temp, "runscript {\n");
               indent_config_item(cfg_str, 1, temp.c_str());

               char *cmdstring = (char *)"command"; /* '|' */
               if (runscript->cmd_type == '@') {
                  cmdstring = (char *)"console";
               }

               Mmsg(temp, "%s = \"%s\"\n", cmdstring, cmdbuf);
               indent_config_item(cfg_str, 2, temp.c_str());

               /*
                * Default: never
                */
               char *when = (char *)"never";
               switch (runscript->when) {
                  case SCRIPT_Before:
                     when  = (char *)"before";
                     break;
                  case SCRIPT_After:
                     when = (char *)"after";
                     break;
                  case SCRIPT_AfterVSS:
                     when = (char *)"aftervss";
                     break;
                  case SCRIPT_Any:
                     when = (char *)"always";
                     break;
               }

               if (! bstrcmp(when, "never")) { /* suppress default value */
                  Mmsg(temp, "runswhen = %s\n", when);
                  indent_config_item(cfg_str, 2, temp.c_str());
               }

               /*
                * Default: fail_on_error = true
                */
               char *fail_on_error = (char *)"Yes";
               if (!runscript->fail_on_error){
                  fail_on_error = (char *)"No";
                  Mmsg(temp, "failonerror = %s\n", fail_on_error);
                  indent_config_item(cfg_str, 2, temp.c_str());
               }

               /*
                * Default: on_success = true
                */
               char *run_on_success = (char *)"Yes";
               if (!runscript->on_success){
                  run_on_success = (char *)"No";
                  Mmsg(temp, "runsonsuccess = %s\n", run_on_success);
                  indent_config_item(cfg_str, 2, temp.c_str());
               }

               /*
                * Default: on_failure = false
                */
               char *run_on_failure = (char *)"No";
               if (runscript->on_failure) {
                  run_on_failure = (char *)"Yes";
                  Mmsg(temp, "runsonfailure = %s\n", run_on_failure);
                  indent_config_item(cfg_str, 2, temp.c_str());
               }

               /* level is not implemented
               Dmsg1(200, "   level = %d\n", runscript->level);
               */

               /*
                * Default: runsonclient = yes
                */
               char *runsonclient = (char *)"Yes";
               if (bstrcmp(runscript->target, "")) {
                  runsonclient = (char *)"No";
                  Mmsg(temp, "runsonclient = %s\n", runsonclient);
                  indent_config_item(cfg_str, 2, temp.c_str());
               }

               indent_config_item(cfg_str, 1, "}\n");
            }

            free_pool_memory(cmdbuf);
         }
      } /* foreach runscript */
   }
}

static inline void print_config_run(RES_ITEM *item, POOL_MEM &cfg_str)
{
   POOL_MEM temp;
   RUNRES *run;
   bool all_set;
   int interval_start;
   char *weekdays[] = {
      (char *)"Sun",
      (char *)"Mon",
      (char *)"Tue",
      (char *)"Wed",
      (char *)"Thu",
      (char *)"Fri",
      (char *)"Sat"
   };
   char *ordinals[] = {
      (char *)"1st",
      (char *)"2nd",
      (char *)"3rd",
      (char *)"4th",
      (char *)"5th"
   };

   run = (RUNRES *)*(item->value);
   if (run != NULL) {
      while (run) {
         POOL_MEM run_str; /* holds the complete run= ... line */

         indent_config_item(cfg_str, 1, "run = ");

         /*
          * Overrides
          */
         if (run->pool) {
            Mmsg(temp, "pool=\"%s\" ", run->pool->name());
            pm_strcat(run_str, temp.c_str());
         }

         if (run->full_pool) {
            Mmsg(temp, "fullpool=\"%s\" ", run->full_pool->name());
            pm_strcat(run_str, temp.c_str());
         }

         if (run->inc_pool) {
            Mmsg(temp, "incrementalpool=\"%s\" ", run->inc_pool->name());
            pm_strcat(run_str, temp.c_str());
         }

         if (run->diff_pool) {
            Mmsg(temp, "differentialpool=\"%s\" ", run->diff_pool->name());
            pm_strcat(run_str, temp.c_str());
         }

         if (run->next_pool) {
            Mmsg(temp, "nextpool=\"%s\" ", run->next_pool->name());
            pm_strcat(run_str, temp.c_str());
         }

         if (run->level) {
            for (int j = 0; joblevels[j].level_name; j++) {
               if (joblevels[j].level == run->level) {
                  Mmsg(temp, "level=%s ", joblevels[j].level_name);
                  pm_strcat(run_str, temp.c_str());
                  break;
               }
            }
         }

         if (run->storage) {
            Mmsg(temp, "storage=\"%s\" ", run->storage->name());
            pm_strcat(run_str, temp.c_str());
         }

         if (run->msgs) {
            Mmsg(temp, "messages=\"%s\" ", run->msgs->name());
            pm_strcat(run_str, temp.c_str());
         }

         if (run->Priority && run->Priority != 10) {
            Mmsg(temp, "priority=%d ", run->Priority);
            pm_strcat(run_str, temp.c_str());
         }

         if (run->MaxRunSchedTime) {
            Mmsg(temp, "maxrunschedtime=%d ", run->MaxRunSchedTime);
            pm_strcat(run_str, temp.c_str());
         }

         if (run->accurate) {
            /*
             * TODO: You cannot distinct if accurate was not set or if it was set to no
             *       maybe we need an additional variable like "accurate_set".
             */
            Mmsg(temp, "accurate=\"%s\" ","yes");
            pm_strcat(run_str, temp.c_str());
         }

         /*
          * Now the time specification
          */

         /*
          * run->mday , output is just the number comma separated
          */
         all_set = true;
         interval_start = -1;

         POOL_MEM t; /* is one entry of day/month/week etc. */

         pm_strcpy(temp, "");
         for (int i = 0; i < 32; i++) {  /* search for one more than needed to detect also intervals that stop on last value  */
            if (bit_is_set(i, run->mday)) {
               if (interval_start == -1) {   // bit is set and we are not in an interval
                  interval_start = i;        // start an interval
                  Dmsg1(200, "starting interval at %d\n", i+1);
                  Mmsg(t, ",%d", i+1);
                  pm_strcat(temp, t.c_str());
               }
            }

            if (!bit_is_set(i, run->mday)) {
               if (interval_start != -1) {   // bit is unset and we are in an interval
                  if ((i - interval_start) > 1) {
                     Dmsg2(200, "found end of interval from %d to %d\n", interval_start+1, i);
                     Mmsg(t, "-%d", i);
                     pm_strcat(temp, t.c_str());

                  }
                  interval_start = -1;       // end the interval
               }
            }

            if ((!bit_is_set(i, run->mday)) && (i != 31)) {
               all_set = false;
            }
         }

         if (!all_set) { // suppress output if all bits are set
            pm_strcat(temp, " ");
            pm_strcat(run_str, temp.c_str() + 1); /* jump over first comma*/
            all_set = true;
         }

         /*
          * run->wom output is 1st, 2nd... 5th comma separated
          *                    first, second, third... is also allowed
          *                    but we ignore that for now
          */
         interval_start = -1;

         all_set = true;
         pm_strcpy(temp, "");
         for (int i = 0; i < 5; i++) {
            if (bit_is_set(i, run->wom)) {
               if (interval_start == -1) {   // bit is set and we are not in an interval
                  interval_start = i;        // start an interval
                  Dmsg1(200, "starting interval at %s\n", ordinals[i]);
                  Mmsg(t, ",%s", ordinals[i]);
                  pm_strcat(temp, t.c_str());
               }
            }

            if (! bit_is_set(i, run->wom)) {
               if (interval_start != -1) {   // bit is unset and we are in an interval
                  if ((i - interval_start) > 1) {
                     Dmsg2(200, "found end of interval from %s to %s\n", ordinals[interval_start], ordinals[i-1]);
                     Mmsg(t, "-%s", ordinals[i-1]);
                     pm_strcat(temp, t.c_str());

                  }
                  interval_start = -1;       // end the interval
               }
            }

            if ((!bit_is_set(i, run->wom)) && (i != 5)) {
               all_set = false;
            }
         }


         if (!all_set) { // suppress output if all bits are set
            pm_strcat(temp, " ");
            pm_strcat(run_str, temp.c_str() + 1); /* jump over first comma*/
            all_set = true;
         }


         /*
          * run->wday output is Sun, Mon, ..., Sat comma separated
          */
         all_set = true;
         pm_strcpy(temp, "");
         for (int i = 0; i < 7; i++) {
            if (bit_is_set(i, run->wday)) {
               if (interval_start == -1) {   // bit is set and we are not in an interval
                  interval_start = i;        // start an interval
                  Dmsg1(200, "starting interval at %s\n", weekdays[i]);
                  Mmsg(t, ",%s", weekdays[i]);
                  pm_strcat(temp, t.c_str());
               }
            }

            if (!bit_is_set(i, run->wday)) {
               if (interval_start != -1) {   // bit is unset and we are in an interval
                  if ((i - interval_start) > 1) {
                     Dmsg2(200, "found end of interval from %s to %s\n", weekdays[interval_start], weekdays[i-1]);
                     Mmsg(t, "-%s", weekdays[i-1]);
                     pm_strcat(temp, t.c_str());

                  }
                  interval_start = -1;       // end the interval
               }
            }

            if ((! bit_is_set(i, run->wday)) && (i != 7)) {
               all_set = false;
            }
         }

         if (!all_set) { // suppress output if all bits are set
            pm_strcat(temp, " ");
            pm_strcat(run_str, temp.c_str() + 1); /* jump over first comma*/
            all_set = true;
         }

         /*
          * run->woy output is w00 - w53, comma separated
          */
         all_set = true;
         pm_strcpy(temp, "");
         for (int i = 0; i < 55; i++) {
            if (bit_is_set(i, run->woy)) {
               if (interval_start == -1) {   // bit is set and we are not in an interval
                  interval_start = i;        // start an interval
                  Dmsg1(200, "starting interval at w%02d\n", i);
                  Mmsg(t, ",w%02d", i);
                  pm_strcat(temp, t.c_str());
               }
            }
            if (! bit_is_set(i, run->woy)) {
               if (interval_start != -1) {   // bit is unset and we are in an interval
                  if ((i - interval_start) > 1) {
                     Dmsg2(200, "found end of interval from w%02d to w%02d\n", interval_start, i-1);
                     Mmsg(t, "-w%02d", i-1);
                     pm_strcat(temp, t.c_str());

                  }
                  interval_start = -1;       // end the interval
               }
            }

            if ((! bit_is_set(i, run->woy)) && (i != 54)) {
                  all_set = false;
            }
         }

         if (!all_set) { // suppress output if all bits are set
            pm_strcat(temp, " ");
            pm_strcat(run_str, temp.c_str() + 1); /* jump over first comma*/
            all_set = true;
         }

         /*
          * run->hour output is HH:MM for hour and minute though its a bitfield.
          * only "hourly" sets all bits.
          */
         all_set = true;
         pm_strcpy(temp, "");
         for (int i = 0; i < 24; i++) {
            if bit_is_set(i, run->hour) {
               Mmsg(temp, "at %02d:%02d\n", i, run->minute);
               pm_strcat(run_str, temp.c_str());
            } else {
               all_set = false;
            }
         }

         /* run->minute output is smply the minute in HH:MM */
         pm_strcat(cfg_str, run_str.c_str());
         all_set = true;

         run = run->next;
      } // loop over runs
   }
}

bool FILESETRES::print_config(POOL_MEM &buff)
{
   POOL_MEM cfg_str;
   POOL_MEM temp;
   const char *p;

   Dmsg0(200,"FILESETRES::print_config\n");

   Mmsg(temp, "FileSet {\n");
   pm_strcat(cfg_str, temp.c_str());

   Mmsg(temp, "Name = \"%s\"\n", this->name());
   indent_config_item(cfg_str, 1, temp.c_str());

   if (num_includes) {
      /*
       * Loop over all exclude blocks.
       */
      for (int i = 0;  i < num_includes; i++) {
         INCEXE *incexe = include_items[i];

         Mmsg(temp, "Include {\n");
         indent_config_item(cfg_str, 1, temp.c_str());

         /*
          * Start options block
          */
         if (incexe->num_opts > 0) {
            Mmsg(temp, "Options {\n");
            indent_config_item(cfg_str, 2, temp.c_str());

            for (int j = 0; j < incexe->num_opts; j++) {
               FOPTS *fo = incexe->opts_list[j];
               bool enhanced_wild = false;

               for (int k = 0; fo->opts[k] != '\0'; k++) {
                  if (fo->opts[k]=='W') {
                     enhanced_wild = true;
                     break;
                  }
               }

               for (p = &fo->opts[0]; *p; p++) {
                  switch (*p) {
                     case '0':                 /* no option */
                        break;
                     case 'a':                 /* alway replace */
                        indent_config_item(cfg_str, 3, "Replace = Always\n");
                        break;
                     case 'C':                 /* */
                        indent_config_item(cfg_str, 3, "Accurate = ");
                        p++;                   /* skip C */
                        for (; *p && *p != ':'; p++) {
                           Mmsg(temp, "%c", *p);
                           pm_strcat(cfg_str, temp.c_str());
                        }
                        pm_strcat(cfg_str, "\n");
                        break;
                     case 'd':
                        switch(*(p + 1)) {
                        case '1':
                           indent_config_item(cfg_str, 3, "Shadowing = LocalWarn\n");
                           p++;
                           break;
                        case '2':
                           indent_config_item(cfg_str, 3, "Shadowing = LocalRemove\n");
                           p++;
                           break;
                        case '3':
                           indent_config_item(cfg_str, 3, "Shadowing = GlobalWarn\n");
                           p++;
                           break;
                        case '4':
                           indent_config_item(cfg_str, 3, "Shadowing = GlobalRemove\n");
                           p++;
                           break;
                        }
                        break;
                     case 'e':
                        indent_config_item(cfg_str, 3, "Exclude = yes\n");
                        break;
                     case 'f':
                        indent_config_item(cfg_str, 3, "OneFS = no\n");
                        break;
                     case 'h':                 /* no recursion */
                        indent_config_item(cfg_str, 3, "Recurse = no\n");
                        break;
                     case 'H':                 /* no hard link handling */
                        indent_config_item(cfg_str, 3, "Hardlinks = no\n");
                        break;
                     case 'i':
                        indent_config_item(cfg_str, 3, "IgnoreCase = yes\n");
                        break;
                     case 'J':                 /* Base Job */
                        indent_config_item(cfg_str, 3, "BaseJob = ");
                        p++;                   /* skip J */
                        for (; *p && *p != ':'; p++) {
                           Mmsg(temp, "%c", *p);
                           pm_strcat(cfg_str, temp.c_str());
                        }
                        pm_strcat(cfg_str, "\n");
                        break;
                     case 'M':                 /* MD5 */
                        indent_config_item(cfg_str, 3, "Signature = MD5\n");
                        break;
                     case 'n':
                        indent_config_item(cfg_str, 3, "Replace = Never\n");
                        break;
                     case 'p':                 /* use portable data format */
                        indent_config_item(cfg_str, 3, "Portable = Yes\n");
                        break;
                     case 'P':                 /* strip path */
                        indent_config_item(cfg_str, 3, "Strip = ");
                        p++;                   /* skip P */
                        for (; *p && *p != ':'; p++) {
                           Mmsg(temp, "%c", *p);
                           pm_strcat(cfg_str, temp.c_str());
                        }
                        pm_strcat(cfg_str, "\n");
                        break;
                     case 'R':                 /* Resource forks and Finder Info */
                        indent_config_item(cfg_str, 3, "HFSPlusSupport = Yes\n");
                        break;
                     case 'r':                 /* read fifo */
                        indent_config_item(cfg_str, 3, "ReadFifo = Yes\n");
                        break;
                     case 'S':
                        switch(*(p + 1)) {
                        case ' ':
                           /* Old director did not specify SHA variant */
                           break;
                        case '1':
                           indent_config_item(cfg_str, 3, "Signature = SHA1\n");
                           p++;
                           break;
#ifdef HAVE_SHA2
                        case '2':
                           indent_config_item(cfg_str, 3, "Signature = SHA256\n");
                           p++;
                           break;
                        case '3':
                           indent_config_item(cfg_str, 3, "Signature = SHA512\n");
                           p++;
                           break;
#endif
                        default:
                           /* Automatically downgrade to SHA-1 if an unsupported
                            * SHA variant is specified */
                           indent_config_item(cfg_str, 3, "Signature = SHA1\n");
                           p++;
                           break;
                        }
                        break;
                     case 's':
                        indent_config_item(cfg_str, 3, "Sparse = Yes\n");
                        break;
                     case 'm':
                        indent_config_item(cfg_str, 3, "MtimeOnly = Yes\n");
                        break;
                     case 'k':
                        indent_config_item(cfg_str, 3, "KeepAtime = Yes\n");
                        break;
                     case 'K':
                        indent_config_item(cfg_str, 3, "NoAtime = Yes\n");
                        break;
                     case 'A':
                        indent_config_item(cfg_str, 3, "AclSupport = Yes\n");
                        break;
                     case 'V':                  /* verify options */
                        indent_config_item(cfg_str, 3, "Verify = ");
                        p++;                   /* skip V */
                        for (; *p && *p != ':'; p++) {
                           Mmsg(temp, "%c", *p);
                           pm_strcat(cfg_str, temp.c_str());
                        }
                        pm_strcat(cfg_str, "\n");
                        break;
                     case 'w':
                        indent_config_item(cfg_str, 3, "Replace = IfNewer\n");
                        break;
                     case 'W':
                        indent_config_item(cfg_str, 3, "EnhancedWild = Yes\n");
                        break;
                     case 'z':                 /* size */
                        indent_config_item(cfg_str, 3, "Size = ");
                        p++;                   /* skip z */
                        for (; *p && *p != ':'; p++) {
                           Mmsg(temp, "%c", *p);
                           pm_strcat(cfg_str, temp.c_str());
                        }
                        pm_strcat(cfg_str, "\n");
                        break;
                     case 'Z':                 /* compression */
                        indent_config_item(cfg_str, 3, "Compression = ");
                        p++;                   /* skip Z */
                        if (*p >= '0' && *p <= '9') {
                           Mmsg(temp, "GZIP\n");
                           pm_strcat(cfg_str, temp.c_str());
                           Mmsg(temp, "%c\n", *p);
                           pm_strcat(cfg_str, temp.c_str());
                           p++; /* skip number */
                        } else if (*p == 'o') {
                           Mmsg(temp, "LZO\n");
                           pm_strcat(cfg_str, temp.c_str());
                           break;
                        } else if (*p == 'f') {
                           p++;
                           if (*p == 'f') {
                              Mmsg(temp, "LZFAST\n");
                              pm_strcat(cfg_str, temp.c_str());
                              break;
                           } else if (*p == '4') {
                              Mmsg(temp, "LZ4\n");
                              pm_strcat(cfg_str, temp.c_str());
                              break;
                           } else if (*p == 'h') {
                              Mmsg(temp, "LZ4HC\n");
                              pm_strcat(cfg_str, temp.c_str());
                              break;
                           }
                        }
                        break;
                     case 'X':
                        indent_config_item(cfg_str, 3, "Xattr = Yes\n");
                        break;
                     default:
                        Emsg1(M_ERROR, 0, _("Unknown include/exclude option: %c\n"), *p);
                        break;
                  }
               }

               for (int k = 0; k < fo->regex.size(); k++) {
                  Mmsg(temp, "Regex = \"%s\"\n", fo->regex.get(k));
                  indent_config_item(cfg_str, 3, temp.c_str());
               }

               for (int k = 0; k < fo->regexdir.size(); k++) {
                  Mmsg(temp, "Regex Dir = \"%s\"\n", fo->regexdir.get(k));
                  indent_config_item(cfg_str, 3, temp.c_str());
               }

               for (int k = 0; k < fo->regexfile.size(); k++) {
                  Mmsg(temp, "Regex File = \"%s\"\n", fo->regexfile.get(k));
                  indent_config_item(cfg_str, 3, temp.c_str());
               }

               for (int k = 0; k < fo->wild.size(); k++) {
                  Mmsg(temp, "Wild = \"%s\"\n", fo->wild.get(k));
                  indent_config_item(cfg_str, 3, temp.c_str());
               }

               for (int k = 0; k < fo->wilddir.size(); k++) {
                  Mmsg(temp, "Wild Dir = \"%s\"\n", fo->wilddir.get(k));
                  indent_config_item(cfg_str, 3, temp.c_str());
               }

               for (int k = 0; k < fo->wildfile.size(); k++) {
                  Mmsg(temp, "Wild File = \"%s\"\n", fo->wildfile.get(k));
                  indent_config_item(cfg_str, 3, temp.c_str());
               }

               for (int k = 0; k < fo->wildbase.size(); k++) {
                  Mmsg(temp, "Wild Base = \"%c %s\"\n", enhanced_wild ? 'B' : 'F', fo->wildbase.get(k));
                  indent_config_item(cfg_str, 3, temp.c_str());
               }

               for (int k = 0; k < fo->base.size(); k++) {
                  Mmsg(temp, "Base = \"%s\"\n", fo->base.get(k));
                  indent_config_item(cfg_str, 3, temp.c_str());
               }

               for (int k = 0; k < fo->fstype.size(); k++) {
                  Mmsg(temp, "Fs Type = \"%s\"\n", fo->fstype.get(k));
                  indent_config_item(cfg_str, 3, temp.c_str());
               }

               for (int k = 0; k < fo->drivetype.size(); k++) {
                  Mmsg(temp, "Drive Type = \"%s\"\n", fo->drivetype.get(k));
                  indent_config_item(cfg_str, 3, temp.c_str());
               }

               for (int k = 0; k < fo->meta.size(); k++) {
                  Mmsg(temp, "Meta = \"%s\"\n", fo->meta.get(k));
                  indent_config_item(cfg_str, 3, temp.c_str());
               }

               if (fo->plugin) {
                  Mmsg(temp, "Plugin = \"%s\"\n", fo->plugin);
                  indent_config_item(cfg_str, 3, temp.c_str());
               }

               if (fo->reader) {
                  Mmsg(temp, "Reader = \"%s\"\n", fo->reader);
                  indent_config_item(cfg_str, 3, temp.c_str());
               }

               if (fo->writer) {
                  Mmsg(temp, "Writer = \"%s\"\n", fo->writer);
                  indent_config_item(cfg_str, 3, temp.c_str());
               }
            }

            if (incexe->num_opts > 0) {
               indent_config_item(cfg_str, 2, "}\n");
            }
         } /* end options block */

         /*
          * File = entries.
          */
         if (incexe->name_list.size()) {
            for (int l = 0; l < incexe->name_list.size(); l++) {
               Mmsg(temp, "File = \"%s\"\n", incexe->name_list.get(l));
               indent_config_item(cfg_str, 2, temp.c_str());
            }
         }

         /*
          * Plugin = entries.
          */
         if (incexe->plugin_list.size()) {
            for (int l = 0; l < incexe->plugin_list.size(); l++) {
               Mmsg(temp, "Plugin = %s\n", incexe->plugin_list.get(l));
               indent_config_item(cfg_str, 2, temp.c_str());
            }
         }

         /*
          * Exclude Dir Containing = entry.
          */
         if (incexe->ignoredir.size()) {
            for (int l = 0; l < incexe->ignoredir.size(); l++) {
               Mmsg(temp, "Exclude Dir Containing = \"%s\"\n", incexe->ignoredir.get(l));
               indent_config_item(cfg_str, 2, temp.c_str());
            }
         }

         indent_config_item(cfg_str, 1, "}\n");

         /*
          * End Include block
          */
      } /* loop over all include blocks */
   }

   if (num_excludes) {
      /*
       * Loop over all exclude blocks.
       */
      for (int j = 0; j < num_excludes; j++) {
         INCEXE *incexe = exclude_items[j];

         if (incexe->name_list.size()) {
            indent_config_item(cfg_str, 1, "Exclude {\n");

            for (int k = 0; k < incexe->name_list.size(); k++) {
               Mmsg(temp, "File = \"%s\"\n", incexe->name_list.get(k));
               indent_config_item(cfg_str, 2, temp.c_str());
            }

            indent_config_item(cfg_str, 1, "}\n");
         }
      } /* loop over all exclude blocks */
   }

   pm_strcat(cfg_str, "}\n\n");
   pm_strcat(buff, cfg_str.c_str());

   return true;
}

const char *auth_protocol_to_str(uint32_t auth_protocol)
{
   for (int i = 0; authprotocols[i].name; i++) {
      if (authprotocols[i].token == auth_protocol) {
         return authprotocols[i].name;
      }
   }

   return "Unknown";
}

const char *level_to_str(int level)
{
   static char level_no[30];
   const char *str = level_no;

   bsnprintf(level_no, sizeof(level_no), "%c (%d)", level, level); /* default if not found */
   for (int i = 0; joblevels[i].level_name; i++) {
      if (level == (int)joblevels[i].level) {
         str = joblevels[i].level_name;
         break;
      }
   }

   return str;
}

/*
 * Dump contents of resource
 */
void dump_resource(int type, RES *ures, void sendit(void *sock, const char *fmt, ...), void *sock)
{
   URES *res = (URES *)ures;
   bool recurse = true;
   POOL_MEM buf;
   UAContext *ua = (UAContext *)sock;

   if (res == NULL) {
      sendit(sock, _("No %s resource defined\n"), res_to_str(type));
      return;
   }

   if (type < 0) { /* no recursion */
      type = -type;
      recurse = false;
   }

   switch (type) {
   case R_DIRECTOR:
      res->res_dir.print_config(buf);
      sendit(sock, "%s", buf.c_str());
      break;
   case R_CONSOLE:
      res->res_con.print_config(buf);
      sendit(sock, "%s", buf.c_str());
      break;
   case R_COUNTER:
      res->res_counter.print_config(buf);
      sendit(sock, "%s", buf.c_str());
      break;
   case R_CLIENT:
      if (!ua || acl_access_ok(ua, Client_ACL, res->res_client.hdr.name)) {
         res->res_client.print_config(buf);
         sendit(sock, "%s", buf.c_str());
      }
      break;
   case R_DEVICE:
      res->res_dev.print_config(buf);
      sendit(sock, "%s", buf.c_str());
      break;
   case R_STORAGE:
      if (!ua || acl_access_ok(ua, Storage_ACL, res->res_store.hdr.name)) {
         res->res_store.print_config(buf);
         sendit(sock, "%s", buf.c_str());
      }
      break;
   case R_CATALOG:
      if (!ua || acl_access_ok(ua, Catalog_ACL, res->res_cat.hdr.name)) {
         res->res_cat.print_config(buf);
         sendit(sock, "%s", buf.c_str());
      }
      break;
   case R_JOBDEFS:
   case R_JOB:
      if (!ua || acl_access_ok(ua, Job_ACL, res->res_job.hdr.name)) {
         res->res_job.print_config(buf);
         sendit(sock, "%s", buf.c_str());
      }
      break;
   case R_FILESET: {
      if (!ua || acl_access_ok(ua, FileSet_ACL, res->res_fs.hdr.name)) {
         res->res_fs.print_config(buf);
         sendit(sock, "%s", buf.c_str());
      }
      break;
   }
   case R_SCHEDULE:
      if (!ua || acl_access_ok(ua, Schedule_ACL, res->res_sch.hdr.name)) {
         res->res_sch.print_config(buf);
         sendit(sock, "%s", buf.c_str());
      }
      break;
   case R_POOL:
      if (!ua || acl_access_ok(ua, Pool_ACL, res->res_pool.hdr.name)) {
        res->res_pool.print_config(buf);
        sendit(sock, "%s", buf.c_str());
      }
      break;
   case R_MSGS:
      res->res_msgs.print_config(buf);
      sendit(sock, "%s", buf.c_str());
      break;
   default:
      sendit(sock, _("Unknown resource type %d in dump_resource.\n"), type);
      break;
   }

   if (recurse && res->res_dir.hdr.next) {
      dump_resource(type, res->res_dir.hdr.next, sendit, sock);
   }
}

/*
 * Free all the members of an INCEXE structure
 */
static void free_incexe(INCEXE *incexe)
{
   incexe->name_list.destroy();
   incexe->plugin_list.destroy();
   for (int i = 0; i < incexe->num_opts; i++) {
      FOPTS *fopt = incexe->opts_list[i];
      fopt->regex.destroy();
      fopt->regexdir.destroy();
      fopt->regexfile.destroy();
      fopt->wild.destroy();
      fopt->wilddir.destroy();
      fopt->wildfile.destroy();
      fopt->wildbase.destroy();
      fopt->base.destroy();
      fopt->fstype.destroy();
      fopt->drivetype.destroy();
      fopt->meta.destroy();
      if (fopt->plugin) {
         free(fopt->plugin);
      }
      if (fopt->reader) {
         free(fopt->reader);
      }
      if (fopt->writer) {
         free(fopt->writer);
      }
      free(fopt);
   }
   if (incexe->opts_list) {
      free(incexe->opts_list);
   }
   incexe->ignoredir.destroy();
   free(incexe);
}

/*
 * Free memory of resource -- called when daemon terminates.
 * NB, we don't need to worry about freeing any references
 * to other resources as they will be freed when that
 * resource chain is traversed.  Mainly we worry about freeing
 * allocated strings (names).
 */
void free_resource(RES *sres, int type)
{
   int num;
   RES *nres; /* next resource if linked */
   URES *res = (URES *)sres;

   if (res == NULL)
      return;

   /*
    * Common stuff -- free the resource name and description
    */
   nres = (RES *)res->res_dir.hdr.next;
   if (res->res_dir.hdr.name) {
      free(res->res_dir.hdr.name);
   }
   if (res->res_dir.hdr.desc) {
      free(res->res_dir.hdr.desc);
   }

   switch (type) {
   case R_DIRECTOR:
      if (res->res_dir.working_directory) {
         free(res->res_dir.working_directory);
      }
      if (res->res_dir.scripts_directory) {
         free(res->res_dir.scripts_directory);
      }
      if (res->res_dir.plugin_directory) {
         free(res->res_dir.plugin_directory);
      }
      if (res->res_dir.plugin_names) {
         delete res->res_dir.plugin_names;
      }
      if (res->res_dir.pid_directory) {
         free(res->res_dir.pid_directory);
      }
      if (res->res_dir.subsys_directory) {
         free(res->res_dir.subsys_directory);
      }
      if (res->res_dir.backend_directories) {
         delete res->res_dir.backend_directories;
      }
      if (res->res_dir.password.value) {
         free(res->res_dir.password.value);
      }
      if (res->res_dir.query_file) {
         free(res->res_dir.query_file);
      }
      if (res->res_dir.DIRaddrs) {
         free_addresses(res->res_dir.DIRaddrs);
      }
      if (res->res_dir.DIRsrc_addr) {
         free_addresses(res->res_dir.DIRsrc_addr);
      }
      if (res->res_dir.tls_ctx) {
         free_tls_context(res->res_dir.tls_ctx);
      }
      if (res->res_dir.tls_ca_certfile) {
         free(res->res_dir.tls_ca_certfile);
      }
      if (res->res_dir.tls_ca_certdir) {
         free(res->res_dir.tls_ca_certdir);
      }
      if (res->res_dir.tls_crlfile) {
         free(res->res_dir.tls_crlfile);
      }
      if (res->res_dir.tls_certfile) {
         free(res->res_dir.tls_certfile);
      }
      if (res->res_dir.tls_keyfile) {
         free(res->res_dir.tls_keyfile);
      }
      if (res->res_dir.tls_dhfile) {
         free(res->res_dir.tls_dhfile);
      }
      if (res->res_dir.tls_allowed_cns) {
         delete res->res_dir.tls_allowed_cns;
      }
      if (res->res_dir.verid) {
         free(res->res_dir.verid);
      }
      if (res->res_dir.keyencrkey.value) {
         free(res->res_dir.keyencrkey.value);
      }
      if (res->res_dir.audit_events) {
         delete res->res_dir.audit_events;
      }
      break;
   case R_DEVICE:
   case R_COUNTER:
       break;
   case R_CONSOLE:
      if (res->res_con.password.value) {
         free(res->res_con.password.value);
      }
      if (res->res_con.tls_ctx) {
         free_tls_context(res->res_con.tls_ctx);
      }
      if (res->res_con.tls_ca_certfile) {
         free(res->res_con.tls_ca_certfile);
      }
      if (res->res_con.tls_ca_certdir) {
         free(res->res_con.tls_ca_certdir);
      }
      if (res->res_con.tls_crlfile) {
         free(res->res_con.tls_crlfile);
      }
      if (res->res_con.tls_certfile) {
         free(res->res_con.tls_certfile);
      }
      if (res->res_con.tls_keyfile) {
         free(res->res_con.tls_keyfile);
      }
      if (res->res_con.tls_dhfile) {
         free(res->res_con.tls_dhfile);
      }
      if (res->res_con.tls_allowed_cns) {
         delete res->res_con.tls_allowed_cns;
      }
      for (int i = 0; i < Num_ACL; i++) {
         if (res->res_con.ACL_lists[i]) {
            delete res->res_con.ACL_lists[i];
            res->res_con.ACL_lists[i] = NULL;
         }
      }
      break;
   case R_CLIENT:
      if (res->res_client.address) {
         free(res->res_client.address);
      }
      if (res->res_client.username) {
         free(res->res_client.username);
      }
      if (res->res_client.password.value) {
         free(res->res_client.password.value);
      }
      if (res->res_client.tls_ctx) {
         free_tls_context(res->res_client.tls_ctx);
      }
      if (res->res_client.tls_ca_certfile) {
         free(res->res_client.tls_ca_certfile);
      }
      if (res->res_client.tls_ca_certdir) {
         free(res->res_client.tls_ca_certdir);
      }
      if (res->res_client.tls_crlfile) {
         free(res->res_client.tls_crlfile);
      }
      if (res->res_client.tls_certfile) {
         free(res->res_client.tls_certfile);
      }
      if (res->res_client.tls_keyfile) {
         free(res->res_client.tls_keyfile);
      }
      if (res->res_client.tls_allowed_cns) {
         delete res->res_client.tls_allowed_cns;
      }
      break;
   case R_STORAGE:
      if (res->res_store.address) {
         free(res->res_store.address);
      }
      if (res->res_store.username) {
         free(res->res_store.username);
      }
      if (res->res_store.password.value) {
         free(res->res_store.password.value);
      }
      if (res->res_store.media_type) {
         free(res->res_store.media_type);
      }
      if (res->res_store.device) {
         delete res->res_store.device;
      }
      if (res->res_store.tls_ctx) {
         free_tls_context(res->res_store.tls_ctx);
      }
      if (res->res_store.tls_ca_certfile) {
         free(res->res_store.tls_ca_certfile);
      }
      if (res->res_store.tls_ca_certdir) {
         free(res->res_store.tls_ca_certdir);
      }
      if (res->res_store.tls_crlfile) {
         free(res->res_store.tls_crlfile);
      }
      if (res->res_store.tls_certfile) {
         free(res->res_store.tls_certfile);
      }
      if (res->res_store.tls_keyfile) {
         free(res->res_store.tls_keyfile);
      }
      break;
   case R_CATALOG:
      if (res->res_cat.db_address) {
         free(res->res_cat.db_address);
      }
      if (res->res_cat.db_socket) {
         free(res->res_cat.db_socket);
      }
      if (res->res_cat.db_user) {
         free(res->res_cat.db_user);
      }
      if (res->res_cat.db_name) {
         free(res->res_cat.db_name);
      }
      if (res->res_cat.db_driver) {
         free(res->res_cat.db_driver);
      }
      if (res->res_cat.db_password.value) {
         free(res->res_cat.db_password.value);
      }
      break;
   case R_FILESET:
      if ((num=res->res_fs.num_includes)) {
         while (--num >= 0) {
            free_incexe(res->res_fs.include_items[num]);
         }
         free(res->res_fs.include_items);
      }
      res->res_fs.num_includes = 0;
      if ((num=res->res_fs.num_excludes)) {
         while (--num >= 0) {
            free_incexe(res->res_fs.exclude_items[num]);
         }
         free(res->res_fs.exclude_items);
      }
      res->res_fs.num_excludes = 0;
      break;
   case R_POOL:
      if (res->res_pool.pool_type) {
         free(res->res_pool.pool_type);
      }
      if (res->res_pool.label_format) {
         free(res->res_pool.label_format);
      }
      if (res->res_pool.cleaning_prefix) {
         free(res->res_pool.cleaning_prefix);
      }
      if (res->res_pool.storage) {
         delete res->res_pool.storage;
      }
      break;
   case R_SCHEDULE:
      if (res->res_sch.run) {
         RUNRES *nrun, *next;
         nrun = res->res_sch.run;
         while (nrun) {
            next = nrun->next;
            free(nrun);
            nrun = next;
         }
      }
      break;
   case R_JOBDEFS:
   case R_JOB:
      if (res->res_job.backup_format) {
         free(res->res_job.backup_format);
      }
      if (res->res_job.RestoreWhere) {
         free(res->res_job.RestoreWhere);
      }
      if (res->res_job.RegexWhere) {
         free(res->res_job.RegexWhere);
      }
      if (res->res_job.strip_prefix) {
         free(res->res_job.strip_prefix);
      }
      if (res->res_job.add_prefix) {
         free(res->res_job.add_prefix);
      }
      if (res->res_job.add_suffix) {
         free(res->res_job.add_suffix);
      }
      if (res->res_job.RestoreBootstrap) {
         free(res->res_job.RestoreBootstrap);
      }
      if (res->res_job.WriteBootstrap) {
         free(res->res_job.WriteBootstrap);
      }
      if (res->res_job.WriteVerifyList) {
         free(res->res_job.WriteVerifyList);
      }
      if (res->res_job.PluginOptions) {
         free(res->res_job.PluginOptions);
      }
      if (res->res_job.selection_pattern) {
         free(res->res_job.selection_pattern);
      }
      if (res->res_job.run_cmds) {
         delete res->res_job.run_cmds;
      }
      if (res->res_job.storage) {
         delete res->res_job.storage;
      }
      if (res->res_job.base) {
         delete res->res_job.base;
      }
      if (res->res_job.RunScripts) {
         free_runscripts(res->res_job.RunScripts);
         delete res->res_job.RunScripts;
      }
      break;
   case R_MSGS:
      if (res->res_msgs.mail_cmd) {
         free(res->res_msgs.mail_cmd);
      }
      if (res->res_msgs.operator_cmd) {
         free(res->res_msgs.operator_cmd);
      }
      free_msgs_res((MSGSRES *)res); /* free message resource */
      res = NULL;
      break;
   default:
      printf(_("Unknown resource type %d in free_resource.\n"), type);
   }
   /*
    * Common stuff again -- free the resource, recurse to next one
    */
   if (res) {
      free(res);
   }
   if (nres) {
      free_resource(nres, type);
   }
}

/*
 * Save the new resource by chaining it into the head list for
 * the resource. If this is pass 2, we update any resource
 * pointers because they may not have been defined until
 * later in pass 1.
 */
void save_resource(int type, RES_ITEM *items, int pass)
{
   URES *res;
   int rindex = type - R_FIRST;
   bool error = false;

   /*
    * Check Job requirements after applying JobDefs
    */
   if (type != R_JOBDEFS && type != R_JOB) {
      /*
       * Ensure that all required items are present
       */
      for (int i = 0; items[i].name; i++) {
         if (items[i].flags & CFG_ITEM_REQUIRED) {
            if (!bit_is_set(i, res_all.res_dir.hdr.item_present)) {
                Emsg2(M_ERROR_TERM, 0,
                      _("%s item is required in %s resource, but not found.\n"),
                      items[i].name, resources[rindex]);
            }
         }
         /*
          * If this triggers, take a look at lib/parse_conf.h
          */
         if (i >= MAX_RES_ITEMS) {
            Emsg1(M_ERROR_TERM, 0, _("Too many items in %s resource\n"), resources[rindex]);
         }
      }
   } else if (type == R_JOB) {
      /*
       * Ensure that the name item is present
       */
      if (items[0].flags & CFG_ITEM_REQUIRED) {
         if (!bit_is_set(0, res_all.res_dir.hdr.item_present)) {
             Emsg2(M_ERROR_TERM, 0,
                   _("%s item is required in %s resource, but not found.\n"),
                   items[0].name, resources[rindex]);
         }
      }
   }

   /*
    * During pass 2 in each "store" routine, we looked up pointers
    * to all the resources referrenced in the current resource, now we
    * must copy their addresses from the static record to the allocated
    * record.
    */
   if (pass == 2) {
      switch (type) {
      case R_CATALOG:
      case R_MSGS:
      case R_FILESET:
      case R_DEVICE:
         /*
          * Resources not containing a resource
          */
         break;
      case R_POOL:
         /*
          * Resources containing another resource or alist. First
          * look up the resource which contains another resource. It
          * was written during pass 1.  Then stuff in the pointers to
          * the resources it contains, which were inserted this pass.
          * Finally, it will all be stored back.
          *
          * Find resource saved in pass 1
          */
         if ((res = (URES *)GetResWithName(R_POOL, res_all.res_con.hdr.name)) == NULL) {
            Emsg1(M_ERROR_TERM, 0, _("Cannot find Pool resource %s\n"), res_all.res_con.hdr.name);
         } else {
            /*
             * Explicitly copy resource pointers from this pass (res_all)
             */
            res->res_pool.NextPool = res_all.res_pool.NextPool;
            res->res_pool.RecyclePool = res_all.res_pool.RecyclePool;
            res->res_pool.ScratchPool = res_all.res_pool.ScratchPool;
            res->res_pool.storage = res_all.res_pool.storage;
            if (res_all.res_pool.catalog || !res->res_pool.use_catalog) {
               res->res_pool.catalog = res_all.res_pool.catalog;
            }
         }
         break;
      case R_CONSOLE:
         if ((res = (URES *)GetResWithName(R_CONSOLE, res_all.res_con.hdr.name)) == NULL) {
            Emsg1(M_ERROR_TERM, 0, _("Cannot find Console resource %s\n"), res_all.res_con.hdr.name);
         } else {
            res->res_con.tls_allowed_cns = res_all.res_con.tls_allowed_cns;
         }
         break;
      case R_DIRECTOR:
         if ((res = (URES *)GetResWithName(R_DIRECTOR, res_all.res_dir.hdr.name)) == NULL) {
            Emsg1(M_ERROR_TERM, 0, _("Cannot find Director resource %s\n"), res_all.res_dir.hdr.name);
         } else {
            res->res_dir.plugin_names = res_all.res_dir.plugin_names;
            res->res_dir.messages = res_all.res_dir.messages;
            res->res_dir.backend_directories = res_all.res_dir.backend_directories;
            res->res_dir.tls_allowed_cns = res_all.res_dir.tls_allowed_cns;
         }
         break;
      case R_STORAGE:
         if ((res = (URES *)GetResWithName(type, res_all.res_store.hdr.name)) == NULL) {
            Emsg1(M_ERROR_TERM, 0, _("Cannot find Storage resource %s\n"), res_all.res_dir.hdr.name);
         } else {
            res->res_store.paired_storage = res_all.res_store.paired_storage;

            /*
             * We must explicitly copy the device alist pointer
             */
            res->res_store.device = res_all.res_store.device;
         }
         break;
      case R_JOBDEFS:
      case R_JOB:
         if ((res = (URES *)GetResWithName(type, res_all.res_dir.hdr.name)) == NULL) {
            Emsg1(M_ERROR_TERM, 0, _("Cannot find Job resource %s\n"), res_all.res_dir.hdr.name);
         } else {
            res->res_job.messages = res_all.res_job.messages;
            res->res_job.schedule = res_all.res_job.schedule;
            res->res_job.client = res_all.res_job.client;
            res->res_job.fileset = res_all.res_job.fileset;
            res->res_job.storage = res_all.res_job.storage;
            res->res_job.catalog = res_all.res_job.catalog;
            res->res_job.base = res_all.res_job.base;
            res->res_job.pool = res_all.res_job.pool;
            res->res_job.full_pool = res_all.res_job.full_pool;
            res->res_job.inc_pool = res_all.res_job.inc_pool;
            res->res_job.diff_pool = res_all.res_job.diff_pool;
            res->res_job.next_pool = res_all.res_job.next_pool;
            res->res_job.verify_job = res_all.res_job.verify_job;
            res->res_job.jobdefs = res_all.res_job.jobdefs;
            res->res_job.run_cmds = res_all.res_job.run_cmds;
            res->res_job.RunScripts = res_all.res_job.RunScripts;

            /*
             * TODO: JobDefs where/regexwhere doesn't work well (but this is not very useful)
             * We have to set_bit(index, res_all.hdr.item_present); or something like that
             *
             * We take RegexWhere before all other options
             */
            if (!res->res_job.RegexWhere &&
                (res->res_job.strip_prefix ||
                 res->res_job.add_suffix ||
                 res->res_job.add_prefix)) {
               int len = bregexp_get_build_where_size(res->res_job.strip_prefix,
                                                      res->res_job.add_prefix,
                                                      res->res_job.add_suffix);
               res->res_job.RegexWhere = (char *) bmalloc (len * sizeof(char));
               bregexp_build_where(res->res_job.RegexWhere, len,
                                   res->res_job.strip_prefix,
                                   res->res_job.add_prefix,
                                   res->res_job.add_suffix);
               /*
                * TODO: test bregexp
                */
            }

            if (res->res_job.RegexWhere && res->res_job.RestoreWhere) {
               free(res->res_job.RestoreWhere);
               res->res_job.RestoreWhere = NULL;
            }
         }
         break;
      case R_COUNTER:
         if ((res = (URES *)GetResWithName(R_COUNTER, res_all.res_counter.hdr.name)) == NULL) {
            Emsg1(M_ERROR_TERM, 0, _("Cannot find Counter resource %s\n"), res_all.res_counter.hdr.name);
         } else {
            res->res_counter.Catalog = res_all.res_counter.Catalog;
            res->res_counter.WrapCounter = res_all.res_counter.WrapCounter;
         }
         break;
      case R_CLIENT:
         if ((res = (URES *)GetResWithName(R_CLIENT, res_all.res_client.hdr.name)) == NULL) {
            Emsg1(M_ERROR_TERM, 0, _("Cannot find Client resource %s\n"), res_all.res_client.hdr.name);
         } else {
            if (res_all.res_client.catalog) {
               res->res_client.catalog = res_all.res_client.catalog;
            } else {
               /*
                * No catalog overwrite given use the first catalog definition.
                */
               res->res_client.catalog = (CATRES *)GetNextRes(R_CATALOG, NULL);
            }
            res->res_client.tls_allowed_cns = res_all.res_client.tls_allowed_cns;
         }
         break;
      case R_SCHEDULE:
         /*
          * Schedule is a bit different in that it contains a RUNRES record
          * chain which isn't a "named" resource. This chain was linked
          * in by run_conf.c during pass 2, so here we jam the pointer
          * into the Schedule resource.
          */
         if ((res = (URES *)GetResWithName(R_SCHEDULE, res_all.res_client.hdr.name)) == NULL) {
            Emsg1(M_ERROR_TERM, 0, _("Cannot find Schedule resource %s\n"), res_all.res_client.hdr.name);
         } else {
            res->res_sch.run = res_all.res_sch.run;
         }
         break;
      default:
         Emsg1(M_ERROR, 0, _("Unknown resource type %d in save_resource.\n"), type);
         error = true;
         break;
      }

      /*
       * Note, the resource name was already saved during pass 1,
       * so here, we can just release it.
       */
      if (res_all.res_dir.hdr.name) {
         free(res_all.res_dir.hdr.name);
         res_all.res_dir.hdr.name = NULL;
      }

      if (res_all.res_dir.hdr.desc) {
         free(res_all.res_dir.hdr.desc);
         res_all.res_dir.hdr.desc = NULL;
      }

      return;
   }

   /*
    * Common
    */
   if (!error) {
      res = (URES *)malloc(resources[rindex].size);
      memcpy(res, &res_all, resources[rindex].size);
      if (!res_head[rindex]) {
         res_head[rindex] = (RES *)res; /* store first entry */
         Dmsg3(900, "Inserting first %s res: %s index=%d\n", res_to_str(type),
               res->res_dir.hdr.name, rindex);
      } else {
         RES *next, *last;
         if (res->res_dir.hdr.name == NULL) {
            Emsg1(M_ERROR_TERM, 0, _("Name item is required in %s resource, but not found.\n"),
                  resources[rindex]);
         }
         /*
          * Add new res to end of chain
          */
         for (last = next = res_head[rindex]; next; next = next->next) {
            last = next;
            if (bstrcmp(next->name, res->res_dir.hdr.name)) {
               Emsg2(M_ERROR_TERM, 0,
                  _("Attempt to define second %s resource named \"%s\" is not permitted.\n"),
                  resources[rindex].name, res->res_dir.hdr.name);
            }
         }
         last->next = (RES *)res;
         Dmsg4(900, _("Inserting %s res: %s index=%d pass=%d\n"), res_to_str(type),
               res->res_dir.hdr.name, rindex, pass);
      }
   }
}

bool populate_jobdefs()
{
   JOBRES *job;
   bool retval = true;

   foreach_res(job, R_JOB) {
      if (job->jobdefs) {
         /*
          * Handle Storage alists specifically
          */
         JOBRES *jobdefs = job->jobdefs;
         if (jobdefs->storage && !job->storage) {
            STORERES *store;

            job->storage = New(alist(10, not_owned_by_alist));
            foreach_alist(store, jobdefs->storage) {
               job->storage->append(store);
            }
         }

         /*
          * Handle RunScripts alists specifically
          */
         if (jobdefs->RunScripts) {
            RUNSCRIPT *rs, *elt;

            if (!job->RunScripts) {
               job->RunScripts = New(alist(10, not_owned_by_alist));
            }

            foreach_alist(rs, jobdefs->RunScripts) {
               elt = copy_runscript(rs);
               elt->from_jobdef = true;
               job->RunScripts->append(elt); /* we have to free it */
            }
         }

         /*
          * Transfer default items from JobDefs Resource
          */
         for (int i = 0; job_items[i].name; i++) {
            char **def_svalue, **svalue;   /* string value */
            uint32_t *def_ivalue, *ivalue; /* integer value */
            bool *def_bvalue, *bvalue;     /* bool value */
            int64_t *def_lvalue, *lvalue;  /* 64 bit values */
            uint32_t offset;

            Dmsg4(1400, "Job \"%s\", field \"%s\" bit=%d def=%d\n",
                job->name(), job_items[i].name,
                bit_is_set(i, job->hdr.item_present),
                bit_is_set(i, job->jobdefs->hdr.item_present));

            if (!bit_is_set(i, job->hdr.item_present) &&
                 bit_is_set(i, job->jobdefs->hdr.item_present)) {
               Dmsg2(400, "Job \"%s\", field \"%s\": getting default.\n",
                     job->name(), job_items[i].name);
               offset = (char *)(job_items[i].value) - (char *)&res_all;
               switch (job_items[i].type) {
               case CFG_TYPE_STR:
               case CFG_TYPE_DIR:
                  /*
                   * Handle strings and directory strings
                   */
                  def_svalue = (char **)((char *)(job->jobdefs) + offset);
                  Dmsg5(400, "Job \"%s\", field \"%s\" def_svalue=%s item %d offset=%u\n",
                        job->name(), job_items[i].name, *def_svalue, i, offset);
                  svalue = (char **)((char *)job + offset);
                  if (*svalue) {
                     free(*svalue);
                  }
                  *svalue = bstrdup(*def_svalue);
                  set_bit(i, job->hdr.item_present);
                  set_bit(i, job->hdr.inherit_content);
                  break;
               case CFG_TYPE_RES:
                  /*
                   * Handle resources
                   */
                  def_svalue = (char **)((char *)(job->jobdefs) + offset);
                  Dmsg4(400, "Job \"%s\", field \"%s\" item %d offset=%u\n",
                        job->name(), job_items[i].name, i, offset);
                  svalue = (char **)((char *)job + offset);
                  if (*svalue) {
                     Pmsg1(000, _("Hey something is wrong. p=0x%lu\n"), *svalue);
                  }
                  *svalue = *def_svalue;
                  set_bit(i, job->hdr.item_present);
                  set_bit(i, job->hdr.inherit_content);
                  break;
               case CFG_TYPE_ALIST_RES:
                  /*
                   * Handle alist resources
                   */
                  if (bit_is_set(i, job->jobdefs->hdr.item_present)) {
                     set_bit(i, job->hdr.item_present);
                     set_bit(i, job->hdr.inherit_content);
                  }
                  break;
               case CFG_TYPE_BIT:
               case CFG_TYPE_PINT32:
               case CFG_TYPE_JOBTYPE:
               case CFG_TYPE_PROTOCOLTYPE:
               case CFG_TYPE_LEVEL:
               case CFG_TYPE_INT32:
               case CFG_TYPE_SIZE32:
               case CFG_TYPE_MIGTYPE:
               case CFG_TYPE_REPLACE:
                  /*
                   * Handle integer fields
                   *    Note, our store_bit does not handle bitmaped fields
                   */
                  def_ivalue = (uint32_t *)((char *)(job->jobdefs) + offset);
                  Dmsg5(400, "Job \"%s\", field \"%s\" def_ivalue=%d item %d offset=%u\n",
                       job->name(), job_items[i].name, *def_ivalue, i, offset);
                  ivalue = (uint32_t *)((char *)job + offset);
                  *ivalue = *def_ivalue;
                  set_bit(i, job->hdr.item_present);
                  set_bit(i, job->hdr.inherit_content);
                  break;
               case CFG_TYPE_TIME:
               case CFG_TYPE_SIZE64:
               case CFG_TYPE_INT64:
               case CFG_TYPE_SPEED:
                  /*
                   * Handle 64 bit integer fields
                   */
                  def_lvalue = (int64_t *)((char *)(job->jobdefs) + offset);
                  Dmsg5(400, "Job \"%s\", field \"%s\" def_lvalue=%" lld " item %d offset=%u\n",
                       job->name(), job_items[i].name, *def_lvalue, i, offset);
                  lvalue = (int64_t *)((char *)job + offset);
                  *lvalue = *def_lvalue;
                  set_bit(i, job->hdr.item_present);
                  set_bit(i, job->hdr.inherit_content);
                  break;
               case CFG_TYPE_BOOL:
                  /*
                   * Handle bool fields
                   */
                  def_bvalue = (bool *)((char *)(job->jobdefs) + offset);
                  Dmsg5(400, "Job \"%s\", field \"%s\" def_bvalue=%d item %d offset=%u\n",
                       job->name(), job_items[i].name, *def_bvalue, i, offset);
                  bvalue = (bool *)((char *)job + offset);
                  *bvalue = *def_bvalue;
                  set_bit(i, job->hdr.item_present);
                  set_bit(i, job->hdr.inherit_content);
                  break;
               default:
                  break;
               }
            }
         }
      }

      /*
       * Ensure that all required items are present
       */
      for (int i = 0; job_items[i].name; i++) {
         if (job_items[i].flags & CFG_ITEM_REQUIRED) {
            if (!bit_is_set(i, job->hdr.item_present)) {
               Jmsg(NULL, M_ERROR_TERM, 0,
                    _("\"%s\" directive in Job \"%s\" resource is required, but not found.\n"),
                    job_items[i].name, job->name());
               retval = false;
               goto bail_out;
            }
         }

         /*
          * If this triggers, take a look at lib/parse_conf.h
          */
         if (i >= MAX_RES_ITEMS) {
            Emsg0(M_ERROR_TERM, 0, _("Too many items in Job resource\n"));
            goto bail_out;
         }
      }

      /*
       * For Copy and Migrate we can have Jobs without a client or fileset.
       * As for a copy we use the original Job as a reference for the Read storage
       * we also don't need to check if there is an explicit storage definition in
       * either the Job or the Read pool.
       */
      switch (job->JobType) {
      case JT_COPY:
      case JT_MIGRATE:
         break;
      default:
         /*
          * All others must have a client and fileset.
          */
         if (!job->client) {
            Jmsg(NULL, M_ERROR_TERM, 0,
                 _("\"client\" directive in Job \"%s\" resource is required, but not found.\n"), job->name());
            retval = false;
            goto bail_out;
         }

         if (!job->fileset) {
            Jmsg(NULL, M_ERROR_TERM, 0,
                 _("\"fileset\" directive in Job \"%s\" resource is required, but not found.\n"), job->name());
            retval = false;
            goto bail_out;
         }

         if (!job->storage && !job->pool->storage) {
            Jmsg(NULL, M_FATAL, 0, _("No storage specified in Job \"%s\" nor in Pool.\n"), job->name());
            retval = false;
            goto bail_out;
         }
         break;
      }
   } /* End loop over Job res */

bail_out:
   return retval;
}

static void store_actiononpurge(LEX *lc, RES_ITEM *item, int index, int pass)
{
   int i;
   uint32_t *destination = item->ui32value;

   lex_get_token(lc, T_NAME);
   /*
    * Scan ActionOnPurge options
    */
   for (i = 0; ActionOnPurgeOptions[i].name; i++) {
      if (bstrcasecmp(lc->str, ActionOnPurgeOptions[i].name)) {
         *destination = (*destination) | ActionOnPurgeOptions[i].token;
         i = 0;
         break;
      }
   }

   if (i != 0) {
      scan_err1(lc, _("Expected an Action On Purge option, got: %s"), lc->str);
   }

   scan_to_eol(lc);
   set_bit(index, res_all.hdr.item_present);
   clear_bit(index, res_all.hdr.inherit_content);
}

/*
 * Store Device. Note, the resource is created upon the
 * first reference. The details of the resource are obtained
 * later from the SD.
 */
static void store_device(LEX *lc, RES_ITEM *item, int index, int pass)
{
   URES *res;
   int rindex = R_DEVICE - R_FIRST;
   bool found = false;

   if (pass == 1) {
      lex_get_token(lc, T_NAME);
      if (!res_head[rindex]) {
         res = (URES *)malloc(resources[rindex].size);
         memset(res, 0, resources[rindex].size);
         res->res_dev.hdr.name = bstrdup(lc->str);
         res_head[rindex] = (RES *)res; /* store first entry */
         Dmsg3(900, "Inserting first %s res: %s index=%d\n", res_to_str(R_DEVICE),
               res->res_dir.hdr.name, rindex);
      } else {
         RES *next;
         /*
          * See if it is already defined
          */
         for (next = res_head[rindex]; next->next; next = next->next) {
            if (bstrcmp(next->name, lc->str)) {
               found = true;
               break;
            }
         }
         if (!found) {
            res = (URES *)malloc(resources[rindex].size);
            memset(res, 0, resources[rindex].size);
            res->res_dev.hdr.name = bstrdup(lc->str);
            next->next = (RES *)res;
            Dmsg4(900, "Inserting %s res: %s index=%d pass=%d\n", res_to_str(R_DEVICE),
               res->res_dir.hdr.name, rindex, pass);
         }
      }

      scan_to_eol(lc);
      set_bit(index, res_all.hdr.item_present);
      clear_bit(index, res_all.hdr.inherit_content);
   } else {
      store_resource(CFG_TYPE_ALIST_RES, lc, item, index, pass);
   }
}

/*
 * Store Migration/Copy type
 */
static void store_migtype(LEX *lc, RES_ITEM *item, int index, int pass)
{
   int i;

   lex_get_token(lc, T_NAME);
   /*
    * Store the type both pass 1 and pass 2
    */
   for (i = 0; migtypes[i].type_name; i++) {
      if (bstrcasecmp(lc->str, migtypes[i].type_name)) {
         *(item->ui32value) = migtypes[i].job_type;
         i = 0;
         break;
      }
   }

   if (i != 0) {
      scan_err1(lc, _("Expected a Migration Job Type keyword, got: %s"), lc->str);
   }

   scan_to_eol(lc);
   set_bit(index, res_all.hdr.item_present);
   clear_bit(index, res_all.hdr.inherit_content);
}

/*
 * Store JobType (backup, verify, restore)
 */
static void store_jobtype(LEX *lc, RES_ITEM *item, int index, int pass)
{
   int i;

   lex_get_token(lc, T_NAME);
   /*
    * Store the type both pass 1 and pass 2
    */
   for (i = 0; jobtypes[i].type_name; i++) {
      if (bstrcasecmp(lc->str, jobtypes[i].type_name)) {
         *(item->ui32value) = jobtypes[i].job_type;
         i = 0;
         break;
      }
   }

   if (i != 0) {
      scan_err1(lc, _("Expected a Job Type keyword, got: %s"), lc->str);
   }

   scan_to_eol(lc);
   set_bit(index, res_all.hdr.item_present);
   clear_bit(index, res_all.hdr.inherit_content);
}

/*
 * Store Protocol (Native, NDMP)
 */
static void store_protocoltype(LEX *lc, RES_ITEM *item, int index, int pass)
{
   int i;

   lex_get_token(lc, T_NAME);
   /*
    * Store the type both pass 1 and pass 2
    */
   for (i = 0; backupprotocols[i].name; i++) {
      if (bstrcasecmp(lc->str, backupprotocols[i].name)) {
         *(item->ui32value) = backupprotocols[i].token;
         i = 0;
         break;
      }
   }

   if (i != 0) {
      scan_err1(lc, _("Expected a Protocol Type keyword, got: %s"), lc->str);
   }

   scan_to_eol(lc);
   set_bit(index, res_all.hdr.item_present);
   clear_bit(index, res_all.hdr.inherit_content);
}

static void store_replace(LEX *lc, RES_ITEM *item, int index, int pass)
{
   int i;

   lex_get_token(lc, T_NAME);
   /*
    * Scan Replacement options
    */
   for (i = 0; ReplaceOptions[i].name; i++) {
      if (bstrcasecmp(lc->str, ReplaceOptions[i].name)) {
         *(item->ui32value) = ReplaceOptions[i].token;
         i = 0;
         break;
      }
   }

   if (i != 0) {
      scan_err1(lc, _("Expected a Restore replacement option, got: %s"), lc->str);
   }

   scan_to_eol(lc);
   set_bit(index, res_all.hdr.item_present);
   clear_bit(index, res_all.hdr.inherit_content);
}

/*
 * Store Auth Protocol (Native, NDMPv2, NDMPv3, NDMPv4)
 */
static void store_authprotocoltype(LEX *lc, RES_ITEM *item, int index, int pass)
{
   int i;

   lex_get_token(lc, T_NAME);
   /*
    * Store the type both pass 1 and pass 2
    */
   for (i = 0; authprotocols[i].name; i++) {
      if (bstrcasecmp(lc->str, authprotocols[i].name)) {
         *(item->ui32value) = authprotocols[i].token;
         i = 0;
         break;
      }
   }

   if (i != 0) {
      scan_err1(lc, _("Expected a Auth Protocol Type keyword, got: %s"), lc->str);
   }
   scan_to_eol(lc);
   set_bit(index, res_all.hdr.item_present);
   clear_bit(index, res_all.hdr.inherit_content);
}

/*
 * Store authentication type (Mostly for NDMP like clear or MD5).
 */
static void store_authtype(LEX *lc, RES_ITEM *item, int index, int pass)
{
   int i;

   lex_get_token(lc, T_NAME);
   /*
    * Store the type both pass 1 and pass 2
    */
   for (i = 0; authmethods[i].name; i++) {
      if (bstrcasecmp(lc->str, authmethods[i].name)) {
         *(item->ui32value) = authmethods[i].token;
         i = 0;
         break;
      }
   }

   if (i != 0) {
      scan_err1(lc, _("Expected a Authentication Type keyword, got: %s"), lc->str);
   }

   scan_to_eol(lc);
   set_bit(index, res_all.hdr.item_present);
   clear_bit(index, res_all.hdr.inherit_content);
}

/*
 * Store Job Level (Full, Incremental, ...)
 */
static void store_level(LEX *lc, RES_ITEM *item, int index, int pass)
{
   int i;

   lex_get_token(lc, T_NAME);
   /*
    * Store the level pass 2 so that type is defined
    */
   for (i = 0; joblevels[i].level_name; i++) {
      if (bstrcasecmp(lc->str, joblevels[i].level_name)) {
         *(item->ui32value) = joblevels[i].level;
         i = 0;
         break;
      }
   }

   if (i != 0) {
      scan_err1(lc, _("Expected a Job Level keyword, got: %s"), lc->str);
   }

   scan_to_eol(lc);
   set_bit(index, res_all.hdr.item_present);
   clear_bit(index, res_all.hdr.inherit_content);
}

/*
 * Store password either clear if for NDMP and catalog or MD5 hashed for native.
 */
static void store_autopassword(LEX *lc, RES_ITEM *item, int index, int pass)
{
   switch (res_all.hdr.rcode) {
   case R_DIRECTOR:
      /*
       * As we need to store both clear and MD5 hashed within the same
       * resource class we use the item->code as a hint default is 0
       * and for clear we need a code of 1.
       */
      switch (item->code) {
      case 1:
         store_resource(CFG_TYPE_CLEARPASSWORD, lc, item, index, pass);
         break;
      default:
         store_resource(CFG_TYPE_MD5PASSWORD, lc, item, index, pass);
         break;
      }
      break;
   case R_CLIENT:
      switch (res_all.res_client.Protocol) {
      case APT_NDMPV2:
      case APT_NDMPV3:
      case APT_NDMPV4:
         store_resource(CFG_TYPE_CLEARPASSWORD, lc, item, index, pass);
         break;
      default:
         store_resource(CFG_TYPE_MD5PASSWORD, lc, item, index, pass);
         break;
      }
      break;
   case R_STORAGE:
      switch (res_all.res_store.Protocol) {
      case APT_NDMPV2:
      case APT_NDMPV3:
      case APT_NDMPV4:
         store_resource(CFG_TYPE_CLEARPASSWORD, lc, item, index, pass);
         break;
      default:
         store_resource(CFG_TYPE_MD5PASSWORD, lc, item, index, pass);
         break;
      }
      break;
   case R_CATALOG:
      store_resource(CFG_TYPE_CLEARPASSWORD, lc, item, index, pass);
      break;
   default:
      store_resource(CFG_TYPE_MD5PASSWORD, lc, item, index, pass);
      break;
   }
}

/*
 * Store ACL (access control list)
 */
static void store_acl(LEX *lc, RES_ITEM *item, int index, int pass)
{
   int token;
   alist *list;

   if (pass == 1) {
      if (!item->alistvalue[item->code]) {
         item->alistvalue[item->code] = New(alist(10, owned_by_alist));
         Dmsg1(900, "Defined new ACL alist at %d\n", item->code);
      }
   }
   list = item->alistvalue[item->code];

   for (;;) {
      lex_get_token(lc, T_STRING);
      if (pass == 1) {
         list->append(bstrdup(lc->str));
         Dmsg2(900, "Appended to %d %s\n", item->code, lc->str);
      }
      token = lex_get_token(lc, T_ALL);
      if (token == T_COMMA) {
         continue; /* get another ACL */
      }
      break;
   }
   set_bit(index, res_all.hdr.item_present);
   clear_bit(index, res_all.hdr.inherit_content);
}

/*
 * Store Audit event.
 */
static void store_audit(LEX *lc, RES_ITEM *item, int index, int pass)
{
   int token;
   alist *list;

   if (pass == 1) {
      if (!*item->alistvalue) {
         *(item->alistvalue) = New(alist(10, owned_by_alist));
      }
   }
   list = *item->alistvalue;

   for (;;) {
      lex_get_token(lc, T_STRING);
      if (pass == 1) {
         list->append(bstrdup(lc->str));
      }
      token = lex_get_token(lc, T_ALL);
      if (token == T_COMMA) {
         continue;
      }
      break;
   }
   set_bit(index, res_all.hdr.item_present);
   clear_bit(index, res_all.hdr.inherit_content);
}
/*
 * Store a runscript->when in a bit field
 */
static void store_runscript_when(LEX *lc, RES_ITEM *item, int index, int pass)
{
   lex_get_token(lc, T_NAME);

   if (bstrcasecmp(lc->str, "before")) {
      *(item->ui32value) = SCRIPT_Before;
   } else if (bstrcasecmp(lc->str, "after")) {
      *(item->ui32value) = SCRIPT_After;
   } else if (bstrcasecmp(lc->str, "aftervss")) {
      *(item->ui32value) = SCRIPT_AfterVSS;
   } else if (bstrcasecmp(lc->str, "always")) {
      *(item->ui32value) = SCRIPT_Any;
   } else {
      scan_err2(lc, _("Expect %s, got: %s"), "Before, After, AfterVSS or Always", lc->str);
   }
   scan_to_eol(lc);
}

/*
 * Store a runscript->target
 */
static void store_runscript_target(LEX *lc, RES_ITEM *item, int index, int pass)
{
   lex_get_token(lc, T_STRING);

   if (pass == 2) {
      if (bstrcmp(lc->str, "%c")) {
         ((RUNSCRIPT *)item->value)->set_target(lc->str);
      } else if (bstrcasecmp(lc->str, "yes")) {
         ((RUNSCRIPT *)item->value)->set_target("%c");
      } else if (bstrcasecmp(lc->str, "no")) {
         ((RUNSCRIPT *)item->value)->set_target("");
      } else {
         RES *res = GetResWithName(R_CLIENT, lc->str);
         if (res == NULL) {
            scan_err3(lc, _("Could not find config Resource %s referenced on line %d : %s\n"),
                      lc->str, lc->line_no, lc->line);
         }

         ((RUNSCRIPT *)item->value)->set_target(lc->str);
      }
   }
   scan_to_eol(lc);
}

/*
 * Store a runscript->command as a string and runscript->cmd_type as a pointer
 */
static void store_runscript_cmd(LEX *lc, RES_ITEM *item, int index, int pass)
{
   lex_get_token(lc, T_STRING);

   if (pass == 2) {
      Dmsg2(1, "runscript cmd=%s type=%c\n", lc->str, item->code);
      POOLMEM *c = get_pool_memory(PM_FNAME);
      /*
       * Each runscript command takes 2 entries in commands list
       */
      pm_strcpy(c, lc->str);
      ((RUNSCRIPT *)item->value)->commands->prepend(c); /* command line */
      ((RUNSCRIPT *)item->value)->commands->prepend((void *)(intptr_t)item->code); /* command type */
   }
   scan_to_eol(lc);
}

static void store_short_runscript(LEX *lc, RES_ITEM *item, int index, int pass)
{
   lex_get_token(lc, T_STRING);
   alist **runscripts = item->alistvalue;

   if (pass == 2) {
      RUNSCRIPT *script = new_runscript();
      script->set_job_code_callback(job_code_callback_director);

      script->set_command(lc->str);
      if (bstrcasecmp(item->name, "runbeforejob")) {
         script->when = SCRIPT_Before;
         script->set_target("");
      } else if (bstrcasecmp(item->name, "runafterjob")) {
         script->when = SCRIPT_After;
         script->on_success = true;
         script->on_failure = false;
         script->fail_on_error = false;
         script->set_target("");
      } else if (bstrcasecmp(item->name, "clientrunafterjob")) {
         script->when = SCRIPT_After;
         script->on_success = true;
         script->on_failure = false;
         script->fail_on_error = false;
         script->set_target("%c");
      } else if (bstrcasecmp(item->name, "clientrunbeforejob")) {
         script->when = SCRIPT_Before;
         script->set_target("%c");
      } else if (bstrcasecmp(item->name, "runafterfailedjob")) {
         script->when = SCRIPT_After;
         script->on_failure = true;
         script->on_success = false;
         script->fail_on_error = false;
         script->set_target("");
      }

      /*
       * Remember that the entry was configured in the short runscript form.
       */
      script->short_form = true;

      if (*runscripts == NULL) {
        *runscripts = New(alist(10, not_owned_by_alist));
      }

      (*runscripts)->append(script);
      script->debug();
   }

   scan_to_eol(lc);
}

/*
 * Store a bool in a bit field without modifing res_all.hdr
 * We can also add an option to store_bool to skip res_all.hdr
 */
static void store_runscript_bool(LEX *lc, RES_ITEM *item, int index, int pass)
{
   lex_get_token(lc, T_NAME);
   if (bstrcasecmp(lc->str, "yes") || bstrcasecmp(lc->str, "true")) {
      *(item->boolvalue) = true;
   } else if (bstrcasecmp(lc->str, "no") || bstrcasecmp(lc->str, "false")) {
      *(item->boolvalue) = false;
   } else {
      scan_err2(lc, _("Expect %s, got: %s"), "YES, NO, TRUE, or FALSE", lc->str); /* YES and NO must not be translated */
   }
   scan_to_eol(lc);
}

/*
 * Store RunScript info
 *
 * Note, when this routine is called, we are inside a Job
 * resource.  We treat the RunScript like a sort of
 * mini-resource within the Job resource.
 */
static void store_runscript(LEX *lc, RES_ITEM *item, int index, int pass)
{
   char *c;
   int token, i, t;
   alist **runscripts = item->alistvalue;

   Dmsg1(200, "store_runscript: begin store_runscript pass=%i\n", pass);

   token = lex_get_token(lc, T_SKIP_EOL);

   if (token != T_BOB) {
      scan_err1(lc, _("Expecting open brace. Got %s"), lc->str);
   }
   /*
    * Setting on_success, on_failure, fail_on_error
    */
   res_runscript.reset_default();

   if (pass == 2) {
      res_runscript.commands = New(alist(10, not_owned_by_alist));
   }

   while ((token = lex_get_token(lc, T_SKIP_EOL)) != T_EOF) {
      if (token == T_EOB) {
        break;
      }

      if (token != T_IDENTIFIER) {
        scan_err1(lc, _("Expecting keyword, got: %s\n"), lc->str);
      }

      for (i = 0; runscript_items[i].name; i++) {
        if (bstrcasecmp(runscript_items[i].name, lc->str)) {
           token = lex_get_token(lc, T_SKIP_EOL);
           if (token != T_EQUALS) {
              scan_err1(lc, _("Expected an equals, got: %s"), lc->str);
           }

           /*
            * Call item handler
            */
           switch (runscript_items[i].type) {
           case CFG_TYPE_RUNSCRIPT_CMD:
              store_runscript_cmd(lc, &runscript_items[i], i, pass);
              break;
           case CFG_TYPE_RUNSCRIPT_TARGET:
              store_runscript_target(lc, &runscript_items[i], i, pass);
              break;
           case CFG_TYPE_RUNSCRIPT_BOOL:
              store_runscript_bool(lc, &runscript_items[i], i, pass);
              break;
           case CFG_TYPE_RUNSCRIPT_WHEN:
              store_runscript_when(lc, &runscript_items[i], i, pass);
              break;
           default:
              break;
           }
           i = -1;
           break;
        }
      }

      if (i >=0) {
        scan_err1(lc, _("Keyword %s not permitted in this resource"), lc->str);
      }
   }

   if (pass == 2) {
      /*
       * Run on client by default
       */
      if (res_runscript.target == NULL) {
         res_runscript.set_target("%c");
      }
      if (*runscripts == NULL) {
         *runscripts = New(alist(10, not_owned_by_alist));
      }
      /*
       * commands list contains 2 values per command
       * - POOLMEM command string (ex: /bin/true)
       * - int command type (ex: SHELL_CMD)
       */
      res_runscript.set_job_code_callback(job_code_callback_director);
      while ((c = (char *)res_runscript.commands->pop()) != NULL) {
         t = (intptr_t)res_runscript.commands->pop();
         RUNSCRIPT *script = new_runscript();
         memcpy(script, &res_runscript, sizeof(RUNSCRIPT));
         script->command = c;
         script->cmd_type = t;
         /*
          * target is taken from res_runscript,
          * each runscript object have a copy
          */
         script->target = NULL;
         script->set_target(res_runscript.target);

         /*
          * Remember that the entry was configured in the short runscript form.
          */
         script->short_form = false;

         (*runscripts)->append(script);
         script->debug();
      }
      delete res_runscript.commands;
      /*
       * setting on_success, on_failure... cleanup target field
       */
      res_runscript.reset_default(true);
   }

   scan_to_eol(lc);
   set_bit(index, res_all.hdr.item_present);
   clear_bit(index, res_all.hdr.inherit_content);
}

/*
 * callback function for edit_job_codes
 * See ../lib/util.c, function edit_job_codes, for more remaining codes
 *
 * %f = Filesetname
 * %h = Client Address
 * %p = Poolname
 * %w = Write storage
 * %x = Spooling (yes/no)
 * %C = Cloning (yes/no)
 * %D = Director name
 * %V = Volume name(s) (Destination)
 */
extern "C" char *job_code_callback_director(JCR *jcr, const char *param)
{
   static char yes[] = "yes";
   static char no[] = "no";

   switch (param[0]) {
   case 'f':
      if (jcr->res.fileset) {
         return jcr->res.fileset->name();
      }
      break;
   case 'h':
      if (jcr->res.client) {
         return jcr->res.client->address;
      }
      break;
   case 'p':
      if (jcr->res.pool) {
         return jcr->res.pool->name();
      }
      break;
   case 'w':
      if (jcr->res.wstore) {
         return jcr->res.wstore->name();
      }
      break;
   case 'x':
      return jcr->spool_data ? yes : no;
   case 'C':
      return jcr->cloned ? yes : no;
   case 'D':
      return my_name;
   case 'V':
      if (jcr) {
         /*
          * If this is a migration/copy we need the volume name from the mig_jcr.
          */
         if (jcr->mig_jcr) {
            jcr = jcr->mig_jcr;
         }

         if (jcr->VolumeName) {
            return jcr->VolumeName;
         } else {
            return (char *)_("*none*");
         }
      } else {
         return (char *)_("*none*");
      }
      break;
   }

   return NULL;
}

/*
 * callback function for init_resource
 * See ../lib/parse_conf.c, function init_resource, for more generic handling.
 */
static void init_resource_cb(RES_ITEM *item, int pass)
{
   switch (pass) {
   case 1:
      switch (item->type) {
      case CFG_TYPE_REPLACE:
         for (int i = 0; ReplaceOptions[i].name; i++) {
            if (bstrcasecmp(item->default_value, ReplaceOptions[i].name)) {
               *(item->ui32value) = ReplaceOptions[i].token;
            }
         }
         break;
      case CFG_TYPE_AUTHPROTOCOLTYPE:
         for (int i = 0; authprotocols[i].name; i++) {
            if (bstrcasecmp(item->default_value, authprotocols[i].name)) {
               *(item->ui32value) = authprotocols[i].token;
            }
         }
         break;
      case CFG_TYPE_AUTHTYPE:
         for (int i = 0; authmethods[i].name; i++) {
            if (bstrcasecmp(item->default_value, authmethods[i].name)) {
               *(item->ui32value) = authmethods[i].token;
            }
         }
         break;
      default:
         break;
      }
      break;
   default:
      break;
   }
}

/*
 * callback function for parse_config
 * See ../lib/parse_conf.c, function parse_config, for more generic handling.
 */
static void parse_config_cb(LEX *lc, RES_ITEM *item, int index, int pass)
{
   switch (item->type) {
   case CFG_TYPE_AUTOPASSWORD:
      store_autopassword(lc, item, index, pass);
      break;
   case CFG_TYPE_ACL:
      store_acl(lc, item, index, pass);
      break;
   case CFG_TYPE_AUDIT:
      store_audit(lc, item, index, pass);
      break;
   case CFG_TYPE_AUTHPROTOCOLTYPE:
      store_authprotocoltype(lc, item, index, pass);
      break;
   case CFG_TYPE_AUTHTYPE:
      store_authtype(lc, item, index, pass);
      break;
   case CFG_TYPE_DEVICE:
      store_device(lc, item, index, pass);
      break;
   case CFG_TYPE_JOBTYPE:
      store_jobtype(lc, item, index, pass);
      break;
   case CFG_TYPE_PROTOCOLTYPE:
      store_protocoltype(lc, item, index, pass);
      break;
   case CFG_TYPE_LEVEL:
      store_level(lc, item, index, pass);
      break;
   case CFG_TYPE_REPLACE:
      store_replace(lc, item, index, pass);
      break;
   case CFG_TYPE_SHRTRUNSCRIPT:
      store_short_runscript(lc, item, index, pass);
      break;
   case CFG_TYPE_RUNSCRIPT:
      store_runscript(lc, item, index, pass);
      break;
   case CFG_TYPE_MIGTYPE:
      store_migtype(lc, item, index, pass);
      break;
   case CFG_TYPE_INCEXC:
      store_inc(lc, item, index, pass);
      break;
   case CFG_TYPE_RUN:
      store_run(lc, item, index, pass);
      break;
   case CFG_TYPE_ACTIONONPURGE:
      store_actiononpurge(lc, item, index, pass);
      break;
   default:
      break;
   }
}

/*
 * callback function for print_config
 * See ../lib/res.c, function BRSRES::print_config, for more generic handling.
 */
static void print_config_cb(RES_ITEM *items, int i, POOL_MEM &cfg_str)
{
   POOL_MEM temp;

   switch (items[i].type) {
   case CFG_TYPE_DEVICE: {
      /*
       * Each member of the list is comma-separated
       */
      int cnt = 0;
      RES *res;
      alist* list;
      POOL_MEM res_names;

      list = *(items[i].alistvalue);
      if (list != NULL) {
         Mmsg(temp, "%s = ", items[i].name);
         indent_config_item(cfg_str, 1, temp.c_str());

         pm_strcpy(res_names, "");
         foreach_alist(res, list) {
            if (cnt) {
               Mmsg(temp, ",\"%s\"", res->name);
            } else {
               Mmsg(temp, "\"%s\"", res->name);
            }
            pm_strcat(res_names, temp.c_str());
            cnt++;
         }

         pm_strcat(cfg_str, res_names.c_str());
         pm_strcat(cfg_str, "\n");
      }
      break;
   }
   case CFG_TYPE_RUNSCRIPT:
      Dmsg0(200, "CFG_TYPE_RUNSCRIPT\n");
      print_config_runscript(&items[i], cfg_str);
      break;
   case CFG_TYPE_SHRTRUNSCRIPT:
      /*
       * We don't get here as this type is converted to a CFG_TYPE_RUNSCRIPT when parsed
       */
      break;
   case CFG_TYPE_ACL: {
      int cnt = 0;
      char *value;
      alist *list;
      POOL_MEM acl;

      list = items[i].alistvalue[items[i].code];
      if (list != NULL) {
         Mmsg(temp, "%s = ", items[i].name);
         indent_config_item(cfg_str, 1, temp.c_str());
         foreach_alist(value, list) {
            if (cnt) {
               Mmsg(temp, ",\"%s\"", value);
            } else {
               Mmsg(temp, "\"%s\"", value);
            }
            pm_strcat(acl, temp.c_str());
            cnt++;
         }

         pm_strcat(cfg_str, acl.c_str());
         pm_strcat(cfg_str, "\n");
      }
      break;
   }
   case CFG_TYPE_RUN:
      print_config_run(&items[i], cfg_str);
      break;
   case CFG_TYPE_JOBTYPE: {
      int32_t jobtype = *(items[i].ui32value);

      if (jobtype) {
         for (int j = 0; jobtypes[j].type_name; j++) {
            if (jobtypes[j].job_type == jobtype) {
               Mmsg(temp, "%s = %s\n", items[i].name, jobtypes[j].type_name);
               indent_config_item(cfg_str, 1, temp.c_str());
               break;
            }
         }
      }
      break;
   }
   case CFG_TYPE_PROTOCOLTYPE: {
      uint32_t protocol = *(items[i].ui32value);

      if (protocol) {
         for (int j = 0; backupprotocols[j].name; j++) {
            if (backupprotocols[j].token == protocol) {
               /*
                * Supress printing default value.
                */
               if (items[i].flags & CFG_ITEM_DEFAULT) {
                  if (bstrcasecmp(items[i].default_value, backupprotocols[j].name)) {
                     break;
                  }
               }

               Mmsg(temp, "%s = %s\n", items[i].name, backupprotocols[j].name);
               indent_config_item(cfg_str, 1, temp.c_str());
               break;
            }
         }
      }
      break;
   }
   case CFG_TYPE_MIGTYPE: {
      int32_t migtype = *(items[i].ui32value);

      if (migtype) {
         for (int j = 0; migtypes[j].type_name; j++) {
            if (migtypes[j].job_type == migtype) {
               Mmsg(temp, "%s = %s\n", items[i].name, migtypes[j].type_name);
               indent_config_item(cfg_str, 1, temp.c_str());
               break;
            }
         }
      }
      break;
   }
   case CFG_TYPE_REPLACE: {
      uint32_t replace = *(items[i].ui32value);

      if (replace) {
         for (int j = 0; ReplaceOptions[j].name; j++) {
            if (ReplaceOptions[j].token == replace) {
               /*
                * Supress printing default value.
                */
               if (items[i].flags & CFG_ITEM_DEFAULT) {
                  if (bstrcasecmp(items[i].default_value, ReplaceOptions[j].name)) {
                     break;
                  }
               }

               Mmsg(temp, "%s = %s\n", items[i].name, ReplaceOptions[j].name);
               indent_config_item(cfg_str, 1, temp.c_str());
               break;
            }
         }
      }
      break;
   }
   case CFG_TYPE_LEVEL: {
      uint32_t level = *(items[i].ui32value);

      if (level) {
         for (int j = 0; joblevels[j].level_name; j++) {
            if (joblevels[j].level == level) {
               Mmsg(temp, "%s = %s\n", items[i].name, joblevels[j].level_name);
               indent_config_item(cfg_str, 1, temp.c_str());
               break;
            }
         }
      }
      break;
   }
   case CFG_TYPE_ACTIONONPURGE: {
      uint32_t action = *(items[i].ui32value);

      if (action) {
         for (int j = 0; ActionOnPurgeOptions[j].name; j++) {
            if (ActionOnPurgeOptions[j].token == action) {
               Mmsg(temp, "%s = %s\n", items[i].name, ActionOnPurgeOptions[j].name);
               indent_config_item(cfg_str, 1, temp.c_str());
               break;
            }
         }
      }
      break;
   }
   case CFG_TYPE_AUTHPROTOCOLTYPE: {
      uint32_t authprotocol = *(items[i].ui32value);

      if (authprotocol) {
         for (int j = 0; authprotocols[j].name; j++) {
            if (authprotocols[j].token == authprotocol) {
               Mmsg(temp, "%s = %s\n", items[i].name, authprotocols[j].name);
               indent_config_item(cfg_str, 1, temp.c_str());
               break;
            }
         }
      }
      break;
   }
   case CFG_TYPE_AUTHTYPE: {
      uint32_t authtype = *(items[i].ui32value);

      if (authtype) {
         for (int j = 0; authmethods[j].name; j++) {
            if (authprotocols[j].token == authtype) {
               Mmsg(temp, "%s = %s\n", items[i].name, authmethods[j].name);
               indent_config_item(cfg_str, 1, temp.c_str());
               break;
            }
         }
      }
      break;
   }
   case CFG_TYPE_AUDIT: {
      /*
       * Each member of the list is comma-separated
       */
      int cnt = 0;
      char *audit_event;
      alist* list;
      POOL_MEM audit_events;

      list = *(items[i].alistvalue);
      if (list != NULL) {
         Mmsg(temp, "%s = ", items[i].name);
         indent_config_item(cfg_str, 1, temp.c_str());

         pm_strcpy(audit_events, "");
         foreach_alist(audit_event, list) {
            if (cnt) {
               Mmsg(temp, ",\"%s\"", audit_event);
            } else {
               Mmsg(temp, "\"%s\"", audit_event);
            }
            pm_strcat(audit_events, temp.c_str());
            cnt++;
         }

         pm_strcat(cfg_str, audit_events.c_str());
         pm_strcat(cfg_str, "\n");
      }
      break;
   }
   default:
      Dmsg2(200, "%s is UNSUPPORTED TYPE: %d\n", items[i].name, items[i].type);
      break;
   }
}

void init_dir_config(CONFIG *config, const char *configfile, int exit_code)
{
   config->init(configfile,
                NULL,
                NULL,
                init_resource_cb,
                parse_config_cb,
                print_config_cb,
                exit_code,
                (void *)&res_all,
                res_all_size,
                R_FIRST,
                R_LAST,
                resources,
                res_head);
}

bool parse_dir_config(CONFIG *config, const char *configfile, int exit_code)
{
   init_dir_config( config, configfile, exit_code );

   return config->parse_config();
}
