/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2009 Free Software Foundation Europe e.V.
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

#define NEED_JANSSON_NAMESPACE 1
#include "bareos.h"
#include "console_conf.h"

/*
 * Define the first and last resource ID record
 * types. Note, these should be unique for each
 * daemon though not a requirement.
 */
static RES *sres_head[R_LAST - R_FIRST + 1];
static RES **res_head = sres_head;

/* Forward referenced subroutines */


/* We build the current resource here as we are
 * scanning the resource configuration definition,
 * then move it to allocated memory when the resource
 * scan is complete.
 */
static URES res_all;
static int32_t res_all_size = sizeof(res_all);

/* Definition of records permitted within each
 * resource with the routine to process the record
 * information.
 */

/*  Console "globals" */
static RES_ITEM cons_items[] = {
   { "Name", CFG_TYPE_NAME, ITEM(res_cons.hdr.name), 0, CFG_ITEM_REQUIRED, NULL, NULL, "The name of this resource." },
   { "Description", CFG_TYPE_STR, ITEM(res_cons.hdr.desc), 0, 0, NULL, NULL, NULL },
   { "RcFile", CFG_TYPE_DIR, ITEM(res_cons.rc_file), 0, 0, NULL, NULL, NULL },
   { "HistoryFile", CFG_TYPE_DIR, ITEM(res_cons.history_file), 0, 0, NULL, NULL, NULL },
   { "HistoryLength", CFG_TYPE_PINT32, ITEM(res_cons.history_length), 0, CFG_ITEM_DEFAULT, "100", NULL, NULL },
   { "Password", CFG_TYPE_MD5PASSWORD, ITEM(res_cons.password), 0, CFG_ITEM_REQUIRED, NULL, NULL, NULL },
   { "Director", CFG_TYPE_STR, ITEM(res_cons.director), 0, 0, NULL, NULL, NULL },
   { "HeartbeatInterval", CFG_TYPE_TIME, ITEM(res_cons.heartbeat_interval), 0, CFG_ITEM_DEFAULT, "0", NULL, NULL },
   TLS_CONFIG(res_cons)
   { NULL, 0, { 0 }, 0, 0, NULL, NULL, NULL }
};

/*  Director's that we can contact */
static RES_ITEM dir_items[] = {
   { "Name", CFG_TYPE_NAME, ITEM(res_dir.hdr.name), 0, CFG_ITEM_REQUIRED, NULL, NULL, NULL },
   { "Description", CFG_TYPE_STR, ITEM(res_dir.hdr.desc), 0, 0, NULL, NULL, NULL },
   { "DirPort", CFG_TYPE_PINT32, ITEM(res_dir.DIRport), 0, CFG_ITEM_DEFAULT, DIR_DEFAULT_PORT, NULL, NULL },
   { "Address", CFG_TYPE_STR, ITEM(res_dir.address), 0, 0, NULL, NULL, NULL },
   { "Password", CFG_TYPE_MD5PASSWORD, ITEM(res_dir.password), 0, CFG_ITEM_REQUIRED, NULL, NULL, NULL },
   { "HeartbeatInterval", CFG_TYPE_TIME, ITEM(res_dir.heartbeat_interval), 0, CFG_ITEM_DEFAULT, "0", NULL, NULL },
   TLS_CONFIG(res_dir)
   { NULL, 0, { 0 }, 0, 0, NULL, NULL, NULL }
};

/*
 * This is the master resource definition.
 * It must have one item for each of the resources.
 */
static RES_TABLE resources[] = {
   { "Console", cons_items, R_CONSOLE, sizeof(CONRES) },
   { "Director", dir_items, R_DIRECTOR, sizeof(DIRRES) },
   { NULL, NULL, 0 }
};

/*
 * Dump contents of resource
 */
void dump_resource(int type, RES *reshdr,
                   void sendit(void *sock, const char *fmt, ...),
                   void *sock, bool hide_sensitive_data)
{
   POOL_MEM buf;
   URES *res = (URES *)reshdr;
   BRSRES *resclass;
   bool recurse = true;

   if (res == NULL) {
      sendit(sock, _("Warning: no \"%s\" resource (%d) defined.\n"), res_to_str(type), type);
      return;
   }
   if (type < 0) {                    /* no recursion */
      type = - type;
      recurse = false;
   }

   switch (type) {
      default:
         resclass = (BRSRES *)reshdr;
         resclass->print_config(buf);
         break;
   }
   sendit(sock, "%s", buf.c_str());

   if (recurse && res->res_dir.hdr.next) {
      dump_resource(type, res->res_dir.hdr.next, sendit, sock, hide_sensitive_data);
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
      if (res->res_cons.history_file) {
         free(res->res_cons.history_file);
      }
      free_tls_t(res->res_cons.tls);
      break;
   case R_DIRECTOR:
      if (res->res_dir.address) {
         free(res->res_dir.address);
      }
      free_tls_t(res->res_dir.tls);
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
         case R_CONSOLE:
            if ((res = (URES *)GetResWithName(R_CONSOLE, res_all.res_cons.name())) == NULL) {
               Emsg1(M_ABORT, 0, _("Cannot find Console resource %s\n"), res_all.res_cons.name());
            } else {
               res->res_cons.tls.allowed_cns = res_all.res_cons.tls.allowed_cns;
            }
            break;
         case R_DIRECTOR:
            if ((res = (URES *)GetResWithName(R_DIRECTOR, res_all.res_dir.name())) == NULL) {
               Emsg1(M_ABORT, 0, _("Cannot find Director resource %s\n"), res_all.res_dir.name());
            } else {
               res->res_dir.tls.allowed_cns = res_all.res_dir.tls.allowed_cns;
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
      res = (URES *)malloc(resources[rindex].size);
      memcpy(res, &res_all, resources[rindex].size);
      if (!res_head[rindex]) {
         res_head[rindex] = (RES *)res; /* store first entry */
      } else {
         RES *next, *last;
         for (last=next=res_head[rindex]; next; next=next->next) {
            last = next;
            if (bstrcmp(next->name, res->res_dir.name())) {
               Emsg2(M_ERROR_TERM, 0,
                     _("Attempt to define second %s resource named \"%s\" is not permitted.\n"),
                     resources[rindex].name, res->res_dir.name());
            }
         }
         last->next = (RES *)res;
         Dmsg2(90, "Inserting %s res: %s\n", res_to_str(type), res->res_dir.name());
      }
   }
   return (error == 0);
}

void init_cons_config(CONFIG *config, const char *configfile, int exit_code)
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
   config->set_default_config_filename(CONFIG_FILE);
   config->set_config_include_dir("bconsole.d");
}

bool parse_cons_config(CONFIG *config, const char *configfile, int exit_code)
{
   init_cons_config(config, configfile, exit_code);
   return config->parse_config();
}

/*
 * Print configuration file schema in json format
 */
#ifdef HAVE_JANSSON
bool print_config_schema_json(POOL_MEM &buffer)
{
   RES_TABLE *resources = my_config->m_resources;

   initialize_json();

   json_t *json = json_object();
   json_object_set_new(json, "format-version", json_integer(2));
   json_object_set_new(json, "component", json_string("bconsole"));
   json_object_set_new(json, "version", json_string(VERSION));

   /*
    * Resources
    */
   json_t *resource = json_object();
   json_object_set(json, "resource", resource);
   json_t *bconsole = json_object();
   json_object_set(resource, "bconsole", bconsole);

   for (int r = 0; resources[r].name; r++) {
      RES_TABLE resource = my_config->m_resources[r];
      json_object_set(bconsole, resource.name, json_items(resource.items));
   }

   pm_strcat(buffer, json_dumps(json, JSON_INDENT(2)));
   json_decref(json);

   return true;
}
#else
bool print_config_schema_json(POOL_MEM &buffer)
{
   pm_strcat(buffer, "{ \"success\": false, \"message\": \"not available\" }");
   return false;
}
#endif
