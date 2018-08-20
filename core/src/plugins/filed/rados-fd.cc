/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2014-2016 Planets Communications B.V.
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
/**
 * @file
 * CEPH rados plugin for the Bareos File Daemon
 */
#include "include/bareos.h"
#include "fd_plugins.h"
#include "fd_common.h"

#include <rados/librados.h>

namespace filedaemon {

static const int debuglevel = 150;

#define PLUGIN_LICENSE      "Bareos AGPLv3"
#define PLUGIN_AUTHOR       "Marco van Wieringen"
#define PLUGIN_DATE         "February 2016"
#define PLUGIN_VERSION      "2"
#define PLUGIN_DESCRIPTION  "Bareos CEPH rados File Daemon Plugin"
#define PLUGIN_USAGE        "rados:conffile=<ceph_conf_file>:namespace=<name_space>:clientid=<client_id>:clustername=<clustername>:username=<username>:snapshotname=<snapshot_name>:"

/**
 * Use for versions lower then 0.68.0 of the API the old format and otherwise the new one.
 */
#if LIBRADOS_VERSION_CODE < 17408
#define DEFAULT_CLIENTID "admin"
#else
#define DEFAULT_CLUSTERNAME "ceph"
#define DEFAULT_USERNAME "client.admin"
#endif

/**
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
static bRC setup_backup(bpContext *ctx, void *value);
static bRC setup_restore(bpContext *ctx, void *value);
static bRC end_restore_job(bpContext *ctx, void *value);

/**
 * Pointers to Bareos functions
 */
static bFuncs *bfuncs = NULL;
static bInfo  *binfo = NULL;

/**
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

/**
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

/**
 * Plugin private context
 */
struct plugin_ctx {
   int32_t backup_level;
   utime_t since;
   char *plugin_options;
   uint32_t JobId;
   char *rados_conffile;
   char *rados_clientid;
   char *rados_clustername;
   char *rados_username;
   char *rados_poolname;
   char *rados_namespace;
   char *rados_snapshotname;
   bool cluster_initialized;
   const char *object_name;
   uint64_t object_size;
   time_t object_mtime;
   POOLMEM *next_filename;
   rados_t cluster;
   rados_ioctx_t ioctx;
   rados_snap_t snap_id;
   rados_list_ctx_t list_iterator;
   rados_xattrs_iter_t xattr_iterator;
   boffset_t offset;
};

/**
 * This defines the arguments that the plugin parser understands.
 */
enum plugin_argument_type {
   argument_none,
   argument_conffile,
   argument_poolname,
   argument_clientid,
   argument_clustername,
   argument_username,
   argument_namespace,
   argument_snapshotname
};

struct plugin_argument {
   const char *name;
   enum plugin_argument_type type;
};

static plugin_argument plugin_arguments[] = {
   { "conffile", argument_conffile },
   { "poolname", argument_poolname },
   { "clientid", argument_clientid },
   { "clustername", argument_clustername },
   { "username", argument_username },
   { "namespace", argument_namespace },
   { "snapshotname", argument_snapshotname },
   { NULL, argument_none }
};

#ifdef __cplusplus
extern "C" {
#endif

/**
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

/**
 * External entry point to unload the plugin
 */
bRC DLL_IMP_EXP unloadPlugin()
{
   return bRC_OK;
}

#ifdef __cplusplus
}
#endif

/**
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

   p_ctx->next_filename = GetPoolMemory(PM_FNAME);
   bfuncs->getBareosValue(ctx, bVarJobId, (void *)&p_ctx->JobId);

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

/**
 * Free a plugin instance, i.e. release our private storage
 */
static bRC freePlugin(bpContext *ctx)
{
   plugin_ctx *p_ctx = (plugin_ctx *)ctx->pContext;
   if (!p_ctx) {
      return bRC_Error;
   }

   Dmsg(ctx, debuglevel, "rados-fd: entering freePlugin\n");

   if (p_ctx->snap_id) {
      rados_ioctx_snap_remove(p_ctx->ioctx, p_ctx->rados_snapshotname);
      p_ctx->snap_id = 0;
   }

   if (p_ctx->cluster_initialized) {
      rados_shutdown(p_ctx->cluster);
      p_ctx->cluster_initialized = false;
   }

   FreePoolMemory(p_ctx->next_filename);

   if (p_ctx->rados_snapshotname) {
      free(p_ctx->rados_snapshotname);
   }

   if (p_ctx->rados_namespace) {
      free(p_ctx->rados_namespace);
   }

   if (p_ctx->rados_poolname) {
      free(p_ctx->rados_poolname);
   }

   if (p_ctx->rados_clientid) {
      free(p_ctx->rados_clientid);
   }

   if (p_ctx->rados_clustername) {
      free(p_ctx->rados_clustername);
   }

   if (p_ctx->rados_username) {
      free(p_ctx->rados_username);
   }

   if (p_ctx->rados_conffile) {
      free(p_ctx->rados_conffile);
   }

   if (p_ctx->plugin_options) {
      free(p_ctx->plugin_options);
   }

   free(p_ctx);
   p_ctx = NULL;

   Dmsg(ctx, debuglevel, "rados-fd: leaving freePlugin\n");

   return bRC_OK;
}

/**
 * Return some plugin value (none defined)
 */
static bRC getPluginValue(bpContext *ctx, pVariable var, void *value)
{
   return bRC_OK;
}

/**
 * Set a plugin value (none defined)
 */
static bRC setPluginValue(bpContext *ctx, pVariable var, void *value)
{
   return bRC_OK;
}

/**
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
      Jmsg(ctx, M_FATAL, "rados-fd: unknown event=%d\n", event->eventType);
      Dmsg(ctx, debuglevel, "rados-fd: unknown event=%d\n", event->eventType);
      retval = bRC_Error;
      break;
   }

   return retval;
}

/**
 * Get the next object to backup.
 * - Get the next objectname from the list iterator.
 * - Check using AcceptFile if it matches the fileset.
 */
static bRC get_next_object_to_backup(bpContext *ctx)
{
   int status;
   struct save_pkt sp;
   plugin_ctx *p_ctx = (plugin_ctx *)ctx->pContext;

   while (1) {
#if defined(HAVE_RADOS_NAMESPACES) && defined(HAVE_RADOS_NOBJECTS_LIST)
      status = rados_nobjects_list_next(p_ctx->list_iterator, &p_ctx->object_name, NULL, NULL);
#else
      status = rados_objects_list_next(p_ctx->list_iterator, &p_ctx->object_name, NULL);
#endif
      if (status < 0) {
         BErrNo be;

         switch (status) {
         case -ENOENT:
#if defined(HAVE_RADOS_NAMESPACES) && defined(HAVE_RADOS_NOBJECTS_LIST)
            rados_nobjects_list_close(p_ctx->list_iterator);
#else
            rados_objects_list_close(p_ctx->list_iterator);
#endif
            p_ctx->list_iterator = NULL;
            return bRC_OK;
         default:
#if defined(HAVE_RADOS_NAMESPACES) && defined(HAVE_RADOS_NOBJECTS_LIST)
            Jmsg(ctx, M_ERROR, "rados-fd: rados_nobjects_list_next() failed: %s\n", be.bstrerror(-status));
#else
            Jmsg(ctx, M_ERROR, "rados-fd: rados_objects_list_next() failed: %s\n", be.bstrerror(-status));
#endif
            return bRC_Error;
         }
      } else {
         Mmsg(p_ctx->next_filename, "%s/%s", p_ctx->rados_poolname, p_ctx->object_name);
      }

      /*
       * See if we accept this file under the currently loaded fileset.
       */
      memset(&sp, 0, sizeof(sp));
      sp.pkt_size = sizeof(sp);
      sp.pkt_end = sizeof(sp);
      sp.fname = p_ctx->next_filename;
      sp.statp.st_mode = 0700 | S_IFREG;

      if (bfuncs->AcceptFile(ctx, &sp) == bRC_Skip) {
         continue;
      }

      status = rados_stat(p_ctx->ioctx, p_ctx->object_name,
                          &p_ctx->object_size, &p_ctx->object_mtime);
      if (status < 0) {
         BErrNo be;

         Jmsg(ctx, M_ERROR, "rados-fd: rados_stat(%s) failed: %s\n", p_ctx->object_name, be.bstrerror(-status));
         return bRC_Error;
      }

      /*
       * If we made it here we have the next object to backup.
       */
      break;
   }

   return bRC_More;
}

/**
 * Start the backup of a specific file
 */
static bRC startBackupFile(bpContext *ctx, struct save_pkt *sp)
{
   plugin_ctx *p_ctx = (plugin_ctx *)ctx->pContext;

   if (!p_ctx) {
      return bRC_Error;
   }

   sp->fname = p_ctx->next_filename;
   sp->statp.st_mode = 0700 | S_IFREG;
   sp->statp.st_mtime = p_ctx->object_mtime;
   sp->statp.st_ctime = sp->statp.st_mtime;
   sp->statp.st_atime = sp->statp.st_mtime;
   sp->statp.st_blksize = 4096;
   sp->statp.st_size = p_ctx->object_size;
   sp->statp.st_blocks = (uint32_t)(sp->statp.st_size + 4095) / 4096;
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
         sp->type = FT_NOCHG;
         break;
      default:
         sp->type = FT_REG;
         break;
      }
      break;
   default:
      sp->type = FT_REG;
      break;
   }

   return bRC_OK;
}

/**
 * Done with backup of this file
 */
static bRC endBackupFile(bpContext *ctx)
{
   plugin_ctx *p_ctx = (plugin_ctx *)ctx->pContext;

   if (!p_ctx || !p_ctx->list_iterator) {
      return bRC_Error;
   }

   return get_next_object_to_backup(ctx);
}

/**
 * Strip any backslashes in the string.
 */
static inline void StripBackSlashes(char *value)
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

/**
 * Only set destination to value when it has no previous setting.
 */
static inline void SetStringIfNull(char **destination, char *value)
{
   if (!*destination) {
      *destination = bstrdup(value);
      StripBackSlashes(*destination);
   }
}

/**
 * Always set destination to value and clean any previous one.
 */
static inline void SetString(char **destination, char *value)
{
   if (*destination) {
      free(*destination);
   }

   *destination = bstrdup(value);
   StripBackSlashes(*destination);
}

/**
 * Parse the plugin definition passed in.
 *
 * The definition is in this form:
 *
 * rados:conffile=<ceph_conf_file>:namespace=<name_space>:clientid=<client_id>:clustername=<clustername>:username=<username>:snapshotname=<snapshot_name>:
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

   keep_existing = (p_ctx->plugin_options) ? true : false;

   /*
    * Parse the plugin definition.
    * Make a private copy of the whole string.
    */
   plugin_definition = bstrdup((char *)value);

   bp = strchr(plugin_definition, ':');
   if (!bp) {
      Jmsg(ctx, M_FATAL, "rados-fd: Illegal plugin definition %s\n", plugin_definition);
      Dmsg(ctx, debuglevel, "rados-fd: Illegal plugin definition %s\n", plugin_definition);
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
         Jmsg(ctx, M_FATAL, "rados-fd: Illegal argument %s without value\n", argument);
         Dmsg(ctx, debuglevel, "rados-fd: Illegal argument %s without value\n", argument);
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
            char **str_destination = NULL;

            switch (plugin_arguments[i].type) {
            case argument_conffile:
               str_destination = &p_ctx->rados_conffile;
               break;
            case argument_clientid:
               str_destination = &p_ctx->rados_clientid;
               break;
            case argument_clustername:
               str_destination = &p_ctx->rados_clustername;
               break;
            case argument_username:
               str_destination = &p_ctx->rados_username;
               break;
            case argument_poolname:
               str_destination = &p_ctx->rados_poolname;
               break;
            case argument_namespace:
               str_destination = &p_ctx->rados_namespace;
               break;
            case argument_snapshotname:
               str_destination = &p_ctx->rados_snapshotname;
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
         Jmsg(ctx, M_FATAL, "rados-fd: Illegal argument %s with value %s in plugin definition\n", argument, argument_value);
         Dmsg(ctx, debuglevel, "rados-fd: Illegal argument %s with value %s in plugin definition\n", argument, argument_value);
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
 * Connect via RADOS protocol to a CEPH cluster.
 */
static bRC connect_to_rados(bpContext *ctx)
{
   int status;
#if LIBRADOS_VERSION_CODE >= 17408
   uint64_t rados_flags = 0;
#endif
   plugin_ctx *p_ctx = (plugin_ctx *)ctx->pContext;

   /*
    * See if we need to initialize the cluster connection.
    */
   if (!p_ctx->cluster_initialized) {
#if LIBRADOS_VERSION_CODE < 17408
      if (!p_ctx->rados_clientid) {
         p_ctx->rados_clientid = bstrdup(DEFAULT_CLIENTID);
      }

      status = rados_create(&p_ctx->cluster, p_ctx->rados_clientid);
#else
      if (!p_ctx->rados_clustername) {
         p_ctx->rados_clustername = bstrdup(DEFAULT_CLUSTERNAME);
      }

      if (!p_ctx->rados_username) {
         /*
          * See if this uses the old clientid.
          */
         if (p_ctx->rados_clientid) {
            PoolMem temp;

            Mmsg(temp, "client.%s", p_ctx->rados_clientid);
            p_ctx->rados_username = bstrdup(temp.c_str());
         } else {
            p_ctx->rados_username = bstrdup(DEFAULT_USERNAME);
         }
      }

      status = rados_create2(&p_ctx->cluster, p_ctx->rados_clustername, p_ctx->rados_username, rados_flags);
#endif
      if (status < 0) {
         BErrNo be;

         Jmsg(ctx, M_ERROR, "rados-fd: rados_create() failed: %s\n", be.bstrerror(-status));
         return bRC_Error;
      }

      status = rados_conf_read_file(p_ctx->cluster, p_ctx->rados_conffile);
      if (status < 0) {
         BErrNo be;

         Jmsg(ctx, M_ERROR, "rados-fd: rados_conf_read_file(%s) failed: %s\n",
              p_ctx->rados_conffile, be.bstrerror(-status));
         return bRC_Error;
      }

      status = rados_connect(p_ctx->cluster);
      if (status < 0) {
         BErrNo be;

         Jmsg(ctx, M_ERROR, "rados-fd: rados_connect() failed: %s\n", be.bstrerror(-status));
         rados_shutdown(p_ctx->cluster);
         return bRC_Error;
      }

      p_ctx->cluster_initialized = true;
   }

   /*
    * See if we need to initialize the IO context.
    */
   if (!p_ctx->ioctx) {
      status = rados_ioctx_create(p_ctx->cluster, p_ctx->rados_poolname, &p_ctx->ioctx);
      if (status < 0) {
         BErrNo be;

         Jmsg(ctx, M_ERROR, "rados-fd: rados_ioctx_create(%s) failed: %s\n",
              p_ctx->rados_poolname, be.bstrerror(-status));
         rados_shutdown(p_ctx->cluster);
         p_ctx->cluster_initialized = false;
         return bRC_Error;
      }
   }

   return bRC_OK;
}

/**
 * Generic setup for performing a backup.
 */
static bRC setup_backup(bpContext *ctx, void *value)
{
   int status;
   plugin_ctx *p_ctx = (plugin_ctx *)ctx->pContext;

   if (!p_ctx || !value) {
      return bRC_Error;
   }

   if (connect_to_rados(ctx) != bRC_OK) {
      return bRC_Error;
   }

   /*
    * Create a snapshot and use it for consistent reading.
    */
   if (!p_ctx->rados_snapshotname) {
      PoolMem snapshotname(PM_NAME);

      Mmsg(snapshotname, "bareos_backup_%ld", p_ctx->JobId);
      p_ctx->rados_snapshotname = bstrdup(snapshotname.c_str());
   }

   status = rados_ioctx_snap_create(p_ctx->ioctx, p_ctx->rados_snapshotname);
   if (status < 0) {
      BErrNo be;

      Jmsg(ctx, M_ERROR, "rados-fd: rados_ioctx_snap_create(%s) failed: %s\n",
           p_ctx->rados_snapshotname, be.bstrerror(-status));
      goto bail_out;
   }

   status = rados_ioctx_snap_lookup(p_ctx->ioctx, p_ctx->rados_snapshotname, &p_ctx->snap_id);
   if (status < 0) {
      BErrNo be;

      Jmsg(ctx, M_ERROR, "rados-fd: rados_ioctx_snap_lookup(%s) failed: %s\n",
           p_ctx->rados_snapshotname, be.bstrerror(-status));
      goto bail_out;
   }

   rados_ioctx_snap_set_read(p_ctx->ioctx, p_ctx->snap_id);

#if defined(HAVE_RADOS_NAMESPACES) && defined(LIBRADOS_ALL_NSPACES)
   if (!p_ctx->rados_namespace || Bstrcasecmp(p_ctx->rados_namespace, "all")) {
      rados_ioctx_set_namespace(p_ctx->ioctx, LIBRADOS_ALL_NSPACES);
   } else {
      rados_ioctx_set_namespace(p_ctx->ioctx, p_ctx->rados_namespace);
   }
#endif

   /*
    * See if we need to initialize an list iterator.
    */
   if (!p_ctx->list_iterator) {
#if defined(HAVE_RADOS_NAMESPACES) && defined(HAVE_RADOS_NOBJECTS_LIST)
      status = rados_nobjects_list_open(p_ctx->ioctx, &p_ctx->list_iterator);
#else
      status = rados_objects_list_open(p_ctx->ioctx, &p_ctx->list_iterator);
#endif
      if (status < 0) {
         BErrNo be;

#if defined(HAVE_RADOS_NAMESPACES) && defined(HAVE_RADOS_NOBJECTS_LIST)
         Jmsg(ctx, M_ERROR, "rados-fd: rados_nobjects_list_open() failed: %s\n", be.bstrerror(-status));
#else
         Jmsg(ctx, M_ERROR, "rados-fd: rados_objects_list_open() failed: %s\n", be.bstrerror(-status));
#endif
         goto bail_out;
      }

      return get_next_object_to_backup(ctx);
   }

   return bRC_OK;

bail_out:
   if (p_ctx->snap_id) {
      rados_ioctx_snap_remove(p_ctx->ioctx, p_ctx->rados_snapshotname);
      p_ctx->snap_id = 0;
   }

   if (p_ctx->cluster_initialized) {
      rados_shutdown(p_ctx->cluster);
      p_ctx->cluster_initialized = false;
   }

   return bRC_Error;
}

/**
 * Generic setup for performing a restore.
 */
static bRC setup_restore(bpContext *ctx, void *value)
{
   plugin_ctx *p_ctx = (plugin_ctx *)ctx->pContext;

   if (!p_ctx || !value) {
      return bRC_Error;
   }

   if (connect_to_rados(ctx) != bRC_OK) {
      return bRC_Error;
   }

   return bRC_OK;
}

/**
 * Bareos is calling us to do the actual I/O
 */
static bRC pluginIO(bpContext *ctx, struct io_pkt *io)
{
   int io_count;
   plugin_ctx *p_ctx = (plugin_ctx *)ctx->pContext;

   if (!p_ctx) {
      return bRC_Error;
   }

   switch(io->func) {
   case IO_OPEN:
      p_ctx->offset = 0;
      io->status = 0;
      io->io_errno = 0;
      break;
   case IO_READ:
      io_count = rados_read(p_ctx->ioctx, p_ctx->object_name, io->buf, io->count, p_ctx->offset);
      if (io_count >= 0) {
         p_ctx->offset += io_count;
         io->status = io_count;
         io->io_errno = 0;
      } else {
         io->io_errno = -io_count;
         goto bail_out;
      }
      break;
   case IO_WRITE:
      io_count = rados_write(p_ctx->ioctx, p_ctx->object_name, io->buf, io->count, p_ctx->offset);
#if LIBRADOS_VERSION_CODE <= 17408
      if (io_count >= 0) {
         p_ctx->offset += io_count;
         io->status = io_count;
#else
      if (io_count == 0) {
         p_ctx->offset += io->count;
         io->status = io->count;
#endif
         io->io_errno = 0;
      } else {
         io->io_errno = -io_count;
         goto bail_out;
      }
      break;
   case IO_CLOSE:
      p_ctx->offset = 0;
      io->status = 0;
      io->io_errno = 0;
      break;
   case IO_SEEK:
      Jmsg(ctx, M_ERROR, "rados-fd: Illegal Seek request on rados device.");
      Dmsg(ctx, debuglevel, "rados-fd: Illegal Seek request on rados device.");
      io->io_errno = EINVAL;
      goto bail_out;
   }

   return bRC_OK;

bail_out:
   io->lerror = 0;
   io->win32 = false;
   io->status = -1;

   return bRC_Error;
}

/**
 * See if we need to do any postprocessing after the restore.
 */
static bRC end_restore_job(bpContext *ctx, void *value)
{
   bRC retval = bRC_OK;
   plugin_ctx *p_ctx = (plugin_ctx *)ctx->pContext;

   if (!p_ctx) {
      return bRC_Error;
   }

   Dmsg(ctx, debuglevel, "rados-fd: entering end_restore_job\n");

   Dmsg(ctx, debuglevel, "rados-fd: leaving end_restore_job\n");

   return retval;
}

/**
 * Bareos is notifying us that a plugin name string was found,
 * and passing us the plugin command, so we can prepare for a restore.
 */
static bRC startRestoreFile(bpContext *ctx, const char *cmd)
{
   return bRC_OK;
}

/**
 * Bareos is notifying us that the plugin data has terminated,
 * so the restore for this particular file is done.
 */
static bRC endRestoreFile(bpContext *ctx)
{
   return bRC_OK;
}

/**
 * This is called during restore to create the file (if necessary) We must return in rp->create_status:
 *
 *  CF_ERROR    -- error
 *  CF_SKIP     -- skip processing this file
 *  CF_EXTRACT  -- extract the file (i.e.call i/o routines)
 *  CF_CREATED  -- created, but no content to extract (typically directories)
 */
static bRC createFile(bpContext *ctx, struct restore_pkt *rp)
{
   char *bp;
   int status;
   bool exists = false;
   plugin_ctx *p_ctx = (plugin_ctx *)ctx->pContext;

   if (!p_ctx) {
      return bRC_Error;
   }

   /*
    * Filename is in the form <pool_name>/<object_name>
    */
   PmStrcpy(p_ctx->next_filename, rp->ofname);
   bp = strrchr(p_ctx->next_filename, '/');
   if (!bp) {
      rp->create_status = CF_SKIP;
      goto bail_out;
   }
   p_ctx->object_name = ++bp;

   rp->create_status = CF_EXTRACT;

   status = rados_stat(p_ctx->ioctx, p_ctx->object_name, &p_ctx->object_size, &p_ctx->object_mtime);
   if (status < 0) {
      goto bail_out;
   } else {
      exists = true;

      switch (rp->replace) {
      case REPLACE_IFNEWER:
         if (rp->statp.st_mtime <= p_ctx->object_mtime) {
            Jmsg(ctx, M_INFO, 0, _("rados-fd: File skipped. Not newer: %s\n"), rp->ofname);
            rp->create_status = CF_SKIP;
            goto bail_out;
         }
         break;
      case REPLACE_IFOLDER:
         if (rp->statp.st_mtime >= p_ctx->object_mtime) {
            Jmsg(ctx, M_INFO, 0, _("rados-fd: File skipped. Not older: %s\n"), rp->ofname);
            rp->create_status = CF_SKIP;
            goto bail_out;
         }
         break;
      case REPLACE_NEVER:
         Jmsg(ctx, M_INFO, 0, _("rados-fd: File skipped. Already exists: %s\n"), rp->ofname);
         rp->create_status = CF_SKIP;
         goto bail_out;
      case REPLACE_ALWAYS:
         break;
      }
   }

   switch (rp->type) {
   case FT_REG:                       /* Regular file */
      /*
       * See if object already exists then we need to unlink it.
       */
      if (exists) {
         status = rados_remove(p_ctx->ioctx, p_ctx->object_name);
         if (status < 0) {
            BErrNo be;

            Jmsg(ctx, M_ERROR, "rados-fd: rados_remove(%s) failed: %s\n",
                 p_ctx->object_name, be.bstrerror(-status));
            goto bail_out;
         }
      }
      break;
   case FT_DELETED:
      Jmsg(ctx, M_INFO, 0, _("rados-fd: Original file %s have been deleted: type=%d\n"), rp->ofname, rp->type);
      rp->create_status = CF_SKIP;
      break;
   default:
      Jmsg(ctx, M_ERROR, 0, _("rados-fd: Unknown file type %d; not restored: %s\n"), rp->type, rp->ofname);
      rp->create_status = CF_ERROR;
      break;
   }

bail_out:
   return bRC_OK;
}

static bRC setFileAttributes(bpContext *ctx, struct restore_pkt *rp)
{
   return bRC_OK;
}

/**
 * When using Incremental dump, all previous dumps are necessary
 */
static bRC checkFile(bpContext *ctx, char *fname)
{
   return bRC_OK;
}

static bRC getAcl(bpContext *ctx, acl_pkt *ap)
{
   return bRC_OK;
}

static bRC setAcl(bpContext *ctx, acl_pkt *ap)
{
   return bRC_OK;
}

static bRC getXattr(bpContext *ctx, xattr_pkt *xp)
{
   int status;
   size_t xattr_value_length;
   const char *xattr_name, *xattr_value;
   plugin_ctx *p_ctx = (plugin_ctx *)ctx->pContext;

   if (!p_ctx) {
      return bRC_Error;
   }

   /*
    * See if we need to open a new xattr iterator.
    */
   if (!p_ctx->xattr_iterator) {
      status = rados_getxattrs(p_ctx->ioctx, p_ctx->object_name, &p_ctx->xattr_iterator);
      if (status < 0) {
         BErrNo be;

         Jmsg(ctx, M_ERROR, "rados-fd: rados_getxattrs(%s) failed: %s\n",
              p_ctx->object_name, be.bstrerror(-status));
         goto bail_out;
      }
   }

   /*
    * Get the next xattr value.
    */
   status = rados_getxattrs_next(p_ctx->xattr_iterator, &xattr_name, &xattr_value, &xattr_value_length);
   if (status < 0) {
      BErrNo be;

      Jmsg(ctx, M_ERROR, "rados-fd: rados_getxattrs_next(%s) failed: %s\n",
           p_ctx->object_name, be.bstrerror(-status));
      goto bail_out;
   }

   /*
    * Got last xattr ?
    */
   if (!xattr_name) {
      rados_getxattrs_end(p_ctx->xattr_iterator);
      p_ctx->xattr_iterator = NULL;
      return bRC_OK;
   }

   /*
    * Create a new xattr and return the data in allocated memory.
    * Caller will free this for us.
    */
   xp->name = bstrdup(xattr_name);
   xp->name_length = strlen(xattr_name) + 1;
   xp->value = (char *)malloc(xattr_value_length);
   memcpy(xp->value, xattr_value, xattr_value_length);
   xp->value_length = xattr_value_length;

   return bRC_More;

bail_out:
   return bRC_Error;
}

static bRC setXattr(bpContext *ctx, xattr_pkt *xp)
{
   int status;
   const char *bp;
   plugin_ctx *p_ctx = (plugin_ctx *)ctx->pContext;

   if (!p_ctx) {
      return bRC_Error;
   }

   bp = strrchr(xp->fname, '/');
   if (!bp) {
      goto bail_out;
   }

   p_ctx->object_name = ++bp;
   status = rados_setxattr(p_ctx->ioctx, p_ctx->object_name, xp->name, xp->value, xp->value_length);
   if (status < 0) {
      BErrNo be;

      Jmsg(ctx, M_ERROR, "rados-fd: rados_setxattr(%s) set xattr %s failed: %s\n",
           p_ctx->object_name, xp->name, be.bstrerror(-status));
      goto bail_out;
   }

   return bRC_OK;

bail_out:
   return bRC_Error;
}
} /* namespace filedaemon */
