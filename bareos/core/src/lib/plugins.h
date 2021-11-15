/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2007-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2021 Bareos GmbH & Co. KG

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation, which is
   listed in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   Affero General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
*/
// Kern Sibbald, October 2007
/**
 * @file
 * Common plugin definitions
 */
#ifndef BAREOS_LIB_PLUGINS_H_
#define BAREOS_LIB_PLUGINS_H_

/****************************************************************************
 *                                                                          *
 *                Common definitions for all plugins                        *
 *                                                                          *
 ****************************************************************************/

// Universal return codes from all plugin functions
typedef enum
{
  PYTHON_UNDEFINED_RETURN_VALUE = -1,
  bRC_OK = 0,     /* OK */
  bRC_Stop = 1,   /* Stop calling other plugins */
  bRC_Error = 2,  /* Some kind of error */
  bRC_More = 3,   /* More files to backup */
  bRC_Term = 4,   /* Unload me */
  bRC_Seen = 5,   /* Return code from checkFiles */
  bRC_Core = 6,   /* Let BAREOS core handles this file */
  bRC_Skip = 7,   /* Skip the proposed file */
  bRC_Cancel = 8, /* Job cancelled */

  bRC_Max = 9999 /* Max code BAREOS can use */
} bRC;

#define LOWEST_PLUGIN_INSTANCE 0
#define HIGHEST_PLUGIN_INSTANCE 127

extern "C" {
typedef bRC (*t_loadPlugin)(void* bareos_plugin_interface_version,
                            void* bareos_core_functions,
                            void** plugin_information,
                            void** plugin_functions);
typedef bRC (*t_unloadPlugin)(void);
}

class Plugin {
 public:
  char* file;
  int32_t file_len;
  t_unloadPlugin unloadPlugin;
  void* plugin_information;
  void* plugin_functions;
  void* plugin_handle;
};

// Context packet as first argument of all functions
struct PluginContext {
  uint32_t instance;
  Plugin* plugin;
  void* core_private_context;   /* BAREOS private context */
  void* plugin_private_context; /* Plugin private context */
};

typedef struct gen_pluginInfo {
  uint32_t size;
  uint32_t version;
  const char* plugin_magic;
  const char* plugin_license;
  const char* plugin_author;
  const char* plugin_date;
  const char* plugin_version;
  const char* plugin_description;
  const char* plugin_usage;
} PluginInformation;

template <typename T> class alist;

/* Functions */
bool LoadPlugins(void* bareos_plugin_interface_version,
                 void* bareos_core_functions,
                 alist<Plugin*>* plugin_list,
                 const char* plugin_dir,
                 alist<const char*>* plugin_names,
                 const char* type,
                 bool IsPluginCompatible(Plugin* plugin));
void UnloadPlugins(alist<Plugin*>* plugin_list);
void UnloadPlugin(alist<Plugin*>* plugin_list, Plugin* plugin, int index);
int ListPlugins(alist<Plugin*>* plugin_list, PoolMem& msg);

/* Each daemon can register a debug hook that will be called
 * after a fatal signal
 */
typedef void(dbg_plugin_hook_t)(Plugin* plug, FILE* fp);
void DbgPluginAddHook(dbg_plugin_hook_t* fct);
typedef void(dbg_print_plugin_hook_t)(FILE* fp);
void DbgPrintPluginAddHook(dbg_print_plugin_hook_t* fct);
void DumpPlugins(alist<Plugin*>* plugin_list, FILE* fp);

#endif  // BAREOS_LIB_PLUGINS_H_
