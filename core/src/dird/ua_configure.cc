/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2015-2026 Bareos GmbH & Co. KG

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
// Written by Marco van Wieringen, January 2015
/**
 * @file
 * Interactive configuration engine for director.
 */

#include "include/fcntl_def.h"
#include "include/bareos.h"
#include "dird.h"
#include "dird/dird_globals.h"
#include "dird/ua_select.h"
#include "lib/parse_conf.h"
#include "lib/util.h"
#include <array>
#include <algorithm>

namespace directordaemon {

static void ConfigureLexErrorHandler(const char*, int, lexer* lc, PoolMem& msg)
{
  UaContext* ua;

  lc->error_counter++;
  if (lc->caller_ctx) {
    ua = (UaContext*)(lc->caller_ctx);
    ua->ErrorMsg("configure error: %s\n", msg.c_str());
  }
}

static void ConfigureLexErrorHandler(const char* file,
                                     int line,
                                     lexer* lc,
                                     const char* msg,
                                     ...)
{
  // This function is an error handler, used by lex.
  PoolMem buf(PM_NAME);
  va_list ap;

  va_start(ap, msg);
  buf.Bvsprintf(msg, ap);
  va_end(ap);

  ConfigureLexErrorHandler(file, line, lc, buf);
}

static inline bool configure_write_resource(const char* filename,
                                            const char*,
                                            const char*,
                                            const char* content,
                                            const bool overwrite = false)
{
  bool result = false;
  size_t len;
  int fd;
  int flags = O_CREAT | O_WRONLY | O_TRUNC;

  if (!overwrite) { flags |= O_EXCL; }

  if ((fd = open(filename, flags, 0640)) >= 0) {
    len = strlen(content);
    if (write(fd, content, len) < 0) {
      result = false;
    } else {
      result = true;
    }
    close(fd);
  }

  return result;
}

static inline const ResourceItem* config_get_res_item(
    UaContext* ua,
    const ResourceTable* res_table,
    const char* key,
    const char* value)
{
  const ResourceItem* item = NULL;
  const char* errorcharmsg = NULL;

  if (res_table) {
    item = my_config->GetResourceItem(res_table->items, key);
    if (!item) {
      ua->ErrorMsg("Resource \"%s\" does not permit directive \"%s\".\n",
                   res_table->name, key);
      return NULL;
    }
  }

  /* Check against dangerous characters ('@', ';').
   * Could be less strict, if this characters are quoted,
   * but it is easier to handle it like this. */
  if (strchr(value, '@')) { errorcharmsg = "'@' (include)"; }
  if (strchr(value, ';')) { errorcharmsg = "';' (end of directive)"; }
  if (errorcharmsg) {
    if (ua) {
      ua->ErrorMsg(
          "Could not add directive \"%s\": character %s is forbidden.\n", key,
          errorcharmsg);
    }
    return NULL;
  }

  return item;
}

static inline bool config_add_directive(UaContext* ua,
                                        const ResourceTable* res_table,
                                        const char* key,
                                        const char* value,
                                        PoolMem& resource,
                                        int indent = 2)
{
  PoolMem temp(PM_MESSAGE);
  const ResourceItem* item = NULL;
  std::string format("%-*s%s = %s\n");

  const std::array<int, 15> quotable_types{
      CFG_TYPE_AUTOPASSWORD,   CFG_TYPE_CLEARPASSWORD, CFG_TYPE_DIR,
      CFG_TYPE_DIR_OR_CMD,     CFG_TYPE_LABEL,         CFG_TYPE_MD5PASSWORD,
      CFG_TYPE_NAME,           CFG_TYPE_RES,           CFG_TYPE_RUNSCRIPT_CMD,
      CFG_TYPE_RUNSCRIPT_WHEN, CFG_TYPE_SHRTRUNSCRIPT, CFG_TYPE_STDSTR,
      CFG_TYPE_STDSTRDIR,      CFG_TYPE_STR,           CFG_TYPE_STRNAME};

  const std::array<int, 3> script_types{
      CFG_TYPE_RUNSCRIPT_CMD, CFG_TYPE_DIR_OR_CMD, CFG_TYPE_SHRTRUNSCRIPT};

  std::string temp_value(value);

  item = config_get_res_item(ua, res_table, key, value);

  if (res_table && (!item)) { return false; }

  if (item) {
    // Use item->name instead of key for uniform formatting.
    key = item->name;
    if (std::find(quotable_types.begin(), quotable_types.end(), item->type)
        != quotable_types.end()) {
      format = "%-*s%s = \"%s\"\n";

      if (std::find(script_types.begin(), script_types.end(), item->type)
          != script_types.end()) {
        temp_value = EscapeString(value);
      }
    }
  }

  temp.bsprintf(format.c_str(), indent, "", key, temp_value.c_str());

  resource.strcat(temp);

  return true;
}

static inline bool configure_create_resource_string(
    UaContext* ua,
    int first_parameter,
    const ResourceTable* res_table,
    PoolMem& resourcename,
    PoolMem& resource)
{
  resource.strcat(res_table->name);
  resource.strcat(" {\n");

  /* Is the name of the resource already given as value of the resource type?
   * E.g. configure add client=newclient address=127.0.0.1 ...
   * instead of configure add client name=newclient address=127.0.0.1 ... */
  if (ua->argv[first_parameter - 1]) {
    resourcename.strcat(ua->argv[first_parameter - 1]);
    if (!config_add_directive(ua, res_table, "name", resourcename.c_str(),
                              resource)) {
      return false;
    }
  }

  for (int i = first_parameter; i < ua->argc; i++) {
    if (!ua->argv[i]) {
      ua->ErrorMsg("Missing value for directive \"%s\"\n", ua->argk[i]);
      return false;
    }
    if (Bstrcasecmp(ua->argk[i], "name")) { resourcename.strcat(ua->argv[i]); }
    if (!config_add_directive(ua, res_table, ua->argk[i], ua->argv[i],
                              resource)) {
      return false;
    }
  }
  resource.strcat("}\n");

  if (strlen(resourcename.c_str()) <= 0) {
    ua->ErrorMsg("Resource \"%s\": missing name parameter.\n", res_table->name);
    return false;
  }

  return true;
}

static inline bool ConfigureCreateFdResourceString(UaContext* ua,
                                                   PoolMem& resource,
                                                   const char* clientname)
{
  ClientResource* client;
  s_password* password;
  PoolMem temp(PM_MESSAGE);

  client = ua->GetClientResWithName(clientname);
  if (!client) { return false; }
  password = &client->password_;

  resource.strcat("Director {\n");

  Mmsg(temp, "\"%s\"", me->resource_name_);
  config_add_directive(NULL, NULL, "Name", temp.c_str(), resource);

  switch (password->encoding) {
    case p_encoding_clear:
      Mmsg(temp, "\"%s\"", password->value);
      break;
    case p_encoding_md5:
      Mmsg(temp, "\"[md5]%s\"", password->value);
      break;
    default:
      break;
  }
  config_add_directive(NULL, NULL, "Password", temp.c_str(), resource);

  resource.strcat("}\n");

  return true;
}

/**
 * Create a bareos-fd director resource file
 * that corresponds to our client definition.
 */
static inline bool ConfigureCreateFdResource(UaContext* ua,
                                             const char* clientname)
{
  PoolMem resource(PM_MESSAGE);
  PoolMem filename_tmp(PM_FNAME);
  PoolMem filename(PM_FNAME);
  PoolMem basedir(PM_FNAME);
  PoolMem temp(PM_MESSAGE);
  const char* dirname = NULL;
  const bool error_if_exists = false;
  const bool create_directories = true;
  const bool overwrite = true;

  if (!ConfigureCreateFdResourceString(ua, resource, clientname)) {
    return false;
  }

  // Get the path where the resource should get stored.
  basedir.bsprintf("bareos-dir-export/client/%s/bareos-fd.d", clientname);
  dirname = my_config->GetNextRes(R_DIRECTOR, NULL)->resource_name_;
  if (!my_config->GetPathOfNewResource(filename, temp, basedir.c_str(),
                                       "director", dirname, error_if_exists,
                                       create_directories)) {
    ua->ErrorMsg("%s", temp.c_str());
    return false;
  }
  filename_tmp.strcpy(temp);

  // Write resource to file.
  if (!configure_write_resource(filename.c_str(), "filedaemon-export",
                                clientname, resource.c_str(), overwrite)) {
    ua->ErrorMsg("failed to write filedaemon config resource file\n");
    return false;
  }

  ua->send->ObjectStart("export");
  ua->send->ObjectKeyValue("clientname", clientname);
  ua->send->ObjectKeyValue("component", "bareos-fd");
  ua->send->ObjectKeyValue("resource", "director");
  ua->send->ObjectKeyValue("name", dirname);
  ua->send->ObjectKeyValue("filename", filename.c_str(),
                           "Exported resource file \"%s\":\n");
  ua->send->ObjectKeyValue("content", resource.c_str(), "%s");
  ua->send->ObjectEnd("export");

  return true;
}

/**
 * To add a resource during runtime, the following approach is used:
 *
 * - Create a temporary file which contains the new resource.
 * - Use the existing parsing functions to add the new resource to the
 * configured resources.
 *   - on error:
 *     - remove the resource and the temporary file.
 *   - on success:
 *     - move the new temporary resource file to a place, where it will also be
 * loaded on restart
 *       (<CONFIGDIR>/bareos-dir.d/<resourcetype>/<name_of_the_resource>.conf).
 *
 * This way, the existing parsing functionality is used.
 */
static inline bool ConfigureAddResource(UaContext* ua,
                                        int first_parameter,
                                        const ResourceTable* res_table)
{
  PoolMem resource(PM_MESSAGE);
  PoolMem name(PM_MESSAGE);
  PoolMem filename_tmp(PM_FNAME);
  PoolMem filename(PM_FNAME);
  PoolMem temp(PM_FNAME);
  JobResource* res = NULL;

  if (!configure_create_resource_string(ua, first_parameter, res_table, name,
                                        resource)) {
    return false;
  }

  if (my_config->GetResWithName(res_table->rcode, name.c_str())) {
    ua->ErrorMsg("Resource \"%s\" with name \"%s\" already exists.\n",
                 res_table->name, name.c_str());
    return false;
  }

  if (!my_config->GetPathOfNewResource(filename, temp, NULL, res_table->name,
                                       name.c_str(), true)) {
    ua->ErrorMsg("%s", temp.c_str());
    return false;
  } else {
    filename_tmp.strcpy(temp);
  }

  if (!configure_write_resource(filename_tmp.c_str(), res_table->name,
                                name.c_str(), resource.c_str())) {
    ua->ErrorMsg("failed to write config resource file\n");
    return false;
  }

  my_config->err_type_ = M_ERROR;
  if (!my_config->ParseConfigFile(filename_tmp.c_str(), ua,
                                  ConfigureLexErrorHandler)) {
    unlink(filename_tmp.c_str());
    my_config->RemoveResource(res_table->rcode, name.c_str());
    return false;
  }

  /* ParseConfigFile has already done some validation.
   * However, it skipped at least some checks for R_JOB
   * (reason: a job can get values from jobdefs,
   * and the value propagation happens after reading the full configuration)
   * therefore we explicitly check the new resource here. */
  if ((res_table->rcode == R_JOB) || (res_table->rcode == R_JOBDEFS)) {
    res = (JobResource*)my_config->GetResWithName(res_table->rcode,
                                                  name.c_str());
    PropagateJobdefs(res_table->rcode, res);
    if (!ValidateResource(res_table->rcode, res_table->items,
                          (BareosResource*)res)) {
      ua->ErrorMsg("failed to create config resource \"%s\"\n", name.c_str());
      unlink(filename_tmp.c_str());
      my_config->RemoveResource(res_table->rcode, name.c_str());
      return false;
    }
  }

  // new config resource is working fine. Rename file to its permanent name.
  if (rename(filename_tmp.c_str(), filename.c_str()) != 0) {
    ua->ErrorMsg("failed to create config file \"%s\"\n", filename.c_str());
    unlink(filename_tmp.c_str());
    my_config->RemoveResource(res_table->rcode, name.c_str());
    return false;
  }

  // When adding a client, also create the File Daemon client resource file.
  if (res_table->rcode == R_CLIENT) {
    ConfigureCreateFdResource(ua, name.c_str());
  }

  ua->send->ObjectStart("add");
  ua->send->ObjectKeyValue("resource", res_table->name);
  ua->send->ObjectKeyValue("name", name.c_str());
  ua->send->ObjectKeyValue("filename", filename.c_str(),
                           "Created resource config file \"%s\":\n");
  ua->send->ObjectKeyValue("content", resource.c_str(), "%s");
  ua->send->ObjectEnd("add");

  return true;
}

static inline bool ConfigureAdd(UaContext* ua, int resource_type_parameter)
{
  bool result = false;
  const ResourceTable* res_table
      = my_config->GetResourceTable(ua->argk[resource_type_parameter]);
  if (!res_table) {
    ua->ErrorMsg(T_("invalid resource type %s.\n"),
                 ua->argk[resource_type_parameter]);
    return false;
  }

  if (res_table->rcode == R_DIRECTOR) {
    ua->ErrorMsg(T_("Only one Director resource allowed.\n"));
    return false;
  }

  ua->send->ObjectStart("configure");
  result = ConfigureAddResource(ua, resource_type_parameter + 1, res_table);
  ua->send->ObjectEnd("configure");

  return result;
}

static inline void ConfigureExportUsage(UaContext* ua)
{
  ua->ErrorMsg(T_("usage: configure export client=<clientname>\n"));
}

static inline bool ConfigureExport(UaContext* ua)
{
  bool result = false;
  int i;

  i = FindArgWithValue(ua, NT_("client"));
  if (i < 0) {
    ConfigureExportUsage(ua);
    return false;
  }

  if (!ua->GetClientResWithName(ua->argv[i])) {
    ConfigureExportUsage(ua);
    return false;
  }

  ua->send->ObjectStart("configure");
  result = ConfigureCreateFdResource(ua, ua->argv[i]);
  ua->send->ObjectEnd("configure");

  return result;
}

bool ConfigureCmd(UaContext* ua, const char*)
{
  bool result = false;

  if (!(my_config->IsUsingConfigIncludeDir())) {
    ua->WarningMsg(
        T_("It seems that the configuration is not adapted to the include "
           "directory structure. "
           "This means, that the configure command may not work as expected. "
           "Your configuration changes may not survive a reload/restart. "));
  }

  if (ua->argc < 3) {
    ua->ErrorMsg(
        T_("usage:\n"
           "  configure add <resourcetype> <key1>=<value1> ...\n"
           "  configure export client=<clientname>\n"));
    return false;
  }

  if (Bstrcasecmp(ua->argk[1], NT_("add"))) {
    result = ConfigureAdd(ua, 2);
  } else if (Bstrcasecmp(ua->argk[1], NT_("export"))) {
    result = ConfigureExport(ua);
  } else {
    ua->ErrorMsg(T_("invalid subcommand %s.\n"), ua->argk[1]);
    return false;
  }

  return result;
}
} /* namespace directordaemon */
