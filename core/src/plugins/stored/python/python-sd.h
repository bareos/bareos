/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2013-2014 Planets Communications B.V.
   Copyright (C) 2013-2020 Bareos GmbH & Co. KG

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

#ifndef BAREOS_PLUGINS_STORED_PYTHON_SD_H_
#define BAREOS_PLUGINS_STORED_PYTHON_SD_H_ 1

#define PYTHON_MODULE_NAME bareossd
#define PYTHON_MODULE_NAME_QUOTED "bareossd"

/* common code for all python plugins */
#include "plugins/python_plugins_common.h"
#include "plugins/filed/fd_common.h"

namespace storagedaemon {

/**
 * This defines the arguments that the plugin parser understands.
 */
enum plugin_argument_type
{
  argument_none,
  argument_instance,
  argument_module_path,
  argument_module_name
};

struct plugin_argument {
  const char* name;
  enum plugin_argument_type type;
};

static plugin_argument plugin_arguments[] = {
    {"instance", argument_instance},
    {"module_path", argument_module_path},
    {"module_name", argument_module_name},
    {NULL, argument_none}};
/**
 * Callback methods from Python.
 */
static PyObject* PyBareosGetValue(PyObject* self, PyObject* args);
static PyObject* PyBareosSetValue(PyObject* self, PyObject* args);
static PyObject* PyBareosDebugMessage(PyObject* self, PyObject* args);
static PyObject* PyBareosJobMessage(PyObject* self, PyObject* args);
static PyObject* PyBareosRegisterEvents(PyObject* self, PyObject* args);
static PyObject* PyBareosUnRegisterEvents(PyObject* self, PyObject* args);
static PyObject* PyBareosGetInstanceCount(PyObject* self, PyObject* args);

static PyMethodDef Methods[] = {
    {"GetValue", PyBareosGetValue, METH_VARARGS, "Get a Plugin value"},
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


static void* bareos_plugin_context = NULL;

// MOD_INIT(PYTHON_MODULE_NAME)
MOD_INIT(bareossd)
{
  /* bareos_plugin_context holds the bpContext instead of passing to Python and
   * extracting it back like it was before. bareos_plugin_context needs to be
   * set after loading the PYTHON_MODULE_NAME binary python module and will be
   * used for all calls.
   */

  PyObject* m = NULL;

  /* Pointer Capsules to avoid context transfer back and forth */
  PyObject* PyModulePluginContext =
      PyCapsule_New((void*)&bareos_plugin_context,
                    PYTHON_MODULE_NAME_QUOTED ".bpContext", NULL);

  if (!PyModulePluginContext) {
    printf(PYTHON_MODULE_NAME_QUOTED ": PyCapsule_New failed\n");
    return MOD_ERROR_VAL;
  }

  MOD_DEF(m, PYTHON_MODULE_NAME_QUOTED, NULL, Methods)

  if (PyModulePluginContext) {
    PyModule_AddObject(m, "bpContext", PyModulePluginContext);
  } else {
    printf(PYTHON_MODULE_NAME_QUOTED ":PyModule_AddObject failed\n");
    return MOD_ERROR_VAL;
  }

  /* module dictionaries */
  DEFINE_bRCs_DICT();
  DEFINE_bJobMessageTypes_DICT();

  const char* bsdVariable = "bsdVariable";
  PyObject* pDictsdVariable = NULL;
  pDictsdVariable = PyDict_New();
  if (!pDictsdVariable) { return MOD_ERROR_VAL; }
  DictSet_StrLong(pDictsdVariable, bsdVarJob, 1);
  DictSet_StrLong(pDictsdVariable, bsdVarLevel, 2);
  DictSet_StrLong(pDictsdVariable, bsdVarType, 3);
  DictSet_StrLong(pDictsdVariable, bsdVarJobId, 4);
  DictSet_StrLong(pDictsdVariable, bsdVarClient, 5);
  DictSet_StrLong(pDictsdVariable, bsdVarPool, 6);
  DictSet_StrLong(pDictsdVariable, bsdVarPoolType, 7);
  DictSet_StrLong(pDictsdVariable, bsdVarStorage, 8);
  DictSet_StrLong(pDictsdVariable, bsdVarMediaType, 9);
  DictSet_StrLong(pDictsdVariable, bsdVarJobName, 10);
  DictSet_StrLong(pDictsdVariable, bsdVarJobStatus, 11);
  DictSet_StrLong(pDictsdVariable, bsdVarVolumeName, 12);
  DictSet_StrLong(pDictsdVariable, bsdVarJobErrors, 13);
  DictSet_StrLong(pDictsdVariable, bsdVarJobFiles, 14);
  DictSet_StrLong(pDictsdVariable, bsdVarJobBytes, 15);
  DictSet_StrLong(pDictsdVariable, bsdVarCompatible, 16);
  DictSet_StrLong(pDictsdVariable, bsdVarPluginDir, 17);
  if (PyModule_AddObject(m, bsdVariable, pDictsdVariable)) {
    return MOD_ERROR_VAL;
  }

  const char* bsdwVariable = "bsdwVariable";
  PyObject* pDictsdwVariable = NULL;
  pDictsdwVariable = PyDict_New();
  if (!pDictsdwVariable) { return MOD_ERROR_VAL; }
  DictSet_StrLong(pDictsdwVariable, bsdwVarJobReport, 1);
  DictSet_StrLong(pDictsdwVariable, bsdwVarVolumeName, 2);
  DictSet_StrLong(pDictsdwVariable, bsdwVarPriority, 3);
  DictSet_StrLong(pDictsdwVariable, bsdwVarJobLevel, 4);
  if (PyModule_AddObject(m, bsdwVariable, pDictsdwVariable)) {
    return MOD_ERROR_VAL;
  }


  const char* bsdEventType = "bsdEventType";
  PyObject* pDictbsdEventType = NULL;
  pDictbsdEventType = PyDict_New();
  DictSet_StrLong(pDictbsdEventType, bsdEventJobStart, 1);
  DictSet_StrLong(pDictbsdEventType, bsdEventJobEnd, 2);
  DictSet_StrLong(pDictbsdEventType, bsdEventDeviceInit, 3);
  DictSet_StrLong(pDictbsdEventType, bsdEventDeviceMount, 4);
  DictSet_StrLong(pDictbsdEventType, bsdEventVolumeLoad, 5);
  DictSet_StrLong(pDictbsdEventType, bsdEventDeviceReserve, 6);
  DictSet_StrLong(pDictbsdEventType, bsdEventDeviceOpen, 7);
  DictSet_StrLong(pDictbsdEventType, bsdEventLabelRead, 8);
  DictSet_StrLong(pDictbsdEventType, bsdEventLabelVerified, 9);
  DictSet_StrLong(pDictbsdEventType, bsdEventLabelWrite, 10);
  DictSet_StrLong(pDictbsdEventType, bsdEventDeviceClose, 11);
  DictSet_StrLong(pDictbsdEventType, bsdEventVolumeUnload, 12);
  DictSet_StrLong(pDictbsdEventType, bsdEventDeviceUnmount, 13);
  DictSet_StrLong(pDictbsdEventType, bsdEventReadError, 14);
  DictSet_StrLong(pDictbsdEventType, bsdEventWriteError, 15);
  DictSet_StrLong(pDictbsdEventType, bsdEventDriveStatus, 16);
  DictSet_StrLong(pDictbsdEventType, bsdEventVolumeStatus, 17);
  DictSet_StrLong(pDictbsdEventType, bsdEventSetupRecordTranslation, 18);
  DictSet_StrLong(pDictbsdEventType, bsdEventReadRecordTranslation, 19);
  DictSet_StrLong(pDictbsdEventType, bsdEventWriteRecordTranslation, 20);
  DictSet_StrLong(pDictbsdEventType, bsdEventDeviceRelease, 21);
  DictSet_StrLong(pDictbsdEventType, bsdEventNewPluginOptions, 22);
  DictSet_StrLong(pDictbsdEventType, bsdEventChangerLock, 23);
  DictSet_StrLong(pDictbsdEventType, bsdEventChangerUnlock, 24);

  if (!pDictbsdEventType) { return MOD_ERROR_VAL; }
  if (PyModule_AddObject(m, bsdEventType, pDictbsdEventType)) {
    return MOD_ERROR_VAL;
  }

  return MOD_SUCCESS_VAL(m);
}

} /* namespace storagedaemon*/
#endif /* BAREOS_PLUGINS_STORED_PYTHON_SD_H_ */
