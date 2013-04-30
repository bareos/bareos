/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2007-2011 Free Software Foundation Europe e.V.

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
#include "stored.h"
#include "sd_plugins.h"

const int dbglvl = 250;
const char *plugin_type = "-sd.so";


/* Forward referenced functions */
static bRC baculaGetValue(bpContext *ctx, bsdrVariable var, void *value);
static bRC baculaSetValue(bpContext *ctx, bsdwVariable var, void *value);
static bRC baculaRegisterEvents(bpContext *ctx, ...);
static bRC baculaJobMsg(bpContext *ctx, const char *file, int line,
  int type, utime_t mtime, const char *fmt, ...);
static bRC baculaDebugMsg(bpContext *ctx, const char *file, int line,
  int level, const char *fmt, ...);
static char *baculaEditDeviceCodes(DCR *dcr, char *omsg, 
  const char *imsg, const char *cmd);
static bool is_plugin_compatible(Plugin *plugin);


/* Bacula info */
static bsdInfo binfo = {
   sizeof(bsdFuncs),
   SD_PLUGIN_INTERFACE_VERSION
};

/* Bacula entry points */
static bsdFuncs bfuncs = {
   sizeof(bsdFuncs),
   SD_PLUGIN_INTERFACE_VERSION,
   baculaRegisterEvents,
   baculaGetValue,
   baculaSetValue,
   baculaJobMsg,
   baculaDebugMsg,
   baculaEditDeviceCodes
};

/* 
 * Bacula private context
 */
struct bacula_ctx {
   JCR *jcr;                             /* jcr for plugin */
   bRC  rc;                              /* last return code */
   bool disabled;                        /* set if plugin disabled */
};

static bool is_plugin_disabled(bpContext *plugin_ctx)
{
   bacula_ctx *b_ctx;
   if (!plugin_ctx) {
      return true;
   }
   b_ctx = (bacula_ctx *)plugin_ctx->bContext;
   return b_ctx->disabled;
}

#ifdef needed
static bool is_plugin_disabled(JCR *jcr)
{
   return is_plugin_disabled(jcr->plugin_ctx);
}
#endif

/*
 * Create a plugin event 
 */
int generate_plugin_event(JCR *jcr, bsdEventType eventType, void *value)
{
   bpContext *plugin_ctx;
   bsdEvent event;
   Plugin *plugin;
   int i;
   bRC rc = bRC_OK;

   if (!bplugin_list) {
      Dmsg0(dbglvl, "No bplugin_list: generate_plugin_event ignored.\n");
      return bRC_OK;
   }
   if (!jcr) {
      Dmsg0(dbglvl, "No jcr: generate_plugin_event ignored.\n");
      return bRC_OK;
   }
   if (!jcr->plugin_ctx_list) {
      Dmsg0(dbglvl, "No plugin_ctx_list: generate_plugin_event ignored.\n");
      return bRC_OK;                  /* Return if no plugins loaded */
   }
   if (jcr->is_job_canceled()) {
      Dmsg0(dbglvl, "Cancel return from generate_plugin_event\n");
      return bRC_Cancel;
   }

   bpContext *plugin_ctx_list = (bpContext *)jcr->plugin_ctx_list;
   event.eventType = eventType;

   Dmsg2(dbglvl, "sd-plugin_ctx_list=%p JobId=%d\n", jcr->plugin_ctx_list, jcr->JobId);

   foreach_alist_index(i, plugin, bplugin_list) {
      plugin_ctx = &plugin_ctx_list[i];
      if (is_plugin_disabled(plugin_ctx)) {
         continue;
      }
      rc = sdplug_func(plugin)->handlePluginEvent(plugin_ctx, &event, value);
      if (rc != bRC_OK) {
         break;
      }
   }

   return rc;
}

/*
 * Print to file the plugin info.
 */
void dump_sd_plugin(Plugin *plugin, FILE *fp)
{
   if (!plugin) {
      return ;
   }
   psdInfo *info = (psdInfo *) plugin->pinfo;
   fprintf(fp, "\tversion=%d\n", info->version);
   fprintf(fp, "\tdate=%s\n", NPRTB(info->plugin_date));
   fprintf(fp, "\tmagic=%s\n", NPRTB(info->plugin_magic));
   fprintf(fp, "\tauthor=%s\n", NPRTB(info->plugin_author));
   fprintf(fp, "\tlicence=%s\n", NPRTB(info->plugin_license));
   fprintf(fp, "\tversion=%s\n", NPRTB(info->plugin_version));
   fprintf(fp, "\tdescription=%s\n", NPRTB(info->plugin_description));
}

/**
 * This entry point is called internally by Bacula to ensure
 *  that the plugin IO calls come into this code.
 */
void load_sd_plugins(const char *plugin_dir)
{
   Plugin *plugin;
   int i;

   Dmsg0(dbglvl, "Load sd plugins\n");
   if (!plugin_dir) {
      Dmsg0(dbglvl, "No sd plugin dir!\n");
      return;
   }
   bplugin_list = New(alist(10, not_owned_by_alist));
   if (!load_plugins((void *)&binfo, (void *)&bfuncs, plugin_dir, plugin_type, 
                is_plugin_compatible)) {
      /* Either none found, or some error */
      if (bplugin_list->size() == 0) {
         delete bplugin_list;
         bplugin_list = NULL;
         Dmsg0(dbglvl, "No plugins loaded\n");
         return;
      }
   }
   /* 
    * Verify that the plugin is acceptable, and print information
    *  about it.
    */
   foreach_alist_index(i, plugin, bplugin_list) {
      Jmsg(NULL, M_INFO, 0, _("Loaded plugin: %s\n"), plugin->file);
      Dmsg1(dbglvl, "Loaded plugin: %s\n", plugin->file);
   }

   Dmsg1(dbglvl, "num plugins=%d\n", bplugin_list->size());
   dbg_plugin_add_hook(dump_sd_plugin);
}

/**
 * Check if a plugin is compatible.  Called by the load_plugin function
 *  to allow us to verify the plugin.
 */
static bool is_plugin_compatible(Plugin *plugin)
{
   psdInfo *info = (psdInfo *)plugin->pinfo;
   Dmsg0(50, "is_plugin_compatible called\n");
   if (debug_level >= 50) {
      dump_sd_plugin(plugin, stdin);
   }
   if (strcmp(info->plugin_magic, SD_PLUGIN_MAGIC) != 0) {
      Jmsg(NULL, M_ERROR, 0, _("Plugin magic wrong. Plugin=%s wanted=%s got=%s\n"),
           plugin->file, SD_PLUGIN_MAGIC, info->plugin_magic);
      Dmsg3(50, "Plugin magic wrong. Plugin=%s wanted=%s got=%s\n",
           plugin->file, SD_PLUGIN_MAGIC, info->plugin_magic);

      return false;
   }
   if (info->version != SD_PLUGIN_INTERFACE_VERSION) {
      Jmsg(NULL, M_ERROR, 0, _("Plugin version incorrect. Plugin=%s wanted=%d got=%d\n"),
           plugin->file, SD_PLUGIN_INTERFACE_VERSION, info->version);
      Dmsg3(50, "Plugin version incorrect. Plugin=%s wanted=%d got=%d\n",
           plugin->file, SD_PLUGIN_INTERFACE_VERSION, info->version);
      return false;
   }
   if (strcmp(info->plugin_license, "Bacula AGPLv3") != 0 &&
       strcmp(info->plugin_license, "AGPLv3") != 0 &&
       strcmp(info->plugin_license, "Bacula Systems(R) SA") != 0) {
      Jmsg(NULL, M_ERROR, 0, _("Plugin license incompatible. Plugin=%s license=%s\n"),
           plugin->file, info->plugin_license);
      Dmsg2(50, "Plugin license incompatible. Plugin=%s license=%s\n",
           plugin->file, info->plugin_license);
      return false;
   }
   if (info->size != sizeof(psdInfo)) {
      Jmsg(NULL, M_ERROR, 0,
           _("Plugin size incorrect. Plugin=%s wanted=%d got=%d\n"),
           plugin->file, sizeof(psdInfo), info->size);
      return false;
   }
      
   return true;
}


/*
 * Create a new instance of each plugin for this Job
 */
void new_plugins(JCR *jcr)
{
   Plugin *plugin;
   int i;

   Dmsg0(dbglvl, "=== enter new_plugins ===\n");
   if (!bplugin_list) {
      Dmsg0(dbglvl, "No sd plugin list!\n");
      return;
   }
   if (jcr->is_job_canceled()) {
      return;
   }
   /* 
    * If plugins already loaded, just return
    */
   if (jcr->plugin_ctx_list) {
      return;
   }

   int num = bplugin_list->size();

   Dmsg1(dbglvl, "sd-plugin-list size=%d\n", num);
   if (num == 0) {
      return;
   }

   jcr->plugin_ctx_list = (bpContext *)malloc(sizeof(bpContext) * num);

   bpContext *plugin_ctx_list = jcr->plugin_ctx_list;
   Dmsg2(dbglvl, "Instantiate sd-plugin_ctx_list=%p JobId=%d\n", jcr->plugin_ctx_list, jcr->JobId);
   foreach_alist_index(i, plugin, bplugin_list) {
      /* Start a new instance of each plugin */
      bacula_ctx *b_ctx = (bacula_ctx *)malloc(sizeof(bacula_ctx));
      memset(b_ctx, 0, sizeof(bacula_ctx));
      b_ctx->jcr = jcr;
      plugin_ctx_list[i].bContext = (void *)b_ctx;
      plugin_ctx_list[i].pContext = NULL;
      if (sdplug_func(plugin)->newPlugin(&plugin_ctx_list[i]) != bRC_OK) {
         b_ctx->disabled = true;
      }
   }
}

/*
 * Free the plugin instances for this Job
 */
void free_plugins(JCR *jcr)
{
   Plugin *plugin;
   int i;

   if (!bplugin_list || !jcr->plugin_ctx_list) {
      return;
   }

   bpContext *plugin_ctx_list = (bpContext *)jcr->plugin_ctx_list;
   Dmsg2(dbglvl, "Free instance sd-plugin_ctx_list=%p JobId=%d\n", jcr->plugin_ctx_list, jcr->JobId);
   foreach_alist_index(i, plugin, bplugin_list) {
      /* Free the plugin instance */
      sdplug_func(plugin)->freePlugin(&plugin_ctx_list[i]);
      free(plugin_ctx_list[i].bContext);     /* free Bacula private context */
   }
   free(plugin_ctx_list);
   jcr->plugin_ctx_list = NULL;
}


/* ==============================================================
 *
 * Callbacks from the plugin
 *
 * ==============================================================
 */
static bRC baculaGetValue(bpContext *ctx, bsdrVariable var, void *value)
{
   JCR *jcr;
   if (!ctx) {
      return bRC_Error;
   }
   jcr = ((bacula_ctx *)ctx->bContext)->jcr;
   if (!jcr) {
      return bRC_Error;
   }
   if (!value) {
      return bRC_Error;
   }
   switch (var) {
   case bsdVarJobId:
      *((int *)value) = jcr->JobId;
      Dmsg1(dbglvl, "sd-plugin: return bVarJobId=%d\n", jcr->JobId);
      break;
   case bsdVarJobName:
      *((char **)value) = jcr->Job;
      Dmsg1(dbglvl, "Bacula: return Job name=%s\n", jcr->Job);
      break;
   default:
      break;
   }
   return bRC_OK;
}

static bRC baculaSetValue(bpContext *ctx, bsdwVariable var, void *value)
{
   JCR *jcr;   
   if (!value || !ctx) {
      return bRC_Error;
   }
// Dmsg1(dbglvl, "bacula: baculaGetValue var=%d\n", var);
   jcr = ((bacula_ctx *)ctx->bContext)->jcr;
   if (!jcr) {
      return bRC_Error;
   }
// Dmsg1(dbglvl, "Bacula: jcr=%p\n", jcr); 
   /* Nothing implemented yet */
   Dmsg1(dbglvl, "sd-plugin: baculaSetValue var=%d\n", var);
   return bRC_OK;
}

static bRC baculaRegisterEvents(bpContext *ctx, ...)
{
   va_list args;
   uint32_t event;

   va_start(args, ctx);
   while ((event = va_arg(args, uint32_t))) {
      Dmsg1(dbglvl, "sd-Plugin wants event=%u\n", event);
   }
   va_end(args);
   return bRC_OK;
}

static bRC baculaJobMsg(bpContext *ctx, const char *file, int line,
  int type, utime_t mtime, const char *fmt, ...)
{
   va_list arg_ptr;
   char buf[2000];
   JCR *jcr;

   if (ctx) {
      jcr = ((bacula_ctx *)ctx->bContext)->jcr;
   } else {
      jcr = NULL;
   }

   va_start(arg_ptr, fmt);
   bvsnprintf(buf, sizeof(buf), fmt, arg_ptr);
   va_end(arg_ptr);
   Jmsg(jcr, type, mtime, "%s", buf);
   return bRC_OK;
}

static bRC baculaDebugMsg(bpContext *ctx, const char *file, int line,
  int level, const char *fmt, ...)
{
   va_list arg_ptr;
   char buf[2000];

   va_start(arg_ptr, fmt);
   bvsnprintf(buf, sizeof(buf), fmt, arg_ptr);
   va_end(arg_ptr);
   d_msg(file, line, level, "%s", buf);
   return bRC_OK;
}

static char *baculaEditDeviceCodes(DCR *dcr, char *omsg, 
  const char *imsg, const char *cmd)
{
   return edit_device_codes(dcr, omsg, imsg, cmd);
}

#ifdef TEST_PROGRAM

int main(int argc, char *argv[])
{
   char plugin_dir[1000];
   JCR mjcr1, mjcr2;
   JCR *jcr1 = &mjcr1;
   JCR *jcr2 = &mjcr2;

   strcpy(my_name, "test-dir");
    
   getcwd(plugin_dir, sizeof(plugin_dir)-1);
   load_sd_plugins(plugin_dir);

   jcr1->JobId = 111;
   new_plugins(jcr1);

   jcr2->JobId = 222;
   new_plugins(jcr2);

   generate_plugin_event(jcr1, bsdEventJobStart, (void *)"Start Job 1");
   generate_plugin_event(jcr1, bsdEventJobEnd);
   generate_plugin_event(jcr2, bsdEventJobStart, (void *)"Start Job 1");
   free_plugins(jcr1);
   generate_plugin_event(jcr2, bsdEventJobEnd);
   free_plugins(jcr2);

   unload_plugins();

   Dmsg0(dbglvl, "sd-plugin: OK ...\n");
   close_memory_pool();
   sm_dump(false);
   return 0;
}

#endif /* TEST_PROGRAM */
