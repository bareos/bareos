/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2008 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2022 Bareos GmbH & Co. KG

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
// Kern Sibbald, September MM
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
#include "lib/output_formatter_resource.h"
#include "lib/tls_resource_items.h"

#include <cassert>

namespace filedaemon {

static bool SaveResource(int type, ResourceItem* items, int pass);
static void FreeResource(BareosResource* sres, int type);
static void DumpResource(int type,
                         BareosResource* reshdr,
                         bool sendit(void* sock, const char* fmt, ...),
                         void* sock,
                         bool hide_sensitive_data,
                         bool verbose);

/**
 * We build the current resource here as we are
 * scanning the resource configuration definition,
 * then move it to allocated memory when the resource
 * scan is complete.
 */
static DirectorResource* res_dir;
static ClientResource* res_client;
static MessagesResource* res_msgs;

/* clang-format off */

// Client or File daemon "Global" resources
static ResourceItem cli_items[] = {
  {"Name", CFG_TYPE_NAME, ITEM(res_client, resource_name_), 0, CFG_ITEM_REQUIRED, NULL, NULL,
      "The name of this resource. It is used to reference to it."},
  {"Description", CFG_TYPE_STR, ITEM(res_client, description_), 0, 0, NULL, NULL, NULL},
  {"FdPort", CFG_TYPE_ADDRESSES_PORT, ITEM(res_client, FDaddrs), 0, CFG_ITEM_DEFAULT, FD_DEFAULT_PORT, NULL, NULL},
  {"FdAddress", CFG_TYPE_ADDRESSES_ADDRESS, ITEM(res_client, FDaddrs), 0, CFG_ITEM_DEFAULT, FD_DEFAULT_PORT, NULL, NULL},
  {"FdAddresses", CFG_TYPE_ADDRESSES, ITEM(res_client, FDaddrs), 0, CFG_ITEM_DEFAULT, FD_DEFAULT_PORT, NULL, NULL},
  {"FdSourceAddress", CFG_TYPE_ADDRESSES_ADDRESS, ITEM(res_client, FDsrc_addr), 0, CFG_ITEM_DEFAULT, "0", NULL, NULL},
  {"WorkingDirectory", CFG_TYPE_DIR, ITEM(res_client, working_directory), 0,
      CFG_ITEM_DEFAULT | CFG_ITEM_PLATFORM_SPECIFIC, PATH_BAREOS_WORKINGDIR, NULL, NULL},
  {"PidDirectory", CFG_TYPE_DIR, ITEM(res_client, pid_directory), 0, CFG_ITEM_DEPRECATED, NULL, NULL, NULL},
  {"PluginDirectory", CFG_TYPE_DIR, ITEM(res_client, plugin_directory), 0, 0, NULL, NULL, NULL},
  {"PluginNames", CFG_TYPE_PLUGIN_NAMES, ITEM(res_client, plugin_names), 0, 0, NULL, NULL, NULL},
  {"ScriptsDirectory", CFG_TYPE_DIR, ITEM(res_client, scripts_directory), 0, 0, NULL, NULL, NULL},
  {"MaximumConcurrentJobs", CFG_TYPE_PINT32, ITEM(res_client, MaxConcurrentJobs), 0, CFG_ITEM_DEFAULT, "20", NULL, NULL},
  {"MaximumConnections", CFG_TYPE_PINT32, ITEM(res_client, MaxConnections), 0, CFG_ITEM_DEFAULT, "42", "15.2.3-", NULL},
  {"Messages", CFG_TYPE_RES, ITEM(res_client, messages), R_MSGS, 0, NULL, NULL, NULL},
  {"SdConnectTimeout", CFG_TYPE_TIME, ITEM(res_client, SDConnectTimeout), 0, CFG_ITEM_DEFAULT, "1800" /* 30 minutes */, NULL, NULL},
  {"HeartbeatInterval", CFG_TYPE_TIME, ITEM(res_client, heartbeat_interval), 0, CFG_ITEM_DEFAULT, "0", NULL, NULL},
  {"MaximumNetworkBufferSize", CFG_TYPE_PINT32, ITEM(res_client, max_network_buffer_size), 0, 0, NULL, NULL, NULL},
  {"PkiSignatures", CFG_TYPE_BOOL, ITEM(res_client, pki_sign), 0, CFG_ITEM_DEFAULT, "false", NULL, "Enable Data Signing."},
  {"PkiEncryption", CFG_TYPE_BOOL, ITEM(res_client, pki_encrypt), 0, CFG_ITEM_DEFAULT, "false", NULL, "Enable Data Encryption."},
  {"PkiKeyPair", CFG_TYPE_DIR, ITEM(res_client, pki_keypair_file), 0, 0, NULL, NULL,
      "File with public and private key to sign, encrypt (backup) and decrypt (restore) the data."},
  {"PkiSigner", CFG_TYPE_ALIST_DIR, ITEM(res_client, pki_signing_key_files), 0, 0, NULL, NULL,
      "Additional public/private key files to sign or verify the data."},
  {"PkiMasterKey", CFG_TYPE_ALIST_DIR, ITEM(res_client, pki_master_key_files), 0, 0, NULL, NULL,
      "List of public key files. Data will be decryptable via the corresponding private keys."},
  {"PkiCipher", CFG_TYPE_CIPHER, ITEM(res_client, pki_cipher), 0, CFG_ITEM_DEFAULT, "aes128", NULL,
      "PKI Cipher used for data encryption."},
  {"VerId", CFG_TYPE_STR, ITEM(res_client, verid), 0, 0, NULL, NULL, NULL},
  {"Compatible", CFG_TYPE_BOOL, ITEM(res_client, compatible), 0, CFG_ITEM_DEFAULT, "false", NULL, NULL},
  {"MaximumBandwidthPerJob", CFG_TYPE_SPEED, ITEM(res_client, max_bandwidth_per_job), 0, 0, NULL, NULL, NULL},
  {"AllowBandwidthBursting", CFG_TYPE_BOOL, ITEM(res_client, allow_bw_bursting), 0, CFG_ITEM_DEFAULT, "false", NULL, NULL},
  {"AllowedScriptDir", CFG_TYPE_ALIST_DIR, ITEM(res_client, allowed_script_dirs), 0, 0, NULL, NULL, NULL},
  {"AllowedJobCommand", CFG_TYPE_ALIST_STR, ITEM(res_client, allowed_job_cmds), 0, 0, NULL, NULL, NULL},
  {"AbsoluteJobTimeout", CFG_TYPE_PINT32, ITEM(res_client, jcr_watchdog_time), 0, 0, NULL, NULL, NULL},
  {"AlwaysUseLmdb", CFG_TYPE_BOOL, ITEM(res_client, always_use_lmdb), 0, CFG_ITEM_DEFAULT, "false", NULL, NULL},
  {"LmdbThreshold", CFG_TYPE_PINT32, ITEM(res_client, lmdb_threshold), 0, 0, NULL, NULL, NULL},
  {"SecureEraseCommand", CFG_TYPE_STR, ITEM(res_client, secure_erase_cmdline), 0, 0, NULL, "15.2.1-",
      "Specify command that will be called when bareos unlinks files."},
  {"LogTimestampFormat", CFG_TYPE_STR, ITEM(res_client, log_timestamp_format), 0, 0, NULL, "15.2.3-", NULL},
    TLS_COMMON_CONFIG(res_client),
    TLS_CERT_CONFIG(res_client),
  {nullptr, 0, 0, nullptr, 0, 0, nullptr, nullptr, nullptr}
};
// Directors that can use our services
static ResourceItem dir_items[] = {
  {"Name", CFG_TYPE_NAME, ITEM(res_dir, resource_name_), 0, CFG_ITEM_REQUIRED, NULL, NULL, NULL},
  {"Description", CFG_TYPE_STR, ITEM(res_dir, description_), 0, 0, NULL, NULL, NULL},
  {"Password", CFG_TYPE_MD5PASSWORD, ITEM(res_dir, password_), 0, CFG_ITEM_REQUIRED, NULL, NULL, NULL},
  {"Address", CFG_TYPE_STR, ITEM(res_dir, address), 0, 0, NULL, NULL,
      "Director Network Address. Only required if \"Connection From Client To Director\" is enabled."},
  {"Port", CFG_TYPE_PINT32, ITEM(res_dir, port), 0, CFG_ITEM_DEFAULT, DIR_DEFAULT_PORT, "16.2.2",
      "Director Network Port. Only used if \"Connection From Client To Director\" is enabled."},
  {"ConnectionFromDirectorToClient", CFG_TYPE_BOOL, ITEM(res_dir, conn_from_dir_to_fd), 0, CFG_ITEM_DEFAULT,
      "true", "16.2.2", "This Client will accept incoming network connection from this Director."},
  {"ConnectionFromClientToDirector", CFG_TYPE_BOOL, ITEM(res_dir, conn_from_fd_to_dir), 0, CFG_ITEM_DEFAULT,
      "false", "16.2.2", "Let the Filedaemon initiate network connections to the Director."},
  {"Monitor", CFG_TYPE_BOOL, ITEM(res_dir, monitor), 0, CFG_ITEM_DEFAULT, "false", NULL, NULL},
  {"MaximumBandwidthPerJob", CFG_TYPE_SPEED, ITEM(res_dir, max_bandwidth_per_job), 0, 0, NULL, NULL, NULL},
  {"AllowedScriptDir", CFG_TYPE_ALIST_DIR, ITEM(res_dir, allowed_script_dirs), 0, 0, NULL, NULL, NULL},
  {"AllowedJobCommand", CFG_TYPE_ALIST_STR, ITEM(res_dir, allowed_job_cmds), 0, 0, NULL, NULL, NULL},
    TLS_COMMON_CONFIG(res_dir),
    TLS_CERT_CONFIG(res_dir),
  {nullptr, 0, 0, nullptr, 0, 0, nullptr, nullptr, nullptr}
};
// Message resource
#include "lib/messages_resource_items.h"

static ResourceTable resources[] = {
  {"Director", "Directors", dir_items, R_DIRECTOR, sizeof(DirectorResource),
      []() { res_dir = new  DirectorResource(); }, reinterpret_cast<BareosResource**>(&res_dir)},
  {"FileDaemon", "FileDaemons", cli_items, R_CLIENT, sizeof(ClientResource),
      []() { res_client = new ClientResource(); }, reinterpret_cast<BareosResource**>(&res_client)},
  {"Client", "Clients", cli_items, R_CLIENT, sizeof(ClientResource),
      []() { res_client = new ClientResource(); }, reinterpret_cast<BareosResource**>(&res_client)}, /* alias for filedaemon */
  {"Messages", "Messages", msgs_items, R_MSGS, sizeof(MessagesResource),
      []() { res_msgs = new MessagesResource(); }, reinterpret_cast<BareosResource**>(&res_msgs)},
  {nullptr, nullptr, nullptr, 0, 0, nullptr, nullptr}
};

/* clang-format on */

static struct s_kw CryptoCiphers[]
    = {{"blowfish", CRYPTO_CIPHER_BLOWFISH_CBC},
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

  // Scan Crypto Ciphers name.
  for (i = 0; CryptoCiphers[i].name; i++) {
    if (Bstrcasecmp(lc->str, CryptoCiphers[i].name)) {
      SetItemVariable<uint32_t>(*item, CryptoCiphers[i].token);
      i = 0;
      break;
    }
  }
  if (i != 0) {
    scan_err1(lc, _("Expected a Crypto Cipher option, got: %s"), lc->str);
  }
  ScanToEol(lc);
  SetBit(index, (*item->allocated_resource)->item_present_);
  ClearBit(index, (*item->allocated_resource)->inherit_content_);
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
              SetItemVariable<uint32_t>(*item, CryptoCiphers[i].token);
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
static void ParseConfigCb(LEX* lc,
                          ResourceItem* item,
                          int index,
                          int pass,
                          BareosResource** configuration_resources)
{
  switch (item->type) {
    case CFG_TYPE_CIPHER:
      StoreCipher(lc, item, index, pass);
      break;
    default:
      break;
  }
}

static void ConfigBeforeCallback(ConfigurationParser& my_config)
{
  std::map<int, std::string> map{{R_DIRECTOR, "R_DIRECTOR"},
                                 {R_CLIENT, "R_CLIENT"},
                                 {R_STORAGE, "R_STORAGE"},
                                 {R_MSGS, "R_MSGS"},
                                 {R_JOB, "R_JOB"}};
  my_config.InitializeQualifiedResourceNameTypeConverter(map);
}

static void ConfigReadyCallback(ConfigurationParser& my_config) {}

ConfigurationParser* InitFdConfig(const char* configfile, int exit_code)
{
  ConfigurationParser* config = new ConfigurationParser(
      configfile, nullptr, nullptr, InitResourceCb, ParseConfigCb, nullptr,
      exit_code, R_NUM, resources, default_config_filename.c_str(),
      "bareos-fd.d", ConfigBeforeCallback, ConfigReadyCallback, SaveResource,
      DumpResource, FreeResource);
  if (config) { config->r_own_ = R_CLIENT; }
  return config;
}

// Print configuration file schema in json format
#ifdef HAVE_JANSSON
bool PrintConfigSchemaJson(PoolMem& buffer)
{
  ResourceTable* resources = my_config->resource_definitions_;

  json_t* json = json_object();
  json_object_set_new(json, "format-version", json_integer(2));
  json_object_set_new(json, "component", json_string("bareos-fd"));
  json_object_set_new(json, "version", json_string(kBareosVersionStrings.Full));

  // Resources
  json_t* resource = json_object();
  json_object_set(json, "resource", resource);
  json_t* bareos_fd = json_object();
  json_object_set(resource, "bareos-fd", bareos_fd);

  for (int r = 0; resources[r].name; r++) {
    ResourceTable resource = my_config->resource_definitions_[r];
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
                         BareosResource* res,
                         bool sendit(void* sock, const char* fmt, ...),
                         void* sock,
                         bool hide_sensitive_data,
                         bool verbose)
{
  PoolMem buf;
  int recurse = 1;
  OutputFormatter output_formatter
      = OutputFormatter(sendit, sock, nullptr, nullptr);
  OutputFormatterResource output_formatter_resource
      = OutputFormatterResource(&output_formatter);

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
      MessagesResource* resclass = dynamic_cast<MessagesResource*>(res);
      assert(resclass);
      resclass->PrintConfig(output_formatter_resource, *my_config,
                            hide_sensitive_data, verbose);
      break;
    }
    default:
      res->PrintConfig(output_formatter_resource, *my_config,
                       hide_sensitive_data, verbose);

      break;
  }

  if (recurse && res->next_) {
    my_config->DumpResourceCb_(type, res->next_, sendit, sock,
                               hide_sensitive_data, verbose);
  }
}

static void FreeResource(BareosResource* res, int type)
{
  BareosResource* next_resource;

  if (res == NULL) { return; }

  // Common stuff -- free the resource name
  next_resource = (BareosResource*)res->next_;

  if (res->resource_name_) { free(res->resource_name_); }
  if (res->description_) { free(res->description_); }

  switch (type) {
    case R_DIRECTOR: {
      DirectorResource* p = dynamic_cast<DirectorResource*>(res);
      assert(p);
      if (p->password_.value) { free(p->password_.value); }
      if (p->address) { free(p->address); }
      if (p->allowed_script_dirs) { delete p->allowed_script_dirs; }
      if (p->allowed_job_cmds) { delete p->allowed_job_cmds; }
      delete p;
      break;
    }
    case R_CLIENT: {
      ClientResource* p = dynamic_cast<ClientResource*>(res);
      assert(p);
      if (p->working_directory) { free(p->working_directory); }
      if (p->pid_directory) { free(p->pid_directory); }
      if (p->scripts_directory) { free(p->scripts_directory); }
      if (p->plugin_directory) { free(p->plugin_directory); }
      if (p->plugin_names) { delete p->plugin_names; }
      if (p->FDaddrs) { FreeAddresses(p->FDaddrs); }
      if (p->FDsrc_addr) { FreeAddresses(p->FDsrc_addr); }
      if (p->pki_keypair_file) { free(p->pki_keypair_file); }
      if (p->pki_keypair) { CryptoKeypairFree(p->pki_keypair); }
      if (p->pki_signing_key_files) { delete p->pki_signing_key_files; }
      if (p->pki_signers) {
        X509_KEYPAIR* keypair = nullptr;
        foreach_alist (keypair, p->pki_signers) {
          CryptoKeypairFree(keypair);
        }
        delete p->pki_signers;
      }
      if (p->pki_master_key_files) { delete p->pki_master_key_files; }
      if (p->pki_recipients) {
        X509_KEYPAIR* keypair = nullptr;
        foreach_alist (keypair, p->pki_recipients) {
          CryptoKeypairFree(keypair);
        }
        delete p->pki_recipients;
      }
      if (p->verid) { free(p->verid); }
      if (p->allowed_script_dirs) { delete p->allowed_script_dirs; }
      if (p->allowed_job_cmds) { delete p->allowed_job_cmds; }
      if (p->secure_erase_cmdline) { free(p->secure_erase_cmdline); }
      if (p->log_timestamp_format) { free(p->log_timestamp_format); }
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
      printf(_("Unknown resource type %d\n"), type);
      break;
  }
  if (next_resource) { FreeResource(next_resource, type); }
}

/**
 * Save the new resource by chaining it into the head list for
 * the resource. If this is pass 2, we update any resource
 * pointers (currently only in the Job resource).
 */
static bool SaveResource(int type, ResourceItem* items, int pass)
{
  int rindex = type;
  int i;
  int error = 0;

  // Ensure that all required items are present
  for (i = 0; items[i].name; i++) {
    if (items[i].flags & CFG_ITEM_REQUIRED) {
      if (!BitIsSet(i, (*items[i].allocated_resource)->item_present_)) {
        Emsg2(M_ABORT, 0,
              _("%s item is required in %s resource, but not found.\n"),
              items[i].name, resources[rindex].name);
      }
    }
  }

  // save previously discovered pointers into dynamic memory
  if (pass == 2) {
    switch (type) {
      case R_MSGS:
        // Resources not containing a resource
        break;
      case R_DIRECTOR: {
        DirectorResource* p = dynamic_cast<DirectorResource*>(
            my_config->GetResWithName(R_DIRECTOR, res_dir->resource_name_));
        if (!p) {
          Emsg1(M_ABORT, 0, _("Cannot find Director resource %s\n"),
                res_dir->resource_name_);
        } else {
          p->tls_cert_.allowed_certificate_common_names_
              = std::move(res_dir->tls_cert_.allowed_certificate_common_names_);
          p->allowed_script_dirs = res_dir->allowed_script_dirs;
          p->allowed_job_cmds = res_dir->allowed_job_cmds;
        }
        break;
      }
      case R_CLIENT: {
        ClientResource* p = dynamic_cast<ClientResource*>(
            my_config->GetResWithName(R_CLIENT, res_client->resource_name_));
        if (!p) {
          Emsg1(M_ABORT, 0, _("Cannot find Client resource %s\n"),
                res_client->resource_name_);
        } else {
          p->plugin_names = res_client->plugin_names;
          p->pki_signing_key_files = res_client->pki_signing_key_files;
          p->pki_master_key_files = res_client->pki_master_key_files;
          p->pki_signers = res_client->pki_signers;
          p->pki_recipients = res_client->pki_recipients;
          p->messages = res_client->messages;
          p->tls_cert_.allowed_certificate_common_names_ = std::move(
              res_client->tls_cert_.allowed_certificate_common_names_);
          p->allowed_script_dirs = res_client->allowed_script_dirs;
          p->allowed_job_cmds = res_client->allowed_job_cmds;
        }
        break;
      }
      default:
        Emsg1(M_ERROR, 0, _("Unknown resource type %d\n"), type);
        error = 1;
        break;
    }

    /* resource_name_ was already deep copied during 1. pass
     * as matter of fact the remaining allocated memory is
     * redundant and would not be freed in the dynamic resources;
     *
     * currently, this is the best place to free that */

    return (error == 0);
  }

  if (!error) {
    BareosResource* new_resource = nullptr;
    switch (type) {
      case R_DIRECTOR: {
        new_resource = res_dir;
        res_dir = nullptr;
        break;
      }
      case R_CLIENT: {
        new_resource = res_client;
        res_client = nullptr;
        break;
      }
      case R_MSGS: {
        new_resource = res_msgs;
        res_msgs = nullptr;
        break;
      }
      default:
        ASSERT(false);
        break;
    }
    error = my_config->AppendToResourcesChain(new_resource, type) ? 0 : 1;
  }
  return (error == 0);
}
} /* namespace filedaemon */
