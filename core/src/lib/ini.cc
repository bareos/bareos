/*
   Copyright (C) 2011-2011 Bacula Systems(R) SA
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2022 Bareos GmbH & Co. KG

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

#include "include/bareos.h"
#include "ini.h"
#include "lib/berrno.h"
#include "lib/alist.h"

#define bfree_and_null_const(a) \
  do {                          \
    if (a) {                    \
      free((void*)a);           \
      (a) = NULL;               \
    }                           \
  } while (0)
static int dbglevel = 100;

// We use this structure to associate a key to the function
struct ini_store {
  const char* key;
  const char* comment;
  int type;
};

static struct ini_store funcs[]
    = {{"@INT32@", "Integer", INI_CFG_TYPE_INT32},
       {"@PINT32@", "Integer", INI_CFG_TYPE_PINT32},
       {"@INT64@", "Integer", INI_CFG_TYPE_INT64},
       {"@PINT64@", "Positive Integer", INI_CFG_TYPE_PINT64},
       {"@NAME@", "Simple String", INI_CFG_TYPE_NAME},
       {"@STR@", "String", INI_CFG_TYPE_STR},
       {"@BOOL@", "on/off", INI_CFG_TYPE_BOOL},
       {"@ALIST@", "String list", INI_CFG_TYPE_ALIST_STR},
       {NULL, NULL, 0}};

// Get handler code from storage type.
const char* ini_get_store_code(int type)
{
  int i;

  for (i = 0; funcs[i].key; i++) {
    if (funcs[i].type == type) { return funcs[i].key; }
  }
  return NULL;
}

// Get storage type from handler name.
int IniGetStoreType(const char* key)
{
  int i;

  for (i = 0; funcs[i].key; i++) {
    if (!strcmp(funcs[i].key, key)) { return funcs[i].type; }
  }
  return 0;
}

/* ----------------------------------------------------------------
 * Handle data type. Import/Export
 * ----------------------------------------------------------------
 */
static bool IniStoreStr(LEX* lc, ConfigFile* inifile, ini_items* item)
{
  if (!lc) {
    Mmsg(inifile->edit, "%s", item->val.strval);
    return true;
  }
  if (LexGetToken(lc, BCT_STRING) == BCT_ERROR) { return false; }
  // If already allocated, free first
  if (item->found && item->val.strval) { free(item->val.strval); }
  item->val.strval = strdup(lc->str);
  ScanToEol(lc);
  return true;
}

static bool IniStoreName(LEX* lc, ConfigFile* inifile, ini_items* item)
{
  if (!lc) {
    Mmsg(inifile->edit, "%s", item->val.nameval);
    return true;
  }
  if (LexGetToken(lc, BCT_NAME) == BCT_ERROR) { return false; }
  bstrncpy(item->val.nameval, lc->str, sizeof(item->val.nameval));
  ScanToEol(lc);
  return true;
}

static bool IniStoreAlistStr(LEX* lc, ConfigFile*, ini_items* item)
{
  alist<char*>* list;
  if (!lc) {
    // TODO, write back the alist to edit buffer
    return true;
  }
  if (LexGetToken(lc, BCT_STRING) == BCT_ERROR) { return false; }

  if (item->val.alistval == NULL) {
    list = new alist<char*>(10, owned_by_alist);
  } else {
    list = item->val.alistval;
  }

  Dmsg4(900, "Append %s to alist %p size=%d %s\n", lc->str, list, list->size(),
        item->name);
  list->append(strdup(lc->str));
  item->val.alistval = list;

  ScanToEol(lc);
  return true;
}

static bool ini_store_int64(LEX* lc, ConfigFile* inifile, ini_items* item)
{
  if (!lc) {
    Mmsg(inifile->edit, "%lld", item->val.int64val);
    return true;
  }
  if (LexGetToken(lc, BCT_INT64) == BCT_ERROR) { return false; }
  item->val.int64val = lc->u.int64_val;
  ScanToEol(lc);
  return true;
}

static bool ini_store_pint64(LEX* lc, ConfigFile* inifile, ini_items* item)
{
  if (!lc) {
    Mmsg(inifile->edit, "%lld", item->val.int64val);
    return true;
  }
  if (LexGetToken(lc, BCT_PINT64) == BCT_ERROR) { return false; }
  item->val.int64val = lc->u.pint64_val;
  ScanToEol(lc);
  return true;
}

static bool ini_store_pint32(LEX* lc, ConfigFile* inifile, ini_items* item)
{
  if (!lc) {
    Mmsg(inifile->edit, "%d", item->val.int32val);
    return true;
  }
  if (LexGetToken(lc, BCT_PINT32) == BCT_ERROR) { return false; }
  item->val.int32val = lc->u.pint32_val;
  ScanToEol(lc);
  return true;
}

static bool ini_store_int32(LEX* lc, ConfigFile* inifile, ini_items* item)
{
  if (!lc) {
    Mmsg(inifile->edit, "%d", item->val.int32val);
    return true;
  }
  if (LexGetToken(lc, BCT_INT32) == BCT_ERROR) { return false; }
  item->val.int32val = lc->u.int32_val;
  ScanToEol(lc);
  return true;
}

static bool IniStoreBool(LEX* lc, ConfigFile* inifile, ini_items* item)
{
  if (!lc) {
    Mmsg(inifile->edit, "%s", item->val.boolval ? "yes" : "no");
    return true;
  }
  if (LexGetToken(lc, BCT_NAME) == BCT_ERROR) { return false; }
  if (Bstrcasecmp(lc->str, "yes") || Bstrcasecmp(lc->str, "true")) {
    item->val.boolval = true;
  } else if (Bstrcasecmp(lc->str, "no") || Bstrcasecmp(lc->str, "false")) {
    item->val.boolval = false;
  } else {
    // YES and NO must not be translated
    scan_err2(lc, T_("Expect %s, got: %s"), "YES, NO, TRUE, or FALSE", lc->str);
    return false;
  }
  ScanToEol(lc);
  return true;
}

// Format a scanner error message
static void s_err(const char* file, int line, LEX* lc, const char* msg, ...)
{
  va_list ap;
  int len, maxlen;
  ConfigFile* ini;
  PoolMem buf(PM_MESSAGE);

  while (1) {
    maxlen = buf.size() - 1;
    va_start(ap, msg);
    len = Bvsnprintf(buf.c_str(), maxlen, msg, ap);
    va_end(ap);

    if (len < 0 || len >= (maxlen - 5)) {
      buf.ReallocPm(maxlen + maxlen / 2);
      continue;
    }

    break;
  }

  ini = (ConfigFile*)(lc->caller_ctx);
  if (ini->jcr) { /* called from core */
    Jmsg(ini->jcr, M_ERROR, 0,
         T_("Config file error: %s\n"
           "            : Line %d, col %d of file %s\n%s\n"),
         buf.c_str(), lc->line_no, lc->col_no, lc->fname, lc->line);

    //   } else if (ini->ctx) {       /* called from plugin */
    //      ini->bareos_core_functions->JobMessage(ini->ctx, __FILE__, __LINE__,
    //      M_FATAL, 0,
    //                    T_("Config file error: %s\n"
    //                      "            : Line %d, col %d of file %s\n%s\n"),
    //                            buf.c_str(), lc->line_no, lc->col_no,
    //                            lc->fname, lc->line);
    //
  } else { /* called from ??? */
    e_msg(file, line, M_ERROR, 0,
          T_("Config file error: %s\n"
            "            : Line %d, col %d of file %s\n%s\n"),
          buf.c_str(), lc->line_no, lc->col_no, lc->fname, lc->line);
  }
}

// Format a scanner error message
static void s_warn(const char* file, int line, LEX* lc, const char* msg, ...)
{
  va_list ap;
  int len, maxlen;
  ConfigFile* ini;
  PoolMem buf(PM_MESSAGE);

  while (1) {
    maxlen = buf.size() - 1;
    va_start(ap, msg);
    len = Bvsnprintf(buf.c_str(), maxlen, msg, ap);
    va_end(ap);

    if (len < 0 || len >= (maxlen - 5)) {
      buf.ReallocPm(maxlen + maxlen / 2);
      continue;
    }

    break;
  }

  ini = (ConfigFile*)(lc->caller_ctx);
  if (ini->jcr) { /* called from core */
    Jmsg(ini->jcr, M_WARNING, 0,
         T_("Config file warning: %s\n"
           "            : Line %d, col %d of file %s\n%s\n"),
         buf.c_str(), lc->line_no, lc->col_no, lc->fname, lc->line);

    //   } else if (ini->ctx) {       /* called from plugin */
    //      ini->bareos_core_functions->JobMessage(ini->ctx, __FILE__, __LINE__,
    //      M_WARNING, 0,
    //                    T_("Config file warning: %s\n"
    //                      "            : Line %d, col %d of file %s\n%s\n"),
    //                            buf.c_str(), lc->line_no, lc->col_no,
    //                            lc->fname, lc->line);
    //
  } else { /* called from ??? */
    p_msg(file, line, 0,
          T_("Config file warning: %s\n"
            "            : Line %d, col %d of file %s\n%s\n"),
          buf.c_str(), lc->line_no, lc->col_no, lc->fname, lc->line);
  }
}

// Reset free items
void ConfigFile::ClearItems()
{
  if (!items) { return; }

  for (int i = 0; items[i].name; i++) {
    if (items[i].found) {
      // Special members require delete or free
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

void ConfigFile::FreeItems()
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

// Get a particular item from the items list
int ConfigFile::GetItem(const char* name)
{
  if (!items) { return -1; }

  for (int i = 0; i < MAX_INI_ITEMS && items[i].name; i++) {
    if (Bstrcasecmp(name, items[i].name)) { return i; }
  }
  return -1;
}

/*
 * Dump a buffer to a file in the working directory
 * Needed to unserialise() a config
 */
bool ConfigFile::DumpString(const char* buf, int32_t len)
{
  FILE* fp;
  bool ret = false;

  if (!out_fname) {
    out_fname = GetPoolMemory(PM_FNAME);
    MakeUniqueFilename(out_fname, (int)(intptr_t)this, (char*)"configfile");
  }

  fp = fopen(out_fname, "wb");
  if (!fp) { return ret; }

  if (fwrite(buf, len, 1, fp) == 1) { ret = true; }

  fclose(fp);
  return ret;
}

// Dump the item table format to a text file (used by plugin)
bool ConfigFile::Serialize(const char* fname)
{
  FILE* fp;
  int32_t len;
  bool ret = false;
  PoolMem tmp(PM_MESSAGE);

  if (!items) { return ret; }

  fp = fopen(fname, "w");
  if (!fp) { return ret; }

  len = Serialize(&tmp);
  if (fwrite(tmp.c_str(), len, 1, fp) == 1) { ret = true; }

  fclose(fp);
  return ret;
}

// Dump the item table format to a text file (used by plugin)
int ConfigFile::Serialize(PoolMem* buf)
{
  int len;
  PoolMem tmp(PM_MESSAGE);

  if (!items) {
    char* p;

    p = buf->c_str();
    p[0] = '\0';
    return 0;
  }

  len = Mmsg(buf, "# Plugin configuration file\n# Version %d\n", version);
  for (int i = 0; items[i].name; i++) {
    if (items[i].comment) {
      Mmsg(tmp, "OptPrompt=%s\n", items[i].comment);
      PmStrcat(buf, tmp.c_str());
    }
    if (items[i].default_value) {
      Mmsg(tmp, "OptDefault=%s\n", items[i].default_value);
      PmStrcat(buf, tmp.c_str());
    }
    if (items[i].required) {
      Mmsg(tmp, "OptRequired=yes\n");
      PmStrcat(buf, tmp.c_str());
    }

    /* variable = @INT64@ */
    Mmsg(tmp, "%s=%s\n\n", items[i].name, ini_get_store_code(items[i].type));
    len = PmStrcat(buf, tmp.c_str());
  }

  return len;
}

// Dump the item table content to a text file (used by director)
int ConfigFile::DumpResults(PoolMem* buf)
{
  int len;
  PoolMem tmp(PM_MESSAGE);

  if (!items) {
    char* p;

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
          IniStoreName(NULL, this, &items[i]);
          break;
        case INI_CFG_TYPE_STR:
          IniStoreStr(NULL, this, &items[i]);
          break;
        case INI_CFG_TYPE_BOOL:
          IniStoreBool(NULL, this, &items[i]);
          break;
        case INI_CFG_TYPE_ALIST_STR:
          IniStoreAlistStr(NULL, this, &items[i]);
          break;
        default:
          break;
      }
      if (items[i].comment && *items[i].comment) {
        Mmsg(tmp, "# %s\n", items[i].comment);
        PmStrcat(buf, tmp.c_str());
      }
      Mmsg(tmp, "%s=%s\n\n", items[i].name, this->edit);
      len = PmStrcat(buf, tmp.c_str());
    }
  }

  return len;
}

// Parse a config file used by Plugin/Director
bool ConfigFile::parse(const char* fname)
{
  int token, i;
  bool ret = false;

  if (!items) { return false; }

  if ((lc = lex_open_file(lc, fname, s_err, s_warn)) == NULL) {
    BErrNo be;
    Emsg2(M_ERROR, 0, T_("Cannot open config file %s: %s\n"), fname,
          be.bstrerror());
    return false;
  }
  lc->options |= LOPT_NO_EXTERN;
  lc->caller_ctx = (void*)this;

  while ((token = LexGetToken(lc, BCT_ALL)) != BCT_EOF) {
    Dmsg1(dbglevel, "parse got token=%s\n", lex_tok_to_str(token));
    if (token == BCT_EOL) { continue; }
    for (i = 0; items[i].name; i++) {
      if (Bstrcasecmp(items[i].name, lc->str)) {
        if ((token = LexGetToken(lc, BCT_EQUALS)) == BCT_ERROR) {
          Dmsg1(dbglevel, "in BCT_IDENT got token=%s\n", lex_tok_to_str(token));
          break;
        }

        Dmsg1(dbglevel, "calling handler for %s\n", items[i].name);

        // Call item handler
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
            ret = IniStoreName(lc, this, &items[i]);
            break;
          case INI_CFG_TYPE_STR:
            ret = IniStoreStr(lc, this, &items[i]);
            break;
          case INI_CFG_TYPE_BOOL:
            ret = IniStoreBool(lc, this, &items[i]);
            break;
          case INI_CFG_TYPE_ALIST_STR:
            ret = IniStoreAlistStr(lc, this, &items[i]);
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
      // We can raise an error here
      break;
    }
    if (!ret) { break; }
  }

  for (i = 0; items[i].name; i++) {
    if (items[i].required && !items[i].found) {
      scan_err1(lc, "%s required but not found", items[i].name);
      ret = false;
    }
  }

  lc = LexCloseFile(lc);

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
bool ConfigFile::UnSerialize(const char* fname)
{
  int token, i, nb = 0;
  bool ret = false;
  const char** assign;

  // At this time, we allow only 32 different items
  int s = MAX_INI_ITEMS * sizeof(struct ini_items);

  items = (struct ini_items*)malloc(s);
  memset(items, 0, s);
  items_allocated = true;

  // Parse the file and generate the items structure on the fly
  if ((lc = lex_open_file(lc, fname, s_err, s_warn)) == NULL) {
    BErrNo be;
    Emsg2(M_ERROR, 0, T_("Cannot open config file %s: %s\n"), fname,
          be.bstrerror());
    return false;
  }
  lc->options |= LOPT_NO_EXTERN;
  lc->caller_ctx = (void*)this;

  while ((token = LexGetToken(lc, BCT_ALL)) != BCT_EOF) {
    Dmsg1(dbglevel, "parse got token=%s\n", lex_tok_to_str(token));

    if (token == BCT_EOL) { continue; }

    ret = false;
    assign = NULL;

    if (nb >= MAX_INI_ITEMS) { break; }

    if (Bstrcasecmp("optprompt", lc->str)) {
      assign = &(items[nb].comment);
    } else if (Bstrcasecmp("optdefault", lc->str)) {
      assign = &(items[nb].default_value);
    } else if (Bstrcasecmp("optrequired", lc->str)) {
      items[nb].required = true; /* Don't use argument */
      ScanToEol(lc);
      continue;
    } else {
      items[nb].name = strdup(lc->str);
    }

    token = LexGetToken(lc, BCT_ALL);
    Dmsg1(dbglevel, "in BCT_IDENT got token=%s\n", lex_tok_to_str(token));

    if (token != BCT_EQUALS) {
      scan_err1(lc, "expected an equals, got: %s", lc->str);
      break;
    }

    // We may allow blank variable
    if (LexGetToken(lc, BCT_STRING) == BCT_ERROR) { break; }

    if (assign) {
      *assign = strdup(lc->str);

    } else {
      if ((items[nb].type = IniGetStoreType(lc->str)) == 0) {
        scan_err1(lc, "expected a data type, got: %s", lc->str);
        break;
      }
      nb++;
    }
    ScanToEol(lc);
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

  lc = LexCloseFile(lc);
  return ret;
}
