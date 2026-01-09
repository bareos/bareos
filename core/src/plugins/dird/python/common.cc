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
    result.error
        = (std::stringstream{} << "Illegal plugin definition '"
                               << plugin_definition << "' (missing ':')")
              .str();


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
      result.error = (std::stringstream{} << "Illegal argument '" << argument
                                          << "' without value")
                         .str();

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
