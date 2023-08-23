/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2020-2021 Bareos GmbH & Co. KG

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

#ifndef BAREOS_PLUGINS_DIRD_PYTHON_MODULE_BAREOSDIR_H_
#define BAREOS_PLUGINS_DIRD_PYTHON_MODULE_BAREOSDIR_H_

#define PYTHON_MODULE_NAME bareosdir
#define PYTHON_MODULE_NAME_QUOTED "bareosdir"

/* common code for all python plugins */
#include "plugins/include/python_plugins_common.h"


/* include automatically generated C API */
#include "c_api/capi_1.inc"


#ifdef BAREOSDIR_MODULE
/* This section is used when compiling bareosdir.cc */

namespace directordaemon {

// Python structures mapping C++ ones.

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
                               bDirEvent* event,
                               void* value);

}  // namespace directordaemon
using namespace directordaemon;

/* variables storing bareos pointers */
thread_local PluginContext* plugin_context = NULL;

MOD_INIT(bareosdir)
{
  PyObject* m = NULL;
  MOD_DEF(m, PYTHON_MODULE_NAME_QUOTED, NULL, Methods)
  static void* Bareosdir_API[Bareosdir_API_pointers];
  PyObject* c_api_object;

  /* Initialize the C API pointer array */
#  include "c_api/capi_3.inc"

  /* Create a Capsule containing the API pointer array's address */
  c_api_object = PyCapsule_New((void*)Bareosdir_API,
                               PYTHON_MODULE_NAME_QUOTED "._C_API", NULL);

  if (c_api_object != NULL) {
    PyModule_AddObject(m, "_C_API", c_api_object);
  } else {
    return MOD_ERROR_VAL;
  }

  /* module dictionaries */
  DEFINE_bRCs_DICT();
  DEFINE_bJobMessageTypes_DICT();

  const char* bDirVariable = "bDirVariable";
  PyObject* pDictbDirVariable = NULL;
  pDictbDirVariable = PyDict_New();
  if (!pDictbDirVariable) { return MOD_ERROR_VAL; }
  ConstSet_StrLong(pDictbDirVariable, bDirVarJob, 1);
  ConstSet_StrLong(pDictbDirVariable, bDirVarLevel, 2);
  ConstSet_StrLong(pDictbDirVariable, bDirVarType, 3);
  ConstSet_StrLong(pDictbDirVariable, bDirVarJobId, 4);
  ConstSet_StrLong(pDictbDirVariable, bDirVarClient, 5);
  ConstSet_StrLong(pDictbDirVariable, bDirVarNumVols, 6);
  ConstSet_StrLong(pDictbDirVariable, bDirVarPool, 7);
  ConstSet_StrLong(pDictbDirVariable, bDirVarStorage, 8);
  ConstSet_StrLong(pDictbDirVariable, bDirVarWriteStorage, 9);
  ConstSet_StrLong(pDictbDirVariable, bDirVarReadStorage, 10);
  ConstSet_StrLong(pDictbDirVariable, bDirVarCatalog, 11);
  ConstSet_StrLong(pDictbDirVariable, bDirVarMediaType, 12);
  ConstSet_StrLong(pDictbDirVariable, bDirVarJobName, 13);
  ConstSet_StrLong(pDictbDirVariable, bDirVarJobStatus, 14);
  ConstSet_StrLong(pDictbDirVariable, bDirVarPriority, 15);
  ConstSet_StrLong(pDictbDirVariable, bDirVarVolumeName, 16);
  ConstSet_StrLong(pDictbDirVariable, bDirVarCatalogRes, 17);
  ConstSet_StrLong(pDictbDirVariable, bDirVarJobErrors, 18);
  ConstSet_StrLong(pDictbDirVariable, bDirVarJobFiles, 19);
  ConstSet_StrLong(pDictbDirVariable, bDirVarSDJobFiles, 20);
  ConstSet_StrLong(pDictbDirVariable, bDirVarSDErrors, 21);
  ConstSet_StrLong(pDictbDirVariable, bDirVarFDJobStatus, 22);
  ConstSet_StrLong(pDictbDirVariable, bDirVarSDJobStatus, 23);
  ConstSet_StrLong(pDictbDirVariable, bDirVarPluginDir, 24);
  ConstSet_StrLong(pDictbDirVariable, bDirVarJobBytes, 25);
  ConstSet_StrLong(pDictbDirVariable, bDirVarReadBytes, 26);
  if (PyModule_AddObject(m, bDirVariable, pDictbDirVariable)) {
    return MOD_ERROR_VAL;
  }

  const char* bwDirVariable = "bwDirVariable";
  PyObject* pDictbwDirVariable = NULL;
  pDictbwDirVariable = PyDict_New();
  if (!pDictbwDirVariable) { return MOD_ERROR_VAL; }
  ConstSet_StrLong(pDictbwDirVariable, bwDirVarJobReport, 1);
  ConstSet_StrLong(pDictbwDirVariable, bwDirVarVolumeName, 2);
  ConstSet_StrLong(pDictbwDirVariable, bwDirVarPriority, 3);
  ConstSet_StrLong(pDictbwDirVariable, bwDirVarJobLevel, 4);
  if (PyModule_AddObject(m, bwDirVariable, pDictbwDirVariable)) {
    return MOD_ERROR_VAL;
  }

  const char* bDirEvent = "bDirEventType";
  PyObject* pDictbDirEvent = NULL;
  pDictbDirEvent = PyDict_New();
  ConstSet_StrLong(pDictbDirEvent, bDirEventJobStart, 1);
  ConstSet_StrLong(pDictbDirEvent, bDirEventJobEnd, 2);
  ConstSet_StrLong(pDictbDirEvent, bDirEventJobInit, 3);
  ConstSet_StrLong(pDictbDirEvent, bDirEventJobRun, 4);
  ConstSet_StrLong(pDictbDirEvent, bDirEventVolumePurged, 5);
  ConstSet_StrLong(pDictbDirEvent, bDirEventNewVolume, 6);
  ConstSet_StrLong(pDictbDirEvent, bDirEventNeedVolume, 7);
  ConstSet_StrLong(pDictbDirEvent, bDirEventVolumeFull, 8);
  ConstSet_StrLong(pDictbDirEvent, bDirEventRecyle, 9);
  ConstSet_StrLong(pDictbDirEvent, bDirEventGetScratch, 10);
  ConstSet_StrLong(pDictbDirEvent, bDirEventNewPluginOptions, 11);
  if (!pDictbDirEvent) { return MOD_ERROR_VAL; }
  if (PyModule_AddObject(m, bDirEvent, pDictbDirEvent)) {
    return MOD_ERROR_VAL;
  }


  return MOD_SUCCESS_VAL(m);
}


#else  // NOT BAREOSDIR_MODULE


/* This section is used in modules that use bareosdir's API */

static void** Bareosdir_API;

/* include automatically generated C API */
#  include "c_api/capi_2.inc"

static int import_bareosdir()
{
  Bareosdir_API = (void**)PyCapsule_Import("bareosdir._C_API", 0);
  return (Bareosdir_API != NULL) ? 0 : -1;
}
#endif  // BAREOSDIR_MODULE

#endif  // BAREOS_PLUGINS_DIRD_PYTHON_MODULE_BAREOSDIR_H_
