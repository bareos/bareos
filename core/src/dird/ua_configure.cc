/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2015-2016 Bareos GmbH & Co. KG

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
 * Written by Marco van Wieringen, January 2015
 */
/**
 * @file
 * Interactive configuration engine for director.
 */

#include "bareos.h"
#include "dird.h"

static void configure_lex_error_handler(const char *file, int line, LEX *lc, PoolMem &msg)
{
   UaContext *ua;

   lc->error_counter++;
   if (lc->caller_ctx) {
      ua = (UaContext *)(lc->caller_ctx);
      ua->error_msg("configure error: %s\n", msg.c_str());
   };
}

static void configure_lex_error_handler(const char *file, int line, LEX *lc, const char *msg, ...)
{
   /*
    * This function is an error handler, used by lex.
    */
   PoolMem buf(PM_NAME);
   va_list ap;

   va_start(ap, msg);
   buf.bvsprintf(msg, ap);
   va_end(ap);

   configure_lex_error_handler(file, line, lc, buf);
}

static inline bool configure_write_resource(const char *filename, const char *resourcetype,
                                            const char *name, const char *content,
                                            const bool overwrite = false)
{
   bool result = false;
   size_t len;
   int fd;
   int flags = O_CREAT|O_WRONLY|O_TRUNC;

   if (!overwrite) {
      flags |= O_EXCL;
   }

   if ((fd = open(filename, flags, 0640)) >= 0) {
      len = strlen(content);
      write(fd, content, len);
      close(fd);
      result = true;
   }

   return result;
}

static inline ResourceItem *config_get_res_item(UaContext *ua, ResourceTable *res_table,
                                            const char *key, const char *value)
{
   ResourceItem *item = NULL;
   const char *errorcharmsg = NULL;

   if (res_table) {
      item = my_config->get_resource_item(res_table->items, key);
      if (!item) {
         ua->error_msg("Resource \"%s\" does not permit directive \"%s\".\n", res_table->name, key);
         return NULL;
      }
   }

   /*
    * Check against dangerous characters ('@', ';').
    * Could be less strict, if this characters are quoted,
    * but it is easier to handle it like this.
    */
   if (strchr(value, '@')) {
      errorcharmsg = "'@' (include)";
   }
   if (strchr(value, ';')) {
      errorcharmsg = "';' (end of directive)";
   }
   if (errorcharmsg) {
      if (ua) {
         ua->error_msg("Could not add directive \"%s\": character %s is forbidden.\n", key, errorcharmsg);
      }
      return NULL;
   }

   return item;
}

static inline bool config_add_directive(UaContext *ua, ResourceTable *res_table, const char *key,
                                        const char *value, PoolMem &resource, int indent = 2)
{
   PoolMem temp(PM_MESSAGE);
   ResourceItem *item = NULL;

   item = config_get_res_item(ua, res_table, key, value);
   if (res_table && (!item)) {
      return false;
   }

   /*
    * Use item->name instead of key for uniform formatting.
    */
   if (item) {
      key = item->name;
   }

   /* TODO: check type, to see if quotes should be used */
   temp.bsprintf("%-*s%s = %s\n", indent, "", key, value);
   resource.strcat(temp);

   return true;
}

static inline bool configure_create_resource_string(UaContext *ua, int first_parameter, ResourceTable *res_table,
                                                    PoolMem &resourcename, PoolMem &resource)
{
   resource.strcat(res_table->name);
   resource.strcat(" {\n");

   /*
    * Is the name of the resource already given as value of the resource type?
    * E.g. configure add client=newclient address=127.0.0.1 ...
    * instead of configure add client name=newclient address=127.0.0.1 ...
    */
   if (ua->argv[first_parameter - 1]) {
      resourcename.strcat(ua->argv[first_parameter - 1]);
      if (!config_add_directive(ua, res_table, "name", resourcename.c_str(), resource)) {
         return false;
      }
   }

   for (int i = first_parameter; i < ua->argc; i++) {
      if (!ua->argv[i]) {
         ua->error_msg("Missing value for directive \"%s\"\n", ua->argk[i]);
         return false;
      }
      if (bstrcasecmp(ua->argk[i], "name")) {
         resourcename.strcat(ua->argv[i]);
      }
      if (!config_add_directive(ua, res_table, ua->argk[i], ua->argv[i], resource)) {
         return false;
      }
   }
   resource.strcat("}\n");

   if (strlen(resourcename.c_str()) <= 0) {
         ua->error_msg("Resource \"%s\": missing name parameter.\n", res_table->name);
         return false;
   }

   return true;
}

static inline bool configure_create_fd_resource_string(UaContext *ua, PoolMem &resource, const char *clientname)
{
   ClientResource *client;
   s_password *password;
   PoolMem temp(PM_MESSAGE);

   client = ua->GetClientResWithName(clientname);
   if (!client) {
      return false;
   }
   password = &client->password;

   resource.strcat("Director {\n");
   config_add_directive(NULL, NULL, "Name", me->name(), resource);

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
static inline bool configure_create_fd_resource(UaContext *ua, const char *clientname)
{
   PoolMem resource(PM_MESSAGE);
   PoolMem filename_tmp(PM_FNAME);
   PoolMem filename(PM_FNAME);
   PoolMem basedir(PM_FNAME);
   PoolMem temp(PM_MESSAGE);
   const char *dirname = NULL;
   const bool error_if_exists = false;
   const bool create_directories = true;
   const bool overwrite = true;

   if (!configure_create_fd_resource_string(ua, resource, clientname)) {
      return false;
   }

   /*
    * Get the path where the resource should get stored.
    */
   basedir.bsprintf("bareos-dir-export/client/%s/bareos-fd.d", clientname);
   dirname = GetNextRes(R_DIRECTOR, NULL)->name;
   if (!my_config->get_path_of_new_resource(filename, temp, basedir.c_str(), "director",
                                            dirname, error_if_exists, create_directories)) {
      ua->error_msg("%s", temp.c_str());
      return false;
   }
   filename_tmp.strcpy(temp);

   /*
    * Write resource to file.
    */
   if (!configure_write_resource(filename.c_str(), "filedaemon-export", clientname, resource.c_str(), overwrite)) {
      ua->error_msg("failed to write filedaemon config resource file\n");
      return false;
   }

   ua->send->object_start("export");
   ua->send->object_key_value("clientname", clientname);
   ua->send->object_key_value("component", "bareos-fd");
   ua->send->object_key_value("resource", "director");
   ua->send->object_key_value("name", dirname);
   ua->send->object_key_value("filename", filename.c_str(), "Exported resource file \"%s\":\n");
   ua->send->object_key_value("content", resource.c_str(), "%s");
   ua->send->object_end("export");

   return true;
}

/**
 * To add a resource during runtime, the following approach is used:
 *
 * - Create a temporary file which contains the new resource.
 * - Use the existing parsing functions to add the new resource to the configured resources.
 *   - on error:
 *     - remove the resource and the temporary file.
 *   - on success:
 *     - move the new temporary resource file to a place, where it will also be loaded on restart
 *       (<CONFIGDIR>/bareos-dir.d/<resourcetype>/<name_of_the_resource>.conf).
 *
 * This way, the existing parsing functionality is used.
 */
static inline bool configure_add_resource(UaContext *ua, int first_parameter, ResourceTable *res_table)
{
   PoolMem resource(PM_MESSAGE);
   PoolMem name(PM_MESSAGE);
   PoolMem filename_tmp(PM_FNAME);
   PoolMem filename(PM_FNAME);
   PoolMem temp(PM_FNAME);
   JobResource *res = NULL;

   if (!configure_create_resource_string(ua, first_parameter, res_table, name, resource)) {
      return false;
   }

   if (GetResWithName(res_table->rcode, name.c_str())) {
      ua->error_msg("Resource \"%s\" with name \"%s\" already exists.\n", res_table->name, name.c_str());
      return false;
   }

   if (!my_config->get_path_of_new_resource(filename, temp, NULL, res_table->name, name.c_str(), true)) {
      ua->error_msg("%s", temp.c_str());
      return false;
   } else {
      filename_tmp.strcpy(temp);
   }

   if (!configure_write_resource(filename_tmp.c_str(), res_table->name, name.c_str(), resource.c_str())) {
      ua->error_msg("failed to write config resource file\n");
      return false;
   }

   if (!my_config->parse_config_file(filename_tmp.c_str(), ua, configure_lex_error_handler, NULL, M_ERROR)) {
      unlink(filename_tmp.c_str());
      my_config->remove_resource(res_table->rcode, name.c_str());
      return false;
   }

   /*
    * parse_config_file has already done some validation.
    * However, it skipped at least some checks for R_JOB
    * (reason: a job can get values from jobdefs,
    * and the value propagation happens after reading the full configuration)
    * therefore we explicitly check the new resource here.
    */
   if ((res_table->rcode == R_JOB) || (res_table->rcode == R_JOBDEFS)) {
      res = (JobResource *)GetResWithName(res_table->rcode, name.c_str());
      propagate_jobdefs(res_table->rcode, res);
      if (!validate_resource(res_table->rcode, res_table->items, (BareosResource *)res)) {
         ua->error_msg("failed to create config resource \"%s\"\n", name.c_str());
         unlink(filename_tmp.c_str());
         my_config->remove_resource(res_table->rcode, name.c_str());
         return false;
      }
   }

   /*
    * new config resource is working fine. Rename file to its permanent name.
    */
   if (rename(filename_tmp.c_str(), filename.c_str()) != 0 ) {
      ua->error_msg("failed to create config file \"%s\"\n", filename.c_str());
      unlink(filename_tmp.c_str());
      my_config->remove_resource(res_table->rcode, name.c_str());
      return false;
   }

   /*
    * When adding a client, also create the client configuration file.
    */
   if (res_table->rcode==R_CLIENT) {
      configure_create_fd_resource(ua, name.c_str());
   }

   ua->send->object_start("add");
   ua->send->object_key_value("resource", res_table->name);
   ua->send->object_key_value("name", name.c_str());
   ua->send->object_key_value("filename", filename.c_str(), "Created resource config file \"%s\":\n");
   ua->send->object_key_value("content", resource.c_str(), "%s");
   ua->send->object_end("add");

   return true;
}

static inline bool configure_add(UaContext *ua, int resource_type_parameter)
{
   bool result = false;
   ResourceTable *res_table = NULL;

   res_table = my_config->get_resource_table(ua->argk[resource_type_parameter]);
   if (!res_table) {
      ua->error_msg(_("invalid resource type %s.\n"), ua->argk[resource_type_parameter]);
      return false;
   }

   if (res_table->rcode == R_DIRECTOR) {
      ua->error_msg(_("Only one Director resource allowed.\n"));
      return false;
   }

   ua->send->object_start("configure");
   result = configure_add_resource(ua, resource_type_parameter+1, res_table);
   ua->send->object_end("configure");

   return result;
}

static inline void configure_export_usage(UaContext *ua)
{
   ua->error_msg(_("usage: configure export client=<clientname>\n"));
}

static inline bool configure_export(UaContext *ua)
{
   bool result = false;
   int i;

   i = find_arg_with_value(ua, NT_("client"));
   if (i < 0) {
      configure_export_usage(ua);
      return false;
   }

   if (!ua->GetClientResWithName(ua->argv[i])) {
      configure_export_usage(ua);
      return false;
   }

   ua->send->object_start("configure");
   result = configure_create_fd_resource(ua, ua->argv[i]);
   ua->send->object_end("configure");

   return result;
}

bool configure_cmd(UaContext *ua, const char *cmd)
{
   bool result = false;

   if (!(my_config->is_using_config_include_dir())) {
      ua->warning_msg(_(
               "It seems that the configuration is not adapted to the include directory structure. "
               "This means, that the configure command may not work as expected. "
               "Your configuration changes may not survive a reload/restart. "
               "Please see %s for details.\n"),
               MANUAL_CONFIG_DIR_URL);
   }

   if (ua->argc < 3) {
      ua->error_msg(_("usage:\n"
                      "  configure add <resourcetype> <key1>=<value1> ...\n"
                      "  configure export client=<clientname>\n"));
      return false;
   }

   if (bstrcasecmp(ua->argk[1], NT_("add"))) {
      result = configure_add(ua, 2);
   } else if (bstrcasecmp(ua->argk[1], NT_("export"))) {
      result = configure_export(ua);
   } else {
      ua->error_msg(_("invalid subcommand %s.\n"), ua->argk[1]);
      return false;
   }

   return result;
}
