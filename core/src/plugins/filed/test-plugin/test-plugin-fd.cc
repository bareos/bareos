/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2007-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2016-2020 Bareos GmbH & Co. KG

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
 * A simple test plugin for the Bareos File Daemon derived from
 * the bpipe plugin, but used for testing new features.
 */
#include "include/bareos.h"
#include "filed/fd_plugins.h"
#include "plugins/include/common.h"
#include "lib/ini.h"
#include <wchar.h>

namespace filedaemon {

static const int debuglevel = 000;

#define PLUGIN_LICENSE "Bareos AGPLv3"
#define PLUGIN_AUTHOR "Kern Sibbald"
#define PLUGIN_DATE "May 2011"
#define PLUGIN_VERSION "3"
#define PLUGIN_DESCRIPTION "Bareos Test File Daemon Plugin"

/* Forward referenced functions */
static bRC newPlugin(PluginContext* ctx);
static bRC freePlugin(PluginContext* ctx);
static bRC getPluginValue(PluginContext* ctx, pVariable var, void* value);
static bRC setPluginValue(PluginContext* ctx, pVariable var, void* value);
static bRC handlePluginEvent(PluginContext* ctx, bEvent* event, void* value);
static bRC startBackupFile(PluginContext* ctx, struct save_pkt* sp);
static bRC endBackupFile(PluginContext* ctx);
static bRC pluginIO(PluginContext* ctx, struct io_pkt* io);
static bRC startRestoreFile(PluginContext* ctx, const char* cmd);
static bRC endRestoreFile(PluginContext* ctx);
static bRC createFile(PluginContext* ctx, struct restore_pkt* rp);
static bRC setFileAttributes(PluginContext* ctx, struct restore_pkt* rp);
static bRC checkFile(PluginContext* ctx, char* fname);
static bRC getAcl(PluginContext* ctx, acl_pkt* ap);
static bRC setAcl(PluginContext* ctx, acl_pkt* ap);
static bRC getXattr(PluginContext* ctx, xattr_pkt* xp);
static bRC setXattr(PluginContext* ctx, xattr_pkt* xp);

/* Pointers to Bareos functions */
static CoreFunctions* bareos_core_functions = NULL;
static PluginApiDefinition* bareos_plugin_interface_version = NULL;

/* Plugin Information block */
static PluginInformation pluginInfo
    = {sizeof(pluginInfo), FD_PLUGIN_INTERFACE_VERSION,
       FD_PLUGIN_MAGIC,    PLUGIN_LICENSE,
       PLUGIN_AUTHOR,      PLUGIN_DATE,
       PLUGIN_VERSION,     PLUGIN_DESCRIPTION};

/* Plugin entry points for Bareos */
static PluginFunctions pluginFuncs
    = {sizeof(pluginFuncs), FD_PLUGIN_INTERFACE_VERSION,

       /* Entry points into plugin */
       newPlugin,  /* new plugin instance */
       freePlugin, /* free plugin instance */
       getPluginValue, setPluginValue, handlePluginEvent, startBackupFile,
       endBackupFile, startRestoreFile, endRestoreFile, pluginIO, createFile,
       setFileAttributes, checkFile, getAcl, setAcl, getXattr, setXattr};

static struct ini_items test_items[] = {
    // name type comment required
    {"string1", INI_CFG_TYPE_STR, "Special String", 1},
    {"string2", INI_CFG_TYPE_STR, "2nd String", 0},
    {"ok", INI_CFG_TYPE_BOOL, "boolean", 0},
    // We can also use the ITEMS_DEFAULT
    // { "ok", INI_CFG_TYPE_BOOL, "boolean", 0, ITEMS_DEFAULT },
    {NULL, 0, NULL, 0}};

// Plugin private context
struct plugin_ctx {
  boffset_t offset;
  FILE* fd;     /* pipe file descriptor */
  char* cmd;    /* plugin command line */
  char* fname;  /* filename to "backup/restore" */
  char* reader; /* reader program for backup */
  char* writer; /* writer program for backup */

  char where[512];
  int replace;

  int nb_obj;   /* Number of objects created */
  POOLMEM* buf; /* store ConfigFile */
};

#ifdef __cplusplus
extern "C" {
#endif

/**
 * loadPlugin() and unloadPlugin() are entry points that are
 *  exported, so Bareos can directly call these two entry points
 *  they are common to all Bareos plugins.
 */
// External entry point called by Bareos to "load" the plugin
bRC loadPlugin(PluginApiDefinition* lbareos_plugin_interface_version,
               CoreFunctions* lbareos_core_functions,
               PluginInformation** plugin_information,
               PluginFunctions** plugin_functions)
{
  bareos_core_functions
      = lbareos_core_functions; /* set Bareos funct pointers */
  bareos_plugin_interface_version = lbareos_plugin_interface_version;
  *plugin_information = &pluginInfo; /* return pointer to our info */
  *plugin_functions = &pluginFuncs;  /* return pointer to our functions */

  return bRC_OK;
}

// External entry point to unload the plugin
bRC unloadPlugin() { return bRC_OK; }

#ifdef __cplusplus
}
#endif

/**
 * The following entry points are accessed through the function
 *   pointers we supplied to Bareos. Each plugin type (dir, fd, sd)
 *   has its own set of entry points that the plugin must define.
 */
// Create a new instance of the plugin i.e. allocate our private storage
static bRC newPlugin(PluginContext* ctx)
{
  struct plugin_ctx* p_ctx
      = (struct plugin_ctx*)malloc(sizeof(struct plugin_ctx));
  if (!p_ctx) { return bRC_Error; }
  p_ctx = (plugin_ctx*)memset(p_ctx, 0, sizeof(struct plugin_ctx));
  ctx->plugin_private_context = (void*)p_ctx; /* set our context pointer */

  bareos_core_functions->registerBareosEvents(
      ctx, 5, bEventJobStart, bEventEndFileSet, bEventRestoreObject,
      bEventEstimateCommand, bEventBackupCommand);

  return bRC_OK;
}

// Free a plugin instance, i.e. release our private storage
static bRC freePlugin(PluginContext* ctx)
{
  struct plugin_ctx* p_ctx = (struct plugin_ctx*)ctx->plugin_private_context;
  if (!p_ctx) { return bRC_Error; }
  if (p_ctx->buf) { FreePoolMemory(p_ctx->buf); }
  if (p_ctx->cmd) { free(p_ctx->cmd); /* free any allocated command string */ }
  free(p_ctx); /* free our private context */
  ctx->plugin_private_context = NULL;
  return bRC_OK;
}

// Return some plugin value (none defined)
static bRC getPluginValue(PluginContext* ctx, pVariable var, void* value)
{
  return bRC_OK;
}

// Set a plugin value (none defined)
static bRC setPluginValue(PluginContext* ctx, pVariable var, void* value)
{
  return bRC_OK;
}

// Handle an event that was generated in Bareos
static bRC handlePluginEvent(PluginContext* ctx, bEvent* event, void* value)
{
  struct plugin_ctx* p_ctx = (struct plugin_ctx*)ctx->plugin_private_context;
  restore_object_pkt* rop;
  if (!p_ctx) { return bRC_Error; }

  // char *name;

  switch (event->eventType) {
    case bEventJobStart:
      Dmsg(ctx, debuglevel, "test-plugin-fd: JobStart=%s\n", (char*)value);
      break;
    case bEventEndFileSet:
      // End of Dir FileSet commands, now we can add excludes
      bareos_core_functions->NewOptions(ctx);
      bareos_core_functions->AddWild(ctx, "*.c", ' ');
      bareos_core_functions->AddWild(ctx, "*.cpp", ' ');
      bareos_core_functions->AddOptions(ctx, "ei"); /* exclude, ignore case */
      bareos_core_functions->AddExclude(ctx,
                                        "/home/user/bareos/regress/README");
      break;
    case bEventRestoreObject: {
      FILE* fp;
      POOLMEM* q;
      char* working;
      static int _nb = 0;

      printf("Plugin RestoreObject\n");
      if (!value) {
        Dmsg(ctx, debuglevel, "test-plugin-fd: End restore objects\n");
        break;
      }
      rop = (restore_object_pkt*)value;
      Dmsg(ctx, debuglevel,
           "Get RestoreObject len=%d JobId=%d oname=%s type=%d data=%.127s\n",
           rop->object_len, rop->JobId, rop->object_name, rop->object_type,
           rop->object);
      q = GetPoolMemory(PM_FNAME);

      bareos_core_functions->getBareosValue(ctx, bVarWorkingDir, &working);
      Mmsg(q, "%s/restore.%d", working, _nb++);
      if ((fp = fopen(q, "w")) != NULL) {
        fwrite(rop->object, rop->object_len, 1, fp);
        fclose(fp);
      }

      FreePoolMemory(q);

      if (!strcmp(rop->object_name, INI_RESTORE_OBJECT_NAME)) {
        ConfigFile ini;
        if (!ini.DumpString(rop->object, rop->object_len)) { break; }
        ini.RegisterItems(test_items, sizeof(struct ini_items));
        if (ini.parse(ini.out_fname)) {
          Jmsg(ctx, M_INFO, "string1 = %s\n", ini.items[0].val.strval);
        } else {
          Jmsg(ctx, M_ERROR, "Can't parse config\n");
        }
      }

      break;
    }
    case bEventEstimateCommand:
      /* Fall-through wanted */
    case bEventBackupCommand: {
      /*
       * Plugin command e.g. plugin = <plugin-name>:<name-space>:read
       * command:write command
       */
      char* p;

      Dmsg(ctx, debuglevel, "test-plugin-fd: pluginEvent cmd=%s\n",
           (char*)value);
      p_ctx->cmd = strdup((char*)value);
      p = strchr(p_ctx->cmd, ':');
      if (!p) {
        Jmsg(ctx, M_FATAL, "Plugin terminator not found: %s\n", (char*)value);
        Dmsg(ctx, debuglevel, "Plugin terminator not found: %s\n",
             (char*)value);
        return bRC_Error;
      }
      *p++ = 0; /* Terminate plugin */
      p_ctx->fname = p;
      p = strchr(p, ':');
      if (!p) {
        Jmsg(ctx, M_FATAL, "File terminator not found: %s\n", (char*)value);
        Dmsg(ctx, debuglevel, "File terminator not found: %s\n", (char*)value);
        return bRC_Error;
      }
      *p++ = 0; /* Terminate file */
      p_ctx->reader = p;
      p = strchr(p, ':');
      if (!p) {
        Jmsg(ctx, M_FATAL, "Reader terminator not found: %s\n", (char*)value);
        Dmsg(ctx, debuglevel, "Reader terminator not found: %s\n",
             (char*)value);
        return bRC_Error;
      }
      *p++ = 0; /* Terminate reader string */
      p_ctx->writer = p;
      Dmsg(ctx, debuglevel,
           "test-plugin-fd: plugin=%s fname=%s reader=%s writer=%s\n",
           p_ctx->cmd, p_ctx->fname, p_ctx->reader, p_ctx->writer);
      break;
    }
    default:
      Dmsg(ctx, debuglevel, "test-plugin-fd: unknown event=%d\n",
           event->eventType);
      break;
  }

  return bRC_OK;
}

// Start the backup of a specific file
static bRC startBackupFile(PluginContext* ctx, struct save_pkt* sp)
{
  struct plugin_ctx* p_ctx = (struct plugin_ctx*)ctx->plugin_private_context;
  if (!p_ctx) { return bRC_Error; }

  if (p_ctx->nb_obj == 0) {
    sp->fname = (char*)"takeme.h";
    Dmsg(ctx, debuglevel, "AcceptFile=%s = %d\n", sp->fname,
         bareos_core_functions->AcceptFile(ctx, sp));

    sp->fname = (char*)"/path/to/excludeme.o";
    Dmsg(ctx, debuglevel, "AcceptFile=%s = %d\n", sp->fname,
         bareos_core_functions->AcceptFile(ctx, sp));

    sp->fname = (char*)"/path/to/excludeme.c";
    Dmsg(ctx, debuglevel, "AcceptFile=%s = %d\n", sp->fname,
         bareos_core_functions->AcceptFile(ctx, sp));
  }

  if (p_ctx->nb_obj == 0) {
    sp->object_name = (char*)"james.xml";
    sp->object = (char *)"This is test data for the restore object. "
  "garbage=aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
  "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
  "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
  "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
  "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
  "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
  "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
  "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
  "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
  "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
  "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
  "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
  "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
  "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
  "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
  "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
  "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
  "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
  "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
  "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
  "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
  "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
  "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
  "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
  "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
  "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
  "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
  "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
  "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
  "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
  "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
  "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
  "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
  "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
  "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
  "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
  "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
  "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
  "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
  "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
  "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
  "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
  "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
  "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
  "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
  "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
  "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
  "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
  "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
  "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
  "\0secret";
    sp->object_len = strlen(sp->object) + 1 + 6 + 1; /* str + 0 + secret + 0 */
    sp->type = FT_RESTORE_FIRST;

    static int _nb = 0;
    POOLMEM* q = GetPoolMemory(PM_FNAME);
    char* working;
    FILE* fp;

    bareos_core_functions->getBareosValue(ctx, bVarWorkingDir, &working);
    Mmsg(q, "%s/torestore.%d", working, _nb++);
    if ((fp = fopen(q, "w")) != NULL) {
      fwrite(sp->object, sp->object_len, 1, fp);
      fclose(fp);
    }
    FreePoolMemory(q);

  } else if (p_ctx->nb_obj == 1) {
    ConfigFile ini;

    p_ctx->buf = GetPoolMemory(PM_BSOCK);
    Dmsg(ctx, debuglevel, "p_ctx->buf = 0x%x\n", p_ctx->buf);
    ini.RegisterItems(test_items, sizeof(struct ini_items));

    sp->object_name = (char*)INI_RESTORE_OBJECT_NAME;
    sp->object_len = ini.Serialize(p_ctx->buf);
    sp->object = p_ctx->buf;
    sp->type = FT_PLUGIN_CONFIG;

    Dmsg(ctx, debuglevel, "RestoreOptions=<%s>\n", p_ctx->buf);
  }

  time_t now = time(NULL);
  sp->index = ++p_ctx->nb_obj;
  sp->statp.st_mode = 0700 | S_IFREG;
  sp->statp.st_ctime = now;
  sp->statp.st_mtime = now;
  sp->statp.st_atime = now;
  sp->statp.st_size = sp->object_len;
  sp->statp.st_blksize = 4096;
  sp->statp.st_blocks = 1;
  Dmsg(ctx, debuglevel, "Creating RestoreObject len=%d oname=%s data=%.127s\n",
       sp->object_len, sp->object_name, sp->object);
  Dmsg(ctx, debuglevel, "test-plugin-fd: startBackupFile\n");
  return bRC_OK;
}

static bRC getAcl(PluginContext* ctx, acl_pkt* ap) { return bRC_OK; }

static bRC setAcl(PluginContext* ctx, acl_pkt* ap) { return bRC_OK; }

static bRC getXattr(PluginContext* ctx, xattr_pkt* xp) { return bRC_OK; }

static bRC setXattr(PluginContext* ctx, xattr_pkt* xp) { return bRC_OK; }

// Done with backup of this file
static bRC endBackupFile(PluginContext* ctx)
{
  /*
   * We would return bRC_More if we wanted startBackupFile to be
   * called again to backup another file
   */
  return bRC_OK;
}


// Bareos is calling us to do the actual I/O
static bRC pluginIO(PluginContext* ctx, struct io_pkt* io)
{
  struct plugin_ctx* p_ctx = (struct plugin_ctx*)ctx->plugin_private_context;
  if (!p_ctx) { return bRC_Error; }

  io->status = 0;
  io->io_errno = 0;
  return bRC_OK;
}

/**
 * Bareos is notifying us that a plugin name string was found, and
 *   passing us the plugin command, so we can prepare for a restore.
 */
static bRC startRestoreFile(PluginContext* ctx, const char* cmd)
{
  printf("test-plugin-fd: startRestoreFile cmd=%s\n", cmd);
  return bRC_OK;
}

/**
 * Bareos is notifying us that the plugin data has terminated, so
 *  the restore for this particular file is done.
 */
static bRC endRestoreFile(PluginContext* ctx)
{
  printf("test-plugin-fd: endRestoreFile\n");
  return bRC_OK;
}

/**
 * This is called during restore to create the file (if necessary)
 * We must return in rp->create_status:
 *
 *  CF_ERROR    -- error
 *  CF_SKIP     -- skip processing this file
 *  CF_EXTRACT  -- extract the file (i.e.call i/o routines)
 *  CF_CREATED  -- created, but no content to extract (typically directories)
 *
 */
static bRC createFile(PluginContext* ctx, struct restore_pkt* rp)
{
  printf("test-plugin-fd: createFile\n");
  if (strlen(rp->where) > 512) {
    printf("Restore target dir too long. Restricting to first 512 bytes.\n");
  }
  bstrncpy(((struct plugin_ctx*)ctx->plugin_private_context)->where, rp->where,
           513);
  ((struct plugin_ctx*)ctx->plugin_private_context)->replace = rp->replace;
  rp->create_status = CF_EXTRACT;
  return bRC_OK;
}

static bRC setFileAttributes(PluginContext* ctx, struct restore_pkt* rp)
{
  printf("test-plugin-fd: setFileAttributes\n");
  return bRC_OK;
}

/* When using Incremental dump, all previous dumps are necessary */
static bRC checkFile(PluginContext* ctx, char* fname) { return bRC_OK; }
} /* namespace filedaemon */
