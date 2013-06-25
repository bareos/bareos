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
 * Bareos pluginloader
 *
 * Kern Sibbald, October 2007
 */
#include "bareos.h"
#include "stored.h"
#include "sd_plugins.h"
#include "lib/crypto_cache.h"

const int dbglvl = 250;
const char *plugin_type = "-sd.so";
static alist *sd_plugin_list = NULL;

/* Forward referenced functions */
static bRC bareosGetValue(bpContext *ctx, bsdrVariable var, void *value);
static bRC bareosSetValue(bpContext *ctx, bsdwVariable var, void *value);
static bRC bareosRegisterEvents(bpContext *ctx, int nr_events, ...);
static bRC bareosJobMsg(bpContext *ctx, const char *file, int line,
                        int type, utime_t mtime, const char *fmt, ...);
static bRC bareosDebugMsg(bpContext *ctx, const char *file, int line,
                          int level, const char *fmt, ...);
static char *bareosEditDeviceCodes(DCR *dcr, char *omsg,
                                   const char *imsg, const char *cmd);
static char *bareosLookupCryptoKey(const char *VolumeName);
static bool bareosUpdateVolumeInfo(DCR *dcr);
static bool is_plugin_compatible(Plugin *plugin);

/* Bareos info */
static bsdInfo binfo = {
   sizeof(bsdFuncs),
   SD_PLUGIN_INTERFACE_VERSION
};

/* Bareos entry points */
static bsdFuncs bfuncs = {
   sizeof(bsdFuncs),
   SD_PLUGIN_INTERFACE_VERSION,
   bareosRegisterEvents,
   bareosGetValue,
   bareosSetValue,
   bareosJobMsg,
   bareosDebugMsg,
   bareosEditDeviceCodes,
   bareosLookupCryptoKey,
   bareosUpdateVolumeInfo
};

/*
 * Bareos private context
 */
struct b_plugin_ctx {
   JCR *jcr;                                       /* jcr for plugin */
   bRC  rc;                                        /* last return code */
   bool disabled;                                  /* set if plugin disabled */
   char events[nbytes_for_bits(SD_NR_EVENTS + 1)]; /* enabled events bitmask */
};

static inline bool is_event_enabled(bpContext *ctx, bsdEventType eventType)
{
   b_plugin_ctx *b_ctx;
   if (!ctx) {
      return true;
   }
   b_ctx = (b_plugin_ctx *)ctx->bContext;
   if (!b_ctx) {
      return true;
   }
   return bit_is_set(eventType, b_ctx->events);
}

static inline bool is_plugin_disabled(bpContext *ctx)
{
   b_plugin_ctx *b_ctx;
   if (!ctx) {
      return true;
   }
   b_ctx = (b_plugin_ctx *)ctx->bContext;
   return b_ctx->disabled;
}

#ifdef needed
static inline bool is_plugin_disabled(JCR *jcr)
{
   return is_plugin_disabled(jcr->plugin_ctx);
}
#endif

/*
 * Edit codes into ChangerCommand
 *  %% = %
 *  %a = archive device name
 *  %c = changer device name
 *  %d = changer drive index
 *  %f = Client's name
 *  %j = Job name
 *  %o = command
 *  %s = Slot base 0
 *  %S = Slot base 1
 *  %v = Volume name
 *
 *
 *  omsg = edited output message
 *  imsg = input string containing edit codes (%x)
 *  cmd = command string (load, unload, ...)
 *
 */
char *edit_device_codes(DCR *dcr, char *omsg, const char *imsg, const char *cmd)
{
   const char *p;
   const char *str;
   char ed1[50];

   *omsg = 0;
   Dmsg1(1800, "edit_device_codes: %s\n", imsg);
   for (p=imsg; *p; p++) {
      if (*p == '%') {
         switch (*++p) {
         case '%':
            str = "%";
            break;
         case 'a':
            str = dcr->dev->archive_name();
            break;
         case 'c':
            str = NPRT(dcr->device->changer_name);
            break;
         case 'd':
            str = edit_int64(dcr->dev->drive_index, ed1);
            break;
         case 'o':
            str = NPRT(cmd);
            break;
         case 's':
            str = edit_int64(dcr->VolCatInfo.Slot - 1, ed1);
            break;
         case 'S':
            str = edit_int64(dcr->VolCatInfo.Slot, ed1);
            break;
         case 'j':                    /* Job name */
            str = dcr->jcr->Job;
            break;
         case 'v':
            if (dcr->VolCatInfo.VolCatName[0]) {
               str = dcr->VolCatInfo.VolCatName;
            } else if (dcr->VolumeName[0]) {
               str = dcr->VolumeName;
            } else if (dcr->dev->vol && dcr->dev->vol->vol_name) {
               str = dcr->dev->vol->vol_name;
            } else {
               str = dcr->dev->VolHdr.VolumeName;
            }
            break;
         case 'f':
            str = NPRT(dcr->jcr->client_name);
            break;
         default:
            ed1[0] = '%';
            ed1[1] = *p;
            ed1[2] = 0;
            str = ed1;
            break;
         }
      } else {
         ed1[0] = *p;
         ed1[1] = 0;
         str = ed1;
      }
      Dmsg1(1900, "add_str %s\n", str);
      pm_strcat(&omsg, (char *)str);
      Dmsg1(1800, "omsg=%s\n", omsg);
   }
   Dmsg1(800, "omsg=%s\n", omsg);
   return omsg;
}

static inline bRC trigger_plugin_event(JCR *jcr, bsdEventType eventType, bsdEvent *event,
                                       Plugin *plugin, bpContext *plugin_ctx_list,
                                       int index, void *value)
{
   bpContext *ctx;

   ctx = &plugin_ctx_list[index];
   if (!is_event_enabled(ctx, eventType)) {
      Dmsg1(dbglvl, "Event %d disabled for this plugin.\n", eventType);
      return bRC_OK;
   }

   if (is_plugin_disabled(ctx)) {
      Dmsg0(dbglvl, "Plugin disabled.\n");
      return bRC_OK;
   }

   return sdplug_func(plugin)->handlePluginEvent(ctx, event, value);
}

/*
 * Create a plugin event
 */
int generate_plugin_event(JCR *jcr, bsdEventType eventType, void *value, bool reverse)
{
   bsdEvent event;
   Plugin *plugin;
   int i;
   bRC rc = bRC_OK;

   if (!sd_plugin_list) {
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

   /*
    * See if we need to trigger the loaded plugins in reverse order.
    */
   if (reverse) {
      foreach_alist_rindex(i, plugin, sd_plugin_list) {
         rc = trigger_plugin_event(jcr, eventType, &event, plugin, plugin_ctx_list, i, value);
         if (rc != bRC_OK) {
            break;
         }
      }
   } else {
      foreach_alist_index(i, plugin, sd_plugin_list) {
         rc = trigger_plugin_event(jcr, eventType, &event, plugin, plugin_ctx_list, i, value);
         if (rc != bRC_OK) {
            break;
         }
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
   genpInfo *info = (genpInfo *) plugin->pinfo;
   fprintf(fp, "\tversion=%d\n", info->version);
   fprintf(fp, "\tdate=%s\n", NPRTB(info->plugin_date));
   fprintf(fp, "\tmagic=%s\n", NPRTB(info->plugin_magic));
   fprintf(fp, "\tauthor=%s\n", NPRTB(info->plugin_author));
   fprintf(fp, "\tlicence=%s\n", NPRTB(info->plugin_license));
   fprintf(fp, "\tversion=%s\n", NPRTB(info->plugin_version));
   fprintf(fp, "\tdescription=%s\n", NPRTB(info->plugin_description));
}

static void dump_sd_plugins(FILE *fp)
{
   dump_plugins(sd_plugin_list, fp);
}

/**
 * This entry point is called internally by Bareos to ensure
 *  that the plugin IO calls come into this code.
 */
void load_sd_plugins(const char *plugin_dir, const char *plugin_names)
{
   Plugin *plugin;
   int i;

   Dmsg0(dbglvl, "Load sd plugins\n");
   if (!plugin_dir) {
      Dmsg0(dbglvl, "No sd plugin dir!\n");
      return;
   }
   sd_plugin_list = New(alist(10, not_owned_by_alist));
   if (!load_plugins((void *)&binfo, (void *)&bfuncs, sd_plugin_list,
                     plugin_dir, plugin_names, plugin_type, is_plugin_compatible)) {
      /*
       * Either none found, or some error
       */
      if (sd_plugin_list->size() == 0) {
         delete sd_plugin_list;
         sd_plugin_list = NULL;
         Dmsg0(dbglvl, "No plugins loaded\n");
         return;
      }
   }
   /*
    * Verify that the plugin is acceptable, and print information about it.
    */
   foreach_alist_index(i, plugin, sd_plugin_list) {
      Jmsg(NULL, M_INFO, 0, _("Loaded plugin: %s\n"), plugin->file);
      Dmsg1(dbglvl, "Loaded plugin: %s\n", plugin->file);
   }

   Dmsg1(dbglvl, "num plugins=%d\n", sd_plugin_list->size());
   dbg_plugin_add_hook(dump_sd_plugin);
   dbg_print_plugin_add_hook(dump_sd_plugins);
}

void unload_sd_plugins(void)
{
   unload_plugins(sd_plugin_list);
   delete sd_plugin_list;
   sd_plugin_list = NULL;
}

int list_sd_plugins(POOL_MEM &msg)
{
   return list_plugins(sd_plugin_list, msg);
}

/**
 * Check if a plugin is compatible.  Called by the load_plugin function
 *  to allow us to verify the plugin.
 */
static bool is_plugin_compatible(Plugin *plugin)
{
   genpInfo *info = (genpInfo *)plugin->pinfo;
   Dmsg0(50, "is_plugin_compatible called\n");
   if (debug_level >= 50) {
      dump_sd_plugin(plugin, stdin);
   }
   if (!bstrcmp(info->plugin_magic, SD_PLUGIN_MAGIC)) {
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
   if (!bstrcasecmp(info->plugin_license, "Bareos AGPLv3") &&
       !bstrcasecmp(info->plugin_license, "Bacula AGPLv3") &&
       !bstrcasecmp(info->plugin_license, "AGPLv3")) {
      Jmsg(NULL, M_ERROR, 0, _("Plugin license incompatible. Plugin=%s license=%s\n"),
           plugin->file, info->plugin_license);
      Dmsg2(50, "Plugin license incompatible. Plugin=%s license=%s\n",
           plugin->file, info->plugin_license);
      return false;
   }
   if (info->size != sizeof(genpInfo)) {
      Jmsg(NULL, M_ERROR, 0,
           _("Plugin size incorrect. Plugin=%s wanted=%d got=%d\n"),
           plugin->file, sizeof(genpInfo), info->size);
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
   int i, num;

   Dmsg0(dbglvl, "=== enter new_plugins ===\n");
   if (!sd_plugin_list) {
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

   num = sd_plugin_list->size();
   Dmsg1(dbglvl, "sd-plugin-list size=%d\n", num);
   if (num == 0) {
      return;
   }

   jcr->plugin_ctx_list = (bpContext *)malloc(sizeof(bpContext) * num);

   bpContext *plugin_ctx_list = jcr->plugin_ctx_list;
   Dmsg2(dbglvl, "Instantiate sd-plugin_ctx_list=%p JobId=%d\n", jcr->plugin_ctx_list, jcr->JobId);
   foreach_alist_index(i, plugin, sd_plugin_list) {
      /* Start a new instance of each plugin */
      b_plugin_ctx *b_ctx = (b_plugin_ctx *)malloc(sizeof(b_plugin_ctx));
      memset(b_ctx, 0, sizeof(b_plugin_ctx));
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

   if (!sd_plugin_list || !jcr->plugin_ctx_list) {
      return;
   }

   bpContext *plugin_ctx_list = (bpContext *)jcr->plugin_ctx_list;
   Dmsg2(dbglvl, "Free instance sd-plugin_ctx_list=%p JobId=%d\n", jcr->plugin_ctx_list, jcr->JobId);
   foreach_alist_index(i, plugin, sd_plugin_list) {
      /* Free the plugin instance */
      sdplug_func(plugin)->freePlugin(&plugin_ctx_list[i]);
      free(plugin_ctx_list[i].bContext);     /* free Bareos private context */
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
static bRC bareosGetValue(bpContext *ctx, bsdrVariable var, void *value)
{
   JCR *jcr;
   if (!ctx) {
      return bRC_Error;
   }
   jcr = ((b_plugin_ctx *)ctx->bContext)->jcr;
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
      Dmsg1(dbglvl, "Bareos: return Job name=%s\n", jcr->Job);
      break;
   default:
      break;
   }
   return bRC_OK;
}

static bRC bareosSetValue(bpContext *ctx, bsdwVariable var, void *value)
{
   JCR *jcr;
   if (!value || !ctx) {
      return bRC_Error;
   }
// Dmsg1(dbglvl, "bareos: bareosGetValue var=%d\n", var);
   jcr = ((b_plugin_ctx *)ctx->bContext)->jcr;
   if (!jcr) {
      return bRC_Error;
   }
// Dmsg1(dbglvl, "Bareos: jcr=%p\n", jcr);
   /* Nothing implemented yet */
   Dmsg1(dbglvl, "sd-plugin: bareosSetValue var=%d\n", var);
   return bRC_OK;
}

static bRC bareosRegisterEvents(bpContext *ctx, int nr_events, ...)
{
   int i;
   va_list args;
   uint32_t event;
   b_plugin_ctx *b_ctx;

   if (!ctx) {
      return bRC_Error;
   }
   b_ctx = (b_plugin_ctx *)ctx->bContext;
   va_start(args, nr_events);
   for (i = 0; i < nr_events; i++) {
      event = va_arg(args, uint32_t);
      Dmsg1(dbglvl, "sd-Plugin wants event=%u\n", event);
      set_bit(event, b_ctx->events);
   }
   va_end(args);
   return bRC_OK;
}

static bRC bareosJobMsg(bpContext *ctx, const char *file, int line,
                        int type, utime_t mtime, const char *fmt, ...)
{
   va_list arg_ptr;
   char buf[2000];
   JCR *jcr;

   if (ctx) {
      jcr = ((b_plugin_ctx *)ctx->bContext)->jcr;
   } else {
      jcr = NULL;
   }

   va_start(arg_ptr, fmt);
   bvsnprintf(buf, sizeof(buf), fmt, arg_ptr);
   va_end(arg_ptr);
   Jmsg(jcr, type, mtime, "%s", buf);
   return bRC_OK;
}

static bRC bareosDebugMsg(bpContext *ctx, const char *file, int line,
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

static char *bareosEditDeviceCodes(DCR *dcr, char *omsg,
                                   const char *imsg, const char *cmd)
{
   return edit_device_codes(dcr, omsg, imsg, cmd);
}

static char *bareosLookupCryptoKey(const char *VolumeName)
{
   return lookup_crypto_cache_entry(VolumeName);
}

static bool bareosUpdateVolumeInfo(DCR *dcr)
{
   return dir_get_volume_info(dcr, GET_VOL_INFO_FOR_READ);
}

#ifdef TEST_PROGRAM

int main(int argc, char *argv[])
{
   char plugin_dir[1000];
   JCR mjcr1, mjcr2;
   JCR *jcr1 = &mjcr1;
   JCR *jcr2 = &mjcr2;

   my_name_is(argc, argv, "plugtest");
   init_msg(NULL, NULL);

   OSDependentInit();

   if (argc != 1) {
      bstrncpy(plugin_dir, argv[1], sizeof(plugin_dir));
   } else {
      getcwd(plugin_dir, sizeof(plugin_dir)-1);
   }
   load_sd_plugins(plugin_dir, NULL);

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

   unload_sd_plugins();

   term_msg();
   close_memory_pool();
   lmgr_cleanup_main();
   sm_dump(false);
   exit(0);
}

#endif /* TEST_PROGRAM */
