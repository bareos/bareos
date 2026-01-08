/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2026-2026 Bareos GmbH & Co. KG

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   Affero General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
*/

#include "module.h"
#include <format>

struct module_state {
  PluginContext* ctx;
  directordaemon::CoreFunctions funs;
};

namespace {
int debuglevel = 100;

// // Convert a return value into a bRC enum value.
// bRC ConvertPythonRetvalTobRCRetval(PyObject* pRetVal)
// {
//   return (bRC)PyLong_AsLong(pRetVal);
// }

// Convert a return value from bRC enum value into Python Object.
PyObject* ConvertbRCRetvalToPythonRetval(bRC retval)
{
  return (PyObject*)PyLong_FromLong((int)retval);
}

module_state* get_state(PyObject* module)
{
  auto* state = static_cast<module_state*>(PyModule_GetState(module));
  return state;
}

bRC registerBareosEvent(module_state* state, int event)
{
  return state->funs.registerBareosEvents(state->ctx, 1, event);
}
bRC unregisterBareosEvent(module_state* state, int event)
{
  return state->funs.unregisterBareosEvents(state->ctx, 1, event);
}
bRC getInstanceCount(module_state* state, int* ret)
{
  return state->funs.getInstanceCount(state->ctx, ret);
}
bRC getBareosValue(module_state* state,
                   directordaemon::brDirVariable var,
                   void* value)
{
  return state->funs.getBareosValue(state->ctx, var, value);
}
bRC setBareosValue(module_state* state,
                   directordaemon::bwDirVariable var,
                   void* value)
{
  return state->funs.setBareosValue(state->ctx, var, value);
}

void Jmsg(module_state* state,
          int type,
          std::string msg,
          std::source_location loc = std::source_location::current())
{
  state->funs.JobMessage(state->ctx, loc.file_name(), loc.line(), type, 0,
                         "%s\n", msg.c_str());
}

void Dmsg(module_state* state,
          int level,
          std::string msg,
          std::source_location loc = std::source_location::current())
{
  state->funs.DebugMessage(state->ctx, loc.file_name(), loc.line(), level,
                           "%s\n", msg.c_str());
}
};  // namespace

namespace directordaemon {
// static bRC PyHandlePluginEvent(PluginContext* plugin_ctx,
//                                bDirEvent* event,
//                                void*)
// {
//   bRC retval = bRC_Error;
//   PyObject* moduleDict = get_module_function_dict(plugin_ctx);
//   PyObject* pFunc;

//   // Lookup the handle_plugin_event() function in the python module.
//   pFunc = PyDict_GetItemString(moduleDict,
//                                "handle_plugin_event"); /* Borrowed reference
//                                */
//   if (pFunc && PyCallable_Check(pFunc)) {
//     PyObject *pEventType, *pRetVal;

//     pEventType = PyLong_FromLong(event->eventType);

//     pRetVal = PyObject_CallFunctionObjArgs(pFunc, pEventType, NULL);
//     Py_DECREF(pEventType);

//     if (!pRetVal) {
//       goto bail_out;
//     } else {
//       retval = ConvertPythonRetvalTobRCRetval(pRetVal);
//       Py_DECREF(pRetVal);
//     }
//   } else {
//     Dmsg(plugin_ctx, debuglevel,
//          LOGPREFIX "Failed to find function named handle_plugin_event()\n");
//   }

//   return retval;

// bail_out:
//   if (PyErr_Occurred()) { PyErrorHandler(plugin_ctx, M_FATAL); }

//   return retval;
// }

/**
 * Callback function which is exposed as a part of the additional methods
 * which allow a Python plugin to get certain internal values of the current
 * Job.
 */
static PyObject* PyBareosGetValue(PyObject* module, PyObject* args)
{
  auto* state = get_state(module);
  if (!state) {
    PyErr_SetString(PyExc_RuntimeError, "state not set");
    return nullptr;
  }

  int var;
  PyObject* pRetVal = NULL;

  if (!PyArg_ParseTuple(args, "i:BareosGetValue", &var)) { return NULL; }

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

      if (getBareosValue(state, (brDirVariable)var, &value) == bRC_OK) {
        pRetVal = PyLong_FromLong(value);
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

      if (getBareosValue(state, (brDirVariable)var, &value) == bRC_OK) {
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

      if (getBareosValue(state, (brDirVariable)var, &value) == bRC_OK) {
        if (value) { pRetVal = PyUnicode_FromString(value); }
      }
      break;
    }
    case bDirVarPluginDir: {
      char* value = NULL;

      if (getBareosValue(state, (brDirVariable)var, &value) == bRC_OK) {
        if (value) { pRetVal = PyUnicode_FromString(value); }
      }
      break;
    }
    default:
      Dmsg(state, debuglevel,
           std::format("PyBareosGetValue unknown variable requested {}", var));
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
static PyObject* PyBareosSetValue(PyObject* module, PyObject* args)
{
  auto* state = get_state(module);
  if (!state) {
    PyErr_SetString(PyExc_RuntimeError, "state not set");
    return nullptr;
  }

  int var;
  bRC retval = bRC_Error;
  PyObject* pyValue;

  if (!PyArg_ParseTuple(args, "iO:BareosSetValue", &var, &pyValue)) {
    goto bail_out;
  }

  switch (var) {
    case bwDirVarVolumeName: {
      const char* value;

      value = PyUnicode_AsUTF8(pyValue);
      if (value) {
        retval = setBareosValue(state, (bwDirVariable)var,
                                static_cast<void*>(const_cast<char*>(value)));
      }

      break;
    }
    case bwDirVarPriority:
    case bwDirVarJobLevel: {
      int value;

      value = PyLong_AsLong(pyValue);
      if (value >= 0) {
        retval = setBareosValue(state, (bwDirVariable)var, &value);
      }
      break;
    }
    default:
      Dmsg(state, debuglevel,
           std::format("PyBareosSetValue unknown variable requested {}", var));
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
static PyObject* PyBareosDebugMessage(PyObject* module, PyObject* args)
{
  auto* state = get_state(module);
  if (!state) {
    PyErr_SetString(PyExc_RuntimeError, "state not set");
    return nullptr;
  }
  int level;
  char* dbgmsg = NULL;

  if (!PyArg_ParseTuple(args, "i|z:BareosDebugMessage", &level, &dbgmsg)) {
    return NULL;
  }

  if (dbgmsg) { Dmsg(state, level, dbgmsg); }

  Py_RETURN_NONE;
}

/**
 * Callback function which is exposed as a part of the additional methods
 * which allow a Python plugin to issue Job messages using the Bareos Job
 * message facility.
 */
static PyObject* PyBareosJobMessage(PyObject* module, PyObject* args)
{
  auto* state = get_state(module);
  if (!state) {
    PyErr_SetString(PyExc_RuntimeError, "state not set");
    return nullptr;
  }

  int type;
  char* jobmsg = NULL;

  if (!PyArg_ParseTuple(args, "i|z:BareosJobMessage", &type, &jobmsg)) {
    return NULL;
  }

  if (jobmsg) { Jmsg(state, type, std::format("{}", jobmsg)); }

  Py_RETURN_NONE;
}

/**
 * Callback function which is exposed as a part of the additional methods
 * which allow a Python plugin to issue a Register Event to register
 * additional events it wants to receive.
 */
static PyObject* PyBareosRegisterEvents(PyObject* module, PyObject* args)
{
  auto* state = get_state(module);
  if (!state) {
    PyErr_SetString(PyExc_RuntimeError, "state not set");
    return nullptr;
  }

  int len, event;
  bRC retval = bRC_Error;
  PyObject *pyEvents, *pySeq, *pyEvent;

  if (!PyArg_ParseTuple(args, "O:BareosRegisterEvents", &pyEvents)) {
    goto bail_out;
  }

  pySeq = PySequence_Fast(pyEvents, "Expected a sequence of events");
  if (!pySeq) { goto bail_out; }

  len = PySequence_Fast_GET_SIZE(pySeq);

  for (int i = 0; i < len; i++) {
    pyEvent = PySequence_Fast_GET_ITEM(pySeq, i);
    event = PyLong_AsLong(pyEvent);

    if (event >= bDirEventJobStart && event <= bDirEventGetScratch) {
      Dmsg(state, debuglevel,
           std::format("PyBareosRegisterEvents registering event {}", event));
      retval = registerBareosEvent(state, event);

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
static PyObject* PyBareosUnRegisterEvents(PyObject* module, PyObject* args)
{
  auto* state = get_state(module);
  if (!state) {
    PyErr_SetString(PyExc_RuntimeError, "state not set");
    return nullptr;
  }

  int len, event;
  bRC retval = bRC_Error;
  PyObject *pyEvents, *pySeq, *pyEvent;

  if (!PyArg_ParseTuple(args, "O:BareosUnRegisterEvents", &pyEvents)) {
    goto bail_out;
  }

  pySeq = PySequence_Fast(pyEvents, "Expected a sequence of events");
  if (!pySeq) { goto bail_out; }

  len = PySequence_Fast_GET_SIZE(pySeq);

  for (int i = 0; i < len; i++) {
    pyEvent = PySequence_Fast_GET_ITEM(pySeq, i);
    event = PyLong_AsLong(pyEvent);

    if (event >= bDirEventJobStart && event <= bDirEventGetScratch) {
      Dmsg(
          state, debuglevel,
          std::format("PyBareosUnRegisterEvents: registering event {}", event));
      retval = unregisterBareosEvent(state, event);

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
static PyObject* PyBareosGetInstanceCount(PyObject* module, PyObject* args)
{
  auto* state = get_state(module);
  if (!state) {
    PyErr_SetString(PyExc_RuntimeError, "state not set");
    return nullptr;
  }

  int value;
  PyObject* pRetVal = NULL;

  if (!PyArg_ParseTuple(args, ":BareosGetInstanceCount")) { return NULL; }

  if (getInstanceCount(state, &value) == bRC_OK) {
    pRetVal = PyLong_FromLong(value);
  }

  if (!pRetVal) { Py_RETURN_NONE; }

  return pRetVal;
}
}  // namespace directordaemon

PyObject* make_module(PluginContext* ctx, directordaemon::CoreFunctions* funs)
{
  PyMethodDef module_methods[]
      = {{"GetValue", directordaemon::PyBareosGetValue, METH_VARARGS,
          "Get a Plugin value"},
         {"SetValue", directordaemon::PyBareosSetValue, METH_VARARGS,
          "Set a Plugin value"},
         {"DebugMessage", directordaemon::PyBareosDebugMessage, METH_VARARGS,
          "Print a Debug message"},
         {"JobMessage", directordaemon::PyBareosJobMessage, METH_VARARGS,
          "Print a Job message"},
         {"RegisterEvents", directordaemon::PyBareosRegisterEvents,
          METH_VARARGS, "Register Plugin Events"},
         {"UnRegisterEvents", directordaemon::PyBareosUnRegisterEvents,
          METH_VARARGS, "Unregister Plugin Events"},
         {"GetInstanceCount", directordaemon::PyBareosGetInstanceCount,
          METH_VARARGS, "Get number of instances of current plugin"},
         {}};

  PyModuleDef def = {
      .m_base = PyModuleDef_HEAD_INIT,
      .m_name = "bareosdir",
      .m_doc = "stuff",
      .m_size = sizeof(module_state),
      .m_methods = module_methods,
      .m_slots = nullptr,

      // maybe needs to be a real fun
      .m_traverse = nullptr,
      .m_clear = nullptr,
      .m_free = nullptr,
  };

  PyObject* mod = PyModule_Create(&def);
  if (!mod) { return nullptr; }

  auto* state = get_state(mod);
  state->ctx = ctx;
  state->funs = *funs;

  return mod;
}
