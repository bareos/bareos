/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2007-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2013 Bareos GmbH & Co. KG

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
/*
 * Common plugin definitions
 *
 * Kern Sibbald, October 2007
 */
#ifndef __PLUGINS_H
#define __PLUGINS_H

/****************************************************************************
 *                                                                          *
 *                Common definitions for all plugins                        *
 *                                                                          *
 ****************************************************************************/

/*
 * Universal return codes from all plugin functions
 */
typedef enum {
   bRC_OK     = 0,                       /* OK */
   bRC_Stop   = 1,                       /* Stop calling other plugins */
   bRC_Error  = 2,                       /* Some kind of error */
   bRC_More   = 3,                       /* More files to backup */
   bRC_Term   = 4,                       /* Unload me */
   bRC_Seen   = 5,                       /* Return code from checkFiles */
   bRC_Core   = 6,                       /* Let BAREOS core handles this file */
   bRC_Skip   = 7,                       /* Skip the proposed file */
   bRC_Cancel = 8,                       /* Job cancelled */

   bRC_Max    = 9999                     /* Max code BAREOS can use */
} bRC;

#define LOWEST_PLUGIN_INSTANCE 0
#define HIGHEST_PLUGIN_INSTANCE 127

extern "C" {
   typedef bRC (*t_loadPlugin)(void *binfo, void *bfuncs, void **pinfo, void **pfuncs);
   typedef bRC (*t_unloadPlugin)(void);
}

class Plugin {
public:
   char *file;
   int32_t file_len;
   t_unloadPlugin unloadPlugin;
   void *pinfo;
   void *pfuncs;
   void *pHandle;
};

/*
 * Context packet as first argument of all functions
 */
struct bpContext {
   uint32_t instance;
   Plugin *plugin;
   void *bContext;                       /* BAREOS private context */
   void *pContext;                       /* Plugin private context */
};

typedef struct gen_pluginInfo {
   uint32_t size;
   uint32_t version;
   const char *plugin_magic;
   const char *plugin_license;
   const char *plugin_author;
   const char *plugin_date;
   const char *plugin_version;
   const char *plugin_description;
   const char *plugin_usage;
} genpInfo;

/* Functions */
bool load_plugins(void *binfo, void *bfuncs, alist *plugin_list,
                  const char *plugin_dir, alist *plugin_names,
                  const char *type, bool is_plugin_compatible(Plugin *plugin));
void unload_plugins(alist *plugin_list);
void unload_plugin(alist *plugin_list, Plugin *plugin, int index);
int list_plugins(alist *plugin_list, POOL_MEM &msg);

/* Each daemon can register a debug hook that will be called
 * after a fatal signal
 */
typedef void (dbg_plugin_hook_t)(Plugin *plug, FILE *fp);
void dbg_plugin_add_hook(dbg_plugin_hook_t *fct);
typedef void(dbg_print_plugin_hook_t)(FILE *fp);
void dbg_print_plugin_add_hook(dbg_print_plugin_hook_t *fct);
void dump_plugins(alist *plugin_list, FILE *fp);

#endif /* __PLUGINS_H */
