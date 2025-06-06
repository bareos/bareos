/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2007-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2025 Bareos GmbH & Co. KG

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
// Kern Sibbald, October 2007
/**
 * @file
 * Bareos pluginloader
 */
#include "include/fcntl_def.h"
#include "include/bareos.h"
#include "include/filetypes.h"
#include "include/streams.h"
#include "filed/filed.h"
#include "filed/filed_globals.h"
#include "filed/accurate.h"
#include "filed/heartbeat.h"
#include "filed/fileset.h"
#include "filed/heartbeat.h"
#include "filed/filed_jcr_impl.h"
#include "findlib/attribs.h"
#include "findlib/find.h"
#include "findlib/find_one.h"
#include "findlib/hardlink.h"
#include "lib/berrno.h"
#include "lib/bsock.h"
#include "lib/plugins.h"
#include "lib/parse_conf.h"

// Function pointers to be set here (findlib)
BAREOS_IMPORT int (*plugin_bopen)(BareosFilePacket* bfd,
                                  const char* fname,
                                  int flags,
                                  mode_t mode);
BAREOS_IMPORT int (*plugin_bclose)(BareosFilePacket* bfd);
BAREOS_IMPORT ssize_t (*plugin_bread)(BareosFilePacket* bfd,
                                      void* buf,
                                      size_t count);
BAREOS_IMPORT ssize_t (*plugin_bwrite)(BareosFilePacket* bfd,
                                       void* buf,
                                       size_t count);
BAREOS_IMPORT boffset_t (*plugin_blseek)(BareosFilePacket* bfd,
                                         boffset_t offset,
                                         int whence);

BAREOS_IMPORT char* exepath;

namespace filedaemon {

const int debuglevel = 150;
#ifdef HAVE_WIN32
const char* plugin_type = "-fd.dll";
#else
const char* plugin_type = "-fd.so";
#endif
static alist<Plugin*>* fd_plugin_list = NULL;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

extern int SaveFile(JobControlRecord* jcr,
                    FindFilesPacket* ff_pkt,
                    bool top_level);

// Forward referenced functions
static bRC bareosGetValue(PluginContext* ctx, bVariable var, void* value);
static bRC bareosSetValue(PluginContext* ctx, bVariable var, const void* value);
static bRC bareosRegisterEvents(PluginContext* ctx, int nr_events, ...);
static bRC bareosUnRegisterEvents(PluginContext* ctx, int nr_events, ...);
static bRC bareosJobMsg(PluginContext* ctx,
                        const char* fname,
                        int line,
                        int type,
                        utime_t mtime,
                        const char* fmt,
                        ...) PRINTF_LIKE(6, 7);
static bRC bareosDebugMsg(PluginContext* ctx,
                          const char* fname,
                          int line,
                          int level,
                          const char* fmt,
                          ...) PRINTF_LIKE(5, 6);
static void* bareosMalloc(PluginContext* ctx,
                          const char* fname,
                          int line,
                          size_t size);
static void bareosFree(PluginContext* ctx,
                       const char* file,
                       int line,
                       void* mem);
static bRC bareosAddExclude(PluginContext* ctx, const char* file);
static bRC bareosAddInclude(PluginContext* ctx, const char* file);
static bRC bareosAddOptions(PluginContext* ctx, const char* opts);
static bRC bareosAddRegex(PluginContext* ctx, const char* item, int type);
static bRC bareosAddWild(PluginContext* ctx, const char* item, int type);
static bRC bareosNewOptions(PluginContext* ctx);
static bRC bareosNewInclude(PluginContext* ctx);
static bRC bareosNewPreInclude(PluginContext* ctx);
static bool IsPluginCompatible(Plugin* plugin);
static bool GetPluginName(JobControlRecord* jcr, char* cmd, int* ret);
static bRC bareosCheckChanges(PluginContext* ctx, save_pkt* sp);
static bRC bareosAcceptFile(PluginContext* ctx, save_pkt* sp);
static bRC bareosSetSeenBitmap(PluginContext* ctx, bool all, char* fname);
static bRC bareosClearSeenBitmap(PluginContext* ctx, bool all, char* fname);
static bRC bareosGetInstanceCount(PluginContext* ctx, int* ret);

// These will be plugged into the global pointer structure for the findlib.
static int MyPluginBopen(BareosFilePacket* bfd,
                         const char* fname,
                         int flags,
                         mode_t mode);
static int MyPluginBclose(BareosFilePacket* bfd);
static ssize_t MyPluginBread(BareosFilePacket* bfd, void* buf, size_t count);
static ssize_t MyPluginBwrite(BareosFilePacket* bfd, void* buf, size_t count);
static boffset_t MyPluginBlseek(BareosFilePacket* bfd,
                                boffset_t offset,
                                int whence);

/* Bareos info */
static PluginApiDefinition bareos_plugin_interface_version
    = {sizeof(PluginApiDefinition), FD_PLUGIN_INTERFACE_VERSION};

/* Bareos entry points */
static CoreFunctions bareos_core_functions = {sizeof(CoreFunctions),
                                              FD_PLUGIN_INTERFACE_VERSION,
                                              bareosRegisterEvents,
                                              bareosUnRegisterEvents,
                                              bareosGetInstanceCount,
                                              bareosGetValue,
                                              bareosSetValue,
                                              bareosJobMsg,
                                              bareosDebugMsg,
                                              bareosMalloc,
                                              bareosFree,
                                              bareosAddExclude,
                                              bareosAddInclude,
                                              bareosAddOptions,
                                              bareosAddRegex,
                                              bareosAddWild,
                                              bareosNewOptions,
                                              bareosNewInclude,
                                              bareosNewPreInclude,
                                              bareosCheckChanges,
                                              bareosAcceptFile,
                                              bareosSetSeenBitmap,
                                              bareosClearSeenBitmap};

struct FiledPluginContext {
  FiledPluginContext(JobControlRecord* t_jcr, Plugin* t_plugin)
      : jcr(t_jcr), plugin(t_plugin)
  {
  }
  JobControlRecord* jcr; /* jcr for plugin */
  bRC ret{bRC_OK};       /* last return code */
  bool disabled{false};  /* set if plugin disabled */
  bool restoreFileStarted{false};
  bool createFileCalled{false};
  char events[NbytesForBits(FD_NR_EVENTS + 1)]{}; /* enabled events bitmask */
  findIncludeExcludeItem* exclude{nullptr};       /* pointer to exclude files */
  findIncludeExcludeItem* include{
      nullptr};   /* pointer to include/exclude files */
  Plugin* plugin; /* pointer to plugin of which this is an instance off */
  bool check_changes{true}; /* call CheckChanges() on every file */
};

static inline bool IsEventEnabled(PluginContext* ctx, bEventType eventType)
{
  FiledPluginContext* b_ctx;

  if (!ctx) { return false; }

  b_ctx = (FiledPluginContext*)ctx->core_private_context;
  if (!b_ctx) { return false; }

  return BitIsSet(eventType, b_ctx->events);
}

static inline bool IsPluginDisabled(PluginContext* ctx)
{
  FiledPluginContext* b_ctx;

  if (!ctx) { return true; }

  b_ctx = (FiledPluginContext*)ctx->core_private_context;
  if (!b_ctx) { return true; }

  return b_ctx->disabled;
}

static bool IsCtxGood(PluginContext* ctx,
                      JobControlRecord*& jcr,
                      FiledPluginContext*& bctx)
{
  if (!ctx) { return false; }

  bctx = (FiledPluginContext*)ctx->core_private_context;
  if (!bctx) { return false; }

  jcr = bctx->jcr;
  if (!jcr) { return false; }

  return true;
}

// Test if event is for this plugin
// Raise a certain plugin event.
static inline bool trigger_plugin_event(JobControlRecord*,
                                        bEventType eventType,
                                        bEvent* event,
                                        PluginContext* ctx,
                                        void* value,
                                        alist<PluginContext*>*,
                                        int* index,
                                        bRC* rc)

{
  bool stop = false;

  if (!IsEventEnabled(ctx, eventType)) {
    Dmsg1(debuglevel, "Event %d disabled for this plugin.\n", eventType);
    if (rc) { *rc = bRC_OK; }
    goto bail_out;
  }

  if (IsPluginDisabled(ctx)) {
    if (rc) { *rc = bRC_OK; }
    goto bail_out;
  }

  if (eventType == bEventEndRestoreJob) {
    FiledPluginContext* b_ctx = (FiledPluginContext*)ctx->core_private_context;

    Dmsg0(50, "eventType == bEventEndRestoreJob\n");
    if (b_ctx && b_ctx->restoreFileStarted) {
      PlugFunc(ctx->plugin)->endRestoreFile(ctx);
    }

    if (b_ctx) {
      b_ctx->restoreFileStarted = false;
      b_ctx->createFileCalled = false;
    }
  }

  // See if we should care about the return code.
  if (rc) {
    *rc = PlugFunc(ctx->plugin)->handlePluginEvent(ctx, event, value);
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
        /* Request to unload this plugin.
         * As we remove the plugin from the list of plugins we decrement
         * the running index value so the next plugin gets triggered as
         * that moved back a position in the alist. */
        if (index) {
          UnloadPlugin(fd_plugin_list, ctx->plugin, *index);
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
    PlugFunc(ctx->plugin)->handlePluginEvent(ctx, event, value);
  }

bail_out:
  return stop;
}

static bool IsEventForThisPlugin(Plugin* plugin, const char* name, int len)
{
  Dmsg4(debuglevel, "IsEventForThisPlugin? name=%s len=%d plugin=%s plen=%d\n",
        name ? name : "(null)", len, plugin->file, plugin->file_len);
  if (!name) { /* if no plugin name, all plugins get it */
    return true;
  }

  // Return global VSS job metadata to all plugins
  if (bstrcmp("job", name)) { /* old V4.0 name for VSS job metadata */
    return true;
  }

  if (bstrcmp("*all*", name)) { /* new v6.0 name for VSS job metadata */
    return true;
  }

  // Check if this is the correct plugin
  if (len == plugin->file_len && bstrncmp(plugin->file, name, len)) {
    Dmsg4(debuglevel,
          "IsEventForThisPlugin: yes, full match (plugin=%s, name=%s)\n",
          plugin->file, name);
    return true;
  }
  // To be able to restore "python" plugin streams with the "python3" plugin,
  // we check if the required name is the same as the plugin name without the
  // last character
  if (len == plugin->file_len - 1 && bstrncmp(plugin->file, name, len)) {
    Dmsg4(debuglevel,
          "IsEventForThisPlugin: yes, without last character: (plugin=%s, "
          "name=%s)\n",
          plugin->file, name);
    return true;
  }

  Dmsg4(debuglevel, "IsEventForThisPlugin: no (plugin=%s, name=%s)\n",
        plugin->file, name);

  return false;
}

PluginContext* grpc_plugin_context(alist<PluginContext*>* plugin_ctx_list)
{
  for (auto* ctx : plugin_ctx_list) {
    if (ctx->plugin->file_len < 0) {
      Dmsg0(50, "plugin context list contains plugin with file_len < 0!!!\n");
      continue;
    }
    std::string_view pname{ctx->plugin->file,
                           static_cast<size_t>(ctx->plugin->file_len)};

    if (pname == std::string_view{"grpc"} && !IsPluginDisabled(ctx)) {
      return ctx;
    }
  }
  return nullptr;
}

PluginContext* find_plugin(alist<PluginContext*>* plugin_list,
                           std::string& cmd,
                           int name_len,
                           bEventType eventType)
{
  for (auto* ctx : plugin_list) {
    Dmsg4(debuglevel, "plugin=%s plen=%d cmd=%s len=%d\n", ctx->plugin->file,
          ctx->plugin->file_len, cmd.c_str(), name_len);
    if (!IsEventForThisPlugin(ctx->plugin, cmd.c_str(), name_len)) { continue; }

    if (!IsEventEnabled(ctx, eventType)) {
      Dmsg1(debuglevel, "Event %d disabled for this plugin.\n", eventType);
      continue;
    }

    return ctx;
  }

  // we were not able to find the plugin among our loaded plugins.
  // lets try to load it via grpc fallback

  ResLocker _{my_config};

  ClientResource* res
      = dynamic_cast<ClientResource*>(my_config->GetNextRes(R_CLIENT, nullptr));

  PluginContext* ctx = grpc_plugin_context(plugin_list);

  if (res && !res->grpc_module.empty() && ctx) {
    std::string grpc_cmd = "grpc:";
    grpc_cmd += res->grpc_module;
    grpc_cmd += ":";
    grpc_cmd += cmd;

    if (!IsEventEnabled(ctx, eventType)) { return nullptr; }

    Dmsg1(100, "using grpc to handle this event\n '%s' -> '%s'\n", cmd.c_str(),
          grpc_cmd.c_str());

    cmd = std::move(grpc_cmd);

    return ctx;
  }

  Dmsg1(
      100,
      "grpc fallback is not enabled:\n"
      " module='%s' ctx=%s",
      (res && res->grpc_module.empty()) ? res->grpc_module.c_str() : "<UNSET>",
      ctx ? "found" : "notfound");

  return nullptr;
}

/* Returns the found plugin context and the command string to use with that
 * plugin. PluginContext* is nullptr iff no plugin was found */
std::pair<std::string, PluginContext*> find_plugin_from_list(
    JobControlRecord* jcr,
    const char* original_cmd,
    alist<PluginContext*>* plugin_ctx_list,
    bEventType eventType)
{
  int len;

  std::string cmd{original_cmd};

  if (!GetPluginName(jcr, cmd.data(), &len)) { return {}; }

  auto* ctx = find_plugin(plugin_ctx_list, cmd, len, eventType);

  if (!ctx) {
    Jmsg1(jcr, M_FATAL, 0, "Command plugin \"%s\" not found.\n", cmd.c_str());
    return {};
  }

  if (IsPluginDisabled(ctx)) { return {}; }

  return {std::move(cmd), ctx};
}


/**
 * Create a plugin event When receiving bEventCancelCommand, this function is
 * called by another thread.
 */
bRC GeneratePluginEvent(JobControlRecord* jcr,
                        bEventType eventType,
                        void* value)
{
  bEvent event;
  char* name = NULL;
  int len = 0;
  bool call_if_canceled = false;
  restore_object_pkt* rop;
  alist<PluginContext*>* plugin_ctx_list;
  bRC rc = bRC_OK;

  if (!fd_plugin_list) {
    Dmsg0(debuglevel, "No bplugin_list: GeneratePluginEvent ignored.\n");
    goto bail_out;
  }

  if (!jcr) {
    Dmsg0(debuglevel, "No jcr: GeneratePluginEvent ignored.\n");
    goto bail_out;
  }

  if (!jcr->plugin_ctx_list) {
    Dmsg0(debuglevel, "No plugin_ctx_list: GeneratePluginEvent ignored.\n");
    goto bail_out;
  }

  plugin_ctx_list = jcr->plugin_ctx_list;

  /* Some events are sent to only a particular plugin or must be called even if
   * the job is canceled. */
  switch (eventType) {
    case bEventPluginCommand:
    case bEventOptionPlugin:
      name = (char*)value;
      if (!GetPluginName(jcr, name, &len)) { goto bail_out; }
      break;
    case bEventNewPluginOptions: {
      // If this event contains a name, then we need to fix it up
      // It may be possible for it to not contain a name: in that case
      // we dont touch the value.
      // This has some problems of course, but the values should be plugin
      // specific most of the time.
      name = (char*)value;
      if (!GetPluginName(jcr, name, &len)) { name = nullptr; }
    } break;
    case bEventRestoreObject:
      // After all RestoreObject, we have it one more time with value = NULL
      if (value) {
        // Some RestoreObjects may not have a plugin name
        rop = (restore_object_pkt*)value;
        if (*rop->plugin_name) {
          name = rop->plugin_name;
          if (!GetPluginName(jcr, name, &len)) { goto bail_out; }
        }
      }
      break;
    case bEventEndBackupJob:
    case bEventEndVerifyJob:
      call_if_canceled = true; /* plugin *must* see this call */
      break;
    case bEventStartRestoreJob:
      for (auto* ctx : plugin_ctx_list) {
        ((FiledPluginContext*)ctx->core_private_context)->restoreFileStarted
            = false;
        ((FiledPluginContext*)ctx->core_private_context)->createFileCalled
            = false;
      }
      break;
    case bEventEndRestoreJob:
      call_if_canceled = true; /* plugin *must* see this call */
      break;
    default:
      break;
  }

  // If call_if_canceled is set, we call the plugin anyway
  if (!call_if_canceled && jcr->IsJobCanceled()) { goto bail_out; }

  event.eventType = eventType;

  Dmsg2(debuglevel, "plugin_ctx=%p JobId=%d\n", plugin_ctx_list, jcr->JobId);

  /* Pass event to every plugin that has requested this event type (except if
   * name is set). If name is set, we pass it only to the plugin with that name.
   *
   * See if we need to trigger the loaded plugins in reverse order. */

  if (name) {
    // we want to sent this to a specific plugin
    std::string cmd = name;
    auto* ctx = find_plugin(plugin_ctx_list, cmd, len, eventType);

    Dmsg2(debuglevel, "updated cmd = '%s'\n", cmd.c_str());
    void* updated_value = const_cast<char*>(cmd.c_str());

    restore_object_pkt updated_rop;
    if (eventType == bEventRestoreObject) {
      updated_rop = *(restore_object_pkt*)value;
      updated_rop.plugin_name = const_cast<char*>(cmd.c_str());
      updated_value = &updated_rop;
    }

    if (ctx) {
      int unused{};
      trigger_plugin_event(jcr, eventType, &event, ctx, updated_value,
                           plugin_ctx_list, &unused, &rc);
    }
  } else {
    PluginContext* ctx;
    int i{};
    foreach_alist_index (i, ctx, plugin_ctx_list) {
      if (!IsEventForThisPlugin(ctx->plugin, name, len)) {
        Dmsg2(debuglevel, "Not for this plugin name=%s NULL=%d\n",
              name ? name : "(null)", name ? 1 : 0);
        continue;
      }

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

// Check if file was seen for accurate
bool PluginCheckFile(JobControlRecord* jcr, char* fname)
{
  alist<PluginContext*>* plugin_ctx_list;
  int retval = bRC_OK;

  if (!fd_plugin_list || !jcr || !jcr->plugin_ctx_list
      || jcr->IsJobCanceled()) {
    return false; /* Return if no plugins loaded */
  }

  plugin_ctx_list = jcr->plugin_ctx_list;
  Dmsg2(debuglevel, "plugin_ctx=%p JobId=%d\n", jcr->plugin_ctx_list,
        jcr->JobId);

  // Pass event to every plugin
  for (auto* ctx : plugin_ctx_list) {
    if (IsPluginDisabled(ctx)) { continue; }

    jcr->plugin_ctx = ctx;
    if (PlugFunc(ctx->plugin)->checkFile == NULL) { continue; }

    retval = PlugFunc(ctx->plugin)->checkFile(ctx, fname);
    if (retval == bRC_Seen) { break; }
  }

  return retval == bRC_Seen;
}

/**
 * Get the first part of the the plugin command
 *  systemstate:/@SYSTEMSTATE/
 * => ret = 11
 * => can use IsEventForThisPlugin(plug, cmd, ret);
 *
 * The plugin command can contain only the plugin name
 *  Plugin = alldrives
 * => ret = 9
 */
static bool GetPluginName(JobControlRecord* jcr, char* cmd, int* ret)
{
  char* p;
  int len;

  if (!cmd || (*cmd == '\0')) { return false; }

  // Handle plugin command here backup
  Dmsg1(debuglevel, "plugin cmd=%s\n", cmd);
  if ((p = strchr(cmd, ':')) == NULL) {
    if (strchr(cmd, ' ') == NULL) { /* we have just the plugin name */
      len = strlen(cmd);
    } else {
      Jmsg1(jcr, M_ERROR, 0, "Malformed plugin command: %s\n", cmd);
      return false;
    }
  } else { /* plugin:argument */
    len = p - cmd;
    if (len <= 0) { return false; }
  }
  *ret = len;

  return true;
}

void PluginUpdateFfPkt(FindFilesPacket* ff_pkt, save_pkt* sp)
{
  ff_pkt->no_read = sp->no_read;
  ff_pkt->delta_seq = sp->delta_seq;

  if (BitIsSet(FO_DELTA, sp->flags)) {
    SetBit(FO_DELTA, ff_pkt->flags);
    ff_pkt->delta_seq++; /* make new delta sequence number */
  } else {
    ClearBit(FO_DELTA, ff_pkt->flags);
    ff_pkt->delta_seq = 0; /* clean delta sequence number */
  }

  if (BitIsSet(FO_OFFSETS, sp->flags)) {
    SetBit(FO_OFFSETS, ff_pkt->flags);
  } else {
    ClearBit(FO_OFFSETS, ff_pkt->flags);
  }

  /* Sparse code doesn't work with plugins
   * that use FIFO or STDOUT/IN to communicate */
  if (BitIsSet(FO_SPARSE, sp->flags)) {
    SetBit(FO_SPARSE, ff_pkt->flags);
  } else {
    ClearBit(FO_SPARSE, ff_pkt->flags);
  }

  if (BitIsSet(FO_PORTABLE_DATA, sp->flags)) {
    SetBit(FO_PORTABLE_DATA, ff_pkt->flags);
  } else {
    ClearBit(FO_PORTABLE_DATA, ff_pkt->flags);
  }

  SetBit(FO_PLUGIN, ff_pkt->flags); /* data from plugin */
}

// Ask to a Option Plugin what to do with the current file
bRC PluginOptionHandleFile(JobControlRecord* jcr,
                           FindFilesPacket* ff_pkt,
                           save_pkt* sp)
{
  int len;
  char* cmd;
  bRC retval = bRC_Core;
  bEvent event;
  bEventType eventType;
  alist<PluginContext*>* plugin_ctx_list;

  cmd = ff_pkt->plugin;
  eventType = bEventHandleBackupFile;
  event.eventType = eventType;

  sp->portable = true;
  CopyBits(FO_MAX, ff_pkt->flags, sp->flags);
  sp->cmd = cmd;
  sp->link = ff_pkt->link;
  sp->statp = ff_pkt->statp;
  sp->type = ff_pkt->type;
  sp->fname = ff_pkt->fname;
  sp->delta_seq = ff_pkt->delta_seq;
  sp->accurate_found = ff_pkt->accurate_found;

  plugin_ctx_list = jcr->plugin_ctx_list;

  if (!fd_plugin_list || !plugin_ctx_list) {
    Jmsg1(jcr, M_FATAL, 0,
          "PluginOptionHandleFile: Command plugin \"%s\" requested, but is not "
          "loaded.\n",
          cmd);
    goto bail_out; /* Return if no plugins loaded */
  }

  if (jcr->IsJobCanceled()) {
    Jmsg1(jcr, M_FATAL, 0,
          "PluginOptionHandleFile: Command plugin \"%s\" requested, but job is "
          "already cancelled.\n",
          cmd);
    goto bail_out; /* Return if job is cancelled */
  }

  if (!GetPluginName(jcr, cmd, &len)) { goto bail_out; }

  // Note, we stop the loop on the first plugin that matches the name
  for (auto* ctx : plugin_ctx_list) {
    Dmsg4(debuglevel, "plugin=%s plen=%d cmd=%s len=%d\n", ctx->plugin->file,
          ctx->plugin->file_len, cmd, len);
    if (!IsEventForThisPlugin(ctx->plugin, cmd, len)) { continue; }

    if (!IsEventEnabled(ctx, eventType)) {
      Dmsg1(debuglevel, "Event %d disabled for this plugin.\n", eventType);
      continue;
    }

    if (IsPluginDisabled(ctx)) { goto bail_out; }

    jcr->plugin_ctx = ctx;
    retval = PlugFunc(ctx->plugin)->handlePluginEvent(ctx, &event, sp);

    goto bail_out;
  } /* end foreach loop */

bail_out:
  return retval;
}

/**
 * Sequence of calls for a backup:
 * 1. PluginSave() here is called with ff_pkt
 * 2. we find the plugin requested on the command string
 * 3. we generate a bEventBackupCommand event to the specified plugin
 *    and pass it the command string.
 * 4. we make a startPluginBackup call to the plugin, which gives
 *    us the data we need in save_pkt
 * 5. we call Bareos's SaveFile() subroutine to save the specified
 *    file.  The plugin will be called at pluginIO() to supply the
 *    file data.
 *
 * Sequence of calls for restore:
 *   See subroutine PluginNameStream() below.
 */
int PluginSave(JobControlRecord* jcr, FindFilesPacket* ff_pkt, bool)
{
  int ret = 1;  // everything ok
  bEvent event;
  PoolMem fname(PM_FNAME);
  PoolMem link(PM_FNAME);
  alist<PluginContext*>* plugin_ctx_list;
  char flags[FOPTS_BYTES];

  const char* original_cmd = ff_pkt->top_fname;
  plugin_ctx_list = jcr->plugin_ctx_list;

  if (!fd_plugin_list || !plugin_ctx_list) {
    Jmsg1(jcr, M_FATAL, 0,
          "PluginSave: Command plugin \"%s\" requested, but is not "
          "loaded.\n",
          original_cmd);
    goto bail_out; /* Return if no plugins loaded */
  }

  if (jcr->IsJobCanceled()) {
    Jmsg1(jcr, M_FATAL, 0,
          "PluginSave: Command plugin \"%s\" requested, but job is "
          "already cancelled.\n",
          original_cmd);
    goto bail_out; /* Return if job is cancelled */
  }


  jcr->cmd_plugin = true;
  event.eventType = bEventBackupCommand;

  // Note, we stop the loop on the first plugin that matches the name
  if (auto [cmd, ctx] = find_plugin_from_list(
          jcr, original_cmd, plugin_ctx_list, bEventBackupCommand);
      ctx) {
    /* We put the current plugin pointer, and the plugin context into the jcr,
     * because during SaveFile(), the plugin will be called many times and
     * these values are needed. */
    jcr->plugin_ctx = ctx;

    // Send the backup command to the right plugin
    Dmsg1(debuglevel, "Command plugin = %s\n", cmd.c_str());
    if (PlugFunc(ctx->plugin)
            ->handlePluginEvent(jcr->plugin_ctx, &event, cmd.data())
        != bRC_OK) {
      goto bail_out;
    }

    // Loop getting filenames to backup then saving them
    while (!jcr->IsJobCanceled()) {
      save_pkt sp;
      sp.portable = true;
      sp.no_read = false;
      CopyBits(FO_MAX, ff_pkt->flags, sp.flags);
      sp.cmd = const_cast<char*>(cmd.c_str());
      Dmsg3(debuglevel, "startBackup st_size=%p st_blocks=%p sp=%p\n",
            &sp.statp.st_size, &sp.statp.st_blocks, &sp);

      // Get the file save parameters. I.e. the stat pkt ...
      switch (PlugFunc(ctx->plugin)->startBackupFile(ctx, &sp)) {
        case bRC_OK:
          if (sp.type == 0) {
            Jmsg1(jcr, M_FATAL, 0,
                  T_("Command plugin \"%s\": no type in startBackupFile "
                     "packet.\n"),
                  cmd.c_str());
            goto bail_out;
          }
          break;
        case bRC_Stop:
          Dmsg0(debuglevel,
                "Plugin returned bRC_Stop, continue with next steps\n");
          goto fun_end;
        case bRC_Skip:
          Dmsg0(debuglevel,
                "Plugin returned bRC_Skip, continue with next file\n");
          continue;
        case bRC_Error:
          Jmsg1(jcr, M_FATAL, 0,
                T_("Command plugin \"%s\": startBackupFile failed.\n"),
                cmd.c_str());
          goto bail_out;
        case PYTHON_UNDEFINED_RETURN_VALUE:
        case bRC_Cancel:
        case bRC_Core:
        case bRC_Max:
        case bRC_More:
        case bRC_Seen:
        case bRC_Term:
          Jmsg1(jcr, M_ERROR, 0,
                T_("Command plugin \"%s\": unhandled returncode from "
                   "startBackupFile.\n"),
                cmd.c_str());

          goto fun_end;
      }

      jcr->fd_impl->plugin_sp = &sp;
      ff_pkt = jcr->fd_impl->ff;

      // Save original flags.
      CopyBits(FO_MAX, ff_pkt->flags, flags);

      /* Copy fname and link because SaveFile() zaps them.  This avoids zaping
       * the plugin's strings. */
      ff_pkt->type = sp.type;
      if (IS_FT_OBJECT(sp.type)) {
        if (!sp.object_name) {
          Jmsg1(jcr, M_FATAL, 0,
                T_("Command plugin \"%s\": no object_name in startBackupFile "
                   "packet.\n"),
                cmd.c_str());
          goto bail_out;
        }
        ff_pkt->fname
            = const_cast<char*>(original_cmd); /* full plugin string */
        ff_pkt->object_name = sp.object_name;
        ff_pkt->object_index = sp.index; /* restore object index */
        ff_pkt->object_compression = 0;  /* no compression for now */
        ff_pkt->object = sp.object;
        ff_pkt->object_len = sp.object_len;
      } else {
        if (!sp.fname) {
          Jmsg1(jcr, M_FATAL, 0,
                T_("Command plugin \"%s\": no fname in startBackupFile "
                   "packet.\n"),
                cmd.c_str());
          goto bail_out;
        }

        PmStrcpy(fname, sp.fname);
        PmStrcpy(link, sp.link);

        ff_pkt->fname = fname.c_str();
        ff_pkt->link = link.c_str();

        PluginUpdateFfPkt(ff_pkt, &sp);
      }

      memcpy(&ff_pkt->statp, &sp.statp, sizeof(ff_pkt->statp));
      Dmsg2(debuglevel, "startBackup returned type=%d, fname=%s\n", sp.type,
            sp.fname);
      if (sp.object) {
        Dmsg2(debuglevel, "index=%d object=%.*s\n", sp.index, sp.object_len,
              sp.object);
      }

      /* Handle hard linked files
       *
       * Maintain a list of hard linked files already backed up. This allows
       * us to ensure that the data of each file gets backed up only once. */
      ff_pkt->LinkFI = 0;
      ff_pkt->FileIndex = 0;
      ff_pkt->linked = nullptr;
      if (!BitIsSet(FO_NO_HARDLINK, ff_pkt->flags)
          && ff_pkt->statp.st_nlink > 1) {
        switch (ff_pkt->statp.st_mode & S_IFMT) {
          case S_IFREG:
          case S_IFCHR:
          case S_IFBLK:
          case S_IFIFO:
#ifdef S_IFSOCK
          case S_IFSOCK:
#endif

            if (!ff_pkt->linkhash) { ff_pkt->linkhash = new LinkHash(10000); }

            auto [iter, _] = ff_pkt->linkhash->try_emplace(
                Hardlink{ff_pkt->statp.st_dev, ff_pkt->statp.st_ino}, sp.fname);
            auto& hl = iter->second;
            if (hl.FileIndex == 0) {
              ff_pkt->linked = &hl;
              Dmsg2(400, "Added to hash FI=%d file=%s\n", ff_pkt->FileIndex,
                    hl.name.c_str());
            } else if (bstrcmp(hl.name.c_str(), sp.fname)) {
              Dmsg2(400, "== Name identical skip FI=%d file=%s\n", hl.FileIndex,
                    fname.c_str());
              ff_pkt->no_read = true;
            } else {
              ff_pkt->link = hl.name.data();
              ff_pkt->type = FT_LNKSAVED; /* Handle link, file already saved */
              ff_pkt->LinkFI = hl.FileIndex;
              ff_pkt->digest = hl.digest.data();
              ff_pkt->digest_stream = hl.digest_stream;
              ff_pkt->digest_len = hl.digest.size();

              Dmsg3(400, "FT_LNKSAVED FI=%d LinkFI=%d file=%s\n",
                    ff_pkt->FileIndex, hl.FileIndex, hl.name.c_str());

              ff_pkt->no_read = true;
            }
            break;
        }
      }

      sp.cmd = const_cast<char*>(original_cmd);

      // Call Bareos core code to backup the plugin's file
      SaveFile(jcr, ff_pkt, true);

      if (ff_pkt->linked) { ff_pkt->linked->FileIndex = ff_pkt->FileIndex; }

      // Restore original flags.
      CopyBits(FO_MAX, flags, ff_pkt->flags);

      bRC retval = PlugFunc(ctx->plugin)->endBackupFile(ctx);
      if (retval == bRC_More || retval == bRC_OK) {
        AccurateMarkFileAsSeen(jcr, fname.c_str());
      }

      if (retval == bRC_More) { continue; }

      goto fun_end;
    } /* end while loop */

    goto fun_end;
  }

bail_out:
  ret = 0;

fun_end:
  jcr->cmd_plugin = false;

  return ret;
}

/**
 * Sequence of calls for a estimate:
 * 1. PluginEstimate() here is called with ff_pkt
 * 2. we find the plugin requested on the command string
 * 3. we generate a bEventEstimateCommand event to the specified plugin
 *    and pass it the command string.
 * 4. we make a startPluginBackup call to the plugin, which gives
 *    us the data we need in save_pkt
 *
 */
int PluginEstimate(JobControlRecord* jcr, FindFilesPacket* ff_pkt, bool)
{
  const char* original_cmd = ff_pkt->top_fname;
  bEvent event;
  bEventType eventType;
  PoolMem fname(PM_FNAME);
  PoolMem link(PM_FNAME);
  alist<PluginContext*>* plugin_ctx_list;
  Attributes attr;

  plugin_ctx_list = jcr->plugin_ctx_list;
  if (!fd_plugin_list || !plugin_ctx_list) {
    Jmsg1(jcr, M_FATAL, 0,
          "Command plugin \"%s\" requested, but is not loaded.\n",
          original_cmd);
    return 1; /* Return if no plugins loaded */
  }

  jcr->cmd_plugin = true;
  eventType = bEventEstimateCommand;
  event.eventType = eventType;


  if (auto [cmd, ctx] = find_plugin_from_list(
          jcr, original_cmd, plugin_ctx_list, bEventEstimateCommand);
      ctx) {
    Dmsg4(debuglevel, "plugin=%s plen=%d cmd=%s len=%zu\n", ctx->plugin->file,
          ctx->plugin->file_len, cmd.c_str(), cmd.size());

    /* We put the current plugin pointer, and the plugin context into the jcr,
     * because during SaveFile(), the plugin will be called many times and
     * these values are needed. */
    jcr->plugin_ctx = ctx;

    // Send the backup command to the right plugin
    Dmsg1(debuglevel, "Command plugin = %s\n", cmd.c_str());
    if (PlugFunc(ctx->plugin)
            ->handlePluginEvent(ctx, &event, const_cast<char*>(cmd.c_str()))
        != bRC_OK) {
      goto bail_out;
    }

    // Loop getting filenames to backup then saving them
    while (!jcr->IsJobCanceled()) {
      save_pkt sp;
      sp.portable = true;
      CopyBits(FO_MAX, ff_pkt->flags, sp.flags);
      sp.cmd = const_cast<char*>(original_cmd);
      Dmsg3(debuglevel, "startBackup st_size=%p st_blocks=%p sp=%p\n",
            &sp.statp.st_size, &sp.statp.st_blocks, &sp);

      // Get the file save parameters. I.e. the stat pkt ...
      if (PlugFunc(ctx->plugin)->startBackupFile(ctx, &sp) != bRC_OK) {
        goto bail_out;
      }

      if (sp.type == 0) {
        Jmsg1(jcr, M_FATAL, 0,
              T_("Command plugin \"%s\": no type in startBackupFile packet.\n"),
              cmd.c_str());
        goto bail_out;
      }

      if (!IS_FT_OBJECT(sp.type)) {
        if (!sp.fname) {
          Jmsg1(jcr, M_FATAL, 0,
                T_("Command plugin \"%s\": no fname in startBackupFile "
                   "packet.\n"),
                cmd.c_str());
          goto bail_out;
        }

        // Count only files backed up
        switch (sp.type) {
          case FT_REGE:
          case FT_REG:
          case FT_LNK:
          case FT_DIREND:
          case FT_SPEC:
          case FT_RAW:
          case FT_FIFO:
          case FT_LNKSAVED:
            jcr->JobFiles++; /* increment number of files backed up */
            break;
          default:
            break;
        }
        jcr->fd_impl->num_files_examined++;

        if (sp.type != FT_LNKSAVED && S_ISREG(sp.statp.st_mode)) {
          if (sp.statp.st_size > 0) { jcr->JobBytes += sp.statp.st_size; }
        }

        if (jcr->fd_impl->listing) {
          memcpy(&attr.statp, &sp.statp, sizeof(struct stat));
          attr.type = sp.type;
          attr.ofname = (POOLMEM*)sp.fname;
          attr.olname = (POOLMEM*)sp.link;
          PrintLsOutput(jcr, &attr);
        }
      }

      Dmsg2(debuglevel, "startBackup returned type=%d, fname=%s\n", sp.type,
            sp.fname);
      if (sp.object) {
        Dmsg2(debuglevel, "index=%d object=%s\n", sp.index, sp.object);
      }

      bRC retval = PlugFunc(ctx->plugin)->endBackupFile(ctx);
      if (retval == bRC_More || retval == bRC_OK) {
        AccurateMarkFileAsSeen(jcr, sp.fname);
      }

      if (retval == bRC_More) { continue; }

      break;
    } /* end while loop */
  } else {
    Jmsg1(jcr, M_FATAL, 0, "Command plugin \"%s\" not found.\n", original_cmd);
  }

bail_out:
  jcr->cmd_plugin = false;

  return 1;
}

// Send plugin name start/end record to SD
bool SendPluginName(JobControlRecord* jcr, BareosSocket* sd, bool start)
{
  int status;
  auto index = jcr->JobFiles;
  save_pkt* sp = (save_pkt*)jcr->fd_impl->plugin_sp;

  if (!sp) {
    Jmsg0(jcr, M_FATAL, 0, T_("Plugin save packet not found.\n"));
    return false;
  }
  if (jcr->IsJobCanceled()) { return false; }

  if (start) { index++; /* JobFiles not incremented yet */ }
  Dmsg1(debuglevel, "SendPluginName=%s\n", sp->cmd);

  // Send stream header
  if (!sd->fsend("%" PRIu32 " %" PRId32 " 0", index, STREAM_PLUGIN_NAME)) {
    Jmsg1(jcr, M_FATAL, 0, T_("Network send error to SD. ERR=%s\n"),
          sd->bstrerror());
    return false;
  }
  Dmsg1(debuglevel, "send plugin name hdr: %s\n", sd->msg);

  if (start) {
    // Send data -- not much
    status
        = sd->fsend("%" PRIu32 " 1 %d %s%c", index, sp->portable, sp->cmd, 0);
  } else {
    // Send end of data
    status = sd->fsend("%" PRIu32 " 0", jcr->JobFiles);
  }
  if (!status) {
    Jmsg1(jcr, M_FATAL, 0, T_("Network send error to SD. ERR=%s\n"),
          sd->bstrerror());
    return false;
  }
  Dmsg1(debuglevel, "send plugin start/end: %s\n", sd->msg);
  sd->signal(BNET_EOD); /* indicate end of plugin name data */

  return true;
}

/**
 * Plugin name stream found during restore.  The record passed in argument
 * name was generated in SendPluginName() above.
 *
 * Returns: true  if start of stream
 *          false if end of steam
 */
bool PluginNameStream(JobControlRecord* jcr, char* name)
{
  int len;
  char* cmd;
  char* p = name;
  bool start;
  bool retval = true;
  alist<PluginContext*>* plugin_ctx_list;

  Dmsg1(debuglevel, "Read plugin stream string=%s\n", name);
  SkipNonspaces(&p); /* skip over jcr->JobFiles */
  SkipSpaces(&p);
  start = *p == '1';
  if (start) {
    // Start of plugin data
    SkipNonspaces(&p); /* skip start/end flag */
    SkipSpaces(&p);
    SkipNonspaces(&p); /* skip portable flag */
    SkipSpaces(&p);
    cmd = p;
  } else {
    // End of plugin data, notify plugin, then clear flags
    if (jcr->plugin_ctx) {
      Plugin* plugin = jcr->plugin_ctx->plugin;
      FiledPluginContext* b_ctx
          = (FiledPluginContext*)jcr->plugin_ctx->core_private_context;

      Dmsg2(debuglevel, "End plugin data plugin=%p ctx=%p\n", plugin,
            jcr->plugin_ctx);
      if (b_ctx->restoreFileStarted) {
        /* PlugFunc(plugin)->endRestoreFile(jcr->plugin_ctx); */
        bRC ret = PlugFunc(plugin)->endRestoreFile(jcr->plugin_ctx);
        Dmsg1(debuglevel, "endRestoreFile ret: %d\n", ret);
        if (ret == PYTHON_UNDEFINED_RETURN_VALUE) {
          Jmsg2(jcr, M_FATAL, 0,
                "Return value of endRestoreFile invalid: %d, plugin=%s\n", ret,
                jcr->plugin_ctx->plugin->file);
          retval = false;
        }
      }
      b_ctx->restoreFileStarted = false;
      b_ctx->createFileCalled = false;
    }

    goto bail_out;
  }

  plugin_ctx_list = jcr->plugin_ctx_list;
  if (!plugin_ctx_list) { goto bail_out; }

  // After this point, we are dealing with a restore start
  if (!GetPluginName(jcr, cmd, &len)) { goto bail_out; }

  // Search for correct plugin as specified on the command
  for (auto* ctx : plugin_ctx_list) {
    bEvent event;
    bEventType eventType;
    FiledPluginContext* b_ctx;

    Dmsg3(debuglevel, "plugin=%s cmd=%s len=%d\n", ctx->plugin->file, cmd, len);
    if (!IsEventForThisPlugin(ctx->plugin, cmd, len)) { continue; }

    if (IsPluginDisabled(ctx)) {
      Dmsg1(debuglevel, "Plugin %s disabled\n", cmd);
      goto bail_out;
    }

    Dmsg1(debuglevel, "Restore Command plugin = %s\n", cmd);
    eventType = bEventRestoreCommand;
    event.eventType = eventType;

    if (!IsEventEnabled(ctx, eventType)) {
      Dmsg1(debuglevel, "Event %d disabled for this plugin.\n", eventType);
      continue;
    }

    jcr->plugin_ctx = ctx;
    jcr->cmd_plugin = true;
    b_ctx = (FiledPluginContext*)ctx->core_private_context;

    if (PlugFunc(ctx->plugin)->handlePluginEvent(jcr->plugin_ctx, &event, cmd)
        != bRC_OK) {
      Dmsg1(debuglevel, "Handle event failed. Plugin=%s\n", cmd);
      goto bail_out;
    }

    if (b_ctx->restoreFileStarted) {
      Jmsg2(jcr, M_FATAL, 0,
            "Second call to startRestoreFile. plugin=%s cmd=%s\n",
            ctx->plugin->file, cmd);
      b_ctx->restoreFileStarted = false;
      goto bail_out;
    }

    if (PlugFunc(ctx->plugin)->startRestoreFile(jcr->plugin_ctx, cmd)
        == bRC_OK) {
      b_ctx->restoreFileStarted = true;
      goto bail_out;
    } else {
      Dmsg1(debuglevel, "startRestoreFile failed. plugin=%s\n", cmd);
      goto bail_out;
    }
  }

  // try using grpc if the plugin was not found normally
  {
    ResLocker _{my_config};

    ClientResource* res = dynamic_cast<ClientResource*>(
        my_config->GetNextRes(R_CLIENT, nullptr));

    PluginContext* ctx;

    if (res && !res->grpc_module.empty()
        && (ctx = grpc_plugin_context(plugin_ctx_list))) {
      std::string grpc_cmd = "grpc:";
      grpc_cmd += res->grpc_module;
      grpc_cmd += ":";
      grpc_cmd += cmd;

      Dmsg1(100, "using grpc to handle this event\n'%s' -> '%s'\n", cmd,
            grpc_cmd.c_str());

      jcr->plugin_ctx = ctx;
      jcr->cmd_plugin = true;
      auto* b_ctx
          = reinterpret_cast<FiledPluginContext*>(ctx->core_private_context);

      bEvent event = {bEventRestoreCommand};
      if (PlugFunc(ctx->plugin)
              ->handlePluginEvent(jcr->plugin_ctx, &event, grpc_cmd.data())
          != bRC_OK) {
        Dmsg1(debuglevel, "Handle event failed. Plugin=%s\n", grpc_cmd.c_str());
        goto bail_out;
      }

      if (b_ctx->restoreFileStarted) {
        Jmsg2(jcr, M_FATAL, 0,
              "Second call to startRestoreFile. plugin=%s cmd=%s\n",
              ctx->plugin->file, grpc_cmd.c_str());
        b_ctx->restoreFileStarted = false;
        goto bail_out;
      }

      if (PlugFunc(ctx->plugin)
              ->startRestoreFile(jcr->plugin_ctx, grpc_cmd.c_str())
          == bRC_OK) {
        b_ctx->restoreFileStarted = true;
        goto bail_out;
      } else {
        Dmsg1(debuglevel, "startRestoreFile failed. plugin=%s\n",
              grpc_cmd.c_str());
        goto bail_out;
      }
    }
  }

  Jmsg1(jcr, M_WARNING, 0, T_("Plugin=%s not found.\n"), cmd);

bail_out:
  return retval;
}

/**
 * Tell the plugin to create the file. this is called only during Restore.
 * Return values are:
 *
 * CF_ERROR    -> error
 * CF_SKIP     -> skip processing this file
 * CF_EXTRACT  -> extract the file (i.e.call i/o routines)
 * CF_CREATED  -> created, but no content to extract (typically directories)
 */
int PluginCreateFile(JobControlRecord* jcr,
                     Attributes* attr,
                     BareosFilePacket* bfd,
                     int)
{
  int flags;
  int retval;
  int status;
  Plugin* plugin;
  restore_pkt rp;
  PluginContext* ctx = jcr->plugin_ctx;
  FiledPluginContext* b_ctx
      = (FiledPluginContext*)jcr->plugin_ctx->core_private_context;

  if (!ctx || !SetCmdPlugin(bfd, jcr) || jcr->IsJobCanceled()) {
    return CF_ERROR;
  }
  plugin = ctx->plugin;

  rp.pkt_size = sizeof(rp);
  rp.pkt_end = sizeof(rp);
  rp.delta_seq = attr->delta_seq;
  rp.stream = attr->stream;
  rp.data_stream = attr->data_stream;
  rp.type = attr->type;
  rp.file_index = attr->file_index;
  rp.LinkFI = attr->LinkFI;
  rp.uid = attr->uid;
  memcpy(&rp.statp, &attr->statp, sizeof(rp.statp));
  rp.attrEx = attr->attrEx;
  rp.ofname = attr->ofname;
  rp.olname = attr->olname;
  rp.where = jcr->where;
  rp.RegexWhere = jcr->RegexWhere;
  rp.replace = jcr->fd_impl->replace;
  rp.create_status = CF_ERROR;
  rp.filedes = kInvalidFiledescriptor;

  Dmsg4(debuglevel,
        "call plugin createFile stream=%d type=%d LinkFI=%d File=%s\n",
        rp.stream, rp.type, rp.LinkFI, rp.ofname);
  if (rp.attrEx) { Dmsg1(debuglevel, "attrEx=\"%s\"\n", rp.attrEx); }

  if (!b_ctx->restoreFileStarted || b_ctx->createFileCalled) {
    Jmsg2(jcr, M_FATAL, 0, "Unbalanced call to createFile=%d %d\n",
          b_ctx->createFileCalled, b_ctx->restoreFileStarted);
    b_ctx->createFileCalled = false;
    return CF_ERROR;
  }

  retval = PlugFunc(plugin)->createFile(ctx, &rp);
  if (retval != bRC_OK) {
    Qmsg2(jcr, M_ERROR, 0,
          T_("Plugin createFile call failed. Stat=%d file=%s\n"), retval,
          attr->ofname);
    return CF_ERROR;
  }

  bfd->filedes = rp.filedes;
  switch (rp.create_status) {
    case CF_ERROR:
      Qmsg1(jcr, M_ERROR, 0,
            T_("Plugin createFile call failed. Returned CF_ERROR file=%s\n"),
            attr->ofname);
      [[fallthrough]];
    case CF_SKIP:
      [[fallthrough]];
    case CF_CORE:
      [[fallthrough]];
    case CF_CREATED:
      return rp.create_status;
  }

  flags = O_WRONLY | O_CREAT | O_TRUNC | O_BINARY;
  Dmsg0(debuglevel, "call bopen\n");

  status
      = bopen(bfd, attr->ofname, flags, S_IRUSR | S_IWUSR, attr->statp.st_rdev);
  Dmsg1(debuglevel, "bopen status=%d\n", status);

  if (status < 0) {
    BErrNo be;

    be.SetErrno(bfd->BErrNo);
    Qmsg2(jcr, M_ERROR, 0, T_("Could not create %s: ERR=%s\n"), attr->ofname,
          be.bstrerror());
    Dmsg2(debuglevel, "Could not bopen file %s: ERR=%s\n", attr->ofname,
          be.bstrerror());
    return CF_ERROR;
  }

  if (!IsBopen(bfd)) { Dmsg0(000, "===== BFD is not open!!!!\n"); }

  return CF_EXTRACT;
}

/**
 * Reset the file attributes after all file I/O is done -- this allows the
 * previous access time/dates to be set properly, and it also allows us to
 * properly set directory permissions.
 */
bool PluginSetAttributes(JobControlRecord* jcr,
                         Attributes* attr,
                         BareosFilePacket* ofd)
{
  Plugin* plugin;
  restore_pkt rp;

  Dmsg0(debuglevel, "PluginSetAttributes\n");

  if (!jcr->plugin_ctx) { return false; }
  plugin = (Plugin*)jcr->plugin_ctx->plugin;

  rp.stream = attr->stream;
  rp.data_stream = attr->data_stream;
  rp.type = attr->type;
  rp.file_index = attr->file_index;
  rp.LinkFI = attr->LinkFI;
  rp.uid = attr->uid;
  memcpy(&rp.statp, &attr->statp, sizeof(rp.statp));
  rp.attrEx = attr->attrEx;
  rp.ofname = attr->ofname;
  rp.olname = attr->olname;
  rp.where = jcr->where;
  rp.RegexWhere = jcr->RegexWhere;
  rp.replace = jcr->fd_impl->replace;
  rp.create_status = CF_ERROR;
  rp.filedes = kInvalidFiledescriptor;

  PlugFunc(plugin)->setFileAttributes(jcr->plugin_ctx, &rp);

  if (rp.create_status == CF_CORE) {
    SetAttributes(jcr, attr, ofd);
  } else {
    if (IsBopen(ofd)) { bclose(ofd); }
    PmStrcpy(attr->ofname, "*None*");
  }

  return true;
}

// Plugin specific callback for getting ACL information.
bacl_exit_code PluginBuildAclStreams(JobControlRecord* jcr,
                                     [[maybe_unused]] AclBuildData* acl_data,
                                     FindFilesPacket*)
{
  Plugin* plugin;

  Dmsg0(debuglevel, "PluginBuildAclStreams\n");

  if (!jcr->plugin_ctx) { return bacl_exit_ok; }
  plugin = (Plugin*)jcr->plugin_ctx->plugin;

  if (PlugFunc(plugin)->getAcl == NULL) {
    return bacl_exit_ok;
  } else {
    bacl_exit_code retval = bacl_exit_error;
#if defined(HAVE_ACL)
    acl_pkt ap;
    ap.fname = acl_data->last_fname;

    switch (PlugFunc(plugin)->getAcl(jcr->plugin_ctx, &ap)) {
      case bRC_OK:
        if (ap.content_length && ap.content) {
          acl_data->content.check_size(ap.content_length + 1);
          PmMemcpy(acl_data->content, ap.content, ap.content_length);
          acl_data->content_length = ap.content_length;
          free(ap.content);
          acl_data->content.c_str()[acl_data->content_length] = '\0';
          retval = SendAclStream(jcr, acl_data, STREAM_ACL_PLUGIN);
        } else {
          retval = bacl_exit_ok;
        }
        break;
      default:
        break;
    }
#endif

    return retval;
  }
}

// Plugin specific callback for setting ACL information.
bacl_exit_code plugin_parse_acl_streams(
    JobControlRecord* jcr,
    [[maybe_unused]] AclData* acl_data,
    int,
    [[maybe_unused]] char* content,
    [[maybe_unused]] uint32_t content_length)
{
  Plugin* plugin;

  Dmsg0(debuglevel, "plugin_parse_acl_streams\n");

  if (!jcr->plugin_ctx) { return bacl_exit_ok; }
  plugin = (Plugin*)jcr->plugin_ctx->plugin;

  if (PlugFunc(plugin)->setAcl == NULL) {
    return bacl_exit_error;
  } else {
    bacl_exit_code retval = bacl_exit_error;
#if defined(HAVE_ACL)
    acl_pkt ap;
    ap.fname = acl_data->last_fname;
    ap.content = content;
    ap.content_length = content_length;

    switch (PlugFunc(plugin)->setAcl(jcr->plugin_ctx, &ap)) {
      case bRC_OK:
        retval = bacl_exit_ok;
        break;
      default:
        break;
    }
#endif

    return retval;
  }
}

// Plugin specific callback for getting XATTR information.
BxattrExitCode PluginBuildXattrStreams(
    JobControlRecord* jcr,
    [[maybe_unused]] XattrBuildData* xattr_data,
    FindFilesPacket*)
{
  Plugin* plugin;
#if defined(HAVE_XATTR)
  alist<xattr_t*>* xattr_value_list = NULL;
#endif
  BxattrExitCode retval = BxattrExitCode::kError;

  Dmsg0(debuglevel, "PluginBuildXattrStreams\n");

  if (!jcr->plugin_ctx) { return BxattrExitCode::kSuccess; }
  plugin = (Plugin*)jcr->plugin_ctx->plugin;

  if (PlugFunc(plugin)->getXattr == NULL) {
    return BxattrExitCode::kSuccess;
  } else {
#if defined(HAVE_XATTR)
    bool more;
    int xattr_count = 0;
    xattr_t* current_xattr;
    uint32_t expected_serialize_len = 0;

    while (1) {
      xattr_pkt xp;
      xp.fname = xattr_data->last_fname;

      switch (PlugFunc(plugin)->getXattr(jcr->plugin_ctx, &xp)) {
        case bRC_OK:
          more = false;
          break;
        case bRC_More:
          more = true;
          break;
        default:
          goto bail_out;
      }

      /* Make sure the plugin filled a XATTR name.
       * The name and value returned by the plugin need to be in allocated
       * memory and are freed by XattrDropInternalTable() function when we are
       * done processing the data. */
      if (xp.name_length && xp.name) {
        // Each xattr valuepair starts with a magic so we can parse it easier.
        current_xattr = (xattr_t*)malloc(sizeof(xattr_t));
        current_xattr->magic = XATTR_MAGIC;
        expected_serialize_len += sizeof(current_xattr->magic);

        current_xattr->name_length = xp.name_length;
        current_xattr->name = xp.name;
        expected_serialize_len
            += sizeof(current_xattr->name_length) + current_xattr->name_length;

        current_xattr->value_length = xp.value_length;
        current_xattr->value = xp.value;
        expected_serialize_len += sizeof(current_xattr->value_length)
                                  + current_xattr->value_length;

        if (xattr_value_list == NULL) {
          xattr_value_list = new alist<xattr_t*>(10, not_owned_by_alist);
        }

        xattr_value_list->append(current_xattr);
        xattr_count++;

        // Protect ourself against things getting out of hand.
        if (expected_serialize_len >= MAX_XATTR_STREAM) {
          Mmsg2(jcr->errmsg,
                T_("Xattr stream on file \"%s\" exceeds maximum size of %d "
                   "bytes\n"),
                xattr_data->last_fname, MAX_XATTR_STREAM);
          goto bail_out;
        }
      }

      // Does the plugin have more xattrs ?
      if (!more) { break; }
    }

    // If we found any xattr send them to the SD.
    if (xattr_count > 0) {
      retval
          = SerializeAndSendXattrStream(jcr, xattr_data, expected_serialize_len,
                                        xattr_value_list, STREAM_XATTR_PLUGIN);
    } else {
      retval = BxattrExitCode::kSuccess;
    }
#endif
  }

#if defined(HAVE_XATTR)
bail_out:
  if (xattr_value_list) { XattrDropInternalTable(xattr_value_list); }
#endif

  return retval;
}

// Plugin specific callback for setting XATTR information.
BxattrExitCode PluginParseXattrStreams(JobControlRecord* jcr,
                                       [[maybe_unused]] XattrData* xattr_data,
                                       int,
                                       [[maybe_unused]] char* content,
                                       [[maybe_unused]] uint32_t content_length)
{
#if defined(HAVE_XATTR)
  Plugin* plugin;
  alist<xattr_t*>* xattr_value_list = NULL;
#endif
  BxattrExitCode retval = BxattrExitCode::kError;

  Dmsg0(debuglevel, "PluginParseXattrStreams\n");

  if (!jcr->plugin_ctx) { return BxattrExitCode::kSuccess; }

#if defined(HAVE_XATTR)
  plugin = (Plugin*)jcr->plugin_ctx->plugin;
  if (PlugFunc(plugin)->setXattr != NULL) {
    xattr_value_list = new alist<xattr_t*>(10, not_owned_by_alist);

    if (UnSerializeXattrStream(jcr, xattr_data, content, content_length,
                               xattr_value_list)
        != BxattrExitCode::kSuccess) {
      goto bail_out;
    }

    xattr_pkt xp;
    xp.fname = xattr_data->last_fname;

    for (auto* current_xattr : xattr_value_list) {
      xp.name = current_xattr->name;
      xp.name_length = current_xattr->name_length;
      xp.value = current_xattr->value;
      xp.value_length = current_xattr->value_length;

      switch (PlugFunc(plugin)->setXattr(jcr->plugin_ctx, &xp)) {
        case bRC_OK:
          break;
        default:
          goto bail_out;
      }
    }

    retval = BxattrExitCode::kSuccess;
  }

bail_out:
  if (xattr_value_list) { XattrDropInternalTable(xattr_value_list); }
#endif

  return retval;
}

// Print to file the plugin info.
static void DumpFdPlugin(Plugin* plugin, FILE* fp)
{
  PluginInformation* info;

  if (!plugin) { return; }

  info = (PluginInformation*)plugin->plugin_information;
  if (!info) { return; }

  fprintf(fp, "\tversion=%d\n", info->version);
  fprintf(fp, "\tdate=%s\n", NPRTB(info->plugin_date));
  fprintf(fp, "\tmagic=%s\n", NPRTB(info->plugin_magic));
  fprintf(fp, "\tauthor=%s\n", NPRTB(info->plugin_author));
  fprintf(fp, "\tlicence=%s\n", NPRTB(info->plugin_license));
  fprintf(fp, "\tversion=%s\n", NPRTB(info->plugin_version));
  fprintf(fp, "\tdescription=%s\n", NPRTB(info->plugin_description));
}

static void DumpFdPlugins(FILE* fp) { DumpPlugins(fd_plugin_list, fp); }

/**
 * This entry point is called internally by Bareos to ensure that the plugin
 * IO calls come into this code.
 */
void LoadFdPlugins(const char* plugin_dir, alist<const char*>* plugin_names)
{
  Plugin* plugin;

  if (!plugin_dir) {
    Dmsg0(debuglevel, "plugin dir is NULL\n");
    return;
  }

  fd_plugin_list = new alist<Plugin*>(10, not_owned_by_alist);
  if (!LoadPlugins((void*)&bareos_plugin_interface_version,
                   (void*)&bareos_core_functions, fd_plugin_list, plugin_dir,
                   plugin_names, plugin_type, IsPluginCompatible)) {
    // Either none found, or some error
    if (fd_plugin_list->size() == 0) {
      delete fd_plugin_list;
      fd_plugin_list = NULL;
      Dmsg0(debuglevel, "No plugins loaded\n");
      return;
    }
  }

  // Plug entry points called from findlib
  plugin_bopen = MyPluginBopen;
  plugin_bclose = MyPluginBclose;
  plugin_bread = MyPluginBread;
  plugin_bwrite = MyPluginBwrite;
  plugin_blseek = MyPluginBlseek;

  // Verify that the plugin is acceptable, and print information about it.
  int i{0};
  foreach_alist_index (i, plugin, fd_plugin_list) {
    Dmsg1(debuglevel, "Loaded plugin: %s\n", plugin->file);
  }

  DbgPluginAddHook(DumpFdPlugin);
  DbgPrintPluginAddHook(DumpFdPlugins);
}

void UnloadFdPlugins(void)
{
  UnloadPlugins(fd_plugin_list);
  delete fd_plugin_list;
  fd_plugin_list = NULL;
}

int ListFdPlugins(PoolMem& msg) { return ListPlugins(fd_plugin_list, msg); }

/**
 * Check if a plugin is compatible.  Called by the load_plugin function to
 * allow us to verify the plugin.
 */
static bool IsPluginCompatible(Plugin* plugin)
{
  PluginInformation* info = (PluginInformation*)plugin->plugin_information;
  Dmsg0(debuglevel, "IsPluginCompatible called\n");
  if (!info) {
    Dmsg0(debuglevel, "IsPluginCompatible: plugin_information is empty\n");
    return false;
  }
  if (debug_level >= 50) { DumpFdPlugin(plugin, stdin); }
  if (!bstrcmp(info->plugin_magic, FD_PLUGIN_MAGIC)) {
    Jmsg(NULL, M_ERROR, 0,
         T_("Plugin magic wrong. Plugin=%s wanted=%s got=%s\n"), plugin->file,
         FD_PLUGIN_MAGIC, info->plugin_magic);
    Dmsg3(50, "Plugin magic wrong. Plugin=%s wanted=%s got=%s\n", plugin->file,
          FD_PLUGIN_MAGIC, info->plugin_magic);

    return false;
  }
  if (info->version != FD_PLUGIN_INTERFACE_VERSION) {
    Jmsg(NULL, M_ERROR, 0,
         T_("Plugin version incorrect. Plugin=%s wanted=%d got=%d\n"),
         plugin->file, FD_PLUGIN_INTERFACE_VERSION, info->version);
    Dmsg3(50, "Plugin version incorrect. Plugin=%s wanted=%d got=%d\n",
          plugin->file, FD_PLUGIN_INTERFACE_VERSION, info->version);
    return false;
  }
  if (!Bstrcasecmp(info->plugin_license, "Bareos AGPLv3")
      && !Bstrcasecmp(info->plugin_license, "AGPLv3")) {
    Jmsg(NULL, M_ERROR, 0,
         T_("Plugin license incompatible. Plugin=%s license=%s\n"),
         plugin->file, info->plugin_license);
    Dmsg2(50, "Plugin license incompatible. Plugin=%s license=%s\n",
          plugin->file, info->plugin_license);
    return false;
  }
  if (info->size != sizeof(PluginInformation)) {
    Jmsg(NULL, M_ERROR, 0,
         T_("Plugin size incorrect. Plugin=%s wanted=%" PRIuz " got=%" PRIu32
            "\n"),
         plugin->file, sizeof(PluginInformation), info->size);
    return false;
  }

  return true;
}

// Instantiate a new plugin instance.
static inline PluginContext* instantiate_plugin(JobControlRecord* jcr,
                                                Plugin* plugin,
                                                char instance)
{
  FiledPluginContext* b_ctx = new FiledPluginContext(jcr, plugin);
  PluginContext* ctx = (PluginContext*)malloc(sizeof(PluginContext));
  ctx->instance = instance;
  ctx->plugin = plugin;
  ctx->core_private_context = b_ctx;
  ctx->plugin_private_context = NULL;

  jcr->plugin_ctx_list->append(ctx);

  if (PlugFunc(plugin)->newPlugin(ctx) != bRC_OK) { b_ctx->disabled = true; }

  return ctx;
}

/**
 * Create a new instance of each plugin for this Job
 *
 * Note, fd_plugin_list can exist but jcr->plugin_ctx_list can be NULL if no
 * plugins were loaded.
 */
void NewPlugins(JobControlRecord* jcr)
{
  Plugin* plugin;

  if (!fd_plugin_list) {
    Dmsg0(debuglevel, "plugin list is NULL\n");
    return;
  }

  if (jcr->IsJobCanceled()) { return; }

  int num;
  num = fd_plugin_list->size();
  if (num == 0) {
    Dmsg0(debuglevel, "No plugins loaded\n");
    return;
  }

  jcr->plugin_ctx_list = new alist<PluginContext*>(10, owned_by_alist);
  Dmsg2(debuglevel, "Instantiate plugin_ctx=%p JobId=%d\n",
        jcr->plugin_ctx_list, jcr->JobId);

  int i{};
  foreach_alist_index (i, plugin, fd_plugin_list) {
    // Start a new instance of each plugin
    instantiate_plugin(jcr, plugin, 0);
  }
}

// Free the plugin instances for this Job
void FreePlugins(JobControlRecord* jcr)
{
  if (!fd_plugin_list || !jcr->plugin_ctx_list) { return; }

  Dmsg2(debuglevel, "Free instance fd-plugin_ctx_list=%p JobId=%d\n",
        jcr->plugin_ctx_list, jcr->JobId);
  for (auto* ctx : jcr->plugin_ctx_list) {
    // Free the plugin instance
    PlugFunc(ctx->plugin)->freePlugin(ctx);
    delete static_cast<FiledPluginContext*>(ctx->core_private_context);
  }

  delete jcr->plugin_ctx_list;
  jcr->plugin_ctx_list = NULL;
}

/** Entry point for opening the file this is a wrapper around
    the pluginIO entry point in the plugin.  */
static int MyPluginBopen(BareosFilePacket* bfd,
                         const char* fname,
                         int flags,
                         mode_t mode)
{
  Plugin* plugin;
  io_pkt io;
  JobControlRecord* jcr = bfd->jcr;

  Dmsg1(debuglevel, "plugin_bopen flags=%08o\n", flags);
  if (!jcr->plugin_ctx) { return 0; }
  plugin = (Plugin*)jcr->plugin_ctx->plugin;

  io.filedes = kInvalidFiledescriptor;

  io.func = IO_OPEN;
  io.fname = fname;
  io.flags = flags;
  io.mode = mode;

  auto res = PlugFunc(plugin)->pluginIO(jcr->plugin_ctx, &io);
  bfd->BErrNo = io.io_errno;

  if (io.win32) {
    errno = b_errno_win32;
  } else {
    errno = io.io_errno;
    bfd->lerror = io.lerror;
  }

  if (res == bRC_Error) { return -1; }

  //  The plugin has two options for the read/write during the IO_OPEN call:
  //  1.: - Set io.status to IoStatus::do_io_in_core, and
  //      - Set the io.filedes to the filedescriptor of the file that was
  //        opened in the plugin. In this case the core code will read from
  //        write to that filedescriptor during backup and restore.
  //
  //  2.: - Set io.status to bareosfd.iostat_do_in_plugin.
  //        This will call the plugin to do the IO itself.

  bfd->filedes = io.filedes;

  if (io.status == IoStatus::do_io_in_core) { bfd->do_io_in_core = true; }

  if (bfd->do_io_in_core) {
    if (io.filedes != kInvalidFiledescriptor) {
      Dmsg1(
          debuglevel,
          "bopen: plugin asks for core to do the read/write via filedescriptor "
          "%d\n",
          io.filedes);
      return io.filedes;
    } else {
      Dmsg1(debuglevel,
            "bopen: ERROR: plugin wants to do read/write itself but did not "
            "return a valid filedescriptor: %d\n",
            io.filedes);
      return -2;
    }
  } else {
    Dmsg1(debuglevel,
          "bopen: plugin wants to do read/write itself. status: %d\n",
          io.status);
    bfd->filedes = 0;  // set filedes to 0 as otherwise IsBopen() fails.
    return io.status;
  }
}

/**
 * Entry point for closing the file this is a wrapper around the pluginIO
 * entry point in the plugin.
 */
static int MyPluginBclose(BareosFilePacket* bfd)
{
  Plugin* plugin;
  io_pkt io;
  JobControlRecord* jcr = bfd->jcr;

  Dmsg0(debuglevel, "===== plugin_bclose\n");
  if (!jcr->plugin_ctx) { return 0; }
  plugin = (Plugin*)jcr->plugin_ctx->plugin;

  io.filedes = bfd->filedes;

  io.func = IO_CLOSE;

  PlugFunc(plugin)->pluginIO(jcr->plugin_ctx, &io);
  bfd->BErrNo = io.io_errno;

  if (io.win32) {
    errno = b_errno_win32;
  } else {
    errno = io.io_errno;
    bfd->lerror = io.lerror;
  }

  Dmsg1(debuglevel, "plugin_bclose stat=%d\n", io.status);
  bfd->filedes = kInvalidFiledescriptor;
  return io.status;
}

/**
 * Entry point for reading from the file this is a wrapper around the pluginIO
 * entry point in the plugin.
 */
static ssize_t MyPluginBread(BareosFilePacket* bfd, void* buf, size_t count)
{
  Plugin* plugin;
  io_pkt io;
  JobControlRecord* jcr = bfd->jcr;

  Dmsg0(debuglevel, "plugin_bread\n");
  if (!jcr->plugin_ctx) { return 0; }
  plugin = (Plugin*)jcr->plugin_ctx->plugin;

  io.filedes = bfd->filedes;

  io.func = IO_READ;
  io.count = count;
  io.buf = (char*)buf;

  PlugFunc(plugin)->pluginIO(jcr->plugin_ctx, &io);
  bfd->offset = io.offset;
  bfd->BErrNo = io.io_errno;

  if (io.win32) {
    errno = b_errno_win32;
  } else {
    errno = io.io_errno;
    bfd->lerror = io.lerror;
  }

  return (ssize_t)io.status;
}

/**
 * Entry point for writing to the file this is a wrapper around the pluginIO
 * entry point in the plugin.
 */
static ssize_t MyPluginBwrite(BareosFilePacket* bfd, void* buf, size_t count)
{
  Plugin* plugin;
  io_pkt io;
  JobControlRecord* jcr = bfd->jcr;

  Dmsg0(debuglevel, "plugin_bwrite\n");
  if (!jcr->plugin_ctx) {
    Dmsg0(0, "No plugin context\n");
    return 0;
  }
  plugin = (Plugin*)jcr->plugin_ctx->plugin;


  io.func = IO_WRITE;
  io.count = count;
  io.buf = (char*)buf;

  PlugFunc(plugin)->pluginIO(jcr->plugin_ctx, &io);
  bfd->BErrNo = io.io_errno;

  if (io.win32) {
    errno = b_errno_win32;
  } else {
    errno = io.io_errno;
    bfd->lerror = io.lerror;
  }

  return (ssize_t)io.status;
}

/**
 * Entry point for seeking in the file this is a wrapper around the pluginIO
 * entry point in the plugin.
 */
static boffset_t MyPluginBlseek(BareosFilePacket* bfd,
                                boffset_t offset,
                                int whence)
{
  Plugin* plugin;
  io_pkt io;
  JobControlRecord* jcr = bfd->jcr;

  Dmsg0(debuglevel, "plugin_bseek\n");
  if (!jcr->plugin_ctx) { return 0; }
  plugin = (Plugin*)jcr->plugin_ctx->plugin;

  io.pkt_size = sizeof(io);
  io.pkt_end = sizeof(io);
  io.func = IO_SEEK;
  io.offset = offset;
  io.whence = whence;
  io.win32 = false;
  io.lerror = 0;
  PlugFunc(plugin)->pluginIO(jcr->plugin_ctx, &io);
  bfd->BErrNo = io.io_errno;
  if (io.win32) {
    errno = b_errno_win32;
  } else {
    errno = io.io_errno;
    bfd->lerror = io.lerror;
  }

  return (boffset_t)io.offset;
}

/* ==============================================================
 *
 * Callbacks from the plugin
 *
 * ==============================================================
 */
static bRC bareosGetValue(PluginContext* ctx, bVariable var, void* value)
{
  JobControlRecord* jcr = NULL;
  if (!value) { return bRC_Error; }

  switch (var) { /* General variables, no need of ctx */
    case bVarFDName:
      *((char**)value) = my_name;
      break;
    case bVarWorkingDir:
      *(char**)value = me->working_directory;
      break;
    case bVarPluginPath:
      *(const char**)value = me->plugin_directory;
      break;
    case bVarUsedConfig:
      *(const char**)value = my_config->get_base_config_path().c_str();
      break;
    case bVarExePath:
      *(char**)value = exepath;
      break;
    case bVarVersion:
      *(const char**)value = kBareosVersionStrings.FullWithDate;
      break;
    case bVarDistName:
      /* removed, as this value was never used by any plugin */
      return bRC_Error;
    case bVarCheckChanges: {
      *static_cast<bool*>(value)
          = static_cast<FiledPluginContext*>(ctx->core_private_context)
                ->check_changes;
      break;
    }
    default:
      if (!ctx) { /* Other variables need context */
        return bRC_Error;
      }

      jcr = ((FiledPluginContext*)ctx->core_private_context)->jcr;
      if (!jcr) { return bRC_Error; }
      break;
  }

  if (jcr) {
    switch (var) {
      case bVarJobId:
        *((int*)value) = jcr->JobId;
        Dmsg1(debuglevel, "fd-plugin: return bVarJobId=%d\n", jcr->JobId);
        break;
      case bVarLevel:
        *((int*)value) = jcr->getJobLevel();
        Dmsg1(debuglevel, "fd-plugin: return bVarJobLevel=%d\n",
              jcr->getJobLevel());
        break;
      case bVarType:
        *((int*)value) = jcr->getJobType();
        Dmsg1(debuglevel, "fd-plugin: return bVarJobType=%d\n",
              jcr->getJobType());
        break;
      case bVarClient:
        *((char**)value) = jcr->client_name;
        Dmsg1(debuglevel, "fd-plugin: return bVarClient=%s\n",
              NPRT(*((char**)value)));
        break;
      case bVarJobName:
        *((char**)value) = jcr->Job;
        Dmsg1(debuglevel, "fd-plugin: return bVarJobName=%s\n",
              NPRT(*((char**)value)));
        break;
      case bVarPrevJobName:
        *((char**)value) = jcr->fd_impl->PrevJob;
        Dmsg1(debuglevel, "fd-plugin: return bVarPrevJobName=%s\n",
              NPRT(*((char**)value)));
        break;
      case bVarJobStatus:
        *((int*)value) = jcr->getJobStatus();
        Dmsg1(debuglevel, "fd-plugin: return bVarJobStatus=%d\n",
              jcr->getJobStatus());
        break;
      case bVarSinceTime:
        *((int*)value) = (int)jcr->fd_impl->since_time;
        Dmsg1(debuglevel, "fd-plugin: return bVarSinceTime=%d\n",
              (int)jcr->fd_impl->since_time);
        break;
      case bVarAccurate:
        *((int*)value) = (int)jcr->accurate;
        Dmsg1(debuglevel, "fd-plugin: return bVarAccurate=%d\n",
              (int)jcr->accurate);
        break;
      case bVarFileSeen:
        break; /* a write only variable, ignore read request */
      case bVarVssClient:
#ifdef HAVE_WIN32
        if (jcr->fd_impl->pVSSClient) {
          *(void**)value = jcr->fd_impl->pVSSClient;
          Dmsg1(debuglevel, "fd-plugin: return bVarVssClient=%p\n",
                *(void**)value);
          break;
        }
#endif
        return bRC_Error;
      case bVarWhere:
        *(char**)value = jcr->where;
        Dmsg1(debuglevel, "fd-plugin: return bVarWhere=%s\n",
              NPRT(*((char**)value)));
        break;
      case bVarRegexWhere:
        *(char**)value = jcr->RegexWhere;
        Dmsg1(debuglevel, "fd-plugin: return bVarRegexWhere=%s\n",
              NPRT(*((char**)value)));
        break;
      case bVarPrefixLinks:
        *(int*)value = (int)jcr->prefix_links;
        Dmsg1(debuglevel, "fd-plugin: return bVarPrefixLinks=%d\n",
              (int)jcr->prefix_links);
        break;
      default:
        break;
    }
  }

  return bRC_OK;
}

static bRC bareosSetValue(PluginContext* ctx, bVariable var, const void* value)
{
  JobControlRecord* jcr;

  if (!value || !ctx) { return bRC_Error; }

  jcr = ((FiledPluginContext*)ctx->core_private_context)->jcr;
  if (!jcr) { return bRC_Error; }

  switch (var) {
    case bVarCheckChanges: {
      const bool bool_value = *static_cast<const bool*>(value);
      static_cast<FiledPluginContext*>(ctx->core_private_context)->check_changes
          = bool_value;
      Dmsg0(100, "check_changes set to %d\n", bool_value);
      return bRC_OK;
    }
    case bVarFileSeen:
      if (!AccurateMarkFileAsSeen(jcr, (char*)value)) { return bRC_Error; }
      break;
    default:
      Jmsg1(jcr, M_ERROR, 0,
            "Warning: bareosSetValue not implemented for var %d.\n", var);
      break;
  }

  return bRC_OK;
}

static bRC bareosRegisterEvents(PluginContext* ctx, int nr_events, ...)
{
  int i;
  va_list args;
  uint32_t event;
  FiledPluginContext* b_ctx;

  if (!ctx) { return bRC_Error; }
  b_ctx = (FiledPluginContext*)ctx->core_private_context;

  va_start(args, nr_events);
  for (i = 0; i < nr_events; i++) {
    event = va_arg(args, uint32_t);
    Dmsg1(debuglevel, "fd-plugin: Plugin registered event=%u\n", event);
    SetBit(event, b_ctx->events);
  }
  va_end(args);

  return bRC_OK;
}

// Get the number of instaces instantiated of a certain plugin.
static bRC bareosGetInstanceCount(PluginContext* ctx, int* ret)
{
  int cnt;
  JobControlRecord *jcr, *njcr;
  FiledPluginContext* bctx;
  bRC retval = bRC_Error;

  if (!IsCtxGood(ctx, jcr, bctx)) { goto bail_out; }

  lock_mutex(mutex);

  cnt = 0;
  foreach_jcr (njcr) {
    for (auto* nctx : jcr->plugin_ctx_list) {
      if (nctx->plugin == bctx->plugin) { cnt++; }
    }
  }
  endeach_jcr(njcr);

  unlock_mutex(mutex);

  *ret = cnt;
  retval = bRC_OK;

bail_out:
  return retval;
}

static bRC bareosUnRegisterEvents(PluginContext* ctx, int nr_events, ...)
{
  int i;
  va_list args;
  uint32_t event;
  FiledPluginContext* b_ctx;

  if (!ctx) { return bRC_Error; }
  b_ctx = (FiledPluginContext*)ctx->core_private_context;

  va_start(args, nr_events);
  for (i = 0; i < nr_events; i++) {
    event = va_arg(args, uint32_t);
    Dmsg1(debuglevel, "fd-plugin: Plugin unregistered event=%u\n", event);
    ClearBit(event, b_ctx->events);
  }
  va_end(args);

  return bRC_OK;
}

static bRC bareosJobMsg(PluginContext* ctx,
                        const char*,
                        int,
                        int type,
                        utime_t mtime,
                        const char* fmt,
                        ...)
{
  JobControlRecord* jcr;
  va_list arg_ptr;
  PoolMem buffer(PM_MESSAGE);

  if (ctx) {
    jcr = ((FiledPluginContext*)ctx->core_private_context)->jcr;
  } else {
    jcr = NULL;
  }

  va_start(arg_ptr, fmt);
  buffer.Bvsprintf(fmt, arg_ptr);
  va_end(arg_ptr);
  Jmsg(jcr, type, mtime, "%s", buffer.c_str());

  return bRC_OK;
}

static bRC bareosDebugMsg(PluginContext* ctx,
                          const char* fname,
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
  d_msg(fname, line, level, "%s: %s",
        ctx ? ctx->plugin->file : "<unknown plugin>", buffer.c_str());

  return bRC_OK;
}

static void* bareosMalloc(PluginContext*, const char*, int, size_t size)
{
  return malloc(size);
}

static void bareosFree(PluginContext*, const char*, int, void* mem)
{
  free(mem);
}

/**
 * Let the plugin define files/directories to be excluded from the main
 * backup.
 */
static bRC bareosAddExclude(PluginContext* ctx, const char* fname)
{
  JobControlRecord* jcr;
  findIncludeExcludeItem* old;
  FiledPluginContext* bctx;

  if (!IsCtxGood(ctx, jcr, bctx)) { return bRC_Error; }
  if (!fname) { return bRC_Error; }

  // Save the include context
  old = get_incexe(jcr);

  // Not right time to add exlude
  if (!old) { return bRC_Error; }

  if (!bctx->exclude) {
    bctx->exclude = new_exclude(jcr->fd_impl->ff->fileset);
  }

  // Set the Exclude context
  SetIncexe(jcr, bctx->exclude);

  AddFileToFileset(jcr, fname, true, jcr->fd_impl->ff->fileset);

  // Restore the current context
  SetIncexe(jcr, old);

  Dmsg1(100, "Add exclude file=%s\n", fname);

  return bRC_OK;
}

/**
 * Let the plugin define files/directories to be excluded from the main
 * backup.
 */
static bRC bareosAddInclude(PluginContext* ctx, const char* fname)
{
  JobControlRecord* jcr;
  findIncludeExcludeItem* old;
  FiledPluginContext* bctx;

  if (!IsCtxGood(ctx, jcr, bctx)) { return bRC_Error; }

  if (!fname) { return bRC_Error; }

  // Save the include context
  old = get_incexe(jcr);

  // Not right time to add include
  if (!old) { return bRC_Error; }

  if (!bctx->include) { bctx->include = old; }

  SetIncexe(jcr, bctx->include);
  AddFileToFileset(jcr, fname, true, jcr->fd_impl->ff->fileset);

  // Restore the current context
  SetIncexe(jcr, old);

  Dmsg1(100, "Add include file=%s\n", fname);

  return bRC_OK;
}

static bRC bareosAddOptions(PluginContext* ctx, const char* opts)
{
  JobControlRecord* jcr;
  FiledPluginContext* bctx;

  if (!IsCtxGood(ctx, jcr, bctx)) { return bRC_Error; }

  if (!opts) { return bRC_Error; }
  AddOptionsFlagsToFileset(jcr, opts);
  Dmsg1(1000, "Add options=%s\n", opts);

  return bRC_OK;
}

static bRC bareosAddRegex(PluginContext* ctx, const char* item, int type)
{
  JobControlRecord* jcr;
  FiledPluginContext* bctx;

  if (!IsCtxGood(ctx, jcr, bctx)) { return bRC_Error; }

  if (!item) { return bRC_Error; }
  AddRegexToFileset(jcr, item, type);
  Dmsg1(100, "Add regex=%s\n", item);

  return bRC_OK;
}

static bRC bareosAddWild(PluginContext* ctx, const char* item, int type)
{
  JobControlRecord* jcr;
  FiledPluginContext* bctx;

  if (!IsCtxGood(ctx, jcr, bctx)) { return bRC_Error; }

  if (!item) { return bRC_Error; }
  AddWildToFileset(jcr, item, type);
  Dmsg1(100, "Add wild=%s\n", item);

  return bRC_OK;
}

static bRC bareosNewOptions(PluginContext* ctx)
{
  JobControlRecord* jcr;
  FiledPluginContext* bctx;

  if (!IsCtxGood(ctx, jcr, bctx)) { return bRC_Error; }
  (void)NewOptions(jcr->fd_impl->ff, jcr->fd_impl->ff->fileset->incexe);

  return bRC_OK;
}

static bRC bareosNewInclude(PluginContext* ctx)
{
  JobControlRecord* jcr;
  FiledPluginContext* bctx;

  if (!IsCtxGood(ctx, jcr, bctx)) { return bRC_Error; }
  (void)new_include(jcr->fd_impl->ff->fileset);

  return bRC_OK;
}

static bRC bareosNewPreInclude(PluginContext* ctx)
{
  JobControlRecord* jcr;
  FiledPluginContext* bctx;

  if (!IsCtxGood(ctx, jcr, bctx)) { return bRC_Error; }

  bctx->include = new_preinclude(jcr->fd_impl->ff->fileset);
  NewOptions(jcr->fd_impl->ff, bctx->include);
  SetIncexe(jcr, bctx->include);

  return bRC_OK;
}

// Check if a file have to be backed up using Accurate code
static bRC bareosCheckChanges(PluginContext* ctx, save_pkt* sp)
{
  JobControlRecord* jcr;
  FiledPluginContext* bctx;
  FindFilesPacket* ff_pkt;
  bRC retval = bRC_Error;

  if (!IsCtxGood(ctx, jcr, bctx)) { goto bail_out; }

  if (!sp) { goto bail_out; }

  ff_pkt = jcr->fd_impl->ff;
  /* Copy fname and link because SaveFile() zaps them.
   * This avoids zapping the plugin's strings. */
  ff_pkt->type = sp->type;
  if (!sp->fname) {
    Jmsg0(jcr, M_FATAL, 0,
          T_("Command plugin: no fname in bareosCheckChanges packet.\n"));
    goto bail_out;
  }

  {
    const auto orig_save_time{ff_pkt->save_time};
    const auto orig_incremental{ff_pkt->incremental};

    ff_pkt->fname = sp->fname;
    ff_pkt->link = sp->link;
    if (sp->save_time) {
      ff_pkt->save_time = sp->save_time;
      ff_pkt->incremental = true;
    }
    memcpy(&ff_pkt->statp, &sp->statp, sizeof(ff_pkt->statp));

    if (!bctx->check_changes || CheckChanges(jcr, ff_pkt)) {
      retval = bRC_OK;
    } else {
      retval = bRC_Seen;
    }

    ff_pkt->save_time = orig_save_time;
    ff_pkt->incremental = orig_incremental;
  }
  // CheckChanges() can update delta sequence number, return it to the plugin
  sp->delta_seq = ff_pkt->delta_seq;
  sp->accurate_found = ff_pkt->accurate_found;

bail_out:
  Dmsg1(100, "checkChanges=%i\n", retval);
  return retval;
}

// Check if a file would be saved using current Include/Exclude code
static bRC bareosAcceptFile(PluginContext* ctx, save_pkt* sp)
{
  JobControlRecord* jcr;
  FindFilesPacket* ff_pkt;
  FiledPluginContext* bctx;
  bRC retval = bRC_Error;

  if (!IsCtxGood(ctx, jcr, bctx)) { goto bail_out; }
  if (!sp) { goto bail_out; }

  ff_pkt = jcr->fd_impl->ff;

  ff_pkt->fname = sp->fname;
  memcpy(&ff_pkt->statp, &sp->statp, sizeof(ff_pkt->statp));

  if (AcceptFile(ff_pkt)) {
    retval = bRC_OK;
  } else {
    retval = bRC_Skip;
  }

bail_out:
  return retval;
}

// Manipulate the accurate seen bitmap for setting bits
static bRC bareosSetSeenBitmap(PluginContext* ctx, bool all, char* fname)
{
  JobControlRecord* jcr;
  FiledPluginContext* bctx;
  bRC retval = bRC_Error;

  if (!IsCtxGood(ctx, jcr, bctx)) { goto bail_out; }

  if (all && fname) {
    Dmsg0(debuglevel,
          "fd-plugin: API error in call to SetSeenBitmap, both all and fname "
          "set!!!\n");
    goto bail_out;
  }

  if (all) {
    if (AccurateMarkAllFilesAsSeen(jcr)) { retval = bRC_OK; }
  } else if (fname) {
    if (AccurateMarkFileAsSeen(jcr, fname)) { retval = bRC_OK; }
  }

bail_out:
  return retval;
}

// Manipulate the accurate seen bitmap for clearing bits
static bRC bareosClearSeenBitmap(PluginContext* ctx, bool all, char* fname)
{
  JobControlRecord* jcr;
  FiledPluginContext* bctx;
  bRC retval = bRC_Error;

  if (!IsCtxGood(ctx, jcr, bctx)) { goto bail_out; }

  if (all && fname) {
    Dmsg0(debuglevel,
          "fd-plugin: API error in call to ClearSeenBitmap, both all and fname "
          "set!!!\n");
    goto bail_out;
  }

  if (all) {
    if (accurate_unMarkAllFilesAsSeen(jcr)) { retval = bRC_OK; }
  } else if (fname) {
    if (accurate_unMarkFileAsSeen(jcr, fname)) { retval = bRC_OK; }
  }

bail_out:
  return retval;
}
} /* namespace filedaemon */
