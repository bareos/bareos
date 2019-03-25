/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2009 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2018 Bareos GmbH & Co. KG

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
 * Kern Sibbald, January MM, September MM
 */
/**
 * @file
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
 */

#define NEED_JANSSON_NAMESPACE 1
#include "include/bareos.h"
#include "console/console_globals.h"
#include "console/console_conf.h"
#include "lib/alist.h"
#include "lib/json.h"
#include "lib/resource_item.h"
#include "lib/output_formatter.h"

namespace console {

/**
 * Define the first and last resource ID record
 * types. Note, these should be unique for each
 * daemon though not a requirement.
 */
static BareosResource* sres_head[R_LAST - R_FIRST + 1];
static BareosResource** res_head = sres_head;

/* Forward referenced subroutines */
static bool SaveResource(int type, ResourceItem* items, int pass);
static void FreeResource(BareosResource* sres, int type);
static void DumpResource(int type,
                         BareosResource* reshdr,
                         void sendit(void* sock, const char* fmt, ...),
                         void* sock,
                         bool hide_sensitive_data,
                         bool verbose);

/* We build the current resource here as we are
 * scanning the resource configuration definition,
 * then move it to allocated memory when the resource
 * scan is complete.
 */
static UnionOfResources res_all;
static int32_t res_all_size = sizeof(res_all);

/* Definition of records permitted within each
 * resource with the routine to process the record
 * information.
 */

/* clang-format off */

static ResourceItem cons_items[] = {
  { "Name", CFG_TYPE_NAME, ITEM(res_cons.resource_name_), 0, CFG_ITEM_REQUIRED, NULL, NULL, "The name of this resource." },
  { "Description", CFG_TYPE_STR, ITEM(res_cons.description_), 0, 0, NULL, NULL, NULL },
  { "RcFile", CFG_TYPE_DIR, ITEM(res_cons.rc_file), 0, 0, NULL, NULL, NULL },
  { "HistoryFile", CFG_TYPE_DIR, ITEM(res_cons.history_file), 0, 0, NULL, NULL, NULL },
  { "HistoryLength", CFG_TYPE_PINT32, ITEM(res_cons.history_length), 0, CFG_ITEM_DEFAULT, "100", NULL, NULL },
  { "Password", CFG_TYPE_MD5PASSWORD, ITEM(res_cons.password), 0, CFG_ITEM_REQUIRED, NULL, NULL, NULL },
  { "Director", CFG_TYPE_STR, ITEM(res_cons.director), 0, 0, NULL, NULL, NULL },
  { "HeartbeatInterval", CFG_TYPE_TIME, ITEM(res_cons.heartbeat_interval), 0, CFG_ITEM_DEFAULT, "0", NULL, NULL },
  TLS_COMMON_CONFIG(res_dir),
  TLS_CERT_CONFIG(res_dir),
  { NULL, 0, { 0 }, 0, 0, NULL, NULL, NULL }
};

static ResourceItem dir_items[] = {
  { "Name", CFG_TYPE_NAME, ITEM(res_dir.resource_name_), 0, CFG_ITEM_REQUIRED, NULL, NULL, NULL },
  { "Description", CFG_TYPE_STR, ITEM(res_dir.description_), 0, 0, NULL, NULL, NULL },
  { "DirPort", CFG_TYPE_PINT32, ITEM(res_dir.DIRport), 0, CFG_ITEM_DEFAULT, DIR_DEFAULT_PORT, NULL, NULL },
  { "Address", CFG_TYPE_STR, ITEM(res_dir.address), 0, 0, NULL, NULL, NULL },
  { "Password", CFG_TYPE_MD5PASSWORD, ITEM(res_dir.password_), 0, CFG_ITEM_REQUIRED, NULL, NULL, NULL },
  { "HeartbeatInterval", CFG_TYPE_TIME, ITEM(res_dir.heartbeat_interval), 0, CFG_ITEM_DEFAULT, "0", NULL, NULL },
  TLS_COMMON_CONFIG(res_dir),
  TLS_CERT_CONFIG(res_dir),
  { NULL, 0, { 0 }, 0, 0, NULL, NULL, NULL }
};

static ResourceTable resources[] = {
  { "Console", cons_items, R_CONSOLE, sizeof(ConsoleResource), [] (void *res){ return new((ConsoleResource *) res) ConsoleResource(); } },
  { "Director", dir_items, R_DIRECTOR, sizeof(DirectorResource), [] (void *res){ return new((DirectorResource *) res) DirectorResource(); } },
  { NULL, NULL, 0 }
};

/* clang-format on */


static void DumpResource(int type,
                         BareosResource* reshdr,
                         void sendit(void* sock, const char* fmt, ...),
                         void* sock,
                         bool hide_sensitive_data,
                         bool verbose)
{
  PoolMem buf;
  UnionOfResources* res = (UnionOfResources*)reshdr;
  BareosResource* resclass;
  bool recurse = true;

  if (res == NULL) {
    sendit(sock, _("Warning: no \"%s\" resource (%d) defined.\n"),
           my_config->ResToStr(type), type);
    return;
  }
  if (type < 0) { /* no recursion */
    type = -type;
    recurse = false;
  }

  switch (type) {
    default:
      resclass = (BareosResource*)reshdr;
      resclass->PrintConfig(buf, *my_config);
      break;
  }
  sendit(sock, "%s", buf.c_str());

  if (recurse && res->res_dir.next_) {
    my_config->DumpResourceCb_(type, res->res_dir.next_, sendit, sock,
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
static void FreeResource(BareosResource* sres, int type)
{
  BareosResource* nres;
  UnionOfResources* res = (UnionOfResources*)sres;

  if (res == NULL) return;

  /* common stuff -- free the resource name */
  nres = (BareosResource*)res->res_dir.next_;
  if (res->res_dir.resource_name_) { free(res->res_dir.resource_name_); }
  if (res->res_dir.description_) { free(res->res_dir.description_); }

  switch (type) {
    case R_CONSOLE:
      if (res->res_cons.rc_file) { free(res->res_cons.rc_file); }
      if (res->res_cons.history_file) { free(res->res_cons.history_file); }
      if (res->res_cons.tls_cert_.allowed_certificate_common_names_) {
        res->res_cons.tls_cert_.allowed_certificate_common_names_->destroy();
        free(res->res_cons.tls_cert_.allowed_certificate_common_names_);
      }
      if (res->res_cons.tls_cert_.ca_certfile_) {
        delete res->res_cons.tls_cert_.ca_certfile_;
      }
      if (res->res_cons.tls_cert_.ca_certdir_) {
        delete res->res_cons.tls_cert_.ca_certdir_;
      }
      if (res->res_cons.tls_cert_.crlfile_) {
        delete res->res_cons.tls_cert_.crlfile_;
      }
      if (res->res_cons.tls_cert_.certfile_) {
        delete res->res_cons.tls_cert_.certfile_;
      }
      if (res->res_cons.tls_cert_.keyfile_) {
        delete res->res_cons.tls_cert_.keyfile_;
      }
      if (res->res_cons.cipherlist_) { delete res->res_cons.cipherlist_; }
      if (res->res_cons.tls_cert_.dhfile_) {
        delete res->res_cons.tls_cert_.dhfile_;
      }
      if (res->res_cons.tls_cert_.pem_message_) {
        delete res->res_cons.tls_cert_.pem_message_;
      }
      break;
    case R_DIRECTOR:
      if (res->res_dir.address) { free(res->res_dir.address); }
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
    default:
      printf(_("Unknown resource type %d\n"), type);
  }
  /* Common stuff again -- free the resource, recurse to next one */
  free(res);
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
      if (!BitIsSet(i, res_all.res_dir.item_present_)) {
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
      case R_CONSOLE:
        if ((res = (UnionOfResources*)my_config->GetResWithName(
                 R_CONSOLE, res_all.res_cons.resource_name_)) == NULL) {
          Emsg1(M_ABORT, 0, _("Cannot find Console resource %s\n"),
                res_all.res_cons.resource_name_);
        } else {
          res->res_cons.tls_cert_.allowed_certificate_common_names_ =
              res_all.res_cons.tls_cert_.allowed_certificate_common_names_;
        }
        break;
      case R_DIRECTOR:
        if ((res = (UnionOfResources*)my_config->GetResWithName(
                 R_DIRECTOR, res_all.res_dir.resource_name_)) == NULL) {
          Emsg1(M_ABORT, 0, _("Cannot find Director resource %s\n"),
                res_all.res_dir.resource_name_);
        } else {
          res->res_dir.tls_cert_.allowed_certificate_common_names_ =
              res_all.res_dir.tls_cert_.allowed_certificate_common_names_;
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
    if (res_all.res_dir.resource_name_) {
      free(res_all.res_dir.resource_name_);
      res_all.res_dir.resource_name_ = NULL;
    }
    if (res_all.res_dir.description_) {
      free(res_all.res_dir.description_);
      res_all.res_dir.description_ = NULL;
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
      res_head[rindex] = (BareosResource*)res; /* store first entry */
    } else {
      BareosResource *next, *last;
      for (last = next = res_head[rindex]; next; next = next->next_) {
        last = next;
        if (bstrcmp(next->resource_name_, res->res_dir.resource_name_)) {
          Emsg2(M_ERROR_TERM, 0,
                _("Attempt to define second %s resource named \"%s\" is not "
                  "permitted.\n"),
                resources[rindex].name, res->res_dir.resource_name_);
        }
      }
      last->next_ = (BareosResource*)res;
      Dmsg2(90, "Inserting %s res: %s\n", my_config->ResToStr(type),
            res->res_dir.resource_name_);
    }
  }
  return (error == 0);
}

static void ConfigReadyCallback(ConfigurationParser& my_config)
{
  std::map<int, std::string> map{{R_DIRECTOR, "R_DIRECTOR"},
                                 {R_CONSOLE, "R_CONSOLE"}};
  my_config.InitializeQualifiedResourceNameTypeConverter(map);
}

ConfigurationParser* InitConsConfig(const char* configfile, int exit_code)
{
  ConfigurationParser* config = new ConfigurationParser(
      configfile, nullptr, nullptr, nullptr, nullptr, nullptr, exit_code,
      (void*)&res_all, res_all_size, R_FIRST, R_LAST, resources, res_head,
      default_config_filename.c_str(), "bconsole.d", ConfigReadyCallback,
      SaveResource, DumpResource, FreeResource);
  if (config) { config->r_own_ = R_CONSOLE; }
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
  json_object_set_new(json, "component", json_string("bconsole"));
  json_object_set_new(json, "version", json_string(VERSION));

  /*
   * Resources
   */
  json_t* resource = json_object();
  json_object_set(json, "resource", resource);
  json_t* bconsole = json_object();
  json_object_set(resource, "bconsole", bconsole);

  for (int r = 0; resources[r].name; r++) {
    ResourceTable resource = my_config->resources_[r];
    json_object_set(bconsole, resource.name, json_items(resource.items));
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
} /* namespace console */
