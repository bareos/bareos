/*
   Copyright (C) 2011-2011 Bacula Systems(R) SA
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2013 Bareos GmbH & Co. KG

   This program is Free Software; you can modify it under the terms of
   version three of the GNU Affero General Public License as published by the
   Free Software Foundation, which is listed in the file LICENSE.

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
 * Handle simple configuration file such as "ini" files.
 * key1 = val     # comment
 * key2 = val     # <type>
 */

#include "bareos.h"
#include "ini.h"

#define bfree_and_null_const(a) do{if(a){free((void *)a); (a)=NULL;}} while(0)
static int dbglevel = 100;

/*
 * We use this structure to associate a key to the function
 */
struct ini_store {
   const char *key;
   const char *comment;
   int type;
};

static struct ini_store funcs[] = {
   { "@INT32@", "Integer", INI_CFG_TYPE_INT32 },
   { "@PINT32@", "Integer", INI_CFG_TYPE_PINT32 },
   { "@INT64@", "Integer", INI_CFG_TYPE_INT64 },
   { "@PINT64@", "Positive Integer", INI_CFG_TYPE_PINT64 },
   { "@NAME@", "Simple String", INI_CFG_TYPE_NAME },
   { "@STR@", "String", INI_CFG_TYPE_STR },
   { "@BOOL@", "on/off", INI_CFG_TYPE_BOOL },
   { "@ALIST@", "String list", INI_CFG_TYPE_ALIST_STR },
   { NULL, NULL, 0 }
};

/*
 * Get handler code from storage type.
 */
const char *ini_get_store_code(int type)
{
   int i;

   for (i = 0; funcs[i].key ; i++) {
      if (funcs[i].type == type) {
         return funcs[i].key;
      }
   }
   return NULL;
}

/*
 * Get storage type from handler name.
 */
int ini_get_store_type(const char *key)
{
   int i;

   for (i = 0; funcs[i].key ; i++) {
      if (!strcmp(funcs[i].key, key)) {
         return funcs[i].type;
      }
   }
   return 0;
}

/* ----------------------------------------------------------------
 * Handle data type. Import/Export
 * ----------------------------------------------------------------
 */
static bool ini_store_str(LEX *lc, ConfigFile *inifile, ini_items *item)
{
   if (!lc) {
      Mmsg(inifile->edit, "%s", item->val.strval);
      return true;
   }
   if (lex_get_token(lc, BCT_STRING) == BCT_ERROR) {
      return false;
   }
   /*
    * If already allocated, free first
    */
   if (item->found && item->val.strval) {
      free(item->val.strval);
   }
   item->val.strval = bstrdup(lc->str);
   scan_to_eol(lc);
   return true;
}

static bool ini_store_name(LEX *lc, ConfigFile *inifile, ini_items *item)
{
   if (!lc) {
      Mmsg(inifile->edit, "%s", item->val.nameval);
      return true;
   }
   if (lex_get_token(lc, BCT_NAME) == BCT_ERROR) {
      return false;
   }
   bstrncpy(item->val.nameval, lc->str, sizeof(item->val.nameval));
   scan_to_eol(lc);
   return true;
}

static bool ini_store_alist_str(LEX *lc, ConfigFile *inifile, ini_items *item)
{
   alist *list;
   if (!lc) {
      /*
       * TODO, write back the alist to edit buffer
       */
      return true;
   }
   if (lex_get_token(lc, BCT_STRING) == BCT_ERROR) {
      return false;
   }

   if (item->val.alistval == NULL) {
      list = New(alist(10, owned_by_alist));
   } else {
      list = item->val.alistval;
   }

   Dmsg4(900, "Append %s to alist %p size=%d %s\n",
         lc->str, list, list->size(), item->name);
   list->append(bstrdup(lc->str));
   item->val.alistval = list;

   scan_to_eol(lc);
   return true;
}

static bool ini_store_int64(LEX *lc, ConfigFile *inifile, ini_items *item)
{
   if (!lc) {
      Mmsg(inifile->edit, "%lld", item->val.int64val);
      return true;
   }
   if (lex_get_token(lc, BCT_INT64) == BCT_ERROR) {
      return false;
   }
   item->val.int64val = lc->u.int64_val;
   scan_to_eol(lc);
   return true;
}

static bool ini_store_pint64(LEX *lc, ConfigFile *inifile, ini_items *item)
{
   if (!lc) {
      Mmsg(inifile->edit, "%lld", item->val.int64val);
      return true;
   }
   if (lex_get_token(lc, BCT_PINT64) == BCT_ERROR) {
      return false;
   }
   item->val.int64val = lc->u.pint64_val;
   scan_to_eol(lc);
   return true;
}

static bool ini_store_pint32(LEX *lc, ConfigFile *inifile, ini_items *item)
{
   if (!lc) {
      Mmsg(inifile->edit, "%d", item->val.int32val);
      return true;
   }
   if (lex_get_token(lc, BCT_PINT32) == BCT_ERROR) {
      return false;
   }
   item->val.int32val = lc->u.pint32_val;
   scan_to_eol(lc);
   return true;
}

static bool ini_store_int32(LEX *lc, ConfigFile *inifile, ini_items *item)
{
   if (!lc) {
      Mmsg(inifile->edit, "%d", item->val.int32val);
      return true;
   }
   if (lex_get_token(lc, BCT_INT32) == BCT_ERROR) {
      return false;
   }
   item->val.int32val = lc->u.int32_val;
   scan_to_eol(lc);
   return true;
}

static bool ini_store_bool(LEX *lc, ConfigFile *inifile, ini_items *item)
{
   if (!lc) {
      Mmsg(inifile->edit, "%s", item->val.boolval?"yes":"no");
      return true;
   }
   if (lex_get_token(lc, BCT_NAME) == BCT_ERROR) {
      return false;
   }
   if (bstrcasecmp(lc->str, "yes") || bstrcasecmp(lc->str, "true")) {
      item->val.boolval = true;
   } else if (bstrcasecmp(lc->str, "no") || bstrcasecmp(lc->str, "false")) {
      item->val.boolval = false;
   } else {
      /*
       * YES and NO must not be translated
       */
      scan_err2(lc, _("Expect %s, got: %s"), "YES, NO, TRUE, or FALSE", lc->str);
      return false;
   }
   scan_to_eol(lc);
   return true;
}

/*
 * Format a scanner error message
 */
static void s_err(const char *file, int line, LEX *lc, const char *msg, ...)
{
   va_list ap;
   int len, maxlen;
   ConfigFile *ini;
   PoolMem buf(PM_MESSAGE);

   while (1) {
      maxlen = buf.size() - 1;
      va_start(ap, msg);
      len = bvsnprintf(buf.c_str(), maxlen, msg, ap);
      va_end(ap);

      if (len < 0 || len >= (maxlen - 5)) {
         buf.realloc_pm(maxlen + maxlen / 2);
         continue;
      }

      break;
   }

   ini = (ConfigFile *)(lc->caller_ctx);
   if (ini->jcr) {              /* called from core */
      Jmsg(ini->jcr, M_ERROR, 0, _("Config file error: %s\n"
                              "            : Line %d, col %d of file %s\n%s\n"),
         buf.c_str(), lc->line_no, lc->col_no, lc->fname, lc->line);

//   } else if (ini->ctx) {       /* called from plugin */
//      ini->bfuncs->JobMessage(ini->ctx, __FILE__, __LINE__, M_FATAL, 0,
//                    _("Config file error: %s\n"
//                      "            : Line %d, col %d of file %s\n%s\n"),
//                            buf.c_str(), lc->line_no, lc->col_no, lc->fname, lc->line);
//
   } else {                     /* called from ??? */
      e_msg(file, line, M_ERROR, 0,
            _("Config file error: %s\n"
              "            : Line %d, col %d of file %s\n%s\n"),
            buf.c_str(), lc->line_no, lc->col_no, lc->fname, lc->line);
   }
}

/*
 * Format a scanner error message
 */
static void s_warn(const char *file, int line, LEX *lc, const char *msg, ...)
{
   va_list ap;
   int len, maxlen;
   ConfigFile *ini;
   PoolMem buf(PM_MESSAGE);

   while (1) {
      maxlen = buf.size() - 1;
      va_start(ap, msg);
      len = bvsnprintf(buf.c_str(), maxlen, msg, ap);
      va_end(ap);

      if (len < 0 || len >= (maxlen - 5)) {
         buf.realloc_pm(maxlen + maxlen / 2);
         continue;
      }

      break;
   }

   ini = (ConfigFile *)(lc->caller_ctx);
   if (ini->jcr) {              /* called from core */
      Jmsg(ini->jcr, M_WARNING, 0, _("Config file warning: %s\n"
                              "            : Line %d, col %d of file %s\n%s\n"),
         buf.c_str(), lc->line_no, lc->col_no, lc->fname, lc->line);

//   } else if (ini->ctx) {       /* called from plugin */
//      ini->bfuncs->JobMessage(ini->ctx, __FILE__, __LINE__, M_WARNING, 0,
//                    _("Config file warning: %s\n"
//                      "            : Line %d, col %d of file %s\n%s\n"),
//                            buf.c_str(), lc->line_no, lc->col_no, lc->fname, lc->line);
//
   } else {                     /* called from ??? */
      p_msg(file, line, 0,
            _("Config file warning: %s\n"
              "            : Line %d, col %d of file %s\n%s\n"),
            buf.c_str(), lc->line_no, lc->col_no, lc->fname, lc->line);
   }
}

/*
 * Reset free items
 */
void ConfigFile::clear_items()
{
   if (!items) {
      return;
   }

   for (int i = 0; items[i].name; i++) {
      if (items[i].found) {
         /*
          * Special members require delete or free
          */
         switch (items[i].type) {
         case INI_CFG_TYPE_STR:
            free(items[i].val.strval);
            items[i].val.strval = NULL;
            break;
         case INI_CFG_TYPE_ALIST_STR:
            delete items[i].val.alistval;
            items[i].val.alistval = NULL;
            break;
         default:
            break;
         }
         items[i].found = false;
      }
   }
}

void ConfigFile::free_items()
{
   if (items_allocated) {
      for (int i = 0; items[i].name; i++) {
         bfree_and_null_const(items[i].name);
         bfree_and_null_const(items[i].comment);
      }
      free(items);
   }
   items = NULL;
   items_allocated = false;
}

/*
 * Get a particular item from the items list
 */
int ConfigFile::get_item(const char *name)
{
   if (!items) {
      return -1;
   }

   for (int i = 0; i < MAX_INI_ITEMS && items[i].name; i++) {
      if (bstrcasecmp(name, items[i].name)) {
         return i;
      }
   }
   return -1;
}

/*
 * Dump a buffer to a file in the working directory
 * Needed to unserialise() a config
 */
bool ConfigFile::dump_string(const char *buf, int32_t len)
{
   FILE *fp;
   bool ret = false;

   if (!out_fname) {
      out_fname = get_pool_memory(PM_FNAME);
      make_unique_filename(out_fname, (int)(intptr_t)this, (char*)"configfile");
   }

   fp = fopen(out_fname, "wb");
   if (!fp) {
      return ret;
   }

   if (fwrite(buf, len, 1, fp) == 1) {
      ret = true;
   }

   fclose(fp);
   return ret;
}

/*
 * Dump the item table format to a text file (used by plugin)
 */
bool ConfigFile::serialize(const char *fname)
{
   FILE *fp;
   int32_t len;
   bool ret = false;
   PoolMem tmp(PM_MESSAGE);

   if (!items) {
      return ret;
   }

   fp = fopen(fname, "w");
   if (!fp) {
      return ret;
   }

   len = serialize(&tmp);
   if (fwrite(tmp.c_str(), len, 1, fp) == 1) {
      ret = true;
   }

   fclose(fp);
   return ret;
}

/*
 * Dump the item table format to a text file (used by plugin)
 */
int ConfigFile::serialize(PoolMem *buf)
{
   int len;
   PoolMem tmp(PM_MESSAGE);

   if (!items) {
      char *p;

      p = buf->c_str();
      p[0] = '\0';
      return 0;
   }

   len = Mmsg(buf, "# Plugin configuration file\n# Version %d\n", version);
   for (int i = 0; items[i].name; i++) {
      if (items[i].comment) {
         Mmsg(tmp, "OptPrompt=%s\n", items[i].comment);
         pm_strcat(buf, tmp.c_str());
      }
      if (items[i].default_value) {
         Mmsg(tmp, "OptDefault=%s\n", items[i].default_value);
         pm_strcat(buf, tmp.c_str());
      }
      if (items[i].required) {
         Mmsg(tmp, "OptRequired=yes\n");
         pm_strcat(buf, tmp.c_str());
      }

      /* variable = @INT64@ */
      Mmsg(tmp, "%s=%s\n\n",
           items[i].name, ini_get_store_code(items[i].type));
      len = pm_strcat(buf, tmp.c_str());
   }

   return len ;
}

/*
 * Dump the item table content to a text file (used by director)
 */
int ConfigFile::dump_results(PoolMem *buf)
{
   int len;
   PoolMem tmp(PM_MESSAGE);

   if (!items) {
      char *p;

      p = buf->c_str();
      p[0] = '\0';
      return 0;
   }
   len = Mmsg(buf, "# Plugin configuration file\n# Version %d\n", version);

   for (int i = 0; items[i].name; i++) {
      if (items[i].found) {
         switch (items[i].type) {
         case INI_CFG_TYPE_INT32:
            ini_store_int32(NULL, this, &items[i]);
            break;
         case INI_CFG_TYPE_PINT32:
            ini_store_pint32(NULL, this, &items[i]);
            break;
         case INI_CFG_TYPE_INT64:
            ini_store_int64(NULL, this, &items[i]);
            break;
         case INI_CFG_TYPE_PINT64:
            ini_store_pint64(NULL, this, &items[i]);
            break;
         case INI_CFG_TYPE_NAME:
            ini_store_name(NULL, this, &items[i]);
            break;
         case INI_CFG_TYPE_STR:
            ini_store_str(NULL, this, &items[i]);
            break;
         case INI_CFG_TYPE_BOOL:
            ini_store_bool(NULL, this, &items[i]);
            break;
         case INI_CFG_TYPE_ALIST_STR:
            ini_store_alist_str(NULL, this, &items[i]);
            break;
         default:
            break;
         }
         if (items[i].comment && *items[i].comment) {
            Mmsg(tmp, "# %s\n", items[i].comment);
            pm_strcat(buf, tmp.c_str());
         }
         Mmsg(tmp, "%s=%s\n\n", items[i].name, this->edit);
         len = pm_strcat(buf, tmp.c_str());
      }
   }

   return len ;
}

/*
 * Parse a config file used by Plugin/Director
 */
bool ConfigFile::parse(const char *fname)
{
   int token, i;
   bool ret = false;

   if (!items) {
      return false;
   }

   if ((lc = lex_open_file(lc, fname, s_err, s_warn)) == NULL) {
      berrno be;
      Emsg2(M_ERROR, 0, _("Cannot open config file %s: %s\n"),
            fname, be.bstrerror());
      return false;
   }
   lc->options |= LOPT_NO_EXTERN;
   lc->caller_ctx = (void *)this;

   while ((token=lex_get_token(lc, BCT_ALL)) != BCT_EOF) {
      Dmsg1(dbglevel, "parse got token=%s\n", lex_tok_to_str(token));
      if (token == BCT_EOL) {
         continue;
      }
      for (i = 0; items[i].name; i++) {
         if (bstrcasecmp(items[i].name, lc->str)) {
            if ((token = lex_get_token(lc, BCT_EQUALS)) == BCT_ERROR) {
               Dmsg1(dbglevel, "in BCT_IDENT got token=%s\n",
                     lex_tok_to_str(token));
               break;
            }

            Dmsg1(dbglevel, "calling handler for %s\n", items[i].name);

            /*
             * Call item handler
             */
            switch (items[i].type) {
            case INI_CFG_TYPE_INT32:
               ret = ini_store_int32(lc, this, &items[i]);
               break;
            case INI_CFG_TYPE_PINT32:
               ret = ini_store_pint32(lc, this, &items[i]);
               break;
            case INI_CFG_TYPE_INT64:
               ret = ini_store_int64(lc, this, &items[i]);
               break;
            case INI_CFG_TYPE_PINT64:
               ret = ini_store_pint64(lc, this, &items[i]);
               break;
            case INI_CFG_TYPE_NAME:
               ret = ini_store_name(lc, this, &items[i]);
               break;
            case INI_CFG_TYPE_STR:
               ret = ini_store_str(lc, this, &items[i]);
               break;
            case INI_CFG_TYPE_BOOL:
               ret = ini_store_bool(lc, this, &items[i]);
               break;
            case INI_CFG_TYPE_ALIST_STR:
               ret = ini_store_alist_str(lc, this, &items[i]);
               break;
            default:
               break;
            }
            i = -1;
            break;
         }
      }
      if (i >= 0) {
         Dmsg1(dbglevel, "Keyword = %s\n", lc->str);
         scan_err1(lc, "Keyword %s not found", lc->str);
         /*
          * We can raise an error here
          */
         break;
      }
      if (!ret) {
         break;
      }
   }

   for (i = 0; items[i].name; i++) {
      if (items[i].required && !items[i].found) {
         scan_err1(lc, "%s required but not found", items[i].name);
         ret = false;
      }
   }

   lc = lex_close_file(lc);

   return ret;
}

/*
 * Analyse the content of a ini file to build the item list
 * It uses special syntax for datatype. Used by Director on Restore object
 *
 * OptPrompt = "Variable1"
 * OptRequired
 * OptDefault = 100
 * Variable1 = @PINT32@
 * ...
 */
bool ConfigFile::unserialize(const char *fname)
{
   int token, i, nb = 0;
   bool ret = false;
   const char **assign;

   /*
    * At this time, we allow only 32 different items
    */
   int s = MAX_INI_ITEMS * sizeof (struct ini_items);

   items = (struct ini_items *) malloc (s);
   memset(items, 0, s);
   items_allocated = true;

   /*
    * Parse the file and generate the items structure on the fly
    */
   if ((lc = lex_open_file(lc, fname, s_err, s_warn)) == NULL) {
      berrno be;
      Emsg2(M_ERROR, 0, _("Cannot open config file %s: %s\n"),
            fname, be.bstrerror());
      return false;
   }
   lc->options |= LOPT_NO_EXTERN;
   lc->caller_ctx = (void *)this;

   while ((token=lex_get_token(lc, BCT_ALL)) != BCT_EOF) {
      Dmsg1(dbglevel, "parse got token=%s\n", lex_tok_to_str(token));

      if (token == BCT_EOL) {
         continue;
      }

      ret = false;
      assign = NULL;

      if (nb >= MAX_INI_ITEMS) {
         break;
      }

      if (bstrcasecmp("optprompt", lc->str)) {
         assign = &(items[nb].comment);
      } else if (bstrcasecmp("optdefault", lc->str)) {
         assign = &(items[nb].default_value);
      } else if (bstrcasecmp("optrequired", lc->str)) {
         items[nb].required = true;               /* Don't use argument */
         scan_to_eol(lc);
         continue;
      } else {
         items[nb].name = bstrdup(lc->str);
      }

      token = lex_get_token(lc, BCT_ALL);
      Dmsg1(dbglevel, "in BCT_IDENT got token=%s\n", lex_tok_to_str(token));

      if (token != BCT_EQUALS) {
         scan_err1(lc, "expected an equals, got: %s", lc->str);
         break;
      }

      /*
       * We may allow blank variable
       */
      if (lex_get_token(lc, BCT_STRING) == BCT_ERROR) {
         break;
      }

      if (assign) {
         *assign = bstrdup(lc->str);

      } else {
         if ((items[nb].type = ini_get_store_type(lc->str)) == 0) {
            scan_err1(lc, "expected a data type, got: %s", lc->str);
            break;
         }
         nb++;
      }
      scan_to_eol(lc);
      ret = true;
   }

   if (!ret) {
      for (i = 0; i < nb; i++) {
         bfree_and_null_const(items[i].name);
         bfree_and_null_const(items[i].comment);
         bfree_and_null_const(items[i].default_value);
         items[i].type = 0;
         items[i].required = false;
      }
   }

   lc = lex_close_file(lc);
   return ret;
}
