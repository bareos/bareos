/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2007-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2018 Bareos GmbH & Co. KG

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
#include "filed/filed.h"
#include "filed/accurate.h"
#include "filed/heartbeat.h"
#include "filed/fileset.h"
#include "filed/heartbeat.h"
#include "findlib/attribs.h"
#include "findlib/find.h"
#include "findlib/find_one.h"
#include "findlib/hardlink.h"

extern ClientResource *me;
extern DLL_IMP_EXP char *exepath;
extern DLL_IMP_EXP char *version;
extern DLL_IMP_EXP char *dist_name;

const int debuglevel = 150;
#ifdef HAVE_WIN32
const char *plugin_type = "-fd.dll";
#else
const char *plugin_type = "-fd.so";
#endif
static alist *fd_plugin_list = NULL;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

extern int save_file(JobControlRecord *jcr, FindFilesPacket *ff_pkt, bool top_level);

/**
 * Function pointers to be set here
 */
extern DLL_IMP_EXP int (*plugin_bopen)(BareosWinFilePacket *bfd, const char *fname, int flags, mode_t mode);
extern DLL_IMP_EXP int (*plugin_bclose)(BareosWinFilePacket *bfd);
extern DLL_IMP_EXP ssize_t (*plugin_bread)(BareosWinFilePacket *bfd, void *buf, size_t count);
extern DLL_IMP_EXP ssize_t (*plugin_bwrite)(BareosWinFilePacket *bfd, void *buf, size_t count);
extern DLL_IMP_EXP boffset_t (*plugin_blseek)(BareosWinFilePacket *bfd, boffset_t offset, int whence);

/**
 * Forward referenced functions
 */
static bRC bareosGetValue(bpContext *ctx, bVariable var, void *value);
static bRC bareosSetValue(bpContext *ctx, bVariable var, void *value);
static bRC bareosRegisterEvents(bpContext *ctx, int nr_events, ...);
static bRC bareosUnRegisterEvents(bpContext *ctx, int nr_events, ...);
static bRC bareosJobMsg(bpContext *ctx, const char *fname, int line,
                        int type, utime_t mtime, const char *fmt, ...);
static bRC bareosDebugMsg(bpContext *ctx, const char *fname, int line,
                          int level, const char *fmt, ...);
static void *bareosMalloc(bpContext *ctx, const char *fname, int line,
                          size_t size);
static void bareosFree(bpContext *ctx, const char *file, int line, void *mem);
static bRC bareosAddExclude(bpContext *ctx, const char *file);
static bRC bareosAddInclude(bpContext *ctx, const char *file);
static bRC bareosAddOptions(bpContext *ctx, const char *opts);
static bRC bareosAddRegex(bpContext *ctx, const char *item, int type);
static bRC bareosAddWild(bpContext *ctx, const char *item, int type);
static bRC bareosNewOptions(bpContext *ctx);
static bRC bareosNewInclude(bpContext *ctx);
static bRC bareosNewPreInclude(bpContext *ctx);
static bool is_plugin_compatible(Plugin *plugin);
static bool get_plugin_name(JobControlRecord *jcr, char *cmd, int *ret);
static bRC bareosCheckChanges(bpContext *ctx, struct save_pkt *sp);
static bRC bareosAcceptFile(bpContext *ctx, struct save_pkt *sp);
static bRC bareosSetSeenBitmap(bpContext *ctx, bool all, char *fname);
static bRC bareosClearSeenBitmap(bpContext *ctx, bool all, char *fname);
static bRC bareosGetInstanceCount(bpContext *ctx, int *ret);

/**
 * These will be plugged into the global pointer structure for the findlib.
 */
static int my_plugin_bopen(BareosWinFilePacket *bfd, const char *fname, int flags, mode_t mode);
static int my_plugin_bclose(BareosWinFilePacket *bfd);
static ssize_t my_plugin_bread(BareosWinFilePacket *bfd, void *buf, size_t count);
static ssize_t my_plugin_bwrite(BareosWinFilePacket *bfd, void *buf, size_t count);
static boffset_t my_plugin_blseek(BareosWinFilePacket *bfd, boffset_t offset, int whence);

/* Bareos info */
static bInfo binfo = {
   sizeof(bInfo),
   FD_PLUGIN_INTERFACE_VERSION
};

/* Bareos entry points */
static bFuncs bfuncs = {
   sizeof(bFuncs),
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
   bareosClearSeenBitmap
};

/**
 * Bareos private context
 */
struct b_plugin_ctx {
   JobControlRecord *jcr;                                       /* jcr for plugin */
   bRC ret;                                        /* last return code */
   bool disabled;                                  /* set if plugin disabled */
   bool restoreFileStarted;
   bool createFileCalled;
   char events[nbytes_for_bits(FD_NR_EVENTS + 1)]; /* enabled events bitmask */
   findIncludeExcludeItem *exclude;                            /* pointer to exclude files */
   findIncludeExcludeItem *include;                            /* pointer to include/exclude files */
   Plugin *plugin;                                 /* pointer to plugin of which this is an instance off */
};

static inline bool is_event_enabled(bpContext *ctx, bEventType eventType)
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

static bool is_ctx_good(bpContext *ctx, JobControlRecord *&jcr, b_plugin_ctx *&bctx)
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
 * Test if event is for this plugin
 */
static bool for_this_plugin(Plugin *plugin, char *name, int len)
{
   Dmsg4(debuglevel, "name=%s len=%d plugin=%s plen=%d\n", name, len, plugin->file, plugin->file_len);
   if (!name) {   /* if no plugin name, all plugins get it */
      return true;
   }

   /*
    * Return global VSS job metadata to all plugins
    */
   if (bstrcmp("job", name)) {  /* old V4.0 name for VSS job metadata */
      return true;
   }

   if (bstrcmp("*all*", name)) { /* new v6.0 name for VSS job metadata */
      return true;
   }

   /*
    * Check if this is the correct plugin
    */
   if (len == plugin->file_len && bstrncmp(plugin->file, name, len)) {
      return true;
   }

   return false;
}

/**
 * Raise a certain plugin event.
 */
static inline bool trigger_plugin_event(JobControlRecord *jcr, bEventType eventType,
                                        bEvent *event, bpContext *ctx,
                                        void *value, alist *plugin_ctx_list,
                                        int *index, bRC *rc)

{
   bool stop = false;

   if (!is_event_enabled(ctx, eventType)) {
      Dmsg1(debuglevel, "Event %d disabled for this plugin.\n", eventType);
      if (rc) {
         *rc = bRC_OK;
      }
      goto bail_out;
   }

   if (is_plugin_disabled(ctx)) {
      if (rc) {
         *rc = bRC_OK;
      }
      goto bail_out;
   }

   if (eventType == bEventEndRestoreJob) {
      b_plugin_ctx *b_ctx = (b_plugin_ctx *)ctx->bContext;

      Dmsg0(50, "eventType == bEventEndRestoreJob\n");
      if (b_ctx && b_ctx->restoreFileStarted) {
         plug_func(ctx->plugin)->endRestoreFile(ctx);
      }

      if (b_ctx) {
         b_ctx->restoreFileStarted = false;
         b_ctx->createFileCalled = false;
      }
   }

   /*
    * See if we should care about the return code.
    */
   if (rc) {
      *rc = plug_func(ctx->plugin)->handlePluginEvent(ctx, event, value);
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
      plug_func(ctx->plugin)->handlePluginEvent(ctx, event, value);
   }

bail_out:
   return stop;
}

/**
 * Create a plugin event When receiving bEventCancelCommand, this function is called by another thread.
 */
bRC generate_plugin_event(JobControlRecord *jcr, bEventType eventType, void *value, bool reverse)
{
   bEvent event;
   char *name = NULL;
   int i;
   int len = 0;
   bool call_if_canceled = false;
   restore_object_pkt *rop;
   bpContext *ctx;
   alist *plugin_ctx_list;
   bRC rc = bRC_OK;

   if (!fd_plugin_list) {
      Dmsg0(debuglevel, "No bplugin_list: generate_plugin_event ignored.\n");
      goto bail_out;
   }

   if (!jcr) {
      Dmsg0(debuglevel, "No jcr: generate_plugin_event ignored.\n");
      goto bail_out;
   }

   if (!jcr->plugin_ctx_list) {
      Dmsg0(debuglevel, "No plugin_ctx_list: generate_plugin_event ignored.\n");
      goto bail_out;
   }

   plugin_ctx_list = jcr->plugin_ctx_list;

   /*
    * Some events are sent to only a particular plugin or must be called even if the job is canceled.
    */
   switch(eventType) {
   case bEventPluginCommand:
   case bEventOptionPlugin:
      name = (char *)value;
      if (!get_plugin_name(jcr, name, &len)) {
         goto bail_out;
      }
      break;
   case bEventRestoreObject:
      /*
       * After all RestoreObject, we have it one more time with value = NULL
       */
      if (value) {
         /*
          * Some RestoreObjects may not have a plugin name
          */
         rop = (restore_object_pkt *)value;
         if (*rop->plugin_name) {
            name = rop->plugin_name;
            if (!get_plugin_name(jcr, name, &len)) {
               goto bail_out;
            }
         }
      }
      break;
   case bEventEndBackupJob:
   case bEventEndVerifyJob:
      call_if_canceled = true; /* plugin *must* see this call */
      break;
   case bEventStartRestoreJob:
      foreach_alist(ctx, plugin_ctx_list) {
         ((b_plugin_ctx *)ctx->bContext)->restoreFileStarted = false;
         ((b_plugin_ctx *)ctx->bContext)->createFileCalled = false;
      }
      break;
   case bEventEndRestoreJob:
      call_if_canceled = true; /* plugin *must* see this call */
      break;
   default:
      break;
   }

   /*
    * If call_if_canceled is set, we call the plugin anyway
    */
   if (!call_if_canceled && jcr->is_job_canceled()) {
      goto bail_out;
   }

   event.eventType = eventType;

   Dmsg2(debuglevel, "plugin_ctx=%p JobId=%d\n", plugin_ctx_list, jcr->JobId);

   /*
    * Pass event to every plugin that has requested this event type (except if name is set).
    * If name is set, we pass it only to the plugin with that name.
    *
    * See if we need to trigger the loaded plugins in reverse order.
    */
   if (reverse) {
      foreach_alist_rindex(i, ctx, plugin_ctx_list) {
         if (!for_this_plugin(ctx->plugin, name, len)) {
            Dmsg2(debuglevel, "Not for this plugin name=%s NULL=%d\n", name, (name == NULL) ? 1 : 0);
            continue;
         }

         if (trigger_plugin_event(jcr, eventType, &event, ctx, value, plugin_ctx_list, &i, &rc)) {
            break;
         }
      }
   } else {
      foreach_alist_index(i, ctx, plugin_ctx_list) {
         if (!for_this_plugin(ctx->plugin, name, len)) {
            Dmsg2(debuglevel, "Not for this plugin name=%s NULL=%d\n", name, (name == NULL) ? 1 : 0);
            continue;
         }

         if (trigger_plugin_event(jcr, eventType, &event, ctx, value, plugin_ctx_list, &i, &rc)) {
            break;
         }
      }
   }

   if (jcr->is_job_canceled()) {
      Dmsg0(debuglevel, "Cancel return from generate_plugin_event\n");
      rc = bRC_Cancel;
   }

bail_out:
   return rc;
}

/**
 * Check if file was seen for accurate
 */
bool plugin_check_file(JobControlRecord *jcr, char *fname)
{
   bpContext *ctx;
   alist *plugin_ctx_list;
   int retval = bRC_OK;

   if (!fd_plugin_list || !jcr || !jcr->plugin_ctx_list || jcr->is_job_canceled()) {
      return false;                      /* Return if no plugins loaded */
   }

   plugin_ctx_list = jcr->plugin_ctx_list;
   Dmsg2(debuglevel, "plugin_ctx=%p JobId=%d\n", jcr->plugin_ctx_list, jcr->JobId);

   /*
    * Pass event to every plugin
    */
   foreach_alist(ctx, plugin_ctx_list) {
      if (is_plugin_disabled(ctx)) {
         continue;
      }

      jcr->plugin_ctx = ctx;
      if (plug_func(ctx->plugin)->checkFile == NULL) {
         continue;
      }

      retval = plug_func(ctx->plugin)->checkFile(ctx, fname);
      if (retval == bRC_Seen) {
         break;
      }
   }

   return retval == bRC_Seen;
}

/**
 * Get the first part of the the plugin command
 *  systemstate:/@SYSTEMSTATE/
 * => ret = 11
 * => can use for_this_plugin(plug, cmd, ret);
 *
 * The plugin command can contain only the plugin name
 *  Plugin = alldrives
 * => ret = 9
 */
static bool get_plugin_name(JobControlRecord *jcr, char *cmd, int *ret)
{
   char *p;
   int len;

   if (!cmd || (*cmd == '\0')) {
      return false;
   }

   /*
    * Handle plugin command here backup
    */
   Dmsg1(debuglevel, "plugin cmd=%s\n", cmd);
   if ((p = strchr(cmd, ':')) == NULL) {
      if (strchr(cmd, ' ') == NULL) { /* we have just the plugin name */
         len = strlen(cmd);
      } else {
         Jmsg1(jcr, M_ERROR, 0, "Malformed plugin command: %s\n", cmd);
         return false;
      }
   } else {                     /* plugin:argument */
      len = p - cmd;
      if (len <= 0) {
         return false;
      }
   }
   *ret = len;

   return true;
}

void plugin_update_ff_pkt(FindFilesPacket *ff_pkt, struct save_pkt *sp)
{
   ff_pkt->no_read = sp->no_read;
   ff_pkt->delta_seq = sp->delta_seq;

   if (bit_is_set(FO_DELTA, sp->flags)) {
      set_bit(FO_DELTA, ff_pkt->flags);
      ff_pkt->delta_seq++;            /* make new delta sequence number */
   } else {
      clear_bit(FO_DELTA, ff_pkt->flags);
      ff_pkt->delta_seq = 0;          /* clean delta sequence number */
   }

   if (bit_is_set(FO_OFFSETS, sp->flags)) {
      set_bit(FO_OFFSETS, ff_pkt->flags);
   } else {
      clear_bit(FO_OFFSETS, ff_pkt->flags);
   }

   /*
    * Sparse code doesn't work with plugins
    * that use FIFO or STDOUT/IN to communicate
    */
   if (bit_is_set(FO_SPARSE, sp->flags)) {
      set_bit(FO_SPARSE, ff_pkt->flags);
   } else {
      clear_bit(FO_SPARSE, ff_pkt->flags);
   }

   if (bit_is_set(FO_PORTABLE_DATA, sp->flags)) {
      set_bit(FO_PORTABLE_DATA, ff_pkt->flags);
   } else {
      clear_bit(FO_PORTABLE_DATA, ff_pkt->flags);
   }

   set_bit(FO_PLUGIN, ff_pkt->flags); /* data from plugin */
}

/**
 * Ask to a Option Plugin what to do with the current file
 */
bRC plugin_option_handle_file(JobControlRecord *jcr, FindFilesPacket *ff_pkt, struct save_pkt *sp)
{
   int len;
   char *cmd;
   bRC retval = bRC_Core;
   bEvent event;
   bEventType eventType;
   bpContext *ctx;
   alist *plugin_ctx_list;

   cmd = ff_pkt->plugin;
   eventType = bEventHandleBackupFile;
   event.eventType = eventType;

   memset(sp, 0, sizeof(struct save_pkt));
   sp->pkt_size = sp->pkt_end = sizeof(struct save_pkt);
   sp->portable = true;
   copy_bits(FO_MAX, ff_pkt->flags, sp->flags);
   sp->cmd = cmd;
   sp->link = ff_pkt->link;
   sp->statp = ff_pkt->statp;
   sp->type = ff_pkt->type;
   sp->fname = ff_pkt->fname;
   sp->delta_seq = ff_pkt->delta_seq;
   sp->accurate_found = ff_pkt->accurate_found;

   plugin_ctx_list = jcr->plugin_ctx_list;
   if (!fd_plugin_list || !plugin_ctx_list || jcr->is_job_canceled()) {
      Jmsg1(jcr, M_FATAL, 0, "Command plugin \"%s\" requested, but is not loaded.\n", cmd);
      goto bail_out;         /* Return if no plugins loaded */
   }

   if (!get_plugin_name(jcr, cmd, &len)) {
      goto bail_out;
   }

   /*
    * Note, we stop the loop on the first plugin that matches the name
    */
   foreach_alist(ctx, plugin_ctx_list) {
      Dmsg4(debuglevel, "plugin=%s plen=%d cmd=%s len=%d\n", ctx->plugin->file, ctx->plugin->file_len, cmd, len);
      if (!for_this_plugin(ctx->plugin, cmd, len)) {
         continue;
      }

      if (!is_event_enabled(ctx, eventType)) {
         Dmsg1(debuglevel, "Event %d disabled for this plugin.\n", eventType);
         continue;
      }

      if (is_plugin_disabled(ctx)) {
         goto bail_out;
      }

      jcr->plugin_ctx = ctx;
      retval = plug_func(ctx->plugin)->handlePluginEvent(ctx, &event, sp);

      goto bail_out;
   } /* end foreach loop */

bail_out:
   return retval;
}

/**
 * Sequence of calls for a backup:
 * 1. plugin_save() here is called with ff_pkt
 * 2. we find the plugin requested on the command string
 * 3. we generate a bEventBackupCommand event to the specified plugin
 *    and pass it the command string.
 * 4. we make a startPluginBackup call to the plugin, which gives
 *    us the data we need in save_pkt
 * 5. we call Bareos's save_file() subroutine to save the specified
 *    file.  The plugin will be called at pluginIO() to supply the
 *    file data.
 *
 * Sequence of calls for restore:
 *   See subroutine plugin_name_stream() below.
 */
int plugin_save(JobControlRecord *jcr, FindFilesPacket *ff_pkt, bool top_level)
{
   int len;
   bRC retval;
   char *cmd;
   bEvent event;
   bpContext *ctx;
   struct save_pkt sp;
   bEventType eventType;
   PoolMem fname(PM_FNAME);
   PoolMem link(PM_FNAME);
   alist *plugin_ctx_list;
   char flags[FOPTS_BYTES];

   cmd = ff_pkt->top_fname;
   plugin_ctx_list = jcr->plugin_ctx_list;
   if (!fd_plugin_list || !plugin_ctx_list || jcr->is_job_canceled()) {
      Jmsg1(jcr, M_FATAL, 0, "Command plugin \"%s\" requested, but is not loaded.\n", cmd);
      return 1;                            /* Return if no plugins loaded */
   }

   jcr->cmd_plugin = true;
   eventType = bEventBackupCommand;
   event.eventType = eventType;

   if (!get_plugin_name(jcr, cmd, &len)) {
      goto bail_out;
   }

   /*
    * Note, we stop the loop on the first plugin that matches the name
    */
   foreach_alist(ctx, plugin_ctx_list) {
      Dmsg4(debuglevel, "plugin=%s plen=%d cmd=%s len=%d\n", ctx->plugin->file, ctx->plugin->file_len, cmd, len);
      if (!for_this_plugin(ctx->plugin, cmd, len)) {
         continue;
      }

      /*
       * We put the current plugin pointer, and the plugin context into the jcr, because during save_file(),
       * the plugin will be called many times and these values are needed.
       */
      if (!is_event_enabled(ctx, eventType)) {
         Dmsg1(debuglevel, "Event %d disabled for this plugin.\n", eventType);
         continue;
      }

      if (is_plugin_disabled(ctx)) {
         goto bail_out;
      }

      jcr->plugin_ctx = ctx;

      /*
       * Send the backup command to the right plugin
       */
      Dmsg1(debuglevel, "Command plugin = %s\n", cmd);
      if (plug_func(ctx->plugin)->handlePluginEvent(jcr->plugin_ctx, &event, cmd) != bRC_OK) {
         goto bail_out;
      }

      /*
       * Loop getting filenames to backup then saving them
       */
      while (!jcr->is_job_canceled()) {
         memset(&sp, 0, sizeof(sp));
         sp.pkt_size = sizeof(sp);
         sp.pkt_end = sizeof(sp);
         sp.portable = true;
         sp.no_read = false;
         copy_bits(FO_MAX, ff_pkt->flags, sp.flags);
         sp.cmd = cmd;
         Dmsg3(debuglevel, "startBackup st_size=%p st_blocks=%p sp=%p\n", &sp.statp.st_size, &sp.statp.st_blocks, &sp);

         /*
          * Get the file save parameters. I.e. the stat pkt ...
          */
         if (plug_func(ctx->plugin)->startBackupFile(ctx, &sp) != bRC_OK) {
            goto bail_out;
         }

         if (sp.type == 0) {
            Jmsg1(jcr, M_FATAL, 0, _("Command plugin \"%s\": no type in startBackupFile packet.\n"), cmd);
            goto bail_out;
         }

         jcr->plugin_sp = &sp;
         ff_pkt = jcr->ff;

         /*
          * Save original flags.
          */
         copy_bits(FO_MAX, ff_pkt->flags, flags);

         /*
          * Copy fname and link because save_file() zaps them.  This avoids zaping the plugin's strings.
          */
         ff_pkt->type = sp.type;
         if (IS_FT_OBJECT(sp.type)) {
            if (!sp.object_name) {
               Jmsg1(jcr, M_FATAL, 0, _("Command plugin \"%s\": no object_name in startBackupFile packet.\n"), cmd);
               goto bail_out;
            }
            ff_pkt->fname = cmd;                 /* full plugin string */
            ff_pkt->object_name = sp.object_name;
            ff_pkt->object_index = sp.index;     /* restore object index */
            ff_pkt->object_compression = 0;      /* no compression for now */
            ff_pkt->object = sp.object;
            ff_pkt->object_len = sp.object_len;
         } else {
            if (!sp.fname) {
               Jmsg1(jcr, M_FATAL, 0, _("Command plugin \"%s\": no fname in startBackupFile packet.\n"), cmd);
               goto bail_out;
            }

            pm_strcpy(fname, sp.fname);
            pm_strcpy(link, sp.link);

            ff_pkt->fname = fname.c_str();
            ff_pkt->link = link.c_str();

            plugin_update_ff_pkt(ff_pkt, &sp);
         }

         memcpy(&ff_pkt->statp, &sp.statp, sizeof(ff_pkt->statp));
         Dmsg2(debuglevel, "startBackup returned type=%d, fname=%s\n", sp.type, sp.fname);
         if (sp.object) {
            Dmsg2(debuglevel, "index=%d object=%s\n", sp.index, sp.object);
         }

         /*
          * Handle hard linked files
          *
          * Maintain a list of hard linked files already backed up. This allows us to ensure
          * that the data of each file gets backed up only once.
          */
         ff_pkt->LinkFI = 0;
         if (!bit_is_set(FO_NO_HARDLINK, ff_pkt->flags) && ff_pkt->statp.st_nlink > 1) {
            CurLink *hl;

            switch (ff_pkt->statp.st_mode & S_IFMT) {
            case S_IFREG:
            case S_IFCHR:
            case S_IFBLK:
            case S_IFIFO:
#ifdef S_IFSOCK
            case S_IFSOCK:
#endif
               hl = lookup_hardlink(jcr, ff_pkt, ff_pkt->statp.st_ino, ff_pkt->statp.st_dev);
               if (hl) {
                  /*
                   * If we have already backed up the hard linked file don't do it again
                   */
                  if (bstrcmp(hl->name, sp.fname)) {
                     Dmsg2(400, "== Name identical skip FI=%d file=%s\n", hl->FileIndex, fname.c_str());
                     ff_pkt->no_read = true;
                  } else {
                     ff_pkt->link = hl->name;
                     ff_pkt->type = FT_LNKSAVED; /* Handle link, file already saved */
                     ff_pkt->LinkFI = hl->FileIndex;
                     ff_pkt->linked = NULL;
                     ff_pkt->digest = hl->digest;
                     ff_pkt->digest_stream = hl->digest_stream;
                     ff_pkt->digest_len = hl->digest_len;

                     Dmsg3(400, "FT_LNKSAVED FI=%d LinkFI=%d file=%s\n", ff_pkt->FileIndex, hl->FileIndex, hl->name);

                     ff_pkt->no_read = true;
                  }
               } else {
                  /*
                   * File not previously dumped. Chain it into our list.
                   */
                  hl = new_hardlink(jcr, ff_pkt, sp.fname, ff_pkt->statp.st_ino, ff_pkt->statp.st_dev);
                  ff_pkt->linked = hl;              /* Mark saved link */
                  Dmsg2(400, "Added to hash FI=%d file=%s\n", ff_pkt->FileIndex, hl->name);
               }
               break;
            default:
               ff_pkt->linked = NULL;
               break;
            }
         } else {
            ff_pkt->linked = NULL;
         }

         /*
          * Call Bareos core code to backup the plugin's file
          */
         save_file(jcr, ff_pkt, true);

         /*
          * Restore original flags.
          */
         copy_bits(FO_MAX, flags, ff_pkt->flags);

         retval = plug_func(ctx->plugin)->endBackupFile(ctx);
         if (retval == bRC_More || retval == bRC_OK) {
            accurate_mark_file_as_seen(jcr, fname.c_str());
         }

         if (retval == bRC_More) {
            continue;
         }

         goto bail_out;
      } /* end while loop */

      goto bail_out;
   } /* end loop over all plugins */
   Jmsg1(jcr, M_FATAL, 0, "Command plugin \"%s\" not found.\n", cmd);

bail_out:
   jcr->cmd_plugin = false;

   return 1;
}

/**
 * Sequence of calls for a estimate:
 * 1. plugin_estimate() here is called with ff_pkt
 * 2. we find the plugin requested on the command string
 * 3. we generate a bEventEstimateCommand event to the specified plugin
 *    and pass it the command string.
 * 4. we make a startPluginBackup call to the plugin, which gives
 *    us the data we need in save_pkt
 *
 */
int plugin_estimate(JobControlRecord *jcr, FindFilesPacket *ff_pkt, bool top_level)
{
   int len;
   char *cmd = ff_pkt->top_fname;
   struct save_pkt sp;
   bEvent event;
   bEventType eventType;
   PoolMem fname(PM_FNAME);
   PoolMem link(PM_FNAME);
   bpContext *ctx;
   alist *plugin_ctx_list;
   Attributes attr;

   plugin_ctx_list = jcr->plugin_ctx_list;
   if (!fd_plugin_list || !plugin_ctx_list) {
      Jmsg1(jcr, M_FATAL, 0, "Command plugin \"%s\" requested, but is not loaded.\n", cmd);
      return 1;                            /* Return if no plugins loaded */
   }

   jcr->cmd_plugin = true;
   eventType = bEventEstimateCommand;
   event.eventType = eventType;

   if (!get_plugin_name(jcr, cmd, &len)) {
      goto bail_out;
   }

   /*
    * Note, we stop the loop on the first plugin that matches the name
    */
   foreach_alist(ctx, plugin_ctx_list) {
      Dmsg4(debuglevel, "plugin=%s plen=%d cmd=%s len=%d\n", ctx->plugin->file, ctx->plugin->file_len, cmd, len);
      if (!for_this_plugin(ctx->plugin, cmd, len)) {
         continue;
      }

      /*
       * We put the current plugin pointer, and the plugin context into the jcr, because during save_file(),
       * the plugin will be called many times and these values are needed.
       */
      if (!is_event_enabled(ctx, eventType)) {
         Dmsg1(debuglevel, "Event %d disabled for this plugin.\n", eventType);
         continue;
      }

      if (is_plugin_disabled(ctx)) {
         goto bail_out;
      }

      jcr->plugin_ctx = ctx;

      /*
       * Send the backup command to the right plugin
       */
      Dmsg1(debuglevel, "Command plugin = %s\n", cmd);
      if (plug_func(ctx->plugin)->handlePluginEvent(ctx, &event, cmd) != bRC_OK) {
         goto bail_out;
      }

      /*
       * Loop getting filenames to backup then saving them
       */
      while (!jcr->is_job_canceled()) {
         memset(&sp, 0, sizeof(sp));
         sp.pkt_size = sizeof(sp);
         sp.pkt_end = sizeof(sp);
         sp.portable = true;
         copy_bits(FO_MAX, ff_pkt->flags, sp.flags);
         sp.cmd = cmd;
         Dmsg3(debuglevel, "startBackup st_size=%p st_blocks=%p sp=%p\n",
               &sp.statp.st_size, &sp.statp.st_blocks, &sp);

         /*
          * Get the file save parameters. I.e. the stat pkt ...
          */
         if (plug_func(ctx->plugin)->startBackupFile(ctx, &sp) != bRC_OK) {
            goto bail_out;
         }

         if (sp.type == 0) {
            Jmsg1(jcr, M_FATAL, 0, _("Command plugin \"%s\": no type in startBackupFile packet.\n"), cmd);
            goto bail_out;
         }

         if (!IS_FT_OBJECT(sp.type)) {
            if (!sp.fname) {
               Jmsg1(jcr, M_FATAL, 0, _("Command plugin \"%s\": no fname in startBackupFile packet.\n"),
                  cmd);
               goto bail_out;
            }

            /*
             * Count only files backed up
             */
            switch (sp.type) {
            case FT_REGE:
            case FT_REG:
            case FT_LNK:
            case FT_DIREND:
            case FT_SPEC:
            case FT_RAW:
            case FT_FIFO:
            case FT_LNKSAVED:
               jcr->JobFiles++;        /* increment number of files backed up */
               break;
            default:
               break;
            }
            jcr->num_files_examined++;

            if (sp.type != FT_LNKSAVED && S_ISREG(sp.statp.st_mode)) {
               if (sp.statp.st_size > 0) {
                  jcr->JobBytes += sp.statp.st_size;
               }
            }

            if (jcr->listing) {
               memcpy(&attr.statp, &sp.statp, sizeof(struct stat));
               attr.type = sp.type;
               attr.ofname = (POOLMEM *)sp.fname;
               attr.olname = (POOLMEM *)sp.link;
               print_ls_output(jcr, &attr);
            }
         }

         Dmsg2(debuglevel, "startBackup returned type=%d, fname=%s\n", sp.type, sp.fname);
         if (sp.object) {
            Dmsg2(debuglevel, "index=%d object=%s\n", sp.index, sp.object);
         }

         bRC retval = plug_func(ctx->plugin)->endBackupFile(ctx);
         if (retval == bRC_More || retval == bRC_OK) {
            accurate_mark_file_as_seen(jcr, sp.fname);
         }

         if (retval == bRC_More) {
            continue;
         }

         goto bail_out;
      } /* end while loop */

      goto bail_out;
   } /* end loop over all plugins */
   Jmsg1(jcr, M_FATAL, 0, "Command plugin \"%s\" not found.\n", cmd);

bail_out:
   jcr->cmd_plugin = false;

   return 1;
}

/**
 * Send plugin name start/end record to SD
 */
bool send_plugin_name(JobControlRecord *jcr, BareosSocket *sd, bool start)
{
   int status;
   int index = jcr->JobFiles;
   struct save_pkt *sp = (struct save_pkt *)jcr->plugin_sp;

   if (!sp) {
      Jmsg0(jcr, M_FATAL, 0, _("Plugin save packet not found.\n"));
      return false;
   }
   if (jcr->is_job_canceled()) {
      return false;
   }

   if (start) {
      index++;                  /* JobFiles not incremented yet */
   }
   Dmsg1(debuglevel, "send_plugin_name=%s\n", sp->cmd);

   /*
    * Send stream header
    */
   if (!sd->fsend("%ld %d 0", index, STREAM_PLUGIN_NAME)) {
     Jmsg1(jcr, M_FATAL, 0, _("Network send error to SD. ERR=%s\n"), sd->bstrerror());
     return false;
   }
   Dmsg1(debuglevel, "send plugin name hdr: %s\n", sd->msg);

   if (start) {
      /*
       * Send data -- not much
       */
      status = sd->fsend("%ld 1 %d %s%c", index, sp->portable, sp->cmd, 0);
   } else {
      /*
       * Send end of data
       */
      status = sd->fsend("%ld 0", jcr->JobFiles);
   }
   if (!status) {
      Jmsg1(jcr, M_FATAL, 0, _("Network send error to SD. ERR=%s\n"), sd->bstrerror());
         return false;
   }
   Dmsg1(debuglevel, "send plugin start/end: %s\n", sd->msg);
   sd->signal(BNET_EOD);            /* indicate end of plugin name data */

   return true;
}

/**
 * Plugin name stream found during restore.  The record passed in argument name was
 * generated in send_plugin_name() above.
 *
 * Returns: true  if start of stream
 *          false if end of steam
 */
bool plugin_name_stream(JobControlRecord *jcr, char *name)
{
   int len;
   char *cmd;
   char *p = name;
   bool start;
   bpContext *ctx;
   alist *plugin_ctx_list;

   Dmsg1(debuglevel, "Read plugin stream string=%s\n", name);
   skip_nonspaces(&p);             /* skip over jcr->JobFiles */
   skip_spaces(&p);
   start = *p == '1';
   if (start) {
      /*
       * Start of plugin data
       */
      skip_nonspaces(&p);          /* skip start/end flag */
      skip_spaces(&p);
      skip_nonspaces(&p);          /* skip portable flag */
      skip_spaces(&p);
      cmd = p;
   } else {
      /*
       * End of plugin data, notify plugin, then clear flags
       */
      if (jcr->plugin_ctx) {
         Plugin *plugin = jcr->plugin_ctx->plugin;
         b_plugin_ctx *b_ctx = (b_plugin_ctx *)jcr->plugin_ctx->bContext;

         Dmsg2(debuglevel, "End plugin data plugin=%p ctx=%p\n", plugin, jcr->plugin_ctx);
         if (b_ctx->restoreFileStarted) {
            plug_func(plugin)->endRestoreFile(jcr->plugin_ctx);
         }
         b_ctx->restoreFileStarted = false;
         b_ctx->createFileCalled = false;
      }

      goto bail_out;
   }

   plugin_ctx_list = jcr->plugin_ctx_list;
   if (!plugin_ctx_list) {
      goto bail_out;
   }

   /*
    * After this point, we are dealing with a restore start
    */
   if (!get_plugin_name(jcr, cmd, &len)) {
      goto bail_out;
   }

   /*
    * Search for correct plugin as specified on the command
    */
   foreach_alist(ctx, plugin_ctx_list) {
      bEvent event;
      bEventType eventType;
      b_plugin_ctx *b_ctx;

      Dmsg3(debuglevel, "plugin=%s cmd=%s len=%d\n", ctx->plugin->file, cmd, len);
      if (!for_this_plugin(ctx->plugin, cmd, len)) {
         continue;
      }

      if (is_plugin_disabled(ctx)) {
         Dmsg1(debuglevel, "Plugin %s disabled\n", cmd);
         goto bail_out;
      }

      Dmsg1(debuglevel, "Restore Command plugin = %s\n", cmd);
      eventType = bEventRestoreCommand;
      event.eventType = eventType;

      if (!is_event_enabled(ctx, eventType)) {
         Dmsg1(debuglevel, "Event %d disabled for this plugin.\n", eventType);
         continue;
      }

      jcr->plugin_ctx = ctx;
      jcr->cmd_plugin = true;
      b_ctx = (b_plugin_ctx *)ctx->bContext;

      if (plug_func(ctx->plugin)->handlePluginEvent(jcr->plugin_ctx, &event, cmd) != bRC_OK) {
         Dmsg1(debuglevel, "Handle event failed. Plugin=%s\n", cmd);
         goto bail_out;
      }

      if (b_ctx->restoreFileStarted) {
         Jmsg2(jcr, M_FATAL, 0, "Second call to startRestoreFile. plugin=%s cmd=%s\n", ctx->plugin->file, cmd);
         b_ctx->restoreFileStarted = false;
         goto bail_out;
      }

      if (plug_func(ctx->plugin)->startRestoreFile(jcr->plugin_ctx, cmd) == bRC_OK) {
         b_ctx->restoreFileStarted = true;
         goto bail_out;
      } else {
         Dmsg1(debuglevel, "startRestoreFile failed. plugin=%s\n", cmd);
         goto bail_out;
      }
   }
   Jmsg1(jcr, M_WARNING, 0, _("Plugin=%s not found.\n"), cmd);

bail_out:
   return start;
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
int plugin_create_file(JobControlRecord *jcr, Attributes *attr, BareosWinFilePacket *bfd, int replace)
{
   int flags;
   int retval;
   int status;
   Plugin *plugin;
   struct restore_pkt rp;
   bpContext *ctx = jcr->plugin_ctx;
   b_plugin_ctx *b_ctx = (b_plugin_ctx *)jcr->plugin_ctx->bContext;

   if (!ctx || !set_cmd_plugin(bfd, jcr) || jcr->is_job_canceled()) {
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
   rp.replace = jcr->replace;
   rp.create_status = CF_ERROR;

   Dmsg4(debuglevel, "call plugin createFile stream=%d type=%d LinkFI=%d File=%s\n",
         rp.stream, rp.type, rp.LinkFI, rp.ofname);
   if (rp.attrEx) {
      Dmsg1(debuglevel, "attrEx=\"%s\"\n", rp.attrEx);
   }

   if (!b_ctx->restoreFileStarted || b_ctx->createFileCalled) {
      Jmsg2(jcr, M_FATAL, 0, "Unbalanced call to createFile=%d %d\n",
            b_ctx->createFileCalled, b_ctx->restoreFileStarted);
      b_ctx->createFileCalled = false;
      return CF_ERROR;
   }

   retval = plug_func(plugin)->createFile(ctx, &rp);
   if (retval != bRC_OK) {
      Qmsg2(jcr, M_ERROR, 0, _("Plugin createFile call failed. Stat=%d file=%s\n"), retval, attr->ofname);
      return CF_ERROR;
   }

   switch (rp.create_status) {
   case CF_ERROR:
      Qmsg1(jcr, M_ERROR, 0, _("Plugin createFile call failed. Returned CF_ERROR file=%s\n"), attr->ofname);
      /*
       * FALLTHROUGH
       */
   case CF_SKIP:
      /*
       * FALLTHROUGH
       */
   case CF_CORE:
      /*
       * FALLTHROUGH
       */
   case CF_CREATED:
      return rp.create_status;
   }

   flags =  O_WRONLY | O_CREAT | O_TRUNC | O_BINARY;
   Dmsg0(debuglevel, "call bopen\n");

   status = bopen(bfd, attr->ofname, flags, S_IRUSR | S_IWUSR, attr->statp.st_rdev);
   Dmsg1(debuglevel, "bopen status=%d\n", status);

   if (status < 0) {
      berrno be;

      be.set_errno(bfd->berrno);
      Qmsg2(jcr, M_ERROR, 0, _("Could not create %s: ERR=%s\n"),
            attr->ofname, be.bstrerror());
      Dmsg2(debuglevel,"Could not bopen file %s: ERR=%s\n", attr->ofname, be.bstrerror());
      return CF_ERROR;
   }

   if (!is_bopen(bfd)) {
      Dmsg0(000, "===== BFD is not open!!!!\n");
   }

   return CF_EXTRACT;
}

/**
 * Reset the file attributes after all file I/O is done -- this allows the previous access time/dates
 * to be set properly, and it also allows us to properly set directory permissions.
 */
bool plugin_set_attributes(JobControlRecord *jcr, Attributes *attr, BareosWinFilePacket *ofd)
{
   Plugin *plugin;
   struct restore_pkt rp;

   Dmsg0(debuglevel, "plugin_set_attributes\n");

   if (!jcr->plugin_ctx) {
      return false;
   }
   plugin = (Plugin *)jcr->plugin_ctx->plugin;

   memset(&rp, 0, sizeof(rp));
   rp.pkt_size = sizeof(rp);
   rp.pkt_end = sizeof(rp);
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
   rp.replace = jcr->replace;
   rp.create_status = CF_ERROR;

   plug_func(plugin)->setFileAttributes(jcr->plugin_ctx, &rp);

   if (rp.create_status == CF_CORE) {
      set_attributes(jcr, attr, ofd);
   } else {
      if (is_bopen(ofd)) {
         bclose(ofd);
      }
      pm_strcpy(attr->ofname, "*None*");
   }

   return true;
}

/**
 * Plugin specific callback for getting ACL information.
 */
bacl_exit_code plugin_build_acl_streams(JobControlRecord *jcr,
                                        acl_data_t *acl_data,
                                        FindFilesPacket *ff_pkt)
{
   Plugin *plugin;

   Dmsg0(debuglevel, "plugin_build_acl_streams\n");

   if (!jcr->plugin_ctx) {
      return bacl_exit_ok;
   }
   plugin = (Plugin *)jcr->plugin_ctx->plugin;

   if (plug_func(plugin)->getAcl == NULL) {
      return bacl_exit_ok;
   } else {
      bacl_exit_code retval = bacl_exit_error;
#if defined(HAVE_ACL)
      struct acl_pkt ap;

      memset(&ap, 0, sizeof(ap));
      ap.pkt_size = ap.pkt_end = sizeof(struct acl_pkt);
      ap.fname = acl_data->last_fname;

      switch (plug_func(plugin)->getAcl(jcr->plugin_ctx, &ap)) {
      case bRC_OK:
         if (ap.content_length && ap.content) {
            acl_data->u.build->content = check_pool_memory_size(acl_data->u.build->content, ap.content_length);
            memcpy(acl_data->u.build->content, ap.content, ap.content_length);
            acl_data->u.build->content_length = ap.content_length;
            free(ap.content);
            retval = send_acl_stream(jcr, acl_data, STREAM_ACL_PLUGIN);
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

/**
 * Plugin specific callback for setting ACL information.
 */
bacl_exit_code plugin_parse_acl_streams(JobControlRecord *jcr,
                                        acl_data_t *acl_data,
                                        int stream,
                                        char *content,
                                        uint32_t content_length)
{
   Plugin *plugin;

   Dmsg0(debuglevel, "plugin_parse_acl_streams\n");

   if (!jcr->plugin_ctx) {
      return bacl_exit_ok;
   }
   plugin = (Plugin *)jcr->plugin_ctx->plugin;

   if (plug_func(plugin)->setAcl == NULL) {
      return bacl_exit_error;
   } else {
      bacl_exit_code retval = bacl_exit_error;
#if defined(HAVE_ACL)
      struct acl_pkt ap;

      memset(&ap, 0, sizeof(ap));
      ap.pkt_size = ap.pkt_end = sizeof(struct acl_pkt);
      ap.fname = acl_data->last_fname;
      ap.content = content;
      ap.content_length = content_length;

      switch (plug_func(plugin)->setAcl(jcr->plugin_ctx, &ap)) {
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

/**
 * Plugin specific callback for getting XATTR information.
 */
bxattr_exit_code plugin_build_xattr_streams(JobControlRecord *jcr,
                                            struct xattr_data_t *xattr_data,
                                            FindFilesPacket *ff_pkt)
{
   Plugin *plugin;
#if defined(HAVE_XATTR)
   alist *xattr_value_list = NULL;
#endif
   bxattr_exit_code retval = bxattr_exit_error;

   Dmsg0(debuglevel, "plugin_build_xattr_streams\n");

   if (!jcr->plugin_ctx) {
      return bxattr_exit_ok;
   }
   plugin = (Plugin *)jcr->plugin_ctx->plugin;

   if (plug_func(plugin)->getXattr == NULL) {
      return bxattr_exit_ok;
   } else {
#if defined(HAVE_XATTR)
      bool more;
      int xattr_count = 0;
      xattr_t *current_xattr;
      struct xattr_pkt xp;
      uint32_t expected_serialize_len = 0;

      while (1) {
         memset(&xp, 0, sizeof(xp));
         xp.pkt_size = xp.pkt_end = sizeof(struct xattr_pkt);
         xp.fname = xattr_data->last_fname;

         switch (plug_func(plugin)->getXattr(jcr->plugin_ctx, &xp)) {
         case bRC_OK:
            more = false;
            break;
         case bRC_More:
            more = true;
            break;
         default:
            goto bail_out;
         }

         /*
          * Make sure the plugin filled a XATTR name.
          * The name and value returned by the plugin need to be in allocated memory
          * and are freed by xattr_drop_internal_table() function when we are done
          * processing the data.
          */
         if (xp.name_length && xp.name) {
            /*
             * Each xattr valuepair starts with a magic so we can parse it easier.
             */
            current_xattr = (xattr_t *)malloc(sizeof(xattr_t));
            current_xattr->magic = XATTR_MAGIC;
            expected_serialize_len += sizeof(current_xattr->magic);

            current_xattr->name_length = xp.name_length;
            current_xattr->name = xp.name;
            expected_serialize_len += sizeof(current_xattr->name_length) + current_xattr->name_length;

            current_xattr->value_length = xp.value_length;
            current_xattr->value = xp.value;
            expected_serialize_len += sizeof(current_xattr->value_length) + current_xattr->value_length;

            if (xattr_value_list == NULL) {
               xattr_value_list = New(alist(10, not_owned_by_alist));
            }

            xattr_value_list->append(current_xattr);
            xattr_count++;

            /*
             * Protect ourself against things getting out of hand.
             */
            if (expected_serialize_len >= MAX_XATTR_STREAM) {
               Mmsg2(jcr->errmsg,
                     _("Xattr stream on file \"%s\" exceeds maximum size of %d bytes\n"),
                     xattr_data->last_fname, MAX_XATTR_STREAM);
               goto bail_out;
            }
         }

         /*
          * Does the plugin have more xattrs ?
          */
         if (!more) {
            break;
         }
      }

      /*
       * If we found any xattr send them to the SD.
       */
      if (xattr_count > 0) {
         /*
          * Serialize the datastream.
          */
         if (serialize_xattr_stream(jcr,
                                    xattr_data,
                                    expected_serialize_len,
                                    xattr_value_list) < expected_serialize_len) {
            Mmsg1(jcr->errmsg,
                  _("Failed to serialize extended attributes on file \"%s\"\n"),
                  xattr_data->last_fname);
            Dmsg1(100, "Failed to serialize extended attributes on file \"%s\"\n",
                  xattr_data->last_fname);
            goto bail_out;
         }

         /*
          * Send the datastream to the SD.
          */
         retval = send_xattr_stream(jcr, xattr_data, STREAM_XATTR_PLUGIN);
      } else {
         retval = bxattr_exit_ok;
      }
#endif
   }

#if defined(HAVE_XATTR)
bail_out:
   if (xattr_value_list) {
      xattr_drop_internal_table(xattr_value_list);
   }
#endif

   return retval;
}

/**
 * Plugin specific callback for setting XATTR information.
 */
bxattr_exit_code plugin_parse_xattr_streams(JobControlRecord *jcr,
                                            struct xattr_data_t *xattr_data,
                                            int stream,
                                            char *content,
                                            uint32_t content_length)
{
#if defined(HAVE_XATTR)
   Plugin *plugin;
   alist *xattr_value_list = NULL;
#endif
   bxattr_exit_code retval = bxattr_exit_error;

   Dmsg0(debuglevel, "plugin_parse_xattr_streams\n");

   if (!jcr->plugin_ctx) {
      return bxattr_exit_ok;
   }

#if defined(HAVE_XATTR)
   plugin = (Plugin *)jcr->plugin_ctx->plugin;
   if (plug_func(plugin)->setXattr != NULL) {
      xattr_t *current_xattr;
      struct xattr_pkt xp;

      xattr_value_list = New(alist(10, not_owned_by_alist));

      if (unserialize_xattr_stream(jcr,
                                   xattr_data,
                                   content,
                                   content_length,
                                   xattr_value_list) != bxattr_exit_ok) {
         goto bail_out;
      }

      memset(&xp, 0, sizeof(xp));
      xp.pkt_size = xp.pkt_end = sizeof(struct xattr_pkt);
      xp.fname = xattr_data->last_fname;

      foreach_alist(current_xattr, xattr_value_list) {
         xp.name = current_xattr->name;
         xp.name_length = current_xattr->name_length;
         xp.value = current_xattr->value;
         xp.value_length = current_xattr->value_length;

         switch (plug_func(plugin)->setXattr(jcr->plugin_ctx, &xp)) {
         case bRC_OK:
            break;
         default:
            goto bail_out;
         }
      }

      retval = bxattr_exit_ok;
   }

bail_out:
   if (xattr_value_list) {
      xattr_drop_internal_table(xattr_value_list);
   }
#endif

   return retval;
}

/**
 * Print to file the plugin info.
 */
static void dump_fd_plugin(Plugin *plugin, FILE *fp)
{
   genpInfo *info;

   if (!plugin) {
      return ;
   }

   info = (genpInfo *)plugin->pinfo;
   fprintf(fp, "\tversion=%d\n", info->version);
   fprintf(fp, "\tdate=%s\n", NPRTB(info->plugin_date));
   fprintf(fp, "\tmagic=%s\n", NPRTB(info->plugin_magic));
   fprintf(fp, "\tauthor=%s\n", NPRTB(info->plugin_author));
   fprintf(fp, "\tlicence=%s\n", NPRTB(info->plugin_license));
   fprintf(fp, "\tversion=%s\n", NPRTB(info->plugin_version));
   fprintf(fp, "\tdescription=%s\n", NPRTB(info->plugin_description));
}

static void dump_fd_plugins(FILE *fp)
{
   dump_plugins(fd_plugin_list, fp);
}

/**
 * This entry point is called internally by Bareos to ensure that the plugin IO calls come into this code.
 */
void load_fd_plugins(const char *plugin_dir, alist *plugin_names)
{
   Plugin *plugin;
   int i;

   if (!plugin_dir) {
      Dmsg0(debuglevel, "plugin dir is NULL\n");
      return;
   }

   fd_plugin_list = New(alist(10, not_owned_by_alist));
   if (!load_plugins((void *)&binfo, (void *)&bfuncs, fd_plugin_list,
                     plugin_dir, plugin_names, plugin_type, is_plugin_compatible)) {
      /*
       * Either none found, or some error
       */
      if (fd_plugin_list->size() == 0) {
         delete fd_plugin_list;
         fd_plugin_list = NULL;
         Dmsg0(debuglevel, "No plugins loaded\n");
         return;
      }
   }

   /*
    * Plug entry points called from findlib
    */
   plugin_bopen  = my_plugin_bopen;
   plugin_bclose = my_plugin_bclose;
   plugin_bread  = my_plugin_bread;
   plugin_bwrite = my_plugin_bwrite;
   plugin_blseek = my_plugin_blseek;

   /*
    * Verify that the plugin is acceptable, and print information about it.
    */
   foreach_alist_index(i, plugin, fd_plugin_list) {
      Dmsg1(debuglevel, "Loaded plugin: %s\n", plugin->file);
   }

   dbg_plugin_add_hook(dump_fd_plugin);
   dbg_print_plugin_add_hook(dump_fd_plugins);
}

void unload_fd_plugins(void)
{
   unload_plugins(fd_plugin_list);
   delete fd_plugin_list;
   fd_plugin_list = NULL;
}

int list_fd_plugins(PoolMem &msg)
{
   return list_plugins(fd_plugin_list, msg);
}

/**
 * Check if a plugin is compatible.  Called by the load_plugin function to allow us to verify the plugin.
 */
static bool is_plugin_compatible(Plugin *plugin)
{
   genpInfo *info = (genpInfo *)plugin->pinfo;
   Dmsg0(debuglevel, "is_plugin_compatible called\n");
   if (debug_level >= 50) {
      dump_fd_plugin(plugin, stdin);
   }
   if (!bstrcmp(info->plugin_magic, FD_PLUGIN_MAGIC)) {
      Jmsg(NULL, M_ERROR, 0, _("Plugin magic wrong. Plugin=%s wanted=%s got=%s\n"),
           plugin->file, FD_PLUGIN_MAGIC, info->plugin_magic);
      Dmsg3(50, "Plugin magic wrong. Plugin=%s wanted=%s got=%s\n",
           plugin->file, FD_PLUGIN_MAGIC, info->plugin_magic);

      return false;
   }
   if (info->version != FD_PLUGIN_INTERFACE_VERSION) {
      Jmsg(NULL, M_ERROR, 0, _("Plugin version incorrect. Plugin=%s wanted=%d got=%d\n"),
           plugin->file, FD_PLUGIN_INTERFACE_VERSION, info->version);
      Dmsg3(50, "Plugin version incorrect. Plugin=%s wanted=%d got=%d\n",
           plugin->file, FD_PLUGIN_INTERFACE_VERSION, info->version);
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

/**
 * Instantiate a new plugin instance.
 */
static inline bpContext *instantiate_plugin(JobControlRecord *jcr, Plugin *plugin, char instance)
{
   bpContext *ctx;
   b_plugin_ctx *b_ctx;

   b_ctx = (b_plugin_ctx *)malloc(sizeof(b_plugin_ctx));
   b_ctx = (b_plugin_ctx *)memset(b_ctx, 0, sizeof(b_plugin_ctx));
   b_ctx->jcr = jcr;
   b_ctx->plugin = plugin;

   ctx = (bpContext *)malloc(sizeof(bpContext));
   ctx->instance = instance;
   ctx->plugin = plugin;
   ctx->bContext = (void *)b_ctx;
   ctx->pContext = NULL;

   jcr->plugin_ctx_list->append(ctx);

   if (plug_func(plugin)->newPlugin(ctx) != bRC_OK) {
      b_ctx->disabled = true;
   }

   return ctx;
}

/**
 * Create a new instance of each plugin for this Job
 *
 * Note, fd_plugin_list can exist but jcr->plugin_ctx_list can be NULL if no plugins were loaded.
 */
void new_plugins(JobControlRecord *jcr)
{
   int i, num;
   Plugin *plugin;

   if (!fd_plugin_list) {
      Dmsg0(debuglevel, "plugin list is NULL\n");
      return;
   }

   if (jcr->is_job_canceled()) {
      return;
   }

   num = fd_plugin_list->size();
   if (num == 0) {
      Dmsg0(debuglevel, "No plugins loaded\n");
      return;
   }

   jcr->plugin_ctx_list = New(alist(10, owned_by_alist));
   Dmsg2(debuglevel, "Instantiate plugin_ctx=%p JobId=%d\n", jcr->plugin_ctx_list, jcr->JobId);
   foreach_alist_index(i, plugin, fd_plugin_list) {
      /*
       * Start a new instance of each plugin
       */
      instantiate_plugin(jcr, plugin, 0);
   }
}

/**
 * Free the plugin instances for this Job
 */
void free_plugins(JobControlRecord *jcr)
{
   bpContext *ctx;

   if (!fd_plugin_list || !jcr->plugin_ctx_list) {
      return;
   }

   Dmsg2(debuglevel, "Free instance fd-plugin_ctx_list=%p JobId=%d\n", jcr->plugin_ctx_list, jcr->JobId);
   foreach_alist(ctx, jcr->plugin_ctx_list) {
      /*
       * Free the plugin instance
       */
      plug_func(ctx->plugin)->freePlugin(ctx);
      free(ctx->bContext);                   /* Free BAREOS private context */
   }

   delete jcr->plugin_ctx_list;
   jcr->plugin_ctx_list = NULL;
}

/**
 * Entry point for opening the file this is a wrapper around the pluginIO entry point in the plugin.
 */
static int my_plugin_bopen(BareosWinFilePacket *bfd, const char *fname, int flags, mode_t mode)
{
   Plugin *plugin;
   struct io_pkt io;
   JobControlRecord *jcr = bfd->jcr;

   Dmsg1(debuglevel, "plugin_bopen flags=%08o\n", flags);
   if (!jcr->plugin_ctx) {
      return 0;
   }
   plugin = (Plugin *)jcr->plugin_ctx->plugin;

   memset(&io, 0, sizeof(io));
   io.pkt_size = sizeof(io);
   io.pkt_end = sizeof(io);

   io.func = IO_OPEN;
   io.fname = fname;
   io.flags = flags;
   io.mode = mode;

   plug_func(plugin)->pluginIO(jcr->plugin_ctx, &io);
   bfd->berrno = io.io_errno;

   if (io.win32) {
      errno = b_errno_win32;
   } else {
      errno = io.io_errno;
      bfd->lerror = io.lerror;
   }

   Dmsg1(debuglevel, "Return from plugin open status=%d\n", io.status);

   return io.status;
}

/**
 * Entry point for closing the file this is a wrapper around the pluginIO entry point in the plugin.
 */
static int my_plugin_bclose(BareosWinFilePacket *bfd)
{
   Plugin *plugin;
   struct io_pkt io;
   JobControlRecord *jcr = bfd->jcr;

   Dmsg0(debuglevel, "===== plugin_bclose\n");
   if (!jcr->plugin_ctx) {
      return 0;
   }
   plugin = (Plugin *)jcr->plugin_ctx->plugin;

   memset(&io, 0, sizeof(io));
   io.pkt_size = sizeof(io);
   io.pkt_end = sizeof(io);

   io.func = IO_CLOSE;

   plug_func(plugin)->pluginIO(jcr->plugin_ctx, &io);
   bfd->berrno = io.io_errno;

   if (io.win32) {
      errno = b_errno_win32;
   } else {
      errno = io.io_errno;
      bfd->lerror = io.lerror;
   }

   Dmsg1(debuglevel, "plugin_bclose stat=%d\n", io.status);

   return io.status;
}

/**
 * Entry point for reading from the file this is a wrapper around the pluginIO entry point in the plugin.
 */
static ssize_t my_plugin_bread(BareosWinFilePacket *bfd, void *buf, size_t count)
{
   Plugin *plugin;
   struct io_pkt io;
   JobControlRecord *jcr = bfd->jcr;

   Dmsg0(debuglevel, "plugin_bread\n");
   if (!jcr->plugin_ctx) {
      return 0;
   }
   plugin = (Plugin *)jcr->plugin_ctx->plugin;

   memset(&io, 0, sizeof(io));
   io.pkt_size = sizeof(io);
   io.pkt_end = sizeof(io);

   io.func = IO_READ;
   io.count = count;
   io.buf = (char *)buf;

   plug_func(plugin)->pluginIO(jcr->plugin_ctx, &io);
   bfd->offset = io.offset;
   bfd->berrno = io.io_errno;

   if (io.win32) {
      errno = b_errno_win32;
   } else {
      errno = io.io_errno;
      bfd->lerror = io.lerror;
   }

   return (ssize_t)io.status;
}

/**
 * Entry point for writing to the file this is a wrapper around the pluginIO entry point in the plugin.
 */
static ssize_t my_plugin_bwrite(BareosWinFilePacket *bfd, void *buf, size_t count)
{
   Plugin *plugin;
   struct io_pkt io;
   JobControlRecord *jcr = bfd->jcr;

   Dmsg0(debuglevel, "plugin_bwrite\n");
   if (!jcr->plugin_ctx) {
      Dmsg0(0, "No plugin context\n");
      return 0;
   }
   plugin = (Plugin *)jcr->plugin_ctx->plugin;

   memset(&io, 0, sizeof(io));
   io.pkt_size = sizeof(io);
   io.pkt_end = sizeof(io);

   io.func = IO_WRITE;
   io.count = count;
   io.buf = (char *)buf;

   plug_func(plugin)->pluginIO(jcr->plugin_ctx, &io);
   bfd->berrno = io.io_errno;

   if (io.win32) {
      errno = b_errno_win32;
   } else {
      errno = io.io_errno;
      bfd->lerror = io.lerror;
   }

   return (ssize_t)io.status;
}

/**
 * Entry point for seeking in the file this is a wrapper around the pluginIO entry point in the plugin.
 */
static boffset_t my_plugin_blseek(BareosWinFilePacket *bfd, boffset_t offset, int whence)
{
   Plugin *plugin;
   struct io_pkt io;
   JobControlRecord *jcr = bfd->jcr;

   Dmsg0(debuglevel, "plugin_bseek\n");
   if (!jcr->plugin_ctx) {
      return 0;
   }
   plugin = (Plugin *)jcr->plugin_ctx->plugin;

   io.pkt_size = sizeof(io);
   io.pkt_end = sizeof(io);
   io.func = IO_SEEK;
   io.offset = offset;
   io.whence = whence;
   io.win32 = false;
   io.lerror = 0;
   plug_func(plugin)->pluginIO(jcr->plugin_ctx, &io);
   bfd->berrno = io.io_errno;
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
static bRC bareosGetValue(bpContext *ctx, bVariable var, void *value)
{
   JobControlRecord *jcr = NULL;
   if (!value) {
      return bRC_Error;
   }

   switch (var) {               /* General variables, no need of ctx */
   case bVarFDName:
      *((char **)value) = my_name;
      break;
   case bVarWorkingDir:
      *(void **)value = me->working_directory;
      break;
   case bVarExePath:
      *(char **)value = exepath;
      break;
   case bVarVersion:
      *(char **)value = version;
      break;
   case bVarDistName:
      *(char **)value = dist_name;
      break;
   default:
      if (!ctx) {               /* Other variables need context */
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
      case bVarJobId:
         *((int *)value) = jcr->JobId;
         Dmsg1(debuglevel, "fd-plugin: return bVarJobId=%d\n", jcr->JobId);
         break;
      case bVarLevel:
         *((int *)value) = jcr->getJobLevel();
         Dmsg1(debuglevel, "fd-plugin: return bVarJobLevel=%d\n", jcr->getJobLevel());
         break;
      case bVarType:
         *((int *)value) = jcr->getJobType();
         Dmsg1(debuglevel, "fd-plugin: return bVarJobType=%d\n", jcr->getJobType());
         break;
      case bVarClient:
         *((char **)value) = jcr->client_name;
         Dmsg1(debuglevel, "fd-plugin: return bVarClient=%s\n", NPRT(*((char **)value)));
         break;
      case bVarJobName:
         *((char **)value) = jcr->Job;
         Dmsg1(debuglevel, "fd-plugin: return bVarJobName=%s\n", NPRT(*((char **)value)));
         break;
      case bVarPrevJobName:
         *((char **)value) = jcr->PrevJob;
         Dmsg1(debuglevel, "fd-plugin: return bVarPrevJobName=%s\n", NPRT(*((char **)value)));
         break;
      case bVarJobStatus:
         *((int *)value) = jcr->JobStatus;
         Dmsg1(debuglevel, "fd-plugin: return bVarJobStatus=%d\n", jcr->JobStatus);
         break;
      case bVarSinceTime:
         *((int *)value) = (int)jcr->mtime;
         Dmsg1(debuglevel, "fd-plugin: return bVarSinceTime=%d\n", (int)jcr->mtime);
         break;
      case bVarAccurate:
         *((int *)value) = (int)jcr->accurate;
         Dmsg1(debuglevel, "fd-plugin: return bVarAccurate=%d\n", (int)jcr->accurate);
         break;
      case bVarFileSeen:
         break;                 /* a write only variable, ignore read request */
      case bVarVssClient:
#ifdef HAVE_WIN32
         if (jcr->pVSSClient) {
            *(void **)value = jcr->pVSSClient;
            Dmsg1(debuglevel, "fd-plugin: return bVarVssClient=%p\n", *(void **)value);
            break;
         }
#endif
         return bRC_Error;
      case bVarWhere:
         *(char **)value = jcr->where;
         Dmsg1(debuglevel, "fd-plugin: return bVarWhere=%s\n", NPRT(*((char **)value)));
         break;
      case bVarRegexWhere:
         *(char **)value = jcr->RegexWhere;
         Dmsg1(debuglevel, "fd-plugin: return bVarRegexWhere=%s\n", NPRT(*((char **)value)));
         break;
      case bVarPrefixLinks:
         *(int *)value = (int)jcr->prefix_links;
         Dmsg1(debuglevel, "fd-plugin: return bVarPrefixLinks=%d\n", (int)jcr->prefix_links);
         break;
      default:
         break;
      }
   }

   return bRC_OK;
}

static bRC bareosSetValue(bpContext *ctx, bVariable var, void *value)
{
   JobControlRecord *jcr;

   if (!value || !ctx) {
      return bRC_Error;
   }

   jcr = ((b_plugin_ctx *)ctx->bContext)->jcr;
   if (!jcr) {
      return bRC_Error;
   }

   switch (var) {
   case bVarLevel:
      jcr->setJobLevel(*(int *)value);
      break;
   case bVarFileSeen:
      if (!accurate_mark_file_as_seen(jcr, (char *)value)) {
         return bRC_Error;
      }
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
      Dmsg1(debuglevel, "fd-plugin: Plugin registered event=%u\n", event);
      set_bit(event, b_ctx->events);
   }
   va_end(args);

   return bRC_OK;
}

/**
 * Get the number of instaces instantiated of a certain plugin.
 */
static bRC bareosGetInstanceCount(bpContext *ctx, int *ret)
{
   int cnt;
   JobControlRecord *jcr, *njcr;
   bpContext *nctx;
   b_plugin_ctx *bctx;
   bRC retval = bRC_Error;

   if (!is_ctx_good(ctx, jcr, bctx)) {
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
      Dmsg1(debuglevel, "fd-plugin: Plugin unregistered event=%u\n", event);
      clear_bit(event, b_ctx->events);
   }
   va_end(args);

   return bRC_OK;
}

static bRC bareosJobMsg(bpContext *ctx, const char *fname, int line,
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
   buffer.bvsprintf(fmt, arg_ptr);
   va_end(arg_ptr);
   Jmsg(jcr, type, mtime, "%s", buffer.c_str());

   return bRC_OK;
}

static bRC bareosDebugMsg(bpContext *ctx, const char *fname, int line,
                          int level, const char *fmt, ...)
{
   va_list arg_ptr;
   PoolMem buffer(PM_MESSAGE);

   va_start(arg_ptr, fmt);
   buffer.bvsprintf(fmt, arg_ptr);
   va_end(arg_ptr);
   d_msg(fname, line, level, "%s", buffer.c_str());

   return bRC_OK;
}

static void *bareosMalloc(bpContext *ctx, const char *fname, int line, size_t size)
{
#ifdef SMARTALLOC
   return sm_malloc(fname, line, size);
#else
   return malloc(size);
#endif
}

static void bareosFree(bpContext *ctx, const char *fname, int line, void *mem)
{
#ifdef SMARTALLOC
   sm_free(fname, line, mem);
#else
   free(mem);
#endif
}

/**
 * Let the plugin define files/directories to be excluded from the main backup.
 */
static bRC bareosAddExclude(bpContext *ctx, const char *fname)
{
   JobControlRecord *jcr;
   findIncludeExcludeItem *old;
   b_plugin_ctx *bctx;

   if (!is_ctx_good(ctx, jcr, bctx)) {
      return bRC_Error;
   }
   if (!fname) {
      return bRC_Error;
   }

   /*
    * Save the include context
    */
   old = get_incexe(jcr);

   /*
    * Not right time to add exlude
    */
   if (!old) {
      return bRC_Error;
   }

   if (!bctx->exclude) {
      bctx->exclude = new_exclude(jcr->ff->fileset);
   }

   /*
    * Set the Exclude context
    */
   set_incexe(jcr, bctx->exclude);

   add_file_to_fileset(jcr, fname, true);

   /*
    * Restore the current context
    */
   set_incexe(jcr, old);

   Dmsg1(100, "Add exclude file=%s\n", fname);

   return bRC_OK;
}

/**
 * Let the plugin define files/directories to be excluded from the main backup.
 */
static bRC bareosAddInclude(bpContext *ctx, const char *fname)
{
   JobControlRecord *jcr;
   findIncludeExcludeItem *old;
   b_plugin_ctx *bctx;

   if (!is_ctx_good(ctx, jcr, bctx)) {
      return bRC_Error;
   }

   if (!fname) {
      return bRC_Error;
   }

   /*
    * Save the include context
    */
   old = get_incexe(jcr);

   /*
    * Not right time to add include
    */
   if (!old) {
      return bRC_Error;
   }

   if (!bctx->include) {
      bctx->include = old;
   }

   set_incexe(jcr, bctx->include);
   add_file_to_fileset(jcr, fname, true);

   /*
    * Restore the current context
    */
   set_incexe(jcr, old);

   Dmsg1(100, "Add include file=%s\n", fname);

   return bRC_OK;
}

static bRC bareosAddOptions(bpContext *ctx, const char *opts)
{
   JobControlRecord *jcr;
   b_plugin_ctx *bctx;

   if (!is_ctx_good(ctx, jcr, bctx)) {
      return bRC_Error;
   }

   if (!opts) {
      return bRC_Error;
   }
   add_options_to_fileset(jcr, opts);
   Dmsg1(1000, "Add options=%s\n", opts);

   return bRC_OK;
}

static bRC bareosAddRegex(bpContext *ctx, const char *item, int type)
{
   JobControlRecord *jcr;
   b_plugin_ctx *bctx;

   if (!is_ctx_good(ctx, jcr, bctx)) {
      return bRC_Error;
   }

   if (!item) {
      return bRC_Error;
   }
   add_regex_to_fileset(jcr, item, type);
   Dmsg1(100, "Add regex=%s\n", item);

   return bRC_OK;
}

static bRC bareosAddWild(bpContext *ctx, const char *item, int type)
{
   JobControlRecord *jcr;
   b_plugin_ctx *bctx;

   if (!is_ctx_good(ctx, jcr, bctx)) {
      return bRC_Error;
   }

   if (!item) {
      return bRC_Error;
   }
   add_wild_to_fileset(jcr, item, type);
   Dmsg1(100, "Add wild=%s\n", item);

   return bRC_OK;
}

static bRC bareosNewOptions(bpContext *ctx)
{
   JobControlRecord *jcr;
   b_plugin_ctx *bctx;

   if (!is_ctx_good(ctx, jcr, bctx)) {
      return bRC_Error;
   }
   (void)new_options(jcr->ff, jcr->ff->fileset->incexe);

   return bRC_OK;
}

static bRC bareosNewInclude(bpContext *ctx)
{
   JobControlRecord *jcr;
   b_plugin_ctx *bctx;

   if (!is_ctx_good(ctx, jcr, bctx)) {
      return bRC_Error;
   }
   (void)new_include(jcr->ff->fileset);

   return bRC_OK;
}

static bRC bareosNewPreInclude(bpContext *ctx)
{
   JobControlRecord *jcr;
   b_plugin_ctx *bctx;

   if (!is_ctx_good(ctx, jcr, bctx)) {
      return bRC_Error;
   }

   bctx->include = new_preinclude(jcr->ff->fileset);
   new_options(jcr->ff, bctx->include);
   set_incexe(jcr, bctx->include);

   return bRC_OK;
}

/**
 * Check if a file have to be backuped using Accurate code
 */
static bRC bareosCheckChanges(bpContext *ctx, struct save_pkt *sp)
{
   JobControlRecord *jcr;
   b_plugin_ctx *bctx;
   FindFilesPacket *ff_pkt;
   bRC retval = bRC_Error;

   if (!is_ctx_good(ctx, jcr, bctx)) {
      goto bail_out;
   }

   if (!sp) {
      goto bail_out;
   }

   ff_pkt = jcr->ff;
   /*
    * Copy fname and link because save_file() zaps them.
    * This avoids zaping the plugin's strings.
    */
   ff_pkt->type = sp->type;
   if (!sp->fname) {
      Jmsg0(jcr, M_FATAL, 0, _("Command plugin: no fname in bareosCheckChanges packet.\n"));
      goto bail_out;
   }

   ff_pkt->fname = sp->fname;
   ff_pkt->link = sp->link;
   if (sp->save_time) {
      ff_pkt->save_time = sp->save_time;
      ff_pkt->incremental = true;
   }
   memcpy(&ff_pkt->statp, &sp->statp, sizeof(ff_pkt->statp));

   if (check_changes(jcr, ff_pkt))  {
      retval = bRC_OK;
   } else {
      retval = bRC_Seen;
   }

   /*
    * check_changes() can update delta sequence number, return it to the plugin
    */
   sp->delta_seq = ff_pkt->delta_seq;
   sp->accurate_found = ff_pkt->accurate_found;

bail_out:
   Dmsg1(100, "checkChanges=%i\n", retval);
   return retval;
}

/**
 * Check if a file would be saved using current Include/Exclude code
 */
static bRC bareosAcceptFile(bpContext *ctx, struct save_pkt *sp)
{
   JobControlRecord *jcr;
   FindFilesPacket *ff_pkt;
   b_plugin_ctx *bctx;
   bRC retval = bRC_Error;

   if (!is_ctx_good(ctx, jcr, bctx)) {
      goto bail_out;
   }
   if (!sp) {
      goto bail_out;
   }

   ff_pkt = jcr->ff;

   ff_pkt->fname = sp->fname;
   memcpy(&ff_pkt->statp, &sp->statp, sizeof(ff_pkt->statp));

   if (accept_file(ff_pkt)) {
      retval = bRC_OK;
   } else {
      retval = bRC_Skip;
   }

bail_out:
   return retval;
}

/**
 * Manipulate the accurate seen bitmap for setting bits
 */
static bRC bareosSetSeenBitmap(bpContext *ctx, bool all, char *fname)
{
   JobControlRecord *jcr;
   b_plugin_ctx *bctx;
   bRC retval = bRC_Error;

   if (!is_ctx_good(ctx, jcr, bctx)) {
      goto bail_out;
   }

   if (all && fname) {
      Dmsg0(debuglevel, "fd-plugin: API error in call to SetSeenBitmap, both all and fname set!!!\n");
      goto bail_out;
   }

   if (all) {
      if (accurate_mark_all_files_as_seen(jcr)) {
         retval = bRC_OK;
      }
   } else if (fname) {
      if (accurate_mark_file_as_seen(jcr, fname)) {
         retval = bRC_OK;
      }
   }

bail_out:
   return retval;
}

/**
 * Manipulate the accurate seen bitmap for clearing bits
 */
static bRC bareosClearSeenBitmap(bpContext *ctx, bool all, char *fname)
{
   JobControlRecord *jcr;
   b_plugin_ctx *bctx;
   bRC retval = bRC_Error;

   if (!is_ctx_good(ctx, jcr, bctx)) {
      goto bail_out;
   }

   if (all && fname) {
      Dmsg0(debuglevel, "fd-plugin: API error in call to ClearSeenBitmap, both all and fname set!!!\n");
      goto bail_out;
   }

   if (all) {
      if (accurate_unmark_all_files_as_seen(jcr)) {
         retval = bRC_OK;
      }
   } else if (fname) {
      if (accurate_unmark_file_as_seen(jcr, fname)) {
         retval = bRC_OK;
      }
   }

bail_out:
   return retval;
}

#ifdef TEST_PROGRAM
/* Exported variables */
ClientResource *me;                        /* my resource */

int save_file(JobControlRecord *jcr, FindFilesPacket *ff_pkt, bool top_level)
{
   return 0;
}

bool accurate_mark_file_as_seen(JobControlRecord *jcr, char *fname)
{
   return true;
}

bool set_cmd_plugin(BareosWinFilePacket *bfd, JobControlRecord *jcr)
{
   return true;
}

int main(int argc, char *argv[])
{
   char plugin_dir[PATH_MAX];
   JobControlRecord mjcr1, mjcr2;
   JobControlRecord *jcr1 = &mjcr1;
   JobControlRecord *jcr2 = &mjcr2;

   my_name_is(argc, argv, "plugtest");
   init_msg(NULL, NULL);

   OSDependentInit();

   if (argc != 1) {
      bstrncpy(plugin_dir, argv[1], sizeof(plugin_dir));
   } else {
      getcwd(plugin_dir, sizeof(plugin_dir)-1);
   }
   load_fd_plugins(plugin_dir, NULL);

   jcr1->JobId = 111;
   new_plugins(jcr1);

   jcr2->JobId = 222;
   new_plugins(jcr2);

   generate_plugin_event(jcr1, bEventJobStart, (void *)"Start Job 1");
   generate_plugin_event(jcr1, bEventJobEnd);
   generate_plugin_event(jcr2, bEventJobStart, (void *)"Start Job 2");
   free_plugins(jcr1);
   generate_plugin_event(jcr2, bEventJobEnd);
   free_plugins(jcr2);

   unload_fd_plugins();

   Dmsg0(debuglevel, "bareos: OK ...\n");

   term_msg();
   close_memory_pool();
   lmgr_cleanup_main();
   sm_dump(false);
   exit(0);
}

#endif /* TEST_PROGRAM */
