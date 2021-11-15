/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2007-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2021 Bareos GmbH & Co. KG

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
// Kern Sibbald, October 2007
/**
 * @file
 * Interface definition for Bareos DIR Plugins
 */

#ifndef BAREOS_DIRD_DIR_PLUGINS_H_
#define BAREOS_DIRD_DIR_PLUGINS_H_

#ifndef BAREOS_INCLUDE_BAREOS_H_
#  ifdef __cplusplus
/* Workaround for SGI IRIX 6.5 */
#    define _LANGUAGE_C_PLUS_PLUS 1
#  endif
#  define _REENTRANT 1
#  define _THREAD_SAFE 1
#  define _POSIX_PTHREAD_SEMANTICS 1
#  define _FILE_OFFSET_BITS 64
#  define _LARGEFILE_SOURCE 1
#  define _LARGE_FILES 1
#endif

#include <sys/types.h>
#include "include/config.h"
#include "include/bc_types.h"
#include "lib/plugins.h"

template <typename T> class alist;

namespace directordaemon {

//  Bareos definitions

// Bareos Variable Ids (Read)
typedef enum
{
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

// Bareos Variable Ids (Write)
typedef enum
{
  bwDirVarJobReport = 1,
  bwDirVarVolumeName = 2,
  bwDirVarPriority = 3,
  bwDirVarJobLevel = 4
} bwDirVariable;

// Events that are passed to plugin
typedef enum
{
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
} PluginApiDefinition;

#ifdef __cplusplus
extern "C" {
#endif

// Bareos interface version and function pointers
typedef struct s_dirbareosFuncs {
  uint32_t size;
  uint32_t version;
  bRC (*registerBareosEvents)(PluginContext* ctx, int nr_events, ...);
  bRC (*unregisterBareosEvents)(PluginContext* ctx, int nr_events, ...);
  bRC (*getInstanceCount)(PluginContext* ctx, int* ret);
  bRC (*getBareosValue)(PluginContext* ctx, brDirVariable var, void* value);
  bRC (*setBareosValue)(PluginContext* ctx, bwDirVariable var, void* value);
  bRC (*JobMessage)(PluginContext* ctx,
                    const char* file,
                    int line,
                    int type,
                    utime_t mtime,
                    const char* fmt,
                    ...);
  bRC (*DebugMessage)(PluginContext* ctx,
                      const char* file,
                      int line,
                      int level,
                      const char* fmt,
                      ...);
} CoreFunctions;

// Bareos Core Routines -- not used within a plugin
#ifdef DIRECTOR_DAEMON
void LoadDirPlugins(const char* plugin_dir, alist<const char*>* plugin_names);
void UnloadDirPlugins(void);
int ListDirPlugins(PoolMem& msg);
void DispatchNewPluginOptions(JobControlRecord* jcr);
void NewPlugins(JobControlRecord* jcr);
void FreePlugins(JobControlRecord* jcr);
bRC GeneratePluginEvent(JobControlRecord* jcr,
                        bDirEventType event,
                        void* value = NULL,
                        bool reverse = false);
#endif

// Plugin definitions
typedef enum
{
  pVarName = 1,
  pVarDescription = 2
} pVariable;

#define DIR_PLUGIN_MAGIC "*DirPluginData*"
#define DIR_PLUGIN_INTERFACE_VERSION 4

typedef struct s_dirpluginFuncs {
  uint32_t size;
  uint32_t version;
  bRC (*newPlugin)(PluginContext* ctx);
  bRC (*freePlugin)(PluginContext* ctx);
  bRC (*getPluginValue)(PluginContext* ctx, pVariable var, void* value);
  bRC (*setPluginValue)(PluginContext* ctx, pVariable var, void* value);
  bRC (*handlePluginEvent)(PluginContext* ctx, bDirEvent* event, void* value);
} PluginFunctions;

#define DirplugFunc(plugin) ((PluginFunctions*)(plugin->plugin_functions))
#define dirplug_info(plugin) ((PluginInformation*)(plugin->plugin_information))

#ifdef __cplusplus
}
#endif

} /* namespace directordaemon */
#endif  // BAREOS_DIRD_DIR_PLUGINS_H_
