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

/* Common functions used in all python plugins.  */
static bRC getPluginValue(bpContext* bareos_plugin_ctx,
                          pVariable var,
                          void* value)
{
  struct plugin_private_context* plugin_priv_ctx =
      (struct plugin_private_context*)bareos_plugin_ctx->pContext;
  bRC retval = bRC_Error;

  if (!plugin_priv_ctx) { goto bail_out; }

  PyEval_AcquireThread(plugin_priv_ctx->interpreter);
  retval = PyGetPluginValue(bareos_plugin_ctx, var, value);
  PyEval_ReleaseThread(plugin_priv_ctx->interpreter);

bail_out:
  return retval;
}

static bRC setPluginValue(bpContext* bareos_plugin_ctx,
                          pVariable var,
                          void* value)
{
  struct plugin_private_context* plugin_priv_ctx =
      (struct plugin_private_context*)bareos_plugin_ctx->pContext;
  bRC retval = bRC_Error;

  if (!plugin_priv_ctx) { return bRC_Error; }

  PyEval_AcquireThread(plugin_priv_ctx->interpreter);
  retval = PySetPluginValue(bareos_plugin_ctx, var, value);
  PyEval_ReleaseThread(plugin_priv_ctx->interpreter);

  return retval;
}


/**
 * Strip any backslashes in the string.
 */
static inline void StripBackSlashes(char* value)
{
  char* bp;

  bp = value;
  while (*bp) {
    switch (*bp) {
      case '\\':
        bstrinlinecpy(bp, bp + 1);
        break;
      default:
        break;
    }

    bp++;
  }
}

/**
 * Parse a boolean value e.g. check if its yes or true anything else translates
 * to false.
 */
static inline bool ParseBoolean(const char* argument_value)
{
  if (Bstrcasecmp(argument_value, "yes") ||
      Bstrcasecmp(argument_value, "true")) {
    return true;
  } else {
    return false;
  }
}

/**
 * Parse a integer value.
 */
static inline int64_t parse_integer(const char* argument_value)
{
  return str_to_int64(argument_value);
}

/**
 * Always set destination to value and clean any previous one.
 */
static inline void SetString(char** destination, char* value)
{
  if (*destination) { free(*destination); }

  *destination = strdup(value);
  StripBackSlashes(*destination);
}


/**
 * Handle a Python error.
 *
 * Python equivalent:
 *
 * import traceback, sys
 * return "".join(traceback.format_exception(sys.exc_type,
 *    sys.exc_value, sys.exc_traceback))
 */
static void PyErrorHandler(bpContext* bareos_plugin_ctx, int msgtype)
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

  Dmsg(bareos_plugin_ctx, debuglevel, PYTHON_MODULE_NAME_QUOTED ": %s\n",
       error_string);
  if (msgtype) {
    Jmsg(bareos_plugin_ctx, msgtype, PYTHON_MODULE_NAME_QUOTED ": %s\n",
         error_string);
  }

  free(error_string);
}

static void StorePluginContextInPythonModule(bpContext* bareos_plugin_ctx)
{
  /* get the pointer to the module variable that is exported via the capsule */
  bpContext** bareosfd_bpContext =
      (bpContext**)PyCapsule_Import(PYTHON_MODULE_NAME_QUOTED ".bpContext", 0);

  /* store bareos_plugin_ctx in module */
  if (bareosfd_bpContext) {
    *bareosfd_bpContext = bareos_plugin_ctx;
  } else {
    printf(PYTHON_MODULE_NAME_QUOTED ": could not get bareosfd_bpContext from module\n");
  }
}

static bpContext* GetPluginContextFromPythonModule()
{
  const char* capsule_name =  PYTHON_MODULE_NAME_QUOTED ".bpContext";
  bpContext** retval =
      (bpContext**)PyCapsule_Import(capsule_name, 0);
  return *retval;
}

/**
 * Convert a return value into a bRC enum value.
 */
static inline bRC ConvertPythonRetvalTobRCRetval(PyObject* pRetVal)
{
  return (bRC)PyInt_AsLong(pRetVal);
}

/**
 * Convert a return value from bRC enum value into Python Object.
 */
static inline PyObject* ConvertbRCRetvalToPythonRetval(bRC retval)
{
  return (PyObject*)PyInt_FromLong((int)retval);
}
