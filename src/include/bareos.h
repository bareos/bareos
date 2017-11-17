/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2010 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2013 Bareos GmbH & Co. KG

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
 * bareos.h -- main header file to include in all Bareos source
 *
 */

#ifndef _BAREOS_H
#define _BAREOS_H 1

/* Disable FORTIFY_SOURCE, because bareos uses is own memory
 * manager
 */
#ifdef _FORTIFY_SOURCE
#undef _FORTIFY_SOURCE
#endif

#ifdef __cplusplus
/* Workaround for SGI IRIX 6.5 */
#define _LANGUAGE_C_PLUS_PLUS 1
#endif

#include "hostconfig.h"

#ifdef HAVE_HPUX_OS
#undef HAVE_LCHMOD
#undef HAVE_POSIX_FADVISE
#endif

#define _REENTRANT    1
#define _THREAD_SAFE  1
#define _POSIX_PTHREAD_SEMANTICS 1

/* System includes */
#if HAVE_STDINT_H
#ifndef __sgi
#include <stdint.h>
#endif
#endif
#if HAVE_STDARG_H
#include <stdarg.h>
#endif
#include <stdio.h>
#if HAVE_STDLIB_H
#include <stdlib.h>
#endif
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#if HAVE_UMEM_H
#include <umem.h>
#endif
#if HAVE_ALLOCA_H
#include <alloca.h>
#endif
#if defined(_MSC_VER)
#include <io.h>
#include <direct.h>
#include <process.h>
#endif
#include <errno.h>
#include <fcntl.h>

/* O_NOATIME is defined at fcntl.h when supported */
#ifndef O_NOATIME
#define O_NOATIME 0
#endif

#if defined(_MSC_VER)
extern "C" {
#include "getopt.h"
}
#endif

#ifdef xxxxx
#ifdef HAVE_GETOPT_LONG
#include <getopt.h>
#else
#include "lib/getopt.h"
#endif
#endif

#include <string.h>
#include <strings.h>
#include <signal.h>
#include <ctype.h>
#ifndef _SPLINT_
#include <syslog.h>
#endif
#if HAVE_LIMITS_H
#include <limits.h>
#endif
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <netdb.h>
#include <sys/types.h>
#ifdef HAVE_SYS_BITYPES_H
#include <sys/bitypes.h>
#endif
#include <sys/ioctl.h>
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#if defined(HAVE_WIN32) & !defined(HAVE_MINGW)
#include <winsock2.h>
#endif
#if !defined(HAVE_WIN32) & !defined(HAVE_MINGW)
#include <sys/stat.h>
#endif
#include <sys/time.h>
#if HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#if defined(HAVE_WIN32)
#include <windows.h>
#endif

/*
 * Local Bareos includes. Be sure to put all the system includes before these.
 */
#include "version.h"
#include "bc_types.h"
#if defined(HAVE_WIN32)
#include "compat.h"
#endif
#include "streams.h"
#include "filetypes.h"
#include "baconfig.h"
#include "lib/lib.h"

/*
 * For wx-console compiles, we undo some Bareos defines.
 *  This prevents conflicts between wx-Widgets and Bareos.
 *  In wx-console files that malloc or free() Bareos structures
 *  config/resources and interface to the Bareos libraries,
 *  you must use bmalloc() and bfree().
 */
#ifdef HAVE_WXCONSOLE
#undef New
#undef _
#undef free
#undef malloc
#endif

#if defined(HAVE_WIN32)
#include "winapi.h"
#include "winhost.h"
#else
#include "host.h"
#endif

#ifndef HAVE_ZLIB_H
#undef HAVE_LIBZ                      /* no good without headers */
#endif

#endif
