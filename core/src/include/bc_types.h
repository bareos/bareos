/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2008 Free Software Foundation Europe e.V.
   Copyright (C) 2016-2024 Bareos GmbH & Co. KG

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
// Kern Sibbald, March MM
/**
 * @file
 * Define integer types for Bareos
 *
 * Integer types.  These types should be be used in all
 * contexts in which the length of an integer stored on
 * removable media must be known regardless of the
 * architecture of the platform.
 *
 * Bareos types are:
 *
 * int8_t,  int16_t,  int32_t,  int64_t
 * uint8_t, uint16_t, uint32_t, uint64_t
 *
 * Also, we define types such as file address lengths.
 */

#ifndef BAREOS_INCLUDE_BC_TYPES_H_
#define BAREOS_INCLUDE_BC_TYPES_H_

#include <stdint.h>
#include <cinttypes>

#ifdef HAVE_WIN32
typedef int __daddr_t;

#  if !defined(HAVE_MSVC) || (_MSC_VER < 1400)  // VC8+
#    ifndef _TIME_T_DEFINED
#      define _TIME_T_DEFINED
typedef long time_t;
#    endif
#  endif

#  if __STDC__ && !defined(HAVE_MINGW)
typedef _dev_t dev_t;
#    if !defined(HAVE_WXCONSOLE)
typedef __int64 ino_t;
#    endif
#  endif

#endif /* HAVE_WIN32 */

/**
 * These are the sizes of the current definitions of database
 *  Ids.  In general, FileId_t can be set to uint64_t and it
 *  *should* work.  Users have reported back that it does work
 *  for PostgreSQL.  For the other types, all places in Bareos
 *  have been converted, but no one has actually tested it.
 * In principle, the only field that really should need to be
 *  64 bits is the FileId_t
 */
typedef uint64_t FileId_t;
typedef uint32_t DBId_t; /* general DB id type */
#define PRIdbid PRIu32
typedef uint32_t JobId_t;

typedef char POOLMEM;

typedef double float64_t;

/* Bareos time -- Unix time with microseconds */
#define btime_t int64_t
/* Unix time (time_t) widened to 64 bits */
#define utime_t int64_t

#ifdef HAVE_WIN32
#  define sockopt_val_t const char*
#else
#  ifdef HAVE_OLD_SOCKOPT
#    define sockopt_val_t char*
#  else
#    define sockopt_val_t void*
#  endif
#endif /* HAVE_WIN32 */

/**
 * Status codes returned by CreateFile()
 *   Used in findlib, filed, and plugins
 */
enum
{
  CF_SKIP = 1, /**< skip file (not newer or something) */
  CF_ERROR,    /**< error creating file */
  CF_EXTRACT,  /**< file created, data to extract */
  CF_CREATED,  /**< file created, no data to extract */
  CF_CORE      /**< let bareos core handle the file creation */
};

#ifndef MAX
#  define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif
#ifndef MIN
#  define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#endif  // BAREOS_INCLUDE_BC_TYPES_H_
