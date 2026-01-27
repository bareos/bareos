/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2020-2026 Bareos GmbH & Co. KG

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


// Strip any backslashes in the string.
static inline void StripBackSlashes(char* value)
{
  char* in = value;

  while (*in != '\0' && *in != '\\') {
    in += 1;
  }

  char* out = in;

  while (*in) {
    switch (*in) {
      case '\\': {
        in += 1;
      } break;
      default: {
        *out = *in;
        in += 1;
        out += 1;
      } break;
    }
  }
}

/**
 * Parse a boolean value e.g. check if its yes or true anything else translates
 * to false.
 */
static inline bool ParseBoolean(const char* argument_value)
{
  if (Bstrcasecmp(argument_value, "yes")
      || Bstrcasecmp(argument_value, "true")) {
    return true;
  } else {
    return false;
  }
}

// Parse a integer value.
static inline int64_t parse_integer(const char* argument_value)
{
  return str_to_int64(argument_value);
}

// this is needed because windows does not come with strndup ...
static char* CopyStringView(std::string_view value)
{
        char* x = reinterpret_cast<char*>(malloc(value.size() + 1));

        x[value.size()] = 0;
        memcpy(x, value.data(), value.size());
        return x;
}

// Always set destination to value and clean any previous one.
static inline void SetString(char** destination, std::string_view value)
{
  if (*destination) { free(*destination); }

  *destination = CopyStringView(value);
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
static std::string GetStringFromPyErrorHandler()
{
  PyObject *type, *value, *traceback;
  std::string error_string;

  PyErr_Fetch(&type, &value, &traceback);
  if (!type && !value && !traceback) {
    return "No python error could be fetched.";
  }
  PyErr_NormalizeException(&type, &value, &traceback);

  PyObject* tracebackModule = PyImport_ImportModule("traceback");
  if (tracebackModule) {
    PyObject *tbList = nullptr, *emptyString = nullptr, *strRetval = nullptr;
    if ((tbList = PyObject_CallMethod(tracebackModule, (char*)"format_exception",
                                      (char*)"OOO", type,
                                      value == NULL ? Py_None : value,
                                      traceback == NULL ? Py_None : traceback))) {
      if ((emptyString = PyUnicode_FromString(""))) {
        if ((strRetval = PyObject_CallMethod(emptyString, (char*)"join", (char*)"O", tbList))) {
          if (const char* err = PyUnicode_AsUTF8(strRetval)) {
            error_string = err;
          }
          else {
            error_string = "Unspecified error, retrieving error message failed.";
          }
        }
        else {
          error_string = "Calling method 'join' failed.";
        }
      }
      else {
        error_string = "Unable to retrieve empty-string.";
      }
    }
    else {
      error_string = "Calling method 'format_exception' failed.";
    }
    Py_XDECREF(tbList);
    Py_XDECREF(emptyString);
    Py_XDECREF(strRetval);
    Py_XDECREF(tracebackModule);
  } else {
    error_string = "Unable to import traceback module.";
  }

  Py_XDECREF(type);
  Py_XDECREF(value);
  Py_XDECREF(traceback);

  return error_string;
}


void PyErrorHandler(PluginContext* bareos_plugin_ctx, int msgtype)
{
  std::string error_string = GetStringFromPyErrorHandler();

  Dmsg(bareos_plugin_ctx, debuglevel, PYTHON_MODULE_NAME_QUOTED ": %s\n",
       error_string.c_str());
  if (msgtype) {
    Jmsg(bareos_plugin_ctx, msgtype, PYTHON_MODULE_NAME_QUOTED ": %s\n",
         error_string.c_str());
  }
}

// Convert a return value into a bRC enum value.
static inline bRC ConvertPythonRetvalTobRCRetval(PyObject* pRetVal)
{
  return (bRC)PyLong_AsLong(pRetVal);
}

// Convert a return value from bRC enum value into Python Object.
static inline PyObject* ConvertbRCRetvalToPythonRetval(bRC retval)
{
  return (PyObject*)PyLong_FromLong((int)retval);
}

// vim: set ft=c
