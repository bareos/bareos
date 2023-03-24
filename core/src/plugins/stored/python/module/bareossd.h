/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2020-2023 Bareos GmbH & Co. KG

   This program is Free Software; you can modify it under the terms of
   version three of the GNU Affero General Public License as published by the
   Free Software Foundation, which is listed in the file LICENSE.

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
 * This defines the Python types in C++ and the callbacks from Python we
 * support.
 */

#ifndef BAREOS_PLUGINS_STORED_PYTHON_MODULE_BAREOSSD_H_
#define BAREOS_PLUGINS_STORED_PYTHON_MODULE_BAREOSSD_H_

#define PYTHON_MODULE_NAME bareossd
#define PYTHON_MODULE_NAME_QUOTED "bareossd"

/* common code for all python plugins */
#include "plugins/include/python_plugins_common.h"
#include "plugins/include/common.h"

#include "c_api/capi_1.inc"


#ifdef BAREOSSD_MODULE
/* This section is used when compiling bareossd.cc */

namespace storagedaemon {
/* include automatically generated C API */


// Callback methods from Python.
static PyObject* PyBareosGetValue(PyObject* self, PyObject* args);
static PyObject* PyBareosSetValue(PyObject* self, PyObject* args);
static PyObject* PyBareosDebugMessage(PyObject* self, PyObject* args);
static PyObject* PyBareosJobMessage(PyObject* self, PyObject* args);
static PyObject* PyBareosRegisterEvents(PyObject* self, PyObject* args);
static PyObject* PyBareosUnRegisterEvents(PyObject* self, PyObject* args);
static PyObject* PyBareosGetInstanceCount(PyObject* self, PyObject* args);

static PyMethodDef Methods[]
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
       {NULL, NULL, 0, NULL}};


static bRC set_bareos_core_functions(CoreFunctions* new_bareos_core_functions);
static bRC set_plugin_context(PluginContext* new_plugin_context);
static void PyErrorHandler(PluginContext* plugin_ctx, int msgtype);
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

} /* namespace storagedaemon*/
using namespace storagedaemon;

/* variables storing bareos pointers */
thread_local PluginContext* plugin_context = NULL;

MOD_INIT(bareossd)
{
  PyObject* m = NULL;
  MOD_DEF(m, PYTHON_MODULE_NAME_QUOTED, NULL, Methods)
  static void* Bareossd_API[Bareossd_API_pointers];
  PyObject* c_api_object;

  /* Initialize the C API pointer array */
#  include "c_api/capi_3.inc"

  /* Create a Capsule containing the API pointer array's address */
  c_api_object = PyCapsule_New((void*)Bareossd_API,
                               PYTHON_MODULE_NAME_QUOTED "._C_API", NULL);

  if (c_api_object != NULL) {
    PyModule_AddObject(m, "_C_API", c_api_object);
  } else {
    return MOD_ERROR_VAL;
  }

  /* module dictionaries */
  DEFINE_bRCs_DICT();
  DEFINE_bJobMessageTypes_DICT();

  const char* bsdVariable = "bsdVariable";
  PyObject* pDictsdVariable = NULL;
  pDictsdVariable = PyDict_New();
  if (!pDictsdVariable) { return MOD_ERROR_VAL; }
  ConstSet_StrLong(pDictsdVariable, bsdVarJob, 1);
  ConstSet_StrLong(pDictsdVariable, bsdVarLevel, 2);
  ConstSet_StrLong(pDictsdVariable, bsdVarType, 3);
  ConstSet_StrLong(pDictsdVariable, bsdVarJobId, 4);
  ConstSet_StrLong(pDictsdVariable, bsdVarClient, 5);
  ConstSet_StrLong(pDictsdVariable, bsdVarPool, 6);
  ConstSet_StrLong(pDictsdVariable, bsdVarPoolType, 7);
  ConstSet_StrLong(pDictsdVariable, bsdVarStorage, 8);
  ConstSet_StrLong(pDictsdVariable, bsdVarMediaType, 9);
  ConstSet_StrLong(pDictsdVariable, bsdVarJobName, 10);
  ConstSet_StrLong(pDictsdVariable, bsdVarJobStatus, 11);
  ConstSet_StrLong(pDictsdVariable, bsdVarVolumeName, 12);
  ConstSet_StrLong(pDictsdVariable, bsdVarJobErrors, 13);
  ConstSet_StrLong(pDictsdVariable, bsdVarJobFiles, 14);
  ConstSet_StrLong(pDictsdVariable, bsdVarJobBytes, 15);
  ConstSet_StrLong(pDictsdVariable, bsdVarPluginDir, 16);
  if (PyModule_AddObject(m, bsdVariable, pDictsdVariable)) {
    return MOD_ERROR_VAL;
  }

  const char* bsdwVariable = "bsdwVariable";
  PyObject* pDictsdwVariable = NULL;
  pDictsdwVariable = PyDict_New();
  if (!pDictsdwVariable) { return MOD_ERROR_VAL; }
  ConstSet_StrLong(pDictsdwVariable, bsdwVarJobReport, 1);
  ConstSet_StrLong(pDictsdwVariable, bsdwVarVolumeName, 2);
  ConstSet_StrLong(pDictsdwVariable, bsdwVarPriority, 3);
  ConstSet_StrLong(pDictsdwVariable, bsdwVarJobLevel, 4);
  if (PyModule_AddObject(m, bsdwVariable, pDictsdwVariable)) {
    return MOD_ERROR_VAL;
  }


  const char* bsdEventType = "bsdEventType";
  PyObject* pDictbsdEventType = NULL;
  pDictbsdEventType = PyDict_New();
  ConstSet_StrLong(pDictbsdEventType, bsdEventJobStart, 1);
  ConstSet_StrLong(pDictbsdEventType, bsdEventJobEnd, 2);
  ConstSet_StrLong(pDictbsdEventType, bsdEventDeviceInit, 3);
  ConstSet_StrLong(pDictbsdEventType, bsdEventDeviceMount, 4);
  ConstSet_StrLong(pDictbsdEventType, bsdEventVolumeLoad, 5);
  ConstSet_StrLong(pDictbsdEventType, bsdEventDeviceReserve, 6);
  ConstSet_StrLong(pDictbsdEventType, bsdEventDeviceOpen, 7);
  ConstSet_StrLong(pDictbsdEventType, bsdEventLabelRead, 8);
  ConstSet_StrLong(pDictbsdEventType, bsdEventLabelVerified, 9);
  ConstSet_StrLong(pDictbsdEventType, bsdEventLabelWrite, 10);
  ConstSet_StrLong(pDictbsdEventType, bsdEventDeviceClose, 11);
  ConstSet_StrLong(pDictbsdEventType, bsdEventVolumeUnload, 12);
  ConstSet_StrLong(pDictbsdEventType, bsdEventDeviceUnmount, 13);
  ConstSet_StrLong(pDictbsdEventType, bsdEventReadError, 14);
  ConstSet_StrLong(pDictbsdEventType, bsdEventWriteError, 15);
  ConstSet_StrLong(pDictbsdEventType, bsdEventDriveStatus, 16);
  ConstSet_StrLong(pDictbsdEventType, bsdEventVolumeStatus, 17);
  ConstSet_StrLong(pDictbsdEventType, bsdEventSetupRecordTranslation, 18);
  ConstSet_StrLong(pDictbsdEventType, bsdEventReadRecordTranslation, 19);
  ConstSet_StrLong(pDictbsdEventType, bsdEventWriteRecordTranslation, 20);
  ConstSet_StrLong(pDictbsdEventType, bsdEventDeviceRelease, 21);
  ConstSet_StrLong(pDictbsdEventType, bsdEventNewPluginOptions, 22);
  ConstSet_StrLong(pDictbsdEventType, bsdEventChangerLock, 23);
  ConstSet_StrLong(pDictbsdEventType, bsdEventChangerUnlock, 24);

  if (!pDictbsdEventType) { return MOD_ERROR_VAL; }
  if (PyModule_AddObject(m, bsdEventType, pDictbsdEventType)) {
    return MOD_ERROR_VAL;
  }

  return MOD_SUCCESS_VAL(m);
}


#else  // NOT BAREOSSD_MODULE


/* This section is used in modules that use bareossd's API */

static void** Bareossd_API;

/* include automatically generated C API */
#  include "c_api/capi_2.inc"

static int import_bareossd()
{
  Bareossd_API = (void**)PyCapsule_Import("bareossd._C_API", 0);
  return (Bareossd_API != NULL) ? 0 : -1;
}
#endif  // BAREOSSD_MODULE

#endif  // BAREOS_PLUGINS_STORED_PYTHON_MODULE_BAREOSSD_H_
