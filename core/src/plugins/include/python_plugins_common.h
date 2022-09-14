/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2020-2022 Bareos GmbH & Co. KG

   This program is Free Software; you can modify it under the terms of
   version three of the GNU Affero General Public License as published by the
   Free Software Foundation, which is listed in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   Affero General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
*/

#ifndef BAREOS_PLUGINS_INCLUDE_PYTHON_PLUGINS_COMMON_H_
#define BAREOS_PLUGINS_INCLUDE_PYTHON_PLUGINS_COMMON_H_

/* Common definitions used in all python plugins.  */

/* macros for uniform python module definition
   see http://python3porting.com/cextensions.html */
#if PY_MAJOR_VERSION >= 3
#  define MOD_ERROR_VAL NULL
#  define MOD_SUCCESS_VAL(val) val
#  define MOD_INIT(name) PyMODINIT_FUNC PyInit_##name(void)
#  define MOD_DEF(ob, name, doc, methods)                         \
    static struct PyModuleDef moduledef = {PyModuleDef_HEAD_INIT, \
                                           name,                  \
                                           doc,                   \
                                           -1,                    \
                                           methods,               \
                                           NULL,                  \
                                           NULL,                  \
                                           NULL,                  \
                                           NULL};                 \
                                                                  \
    ob = PyModule_Create(&moduledef);
#else
#  define MOD_ERROR_VAL
#  define MOD_SUCCESS_VAL(val)
#  define MOD_INIT(name) PyMODINIT_FUNC init##name(void)
#  define MOD_DEF(ob, name, doc, methods) \
    ob = Py_InitModule3(name, methods, doc);
#endif

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)
#define AT __FILE__ ":" TOSTRING(__LINE__)

/* check if plugin_ctx and bfunc are set.
 * Otherwise return NULL and throw RuntimeError */

#define RETURN_RUNTIME_ERROR_IF_BAREOS_PLUGIN_CTX_UNSET()            \
  if (!plugin_ctx) {                                                 \
    PyErr_SetString(PyExc_RuntimeError, AT ": plugin_ctx is unset"); \
    return NULL;                                                     \
  }

#define RETURN_RUNTIME_ERROR_IF_BFUNC_UNSET()               \
  if (!bareos_core_functions) {                             \
    PyErr_SetString(PyExc_RuntimeError,                     \
                    AT ": bareos_core_functions is unset"); \
    return NULL;                                            \
  }

#define RETURN_RUNTIME_ERROR_IF_BFUNC_OR_BAREOS_PLUGIN_CTX_UNSET() \
  RETURN_RUNTIME_ERROR_IF_BAREOS_PLUGIN_CTX_UNSET()                \
  RETURN_RUNTIME_ERROR_IF_BFUNC_UNSET()

/* define the given string constant as member of an dict and additionally
 add it directly as constant to the module.

 This is both done for string to long and for string to string mappings.

 The result is that the module both contains a dict:
 bIOPS = {'IO_CLOSE': 4L, 'IO_OPEN': 1L, 'IO_READ': 2L, 'IO_SEEK': 5L,
          'IO_WRITE': 3L}
 and the single values directly:
     IO_CLOSE = 4
     IO_OPEN = 1
     IO_READ = 2
     IO_SEEK = 5
     IO_WRITE = 3
 */

#define ConstSet_StrLong(dict, string, value)                                 \
  {                                                                           \
    PyObject* stringKey = PyBytes_FromString(#string);                        \
    PyObject* longValue = PyLong_FromLong(value);                             \
    if (PyDict_SetItem(dict, stringKey, longValue)) { return MOD_ERROR_VAL; } \
    if (PyModule_AddIntConstant(m, #string, value)) { return MOD_ERROR_VAL; } \
    Py_DECREF(stringKey);                                                     \
    Py_DECREF(longValue);                                                     \
  }
#define ConstSet_StrStr(dict, string, value)             \
  {                                                      \
    PyObject* stringKey = PyBytes_FromString(#string);   \
    PyObject* stringValue = PyBytes_FromString(value);   \
    if (PyDict_SetItem(dict, stringKey, stringValue)) {  \
      return MOD_ERROR_VAL;                              \
    }                                                    \
    if (PyModule_AddStringConstant(m, #string, value)) { \
      return MOD_ERROR_VAL;                              \
    }                                                    \
    Py_DECREF(stringKey);                                \
    Py_DECREF(stringValue);                              \
  }


/* commonly used definitions in multiple python modules */
#define DEFINE_bRCs_DICT()                    \
  const char* bRCs = "bRCs";                  \
  PyObject* pDictbRCs = NULL;                 \
  pDictbRCs = PyDict_New();                   \
  ConstSet_StrLong(pDictbRCs, bRC_OK, 0);     \
  ConstSet_StrLong(pDictbRCs, bRC_Stop, 1);   \
  ConstSet_StrLong(pDictbRCs, bRC_Error, 2);  \
  ConstSet_StrLong(pDictbRCs, bRC_More, 3);   \
  ConstSet_StrLong(pDictbRCs, bRC_Term, 4);   \
  ConstSet_StrLong(pDictbRCs, bRC_Seen, 5);   \
  ConstSet_StrLong(pDictbRCs, bRC_Core, 6);   \
  ConstSet_StrLong(pDictbRCs, bRC_Skip, 7);   \
  ConstSet_StrLong(pDictbRCs, bRC_Cancel, 8); \
  if (!pDictbRCs) { return MOD_ERROR_VAL; }   \
  if (PyModule_AddObject(m, bRCs, pDictbRCs)) { return MOD_ERROR_VAL; }

#define DEFINE_bJobMessageTypes_DICT()                               \
  const char* bJobMessageType = "bJobMessageType";                   \
  PyObject* pDictJobMessageType = NULL;                              \
  pDictJobMessageType = PyDict_New();                                \
  if (!pDictJobMessageType) { return MOD_ERROR_VAL; }                \
  ConstSet_StrLong(pDictJobMessageType, M_ABORT, 1);                 \
  ConstSet_StrLong(pDictJobMessageType, M_DEBUG, 2);                 \
  ConstSet_StrLong(pDictJobMessageType, M_FATAL, 3);                 \
  ConstSet_StrLong(pDictJobMessageType, M_ERROR, 4);                 \
  ConstSet_StrLong(pDictJobMessageType, M_WARNING, 5);               \
  ConstSet_StrLong(pDictJobMessageType, M_INFO, 6);                  \
  ConstSet_StrLong(pDictJobMessageType, M_SAVED, 7);                 \
  ConstSet_StrLong(pDictJobMessageType, M_NOTSAVED, 8);              \
  ConstSet_StrLong(pDictJobMessageType, M_SKIPPED, 9);               \
  ConstSet_StrLong(pDictJobMessageType, M_MOUNT, 10);                \
  ConstSet_StrLong(pDictJobMessageType, M_ERROR_TERM, 11);           \
  ConstSet_StrLong(pDictJobMessageType, M_TERM, 12);                 \
  ConstSet_StrLong(pDictJobMessageType, M_RESTORED, 13);             \
  ConstSet_StrLong(pDictJobMessageType, M_SECURITY, 14);             \
  ConstSet_StrLong(pDictJobMessageType, M_ALERT, 15);                \
  ConstSet_StrLong(pDictJobMessageType, M_VOLMGMT, 16);              \
  if (PyModule_AddObject(m, bJobMessageType, pDictJobMessageType)) { \
    return MOD_ERROR_VAL;                                            \
  }

#endif  // BAREOS_PLUGINS_INCLUDE_PYTHON_PLUGINS_COMMON_H_
