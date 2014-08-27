/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2004-2011 Free Software Foundation Europe e.V.
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
 * Main configuration file parser for Bareos Tray Monitor.
 * Adapted from dird_conf.c
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
 * Nicolas Boichat, August MMIV
 */

#include "bareos.h"
#include "tray_conf.h"

/*
 * Define the first and last resource ID record
 * types. Note, these should be unique for each
 * daemon though not a requirement.
 */
static RES *sres_head[R_LAST - R_FIRST + 1];
static RES **res_head = sres_head;

/*
 * We build the current resource here as we are
 * scanning the resource configuration definition,
 * then move it to allocated memory when the resource
 * scan is complete.
 */
URES res_all;
int32_t res_all_size = sizeof(res_all);

/*
 * Definition of records permitted within each
 * resource with the routine to process the record
 * information.  NOTE! quoted names must be in lower case.
 */
/*
 * Monitor Resource
 *
 * name handler value code flags default_value
 */
static RES_ITEM mon_items[] = {
   { "name", CFG_TYPE_NAME, ITEM(res_monitor.hdr.name), 0, CFG_ITEM_REQUIRED, 0 },
   { "description", CFG_TYPE_STR, ITEM(res_monitor.hdr.desc), 0, 0, 0 },
   { "requiressl", CFG_TYPE_BOOL, ITEM(res_monitor.require_ssl), 0, CFG_ITEM_DEFAULT, "false" },
   { "password", CFG_TYPE_MD5PASSWORD, ITEM(res_monitor.password), 0, CFG_ITEM_REQUIRED, NULL },
   { "refreshinterval", CFG_TYPE_TIME, ITEM(res_monitor.RefreshInterval), 0, CFG_ITEM_DEFAULT, "60" },
   { "fdconnecttimeout", CFG_TYPE_TIME, ITEM(res_monitor.FDConnectTimeout), 0, CFG_ITEM_DEFAULT, "10" },
   { "sdconnecttimeout", CFG_TYPE_TIME, ITEM(res_monitor.SDConnectTimeout), 0, CFG_ITEM_DEFAULT, "10" },
   { "dirconnecttimeout", CFG_TYPE_TIME, ITEM(res_monitor.DIRConnectTimeout), 0, CFG_ITEM_DEFAULT, "10" },
   { NULL, 0, { 0 }, 0, 0, NULL }
};

/*
 * Director's that we can contact
 *
 * name handler value code flags default_value
 */
static RES_ITEM dir_items[] = {
   { "name", CFG_TYPE_NAME, ITEM(res_dir.hdr.name), 0, CFG_ITEM_REQUIRED, NULL },
   { "description", CFG_TYPE_STR, ITEM(res_dir.hdr.desc), 0, 0, NULL },
   { "dirport", CFG_TYPE_PINT32, ITEM(res_dir.DIRport), 0, CFG_ITEM_DEFAULT, DIR_DEFAULT_PORT },
   { "address", CFG_TYPE_STR, ITEM(res_dir.address), 0, CFG_ITEM_REQUIRED, NULL },
   { "enablessl", CFG_TYPE_BOOL, ITEM(res_dir.enable_ssl), 0, CFG_ITEM_DEFAULT, "false" },
   { NULL, 0, { 0 }, 0, 0, NULL }
};

/*
 * Client or File daemon resource
 *
 * name handler value code flags default_value
 */
static RES_ITEM cli_items[] = {
   { "name", CFG_TYPE_NAME, ITEM(res_client.hdr.name), 0, CFG_ITEM_REQUIRED, NULL },
   { "description", CFG_TYPE_STR, ITEM(res_client.hdr.desc), 0, 0, NULL },
   { "address", CFG_TYPE_STR, ITEM(res_client.address), 0, CFG_ITEM_REQUIRED, NULL },
   { "fdport", CFG_TYPE_PINT32, ITEM(res_client.FDport), 0, CFG_ITEM_DEFAULT, FD_DEFAULT_PORT },
   { "password", CFG_TYPE_MD5PASSWORD, ITEM(res_client.password), 0, CFG_ITEM_REQUIRED, NULL },
   { "enablessl", CFG_TYPE_BOOL, ITEM(res_client.enable_ssl), 0, CFG_ITEM_DEFAULT, "false" },
   { NULL, 0, { 0 }, 0, 0, NULL }
};

/*
 * Storage daemon resource
 *
 * name handler value code flags default_value
 */
static RES_ITEM store_items[] = {
   { "name", CFG_TYPE_NAME, ITEM(res_store.hdr.name), 0, CFG_ITEM_REQUIRED, NULL },
   { "description", CFG_TYPE_STR, ITEM(res_store.hdr.desc), 0, 0, NULL },
   { "sdport", CFG_TYPE_PINT32, ITEM(res_store.SDport), 0, CFG_ITEM_DEFAULT, SD_DEFAULT_PORT },
   { "address", CFG_TYPE_STR, ITEM(res_store.address), 0, CFG_ITEM_REQUIRED, NULL },
   { "sdaddress", CFG_TYPE_STR, ITEM(res_store.address), 0, 0, NULL },
   { "password", CFG_TYPE_MD5PASSWORD, ITEM(res_store.password), 0, CFG_ITEM_REQUIRED, NULL },
   { "sdpassword", CFG_TYPE_MD5PASSWORD, ITEM(res_store.password), 0, 0, NULL },
   { "enablessl", CFG_TYPE_BOOL, ITEM(res_store.enable_ssl), 0, CFG_ITEM_DEFAULT, "false" },
   { NULL, 0, { 0 }, 0, 0, NULL }
};

/*
 * Font resource
 *
 * name handler value code flags default_value
 */
static RES_ITEM con_font_items[] = {
   { "name", CFG_TYPE_NAME, ITEM(con_font.hdr.name), 0, CFG_ITEM_REQUIRED, NULL },
   { "description", CFG_TYPE_STR, ITEM(con_font.hdr.desc), 0, 0, NULL },
   { "font", CFG_TYPE_STR, ITEM(con_font.fontface), 0, 0, NULL },
   { NULL, 0, { 0 }, 0, 0, NULL }
};

/*
 * This is the master resource definition.
 * It must have one item for each of the resources.
 *
 * NOTE!!! keep it in the same order as the R_codes
 *   or eliminate all resources[rindex].name
 *
 *  name items rcode res_head
 */
static RES_TABLE resources[] = {
   { "monitor", mon_items, R_MONITOR, sizeof(MONITORRES) },
   { "director", dir_items, R_DIRECTOR, sizeof(DIRRES) },
   { "client", cli_items, R_CLIENT, sizeof(CLIENTRES) },
   { "storage", store_items, R_STORAGE, sizeof(STORERES) },
   { "consolefont", con_font_items, R_CONSOLE_FONT, sizeof(CONFONTRES) },
   { NULL, NULL, 0, 0 }
};

/*
 * Dump contents of resource
 */
void dump_resource(int type, RES *reshdr, void sendit(void *sock, const char *fmt, ...), void *sock)
{
   URES *res = (URES *)reshdr;
   bool recurse = true;
   char ed1[100], ed2[100];

   if (res == NULL) {
      sendit(sock, _("No %s resource defined\n"), res_to_str(type));
      return;
   }
   if (type < 0) { /* no recursion */
      type = - type;
      recurse = false;
   }
   switch (type) {
   case R_MONITOR:
      sendit(sock, _("Monitor: name=%s FDtimeout=%s SDtimeout=%s\n"),
             reshdr->name,
             edit_uint64(res->res_monitor.FDConnectTimeout, ed1),
             edit_uint64(res->res_monitor.SDConnectTimeout, ed2));
      break;
   case R_DIRECTOR:
      sendit(sock, _("Director: name=%s address=%s FDport=%d\n"),
             res->res_dir.hdr.name, res->res_dir.address, res->res_dir.DIRport);
      break;
   case R_CLIENT:
      sendit(sock, _("Client: name=%s address=%s FDport=%d\n"),
             res->res_client.hdr.name, res->res_client.address, res->res_client.FDport);
      break;
   case R_STORAGE:
      sendit(sock, _("Storage: name=%s address=%s SDport=%d\n"),
             res->res_store.hdr.name, res->res_store.address, res->res_store.SDport);
      break;
   case R_CONSOLE_FONT:
      sendit(sock, _("ConsoleFont: name=%s font face=%s\n"),
             reshdr->name, NPRT(res->con_font.fontface));
      break;
   default:
      sendit(sock, _("Unknown resource type %d in dump_resource.\n"), type);
      break;
   }
   if (recurse && res->res_monitor.hdr.next) {
      dump_resource(type, res->res_monitor.hdr.next, sendit, sock);
   }
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
   RES *nres; /* next resource if linked */
   URES *res = (URES *)sres;

   if (res == NULL)
      return;

   /*
    * Common stuff -- free the resource name and description
    */
   nres = (RES *)res->res_monitor.hdr.next;
   if (res->res_monitor.hdr.name) {
      free(res->res_monitor.hdr.name);
   }
   if (res->res_monitor.hdr.desc) {
      free(res->res_monitor.hdr.desc);
   }

   switch (type) {
   case R_MONITOR:
      break;
   case R_DIRECTOR:
      if (res->res_dir.address) {
         free(res->res_dir.address);
      }
      break;
   case R_CLIENT:
      if (res->res_client.address) {
         free(res->res_client.address);
      }
      if (res->res_client.password.value) {
         free(res->res_client.password.value);
      }
      break;
   case R_STORAGE:
      if (res->res_store.address) {
         free(res->res_store.address);
      }
      if (res->res_store.password.value) {
         free(res->res_store.password.value);
      }
      break;
   case R_CONSOLE_FONT:
      if (res->con_font.fontface) {
         free(res->con_font.fontface);
      }
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
   int rindex = type - R_FIRST;
   int i;
   int error = 0;

   /*
    * Ensure that all required items are present
    */
   for (i = 0; items[i].name; i++) {
      if (items[i].flags & CFG_ITEM_REQUIRED) {
         if (!bit_is_set(i, res_all.res_monitor.hdr.item_present)) {
               Emsg2(M_ERROR_TERM, 0, _("%s item is required in %s resource, but not found.\n"),
                  items[i].name, resources[rindex]);
         }
      }
      /* If this triggers, take a look at lib/parse_conf.h */
      if (i >= MAX_RES_ITEMS) {
         Emsg1(M_ERROR_TERM, 0, _("Too many items in %s resource\n"), resources[rindex]);
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
      case R_MONITOR:
      case R_CLIENT:
      case R_STORAGE:
      case R_DIRECTOR:
      case R_CONSOLE_FONT:
         break;
      default:
         Emsg1(M_ERROR, 0, _("Unknown resource type %d in save_resource.\n"), type);
         error = 1;
         break;
      }
      /*
       * Note, the resource name was already saved during pass 1,
       * so here, we can just release it.
       */
      if (res_all.res_monitor.hdr.name) {
         free(res_all.res_monitor.hdr.name);
         res_all.res_monitor.hdr.name = NULL;
      }
      if (res_all.res_monitor.hdr.desc) {
         free(res_all.res_monitor.hdr.desc);
         res_all.res_monitor.hdr.desc = NULL;
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
         Dmsg3(900, "Inserting first %s res: %s index=%d\n", res_to_str(type),
         res->res_monitor.hdr.name, rindex);
      } else {
         RES *next, *last;
         /*
          * Add new res to end of chain
          */
         for (last = next = res_head[rindex]; next; next=next->next) {
            last = next;
            if (strcmp(next->name, res->res_monitor.hdr.name) == 0) {
               Emsg2(M_ERROR_TERM, 0,
                     _("Attempt to define second %s resource named \"%s\" is not permitted.\n"),
               resources[rindex].name, res->res_monitor.hdr.name);
            }
         }
         last->next = (RES *)res;
         Dmsg4(900, "Inserting %s res: %s index=%d pass=%d\n", res_to_str(type),
         res->res_monitor.hdr.name, rindex, pass);
      }
   }
}

bool parse_tmon_config(CONFIG *config, const char *configfile, int exit_code)
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
