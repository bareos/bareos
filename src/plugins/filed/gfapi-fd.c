/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2014-2017 Planets Communications B.V.
   Copyright (C) 2014-2016 Bareos GmbH & Co. KG

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
 * GlusterFS GFAPI plugin for the Bareos File Daemon
 */
#include "bareos.h"
#include "fd_plugins.h"
#include "fd_common.h"
#include "fileopts.h"

#include <api/glfs.h>
#include <compat-errno.h>

static const int dbglvl = 150;

#define PLUGIN_LICENSE      "Bareos AGPLv3"
#define PLUGIN_AUTHOR       "Marco van Wieringen"
#define PLUGIN_DATE         "February 2016"
#define PLUGIN_VERSION      "2"
#define PLUGIN_DESCRIPTION  "Bareos GlusterFS GFAPI File Daemon Plugin"
#define PLUGIN_USAGE        "gfapi:volume=gluster[+transport]\\://[server[:port]]/volname[/dir][?socket=...]:snapdir=<snapdir>:gffilelist=<file>"

#define GLFS_PATH_MAX 4096

/*
 * Forward referenced functions
 */
static bRC newPlugin(bpContext *ctx);
static bRC freePlugin(bpContext *ctx);
static bRC getPluginValue(bpContext *ctx, pVariable var, void *value);
static bRC setPluginValue(bpContext *ctx, pVariable var, void *value);
static bRC handlePluginEvent(bpContext *ctx, bEvent *event, void *value);
static bRC startBackupFile(bpContext *ctx, struct save_pkt *sp);
static bRC endBackupFile(bpContext *ctx);
static bRC pluginIO(bpContext *ctx, struct io_pkt *io);
static bRC startRestoreFile(bpContext *ctx, const char *cmd);
static bRC endRestoreFile(bpContext *ctx);
static bRC createFile(bpContext *ctx, struct restore_pkt *rp);
static bRC setFileAttributes(bpContext *ctx, struct restore_pkt *rp);
static bRC checkFile(bpContext *ctx, char *fname);
static bRC getAcl(bpContext *ctx, acl_pkt *ap);
static bRC setAcl(bpContext *ctx, acl_pkt *ap);
static bRC getXattr(bpContext *ctx, xattr_pkt *xp);
static bRC setXattr(bpContext *ctx, xattr_pkt *xp);

static bRC parse_plugin_definition(bpContext *ctx, void *value);
static bRC end_restore_job(bpContext *ctx, void *value);
static bRC setup_backup(bpContext *ctx, void *value);
static bRC setup_restore(bpContext *ctx, void *value);

/*
 * Pointers to Bareos functions
 */
static bFuncs *bfuncs = NULL;
static bInfo  *binfo = NULL;

/*
 * Plugin Information block
 */
static genpInfo pluginInfo = {
   sizeof(pluginInfo),
   FD_PLUGIN_INTERFACE_VERSION,
   FD_PLUGIN_MAGIC,
   PLUGIN_LICENSE,
   PLUGIN_AUTHOR,
   PLUGIN_DATE,
   PLUGIN_VERSION,
   PLUGIN_DESCRIPTION,
   PLUGIN_USAGE
};

/*
 * Plugin entry points for Bareos
 */
static pFuncs pluginFuncs = {
   sizeof(pluginFuncs),
   FD_PLUGIN_INTERFACE_VERSION,

   /* Entry points into plugin */
   newPlugin,                         /* new plugin instance */
   freePlugin,                        /* free plugin instance */
   getPluginValue,
   setPluginValue,
   handlePluginEvent,
   startBackupFile,
   endBackupFile,
   startRestoreFile,
   endRestoreFile,
   pluginIO,
   createFile,
   setFileAttributes,
   checkFile,
   getAcl,
   setAcl,
   getXattr,
   setXattr
};

/*
 * Plugin private context
 */
struct plugin_ctx {
   int32_t backup_level;              /* Backup level e.g. Full/Differential/Incremental */
   utime_t since;                     /* Since time for Differential/Incremental */
   char *plugin_options;              /* Options passed to plugin */
   char *plugin_definition;           /* Previous plugin definition passed to plugin */
   char *gfapi_volume_spec;           /* Unparsed Gluster volume specification */
   char *transport;                   /* Gluster transport protocol to management server */
   char *servername;                  /* Gluster management server */
   char *volumename;                  /* Gluster volume */
   char *basedir;                     /* Basedir to start backup in */
   char *snapdir;                     /* Specific snapdir to use while doing backup */
   int serverport;                    /* Gluster management server portnr */
   char flags[FOPTS_BYTES];           /* Bareos internal flags */
   int32_t type;                      /* FT_xx for this file */
   struct stat statp;                 /* Stat struct for next file to save */
   bool processing_xattr;             /* Set when we are processing a xattr list */
   char *next_xattr_name;             /* Next xattr name to process */
   bool crawl_fs;                     /* Use local fs crawler to find files to backup */
   char *gf_file_list;                /* File with list of files generated by glusterfind to backup */
   bool is_accurate;                  /* Backup has accurate option enabled */
   POOLMEM *cwd;                      /* Current Working Directory */
   POOLMEM *next_filename;            /* Next filename to save */
   POOLMEM *link_target;              /* Target symlink points to */
   POOLMEM *xattr_list;               /* List of xattrs */
#ifndef HAVE_GLFS_READDIRPLUS
   POOLMEM *dirent_buffer;            /* Temporary buffer for current dirent structure */
#endif
   alist *dir_stack;                  /* Stack of directories when recursing */
   htable *path_list;                 /* Hash table with directories created on restore. */
   glfs_t *glfs;                      /* Gluster volume handle */
   glfs_fd_t *gdir;                   /* Gluster directory handle */
   glfs_fd_t *gfd;                    /* Gluster file handle */
   FILE *file_list_handle;            /* File handle to file with files to backup */
};

/*
 * This defines the arguments that the plugin parser understands.
 */
enum plugin_argument_type {
   argument_none,
   argument_volume_spec,
   argument_snapdir,
   argument_basedir,
   argument_gf_file_list
};

struct plugin_argument {
   const char *name;
   enum plugin_argument_type type;
};

static plugin_argument plugin_arguments[] = {
   { "volume", argument_volume_spec },
   { "snapdir", argument_snapdir },
   { "basedir", argument_basedir },
   { "gffilelist", argument_gf_file_list },
   { NULL, argument_none }
};

enum gluster_find_type {
   gf_type_none,
   gf_type_new,
   gf_type_modify,
   gf_type_rename,
   gf_type_delete
};

/*
 * If we recurse into a subdir we push the current directory onto
 * a stack so we can pop it after we have processed the subdir.
 */
struct dir_stack_entry {
   struct stat statp;                 /* Stat struct of directory */
   glfs_fd_t *gdir;                   /* Gluster directory handle */
};

struct gluster_find_mapping {
   const char *name;
   enum gluster_find_type type;
   int compare_size;
};

static gluster_find_mapping gluster_find_mappings[] = {
   { "NEW ", gf_type_new, 4 },
   { "MODIFY ", gf_type_modify, 7 },
   { "RENAME ", gf_type_rename, 7 },
   { "DELETE ", gf_type_delete, 7 },
   { NULL, gf_type_none, 0 }
};

static inline struct gluster_find_mapping *find_glustermap_eventtype(const char *gf_entry)
{
   struct gluster_find_mapping *gf_mapping;

   gf_mapping = NULL;
   for (int i = 0; gluster_find_mappings[i].name; i++) {
      if (bstrncasecmp(gf_entry,
                       gluster_find_mappings[i].name,
                       gluster_find_mappings[i].compare_size)) {
         gf_mapping = &gluster_find_mappings[i];
         break;
      }
   }

   return gf_mapping;
}

static inline int to_hex(char ch)
{
   int retval;

   if (B_ISDIGIT(ch)) {
      retval = (ch - '0');
   } else if ( ch >= 'a' && ch <= 'f') {
      retval = (ch - 'a') + 10;
   } else if (ch >= 'A' && ch <= 'F') {
      retval = (ch - 'A') + 10;
   } else {
      retval = -1;
   }

   return retval;
}

/*
 * Quick and dirty version of RFC 3986 percent-decoding
 * It decodes the entries returned by glusterfind which uses
 * the python urllib.quote_plus method.
 */
static bool urllib_unquote_plus(char *str)
{
   char *p, *q;
   bool retval = true;

   /*
    * Set both pointers to the beginning of the string.
    * q is the converted cursor and p is the walking cursor.
    */
   q = str;
   p = str;

   while (*p) {
      switch (*p) {
      case '%': {
         int ch, hex;

         /*
          * See if the % is followed by at least two chars.
          */
         hex = to_hex(*(p + 1));
         if (hex == -1) {
            retval = false;
            goto bail_out;
         }
         ch = hex * 16;
         hex = to_hex(*(p + 2));
         if (hex == -1) {
            retval = false;
            goto bail_out;
         }
         ch += hex;
         *q++ = ch;
         p += 2;
         break;
      }
      case '+':
         *q++ = ' ';
         break;
      default:
         *q++ = *p;
         break;
      }
      p++;
   }

   /*
    * Terminate translated string.
    */
   *q = '\0';

bail_out:
   return retval;
}

#ifdef __cplusplus
extern "C" {
#endif

/*
 * loadPlugin() and unloadPlugin() are entry points that are exported, so Bareos can
 * directly call these two entry points they are common to all Bareos plugins.
 *
 * External entry point called by Bareos to "load" the plugin
 */
bRC DLL_IMP_EXP loadPlugin(bInfo *lbinfo,
                           bFuncs *lbfuncs,
                           genpInfo **pinfo,
                           pFuncs **pfuncs)
{
   bfuncs = lbfuncs;                  /* set Bareos funct pointers */
   binfo = lbinfo;
   *pinfo = &pluginInfo;              /* return pointer to our info */
   *pfuncs = &pluginFuncs;            /* return pointer to our functions */

   return bRC_OK;
}

/*
 * External entry point to unload the plugin
 */
bRC DLL_IMP_EXP unloadPlugin()
{
   return bRC_OK;
}

#ifdef __cplusplus
}
#endif

/*
 * The following entry points are accessed through the function pointers we supplied to Bareos.
 * Each plugin type (dir, fd, sd) has its own set of entry points that the plugin must define.
 *
 * Create a new instance of the plugin i.e. allocate our private storage
 */
static bRC newPlugin(bpContext *ctx)
{
   plugin_ctx *p_ctx;

   p_ctx = (plugin_ctx *)malloc(sizeof(plugin_ctx));
   if (!p_ctx) {
      return bRC_Error;
   }
   memset(p_ctx, 0, sizeof(plugin_ctx));
   ctx->pContext = (void *)p_ctx;        /* set our context pointer */

   /*
    * Allocate some internal memory for:
    * - The file we are processing
    * - The link target of a symbolic link.
    * - The list of xattrs.
    */
   p_ctx->next_filename = get_pool_memory(PM_FNAME);
   p_ctx->link_target = get_pool_memory(PM_FNAME);
   p_ctx->xattr_list = get_pool_memory(PM_MESSAGE);

   /*
    * Resize all buffers for PATH like names to GLFS_PATH_MAX.
    */
   p_ctx->next_filename = check_pool_memory_size(p_ctx->next_filename, GLFS_PATH_MAX);
   p_ctx->link_target = check_pool_memory_size(p_ctx->link_target, GLFS_PATH_MAX);

   /*
    * Only register the events we are really interested in.
    */
   bfuncs->registerBareosEvents(ctx,
                                7,
                                bEventLevel,
                                bEventSince,
                                bEventRestoreCommand,
                                bEventBackupCommand,
                                bEventPluginCommand,
                                bEventEndRestoreJob,
                                bEventNewPluginOptions);

   return bRC_OK;
}

/*
 * Free a plugin instance, i.e. release our private storage
 */
static bRC freePlugin(bpContext *ctx)
{
   plugin_ctx *p_ctx = (plugin_ctx *)ctx->pContext;
   if (!p_ctx) {
      return bRC_Error;
   }

   Dmsg(ctx, dbglvl, "gfapi-fd: entering freePlugin\n");

   if (p_ctx->file_list_handle) {
      fclose(p_ctx->file_list_handle);
   }

   if (p_ctx->path_list) {
      free_path_list(p_ctx->path_list);
      p_ctx->path_list = NULL;
   }

   if (p_ctx->dir_stack) {
      p_ctx->dir_stack->destroy();
      delete p_ctx->dir_stack;
   }

   if (p_ctx->glfs) {
      glfs_fini(p_ctx->glfs);
      p_ctx->glfs = NULL;
   }

#ifndef HAVE_GLFS_READDIRPLUS
   if (p_ctx->dirent_buffer) {
      free_pool_memory(p_ctx->dirent_buffer);
   }
#endif

   if (p_ctx->cwd) {
      free_pool_memory(p_ctx->cwd);
   }

   free_pool_memory(p_ctx->xattr_list);
   free_pool_memory(p_ctx->link_target);
   free_pool_memory(p_ctx->next_filename);

   if (p_ctx->basedir) {
      free(p_ctx->basedir);
   }

   if (p_ctx->snapdir) {
      free(p_ctx->snapdir);
   }

   if (p_ctx->gfapi_volume_spec) {
      free(p_ctx->gfapi_volume_spec);
   }

   if (p_ctx->plugin_definition) {
      free(p_ctx->plugin_definition);
   }

   if (p_ctx->plugin_options) {
      free(p_ctx->plugin_options);
   }

   free(p_ctx);
   p_ctx = NULL;

   Dmsg(ctx, dbglvl, "gfapi-fd: leaving freePlugin\n");

   return bRC_OK;
}

/*
 * Return some plugin value (none defined)
 */
static bRC getPluginValue(bpContext *ctx, pVariable var, void *value)
{
   return bRC_OK;
}

/*
 * Set a plugin value (none defined)
 */
static bRC setPluginValue(bpContext *ctx, pVariable var, void *value)
{
   return bRC_OK;
}

/*
 * Handle an event that was generated in Bareos
 */
static bRC handlePluginEvent(bpContext *ctx, bEvent *event, void *value)
{
   bRC retval;
   plugin_ctx *p_ctx = (plugin_ctx *)ctx->pContext;

   if (!p_ctx) {
      return bRC_Error;
   }

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
      if (retval == bRC_OK) {
         retval = setup_restore(ctx, value);
      }
      break;
   case bEventBackupCommand:
      retval = parse_plugin_definition(ctx, value);
      if (retval == bRC_OK) {
         retval = setup_backup(ctx, value);
      }
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
      p_ctx->plugin_options = bstrdup((char *)value);
      break;
   case bEventEndRestoreJob:
      retval = end_restore_job(ctx, value);
      break;
   default:
      Jmsg(ctx, M_FATAL, "gfapi-fd: unknown event=%d\n", event->eventType);
      Dmsg(ctx, dbglvl, "gfapi-fd: unknown event=%d\n", event->eventType);
      retval = bRC_Error;
      break;
   }

   return retval;
}

/*
 * Get the next file to backup.
 */
static bRC get_next_file_to_backup(bpContext *ctx)
{
   int status;
   struct save_pkt sp;
   struct dirent *entry;
   plugin_ctx *p_ctx = (plugin_ctx *)ctx->pContext;

   /*
    * See if we are actually crawling the fs ourself or depending on an external filelist.
    */
   if (p_ctx->crawl_fs) {
      /*
       * See if we just saved the directory then we are done processing this directory.
       */
      switch (p_ctx->type) {
      case FT_DIREND:
         /*
          * See if there is anything on the dir stack to pop off and continue reading that directory.
          */
         if (!p_ctx->dir_stack->empty()) {
            struct dir_stack_entry *entry;

            /*
             * Change the GLFS cwd back one dir.
             */
            status = glfs_chdir(p_ctx->glfs, "..");
            if (status != 0) {
               berrno be;

               Jmsg(ctx, M_ERROR, "gfapi-fd: glfs_chdir(%s) failed: %s\n", "..", be.bstrerror());
               return bRC_Error;
            }

            /*
             * Save where we are in the tree.
             */
            glfs_getcwd(p_ctx->glfs, p_ctx->cwd, sizeof_pool_memory(p_ctx->cwd));

            /*
             * Pop the previous directory handle and continue processing that.
             */
            entry = (struct dir_stack_entry *)p_ctx->dir_stack->pop();
            memcpy(&p_ctx->statp, &entry->statp, sizeof(p_ctx->statp));
            p_ctx->gdir = entry->gdir;
            free(entry);
         } else {
            return bRC_OK;
         }
         break;
      default:
         break;
      }

      if (!p_ctx->gdir) {
         return bRC_Error;
      }
   }

   /*
    * Loop until we know what file is next or when we are done.
    */
   while (1) {
      memset(&p_ctx->statp, 0, sizeof(p_ctx->statp));

      /*
       * See if we are actually crawling the fs ourself or depending on an external filelist.
       */
      if (!p_ctx->crawl_fs) {
         char *bp;
         struct gluster_find_mapping *gf_mapping;

         /*
          * Get the next file from the filelist.
          */
         if (bfgets(p_ctx->next_filename, p_ctx->file_list_handle) == NULL) {
            /*
             * See if we hit EOF.
             */
            if (feof( p_ctx->file_list_handle)) {
               return bRC_OK;
            }

            return bRC_Error;
         }

         /*
          * Strip the newline.
          */
         strip_trailing_junk(p_ctx->next_filename);
         Dmsg(ctx, dbglvl, "gfapi-fd: Processing glusterfind entry %s\n", p_ctx->next_filename);

         /*
          * Lookup mapping to see what type of entry we are processing.
          */
         gf_mapping = find_glustermap_eventtype(p_ctx->next_filename);
         if (!gf_mapping) {
            Dmsg(ctx, dbglvl, "gfapi-fd: Unknown glusterfind entry %s\n", p_ctx->next_filename);
            continue;
         }

         switch (gf_mapping->type) {
         case gf_type_new:
         case gf_type_modify:
            /*
             * NEW and MODIFY just means we need to backup the file.
             */
            bstrinlinecpy(p_ctx->next_filename, p_ctx->next_filename + gf_mapping->compare_size);
            urllib_unquote_plus(p_ctx->next_filename);
            break;
         case gf_type_rename:
            /*
             * RENAME means we clear the seen bitmap for the original name and backup the new filename.
             */
            bstrinlinecpy(p_ctx->next_filename, p_ctx->next_filename + gf_mapping->compare_size);
            bp = strchr(p_ctx->next_filename, ' ');
            if (!bp) {
               Jmsg(ctx, M_ERROR, "Illegal glusterfind RENAME entry: %s\n", p_ctx->next_filename);
               continue;
            }
            *bp++ = '\0';
            if (p_ctx->is_accurate) {
               urllib_unquote_plus(p_ctx->next_filename);
               bfuncs->ClearSeenBitmap(ctx, false, p_ctx->next_filename);
            }
            bstrinlinecpy(p_ctx->next_filename, bp);
            urllib_unquote_plus(p_ctx->next_filename);
            break;
         case gf_type_delete:
            /*
             * DELETE means we clear the seen bitmap for this file and continue.
             */
            if (p_ctx->is_accurate) {
               bstrinlinecpy(p_ctx->next_filename, p_ctx->next_filename + gf_mapping->compare_size);
               urllib_unquote_plus(p_ctx->next_filename);
               bfuncs->ClearSeenBitmap(ctx, false, p_ctx->next_filename);
            }
            continue;
         default:
            Jmsg(ctx, M_ERROR, "Unrecognized glusterfind entry %s\n", p_ctx->next_filename);
            Dmsg(ctx, dbglvl, "gfapi-fd: Skipping glusterfind entry %s\n", p_ctx->next_filename);
            continue;
         }

         /*
          * If we have a basename we should filter on that.
          */
         if (p_ctx->basedir && !bstrncmp(p_ctx->basedir, p_ctx->next_filename, strlen(p_ctx->basedir))) {
            Dmsg(ctx, dbglvl, "gfapi-fd: next file %s not under basedir %d\n",
                 p_ctx->next_filename, p_ctx->basedir);
            continue;
         }

         status = glfs_stat(p_ctx->glfs, p_ctx->next_filename, &p_ctx->statp);
         if (status != 0) {
            berrno be;

            switch (errno) {
            case ENOENT:
               /*
                * Note: This was silently ignored before, now at least emit a warning
                * in the job log that does not trigger the "OK -- with warnings" termination
                */
               Jmsg(ctx, M_WARNING, "gfapi-fd: glfs_stat(%s) failed: %s (skipped)\n", p_ctx->next_filename, be.bstrerror());
               continue;
            case GF_ERROR_CODE_STALE:
               Jmsg(ctx, M_ERROR, "gfapi-fd: glfs_stat(%s) failed: %s (skipped)\n", p_ctx->next_filename, be.bstrerror());
               continue;
            default:
               Dmsg(ctx, dbglvl, "gfapi-fd: glfs_stat(%s) failed: %s errno: %d\n",
                    p_ctx->next_filename, be.bstrerror(), errno);
               Jmsg(ctx, M_FATAL, "gfapi-fd: glfs_stat(%s) failed: %s\n", p_ctx->next_filename, be.bstrerror());
               return bRC_Error;
            }
         }
      } else {
#ifndef HAVE_GLFS_READDIRPLUS
         entry = NULL;
         glfs_readdirplus_r(p_ctx->gdir, &p_ctx->statp, (dirent *)p_ctx->dirent_buffer, &entry);
#else
         entry = glfs_readdirplus(p_ctx->gdir, &p_ctx->statp);
#endif

         /*
          * No more entries in this directory ?
          */
         if (!entry) {
            status = glfs_stat(p_ctx->glfs, p_ctx->cwd, &p_ctx->statp);
            if (status != 0) {
               berrno be;

               Jmsg(ctx, M_FATAL, "glfs_stat(%s) failed: %s\n", p_ctx->cwd, be.bstrerror());
               return bRC_Error;
            }

            glfs_closedir(p_ctx->gdir);
            p_ctx->gdir = NULL;
            p_ctx->type = FT_DIREND;

            pm_strcpy(p_ctx->next_filename, p_ctx->cwd);

            Dmsg(ctx, dbglvl, "gfapi-fd: next file to backup %s\n", p_ctx->next_filename);

            return bRC_More;
         }

         /*
          * Skip `.', `..', and excluded file names.
          */
         if (entry->d_name[0] == '\0' ||
            (entry->d_name[0] == '.' && (entry->d_name[1] == '\0' ||
            (entry->d_name[1] == '.' && entry->d_name[2] == '\0')))) {
            continue;
         }

         Mmsg(p_ctx->next_filename, "%s/%s", p_ctx->cwd, entry->d_name);
      }

      /*
       * Determine the FileType.
       */
      switch (p_ctx->statp.st_mode & S_IFMT) {
      case S_IFREG:
         p_ctx->type = FT_REG;
         break;
      case S_IFLNK:
         p_ctx->type = FT_LNK;
         status = glfs_readlink(p_ctx->glfs, p_ctx->next_filename,
                                p_ctx->link_target, sizeof_pool_memory(p_ctx->link_target));
         if (status < 0) {
            berrno be;

            Jmsg(ctx, M_ERROR, "gfapi-fd: glfs_readlink(%s) failed: %s\n", p_ctx->next_filename, be.bstrerror());
            p_ctx->type = FT_NOFOLLOW;
         }
         p_ctx->link_target[status] = '\0';
         break;
      case S_IFDIR:
         if (!p_ctx->crawl_fs) {
            /*
             * When we don't crawl the filesystem ourself we directly move to FT_DIREND as we
             * don't recurse into the directory but just process a list of externally provided filenames.
             */
            p_ctx->type = FT_DIREND;
         } else {
            /*
             * When we crawl the filesystem ourself we first do a FT_DIRBEGIN and recurse if needed
             * and then save the directory in the FT_DIREND.
             */
            p_ctx->type = FT_DIRBEGIN;
         }
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
         Jmsg(ctx, M_FATAL, "gfapi-fd: Unknown filetype encountered %ld for %s\n",
              p_ctx->statp.st_mode & S_IFMT, p_ctx->next_filename);
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
      memcpy(&sp.statp, &p_ctx->statp, sizeof(sp.statp));

      if (bfuncs->AcceptFile(ctx, &sp) == bRC_Skip) {
         Dmsg(ctx, dbglvl, "gfapi-fd: file %s skipped due to current fileset settings\n", p_ctx->next_filename);
         continue;
      }

      /*
       * If we made it here we have the next file to backup.
       */
      break;
   }

   Dmsg(ctx, dbglvl, "gfapi-fd: next file to backup %s\n", p_ctx->next_filename);

   return bRC_More;
}

/*
 * Start the backup of a specific file
 */
static bRC startBackupFile(bpContext *ctx, struct save_pkt *sp)
{
   int status;
   plugin_ctx *p_ctx = (plugin_ctx *)ctx->pContext;

   /*
    * Save the current flags used to save the next file.
    */
   copy_bits(FO_MAX, sp->flags, p_ctx->flags);

   switch (p_ctx->type) {
   case FT_DIRBEGIN:
      /*
       * We should never get here when we don't crawl the filesystem ourself but just
       * in case we do we test in the if that p_ctx->crawl_fs is not set.
       *
       * See if we are recursing if so we open the directory and process it.
       * We also open the directory when it is the toplevel e.g. when p_ctx->gdir == NULL.
       */
      if (p_ctx->crawl_fs && (!p_ctx->gdir || !bit_is_set(FO_NO_RECURSION, p_ctx->flags))) {
         /*
          * Change into the directory and process all entries in it.
          */
         status = glfs_chdir(p_ctx->glfs, p_ctx->next_filename);
         if (status != 0) {
            berrno be;

            Jmsg(ctx, M_ERROR, "gfapi-fd: glfs_chdir(%s) failed: %s\n",
                 p_ctx->next_filename, be.bstrerror());
            p_ctx->type = FT_NOOPEN;
         } else {
            /*
             * Push the current directory onto the directory stack so we can
             * continue processing it later on.
             */
            if (p_ctx->gdir) {
               struct dir_stack_entry *new_entry;

               new_entry = (struct dir_stack_entry *)malloc(sizeof(struct dir_stack_entry));
               memcpy(&new_entry->statp, &p_ctx->statp, sizeof(new_entry->statp));
               new_entry->gdir = p_ctx->gdir;
               p_ctx->dir_stack->push(new_entry);
            }

            /*
             * Open this directory for processing.
             */
            p_ctx->gdir = glfs_opendir(p_ctx->glfs, ".");
            if (!p_ctx->gdir) {
               berrno be;

               Jmsg(ctx, M_ERROR, "gfapi-fd: glfs_opendir(%s) failed: %s\n",
                    p_ctx->next_filename, be.bstrerror());
               p_ctx->type = FT_NOOPEN;

               /*
                * Pop the previous directory handle and continue processing that.
                */
               if (!p_ctx->dir_stack->empty()) {
                  struct dir_stack_entry *entry;

                  entry = (struct dir_stack_entry *)p_ctx->dir_stack->pop();
                  memcpy(&p_ctx->statp, &entry->statp, sizeof(p_ctx->statp));
                  p_ctx->gdir = entry->gdir;
                  free(entry);

                  glfs_chdir(p_ctx->glfs, "..");
               }
            } else {
               glfs_getcwd(p_ctx->glfs, p_ctx->cwd, sizeof_pool_memory(p_ctx->cwd));
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
   memcpy(&sp->statp, &p_ctx->statp, sizeof(sp->statp));
   sp->save_time = p_ctx->since;

   /*
    * If we crawl the filesystem ourself we check the timestamps when
    * using glusterfind the changelog gives changes since the since time
    * we provided to it so it makes no sense to check again.
    */
   if (p_ctx->crawl_fs) {
      /*
       * For Incremental and Differential backups use checkChanges method to
       * see if we need to backup this file.
       */
      switch (p_ctx->backup_level) {
      case L_INCREMENTAL:
      case L_DIFFERENTIAL:
         /*
          * When sp->type is FT_DIRBEGIN, skip calling checkChanges() because it would be useless.
          */
         if (sp->type == FT_DIRBEGIN) {
            Dmsg(ctx, dbglvl, "gfapi-fd: skip checkChanges() for %s because sp->type is FT_DIRBEGIN\n", p_ctx->next_filename);
            sp->type = FT_DIRNOCHG;
            break;
         }
         /*
          * When glfs_chdir() or glfs_opendir() failed, then sp->type is FT_NOOPEN, then
          * skip calling checkChanges() because it would be useless.
          */
         if (sp->type == FT_NOOPEN) {
            Dmsg(ctx, dbglvl, "gfapi-fd: skip checkChanges() for %s because sp->type is FT_NOOPEN\n", p_ctx->next_filename);
            break;
         }

         switch (bfuncs->checkChanges(ctx, sp)) {
         case bRC_Seen:
            Dmsg(ctx, dbglvl, "gfapi-fd: skipping %s checkChanges returns bRC_Seen\n", p_ctx->next_filename);
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
   }

   return bRC_OK;
}

/*
 * Done with backup of this file
 */
static bRC endBackupFile(bpContext *ctx)
{
   plugin_ctx *p_ctx = (plugin_ctx *)ctx->pContext;

   if (!p_ctx) {
      return bRC_Error;
   }

   /*
    * See if we need to fix the utimes.
    */
   if (bit_is_set(FO_NOATIME, p_ctx->flags)) {
      struct timespec times[2];

      times[0].tv_sec = p_ctx->statp.st_atime;
      times[0].tv_nsec = 0;
      times[1].tv_sec = p_ctx->statp.st_mtime;
      times[1].tv_nsec = 0;

      glfs_lutimens(p_ctx->glfs, p_ctx->next_filename, times);
   }

   return get_next_file_to_backup(ctx);
}

/*
 * Strip any backslashes in the string.
 */
static inline void strip_back_slashes(char *value)
{
   char *bp;

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

/*
 * Only set destination to value when it has no previous setting.
 */
static inline void set_string_if_null(char **destination, char *value)
{
   if (!*destination) {
      *destination = bstrdup(value);
      strip_back_slashes(*destination);
   }
}

/*
 * Always set destination to value and clean any previous one.
 */
static inline void set_string(char **destination, char *value)
{
   if (*destination) {
      free(*destination);
   }

   *destination = bstrdup(value);
   strip_back_slashes(*destination);
}

/*
 * Parse the plugin definition passed in.
 *
 * The definition is in this form:
 *
 * gfapi:volume=gluster[+transport]\\://[server[:port]]/volname[/dir][?socket=...]
 */
static bRC parse_plugin_definition(bpContext *ctx, void *value)
{
   int i;
   bool keep_existing;
   char *plugin_definition, *bp, *argument, *argument_value;
   plugin_ctx *p_ctx = (plugin_ctx *)ctx->pContext;

   if (!p_ctx || !value) {
      return bRC_Error;
   }

   /*
    * See if we already got some plugin definition before and its exactly the same.
    */
   if (p_ctx->plugin_definition) {
      if (bstrcmp(p_ctx->plugin_definition, (char *)value)) {
         return bRC_OK;
      }

      free(p_ctx->plugin_definition);
   }

   /*
    * Keep track of the last processed plugin definition.
    */
   p_ctx->plugin_definition = bstrdup((char *)value);

   /*
    * Keep overrides passed in via pluginoptions.
    */
   keep_existing = (p_ctx->plugin_options) ? true : false;

   /*
    * Parse the plugin definition.
    * Make a private copy of the whole string.
    */
   plugin_definition = bstrdup((char *)value);

   bp = strchr(plugin_definition, ':');
   if (!bp) {
      Jmsg(ctx, M_FATAL, "gfapi-fd: Illegal plugin definition %s\n", plugin_definition);
      Dmsg(ctx, dbglvl, "gfapi-fd: Illegal plugin definition %s\n", plugin_definition);
      goto bail_out;
   }

   /*
    * Skip the first ':'
    */
   bp++;
   while (bp) {
      if (strlen(bp) == 0) {
         break;
      }

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
         Jmsg(ctx, M_FATAL, "gfapi-fd: Illegal argument %s without value\n", argument);
         Dmsg(ctx, dbglvl, "gfapi-fd: Illegal argument %s without value\n", argument);
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
         if (bstrcasecmp(argument, plugin_arguments[i].name)) {
            char **str_destination = NULL;

            switch (plugin_arguments[i].type) {
            case argument_volume_spec:
               str_destination = &p_ctx->gfapi_volume_spec;
               break;
            case argument_snapdir:
               str_destination = &p_ctx->snapdir;
               break;
            case argument_basedir:
               str_destination = &p_ctx->basedir;
               break;
            case argument_gf_file_list:
               str_destination = &p_ctx->gf_file_list;
               break;
            default:
               break;
            }

            /*
             * Keep the first value, ignore any next setting.
             */
            if (str_destination) {
               if (keep_existing) {
                  set_string_if_null(str_destination, argument_value);
               } else {
                  set_string(str_destination, argument_value);
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
         Jmsg(ctx, M_FATAL, "gfapi-fd: Illegal argument %s with value %s in plugin definition\n", argument, argument_value);
         Dmsg(ctx, dbglvl, "gfapi-fd: Illegal argument %s with value %s in plugin definition\n", argument, argument_value);
         goto bail_out;
      }
   }

   free(plugin_definition);

   return bRC_OK;

bail_out:
   free(plugin_definition);
   return bRC_Error;
}

/*
 * Create a parent directory using the gfapi.
 */
static inline bool gfapi_makedirs(plugin_ctx *p_ctx, const char *directory)
{
   int len;
   char *bp;
   struct stat st;
   bool retval = false;
   POOL_MEM new_directory(PM_FNAME);

   pm_strcpy(new_directory, directory);
   len = strlen(new_directory.c_str());

   /*
    * Strip any trailing slashes.
    */
   for (char *p = new_directory.c_str() + (len - 1);
        (p >= new_directory.c_str()) && *p == '/';
        p--) {
      *p = '\0';
   }

   if (strlen(new_directory.c_str()) &&
       glfs_stat(p_ctx->glfs, new_directory.c_str(), &st) != 0) {
      /*
       * See if the parent exists.
       */
      switch (errno) {
         case ENOENT:
            bp = strrchr(new_directory.c_str(), '/');
            if (bp) {
               /*
                * Make sure our parent exists.
                */
               *bp = '\0';
               retval = gfapi_makedirs(p_ctx, new_directory.c_str());
               if (!retval) {
                  return false;
               }

               /*
                * Create the directory.
                */
               if (glfs_mkdir(p_ctx->glfs, directory, 0750) == 0) {
                  if (!p_ctx->path_list) {
                     p_ctx->path_list = path_list_init();
                  }
                  path_list_add(p_ctx->path_list, strlen(directory), directory);
                  retval = true;
               }
            }
            break;
         default:
            break;
      }
   } else {
      retval = true;
   }

   return retval;
}

/*
 * Parse a gluster definition into something we can use for setting
 * up the right connection to a gluster management server and get access
 * to a gluster volume.
 *
 * Syntax:
 *
 * gluster[+transport]://[server[:port]]/volname[/dir][?socket=...]
 *
 * 'gluster' is the protocol.
 *
 * 'transport' specifies the transport type used to connect to gluster
 * management daemon (glusterd). Valid transport types are tcp, unix
 * and rdma. If a transport type isn't specified, then tcp type is assumed.
 *
 * 'server' specifies the server where the volume file specification for
 * the given volume resides. This can be either hostname, ipv4 address
 * or ipv6 address. ipv6 address needs to be within square brackets [ ].
 * If transport type is 'unix', then 'server' field should not be specifed.
 * The 'socket' field needs to be populated with the path to unix domain
 * socket.
 *
 * 'port' is the port number on which glusterd is listening. This is optional
 * and if not specified, QEMU will send 0 which will make gluster to use the
 * default port. If the transport type is unix, then 'port' should not be
 * specified.
 *
 * 'volname' is the name of the gluster volume which contains the data that
 * we need to be backup.
 *
 * 'dir' is an optional base directory on the 'volname'
 *
 * Examples:
 *
 * gluster://1.2.3.4/testvol[/dir]
 * gluster+tcp://1.2.3.4/testvol[/dir]
 * gluster+tcp://1.2.3.4:24007/testvol[/dir]
 * gluster+tcp://[1:2:3:4:5:6:7:8]/testvol[/dir]
 * gluster+tcp://[1:2:3:4:5:6:7:8]:24007/testvol[/dir]
 * gluster+tcp://server.domain.com:24007/testvol[/dir]
 * gluster+unix:///testvol[/dir]?socket=/tmp/glusterd.socket
 * gluster+rdma://1.2.3.4:24007/testvol[/dir]
 */
static inline bool parse_gfapi_devicename(char *devicename,
                                          char **transport,
                                          char **servername,
                                          char **volumename,
                                          char **dir,
                                          int *serverport)
{
   char *bp;

   /*
    * Make sure its a URI that starts with gluster.
    */
   if (!bstrncasecmp(devicename, "gluster", 7)) {
      return false;
   }

   /*
    * Parse any explicit protocol.
    */
   bp = strchr(devicename, '+');
   if (bp) {
      *transport = ++bp;
      bp = strchr(bp, ':');
      if (bp) {
         *bp++ = '\0';
      } else {
         goto bail_out;
      }
   } else {
      *transport = NULL;
      bp = strchr(devicename, ':');
      if (!bp) {
         goto bail_out;
      }
   }

   /*
    * When protocol is not UNIX parse servername and portnr.
    */
   if (!*transport || !bstrcasecmp(*transport, "unix")) {
      /*
       * Parse servername of gluster management server.
       */
      bp = strchr(bp, '/');

      /*
       * Validate URI.
       */
      if (!bp || *(bp + 1) != '/') {
         goto bail_out;
      }

      /*
       * Skip the two //
       */
      *bp++ = '\0';
      bp++;
      *servername = bp;

      /*
       * Parse any explicit server portnr.
       * We search reverse in the string for a : what indicates
       * a port specification but in that string there may not contain a ']'
       * because then we searching in a IPv6 string.
       */
      bp = strrchr(bp, ':');
      if (bp && !strchr(bp, ']')) {
         char *port;

         *bp++ = '\0';
         port = bp;
         bp = strchr(bp, '/');
         if (!bp) {
            goto bail_out;
         }
         *bp++ = '\0';
         *serverport = str_to_int64(port);
         *volumename = bp;

         /*
          * See if there is a dir specified.
          */
         bp = strchr(bp, '/');
         if (bp) {
            *bp++ = '\0';
            *dir = bp;
         }
      } else {
         *serverport = 0;
         bp = *servername;

         /*
          * Parse the volume name.
          */
         bp = strchr(bp, '/');
         if (!bp) {
            goto bail_out;
         }
         *bp++ = '\0';
         *volumename = bp;

         /*
          * See if there is a dir specified.
          */
         bp = strchr(bp, '/');
         if (bp) {
            *bp++ = '\0';
            *dir = bp;
         }
      }
   } else {
      /*
       * For UNIX serverport is zero.
       */
      *serverport = 0;

      /*
       * Validate URI.
       */
      if (*bp != '/' || *(bp + 1) != '/') {
         goto bail_out;
      }

      /*
       * Skip the two //
       */
      *bp++ = '\0';
      bp++;

      /*
       * For UNIX URIs the server part of the URI needs to be empty.
       */
      if (*bp++ != '/') {
         goto bail_out;
      }
      *volumename = bp;

      /*
       * See if there is a dir specified.
       */
      bp = strchr(bp, '/');
      if (bp) {
         *bp++ = '\0';
         *dir = bp;
      }

      /*
       * Parse any socket parameters.
       */
      bp = strchr(bp, '?');
      if (bp) {
         if (bstrncasecmp(bp + 1, "socket=", 7)) {
            *bp = '\0';
            *servername = bp + 8;
         }
      }
   }

   return true;

bail_out:
   return false;
}

/*
 * Open a volume using GFAPI.
 */
static bRC connect_to_gluster(bpContext *ctx, bool is_backup)
{
   int status;
   plugin_ctx *p_ctx = (plugin_ctx *)ctx->pContext;

   if (!p_ctx->gfapi_volume_spec) {
      return bRC_Error;
   }

   if (!parse_gfapi_devicename(p_ctx->gfapi_volume_spec,
                               &p_ctx->transport,
                               &p_ctx->servername,
                               &p_ctx->volumename,
                               &p_ctx->basedir,
                               &p_ctx->serverport)) {
      return bRC_Error;
   }

   /*
    * If we get called and we already have a handle to gfapi we should tear it down.
    */
   if (p_ctx->glfs) {
      glfs_fini(p_ctx->glfs);
      p_ctx->glfs = NULL;
   }

   p_ctx->glfs = glfs_new(p_ctx->volumename);
   if (!p_ctx->glfs) {
      goto bail_out;
   }

   status = glfs_set_volfile_server(p_ctx->glfs, (p_ctx->transport) ? p_ctx->transport : "tcp",
                                    p_ctx->servername, p_ctx->serverport);
   if (status < 0) {
      goto bail_out;
   }

   if (is_backup) {
      status = glfs_set_xlator_option(p_ctx->glfs, "*-md-cache", "cache-posix-acl", "true");
      if (status < 0) {
         goto bail_out;
      }
   }

   if (is_backup && p_ctx->snapdir) {
      status = glfs_set_xlator_option(p_ctx->glfs, "*-snapview-client", "snapdir-entry-path", p_ctx->snapdir);
      if (status < 0) {
         goto bail_out;
      }
   }

   status = glfs_init(p_ctx->glfs);
   if (status < 0) {
      goto bail_out;
   }

   return bRC_OK;

bail_out:
   if (p_ctx->glfs) {
      glfs_fini(p_ctx->glfs);
      p_ctx->glfs = NULL;
   }

   return bRC_Error;
}

/*
 * Generic setup for performing a backup.
 */
static bRC setup_backup(bpContext *ctx, void *value)
{
   bRC retval = bRC_Error;
   plugin_ctx *p_ctx = (plugin_ctx *)ctx->pContext;

   if (!p_ctx || !value) {
      goto bail_out;
   }

   /*
    * If we are already having a handle to gfapi and we are getting the
    * same plugin definition there is no need to tear down the whole stuff and
    * setup exactly the same.
    */
   if (p_ctx->glfs &&
       bstrcmp((char *)value, p_ctx->plugin_definition)) {
      return bRC_OK;
   }

   if (connect_to_gluster(ctx, true) != bRC_OK) {
      goto bail_out;
   }

   /*
    * See if we use an external list with files to backup or should crawl the filesystem ourself.
    */
   if (p_ctx->gf_file_list) {
      int accurate;

      /*
       * Get the setting for accurate for this Job.
       */
      bfuncs->getBareosValue(ctx, bVarAccurate, (void *)&accurate);
      if (accurate) {
         p_ctx->is_accurate = true;
      }

      p_ctx->crawl_fs = false;
      if ((p_ctx->file_list_handle = fopen(p_ctx->gf_file_list, "r")) == (FILE *)NULL) {
         Jmsg(ctx, M_FATAL, "Failed to open %s for reading files to backup\n", p_ctx->gf_file_list);
         Dmsg(ctx, dbglvl, "Failed to open %s for reading files to backup\n", p_ctx->gf_file_list);
         goto bail_out;
      }

      if (p_ctx->is_accurate) {
         /*
          * Mark all files as seen from the previous backup when this is a incremental or differential backup.
          * The entries we get from glusterfind are only the changes since that earlier backup.
          */
         switch (p_ctx->backup_level) {
         case L_INCREMENTAL:
         case L_DIFFERENTIAL:
            if (bfuncs->SetSeenBitmap(ctx, true, NULL) != bRC_OK) {
               Jmsg(ctx, M_FATAL, "Failed to enable all entries in Seen bitmap, not an accurate backup ?\n");
               Dmsg(ctx, dbglvl, "Failed to enable all entries in Seen bitmap, not an accurate backup ?\n");
               goto bail_out;
            }
            break;
         default:
            break;
         }
      }

      /*
       * Setup the plugin for the first entry.
       * As we need to get it from the gfflilelist we use get_next_file_to_backup()
       * to do the setup for us it retrieves the entry and does a setup of filetype etc.
       */
      switch (get_next_file_to_backup(ctx)) {
      case bRC_OK:
         /*
          * get_next_file_to_backup() normally returns bRC_More to indicate that there are
          * more files to backup. But when using glusterfind we use an external filelist which
          * could be empty in that special case we get bRC_OK back from get_next_file_to_backup()
          * and then only in setup_backup() we return bRC_Skip which will skip processing of any
          * more files to backup.
          */
         retval = bRC_Skip;
         break;
      case bRC_Error:
         Jmsg(ctx, M_FATAL, "Failed to get first file to backup\n");
         Dmsg(ctx, dbglvl, "Failed to get first file to backup\n");
         goto bail_out;
      default:
         retval = bRC_OK;
         break;
      }
   } else {
      p_ctx->crawl_fs = true;

      /*
       * Allocate some internal memory for:
       * - The current working dir.
       * - For the older glfs_readdirplus_r() function an dirent hold buffer.
       */
      p_ctx->cwd = get_pool_memory(PM_FNAME);
      p_ctx->cwd = check_pool_memory_size(p_ctx->cwd, GLFS_PATH_MAX);
#ifndef HAVE_GLFS_READDIRPLUS
      p_ctx->dirent_buffer = get_pool_memory(PM_FNAME);

      /*
       * Resize the dirent buffer to 512 bytes which should be enough to hold any dirent.
       */
      p_ctx->dirent_buffer = check_pool_memory_size(p_ctx->dirent_buffer, 512);
#endif

      /*
       * This is an alist that holds the stack of directories we have open.
       * We push the current directory onto this stack the moment we start
       * processing a sub directory and pop it from this list when we are
       * done processing that sub directory.
       */
      p_ctx->dir_stack = new alist(10, owned_by_alist);

      /*
       * Setup the directory we need to start scanning by setting the filetype
       * to FT_DIRBEGIN e.g. same as recursing into directory and let the recurse
       * logic do the rest of the work.
       */
      p_ctx->type = FT_DIRBEGIN;
      if (p_ctx->basedir && strlen(p_ctx->basedir) > 0) {
         pm_strcpy(p_ctx->next_filename, p_ctx->basedir);
      } else {
         pm_strcpy(p_ctx->next_filename, "/");
      }

      retval = bRC_OK;
   }

bail_out:
   return retval;
}

/*
 * Generic setup for performing a restore.
 */
static bRC setup_restore(bpContext *ctx, void *value)
{
   plugin_ctx *p_ctx = (plugin_ctx *)ctx->pContext;

   if (!p_ctx || !value) {
      return bRC_Error;
   }

   /*
    * If we are already having a handle to gfapi and we are getting the
    * same plugin definition there is no need to tear down the whole stuff and
    * setup exactly the same.
    */
   if (p_ctx->glfs &&
       bstrcmp((char *)value, p_ctx->plugin_definition)) {
      return bRC_OK;
   }

   return connect_to_gluster(ctx, false);
}

/*
 * Bareos is calling us to do the actual I/O
 */
static bRC pluginIO(bpContext *ctx, struct io_pkt *io)
{
   plugin_ctx *p_ctx = (plugin_ctx *)ctx->pContext;

   if (!p_ctx) {
      return bRC_Error;
   }

   io->io_errno = 0;
   io->lerror = 0;
   io->win32 = false;

   switch(io->func) {
   case IO_OPEN:
      /*
       * Close the gfd when it was not closed before.
       */
      if (p_ctx->gfd) {
         glfs_close(p_ctx->gfd);
      }

      if (io->flags & (O_CREAT | O_WRONLY)) {
         p_ctx->gfd = glfs_creat(p_ctx->glfs, io->fname, io->flags, io->mode);
      } else {
         p_ctx->gfd = glfs_open(p_ctx->glfs, io->fname, io->flags);
      }

      if (!p_ctx->gfd) {
         io->status = -1;
         io->io_errno = errno;
         goto bail_out;
      }
      io->status = 0;
      break;
   case IO_READ:
      if (p_ctx->gfd) {
         io->status = glfs_read(p_ctx->gfd, io->buf, io->count, 0);
         if (io->status < 0) {
            io->io_errno = errno;
            goto bail_out;
         }
      } else {
         io->status = -1;
         io->io_errno = EBADF;
         goto bail_out;
      }
      break;
   case IO_WRITE:
      if (p_ctx->gfd) {
         io->status = glfs_write(p_ctx->gfd, io->buf, io->count, 0);
         if (io->status < 0) {
            io->io_errno = errno;
            goto bail_out;
         }
      } else {
         io->status = -1;
         io->io_errno = EBADF;
         goto bail_out;
      }
      break;
   case IO_CLOSE:
      if (p_ctx->gfd) {
         io->status = glfs_close(p_ctx->gfd);
         if (io->status < 0) {
            io->io_errno = errno;
            goto bail_out;
         }
         p_ctx->gfd = NULL;
      } else {
         io->status = -1;
         io->io_errno = EBADF;
         goto bail_out;
      }
      break;
   case IO_SEEK:
      if (p_ctx->gfd) {
         io->status = glfs_lseek(p_ctx->gfd, io->offset, io->whence);
         if (io->status < 0) {
            io->io_errno = errno;
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

/*
 * See if we need to do any postprocessing after the restore.
 */
static bRC end_restore_job(bpContext *ctx, void *value)
{
   bRC retval = bRC_OK;
   plugin_ctx *p_ctx = (plugin_ctx *)ctx->pContext;

   if (!p_ctx) {
      return bRC_Error;
   }

   Dmsg(ctx, dbglvl, "gfapi-fd: entering end_restore_job\n");

   Dmsg(ctx, dbglvl, "gfapi-fd: leaving end_restore_job\n");

   return retval;
}

/*
 * Bareos is notifying us that a plugin name string was found,
 * and passing us the plugin command, so we can prepare for a restore.
 */
static bRC startRestoreFile(bpContext *ctx, const char *cmd)
{
   return bRC_OK;
}

/*
 * Bareos is notifying us that the plugin data has terminated,
 * so the restore for this particular file is done.
 */
static bRC endRestoreFile(bpContext *ctx)
{
   return bRC_OK;
}

/*
 * This is called during restore to create the file (if necessary) We must return in rp->create_status:
 *
 *  CF_ERROR    -- error
 *  CF_SKIP     -- skip processing this file
 *  CF_EXTRACT  -- extract the file (i.e.call i/o routines)
 *  CF_CREATED  -- created, but no content to extract (typically directories)
 */
static bRC createFile(bpContext *ctx, struct restore_pkt *rp)
{
   int status;
   bool exists = false;
   struct stat st;
   plugin_ctx *p_ctx = (plugin_ctx *)ctx->pContext;

   if (!p_ctx) {
      return bRC_Error;
   }

   /*
    * See if the file already exists.
    */
   Dmsg(ctx, 400, "gfapi-fd: Replace=%c %d\n", (char)rp->replace, rp->replace);
   status = glfs_lstat(p_ctx->glfs, rp->ofname, &st);
   if (status == 0) {
      exists = true;

      switch (rp->replace) {
      case REPLACE_IFNEWER:
         if (rp->statp.st_mtime <= st.st_mtime) {
            Jmsg(ctx, M_INFO, 0, _("gfapi-fd: File skipped. Not newer: %s\n"), rp->ofname);
            rp->create_status = CF_SKIP;
            goto bail_out;
         }
         break;
      case REPLACE_IFOLDER:
         if (rp->statp.st_mtime >= st.st_mtime) {
            Jmsg(ctx, M_INFO, 0, _("gfapi-fd: File skipped. Not older: %s\n"), rp->ofname);
            rp->create_status = CF_SKIP;
            goto bail_out;
         }
         break;
      case REPLACE_NEVER:
         /*
          * Set attributes if we created this directory
          */
         if (rp->type == FT_DIREND && path_list_lookup(p_ctx->path_list, rp->ofname)) {
            break;
         }
         Jmsg(ctx, M_INFO, 0, _("gfapi-fd: File skipped. Already exists: %s\n"), rp->ofname);
         rp->create_status = CF_SKIP;
         goto bail_out;
      case REPLACE_ALWAYS:
         break;
      }
   }

   switch (rp->type) {
   case FT_LNKSAVED:                  /* Hard linked, file already saved */
   case FT_LNK:
   case FT_SPEC:                      /* Fifo, ... to be backed up */
   case FT_REGE:                      /* Empty file */
   case FT_REG:                       /* Regular file */
      /*
       * See if file already exists then we need to unlink it.
       */
      if (exists) {
         Dmsg(ctx, 400, "gfapi-fd: unlink %s\n", rp->ofname);
         status = glfs_unlink(p_ctx->glfs, rp->ofname);
         if (status != 0) {
            berrno be;

            Jmsg(ctx, M_ERROR, 0,
                 _("gfapi-fd: File %s already exists and could not be replaced. ERR=%s.\n"),
                 rp->ofname, be.bstrerror());
            /*
             * Continue despite error
             */
         }
      } else {
         /*
          * File doesn't exist see if we need to create the parent directory.
          */
         POOL_MEM parent_dir(PM_FNAME);
         char *bp;

         pm_strcpy(parent_dir, rp->ofname);
         bp = strrchr(parent_dir.c_str(), '/');
         if (bp) {
            *bp = '\0';
            if (strlen(parent_dir.c_str())) {
               if (!gfapi_makedirs(p_ctx, parent_dir.c_str())) {
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
         status = glfs_link(p_ctx->glfs, rp->olname, rp->ofname);
         if (status != 0) {
            berrno be;

            Jmsg(ctx, M_ERROR, "gfapi-fd: glfs_link(%s) failed: %s\n", rp->ofname, be.bstrerror());
            rp->create_status = CF_ERROR;
         } else {
            rp->create_status = CF_CREATED;
         }
         break;
      case FT_LNK:
         status = glfs_symlink(p_ctx->glfs, rp->olname, rp->ofname);
         if (status != 0) {
            berrno be;

            Jmsg(ctx, M_ERROR, "gfapi-fd: glfs_symlink(%s) failed: %s\n", rp->ofname, be.bstrerror());
            rp->create_status = CF_ERROR;
         } else {
            rp->create_status = CF_CREATED;
         }
         break;
      case FT_SPEC:
         status = glfs_mknod(p_ctx->glfs, rp->olname, rp->statp.st_mode, rp->statp.st_rdev);
         if (status != 0) {
            berrno be;

            Jmsg(ctx, M_ERROR, "gfapi-fd: glfs_mknod(%s) failed: %s\n", rp->ofname, be.bstrerror());
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
      if (!gfapi_makedirs(p_ctx, rp->ofname)) {
         rp->create_status = CF_ERROR;
      } else {
         rp->create_status = CF_CREATED;
      }
      break;
   case FT_DELETED:
      Jmsg(ctx, M_INFO, 0, _("gfapi-fd: Original file %s have been deleted: type=%d\n"), rp->ofname, rp->type);
      rp->create_status = CF_SKIP;
      break;
   default:
      Jmsg(ctx, M_ERROR, 0, _("gfapi-fd: Unknown file type %d; not restored: %s\n"), rp->type, rp->ofname);
      rp->create_status = CF_ERROR;
      break;
   }

bail_out:
   return bRC_OK;
}

static bRC setFileAttributes(bpContext *ctx, struct restore_pkt *rp)
{
   int status;
   struct timespec times[2];
   plugin_ctx *p_ctx = (plugin_ctx *)ctx->pContext;

   if (!p_ctx) {
      return bRC_Error;
   }

   /*
    * Restore uid and gid.
    */
   status = glfs_lchown(p_ctx->glfs, rp->ofname, rp->statp.st_uid, rp->statp.st_gid);
   if (status != 0) {
      berrno be;

      Jmsg(ctx, M_ERROR, "gfapi-fd: glfs_lchown(%s) failed: %s\n", rp->ofname, be.bstrerror());
      return bRC_Error;
   }

   /*
    * Restore mode.
    */
   status = glfs_chmod(p_ctx->glfs, rp->ofname, rp->statp.st_mode);
   if (status != 0) {
      berrno be;

      Jmsg(ctx, M_ERROR, "gfapi-fd: glfs_chmod(%s) failed: %s\n", rp->ofname, be.bstrerror());
      return bRC_Error;
   }

   /*
    * Restore access and modification times.
    */
   times[0].tv_sec = rp->statp.st_atime;
   times[0].tv_nsec = 0;
   times[1].tv_sec = rp->statp.st_mtime;
   times[1].tv_nsec = 0;

   status = glfs_lutimens(p_ctx->glfs, rp->ofname, times);
   if (status != 0) {
      berrno be;

      Jmsg(ctx, M_ERROR, "gfapi-fd: glfs_lutimens(%s) failed: %s\n", rp->ofname, be.bstrerror());
      return bRC_Error;
   }

   return bRC_OK;
}

/*
 * When using Incremental dump, all previous dumps are necessary
 */
static bRC checkFile(bpContext *ctx, char *fname)
{
   plugin_ctx *p_ctx = (plugin_ctx *)ctx->pContext;

   if (!p_ctx) {
      return bRC_Error;
   }

   return bRC_OK;
}

/*
 * Acls are saved using extended attributes.
 */
static const char *xattr_acl_skiplist[3] = {
   "system.posix_acl_access",
   "system.posix_acl_default",
   NULL
};

static inline uint32_t serialize_acl_stream(POOL_MEM *buf, uint32_t expected_serialize_len, uint32_t offset,
                                            const char *acl_name, uint32_t acl_name_length,
                                            char *xattr_value, uint32_t xattr_value_length)
{
   ser_declare;
   uint32_t content_length;
   char *buffer;

   /*
    * Make sure the serialized stream fits in the poolmem buffer.
    * We allocate some more to be sure the stream is gonna fit.
    */
   buf->check_size(offset + expected_serialize_len + 10);

   buffer = buf->c_str() + offset;
   ser_begin(buffer, expected_serialize_len + 10);

   /*
    * Encode the ACL name including the \0
    */
   ser_uint32(acl_name_length + 1);
   ser_bytes(acl_name, acl_name_length + 1);

   /*
    * Encode the actual ACL data as stored as XATTR.
    */
   ser_uint32(xattr_value_length);
   ser_bytes(xattr_value, xattr_value_length);

   ser_end(buffer, expected_serialize_len + 10);
   content_length = ser_length(buffer);

   return offset + content_length;
}

static bRC getAcl(bpContext *ctx, acl_pkt *ap)
{
   bool skip_xattr, abort_retrieval;
   int current_size;
   int32_t xattr_value_length;
   uint32_t content_length = 0;
   uint32_t expected_serialize_len;
   POOL_MEM xattr_value(PM_MESSAGE),
            serialized_acls(PM_MESSAGE);
   plugin_ctx *p_ctx = (plugin_ctx *)ctx->pContext;

   if (!p_ctx) {
      return bRC_Error;
   }

   abort_retrieval = false;
   for (int cnt = 0; xattr_acl_skiplist[cnt] != NULL; cnt++) {
      skip_xattr = false;
      while (1) {
         current_size = xattr_value.max_size();
         xattr_value_length = glfs_lgetxattr(p_ctx->glfs, ap->fname, xattr_acl_skiplist[cnt],
                                             xattr_value.c_str(), current_size);
         if (xattr_value_length < 0) {
            berrno be;

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
               Jmsg(ctx, M_ERROR, "gfapi-fd: glfs_lgetxattr(%s) failed: %s\n", ap->fname, be.bstrerror());
               return bRC_Error;
            }
         }

         /*
          * Retrieved the xattr so break the loop.
          */
         break;
      }

      if (abort_retrieval) {
         break;
      }

      if (skip_xattr) {
         continue;
      }

      /*
       * Serialize the data.
       */
      expected_serialize_len = strlen(xattr_acl_skiplist[cnt]) + xattr_value_length + 4;
      content_length = serialize_acl_stream(&serialized_acls, expected_serialize_len, content_length,
                                            xattr_acl_skiplist[cnt], strlen(xattr_acl_skiplist[cnt]),
                                            xattr_value.c_str(), xattr_value_length);
   }

   if (content_length > 0) {
      ap->content = (char *)malloc(content_length);
      memcpy(ap->content, serialized_acls.c_str(), content_length);
      ap->content_length = content_length;
   }

   return bRC_OK;
}

static bRC setAcl(bpContext *ctx, acl_pkt *ap)
{
   int status;
   unser_declare;
   uint32_t acl_name_length;
   uint32_t xattr_value_length;
   POOL_MEM xattr_value(PM_MESSAGE),
            acl_name(PM_MESSAGE);

   plugin_ctx *p_ctx = (plugin_ctx *)ctx->pContext;

   if (!p_ctx) {
      return bRC_Error;
   }

   unser_begin(ap->content, ap->content_length);
   while (unser_length(ap->content) < ap->content_length) {
      unser_uint32(acl_name_length);

      /*
       * Decode the ACL name including the \0
       */
      acl_name.check_size(acl_name_length);
      unser_bytes(acl_name.c_str(), acl_name_length);

      unser_uint32(xattr_value_length);

      /*
       * Decode the actual ACL data as stored as XATTR.
       */
      xattr_value.check_size(xattr_value_length);
      unser_bytes(xattr_value.c_str(), xattr_value_length);

      status = glfs_lsetxattr(p_ctx->glfs, ap->fname, acl_name.c_str(),
                              xattr_value.c_str(), xattr_value_length, 0);
      if (status < 0) {
         berrno be;

         Jmsg(ctx, M_ERROR, "gfapi-fd: glfs_lsetxattr(%s) failed: %s\n", ap->fname, be.bstrerror());
         return bRC_Error;
      }
   }

   unser_end(ap->content, ap->content_length);

   return bRC_OK;
}

static bRC getXattr(bpContext *ctx, xattr_pkt *xp)
{
   char *bp;
   bool skip_xattr;
   int status, current_size;
   int32_t xattr_value_length;
   POOL_MEM xattr_value(PM_MESSAGE);
   plugin_ctx *p_ctx = (plugin_ctx *)ctx->pContext;

   if (!p_ctx) {
      return bRC_Error;
   }

   /*
    * See if we need to retrieve the xattr list.
    */
   if (!p_ctx->processing_xattr) {
      while (1) {
         current_size = sizeof_pool_memory(p_ctx->xattr_list);
         status = glfs_llistxattr(p_ctx->glfs, xp->fname, p_ctx->xattr_list, current_size);
         if (status < 0) {
            berrno be;

            switch (errno) {
#if defined(ENOTSUP) || defined(EOPNOTSUPP)
#if defined(ENOTSUP)
            case ENOTSUP:
#endif
#if defined(EOPNOTSUPP) && EOPNOTSUPP != ENOTSUP
            case EOPNOTSUPP:
#endif
               return bRC_OK;
#endif
            case ERANGE:
               /*
                * Not enough room in buffer double its size and retry.
                */
               p_ctx->xattr_list = check_pool_memory_size(p_ctx->xattr_list, current_size * 2);
               continue;
            default:
               Jmsg(ctx, M_ERROR, "gfapi-fd: glfs_llistxattr(%s) failed: %s\n", xp->fname, be.bstrerror());
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
      p_ctx->xattr_list = check_pool_memory_size(p_ctx->xattr_list, status + 1);
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
      if (bit_is_set(FO_ACL, p_ctx->flags)) {
         for (int cnt = 0; xattr_acl_skiplist[cnt] != NULL; cnt++) {
            if (bstrcmp(p_ctx->next_xattr_name, xattr_acl_skiplist[cnt])) {
               skip_xattr = true;
               break;
            }
         }
      }

      if (!skip_xattr) {
         current_size = xattr_value.max_size();
         xattr_value_length = glfs_lgetxattr(p_ctx->glfs, xp->fname, p_ctx->next_xattr_name,
                                             xattr_value.c_str(), current_size);
         if (xattr_value_length < 0) {
            berrno be;

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
               skip_xattr = true;
               break;
#endif
            case ERANGE:
               /*
                * Not enough room in buffer double its size and retry.
                */
               xattr_value.check_size(current_size * 2);
               continue;
            default:
               Jmsg(ctx, M_ERROR, "gfapi-fd: glfs_lgetxattr(%s) failed: %s\n", xp->fname, be.bstrerror());
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
      xp->name = bstrdup(p_ctx->next_xattr_name);
      xp->name_length = strlen(xp->name) + 1;
      xp->value = (char *)malloc(xattr_value_length);
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

static bRC setXattr(bpContext *ctx, xattr_pkt *xp)
{
   int status;
   plugin_ctx *p_ctx = (plugin_ctx *)ctx->pContext;

   if (!p_ctx) {
      return bRC_Error;
   }

   status = glfs_lsetxattr(p_ctx->glfs, xp->fname, xp->name, xp->value, xp->value_length, 0);
   if (status < 0) {
      berrno be;

      Jmsg(ctx, M_ERROR, "gfapi-fd: glfs_lsetxattr(%s) failed: %s\n", xp->fname, be.bstrerror());
      return bRC_Error;
   }

   return bRC_OK;
}
