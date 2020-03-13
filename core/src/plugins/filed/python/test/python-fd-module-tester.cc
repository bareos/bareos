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
bRC bareosRegisterEvents(bpContext* ctx, int nr_events, ...) { return bRC_OK; };
bRC bareosUnRegisterEvents(bpContext* ctx, int nr_events, ...)
{
  return bRC_OK;
};
bRC bareosGetInstanceCount(bpContext* ctx, int* ret) { return bRC_OK; };
bRC bareosGetValue(bpContext* ctx, filedaemon::bVariable var, void* value)
{
  return bRC_OK;
};
bRC bareosSetValue(bpContext* ctx, filedaemon::bVariable var, void* value)
{
  return bRC_OK;
};
bRC bareosJobMsg(bpContext* ctx,
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
bRC bareosDebugMsg(bpContext* ctx,
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
void* bareosMalloc(bpContext* ctx, const char* file, int line, size_t size)
{
  return NULL;
};
void bareosFree(bpContext* ctx, const char* file, int line, void* mem)
{
  return;
};
bRC bareosAddExclude(bpContext* ctx, const char* file) { return bRC_OK; };
bRC bareosAddInclude(bpContext* ctx, const char* file) { return bRC_OK; };
bRC bareosAddOptions(bpContext* ctx, const char* opts) { return bRC_OK; };
bRC bareosAddRegex(bpContext* ctx, const char* item, int type)
{
  return bRC_OK;
};
bRC bareosAddWild(bpContext* ctx, const char* item, int type)
{
  return bRC_OK;
};
bRC bareosNewOptions(bpContext* ctx) { return bRC_OK; };
bRC bareosNewInclude(bpContext* ctx) { return bRC_OK; };
bRC bareosNewPreInclude(bpContext* ctx) { return bRC_OK; };
bRC bareosCheckChanges(bpContext* ctx, struct filedaemon::save_pkt* sp)
{
  return bRC_OK;
};
bRC bareosAcceptFile(bpContext* ctx, struct filedaemon::save_pkt* sp)
{
  return bRC_OK;
}; /* Need fname and statp */
bRC bareosSetSeenBitmap(bpContext* ctx, bool all, char* fname)
{
  return bRC_OK;
};
bRC bareosClearSeenBitmap(bpContext* ctx, bool all, char* fname)
{
  return bRC_OK;
};


/* Bareos entry points */
static filedaemon::bFuncs bfuncs = {sizeof(filedaemon::bFuncs),
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

static void* bareos_plugin_context = NULL;

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

  /* printf("bfuncs is at          %p\n", &bfuncs); */
  /* printf("bareos_plugin_context %p\n", &bareos_plugin_context); */

  // Extract capsules pointer from bareosfd module
  void* ctx_from_bareosfd_module = PyCapsule_Import("bareosfd.bpContext", 0);
  if (!ctx_from_bareosfd_module) {
    printf("importing bareosfd.bpContext failed \n");
  }

  // Extract capsules pointer from bareosfd module
  void* bfuncs_from_bareosfd_module = PyCapsule_Import("bareosfd.bFuncs", 0);
  if (!bfuncs_from_bareosfd_module) {
    printf("importing bareosfd.bFuncs failed \n");
  }

  // Extract capsules pointer from bareosfd module
  void (*loadplugin_from_bareosfd_module)(
      filedaemon::bInfo * lbinfo, filedaemon::bFuncs * lbfuncs,
      genpInfo * *pinfo, filedaemon::pFuncs * *pfuncs) =
      (void (*)(filedaemon::bInfo*, filedaemon::bFuncs*, genpInfo**,
                filedaemon::pFuncs**))PyCapsule_Import("bareosfd.loadPlugin",
                                                       0);

  if (!loadplugin_from_bareosfd_module) {
    printf("importing bareosfd.loadPlugin failed \n");
  }

  /* printf("ctx_from_bareosfd_module is at       %p\n",
   * ctx_from_bareosfd_module); */
  /* printf("bfuncs_from_bareosfd_module is at    %p\n", */
  /*        bfuncs_from_bareosfd_module); */
  /* printf("loadplugin_from_bareosfd_module is @ %p\n", */
  /*        loadplugin_from_bareosfd_module); */

  /* printf("ctx_from_bareosfd_module contains    %p\n", */
  /*        *(void**)ctx_from_bareosfd_module); */
  /* printf("bfuncs_from_bareosfd_module contains %p\n", */
  /*        *(void**)bfuncs_from_bareosfd_module); */

  *(void**)ctx_from_bareosfd_module = &bareos_plugin_context;
  *(void**)bfuncs_from_bareosfd_module = &bfuncs;

  /* call loadPlugin in plugin */
  filedaemon::bInfo myInfo;
  genpInfo pinfo;
  filedaemon::pFuncs pfuncs;

  loadplugin_from_bareosfd_module(&myInfo, &bfuncs, (genpInfo**)&pinfo,
                                  (filedaemon::pFuncs**)&pfuncs);


  printf("ctx_from_bareosfd_module contains    %p\n",
         *(void**)ctx_from_bareosfd_module);
  printf("bfuncs_from_bareosfd_module contains %p\n",
         *(void**)bfuncs_from_bareosfd_module);


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
