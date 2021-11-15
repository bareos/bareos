/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2020-2021 Bareos GmbH & Co. KG

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

#ifndef BAREOS_PLUGINS_INCLUDE_PYTHON3COMPAT_H_
#define BAREOS_PLUGINS_INCLUDE_PYTHON3COMPAT_H_

/* redefine python3 calls to python2 pendants */

#include "include/version_hex.h"

#if PY_VERSION_HEX < VERSION_HEX(3, 0, 0)
#  define PyLong_FromLong PyInt_FromLong
#  define PyLong_AsLong PyInt_AsLong

#  define PyBytes_FromString PyString_FromString

#  undef PyUnicode_FromString
#  define PyUnicode_FromString PyString_FromString

#  define PyUnicode_AsUTF8 PyString_AsString

#  undef PyUnicode_Check
#  define PyUnicode_Check PyString_Check

#endif


#endif  // BAREOS_PLUGINS_INCLUDE_PYTHON3COMPAT_H_
