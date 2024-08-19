/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2024-2024 Bareos GmbH & Co. KG

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
/*
 * Kern Sibbald, Nov MM
 *
 */
/** @file
 * Bareos JobControlRecord Structure definition for Daemons and the Library
 *
 *  This definition consists of a "Global" definition common
 *  to all daemons and used by the library routines, and a
 *  daemon specific part that is enabled with #defines.
 */
#ifndef BAREOS_INCLUDE_COMPILER_MACRO_H_
#define BAREOS_INCLUDE_COMPILER_MACRO_H_

#if defined(HAVE_GCC)
#  define IGNORE_MISSING_INITIALIZERS_ON \
    _Pragma("GCC diagnostic push");      \
    _Pragma("GCC diagnostic ignored \"-Wmissing-field-initializers\"");
#  define IGNORE_MISSING_INITIALIZERS_OFF _Pragma("GCC diagnostic pop");
#elif defined(HAVE_MSVC)
// msvc does not warn for this it seems.
#  define IGNORE_MISSING_INITIALIZERS_ON
#  define IGNORE_MISSING_INITIALIZERS_OFF
#elif defined(HAVE_CLANG)
#  define IGNORE_MISSING_INITIALIZERS_ON \
    _Pragma("clang diagnostic push");    \
    _Pragma("clang diagnostic ignored \"-Wmissing-field-initializers\"");
#  define IGNORE_MISSING_INITIALIZERS_OFF _Pragma("clang diagnostic pop");
#else
#  define IGNORE_MISSING_INITIALIZERS_ON
#  define IGNORE_MISSING_INITIALIZERS_OFF
#endif


#if defined(HAVE_GCC)
#  define IGNORE_DEPRECATED_ON      \
    _Pragma("GCC diagnostic push"); \
    _Pragma("GCC diagnostic ignored \"-Wdeprecated-declarations\"");
#  define IGNORE_DEPRECATED_OFF _Pragma("GCC diagnostic pop")
#elif defined(HAVE_CLANG)
#  define IGNORE_DEPRECATED_ON        \
    _Pragma("clang diagnostic push"); \
    _Pragma("clang diagnostic ignored \"-Wdeprecated-declarations\"");
#  define IGNORE_DEPRECATED_OFF _Pragma("clang diagnostic pop")
#elif defined(HAVE_MSVC)
#  define IGNORE_DEPRECATED_ON  \
    _Pragma("warning( push )"); \
    _Pragma("warning( disable : 4996 )")
#  define IGNORE_DEPRECATED_OFF _Pragma("warning( pop )")
#else
#  define IGNORE_DEPRECATED_ON
#  define IGNORE_DEPRECATED_OFF
#endif

#if defined(HAVE_GCC)
#  define IGNORE_UNREFERENCED_FUNCTION_ON \
    _Pragma("GCC diagnostic push");       \
    _Pragma("GCC diagnostic ignored \"-Wunused-function\"");
#  define IGNORE_UNREFERENCED_FUNCTION_OFF _Pragma("GCC diagnostic pop")
#elif defined(HAVE_CLANG)
#  define IGNORE_UNREFERENCED_FUNCTION_ON \
    _Pragma("clang diagnostic push");     \
    _Pragma("clang diagnostic ignored \"-Wunused-function\"");
#  define IGNORE_UNREFERENCED_FUNCTION_OFF _Pragma("clang diagnostic pop")
#elif defined(HAVE_MSVC)
#  define IGNORE_UNREFERENCED_FUNCTION_ON \
    _Pragma("warning( push )");           \
    _Pragma("warning( disable : 4505 )")
#  define IGNORE_UNREFERENCED_FUNCTION_OFF _Pragma("warning( pop )")
#else
#  define IGNORE_UNREFERENCED_FUNCTION_ON
#  define IGNORE_UNREFERENCED_FUNCTION_OFF
#endif

#if defined(HAVE_MSVC)
#  define IGNORE_INT_PTR_CAST_ON \
    _Pragma("warning( push )");  \
    _Pragma("warning( disable : 4312 )")
#  define IGNORE_INT_PTR_CAST_OFF _Pragma("warning( pop )")
#else
#  define IGNORE_INT_PTR_CAST_ON
#  define IGNORE_INT_PTR_CAST_OFF
#endif


#if defined(HAVE_MSVC)
#  define PRINTF_LIKE(fmt_pos, arg_pos)
#else
#  define PRINTF_LIKE(fmt_pos, arg_pos) \
    __attribute__((format(printf, fmt_pos, arg_pos)))
#endif

#endif  // BAREOS_INCLUDE_COMPILER_MACRO_H_
