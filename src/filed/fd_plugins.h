/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2007-2012 Free Software Foundation Europe e.V.

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
 * Application Programming Interface (API) definition for Bacula Plugins
 *
 * Kern Sibbald, October 2007
 *
 */
 
#ifndef __FD_PLUGINS_H 
#define __FD_PLUGINS_H

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
#endif  /* ! _BACULA_H */

#include <sys/types.h>

#if defined(HAVE_WIN32)
#if defined(HAVE_MINGW)
#include "mingwconfig.h"
#else
#include "winconfig.h"
#endif
#else  /* !HAVE_WIN32 */
#ifndef __CONFIG_H
#include "config.h"
#define __CONFIG_H
#endif
#endif

#include "../src/version.h"
#include "bc_types.h"
#include "lib/plugins.h"
#include <sys/stat.h>
#ifdef HAVE_WIN32
#include "../win32/filed/vss.h"
#endif

/*
 * This packet is used for the restore objects
 *  It is passed to the plugin when restoring
 *  the object.
 */
struct restore_object_pkt {
   int32_t pkt_size;                  /* size of this packet */
   char *object_name;                 /* Object name */
   char *object;                      /* restore object data to save */
   char *plugin_name;                 /* Plugin name */
   int32_t object_type;               /* FT_xx for this file */             
   int32_t object_len;                /* restore object length */
   int32_t object_full_len;           /* restore object uncompressed length */
   int32_t object_index;              /* restore object index */
   int32_t object_compression;        /* set to compression type */
   int32_t stream;                    /* attribute stream id */
   uint32_t JobId;                    /* JobId object came from */
   int32_t pkt_end;                   /* end packet sentinel */
};

/*
 * This packet is used for file save info transfer.
*/
struct save_pkt {
   int32_t pkt_size;                  /* size of this packet */
   char *fname;                       /* Full path and filename */
   char *link;                        /* Link name if any */
   struct stat statp;                 /* System stat() packet for file */
   int32_t type;                      /* FT_xx for this file */             
   uint32_t flags;                    /* Bacula internal flags */
   bool no_read;                      /* During the save, the file should not be saved */
   bool portable;                     /* set if data format is portable */
   bool accurate_found;               /* Found in accurate list (valid after check_changes()) */
   char *cmd;                         /* command */
   uint32_t delta_seq;                /* Delta sequence number */
   char *object_name;                 /* Object name to create */
   char *object;                      /* restore object data to save */
   int32_t object_len;                /* restore object length */
   int32_t index;                     /* restore object index */
   int32_t pkt_end;                   /* end packet sentinel */
};

/*
 * This packet is used for file restore info transfer.
*/
struct restore_pkt {
   int32_t pkt_size;                  /* size of this packet */
   int32_t stream;                    /* attribute stream id */
   int32_t data_stream;               /* id of data stream to follow */
   int32_t type;                      /* file type FT */
   int32_t file_index;                /* file index */
   int32_t LinkFI;                    /* file index to data if hard link */
   uid_t uid;                         /* userid */
   struct stat statp;                 /* decoded stat packet */
   const char *attrEx;                /* extended attributes if any */
   const char *ofname;                /* output filename */
   const char *olname;                /* output link name */
   const char *where;                 /* where */
   const char *RegexWhere;            /* regex where */
   int replace;                       /* replace flag */
   int create_status;                 /* status from createFile() */
   uint32_t delta_seq;                /* Delta sequence number */
   int32_t pkt_end;                   /* end packet sentinel */
};

enum {
   IO_OPEN = 1,
   IO_READ = 2,
   IO_WRITE = 3,
   IO_CLOSE = 4,
   IO_SEEK = 5
};

struct io_pkt {
   int32_t pkt_size;                  /* Size of this packet */
   int32_t func;                      /* Function code */
   int32_t count;                     /* read/write count */
   int32_t flags;                     /* Open flags */
   mode_t mode;                       /* permissions for created files */
   char *buf;                         /* read/write buffer */
   const char *fname;                 /* open filename */
   int32_t status;                    /* return status */
   int32_t io_errno;                  /* errno code */  
   int32_t lerror;                    /* Win32 error code */
   int32_t whence;                    /* lseek argument */
   boffset_t offset;                  /* lseek argument */
   bool win32;                        /* Win32 GetLastError returned */
   int32_t pkt_end;                   /* end packet sentinel */
};

/****************************************************************************
 *                                                                          *
 *                Bacula definitions                                        *
 *                                                                          *
 ****************************************************************************/

/* Bacula Variable Ids */
typedef enum {
  bVarJobId     = 1,
  bVarFDName    = 2,
  bVarLevel     = 3,
  bVarType      = 4,
  bVarClient    = 5,
  bVarJobName   = 6,
  bVarJobStatus = 7,
  bVarSinceTime = 8,
  bVarAccurate  = 9,
  bVarFileSeen  = 10,
  bVarVssObject = 11,
  bVarVssDllHandle = 12,
  bVarWorkingDir = 13,
  bVarWhere      = 14,
  bVarRegexWhere = 15,
  bVarExePath    = 16,
  bVarVersion    = 17,
  bVarDistName   = 18,
  bVarBEEF       = 19,
  bVarPrevJobName = 20,
  bVarPrefixLinks = 21
} bVariable;

/* Events that are passed to plugin */
typedef enum {
  bEventJobStart                        = 1,
  bEventJobEnd                          = 2,
  bEventStartBackupJob                  = 3,
  bEventEndBackupJob                    = 4,
  bEventStartRestoreJob                 = 5,
  bEventEndRestoreJob                   = 6,
  bEventStartVerifyJob                  = 7,
  bEventEndVerifyJob                    = 8,
  bEventBackupCommand                   = 9,
  bEventRestoreCommand                  = 10,
  bEventEstimateCommand                 = 11,
  bEventLevel                           = 12,
  bEventSince                           = 13,
  bEventCancelCommand                   = 14, /* Executed by another thread */
  bEventVssBackupAddComponents          = 15, /* Just before bEventVssPrepareSnapshot */
  bEventVssRestoreLoadComponentMetadata = 16,
  bEventVssRestoreSetComponentsSelected = 17,
  bEventRestoreObject                   = 18,
  bEventEndFileSet                      = 19,
  bEventPluginCommand                   = 20, /* Sent during FileSet creation */
  bEventVssBeforeCloseRestore           = 21,
  /* Add drives to VSS snapshot 
   *  argument: char[27] drivelist
   * You need to add them without duplicates, 
   * see fd_common.h add_drive() copy_drives() to get help
   */
  bEventVssPrepareSnapshot              = 22,
  bEventOptionPlugin                    = 23,
  bEventHandleBackupFile                = 24, /* Used with Options Plugin */
  bEventComponentInfo                   = 25  /* Plugin component */
} bEventType;

typedef struct s_bEvent {
   uint32_t eventType;
} bEvent;

typedef struct s_baculaInfo {
   uint32_t size;
   uint32_t version;
} bInfo;

/* Bacula Core Routines -- not used within a plugin */
#ifdef FILE_DAEMON
struct BFILE;                   /* forward referenced */
struct FF_PKT;
void load_fd_plugins(const char *plugin_dir);
void new_plugins(JCR *jcr);
void free_plugins(JCR *jcr);
void generate_plugin_event(JCR *jcr, bEventType event, void *value=NULL);
bool send_plugin_name(JCR *jcr, BSOCK *sd, bool start);
bool plugin_name_stream(JCR *jcr, char *name);    
int plugin_create_file(JCR *jcr, ATTR *attr, BFILE *bfd, int replace);
bool plugin_set_attributes(JCR *jcr, ATTR *attr, BFILE *ofd);
int plugin_save(JCR *jcr, FF_PKT *ff_pkt, bool top_level);
int plugin_estimate(JCR *jcr, FF_PKT *ff_pkt, bool top_level);
bool plugin_check_file(JCR *jcr, char *fname);
bRC plugin_option_handle_file(JCR *jcr, FF_PKT *ff_pkt, struct save_pkt *sp);
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* 
 * Bacula interface version and function pointers -- 
 *  i.e. callbacks from the plugin to Bacula
 */
typedef struct s_baculaFuncs {  
   uint32_t size;
   uint32_t version;
   bRC (*registerBaculaEvents)(bpContext *ctx, ...);
   bRC (*getBaculaValue)(bpContext *ctx, bVariable var, void *value);
   bRC (*setBaculaValue)(bpContext *ctx, bVariable var, void *value);
   bRC (*JobMessage)(bpContext *ctx, const char *file, int line, 
       int type, utime_t mtime, const char *fmt, ...);     
   bRC (*DebugMessage)(bpContext *ctx, const char *file, int line,
       int level, const char *fmt, ...);
   void *(*baculaMalloc)(bpContext *ctx, const char *file, int line, 
       size_t size);
   void (*baculaFree)(bpContext *ctx, const char *file, int line, void *mem);
   bRC (*AddExclude)(bpContext *ctx, const char *file);
   bRC (*AddInclude)(bpContext *ctx, const char *file);
   bRC (*AddOptions)(bpContext *ctx, const char *opts);
   bRC (*AddRegex)(bpContext *ctx, const char *item, int type);
   bRC (*AddWild)(bpContext *ctx, const char *item, int type);
   bRC (*NewOptions)(bpContext *ctx);
   bRC (*NewInclude)(bpContext *ctx);
   bRC (*NewPreInclude)(bpContext *ctx);
   bRC (*checkChanges)(bpContext *ctx, struct save_pkt *sp);
   bRC (*AcceptFile)(bpContext *ctx, struct save_pkt *sp); /* Need fname and statp */
} bFuncs;




/****************************************************************************
 *                                                                          *
 *                Plugin definitions                                        *
 *                                                                          *
 ****************************************************************************/

typedef enum {
  pVarName = 1,
  pVarDescription = 2
} pVariable;

# define FD_PLUGIN_MAGIC  "*FDPluginData*" 

#define FD_PLUGIN_INTERFACE_VERSION  7

typedef struct s_pluginInfo {
   uint32_t size;
   uint32_t version;
   const char *plugin_magic;
   const char *plugin_license;
   const char *plugin_author;
   const char *plugin_date;
   const char *plugin_version;
   const char *plugin_description;
} pInfo;

/*
 * This is a set of function pointers that Bacula can call
 *  within the plugin.
 */
typedef struct s_pluginFuncs {  
   uint32_t size;
   uint32_t version;
   bRC (*newPlugin)(bpContext *ctx);
   bRC (*freePlugin)(bpContext *ctx);
   bRC (*getPluginValue)(bpContext *ctx, pVariable var, void *value);
   bRC (*setPluginValue)(bpContext *ctx, pVariable var, void *value);
   bRC (*handlePluginEvent)(bpContext *ctx, bEvent *event, void *value);
   bRC (*startBackupFile)(bpContext *ctx, struct save_pkt *sp);
   bRC (*endBackupFile)(bpContext *ctx);
   bRC (*startRestoreFile)(bpContext *ctx, const char *cmd);
   bRC (*endRestoreFile)(bpContext *ctx);
   bRC (*pluginIO)(bpContext *ctx, struct io_pkt *io);
   bRC (*createFile)(bpContext *ctx, struct restore_pkt *rp);
   bRC (*setFileAttributes)(bpContext *ctx, struct restore_pkt *rp);
   bRC (*checkFile)(bpContext *ctx, char *fname);
} pFuncs;

#define plug_func(plugin) ((pFuncs *)(plugin->pfuncs))
#define plug_info(plugin) ((pInfo *)(plugin->pinfo))

#ifdef __cplusplus
}
#endif

#endif /* __FD_PLUGINS_H */
