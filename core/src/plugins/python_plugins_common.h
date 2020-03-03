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

/* Common definitions used in all python plugins.  */

#ifndef BAREOS_PYTHON_PLUGINS_COMMON_H_
#define BAREOS_PYTHON_PLUGINS_COMMON_H_

/* macros for uniform python module definition
   see http://python3porting.com/cextensions.html */
#if PY_MAJOR_VERSION >= 3
#define MOD_ERROR_VAL NULL
#define MOD_SUCCESS_VAL(val) val
#define MOD_INIT(name) PyMODINIT_FUNC PyInit_##name(void)
#define MOD_DEF(ob, name, doc, methods)              \
  static struct PyModuleDef moduledef = {            \
      PyModuleDef_HEAD_INIT, name, doc, -1, methods, \
  };                                                 \
                                                     \
  ob = PyModule_Create(&moduledef);
#else
#define MOD_ERROR_VAL
#define MOD_SUCCESS_VAL(val)
#define MOD_INIT(name) PyMODINIT_FUNC init##name(void)
#define MOD_DEF(ob, name, doc, methods) ob = Py_InitModule3(name, methods, doc);
#endif

/* check if bareos_plugin_ctx and bfunc are set.
 * Otherwise return NULL and throw RuntimeError */
#define RETURN_RUNTIME_ERROR_IF_BFUNC_OR_BAREOS_PLUGIN_CTX_UNSET()         \
  if (!bareos_plugin_ctx) {                                                \
    PyErr_SetString(PyExc_RuntimeError, "bareos_plugin_ctx is unset");     \
    return NULL;                                                           \
  }                                                                        \
  if (!bfuncs) { PyErr_SetString(PyExc_RuntimeError, "bfuncs is unset"); } \
  return NULL;

#endif  // BAREOS_PYTHON_PLUGINS_COMMON_H_
