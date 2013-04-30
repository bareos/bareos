/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2010-2011 Bacula Systems(R) SA

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
 *  Written by James Harper, October 2008
 */

#include "exchange-fd.h"

/* Pointers to Bacula functions */
bFuncs *bfuncs = NULL;
bInfo  *binfo = NULL;

#define PLUGIN_LICENSE      "Bacula AGPLv3"
#define PLUGIN_AUTHOR       "James Harper"
#define PLUGIN_DATE    "September 2008"
#define PLUGIN_VERSION      "1"
#define PLUGIN_DESCRIPTION  "Exchange Plugin"

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
static bRC checkFile(bpContext *ctx, char *fname);

static pFuncs pluginFuncs = {
   sizeof(pluginFuncs),
   FD_PLUGIN_INTERFACE_VERSION,

   /* Entry points into plugin */
   newPlugin,          /* new plugin instance */
   freePlugin,         /* free plugin instance */
   getPluginValue,
   setPluginValue,
   handlePluginEvent,
   startBackupFile,
   endBackupFile,
   startRestoreFile,
   endRestoreFile,
   pluginIO,
   createFile,
   setFileAttributes,
   checkFile,
};

extern "C" {

static char **
splitString(char *string, char split, int maxParts, int *count)
{
  char **RetVal;
  char *first;
  char *last;

  //KdPrint((__DRIVER_NAME "     a\n"));

  *count = 0;

  RetVal = (char **)malloc((maxParts + 1) * sizeof(char *));
  last = string;
  do
  {
    if (*count == maxParts)
      break;
    first = last;
    for (last = first; *last != '\0' && *last != split; last++);
    RetVal[*count] = (char *)malloc(last - first + 1);
    strncpy(RetVal[*count], first, last - first);
    RetVal[*count][last - first] = 0;
    (*count)++;
    if (*last == split)
      last++;
  } while (*last != 0);
  RetVal[*count] = NULL;
  return RetVal;
}

bRC DLL_IMP_EXP
loadPlugin(bInfo *lbinfo, bFuncs *lbfuncs, pInfo **pinfo, pFuncs **pfuncs)
{
   bRC retval;
   bfuncs = lbfuncs;        /* set Bacula funct pointers */
   binfo  = lbinfo;
   *pinfo = &pluginInfo;
   *pfuncs = &pluginFuncs;
   retval = loadExchangeApi();
   if (retval != bRC_OK) {
      printf("Cannot load Exchange DLL\n");
      return retval;
   }
   return retval;
}

bRC DLL_IMP_EXP
unloadPlugin()
{
   return bRC_OK;
}

}
/*
void *
b_malloc(const char *file, int lone, size_t size)
{
   return NULL;
}

void *
sm_malloc(const char *file, int lone, size_t size)
{
   return NULL;
}
*/

static bRC newPlugin(bpContext *ctx)
{
   exchange_fd_context_t *context;
   bRC retval = bRC_OK;
   DWORD size;

   int JobId = 0;
   ctx->pContext = new exchange_fd_context_t;
   context = (exchange_fd_context_t *)ctx->pContext;
   context->bpContext = ctx;
   context->job_since = 0;
   context->notrunconfull_option = false;
   context->plugin_active = false;
   bfuncs->getBaculaValue(ctx, bVarJobId, (void *)&JobId);
   _DebugMessage(100, "newPlugin JobId=%d\n", JobId);
   bfuncs->registerBaculaEvents(ctx, 1, 2, 0);
   size = MAX_COMPUTERNAME_LENGTH + 1;
   context->computer_name = new WCHAR[size];
   /*
   GetComputerNameW(context->computer_name, &size);
   */
   GetComputerNameExW(ComputerNameNetBIOS, context->computer_name, &size);
   context->current_node = NULL;
   context->root_node = NULL;
   return retval;
}

static bRC freePlugin(bpContext *ctx)
{
   exchange_fd_context_t *context = (exchange_fd_context_t *)ctx->pContext;
   int JobId = 0;
   bfuncs->getBaculaValue(ctx, bVarJobId, (void *)&JobId);
   _DebugMessage(100, "freePlugin JobId=%d\n", JobId);
   delete context;
   return bRC_OK;
}

static bRC getPluginValue(bpContext *ctx, pVariable var, void *value) 
{
   exchange_fd_context_t *context = (exchange_fd_context_t *)ctx->pContext;
   _DebugMessage(100, "getPluginValue var=%d\n", var);
   return bRC_OK;
}

static bRC setPluginValue(bpContext *ctx, pVariable var, void *value) 
{
   exchange_fd_context_t *context = (exchange_fd_context_t *)ctx->pContext;
   _DebugMessage(100, "setPluginValue var=%d\n", var);
   return bRC_OK;
}

static bRC handlePluginEvent(bpContext *ctx, bEvent *event, void *value)
{
   exchange_fd_context_t *context = (exchange_fd_context_t *)ctx->pContext;
   char *name;
   int i, intval;
   int accurate;
   char *command;
   char *plugin_name;

   switch (event->eventType) {
   case bEventJobStart:
      _DebugMessage(100, "JobStart=%s\n", (char *)value);
      context->plugin_active = false;
      break;
   case bEventJobEnd:
      _DebugMessage(100, "JobEnd\n");
      break;
   case bEventPluginCommand:
      _DebugMessage(100, "bEventPluginCommand %s\n", value);
      command = bstrdup((char *)value);
      /* this isn't really unused */
      plugin_name = strtok((char *)command, ":");
      if (strcmp(plugin_name, "exchange") != 0) {
         context->plugin_active = false;
      } else {
         context->plugin_active = true;
      }
      free(command);
      break;
   case bEventStartBackupJob:
      if (!context->plugin_active) {
         break;
      }
      _DebugMessage(100, "BackupStart\n");
      bfuncs->getBaculaValue(ctx, bVarAccurate, (void *)&accurate);
      context->accurate = accurate;
      context->job_type = JOB_TYPE_BACKUP;
      // level should have been specified by now - check it
      // if level is D or I, since should have been specified too
      switch (context->job_level) {
      case 'F':
         if (context->notrunconfull_option) {
            context->truncate_logs = false;
         } else {
            context->truncate_logs = true;
         }
         break;
      case 'D':
         context->truncate_logs = false;
         break;
      case 'I':
         context->truncate_logs = false;
         break;
      default:
         _DebugMessage(100, "Invalid job level %c\n", context->job_level);
         return bRC_Error;
      }
      break;
   case bEventEndBackupJob:
      _DebugMessage(100, "BackupEnd\n");
      if (!context->plugin_active) {
         break;
      }
      break;
   case bEventLevel:
      /* We don't know if the plugin is active here yet */
      intval = (intptr_t)value;
      _DebugMessage(100, "JobLevel=%c %d\n", intval, intval);
      context->job_level = intval;
      break;
   case bEventSince:
      /* We don't know if the plugin is active here yet */
      intval = (intptr_t)value;
      _DebugMessage(100, "since=%d\n", intval);
      context->job_since = (time_t)value;
      break;
   case bEventStartRestoreJob:
      _DebugMessage(100, "StartRestoreJob\n");
      context->job_type = JOB_TYPE_RESTORE;
      context->plugin_active = true;
      break;
   case bEventEndRestoreJob:
      if (!context->plugin_active) {
         break;
      }
      _DebugMessage(100, "EndRestoreJob\n");
      context->plugin_active = false;
      break;
   
   /* Plugin command e.g. plugin = <plugin-name>:<name-space>:command */
   case bEventRestoreCommand:
      _DebugMessage(100, "restore\n"); // command=%s\n", (char *)value);
      if (!context->plugin_active) {
         break;
      }
      break;

   case bEventBackupCommand:
      if (!context->plugin_active) {
         break;
      }
      {
      _DebugMessage(100, "backup command=%s\n", (char *)value);    
      char *command = new char[strlen((char *)value) + 1];
      strcpy(command, (char *)value);
      char *plugin_name = strtok((char *)command, ":");
      char *path = strtok(NULL, ":");
      char *option;
      while ((option = strtok(NULL, ":")) != NULL) {
         _DebugMessage(100, "option %s\n", option);
         if (stricmp(option, "notrunconfull") == 0) {
            context->notrunconfull_option = true;
         } else {
            _JobMessage(M_WARNING, "Unknown plugin option '%s'\n", option);
         }
      }
      _DebugMessage(100, "name = %s\n", plugin_name);
      _DebugMessage(100, "path = %s\n", path);
      if (*path != '/') {
         _JobMessage(M_FATAL, "Path does not begin with a '/'\n");
         return bRC_Error;
      }

      for (i = 0; i < 6; i++) {
         context->path_bits[i] = NULL;
      }

      char *path_bit = strtok(path, "/");
      for (i = 0; path_bit != NULL && i < 6; i++) {
         context->path_bits[i] = new char[strlen(path_bit) + 1];
         strcpy(context->path_bits[i], path_bit);
         path_bit = strtok(NULL, "/");
      }

      if (i < 2 || i > 4) {
         _JobMessage(M_FATAL, "Invalid plugin backup path\n");
         return bRC_Error;
      }
      context->root_node = new root_node_t(context->path_bits[0]);
      context->current_node = context->root_node;

      }
      break;

   default:
      _DebugMessage(100, "Ignored event=%d\n", event->eventType);
      break;
   }
   bfuncs->getBaculaValue(ctx, bVarFDName, (void *)&name);
   return bRC_OK;
}

static bRC
startBackupFile(bpContext *ctx, struct save_pkt *sp)
{
   bRC retval;
   exchange_fd_context_t *context = (exchange_fd_context_t *)ctx->pContext;
   node_t *current_node;

   _DebugMessage(100, "startBackupFile, cmd = %s\n", sp->cmd);
   if (sp->pkt_size != sizeof(struct save_pkt) || sp->pkt_end != sizeof(struct save_pkt)) {
      _JobMessage(M_FATAL, "save_pkt size mismatch - sizeof(struct save_pkt) = %d, pkt_size = %d, pkt_end = %d\n", sizeof(struct save_pkt), sp->pkt_size, sp->pkt_end);
      return bRC_Error;
   }

   //context->root_node = new root_node_t(PLUGIN_PATH_PREFIX_BASE);
   //context->current_node = context->root_node;
   do {
      current_node  = context->current_node;
      retval = current_node->startBackupFile(context, sp);
      if (retval == bRC_Seen)
         endBackupFile(ctx);
   } while (current_node != context->current_node);
   _DebugMessage(100, "startBackupFile done - type = %d, fname = %s, retval = %d\n", sp->type, sp->fname, retval);
   return retval;
}

static bRC endBackupFile(bpContext *ctx)
{ 
   bRC retval;
   exchange_fd_context_t *context = (exchange_fd_context_t *)ctx->pContext;
   node_t *current_node;

   _DebugMessage(100, "endBackupFile\n");

   do {
      current_node  = context->current_node;
      retval = current_node->endBackupFile(context);
   } while (current_node != context->current_node);
   _DebugMessage(100, "endBackupFile done - retval = %d\n", retval);
   return retval;
}

/*
 * Do actual I/O
 */
static bRC pluginIO(bpContext *ctx, struct io_pkt *io)
{
   bRC retval = bRC_OK;
   exchange_fd_context_t *context = (exchange_fd_context_t *)ctx->pContext;

   if (io->pkt_size != sizeof(struct io_pkt) || io->pkt_end != sizeof(struct io_pkt))
   {
      _JobMessage(M_FATAL, "io_pkt size mismatch - sizeof(struct io_pkt) = %d, pkt_size = %d, pkt_end = %d\n", sizeof(struct io_pkt), io->pkt_size, io->pkt_end);
   }

   switch(io->func) {
   case IO_OPEN:
      _DebugMessage(100, "IO_OPEN\n");
      retval = context->current_node->pluginIoOpen(context, io);
      break;
   case IO_READ:
      //_DebugMessage(100, "IO_READ buf=%p len=%d\n", io->buf, io->count);
      retval = context->current_node->pluginIoRead(context, io);
      break;
   case IO_WRITE:
      //_DebugMessage(100, "IO_WRITE buf=%p len=%d\n", io->buf, io->count);
      retval = context->current_node->pluginIoWrite(context, io);
      break;
   case IO_CLOSE:
      _DebugMessage(100, "IO_CLOSE\n");
      retval = context->current_node->pluginIoClose(context, io);
      break;
   }
   return retval;
}

static bRC startRestoreFile(bpContext *ctx, const char *cmd)
{
   exchange_fd_context_t *context = (exchange_fd_context_t *)ctx->pContext;
   _DebugMessage(100, "startRestoreFile\n");

   return bRC_OK;
}

static bRC endRestoreFile(bpContext *ctx)
{
   bRC retval;
   exchange_fd_context_t *context = (exchange_fd_context_t *)ctx->pContext;
   node_t *current_node;

   _DebugMessage(100, "endRestoreFile\n");

   do {
      current_node  = context->current_node;
      retval = current_node->endRestoreFile(context);
   } while (current_node != context->current_node);
   _DebugMessage(100, "endRestoreFile done - retval = %d\n", retval);
   return retval;
}

static bRC createFile(bpContext *ctx, struct restore_pkt *rp)
{
   bRC retval;
   exchange_fd_context_t *context = (exchange_fd_context_t *)ctx->pContext;
   node_t *current_node;
   char **path_bits;
   int count;
   int i;


   _DebugMessage(100, "createFile - type = %d, ofname = %s\n", rp->type, rp->ofname);
   if (rp->pkt_size != sizeof(struct restore_pkt) || rp->pkt_end != sizeof(struct restore_pkt))
   {
      _JobMessage(M_FATAL, "restore_pkt size mismatch - sizeof(struct restore_pkt) = %d, pkt_size = %d, pkt_end = %d\n", sizeof(struct restore_pkt), rp->pkt_size, rp->pkt_end);
      return bRC_Error;
   }

   for (i = 0; i < 6; i++)
   {
      context->path_bits[i] = NULL;
   }

   path_bits = splitString((char *)rp->ofname, '/', 7, &count);

   _DebugMessage(100, "count = %d\n", count);

   for (i = 1; i < count; i++)
   {
      _DebugMessage(150, "%d = '%s'\n", i, path_bits[i]);
      context->path_bits[i - 1] = path_bits[i];
   }

   if (context->current_node == NULL)
   {
      context->root_node = new root_node_t(context->path_bits[0]);
      context->current_node = context->root_node;
   }

   do {
      current_node  = context->current_node;
      retval = current_node->createFile(context, rp);
   } while (current_node != context->current_node);
   _DebugMessage(100, "createFile done - retval = %d\n", retval);
   return retval;
}

static bRC setFileAttributes(bpContext *ctx, struct restore_pkt *rp)
{
   exchange_fd_context_t *context = (exchange_fd_context_t *)ctx->pContext;
   _DebugMessage(100, "setFileAttributes\n");
   return bRC_OK;
}

static bRC checkFile(bpContext *ctx, char *fname)
{
   exchange_fd_context_t *context = (exchange_fd_context_t *)ctx->pContext;
   if (context->plugin_active) {
      _DebugMessage(100, "marked as seen %s\n", fname);
      return bRC_Seen;
   }
   return bRC_OK;
}
