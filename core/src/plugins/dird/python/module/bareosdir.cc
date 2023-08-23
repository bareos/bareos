/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2020-2022 Bareos GmbH & Co. KG

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
 * Python module for the Bareos director plugin
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

#if PY_VERSION_HEX < VERSION_HEX(3, 0, 0)
#  define LOGPREFIX "python-dir-mod: "
#else
#  define LOGPREFIX "python3-dir-mod: "
#endif

#include "dird/dird.h"
#include "dird/dir_plugins.h"


#include "plugins/include/common.h"  // for Dmsg Macro

#include "plugins/dird/python/plugin_private_context.h"

#include "plugins/include/python3compat.h"

#define BAREOSDIR_MODULE
#include "bareosdir.h"
#include "lib/edit.h"

namespace directordaemon {
static const int debuglevel = 150;

static bRC set_bareos_core_functions(CoreFunctions* new_bareos_core_functions);
static bRC set_plugin_context(PluginContext* new_plugin_context);

static bRC PyParsePluginDefinition(PluginContext* plugin_ctx, void* value);
static bRC PyGetPluginValue(PluginContext* plugin_ctx,
                            pVariable var,
                            void* value);
static bRC PySetPluginValue(PluginContext* plugin_ctx,
                            pVariable var,
                            void* value);
static bRC PyHandlePluginEvent(PluginContext* plugin_ctx,
                               bDirEvent* event,
                               void* value);

/* Pointers to Bareos functions */
static CoreFunctions* bareos_core_functions = NULL;


#define NOPLUGINSETGETVALUE 1
/* functions common to all plugins */
#include "plugins/include/python_plugins_common.inc"


/* set the bareos_core_functions pointer to the given value */
static bRC set_bareos_core_functions(CoreFunctions* new_bareos_core_functions)
{
  bareos_core_functions = new_bareos_core_functions;
  return bRC_OK;
}

/* set the plugin context pointer to the given value */
static bRC set_plugin_context(PluginContext* new_plugin_context)
{
  plugin_context = new_plugin_context;
  return bRC_OK;
}


/**
 * Any plugin options which are passed in are dispatched here to a Python
 * method and it can parse the plugin options. This function is also called
 * after PyLoadModule() has loaded the Python module and made sure things are
 * operational.
 */
static bRC PyParsePluginDefinition(PluginContext* plugin_ctx, void* value)
{
  bRC retval = bRC_Error;
  struct plugin_private_context* plugin_priv_ctx
      = (struct plugin_private_context*)plugin_ctx->plugin_private_context;
  PyObject* pFunc;

  // Lookup the parse_plugin_definition() function in the python module.
  pFunc = PyDict_GetItemString(
      plugin_priv_ctx->pyModuleFunctionsDict,
      "parse_plugin_definition"); /* Borrowed reference */
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
    Dmsg(plugin_ctx, debuglevel,
         LOGPREFIX
         "Failed to find function named "
         "parse_plugin_definition()\n");
    return bRC_Error;
  }

bail_out:
  if (PyErr_Occurred()) { PyErrorHandler(plugin_ctx, M_FATAL); }

  return retval;
}

static bRC PyGetPluginValue(PluginContext*, pVariable, void*) { return bRC_OK; }

static bRC PySetPluginValue(PluginContext*, pVariable, void*) { return bRC_OK; }

static bRC PyHandlePluginEvent(PluginContext* plugin_ctx,
                               bDirEvent* event,
                               void*)
{
  bRC retval = bRC_Error;
  struct plugin_private_context* plugin_priv_ctx
      = (struct plugin_private_context*)plugin_ctx->plugin_private_context;
  PyObject* pFunc;

  // Lookup the handle_plugin_event() function in the python module.
  pFunc = PyDict_GetItemString(plugin_priv_ctx->pyModuleFunctionsDict,
                               "handle_plugin_event"); /* Borrowed reference */
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
    Dmsg(plugin_ctx, debuglevel,
         LOGPREFIX "Failed to find function named handle_plugin_event()\n");
  }

  return retval;

bail_out:
  if (PyErr_Occurred()) { PyErrorHandler(plugin_ctx, M_FATAL); }

  return retval;
}

/**
 * Callback function which is exposed as a part of the additional methods
 * which allow a Python plugin to get certain internal values of the current
 * Job.
 */
static PyObject* PyBareosGetValue(PyObject*, PyObject* args)
{
  int var;
  PluginContext* plugin_ctx = plugin_context;
  PyObject* pRetVal = NULL;

  if (!PyArg_ParseTuple(args, "i:BareosGetValue", &var)) { return NULL; }
  RETURN_RUNTIME_ERROR_IF_BFUNC_OR_BAREOS_PLUGIN_CTX_UNSET()

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

      if (bareos_core_functions->getBareosValue(plugin_ctx, (brDirVariable)var,
                                                &value)
          == bRC_OK) {
        pRetVal = PyLong_FromLong(value);
      }
      break;
    }
    case bDirVarJobErrors:
    case bDirVarSDErrors:
    case bDirVarJobFiles:
    case bDirVarSDJobFiles:
    case bDirVarJobBytes:
    case bDirVarReadBytes: {
      uint64_t value = 0;

      if (bareos_core_functions->getBareosValue(plugin_ctx, (brDirVariable)var,
                                                &value)
          == bRC_OK) {
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
      char* value = NULL;

      if (bareos_core_functions->getBareosValue(plugin_ctx, (brDirVariable)var,
                                                &value)
          == bRC_OK) {
        if (value) { pRetVal = PyUnicode_FromString(value); }
      }
      break;
    }
    case bDirVarPluginDir: {
      char* value = NULL;

      if (bareos_core_functions->getBareosValue(NULL, (brDirVariable)var,
                                                &value)
          == bRC_OK) {
        if (value) { pRetVal = PyUnicode_FromString(value); }
      }
      break;
    }
    default:
      Dmsg(plugin_ctx, debuglevel,
           LOGPREFIX "PyBareosGetValue unknown variable requested %d\n", var);
      break;
  }

  if (!pRetVal) { Py_RETURN_NONE; }

  return pRetVal;
}

/**
 * Callback function which is exposed as a part of the additional methods
 * which allow a Python plugin to set certain internal values of the current
 * Job.
 */
static PyObject* PyBareosSetValue(PyObject*, PyObject* args)
{
  int var;
  PluginContext* plugin_ctx = plugin_context;
  bRC retval = bRC_Error;
  PyObject* pyValue;

  if (!PyArg_ParseTuple(args, "iO:BareosSetValue", &var, &pyValue)) {
    goto bail_out;
  }
  RETURN_RUNTIME_ERROR_IF_BFUNC_OR_BAREOS_PLUGIN_CTX_UNSET()

  switch (var) {
    case bwDirVarVolumeName: {
      const char* value;

      value = PyUnicode_AsUTF8(pyValue);
      if (value) {
        retval = bareos_core_functions->setBareosValue(
            plugin_ctx, (bwDirVariable)var,
            static_cast<void*>(const_cast<char*>(value)));
      }

      break;
    }
    case bwDirVarPriority:
    case bwDirVarJobLevel: {
      int value;

      value = PyLong_AsLong(pyValue);
      if (value >= 0) {
        retval = bareos_core_functions->setBareosValue(
            plugin_ctx, (bwDirVariable)var, &value);
      }
      break;
    }
    default:
      Dmsg(plugin_ctx, debuglevel,
           LOGPREFIX "PyBareosSetValue unknown variable requested %d\n", var);
      break;
  }

bail_out:
  return ConvertbRCRetvalToPythonRetval(retval);
}

/**
 * Callback function which is exposed as a part of the additional methods
 * which allow a Python plugin to issue debug messages using the Bareos debug
 * message facility.
 */
static PyObject* PyBareosDebugMessage(PyObject*, PyObject* args)
{
  int level;
  char* dbgmsg = NULL;
  PluginContext* plugin_ctx = plugin_context;

  if (!PyArg_ParseTuple(args, "i|z:BareosDebugMessage", &level, &dbgmsg)) {
    return NULL;
  }
  RETURN_RUNTIME_ERROR_IF_BFUNC_OR_BAREOS_PLUGIN_CTX_UNSET()

  if (dbgmsg) { Dmsg(plugin_ctx, level, LOGPREFIX "%s", dbgmsg); }

  Py_RETURN_NONE;
}

/**
 * Callback function which is exposed as a part of the additional methods
 * which allow a Python plugin to issue Job messages using the Bareos Job
 * message facility.
 */
static PyObject* PyBareosJobMessage(PyObject*, PyObject* args)
{
  int type;
  char* jobmsg = NULL;
  PluginContext* plugin_ctx = plugin_context;

  if (!PyArg_ParseTuple(args, "i|z:BareosJobMessage", &type, &jobmsg)) {
    return NULL;
  }
  RETURN_RUNTIME_ERROR_IF_BFUNC_OR_BAREOS_PLUGIN_CTX_UNSET()

  if (jobmsg) { Jmsg(plugin_ctx, type, LOGPREFIX "%s", jobmsg); }

  Py_RETURN_NONE;
}

/**
 * Callback function which is exposed as a part of the additional methods
 * which allow a Python plugin to issue a Register Event to register
 * additional events it wants to receive.
 */
static PyObject* PyBareosRegisterEvents(PyObject*, PyObject* args)
{
  int len, event;
  PluginContext* plugin_ctx = plugin_context;
  bRC retval = bRC_Error;
  PyObject *pyEvents, *pySeq, *pyEvent;

  if (!PyArg_ParseTuple(args, "O:BareosRegisterEvents", &pyEvents)) {
    goto bail_out;
  }
  RETURN_RUNTIME_ERROR_IF_BFUNC_OR_BAREOS_PLUGIN_CTX_UNSET()

  pySeq = PySequence_Fast(pyEvents, "Expected a sequence of events");
  if (!pySeq) { goto bail_out; }

  len = PySequence_Fast_GET_SIZE(pySeq);

  for (int i = 0; i < len; i++) {
    pyEvent = PySequence_Fast_GET_ITEM(pySeq, i);
    event = PyLong_AsLong(pyEvent);

    if (event >= bDirEventJobStart && event <= bDirEventGetScratch) {
      Dmsg(plugin_ctx, debuglevel,
           LOGPREFIX "PyBareosRegisterEvents registering event %d\n", event);
      retval
          = bareos_core_functions->registerBareosEvents(plugin_ctx, 1, event);

      if (retval != bRC_OK) { break; }
    }
  }

  Py_DECREF(pySeq);

bail_out:
  return ConvertbRCRetvalToPythonRetval(retval);
}

/**
 * Callback function which is exposed as a part of the additional methods
 * which allow a Python plugin to issue an Unregister Event to unregister
 * events it doesn't want to receive anymore.
 */
static PyObject* PyBareosUnRegisterEvents(PyObject*, PyObject* args)
{
  int len, event;
  PluginContext* plugin_ctx = plugin_context;
  bRC retval = bRC_Error;
  PyObject *pyEvents, *pySeq, *pyEvent;

  if (!PyArg_ParseTuple(args, "O:BareosUnRegisterEvents", &pyEvents)) {
    goto bail_out;
  }
  RETURN_RUNTIME_ERROR_IF_BFUNC_OR_BAREOS_PLUGIN_CTX_UNSET()

  pySeq = PySequence_Fast(pyEvents, "Expected a sequence of events");
  if (!pySeq) { goto bail_out; }

  len = PySequence_Fast_GET_SIZE(pySeq);

  for (int i = 0; i < len; i++) {
    pyEvent = PySequence_Fast_GET_ITEM(pySeq, i);
    event = PyLong_AsLong(pyEvent);

    if (event >= bDirEventJobStart && event <= bDirEventGetScratch) {
      Dmsg(plugin_ctx, debuglevel,
           "PyBareosUnRegisterEvents: registering event %d\n", event);
      retval
          = bareos_core_functions->unregisterBareosEvents(plugin_ctx, 1, event);

      if (retval != bRC_OK) { break; }
    }
  }

  Py_DECREF(pySeq);

bail_out:
  return ConvertbRCRetvalToPythonRetval(retval);
}

/**
 * Callback function which is exposed as a part of the additional methods
 * which allow a Python plugin to issue a GetInstanceCount to retrieve the
 * number of instances of the current plugin being loaded into the daemon.
 */
static PyObject* PyBareosGetInstanceCount(PyObject*, PyObject* args)
{
  int value;
  PluginContext* plugin_ctx = plugin_context;
  PyObject* pRetVal = NULL;

  if (!PyArg_ParseTuple(args, ":BareosGetInstanceCount")) { return NULL; }
  RETURN_RUNTIME_ERROR_IF_BFUNC_OR_BAREOS_PLUGIN_CTX_UNSET()

  if (bareos_core_functions->getInstanceCount(plugin_ctx, &value) == bRC_OK) {
    pRetVal = PyLong_FromLong(value);
  }

  if (!pRetVal) { Py_RETURN_NONE; }

  return pRetVal;
}

} /* namespace directordaemon */
