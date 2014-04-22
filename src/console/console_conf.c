/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2009 Free Software Foundation Europe e.V.
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
 * Main configuration file parser for Bareos User Agent
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
 * Kern Sibbald, January MM, September MM
 */

#include "bareos.h"
#include "console_conf.h"

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

/*  Console "globals" */
static RES_ITEM cons_items[] = {
   { "name", store_name, ITEM(res_cons.hdr.name), 0, ITEM_REQUIRED, NULL },
   { "description", store_str, ITEM(res_cons.hdr.desc), 0, 0, NULL },
   { "rcfile", store_dir, ITEM(res_cons.rc_file), 0, 0, NULL },
   { "historyfile", store_dir, ITEM(res_cons.hist_file), 0, 0, NULL },
   { "password", store_password, ITEM(res_cons.password), 0, ITEM_REQUIRED, NULL },
   { "tlsauthenticate",store_bool, ITEM(res_cons.tls_authenticate), 0, 0, NULL },
   { "tlsenable", store_bool, ITEM(res_cons.tls_enable), 0, 0, NULL },
   { "tlsrequire", store_bool, ITEM(res_cons.tls_require), 0, 0, NULL },
   { "tlsverifypeer", store_bool, ITEM(res_cons.tls_verify_peer), 0, ITEM_DEFAULT, "true" },
   { "tlscacertificatefile", store_dir, ITEM(res_cons.tls_ca_certfile), 0, 0, NULL },
   { "tlscacertificatedir", store_dir, ITEM(res_cons.tls_ca_certdir), 0, 0, NULL },
   { "tlscertificaterevocationlist", store_dir, ITEM(res_cons.tls_crlfile), 0, 0, NULL },
   { "tlscertificate", store_dir, ITEM(res_cons.tls_certfile), 0, 0, NULL },
   { "tlskey", store_dir, ITEM(res_cons.tls_keyfile), 0, 0, NULL },
   { "director", store_str, ITEM(res_cons.director), 0, 0, NULL },
   { "heartbeatinterval", store_time, ITEM(res_cons.heartbeat_interval), 0, ITEM_DEFAULT, "0" },
   { NULL, NULL, { 0 }, 0, 0, NULL }
};

/*  Director's that we can contact */
static RES_ITEM dir_items[] = {
   { "name", store_name, ITEM(res_dir.hdr.name), 0, ITEM_REQUIRED, NULL },
   { "description", store_str, ITEM(res_dir.hdr.desc), 0, 0, NULL },
   { "dirport", store_pint32, ITEM(res_dir.DIRport), 0, ITEM_DEFAULT, DIR_DEFAULT_PORT },
   { "address", store_str, ITEM(res_dir.address), 0, 0, NULL },
   { "password", store_password, ITEM(res_dir.password), 0, ITEM_REQUIRED, NULL },
   { "tlsauthenticate",store_bool, ITEM(res_dir.tls_enable), 0, 0, NULL },
   { "tlsenable", store_bool, ITEM(res_dir.tls_enable), 0, 0, NULL },
   { "tlsrequire", store_bool, ITEM(res_dir.tls_require), 0, 0, NULL },
   { "tlsverifypeer", store_bool, ITEM(res_dir.tls_verify_peer), 0, ITEM_DEFAULT, "true" },
   { "tlscacertificatefile", store_dir, ITEM(res_dir.tls_ca_certfile), 0, 0, NULL },
   { "tlscacertificatedir", store_dir, ITEM(res_dir.tls_ca_certdir), 0, 0, NULL },
   { "tlscertificaterevocationlist", store_dir, ITEM(res_dir.tls_crlfile), 0, 0, NULL },
   { "tlscertificate", store_dir, ITEM(res_dir.tls_certfile), 0, 0, NULL },
   { "tlskey", store_dir, ITEM(res_dir.tls_keyfile), 0, 0, NULL },
   { "heartbeatinterval", store_time, ITEM(res_dir.heartbeat_interval), 0, ITEM_DEFAULT, "0" },
   { NULL, NULL, { 0 }, 0, 0, NULL }
};

/*
 * This is the master resource definition.
 * It must have one item for each of the resources.
 */
RES_TABLE resources[] = {
   { "console", cons_items, R_CONSOLE },
   { "director", dir_items, R_DIRECTOR },
   { NULL, NULL, 0 }
};

/* Dump contents of resource */
void dump_resource(int type, RES *reshdr, void sendit(void *sock, const char *fmt, ...), void *sock)
{
   URES *res = (URES *)reshdr;
   bool recurse = true;

   if (res == NULL) {
      printf(_("No record for %d %s\n"), type, res_to_str(type));
      return;
   }
   if (type < 0) {                    /* no recursion */
      type = - type;
      recurse = false;
   }
   switch (type) {
   case R_CONSOLE:
      printf(_("Console: name=%s rcfile=%s histfile=%s\n"), reshdr->name,
             res->res_cons.rc_file, res->res_cons.hist_file);
      break;
   case R_DIRECTOR:
      printf(_("Director: name=%s address=%s DIRport=%d\n"), reshdr->name,
              res->res_dir.address, res->res_dir.DIRport);
      break;
   default:
      printf(_("Unknown resource type %d\n"), type);
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

   if (res == NULL)
      return;

   /* common stuff -- free the resource name */
   nres = (RES *)res->res_dir.hdr.next;
   if (res->res_dir.hdr.name) {
      free(res->res_dir.hdr.name);
   }
   if (res->res_dir.hdr.desc) {
      free(res->res_dir.hdr.desc);
   }

   switch (type) {
   case R_CONSOLE:
      if (res->res_cons.rc_file) {
         free(res->res_cons.rc_file);
      }
      if (res->res_cons.hist_file) {
         free(res->res_cons.hist_file);
      }
      if (res->res_cons.tls_ctx) {
         free_tls_context(res->res_cons.tls_ctx);
      }
      if (res->res_cons.tls_ca_certfile) {
         free(res->res_cons.tls_ca_certfile);
      }
      if (res->res_cons.tls_ca_certdir) {
         free(res->res_cons.tls_ca_certdir);
      }
      if (res->res_cons.tls_crlfile) {
         free(res->res_cons.tls_crlfile);
      }
      if (res->res_cons.tls_certfile) {
         free(res->res_cons.tls_certfile);
      }
      if (res->res_cons.tls_keyfile) {
         free(res->res_cons.tls_keyfile);
      }
      break;
   case R_DIRECTOR:
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
      break;
   default:
      printf(_("Unknown resource type %d\n"), type);
   }
   /* Common stuff again -- free the resource, recurse to next one */
   free(res);
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
   for (i = 0; items[i].name; i++) {
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
         case R_CONSOLE:
         case R_DIRECTOR:
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

   /* The following code is only executed during pass 1 */
   switch (type) {
   case R_CONSOLE:
      size = sizeof(CONRES);
      break;
   case R_DIRECTOR:
      size = sizeof(DIRRES);
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
         for (last=next=res_head[rindex]; next; next=next->next) {
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

bool parse_cons_config(CONFIG *config, const char *configfile, int exit_code)
{
   config->init(configfile, NULL, NULL, NULL, exit_code,
                (void *)&res_all, res_all_size, r_first,
                r_last, resources, res_head);
   return config->parse_config();
}
