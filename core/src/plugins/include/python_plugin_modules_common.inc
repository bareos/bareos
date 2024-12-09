/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2021-2024 Bareos GmbH & Co. KG

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

static void PyErrorHandler()
{
  std::string error_string = GetStringFromPyErrorHandler();
  fprintf(stderr, "Error: %s\n", error_string.c_str());

  exit(1);
}


/**
 * Initial load of the Python module.
 *
 * Based on the parsed plugin options we set some prerequisites like the
 * module path and the module to load. We also load the dictionary used
 * for looking up the Python methods.
 */
static bRC PyLoadModule(PluginContext* plugin_ctx, const void* value)
{
  bRC retval = bRC_Error;
  struct plugin_private_context* plugin_priv_ctx
      = (struct plugin_private_context*)plugin_ctx->plugin_private_context;
  PyObject *sysPath, *mPath, *pName, *pFunc;

  /* See if we already setup the python search path.  */
  if (!plugin_priv_ctx->python_default_path_is_set) {
    sysPath = PySys_GetObject((char*)"path");
    mPath = PyUnicode_FromString(PYTHON_MODULE_PATH);
    PyList_Insert(sysPath, 0, mPath);
    Py_DECREF(mPath);
    plugin_priv_ctx->python_default_path_is_set = true;
  }
  if (!plugin_priv_ctx->python_path_is_set) {
    /* Extend the Python search path with the given module_path.  */
    if (plugin_priv_ctx->module_path) {
      sysPath = PySys_GetObject((char*)"path");
      mPath = PyUnicode_FromString(plugin_priv_ctx->module_path);
      PyList_Insert(sysPath, 0, mPath);
      Py_DECREF(mPath);
      plugin_priv_ctx->python_path_is_set = true;
    }
  }

  /* Try to load the Python module by name. */
  if (plugin_priv_ctx->module_name) {
    Dmsg(plugin_ctx, debuglevel,
         LOGPREFIX "Trying to load module with name %s\n",
         plugin_priv_ctx->module_name);
    pName = PyUnicode_FromString(plugin_priv_ctx->module_name);
    plugin_priv_ctx->pModule = PyImport_Import(pName);
    Py_DECREF(pName);

    if (!plugin_priv_ctx->pModule) {
      Dmsg(plugin_ctx, debuglevel,
           LOGPREFIX "Failed to load module with name %s\n",
           plugin_priv_ctx->module_name);
      goto bail_out;
    }

    Dmsg(plugin_ctx, debuglevel,
         LOGPREFIX "Successfully loaded module with name %s\n",
         plugin_priv_ctx->module_name);

    /* Get the Python dictionary for lookups in the Python namespace.  */
    plugin_priv_ctx->pyModuleFunctionsDict
        = PyModule_GetDict(plugin_priv_ctx->pModule); /* Borrowed reference */


    /* Lookup the load_bareos_plugin() function in the python module.  */
    pFunc = PyDict_GetItemString(plugin_priv_ctx->pyModuleFunctionsDict,
                                 "load_bareos_plugin"); /* Borrowed reference */
    if (pFunc && PyCallable_Check(pFunc)) {
      PyObject *pPluginDefinition, *pRetVal;

      pPluginDefinition = PyUnicode_FromString((const char*)value);
      if (!pPluginDefinition) { goto bail_out; }

      pRetVal = PyObject_CallFunctionObjArgs(pFunc, pPluginDefinition, NULL);
      Py_DECREF(pPluginDefinition);

      if (!pRetVal) {
        goto bail_out;
      } else {
        retval = ConvertPythonRetvalTobRCRetval(pRetVal);
        Py_DECREF(pRetVal);
      }
    } else {
      Dmsg(plugin_ctx, debuglevel,
           LOGPREFIX "Failed to find function named load_bareos_plugin()\n");
      goto bail_out;
    }

    // Keep track we successfully loaded.
    plugin_priv_ctx->python_loaded = true;
  }

  return retval;

bail_out:
  if (PyErr_Occurred()) { PyErrorHandler(plugin_ctx, M_FATAL); }

  return retval;
}
