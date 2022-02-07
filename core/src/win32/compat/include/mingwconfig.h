/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2013-2021 Bareos GmbH & Co. KG

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
/* ------------------------------------------------------------------------- */
/* --                     CONFIGURE SPECIFIED FEATURES                    -- */
/* ------------------------------------------------------------------------- */

#ifndef BAREOS_WIN32_COMPAT_INCLUDE_MINGWCONFIG_H_
#define BAREOS_WIN32_COMPAT_INCLUDE_MINGWCONFIG_H_

/* Define if you want to use Batch Mode */
/* #define HAVE_BATCH_FILE_INSERT 1 */

/* Define if you have GCC */
#define HAVE_GCC 1

/* Define if you have sys/bitypes.h */
/* #undef HAVE_SYS_BITYPES_H */

/* Define if you have zlib */
#define HAVE_LIBZ 1

/* Define if you have lzo lib */
#define HAVE_LZO 1

/* File daemon specif libraries */
#define FDLIBS 1

/* Define to 1 if you have `alloca', as a function or macro. */
#ifndef HAVE_MINGW
#  define alloca _alloca
#endif

/* Define to 1 if you have the `getmntent' function. */
/*#define HAVE_GETMNTENT 1 */

/* Define to 1 if you have the `getpid' function. */
#define getpid _getpid

/* Define to 1 if you have the <grp.h> header file. */
/*#define HAVE_GRP_H 1*/

/* Define to 1 if you have the `lchown' function. */
#define HAVE_LCHOWN 1

/* Define to 1 if you have the `localtime_r' function. */
#define HAVE_LOCALTIME_R 1

/* Define to 1 if you have the <mtio.h> header file. */
/* #undef HAVE_MTIO_H */

/* Define to 1 if you have the `nanosleep' function. */
/* #undef HAVE_NANOSLEEP  */

/* Define to 1 if you have the <pwd.h> header file. */
/* #undef HAVE_PWD_H */

/* Define to 1 if you have the `Readdir_r' function. */
/* #undef HAVE_READDIR_R */

/* Define to 1 if you have the <resolv.h> header file. */
/* #undef HAVE_RESOLV_H */

/* Define to 1 if translation of program messages to the user's native
   language is requested. */
#if (defined _MSC_VER) && (_MSC_VER >= 1400)  // VC8+
/* Enable NLS only if we are using the new VC++.
 * NLS should also work with VC++ 7.1, but the Makefiles are
 * not adapted to support it (include, lib...). */
#  define ENABLE_NLS 1
#endif

#undef LOCALEDIR
#define LOCALEDIR "."

/* Define to 1 if you have the <sys/mtio.h> header file. */
#define HAVE_SYS_MTIO_H 1

/* Define to 1 if you have the <sys/time.h> header file. */
#define HAVE_SYS_TIME_H 1

/* Define to 1 if you have the <zlib.h> header file. */
#define HAVE_ZLIB_H 1

/* Define to the full name of this package. */
#define PACKAGE_NAME ""

/* Define to the full name and version of this package. */
#define PACKAGE_STRING ""

/* Define to the version of this package. */
#define PACKAGE_VERSION ""

/* Define to 1 if you can safely include both <sys/time.h> and <time.h>. */
#define TIME_WITH_SYS_TIME 1

/* Use long unsigned int for ioctl request */
#define HAVE_IOCTL_ULINT_REQUEST

/* Number of bits in a file offset, on hosts where this is settable. */
#define _FILE_OFFSET_BITS 64

/* Define to make fseeko etc. visible, on some hosts. */
#define _LARGEFILE_SOURCE 1

/* Define for large files, on AIX-style hosts. */
#define _LARGE_FILES 1

/* Define to 1 if encryption support should be enabled */
#define HAVE_CRYPTO 1

/* Define to 1 if TLS support should be enabled */
#define HAVE_TLS 1

/* Define to 1 if OpenSSL library is available */
#define HAVE_OPENSSL 1

/* Define to 1 if you have the `getaddrinfo' function. */
#define HAVE_GETADDRINFO 1

/* Define to 1 if you have the `__builtin_va_copy' function. */
#define HAVE___BUILTIN_VA_COPY 1

/* Directory for daemon files */
#define PATH_BAREOS_WORKINGDIR "%TEMP%"

/* Directory for backend drivers */
#define PATH_BAREOS_BACKENDDIR "."

/* Default path for Bareos Python modules */
#define PYTHON_MODULE_PATH "."

/* Define to 1 if dynamic loading of catalog backends is enabled */
#define HAVE_DYNAMIC_CATS_BACKENDS 1

/* Define to 1 if DB batch insert code enabled */
#define USE_BATCH_FILE_INSERT 1

/* Set if you have an SQLite3 Database */
#define HAVE_SQLITE3 1

/* Define to 1 if you have the `sqlite3_threadsafe' function. */
#define HAVE_SQLITE3_THREADSAFE 1

/* Set if you have an PostgreSQL Database */
#define HAVE_POSTGRESQL 1

/* Set if PostgreSQL DB batch insert code enabled */
#define HAVE_POSTGRESQL_BATCH_FILE_INSERT 1

/* Set if have PQisthreadsafe */
#define HAVE_PQISTHREADSAFE 1

/* Define to 1 if LMDB support should be enabled */
#define HAVE_LMDB 1

/* Define to 1 if you have jansson lib */
#define HAVE_JANSSON 1

/* Define to 1 if you have the `glob' function. */
#define HAVE_GLOB 1

/* Define to 1 if you have the `inet_ntop' function. */
#define HAVE_INET_NTOP 1

#endif  // BAREOS_WIN32_COMPAT_INCLUDE_MINGWCONFIG_H_
