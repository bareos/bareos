
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

  Dmsg(bareos_plugin_ctx, debuglevel, Quote(PYTHON_MODULE_NAME) ": %s\n",  error_string);
  if (msgtype) {
    Jmsg(bareos_plugin_ctx, msgtype, Quote(PYTHON_MODULE_NAME) ": %s\n", error_string);
  }

  free(error_string);
}
