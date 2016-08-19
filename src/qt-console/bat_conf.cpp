/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2009 Free Software Foundation Europe e.V.
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
#include "bat_conf.h"

/*
 * Define the first and last resource ID record
 * types. Note, these should be unique for each
 * daemon though not a requirement.
 */
static RES *sres_head[R_LAST - R_FIRST + 1];
static RES **res_head = sres_head;

/* Forward referenced subroutines */


/*
 * We build the current resource here as we are
 * scanning the resource configuration definition,
 * then move it to allocated memory when the resource
 * scan is complete.
 */
#if defined(_MSC_VER)
extern "C" URES res_all; /* declare as C to avoid name mangling by visual c */
#endif
URES res_all;
int32_t res_all_size = sizeof(res_all);

/*
 * Definition of records permitted within each
 * resource with the routine to process the record
 * information.
 */
static RES_ITEM dir_items[] = {
   { "Name", CFG_TYPE_NAME, ITEM(dir_res.hdr.name), 0, CFG_ITEM_REQUIRED, NULL, NULL, NULL },
   { "Description", CFG_TYPE_STR, ITEM(dir_res.hdr.desc), 0, 0, NULL, NULL, NULL },
   { "DirPort", CFG_TYPE_PINT32, ITEM(dir_res.DIRport), 0, CFG_ITEM_DEFAULT, DIR_DEFAULT_PORT, NULL, NULL },
   { "Address", CFG_TYPE_STR, ITEM(dir_res.address), 0, CFG_ITEM_REQUIRED, NULL, NULL, NULL },
   { "Password", CFG_TYPE_MD5PASSWORD, ITEM(dir_res.password), 0, CFG_ITEM_REQUIRED, NULL, NULL, NULL },
   { "HeartBeatInterval", CFG_TYPE_TIME, ITEM(dir_res.heartbeat_interval), 0, CFG_ITEM_DEFAULT, "0", NULL, NULL },
   TLS_CONFIG(dir_res)
   { NULL, 0, { 0 }, 0, 0, NULL, NULL, NULL }
};

static RES_ITEM con_items[] = {
   { "Name", CFG_TYPE_NAME, ITEM(con_res.hdr.name), 0, CFG_ITEM_REQUIRED, NULL, NULL, NULL },
   { "Description", CFG_TYPE_STR, ITEM(con_res.hdr.desc), 0, 0, NULL, NULL, NULL },
   { "Password", CFG_TYPE_MD5PASSWORD, ITEM(con_res.password), 0, CFG_ITEM_REQUIRED, NULL, NULL, NULL },
   { "HeartBeatInterval", CFG_TYPE_TIME, ITEM(con_res.heartbeat_interval), 0, CFG_ITEM_DEFAULT, "0", NULL, NULL },
   TLS_CONFIG(dir_res)
   { NULL, 0, { 0 }, 0, 0, NULL, NULL, NULL }
};

static RES_ITEM con_font_items[] = {
   { "Name", CFG_TYPE_NAME, ITEM(con_font.hdr.name), 0, CFG_ITEM_REQUIRED, NULL, NULL, NULL },
   { "Description", CFG_TYPE_STR, ITEM(con_font.hdr.desc), 0, 0, NULL, NULL, NULL },
   { "Font", CFG_TYPE_STR, ITEM(con_font.fontface), 0, 0, NULL, NULL, NULL },
   { NULL, 0, { 0 }, 0, 0, NULL, NULL, NULL }
};

/*
 * This is the master resource definition.
 * It must have one item for each of the resources.
 */
static RES_TABLE resources[] = {
   { "Director", dir_items, R_DIRECTOR, sizeof(DIRRES) },
   { "Console", con_items, R_CONSOLE, sizeof(CONRES) },
   { "ConsoleFont", con_font_items, R_CONSOLE_FONT, sizeof(CONFONTRES) },
   { NULL, NULL, 0, 0 }
};

/*
 * Dump contents of resource
 */
void dump_resource(int type, RES *reshdr,
                   void sendit(void *sock, const char *fmt, ...),
                   void *sock, bool hide_sensitive_data, bool verbose)
{
   URES *res = (URES *)reshdr;
   bool recurse = true;

   if (res == NULL) {
      printf(_("No record for %d %s\n"), type, res_to_str(type));
      return;
   }
   if (type < 0) { /* no recursion */
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
      dump_resource(type, res->dir_res.hdr.next, sendit, sock, hide_sensitive_data, verbose);
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

   /*
    * Common stuff -- free the resource name
    */
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
      free_tls_t(res->dir_res.tls);
      break;
   case R_CONSOLE:
      if (res->con_res.password.value) {
         free(res->con_res.password.value);
      }
      free_tls_t(res->con_res.tls);
      break;
   case R_CONSOLE_FONT:
      if (res->con_font.fontface) {
         free(res->con_font.fontface);
      }
      break;
   default:
      printf(_("Unknown resource type %d\n"), type);
   }
   /*
    * Common stuff again -- free the resource, recurse to next one
    */
   free(res);
   if (nres) {
      free_resource(nres, type);
   }
}

/*
 * Save the new resource by chaining it into the head list for
 * the resource. If this is pass 2, we update any resource
 * pointers (currently only in the Job resource).
 */
bool save_resource(int type, RES_ITEM *items, int pass)
{
   URES *res;
   int rindex = type - R_FIRST;
   int i;
   int error = 0;

   /*
    * Ensure that all required items are present
    */
   for (i=0; items[i].name; i++) {
      if (items[i].flags & CFG_ITEM_REQUIRED) {
            if (!bit_is_set(i, res_all.dir_res.hdr.item_present)) {
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
      /*
       * Resources not containing a resource
       */
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
      /*
       * Note, the resoure name was already saved during pass 1,
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
      return (error == 0);
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
         for (last=next=res_head[rindex]; next; next=next->next) {
            last = next;
            if (strcmp(next->name, res->dir_res.name()) == 0) {
               Emsg2(M_ERROR_TERM, 0,
                  _("Attempt to define second %s resource named \"%s\" is not permitted.\n"),
                  resources[rindex].name, res->dir_res.name());
            }
         }
         last->next = (RES *)res;
         Dmsg2(90, "Inserting %s res: %s\n", res_to_str(type), res->dir_res.name());
      }
   }
   return (error == 0);
}

bool parse_bat_config(CONFIG *config, const char *configfile, int exit_code)
{
   config->init(configfile,
                NULL,
                NULL,
                NULL,
                NULL,
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
