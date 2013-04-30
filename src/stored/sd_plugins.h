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
#ifndef __CONFIG_H
#define __CONFIG_H
#include "config.h"
#endif
#include "bc_types.h"
#include "lib/plugins.h"

#ifdef __cplusplus
extern "C" {
#endif




/****************************************************************************
 *                                                                          *
 *                Bacula definitions                                        *
 *                                                                          *
 ****************************************************************************/

/* Bacula Variable Ids */
typedef enum {
  bsdVarJob       = 1,
  bsdVarLevel     = 2,
  bsdVarType      = 3,
  bsdVarJobId     = 4,
  bsdVarClient    = 5,
  bsdVarNumVols   = 6,
  bsdVarPool      = 7,
  bsdVarStorage   = 8,
  bsdVarCatalog   = 9,
  bsdVarMediaType = 10,
  bsdVarJobName   = 11,
  bsdVarJobStatus = 12,
  bsdVarPriority  = 13,
  bsdVarVolumeName = 14,
  bsdVarCatalogRes = 15,
  bsdVarJobErrors  = 16,
  bsdVarJobFiles   = 17,
  bsdVarSDJobFiles = 18,
  bsdVarSDErrors   = 19,
  bsdVarFDJobStatus = 20,
  bsdVarSDJobStatus = 21
} bsdrVariable;

typedef enum {
  bsdwVarJobReport  = 1,
  bsdwVarVolumeName = 2,
  bsdwVarPriority   = 3,
  bsdwVarJobLevel   = 4
} bsdwVariable;


typedef enum {
  bsdEventJobStart       = 1,
  bsdEventJobEnd         = 2,
  bsdEventDeviceInit     = 3,
  bsdEventDeviceOpen     = 4,
  bsdEventDeviceTryOpen  = 5,
  bsdEventDeviceClose    = 6
} bsdEventType;

typedef struct s_bsdEvent {
   uint32_t eventType;
} bsdEvent;

typedef struct s_sdbaculaInfo {
   uint32_t size;
   uint32_t version;  
} bsdInfo;

/* Bacula interface version and function pointers */
typedef struct s_sdbaculaFuncs {  
   uint32_t size;
   uint32_t version;
   bRC (*registerBaculaEvents)(bpContext *ctx, ...);
   bRC (*getBaculaValue)(bpContext *ctx, bsdrVariable var, void *value);
   bRC (*setBaculaValue)(bpContext *ctx, bsdwVariable var, void *value);
   bRC (*JobMessage)(bpContext *ctx, const char *file, int line, 
       int type, utime_t mtime, const char *fmt, ...);
   bRC (*DebugMessage)(bpContext *ctx, const char *file, int line,
       int level, const char *fmt, ...);
   char *(*EditDeviceCodes)(DCR *dcr, char *omsg,
       const char *imsg, const char *cmd);
} bsdFuncs;

/* Bacula Subroutines */
void load_sd_plugins(const char *plugin_dir);
void new_plugins(JCR *jcr);
void free_plugins(JCR *jcr);
int generate_plugin_event(JCR *jcr, bsdEventType event, void *value=NULL);



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

typedef struct s_sdpluginInfo {
   uint32_t size;
   uint32_t version;
   const char *plugin_magic;
   const char *plugin_license;
   const char *plugin_author;
   const char *plugin_date;
   const char *plugin_version;
   const char *plugin_description;
} psdInfo;

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
#define sdplug_info(plugin) ((psdInfo *)(plugin->pinfo))

#ifdef __cplusplus
}
#endif

#endif /* __SD_PLUGINS_H */
