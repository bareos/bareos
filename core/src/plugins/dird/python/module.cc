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

#include "common.h"
#include "common_module.h"
#include "module.h"
#include "fmt/format.h"
#include "lib/source_location.h"

namespace {
inline constexpr int debuglevel = 100;

struct module_state {
  PluginContext* ctx;
  directordaemon::CoreFunctions funs;
};

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
          libbareos::source_location loc
          = libbareos::source_location::current())
{
  state->funs.JobMessage(state->ctx, loc.file_name(), loc.line(), type, 0,
                         "%s\n", msg.c_str());
}

void Dmsg(module_state* state,
          int level,
          std::string msg,
          libbareos::source_location loc
          = libbareos::source_location::current())
{
  state->funs.DebugMessage(state->ctx, loc.file_name(), loc.line(), level,
                           "%s\n", msg.c_str());
}

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
    using namespace directordaemon;

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
    } break;
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
    } break;
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
    } break;
    case bDirVarPluginDir: {
      char* value = NULL;

      if (getBareosValue(state, (brDirVariable)var, &value) == bRC_OK) {
        if (value) { pRetVal = PyUnicode_FromString(value); }
      }
    } break;
    default: {
      Dmsg(state, debuglevel,
           fmt::format("PyBareosGetValue unknown variable requested {}", var));
    } break;
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
    using namespace directordaemon;

    case bwDirVarVolumeName: {
      const char* value;

      value = PyUnicode_AsUTF8(pyValue);
      if (value) {
        retval = setBareosValue(state, (bwDirVariable)var,
                                static_cast<void*>(const_cast<char*>(value)));
      }

    } break;
    case bwDirVarPriority:
    case bwDirVarJobLevel: {
      int value;

      value = PyLong_AsLong(pyValue);
      if (value >= 0) {
        retval = setBareosValue(state, (bwDirVariable)var, &value);
      }
    } break;
    default: {
      Dmsg(state, debuglevel,
           fmt::format("PyBareosSetValue unknown variable requested {}", var));
    } break;
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

  if (jobmsg) { Jmsg(state, type, fmt::format("{}", jobmsg)); }

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

    if (event >= directordaemon::bDirEventJobStart
        && event <= directordaemon::bDirEventGetScratch) {
      Dmsg(state, debuglevel,
           fmt::format("PyBareosRegisterEvents registering event {}", event));
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

    if (event >= directordaemon::bDirEventJobStart
        && event <= directordaemon::bDirEventGetScratch) {
      Dmsg(
          state, debuglevel,
          fmt::format("PyBareosUnRegisterEvents: registering event {}", event));
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

PyMethodDef module_methods[]
    = {{"GetValue", PyBareosGetValue, METH_VARARGS, "Get a Plugin value"},
       {"SetValue", PyBareosSetValue, METH_VARARGS, "Set a Plugin value"},
       {"DebugMessage", PyBareosDebugMessage, METH_VARARGS,
        "Print a Debug message"},
       {"JobMessage", PyBareosJobMessage, METH_VARARGS, "Print a Job message"},
       {"RegisterEvents", PyBareosRegisterEvents, METH_VARARGS,
        "Register Plugin Events"},
       {"UnRegisterEvents", PyBareosUnRegisterEvents, METH_VARARGS,
        "Unregister Plugin Events"},
       {"GetInstanceCount", PyBareosGetInstanceCount, METH_VARARGS,
        "Get number of instances of current plugin"},
       {}};

#define set_enum_value(dict, eval) \
  dict_insert_steal((dict), #eval, make_python_object(eval))

PyObject* Var_Dict()
{
  PyObject* dict = PyDict_New();
  if (!dict) { return nullptr; }

  using namespace directordaemon;

  if (!set_enum_value(dict, bDirVarJob) || !set_enum_value(dict, bDirVarLevel)
      || !set_enum_value(dict, bDirVarType)
      || !set_enum_value(dict, bDirVarJobId)
      || !set_enum_value(dict, bDirVarClient)
      || !set_enum_value(dict, bDirVarNumVols)
      || !set_enum_value(dict, bDirVarPool)
      || !set_enum_value(dict, bDirVarStorage)
      || !set_enum_value(dict, bDirVarWriteStorage)
      || !set_enum_value(dict, bDirVarReadStorage)
      || !set_enum_value(dict, bDirVarCatalog)
      || !set_enum_value(dict, bDirVarMediaType)
      || !set_enum_value(dict, bDirVarJobName)
      || !set_enum_value(dict, bDirVarJobStatus)
      || !set_enum_value(dict, bDirVarPriority)
      || !set_enum_value(dict, bDirVarVolumeName)
      || !set_enum_value(dict, bDirVarCatalogRes)
      || !set_enum_value(dict, bDirVarJobErrors)
      || !set_enum_value(dict, bDirVarJobFiles)
      || !set_enum_value(dict, bDirVarSDJobFiles)
      || !set_enum_value(dict, bDirVarSDErrors)
      || !set_enum_value(dict, bDirVarFDJobStatus)
      || !set_enum_value(dict, bDirVarSDJobStatus)
      || !set_enum_value(dict, bDirVarPluginDir)
      || !set_enum_value(dict, bDirVarLastRate)
      || !set_enum_value(dict, bDirVarJobBytes)
      || !set_enum_value(dict, bDirVarReadBytes)) {
    Py_DECREF(dict);
    return nullptr;
  }

  return dict;
}

PyObject* wVar_Dict()
{
  PyObject* dict = PyDict_New();
  if (!dict) { return nullptr; }

  using namespace directordaemon;

  if (!set_enum_value(dict, bwDirVarJobReport)
      || !set_enum_value(dict, bwDirVarVolumeName)
      || !set_enum_value(dict, bwDirVarPriority)
      || !set_enum_value(dict, bwDirVarJobLevel)) {
    Py_DECREF(dict);
    return nullptr;
  }

  return dict;
}

PyObject* Event_Dict()
{
  PyObject* dict = PyDict_New();
  if (!dict) { return nullptr; }

  using namespace directordaemon;

  if (!set_enum_value(dict, bDirEventJobStart)
      || !set_enum_value(dict, bDirEventJobEnd)
      || !set_enum_value(dict, bDirEventJobInit)
      || !set_enum_value(dict, bDirEventJobRun)
      || !set_enum_value(dict, bDirEventVolumePurged)
      || !set_enum_value(dict, bDirEventNewVolume)
      || !set_enum_value(dict, bDirEventNeedVolume)
      || !set_enum_value(dict, bDirEventVolumeFull)
      || !set_enum_value(dict, bDirEventRecyle)
      || !set_enum_value(dict, bDirEventGetScratch)
      || !set_enum_value(dict, bDirEventNewPluginOptions)) {
    Py_DECREF(dict);
    return nullptr;
  }

  return dict;
}

#undef set_enum_value

int setup_dicts(PyObject* module)
{
  named_dict dicts[] = {
      {"bJobMessageType", JMsg_Dict()}, {"bRCs", bRC_Dict()},
      {"bDirVariable", Var_Dict()},     {"bwDirVariable", wVar_Dict()},
      {"bDirEventType", Event_Dict()},
  };

  if (!include_dicts(module, dicts)) { return -1; }

  return 0;
}

PyModuleDef def = {
    .m_base = PyModuleDef_HEAD_INIT,
    .m_name = "bareosdir",
    .m_doc
    = "This module allows you to interact with the bareos director daemon",
    .m_size = sizeof(module_state),
    .m_methods = module_methods,
    .m_slots = nullptr,

    // we do not store python objects that _we_ need to keep alive
    .m_traverse = nullptr,
    .m_clear = nullptr,
    .m_free = nullptr,
};
};  // namespace

PyObject* make_module(PluginContext* ctx, directordaemon::CoreFunctions* funs)
{
  PyObject* mod = PyModule_Create(&def);
  if (!mod) { return nullptr; }

  if (setup_dicts(mod) < 0) {
    Py_DECREF(mod);
    return nullptr;
  }

  auto* state = get_state(mod);
  state->ctx = ctx;
  state->funs = *funs;

  return mod;
}
