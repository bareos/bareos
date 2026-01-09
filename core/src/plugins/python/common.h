/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2026-2026 Bareos GmbH & Co. KG

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   Affero General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
*/

#ifndef BAREOS_PLUGINS_PYTHON_COMMON_H_
#define BAREOS_PLUGINS_PYTHON_COMMON_H_

#if defined(HAVE_WIN32)
#  include "include/bareos.h"
#  include <Python.h>
#else
#  include <Python.h>
#  include "include/bareos.h"
#endif
#include "lib/plugins.h"

#include <string_view>
#include <string>
#include <variant>
#include <cstdint>
#include <span>

#include <mutex>
#include <condition_variable>
#include <future>

struct python_thread_ctx {
 public:
  void start(PyInterpreterState* main_interp, std::promise<bool>* ready);

  void stop();

  template <typename F> void run(F&& fun)
  {
    using data = std::pair<F*, std::promise<void>>;
    data d;
    d.first = &fun;

    auto executed = d.second.get_future();

    submit({+[](void* ptr) {
              data* data_ptr = static_cast<data*>(ptr);

              (*data_ptr->first)();
              data_ptr->second.set_value();
            },
            &d});

    executed.wait();
  }

 private:
  using execution_request = std::pair<void (*)(void*), void*>;

  friend class python_thread_ctx_helper;

  execution_request get_next_request();

  void submit(execution_request req);

  std::mutex execute_mut{};
  std::condition_variable request_empty{};
  std::condition_variable execution_requested{};
  std::optional<execution_request> to_execute{};

  std::thread python_thread;
};

struct plugin_argument {
  const char* name;
  std::variant<std::int64_t*, char**> destination;
};

struct option_parser {
 public:
  static option_parser parse(std::string_view options,
                             std::span<plugin_argument> arguments) noexcept;

  const std::string& unparsed_options() const noexcept { return unparsed; }

  bool ok() const noexcept { return error.empty(); }

  const std::string& error_string() const noexcept { return error; }


 private:
  option_parser() = default;

  std::string unparsed{};
  std::string error{};
};

std::string GetStringFromPyErrorHandler();

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

#endif  // BAREOS_PLUGINS_PYTHON_COMMON_H_
