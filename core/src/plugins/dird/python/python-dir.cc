/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2011-2014 Planets Communications B.V.
   Copyright (C) 2013-2026 Bareos GmbH & Co. KG

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
 * Python plugin for the Bareos Director Daemon
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

#define PLUGIN_DIR PY3MODDIR

#include "dird/dird.h"

#include "python-dir.h"
#include "lib/plugins.h"
#include "lib/edit.h"

#include <algorithm>
#include <future>
#include <thread>
#include <cstdint>
#include <string_view>

#include "module.h"

namespace {
using python_execution_request = std::pair<void (*)(PyObject*, void*), void*>;
static inline constexpr python_execution_request REQUEST_THREAD_STOP
    = {nullptr, nullptr};

// Plugin private context
struct plugin_private_context {
  int64_t instance{};                 // Instance number of plugin
  bool python_loaded{};               // Plugin has python module loaded?
  bool python_default_path_is_set{};  // Python plugin default search path is
                                      // set?
  bool python_path_is_set{};          // Python plugin search path is set?
  char* module_path{};                // Plugin Module Path
  char* module_name{};                // Plugin Module Name

  PyObject* plugin_module;

  std::mutex execute_mut;
  std::condition_variable request_empty;
  std::condition_variable execution_requested;
  std::optional<python_execution_request> to_execute;

  std::thread python_thread;
};

plugin_private_context* get_private_context(PluginContext* ctx)
{
  return static_cast<plugin_private_context*>(ctx->plugin_private_context);
}
}  // namespace

namespace directordaemon {

static const int debuglevel = 150;

#define PLUGIN_LICENSE "Bareos AGPLv3"
#define PLUGIN_AUTHOR "Bareos GmbH & Co. KG"
#define PLUGIN_DATE "Jan 2026"
#define PLUGIN_VERSION "5"
#define PLUGIN_DESCRIPTION "Python Director Daemon Plugin"
#define PLUGIN_USAGE                                                           \
  "python3"                                                                    \
  ":module_name=<python-module-to-load>:module_path=<path-to-python-modules>:" \
  "instance=<instance_id>:...\n"                                               \
  "\n"                                                                         \
  "  module_name: The name of the Python module.\n"                            \
  "  module_path: Python search path for the module.\n"                        \
  "               The path '" PYTHON_MODULE_PATH                               \
  "' is always checked for modules.\n"                                         \
  "  instance:    Default is ’0’.\n"                                           \
  "               Increment the number, when using more than one plugin.\n"    \
  "  Additional parameters are plugin specific."


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
                             bDirEvent* event,
                             void* value);
static bRC parse_plugin_definition(PluginContext* plugin_ctx,
                                   void* value,
                                   PoolMem& plugin_options);

/* Pointers to Bareos functions */
static CoreFunctions* bareos_core_functions = NULL;
static PluginApiDefinition* bareos_plugin_interface_version = NULL;

static PluginInformation pluginInfo
    = {sizeof(pluginInfo), DIR_PLUGIN_INTERFACE_VERSION,
       DIR_PLUGIN_MAGIC,   PLUGIN_LICENSE,
       PLUGIN_AUTHOR,      PLUGIN_DATE,
       PLUGIN_VERSION,     PLUGIN_DESCRIPTION,
       PLUGIN_USAGE};

static PluginFunctions pluginFuncs
    = {sizeof(pluginFuncs), DIR_PLUGIN_INTERFACE_VERSION,

       /* Entry points into plugin */
       newPlugin,  /* new plugin instance */
       freePlugin, /* free plugin instance */
       getPluginValue, setPluginValue, handlePluginEvent};

static PyThreadState* mainThreadState{nullptr};

/* functions common to all plugins */
#include "plugins/include/python_plugins_common.inc"

void run_python_thread(PluginContext* plugin_ctx,
                       PyInterpreterState* main_interp,
                       std::promise<bool>* ready)
{
  /* For each plugin instance we instantiate a new Python interpreter. */

  // before creating the subinterpreter, we first need to acquire
  // the main interpreter gil.  To do this correctly, we need to
  // create a new threadstate for that interpreter
  auto* ts_maininterp = PyThreadState_New(main_interp);
  PyEval_RestoreThread(ts_maininterp);

  bool ok = true;

  PyThreadState* ts = Py_NewInterpreter();
  if (!ts) { ok = false; }
  PyObject* module
      = ts ? make_module(plugin_ctx, bareos_core_functions) : nullptr;
  if (!module) { ok = false; }

  // we created the module now, but it is not registered yet,
  // so any `import <module>` will fail.

  if (ok) {
    PyObject* module_dict = PyImport_GetModuleDict();
    PyDict_SetItemString(module_dict, "bareosdir", module);
  }

  ready->set_value(ok);

  auto* priv_ctx = get_private_context(plugin_ctx);

  for (;;) {
    std::unique_lock lock{priv_ctx->execute_mut};
    priv_ctx->execution_requested.wait(
        lock, [&] { return priv_ctx->to_execute.has_value(); });

    ASSERT(priv_ctx->to_execute.has_value());

    python_execution_request req = priv_ctx->to_execute.value();
    priv_ctx->to_execute.reset();

    priv_ctx->request_empty.notify_one();
    lock.unlock();

    if (req == REQUEST_THREAD_STOP) { break; }

    if (ok) {
      ASSERT(req.first != nullptr);

      (*req.first)(module, req.second);
    }
  }

  if (ts) {
    Py_EndInterpreter(ts);

    // now we also need to cleanup the maininterp threadstate
    PyThreadState_Swap(ts_maininterp);
  }

  PyThreadState_Clear(ts_maininterp);

  // delete the threadstate and release the gil
  PyThreadState_DeleteCurrent();
}

// wait until there is nothing currently executing
// returns with execution lock
void plugin_submit(plugin_private_context* ctx, python_execution_request req)
{
  std::unique_lock lock{ctx->execute_mut};

  ctx->request_empty.wait(lock,
                          [&ctx] { return !ctx->to_execute.has_value(); });

  ASSERT(!ctx->to_execute.has_value());

  ctx->to_execute.emplace(req);

  ctx->execution_requested.notify_one();
}

template <typename F> void plugin_run(plugin_private_context* ctx, F&& fun)
{
  using data = std::pair<F*, std::promise<void>>;
  data d;
  d.first = &fun;

  auto executed = d.second.get_future();

  if constexpr (std::is_invocable_v<F, PyObject*>) {
    plugin_submit(ctx, {+[](PyObject* module, void* ptr) {
                          data* data_ptr = static_cast<data*>(ptr);

                          (*data_ptr->first)(module);
                          data_ptr->second.set_value();
                        },
                        &d});
  } else {
    plugin_submit(ctx, {+[](PyObject*, void* ptr) {
                          data* data_ptr = static_cast<data*>(ptr);

                          (*data_ptr->first)();
                          data_ptr->second.set_value();
                        },
                        &d});
  }

  executed.wait();
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
BAREOS_EXPORT bRC
loadPlugin(PluginApiDefinition* lbareos_plugin_interface_version,
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

// External entry point to unload the plugin
BAREOS_EXPORT bRC unloadPlugin()
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
  plugin_private_context* plugin_priv_ctx = new plugin_private_context;
  if (!plugin_priv_ctx) { return bRC_Error; }
  plugin_ctx->plugin_private_context
      = (void*)plugin_priv_ctx; /* set our context pointer */


  std::promise<bool> ready{};
  auto thread_ready = ready.get_future();
  plugin_priv_ctx->python_thread = std::thread{run_python_thread, plugin_ctx,
                                               mainThreadState->interp, &ready};

  auto start_success = thread_ready.get();

  if (!start_success) {
    Jmsg(plugin_ctx, M_FATAL, "could not start the python sub interpreter");
    return bRC_Error;
  }

  /* Always register some events the python plugin itself can register
     any other events it is interested in.  */
  bareos_core_functions->registerBareosEvents(plugin_ctx, 1,
                                              bDirEventNewPluginOptions);

  return bRC_OK;
}

/* Free a plugin instance, i.e. release our private storage */
static bRC freePlugin(PluginContext* plugin_ctx)
{
  auto* plugin_priv_ctx = get_private_context(plugin_ctx);

  if (!plugin_priv_ctx) { return bRC_Error; }

  plugin_submit(plugin_priv_ctx, REQUEST_THREAD_STOP);
  plugin_priv_ctx->python_thread.join();

  if (plugin_priv_ctx->module_path) { free(plugin_priv_ctx->module_path); }

  if (plugin_priv_ctx->module_name) { free(plugin_priv_ctx->module_name); }

  delete plugin_priv_ctx;
  plugin_ctx->plugin_private_context = NULL;

  return bRC_OK;
}

/**
 * Any plugin options which are passed in are dispatched here to a Python
 * method and it can parse the plugin options. This function is also called
 * after PyLoadModule() has loaded the Python module and made sure things are
 * operational.
 */
bRC PyParsePluginDefinition(PluginContext* ctx,
                            PyObject* plugin,
                            const char* value)
{
  bRC retval = bRC_Error;
  PyObject* moduleDict = PyModule_GetDict(plugin);
  PyObject* pFunc;

  // Lookup the parse_plugin_definition() function in the python module.
  pFunc = PyDict_GetItemString(
      moduleDict, "parse_plugin_definition"); /* Borrowed reference */
  if (pFunc && PyCallable_Check(pFunc)) {
    PyObject *pPluginDefinition, *pRetVal;

    pPluginDefinition = PyUnicode_FromString((char*)value);
    if (!pPluginDefinition) { goto bail_out; }

    pRetVal = PyObject_CallFunctionObjArgs(pFunc, pPluginDefinition, NULL);
    Py_DECREF(pPluginDefinition);

    if (!pRetVal) {
      goto bail_out;
    } else {
      retval = ConvertPythonRetvalTobRCRetval(pRetVal);
      Py_DECREF(pRetVal);
    }

    return retval;
  } else {
    Dmsg(ctx, debuglevel,
         "Failed to find function named "
         "parse_plugin_definition()\n");
    return bRC_Error;
  }

bail_out:
  if (PyErr_Occurred()) { PyErrorHandler(ctx, M_FATAL); }

  return retval;
}

bRC PyHandlePluginEvent(PluginContext* ctx,
                        PyObject* plugin,
                        bDirEvent* event,
                        void*)
{
  bRC retval = bRC_Error;
  PyObject* moduleDict = PyModule_GetDict(plugin);
  PyObject* pFunc;

  // Lookup the handle_plugin_event() function in the python module.
  pFunc = PyDict_GetItemString(moduleDict,
                               "handle_plugin_event"); /* Borrowed reference
                                                        */
  if (pFunc && PyCallable_Check(pFunc)) {
    PyObject *pEventType, *pRetVal;

    pEventType = PyLong_FromLong(event->eventType);

    pRetVal = PyObject_CallFunctionObjArgs(pFunc, pEventType, NULL);
    Py_DECREF(pEventType);

    if (!pRetVal) {
      goto bail_out;
    } else {
      retval = ConvertPythonRetvalTobRCRetval(pRetVal);
      Py_DECREF(pRetVal);
    }
  } else {
    Dmsg(ctx, debuglevel,
         "Failed to find function named handle_plugin_event()\n");
  }

  return retval;

bail_out:
  if (PyErr_Occurred()) {
    PyErrorHandler(ctx, M_FATAL);
  }

  return retval;
}

bRC PyGetPluginValue(PluginContext*,
                     PyObject*,
                     directordaemon::pVariable,
                     void*)
{
  return bRC_OK;
}
bRC PySetPluginValue(PluginContext*,
                     PyObject*,
                     directordaemon::pVariable,
                     void*)
{
  return bRC_OK;
}


static bRC PyLoadModule(PluginContext* plugin_ctx, const char* options)
{
  auto* priv_ctx = get_private_context(plugin_ctx);
  ASSERT(priv_ctx);

  bRC retval = bRC_Error;

  /* See if we already setup the python search path.  */
  if (!priv_ctx->python_default_path_is_set) {
    PyObject* sysPath = PySys_GetObject((char*)"path");
    PyObject* mPath = PyUnicode_FromString(PYTHON_MODULE_PATH);
    PyList_Insert(sysPath, 0, mPath);
    Py_DECREF(mPath);
    priv_ctx->python_default_path_is_set = true;
  }
  if (!priv_ctx->python_path_is_set) {
    /* Extend the Python search path with the given module_path.  */
    if (priv_ctx->module_path) {
      PyObject* sysPath = PySys_GetObject((char*)"path");
      PyObject* mPath = PyUnicode_FromString(priv_ctx->module_path);
      PyList_Insert(sysPath, 0, mPath);
      Py_DECREF(mPath);
      priv_ctx->python_path_is_set = true;
    }
  }

  /* Try to load the Python module by name. */
  if (priv_ctx->module_name) {
    Dmsg(plugin_ctx, debuglevel, "Trying to load module with name %s\n",
         priv_ctx->module_name);
    priv_ctx->plugin_module = PyImport_ImportModule(priv_ctx->module_name);
    if (!priv_ctx->plugin_module) {
      Dmsg(plugin_ctx, debuglevel, "Failed to load module with name %s\n",
           priv_ctx->module_name);
      goto bail_out;
    }

    Dmsg(plugin_ctx, debuglevel, "Successfully loaded module with name %s\n",
         priv_ctx->module_name);

    PyObject* module_dict = PyModule_GetDict(priv_ctx->plugin_module);
    /* Lookup the load_bareos_plugin() function in the python module.  */
    PyObject* pFunc = PyDict_GetItemString(
        module_dict, "load_bareos_plugin"); /* Borrowed reference */
    if (pFunc && PyCallable_Check(pFunc)) {
      PyObject *pPluginDefinition, *pRetVal;

      pPluginDefinition = PyUnicode_FromString(options);
      if (!pPluginDefinition) { goto bail_out; }

      pRetVal = PyObject_CallFunctionObjArgs(pFunc, pPluginDefinition, NULL);
      Py_DECREF(pPluginDefinition);

      if (!pRetVal) {
        goto bail_out;
      } else {
        retval = ConvertPythonRetvalTobRCRetval(pRetVal);
        Py_DECREF(pRetVal);
      }
    } else {
      Dmsg(plugin_ctx, debuglevel,
           "Failed to find function named load_bareos_plugin()\n");
      goto bail_out;
    }

    // Keep track we successfully loaded.
    priv_ctx->python_loaded = true;
  }

  return retval;

bail_out:
  if (PyErr_Occurred()) { PyErrorHandler(plugin_ctx, M_FATAL); }

  return retval;
}

/* Common functions used in all python plugins.  */
static bRC getPluginValue(PluginContext* bareos_plugin_ctx,
                          pVariable var,
                          void* value)
{
  auto* plugin_priv_ctx = get_private_context(bareos_plugin_ctx);
  bRC retval = bRC_Error;

  if (!plugin_priv_ctx) { goto bail_out; }

  plugin_run(plugin_priv_ctx, [&] {
    retval = PyGetPluginValue(bareos_plugin_ctx, plugin_priv_ctx->plugin_module,
                              var, value);
  });

bail_out:
  return retval;
}

static bRC setPluginValue(PluginContext* bareos_plugin_ctx,
                          pVariable var,
                          void* value)
{
  auto* plugin_priv_ctx = get_private_context(bareos_plugin_ctx);
  bRC retval = bRC_Error;

  if (!plugin_priv_ctx) { return bRC_Error; }

  plugin_run(plugin_priv_ctx, [&] {
    retval = PySetPluginValue(bareos_plugin_ctx, plugin_priv_ctx->plugin_module,
                              var, value);
  });

  return retval;
}


static bRC handlePluginEvent(PluginContext* plugin_ctx,
                             bDirEvent* event,
                             void* value)
{
  bRC retval = bRC_Error;
  bool event_dispatched = false;
  PoolMem plugin_options(PM_FNAME);

  auto* plugin_priv_ctx = get_private_context(plugin_ctx);
  if (!plugin_priv_ctx) { goto bail_out; }

  /* First handle some events internally before calling python if it
   * want to do some special handling on the event triggered. */
  switch (event->eventType) {
    case bDirEventNewPluginOptions:
      event_dispatched = true;
      retval = parse_plugin_definition(plugin_ctx, value, plugin_options);
      break;
    default:
      break;
  }

  /* See if we have been triggered in the previous switch if not we have to
   * always dispatch the event. If we already processed the event internally
   * we only do a dispatch to the python entry point when that internal
   * processing was successful (e.g. retval == bRC_OK). */
  if (!event_dispatched || retval == bRC_OK) {
    /* Now dispatch the event to Python.
     * First the calls that need special handling. */
    plugin_run(plugin_priv_ctx, [&] {
      switch (event->eventType) {
        case bDirEventNewPluginOptions:
          // See if we already loaded the Python modules.
          if (!plugin_priv_ctx->python_loaded) {
            retval = PyLoadModule(plugin_ctx, plugin_options.c_str());
          }

          /* Only try to call when the loading succeeded. */
          if (retval == bRC_OK) {
            retval = PyParsePluginDefinition(plugin_ctx,
                                             plugin_priv_ctx->plugin_module,
                                             plugin_options.c_str());
          }
          break;
        default:
          /* Handle the generic events e.g. the ones which are just passed on.
           * We only try to call Python when we loaded the right module until
           * that time we pretend the call succeeded. */
          if (plugin_priv_ctx->python_loaded) {
            retval = PyHandlePluginEvent(
                plugin_ctx, plugin_priv_ctx->plugin_module, event, value);
          } else {
            retval = bRC_OK;
          }
          break;
      }
    });
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

  /* Parse the plugin definition.
   * Make a private copy of the whole string. */
  PmStrcpy(plugin_definition, (char*)value);

  bp = strchr(plugin_definition.c_str(), ':');
  if (!bp) {
    Jmsg(plugin_ctx, M_FATAL, "Illegal plugin definition %s\n",
         plugin_definition.c_str());
    Dmsg(plugin_ctx, debuglevel, "Illegal plugin definition %s\n",
         plugin_definition.c_str());
    goto bail_out;
  }

  // Skip the first ':'
  bp++;

  cnt = 0;
  while (bp) {
    if (strlen(bp) == 0) { break; }

    /* Each argument is in the form:
     *    <argument> = <argument_value>
     *
     * So we setup the right pointers here, argument to the beginning
     * of the argument, argument_value to the beginning of the argument_value.
     */
    argument = bp;
    argument_value = strchr(bp, '=');
    if (!argument_value) {
      Jmsg(plugin_ctx, M_FATAL, "Illegal argument %s without value\n",
           argument);
      Dmsg(plugin_ctx, debuglevel, "Illegal argument %s without value\n",
           argument);
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

} /* namespace directordaemon */
