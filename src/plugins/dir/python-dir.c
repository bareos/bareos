/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2012-2012 Free Software Foundation Europe e.V.

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
 * Python Plugin
 *
 * Marco van Wieringen, August 2012
 *
 */
#include "bacula.h"
#include "dir_plugins.h"

#ifdef HAVE_PYTHON
#undef _POSIX_C_SOURCE
#include <Python.h>
#endif

#define PLUGIN_LICENSE      "Bacula AGPLv3"
#define PLUGIN_AUTHOR       "Marco van Wieringen"
#define PLUGIN_DATE         "August 2012"
#define PLUGIN_VERSION      "1"
#define PLUGIN_DESCRIPTION  "Python Director Daemon Plugin"

/* Forward referenced functions */
static bRC newPlugin(bpContext *ctx);
static bRC freePlugin(bpContext *ctx);
static bRC getPluginValue(bpContext *ctx, pDirVariable var, void *value);
static bRC setPluginValue(bpContext *ctx, pDirVariable var, void *value);
static bRC handlePluginEvent(bpContext *ctx, bDirEvent *event, void *value);

/* Pointers to Bacula functions */
static bDirFuncs *bfuncs = NULL;
static bDirInfo  *binfo = NULL;

static genpInfo pluginInfo = {
   sizeof(pluginInfo),
   DIR_PLUGIN_INTERFACE_VERSION,
   DIR_PLUGIN_MAGIC,
   PLUGIN_LICENSE,
   PLUGIN_AUTHOR,
   PLUGIN_DATE,
   PLUGIN_VERSION,
   PLUGIN_DESCRIPTION
};

static pDirFuncs pluginFuncs = {
   sizeof(pluginFuncs),
   DIR_PLUGIN_INTERFACE_VERSION,

   /* Entry points into plugin */
   newPlugin,                         /* New plugin instance */
   freePlugin,                        /* Free plugin instance */
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
 *  exported, so Bacula can directly call these two entry points
 *  they are common to all Bacula plugins.
 *
 * External entry point called by Bacula to "load" the plugin
 */
bRC loadPlugin(bDirInfo *lbinfo, bDirFuncs *lbfuncs, genpInfo **pinfo, pDirFuncs **pfuncs)
{
   bfuncs = lbfuncs;                  /* Set Bacula funct pointers */
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
 * External entry point to unload the plugin
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

static bRC newPlugin(bpContext *ctx)
{
   struct plugin_ctx *p_ctx;

   p_ctx = (struct plugin_ctx *)malloc(sizeof(struct plugin_ctx));
   if (!p_ctx) {
      return bRC_Error;
   }
   memset(p_ctx, 0, sizeof(struct plugin_ctx));
   ctx->pContext = (void *)p_ctx;        /* set our context pointer */

#ifdef HAVE_PYTHON
   /*
    * For each plugin instance we instantiate a new Python interpreter.
    */
   PyEval_AcquireLock();
   p_ctx->interpreter = Py_NewInterpreter();
   PyEval_ReleaseThread(p_ctx->interpreter);
#endif

   return bRC_OK;
}

static bRC freePlugin(bpContext *ctx)
{
   struct plugin_ctx *p_ctx = (struct plugin_ctx *)ctx->pContext;

#ifdef HAVE_PYTHON
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

static bRC getPluginValue(bpContext *ctx, pDirVariable var, void *value)
{
   struct plugin_ctx *p_ctx = (struct plugin_ctx *)ctx->pContext;

#ifdef HAVE_PYTHON
   PyEval_AcquireThread(p_ctx->interpreter);

   /*
    * Do python calls.
    */

   PyEval_ReleaseThread(p_ctx->interpreter);
#endif
   return bRC_OK;
}

static bRC setPluginValue(bpContext *ctx, pDirVariable var, void *value)
{
   struct plugin_ctx *p_ctx = (struct plugin_ctx *)ctx->pContext;

#ifdef HAVE_PYTHON
   PyEval_AcquireThread(p_ctx->interpreter);

   /*
    * Do python calls.
    */

   PyEval_ReleaseThread(p_ctx->interpreter);
#endif
   return bRC_OK;
}

static bRC handlePluginEvent(bpContext *ctx, bDirEvent *event, void *value)
{
   struct plugin_ctx *p_ctx = (struct plugin_ctx *)ctx->pContext;

#ifdef HAVE_PYTHON
   PyEval_AcquireThread(p_ctx->interpreter);

   /*
    * Do python calls.
    */

   PyEval_ReleaseThread(p_ctx->interpreter);
#endif

   return bRC_OK;
}
