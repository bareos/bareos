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

/* Load the python-fd plugin and test it */

#include "Python.h"
class PoolMem;
#define NbytesForBits(n) ((((n)-1) >> 3) + 1)
typedef off_t boffset_t;

#include "lib/plugins.h"
#include "filed/fd_plugins.h"
#include "plugins/filed/python/bareosfd.h"
static void PyErrorHandler()
{
  PyObject *type, *value, *traceback;
  PyObject* tracebackModule;
  char* error_string;

  PyErr_Fetch(&type, &value, &traceback);

  tracebackModule = PyImport_ImportModule("traceback");
  if (tracebackModule != NULL) {
    PyObject *tbList, *emptyString, *strRetval;

    tbList =
        PyObject_CallMethod(tracebackModule, (char*)"format_exception",
                            (char*)"OOO", type, value == NULL ? Py_None : value,
                            traceback == NULL ? Py_None : traceback);

    emptyString = PyString_FromString("");
    strRetval =
        PyObject_CallMethod(emptyString, (char*)"join", (char*)"O", tbList);

    error_string = strdup(PyString_AsString(strRetval));

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

// using namespace filedaemon;
bRC bareosRegisterEvents(PluginContext* ctx, int nr_events, ...)
{
  return bRC_OK;
};
bRC bareosUnRegisterEvents(PluginContext* ctx, int nr_events, ...)
{
  return bRC_OK;
};
bRC bareosGetInstanceCount(PluginContext* ctx, int* ret) { return bRC_OK; };
bRC bareosGetValue(PluginContext* ctx, filedaemon::bVariable var, void* value)
{
  return bRC_OK;
};
bRC bareosSetValue(PluginContext* ctx, filedaemon::bVariable var, void* value)
{
  return bRC_OK;
};
bRC bareosJobMsg(PluginContext* ctx,
                 const char* file,
                 int line,
                 int type,
                 utime_t mtime,
                 const char* fmt,
                 ...)
{
  printf("bareosJobMsg file:%s line:%d type:%d time: %ld, fmt:%s\n", file, line,
         type, (int64_t)mtime, fmt);
  return bRC_OK;
};
bRC bareosDebugMsg(PluginContext* ctx,
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
void* bareosMalloc(PluginContext* ctx, const char* file, int line, size_t size)
{
  return NULL;
};
void bareosFree(PluginContext* ctx, const char* file, int line, void* mem)
{
  return;
};
bRC bareosAddExclude(PluginContext* ctx, const char* file) { return bRC_OK; };
bRC bareosAddInclude(PluginContext* ctx, const char* file) { return bRC_OK; };
bRC bareosAddOptions(PluginContext* ctx, const char* opts) { return bRC_OK; };
bRC bareosAddRegex(PluginContext* ctx, const char* item, int type)
{
  return bRC_OK;
};
bRC bareosAddWild(PluginContext* ctx, const char* item, int type)
{
  return bRC_OK;
};
bRC bareosNewOptions(PluginContext* ctx) { return bRC_OK; };
bRC bareosNewInclude(PluginContext* ctx) { return bRC_OK; };
bRC bareosNewPreInclude(PluginContext* ctx) { return bRC_OK; };
bRC bareosCheckChanges(PluginContext* ctx, struct filedaemon::save_pkt* sp)
{
  return bRC_OK;
};
bRC bareosAcceptFile(PluginContext* ctx, struct filedaemon::save_pkt* sp)
{
  return bRC_OK;
}; /* Need fname and statp */
bRC bareosSetSeenBitmap(PluginContext* ctx, bool all, char* fname)
{
  return bRC_OK;
};
bRC bareosClearSeenBitmap(PluginContext* ctx, bool all, char* fname)
{
  return bRC_OK;
};


/* Bareos entry points */
static filedaemon::BareosCoreFunctions bareos_core_functions = {
    sizeof(filedaemon::BareosCoreFunctions),
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

static void* bareos_PluginContext = NULL;

int main(int argc, char* argv[])
{
  Py_SetProgramName(argv[0]);
  Py_Initialize();

  PyObject* bareosfdModule = PyImport_ImportModule("bareosfd");
  if (bareosfdModule) {
    printf("loaded bareosfd successfully\n");
  } else {
    printf("loading of bareosfd failed\n");
  }
  if (PyErr_Occurred()) { PyErrorHandler(); }


  /* static void** Bareosfd_API = (void**)PyCapsule_Import("bareosfd._C_API",
   * 0); */
  /* if (!Bareosfd_API) { */
  /*   printf("importing bareosfd._C_API failed \n"); */
  /* } else { */
  /*   printf("importing bareosfd._C_API successful \n"); */
  /* } */


  import_bareosfd();
  // bRC retval = Bareosfd_PyLoadModule((PluginContext*)nullptr, nullptr);

  /* printf("bareos_core_functions is at          %p\n",
   * &bareos_core_functions); */
  /* printf("bareos_PluginContext %p\n", &bareos_PluginContext); */

  // Extract capsules pointer from bareosfd module
  void* ctx_from_bareosfd_module =
      PyCapsule_Import("bareosfd.PluginContext", 0);
  if (!ctx_from_bareosfd_module) {
    printf("importing bareosfd.PluginContext failed \n");
  }

  // Extract capsules pointer from bareosfd module
  void* bareos_core_functions_from_bareosfd_module =
      PyCapsule_Import("bareosfd.BareosCoreFunctions", 0);
  if (!bareos_core_functions_from_bareosfd_module) {
    printf("importing bareosfd.BareosCoreFunctions failed \n");
  }

  // Extract capsules pointer from bareosfd module
  void (*loadplugin_from_bareosfd_module)(
      filedaemon::Core_PluginApiDefinition * lbareos_plugin_interface_version,
      filedaemon::BareosCoreFunctions * lbareos_core_functions,
      PluginInformation * *plugin_information,
      filedaemon::pFuncs * *plugin_functions) =
      (void (*)(filedaemon::Core_PluginApiDefinition*,
                filedaemon::BareosCoreFunctions*, PluginInformation**,
                filedaemon::pFuncs**))PyCapsule_Import("bareosfd.loadPlugin",
                                                       0);

  if (!loadplugin_from_bareosfd_module) {
    printf("importing bareosfd.loadPlugin failed \n");
  }

  /* printf("ctx_from_bareosfd_module is at       %p\n",
   * ctx_from_bareosfd_module); */
  /* printf("bareos_core_functions_from_bareosfd_module is at    %p\n", */
  /*        bareos_core_functions_from_bareosfd_module); */
  /* printf("loadplugin_from_bareosfd_module is @ %p\n", */
  /*        loadplugin_from_bareosfd_module); */

  /* printf("ctx_from_bareosfd_module contains    %p\n", */
  /*        *(void**)ctx_from_bareosfd_module); */
  /* printf("bareos_core_functions_from_bareosfd_module contains %p\n", */
  /*        *(void**)bareos_core_functions_from_bareosfd_module); */

  *(void**)ctx_from_bareosfd_module = &bareos_PluginContext;
  *(void**)bareos_core_functions_from_bareosfd_module = &bareos_core_functions;

  /* call loadPlugin in plugin */
  filedaemon::Core_PluginApiDefinition myInfo;
  PluginInformation plugin_information;
  filedaemon::pFuncs plugin_functions;

  loadplugin_from_bareosfd_module(&myInfo, &bareos_core_functions,
                                  (PluginInformation**)&plugin_information,
                                  (filedaemon::pFuncs**)&plugin_functions);


  printf("ctx_from_bareosfd_module contains    %p\n",
         *(void**)ctx_from_bareosfd_module);
  printf("bareos_core_functions_from_bareosfd_module contains %p\n",
         *(void**)bareos_core_functions_from_bareosfd_module);


  PyObject* pModule = PyImport_ImportModule("bareosfd-module-test");

  if (PyErr_Occurred()) { PyErrorHandler(); }

  PyObject* pDict = PyModule_GetDict(pModule);
  PyObject* pFunc = PyDict_GetItemString(pDict, "load_bareos_plugin");

  if (pFunc && PyCallable_Check(pFunc)) {
    printf("load_bareos_plugin found and is callable\n");
    PyObject *pPluginDefinition, *pRetVal;


    pPluginDefinition = PyString_FromString((char*)"PluginDefinition");

    pRetVal = PyObject_CallFunctionObjArgs(pFunc, pPluginDefinition, NULL);

    Py_DECREF(pPluginDefinition);
    if (pRetVal) { Py_DECREF(pRetVal); }
    if (PyErr_Occurred()) { PyErrorHandler(); }
  } else {
    printf("load_bareos_plugin() not found in module");
  }
  Py_DECREF(pDict);
  Py_DECREF(pFunc);
  Py_Finalize();
  return 0;
}
