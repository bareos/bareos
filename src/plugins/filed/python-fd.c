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
 * Python plugin for the Bareos File Daemon
 *
 * Marco van Wieringen, August 2012
 */
#define BUILD_PLUGIN
#define BUILDING_DLL            /* required for Windows plugin */

#include "bareos.h"
#include "fd_plugins.h"
#include "fd_common.h"

#ifdef HAVE_PYTHON
#undef _POSIX_C_SOURCE
#include <Python.h>

#if (PY_VERSION_HEX <  0x02060000)
#error "Need at least Python version 2.6 or newer"
#endif
#endif

static const int dbglvl = 150;

#define PLUGIN_LICENSE      "Bareos AGPLv3"
#define PLUGIN_AUTHOR       "Marco van Wieringen"
#define PLUGIN_DATE         "October 2013"
#define PLUGIN_VERSION      "2"
#define PLUGIN_DESCRIPTION  "Python File Daemon Plugin"
#define PLUGIN_USAGE        "python:module_path=<path-to-python-modules>:module_name=<python-module-to-load>"

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

#ifdef HAVE_PYTHON
static bRC parse_plugin_definition(bpContext *ctx, void *value);

static void PyErrorHandler(bpContext *ctx, int msgtype);
static bRC PyLoadModule(bpContext *ctx, void *value);
static bRC PyParsePluginDefinition(bpContext *ctx, void *value);
static bRC PyGetPluginValue(bpContext *ctx, pVariable var, void *value);
static bRC PySetPluginValue(bpContext *ctx, pVariable var, void *value);
static bRC PyHandlePluginEvent(bpContext *ctx, bEvent *event, void *value);
static bRC PyStartBackupFile(bpContext *ctx, struct save_pkt *sp);
static bRC PyEndBackupFile(bpContext *ctx);
static bRC PyPluginIO(bpContext *ctx, struct io_pkt *io);
static bRC PyStartRestoreFile(bpContext *ctx, const char *cmd);
static bRC PyEndRestoreFile(bpContext *ctx);
static bRC PyCreateFile(bpContext *ctx, struct restore_pkt *rp);
static bRC PySetFileAttributes(bpContext *ctx, struct restore_pkt *rp);
static bRC PyCheckFile(bpContext *ctx, char *fname);
static bRC PyRestoreObjectData(bpContext *ctx, restore_object_pkt *rop);
#endif

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
   int32_t backup_level;
#ifdef HAVE_PYTHON
   bool python_loaded;
   char *module_path;
   char *module_name;
   char *fname;
   char *link;
   char *object_name;
   char *object;
   PyThreadState *interpreter;
   PyObject *pModule;
   PyObject *pDict;
   PyObject *bpContext;
#endif
};

#ifdef HAVE_PYTHON
#include "python-fd.h"
#endif

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
bRC DLL_IMP_EXP loadPlugin(bInfo *lbinfo,
                           bFuncs *lbfuncs,
                           genpInfo **pinfo,
                           pFuncs **pfuncs)
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
 * Plugin called here when it is unloaded, normally when Bareos is going to exit.
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
 * a new Job is started. There can be multiple instances of
 * each plugin that are running at the same time.  Your
 * plugin instance must be thread safe and keep its own
 * local data.
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

   /*
    * Always register some events the python plugin itself can register
    * any other events it is interested in.
    */
   bfuncs->registerBareosEvents(ctx,
                                5,
                                bEventBackupCommand,
                                bEventRestoreCommand,
                                bEventEstimateCommand,
                                bEventLevel,
                                bEventPluginCommand);

   return bRC_OK;
}

/*
 * Release everything concerning a particular instance of a
 * plugin. Normally called when the Job terminates.
 */
static bRC freePlugin(bpContext *ctx)
{
   struct plugin_ctx *p_ctx = (struct plugin_ctx *)ctx->pContext;

   if (!p_ctx) {
      return bRC_Error;
   }

#ifdef HAVE_PYTHON
   if (p_ctx->module_path) {
      free(p_ctx->module_path);
   }

   if (p_ctx->module_name) {
      free(p_ctx->module_name);
   }

   if (p_ctx->fname) {
      free(p_ctx->fname);
   }

   if (p_ctx->link) {
      free(p_ctx->link);
   }

   if (p_ctx->object_name) {
      free(p_ctx->object_name);
   }

   if (p_ctx->object) {
      free(p_ctx->object);
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
#endif

   free(p_ctx);
   ctx->pContext = NULL;

   return bRC_OK;
}

/*
 * Called by core code to get a variable from the plugin.
 * Not currently used.
 */
static bRC getPluginValue(bpContext *ctx, pVariable var, void *value)
{
   struct plugin_ctx *p_ctx = (struct plugin_ctx *)ctx->pContext;
   bRC retval = bRC_Error;

   if (!p_ctx) {
      goto bail_out;
   }

#ifdef HAVE_PYTHON
   PyEval_AcquireThread(p_ctx->interpreter);
   retval = PyGetPluginValue(ctx, var, value);
   PyEval_ReleaseThread(p_ctx->interpreter);
#endif

bail_out:
   return retval;
}

/*
 * Called by core code to set a plugin variable.
 * Not currently used.
 */
static bRC setPluginValue(bpContext *ctx, pVariable var, void *value)
{
   struct plugin_ctx *p_ctx = (struct plugin_ctx *)ctx->pContext;
   bRC retval = bRC_Error;

   if (!p_ctx) {
      return bRC_Error;
   }

#ifdef HAVE_PYTHON
   PyEval_AcquireThread(p_ctx->interpreter);
   retval = PySetPluginValue(ctx, var, value);
   PyEval_ReleaseThread(p_ctx->interpreter);
#endif

   return retval;
}

/*
 * Called by Bareos when there are certain events that the
 * plugin might want to know.  The value depends on the event.
 */
static bRC handlePluginEvent(bpContext *ctx, bEvent *event, void *value)
{
#ifdef HAVE_PYTHON
   bool event_dispatched = false;
   plugin_ctx *p_ctx = (plugin_ctx *)ctx->pContext;
   bRC retval = bRC_Error;

   if (!p_ctx) {
      goto bail_out;
   }

   /*
    * First handle some events internally before calling python if it
    * want to do some special handling on the event triggered.
    */
   switch (event->eventType) {
   case bEventLevel:
      p_ctx->backup_level = (int64_t)value;
      break;
   case bEventBackupCommand:
     /*
      * Fall-through wanted
      */
   case bEventRestoreCommand:
     /*
      * Fall-through wanted
      */
   case bEventEstimateCommand:
     /*
      * Fall-through wanted
      */
   case bEventPluginCommand:
      event_dispatched = true;
      retval = parse_plugin_definition(ctx, value);
   default:
      break;
   }

   /*
    * See if we have been triggered in the previous switch of not we have to
    * do out work here and if we did we need to be sure the parse_plugin_definition
    * returned bRC_OK indicating the plugin definition was OK.
    */
   if (!event_dispatched || retval == bRC_OK) {
      PyEval_AcquireThread(p_ctx->interpreter);

      /*
       * Now dispatch the event to Python.
       * First the calls that need special handling.
       */
      switch (event->eventType) {
      case bEventBackupCommand:
        /*
         * Fall-through wanted
         */
      case bEventRestoreCommand:
        /*
         * Fall-through wanted
         */
      case bEventEstimateCommand:
        /*
         * Fall-through wanted
         */
      case bEventPluginCommand:
         /*
          * See if we already loaded the Python modules.
          */
         if (!p_ctx->python_loaded) {
            retval = PyLoadModule(ctx, value);
         }

         /*
          * Only try to call when the loading succeeded.
          */
         if (retval == bRC_OK) {
            retval = PyParsePluginDefinition(ctx, value);
         }
         break;
      case bEventRestoreObject:
         retval = PyRestoreObjectData(ctx, (restore_object_pkt *)value);
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
#else
   return bRC_OK;
#endif
}

/*
 * Called when starting to backup a file. Here the plugin must
 * return the "stat" packet for the directory/file and provide
 * certain information so that Bareos knows what the file is.
 * The plugin can create "Virtual" files by giving them a
 * name that is not normally found on the file system.
 */
static bRC startBackupFile(bpContext *ctx, struct save_pkt *sp)
{
   bRC retval = bRC_Error;
   struct plugin_ctx *p_ctx = (struct plugin_ctx *)ctx->pContext;

   if (!p_ctx) {
      goto bail_out;
   }

#ifdef HAVE_PYTHON
   PyEval_AcquireThread(p_ctx->interpreter);
   retval = PyStartBackupFile(ctx, sp);
   PyEval_ReleaseThread(p_ctx->interpreter);
#endif

bail_out:
   return retval;
}

/*
 * Done backing up a file.
 */
static bRC endBackupFile(bpContext *ctx)
{
   bRC retval = bRC_Error;
   struct plugin_ctx *p_ctx = (struct plugin_ctx *)ctx->pContext;

   if (!p_ctx) {
      goto bail_out;
   }

#ifdef HAVE_PYTHON
   PyEval_AcquireThread(p_ctx->interpreter);
   retval = PyEndBackupFile(ctx);
   PyEval_ReleaseThread(p_ctx->interpreter);
#endif

bail_out:
   return retval;
}

/*
 * Do actual I/O. Bareos calls this after startBackupFile
 * or after startRestoreFile to do the actual file
 * input or output.
 */
static bRC pluginIO(bpContext *ctx, struct io_pkt *io)
{
   bRC retval = bRC_Error;
   struct plugin_ctx *p_ctx = (struct plugin_ctx *)ctx->pContext;

   if (!p_ctx) {
      goto bail_out;
   }

#ifdef HAVE_PYTHON
   PyEval_AcquireThread(p_ctx->interpreter);
   retval = PyPluginIO(ctx, io);
   PyEval_ReleaseThread(p_ctx->interpreter);
#endif

bail_out:
   return retval;
}

/*
 * Start restore of a file.
 */
static bRC startRestoreFile(bpContext *ctx, const char *cmd)
{
   bRC retval = bRC_Error;
   struct plugin_ctx *p_ctx = (struct plugin_ctx *)ctx->pContext;

   if (!p_ctx) {
      goto bail_out;
   }

#ifdef HAVE_PYTHON
   PyEval_AcquireThread(p_ctx->interpreter);
   retval = PyStartRestoreFile(ctx, cmd);
   PyEval_ReleaseThread(p_ctx->interpreter);
#endif

bail_out:
   return retval;
}

/*
 * Done restoring a file.
 */
static bRC endRestoreFile(bpContext *ctx)
{
   bRC retval = bRC_Error;
   struct plugin_ctx *p_ctx = (struct plugin_ctx *)ctx->pContext;

   if (!p_ctx) {
      goto bail_out;
   }

#ifdef HAVE_PYTHON
   PyEval_AcquireThread(p_ctx->interpreter);
   retval = PyEndRestoreFile(ctx);
   PyEval_ReleaseThread(p_ctx->interpreter);
#endif

bail_out:
   return retval;
}

/*
 * Called here to give the plugin the information needed to
 * re-create the file on a restore.  It basically gets the
 * stat packet that was created during the backup phase.
 * This data is what is needed to create the file, but does
 * not contain actual file data.
 */
static bRC createFile(bpContext *ctx, struct restore_pkt *rp)
{
   bRC retval = bRC_Error;
   struct plugin_ctx *p_ctx = (struct plugin_ctx *)ctx->pContext;

   if (!p_ctx) {
      goto bail_out;
   }

#ifdef HAVE_PYTHON
   PyEval_AcquireThread(p_ctx->interpreter);
   retval = PyCreateFile(ctx, rp);
   PyEval_ReleaseThread(p_ctx->interpreter);
#endif

bail_out:
   return retval;
}

/*
 * Called after the file has been restored. This can be used to
 * set directory permissions, ...
 */
static bRC setFileAttributes(bpContext *ctx, struct restore_pkt *rp)
{
   bRC retval = bRC_Error;
   struct plugin_ctx *p_ctx = (struct plugin_ctx *)ctx->pContext;

   if (!p_ctx) {
      goto bail_out;
   }

#ifdef HAVE_PYTHON
   PyEval_AcquireThread(p_ctx->interpreter);
   retval = PySetFileAttributes(ctx, rp);
   PyEval_ReleaseThread(p_ctx->interpreter);
#endif

bail_out:
   return retval;
}

/*
 * When using Incremental dump, all previous dumps are necessary
 */
static bRC checkFile(bpContext *ctx, char *fname)
{
   bRC retval = bRC_Error;
   struct plugin_ctx *p_ctx = (struct plugin_ctx *)ctx->pContext;

   if (!p_ctx) {
      goto bail_out;
   }

#ifdef HAVE_PYTHON
   PyEval_AcquireThread(p_ctx->interpreter);
   retval = PyCheckFile(ctx, fname);
   PyEval_ReleaseThread(p_ctx->interpreter);
#endif

bail_out:
   return retval;
}

#ifdef HAVE_PYTHON
/*
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

/*
 * Only set destination to value when it has no previous setting.
 */
static inline void set_if_null(char **destination, char *value)
{
   if (!*destination) {
      *destination = bstrdup(value);
   }
}

/*
 * Parse the plugin definition passed in.
 *
 * The definition is in this form:
 *
 * python:module_path=<path>:module_name=<python_module_name>:...
 */
static bRC parse_plugin_definition(bpContext *ctx, void *value)
{
   int i;
   char *plugin_definition, *bp, *argument, *argument_value;
   plugin_ctx *p_ctx = (plugin_ctx *)ctx->pContext;

   if (!value) {
      return bRC_Error;
   }

   /*
    * Parse the plugin definition.
    * Make a private copy of the whole string.
    */
   plugin_definition = bstrdup((char *)value);

   bp = strchr(plugin_definition, ':');
   if (!bp) {
      Jmsg(ctx, M_FATAL, "Illegal plugin definition %s\n", plugin_definition);
      Dmsg(ctx, dbglvl, "Illegal plugin definition %s\n", plugin_definition);
      goto bail_out;
   }

   /*
    * Skip the first ':'
    */
   bp++;
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
         Jmsg(ctx, M_FATAL, "Illegal argument %s without value\n", argument);
         Dmsg(ctx, dbglvl, "Illegal argument %s without value\n", argument);
         goto bail_out;
      }
      *argument_value++ = '\0';

      /*
       * See if there are more arguments and setup for the next run.
       */
      bp = strchr(argument_value, ':');
      if (bp) {
         *bp++ = '\0';
      }

      for (i = 0; plugin_arguments[i].name; i++) {
         if (bstrcasecmp(argument, plugin_arguments[i].name)) {
            char **str_destination = NULL;
            bool *bool_destination = NULL;

            switch (plugin_arguments[i].type) {
            case argument_module_path:
               str_destination = &p_ctx->module_path;
               break;
            case argument_module_name:
               str_destination = &p_ctx->module_name;
               break;
            default:
               break;
            }

            /*
             * Keep the first value, ignore any next setting.
             */
            if (str_destination) {
               set_if_null(str_destination, argument_value);
            }

            /*
             * Set any boolean variable.
             */
            if (bool_destination) {
               *bool_destination = parse_boolean(argument_value);
            }

            /*
             * When we have a match break the loop.
             */
            break;
         }
      }
   }

   free(plugin_definition);
   return bRC_OK;

bail_out:
   free(plugin_definition);
   return bRC_Error;
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
static bRC PyLoadModule(bpContext *ctx, void *value)
{
   bRC retval = bRC_Error;
   struct plugin_ctx *p_ctx = (struct plugin_ctx *)ctx->pContext;
   PyObject *sysPath,
            *mPath,
            *pName,
            *pFunc,
            *module;

   /*
    * Extend the Python search path with the given module_path.
    */
   if (p_ctx->module_path) {
      sysPath = PySys_GetObject((char *)"path");
      mPath = PyString_FromString(p_ctx->module_path);
      PyList_Append(sysPath, mPath);
      Py_DECREF(mPath);
   }

   /*
    * Make our callback methods available for Python.
    */
   module = Py_InitModule("bareosfd", BareosFDMethods);

   /*
    * Fill in the slots of PyRestoreObject
    */
   PyRestoreObjectType.tp_new = PyType_GenericNew;
   if (PyType_Ready(&PyRestoreObjectType) < 0) {
      goto bail_out;
   }

   /*
    * Fill in the slots of PyStatPacket
    */
   PyStatPacketType.tp_new = PyType_GenericNew;
   if (PyType_Ready(&PyStatPacketType) < 0) {
      goto bail_out;
   }

   /*
    * Fill in the slots of PySavePacket
    */
   PySavePacketType.tp_new = PyType_GenericNew;
   if (PyType_Ready(&PySavePacketType) < 0) {
      goto bail_out;
   }

   /*
    * Fill in the slots of PyIoPacketType
    */
   PyIoPacketType.tp_new = PyType_GenericNew;
   if (PyType_Ready(&PyIoPacketType) < 0) {
      goto bail_out;
   }

   /*
    * Add the types to the module
    */
   Py_INCREF(&PyRestoreObjectType);
   PyModule_AddObject(module, "RestoreObject", (PyObject *)&PyRestoreObjectType);

   Py_INCREF(&PyStatPacketType);
   PyModule_AddObject(module, "StatPacket", (PyObject *)&PyStatPacketType);

   Py_INCREF(&PySavePacketType);
   PyModule_AddObject(module, "SavePacket", (PyObject *)&PySavePacketType);

   Py_INCREF(&PyIoPacketType);
   PyModule_AddObject(module, "IoPacket", (PyObject *)&PyIoPacketType);

   /*
    * Try to load the Python module by name.
    */
   if (p_ctx->module_name) {
      Dmsg(ctx, dbglvl, "Trying to load module with name %s\n", p_ctx->module_name);
      pName = PyString_FromString(p_ctx->module_name);
      p_ctx->pModule = PyImport_Import(pName);
      Py_DECREF(pName);

      if (!p_ctx->pModule) {
         Dmsg(ctx, dbglvl, "Failed to load module with name %s\n", p_ctx->module_name);
         goto bail_out;
      }

      Dmsg(ctx, dbglvl, "Sucessfully loaded module with name %s\n", p_ctx->module_name);

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
         Dmsg(ctx, dbglvl, "Failed to find function named load_bareos_plugins()\n");
         goto bail_out;
      }
   }

   /*
    * Keep track we successfully loaded.
    */
   p_ctx->python_loaded = true;

   return retval;

bail_out:
   if (PyErr_Occurred()) {
      PyErrorHandler(ctx, M_FATAL);
   }

   return retval;
}

/*
 * Any plugin options which are passed in are dispatched here to a Python method and it
 * can parse the plugin options. This function is also called after PyLoadModule() has
 * loaded the Python module and made sure things are operational. Normally you would only get
 * one set of plugin options but for a restore overrides can be passed in before the actual
 * plugin options are restored as part of the restore stream handling.
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
      Dmsg(ctx, dbglvl, "Failed to find function named parse_plugin_definition()\n");
      return bRC_Error;
   }

bail_out:
   if (PyErr_Occurred()) {
      PyErrorHandler(ctx, M_FATAL);
   }

   return retval;
}

static bRC PyGetPluginValue(bpContext *ctx, pVariable var, void *value)
{
   return bRC_OK;
}

static bRC PySetPluginValue(bpContext *ctx, pVariable var, void *value)
{
   return bRC_OK;
}

static bRC PyHandlePluginEvent(bpContext *ctx, bEvent *event, void *value)
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

static inline PySavePacket *NativeToPySavePacket(struct save_pkt *sp)
{
   PySavePacket *pSavePkt;

   pSavePkt = PyObject_New(PySavePacket, &PySavePacketType);
   if (!pSavePkt) {
      return (PySavePacket *)NULL;
   }

   /*
    * Initialize the Python SavePkt with the data we got passed in.
    */
   pSavePkt->fname = NULL;
   pSavePkt->link = NULL;
   pSavePkt->statp = NULL;
   pSavePkt->type = sp->type;
   pSavePkt->flags = sp->flags;
   pSavePkt->no_read = sp->no_read;
   pSavePkt->portable = sp->portable;
   pSavePkt->accurate_found = sp->accurate_found;
   pSavePkt->cmd = sp->cmd;
   pSavePkt->delta_seq = sp->delta_seq;
   pSavePkt->object_name = NULL;
   pSavePkt->object = NULL;
   pSavePkt->object_len = sp->object_len;
   pSavePkt->object_index = sp->index;

   return pSavePkt;
}

static inline bool PySavePacketToNative(PySavePacket *pSavePkt, struct save_pkt *sp, struct plugin_ctx *p_ctx)
{
   /*
    * Only copy back the arguments that are allowed to change.
    */
   if (pSavePkt->fname) {
      /*
       * As this has to linger as long as the backup is running we save it in our plugin context.
       */
      if (PyString_Check(pSavePkt->fname)) {
         if (p_ctx->fname) {
            free(p_ctx->fname);
         }
         p_ctx->fname = bstrdup(PyString_AsString(pSavePkt->fname));
         sp->fname = p_ctx->fname;
      }
   } else {
      goto bail_out;
   }

   /*
    * Optional field.
    */
   if (pSavePkt->link) {
      /*
       * As this has to linger as long as the backup is running we save it in our plugin context.
       */
      if (PyString_Check(pSavePkt->link)) {
         if (p_ctx->link) {
            free(p_ctx->link);
         }
         p_ctx->link = bstrdup(PyString_AsString(pSavePkt->link));
         sp->link = p_ctx->link;
      }
   }

   /*
    * Handle the stat structure.
    */
   if (pSavePkt->statp) {
      PyStatPacket *statp = (PyStatPacket *)pSavePkt->statp;
      sp->statp.st_dev = statp->dev;
      sp->statp.st_ino = statp->ino;
      sp->statp.st_mode = statp->mode;
      sp->statp.st_nlink = statp->nlink;
      sp->statp.st_uid = statp->uid;
      sp->statp.st_gid = statp->gid;
      sp->statp.st_rdev = statp->rdev;
      sp->statp.st_atime = statp->atime;
      sp->statp.st_mtime = statp->mtime;
      sp->statp.st_ctime = statp->ctime;
      sp->statp.st_blksize = statp->blksize;
      sp->statp.st_blocks = statp->blocks;
   } else {
      goto bail_out;
   }

   sp->type = pSavePkt->type;
   sp->flags = pSavePkt->flags;

   /*
    * Special code for handling restore objects.
    */
   if (IS_FT_OBJECT(sp->type)) {
      /*
       * See if a proper restore object was created.
       */
      if (pSavePkt->object_len > 0) {
         /*
          * As this has to linger as long as the backup is running we save it in our plugin context.
          */
         if (pSavePkt->object_name &&
             pSavePkt->object &&
             PyString_Check(pSavePkt->object_name) &&
             PyByteArray_Check(pSavePkt->object)) {
            char *buf;

            if (p_ctx->object_name) {
               free(p_ctx->object_name);
            }
            p_ctx->object_name = bstrdup(PyString_AsString(pSavePkt->object_name));
            sp->object_name = p_ctx->object_name;

            sp->object_len = pSavePkt->object_len;
            sp->index = pSavePkt->object_index;

            if (!(buf = PyByteArray_AsString(pSavePkt->object))) {
               if (p_ctx->object) {
                  free(p_ctx->object);
               }
               p_ctx->object = (char *)malloc(pSavePkt->object_len);
               memcpy(p_ctx->object, buf, pSavePkt->object_len);
            } else {
               goto bail_out;
            }
         } else {
            goto bail_out;
         }
      } else {
         goto bail_out;
      }
   }

   return true;

bail_out:
   return false;
}

/*
 * Called when starting to backup a file. Here the plugin must
 * return the "stat" packet for the directory/file and provide
 * certain information so that Bareos knows what the file is.
 * The plugin can create "Virtual" files by giving them a
 * name that is not normally found on the file system.
 */
static bRC PyStartBackupFile(bpContext *ctx, struct save_pkt *sp)
{
   bRC retval = bRC_Error;
   struct plugin_ctx *p_ctx = (struct plugin_ctx *)ctx->pContext;
   PyObject *pFunc;

   /*
    * Lookup the start_backup_file() function in the python module.
    */
   pFunc = PyDict_GetItemString(p_ctx->pDict, "start_backup_file"); /* Borrowed reference */
   if (pFunc && PyCallable_Check(pFunc)) {
      PySavePacket *pSavePkt;
      PyObject *pRetVal;

      pSavePkt = NativeToPySavePacket(sp);
      if (!pSavePkt) {
         goto bail_out;
      }

      pRetVal = PyObject_CallFunctionObjArgs(pFunc, p_ctx->bpContext, (PyObject *)pSavePkt, NULL);
      if (!pRetVal) {
         Py_DECREF((PyObject *)pSavePkt);
         goto bail_out;
      } else {
         retval = conv_python_retval(pRetVal);
         Py_DECREF(pRetVal);

         if (!PySavePacketToNative(pSavePkt, sp, p_ctx)) {
            Py_DECREF((PyObject *)pSavePkt);
            goto bail_out;
         }
         Py_DECREF((PyObject *)pSavePkt);
      }
   } else {
      Dmsg(ctx, dbglvl, "Failed to find function named start_backup_file()\n");
   }

   return retval;

bail_out:
   if (PyErr_Occurred()) {
      PyErrorHandler(ctx, M_FATAL);
   }

   return retval;
}

/*
 * Called at the end of backing up a file for a command plugin.
 * If the plugin's work is done, it should return bRC_OK.
 * If the plugin wishes to create another file and back it up,
 * then it must return bRC_More (not yet implemented).
 */
static bRC PyEndBackupFile(bpContext *ctx)
{
   bRC retval = bRC_Error;
   struct plugin_ctx *p_ctx = (struct plugin_ctx *)ctx->pContext;
   PyObject *pFunc;

   /*
    * Lookup the end_backup_file() function in the python module.
    */
   pFunc = PyDict_GetItemString(p_ctx->pDict, "end_backup_file"); /* Borrowed reference */
   if (pFunc && PyCallable_Check(pFunc)) {
      PyObject *pRetVal;

      pRetVal = PyObject_CallFunctionObjArgs(pFunc, p_ctx->bpContext, NULL);
      if (!pRetVal) {
         goto bail_out;
      } else {
         retval = conv_python_retval(pRetVal);
      }
   } else {
      Dmsg(ctx, dbglvl, "Failed to find function named end_backup_file()\n");
   }

   return retval;

bail_out:
   if (PyErr_Occurred()) {
      PyErrorHandler(ctx, M_FATAL);
   }

   return retval;
}

static inline PyIoPacket *NativeToPyIoPacket(struct io_pkt *io)
{
   PyIoPacket *pIoPkt;

   pIoPkt = PyObject_New(PyIoPacket, &PyIoPacketType);
   if (!pIoPkt) {
      return (PyIoPacket *)NULL;
   }

   /*
    * Initialize the Python IoPkt with the data we got passed in.
    */
   pIoPkt->func = io->func;
   pIoPkt->count = io->count;
   pIoPkt->flags = io->flags;
   pIoPkt->mode = io->mode;
   pIoPkt->fname = io->fname;
   pIoPkt->whence = io->whence;
   pIoPkt->offset = io->offset;
   if (io->func == IO_WRITE && io->count > 0) {
      /*
       * Only initialize the buffer with read data when we are writing and there is data.
       */
      pIoPkt->buf = PyByteArray_FromStringAndSize(io->buf, io->count);
      if (!pIoPkt->buf) {
         Py_DECREF((PyObject *)pIoPkt);
         return (PyIoPacket *)NULL;
      }
   } else {
      pIoPkt->buf = NULL;
   }

   /*
    * These must be set by the Python function but we initialize them to zero
    * to be sure they have some valid setting an not random data.
    */
   pIoPkt->io_errno = 0;
   pIoPkt->lerror = 0;
   pIoPkt->win32 = false;
   pIoPkt->status = 0;

   return pIoPkt;
}

static inline bool PyIoPacketToNative(PyIoPacket *pIoPkt, struct io_pkt *io)
{
   /*
    * Only copy back the arguments that are allowed to change.
    */
   io->io_errno = pIoPkt->io_errno;
   io->lerror = pIoPkt->lerror;
   io->win32 = pIoPkt->win32;
   io->status = pIoPkt->status;
   if (io->func == IO_READ && io->status > 0) {
      /*
       * Only copy back the data when doing a read and there is data.
       */
      if (PyByteArray_Check(pIoPkt->buf)) {
         char *buf;

         if (!(buf = PyByteArray_AsString(pIoPkt->buf))) {
            return false;
         }
         memcpy(io->buf, buf, io->status);
      }
   }

   return true;
}

/*
 * Do actual I/O. Bareos calls this after startBackupFile
 * or after startRestoreFile to do the actual file
 * input or output.
 */
static bRC PyPluginIO(bpContext *ctx, struct io_pkt *io)
{
   bRC retval = bRC_Error;
   struct plugin_ctx *p_ctx = (struct plugin_ctx *)ctx->pContext;
   PyObject *pFunc;

   /*
    * Lookup the plugin_io() function in the python module.
    */
   pFunc = PyDict_GetItemString(p_ctx->pDict, "plugin_io"); /* Borrowed reference */
   if (pFunc && PyCallable_Check(pFunc)) {
      PyIoPacket *pIoPkt;
      PyObject *pRetVal;

      pIoPkt = NativeToPyIoPacket(io);
      if (!pIoPkt) {
         goto bail_out;
      }

      pRetVal = PyObject_CallFunctionObjArgs(pFunc, p_ctx->bpContext, (PyObject *)pIoPkt, NULL);
      if (!pRetVal) {
         Py_DECREF((PyObject *)pIoPkt);
         goto bail_out;
      } else {
         retval = conv_python_retval(pRetVal);
         Py_DECREF(pRetVal);

         if (!PyIoPacketToNative(pIoPkt, io)) {
            Py_DECREF((PyObject *)pIoPkt);
            goto bail_out;
         }
      }
      Py_DECREF((PyObject *)pIoPkt);
   } else {
      Dmsg(ctx, dbglvl, "Failed to find function named plugin_io()\n");
   }

   return retval;

bail_out:
   if (PyErr_Occurred()) {
      PyErrorHandler(ctx, M_FATAL);
   }

   io->status = -1;

   return retval;
}

/*
 * Called when the first record is read from the Volume that was previously written by the command plugin.
 */
static bRC PyStartRestoreFile(bpContext *ctx, const char *cmd)
{
   bRC retval = bRC_Error;
   struct plugin_ctx *p_ctx = (struct plugin_ctx *)ctx->pContext;
   PyObject *pFunc;

   /*
    * Lookup the start_restore_file() function in the python module.
    */
   pFunc = PyDict_GetItemString(p_ctx->pDict, "start_restore_file"); /* Borrowed reference */
   if (pFunc && PyCallable_Check(pFunc)) {
      PyObject *pCmd,
               *pRetVal;

      pCmd = PyString_FromString(cmd);
      if (!pCmd) {
         goto bail_out;
      }

      pRetVal = PyObject_CallFunctionObjArgs(pFunc, p_ctx->bpContext, pCmd, NULL);
      Py_DECREF(pCmd);

      if (!pRetVal) {
         goto bail_out;
      } else {
         retval = conv_python_retval(pRetVal);
         Py_DECREF(pRetVal);
      }
   } else {
      Dmsg(ctx, dbglvl, "Failed to find function named start_restore_file()\n");
   }

   return retval;

bail_out:
   if (PyErr_Occurred()) {
      PyErrorHandler(ctx, M_FATAL);
   }

   return retval;
}

/*
 * Called when a command plugin is done restoring a file.
 */
static bRC PyEndRestoreFile(bpContext *ctx)
{
   bRC retval = bRC_Error;
   struct plugin_ctx *p_ctx = (struct plugin_ctx *)ctx->pContext;
   PyObject *pFunc;

   /*
    * Lookup the end_restore_file() function in the python module.
    */
   pFunc = PyDict_GetItemString(p_ctx->pDict, "end_restore_file"); /* Borrowed reference */
   if (pFunc && PyCallable_Check(pFunc)) {
      PyObject *pRetVal;

      pRetVal = PyObject_CallFunctionObjArgs(pFunc, p_ctx->bpContext, NULL);
      if (!pRetVal) {
         goto bail_out;
      } else {
         retval = conv_python_retval(pRetVal);
      }
   } else {
      Dmsg(ctx, dbglvl, "Failed to find function named end_restore_file()\n");
   }

   return retval;

bail_out:
   if (PyErr_Occurred()) {
      PyErrorHandler(ctx, M_FATAL);
   }

   return retval;
}

/*
 * Called for a command plugin to create a file during a Restore job before restoring the data.
 * This entry point is called before any I/O is done on the file. After this call,
 * Bareos will call pluginIO() to open the file for write.
 *
 * We must return in rp->create_status:
 *
 * CF_ERROR    -- error
 * CF_SKIP     -- skip processing this file
 * CF_EXTRACT  -- extract the file (i.e.call i/o routines)
 * CF_CREATED  -- created, but no content to extract (typically directories)
 */
static bRC PyCreateFile(bpContext *ctx, struct restore_pkt *rp)
{
   rp->create_status = CF_EXTRACT;
   return bRC_OK;
}

static bRC PySetFileAttributes(bpContext *ctx, struct restore_pkt *rp)
{
   return bRC_OK;
}

static bRC PyCheckFile(bpContext *ctx, char *fname)
{
   return bRC_OK;
}

static inline PyObject *NativeToPyRestoreObject(restore_object_pkt *rop)
{
   PyRestoreObject *pRestoreObject;

   pRestoreObject = PyObject_New(PyRestoreObject, &PyRestoreObjectType);
   if (!pRestoreObject) {
      return (PyObject *)NULL;
   }

   pRestoreObject->object_name = rop->object_name;
   pRestoreObject->object = PyByteArray_FromStringAndSize(rop->object, rop->object_len);
   pRestoreObject->plugin_name = rop->plugin_name;
   pRestoreObject->object_type = rop->object_type;
   pRestoreObject->object_len = rop->object_len;
   pRestoreObject->object_full_len = rop->object_full_len;
   pRestoreObject->object_index = rop->object_index;
   pRestoreObject->object_compression = rop->object_compression;
   pRestoreObject->stream = rop->stream;
   pRestoreObject->JobId = rop->JobId;

   return (PyObject *)pRestoreObject;
}

static bRC PyRestoreObjectData(bpContext *ctx, restore_object_pkt *rop)
{
   bRC retval = bRC_Error;
   struct plugin_ctx *p_ctx = (struct plugin_ctx *)ctx->pContext;
   PyObject *pFunc;

   if (!rop) {
      return bRC_OK;
   }

   /*
    * Lookup the restore_object_data() function in the python module.
    */
   pFunc = PyDict_GetItemString(p_ctx->pDict, "restore_object_data"); /* Borrowed reference */
   if (pFunc && PyCallable_Check(pFunc)) {
      PyObject *pRestoreObject,
               *pRetVal;

      pRestoreObject = NativeToPyRestoreObject(rop);
      if (!pRestoreObject) {
         goto bail_out;
      }

      pRetVal = PyObject_CallFunctionObjArgs(pFunc, p_ctx->bpContext, pRestoreObject, NULL);
      Py_DECREF(pRestoreObject);

      if (!pRetVal) {
         goto bail_out;
      } else {
         retval = conv_python_retval(pRetVal);
         Py_DECREF(pRetVal);
      }
   } else {
      Dmsg(ctx, dbglvl, "Failed to find function named start_restore_file()\n");
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
   case bVarFDName:
   case bVarWorkingDir:
   case bVarExePath:
   case bVarVersion:
   case bVarDistName: {
      char *value;

      if (bfuncs->getBareosValue(ctx, (bVariable)var, &value) == bRC_OK) {
         pRetVal = PyString_FromString(value);
      }
      break;
   }
   case bVarJobId:
   case bVarLevel:
   case bVarType:
   case bVarJobStatus:
   case bVarSinceTime:
   case bVarAccurate:
   case bVarPrefixLinks: {
      int value;

      ctx = PyGetbpContext(pyCtx);
      if (bfuncs->getBareosValue(ctx, (bVariable)var, &value) == bRC_OK) {
         pRetVal = PyInt_FromLong(value);
      }
      break;
   }
   case bVarClient:
   case bVarJobName:
   case bVarPrevJobName:
   case bVarWhere:
   case bVarRegexWhere: {
      char *value;

      ctx = PyGetbpContext(pyCtx);
      if (bfuncs->getBareosValue(ctx, (bVariable)var, &value) == bRC_OK) {
         pRetVal = PyString_FromString(value);
      }
      break;
   }
   case bVarFileSeen:
      break;                 /* a write only variable, ignore read request */
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
 * a Python plugin to get certain internal values of the current Job.
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
   case bVarFileSeen: {
      char *value;

      value = PyString_AsString(pyValue);
      if (value) {
         bfuncs->setBareosValue(ctx, (bVariable)var, value);
      }
      break;
   }
   default:
      ctx = PyGetbpContext(pyCtx);
      Dmsg(ctx, dbglvl, "PyBareosSetValue: Unknown variable requested %d\n", var);
      break;
   }
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

      if (event >= bEventJobStart && event <= bEventComponentInfo) {
         Dmsg(ctx, dbglvl, "PyBareosRegisterEvents: registering event %d\n", event);
         bfuncs->registerBareosEvents(ctx, 1, event);
      }
   }

   Py_DECREF(pySeq);

   Py_INCREF(Py_None);
   return Py_None;
}

/*
 * Callback function which is exposed as a part of the additional methods which allow
 * a Python plugin to issue a Add Exclude pattern to the fileset.
 */
static PyObject *PyBareosAddExclude(PyObject *self, PyObject *args)
{
   char *file = NULL;
   bpContext *ctx;
   PyObject *pyCtx;

   if (!PyArg_ParseTuple(args, "O|z:BareosAddExclude", &pyCtx, &file)) {
      return NULL;
   }

   if (file) {
      ctx = PyGetbpContext(pyCtx);
      bfuncs->AddExclude(ctx, file);
   }

   Py_INCREF(Py_None);
   return Py_None;
}

/*
 * Callback function which is exposed as a part of the additional methods which allow
 * a Python plugin to issue a Add Include pattern to the fileset.
 */
static PyObject *PyBareosAddInclude(PyObject *self, PyObject *args)
{
   char *file = NULL;
   bpContext *ctx;
   PyObject *pyCtx;

   if (!PyArg_ParseTuple(args, "O|z:BareosAddInclude", &pyCtx, &file)) {
      return NULL;
   }

   if (file) {
      ctx = PyGetbpContext(pyCtx);
      bfuncs->AddInclude(ctx, file);
   }

   Py_INCREF(Py_None);
   return Py_None;
}

/*
 * Callback function which is exposed as a part of the additional methods which allow
 * a Python plugin to issue a Add Include Options to the fileset.
 */
static PyObject *PyBareosAddOptions(PyObject *self, PyObject *args)
{
   char *opts = NULL;
   bpContext *ctx;
   PyObject *pyCtx;

   if (!PyArg_ParseTuple(args, "O|z:BareosAddOptions", &pyCtx, &opts)) {
      return NULL;
   }

   if (opts) {
      ctx = PyGetbpContext(pyCtx);
      bfuncs->AddOptions(ctx, opts);
   }

   Py_INCREF(Py_None);
   return Py_None;
}

/*
 * Callback function which is exposed as a part of the additional methods which allow
 * a Python plugin to issue a Add Regex to the fileset.
 */
static PyObject *PyBareosAddRegex(PyObject *self, PyObject *args)
{
   int type;
   char *item = NULL;
   bpContext *ctx;
   PyObject *pyCtx;

   if (!PyArg_ParseTuple(args, "O|zi:BareosAddRegex", &pyCtx, &item, &type)) {
      return NULL;
   }

   if (item) {
      ctx = PyGetbpContext(pyCtx);
      bfuncs->AddRegex(ctx, item, type);
   }

   Py_INCREF(Py_None);
   return Py_None;
}

/*
 * Callback function which is exposed as a part of the additional methods which allow
 * a Python plugin to issue a Add Wildcard to the fileset.
 */
static PyObject *PyBareosAddWild(PyObject *self, PyObject *args)
{
   int type;
   char *item = NULL;
   bpContext *ctx;
   PyObject *pyCtx;

   if (!PyArg_ParseTuple(args, "O|zi:BareosAddWild", &pyCtx, &item, &type)) {
      return NULL;
   }

   if (item) {
      ctx = PyGetbpContext(pyCtx);
      bfuncs->AddWild(ctx, item, type);
   }

   Py_INCREF(Py_None);
   return Py_None;
}

/*
 * Callback function which is exposed as a part of the additional methods which allow
 * a Python plugin to issue a Add New Option block.
 */
static PyObject *PyBareosNewOptions(PyObject *self, PyObject *args)
{
   bpContext *ctx;
   PyObject *pyCtx;

   if (!PyArg_ParseTuple(args, "O:BareosNewOptions", &pyCtx)) {
      return NULL;
   }

   ctx = PyGetbpContext(pyCtx);
   bfuncs->NewOptions(ctx);

   Py_INCREF(Py_None);
   return Py_None;
}

/*
 * Callback function which is exposed as a part of the additional methods which allow
 * a Python plugin to issue a Add New Include block.
 */
static PyObject *PyBareosNewInclude(PyObject *self, PyObject *args)
{
   bpContext *ctx;
   PyObject *pyCtx;

   if (!PyArg_ParseTuple(args, "O:BareosNewInclude", &pyCtx)) {
      return NULL;
   }

   ctx = PyGetbpContext(pyCtx);
   bfuncs->NewInclude(ctx);

   Py_INCREF(Py_None);
   return Py_None;
}

/*
 * Callback function which is exposed as a part of the additional methods which allow
 * a Python plugin to issue a Add New Pre Include block.
 */
static PyObject *PyBareosNewPreInclude(PyObject *self, PyObject *args)
{
   bpContext *ctx;
   PyObject *pyCtx;

   if (!PyArg_ParseTuple(args, "O:BareosNewPreInclude", &pyCtx)) {
      return NULL;
   }

   ctx = PyGetbpContext(pyCtx);
   bfuncs->NewPreInclude(ctx);

   Py_INCREF(Py_None);
   return Py_None;
}

/*
 * Some helper functions.
 */
static inline char *PyGetStringValue(PyObject *object)
{
   if (!object || !PyString_Check(object)) {
      return (char *)"";
   }

   return PyString_AsString(object);
}

static inline char *PyGetByteArrayValue(PyObject *object)
{
   if (!object || !PyByteArray_Check(object)) {
      return (char *)"";
   }

   return PyByteArray_AsString(object);
}

/*
 * Python specific handlers for PyRestoreObject structure mapping.
 */

/*
 * Representation.
 */
static PyObject *PyRestoreObject_repr(PyRestoreObject *self)
{
   PyObject *s;
   POOL_MEM buf(PM_MESSAGE);

   Mmsg(buf, "RestoreObject(object_name=\"%s\", object=\"%s\", plugin_name=\"%s\", "
             "object_type=%d, object_len=%d, object_full_len=%d, "
             "object_index=%d, object_compression=%d, stream=%d, jobid=%d)",
        self->object_name, self->object, self->plugin_name,
        self->object_type, self->object_len, self->object_full_len,
        self->object_index, self->object_compression, self->stream, self->JobId);
   s = PyString_FromString(buf.c_str());

   return s;
}

/*
 * Initialization.
 */
static int PyRestoreObject_init(PyRestoreObject *self, PyObject *args, PyObject *kwds)
{
   static char *kwlist[] = {
      (char *)"object_name",
      (char *)"object",
      (char *)"plugin_name",
      (char *)"object_type",
      (char *)"object_len",
      (char *)"object_full_len",
      (char *)"object_index",
      (char *)"object_compression",
      (char *)"stream",
      (char *)"jobid",
      NULL
   };

   self->object_name = NULL;
   self->object = NULL;
   self->plugin_name = NULL;
   self->object_type = 0;
   self->object_len = 0;
   self->object_full_len = 0;
   self->object_index = 0;
   self->object_compression = 0;
   self->stream = 0;
   self->JobId = 0;

   if (!PyArg_ParseTupleAndKeywords(args,
                                    kwds,
                                    "|sosiiiiiiI",
                                    kwlist,
                                    &self->object_name,
                                    &self->object,
                                    &self->plugin_name,
                                    &self->object_type,
                                    &self->object_len,
                                    &self->object_full_len,
                                    &self->object_index,
                                    &self->object_compression,
                                    &self->stream,
                                    &self->JobId)) {
      return -1;
   }

   return 0;
}

/*
 * Destructor.
 */
static void PyRestoreObject_dealloc(PyRestoreObject *self)
{
   PyObject_Del(self);
}

/*
 * Python specific handlers for PyStatPacket structure mapping.
 */

/*
 * Representation.
 */
static PyObject *PyStatPacket_repr(PyStatPacket *self)
{
   PyObject *s;
   POOL_MEM buf(PM_MESSAGE);

   Mmsg(buf, "StatPacket(dev=%d, ino=%ld, mode=%d, nlink=%d, "
             "uid=%d, gid=%d, rdev=%d, size=%ld, "
             "atime=%d, mtime=%d, ctime=%d, blksize=%d, blocks=%ld",
        self->dev, self->ino, self->mode, self->nlink,
        self->uid, self->gid, self->rdev, self->size,
        self->atime, self->mtime, self->ctime, self->blksize, self->blocks);

   s = PyString_FromString(buf.c_str());

   return s;
}

/*
 * Initialization.
 */
static int PyStatPacket_init(PyStatPacket *self, PyObject *args, PyObject *kwds)
{
   time_t now;
   static char *kwlist[] = {
      (char *)"dev",
      (char *)"ino",
      (char *)"mode",
      (char *)"nlink",
      (char *)"uid",
      (char *)"gid",
      (char *)"rdev",
      (char *)"size",
      (char *)"atime",
      (char *)"mtime",
      (char *)"ctime",
      (char *)"blksize",
      (char *)"blocks",
      NULL
   };

   now = time(NULL);
   self->dev = 0;
   self->ino = 0;
   self->mode = 0700 | S_IFREG;
   self->nlink = 0;
   self->uid = 0;
   self->gid = 0;
   self->rdev = 0;
   self->size = -1;
   self->atime = now;
   self->mtime = now;
   self->ctime = now;
   self->blksize = 4096;
   self->blocks = 1;

   if (!PyArg_ParseTupleAndKeywords(args,
                                    kwds,
                                    "|IKHHIIIKIIIIK",
                                    kwlist,
                                    &self->dev,
                                    &self->ino,
                                    &self->mode,
                                    &self->nlink,
                                    &self->uid,
                                    &self->gid,
                                    &self->rdev,
                                    &self->size,
                                    &self->atime,
                                    &self->mtime,
                                    &self->ctime,
                                    &self->blksize,
                                    &self->blocks)) {
      return -1;
   }

   return 0;
}

/*
 * Destructor.
 */
static void PyStatPacket_dealloc(PyStatPacket *self)
{
   PyObject_Del(self);
}

/*
 * Python specific handlers for PySavePacket structure mapping.
 */

/*
 * Representation.
 */
static PyObject *PySavePacket_repr(PySavePacket *self)
{
   PyObject *s;
   POOL_MEM buf(PM_MESSAGE);

   Mmsg(buf, "SavePacket(fname=\"%s\", link=\"%s\", type=%d, flags=%d, "
             "no_read=%s, portable=%s, accurate_found=%s, "
             "cmd=\"%s\", delta_seq=%d, object_name=\"%s\", "
             "object=\"%s\", object_len=%d, object_index=%d)",
             PyGetStringValue(self->fname), PyGetStringValue(self->link),
             self->type, self->flags, self->no_read, self->portable,
             self->accurate_found, self->cmd, self->delta_seq,
             PyGetStringValue(self->object_name), PyGetByteArrayValue(self->object),
             self->object_len, self->object_index);

   s = PyString_FromString(buf.c_str());

   return s;
}

/*
 * Initialization.
 */
static int PySavePacket_init(PySavePacket *self, PyObject *args, PyObject *kwds)
{
   static char *kwlist[] = {
      (char *)"fname",
      (char *)"link",
      (char *)"type",
      (char *)"flags",
      (char *)"no_read",
      (char *)"portable",
      (char *)"accurate_found",
      (char *)"cmd",
      (char *)"delta_seq",
      (char *)"object_name",
      (char *)"object",
      (char *)"object_len",
      (char *)"object_index",
      NULL
   };

   self->fname = NULL;
   self->link = NULL;
   self->type = 0;
   self->flags = 0;
   self->no_read = false;
   self->portable = false;
   self->accurate_found = false;
   self->cmd = NULL;
   self->delta_seq = 0;
   self->object_name = NULL;
   self->object = NULL;
   self->object_len = 0;
   self->object_index = 0;

   if (!PyArg_ParseTupleAndKeywords(args,
                                    kwds,
                                    "|ooiIcccsiooii",
                                    kwlist,
                                    &self->fname,
                                    &self->link,
                                    &self->type,
                                    &self->flags,
                                    &self->no_read,
                                    &self->portable,
                                    &self->accurate_found,
                                    &self->cmd,
                                    &self->delta_seq,
                                    &self->object_name,
                                    &self->object,
                                    &self->object_len,
                                    &self->object_index)) {
      return -1;
   }

   return 0;
}

/*
 * Destructor.
 */
static void PySavePacket_dealloc(PySavePacket *self)
{
   if (self->fname) {
      Py_XDECREF(self->fname);
   }
   if (self->link) {
      Py_XDECREF(self->link);
   }
   if (self->object_name) {
      Py_XDECREF(self->object_name);
   }
   if (self->object) {
      Py_XDECREF(self->object);
   }
   PyObject_Del(self);
}

/*
 * Python specific handlers for PyIOPacket structure mapping.
 */

/*
 * Representation.
 */
static PyObject *PyIoPacket_repr(PyIoPacket *self)
{
   PyObject *s;
   POOL_MEM buf(PM_MESSAGE);

   Mmsg(buf, "IoPacket(func=%d, count=%d, flags=%d, mode=%d, "
             "buf=\"%s\", fname=\"%s\", status=%d, io_errno=%d, lerror=%d, "
             "whence=%d, offset=%d, win32=%d)",
        self->func, self->count, self->flags, self->mode,
        PyGetByteArrayValue(self->buf), self->fname, self->status,
        self->io_errno, self->lerror,
        self->whence, self->offset, self->win32);
   s = PyString_FromString(buf.c_str());

   return s;
}

/*
 * Initialization.
 */
static int PyIoPacket_init(PyIoPacket *self, PyObject *args, PyObject *kwds)
{
   static char *kwlist[] = {
      (char *)"func",
      (char *)"count",
      (char *)"flags",
      (char *)"mode",
      (char *)"buf",
      (char *)"fname",
      (char *)"status",
      (char *)"io_errno",
      (char *)"lerror",
      (char *)"whence",
      (char *)"offset",
      (char *)"win32",
      NULL
   };

   self->func = 0;
   self->count = 0;
   self->flags = 0;
   self->mode = 0;
   self->buf = NULL;
   self->fname = NULL;
   self->status = 0;
   self->io_errno = 0;
   self->lerror = 0;
   self->whence - 0;
   self->offset = 0;
   self->win32 = false;

   if (!PyArg_ParseTupleAndKeywords(args,
                                    kwds,
                                    "|Hiiiosiiiilc",
                                    kwlist,
                                    &self->func,
                                    &self->count,
                                    &self->flags,
                                    &self->mode,
                                    &self->buf,
                                    &self->fname,
                                    &self->status,
                                    &self->io_errno,
                                    &self->lerror,
                                    &self->whence,
                                    &self->offset,
                                    &self->win32)) {
      return -1;
   }

   return 0;
}

/*
 * Destructor.
 */
static void PyIoPacket_dealloc(PyIoPacket *self)
{
   if (self->buf) {
      Py_XDECREF(self->buf);
   }
   PyObject_Del(self);
}
#endif
