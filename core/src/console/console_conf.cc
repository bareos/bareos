/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2009 Free Software Foundation Europe e.V.
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
// Kern Sibbald, January MM, September MM
#define NEED_JANSSON_NAMESPACE 1
#include "include/bareos.h"
#include "console/console_globals.h"
#include "console/console_conf.h"
#include "lib/alist.h"
#include "lib/resource_item.h"
#include "lib/tls_resource_items.h"
#include "lib/output_formatter.h"
#include "lib/output_formatter_resource.h"
#include "lib/version.h"
#include "include/compiler_macro.h"

#include <cassert>

namespace console {

static bool SaveResource(int type, const ResourceItem* items, int pass);
static void FreeResource(BareosResource* sres, int type);
static void DumpResource(int type,
                         BareosResource* reshdr,
                         ConfigurationParser::sender* sendit,
                         void* sock,
                         bool hide_sensitive_data,
                         bool verbose);

/* the first configuration pass uses this static memory */
static DirectorResource* res_dir;
static ConsoleResource* res_cons;

/* clang-format off */

static const ResourceItem cons_items[] = {
  { "Name", CFG_TYPE_NAME, ITEM(res_cons, resource_name_), {config::Required{}, config::Description{"The name of this resource."}}},
  { "Description", CFG_TYPE_STR, ITEM(res_cons, description_), {}},
  { "RcFile", CFG_TYPE_DIR, ITEM(res_cons, rc_file), {}},
  { "HistoryFile", CFG_TYPE_DIR, ITEM(res_cons, history_file), {}},
  { "HistoryLength", CFG_TYPE_PINT32, ITEM(res_cons, history_length), {config::DefaultValue{"100"}}},
  { "Password", CFG_TYPE_MD5PASSWORD, ITEM(res_cons, password_), {config::Required{}}},
  { "Director", CFG_TYPE_STR, ITEM(res_cons, director), {}},
  { "HeartbeatInterval", CFG_TYPE_TIME, ITEM(res_cons, heartbeat_interval), {config::DefaultValue{"0"}}},
  TLS_COMMON_CONFIG(res_cons),
  TLS_CERT_CONFIG(res_cons),
  {}
};

static const ResourceItem dir_items[] = {
  { "Name", CFG_TYPE_NAME, ITEM(res_dir, resource_name_), {config::Required{}}},
  { "Description", CFG_TYPE_STR, ITEM(res_dir, description_), {}},
  { "Port", CFG_TYPE_PINT32, ITEM(res_dir, DIRport), {config::DefaultValue{DIR_DEFAULT_PORT}, config::Alias{"DirPort"}}},
  { "Address", CFG_TYPE_STR, ITEM(res_dir, address), {config::Required{}, config::Alias{"DirAddress"}}},
  { "Password", CFG_TYPE_MD5PASSWORD, ITEM(res_dir, password_), {}},
  { "HeartbeatInterval", CFG_TYPE_TIME, ITEM(res_dir, heartbeat_interval), {config::DefaultValue{"0"}}},
  TLS_COMMON_CONFIG(res_dir),
  TLS_CERT_CONFIG(res_dir),
  {}
};

static ResourceTable resources[] = {
  { "Console", "Consoles", cons_items, R_CONSOLE, sizeof(ConsoleResource),
      [] (){ res_cons = new ConsoleResource(); }, reinterpret_cast<BareosResource**>(&res_cons) },
  { "Director", "Directors", dir_items, R_DIRECTOR, sizeof(DirectorResource),
      [] (){ res_dir = new DirectorResource(); }, reinterpret_cast<BareosResource**>(&res_dir) },
  {nullptr, nullptr, nullptr, 0, 0, nullptr, nullptr}
};

/* clang-format on */


static void DumpResource(int type,
                         BareosResource* res,
                         ConfigurationParser::sender* sendit,
                         void* sock,
                         bool hide_sensitive_data,
                         bool verbose)
{
  PoolMem buf;
  bool recurse = true;
  OutputFormatter output_formatter
      = OutputFormatter(sendit, sock, nullptr, nullptr);
  OutputFormatterResource output_formatter_resource
      = OutputFormatterResource(&output_formatter);

  if (!res) {
    sendit(sock, T_("Warning: no \"%s\" resource (%d) defined.\n"),
           my_config->ResToStr(type), type);
    return;
  }
  if (type < 0) {  // no recursion
    type = -type;
    recurse = false;
  }

  res->PrintConfig(output_formatter_resource, *my_config, hide_sensitive_data,
                   verbose);

  if (recurse && res->next_) {
    DumpResource(type, res->next_, sendit, sock, hide_sensitive_data, verbose);
  }
}

static void FreeResource(BareosResource* res, int type)
{
  if (!res) return;

  BareosResource* next_resource = (BareosResource*)res->next_;

  if (res->resource_name_) {
    free(res->resource_name_);
    res->resource_name_ = nullptr;
  }
  if (res->description_) {
    free(res->description_);
    res->description_ = nullptr;
  }

  switch (type) {
    case R_CONSOLE: {
      ConsoleResource* p = dynamic_cast<ConsoleResource*>(res);
      assert(p);
      if (p->rc_file) { free(p->rc_file); }
      if (p->history_file) { free(p->history_file); }
      if (p->password_.value) { free(p->password_.value); }
      if (p->director) { free(p->director); }
      delete p;
      break;
    }
    case R_DIRECTOR: {
      DirectorResource* p = dynamic_cast<DirectorResource*>(res);
      assert(p);
      if (p->address) { free(p->address); }
      if (p->password_.value) { free(p->password_.value); }
      delete p;
      break;
    }
    default:
      printf(T_("Unknown resource type %d\n"), type);
      break;
  }
  if (next_resource) { FreeResource(next_resource, type); }
}

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
      case R_CONSOLE: {
        ConsoleResource* p = dynamic_cast<ConsoleResource*>(
            my_config->GetResWithName(R_CONSOLE, res_cons->resource_name_));
        if (!p) {
          Emsg1(M_ABORT, 0, T_("Cannot find Console resource %s\n"),
                res_cons->resource_name_);
        } else {
          p->tls_cert_.allowed_certificate_common_names_ = std::move(
              res_cons->tls_cert_.allowed_certificate_common_names_);
        }
        break;
      }
      case R_DIRECTOR: {
        DirectorResource* p = dynamic_cast<DirectorResource*>(
            my_config->GetResWithName(R_DIRECTOR, res_dir->resource_name_));
        if (!p) {
          Emsg1(M_ABORT, 0, T_("Cannot find Director resource %s\n"),
                res_dir->resource_name_);
        } else {
          p->tls_cert_.allowed_certificate_common_names_
              = std::move(res_dir->tls_cert_.allowed_certificate_common_names_);
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
    switch (resources[type].rcode) {
      case R_DIRECTOR: {
        new_resource = res_dir;
        res_dir = nullptr;
        break;
      }
      case R_CONSOLE: {
        new_resource = res_cons;
        res_cons = nullptr;
        break;
      }
      default:
        Emsg1(M_ERROR_TERM, 0, "Unknown resource type: %d\n",
              resources[type].rcode);
        return false;
    }
    error = my_config->AppendToResourcesChain(new_resource, type) ? 0 : 1;
  }
  return (error == 0);
}

static void ConfigBeforeCallback(ConfigurationParser& t_config)
{
  std::map<int, std::string> map{{R_DIRECTOR, "R_DIRECTOR"},
                                 {R_CONSOLE, "R_CONSOLE"}};
  t_config.InitializeQualifiedResourceNameTypeConverter(map);
}

static void ConfigReadyCallback(ConfigurationParser&) {}

ConfigurationParser* InitConsConfig(const char* configfile, int exit_code)
{
  ConfigurationParser* config = new ConfigurationParser(
      configfile, nullptr, nullptr, nullptr, exit_code, R_NUM, resources,
      default_config_filename.c_str(), "bconsole.d", ConfigBeforeCallback,
      ConfigReadyCallback, SaveResource, DumpResource, FreeResource);
  if (config) { config->r_own_ = R_CONSOLE; }
  return config;
}

#ifdef HAVE_JANSSON
bool PrintConfigSchemaJson(PoolMem& buffer)
{
  json_t* json = json_object();
  json_object_set_new(json, "format-version", json_integer(2));
  json_object_set_new(json, "component", json_string("bconsole"));
  json_object_set_new(json, "version", json_string(kBareosVersionStrings.Full));

  json_t* json_resource_object = json_object();
  json_object_set_new(json, "resource", json_resource_object);
  json_t* bconsole = json_object();
  json_object_set_new(json_resource_object, "bconsole", bconsole);

  const ResourceTable* resource_definition = my_config->resource_definitions_;
  for (; resource_definition->name; ++resource_definition) {
    json_object_set_new(bconsole, resource_definition->name,
                        json_items(resource_definition->items));
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
} /* namespace console */
