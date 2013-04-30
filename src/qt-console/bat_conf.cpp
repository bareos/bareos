/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2000-2009 Free Software Foundation Europe e.V.

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
 *   Main configuration file parser for Bacula User Agent
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
 *     Kern Sibbald, January MM, September MM
 *
 *     Version $Id$
 */

#include "bacula.h"
#include "bat_conf.h"

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
extern "C" URES res_all; /* declare as C to avoid name mangling by visual c */
#endif
URES res_all;
int32_t res_all_size = sizeof(res_all);

/* Definition of records permitted within each
 * resource with the routine to process the record
 * information.
 */
static RES_ITEM dir_items[] = {
   {"name",        store_name,     ITEM(dir_res.hdr.name), 0, ITEM_REQUIRED, 0},
   {"description", store_str,      ITEM(dir_res.hdr.desc), 0, 0, 0},
   {"dirport",     store_pint32,   ITEM(dir_res.DIRport),  0, ITEM_DEFAULT, 9101},
   {"address",     store_str,      ITEM(dir_res.address),  0, ITEM_REQUIRED, 0},
   {"password",    store_password, ITEM(dir_res.password), 0, 0, 0},
   {"tlsauthenticate",store_bool,    ITEM(dir_res.tls_authenticate), 0, 0, 0},
   {"tlsenable",      store_bool,    ITEM(dir_res.tls_enable), 0, 0, 0},
   {"tlsrequire",     store_bool,    ITEM(dir_res.tls_require), 0, 0, 0},
   {"tlscacertificatefile", store_dir, ITEM(dir_res.tls_ca_certfile), 0, 0, 0},
   {"tlscacertificatedir", store_dir,  ITEM(dir_res.tls_ca_certdir), 0, 0, 0},
   {"tlscertificate", store_dir,       ITEM(dir_res.tls_certfile), 0, 0, 0},
   {"tlskey",         store_dir,       ITEM(dir_res.tls_keyfile), 0, 0, 0},
   {"heartbeatinterval", store_time, ITEM(dir_res.heartbeat_interval), 0, ITEM_DEFAULT, 0},
   {NULL, NULL, {0}, 0, 0, 0}
};

static RES_ITEM con_items[] = {
   {"name",        store_name,     ITEM(con_res.hdr.name), 0, ITEM_REQUIRED, 0},
   {"description", store_str,      ITEM(con_res.hdr.desc), 0, 0, 0},
   {"password",    store_password, ITEM(con_res.password), 0, ITEM_REQUIRED, 0},
   {"tlsauthenticate",store_bool,    ITEM(con_res.tls_authenticate), 0, 0, 0},
   {"tlsenable",      store_bool,    ITEM(con_res.tls_enable), 0, 0, 0},
   {"tlsrequire",     store_bool,    ITEM(con_res.tls_require), 0, 0, 0},
   {"tlscacertificatefile", store_dir, ITEM(con_res.tls_ca_certfile), 0, 0, 0},
   {"tlscacertificatedir", store_dir,  ITEM(con_res.tls_ca_certdir), 0, 0, 0},
   {"tlscertificate", store_dir,       ITEM(con_res.tls_certfile), 0, 0, 0},
   {"tlskey",         store_dir,       ITEM(con_res.tls_keyfile), 0, 0, 0},
   {"heartbeatinterval", store_time, ITEM(con_res.heartbeat_interval), 0, ITEM_DEFAULT, 0},
   {NULL, NULL, {0}, 0, 0, 0}
};

static RES_ITEM con_font_items[] = {
   {"name",        store_name,     ITEM(con_font.hdr.name), 0, ITEM_REQUIRED, 0},
   {"description", store_str,      ITEM(con_font.hdr.desc), 0, 0, 0},
   {"font",        store_str,      ITEM(con_font.fontface), 0, 0, 0},
   {NULL, NULL, {0}, 0, 0, 0}
};


/*
 * This is the master resource definition.
 * It must have one item for each of the resources.
 */
RES_TABLE resources[] = {
   {"director",      dir_items,   R_DIRECTOR},
   {"console",       con_items,   R_CONSOLE},
   {"consolefont",   con_font_items, R_CONSOLE_FONT},
   {NULL,            NULL,        0}
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
   case R_DIRECTOR:
      printf(_("Director: name=%s address=%s DIRport=%d\n"), reshdr->name,
              res->dir_res.address, res->dir_res.DIRport);
      break;
   case R_CONSOLE:
      printf(_("Console: name=%s\n"), reshdr->name);
      break;
   case R_CONSOLE_FONT:
      printf(_("ConsoleFont: name=%s font face=%s\n"),
             reshdr->name, NPRT(res->con_font.fontface));
      break;
   default:
      printf(_("Unknown resource type %d\n"), type);
   }
   if (recurse && res->dir_res.hdr.next) {
      dump_resource(type, res->dir_res.hdr.next, sendit, sock);
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
   nres = (RES *)res->dir_res.hdr.next;
   if (res->dir_res.hdr.name) {
      free(res->dir_res.hdr.name);
   }
   if (res->dir_res.hdr.desc) {
      free(res->dir_res.hdr.desc);
   }

   switch (type) {
   case R_DIRECTOR:
      if (res->dir_res.address) {
         free(res->dir_res.address);
      }
      if (res->dir_res.tls_ctx) { 
         free_tls_context(res->dir_res.tls_ctx);
      }
      if (res->dir_res.tls_ca_certfile) {
         free(res->dir_res.tls_ca_certfile);
      }
      if (res->dir_res.tls_ca_certdir) {
         free(res->dir_res.tls_ca_certdir);
      }
      if (res->dir_res.tls_certfile) {
         free(res->dir_res.tls_certfile);
      }
      if (res->dir_res.tls_keyfile) {
         free(res->dir_res.tls_keyfile);
      }
      break;
   case R_CONSOLE:
      if (res->con_res.password) {
         free(res->con_res.password);
      }
      if (res->con_res.tls_ctx) { 
         free_tls_context(res->con_res.tls_ctx);
      }
      if (res->con_res.tls_ca_certfile) {
         free(res->con_res.tls_ca_certfile);
      }
      if (res->con_res.tls_ca_certdir) {
         free(res->con_res.tls_ca_certdir);
      }
      if (res->con_res.tls_certfile) {
         free(res->con_res.tls_certfile);
      }
      if (res->con_res.tls_keyfile) {
         free(res->con_res.tls_keyfile);
      }
      break;
   case R_CONSOLE_FONT:
      if (res->con_font.fontface) {
         free(res->con_font.fontface);
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
   int i, size = 0;
   int error = 0;

   /*
    * Ensure that all required items are present
    */
   for (i=0; items[i].name; i++) {
      if (items[i].flags & ITEM_REQUIRED) {
            if (!bit_is_set(i, res_all.dir_res.hdr.item_present)) {
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
      case R_DIRECTOR:
         break;

      case R_CONSOLE:
      case R_CONSOLE_FONT:
         break;

      default:
         Emsg1(M_ERROR, 0, _("Unknown resource type %d\n"), type);
         error = 1;
         break;
      }
      /* Note, the resoure name was already saved during pass 1,
       * so here, we can just release it.
       */
      if (res_all.dir_res.hdr.name) {
         free(res_all.dir_res.hdr.name);
         res_all.dir_res.hdr.name = NULL;
      }
      if (res_all.dir_res.hdr.desc) {
         free(res_all.dir_res.hdr.desc);
         res_all.dir_res.hdr.desc = NULL;
      }
      return;
   }

   /* The following code is only executed during pass 1 */
   switch (type) {
   case R_DIRECTOR:
      size = sizeof(DIRRES);
      break;
   case R_CONSOLE_FONT:
      size = sizeof(CONFONTRES);
      break;
   case R_CONSOLE:
      size = sizeof(CONRES);
      break;
   default:
      printf(_("Unknown resource type %d\n"), type);
      error = 1;
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
            if (strcmp(next->name, res->dir_res.hdr.name) == 0) {
               Emsg2(M_ERROR_TERM, 0,
                  _("Attempt to define second %s resource named \"%s\" is not permitted.\n"),
                  resources[rindex].name, res->dir_res.hdr.name);
            }
         }
         last->next = (RES *)res;
         Dmsg2(90, "Inserting %s res: %s\n", res_to_str(type),
               res->dir_res.hdr.name);
      }
   }
}

bool parse_bat_config(CONFIG *config, const char *configfile, int exit_code)
{
   config->init(configfile, NULL, exit_code, (void *)&res_all, res_all_size,
      r_first, r_last, resources, res_head);
   return config->parse_config();
}
