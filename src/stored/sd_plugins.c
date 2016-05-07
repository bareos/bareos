/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2007-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2014 Bareos GmbH & Co. KG

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
#ifdef HAVE_WIN32
const char *plugin_type = "-sd.dll";
#else
const char *plugin_type = "-sd.so";
#endif
static alist *sd_plugin_list = NULL;

/* Forward referenced functions */
static bRC bareosGetValue(bpContext *ctx, bsdrVariable var, void *value);
static bRC bareosSetValue(bpContext *ctx, bsdwVariable var, void *value);
static bRC bareosRegisterEvents(bpContext *ctx, int nr_events, ...);
static bRC bareosUnRegisterEvents(bpContext *ctx, int nr_events, ...);
static bRC bareosJobMsg(bpContext *ctx, const char *file, int line,
                        int type, utime_t mtime, const char *fmt, ...);
static bRC bareosDebugMsg(bpContext *ctx, const char *file, int line,
                          int level, const char *fmt, ...);
static char *bareosEditDeviceCodes(DCR *dcr, POOLMEM *&omsg,
                                   const char *imsg, const char *cmd);
static char *bareosLookupCryptoKey(const char *VolumeName);
static bool bareosUpdateVolumeInfo(DCR *dcr);
static void bareosUpdateTapeAlert(DCR *dcr, uint64_t flags);
static DEV_RECORD *bareosNewRecord(bool with_data);
static void bareosCopyRecordState(DEV_RECORD *dst, DEV_RECORD *src);
static void bareosFreeRecord(DEV_RECORD *rec);
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
   bareosUnRegisterEvents,
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
      return false;
   }
   b_ctx = (b_plugin_ctx *)ctx->bContext;
   if (!b_ctx) {
      return false;
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
   if (!b_ctx) {
      return true;
   }
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
char *edit_device_codes(DCR *dcr, POOLMEM *&omsg, const char *imsg, const char *cmd)
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
      pm_strcat(omsg, (char *)str);
      Dmsg1(1800, "omsg=%s\n", omsg);
   }
   Dmsg1(800, "omsg=%s\n", omsg);

   return omsg;
}

static inline bool trigger_plugin_event(JCR *jcr, bsdEventType eventType,
                                        bsdEvent *event, bpContext *ctx,
                                        void *value, alist *plugin_ctx_list,
                                        int *index, bRC *rc)
{
   bool stop = false;

   if (!is_event_enabled(ctx, eventType)) {
      Dmsg1(dbglvl, "Event %d disabled for this plugin.\n", eventType);
      goto bail_out;
   }

   if (is_plugin_disabled(ctx)) {
      Dmsg0(dbglvl, "Plugin disabled.\n");
      goto bail_out;
   }

   /*
    * See if we should care about the return code.
    */
   if (rc) {
      *rc = sdplug_func(ctx->plugin)->handlePluginEvent(ctx, event, value);
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
            unload_plugin(plugin_ctx_list, ctx->plugin, *index);
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
      sdplug_func(ctx->plugin)->handlePluginEvent(ctx, event, value);
   }

bail_out:
   return stop;
}

/*
 * Create a plugin event
 */
bRC generate_plugin_event(JCR *jcr, bsdEventType eventType, void *value, bool reverse)
{
   int i;
   bsdEvent event;
   alist *plugin_ctx_list;
   bRC rc = bRC_OK;

   if (!sd_plugin_list) {
      Dmsg0(dbglvl, "No bplugin_list: generate_plugin_event ignored.\n");
      goto bail_out;
   }

   if (!jcr) {
      Dmsg0(dbglvl, "No jcr: generate_plugin_event ignored.\n");
      goto bail_out;
   }

   /*
    * Return if no plugins loaded
    */
   if (!jcr->plugin_ctx_list) {
      Dmsg0(dbglvl, "No plugin_ctx_list: generate_plugin_event ignored.\n");
      goto bail_out;
   }

   plugin_ctx_list = jcr->plugin_ctx_list;
   event.eventType = eventType;

   Dmsg2(dbglvl, "sd-plugin_ctx_list=%p JobId=%d\n", plugin_ctx_list, jcr->JobId);

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

   if (jcr->is_job_canceled()) {
      Dmsg0(dbglvl, "Cancel return from generate_plugin_event\n");
      rc = bRC_Cancel;
   }

bail_out:
   return rc;
}

/*
 * Print to file the plugin info.
 */
void dump_sd_plugin(Plugin *plugin, FILE *fp)
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

static void dump_sd_plugins(FILE *fp)
{
   dump_plugins(sd_plugin_list, fp);
}

/**
 * This entry point is called internally by Bareos to ensure
 *  that the plugin IO calls come into this code.
 */
void load_sd_plugins(const char *plugin_dir, alist *plugin_names)
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
 * Instantiate a new plugin instance.
 */
static inline bpContext *instantiate_plugin(JCR *jcr, Plugin *plugin, uint32_t instance)
{
   bpContext *ctx;
   b_plugin_ctx *b_ctx;

   b_ctx = (b_plugin_ctx *)malloc(sizeof(b_plugin_ctx));
   memset(b_ctx, 0, sizeof(b_plugin_ctx));
   b_ctx->jcr = jcr;

   Dmsg2(dbglvl, "Instantiate dir-plugin_ctx_list=%p JobId=%d\n", jcr->plugin_ctx_list, jcr->JobId);

   ctx = (bpContext *)malloc(sizeof(bpContext));
   ctx->instance = instance;
   ctx->plugin = plugin;
   ctx->bContext = (void *)b_ctx;
   ctx->pContext = NULL;

   jcr->plugin_ctx_list->append(ctx);

   if (sdplug_func(plugin)->newPlugin(ctx) != bRC_OK) {
      b_ctx->disabled = true;
   }

   return ctx;
}

/*
 * Send a bsdEventNewPluginOptions event to all plugins configured in jcr->plugin_options.
 */
void dispatch_new_plugin_options(JCR *jcr)
{
   int i, j, len;
   Plugin *plugin;
   bpContext *ctx;
   uint32_t instance;
   bsdEvent event;
   bsdEventType eventType;
   char *bp, *plugin_name, *option;
   const char *plugin_options;
   POOL_MEM priv_plugin_options(PM_MESSAGE);

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
         pm_strcpy(priv_plugin_options, plugin_options);

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

   jcr->plugin_ctx_list = New(alist(10, owned_by_alist));
   foreach_alist_index(i, plugin, sd_plugin_list) {
      /*
       * Start a new instance of each plugin
       */
      instantiate_plugin(jcr, plugin, 0);
   }
}

/*
 * Free the plugin instances for this Job
 */
void free_plugins(JCR *jcr)
{
   bpContext *ctx;

   if (!sd_plugin_list || !jcr->plugin_ctx_list) {
      return;
   }

   Dmsg2(dbglvl, "Free instance dir-plugin_ctx_list=%p JobId=%d\n", jcr->plugin_ctx_list, jcr->JobId);
   foreach_alist(ctx, jcr->plugin_ctx_list) {
      /*
       * Free the plugin instance
       */
      sdplug_func(ctx->plugin)->freePlugin(ctx);
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
   JCR *jcr = NULL;
   bRC ret = bRC_OK;

   if (!value) {
      return bRC_Error;
   }

   switch (var) {               /* General variables, no need of ctx */
   case bsdVarCompatible:
      *((bool *)value) = me->compatible;
      Dmsg1(dbglvl, "sd-plugin: return bsdVarCompatible=%s\n", (me->compatible) ? "true" : "false");
      break;
   case bsdVarPluginDir:
      *((char **)value) = me->plugin_directory;
      Dmsg1(dbglvl, "sd-plugin: return bsdVarPluginDir=%s\n", me->plugin_directory);
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
         Dmsg1(dbglvl, "sd-plugin: return bsdVarJobName=%s\n", NPRT(*((char **)value)));
         break;
      case bsdVarLevel:
         *((int *)value) = jcr->getJobLevel();
         Dmsg1(dbglvl, "sd-plugin: return bsdVarLevel=%c\n", jcr->getJobLevel());
         break;
      case bsdVarType:
         *((int *)value) = jcr->getJobType();
         Dmsg1(dbglvl, "sd-plugin: return bsdVarType=%c\n", jcr->getJobType());
         break;
      case bsdVarJobId:
         *((int *)value) = jcr->JobId;
         Dmsg1(dbglvl, "sd-plugin: return bsdVarJobId=%d\n", jcr->JobId);
         break;
      case bsdVarClient:
         *((char **)value) = jcr->client_name;
         Dmsg1(dbglvl, "sd-plugin: return bsdVarClient=%s\n", NPRT(*((char **)value)));
         break;
      case bsdVarPool:
         if (jcr->dcr) {
            *((char **)value) = jcr->dcr->pool_name;
            Dmsg1(dbglvl, "sd-plugin: return bsdVarPool=%s\n", NPRT(*((char **)value)));
         } else {
            ret = bRC_Error;
         }
         break;
      case bsdVarPoolType:
         if (jcr->dcr) {
            *((char **)value) = jcr->dcr->pool_type;
            Dmsg1(dbglvl, "sd-plugin: return bsdVarPoolType=%s\n", NPRT(*((char **)value)));
         } else {
            ret = bRC_Error;
         }
         break;
      case bsdVarStorage:
         if (jcr->dcr && jcr->dcr->device) {
            *((char **)value) = jcr->dcr->device->name();
            Dmsg1(dbglvl, "sd-plugin: return bsdVarStorage=%s\n", NPRT(*((char **)value)));
         } else {
            ret = bRC_Error;
         }
         break;
      case bsdVarMediaType:
         if (jcr->dcr) {
            *((char **)value) = jcr->dcr->media_type;
            Dmsg1(dbglvl, "sd-plugin: return bsdVarMediaType=%s\n", NPRT(*((char **)value)));
         } else {
            ret = bRC_Error;
         }
         break;
      case bsdVarJobName:
         *((char **)value) = jcr->Job;
         Dmsg1(dbglvl, "sd-plugin: return bsdVarJobName=%s\n", NPRT(*((char **)value)));
         break;
      case bsdVarJobStatus:
         *((int *)value) = jcr->JobStatus;
         Dmsg1(dbglvl, "sd-plugin: return bsdVarJobStatus=%c\n", jcr->JobStatus);
         break;
      case bsdVarVolumeName:
         if (jcr->dcr) {
            *((char **)value) = jcr->dcr->VolumeName;
            Dmsg1(dbglvl, "sd-plugin: return bsdVarVolumeName=%s\n", NPRT(*((char **)value)));
         } else {
            ret = bRC_Error;
         }
         Dmsg1(dbglvl, "sd-plugin: return bsdVarVolumeName=%s\n", jcr->VolumeName);
         break;
      case bsdVarJobErrors:
         *((int *)value) = jcr->JobErrors;
         Dmsg1(dbglvl, "sd-plugin: return bsdVarJobErrors=%d\n", jcr->JobErrors);
         break;
      case bsdVarJobFiles:
         *((int *)value) = jcr->JobFiles;
         Dmsg1(dbglvl, "sd-plugin: return bsdVarJobFiles=%d\n", jcr->JobFiles);
         break;
      case bsdVarJobBytes:
         *((uint64_t *)value) = jcr->JobBytes;
         Dmsg1(dbglvl, "sd-plugin: return bsdVarJobBytes=%d\n", jcr->JobBytes);
         break;
      default:
         break;
      }
   }

   return ret;
}

static bRC bareosSetValue(bpContext *ctx, bsdwVariable var, void *value)
{
   JCR *jcr;
   if (!value || !ctx) {
      return bRC_Error;
   }

   jcr = ((b_plugin_ctx *)ctx->bContext)->jcr;
   if (!jcr) {
      return bRC_Error;
   }

   Dmsg1(dbglvl, "sd-plugin: bareosSetValue var=%d\n", var);
   switch (var) {
   case bsdwVarVolumeName:
      pm_strcpy(jcr->VolumeName, ((char *)value));
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
      Dmsg1(dbglvl, "sd-plugin: Plugin registered event=%u\n", event);
      set_bit(event, b_ctx->events);
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
      Dmsg1(dbglvl, "sd-plugin: Plugin unregistered event=%u\n", event);
      clear_bit(event, b_ctx->events);
   }
   va_end(args);
   return bRC_OK;
}

static bRC bareosJobMsg(bpContext *ctx, const char *file, int line,
                        int type, utime_t mtime, const char *fmt, ...)
{
   JCR *jcr;
   va_list arg_ptr;
   POOL_MEM buffer(PM_MESSAGE);

   if (ctx) {
      jcr = ((b_plugin_ctx *)ctx->bContext)->jcr;
   } else {
      jcr = NULL;
   }

   va_start(arg_ptr, fmt);
   buffer.bvsprintf(fmt, arg_ptr);
   va_end(arg_ptr);
   Jmsg(jcr, type, mtime, "%s", buffer.c_str());

   return bRC_OK;
}

static bRC bareosDebugMsg(bpContext *ctx, const char *file, int line,
                          int level, const char *fmt, ...)
{
   va_list arg_ptr;
   POOL_MEM buffer(PM_MESSAGE);

   va_start(arg_ptr, fmt);
   buffer.bvsprintf(fmt, arg_ptr);
   va_end(arg_ptr);
   d_msg(file, line, level, "%s", buffer.c_str());

   return bRC_OK;
}

static char *bareosEditDeviceCodes(DCR *dcr, POOLMEM *&omsg,
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
   return dcr->dir_get_volume_info(GET_VOL_INFO_FOR_READ);
}

static void bareosUpdateTapeAlert(DCR *dcr, uint64_t flags)
{
   utime_t now;
   now = (utime_t)time(NULL);

   update_device_tapealert(dcr->device->name(), flags, now);
}

static DEV_RECORD *bareosNewRecord(bool with_data)
{
   return new_record(with_data);
}

static void bareosCopyRecordState(DEV_RECORD *dst, DEV_RECORD *src)
{
   copy_record_state(dst, src);
}

static void bareosFreeRecord(DEV_RECORD *rec)
{
   free_record(rec);
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
