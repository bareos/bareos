/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.
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
 * Master Configuration routines.
 *
 * This file contains the common parts of the BAREOS configuration routines.
 *
 * Note, the configuration file parser consists of four parts
 *
 * 1. The generic lexical scanner in lib/lex.c and lib/lex.h
 *
 * 2. The generic config scanner in lib/parse_conf.c and lib/parse_conf.h.
 *    These files contain the parser code, some utility routines,
 *
 * 3. The generic resource functions in lib/res.c
 *    Which form the common store routines (name, int, string, time,
 *    int64, size, ...).
 *
 * 4. The daemon specific file, which contains the Resource definitions
 *    as well as any specific store routines for the resource records.
 *
 * N.B. This is a two pass parser, so if you malloc() a string in a "store" routine,
 * you must ensure to do it during only one of the two passes, or to free it between.
 *
 * Also, note that the resource record is malloced and saved in save_resource()
 * during pass 1. Anything that you want saved after pass two (e.g. resource pointers)
 * must explicitly be done in save_resource. Take a look at the Job resource in
 * src/dird/dird_conf.c to see how it is done.
 *
 * Kern Sibbald, January MM
 */

#include "include/bareos.h"
#include "lib/edit.h"

#if defined(HAVE_WIN32)
#include "shlobj.h"
#else
#define MAX_PATH  1024
#endif

/*
 * Define the Union of all the common resource structure definitions.
 */
union UnionOfResources {
   MessagesResource  res_msgs;
   CommonResourceHeader hdr;
};

/* Common Resource definitions */

/*
 * Simply print a message
 */
void prtmsg(void *sock, const char *fmt, ...)
{
   va_list arg_ptr;

   va_start(arg_ptr, fmt);
   vfprintf(stdout, fmt, arg_ptr);
   va_end(arg_ptr);
}

ConfigurationParser *new_config_parser()
{
   ConfigurationParser *config;
   config = (ConfigurationParser *)malloc(sizeof(ConfigurationParser));
   memset(config, 0, sizeof(ConfigurationParser));
   return config;
}

void ConfigurationParser::init(const char *cf,
                  LEX_ERROR_HANDLER *ScanError,
                  LEX_WARNING_HANDLER *scan_warning,
                  INIT_RES_HANDLER *init_res,
                  STORE_RES_HANDLER *store_res,
                  PRINT_RES_HANDLER *print_res,
                  int32_t err_type,
                  void *vres_all,
                  int32_t res_all_size,
                  int32_t r_first,
                  int32_t r_last,
                  ResourceTable *resources,
                  CommonResourceHeader **res_head)
{
   cf_ = cf;
   use_config_include_dir_ = false;
   config_include_dir_ = NULL;
   config_include_naming_format_ = "%s/%s/%s.conf";
   used_config_path_ = NULL;
   scan_error_ = ScanError;
   scan_warning_ = scan_warning;
   init_res_ = init_res;
   store_res_ = store_res;
   print_res_ = print_res;
   err_type_ = err_type;
   res_all_ = vres_all;
   res_all_size_ = res_all_size;
   r_first_ = r_first;
   r_last_ = r_last;
   resources_ = resources;
   res_head_ = res_head;
}

void ConfigurationParser::SetDefaultConfigFilename(const char *filename)
{
   config_default_filename_ = bstrdup(filename);
}

void ConfigurationParser::SetConfigIncludeDir(const char* rel_path)
{
   config_include_dir_ = bstrdup(rel_path);
}

bool ConfigurationParser::ParseConfig()
{
   static bool first = true;
   int errstat;
   PoolMem config_path;

   if (first && (errstat = RwlInit(&res_lock_)) != 0) {
      berrno be;
      Jmsg1(NULL, M_ABORT, 0, _("Unable to initialize resource lock. ERR=%s\n"),
            be.bstrerror(errstat));
   }
   first = false;

   if (!FindConfigPath(config_path)) {
      Jmsg0(NULL, M_ERROR_TERM, 0, _("Failed to find config filename.\n"));
   }
   used_config_path_ = bstrdup(config_path.c_str());
   Dmsg1(100, "config file = %s\n", used_config_path_);
   return ParseConfigFile(config_path.c_str(), NULL, scan_error_, scan_warning_, err_type_);
}

bool ConfigurationParser::ParseConfigFile(const char *cf, void *caller_ctx, LEX_ERROR_HANDLER *ScanError,
                               LEX_WARNING_HANDLER *scan_warning, int32_t err_type)
{
   bool result = true;
   LEX *lc = NULL;
   int token, i, pass;
   int res_type = 0;
   enum parse_state state = p_none;
   ResourceTable *res_table = NULL;
   ResourceItem *items = NULL;
   ResourceItem *item = NULL;
   int level = 0;

   /*
    * Make two passes. The first builds the name symbol table,
    * and the second picks up the items.
    */
   Dmsg1(900, "Enter ParseConfigFile(%s)\n", cf);
   for (pass = 1; pass <= 2; pass++) {
      Dmsg1(900, "ParseConfig pass %d\n", pass);
      if ((lc = lex_open_file(lc, cf, ScanError, scan_warning)) == NULL) {
         berrno be;

         /*
          * We must create a lex packet to print the error
          */
         lc = (LEX *)malloc(sizeof(LEX));
         memset(lc, 0, sizeof(LEX));

         if (ScanError) {
            lc->ScanError = ScanError;
         } else {
            LexSetDefaultErrorHandler(lc);
         }

         if (scan_warning) {
            lc->scan_warning = scan_warning;
         } else {
            LexSetDefaultWarningHandler(lc);
         }

         LexSetErrorHandlerErrorType(lc, err_type) ;
         scan_err2(lc, _("Cannot open config file \"%s\": %s\n"),
            cf, be.bstrerror());
         free(lc);

         return false;
      }
      LexSetErrorHandlerErrorType(lc, err_type);
      lc->error_counter = 0;
      lc->caller_ctx = caller_ctx;

      while ((token=LexGetToken(lc, BCT_ALL)) != BCT_EOF) {
         Dmsg3(900, "parse state=%d pass=%d got token=%s\n", state, pass,
               lex_tok_to_str(token));
         switch (state) {
         case p_none:
            if (token == BCT_EOL) {
               break;
            } else if (token == BCT_UTF8_BOM) {
               /*
                * We can assume the file is UTF-8 as we have seen a UTF-8 BOM
                */
               break;
            } else if (token == BCT_UTF16_BOM) {
               scan_err0(lc, _("Currently we cannot handle UTF-16 source files. "
                               "Please convert the conf file to UTF-8\n"));
               goto bail_out;
            } else if (token != BCT_IDENTIFIER) {
               scan_err1(lc, _("Expected a Resource name identifier, got: %s"), lc->str);
               goto bail_out;
            }
            res_table = get_resource_table(lc->str);
            if(res_table && res_table->items) {
               items = res_table->items;
               state = p_resource;
               res_type = res_table->rcode;
               InitResource(res_type, items, pass, res_table->initres);
            }
            if (state == p_none) {
               scan_err1(lc, _("expected resource name, got: %s"), lc->str);
               goto bail_out;
            }
            break;
         case p_resource:
            switch (token) {
            case BCT_BOB:
               level++;
               break;
            case BCT_IDENTIFIER:
               if (level != 1) {
                  scan_err1(lc, _("not in resource definition: %s"), lc->str);
                  goto bail_out;
               }
               i = GetResourceItemIndex(items, lc->str);
               if (i>=0) {
                  item = &items[i];
                  /*
                   * If the CFG_ITEM_NO_EQUALS flag is set we do NOT
                   *   scan for = after the keyword
                   */
                  if (!(item->flags & CFG_ITEM_NO_EQUALS)) {
                     token = LexGetToken(lc, BCT_SKIP_EOL);
                     Dmsg1 (900, "in BCT_IDENT got token=%s\n", lex_tok_to_str(token));
                     if (token != BCT_EQUALS) {
                        scan_err1(lc, _("expected an equals, got: %s"), lc->str);
                        goto bail_out;
                     }
                  }

                  /*
                   * See if we are processing a deprecated keyword if so warn the user about it.
                   */
                  if (item->flags & CFG_ITEM_DEPRECATED) {
                     scan_warn2(lc, _("using deprecated keyword %s on line %d"), item->name, lc->line_no);
                     /*
                      * As we only want to warn we continue parsing the config. So no goto bail_out here.
                      */
                  }

                  Dmsg1(800, "calling handler for %s\n", item->name);

                  /*
                   * Call item handler
                   */
                  if (!StoreResource(item->type, lc, item, i, pass)) {
                     /*
                      * None of the generic types fired if there is a registered callback call that now.
                      */
                     if (store_res_) {
                        store_res_(lc, item, i, pass);
                     }
                  }
               } else {
                  Dmsg2(900, "level=%d id=%s\n", level, lc->str);
                  Dmsg1(900, "Keyword = %s\n", lc->str);
                  scan_err1(lc, _("Keyword \"%s\" not permitted in this resource.\n"
                                  "Perhaps you left the trailing brace off of the previous resource."), lc->str);
                  goto bail_out;
               }
               break;

            case BCT_EOB:
               level--;
               state = p_none;
               Dmsg0(900, "BCT_EOB => define new resource\n");
               if (((UnionOfResources *)res_all_)->hdr.name == NULL) {
                  scan_err0(lc, _("Name not specified for resource"));
                  goto bail_out;
               }
               /* save resource */
               if (!save_resource(res_type, items, pass)) {
                  scan_err0(lc, _("save_resource failed"));
                  goto bail_out;
               }
               break;

            case BCT_EOL:
               break;

            default:
               scan_err2(lc, _("unexpected token %d %s in resource definition"),
                  token, lex_tok_to_str(token));
               goto bail_out;
            }
            break;
         default:
            scan_err1(lc, _("Unknown parser state %d\n"), state);
            goto bail_out;
         }
      }
      if (state != p_none) {
         scan_err0(lc, _("End of conf file reached with unclosed resource."));
         goto bail_out;
      }
      if (debug_level >= 900 && pass == 2) {
         int i;
         for (i = r_first_; i <= r_last_; i++) {
            dump_resource(i, res_head_[i-r_first_], prtmsg, NULL, false);
         }
      }

      if (lc->error_counter > 0) {
         result = false;
      }

      lc = lex_close_file(lc);
   }
   Dmsg0(900, "Leave ParseConfigFile()\n");

   return result;

bail_out:
   /* close all open configuration files */
   while(lc) {
      lc = lex_close_file(lc);
   }

   return false;
}

const char *ConfigurationParser::get_resource_type_name(int code)
{
   return res_to_str(code);
}

int ConfigurationParser::get_resource_table_index(int resource_type)
{
   int rindex = -1;

   if ((resource_type >= r_first_) && (resource_type <= r_last_)) {
      rindex = resource_type = r_first_;
   }

   return rindex;
}

ResourceTable *ConfigurationParser::get_resource_table(int resource_type)
{
   ResourceTable *result = NULL;
   int rindex = get_resource_table_index(resource_type);

   if (rindex >= 0) {
      result = &resources_[rindex];
   }

   return result;
}

ResourceTable *ConfigurationParser::get_resource_table(const char *resource_type_name)
{
   ResourceTable *result = NULL;
   int i;

   for (i = 0; resources_[i].name; i++) {
      if (bstrcasecmp(resources_[i].name, resource_type_name)) {
         result = &resources_[i];
      }
   }

   return result;
}

int ConfigurationParser::GetResourceItemIndex(ResourceItem *items, const char *item)
{
   int result = -1;
   int i;

   for (i = 0; items[i].name; i++) {
      if (bstrcasecmp(items[i].name, item)) {
         result = i;
         break;
      }
   }

   return result;
}

ResourceItem *ConfigurationParser::get_resource_item(ResourceItem *items, const char *item)
{
   ResourceItem *result = NULL;
   int i = -1;

   i = GetResourceItemIndex(items, item);
   if (i>=0) {
      result = &items[i];
   }

   return result;
}

const char *ConfigurationParser::get_default_configdir()
{
#if defined(HAVE_WIN32)
   HRESULT hr;
   static char szConfigDir[MAX_PATH + 1] = { 0 };

   if (!p_SHGetFolderPath) {
      bstrncpy(szConfigDir, DEFAULT_CONFIGDIR, sizeof(szConfigDir));
      return szConfigDir;
   }

   if (szConfigDir[0] == '\0') {
      hr = p_SHGetFolderPath(NULL, CSIDL_COMMON_APPDATA, NULL, 0, szConfigDir);

      if (SUCCEEDED(hr)) {
         bstrncat(szConfigDir, "\\Bareos", sizeof(szConfigDir));
      } else {
         bstrncpy(szConfigDir, DEFAULT_CONFIGDIR, sizeof(szConfigDir));
      }
   }

   return szConfigDir;
#else
   return CONFDIR;
#endif
}

#ifdef HAVE_SETENV
static inline void set_env(const char *key, const char *value)
{
   setenv(key, value, 1);
}
#elif HAVE_PUTENV
static inline void set_env(const char *key, const char *value)
{
   PoolMem env_string;

   Mmsg(env_string, "%s=%s", key, value);
   putenv(bstrdup(env_string.c_str()));
}
#else
static inline void set_env(const char *key, const char *value)
{
}
#endif

bool ConfigurationParser::get_config_file(PoolMem &full_path, const char *config_dir, const char *config_filename)
{
   bool found = false;

   if (!PathIsDirectory(config_dir)) {
      return false;
   }

   if (config_filename) {
      full_path.strcpy(config_dir);
      if (PathAppend(full_path, config_filename)) {
         if (PathExists(full_path)) {
            config_dir_ = bstrdup(config_dir);
            found = true;
         }
      }
   }

   return found;
}

bool ConfigurationParser::get_config_include_path(PoolMem &full_path, const char *config_dir)
{
   bool found = false;

   if (config_include_dir_) {
      /*
       * Set full_path to the initial part of the include path,
       * so it can be used as result, even on errors.
       * On success, full_path will be overwritten with the full path.
       */
      full_path.strcpy(config_dir);
      PathAppend(full_path, config_include_dir_);
      if (PathIsDirectory(full_path)) {
         config_dir_ = bstrdup(config_dir);
         /*
          * Set full_path to wildcard path.
          */
         if (GetPathOfResource(full_path, NULL, NULL, NULL, true)) {
            use_config_include_dir_ = true;
            found = true;
         }
      }
   }

   return found;
}

/*
 * Returns false on error
 *         true  on OK, with full_path set to where config file should be
 */
bool ConfigurationParser::FindConfigPath(PoolMem &full_path)
{
   bool found = false;
   PoolMem config_dir;
   PoolMem config_path_file;

   if (!cf_) {
      /*
       * No path is given, so use the defaults.
       */
      found = get_config_file(full_path, get_default_configdir(), config_default_filename_);
      if (!found) {
         config_path_file.strcpy(full_path);
         found = get_config_include_path(full_path, get_default_configdir());
      }
      if (!found) {
         Jmsg2(NULL, M_ERROR, 0,
               _("Failed to read config file at the default locations "
                 "\"%s\" (config file path) and \"%s\" (config include directory).\n"),
               config_path_file.c_str(), full_path.c_str());
      }
   } else if (PathExists(cf_)) {
      /*
       * Path is given and exists.
       */
      if (PathIsDirectory(cf_)) {
         found = get_config_file(full_path, cf_, config_default_filename_);
         if (!found) {
            config_path_file.strcpy(full_path);
            found = get_config_include_path(full_path, cf_);
         }
         if (!found) {
            Jmsg3(NULL, M_ERROR, 0,
                  _("Failed to find configuration files under directory \"%s\". "
                  "Did look for \"%s\" (config file path) and \"%s\" (config include directory).\n"),
                  cf_, config_path_file.c_str(), full_path.c_str());
         }
      } else {
         full_path.strcpy(cf_);
         PathGetDirectory(config_dir, full_path);
         config_dir_ = bstrdup(config_dir.c_str());
         found = true;
      }
   } else if (!config_default_filename_) {
      /*
       * Compatibility with older versions.
       * If config_default_filename_ is not set,
       * cf_ may contain what is expected in config_default_filename_.
       */
      found = get_config_file(full_path, get_default_configdir(), cf_);
      if (!found) {
         Jmsg2(NULL, M_ERROR, 0,
               _("Failed to find configuration files at \"%s\" and \"%s\".\n"),
               cf_, full_path.c_str());
      }
   } else {
      Jmsg1(NULL, M_ERROR, 0, _("Failed to read config file \"%s\"\n"), cf_);
   }

   if (found) {
      set_env("BAREOS_CFGDIR", config_dir_);
   }

   return found;
}

void ConfigurationParser::FreeResources()
{
   for (int i = r_first_; i<= r_last_; i++) {
      FreeResource(res_head_[i-r_first_], i);
      res_head_[i-r_first_] = NULL;
   }

   if (config_default_filename_) {
      free((void *)config_default_filename_);
   }

   if (config_dir_) {
      free((void *)config_dir_);
   }

   if (config_include_dir_) {
      free((void *)config_include_dir_);
   }

   if (used_config_path_) {
      free((void *)used_config_path_);
   }

}

CommonResourceHeader **ConfigurationParser::save_resources()
{
   int num = r_last_ - r_first_ + 1;
   CommonResourceHeader **res = (CommonResourceHeader **)malloc(num*sizeof(CommonResourceHeader *));

   for (int i = 0; i < num; i++) {
      res[i] = res_head_[i];
      res_head_[i] = NULL;
   }

   return res;
}

CommonResourceHeader **ConfigurationParser::new_res_head()
{
   int size = (r_last_ - r_first_ + 1) * sizeof(CommonResourceHeader *);
   CommonResourceHeader **res = (CommonResourceHeader **)malloc(size);

   memset(res, 0, size);

   return res;
}

/*
 * Initialize the static structure to zeros, then apply all the default values.
 */
void ConfigurationParser::InitResource(int type,
                           ResourceItem *items,
                           int pass,
                           std::function<void *(void *res)> initres) {
   UnionOfResources *res_all;

   memset(res_all_, 0, res_all_size_);
   res_all = ((UnionOfResources *)res_all_);
   if (initres != nullptr) {
      initres(res_all);
   }
   res_all->hdr.rcode  = type;
   res_all->hdr.refcnt = 1;

   /*
    * See what pass of the config parsing this is.
    */
   switch (pass) {
   case 1: {
      /*
       * Set all defaults for types that are filled in pass 1 of the config parser.
       */
      int i;

      for (i = 0; items[i].name; i++) {
         Dmsg3(900, "Item=%s def=%s defval=%s\n", items[i].name,
               (items[i].flags & CFG_ITEM_DEFAULT) ? "yes" : "no",
               (items[i].default_value) ? items[i].default_value : "None");

         /*
          * Sanity check.
          *
          * Items with a default value but without the CFG_ITEM_DEFAULT flag set
          * are most of the time an indication of a programmers error.
          */
         if (items[i].default_value != NULL && !(items[i].flags & CFG_ITEM_DEFAULT)) {
            Pmsg1(000, _("Found config item %s which has default value but no CFG_ITEM_DEFAULT flag set\n"),
                  items[i].name);
            items[i].flags |= CFG_ITEM_DEFAULT;
         }

         /*
          * See if the CFG_ITEM_DEFAULT flag is set and a default value is available.
          */
         if (items[i].flags & CFG_ITEM_DEFAULT && items[i].default_value != NULL) {
            /*
             * First try to handle the generic types.
             */
            switch (items[i].type) {
            case CFG_TYPE_BIT:
               if (bstrcasecmp(items[i].default_value, "on")) {
                  SetBit(items[i].code, items[i].bitvalue);
               } else if (bstrcasecmp(items[i].default_value, "off")) {
                  ClearBit(items[i].code, items[i].bitvalue);
               }
               break;
            case CFG_TYPE_BOOL:
               if (bstrcasecmp(items[i].default_value, "yes") ||
                   bstrcasecmp(items[i].default_value, "true")) {
                  *(items[i].boolvalue) = true;
               } else if (bstrcasecmp(items[i].default_value, "no") ||
                          bstrcasecmp(items[i].default_value, "false")) {
                  *(items[i].boolvalue) = false;
               }
               break;
            case CFG_TYPE_PINT32:
            case CFG_TYPE_INT32:
            case CFG_TYPE_SIZE32:
               *(items[i].ui32value) = str_to_int32(items[i].default_value);
               break;
            case CFG_TYPE_INT64:
               *(items[i].i64value) = str_to_int64(items[i].default_value);
               break;
            case CFG_TYPE_SIZE64:
               *(items[i].ui64value) = str_to_uint64(items[i].default_value);
               break;
            case CFG_TYPE_SPEED:
               *(items[i].ui64value) = str_to_uint64(items[i].default_value);
               break;
            case CFG_TYPE_TIME:
               *(items[i].utimevalue) = str_to_int64(items[i].default_value);
               break;
            case CFG_TYPE_STRNAME:
            case CFG_TYPE_STR:
               *(items[i].value) = bstrdup(items[i].default_value);
               break;
            case CFG_TYPE_STDSTR:
               *(items[i].strValue) = new std::string(items[i].default_value);
               break;
            case CFG_TYPE_DIR: {
               PoolMem pathname(PM_FNAME);

               PmStrcpy(pathname, items[i].default_value);
               if (*pathname.c_str() != '|') {
                  int size;

                  /*
                   * Make sure we have enough room
                   */
                  size = pathname.size() + 1024;
                  pathname.check_size(size);
                  DoShellExpansion(pathname.c_str(), pathname.size());
               }
               *items[i].value = bstrdup(pathname.c_str());
               break;
            }
            case CFG_TYPE_STDSTRDIR: {
               PoolMem pathname(PM_FNAME);

               PmStrcpy(pathname, items[i].default_value);
               if (*pathname.c_str() != '|') {
                  int size;

                  /*
                   * Make sure we have enough room
                   */
                  size = pathname.size() + 1024;
                  pathname.check_size(size);
                  DoShellExpansion(pathname.c_str(), pathname.size());
               }
               *(items[i].strValue) = new std::string(pathname.c_str());
               break;
            }
            case CFG_TYPE_ADDRESSES:
               init_default_addresses(items[i].dlistvalue, items[i].default_value);
               break;
            default:
               /*
                * None of the generic types fired if there is a registered callback call that now.
                */
               if (init_res_) {
                  init_res_(&items[i], pass);
               }
               break;
            }

            if (!omit_defaults_) {
               SetBit(i, res_all->hdr.inherit_content);
            }
         }

         /*
          * If this triggers, take a look at lib/parse_conf.h
          */
         if (i >= MAX_RES_ITEMS) {
            Emsg1(M_ERROR_TERM, 0, _("Too many items in %s resource\n"), resources_[type - r_first_].name);
         }
      }
      break;
   }
   case 2: {
      /*
       * Set all defaults for types that are filled in pass 2 of the config parser.
       */
      int i;

      for (i = 0; items[i].name; i++) {
         Dmsg3(900, "Item=%s def=%s defval=%s\n", items[i].name,
               (items[i].flags & CFG_ITEM_DEFAULT) ? "yes" : "no",
               (items[i].default_value) ? items[i].default_value : "None");

         /*
          * See if the CFG_ITEM_DEFAULT flag is set and a default value is available.
          */
         if (items[i].flags & CFG_ITEM_DEFAULT && items[i].default_value != NULL) {
            /*
             * First try to handle the generic types.
             */
            switch (items[i].type) {
            case CFG_TYPE_ALIST_STR:
               if (!*items[i].alistvalue) {
                  *(items[i].alistvalue) = New(alist(10, owned_by_alist));
               }
               (*(items[i].alistvalue))->append(bstrdup(items[i].default_value));
               break;
            case CFG_TYPE_ALIST_DIR: {
               PoolMem pathname(PM_FNAME);

               if (!*items[i].alistvalue) {
                  *(items[i].alistvalue) = New(alist(10, owned_by_alist));
               }

               PmStrcpy(pathname, items[i].default_value);
               if (*items[i].default_value != '|') {
                  int size;

                  /*
                   * Make sure we have enough room
                   */
                  size = pathname.size() + 1024;
                  pathname.check_size(size);
                  DoShellExpansion(pathname.c_str(), pathname.size());
               }
               (*(items[i].alistvalue))->append(bstrdup(pathname.c_str()));
               break;
            }
            default:
               /*
                * None of the generic types fired if there is a registered callback call that now.
                */
               if (init_res_) {
                  init_res_(&items[i], pass);
               }
               break;
            }

            if (!omit_defaults_) {
               SetBit(i, res_all->hdr.inherit_content);
            }
         }

         /*
          * If this triggers, take a look at lib/parse_conf.h
          */
         if (i >= MAX_RES_ITEMS) {
            Emsg1(M_ERROR_TERM, 0, _("Too many items in %s resource\n"), resources_[type - r_first_].name);
         }
      }
      break;
   }
   default:
      break;
   }
}

bool ConfigurationParser::RemoveResource(int type, const char *name)
{
   int rindex = type - r_first_;
   CommonResourceHeader *last;

   /*
    * Remove resource from list.
    *
    * Note: this is intended for removing a resource that has just been added,
    * but proven to be incorrect (added by console command "configure add").
    * For a general approach, a check if this resource is referenced by other resources must be added.
    * If it is referenced, don't remove it.
    */
   last = NULL;
   for (CommonResourceHeader *res = res_head_[rindex]; res; res = res->next) {
      if (bstrcmp(res->name, name)) {
         if (!last) {
            Dmsg2(900, _("removing resource %s, name=%s (first resource in list)\n"), res_to_str(type), name);
            res_head_[rindex] = res->next;
        } else {
            Dmsg2(900, _("removing resource %s, name=%s\n"), res_to_str(type), name);
            last->next = res->next;
        }
        res->next = NULL;
        FreeResource(res, type);
        return true;
      }
      last = res;
   }

   /*
    * Resource with this name not found
    */
   return false;
}

void ConfigurationParser::DumpResources(void sendit(void *sock, const char *fmt, ...),
                            void *sock, bool hide_sensitive_data)
{
   for (int i = r_first_; i <= r_last_; i++) {
      if (res_head_[i - r_first_]) {
         dump_resource(i,res_head_[i - r_first_],sendit, sock, hide_sensitive_data);
      }
   }
}

bool ConfigurationParser::GetPathOfResource(PoolMem &path, const char *component,
                                  const char *resourcetype, const char *name, bool set_wildcards)
{
   PoolMem rel_path(PM_FNAME);
   PoolMem directory(PM_FNAME);
   PoolMem resourcetype_lowercase(resourcetype);
   resourcetype_lowercase.toLower();

   if (!component) {
      if (config_include_dir_) {
         component = config_include_dir_;
      } else {
         return false;
      }
   }

   if (resourcetype_lowercase.strlen() <= 0) {
      if (set_wildcards) {
         resourcetype_lowercase.strcpy("*");
      } else {
         return false;
      }
   }

   if (!name) {
      if (set_wildcards) {
         name = "*";
      } else {
         return false;
      }
   }

   path.strcpy(config_dir_);
   rel_path.bsprintf(config_include_naming_format_, component, resourcetype_lowercase.c_str(), name);
   PathAppend(path, rel_path);

   return true;
}

bool ConfigurationParser::GetPathOfNewResource(PoolMem &path, PoolMem &extramsg, const char *component,
                                      const char *resourcetype, const char *name,
                                      bool error_if_exists, bool create_directories)
{
   PoolMem rel_path(PM_FNAME);
   PoolMem directory(PM_FNAME);
   PoolMem resourcetype_lowercase(resourcetype);
   resourcetype_lowercase.toLower();

   if (!GetPathOfResource(path, component, resourcetype, name, false)) {
      return false;
   }

   PathGetDirectory(directory, path);

   if (create_directories) {
      PathCreate(directory);
   }

   if (!PathExists(directory)) {
      extramsg.bsprintf("Resource config directory \"%s\" does not exist.\n", directory.c_str());
      return false;
   }

   /*
    * Store name for temporary file in extramsg.
    * Can be used, if result is true.
    * Otherwise it contains an error message.
    */
   extramsg.bsprintf("%s.tmp", path.c_str());

   if (!error_if_exists) {
      return true;
   }

   /*
    * File should not exists, as it is going to be created.
    */
   if (PathExists(path)) {
      extramsg.bsprintf("Resource config file \"%s\" already exists.\n", path.c_str());
      return false;
   }

   if (PathExists(extramsg)) {
      extramsg.bsprintf("Temporary resource config file \"%s.tmp\" already exists.\n", path.c_str());
      return false;
   }

   return true;
}

