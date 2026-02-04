/**
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2026 Bareos GmbH & Co. KG

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
// Kern Sibbald, January MM
/***
 * @file
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
 */

#include "lib/resource_item.h"
#define NEED_JANSSON_NAMESPACE 1
#include "include/bareos.h"
#include "dird.h"
#include "dird/inc_conf.h"
#include "dird/date_time.h"
#include "dird/dird_globals.h"
#include "dird/director_jcr_impl.h"
#include "include/auth_protocol_types.h"
#include "include/auth_types.h"
#include "include/migration_selection_types.h"
#include "include/protocol_types.h"
#include "lib/berrno.h"
#include "lib/breg.h"
#include "lib/tls_conf.h"
#include "lib/qualified_resource_name_type_converter.h"
#include "lib/parse_conf.h"
#include "lib/keyword_table_s.h"
#include "lib/util.h"
#include "lib/edit.h"
#include "lib/tls_resource_items.h"
#include "lib/output_formatter_resource.h"
#include "lib/version.h"

#include "job_levels.h"

#include <cassert>
#include <memory>
#include <string>
#include <unordered_map>

namespace directordaemon {

// Used by print_config_schema_json
extern struct s_kw RunFields[];

/**
 * Define the first and last resource ID record
 * types. Note, these should be unique for each
 * daemon though not a requirement.
 */
static PoolMem* configure_usage_string = NULL;

extern void StoreInc(ConfigurationParser* p,
                     lexer* lc,
                     const ResourceItem* item,
                     int index,
                     int pass);
extern void StoreRun(ConfigurationParser* p,
                     lexer* lc,
                     const ResourceItem* item,
                     int index,
                     int pass);

extern void ParseInc(ConfigurationParser* p,
                     lexer* lc,
                     const ResourceItem* item,
                     int index,
                     int pass);
extern void ParseRun(ConfigurationParser* p,
                     lexer* lc,
                     const ResourceItem* item,
                     int index,
                     int pass);

static void CreateAndAddUserAgentConsoleResource(
    ConfigurationParser& my_config);
static bool SaveResource(int type, const ResourceItem* items, int pass);
static void FreeResource(BareosResource* sres, int type);
static void DumpResource(int type,
                         BareosResource* ures,
                         ConfigurationParser::sender* sendit,
                         void* sock,
                         bool hide_sensitive_data,
                         bool verbose);

/* the first configuration pass uses this static memory */
static DirectorResource* res_dir;
static ConsoleResource* res_con;
static ProfileResource* res_profile;
static ClientResource* res_client;
static StorageResource* res_store;
static CatalogResource* res_cat;
static JobResource* res_job;
static FilesetResource* res_fs;
static ScheduleResource* res_sch;
static PoolResource* res_pool;
static MessagesResource* res_msgs;
static CounterResource* res_counter;
static UserResource* res_user;

/* clang-format off */
static const ResourceItem dir_items[] = {
  { "Name", CFG_TYPE_NAME, ITEM(res_dir, resource_name_), {config::Required{}, config::Description{"The name of the resource."}}},
  { "Description", CFG_TYPE_STR, ITEM(res_dir, description_), {}},
  { "Messages", CFG_TYPE_RES, ITEM(res_dir, messages), {config::Code{R_MSGS}}},
  { "Port", CFG_TYPE_ADDRESSES_PORT, ITEM(res_dir, DIRaddrs), {config::DefaultValue{DIR_DEFAULT_PORT}, config::Alias{"DirPort"}}},
  { "Address", CFG_TYPE_ADDRESSES_ADDRESS, ITEM(res_dir, DIRaddrs), {config::DefaultValue{DIR_DEFAULT_PORT}, config::Alias{"DirAddress"}}},
  { "Addresses", CFG_TYPE_ADDRESSES, ITEM(res_dir, DIRaddrs), {config::DefaultValue{DIR_DEFAULT_PORT}, config::Alias{"DirAddresses"}}},
  { "SourceAddress", CFG_TYPE_ADDRESSES_ADDRESS, ITEM(res_dir, DIRsrc_addr), {config::DefaultValue{"0"}, config::Alias{"DirSourceAddress"}}},
  { "QueryFile", CFG_TYPE_DIR, ITEM(res_dir, query_file), {config::DefaultValue{PATH_BAREOS_SCRIPTDIR "/query.sql"}, config::Description{"File containing queries used by the bconsole 'query' command."}, config::PlatformSpecific{}}},
  { "WorkingDirectory", CFG_TYPE_DIR, ITEM(res_dir, working_directory), {config::DefaultValue{PATH_BAREOS_WORKINGDIR}, config::PlatformSpecific{}}},
  { "PluginDirectory", CFG_TYPE_DIR, ITEM(res_dir, plugin_directory), {config::IntroducedIn{14, 2, 0}, config::Description{"Plugins are loaded from this directory. To load only specific plugins, use 'Plugin Names'."}}},
  { "PluginNames", CFG_TYPE_PLUGIN_NAMES, ITEM(res_dir, plugin_names), {config::IntroducedIn{14, 2, 0}, config::Description{"List of plugins, that should get loaded from 'Plugin Directory' (only basenames, '-dir.so' is added automatically). If empty, all plugins will get loaded."}}},
  { "ScriptsDirectory", CFG_TYPE_DIR, ITEM(res_dir, scripts_directory), {config::DefaultValue{PATH_BAREOS_SCRIPTDIR}, config::Description{"Path to directory containing script files"}, config::PlatformSpecific{}}},
  { "Subscriptions", CFG_TYPE_PINT32, ITEM(res_dir, subscriptions), {config::IntroducedIn{12, 4, 4}, config::DefaultValue{"0"}}},
  { "MaximumConcurrentJobs", CFG_TYPE_PINT32, ITEM(res_dir, MaxConcurrentJobs), {config::DefaultValue{"1"}}},
  { "MaximumConsoleConnections", CFG_TYPE_PINT32, ITEM(res_dir, MaxConsoleConnections), {config::DefaultValue{"20"}}},
  { "Password", CFG_TYPE_AUTOPASSWORD, ITEM(res_dir, password_), {config::Required{}}},
  { "FdConnectTimeout", CFG_TYPE_TIME, ITEM(res_dir, FDConnectTimeout), {config::DefaultValue{"180"}}},
  { "SdConnectTimeout", CFG_TYPE_TIME, ITEM(res_dir, SDConnectTimeout), {config::DefaultValue{"1800"}}},
  { "HeartbeatInterval", CFG_TYPE_TIME, ITEM(res_dir, heartbeat_interval), {config::DefaultValue{"0"}}},
  { "StatisticsRetention", CFG_TYPE_TIME, ITEM(res_dir, stats_retention), {config::DeprecatedSince{22, 0, 0}, config::DefaultValue{"160704000"}}},
  { "StatisticsCollectInterval", CFG_TYPE_PINT32, ITEM(res_dir, stats_collect_interval), {config::DeprecatedSince{22, 0, 0}, config::IntroducedIn{14, 2, 0}, config::DefaultValue{"0"}}},
  { "VerId", CFG_TYPE_STR, ITEM(res_dir, verid), {}},
  { "KeyEncryptionKey", CFG_TYPE_AUTOPASSWORD, ITEM(res_dir, keyencrkey), {config::Code{1}}},
  { "NdmpSnooping", CFG_TYPE_BOOL, ITEM(res_dir, ndmp_snooping), {config::IntroducedIn{13, 2, 0}}},
  { "NdmpLogLevel", CFG_TYPE_PINT32, ITEM(res_dir, ndmp_loglevel), {config::IntroducedIn{13, 2, 0}, config::DefaultValue{"4"}}},
  { "NdmpNamelistFhinfoSetZeroForInvalidUquad", CFG_TYPE_BOOL, ITEM(res_dir, ndmp_fhinfo_set_zero_for_invalid_u_quad), {config::IntroducedIn{20, 0, 6}, config::DefaultValue{"false"}}},
  { "AbsoluteJobTimeout", CFG_TYPE_PINT32, ITEM(res_dir, jcr_watchdog_time), {config::IntroducedIn{14, 2, 0}, config::Description{"Absolute time after which a Job gets terminated regardless of its progress"}}},
  { "Auditing", CFG_TYPE_BOOL, ITEM(res_dir, auditing), {config::IntroducedIn{14, 2, 0}, config::DefaultValue{"false"}}},
  { "AuditEvents", CFG_TYPE_AUDIT, ITEM(res_dir, audit_events), {config::IntroducedIn{14, 2, 0}}},
  { "SecureEraseCommand", CFG_TYPE_STR, ITEM(res_dir, secure_erase_cmdline), {config::IntroducedIn{15, 2, 1}, config::Description{"Specify command that will be called when bareos unlinks files."}}},
  { "LogTimestampFormat", CFG_TYPE_STR, ITEM(res_dir, log_timestamp_format), {config::IntroducedIn{15, 2, 3}, config::DefaultValue{"%d-%b %H:%M"}}},
  { "EnableKtls", CFG_TYPE_BOOL, ITEM(res_dir, enable_ktls), {config::DefaultValue{"false"}, config::Description{"If set to \"yes\", Bareos will allow the SSL implementation to use Kernel TLS."}, config::IntroducedIn{23, 0, 0}}},
   TLS_COMMON_CONFIG(res_dir),
   TLS_CERT_CONFIG(res_dir),
  {}
};

#define USER_ACL(resource, ACL_lists) \
  { "JobAcl", CFG_TYPE_ACL, ITEM(resource, ACL_lists), {config::Code{Job_ACL}, config::Description{"Lists the Job resources, this resource has access to. The special keyword *all* allows access to all Job resources."}}}, \
  { "ClientAcl", CFG_TYPE_ACL, ITEM(resource, ACL_lists), {config::Code{Client_ACL}, config::Description{"Lists the Client resources, this resource has access to. The special keyword *all* allows access to all Client resources."}}}, \
  { "StorageAcl", CFG_TYPE_ACL, ITEM(resource, ACL_lists), {config::Code{Storage_ACL}, config::Description{"Lists the Storage resources, this resource has access to. The special keyword *all* allows access to all Storage resources."}}}, \
  { "ScheduleAcl", CFG_TYPE_ACL, ITEM(resource, ACL_lists), {config::Code{Schedule_ACL}, config::Description{"Lists the Schedule resources, this resource has access to. The special keyword *all* allows access to all Schedule resources."}}}, \
  { "PoolAcl", CFG_TYPE_ACL, ITEM(resource, ACL_lists), {config::Code{Pool_ACL}, config::Description{"Lists the Pool resources, this resource has access to. The special keyword *all* allows access to all Pool resources."}}}, \
  { "CommandAcl", CFG_TYPE_ACL, ITEM(resource, ACL_lists), {config::Code{Command_ACL}, config::Description{"Lists the commands, this resource has access to. The special keyword *all* allows using commands."}}}, \
  { "FileSetAcl", CFG_TYPE_ACL, ITEM(resource, ACL_lists), {config::Code{FileSet_ACL}, config::Description{"Lists the File Set resources, this resource has access to. The special keyword *all* allows access to all File Set resources."}}}, \
  { "CatalogAcl", CFG_TYPE_ACL, ITEM(resource, ACL_lists), {config::Code{Catalog_ACL}, config::Description{"Lists the Catalog resources, this resource has access to. The special keyword *all* allows access to all Catalog resources."}}}, \
  { "WhereAcl", CFG_TYPE_ACL, ITEM(resource, ACL_lists), {config::Code{Where_ACL}, config::Description{"Specifies the base directories, where files could be restored."}}}, \
  { "PluginOptionsAcl", CFG_TYPE_ACL, ITEM(resource, ACL_lists), {config::Code{PluginOptions_ACL}, config::Description{"Specifies the allowed plugin options. An empty strings allows all Plugin Options."}}}

static const ResourceItem profile_items[] = {
  { "Name", CFG_TYPE_NAME, ITEM(res_profile, resource_name_), {config::Required{}, config::Description{"The name of the resource."}}},
  { "Description", CFG_TYPE_STR, ITEM(res_profile, description_), {config::Description{"Additional information about the resource. Only used for UIs."}}},
  USER_ACL(res_profile, ACL_lists),
  {}
};

#define ACL_PROFILE(resource) \
  { "Profile", CFG_TYPE_ALIST_RES, ITEM(resource, user_acl.profiles), {config::IntroducedIn{14, 2, 3}, config::Code{R_PROFILE}, config::Description{"Profiles can be assigned to a Console. ACL are checked until either a deny ACL is found or an allow ACL. First the console ACL is checked then any profile the console is linked to."}}}

static const ResourceItem con_items[] = {
  { "Name", CFG_TYPE_NAME, ITEM(res_con, resource_name_), {config::Required{}}},
  { "Description", CFG_TYPE_STR, ITEM(res_con, description_), {}},
  { "Password", CFG_TYPE_AUTOPASSWORD, ITEM(res_con, password_), {config::Required{}}},
  USER_ACL(res_con, user_acl.ACL_lists),
  ACL_PROFILE(res_con),
  { "UsePamAuthentication", CFG_TYPE_BOOL, ITEM(res_con, use_pam_authentication_), {config::IntroducedIn{18, 2, 4}, config::DefaultValue{"false"}, config::Description{"If set to yes, PAM will be used to authenticate the user on this console. Otherwise, only the credentials of this console resource are used for authentication."}}},
   TLS_COMMON_CONFIG(res_con),
   TLS_CERT_CONFIG(res_con),
  {}
};

static const ResourceItem user_items[] = {
  { "Name", CFG_TYPE_NAME, ITEM(res_user, resource_name_), {config::Required{}}},
  { "Description", CFG_TYPE_STR, ITEM(res_user, description_), {}},
  USER_ACL(res_user, user_acl.ACL_lists),
  ACL_PROFILE(res_user),
  {}
};

static const ResourceItem client_items[] = {
  { "Name", CFG_TYPE_NAME, ITEM(res_client, resource_name_), {config::Required{}, config::Description{"The name of the resource."}}},
  { "Description", CFG_TYPE_STR, ITEM(res_client, description_), {}},
  { "Protocol", CFG_TYPE_AUTHPROTOCOLTYPE, ITEM(res_client, Protocol), {config::IntroducedIn{13, 2, 0}, config::DefaultValue{"Native"}}},
  { "AuthType", CFG_TYPE_AUTHTYPE, ITEM(res_client, AuthType), {config::DefaultValue{"None"}}},
  { "Address", CFG_TYPE_STR, ITEM(res_client, address), {config::Required{}, config::Alias{"FdAddress"}}},
  { "LanAddress", CFG_TYPE_STR, ITEM(res_client, lanaddress), {config::IntroducedIn{16, 2, 6}, config::Description{"Sets additional address used for connections between Client and Storage Daemon inside separate network."}}},
  { "Port", CFG_TYPE_PINT32, ITEM(res_client, FDport), {config::DefaultValue{FD_DEFAULT_PORT}, config::Alias{"FdPort"}}},
  { "Username", CFG_TYPE_STR, ITEM(res_client, username), {}},
  { "Password", CFG_TYPE_AUTOPASSWORD, ITEM(res_client, password_), {config::Alias{"FdPassword"}, config::Required{}}},
  { "Catalog", CFG_TYPE_RES, ITEM(res_client, catalog), {config::Code{R_CATALOG}}},
  { "Passive", CFG_TYPE_BOOL, ITEM(res_client, passive), {config::IntroducedIn{13, 2, 0}, config::DefaultValue{"false"}, config::Description{"If enabled, the Storage Daemon will initiate the network connection to the Client. If disabled, the Client will initiate the network connection to the Storage Daemon."}}},
  { "ConnectionFromDirectorToClient", CFG_TYPE_BOOL, ITEM(res_client, conn_from_dir_to_fd), {config::IntroducedIn{16, 2, 2}, config::DefaultValue{"true"}, config::Description{"Let the Director initiate the network connection to the Client."}}},
  { "ConnectionFromClientToDirector", CFG_TYPE_BOOL, ITEM(res_client, conn_from_fd_to_dir), {config::IntroducedIn{16, 2, 2}, config::DefaultValue{"false"}, config::Description{"The Director will accept incoming network connection from this Client."}}},
  { "Enabled", CFG_TYPE_BOOL, ITEM(res_client, enabled), {config::DefaultValue{"true"}, config::Description{"En- or disable this resource."}}},
  { "HardQuota", CFG_TYPE_SIZE64, ITEM(res_client, HardQuota), {config::DefaultValue{"0"}}},
  { "SoftQuota", CFG_TYPE_SIZE64, ITEM(res_client, SoftQuota), {config::DefaultValue{"0"}}},
  { "SoftQuotaGracePeriod", CFG_TYPE_TIME, ITEM(res_client, SoftQuotaGracePeriod), {config::DefaultValue{"0"}}},
  { "StrictQuotas", CFG_TYPE_BOOL, ITEM(res_client, StrictQuotas), {config::DefaultValue{"false"}}},
  { "QuotaIncludeFailedJobs", CFG_TYPE_BOOL, ITEM(res_client, QuotaIncludeFailedJobs), {config::DefaultValue{"true"}}},
  { "FileRetention", CFG_TYPE_TIME, ITEM(res_client, FileRetention),
    { config::DeprecatedSince{config::Version{23, 0, 0}}, config::DefaultValue{"5184000"}, config::Description{"File retention"}}},
  { "JobRetention", CFG_TYPE_TIME, ITEM(res_client, JobRetention), {config::DeprecatedSince{23, 0, 0}, config::DefaultValue{"15552000"}}},
  { "HeartbeatInterval", CFG_TYPE_TIME, ITEM(res_client, heartbeat_interval), {config::DefaultValue{"0"}}},
  { "AutoPrune", CFG_TYPE_BOOL, ITEM(res_client, AutoPrune), {config::DeprecatedSince{23, 0, 0}, config::DefaultValue{"false"}}},
  { "MaximumConcurrentJobs", CFG_TYPE_PINT32, ITEM(res_client, MaxConcurrentJobs), {config::DefaultValue{"1"}}},
  { "MaximumBandwidthPerJob", CFG_TYPE_SPEED, ITEM(res_client, max_bandwidth), {}},
  { "NdmpLogLevel", CFG_TYPE_PINT32, ITEM(res_client, ndmp_loglevel), {config::DefaultValue{"4"}}},
  { "NdmpBlockSize", CFG_TYPE_SIZE32, ITEM(res_client, ndmp_blocksize), {config::DefaultValue{"64512"}}},
  { "NdmpUseLmdb", CFG_TYPE_BOOL, ITEM(res_client, ndmp_use_lmdb), {config::DefaultValue{"true"}}},
   TLS_COMMON_CONFIG(res_client),
   TLS_CERT_CONFIG(res_client),
  {}
};

static const ResourceItem store_items[] = {
  { "Name", CFG_TYPE_NAME, ITEM(res_store, resource_name_), {config::Required{}, config::Description{"The name of the resource."}}},
  { "Description", CFG_TYPE_STR, ITEM(res_store, description_), {}},
  { "Protocol", CFG_TYPE_AUTHPROTOCOLTYPE, ITEM(res_store, Protocol), {config::DefaultValue{"Native"}}},
  { "AuthType", CFG_TYPE_AUTHTYPE, ITEM(res_store, AuthType), {config::DefaultValue{"None"}}},
  { "Address", CFG_TYPE_STR, ITEM(res_store, address), {config::Alias{"SdAddress"}, config::Required{}}},
  { "LanAddress", CFG_TYPE_STR, ITEM(res_store, lanaddress), {config::IntroducedIn{16, 2, 6}, config::Description{"Sets additional address used for connections between Client and Storage Daemon inside separate network."}}},
  { "Port", CFG_TYPE_PINT32, ITEM(res_store, SDport), {config::DefaultValue{SD_DEFAULT_PORT}, config::Alias{"SdPort"}}},
  { "Username", CFG_TYPE_STR, ITEM(res_store, username), {}},
  { "Password", CFG_TYPE_AUTOPASSWORD, ITEM(res_store, password_), {config::Alias{"SdPassword"}, config::Required{}}},
  { "Device", CFG_TYPE_DEVICE, ITEM(res_store, devices), {config::Required{}}},
  { "MediaType", CFG_TYPE_STRNAME, ITEM(res_store, media_type), {config::Required{}}},
  { "AutoChanger", CFG_TYPE_BOOL, ITEM(res_store, autochanger), {config::DefaultValue{"false"}}},
  { "Enabled", CFG_TYPE_BOOL, ITEM(res_store, enabled), {config::DefaultValue{"true"}, config::Description{"En- or disable this resource."}}},
  { "AllowCompression", CFG_TYPE_BOOL, ITEM(res_store, AllowCompress), {config::DefaultValue{"true"}}},
  { "HeartbeatInterval", CFG_TYPE_TIME, ITEM(res_store, heartbeat_interval), {config::DefaultValue{"0"}}},
  { "CacheStatusInterval", CFG_TYPE_TIME, ITEM(res_store, cache_status_interval), {config::DefaultValue{"30"}}},
  { "MaximumConcurrentJobs", CFG_TYPE_PINT32, ITEM(res_store, MaxConcurrentJobs), {config::DefaultValue{"1"}}},
  { "MaximumConcurrentReadJobs", CFG_TYPE_PINT32, ITEM(res_store, MaxConcurrentReadJobs), {config::DefaultValue{"0"}}},
  { "PairedStorage", CFG_TYPE_RES, ITEM(res_store, paired_storage), {config::Code{R_STORAGE}}},
  { "MaximumBandwidthPerJob", CFG_TYPE_SPEED, ITEM(res_store, max_bandwidth), {}},
  { "CollectStatistics", CFG_TYPE_BOOL, ITEM(res_store, collectstats), {config::DeprecatedSince{22, 0, 0}, config::DefaultValue{"false"}}},
  { "NdmpChangerDevice", CFG_TYPE_STRNAME, ITEM(res_store, ndmp_changer_device), {config::IntroducedIn{16, 2, 4}, config::Description{"Allows direct control of a Storage Daemon Auto Changer device by the Director. Only used in NDMP_NATIVE environments."}}},
   TLS_COMMON_CONFIG(res_store),
   TLS_CERT_CONFIG(res_store),
  {}
};

static const ResourceItem cat_items[] = {
  { "Name", CFG_TYPE_NAME, ITEM(res_cat, resource_name_), {config::Required{}, config::Description{"The name of the resource."}}},
  { "Description", CFG_TYPE_STR, ITEM(res_cat, description_), {}},
  { "DbAddress", CFG_TYPE_STR, ITEM(res_cat, db_address), {config::Alias{"Address"}}},
  { "DbPort", CFG_TYPE_PINT32, ITEM(res_cat, db_port), {config::Alias{"Port"}}},
  { "DbPassword", CFG_TYPE_AUTOPASSWORD, ITEM(res_cat, db_password), {config::Alias{"Password"}}},
  { "DbUser", CFG_TYPE_STR, ITEM(res_cat, db_user), {config::Required{}, config::Alias{"User"}}},
  { "DbName", CFG_TYPE_STR, ITEM(res_cat, db_name), {config::Required{}}},
  { "DbSocket", CFG_TYPE_STR, ITEM(res_cat, db_socket), {config::Alias{"Socket"}}},
  /* Turned off for the moment */
  { "MultipleConnections", CFG_TYPE_BIT, ITEM(res_cat, mult_db_connections), {config::DeprecatedSince{25,0,0}}},
  { "DisableBatchInsert", CFG_TYPE_BOOL, ITEM(res_cat, disable_batch_insert), {config::DeprecatedSince{25,0,0}, config::DefaultValue{"false"}}},
  { "Reconnect", CFG_TYPE_BOOL, ITEM(res_cat, try_reconnect), {config::IntroducedIn{15, 1, 0}, config::DefaultValue{"true"}, config::Description{"Try to reconnect a database connection when it is dropped"}}},
  { "ExitOnFatal", CFG_TYPE_BOOL, ITEM(res_cat, exit_on_fatal), {config::IntroducedIn{15, 1, 0}, config::DefaultValue{"false"}, config::Description{"Make any fatal error in the connection to the database exit the program"}}},
  { "MinConnections", CFG_TYPE_PINT32, ITEM(res_cat, pooling_min_connections), {config::DefaultValue{"1"}, config::Description{"This directive is used by the experimental database pooling functionality. Only use this for non production sites. This sets the minimum number of connections to a database to keep in this database pool."}}},
  { "MaxConnections", CFG_TYPE_PINT32, ITEM(res_cat, pooling_max_connections), {config::DefaultValue{"5"}, config::Description{"This directive is used by the experimental database pooling functionality. Only use this for non production sites. This sets the maximum number of connections to a database to keep in this database pool."}}},
  { "IncConnections", CFG_TYPE_PINT32, ITEM(res_cat, pooling_increment_connections), {config::DefaultValue{"1"}, config::Description{"This directive is used by the experimental database pooling functionality. Only use this for non production sites. This sets the number of connections to add to a database pool when not enough connections are available on the pool anymore."}}},
  { "IdleTimeout", CFG_TYPE_PINT32, ITEM(res_cat, pooling_idle_timeout), {config::DefaultValue{"30"}, config::Description{"This directive is used by the experimental database pooling functionality. Only use this for non production sites.  This sets the idle time after which a database pool should be shrinked."}}},
  { "ValidateTimeout", CFG_TYPE_PINT32, ITEM(res_cat, pooling_validate_timeout), {config::DefaultValue{"120"}, config::Description{"This directive is used by the experimental database pooling functionality. Only use this for non production sites. This sets the validation timeout after which the database connection is polled to see if its still alive."}}},
  {}
};

const ResourceItem job_items[] = {
  { "Name", CFG_TYPE_NAME, ITEM(res_job, resource_name_), {config::Required{}, config::Description{"The name of the resource."}}},
  { "Description", CFG_TYPE_STR, ITEM(res_job, description_), {}},
  { "Type", CFG_TYPE_JOBTYPE, ITEM(res_job, JobType), {config::Required{}}},
  { "Protocol", CFG_TYPE_PROTOCOLTYPE, ITEM(res_job, Protocol), {config::DefaultValue{"Native"}}},
  { "BackupFormat", CFG_TYPE_STR, ITEM(res_job, backup_format), {config::DefaultValue{"Native"}}},
  { "Level", CFG_TYPE_LEVEL, ITEM(res_job, JobLevel), {}},
  { "Messages", CFG_TYPE_RES, ITEM(res_job, messages), {config::Required{}, config::Code{R_MSGS}}},
  { "Storage", CFG_TYPE_ALIST_RES, ITEM(res_job, storage), {config::Code{R_STORAGE}}},
  { "Pool", CFG_TYPE_RES, ITEM(res_job, pool), {config::Required{}, config::Code{R_POOL}}},
  { "FullBackupPool", CFG_TYPE_RES, ITEM(res_job, full_pool), {config::Code{R_POOL}}},
  { "VirtualFullBackupPool", CFG_TYPE_RES, ITEM(res_job, vfull_pool), {config::Code{R_POOL}}},
  { "IncrementalBackupPool", CFG_TYPE_RES, ITEM(res_job, inc_pool), {config::Code{R_POOL}}},
  { "DifferentialBackupPool", CFG_TYPE_RES, ITEM(res_job, diff_pool), {config::Code{R_POOL}}},
  { "NextPool", CFG_TYPE_RES, ITEM(res_job, next_pool), {config::Code{R_POOL}}},
  { "Client", CFG_TYPE_RES, ITEM(res_job, client), {config::Code{R_CLIENT}}},
  { "FileSet", CFG_TYPE_RES, ITEM(res_job, fileset), {config::Code{R_FILESET}}},
  { "Schedule", CFG_TYPE_RES, ITEM(res_job, schedule), {config::Code{R_SCHEDULE}}},
  { "JobToVerify", CFG_TYPE_RES, ITEM(res_job, verify_job), {config::Code{R_JOB}, config::Alias{"VerifyJob"}}},
  { "Catalog", CFG_TYPE_RES, ITEM(res_job, catalog), {config::IntroducedIn{13, 4, 0}, config::Code{R_CATALOG}}},
  { "JobDefs", CFG_TYPE_RES, ITEM(res_job, jobdefs), {config::Code{R_JOBDEFS}}},
  { "Run", CFG_TYPE_ALIST_STR, ITEM(res_job, run_cmds), {}},
  { "Where", CFG_TYPE_DIR, ITEM(res_job, RestoreWhere), {}},
  { "RegexWhere", CFG_TYPE_STR, ITEM(res_job, RegexWhere), {}},
  { "StripPrefix", CFG_TYPE_STR, ITEM(res_job, strip_prefix), {}},
  { "AddPrefix", CFG_TYPE_STR, ITEM(res_job, add_prefix), {}},
  { "AddSuffix", CFG_TYPE_STR, ITEM(res_job, add_suffix), {}},
  { "Bootstrap", CFG_TYPE_DIR, ITEM(res_job, RestoreBootstrap), {}},
  { "WriteBootstrap", CFG_TYPE_DIR_OR_CMD, ITEM(res_job, WriteBootstrap), {}},
  { "WriteVerifyList", CFG_TYPE_DIR, ITEM(res_job, WriteVerifyList), {}},
  { "Replace", CFG_TYPE_REPLACE, ITEM(res_job, replace), {config::DefaultValue{"Always"}}},
  { "MaximumBandwidth", CFG_TYPE_SPEED, ITEM(res_job, max_bandwidth), {}},
  { "MaxRunSchedTime", CFG_TYPE_TIME, ITEM(res_job, MaxRunSchedTime), {}},
  { "MaxRunTime", CFG_TYPE_TIME, ITEM(res_job, MaxRunTime), {}},
  { "FullMaxRuntime", CFG_TYPE_TIME, ITEM(res_job, FullMaxRunTime), {}},
  { "IncrementalMaxRuntime", CFG_TYPE_TIME, ITEM(res_job, IncMaxRunTime), {}},
  { "DifferentialMaxRuntime", CFG_TYPE_TIME, ITEM(res_job, DiffMaxRunTime), {}},
  { "MaxWaitTime", CFG_TYPE_TIME, ITEM(res_job, MaxWaitTime), {}},
  { "MaxStartDelay", CFG_TYPE_TIME, ITEM(res_job, MaxStartDelay), {}},
  { "MaxFullInterval", CFG_TYPE_TIME, ITEM(res_job, MaxFullInterval), {}},
  { "MaxVirtualFullInterval", CFG_TYPE_TIME, ITEM(res_job, MaxVFullInterval), {config::IntroducedIn{14, 4, 0}}},
  { "MaxDiffInterval", CFG_TYPE_TIME, ITEM(res_job, MaxDiffInterval), {}},
  { "PrefixLinks", CFG_TYPE_BOOL, ITEM(res_job, PrefixLinks), {config::DefaultValue{"false"}}},
  { "PruneJobs", CFG_TYPE_BOOL, ITEM(res_job, PruneJobs), {config::DefaultValue{"false"}}},
  { "PruneFiles", CFG_TYPE_BOOL, ITEM(res_job, PruneFiles), {config::DefaultValue{"false"}}},
  { "PruneVolumes", CFG_TYPE_BOOL, ITEM(res_job, PruneVolumes), {config::DefaultValue{"false"}}},
  { "PurgeMigrationJob", CFG_TYPE_BOOL, ITEM(res_job, PurgeMigrateJob), {config::DefaultValue{"false"}}},
  { "Enabled", CFG_TYPE_BOOL, ITEM(res_job, enabled), {config::DefaultValue{"true"}, config::Description{"En- or disable this resource."}}},
  { "SpoolAttributes", CFG_TYPE_BOOL, ITEM(res_job, SpoolAttributes), {config::DefaultValue{"false"}}},
  { "SpoolData", CFG_TYPE_BOOL, ITEM(res_job, spool_data), {config::DefaultValue{"false"}}},
  { "SpoolSize", CFG_TYPE_SIZE64, ITEM(res_job, spool_size), {}},
  { "RerunFailedLevels", CFG_TYPE_BOOL, ITEM(res_job, rerun_failed_levels), {config::DefaultValue{"false"}}},
  { "PreferMountedVolumes", CFG_TYPE_BOOL, ITEM(res_job, PreferMountedVolumes), {config::DefaultValue{"true"}}},
  { "RunBeforeJob", CFG_TYPE_SHRTRUNSCRIPT, ITEM(res_job, RunScripts), {}},
  { "RunAfterJob", CFG_TYPE_SHRTRUNSCRIPT, ITEM(res_job, RunScripts), {}},
  { "RunAfterFailedJob", CFG_TYPE_SHRTRUNSCRIPT, ITEM(res_job, RunScripts), {}},
  { "ClientRunBeforeJob", CFG_TYPE_SHRTRUNSCRIPT, ITEM(res_job, RunScripts), {}},
  { "ClientRunAfterJob", CFG_TYPE_SHRTRUNSCRIPT, ITEM(res_job, RunScripts), {}},
  { "MaximumConcurrentJobs", CFG_TYPE_PINT32, ITEM(res_job, MaxConcurrentJobs), {config::DefaultValue{"1"}}},
  { "RescheduleOnError", CFG_TYPE_BOOL, ITEM(res_job, RescheduleOnError), {config::DefaultValue{"false"}}},
  { "RescheduleInterval", CFG_TYPE_TIME, ITEM(res_job, RescheduleInterval), {config::DefaultValue{"1800"}}},
  { "RescheduleTimes", CFG_TYPE_PINT32, ITEM(res_job, RescheduleTimes), {config::DefaultValue{"5"}}},
  { "Priority", CFG_TYPE_PINT32, ITEM(res_job, Priority), {config::DefaultValue{"10"}}},
  { "AllowMixedPriority", CFG_TYPE_BOOL, ITEM(res_job, allow_mixed_priority), {config::DefaultValue{"false"}}},
  { "SelectionPattern", CFG_TYPE_STR, ITEM(res_job, selection_pattern), {}},
  { "RunScript", CFG_TYPE_RUNSCRIPT, ITEM(res_job, RunScripts), {config::UsesNoEquals{}}},
  { "SelectionType", CFG_TYPE_MIGTYPE, ITEM(res_job, selection_type), {}},
  { "Accurate", CFG_TYPE_BOOL, ITEM(res_job, accurate), {config::DefaultValue{"false"}}},
  { "AllowDuplicateJobs", CFG_TYPE_BOOL, ITEM(res_job, AllowDuplicateJobs), {config::DefaultValue{"true"}}},
  { "AllowHigherDuplicates", CFG_TYPE_BOOL, ITEM(res_job, AllowHigherDuplicates), {config::DeprecatedSince{24, 0, 0}, config::DefaultValue{"true"}}},
  { "CancelLowerLevelDuplicates", CFG_TYPE_BOOL, ITEM(res_job, CancelLowerLevelDuplicates), {config::DefaultValue{"false"}}},
  { "CancelQueuedDuplicates", CFG_TYPE_BOOL, ITEM(res_job, CancelQueuedDuplicates), {config::DefaultValue{"false"}}},
  { "CancelRunningDuplicates", CFG_TYPE_BOOL, ITEM(res_job, CancelRunningDuplicates), {config::DefaultValue{"false"}}},
  { "SaveFileHistory", CFG_TYPE_BOOL, ITEM(res_job, SaveFileHist), {config::IntroducedIn{14, 2, 0}, config::DefaultValue{"true"}}},
  { "FileHistorySize", CFG_TYPE_SIZE64, ITEM(res_job, FileHistSize), {config::IntroducedIn{15, 2, 4}, config::DefaultValue{"10000000"}}},
  { "FdPluginOptions", CFG_TYPE_ALIST_STR, ITEM(res_job, FdPluginOptions), {}},
  { "SdPluginOptions", CFG_TYPE_ALIST_STR, ITEM(res_job, SdPluginOptions), {}},
  { "DirPluginOptions", CFG_TYPE_ALIST_STR, ITEM(res_job, DirPluginOptions), {}},
  { "MaxConcurrentCopies", CFG_TYPE_PINT32, ITEM(res_job, MaxConcurrentCopies), {config::DefaultValue{"100"}}},
  { "AlwaysIncremental", CFG_TYPE_BOOL, ITEM(res_job, AlwaysIncremental), {config::IntroducedIn{16, 2, 4}, config::DefaultValue{"false"}, config::Description{"Enable/disable always incremental backup scheme."}}},
  { "AlwaysIncrementalJobRetention", CFG_TYPE_TIME, ITEM(res_job, AlwaysIncrementalJobRetention), {config::IntroducedIn{16, 2, 4}, config::DefaultValue{"0"}, config::Description{"Backup Jobs older than the specified time duration will be merged into a new Virtual backup."}}},
  { "AlwaysIncrementalKeepNumber", CFG_TYPE_PINT32, ITEM(res_job, AlwaysIncrementalKeepNumber), {config::IntroducedIn{16, 2, 4}, config::DefaultValue{"0"}, config::Description{"Guarantee that at least the specified number of Backup Jobs will persist, even if they are older than \"Always Incremental Job Retention\"."}}},
  { "AlwaysIncrementalMaxFullAge", CFG_TYPE_TIME, ITEM(res_job, AlwaysIncrementalMaxFullAge), {config::IntroducedIn{16, 2, 4}, config::Description{"If \"AlwaysIncrementalMaxFullAge\" is set, during consolidations only incremental backups will be considered while the Full Backup remains to reduce the amount of data being consolidated. Only if the Full Backup is older than \"AlwaysIncrementalMaxFullAge\", the Full Backup will be part of the consolidation to avoid the Full Backup becoming too old ."}}},
  { "MaxFullConsolidations", CFG_TYPE_PINT32, ITEM(res_job, MaxFullConsolidations), {config::IntroducedIn{16, 2, 4}, config::DefaultValue{"0"}, config::Description{"If \"AlwaysIncrementalMaxFullAge\" is configured, do not run more than \"MaxFullConsolidations\" consolidation jobs that include the Full backup."}}},
  { "RunOnIncomingConnectInterval", CFG_TYPE_TIME, ITEM(res_job, RunOnIncomingConnectInterval), {config::IntroducedIn{19, 2, 4}, config::DefaultValue{"0"}, config::Description{"The interval specifies the time between the most recent successful backup (counting from start time) and the event of a client initiated connection. When this interval is exceeded the job is started automatically."}}},
  {}
};

static const ResourceItem fs_items[] = {
  { "Name", CFG_TYPE_NAME, ITEM(res_fs, resource_name_), {config::Required{}, config::Description{"The name of the resource."}}},
  { "Description", CFG_TYPE_STR, ITEM(res_fs, description_), {}},
  { "Include", CFG_TYPE_INCEXC, ITEMC(res_fs), {config::UsesNoEquals{}, config::Code{0}}},
  { "Exclude", CFG_TYPE_INCEXC, ITEMC(res_fs), {config::UsesNoEquals{}, config::Code{1}}},
  { "IgnoreFileSetChanges", CFG_TYPE_BOOL, ITEM(res_fs, ignore_fs_changes), {config::DefaultValue{"false"}}},
  { "EnableVSS", CFG_TYPE_BOOL, ITEM(res_fs, enable_vss), {config::DefaultValue{"true"}}},
  {}
};

static const ResourceItem sch_items[] = {
  { "Name", CFG_TYPE_NAME, ITEM(res_sch, resource_name_), {config::Required{}, config::Description{"The name of the resource."}}},
  { "Description", CFG_TYPE_STR, ITEM(res_sch, description_), {}},
  { "Run", CFG_TYPE_RUN, ITEM(res_sch, run), {}},
  { "Enabled", CFG_TYPE_BOOL, ITEM(res_sch, enabled), {config::DefaultValue{"true"}, config::Description{"En- or disable this resource."}}},
  {}
};

static const ResourceItem pool_items[] = {
  { "Name", CFG_TYPE_NAME, ITEM(res_pool, resource_name_), {config::Required{}, config::Description{"The name of the resource."}}},
  { "Description", CFG_TYPE_STR, ITEM(res_pool, description_), {}},
  { "PoolType", CFG_TYPE_POOLTYPE, ITEM(res_pool, pool_type), {config::DefaultValue{"Backup"}}},
  { "LabelFormat", CFG_TYPE_STRNAME, ITEM(res_pool, label_format), {}},
  { "LabelType", CFG_TYPE_LABEL, ITEM(res_pool, LabelType), {config::DeprecatedSince{23, 0, 0}}},
  { "CleaningPrefix", CFG_TYPE_STRNAME, ITEM(res_pool, cleaning_prefix), {config::DefaultValue{"CLN"}}},
  { "UseCatalog", CFG_TYPE_BOOL, ITEM(res_pool, use_catalog), {config::DefaultValue{"true"}}},
  { "PurgeOldestVolume", CFG_TYPE_BOOL, ITEM(res_pool, purge_oldest_volume), {config::DefaultValue{"false"}}},
  { "ActionOnPurge", CFG_TYPE_ACTIONONPURGE, ITEM(res_pool, action_on_purge), {}},
  { "RecycleOldestVolume", CFG_TYPE_BOOL, ITEM(res_pool, recycle_oldest_volume), {config::DefaultValue{"false"}}},
  { "RecycleCurrentVolume", CFG_TYPE_BOOL, ITEM(res_pool, recycle_current_volume), {config::DefaultValue{"false"}}},
  { "MaximumVolumes", CFG_TYPE_PINT32, ITEM(res_pool, max_volumes), {}},
  { "MaximumVolumeJobs", CFG_TYPE_PINT32, ITEM(res_pool, MaxVolJobs), {}},
  { "MaximumVolumeFiles", CFG_TYPE_PINT32, ITEM(res_pool, MaxVolFiles), {}},
  { "MaximumVolumeBytes", CFG_TYPE_SIZE64, ITEM(res_pool, MaxVolBytes), {}},
  { "CatalogFiles", CFG_TYPE_BOOL, ITEM(res_pool, catalog_files), {config::DefaultValue{"true"}}},
  { "VolumeRetention", CFG_TYPE_TIME, ITEM(res_pool, VolRetention), {config::DefaultValue{"31536000"}}},
  { "VolumeUseDuration", CFG_TYPE_TIME, ITEM(res_pool, VolUseDuration), {}},
  { "MigrationTime", CFG_TYPE_TIME, ITEM(res_pool, MigrationTime), {}},
  { "MigrationHighBytes", CFG_TYPE_SIZE64, ITEM(res_pool, MigrationHighBytes), {}},
  { "MigrationLowBytes", CFG_TYPE_SIZE64, ITEM(res_pool, MigrationLowBytes), {}},
  { "NextPool", CFG_TYPE_RES, ITEM(res_pool, NextPool), {config::Code{R_POOL}}},
  { "Storage", CFG_TYPE_ALIST_RES, ITEM(res_pool, storage), {config::Code{R_STORAGE}}},
  { "AutoPrune", CFG_TYPE_BOOL, ITEM(res_pool, AutoPrune), {config::DefaultValue{"true"}}},
  { "Recycle", CFG_TYPE_BOOL, ITEM(res_pool, Recycle), {config::DefaultValue{"true"}}},
  { "RecyclePool", CFG_TYPE_RES, ITEM(res_pool, RecyclePool), {config::Code{R_POOL}}},
  { "ScratchPool", CFG_TYPE_RES, ITEM(res_pool, ScratchPool), {config::Code{R_POOL}}},
  { "Catalog", CFG_TYPE_RES, ITEM(res_pool, catalog), {config::Code{R_CATALOG}}},
  { "FileRetention", CFG_TYPE_TIME, ITEM(res_pool, FileRetention), {}},
  { "JobRetention", CFG_TYPE_TIME, ITEM(res_pool, JobRetention), {}},
  { "MinimumBlockSize", CFG_TYPE_SIZE32, ITEM(res_pool, MinBlocksize), {}},
  { "MaximumBlockSize", CFG_TYPE_SIZE32, ITEM(res_pool, MaxBlocksize), {config::IntroducedIn{14, 2, 0}}},
  {}
};

static const ResourceItem counter_items[] = {
  { "Name", CFG_TYPE_NAME, ITEM(res_counter, resource_name_), {config::Required{}, config::Description{"The name of the resource."}}},
  { "Description", CFG_TYPE_STR, ITEM(res_counter, description_), {}},
  { "Minimum", CFG_TYPE_INT32, ITEM(res_counter, MinValue), {config::DefaultValue{"0"}}},
  { "Maximum", CFG_TYPE_PINT32, ITEM(res_counter, MaxValue), {config::DefaultValue{"2147483647"}}},
  { "WrapCounter", CFG_TYPE_RES, ITEM(res_counter, WrapCounter), {config::Code{R_COUNTER}}},
  { "Catalog", CFG_TYPE_RES, ITEM(res_counter, Catalog), {config::Code{R_CATALOG}}},
  {}
};

#include "lib/messages_resource_items.h"

/**
 * This is the master resource definition.
 * It must have one item for each of the resources.
 *
 *  NOTE!!! keep it in the same order as the R_codes
 *    or eliminate all resources[rindex].name
 *
 * name handler value code flags default_value
 */
static ResourceTable dird_resource_tables[] = {
  { "Director", "Directors", dir_items, R_DIRECTOR, sizeof(DirectorResource),
      [] (){ res_dir = new DirectorResource(); }, reinterpret_cast<BareosResource**>(&res_dir) },
  { "Client", "Clients", client_items, R_CLIENT, sizeof(ClientResource),
      [] (){ res_client = new ClientResource(); }, reinterpret_cast<BareosResource**>(&res_client)  },
  { "JobDefs", "JobDefs", job_items, R_JOBDEFS, sizeof(JobResource),
      [] (){ res_job = new JobResource(); }, reinterpret_cast<BareosResource**>(&res_job) },
  { "Job", "Jobs", job_items, R_JOB, sizeof(JobResource),
      [] (){ res_job = new JobResource(); }, reinterpret_cast<BareosResource**>(&res_job) },
  { "Storage", "Storages", store_items, R_STORAGE, sizeof(StorageResource),
      [] (){ res_store = new StorageResource(); }, reinterpret_cast<BareosResource**>(&res_store) },
  { "Catalog", "Catalogs", cat_items, R_CATALOG, sizeof(CatalogResource),
      [] (){ res_cat = new CatalogResource(); }, reinterpret_cast<BareosResource**>(&res_cat) },
  { "Schedule", "Schedules", sch_items, R_SCHEDULE, sizeof(ScheduleResource),
      [] (){ res_sch = new ScheduleResource(); }, reinterpret_cast<BareosResource**>(&res_sch) },
  { "FileSet", "FileSets", fs_items, R_FILESET, sizeof(FilesetResource),
      [] (){ res_fs = new FilesetResource(); }, reinterpret_cast<BareosResource**>(&res_fs) },
  { "Pool", "Pools", pool_items, R_POOL, sizeof(PoolResource),
      [] (){ res_pool = new PoolResource(); }, reinterpret_cast<BareosResource**>(&res_pool) },
  { "Messages", "Messages", msgs_items, R_MSGS, sizeof(MessagesResource),
      [] (){ res_msgs = new MessagesResource(); }, reinterpret_cast<BareosResource**>(&res_msgs) },
  { "Counter", "Counters", counter_items, R_COUNTER, sizeof(CounterResource),
      [] (){ res_counter = new CounterResource(); }, reinterpret_cast<BareosResource**>(&res_counter) },
  { "Profile", "Profiles", profile_items, R_PROFILE, sizeof(ProfileResource),
      [] (){ res_profile = new ProfileResource(); }, reinterpret_cast<BareosResource**>(&res_profile) },
  { "Console", "Consoles", con_items, R_CONSOLE, sizeof(ConsoleResource),
      [] (){ res_con = new ConsoleResource(); }, reinterpret_cast<BareosResource**>(&res_con) },
  { "User", "Users", user_items, R_USER, sizeof(UserResource),
      [] (){ res_user = new UserResource(); }, reinterpret_cast<BareosResource**>(&res_user) },
  { nullptr, nullptr, nullptr, 0, 0, nullptr, nullptr }
};

/**
 * Note, when this resource is used, we are inside a Job
 * resource. We treat the RunScript like a sort of
 * mini-resource within the Job resource.
 */
static RunScript *res_runscript;

/**
 * new RunScript items
 * name handler value code flags default_value
 */
static const ResourceItem runscript_items[] = {
  { "Command", CFG_TYPE_RUNSCRIPT_CMD, ITEMC(res_runscript), {config::Code{SHELL_CMD}}},
  { "Console", CFG_TYPE_RUNSCRIPT_CMD, ITEMC(res_runscript), {config::Code{CONSOLE_CMD}}},
  { "Target", CFG_TYPE_RUNSCRIPT_TARGET, ITEMC(res_runscript), {}},
  { "RunsOnSuccess", CFG_TYPE_RUNSCRIPT_BOOL, ITEM(res_runscript, on_success), {}},
  { "RunsOnFailure", CFG_TYPE_RUNSCRIPT_BOOL, ITEM(res_runscript, on_failure), {}},
  { "FailJobOnError", CFG_TYPE_RUNSCRIPT_BOOL, ITEM(res_runscript, fail_on_error), {}},
  { "AbortJobOnError", CFG_TYPE_RUNSCRIPT_BOOL, ITEM(res_runscript, fail_on_error), {}},
  { "RunsWhen", CFG_TYPE_RUNSCRIPT_WHEN, ITEM(res_runscript, when), {}},
  { "RunsOnClient", CFG_TYPE_RUNSCRIPT_TARGET, ITEMC(res_runscript), {}},
  {}
};

/* clang-format on */
/**
 * The following arrays are referenced from else where and
 * used for display to the user so the keyword are pretty
 * printed with additional capitals. As the code uses
 * strcasecmp anyhow this doesn't matter.
 */

/** Keywords (RHS) permitted in Job type records
 *
 * type_name job_type
 */
struct s_jt jobtypes[] = {{"Backup", JT_BACKUP},
                          {"Admin", JT_ADMIN},
                          {"Archive", JT_ARCHIVE},
                          {"Verify", JT_VERIFY},
                          {"Restore", JT_RESTORE},
                          {"Migrate", JT_MIGRATE},
                          {"Copy", JT_COPY},
                          {"Consolidate", JT_CONSOLIDATE},
                          {NULL, 0}};

/** Keywords (RHS) permitted in Protocol type records
 *
 * name token
 */
static struct s_kw backupprotocols[]
    = {{"Native", PT_NATIVE},
       {"NDMP_BAREOS", PT_NDMP_BAREOS}, /* alias for PT_NDMP */
       {"NDMP", PT_NDMP_BAREOS},
       {"NDMP_NATIVE", PT_NDMP_NATIVE},
       {NULL, 0}};

/** Keywords (RHS) permitted in AuthProtocol type records
 *
 * name token
 */
static struct s_kw authprotocols[] = {{"Native", APT_NATIVE},
                                      {"NDMPV2", APT_NDMPV2},
                                      {"NDMPV3", APT_NDMPV3},
                                      {"NDMPV4", APT_NDMPV4},
                                      {NULL, 0}};

/**
 * Keywords (RHS) permitted in Authentication type records
 *
 * name token
 */
static struct s_kw authmethods[]
    = {{"None", AT_NONE}, {"Clear", AT_CLEAR}, {"MD5", AT_MD5}, {NULL, 0}};

/**
 * Keywords (RHS) permitted in Selection type records
 *
 * type_name job_type
 */
static struct s_jt migtypes[] = {{"SmallestVolume", MT_SMALLEST_VOL},
                                 {"OldestVolume", MT_OLDEST_VOL},
                                 {"PoolOccupancy", MT_POOL_OCCUPANCY},
                                 {"PoolTime", MT_POOL_TIME},
                                 {"PoolUncopiedJobs", MT_POOL_UNCOPIED_JOBS},
                                 {"Client", MT_CLIENT},
                                 {"Volume", MT_VOLUME},
                                 {"Job", MT_JOB},
                                 {"SqlQuery", MT_SQLQUERY},
                                 {NULL, 0}};

/**
 * Keywords (RHS) permitted in Restore replace type records
 *
 * name token
 */
struct s_kw ReplaceOptions[] = {{"Always", REPLACE_ALWAYS},
                                {"IfNewer", REPLACE_IFNEWER},
                                {"IfOlder", REPLACE_IFOLDER},
                                {"Never", REPLACE_NEVER},
                                {NULL, 0}};

/**
 * Keywords (RHS) permitted in ActionOnPurge type records
 *
 * name token
 */
struct s_kw ActionOnPurgeOptions[]
    = {{"None", ON_PURGE_NONE}, {"Truncate", ON_PURGE_TRUNCATE}, {NULL, 0}};

/**
 * Keywords (RHS) permitted in Volume status type records
 *
 * token is set to zero for all items as this
 * is not a mapping but a filter table.
 *
 * name token
 */
struct s_kw VolumeStatus[]
    = {{"Unlabeled", 0}, {"Append", 0},   {"Full", 0},     {"Used", 0},
       {"Recycle", 0},   {"Purged", 0},   {"Cleaning", 0}, {"Error", 0},
       {"Archive", 0},   {"Disabled", 0}, {NULL, 0}};

/**
 * Keywords (RHS) permitted in Pool type records
 *
 * token is set to zero for all items as this
 * is not a mapping but a filter table.
 *
 * name token
 */
struct s_kw PoolTypes[]
    = {{"Backup", 0},    {"Copy", 0},    {"Cloned", 0}, {"Archive", 0},
       {"Migration", 0}, {"Scratch", 0}, {NULL, 0}};

json_t* json_item(const s_jl* item)
{
  json_t* json = json_object();

  json_object_set_new(json, "level", json_integer(item->level));
  json_object_set_new(json, "type", json_integer(item->job_type));

  return json;
}

json_t* json_item(const s_jt* item)
{
  json_t* json = json_object();

  json_object_set_new(json, "type", json_integer(item->job_type));

  return json;
}

json_t* json_datatype_header(const int type, const char* typeclass)
{
  json_t* json = json_object();
  const char* description = DatatypeToDescription(type);

  json_object_set_new(json, "number", json_integer(type));

  if (description) {
    json_object_set_new(json, "description", json_string(description));
  }

  if (typeclass) { json_object_set_new(json, "class", json_string(typeclass)); }

  return json;
}

json_t* json_datatype(const int type)
{
  return json_datatype_header(type, NULL);
}

json_t* json_datatype(const int type, const s_kw items[])
{
  json_t* json = json_datatype_header(type, "keyword");
  if (items) {
    json_t* values = json_object();
    for (int i = 0; items[i].name; i++) {
      json_object_set_new(values, items[i].name, json_item(&items[i]));
    }
    json_object_set_new(json, "values", values);
  }
  return json;
}

json_t* json_datatype(const int type, const s_jl items[])
{
  // FIXME: level_name keyword is not unique
  json_t* json = json_datatype_header(type, "keyword");
  if (items) {
    json_t* values = json_object();
    for (int i = 0; items[i].name; i++) {
      json_object_set_new(values, items[i].name, json_item(&items[i]));
    }
    json_object_set_new(json, "values", values);
  }
  return json;
}

json_t* json_datatype(const int type, const s_jt items[])
{
  json_t* json = json_datatype_header(type, "keyword");
  if (items) {
    json_t* values = json_object();
    for (int i = 0; items[i].name; i++) {
      json_object_set_new(values, items[i].name, json_item(&items[i]));
    }
    json_object_set_new(json, "values", values);
  }
  return json;
}

json_t* json_datatype(const int type, const ResourceItem items[])
{
  json_t* json = json_datatype_header(type, "sub");
  if (items) {
    json_t* values = json_object();
    for (int i = 0; items[i].name; i++) {
      json_object_set_new(values, items[i].name, json_item(&items[i]));
    }
    json_object_set_new(json, "values", values);
  }
  return json;
}

// Print configuration file schema in json format
bool PrintConfigSchemaJson(PoolMem& buffer)
{
  DatatypeName* datatype;
  const ResourceTable* resources = my_config->resource_definitions_;

  json_t* json = json_object();
  json_object_set_new(json, "format-version", json_integer(2));
  json_object_set_new(json, "component", json_string("bareos-dir"));
  json_object_set_new(json, "version", json_string(kBareosVersionStrings.Full));

  // Resources
  json_t* resource = json_object();
  json_object_set_new(json, "resource", resource);
  json_t* bareos_dir = json_object();
  json_object_set_new(resource, "bareos-dir", bareos_dir);

  for (int r = 0; resources[r].name; r++) {
    const ResourceTable& resource_table = my_config->resource_definitions_[r];
    json_object_set_new(bareos_dir, resource_table.name,
                        json_items(resource_table.items));
  }

  // Datatypes
  json_t* json_datatype_obj = json_object();
  json_object_set_new(json, "datatype", json_datatype_obj);

  int d = 0;
  while (GetDatatype(d)->name != NULL) {
    datatype = GetDatatype(d);

    switch (datatype->number) {
      case CFG_TYPE_RUNSCRIPT:
        json_object_set_new(json_datatype_obj,
                            DatatypeToString(datatype->number),
                            json_datatype(CFG_TYPE_RUNSCRIPT, runscript_items));
        break;
      case CFG_TYPE_INCEXC:
        json_object_set_new(json_datatype_obj,
                            DatatypeToString(datatype->number),
                            json_incexc(CFG_TYPE_INCEXC));
        break;
      case CFG_TYPE_OPTIONS:
        json_object_set_new(json_datatype_obj,
                            DatatypeToString(datatype->number),
                            json_options(CFG_TYPE_OPTIONS));
        break;
      case CFG_TYPE_PROTOCOLTYPE:
        json_object_set_new(
            json_datatype_obj, DatatypeToString(datatype->number),
            json_datatype(CFG_TYPE_PROTOCOLTYPE, backupprotocols));
        break;
      case CFG_TYPE_AUTHPROTOCOLTYPE:
        json_object_set_new(
            json_datatype_obj, DatatypeToString(datatype->number),
            json_datatype(CFG_TYPE_AUTHPROTOCOLTYPE, authprotocols));
        break;
      case CFG_TYPE_AUTHTYPE:
        json_object_set_new(json_datatype_obj,
                            DatatypeToString(datatype->number),
                            json_datatype(CFG_TYPE_AUTHTYPE, authmethods));
        break;
      case CFG_TYPE_LEVEL:
        json_object_set_new(json_datatype_obj,
                            DatatypeToString(datatype->number),
                            json_datatype(CFG_TYPE_LEVEL, joblevels));
        break;
      case CFG_TYPE_JOBTYPE:
        json_object_set_new(json_datatype_obj,
                            DatatypeToString(datatype->number),
                            json_datatype(CFG_TYPE_JOBTYPE, jobtypes));
        break;
      case CFG_TYPE_MIGTYPE:
        json_object_set_new(json_datatype_obj,
                            DatatypeToString(datatype->number),
                            json_datatype(CFG_TYPE_MIGTYPE, migtypes));
        break;
      case CFG_TYPE_REPLACE:
        json_object_set_new(json_datatype_obj,
                            DatatypeToString(datatype->number),
                            json_datatype(CFG_TYPE_REPLACE, ReplaceOptions));
        break;
      case CFG_TYPE_ACTIONONPURGE:
        json_object_set_new(
            json_datatype_obj, DatatypeToString(datatype->number),
            json_datatype(CFG_TYPE_ACTIONONPURGE, ActionOnPurgeOptions));
        break;
      case CFG_TYPE_POOLTYPE:
        json_object_set_new(json_datatype_obj,
                            DatatypeToString(datatype->number),
                            json_datatype(CFG_TYPE_POOLTYPE, PoolTypes));
        break;
      case CFG_TYPE_RUN:
        json_object_set_new(json_datatype_obj,
                            DatatypeToString(datatype->number),
                            json_datatype(CFG_TYPE_RUN, RunFields));
        break;
      default:
        json_object_set_new(json_datatype_obj,
                            DatatypeToString(datatype->number),
                            json_datatype(datatype->number));
        break;
    }
    d++;
  }

  /* following datatypes are ignored:
   * - VolumeStatus: only used in ua_dotcmds, not a datatype
   * - FS_option_kw: from inc_conf. Replaced by CFG_TYPE_OPTIONS",
   * options_items. */
  char* const json_str = json_dumps(json, JSON_INDENT(2));
  PmStrcat(buffer, json_str);
  free(json_str);
  json_decref(json);

  return true;
}

namespace {
template <typename T>
std::shared_ptr<T> GetRuntimeStatus(const std::string& name)
{
  static std::unordered_map<std::string, std::weak_ptr<T>> map{};

  std::shared_ptr<T> sptr = map[name].lock();
  if (!sptr) {
    sptr = std::make_shared<T>();
    map[name] = sptr;
  }
  Dmsg0(50, "Returning RuntimeStatus with use_count=%ld\n", sptr.use_count());
  return sptr;
}
}  // namespace

static bool CmdlineItem(PoolMem* buffer, const ResourceItem* item)
{
  PoolMem temp;
  PoolMem key;
  const char* nomod = "";
  const char* mod_start = nomod;
  const char* mod_end = nomod;

  if (item->is_deprecated) { return false; }

  if (item->uses_no_equal) {
    /* TODO: currently not supported */
    return false;
  }

  if (!item->is_required) {
    mod_start = "[";
    mod_end = "]";
  }

  // Tab completion only supports lower case keywords.
  key.strcat(item->name);
  key.toLower();

  temp.bsprintf(" %s%s=<%s>%s", mod_start, key.c_str(),
                DatatypeToString(item->type), mod_end);
  PmStrcat(buffer, temp.c_str());

  return true;
}

static bool CmdlineItems(PoolMem* buffer, const ResourceItem items[])
{
  if (!items) { return false; }

  for (int i = 0; items[i].name; i++) { CmdlineItem(buffer, &items[i]); }

  return true;
}

/**
 * Get the usage string for the console "configure" command.
 *
 * This will be all available resource directives.
 * They are formated in a way to be usable for command line completion.
 */
const char* GetUsageStringForConsoleConfigureCommand()
{
  PoolMem resourcename;

  if (!configure_usage_string) {
    configure_usage_string = new PoolMem(PM_BSOCK);
  }

  // Only fill the configure_usage_string once. The content is static.
  if (configure_usage_string->strlen() == 0) {
    // subcommand: add
    for (int r = 0; dird_resource_tables[r].name; r++) {
      auto& table = dird_resource_tables[r];
      /* Only one Director is allowed.
       * If the resource have not items, there is no need to add it. */
      if ((table.rcode != R_DIRECTOR) && (table.items)) {
        configure_usage_string->strcat("add ");
        resourcename.strcpy(table.name);
        resourcename.toLower();
        configure_usage_string->strcat(resourcename);
        CmdlineItems(configure_usage_string, table.items);
        configure_usage_string->strcat(" |\n");
      }
    }
    // subcommand: export
    configure_usage_string->strcat("export client=<client>");
  }

  return configure_usage_string->c_str();
}

void DestroyConfigureUsageString()
{
  if (configure_usage_string) {
    delete configure_usage_string;
    configure_usage_string = NULL;
  }
}

/**
 * Propagate the settings from source BareosResource to dest BareosResource
 * using the RES_ITEMS array.
 */
static void PropagateResource(const ResourceItem* items,
                              JobResource* source,
                              JobResource* dest)
{
  uint32_t offset;

  for (int i = 0; items[i].name; i++) {
    offset = items[i].offset;
    if (!dest->IsMemberPresent(items[i].name)
        && source->IsMemberPresent(items[i].name)) {
      switch (items[i].type) {
        case CFG_TYPE_STR:
        case CFG_TYPE_DIR:
        case CFG_TYPE_DIR_OR_CMD: {
          char **def_svalue, **svalue;

          // Handle strings and directory strings
          def_svalue = (char**)((char*)(source) + offset);
          svalue = (char**)((char*)dest + offset);
          if (*svalue) { free(*svalue); }
          *svalue = strdup(*def_svalue);
          dest->SetMemberPresent(items[i].name);
          SetBit(i, dest->inherit_content_);
          break;
        }
        case CFG_TYPE_RES: {
          char **def_svalue, **svalue;

          // Handle resources
          def_svalue = (char**)((char*)(source) + offset);
          svalue = (char**)((char*)dest + offset);
          if (*svalue) {
            Pmsg1(000, T_("Hey something is wrong. p=%p\n"), *svalue);
          }
          *svalue = *def_svalue;
          dest->SetMemberPresent(items[i].name);
          SetBit(i, dest->inherit_content_);
          break;
        }
        case CFG_TYPE_ALIST_STR: {
          alist<const char*>*orig_list, **new_list;

          // Handle alist strings
          orig_list = *(alist<const char*>**)((char*)(source) + offset);

          // See if there is anything on the list.
          if (orig_list && orig_list->size()) {
            new_list = (alist<const char*>**)((char*)(dest) + offset);

            if (!*new_list) {
              *new_list = new alist<const char*>(10, owned_by_alist);
            }

            for (auto* str : orig_list) { (*new_list)->append(strdup(str)); }

            dest->SetMemberPresent(items[i].name);
            SetBit(i, dest->inherit_content_);
          }
          break;
        }
        case CFG_TYPE_ALIST_RES: {
          alist<BareosResource*>*orig_list, **new_list;

          // Handle alist resources
          orig_list = *(alist<BareosResource*>**)((char*)(source) + offset);

          // See if there is anything on the list.
          if (orig_list && orig_list->size()) {
            new_list = (alist<BareosResource*>**)((char*)(dest) + offset);

            if (!*new_list) {
              *new_list = new alist<BareosResource*>(10, not_owned_by_alist);
            }

            for (auto* res : orig_list) { (*new_list)->append(res); }

            dest->SetMemberPresent(items[i].name);
            SetBit(i, dest->inherit_content_);
          }
          break;
        }
        case CFG_TYPE_ACL: {
          alist<const char*>*orig_list, **new_list;

          // Handle ACL lists.
          orig_list = ((alist<const char*>**)((char*)(source)
                                              + offset))[items[i].code];

          // See if there is anything on the list.
          if (orig_list && orig_list->size()) {
            new_list = &((
                (alist<const char*>**)((char*)(dest) + offset))[items[i].code]);

            if (!*new_list) {
              *new_list = new alist<const char*>(10, owned_by_alist);
            }

            for (auto* str : orig_list) { (*new_list)->append(strdup(str)); }

            dest->SetMemberPresent(items[i].name);
            SetBit(i, dest->inherit_content_);
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

          /* Handle integer fields
           *    Note, our StoreBit does not handle bitmaped fields */
          def_ivalue = (uint32_t*)((char*)(source) + offset);
          ivalue = (uint32_t*)((char*)dest + offset);
          *ivalue = *def_ivalue;
          dest->SetMemberPresent(items[i].name);
          SetBit(i, dest->inherit_content_);
          break;
        }
        case CFG_TYPE_TIME:
        case CFG_TYPE_SIZE64:
        case CFG_TYPE_INT64:
        case CFG_TYPE_SPEED: {
          int64_t *def_lvalue, *lvalue;

          // Handle 64 bit integer fields
          def_lvalue = (int64_t*)((char*)(source) + offset);
          lvalue = (int64_t*)((char*)dest + offset);
          *lvalue = *def_lvalue;
          dest->SetMemberPresent(items[i].name);
          SetBit(i, dest->inherit_content_);
          break;
        }
        case CFG_TYPE_BOOL: {
          bool *def_bvalue, *bvalue;

          // Handle bool fields
          def_bvalue = (bool*)((char*)(source) + offset);
          bvalue = (bool*)((char*)dest + offset);
          *bvalue = *def_bvalue;
          dest->SetMemberPresent(items[i].name);
          SetBit(i, dest->inherit_content_);
          break;
        }
        case CFG_TYPE_AUTOPASSWORD: {
          s_password *s_pwd, *d_pwd;

          // Handle password fields
          s_pwd = (s_password*)((char*)(source) + offset);
          d_pwd = (s_password*)((char*)(dest) + offset);

          d_pwd->encoding = s_pwd->encoding;
          d_pwd->value = strdup(s_pwd->value);
          dest->SetMemberPresent(items[i].name);
          SetBit(i, dest->inherit_content_);
          break;
        }
        default:
          Dmsg2(200,
                "Don't know how to propagate resource %s of configtype %d\n",
                items[i].name, items[i].type);
          break;
      }
    }
  }
}


// Ensure that all required items are present
bool ValidateResource(int res_type,
                      const ResourceItem* items,
                      BareosResource* res)
{
  if (res_type == R_JOBDEFS) {
    // a jobdef don't have to be fully defined.
    return true;
  } else if (!res->Validate()) {
    return false;
  }

  for (int i = 0; items[i].name; i++) {
    if (items[i].is_required) {
      if (!res->IsMemberPresent(items[i].name)) {
        Jmsg(NULL, M_ERROR, 0,
             T_("\"%s\" directive in %s \"%s\" resource is required, but not "
                "found.\n"),
             items[i].name, my_config->ResToStr(res_type), res->resource_name_);
        return false;
      }
    }

    // If this triggers, take a look at lib/parse_conf.h
    if (i >= MAX_RES_ITEMS) {
      Emsg1(M_ERROR, 0, T_("Too many items in %s resource\n"),
            my_config->ResToStr(res_type));
      return false;
    }
  }

  return true;
}

bool JobResource::Validate()
{
  /* For Copy and Migrate we can have Jobs without a client or fileset.
   * As for a copy we use the original Job as a reference for the Read storage
   * we also don't need to check if there is an explicit storage definition in
   * either the Job or the Read pool. */
  switch (JobType) {
    case JT_COPY:
    case JT_MIGRATE:
      break;
    default:
      // All others must have a client and fileset.
      if (!client) {
        Jmsg(NULL, M_ERROR, 0,
             T_("\"client\" directive in Job \"%s\" resource is required, but "
                "not found.\n"),
             resource_name_);
        return false;
      }

      if (!fileset) {
        Jmsg(NULL, M_ERROR, 0,
             T_("\"fileset\" directive in Job \"%s\" resource is required, but "
                "not found.\n"),
             resource_name_);
        return false;
      }

      if (!storage && (!pool || !pool->storage)) {
        Jmsg(NULL, M_ERROR, 0,
             T_("No storage specified in Job \"%s\" nor in Pool.\n"),
             resource_name_);
        return false;
      }
      break;
  }
  if (JobLevel == L_BASE) {
    Jmsg(NULL, M_WARNING, 0,
         T_("Job \"%s\" has level 'Base' which is deprecated!\n"),
         resource_name_);
  }
  return true;
}

bool CatalogResource::Validate() { return true; }

char* CatalogResource::display(POOLMEM* dst)
{
  Mmsg(dst,
       "catalog=%s\ndb_name=%s\ndb_driver=%s\ndb_user=%s\n"
       "db_password=%s\ndb_address=%s\ndb_port=%i\n"
       "db_socket=%s\n",
       resource_name_, NPRTB(db_name), NPRTB(db_driver), NPRTB(db_user),
       NPRTB(db_password.value), NPRTB(db_address), db_port, NPRTB(db_socket));

  return dst;
}


static void PrintConfigRunscript(OutputFormatterResource& send,
                                 const ResourceItem& item,
                                 bool inherited,
                                 bool verbose)
{
  if (!Bstrcasecmp(item.name, "runscript")) { return; }

  alist<RunScript*>* list = GetItemVariable<alist<RunScript*>*>(item);
  if ((!list) or (list->empty())) { return; }

  send.ArrayStart(item.name, inherited, "");

  for (auto* runscript : list) {
    std::string esc = EscapeString(runscript->command.c_str());

    bool print_as_comment = inherited;

    // do not print if inherited by a JobDef
    if (runscript->from_jobdef) {
      if (verbose) {
        print_as_comment = true;
      } else {
        continue;
      }
    }

    if (runscript->short_form) {
      send.SubResourceStart(NULL, print_as_comment, "");

      if (runscript->when == SCRIPT_Before && /* runbeforejob */
          runscript->target.empty()) {
        send.KeyQuotedString("RunBeforeJob", esc.c_str(), print_as_comment);
      } else if (runscript->when == SCRIPT_After && /* runafterjob */
                 runscript->on_success && !runscript->on_failure
                 && !runscript->fail_on_error && runscript->target.empty()) {
        send.KeyQuotedString("RunAfterJob", esc.c_str(), print_as_comment);
      } else if (runscript->when == SCRIPT_After && /* client run after job */
                 runscript->on_success && !runscript->on_failure
                 && !runscript->fail_on_error && !runscript->target.empty()) {
        send.KeyQuotedString("ClientRunAfterJob", esc.c_str(),
                             print_as_comment);
      } else if (runscript->when == SCRIPT_Before && /* client run before job */
                 !runscript->target.empty()) {
        send.KeyQuotedString("ClientRunBeforeJob", esc.c_str(),
                             print_as_comment);
      } else if (runscript->when == SCRIPT_After && /* run after failed job */
                 runscript->on_failure && !runscript->on_success
                 && !runscript->fail_on_error && runscript->target.empty()) {
        send.KeyQuotedString("RunAfterFailedJob", esc.c_str(),
                             print_as_comment);
      }
      send.SubResourceEnd(NULL, print_as_comment, "");
    } else {
      send.SubResourceStart(NULL, print_as_comment, "RunScript {\n");

      bool command_runscript = true;
      char* cmdstring = (char*)"Command"; /* '|' */
      if (runscript->cmd_type == CONSOLE_CMD) {
        cmdstring = (char*)"Console";
        command_runscript = false;
      }
      send.KeyQuotedString(cmdstring, esc.c_str(), print_as_comment);

      // Default: never
      char* when = (char*)"never";
      switch (runscript->when) {
        case SCRIPT_Before:
          when = (char*)"before";
          break;
        case SCRIPT_After:
          when = (char*)"after";
          break;
        case SCRIPT_AfterVSS:
          when = (char*)"aftervss";
          break;
        case SCRIPT_Any:
          when = (char*)"always";
          break;
      }

      if (!Bstrcasecmp(when, "never")) { /* suppress default value */
        send.KeyQuotedString("RunsWhen", when, print_as_comment);
      } else if (verbose) {
        send.KeyQuotedString("RunsWhen", when, true);
      }

      // Default: fail_on_error = true
      if (!runscript->fail_on_error) {
        send.KeyBool("FailJobOnError", runscript->fail_on_error,
                     print_as_comment);
      } else if (verbose) {
        send.KeyBool("FailJobOnError", runscript->fail_on_error, true);
      }

      // Default: on_success = true
      if (!runscript->on_success) {
        send.KeyBool("RunsOnSuccess", runscript->on_success, print_as_comment);
      } else if (verbose) {
        send.KeyBool("RunsOnSuccess", runscript->on_success, true);
      }

      // Default: on_failure = false
      if (runscript->on_failure) {
        send.KeyBool("RunsOnFailure", runscript->on_failure, print_as_comment);
      } else if (verbose) {
        send.KeyBool("RunsOnFailure", runscript->on_failure, true);
      }

      if (command_runscript) {
        // Default: runsonclient = yes
        bool runsonclient = !runscript->target.empty();
        if (!runsonclient) {
          send.KeyBool("RunsOnClient", false, print_as_comment);
        } else if (verbose) {
          send.KeyBool("RunsOnClient", true, true);
        }
      }

      send.SubResourceEnd(NULL, print_as_comment, "}\n");
    }
  }

  send.ArrayEnd(item.name, inherited, "");
}

std::optional<time_t> RunResource::NextScheduleTime(time_t start,
                                                    uint32_t ndays) const
{
  for (uint32_t d = 0; d <= ndays; ++d) {
    if (date_time_mask.TriggersOnDay(start)) {
      struct tm tm = {};
      Blocaltime(&start, &tm);
      for (int h = (d == 0 ? tm.tm_hour : 0); h < 24; ++h) {
        if (BitIsSet(h, date_time_mask.hour)) {
          tm.tm_hour = h;
          tm.tm_min = minute;
          tm.tm_sec = 0;
          return mktime(&tm);
        }
      }
    }
    start += 24 * 60 * 60;
  }
  return std::nullopt;
}

static std::string PrintConfigRun(RunResource* run)
{
  PoolMem temp;

  bool all_set;
  int i, nr_items;
  int interval_start;

  /* clang-format off */

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

  /* clang-format on */


  PoolMem run_str;  /* holds the complete run= ... line */
  PoolMem interval; /* is one entry of day/month/week etc. */

  // Overrides
  if (run->pool) {
    Mmsg(temp, "pool=\"%s\" ", run->pool->resource_name_);
    PmStrcat(run_str, temp.c_str());
  }

  if (run->full_pool) {
    Mmsg(temp, "fullpool=\"%s\" ", run->full_pool->resource_name_);
    PmStrcat(run_str, temp.c_str());
  }

  if (run->vfull_pool) {
    Mmsg(temp, "virtualfullpool=\"%s\" ", run->vfull_pool->resource_name_);
    PmStrcat(run_str, temp.c_str());
  }

  if (run->inc_pool) {
    Mmsg(temp, "incrementalpool=\"%s\" ", run->inc_pool->resource_name_);
    PmStrcat(run_str, temp.c_str());
  }

  if (run->diff_pool) {
    Mmsg(temp, "differentialpool=\"%s\" ", run->diff_pool->resource_name_);
    PmStrcat(run_str, temp.c_str());
  }

  if (run->next_pool) {
    Mmsg(temp, "nextpool=\"%s\" ", run->next_pool->resource_name_);
    PmStrcat(run_str, temp.c_str());
  }

  if (run->level) {
    for (int j = 0; joblevels[j].name; j++) {
      if (joblevels[j].level == run->level) {
        PmStrcat(run_str, joblevels[j].name);
        PmStrcat(run_str, " ");
        break;
      }
    }
  }

  if (run->storage) {
    Mmsg(temp, "storage=\"%s\" ", run->storage->resource_name_);
    PmStrcat(run_str, temp.c_str());
  }

  if (run->msgs) {
    Mmsg(temp, "messages=\"%s\" ", run->msgs->resource_name_);
    PmStrcat(run_str, temp.c_str());
  }

  if (run->Priority && run->Priority != 10) {
    Mmsg(temp, "priority=%d ", run->Priority);
    PmStrcat(run_str, temp.c_str());
  }

  if (run->MaxRunSchedTime) {
    Mmsg(temp, "maxrunschedtime=%" PRIu64 " ", run->MaxRunSchedTime);
    PmStrcat(run_str, temp.c_str());
  }

  if (run->accurate) {
    /* TODO: You cannot distinct if accurate was not set or if it was set to
     * no maybe we need an additional variable like "accurate_set". */
    Mmsg(temp, "accurate=\"%s\" ", "yes");
    PmStrcat(run_str, temp.c_str());
  }

  // Now the time specification

  // run->mday , output is just the number comma separated
  PmStrcpy(temp, "");

  // First see if not all bits are set.
  all_set = true;
  nr_items = 31;
  for (i = 0; i < nr_items; i++) {
    if (!BitIsSet(i, run->date_time_mask.mday)) { all_set = false; }
  }

  if (!all_set) {
    interval_start = -1;

    for (i = 0; i < nr_items; i++) {
      if (BitIsSet(i, run->date_time_mask.mday)) {
        if (interval_start
            == -1) {          /* bit is set and we are not in an interval */
          interval_start = i; /* start an interval */
          Dmsg1(200, "starting interval at %d\n", i + 1);
          Mmsg(interval, ",%d", i + 1);
          PmStrcat(temp, interval.c_str());
        }
      }

      if (!BitIsSet(i, run->date_time_mask.mday)) {
        if (interval_start != -1) { /* bit is unset and we are in an interval */
          if ((i - interval_start) > 1) {
            Dmsg2(200, "found end of interval from %d to %d\n",
                  interval_start + 1, i);
            Mmsg(interval, "-%d", i);
            PmStrcat(temp, interval.c_str());
          }
          interval_start = -1; /* end the interval */
        }
      }
    }

    /* See if we are still in an interval and the last bit is also set then
     * the interval stretches to the last item. */
    i = nr_items - 1;
    if (interval_start != -1 && BitIsSet(i, run->date_time_mask.mday)) {
      if ((i - interval_start) > 1) {
        Dmsg2(200, "found end of interval from %d to %d\n", interval_start + 1,
              i + 1);
        Mmsg(interval, "-%d", i + 1);
        PmStrcat(temp, interval.c_str());
      }
    }

    PmStrcat(temp, " ");
    PmStrcat(run_str, temp.c_str() + 1); /* jump over first comma*/
  }

  /* run->wom output is 1st, 2nd... 5th or last comma separated
   *                    first, second, third... is also allowed
   *                    but we ignore that for now */
  all_set = true;
  nr_items = 5;
  for (i = 0; i < nr_items; i++) {
    if (!BitIsSet(i, run->date_time_mask.wom)) { all_set = false; }
  }

  if (!all_set) {
    interval_start = -1;

    PmStrcpy(temp, "");
    for (i = 0; i < nr_items; i++) {
      if (BitIsSet(i, run->date_time_mask.wom)) {
        if (interval_start
            == -1) {          /* bit is set and we are not in an interval */
          interval_start = i; /* start an interval */
          Dmsg1(200, "starting interval at %s\n", ordinals[i]);
          Mmsg(interval, ",%s", ordinals[i]);
          PmStrcat(temp, interval.c_str());
        }
      }

      if (!BitIsSet(i, run->date_time_mask.wom)) {
        if (interval_start != -1) { /* bit is unset and we are in an interval */
          if ((i - interval_start) > 1) {
            Dmsg2(200, "found end of interval from %s to %s\n",
                  ordinals[interval_start], ordinals[i - 1]);
            Mmsg(interval, "-%s", ordinals[i - 1]);
            PmStrcat(temp, interval.c_str());
          }
          interval_start = -1; /* end the interval */
        }
      }
    }

    /* See if we are still in an interval and the last bit is also set then
     * the interval stretches to the last item. */
    i = nr_items - 1;
    if (interval_start != -1 && BitIsSet(i, run->date_time_mask.wom)) {
      if ((i - interval_start) > 1) {
        Dmsg2(200, "found end of interval from %s to %s\n",
              ordinals[interval_start], ordinals[i]);
        Mmsg(interval, "-%s", ordinals[i]);
        PmStrcat(temp, interval.c_str());
      }
    }

    PmStrcat(temp, " ");
    PmStrcat(run_str, temp.c_str() + 1); /* jump over first comma*/
  }
  if (run->date_time_mask.last_7days_of_month) {
    PmStrcat(run_str, "last");
    PmStrcat(run_str, " ");
  }

  // run->wday output is Sun, Mon, ..., Sat comma separated
  all_set = true;
  nr_items = 7;
  for (i = 0; i < nr_items; i++) {
    if (!BitIsSet(i, run->date_time_mask.wday)) { all_set = false; }
  }

  if (!all_set) {
    interval_start = -1;

    PmStrcpy(temp, "");
    for (i = 0; i < nr_items; i++) {
      if (BitIsSet(i, run->date_time_mask.wday)) {
        if (interval_start
            == -1) {          /* bit is set and we are not in an interval */
          interval_start = i; /* start an interval */
          Dmsg1(200, "starting interval at %s\n", weekdays[i]);
          Mmsg(interval, ",%s", weekdays[i]);
          PmStrcat(temp, interval.c_str());
        }
      }

      if (!BitIsSet(i, run->date_time_mask.wday)) {
        if (interval_start != -1) { /* bit is unset and we are in an interval */
          if ((i - interval_start) > 1) {
            Dmsg2(200, "found end of interval from %s to %s\n",
                  weekdays[interval_start], weekdays[i - 1]);
            Mmsg(interval, "-%s", weekdays[i - 1]);
            PmStrcat(temp, interval.c_str());
          }
          interval_start = -1; /* end the interval */
        }
      }
    }

    /* See if we are still in an interval and the last bit is also set then
     * the interval stretches to the last item. */
    i = nr_items - 1;
    if (interval_start != -1 && BitIsSet(i, run->date_time_mask.wday)) {
      if ((i - interval_start) > 1) {
        Dmsg2(200, "found end of interval from %s to %s\n",
              weekdays[interval_start], weekdays[i]);
        Mmsg(interval, "-%s", weekdays[i]);
        PmStrcat(temp, interval.c_str());
      }
    }

    PmStrcat(temp, " ");
    PmStrcat(run_str, temp.c_str() + 1); /* jump over first comma*/
  }

  // run->month output is Jan, Feb, ..., Dec comma separated
  all_set = true;
  nr_items = 12;
  for (i = 0; i < nr_items; i++) {
    if (!BitIsSet(i, run->date_time_mask.month)) { all_set = false; }
  }

  if (!all_set) {
    interval_start = -1;

    PmStrcpy(temp, "");
    for (i = 0; i < nr_items; i++) {
      if (BitIsSet(i, run->date_time_mask.month)) {
        if (interval_start
            == -1) {          /* bit is set and we are not in an interval */
          interval_start = i; /* start an interval */
          Dmsg1(200, "starting interval at %s\n", months[i]);
          Mmsg(interval, ",%s", months[i]);
          PmStrcat(temp, interval.c_str());
        }
      }

      if (!BitIsSet(i, run->date_time_mask.month)) {
        if (interval_start != -1) { /* bit is unset and we are in an interval */
          if ((i - interval_start) > 1) {
            Dmsg2(200, "found end of interval from %s to %s\n",
                  months[interval_start], months[i - 1]);
            Mmsg(interval, "-%s", months[i - 1]);
            PmStrcat(temp, interval.c_str());
          }
          interval_start = -1; /* end the interval */
        }
      }
    }

    /* See if we are still in an interval and the last bit is also set then
     * the interval stretches to the last item. */
    i = nr_items - 1;
    if (interval_start != -1 && BitIsSet(i, run->date_time_mask.month)) {
      if ((i - interval_start) > 1) {
        Dmsg2(200, "found end of interval from %s to %s\n",
              months[interval_start], months[i]);
        Mmsg(interval, "-%s", months[i]);
        PmStrcat(temp, interval.c_str());
      }
    }

    PmStrcat(temp, " ");
    PmStrcat(run_str, temp.c_str() + 1); /* jump over first comma*/
  }

  // run->woy output is w00 - w53, comma separated
  all_set = true;
  nr_items = 54;
  for (i = 0; i < nr_items; i++) {
    if (!BitIsSet(i, run->date_time_mask.woy)) { all_set = false; }
  }

  if (!all_set) {
    interval_start = -1;

    PmStrcpy(temp, "");
    for (i = 0; i < nr_items; i++) {
      if (BitIsSet(i, run->date_time_mask.woy)) {
        if (interval_start
            == -1) {          /* bit is set and we are not in an interval */
          interval_start = i; /* start an interval */
          Dmsg1(200, "starting interval at w%02d\n", i);
          Mmsg(interval, ",w%02d", i);
          PmStrcat(temp, interval.c_str());
        }
      }

      if (!BitIsSet(i, run->date_time_mask.woy)) {
        if (interval_start != -1) { /* bit is unset and we are in an interval */
          if ((i - interval_start) > 1) {
            Dmsg2(200, "found end of interval from w%02d to w%02d\n",
                  interval_start, i - 1);
            Mmsg(interval, "-w%02d", i - 1);
            PmStrcat(temp, interval.c_str());
          }
          interval_start = -1; /* end the interval */
        }
      }
    }

    /* See if we are still in an interval and the last bit is also set then
     * the interval stretches to the last item. */
    i = nr_items - 1;
    if (interval_start != -1 && BitIsSet(i, run->date_time_mask.woy)) {
      if ((i - interval_start) > 1) {
        Dmsg2(200, "found end of interval from w%02d to w%02d\n",
              interval_start, i);
        Mmsg(interval, "-w%02d", i);
        PmStrcat(temp, interval.c_str());
      }
    }

    PmStrcat(temp, " ");
    PmStrcat(run_str, temp.c_str() + 1); /* jump over first comma*/
  }

  /* run->hour output is HH:MM for hour and minute though its a bitfield.
   * only "hourly" sets all bits. */
  PmStrcpy(temp, "");
  for (i = 0; i < 24; i++) {
    if (BitIsSet(i, run->date_time_mask.hour)) {
      Mmsg(temp, "at %02d:%02d", i, run->minute);
      PmStrcat(run_str, temp.c_str());
    }
  }

  // run->minute output is smply the minute in HH:MM

  return std::string(run_str.c_str());
}


static void PrintConfigRun(OutputFormatterResource& send,
                           const ResourceItem* item,
                           bool inherited)
{
  RunResource* run = GetItemVariable<RunResource*>(*item);
  if (run != NULL) {
    std::vector<std::string> run_strings;
    while (run) {
      std::string run_str = PrintConfigRun(run);
      run_strings.push_back(run_str);
      run = run->next;
    } /* loop over runs */
    send.KeyMultipleStringsOnePerLine("Run", run_strings, inherited, false);
  }
}


std::string FilesetResource::GetOptionValue(const char** option)
{
  /* Extract substring until next ":" chararcter.
   * Modify the string pointer. Move it forward. */
  std::string value;
  (*option)++; /* skip option key */
  for (; *option[0] && *option[0] != ':'; (*option)++) { value += *option[0]; }
  return value;
}


void FilesetResource::PrintConfigIncludeExcludeOptions(
    OutputFormatterResource& send,
    FileOptions* fo,
    bool)
{
  send.SubResourceStart(NULL, false, "Options {\n");

  for (const char* p = &fo->opts[0]; *p; p++) {
    switch (*p) {
      case '0': /* no option */
        break;
      case 'a': /* alway replace */
        send.KeyQuotedString("Replace", "Always");
        break;
      case 'C': /* */
        send.KeyQuotedString("Accurate", GetOptionValue(&p));
        break;
      case 'c':
        send.KeyBool("CheckFileChanges", true);
        break;
      case 'd':
        switch (*(p + 1)) {
          case '1':
            send.KeyQuotedString("Shadowing", "LocalWarn");
            break;
          case '2':
            send.KeyQuotedString("Shadowing", "LocalRemove");
            break;
          case '3':
            send.KeyQuotedString("Shadowing", "GlobalWarn");
            break;
          case '4':
            send.KeyQuotedString("Shadowing", "GlobalRemove");
            break;
        }
        p++;
        break;
      case 'e':
        send.KeyBool("Exclude", true);
        break;
      case 'f':
        send.KeyBool("OneFS", false);
        break;
      case 'h': /* no recursion */
        send.KeyBool("Recurse", false);
        break;
      case 'H': /* no hard link handling */
        send.KeyBool("HardLinks", false);
        break;
      case 'i':
        send.KeyBool("IgnoreCase", true);
        break;
      case 'M': /* MD5 */
        send.KeyQuotedString("Signature", "MD5");
        break;
      case 'n':
        send.KeyQuotedString("Replace", "Never");
        break;
      case 'p': /* use portable data format */
        send.KeyBool("Portable", true);
        break;
      case 'P': /* strip path */
        send.KeyQuotedString("Strip", GetOptionValue(&p));
        break;
      case 'R': /* Resource forks and Finder Info */
        send.KeyBool("HfsPlusSupport", true);
        break;
      case 'r': /* read fifo */
        send.KeyBool("ReadFifo", true);
        break;
      case 'S':
        switch (*(p + 1)) {
#ifdef HAVE_SHA2
          case '2':
            send.KeyQuotedString("Signature", "SHA256");
            p++;
            break;
          case '3':
            send.KeyQuotedString("Signature", "SHA512");
            p++;
            break;
#endif
          case '4':
            send.KeyQuotedString("Signature", "XXH128");
            p++;
            break;
          default:
            send.KeyQuotedString("Signature", "SHA1");
            break;
        }
        break;
      case 's':
        send.KeyBool("Sparse", true);
        break;
      case 'm':
        send.KeyBool("MtimeOnly", true);
        break;
      case 'k':
        send.KeyBool("KeepAtime", true);
        break;
      case 'K':
        send.KeyBool("NoAtime", true);
        break;
      case 'A':
        send.KeyBool("AclSupport", true);
        break;
      case 'N':
        send.KeyBool("HonorNoDumpFlag", true);
        break;
      case 'V': /* verify options */
        send.KeyQuotedString("Verify", GetOptionValue(&p));
        break;
      case 'w':
        send.KeyQuotedString("Replace", "IfNewer");
        break;
      case 'W':
        send.KeyBool("EnhancedWild", true);
        break;
      case 'z': /* size */
        send.KeyString("Size", GetOptionValue(&p));
        break;
      case 'Z': /* compression */
        p++;    /* skip Z */
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
            send.KeyQuotedString("Compression", std::string("GZIP") + *p);
            break;
          case 'o':
            send.KeyQuotedString("Compression", "LZO");
            break;
          case 'f':
            p++; /* skip f */
            switch (*p) {
              case 'f':
                send.KeyQuotedString("Compression", "LZFAST");
                break;
              case '4':
                send.KeyQuotedString("Compression", "LZ4");
                break;
              case 'h':
                send.KeyQuotedString("Compression", "LZ4HC");
                break;
              default:
                Emsg1(M_ERROR, 0,
                      T_("Unknown compression include/exclude option: "
                         "%c\n"),
                      *p);
                break;
            }
            break;
          default:
            Emsg1(M_ERROR, 0,
                  T_("Unknown compression include/exclude option: %c\n"), *p);
            break;
        }
        break;
      case 'X':
        send.KeyBool("XattrSupport", true);
        break;
      case 'x':
        send.KeyBool("AutoExclude", false);
        break;
      default:
        Emsg1(M_ERROR, 0, T_("Unknown include/exclude option: %c\n"), *p);
        break;
    }
  }

  send.KeyMultipleStringsOnePerLine("Regex", std::addressof(fo->regex));
  send.KeyMultipleStringsOnePerLine("RegexDir", std::addressof(fo->regexdir));
  send.KeyMultipleStringsOnePerLine("RegexFile", std::addressof(fo->regexfile));
  send.KeyMultipleStringsOnePerLine("Wild", std::addressof(fo->wild));
  send.KeyMultipleStringsOnePerLine("WildDir", std::addressof(fo->wilddir));
  send.KeyMultipleStringsOnePerLine("WildFile", std::addressof(fo->wildfile));
  /*  Wildbase is WildFile not containing a / or \\
   *  see  void StoreWild() in inc_conf.c
   *  so we need to translate it back to a Wild File entry */
  send.KeyMultipleStringsOnePerLine("WildBase", std::addressof(fo->wildbase));
  send.KeyMultipleStringsOnePerLine("Base", std::addressof(fo->base));
  send.KeyMultipleStringsOnePerLine("FsType", std::addressof(fo->fstype));
  send.KeyMultipleStringsOnePerLine("DriveType", std::addressof(fo->Drivetype));
  send.KeyMultipleStringsOnePerLine("Meta", std::addressof(fo->meta));

  if (fo->plugin) { send.KeyString("Plugin", fo->plugin); }
  if (fo->reader) { send.KeyString("Reader", fo->reader); }
  if (fo->writer) { send.KeyString("Writer", fo->writer); }

  send.SubResourceEnd(NULL, false, "}\n");
}


bool FilesetResource::PrintConfig(OutputFormatterResource& send,
                                  const ConfigurationParser&,
                                  bool,
                                  bool verbose)
{
  bool inherited = false;

  Dmsg0(200, "FilesetResource::PrintConfig\n");

  send.ResourceStart("FileSets", "FileSet", this->resource_name_);
  send.KeyQuotedString("Name", this->resource_name_);

  if (this->description_ != NULL) {
    send.KeyQuotedString("Description", this->description_);
  }

  if (include_items.size() > 0) {
    send.ArrayStart("Include", inherited, "");

    for (std::size_t i = 0; i < include_items.size(); i++) {
      IncludeExcludeItem* incexe = include_items[i];

      send.SubResourceStart(NULL, inherited, "Include {\n");

      // Start options blocks
      if (incexe->file_options_list.size() > 0) {
        send.ArrayStart("Options", inherited, "");

        for (std::size_t j = 0; j < incexe->file_options_list.size(); j++) {
          FileOptions* fo = incexe->file_options_list[j];

          PrintConfigIncludeExcludeOptions(send, fo, verbose);
        } /* end options block */
        send.ArrayEnd("Options", inherited, "");
      }

      // File = entries.
      send.KeyMultipleStringsOnePerLine(
          "File", std::addressof(incexe->name_list), false, true, true);

      // Plugin = entries.
      send.KeyMultipleStringsOnePerLine("Plugin",
                                        std::addressof(incexe->plugin_list));

      // ExcludeDirContaining = entry.
      send.KeyMultipleStringsOnePerLine("ExcludeDirContaining",
                                        std::addressof(incexe->ignoredir));

      send.SubResourceEnd(NULL, inherited, "}\n");


    } /* loop over all include blocks */

    send.ArrayEnd("Include", inherited, "");
  }

  if (exclude_items.size() > 0) {
    send.ArrayStart("Exclude", inherited, "");

    for (std::size_t j = 0; j < exclude_items.size(); j++) {
      IncludeExcludeItem* incexe = exclude_items[j];

      send.SubResourceStart(NULL, inherited, "Exclude {\n");

      send.KeyMultipleStringsOnePerLine("File",
                                        std::addressof(incexe->name_list));

      send.SubResourceEnd(NULL, inherited, "}\n");

    } /* loop over all exclude blocks */

    send.ArrayEnd("Exclude", inherited, "");
  }

  send.ResourceEnd("FileSets", "FileSet", this->resource_name_);

  return true;
}


const char* AuthenticationProtocolTypeToString(uint32_t auth_protocol)
{
  for (int i = 0; authprotocols[i].name; i++) {
    if (authprotocols[i].token == auth_protocol) {
      return authprotocols[i].name;
    }
  }

  return "Unknown";
}

const char* JobLevelToString(int level)
{
  static char level_no[30];
  const char* str = level_no;

  Bsnprintf(level_no, sizeof(level_no), "%c (%d)", level,
            level); /* default if not found */
  for (int i = 0; joblevels[i].name; i++) {
    if (level == (int)joblevels[i].level) {
      str = joblevels[i].name;
      break;
    }
  }

  return str;
}

static void FreeIncludeExcludeItem(IncludeExcludeItem* incexe)
{
  incexe->name_list.destroy();
  incexe->plugin_list.destroy();
  for (FileOptions* fopt : incexe->file_options_list) {
    fopt->regex.destroy();
    fopt->regexdir.destroy();
    fopt->regexfile.destroy();
    fopt->wild.destroy();
    fopt->wilddir.destroy();
    fopt->wildfile.destroy();
    fopt->wildbase.destroy();
    fopt->base.destroy();
    fopt->fstype.destroy();
    fopt->Drivetype.destroy();
    fopt->meta.destroy();
    if (fopt->plugin) { free(fopt->plugin); }
    if (fopt->reader) { free(fopt->reader); }
    if (fopt->writer) { free(fopt->writer); }
    delete fopt;
  }
  incexe->file_options_list.clear();
  incexe->ignoredir.destroy();
  delete incexe;
}

static bool UpdateResourcePointer(int type, const ResourceItem* items)
{
  switch (type) {
    case R_PROFILE:
    case R_CATALOG:
    case R_MSGS:
    case R_FILESET:
      // Resources not containing a resource
      break;
    case R_POOL: {
      /* Resources containing another resource or alist. First
       * look up the resource which contains another resource. It
       * was written during pass 1.  Then stuff in the pointers to
       * the resources it contains, which were inserted this pass.
       * Finally, it will all be stored back.
       *
       * Find resource saved in pass 1 */
      PoolResource* p = dynamic_cast<PoolResource*>(
          my_config->GetResWithName(R_POOL, res_pool->resource_name_));
      if (!p) {
        Emsg1(M_ERROR, 0, T_("Cannot find Pool resource %s\n"),
              res_pool->resource_name_);
        return false;
      } else {
        p->NextPool = res_pool->NextPool;
        p->RecyclePool = res_pool->RecyclePool;
        p->ScratchPool = res_pool->ScratchPool;
        p->storage = res_pool->storage;
        if (res_pool->catalog || !res_pool->use_catalog) {
          p->catalog = res_pool->catalog;
        }
      }
      break;
    }
    case R_CONSOLE: {
      ConsoleResource* p = dynamic_cast<ConsoleResource*>(
          my_config->GetResWithName(R_CONSOLE, res_con->resource_name_));
      if (!p) {
        Emsg1(M_ERROR, 0, T_("Cannot find Console resource %s\n"),
              res_con->resource_name_);
        return false;
      } else {
        p->tls_cert_.allowed_certificate_common_names_
            = std::move(res_con->tls_cert_.allowed_certificate_common_names_);
        p->user_acl.profiles = res_con->user_acl.profiles;
        p->user_acl.corresponding_resource = p;
      }
      break;
    }
    case R_USER: {
      UserResource* p = dynamic_cast<UserResource*>(
          my_config->GetResWithName(R_USER, res_user->resource_name_));
      if (!p) {
        Emsg1(M_ERROR, 0, T_("Cannot find User resource %s\n"),
              res_user->resource_name_);
        return false;
      } else {
        p->user_acl.profiles = res_user->user_acl.profiles;
        p->user_acl.corresponding_resource = p;
      }
      break;
    }
    case R_DIRECTOR: {
      DirectorResource* p = dynamic_cast<DirectorResource*>(
          my_config->GetResWithName(R_DIRECTOR, res_dir->resource_name_));
      if (!p) {
        Emsg1(M_ERROR, 0, T_("Cannot find Director resource %s\n"),
              res_dir->resource_name_);
        return false;
      } else {
        p->plugin_names = res_dir->plugin_names;
        p->messages = res_dir->messages;
        p->tls_cert_.allowed_certificate_common_names_
            = std::move(res_dir->tls_cert_.allowed_certificate_common_names_);
      }
      break;
    }
    case R_STORAGE: {
      StorageResource* p = dynamic_cast<StorageResource*>(
          my_config->GetResWithName(type, res_store->resource_name_));
      if (!p) {
        Emsg1(M_ERROR, 0, T_("Cannot find Storage resource %s\n"),
              res_dir->resource_name_);
        return false;
      } else {
        p->paired_storage = res_store->paired_storage;
        p->tls_cert_.allowed_certificate_common_names_
            = std::move(res_store->tls_cert_.allowed_certificate_common_names_);
        p->runtime_storage_status
            = GetRuntimeStatus<RuntimeStorageStatus>(p->resource_name_);
      }
      break;
    }
    case R_JOBDEFS:
    case R_JOB: {
      JobResource* p = dynamic_cast<JobResource*>(
          my_config->GetResWithName(type, res_job->resource_name_));
      if (!p) {
        Emsg1(M_ERROR, 0, T_("Cannot find Job resource %s\n"),
              res_job->resource_name_);
        return false;
      } else {
        p->messages = res_job->messages;
        p->schedule = res_job->schedule;
        p->client = res_job->client;
        p->fileset = res_job->fileset;
        p->storage = res_job->storage;
        p->catalog = res_job->catalog;
        p->FdPluginOptions = res_job->FdPluginOptions;
        p->SdPluginOptions = res_job->SdPluginOptions;
        p->DirPluginOptions = res_job->DirPluginOptions;
        p->base = res_job->base;
        p->pool = res_job->pool;
        p->full_pool = res_job->full_pool;
        p->vfull_pool = res_job->vfull_pool;
        p->inc_pool = res_job->inc_pool;
        p->diff_pool = res_job->diff_pool;
        p->next_pool = res_job->next_pool;
        p->verify_job = res_job->verify_job;
        p->jobdefs = res_job->jobdefs;
        p->run_cmds = res_job->run_cmds;
        p->RunScripts = res_job->RunScripts;
        if ((p->RunScripts) && (p->RunScripts->size() > 0)) {
          for (int i = 0; items[i].name; i++) {
            if (Bstrcasecmp(items[i].name, "RunScript")) {
              // SetBit(i, p->item_present_);
              ClearBit(i, p->inherit_content_);
            }
          }
        }

        Dmsg3(200, "job %s RunScript inherited: %i %i\n",
              res_job->resource_name_, BitIsSet(69, res_job->inherit_content_),
              BitIsSet(69, p->inherit_content_));

        /* TODO: JobDefs where/regexwhere doesn't work well (but this is not
         * very useful) We have to SetBit(index, item_present_); or
         * something like that
         *
         * We take RegexWhere before all other options */
        if (!p->RegexWhere
            && (p->strip_prefix || p->add_suffix || p->add_prefix)) {
          int len = BregexpGetBuildWhereSize(p->strip_prefix, p->add_prefix,
                                             p->add_suffix);
          p->RegexWhere = (char*)malloc(len * sizeof(char));
          bregexp_build_where(p->RegexWhere, len, p->strip_prefix,
                              p->add_prefix, p->add_suffix);
          // TODO: test bregexp
        }

        if (p->RegexWhere && p->RestoreWhere) {
          free(p->RestoreWhere);
          p->RestoreWhere = NULL;
        }

        p->rjs = GetRuntimeStatus<RuntimeJobStatus>(p->resource_name_);
      }
      break;
    }
    case R_COUNTER: {
      CounterResource* p = dynamic_cast<CounterResource*>(
          my_config->GetResWithName(type, res_counter->resource_name_));
      if (!p) {
        Emsg1(M_ERROR, 0, T_("Cannot find Counter resource %s\n"),
              res_counter->resource_name_);
        return false;
      } else {
        p->Catalog = res_counter->Catalog;
        p->WrapCounter = res_counter->WrapCounter;
      }
      break;
    }
    case R_CLIENT: {
      ClientResource* p = dynamic_cast<ClientResource*>(
          my_config->GetResWithName(type, res_client->resource_name_));
      if (!p) {
        Emsg1(M_ERROR, 0, T_("Cannot find Client resource %s\n"),
              res_client->resource_name_);
        return false;
      } else {
        if (res_client->catalog) {
          p->catalog = res_client->catalog;
        } else {
          // No catalog overwrite given use the first catalog definition.
          p->catalog = (CatalogResource*)my_config->GetNextRes(R_CATALOG, NULL);
        }
        p->tls_cert_.allowed_certificate_common_names_ = std::move(
            res_client->tls_cert_.allowed_certificate_common_names_);

        p->rcs = GetRuntimeStatus<RuntimeClientStatus>(p->resource_name_);
      }
      break;
    }
    case R_SCHEDULE: {
      /* Schedule is a bit different in that it contains a RunResource record
       * chain which isn't a "named" resource. This chain was linked
       * in by run_conf.c during pass 2, so here we jam the pointer
       * into the Schedule resource. */
      ScheduleResource* p = dynamic_cast<ScheduleResource*>(
          my_config->GetResWithName(type, res_sch->resource_name_));
      if (!p) {
        Emsg1(M_ERROR, 0, T_("Cannot find Schedule resource %s\n"),
              res_client->resource_name_);
        return false;
      } else {
        p->run = res_sch->run;
      }
      break;
    }
    default:
      Emsg1(M_ERROR, 0, T_("Unknown resource type %d in SaveResource.\n"),
            type);
      return false;
  }

  return true;
}

bool PropagateJobdefs(int res_type, JobResource* res)
{
  JobResource* jobdefs = NULL;

  if (!res->jobdefs) { return true; }

  // Don't allow the JobDefs pointing to itself.
  if (res->jobdefs == res) { return false; }

  if (res_type == R_JOB || res_type == R_JOBDEFS) {
    jobdefs = res->jobdefs;

    // Handle RunScripts alists specifically
    if (jobdefs->RunScripts) {
      if (!res->RunScripts) {
        res->RunScripts = new alist<RunScript*>(10, not_owned_by_alist);
      }

      for (auto* rs : jobdefs->RunScripts) {
        RunScript* r = DuplicateRunscript(rs);
        r->from_jobdef = true;
        res->RunScripts->append(r); /* free it at FreeResource */
      }
    }
  }

  // Transfer default items from JobDefs Resource
  PropagateResource(job_items, res->jobdefs, res);

  return true;
}

static bool PopulateJobdefaults()
{
  JobResource *job, *jobdefs;
  bool retval = true;

  // Propagate the content of a JobDefs to another.
  foreach_res (jobdefs, R_JOBDEFS) { PropagateJobdefs(R_JOBDEFS, jobdefs); }

  // Propagate the content of the JobDefs to the actual Job.
  foreach_res (job, R_JOB) {
    PropagateJobdefs(R_JOB, job);

    // Ensure that all required items are present
    if (!ValidateResource(R_JOB, job_items, job)) {
      retval = false;
      return retval;
    }

  } /* End loop over Job res */

  return retval;
}

bool PopulateDefs() { return PopulateJobdefaults(); }

static void StorePooltype(ConfigurationParser* p,
                          lexer* lc,
                          const ResourceItem* item,
                          int index,
                          int pass)
{
  LexGetToken(lc, BCT_NAME);
  if (pass == 1) {
    bool found = false;
    for (int i = 0; PoolTypes[i].name; i++) {
      if (Bstrcasecmp(lc->str(), PoolTypes[i].name)) {
        SetItemVariableFreeMemory<char*>(*item, strdup(PoolTypes[i].name));
        p->PushString(PoolTypes[i].name);
        found = true;
        break;
      }
    }

    if (!found) {
      scan_err1(lc, T_("Expected a Pool Type option, got: %s"), lc->str());
    }
  }

  ScanToEol(lc);
  item->SetPresent();
  ClearBit(index, (*item->allocated_resource)->inherit_content_);
}

static void ParsePooltype(ConfigurationParser* p,
                          lexer* lc,
                          const ResourceItem*,
                          int,
                          int)
{
  LexGetToken(lc, BCT_NAME);

  bool found = false;
  for (int i = 0; PoolTypes[i].name; i++) {
    if (Bstrcasecmp(lc->str(), PoolTypes[i].name)) {
      p->PushString(PoolTypes[i].name);
      found = true;
      break;
    }
  }

  if (!found) {
    scan_err1(lc, T_("Expected a Pool Type option, got: %s"), lc->str());
  }

  ScanToEol(lc);
}

static void StoreActiononpurge(ConfigurationParser* p,
                               lexer* lc,
                               const ResourceItem* item,
                               int index,
                               int)
{
  uint32_t* destination = GetItemVariablePointer<uint32_t*>(*item);

  LexGetToken(lc, BCT_NAME);
  /* Store the type both in pass 1 and pass 2
   * Scan ActionOnPurge options */
  bool found = false;
  for (int i = 0; ActionOnPurgeOptions[i].name; i++) {
    if (Bstrcasecmp(lc->str(), ActionOnPurgeOptions[i].name)) {
      *destination = (*destination) | ActionOnPurgeOptions[i].token;
      p->PushU(ActionOnPurgeOptions[i].token);
      found = true;
      break;
    }
  }

  if (!found) {
    scan_err1(lc, T_("Expected an Action On Purge option, got: %s"), lc->str());
  }

  ScanToEol(lc);
  item->SetPresent();
  ClearBit(index, (*item->allocated_resource)->inherit_content_);
}

static void ParseActiononpurge(ConfigurationParser* p,
                               lexer* lc,
                               const ResourceItem*,
                               int,
                               int)
{
  LexGetToken(lc, BCT_NAME);
  /* Store the type both in pass 1 and pass 2
   * Scan ActionOnPurge options */
  bool found = false;
  for (int i = 0; ActionOnPurgeOptions[i].name; i++) {
    if (Bstrcasecmp(lc->str(), ActionOnPurgeOptions[i].name)) {
      p->PushU(ActionOnPurgeOptions[i].token);
      found = true;
      break;
    }
  }

  if (!found) {
    scan_err1(lc, T_("Expected an Action On Purge option, got: %s"), lc->str());
  }

  ScanToEol(lc);
}

/**
 * Store Device. Note, the resource is created upon the
 * first reference. The details of the resource are obtained
 * later from the SD.
 */
static void StoreDevice(ConfigurationParser* p,
                        lexer* lc,
                        const ResourceItem* item,
                        int index,
                        int pass)
{
  LexGetToken(lc, BCT_NAME);

  if (pass == 1) {
    auto& devices = *GetItemVariablePointer<std::vector<Device>*>(*item);

    Dmsg4(900, "Add device %s to vector %p size=%" PRIuz " %s\n", lc->str(),
          &devices, devices.size(), item->name);
    p->PushString(lc->str());

    devices.emplace_back(lc->str());
  }

  ScanToEol(lc);
  item->SetPresent();
  ClearBit(index, (*item->allocated_resource)->inherit_content_);
}

static void ParseDevice(ConfigurationParser* p,
                        lexer* lc,
                        const ResourceItem* item,
                        int index,
                        int pass)
{
  StoreResource(p, CFG_TYPE_ALIST_RES, lc, item, index, pass);
}

// Store Migration/Copy type
static void StoreMigtype(ConfigurationParser* p,
                         lexer* lc,
                         const ResourceItem* item,
                         int index)
{
  LexGetToken(lc, BCT_NAME);
  // Store the type both in pass 1 and pass 2
  bool found = false;
  for (int i = 0; migtypes[i].name; i++) {
    if (Bstrcasecmp(lc->str(), migtypes[i].name)) {
      SetItemVariable<uint32_t>(*item, migtypes[i].job_type);
      p->PushU(migtypes[i].job_type);
      found = true;
      break;
    }
  }

  if (!found) {
    scan_err1(lc, T_("Expected a Migration Job Type keyword, got: %s"),
              lc->str());
  }

  ScanToEol(lc);
  item->SetPresent();
  ClearBit(index, (*item->allocated_resource)->inherit_content_);
}

static void ParseMigtype(ConfigurationParser* p,
                         lexer* lc,
                         const ResourceItem*,
                         int)
{
  LexGetToken(lc, BCT_NAME);
  // Store the type both in pass 1 and pass 2
  bool found = false;
  for (int i = 0; migtypes[i].name; i++) {
    if (Bstrcasecmp(lc->str(), migtypes[i].name)) {
      p->PushU(migtypes[i].job_type);
      found = true;
      break;
    }
  }

  if (!found) {
    scan_err1(lc, T_("Expected a Migration Job Type keyword, got: %s"),
              lc->str());
  }

  ScanToEol(lc);
}

// Store JobType (backup, verify, restore)
static void StoreJobtype(ConfigurationParser* p,
                         lexer* lc,
                         const ResourceItem* item,
                         int index,
                         int)
{
  // Store the type both in pass 1 and pass 2
  auto found = ReadKeyword(p, lc, jobtypes);

  if (!found) {
    scan_err1(lc, T_("Expected a Restore replacement option, got: %s"),
              lc->str());
  } else {
    SetItemVariable<uint32_t>(*item, found->job_type);
  }

  ScanToEol(lc);
  item->SetPresent();
  ClearBit(index, (*item->allocated_resource)->inherit_content_);
}

static void ParseJobtype(ConfigurationParser* p,
                         lexer* lc,
                         const ResourceItem*,
                         int,
                         int)
{
  LexGetToken(lc, BCT_NAME);
  // Store the type both in pass 1 and pass 2
  bool found = false;
  for (int i = 0; jobtypes[i].name; i++) {
    if (Bstrcasecmp(lc->str(), jobtypes[i].name)) {
      p->PushU(jobtypes[i].job_type);
      found = true;
      break;
    }
  }

  if (!found) {
    scan_err1(lc, T_("Expected a Job Type keyword, got: %s"), lc->str());
  }

  ScanToEol(lc);
}

// Store Protocol (Native, NDMP/NDMP_BAREOS, NDMP_NATIVE)
static void StoreProtocoltype(ConfigurationParser* p,
                              lexer* lc,
                              const ResourceItem* item,
                              int index,
                              int)
{
  auto found = ReadKeyword(p, lc, backupprotocols);

  if (!found) {
    scan_err1(lc, T_("Expected a Restore replacement option, got: %s"),
              lc->str());
  } else {
    SetItemVariable<uint32_t>(*item, found->token);
  }

  ScanToEol(lc);
  item->SetPresent();
  ClearBit(index, (*item->allocated_resource)->inherit_content_);
}

static void ParseProtocoltype(ConfigurationParser* p,
                              lexer* lc,
                              const ResourceItem*,
                              int,
                              int)
{
  LexGetToken(lc, BCT_NAME);
  // Store the type both in pass 1 and pass 2
  bool found = false;
  for (int i = 0; backupprotocols[i].name; i++) {
    if (Bstrcasecmp(lc->str(), backupprotocols[i].name)) {
      p->PushU(backupprotocols[i].token);
      found = true;
      break;
    }
  }

  if (!found) {
    scan_err1(lc, T_("Expected a Protocol Type keyword, got: %s"), lc->str());
  }

  ScanToEol(lc);
}

static void StoreReplace(ConfigurationParser* p,
                         lexer* lc,
                         const ResourceItem* item,
                         int index,
                         int)
{
  auto found = ReadKeyword(p, lc, ReplaceOptions);

  if (!found) {
    scan_err1(lc, T_("Expected a Restore replacement option, got: %s"),
              lc->str());
  } else {
    SetItemVariable<uint32_t>(*item, found->token);
  }

  ScanToEol(lc);
  item->SetPresent();
  ClearBit(index, (*item->allocated_resource)->inherit_content_);
}

static void ParseReplace(ConfigurationParser* p,
                         lexer* lc,
                         const ResourceItem*,
                         int,
                         int)
{
  auto found = ReadKeyword(p, lc, ReplaceOptions);

  if (!found) {
    scan_err1(lc, T_("Expected a Restore replacement option, got: %s"),
              lc->str());
  }

  ScanToEol(lc);
}

// Store Auth Protocol (Native, NDMPv2, NDMPv3, NDMPv4)
static void StoreAuthprotocoltype(ConfigurationParser* p,
                                  lexer* lc,
                                  const ResourceItem* item,
                                  int index,
                                  int)
{
  auto found = ReadKeyword(p, lc, authprotocols);

  if (!found) {
    scan_err1(lc, T_("Expected a Auth Protocol Type keyword, got: %s"),
              lc->str());
  } else {
    SetItemVariable<uint32_t>(*item, found->token);
  }

  ScanToEol(lc);
  item->SetPresent();
  ClearBit(index, (*item->allocated_resource)->inherit_content_);
}

static void ParseAuthprotocoltype(ConfigurationParser* p,
                                  lexer* lc,
                                  const ResourceItem*,
                                  int,
                                  int)
{
  auto found = ReadKeyword(p, lc, authprotocols);

  if (!found) {
    scan_err1(lc, T_("Expected a Auth Protocol Type keyword, got: %s"),
              lc->str());
  }

  ScanToEol(lc);
}

// Store authentication type (Mostly for NDMP like clear or MD5).
static void StoreAuthtype(ConfigurationParser* p,
                          lexer* lc,
                          const ResourceItem* item,
                          int index,
                          int)
{
  auto* found = ReadKeyword(p, lc, authmethods);
  if (!found) {
    scan_err1(lc, T_("Expected a Authentication Type keyword, got: %s"),
              lc->str());
  } else {
    SetItemVariable<uint32_t>(*item, found->token);
  }

  ScanToEol(lc);
  item->SetPresent();
  ClearBit(index, (*item->allocated_resource)->inherit_content_);
}

static void ParseAuthtype(ConfigurationParser* p,
                          lexer* lc,
                          const ResourceItem*,
                          int,
                          int)
{
  auto* found = ReadKeyword(p, lc, authmethods);

  if (!found) {
    scan_err1(lc, T_("Expected a Authentication Type keyword, got: %s"),
              lc->str());
  }

  ScanToEol(lc);
}

// Store Job Level (Full, Incremental, ...)
static void StoreLevel(ConfigurationParser* p,
                       lexer* lc,
                       const ResourceItem* item,
                       int index,
                       int)
{
  auto* found = ReadKeyword(p, lc, joblevels);
  // Store the level in pass 2 so that type is defined

  if (!found) {
    scan_err1(lc, T_("Expected a Job Level keyword, got: %s"), lc->str());
  } else {
    SetItemVariable<uint32_t>(*item, found->level);
  }

  ScanToEol(lc);
  item->SetPresent();
  ClearBit(index, (*item->allocated_resource)->inherit_content_);
}

static void ParseLevel(ConfigurationParser* p,
                       lexer* lc,
                       const ResourceItem*,
                       int,
                       int)
{
  auto* found = ReadKeyword(p, lc, joblevels);

  if (!found) {
    scan_err1(lc, T_("Expected a Job Level keyword, got: %s"), lc->str());
  }

  ScanToEol(lc);
}

/**
 * Store password either clear if for NDMP and catalog or MD5 hashed for
 * native.
 */
static void StoreAutopassword(ConfigurationParser* p,
                              lexer* lc,
                              const ResourceItem* item,
                              int index,
                              int pass)
{
  switch ((*item->allocated_resource)->rcode_) {
    case R_DIRECTOR:
      /* As we need to store both clear and MD5 hashed within the same
       * resource class we use the item->code as a hint default is 0
       * and for clear we need a code of 1. */
      switch (item->code) {
        case 1:
          StoreResource(p, CFG_TYPE_CLEARPASSWORD, lc, item, index, pass);
          break;
        default:
          StoreResource(p, CFG_TYPE_MD5PASSWORD, lc, item, index, pass);
          break;
      }
      break;
    case R_CLIENT:
      if (pass == 2) {
        auto* res = dynamic_cast<ClientResource*>(p->GetResWithName(
            R_CLIENT, (*item->allocated_resource)->resource_name_));
        ASSERT(res);

        if (res_client->Protocol != res->Protocol) {
          scan_err1(lc,
                    "Trying to store password to resource \"%s\", but protocol "
                    "is not known.\n",
                    (*item->allocated_resource)->resource_name_);
        }
      }
      switch (res_client->Protocol) {
        case APT_NDMPV2:
        case APT_NDMPV3:
        case APT_NDMPV4:
          StoreResource(p, CFG_TYPE_CLEARPASSWORD, lc, item, index, pass);
          break;
        default:
          StoreResource(p, CFG_TYPE_MD5PASSWORD, lc, item, index, pass);
          break;
      }
      break;
    case R_STORAGE:
      if (pass == 2) {
        auto* res = dynamic_cast<StorageResource*>(p->GetResWithName(
            R_STORAGE, (*item->allocated_resource)->resource_name_));
        ASSERT(res);

        if (res_store->Protocol != res->Protocol) {
          scan_err1(lc,
                    "Trying to store password to resource \"%s\", but protocol "
                    "is not known.\n",
                    (*item->allocated_resource)->resource_name_);
        }
      }
      switch (res_store->Protocol) {
        case APT_NDMPV2:
        case APT_NDMPV3:
        case APT_NDMPV4:
          StoreResource(p, CFG_TYPE_CLEARPASSWORD, lc, item, index, pass);
          break;
        default:
          StoreResource(p, CFG_TYPE_MD5PASSWORD, lc, item, index, pass);
          break;
      }
      break;
    case R_CATALOG:
      StoreResource(p, CFG_TYPE_CLEARPASSWORD, lc, item, index, pass);
      break;
    default:
      StoreResource(p, CFG_TYPE_MD5PASSWORD, lc, item, index, pass);
      break;
  }
}

static void ParseAutopassword(ConfigurationParser* p,
                              lexer* lc,
                              const ResourceItem* item,
                              int index,
                              int pass)
{
  switch ((*item->allocated_resource)->rcode_) {
    case R_DIRECTOR:
      /* As we need to store both clear and MD5 hashed within the same
       * resource class we use the item->code as a hint default is 0
       * and for clear we need a code of 1. */
      switch (item->code) {
        case 1:
          ParseResource(p, CFG_TYPE_CLEARPASSWORD, lc, item, index, pass);
          break;
        default:
          ParseResource(p, CFG_TYPE_MD5PASSWORD, lc, item, index, pass);
          break;
      }
      break;
    case R_CLIENT:
      if (pass == 2) {
        auto* res = dynamic_cast<ClientResource*>(p->GetResWithName(
            R_CLIENT, (*item->allocated_resource)->resource_name_));
        ASSERT(res);

        if (res_client->Protocol != res->Protocol) {
          scan_err1(lc,
                    "Trying to store password to resource \"%s\", but protocol "
                    "is not known.\n",
                    (*item->allocated_resource)->resource_name_);
        }
      }
      switch (res_client->Protocol) {
        case APT_NDMPV2:
        case APT_NDMPV3:
        case APT_NDMPV4:
          ParseResource(p, CFG_TYPE_CLEARPASSWORD, lc, item, index, pass);
          break;
        default:
          ParseResource(p, CFG_TYPE_MD5PASSWORD, lc, item, index, pass);
          break;
      }
      break;
    case R_STORAGE:
      if (pass == 2) {
        auto* res = dynamic_cast<StorageResource*>(p->GetResWithName(
            R_STORAGE, (*item->allocated_resource)->resource_name_));
        ASSERT(res);

        if (res_store->Protocol != res->Protocol) {
          scan_err1(lc,
                    "Trying to store password to resource \"%s\", but protocol "
                    "is not known.\n",
                    (*item->allocated_resource)->resource_name_);
        }
      }
      switch (res_store->Protocol) {
        case APT_NDMPV2:
        case APT_NDMPV3:
        case APT_NDMPV4:
          ParseResource(p, CFG_TYPE_CLEARPASSWORD, lc, item, index, pass);
          break;
        default:
          ParseResource(p, CFG_TYPE_MD5PASSWORD, lc, item, index, pass);
          break;
      }
      break;
    case R_CATALOG:
      ParseResource(p, CFG_TYPE_CLEARPASSWORD, lc, item, index, pass);
      break;
    default:
      ParseResource(p, CFG_TYPE_MD5PASSWORD, lc, item, index, pass);
      break;
  }
}

static void StoreAcl(ConfigurationParser* p,
                     lexer* lc,
                     const ResourceItem* item,
                     int index,
                     int pass)
{
  alist<const char*>** alistvalue
      = GetItemVariablePointer<alist<const char*>**>(*item);
  if (pass == 1) {
    if (!alistvalue[item->code]) {
      alistvalue[item->code] = new alist<const char*>(10, owned_by_alist);
      Dmsg1(900, "Defined new ACL alist at %d\n", item->code);
    }
  }
  alist<const char*>* list = alistvalue[item->code];
  PoolMem msg;
  int token = BCT_COMMA;

  p->PushMergeArray();
  while (token == BCT_COMMA) {
    LexGetToken(lc, BCT_STRING);
    p->PushString(lc->str());
    if (pass == 1) {
      if (!IsAclEntryValid(lc->str(), msg)) {
        scan_err1(lc, T_("Cannot store Acl: %s"), msg.c_str());
        return;
      }
      list->append(strdup(lc->str()));
      Dmsg2(900, "Appended to %d %s\n", item->code, lc->str());
    }
    token = LexGetToken(lc, BCT_ALL);
  }
  p->PopArray();
  item->SetPresent();
  ClearBit(index, (*item->allocated_resource)->inherit_content_);
}

static void ParseAcl(ConfigurationParser* p,
                     lexer* lc,
                     const ResourceItem*,
                     int,
                     int)
{
  int token = BCT_COMMA;

  p->PushArray();
  while (token == BCT_COMMA) {
    LexGetToken(lc, BCT_STRING);
    p->PushString(lc->str());
    token = LexGetToken(lc, BCT_ALL);
  }
  p->PopArray();
}

static void StoreAudit(ConfigurationParser* p,
                       lexer* lc,
                       const ResourceItem* item,
                       int index,
                       int pass)
{
  int token;
  alist<const char*>* list;

  alist<const char*>** alistvalue
      = GetItemVariablePointer<alist<const char*>**>(*item);

  if (pass == 1) {
    if (!*alistvalue) {
      *alistvalue = new alist<const char*>(10, owned_by_alist);
    }
  }
  list = *alistvalue;

  p->PushArray();
  for (;;) {
    LexGetToken(lc, BCT_STRING);
    p->PushString(lc->str());
    if (pass == 1) { list->append(strdup(lc->str())); }
    token = LexGetToken(lc, BCT_ALL);
    if (token == BCT_COMMA) { continue; }
    break;
  }
  p->PopArray();
  item->SetPresent();
  ClearBit(index, (*item->allocated_resource)->inherit_content_);
}

static void ParseAudit(ConfigurationParser* p,
                       lexer* lc,
                       const ResourceItem*,
                       int,
                       int)
{
  p->PushArray();

  int token = BCT_COMMA;
  while (token == BCT_COMMA) {
    LexGetToken(lc, BCT_STRING);
    p->PushString(lc->str());
    token = LexGetToken(lc, BCT_ALL);
  }
  p->PopArray();
}

static constexpr s_kw script_timing[] = {
    {"before", SCRIPT_Before},
    {"after", SCRIPT_After},
    {"aftervss", SCRIPT_AfterVSS},
    {"always", SCRIPT_Any},
};

static void StoreRunscriptWhen(ConfigurationParser* p,
                               lexer* lc,
                               const ResourceItem* item,
                               int)
{
  auto* found = ReadKeyword(p, lc, script_timing);
  if (!found) {
    scan_err2(lc, T_("Expect %s, got: %s"), "Before, After, AfterVSS or Always",
              lc->str());
  } else {
    SetItemVariable<uint32_t>(*item, found->token);
  }
  ScanToEol(lc);
}

static void ParseRunscriptWhen(ConfigurationParser* p, lexer* lc)
{
  LexGetToken(lc, BCT_NAME);
  p->PushString(lc->str());
  ScanToEol(lc);
}

static void StoreRunscriptTarget(ConfigurationParser* p,
                                 lexer* lc,
                                 const ResourceItem* item,
                                 int pass)
{
  LexGetToken(lc, BCT_STRING);
  p->PushString(lc->str());

  if (pass == 2) {
    RunScript* r = GetItemVariablePointer<RunScript*>(*item);
    if (bstrcmp(lc->str(), "%c")) {
      r->SetTarget(lc->str());
    } else if (Bstrcasecmp(lc->str(), "yes")) {
      r->SetTarget("%c");
    } else if (Bstrcasecmp(lc->str(), "no")) {
      r->SetTarget("");
    } else {
      BareosResource* res;

      if (!(res = p->GetResWithName(R_CLIENT, lc->str()))) {
        scan_err3(lc,
                  T_("Could not find config Resource %s referenced on line %d "
                     ": %s\n"),
                  lc->str(), lc->line_no(), lc->line());
      }

      r->SetTarget(lc->str());
    }
  }
  ScanToEol(lc);
}

static void ParseRunscriptTarget(ConfigurationParser* p, lexer* lc)
{
  auto* found = ReadKeyword(p, lc, script_timing);
  if (!found) {
    scan_err2(lc, T_("Expect %s, got: %s"), "Before, After, AfterVSS or Always",
              lc->str());
  }
  ScanToEol(lc);
}

static void StoreRunscriptCmd(ConfigurationParser* p,
                              lexer* lc,
                              const ResourceItem* item,
                              int pass)
{
  LexGetToken(lc, BCT_STRING);

  // multiple commands are allowed
  p->PushMergeArray();
  p->PushString(lc->str());
  p->PopArray();

  if (pass == 2) {
    Dmsg2(100, "runscript cmd=%s type=%c\n", lc->str(), item->code);
    RunScript* r = GetItemVariablePointer<RunScript*>(*item);
    r->temp_parser_command_container.emplace_back(lc->str(), item->code);
  }
  ScanToEol(lc);
}

static void ParseRunscriptCmd(ConfigurationParser* p, lexer* lc)
{
  LexGetToken(lc, BCT_STRING);
  p->PushString(lc->str());
  ScanToEol(lc);
}

static void StoreShortRunscript(ConfigurationParser* p,
                                lexer* lc,
                                const ResourceItem* item,
                                int,
                                int pass)
{
  LexGetToken(lc, BCT_STRING);
  alist<RunScript*>** runscripts
      = GetItemVariablePointer<alist<RunScript*>**>(*item);

  Dmsg0(500, "runscript: creating new RunScript object\n");
  RunScript* script = nullptr;
  if (pass == 2) {
    script = new RunScript;
    script->SetJobCodeCallback(job_code_callback_director);
    script->SetCommand(lc->str());
  }

  p->PushAlias("RunScript");
  p->PushMergeArray();
  p->PushObject();
  p->PushLabel("command");
  p->PushString(lc->str());

  auto set_bools = [&](bool on_success, bool on_error, bool fail_on_error) {
    p->PushLabel("on_success");
    p->PushB(on_success);
    p->PushLabel("on_failure");
    p->PushB(on_error);
    p->PushLabel("fail_on_error");
    p->PushB(fail_on_error);
  };

  if (Bstrcasecmp(item->name, "runbeforejob")) {
    p->PushLabel("target");
    p->PushString("");
    p->PushLabel("when");
    p->PushString("before");
    if (pass == 2) {
      script->when = SCRIPT_Before;
      script->SetTarget("");
    }
  } else if (Bstrcasecmp(item->name, "runafterjob")) {
    p->PushLabel("target");
    p->PushString("");
    p->PushLabel("when");
    p->PushString("after");
    set_bools(true, false, false);
    if (pass == 2) {
      script->when = SCRIPT_After;
      script->on_success = true;
      script->on_failure = false;
      script->fail_on_error = false;
      script->SetTarget("");
    }
  } else if (Bstrcasecmp(item->name, "clientrunafterjob")) {
    p->PushLabel("target");
    p->PushString("%c");
    p->PushLabel("when");
    p->PushString("after");
    set_bools(true, false, false);
    if (pass == 2) {
      script->when = SCRIPT_After;
      script->on_success = true;
      script->on_failure = false;
      script->fail_on_error = false;
      script->SetTarget("%c");
    }
  } else if (Bstrcasecmp(item->name, "clientrunbeforejob")) {
    p->PushLabel("target");
    p->PushString("%c");
    p->PushLabel("when");
    p->PushString("before");
    if (pass == 2) {
      script->when = SCRIPT_Before;
      script->SetTarget("%c");
    }
  } else if (Bstrcasecmp(item->name, "runafterfailedjob")) {
    p->PushLabel("target");
    p->PushString("");
    p->PushLabel("when");
    p->PushString("after");
    set_bools(false, true, false);
    if (pass == 2) {
      script->when = SCRIPT_After;
      script->on_success = false;
      script->on_failure = true;
      script->fail_on_error = false;
      script->SetTarget("");
    }
  }

  p->PopObject();
  p->PopArray();

  if (pass == 2) {
    // Remember that the entry was configured in the short runscript form.
    script->short_form = true;

    if (!*runscripts) {
      *runscripts = new alist<RunScript*>(10, not_owned_by_alist);
    }

    (*runscripts)->append(script);
    script->Debug();
  }

  ScanToEol(lc);
}

static void ParseShortRunscript(ConfigurationParser* p,
                                lexer* lc,
                                const ResourceItem* item,
                                int,
                                int)
{
  LexGetToken(lc, BCT_STRING);

  Dmsg0(500, "runscript: creating new RunScript object\n");

  p->PushAlias("RunScript");
  p->PushObject();
  p->PushLabel("command");
  p->PushString(lc->str());

  auto set_bools = [&](bool on_success, bool on_error, bool fail_on_error) {
    p->PushLabel("on_success");
    p->PushB(on_success);
    p->PushLabel("on_failure");
    p->PushB(on_error);
    p->PushLabel("fail_on_error");
    p->PushB(fail_on_error);
  };

  if (Bstrcasecmp(item->name, "runbeforejob")) {
    p->PushLabel("target");
    p->PushString("");
    p->PushLabel("when");
    p->PushString("before");
  } else if (Bstrcasecmp(item->name, "runafterjob")) {
    p->PushLabel("target");
    p->PushString("");
    p->PushLabel("when");
    p->PushString("after");
    set_bools(true, false, false);
  } else if (Bstrcasecmp(item->name, "clientrunafterjob")) {
    p->PushLabel("target");
    p->PushString("%c");
    p->PushLabel("when");
    p->PushString("after");
    set_bools(true, false, false);
  } else if (Bstrcasecmp(item->name, "clientrunbeforejob")) {
    p->PushLabel("target");
    p->PushString("%c");
    p->PushLabel("when");
    p->PushString("before");
  } else if (Bstrcasecmp(item->name, "runafterfailedjob")) {
    p->PushLabel("target");
    p->PushString("");
    p->PushLabel("when");
    p->PushString("after");
    set_bools(false, true, false);
  }

  p->PopObject();

  ScanToEol(lc);
}

/**
 * Store a bool in a bit field without modifing hdr
 * We can also add an option to StoreBool to skip hdr
 */
static void StoreRunscriptBool(ConfigurationParser* p,
                               lexer* lc,
                               const ResourceItem* item,
                               int)
{
  auto* found = ReadKeyword(p, lc, bool_kw);

  if (!found) {
    scan_err2(lc, T_("Expect %s, got: %s"), "YES, NO, TRUE, or FALSE",
              lc->str()); /* YES and NO must not be translated */
  }

  SetItemVariable<bool>(*item, found->token != 0);

  ScanToEol(lc);
}

static void ParseRunscriptBool(ConfigurationParser* p, lexer* lc)
{
  auto* found = ReadKeyword(p, lc, bool_kw);

  if (!found) {
    scan_err2(lc, T_("Expect %s, got: %s"), "YES, NO, TRUE, or FALSE",
              lc->str()); /* YES and NO must not be translated */
  }

  ScanToEol(lc);
}

/**
 * Store RunScript info
 *
 * Note, when this routine is called, we are inside a Job
 * resource.  We treat the RunScript like a sort of
 * mini-resource within the Job resource.
 */
static void StoreRunscript(ConfigurationParser* p,
                           lexer* lc,
                           const ResourceItem* item,
                           int index,
                           int pass)
{
  Dmsg1(200, "StoreRunscript: begin StoreRunscript pass=%i\n", pass);

  int token = LexGetToken(lc, BCT_SKIP_EOL);

  if (token != BCT_BOB) {
    scan_err1(lc, T_("Expecting open brace. Got %s"), lc->str());
    return;
  }

  res_runscript = new RunScript();

  /* Run on client by default.
   * Set this here, instead of in the class constructor,
   * as the class is also used by other daemon,
   * where the default differs. */
  if (res_runscript->target.empty()) { res_runscript->SetTarget("%c"); }

  p->PushMergeArray();
  p->PushObject();
  while ((token = LexGetToken(lc, BCT_SKIP_EOL)) != BCT_EOF) {
    if (token == BCT_EOB) { break; }

    if (token != BCT_IDENTIFIER) {
      scan_err1(lc, T_("Expecting keyword, got: %s\n"), lc->str());
      goto bail_out;
    }

    bool keyword_ok = false;
    for (int i = 0; runscript_items[i].name; i++) {
      if (Bstrcasecmp(runscript_items[i].name, lc->str())) {
        p->PushLabel(runscript_items[i].name);
        token = LexGetToken(lc, BCT_SKIP_EOL);
        if (token != BCT_EQUALS) {
          scan_err1(lc, T_("Expected an equals, got: %s"), lc->str());
          goto bail_out;
        }
        switch (runscript_items[i].type) {
          case CFG_TYPE_RUNSCRIPT_CMD:
            StoreRunscriptCmd(p, lc, &runscript_items[i], pass);
            break;
          case CFG_TYPE_RUNSCRIPT_TARGET:
            StoreRunscriptTarget(p, lc, &runscript_items[i], pass);
            break;
          case CFG_TYPE_RUNSCRIPT_BOOL:
            StoreRunscriptBool(p, lc, &runscript_items[i], pass);
            break;
          case CFG_TYPE_RUNSCRIPT_WHEN:
            StoreRunscriptWhen(p, lc, &runscript_items[i], pass);
            break;
          default:
            break;
        }
        keyword_ok = true;
        break;
      }
    }

    if (!keyword_ok) {
      scan_err1(lc, T_("Keyword %s not permitted in this resource"), lc->str());
      goto bail_out;
    }
  }
  p->PopObject();
  p->PopArray();

  if (pass == 2) {
    alist<RunScript*>** runscripts
        = GetItemVariablePointer<alist<RunScript*>**>(*item);
    if (!*runscripts) {
      *runscripts = new alist<RunScript*>(10, not_owned_by_alist);
    }

    res_runscript->SetJobCodeCallback(job_code_callback_director);

    for (TempParserCommand cmd : res_runscript->temp_parser_command_container) {
      RunScript* script = new RunScript(*res_runscript);
      script->command = cmd.command_;
      script->cmd_type = cmd.code_;

      // each shell runscript object have a copy of target.
      // console runscripts are always executed on the Director.
      script->target.clear();
      if (!res_runscript->target.empty() && (script->cmd_type == SHELL_CMD)) {
        script->SetTarget(res_runscript->target);
      }

      script->short_form = false;

      (*runscripts)->append(script);
      script->Debug();
    }
  }

bail_out:
  /* for pass == 1 only delete the memory
     because it is only used while parsing */
  delete res_runscript;
  res_runscript = nullptr;

  ScanToEol(lc);
  item->SetPresent();
  ClearBit(index, (*item->allocated_resource)->inherit_content_);
}

static void ParseRunscript(ConfigurationParser* p,
                           lexer* lc,
                           const ResourceItem*,
                           int,
                           int)
{
  int token = LexGetToken(lc, BCT_SKIP_EOL);

  if (token != BCT_BOB) {
    scan_err1(lc, T_("Expecting open brace. Got %s"), lc->str());
    return;
  }

  p->PushObject();
  while ((token = LexGetToken(lc, BCT_SKIP_EOL)) != BCT_EOF) {
    if (token == BCT_EOB) { break; }

    if (token != BCT_IDENTIFIER) {
      scan_err1(lc, T_("Expecting keyword, got: %s\n"), lc->str());
      goto bail_out;
    }

    bool keyword_ok = false;
    for (int i = 0; runscript_items[i].name; i++) {
      if (Bstrcasecmp(runscript_items[i].name, lc->str())) {
        p->PushLabel(runscript_items[i].name);
        token = LexGetToken(lc, BCT_SKIP_EOL);
        if (token != BCT_EQUALS) {
          scan_err1(lc, T_("Expected an equals, got: %s"), lc->str());
          goto bail_out;
        }
        switch (runscript_items[i].type) {
          case CFG_TYPE_RUNSCRIPT_CMD:
            ParseRunscriptCmd(p, lc);
            break;
          case CFG_TYPE_RUNSCRIPT_TARGET:
            ParseRunscriptTarget(p, lc);
            break;
          case CFG_TYPE_RUNSCRIPT_BOOL:
            ParseRunscriptBool(p, lc);
            break;
          case CFG_TYPE_RUNSCRIPT_WHEN:
            ParseRunscriptWhen(p, lc);
            break;
          default:
            break;
        }
        keyword_ok = true;
        break;
      }
    }

    if (!keyword_ok) {
      scan_err1(lc, T_("Keyword %s not permitted in this resource"), lc->str());
      goto bail_out;
    }
  }
  p->PopObject();

bail_out:
  ScanToEol(lc);
}

/**
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
 * %O = Prev Backup JobId
 * %N = New Backup JobId
 */
std::optional<std::string> job_code_callback_director(JobControlRecord* jcr,
                                                      const char* param)
{
  static const std::string yes = "yes";
  static const std::string no = "no";
  static const std::string none = "*None*";

  switch (param[0]) {
    case 'f':
      if (jcr->dir_impl->res.fileset) {
        return jcr->dir_impl->res.fileset->resource_name_;
      }
      break;
    case 'h':
      if (jcr->dir_impl->res.client) {
        return jcr->dir_impl->res.client->address;
      }
      break;
    case 'p':
      if (jcr->dir_impl->res.pool) {
        return jcr->dir_impl->res.pool->resource_name_;
      }
      break;
    case 'w':
      if (jcr->dir_impl->res.write_storage) {
        return jcr->dir_impl->res.write_storage->resource_name_;
      }
      break;
    case 'x':
      return jcr->dir_impl->spool_data ? yes : no;
    case 'C':
      return jcr->dir_impl->cloned ? yes : no;
    case 'D':
      return my_name;
    case 'V':
      if (jcr) {
        /* If this is a migration/copy we need the volume name from the
         * mig_jcr. */
        if (jcr->dir_impl->mig_jcr) { jcr = jcr->dir_impl->mig_jcr; }

        if (jcr->VolumeName) {
          return jcr->VolumeName;
        } else {
          return none;
        }
      } else {
        return none;
      }
      break;
    case 'O': /* Migration/copy job prev jobid */
      if (jcr && jcr->dir_impl && jcr->dir_impl->previous_jr) {
        return std::to_string(jcr->dir_impl->previous_jr->JobId);
      } else {
        return none;
      }
      break;
    case 'N': /* Migration/copy job new jobid */
      if (jcr && jcr->dir_impl && jcr->dir_impl->mig_jcr
          && jcr->dir_impl->mig_jcr->dir_impl
          && jcr->dir_impl->mig_jcr->dir_impl->jr.JobId) {
        return std::to_string(jcr->dir_impl->mig_jcr->dir_impl->jr.JobId);
      } else {
        return none;
      }
      break;
  }

  return std::nullopt;
}

/**
 * callback function for init_resource
 * See ../lib/parse_conf.cc, function InitResource, for more generic handling.
 */
static void InitResourceCb(const ResourceItem* item, int pass)
{
  switch (pass) {
    case 1:
      switch (item->type) {
        case CFG_TYPE_REPLACE:
          for (int i = 0; ReplaceOptions[i].name; i++) {
            if (Bstrcasecmp(item->default_value, ReplaceOptions[i].name)) {
              SetItemVariable<uint32_t>(*item, ReplaceOptions[i].token);
            }
          }
          break;
        case CFG_TYPE_AUTHPROTOCOLTYPE:
          for (int i = 0; authprotocols[i].name; i++) {
            if (Bstrcasecmp(item->default_value, authprotocols[i].name)) {
              SetItemVariable<uint32_t>(*item, authprotocols[i].token);
            }
          }
          break;
        case CFG_TYPE_AUTHTYPE:
          for (int i = 0; authmethods[i].name; i++) {
            if (Bstrcasecmp(item->default_value, authmethods[i].name)) {
              SetItemVariable<uint32_t>(*item, authmethods[i].token);
            }
          }
          break;
        case CFG_TYPE_POOLTYPE:
          SetItemVariable<char*>(*item, strdup(item->default_value));
          break;
        default:
          break;
      }
      break;
    default:
      break;
  }
}

/**
 * callback function for parse_config
 * See ../lib/parse_conf.c, function ParseConfig, for more generic handling.
 */
static void StoreConfigCb(ConfigurationParser* p,
                          lexer* lc,
                          const ResourceItem* item,
                          int index,
                          int pass,
                          BareosResource**)
{
  switch (item->type) {
    case CFG_TYPE_AUTOPASSWORD:
      StoreAutopassword(p, lc, item, index, pass);
      break;
    case CFG_TYPE_ACL:
      StoreAcl(p, lc, item, index, pass);
      break;
    case CFG_TYPE_AUDIT:
      StoreAudit(p, lc, item, index, pass);
      break;
    case CFG_TYPE_AUTHPROTOCOLTYPE:
      StoreAuthprotocoltype(p, lc, item, index, pass);
      break;
    case CFG_TYPE_AUTHTYPE:
      StoreAuthtype(p, lc, item, index, pass);
      break;
    case CFG_TYPE_DEVICE:
      StoreDevice(p, lc, item, index, pass);
      break;
    case CFG_TYPE_JOBTYPE:
      StoreJobtype(p, lc, item, index, pass);
      break;
    case CFG_TYPE_PROTOCOLTYPE:
      StoreProtocoltype(p, lc, item, index, pass);
      break;
    case CFG_TYPE_LEVEL:
      StoreLevel(p, lc, item, index, pass);
      break;
    case CFG_TYPE_REPLACE:
      StoreReplace(p, lc, item, index, pass);
      break;
    case CFG_TYPE_SHRTRUNSCRIPT:
      StoreShortRunscript(p, lc, item, index, pass);
      break;
    case CFG_TYPE_RUNSCRIPT:
      StoreRunscript(p, lc, item, index, pass);
      break;
    case CFG_TYPE_MIGTYPE:
      StoreMigtype(p, lc, item, index);
      break;
    case CFG_TYPE_INCEXC:
      StoreInc(p, lc, item, index, pass);
      break;
    case CFG_TYPE_RUN:
      StoreRun(p, lc, item, index, pass);
      break;
    case CFG_TYPE_ACTIONONPURGE:
      StoreActiononpurge(p, lc, item, index, pass);
      break;
    case CFG_TYPE_POOLTYPE:
      StorePooltype(p, lc, item, index, pass);
      break;
    default:
      break;
  }
}

[[maybe_unused]] static void ParseConfigCb(ConfigurationParser* p,
                                           lexer* lc,
                                           const ResourceItem* item,
                                           int index,
                                           int pass)
{
  switch (item->type) {
    case CFG_TYPE_AUTOPASSWORD:
      ParseAutopassword(p, lc, item, index, pass);
      break;
    case CFG_TYPE_ACL:
      ParseAcl(p, lc, item, index, pass);
      break;
    case CFG_TYPE_AUDIT:
      ParseAudit(p, lc, item, index, pass);
      break;
    case CFG_TYPE_AUTHPROTOCOLTYPE:
      ParseAuthprotocoltype(p, lc, item, index, pass);
      break;
    case CFG_TYPE_AUTHTYPE:
      ParseAuthtype(p, lc, item, index, pass);
      break;
    case CFG_TYPE_DEVICE:
      ParseDevice(p, lc, item, index, pass);
      break;
    case CFG_TYPE_JOBTYPE:
      ParseJobtype(p, lc, item, index, pass);
      break;
    case CFG_TYPE_PROTOCOLTYPE:
      ParseProtocoltype(p, lc, item, index, pass);
      break;
    case CFG_TYPE_LEVEL:
      ParseLevel(p, lc, item, index, pass);
      break;
    case CFG_TYPE_REPLACE:
      ParseReplace(p, lc, item, index, pass);
      break;
    case CFG_TYPE_SHRTRUNSCRIPT:
      ParseShortRunscript(p, lc, item, index, pass);
      break;
    case CFG_TYPE_RUNSCRIPT:
      ParseRunscript(p, lc, item, index, pass);
      break;
    case CFG_TYPE_MIGTYPE:
      ParseMigtype(p, lc, item, index);
      break;
    case CFG_TYPE_INCEXC:
      ParseInc(p, lc, item, index, pass);
      break;
    case CFG_TYPE_RUN:
      ParseRun(p, lc, item, index, pass);
      break;
    case CFG_TYPE_ACTIONONPURGE:
      ParseActiononpurge(p, lc, item, index, pass);
      break;
    case CFG_TYPE_POOLTYPE:
      ParsePooltype(p, lc, item, index, pass);
      break;
    default:
      break;
  }
}


static bool HasDefaultValue(const ResourceItem& item, const s_jt* keywords)
{
  bool is_default = false;
  uint32_t value = GetItemVariable<uint32_t>(item);
  if (item.default_value) {
    for (int j = 0; keywords[j].name; j++) {
      if (keywords[j].job_type == value) {
        is_default = Bstrcasecmp(item.default_value, keywords[j].name);
        break;
      }
    }
  } else {
    if (value == 0) { is_default = true; }
  }
  return is_default;
}


static bool HasDefaultValue(const ResourceItem& item, const s_jl* keywords)
{
  bool is_default = false;
  uint32_t value = GetItemVariable<uint32_t>(item);
  if (item.default_value) {
    for (int j = 0; keywords[j].name; j++) {
      if (keywords[j].level == value) {
        is_default = Bstrcasecmp(item.default_value, keywords[j].name);
        break;
      }
    }
  } else {
    if (value == 0) { is_default = true; }
  }
  return is_default;
}

template <typename T>
static bool HasDefaultValue(const ResourceItem& item, alist<T>* values)
{
  if (item.default_value) {
    if ((values->size() == 1)
        and (bstrcmp((const char*)values->get(0), item.default_value))) {
      return true;
    }
  } else {
    if ((!values) or (values->size() == 0)) { return true; }
  }
  return false;
}

static bool HasDefaultValue(const ResourceItem& item)
{
  bool is_default = false;

  switch (item.type) {
    case CFG_TYPE_DEVICE: {
      // there are no "default" devices

      ASSERT(!item.default_value);

      const auto& devices = *GetItemVariablePointer<std::vector<Device>*>(item);

      is_default = devices.empty();
      break;
    }
    case CFG_TYPE_RUNSCRIPT: {
      if (item.default_value) {
        /* this should not happen */
        is_default = false;
      } else {
        is_default = (GetItemVariable<alist<RunScript*>*>(item) == NULL);
      }
      break;
    }
    case CFG_TYPE_SHRTRUNSCRIPT:
      /* We don't get here as this type is converted to a CFG_TYPE_RUNSCRIPT
       * when parsed */
      break;
    case CFG_TYPE_ACL: {
      alist<const char*>** alistvalue
          = GetItemVariablePointer<alist<const char*>**>(item);
      alist<const char*>* list = alistvalue[item.code];
      is_default = HasDefaultValue(item, list);
      break;
    }
    case CFG_TYPE_RUN: {
      if (item.default_value) {
        /* this should not happen */
        is_default = false;
      } else {
        is_default = (GetItemVariable<RunResource*>(item) == nullptr);
      }
      break;
    }
    case CFG_TYPE_JOBTYPE: {
      is_default = HasDefaultValue(item, jobtypes);
      break;
    }
    case CFG_TYPE_PROTOCOLTYPE: {
      is_default = HasDefaultValue(item, backupprotocols);
      break;
    }
    case CFG_TYPE_MIGTYPE: {
      is_default = HasDefaultValue(item, migtypes);
      break;
    }
    case CFG_TYPE_REPLACE: {
      is_default = HasDefaultValue(item, ReplaceOptions);
      break;
    }
    case CFG_TYPE_LEVEL: {
      is_default = HasDefaultValue(item, joblevels);
      break;
    }
    case CFG_TYPE_ACTIONONPURGE: {
      is_default = HasDefaultValue(item, ActionOnPurgeOptions);
      break;
    }
    case CFG_TYPE_AUTHPROTOCOLTYPE: {
      is_default = HasDefaultValue(item, authprotocols);
      break;
    }
    case CFG_TYPE_AUTHTYPE: {
      is_default = HasDefaultValue(item, authmethods);
      break;
    }
    case CFG_TYPE_AUDIT: {
      is_default
          = HasDefaultValue(item, GetItemVariable<alist<const char*>*>(item));
      break;
    }
    case CFG_TYPE_POOLTYPE:
      is_default
          = bstrcmp(GetItemVariable<const char*>(item), item.default_value);
      Dmsg1(200, "CFG_TYPE_POOLTYPE: default: %d\n", is_default);
      break;
    default:
      Dmsg2(200, "%s is UNSUPPORTED TYPE: %d\n", item.name, item.type);
      break;
  }

  return is_default;
}


/**
 * callback function for print_config
 * See ../lib/res.cc, function BareosResource::PrintConfig, for more generic
 * handling.
 */
static void PrintConfigCb(const ResourceItem& item,
                          OutputFormatterResource& send,
                          bool,
                          bool inherited,
                          bool verbose)
{
  PoolMem temp;

  bool print = false;

  if (item.is_required) { print = true; }

  if (HasDefaultValue(item)) {
    if (verbose && !item.is_deprecated) {
      print = true;
      inherited = true;
    }
  } else {
    print = true;
  }

  if (!print) { return; }

  switch (item.type) {
    case CFG_TYPE_DEVICE: {
      // Each member of the list is comma-separated

      const auto& devices = *GetItemVariablePointer<std::vector<Device>*>(item);

      alist<const char*> list(devices.size(), not_owned_by_alist);

      for (auto& dev : devices) { list.append(dev.name.c_str()); }

      send.KeyMultipleStringsInOneLine(item.name, &list, false, true);
      break;
    }
    case CFG_TYPE_RUNSCRIPT:
      Dmsg0(200, "CFG_TYPE_RUNSCRIPT\n");
      PrintConfigRunscript(send, item, inherited, verbose);
      break;
    case CFG_TYPE_SHRTRUNSCRIPT:
      /* We don't get here as this type is converted to a CFG_TYPE_RUNSCRIPT
       * when parsed */
      break;
    case CFG_TYPE_ACL: {
      alist<const char*>** alistvalue
          = GetItemVariablePointer<alist<const char*>**>(item);
      alist<const char*>* list = alistvalue[item.code];
      send.KeyMultipleStringsInOneLine(item.name, list, inherited);
      break;
    }
    case CFG_TYPE_RUN:
      PrintConfigRun(send, &item, inherited);
      break;
    case CFG_TYPE_JOBTYPE: {
      uint32_t jobtype = GetItemVariable<uint32_t>(item);

      if (jobtype) {
        for (int j = 0; jobtypes[j].name; j++) {
          if (jobtypes[j].job_type == jobtype) {
            send.KeyString(item.name, jobtypes[j].name, inherited);
            break;
          }
        }
      }
      break;
    }
    case CFG_TYPE_PROTOCOLTYPE: {
      send.KeyString(item.name, GetName(item, backupprotocols), inherited);
      break;
    }
    case CFG_TYPE_MIGTYPE: {
      uint32_t migtype = GetItemVariable<uint32_t>(item);

      if (migtype) {
        for (int j = 0; migtypes[j].name; j++) {
          if (migtypes[j].job_type == migtype) {
            send.KeyString(item.name, migtypes[j].name, inherited);
            break;
          }
        }
      }
      break;
    }
    case CFG_TYPE_REPLACE: {
      send.KeyString(item.name, GetName(item, ReplaceOptions), inherited);
      break;
    }
    case CFG_TYPE_LEVEL: {
      uint32_t level = GetItemVariable<uint32_t>(item);

      if (!level) {
        send.KeyString(item.name, "", true /*inherited*/);
      } else {
        for (int j = 0; joblevels[j].name; j++) {
          if (joblevels[j].level == level) {
            send.KeyString(item.name, joblevels[j].name, inherited);
            break;
          }
        }
      }
      break;
    }
    case CFG_TYPE_ACTIONONPURGE: {
      send.KeyString(item.name, GetName(item, ActionOnPurgeOptions), inherited);
      break;
    }
    case CFG_TYPE_AUTHPROTOCOLTYPE: {
      send.KeyString(item.name, GetName(item, authprotocols), inherited);
      break;
    }
    case CFG_TYPE_AUTHTYPE: {
      send.KeyString(item.name, GetName(item, authmethods), inherited);
      break;
    }
    case CFG_TYPE_AUDIT: {
      // Each member of the list is comma-separated
      send.KeyMultipleStringsInOneLine(
          item.name, GetItemVariable<alist<const char*>*>(item), inherited);
      break;
    }
    case CFG_TYPE_POOLTYPE:
      Dmsg1(200, "%s = %s (%d)\n", item.name,
            GetItemVariable<const char*>(item), inherited);
      send.KeyString(item.name, GetItemVariable<const char*>(item), inherited);
      break;
    default:
      Dmsg2(200, "%s is UNSUPPORTED TYPE: %d\n", item.name, item.type);
      break;
  }
}  // namespace directordaemon

static void ResetAllClientConnectionHandshakeModes(
    ConfigurationParser& t_config)
{
  BareosResource* p = t_config.GetNextRes(R_CLIENT, nullptr);
  while (p) {
    ClientResource* client = dynamic_cast<ClientResource*>(p);
    if (client) {
      client->connection_successful_handshake_
          = ClientConnectionHandshakeMode::kUndefined;
    }
    p = t_config.GetNextRes(R_CLIENT, p);
  };
}

static void ConfigBeforeCallback(ConfigurationParser& t_config)
{
  std::map<int, std::string> map{
      {R_DIRECTOR, "R_DIRECTOR"}, {R_CLIENT, "R_CLIENT"},
      {R_JOBDEFS, "R_JOBDEFS"},   {R_JOB, "R_JOB"},
      {R_STORAGE, "R_STORAGE"},   {R_CATALOG, "R_CATALOG"},
      {R_SCHEDULE, "R_SCHEDULE"}, {R_FILESET, "R_FILESET"},
      {R_POOL, "R_POOL"},         {R_MSGS, "R_MSGS"},
      {R_COUNTER, "R_COUNTER"},   {R_PROFILE, "R_PROFILE"},
      {R_CONSOLE, "R_CONSOLE"},   {R_USER, "R_USER"}};
  t_config.InitializeQualifiedResourceNameTypeConverter(map);
}

static void ConfigReadyCallback(ConfigurationParser& t_config)
{
  CreateAndAddUserAgentConsoleResource(t_config);

  ResetAllClientConnectionHandshakeModes(t_config);
}

static bool AddResourceCopyToEndOfChain(ConfigurationParser* p,
                                        int type,
                                        BareosResource* new_resource = nullptr)
{
  if (!new_resource) {
    switch (type) {
      case R_DIRECTOR:
        new_resource = res_dir;
        res_dir = nullptr;
        break;
      case R_CLIENT:
        new_resource = res_client;
        res_client = nullptr;
        break;
      case R_JOBDEFS:
      case R_JOB:
        new_resource = res_job;
        res_job = nullptr;
        break;
      case R_STORAGE:
        new_resource = res_store;
        res_store = nullptr;
        break;
      case R_CATALOG:
        new_resource = res_cat;
        res_cat = nullptr;
        break;
      case R_SCHEDULE:
        new_resource = res_sch;
        res_sch = nullptr;
        break;
      case R_FILESET:
        new_resource = res_fs;
        res_fs = nullptr;
        break;
      case R_POOL:
        new_resource = res_pool;
        res_pool = nullptr;
        break;
      case R_MSGS:
        new_resource = res_msgs;
        res_msgs = nullptr;
        break;
      case R_COUNTER:
        new_resource = res_counter;
        res_counter = nullptr;
        break;
      case R_PROFILE:
        new_resource = res_profile;
        res_profile = nullptr;
        break;
      case R_CONSOLE:
        new_resource = res_con;
        res_con = nullptr;
        break;
      case R_USER:
        new_resource = res_user;
        res_user = nullptr;
        break;
      default:
        Dmsg3(100, "Unhandled resource type: %d\n", type);
        return false;
    }
  }
  return p->AppendToResourcesChain(new_resource, type);
}

/*
 * Create a special Console named "*UserAgent*" with
 * root console password so that incoming console
 * connections can be handled in unique way
 *
 */
static void CreateAndAddUserAgentConsoleResource(ConfigurationParser& t_config)
{
  DirectorResource* dir_resource
      = (DirectorResource*)t_config.GetNextRes(R_DIRECTOR, NULL);
  if (!dir_resource) { return; }

  ConsoleResource* c = new ConsoleResource();
  c->password_.encoding = dir_resource->password_.encoding;
  c->password_.value = strdup(dir_resource->password_.value);
  c->tls_enable_ = true;
  c->resource_name_ = strdup("*UserAgent*");
  c->description_ = strdup("root console definition");
  c->rcode_ = R_CONSOLE;
  c->rcode_str_ = "R_CONSOLE";
  c->refcnt_ = 1;
  c->internal_ = true;

  AddResourceCopyToEndOfChain(&t_config, R_CONSOLE, c);
}

ConfigurationParser* InitDirConfig(const char* t_configfile, int exit_code)
{
  ConfigurationParser* config = new ConfigurationParser(
      t_configfile, InitResourceCb, StoreConfigCb, PrintConfigCb, exit_code,
      R_NUM, dird_resource_tables, default_config_filename.c_str(),
      "bareos-dir.d", ConfigBeforeCallback, ConfigReadyCallback, SaveResource,
      DumpResource, FreeResource);
  if (config) { config->r_own_ = R_DIRECTOR; }
  return config;
}


// Dump contents of resource
static void DumpResource(int type,
                         BareosResource* res,
                         ConfigurationParser::sender* sendit,
                         void* sock,
                         bool hide_sensitive_data,
                         bool verbose)
{
  PoolMem buf;
  bool recurse = true;
  UaContext* ua = (UaContext*)sock;
  OutputFormatter* output_formatter = nullptr;
  std::unique_ptr<OutputFormatter> output_formatter_local;
  if (ua) {
    output_formatter = ua->send;
  } else {
    output_formatter_local
        = std::make_unique<OutputFormatter>(sendit, nullptr, nullptr, nullptr);
    output_formatter = output_formatter_local.get();
  }

  OutputFormatterResource output_formatter_resource
      = OutputFormatterResource(output_formatter);

  if (!res) {
    PoolMem msg;
    msg.bsprintf(T_("No %s resource defined\n"), my_config->ResToStr(type));
    output_formatter->message(MSG_TYPE_INFO, msg);
    return;
  }

  if (type < 0) { /* no recursion */
    type = -type;
    recurse = false;
  }

  if (ua && !ua->IsResAllowed(res)) { goto bail_out; }

  switch (type) {
    case R_DIRECTOR:
    case R_PROFILE:
    case R_CONSOLE:
    case R_USER:
    case R_COUNTER:
    case R_CLIENT:
    case R_STORAGE:
    case R_CATALOG:
    case R_JOBDEFS:
    case R_JOB:
    case R_SCHEDULE:
    case R_POOL:
      res->PrintConfig(output_formatter_resource, *my_config,
                       hide_sensitive_data, verbose);
      break;
    case R_FILESET: {
      FilesetResource* p = dynamic_cast<FilesetResource*>(res);
      assert(p);
      p->PrintConfig(output_formatter_resource, *my_config, hide_sensitive_data,
                     verbose);
      break;
    }
    case R_MSGS: {
      MessagesResource* p = dynamic_cast<MessagesResource*>(res);
      assert(p);
      p->PrintConfig(output_formatter_resource, *my_config, hide_sensitive_data,
                     verbose);
      break;
    }
    default:
      sendit(sock, T_("Unknown resource type %d in DumpResource.\n"), type);
      break;
  }

bail_out:
  if (recurse && res->next_) {
    DumpResource(type, res->next_, sendit, sock, hide_sensitive_data, verbose);
  }
}

static void FreeResource(BareosResource* res, int type)
{
  if (!res) return;

  if (res->resource_name_) {
    free(res->resource_name_);
    res->resource_name_ = nullptr;
  }
  if (res->description_) {
    free(res->description_);
    res->description_ = nullptr;
  }

  BareosResource* next_resource = res->next_;

  switch (type) {
    case R_DIRECTOR: {
      DirectorResource* p = dynamic_cast<DirectorResource*>(res);
      assert(p);
      if (p->working_directory) { free(p->working_directory); }
      if (p->scripts_directory) { free(p->scripts_directory); }
      if (p->plugin_directory) { free(p->plugin_directory); }
      if (p->plugin_names) { delete p->plugin_names; }
      if (p->password_.value) { free(p->password_.value); }
      if (p->query_file) { free(p->query_file); }
      if (p->DIRaddrs) { FreeAddresses(p->DIRaddrs); }
      if (p->DIRsrc_addr) { FreeAddresses(p->DIRsrc_addr); }
      if (p->verid) { free(p->verid); }
      if (p->keyencrkey.value) { free(p->keyencrkey.value); }
      if (p->audit_events) { delete p->audit_events; }
      if (p->secure_erase_cmdline) { free(p->secure_erase_cmdline); }
      if (p->log_timestamp_format) { free(p->log_timestamp_format); }
      delete p;
      break;
    }
    case R_COUNTER: {
      CounterResource* p = dynamic_cast<CounterResource*>(res);
      assert(p);
      delete p;
      break;
    }
    case R_PROFILE: {
      ProfileResource* p = dynamic_cast<ProfileResource*>(res);
      assert(p);
      for (int i = 0; i < Num_ACL; i++) {
        if (p->ACL_lists[i]) {
          delete p->ACL_lists[i];
          p->ACL_lists[i] = NULL;
        }
      }
      delete p;
      break;
    }
    case R_CONSOLE: {
      ConsoleResource* p = dynamic_cast<ConsoleResource*>(res);
      assert(p);
      if (p->password_.value) { free(p->password_.value); }
      if (p->user_acl.profiles) { delete p->user_acl.profiles; }
      for (int i = 0; i < Num_ACL; i++) {
        if (p->user_acl.ACL_lists[i]) {
          delete p->user_acl.ACL_lists[i];
          p->user_acl.corresponding_resource = nullptr;
          p->user_acl.ACL_lists[i] = NULL;
        }
      }
      delete p;
      break;
    }
    case R_USER: {
      UserResource* p = dynamic_cast<UserResource*>(res);
      assert(p);
      if (p->user_acl.profiles) { delete p->user_acl.profiles; }
      for (int i = 0; i < Num_ACL; i++) {
        if (p->user_acl.ACL_lists[i]) {
          delete p->user_acl.ACL_lists[i];
          p->user_acl.corresponding_resource = nullptr;
          p->user_acl.ACL_lists[i] = NULL;
        }
      }
      delete p;
      break;
    }
    case R_CLIENT: {
      ClientResource* p = dynamic_cast<ClientResource*>(res);
      assert(p);
      if (p->address) { free(p->address); }
      if (p->lanaddress) { free(p->lanaddress); }
      if (p->username) { free(p->username); }
      if (p->password_.value) { free(p->password_.value); }
      delete p;
      break;
    }
    case R_STORAGE: {
      StorageResource* p = dynamic_cast<StorageResource*>(res);
      assert(p);
      if (p->address) { free(p->address); }
      if (p->lanaddress) { free(p->lanaddress); }
      if (p->username) { free(p->username); }
      if (p->password_.value) { free(p->password_.value); }
      if (p->media_type) { free(p->media_type); }
      if (p->ndmp_changer_device) { free(p->ndmp_changer_device); }
      delete p;
      break;
    }
    case R_CATALOG: {
      CatalogResource* p = dynamic_cast<CatalogResource*>(res);
      assert(p);
      if (p->db_address) { free(p->db_address); }
      if (p->db_socket) { free(p->db_socket); }
      if (p->db_user) { free(p->db_user); }
      if (p->db_name) { free(p->db_name); }
      if (p->db_password.value) { free(p->db_password.value); }
      delete p;
      break;
    }
    case R_FILESET: {
      FilesetResource* p = dynamic_cast<FilesetResource*>(res);
      assert(p);
      for (auto q : p->include_items) { FreeIncludeExcludeItem(q); }
      p->include_items.clear();
      for (auto q : p->exclude_items) { FreeIncludeExcludeItem(q); }
      p->exclude_items.clear();
      delete p;
      break;
    }
    case R_POOL: {
      PoolResource* p = dynamic_cast<PoolResource*>(res);
      assert(p);
      if (p->pool_type) { free(p->pool_type); }
      if (p->label_format) { free(p->label_format); }
      if (p->cleaning_prefix) { free(p->cleaning_prefix); }
      if (p->storage) { delete p->storage; }
      delete p;
      break;
    }
    case R_SCHEDULE: {
      ScheduleResource* p = dynamic_cast<ScheduleResource*>(res);
      assert(p);
      if (p->run) {
        RunResource *nrun, *next;
        nrun = p->run;
        while (nrun) {
          next = nrun->next;
          delete nrun;
          nrun = next;
        }
      }
      delete p;
      break;
    }
    case R_JOBDEFS:
    case R_JOB: {
      JobResource* p = dynamic_cast<JobResource*>(res);
      assert(p);
      if (p->backup_format) { free(p->backup_format); }
      if (p->RestoreWhere) { free(p->RestoreWhere); }
      if (p->RegexWhere) { free(p->RegexWhere); }
      if (p->strip_prefix) { free(p->strip_prefix); }
      if (p->add_prefix) { free(p->add_prefix); }
      if (p->add_suffix) { free(p->add_suffix); }
      if (p->RestoreBootstrap) { free(p->RestoreBootstrap); }
      if (p->WriteBootstrap) { free(p->WriteBootstrap); }
      if (p->WriteVerifyList) { free(p->WriteVerifyList); }
      if (p->selection_pattern) { free(p->selection_pattern); }
      if (p->run_cmds) { delete p->run_cmds; }
      if (p->storage) { delete p->storage; }
      if (p->FdPluginOptions) { delete p->FdPluginOptions; }
      if (p->SdPluginOptions) { delete p->SdPluginOptions; }
      if (p->DirPluginOptions) { delete p->DirPluginOptions; }
      if (p->base) { delete p->base; }
      if (p->RunScripts) {
        FreeRunscripts(p->RunScripts);
        delete p->RunScripts;
      }
      delete p;
      break;
    }
    case R_MSGS: {
      MessagesResource* p = dynamic_cast<MessagesResource*>(res);
      assert(p);
      delete p;
      break;
    }
    default:
      printf(T_("Unknown resource type %d in FreeResource.\n"), type);
      break;
  }
  if (next_resource) { my_config->FreeResourceCb_(next_resource, type); }
}

/**
 * Save the new resource by chaining it into the head list for
 * the resource. If this is pass 2, we update any resource
 * pointers because they may not have been defined until
 * later in pass 1.
 */
static bool SaveResource(int type, const ResourceItem* items, int pass)
{
  BareosResource* allocated_resource
      = *dird_resource_tables[type].allocated_resource_;

  // Job resources are validated after applying JobDefs
  if (type != R_JOB && type != R_JOBDEFS) {
    if (pass == 1 && !ValidateResource(type, items, allocated_resource)) {
      return false;
    }
  }
  switch (type) {
    case R_DIRECTOR: {
      /* IP Addresses can be set by multiple directives.
       * If they differ from the default,
       * the set the main directive to be set. */
      if ((res_dir->DIRaddrs) && (res_dir->DIRaddrs->size() > 0)) {
        for (int i = 0; items[i].name; i++) {
          if (Bstrcasecmp(items[i].name, "Addresses")) {
            // SetBit(i, allocated_resource->item_present_);
            ClearBit(i, allocated_resource->inherit_content_);
          }
        }
      }
      break;
    }
    case R_JOB:
      /* Check Job requirements after applying JobDefs
       * Ensure that the name item is present however. */
      if (items[0].is_required) {
        if (!allocated_resource->IsMemberPresent(items[0].name)) {
          Emsg2(M_ERROR, 0,
                T_("%s item is required in %s resource, but not found.\n"),
                items[0].name, dird_resource_tables[type].name);
          return false;
        }
      }
      break;
    default:
      break;
  }

  /* During pass 2 in each "store" routine, we looked up pointers
   * to all the resources referenced in the current resource, now we
   * must copy their addresses from the static record to the allocated
   * record. */
  if (pass == 2) {
    bool ret = UpdateResourcePointer(type, items);
    bool validation = true;
    switch (type) {
      case R_JOBDEFS:
      case R_JOB:
      case R_DIRECTOR:
        break;
      default: {
        BareosResource* pass1_resource = my_config->GetResWithName(
            type, allocated_resource->resource_name_);
        validation = ValidateResource(type, items, pass1_resource);
      } break;
    }

    return validation && ret;
  }

  if (!AddResourceCopyToEndOfChain(my_config, type)) { return false; }
  return true;
}

std::vector<JobResource*> GetAllJobResourcesByClientName(std::string name)
{
  std::vector<JobResource*> all_matching_jobs;
  JobResource* job{nullptr};

  do {
    job = static_cast<JobResource*>(my_config->GetNextRes(R_JOB, job));
    if (job && job->client) {
      if (job->client->resource_name_ == name) {
        all_matching_jobs.push_back(job);
      }
    }
  } while (job);

  return all_matching_jobs;
}

} /* namespace directordaemon */
