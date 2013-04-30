/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2000-2008 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from
   many others, a complete list can be found in the file AUTHORS.
   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.

   Bacula® is a registered trademark of Kern Sibbald.
   The licensor of Bacula is the Free Software Foundation Europe
   (FSFE), Fiduciary Program, Sumatrastrasse 25, 8006 Zürich,
   Switzerland, email:ftf@fsfeurope.org.
*/
/*
 *   Main configuration file parser for Bacula File Daemon (Client)
 *    some parts may be split into separate files such as
 *    the schedule configuration (sch_config.c).
 *
 *   Note, the configuration file parser consists of three parts
 *
 *   1. The generic lexical scanner in lib/lex.c and lib/lex.h
 *
 *   2. The generic config  scanner in lib/parse_config.c and
 *      lib/parse_config.h.
 *      These files contain the parser code, some utility
 *      routines, and the common store routines (name, int,
 *      string).
 *
 *   3. The daemon specific file, which contains the Resource
 *      definitions as well as any specific store routines
 *      for the resource records.
 *
 *     Kern Sibbald, September MM
 *
 *   Version $Id$
 */

#include "bacula.h"
#include "filed.h"

/* Define the first and last resource ID record
 * types. Note, these should be unique for each
 * daemon though not a requirement.
 */
int32_t r_first = R_FIRST;
int32_t r_last  = R_LAST;
static RES *sres_head[R_LAST - R_FIRST + 1];
RES **res_head = sres_head;


/* Forward referenced subroutines */


/* We build the current resource here as we are
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

/* Definition of records permitted within each
 * resource with the routine to process the record
 * information.
 */

/* Client or File daemon "Global" resources */
static RES_ITEM cli_items[] = {
   {"name",        store_name,  ITEM(res_client.hdr.name), 0, ITEM_REQUIRED, 0},
   {"description", store_str,   ITEM(res_client.hdr.desc), 0, 0, 0},
   {"fdport",      store_addresses_port,    ITEM(res_client.FDaddrs),  0, ITEM_DEFAULT, 9102},
   {"fdaddress",   store_addresses_address, ITEM(res_client.FDaddrs),  0, ITEM_DEFAULT, 9102},
   {"fdaddresses", store_addresses,         ITEM(res_client.FDaddrs),  0, ITEM_DEFAULT, 9102},
   {"fdsourceaddress", store_addresses_address, ITEM(res_client.FDsrc_addr),  0, ITEM_DEFAULT, 0},

   {"workingdirectory",  store_dir, ITEM(res_client.working_directory), 0, ITEM_REQUIRED, 0},
   {"piddirectory",  store_dir,     ITEM(res_client.pid_directory),     0, ITEM_REQUIRED, 0},
   {"subsysdirectory",  store_dir,  ITEM(res_client.subsys_directory),  0, 0, 0},
   {"plugindirectory",  store_dir,  ITEM(res_client.plugin_directory),  0, 0, 0},
   {"scriptsdirectory", store_dir,  ITEM(res_client.scripts_directory),  0, 0, 0},
   {"maximumconcurrentjobs", store_pint32,  ITEM(res_client.MaxConcurrentJobs), 0, ITEM_DEFAULT, 20},
   {"messages",      store_res, ITEM(res_client.messages), R_MSGS, 0, 0},
   {"sdconnecttimeout", store_time,ITEM(res_client.SDConnectTimeout), 0, ITEM_DEFAULT, 60 * 30},
   {"heartbeatinterval", store_time, ITEM(res_client.heartbeat_interval), 0, ITEM_DEFAULT, 0},
   {"maximumnetworkbuffersize", store_pint32, ITEM(res_client.max_network_buffer_size), 0, 0, 0},
#ifdef DATA_ENCRYPTION
   {"pkisignatures",         store_bool,    ITEM(res_client.pki_sign), 0, ITEM_DEFAULT, 0},
   {"pkiencryption",         store_bool,    ITEM(res_client.pki_encrypt), 0, ITEM_DEFAULT, 0},
   {"pkikeypair",            store_dir,       ITEM(res_client.pki_keypair_file), 0, 0, 0},
   {"pkisigner",             store_alist_str, ITEM(res_client.pki_signing_key_files), 0, 0, 0},
   {"pkimasterkey",          store_alist_str, ITEM(res_client.pki_master_key_files), 0, 0, 0},
#endif
   {"tlsauthenticate",       store_bool,    ITEM(res_client.tls_authenticate),  0, 0, 0},
   {"tlsenable",             store_bool,    ITEM(res_client.tls_enable),  0, 0, 0},
   {"tlsrequire",            store_bool,    ITEM(res_client.tls_require), 0, 0, 0},
   {"tlscacertificatefile",  store_dir,       ITEM(res_client.tls_ca_certfile), 0, 0, 0},
   {"tlscacertificatedir",   store_dir,       ITEM(res_client.tls_ca_certdir), 0, 0, 0},
   {"tlscertificate",        store_dir,       ITEM(res_client.tls_certfile), 0, 0, 0},
   {"tlskey",                store_dir,       ITEM(res_client.tls_keyfile), 0, 0, 0},
   {"verid",                 store_str,       ITEM(res_client.verid), 0, 0, 0},
   {NULL, NULL, {0}, 0, 0, 0}
};

/* Directors that can use our services */
static RES_ITEM dir_items[] = {
   {"name",        store_name,     ITEM(res_dir.hdr.name),  0, ITEM_REQUIRED, 0},
   {"description", store_str,      ITEM(res_dir.hdr.desc),  0, 0, 0},
   {"password",    store_password, ITEM(res_dir.password),  0, ITEM_REQUIRED, 0},
   {"address",     store_str,      ITEM(res_dir.address),   0, 0, 0},
   {"monitor",     store_bool,   ITEM(res_dir.monitor),   0, ITEM_DEFAULT, 0},
   {"tlsauthenticate",      store_bool,    ITEM(res_dir.tls_authenticate), 0, 0, 0},
   {"tlsenable",            store_bool,    ITEM(res_dir.tls_enable), 0, 0, 0},
   {"tlsrequire",           store_bool,    ITEM(res_dir.tls_require), 0, 0, 0},
   {"tlsverifypeer",        store_bool,    ITEM(res_dir.tls_verify_peer), 0, ITEM_DEFAULT, 1},
   {"tlscacertificatefile", store_dir,       ITEM(res_dir.tls_ca_certfile), 0, 0, 0},
   {"tlscacertificatedir",  store_dir,       ITEM(res_dir.tls_ca_certdir), 0, 0, 0},
   {"tlscertificate",       store_dir,       ITEM(res_dir.tls_certfile), 0, 0, 0},
   {"tlskey",               store_dir,       ITEM(res_dir.tls_keyfile), 0, 0, 0},
   {"tlsdhfile",            store_dir,       ITEM(res_dir.tls_dhfile), 0, 0, 0},
   {"tlsallowedcn",         store_alist_str, ITEM(res_dir.tls_allowed_cns), 0, 0, 0},
   {NULL, NULL, {0}, 0, 0, 0}
};

/* Message resource */
extern RES_ITEM msgs_items[];

/*
 * This is the master resource definition.
 * It must have one item for each of the resources.
 */
RES_TABLE resources[] = {
   {"director",      dir_items,   R_DIRECTOR},
   {"filedaemon",    cli_items,   R_CLIENT},
   {"client",        cli_items,   R_CLIENT},     /* alias for filedaemon */
   {"messages",      msgs_items,  R_MSGS},
   {NULL,            NULL,        0}
};


/* Dump contents of resource */
void dump_resource(int type, RES *reshdr, void sendit(void *sock, const char *fmt, ...), void *sock)
{
   URES *res = (URES *)reshdr;
   int recurse = 1;

   if (res == NULL) {
      sendit(sock, "No record for %d %s\n", type, res_to_str(type));
      return;
   }
   if (type < 0) {                    /* no recursion */
      type = - type;
      recurse = 0;
   }
   switch (type) {
   case R_DIRECTOR:
      sendit(sock, "Director: name=%s password=%s\n", reshdr->name,
              res->res_dir.password);
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

   /* common stuff -- free the resource name */
   nres = (RES *)res->res_dir.hdr.next;
   if (res->res_dir.hdr.name) {
      free(res->res_dir.hdr.name);
   }
   if (res->res_dir.hdr.desc) {
      free(res->res_dir.hdr.desc);
   }
   switch (type) {
   case R_DIRECTOR:
      if (res->res_dir.password) {
         free(res->res_dir.password);
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
      if (res->res_client.tls_certfile) {
         free(res->res_client.tls_certfile);
      }
      if (res->res_client.tls_keyfile) {
         free(res->res_client.tls_keyfile);
      }
      if (res->res_client.verid) {
         free(res->res_client.verid);
      }
      break;
   case R_MSGS:
      if (res->res_msgs.mail_cmd)
         free(res->res_msgs.mail_cmd);
      if (res->res_msgs.operator_cmd)
         free(res->res_msgs.operator_cmd);
      free_msgs_res((MSGS *)res);  /* free message resource */
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

/* Save the new resource by chaining it into the head list for
 * the resource. If this is pass 2, we update any resource
 * pointers (currently only in the Job resource).
 */
void save_resource(int type, RES_ITEM *items, int pass)
{
   URES *res;
   int rindex = type - r_first;
   int i, size;
   int error = 0;

   /*
    * Ensure that all required items are present
    */
   for (i=0; items[i].name; i++) {
      if (items[i].flags & ITEM_REQUIRED) {
            if (!bit_is_set(i, res_all.res_dir.hdr.item_present)) {
               Emsg2(M_ABORT, 0, _("%s item is required in %s resource, but not found.\n"),
                 items[i].name, resources[rindex]);
             }
      }
   }

   /* During pass 2, we looked up pointers to all the resources
    * referrenced in the current resource, , now we
    * must copy their address from the static record to the allocated
    * record.
    */
   if (pass == 2) {
      switch (type) {
         /* Resources not containing a resource */
         case R_MSGS:
            break;

         /* Resources containing another resource */
         case R_DIRECTOR:
            if ((res = (URES *)GetResWithName(R_DIRECTOR, res_all.res_dir.hdr.name)) == NULL) {
               Emsg1(M_ABORT, 0, _("Cannot find Director resource %s\n"), res_all.res_dir.hdr.name);
            }
            res->res_dir.tls_allowed_cns = res_all.res_dir.tls_allowed_cns;
            break;
         case R_CLIENT:
            if ((res = (URES *)GetResWithName(R_CLIENT, res_all.res_dir.hdr.name)) == NULL) {
               Emsg1(M_ABORT, 0, _("Cannot find Client resource %s\n"), res_all.res_dir.hdr.name);
            }
            res->res_client.pki_signing_key_files = res_all.res_client.pki_signing_key_files;
            res->res_client.pki_master_key_files = res_all.res_client.pki_master_key_files;

            res->res_client.pki_signers = res_all.res_client.pki_signers;
            res->res_client.pki_recipients = res_all.res_client.pki_recipients;

            res->res_client.messages = res_all.res_client.messages;
            break;
         default:
            Emsg1(M_ERROR, 0, _("Unknown resource type %d\n"), type);
            error = 1;
            break;
      }
      /* Note, the resoure name was already saved during pass 1,
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

   /* The following code is only executed on pass 1 */
   switch (type) {
      case R_DIRECTOR:
         size = sizeof(DIRRES);
         break;
      case R_CLIENT:
         size = sizeof(CLIENT);
         break;
      case R_MSGS:
         size = sizeof(MSGS);
         break;
      default:
         printf(_("Unknown resource type %d\n"), type);
         error = 1;
         size = 1;
         break;
   }
   /* Common */
   if (!error) {
      res = (URES *)malloc(size);
      memcpy(res, &res_all, size);
      if (!res_head[rindex]) {
         res_head[rindex] = (RES *)res; /* store first entry */
      } else {
         RES *next, *last;
         /* Add new res to end of chain */
         for (last=next=res_head[rindex]; next; next=next->next) {
            last = next;
            if (strcmp(next->name, res->res_dir.hdr.name) == 0) {
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

bool parse_fd_config(CONFIG *config, const char *configfile, int exit_code)
{
   config->init(configfile, NULL, exit_code, (void *)&res_all, res_all_size,
      r_first, r_last, resources, res_head);
   return config->parse_config();
}
