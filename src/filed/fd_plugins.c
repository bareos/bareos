/*
   Bacula® - The Network Backup Solution

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

   Bacula® is a registered trademark of Kern Sibbald.
   The licensor of Bacula is the Free Software Foundation Europe
   (FSFE), Fiduciary Program, Sumatrastrasse 25, 8006 Zürich,
   Switzerland, email:ftf@fsfeurope.org.
*/
/**
 * Main program to test loading and running Bacula plugins.
 *   Destined to become Bacula pluginloader, ...
 *
 * Kern Sibbald, October 2007
 */
#include "bacula.h"
#include "filed.h"

extern CLIENT *me;
extern DLL_IMP_EXP char *exepath;
extern DLL_IMP_EXP char *version;
extern DLL_IMP_EXP char *dist_name;
extern DLL_IMP_EXP int beef;

const int dbglvl = 150;
#ifdef HAVE_WIN32
const char *plugin_type = "-fd.dll";
#else
const char *plugin_type = "-fd.so";
#endif

extern int save_file(JCR *jcr, FF_PKT *ff_pkt, bool top_level);
extern bool check_changes(JCR *jcr, FF_PKT *ff_pkt);

/* Function pointers to be set here */
extern DLL_IMP_EXP int     (*plugin_bopen)(BFILE *bfd, const char *fname, int flags, mode_t mode);
extern DLL_IMP_EXP int     (*plugin_bclose)(BFILE *bfd);
extern DLL_IMP_EXP ssize_t (*plugin_bread)(BFILE *bfd, void *buf, size_t count);
extern DLL_IMP_EXP ssize_t (*plugin_bwrite)(BFILE *bfd, void *buf, size_t count);
extern DLL_IMP_EXP boffset_t (*plugin_blseek)(BFILE *bfd, boffset_t offset, int whence);


/* Forward referenced functions */
static bRC baculaGetValue(bpContext *ctx, bVariable var, void *value);
static bRC baculaSetValue(bpContext *ctx, bVariable var, void *value);
static bRC baculaRegisterEvents(bpContext *ctx, ...);
static bRC baculaJobMsg(bpContext *ctx, const char *file, int line,
  int type, utime_t mtime, const char *fmt, ...);
static bRC baculaDebugMsg(bpContext *ctx, const char *file, int line,
  int level, const char *fmt, ...);
static void *baculaMalloc(bpContext *ctx, const char *file, int line,
              size_t size);
static void baculaFree(bpContext *ctx, const char *file, int line, void *mem);
static bRC  baculaAddExclude(bpContext *ctx, const char *file);
static bRC baculaAddInclude(bpContext *ctx, const char *file);
static bRC baculaAddOptions(bpContext *ctx, const char *opts);
static bRC baculaAddRegex(bpContext *ctx, const char *item, int type);
static bRC baculaAddWild(bpContext *ctx, const char *item, int type);
static bRC baculaNewOptions(bpContext *ctx);
static bRC baculaNewInclude(bpContext *ctx);
static bRC baculaNewPreInclude(bpContext *ctx);
static bool is_plugin_compatible(Plugin *plugin);
static bool get_plugin_name(JCR *jcr, char *cmd, int *ret);
static bRC baculaCheckChanges(bpContext *ctx, struct save_pkt *sp);
static bRC baculaAcceptFile(bpContext *ctx, struct save_pkt *sp);

/*
 * These will be plugged into the global pointer structure for
 *  the findlib.
 */
static int     my_plugin_bopen(BFILE *bfd, const char *fname, int flags, mode_t mode);
static int     my_plugin_bclose(BFILE *bfd);
static ssize_t my_plugin_bread(BFILE *bfd, void *buf, size_t count);
static ssize_t my_plugin_bwrite(BFILE *bfd, void *buf, size_t count);
static boffset_t my_plugin_blseek(BFILE *bfd, boffset_t offset, int whence);


/* Bacula info */
static bInfo binfo = {
   sizeof(bInfo),
   FD_PLUGIN_INTERFACE_VERSION 
};

/* Bacula entry points */
static bFuncs bfuncs = {
   sizeof(bFuncs),
   FD_PLUGIN_INTERFACE_VERSION,
   baculaRegisterEvents,
   baculaGetValue,
   baculaSetValue,
   baculaJobMsg,
   baculaDebugMsg,
   baculaMalloc,
   baculaFree,
   baculaAddExclude,
   baculaAddInclude,
   baculaAddOptions,
   baculaAddRegex,
   baculaAddWild,
   baculaNewOptions,
   baculaNewInclude,
   baculaNewPreInclude,
   baculaCheckChanges,
   baculaAcceptFile
};

/* 
 * Bacula private context
 */
struct bacula_ctx {
   JCR *jcr;                             /* jcr for plugin */
   bRC  rc;                              /* last return code */
   bool disabled;                        /* set if plugin disabled */
   findINCEXE *exclude;                  /* pointer to exclude files */
   findINCEXE *include;                  /* pointer to include/exclude files */
};

/*
 * Test if event is for this plugin
 */
static bool for_this_plugin(Plugin *plugin, char *name, int len)
{
   Dmsg4(dbglvl, "name=%s len=%d plugin=%s plen=%d\n", name, len, plugin->file, plugin->file_len);
   if (!name) {   /* if no plugin name, all plugins get it */
      return true;
   }
   /* Return global VSS job metadata to all plugins */
   if (strcmp("job", name) == 0) {  /* old V4.0 name for VSS job metadata */
      return true;
   }
   if (strcmp("*all*", name) == 0) { /* new v6.0 name for VSS job metadata */
      return true;
   } 
   /* Check if this is the correct plugin */
   if (len == plugin->file_len && strncmp(plugin->file, name, len) == 0) {
      return true;
   }
   return false;
}


bool is_plugin_disabled(bpContext *plugin_ctx)
{
   bacula_ctx *b_ctx;
   Dsm_check(999);
   if (!plugin_ctx) {
      return true;
   }
   b_ctx = (bacula_ctx *)plugin_ctx->bContext;
   if (!b_ctx) {
      return true;
   }
   return b_ctx->disabled;
}

bool is_plugin_disabled(JCR *jcr)
{
   return is_plugin_disabled(jcr->plugin_ctx);
}

/**
 * Create a plugin event When receiving bEventCancelCommand, this function is
 * called by an other thread.
 */
void generate_plugin_event(JCR *jcr, bEventType eventType, void *value)     
{
   bpContext *plugin_ctx;
   bEvent event;
   Plugin *plugin;
   char *name = NULL;
   int i;
   int len = 0;
   bool call_if_canceled = false;
   restore_object_pkt *rop;

   Dsm_check(999);
   if (!bplugin_list || !jcr || !jcr->plugin_ctx_list) {
      return;                         /* Return if no plugins loaded */
   }
   
   /*
    * Some events are sent to only a particular plugin or must be
    *  called even if the job is canceled
    */
   switch(eventType) {
   case bEventPluginCommand:
   case bEventOptionPlugin:
      name = (char *)value;
      if (!get_plugin_name(jcr, name, &len)) {
         return;
      }
      break;
   case bEventRestoreObject:
      /* After all RestoreObject, we have it one more time with value=NULL */
      if (value) {
         /* Some RestoreObjects may not have a plugin name */
         rop = (restore_object_pkt *)value;
         if (*rop->plugin_name) {
            name = rop->plugin_name;
            get_plugin_name(jcr, name, &len);
         }
      }

      break;
   case bEventEndBackupJob:
   case bEventEndVerifyJob:
      call_if_canceled = true; /* plugin *must* see this call */
      break;
   case bEventStartRestoreJob:
      foreach_alist_index(i, plugin, bplugin_list) {
         plugin->restoreFileStarted = false;
         plugin->createFileCalled = false;
      }
      break;
   case bEventEndRestoreJob:
      call_if_canceled = true; /* plugin *must* see this call */
      break;
   default:
      break;
   }

   /* If call_if_canceled is set, we call the plugin anyway */
   if (!call_if_canceled && jcr->is_job_canceled()) {
      return;
   }

   bpContext *plugin_ctx_list = (bpContext *)jcr->plugin_ctx_list;
   event.eventType = eventType;

   Dmsg2(dbglvl, "plugin_ctx=%p JobId=%d\n", jcr->plugin_ctx_list, jcr->JobId);

   /*
    * Pass event to every plugin (except if name is set). If name
    *   is set, we pass it only to the plugin with that name.
    */
   foreach_alist_index(i, plugin, bplugin_list) {
      if (!for_this_plugin(plugin, name, len)) {
         Dmsg2(dbglvl, "Not for this plugin name=%s NULL=%d\n", 
            name, name==NULL?1:0);
         continue;
      }
      /*
       * Note, at this point do not change 
       *   jcr->plugin or jcr->plugin_ctx
       */
      Dsm_check(999);
      plugin_ctx = &plugin_ctx_list[i];
      if (is_plugin_disabled(plugin_ctx)) {
         continue;
      }
      if (eventType == bEventEndRestoreJob) {
         Dmsg0(50, "eventType==bEventEndRestoreJob\n");
         if (jcr->plugin && jcr->plugin->restoreFileStarted) {
            plug_func(jcr->plugin)->endRestoreFile(jcr->plugin_ctx);
         }
         if (jcr->plugin) {
            jcr->plugin->restoreFileStarted = false;
            jcr->plugin->createFileCalled = false;
         }
      }
      plug_func(plugin)->handlePluginEvent(plugin_ctx, &event, value);
   }
   return;
}

/**
 * Check if file was seen for accurate
 */
bool plugin_check_file(JCR *jcr, char *fname)
{
   Plugin *plugin;
   int rc = bRC_OK;
   int i;

   Dsm_check(999);
   if (!bplugin_list || !jcr || !jcr->plugin_ctx_list || jcr->is_job_canceled()) {
      return false;                      /* Return if no plugins loaded */
   }

   bpContext *plugin_ctx_list = (bpContext *)jcr->plugin_ctx_list;

   Dmsg2(dbglvl, "plugin_ctx=%p JobId=%d\n", jcr->plugin_ctx_list, jcr->JobId);

   /* Pass event to every plugin */
   foreach_alist_index(i, plugin, bplugin_list) {
      jcr->plugin_ctx = &plugin_ctx_list[i];
      jcr->plugin = plugin;
      if (is_plugin_disabled(jcr)) {
         continue;
      }
      if (plug_func(plugin)->checkFile == NULL) {
         continue;
      }
      rc = plug_func(plugin)->checkFile(jcr->plugin_ctx, fname);
      if (rc == bRC_Seen) {
         break;
      }
   }

   Dsm_check(999);
   jcr->plugin = NULL;
   jcr->plugin_ctx = NULL;
   return rc == bRC_Seen;
}

/* Get the first part of the the plugin command
 *  systemstate:/@SYSTEMSTATE/ 
 * => ret = 11
 * => can use for_this_plugin(plug, cmd, ret);
 *
 * The plugin command can contain only the plugin name
 *  Plugin = alldrives
 * => ret = 9
 */
static bool get_plugin_name(JCR *jcr, char *cmd, int *ret)
{
   char *p;
   int len;
   Dsm_check(999);
   if (!cmd || (*cmd == '\0')) {
      return false;
   }
   /* Handle plugin command here backup */
   Dmsg1(dbglvl, "plugin cmd=%s\n", cmd);
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
   Dsm_check(999);
   return true;
}


static void update_ff_pkt(FF_PKT *ff_pkt, struct save_pkt *sp)
{
   Dsm_check(999);
   ff_pkt->no_read = sp->no_read;
   ff_pkt->delta_seq = sp->delta_seq;
   if (sp->flags & FO_DELTA) {
      ff_pkt->flags |= FO_DELTA;
      ff_pkt->delta_seq++;          /* make new delta sequence number */
   } else {
      ff_pkt->flags &= ~FO_DELTA;   /* clean delta sequence number */
      ff_pkt->delta_seq = 0;
   }
   
   if (sp->flags & FO_OFFSETS) {
      ff_pkt->flags |= FO_OFFSETS;
   } else {
      ff_pkt->flags &= ~FO_OFFSETS;
   }
   /* Sparse code doesn't work with plugins
    * that use FIFO or STDOUT/IN to communicate 
    */
   if (sp->flags & FO_SPARSE) {
      ff_pkt->flags |= FO_SPARSE;
   } else {
      ff_pkt->flags &= ~FO_SPARSE;
   }
   if (sp->flags & FO_PORTABLE_DATA) {
      ff_pkt->flags |= FO_PORTABLE_DATA;
   } else {
      ff_pkt->flags &= ~FO_PORTABLE_DATA;
   }
   ff_pkt->flags |= FO_PLUGIN;       /* data from plugin */
   Dsm_check(999);
}

/* Ask to a Option Plugin what to do with the current file */
bRC plugin_option_handle_file(JCR *jcr, FF_PKT *ff_pkt, struct save_pkt *sp)
{
   Plugin *plugin;
   bRC ret = bRC_Error;
   char *cmd = ff_pkt->plugin;
   int len;
   int i=0;
   bEvent event;
   event.eventType = bEventHandleBackupFile;

   Dsm_check(999);
   bpContext *plugin_ctx_list = (bpContext *)jcr->plugin_ctx_list;
   memset(sp, 0, sizeof(struct save_pkt));
   sp->pkt_size = sp->pkt_end = sizeof(struct save_pkt);
   sp->portable = true;
   sp->cmd = cmd;
   sp->link = ff_pkt->link;
   sp->cmd = ff_pkt->plugin;
   sp->statp = ff_pkt->statp;
   sp->fname = ff_pkt->fname;
   sp->delta_seq = ff_pkt->delta_seq;
   sp->accurate_found = ff_pkt->accurate_found;

   if (!bplugin_list || !jcr->plugin_ctx_list || jcr->is_job_canceled()) {
      Jmsg1(jcr, M_FATAL, 0, "Command plugin \"%s\" requested, but is not loaded.\n", cmd);
      goto bail_out;         /* Return if no plugins loaded */
   }

   if (!get_plugin_name(jcr, cmd, &len)) {
      goto bail_out;
   }

   /* Note, we stop the loop on the first plugin that matches the name */
   foreach_alist_index(i, plugin, bplugin_list) {
      Dmsg4(dbglvl, "plugin=%s plen=%d cmd=%s len=%d\n", plugin->file, plugin->file_len, cmd, len);
      if (!for_this_plugin(plugin, cmd, len)) {
         continue;
      }

      Dsm_check(999);
      if (is_plugin_disabled(&plugin_ctx_list[i])) {
         goto bail_out;
      }

      jcr->plugin_ctx = &plugin_ctx_list[i];
      jcr->plugin = plugin;
      
      ret = plug_func(plugin)->handlePluginEvent(&plugin_ctx_list[i], 
                                                 &event, sp);
      
      /* TODO: would be better to set this in save_file() */
      if (ret == bRC_OK) {
         jcr->opt_plugin = true;
         jcr->plugin = plugin;
         jcr->plugin_sp = sp;      /* Unset sp in save_file */
         jcr->plugin_ctx = &plugin_ctx_list[i];

         update_ff_pkt(ff_pkt, sp);

      /* reset plugin in JCR if not used this time */
      } else {
         jcr->plugin_ctx = NULL;
         jcr->plugin = NULL;
      }

      goto bail_out;
   } /* end foreach loop */
bail_out:
   Dsm_check(999);
   return ret;
}

/**  
 * Sequence of calls for a backup:
 * 1. plugin_save() here is called with ff_pkt
 * 2. we find the plugin requested on the command string
 * 3. we generate a bEventBackupCommand event to the specified plugin
 *    and pass it the command string.
 * 4. we make a startPluginBackup call to the plugin, which gives
 *    us the data we need in save_pkt
 * 5. we call Bacula's save_file() subroutine to save the specified
 *    file.  The plugin will be called at pluginIO() to supply the
 *    file data.
 *
 * Sequence of calls for restore:
 *   See subroutine plugin_name_stream() below.
 */
int plugin_save(JCR *jcr, FF_PKT *ff_pkt, bool top_level)
{
   Plugin *plugin;
   int len;
   int i;
   char *cmd = ff_pkt->top_fname;
   struct save_pkt sp;
   bEvent event;
   POOL_MEM fname(PM_FNAME);
   POOL_MEM link(PM_FNAME);

   Dsm_check(999);
   if (!bplugin_list || !jcr->plugin_ctx_list || jcr->is_job_canceled()) {
      Jmsg1(jcr, M_FATAL, 0, "Command plugin \"%s\" requested, but is not loaded.\n", cmd);
      return 1;                            /* Return if no plugins loaded */
   }

   jcr->cmd_plugin = true;
   bpContext *plugin_ctx_list = (bpContext *)jcr->plugin_ctx_list;
   event.eventType = bEventBackupCommand;

   if (!get_plugin_name(jcr, cmd, &len)) {
      goto bail_out;
   }

   /* Note, we stop the loop on the first plugin that matches the name */
   foreach_alist_index(i, plugin, bplugin_list) {
      Dmsg4(dbglvl, "plugin=%s plen=%d cmd=%s len=%d\n", plugin->file, plugin->file_len, cmd, len);
      if (!for_this_plugin(plugin, cmd, len)) {
         continue;
      }
      /* 
       * We put the current plugin pointer, and the plugin context
       *  into the jcr, because during save_file(), the plugin
       *  will be called many times and these values are needed.
       */
      Dsm_check(999);
      jcr->plugin_ctx = &plugin_ctx_list[i];
      jcr->plugin = plugin;
      if (is_plugin_disabled(jcr)) {
         goto bail_out;
      }

      Dmsg1(dbglvl, "Command plugin = %s\n", cmd);
      /* Send the backup command to the right plugin*/
      if (plug_func(plugin)->handlePluginEvent(jcr->plugin_ctx, &event, cmd) != bRC_OK) {
         goto bail_out;
      }
      /* Loop getting filenames to backup then saving them */
      while (!jcr->is_job_canceled()) { 
         memset(&sp, 0, sizeof(sp));
         sp.pkt_size = sizeof(sp);
         sp.pkt_end = sizeof(sp);
         sp.portable = true;
         sp.no_read = false;
         sp.flags = 0;
         sp.cmd = cmd;
         Dmsg3(dbglvl, "startBackup st_size=%p st_blocks=%p sp=%p\n", &sp.statp.st_size, &sp.statp.st_blocks,
                &sp);
         Dsm_check(999);
         /* Get the file save parameters. I.e. the stat pkt ... */
         if (plug_func(plugin)->startBackupFile(jcr->plugin_ctx, &sp) != bRC_OK) {
            goto bail_out;
         }
         if (sp.type == 0) {
            Jmsg1(jcr, M_FATAL, 0, _("Command plugin \"%s\": no type in startBackupFile packet.\n"),
               cmd);
            goto bail_out;
         }
         jcr->plugin_sp = &sp;
         ff_pkt = jcr->ff;
         /*
          * Copy fname and link because save_file() zaps them.  This 
          *  avoids zaping the plugin's strings.
          */
         ff_pkt->type = sp.type;
         if (IS_FT_OBJECT(sp.type)) {
            if (!sp.object_name) {
               Jmsg1(jcr, M_FATAL, 0, _("Command plugin \"%s\": no object_name in startBackupFile packet.\n"),
                  cmd);
               goto bail_out;
            }
            ff_pkt->fname = cmd;                 /* full plugin string */
            ff_pkt->object_name = sp.object_name;
            ff_pkt->object_index = sp.index;     /* restore object index */
            ff_pkt->object_compression = 0;      /* no compression for now */
            ff_pkt->object = sp.object;
            ff_pkt->object_len = sp.object_len;
         } else {
            Dsm_check(999);
            if (!sp.fname) {
               Jmsg1(jcr, M_FATAL, 0, _("Command plugin \"%s\": no fname in startBackupFile packet.\n"),
                  cmd);
               goto bail_out;
            }
            pm_strcpy(fname, sp.fname);
            pm_strcpy(link, sp.link);


            ff_pkt->fname = fname.c_str();
            ff_pkt->link = link.c_str();
            update_ff_pkt(ff_pkt, &sp);
         }

         memcpy(&ff_pkt->statp, &sp.statp, sizeof(ff_pkt->statp));
         Dmsg2(dbglvl, "startBackup returned type=%d, fname=%s\n", sp.type, sp.fname);
         if (sp.object) {
            Dmsg2(dbglvl, "index=%d object=%s\n", sp.index, sp.object);
         }   
         /* Call Bacula core code to backup the plugin's file */
         save_file(jcr, ff_pkt, true);
         bRC rc = plug_func(plugin)->endBackupFile(jcr->plugin_ctx);
         if (rc == bRC_More || rc == bRC_OK) {
            accurate_mark_file_as_seen(jcr, fname.c_str());
         }
         Dsm_check(999);
         if (rc == bRC_More) {
            continue;
         }
         goto bail_out;
      } /* end while loop */
      goto bail_out;
   } /* end loop over all plugins */
   Jmsg1(jcr, M_FATAL, 0, "Command plugin \"%s\" not found.\n", cmd);

bail_out:
   Dsm_check(999);
   jcr->cmd_plugin = false;
   jcr->plugin = NULL;
   jcr->plugin_ctx = NULL;
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
int plugin_estimate(JCR *jcr, FF_PKT *ff_pkt, bool top_level)
{
   Plugin *plugin;
   int len;
   int i;
   char *cmd = ff_pkt->top_fname;
   struct save_pkt sp;
   bEvent event;
   POOL_MEM fname(PM_FNAME);
   POOL_MEM link(PM_FNAME);
   ATTR attr;

   Dsm_check(999);
   if (!bplugin_list || !jcr->plugin_ctx_list) {
      Jmsg1(jcr, M_FATAL, 0, "Command plugin \"%s\" requested, but is not loaded.\n", cmd);
      return 1;                            /* Return if no plugins loaded */
   }

   jcr->cmd_plugin = true;
   bpContext *plugin_ctx_list = (bpContext *)jcr->plugin_ctx_list;
   event.eventType = bEventEstimateCommand;

   if (!get_plugin_name(jcr, cmd, &len)) {
      goto bail_out;
   }

   /* Note, we stop the loop on the first plugin that matches the name */
   foreach_alist_index(i, plugin, bplugin_list) {
      Dmsg4(dbglvl, "plugin=%s plen=%d cmd=%s len=%d\n", plugin->file, plugin->file_len, cmd, len);
      if (!for_this_plugin(plugin, cmd, len)) {
         continue;
      }
      /* 
       * We put the current plugin pointer, and the plugin context
       *  into the jcr, because during save_file(), the plugin
       *  will be called many times and these values are needed.
       */
      Dsm_check(999);
      jcr->plugin_ctx = &plugin_ctx_list[i];
      jcr->plugin = plugin;
      if (is_plugin_disabled(jcr)) {
         goto bail_out;
      }

      Dmsg1(dbglvl, "Command plugin = %s\n", cmd);
      /* Send the backup command to the right plugin*/
      if (plug_func(plugin)->handlePluginEvent(jcr->plugin_ctx, &event, cmd) != bRC_OK) {
         goto bail_out;
      }
      /* Loop getting filenames to backup then saving them */
      while (!jcr->is_job_canceled()) { 
         Dsm_check(999);
         memset(&sp, 0, sizeof(sp));
         sp.pkt_size = sizeof(sp);
         sp.pkt_end = sizeof(sp);
         sp.portable = true;
         sp.flags = 0;
         sp.cmd = cmd;
         Dmsg3(dbglvl, "startBackup st_size=%p st_blocks=%p sp=%p\n", &sp.statp.st_size, &sp.statp.st_blocks,
                &sp);
         /* Get the file save parameters. I.e. the stat pkt ... */
         if (plug_func(plugin)->startBackupFile(jcr->plugin_ctx, &sp) != bRC_OK) {
            goto bail_out;
         }
         if (sp.type == 0) {
            Jmsg1(jcr, M_FATAL, 0, _("Command plugin \"%s\": no type in startBackupFile packet.\n"),
               cmd);
            goto bail_out;
         }

         if (!IS_FT_OBJECT(sp.type)) {
            if (!sp.fname) {
               Jmsg1(jcr, M_FATAL, 0, _("Command plugin \"%s\": no fname in startBackupFile packet.\n"),
                  cmd);
               goto bail_out;
            }

            /* Count only files backed up */
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

         Dmsg2(dbglvl, "startBackup returned type=%d, fname=%s\n", sp.type, sp.fname);
         if (sp.object) {
            Dmsg2(dbglvl, "index=%d object=%s\n", sp.index, sp.object);
         }
         bRC rc = plug_func(plugin)->endBackupFile(jcr->plugin_ctx);
         if (rc == bRC_More || rc == bRC_OK) {
            accurate_mark_file_as_seen(jcr, sp.fname);
         }
         Dsm_check(999);
         if (rc == bRC_More) {
            continue;
         }
         goto bail_out;
      } /* end while loop */
      goto bail_out;
   } /* end loop over all plugins */
   Jmsg1(jcr, M_FATAL, 0, "Command plugin \"%s\" not found.\n", cmd);

bail_out:
   Dsm_check(999);
   jcr->cmd_plugin = false;
   jcr->plugin = NULL;
   jcr->plugin_ctx = NULL;
   return 1;
}

/**
 * Send plugin name start/end record to SD
 */
bool send_plugin_name(JCR *jcr, BSOCK *sd, bool start)
{
   int stat;
   int index = jcr->JobFiles;
   struct save_pkt *sp = (struct save_pkt *)jcr->plugin_sp;

   Dsm_check(999);
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
   Dmsg1(dbglvl, "send_plugin_name=%s\n", sp->cmd);
   /* Send stream header */
   Dsm_check(999);
   if (!sd->fsend("%ld %d 0", index, STREAM_PLUGIN_NAME)) {
     Jmsg1(jcr, M_FATAL, 0, _("Network send error to SD. ERR=%s\n"),
           sd->bstrerror());
     return false;
   }
   Dmsg1(dbglvl, "send plugin name hdr: %s\n", sd->msg);

   Dsm_check(999);
   if (start) {
      /* Send data -- not much */
      stat = sd->fsend("%ld 1 %d %s%c", index, sp->portable, sp->cmd, 0);
   } else {
      /* Send end of data */
      stat = sd->fsend("%ld 0", jcr->JobFiles);
   }
   Dsm_check(999);
   if (!stat) {
      Jmsg1(jcr, M_FATAL, 0, _("Network send error to SD. ERR=%s\n"),
            sd->bstrerror());
         return false;
   }
   Dmsg1(dbglvl, "send plugin start/end: %s\n", sd->msg);
   sd->signal(BNET_EOD);            /* indicate end of plugin name data */
   Dsm_check(999);
   return true;
}

/**
 * Plugin name stream found during restore.  The record passed in
 *  argument name was generated in send_plugin_name() above.
 *
 * Returns: true  if start of stream
 *          false if end of steam
 */
bool plugin_name_stream(JCR *jcr, char *name)    
{
   char *p = name;
   char *cmd;
   bool start;
   Plugin *plugin;
   int len;
   int i;
   bpContext *plugin_ctx_list = jcr->plugin_ctx_list;

   Dsm_check(999);
   Dmsg1(dbglvl, "Read plugin stream string=%s\n", name);
   skip_nonspaces(&p);             /* skip over jcr->JobFiles */
   skip_spaces(&p);
   start = *p == '1';
   if (start) {
      /* Start of plugin data */
      skip_nonspaces(&p);          /* skip start/end flag */
      skip_spaces(&p);
//    portable = *p == '1';
      skip_nonspaces(&p);          /* skip portable flag */
      skip_spaces(&p);
      cmd = p;
   } else {
      /*
       * End of plugin data, notify plugin, then clear flags   
       */
      Dmsg2(dbglvl, "End plugin data plugin=%p ctx=%p\n", jcr->plugin, jcr->plugin_ctx);
      if (jcr->plugin && jcr->plugin->restoreFileStarted) {
         plug_func(jcr->plugin)->endRestoreFile(jcr->plugin_ctx);
      }
      if (jcr->plugin) {
         jcr->plugin->restoreFileStarted = false;
         jcr->plugin->createFileCalled = false;
      }
      jcr->plugin_ctx = NULL;
      jcr->plugin = NULL;
      goto bail_out;
   }
   Dsm_check(999);
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
   Dsm_check(999);
   foreach_alist_index(i, plugin, bplugin_list) {
      bEvent event;
      Dmsg3(dbglvl, "plugin=%s cmd=%s len=%d\n", plugin->file, cmd, len);
      if (!for_this_plugin(plugin, cmd, len)) {
         continue;
      }
      Dsm_check(999);
      jcr->plugin_ctx = &plugin_ctx_list[i];
      jcr->plugin = plugin;
      if (is_plugin_disabled(jcr)) {
         Dmsg1(dbglvl, "Plugin %s disabled\n", cmd);
         goto bail_out;
      }
      Dmsg1(dbglvl, "Restore Command plugin = %s\n", cmd);
      event.eventType = bEventRestoreCommand;     
      if (plug_func(plugin)->handlePluginEvent(jcr->plugin_ctx, 
            &event, cmd) != bRC_OK) {
         Dmsg1(dbglvl, "Handle event failed. Plugin=%s\n", cmd);
         goto bail_out;
      }
      if (plugin->restoreFileStarted) {
         Jmsg2(jcr, M_FATAL, 0, "Second call to startRestoreFile. plugin=%s cmd=%s\n", plugin->file, cmd);
         plugin->restoreFileStarted = false;
         goto bail_out;
      }
      if (plug_func(plugin)->startRestoreFile(jcr->plugin_ctx, cmd) == bRC_OK) {
         plugin->restoreFileStarted = true;
         goto ok_out;
      } else {
         Dmsg1(dbglvl, "startRestoreFile failed. plugin=%s\n", cmd);
      }
      goto bail_out;
   }
   Jmsg1(jcr, M_WARNING, 0, _("Plugin=%s not found.\n"), cmd);
   goto bail_out;

ok_out:
   return start;

bail_out:
   Dsm_check(999);
   jcr->plugin = NULL;
   jcr->plugin_ctx = NULL;
   return start;
}

/**
 * Tell the plugin to create the file.  Return values are
 *   This is called only during Restore
 *
 *  CF_ERROR    -- error
 *  CF_SKIP     -- skip processing this file
 *  CF_EXTRACT  -- extract the file (i.e.call i/o routines)
 *  CF_CREATED  -- created, but no content to extract (typically directories)
 *
 */
int plugin_create_file(JCR *jcr, ATTR *attr, BFILE *bfd, int replace)
{
   bpContext *plugin_ctx = jcr->plugin_ctx;
   Plugin *plugin = jcr->plugin;
   struct restore_pkt rp;
   int flags;
   int rc;

   Dsm_check(999);
   if (!plugin || !plugin_ctx || !set_cmd_plugin(bfd, jcr) || jcr->is_job_canceled()) {
      return CF_ERROR;
   }

   rp.pkt_size = sizeof(rp);
   rp.pkt_end = sizeof(rp);
   rp.delta_seq = attr->delta_seq;
   rp.stream = attr->stream;
   rp.data_stream = attr->data_stream;
   rp.type = attr->type;
   rp.file_index = attr->file_index;
   rp.LinkFI = attr->LinkFI;
   rp.uid = attr->uid;
   rp.statp = attr->statp;                /* structure assignment */
   rp.attrEx = attr->attrEx;
   rp.ofname = attr->ofname;
   rp.olname = attr->olname;
   rp.where = jcr->where;
   rp.RegexWhere = jcr->RegexWhere;
   rp.replace = jcr->replace;
   rp.create_status = CF_ERROR;
   Dmsg4(dbglvl, "call plugin createFile stream=%d type=%d LinkFI=%d File=%s\n", 
         rp.stream, rp.type, rp.LinkFI, rp.ofname);
   if (rp.attrEx) {
      Dmsg1(dbglvl, "attrEx=\"%s\"\n", rp.attrEx);
   }
   Dsm_check(999);
   if (!plugin->restoreFileStarted || plugin->createFileCalled) {
      Jmsg2(jcr, M_FATAL, 0, "Unbalanced call to createFile=%d %d\n",
         plugin->createFileCalled, plugin->restoreFileStarted);
      plugin->createFileCalled = false;
      return CF_ERROR;
   }
   rc = plug_func(plugin)->createFile(plugin_ctx, &rp);
   if (rc != bRC_OK) {
      Qmsg2(jcr, M_ERROR, 0, _("Plugin createFile call failed. Stat=%d file=%s\n"),
            rc, attr->ofname);
      return CF_ERROR;
   }
   if (rp.create_status == CF_ERROR) {
      Qmsg1(jcr, M_ERROR, 0, _("Plugin createFile call failed. Returned CF_ERROR file=%s\n"),
            attr->ofname);
      return CF_ERROR;
   }

   if (rp.create_status == CF_SKIP) {
      return CF_SKIP;
   }

   if (rp.create_status == CF_CORE) {
      return CF_CORE;           /* Let Bacula core handle the file creation */
   }

   /* Created link or directory? */
   if (rp.create_status == CF_CREATED) {
      return rp.create_status;        /* yes, no need to bopen */
   }

   Dsm_check(999);

   flags =  O_WRONLY | O_CREAT | O_TRUNC | O_BINARY;
   Dmsg0(dbglvl, "call bopen\n");
   int stat = bopen(bfd, attr->ofname, flags, S_IRUSR | S_IWUSR);
   Dmsg1(dbglvl, "bopen status=%d\n", stat);
   if (stat < 0) {
      berrno be;
      be.set_errno(bfd->berrno);
      Qmsg2(jcr, M_ERROR, 0, _("Could not create %s: ERR=%s\n"),
            attr->ofname, be.bstrerror());
      Dmsg2(dbglvl,"Could not bopen file %s: ERR=%s\n", attr->ofname, be.bstrerror());
      return CF_ERROR;
   }

   if (!is_bopen(bfd)) {
      Dmsg0(000, "===== BFD is not open!!!!\n");
   }
   Dsm_check(999);
   return CF_EXTRACT;
}

/**
 * Reset the file attributes after all file I/O is done -- this allows
 *  the previous access time/dates to be set properly, and it also allows
 *  us to properly set directory permissions.
 *  Not currently Implemented.
 */
bool plugin_set_attributes(JCR *jcr, ATTR *attr, BFILE *ofd)
{
   Plugin *plugin = (Plugin *)jcr->plugin;
   struct restore_pkt rp;

   Dmsg0(dbglvl, "plugin_set_attributes\n");

   if (!plugin || !jcr->plugin_ctx) {
      return false;
   }

   memset(&rp, 0, sizeof(rp));
   rp.pkt_size = sizeof(rp);
   rp.pkt_end = sizeof(rp);
   rp.stream = attr->stream;
   rp.data_stream = attr->data_stream;
   rp.type = attr->type;
   rp.file_index = attr->file_index;
   rp.LinkFI = attr->LinkFI;
   rp.uid = attr->uid;
   rp.statp = attr->statp;                /* structure assignment */
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
      pm_strcpy(attr->ofname, "*none*");
   }

   Dsm_check(999);
   return true;
}

/*
 * Print to file the plugin info.
 */
void dump_fd_plugin(Plugin *plugin, FILE *fp)
{
   if (!plugin) {
      return ;
   }
   pInfo *info = (pInfo *)plugin->pinfo;
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
void load_fd_plugins(const char *plugin_dir)
{
   Plugin *plugin;
   int i;

   if (!plugin_dir) {
      Dmsg0(dbglvl, "plugin dir is NULL\n");
      return;
   }

   bplugin_list = New(alist(10, not_owned_by_alist));
   Dsm_check(999);
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

   /* Plug entry points called from findlib */
   plugin_bopen  = my_plugin_bopen;
   plugin_bclose = my_plugin_bclose;
   plugin_bread  = my_plugin_bread;
   plugin_bwrite = my_plugin_bwrite;
   plugin_blseek = my_plugin_blseek;
   Dsm_check(999);

   /* 
    * Verify that the plugin is acceptable, and print information
    *  about it.
    */
   foreach_alist_index(i, plugin, bplugin_list) {
      Jmsg(NULL, M_INFO, 0, _("Loaded plugin: %s\n"), plugin->file);
      Dmsg1(dbglvl, "Loaded plugin: %s\n", plugin->file);
   }

   dbg_plugin_add_hook(dump_fd_plugin);
   Dsm_check(999);
}

/**
 * Check if a plugin is compatible.  Called by the load_plugin function
 *  to allow us to verify the plugin.
 */
static bool is_plugin_compatible(Plugin *plugin)
{
   pInfo *info = (pInfo *)plugin->pinfo;
   Dmsg0(dbglvl, "is_plugin_compatible called\n");
   Dsm_check(999);
   if (debug_level >= 50) {
      dump_fd_plugin(plugin, stdin);
   }
   if (strcmp(info->plugin_magic, FD_PLUGIN_MAGIC) != 0) {
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
   if (strcmp(info->plugin_license, "Bacula AGPLv3") != 0 &&
       strcmp(info->plugin_license, "AGPLv3") != 0) {
      Jmsg(NULL, M_ERROR, 0, _("Plugin license incompatible. Plugin=%s license=%s\n"),
           plugin->file, info->plugin_license);
      Dmsg2(50, "Plugin license incompatible. Plugin=%s license=%s\n",
           plugin->file, info->plugin_license);
      return false;
   }
   if (info->size != sizeof(pInfo)) {
      Jmsg(NULL, M_ERROR, 0,
           _("Plugin size incorrect. Plugin=%s wanted=%d got=%d\n"),
           plugin->file, sizeof(pInfo), info->size);
      return false;
   }
      
   Dsm_check(999);
   return true;
}


/**
 * Create a new instance of each plugin for this Job
 *   Note, bplugin_list can exist but jcr->plugin_ctx_list can
 *   be NULL if no plugins were loaded.
 */
void new_plugins(JCR *jcr)
{
   Plugin *plugin;
   int i;

   Dsm_check(999);
   if (!bplugin_list) {
      Dmsg0(dbglvl, "plugin list is NULL\n");
      return;
   }
   if (jcr->is_job_canceled()) {
      return;
   }

   int num = bplugin_list->size();

   if (num == 0) {
      Dmsg0(dbglvl, "No plugins loaded\n");
      return;
   }

   jcr->plugin_ctx_list = (bpContext *)malloc(sizeof(bpContext) * num);

   bpContext *plugin_ctx_list = (bpContext *)jcr->plugin_ctx_list;
   Dmsg2(dbglvl, "Instantiate plugin_ctx=%p JobId=%d\n", plugin_ctx_list, jcr->JobId);
   foreach_alist_index(i, plugin, bplugin_list) {
      Dsm_check(999);
      /* Start a new instance of each plugin */
      bacula_ctx *b_ctx = (bacula_ctx *)malloc(sizeof(bacula_ctx));
      memset(b_ctx, 0, sizeof(bacula_ctx));
      b_ctx->jcr = jcr;
      plugin_ctx_list[i].bContext = (void *)b_ctx;   /* Bacula private context */
      plugin_ctx_list[i].pContext = NULL;
      if (plug_func(plugin)->newPlugin(&plugin_ctx_list[i]) != bRC_OK) {
         b_ctx->disabled = true;
      }
   }
   if (i > num) {
      Jmsg2(jcr, M_ABORT, 0, "Num plugins=%d exceeds list size=%d\n",
            i, num);
   }
   Dsm_check(999);
}

/**
 * Free the plugin instances for this Job
 */
void free_plugins(JCR *jcr)
{
   Plugin *plugin;
   int i;

   if (!bplugin_list || !jcr->plugin_ctx_list) {
      return;                         /* no plugins, nothing to do */
   }

   Dsm_check(999);
   bpContext *plugin_ctx_list = (bpContext *)jcr->plugin_ctx_list;
   Dmsg2(dbglvl, "Free instance plugin_ctx=%p JobId=%d\n", plugin_ctx_list, jcr->JobId);
   foreach_alist_index(i, plugin, bplugin_list) {   
      /* Free the plugin instance */
      plug_func(plugin)->freePlugin(&plugin_ctx_list[i]);
      free(plugin_ctx_list[i].bContext);     /* free Bacula private context */
      Dsm_check(999);
   }
   Dsm_check(999);
   free(plugin_ctx_list);
   jcr->plugin_ctx_list = NULL;
}

static int my_plugin_bopen(BFILE *bfd, const char *fname, int flags, mode_t mode)
{
   JCR *jcr = bfd->jcr;
   Plugin *plugin = (Plugin *)jcr->plugin;
   struct io_pkt io;

   Dmsg1(dbglvl, "plugin_bopen flags=%x\n", flags);
   Dsm_check(999);
   if (!plugin || !jcr->plugin_ctx) {
      return 0;
   }
   io.pkt_size = sizeof(io);
   io.pkt_end = sizeof(io);
   io.func = IO_OPEN;
   io.count = 0;
   io.buf = NULL;
   io.fname = fname;
   io.flags = flags;
   io.mode = mode;
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
   Dmsg1(dbglvl, "Return from plugin open status=%d\n", io.status);
   Dsm_check(999);
   return io.status;
}

static int my_plugin_bclose(BFILE *bfd)
{
   JCR *jcr = bfd->jcr;
   Plugin *plugin = (Plugin *)jcr->plugin;
   struct io_pkt io;

   Dsm_check(999);
   Dmsg0(dbglvl, "===== plugin_bclose\n");
   if (!plugin || !jcr->plugin_ctx) {
      return 0;
   }
   io.pkt_size = sizeof(io);
   io.pkt_end = sizeof(io);
   io.func = IO_CLOSE;
   io.count = 0;
   io.buf = NULL;
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
   Dmsg1(dbglvl, "plugin_bclose stat=%d\n", io.status);
   Dsm_check(999);
   return io.status;
}

static ssize_t my_plugin_bread(BFILE *bfd, void *buf, size_t count)
{
   JCR *jcr = bfd->jcr;
   Plugin *plugin = (Plugin *)jcr->plugin;
   struct io_pkt io;

   Dsm_check(999);
   Dmsg0(dbglvl, "plugin_bread\n");
   if (!plugin || !jcr->plugin_ctx) {
      return 0;
   }
   io.pkt_size = sizeof(io);
   io.pkt_end = sizeof(io);
   io.func = IO_READ;
   io.count = count;
   io.buf = (char *)buf;
   io.win32 = false;
   io.offset = 0;
   io.lerror = 0;
   plug_func(plugin)->pluginIO(jcr->plugin_ctx, &io);
   bfd->offset = io.offset;
   bfd->berrno = io.io_errno;
   if (io.win32) {
      errno = b_errno_win32;
   } else {
      errno = io.io_errno;
      bfd->lerror = io.lerror;
   }
   Dsm_check(999);
   return (ssize_t)io.status;
}

static ssize_t my_plugin_bwrite(BFILE *bfd, void *buf, size_t count)
{
   JCR *jcr = bfd->jcr;
   Plugin *plugin = (Plugin *)jcr->plugin;
   struct io_pkt io;

   Dsm_check(999);
   Dmsg0(dbglvl, "plugin_bwrite\n");
   if (!plugin || !jcr->plugin_ctx) {
      Dmsg0(0, "No plugin context\n");
      return 0;
   }
   io.pkt_size = sizeof(io);
   io.pkt_end = sizeof(io);
   io.func = IO_WRITE;
   io.count = count;
   io.buf = (char *)buf;
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
   Dsm_check(999);
   return (ssize_t)io.status;
}

static boffset_t my_plugin_blseek(BFILE *bfd, boffset_t offset, int whence)
{
   JCR *jcr = bfd->jcr;
   Plugin *plugin = (Plugin *)jcr->plugin;
   struct io_pkt io;

   Dsm_check(999);
   Dmsg0(dbglvl, "plugin_bseek\n");
   if (!plugin || !jcr->plugin_ctx) {
      return 0;
   }
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
   Dsm_check(999);
   return (boffset_t)io.offset;
}

/* ==============================================================
 *
 * Callbacks from the plugin
 *
 * ==============================================================
 */
static bRC baculaGetValue(bpContext *ctx, bVariable var, void *value)
{
   JCR *jcr;
   if (!value) {
      return bRC_Error;
   }

   Dsm_check(999);
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
   case bVarBEEF:
      *((int *)value) = beef;
      break;
   default:
      break;
   }

   if (!ctx) {                  /* Other variables need context */
      return bRC_Error;
   }

   jcr = ((bacula_ctx *)ctx->bContext)->jcr;
   if (!jcr) {
      return bRC_Error;
   }

   switch (var) {
   case bVarJobId:
      *((int *)value) = jcr->JobId;
      Dmsg1(dbglvl, "Bacula: return bVarJobId=%d\n", jcr->JobId);
      break;
   case bVarLevel:
      *((int *)value) = jcr->getJobLevel();
      Dmsg1(dbglvl, "Bacula: return bVarJobLevel=%d\n", jcr->getJobLevel());
      break;
   case bVarType:
      *((int *)value) = jcr->getJobType();
      Dmsg1(dbglvl, "Bacula: return bVarJobType=%d\n", jcr->getJobType());
      break;
   case bVarClient:
      *((char **)value) = jcr->client_name;
      Dmsg1(dbglvl, "Bacula: return Client_name=%s\n", jcr->client_name);
      break;
   case bVarJobName:
      *((char **)value) = jcr->Job;
      Dmsg1(dbglvl, "Bacula: return Job name=%s\n", jcr->Job);
      break;
   case bVarPrevJobName:
      *((char **)value) = jcr->PrevJob;
      Dmsg1(dbglvl, "Bacula: return Previous Job name=%s\n", jcr->PrevJob);
      break;
   case bVarJobStatus:
      *((int *)value) = jcr->JobStatus;
      Dmsg1(dbglvl, "Bacula: return bVarJobStatus=%d\n", jcr->JobStatus);
      break;
   case bVarSinceTime:
      *((int *)value) = (int)jcr->mtime;
      Dmsg1(dbglvl, "Bacula: return since=%d\n", (int)jcr->mtime);
      break;
   case bVarAccurate:
      *((int *)value) = (int)jcr->accurate;
      Dmsg1(dbglvl, "Bacula: return accurate=%d\n", (int)jcr->accurate);
      break;
   case bVarFileSeen:
      break;                 /* a write only variable, ignore read request */
   case bVarVssObject:
#ifdef HAVE_WIN32
      if (g_pVSSClient) {
         *(void **)value = g_pVSSClient->GetVssObject();
         break;
       }
#endif
       return bRC_Error;
   case bVarVssDllHandle:
#ifdef HAVE_WIN32
      if (g_pVSSClient) {
         *(void **)value = g_pVSSClient->GetVssDllHandle();
         break;
       }
#endif
       return bRC_Error;
   case bVarWhere:
      *(char **)value = jcr->where;
      break;
   case bVarRegexWhere:
      *(char **)value = jcr->RegexWhere;
      break;
   case bVarPrefixLinks:
      *(int *)value = (int)jcr->prefix_links;
      break;
   case bVarFDName:             /* get warning with g++ if we missed one */
   case bVarWorkingDir:
   case bVarExePath:
   case bVarVersion:
   case bVarDistName:
   case bVarBEEF:
      break;
   }
   Dsm_check(999);
   return bRC_OK;
}

static bRC baculaSetValue(bpContext *ctx, bVariable var, void *value)
{
   JCR *jcr;
   Dsm_check(999);
   if (!value || !ctx) {
      return bRC_Error;
   }
// Dmsg1(dbglvl, "bacula: baculaGetValue var=%d\n", var);
   jcr = ((bacula_ctx *)ctx->bContext)->jcr;
   if (!jcr) {
      return bRC_Error;
   }
// Dmsg1(dbglvl, "Bacula: jcr=%p\n", jcr); 
   switch (var) {
   case bVarFileSeen:
      if (!accurate_mark_file_as_seen(jcr, (char *)value)) {
         return bRC_Error;
      } 
      break;
   default:
      break;
   }
   Dsm_check(999);
   return bRC_OK;
}

static bRC baculaRegisterEvents(bpContext *ctx, ...)
{
   va_list args;
   uint32_t event;

   Dsm_check(999);
   if (!ctx) {
      return bRC_Error;
   }

   va_start(args, ctx);
   while ((event = va_arg(args, uint32_t))) {
      Dmsg1(dbglvl, "Plugin wants event=%u\n", event);
   }
   va_end(args);
   Dsm_check(999);
   return bRC_OK;
}

static bRC baculaJobMsg(bpContext *ctx, const char *file, int line,
  int type, utime_t mtime, const char *fmt, ...)
{
   va_list arg_ptr;
   char buf[2000];
   JCR *jcr;

   Dsm_check(999);
   if (ctx) {
      jcr = ((bacula_ctx *)ctx->bContext)->jcr;
   } else {
      jcr = NULL;
   }

   va_start(arg_ptr, fmt);
   bvsnprintf(buf, sizeof(buf), fmt, arg_ptr);
   va_end(arg_ptr);
   Jmsg(jcr, type, mtime, "%s", buf);
   Dsm_check(999);
   return bRC_OK;
}

static bRC baculaDebugMsg(bpContext *ctx, const char *file, int line,
  int level, const char *fmt, ...)
{
   va_list arg_ptr;
   char buf[2000];

   Dsm_check(999);
   va_start(arg_ptr, fmt);
   bvsnprintf(buf, sizeof(buf), fmt, arg_ptr);
   va_end(arg_ptr);
   d_msg(file, line, level, "%s", buf);
   Dsm_check(999);
   return bRC_OK;
}

static void *baculaMalloc(bpContext *ctx, const char *file, int line,
              size_t size)
{
#ifdef SMARTALLOC
   return sm_malloc(file, line, size);
#else
   return malloc(size);
#endif
}

static void baculaFree(bpContext *ctx, const char *file, int line, void *mem)
{
#ifdef SMARTALLOC
   sm_free(file, line, mem);
#else
   free(mem);
#endif
}

static bool is_ctx_good(bpContext *ctx, JCR *&jcr, bacula_ctx *&bctx)
{
   Dsm_check(999);
   if (!ctx) {
      return false;
   }
   bctx = (bacula_ctx *)ctx->bContext;
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
 * Let the plugin define files/directories to be excluded
 *  from the main backup.
 */
static bRC baculaAddExclude(bpContext *ctx, const char *file)
{
   JCR *jcr;
   findINCEXE *old;
   bacula_ctx *bctx;
   Dsm_check(999);
   if (!is_ctx_good(ctx, jcr, bctx)) {
      return bRC_Error;
   }
   if (!file) {
      return bRC_Error;
   }

   /* Save the include context */
   old = get_incexe(jcr);

   /* Not right time to add exlude */
   if (!old) {
      return bRC_Error;
   }

   if (!bctx->exclude) {  
      bctx->exclude = new_exclude(jcr);
   }

   /* Set the Exclude context */
   set_incexe(jcr, bctx->exclude);

   add_file_to_fileset(jcr, file, true);

   /* Restore the current context */
   set_incexe(jcr, old);

   Dmsg1(100, "Add exclude file=%s\n", file);
   Dsm_check(999);
   return bRC_OK;
}

/**
 * Let the plugin define files/directories to be excluded
 *  from the main backup.
 */
static bRC baculaAddInclude(bpContext *ctx, const char *file)
{
   JCR *jcr;
   findINCEXE *old;
   bacula_ctx *bctx;

   Dsm_check(999);
   if (!is_ctx_good(ctx, jcr, bctx)) {
      return bRC_Error;
   }
   if (!file) {
      return bRC_Error;
   }

   /* Save the include context */
   old = get_incexe(jcr);

   /* Not right time to add include */
   if (!old) {
      return bRC_Error;
   }
   if (!bctx->include) {
      bctx->include = old;
   }

   set_incexe(jcr, bctx->include);
   add_file_to_fileset(jcr, file, true);

   /* Restore the current context */
   set_incexe(jcr, old);

   Dmsg1(100, "Add include file=%s\n", file);
   Dsm_check(999);
   return bRC_OK;
}

static bRC baculaAddOptions(bpContext *ctx, const char *opts)
{
   JCR *jcr;
   bacula_ctx *bctx;
   Dsm_check(999);
   if (!is_ctx_good(ctx, jcr, bctx)) {
      return bRC_Error;
   }
   if (!opts) {
      return bRC_Error;
   }
   add_options_to_fileset(jcr, opts);
   Dsm_check(999);
   Dmsg1(1000, "Add options=%s\n", opts);
   return bRC_OK;
}

static bRC baculaAddRegex(bpContext *ctx, const char *item, int type)
{
   JCR *jcr;
   bacula_ctx *bctx;
   Dsm_check(999);
   if (!is_ctx_good(ctx, jcr, bctx)) {
      return bRC_Error;
   }
   if (!item) {
      return bRC_Error;
   }
   add_regex_to_fileset(jcr, item, type);
   Dmsg1(100, "Add regex=%s\n", item);
   Dsm_check(999);
   return bRC_OK;
}

static bRC baculaAddWild(bpContext *ctx, const char *item, int type)
{
   JCR *jcr;
   bacula_ctx *bctx;
   Dsm_check(999);
   if (!is_ctx_good(ctx, jcr, bctx)) {
      return bRC_Error;
   }
   if (!item) {
      return bRC_Error;
   }
   add_wild_to_fileset(jcr, item, type);
   Dmsg1(100, "Add wild=%s\n", item);
   Dsm_check(999);
   return bRC_OK;
}

static bRC baculaNewOptions(bpContext *ctx)
{
   JCR *jcr;
   bacula_ctx *bctx;
   Dsm_check(999);
   if (!is_ctx_good(ctx, jcr, bctx)) {
      return bRC_Error;
   }
   (void)new_options(jcr, NULL);
   Dsm_check(999);
   return bRC_OK;
}

static bRC baculaNewInclude(bpContext *ctx)
{
   JCR *jcr;
   bacula_ctx *bctx;
   Dsm_check(999);
   if (!is_ctx_good(ctx, jcr, bctx)) {
      return bRC_Error;
   }
   (void)new_include(jcr);
   Dsm_check(999);
   return bRC_OK;
}

static bRC baculaNewPreInclude(bpContext *ctx)
{
   JCR *jcr;
   bacula_ctx *bctx;
   Dsm_check(999);
   if (!is_ctx_good(ctx, jcr, bctx)) {
      return bRC_Error;
   }

   bctx->include = new_preinclude(jcr);
   new_options(jcr, bctx->include);
   set_incexe(jcr, bctx->include);

   Dsm_check(999);
   return bRC_OK;
}

/* 
 * Check if a file have to be backuped using Accurate code
 */
static bRC baculaCheckChanges(bpContext *ctx, struct save_pkt *sp)
{
   JCR *jcr;
   bacula_ctx *bctx;
   FF_PKT *ff_pkt;
   bRC ret = bRC_Error;

   Dsm_check(999);
   if (!is_ctx_good(ctx, jcr, bctx)) {
      goto bail_out;
   }
   if (!sp) {
      goto bail_out;
   }
   
   ff_pkt = jcr->ff;
   /*
    * Copy fname and link because save_file() zaps them.  This 
    *  avoids zaping the plugin's strings.
    */
   ff_pkt->type = sp->type;
   if (!sp->fname) {
      Jmsg0(jcr, M_FATAL, 0, _("Command plugin: no fname in baculaCheckChanges packet.\n"));
      goto bail_out;
   }

   ff_pkt->fname = sp->fname;
   ff_pkt->link = sp->link;
   memcpy(&ff_pkt->statp, &sp->statp, sizeof(ff_pkt->statp));

   if (check_changes(jcr, ff_pkt))  {
      ret = bRC_OK;
   } else {
      ret = bRC_Seen;
   }

   /* check_changes() can update delta sequence number, return it to the
    * plugin 
    */
   sp->delta_seq = ff_pkt->delta_seq;
   sp->accurate_found = ff_pkt->accurate_found;

bail_out:
   Dsm_check(999);
   Dmsg1(100, "checkChanges=%i\n", ret);
   return ret;
}

/* 
 * Check if a file would be saved using current Include/Exclude code
 */
static bRC baculaAcceptFile(bpContext *ctx, struct save_pkt *sp)
{
   JCR *jcr;
   FF_PKT *ff_pkt;
   bacula_ctx *bctx;

   char *old;
   struct stat oldstat;
   bRC ret = bRC_Error;

   Dsm_check(999);
   if (!is_ctx_good(ctx, jcr, bctx)) {
      goto bail_out;
   }
   if (!sp) {
      goto bail_out;
   }
   
   ff_pkt = jcr->ff;

   /* Probably not needed, but keep a copy */
   old = ff_pkt->fname;
   oldstat = ff_pkt->statp;

   ff_pkt->fname = sp->fname;
   ff_pkt->statp = sp->statp;

   if (accept_file(ff_pkt)) {
      ret = bRC_OK;
   } else {
      ret = bRC_Skip;
   }

   ff_pkt->fname = old;
   ff_pkt->statp = oldstat;

bail_out:
   return ret;
}

#ifdef TEST_PROGRAM

int     (*plugin_bopen)(JCR *jcr, const char *fname, int flags, mode_t mode) = NULL;
int     (*plugin_bclose)(JCR *jcr) = NULL;
ssize_t (*plugin_bread)(JCR *jcr, void *buf, size_t count) = NULL;
ssize_t (*plugin_bwrite)(JCR *jcr, void *buf, size_t count) = NULL;
boffset_t (*plugin_blseek)(JCR *jcr, boffset_t offset, int whence) = NULL;

int save_file(JCR *jcr, FF_PKT *ff_pkt, bool top_level)
{
   return 0;
}

bool set_cmd_plugin(BFILE *bfd, JCR *jcr)
{
   return true;
}

int main(int argc, char *argv[])
{
   char plugin_dir[1000];
   JCR mjcr1, mjcr2;
   JCR *jcr1 = &mjcr1;
   JCR *jcr2 = &mjcr2;

   strcpy(my_name, "test-fd");
    
   getcwd(plugin_dir, sizeof(plugin_dir)-1);
   load_fd_plugins(plugin_dir);

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

   unload_plugins();

   Dmsg0(dbglvl, "bacula: OK ...\n");
   close_memory_pool();
   sm_dump(false);     /* unit test */
   return 0;
}

#endif /* TEST_PROGRAM */
