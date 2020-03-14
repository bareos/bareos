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

#ifndef BAREOS_PLUGINS_DIRD_PYTHON_DIR_H_
#define BAREOS_PLUGINS_DIRD_PYTHON_DIR_H_ 1

#define PYTHON_MODULE_NAME bareosdir
#define PYTHON_MODULE_NAME_QUOTED "bareosdir"

/* common code for all python plugins */
#include "plugins/python_plugins_common.h"
#include "plugins/filed/fd_common.h"

namespace directordaemon {

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
MOD_INIT(bareosdir)
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
  const char* bJobMessageType = "bJobMessageType";
  PyObject* pDictJobMessageType = NULL;
  pDictJobMessageType = PyDict_New();
  if (!pDictJobMessageType) { return MOD_ERROR_VAL; }
  DictSet_StrLong(pDictJobMessageType, M_ABORT, 1);
  DictSet_StrLong(pDictJobMessageType, M_DEBUG, 2);
  DictSet_StrLong(pDictJobMessageType, M_FATAL, 3);
  DictSet_StrLong(pDictJobMessageType, M_ERROR, 4);
  DictSet_StrLong(pDictJobMessageType, M_WARNING, 5);
  DictSet_StrLong(pDictJobMessageType, M_INFO, 6);
  DictSet_StrLong(pDictJobMessageType, M_SAVED, 7);
  DictSet_StrLong(pDictJobMessageType, M_NOTSAVED, 8);
  DictSet_StrLong(pDictJobMessageType, M_SKIPPED, 9);
  DictSet_StrLong(pDictJobMessageType, M_MOUNT, 10);
  DictSet_StrLong(pDictJobMessageType, M_ERROR_TERM, 11);
  DictSet_StrLong(pDictJobMessageType, M_TERM, 12);
  DictSet_StrLong(pDictJobMessageType, M_RESTORED, 13);
  DictSet_StrLong(pDictJobMessageType, M_SECURITY, 14);
  DictSet_StrLong(pDictJobMessageType, M_ALERT, 15);
  DictSet_StrLong(pDictJobMessageType, M_VOLMGMT, 16);
  if (PyModule_AddObject(m, bJobMessageType, pDictJobMessageType)) {
    return MOD_ERROR_VAL;
  }

  const char* bDirVariable = "bDirVariable";
  PyObject* pDictbDirVariable = NULL;
  pDictbDirVariable = PyDict_New();
  if (!pDictbDirVariable) { return MOD_ERROR_VAL; }
  DictSet_StrLong(pDictbDirVariable, bDirVarJob, 1);
  DictSet_StrLong(pDictbDirVariable, bDirVarLevel, 2);
  DictSet_StrLong(pDictbDirVariable, bDirVarType, 3);
  DictSet_StrLong(pDictbDirVariable, bDirVarJobId, 4);
  DictSet_StrLong(pDictbDirVariable, bDirVarClient, 5);
  DictSet_StrLong(pDictbDirVariable, bDirVarNumVols, 6);
  DictSet_StrLong(pDictbDirVariable, bDirVarPool, 7);
  DictSet_StrLong(pDictbDirVariable, bDirVarStorage, 8);
  DictSet_StrLong(pDictbDirVariable, bDirVarWriteStorage, 9);
  DictSet_StrLong(pDictbDirVariable, bDirVarReadStorage, 10);
  DictSet_StrLong(pDictbDirVariable, bDirVarCatalog, 11);
  DictSet_StrLong(pDictbDirVariable, bDirVarMediaType, 12);
  DictSet_StrLong(pDictbDirVariable, bDirVarJobName, 13);
  DictSet_StrLong(pDictbDirVariable, bDirVarJobStatus, 14);
  DictSet_StrLong(pDictbDirVariable, bDirVarPriority, 15);
  DictSet_StrLong(pDictbDirVariable, bDirVarVolumeName, 16);
  DictSet_StrLong(pDictbDirVariable, bDirVarCatalogRes, 17);
  DictSet_StrLong(pDictbDirVariable, bDirVarJobErrors, 18);
  DictSet_StrLong(pDictbDirVariable, bDirVarJobFiles, 19);
  DictSet_StrLong(pDictbDirVariable, bDirVarSDJobFiles, 20);
  DictSet_StrLong(pDictbDirVariable, bDirVarSDErrors, 21);
  DictSet_StrLong(pDictbDirVariable, bDirVarFDJobStatus, 22);
  DictSet_StrLong(pDictbDirVariable, bDirVarSDJobStatus, 23);
  DictSet_StrLong(pDictbDirVariable, bDirVarPluginDir, 24);
  DictSet_StrLong(pDictbDirVariable, bDirVarLastRate, 25);
  DictSet_StrLong(pDictbDirVariable, bDirVarJobBytes, 26);
  DictSet_StrLong(pDictbDirVariable, bDirVarReadBytes, 27);
  if (PyModule_AddObject(m, bDirVariable, pDictbDirVariable)) {
    return MOD_ERROR_VAL;
  }

  const char* bwDirVariable = "bwDirVariable";
  PyObject* pDictbwDirVariable = NULL;
  pDictbwDirVariable = PyDict_New();
  if (!pDictbwDirVariable) { return MOD_ERROR_VAL; }
  DictSet_StrLong(pDictbwDirVariable, bwDirVarJobReport, 1);
  DictSet_StrLong(pDictbwDirVariable, bwDirVarVolumeName, 2);
  DictSet_StrLong(pDictbwDirVariable, bwDirVarPriority, 3);
  DictSet_StrLong(pDictbwDirVariable, bwDirVarJobLevel, 4);
  if (PyModule_AddObject(m, bwDirVariable, pDictbwDirVariable)) {
    return MOD_ERROR_VAL;
  }

  const char* bRCs = "bRCs";
  PyObject* pDictbRCs = NULL;
  pDictbRCs = PyDict_New();
  DictSet_StrLong(pDictbRCs, bRC_OK, 0);
  DictSet_StrLong(pDictbRCs, bRC_Stop, 1);
  DictSet_StrLong(pDictbRCs, bRC_Error, 2);
  DictSet_StrLong(pDictbRCs, bRC_More, 3);
  DictSet_StrLong(pDictbRCs, bRC_Term, 4);
  DictSet_StrLong(pDictbRCs, bRC_Seen, 5);
  DictSet_StrLong(pDictbRCs, bRC_Core, 6);
  DictSet_StrLong(pDictbRCs, bRC_Skip, 7);
  DictSet_StrLong(pDictbRCs, bRC_Cancel, 8);
  if (!pDictbRCs) { return MOD_ERROR_VAL; }
  if (PyModule_AddObject(m, bRCs, pDictbRCs)) { return MOD_ERROR_VAL; }

  const char* bDirEvent = "bDirEvent";
  PyObject* pDictbDirEvent = NULL;
  pDictbDirEvent = PyDict_New();
  DictSet_StrLong(pDictbDirEvent, bEventJobStart, 1);
  DictSet_StrLong(pDictbDirEvent, bEventJobEnd, 2);
  DictSet_StrLong(pDictbDirEvent, bDirEventJobInit, 3);
  DictSet_StrLong(pDictbDirEvent, bDirEventJobRun, 4);
  DictSet_StrLong(pDictbDirEvent, bDirEventVolumePurged, 5);
  DictSet_StrLong(pDictbDirEvent, bDirEventNewVolume, 6);
  DictSet_StrLong(pDictbDirEvent, bDirEventNeedVolume, 7);
  DictSet_StrLong(pDictbDirEvent, bDirEventVolumeFull, 8);
  DictSet_StrLong(pDictbDirEvent, bDirEventRecyle, 9);
  DictSet_StrLong(pDictbDirEvent, bDirEventGetScratch, 10);
  DictSet_StrLong(pDictbDirEvent, bDirEventNewPluginOptions, 11);
  if (!pDictbDirEvent) { return MOD_ERROR_VAL; }
  if (PyModule_AddObject(m, bDirEvent, pDictbDirEvent)) {
    return MOD_ERROR_VAL;
  }


  return MOD_SUCCESS_VAL(m);
}

} /* namespace directordaemon */
#endif /* BAREOS_PLUGINS_DIRD_PYTHON_DIR_H_ */
