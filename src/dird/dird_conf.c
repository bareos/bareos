/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2016 Bareos GmbH & Co. KG

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

#define NEED_JANSSON_NAMESPACE 1
#include "bareos.h"
#include "dird.h"

/*
 * Used by print_config_schema_json
 */
extern struct s_kw RunFields[];

/*
 * Define the first and last resource ID record
 * types. Note, these should be unique for each
 * daemon though not a requirement.
 */
static RES *sres_head[R_LAST - R_FIRST + 1];
static RES **res_head = sres_head;
static POOL_MEM *configure_usage_string = NULL;

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
   { "Name", CFG_TYPE_NAME, ITEM(res_dir.hdr.name), 0, CFG_ITEM_REQUIRED, NULL, NULL,
     "The name of the resource." },
   { "Description", CFG_TYPE_STR, ITEM(res_dir.hdr.desc), 0, 0, NULL, NULL, NULL },
   { "Messages", CFG_TYPE_RES, ITEM(res_dir.messages), R_MSGS, 0, NULL, NULL, NULL },
   { "DirPort", CFG_TYPE_ADDRESSES_PORT, ITEM(res_dir.DIRaddrs), 0, CFG_ITEM_DEFAULT, DIR_DEFAULT_PORT, NULL, NULL },
   { "DirAddress", CFG_TYPE_ADDRESSES_ADDRESS, ITEM(res_dir.DIRaddrs), 0, CFG_ITEM_DEFAULT, DIR_DEFAULT_PORT, NULL, NULL },
   { "DirAddresses", CFG_TYPE_ADDRESSES, ITEM(res_dir.DIRaddrs), 0, CFG_ITEM_DEFAULT, DIR_DEFAULT_PORT, NULL, NULL },
   { "DirSourceAddress", CFG_TYPE_ADDRESSES_ADDRESS, ITEM(res_dir.DIRsrc_addr), 0, CFG_ITEM_DEFAULT, "0", NULL, NULL },
   { "QueryFile", CFG_TYPE_DIR, ITEM(res_dir.query_file), 0, CFG_ITEM_REQUIRED, NULL, NULL, NULL },
   { "WorkingDirectory", CFG_TYPE_DIR, ITEM(res_dir.working_directory), 0, CFG_ITEM_DEFAULT | CFG_ITEM_PLATFORM_SPECIFIC, _PATH_BAREOS_WORKINGDIR, NULL, NULL },
   { "PidDirectory", CFG_TYPE_DIR, ITEM(res_dir.pid_directory), 0, CFG_ITEM_DEFAULT | CFG_ITEM_PLATFORM_SPECIFIC, _PATH_BAREOS_PIDDIR, NULL, NULL },
   { "PluginDirectory", CFG_TYPE_DIR, ITEM(res_dir.plugin_directory), 0, 0, NULL,
     "14.2.0-", "Plugins are loaded from this directory. To load only specific plugins, use 'Plugin Names'." },
   { "PluginNames", CFG_TYPE_PLUGIN_NAMES, ITEM(res_dir.plugin_names), 0, 0, NULL,
      "14.2.0-", "List of plugins, that should get loaded from 'Plugin Directory' (only basenames, '-dir.so' is added automatically). If empty, all plugins will get loaded." },
   { "ScriptsDirectory", CFG_TYPE_DIR, ITEM(res_dir.scripts_directory), 0, 0, NULL, NULL, "This directive is currently unused." },
#if defined(HAVE_DYNAMIC_CATS_BACKENDS)
   { "BackendDirectory", CFG_TYPE_ALIST_DIR, ITEM(res_dir.backend_directories), 0, CFG_ITEM_DEFAULT | CFG_ITEM_PLATFORM_SPECIFIC, _PATH_BAREOS_BACKENDDIR, NULL, NULL },
#endif
   { "Subscriptions", CFG_TYPE_PINT32, ITEM(res_dir.subscriptions), 0, CFG_ITEM_DEFAULT, "0", "12.4.4-", NULL },
   { "SubSysDirectory", CFG_TYPE_DIR, ITEM(res_dir.subsys_directory), 0, CFG_ITEM_DEPRECATED, NULL, "-12.4.0", NULL },
   { "MaximumConcurrentJobs", CFG_TYPE_PINT32, ITEM(res_dir.MaxConcurrentJobs), 0, CFG_ITEM_DEFAULT, "1", NULL, NULL },
   { "MaximumConnections", CFG_TYPE_PINT32, ITEM(res_dir.MaxConnections), 0, CFG_ITEM_DEFAULT, "30", NULL, NULL },
   { "MaximumConsoleConnections", CFG_TYPE_PINT32, ITEM(res_dir.MaxConsoleConnections), 0, CFG_ITEM_DEFAULT, "20", NULL, NULL },
   { "Password", CFG_TYPE_AUTOPASSWORD, ITEM(res_dir.password), 0, CFG_ITEM_REQUIRED, NULL, NULL, NULL },
   { "FdConnectTimeout", CFG_TYPE_TIME, ITEM(res_dir.FDConnectTimeout), 0, CFG_ITEM_DEFAULT, "180" /* 3 minutes */, NULL, NULL },
   { "SdConnectTimeout", CFG_TYPE_TIME, ITEM(res_dir.SDConnectTimeout), 0, CFG_ITEM_DEFAULT, "1800" /* 30 minutes */, NULL, NULL },
   { "HeartbeatInterval", CFG_TYPE_TIME, ITEM(res_dir.heartbeat_interval), 0, CFG_ITEM_DEFAULT, "0", NULL, NULL },
   { "StatisticsRetention", CFG_TYPE_TIME, ITEM(res_dir.stats_retention), 0, CFG_ITEM_DEFAULT, "160704000" /* 5 years */, NULL, NULL },
   { "StatisticsCollectInterval", CFG_TYPE_PINT32, ITEM(res_dir.stats_collect_interval), 0, CFG_ITEM_DEFAULT, "150", "14.2.0-", NULL },
   { "VerId", CFG_TYPE_STR, ITEM(res_dir.verid), 0, 0, NULL, NULL, NULL },
   { "OptimizeForSize", CFG_TYPE_BOOL, ITEM(res_dir.optimize_for_size), 0, CFG_ITEM_DEFAULT, "false", NULL, NULL },
   { "OptimizeForSpeed", CFG_TYPE_BOOL, ITEM(res_dir.optimize_for_speed), 0, CFG_ITEM_DEFAULT, "false", NULL, NULL },
   { "OmitDefaults", CFG_TYPE_BOOL, ITEM(res_dir.omit_defaults), 0, CFG_ITEM_DEFAULT, "true", NULL, NULL },
   { "KeyEncryptionKey", CFG_TYPE_AUTOPASSWORD, ITEM(res_dir.keyencrkey), 1, 0, NULL, NULL, NULL },
   { "NdmpSnooping", CFG_TYPE_BOOL, ITEM(res_dir.ndmp_snooping), 0, 0, NULL, "13.2.0-", NULL },
   { "NdmpLogLevel", CFG_TYPE_PINT32, ITEM(res_dir.ndmp_loglevel), 0, CFG_ITEM_DEFAULT, "4", "13.2.0-", NULL },
   { "AbsoluteJobTimeout", CFG_TYPE_PINT32, ITEM(res_dir.jcr_watchdog_time), 0, 0, NULL, "14.2.0-", NULL },
   { "Auditing", CFG_TYPE_BOOL, ITEM(res_dir.auditing), 0, CFG_ITEM_DEFAULT, "false", "14.2.0-", NULL },
   { "AuditEvents", CFG_TYPE_AUDIT, ITEM(res_dir.audit_events), 0, 0, NULL, "14.2.0-", NULL },
   { "SecureEraseCommand", CFG_TYPE_STR, ITEM(res_dir.secure_erase_cmdline), 0, 0, NULL, "15.2.1-",
     "Specify command that will be called when bareos unlinks files." },
   { "LogTimestampFormat", CFG_TYPE_STR, ITEM(res_dir.log_timestamp_format), 0, 0, NULL, "15.2.3-", NULL },
   TLS_CONFIG(res_dir)
   { NULL, 0, { 0 }, 0, 0, NULL, NULL, NULL }
};

/*
 * Profile Resource
 *
 * name handler value code flags default_value
 */
static RES_ITEM profile_items[] = {
   { "Name", CFG_TYPE_NAME, ITEM(res_profile.hdr.name), 0, CFG_ITEM_REQUIRED, NULL, NULL,
     "The name of the resource." },
   { "Description", CFG_TYPE_STR, ITEM(res_profile.hdr.desc), 0, 0, NULL, NULL,
     "Additional information about the resource. Only used for UIs." },
   { "JobACL", CFG_TYPE_ACL, ITEM(res_profile.ACL_lists), Job_ACL, 0, NULL, NULL,
     "Lists the Job resources, this resource has access to. The special keyword *all* allows access to all Job resources." },
   { "ClientACL", CFG_TYPE_ACL, ITEM(res_profile.ACL_lists), Client_ACL, 0, NULL, NULL,
     "Lists the Client resources, this resource has access to. The special keyword *all* allows access to all Client resources." },
   { "StorageACL", CFG_TYPE_ACL, ITEM(res_profile.ACL_lists), Storage_ACL, 0, NULL, NULL,
     "Lists the Storage resources, this resource has access to. The special keyword *all* allows access to all Storage resources." },
   { "ScheduleACL", CFG_TYPE_ACL, ITEM(res_profile.ACL_lists), Schedule_ACL, 0, NULL, NULL,
     "Lists the Schedule resources, this resource has access to. The special keyword *all* allows access to all Schedule resources." },
   { "PoolACL", CFG_TYPE_ACL, ITEM(res_profile.ACL_lists), Pool_ACL, 0, NULL, NULL,
     "Lists the Pool resources, this resource has access to. The special keyword *all* allows access to all Pool resources." },
   { "CommandACL", CFG_TYPE_ACL, ITEM(res_profile.ACL_lists), Command_ACL, 0, NULL, NULL,
     "Lists the commands, this resource has access to. The special keyword *all* allows using commands." },
   { "FileSetACL", CFG_TYPE_ACL, ITEM(res_profile.ACL_lists), FileSet_ACL, 0, NULL, NULL,
     "Lists the File Set resources, this resource has access to. The special keyword *all* allows access to all File Set resources." },
   { "CatalogACL", CFG_TYPE_ACL, ITEM(res_profile.ACL_lists), Catalog_ACL, 0, NULL, NULL,
     "Lists the Catalog resources, this resource has access to. The special keyword *all* allows access to all Catalog resources." },
   { "WhereACL", CFG_TYPE_ACL, ITEM(res_profile.ACL_lists), Where_ACL, 0, NULL, NULL,
     "Specifies the base directories, where files could be restored. An empty string allows restores to all directories." },
   { "PluginOptionsACL", CFG_TYPE_ACL, ITEM(res_profile.ACL_lists), PluginOptions_ACL, 0, NULL, NULL,
     "Specifies the allowed plugin options. An empty strings allows all Plugin Options." },
   { NULL, 0, { 0 }, 0, 0, NULL, NULL, NULL }
};

/*
 * Console Resource
 *
 * name handler value code flags default_value
 */
static RES_ITEM con_items[] = {
   { "Name", CFG_TYPE_NAME, ITEM(res_con.hdr.name), 0, CFG_ITEM_REQUIRED, NULL, NULL, NULL },
   { "Description", CFG_TYPE_STR, ITEM(res_con.hdr.desc), 0, 0, NULL, NULL, NULL },
   { "Password", CFG_TYPE_AUTOPASSWORD, ITEM(res_con.password), 0, CFG_ITEM_REQUIRED, NULL, NULL, NULL },
   { "JobACL", CFG_TYPE_ACL, ITEM(res_con.ACL_lists), Job_ACL, 0, NULL, NULL, NULL },
   { "ClientACL", CFG_TYPE_ACL, ITEM(res_con.ACL_lists), Client_ACL, 0, NULL, NULL, NULL },
   { "StorageACL", CFG_TYPE_ACL, ITEM(res_con.ACL_lists), Storage_ACL, 0, NULL, NULL, NULL },
   { "ScheduleACL", CFG_TYPE_ACL, ITEM(res_con.ACL_lists), Schedule_ACL, 0, NULL, NULL, NULL },
   { "RunACL", CFG_TYPE_ACL, ITEM(res_con.ACL_lists), Run_ACL, 0, NULL, NULL, NULL },
   { "PoolACL", CFG_TYPE_ACL, ITEM(res_con.ACL_lists), Pool_ACL, 0, NULL, NULL, NULL },
   { "CommandACL", CFG_TYPE_ACL, ITEM(res_con.ACL_lists), Command_ACL, 0, NULL, NULL, NULL },
   { "FileSetACL", CFG_TYPE_ACL, ITEM(res_con.ACL_lists), FileSet_ACL, 0, NULL, NULL, NULL },
   { "CatalogACL", CFG_TYPE_ACL, ITEM(res_con.ACL_lists), Catalog_ACL, 0, NULL, NULL, NULL },
   { "WhereACL", CFG_TYPE_ACL, ITEM(res_con.ACL_lists), Where_ACL, 0, NULL, NULL, NULL },
   { "PluginOptionsACL", CFG_TYPE_ACL, ITEM(res_con.ACL_lists), PluginOptions_ACL, 0, NULL, NULL, NULL },
   { "Profile", CFG_TYPE_ALIST_RES, ITEM(res_con.profiles), R_PROFILE, 0, NULL, "14.2.3-",
     "Profiles can be assigned to a Console. ACL are checked until either a deny ACL is found or an allow ACL. "
     "First the console ACL is checked then any profile the console is linked to." },
   TLS_CONFIG(res_con)
   { NULL, 0, { 0 }, 0, 0, NULL, NULL, NULL }
};

/*
 * Client or File daemon resource
 *
 * name handler value code flags default_value
 */
static RES_ITEM cli_items[] = {
   { "Name", CFG_TYPE_NAME, ITEM(res_client.hdr.name), 0, CFG_ITEM_REQUIRED, NULL, NULL,
     "The name of the resource." },
   { "Description", CFG_TYPE_STR, ITEM(res_client.hdr.desc), 0, 0, NULL, NULL, NULL },
   { "Protocol", CFG_TYPE_AUTHPROTOCOLTYPE, ITEM(res_client.Protocol), 0, CFG_ITEM_DEFAULT, "Native", "13.2.0-", NULL },
   { "AuthType", CFG_TYPE_AUTHTYPE, ITEM(res_client.AuthType), 0, CFG_ITEM_DEFAULT, "None", NULL, NULL },
   { "Address", CFG_TYPE_STR, ITEM(res_client.address), 0, CFG_ITEM_REQUIRED, NULL, NULL, NULL },
   { "FdAddress", CFG_TYPE_STR, ITEM(res_client.address), 0, CFG_ITEM_ALIAS, NULL, NULL, "Alias for Address." },
   { "Port", CFG_TYPE_PINT32, ITEM(res_client.FDport), 0, CFG_ITEM_DEFAULT, FD_DEFAULT_PORT, NULL, NULL },
   { "FdPort", CFG_TYPE_PINT32, ITEM(res_client.FDport), 0, CFG_ITEM_DEFAULT | CFG_ITEM_ALIAS, FD_DEFAULT_PORT, NULL, NULL },
   { "Username", CFG_TYPE_STR, ITEM(res_client.username), 0, 0, NULL, NULL, NULL },
   { "Password", CFG_TYPE_AUTOPASSWORD, ITEM(res_client.password), 0, CFG_ITEM_REQUIRED, NULL, NULL, NULL },
   { "FdPassword", CFG_TYPE_AUTOPASSWORD, ITEM(res_client.password), 0, CFG_ITEM_ALIAS, NULL, NULL, NULL },
   { "Catalog", CFG_TYPE_RES, ITEM(res_client.catalog), R_CATALOG, 0, NULL, NULL, NULL },
   { "Passive", CFG_TYPE_BOOL, ITEM(res_client.passive), 0, CFG_ITEM_DEFAULT, "false", "13.2.0-",
     "If enabled, the Storage Daemon will initiate the network connection to the Client. If disabled, the Client will initiate the netowrk connection to the Storage Daemon." },
   { "ConnectionFromDirectorToClient", CFG_TYPE_BOOL, ITEM(res_client.conn_from_dir_to_fd), 0, CFG_ITEM_DEFAULT, "true", "16.2.2",
     "Let the Director initiate the network connection to the Client." },
   { "AllowClientConnect", CFG_TYPE_BOOL, ITEM(res_client.conn_from_fd_to_dir), 0, CFG_ITEM_DEPRECATED | CFG_ITEM_ALIAS, NULL, NULL,
     "Alias of \"Connection From Client To Director\"." },
   { "ConnectionFromClientToDirector", CFG_TYPE_BOOL, ITEM(res_client.conn_from_fd_to_dir), 0, CFG_ITEM_DEFAULT, "false", "16.2.2",
     "The Director will accept incoming network connection from this Client." },
   { "Enabled", CFG_TYPE_BOOL, ITEM(res_client.enabled), 0, CFG_ITEM_DEFAULT, "true", NULL,
     "En- or disable this resource." },
   { "HardQuota", CFG_TYPE_SIZE64, ITEM(res_client.HardQuota), 0, CFG_ITEM_DEFAULT, "0", NULL, NULL },
   { "SoftQuota", CFG_TYPE_SIZE64, ITEM(res_client.SoftQuota), 0, CFG_ITEM_DEFAULT, "0", NULL, NULL },
   { "SoftQuotaGracePeriod", CFG_TYPE_TIME, ITEM(res_client.SoftQuotaGracePeriod), 0, CFG_ITEM_DEFAULT, "0", NULL, NULL },
   { "StrictQuotas", CFG_TYPE_BOOL, ITEM(res_client.StrictQuotas), 0, CFG_ITEM_DEFAULT, "false", NULL, NULL },
   { "QuotaIncludeFailedJobs", CFG_TYPE_BOOL, ITEM(res_client.QuotaIncludeFailedJobs), 0, CFG_ITEM_DEFAULT, "true", NULL, NULL },
   { "FileRetention", CFG_TYPE_TIME, ITEM(res_client.FileRetention), 0, CFG_ITEM_DEFAULT, "5184000" /* 60 days */, NULL, NULL },
   { "JobRetention", CFG_TYPE_TIME, ITEM(res_client.JobRetention), 0, CFG_ITEM_DEFAULT, "15552000" /* 180 days */, NULL, NULL },
   { "HeartbeatInterval", CFG_TYPE_TIME, ITEM(res_client.heartbeat_interval), 0, CFG_ITEM_DEFAULT, "0", NULL, NULL },
   { "AutoPrune", CFG_TYPE_BOOL, ITEM(res_client.AutoPrune), 0, CFG_ITEM_DEFAULT, "false", NULL, NULL },
   { "MaximumConcurrentJobs", CFG_TYPE_PINT32, ITEM(res_client.MaxConcurrentJobs), 0, CFG_ITEM_DEFAULT, "1", NULL, NULL },
   { "MaximumBandwidthPerJob", CFG_TYPE_SPEED, ITEM(res_client.max_bandwidth), 0, 0, NULL, NULL, NULL },
   { "NdmpLogLevel", CFG_TYPE_PINT32, ITEM(res_client.ndmp_loglevel), 0, CFG_ITEM_DEFAULT, "4", NULL, NULL },
   { "NdmpBlockSize", CFG_TYPE_PINT32, ITEM(res_client.ndmp_blocksize), 0, CFG_ITEM_DEFAULT, "64512", NULL, NULL },
   { "NdmpUseLmdb", CFG_TYPE_BOOL, ITEM(res_client.ndmp_use_lmdb), 0, CFG_ITEM_DEFAULT, "true", NULL, NULL },
   TLS_CONFIG(res_client)
   { NULL, 0, { 0 }, 0, 0, NULL, NULL, NULL }
};

/* Storage daemon resource
 *
 * name handler value code flags default_value
 */
static RES_ITEM store_items[] = {
   { "Name", CFG_TYPE_NAME, ITEM(res_store.hdr.name), 0, CFG_ITEM_REQUIRED, NULL, NULL,
      "The name of the resource." },
   { "Description", CFG_TYPE_STR, ITEM(res_store.hdr.desc), 0, 0, NULL, NULL, NULL },
   { "Protocol", CFG_TYPE_AUTHPROTOCOLTYPE, ITEM(res_store.Protocol), 0, CFG_ITEM_DEFAULT, "Native", NULL, NULL },
   { "AuthType", CFG_TYPE_AUTHTYPE, ITEM(res_store.AuthType), 0, CFG_ITEM_DEFAULT, "None", NULL, NULL },
   { "Address", CFG_TYPE_STR, ITEM(res_store.address), 0, CFG_ITEM_REQUIRED, NULL, NULL, NULL },
   { "SdAddress", CFG_TYPE_STR, ITEM(res_store.address), 0, CFG_ITEM_ALIAS, NULL, NULL, "Alias for Address." },
   { "Port", CFG_TYPE_PINT32, ITEM(res_store.SDport), 0, CFG_ITEM_DEFAULT, SD_DEFAULT_PORT, NULL, NULL },
   { "SdPort", CFG_TYPE_PINT32, ITEM(res_store.SDport), 0, CFG_ITEM_DEFAULT | CFG_ITEM_ALIAS, SD_DEFAULT_PORT, NULL, "Alias for Port." },
   { "Username", CFG_TYPE_STR, ITEM(res_store.username), 0, 0, NULL, NULL, NULL },
   { "Password", CFG_TYPE_AUTOPASSWORD, ITEM(res_store.password), 0, CFG_ITEM_REQUIRED, NULL, NULL, NULL },
   { "SdPassword", CFG_TYPE_AUTOPASSWORD, ITEM(res_store.password), 0, CFG_ITEM_ALIAS, NULL, NULL, "Alias for Password." },
   { "Device", CFG_TYPE_DEVICE, ITEM(res_store.device), R_DEVICE, CFG_ITEM_REQUIRED, NULL, NULL, NULL },
   { "MediaType", CFG_TYPE_STRNAME, ITEM(res_store.media_type), 0, CFG_ITEM_REQUIRED, NULL, NULL, NULL },
   { "AutoChanger", CFG_TYPE_BOOL, ITEM(res_store.autochanger), 0, CFG_ITEM_DEFAULT, "false", NULL, NULL },
   { "Enabled", CFG_TYPE_BOOL, ITEM(res_store.enabled), 0, CFG_ITEM_DEFAULT, "true", NULL,
     "En- or disable this resource." },
   { "AllowCompression", CFG_TYPE_BOOL, ITEM(res_store.AllowCompress), 0, CFG_ITEM_DEFAULT, "true", NULL, NULL },
   { "HeartbeatInterval", CFG_TYPE_TIME, ITEM(res_store.heartbeat_interval), 0, CFG_ITEM_DEFAULT, "0", NULL, NULL },
   { "CacheStatusInterval", CFG_TYPE_TIME, ITEM(res_store.cache_status_interval), 0, CFG_ITEM_DEFAULT, "30", NULL, NULL },
   { "MaximumConcurrentJobs", CFG_TYPE_PINT32, ITEM(res_store.MaxConcurrentJobs), 0, CFG_ITEM_DEFAULT, "1", NULL, NULL },
   { "MaximumConcurrentReadJobs", CFG_TYPE_PINT32, ITEM(res_store.MaxConcurrentReadJobs), 0, CFG_ITEM_DEFAULT, "0", NULL, NULL },
   { "SddPort", CFG_TYPE_PINT32, ITEM(res_store.SDDport), 0, CFG_ITEM_DEPRECATED, NULL, "-12.4.0", NULL },
   { "PairedStorage", CFG_TYPE_RES, ITEM(res_store.paired_storage), R_STORAGE, 0, NULL, NULL, NULL },
   { "MaximumBandwidthPerJob", CFG_TYPE_SPEED, ITEM(res_store.max_bandwidth), 0, 0, NULL, NULL, NULL },
   { "CollectStatistics", CFG_TYPE_BOOL, ITEM(res_store.collectstats), 0, CFG_ITEM_DEFAULT, "false", NULL, NULL },
   { "ChangerDevice", CFG_TYPE_STRNAME, ITEM(res_store.changer_device), 0, 0, NULL, NULL, NULL },
   { "TapeDevice", CFG_TYPE_ALIST_STR, ITEM(res_store.tape_devices), 0, 0, NULL, NULL, NULL },
   TLS_CONFIG(res_store)
   { NULL, 0, { 0 }, 0, 0, NULL, NULL, NULL }
};

/*
 * Catalog Resource Directives
 *
 * name handler value code flags default_value
 */
static RES_ITEM cat_items[] = {
   { "Name", CFG_TYPE_NAME, ITEM(res_cat.hdr.name), 0, CFG_ITEM_REQUIRED, NULL, NULL,
     "The name of the resource." },
   { "Description", CFG_TYPE_STR, ITEM(res_cat.hdr.desc), 0, 0, NULL, NULL, NULL },
   { "Address", CFG_TYPE_STR, ITEM(res_cat.db_address), 0, CFG_ITEM_ALIAS, NULL, NULL, NULL },
   { "DbAddress", CFG_TYPE_STR, ITEM(res_cat.db_address), 0, 0, NULL, NULL, NULL },
   { "DbPort", CFG_TYPE_PINT32, ITEM(res_cat.db_port), 0, 0, NULL, NULL, NULL },
   { "Password", CFG_TYPE_AUTOPASSWORD, ITEM(res_cat.db_password), 0, CFG_ITEM_ALIAS, NULL, NULL, NULL },
   { "DbPassword", CFG_TYPE_AUTOPASSWORD, ITEM(res_cat.db_password), 0, 0, NULL, NULL, NULL },
   { "DbUser", CFG_TYPE_STR, ITEM(res_cat.db_user), 0, 0, NULL, NULL, NULL },
   { "User", CFG_TYPE_STR, ITEM(res_cat.db_user), 0, CFG_ITEM_ALIAS, NULL, NULL, NULL },
   { "DbName", CFG_TYPE_STR, ITEM(res_cat.db_name), 0, CFG_ITEM_REQUIRED, NULL, NULL, NULL },
#ifdef HAVE_DYNAMIC_CATS_BACKENDS
   { "DbDriver", CFG_TYPE_STR, ITEM(res_cat.db_driver), 0, CFG_ITEM_REQUIRED, NULL, NULL, NULL },
#else
   { "DbDriver", CFG_TYPE_STR, ITEM(res_cat.db_driver), 0, 0, NULL, NULL, NULL },
#endif
   { "DbSocket", CFG_TYPE_STR, ITEM(res_cat.db_socket), 0, 0, NULL, NULL, NULL },
   /* Turned off for the moment */
   { "MultipleConnections", CFG_TYPE_BIT, ITEM(res_cat.mult_db_connections), 0, 0, NULL, NULL, NULL },
   { "DisableBatchInsert", CFG_TYPE_BOOL, ITEM(res_cat.disable_batch_insert), 0, CFG_ITEM_DEFAULT, "false", NULL, NULL },
   { "Reconnect", CFG_TYPE_BOOL, ITEM(res_cat.try_reconnect), 0, CFG_ITEM_DEFAULT, "false",
     "15.1.0-", "Try to reconnect a database connection when its dropped" },
   { "ExitOnFatal", CFG_TYPE_BOOL, ITEM(res_cat.exit_on_fatal), 0, CFG_ITEM_DEFAULT, "false",
     "15.1.0-", "Make any fatal error in the connection to the database exit the program" },
   { "MinConnections", CFG_TYPE_PINT32, ITEM(res_cat.pooling_min_connections), 0, CFG_ITEM_DEFAULT, "1", NULL, NULL },
   { "MaxConnections", CFG_TYPE_PINT32, ITEM(res_cat.pooling_max_connections), 0, CFG_ITEM_DEFAULT, "5", NULL, NULL },
   { "IncConnections", CFG_TYPE_PINT32, ITEM(res_cat.pooling_increment_connections), 0, CFG_ITEM_DEFAULT, "1", NULL, NULL },
   { "IdleTimeout", CFG_TYPE_PINT32, ITEM(res_cat.pooling_idle_timeout), 0, CFG_ITEM_DEFAULT, "30", NULL, NULL },
   { "ValidateTimeout", CFG_TYPE_PINT32, ITEM(res_cat.pooling_validate_timeout), 0, CFG_ITEM_DEFAULT, "120", NULL, NULL },
   { NULL, 0, { 0 }, 0, 0, NULL, NULL, NULL }
};

/*
 * Job Resource Directives
 *
 * name handler value code flags default_value
 */
RES_ITEM job_items[] = {
   { "Name", CFG_TYPE_NAME, ITEM(res_job.hdr.name), 0, CFG_ITEM_REQUIRED, NULL, NULL,
     "The name of the resource." },
   { "Description", CFG_TYPE_STR, ITEM(res_job.hdr.desc), 0, 0, NULL, NULL, NULL },
   { "Type", CFG_TYPE_JOBTYPE, ITEM(res_job.JobType), 0, CFG_ITEM_REQUIRED, NULL, NULL, NULL },
   { "Protocol", CFG_TYPE_PROTOCOLTYPE, ITEM(res_job.Protocol), 0, CFG_ITEM_DEFAULT, "Native", NULL, NULL },
   { "BackupFormat", CFG_TYPE_STR, ITEM(res_job.backup_format), 0, CFG_ITEM_DEFAULT, "Native", NULL, NULL },
   { "Level", CFG_TYPE_LEVEL, ITEM(res_job.JobLevel), 0, 0, NULL, NULL, NULL },
   { "Messages", CFG_TYPE_RES, ITEM(res_job.messages), R_MSGS, CFG_ITEM_REQUIRED, NULL, NULL, NULL },
   { "Storage", CFG_TYPE_ALIST_RES, ITEM(res_job.storage), R_STORAGE, 0, NULL, NULL, NULL },
   { "Pool", CFG_TYPE_RES, ITEM(res_job.pool), R_POOL, CFG_ITEM_REQUIRED, NULL, NULL, NULL },
   { "FullBackupPool", CFG_TYPE_RES, ITEM(res_job.full_pool), R_POOL, 0, NULL, NULL, NULL },
   { "VirtualFullBackupPool", CFG_TYPE_RES, ITEM(res_job.vfull_pool), R_POOL, 0, NULL, NULL, NULL },
   { "IncrementalBackupPool", CFG_TYPE_RES, ITEM(res_job.inc_pool), R_POOL, 0, NULL, NULL, NULL },
   { "DifferentialBackupPool", CFG_TYPE_RES, ITEM(res_job.diff_pool), R_POOL, 0, NULL, NULL, NULL },
   { "NextPool", CFG_TYPE_RES, ITEM(res_job.next_pool), R_POOL, 0, NULL, NULL, NULL },
   { "Client", CFG_TYPE_RES, ITEM(res_job.client), R_CLIENT, 0, NULL, NULL, NULL },
   { "FileSet", CFG_TYPE_RES, ITEM(res_job.fileset), R_FILESET, 0, NULL, NULL, NULL },
   { "Schedule", CFG_TYPE_RES, ITEM(res_job.schedule), R_SCHEDULE, 0, NULL, NULL, NULL },
   { "VerifyJob", CFG_TYPE_RES, ITEM(res_job.verify_job), R_JOB, CFG_ITEM_ALIAS, NULL, NULL, NULL },
   { "JobToVerify", CFG_TYPE_RES, ITEM(res_job.verify_job), R_JOB, 0, NULL, NULL, NULL },
   { "Catalog", CFG_TYPE_RES, ITEM(res_job.catalog), R_CATALOG, 0, NULL, "13.4.0-", NULL },
   { "JobDefs", CFG_TYPE_RES, ITEM(res_job.jobdefs), R_JOBDEFS, 0, NULL, NULL, NULL },
   { "Run", CFG_TYPE_ALIST_STR, ITEM(res_job.run_cmds), 0, 0, NULL, NULL, NULL },
   /* Root of where to restore files */
   { "Where", CFG_TYPE_DIR, ITEM(res_job.RestoreWhere), 0, 0, NULL, NULL, NULL },
   { "RegexWhere", CFG_TYPE_STR, ITEM(res_job.RegexWhere), 0, 0, NULL, NULL, NULL },
   { "StripPrefix", CFG_TYPE_STR, ITEM(res_job.strip_prefix), 0, 0, NULL, NULL, NULL },
   { "AddPrefix", CFG_TYPE_STR, ITEM(res_job.add_prefix), 0, 0, NULL, NULL, NULL },
   { "AddSuffix", CFG_TYPE_STR, ITEM(res_job.add_suffix), 0, 0, NULL, NULL, NULL },
   /* Where to find bootstrap during restore */
   { "Bootstrap", CFG_TYPE_DIR, ITEM(res_job.RestoreBootstrap), 0, 0, NULL, NULL, NULL },
   /* Where to write bootstrap file during backup */
   { "WriteBootstrap", CFG_TYPE_DIR, ITEM(res_job.WriteBootstrap), 0, 0, NULL, NULL, NULL },
   { "WriteVerifyList", CFG_TYPE_DIR, ITEM(res_job.WriteVerifyList), 0, 0, NULL, NULL, NULL },
   { "Replace", CFG_TYPE_REPLACE, ITEM(res_job.replace), 0, CFG_ITEM_DEFAULT, "Always", NULL, NULL },
   { "MaximumBandwidth", CFG_TYPE_SPEED, ITEM(res_job.max_bandwidth), 0, 0, NULL, NULL, NULL },
   { "MaxrunSchedTime", CFG_TYPE_TIME, ITEM(res_job.MaxRunSchedTime), 0, 0, NULL, NULL, NULL },
   { "MaxRunTime", CFG_TYPE_TIME, ITEM(res_job.MaxRunTime), 0, 0, NULL, NULL, NULL },
   { "FullMaxWaitTime", CFG_TYPE_TIME, ITEM(res_job.FullMaxRunTime), 0, CFG_ITEM_DEPRECATED, NULL, "-12.4.0", NULL },
   { "IncrementalMaxWaitTime", CFG_TYPE_TIME, ITEM(res_job.IncMaxRunTime), 0, CFG_ITEM_DEPRECATED, NULL, "-12.4.0", NULL },
   { "DifferentialMaxWaitTime", CFG_TYPE_TIME, ITEM(res_job.DiffMaxRunTime), 0, CFG_ITEM_DEPRECATED, NULL, "-12.4.0", NULL },
   { "FullMaxRuntime", CFG_TYPE_TIME, ITEM(res_job.FullMaxRunTime), 0, 0, NULL, NULL, NULL },
   { "IncrementalMaxRuntime", CFG_TYPE_TIME, ITEM(res_job.IncMaxRunTime), 0, 0, NULL, NULL, NULL },
   { "DifferentialMaxRuntime", CFG_TYPE_TIME, ITEM(res_job.DiffMaxRunTime), 0, 0, NULL, NULL, NULL },
   { "MaxWaitTime", CFG_TYPE_TIME, ITEM(res_job.MaxWaitTime), 0, 0, NULL, NULL, NULL },
   { "MaxStartDelay", CFG_TYPE_TIME, ITEM(res_job.MaxStartDelay), 0, 0, NULL, NULL, NULL },
   { "MaxFullInterval", CFG_TYPE_TIME, ITEM(res_job.MaxFullInterval), 0, 0, NULL, NULL, NULL },
   { "MaxVirtualFullInterval", CFG_TYPE_TIME, ITEM(res_job.MaxVFullInterval), 0, 0, NULL, "14.4.0-", NULL },
   { "MaxDiffInterval", CFG_TYPE_TIME, ITEM(res_job.MaxDiffInterval), 0, 0, NULL, NULL, NULL },
   { "PrefixLinks", CFG_TYPE_BOOL, ITEM(res_job.PrefixLinks), 0, CFG_ITEM_DEFAULT, "false", NULL, NULL },
   { "PruneJobs", CFG_TYPE_BOOL, ITEM(res_job.PruneJobs), 0, CFG_ITEM_DEFAULT, "false", NULL, NULL },
   { "PruneFiles", CFG_TYPE_BOOL, ITEM(res_job.PruneFiles), 0, CFG_ITEM_DEFAULT, "false", NULL, NULL },
   { "PruneVolumes", CFG_TYPE_BOOL, ITEM(res_job.PruneVolumes), 0, CFG_ITEM_DEFAULT, "false", NULL, NULL },
   { "PurgeMigrationJob", CFG_TYPE_BOOL, ITEM(res_job.PurgeMigrateJob), 0, CFG_ITEM_DEFAULT, "false", NULL, NULL },
   { "Enabled", CFG_TYPE_BOOL, ITEM(res_job.enabled), 0, CFG_ITEM_DEFAULT, "true", NULL,
     "En- or disable this resource." },
   { "SpoolAttributes", CFG_TYPE_BOOL, ITEM(res_job.SpoolAttributes), 0, CFG_ITEM_DEFAULT, "false", NULL, NULL },
   { "SpoolData", CFG_TYPE_BOOL, ITEM(res_job.spool_data), 0, CFG_ITEM_DEFAULT, "false", NULL, NULL },
   { "SpoolSize", CFG_TYPE_SIZE64, ITEM(res_job.spool_size), 0, 0, NULL, NULL, NULL },
   { "RerunFailedLevels", CFG_TYPE_BOOL, ITEM(res_job.rerun_failed_levels), 0, CFG_ITEM_DEFAULT, "false", NULL, NULL },
   { "PreferMountedVolumes", CFG_TYPE_BOOL, ITEM(res_job.PreferMountedVolumes), 0, CFG_ITEM_DEFAULT, "true", NULL, NULL },
   { "RunBeforeJob", CFG_TYPE_SHRTRUNSCRIPT, ITEM(res_job.RunScripts), 0, 0, NULL, NULL, NULL },
   { "RunAfterJob", CFG_TYPE_SHRTRUNSCRIPT, ITEM(res_job.RunScripts), 0, 0, NULL, NULL, NULL },
   { "RunAfterFailedJob", CFG_TYPE_SHRTRUNSCRIPT, ITEM(res_job.RunScripts), 0, 0, NULL, NULL, NULL },
   { "ClientRunBeforeJob", CFG_TYPE_SHRTRUNSCRIPT, ITEM(res_job.RunScripts), 0, 0, NULL, NULL, NULL },
   { "ClientRunAfterJob", CFG_TYPE_SHRTRUNSCRIPT, ITEM(res_job.RunScripts), 0, 0, NULL, NULL, NULL },
   { "MaximumConcurrentJobs", CFG_TYPE_PINT32, ITEM(res_job.MaxConcurrentJobs), 0, CFG_ITEM_DEFAULT, "1", NULL, NULL },
   { "RescheduleOnError", CFG_TYPE_BOOL, ITEM(res_job.RescheduleOnError), 0, CFG_ITEM_DEFAULT, "false", NULL, NULL },
   { "RescheduleInterval", CFG_TYPE_TIME, ITEM(res_job.RescheduleInterval), 0, CFG_ITEM_DEFAULT, "1800" /* 30 minutes */, NULL, NULL },
   { "RescheduleTimes", CFG_TYPE_PINT32, ITEM(res_job.RescheduleTimes), 0, CFG_ITEM_DEFAULT, "5", NULL, NULL },
   { "Priority", CFG_TYPE_PINT32, ITEM(res_job.Priority), 0, CFG_ITEM_DEFAULT, "10", NULL, NULL },
   { "AllowMixedPriority", CFG_TYPE_BOOL, ITEM(res_job.allow_mixed_priority), 0, CFG_ITEM_DEFAULT, "false", NULL, NULL },
   { "WritePartAfterJob", CFG_TYPE_BOOL, ITEM(res_job.write_part_after_job), 0, CFG_ITEM_DEPRECATED, NULL, "-12.4.0", NULL },
   { "SelectionPattern", CFG_TYPE_STR, ITEM(res_job.selection_pattern), 0, 0, NULL, NULL, NULL },
   { "RunScript", CFG_TYPE_RUNSCRIPT, ITEM(res_job.RunScripts), 0, CFG_ITEM_NO_EQUALS, NULL, NULL, NULL },
   { "SelectionType", CFG_TYPE_MIGTYPE, ITEM(res_job.selection_type), 0, 0, NULL, NULL, NULL },
   { "Accurate", CFG_TYPE_BOOL, ITEM(res_job.accurate), 0, CFG_ITEM_DEFAULT, "false", NULL, NULL },
   { "AllowDuplicateJobs", CFG_TYPE_BOOL, ITEM(res_job.AllowDuplicateJobs), 0, CFG_ITEM_DEFAULT, "true", NULL, NULL },
   { "AllowHigherDuplicates", CFG_TYPE_BOOL, ITEM(res_job.AllowHigherDuplicates), 0, CFG_ITEM_DEFAULT, "true", NULL, NULL },
   { "CancelLowerLevelDuplicates", CFG_TYPE_BOOL, ITEM(res_job.CancelLowerLevelDuplicates), 0, CFG_ITEM_DEFAULT, "false", NULL, NULL },
   { "CancelQueuedDuplicates", CFG_TYPE_BOOL, ITEM(res_job.CancelQueuedDuplicates), 0, CFG_ITEM_DEFAULT, "false", NULL, NULL },
   { "CancelRunningDuplicates", CFG_TYPE_BOOL, ITEM(res_job.CancelRunningDuplicates), 0, CFG_ITEM_DEFAULT, "false", NULL, NULL },
   { "SaveFileHistory", CFG_TYPE_BOOL, ITEM(res_job.SaveFileHist), 0, CFG_ITEM_DEFAULT, "true", "14.2.0-", NULL },
   { "FileHistorySize", CFG_TYPE_SIZE64, ITEM(res_job.FileHistSize), 0, CFG_ITEM_DEFAULT, "10000000", "15.2.4-", NULL },
   { "PluginOptions", CFG_TYPE_ALIST_STR, ITEM(res_job.FdPluginOptions), 0, CFG_ITEM_DEPRECATED | CFG_ITEM_ALIAS, NULL, "-12.4.0", NULL },
   { "FdPluginOptions", CFG_TYPE_ALIST_STR, ITEM(res_job.FdPluginOptions), 0, 0, NULL, NULL, NULL },
   { "SdPluginOptions", CFG_TYPE_ALIST_STR, ITEM(res_job.SdPluginOptions), 0, 0, NULL, NULL, NULL },
   { "DirPluginOptions", CFG_TYPE_ALIST_STR, ITEM(res_job.DirPluginOptions), 0, 0, NULL, NULL, NULL },
   { "Base", CFG_TYPE_ALIST_RES, ITEM(res_job.base), R_JOB, 0, NULL, NULL, NULL },
   { "MaxConcurrentCopies", CFG_TYPE_PINT32, ITEM(res_job.MaxConcurrentCopies), 0, CFG_ITEM_DEFAULT, "100", NULL, NULL },
   /* Settings for always incremental */
   { "AlwaysIncremental", CFG_TYPE_BOOL, ITEM(res_job.AlwaysIncremental), 0, CFG_ITEM_DEFAULT, "false", NULL, NULL },
   { "AlwaysIncrementalInterval", CFG_TYPE_TIME, ITEM(res_job.AlwaysIncrementalInterval), 0, CFG_ITEM_DEFAULT, "0", NULL, NULL },
   { "AlwaysIncrementalNumber", CFG_TYPE_PINT32, ITEM(res_job.AlwaysIncrementalNumber), 0, CFG_ITEM_DEFAULT, "0", NULL, NULL },
   { NULL, 0, { 0 }, 0, 0, NULL, NULL, NULL }
};

/*
 * FileSet resource
 *
 * name handler value code flags default_value
 */
static RES_ITEM fs_items[] = {
   { "Name", CFG_TYPE_NAME, ITEM(res_fs.hdr.name), 0, CFG_ITEM_REQUIRED, NULL, NULL,
     "The name of the resource." },
   { "Description", CFG_TYPE_STR, ITEM(res_fs.hdr.desc), 0, 0, NULL, NULL, NULL },
   { "Include", CFG_TYPE_INCEXC, { 0 }, 0, CFG_ITEM_NO_EQUALS, NULL, NULL, NULL },
   { "Exclude", CFG_TYPE_INCEXC, { 0 }, 1, CFG_ITEM_NO_EQUALS, NULL, NULL, NULL },
   { "IgnoreFileSetChanges", CFG_TYPE_BOOL, ITEM(res_fs.ignore_fs_changes), 0, CFG_ITEM_DEFAULT, "false", NULL, NULL },
   { "EnableVSS", CFG_TYPE_BOOL, ITEM(res_fs.enable_vss), 0, CFG_ITEM_DEFAULT, "true", NULL, NULL },
   { NULL, 0, { 0 }, 0, 0, NULL, NULL, NULL }
};

/*
 * Schedule -- see run_conf.c
 *
 * name handler value code flags default_value
 */
static RES_ITEM sch_items[] = {
   { "Name", CFG_TYPE_NAME, ITEM(res_sch.hdr.name), 0, CFG_ITEM_REQUIRED, NULL, NULL,
     "The name of the resource." },
   { "Description", CFG_TYPE_STR, ITEM(res_sch.hdr.desc), 0, 0, NULL, NULL, NULL },
   { "Run", CFG_TYPE_RUN, ITEM(res_sch.run), 0, 0, NULL, NULL, NULL },
   { "Enabled", CFG_TYPE_BOOL, ITEM(res_sch.enabled), 0, CFG_ITEM_DEFAULT, "true", NULL,
     "En- or disable this resource." },
   { NULL, 0, { 0 }, 0, 0, NULL, NULL, NULL }
};

/*
 * Pool resource
 *
 * name handler value code flags default_value
 */
static RES_ITEM pool_items[] = {
   { "Name", CFG_TYPE_NAME, ITEM(res_pool.hdr.name), 0, CFG_ITEM_REQUIRED, NULL, NULL,
     "The name of the resource." },
   { "Description", CFG_TYPE_STR, ITEM(res_pool.hdr.desc), 0, 0, NULL, NULL, NULL },
   { "PoolType", CFG_TYPE_POOLTYPE, ITEM(res_pool.pool_type), 0, CFG_ITEM_DEFAULT, "Backup", NULL, NULL },
   { "LabelFormat", CFG_TYPE_STRNAME, ITEM(res_pool.label_format), 0, 0, NULL, NULL, NULL },
   { "LabelType", CFG_TYPE_LABEL, ITEM(res_pool.LabelType), 0, 0, NULL, NULL, NULL },
   { "CleaningPrefix", CFG_TYPE_STRNAME, ITEM(res_pool.cleaning_prefix), 0, CFG_ITEM_DEFAULT, "CLN", NULL, NULL },
   { "UseCatalog", CFG_TYPE_BOOL, ITEM(res_pool.use_catalog), 0, CFG_ITEM_DEFAULT, "true", NULL, NULL },
   { "UseVolumeOnce", CFG_TYPE_BOOL, ITEM(res_pool.use_volume_once), 0, CFG_ITEM_DEPRECATED, NULL, "-12.4.0", NULL },
   { "PurgeOldestVolume", CFG_TYPE_BOOL, ITEM(res_pool.purge_oldest_volume), 0, 0, NULL, NULL, NULL },
   { "ActionOnPurge", CFG_TYPE_ACTIONONPURGE, ITEM(res_pool.action_on_purge), 0, 0, NULL, NULL, NULL },
   { "RecycleOldestVolume", CFG_TYPE_BOOL, ITEM(res_pool.recycle_oldest_volume), 0, 0, NULL, NULL, NULL },
   { "RecycleCurrentVolume", CFG_TYPE_BOOL, ITEM(res_pool.recycle_current_volume), 0, 0, NULL, NULL, NULL },
   { "MaximumVolumes", CFG_TYPE_PINT32, ITEM(res_pool.max_volumes), 0, 0, NULL, NULL, NULL },
   { "MaximumVolumeJobs", CFG_TYPE_PINT32, ITEM(res_pool.MaxVolJobs), 0, 0, NULL, NULL, NULL },
   { "MaximumVolumeFiles", CFG_TYPE_PINT32, ITEM(res_pool.MaxVolFiles), 0, 0, NULL, NULL, NULL },
   { "MaximumVolumeBytes", CFG_TYPE_SIZE64, ITEM(res_pool.MaxVolBytes), 0, 0, NULL, NULL, NULL },
   { "CatalogFiles", CFG_TYPE_BOOL, ITEM(res_pool.catalog_files), 0, CFG_ITEM_DEFAULT, "true", NULL, NULL },
   { "VolumeRetention", CFG_TYPE_TIME, ITEM(res_pool.VolRetention), 0, CFG_ITEM_DEFAULT, "31536000" /* 365 days */, NULL, NULL },
   { "VolumeUseDuration", CFG_TYPE_TIME, ITEM(res_pool.VolUseDuration), 0, 0, NULL, NULL, NULL },
   { "MigrationTime", CFG_TYPE_TIME, ITEM(res_pool.MigrationTime), 0, 0, NULL, NULL, NULL },
   { "MigrationHighBytes", CFG_TYPE_SIZE64, ITEM(res_pool.MigrationHighBytes), 0, 0, NULL, NULL, NULL },
   { "MigrationLowBytes", CFG_TYPE_SIZE64, ITEM(res_pool.MigrationLowBytes), 0, 0, NULL, NULL, NULL },
   { "NextPool", CFG_TYPE_RES, ITEM(res_pool.NextPool), R_POOL, 0, NULL, NULL, NULL },
   { "Storage", CFG_TYPE_ALIST_RES, ITEM(res_pool.storage), R_STORAGE, 0, NULL, NULL, NULL },
   { "AutoPrune", CFG_TYPE_BOOL, ITEM(res_pool.AutoPrune), 0, CFG_ITEM_DEFAULT, "true", NULL, NULL },
   { "Recycle", CFG_TYPE_BOOL, ITEM(res_pool.Recycle), 0, CFG_ITEM_DEFAULT, "true", NULL, NULL },
   { "RecyclePool", CFG_TYPE_RES, ITEM(res_pool.RecyclePool), R_POOL, 0, NULL, NULL, NULL },
   { "ScratchPool", CFG_TYPE_RES, ITEM(res_pool.ScratchPool), R_POOL, 0, NULL, NULL, NULL },
   { "Catalog", CFG_TYPE_RES, ITEM(res_pool.catalog), R_CATALOG, 0, NULL, NULL, NULL },
   { "FileRetention", CFG_TYPE_TIME, ITEM(res_pool.FileRetention), 0, 0, NULL, NULL, NULL },
   { "JobRetention", CFG_TYPE_TIME, ITEM(res_pool.JobRetention), 0, 0, NULL, NULL, NULL },
   { "MinimumBlockSize", CFG_TYPE_PINT32, ITEM(res_pool.MinBlocksize), 0, 0, NULL, NULL, NULL },
   { "MaximumBlockSize", CFG_TYPE_PINT32, ITEM(res_pool.MaxBlocksize), 0, 0, NULL, "14.2.0-", NULL },
   { NULL, 0, { 0 }, 0, 0, NULL, NULL, NULL }
};

/*
 * Counter Resource
 *
 * name handler value code flags default_value
 */
static RES_ITEM counter_items[] = {
   { "Name", CFG_TYPE_NAME, ITEM(res_counter.hdr.name), 0, CFG_ITEM_REQUIRED, NULL, NULL,
     "The name of the resource." },
   { "Description", CFG_TYPE_STR, ITEM(res_counter.hdr.desc), 0, 0, NULL, NULL, NULL },
   { "Minimum", CFG_TYPE_INT32, ITEM(res_counter.MinValue), 0, CFG_ITEM_DEFAULT, "0", NULL, NULL },
   { "Maximum", CFG_TYPE_PINT32, ITEM(res_counter.MaxValue), 0, CFG_ITEM_DEFAULT, "2147483647" /* INT32_MAX */, NULL, NULL },
   { "WrapCounter", CFG_TYPE_RES, ITEM(res_counter.WrapCounter), R_COUNTER, 0, NULL, NULL, NULL },
   { "Catalog", CFG_TYPE_RES, ITEM(res_counter.Catalog), R_CATALOG, 0, NULL, NULL, NULL },
   { NULL, 0, { 0 }, 0, 0, NULL, NULL, NULL }
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
   { "FileSet", fs_items, R_FILESET, sizeof(FILESETRES) },
   { "Pool", pool_items, R_POOL, sizeof(POOLRES) },
   { "Messages", msgs_items, R_MSGS, sizeof(MSGSRES) },
   { "Counter", counter_items, R_COUNTER, sizeof(COUNTERRES) },
   { "Profile", profile_items, R_PROFILE, sizeof(PROFILERES) },
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
 { "Command", CFG_TYPE_RUNSCRIPT_CMD, { (char **)&res_runscript }, SHELL_CMD, 0, NULL, NULL, NULL },
 { "Console", CFG_TYPE_RUNSCRIPT_CMD, { (char **)&res_runscript }, CONSOLE_CMD, 0, NULL, NULL, NULL },
 { "Target", CFG_TYPE_RUNSCRIPT_TARGET, { (char **)&res_runscript }, 0, 0, NULL, NULL, NULL },
 { "RunsOnSuccess", CFG_TYPE_RUNSCRIPT_BOOL, { (char **)&res_runscript.on_success }, 0, 0, NULL, NULL, NULL },
 { "RunsOnFailure", CFG_TYPE_RUNSCRIPT_BOOL, { (char **)&res_runscript.on_failure }, 0, 0, NULL, NULL, NULL },
 { "FailJobOnError", CFG_TYPE_RUNSCRIPT_BOOL, { (char **)&res_runscript.fail_on_error }, 0, 0, NULL, NULL, NULL },
 { "AbortJobOnError", CFG_TYPE_RUNSCRIPT_BOOL, { (char **)&res_runscript.fail_on_error }, 0, 0, NULL, NULL, NULL },
 { "RunsWhen", CFG_TYPE_RUNSCRIPT_WHEN, { (char **)&res_runscript.when }, 0, 0, NULL, NULL, NULL },
 { "RunsOnClient", CFG_TYPE_RUNSCRIPT_TARGET, { (char **)&res_runscript }, 0, 0, NULL, NULL, NULL }, /* TODO */
 { NULL, 0, { 0 }, 0, 0, NULL, NULL, NULL }
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
   { " ", L_NONE, JT_ARCHIVE },
   { " ", L_NONE, JT_RESTORE },
   { " ", L_NONE, JT_CONSOLIDATE },
   { NULL, 0, 0 }
};

/* Keywords (RHS) permitted in Job type records
 *
 * type_name job_type
 */
struct s_jt jobtypes[] = {
   { "Backup", JT_BACKUP },
   { "Admin", JT_ADMIN },
   { "Archive", JT_ARCHIVE },
   { "Verify", JT_VERIFY },
   { "Restore", JT_RESTORE },
   { "Migrate",JT_MIGRATE },
   { "Copy", JT_COPY },
   { "Consolidate", JT_CONSOLIDATE },
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

/*
 * Keywords (RHS) permitted in Authentication type records
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
 * Keywords (RHS) permitted in Restore replace type records
 *
 * name token
 */
struct s_kw ReplaceOptions[] = {
   { "Always", REPLACE_ALWAYS },
   { "IfNewer", REPLACE_IFNEWER },
   { "IfOlder", REPLACE_IFOLDER },
   { "Never", REPLACE_NEVER },
   { NULL, 0 }
};

/*
 * Keywords (RHS) permitted in ActionOnPurge type records
 *
 * name token
 */
struct s_kw ActionOnPurgeOptions[] = {
   { "None", ON_PURGE_NONE },
   { "Truncate", ON_PURGE_TRUNCATE },
   { NULL, 0 }
};

/*
 * Keywords (RHS) permitted in Volume status type records
 *
 * token is set to zero for all items as this
 * is not a mapping but a filter table.
 *
 * name token
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

/*
 * Keywords (RHS) permitted in Pool type records
 *
 * token is set to zero for all items as this
 * is not a mapping but a filter table.
 *
 * name token
 */
struct s_kw PoolTypes[] = {
   { "Backup", 0 },
   { "Copy", 0 },
   { "Cloned", 0 },
   { "Archive", 0 },
   { "Migration", 0 },
   { "Scratch", 0 },
   { NULL, 0 }
};

#ifdef HAVE_JANSSON
json_t *json_item(s_jl *item)
{
   json_t *json = json_object();

   json_object_set_new(json, "level", json_integer(item->level));
   json_object_set_new(json, "type", json_integer(item->job_type));

   return json;
}

json_t *json_item(s_jt *item)
{
   json_t *json = json_object();

   json_object_set_new(json, "type", json_integer(item->job_type));

   return json;
}

json_t *json_datatype_header(const int type, const char *typeclass)
{
   json_t *json = json_object();
   const char *description = datatype_to_description(type);

   json_object_set_new(json, "number", json_integer(type));

   if (description) {
      json_object_set_new(json, "description", json_string(description));
   }

   if (typeclass) {
      json_object_set_new(json, "class", json_string(typeclass));
   }

   return json;
}

json_t *json_datatype(const int type)
{
   return json_datatype_header(type, NULL);
}

json_t *json_datatype(const int type, s_kw items[])
{
   json_t *json = json_datatype_header(type, "keyword");
   if (items) {
      json_t *values = json_object();
      for (int i = 0; items[i].name; i++) {
         json_object_set_new(values, items[i].name, json_item(&items[i]));
      }
      json_object_set_new(json, "values", values);
   }
   return json;
}

json_t *json_datatype(const int type, s_jl items[])
{
   // FIXME: level_name keyword is not unique
   json_t *json = json_datatype_header(type, "keyword");
   if (items) {
      json_t *values = json_object();
      for (int i = 0; items[i].level_name; i++) {
         json_object_set_new(values, items[i].level_name, json_item(&items[i]));
      }
      json_object_set_new(json, "values", values);
   }
   return json;
}

json_t *json_datatype(const int type, s_jt items[])
{
   json_t *json = json_datatype_header(type, "keyword");
   if (items) {
      json_t *values = json_object();
      for (int i = 0; items[i].type_name; i++) {
         json_object_set_new(values, items[i].type_name, json_item(&items[i]));
      }
      json_object_set_new(json, "values", values);
   }
   return json;
}

json_t *json_datatype(const int type, RES_ITEM items[])
{
   json_t *json = json_datatype_header(type, "sub");
   if (items) {
      json_t *values = json_object();
      for (int i = 0; items[i].name; i++) {
         json_object_set_new(values, items[i].name, json_item(&items[i]));
      }
      json_object_set_new(json, "values", values);
   }
   return json;
}

/*
 * Print configuration file schema in json format
 */
bool print_config_schema_json(POOL_MEM &buffer)
{
   DATATYPE_NAME *datatype;
   RES_TABLE *resources = my_config->m_resources;

   initialize_json();

   json_t *json = json_object();
   json_object_set_new(json, "format-version", json_integer(2));
   json_object_set_new(json, "component", json_string("bareos-dir"));
   json_object_set_new(json, "version", json_string(VERSION));

   /*
    * Resources
    */
   json_t *resource = json_object();
   json_object_set(json, "resource", resource);
   json_t *bareos_dir = json_object();
   json_object_set(resource, "bareos-dir", bareos_dir);

   for (int r = 0; resources[r].name; r++) {
      RES_TABLE resource = my_config->m_resources[r];
      json_object_set(bareos_dir, resource.name, json_items(resource.items));
   }

   /*
    * Datatypes
    */
   json_t *json_datatype_obj = json_object();
   json_object_set(json, "datatype", json_datatype_obj);

   int d = 0;
   while (get_datatype(d)->name != NULL) {
      datatype = get_datatype(d);

      switch (datatype->number) {
      case CFG_TYPE_RUNSCRIPT:
         json_object_set(json_datatype_obj, datatype_to_str(datatype->number), json_datatype(CFG_TYPE_RUNSCRIPT, runscript_items));
         break;
      case CFG_TYPE_INCEXC:
         json_object_set(json_datatype_obj, datatype_to_str(datatype->number), json_incexc(CFG_TYPE_INCEXC));
         break;
      case CFG_TYPE_OPTIONS:
         json_object_set(json_datatype_obj, datatype_to_str(datatype->number), json_options(CFG_TYPE_OPTIONS));
         break;
      case CFG_TYPE_PROTOCOLTYPE:
         json_object_set(json_datatype_obj, datatype_to_str(datatype->number), json_datatype(CFG_TYPE_PROTOCOLTYPE, backupprotocols));
         break;
      case CFG_TYPE_AUTHPROTOCOLTYPE:
         json_object_set(json_datatype_obj, datatype_to_str(datatype->number), json_datatype(CFG_TYPE_AUTHPROTOCOLTYPE, authprotocols));
         break;
      case CFG_TYPE_AUTHTYPE:
         json_object_set(json_datatype_obj, datatype_to_str(datatype->number), json_datatype(CFG_TYPE_AUTHTYPE, authmethods));
         break;
      case CFG_TYPE_LEVEL:
         json_object_set(json_datatype_obj, datatype_to_str(datatype->number), json_datatype(CFG_TYPE_LEVEL, joblevels));
         break;
      case CFG_TYPE_JOBTYPE:
         json_object_set(json_datatype_obj, datatype_to_str(datatype->number), json_datatype(CFG_TYPE_JOBTYPE, jobtypes));
         break;
      case CFG_TYPE_MIGTYPE:
         json_object_set(json_datatype_obj, datatype_to_str(datatype->number), json_datatype(CFG_TYPE_MIGTYPE, migtypes));
         break;
      case CFG_TYPE_REPLACE:
         json_object_set(json_datatype_obj, datatype_to_str(datatype->number), json_datatype(CFG_TYPE_REPLACE, ReplaceOptions));
         break;
      case CFG_TYPE_ACTIONONPURGE:
         json_object_set(json_datatype_obj, datatype_to_str(datatype->number), json_datatype(CFG_TYPE_ACTIONONPURGE, ActionOnPurgeOptions));
         break;
      case CFG_TYPE_POOLTYPE:
         json_object_set(json_datatype_obj, datatype_to_str(datatype->number), json_datatype(CFG_TYPE_POOLTYPE, PoolTypes));
         break;
      case CFG_TYPE_RUN:
         json_object_set(json_datatype_obj, datatype_to_str(datatype->number), json_datatype(CFG_TYPE_RUN, RunFields));
         break;
      default:
         json_object_set(json_datatype_obj, datatype_to_str(datatype->number), json_datatype(datatype->number));
         break;
      }
      d++;
   }

   /*
    * following datatypes are ignored:
    * - VolumeStatus: only used in ua_dotcmds, not a datatype
    * - FS_option_kw: from inc_conf. Replaced by CFG_TYPE_OPTIONS", options_items.
    * - FS_options: are they needed?
    */

   pm_strcat(buffer, json_dumps(json, JSON_INDENT(2)));
   json_decref(json);

   return true;
}
#else
bool print_config_schema_json(POOL_MEM &buffer)
{
   pm_strcat(buffer, "{ \"success\": false, \"message\": \"not available\" }");

   return false;
}
#endif

static inline bool cmdline_item(POOL_MEM *buffer, RES_ITEM *item)
{
   POOL_MEM temp;
   POOL_MEM key;
   const char *nomod = "";
   const char *mod_start = nomod;
   const char *mod_end = nomod;

   if (item->flags & CFG_ITEM_ALIAS) {
      return false;
   }

   if (item->flags & CFG_ITEM_DEPRECATED) {
      return false;
   }

   if (item->flags & CFG_ITEM_NO_EQUALS) {
      /* TODO: currently not supported */
      return false;
   }

   if (!(item->flags & CFG_ITEM_REQUIRED)) {
      mod_start = "[";
      mod_end = "]";
   }

   /*
    * Tab completion only supports lower case keywords.
    */
   key.strcat(item->name);
   key.toLower();

   temp.bsprintf(" %s%s=<%s>%s", mod_start, key.c_str(), datatype_to_str(item->type), mod_end);
   pm_strcat(buffer, temp.c_str());

   return true;
}

static inline bool cmdline_items(POOL_MEM *buffer, RES_ITEM items[])
{
   if (!items) {
      return false;
   }

   for (int i = 0; items[i].name; i++) {
      cmdline_item(buffer, &items[i]);
   }

   return true;
}

/*
 * Get the usage string for the console "configure" command.
 *
 * This will be all available resource directives.
 * They are formated in a way to be usable for command line completion.
 */
const char *get_configure_usage_string()
{
   POOL_MEM resourcename;

   if (!configure_usage_string) {
      configure_usage_string = new POOL_MEM(PM_BSOCK);
   }

   /*
    * Only fill the configure_usage_string once. The content is static.
    */
   if (configure_usage_string->strlen() == 0) {
      configure_usage_string->strcpy("add [");
      for (int r = 0; resources[r].name; r++) {
         if (resources[r].items) {
            resourcename.strcpy(resources[r].name);
            resourcename.toLower();
            configure_usage_string->strcat(resourcename);
            cmdline_items(configure_usage_string, resources[r].items);
         }
         if (resources[r+1].items) {
            configure_usage_string->strcat("] | [");
         }
      }
      configure_usage_string->strcat("]");
   }

   return configure_usage_string->c_str();
}

void destroy_configure_usage_string()
{
   if (configure_usage_string) {
      delete configure_usage_string;
      configure_usage_string = NULL;
   }
}

/*
 * Propagate the settings from source BRSRES to dest BRSRES using the RES_ITEMS array.
 */
static void propagate_resource(RES_ITEM *items, BRSRES *source, BRSRES *dest)
{
   uint32_t offset;

   for (int i = 0; items[i].name; i++) {
      if (!bit_is_set(i, dest->hdr.item_present) &&
           bit_is_set(i, source->hdr.item_present)) {
         offset = (char *)(items[i].value) - (char *)&res_all;
         switch (items[i].type) {
         case CFG_TYPE_STR:
         case CFG_TYPE_DIR: {
            char **def_svalue, **svalue;

            /*
             * Handle strings and directory strings
             */
            def_svalue = (char **)((char *)(source) + offset);
            svalue = (char **)((char *)dest + offset);
            if (*svalue) {
               free(*svalue);
            }
            *svalue = bstrdup(*def_svalue);
            set_bit(i, dest->hdr.item_present);
            set_bit(i, dest->hdr.inherit_content);
            break;
         }
         case CFG_TYPE_RES: {
            char **def_svalue, **svalue;

            /*
             * Handle resources
             */
            def_svalue = (char **)((char *)(source) + offset);
            svalue = (char **)((char *)dest + offset);
            if (*svalue) {
               Pmsg1(000, _("Hey something is wrong. p=0x%lu\n"), *svalue);
            }
            *svalue = *def_svalue;
            set_bit(i, dest->hdr.item_present);
            set_bit(i, dest->hdr.inherit_content);
            break;
         }
         case CFG_TYPE_ALIST_STR: {
            const char *str;
            alist *orig_list, **new_list;

            /*
             * Handle alist strings
             */
            orig_list = *(alist **)((char *)(source) + offset);

            /*
             * See if there is anything on the list.
             */
            if (orig_list && orig_list->size()) {
               new_list = (alist **)((char *)(dest) + offset);

               if (!*new_list) {
                  *new_list = New(alist(10, owned_by_alist));
               }

               foreach_alist(str, orig_list) {
                  (*new_list)->append(bstrdup(str));
               }

               set_bit(i, dest->hdr.item_present);
               set_bit(i, dest->hdr.inherit_content);
            }
            break;
         }
         case CFG_TYPE_ALIST_RES: {
            RES *res;
            alist *orig_list, **new_list;

            /*
             * Handle alist resources
             */
            orig_list = *(alist **)((char *)(source) + offset);

            /*
             * See if there is anything on the list.
             */
            if (orig_list && orig_list->size()) {
               new_list = (alist **)((char *)(dest) + offset);

               if (!*new_list) {
                  *new_list = New(alist(10, not_owned_by_alist));
               }

               foreach_alist(res, orig_list) {
                  (*new_list)->append(res);
               }

               set_bit(i, dest->hdr.item_present);
               set_bit(i, dest->hdr.inherit_content);
            }
            break;
         }
         case CFG_TYPE_ACL: {
            const char *str;
            alist *orig_list, **new_list;

            /*
             * Handle ACL lists.
             */
            orig_list = ((alist **)((char *)(source) + offset))[items[i].code];

            /*
             * See if there is anything on the list.
             */
            if (orig_list && orig_list->size()) {
               new_list = &(((alist **)((char *)(dest) + offset))[items[i].code]);

               if (!*new_list) {
                  *new_list = New(alist(10, owned_by_alist));
               }

               foreach_alist(str, orig_list) {
                  (*new_list)->append(bstrdup(str));
               }

               set_bit(i, dest->hdr.item_present);
               set_bit(i, dest->hdr.inherit_content);
            }
            break;
         }
         case CFG_TYPE_BIT:
         case CFG_TYPE_PINT32:
         case CFG_TYPE_JOBTYPE:
         case CFG_TYPE_PROTOCOLTYPE:
         case CFG_TYPE_LEVEL:
         case CFG_TYPE_INT32:
         case CFG_TYPE_SIZE32:
         case CFG_TYPE_MIGTYPE:
         case CFG_TYPE_REPLACE: {
            uint32_t *def_ivalue, *ivalue;

            /*
             * Handle integer fields
             *    Note, our store_bit does not handle bitmaped fields
             */
            def_ivalue = (uint32_t *)((char *)(source) + offset);
            ivalue = (uint32_t *)((char *)dest + offset);
            *ivalue = *def_ivalue;
            set_bit(i, dest->hdr.item_present);
            set_bit(i, dest->hdr.inherit_content);
            break;
         }
         case CFG_TYPE_TIME:
         case CFG_TYPE_SIZE64:
         case CFG_TYPE_INT64:
         case CFG_TYPE_SPEED: {
            int64_t *def_lvalue, *lvalue;

            /*
             * Handle 64 bit integer fields
             */
            def_lvalue = (int64_t *)((char *)(source) + offset);
            lvalue = (int64_t *)((char *)dest + offset);
            *lvalue = *def_lvalue;
            set_bit(i, dest->hdr.item_present);
            set_bit(i, dest->hdr.inherit_content);
            break;
         }
         case CFG_TYPE_BOOL: {
            bool *def_bvalue, *bvalue;

            /*
             * Handle bool fields
             */
            def_bvalue = (bool *)((char *)(source) + offset);
            bvalue = (bool *)((char *)dest + offset);
            *bvalue = *def_bvalue;
            set_bit(i, dest->hdr.item_present);
            set_bit(i, dest->hdr.inherit_content);
            break;
         }
         case CFG_TYPE_AUTOPASSWORD: {
            s_password *s_pwd, *d_pwd;

            /*
             * Handle password fields
             */
            s_pwd = (s_password *)((char *)(source) + offset);
            d_pwd = (s_password *)((char *)(dest) + offset);

            d_pwd->encoding = s_pwd->encoding;
            d_pwd->value = bstrdup(s_pwd->value);
            set_bit(i, dest->hdr.item_present);
            set_bit(i, dest->hdr.inherit_content);
            break;
         }
         default:
            Dmsg2(200, "Don't know how to propagate resource %s of configtype %d\n", items[i].name, items[i].type);
            break;
         }
      }
   }
}

/*
 * Ensure that all required items are present
 */
static bool validate_resource(RES_ITEM *items, BRSRES *res, const char *res_type)
{
   for (int i = 0; items[i].name; i++) {
      if (items[i].flags & CFG_ITEM_REQUIRED) {
         if (!bit_is_set(i, res->hdr.item_present)) {
            Jmsg(NULL, M_ERROR, 0,
                 _("\"%s\" directive in %s \"%s\" resource is required, but not found.\n"),
                 items[i].name, res_type, res->name());
            return false;
         }
      }

      /*
       * If this triggers, take a look at lib/parse_conf.h
       */
      if (i >= MAX_RES_ITEMS) {
         Emsg1(M_ERROR, 0, _("Too many items in %s resource\n"), res_type);
         return false;
      }
   }

   return true;
}

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
   RUNSCRIPT *runscript;
   alist *list;

   list = *item->alistvalue;
   if (bstrcasecmp(item->name, "runscript")) {
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
                  Mmsg(temp, "client run before job = \"%s\"\n", cmdbuf);
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

               if (!bstrcasecmp(when, "never")) { /* suppress default value */
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
   int i, nr_items;
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
   char *months[] = {
      (char *)"Jan",
      (char *)"Feb",
      (char *)"Mar",
      (char *)"Apr",
      (char *)"May",
      (char *)"Jun",
      (char *)"Jul",
      (char *)"Aug",
      (char *)"Sep",
      (char *)"Oct",
      (char *)"Nov",
      (char *)"Dec"
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
         POOL_MEM interval; /* is one entry of day/month/week etc. */

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

         if (run->vfull_pool) {
            Mmsg(temp, "virtualfullpool=\"%s\" ", run->vfull_pool->name());
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
                  pm_strcat(run_str, joblevels[j].level_name);
                  pm_strcat(run_str, " ");
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
         pm_strcpy(temp, "");

         /*
          * First see if not all bits are set.
          */
         all_set = true;
         nr_items = 31;
         for (i = 0; i < nr_items; i++) {
            if (!bit_is_set(i, run->mday)) {
               all_set = false;
            }
         }

         if (!all_set) {
            interval_start = -1;

            for (i = 0; i < nr_items; i++) {
               if (bit_is_set(i, run->mday)) {
                  if (interval_start == -1) {   /* bit is set and we are not in an interval */
                     interval_start = i;        /* start an interval */
                     Dmsg1(200, "starting interval at %d\n", i + 1);
                     Mmsg(interval, ",%d", i + 1);
                     pm_strcat(temp, interval.c_str());
                  }
               }

               if (!bit_is_set(i, run->mday)) {
                  if (interval_start != -1) {   /* bit is unset and we are in an interval */
                     if ((i - interval_start) > 1) {
                        Dmsg2(200, "found end of interval from %d to %d\n", interval_start + 1, i);
                        Mmsg(interval, "-%d", i);
                        pm_strcat(temp, interval.c_str());
                     }
                     interval_start = -1;       /* end the interval */
                  }
               }
            }

            /*
             * See if we are still in an interval and the last bit is also set then the interval stretches to the last item.
             */
            i = nr_items - 1;
            if (interval_start != -1 && bit_is_set(i, run->mday)) {
               if ((i - interval_start) > 1) {
                  Dmsg2(200, "found end of interval from %d to %d\n", interval_start + 1, i + 1);
                  Mmsg(interval, "-%d", i + 1);
                  pm_strcat(temp, interval.c_str());
               }
            }

            pm_strcat(temp, " ");
            pm_strcat(run_str, temp.c_str() + 1); /* jump over first comma*/
         }

         /*
          * run->wom output is 1st, 2nd... 5th comma separated
          *                    first, second, third... is also allowed
          *                    but we ignore that for now
          */
         all_set = true;
         nr_items = 5;
         for (i = 0; i < nr_items; i++) {
            if (!bit_is_set(i, run->wom)) {
               all_set = false;
            }
         }

         if (!all_set) {
            interval_start = -1;

            pm_strcpy(temp, "");
            for (i = 0; i < nr_items; i++) {
               if (bit_is_set(i, run->wom)) {
                  if (interval_start == -1) {   /* bit is set and we are not in an interval */
                     interval_start = i;        /* start an interval */
                     Dmsg1(200, "starting interval at %s\n", ordinals[i]);
                     Mmsg(interval, ",%s", ordinals[i]);
                     pm_strcat(temp, interval.c_str());
                  }
               }

               if (!bit_is_set(i, run->wom)) {
                  if (interval_start != -1) {   /* bit is unset and we are in an interval */
                     if ((i - interval_start) > 1) {
                        Dmsg2(200, "found end of interval from %s to %s\n", ordinals[interval_start], ordinals[i - 1]);
                        Mmsg(interval, "-%s", ordinals[i - 1]);
                        pm_strcat(temp, interval.c_str());
                     }
                     interval_start = -1;       /* end the interval */
                  }
               }
            }

            /*
             * See if we are still in an interval and the last bit is also set then the interval stretches to the last item.
             */
            i = nr_items - 1;
            if (interval_start != -1 && bit_is_set(i, run->wom)) {
               if ((i - interval_start) > 1) {
                  Dmsg2(200, "found end of interval from %s to %s\n", ordinals[interval_start], ordinals[i]);
                  Mmsg(interval, "-%s", ordinals[i]);
                  pm_strcat(temp, interval.c_str());
               }
            }

            pm_strcat(temp, " ");
            pm_strcat(run_str, temp.c_str() + 1); /* jump over first comma*/
         }

         /*
          * run->wday output is Sun, Mon, ..., Sat comma separated
          */
         all_set = true;
         nr_items = 7;
         for (i = 0; i < nr_items; i++) {
            if (!bit_is_set(i, run->wday)) {
               all_set = false;
            }
         }

         if (!all_set) {
            interval_start = -1;

            pm_strcpy(temp, "");
            for (i = 0; i < nr_items; i++) {
               if (bit_is_set(i, run->wday)) {
                  if (interval_start == -1) {   /* bit is set and we are not in an interval */
                     interval_start = i;        /* start an interval */
                     Dmsg1(200, "starting interval at %s\n", weekdays[i]);
                     Mmsg(interval, ",%s", weekdays[i]);
                     pm_strcat(temp, interval.c_str());
                  }
               }

               if (!bit_is_set(i, run->wday)) {
                  if (interval_start != -1) {   /* bit is unset and we are in an interval */
                     if ((i - interval_start) > 1) {
                        Dmsg2(200, "found end of interval from %s to %s\n", weekdays[interval_start], weekdays[i - 1]);
                        Mmsg(interval, "-%s", weekdays[i - 1]);
                        pm_strcat(temp, interval.c_str());
                     }
                     interval_start = -1;       /* end the interval */
                  }
               }
            }

            /*
             * See if we are still in an interval and the last bit is also set then the interval stretches to the last item.
             */
            i = nr_items - 1;
            if (interval_start != -1 && bit_is_set(i, run->wday)) {
               if ((i - interval_start) > 1) {
                  Dmsg2(200, "found end of interval from %s to %s\n", weekdays[interval_start], weekdays[i]);
                  Mmsg(interval, "-%s", weekdays[i]);
                  pm_strcat(temp, interval.c_str());
               }
            }

            pm_strcat(temp, " ");
            pm_strcat(run_str, temp.c_str() + 1); /* jump over first comma*/
         }

         /*
          * run->month output is Jan, Feb, ..., Dec comma separated
          */
         all_set = true;
         nr_items = 12;
         for (i = 0; i < nr_items; i++) {
            if (!bit_is_set(i, run->month)) {
               all_set = false;
            }
         }

         if (!all_set) {
            interval_start = -1;

            pm_strcpy(temp, "");
            for (i = 0; i < nr_items; i++) {
               if (bit_is_set(i, run->month)) {
                  if (interval_start == -1) {   /* bit is set and we are not in an interval */
                     interval_start = i;        /* start an interval */
                     Dmsg1(200, "starting interval at %s\n", months[i]);
                     Mmsg(interval, ",%s", months[i]);
                     pm_strcat(temp, interval.c_str());
                  }
               }

               if (!bit_is_set(i, run->month)) {
                  if (interval_start != -1) {   /* bit is unset and we are in an interval */
                     if ((i - interval_start) > 1) {
                        Dmsg2(200, "found end of interval from %s to %s\n", months[interval_start], months[i - 1]);
                        Mmsg(interval, "-%s", months[i - 1]);
                        pm_strcat(temp, interval.c_str());
                     }
                     interval_start = -1;       /* end the interval */
                  }
               }
            }

            /*
             * See if we are still in an interval and the last bit is also set then the interval stretches to the last item.
             */
            i = nr_items - 1;
            if (interval_start != -1 && bit_is_set(i, run->month)) {
               if ((i - interval_start) > 1) {
                  Dmsg2(200, "found end of interval from %s to %s\n", months[interval_start], months[i]);
                  Mmsg(interval, "-%s", months[i]);
                  pm_strcat(temp, interval.c_str());
               }
            }

            pm_strcat(temp, " ");
            pm_strcat(run_str, temp.c_str() + 1); /* jump over first comma*/
         }

         /*
          * run->woy output is w00 - w53, comma separated
          */
         all_set = true;
         nr_items = 54;
         for (i = 0; i < nr_items; i++) {
            if (!bit_is_set(i, run->woy)) {
               all_set = false;
            }
         }

         if (!all_set) {
            interval_start = -1;

            pm_strcpy(temp, "");
            for (i = 0; i < nr_items; i++) {
               if (bit_is_set(i, run->woy)) {
                  if (interval_start == -1) {   /* bit is set and we are not in an interval */
                     interval_start = i;        /* start an interval */
                     Dmsg1(200, "starting interval at w%02d\n", i);
                     Mmsg(interval, ",w%02d", i);
                     pm_strcat(temp, interval.c_str());
                  }
               }

               if (!bit_is_set(i, run->woy)) {
                  if (interval_start != -1) {   /* bit is unset and we are in an interval */
                     if ((i - interval_start) > 1) {
                        Dmsg2(200, "found end of interval from w%02d to w%02d\n", interval_start, i - 1);
                        Mmsg(interval, "-w%02d", i - 1);
                        pm_strcat(temp, interval.c_str());
                     }
                     interval_start = -1;       /* end the interval */
                  }
               }
            }

            /*
             * See if we are still in an interval and the last bit is also set then the interval stretches to the last item.
             */
            i = nr_items - 1;
            if (interval_start != -1 && bit_is_set(i, run->woy)) {
               if ((i - interval_start) > 1) {
                  Dmsg2(200, "found end of interval from w%02d to w%02d\n", interval_start, i);
                  Mmsg(interval, "-w%02d", i);
                  pm_strcat(temp, interval.c_str());
               }
            }

            pm_strcat(temp, " ");
            pm_strcat(run_str, temp.c_str() + 1); /* jump over first comma*/
         }

         /*
          * run->hour output is HH:MM for hour and minute though its a bitfield.
          * only "hourly" sets all bits.
          */
         pm_strcpy(temp, "");
         for (i = 0; i < 24; i++) {
            if bit_is_set(i, run->hour) {
               Mmsg(temp, "at %02d:%02d\n", i, run->minute);
               pm_strcat(run_str, temp.c_str());
            }
         }

         /*
          * run->minute output is smply the minute in HH:MM
          */
         pm_strcat(cfg_str, run_str.c_str());

         run = run->next;
      } /* loop over runs */
   }
}

bool FILESETRES::print_config(POOL_MEM &buff, bool hide_sensitive_data)
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

         indent_config_item(cfg_str, 1, "Include {\n");

         /*
          * Start options block
          */
         if (incexe->num_opts > 0) {
            for (int j = 0; j < incexe->num_opts; j++) {
               FOPTS *fo = incexe->opts_list[j];
               bool enhanced_wild = false;

               indent_config_item(cfg_str, 2, "Options {\n");

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
                  case 'c':
                     indent_config_item(cfg_str, 3, "CheckFileChanges = Yes\n");
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
                     indent_config_item(cfg_str, 3, "Exclude = Yes\n");
                     break;
                  case 'f':
                     indent_config_item(cfg_str, 3, "OneFS = No\n");
                     break;
                  case 'h':                 /* no recursion */
                     indent_config_item(cfg_str, 3, "Recurse = No\n");
                     break;
                  case 'H':                 /* no hard link handling */
                     indent_config_item(cfg_str, 3, "Hardlinks = No\n");
                     break;
                  case 'i':
                     indent_config_item(cfg_str, 3, "IgnoreCase = Yes\n");
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
                        indent_config_item(cfg_str, 3, "Signature = SHA1\n");
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
                     switch (*p) {
                     case '0':
                     case '1':
                     case '2':
                     case '3':
                     case '4':
                     case '5':
                     case '6':
                     case '7':
                     case '8':
                     case '9':
                        Mmsg(temp, "GZIP%c\n", *p);
                        pm_strcat(cfg_str, temp.c_str());
                        break;
                     case 'o':
                        Mmsg(temp, "LZO\n");
                        pm_strcat(cfg_str, temp.c_str());
                        break;
                     case 'f':
                        p++;                /* skip f */
                        switch (*p) {
                        case 'f':
                           Mmsg(temp, "LZFAST\n");
                           pm_strcat(cfg_str, temp.c_str());
                           break;
                        case '4':
                           Mmsg(temp, "LZ4\n");
                           pm_strcat(cfg_str, temp.c_str());
                           break;
                        case 'h':
                           Mmsg(temp, "LZ4HC\n");
                           pm_strcat(cfg_str, temp.c_str());
                           break;
                        default:
                           Emsg1(M_ERROR, 0, _("Unknown compression include/exclude option: %c\n"), *p);
                           break;
                        }
                        break;
                     default:
                        Emsg1(M_ERROR, 0, _("Unknown compression include/exclude option: %c\n"), *p);
                        break;
                     }
                     break;
                  case 'X':
                     indent_config_item(cfg_str, 3, "Xattr = Yes\n");
                     break;
                  case 'x':
                     indent_config_item(cfg_str, 3, "AutoExclude = No\n");
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
void dump_resource(int type, RES *ures,
                   void sendit(void *sock, const char *fmt, ...),
                   void *sock, bool hide_sensitive_data)
{
   URES *res = (URES *)ures;
   bool recurse = true;
   POOL_MEM buf;
   UAContext *ua = (UAContext *)sock;

   if (!res) {
      sendit(sock, _("No %s resource defined\n"), res_to_str(type));
      return;
   }

   if (type < 0) { /* no recursion */
      type = -type;
      recurse = false;
   }

   switch (type) {
   case R_DIRECTOR:
      res->res_dir.print_config(buf, hide_sensitive_data);
      sendit(sock, "%s", buf.c_str());
      break;
   case R_PROFILE:
      res->res_profile.print_config(buf, hide_sensitive_data);
      sendit(sock, "%s", buf.c_str());
      break;
   case R_CONSOLE:
      res->res_con.print_config(buf, hide_sensitive_data);
      sendit(sock, "%s", buf.c_str());
      break;
   case R_COUNTER:
      res->res_counter.print_config(buf, hide_sensitive_data);
      sendit(sock, "%s", buf.c_str());
      break;
   case R_CLIENT:
      if (!ua || acl_access_ok(ua, Client_ACL, res->res_client.name())) {
         res->res_client.print_config(buf, hide_sensitive_data);
         sendit(sock, "%s", buf.c_str());
      }
      break;
   case R_DEVICE:
      res->res_dev.print_config(buf, hide_sensitive_data);
      sendit(sock, "%s", buf.c_str());
      break;
   case R_STORAGE:
      if (!ua || acl_access_ok(ua, Storage_ACL, res->res_store.name())) {
         res->res_store.print_config(buf, hide_sensitive_data);
         sendit(sock, "%s", buf.c_str());
      }
      break;
   case R_CATALOG:
      if (!ua || acl_access_ok(ua, Catalog_ACL, res->res_cat.name())) {
         res->res_cat.print_config(buf, hide_sensitive_data);
         sendit(sock, "%s", buf.c_str());
      }
      break;
   case R_JOBDEFS:
   case R_JOB:
      if (!ua || acl_access_ok(ua, Job_ACL, res->res_job.name())) {
         res->res_job.print_config(buf, hide_sensitive_data);
         sendit(sock, "%s", buf.c_str());
      }
      break;
   case R_FILESET: {
      if (!ua || acl_access_ok(ua, FileSet_ACL, res->res_fs.name())) {
         res->res_fs.print_config(buf, hide_sensitive_data);
         sendit(sock, "%s", buf.c_str());
      }
      break;
   }
   case R_SCHEDULE:
      if (!ua || acl_access_ok(ua, Schedule_ACL, res->res_sch.name())) {
         res->res_sch.print_config(buf, hide_sensitive_data);
         sendit(sock, "%s", buf.c_str());
      }
      break;
   case R_POOL:
      if (!ua || acl_access_ok(ua, Pool_ACL, res->res_pool.name())) {
        res->res_pool.print_config(buf, hide_sensitive_data);
        sendit(sock, "%s", buf.c_str());
      }
      break;
   case R_MSGS:
      res->res_msgs.print_config(buf, hide_sensitive_data);
      sendit(sock, "%s", buf.c_str());
      break;
   default:
      sendit(sock, _("Unknown resource type %d in dump_resource.\n"), type);
      break;
   }

   if (recurse && res->res_dir.hdr.next) {
      dump_resource(type, res->res_dir.hdr.next, sendit, sock, hide_sensitive_data);
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

   if (!res)
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
      if (res->res_dir.verid) {
         free(res->res_dir.verid);
      }
      if (res->res_dir.keyencrkey.value) {
         free(res->res_dir.keyencrkey.value);
      }
      if (res->res_dir.audit_events) {
         delete res->res_dir.audit_events;
      }
      if (res->res_dir.secure_erase_cmdline) {
         free(res->res_dir.secure_erase_cmdline);
      }
      if (res->res_dir.log_timestamp_format) {
         free(res->res_dir.log_timestamp_format);
      }
      free_tls_t(res->res_dir.tls);
      break;
   case R_DEVICE:
   case R_COUNTER:
      break;
   case R_PROFILE:
      for (int i = 0; i < Num_ACL; i++) {
         if (res->res_profile.ACL_lists[i]) {
            delete res->res_profile.ACL_lists[i];
            res->res_profile.ACL_lists[i] = NULL;
         }
      }
      break;
   case R_CONSOLE:
      if (res->res_con.password.value) {
         free(res->res_con.password.value);
      }
      if (res->res_con.profiles) {
         delete res->res_con.profiles;
      }
      for (int i = 0; i < Num_ACL; i++) {
         if (res->res_con.ACL_lists[i]) {
            delete res->res_con.ACL_lists[i];
            res->res_con.ACL_lists[i] = NULL;
         }
      }
      free_tls_t(res->res_con.tls);
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
      if (res->res_client.rcs) {
         free(res->res_client.rcs);
      }
      free_tls_t(res->res_client.tls);
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
      if (res->res_store.changer_device) {
         free(res->res_store.changer_device);
      }
      if (res->res_store.tape_devices) {
         delete res->res_store.tape_devices;
      }
      if (res->res_store.device) {
         delete res->res_store.device;
      }
      if (res->res_store.rss) {
         if (res->res_store.rss->storage_mappings) {
            delete res->res_store.rss->storage_mappings;
         }
         if (res->res_store.rss->vol_list) {
            if (res->res_store.rss->vol_list->contents) {
               vol_list_t *vl;

               foreach_dlist(vl, res->res_store.rss->vol_list->contents) {
                  if (vl->VolName) {
                     free(vl->VolName);
                  }
               }
               res->res_store.rss->vol_list->contents->destroy();
               delete res->res_store.rss->vol_list->contents;
            }
            free(res->res_store.rss->vol_list);
         }
         pthread_mutex_destroy(&res->res_store.rss->changer_lock);
         free(res->res_store.rss);
      }
      free_tls_t(res->res_store.tls);
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
      if (res->res_job.selection_pattern) {
         free(res->res_job.selection_pattern);
      }
      if (res->res_job.run_cmds) {
         delete res->res_job.run_cmds;
      }
      if (res->res_job.storage) {
         delete res->res_job.storage;
      }
      if (res->res_job.FdPluginOptions) {
         delete res->res_job.FdPluginOptions;
      }
      if (res->res_job.SdPluginOptions) {
         delete res->res_job.SdPluginOptions;
      }
      if (res->res_job.DirPluginOptions) {
         delete res->res_job.DirPluginOptions;
      }
      if (res->res_job.base) {
         delete res->res_job.base;
      }
      if (res->res_job.RunScripts) {
         free_runscripts(res->res_job.RunScripts);
         delete res->res_job.RunScripts;
      }
      if (res->res_job.rjs) {
         free(res->res_job.rjs);
      }
      break;
   case R_MSGS:
      if (res->res_msgs.mail_cmd) {
         free(res->res_msgs.mail_cmd);
      }
      if (res->res_msgs.operator_cmd) {
         free(res->res_msgs.operator_cmd);
      }
      if (res->res_msgs.timestamp_format) {
         free(res->res_msgs.timestamp_format);
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

static bool update_resource_pointer(int type, RES_ITEM *items)
{
   URES *res;
   bool result = true;

   switch (type) {
   case R_PROFILE:
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
      if (!(res = (URES *)GetResWithName(R_POOL, res_all.res_pool.name()))) {
         Emsg1(M_ERROR, 0, _("Cannot find Pool resource %s\n"), res_all.res_pool.name());
         return false;
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
      if (!(res = (URES *)GetResWithName(R_CONSOLE, res_all.res_con.name()))) {
         Emsg1(M_ERROR, 0, _("Cannot find Console resource %s\n"), res_all.res_con.name());
         return false;
      } else {
         res->res_con.tls.allowed_cns = res_all.res_con.tls.allowed_cns;
         res->res_con.profiles = res_all.res_con.profiles;
      }
      break;
   case R_DIRECTOR:
      if (!(res = (URES *)GetResWithName(R_DIRECTOR, res_all.res_dir.name()))) {
         Emsg1(M_ERROR, 0, _("Cannot find Director resource %s\n"), res_all.res_dir.name());
         return false;
      } else {
         res->res_dir.plugin_names = res_all.res_dir.plugin_names;
         res->res_dir.messages = res_all.res_dir.messages;
         res->res_dir.backend_directories = res_all.res_dir.backend_directories;
         res->res_dir.tls.allowed_cns = res_all.res_dir.tls.allowed_cns;
      }
      break;
   case R_STORAGE:
      if (!(res = (URES *)GetResWithName(type, res_all.res_store.name()))) {
         Emsg1(M_ERROR, 0, _("Cannot find Storage resource %s\n"), res_all.res_dir.name());
         return false;
      } else {
         int status;

         res->res_store.tape_devices = res_all.res_store.tape_devices;
         res->res_store.paired_storage = res_all.res_store.paired_storage;
         res->res_store.tls.allowed_cns = res_all.res_store.tls.allowed_cns;

         /*
          * We must explicitly copy the device alist pointer
          */
         res->res_store.device = res_all.res_store.device;

         res->res_store.rss = (runtime_storage_status_t *)malloc(sizeof(runtime_storage_status_t));
         memset(res->res_store.rss, 0, sizeof(runtime_storage_status_t));
         if ((status = pthread_mutex_init(&res->res_store.rss->changer_lock, NULL)) != 0) {
            berrno be;

            Emsg1(M_ERROR_TERM, 0, _("pthread_mutex_init: ERR=%s\n"), be.bstrerror(status));
         }
      }
      break;
   case R_JOBDEFS:
   case R_JOB:
      if (!(res = (URES *)GetResWithName(type, res_all.res_job.name()))) {
         Emsg1(M_ERROR, 0, _("Cannot find Job resource %s\n"), res_all.res_job.name());
         return false;
      } else {
         res->res_job.messages = res_all.res_job.messages;
         res->res_job.schedule = res_all.res_job.schedule;
         res->res_job.client = res_all.res_job.client;
         res->res_job.fileset = res_all.res_job.fileset;
         res->res_job.storage = res_all.res_job.storage;
         res->res_job.catalog = res_all.res_job.catalog;
         res->res_job.FdPluginOptions = res_all.res_job.FdPluginOptions;
         res->res_job.SdPluginOptions = res_all.res_job.SdPluginOptions;
         res->res_job.DirPluginOptions = res_all.res_job.DirPluginOptions;
         res->res_job.base = res_all.res_job.base;
         res->res_job.pool = res_all.res_job.pool;
         res->res_job.full_pool = res_all.res_job.full_pool;
         res->res_job.vfull_pool = res_all.res_job.vfull_pool;
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

         if (type == R_JOB) {
            res->res_job.rjs = (runtime_job_status_t *)malloc(sizeof(runtime_job_status_t));
            memset(res->res_job.rjs, 0, sizeof(runtime_job_status_t));
         }
      }
      break;
   case R_COUNTER:
      if (!(res = (URES *)GetResWithName(R_COUNTER, res_all.res_counter.name()))) {
         Emsg1(M_ERROR, 0, _("Cannot find Counter resource %s\n"), res_all.res_counter.name());
         return false;
      } else {
         res->res_counter.Catalog = res_all.res_counter.Catalog;
         res->res_counter.WrapCounter = res_all.res_counter.WrapCounter;
      }
      break;
   case R_CLIENT:
      if (!(res = (URES *)GetResWithName(R_CLIENT, res_all.res_client.name()))) {
         Emsg1(M_ERROR, 0, _("Cannot find Client resource %s\n"), res_all.res_client.name());
         return false;
      } else {
         if (res_all.res_client.catalog) {
            res->res_client.catalog = res_all.res_client.catalog;
         } else {
            /*
             * No catalog overwrite given use the first catalog definition.
             */
            res->res_client.catalog = (CATRES *)GetNextRes(R_CATALOG, NULL);
         }
         res->res_client.tls.allowed_cns = res_all.res_client.tls.allowed_cns;

         res->res_client.rcs = (runtime_client_status_t *)malloc(sizeof(runtime_client_status_t));
         memset(res->res_client.rcs, 0, sizeof(runtime_client_status_t));
      }
      break;
   case R_SCHEDULE:
      /*
       * Schedule is a bit different in that it contains a RUNRES record
       * chain which isn't a "named" resource. This chain was linked
       * in by run_conf.c during pass 2, so here we jam the pointer
       * into the Schedule resource.
       */
      if (!(res = (URES *)GetResWithName(R_SCHEDULE, res_all.res_client.name()))) {
         Emsg1(M_ERROR, 0, _("Cannot find Schedule resource %s\n"), res_all.res_client.name());
         return false;
      } else {
         res->res_sch.run = res_all.res_sch.run;
      }
      break;
   default:
      Emsg1(M_ERROR, 0, _("Unknown resource type %d in save_resource.\n"), type);
      result = false;
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

   return result;
}

/*
 * Save the new resource by chaining it into the head list for
 * the resource. If this is pass 2, we update any resource
 * pointers because they may not have been defined until
 * later in pass 1.
 */
bool save_resource(int type, RES_ITEM *items, int pass)
{
   URES *res;
   int rindex = type - R_FIRST;

   switch (type) {
   case R_JOBDEFS:
      break;
   case R_JOB:
      /*
       * Check Job requirements after applying JobDefs
       * Ensure that the name item is present however.
       */
      if (items[0].flags & CFG_ITEM_REQUIRED) {
         if (!bit_is_set(0, res_all.res_dir.hdr.item_present)) {
            Emsg2(M_ERROR, 0,
                  _("%s item is required in %s resource, but not found.\n"),
                  items[0].name, resources[rindex]);
            return false;
         }
      }
      break;
   default:
      /*
       * Ensure that all required items are present
       */
      if (!validate_resource(items, &res_all.res_dir, resources[rindex].name)) {
         return false;
      }
   }

   /*
    * During pass 2 in each "store" routine, we looked up pointers
    * to all the resources referrenced in the current resource, now we
    * must copy their addresses from the static record to the allocated
    * record.
    */
   if (pass == 2) {
      return update_resource_pointer(type, items);
   }

   /*
    * Common
    */
   res = (URES *)malloc(resources[rindex].size);
   memcpy(res, &res_all, resources[rindex].size);
   if (!res_head[rindex]) {
      res_head[rindex] = (RES *)res; /* store first entry */
      Dmsg3(900, "Inserting first %s res: %s index=%d\n", res_to_str(type),
            res->res_dir.name(), rindex);
   } else {
      RES *next, *last;
      if (!res->res_dir.name()) {
         Emsg1(M_ERROR, 0, _("Name item is required in %s resource, but not found.\n"),
               resources[rindex]);
         return false;
      }
      /*
       * Add new res to end of chain
       */
      for (last = next = res_head[rindex]; next; next = next->next) {
         last = next;
         if (bstrcmp(next->name, res->res_dir.name())) {
            Emsg2(M_ERROR, 0,
                  _("Attempt to define second %s resource named \"%s\" is not permitted.\n"),
                  resources[rindex].name, res->res_dir.name());
            return false;
         }
      }
      last->next = (RES *)res;
      Dmsg4(900, _("Inserting %s res: %s index=%d pass=%d\n"), res_to_str(type),
            res->res_dir.name(), rindex, pass);
   }
   return true;
}

/*
 * Populate Job Defaults (e.g. JobDefs)
 */
static inline bool populate_jobdefs()
{
   JOBRES *job, *jobdefs;
   bool retval = true;

   /*
    * Propagate the content of a JobDefs to another.
    */
   foreach_res(jobdefs, R_JOBDEFS) {
      /*
       * Don't allow the JobDefs pointing to itself.
       */
      if (!jobdefs->jobdefs || jobdefs->jobdefs == jobdefs) {
         continue;
      }

      propagate_resource(job_items, jobdefs->jobdefs, jobdefs);
   }

   /*
    * Propagate the content of the JobDefs to the actual Job.
    */
   foreach_res(job, R_JOB) {
      if (job->jobdefs) {
         jobdefs = job->jobdefs;

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
         propagate_resource(job_items, jobdefs, job);
      }

      /*
       * Ensure that all required items are present
       */
      if (!validate_resource(job_items, job, "Job")) {
         retval = false;
         goto bail_out;
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

bool populate_defs()
{
   bool retval;

   retval = populate_jobdefs();
   if (!retval) {
      goto bail_out;
   }

bail_out:
   return retval;
}

static void store_pooltype(LEX *lc, RES_ITEM *item, int index, int pass)
{
   int i;

   lex_get_token(lc, T_NAME);
   if (pass == 1) {
      for (i = 0; PoolTypes[i].name; i++) {
         if (bstrcasecmp(lc->str, PoolTypes[i].name)) {
            /*
             * If a default was set free it first.
             */
            if (*(item->value)) {
               free(*(item->value));
            }
            *(item->value) = bstrdup(PoolTypes[i].name);
            i = 0;
            break;
         }
      }

      if (i != 0) {
         scan_err1(lc, _("Expected a Pool Type option, got: %s"), lc->str);
      }
   }

   scan_to_eol(lc);
   set_bit(index, res_all.hdr.item_present);
   clear_bit(index, res_all.hdr.inherit_content);
}

static void store_actiononpurge(LEX *lc, RES_ITEM *item, int index, int pass)
{
   int i;
   uint32_t *destination = item->ui32value;

   lex_get_token(lc, T_NAME);
   /*
    * Store the type both in pass 1 and pass 2
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
         Dmsg3(900, "Inserting first %s res: %s index=%d\n",
               res_to_str(R_DEVICE), res->res_dir.name(), rindex);
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
            Dmsg4(900, "Inserting %s res: %s index=%d pass=%d\n",
                  res_to_str(R_DEVICE), res->res_dir.name(), rindex, pass);
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
    * Store the type both in pass 1 and pass 2
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
    * Store the type both in pass 1 and pass 2
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
    * Store the type both in pass 1 and pass 2
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
    * Store the type both in pass 1 and pass 2
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
    * Store the type both in pass 1 and pass 2
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
         RES *res;

         if (!(res = GetResWithName(R_CLIENT, lc->str))) {
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

      if (!*runscripts) {
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
      if (!res_runscript.target) {
         res_runscript.set_target("%c");
      }
      if (!*runscripts) {
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
            return (char *)_("*None*");
         }
      } else {
         return (char *)_("*None*");
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
      case CFG_TYPE_POOLTYPE:
         *(item->value) = bstrdup(item->default_value);
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
   case CFG_TYPE_POOLTYPE:
      store_pooltype(lc, item, index, pass);
      break;
   default:
      break;
   }
}

/*
 * callback function for print_config
 * See ../lib/res.c, function BRSRES::print_config, for more generic handling.
 */
static void print_config_cb(RES_ITEM *items, int i, POOL_MEM &cfg_str, bool hide_sensitive_data)
{
   POOL_MEM temp;

   switch (items[i].type) {
   case CFG_TYPE_DEVICE: {
      /*
       * Each member of the list is comma-separated
       */
      int cnt = 0;
      RES *res;
      alist *list;
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
      alist *list;
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
   case CFG_TYPE_POOLTYPE:
      Mmsg(temp, "%s = %s\n", items[i].name, *(items[i].value));
      indent_config_item(cfg_str, 1, temp.c_str());
      break;
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
   config->set_default_config_filename(CONFIG_FILE);
   config->set_config_include_dir("bareos-dir.d");
}

bool parse_dir_config(CONFIG *config, const char *configfile, int exit_code)
{
   init_dir_config(config, configfile, exit_code);

   return config->parse_config();
}
