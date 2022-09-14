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

/* Load the python-fd extension module and test it */

#if defined(HAVE_MINGW)
#  include "include/bareos.h"
#endif

#include "Python.h"
#include "plugins/include/python3compat.h"
#include <inttypes.h>
class PoolMem;
#define NbytesForBits(n) ((((n)-1) >> 3) + 1)
typedef off_t boffset_t;

#include "lib/plugins.h"
#include "filed/fd_plugins.h"
#include "../module/bareosfd.h"
static void PyErrorHandler()
{
  PyObject *type, *value, *traceback;
  PyObject* tracebackModule;
  char* error_string;

  PyErr_Fetch(&type, &value, &traceback);
  PyErr_NormalizeException(&type, &value, &traceback);

  tracebackModule = PyImport_ImportModule("traceback");
  if (tracebackModule != NULL) {
    PyObject *tbList, *emptyString, *strRetval;

    tbList = PyObject_CallMethod(tracebackModule, (char*)"format_exception",
                                 (char*)"OOO", type,
                                 value == NULL ? Py_None : value,
                                 traceback == NULL ? Py_None : traceback);

    emptyString = PyUnicode_FromString("");
    strRetval
        = PyObject_CallMethod(emptyString, (char*)"join", (char*)"O", tbList);

    error_string = strdup(PyUnicode_AsUTF8(strRetval));

    Py_DECREF(tbList);
    Py_DECREF(emptyString);
    Py_DECREF(strRetval);
    Py_DECREF(tracebackModule);
  } else {
    error_string = strdup("Unable to import traceback module.");
  }
  Py_DECREF(type);
  Py_XDECREF(value);
  Py_XDECREF(traceback);
  printf("%s", error_string);

  free(error_string);
  exit(1);
}

using namespace filedaemon;
bRC bareosRegisterEvents([[maybe_unused]] PluginContext* ctx,
                         [[maybe_unused]] int nr_events,
                         ...)
{
  return bRC_OK;
};
bRC bareosUnRegisterEvents([[maybe_unused]] PluginContext* ctx,
                           [[maybe_unused]] int nr_events,
                           ...)
{
  return bRC_OK;
};
bRC bareosGetInstanceCount([[maybe_unused]] PluginContext* ctx,
                           [[maybe_unused]] int* ret)
{
  return bRC_OK;
};
bRC bareosGetValue([[maybe_unused]] PluginContext* ctx,
                   [[maybe_unused]] filedaemon::bVariable var,
                   [[maybe_unused]] void* value)
{
  return bRC_OK;
};
bRC bareosSetValue([[maybe_unused]] PluginContext* ctx,
                   [[maybe_unused]] filedaemon::bVariable var,
                   [[maybe_unused]] void* value)
{
  return bRC_OK;
};
bRC bareosJobMsg([[maybe_unused]] PluginContext* ctx,
                 const char* file,
                 int line,
                 int type,
                 utime_t mtime,
                 const char* fmt,
                 ...)
{
  printf("bareosJobMsg file:%s line:%d type:%d time: %+" PRId64 ", fmt:%s\n",
         file, line, type, (int64_t)mtime, fmt);
  return bRC_OK;
};
bRC bareosDebugMsg([[maybe_unused]] PluginContext* ctx,
                   const char* file,
                   int line,
                   int level,
                   const char* fmt,
                   ...)
{
  printf("bareosDebugMsg file:%s line:%d level:%d fmt:%s\n", file, line, level,
         fmt);
  return bRC_OK;
};
void* bareosMalloc([[maybe_unused]] PluginContext* ctx,
                   [[maybe_unused]] const char* file,
                   [[maybe_unused]] int line,
                   [[maybe_unused]] size_t size)
{
  return NULL;
};
void bareosFree([[maybe_unused]] PluginContext* ctx,
                [[maybe_unused]] const char* file,
                [[maybe_unused]] int line,
                [[maybe_unused]] void* mem)
{
  return;
};
bRC bareosAddExclude([[maybe_unused]] PluginContext* ctx,
                     [[maybe_unused]] const char* file)
{
  return bRC_OK;
};
bRC bareosAddInclude([[maybe_unused]] PluginContext* ctx,
                     [[maybe_unused]] const char* file)
{
  return bRC_OK;
};
bRC bareosAddOptions([[maybe_unused]] PluginContext* ctx,
                     [[maybe_unused]] const char* opts)
{
  return bRC_OK;
};
bRC bareosAddRegex([[maybe_unused]] PluginContext* ctx,
                   [[maybe_unused]] const char* item,
                   [[maybe_unused]] int type)
{
  return bRC_OK;
};
bRC bareosAddWild([[maybe_unused]] PluginContext* ctx,
                  [[maybe_unused]] const char* item,
                  [[maybe_unused]] int type)
{
  return bRC_OK;
};
bRC bareosNewOptions([[maybe_unused]] PluginContext* ctx) { return bRC_OK; };
bRC bareosNewInclude([[maybe_unused]] PluginContext* ctx) { return bRC_OK; };
bRC bareosNewPreInclude([[maybe_unused]] PluginContext* ctx) { return bRC_OK; };
bRC bareosCheckChanges([[maybe_unused]] PluginContext* ctx,
                       [[maybe_unused]] struct filedaemon::save_pkt* sp)
{
  return bRC_OK;
};
bRC bareosAcceptFile([[maybe_unused]] PluginContext* ctx,
                     [[maybe_unused]] struct filedaemon::save_pkt* sp)
{
  return bRC_OK;
}; /* Need fname and statp */
bRC bareosSetSeenBitmap([[maybe_unused]] PluginContext* ctx,
                        [[maybe_unused]] bool all,
                        [[maybe_unused]] char* fname)
{
  return bRC_OK;
};
bRC bareosClearSeenBitmap([[maybe_unused]] PluginContext* ctx,
                          [[maybe_unused]] bool all,
                          [[maybe_unused]] char* fname)
{
  return bRC_OK;
};


/* Bareos entry points */
static filedaemon::CoreFunctions bareos_core_functions
    = {sizeof(filedaemon::CoreFunctions),
       FD_PLUGIN_INTERFACE_VERSION,
       bareosRegisterEvents,
       bareosUnRegisterEvents,
       bareosGetInstanceCount,
       bareosGetValue,
       bareosSetValue,
       bareosJobMsg,
       bareosDebugMsg,
       bareosMalloc,
       bareosFree,
       bareosAddExclude,
       bareosAddInclude,
       bareosAddOptions,
       bareosAddRegex,
       bareosAddWild,
       bareosNewOptions,
       bareosNewInclude,
       bareosNewPreInclude,
       bareosCheckChanges,
       bareosAcceptFile,
       bareosSetSeenBitmap,
       bareosClearSeenBitmap};


// create plugin context

Plugin plugin = {(char*)"python-fd-module-tester", 123, NULL, NULL, NULL, NULL};

static PluginContext bareos_PluginContext = {0, &plugin, NULL, NULL};

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[])
{
  /* Py_SetProgramName(argv[0]); */
  Py_Initialize();

  PyObject* bareosfdModule = PyImport_ImportModule("bareosfd");
  if (bareosfdModule) {
    printf("loaded bareosfd successfully\n");
  } else {
    printf("loading of bareosfd extension module failed\n");
  }
  if (PyErr_Occurred()) { PyErrorHandler(); }

  import_bareosfd();
  Bareosfd_set_bareos_core_functions(&bareos_core_functions);
  Bareosfd_set_plugin_context(&bareos_PluginContext);

  PyObject* pModule = PyImport_ImportModule("bareosfd-module-test");

  if (PyErr_Occurred()) { PyErrorHandler(); }
  PyObject* pDict = PyModule_GetDict(pModule);
  PyObject* pFunc = PyDict_GetItemString(pDict, "load_bareos_plugin");

  if (pFunc && PyCallable_Check(pFunc)) {
    printf("load_bareos_plugin found and is callable\n");
    PyObject *pPluginDefinition, *pRetVal;


    pPluginDefinition = PyUnicode_FromString((char*)"PluginDefinition");

    pRetVal = PyObject_CallFunctionObjArgs(pFunc, pPluginDefinition, NULL);

    Py_DECREF(pPluginDefinition);
    if (pRetVal) { Py_DECREF(pRetVal); }
    if (PyErr_Occurred()) { PyErrorHandler(); }
  } else {
    printf("load_bareos_plugin() not found in module");
  }
  Py_DECREF(pDict);
  Py_DECREF(pFunc);

  Py_DECREF(pModule);
  Py_Finalize();
  return 0;
}
