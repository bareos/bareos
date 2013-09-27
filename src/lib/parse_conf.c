/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2013 Bareos GmbH & Co. KG

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
 * This file contains the common parts of the BAREOS
 * configuration routines.
 *
 * Note, the configuration file parser consists of three parts
 *
 * 1. The generic lexical scanner in lib/lex.c and lib/lex.h
 *
 * 2. The generic config scanner in lib/parse_conf.c and
 *    lib/parse_conf.h.
 *
 *    These files contain the parser code, some utility
 *    routines, and the common store routines (name, int,
 *    string, time, int64, size, ...).
 *
 * 3. The daemon specific file, which contains the Resource
 *    definitions as well as any specific store routines
 *    for the resource records.
 *
 * N.B. This is a two pass parser, so if you malloc() a string
 * in a "store" routine, you must ensure to do it during
 * only one of the two passes, or to free it between.
 * Also, note that the resource record is malloced and
 * saved in save_resource() during pass 1. Anything that
 * you want saved after pass two (e.g. resource pointers)
 * must explicitly be done in save_resource. Take a look
 * at the Job resource in src/dird/dird_conf.c to see how
 * it is done.
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

#if defined(_MSC_VER)
// work around visual studio name mangling preventing external linkage since res_all
// is declared as a different type when instantiated.
extern "C" URES res_all;
#else
extern  URES res_all;
#endif

/*
 * Simply print a message
 */
static void prtmsg(void *sock, const char *fmt, ...)
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

static const char *get_default_configdir()
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
   return SYSCONFDIR;
#endif
}

/*
 * Returns false on error
 *         true  on OK, with full_path set to where config file should be
 */
static bool find_config_file(const char *config_file, char *full_path, int max_path)
{
   int dir_length, file_length;
   const char *config_dir;
#if defined(HAVE_SETENV) || defined(HAVE_PUTENV)
   char *bp;
   POOL_MEM env_string(PM_NAME);
#endif

   /*
    * If a full path specified, use it
    */
   file_length = strlen(config_file) + 1;
   if (first_path_separator(config_file) != NULL) {
      if (file_length > max_path) {
         return false;
      }

      bstrncpy(full_path, config_file, file_length);

#ifdef HAVE_SETENV
      pm_strcpy(env_string, config_file);
      bp = (char *)last_path_separator(env_string.c_str());
      *bp = '\0';
      setenv("BAREOS_CFGDIR", env_string.c_str(), 1);
#elif HAVE_PUTENV
      Mmsg(env_string, "BAREOS_CFGDIR=%s", config_file);
      bp = (char *)last_path_separator(env_string.c_str());
      *bp = '\0';
      putenv(bstrdup(env_string.c_str()));
#endif

      return true;
   }

   /*
    * config_file is default file name, now find default dir
    */
   config_dir = get_default_configdir();
   dir_length = strlen(config_dir);

   if ((dir_length + 1 + file_length) > max_path) {
      return false;
   }

#ifdef HAVE_SETENV
   pm_strcpy(env_string, config_dir);
   setenv("BAREOS_CFGDIR", env_string.c_str(), 1);
#elif HAVE_PUTENV
   Mmsg(env_string, "BAREOS_CFGDIR=%s", config_dir);
   putenv(bstrdup(env_string.c_str()));
#endif

   memcpy(full_path, config_dir, dir_length + 1);

   if (!IsPathSeparator(full_path[dir_length - 1])) {
      full_path[dir_length++] = '/';
   }

   memcpy(full_path + dir_length, config_file, file_length);

   return true;
}

void CONFIG::init(const char *cf,
                  LEX_ERROR_HANDLER *scan_error,
                  LEX_WARNING_HANDLER *scan_warning,
                  INIT_RES_HANDLER *init_res,
                  STORE_RES_HANDLER *store_res,
                  int32_t err_type,
                  void *vres_all,
                  int32_t res_all_size,
                  int32_t r_first,
                  int32_t r_last,
                  RES_TABLE *resources)
{
   m_cf = cf;
   m_scan_error = scan_error;
   m_scan_warning = scan_warning;
   m_init_res = init_res;
   m_store_res = store_res;
   m_err_type = err_type;
   m_res_all = vres_all;
   m_res_all_size = res_all_size;
   m_r_first = r_first;
   m_r_last = r_last;
   m_resources = resources;

   new_res_containers();
}

bool CONFIG::parse_config()
{
   LEX *lc = NULL;
   int token, i, pass;
   int res_type = 0;
   enum parse_state state = p_none;
   RES_ITEM *items = NULL;
   int level = 0;
   static bool first = true;
   int errstat;
   const char *cf = m_cf;
   LEX_ERROR_HANDLER *scan_error = m_scan_error;
   LEX_WARNING_HANDLER *scan_warning = m_scan_warning;
   int err_type = m_err_type;

   if (first && (errstat=rwl_init(&m_res_lock)) != 0) {
      berrno be;
      Jmsg1(NULL, M_ABORT, 0, _("Unable to initialize resource lock. ERR=%s\n"),
            be.bstrerror(errstat));
   }
   first = false;

   char *full_path = (char *)alloca(MAX_PATH + 1);

   if (!find_config_file(cf, full_path, MAX_PATH +1)) {
      Jmsg0(NULL, M_ABORT, 0, _("Config filename too long.\n"));
   }
   cf = full_path;

   /*
    * Make two passes. The first builds the name symbol table,
    * and the second picks up the items.
    */
   Dmsg0(900, "Enter parse_config()\n");
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
         bstrncpy(lc->str, cf, sizeof(lc->str));
         lc->fname = lc->str;
         scan_err2(lc, _("Cannot open config file \"%s\": %s\n"),
            lc->str, be.bstrerror());
         free(lc);
         return 0;
      }
      lex_set_error_handler_error_type(lc, err_type) ;
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
            for (i = 0; m_resources[i].name; i++) {
               if (bstrcasecmp(m_resources[i].name, lc->str)) {
                  items = m_resources[i].items;
                  if (!items) {
                     break;
                  }
                  state = p_resource;
                  res_type = m_resources[i].rcode;
                  initialize_resource(res_type, items, pass);
                  break;
               }
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
               for (i = 0; items[i].name; i++) {
                  if (bstrcasecmp(items[i].name, lc->str)) {
                     /*
                      * If the CFG_ITEM_NO_EQUALS flag is set we do NOT
                      *   scan for = after the keyword
                      */
                     if (!(items[i].flags & CFG_ITEM_NO_EQUALS)) {
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
                     if (items[i].flags & CFG_ITEM_DEPRECATED) {
                        scan_warn2(lc, _("using deprecated keyword %s on line %d"), items[i].name, lc->line_no);
                        /*
                         * As we only want to warn we continue parsing the config. So no goto bail_out here.
                         */
                     }

                     Dmsg1(800, "calling handler for %s\n", items[i].name);

                     /*
                      * Call item handler
                      */
                     if (!store_resource(items[i].type, lc, &items[i], i, pass)) {
                        /*
                         * None of the generic types fired if there is a registered callback call that now.
                         */
                        if (m_store_res) {
                           m_store_res(this, lc, &items[i], i, pass);
                        }
                     }
                     i = -1;
                     break;
                  }
               }
               if (i >= 0) {
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
               if (res_all.hdr.name == NULL) {
                  scan_err0(lc, _("Name not specified for resource"));
                  goto bail_out;
               }
               save_resource(this, res_type, items, pass);  /* save resource */
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
            dump_all_resources(i, prtmsg, NULL);
         }
      }
      lc = lex_close_file(lc);
   }
   Dmsg0(900, "Leave parse_config()\n");
   return 1;
bail_out:
   if (lc) {
      lc = lex_close_file(lc);
   }
   return 0;
}
