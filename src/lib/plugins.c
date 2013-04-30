/*
   Bacula(R) - The Network Backup Solution

   Copyright (C) 2007-2012 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from
   many others, a complete list can be found in the file AUTHORS.
   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation, which is 
   listed in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.

   Bacula(R) is a registered trademark of Kern Sibbald.
   The licensor of Bacula is the Free Software Foundation Europe
   (FSFE), Fiduciary Program, Sumatrastrasse 25, 8006 Zürich,
   Switzerland, email:ftf@fsfeurope.org.
*/
/*
 *    Plugin load/unloader for all Bacula daemons
 *
 * Kern Sibbald, October 2007
 */

#include "bacula.h"
#include <dlfcn.h>
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

#include "plugins.h"

static const int dbglvl = 50;

/* 
 * List of all loaded plugins.
 *
 * NOTE!!! This is a global do not try walking it with
 *   foreach_alist, you must use foreach_alist_index !!!!!!
 */
alist *bplugin_list = NULL;

/*
 * Create a new plugin "class" entry and enter it in the
 *  list of plugins.  Note, this is not the same as
 *  an instance of the plugin. 
 */
Plugin *new_plugin()
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
 * Load all the plugins in the specified directory.
 */
bool load_plugins(void *binfo, void *bfuncs, const char *plugin_dir, 
        const char *type, bool is_plugin_compatible(Plugin *plugin))
{
   bool found = false;
   t_loadPlugin loadPlugin;
   Plugin *plugin = NULL;
   DIR* dp = NULL;
   struct dirent *entry = NULL, *result;
   int name_max;
   struct stat statp;
   POOL_MEM fname(PM_FNAME);
   bool need_slash = false;
   int len, type_len;


   Dmsg0(dbglvl, "load_plugins\n");
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
      goto get_out;
   }
   
   len = strlen(plugin_dir);
   if (len > 0) {
      need_slash = !IsPathSeparator(plugin_dir[len - 1]);
   }
   entry = (struct dirent *)malloc(sizeof(struct dirent) + name_max + 1000);
   for ( ;; ) {
      plugin = NULL;            /* Start from a fresh plugin */

      if ((readdir_r(dp, entry, &result) != 0) || (result == NULL)) {
         if (!found) {
            Jmsg(NULL, M_WARNING, 0, _("Failed to find any plugins in %s\n"), 
                  plugin_dir);
            Dmsg1(dbglvl, "Failed to find any plugins in %s\n", plugin_dir);
         }
         break;
      }
      if (strcmp(result->d_name, ".") == 0 || 
          strcmp(result->d_name, "..") == 0) {
         continue;
      }

      len = strlen(result->d_name);
      type_len = strlen(type);
      if (len < type_len+1 || strcmp(&result->d_name[len-type_len], type) != 0) {
         Dmsg3(dbglvl, "Rejected plugin: want=%s name=%s len=%d\n", type, result->d_name, len);
         continue;
      }
      Dmsg2(dbglvl, "Found plugin: name=%s len=%d\n", result->d_name, len);
       
      pm_strcpy(fname, plugin_dir);
      if (need_slash) {
         pm_strcat(fname, "/");
      }
      pm_strcat(fname, result->d_name);
      if (lstat(fname.c_str(), &statp) != 0 || !S_ISREG(statp.st_mode)) {
         continue;                 /* ignore directories & special files */
      }

      plugin = new_plugin();
      plugin->file = bstrdup(result->d_name);
      plugin->file_len = strstr(plugin->file, type) - plugin->file;
      plugin->pHandle = dlopen(fname.c_str(), RTLD_NOW);
      if (!plugin->pHandle) {
         const char *error = dlerror();
         Jmsg(NULL, M_ERROR, 0, _("dlopen plugin %s failed: ERR=%s\n"), 
              fname.c_str(), NPRT(error));
         Dmsg2(dbglvl, "dlopen plugin %s failed: ERR=%s\n", fname.c_str(), 
               NPRT(error));
         close_plugin(plugin);
         continue;
      }

      /* Get two global entry points */
      loadPlugin = (t_loadPlugin)dlsym(plugin->pHandle, "loadPlugin");
      if (!loadPlugin) {
         Jmsg(NULL, M_ERROR, 0, _("Lookup of loadPlugin in plugin %s failed: ERR=%s\n"),
            fname.c_str(), NPRT(dlerror()));
         Dmsg2(dbglvl, "Lookup of loadPlugin in plugin %s failed: ERR=%s\n", 
            fname.c_str(), NPRT(dlerror()));
         close_plugin(plugin);
         continue;
      }
      plugin->unloadPlugin = (t_unloadPlugin)dlsym(plugin->pHandle, "unloadPlugin");
      if (!plugin->unloadPlugin) {
         Jmsg(NULL, M_ERROR, 0, _("Lookup of unloadPlugin in plugin %s failed: ERR=%s\n"),
            fname.c_str(), NPRT(dlerror()));
         Dmsg2(dbglvl, "Lookup of unloadPlugin in plugin %s failed: ERR=%s\n",
            fname.c_str(), NPRT(dlerror()));
         close_plugin(plugin);
         continue;
      }

      /* Initialize the plugin */
      if (loadPlugin(binfo, bfuncs, &plugin->pinfo, &plugin->pfuncs) != bRC_OK) {
         close_plugin(plugin);
         continue;
      }
      if (!is_plugin_compatible) {
         Dmsg0(50, "Plugin compatibility pointer not set.\n");   
      } else if (!is_plugin_compatible(plugin)) {
         close_plugin(plugin);
         continue;
      }

      found = true;                /* found a plugin */
      bplugin_list->append(plugin);
   }

get_out:
   if (!found && plugin) {
      close_plugin(plugin);
   }
   if (entry) {
      free(entry);
   }
   if (dp) {
      closedir(dp);
   }
   return found;
}

/*
 * Unload all the loaded plugins 
 */
void unload_plugins()
{
   Plugin *plugin;

   if (!bplugin_list) {
      return;
   }
   foreach_alist(plugin, bplugin_list) {
      /* Shut it down and unload it */
      plugin->unloadPlugin();
      dlclose(plugin->pHandle);
      if (plugin->file) {
         free(plugin->file);
      }
      free(plugin);
   }
   delete bplugin_list;
   bplugin_list = NULL;
}

/*
 * Dump plugin information
 * Each daemon can register a hook that will be called
 * after a fatal signal.
 */
#define DBG_MAX_HOOK 10
static dbg_plugin_hook_t *dbg_plugin_hooks[DBG_MAX_HOOK];
static int dbg_plugin_hook_count=0;

void dbg_plugin_add_hook(dbg_plugin_hook_t *fct)
{
   ASSERT(dbg_plugin_hook_count < DBG_MAX_HOOK);
   dbg_plugin_hooks[dbg_plugin_hook_count++] = fct;
}

void dbg_print_plugin(FILE *fp)
{
   Plugin *plugin;
   fprintf(fp, "Attempt to dump plugins. Hook count=%d\n", dbg_plugin_hook_count);

   if (!bplugin_list) {
      return;
   }
   foreach_alist(plugin, bplugin_list) {
      for(int i=0; i < dbg_plugin_hook_count; i++) {
//       dbg_plugin_hook_t *fct = dbg_plugin_hooks[i];
         fprintf(fp, "Plugin %p name=\"%s\" disabled=%d\n",
                 plugin, plugin->file, plugin->disabled);
//       fct(plugin, fp);
      }
   }
}
