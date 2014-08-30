/*
   BAREOS® - Backup Archiving REcovery Open Sourced

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
 * Python Director Plugin program
 *
 * Marco van Wieringen, August 2012
 */
#include "bareos.h"
#include "dird.h"

#undef _POSIX_C_SOURCE
#include <Python.h>

#if (PY_VERSION_HEX <  0x02060000)
#error "Need at least Python version 2.6 or newer"
#endif

static const int dbglvl = 150;

#define PLUGIN_LICENSE      "Bareos AGPLv3"
#define PLUGIN_AUTHOR       "Marco van Wieringen"
#define PLUGIN_DATE         "October 2013"
#define PLUGIN_VERSION      "2"
#define PLUGIN_DESCRIPTION  "Python Director Daemon Plugin"
#define PLUGIN_USAGE        "(No usage yet)"

#define Dmsg(context, level,  ...) bfuncs->DebugMessage(context, __FILE__, __LINE__, level, __VA_ARGS__ )
#define Jmsg(context, type,  ...) bfuncs->JobMessage(context, __FILE__, __LINE__, type, 0, __VA_ARGS__ )

/* Forward referenced functions */
static bRC newPlugin(bpContext *ctx);
static bRC freePlugin(bpContext *ctx);
static bRC getPluginValue(bpContext *ctx, pDirVariable var, void *value);
static bRC setPluginValue(bpContext *ctx, pDirVariable var, void *value);
static bRC handlePluginEvent(bpContext *ctx, bDirEvent *event, void *value);

static void PyErrorHandler(bpContext *ctx, int msgtype);
static bRC PyLoadModule(bpContext *ctx);
static bRC PyGetPluginValue(bpContext *ctx, pDirVariable var, void *value);
static bRC PySetPluginValue(bpContext *ctx, pDirVariable var, void *value);
static bRC PyHandlePluginEvent(bpContext *ctx, bDirEvent *event, void *value);

/* Pointers to Bareos functions */
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
   PLUGIN_DESCRIPTION,
   PLUGIN_USAGE
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
   PyThreadState *interpreter;
   PyObject *pModule;
   PyObject *pDict;
   PyObject *bpContext;
};

#include "python-dir.h"

/*
 * We don't actually use this but we need it to tear down the
 * final python interpreter on unload of the plugin. Each instance of
 * the plugin get its own interpreter.
 */
static PyThreadState *mainThreadState;

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
bRC DLL_IMP_EXP loadPlugin(bDirInfo *lbinfo,
                           bDirFuncs *lbfuncs,
                           genpInfo **pinfo,
                           pDirFuncs **pfuncs)
{
   bfuncs = lbfuncs;                  /* Set Bareos funct pointers */
   binfo  = lbinfo;

   *pinfo  = &pluginInfo;             /* Return pointer to our info */
   *pfuncs = &pluginFuncs;            /* Return pointer to our functions */

   /*
    * Setup Python
    */
   Py_Initialize();
   PyEval_InitThreads();
   mainThreadState = PyEval_SaveThread();

   return bRC_OK;
}

/*
 * External entry point to unload the plugin
 */
bRC DLL_IMP_EXP unloadPlugin()
{
   /*
    * Terminate Python
    */
   PyEval_RestoreThread(mainThreadState);
   Py_Finalize();

   return bRC_OK;
}

#ifdef __cplusplus
}
#endif

static bRC newPlugin(bpContext *ctx)
{
   bRC retval = bRC_Error;
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
   retval = PyLoadModule(ctx);
   PyEval_ReleaseThread(p_ctx->interpreter);

   return retval;
}

static bRC freePlugin(bpContext *ctx)
{
   struct plugin_ctx *p_ctx = (struct plugin_ctx *)ctx->pContext;

   if (!p_ctx) {
      return bRC_Error;
   }

   /*
    * Stop any sub interpreter started per plugin instance.
    */
   PyEval_AcquireThread(p_ctx->interpreter);

   /*
    * Do python cleanup calls.
    */
   if (p_ctx->bpContext) {
      Py_DECREF(p_ctx->bpContext);
   }

   if (p_ctx->pModule) {
      Py_DECREF(p_ctx->pModule);
   }

   Py_EndInterpreter(p_ctx->interpreter);
   PyEval_ReleaseLock();

   free(p_ctx);
   ctx->pContext = NULL;

   return bRC_OK;
}

static bRC getPluginValue(bpContext *ctx, pDirVariable var, void *value)
{
   struct plugin_ctx *p_ctx = (struct plugin_ctx *)ctx->pContext;
   bRC retval = bRC_Error;

   PyEval_AcquireThread(p_ctx->interpreter);
   retval = PyGetPluginValue(ctx, var, value);
   PyEval_ReleaseThread(p_ctx->interpreter);

   return retval;
}

static bRC setPluginValue(bpContext *ctx, pDirVariable var, void *value)
{
   struct plugin_ctx *p_ctx = (struct plugin_ctx *)ctx->pContext;
   bRC retval = bRC_Error;

   PyEval_AcquireThread(p_ctx->interpreter);
   retval = PySetPluginValue(ctx, var, value);
   PyEval_ReleaseThread(p_ctx->interpreter);

   return retval;
}

static bRC handlePluginEvent(bpContext *ctx, bDirEvent *event, void *value)
{
   struct plugin_ctx *p_ctx = (struct plugin_ctx *)ctx->pContext;
   bRC retval = bRC_Error;

   PyEval_AcquireThread(p_ctx->interpreter);
   retval = PyHandlePluginEvent(ctx, event, value);
   PyEval_ReleaseThread(p_ctx->interpreter);

   return retval;
}

/*
 * Work around API changes in Python versions.
 * These function abstract the storage and retrieval of the bpContext
 * which is passed to the Python methods and which the method can pass
 * back and which allow the callback function to understand what bpContext
 * its talking about.
 */
#if ((PY_VERSION_HEX <  0x02070000) || \
     ((PY_VERSION_HEX >= 0x03000000) && \
      (PY_VERSION_HEX <  0x03010000)))
/*
 * Python version before 2.7 and 3.0.
 */
static PyObject *PyCreatebpContext(bpContext *ctx)
{
   /*
    * Setup a new CObject which holds the bpContext structure used here internally.
    */
   return PyCObject_FromVoidPtr((void *)ctx, NULL);
}

static bpContext *PyGetbpContext(PyObject *pyCtx)
{
   return (bpContext *)PyCObject_AsVoidPtr(pyCtx);
}
#else
/*
 * Python version after 2.6 and 3.1.
 */
static PyObject *PyCreatebpContext(bpContext *ctx)
{
   /*
    * Setup a new Capsule which holds the bpContext structure used here internally.
    */
   return PyCapsule_New((void *)ctx, "bareos.bpContext", NULL);
}

static bpContext *PyGetbpContext(PyObject *pyCtx)
{
   return (bpContext *)PyCapsule_GetPointer(pyCtx, "bareos.bpContext");
}
#endif

/*
 * Convert a return value into a bRC enum value.
 */
static inline bRC conv_python_retval(PyObject *pRetVal)
{
   int retval;

   retval = PyInt_AsLong(pRetVal);
   return (bRC)retval;
}

/*
 * Handle a Python error.
 *
 * Python equivalent:
 *
 * import traceback, sys
 * return "".join(traceback.format_exception(sys.exc_type,
 *    sys.exc_value, sys.exc_traceback))
 */
static void PyErrorHandler(bpContext *ctx, int msgtype)
{
   PyObject *type, *value, *traceback;
   PyObject *tracebackModule;
   char *error_string;

   PyErr_Fetch(&type, &value, &traceback);

   tracebackModule = PyImport_ImportModule("traceback");
   if (tracebackModule != NULL) {
      PyObject *tbList, *emptyString, *strRetval;

      tbList = PyObject_CallMethod(tracebackModule,
                                   (char *)"format_exception",
                                   (char *)"OOO",
                                   type,
                                   value == NULL ? Py_None : value,
                                   traceback == NULL ? Py_None : traceback);

      emptyString = PyString_FromString("");
      strRetval = PyObject_CallMethod(emptyString,
                                      (char *)"join",
                                      (char *)"O", tbList);

      error_string = bstrdup(PyString_AsString(strRetval));

      Py_DECREF(tbList);
      Py_DECREF(emptyString);
      Py_DECREF(strRetval);
      Py_DECREF(tracebackModule);
   } else {
      error_string = bstrdup("Unable to import traceback module.");
   }

   Py_DECREF(type);
   Py_XDECREF(value);
   Py_XDECREF(traceback);

   Dmsg(ctx, dbglvl, "%s\n", error_string);
   if (msgtype) {
      Jmsg(ctx, msgtype, "%s\n", error_string);
   }

   free(error_string);
}

/*
 * Initial load of the Python module.
 *
 * Based on the parsed plugin options we set some prerequisits like the
 * module path and the module to load. We also load the dictionary used
 * for looking up the Python methods.
 */
static bRC PyLoadModule(bpContext *ctx)
{
   char *value;
   bRC retval = bRC_Error;
   struct plugin_ctx *p_ctx = (struct plugin_ctx *)ctx->pContext;
   PyObject *sysPath,
            *mPath,
            *pName,
            *pFunc,
            *module;

   /*
    * Extend the Python search path with the defoned plugin_directory.
    */
   if (bfuncs->getBareosValue(NULL, bDirVarPluginDir, &value) == bRC_OK) {
      sysPath = PySys_GetObject((char *)"path");
      mPath = PyString_FromString(value);
      PyList_Append(sysPath, mPath);
      Py_DECREF(mPath);
   }

   /*
    * Make our callback methods available for Python.
    */
   module = Py_InitModule("bareosdir", BareosDIRMethods);

   Dmsg(ctx, dbglvl, "Trying to load module with name bareos-dir\n");
   pName = PyString_FromString("bareos-dir");
   p_ctx->pModule = PyImport_Import(pName);
   Py_DECREF(pName);

   if (!p_ctx->pModule) {
      Dmsg(ctx, dbglvl, "Failed to load module with name bareos-dir\n");
      goto bail_out;
   }

   Dmsg(ctx, dbglvl, "Sucessfully loaded module with name bareos-dir\n");

   /*
    * Get the Python dictionary for lookups in the Python namespace.
    */
   p_ctx->pDict = PyModule_GetDict(p_ctx->pModule); /* Borrowed reference */

   /*
    * Encode the bpContext so a Python method can pass it in on calling back.
    */
   p_ctx->bpContext = PyCreatebpContext(ctx);

   /*
    * Lookup the load_bareos_plugin() function in the python module.
    */
   pFunc = PyDict_GetItemString(p_ctx->pDict, "load_bareos_plugin"); /* Borrowed reference */
   if (pFunc && PyCallable_Check(pFunc)) {
      PyObject *pRetVal;

      pRetVal = PyObject_CallFunctionObjArgs(pFunc, p_ctx->bpContext, NULL);

      if (!pRetVal) {
         goto bail_out;
      } else {
         retval = conv_python_retval(pRetVal);
         Py_DECREF(pRetVal);
      }
   } else {
      Dmsg(ctx, dbglvl, "Failed to find function named load_bareos_plugins()\n");
      goto bail_out;
   }

   return retval;

bail_out:
   if (PyErr_Occurred()) {
      PyErrorHandler(ctx, M_FATAL);
   }

   return retval;
}

static bRC PyGetPluginValue(bpContext *ctx, pDirVariable var, void *value)
{
   return bRC_OK;
}

static bRC PySetPluginValue(bpContext *ctx, pDirVariable var, void *value)
{
   return bRC_OK;
}

static bRC PyHandlePluginEvent(bpContext *ctx, bDirEvent *event, void *value)
{
   bRC retval = bRC_Error;
   plugin_ctx *p_ctx = (plugin_ctx *)ctx->pContext;
   PyObject *pFunc;

   /*
    * Lookup the handle_plugin_event() function in the python module.
    */
   pFunc = PyDict_GetItemString(p_ctx->pDict, "handle_plugin_event"); /* Borrowed reference */
   if (pFunc && PyCallable_Check(pFunc)) {
      PyObject *pEventType,
               *pRetVal;

      pEventType = PyInt_FromLong(event->eventType);

      pRetVal = PyObject_CallFunctionObjArgs(pFunc, p_ctx->bpContext, pEventType, NULL);
      Py_DECREF(pEventType);

      if (!pRetVal) {
         goto bail_out;
      } else {
         retval = conv_python_retval(pRetVal);
         Py_DECREF(pRetVal);
      }
   } else {
      Dmsg(ctx, dbglvl, "Failed to find function named handle_plugin_event()\n");
   }

   return retval;

bail_out:
   if (PyErr_Occurred()) {
      PyErrorHandler(ctx, M_FATAL);
   }

   return retval;
}

/*
 * Callback function which is exposed as a part of the additional methods which allow
 * a Python plugin to get certain internal values of the current Job.
 */
static PyObject *PyBareosGetValue(PyObject *self, PyObject *args)
{
   int var;
   bpContext *ctx = NULL;
   PyObject *pyCtx;
   PyObject *pRetVal = NULL;

   if (!PyArg_ParseTuple(args, "Oi:BareosGetValue", &pyCtx, &var)) {
      return NULL;
   }

   switch (var) {
   case bDirVarJobId:
   case bDirVarLevel:
   case bDirVarType:
   case bDirVarNumVols:
   case bDirVarJobStatus:
   case bDirVarPriority:
   case bDirVarFDJobStatus:
   case bDirVarSDJobStatus: {
      int value = 0;

      ctx = PyGetbpContext(pyCtx);
      if (bfuncs->getBareosValue(ctx, (brDirVariable)var, &value) == bRC_OK) {
         pRetVal = PyInt_FromLong(value);
      }
      break;
   }
   case bDirVarJobErrors:
   case bDirVarSDErrors:
   case bDirVarJobFiles:
   case bDirVarSDJobFiles:
   case bDirVarLastRate:
   case bDirVarJobBytes:
   case bDirVarReadBytes: {
      uint64_t value = 0;

      ctx = PyGetbpContext(pyCtx);
      if (bfuncs->getBareosValue(ctx, (brDirVariable)var, &value) == bRC_OK) {
         pRetVal = PyLong_FromUnsignedLong(value);
      }
      break;
   }
   case bDirVarJobName:
   case bDirVarJob:
   case bDirVarClient:
   case bDirVarPool:
   case bDirVarStorage:
   case bDirVarWriteStorage:
   case bDirVarReadStorage:
   case bDirVarCatalog:
   case bDirVarMediaType:
   case bDirVarVolumeName: {
      char *value;

      ctx = PyGetbpContext(pyCtx);
      if (bfuncs->getBareosValue(ctx, (brDirVariable)var, &value) == bRC_OK) {
         pRetVal = PyString_FromString(value);
      }
      break;
   }
   case bDirVarPluginDir: {
      char *value;

      if (bfuncs->getBareosValue(NULL, (brDirVariable)var, &value) == bRC_OK) {
         pRetVal = PyString_FromString(value);
      }
      break;
   }
   default:
      ctx = PyGetbpContext(pyCtx);
      Dmsg(ctx, dbglvl, "PyBareosGetValue: Unknown variable requested %d\n", var);
      break;
   }

   if (!pRetVal) {
      Py_INCREF(Py_None);
      pRetVal = Py_None;
   }

   return pRetVal;
}

/*
 * Callback function which is exposed as a part of the additional methods which allow
 * a Python plugin to set certain internal values of the current Job.
 */
static PyObject *PyBareosSetValue(PyObject *self, PyObject *args)
{
   int var;
   bpContext *ctx = NULL;
   PyObject *pyCtx, *pyValue;

   if (!PyArg_ParseTuple(args, "OiO:BareosSetValue", &pyCtx, &var, &pyValue)) {
      return NULL;
   }

   switch (var) {
   case bwDirVarVolumeName: {
      char *value;

      ctx = PyGetbpContext(pyCtx);
      value = PyString_AsString(pyValue);
      if (value) {
         bfuncs->setBareosValue(ctx, (bwDirVariable)var, value);
      }

      break;
   }
   case bwDirVarPriority:
   case bwDirVarJobLevel: {
      int value;

      ctx = PyGetbpContext(pyCtx);
      value = PyInt_AsLong(pyValue);
      if (value >= 0) {
         bfuncs->setBareosValue(ctx, (bwDirVariable)var, &value);
      }
      break;
   }
   default:
      ctx = PyGetbpContext(pyCtx);
      Dmsg(ctx, dbglvl, "PyBareosSetValue: Unknown variable requested %d\n", var);
      break;
   }

   Py_INCREF(Py_None);
   return Py_None;
}

/*
 * Callback function which is exposed as a part of the additional methods which allow
 * a Python plugin to issue debug messages using the Bareos debug message facility.
 */
static PyObject *PyBareosDebugMessage(PyObject *self, PyObject *args)
{
   int level;
   char *dbgmsg = NULL;
   bpContext *ctx;
   PyObject *pyCtx;

   if (!PyArg_ParseTuple(args, "Oi|z:BareosDebugMessage", &pyCtx, &level, &dbgmsg)) {
      return NULL;
   }

   if (dbgmsg) {
      ctx = PyGetbpContext(pyCtx);
      Dmsg(ctx, level, dbgmsg);
   }

   Py_INCREF(Py_None);
   return Py_None;
}

/*
 * Callback function which is exposed as a part of the additional methods which allow
 * a Python plugin to issue Job messages using the Bareos Job message facility.
 */
static PyObject *PyBareosJobMessage(PyObject *self, PyObject *args)
{
   int type;
   char *jobmsg = NULL;
   bpContext *ctx;
   PyObject *pyCtx;

   if (!PyArg_ParseTuple(args, "Oi|z:BareosJobMessage", &pyCtx, &type, &jobmsg)) {
      return NULL;
   }

   if (jobmsg) {
      ctx = PyGetbpContext(pyCtx);
      Jmsg(ctx, type, jobmsg);
   }

   Py_INCREF(Py_None);
   return Py_None;
}

/*
 * Callback function which is exposed as a part of the additional methods which allow
 * a Python plugin to issue a Register Event to register additional events it wants
 * to receive.
 */
static PyObject *PyBareosRegisterEvents(PyObject *self, PyObject *args)
{
   int len, event;
   bpContext *ctx;
   PyObject *pyCtx, *pyEvents, *pySeq, *pyEvent;

   if (!PyArg_ParseTuple(args, "OO:BareosRegisterEvents", &pyCtx, &pyEvents)) {
      return NULL;
   }

   pySeq = PySequence_Fast(pyEvents, "Expected a sequence of events");
   if (!pySeq) {
      return NULL;
   }

   len = PySequence_Fast_GET_SIZE(pySeq);

   ctx = PyGetbpContext(pyCtx);
   for (int i = 0; i < len; i++) {
      pyEvent = PySequence_Fast_GET_ITEM(pySeq, i);
      event = PyInt_AsLong(pyEvent);

      if (event >= bDirEventJobStart && event <= bDirEventGetScratch) {
         Dmsg(ctx, dbglvl, "PyBareosRegisterEvents: registering event %d\n", event);
         bfuncs->registerBareosEvents(ctx, 1, event);
      }
   }

   Py_DECREF(pySeq);

   Py_INCREF(Py_None);
   return Py_None;
}
