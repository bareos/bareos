/*
   BAREOS R) - Backup Archiving Recovery Made Open Source

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
 * Python Storage daemon Plugin program
 *
 * Marco van Wieringen, August 2012
 *
 */
#include "bareos.h"
#include "stored.h"

#ifdef HAVE_PYTHON
#undef _POSIX_C_SOURCE
#include <Python.h>
#endif

#define PLUGIN_LICENSE      "Bareos AGPLv3"
#define PLUGIN_AUTHOR       "Marco van Wieringen"
#define PLUGIN_DATE         "August 2012"
#define PLUGIN_VERSION      "1"
#define PLUGIN_DESCRIPTION  "Python Storage Daemon Plugin"
#define PLUGIN_USAGE        "(No usage yet)"

/* Forward referenced functions */
static bRC newPlugin(bpContext *ctx);
static bRC freePlugin(bpContext *ctx);
static bRC getPluginValue(bpContext *ctx, psdVariable var, void *value);
static bRC setPluginValue(bpContext *ctx, psdVariable var, void *value);
static bRC handlePluginEvent(bpContext *ctx, bsdEvent *event, void *value);


/* Pointers to Bareos functions */
static bsdFuncs *bfuncs = NULL;
static bsdInfo  *binfo = NULL;

static genpInfo pluginInfo = {
   sizeof(pluginInfo),
   SD_PLUGIN_INTERFACE_VERSION,
   SD_PLUGIN_MAGIC,
   PLUGIN_LICENSE,
   PLUGIN_AUTHOR,
   PLUGIN_DATE,
   PLUGIN_VERSION,
   PLUGIN_DESCRIPTION,
   PLUGIN_USAGE
};

static psdFuncs pluginFuncs = {
   sizeof(pluginFuncs),
   SD_PLUGIN_INTERFACE_VERSION,

   /* Entry points into plugin */
   newPlugin,                         /* new plugin instance */
   freePlugin,                        /* free plugin instance */
   getPluginValue,
   setPluginValue,
   handlePluginEvent
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
 * loadPlugin() and unloadPlugin() are entry points that are
 *  exported, so Bareos can directly call these two entry points
 *  they are common to all Bareos plugins.
 *
 * External entry point called by Bareos to "load" the plugin
 */
bRC DLL_IMP_EXP loadPlugin(bsdInfo *lbinfo,
                           bsdFuncs *lbfuncs,
                           genpInfo **pinfo,
                           psdFuncs **pfuncs)
{
   bfuncs = lbfuncs;                /* Set Bareos funct pointers */
   binfo  = lbinfo;

   *pinfo  = &pluginInfo;           /* Return pointer to our info */
   *pfuncs = &pluginFuncs;          /* Return pointer to our functions */

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
 * External entry point to unload the plugin
 */
bRC DLL_IMP_EXP unloadPlugin()
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
 * The following entry points are accessed through the function
 * pointers we supplied to Bareos. Each plugin type (dir, fd, sd)
 * has its own set of entry points that the plugin must define.
 */
/*
 * Create a new instance of the plugin i.e. allocate our private storage
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
 * Free a plugin instance, i.e. release our private storage
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
 * Return some plugin value (none defined)
 */
static bRC getPluginValue(bpContext *ctx, psdVariable var, void *value)
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
 * Set a plugin value (none defined)
 */
static bRC setPluginValue(bpContext *ctx, psdVariable var, void *value)
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
 * Handle an event that was generated in Bareos
 */
static bRC handlePluginEvent(bpContext *ctx, bsdEvent *event, void *value)
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
