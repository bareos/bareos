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
 * A simple pipe plugin for the Bacula File Daemon
 *
 *  Kern Sibbald, October 2007
 *    Copied into Windows plugin environment March, 2010 (KES)
 *
 */
#include "bacula.h"
#include "fd_plugins.h"

#undef malloc
#undef free
#undef strdup

#define fi __FILE__
#define li __LINE__

#ifdef __cplusplus
extern "C" {
#endif

static const int dbglvl = 150;

#define PLUGIN_LICENSE      "Bacula AGPLv3"
#define PLUGIN_AUTHOR       "Kern Sibbald"
#define PLUGIN_DATE         "January 2010"
#define PLUGIN_VERSION      "1"
#define PLUGIN_DESCRIPTION  "Bacula Pipe Windows File Daemon Plugin"

/* Forward referenced functions */
static bRC newPlugin(bpContext *ctx);
static bRC freePlugin(bpContext *ctx);
static bRC getPluginValue(bpContext *ctx, pVariable var, void *value);
static bRC setPluginValue(bpContext *ctx, pVariable var, void *value);
static bRC handlePluginEvent(bpContext *ctx, bEvent *event, void *value);
static bRC startBackupFile(bpContext *ctx, struct save_pkt *sp);
static bRC endBackupFile(bpContext *ctx);
static bRC pluginIO(bpContext *ctx, struct io_pkt *io);
static bRC startRestoreFile(bpContext *ctx, const char *cmd);
static bRC endRestoreFile(bpContext *ctx);
static bRC createFile(bpContext *ctx, struct restore_pkt *rp);
static bRC setFileAttributes(bpContext *ctx, struct restore_pkt *rp);

static char *apply_rp_codes(struct plugin_ctx * p_ctx);

/* Pointers to Bacula functions */
static bFuncs *bfuncs = NULL;
static bInfo  *binfo = NULL;

/* Plugin Information block */
static pInfo pluginInfo = {
   sizeof(pluginInfo),
   FD_PLUGIN_INTERFACE_VERSION,
   FD_PLUGIN_MAGIC,
   PLUGIN_LICENSE,
   PLUGIN_AUTHOR,
   PLUGIN_DATE,
   PLUGIN_VERSION,
   PLUGIN_DESCRIPTION,
};

/* Plugin entry points for Bacula */
static pFuncs pluginFuncs = {
   sizeof(pluginFuncs),
   FD_PLUGIN_INTERFACE_VERSION,

   /* Entry points into plugin */
   newPlugin,                         /* new plugin instance */
   freePlugin,                        /* free plugin instance */
   getPluginValue,
   setPluginValue,
   handlePluginEvent,
   startBackupFile,
   endBackupFile,
   startRestoreFile,
   endRestoreFile,
   pluginIO,
   createFile,
   setFileAttributes
};

/*
 * Plugin private context
 */
struct plugin_ctx {
   boffset_t offset;
   FILE *fd;                          /* pipe file descriptor */
   bool backup;                       /* set for backup (not needed) */
   char *cmd;                         /* plugin command line */
   char *fname;                       /* filename to "backup/restore" */
   char *reader;                      /* reader program for backup */
   char *writer;                      /* writer program for backup */

   char where[512];
   int replace;
};

/*
 * loadPlugin() and unloadPlugin() are entry points that are
 *  exported, so Bacula can directly call these two entry points
 *  they are common to all Bacula plugins.
 */
/*
 * External entry point called by Bacula to "load the plugin
 */
bRC loadPlugin(bInfo *lbinfo, bFuncs *lbfuncs, pInfo **pinfo, pFuncs **pfuncs)
{
   bfuncs = lbfuncs;                  /* set Bacula funct pointers */
   binfo  = lbinfo;
   *pinfo  = &pluginInfo;             /* return pointer to our info */
   *pfuncs = &pluginFuncs;            /* return pointer to our functions */

   return bRC_OK;
}

/*
 * External entry point to unload the plugin 
 */
bRC unloadPlugin() 
{
// printf("bpipe-fd: Unloaded\n");
   return bRC_OK;
}

/*
 * The following entry points are accessed through the function 
 *   pointers we supplied to Bacula. Each plugin type (dir, fd, sd)
 *   has its own set of entry points that the plugin must define.
 */
/*
 * Create a new instance of the plugin i.e. allocate our private storage
 */
static bRC newPlugin(bpContext *ctx)
{
   struct plugin_ctx *p_ctx = (struct plugin_ctx *)malloc(sizeof(struct plugin_ctx));
   if (!p_ctx) {
      return bRC_Error;
   }
   memset(p_ctx, 0, sizeof(struct plugin_ctx));
   ctx->pContext = (void *)p_ctx;        /* set our context pointer */
   return bRC_OK;
}

/*
 * Free a plugin instance, i.e. release our private storage
 */
static bRC freePlugin(bpContext *ctx)
{
   struct plugin_ctx *p_ctx = (struct plugin_ctx *)ctx->pContext;
   if (!p_ctx) {
      return bRC_Error;
   }
   if (p_ctx->cmd) {
      free(p_ctx->cmd);                  /* free any allocated command string */
   }
   free(p_ctx);                          /* free our private context */
   p_ctx = NULL;
   return bRC_OK;
}

/*
 * Return some plugin value (none defined)
 */
static bRC getPluginValue(bpContext *ctx, pVariable var, void *value) 
{
   return bRC_OK;
}

/*
 * Set a plugin value (none defined)
 */
static bRC setPluginValue(bpContext *ctx, pVariable var, void *value) 
{
   return bRC_OK;
}

/*
 * Handle an event that was generated in Bacula
 */
static bRC handlePluginEvent(bpContext *ctx, bEvent *event, void *value)
{
   struct plugin_ctx *p_ctx = (struct plugin_ctx *)ctx->pContext;
   if (!p_ctx) {
      return bRC_Error;
   }

// char *name;

   /*
    * Most events don't interest us so we ignore them.
    *   the printfs are so that plugin writers can enable them to see
    *   what is really going on.
    */
   switch (event->eventType) {
   case bEventJobStart:
      bfuncs->DebugMessage(ctx, fi, li, dbglvl, "bpipe-fd: JobStart=%s\n", (char *)value);
      break;
   case bEventJobEnd:
//    printf("bpipe-fd: JobEnd\n");
      break;
   case bEventStartBackupJob:
//    printf("bpipe-fd: StartBackupJob\n");
      break;
   case bEventEndBackupJob:
//    printf("bpipe-fd: EndBackupJob\n");
      break;
   case bEventLevel:
//    printf("bpipe-fd: JobLevel=%c %d\n", (int)value, (int)value);
      break;
   case bEventSince:
//    printf("bpipe-fd: since=%d\n", (int)value);
      break;

   case bEventStartRestoreJob:
//    printf("bpipe-fd: StartRestoreJob\n");
      break;

   case bEventEndRestoreJob:
//    printf("bpipe-fd: EndRestoreJob\n");
      break;

   /* Plugin command e.g. plugin = <plugin-name>:<name-space>:read command:write command */
   case bEventRestoreCommand:
//    printf("bpipe-fd: EventRestoreCommand cmd=%s\n", (char *)value);
      /* Fall-through wanted */
   case bEventBackupCommand:
      char *p;
      bfuncs->DebugMessage(ctx, fi, li, dbglvl, "bpipe-fd: pluginEvent cmd=%s\n", (char *)value);
      p_ctx->cmd = strdup((char *)value);
      p = strchr(p_ctx->cmd, ':');
      if (!p) {
         bfuncs->JobMessage(ctx, fi, li, M_FATAL, 0, "Plugin terminator not found: %s\n", (char *)value);
         return bRC_Error;
      }
      *p++ = 0;           /* terminate plugin */
      p_ctx->fname = p;
      p = strchr(p, ':');
      if (!p) {
         bfuncs->JobMessage(ctx, fi, li, M_FATAL, 0, "File terminator not found: %s\n", (char *)value);
         return bRC_Error;
      }
      *p++ = 0;           /* terminate file */
      p_ctx->reader = p;
      p = strchr(p, ':');
      if (!p) {
         bfuncs->JobMessage(ctx, fi, li, M_FATAL, 0, "Reader terminator not found: %s\n", (char *)value);
         return bRC_Error;
      }
      *p++ = 0;           /* terminate reader string */
      p_ctx->writer = p;
//    printf("bpipe-fd: plugin=%s fname=%s reader=%s writer=%s\n", 
//         p_ctx->cmd, p_ctx->fname, p_ctx->reader, p_ctx->writer);
      break;

   default:
//    printf("bpipe-fd: unknown event=%d\n", event->eventType);
      break;
   }
   return bRC_OK;
}

/* 
 * Start the backup of a specific file
 */
static bRC startBackupFile(bpContext *ctx, struct save_pkt *sp)
{
   struct plugin_ctx *p_ctx = (struct plugin_ctx *)ctx->pContext;
   if (!p_ctx) {
      return bRC_Error;
   }
   time_t now = time(NULL);
   sp->fname = p_ctx->fname;
   sp->type = FT_REG;
   sp->statp.st_mode = 0700 | S_IFREG;
   sp->statp.st_ctime = now;
   sp->statp.st_mtime = now;
   sp->statp.st_atime = now;
   sp->statp.st_size = 0;
   sp->statp.st_blksize = 4096;
   sp->statp.st_blocks = 1;
   p_ctx->backup = true;
// printf("bpipe-fd: startBackupFile\n");
   return bRC_OK;
}

/*
 * Done with backup of this file
 */
static bRC endBackupFile(bpContext *ctx)
{
   /*
    * We would return bRC_More if we wanted startBackupFile to be
    * called again to backup another file
    */
   return bRC_OK;
}


/*
 * Bacula is calling us to do the actual I/O
 */
static bRC pluginIO(bpContext *ctx, struct io_pkt *io)
{
   struct plugin_ctx *p_ctx = (struct plugin_ctx *)ctx->pContext;
   if (!p_ctx) {
      return bRC_Error;
   }
    
   io->status = 0;
   io->io_errno = 0;
   switch(io->func) {
   case IO_OPEN:
      bfuncs->DebugMessage(ctx, fi, li, dbglvl, "bpipe-fd: IO_OPEN\n");
      if (io->flags & (O_CREAT | O_WRONLY)) {
         char *writer_codes = apply_rp_codes(p_ctx);

         p_ctx->fd = popen(writer_codes, "w");
         bfuncs->DebugMessage(ctx, fi, li, dbglvl, "bpipe-fd: IO_OPEN fd=%d writer=%s\n", 
             p_ctx->fd, writer_codes);
         if (!p_ctx->fd) {
            io->io_errno = errno;
            bfuncs->JobMessage(ctx, fi, li, M_FATAL, 0, 
               "Open pipe writer=%s failed: ERR=%s\n", writer_codes, strerror(errno));
            if (writer_codes) {
               free(writer_codes);
            }
            return bRC_Error;
         }
         if (writer_codes) {
            free(writer_codes);
         }
      } else {
         p_ctx->fd = popen(p_ctx->reader, "r");
         bfuncs->DebugMessage(ctx, fi, li, dbglvl, "bpipe-fd: IO_OPEN fd=%p reader=%s\n", 
            p_ctx->fd, p_ctx->reader);
         if (!p_ctx->fd) {
            io->io_errno = errno;
            bfuncs->JobMessage(ctx, fi, li, M_FATAL, 0, 
               "Open pipe reader=%s failed: ERR=%s\n", p_ctx->reader, strerror(errno));
            return bRC_Error;
         }
      }
      bmicrosleep(1,0);         /* let pipe connect */
      break;

   case IO_READ:
      if (!p_ctx->fd) {
         bfuncs->JobMessage(ctx, fi, li, M_FATAL, 0, "Logic error: NULL read FD\n");
         return bRC_Error;
      }
      io->status = fread(io->buf, 1, io->count, p_ctx->fd);
//    bfuncs->DebugMessage(ctx, fi, li, dbglvl, "bpipe-fd: IO_READ buf=%p len=%d\n", io->buf, io->status);
      if (io->status == 0 && ferror(p_ctx->fd)) {
         bfuncs->JobMessage(ctx, fi, li, M_FATAL, 0, 
            "Pipe read error: ERR=%s\n", strerror(errno));
         bfuncs->DebugMessage(ctx, fi, li, dbglvl, 
            "Pipe read error: ERR=%s\n", strerror(errno));
         return bRC_Error;
      }
      break;

   case IO_WRITE:
      if (!p_ctx->fd) {
         bfuncs->JobMessage(ctx, fi, li, M_FATAL, 0, "Logic error: NULL write FD\n");
         return bRC_Error;
      }
//    printf("bpipe-fd: IO_WRITE fd=%p buf=%p len=%d\n", p_ctx->fd, io->buf, io->count);
      io->status = fwrite(io->buf, 1, io->count, p_ctx->fd);
//    printf("bpipe-fd: IO_WRITE buf=%p len=%d\n", io->buf, io->status);
      if (io->status == 0 && ferror(p_ctx->fd)) {
         bfuncs->JobMessage(ctx, fi, li, M_FATAL, 0, 
            "Pipe write error\n");
         bfuncs->DebugMessage(ctx, fi, li, dbglvl, 
            "Pipe read error: ERR=%s\n", strerror(errno));
         return bRC_Error;
      }
      break;

   case IO_CLOSE:
      if (!p_ctx->fd) {
         bfuncs->JobMessage(ctx, fi, li, M_FATAL, 0, "Logic error: NULL FD on bpipe close\n");
         return bRC_Error;
      }
      io->status = pclose(p_ctx->fd);
      break;

   case IO_SEEK:
      io->offset = p_ctx->offset;
      break;
   }
   return bRC_OK;
}

/*
 * Bacula is notifying us that a plugin name string was found, and
 *   passing us the plugin command, so we can prepare for a restore.
 */
static bRC startRestoreFile(bpContext *ctx, const char *cmd)
{
// printf("bpipe-fd: startRestoreFile cmd=%s\n", cmd);
   return bRC_OK;
}

/*
 * Bacula is notifying us that the plugin data has terminated, so
 *  the restore for this particular file is done.
 */
static bRC endRestoreFile(bpContext *ctx)
{
// printf("bpipe-fd: endRestoreFile\n");
   return bRC_OK;
}

/*
 * This is called during restore to create the file (if necessary)
 * We must return in rp->create_status:
 *   
 *  CF_ERROR    -- error
 *  CF_SKIP     -- skip processing this file
 *  CF_EXTRACT  -- extract the file (i.e.call i/o routines)
 *  CF_CREATED  -- created, but no content to extract (typically directories)
 *
 */
static bRC createFile(bpContext *ctx, struct restore_pkt *rp)
{
// printf("bpipe-fd: createFile\n");
   if (strlen(rp->where) > 512) {
      printf("Restore target dir too long. Restricting to first 512 bytes.\n");
   }
   strncpy(((struct plugin_ctx *)ctx->pContext)->where, rp->where, 513);
   ((struct plugin_ctx *)ctx->pContext)->replace = rp->replace;
   rp->create_status = CF_EXTRACT;
   return bRC_OK;
}

/*
 * We will get here if the File is a directory after everything
 * is written in the directory.
 */
static bRC setFileAttributes(bpContext *ctx, struct restore_pkt *rp)
{
// printf("bpipe-fd: setFileAttributes\n");
   return bRC_OK;
}

/*************************************************************************
 * Apply codes in writer command:
 * %w -> "where"
 * %r -> "replace"
 *
 * Replace:
 * 'always' => 'a', chr(97)
 * 'ifnewer' => 'w', chr(119)
 * 'ifolder' => 'o', chr(111)
 * 'never' => 'n', chr(110)
 *
 * This function will allocate the required amount of memory with malloc.
 * Need to be free()d manually.
 * Inspired by edit_job_codes in lib/util.c
 */

static char *apply_rp_codes(struct plugin_ctx * p_ctx)
{
   char *p, *q;
   const char *str;
   char add[10];
   int w_count = 0, r_count = 0;
   char *omsg;

   char *imsg = p_ctx->writer;

   if (!imsg) {
      return NULL;
   }

   if ((p = imsg)) {
      while ((q = strstr(p, "%w"))) {
         w_count++;
         p=q+1;
      }

      p = imsg;
      while ((q = strstr(p, "%r"))) {
         r_count++;
         p=q+1;
      }
   }

   /* Required mem: 
    * len(imsg) 
    * + number of "where" codes * (len(where)-2) 
    * - number of "replace" codes
    */
   omsg = (char*)malloc(strlen(imsg) + (w_count * (strlen(p_ctx->where)-2)) - r_count + 1);
   if (!omsg) {
      fprintf(stderr, "Out of memory.");
      return NULL;
   }

   *omsg = 0;
   //printf("apply_rp_codes: %s\n", imsg);
   for (p=imsg; *p; p++) {
      if (*p == '%') {
         switch (*++p) {
         case '%':
            str = "%";
            break;
         case 'w':
             str = p_ctx->where;
             break;
         case 'r':
            snprintf(add, 2, "%c", p_ctx->replace);
            str = add;
            break;
         default:
            add[0] = '%';
            add[1] = *p;
            add[2] = 0;
            str = add;
            break;
         }
      } else {
         add[0] = *p;
         add[1] = 0;
         str = add;
      }
      //printf("add_str %s\n", str);
      strcat(omsg, str);
      //printf("omsg=%s\n", omsg);
   }
   return omsg;
}


#ifdef __cplusplus
}
#endif
