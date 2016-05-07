/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2007-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2013 Bareos GmbH & Co. KG

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation, which is
   listed in the file LICENSE.

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
 * Interface definition for Bareos DIR Plugins
 *
 * Kern Sibbald, October 2007
 */

#ifndef __DIR_PLUGINS_H
#define __DIR_PLUGINS_H

#ifndef _BAREOS_H
#ifdef __cplusplus
/* Workaround for SGI IRIX 6.5 */
#define _LANGUAGE_C_PLUS_PLUS 1
#endif
#define _REENTRANT    1
#define _THREAD_SAFE  1
#define _POSIX_PTHREAD_SEMANTICS 1
#define _FILE_OFFSET_BITS 64
#define _LARGEFILE_SOURCE 1
#define _LARGE_FILES 1
#endif

#include <sys/types.h>
#include "hostconfig.h"
#include "bc_types.h"
#include "lib/plugins.h"

/****************************************************************************
 *                                                                          *
 *                Bareos definitions                                        *
 *                                                                          *
 ****************************************************************************/

/*
 * Bareos Variable Ids (Read)
 */
typedef enum {
   bDirVarJob = 1,
   bDirVarLevel = 2,
   bDirVarType = 3,
   bDirVarJobId = 4,
   bDirVarClient = 5,
   bDirVarNumVols = 6,
   bDirVarPool = 7,
   bDirVarStorage = 8,
   bDirVarWriteStorage = 9,
   bDirVarReadStorage = 10,
   bDirVarCatalog = 11,
   bDirVarMediaType = 12,
   bDirVarJobName = 13,
   bDirVarJobStatus = 14,
   bDirVarPriority = 15,
   bDirVarVolumeName = 16,
   bDirVarCatalogRes = 17,
   bDirVarJobErrors = 18,
   bDirVarJobFiles = 19,
   bDirVarSDJobFiles = 20,
   bDirVarSDErrors = 21,
   bDirVarFDJobStatus = 22,
   bDirVarSDJobStatus = 23,
   bDirVarPluginDir = 24,
   bDirVarLastRate = 25,
   bDirVarJobBytes = 26,
   bDirVarReadBytes = 27
} brDirVariable;

/*
 * Bareos Variable Ids (Write)
 */
typedef enum {
   bwDirVarJobReport = 1,
   bwDirVarVolumeName = 2,
   bwDirVarPriority = 3,
   bwDirVarJobLevel = 4
} bwDirVariable;

/*
 * Events that are passed to plugin
 */
typedef enum {
   bDirEventJobStart = 1,
   bDirEventJobEnd = 2,
   bDirEventJobInit = 3,
   bDirEventJobRun = 4,
   bDirEventVolumePurged = 5,
   bDirEventNewVolume = 6,
   bDirEventNeedVolume = 7,
   bDirEventVolumeFull = 8,
   bDirEventRecyle = 9,
   bDirEventGetScratch = 10,
   bDirEventNewPluginOptions = 11
} bDirEventType;

#define DIR_NR_EVENTS bDirEventNewPluginOptions /* keep this updated ! */

typedef struct s_bDirEvent {
   uint32_t eventType;
} bDirEvent;

typedef struct s_dirbareosInfo {
   uint32_t size;
   uint32_t version;
} bDirInfo;

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Bareos interface version and function pointers
 */
typedef struct s_dirbareosFuncs {
   uint32_t size;
   uint32_t version;
   bRC (*registerBareosEvents)(bpContext *ctx, int nr_events, ...);
   bRC (*unregisterBareosEvents)(bpContext *ctx, int nr_events, ...);
   bRC (*getBareosValue)(bpContext *ctx, brDirVariable var, void *value);
   bRC (*setBareosValue)(bpContext *ctx, bwDirVariable var, void *value);
   bRC (*JobMessage)(bpContext *ctx, const char *file, int line,
                     int type, utime_t mtime, const char *fmt, ...);
   bRC (*DebugMessage)(bpContext *ctx, const char *file, int line,
                       int level, const char *fmt, ...);
} bDirFuncs;

/*
 * Bareos Core Routines -- not used within a plugin
 */
#ifdef DIRECTOR_DAEMON
void load_dir_plugins(const char *plugin_dir, alist *plugin_names);
void unload_dir_plugins(void);
int list_dir_plugins(POOL_MEM &msg);
void dispatch_new_plugin_options(JCR *jcr);
void new_plugins(JCR *jcr);
void free_plugins(JCR *jcr);
bRC generate_plugin_event(JCR *jcr, bDirEventType event,
                          void *value = NULL, bool reverse = false);
#endif

/****************************************************************************
 *                                                                          *
 *                Plugin definitions                                        *
 *                                                                          *
 ****************************************************************************/

typedef enum {
  pDirVarName = 1,
  pDirVarDescription = 2
} pDirVariable;

#define DIR_PLUGIN_MAGIC     "*DirPluginData*"
#define DIR_PLUGIN_INTERFACE_VERSION  4

typedef struct s_dirpluginFuncs {
   uint32_t size;
   uint32_t version;
   bRC (*newPlugin)(bpContext *ctx);
   bRC (*freePlugin)(bpContext *ctx);
   bRC (*getPluginValue)(bpContext *ctx, pDirVariable var, void *value);
   bRC (*setPluginValue)(bpContext *ctx, pDirVariable var, void *value);
   bRC (*handlePluginEvent)(bpContext *ctx, bDirEvent *event, void *value);
} pDirFuncs;

#define dirplug_func(plugin) ((pDirFuncs *)(plugin->pfuncs))
#define dirplug_info(plugin) ((genpInfo *)(plugin->pinfo))

#ifdef __cplusplus
}
#endif

#endif /* __FD_PLUGINS_H */
