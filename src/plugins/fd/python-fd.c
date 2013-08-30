/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2012-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2013 Bareos GmbH & Co. KG

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
 * Python plugin for the Bareos File Daemon
 *
 * Marco van Wieringen, August 2012
 */
#define BUILD_PLUGIN
#define BUILDING_DLL            /* required for Windows plugin */

#include "bareos.h"
#include "fd_plugins.h"

#ifdef HAVE_PYTHON
#undef _POSIX_C_SOURCE
#include <Python.h>
#endif

#define PLUGIN_LICENSE      "AGPLv3"
#define PLUGIN_AUTHOR       "Marco van Wieringen"
#define PLUGIN_DATE         "August 2012"
#define PLUGIN_VERSION      "1"
#define PLUGIN_DESCRIPTION  "Python File Daemon Plugin"
#define PLUGIN_USAGE        "(No usage yet)"

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

/* Pointers to Bareos functions */
static bFuncs *bfuncs = NULL;
static bInfo  *binfo = NULL;

static genpInfo pluginInfo = {
   sizeof(pluginInfo),
   FD_PLUGIN_INTERFACE_VERSION,
   FD_PLUGIN_MAGIC,
   PLUGIN_LICENSE,
   PLUGIN_AUTHOR,
   PLUGIN_DATE,
   PLUGIN_VERSION,
   PLUGIN_DESCRIPTION,
   PLUGIN_USAGE
};

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
   setFileAttributes,
   checkFile
};

/*
 * Plugin private context
 */
struct plugin_ctx {
#ifdef HAVE_PYTHON
      PyThreadState *interpreter;
#endif
};

/*
 * We don't actually use this but we need it to tear down the
 * final python interpreter on unload of the plugin. Each instance of
 * the plugin get its own interpreter.
 */
#ifdef HAVE_PYTHON
static PyThreadState *mainThreadState;
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Plugin called here when it is first loaded
 */
bRC DLL_IMP_EXP
loadPlugin(bInfo *lbinfo, bFuncs *lbfuncs, genpInfo **pinfo, pFuncs **pfuncs)
{
   bfuncs = lbfuncs;                  /* Set Bareos funct pointers */
   binfo  = lbinfo;

   *pinfo  = &pluginInfo;             /* Return pointer to our info */
   *pfuncs = &pluginFuncs;            /* Return pointer to our functions */

#ifdef HAVE_PYTHON
   /*
    * Setup Python
    */
   Py_Initialize();
   PyEval_InitThreads();
   mainThreadState = PyEval_SaveThread();
#endif

   return bRC_OK;
}

/*
 * Plugin called here when it is unloaded, normally when
 *  Bareos is going to exit.
 */
bRC unloadPlugin()
{
#ifdef HAVE_PYTHON
   /*
    * Terminate Python
    */
   PyEval_RestoreThread(mainThreadState);
   Py_Finalize();
#endif

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
static bRC newPlugin(bpContext *ctx)
{
#ifdef HAVE_PYTHON
   struct plugin_ctx *p_ctx;

   p_ctx = (struct plugin_ctx *)malloc(sizeof(struct plugin_ctx));
   if (!p_ctx) {
      return bRC_Error;
   }
   memset(p_ctx, 0, sizeof(struct plugin_ctx));
   ctx->pContext = (void *)p_ctx;        /* set our context pointer */

   /*
    * For each plugin instance we instantiate a new Python interpreter.
    */
   PyEval_AcquireLock();
   p_ctx->interpreter = Py_NewInterpreter();
   PyEval_ReleaseThread(p_ctx->interpreter);
#endif

   return bRC_OK;
}

/*
 * Release everything concerning a particular instance of a
 *  plugin. Normally called when the Job terminates.
 */
static bRC freePlugin(bpContext *ctx)
{
#ifdef HAVE_PYTHON
   struct plugin_ctx *p_ctx = (struct plugin_ctx *)ctx->pContext;

   /*
    * Stop any sub interpreter started per plugin instance.
    */
   PyEval_AcquireThread(p_ctx->interpreter);

   /*
    * Do python cleanup calls.
    */

   Py_EndInterpreter(p_ctx->interpreter);
   PyEval_ReleaseLock();
#endif

   return bRC_OK;
}

/*
 * Called by core code to get a variable from the plugin.
 *   Not currently used.
 */
static bRC getPluginValue(bpContext *ctx, pVariable var, void *value)
{
#ifdef HAVE_PYTHON
   struct plugin_ctx *p_ctx = (struct plugin_ctx *)ctx->pContext;

   PyEval_AcquireThread(p_ctx->interpreter);

   /*
    * Do python calls.
    */

   PyEval_ReleaseThread(p_ctx->interpreter);
#endif

   return bRC_OK;
}

/*
 * Called by core code to set a plugin variable.
 *  Not currently used.
 */
static bRC setPluginValue(bpContext *ctx, pVariable var, void *value)
{
#ifdef HAVE_PYTHON
   struct plugin_ctx *p_ctx = (struct plugin_ctx *)ctx->pContext;

   PyEval_AcquireThread(p_ctx->interpreter);

   /*
    * Do python calls.
    */

   PyEval_ReleaseThread(p_ctx->interpreter);
#endif

   return bRC_OK;
}

/*
 * Called by Bareos when there are certain events that the
 *   plugin might want to know.  The value depends on the
 *   event.
 */
static bRC handlePluginEvent(bpContext *ctx, bEvent *event, void *value)
{
#ifdef HAVE_PYTHON
   struct plugin_ctx *p_ctx = (struct plugin_ctx *)ctx->pContext;

   PyEval_AcquireThread(p_ctx->interpreter);

   /*
    * Do python calls.
    */

   PyEval_ReleaseThread(p_ctx->interpreter);
#endif

   return bRC_OK;
}

/*
 * Called when starting to backup a file.  Here the plugin must
 *  return the "stat" packet for the directory/file and provide
 *  certain information so that Bareos knows what the file is.
 *  The plugin can create "Virtual" files by giving them a
 *  name that is not normally found on the file system.
 */
static bRC startBackupFile(bpContext *ctx, struct save_pkt *sp)
{
#ifdef HAVE_PYTHON
   struct plugin_ctx *p_ctx = (struct plugin_ctx *)ctx->pContext;

   PyEval_AcquireThread(p_ctx->interpreter);

   /*
    * Do python calls.
    */

   PyEval_ReleaseThread(p_ctx->interpreter);
#endif

   return bRC_OK;
}

/*
 * Done backing up a file.
 */
static bRC endBackupFile(bpContext *ctx)
{
#ifdef HAVE_PYTHON
   struct plugin_ctx *p_ctx = (struct plugin_ctx *)ctx->pContext;

   PyEval_AcquireThread(p_ctx->interpreter);

   /*
    * Do python calls.
    */

   PyEval_ReleaseThread(p_ctx->interpreter);
#endif

   return bRC_OK;
}

/*
 * Do actual I/O.  Bareos calls this after startBackupFile
 *   or after startRestoreFile to do the actual file
 *   input or output.
 */
static bRC pluginIO(bpContext *ctx, struct io_pkt *io)
{
#ifdef HAVE_PYTHON
   struct plugin_ctx *p_ctx = (struct plugin_ctx *)ctx->pContext;

   PyEval_AcquireThread(p_ctx->interpreter);

   /*
    * Do python calls.
    */

   PyEval_ReleaseThread(p_ctx->interpreter);
#endif

   return bRC_OK;
}

static bRC startRestoreFile(bpContext *ctx, const char *cmd)
{
#ifdef HAVE_PYTHON
   struct plugin_ctx *p_ctx = (struct plugin_ctx *)ctx->pContext;

   PyEval_AcquireThread(p_ctx->interpreter);

   /*
    * Do python calls.
    */

   PyEval_ReleaseThread(p_ctx->interpreter);
#endif

   return bRC_OK;
}

static bRC endRestoreFile(bpContext *ctx)
{
#ifdef HAVE_PYTHON
   struct plugin_ctx *p_ctx = (struct plugin_ctx *)ctx->pContext;

   PyEval_AcquireThread(p_ctx->interpreter);

   /*
    * Do python calls.
    */

   PyEval_ReleaseThread(p_ctx->interpreter);
#endif

   return bRC_OK;
}

/*
 * Called here to give the plugin the information needed to
 *  re-create the file on a restore.  It basically gets the
 *  stat packet that was created during the backup phase.
 *  This data is what is needed to create the file, but does
 *  not contain actual file data.
 */
static bRC createFile(bpContext *ctx, struct restore_pkt *rp)
{
#ifdef HAVE_PYTHON
   struct plugin_ctx *p_ctx = (struct plugin_ctx *)ctx->pContext;

   PyEval_AcquireThread(p_ctx->interpreter);

   /*
    * Do python calls.
    */

   PyEval_ReleaseThread(p_ctx->interpreter);
#endif

   return bRC_OK;
}

/*
 * Called after the file has been restored. This can be used to
 *  set directory permissions, ...
 */
static bRC setFileAttributes(bpContext *ctx, struct restore_pkt *rp)
{
#ifdef HAVE_PYTHON
   struct plugin_ctx *p_ctx = (struct plugin_ctx *)ctx->pContext;

   PyEval_AcquireThread(p_ctx->interpreter);

   /*
    * Do python calls.
    */

   PyEval_ReleaseThread(p_ctx->interpreter);
#endif

   return bRC_OK;
}

/* When using Incremental dump, all previous dumps are necessary */
static bRC checkFile(bpContext *ctx, char *fname)
{
#ifdef HAVE_PYTHON
   struct plugin_ctx *p_ctx = (struct plugin_ctx *)ctx->pContext;

   PyEval_AcquireThread(p_ctx->interpreter);

   /*
    * Do python calls.
    */

   PyEval_ReleaseThread(p_ctx->interpreter);
#endif

   return bRC_OK;
}
