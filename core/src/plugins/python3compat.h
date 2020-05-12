#ifndef BAREOS_CORE_SRC_PLUGINS_PYTHON3COMPAT_H_
#define BAREOS_CORE_SRC_PLUGINS_PYTHON3COMPAT_H_

/* redefine python2 calls to python3 pendants */

#if PY_VERSION_HEX >= 0x03000000
#define PyInt_FromLong PyLong_FromLong
#define PyInt_AsLong PyLong_AsLong
#define PyString_FromString PyBytes_FromString
#define PyString_AsString PyUnicode_AsUTF8
#define PyString_Check PyUnicode_Check
#endif

#endif  // BAREOS_CORE_SRC_PLUGINS_PYTHON3COMPAT_H_
