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
  storagedaemon::CoreFunctions funs;
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
                   storagedaemon::bsdrVariable var,
                   void* value)
{
  return state->funs.getBareosValue(state->ctx, var, value);
}
bRC setBareosValue(module_state* state,
                   storagedaemon::bsdwVariable var,
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
    using namespace storagedaemon;

    case bsdVarJobId:
    case bsdVarLevel:
    case bsdVarType:
    case bsdVarJobStatus: {
      int value = 0;

      if (getBareosValue(state, (bsdrVariable)var, &value) == bRC_OK) {
        pRetVal = PyLong_FromLong(value);
      }
    } break;
    case bsdVarJobErrors:
    case bsdVarJobFiles:
    case bsdVarJobBytes: {
      uint64_t value = 0;

      if (getBareosValue(state, (bsdrVariable)var, &value) == bRC_OK) {
        pRetVal = PyLong_FromUnsignedLong(value);
      }
    } break;
    case bsdVarJobName:
    case bsdVarJob:
    case bsdVarClient:
    case bsdVarPool:
    case bsdVarPoolType:
    case bsdVarStorage:
    case bsdVarMediaType:
    case bsdVarVolumeName: {
      char* value = NULL;

      if (getBareosValue(state, (bsdrVariable)var, &value) == bRC_OK) {
        if (value) { pRetVal = PyUnicode_FromString(value); }
      }
    } break;
    case bsdVarPluginDir: {
      char* value = NULL;

      if (getBareosValue(state, (bsdrVariable)var, &value) == bRC_OK) {
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
    using namespace storagedaemon;

    case bsdwVarVolumeName: {
      const char* value;

      value = PyUnicode_AsUTF8(pyValue);
      if (value) {
        retval = setBareosValue(state, (bsdwVariable)var,
                                static_cast<void*>(const_cast<char*>(value)));
      }

    } break;
    case bsdwVarPriority:
    case bsdwVarJobLevel: {
      int value;

      value = PyLong_AsLong(pyValue);
      if (value >= 0) {
        retval = setBareosValue(state, (bsdwVariable)var, &value);
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

    if (event >= storagedaemon::bSdEventJobStart
        && event <= storagedaemon::bSdEventWriteRecordTranslation) {
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

    if (event >= storagedaemon::bSdEventJobStart
        && event <= storagedaemon::bSdEventWriteRecordTranslation) {
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

#define set_enum_value_with_typo(dict, eval) \
  dict_insert_steal((dict), "bsd" #eval, make_python_object(bSd##eval))

PyObject* Var_Dict()
{
  PyObject* dict = PyDict_New();
  if (!dict) { return nullptr; }

  using namespace storagedaemon;

  if (!set_enum_value(dict, bsdVarJob) || !set_enum_value(dict, bsdVarLevel)
      || !set_enum_value(dict, bsdVarType) || !set_enum_value(dict, bsdVarJobId)
      || !set_enum_value(dict, bsdVarClient)
      || !set_enum_value(dict, bsdVarPool)
      || !set_enum_value(dict, bsdVarPoolType)
      || !set_enum_value(dict, bsdVarStorage)
      || !set_enum_value(dict, bsdVarMediaType)
      || !set_enum_value(dict, bsdVarJobName)
      || !set_enum_value(dict, bsdVarJobStatus)
      || !set_enum_value(dict, bsdVarVolumeName)
      || !set_enum_value(dict, bsdVarJobErrors)
      || !set_enum_value(dict, bsdVarJobFiles)
      || !set_enum_value(dict, bsdVarJobBytes)
      || !set_enum_value(dict, bsdVarPluginDir)) {
    Py_DECREF(dict);
    return nullptr;
  }

  return dict;
}

PyObject* wVar_Dict()
{
  PyObject* dict = PyDict_New();
  if (!dict) { return nullptr; }

  using namespace storagedaemon;

  if (!set_enum_value(dict, bsdwVarJobReport)
      || !set_enum_value(dict, bsdwVarVolumeName)
      || !set_enum_value(dict, bsdwVarPriority)
      || !set_enum_value(dict, bsdwVarJobLevel)) {
    Py_DECREF(dict);
    return nullptr;
  }

  return dict;
}

PyObject* Event_Dict()
{
  PyObject* dict = PyDict_New();
  if (!dict) { return nullptr; }

  using namespace storagedaemon;

  if (!set_enum_value_with_typo(dict, EventJobStart)
      || !set_enum_value_with_typo(dict, EventJobEnd)
      || !set_enum_value_with_typo(dict, EventDeviceInit)
      || !set_enum_value_with_typo(dict, EventDeviceMount)
      || !set_enum_value_with_typo(dict, EventVolumeLoad)
      || !set_enum_value_with_typo(dict, EventDeviceReserve)
      || !set_enum_value_with_typo(dict, EventDeviceOpen)
      || !set_enum_value_with_typo(dict, EventLabelRead)
      || !set_enum_value_with_typo(dict, EventLabelVerified)
      || !set_enum_value_with_typo(dict, EventLabelWrite)
      || !set_enum_value_with_typo(dict, EventDeviceClose)
      || !set_enum_value_with_typo(dict, EventVolumeUnload)
      || !set_enum_value_with_typo(dict, EventDeviceUnmount)
      || !set_enum_value_with_typo(dict, EventReadError)
      || !set_enum_value_with_typo(dict, EventWriteError)
      || !set_enum_value_with_typo(dict, EventDriveStatus)
      || !set_enum_value_with_typo(dict, EventVolumeStatus)
      || !set_enum_value_with_typo(dict, EventSetupRecordTranslation)
      || !set_enum_value_with_typo(dict, EventReadRecordTranslation)
      || !set_enum_value_with_typo(dict, EventWriteRecordTranslation)
      || !set_enum_value_with_typo(dict, EventDeviceRelease)
      || !set_enum_value_with_typo(dict, EventNewPluginOptions)
      || !set_enum_value_with_typo(dict, EventChangerLock)
      || !set_enum_value_with_typo(dict, EventChangerUnlock)) {
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
      {"bsdVariable", Var_Dict()},      {"bsdwVariable", wVar_Dict()},
      {"bsdEventType", Event_Dict()},
  };

  if (!include_dicts(module, dicts)) { return -1; }

  return 0;
}

PyModuleDef def = {
    .m_base = PyModuleDef_HEAD_INIT,
    .m_name = "bareossd",
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

PyObject* make_module(PluginContext* ctx, storagedaemon::CoreFunctions* funs)
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
