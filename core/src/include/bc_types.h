/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2008 Free Software Foundation Europe e.V.
   Copyright (C) 2016-2016 Bareos GmbH & Co. KG

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
 * Kern Sibbald, March MM
 */
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

#ifdef HAVE_WIN32
typedef UINT64 u_int64_t;
typedef UINT64 uint64_t;
typedef INT64 int64_t;
typedef UINT32 uint32_t;
typedef INT64 intmax_t;
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef signed short int16_t;
typedef signed char int8_t;
typedef int __daddr_t;

#if !defined(HAVE_MINGW)
typedef long int32_t;
typedef float float32_t;
typedef double float64_t;
#endif

#if !defined(_MSC_VER) || (_MSC_VER < 1400)  // VC8+
#ifndef _TIME_T_DEFINED
#define _TIME_T_DEFINED
typedef long time_t;
#endif
#endif

#if __STDC__ && !defined(HAVE_MINGW)
typedef _dev_t dev_t;
#if !defined(HAVE_WXCONSOLE)
typedef __int64 ino_t;
#endif
#endif

typedef UINT32 u_int32_t;
typedef unsigned char u_int8_t;
typedef unsigned short u_int16_t;

#if !defined(HAVE_MINGW)
#undef uint32_t
#endif
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
typedef uint32_t JobId_t;


typedef char POOLMEM;


/* Types */

/* If sys/types.h does not supply intXX_t, supply them ourselves */
/* (or die trying) */

#ifndef HAVE_U_INT
typedef unsigned int u_int;
#endif

#ifndef HAVE_INTXX_T
#if (SIZEOF_CHAR == 1)
typedef signed char int8_t;
#else
#error "8 bit int type not found."
#endif
#if (SIZEOF_SHORT_INT == 2)
typedef short int int16_t;
#else
#error "16 bit int type not found."
#endif
#if (SIZEOF_INT == 4)
typedef int int32_t;
#else
#error "32 bit int type not found."
#endif
#endif

/* If sys/types.h does not supply u_intXX_t, supply them ourselves */
#ifndef HAVE_U_INTXX_T
#ifdef HAVE_UINTXX_T
typedef uint8_t u_int8_t;
typedef uint16_t u_int16_t;
typedef uint32_t u_int32_t;
#define HAVE_U_INTXX_T 1
#else
#if (SIZEOF_CHAR == 1)
typedef unsigned char u_int8_t;
#else
#error "8 bit int type not found. Required!"
#endif
#if (SIZEOF_SHORT_INT == 2)
typedef unsigned short int u_int16_t;
#else
#error "16 bit int type not found. Required!"
#endif
#if (SIZEOF_INT == 4)
typedef unsigned int u_int32_t;
#else
#error "32 bit int type not found. Required!"
#endif
#endif
#endif

/* 64-bit types */
#ifndef HAVE_INT64_T
#if (SIZEOF_LONG_LONG_INT == 8)
typedef long long int int64_t;
#define HAVE_INT64_T 1
#else
#if (SIZEOF_LONG_INT == 8)
typedef long int int64_t;
#define HAVE_INT64_T 1
#endif
#endif
#endif

#ifndef HAVE_INTMAX_T
#ifdef HAVE_INT64_T
typedef int64_t intmax_t;
#else
#error "64 bit type not found. Required!"
#endif
#endif

#ifndef HAVE_U_INT64_T
#if (SIZEOF_LONG_LONG_INT == 8)
typedef unsigned long long int u_int64_t;
#define HAVE_U_INT64_T 1
#else
#if (SIZEOF_LONG_INT == 8)
typedef unsigned long int u_int64_t;
#define HAVE_U_INT64_T 1
#else
#error "64 bit type not found. Required!"
#endif
#endif
#endif

#ifndef HAVE_U_INTMAX_T
#ifdef HAVE_U_INT64_T
typedef u_int64_t u_intmax_t;
#else
#error "64 bit type not found. Required!"
#endif
#endif

#ifndef HAVE_INTPTR_T
#define HAVE_INTPTR_T 1
#if (SIZEOF_INT_P == 4)
typedef int32_t intptr_t;
#else
#if (SIZEOF_INT_P == 8)
typedef int64_t intptr_t;
#else
#error "Can't find sizeof pointer. Required!"
#endif
#endif
#endif

#ifndef HAVE_UINTPTR_T
#define HAVE_UINTPTR_T 1
#if (SIZEOF_INT_P == 4)
typedef uint32_t uintptr_t;
#else
#if (SIZEOF_INT_P == 8)
typedef uint64_t uintptr_t;
#else
#error "Can't find sizeof pointer. Required!"
#endif
#endif
#endif

/* Limits for the above types. */
#undef INT8_MIN
#undef INT8_MAX
#undef UINT8_MAX
#undef INT16_MIN
#undef INT16_MAX
#undef UINT16_MAX
#undef INT32_MIN
#undef INT32_MAX
#undef UINT32_MAX

#define INT8_MIN (-127 - 1)
#define INT8_MAX (127)
#define UINT8_MAX (255u)
#define INT16_MIN (-32767 - 1)
#define INT16_MAX (32767)
#define UINT16_MAX (65535u)
#define INT32_MIN (-2147483647 - 1)
#define INT32_MAX (2147483647)
#define UINT32_MAX (4294967295u)

typedef double float64_t;
typedef float float32_t;


/* Define the uint versions actually used in Bareos */
#ifndef uint8_t
#define uint8_t u_int8_t
#define uint16_t u_int16_t
#define uint32_t u_int32_t
#define uint64_t u_int64_t
#define uintmax_t u_intmax_t
#endif

/* Bareos time -- Unix time with microseconds */
#define btime_t int64_t
/* Unix time (time_t) widened to 64 bits */
#define utime_t int64_t

#ifndef HAVE_SOCKLEN_T
#define socklen_t int
#endif

#ifdef HAVE_WIN32
#define sockopt_val_t const char*
#else
#ifdef HAVE_OLD_SOCKOPT
#define sockopt_val_t char*
#else
#define sockopt_val_t void*
#endif
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
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif
#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#endif /* BAREOS_INCLUDE_BC_TYPES_H_ */
