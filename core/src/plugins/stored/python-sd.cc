/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2011-2014 Planets Communications B.V.
   Copyright (C) 2013-2016 Bareos GmbH & Co. KG

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
 * Marco van Wieringen, August 2012
 */
/**
 * @file
 * Python Storage daemon Plugin program
 */
#include "include/bareos.h"
#include "stored/stored.h"

#undef _POSIX_C_SOURCE
#include <Python.h>

#if (PY_VERSION_HEX <  0x02060000)
#error "Need at least Python version 2.6 or newer"
#endif

static const int debuglevel = 150;

#define PLUGIN_LICENSE      "Bareos AGPLv3"
#define PLUGIN_AUTHOR       "Marco van Wieringen"
#define PLUGIN_DATE         "October 2013"
#define PLUGIN_VERSION      "3"
#define PLUGIN_DESCRIPTION  "Python Storage Daemon Plugin"
#define PLUGIN_USAGE        "python:instance=<instance_id>:module_path=<path-to-python-modules>:module_name=<python-module-to-load>"

#define Dmsg(context, level,  ...) bfuncs->DebugMessage(context, __FILE__, __LINE__, level, __VA_ARGS__ )
#define Jmsg(context, type,  ...) bfuncs->JobMessage(context, __FILE__, __LINE__, type, 0, __VA_ARGS__ )

/* Forward referenced functions */
static bRC newPlugin(bpContext *ctx);
static bRC freePlugin(bpContext *ctx);
static bRC getPluginValue(bpContext *ctx, psdVariable var, void *value);
static bRC setPluginValue(bpContext *ctx, psdVariable var, void *value);
static bRC handlePluginEvent(bpContext *ctx, bsdEvent *event, void *value);
static bRC parse_plugin_definition(bpContext *ctx, void *value, PoolMem &plugin_options);

static void PyErrorHandler(bpContext *ctx, int msgtype);
static bRC PyLoadModule(bpContext *ctx, void *value);
static bRC PyParsePluginDefinition(bpContext *ctx, void *value);
static bRC PyGetPluginValue(bpContext *ctx, psdVariable var, void *value);
static bRC PySetPluginValue(bpContext *ctx, psdVariable var, void *value);
static bRC PyHandlePluginEvent(bpContext *ctx, bsdEvent *event, void *value);

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

/**
 * Plugin private context
 */
struct plugin_ctx {
   int64_t instance;                  /* Instance number of plugin */
   bool python_loaded;                /* Plugin has python module loaded ? */
   bool python_path_set;              /* Python plugin search path is set ? */
   char *module_path;                 /* Plugin Module Path */
   char *module_name;                 /* Plugin Module Name */
   PyThreadState *interpreter;        /* Python interpreter for this instance of the plugin */
   PyObject *pInstance;               /* Python Module instance */
   PyObject *pModule;                 /* Python Module entry point */
   PyObject *pDict;                   /* Python Dictionary */
   PyObject *bpContext;               /* Python representation of plugin context */
};

#include "python-sd.h"
#include "lib/edit.h"

/**
 * We don't actually use this but we need it to tear down the
 * final python interpreter on unload of the plugin. Each instance of
 * the plugin get its own interpreter.
 */
static PyThreadState *mainThreadState;

#ifdef __cplusplus
extern "C" {
#endif

/**
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

   /*
    * Setup Python
    */
   Py_InitializeEx(0);
   PyEval_InitThreads();
   mainThreadState = PyEval_SaveThread();

   return bRC_OK;
}

/**
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

/**
 * The following entry points are accessed through the function
 * pointers we supplied to Bareos. Each plugin type (dir, fd, sd)
 * has its own set of entry points that the plugin must define.
 */
/**
 * Create a new instance of the plugin i.e. allocate our private storage
 */
static bRC newPlugin(bpContext *ctx)
{
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

   /*
    * Always register some events the python plugin itself can register
    * any other events it is interested in.
    */
   bfuncs->registerBareosEvents(ctx,
                                1,
                                bsdEventNewPluginOptions);

   return bRC_OK;
}

/**
 * Free a plugin instance, i.e. release our private storage
 */
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

/**
 * Return some plugin value (none defined)
 */
static bRC getPluginValue(bpContext *ctx, psdVariable var, void *value)
{
   struct plugin_ctx *p_ctx = (struct plugin_ctx *)ctx->pContext;
   bRC retval = bRC_Error;

   PyEval_AcquireThread(p_ctx->interpreter);
   retval = PyGetPluginValue(ctx, var, value);
   PyEval_ReleaseThread(p_ctx->interpreter);

   return retval;
}

/**
 * Set a plugin value (none defined)
 */
static bRC setPluginValue(bpContext *ctx, psdVariable var, void *value)
{
   struct plugin_ctx *p_ctx = (struct plugin_ctx *)ctx->pContext;
   bRC retval = bRC_Error;

   PyEval_AcquireThread(p_ctx->interpreter);
   retval = PySetPluginValue(ctx, var, value);
   PyEval_ReleaseThread(p_ctx->interpreter);

   return retval;
}

/**
 * Handle an event that was generated in Bareos
 */
static bRC handlePluginEvent(bpContext *ctx, bsdEvent *event, void *value)
{
   bRC retval = bRC_Error;
   bool event_dispatched = false;
   PoolMem plugin_options(PM_FNAME);
   plugin_ctx *p_ctx = (plugin_ctx *)ctx->pContext;

   if (!p_ctx) {
      goto bail_out;
   }

   /*
    * First handle some events internally before calling python if it
    * want to do some special handling on the event triggered.
    */
   switch (event->eventType) {
   case bsdEventNewPluginOptions:
      event_dispatched = true;
      retval = parse_plugin_definition(ctx, value, plugin_options);
      break;
   default:
      break;
   }

   /*
    * See if we have been triggered in the previous switch if not we have to
    * always dispatch the event. If we already processed the event internally
    * we only do a dispatch to the python entry point when that internal processing
    * was successfull (e.g. retval == bRC_OK).
    */
   if (!event_dispatched || retval == bRC_OK) {
      PyEval_AcquireThread(p_ctx->interpreter);

      /*
       * Now dispatch the event to Python.
       * First the calls that need special handling.
       */
      switch (event->eventType) {
      case bsdEventNewPluginOptions:
         /*
          * See if we already loaded the Python modules.
          */
         if (!p_ctx->python_loaded) {
            retval = PyLoadModule(ctx, plugin_options.c_str());
         }

         /*
          * Only try to call when the loading succeeded.
          */
         if (retval == bRC_OK) {
            retval = PyParsePluginDefinition(ctx, plugin_options.c_str());
         }
         break;
      default:
         /*
          * Handle the generic events e.g. the ones which are just passed on.
          * We only try to call Python when we loaded the right module until
          * that time we pretend the call succeeded.
          */
         if (p_ctx->python_loaded) {
            retval = PyHandlePluginEvent(ctx, event, value);
         } else {
            retval = bRC_OK;
         }
         break;
      }

      PyEval_ReleaseThread(p_ctx->interpreter);
   }

bail_out:
   return retval;
}

/**
 * Strip any backslashes in the string.
 */
static inline void strip_back_slashes(char *value)
{
   char *bp;

   bp = value;
   while (*bp) {
      switch (*bp) {
      case '\\':
         bstrinlinecpy(bp, bp + 1);
         break;
      default:
         break;
      }

      bp++;
   }
}

/**
 * Parse a integer value.
 */
static inline int64_t parse_integer(const char *argument_value)
{
   return str_to_int64(argument_value);
}

/**
 * Parse a boolean value e.g. check if its yes or true anything else translates to false.
 */
static inline bool parse_boolean(const char *argument_value)
{
   if (bstrcasecmp(argument_value, "yes") ||
       bstrcasecmp(argument_value, "true")) {
      return true;
   } else {
      return false;
   }
}

/**
 * Always set destination to value and clean any previous one.
 */
static inline void set_string(char **destination, char *value)
{
   if (*destination) {
      free(*destination);
   }

   *destination = bstrdup(value);
   strip_back_slashes(*destination);
}

/**
 * Parse the plugin definition passed in.
 *
 * The definition is in this form:
 *
 * python:module_path=<path>:module_name=<python_module_name>:...
 */
static bRC parse_plugin_definition(bpContext *ctx, void *value, PoolMem &plugin_options)
{
   bool found;
   int i, cnt;
   PoolMem plugin_definition(PM_FNAME);
   char *bp, *argument, *argument_value;
   plugin_ctx *p_ctx = (plugin_ctx *)ctx->pContext;

   if (!value) {
      return bRC_Error;
   }

   /*
    * Parse the plugin definition.
    * Make a private copy of the whole string.
    */
   pm_strcpy(plugin_definition, (char *)value);

   bp = strchr(plugin_definition.c_str(), ':');
   if (!bp) {
      Jmsg(ctx, M_FATAL, "python-sd: Illegal plugin definition %s\n", plugin_definition.c_str());
      Dmsg(ctx, debuglevel, "python-sd: Illegal plugin definition %s\n", plugin_definition.c_str());
      goto bail_out;
   }

   /*
    * Skip the first ':'
    */
   bp++;

   cnt = 0;
   while (bp) {
      if (strlen(bp) == 0) {
         break;
      }

      /*
       * Each argument is in the form:
       *    <argument> = <argument_value>
       *
       * So we setup the right pointers here, argument to the beginning
       * of the argument, argument_value to the beginning of the argument_value.
       */
      argument = bp;
      argument_value = strchr(bp, '=');
      if (!argument_value) {
         Jmsg(ctx, M_FATAL, "python-sd: Illegal argument %s without value\n", argument);
         Dmsg(ctx, debuglevel, "python-sd: Illegal argument %s without value\n", argument);
         goto bail_out;
      }
      *argument_value++ = '\0';

      /*
       * See if there are more arguments and setup for the next run.
       */
      bp = argument_value;
      do {
         bp = strchr(bp, ':');
         if (bp) {
            if (*(bp - 1) != '\\') {
               *bp++ = '\0';
               break;
            } else {
               bp++;
            }
         }
      } while (bp);

      found = false;
      for (i = 0; plugin_arguments[i].name; i++) {
         if (bstrcasecmp(argument, plugin_arguments[i].name)) {
            int64_t *int_destination = NULL;
            char **str_destination = NULL;
            bool *bool_destination = NULL;

            switch (plugin_arguments[i].type) {
            case argument_instance:
               int_destination = &p_ctx->instance;
               break;
            case argument_module_path:
               str_destination = &p_ctx->module_path;
               break;
            case argument_module_name:
               str_destination = &p_ctx->module_name;
               break;
            default:
               break;
            }

            if (int_destination) {
               *int_destination = parse_integer(argument_value);
            }

            if (str_destination) {
               set_string(str_destination, argument_value);
            }

            if (bool_destination) {
               *bool_destination = parse_boolean(argument_value);
            }

            /*
             * When we have a match break the loop.
             */
            found = true;
            break;
         }
      }

      /*
       * If we didn't consume this parameter we add it to the plugin_options list.
       */
      if (!found) {
         PoolMem option(PM_FNAME);

         if (cnt) {
            Mmsg(option, ":%s=%s", argument, argument_value);
            pm_strcat(plugin_options, option.c_str());
         } else {
            Mmsg(option, "%s=%s", argument, argument_value);
            pm_strcat(plugin_options, option.c_str());
         }
         cnt++;
      }
   }

   if (cnt > 0) {
      pm_strcat(plugin_options, ":");
   }

   return bRC_OK;

bail_out:
   return bRC_Error;
}

/**
 * Work around API changes in Python versions.
 * These function abstract the storage and retrieval of the bpContext
 * which is passed to the Python methods and which the method can pass
 * back and which allow the callback function to understand what bpContext
 * its talking about.
 */
#if ((PY_VERSION_HEX < 0x02070000) || \
     ((PY_VERSION_HEX >= 0x03000000) && \
      (PY_VERSION_HEX < 0x03010000)))
/**
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
/**
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

/**
 * Convert a return value into a bRC enum value.
 */
static inline bRC conv_python_retval(PyObject *pRetVal)
{
   return (bRC)PyInt_AsLong(pRetVal);
}

/**
 * Convert a return value from bRC enum value into Python Object.
 */
static inline PyObject *conv_retval_python(bRC retval)
{
   return (PyObject *)PyInt_FromLong((int)retval);
}

/**
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

   Dmsg(ctx, debuglevel, "python-sd: %s\n", error_string);
   if (msgtype) {
      Jmsg(ctx, msgtype, "python-sd: %s\n", error_string);
   }

   free(error_string);
}

/**
 * Initial load of the Python module.
 *
 * Based on the parsed plugin options we set some prerequisits like the
 * module path and the module to load. We also load the dictionary used
 * for looking up the Python methods.
 */
static bRC PyLoadModule(bpContext *ctx, void *value)
{
   bRC retval = bRC_Error;
   struct plugin_ctx *p_ctx = (struct plugin_ctx *)ctx->pContext;
   PyObject *sysPath,
            *mPath,
            *pName,
            *pFunc;

   /*
    * See if we already setup the python search path.
    */
   if (!p_ctx->python_path_set) {
      /*
       * Extend the Python search path with the given module_path.
       */
      if (p_ctx->module_path) {
         sysPath = PySys_GetObject((char *)"path");
         mPath = PyString_FromString(p_ctx->module_path);
         PyList_Append(sysPath, mPath);
         Py_DECREF(mPath);
         p_ctx->python_path_set = true;
      }
   }

   /*
    * See if we already setup the module structure.
    */
   if (!p_ctx->pInstance) {
      /*
       * Make our callback methods available for Python.
       */
      p_ctx->pInstance = Py_InitModule("bareossd", BareosSDMethods);
   }

   /*
    * Try to load the Python module by name.
    */
   if (p_ctx->module_name) {
      Dmsg(ctx, debuglevel, "python-sd: Trying to load module with name %s\n", p_ctx->module_name);
      pName = PyString_FromString(p_ctx->module_name);
      p_ctx->pModule = PyImport_Import(pName);
      Py_DECREF(pName);

      if (!p_ctx->pModule) {
         Dmsg(ctx, debuglevel, "python-sd: Failed to load module with name %s\n", p_ctx->module_name);
         goto bail_out;
      }

      Dmsg(ctx, debuglevel, "python-sd: Successfully loaded module with name %s\n", p_ctx->module_name);

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
         PyObject *pPluginDefinition,
                  *pRetVal;

         pPluginDefinition = PyString_FromString((char *)value);
         if (!pPluginDefinition) {
            goto bail_out;
         }

         pRetVal = PyObject_CallFunctionObjArgs(pFunc, p_ctx->bpContext, pPluginDefinition, NULL);
         Py_DECREF(pPluginDefinition);

         if (!pRetVal) {
            goto bail_out;
         } else {
            retval = conv_python_retval(pRetVal);
            Py_DECREF(pRetVal);
         }
      } else {
         Dmsg(ctx, debuglevel, "python-sd: Failed to find function named load_bareos_plugins()\n");
         goto bail_out;
      }

      /*
       * Keep track we successfully loaded.
       */
      p_ctx->python_loaded = true;
   }

   return retval;

bail_out:
   if (PyErr_Occurred()) {
      PyErrorHandler(ctx, M_FATAL);
   }

   return retval;
}

/**
 * Any plugin options which are passed in are dispatched here to a Python method and it
 * can parse the plugin options. This function is also called after PyLoadModule() has
 * loaded the Python module and made sure things are operational.
 */
static bRC PyParsePluginDefinition(bpContext *ctx, void *value)
{
   bRC retval = bRC_Error;
   struct plugin_ctx *p_ctx = (struct plugin_ctx *)ctx->pContext;
   PyObject *pFunc;

   /*
    * Lookup the parse_plugin_definition() function in the python module.
    */
   pFunc = PyDict_GetItemString(p_ctx->pDict, "parse_plugin_definition"); /* Borrowed reference */
   if (pFunc && PyCallable_Check(pFunc)) {
      PyObject *pPluginDefinition,
               *pRetVal;

      pPluginDefinition = PyString_FromString((char *)value);
      if (!pPluginDefinition) {
         goto bail_out;
      }

      pRetVal = PyObject_CallFunctionObjArgs(pFunc, p_ctx->bpContext, pPluginDefinition, NULL);
      Py_DECREF(pPluginDefinition);

      if (!pRetVal) {
         goto bail_out;
      } else {
         retval = conv_python_retval(pRetVal);
         Py_DECREF(pRetVal);
      }

      return retval;
   } else {
      Dmsg(ctx, debuglevel, "python-sd: Failed to find function named parse_plugin_definition()\n");
      return bRC_Error;
   }

bail_out:
   if (PyErr_Occurred()) {
      PyErrorHandler(ctx, M_FATAL);
   }

   return retval;
}

static bRC PyGetPluginValue(bpContext *ctx, psdVariable var, void *value)
{
   return bRC_OK;
}

static bRC PySetPluginValue(bpContext *ctx, psdVariable var, void *value)
{
   return bRC_OK;
}

static bRC PyHandlePluginEvent(bpContext *ctx, bsdEvent *event, void *value)
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
      Dmsg(ctx, debuglevel, "python-sd: Failed to find function named handle_plugin_event()\n");
   }

   return retval;

bail_out:
   if (PyErr_Occurred()) {
      PyErrorHandler(ctx, M_FATAL);
   }

   return retval;
}

/**
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
   case bsdVarJobId:
   case bsdVarLevel:
   case bsdVarType:
   case bsdVarJobStatus: {
      int value;

      ctx = PyGetbpContext(pyCtx);
      if (bfuncs->getBareosValue(ctx, (bsdrVariable)var, &value) == bRC_OK) {
         pRetVal = PyInt_FromLong(value);
      }
      break;
   }
   case bsdVarJobErrors:
   case bsdVarJobFiles:
   case bsdVarJobBytes: {
      uint64_t value = 0;

      ctx = PyGetbpContext(pyCtx);
      if (bfuncs->getBareosValue(ctx, (bsdrVariable)var, &value) == bRC_OK) {
         pRetVal = PyLong_FromUnsignedLong(value);
      }
      break;
   }
   case bsdVarJobName:
   case bsdVarJob:
   case bsdVarClient:
   case bsdVarPool:
   case bsdVarPoolType:
   case bsdVarStorage:
   case bsdVarMediaType:
   case bsdVarVolumeName: {
      char *value = NULL;

      ctx = PyGetbpContext(pyCtx);
      if (bfuncs->getBareosValue(ctx, (bsdrVariable)var, &value) == bRC_OK) {
         if (value) {
            pRetVal = PyString_FromString(value);
         }
      }
      break;
   }
   case bsdVarCompatible: {
      bool value;

      if (bfuncs->getBareosValue(NULL, (bsdrVariable)var, &value) == bRC_OK) {
         long bool_value;

         bool_value = (value) ? 1 : 0;
         pRetVal = PyBool_FromLong(bool_value);
      }
      break;
   }
   case bsdVarPluginDir: {
      char *value = NULL;

      if (bfuncs->getBareosValue(NULL, (bsdrVariable)var, &value) == bRC_OK) {
         if (value) {
            pRetVal = PyString_FromString(value);
         }
      }
      break;
   }
   default:
      ctx = PyGetbpContext(pyCtx);
      Dmsg(ctx, debuglevel, "python-sd: PyBareosGetValue unknown variable requested %d\n", var);
      break;
   }

   if (!pRetVal) {
      Py_INCREF(Py_None);
      pRetVal = Py_None;
   }

   return pRetVal;
}

/**
 * Callback function which is exposed as a part of the additional methods which allow
 * a Python plugin to get certain internal values of the current Job.
 */
static PyObject *PyBareosSetValue(PyObject *self, PyObject *args)
{
   int var;
   bpContext *ctx = NULL;
   bRC retval = bRC_Error;
   PyObject *pyCtx, *pyValue;

   if (!PyArg_ParseTuple(args, "OiO:BareosSetValue", &pyCtx, &var, &pyValue)) {
      goto bail_out;
   }

   switch (var) {
   case bsdwVarVolumeName: {
      char *value;

      ctx = PyGetbpContext(pyCtx);
      value = PyString_AsString(pyValue);
      if (value) {
         bfuncs->setBareosValue(ctx, (bsdwVariable)var, value);
      }

      break;
   }
   case bsdwVarPriority:
   case bsdwVarJobLevel: {
      int value;

      ctx = PyGetbpContext(pyCtx);
      value = PyInt_AsLong(pyValue);
      if (value >= 0) {
         retval = bfuncs->setBareosValue(ctx, (bsdwVariable)var, &value);
      }
      break;
   }
   default:
      ctx = PyGetbpContext(pyCtx);
      Dmsg(ctx, debuglevel, "python-sd: PyBareosSetValue unknown variable requested %d\n", var);
      break;
   }

bail_out:
   return conv_retval_python(retval);
}

/**
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
      Dmsg(ctx, level, "python-sd: %s", dbgmsg);
   }

   Py_INCREF(Py_None);
   return Py_None;
}

/**
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
      Jmsg(ctx, type, "python-sd: %s", jobmsg);
   }

   Py_INCREF(Py_None);
   return Py_None;
}

/**
 * Callback function which is exposed as a part of the additional methods which allow
 * a Python plugin to issue a Register Event to register additional events it wants
 * to receive.
 */
static PyObject *PyBareosRegisterEvents(PyObject *self, PyObject *args)
{
   int len, event;
   bpContext *ctx;
   bRC retval = bRC_Error;
   PyObject *pyCtx, *pyEvents, *pySeq, *pyEvent;

   if (!PyArg_ParseTuple(args, "OO:BareosRegisterEvents", &pyCtx, &pyEvents)) {
      goto bail_out;
   }

   pySeq = PySequence_Fast(pyEvents, "Expected a sequence of events");
   if (!pySeq) {
      goto bail_out;
   }

   len = PySequence_Fast_GET_SIZE(pySeq);

   ctx = PyGetbpContext(pyCtx);
   for (int i = 0; i < len; i++) {
      pyEvent = PySequence_Fast_GET_ITEM(pySeq, i);
      event = PyInt_AsLong(pyEvent);

      if (event >= bsdEventJobStart && event <= bsdEventWriteRecordTranslation) {
         Dmsg(ctx, debuglevel, "python-sd: PyBareosRegisterEvents registering event %d\n", event);
         retval = bfuncs->registerBareosEvents(ctx, 1, event);

         if (retval != bRC_OK) {
            break;
         }
      }
   }

   Py_DECREF(pySeq);

bail_out:
   return conv_retval_python(retval);
}

/**
 * Callback function which is exposed as a part of the additional methods which allow
 * a Python plugin to issue an Unregister Event to unregister events it doesn't want to
 * receive anymore.
 */
static PyObject *PyBareosUnRegisterEvents(PyObject *self, PyObject *args)
{
   int len, event;
   bpContext *ctx;
   bRC retval = bRC_Error;
   PyObject *pyCtx, *pyEvents, *pySeq, *pyEvent;

   if (!PyArg_ParseTuple(args, "OO:BareosUnRegisterEvents", &pyCtx, &pyEvents)) {
      goto bail_out;
   }

   pySeq = PySequence_Fast(pyEvents, "Expected a sequence of events");
   if (!pySeq) {
      goto bail_out;
   }

   len = PySequence_Fast_GET_SIZE(pySeq);

   ctx = PyGetbpContext(pyCtx);
   for (int i = 0; i < len; i++) {
      pyEvent = PySequence_Fast_GET_ITEM(pySeq, i);
      event = PyInt_AsLong(pyEvent);

      if (event >= bsdEventJobStart && event <= bsdEventWriteRecordTranslation) {
         Dmsg(ctx, debuglevel, "PyBareosUnRegisterEvents: registering event %d\n", event);
         retval = bfuncs->unregisterBareosEvents(ctx, 1, event);

         if (retval != bRC_OK) {
            break;
         }
      }
   }

   Py_DECREF(pySeq);

bail_out:
   return conv_retval_python(retval);
}

/**
 * Callback function which is exposed as a part of the additional methods which allow
 * a Python plugin to issue a GetInstanceCount to retrieve the number of instances of
 * the current plugin being loaded into the daemon.
 */
static PyObject *PyBareosGetInstanceCount(PyObject *self, PyObject *args)
{
   int value;
   bpContext *ctx = NULL;
   PyObject *pyCtx;
   PyObject *pRetVal = NULL;

   if (!PyArg_ParseTuple(args, "O:BareosGetInstanceCount", &pyCtx)) {
      return NULL;
   }

   ctx = PyGetbpContext(pyCtx);
   if (bfuncs->getInstanceCount(ctx, &value) == bRC_OK) {
      pRetVal = PyInt_FromLong(value);
   }

   if (!pRetVal) {
      Py_INCREF(Py_None);
      pRetVal = Py_None;
   }

   return pRetVal;
}
