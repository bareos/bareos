/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2011-2011 Bacula Systems(R) SA

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

#ifndef BAREOS_LIB_INI_H_
#define BAREOS_LIB_INI_H_

/*
 * Standard global types with handlers defined in ini.c
 */
enum
{
  INI_CFG_TYPE_INT32 = 1,    /* 32 bits Integer */
  INI_CFG_TYPE_PINT32 = 2,   /* Positive 32 bits Integer (unsigned) */
  INI_CFG_TYPE_INT64 = 3,    /* 64 bits Integer */
  INI_CFG_TYPE_PINT64 = 4,   /* Positive 64 bits Integer (unsigned) */
  INI_CFG_TYPE_NAME = 5,     /* Name */
  INI_CFG_TYPE_STR = 6,      /* String */
  INI_CFG_TYPE_BOOL = 7,     /* Boolean */
  INI_CFG_TYPE_ALIST_STR = 8 /* List of strings */
};

/*
 * Plugin has a internal C structure that describes the configuration:
 * struct ini_items[]
 *
 * The ConfigFile object can generate a text file that describes the C
 * structure. This text format is saved as RestoreObject in the catalog.
 *
 *   struct ini_items[]  -> RegisterItems()  -> Serialize() -> RestoreObject R1
 *
 * On the Director side, at the restore time, we can analyse this text to
 * get the C structure.
 *
 * RestoreObject R1 -> write to disk -> UnSerialize() -> struct ini_items[]
 *
 * Once done, we can ask questions to the user at the restore time and fill
 * the C struct with answers. The Director can send back as a RestoreObject
 * the result of the questionnaire.
 *
 * struct ini_items[] -> UaContext -> dump_result() -> FD as RestoreObject R2
 *
 * On the Plugin side, it can get back the C structure and use it.
 * RestoreObject R2 -> parse() -> struct ini_items[]
 */

class ConfigFile;
struct ini_items;

/*
 * Used to store result
 */
typedef union {
  char* strval;
  char nameval[MAX_NAME_LENGTH];
  int64_t int64val;
  int32_t int32val;
  alist* alistval;
  bool boolval;
} item_value;

/*
 * If no items are registred at the scan time, we detect this list from
 * the file itself
 */
struct ini_items {
  const char* name;    /* keyword name */
  int type;            /* type accepted */
  const char* comment; /* comment associated, used in prompt */

  int required;              /* optional required or not */
  const char* re_value;      /* optional regexp associated */
  const char* in_values;     /* optional list of values */
  const char* default_value; /* optional default value */

  bool found;     /* if val is set */
  item_value val; /* val contains the value */
};

/*
 * When reading a ini file, we limit the number of items that we can create
 */
#define MAX_INI_ITEMS 32

/*
 * Special RestoreObject name used to get user input at restore time
 */
#define INI_RESTORE_OBJECT_NAME "RestoreOptions"

/*
 * Can be used to set re_value, in_value, default_value, found and val to 0
 * G++ looks to allow partial declaration, let see with another compiler
 */
#define ITEMS_DEFAULT \
  NULL, NULL, NULL, 0, { 0 }

/*
 * Handle simple configuration file such as "ini" files.
 * key1 = val               # comment
 * OptPrompt=comment
 * key2 = val
 *
 */

class ConfigFile {
 private:
  LEX* lc; /* Lex parser */
  bool items_allocated;

 public:
  JobControlRecord* jcr;   /* JobControlRecord needed for Jmsg */
  int version;             /* Internal version check */
  int sizeof_ini_items;    /* Extra check when using dynamic loading */
  struct ini_items* items; /* Structure of the config file */
  POOLMEM* out_fname;      /* Can be used to dump config to disk */
  POOLMEM* edit;           /* Can be used to build result file */

  ConfigFile()
  {
    lc = NULL;
    jcr = NULL;
    items = NULL;
    out_fname = NULL;

    version = 1;
    items_allocated = false;
    edit = GetPoolMemory(PM_FNAME);
    sizeof_ini_items = sizeof(struct ini_items);
  }

  ~ConfigFile()
  {
    if (lc) { lex_close_file(lc); }
    if (edit) { FreePoolMemory(edit); }
    if (out_fname) {
      unlink(out_fname);
      FreePoolMemory(out_fname);
    }
    FreeItems();
  }

  /*
   * Dump a config string to out_fname
   */
  bool DumpString(const char* buf, int32_t len);

  /*
   * JobControlRecord needed for Jmsg
   */
  void SetJcr(JobControlRecord* ajcr) { jcr = ajcr; }

  /*
   * Free malloced items such as char* or alist or items
   */
  void FreeItems();

  /*
   * Clear items member
   */
  void ClearItems();

  /*
   * Dump the item table to a file (used on plugin side)
   */
  bool Serialize(const char* fname);

  /*
   * Dump the item table format to a buffer (used on plugin side)
   * returns the length of the buffer, -1 if error
   */
  int Serialize(PoolMem* buf);

  /*
   * Dump the item table content to a buffer
   */
  int DumpResults(PoolMem* buf);

  /*
   * Get item position in items list (useful when dynamic)
   */
  int GetItem(const char* name);

  /*
   * Register config file structure, if size doesn't match
   */
  bool RegisterItems(struct ini_items* aitems, int size)
  {
    int i;
    if (sizeof_ini_items == size) {
      for (i = 0; aitems[i].name; i++)
        ;
      items = (struct ini_items*)malloc((i + 1) * size); /* NULL terminated */
      memcpy(items, aitems, (i + 1) * size);
      items_allocated = false; /* we copy only pointers, don't free them */
      return true;
    }
    return false;
  }

  /*
   * Parse a ini file with a item list previously registred (plugin side)
   */
  bool parse(const char* filename);

  /*
   * Create a item list from a ini file (director side)
   */
  bool UnSerialize(const char* filename);
};

/*
 * Get handler code from storage type.
 */
const char* ini_get_store_code(int type);

/*
 * Get storage type from handler name.
 */
int IniGetStoreType(const char* key);

#endif
