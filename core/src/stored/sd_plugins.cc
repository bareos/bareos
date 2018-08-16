/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2007-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2016 Bareos GmbH & Co. KG

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
 * Kern Sibbald, October 2007
 */

/**
 * @file
 * Bareos pluginloader
 */
#include "include/bareos.h"
#include "stored/stored.h"
#include "sd_plugins.h"
#include "lib/crypto_cache.h"
#include "stored/sd_stats.h"
#include "lib/edit.h"
#include "include/jcr.h"

namespace storagedaemon {

const int debuglevel = 250;
#ifdef HAVE_WIN32
const char *plugin_type = "-sd.dll";
#else
const char *plugin_type = "-sd.so";
#endif
static alist *sd_plugin_list = NULL;

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

/* Forward referenced functions */
static bRC bareosGetValue(bpContext *ctx, bsdrVariable var, void *value);
static bRC bareosSetValue(bpContext *ctx, bsdwVariable var, void *value);
static bRC bareosRegisterEvents(bpContext *ctx, int nr_events, ...);
static bRC bareosUnRegisterEvents(bpContext *ctx, int nr_events, ...);
static bRC bareosGetInstanceCount(bpContext *ctx, int *ret);
static bRC bareosJobMsg(bpContext *ctx, const char *file, int line,
                        int type, utime_t mtime, const char *fmt, ...);
static bRC bareosDebugMsg(bpContext *ctx, const char *file, int line,
                          int level, const char *fmt, ...);
static char *bareosEditDeviceCodes(DeviceControlRecord *dcr, POOLMEM *&omsg,
                                   const char *imsg, const char *cmd);
static char *bareosLookupCryptoKey(const char *VolumeName);
static bool bareosUpdateVolumeInfo(DeviceControlRecord *dcr);
static void bareosUpdateTapeAlert(DeviceControlRecord *dcr, uint64_t flags);
static DeviceRecord *bareosNewRecord(bool with_data);
static void bareosCopyRecordState(DeviceRecord *dst, DeviceRecord *src);
static void bareosFreeRecord(DeviceRecord *rec);
static bool IsPluginCompatible(Plugin *plugin);

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
   bareosUnRegisterEvents,
   bareosGetInstanceCount,
   bareosGetValue,
   bareosSetValue,
   bareosJobMsg,
   bareosDebugMsg,
   bareosEditDeviceCodes,
   bareosLookupCryptoKey,
   bareosUpdateVolumeInfo,
   bareosUpdateTapeAlert,
   bareosNewRecord,
   bareosCopyRecordState,
   bareosFreeRecord
};

/**
 * Bareos private context
 */
struct b_plugin_ctx {
   JobControlRecord *jcr;                        /* jcr for plugin */
   bRC  rc;                                      /* last return code */
   bool disabled;                                /* set if plugin disabled */
   char events[NbytesForBits(SD_NR_EVENTS + 1)]; /* enabled events bitmask */
   Plugin *plugin;                               /* pointer to plugin of which this is an instance off */
};

static inline bool IsEventEnabled(bpContext *ctx, bsdEventType eventType)
{
   b_plugin_ctx *b_ctx;
   if (!ctx) {
      return false;
   }
   b_ctx = (b_plugin_ctx *)ctx->bContext;
   if (!b_ctx) {
      return false;
   }

   return BitIsSet(eventType, b_ctx->events);
}

static inline bool IsPluginDisabled(bpContext *ctx)
{
   b_plugin_ctx *b_ctx;
   if (!ctx) {
      return true;
   }
   b_ctx = (b_plugin_ctx *)ctx->bContext;
   if (!b_ctx) {
      return true;
   }
   return b_ctx->disabled;
}

#ifdef needed
static inline bool IsPluginDisabled(JobControlRecord *jcr)
{
   return IsPluginDisabled(jcr->plugin_ctx);
}
#endif

static bool IsCtxGood(bpContext *ctx, JobControlRecord *&jcr, b_plugin_ctx *&bctx)
{
   if (!ctx) {
      return false;
   }

   bctx = (b_plugin_ctx *)ctx->bContext;
   if (!bctx) {
      return false;
   }

   jcr = bctx->jcr;
   if (!jcr) {
      return false;
   }

   return true;
}

/**
 * Edit codes into ChangerCommand
 *  %% = %
 *  %a = Archive device name
 *  %c = Changer device name
 *  %D = Diagnostic device name
 *  %d = Changer drive index
 *  %f = Client's name
 *  %j = Job name
 *  %o = Command
 *  %s = Slot base 0
 *  %S = Slot base 1
 *  %v = Volume name
 *
 *
 *  omsg = edited output message
 *  imsg = input string containing edit codes (%x)
 *  cmd = command string (load, unload, ...)
 */
char *edit_device_codes(DeviceControlRecord *dcr, POOLMEM *&omsg, const char *imsg, const char *cmd)
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
         case 'D':
            str = NPRT(dcr->device->diag_device_name);
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
      PmStrcat(omsg, (char *)str);
      Dmsg1(1800, "omsg=%s\n", omsg);
   }
   Dmsg1(800, "omsg=%s\n", omsg);

   return omsg;
}

static inline bool trigger_plugin_event(JobControlRecord *jcr, bsdEventType eventType,
                                        bsdEvent *event, bpContext *ctx,
                                        void *value, alist *plugin_ctx_list,
                                        int *index, bRC *rc)
{
   bool stop = false;

   if (!IsEventEnabled(ctx, eventType)) {
      Dmsg1(debuglevel, "Event %d disabled for this plugin.\n", eventType);
      goto bail_out;
   }

   if (IsPluginDisabled(ctx)) {
      Dmsg0(debuglevel, "Plugin disabled.\n");
      goto bail_out;
   }

   /*
    * See if we should care about the return code.
    */
   if (rc) {
      *rc = SdplugFunc(ctx->plugin)->handlePluginEvent(ctx, event, value);
      switch (*rc) {
      case bRC_OK:
         break;
      case bRC_Stop:
      case bRC_Error:
         stop = true;
         break;
      case bRC_More:
         break;
      case bRC_Term:
         /*
          * Request to unload this plugin.
          * As we remove the plugin from the list of plugins we decrement
          * the running index value so the next plugin gets triggered as
          * that moved back a position in the alist.
          */
         if (index) {
            UnloadPlugin(plugin_ctx_list, ctx->plugin, *index);
            *index = ((*index) - 1);
         }
         break;
      case bRC_Seen:
         break;
      case bRC_Core:
         break;
      case bRC_Skip:
         stop = true;
         break;
      case bRC_Cancel:
         break;
      default:
         break;
      }
   } else {
      SdplugFunc(ctx->plugin)->handlePluginEvent(ctx, event, value);
   }

bail_out:
   return stop;
}

/**
 * Create a plugin event
 */
bRC GeneratePluginEvent(JobControlRecord *jcr, bsdEventType eventType, void *value, bool reverse)
{
   int i;
   bsdEvent event;
   alist *plugin_ctx_list;
   bRC rc = bRC_OK;

   if (!sd_plugin_list) {
      Dmsg0(debuglevel, "No bplugin_list: GeneratePluginEvent ignored.\n");
      goto bail_out;
   }

   if (!jcr) {
      Dmsg0(debuglevel, "No jcr: GeneratePluginEvent ignored.\n");
      goto bail_out;
   }

   /*
    * Return if no plugins loaded
    */
   if (!jcr->plugin_ctx_list) {
      Dmsg0(debuglevel, "No plugin_ctx_list: GeneratePluginEvent ignored.\n");
      goto bail_out;
   }

   plugin_ctx_list = jcr->plugin_ctx_list;
   event.eventType = eventType;

   Dmsg2(debuglevel, "sd-plugin_ctx_list=%p JobId=%d\n", plugin_ctx_list, jcr->JobId);

   /*
    * See if we need to trigger the loaded plugins in reverse order.
    */
   if (reverse) {
      bpContext *ctx;

      foreach_alist_rindex(i, ctx, plugin_ctx_list) {
         if (trigger_plugin_event(jcr, eventType, &event, ctx, value, plugin_ctx_list, &i, &rc)) {
            break;
         }
      }
   } else {
      bpContext *ctx;

      foreach_alist_index(i, ctx, plugin_ctx_list) {
         if (trigger_plugin_event(jcr, eventType, &event, ctx, value, plugin_ctx_list, &i, &rc)) {
            break;
         }
      }
   }

   if (jcr->IsJobCanceled()) {
      Dmsg0(debuglevel, "Cancel return from GeneratePluginEvent\n");
      rc = bRC_Cancel;
   }

bail_out:
   return rc;
}

/**
 * Print to file the plugin info.
 */
void DumpSdPlugin(Plugin *plugin, FILE *fp)
{
   genpInfo *info;

   if (!plugin) {
      return ;
   }

   info = (genpInfo *) plugin->pinfo;
   fprintf(fp, "\tversion=%d\n", info->version);
   fprintf(fp, "\tdate=%s\n", NPRTB(info->plugin_date));
   fprintf(fp, "\tmagic=%s\n", NPRTB(info->plugin_magic));
   fprintf(fp, "\tauthor=%s\n", NPRTB(info->plugin_author));
   fprintf(fp, "\tlicence=%s\n", NPRTB(info->plugin_license));
   fprintf(fp, "\tversion=%s\n", NPRTB(info->plugin_version));
   fprintf(fp, "\tdescription=%s\n", NPRTB(info->plugin_description));
}

static void DumpSdPlugins(FILE *fp)
{
   DumpPlugins(sd_plugin_list, fp);
}

/**
 * This entry point is called internally by Bareos to ensure
 *  that the plugin IO calls come into this code.
 */
void LoadSdPlugins(const char *plugin_dir, alist *plugin_names)
{
   Plugin *plugin;
   int i;

   Dmsg0(debuglevel, "Load sd plugins\n");
   if (!plugin_dir) {
      Dmsg0(debuglevel, "No sd plugin dir!\n");
      return;
   }
   sd_plugin_list = New(alist(10, not_owned_by_alist));
   if (!LoadPlugins((void *)&binfo, (void *)&bfuncs, sd_plugin_list,
                     plugin_dir, plugin_names, plugin_type, IsPluginCompatible)) {
      /*
       * Either none found, or some error
       */
      if (sd_plugin_list->size() == 0) {
         delete sd_plugin_list;
         sd_plugin_list = NULL;
         Dmsg0(debuglevel, "No plugins loaded\n");
         return;
      }
   }
   /*
    * Verify that the plugin is acceptable, and print information about it.
    */
   foreach_alist_index(i, plugin, sd_plugin_list) {
      Dmsg1(debuglevel, "Loaded plugin: %s\n", plugin->file);
   }

   Dmsg1(debuglevel, "num plugins=%d\n", sd_plugin_list->size());
   DbgPluginAddHook(DumpSdPlugin);
   DbgPrintPluginAddHook(DumpSdPlugins);
}

void UnloadSdPlugins(void)
{
   UnloadPlugins(sd_plugin_list);
   delete sd_plugin_list;
   sd_plugin_list = NULL;
}

int ListSdPlugins(PoolMem &msg)
{
   return ListPlugins(sd_plugin_list, msg);
}

/**
 * Check if a plugin is compatible.  Called by the load_plugin function
 *  to allow us to verify the plugin.
 */
static bool IsPluginCompatible(Plugin *plugin)
{
   genpInfo *info = (genpInfo *)plugin->pinfo;
   Dmsg0(50, "IsPluginCompatible called\n");
   if (debug_level >= 50) {
      DumpSdPlugin(plugin, stdin);
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
   if (!Bstrcasecmp(info->plugin_license, "Bareos AGPLv3") &&
       !Bstrcasecmp(info->plugin_license, "AGPLv3")) {
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

/**
 * Instantiate a new plugin instance.
 */
static inline bpContext *instantiate_plugin(JobControlRecord *jcr, Plugin *plugin, uint32_t instance)
{
   bpContext *ctx;
   b_plugin_ctx *b_ctx;

   b_ctx = (b_plugin_ctx *)malloc(sizeof(b_plugin_ctx));
   memset(b_ctx, 0, sizeof(b_plugin_ctx));
   b_ctx->jcr = jcr;
   b_ctx->plugin = plugin;

   Dmsg2(debuglevel, "Instantiate dir-plugin_ctx_list=%p JobId=%d\n", jcr->plugin_ctx_list, jcr->JobId);

   ctx = (bpContext *)malloc(sizeof(bpContext));
   ctx->instance = instance;
   ctx->plugin = plugin;
   ctx->bContext = (void *)b_ctx;
   ctx->pContext = NULL;

   jcr->plugin_ctx_list->append(ctx);

   if (SdplugFunc(plugin)->newPlugin(ctx) != bRC_OK) {
      b_ctx->disabled = true;
   }

   return ctx;
}

/**
 * Send a bsdEventNewPluginOptions event to all plugins configured in jcr->plugin_options.
 */
void DispatchNewPluginOptions(JobControlRecord *jcr)
{
   int i, j, len;
   Plugin *plugin;
   bpContext *ctx;
   uint32_t instance;
   bsdEvent event;
   bsdEventType eventType;
   char *bp, *plugin_name, *option;
   const char *plugin_options;
   PoolMem priv_plugin_options(PM_MESSAGE);

   if (!sd_plugin_list || sd_plugin_list->empty()) {
      return;
   }

   if (jcr->plugin_options &&
       jcr->plugin_options->size()) {

      eventType = bsdEventNewPluginOptions;
      event.eventType = eventType;

      foreach_alist_index(i, plugin_options, jcr->plugin_options) {
         /*
          * Make a private copy of plugin options.
          */
         PmStrcpy(priv_plugin_options, plugin_options);

         plugin_name = priv_plugin_options.c_str();
         if (!(bp = strchr(plugin_name, ':'))) {
            Jmsg(NULL, M_ERROR, 0, _("Illegal SD plugin options encountered, %s skipping\n"),
                 priv_plugin_options.c_str());
            continue;
         }
         *bp++ = '\0';

         /*
          * See if there is any instance named in the options string.
          */
         instance = 0;
         option = bp;
         while (option) {
            bp = strchr(bp, ':');
            if (bp) {
               *bp++ = '\0';
            }

            if (bstrncasecmp(option, "instance=", 9)) {
               instance = str_to_int64(option + 9);
               break;
            }

            option = bp;
         }

         if (instance < LOWEST_PLUGIN_INSTANCE || instance > HIGHEST_PLUGIN_INSTANCE) {
            Jmsg(NULL, M_ERROR, 0, _("Illegal SD plugin options encountered, %s instance %d skipping\n"),
                 plugin_options, instance);
            continue;
         }

         len = strlen(plugin_name);

         /*
          * See if this plugin options are for an already instantiated plugin instance.
          */
         foreach_alist(ctx, jcr->plugin_ctx_list) {
            if (ctx->instance == instance &&
                ctx->plugin->file_len == len &&
                bstrncasecmp(ctx->plugin->file, plugin_name, len)) {
               break;
            }
         }

         /*
          * Found a context in the previous loop ?
          */
         if (!ctx) {
            foreach_alist_index(j, plugin, sd_plugin_list) {
               if (plugin->file_len == len && bstrncasecmp(plugin->file, plugin_name, len)) {
                  ctx = instantiate_plugin(jcr, plugin, instance);
                  break;
               }
            }
         }

         if (ctx) {
            trigger_plugin_event(jcr, eventType, &event, ctx, (void *)plugin_options, NULL, NULL, NULL);
         }
      }
   }
}

/**
 * Create a new instance of each plugin for this Job
 */
void NewPlugins(JobControlRecord *jcr)
{
   Plugin *plugin;
   int i, num;

   Dmsg0(debuglevel, "=== enter NewPlugins ===\n");
   if (!sd_plugin_list) {
      Dmsg0(debuglevel, "No sd plugin list!\n");
      return;
   }
   if (jcr->IsJobCanceled()) {
      return;
   }
   /*
    * If plugins already loaded, just return
    */
   if (jcr->plugin_ctx_list) {
      return;
   }

   num = sd_plugin_list->size();
   Dmsg1(debuglevel, "sd-plugin-list size=%d\n", num);
   if (num == 0) {
      return;
   }

   jcr->plugin_ctx_list = New(alist(10, owned_by_alist));
   foreach_alist_index(i, plugin, sd_plugin_list) {
      /*
       * Start a new instance of each plugin
       */
      instantiate_plugin(jcr, plugin, 0);
   }
}

/**
 * Free the plugin instances for this Job
 */
void FreePlugins(JobControlRecord *jcr)
{
   bpContext *ctx;

   if (!sd_plugin_list || !jcr->plugin_ctx_list) {
      return;
   }

   Dmsg2(debuglevel, "Free instance dir-plugin_ctx_list=%p JobId=%d\n", jcr->plugin_ctx_list, jcr->JobId);
   foreach_alist(ctx, jcr->plugin_ctx_list) {
      /*
       * Free the plugin instance
       */
      SdplugFunc(ctx->plugin)->freePlugin(ctx);
      free(ctx->bContext);                   /* Free BAREOS private context */
   }

   delete jcr->plugin_ctx_list;
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
   JobControlRecord *jcr = NULL;
   bRC retval = bRC_OK;

   if (!value) {
      return bRC_Error;
   }

   switch (var) {               /* General variables, no need of ctx */
   case bsdVarCompatible:
      *((bool *)value) = me->compatible;
      Dmsg1(debuglevel, "sd-plugin: return bsdVarCompatible=%s\n", (me->compatible) ? "true" : "false");
      break;
   case bsdVarPluginDir:
      *((char **)value) = me->plugin_directory;
      Dmsg1(debuglevel, "sd-plugin: return bsdVarPluginDir=%s\n", me->plugin_directory);
      break;
   default:
      if (!ctx) {
         return bRC_Error;
      }

      jcr = ((b_plugin_ctx *)ctx->bContext)->jcr;
      if (!jcr) {
         return bRC_Error;
      }
      break;
   }

   if (jcr) {
      switch (var) {
      case bsdVarJob:
         *((char **)value) = jcr->job_name;
         Dmsg1(debuglevel, "sd-plugin: return bsdVarJobName=%s\n", NPRT(*((char **)value)));
         break;
      case bsdVarLevel:
         *((int *)value) = jcr->getJobLevel();
         Dmsg1(debuglevel, "sd-plugin: return bsdVarLevel=%c\n", jcr->getJobLevel());
         break;
      case bsdVarType:
         *((int *)value) = jcr->getJobType();
         Dmsg1(debuglevel, "sd-plugin: return bsdVarType=%c\n", jcr->getJobType());
         break;
      case bsdVarJobId:
         *((int *)value) = jcr->JobId;
         Dmsg1(debuglevel, "sd-plugin: return bsdVarJobId=%d\n", jcr->JobId);
         break;
      case bsdVarClient:
         *((char **)value) = jcr->client_name;
         Dmsg1(debuglevel, "sd-plugin: return bsdVarClient=%s\n", NPRT(*((char **)value)));
         break;
      case bsdVarPool:
         if (jcr->dcr) {
            *((char **)value) = jcr->dcr->pool_name;
            Dmsg1(debuglevel, "sd-plugin: return bsdVarPool=%s\n", NPRT(*((char **)value)));
         } else {
            retval = bRC_Error;
         }
         break;
      case bsdVarPoolType:
         if (jcr->dcr) {
            *((char **)value) = jcr->dcr->pool_type;
            Dmsg1(debuglevel, "sd-plugin: return bsdVarPoolType=%s\n", NPRT(*((char **)value)));
         } else {
            retval = bRC_Error;
         }
         break;
      case bsdVarStorage:
         if (jcr->dcr && jcr->dcr->device) {
            *((char **)value) = jcr->dcr->device->name();
            Dmsg1(debuglevel, "sd-plugin: return bsdVarStorage=%s\n", NPRT(*((char **)value)));
         } else {
            retval = bRC_Error;
         }
         break;
      case bsdVarMediaType:
         if (jcr->dcr) {
            *((char **)value) = jcr->dcr->media_type;
            Dmsg1(debuglevel, "sd-plugin: return bsdVarMediaType=%s\n", NPRT(*((char **)value)));
         } else {
            retval = bRC_Error;
         }
         break;
      case bsdVarJobName:
         *((char **)value) = jcr->Job;
         Dmsg1(debuglevel, "sd-plugin: return bsdVarJobName=%s\n", NPRT(*((char **)value)));
         break;
      case bsdVarJobStatus:
         *((int *)value) = jcr->JobStatus;
         Dmsg1(debuglevel, "sd-plugin: return bsdVarJobStatus=%c\n", jcr->JobStatus);
         break;
      case bsdVarVolumeName:
         if (jcr->dcr) {
            *((char **)value) = jcr->dcr->VolumeName;
            Dmsg1(debuglevel, "sd-plugin: return bsdVarVolumeName=%s\n", NPRT(*((char **)value)));
         } else {
            retval = bRC_Error;
         }
         Dmsg1(debuglevel, "sd-plugin: return bsdVarVolumeName=%s\n", jcr->VolumeName);
         break;
      case bsdVarJobErrors:
         *((int *)value) = jcr->JobErrors;
         Dmsg1(debuglevel, "sd-plugin: return bsdVarJobErrors=%d\n", jcr->JobErrors);
         break;
      case bsdVarJobFiles:
         *((int *)value) = jcr->JobFiles;
         Dmsg1(debuglevel, "sd-plugin: return bsdVarJobFiles=%d\n", jcr->JobFiles);
         break;
      case bsdVarJobBytes:
         *((uint64_t *)value) = jcr->JobBytes;
         Dmsg1(debuglevel, "sd-plugin: return bsdVarJobBytes=%d\n", jcr->JobBytes);
         break;
      default:
         break;
      }
   }

   return retval;
}

static bRC bareosSetValue(bpContext *ctx, bsdwVariable var, void *value)
{
   JobControlRecord *jcr;
   if (!value || !ctx) {
      return bRC_Error;
   }

   jcr = ((b_plugin_ctx *)ctx->bContext)->jcr;
   if (!jcr) {
      return bRC_Error;
   }

   Dmsg1(debuglevel, "sd-plugin: bareosSetValue var=%d\n", var);
   switch (var) {
   case bsdwVarVolumeName:
      PmStrcpy(jcr->VolumeName, ((char *)value));
      break;
   case bsdwVarPriority:
      jcr->JobPriority = *((int *)value);
      break;
   case bsdwVarJobLevel:
      jcr->setJobLevel(*((int *)value));
      break;
   default:
      break;
   }

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
      Dmsg1(debuglevel, "sd-plugin: Plugin registered event=%u\n", event);
      SetBit(event, b_ctx->events);
   }
   va_end(args);
   return bRC_OK;
}

static bRC bareosUnRegisterEvents(bpContext *ctx, int nr_events, ...)
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
      Dmsg1(debuglevel, "sd-plugin: Plugin unregistered event=%u\n", event);
      ClearBit(event, b_ctx->events);
   }
   va_end(args);
   return bRC_OK;
}

static bRC bareosGetInstanceCount(bpContext *ctx, int *ret)
{
   int cnt;
   JobControlRecord *jcr, *njcr;
   bpContext *nctx;
   b_plugin_ctx *bctx;
   bRC retval = bRC_Error;

   if (!IsCtxGood(ctx, jcr, bctx)) {
      goto bail_out;
   }

   P(mutex);

   cnt = 0;
   foreach_jcr(njcr) {
      if (jcr->plugin_ctx_list) {
         foreach_alist(nctx, jcr->plugin_ctx_list) {
            if (nctx->plugin == bctx->plugin) {
               cnt++;
            }
         }
      }
   }
   endeach_jcr(njcr);

   V(mutex);

   *ret = cnt;
   retval = bRC_OK;

bail_out:
   return retval;
}

static bRC bareosJobMsg(bpContext *ctx, const char *file, int line,
                        int type, utime_t mtime, const char *fmt, ...)
{
   JobControlRecord *jcr;
   va_list arg_ptr;
   PoolMem buffer(PM_MESSAGE);

   if (ctx) {
      jcr = ((b_plugin_ctx *)ctx->bContext)->jcr;
   } else {
      jcr = NULL;
   }

   va_start(arg_ptr, fmt);
   buffer.Bvsprintf(fmt, arg_ptr);
   va_end(arg_ptr);
   Jmsg(jcr, type, mtime, "%s", buffer.c_str());

   return bRC_OK;
}

static bRC bareosDebugMsg(bpContext *ctx, const char *file, int line,
                          int level, const char *fmt, ...)
{
   va_list arg_ptr;
   PoolMem buffer(PM_MESSAGE);

   va_start(arg_ptr, fmt);
   buffer.Bvsprintf(fmt, arg_ptr);
   va_end(arg_ptr);
   d_msg(file, line, level, "%s", buffer.c_str());

   return bRC_OK;
}

static char *bareosEditDeviceCodes(DeviceControlRecord *dcr, POOLMEM *&omsg,
                                   const char *imsg, const char *cmd)
{
   return edit_device_codes(dcr, omsg, imsg, cmd);
}

static char *bareosLookupCryptoKey(const char *VolumeName)
{
   return lookup_crypto_cache_entry(VolumeName);
}

static bool bareosUpdateVolumeInfo(DeviceControlRecord *dcr)
{
   return dcr->DirGetVolumeInfo(GET_VOL_INFO_FOR_READ);
}

static void bareosUpdateTapeAlert(DeviceControlRecord *dcr, uint64_t flags)
{
   utime_t now;
   now = (utime_t)time(NULL);

   UpdateDeviceTapealert(dcr->device->name(), flags, now);
}

static DeviceRecord *bareosNewRecord(bool with_data)
{
   return new_record(with_data);
}

static void bareosCopyRecordState(DeviceRecord *dst, DeviceRecord *src)
{
   CopyRecordState(dst, src);
}

static void bareosFreeRecord(DeviceRecord *rec)
{
   FreeRecord(rec);
}

#ifdef TEST_PROGRAM
int main(int argc, char *argv[])
{
   char plugin_dir[1000];
   JobControlRecord mjcr1, mjcr2;
   JobControlRecord *jcr1 = &mjcr1;
   JobControlRecord *jcr2 = &mjcr2;

   MyNameIs(argc, argv, "plugtest");
   InitMsg(NULL, NULL);

   OSDependentInit();

   if (argc != 1) {
      bstrncpy(plugin_dir, argv[1], sizeof(plugin_dir));
   } else {
      getcwd(plugin_dir, sizeof(plugin_dir)-1);
   }
   LoadSdPlugins(plugin_dir, NULL);

   jcr1->JobId = 111;
   NewPlugins(jcr1);

   jcr2->JobId = 222;
   NewPlugins(jcr2);

   GeneratePluginEvent(jcr1, bsdEventJobStart, (void *)"Start Job 1");
   GeneratePluginEvent(jcr1, bsdEventJobEnd);
   GeneratePluginEvent(jcr2, bsdEventJobStart, (void *)"Start Job 1");
   FreePlugins(jcr1);
   GeneratePluginEvent(jcr2, bsdEventJobEnd);
   FreePlugins(jcr2);

   UnloadSdPlugins();

   TermMsg();
   CloseMemoryPool();
   LmgrCleanupMain();
   sm_dump(false);
   exit(0);
}

#endif /* TEST_PROGRAM */

} /* namespace storagedaemon */
