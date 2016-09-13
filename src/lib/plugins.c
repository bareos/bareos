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

#include "bareos.h"

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
int readdir_r(DIR *dirp, struct dirent *entry, struct dirent **result);
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

static const int dbglvl = 50;

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

static void close_plugin(Plugin *plugin)
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
                          bool is_plugin_compatible(Plugin *plugin))
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
      Dmsg2(dbglvl, "dlopen plugin %s failed: ERR=%s\n",
            plugin_pathname, NPRT(error));

      close_plugin(plugin);

      return false;
   }

   /*
    * Get two global entry points
    */
   loadPlugin = (t_loadPlugin)dlsym(plugin->pHandle, "loadPlugin");
   if (!loadPlugin) {
      Jmsg(NULL, M_ERROR, 0, _("Lookup of loadPlugin in plugin %s failed: ERR=%s\n"),
           plugin_pathname, NPRT(dlerror()));
      Dmsg2(dbglvl, "Lookup of loadPlugin in plugin %s failed: ERR=%s\n",
            plugin_pathname, NPRT(dlerror()));

      close_plugin(plugin);

      return false;
   }

   plugin->unloadPlugin = (t_unloadPlugin)dlsym(plugin->pHandle, "unloadPlugin");
   if (!plugin->unloadPlugin) {
      Jmsg(NULL, M_ERROR, 0, _("Lookup of unloadPlugin in plugin %s failed: ERR=%s\n"),
           plugin_pathname, NPRT(dlerror()));
      Dmsg2(dbglvl, "Lookup of unloadPlugin in plugin %s failed: ERR=%s\n",
            plugin_pathname, NPRT(dlerror()));

      close_plugin(plugin);

      return false;
   }

   /*
    * Initialize the plugin
    */
   if (loadPlugin(binfo, bfuncs, &plugin->pinfo, &plugin->pfuncs) != bRC_OK) {
      close_plugin(plugin);

      return false;
   }

   if (!is_plugin_compatible) {
      Dmsg0(50, "Plugin compatibility pointer not set.\n");
   } else if (!is_plugin_compatible(plugin)) {
      close_plugin(plugin);

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
bool load_plugins(void *binfo,
                  void *bfuncs,
                  alist *plugin_list,
                  const char *plugin_dir,
                  alist *plugin_names,
                  const char *type,
                  bool is_plugin_compatible(Plugin *plugin))
{
   struct stat statp;
   bool found = false;
   POOL_MEM fname(PM_FNAME);
   bool need_slash = false;
   int len;

   Dmsg0(dbglvl, "load_plugins\n");

   len = strlen(plugin_dir);
   if (len > 0) {
      need_slash = !IsPathSeparator(plugin_dir[len - 1]);
   }

   /*
    * See if we are loading certain plugins only or all plugins of a certain type.
    */
   if (plugin_names && plugin_names->size()) {
      char *name;
      POOL_MEM plugin_name(PM_FNAME);

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
                           plugin_list, is_plugin_compatible)) {
            found = true;
         }
      }
   } else {
      int name_max, type_len;
      DIR *dp = NULL;
      struct dirent *entry = NULL, *result;

      name_max = pathconf(".", _PC_NAME_MAX);
      if (name_max < 1024) {
         name_max = 1024;
      }

      if (!(dp = opendir(plugin_dir))) {
         berrno be;
         Jmsg(NULL, M_ERROR_TERM, 0, _("Failed to open Plugin directory %s: ERR=%s\n"),
               plugin_dir, be.bstrerror());
         Dmsg2(dbglvl, "Failed to open Plugin directory %s: ERR=%s\n",
               plugin_dir, be.bstrerror());
         goto bail_out;
      }

      entry = (struct dirent *)malloc(sizeof(struct dirent) + name_max + 1000);
      while (1) {
         if ((readdir_r(dp, entry, &result) != 0) || (result == NULL)) {
            if (!found) {
               Jmsg(NULL, M_WARNING, 0, _("Failed to find any plugins in %s\n"), plugin_dir);
               Dmsg1(dbglvl, "Failed to find any plugins in %s\n", plugin_dir);
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
            Dmsg3(dbglvl, "Rejected plugin: want=%s name=%s len=%d\n", type, result->d_name, len);
            continue;
         }
         Dmsg2(dbglvl, "Found plugin: name=%s len=%d\n", result->d_name, len);

         pm_strcpy(fname, plugin_dir);
         if (need_slash) {
            pm_strcat(fname, "/");
         }
         pm_strcat(fname, result->d_name);

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
                           plugin_list, is_plugin_compatible)) {
            found = true;
         }
      }

      if (entry) {
         free(entry);
      }

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
void unload_plugins(alist *plugin_list)
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

void unload_plugin(alist *plugin_list, Plugin *plugin, int index)
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

int list_plugins(alist *plugin_list, POOL_MEM &msg)
{
   int i, len = 0;
   Plugin *plugin;

   if (plugin_list && plugin_list->size() > 0) {
      pm_strcpy(msg, "Plugin Info:\n");
      foreach_alist_index(i, plugin, plugin_list) {
         pm_strcat(msg, " Plugin     : ");
         len = pm_strcat(msg, plugin->file);
         if (plugin->pinfo) {
            genpInfo *info = (genpInfo *)plugin->pinfo;
            pm_strcat(msg, "\n");
            pm_strcat(msg, " Description: ");
            pm_strcat(msg, NPRT(info->plugin_description));
            pm_strcat(msg, "\n");

            pm_strcat(msg, " Version    : ");
            pm_strcat(msg, NPRT(info->plugin_version));
            pm_strcat(msg, ", Date: ");
            pm_strcat(msg, NPRT(info->plugin_date));
            pm_strcat(msg, "\n");

            pm_strcat(msg, " Author     : ");
            pm_strcat(msg, NPRT(info->plugin_author));
            pm_strcat(msg, "\n");

            pm_strcat(msg, " License    : ");
            pm_strcat(msg, NPRT(info->plugin_license));
            pm_strcat(msg, "\n");

            if (info->plugin_usage) {
               pm_strcat(msg, " Usage      : ");
               pm_strcat(msg, info->plugin_usage);
               pm_strcat(msg, "\n");
            }

            pm_strcat(msg, "\n");
         }
      }
      len = pm_strcat(msg, "\n");
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

void dbg_plugin_add_hook(dbg_plugin_hook_t *fct)
{
   ASSERT(dbg_plugin_hook_count < DBG_MAX_HOOK);
   dbg_plugin_hooks[dbg_plugin_hook_count++] = fct;
}

void dbg_print_plugin_add_hook(dbg_print_plugin_hook_t *fct)
{
   dbg_print_plugin_hook = fct;
}

void dump_plugins(alist *plugin_list, FILE *fp)
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
void dbg_print_plugin(FILE *fp)
{
   if (dbg_print_plugin_hook) {
      dbg_print_plugin_hook(fp);
   }
}
