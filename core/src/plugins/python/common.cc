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

#include "common.h"
#include "common_module.h"
#include "lib/edit.h"

#include <sstream>
#include <algorithm>

namespace {
constexpr int debuglevel = 100;


// Strip any backslashes in the string.
inline void StripBackSlashes(char* value)
{
  char* in = value;

  while (*in != '\0' && *in != '\\') { in += 1; }

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
inline bool parse_into(bool* destination, const char* argument_value)
{
  if (Bstrcasecmp(argument_value, "yes")
      || Bstrcasecmp(argument_value, "true")) {
    *destination = true;
  } else {
    *destination = false;
  }

  return true;
}

// Parse a integer value.
inline bool parse_into(std::int64_t* destination, const char* argument_value)
{
  *destination = str_to_int64(argument_value);
  return true;
}

// this is needed because windows does not come with strndup ...
char* CopyStringView(std::string_view value)
{
  char* x = reinterpret_cast<char*>(malloc(value.size() + 1));

  x[value.size()] = 0;
  memcpy(x, value.data(), value.size());
  return x;
}

// Always set destination to value and clean any previous one.
inline void parse_into(char** destination, std::string_view value)
{
  if (*destination) { free(*destination); }

  *destination = CopyStringView(value);
  StripBackSlashes(*destination);
}
};  // namespace

/**
 * Handle a Python error.
 *
 * Python equivalent:
 *
 * import traceback, sys
 * return "".join(traceback.format_exception(sys.exc_type,
 *    sys.exc_value, sys.exc_traceback))
 */
std::string GetStringFromPyErrorHandler()
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
    if ((tbList = PyObject_CallMethod(
             tracebackModule, (char*)"format_exception", (char*)"OOO", type,
             value == NULL ? Py_None : value,
             traceback == NULL ? Py_None : traceback))) {
      if ((emptyString = PyUnicode_FromString(""))) {
        if ((strRetval = PyObject_CallMethod(emptyString, (char*)"join",
                                             (char*)"O", tbList))) {
          if (const char* err = PyUnicode_AsUTF8(strRetval)) {
            error_string = err;
          } else {
            error_string
                = "Unspecified error, retrieving error message failed.";
          }
        } else {
          error_string = "Calling method 'join' failed.";
        }
      } else {
        error_string = "Unable to retrieve empty-string.";
      }
    } else {
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

option_parser option_parser::parse(
    std::string_view options,
    std::span<plugin_argument> arguments) noexcept
{
  /* Parse the plugin definition.
   * Make a private copy of the whole string. */
  std::string plugin_definition{options};

  std::stringstream leftover{};

  option_parser result;

  char* bp = strchr(plugin_definition.data(), ':');

  if (!bp) {
    std::stringstream msg{};
    msg << "Illegal plugin definition '" << plugin_definition
        << "' (missing ':')";

    result.error = msg.str();


    return result;
  }

  // Skip the first ':'
  bp++;

  std::size_t leftover_cnt = 0;
  while (bp) {
    if (strlen(bp) == 0) { break; }

    /* Each argument is in the form:
     *    <argument> = <argument_value>
     *
     * So we setup the right pointers here, argument to the beginning
     * of the argument, argument_value to the beginning of the argument_value.
     */
    char* argument = bp;
    char* argument_value = strchr(bp, '=');
    if (!argument_value) {
      std::stringstream msg{};
      msg << "Illegal argument '" << argument << "' without value";
      result.error = msg.str();

      return result;
    }
    *argument_value++ = '\0';

    // See if there are more arguments and setup for the next run.
    bp = argument_value;
    do {
      bp = strchr(bp, ':');
      if (bp) {
        if (*(bp - 1) != '\\') {
          *bp++ = '\0';
          break;
        } else {
          bp++;
        }
      }
    } while (bp);

    if (auto found = std::find_if(arguments.begin(), arguments.end(),
                                  [&](auto& candidate) {
                                    return Bstrcasecmp(argument,
                                                       candidate.name);
                                  });
        found != arguments.end()) {
      std::visit([&](auto&& dest) { parse_into(dest, argument_value); },
                 found->destination);
    } else {
      PoolMem option(PM_FNAME);
      if (leftover_cnt) { leftover << ":"; }

      leftover << argument << "=" << argument_value;
      leftover_cnt++;
    }
  }

  if (leftover_cnt > 0) { leftover << ":"; }

  result.unparsed = leftover.str();
  return result;
}

class python_thread_ctx_helper {
 public:
  static inline constexpr python_thread_ctx::execution_request
      REQUEST_THREAD_STOP
      = {};

  static void wait_for_thread_end(python_thread_ctx* ctx)
  {
    for (;;) {
      auto req = ctx->get_next_request();

      if (req == REQUEST_THREAD_STOP) { break; }
    }
  }

  static void run_thread(python_thread_ctx* ctx,
                         PyInterpreterState* main_interp,
                         std::promise<bool>* ready)
  {
    /* For each plugin instance we instantiate a new Python interpreter. */

    // before creating the subinterpreter, we first need to acquire
    // the main interpreter gil.  To do this correctly, we need to
    // create a new threadstate for that interpreter
    auto* ts_maininterp = PyThreadState_New(main_interp);
    if (!ts_maininterp) {
      ready->set_value(false);
      return wait_for_thread_end(ctx);
    }
    PyEval_RestoreThread(ts_maininterp);

    bool ok = true;

    PyThreadState* ts = Py_NewInterpreter();
    if (!ts) {
      ready->set_value(false);
      PyThreadState_Clear(ts_maininterp);

      // delete the threadstate and release the gil
      PyThreadState_DeleteCurrent();
      return wait_for_thread_end(ctx);
    }

    auto delete_python = [&] {
      Py_EndInterpreter(ts);

      // now we also need to cleanup the maininterp threadstate
      PyThreadState_Swap(ts_maininterp);
      PyThreadState_Clear(ts_maininterp);

      // delete the threadstate and release the gil
      PyThreadState_DeleteCurrent();
    };


    ready->set_value(ok);

    // release the (shared) gil, so that other plugins can also continue
    PyEval_SaveThread();

    for (;;) {
      auto req = ctx->get_next_request();

      if (req == REQUEST_THREAD_STOP) { break; }

      if (ok) {
        ASSERT(req.first != nullptr);

        PyEval_RestoreThread(ts);
        (*req.first)(req.second);
        PyEval_SaveThread();
      }
    }

    PyEval_RestoreThread(ts);
    delete_python();
  }
};

// wait until there is nothing currently executing
// returns with execution lock
void python_thread_ctx::submit(python_thread_ctx::execution_request req)
{
  std::unique_lock lock{execute_mut};
  request_empty.wait(
      lock, [&to_execute = to_execute] { return !to_execute.has_value(); });

  ASSERT(!to_execute.has_value());
  to_execute.emplace(req);
  execution_requested.notify_one();
}


void python_thread_ctx::stop()
{
  submit(python_thread_ctx_helper::REQUEST_THREAD_STOP);
  python_thread.join();
}

auto python_thread_ctx::get_next_request() -> execution_request
{
  std::unique_lock lock{execute_mut};
  execution_requested.wait(lock, [&] { return to_execute.has_value(); });

  ASSERT(to_execute.has_value());

  execution_request req = to_execute.value();
  to_execute.reset();

  request_empty.notify_one();

  return req;
}


void python_thread_ctx::start(PyInterpreterState* main_interp,
                              std::promise<bool>* ready)
{
  python_thread = std::thread{python_thread_ctx_helper::run_thread, this,
                              main_interp, ready};
}

#define set_enum_value(dict, eval) \
  dict_insert_steal((dict), #eval, make_python_object(eval))

PyObject* JMsg_Dict()
{
  PyObject* dict = PyDict_New();
  if (!dict) { return nullptr; }

  set_enum_value(dict, M_ABORT);
  set_enum_value(dict, M_DEBUG);
  set_enum_value(dict, M_FATAL);
  set_enum_value(dict, M_ERROR);
  set_enum_value(dict, M_WARNING);
  set_enum_value(dict, M_INFO);
  set_enum_value(dict, M_SAVED);
  set_enum_value(dict, M_NOTSAVED);
  set_enum_value(dict, M_SKIPPED);
  set_enum_value(dict, M_MOUNT);
  set_enum_value(dict, M_ERROR_TERM);
  set_enum_value(dict, M_TERM);
  set_enum_value(dict, M_RESTORED);
  set_enum_value(dict, M_SECURITY);
  set_enum_value(dict, M_ALERT);
  set_enum_value(dict, M_VOLMGMT);

  return dict;
}

PyObject* bRC_Dict()
{
  PyObject* dict = PyDict_New();
  if (!dict) { return nullptr; }

  set_enum_value(dict, bRC_OK);
  set_enum_value(dict, bRC_Stop);
  set_enum_value(dict, bRC_Error);
  set_enum_value(dict, bRC_More);
  set_enum_value(dict, bRC_Term);
  set_enum_value(dict, bRC_Seen);
  set_enum_value(dict, bRC_Core);
  set_enum_value(dict, bRC_Skip);
  set_enum_value(dict, bRC_Cancel);

  return dict;
}

#undef set_enum_value

static bool include_dict(PyObject* module,
                         const char* dict_name,
                         PyObject* dict)
{
  PyObject* module_dict = PyModule_GetDict(module);
  if (!module_dict) { return false; }

  if (PyDict_Merge(module_dict, dict, 1) < 0) {
    Py_DECREF(dict);
    return false;
  }
  if (PyModule_AddObject(module, dict_name, dict) < 0) {
    Py_DECREF(dict);
    return false;
  }

  return true;
}

bool include_dicts(PyObject* module, std::span<named_dict> dicts)
{
  bool error = false;
  for (auto dict : dicts) {
    if (!dict.obj) {
      error = true;
      continue;
    }

    if (!include_dict(module, dict.name, dict.obj)) { error = true; }
  }

  return !error;
}
