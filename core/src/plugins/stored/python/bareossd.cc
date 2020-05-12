/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2020-2020 Bareos GmbH & Co. KG

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
 * Python module for the Bareos storagedaemon plugin
 */
#define PY_SSIZE_T_CLEAN
#define BUILD_PLUGIN

#if defined(HAVE_WIN32)
#include "include/bareos.h"
#include <Python.h>
#else
#include <Python.h>
#include "include/bareos.h"
#endif

#include "stored/sd_plugins.h"

#define BAREOSSD_MODULE
#include "bareossd.h"
#include "lib/edit.h"

namespace storagedaemon {

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
                               bSdEvent* event,
                               void* value);

/* Pointers to Bareos functions */
static CoreFunctions* bareos_core_functions = NULL;

#include "plugin_private_context.h"

#define NOPLUGINSETGETVALUE 1
/* functions common to all plugins */
#include "plugins/python_plugins_common.inc"


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
 * Any plugin options which are passed in are dispatched here to a Python method
 * and it can parse the plugin options. This function is also called after
 * PyLoadModule() has loaded the Python module and made sure things are
 * operational.
 */
static bRC PyParsePluginDefinition(PluginContext* plugin_ctx, void* value)
{
  bRC retval = bRC_Error;
  struct plugin_private_context* plugin_priv_ctx =
      (struct plugin_private_context*)plugin_ctx->plugin_private_context;
  PyObject* pFunc;

  /*
   * Lookup the parse_plugin_definition() function in the python module.
   */
  pFunc =
      PyDict_GetItemString(plugin_priv_ctx->pyModuleFunctionsDict,
                           "parse_plugin_definition"); /* Borrowed reference */
  if (pFunc && PyCallable_Check(pFunc)) {
    PyObject *pPluginDefinition, *pRetVal;

    pPluginDefinition = PyString_FromString((char*)value);
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
    Dmsg(
        plugin_ctx, debuglevel,
        "python-sd: Failed to find function named parse_plugin_definition()\n");
    return bRC_Error;
  }

bail_out:
  if (PyErr_Occurred()) { PyErrorHandler(plugin_ctx, M_FATAL); }

  return retval;
}

static bRC PyGetPluginValue(PluginContext* plugin_ctx,
                            pVariable var,
                            void* value)
{
  return bRC_OK;
}

static bRC PySetPluginValue(PluginContext* plugin_ctx,
                            pVariable var,
                            void* value)
{
  return bRC_OK;
}

static bRC PyHandlePluginEvent(PluginContext* plugin_ctx,
                               bSdEvent* event,
                               void* value)
{
  bRC retval = bRC_Error;
  plugin_private_context* plugin_priv_ctx =
      (plugin_private_context*)plugin_ctx->plugin_private_context;
  PyObject* pFunc;

  /*
   * Lookup the handle_plugin_event() function in the python module.
   */
  pFunc = PyDict_GetItemString(plugin_priv_ctx->pyModuleFunctionsDict,
                               "handle_plugin_event"); /* Borrowed reference */
  if (pFunc && PyCallable_Check(pFunc)) {
    PyObject *pEventType, *pRetVal;

    pEventType = PyInt_FromLong(event->eventType);

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
         "python-sd: Failed to find function named handle_plugin_event()\n");
  }

  return retval;

bail_out:
  if (PyErr_Occurred()) { PyErrorHandler(plugin_ctx, M_FATAL); }

  return retval;
}

/**
 * Callback function which is exposed as a part of the additional methods which
 * allow a Python plugin to get certain internal values of the current Job.
 */
static PyObject* PyBareosGetValue(PyObject* self, PyObject* args)
{
  int var;
  PluginContext* plugin_ctx = plugin_context;
  PyObject* pRetVal = NULL;

  if (!PyArg_ParseTuple(args, "i:BareosGetValue", &var)) { return NULL; }
  RETURN_RUNTIME_ERROR_IF_BFUNC_OR_BAREOS_PLUGIN_CTX_UNSET()

  switch (var) {
    case bsdVarJobId:
    case bsdVarLevel:
    case bsdVarType:
    case bsdVarJobStatus: {
      int value;

      if (bareos_core_functions->getBareosValue(plugin_ctx, (bsdrVariable)var,
                                                &value) == bRC_OK) {
        pRetVal = PyInt_FromLong(value);
      }
      break;
    }
    case bsdVarJobErrors:
    case bsdVarJobFiles:
    case bsdVarJobBytes: {
      uint64_t value = 0;

      if (bareos_core_functions->getBareosValue(plugin_ctx, (bsdrVariable)var,
                                                &value) == bRC_OK) {
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
      char* value = NULL;

      if (bareos_core_functions->getBareosValue(plugin_ctx, (bsdrVariable)var,
                                                &value) == bRC_OK) {
        if (value) { pRetVal = PyString_FromString(value); }
      }
      break;
    }
    case bsdVarCompatible: {
      bool value;

      if (bareos_core_functions->getBareosValue(NULL, (bsdrVariable)var,
                                                &value) == bRC_OK) {
        long bool_value;

        bool_value = (value) ? 1 : 0;
        pRetVal = PyBool_FromLong(bool_value);
      }
      break;
    }
    case bsdVarPluginDir: {
      char* value = NULL;

      if (bareos_core_functions->getBareosValue(NULL, (bsdrVariable)var,
                                                &value) == bRC_OK) {
        if (value) { pRetVal = PyString_FromString(value); }
      }
      break;
    }
    default:
      Dmsg(plugin_ctx, debuglevel,
           "python-sd: PyBareosGetValue unknown variable requested %d\n", var);
      break;
  }

  if (!pRetVal) {
    Py_INCREF(Py_None);
    pRetVal = Py_None;
  }

  return pRetVal;
}

/**
 * Callback function which is exposed as a part of the additional methods which
 * allow a Python plugin to get certain internal values of the current Job.
 */
static PyObject* PyBareosSetValue(PyObject* self, PyObject* args)
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
    case bsdwVarVolumeName: {
      char* value;

      value = PyString_AsString(pyValue);
      if (value) {
        bareos_core_functions->setBareosValue(plugin_ctx, (bsdwVariable)var,
                                              value);
      }

      break;
    }
    case bsdwVarPriority:
    case bsdwVarJobLevel: {
      int value;

      value = PyInt_AsLong(pyValue);
      if (value >= 0) {
        retval = bareos_core_functions->setBareosValue(
            plugin_ctx, (bsdwVariable)var, &value);
      }
      break;
    }
    default:
      Dmsg(plugin_ctx, debuglevel,
           "python-sd: PyBareosSetValue unknown variable requested %d\n", var);
      break;
  }

bail_out:
  return ConvertbRCRetvalToPythonRetval(retval);
}

/**
 * Callback function which is exposed as a part of the additional methods which
 * allow a Python plugin to issue debug messages using the Bareos debug message
 * facility.
 */
static PyObject* PyBareosDebugMessage(PyObject* self, PyObject* args)
{
  int level;
  char* dbgmsg = NULL;
  PluginContext* plugin_ctx = plugin_context;

  if (!PyArg_ParseTuple(args, "i|z:BareosDebugMessage", &level, &dbgmsg)) {
    return NULL;
  }
  RETURN_RUNTIME_ERROR_IF_BFUNC_OR_BAREOS_PLUGIN_CTX_UNSET()

  if (dbgmsg) { Dmsg(plugin_ctx, level, "python-sd: %s", dbgmsg); }

  Py_INCREF(Py_None);
  return Py_None;
}

/**
 * Callback function which is exposed as a part of the additional methods which
 * allow a Python plugin to issue Job messages using the Bareos Job message
 * facility.
 */
static PyObject* PyBareosJobMessage(PyObject* self, PyObject* args)
{
  int type;
  char* jobmsg = NULL;
  PluginContext* plugin_ctx = plugin_context;

  if (!PyArg_ParseTuple(args, "i|z:BareosJobMessage", &type, &jobmsg)) {
    return NULL;
  }
  RETURN_RUNTIME_ERROR_IF_BFUNC_OR_BAREOS_PLUGIN_CTX_UNSET()

  if (jobmsg) { Jmsg(plugin_ctx, type, "python-sd: %s", jobmsg); }

  Py_INCREF(Py_None);
  return Py_None;
}

/**
 * Callback function which is exposed as a part of the additional methods which
 * allow a Python plugin to issue a Register Event to register additional events
 * it wants to receive.
 */
static PyObject* PyBareosRegisterEvents(PyObject* self, PyObject* args)
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
    event = PyInt_AsLong(pyEvent);

    if (event >= bSdEventJobStart && event <= bSdEventWriteRecordTranslation) {
      Dmsg(plugin_ctx, debuglevel,
           "python-sd: PyBareosRegisterEvents registering event %d\n", event);
      retval =
          bareos_core_functions->registerBareosEvents(plugin_ctx, 1, event);

      if (retval != bRC_OK) { break; }
    }
  }

  Py_DECREF(pySeq);

bail_out:
  return ConvertbRCRetvalToPythonRetval(retval);
}

/**
 * Callback function which is exposed as a part of the additional methods which
 * allow a Python plugin to issue an Unregister Event to unregister events it
 * doesn't want to receive anymore.
 */
static PyObject* PyBareosUnRegisterEvents(PyObject* self, PyObject* args)
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
    event = PyInt_AsLong(pyEvent);

    if (event >= bSdEventJobStart && event <= bSdEventWriteRecordTranslation) {
      Dmsg(plugin_ctx, debuglevel,
           "PyBareosUnRegisterEvents: registering event %d\n", event);
      retval =
          bareos_core_functions->unregisterBareosEvents(plugin_ctx, 1, event);

      if (retval != bRC_OK) { break; }
    }
  }

  Py_DECREF(pySeq);

bail_out:
  return ConvertbRCRetvalToPythonRetval(retval);
}

/**
 * Callback function which is exposed as a part of the additional methods which
 * allow a Python plugin to issue a GetInstanceCount to retrieve the number of
 * instances of the current plugin being loaded into the daemon.
 */
static PyObject* PyBareosGetInstanceCount(PyObject* self, PyObject* args)
{
  int value;
  PluginContext* plugin_ctx = plugin_context;
  PyObject* pRetVal = NULL;

  if (!PyArg_ParseTuple(args, ":BareosGetInstanceCount")) { return NULL; }
  RETURN_RUNTIME_ERROR_IF_BFUNC_OR_BAREOS_PLUGIN_CTX_UNSET()

  if (!plugin_ctx) {
    PyErr_SetString(PyExc_ValueError, "plugin_ctx is unset");
    return NULL;
  }
  if (!bareos_core_functions) {
    PyErr_SetString(PyExc_ValueError, "bareos_core_functions is unset");
    return NULL;
  }
  if (bareos_core_functions->getInstanceCount(plugin_ctx, &value) == bRC_OK) {
    pRetVal = PyInt_FromLong(value);
  }

  if (!pRetVal) {
    Py_INCREF(Py_None);
    pRetVal = Py_None;
  }

  return pRetVal;
}


} /* namespace storagedaemon*/
