/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2007-2012 Free Software Foundation Europe e.V.
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
 * Plugin load/unloader for all BAREOS daemons
 *
 * Kern Sibbald, October 2007
 */

#include "include/bareos.h"

#if defined(HAVE_DLFCN_H)
#include <dlfcn.h>
#elif defined(HAVE_SYS_DL_H)
#include <sys/dl.h>
#elif defined(HAVE_DL_H)
#include <dl.h>
#else
#error "Cannot load dynamic objects into program"
#endif

#ifdef HAVE_DIRENT_H
#include <dirent.h>
#define NAMELEN(dirent) (strlen((dirent)->d_name))
#endif
#ifndef HAVE_READDIR_R
int Readdir_r(DIR *dirp, struct dirent *entry, struct dirent **result);
#endif

#ifndef RTLD_NOW
#define RTLD_NOW 2
#endif

#if !defined(LT_LAZY_OR_NOW)
#if defined(RTLD_LAZY)
#define LT_LAZY_OR_NOW RTLD_LAZY
#else
#if defined(DL_LAZY)
#define LT_LAZY_OR_NOW DL_LAZY
#endif
#endif /* !RTLD_LAZY */
#endif
#if !defined(LT_LAZY_OR_NOW)
#if defined(RTLD_NOW)
#define LT_LAZY_OR_NOW RTLD_NOW
#else
#if defined(DL_NOW)
#define LT_LAZY_OR_NOW DL_NOW
#endif
#endif /* !RTLD_NOW */
#endif

#if !defined(LT_LAZY_OR_NOW)
#define LT_LAZY_OR_NOW 0
#endif /* !LT_LAZY_OR_NOW */

#if !defined(LT_GLOBAL)
#if defined(RTLD_GLOBAL)
#define LT_GLOBAL RTLD_GLOBAL
#else
#if defined(DL_GLOBAL)
#define LT_GLOBAL DL_GLOBAL
#endif
#endif /* !RTLD_GLOBAL */
#endif

#include "plugins.h"

static const int debuglevel = 50;

/*
 * Create a new plugin "class" entry and enter it in the list of plugins.
 * Note, this is not the same as an instance of the plugin.
 */
static Plugin *new_plugin()
{
   Plugin *plugin;

   plugin = (Plugin *)malloc(sizeof(Plugin));
   memset(plugin, 0, sizeof(Plugin));
   return plugin;
}

static void ClosePlugin(Plugin *plugin)
{
   if (plugin->file) {
      Dmsg1(50, "Got plugin=%s but not accepted.\n", plugin->file);
   }
   if (plugin->unloadPlugin) {
      plugin->unloadPlugin();
   }
   if (plugin->pHandle) {
      dlclose(plugin->pHandle);
   }
   if (plugin->file) {
      free(plugin->file);
   }
   free(plugin);
}

/*
 * Load a specific plugin and check if the plugin had the correct
 * entry points, the license is compatible and the initialize the plugin.
 */
static bool load_a_plugin(void *binfo,
                          void *bfuncs,
                          const char *plugin_pathname,
                          const char *plugin_name,
                          const char *type,
                          alist *plugin_list,
                          bool IsPluginCompatible(Plugin *plugin))
{
   t_loadPlugin loadPlugin;
   Plugin *plugin = NULL;

   plugin = new_plugin();
   plugin->file = bstrdup(plugin_name);
   plugin->file_len = strstr(plugin->file, type) - plugin->file;

   plugin->pHandle = dlopen(plugin_pathname, LT_LAZY_OR_NOW | LT_GLOBAL);

   if (!plugin->pHandle) {
      const char *error = dlerror();

      Jmsg(NULL, M_ERROR, 0, _("dlopen plugin %s failed: ERR=%s\n"),
           plugin_pathname, NPRT(error));
      Dmsg2(debuglevel, "dlopen plugin %s failed: ERR=%s\n",
            plugin_pathname, NPRT(error));

      ClosePlugin(plugin);

      return false;
   }

   /*
    * Get two global entry points
    */
   loadPlugin = (t_loadPlugin)dlsym(plugin->pHandle, "loadPlugin");
   if (!loadPlugin) {
      Jmsg(NULL, M_ERROR, 0, _("Lookup of loadPlugin in plugin %s failed: ERR=%s\n"),
           plugin_pathname, NPRT(dlerror()));
      Dmsg2(debuglevel, "Lookup of loadPlugin in plugin %s failed: ERR=%s\n",
            plugin_pathname, NPRT(dlerror()));

      ClosePlugin(plugin);

      return false;
   }

   plugin->unloadPlugin = (t_unloadPlugin)dlsym(plugin->pHandle, "unloadPlugin");
   if (!plugin->unloadPlugin) {
      Jmsg(NULL, M_ERROR, 0, _("Lookup of unloadPlugin in plugin %s failed: ERR=%s\n"),
           plugin_pathname, NPRT(dlerror()));
      Dmsg2(debuglevel, "Lookup of unloadPlugin in plugin %s failed: ERR=%s\n",
            plugin_pathname, NPRT(dlerror()));

      ClosePlugin(plugin);

      return false;
   }

   /*
    * Initialize the plugin
    */
   if (loadPlugin(binfo, bfuncs, &plugin->pinfo, &plugin->pfuncs) != bRC_OK) {
      ClosePlugin(plugin);

      return false;
   }

   if (!IsPluginCompatible) {
      Dmsg0(50, "Plugin compatibility pointer not set.\n");
   } else if (!IsPluginCompatible(plugin)) {
      ClosePlugin(plugin);

      return false;
   }

   plugin_list->append(plugin);

   return true;
}

/*
 * Load all the plugins in the specified directory.
 * Or when plugin_names is give it has a list of plugins
 * to load from the specified directory.
 */
bool LoadPlugins(void *binfo,
                  void *bfuncs,
                  alist *plugin_list,
                  const char *plugin_dir,
                  alist *plugin_names,
                  const char *type,
                  bool IsPluginCompatible(Plugin *plugin))
{
   struct stat statp;
   bool found = false;
   PoolMem fname(PM_FNAME);
   bool need_slash = false;
   int len;

   Dmsg0(debuglevel, "LoadPlugins\n");

   len = strlen(plugin_dir);
   if (len > 0) {
      need_slash = !IsPathSeparator(plugin_dir[len - 1]);
   }

   /*
    * See if we are loading certain plugins only or all plugins of a certain type.
    */
   if (plugin_names && plugin_names->size()) {
      char *name = nullptr;
      PoolMem plugin_name(PM_FNAME);

      foreach_alist(name, plugin_names) {
         /*
          * Generate the plugin name e.g. <name>-<daemon>.so
          */
         Mmsg(plugin_name, "%s%s", name, type);

         /*
          * Generate the full pathname to the plugin to load.
          */
         Mmsg(fname, "%s%s%s", plugin_dir, (need_slash) ? "/" : "", plugin_name.c_str());

         /*
          * Make sure the plugin exists and is a regular file.
          */
         if (lstat(fname.c_str(), &statp) != 0 || !S_ISREG(statp.st_mode)) {
            continue;
         }

         /*
          * Try to load the plugin and resolve the wanted symbols.
          */
         if (load_a_plugin(binfo, bfuncs, fname.c_str(), plugin_name.c_str(), type,
                           plugin_list, IsPluginCompatible)) {
            found = true;
         }
      }
   } else {
      int name_max, type_len;
      DIR *dp = NULL;
      struct dirent *result;
#ifdef USE_READDIR_R
      struct dirent *entry = NULL;
#endif
      name_max = pathconf(".", _PC_NAME_MAX);
      if (name_max < 1024) {
         name_max = 1024;
      }

      if (!(dp = opendir(plugin_dir))) {
         BErrNo be;
         Jmsg(NULL, M_ERROR_TERM, 0, _("Failed to open Plugin directory %s: ERR=%s\n"),
               plugin_dir, be.bstrerror());
         Dmsg2(debuglevel, "Failed to open Plugin directory %s: ERR=%s\n",
               plugin_dir, be.bstrerror());
         goto bail_out;
      }

#ifdef USE_READDIR_R
      entry = (struct dirent *)malloc(sizeof(struct dirent) + name_max + 1000);
      while (1) {
         if ((Readdir_r(dp, entry, &result) != 0) || (result == NULL)) {
#else
      while (1) {
         result = readdir(dp);
         if (result == NULL) {
#endif
            if (!found) {
               Jmsg(NULL, M_WARNING, 0, _("Failed to find any plugins in %s\n"), plugin_dir);
               Dmsg1(debuglevel, "Failed to find any plugins in %s\n", plugin_dir);
            }
            break;
         }

         if (bstrcmp(result->d_name, ".") ||
             bstrcmp(result->d_name, "..")) {
            continue;
         }

         len = strlen(result->d_name);
         type_len = strlen(type);
         if (len < type_len + 1 || !bstrcmp(&result->d_name[len - type_len], type)) {
            Dmsg3(debuglevel, "Rejected plugin: want=%s name=%s len=%d\n", type, result->d_name, len);
            continue;
         }
         Dmsg2(debuglevel, "Found plugin: name=%s len=%d\n", result->d_name, len);

         PmStrcpy(fname, plugin_dir);
         if (need_slash) {
            PmStrcat(fname, "/");
         }
         PmStrcat(fname, result->d_name);

         /*
          * Make sure the plugin exists and is a regular file.
          */
         if (lstat(fname.c_str(), &statp) != 0 || !S_ISREG(statp.st_mode)) {
            continue;
         }

         /*
          * Try to load the plugin and resolve the wanted symbols.
          */
         if (load_a_plugin(binfo, bfuncs, fname.c_str(), result->d_name, type,
                           plugin_list, IsPluginCompatible)) {
            found = true;
         }
      }

#ifdef USE_READDIR_R
      if (entry) {
         free(entry);
      }
#endif
      if (dp) {
         closedir(dp);
      }
   }

bail_out:
   return found;
}

/*
 * Unload all the loaded plugins
 */
void UnloadPlugins(alist *plugin_list)
{
   int i;
   Plugin *plugin;

   if (!plugin_list) {
      return;
   }
   foreach_alist_index(i, plugin, plugin_list) {
      /*
       * Shut it down and unload it
       */
      plugin->unloadPlugin();
      dlclose(plugin->pHandle);
      if (plugin->file) {
         free(plugin->file);
      }
      free(plugin);
   }
}

void UnloadPlugin(alist *plugin_list, Plugin *plugin, int index)
{
   /*
    * Shut it down and unload it
    */
   plugin->unloadPlugin();
   dlclose(plugin->pHandle);
   if (plugin->file) {
      free(plugin->file);
   }
   plugin_list->remove(index);
   free(plugin);
}

int ListPlugins(alist *plugin_list, PoolMem &msg)
{
   int i, len = 0;
   Plugin *plugin;

   if (plugin_list && plugin_list->size() > 0) {
      PmStrcpy(msg, "Plugin Info:\n");
      foreach_alist_index(i, plugin, plugin_list) {
         PmStrcat(msg, " Plugin     : ");
         len = PmStrcat(msg, plugin->file);
         if (plugin->pinfo) {
            genpInfo *info = (genpInfo *)plugin->pinfo;
            PmStrcat(msg, "\n");
            PmStrcat(msg, " Description: ");
            PmStrcat(msg, NPRT(info->plugin_description));
            PmStrcat(msg, "\n");

            PmStrcat(msg, " Version    : ");
            PmStrcat(msg, NPRT(info->plugin_version));
            PmStrcat(msg, ", Date: ");
            PmStrcat(msg, NPRT(info->plugin_date));
            PmStrcat(msg, "\n");

            PmStrcat(msg, " Author     : ");
            PmStrcat(msg, NPRT(info->plugin_author));
            PmStrcat(msg, "\n");

            PmStrcat(msg, " License    : ");
            PmStrcat(msg, NPRT(info->plugin_license));
            PmStrcat(msg, "\n");

            if (info->plugin_usage) {
               PmStrcat(msg, " Usage      : ");
               PmStrcat(msg, info->plugin_usage);
               PmStrcat(msg, "\n");
            }

            PmStrcat(msg, "\n");
         }
      }
      len = PmStrcat(msg, "\n");
   }

   return len;
}

/*
 * Dump plugin information
 * Each daemon can register a hook that will be called
 * after a fatal signal.
 */
#define DBG_MAX_HOOK 10
static dbg_plugin_hook_t *dbg_plugin_hooks[DBG_MAX_HOOK];
static dbg_print_plugin_hook_t *dbg_print_plugin_hook = NULL;
static int dbg_plugin_hook_count = 0;

void DbgPluginAddHook(dbg_plugin_hook_t *fct)
{
   ASSERT(dbg_plugin_hook_count < DBG_MAX_HOOK);
   dbg_plugin_hooks[dbg_plugin_hook_count++] = fct;
}

void DbgPrintPluginAddHook(dbg_print_plugin_hook_t *fct)
{
   dbg_print_plugin_hook = fct;
}

void DumpPlugins(alist *plugin_list, FILE *fp)
{
   int i;
   Plugin *plugin;
   fprintf(fp, "Attempt to dump plugins. Hook count=%d\n", dbg_plugin_hook_count);

   if (!plugin_list) {
      return;
   }
   foreach_alist_index(i, plugin, plugin_list) {
      for(int i=0; i < dbg_plugin_hook_count; i++) {
//       dbg_plugin_hook_t *fct = dbg_plugin_hooks[i];
         fprintf(fp, "Plugin %p name=\"%s\"\n", plugin, plugin->file);
//       fct(plugin, fp);
      }
   }
}

/*
 * Bounce from library to daemon and back to get the proper plugin_list.
 * As the function is called from the signal context we don't have the
 * plugin_list as argument and we don't want to expose it as global variable.
 * If the daemon didn't register a dump plugin function this is a NOP.
 */
void DbgPrintPlugin(FILE *fp)
{
   if (dbg_print_plugin_hook) {
      dbg_print_plugin_hook(fp);
   }
}
