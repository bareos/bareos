/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2011-2014 Planets Communications B.V.
   Copyright (C) 2013-2021 Bareos GmbH & Co. KG

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
/**
 * @file
 * Python plugin for the Bareos Storage Daemon
 */
#define PY_SSIZE_T_CLEAN
#define BUILD_PLUGIN

#if defined(HAVE_WIN32)
#  include "include/bareos.h"
#  include <Python.h>
#else
#  include <Python.h>
#  include "include/bareos.h"
#endif
#include "include/version_hex.h"

#define PLUGIN_DAEMON "sd"

#if PY_VERSION_HEX < VERSION_HEX(3, 0, 0)
#  define PLUGIN_NAME "python"
#  define PLUGIN_DIR PY2MODDIR
#else
#  define PLUGIN_NAME "python3"
#  define PLUGIN_DIR PY3MODDIR
#endif

#define LOGPREFIX PLUGIN_NAME "-" PLUGIN_DAEMON ": "

#include "stored/stored.h"
#include "plugins/include/python3compat.h"

#include "python-sd.h"
#include "module/bareossd.h"
#include "lib/edit.h"

namespace storagedaemon {

static const int debuglevel = 150;

#define PLUGIN_LICENSE "Bareos AGPLv3"
#define PLUGIN_AUTHOR "Bareos GmbH & Co.KG"
#define PLUGIN_DATE "May 2020"
#define PLUGIN_VERSION "4"
#define PLUGIN_DESCRIPTION "Python Storage Daemon Plugin"
#define PLUGIN_USAGE                                                     \
  PLUGIN_NAME                                                            \
  ":instance=<instance_id>:module_path=<path-to-python-modules>:module_" \
  "name=<python-module-to-load>"


/* Forward referenced functions */
static bRC newPlugin(PluginContext* plugin_ctx);
static bRC freePlugin(PluginContext* plugin_ctx);
static bRC getPluginValue(PluginContext* plugin_ctx,
                          pVariable var,
                          void* value);
static bRC setPluginValue(PluginContext* plugin_ctx,
                          pVariable var,
                          void* value);
static bRC handlePluginEvent(PluginContext* plugin_ctx,
                             bSdEvent* event,
                             void* value);
static bRC parse_plugin_definition(PluginContext* plugin_ctx,
                                   void* value,
                                   PoolMem& plugin_options);

/* Pointers to Bareos functions */
static CoreFunctions* bareos_core_functions = NULL;
static PluginApiDefinition* bareos_plugin_interface_version = NULL;

static PluginInformation pluginInfo
    = {sizeof(pluginInfo), SD_PLUGIN_INTERFACE_VERSION,
       SD_PLUGIN_MAGIC,    PLUGIN_LICENSE,
       PLUGIN_AUTHOR,      PLUGIN_DATE,
       PLUGIN_VERSION,     PLUGIN_DESCRIPTION,
       PLUGIN_USAGE};

static PluginFunctions pluginFuncs
    = {sizeof(pluginFuncs), SD_PLUGIN_INTERFACE_VERSION,

       /* Entry points into plugin */
       newPlugin,  /* new plugin instance */
       freePlugin, /* free plugin instance */
       getPluginValue, setPluginValue, handlePluginEvent};

#include "plugin_private_context.h"


/**
 * We don't actually use this but we need it to tear down the
 * final python interpreter on unload of the plugin. Each instance of
 * the plugin get its own interpreter.
 */
static PyThreadState* mainThreadState{nullptr};

/* functions common to all plugins */
#include "plugins/include/python_plugins_common.inc"
#include "plugins/include/python_plugin_modules_common.inc"

/* Common functions used in all python plugins.  */
static bRC getPluginValue(PluginContext* bareos_plugin_ctx,
                          pVariable var,
                          void* value)
{
  struct plugin_private_context* plugin_priv_ctx
      = (struct plugin_private_context*)
            bareos_plugin_ctx->plugin_private_context;
  bRC retval = bRC_Error;

  if (!plugin_priv_ctx) { goto bail_out; }

  PyEval_AcquireThread(plugin_priv_ctx->interpreter);
  retval = Bareossd_PyGetPluginValue(bareos_plugin_ctx, var, value);
  PyEval_ReleaseThread(plugin_priv_ctx->interpreter);

bail_out:
  return retval;
}

static bRC setPluginValue(PluginContext* bareos_plugin_ctx,
                          pVariable var,
                          void* value)
{
  struct plugin_private_context* plugin_priv_ctx
      = (struct plugin_private_context*)
            bareos_plugin_ctx->plugin_private_context;
  bRC retval = bRC_Error;

  if (!plugin_priv_ctx) { return bRC_Error; }

  PyEval_AcquireThread(plugin_priv_ctx->interpreter);
  retval = Bareossd_PySetPluginValue(bareos_plugin_ctx, var, value);
  PyEval_ReleaseThread(plugin_priv_ctx->interpreter);

  return retval;
}


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
bRC loadPlugin(PluginApiDefinition* lbareos_plugin_interface_version,
               CoreFunctions* lbareos_core_functions,
               PluginInformation** plugin_information,
               PluginFunctions** plugin_functions)
{
  if (Py_IsInitialized()) { return bRC_Error; }

  Py_InitializeEx(0);
  // add bareos plugin path to python module search path
  PyObject* sysPath = PySys_GetObject((char*)"path");
  PyObject* pluginPath = PyUnicode_FromString(PLUGIN_DIR);
  PyList_Append(sysPath, pluginPath);
  Py_DECREF(pluginPath);

  /* import the bareossd module */
  PyObject* bareossdModule = PyImport_ImportModule("bareossd");
  if (!bareossdModule) {
    printf("loading of bareossd extension module failed\n");
    if (PyErr_Occurred()) { PyErrorHandler(); }
  }

  /* import the CAPI from the bareossd python module
   * afterwards, Bareossd_* macros are initialized to
   * point to the corresponding functions in the bareossd python
   * module */
  import_bareossd();

  /* set bareos_core_functions inside of barossd module */
  Bareossd_set_bareos_core_functions(lbareos_core_functions);

  bareos_core_functions
      = lbareos_core_functions; /* Set Bareos funct pointers */
  bareos_plugin_interface_version = lbareos_plugin_interface_version;

  *plugin_information = &pluginInfo; /* Return pointer to our info */
  *plugin_functions = &pluginFuncs;  /* Return pointer to our functions */

#if PY_VERSION_HEX < VERSION_HEX(3, 7, 0)
  PyEval_InitThreads();
#endif

  mainThreadState = PyEval_SaveThread();
  return bRC_OK;
}

/**
 * Plugin called here when it is unloaded, normally when Bareos is going to
 * exit.
 */
bRC unloadPlugin()
{
  /* Terminate Python if it was initialized correctly */
  if (mainThreadState) {
    PyEval_RestoreThread(mainThreadState);
    Py_Finalize();
    mainThreadState = nullptr;
  }
  return bRC_OK;
}

#ifdef __cplusplus
}
#endif

/* Create a new instance of the plugin i.e. allocate our private storage */
static bRC newPlugin(PluginContext* plugin_ctx)
{
  struct plugin_private_context* plugin_priv_ctx
      = (struct plugin_private_context*)malloc(
          sizeof(struct plugin_private_context));
  if (!plugin_priv_ctx) { return bRC_Error; }
  memset(plugin_priv_ctx, 0, sizeof(struct plugin_private_context));
  plugin_ctx->plugin_private_context
      = (void*)plugin_priv_ctx; /* set our context pointer */

  /* set bareos_plugin_context inside of barossd module */
  Bareossd_set_plugin_context(plugin_ctx);
  /* For each plugin instance we instantiate a new Python interpreter. */
  PyEval_AcquireThread(mainThreadState);
  plugin_priv_ctx->interpreter = Py_NewInterpreter();
  PyEval_ReleaseThread(plugin_priv_ctx->interpreter);

  /*
   * Always register some events the python plugin itself can register
   * any other events it is interested in.
   */
  bareos_core_functions->registerBareosEvents(plugin_ctx, 1,
                                              bSdEventNewPluginOptions);

  return bRC_OK;
}

/* Free a plugin instance, i.e. release our private storage */
static bRC freePlugin(PluginContext* plugin_ctx)
{
  struct plugin_private_context* plugin_priv_ctx
      = (struct plugin_private_context*)plugin_ctx->plugin_private_context;

  if (!plugin_priv_ctx) { return bRC_Error; }

  // Stop any sub interpreter started per plugin instance.
  PyEval_AcquireThread(plugin_priv_ctx->interpreter);


  if (plugin_priv_ctx->pModule) { Py_DECREF(plugin_priv_ctx->pModule); }

  Py_EndInterpreter(plugin_priv_ctx->interpreter);
#if PY_VERSION_HEX < VERSION_HEX(3, 2, 0)
  PyEval_ReleaseLock();
#else
  PyThreadState_Swap(mainThreadState);
  PyEval_ReleaseThread(mainThreadState);
#endif

  free(plugin_priv_ctx);
  plugin_ctx->plugin_private_context = NULL;

  return bRC_OK;
}


static bRC handlePluginEvent(PluginContext* plugin_ctx,
                             bSdEvent* event,
                             void* value)
{
  bRC retval = bRC_Error;
  bool event_dispatched = false;
  PoolMem plugin_options(PM_FNAME);
  plugin_private_context* plugin_priv_ctx
      = (plugin_private_context*)plugin_ctx->plugin_private_context;

  if (!plugin_priv_ctx) { goto bail_out; }

  /*
   * First handle some events internally before calling python if it
   * want to do some special handling on the event triggered.
   */
  switch (event->eventType) {
    case bSdEventNewPluginOptions:
      event_dispatched = true;
      retval = parse_plugin_definition(plugin_ctx, value, plugin_options);
      break;
    default:
      break;
  }

  /*
   * See if we have been triggered in the previous switch if not we have to
   * always dispatch the event. If we already processed the event internally
   * we only do a dispatch to the python entry point when that internal
   * processing was successful (e.g. retval == bRC_OK).
   */
  if (!event_dispatched || retval == bRC_OK) {
    PyEval_AcquireThread(plugin_priv_ctx->interpreter);

    /*
     * Now dispatch the event to Python.
     * First the calls that need special handling.
     */
    switch (event->eventType) {
      case bSdEventNewPluginOptions:
        // See if we already loaded the Python modules.
        if (!plugin_priv_ctx->python_loaded) {
          retval = PyLoadModule(plugin_ctx, plugin_options.c_str());
        }

        /* Only try to call when the loading succeeded. */
        if (retval == bRC_OK) {
          retval = Bareossd_PyParsePluginDefinition(plugin_ctx,
                                                    plugin_options.c_str());
        }
        break;
      default:
        /*
         * Handle the generic events e.g. the ones which are just passed on.
         * We only try to call Python when we loaded the right module until
         * that time we pretend the call succeeded.
         */
        if (plugin_priv_ctx->python_loaded) {
          retval = Bareossd_PyHandlePluginEvent(plugin_ctx, event, value);
        } else {
          retval = bRC_OK;
        }
        break;
    }

    PyEval_ReleaseThread(plugin_priv_ctx->interpreter);
  }

bail_out:
  return retval;
}

/**
 * Parse the plugin definition passed in.
 *
 * The definition is in this form:
 *
 * python:module_path=<path>:module_name=<python_module_name>:...
 */
static bRC parse_plugin_definition(PluginContext* plugin_ctx,
                                   void* value,
                                   PoolMem& plugin_options)
{
  bool found;
  int i, cnt;
  PoolMem plugin_definition(PM_FNAME);
  char *bp, *argument, *argument_value;
  plugin_private_context* plugin_priv_ctx
      = (plugin_private_context*)plugin_ctx->plugin_private_context;

  if (!value) { return bRC_Error; }

  /*
   * Parse the plugin definition.
   * Make a private copy of the whole string.
   */
  PmStrcpy(plugin_definition, (char*)value);

  bp = strchr(plugin_definition.c_str(), ':');
  if (!bp) {
    Jmsg(plugin_ctx, M_FATAL, LOGPREFIX "Illegal plugin definition %s\n",
         plugin_definition.c_str());
    Dmsg(plugin_ctx, debuglevel, LOGPREFIX "Illegal plugin definition %s\n",
         plugin_definition.c_str());
    goto bail_out;
  }

  // Skip the first ':'
  bp++;

  cnt = 0;
  while (bp) {
    if (strlen(bp) == 0) { break; }

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
      Jmsg(plugin_ctx, M_FATAL, LOGPREFIX "Illegal argument %s without value\n",
           argument);
      Dmsg(plugin_ctx, debuglevel,
           LOGPREFIX "Illegal argument %s without value\n", argument);
      goto bail_out;
    }
    *argument_value++ = '\0';

    // See if there are more arguments and setup for the next run.
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
      if (Bstrcasecmp(argument, plugin_arguments[i].name)) {
        int64_t* int_destination = NULL;
        char** str_destination = NULL;
        bool* bool_destination = NULL;

        switch (plugin_arguments[i].type) {
          case argument_instance:
            int_destination = &plugin_priv_ctx->instance;
            break;
          case argument_module_path:
            str_destination = &plugin_priv_ctx->module_path;
            break;
          case argument_module_name:
            str_destination = &plugin_priv_ctx->module_name;
            break;
          default:
            break;
        }

        if (int_destination) {
          *int_destination = parse_integer(argument_value);
        }

        if (str_destination) { SetString(str_destination, argument_value); }

        if (bool_destination) {
          *bool_destination = ParseBoolean(argument_value);
        }

        // When we have a match break the loop.
        found = true;
        break;
      }
    }

    // If we didn't consume this parameter we add it to the plugin_options list.
    if (!found) {
      PoolMem option(PM_FNAME);

      if (cnt) {
        Mmsg(option, ":%s=%s", argument, argument_value);
        PmStrcat(plugin_options, option.c_str());
      } else {
        Mmsg(option, "%s=%s", argument, argument_value);
        PmStrcat(plugin_options, option.c_str());
      }
      cnt++;
    }
  }

  if (cnt > 0) { PmStrcat(plugin_options, ":"); }

  return bRC_OK;

bail_out:
  return bRC_Error;
}

} /* namespace storagedaemon*/
