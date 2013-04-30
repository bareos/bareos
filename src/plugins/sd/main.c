/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2007-2010 Free Software Foundation Europe e.V.

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

   Bacula® is a registered trademark of Kern Sibbald.
   The licensor of Bacula is the Free Software Foundation Europe
   (FSFE), Fiduciary Program, Sumatrastrasse 25, 8006 Zürich,
   Switzerland, email:ftf@fsfeurope.org.
*/
/*
 * Main program to test loading and running Bacula plugins.
 *   Destined to become Bacula pluginloader, ...
 *
 * Kern Sibbald, October 2007
 */
#include "bacula.h"
#include <dlfcn.h>
#include "lib/plugin.h"
#include "plugin-sd.h"

const char *plugin_type = "-sd.so";


/* Forward referenced functions */
static bpError baculaGetValue(bpContext *ctx, bVariable var, void *value);
static bpError baculaSetValue(bpContext *ctx, bVariable var, void *value);


/* Bacula entry points */
static bFuncs bfuncs = {
   sizeof(bFuncs),
   PLUGIN_INTERFACE,
   baculaGetValue,
   baculaSetValue,
   NULL,
   NULL
};
    



int main(int argc, char *argv[])
{
   char plugin_dir[1000];
   bpContext ctx;
   bEvent event;
   Plugin *plugin;
    
   bplugin_list = New(alist(10, not_owned_by_alist));

   ctx.bContext = NULL;
   ctx.pContext = NULL;
   getcwd(plugin_dir, sizeof(plugin_dir)-1);

   load_plugins((void *)&bfuncs, plugin_dir, plugin_type);

   foreach_alist(plugin, bplugin_list) {
      printf("bacula: plugin_size=%d plugin_version=%d\n", 
              pref(plugin)->size, pref(plugin)->interface);
      printf("License: %s\nAuthor: %s\nDate: %s\nVersion: %s\nDescription: %s\n",
         pref(plugin)->plugin_license, pref(plugin)->plugin_author, 
         pref(plugin)->plugin_date, pref(plugin)->plugin_version, 
         pref(plugin)->plugin_description);

      /* Start a new instance of the plugin */
      pref(plugin)->newPlugin(&ctx);
      event.eventType = bEventNewVolume;   
      pref(plugin)->handlePluginEvent(&ctx, &event);
      /* Free the plugin instance */
      pref(plugin)->freePlugin(&ctx);

      /* Start a new instance of the plugin */
      pref(plugin)->newPlugin(&ctx);
      event.eventType = bEventNewVolume;   
      pref(plugin)->handlePluginEvent(&ctx, &event);
      /* Free the plugin instance */
      pref(plugin)->freePlugin(&ctx);
   }

   unload_plugins();

   printf("bacula: OK ...\n");
   close_memory_pool();
   sm_dump(false);
   return 0;
}

static bpError baculaGetValue(bpContext *ctx, bVariable var, void *value)
{
   printf("bacula: baculaGetValue var=%d\n", var);
   if (value) {
      *((int *)value) = 100;
   }
   return 0;
}

static bpError baculaSetValue(bpContext *ctx, bVariable var, void *value)
{
   printf("bacula: baculaSetValue var=%d\n", var);
   return 0;
}
