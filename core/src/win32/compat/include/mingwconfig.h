/* ------------------------------------------------------------------------- */
/* --                     CONFIGURE SPECIFIED FEATURES                    -- */
/* ------------------------------------------------------------------------- */

#ifndef __MINGWCONFIG_H
#define __MINGWCONFIG_H

/* Define if you want to use Batch Mode */
/* #define HAVE_BATCH_FILE_INSERT 1 */

/* Define if you have GCC */
#define HAVE_GCC 1

/* Define if you want TCP Wrappers support */
/* #undef HAVE_LIBWRAP */

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
#define alloca _alloca
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

/* Define to 1 if you have the `setenv' function. */
/* #undef HAVE_SETENV */

/* Define to 1 if you have the `putenv' function. */
#define HAVE_PUTENV 1

/* Define to 1 if translation of program messages to the user's native
   language is requested. */
#if (defined _MSC_VER) && (_MSC_VER >= 1400)  // VC8+
/* Enable NLS only if we are using the new VC++.
 * NLS should also work with VC++ 7.1, but the Makefiles are
 * not adapted to support it (include, lib...). */
#define ENABLE_NLS 1
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

/* Define to 1 if IPv6 support should be enabled */
#define HAVE_IPV6 1

/* Define to 1 if you have the `getaddrinfo' function. */
#define HAVE_GETADDRINFO 1

/* Define to 1 if you have the `__builtin_va_copy' function. */
#define HAVE___BUILTIN_VA_COPY 1

/* Directory for PID files */
#define PATH_BAREOS_PIDDIR "%TEMP%"

/* Directory for daemon files */
#define PATH_BAREOS_WORKINGDIR "%TEMP%"

/* Directory for backend drivers */
#define PATH_BAREOS_BACKENDDIR "."

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

#endif /* __MINGWNCONFIG_H */
