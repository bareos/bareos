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
#define RETURN_RUNTIME_ERROR_IF_BFUNC_OR_BAREOS_PLUGIN_CTX_UNSET()     \
  if (!bareos_plugin_ctx) {                                            \
    PyErr_SetString(PyExc_RuntimeError, "bareos_plugin_ctx is unset"); \
    return NULL;                                                       \
  }                                                                    \
  if (!bfuncs) {                                                       \
    PyErr_SetString(PyExc_RuntimeError, "bfuncs is unset");            \
    return NULL;                                                       \
  }

#define DictSet_StrLong(dict, string, value)            \
  if (PyDict_SetItem(dict, PyBytes_FromString(#string), \
                     PyLong_FromLong(value))) {         \
    return MOD_ERROR_VAL;                               \
  }

#define DictSet_StrStr(dict, string, value)             \
  if (PyDict_SetItem(dict, PyBytes_FromString(#string), \
                     PyBytes_FromString(value))) {      \
    return MOD_ERROR_VAL;                               \
  }

#define DEFINE_bRCs_DICT()                   \
  const char* bRCs = "bRCs";                 \
  PyObject* pDictbRCs = NULL;                \
  pDictbRCs = PyDict_New();                  \
  DictSet_StrLong(pDictbRCs, bRC_OK, 0);     \
  DictSet_StrLong(pDictbRCs, bRC_Stop, 1);   \
  DictSet_StrLong(pDictbRCs, bRC_Error, 2);  \
  DictSet_StrLong(pDictbRCs, bRC_More, 3);   \
  DictSet_StrLong(pDictbRCs, bRC_Term, 4);   \
  DictSet_StrLong(pDictbRCs, bRC_Seen, 5);   \
  DictSet_StrLong(pDictbRCs, bRC_Core, 6);   \
  DictSet_StrLong(pDictbRCs, bRC_Skip, 7);   \
  DictSet_StrLong(pDictbRCs, bRC_Cancel, 8); \
  if (!pDictbRCs) { return MOD_ERROR_VAL; }  \
  if (PyModule_AddObject(m, bRCs, pDictbRCs)) { return MOD_ERROR_VAL; }

#define DEFINE_bJobMessageTypes_DICT()                               \
  const char* bJobMessageType = "bJobMessageType";                   \
  PyObject* pDictJobMessageType = NULL;                              \
  pDictJobMessageType = PyDict_New();                                \
  if (!pDictJobMessageType) { return MOD_ERROR_VAL; }                \
  DictSet_StrLong(pDictJobMessageType, M_ABORT, 1);                  \
  DictSet_StrLong(pDictJobMessageType, M_DEBUG, 2);                  \
  DictSet_StrLong(pDictJobMessageType, M_FATAL, 3);                  \
  DictSet_StrLong(pDictJobMessageType, M_ERROR, 4);                  \
  DictSet_StrLong(pDictJobMessageType, M_WARNING, 5);                \
  DictSet_StrLong(pDictJobMessageType, M_INFO, 6);                   \
  DictSet_StrLong(pDictJobMessageType, M_SAVED, 7);                  \
  DictSet_StrLong(pDictJobMessageType, M_NOTSAVED, 8);               \
  DictSet_StrLong(pDictJobMessageType, M_SKIPPED, 9);                \
  DictSet_StrLong(pDictJobMessageType, M_MOUNT, 10);                 \
  DictSet_StrLong(pDictJobMessageType, M_ERROR_TERM, 11);            \
  DictSet_StrLong(pDictJobMessageType, M_TERM, 12);                  \
  DictSet_StrLong(pDictJobMessageType, M_RESTORED, 13);              \
  DictSet_StrLong(pDictJobMessageType, M_SECURITY, 14);              \
  DictSet_StrLong(pDictJobMessageType, M_ALERT, 15);                 \
  DictSet_StrLong(pDictJobMessageType, M_VOLMGMT, 16);               \
  if (PyModule_AddObject(m, bJobMessageType, pDictJobMessageType)) { \
    return MOD_ERROR_VAL;                                            \
  }
#endif  // BAREOS_PYTHON_PLUGINS_COMMON_H_
