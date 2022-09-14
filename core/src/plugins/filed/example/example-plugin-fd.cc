/*

   Copyright (C) 2007-2012 Kern Sibbald
   Copyright (C) 2013-2022 Bareos GmbH & Co. KG

   You may freely use this code to create your own plugin provided
   it is to write a plugin for Bareos licensed under AGPLv3
   (as Bareos is), and in that case, you may also remove
   the above Copyright and this notice as well as modify
   the code in any way.

*/

#define BUILD_PLUGIN
#define BUILDING_DLL /* required for Windows plugin */

#include <cinttypes>
#include "include/bareos.h"
#include "filed/fd_plugins.h"

#define PLUGIN_LICENSE "Bareos AGPLv3"
#define PLUGIN_AUTHOR "Your name"
#define PLUGIN_DATE "January 2010"
#define PLUGIN_VERSION "1"
#define PLUGIN_DESCRIPTION "Test File Daemon Plugin"

namespace filedaemon {

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

static PluginInformation pluginInfo = {sizeof(pluginInfo),
                                       FD_PLUGIN_INTERFACE_VERSION,
                                       FD_PLUGIN_MAGIC,
                                       PLUGIN_LICENSE,
                                       PLUGIN_AUTHOR,
                                       PLUGIN_DATE,
                                       PLUGIN_VERSION,
                                       PLUGIN_DESCRIPTION,
                                       nullptr};

static PluginFunctions pluginFuncs
    = {sizeof(pluginFuncs), FD_PLUGIN_INTERFACE_VERSION,

       /* Entry points into plugin */
       newPlugin,  /* new plugin instance */
       freePlugin, /* free plugin instance */
       getPluginValue, setPluginValue, handlePluginEvent, startBackupFile,
       endBackupFile, startRestoreFile, endRestoreFile, pluginIO, createFile,
       setFileAttributes, checkFile, getAcl, setAcl, getXattr, setXattr};

#ifdef __cplusplus
extern "C" {
#endif

// Plugin called here when it is first loaded
bRC loadPlugin(PluginApiDefinition* lbareos_plugin_interface_version,
               CoreFunctions* lbareos_core_functions,
               PluginInformation** plugin_information,
               PluginFunctions** plugin_functions)
{
  bareos_core_functions
      = lbareos_core_functions; /* set Bareos funct pointers */
  bareos_plugin_interface_version = lbareos_plugin_interface_version;
  printf("plugin: Loaded: size=%d version=%d\n", bareos_core_functions->size,
         bareos_core_functions->version);

  *plugin_information = &pluginInfo; /* return pointer to our info */
  *plugin_functions = &pluginFuncs;  /* return pointer to our functions */

  return bRC_OK;
}

/*
 * Plugin called here when it is unloaded, normally when
 *  Bareos is going to exit.
 */
bRC unloadPlugin()
{
  printf("plugin: Unloaded\n");
  return bRC_OK;
}

#ifdef __cplusplus
}
#endif

/*
 * Called here to make a new instance of the plugin -- i.e. when
 *  a new Job is started.  There can be multiple instances of
 *  each plugin that are running at the same time.  Your
 *  plugin instance must be thread safe and keep its own
 *  local data.
 */
static bRC newPlugin(PluginContext* ctx)
{
  int JobId = 0;
  bareos_core_functions->getBareosValue(ctx, bVarJobId, (void*)&JobId);
  // printf("plugin: newPlugin JobId=%d\n", JobId);
  bareos_core_functions->registerBareosEvents(
      ctx, 10, bEventJobStart, bEventJobEnd, bEventStartBackupJob,
      bEventEndBackupJob, bEventLevel, bEventSince, bEventStartRestoreJob,
      bEventEndRestoreJob, bEventRestoreCommand, bEventBackupCommand);
  return bRC_OK;
}

/*
 * Release everything concerning a particular instance of a
 *  plugin. Normally called when the Job terminates.
 */
static bRC freePlugin(PluginContext* ctx)
{
  int JobId = 0;
  bareos_core_functions->getBareosValue(ctx, bVarJobId, (void*)&JobId);
  // printf("plugin: freePlugin JobId=%d\n", JobId);
  return bRC_OK;
}

/*
 * Called by core code to get a variable from the plugin.
 *   Not currently used.
 */
static bRC getPluginValue(PluginContext* ctx, pVariable var, void* value)
{
  // printf("plugin: getPluginValue var=%d\n", var);
  return bRC_OK;
}

/*
 * Called by core code to set a plugin variable.
 *  Not currently used.
 */
static bRC setPluginValue(PluginContext* ctx, pVariable var, void* value)
{
  // printf("plugin: setPluginValue var=%d\n", var);
  return bRC_OK;
}

/*
 * Called by Bareos when there are certain events that the
 *   plugin might want to know.  The value depends on the
 *   event.
 */
static bRC handlePluginEvent(PluginContext* ctx, bEvent* event, void* value)
{
  char* name;

  switch (event->eventType) {
    case bEventJobStart:
      printf("plugin: JobStart=%s\n", NPRT((char*)value));
      break;
    case bEventJobEnd:
      printf("plugin: JobEnd\n");
      break;
    case bEventStartBackupJob:
      printf("plugin: BackupStart\n");
      break;
    case bEventEndBackupJob:
      printf("plugin: BackupEnd\n");
      break;
    case bEventLevel:
      printf("plugin: JobLevel=%c %" PRId64 "\n", (int)(int64_t)value,
             (int64_t)value);
      break;
    case bEventSince:
      printf("plugin: since=%" PRId64 "\n", (int64_t)value);
      break;
    case bEventStartRestoreJob:
      printf("plugin: StartRestoreJob\n");
      break;
    case bEventEndRestoreJob:
      printf("plugin: EndRestoreJob\n");
      break;
    case bEventRestoreCommand:
      // Plugin command e.g. plugin = <plugin-name>:<name-space>:command
      printf("plugin: backup command=%s\n", NPRT((char*)value));
      break;
    case bEventBackupCommand:
      // Plugin command e.g. plugin = <plugin-name>:<name-space>:command
      printf("plugin: backup command=%s\n", NPRT((char*)value));
      break;
    default:
      printf("plugin: unknown event=%d\n", event->eventType);
  }
  bareos_core_functions->getBareosValue(ctx, bVarFDName, (void*)&name);

  return bRC_OK;
}

/*
 * Called when starting to backup a file.  Here the plugin must
 * return the "stat" packet for the directory/file and provide
 * certain information so that Bareos knows what the file is.
 * The plugin can create "Virtual" files by giving them a
 * name that is not normally found on the file system.
 */
static bRC startBackupFile(PluginContext* ctx, struct save_pkt* sp)
{
  return bRC_OK;
}

// Done backing up a file.
static bRC endBackupFile(PluginContext* ctx) { return bRC_OK; }

/*
 * Do actual I/O. Bareos calls this after startBackupFile
 * or after startRestoreFile to do the actual file input or output.
 */
static bRC pluginIO(PluginContext* ctx, struct io_pkt* io)
{
  io->status = 0;
  io->io_errno = 0;
  switch (io->func) {
    case IO_OPEN:
      printf("plugin: IO_OPEN\n");
      break;
    case IO_READ:
      printf("plugin: IO_READ buf=%p len=%d\n", io->buf, io->count);
      break;
    case IO_WRITE:
      printf("plugin: IO_WRITE buf=%p len=%d\n", io->buf, io->count);
      break;
    case IO_CLOSE:
      printf("plugin: IO_CLOSE\n");
      break;
  }
  return bRC_OK;
}

static bRC startRestoreFile(PluginContext* ctx, const char* cmd)
{
  return bRC_OK;
}

static bRC endRestoreFile(PluginContext* ctx) { return bRC_OK; }

/*
 * Called here to give the plugin the information needed to
 * re-create the file on a restore.  It basically gets the
 * stat packet that was created during the backup phase.
 * This data is what is needed to create the file, but does
 * not contain actual file data.
 */
static bRC createFile(PluginContext* ctx, struct restore_pkt* rp)
{
  return bRC_OK;
}

/*
 * Called after the file has been restored. This can be used to set directory
 * permissions, ...
 */
static bRC setFileAttributes(PluginContext* ctx, struct restore_pkt* rp)
{
  return bRC_OK;
}

static bRC getAcl(PluginContext* ctx, acl_pkt* ap) { return bRC_OK; }

static bRC setAcl(PluginContext* ctx, acl_pkt* ap) { return bRC_OK; }

static bRC getXattr(PluginContext* ctx, xattr_pkt* xp) { return bRC_OK; }

static bRC setXattr(PluginContext* ctx, xattr_pkt* xp) { return bRC_OK; }

// When using Incremental dump, all previous dumps are necessary
static bRC checkFile(PluginContext* ctx, char* fname) { return bRC_OK; }

} /* namespace filedaemon */
