/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.
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

#include "bareos.h"

#if defined(HAVE_WIN32)
#include "shlobj.h"
#else
#define MAX_PATH  1024
#endif

/*
 * Define the Union of all the common resource structure definitions.
 */
union URES {
   MSGSRES  res_msgs;
   RES hdr;
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

CONFIG *new_config_parser()
{
   CONFIG *config;
   config = (CONFIG *)malloc(sizeof(CONFIG));
   memset(config, 0, sizeof(CONFIG));
   return config;
}

void CONFIG::init(const char *cf,
                  LEX_ERROR_HANDLER *scan_error,
                  LEX_WARNING_HANDLER *scan_warning,
                  INIT_RES_HANDLER *init_res,
                  STORE_RES_HANDLER *store_res,
                  PRINT_RES_HANDLER *print_res,
                  int32_t err_type,
                  void *vres_all,
                  int32_t res_all_size,
                  int32_t r_first,
                  int32_t r_last,
                  RES_TABLE *resources,
                  RES **res_head)
{
   m_cf = cf;
   m_use_config_include_dir = false;
   m_config_include_dir = NULL;
   m_config_include_naming_format = "%s/%s/%s.conf";
   m_used_config_path = NULL;
   m_scan_error = scan_error;
   m_scan_warning = scan_warning;
   m_init_res = init_res;
   m_store_res = store_res;
   m_print_res = print_res;
   m_err_type = err_type;
   m_res_all = vres_all;
   m_res_all_size = res_all_size;
   m_r_first = r_first;
   m_r_last = r_last;
   m_resources = resources;
   m_res_head = res_head;
}

void CONFIG::set_default_config_filename(const char *filename)
{
   m_config_default_filename = bstrdup(filename);
}

void CONFIG::set_config_include_dir(const char* rel_path)
{
   m_config_include_dir = bstrdup(rel_path);
}

bool CONFIG::parse_config()
{
   static bool first = true;
   int errstat;
   POOL_MEM config_path;

   if (first && (errstat = rwl_init(&m_res_lock)) != 0) {
      berrno be;
      Jmsg1(NULL, M_ABORT, 0, _("Unable to initialize resource lock. ERR=%s\n"),
            be.bstrerror(errstat));
   }
   first = false;

   if (!find_config_path(config_path)) {
      Jmsg0(NULL, M_ERROR_TERM, 0, _("Failed to find config filename.\n"));
   }
   m_used_config_path = bstrdup(config_path.c_str());
   Dmsg1(100, "config file = %s\n", m_used_config_path);
   return parse_config_file(config_path.c_str(), NULL, m_scan_error, m_scan_warning, m_err_type);
}

bool CONFIG::parse_config_file(const char *cf, void *caller_ctx, LEX_ERROR_HANDLER *scan_error,
                               LEX_WARNING_HANDLER *scan_warning, int32_t err_type)
{
   bool result = true;
   LEX *lc = NULL;
   int token, i, pass;
   int res_type = 0;
   enum parse_state state = p_none;
   RES_TABLE *res_table = NULL;
   RES_ITEM *items = NULL;
   RES_ITEM *item = NULL;
   int level = 0;

   /*
    * Make two passes. The first builds the name symbol table,
    * and the second picks up the items.
    */
   Dmsg1(900, "Enter parse_config_file(%s)\n", cf);
   for (pass = 1; pass <= 2; pass++) {
      Dmsg1(900, "parse_config pass %d\n", pass);
      if ((lc = lex_open_file(lc, cf, scan_error, scan_warning)) == NULL) {
         berrno be;

         /*
          * We must create a lex packet to print the error
          */
         lc = (LEX *)malloc(sizeof(LEX));
         memset(lc, 0, sizeof(LEX));

         if (scan_error) {
            lc->scan_error = scan_error;
         } else {
            lex_set_default_error_handler(lc);
         }

         if (scan_warning) {
            lc->scan_warning = scan_warning;
         } else {
            lex_set_default_warning_handler(lc);
         }

         lex_set_error_handler_error_type(lc, err_type) ;
         scan_err2(lc, _("Cannot open config file \"%s\": %s\n"),
            cf, be.bstrerror());
         free(lc);

         return 0;
      }
      lex_set_error_handler_error_type(lc, err_type);
      lc->error_counter = 0;
      lc->caller_ctx = caller_ctx;

      while ((token=lex_get_token(lc, T_ALL)) != T_EOF) {
         Dmsg3(900, "parse state=%d pass=%d got token=%s\n", state, pass,
               lex_tok_to_str(token));
         switch (state) {
         case p_none:
            if (token == T_EOL) {
               break;
            } else if (token == T_UTF8_BOM) {
               /*
                * We can assume the file is UTF-8 as we have seen a UTF-8 BOM
                */
               break;
            } else if (token == T_UTF16_BOM) {
               scan_err0(lc, _("Currently we cannot handle UTF-16 source files. "
                               "Please convert the conf file to UTF-8\n"));
               goto bail_out;
            } else if (token != T_IDENTIFIER) {
               scan_err1(lc, _("Expected a Resource name identifier, got: %s"), lc->str);
               goto bail_out;
            }
            res_table = get_resource_table(lc->str);
            if(res_table && res_table->items) {
               items = res_table->items;
               state = p_resource;
               res_type = res_table->rcode;
               init_resource(res_type, items, pass);
            }
            if (state == p_none) {
               scan_err1(lc, _("expected resource name, got: %s"), lc->str);
               goto bail_out;
            }
            break;
         case p_resource:
            switch (token) {
            case T_BOB:
               level++;
               break;
            case T_IDENTIFIER:
               if (level != 1) {
                  scan_err1(lc, _("not in resource definition: %s"), lc->str);
                  goto bail_out;
               }
               i = get_resource_item_index(items, lc->str);
               if (i>=0) {
                  item = &items[i];
                  /*
                   * If the CFG_ITEM_NO_EQUALS flag is set we do NOT
                   *   scan for = after the keyword
                   */
                  if (!(item->flags & CFG_ITEM_NO_EQUALS)) {
                     token = lex_get_token(lc, T_SKIP_EOL);
                     Dmsg1 (900, "in T_IDENT got token=%s\n", lex_tok_to_str(token));
                     if (token != T_EQUALS) {
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
                  if (!store_resource(item->type, lc, item, i, pass)) {
                     /*
                      * None of the generic types fired if there is a registered callback call that now.
                      */
                     if (m_store_res) {
                        m_store_res(lc, item, i, pass);
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

            case T_EOB:
               level--;
               state = p_none;
               Dmsg0(900, "T_EOB => define new resource\n");
               if (((URES *)m_res_all)->hdr.name == NULL) {
                  scan_err0(lc, _("Name not specified for resource"));
                  goto bail_out;
               }
               /* save resource */
               if (!save_resource(res_type, items, pass)) {
                  scan_err0(lc, _("save_resource failed"));
                  goto bail_out;
               };
               break;

            case T_EOL:
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
         for (i = m_r_first; i <= m_r_last; i++) {
            dump_resource(i, m_res_head[i-m_r_first], prtmsg, NULL, false);
         }
      }

      if (lc->error_counter > 0) {
         result = false;
      }

      lc = lex_close_file(lc);
   }
   Dmsg0(900, "Leave parse_config_file()\n");

   return result;

bail_out:
   if (lc) {
      lc = lex_close_file(lc);
   }

   return false;
}


RES_TABLE *CONFIG::get_resource_table(const char *resource_type)
{
   RES_TABLE *result = NULL;
   int i;

   for (i = 0; m_resources[i].name; i++) {
      if (bstrcasecmp(m_resources[i].name, resource_type)) {
         result = &m_resources[i];
      }
   }

   return result;
}

int CONFIG::get_resource_item_index(RES_ITEM *items, const char *item)
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

RES_ITEM *CONFIG::get_resource_item(RES_ITEM *items, const char *item)
{
   RES_ITEM *result = NULL;
   int i = -1;

   i = get_resource_item_index(items, item);
   if (i>=0) {
      result = &items[i];
   }

   return result;
}

const char *CONFIG::get_default_configdir()
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
   POOL_MEM env_string;

   Mmsg(env_string, "%s=%s", key, value);
   putenv(bstrdup(env_string.c_str()));
}
#else
static inline void set_env(const char *key, const char *value)
{
}
#endif

bool CONFIG::get_config_file(POOL_MEM &full_path, const char *config_dir, const char *config_filename)
{
   bool found = false;

   if (!path_is_directory(config_dir)) {
      return false;
   }

   if (config_filename) {
      full_path.strcpy(config_dir);
      if (path_append(full_path, config_filename)) {
         if (path_exists(full_path)) {
            m_config_dir = bstrdup(config_dir);
            found = true;
         }
      }
   }

   return found;
}

bool CONFIG::get_config_include_path(POOL_MEM &full_path, const char *config_dir)
{
   bool found = false;

   if (m_config_include_dir) {
      /*
       * Set full_path to the initial part of the include path,
       * so it can be used as result, even on errors.
       * On success, full_path will be overwritten with the full path.
       */
      full_path.strcpy(config_dir);
      path_append(full_path, m_config_include_dir);
      if (path_is_directory(full_path)) {
         m_config_dir = bstrdup(config_dir);
         /*
          * Set full_path to wildcard path.
          */
         if (get_path_of_resource(full_path, NULL, NULL, NULL, true)) {
            m_use_config_include_dir = true;
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
bool CONFIG::find_config_path(POOL_MEM &full_path)
{
   bool found = false;
   POOL_MEM config_dir;
   POOL_MEM config_path_file;

   if (!m_cf) {
      /*
       * No path is given, so use the defaults.
       */
      found = get_config_file(full_path, get_default_configdir(), m_config_default_filename);
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
   } else if (path_exists(m_cf)) {
      /*
       * Path is given and exists.
       */
      if (path_is_directory(m_cf)) {
         found = get_config_file(full_path, m_cf, m_config_default_filename);
         if (!found) {
            config_path_file.strcpy(full_path);
            found = get_config_include_path(full_path, m_cf);
         }
         if (!found) {
            Jmsg3(NULL, M_ERROR, 0,
                  _("Failed to find configuration files under directory \"%s\". "
                  "Did look for \"%s\" (config file path) and \"%s\" (config include directory).\n"),
                  m_cf, config_path_file.c_str(), full_path.c_str());
         }
      } else {
         full_path.strcpy(m_cf);
         path_get_directory(config_dir, full_path);
         m_config_dir = bstrdup(config_dir.c_str());
         found = true;
      }
   } else if (!m_config_default_filename) {
      /*
       * Compatibility with older versions.
       * If m_config_default_filename is not set,
       * m_cf may contain what is expected in m_config_default_filename.
       */
      found = get_config_file(full_path, get_default_configdir(), m_cf);
      if (!found) {
         Jmsg2(NULL, M_ERROR, 0,
               _("Failed to find configuration files at \"%s\" and \"%s\".\n"),
               m_cf, full_path.c_str());
      }
   } else {
      Jmsg1(NULL, M_ERROR, 0, _("Failed to read config file \"%s\"\n"), m_cf);
   }

   if (found) {
      set_env("BAREOS_CFGDIR", m_config_dir);
   }

   return found;
}

void CONFIG::free_resources()
{
   for (int i = m_r_first; i<= m_r_last; i++) {
      free_resource(m_res_head[i-m_r_first], i);
      m_res_head[i-m_r_first] = NULL;
   }

   if (m_config_default_filename) {
      free((void *)m_config_default_filename);
   }

   if (m_config_dir) {
      free((void *)m_config_dir);
   }

   if (m_config_include_dir) {
      free((void *)m_config_include_dir);
   }

   if (m_used_config_path) {
      free((void *)m_used_config_path);
   }

}

RES **CONFIG::save_resources()
{
   int num = m_r_last - m_r_first + 1;
   RES **res = (RES **)malloc(num*sizeof(RES *));

   for (int i = 0; i < num; i++) {
      res[i] = m_res_head[i];
      m_res_head[i] = NULL;
   }

   return res;
}

RES **CONFIG::new_res_head()
{
   int size = (m_r_last - m_r_first + 1) * sizeof(RES *);
   RES **res = (RES **)malloc(size);

   memset(res, 0, size);

   return res;
}

/*
 * Initialize the static structure to zeros, then apply all the default values.
 */
void CONFIG::init_resource(int type, RES_ITEM *items, int pass)
{
   URES *res_all;

   memset(m_res_all, 0, m_res_all_size);
   res_all = ((URES *)m_res_all);
   res_all->hdr.rcode = type;
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
                  set_bit(items[i].code, items[i].bitvalue);
               } else if (bstrcasecmp(items[i].default_value, "off")) {
                  clear_bit(items[i].code, items[i].bitvalue);
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
            case CFG_TYPE_DIR: {
               POOL_MEM pathname(PM_FNAME);

               pm_strcpy(pathname, items[i].default_value);
               if (*pathname.c_str() != '|') {
                  int size;

                  /*
                   * Make sure we have enough room
                   */
                  size = pathname.size() + 1024;
                  pathname.check_size(size);
                  do_shell_expansion(pathname.c_str(), pathname.size());
               }
               *items[i].value = bstrdup(pathname.c_str());
               break;
            }
            case CFG_TYPE_ADDRESSES:
               init_default_addresses(items[i].dlistvalue, items[i].default_value);
               break;
            default:
               /*
                * None of the generic types fired if there is a registered callback call that now.
                */
               if (m_init_res) {
                  m_init_res(&items[i], pass);
               }
               break;
            }

            if (!m_omit_defaults) {
               set_bit(i, res_all->hdr.inherit_content);
            }
         }

         /*
          * If this triggers, take a look at lib/parse_conf.h
          */
         if (i >= MAX_RES_ITEMS) {
            Emsg1(M_ERROR_TERM, 0, _("Too many items in %s resource\n"), m_resources[type - m_r_first]);
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
               POOL_MEM pathname(PM_FNAME);

               if (!*items[i].alistvalue) {
                  *(items[i].alistvalue) = New(alist(10, owned_by_alist));
               }

               pm_strcpy(pathname, items[i].default_value);
               if (*items[i].default_value != '|') {
                  int size;

                  /*
                   * Make sure we have enough room
                   */
                  size = pathname.size() + 1024;
                  pathname.check_size(size);
                  do_shell_expansion(pathname.c_str(), pathname.size());
               }
               (*(items[i].alistvalue))->append(bstrdup(pathname.c_str()));
               break;
            }
            default:
               /*
                * None of the generic types fired if there is a registered callback call that now.
                */
               if (m_init_res) {
                  m_init_res(&items[i], pass);
               }
               break;
            }

            if (!m_omit_defaults) {
               set_bit(i, res_all->hdr.inherit_content);
            }
         }

         /*
          * If this triggers, take a look at lib/parse_conf.h
          */
         if (i >= MAX_RES_ITEMS) {
            Emsg1(M_ERROR_TERM, 0, _("Too many items in %s resource\n"), m_resources[type - m_r_first]);
         }
      }
      break;
   }
   default:
      break;
   }
}

bool CONFIG::remove_resource(int type, const char *name)
{
   int rindex = type - m_r_first;
   RES *last;

   /*
    * Remove resource from list.
    *
    * Note: this is intended for removing a resource that has just been added,
    * but proven to be incorrect (added by console command "configure add").
    * For a general approach, a check if this resource is referenced by other resources must be added.
    * If it is referenced, don't remove it.
    */
   last = NULL;
   for (RES *res = m_res_head[rindex]; res; res = res->next) {
      if (bstrcmp(res->name, name)) {
         if (!last) {
            Dmsg2(900, _("removing resource %s, name=%s (first resource in list)\n"), res_to_str(type), name);
            m_res_head[rindex] = res->next;
        } else {
            Dmsg2(900, _("removing resource %s, name=%s\n"), res_to_str(type), name);
            last->next = res->next;
        }
        res->next = NULL;
        free_resource(res, type);
        return true;
      }
      last = res;
   }

   /*
    * Resource with this name not found
    */
   return false;
}

void CONFIG::dump_resources(void sendit(void *sock, const char *fmt, ...),
                            void *sock, bool hide_sensitive_data)
{
   for (int i = m_r_first; i <= m_r_last; i++) {
      if (m_res_head[i - m_r_first]) {
         dump_resource(i,m_res_head[i - m_r_first],sendit, sock, hide_sensitive_data);
      }
   }
}

bool CONFIG::get_path_of_resource(POOL_MEM &path, const char *component,
                                  const char *resourcetype, const char *name, bool set_wildcards)
{
   POOL_MEM rel_path(PM_FNAME);
   POOL_MEM directory(PM_FNAME);
   POOL_MEM resourcetype_lowercase(resourcetype);
   resourcetype_lowercase.toLower();

   if (!component) {
      if (m_config_include_dir) {
         component = m_config_include_dir;
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

   path.strcpy(m_config_dir);
   rel_path.bsprintf(m_config_include_naming_format, component, resourcetype_lowercase.c_str(), name);
   path_append(path, rel_path);

   return true;
}

bool CONFIG::get_path_of_new_resource(POOL_MEM &path, POOL_MEM &extramsg, const char *component,
                                      const char *resourcetype, const char *name,
                                      bool error_if_exists, bool create_directories)
{
   POOL_MEM rel_path(PM_FNAME);
   POOL_MEM directory(PM_FNAME);
   POOL_MEM resourcetype_lowercase(resourcetype);
   resourcetype_lowercase.toLower();

   if (!get_path_of_resource(path, component, resourcetype, name, false)) {
      return false;
   }

   path_get_directory(directory, path);

   if (create_directories) {
      path_create(directory);
   }

   if (!path_exists(directory)) {
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
   if (path_exists(path)) {
      extramsg.bsprintf("Resource config file \"%s\" already exists.\n", path.c_str());
      return false;
   }

   if (path_exists(extramsg)) {
      extramsg.bsprintf("Temporary resource config file \"%s.tmp\" already exists.\n", path.c_str());
      return false;
   }

   return true;
}

void free_tls_t(tls_t &tls)
{
   if (tls.ctx) {
      free_tls_context(tls.ctx);
   }
   if (tls.ca_certfile) {
      free(tls.ca_certfile);
   }
   if (tls.ca_certdir) {
      free(tls.ca_certdir);
   }
   if (tls.crlfile) {
      free(tls.crlfile);
   }
   if (tls.certfile) {
      free(tls.certfile);
   }
   if (tls.keyfile) {
      free(tls.keyfile);
   }
   if (tls.dhfile) {
      free(tls.dhfile);
   }
   if (tls.cipherlist) {
      free(tls.cipherlist);
   }
   if (tls.allowed_cns) {
      delete tls.allowed_cns;
   }
}
