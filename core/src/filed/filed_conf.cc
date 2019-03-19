/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2008 Free Software Foundation Europe e.V.
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
 * Kern Sibbald, September MM
 */
/**
 * @file
 * Main configuration file parser for Bareos File Daemon (Client)
 * some parts may be split into separate files such as
 * the schedule configuration (sch_config.c).
 *
 * Note, the configuration file parser consists of three parts
 *
 * 1. The generic lexical scanner in lib/lex.c and lib/lex.h
 *
 * 2. The generic config  scanner in lib/parse_config.c and
 *    lib/parse_config.h. These files contain the parser code,
 *    some utility routines, and the common store routines
 *    (name, int, string).
 *
 * 3. The daemon specific file, which contains the Resource
 *    definitions as well as any specific store routines
 *    for the resource records.
 */

#include "include/bareos.h"
#define NEED_JANSSON_NAMESPACE 1
#include "lib/output_formatter.h"
#include "filed/filed.h"
#include "filed/filed_globals.h"
#include "lib/parse_conf.h"
#include "lib/resource_item.h"
#include "lib/address_conf.h"


namespace filedaemon {

/**
 * Define the first and last resource ID record
 * types. Note, these should be unique for each
 * daemon though not a requirement.
 */
static CommonResourceHeader* sres_head[R_LAST - R_FIRST + 1];
static CommonResourceHeader** res_head = sres_head;

/**
 * Forward referenced subroutines
 */
static bool SaveResource(int type, ResourceItem* items, int pass);
static void FreeResource(CommonResourceHeader* sres, int type);
static void DumpResource(int type,
                         CommonResourceHeader* reshdr,
                         void sendit(void* sock, const char* fmt, ...),
                         void* sock,
                         bool hide_sensitive_data,
                         bool verbose);

/**
 * We build the current resource here as we are
 * scanning the resource configuration definition,
 * then move it to allocated memory when the resource
 * scan is complete.
 */
static UnionOfResources res_all;
static int32_t res_all_size = sizeof(res_all);

/* clang-format off */

/**
 * Client or File daemon "Global" resources
 */
static ResourceItem cli_items[] = {
  {"Name", CFG_TYPE_NAME, ITEM(res_client.hdr.name), 0, CFG_ITEM_REQUIRED, NULL, NULL,
      "The name of this resource. It is used to reference to it."},
  {"Description", CFG_TYPE_STR, ITEM(res_client.hdr.desc), 0, 0, NULL, NULL, NULL},
  {"FdPort", CFG_TYPE_ADDRESSES_PORT, ITEM(res_client.FDaddrs), 0, CFG_ITEM_DEFAULT, FD_DEFAULT_PORT, NULL, NULL},
  {"FdAddress", CFG_TYPE_ADDRESSES_ADDRESS, ITEM(res_client.FDaddrs), 0, CFG_ITEM_DEFAULT, FD_DEFAULT_PORT, NULL, NULL},
  {"FdAddresses", CFG_TYPE_ADDRESSES, ITEM(res_client.FDaddrs), 0, CFG_ITEM_DEFAULT, FD_DEFAULT_PORT, NULL, NULL},
  {"FdSourceAddress", CFG_TYPE_ADDRESSES_ADDRESS, ITEM(res_client.FDsrc_addr), 0, CFG_ITEM_DEFAULT, "0", NULL, NULL},
  {"WorkingDirectory", CFG_TYPE_DIR, ITEM(res_client.working_directory), 0,
      CFG_ITEM_DEFAULT | CFG_ITEM_PLATFORM_SPECIFIC, _PATH_BAREOS_WORKINGDIR, NULL, NULL},
  {"PidDirectory", CFG_TYPE_DIR, ITEM(res_client.pid_directory), 0,
      CFG_ITEM_DEFAULT | CFG_ITEM_PLATFORM_SPECIFIC, _PATH_BAREOS_PIDDIR, NULL, NULL},
  {"SubSysDirectory", CFG_TYPE_DIR, ITEM(res_client.subsys_directory), 0, CFG_ITEM_DEPRECATED, NULL, NULL, NULL},
  {"PluginDirectory", CFG_TYPE_DIR, ITEM(res_client.plugin_directory), 0, 0, NULL, NULL, NULL},
  {"PluginNames", CFG_TYPE_PLUGIN_NAMES, ITEM(res_client.plugin_names), 0, 0, NULL, NULL, NULL},
  {"ScriptsDirectory", CFG_TYPE_DIR, ITEM(res_client.scripts_directory), 0, 0, NULL, NULL, NULL},
  {"MaximumConcurrentJobs", CFG_TYPE_PINT32, ITEM(res_client.MaxConcurrentJobs), 0, CFG_ITEM_DEFAULT, "20", NULL, NULL},
  {"MaximumConnections", CFG_TYPE_PINT32, ITEM(res_client.MaxConnections), 0, CFG_ITEM_DEFAULT, "42", "15.2.3-", NULL},
  {"Messages", CFG_TYPE_RES, ITEM(res_client.messages), R_MSGS, 0, NULL, NULL, NULL},
  {"SdConnectTimeout", CFG_TYPE_TIME, ITEM(res_client.SDConnectTimeout), 0, CFG_ITEM_DEFAULT, "1800" /* 30 minutes */, NULL, NULL},
  {"HeartbeatInterval", CFG_TYPE_TIME, ITEM(res_client.heartbeat_interval), 0, CFG_ITEM_DEFAULT, "0", NULL, NULL},
  {"MaximumNetworkBufferSize", CFG_TYPE_PINT32, ITEM(res_client.max_network_buffer_size), 0, 0, NULL, NULL, NULL},
  {"PkiSignatures", CFG_TYPE_BOOL, ITEM(res_client.pki_sign), 0, CFG_ITEM_DEFAULT, "false", NULL, "Enable Data Signing."},
  {"PkiEncryption", CFG_TYPE_BOOL, ITEM(res_client.pki_encrypt), 0, CFG_ITEM_DEFAULT, "false", NULL, "Enable Data Encryption."},
  {"PkiKeyPair", CFG_TYPE_DIR, ITEM(res_client.pki_keypair_file), 0, 0, NULL, NULL,
      "File with public and private key to sign, encrypt (backup) and decrypt (restore) the data."},
  {"PkiSigner", CFG_TYPE_ALIST_DIR, ITEM(res_client.pki_signing_key_files), 0, 0, NULL, NULL,
      "Additional public/private key files to sign or verify the data."},
  {"PkiMasterKey", CFG_TYPE_ALIST_DIR, ITEM(res_client.pki_master_key_files), 0, 0, NULL, NULL,
      "List of public key files. Data will be decryptable via the corresponding private keys."},
  {"PkiCipher", CFG_TYPE_CIPHER, ITEM(res_client.pki_cipher), 0, CFG_ITEM_DEFAULT, "aes128", NULL,
      "PKI Cipher used for data encryption."},
  {"VerId", CFG_TYPE_STR, ITEM(res_client.verid), 0, 0, NULL, NULL, NULL},
  {"Compatible", CFG_TYPE_BOOL, ITEM(res_client.compatible), 0, CFG_ITEM_DEFAULT, "false", NULL, NULL},
  {"MaximumBandwidthPerJob", CFG_TYPE_SPEED, ITEM(res_client.max_bandwidth_per_job), 0, 0, NULL, NULL, NULL},
  {"AllowBandwidthBursting", CFG_TYPE_BOOL, ITEM(res_client.allow_bw_bursting), 0, CFG_ITEM_DEFAULT, "false", NULL, NULL},
  {"AllowedScriptDir", CFG_TYPE_ALIST_DIR, ITEM(res_client.allowed_script_dirs), 0, 0, NULL, NULL, NULL},
  {"AllowedJobCommand", CFG_TYPE_ALIST_STR, ITEM(res_client.allowed_job_cmds), 0, 0, NULL, NULL, NULL},
  {"AbsoluteJobTimeout", CFG_TYPE_PINT32, ITEM(res_client.jcr_watchdog_time), 0, 0, NULL, NULL, NULL},
  {"AlwaysUseLmdb", CFG_TYPE_BOOL, ITEM(res_client.always_use_lmdb), 0, CFG_ITEM_DEFAULT, "false", NULL, NULL},
  {"LmdbThreshold", CFG_TYPE_PINT32, ITEM(res_client.lmdb_threshold), 0, 0, NULL, NULL, NULL},
  {"SecureEraseCommand", CFG_TYPE_STR, ITEM(res_client.secure_erase_cmdline), 0, 0, NULL, "15.2.1-",
      "Specify command that will be called when bareos unlinks files."},
  {"LogTimestampFormat", CFG_TYPE_STR, ITEM(res_client.log_timestamp_format), 0, 0, NULL, "15.2.3-", NULL},
    TLS_COMMON_CONFIG(res_client),
    TLS_CERT_CONFIG(res_client),
  {NULL, 0, {0}, 0, 0, NULL, NULL, NULL}};

/**
 * Directors that can use our services
 */
static ResourceItem dir_items[] = {
  {"Name", CFG_TYPE_NAME, ITEM(res_dir.hdr.name), 0, CFG_ITEM_REQUIRED, NULL, NULL, NULL},
  {"Description", CFG_TYPE_STR, ITEM(res_dir.hdr.desc), 0, 0, NULL, NULL, NULL},
  {"Password", CFG_TYPE_MD5PASSWORD, ITEM(res_dir.password_), 0, CFG_ITEM_REQUIRED, NULL, NULL, NULL},
  {"Address", CFG_TYPE_STR, ITEM(res_dir.address), 0, 0, NULL, NULL,
      "Director Network Address. Only required if \"Connection From Client To Director\" is enabled."},
  {"Port", CFG_TYPE_PINT32, ITEM(res_dir.port), 0, CFG_ITEM_DEFAULT, DIR_DEFAULT_PORT, "16.2.2",
      "Director Network Port. Only used if \"Connection From Client To Director\" is enabled."},
  {"ConnectionFromDirectorToClient", CFG_TYPE_BOOL, ITEM(res_dir.conn_from_dir_to_fd), 0, CFG_ITEM_DEFAULT,
      "true", "16.2.2", "This Client will accept incoming network connection from this Director."},
  {"ConnectionFromClientToDirector", CFG_TYPE_BOOL, ITEM(res_dir.conn_from_fd_to_dir), 0, CFG_ITEM_DEFAULT,
      "false", "16.2.2", "Let the Filedaemon initiate network connections to the Director."},
  {"Monitor", CFG_TYPE_BOOL, ITEM(res_dir.monitor), 0, CFG_ITEM_DEFAULT, "false", NULL, NULL},
  {"MaximumBandwidthPerJob", CFG_TYPE_SPEED, ITEM(res_dir.max_bandwidth_per_job), 0, 0, NULL, NULL, NULL},
  {"AllowedScriptDir", CFG_TYPE_ALIST_DIR, ITEM(res_dir.allowed_script_dirs), 0, 0, NULL, NULL, NULL},
  {"AllowedJobCommand", CFG_TYPE_ALIST_STR, ITEM(res_dir.allowed_job_cmds), 0, 0, NULL, NULL, NULL},
    TLS_COMMON_CONFIG(res_client),
    TLS_CERT_CONFIG(res_client),
  {NULL, 0, {0}, 0, 0, NULL, NULL, NULL}};

/**
 * Message resource
 */
#include "lib/msg_res.h"

static ResourceTable resources[] = {
  {"Director", dir_items, R_DIRECTOR, sizeof(DirectorResource),
      [](void *res) { return new ((DirectorResource *)res) DirectorResource(); }},
  {"FileDaemon", cli_items, R_CLIENT, sizeof(ClientResource),
      [](void *res) { return new ((ClientResource *)res) ClientResource(); }},
  {"Client", cli_items, R_CLIENT, sizeof(ClientResource),
      [](void *res) { return new ((ClientResource *)res) ClientResource(); }}, /* alias for filedaemon */
  {"Messages", msgs_items, R_MSGS, sizeof(MessagesResource)}, {NULL, NULL, 0}};

/* clang-format on */

static struct s_kw CryptoCiphers[] = {
    {"blowfish", CRYPTO_CIPHER_BLOWFISH_CBC},
    {"3des", CRYPTO_CIPHER_3DES_CBC},
    {"aes128", CRYPTO_CIPHER_AES_128_CBC},
    {"aes192", CRYPTO_CIPHER_AES_192_CBC},
    {"aes256", CRYPTO_CIPHER_AES_256_CBC},
    {"camellia128", CRYPTO_CIPHER_CAMELLIA_128_CBC},
    {"camellia192", CRYPTO_CIPHER_CAMELLIA_192_CBC},
    {"camellia256", CRYPTO_CIPHER_CAMELLIA_256_CBC},
    {"aes128hmacsha1", CRYPTO_CIPHER_AES_128_CBC_HMAC_SHA1},
    {"aes256hmacsha1", CRYPTO_CIPHER_AES_256_CBC_HMAC_SHA1},
    {NULL, 0}};

static void StoreCipher(LEX* lc, ResourceItem* item, int index, int pass)
{
  int i;
  LexGetToken(lc, BCT_NAME);

  /*
   * Scan Crypto Ciphers name.
   */
  for (i = 0; CryptoCiphers[i].name; i++) {
    if (Bstrcasecmp(lc->str, CryptoCiphers[i].name)) {
      *(uint32_t*)(item->value) = CryptoCiphers[i].token;
      i = 0;
      break;
    }
  }
  if (i != 0) {
    scan_err1(lc, _("Expected a Crypto Cipher option, got: %s"), lc->str);
  }
  ScanToEol(lc);
  SetBit(index, res_all.hdr.item_present);
  ClearBit(index, res_all.hdr.inherit_content);
}

/**
 * callback function for init_resource
 * See ../lib/parse_conf.c, function InitResource, for more generic handling.
 */
static void InitResourceCb(ResourceItem* item, int pass)
{
  switch (pass) {
    case 1:
      switch (item->type) {
        case CFG_TYPE_CIPHER:
          for (int i = 0; CryptoCiphers[i].name; i++) {
            if (Bstrcasecmp(item->default_value, CryptoCiphers[i].name)) {
              *(uint32_t*)(item->value) = CryptoCiphers[i].token;
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

/**
 * callback function for parse_config
 * See ../lib/parse_conf.c, function ParseConfig, for more generic handling.
 */
static void ParseConfigCb(LEX* lc, ResourceItem* item, int index, int pass)
{
  switch (item->type) {
    case CFG_TYPE_CIPHER:
      StoreCipher(lc, item, index, pass);
      break;
    default:
      break;
  }
}

static void ConfigReadyCallback(ConfigurationParser& my_config)
{
  std::map<int, std::string> map{{R_DIRECTOR, "R_DIRECTOR"},
                                 {R_CLIENT, "R_CLIENT"},
                                 {R_STORAGE, "R_STORAGE"},
                                 {R_MSGS, "R_MSGS"},
                                 {R_JOB, "R_JOB"}};
  my_config.InitializeQualifiedResourceNameTypeConverter(map);
}

ConfigurationParser* InitFdConfig(const char* configfile, int exit_code)
{
  ConfigurationParser* config = new ConfigurationParser(
      configfile, nullptr, nullptr, InitResourceCb, ParseConfigCb, nullptr,
      exit_code, (void*)&res_all, res_all_size, R_FIRST, R_LAST, resources,
      res_head, default_config_filename.c_str(), "bareos-fd.d",
      ConfigReadyCallback, SaveResource, DumpResource, FreeResource);
  if (config) { config->r_own_ = R_CLIENT; }
  return config;
}

/**
 * Print configuration file schema in json format
 */
#ifdef HAVE_JANSSON
bool PrintConfigSchemaJson(PoolMem& buffer)
{
  ResourceTable* resources = my_config->resources_;

  InitializeJson();

  json_t* json = json_object();
  json_object_set_new(json, "format-version", json_integer(2));
  json_object_set_new(json, "component", json_string("bareos-fd"));
  json_object_set_new(json, "version", json_string(VERSION));

  /*
   * Resources
   */
  json_t* resource = json_object();
  json_object_set(json, "resource", resource);
  json_t* bareos_fd = json_object();
  json_object_set(resource, "bareos-fd", bareos_fd);

  for (int r = 0; resources[r].name; r++) {
    ResourceTable resource = my_config->resources_[r];
    json_object_set(bareos_fd, resource.name, json_items(resource.items));
  }

  PmStrcat(buffer, json_dumps(json, JSON_INDENT(2)));
  json_decref(json);

  return true;
}
#else
bool PrintConfigSchemaJson(PoolMem& buffer)
{
  PmStrcat(buffer, "{ \"success\": false, \"message\": \"not available\" }");
  return false;
}
#endif

static void DumpResource(int type,
                         CommonResourceHeader* reshdr,
                         void sendit(void* sock, const char* fmt, ...),
                         void* sock,
                         bool hide_sensitive_data,
                         bool verbose)
{
  PoolMem buf;
  UnionOfResources* res = (UnionOfResources*)reshdr;
  BareosResource* resclass;
  int recurse = 1;

  if (res == NULL) {
    sendit(sock, "No record for %d %s\n", type, my_config->ResToStr(type));
    return;
  }

  if (type < 0) { /* no recursion */
    type = -type;
    recurse = 0;
  }

  switch (type) {
    case R_MSGS: {
      MessagesResource* resclass = (MessagesResource*)reshdr;
      resclass->PrintConfig(buf);
      break;
    }
    default:
      resclass = (BareosResource*)reshdr;
      resclass->PrintConfig(buf, *my_config);
      break;
  }
  sendit(sock, "%s", buf.c_str());

  if (recurse && res->res_dir.hdr.next) {
    my_config->DumpResourceCb_(type, res->res_dir.hdr.next, sendit, sock,
                               hide_sensitive_data, verbose);
  }
}

/**
 * Free memory of resource.
 * NB, we don't need to worry about freeing any references
 * to other resources as they will be freed when that
 * resource chain is traversed.  Mainly we worry about freeing
 * allocated strings (names).
 */
static void FreeResource(CommonResourceHeader* sres, int type)
{
  CommonResourceHeader* nres;
  UnionOfResources* res = (UnionOfResources*)sres;

  if (res == NULL) { return; }

  /*
   * Common stuff -- free the resource name
   */
  nres = (CommonResourceHeader*)res->res_dir.hdr.next;
  if (res->res_dir.hdr.name) { free(res->res_dir.hdr.name); }
  if (res->res_dir.hdr.desc) { free(res->res_dir.hdr.desc); }
  switch (type) {
    case R_DIRECTOR:
      if (res->res_dir.password_.value) { free(res->res_dir.password_.value); }
      if (res->res_dir.address) { free(res->res_dir.address); }
      if (res->res_dir.allowed_script_dirs) {
        delete res->res_dir.allowed_script_dirs;
      }
      if (res->res_dir.allowed_job_cmds) {
        delete res->res_dir.allowed_job_cmds;
      }
      if (res->res_dir.tls_cert_.allowed_certificate_common_names_) {
        res->res_dir.tls_cert_.allowed_certificate_common_names_->destroy();
        free(res->res_dir.tls_cert_.allowed_certificate_common_names_);
      }
      if (res->res_dir.tls_cert_.ca_certfile_) {
        delete res->res_dir.tls_cert_.ca_certfile_;
      }
      if (res->res_dir.tls_cert_.ca_certdir_) {
        delete res->res_dir.tls_cert_.ca_certdir_;
      }
      if (res->res_dir.tls_cert_.crlfile_) {
        delete res->res_dir.tls_cert_.crlfile_;
      }
      if (res->res_dir.tls_cert_.certfile_) {
        delete res->res_dir.tls_cert_.certfile_;
      }
      if (res->res_dir.tls_cert_.keyfile_) {
        delete res->res_dir.tls_cert_.keyfile_;
      }
      if (res->res_dir.cipherlist_) { delete res->res_dir.cipherlist_; }
      if (res->res_dir.tls_cert_.dhfile_) {
        delete res->res_dir.tls_cert_.dhfile_;
      }
      if (res->res_dir.tls_cert_.pem_message_) {
        delete res->res_dir.tls_cert_.pem_message_;
      }
      break;
    case R_CLIENT:
      if (res->res_client.working_directory) {
        free(res->res_client.working_directory);
      }
      if (res->res_client.pid_directory) {
        free(res->res_client.pid_directory);
      }
      if (res->res_client.subsys_directory) {
        free(res->res_client.subsys_directory);
      }
      if (res->res_client.scripts_directory) {
        free(res->res_client.scripts_directory);
      }
      if (res->res_client.plugin_directory) {
        free(res->res_client.plugin_directory);
      }
      if (res->res_client.plugin_names) { delete res->res_client.plugin_names; }
      if (res->res_client.FDaddrs) { FreeAddresses(res->res_client.FDaddrs); }
      if (res->res_client.FDsrc_addr) {
        FreeAddresses(res->res_client.FDsrc_addr);
      }
      if (res->res_client.pki_keypair_file) {
        free(res->res_client.pki_keypair_file);
      }
      if (res->res_client.pki_keypair) {
        CryptoKeypairFree(res->res_client.pki_keypair);
      }
      if (res->res_client.pki_signing_key_files) {
        delete res->res_client.pki_signing_key_files;
      }
      if (res->res_client.pki_signers) {
        X509_KEYPAIR* keypair = nullptr;
        foreach_alist (keypair, res->res_client.pki_signers) {
          CryptoKeypairFree(keypair);
        }
        delete res->res_client.pki_signers;
      }
      if (res->res_client.pki_master_key_files) {
        delete res->res_client.pki_master_key_files;
      }
      if (res->res_client.pki_recipients) {
        X509_KEYPAIR* keypair = nullptr;
        foreach_alist (keypair, res->res_client.pki_recipients) {
          CryptoKeypairFree(keypair);
        }
        delete res->res_client.pki_recipients;
      }
      if (res->res_client.verid) { free(res->res_client.verid); }
      if (res->res_client.allowed_script_dirs) {
        delete res->res_client.allowed_script_dirs;
      }
      if (res->res_client.allowed_job_cmds) {
        delete res->res_client.allowed_job_cmds;
      }
      if (res->res_client.secure_erase_cmdline) {
        free(res->res_client.secure_erase_cmdline);
      }
      if (res->res_client.log_timestamp_format) {
        free(res->res_client.log_timestamp_format);
      }
      if (res->res_client.tls_cert_.allowed_certificate_common_names_) {
        res->res_client.tls_cert_.allowed_certificate_common_names_->destroy();
        free(res->res_client.tls_cert_.allowed_certificate_common_names_);
      }
      if (res->res_client.tls_cert_.ca_certfile_) {
        delete res->res_client.tls_cert_.ca_certfile_;
      }
      if (res->res_client.tls_cert_.ca_certdir_) {
        delete res->res_client.tls_cert_.ca_certdir_;
      }
      if (res->res_client.tls_cert_.crlfile_) {
        delete res->res_client.tls_cert_.crlfile_;
      }
      if (res->res_client.tls_cert_.certfile_) {
        delete res->res_client.tls_cert_.certfile_;
      }
      if (res->res_client.tls_cert_.keyfile_) {
        delete res->res_client.tls_cert_.keyfile_;
      }
      if (res->res_client.cipherlist_) { delete res->res_client.cipherlist_; }
      if (res->res_client.tls_cert_.dhfile_) {
        delete res->res_client.tls_cert_.dhfile_;
      }
      if (res->res_client.tls_cert_.pem_message_) {
        delete res->res_client.tls_cert_.pem_message_;
      }
      break;
    case R_MSGS:
      if (res->res_msgs.mail_cmd) { free(res->res_msgs.mail_cmd); }
      if (res->res_msgs.operator_cmd) { free(res->res_msgs.operator_cmd); }
      if (res->res_msgs.timestamp_format) {
        free(res->res_msgs.timestamp_format);
      }
      FreeMsgsRes((MessagesResource*)res); /* free message resource */
      res = NULL;
      break;
    default:
      printf(_("Unknown resource type %d\n"), type);
  }
  /* Common stuff again -- free the resource, recurse to next one */
  if (res) { free(res); }
  if (nres) { my_config->FreeResourceCb_(nres, type); }
}

/**
 * Save the new resource by chaining it into the head list for
 * the resource. If this is pass 2, we update any resource
 * pointers (currently only in the Job resource).
 */
static bool SaveResource(int type, ResourceItem* items, int pass)
{
  UnionOfResources* res;
  int rindex = type - R_FIRST;
  int i;
  int error = 0;

  /*
   * Ensure that all required items are present
   */
  for (i = 0; items[i].name; i++) {
    if (items[i].flags & CFG_ITEM_REQUIRED) {
      if (!BitIsSet(i, res_all.res_dir.hdr.item_present)) {
        Emsg2(M_ABORT, 0,
              _("%s item is required in %s resource, but not found.\n"),
              items[i].name, resources[rindex].name);
      }
    }
  }

  /*
   * During pass 2, we looked up pointers to all the resources
   * referrenced in the current resource, , now we
   * must copy their address from the static record to the allocated
   * record.
   */
  if (pass == 2) {
    switch (type) {
      case R_MSGS:
        /*
         * Resources not containing a resource
         */
        break;
      case R_DIRECTOR:
        /*
         * Resources containing another resource
         */
        if ((res = (UnionOfResources*)my_config->GetResWithName(
                 R_DIRECTOR, res_all.res_dir.name())) == NULL) {
          Emsg1(M_ABORT, 0, _("Cannot find Director resource %s\n"),
                res_all.res_dir.name());
        } else {
          res->res_dir.tls_cert_.allowed_certificate_common_names_ =
              res_all.res_dir.tls_cert_.allowed_certificate_common_names_;
          res->res_dir.allowed_script_dirs =
              res_all.res_dir.allowed_script_dirs;
          res->res_dir.allowed_job_cmds = res_all.res_dir.allowed_job_cmds;
        }
        break;
      case R_CLIENT:
        if ((res = (UnionOfResources*)my_config->GetResWithName(
                 R_CLIENT, res_all.res_dir.name())) == NULL) {
          Emsg1(M_ABORT, 0, _("Cannot find Client resource %s\n"),
                res_all.res_dir.name());
        } else {
          res->res_client.plugin_names = res_all.res_client.plugin_names;
          res->res_client.pki_signing_key_files =
              res_all.res_client.pki_signing_key_files;
          res->res_client.pki_master_key_files =
              res_all.res_client.pki_master_key_files;
          res->res_client.pki_signers = res_all.res_client.pki_signers;
          res->res_client.pki_recipients = res_all.res_client.pki_recipients;
          res->res_client.messages = res_all.res_client.messages;
          res->res_client.tls_cert_.allowed_certificate_common_names_ =
              res_all.res_client.tls_cert_.allowed_certificate_common_names_;
          res->res_client.allowed_script_dirs =
              res_all.res_client.allowed_script_dirs;
          res->res_client.allowed_job_cmds =
              res_all.res_client.allowed_job_cmds;
        }
        break;
      default:
        Emsg1(M_ERROR, 0, _("Unknown resource type %d\n"), type);
        error = 1;
        break;
    }
    /*
     * Note, the resoure name was already saved during pass 1,
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
    return (error == 0);
  }

  /*
   * Common
   */
  if (!error) {
    res = (UnionOfResources*)malloc(resources[rindex].size);
    memcpy(res, &res_all, resources[rindex].size);
    if (!res_head[rindex]) {
      res_head[rindex] = (CommonResourceHeader*)res; /* store first entry */
    } else {
      CommonResourceHeader *next, *last;
      /*
       * Add new res to end of chain
       */
      for (last = next = res_head[rindex]; next; next = next->next) {
        last = next;
        if (bstrcmp(next->name, res->res_dir.name())) {
          Emsg2(M_ERROR_TERM, 0,
                _("Attempt to define second %s resource named \"%s\" is not "
                  "permitted.\n"),
                resources[rindex].name, res->res_dir.name());
        }
      }
      last->next = (CommonResourceHeader*)res;
      Dmsg2(90, "Inserting %s res: %s\n", my_config->ResToStr(type),
            res->res_dir.name());
    }
  }
  return (error == 0);
}
} /* namespace filedaemon */
