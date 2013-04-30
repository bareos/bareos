/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2007-2011 Free Software Foundation Europe e.V.

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
 * Interface definition for Bacula Plugins
 *
 * Kern Sibbald, October 2007
 *
 */

#ifndef __SD_PLUGINS_H
#define __SD_PLUGINS_H

#ifndef _BACULA_H
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
 *                Bacula definitions                                        *
 *                                                                          *
 ****************************************************************************/

/*
 * Bacula Variable Ids (Read)
 */
typedef enum {
  bsdVarJob = 1,
  bsdVarLevel = 2,
  bsdVarType = 3,
  bsdVarJobId = 4,
  bsdVarClient = 5,
  bsdVarNumVols = 6,
  bsdVarPool = 7,
  bsdVarStorage = 8,
  bsdVarCatalog = 9,
  bsdVarMediaType = 10,
  bsdVarJobName = 11,
  bsdVarJobStatus = 12,
  bsdVarPriority = 13,
  bsdVarVolumeName = 14,
  bsdVarCatalogRes = 15,
  bsdVarJobErrors = 16,
  bsdVarJobFiles  = 17,
  bsdVarSDJobFiles = 18,
  bsdVarSDErrors = 19,
  bsdVarFDJobStatus = 20,
  bsdVarSDJobStatus = 21
} bsdrVariable;

/*
 * Bacula Variable Ids (Write)
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
  bsdEventVolumeStatus = 17
} bsdEventType;

#define SD_NR_EVENTS bsdEventVolumeStatus /* keep this updated ! */

typedef struct s_bsdEvent {
   uint32_t eventType;
} bsdEvent;

typedef struct s_sdbaculaInfo {
   uint32_t size;
   uint32_t version;
} bsdInfo;

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Bacula interface version and function pointers
 */
typedef struct s_sdbaculaFuncs {
   uint32_t size;
   uint32_t version;
   bRC (*registerBaculaEvents)(bpContext *ctx, int nr_events, ...);
   bRC (*getBaculaValue)(bpContext *ctx, bsdrVariable var, void *value);
   bRC (*setBaculaValue)(bpContext *ctx, bsdwVariable var, void *value);
   bRC (*JobMessage)(bpContext *ctx, const char *file, int line,
                     int type, utime_t mtime, const char *fmt, ...);
   bRC (*DebugMessage)(bpContext *ctx, const char *file, int line,
                       int level, const char *fmt, ...);
   char *(*EditDeviceCodes)(DCR *dcr, char *omsg,
                            const char *imsg, const char *cmd);
   char *(*LookupCryptoKey)(const char *VolumeName);
   bool (*UpdateVolumeInfo)(DCR *dcr);
} bsdFuncs;

/*
 * Bacula Core Routines -- not used within a plugin
 */
#ifdef STORAGE_DAEMON
void load_sd_plugins(const char *plugin_dir);
void unload_sd_plugins(void);
int list_sd_plugins(POOL_MEM &msg);
void new_plugins(JCR *jcr);
void free_plugins(JCR *jcr);
int generate_plugin_event(JCR *jcr, bsdEventType event, void *value=NULL);
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
#define SD_PLUGIN_INTERFACE_VERSION  1

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

typedef struct s_sdbaculaDevStatTrigger {
   DEVRES *device;
   POOLMEM *status;
   int status_length;
} bsdDevStatTrig;

#ifdef __cplusplus
}
#endif

#endif /* __SD_PLUGINS_H */
