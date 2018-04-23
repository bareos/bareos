/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2007-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2016 Bareos GmbH & Co. KG

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
 * Application Programming Interface (API) definition for Bareos Plugins
 *
 * Kern Sibbald, October 2007
 */

#ifndef BAREOS_FILED_FD_PLUGINS_H_
#define BAREOS_FILED_FD_PLUGINS_H_

#ifndef BAREOS_INCLUDE_BAREOS_H_
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
#endif  /* ! BAREOS_INCLUDE_BAREOS_H_ */

#include <sys/types.h>

#include "hostconfig.h"

#include "version.h"
#include "bc_types.h"
#include "fileopts.h"
#include "lib/plugins.h"
#include <sys/stat.h>

#ifdef HAVE_WIN32
#include "vss.h"
#endif

/*
 * This packet is used for the restore objects
 *  It is passed to the plugin when restoring
 *  the object.
 */
struct restore_object_pkt {
   int32_t pkt_size;                  /* Size of this packet */
   char *object_name;                 /* Object name */
   char *object;                      /* Restore object data to save */
   char *plugin_name;                 /* Plugin name */
   int32_t object_type;               /* FT_xx for this file */
   int32_t object_len;                /* Restore object length */
   int32_t object_full_len;           /* Restore object uncompressed length */
   int32_t object_index;              /* Restore object index */
   int32_t object_compression;        /* Set to compression type */
   int32_t stream;                    /* Attribute stream id */
   uint32_t JobId;                    /* JobId object came from */
   int32_t pkt_end;                   /* End packet sentinel */
};

/*
 * This packet is used for file save info transfer.
 */
struct save_pkt {
   int32_t pkt_size;                  /* Size of this packet */
   char *fname;                       /* Full path and filename */
   char *link;                        /* Link name if any */
   struct stat statp;                 /* System stat() packet for file */
   int32_t type;                      /* FT_xx for this file */
   char flags[FOPTS_BYTES];           /* Bareos internal flags */
   bool no_read;                      /* During the save, the file should not be saved */
   bool portable;                     /* Set if data format is portable */
   bool accurate_found;               /* Found in accurate list (valid after check_changes()) */
   char *cmd;                         /* Command */
   time_t save_time;                  /* Start of incremental time */
   uint32_t delta_seq;                /* Delta sequence number */
   char *object_name;                 /* Object name to create */
   char *object;                      /* Restore object data to save */
   int32_t object_len;                /* Restore object length */
   int32_t index;                     /* Restore object index */
   int32_t pkt_end;                   /* End packet sentinel */
};

/*
 * This packet is used for file restore info transfer.
 */
struct restore_pkt {
   int32_t pkt_size;                  /* Size of this packet */
   int32_t stream;                    /* Attribute stream id */
   int32_t data_stream;               /* Id of data stream to follow */
   int32_t type;                      /* File type FT */
   int32_t file_index;                /* File index */
   int32_t LinkFI;                    /* File index to data if hard link */
   uid_t uid;                         /* Userid */
   struct stat statp;                 /* Decoded stat packet */
   const char *attrEx;                /* Extended attributes if any */
   const char *ofname;                /* Output filename */
   const char *olname;                /* Output link name */
   const char *where;                 /* Where */
   const char *RegexWhere;            /* Regex where */
   int replace;                       /* Replace flag */
   int create_status;                 /* Status from createFile() */
   uint32_t delta_seq;                /* Delta sequence number */
   int32_t pkt_end;                   /* End packet sentinel */
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
   int32_t count;                     /* Read/write count */
   int32_t flags;                     /* Open flags */
   mode_t mode;                       /* Permissions for created files */
   char *buf;                         /* Read/write buffer */
   const char *fname;                 /* Open filename */
   int32_t status;                    /* Return status */
   int32_t io_errno;                  /* Errno code */
   int32_t lerror;                    /* Win32 error code */
   int32_t whence;                    /* Lseek argument */
   boffset_t offset;                  /* Lseek argument */
   bool win32;                        /* Win32 GetLastError returned */
   int32_t pkt_end;                   /* End packet sentinel */
};

struct acl_pkt {
   int32_t pkt_size;                  /* Size of this packet */
   const char *fname;                 /* Full path and filename */
   uint32_t content_length;           /* ACL content length */
   char *content;                     /* ACL content */
   int32_t pkt_end;                   /* End packet sentinel */
};

struct xattr_pkt {
   int32_t pkt_size;                  /* Size of this packet */
   const char *fname;                 /* Full path and filename */
   uint32_t name_length;              /* XATTR name length */
   char *name;                        /* XATTR name */
   uint32_t value_length;             /* XATTR value length */
   char *value;                       /* XATTR value */
   int32_t pkt_end;                   /* End packet sentinel */
};

/****************************************************************************
 *                                                                          *
 *                Bareos definitions                                        *
 *                                                                          *
 ****************************************************************************/

/*
 * Bareos Variable Ids
 */
typedef enum {
   bVarJobId = 1,
   bVarFDName = 2,
   bVarLevel = 3,
   bVarType = 4,
   bVarClient = 5,
   bVarJobName = 6,
   bVarJobStatus = 7,
   bVarSinceTime = 8,
   bVarAccurate = 9,
   bVarFileSeen = 10,
   bVarVssClient = 11,
   bVarWorkingDir = 12,
   bVarWhere = 13,
   bVarRegexWhere = 14,
   bVarExePath = 15,
   bVarVersion = 16,
   bVarDistName = 17,
   bVarPrevJobName = 18,
   bVarPrefixLinks = 19
} bVariable;

/*
 * Events that are passed to plugin
 */
typedef enum {
   bEventJobStart = 1,
   bEventJobEnd = 2,
   bEventStartBackupJob = 3,
   bEventEndBackupJob = 4,
   bEventStartRestoreJob = 5,
   bEventEndRestoreJob = 6,
   bEventStartVerifyJob = 7,
   bEventEndVerifyJob = 8,
   bEventBackupCommand = 9,
   bEventRestoreCommand = 10,
   bEventEstimateCommand = 11,
   bEventLevel = 12,
   bEventSince = 13,
   bEventCancelCommand = 14,
   bEventRestoreObject = 15,
   bEventEndFileSet = 16,
   bEventPluginCommand = 17,
   bEventOptionPlugin = 18,
   bEventHandleBackupFile = 19,
   bEventNewPluginOptions = 20,
   bEventVssInitializeForBackup = 21,
   bEventVssInitializeForRestore = 22,
   bEventVssSetBackupState = 23,
   bEventVssPrepareForBackup = 24,
   bEventVssBackupAddComponents = 25,
   bEventVssPrepareSnapshot = 26,
   bEventVssCreateSnapshots = 27,
   bEventVssRestoreLoadComponentMetadata = 28,
   bEventVssRestoreSetComponentsSelected = 29,
   bEventVssCloseRestore = 30,
   bEventVssBackupComplete = 31
} bEventType;

#define FD_NR_EVENTS bEventVssBackupComplete /* keep this updated ! */

typedef struct s_bEvent {
   uint32_t eventType;
} bEvent;

typedef struct s_bareosInfo {
   uint32_t size;
   uint32_t version;
} bInfo;

/*
 * Bareos Core Routines -- not used within a plugin
 */
#ifdef FILE_DAEMON
struct BareosWinFilePacket;                   /* forward referenced */
struct FindFilesPacket;
void load_fd_plugins(const char *plugin_dir, alist *plugin_names);
void unload_fd_plugins(void);
int list_fd_plugins(PoolMem &msg);
void new_plugins(JobControlRecord *jcr);
void free_plugins(JobControlRecord *jcr);
bRC generate_plugin_event(JobControlRecord *jcr, bEventType event,
                          void *value = NULL, bool reverse = false);
bool send_plugin_name(JobControlRecord *jcr, BareosSocket *sd, bool start);
bool plugin_name_stream(JobControlRecord *jcr, char *name);
int plugin_create_file(JobControlRecord *jcr, Attributes *attr, BareosWinFilePacket *bfd, int replace);
bool plugin_set_attributes(JobControlRecord *jcr, Attributes *attr, BareosWinFilePacket *ofd);
bacl_exit_code plugin_build_acl_streams(JobControlRecord *jcr, acl_data_t *acl_data, FindFilesPacket *ff_pkt);
bacl_exit_code plugin_parse_acl_streams(JobControlRecord *jcr, acl_data_t *acl_data, int stream,
                                        char *content, uint32_t content_length);
bxattr_exit_code plugin_build_xattr_streams(JobControlRecord *jcr, struct xattr_data_t *xattr_data,
                                            FindFilesPacket *ff_pkt);
bxattr_exit_code plugin_parse_xattr_streams(JobControlRecord *jcr, struct xattr_data_t *xattr_data,
                                            int stream, char *content, uint32_t content_length);
int plugin_save(JobControlRecord *jcr, FindFilesPacket *ff_pkt, bool top_level);
int plugin_estimate(JobControlRecord *jcr, FindFilesPacket *ff_pkt, bool top_level);
bool plugin_check_file(JobControlRecord *jcr, char *fname);
void plugin_update_ff_pkt(FindFilesPacket *ff_pkt, struct save_pkt *sp);
bRC plugin_option_handle_file(JobControlRecord *jcr, FindFilesPacket *ff_pkt, struct save_pkt *sp);
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Bareos interface version and function pointers --
 *  i.e. callbacks from the plugin to Bareos
 */
typedef struct s_bareosFuncs {
   uint32_t size;
   uint32_t version;
   bRC (*registerBareosEvents)(bpContext *ctx, int nr_events, ...);
   bRC (*unregisterBareosEvents)(bpContext *ctx, int nr_events, ...);
   bRC (*getInstanceCount)(bpContext *ctx, int *ret);
   bRC (*getBareosValue)(bpContext *ctx, bVariable var, void *value);
   bRC (*setBareosValue)(bpContext *ctx, bVariable var, void *value);
   bRC (*JobMessage)(bpContext *ctx, const char *file, int line, int type,
                     utime_t mtime, const char *fmt, ...);
   bRC (*DebugMessage)(bpContext *ctx, const char *file, int line, int level,
                       const char *fmt, ...);
   void *(*bareosMalloc)(bpContext *ctx, const char *file, int line, size_t size);
   void (*bareosFree)(bpContext *ctx, const char *file, int line, void *mem);
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
   bRC (*SetSeenBitmap)(bpContext *ctx, bool all, char *fname);
   bRC (*ClearSeenBitmap)(bpContext *ctx, bool all, char *fname);
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

#define FD_PLUGIN_MAGIC  "*FDPluginData*"
#define FD_PLUGIN_INTERFACE_VERSION 10

/*
 * This is a set of function pointers that Bareos can call within the plugin.
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
   bRC (*getAcl)(bpContext *ctx, struct acl_pkt *ap);
   bRC (*setAcl)(bpContext *ctx, struct acl_pkt *ap);
   bRC (*getXattr)(bpContext *ctx, struct xattr_pkt *xp);
   bRC (*setXattr)(bpContext *ctx, struct xattr_pkt *xp);
} pFuncs;

#define plug_func(plugin) ((pFuncs *)(plugin->pfuncs))
#define plug_info(plugin) ((genpInfo *)(plugin->pinfo))

#ifdef __cplusplus
}
#endif

#endif /* BAREOS_FILED_FD_PLUGINS_H_ */
