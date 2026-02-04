/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2008 Free Software Foundation Europe e.V.
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

static bool SaveResource(int type, const ResourceItem* items, int pass);
static void FreeResource(BareosResource* sres, int type);
static void DumpResource(int type,
                         BareosResource* reshdr,
                         ConfigurationParser::sender* sendit,
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
static const ResourceItem cli_items[] = {
  { "Name", CFG_TYPE_NAME, ITEM(res_client, resource_name_), {config::Required{}, config::Description{"The name of this resource. It is used to reference to it."}}},
  { "Description", CFG_TYPE_STR, ITEM(res_client, description_), {}},
  { "Port", CFG_TYPE_ADDRESSES_PORT, ITEM(res_client, FDaddrs), {config::DefaultValue{FD_DEFAULT_PORT}, config::Alias{"FdPort"}}},
  { "Address", CFG_TYPE_ADDRESSES_ADDRESS, ITEM(res_client, FDaddrs), {config::DefaultValue{FD_DEFAULT_PORT}, config::Alias{"FdAddress"}}},
  { "Addresses", CFG_TYPE_ADDRESSES, ITEM(res_client, FDaddrs), {config::DefaultValue{FD_DEFAULT_PORT}, config::Alias{"FdAddresses"}}},
  { "SourceAddress", CFG_TYPE_ADDRESSES_ADDRESS, ITEM(res_client, FDsrc_addr), {config::DefaultValue{"0"}, config::Alias{"FdSourceAddress"}}},
  { "WorkingDirectory", CFG_TYPE_DIR, ITEM(res_client, working_directory), {config::DefaultValue{PATH_BAREOS_WORKINGDIR}, config::PlatformSpecific{}}},
  { "PluginDirectory", CFG_TYPE_DIR, ITEM(res_client, plugin_directory), {}},
  { "PluginNames", CFG_TYPE_PLUGIN_NAMES, ITEM(res_client, plugin_names), {}},
  { "ScriptsDirectory", CFG_TYPE_DIR, ITEM(res_client, scripts_directory), {config::DefaultValue{PATH_BAREOS_SCRIPTDIR}, config::Description{"Path to directory containing script files"}, config::PlatformSpecific{}}},
  { "MaximumConcurrentJobs", CFG_TYPE_PINT32, ITEM(res_client, MaxConcurrentJobs), {config::DeprecatedSince{24, 0, 0}, config::DefaultValue{"1000"}}},
  { "MaximumWorkersPerJob", CFG_TYPE_PINT32, ITEM(res_client, MaxWorkersPerJob), {config::IntroducedIn{23, 0, 0}, config::DefaultValue{"2"}, config::Description{"The maximum number of worker threads that bareos will use during backup."}}},
  { "Messages", CFG_TYPE_RES, ITEM(res_client, messages), {config::Code{R_MSGS}}},
  { "SdConnectTimeout", CFG_TYPE_TIME, ITEM(res_client, SDConnectTimeout), {config::DefaultValue{"1800"}}},
  { "HeartbeatInterval", CFG_TYPE_TIME, ITEM(res_client, heartbeat_interval), {config::DefaultValue{"0"}}},
  { "MaximumNetworkBufferSize", CFG_TYPE_PINT32, ITEM(res_client, max_network_buffer_size), {}},
  { "PkiSignatures", CFG_TYPE_BOOL, ITEM(res_client, pki_sign), {config::DefaultValue{"false"}, config::Description{"Enable Data Signing."}}},
  { "PkiEncryption", CFG_TYPE_BOOL, ITEM(res_client, pki_encrypt), {config::DefaultValue{"false"}, config::Description{"Enable Data Encryption."}}},
  { "PkiKeyPair", CFG_TYPE_DIR, ITEM(res_client, pki_keypair_file), {config::Description{"File with public and private key to sign, encrypt (backup) and decrypt (restore) the data."}}},
  { "PkiSigner", CFG_TYPE_ALIST_DIR, ITEM(res_client, pki_signing_key_files), {config::Description{"Additional public/private key files to sign or verify the data."}}},
  { "PkiMasterKey", CFG_TYPE_ALIST_DIR, ITEM(res_client, pki_master_key_files), {config::Description{"List of public key files. Data will be decryptable via the corresponding private keys."}}},
  { "PkiCipher", CFG_TYPE_CIPHER, ITEM(res_client, pki_cipher), {config::DefaultValue{"aes128"}, config::Description{"PKI Cipher used for data encryption."}}},
  { "VerId", CFG_TYPE_STR, ITEM(res_client, verid), {}},
  { "MaximumBandwidthPerJob", CFG_TYPE_SPEED, ITEM(res_client, max_bandwidth_per_job), {}},
  { "AllowBandwidthBursting", CFG_TYPE_BOOL, ITEM(res_client, allow_bw_bursting), {config::DefaultValue{"false"}}},
  { "AllowedScriptDir", CFG_TYPE_ALIST_DIR, ITEM(res_client, allowed_script_dirs), {}},
  { "AllowedJobCommand", CFG_TYPE_ALIST_STR, ITEM(res_client, allowed_job_cmds), {}},
  { "AbsoluteJobTimeout", CFG_TYPE_PINT32, ITEM(res_client, jcr_watchdog_time), {config::IntroducedIn{14, 2, 0}, config::Description{"Absolute time after which a Job gets terminated regardless of its progress"}}},
  { "AlwaysUseLmdb", CFG_TYPE_BOOL, ITEM(res_client, always_use_lmdb), {config::DeprecatedSince{24, 0, 0}, config::DefaultValue{"false"}, config::Description{"Ensure that bareos always chooses the lmdb backend for accurate information regardless of the file list size.  Use LmdbThreshold = 0 instead."}}},
  { "LmdbThreshold", CFG_TYPE_PINT32, ITEM(res_client, lmdb_threshold), {config::Description{"File count threshold after which bareos will use the lmdb backend to store accurate information."}}},
  { "SecureEraseCommand", CFG_TYPE_STR, ITEM(res_client, secure_erase_cmdline), {config::IntroducedIn{15, 2, 1}, config::Description{"Specify command that will be called when bareos unlinks files."}}},
  { "LogTimestampFormat", CFG_TYPE_STR, ITEM(res_client, log_timestamp_format), {config::IntroducedIn{15, 2, 3}, config::DefaultValue{"%d-%b %H:%M"}}},
  { "GrpcModule", CFG_TYPE_STDSTR, ITEM(res_client, grpc_module),
   {config::IntroducedIn{25, 0, 0},
    config::Description{"The grpc module to use for grpc fallback."},
    config::DefaultValue{"bareos-grpc-fd-plugin-bridge"}}},
  { "EnableKtls", CFG_TYPE_BOOL, ITEM(res_client, enable_ktls), {config::DefaultValue{"false"}, config::Description{"If set to \"yes\", Bareos will allow the SSL implementation to use Kernel TLS."}, config::IntroducedIn{23, 0, 0}}},
  TLS_COMMON_CONFIG(res_client),
  TLS_CERT_CONFIG(res_client),
  {}
};
// Directors that can use our services
static const ResourceItem dir_items[] = {
  { "Name", CFG_TYPE_NAME, ITEM(res_dir, resource_name_), {config::Required{}}},
  { "Description", CFG_TYPE_STR, ITEM(res_dir, description_), {}},
  { "Password", CFG_TYPE_MD5PASSWORD, ITEM(res_dir, password_), {config::Required{}}},
  { "Address", CFG_TYPE_STR, ITEM(res_dir, address), {config::Description{"Director Network Address. Only required if \"Connection From Client To Director\" is enabled."}}},
  { "Port", CFG_TYPE_PINT32, ITEM(res_dir, port), {config::IntroducedIn{16, 2, 2}, config::DefaultValue{DIR_DEFAULT_PORT}, config::Description{"Director Network Port. Only used if \"Connection From Client To Director\" is enabled."}}},
  { "ConnectionFromDirectorToClient", CFG_TYPE_BOOL, ITEM(res_dir, conn_from_dir_to_fd), {config::IntroducedIn{16, 2, 2}, config::DefaultValue{"true"}, config::Description{"This Client will accept incoming network connection from this Director."}}},
  { "ConnectionFromClientToDirector", CFG_TYPE_BOOL, ITEM(res_dir, conn_from_fd_to_dir), {config::IntroducedIn{16, 2, 2}, config::DefaultValue{"false"}, config::Description{"Let the Filedaemon initiate network connections to the Director."}}},
  { "Monitor", CFG_TYPE_BOOL, ITEM(res_dir, monitor), {config::DefaultValue{"false"}}},
  { "MaximumBandwidthPerJob", CFG_TYPE_SPEED, ITEM(res_dir, max_bandwidth_per_job), {}},
  { "AllowedScriptDir", CFG_TYPE_ALIST_DIR, ITEM(res_dir, allowed_script_dirs), {}},
  { "AllowedJobCommand", CFG_TYPE_ALIST_STR, ITEM(res_dir, allowed_job_cmds), {}},
    TLS_COMMON_CONFIG(res_dir),
    TLS_CERT_CONFIG(res_dir),
    {}
};
// Message resource
#include "lib/messages_resource_items.h"

static ResourceTable resources[] = {
  {"Director", "Directors", dir_items, R_DIRECTOR, sizeof(DirectorResource),
      []() { res_dir = new  DirectorResource(); }, reinterpret_cast<BareosResource**>(&res_dir)},
  {"Client", "Clients", cli_items, R_CLIENT, sizeof(ClientResource),
      []() { res_client = new ClientResource(); }, reinterpret_cast<BareosResource**>(&res_client), { { "FileDaemon", "FileDaemons" } }},
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

static void StoreCipher(ConfigurationParser* conf,
                        lexer* lc,
                        const ResourceItem* item,
                        int index,
                        int)
{
  auto found = ReadKeyword(conf, lc, CryptoCiphers);

  if (!found) {
    scan_err1(lc, T_("Expected a Crypto Cipher option, got: %s"), lc->str());
  } else {
    SetItemVariable<uint32_t>(*item, found->token);
  }

  ScanToEol(lc);
  item->SetPresent();
  ClearBit(index, (*item->allocated_resource)->inherit_content_);
}

/**
 * callback function for init_resource
 * See ../lib/parse_conf.c, function InitResource, for more generic handling.
 */
static void InitResourceCb(const ResourceItem* item, int pass)
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
static void ParseConfigCb(ConfigurationParser* conf,
                          lexer* lc,
                          const ResourceItem* item,
                          int index,
                          int pass)
{
  switch (item->type) {
    case CFG_TYPE_CIPHER:
      StoreCipher(conf, lc, item, index, pass);
      break;
    default:
      break;
  }
}

static void ConfigBeforeCallback(ConfigurationParser& t_config)
{
  std::map<int, std::string> map{{R_DIRECTOR, "R_DIRECTOR"},
                                 {R_CLIENT, "R_CLIENT"},
                                 {R_STORAGE, "R_STORAGE"},
                                 {R_MSGS, "R_MSGS"},
                                 {R_JOB, "R_JOB"}};
  t_config.InitializeQualifiedResourceNameTypeConverter(map);
}

static void ConfigReadyCallback(ConfigurationParser&) {}

ConfigurationParser* InitFdConfig(const char* t_configfile, int exit_code)
{
  ConfigurationParser* config = new ConfigurationParser(
      t_configfile, InitResourceCb, ParseConfigCb, nullptr, exit_code, R_NUM,
      resources, default_config_filename.c_str(), "bareos-fd.d",
      ConfigBeforeCallback, ConfigReadyCallback, SaveResource, DumpResource,
      FreeResource);
  if (config) { config->r_own_ = R_CLIENT; }
  return config;
}

// Print configuration file schema in json format
#ifdef HAVE_JANSSON
bool PrintConfigSchemaJson(PoolMem& buffer)
{
  json_t* json = json_object();
  json_object_set_new(json, "format-version", json_integer(2));
  json_object_set_new(json, "component", json_string("bareos-fd"));
  json_object_set_new(json, "version", json_string(kBareosVersionStrings.Full));

  // Resources
  json_t* resource = json_object();
  json_object_set_new(json, "resource", resource);
  json_t* bareos_fd = json_object();
  json_object_set_new(resource, "bareos-fd", bareos_fd);

  for (int r = 0; my_config->resource_definitions_[r].name; r++) {
    ResourceTable resource_table = my_config->resource_definitions_[r];
    json_object_set_new(bareos_fd, resource_table.name,
                        json_items(resource_table.items));
  }

  char* const json_str = json_dumps(json, JSON_INDENT(2));
  PmStrcat(buffer, json_str);
  free(json_str);
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
                         ConfigurationParser::sender* sendit,
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
      if (p->scripts_directory) { free(p->scripts_directory); }
      if (p->plugin_directory) { free(p->plugin_directory); }
      if (p->plugin_names) { delete p->plugin_names; }
      if (p->FDaddrs) { FreeAddresses(p->FDaddrs); }
      if (p->FDsrc_addr) { FreeAddresses(p->FDsrc_addr); }
      if (p->pki_keypair_file) { free(p->pki_keypair_file); }
      if (p->pki_keypair) { CryptoKeypairFree(p->pki_keypair); }
      if (p->pki_signing_key_files) { delete p->pki_signing_key_files; }
      if (p->pki_signers) {
        for (auto* keypair : p->pki_signers) { CryptoKeypairFree(keypair); }
        delete p->pki_signers;
      }
      if (p->pki_master_key_files) { delete p->pki_master_key_files; }
      if (p->pki_recipients) {
        for (auto* keypair : p->pki_recipients) { CryptoKeypairFree(keypair); }
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
      printf(T_("Unknown resource type %d\n"), type);
      break;
  }
  if (next_resource) { FreeResource(next_resource, type); }
}

/**
 * Save the new resource by chaining it into the head list for
 * the resource. If this is pass 2, we update any resource
 * pointers (currently only in the Job resource).
 */
static bool SaveResource(int type, const ResourceItem* items, int pass)
{
  int i;
  int error = 0;

  // Ensure that all required items are present
  for (i = 0; items[i].name; i++) {
    if (items[i].is_required) {
      if (!items[i].IsPresent()) {
        Emsg2(M_ABORT, 0,
              T_("%s item is required in %s resource, but not found.\n"),
              items[i].name, resources[type].name);
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
          Emsg1(M_ABORT, 0, T_("Cannot find Director resource %s\n"),
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
          Emsg1(M_ABORT, 0, T_("Cannot find Client resource %s\n"),
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
        Emsg1(M_ERROR, 0, T_("Unknown resource type %d\n"), type);
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
