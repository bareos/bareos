/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2010-2011 Bacula Systems(R) SA

   The main author of Bacula is Kern Sibbald, with contributions from
   many others, a complete list can be found in the file AUTHORS.
   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation, which is 
   listed in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.

   Bacula® is a registered trademark of Kern Sibbald.
   The licensor of Bacula is the Free Software Foundation Europe
   (FSFE), Fiduciary Program, Sumatrastrasse 25, 8006 Zürich,
   Switzerland, email:ftf@fsfeurope.org.
*/
/* 
 *  Written by James Harper, October 2008
 */

#ifndef _EXCHANGE_FD_H
#define _EXCHANGE_FD_H

#define BUILD_PLUGIN

#if defined(BUILDING_DLL)
#  define DLL_IMP_EXP   __declspec(dllexport)
#elif defined(USING_DLL)
#  define DLL_IMP_EXP   __declspec(dllimport)
#else
#  define DLL_IMP_EXP
#endif

#if defined(HAVE_WIN32)
#if defined(HAVE_MINGW)
#include "mingwconfig.h"
#else
#include "winconfig.h"
#endif
#else
#include "config.h"
#endif
#define __CONFIG_H

enum {
   /* Keep M_ABORT=1 for dlist.h */
   M_ABORT = 1,                       /* MUST abort immediately */
   M_DEBUG,                           /* debug message */
   M_FATAL,                           /* Fatal error, stopping job */
   M_ERROR,                           /* Error, but recoverable */
   M_WARNING,                         /* Warning message */
   M_INFO,                            /* Informational message */
   M_SAVED,                           /* Info on saved file */
   M_NOTSAVED,                        /* Info on notsaved file */
   M_SKIPPED,                         /* File skipped during backup by option setting */
   M_MOUNT,                           /* Mount requests */
   M_ERROR_TERM,                      /* Error termination request (no dump) */
   M_TERM,                            /* Terminating daemon normally */
   M_RESTORED,                        /* ls -l of restored files */
   M_SECURITY,                        /* security violation */
   M_ALERT,                           /* tape alert messages */
   M_VOLMGMT                          /* Volume management messages */
};

#define FT_REG        3
#define FT_DIREND     5

#define _REENTRANT    1
#define _THREAD_SAFE  1
#define _POSIX_PTHREAD_SEMANTICS 1

#include <stdint.h>

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#ifdef HAVE_SYS_BITYPES_H
#include <sys/bitypes.h>
#endif

//#include "bacula.h"
#include <windows.h>
#include "win32/compat/compat.h"
#include "bc_types.h"

typedef int64_t   boffset_t;
//#define bstrdup(str) strcpy((char *)bmalloc(strlen((str))+1),(str))
#define bstrdup(str) strdup(str)

#include "fd_plugins.h"
#include "api.h"

#if defined(HAVE_WIN32)
#include "win32/winapi.h"
#include "winhost.h"
#else
#include "host.h"
#endif

#define EXCHANGE_PLUGIN_VERSION 1

#define JOB_TYPE_BACKUP 1
#define JOB_TYPE_RESTORE 2

#define JOB_LEVEL_FULL ((int)'F')
#define JOB_LEVEL_INCREMENTAL ((int)'I')
#define JOB_LEVEL_DIFFERENTIAL ((int)'D')

struct exchange_fd_context_t;

#include "node.h"

struct exchange_fd_context_t {
   struct bpContext *bpContext;
   WCHAR *computer_name;
   char *path_bits[6];
   root_node_t *root_node;
   node_t *current_node;
   int job_type;
   int job_level;
   time_t job_since;
   bool notrunconfull_option;
   bool truncate_logs;
   bool accurate;
   bool plugin_active;
};

static inline char *tocharstring(WCHAR *src)
{
   char *tmp = new char[wcslen(src) + 1];
   wcstombs(tmp, src, wcslen(src) + 1);
   return tmp;
}

static inline WCHAR *towcharstring(char *src)
{
   WCHAR *tmp = new WCHAR[strlen(src) + 1];
   mbstowcs(tmp, src, strlen(src) + 1);
   return tmp;
}


extern bFuncs *bfuncs;
extern bInfo  *binfo;

#define _DebugMessage(level, message, ...) bfuncs->DebugMessage(context->bpContext, __FILE__, __LINE__, level, message, ##__VA_ARGS__)
#define _JobMessage(type, message, ...) bfuncs->JobMessage(context->bpContext, __FILE__, __LINE__, type, 0, message, ##__VA_ARGS__)
#define _JobMessageNull(type, message, ...) bfuncs->JobMessage(NULL, __FILE__, __LINE__, type, 0, message, ##__VA_ARGS__)

#define PLUGIN_PATH_PREFIX_BASE "@EXCHANGE"
#define PLUGIN_PATH_PREFIX_SERVICE "Microsoft Information Store"
#define PLUGIN_PATH_PREFIX_SERVICE_W L"Microsoft Information Store"

#endif /* _EXCHANGE_FD_H */
