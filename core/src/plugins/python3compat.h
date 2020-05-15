#ifndef BAREOS_CORE_SRC_PLUGINS_PYTHON3COMPAT_H_
#define BAREOS_CORE_SRC_PLUGINS_PYTHON3COMPAT_H_

/* redefine python3 calls to python2 pendants */

#if PY_VERSION_HEX < 0x03000000
#define PyLong_FromLong PyInt_FromLong
#define PyLong_AsLong PyInt_AsLong

#define PyBytes_FromString PyString_FromString
//#define PyUnicode_FromString PyString_FromString

#define PyUnicode_AsUTF8 PyString_AsString
//#define PyUnicode_Check PyString_Check
#endif

#endif  // BAREOS_CORE_SRC_PLUGINS_PYTHON3COMPAT_H_
