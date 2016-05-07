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
 * Interface definition for Bareos SD Plugins
 *
 * Kern Sibbald, October 2007
 */

#ifndef __SD_PLUGINS_H
#define __SD_PLUGINS_H

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
   bsdVarJob = 1,
   bsdVarLevel = 2,
   bsdVarType = 3,
   bsdVarJobId = 4,
   bsdVarClient = 5,
   bsdVarPool = 6,
   bsdVarPoolType = 7,
   bsdVarStorage = 8,
   bsdVarMediaType = 9,
   bsdVarJobName = 10,
   bsdVarJobStatus = 11,
   bsdVarVolumeName = 12,
   bsdVarJobErrors = 13,
   bsdVarJobFiles = 14,
   bsdVarJobBytes = 15,
   bsdVarCompatible = 16,
   bsdVarPluginDir = 17
} bsdrVariable;

/*
 * Bareos Variable Ids (Write)
 */
typedef enum {
   bsdwVarJobReport = 1,
   bsdwVarVolumeName = 2,
   bsdwVarPriority = 3,
   bsdwVarJobLevel = 4
} bsdwVariable;

/*
 * Events that are passed to plugin
 */
typedef enum {
   bsdEventJobStart = 1,
   bsdEventJobEnd = 2,
   bsdEventDeviceInit = 3,
   bsdEventDeviceMount = 4,
   bsdEventVolumeLoad = 5,
   bsdEventDeviceTryOpen = 6,
   bsdEventDeviceOpen = 7,
   bsdEventLabelRead = 8,
   bsdEventLabelVerified = 9,
   bsdEventLabelWrite = 10,
   bsdEventDeviceClose = 11,
   bsdEventVolumeUnload = 12,
   bsdEventDeviceUnmount = 13,
   bsdEventReadError = 14,
   bsdEventWriteError = 15,
   bsdEventDriveStatus = 16,
   bsdEventVolumeStatus = 17,
   bsdEventSetupRecordTranslation = 18,
   bsdEventReadRecordTranslation = 19,
   bsdEventWriteRecordTranslation = 20,
   bsdEventDeviceReleased = 21,
   bsdEventNewPluginOptions = 22
} bsdEventType;

#define SD_NR_EVENTS bsdEventNewPluginOptions /* keep this updated ! */

typedef struct s_bsdEvent {
   uint32_t eventType;
} bsdEvent;

typedef struct s_sdbareosInfo {
   uint32_t size;
   uint32_t version;
} bsdInfo;

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Bareos interface version and function pointers
 */
class DCR;
struct DEV_RECORD;

typedef struct s_sdbareosFuncs {
   uint32_t size;
   uint32_t version;
   bRC (*registerBareosEvents)(bpContext *ctx, int nr_events, ...);
   bRC (*getBareosValue)(bpContext *ctx, bsdrVariable var, void *value);
   bRC (*setBareosValue)(bpContext *ctx, bsdwVariable var, void *value);
   bRC (*JobMessage)(bpContext *ctx, const char *file, int line,
                     int type, utime_t mtime, const char *fmt, ...);
   bRC (*DebugMessage)(bpContext *ctx, const char *file, int line,
                       int level, const char *fmt, ...);
   char *(*EditDeviceCodes)(DCR *dcr, POOLMEM *&omsg,
                            const char *imsg, const char *cmd);
   char *(*LookupCryptoKey)(const char *VolumeName);
   bool (*UpdateVolumeInfo)(DCR *dcr);
   void (*UpdateTapeAlert)(DCR *dcr, uint64_t flags);
   DEV_RECORD *(*new_record)(bool with_data);
   void (*copy_record_state)(DEV_RECORD *dst, DEV_RECORD *src);
   void (*free_record)(DEV_RECORD *rec);
} bsdFuncs;

/*
 * Bareos Core Routines -- not used within a plugin
 */
#ifdef STORAGE_DAEMON
void load_sd_plugins(const char *plugin_dir, alist *plugin_names);
void unload_sd_plugins(void);
int list_sd_plugins(POOL_MEM &msg);
void dispatch_new_plugin_options(JCR *jcr);
void new_plugins(JCR *jcr);
void free_plugins(JCR *jcr);
bRC generate_plugin_event(JCR *jcr, bsdEventType event,
                          void *value = NULL, bool reverse = false);
#endif

/****************************************************************************
 *                                                                          *
 *                Plugin definitions                                        *
 *                                                                          *
 ****************************************************************************/

typedef enum {
  psdVarName = 1,
  psdVarDescription = 2
} psdVariable;

#define SD_PLUGIN_MAGIC     "*SDPluginData*"
#define SD_PLUGIN_INTERFACE_VERSION  3

/*
 * Functions that must be defined in every plugin
 */
typedef struct s_sdpluginFuncs {
   uint32_t size;
   uint32_t version;
   bRC (*newPlugin)(bpContext *ctx);
   bRC (*freePlugin)(bpContext *ctx);
   bRC (*getPluginValue)(bpContext *ctx, psdVariable var, void *value);
   bRC (*setPluginValue)(bpContext *ctx, psdVariable var, void *value);
   bRC (*handlePluginEvent)(bpContext *ctx, bsdEvent *event, void *value);
} psdFuncs;

#define sdplug_func(plugin) ((psdFuncs *)(plugin->pfuncs))
#define sdplug_info(plugin) ((genpInfo *)(plugin->pinfo))

class DEVRES;
typedef struct s_sdbareosDevStatTrigger {
   DEVRES *device;
   POOLMEM *status;
   int status_length;
} bsdDevStatTrig;

#ifdef __cplusplus
}
#endif

#endif /* __SD_PLUGINS_H */
