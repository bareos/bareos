/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2008 Free Software Foundation Europe e.V.
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
 *
 * Kern Sibbald, September MM
 */

#include "bareos.h"
#include "filed.h"

/*
 * Define the first and last resource ID record
 * types. Note, these should be unique for each
 * daemon though not a requirement.
 */
static RES *sres_head[R_LAST - R_FIRST + 1];
static RES **res_head = sres_head;

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
 * information.
 */

/*
 * Client or File daemon "Global" resources
 */
static RES_ITEM cli_items[] = {
   { "name", CFG_TYPE_NAME, ITEM(res_client.hdr.name), 0, CFG_ITEM_REQUIRED, NULL },
   { "description", CFG_TYPE_STR, ITEM(res_client.hdr.desc), 0, 0, NULL },
   { "fdport", CFG_TYPE_ADDRESSES_PORT, ITEM(res_client.FDaddrs), 0, CFG_ITEM_DEFAULT, FD_DEFAULT_PORT },
   { "fdaddress", CFG_TYPE_ADDRESSES_ADDRESS, ITEM(res_client.FDaddrs), 0, CFG_ITEM_DEFAULT, FD_DEFAULT_PORT },
   { "fdaddresses", CFG_TYPE_ADDRESSES, ITEM(res_client.FDaddrs), 0, CFG_ITEM_DEFAULT, FD_DEFAULT_PORT },
   { "fdsourceaddress", CFG_TYPE_ADDRESSES_ADDRESS, ITEM(res_client.FDsrc_addr), 0, CFG_ITEM_DEFAULT, "0" },
   { "workingdirectory", CFG_TYPE_DIR, ITEM(res_client.working_directory), 0, CFG_ITEM_DEFAULT, _PATH_BAREOS_WORKINGDIR },
   { "piddirectory", CFG_TYPE_DIR, ITEM(res_client.pid_directory), 0, CFG_ITEM_DEFAULT, _PATH_BAREOS_PIDDIR },
   { "subsysdirectory", CFG_TYPE_DIR, ITEM(res_client.subsys_directory), 0, 0, NULL },
   { "plugindirectory", CFG_TYPE_DIR, ITEM(res_client.plugin_directory), 0, 0, NULL },
   { "pluginnames", CFG_TYPE_STR, ITEM(res_client.plugin_names), 0, 0, NULL },
   { "scriptsdirectory", CFG_TYPE_DIR, ITEM(res_client.scripts_directory), 0, 0, NULL },
   { "maximumconcurrentjobs", CFG_TYPE_PINT32, ITEM(res_client.MaxConcurrentJobs), 0, CFG_ITEM_DEFAULT, "20" },
   { "messages", CFG_TYPE_RES, ITEM(res_client.messages), R_MSGS, 0, NULL },
   { "sdconnecttimeout", CFG_TYPE_TIME, ITEM(res_client.SDConnectTimeout), 0, CFG_ITEM_DEFAULT, "1800" /* 30 minutes */ },
   { "heartbeatinterval", CFG_TYPE_TIME, ITEM(res_client.heartbeat_interval), 0, CFG_ITEM_DEFAULT, "0" },
   { "maximumnetworkbuffersize", CFG_TYPE_PINT32, ITEM(res_client.max_network_buffer_size), 0, 0, NULL },
#ifdef DATA_ENCRYPTION
   { "pkisignatures", CFG_TYPE_BOOL, ITEM(res_client.pki_sign), 0, CFG_ITEM_DEFAULT, "false" },
   { "pkiencryption", CFG_TYPE_BOOL, ITEM(res_client.pki_encrypt), 0, CFG_ITEM_DEFAULT, "false" },
   { "pkikeypair", CFG_TYPE_DIR, ITEM(res_client.pki_keypair_file), 0, 0, NULL },
   { "pkisigner", CFG_TYPE_ALIST_DIR, ITEM(res_client.pki_signing_key_files), 0, 0, NULL },
   { "pkimasterkey", CFG_TYPE_ALIST_DIR, ITEM(res_client.pki_master_key_files), 0, 0, NULL },
   { "pkicipher", CFG_TYPE_CIPHER, ITEM(res_client.pki_cipher), 0, CFG_ITEM_DEFAULT, "aes128" },
#endif
   { "tlsauthenticate", CFG_TYPE_BOOL, ITEM(res_client.tls_authenticate), 0, 0, NULL },
   { "tlsenable", CFG_TYPE_BOOL, ITEM(res_client.tls_enable), 0, 0, NULL },
   { "tlsrequire", CFG_TYPE_BOOL, ITEM(res_client.tls_require), 0, 0, NULL },
   { "tlsverifypeer", CFG_TYPE_BOOL, ITEM(res_client.tls_verify_peer), 0, CFG_ITEM_DEFAULT, "true" },
   { "tlscacertificatefile", CFG_TYPE_DIR, ITEM(res_client.tls_ca_certfile), 0, 0, NULL },
   { "tlscacertificatedir", CFG_TYPE_DIR, ITEM(res_client.tls_ca_certdir), 0, 0, NULL },
   { "tlscertificaterevocationlist", CFG_TYPE_DIR, ITEM(res_client.tls_crlfile), 0, 0, NULL },
   { "tlscertificate", CFG_TYPE_DIR, ITEM(res_client.tls_certfile), 0, 0, NULL },
   { "tlskey", CFG_TYPE_DIR, ITEM(res_client.tls_keyfile), 0, 0, NULL },
   { "verid", CFG_TYPE_STR, ITEM(res_client.verid), 0, 0, NULL },
   { "compatible", CFG_TYPE_BOOL, ITEM(res_client.compatible), 0, CFG_ITEM_DEFAULT, "true" },
   { "maximumbandwidthperjob", CFG_TYPE_SPEED, ITEM(res_client.max_bandwidth_per_job), 0, 0, NULL },
   { "allowbandwidthbursting", CFG_TYPE_BOOL, ITEM(res_client.allow_bw_bursting), 0, CFG_ITEM_DEFAULT, "false" },
   { "allowedscriptdir", CFG_TYPE_ALIST_DIR, ITEM(res_client.allowed_script_dirs), 0, 0, NULL },
   { "allowedjobcommand", CFG_TYPE_ALIST_STR, ITEM(res_client.allowed_job_cmds), 0, 0, NULL },
   { "absolutejobtimeout", CFG_TYPE_PINT32, ITEM(res_client.jcr_watchdog_time), 0, 0, NULL },
   { "alwaysuselmdb", CFG_TYPE_BOOL, ITEM(res_client.always_use_lmdb), 0, CFG_ITEM_DEFAULT, "false" },
   { "lmdbthreshold", CFG_TYPE_PINT32, ITEM(res_client.lmdb_threshold), 0, 0, NULL },
   { NULL, 0, { 0 }, 0, 0, NULL }
};

/*
 * Directors that can use our services
 */
static RES_ITEM dir_items[] = {
   { "name", CFG_TYPE_NAME, ITEM(res_dir.hdr.name), 0, CFG_ITEM_REQUIRED, NULL },
   { "description", CFG_TYPE_STR, ITEM(res_dir.hdr.desc), 0, 0, NULL },
   { "password", CFG_TYPE_MD5PASSWORD, ITEM(res_dir.password), 0, CFG_ITEM_REQUIRED, NULL },
   { "address", CFG_TYPE_STR, ITEM(res_dir.address), 0, 0, NULL },
   { "monitor", CFG_TYPE_BOOL, ITEM(res_dir.monitor), 0, CFG_ITEM_DEFAULT, "false" },
   { "tlsauthenticate", CFG_TYPE_BOOL, ITEM(res_dir.tls_authenticate), 0, 0, NULL },
   { "tlsenable", CFG_TYPE_BOOL, ITEM(res_dir.tls_enable), 0, 0, NULL },
   { "tlsrequire", CFG_TYPE_BOOL, ITEM(res_dir.tls_require), 0, 0, NULL },
   { "tlsverifypeer", CFG_TYPE_BOOL, ITEM(res_dir.tls_verify_peer), 0, CFG_ITEM_DEFAULT, "true" },
   { "tlscacertificatefile", CFG_TYPE_DIR, ITEM(res_dir.tls_ca_certfile), 0, 0, NULL },
   { "tlscacertificatedir", CFG_TYPE_DIR, ITEM(res_dir.tls_ca_certdir), 0, 0, NULL },
   { "tlscertificaterevocationlist", CFG_TYPE_DIR, ITEM(res_dir.tls_crlfile), 0, 0, NULL },
   { "tlscertificate", CFG_TYPE_DIR, ITEM(res_dir.tls_certfile), 0, 0, NULL },
   { "tlskey", CFG_TYPE_DIR, ITEM(res_dir.tls_keyfile), 0, 0, NULL },
   { "tlsdhfile", CFG_TYPE_DIR, ITEM(res_dir.tls_dhfile), 0, 0, NULL },
   { "tlsallowedcn", CFG_TYPE_ALIST_STR, ITEM(res_dir.tls_allowed_cns), 0, 0, NULL },
   { "maximumbandwidthperjob", CFG_TYPE_SPEED, ITEM(res_dir.max_bandwidth_per_job), 0, 0, NULL },
   { "allowedscriptdir", CFG_TYPE_ALIST_DIR, ITEM(res_dir.allowed_script_dirs), 0, 0, NULL },
   { "allowedjobcommand", CFG_TYPE_ALIST_STR, ITEM(res_dir.allowed_job_cmds), 0, 0, NULL },
   { NULL, 0, { 0 }, 0, 0, NULL }
};

/*
 * Message resource
 */
#include "lib/msg_res.h"

/*
 * This is the master resource definition.
 * It must have one item for each of the resources.
 */
static RES_TABLE resources[] = {
   { "director", dir_items, R_DIRECTOR, sizeof(DIRRES) },
   { "filedaemon", cli_items, R_CLIENT, sizeof(CLIENTRES) },
   { "client", cli_items, R_CLIENT, sizeof(CLIENTRES) }, /* alias for filedaemon */
   { "messages", msgs_items, R_MSGS, sizeof(MSGSRES) },
   { NULL, NULL, 0}
};

/*
 * Dump contents of resource
 */
void dump_resource(int type, RES *reshdr, void sendit(void *sock, const char *fmt, ...), void *sock)
{
   URES *res = (URES *)reshdr;
   int recurse = 1;

   if (res == NULL) {
      sendit(sock, "No record for %d %s\n", type, res_to_str(type));
      return;
   }
   if (type < 0) { /* no recursion */
      type = - type;
      recurse = 0;
   }
   switch (type) {
   case R_DIRECTOR:
      sendit(sock, "Director: name=%s password=%s\n", reshdr->name,
             res->res_dir.password.value);
      break;
   case R_CLIENT:
      sendit(sock, "Client: name=%s FDport=%d\n", reshdr->name,
             get_first_port_host_order(res->res_client.FDaddrs));
      break;
   case R_MSGS:
      sendit(sock, "Messages: name=%s\n", res->res_msgs.hdr.name);
      if (res->res_msgs.mail_cmd)
         sendit(sock, "      mailcmd=%s\n", res->res_msgs.mail_cmd);
      if (res->res_msgs.operator_cmd)
         sendit(sock, "      opcmd=%s\n", res->res_msgs.operator_cmd);
      break;
   default:
      sendit(sock, "Unknown resource type %d\n", type);
   }
   if (recurse && res->res_dir.hdr.next) {
      dump_resource(type, res->res_dir.hdr.next, sendit, sock);
   }
}

/*
 * Free memory of resource.
 * NB, we don't need to worry about freeing any references
 * to other resources as they will be freed when that
 * resource chain is traversed.  Mainly we worry about freeing
 * allocated strings (names).
 */
void free_resource(RES *sres, int type)
{
   RES *nres;
   URES *res = (URES *)sres;

   if (res == NULL) {
      return;
   }

   /*
    * Common stuff -- free the resource name
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
      if (res->res_dir.password.value) {
         free(res->res_dir.password.value);
      }
      if (res->res_dir.address) {
         free(res->res_dir.address);
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
      if (res->res_dir.allowed_script_dirs) {
         delete res->res_dir.allowed_script_dirs;
      }
      if (res->res_dir.allowed_job_cmds) {
         delete res->res_dir.allowed_job_cmds;
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
      if (res->res_client.plugin_names) {
         free(res->res_client.plugin_names);
      }
      if (res->res_client.FDaddrs) {
         free_addresses(res->res_client.FDaddrs);
      }
      if (res->res_client.FDsrc_addr) {
         free_addresses(res->res_client.FDsrc_addr);
      }
      if (res->res_client.pki_keypair_file) {
         free(res->res_client.pki_keypair_file);
      }
      if (res->res_client.pki_keypair) {
         crypto_keypair_free(res->res_client.pki_keypair);
      }
      if (res->res_client.pki_signing_key_files) {
         delete res->res_client.pki_signing_key_files;
      }
      if (res->res_client.pki_signers) {
         X509_KEYPAIR *keypair;
         foreach_alist(keypair, res->res_client.pki_signers) {
            crypto_keypair_free(keypair);
         }
         delete res->res_client.pki_signers;
      }
      if (res->res_client.pki_master_key_files) {
         delete res->res_client.pki_master_key_files;
      }
      if (res->res_client.pki_recipients) {
         X509_KEYPAIR *keypair;
         foreach_alist(keypair, res->res_client.pki_recipients) {
            crypto_keypair_free(keypair);
         }
         delete res->res_client.pki_recipients;
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
      if (res->res_client.verid) {
         free(res->res_client.verid);
      }
      if (res->res_client.allowed_script_dirs) {
         delete res->res_client.allowed_script_dirs;
      }
      if (res->res_client.allowed_job_cmds) {
         delete res->res_client.allowed_job_cmds;
      }
      break;
   case R_MSGS:
      if (res->res_msgs.mail_cmd) {
         free(res->res_msgs.mail_cmd);
      }
      if (res->res_msgs.operator_cmd) {
         free(res->res_msgs.operator_cmd);
      }
      free_msgs_res((MSGSRES *)res);  /* free message resource */
      res = NULL;
      break;
   default:
      printf(_("Unknown resource type %d\n"), type);
   }
   /* Common stuff again -- free the resource, recurse to next one */
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
 * pointers (currently only in the Job resource).
 */
void save_resource(int type, RES_ITEM *items, int pass)
{
   URES *res;
   int rindex = type - R_FIRST;
   int i;
   int error = 0;

   /*
    * Ensure that all required items are present
    */
   for (i = 0; items[i].name; i++) {
      if (items[i].flags & CFG_ITEM_REQUIRED) {
            if (!bit_is_set(i, res_all.res_dir.hdr.item_present)) {
               Emsg2(M_ABORT, 0, _("%s item is required in %s resource, but not found.\n"),
                 items[i].name, resources[rindex]);
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
            if ((res = (URES *)GetResWithName(R_DIRECTOR, res_all.res_dir.hdr.name)) == NULL) {
               Emsg1(M_ABORT, 0, _("Cannot find Director resource %s\n"), res_all.res_dir.hdr.name);
            } else {
               res->res_dir.tls_allowed_cns = res_all.res_dir.tls_allowed_cns;
               res->res_dir.allowed_script_dirs = res_all.res_dir.allowed_script_dirs;
               res->res_dir.allowed_job_cmds = res_all.res_dir.allowed_job_cmds;
            }
            break;
         case R_CLIENT:
            if ((res = (URES *)GetResWithName(R_CLIENT, res_all.res_dir.hdr.name)) == NULL) {
               Emsg1(M_ABORT, 0, _("Cannot find Client resource %s\n"), res_all.res_dir.hdr.name);
            } else {
               res->res_client.pki_signing_key_files = res_all.res_client.pki_signing_key_files;
               res->res_client.pki_master_key_files = res_all.res_client.pki_master_key_files;
               res->res_client.pki_signers = res_all.res_client.pki_signers;
               res->res_client.pki_recipients = res_all.res_client.pki_recipients;
               res->res_client.messages = res_all.res_client.messages;
               res->res_client.tls_allowed_cns = res_all.res_client.tls_allowed_cns;
               res->res_client.allowed_script_dirs = res_all.res_client.allowed_script_dirs;
               res->res_client.allowed_job_cmds = res_all.res_client.allowed_job_cmds;
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
      } else {
         RES *next, *last;
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
         Dmsg2(90, "Inserting %s res: %s\n", res_to_str(type),
               res->res_dir.hdr.name);
      }
   }
}

static struct s_kw CryptoCiphers[] = {
   { "aes128", CRYPTO_CIPHER_AES_128_CBC },
   { "aes192", CRYPTO_CIPHER_AES_192_CBC },
   { "aes256", CRYPTO_CIPHER_AES_256_CBC },
   { "camellia128", CRYPTO_CIPHER_CAMELLIA_128_CBC },
   { "camellia192", CRYPTO_CIPHER_CAMELLIA_192_CBC },
   { "camellia256", CRYPTO_CIPHER_CAMELLIA_256_CBC },
   { "aes128hmacsha1", CRYPTO_CIPHER_AES_128_CBC_HMAC_SHA1 },
   { "aes256hmacsha1", CRYPTO_CIPHER_AES_256_CBC_HMAC_SHA1 },
   { "blowfish", CRYPTO_CIPHER_BLOWFISH_CBC },
   { NULL, 0 }
};

static void store_cipher(LEX *lc, RES_ITEM *item, int index, int pass)
{
   int i;
   lex_get_token(lc, T_NAME);

   /*
    * Scan Crypto Ciphers name.
    */
   for (i = 0; CryptoCiphers[i].name; i++) {
      if (bstrcasecmp(lc->str, CryptoCiphers[i].name)) {
         *(uint32_t *)(item->value) = CryptoCiphers[i].token;
         i = 0;
         break;
      }
   }
   if (i != 0) {
      scan_err1(lc, _("Expected a Crypto Cipher option, got: %s"), lc->str);
   }
   scan_to_eol(lc);
   set_bit(index, res_all.hdr.item_present);
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
      case CFG_TYPE_CIPHER:
         for (int i = 0; CryptoCiphers[i].name; i++) {
            if (bstrcasecmp(item->default_value, CryptoCiphers[i].name)) {
               *(uint32_t *)(item->value) = CryptoCiphers[i].token;
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
   case CFG_TYPE_CIPHER:
      store_cipher(lc, item, index, pass);
      break;
   default:
      break;
   }
}

bool parse_fd_config(CONFIG *config, const char *configfile, int exit_code)
{
   config->init(configfile,
                NULL,
                NULL,
                init_resource_cb,
                parse_config_cb,
                NULL,
                exit_code,
                (void *)&res_all,
                res_all_size,
                R_FIRST,
                R_LAST,
                resources,
                res_head);
   return config->parse_config();
}
