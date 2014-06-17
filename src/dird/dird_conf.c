/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2013 Bareos GmbH & Co. KG

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
int32_t r_first = R_FIRST;
int32_t r_last  = R_LAST;
static RES *sres_head[R_LAST - R_FIRST + 1];
RES **res_head = sres_head;

/*
 * Imported subroutines
 */
extern void store_run(LEX *lc, RES_ITEM *item, int index, int pass);
extern void store_finc(LEX *lc, RES_ITEM *item, int index, int pass);
extern void store_inc(LEX *lc, RES_ITEM *item, int index, int pass);

/*
 * Forward referenced subroutines
 */
void store_jobtype(LEX *lc, RES_ITEM *item, int index, int pass);
void store_level(LEX *lc, RES_ITEM *item, int index, int pass);
void store_protocoltype(LEX *lc, RES_ITEM *item, int index, int pass);
void store_replace(LEX *lc, RES_ITEM *item, int index, int pass);
void store_migtype(LEX *lc, RES_ITEM *item, int index, int pass);
static void store_authprotocoltype(LEX *lc, RES_ITEM *item, int index, int pass);
static void store_authtype(LEX *lc, RES_ITEM *item, int index, int pass);
static void store_clearpassword(LEX *lc, RES_ITEM *item, int index, int pass);
static void store_acl(LEX *lc, RES_ITEM *item, int index, int pass);
static void store_actiononpurge(LEX *lc, RES_ITEM *item, int index, int pass);
static void store_device(LEX *lc, RES_ITEM *item, int index, int pass);
static void store_runscript_when(LEX *lc, RES_ITEM *item, int index, int pass);
static void store_runscript_target(LEX *lc, RES_ITEM *item, int index, int pass);
static void store_runscript_cmd(LEX *lc, RES_ITEM *item, int index, int pass);
static void store_short_runscript(LEX *lc, RES_ITEM *item, int index, int pass);
static void store_runscript_bool(LEX *lc, RES_ITEM *item, int index, int pass);
static void store_runscript(LEX *lc, RES_ITEM *item, int index, int pass);

/*
 * We build the current resource here as we are
 * scanning the resource configuration definition,
 * then move it to allocated memory when the resource
 * scan is complete.
 */
#if defined(_MSC_VER)
extern "C" { // work around visual compiler mangling variables
   URES res_all;
}
#else
URES res_all;
#endif
int32_t res_all_size = sizeof(res_all);

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
   { "name", store_name, ITEM(res_dir.hdr.name), 0, ITEM_REQUIRED, NULL },
   { "description", store_str, ITEM(res_dir.hdr.desc), 0, 0, NULL },
   { "messages", store_res, ITEM(res_dir.messages), R_MSGS, 0, NULL },
   { "dirport", store_addresses_port, ITEM(res_dir.DIRaddrs), 0, ITEM_DEFAULT, DIR_DEFAULT_PORT },
   { "diraddress", store_addresses_address, ITEM(res_dir.DIRaddrs), 0, ITEM_DEFAULT, DIR_DEFAULT_PORT },
   { "diraddresses", store_addresses, ITEM(res_dir.DIRaddrs), 0, ITEM_DEFAULT, DIR_DEFAULT_PORT },
   { "dirsourceaddress", store_addresses_address, ITEM(res_dir.DIRsrc_addr), 0, ITEM_DEFAULT, "0" },
   { "queryfile", store_dir, ITEM(res_dir.query_file), 0, ITEM_REQUIRED, NULL },
   { "workingdirectory", store_dir, ITEM(res_dir.working_directory), 0, ITEM_DEFAULT, _PATH_BAREOS_WORKINGDIR },
   { "piddirectory", store_dir, ITEM(res_dir.pid_directory), 0, ITEM_DEFAULT, _PATH_BAREOS_PIDDIR },
   { "plugindirectory", store_dir, ITEM(res_dir.plugin_directory), 0, 0, NULL },
   { "scriptsdirectory", store_dir, ITEM(res_dir.scripts_directory), 0, 0, NULL },
   { "subscriptions", store_pint32, ITEM(res_dir.subscriptions), 0, ITEM_DEFAULT, "0" },
   { "subsysdirectory", store_dir, ITEM(res_dir.subsys_directory), 0, 0, NULL },
   { "maximumconcurrentjobs", store_pint32, ITEM(res_dir.MaxConcurrentJobs), 0, ITEM_DEFAULT, "1" },
   { "maximumconsoleconnections", store_pint32, ITEM(res_dir.MaxConsoleConnect), 0, ITEM_DEFAULT, "20" },
   { "password", store_clearpassword, ITEM(res_dir.password), 0, ITEM_REQUIRED, NULL },
   { "fdconnecttimeout", store_time, ITEM(res_dir.FDConnectTimeout), 0, ITEM_DEFAULT, "180" /* 3 minutes */ },
   { "sdconnecttimeout", store_time, ITEM(res_dir.SDConnectTimeout), 0, ITEM_DEFAULT, "1800" /* 30 minutes */ },
   { "heartbeatinterval", store_time, ITEM(res_dir.heartbeat_interval), 0, ITEM_DEFAULT, "0" },
   { "tlsauthenticate", store_bool, ITEM(res_dir.tls_authenticate), 0, 0, NULL },
   { "tlsenable", store_bool, ITEM(res_dir.tls_enable), 0, 0, NULL },
   { "tlsrequire", store_bool, ITEM(res_dir.tls_require), 0, 0, NULL },
   { "tlsverifypeer", store_bool, ITEM(res_dir.tls_verify_peer), 0, ITEM_DEFAULT, "true" },
   { "tlscacertificatefile", store_dir, ITEM(res_dir.tls_ca_certfile), 0, 0, NULL },
   { "tlscacertificatedir", store_dir, ITEM(res_dir.tls_ca_certdir), 0, 0, NULL },
   { "tlscertificate", store_dir, ITEM(res_dir.tls_certfile), 0, 0, NULL },
   { "tlskey", store_dir, ITEM(res_dir.tls_keyfile), 0, 0, NULL },
   { "tlsdhfile", store_dir, ITEM(res_dir.tls_dhfile), 0, 0, NULL },
   { "tlsallowedcn", store_alist_str, ITEM(res_dir.tls_allowed_cns), 0, 0, NULL },
   { "statisticsretention", store_time, ITEM(res_dir.stats_retention), 0, ITEM_DEFAULT, "157680000" /* 5 years */ },
   { "verid", store_str, ITEM(res_dir.verid), 0, 0, NULL },
   { "optimizeforsize", store_bool, ITEM(res_dir.optimize_for_size), 0, ITEM_DEFAULT, "true" },
   { "optimizeforspeed", store_bool, ITEM(res_dir.optimize_for_speed), 0, ITEM_DEFAULT, "false" },
   { "keyencryptionkey", store_clearpassword, ITEM(res_dir.keyencrkey), 1, 0, NULL },
   { "ndmpsnooping", store_bool, ITEM(res_dir.ndmp_snooping), 0, 0, NULL },
   { "ndmploglevel", store_pint32, ITEM(res_dir.ndmp_loglevel), 0, ITEM_DEFAULT, "4" },
   { NULL, NULL, { 0 }, 0, 0, NULL }
};

/*
 * Console Resource
 *
 * name handler value code flags default_value
 */
static RES_ITEM con_items[] = {
   { "name", store_name, ITEM(res_con.hdr.name), 0, ITEM_REQUIRED, NULL },
   { "description", store_str, ITEM(res_con.hdr.desc), 0, 0, NULL },
   { "password", store_clearpassword, ITEM(res_con.password), 0, ITEM_REQUIRED, NULL },
   { "jobacl", store_acl, ITEM(res_con.ACL_lists), Job_ACL, 0, NULL },
   { "clientacl", store_acl, ITEM(res_con.ACL_lists), Client_ACL, 0, NULL },
   { "storageacl", store_acl, ITEM(res_con.ACL_lists), Storage_ACL, 0, NULL },
   { "scheduleacl", store_acl, ITEM(res_con.ACL_lists), Schedule_ACL, 0, NULL },
   { "runacl", store_acl, ITEM(res_con.ACL_lists), Run_ACL, 0, NULL },
   { "poolacl", store_acl, ITEM(res_con.ACL_lists), Pool_ACL, 0, NULL },
   { "commandacl", store_acl, ITEM(res_con.ACL_lists), Command_ACL, 0, NULL },
   { "filesetacl", store_acl, ITEM(res_con.ACL_lists), FileSet_ACL, 0, NULL },
   { "catalogacl", store_acl, ITEM(res_con.ACL_lists), Catalog_ACL, 0, NULL },
   { "whereacl", store_acl, ITEM(res_con.ACL_lists), Where_ACL, 0, NULL },
   { "pluginoptionsacl", store_acl, ITEM(res_con.ACL_lists), PluginOptions_ACL, 0, NULL },
   { "tlsauthenticate", store_bool, ITEM(res_con.tls_authenticate), 0, 0, NULL },
   { "tlsenable", store_bool, ITEM(res_con.tls_enable), 0, 0, NULL },
   { "tlsrequire", store_bool, ITEM(res_con.tls_require), 0, 0, NULL },
   { "tlsverifypeer", store_bool, ITEM(res_con.tls_verify_peer), 0, ITEM_DEFAULT, "true" },
   { "tlscacertificatefile", store_dir, ITEM(res_con.tls_ca_certfile), 0, 0, NULL },
   { "tlscacertificatedir", store_dir, ITEM(res_con.tls_ca_certdir), 0, 0, NULL },
   { "tlscertificate", store_dir, ITEM(res_con.tls_certfile), 0, 0, NULL },
   { "tlskey", store_dir, ITEM(res_con.tls_keyfile), 0, 0, NULL },
   { "tlsdhfile", store_dir, ITEM(res_con.tls_dhfile), 0, 0, NULL },
   { "tlsallowedcn", store_alist_str, ITEM(res_con.tls_allowed_cns), 0, 0, NULL },
   { NULL, NULL, { 0 }, 0, 0, NULL }
};

/*
 * Client or File daemon resource
 *
 * name handler value code flags default_value
 */
static RES_ITEM cli_items[] = {
   { "name", store_name, ITEM(res_client.hdr.name), 0, ITEM_REQUIRED, NULL },
   { "description", store_str, ITEM(res_client.hdr.desc), 0, 0, NULL },
   { "protocol", store_authprotocoltype, ITEM(res_client.Protocol), 0, ITEM_DEFAULT, "Native" },
   { "authtype", store_authtype, ITEM(res_client.AuthType), 0, ITEM_DEFAULT, "None" },
   { "address", store_str, ITEM(res_client.address), 0, ITEM_REQUIRED, NULL },
   { "fdaddress", store_str, ITEM(res_client.address), 0, 0, NULL },
   { "port", store_pint32, ITEM(res_client.FDport), 0, ITEM_DEFAULT, FD_DEFAULT_PORT },
   { "fdport", store_pint32, ITEM(res_client.FDport), 0, ITEM_DEFAULT, FD_DEFAULT_PORT },
   { "username", store_str, ITEM(res_client.username), 0, 0, NULL },
   { "password", store_clearpassword, ITEM(res_client.password), 0, ITEM_REQUIRED, NULL },
   { "fdpassword", store_clearpassword, ITEM(res_client.password), 0, 0, NULL },
   { "catalog", store_res, ITEM(res_client.catalog), R_CATALOG, 0, NULL },
   { "hardquota", store_size64, ITEM(res_client.HardQuota), 0, ITEM_DEFAULT, "0" },
   { "softquota", store_size64, ITEM(res_client.SoftQuota), 0, ITEM_DEFAULT, "0" },
   { "softquotagraceperiod", store_time, ITEM(res_client.SoftQuotaGracePeriod), 0, ITEM_DEFAULT, "0" },
   { "strictquotas", store_bool, ITEM(res_client.StrictQuotas), 0, ITEM_DEFAULT, "false" },
   { "quotaincludefailedjobs", store_bool, ITEM(res_client.QuotaIncludeFailedJobs), 0, ITEM_DEFAULT, "true" },
   { "fileretention", store_time, ITEM(res_client.FileRetention), 0, ITEM_DEFAULT, "5184000" /* 60 days */ },
   { "jobretention", store_time, ITEM(res_client.JobRetention), 0, ITEM_DEFAULT, "15552000" /* 180 days */ },
   { "heartbeatinterval", store_time, ITEM(res_client.heartbeat_interval), 0, ITEM_DEFAULT, "0" },
   { "autoprune", store_bool, ITEM(res_client.AutoPrune), 0, ITEM_DEFAULT, "false" },
   { "maximumconcurrentjobs", store_pint32, ITEM(res_client.MaxConcurrentJobs), 0, ITEM_DEFAULT, "1" },
   { "tlsauthenticate", store_bool, ITEM(res_client.tls_authenticate), 0, 0, NULL },
   { "tlsenable", store_bool, ITEM(res_client.tls_enable), 0, 0, NULL },
   { "tlsrequire", store_bool, ITEM(res_client.tls_require), 0, 0, NULL },
   { "tlscacertificatefile", store_dir, ITEM(res_client.tls_ca_certfile), 0, 0, NULL },
   { "tlscacertificatedir", store_dir, ITEM(res_client.tls_ca_certdir), 0, 0, NULL },
   { "tlscertificate", store_dir, ITEM(res_client.tls_certfile), 0, 0, NULL },
   { "tlskey", store_dir, ITEM(res_client.tls_keyfile), 0, 0, NULL },
   { "tlsallowedcn", store_alist_str, ITEM(res_client.tls_allowed_cns), 0, 0, NULL },
   { "maximumbandwidthperjob", store_speed, ITEM(res_client.max_bandwidth), 0, 0, NULL },
   { "ndmploglevel", store_pint32, ITEM(res_client.ndmp_loglevel), 0, ITEM_DEFAULT, "4" },
   { "ndmpblocksize", store_pint32, ITEM(res_client.ndmp_blocksize), 0, ITEM_DEFAULT, "64512" },
   { NULL, NULL, { 0 }, 0, 0, NULL }
};

/* Storage daemon resource
 *
 * name handler value code flags default_value
 */
static RES_ITEM store_items[] = {
   { "name", store_name, ITEM(res_store.hdr.name), 0, ITEM_REQUIRED, NULL },
   { "description", store_str, ITEM(res_store.hdr.desc), 0, 0, NULL },
   { "protocol", store_authprotocoltype, ITEM(res_store.Protocol), 0, ITEM_DEFAULT, "Native" },
   { "authtype", store_authtype, ITEM(res_store.AuthType), 0, ITEM_DEFAULT, "None" },
   { "address", store_str, ITEM(res_store.address), 0, ITEM_REQUIRED, NULL },
   { "sdaddress", store_str, ITEM(res_store.address), 0, 0, NULL },
   { "port", store_pint32, ITEM(res_store.SDport), 0, ITEM_DEFAULT, SD_DEFAULT_PORT },
   { "sdport", store_pint32, ITEM(res_store.SDport), 0, ITEM_DEFAULT, SD_DEFAULT_PORT },
   { "username", store_str, ITEM(res_store.username), 0, 0, NULL },
   { "password", store_clearpassword, ITEM(res_store.password), 0, ITEM_REQUIRED, NULL },
   { "sdpassword", store_clearpassword, ITEM(res_store.password), 0, 0, NULL },
   { "device", store_device, ITEM(res_store.device), R_DEVICE, ITEM_REQUIRED, NULL },
   { "mediatype", store_strname, ITEM(res_store.media_type), 0, ITEM_REQUIRED, NULL },
   { "autochanger", store_bool, ITEM(res_store.autochanger), 0, ITEM_DEFAULT, "false" },
   { "enabled", store_bool, ITEM(res_store.enabled), 0, ITEM_DEFAULT, "true" },
   { "allowcompression", store_bool, ITEM(res_store.AllowCompress), 0, ITEM_DEFAULT, "true" },
   { "heartbeatinterval", store_time, ITEM(res_store.heartbeat_interval), 0, ITEM_DEFAULT, "0" },
   { "maximumconcurrentjobs", store_pint32, ITEM(res_store.MaxConcurrentJobs), 0, ITEM_DEFAULT, "1" },
   { "maximumconcurrentreadjobs", store_pint32, ITEM(res_store.MaxConcurrentReadJobs), 0, ITEM_DEFAULT, "0" },
   { "sddport", store_pint32, ITEM(res_store.SDDport), 0, ITEM_DEPRECATED, NULL },
   { "tlsauthenticate", store_bool, ITEM(res_store.tls_authenticate), 0, 0, NULL },
   { "tlsenable", store_bool, ITEM(res_store.tls_enable), 0, 0, NULL },
   { "tlsrequire", store_bool, ITEM(res_store.tls_require), 0, 0, NULL },
   { "tlscacertificatefile", store_dir, ITEM(res_store.tls_ca_certfile), 0, 0, NULL },
   { "tlscacertificatedir", store_dir, ITEM(res_store.tls_ca_certdir), 0, 0, NULL },
   { "tlscertificate", store_dir, ITEM(res_store.tls_certfile), 0, 0, NULL },
   { "tlskey", store_dir, ITEM(res_store.tls_keyfile), 0, 0, NULL },
   { "pairedstorage", store_res, ITEM(res_store.paired_storage), R_STORAGE, 0, NULL },
   { NULL, NULL, { 0 }, 0, 0, NULL }
};

/*
 * Catalog Resource Directives
 *
 * name handler value code flags default_value
 */
static RES_ITEM cat_items[] = {
   { "name", store_name, ITEM(res_cat.hdr.name), 0, ITEM_REQUIRED, NULL },
   { "description", store_str, ITEM(res_cat.hdr.desc), 0, 0, NULL },
   { "address", store_str, ITEM(res_cat.db_address), 0, 0, NULL },
   { "dbaddress", store_str, ITEM(res_cat.db_address), 0, 0, NULL },
   { "dbport", store_pint32, ITEM(res_cat.db_port), 0, 0, NULL },
   { "password", store_clearpassword, ITEM(res_cat.db_password), 0, 0, NULL },
   { "dbpassword", store_str, ITEM(res_cat.db_password), 0, 0, NULL },
   { "dbuser", store_str, ITEM(res_cat.db_user), 0, 0, NULL },
   { "user", store_str, ITEM(res_cat.db_user), 0, 0, NULL },
   { "dbname", store_str, ITEM(res_cat.db_name), 0, ITEM_REQUIRED, NULL },
#ifdef HAVE_DYNAMIC_CATS_BACKENDS
   { "dbdriver", store_str, ITEM(res_cat.db_driver), 0, ITEM_REQUIRED, NULL },
#else
   { "dbdriver", store_str, ITEM(res_cat.db_driver), 0, 0, NULL },
#endif
   { "dbsocket", store_str, ITEM(res_cat.db_socket), 0, 0, NULL },
   /* Turned off for the moment */
   { "multipleconnections", store_bit, ITEM(res_cat.mult_db_connections), 0, 0, NULL },
   { "disablebatchinsert", store_bool, ITEM(res_cat.disable_batch_insert), 0, ITEM_DEFAULT, "false" },
   { "minconnections", store_pint32, ITEM(res_cat.pooling_min_connections), 0, ITEM_DEFAULT, "1" },
   { "maxconnections", store_pint32, ITEM(res_cat.pooling_max_connections), 0, ITEM_DEFAULT, "5" },
   { "incconnections", store_pint32, ITEM(res_cat.pooling_increment_connections), 0, ITEM_DEFAULT, "1" },
   { "idletimeout", store_pint32, ITEM(res_cat.pooling_idle_timeout), 0, ITEM_DEFAULT, "30" },
   { "validatetimeout", store_pint32, ITEM(res_cat.pooling_validate_timeout), 0, ITEM_DEFAULT, "120" },
   { NULL, NULL, { 0 }, 0, 0, NULL }
};

/*
 * Job Resource Directives
 *
 * name handler value code flags default_value
 */
RES_ITEM job_items[] = {
   { "name", store_name, ITEM(res_job.hdr.name), 0, ITEM_REQUIRED, NULL },
   { "description", store_str, ITEM(res_job.hdr.desc), 0, 0, NULL },
   { "type", store_jobtype, ITEM(res_job.JobType), 0, ITEM_REQUIRED, NULL },
   { "protocol", store_protocoltype, ITEM(res_job.Protocol), 0, ITEM_DEFAULT, "Native" },
   { "backupformat", store_str, ITEM(res_job.backup_format), 0, ITEM_DEFAULT, "Native" },
   { "level", store_level, ITEM(res_job.JobLevel), 0, 0, NULL },
   { "messages", store_res, ITEM(res_job.messages), R_MSGS, ITEM_REQUIRED, NULL },
   { "storage", store_alist_res, ITEM(res_job.storage), R_STORAGE, 0, NULL },
   { "pool", store_res, ITEM(res_job.pool), R_POOL, ITEM_REQUIRED, NULL },
   { "fullbackuppool", store_res, ITEM(res_job.full_pool), R_POOL, 0, NULL },
   { "incrementalbackuppool", store_res, ITEM(res_job.inc_pool), R_POOL, 0, NULL },
   { "differentialbackuppool", store_res, ITEM(res_job.diff_pool), R_POOL, 0, NULL },
   { "nextpool", store_res, ITEM(res_job.next_pool), R_POOL, 0, NULL },
   { "client", store_res, ITEM(res_job.client), R_CLIENT, ITEM_REQUIRED, NULL },
   { "fileset", store_res, ITEM(res_job.fileset), R_FILESET, ITEM_REQUIRED, NULL },
   { "schedule", store_res, ITEM(res_job.schedule), R_SCHEDULE, 0, NULL },
   { "verifyjob", store_res, ITEM(res_job.verify_job), R_JOB, 0, NULL },
   { "jobtoverify", store_res, ITEM(res_job.verify_job), R_JOB, 0, NULL },
   { "jobdefs", store_res, ITEM(res_job.jobdefs), R_JOBDEFS, 0, NULL },
   { "run", store_alist_str, ITEM(res_job.run_cmds), 0, 0, NULL },
   /* Root of where to restore files */
   { "where", store_dir, ITEM(res_job.RestoreWhere), 0, 0, NULL },
   { "regexwhere", store_str, ITEM(res_job.RegexWhere), 0, 0, NULL },
   { "stripprefix", store_str, ITEM(res_job.strip_prefix), 0, 0, NULL },
   { "addprefix", store_str, ITEM(res_job.add_prefix), 0, 0, NULL },
   { "addsuffix", store_str, ITEM(res_job.add_suffix), 0, 0, NULL },
   /* Where to find bootstrap during restore */
   { "bootstrap",store_dir, ITEM(res_job.RestoreBootstrap), 0, 0, NULL },
   /* Where to write bootstrap file during backup */
   { "writebootstrap",store_dir, ITEM(res_job.WriteBootstrap), 0, 0, NULL },
   { "writeverifylist",store_dir, ITEM(res_job.WriteVerifyList), 0, 0, NULL },
   { "replace", store_replace, ITEM(res_job.replace), 0, ITEM_DEFAULT, "Always" },
   { "maximumbandwidth", store_speed, ITEM(res_job.max_bandwidth), 0, 0, NULL },
   { "maxrunschedtime", store_time, ITEM(res_job.MaxRunSchedTime), 0, 0, NULL },
   { "maxruntime", store_time, ITEM(res_job.MaxRunTime), 0, 0, NULL },
   { "fullmaxwaittime", store_time, ITEM(res_job.FullMaxRunTime), 0, ITEM_DEPRECATED, NULL },
   { "incrementalmaxwaittime", store_time, ITEM(res_job.IncMaxRunTime), 0, ITEM_DEPRECATED, NULL },
   { "differentialmaxwaittime", store_time, ITEM(res_job.DiffMaxRunTime), 0, ITEM_DEPRECATED, NULL },
   { "fullmaxruntime", store_time, ITEM(res_job.FullMaxRunTime), 0, 0, NULL },
   { "incrementalmaxruntime", store_time, ITEM(res_job.IncMaxRunTime), 0, 0, NULL },
   { "differentialmaxruntime", store_time, ITEM(res_job.DiffMaxRunTime), 0, 0, NULL },
   { "maxwaittime", store_time, ITEM(res_job.MaxWaitTime), 0, 0, NULL },
   { "maxstartdelay",store_time, ITEM(res_job.MaxStartDelay), 0, 0, NULL },
   { "maxfullinterval", store_time, ITEM(res_job.MaxFullInterval), 0, 0, NULL },
   { "maxdiffinterval", store_time, ITEM(res_job.MaxDiffInterval), 0, 0, NULL },
   { "prefixlinks", store_bool, ITEM(res_job.PrefixLinks), 0, ITEM_DEFAULT, "false" },
   { "prunejobs", store_bool, ITEM(res_job.PruneJobs), 0, ITEM_DEFAULT, "false" },
   { "prunefiles", store_bool, ITEM(res_job.PruneFiles), 0, ITEM_DEFAULT, "false" },
   { "prunevolumes",store_bool, ITEM(res_job.PruneVolumes), 0, ITEM_DEFAULT, "false" },
   { "purgemigrationjob", store_bool, ITEM(res_job.PurgeMigrateJob), 0, ITEM_DEFAULT, "false" },
   { "enabled", store_bool, ITEM(res_job.enabled), 0, ITEM_DEFAULT, "true" },
   { "spoolattributes",store_bool, ITEM(res_job.SpoolAttributes), 0, ITEM_DEFAULT, "false" },
   { "spooldata", store_bool, ITEM(res_job.spool_data), 0, ITEM_DEFAULT, "false" },
   { "spoolsize", store_size64, ITEM(res_job.spool_size), 0, 0, NULL },
   { "rerunfailedlevels", store_bool, ITEM(res_job.rerun_failed_levels), 0, ITEM_DEFAULT, "false" },
   { "prefermountedvolumes", store_bool, ITEM(res_job.PreferMountedVolumes), 0, ITEM_DEFAULT, "true" },
   { "runbeforejob", store_short_runscript, ITEM(res_job.RunScripts), 0, 0, NULL },
   { "runafterjob", store_short_runscript, ITEM(res_job.RunScripts), 0, 0, NULL },
   { "runafterfailedjob", store_short_runscript, ITEM(res_job.RunScripts), 0, 0, NULL },
   { "clientrunbeforejob", store_short_runscript, ITEM(res_job.RunScripts), 0, 0, NULL },
   { "clientrunafterjob", store_short_runscript, ITEM(res_job.RunScripts), 0, 0, NULL },
   { "maximumconcurrentjobs", store_pint32, ITEM(res_job.MaxConcurrentJobs), 0, ITEM_DEFAULT, "1" },
   { "rescheduleonerror", store_bool, ITEM(res_job.RescheduleOnError), 0, ITEM_DEFAULT, "false" },
   { "rescheduleinterval", store_time, ITEM(res_job.RescheduleInterval), 0, ITEM_DEFAULT, "1800" /* 30 minutes */ },
   { "rescheduletimes", store_pint32, ITEM(res_job.RescheduleTimes), 0, ITEM_DEFAULT, "5" },
   { "priority", store_pint32, ITEM(res_job.Priority), 0, ITEM_DEFAULT, "10" },
   { "allowmixedpriority", store_bool, ITEM(res_job.allow_mixed_priority), 0, ITEM_DEFAULT, "false" },
   { "writepartafterjob", store_bool, ITEM(res_job.write_part_after_job), 0, ITEM_DEPRECATED, NULL },
   { "selectionpattern", store_str, ITEM(res_job.selection_pattern), 0, 0, NULL },
   { "runscript", store_runscript, ITEM(res_job.RunScripts), 0, ITEM_NO_EQUALS, NULL },
   { "selectiontype", store_migtype, ITEM(res_job.selection_type), 0, 0, NULL },
   { "accurate", store_bool, ITEM(res_job.accurate), 0, ITEM_DEFAULT, "false" },
   { "allowduplicatejobs", store_bool, ITEM(res_job.AllowDuplicateJobs), 0, ITEM_DEFAULT, "true" },
   { "allowhigherduplicates", store_bool, ITEM(res_job.AllowHigherDuplicates), 0, ITEM_DEFAULT, "true" },
   { "cancellowerlevelduplicates", store_bool, ITEM(res_job.CancelLowerLevelDuplicates), 0, ITEM_DEFAULT, "false" },
   { "cancelqueuedduplicates", store_bool, ITEM(res_job.CancelQueuedDuplicates), 0, ITEM_DEFAULT, "false" },
   { "cancelrunningduplicates", store_bool, ITEM(res_job.CancelRunningDuplicates), 0, ITEM_DEFAULT, "false" },
   { "pluginoptions", store_str, ITEM(res_job.PluginOptions), 0, 0, NULL },
   { "base", store_alist_res, ITEM(res_job.base), R_JOB, 0, NULL },
   { NULL, NULL, { 0 }, 0, 0, NULL }
};

/*
 * FileSet resource
 *
 * name handler value code flags default_value
 */
static RES_ITEM fs_items[] = {
   { "name", store_name, ITEM(res_fs.hdr.name), 0, ITEM_REQUIRED, NULL },
   { "description", store_str, ITEM(res_fs.hdr.desc), 0, 0, NULL },
   { "include", store_inc, { 0 }, 0, ITEM_NO_EQUALS, NULL },
   { "exclude", store_inc, { 0 }, 1, ITEM_NO_EQUALS, NULL },
   { "ignorefilesetchanges", store_bool, ITEM(res_fs.ignore_fs_changes), 0, ITEM_DEFAULT, "false" },
   { "enablevss", store_bool, ITEM(res_fs.enable_vss), 0, ITEM_DEFAULT, "true" },
   { NULL, NULL, { 0 }, 0, 0, NULL }
};

/*
 * Schedule -- see run_conf.c
 *
 * name handler value code flags default_value
 */
static RES_ITEM sch_items[] = {
   { "name", store_name, ITEM(res_sch.hdr.name), 0, ITEM_REQUIRED, NULL },
   { "description", store_str, ITEM(res_sch.hdr.desc), 0, 0, NULL },
   { "run", store_run, ITEM(res_sch.run), 0, 0, NULL },
   { NULL, NULL, { 0 }, 0, 0, NULL }
};

/*
 * Pool resource
 *
 * name handler value code flags default_value
 */
static RES_ITEM pool_items[] = {
   { "name", store_name, ITEM(res_pool.hdr.name), 0, ITEM_REQUIRED, NULL },
   { "description", store_str, ITEM(res_pool.hdr.desc), 0, 0, NULL },
   { "pooltype", store_strname, ITEM(res_pool.pool_type), 0, ITEM_REQUIRED, NULL },
   { "labelformat", store_strname, ITEM(res_pool.label_format), 0, 0, NULL },
   { "labeltype", store_label, ITEM(res_pool.LabelType), 0, 0, NULL },
   { "cleaningprefix", store_strname, ITEM(res_pool.cleaning_prefix), 0, ITEM_DEFAULT, "CLN" },
   { "usecatalog", store_bool, ITEM(res_pool.use_catalog), 0, ITEM_DEFAULT, "true" },
   { "usevolumeonce", store_bool, ITEM(res_pool.use_volume_once), 0, ITEM_DEPRECATED, NULL },
   { "purgeoldestvolume", store_bool, ITEM(res_pool.purge_oldest_volume), 0, 0, NULL },
   { "actiononpurge", store_actiononpurge, ITEM(res_pool.action_on_purge), 0, 0, NULL },
   { "recycleoldestvolume", store_bool, ITEM(res_pool.recycle_oldest_volume), 0, 0, NULL },
   { "recyclecurrentvolume", store_bool, ITEM(res_pool.recycle_current_volume), 0, 0, NULL },
   { "maximumvolumes", store_pint32, ITEM(res_pool.max_volumes), 0, 0, NULL },
   { "maximumvolumejobs", store_pint32, ITEM(res_pool.MaxVolJobs), 0, 0, NULL },
   { "maximumvolumefiles", store_pint32, ITEM(res_pool.MaxVolFiles), 0, 0, NULL },
   { "maximumvolumebytes", store_size64, ITEM(res_pool.MaxVolBytes), 0, 0, NULL },
   { "catalogfiles", store_bool, ITEM(res_pool.catalog_files), 0, ITEM_DEFAULT, "true" },
   { "volumeretention", store_time, ITEM(res_pool.VolRetention), 0, ITEM_DEFAULT, "31536000" /* 365 days */ },
   { "volumeuseduration", store_time, ITEM(res_pool.VolUseDuration), 0, 0, NULL },
   { "migrationtime", store_time, ITEM(res_pool.MigrationTime), 0, 0, NULL },
   { "migrationhighbytes", store_size64, ITEM(res_pool.MigrationHighBytes), 0, 0, NULL },
   { "migrationlowbytes", store_size64, ITEM(res_pool.MigrationLowBytes), 0, 0, NULL },
   { "nextpool", store_res, ITEM(res_pool.NextPool), R_POOL, 0, NULL },
   { "storage", store_alist_res, ITEM(res_pool.storage), R_STORAGE, 0, NULL },
   { "autoprune", store_bool, ITEM(res_pool.AutoPrune), 0, ITEM_DEFAULT, "true" },
   { "recycle", store_bool, ITEM(res_pool.Recycle), 0, ITEM_DEFAULT, "true" },
   { "recyclepool", store_res, ITEM(res_pool.RecyclePool), R_POOL, 0, NULL },
   { "scratchpool", store_res, ITEM(res_pool.ScratchPool), R_POOL, 0, NULL },
   { "copypool", store_alist_res, ITEM(res_pool.CopyPool), R_POOL, 0, NULL },
   { "catalog", store_res, ITEM(res_pool.catalog), R_CATALOG, 0, NULL },
   { "fileretention", store_time, ITEM(res_pool.FileRetention), 0, 0, NULL },
   { "jobretention", store_time, ITEM(res_pool.JobRetention), 0, 0, NULL },
   { NULL, NULL, { 0 }, 0, 0, NULL }
};

/*
 * Counter Resource
 *
 * name handler value code flags default_value
 */
static RES_ITEM counter_items[] = {
   { "name", store_name, ITEM(res_counter.hdr.name), 0, ITEM_REQUIRED, NULL },
   { "description", store_str, ITEM(res_counter.hdr.desc), 0, 0, NULL },
   { "minimum", store_int32, ITEM(res_counter.MinValue), 0, ITEM_DEFAULT, "0" },
   { "maximum", store_pint32, ITEM(res_counter.MaxValue), 0, ITEM_DEFAULT, "2147483647" /* INT32_MAX */ },
   { "wrapcounter", store_res, ITEM(res_counter.WrapCounter), R_COUNTER, 0, NULL },
   { "catalog", store_res, ITEM(res_counter.Catalog), R_CATALOG, 0, NULL },
   { NULL, NULL, { 0 }, 0, 0, NULL }
};

/*
 * Message resource
 */
extern RES_ITEM msgs_items[];

/*
 * This is the master resource definition.
 * It must have one item for each of the resources.
 *
 *  NOTE!!! keep it in the same order as the R_codes
 *    or eliminate all resources[rindex].name
 *
 * name handler value code flags default_value
 */
RES_TABLE resources[] = {
   { "director", dir_items, R_DIRECTOR },
   { "client", cli_items, R_CLIENT },
   { "job", job_items, R_JOB },
   { "storage", store_items, R_STORAGE },
   { "catalog", cat_items, R_CATALOG },
   { "schedule", sch_items, R_SCHEDULE },
   { "fileset", fs_items, R_FILESET },
   { "pool", pool_items, R_POOL },
   { "messages", msgs_items, R_MSGS },
   { "counter", counter_items, R_COUNTER },
   { "console", con_items, R_CONSOLE },
   { "jobdefs", job_items, R_JOBDEFS },
   { "device", NULL, R_DEVICE }, /* info obtained from SD */
   { NULL, NULL, 0 }
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
 { "command", store_runscript_cmd, { (char **)&res_runscript }, SHELL_CMD, 0, NULL },
 { "console", store_runscript_cmd, { (char **)&res_runscript }, CONSOLE_CMD, 0, NULL },
 { "target", store_runscript_target, { (char **)&res_runscript }, 0, 0, NULL },
 { "runsonsuccess", store_runscript_bool, { (char **)&res_runscript.on_success }, 0, 0, NULL },
 { "runsonfailure", store_runscript_bool, { (char **)&res_runscript.on_failure }, 0, 0, NULL },
 { "failjobonerror", store_runscript_bool, { (char **)&res_runscript.fail_on_error }, 0, 0, NULL },
 { "abortjobonerror",store_runscript_bool, { (char **)&res_runscript.fail_on_error }, 0, 0, NULL },
 { "runswhen", store_runscript_when, { (char **)&res_runscript.when }, 0, 0, NULL },
 { "runsonclient", store_runscript_target, { (char **)&res_runscript }, 0, 0, NULL }, /* TODO */
 { NULL, NULL, { 0 }, 0, 0, NULL }
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
struct s_kw backupprotocols[] = {
   { "Native", PT_NATIVE },
   { "NDMP", PT_NDMP },
   { NULL, 0 }
};

/* Keywords (RHS) permitted in AuthProtocol type records
 *
 * name token
 */
struct s_kw authprotocols[] = {
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
struct s_kw authmethods[] = {
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
struct s_jt migtypes[] = {
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

char *CATRES::display(POOLMEM *dst) {
   Mmsg(dst,"catalog=%s\ndb_name=%s\ndb_driver=%s\ndb_user=%s\n"
        "db_password=%s\ndb_address=%s\ndb_port=%i\n"
        "db_socket=%s\n",
        name(), NPRTB(db_name),
        NPRTB(db_driver), NPRTB(db_user), NPRTB(db_password),
        NPRTB(db_address), db_port, NPRTB(db_socket));
   return dst;
}

const char *level_to_str(int level)
{
   int i;
   static char level_no[30];
   const char *str = level_no;

   bsnprintf(level_no, sizeof(level_no), "%c (%d)", level, level); /* default if not found */
   for (i = 0; joblevels[i].level_name; i++) {
      if (level == (int)joblevels[i].level) {
         str = joblevels[i].level_name;
         break;
      }
   }
   return str;
}

/* Dump contents of resource */
void dump_resource(int type, RES *reshdr, void sendit(void *sock, const char *fmt, ...), void *sock)
{
   URES *res = (URES *)reshdr;
   bool recurse = true;
   char ed1[100], ed2[100], ed3[100], ed4[100], ed5[100];
   DEVICERES *dev;
   UAContext *ua = (UAContext *)sock;

   if (res == NULL) {
      sendit(sock, _("No %s resource defined\n"), res_to_str(type));
      return;
   }
   if (type < 0) { /* no recursion */
      type = - type;
      recurse = false;
   }
   switch (type) {
   case R_DIRECTOR:
      sendit(sock, _("Director: name=%s MaxJobs=%d FDtimeout=%s SDtimeout=%s \n"
                     "   subscriptions=%d, subscriptions_used=%d\n"),
             reshdr->name,
             res->res_dir.MaxConcurrentJobs,
             edit_uint64(res->res_dir.FDConnectTimeout, ed1),
             edit_uint64(res->res_dir.SDConnectTimeout, ed2),
             res->res_dir.subscriptions,
             res->res_dir.subscriptions_used);
      if (res->res_dir.query_file) {
         sendit(sock, _("   query_file=%s\n"), res->res_dir.query_file);
      }
      if (res->res_dir.messages) {
         sendit(sock, _("  --> "));
         dump_resource(-R_MSGS, (RES *)res->res_dir.messages, sendit, sock);
      }

      break;
   case R_CONSOLE:
      sendit(sock, _("Console: name=%s SSL=%d\n"),
             res->res_con.hdr.name, res->res_con.tls_enable);
      break;
   case R_COUNTER:
      if (res->res_counter.WrapCounter) {
         sendit(sock, _("Counter: name=%s min=%d max=%d cur=%d wrapcntr=%s\n"),
                res->res_counter.hdr.name, res->res_counter.MinValue,
                res->res_counter.MaxValue, res->res_counter.CurrentValue,
                res->res_counter.WrapCounter->hdr.name);
      } else {
         sendit(sock, _("Counter: name=%s min=%d max=%d\n"),
                res->res_counter.hdr.name, res->res_counter.MinValue,
                res->res_counter.MaxValue);
      }
      if (res->res_counter.Catalog) {
         sendit(sock, _("  --> "));
         dump_resource(-R_CATALOG, (RES *)res->res_counter.Catalog, sendit, sock);
      }
      break;

   case R_CLIENT:
      if (!ua || acl_access_ok(ua, Client_ACL, res->res_client.hdr.name)) {
         sendit(sock, _("Client: name=%s protocol=%d authtype=%d address=%s FDport=%d MaxJobs=%u\n"),
                res->res_client.hdr.name, res->res_client.Protocol, res->res_client.AuthType,
                res->res_client.address, res->res_client.FDport, res->res_client.MaxConcurrentJobs);
         sendit(sock, _("      JobRetention=%s FileRetention=%s AutoPrune=%d"
                        " SoftQuota=%s SoftQuotaGrace=%s HardQuota=%s StrictQuotas=%d\n"),
                edit_utime(res->res_client.JobRetention, ed1, sizeof(ed1)),
                edit_utime(res->res_client.FileRetention, ed2, sizeof(ed2)),
                res->res_client.AutoPrune,
                edit_uint64(res->res_client.SoftQuota, ed3),
                edit_utime(res->res_client.SoftQuotaGracePeriod, ed4, sizeof(ed4)),
                edit_uint64(res->res_client.HardQuota, ed5),
                res->res_client.StrictQuotas);
         if (res->res_client.max_bandwidth) {
            sendit(sock, _("     MaximumBandwidth=%lld\n"),
                   res->res_client.max_bandwidth);
         }
         if (res->res_client.catalog) {
            sendit(sock, _("  --> "));
            dump_resource(-R_CATALOG, (RES *)res->res_client.catalog, sendit, sock);
         }
      }
      break;

   case R_DEVICE:
      dev = &res->res_dev;
      char ed1[50];
      sendit(sock, _("Device: name=%s ok=%d num_writers=%d max_writers=%d\n"
                     "      reserved=%d open=%d append=%d read=%d labeled=%d offline=%d autochgr=%d\n"
                     "      poolid=%s volname=%s MediaType=%s\n"),
             dev->hdr.name, dev->found, dev->num_writers, dev->max_writers,
             dev->reserved, dev->open, dev->append, dev->read, dev->labeled,
             dev->offline, dev->autochanger,
             edit_uint64(dev->PoolId, ed1),
             dev->VolumeName, dev->MediaType);
      break;

   case R_STORAGE:
      if (!ua || acl_access_ok(ua, Storage_ACL, res->res_store.hdr.name)) {
         sendit(sock, _("Storage: name=%s protocol=%d authtype=%d address=%s SDport=%d MaxJobs=%u\n"
                        "      DeviceName=%s MediaType=%s PairedStorage=%s StorageId=%s\n"),
                res->res_store.hdr.name, res->res_store.Protocol, res->res_store.AuthType,
                res->res_store.address, res->res_store.SDport, res->res_store.MaxConcurrentJobs,
                res->res_store.dev_name(), res->res_store.media_type,
               (res->res_store.paired_storage) ? res->res_store.paired_storage->hdr.name : "None",
                edit_int64(res->res_store.StorageId, ed1));
      }
      break;

   case R_CATALOG:
      if (!ua || acl_access_ok(ua, Catalog_ACL, res->res_cat.hdr.name)) {
         sendit(sock, _("Catalog: name=%s address=%s DBport=%d db_name=%s\n"
                        "      db_driver=%s db_user=%s MutliDBConn=%d\n"),
                res->res_cat.hdr.name, NPRT(res->res_cat.db_address),
                res->res_cat.db_port, res->res_cat.db_name,
                NPRT(res->res_cat.db_driver), NPRT(res->res_cat.db_user),
                res->res_cat.mult_db_connections);
      }
      break;

   case R_JOB:
   case R_JOBDEFS:
      if (!ua || acl_access_ok(ua, Job_ACL, res->res_job.hdr.name)) {
         sendit(sock, _("%s: name=%s JobType=%d protocol=%d level=%s Priority=%d Enabled=%d\n"),
                type == R_JOB ? _("Job") : _("JobDefs"),
                res->res_job.hdr.name, res->res_job.JobType, res->res_job.Protocol,
                level_to_str(res->res_job.JobLevel), res->res_job.Priority,
                res->res_job.enabled);
         sendit(sock, _("     MaxJobs=%u Resched=%d Times=%d Interval=%s Spool=%d\n"),
                res->res_job.MaxConcurrentJobs,
                res->res_job.RescheduleOnError, res->res_job.RescheduleTimes,
                edit_uint64_with_commas(res->res_job.RescheduleInterval, ed1),
                res->res_job.spool_data);
         if (res->res_job.spool_size) {
            sendit(sock, _("     SpoolSize=%s\n"),        edit_uint64(res->res_job.spool_size, ed1));
         }
         if (res->res_job.JobType == JT_BACKUP) {
            sendit(sock, _("     Accurate=%d\n"), res->res_job.accurate);
         }
         if (res->res_job.max_bandwidth) {
            sendit(sock, _("     MaximumBandwidth=%lld\n"),
                   res->res_job.max_bandwidth);
         }
         if (res->res_job.JobType == JT_MIGRATE || res->res_job.JobType == JT_COPY) {
            sendit(sock, _("     SelectionType=%d\n"), res->res_job.selection_type);
         }
         if (res->res_job.client) {
            sendit(sock, _("  --> "));
            dump_resource(-R_CLIENT, (RES *)res->res_job.client, sendit, sock);
         }
         if (res->res_job.fileset) {
            sendit(sock, _("  --> "));
            dump_resource(-R_FILESET, (RES *)res->res_job.fileset, sendit, sock);
         }
         if (res->res_job.schedule) {
            sendit(sock, _("  --> "));
            dump_resource(-R_SCHEDULE, (RES *)res->res_job.schedule, sendit, sock);
         }
         if (res->res_job.RestoreWhere && !res->res_job.RegexWhere) {
              sendit(sock, _("  --> Where=%s\n"), NPRT(res->res_job.RestoreWhere));
         }
         if (res->res_job.RegexWhere) {
              sendit(sock, _("  --> RegexWhere=%s\n"), NPRT(res->res_job.RegexWhere));
         }
         if (res->res_job.RestoreBootstrap) {
            sendit(sock, _("  --> Bootstrap=%s\n"), NPRT(res->res_job.RestoreBootstrap));
         }
         if (res->res_job.WriteBootstrap) {
            sendit(sock, _("  --> WriteBootstrap=%s\n"), NPRT(res->res_job.WriteBootstrap));
         }
         if (res->res_job.PluginOptions) {
            sendit(sock, _("  --> PluginOptions=%s\n"), NPRT(res->res_job.PluginOptions));
         }
         if (res->res_job.MaxRunTime) {
            sendit(sock, _("  --> MaxRunTime=%u\n"), res->res_job.MaxRunTime);
         }
         if (res->res_job.MaxWaitTime) {
            sendit(sock, _("  --> MaxWaitTime=%u\n"), res->res_job.MaxWaitTime);
         }
         if (res->res_job.MaxStartDelay) {
            sendit(sock, _("  --> MaxStartDelay=%u\n"), res->res_job.MaxStartDelay);
         }
         if (res->res_job.MaxRunSchedTime) {
            sendit(sock, _("  --> MaxRunSchedTime=%u\n"), res->res_job.MaxRunSchedTime);
         }
         if (res->res_job.storage) {
            STORERES *store;
            foreach_alist(store, res->res_job.storage) {
               sendit(sock, _("  --> "));
               dump_resource(-R_STORAGE, (RES *)store, sendit, sock);
            }
         }
         if (res->res_job.base) {
            JOBRES *job;
            foreach_alist(job, res->res_job.base) {
               sendit(sock, _("  --> Base %s\n"), job->name());
            }
         }
         if (res->res_job.RunScripts) {
           RUNSCRIPT *script;
           foreach_alist(script, res->res_job.RunScripts) {
              sendit(sock, _(" --> RunScript\n"));
              sendit(sock, _("  --> Command=%s\n"), NPRT(script->command));
              sendit(sock, _("  --> Target=%s\n"),  NPRT(script->target));
              sendit(sock, _("  --> RunOnSuccess=%u\n"),  script->on_success);
              sendit(sock, _("  --> RunOnFailure=%u\n"),  script->on_failure);
              sendit(sock, _("  --> FailJobOnError=%u\n"),  script->fail_on_error);
              sendit(sock, _("  --> RunWhen=%u\n"),  script->when);
           }
         }
         if (res->res_job.pool) {
            sendit(sock, _("  --> "));
            dump_resource(-R_POOL, (RES *)res->res_job.pool, sendit, sock);
         }
         if (res->res_job.full_pool) {
            sendit(sock, _("  --> "));
            dump_resource(-R_POOL, (RES *)res->res_job.full_pool, sendit, sock);
         }
         if (res->res_job.inc_pool) {
            sendit(sock, _("  --> "));
            dump_resource(-R_POOL, (RES *)res->res_job.inc_pool, sendit, sock);
         }
         if (res->res_job.diff_pool) {
            sendit(sock, _("  --> "));
            dump_resource(-R_POOL, (RES *)res->res_job.diff_pool, sendit, sock);
         }
         if (res->res_job.next_pool) {
            sendit(sock, _("  --> "));
            dump_resource(-R_POOL, (RES *)res->res_job.next_pool, sendit, sock);
         }
         if (res->res_job.verify_job) {
            sendit(sock, _("  --> "));
            dump_resource(-type, (RES *)res->res_job.verify_job, sendit, sock);
         }
         if (res->res_job.run_cmds) {
            char *runcmd;
            foreach_alist(runcmd, res->res_job.run_cmds) {
               sendit(sock, _("  --> Run=%s\n"), runcmd);
            }
         }
         if (res->res_job.selection_pattern) {
            sendit(sock, _("  --> SelectionPattern=%s\n"), NPRT(res->res_job.selection_pattern));
         }
         if (res->res_job.messages) {
            sendit(sock, _("  --> "));
            dump_resource(-R_MSGS, (RES *)res->res_job.messages, sendit, sock);
         }
      }
      break;

   case R_FILESET:
   {
      int i, j, k;
      if (!ua || acl_access_ok(ua, FileSet_ACL, res->res_fs.hdr.name)) {
         sendit(sock, _("FileSet: name=%s\n"), res->res_fs.hdr.name);
         for (i = 0;  i < res->res_fs.num_includes; i++) {
            INCEXE *incexe = res->res_fs.include_items[i];
            for (j = 0; j < incexe->num_opts; j++) {
               FOPTS *fo = incexe->opts_list[j];
               sendit(sock, "      O %s\n", fo->opts);

               bool enhanced_wild = false;
               for (k = 0; fo->opts[k] != '\0'; k++) {
                  if (fo->opts[k]=='W') {
                     enhanced_wild = true;
                     break;
                  }
               }

               for (k = 0; k < fo->regex.size(); k++) {
                  sendit(sock, "      R %s\n", fo->regex.get(k));
               }
               for (k = 0; k < fo->regexdir.size(); k++) {
                  sendit(sock, "      RD %s\n", fo->regexdir.get(k));
               }
               for (k = 0; k < fo->regexfile.size(); k++) {
                  sendit(sock, "      RF %s\n", fo->regexfile.get(k));
               }
               for (k = 0; k < fo->wild.size(); k++) {
                  sendit(sock, "      W %s\n", fo->wild.get(k));
               }
               for (k = 0; k < fo->wilddir.size(); k++) {
                  sendit(sock, "      WD %s\n", fo->wilddir.get(k));
               }
               for (k = 0; k < fo->wildfile.size(); k++) {
                  sendit(sock, "      WF %s\n", fo->wildfile.get(k));
               }
               for (k = 0; k < fo->wildbase.size(); k++) {
                  sendit(sock, "      W%c %s\n", enhanced_wild ? 'B' : 'F', fo->wildbase.get(k));
               }
               for (k = 0; k < fo->base.size(); k++) {
                  sendit(sock, "      B %s\n", fo->base.get(k));
               }
               for (k = 0; k < fo->fstype.size(); k++) {
                  sendit(sock, "      X %s\n", fo->fstype.get(k));
               }
               for (k = 0; k < fo->drivetype.size(); k++) {
                  sendit(sock, "      XD %s\n", fo->drivetype.get(k));
               }
               if (fo->plugin) {
                  sendit(sock, "      G %s\n", fo->plugin);
               }
               if (fo->reader) {
                  sendit(sock, "      D %s\n", fo->reader);
               }
               if (fo->writer) {
                  sendit(sock, "      T %s\n", fo->writer);
               }
               sendit(sock, "      N\n");
            }
            for (j = 0; i < incexe->ignoredir.size(); i++) {
               sendit(sock, "      Z %s\n", incexe->ignoredir.get(j));
            }
            for (j = 0; j < incexe->name_list.size(); j++) {
               sendit(sock, "      I %s\n", incexe->name_list.get(j));
            }
            if (incexe->name_list.size()) {
               sendit(sock, "      N\n");
            }
            for (j = 0; j < incexe->plugin_list.size(); j++) {
               sendit(sock, "      P %s\n", incexe->plugin_list.get(j));
            }
            if (incexe->plugin_list.size()) {
               sendit(sock, "      N\n");
            }
         }

         for (i = 0; i < res->res_fs.num_excludes; i++) {
            INCEXE *incexe = res->res_fs.exclude_items[i];
            for (j = 0; j < incexe->name_list.size(); j++) {
               sendit(sock, "      E %s\n", incexe->name_list.get(j));
            }
            if (incexe->name_list.size()) {
               sendit(sock, "      N\n");
            }
         }
      }
      break;
   }

   case R_SCHEDULE:
      if (!ua || acl_access_ok(ua, Schedule_ACL, res->res_sch.hdr.name)) {
         if (res->res_sch.run) {
            int i;
            RUNRES *run = res->res_sch.run;
            char buf[1000], num[30];
            sendit(sock, _("Schedule: name=%s\n"), res->res_sch.hdr.name);
            if (!run) {
               break;
            }

next_run:
            sendit(sock, _("  --> Run Level=%s\n"), level_to_str(run->level));
            bstrncpy(buf, _("      hour="), sizeof(buf));
            for (i = 0; i < 24; i++) {
               if (bit_is_set(i, run->hour)) {
                  bsnprintf(num, sizeof(num), "%d ", i);
                  bstrncat(buf, num, sizeof(buf));
               }
            }
            bstrncat(buf, "\n", sizeof(buf));
            sendit(sock, buf);
            bstrncpy(buf, _("      mday="), sizeof(buf));
            for (i = 0; i < 31; i++) {
               if (bit_is_set(i, run->mday)) {
                  bsnprintf(num, sizeof(num), "%d ", i);
                  bstrncat(buf, num, sizeof(buf));
               }
            }
            bstrncat(buf, "\n", sizeof(buf));
            sendit(sock, buf);
            bstrncpy(buf, _("      month="), sizeof(buf));
            for (i = 0; i < 12; i++) {
               if (bit_is_set(i, run->month)) {
                  bsnprintf(num, sizeof(num), "%d ", i);
                  bstrncat(buf, num, sizeof(buf));
               }
            }
            bstrncat(buf, "\n", sizeof(buf));
            sendit(sock, buf);
            bstrncpy(buf, _("      wday="), sizeof(buf));
            for (i = 0; i < 7; i++) {
               if (bit_is_set(i, run->wday)) {
                  bsnprintf(num, sizeof(num), "%d ", i);
                  bstrncat(buf, num, sizeof(buf));
               }
            }
            bstrncat(buf, "\n", sizeof(buf));
            sendit(sock, buf);
            bstrncpy(buf, _("      wom="), sizeof(buf));
            for (i = 0; i < 5; i++) {
               if (bit_is_set(i, run->wom)) {
                  bsnprintf(num, sizeof(num), "%d ", i);
                  bstrncat(buf, num, sizeof(buf));
               }
            }
            bstrncat(buf, "\n", sizeof(buf));
            sendit(sock, buf);
            bstrncpy(buf, _("      woy="), sizeof(buf));
            for (i = 0; i < 54; i++) {
               if (bit_is_set(i, run->woy)) {
                  bsnprintf(num, sizeof(num), "%d ", i);
                  bstrncat(buf, num, sizeof(buf));
               }
            }
            bstrncat(buf, "\n", sizeof(buf));
            sendit(sock, buf);
            sendit(sock, _("      mins=%d\n"), run->minute);
            if (run->pool) {
               sendit(sock, _("     --> "));
               dump_resource(-R_POOL, (RES *)run->pool, sendit, sock);
            }
            if (run->storage) {
               sendit(sock, _("     --> "));
               dump_resource(-R_STORAGE, (RES *)run->storage, sendit, sock);
            }
            if (run->msgs) {
               sendit(sock, _("     --> "));
               dump_resource(-R_MSGS, (RES *)run->msgs, sendit, sock);
            }
            /* If another Run record is chained in, go print it */
            if (run->next) {
               run = run->next;
               goto next_run;
            }
         } else {
            sendit(sock, _("Schedule: name=%s\n"), res->res_sch.hdr.name);
         }
      }
      break;

   case R_POOL:
      if (!ua || acl_access_ok(ua, Pool_ACL, res->res_pool.hdr.name)) {
         sendit(sock, _("Pool: name=%s PoolType=%s\n"), res->res_pool.hdr.name,
                res->res_pool.pool_type);
         sendit(sock, _("      use_cat=%d use_once=%d cat_files=%d\n"),
                res->res_pool.use_catalog, res->res_pool.use_volume_once,
                res->res_pool.catalog_files);
         sendit(sock, _("      max_vols=%d auto_prune=%d VolRetention=%s\n"),
                res->res_pool.max_volumes, res->res_pool.AutoPrune,
                edit_utime(res->res_pool.VolRetention, ed1, sizeof(ed1)));
         sendit(sock, _("      VolUse=%s recycle=%d LabelFormat=%s\n"),
                edit_utime(res->res_pool.VolUseDuration, ed1, sizeof(ed1)),
                res->res_pool.Recycle,
                NPRT(res->res_pool.label_format));
         sendit(sock, _("      CleaningPrefix=%s LabelType=%d\n"),
                 NPRT(res->res_pool.cleaning_prefix), res->res_pool.LabelType);
         sendit(sock, _("      RecyleOldest=%d PurgeOldest=%d ActionOnPurge=%d\n"),
                res->res_pool.recycle_oldest_volume,
                res->res_pool.purge_oldest_volume,
                res->res_pool.action_on_purge);
         sendit(sock, _("      MaxVolJobs=%d MaxVolFiles=%d MaxVolBytes=%s\n"),
                res->res_pool.MaxVolJobs,
                res->res_pool.MaxVolFiles,
                edit_uint64(res->res_pool.MaxVolBytes, ed1));
         sendit(sock, _("      MigTime=%s MigHiBytes=%s MigLoBytes=%s\n"),
                edit_utime(res->res_pool.MigrationTime, ed1, sizeof(ed1)),
                edit_uint64(res->res_pool.MigrationHighBytes, ed2),
                edit_uint64(res->res_pool.MigrationLowBytes, ed3));
         sendit(sock, _("      JobRetention=%s FileRetention=%s\n"),
                edit_utime(res->res_pool.JobRetention, ed1, sizeof(ed1)),
                edit_utime(res->res_pool.FileRetention, ed2, sizeof(ed2)));
         if (res->res_pool.NextPool) {
            sendit(sock, _("      NextPool=%s\n"), res->res_pool.NextPool->name());
         }
         if (res->res_pool.RecyclePool) {
            sendit(sock, _("      RecyclePool=%s\n"), res->res_pool.RecyclePool->name());
         }
         if (res->res_pool.ScratchPool) {
            sendit(sock, _("      ScratchPool=%s\n"), res->res_pool.ScratchPool->name());
         }
         if (res->res_pool.catalog) {
            sendit(sock, _("      Catalog=%s\n"), res->res_pool.catalog->name());
         }
         if (res->res_pool.storage) {
            STORERES *store;
            foreach_alist(store, res->res_pool.storage) {
               sendit(sock, _("  --> "));
               dump_resource(-R_STORAGE, (RES *)store, sendit, sock);
            }
         }
         if (res->res_pool.CopyPool) {
            POOLRES *copy;
            foreach_alist(copy, res->res_pool.CopyPool) {
               sendit(sock, _("  --> "));
               dump_resource(-R_POOL, (RES *)copy, sendit, sock);
            }
         }
      }
      break;

   case R_MSGS:
      sendit(sock, _("Messages: name=%s\n"), res->res_msgs.hdr.name);
      if (res->res_msgs.mail_cmd)
         sendit(sock, _("      mailcmd=%s\n"), res->res_msgs.mail_cmd);
      if (res->res_msgs.operator_cmd)
         sendit(sock, _("      opcmd=%s\n"), res->res_msgs.operator_cmd);
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
         free((char *)res->res_dir.scripts_directory);
      }
      if (res->res_dir.plugin_directory) {
         free((char *)res->res_dir.plugin_directory);
      }
      if (res->res_dir.pid_directory) {
         free(res->res_dir.pid_directory);
      }
      if (res->res_dir.subsys_directory) {
         free(res->res_dir.subsys_directory);
      }
      if (res->res_dir.password) {
         free(res->res_dir.password);
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
      if (res->res_dir.keyencrkey) {
         free(res->res_dir.keyencrkey);
      }
      break;
   case R_DEVICE:
   case R_COUNTER:
       break;
   case R_CONSOLE:
      if (res->res_con.password) {
         free(res->res_con.password);
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
      if (res->res_client.password) {
         free(res->res_client.password);
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
      if (res->res_store.password) {
         free(res->res_store.password);
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
      if (res->res_cat.db_password) {
         free(res->res_cat.db_password);
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
   case R_JOB:
   case R_JOBDEFS:
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
   int rindex = type - r_first;
   int i, size = 0;
   bool error = false;

   /*
    * Check Job requirements after applying JobDefs
    */
   if (type != R_JOB && type != R_JOBDEFS) {
      /*
       * Ensure that all required items are present
       */
      for (i = 0; items[i].name; i++) {
         if (items[i].flags & ITEM_REQUIRED) {
            if (!bit_is_set(i, res_all.res_dir.hdr.item_present)) {
                Emsg2(M_ERROR_TERM, 0, _("%s item is required in %s resource, but not found.\n"),
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
      if (items[0].flags & ITEM_REQUIRED) {
         if (!bit_is_set(0, res_all.res_dir.hdr.item_present)) {
             Emsg2(M_ERROR_TERM, 0, _("%s item is required in %s resource, but not found.\n"),
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
      /*
       * Resources not containing a resource
       */
      case R_CATALOG:
      case R_MSGS:
      case R_FILESET:
      case R_DEVICE:
         break;
      /*
       * Resources containing another resource or alist. First
       * look up the resource which contains another resource. It
       * was written during pass 1.  Then stuff in the pointers to
       * the resources it contains, which were inserted this pass.
       * Finally, it will all be stored back.
       */
      case R_POOL:
         /*
          * Find resource saved in pass 1
          */
         if ((res = (URES *)GetResWithName(R_POOL, res_all.res_con.hdr.name)) == NULL) {
            Emsg1(M_ERROR_TERM, 0, _("Cannot find Pool resource %s\n"), res_all.res_con.hdr.name);
         }
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
         break;
      case R_CONSOLE:
         if ((res = (URES *)GetResWithName(R_CONSOLE, res_all.res_con.hdr.name)) == NULL) {
            Emsg1(M_ERROR_TERM, 0, _("Cannot find Console resource %s\n"), res_all.res_con.hdr.name);
         }
         res->res_con.tls_allowed_cns = res_all.res_con.tls_allowed_cns;
         break;
      case R_DIRECTOR:
         if ((res = (URES *)GetResWithName(R_DIRECTOR, res_all.res_dir.hdr.name)) == NULL) {
            Emsg1(M_ERROR_TERM, 0, _("Cannot find Director resource %s\n"), res_all.res_dir.hdr.name);
         }
         res->res_dir.messages = res_all.res_dir.messages;
         res->res_dir.tls_allowed_cns = res_all.res_dir.tls_allowed_cns;
         break;
      case R_STORAGE:
         if ((res = (URES *)GetResWithName(type, res_all.res_store.hdr.name)) == NULL) {
            Emsg1(M_ERROR_TERM, 0, _("Cannot find Storage resource %s\n"),
                  res_all.res_dir.hdr.name);
         }
         res->res_store.paired_storage = res_all.res_store.paired_storage;

         /*
          * We must explicitly copy the device alist pointer
          */
         res->res_store.device = res_all.res_store.device;
         break;
      case R_JOB:
      case R_JOBDEFS:
         if ((res = (URES *)GetResWithName(type, res_all.res_dir.hdr.name)) == NULL) {
            Emsg1(M_ERROR_TERM, 0, _("Cannot find Job resource %s\n"),
                  res_all.res_dir.hdr.name);
         }
         res->res_job.messages = res_all.res_job.messages;
         res->res_job.schedule = res_all.res_job.schedule;
         res->res_job.client = res_all.res_job.client;
         res->res_job.fileset = res_all.res_job.fileset;
         res->res_job.storage = res_all.res_job.storage;
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
          * TODO: JobDefs where/regexwhere doesn't work well (but this
          * is not very useful)
          * We have to set_bit(index, res_all.hdr.item_present);
          * or something like that
          */
         /*
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
         break;
      case R_COUNTER:
         if ((res = (URES *)GetResWithName(R_COUNTER, res_all.res_counter.hdr.name)) == NULL) {
            Emsg1(M_ERROR_TERM, 0, _("Cannot find Counter resource %s\n"), res_all.res_counter.hdr.name);
         }
         res->res_counter.Catalog = res_all.res_counter.Catalog;
         res->res_counter.WrapCounter = res_all.res_counter.WrapCounter;
         break;
      case R_CLIENT:
         if ((res = (URES *)GetResWithName(R_CLIENT, res_all.res_client.hdr.name)) == NULL) {
            Emsg1(M_ERROR_TERM, 0, _("Cannot find Client resource %s\n"), res_all.res_client.hdr.name);
         }
         if (res_all.res_client.catalog) {
            res->res_client.catalog = res_all.res_client.catalog;
         } else {
            /*
             * No catalog overwrite given use the first catalog definition.
             */
            res->res_client.catalog = (CATRES *)GetNextRes(R_CATALOG, NULL);
         }
         res->res_client.tls_allowed_cns = res_all.res_client.tls_allowed_cns;
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
         }
         res->res_sch.run = res_all.res_sch.run;
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
    * The following code is only executed during pass 1
    */
   switch (type) {
   case R_DIRECTOR:
      size = sizeof(DIRRES);
      break;
   case R_CONSOLE:
      size = sizeof(CONRES);
      break;
   case R_CLIENT:
      size =sizeof(CLIENTRES);
      break;
   case R_STORAGE:
      size = sizeof(STORERES);
      break;
   case R_CATALOG:
      size = sizeof(CATRES);
      break;
   case R_JOB:
   case R_JOBDEFS:
      size = sizeof(JOBRES);
      break;
   case R_FILESET:
      size = sizeof(FILESETRES);
      break;
   case R_SCHEDULE:
      size = sizeof(SCHEDRES);
      break;
   case R_POOL:
      size = sizeof(POOLRES);
      break;
   case R_MSGS:
      size = sizeof(MSGSRES);
      break;
   case R_COUNTER:
      size = sizeof(COUNTERRES);
      break;
   case R_DEVICE:
      error = true;
      break;
   default:
      printf(_("Unknown resource type %d in save_resource.\n"), type);
      error = true;
      break;
   }
   /*
    * Common
    */
   if (!error) {
      res = (URES *)malloc(size);
      memcpy(res, &res_all, size);
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

static void store_actiononpurge(LEX *lc, RES_ITEM *item, int index, int pass)
{
   int i;
   uint32_t *destination = (uint32_t*)item->value;

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
}

/*
 * Store Device. Note, the resource is created upon the
 * first reference. The details of the resource are obtained
 * later from the SD.
 */
static void store_device(LEX *lc, RES_ITEM *item, int index, int pass)
{
   URES *res;
   int rindex = R_DEVICE - r_first;
   int size = sizeof(DEVICERES);
   bool found = false;

   if (pass == 1) {
      lex_get_token(lc, T_NAME);
      if (!res_head[rindex]) {
         res = (URES *)malloc(size);
         memset(res, 0, size);
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
            res = (URES *)malloc(size);
            memset(res, 0, size);
            res->res_dev.hdr.name = bstrdup(lc->str);
            next->next = (RES *)res;
            Dmsg4(900, "Inserting %s res: %s index=%d pass=%d\n", res_to_str(R_DEVICE),
               res->res_dir.hdr.name, rindex, pass);
         }
      }

      scan_to_eol(lc);
      set_bit(index, res_all.hdr.item_present);
   } else {
      store_alist_res(lc, item, index, pass);
   }
}

/*
 * Store Migration/Copy type
 */
void store_migtype(LEX *lc, RES_ITEM *item, int index, int pass)
{
   int i;

   lex_get_token(lc, T_NAME);
   /*
    * Store the type both pass 1 and pass 2
    */
   for (i = 0; migtypes[i].type_name; i++) {
      if (bstrcasecmp(lc->str, migtypes[i].type_name)) {
         *(uint32_t *)(item->value) = migtypes[i].job_type;
         i = 0;
         break;
      }
   }
   if (i != 0) {
      scan_err1(lc, _("Expected a Migration Job Type keyword, got: %s"), lc->str);
   }
   scan_to_eol(lc);
   set_bit(index, res_all.hdr.item_present);
}

/*
 * Store JobType (backup, verify, restore)
 */
void store_jobtype(LEX *lc, RES_ITEM *item, int index, int pass)
{
   int i;

   lex_get_token(lc, T_NAME);
   /*
    * Store the type both pass 1 and pass 2
    */
   for (i = 0; jobtypes[i].type_name; i++) {
      if (bstrcasecmp(lc->str, jobtypes[i].type_name)) {
         *(uint32_t *)(item->value) = jobtypes[i].job_type;
         i = 0;
         break;
      }
   }
   if (i != 0) {
      scan_err1(lc, _("Expected a Job Type keyword, got: %s"), lc->str);
   }
   scan_to_eol(lc);
   set_bit(index, res_all.hdr.item_present);
}

/*
 * Store Protocol (Native, NDMP)
 */
void store_protocoltype(LEX *lc, RES_ITEM *item, int index, int pass)
{
   int i;

   lex_get_token(lc, T_NAME);
   /*
    * Store the type both pass 1 and pass 2
    */
   for (i = 0; backupprotocols[i].name; i++) {
      if (bstrcasecmp(lc->str, backupprotocols[i].name)) {
         *(uint32_t *)(item->value) = backupprotocols[i].token;
         i = 0;
         break;
      }
   }
   if (i != 0) {
      scan_err1(lc, _("Expected a Protocol Type keyword, got: %s"), lc->str);
   }
   scan_to_eol(lc);
   set_bit(index, res_all.hdr.item_present);
}

void store_replace(LEX *lc, RES_ITEM *item, int index, int pass)
{
   int i;
   lex_get_token(lc, T_NAME);
   /*
    * Scan Replacement options
    */
   for (i = 0; ReplaceOptions[i].name; i++) {
      if (bstrcasecmp(lc->str, ReplaceOptions[i].name)) {
         *(uint32_t *)(item->value) = ReplaceOptions[i].token;
         i = 0;
         break;
      }
   }
   if (i != 0) {
      scan_err1(lc, _("Expected a Restore replacement option, got: %s"), lc->str);
   }
   scan_to_eol(lc);
   set_bit(index, res_all.hdr.item_present);
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
         *(uint32_t *)(item->value) = authprotocols[i].token;
         i = 0;
         break;
      }
   }
   if (i != 0) {
      scan_err1(lc, _("Expected a Auth Protocol Type keyword, got: %s"), lc->str);
   }
   scan_to_eol(lc);
   set_bit(index, res_all.hdr.item_present);
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
         *(uint32_t *)(item->value) = authmethods[i].token;
         i = 0;
         break;
      }
   }
   if (i != 0) {
      scan_err1(lc, _("Expected a Authentication Type keyword, got: %s"), lc->str);
   }
   scan_to_eol(lc);
   set_bit(index, res_all.hdr.item_present);
}

/*
 * Store Job Level (Full, Incremental, ...)
 */
void store_level(LEX *lc, RES_ITEM *item, int index, int pass)
{
   int i;

   lex_get_token(lc, T_NAME);
   /*
    * Store the level pass 2 so that type is defined
    */
   for (i = 0; joblevels[i].level_name; i++) {
      if (bstrcasecmp(lc->str, joblevels[i].level_name)) {
         *(uint32_t *)(item->value) = joblevels[i].level;
         i = 0;
         break;
      }
   }
   if (i != 0) {
      scan_err1(lc, _("Expected a Job Level keyword, got: %s"), lc->str);
   }
   scan_to_eol(lc);
   set_bit(index, res_all.hdr.item_present);
}

/*
 * Store password either clear if for NDMP and catalog or MD5 hashed for native.
 */
static void store_clearpassword(LEX *lc, RES_ITEM *item, int index, int pass)
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
         store_str(lc, item, index, pass);
         break;
      default:
         store_password(lc, item, index, pass);
         break;
      }
      break;
   case R_CLIENT:
      switch (res_all.res_client.Protocol) {
      case APT_NDMPV2:
      case APT_NDMPV3:
      case APT_NDMPV4:
         store_str(lc, item, index, pass);
         break;
      default:
         store_password(lc, item, index, pass);
         break;
      }
      break;
   case R_STORAGE:
      switch (res_all.res_store.Protocol) {
      case APT_NDMPV2:
      case APT_NDMPV3:
      case APT_NDMPV4:
         store_str(lc, item, index, pass);
         break;
      default:
         store_password(lc, item, index, pass);
         break;
      }
      break;
   case R_CATALOG:
      store_str(lc, item, index, pass);
      break;
   default:
      store_password(lc, item, index, pass);
      break;
   }
}

/*
 * Store ACL (access control list)
 */
static void store_acl(LEX *lc, RES_ITEM *item, int index, int pass)
{
   int token;

   for (;;) {
      lex_get_token(lc, T_STRING);
      if (pass == 1) {
         if (((alist **)item->value)[item->code] == NULL) {
            ((alist **)item->value)[item->code] = New(alist(10, owned_by_alist));
            Dmsg1(900, "Defined new ACL alist at %d\n", item->code);
         }
         ((alist **)item->value)[item->code]->append(bstrdup(lc->str));
         Dmsg2(900, "Appended to %d %s\n", item->code, lc->str);
      }
      token = lex_get_token(lc, T_ALL);
      if (token == T_COMMA) {
         continue; /* get another ACL */
      }
      break;
   }
   set_bit(index, res_all.hdr.item_present);
}

/*
 * Store a runscript->when in a bit field
 */
static void store_runscript_when(LEX *lc, RES_ITEM *item, int index, int pass)
{
   lex_get_token(lc, T_NAME);

   if (bstrcasecmp(lc->str, "before")) {
      *(uint32_t *)(item->value) = SCRIPT_Before ;
   } else if (bstrcasecmp(lc->str, "after")) {
      *(uint32_t *)(item->value) = SCRIPT_After;
   } else if (bstrcasecmp(lc->str, "aftervss")) {
      *(uint32_t *)(item->value) = SCRIPT_AfterVSS;
   } else if (bstrcasecmp(lc->str, "always")) {
      *(uint32_t *)(item->value) = SCRIPT_Any;
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
         ((RUNSCRIPT*) item->value)->set_target(lc->str);
      } else if (bstrcasecmp(lc->str, "yes")) {
         ((RUNSCRIPT*) item->value)->set_target("%c");
      } else if (bstrcasecmp(lc->str, "no")) {
         ((RUNSCRIPT*) item->value)->set_target("");
      } else {
         RES *res = GetResWithName(R_CLIENT, lc->str);
         if (res == NULL) {
            scan_err3(lc, _("Could not find config Resource %s referenced on line %d : %s\n"),
                      lc->str, lc->line_no, lc->line);
         }

         ((RUNSCRIPT*) item->value)->set_target(lc->str);
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
      ((RUNSCRIPT*) item->value)->commands->prepend(c); /* command line */
      ((RUNSCRIPT*) item->value)->commands->prepend((void *)(intptr_t)item->code); /* command type */
   }
   scan_to_eol(lc);
}

static void store_short_runscript(LEX *lc, RES_ITEM *item, int index, int pass)
{
   lex_get_token(lc, T_STRING);
   alist **runscripts = (alist **)(item->value) ;

   if (pass == 2) {
      RUNSCRIPT *script = new_runscript();
      script->set_job_code_callback(job_code_callback_director);

      script->set_command(lc->str);
      if (bstrcmp(item->name, "runbeforejob")) {
         script->when = SCRIPT_Before;
         script->set_target("");
      } else if (bstrcmp(item->name, "runafterjob")) {
         script->when = SCRIPT_After;
         script->on_success = true;
         script->on_failure = false;
         script->fail_on_error = false;
         script->set_target("");
      } else if (bstrcmp(item->name, "clientrunafterjob")) {
         script->when = SCRIPT_After;
         script->on_success = true;
         script->on_failure = false;
         script->fail_on_error = false;
         script->set_target("%c");
      } else if (bstrcmp(item->name, "clientrunbeforejob")) {
         script->when = SCRIPT_Before;
         script->set_target("%c");
      } else if (bstrcmp(item->name, "runafterfailedjob")) {
         script->when = SCRIPT_After;
         script->on_failure = true;
         script->on_success = false;
         script->fail_on_error = false;
         script->set_target("");
      }

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
      *(bool *)(item->value) = true;
   } else if (bstrcasecmp(lc->str, "no") || bstrcasecmp(lc->str, "false")) {
      *(bool *)(item->value) = false;
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
   alist **runscripts = (alist **)(item->value) ;

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
           runscript_items[i].handler(lc, &runscript_items[i], i, pass);
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
      while ((c=(char*)res_runscript.commands->pop()) != NULL) {
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
            return _("*none*");
         }
      } else {
         return _("*none*");
      }
      break;
   }

   return NULL;
}

/*
 * callback function for init_resource
 * See ../lib/parse_conf.c, function init_resource, for more generic handling.
 */
static void init_resource_cb(RES_ITEM *item)
{
   int i;

   if (item->handler == store_replace) {
      for (i = 0; ReplaceOptions[i].name; i++) {
         if (bstrcasecmp(item->default_value, ReplaceOptions[i].name)) {
            *(uint32_t *)(item->value) = ReplaceOptions[i].token;
         }
      }
   } else if (item->handler == store_authprotocoltype) {
      for (i = 0; authprotocols[i].name; i++) {
         if (bstrcasecmp(item->default_value, authprotocols[i].name)) {
            *(uint32_t *)(item->value) = authprotocols[i].token;
         }
      }
   } else if (item->handler == store_authtype) {
      for (i = 0; authmethods[i].name; i++) {
         if (bstrcasecmp(item->default_value, authmethods[i].name)) {
            *(uint32_t *)(item->value) = authmethods[i].token;
         }
      }
   }
}

bool parse_dir_config(CONFIG *config, const char *configfile, int exit_code)
{
   config->init(configfile, NULL, NULL, init_resource_cb, exit_code,
                (void *)&res_all, res_all_size, r_first,
                r_last, resources, res_head);
   return config->parse_config();
}
