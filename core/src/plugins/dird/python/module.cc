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
#include "modsupport.h"

module_state* get_state(PyObject* module)
{
  auto* state = static_cast<module_state*>(PyModule_GetState(module));
  return state;
}

namespace {
int debuglevel = 100;

// Convert a return value from bRC enum value into Python Object.
PyObject* ConvertbRCRetvalToPythonRetval(bRC retval)
{
  return (PyObject*)PyLong_FromLong((int)retval);
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

static PyMethodDef module_methods[]
    = {{"GetValue", directordaemon::PyBareosGetValue, METH_VARARGS,
        "Get a Plugin value"},
       {"SetValue", directordaemon::PyBareosSetValue, METH_VARARGS,
        "Set a Plugin value"},
       {"DebugMessage", directordaemon::PyBareosDebugMessage, METH_VARARGS,
        "Print a Debug message"},
       {"JobMessage", directordaemon::PyBareosJobMessage, METH_VARARGS,
        "Print a Job message"},
       {"RegisterEvents", directordaemon::PyBareosRegisterEvents, METH_VARARGS,
        "Register Plugin Events"},
       {"UnRegisterEvents", directordaemon::PyBareosUnRegisterEvents,
        METH_VARARGS, "Unregister Plugin Events"},
       {"GetInstanceCount", directordaemon::PyBareosGetInstanceCount,
        METH_VARARGS, "Get number of instances of current plugin"},
       {}};

/* define the given string constant as member of an dict and additionally
 add it directly as constant to the module.

 This is both done for string to long and for string to string mappings.

 The result is that the module both contains a dict:
 bIOPS = {'IO_CLOSE': 4L, 'IO_OPEN': 1L, 'IO_READ': 2L, 'IO_SEEK': 5L,
          'IO_WRITE': 3L}
 and the single values directly:
     IO_CLOSE = 4
     IO_OPEN = 1
     IO_READ = 2
     IO_SEEK = 5
     IO_WRITE = 3
 */

namespace {
// PyObject* make_python_object(const char* data)
// {
//   return PyBytes_FromString(data);
// }

PyObject* make_python_object(long data) { return PyLong_FromLong(data); }

bool set_value(PyObject* dict, const char* key, auto value)
{
  bool ok = true;
  PyObject* val = make_python_object(value);
  if (PyDict_SetItemString(dict, key, val) < 0) { ok = false; }
  Py_XDECREF(val);

  return ok;
}
};  // namespace

#define set_enum_value(dict, eval) set_value((dict), #eval, eval)

PyObject* JMsg_Dict()
{
  PyObject* dict = PyDict_New();
  if (!dict) { return nullptr; }

  set_enum_value(dict, M_ABORT);
  set_enum_value(dict, M_DEBUG);
  set_enum_value(dict, M_FATAL);
  set_enum_value(dict, M_ERROR);
  set_enum_value(dict, M_WARNING);
  set_enum_value(dict, M_INFO);
  set_enum_value(dict, M_SAVED);
  set_enum_value(dict, M_NOTSAVED);
  set_enum_value(dict, M_SKIPPED);
  set_enum_value(dict, M_MOUNT);
  set_enum_value(dict, M_ERROR_TERM);
  set_enum_value(dict, M_TERM);
  set_enum_value(dict, M_RESTORED);
  set_enum_value(dict, M_SECURITY);
  set_enum_value(dict, M_ALERT);
  set_enum_value(dict, M_VOLMGMT);

  return dict;
}

PyObject* bRC_Dict()
{
  PyObject* dict = PyDict_New();
  if (!dict) { return nullptr; }

  set_enum_value(dict, bRC_OK);
  set_enum_value(dict, bRC_Stop);
  set_enum_value(dict, bRC_Error);
  set_enum_value(dict, bRC_More);
  set_enum_value(dict, bRC_Term);
  set_enum_value(dict, bRC_Seen);
  set_enum_value(dict, bRC_Core);
  set_enum_value(dict, bRC_Skip);
  set_enum_value(dict, bRC_Cancel);

  return dict;
}

bool include_dict(PyObject* module, const char* dict_name, PyObject* dict)
{
  PyObject* module_dict = PyModule_GetDict(module);
  if (!module_dict) { return false; }

  if (PyDict_Merge(module_dict, dict, 1) < 0) {
    Py_DECREF(dict);
    return false;
  }
  if (PyModule_AddObject(module, dict_name, dict) < 0) {
    Py_DECREF(dict);
    return false;
  }

  return true;
}

static int setup_dicts(PyObject* module)
{
  if (PyObject* dict = JMsg_Dict()) {
    if (!include_dict(module, "bJobMessageType", dict)) { return -1; }
  } else {
    return -1;
  }

  if (PyObject* dict = bRC_Dict()) {
    if (!include_dict(module, "bRCs", dict)) { return -1; }
  } else {
    return -1;
  }

  if (PyObject* dict = PyDict_New()) {
    using namespace directordaemon;

    if (0 || !set_enum_value(dict, bDirVarJob)
        || !set_enum_value(dict, bDirVarLevel)
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
      return -1;
    }

    if (!include_dict(module, "bDirVariable", dict)) { return -1; }
  } else {
    return -1;
  }

  if (PyObject* dict = PyDict_New()) {
    using namespace directordaemon;

    if (0 || !set_enum_value(dict, bwDirVarJobReport)
        || !set_enum_value(dict, bwDirVarVolumeName)
        || !set_enum_value(dict, bwDirVarPriority)
        || !set_enum_value(dict, bwDirVarJobLevel)) {
      Py_DECREF(dict);
      return -1;
    }

    if (!include_dict(module, "bwDirVariable", dict)) { return -1; }
  } else {
    return -1;
  }

  if (PyObject* dict = PyDict_New()) {
    using namespace directordaemon;

    if (0 || !set_enum_value(dict, bDirEventJobStart)
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
      return -1;
    }

    if (!include_dict(module, "bDirEventType", dict)) { return -1; }
  } else {
    return -1;
  }

  return 0;
}

#undef set_enum_value

static PyModuleDef def = {
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
