/* ------------------------------------------------------------------------- */
/* --                     CONFIGURE SPECIFIED FEATURES                    -- */
/* ------------------------------------------------------------------------- */

#ifndef __MINGWCONFIG_H
#define __MINGWCONFIG_H

/* Define if you want SmartAlloc debug code enabled */
#define SMARTALLOC 1

/* Define if you want to use Batch Mode */
/* #define HAVE_BATCH_FILE_INSERT 1 */

/* Define if you need function prototypes */
#define PROTOTYPES 1

/* Define if you have GCC */
#define HAVE_GCC 1

/* Define to 1 if utime.h exists and declares struct utimbuf.  */
#define HAVE_UTIME_H 1

/* Data types */
#define HAVE_U_INT 1
#define HAVE_INTXX_T 1
#define HAVE_U_INTXX_T 1
/* #undef HAVE_UINTXX_T */
#define HAVE_INT64_T 1
#define HAVE_U_INT64_T 1
#define HAVE_UINT64_T 1
#define HAVE_INTMAX_T 1
/* #undef HAVE_U_INTMAX_T */
#define HAVE_UINTPTR_T 1
#define HAVE_INTPTR_T 1

/* Define if you want TCP Wrappers support */
/* #undef HAVE_LIBWRAP */

/* Define if you have sys/bitypes.h */
/* #undef HAVE_SYS_BITYPES_H */

/* Define if you have zlib */
#define HAVE_LIBZ 1

/* Define if you have lzo lib */
#define HAVE_LZO 1

/* Define to 1 if you have the <lzo/lzoconf.h> header file. */
#define HAVE_LZO_LZOCONF_H 1

/* Define if you have fastlz lib */
#define HAVE_FASTLZ 1


/* File daemon specif libraries */
#define FDLIBS 1

/* What kind of signals we have */
/*#define HAVE_POSIX_SIGNALS 1 */
/* #undef HAVE_BSD_SIGNALS */
/* #undef HAVE_USG_SIGHOLD */

/* Set to correct scanf value for long long int */
#define lld "lld"
#define llu "llu"
/* #define USE_BSNPRINTF */

#define HAVE_STRTOLL 1

/* Define to 1 if you have `alloca', as a function or macro. */
#define HAVE_ALLOCA 1
#ifndef HAVE_MINGW
#define alloca _alloca
#endif

/* Define to 1 if you have the <dirent.h> header file, and it defines `DIR'. */
#define HAVE_DIRENT_H 1

/* Define to 1 if you have the <dlfcn.h> header file. */
#define HAVE_DLFCN_H 1

/* Define to 1 if you have the <fcntl.h> header file. */
#define HAVE_FCNTL_H 1

/* Define to 1 if you have the `getcwd' function. */
#define HAVE_GETCWD 1

/* Define to 1 if you have the `gethostid' function. */
#define HAVE_GETHOSTID 1

/* Define to 1 if you have the `gethostname' function. */
#define HAVE_GETHOSTNAME 1

/* Define to 1 if you have the `getmntent' function. */
/*#define HAVE_GETMNTENT 1 */

/* Define to 1 if you have the `getpid' function. */
#define HAVE_GETPID 1
#define getpid _getpid

/* Define to 1 if you have the `gettimeofday' function. */
#define HAVE_GETTIMEOFDAY 1

/* Define to 1 if you have the <grp.h> header file. */
/*#define HAVE_GRP_H 1*/

/* Define to 1 if you have the `inet_pton' function. */
/* #undef HAVE_INET_PTON */

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define to 1 if you have the `lchown' function. */
#define HAVE_LCHOWN 1

/* Define to 1 if you have the <limits.h> header file. */
#define HAVE_LIMITS_H 1

/* Define to 1 if you have the `localtime_r' function. */
#define HAVE_LOCALTIME_R 1

/* Define to 1 if you have the `lstat' function. */
#define HAVE_LSTAT 1

/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1

/* Define to 1 if you have the <mtio.h> header file. */
/* #undef HAVE_MTIO_H */

/* Define to 1 if you have the `nanosleep' function. */
/* #undef HAVE_NANOSLEEP  */

/* Define to 1 if you have the <pwd.h> header file. */
/* #undef HAVE_PWD_H */

/* Define to 1 if you have the `readdir_r' function. */
/* #undef HAVE_READDIR_R */

/* Define to 1 if you have the <resolv.h> header file. */
/* #undef HAVE_RESOLV_H */

/* Define to 1 if you have the `select' function. */
#define HAVE_SELECT 1

/* Define to 1 if you have the `setenv' function. */
/* #undef HAVE_SETENV */

/* Define to 1 if you have the `putenv' function. */
#define HAVE_PUTENV 1

/* Define to 1 if you have the `setlocale' function. */
#undef HAVE_SETLOCALE

/* Define to 1 if translation of program messages to the user's native
   language is requested. */
#if (defined _MSC_VER) && (_MSC_VER >= 1400) // VC8+
/* Enable NLS only if we are using the new VC++.
 * NLS should also work with VC++ 7.1, but the Makefiles are
 * not adapted to support it (include, lib...). */
#define ENABLE_NLS 1
#endif

#undef  LOCALEDIR
#define LOCALEDIR "."

#undef HAVE_NL_LANGINFO

/* Define to 1 if you have the `setpgid' function. */
#define HAVE_SETPGID 1

/* Define to 1 if you have the `setpgrp' function. */
#define HAVE_SETPGRP 1

/* Define to 1 if you have the `setsid' function. */
#define HAVE_SETSID 1

/* Define to 1 if you have the `signal' function. */
/*#define HAVE_SIGNAL 1 */

/* Define to 1 if you have the `snprintf' function. */
#define HAVE_SNPRINTF 1

/* Define to 1 if you have the <stdarg.h> header file. */
#define HAVE_STDARG_H 1

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the `strcasecmp' function. */
#define HAVE_STRCASECMP 1

/* Define to 1 if you have the `strerror' function. */
#define HAVE_STRERROR 1

/* Define to 1 if you have the `strerror_r' function. */
#define HAVE_STRERROR_R 1

/* Define to 1 if you have the `strftime' function. */
#define HAVE_STRFTIME 1

/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the `strncmp' function. */
#define HAVE_STRNCMP 1

/* Define to 1 if you have the `strncpy' function. */
#define HAVE_STRNCPY 1

/* Define to 1 if you have the `strtoll' function. */
#define HAVE_STRTOLL 1

/* Define to 1 if `st_blksize' is member of `struct stat'. */
#define HAVE_STRUCT_STAT_ST_BLKSIZE 1

/* Define to 1 if `st_blocks' is member of `struct stat'. */
#define HAVE_STRUCT_STAT_ST_BLOCKS 1

/* Define to 1 if `st_rdev' is member of `struct stat'. */
#define HAVE_STRUCT_STAT_ST_RDEV 1

/* Define to 1 if `tm_zone' is member of `struct tm'. */
/* #undef HAVE_STRUCT_TM_TM_ZONE */

/* Define to 1 if your `struct stat' has `st_blksize'. Deprecated, use
   `HAVE_STRUCT_STAT_ST_BLKSIZE' instead. */
#define HAVE_ST_BLKSIZE 1

/* Define to 1 if your `struct stat' has `st_blocks'. Deprecated, use
   `HAVE_STRUCT_STAT_ST_BLOCKS' instead. */
#define HAVE_ST_BLOCKS 1

/* Define to 1 if your `struct stat' has `st_rdev'. Deprecated, use
   `HAVE_STRUCT_STAT_ST_RDEV' instead. */
#define HAVE_ST_RDEV 1

/* Define to 1 if you have the <sys/byteorder.h> header file. */
/* #undef HAVE_SYS_BYTEORDER_H */

/* Define to 1 if you have the <sys/dir.h> header file, and it defines `DIR'.
   */
/* #undef HAVE_SYS_DIR_H */

/* Define to 1 if you have the <sys/ioctl.h> header file. */
#define HAVE_SYS_IOCTL_H 1

/* Define to 1 if you have the <sys/mtio.h> header file. */
#define HAVE_SYS_MTIO_H 1

/* Define to 1 if you have the <sys/ndir.h> header file, and it defines `DIR'.
   */
/* #undef HAVE_SYS_NDIR_H */

/* Define to 1 if you have the <sys/select.h> header file. */
#define HAVE_SYS_SELECT_H 1

/* Define to 1 if you have the <sys/socket.h> header file. */
#define HAVE_SYS_SOCKET_H 1

/* Define to 1 if you have the <sys/sockio.h> header file. */
/* #undef HAVE_SYS_SOCKIO_H */

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/time.h> header file. */
#define HAVE_SYS_TIME_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have <sys/wait.h> that is POSIX.1 compatible. */
#define HAVE_SYS_WAIT_H 1

/* Define to 1 if you have the `tcgetattr' function. */
#define HAVE_TCGETATTR 1

/* Define to 1 if you have the <termios.h> header file. */
#define HAVE_TERMIOS_H 1

/* Define to 1 if your `struct tm' has `tm_zone'. Deprecated, use
   `HAVE_STRUCT_TM_TM_ZONE' instead. */
/* #undef HAVE_TM_ZONE */

/* Define to 1 if you don't have `tm_zone' but do have the external array
   `tzname'. */
#define HAVE_TZNAME 1

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* Define to 1 if you have the <varargs.h> header file. */
/* #undef HAVE_VARARGS_H */

/* Define to 1 if you have the `vfprintf' function. */
#define HAVE_VFPRINTF 1

/* Define to 1 if you have the `vprintf' function. */
#define HAVE_VPRINTF 1

/* Define to 1 if you have the `vsnprintf' function. */
#define HAVE_VSNPRINTF 1

/* Define to 1 if you have the <zlib.h> header file. */
#define HAVE_ZLIB_H 1

/* Define to 1 if `major', `minor', and `makedev' are declared in <mkdev.h>.
   */
/* #undef MAJOR_IN_MKDEV */

/* Define to 1 if `major', `minor', and `makedev' are declared in
   <sysmacros.h>. */
/* #undef MAJOR_IN_SYSMACROS */

/* Define to 1 if your C compiler doesn't accept -c and -o together. */
/* #undef NO_MINUS_C_MINUS_O */

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT ""

/* Define to the full name of this package. */
#define PACKAGE_NAME ""

/* Define to the full name and version of this package. */
#define PACKAGE_STRING ""

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME ""

/* Define to the version of this package. */
#define PACKAGE_VERSION ""

/* Define as the return type of signal handlers (`int' or `void'). */
#define RETSIGTYPE void

/* Define to 1 if the `setpgrp' function takes no argument. */
#define SETPGRP_VOID 1

/* The size of a `char', as computed by sizeof. */
#define SIZEOF_CHAR 1

/* The size of a `int', as computed by sizeof. */
#define SIZEOF_INT 4

/* The size of a `int *', as computed by sizeof. */
#define SIZEOF_INT_P 4

/* The size of a `long int', as computed by sizeof. */
#define SIZEOF_LONG_INT 4

/* The size of a `long long int', as computed by sizeof. */
#define SIZEOF_LONG_LONG_INT 8

/* The size of a `short int', as computed by sizeof. */
#define SIZEOF_SHORT_INT 2

/* If using the C implementation of alloca, define if you know the
   direction of stack growth for your system; otherwise it will be
   automatically deduced at run-time.
        STACK_DIRECTION > 0 => grows toward higher addresses
        STACK_DIRECTION < 0 => grows toward lower addresses
        STACK_DIRECTION = 0 => direction of growth unknown */
/* #undef STACK_DIRECTION */


/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* Define to 1 if you can safely include both <sys/time.h> and <time.h>. */
#define TIME_WITH_SYS_TIME 1

/* Define to 1 if your <sys/time.h> declares `struct tm'. */
/* #undef TM_IN_SYS_TIME */

/* Use long unsigned int for ioctl request */
#define HAVE_IOCTL_ULINT_REQUEST

/* For now, we only support Little endian on Win32 */
#define HAVE_LITTLE_ENDIAN 1

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

/* Define to 1 if readline support should be enabled */
#define HAVE_READLINE 1

/* Directory for PID files */
#define _PATH_BAREOS_PIDDIR "%TEMP%"

/* Directory for daemon files */
#define _PATH_BAREOS_WORKINGDIR "%TEMP%"

/* Directory for backend drivers */
#define _PATH_BAREOS_BACKENDDIR "."

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

/* Define to 1 if you have the <glob.h> header file. */
#define HAVE_GLOB_H 1

#endif /* __MINGWNCONFIG_H */
