/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2014-2015 Planets Communications B.V.
   Copyright (C) 2014-2015 Bareos GmbH & Co. KG

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
/**
 * @file
 * CEPHFS plugin for the Bareos File Daemon
 */
#include "include/bareos.h"
#include "filed/fd_plugins.h"
#include "fd_common.h"
#include "include/fileopts.h"
#include "lib/alist.h"
#include "lib/berrno.h"
#include "lib/path_list.h"

#include <dirent.h>
#include <cephfs/libcephfs.h>

namespace filedaemon {

static const int debuglevel = 150;

#define PLUGIN_LICENSE "Bareos AGPLv3"
#define PLUGIN_AUTHOR "Marco van Wieringen"
#define PLUGIN_DATE "February 2015"
#define PLUGIN_VERSION "1"
#define PLUGIN_DESCRIPTION "Bareos CEPHFS File Daemon Plugin"
#define PLUGIN_USAGE "cephfs:conffile=<ceph_conf_file>:basedir=<basedir>:"

#define CEPHFS_PATH_MAX 4096

/**
 * Forward referenced functions
 */
static bRC newPlugin(bpContext* ctx);
static bRC freePlugin(bpContext* ctx);
static bRC getPluginValue(bpContext* ctx, pVariable var, void* value);
static bRC setPluginValue(bpContext* ctx, pVariable var, void* value);
static bRC handlePluginEvent(bpContext* ctx, bEvent* event, void* value);
static bRC startBackupFile(bpContext* ctx, struct save_pkt* sp);
static bRC endBackupFile(bpContext* ctx);
static bRC pluginIO(bpContext* ctx, struct io_pkt* io);
static bRC startRestoreFile(bpContext* ctx, const char* cmd);
static bRC endRestoreFile(bpContext* ctx);
static bRC createFile(bpContext* ctx, struct restore_pkt* rp);
static bRC setFileAttributes(bpContext* ctx, struct restore_pkt* rp);
static bRC checkFile(bpContext* ctx, char* fname);
static bRC getAcl(bpContext* ctx, acl_pkt* ap);
static bRC setAcl(bpContext* ctx, acl_pkt* ap);
static bRC getXattr(bpContext* ctx, xattr_pkt* xp);
static bRC setXattr(bpContext* ctx, xattr_pkt* xp);

static bRC parse_plugin_definition(bpContext* ctx, void* value);
static bRC setup_backup(bpContext* ctx, void* value);
static bRC setup_restore(bpContext* ctx, void* value);
static bRC end_restore_job(bpContext* ctx, void* value);

/**
 * Pointers to Bareos functions
 */
static bFuncs* bfuncs = NULL;
static bInfo* binfo = NULL;

/**
 * Plugin Information block
 */
static genpInfo pluginInfo = {sizeof(pluginInfo), FD_PLUGIN_INTERFACE_VERSION,
                              FD_PLUGIN_MAGIC,    PLUGIN_LICENSE,
                              PLUGIN_AUTHOR,      PLUGIN_DATE,
                              PLUGIN_VERSION,     PLUGIN_DESCRIPTION,
                              PLUGIN_USAGE};

/**
 * Plugin entry points for Bareos
 */
static pFuncs pluginFuncs = {
    sizeof(pluginFuncs), FD_PLUGIN_INTERFACE_VERSION,

    /* Entry points into plugin */
    newPlugin,  /* new plugin instance */
    freePlugin, /* free plugin instance */
    getPluginValue, setPluginValue, handlePluginEvent, startBackupFile,
    endBackupFile, startRestoreFile, endRestoreFile, pluginIO, createFile,
    setFileAttributes, checkFile, getAcl, setAcl, getXattr, setXattr};

/**
 * Plugin private context
 */
struct plugin_ctx {
  int32_t backup_level;    /* Backup level e.g. Full/Differential/Incremental */
  utime_t since;           /* Since time for Differential/Incremental */
  char* plugin_options;    /* Options passed to plugin */
  char* plugin_definition; /* Previous plugin definition passed to plugin */
  char* conffile; /* Configfile to read to be able to connect to CEPHFS */
  char* basedir;  /* Basedir to start backup in */
  char flags[FOPTS_BYTES]; /* Bareos internal flags */
  int32_t type;            /* FT_xx for this file */
#if HAVE_CEPHFS_CEPH_STATX_H
  struct ceph_statx statx; /* Stat struct for next file to save */
#else
  struct stat statp; /* Stat struct for next file to save */
#endif
  bool processing_xattr;  /* Set when we are processing a xattr list */
  char* next_xattr_name;  /* Next xattr name to process */
  POOLMEM* cwd;           /* Current Working Directory */
  POOLMEM* next_filename; /* Next filename to save */
  POOLMEM* link_target;   /* Target symlink points to */
  POOLMEM* xattr_list;    /* List of xattrs */
  alist* dir_stack;       /* Stack of directories when recursing */
  htable* path_list;      /* Hash table with directories created on restore. */
  struct dirent de;       /* Current directory entry being processed. */
  struct ceph_mount_info* cmount; /* CEPHFS mountpoint */
  struct ceph_dir_result* cdir;   /* CEPHFS directory handle */
  int cfd;                        /* CEPHFS file handle */
};

/**
 * This defines the arguments that the plugin parser understands.
 */
enum plugin_argument_type
{
  argument_none,
  argument_conffile,
  argument_basedir
};

struct plugin_argument {
  const char* name;
  enum plugin_argument_type type;
};

static plugin_argument plugin_arguments[] = {
    /* configfile: deprecated, use conffile instead (same as the other plugins
       and backends) */
    {"configfile", argument_conffile},
    {"conffile", argument_conffile},
    {"basedir", argument_basedir},
    {NULL, argument_none}};

/**
 * If we recurse into a subdir we push the current directory onto
 * a stack so we can pop it after we have processed the subdir.
 */
struct dir_stack_entry {
#if HAVE_CEPHFS_CEPH_STATX_H
  struct ceph_statx statx; /* Stat struct of directory */
#else
  struct stat statp; /* Stat struct of directory */
#endif
  struct ceph_dir_result* cdir; /* CEPHFS directory handle */
};

#ifdef __cplusplus
extern "C" {
#endif

/**
 * loadPlugin() and unloadPlugin() are entry points that are exported, so Bareos
 * can directly call these two entry points they are common to all Bareos
 * plugins.
 *
 * External entry point called by Bareos to "load" the plugin
 */
bRC loadPlugin(bInfo* lbinfo,
               bFuncs* lbfuncs,
               genpInfo** pinfo,
               pFuncs** pfuncs)
{
  bfuncs = lbfuncs; /* set Bareos funct pointers */
  binfo = lbinfo;
  *pinfo = &pluginInfo;   /* return pointer to our info */
  *pfuncs = &pluginFuncs; /* return pointer to our functions */

  return bRC_OK;
}

/**
 * External entry point to unload the plugin
 */
bRC unloadPlugin() { return bRC_OK; }

#ifdef __cplusplus
}
#endif

/**
 * The following entry points are accessed through the function pointers we
 * supplied to Bareos. Each plugin type (dir, fd, sd) has its own set of entry
 * points that the plugin must define.
 *
 * Create a new instance of the plugin i.e. allocate our private storage
 */
static bRC newPlugin(bpContext* ctx)
{
  plugin_ctx* p_ctx;

  p_ctx = (plugin_ctx*)malloc(sizeof(plugin_ctx));
  if (!p_ctx) { return bRC_Error; }
  memset(p_ctx, 0, sizeof(plugin_ctx));
  ctx->pContext = (void*)p_ctx; /* set our context pointer */

  /*
   * Allocate some internal memory for:
   * - The current working dir.
   * - The file we are processing
   * - The link target of a symbolic link.
   * - The list of xattrs.
   */
  p_ctx->cwd = GetPoolMemory(PM_FNAME);
  p_ctx->next_filename = GetPoolMemory(PM_FNAME);
  p_ctx->link_target = GetPoolMemory(PM_FNAME);
  p_ctx->xattr_list = GetPoolMemory(PM_MESSAGE);

  /*
   * Resize all buffers for PATH like names to CEPHFS_PATH_MAX.
   */
  p_ctx->cwd = CheckPoolMemorySize(p_ctx->cwd, CEPHFS_PATH_MAX);
  p_ctx->next_filename =
      CheckPoolMemorySize(p_ctx->next_filename, CEPHFS_PATH_MAX);
  p_ctx->link_target = CheckPoolMemorySize(p_ctx->link_target, CEPHFS_PATH_MAX);

  /*
   * This is a alist that holds the stack of directories we have open.
   * We push the current directory onto this stack the moment we start
   * processing a sub directory and pop it from this list when we are
   * done processing that sub directory.
   */
  p_ctx->dir_stack = new alist(10, owned_by_alist);

  /*
   * Only register the events we are really interested in.
   */
  bfuncs->registerBareosEvents(ctx, 7, bEventLevel, bEventSince,
                               bEventRestoreCommand, bEventBackupCommand,
                               bEventPluginCommand, bEventEndRestoreJob,
                               bEventNewPluginOptions);

  return bRC_OK;
}

/**
 * Free a plugin instance, i.e. release our private storage
 */
static bRC freePlugin(bpContext* ctx)
{
  plugin_ctx* p_ctx = (plugin_ctx*)ctx->pContext;
  if (!p_ctx) { return bRC_Error; }

  Dmsg(ctx, debuglevel, "cephfs-fd: entering freePlugin\n");

  if (p_ctx->path_list) {
    FreePathList(p_ctx->path_list);
    p_ctx->path_list = NULL;
  }

  if (p_ctx->dir_stack) {
    p_ctx->dir_stack->destroy();
    delete p_ctx->dir_stack;
  }

  if (p_ctx->cmount) {
    ceph_shutdown(p_ctx->cmount);
    p_ctx->cmount = NULL;
  }

  FreePoolMemory(p_ctx->xattr_list);
  FreePoolMemory(p_ctx->link_target);
  FreePoolMemory(p_ctx->next_filename);
  FreePoolMemory(p_ctx->cwd);

  if (p_ctx->basedir) { free(p_ctx->basedir); }

  if (p_ctx->conffile) { free(p_ctx->conffile); }

  if (p_ctx->plugin_definition) { free(p_ctx->plugin_definition); }

  if (p_ctx->plugin_options) { free(p_ctx->plugin_options); }

  free(p_ctx);
  p_ctx = NULL;

  Dmsg(ctx, debuglevel, "cephfs-fd: leaving freePlugin\n");

  return bRC_OK;
}

/**
 * Return some plugin value (none defined)
 */
static bRC getPluginValue(bpContext* ctx, pVariable var, void* value)
{
  return bRC_OK;
}

/**
 * Set a plugin value (none defined)
 */
static bRC setPluginValue(bpContext* ctx, pVariable var, void* value)
{
  return bRC_OK;
}

/**
 * Handle an event that was generated in Bareos
 */
static bRC handlePluginEvent(bpContext* ctx, bEvent* event, void* value)
{
  bRC retval;
  plugin_ctx* p_ctx = (plugin_ctx*)ctx->pContext;

  if (!p_ctx) { return bRC_Error; }

  switch (event->eventType) {
    case bEventLevel:
      p_ctx->backup_level = (int64_t)value;
      retval = bRC_OK;
      break;
    case bEventSince:
      p_ctx->since = (int64_t)value;
      retval = bRC_OK;
      break;
    case bEventRestoreCommand:
      retval = parse_plugin_definition(ctx, value);
      if (retval == bRC_OK) { retval = setup_restore(ctx, value); }
      break;
    case bEventBackupCommand:
      retval = parse_plugin_definition(ctx, value);
      if (retval == bRC_OK) { retval = setup_backup(ctx, value); }
      break;
    case bEventPluginCommand:
      retval = parse_plugin_definition(ctx, value);
      break;
    case bEventNewPluginOptions:
      /*
       * Free any previous value.
       */
      if (p_ctx->plugin_options) {
        free(p_ctx->plugin_options);
        p_ctx->plugin_options = NULL;
      }

      retval = parse_plugin_definition(ctx, value);

      /*
       * Save that we got a plugin override.
       */
      p_ctx->plugin_options = strdup((char*)value);
      break;
    case bEventEndRestoreJob:
      retval = end_restore_job(ctx, value);
      break;
    default:
      Jmsg(ctx, M_FATAL, "cephfs-fd: unknown event=%d\n", event->eventType);
      Dmsg(ctx, debuglevel, "cephfs-fd: unknown event=%d\n", event->eventType);
      retval = bRC_Error;
      break;
  }

  return retval;
}

/**
 * Get the next file to backup.
 */
static bRC get_next_file_to_backup(bpContext* ctx)
{
  int status;
  struct save_pkt sp;
  struct dirent* entry;
  plugin_ctx* p_ctx = (plugin_ctx*)ctx->pContext;

  /*
   * See if we just saved the directory then we are done processing this
   * directory.
   */
  switch (p_ctx->type) {
    case FT_DIREND:
      /*
       * See if there is anything on the dir stack to pop off and continue
       * reading that directory.
       */
      if (!p_ctx->dir_stack->empty()) {
        const char* cwd;
        struct dir_stack_entry* entry;

        /*
         * Change the GLFS cwd back one dir.
         */
        status = ceph_chdir(p_ctx->cmount, "..");
        if (status < 0) {
          BErrNo be;

          Jmsg(ctx, M_ERROR, "cephfs-fd: ceph_chdir(%s) failed: %s\n", "..",
               be.bstrerror(-status));
          return bRC_Error;
        }

        /*
         * Save where we are in the tree.
         */
        cwd = ceph_getcwd(p_ctx->cmount);
        PmStrcpy(p_ctx->cwd, cwd);

        /*
         * Pop the previous directory handle and continue processing that.
         */
        entry = (struct dir_stack_entry*)p_ctx->dir_stack->pop();
#if HAVE_CEPHFS_CEPH_STATX_H
        memcpy(&p_ctx->statx, &entry->statx, sizeof(p_ctx->statx));
#else
        memcpy(&p_ctx->statp, &entry->statp, sizeof(p_ctx->statp));
#endif
        p_ctx->cdir = entry->cdir;
        free(entry);
      } else {
        return bRC_OK;
      }
      break;
    default:
      break;
  }

  if (!p_ctx->cdir) { return bRC_Error; }

  /*
   * Loop until we know what file is next or when we are done.
   */
  while (1) {
#if HAVE_CEPHFS_CEPH_STATX_H
    unsigned int stmask = 0;
    memset(&p_ctx->statx, 0, sizeof(p_ctx->statx));
    status =
        ceph_readdirplus_r(p_ctx->cmount, p_ctx->cdir, &p_ctx->de,
                           &p_ctx->statx, stmask, CEPH_STATX_ALL_STATS, NULL);
#else
    int stmask = 0;
    memset(&p_ctx->statp, 0, sizeof(p_ctx->statp));
    status = ceph_readdirplus_r(p_ctx->cmount, p_ctx->cdir, &p_ctx->de,
                                &p_ctx->statp, &stmask);
#endif

    memset(&p_ctx->de, 0, sizeof(p_ctx->de));

    /*
     * No more entries in this directory ?
     */
    if (status == 0) {
#if HAVE_CEPHFS_CEPH_STATX_H
      status = ceph_statx(p_ctx->cmount, p_ctx->cwd, &p_ctx->statx,
                          CEPH_STATX_MODE, 0);
#else
      status = ceph_stat(p_ctx->cmount, p_ctx->cwd, &p_ctx->statp);
#endif
      if (status < 0) {
        BErrNo be;

        Jmsg(ctx, M_ERROR, "cephfs-fd: ceph_stat(%s) failed: %s\n", p_ctx->cwd,
             be.bstrerror(-status));
        return bRC_Error;
      }

      status = ceph_closedir(p_ctx->cmount, p_ctx->cdir);
      if (status < 0) {
        BErrNo be;

        Jmsg(ctx, M_ERROR, "cephfs-fd: ceph_closedir(%s) failed: %s\n",
             p_ctx->cwd, be.bstrerror(-status));
        return bRC_Error;
      }

      p_ctx->cdir = NULL;
      p_ctx->type = FT_DIREND;

      PmStrcpy(p_ctx->next_filename, p_ctx->cwd);

      Dmsg(ctx, debuglevel, "cephfs-fd: next file to backup %s\n",
           p_ctx->next_filename);

      return bRC_More;
    }

    entry = &p_ctx->de;

    /*
     * Skip `.', `..', and excluded file names.
     */
    if (entry->d_name[0] == '\0' ||
        (entry->d_name[0] == '.' &&
         (entry->d_name[1] == '\0' ||
          (entry->d_name[1] == '.' && entry->d_name[2] == '\0')))) {
      continue;
    }

    Mmsg(p_ctx->next_filename, "%s/%s", p_ctx->cwd, entry->d_name);

    /*
     * Determine the FileType.
     */
#if HAVE_CEPHFS_CEPH_STATX_H
    switch (p_ctx->statx.stx_mode & S_IFMT) {
#else
    switch (p_ctx->statp.st_mode & S_IFMT) {
#endif
      case S_IFREG:
        p_ctx->type = FT_REG;
        break;
      case S_IFLNK:
        p_ctx->type = FT_LNK;
        status = ceph_readlink(p_ctx->cmount, p_ctx->next_filename,
                               p_ctx->link_target,
                               SizeofPoolMemory(p_ctx->link_target));
        if (status < 0) {
          BErrNo be;

          Jmsg(ctx, M_ERROR, "cephfs-fd: ceph_readlink(%s) failed: %s\n",
               p_ctx->next_filename, be.bstrerror(-status));
          p_ctx->type = FT_NOFOLLOW;
        }
        p_ctx->link_target[status] = '\0';
        break;
      case S_IFDIR:
        p_ctx->type = FT_DIRBEGIN;
        break;
      case S_IFCHR:
      case S_IFBLK:
      case S_IFIFO:
#ifdef S_IFSOCK
      case S_IFSOCK:
#endif
        p_ctx->type = FT_SPEC;
        break;
      default:
#if HAVE_CEPHFS_CEPH_STATX_H
        Jmsg(ctx, M_FATAL,
             "cephfs-fd: Unknown filetype encountered %ld for %s\n",
             p_ctx->statx.stx_mode & S_IFMT, p_ctx->next_filename);
#else
        Jmsg(ctx, M_FATAL,
             "cephfs-fd: Unknown filetype encountered %ld for %s\n",
             p_ctx->statp.st_mode & S_IFMT, p_ctx->next_filename);
#endif
        return bRC_Error;
    }

    /*
     * See if we accept this file under the currently loaded fileset.
     */
    memset(&sp, 0, sizeof(sp));
    sp.pkt_size = sizeof(sp);
    sp.pkt_end = sizeof(sp);
    sp.fname = p_ctx->next_filename;
    sp.type = p_ctx->type;
#if HAVE_CEPHFS_CEPH_STATX_H
    memcpy(&sp.statp, &p_ctx->statx, sizeof(sp.statp));
#else
    memcpy(&sp.statp, &p_ctx->statp, sizeof(sp.statp));
#endif
    if (bfuncs->AcceptFile(ctx, &sp) == bRC_Skip) {
      Dmsg(ctx, debuglevel,
           "cephfs-fd: file %s skipped due to current fileset settings\n",
           p_ctx->next_filename);
      continue;
    }

    /*
     * If we made it here we have the next file to backup.
     */
    break;
  }

  Dmsg(ctx, debuglevel, "cephfs-fd: next file to backup %s\n",
       p_ctx->next_filename);

  return bRC_More;
}

/**
 * Start the backup of a specific file
 */
static bRC startBackupFile(bpContext* ctx, struct save_pkt* sp)
{
  int status;
  plugin_ctx* p_ctx = (plugin_ctx*)ctx->pContext;

  /*
   * Save the current flags used to save the next file.
   */
  CopyBits(FO_MAX, sp->flags, p_ctx->flags);

  /*
   * See if we start processing a new directory if so open it so the can recurse
   * into it if wanted.
   */
  switch (p_ctx->type) {
    case FT_DIRBEGIN:
      /*
       * See if we are recursing if so we open the directory and process it.
       * We also open the directory when it the toplevel e.g. when p_ctx->cdir
       * == NULL.
       */
      if (!p_ctx->cdir || !BitIsSet(FO_NO_RECURSION, p_ctx->flags)) {
        /*
         * Change into the directory and process all entries in it.
         */
        status = ceph_chdir(p_ctx->cmount, p_ctx->next_filename);
        if (status < 0) {
          BErrNo be;

          Jmsg(ctx, M_ERROR, "cephfs-fd: ceph_chdir(%s) failed: %s\n",
               p_ctx->next_filename, be.bstrerror(-status));
          p_ctx->type = FT_NOOPEN;
        } else {
          /*
           * Push the current directory onto the directory stack so we can
           * continue processing it later on.
           */
          if (p_ctx->cdir) {
            struct dir_stack_entry* new_entry;

            new_entry =
                (struct dir_stack_entry*)malloc(sizeof(struct dir_stack_entry));
#if HAVE_CEPHFS_CEPH_STATX_H
            memcpy(&new_entry->statx, &p_ctx->statx, sizeof(new_entry->statx));
#else
            memcpy(&new_entry->statp, &p_ctx->statp, sizeof(new_entry->statp));
#endif
            new_entry->cdir = p_ctx->cdir;
            p_ctx->dir_stack->push(new_entry);
          }

          /*
           * Open this directory for processing.
           */
          status = ceph_opendir(p_ctx->cmount, ".", &p_ctx->cdir);
          if (status < 0) {
            BErrNo be;

            Jmsg(ctx, M_ERROR, "cephfs-fd: ceph_opendir(%s) failed: %s\n",
                 p_ctx->next_filename, be.bstrerror(-status));
            p_ctx->type = FT_NOOPEN;

            /*
             * Pop the previous directory handle and continue processing that.
             */
            if (!p_ctx->dir_stack->empty()) {
              struct dir_stack_entry* entry;

              entry = (struct dir_stack_entry*)p_ctx->dir_stack->pop();
#if HAVE_CEPHFS_CEPH_STATX_H
              memcpy(&p_ctx->statx, &entry->statx, sizeof(p_ctx->statx));
#else
              memcpy(&p_ctx->statp, &entry->statp, sizeof(p_ctx->statp));
#endif
              p_ctx->cdir = entry->cdir;
              free(entry);

              status = ceph_chdir(p_ctx->cmount, "..");
              if (status < 0) {
                BErrNo be;

                Jmsg(ctx, M_ERROR, "cephfs-fd: ceph_chdir(..) failed: %s\n",
                     p_ctx->next_filename, be.bstrerror(-status));
                return bRC_Error;
              }
            }
          } else {
            const char* cwd;

            cwd = ceph_getcwd(p_ctx->cmount);
            PmStrcpy(p_ctx->cwd, cwd);
          }
        }
      }

      /*
       * No link target and read the actual content.
       */
      sp->link = NULL;
      sp->no_read = true;
      break;
    case FT_DIREND:
      /*
       * For a directory, link is the same as fname, but with trailing slash
       * and don't read the actual content.
       */
      Mmsg(p_ctx->link_target, "%s/", p_ctx->next_filename);
      sp->link = p_ctx->link_target;
      sp->no_read = true;
      break;
    case FT_LNK:
      /*
       * Link target and don't read the actual content.
       */
      sp->link = p_ctx->link_target;
      sp->no_read = true;
      break;
    case FT_REGE:
    case FT_REG:
    case FT_SPEC:
    case FT_RAW:
    case FT_FIFO:
      /*
       * No link target and read the actual content.
       */
      sp->link = NULL;
      sp->no_read = false;
      break;
    default:
      /*
       * No link target and don't read the actual content.
       */
      sp->link = NULL;
      sp->no_read = true;
      break;
  }

  sp->fname = p_ctx->next_filename;
  sp->type = p_ctx->type;
#if HAVE_CEPHFS_CEPH_STATX_H
  memcpy(&sp->statp, &p_ctx->statx, sizeof(sp->statp));
#else
  memcpy(&sp->statp, &p_ctx->statp, sizeof(sp->statp));
#endif
  sp->save_time = p_ctx->since;

  /*
   * For Incremental and Differential backups use checkChanges method to
   * see if we need to backup this file.
   */
  switch (p_ctx->backup_level) {
    case L_INCREMENTAL:
    case L_DIFFERENTIAL:
      switch (bfuncs->checkChanges(ctx, sp)) {
        case bRC_Seen:
          Dmsg(ctx, debuglevel,
               "cephfs-fd: skipping %s checkChanges returns bRC_Seen\n",
               p_ctx->next_filename);
          switch (sp->type) {
            case FT_DIRBEGIN:
            case FT_DIREND:
              sp->type = FT_DIRNOCHG;
              break;
            default:
              sp->type = FT_NOCHG;
              break;
          }
          break;
        default:
          break;
      }
  }

  return bRC_OK;
}

/**
 * Done with backup of this file
 */
static bRC endBackupFile(bpContext* ctx)
{
  plugin_ctx* p_ctx = (plugin_ctx*)ctx->pContext;

  if (!p_ctx) { return bRC_Error; }

  /*
   * See if we need to fix the utimes.
   */
  if (BitIsSet(FO_NOATIME, p_ctx->flags)) {
    struct utimbuf times;

#if HAVE_CEPHFS_CEPH_STATX_H
    times.actime = p_ctx->statx.stx_atime.tv_sec;
    times.modtime = p_ctx->statx.stx_mtime.tv_sec;
#else
    times.actime = p_ctx->statp.st_atime;
    times.modtime = p_ctx->statp.st_mtime;
#endif

    ceph_utime(p_ctx->cmount, p_ctx->next_filename, &times);
  }

  return get_next_file_to_backup(ctx);
}

/**
 * Strip any backslashes in the string.
 */
static inline void StripBackSlashes(char* value)
{
  char* bp;

  bp = value;
  while (*bp) {
    switch (*bp) {
      case '\\':
        bstrinlinecpy(bp, bp + 1);
        break;
      default:
        break;
    }

    bp++;
  }
}

/**
 * Only set destination to value when it has no previous setting.
 */
static inline void SetStringIfNull(char** destination, char* value)
{
  if (!*destination) {
    *destination = strdup(value);
    StripBackSlashes(*destination);
  }
}

/**
 * Always set destination to value and clean any previous one.
 */
static inline void SetString(char** destination, char* value)
{
  if (*destination) { free(*destination); }

  *destination = strdup(value);
  StripBackSlashes(*destination);
}

/**
 * Parse the plugin definition passed in.
 *
 * The definition is in this form:
 *
 * cephfs:conffile=<path_to_config>:basedir=<basedir>:
 */
static bRC parse_plugin_definition(bpContext* ctx, void* value)
{
  int i;
  bool keep_existing;
  char *plugin_definition, *bp, *argument, *argument_value;
  plugin_ctx* p_ctx = (plugin_ctx*)ctx->pContext;

  if (!p_ctx || !value) { return bRC_Error; }

  /*
   * See if we already got some plugin definition before and its exactly the
   * same.
   */
  if (p_ctx->plugin_definition) {
    if (bstrcmp(p_ctx->plugin_definition, (char*)value)) { return bRC_OK; }

    free(p_ctx->plugin_definition);
  }

  /*
   * Keep track of the last processed plugin definition.
   */
  p_ctx->plugin_definition = strdup((char*)value);

  keep_existing = (p_ctx->plugin_options) ? true : false;

  /*
   * Parse the plugin definition.
   * Make a private copy of the whole string.
   */
  plugin_definition = strdup((char*)value);

  bp = strchr(plugin_definition, ':');
  if (!bp) {
    Jmsg(ctx, M_FATAL, "cephfs-fd: Illegal plugin definition %s\n",
         plugin_definition);
    Dmsg(ctx, debuglevel, "cephfs-fd: Illegal plugin definition %s\n",
         plugin_definition);
    goto bail_out;
  }

  /*
   * Skip the first ':'
   */
  bp++;
  while (bp) {
    if (strlen(bp) == 0) { break; }

    /*
     * Each argument is in the form:
     *    <argument> = <argument_value>
     *
     * So we setup the right pointers here, argument to the beginning
     * of the argument, argument_value to the beginning of the argument_value.
     */
    argument = bp;
    argument_value = strchr(bp, '=');
    if (!argument_value) {
      Jmsg(ctx, M_FATAL, "cephfs-fd: Illegal argument %s without value\n",
           argument);
      Dmsg(ctx, debuglevel, "cephfs-fd: Illegal argument %s without value\n",
           argument);
      goto bail_out;
    }
    *argument_value++ = '\0';

    /*
     * See if there are more arguments and setup for the next run.
     */
    bp = argument_value;
    do {
      bp = strchr(bp, ':');
      if (bp) {
        if (*(bp - 1) != '\\') {
          *bp++ = '\0';
          break;
        } else {
          bp++;
        }
      }
    } while (bp);

    for (i = 0; plugin_arguments[i].name; i++) {
      if (Bstrcasecmp(argument, plugin_arguments[i].name)) {
        char** str_destination = NULL;

        switch (plugin_arguments[i].type) {
          case argument_conffile:
            str_destination = &p_ctx->conffile;
            break;
          case argument_basedir:
            str_destination = &p_ctx->basedir;
            break;
          default:
            break;
        }

        /*
         * Keep the first value, ignore any next setting.
         */
        if (str_destination) {
          if (keep_existing) {
            SetStringIfNull(str_destination, argument_value);
          } else {
            SetString(str_destination, argument_value);
          }
        }

        /*
         * When we have a match break the loop.
         */
        break;
      }
    }

    /*
     * Got an invalid keyword ?
     */
    if (!plugin_arguments[i].name) {
      Jmsg(
          ctx, M_FATAL,
          "cephfs-fd: Illegal argument %s with value %s in plugin definition\n",
          argument, argument_value);
      Dmsg(
          ctx, debuglevel,
          "cephfs-fd: Illegal argument %s with value %s in plugin definition\n",
          argument, argument_value);
      goto bail_out;
    }
  }

  free(plugin_definition);
  return bRC_OK;

bail_out:
  free(plugin_definition);
  return bRC_Error;
}

/**
 * Open a CEPHFS mountpoint.
 */
static bRC connect_to_cephfs(bpContext* ctx)
{
  int status;
  plugin_ctx* p_ctx = (plugin_ctx*)ctx->pContext;

  /*
   * If we get called and we already have a handle to cephfs we should tear it
   * down.
   */
  if (p_ctx->cmount) {
    ceph_shutdown(p_ctx->cmount);
    p_ctx->cmount = NULL;
  }

  status = ceph_create(&p_ctx->cmount, NULL);
  if (status < 0) {
    BErrNo be;

    Jmsg(ctx, M_ERROR, "cephfs-fd: ceph_create failed: %s\n",
         be.bstrerror(-status));
    return bRC_Error;
  }

  status = ceph_conf_read_file(p_ctx->cmount, p_ctx->conffile);
  if (status < 0) {
    BErrNo be;

    Jmsg(ctx, M_ERROR, "cephfs-fd: ceph_conf_read_file(%s) failed: %s\n",
         p_ctx->conffile, be.bstrerror(-status));
    goto bail_out;
  }

  status = ceph_mount(p_ctx->cmount, NULL);
  if (status < 0) {
    BErrNo be;

    Jmsg(ctx, M_ERROR, "cephfs-fd: ceph_mount failed: %s\n",
         be.bstrerror(-status));
    goto bail_out;
  }

  return bRC_OK;

bail_out:

  return bRC_Error;
}

/**
 * Generic setup for performing a backup.
 */
static bRC setup_backup(bpContext* ctx, void* value)
{
  plugin_ctx* p_ctx = (plugin_ctx*)ctx->pContext;

  if (!p_ctx || !value) { return bRC_Error; }

  /*
   * If we are already having a handle to cepfs and we are getting the
   * same plugin definition there is no need to tear down the whole stuff and
   * setup exactly the same.
   */
  if (p_ctx->cmount && bstrcmp((char*)value, p_ctx->plugin_definition)) {
    return bRC_OK;
  }

  if (connect_to_cephfs(ctx) != bRC_OK) { return bRC_Error; }

  /*
   * Setup the directory we need to start scanning by setting the filetype
   * to FT_DIRBEGIN e.g. same as recursing into directory and let the recurse
   * logic do the rest of the work.
   */
  p_ctx->type = FT_DIRBEGIN;
  if (p_ctx->basedir && strlen(p_ctx->basedir) > 0) {
    PmStrcpy(p_ctx->next_filename, p_ctx->basedir);
  } else {
    PmStrcpy(p_ctx->next_filename, "/");
  }

  return bRC_OK;
}

/**
 * Generic setup for performing a restore.
 */
static bRC setup_restore(bpContext* ctx, void* value)
{
  plugin_ctx* p_ctx = (plugin_ctx*)ctx->pContext;

  if (!p_ctx || !value) { return bRC_Error; }

  /*
   * If we are already having a handle to cepfs and we are getting the
   * same plugin definition there is no need to tear down the whole stuff and
   * setup exactly the same.
   */
  if (p_ctx->cmount && bstrcmp((char*)value, p_ctx->plugin_definition)) {
    return bRC_OK;
  }

  return connect_to_cephfs(ctx);
}

/**
 * Bareos is calling us to do the actual I/O
 */
static bRC pluginIO(bpContext* ctx, struct io_pkt* io)
{
  plugin_ctx* p_ctx = (plugin_ctx*)ctx->pContext;

  if (!p_ctx) { return bRC_Error; }

  io->io_errno = 0;
  io->lerror = 0;
  io->win32 = false;

  switch (io->func) {
    case IO_OPEN:
      p_ctx->cfd = ceph_open(p_ctx->cmount, io->fname, io->flags, io->mode);
      if (p_ctx->cfd < 0) {
        io->status = -1;
        io->io_errno = -p_ctx->cfd;
        goto bail_out;
      }
      io->status = 0;
      break;
    case IO_READ:
      if (p_ctx->cfd) {
        io->status =
            ceph_read(p_ctx->cmount, p_ctx->cfd, io->buf, io->count, -1);
        if (io->status < 0) {
          io->io_errno = -io->status;
          io->status = -1;
          goto bail_out;
        }
      } else {
        io->status = -1;
        io->io_errno = EBADF;
        goto bail_out;
      }
      break;
    case IO_WRITE:
      if (p_ctx->cfd) {
        io->status =
            ceph_write(p_ctx->cmount, p_ctx->cfd, io->buf, io->count, -1);
        if (io->status < 0) {
          io->io_errno = -io->status;
          io->status = -1;
          goto bail_out;
        }
      } else {
        io->status = -1;
        io->io_errno = EBADF;
        goto bail_out;
      }
      break;
    case IO_CLOSE:
      if (p_ctx->cfd) {
        io->status = ceph_close(p_ctx->cmount, p_ctx->cfd);
        if (io->status < 0) {
          io->io_errno = -io->status;
          io->status = -1;
          goto bail_out;
        }
      } else {
        io->status = -1;
        io->io_errno = EBADF;
        goto bail_out;
      }
      break;
    case IO_SEEK:
      if (p_ctx->cfd) {
        io->status =
            ceph_lseek(p_ctx->cmount, p_ctx->cfd, io->offset, io->whence);
        if (io->status < 0) {
          io->io_errno = -io->status;
          io->status = -1;
          goto bail_out;
        }
      } else {
        io->status = -1;
        io->io_errno = EBADF;
        goto bail_out;
      }
      break;
  }

  return bRC_OK;

bail_out:
  return bRC_Error;
}

/**
 * See if we need to do any postprocessing after the restore.
 */
static bRC end_restore_job(bpContext* ctx, void* value)
{
  bRC retval = bRC_OK;
  plugin_ctx* p_ctx = (plugin_ctx*)ctx->pContext;

  if (!p_ctx) { return bRC_Error; }

  Dmsg(ctx, debuglevel, "cephfs-fd: entering end_restore_job\n");

  Dmsg(ctx, debuglevel, "cephfs-fd: leaving end_restore_job\n");

  return retval;
}

/**
 * Bareos is notifying us that a plugin name string was found,
 * and passing us the plugin command, so we can prepare for a restore.
 */
static bRC startRestoreFile(bpContext* ctx, const char* cmd) { return bRC_OK; }

/**
 * Bareos is notifying us that the plugin data has terminated,
 * so the restore for this particular file is done.
 */
static bRC endRestoreFile(bpContext* ctx) { return bRC_OK; }

/**
 * Create a parent directory using the cephfs API.
 *
 * We don't use cephfs_mkdirs as we want to keep track which
 * directories got created by us.
 */
static inline bool CephfsMakedirs(plugin_ctx* p_ctx, const char* directory)
{
  char* bp;
#if HAVE_CEPHFS_CEPH_STATX_H
  struct ceph_statx stx;
#else
  struct stat st;
#endif
  bool retval = false;
  PoolMem new_directory(PM_FNAME);

  PmStrcpy(new_directory, directory);

  /*
   * See if the parent exists.
   */
  bp = strrchr(new_directory.c_str(), '/');
  if (bp) {
    /*
     * See if we reached the root.
     */
    if (bp == new_directory.c_str()) {
      /*
       * Create the directory.
       */
      if (ceph_mkdir(p_ctx->cmount, directory, 0750) == 0) {
        if (!p_ctx->path_list) { p_ctx->path_list = path_list_init(); }
        PathListAdd(p_ctx->path_list, strlen(directory), directory);
        retval = true;
      }
    } else {
      *bp = '\0';

#if HAVE_CEPHFS_CEPH_STATX_H
      if (ceph_statx(p_ctx->cmount, new_directory.c_str(), &stx,
                     CEPH_STATX_SIZE, AT_SYMLINK_NOFOLLOW) != 0) {
#else
      if (ceph_lstat(p_ctx->cmount, new_directory.c_str(), &st) != 0) {
#endif
        switch (errno) {
          case ENOENT:
            /*
             * Make sure our parent exists.
             */
            retval = CephfsMakedirs(p_ctx, new_directory.c_str());
            if (!retval) { return false; }

            /*
             * Create the directory.
             */
            if (ceph_mkdir(p_ctx->cmount, directory, 0750) == 0) {
              if (!p_ctx->path_list) { p_ctx->path_list = path_list_init(); }
              PathListAdd(p_ctx->path_list, strlen(directory), directory);
              retval = true;
            }
            break;
          default:
            break;
        }
      } else {
        retval = true;
      }
    }
  }

  return retval;
}

/**
 * This is called during restore to create the file (if necessary) We must
 * return in rp->create_status:
 *
 *  CF_ERROR    -- error
 *  CF_SKIP     -- skip processing this file
 *  CF_EXTRACT  -- extract the file (i.e.call i/o routines)
 *  CF_CREATED  -- created, but no content to extract (typically directories)
 */
static bRC createFile(bpContext* ctx, struct restore_pkt* rp)
{
  int status;
  bool exists = false;
#if HAVE_CEPHFS_CEPH_STATX_H
  struct ceph_statx stx;
#else
  struct stat st;
#endif
  plugin_ctx* p_ctx = (plugin_ctx*)ctx->pContext;

  if (!p_ctx) { return bRC_Error; }

  /*
   * See if the file already exists.
   */
  Dmsg(ctx, 400, "cephfs-fd: Replace=%c %d\n", (char)rp->replace, rp->replace);
#if HAVE_CEPHFS_CEPH_STATX_H
  status = ceph_statx(p_ctx->cmount, rp->ofname, &stx, CEPH_STATX_SIZE,
                      AT_SYMLINK_NOFOLLOW);
#else
  status = ceph_lstat(p_ctx->cmount, rp->ofname, &st);
#endif
  if (status == 0) {
    exists = true;

    switch (rp->replace) {
      case REPLACE_IFNEWER:
#if HAVE_CEPHFS_CEPH_STATX_H
        if (rp->statp.st_mtime <= stx.stx_mtime.tv_sec) {
#else
        if (rp->statp.st_mtime <= st.st_mtime) {
#endif
          Jmsg(ctx, M_INFO, 0, _("cephfs-fd: File skipped. Not newer: %s\n"),
               rp->ofname);
          rp->create_status = CF_SKIP;
          goto bail_out;
        }
        break;
      case REPLACE_IFOLDER:
#if HAVE_CEPHFS_CEPH_STATX_H
        if (rp->statp.st_mtime >= stx.stx_mtime.tv_sec) {
#else
        if (rp->statp.st_mtime >= st.st_mtime) {
#endif
          Jmsg(ctx, M_INFO, 0, _("cephfs-fd: File skipped. Not older: %s\n"),
               rp->ofname);
          rp->create_status = CF_SKIP;
          goto bail_out;
        }
        break;
      case REPLACE_NEVER:
        /*
         * Set attributes if we created this directory
         */
        if (rp->type == FT_DIREND &&
            PathListLookup(p_ctx->path_list, rp->ofname)) {
          break;
        }
        Jmsg(ctx, M_INFO, 0, _("cephfs-fd: File skipped. Already exists: %s\n"),
             rp->ofname);
        rp->create_status = CF_SKIP;
        goto bail_out;
      case REPLACE_ALWAYS:
        break;
    }
  }

  switch (rp->type) {
    case FT_LNKSAVED: /* Hard linked, file already saved */
    case FT_LNK:
    case FT_SPEC: /* Fifo, ... to be backed up */
    case FT_REGE: /* Empty file */
    case FT_REG:  /* Regular file */
      /*
       * See if file already exists then we need to unlink it.
       */
      if (exists) {
        Dmsg(ctx, 400, "cephfs-fd: unlink %s\n", rp->ofname);
        status = ceph_unlink(p_ctx->cmount, rp->ofname);
        if (status != 0) {
          BErrNo be;

          Jmsg(ctx, M_ERROR, 0,
               _("cephfs-fd: File %s already exists and could not be replaced. "
                 "ERR=%s.\n"),
               rp->ofname, be.bstrerror(-status));
          /*
           * Continue despite error
           */
        }
      } else {
        /*
         * File doesn't exist see if we need to create the parent directory.
         */
        PoolMem parent_dir(PM_FNAME);
        char* bp;

        PmStrcpy(parent_dir, rp->ofname);
        bp = strrchr(parent_dir.c_str(), '/');
        if (bp) {
          *bp = '\0';
          if (strlen(parent_dir.c_str())) {
            if (!CephfsMakedirs(p_ctx, parent_dir.c_str())) {
              rp->create_status = CF_ERROR;
              goto bail_out;
            }
          }
        }
      }

      /*
       * See if we need to perform anything special for the restore file type.
       */
      switch (rp->type) {
        case FT_LNKSAVED:
          status = ceph_link(p_ctx->cmount, rp->olname, rp->ofname);
          if (status < 0) {
            BErrNo be;

            Jmsg(ctx, M_ERROR, "cephfs-fd: ceph_link(%s) failed: %s\n",
                 rp->ofname, be.bstrerror(-status));
            rp->create_status = CF_ERROR;
          } else {
            rp->create_status = CF_CREATED;
          }
          break;
        case FT_LNK:
          status = ceph_symlink(p_ctx->cmount, rp->olname, rp->ofname);
          if (status < 0) {
            BErrNo be;

            Jmsg(ctx, M_ERROR, "cephfs-fd: ceph_symlink(%s) failed: %s\n",
                 rp->ofname, be.bstrerror(-status));
            rp->create_status = CF_ERROR;
          } else {
            rp->create_status = CF_CREATED;
          }
          break;
        case FT_SPEC:
          status = ceph_mknod(p_ctx->cmount, rp->olname, rp->statp.st_mode,
                              rp->statp.st_rdev);
          if (status < 0) {
            BErrNo be;

            Jmsg(ctx, M_ERROR, "cephfs-fd: ceph_mknod(%s) failed: %s\n",
                 rp->ofname, be.bstrerror(-status));
            rp->create_status = CF_ERROR;
          } else {
            rp->create_status = CF_CREATED;
          }
          break;
        default:
          rp->create_status = CF_EXTRACT;
          break;
      }
      break;
    case FT_DIRBEGIN:
    case FT_DIREND:
      if (!CephfsMakedirs(p_ctx, rp->ofname)) {
        rp->create_status = CF_ERROR;
      } else {
        rp->create_status = CF_CREATED;
      }
      break;
    case FT_DELETED:
      Jmsg(ctx, M_INFO, 0,
           _("cephfs-fd: Original file %s have been deleted: type=%d\n"),
           rp->ofname, rp->type);
      rp->create_status = CF_SKIP;
      break;
    default:
      Jmsg(ctx, M_ERROR, 0,
           _("cephfs-fd: Unknown file type %d; not restored: %s\n"), rp->type,
           rp->ofname);
      rp->create_status = CF_ERROR;
      break;
  }

bail_out:
  return bRC_OK;
}

static bRC setFileAttributes(bpContext* ctx, struct restore_pkt* rp)
{
  int status;
  struct utimbuf times;
  plugin_ctx* p_ctx = (plugin_ctx*)ctx->pContext;

  if (!p_ctx) { return bRC_Error; }

  /*
   * Restore uid and gid.
   */
  status = ceph_lchown(p_ctx->cmount, rp->ofname, rp->statp.st_uid,
                       rp->statp.st_gid);
  if (status < 0) {
    BErrNo be;

    Jmsg(ctx, M_ERROR, "cephfs-fd: ceph_lchown(%s) failed: %s\n", rp->ofname,
         be.bstrerror(-status));
    return bRC_Error;
  }

  /*
   * Restore mode.
   */
  status = ceph_chmod(p_ctx->cmount, rp->ofname, rp->statp.st_mode);
  if (status < 0) {
    BErrNo be;

    Jmsg(ctx, M_ERROR, "cephfs-fd: ceph_chmod(%s) failed: %s\n", rp->ofname,
         be.bstrerror(-status));
    return bRC_Error;
  }

  /*
   * Restore access and modification times.
   */
  times.actime = rp->statp.st_atime;
  times.modtime = rp->statp.st_mtime;

  status = ceph_utime(p_ctx->cmount, p_ctx->next_filename, &times);
  if (status < 0) {
    BErrNo be;

    Jmsg(ctx, M_ERROR, "cephfs-fd: ceph_utime(%s) failed: %s\n", rp->ofname,
         be.bstrerror(-status));
    return bRC_Error;
  }

  return bRC_OK;
}

/**
 * When using Incremental dump, all previous dumps are necessary
 */
static bRC checkFile(bpContext* ctx, char* fname)
{
  plugin_ctx* p_ctx = (plugin_ctx*)ctx->pContext;

  if (!p_ctx) { return bRC_Error; }

  return bRC_OK;
}

/**
 * Acls are saved using extended attributes.
 */
static const char* xattr_acl_skiplist[3] = {"system.posix_acl_access",
                                            "system.posix_acl_default", NULL};

static inline uint32_t serialize_acl_stream(PoolMem* buf,
                                            uint32_t expected_serialize_len,
                                            uint32_t offset,
                                            const char* acl_name,
                                            uint32_t acl_name_length,
                                            char* xattr_value,
                                            uint32_t xattr_value_length)
{
  ser_declare;
  uint32_t content_length;
  char* buffer;

  /*
   * Make sure the serialized stream fits in the poolmem buffer.
   * We allocate some more to be sure the stream is gonna fit.
   */
  buf->check_size(offset + expected_serialize_len + 10);

  buffer = buf->c_str() + offset;
  SerBegin(buffer, expected_serialize_len + 10);

  /*
   * Encode the ACL name including the \0
   */
  ser_uint32(acl_name_length + 1);
  SerBytes(acl_name, acl_name_length + 1);

  /*
   * Encode the actual ACL data as stored as XATTR.
   */
  ser_uint32(xattr_value_length);
  SerBytes(xattr_value, xattr_value_length);

  SerEnd(buffer, expected_serialize_len + 10);
  content_length = SerLength(buffer);

  return offset + content_length;
}

static bRC getAcl(bpContext* ctx, acl_pkt* ap)
{
  bool skip_xattr, abort_retrieval;
  int current_size;
  int32_t xattr_value_length;
  uint32_t content_length = 0;
  uint32_t expected_serialize_len;
  PoolMem xattr_value(PM_MESSAGE), serialized_acls(PM_MESSAGE);
  plugin_ctx* p_ctx = (plugin_ctx*)ctx->pContext;

  if (!p_ctx) { return bRC_Error; }

  abort_retrieval = false;
  for (int cnt = 0; xattr_acl_skiplist[cnt] != NULL; cnt++) {
    skip_xattr = false;
    while (1) {
      current_size = xattr_value.MaxSize();
      xattr_value_length =
          ceph_lgetxattr(p_ctx->cmount, ap->fname, xattr_acl_skiplist[cnt],
                         xattr_value.c_str(), current_size);
      if (xattr_value_length < 0) {
        BErrNo be;

        switch (errno) {
#if defined(ENOATTR) || defined(ENODATA)
#if defined(ENOATTR)
          case ENOATTR:
#endif
#if defined(ENODATA) && ENOATTR != ENODATA
          case ENODATA:
#endif
            skip_xattr = true;
            break;
#endif
#if defined(ENOTSUP) || defined(EOPNOTSUPP)
#if defined(ENOTSUP)
          case ENOTSUP:
#endif
#if defined(EOPNOTSUPP) && EOPNOTSUPP != ENOTSUP
          case EOPNOTSUPP:
#endif
            abort_retrieval = true;
            break;
#endif
          case ERANGE:
            /*
             * Not enough room in buffer double its size and retry.
             */
            xattr_value.check_size(current_size * 2);
            continue;
          default:
            Jmsg(ctx, M_ERROR, "cephfs-fd: ceph_lgetxattr(%s) failed: %s\n",
                 ap->fname, be.bstrerror(-xattr_value_length));
            return bRC_Error;
        }
      }

      /*
       * Retrieved the xattr so break the loop.
       */
      break;
    }

    if (abort_retrieval) { break; }

    if (skip_xattr) { continue; }

    /*
     * Serialize the data.
     */
    expected_serialize_len =
        strlen(xattr_acl_skiplist[cnt]) + xattr_value_length + 4;
    content_length = serialize_acl_stream(
        &serialized_acls, expected_serialize_len, content_length,
        xattr_acl_skiplist[cnt], strlen(xattr_acl_skiplist[cnt]),
        xattr_value.c_str(), xattr_value_length);
  }

  if (content_length > 0) {
    ap->content = (char*)malloc(content_length);
    memcpy(ap->content, serialized_acls.c_str(), content_length);
    ap->content_length = content_length;
  }

  return bRC_OK;
}

static bRC setAcl(bpContext* ctx, acl_pkt* ap)
{
  int status;
  unser_declare;
  uint32_t acl_name_length;
  uint32_t xattr_value_length;
  PoolMem xattr_value(PM_MESSAGE), acl_name(PM_MESSAGE);

  plugin_ctx* p_ctx = (plugin_ctx*)ctx->pContext;

  if (!p_ctx) { return bRC_Error; }

  UnserBegin(ap->content, ap->content_length);
  while (UnserLength(ap->content) < ap->content_length) {
    unser_uint32(acl_name_length);

    /*
     * Decode the ACL name including the \0
     */
    acl_name.check_size(acl_name_length);
    UnserBytes(acl_name.c_str(), acl_name_length);

    unser_uint32(xattr_value_length);

    /*
     * Decode the actual ACL data as stored as XATTR.
     */
    xattr_value.check_size(xattr_value_length);
    UnserBytes(xattr_value.c_str(), xattr_value_length);

    status = ceph_lsetxattr(p_ctx->cmount, ap->fname, acl_name.c_str(),
                            xattr_value.c_str(), xattr_value_length, 0);
    if (status < 0) {
      BErrNo be;

      Jmsg(ctx, M_ERROR, "cephfs-fd: ceph_lsetxattr(%s) failed: %s\n",
           ap->fname, be.bstrerror(-status));
      return bRC_Error;
    }
  }

  UnserEnd(ap->content, ap->content_length);

  return bRC_OK;
}

static bRC getXattr(bpContext* ctx, xattr_pkt* xp)
{
  char* bp;
  bool skip_xattr;
  int status, current_size;
  int32_t xattr_value_length;
  PoolMem xattr_value(PM_MESSAGE);
  plugin_ctx* p_ctx = (plugin_ctx*)ctx->pContext;

  if (!p_ctx) { return bRC_Error; }

  /*
   * See if we need to retrieve the xattr list.
   */
  if (!p_ctx->processing_xattr) {
    while (1) {
      current_size = SizeofPoolMemory(p_ctx->xattr_list);
      status = ceph_llistxattr(p_ctx->cmount, xp->fname, p_ctx->xattr_list,
                               current_size);
      if (status < 0) {
        BErrNo be;

        switch (status) {
#if defined(ENOTSUP) || defined(EOPNOTSUPP)
#if defined(ENOTSUP)
          case ENOTSUP:
#endif
#if defined(EOPNOTSUPP) && EOPNOTSUPP != ENOTSUP
          case EOPNOTSUPP:
#endif
            return bRC_OK;
#endif
          case -ERANGE:
            /*
             * Not enough room in buffer double its size and retry.
             */
            p_ctx->xattr_list =
                CheckPoolMemorySize(p_ctx->xattr_list, current_size * 2);
            continue;
          default:
            Jmsg(ctx, M_ERROR, "cephfs-fd: ceph_llistxattr(%s) failed: %s\n",
                 xp->fname, be.bstrerror(-status));
            return bRC_Error;
        }
      } else if (status == 0) {
        /*
         * Nothing to do.
         */
        return bRC_OK;
      }

      /*
       * Retrieved the xattr list so break the loop.
       */
      break;
    }

    /*
     * Data from llistxattr is in the following form:
     *
     * user.name1\0system.name1\0user.name2\0
     *
     * We add an extra \0 at the end so we have an unique terminator
     * to know when we hit the end of the list.
     */
    p_ctx->xattr_list = CheckPoolMemorySize(p_ctx->xattr_list, status + 1);
    p_ctx->xattr_list[status] = '\0';
    p_ctx->next_xattr_name = p_ctx->xattr_list;
    p_ctx->processing_xattr = true;
  }

  while (1) {
    /*
     * On some OSes you also get the acls in the extented attribute list.
     * So we check if we are already backing up acls and if we do we
     * don't store the extended attribute with the same info.
     */
    skip_xattr = false;
    if (BitIsSet(FO_ACL, p_ctx->flags)) {
      for (int cnt = 0; xattr_acl_skiplist[cnt] != NULL; cnt++) {
        if (bstrcmp(p_ctx->next_xattr_name, xattr_acl_skiplist[cnt])) {
          skip_xattr = true;
          break;
        }
      }
    }

    if (!skip_xattr) {
      current_size = xattr_value.MaxSize();
      xattr_value_length =
          ceph_lgetxattr(p_ctx->cmount, xp->fname, p_ctx->next_xattr_name,
                         xattr_value.c_str(), current_size);
      if (xattr_value_length < 0) {
        BErrNo be;

        switch (xattr_value_length) {
#if defined(ENOATTR) || defined(ENODATA)
#if defined(ENOATTR)
          case ENOATTR:
#endif
#if defined(ENODATA) && ENOATTR != ENODATA
          case ENODATA:
#endif
            skip_xattr = true;
            break;
#endif
#if defined(ENOTSUP) || defined(EOPNOTSUPP)
#if defined(ENOTSUP)
          case ENOTSUP:
#endif
#if defined(EOPNOTSUPP) && EOPNOTSUPP != ENOTSUP
          case EOPNOTSUPP:
#endif
            return bRC_OK;
#endif
          case -ERANGE:
            /*
             * Not enough room in buffer double its size and retry.
             */
            xattr_value.check_size(current_size * 2);
            continue;
          default:
            Jmsg(ctx, M_ERROR, "cephfs-fd: ceph_lgetxattr(%s) failed: %s\n",
                 xp->fname, be.bstrerror(-xattr_value_length));
            return bRC_Error;
        }
      }

      /*
       * Retrieved the xattr so break the loop.
       */
      break;
    } else {
      /*
       * No data to retrieve so break the loop.
       */
      break;
    }
  }

  if (!skip_xattr) {
    xp->name = strdup(p_ctx->next_xattr_name);
    xp->name_length = strlen(xp->name) + 1;
    xp->value = (char*)malloc(xattr_value_length);
    memcpy(xp->value, xattr_value.c_str(), xattr_value_length);
    xp->value_length = xattr_value_length;
  }

  /*
   * See if there are more xattr to process.
   */
  bp = strchr(p_ctx->next_xattr_name, '\0');
  if (bp) {
    bp++;
    if (*bp != '\0') {
      p_ctx->next_xattr_name = bp;
      return bRC_More;
    }
  }

  /*
   * No more reset processing_xattr flag.
   */
  p_ctx->processing_xattr = false;
  return bRC_OK;
}

static bRC setXattr(bpContext* ctx, xattr_pkt* xp)
{
  int status;
  plugin_ctx* p_ctx = (plugin_ctx*)ctx->pContext;

  if (!p_ctx) { return bRC_Error; }

  status = ceph_lsetxattr(p_ctx->cmount, xp->fname, xp->name, xp->value,
                          xp->value_length, 0);
  if (status < 0) {
    BErrNo be;

    Jmsg(ctx, M_ERROR, "cephfs-fd: ceph_lsetxattr(%s) failed: %s\n", xp->fname,
         be.bstrerror(-status));
    return bRC_Error;
  }

  return bRC_OK;
}

} /* namespace filedaemon */
