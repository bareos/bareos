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

#ifndef BAREOS_PLUGINS_DIRD_PYTHON_COMMON_MODULE_H_
#define BAREOS_PLUGINS_DIRD_PYTHON_COMMON_MODULE_H_

#if defined(HAVE_WIN32)
#  include "include/bareos.h"
#  include <Python.h>
#else
#  include <Python.h>
#  include "include/bareos.h"
#endif

#include <span>

PyObject* JMsg_Dict();
PyObject* bRC_Dict();

struct named_dict {
  const char* name;
  PyObject* obj;
};

bool include_dicts(PyObject* module, std::span<named_dict> dicts);

static inline PyObject* make_python_object(long data)
{
  return PyLong_FromLong(data);
}
// static inline PyObject* make_python_object(const char* data) { return
// PyUnicode_FromUtf8(data); }

static inline bool dict_insert_steal(PyObject* dict,
                                     const char* key,
                                     PyObject* value)
{
  if (!value) { return false; }
  bool ok = true;
  if (PyDict_SetItemString(dict, key, value) < 0) { ok = false; }
  Py_XDECREF(value);
  return ok;
}

#endif  // BAREOS_PLUGINS_DIRD_PYTHON_COMMON_MODULE_H_
