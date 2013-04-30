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
 
#ifndef __DIR_PLUGINS_H 
#define __DIR_PLUGINS_H

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

/* Bacula Variable Ids */       /* return value */
typedef enum {
  bDirVarJob       = 1,            // string
  bDirVarLevel     = 2,            // int   
  bDirVarType      = 3,            // int   
  bDirVarJobId     = 4,            // int   
  bDirVarClient    = 5,            // string
  bDirVarNumVols   = 6,            // int   
  bDirVarPool      = 7,            // string
  bDirVarStorage   = 8,            // string
  bDirVarWriteStorage = 9,         // string
  bDirVarReadStorage  = 10,        // string
  bDirVarCatalog   = 11,           // string
  bDirVarMediaType = 12,           // string
  bDirVarJobName   = 13,           // string
  bDirVarJobStatus = 14,           // int   
  bDirVarPriority  = 15,           // int   
  bDirVarVolumeName = 16,          // string
  bDirVarCatalogRes = 17,          // NYI      
  bDirVarJobErrors  = 18,          // int   
  bDirVarJobFiles   = 19,          // int   
  bDirVarSDJobFiles = 20,          // int   
  bDirVarSDErrors   = 21,          // int   
  bDirVarFDJobStatus = 22,         // int   
  bDirVarSDJobStatus = 23          // int   
} brDirVariable;

typedef enum {
  bwDirVarJobReport  = 1,
  bwDirVarVolumeName = 2,
  bwDirVarPriority   = 3,
  bwDirVarJobLevel   = 4
} bwDirVariable;


typedef enum {
  bDirEventJobStart      = 1,
  bDirEventJobEnd        = 2,
  bDirEventJobInit       = 3,
  bDirEventJobRun        = 4,
  bDirEventVolumePurged  = 5,
  bDirEventNewVolume     = 6,
  bDirEventNeedVolume    = 7,
  bDirEventVolumeFull    = 8,
  bDirEventRecyle        = 9,
  bDirEventGetScratch    = 10
} bDirEventType;

typedef struct s_bDirEvent {
   uint32_t eventType;
} bDirEvent;

typedef struct s_dirbaculaInfo {
   uint32_t size;
   uint32_t version;  
} bDirInfo;

/* Bacula interface version and function pointers */
typedef struct s_dirbaculaFuncs {  
   uint32_t size;
   uint32_t version;
   bRC (*registerBaculaEvents)(bpContext *ctx, ...);
   bRC (*getBaculaValue)(bpContext *ctx, brDirVariable var, void *value);
   bRC (*setBaculaValue)(bpContext *ctx, bwDirVariable var, void *value);
   bRC (*JobMessage)(bpContext *ctx, const char *file, int line, 
                     int type, utime_t mtime, const char *fmt, ...);     
   bRC (*DebugMessage)(bpContext *ctx, const char *file, int line,
                       int level, const char *fmt, ...);
} bDirFuncs;

/* Bacula Core Routines -- not used within a plugin */
#ifdef DIRECTOR_DAEMON
void load_dir_plugins(const char *plugin_dir);
void new_plugins(JCR *jcr);
void free_plugins(JCR *jcr);
int generate_plugin_event(JCR *jcr, bDirEventType event, void *value=NULL);
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
#define DIR_PLUGIN_INTERFACE_VERSION  1

typedef struct s_dirpluginInfo {
   uint32_t size;
   uint32_t version;
   const char *plugin_magic;
   const char *plugin_license;
   const char *plugin_author;
   const char *plugin_date;
   const char *plugin_version;
   const char *plugin_description;
} pDirInfo;

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
#define dirplug_info(plugin) ((pDirInfo *)(plugin->pinfo))

#ifdef __cplusplus
}
#endif

#endif /* __FD_PLUGINS_H */
