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

using namespace filedaemon;

int main(int argc, char* argv[])
{
  static filedaemon::bFuncs bfuncs;
  Py_SetProgramName(argv[0]);
  Py_Initialize();

  PyObject* bareosfdModule = PyImport_ImportModule("bareosfd");
  if (bareosfdModule) {
    printf("loading bareosfd successfully\n");
  } else {
    printf("loaded bareosfd failed\n");
  }
  if (PyErr_Occurred()) { PyErrorHandler(); }

  // Extract capsule pointer from bareosfd module
  void* ctx_from_bareosfd_module = PyCapsule_Import("bareosfd.bpContext", 0);

  if (ctx_from_bareosfd_module) {
    printf("module_main: imported capsule successfully\n");
    printf("module_main: ctx_from_bareosfd_module is at %p\n",
           ctx_from_bareosfd_module);

    printf("bfuncs are at: %p\n", &bfuncs);
    ctx_from_bareosfd_module = &bfuncs;
    printf("ctx_from_bareosfd_module contains %p\n", ctx_from_bareosfd_module);
  }

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
