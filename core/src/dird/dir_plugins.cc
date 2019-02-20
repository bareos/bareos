/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2007-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2016 Planets Communications B.V.
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
/** @file
 * BAREOS pluginloader
 */
#include "include/bareos.h"
#include "dird.h"
#include "dird/dird_globals.h"
#include "dir_plugins.h"
#include "lib/edit.h"

namespace directordaemon {

const int debuglevel = 150;
#ifdef HAVE_WIN32
const char* plugin_type = "-dir.dll";
#else
const char* plugin_type = "-dir.so";
#endif

static alist* dird_plugin_list;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

/* Forward referenced functions */
static bRC bareosGetValue(bpContext* ctx, brDirVariable var, void* value);
static bRC bareosSetValue(bpContext* ctx, bwDirVariable var, void* value);
static bRC bareosRegisterEvents(bpContext* ctx, int nr_events, ...);
static bRC bareosUnRegisterEvents(bpContext* ctx, int nr_events, ...);
static bRC bareosGetInstanceCount(bpContext* ctx, int* ret);
static bRC bareosJobMsg(bpContext* ctx,
                        const char* file,
                        int line,
                        int type,
                        utime_t mtime,
                        const char* fmt,
                        ...);
static bRC bareosDebugMsg(bpContext* ctx,
                          const char* file,
                          int line,
                          int level,
                          const char* fmt,
                          ...);
static bool IsPluginCompatible(Plugin* plugin);

/* BAREOS info */
static bDirInfo binfo = {sizeof(bDirFuncs), DIR_PLUGIN_INTERFACE_VERSION};

/* BAREOS entry points */
static bDirFuncs bfuncs = {sizeof(bDirFuncs),      DIR_PLUGIN_INTERFACE_VERSION,
                           bareosRegisterEvents,   bareosUnRegisterEvents,
                           bareosGetInstanceCount, bareosGetValue,
                           bareosSetValue,         bareosJobMsg,
                           bareosDebugMsg};

/*
 * BAREOS private context
 */
struct b_plugin_ctx {
  JobControlRecord* jcr;                         /* jcr for plugin */
  bRC rc;                                        /* last return code */
  bool disabled;                                 /* set if plugin disabled */
  char events[NbytesForBits(DIR_NR_EVENTS + 1)]; /* enabled events bitmask */
  Plugin* plugin; /* pointer to plugin of which this is an instance off */
};

static inline bool IsEventEnabled(bpContext* ctx, bDirEventType eventType)
{
  b_plugin_ctx* b_ctx;
  if (!ctx) { return false; }
  b_ctx = (b_plugin_ctx*)ctx->bContext;
  if (!b_ctx) { return false; }
  return BitIsSet(eventType, b_ctx->events);
}

static inline bool IsPluginDisabled(bpContext* ctx)
{
  b_plugin_ctx* b_ctx;
  if (!ctx) { return true; }
  b_ctx = (b_plugin_ctx*)ctx->bContext;
  if (!b_ctx) { return true; }
  return b_ctx->disabled;
}

static bool IsCtxGood(bpContext* ctx,
                      JobControlRecord*& jcr,
                      b_plugin_ctx*& bctx)
{
  if (!ctx) { return false; }

  bctx = (b_plugin_ctx*)ctx->bContext;
  if (!bctx) { return false; }

  jcr = bctx->jcr;
  if (!jcr) { return false; }

  return true;
}

static inline bool trigger_plugin_event(JobControlRecord* jcr,
                                        bDirEventType eventType,
                                        bDirEvent* event,
                                        bpContext* ctx,
                                        void* value,
                                        alist* plugin_ctx_list,
                                        int* index,
                                        bRC* rc)
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
    *rc = DirplugFunc(ctx->plugin)->handlePluginEvent(ctx, event, value);
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
    DirplugFunc(ctx->plugin)->handlePluginEvent(ctx, event, value);
  }

bail_out:
  return stop;
}

/*
 * Create a plugin event
 */
bRC GeneratePluginEvent(JobControlRecord* jcr,
                        bDirEventType eventType,
                        void* value,
                        bool reverse)
{
  int i;
  bDirEvent event;
  alist* plugin_ctx_list;
  bRC rc = bRC_OK;

  if (!dird_plugin_list) {
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

  Dmsg2(debuglevel, "dir-plugin_ctx_list=%p JobId=%d\n", plugin_ctx_list,
        jcr->JobId);

  /*
   * See if we need to trigger the loaded plugins in reverse order.
   */
  if (reverse) {
    bpContext* ctx;

    foreach_alist_rindex (i, ctx, plugin_ctx_list) {
      if (trigger_plugin_event(jcr, eventType, &event, ctx, value,
                               plugin_ctx_list, &i, &rc)) {
        break;
      }
    }
  } else {
    bpContext* ctx;

    foreach_alist_index (i, ctx, plugin_ctx_list) {
      if (trigger_plugin_event(jcr, eventType, &event, ctx, value,
                               plugin_ctx_list, &i, &rc)) {
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
void DumpDirPlugin(Plugin* plugin, FILE* fp)
{
  genpInfo* info;

  if (!plugin) { return; }

  info = (genpInfo*)plugin->pinfo;
  fprintf(fp, "\tversion=%d\n", info->version);
  fprintf(fp, "\tdate=%s\n", NPRTB(info->plugin_date));
  fprintf(fp, "\tmagic=%s\n", NPRTB(info->plugin_magic));
  fprintf(fp, "\tauthor=%s\n", NPRTB(info->plugin_author));
  fprintf(fp, "\tlicence=%s\n", NPRTB(info->plugin_license));
  fprintf(fp, "\tversion=%s\n", NPRTB(info->plugin_version));
  fprintf(fp, "\tdescription=%s\n", NPRTB(info->plugin_description));
}

static void DumpDirPlugins(FILE* fp) { DumpPlugins(dird_plugin_list, fp); }

/**
 * This entry point is called internally by BAREOS to ensure
 *  that the plugin IO calls come into this code.
 */
void LoadDirPlugins(const char* plugin_dir, alist* plugin_names)
{
  Plugin* plugin;
  int i;

  Dmsg0(debuglevel, "Load dir plugins\n");
  if (!plugin_dir) {
    Dmsg0(debuglevel, "No dir plugin dir!\n");
    return;
  }

  dird_plugin_list = New(alist(10, not_owned_by_alist));
  if (!LoadPlugins((void*)&binfo, (void*)&bfuncs, dird_plugin_list, plugin_dir,
                   plugin_names, plugin_type, IsPluginCompatible)) {
    /* Either none found, or some error */
    if (dird_plugin_list->size() == 0) {
      delete dird_plugin_list;
      dird_plugin_list = NULL;
      Dmsg0(debuglevel, "No plugins loaded\n");
      return;
    }
  }
  /*
   * Verify that the plugin is acceptable, and print information
   *  about it.
   */
  foreach_alist_index (i, plugin, dird_plugin_list) {
    Dmsg1(debuglevel, "Loaded plugin: %s\n", plugin->file);
  }

  Dmsg1(debuglevel, "num plugins=%d\n", dird_plugin_list->size());
  DbgPluginAddHook(DumpDirPlugin);
  DbgPrintPluginAddHook(DumpDirPlugins);
}

void UnloadDirPlugins(void)
{
  UnloadPlugins(dird_plugin_list);
  delete dird_plugin_list;
  dird_plugin_list = NULL;
}

int ListDirPlugins(PoolMem& msg) { return ListPlugins(dird_plugin_list, msg); }

/**
 * Check if a plugin is compatible.  Called by the load_plugin function
 *  to allow us to verify the plugin.
 */
static bool IsPluginCompatible(Plugin* plugin)
{
  genpInfo* info = (genpInfo*)plugin->pinfo;

  Dmsg0(50, "IsPluginCompatible called\n");
  if (debug_level >= 50) { DumpDirPlugin(plugin, stdin); }
  if (!bstrcmp(info->plugin_magic, DIR_PLUGIN_MAGIC)) {
    Jmsg(NULL, M_ERROR, 0,
         _("Plugin magic wrong. Plugin=%s wanted=%s got=%s\n"), plugin->file,
         DIR_PLUGIN_MAGIC, info->plugin_magic);
    Dmsg3(50, "Plugin magic wrong. Plugin=%s wanted=%s got=%s\n", plugin->file,
          DIR_PLUGIN_MAGIC, info->plugin_magic);

    return false;
  }
  if (info->version != DIR_PLUGIN_INTERFACE_VERSION) {
    Jmsg(NULL, M_ERROR, 0,
         _("Plugin version incorrect. Plugin=%s wanted=%d got=%d\n"),
         plugin->file, DIR_PLUGIN_INTERFACE_VERSION, info->version);
    Dmsg3(50, "Plugin version incorrect. Plugin=%s wanted=%d got=%d\n",
          plugin->file, DIR_PLUGIN_INTERFACE_VERSION, info->version);
    return false;
  }
  if (!Bstrcasecmp(info->plugin_license, "Bareos AGPLv3") &&
      !Bstrcasecmp(info->plugin_license, "AGPLv3")) {
    Jmsg(NULL, M_ERROR, 0,
         _("Plugin license incompatible. Plugin=%s license=%s\n"), plugin->file,
         info->plugin_license);
    Dmsg2(50, "Plugin license incompatible. Plugin=%s license=%s\n",
          plugin->file, info->plugin_license);
    return false;
  }
  if (info->size != sizeof(genpInfo)) {
    Jmsg(NULL, M_ERROR, 0,
         _("Plugin size incorrect. Plugin=%s wanted=%d got=%d\n"), plugin->file,
         sizeof(genpInfo), info->size);
    return false;
  }

  return true;
}

/**
 * Instantiate a new plugin instance.
 */
static inline bpContext* instantiate_plugin(JobControlRecord* jcr,
                                            Plugin* plugin,
                                            uint32_t instance)
{
  bpContext* ctx;
  b_plugin_ctx* b_ctx;

  b_ctx = (b_plugin_ctx*)malloc(sizeof(b_plugin_ctx));
  memset(b_ctx, 0, sizeof(b_plugin_ctx));
  b_ctx->jcr = jcr;
  b_ctx->plugin = plugin;

  Dmsg2(debuglevel, "Instantiate dir-plugin_ctx_list=%p JobId=%d\n",
        jcr->plugin_ctx_list, jcr->JobId);

  ctx = (bpContext*)malloc(sizeof(bpContext));
  ctx->instance = instance;
  ctx->plugin = plugin;
  ctx->bContext = (void*)b_ctx;
  ctx->pContext = NULL;

  jcr->plugin_ctx_list->append(ctx);

  if (DirplugFunc(plugin)->newPlugin(ctx) != bRC_OK) { b_ctx->disabled = true; }

  return ctx;
}

/**
 * Send a bDirEventNewPluginOptions event to all plugins configured in
 * jcr->res.Job.DirPluginOptions
 */
void DispatchNewPluginOptions(JobControlRecord* jcr)
{
  int i, j, len;
  Plugin* plugin;
  bpContext* ctx;
  uint32_t instance;
  bDirEvent event;
  bDirEventType eventType;
  char *bp, *plugin_name, *option;
  const char* plugin_options;
  PoolMem priv_plugin_options(PM_MESSAGE);

  if (!dird_plugin_list || dird_plugin_list->empty()) { return; }

  if (jcr->res.job && jcr->res.job->DirPluginOptions &&
      jcr->res.job->DirPluginOptions->size()) {
    eventType = bDirEventNewPluginOptions;
    event.eventType = eventType;

    foreach_alist_index (i, plugin_options, jcr->res.job->DirPluginOptions) {
      /*
       * Make a private copy of plugin options.
       */
      PmStrcpy(priv_plugin_options, plugin_options);

      plugin_name = priv_plugin_options.c_str();
      if (!(bp = strchr(plugin_name, ':'))) {
        Jmsg(NULL, M_ERROR, 0,
             _("Illegal DIR plugin options encountered, %s skipping\n"),
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
        if (bp) { *bp++ = '\0'; }

        if (bstrncasecmp(option, "instance=", 9)) {
          instance = str_to_int64(option + 9);
          break;
        }

        option = bp;
      }

      if (instance < LOWEST_PLUGIN_INSTANCE ||
          instance > HIGHEST_PLUGIN_INSTANCE) {
        Jmsg(NULL, M_ERROR, 0,
             _("Illegal DIR plugin options encountered, %s instance %d "
               "skipping\n"),
             plugin_options, instance);
        continue;
      }

      len = strlen(plugin_name);

      /*
       * See if this plugin options are for an already instantiated plugin
       * instance.
       */
      foreach_alist (ctx, jcr->plugin_ctx_list) {
        if (ctx->instance == instance && ctx->plugin->file_len == len &&
            bstrncasecmp(ctx->plugin->file, plugin_name, len)) {
          break;
        }
      }

      /*
       * Found a context in the previous loop ?
       */
      if (!ctx) {
        foreach_alist_index (j, plugin, dird_plugin_list) {
          if (plugin->file_len == len &&
              bstrncasecmp(plugin->file, plugin_name, len)) {
            ctx = instantiate_plugin(jcr, plugin, instance);
            break;
          }
        }
      }

      if (ctx) {
        trigger_plugin_event(jcr, eventType, &event, ctx, (void*)plugin_options,
                             NULL, NULL, NULL);
      }
    }
  }
}

/**
 * Create a new instance of each plugin for this Job
 */
void NewPlugins(JobControlRecord* jcr)
{
  int i, num;
  Plugin* plugin;

  Dmsg0(debuglevel, "=== enter NewPlugins ===\n");
  if (!dird_plugin_list) {
    Dmsg0(debuglevel, "No dir plugin list!\n");
    return;
  }
  if (jcr->IsJobCanceled()) { return; }

  num = dird_plugin_list->size();
  Dmsg1(debuglevel, "dir-plugin-list size=%d\n", num);
  if (num == 0) { return; }

  jcr->plugin_ctx_list = New(alist(10, owned_by_alist));
  foreach_alist_index (i, plugin, dird_plugin_list) {
    /*
     * Start a new instance of each plugin
     */
    instantiate_plugin(jcr, plugin, 0);
  }
}

/**
 * Free the plugin instances for this Job
 */
void FreePlugins(JobControlRecord* jcr)
{
  bpContext* ctx = nullptr;

  if (!dird_plugin_list || !jcr->plugin_ctx_list) { return; }

  Dmsg2(debuglevel, "Free instance dir-plugin_ctx_list=%p JobId=%d\n",
        jcr->plugin_ctx_list, jcr->JobId);
  foreach_alist (ctx, jcr->plugin_ctx_list) {
    /*
     * Free the plugin instance
     */
    DirplugFunc(ctx->plugin)->freePlugin(ctx);
    free(ctx->bContext); /* Free BAREOS private context */
  }

  delete jcr->plugin_ctx_list;
  jcr->plugin_ctx_list = NULL;
}

/**
 *
 * Callbacks from the plugin
 *
 */
static bRC bareosGetValue(bpContext* ctx, brDirVariable var, void* value)
{
  JobControlRecord* jcr = NULL;
  bRC retval = bRC_OK;

  if (!value) { return bRC_Error; }

  switch (var) { /* General variables, no need of ctx */
    case bDirVarPluginDir:
      *((char**)value) = me->plugin_directory;
      Dmsg1(debuglevel, "dir-plugin: return bDirVarPluginDir=%s\n",
            NPRT(*((char**)value)));
      break;
    default:
      if (!ctx) { return bRC_Error; }
      jcr = ((b_plugin_ctx*)ctx->bContext)->jcr;
      if (!jcr) { return bRC_Error; }
      break;
  }

  if (jcr) {
    switch (var) {
      case bDirVarJobId:
        *((int*)value) = jcr->JobId;
        Dmsg1(debuglevel, "dir-plugin: return bDirVarJobId=%d\n", jcr->JobId);
        break;
      case bDirVarJobName:
        *((char**)value) = jcr->Job;
        Dmsg1(debuglevel, "dir-plugin: return Job name=%s\n",
              NPRT(*((char**)value)));
        break;
      case bDirVarJob:
        *((char**)value) = jcr->res.job->hdr.name;
        Dmsg1(debuglevel, "dir-plugin: return bDirVarJob=%s\n",
              NPRT(*((char**)value)));
        break;
      case bDirVarLevel:
        *((int*)value) = jcr->getJobLevel();
        Dmsg1(debuglevel, "dir-plugin: return bDirVarLevel=%c\n",
              jcr->getJobLevel());
        break;
      case bDirVarType:
        *((int*)value) = jcr->getJobType();
        Dmsg1(debuglevel, "dir-plugin: return bDirVarType=%c\n",
              jcr->getJobType());
        break;
      case bDirVarClient:
        *((char**)value) = jcr->res.client->hdr.name;
        Dmsg1(debuglevel, "dir-plugin: return bDirVarClient=%s\n",
              NPRT(*((char**)value)));
        break;
      case bDirVarNumVols:
        PoolDbRecord pr;

        memset(&pr, 0, sizeof(pr));
        bstrncpy(pr.Name, jcr->res.pool->hdr.name, sizeof(pr.Name));
        if (!jcr->db->GetPoolRecord(jcr, &pr)) { retval = bRC_Error; }
        *((int*)value) = pr.NumVols;
        Dmsg1(debuglevel, "dir-plugin: return bDirVarNumVols=%d\n", pr.NumVols);
        break;
      case bDirVarPool:
        *((char**)value) = jcr->res.pool->hdr.name;
        Dmsg1(debuglevel, "dir-plugin: return bDirVarPool=%s\n",
              NPRT(*((char**)value)));
        break;
      case bDirVarStorage:
        if (jcr->res.write_storage) {
          *((char**)value) = jcr->res.write_storage->hdr.name;
        } else if (jcr->res.read_storage) {
          *((char**)value) = jcr->res.read_storage->hdr.name;
        } else {
          *((char**)value) = NULL;
          retval = bRC_Error;
        }
        Dmsg1(debuglevel, "dir-plugin: return bDirVarStorage=%s\n",
              NPRT(*((char**)value)));
        break;
      case bDirVarWriteStorage:
        if (jcr->res.write_storage) {
          *((char**)value) = jcr->res.write_storage->hdr.name;
        } else {
          *((char**)value) = NULL;
          retval = bRC_Error;
        }
        Dmsg1(debuglevel, "dir-plugin: return bDirVarWriteStorage=%s\n",
              NPRT(*((char**)value)));
        break;
      case bDirVarReadStorage:
        if (jcr->res.read_storage) {
          *((char**)value) = jcr->res.read_storage->hdr.name;
        } else {
          *((char**)value) = NULL;
          retval = bRC_Error;
        }
        Dmsg1(debuglevel, "dir-plugin: return bDirVarReadStorage=%s\n",
              NPRT(*((char**)value)));
        break;
      case bDirVarCatalog:
        *((char**)value) = jcr->res.catalog->hdr.name;
        Dmsg1(debuglevel, "dir-plugin: return bDirVarCatalog=%s\n",
              NPRT(*((char**)value)));
        break;
      case bDirVarMediaType:
        if (jcr->res.write_storage) {
          *((char**)value) = jcr->res.write_storage->media_type;
        } else if (jcr->res.read_storage) {
          *((char**)value) = jcr->res.read_storage->media_type;
        } else {
          *((char**)value) = NULL;
          retval = bRC_Error;
        }
        Dmsg1(debuglevel, "dir-plugin: return bDirVarMediaType=%s\n",
              NPRT(*((char**)value)));
        break;
      case bDirVarJobStatus:
        *((int*)value) = jcr->JobStatus;
        Dmsg1(debuglevel, "dir-plugin: return bDirVarJobStatus=%c\n",
              jcr->JobStatus);
        break;
      case bDirVarPriority:
        *((int*)value) = jcr->JobPriority;
        Dmsg1(debuglevel, "dir-plugin: return bDirVarPriority=%d\n",
              jcr->JobPriority);
        break;
      case bDirVarVolumeName:
        *((char**)value) = jcr->VolumeName;
        Dmsg1(debuglevel, "dir-plugin: return bDirVarVolumeName=%s\n",
              NPRT(*((char**)value)));
        break;
      case bDirVarCatalogRes:
        retval = bRC_Error;
        break;
      case bDirVarJobErrors:
        *((int*)value) = jcr->JobErrors;
        Dmsg1(debuglevel, "dir-plugin: return bDirVarErrors=%d\n",
              jcr->JobErrors);
        break;
      case bDirVarJobFiles:
        *((int*)value) = jcr->JobFiles;
        Dmsg1(debuglevel, "dir-plugin: return bDirVarFiles=%d\n",
              jcr->JobFiles);
        break;
      case bDirVarSDJobFiles:
        *((int*)value) = jcr->SDJobFiles;
        Dmsg1(debuglevel, "dir-plugin: return bDirVarSDFiles=%d\n",
              jcr->SDJobFiles);
        break;
      case bDirVarSDErrors:
        *((int*)value) = jcr->SDErrors;
        Dmsg1(debuglevel, "dir-plugin: return bDirVarSDErrors=%d\n",
              jcr->SDErrors);
        break;
      case bDirVarFDJobStatus:
        *((int*)value) = jcr->FDJobStatus;
        Dmsg1(debuglevel, "dir-plugin: return bDirVarFDJobStatus=%c\n",
              jcr->FDJobStatus);
        break;
      case bDirVarSDJobStatus:
        *((int*)value) = jcr->SDJobStatus;
        Dmsg1(debuglevel, "dir-plugin: return bDirVarSDJobStatus=%c\n",
              jcr->SDJobStatus);
        break;
      case bDirVarLastRate:
        *((int*)value) = jcr->LastRate;
        Dmsg1(debuglevel, "dir-plugin: return bDirVarLastRate=%d\n",
              jcr->LastRate);
        break;
      case bDirVarJobBytes:
        *((uint64_t*)value) = jcr->JobBytes;
        Dmsg1(debuglevel, "dir-plugin: return bDirVarJobBytes=%u\n",
              jcr->JobBytes);
        break;
      case bDirVarReadBytes:
        *((uint64_t*)value) = jcr->ReadBytes;
        Dmsg1(debuglevel, "dir-plugin: return bDirVarReadBytes=%u\n",
              jcr->ReadBytes);
        break;
      default:
        break;
    }
  }

  return retval;
}

static bRC bareosSetValue(bpContext* ctx, bwDirVariable var, void* value)
{
  JobControlRecord* jcr;

  if (!value || !ctx) { return bRC_Error; }

  jcr = ((b_plugin_ctx*)ctx->bContext)->jcr;
  if (!jcr) { return bRC_Error; }

  Dmsg1(debuglevel, "dir-plugin: bareosSetValue var=%d\n", var);
  switch (var) {
    case bwDirVarVolumeName:
      PmStrcpy(jcr->VolumeName, ((char*)value));
      break;
    case bwDirVarPriority:
      jcr->JobPriority = *((int*)value);
      break;
    case bwDirVarJobLevel:
      jcr->setJobLevel(*((int*)value));
      break;
    default:
      break;
  }

  return bRC_OK;
}

static bRC bareosRegisterEvents(bpContext* ctx, int nr_events, ...)
{
  int i;
  va_list args;
  uint32_t event;
  b_plugin_ctx* b_ctx;

  if (!ctx) { return bRC_Error; }
  b_ctx = (b_plugin_ctx*)ctx->bContext;
  va_start(args, nr_events);
  for (i = 0; i < nr_events; i++) {
    event = va_arg(args, uint32_t);
    Dmsg1(debuglevel, "dir-plugin: Plugin registered event=%u\n", event);
    SetBit(event, b_ctx->events);
  }
  va_end(args);

  return bRC_OK;
}

static bRC bareosUnRegisterEvents(bpContext* ctx, int nr_events, ...)
{
  int i;
  va_list args;
  uint32_t event;
  b_plugin_ctx* b_ctx;

  if (!ctx) { return bRC_Error; }
  b_ctx = (b_plugin_ctx*)ctx->bContext;
  va_start(args, nr_events);
  for (i = 0; i < nr_events; i++) {
    event = va_arg(args, uint32_t);
    Dmsg1(debuglevel, "dir-plugin: Plugin unregistered event=%u\n", event);
    ClearBit(event, b_ctx->events);
  }
  va_end(args);

  return bRC_OK;
}

static bRC bareosGetInstanceCount(bpContext* ctx, int* ret)
{
  int cnt;
  JobControlRecord *jcr, *njcr;
  bpContext* nctx;
  b_plugin_ctx* bctx;
  bRC retval = bRC_Error;

  if (!IsCtxGood(ctx, jcr, bctx)) { goto bail_out; }

  P(mutex);

  cnt = 0;
  foreach_jcr (njcr) {
    if (jcr->plugin_ctx_list) {
      foreach_alist (nctx, jcr->plugin_ctx_list) {
        if (nctx->plugin == bctx->plugin) { cnt++; }
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

static bRC bareosJobMsg(bpContext* ctx,
                        const char* file,
                        int line,
                        int type,
                        utime_t mtime,
                        const char* fmt,
                        ...)
{
  JobControlRecord* jcr;
  va_list arg_ptr;
  PoolMem buffer(PM_MESSAGE);

  if (ctx) {
    jcr = ((b_plugin_ctx*)ctx->bContext)->jcr;
  } else {
    jcr = NULL;
  }

  va_start(arg_ptr, fmt);
  buffer.Bvsprintf(fmt, arg_ptr);
  va_end(arg_ptr);
  Jmsg(jcr, type, mtime, "%s", buffer.c_str());

  return bRC_OK;
}

static bRC bareosDebugMsg(bpContext* ctx,
                          const char* file,
                          int line,
                          int level,
                          const char* fmt,
                          ...)
{
  va_list arg_ptr;
  PoolMem buffer(PM_MESSAGE);

  va_start(arg_ptr, fmt);
  buffer.Bvsprintf(fmt, arg_ptr);
  va_end(arg_ptr);
  d_msg(file, line, level, "%s", buffer.c_str());

  return bRC_OK;
}
} /* namespace directordaemon */
