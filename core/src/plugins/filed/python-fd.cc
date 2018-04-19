/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2011-2015 Planets Communications B.V.
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
 * Marco van Wieringen, August 2012
 */
/**
 * @file
 * Python plugin for the Bareos File Daemon
 */
#define BUILD_PLUGIN

#include "include/bareos.h"
#include "fd_plugins.h"
#include "fd_common.h"

#undef _POSIX_C_SOURCE
#include <Python.h>

#if (PY_VERSION_HEX <  0x02060000)
#error "Need at least Python version 2.6 or newer"
#endif

static const int debuglevel = 150;

#define PLUGIN_LICENSE      "Bareos AGPLv3"
#define PLUGIN_AUTHOR       "Marco van Wieringen"
#define PLUGIN_DATE         "May 2014"
#define PLUGIN_VERSION      "3"
#define PLUGIN_DESCRIPTION  "Python File Daemon Plugin"
#define PLUGIN_USAGE        "python:module_path=<path-to-python-modules>:module_name=<python-module-to-load>:..."

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
static bRC parse_plugin_definition(bpContext *ctx, void *value, PoolMem &plugin_options);

static void PyErrorHandler(bpContext *ctx, int msgtype);
static bRC PyLoadModule(bpContext *ctx, void *value);
static bRC PyParsePluginDefinition(bpContext *ctx, void *value);
static bRC PyGetPluginValue(bpContext *ctx, pVariable var, void *value);
static bRC PySetPluginValue(bpContext *ctx, pVariable var, void *value);
static bRC PyHandlePluginEvent(bpContext *ctx, bEvent *event, void *value);
static bRC PyStartBackupFile(bpContext *ctx, struct save_pkt *sp);
static bRC PyEndBackupFile(bpContext *ctx);
static bRC PyPluginIO(bpContext *ctx, struct io_pkt *io);
static bRC PyStartRestoreFile(bpContext *ctx, const char *cmd);
static bRC PyEndRestoreFile(bpContext *ctx);
static bRC PyCreateFile(bpContext *ctx, struct restore_pkt *rp);
static bRC PySetFileAttributes(bpContext *ctx, struct restore_pkt *rp);
static bRC PyCheckFile(bpContext *ctx, char *fname);
static bRC PyGetAcl(bpContext *ctx, acl_pkt *ap);
static bRC PySetAcl(bpContext *ctx, acl_pkt *ap);
static bRC PyGetXattr(bpContext *ctx, xattr_pkt *xp);
static bRC PySetXattr(bpContext *ctx, xattr_pkt *xp);
static bRC PyRestoreObjectData(bpContext *ctx, struct restore_object_pkt *rop);
static bRC PyHandleBackupFile(bpContext *ctx, struct save_pkt *sp);

/**
 * Pointers to Bareos functions
 */
static bFuncs *bfuncs = NULL;
static bInfo *binfo = NULL;

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
   int32_t backup_level;              /* Backup level e.g. Full/Differential/Incremental */
   utime_t since;                     /* Since time for Differential/Incremental */
   bool python_loaded;                /* Plugin has python module loaded ? */
   bool python_path_set;              /* Python plugin search path is set ? */
   char *plugin_options;              /* Plugin Option string */
   char *module_path;                 /* Plugin Module Path */
   char *module_name;                 /* Plugin Module Name */
   char *fname;                       /* Next filename to save */
   char *link;                        /* Target symlink points to */
   char *object_name;                 /* Restore Object Name */
   char *object;                      /* Restore Object Content */
   PyThreadState *interpreter;        /* Python interpreter for this instance of the plugin */
   PyObject *pInstance;               /* Python Module instance */
   PyObject *pModule;                 /* Python Module entry point */
   PyObject *pDict;                   /* Python Dictionary */
   PyObject *bpContext;               /* Python representation of plugin context */
};

#include "python-fd.h"

/**
 * We don't actually use this but we need it to tear down the
 * final python interpreter on unload of the plugin. Each instance of
 * the plugin get its own interpreter.
 */
static PyThreadState *mainThreadState;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Plugin called here when it is first loaded
 */
bRC DLL_IMP_EXP loadPlugin(bInfo *lbinfo,
                           bFuncs *lbfuncs,
                           genpInfo **pinfo,
                           pFuncs **pfuncs)
{
   bfuncs = lbfuncs;                  /* Set Bareos funct pointers */
   binfo  = lbinfo;

   *pinfo  = &pluginInfo;             /* Return pointer to our info */
   *pfuncs = &pluginFuncs;            /* Return pointer to our functions */

   /*
    * Setup Python
    */
   Py_InitializeEx(0);
   PyEval_InitThreads();
   mainThreadState = PyEval_SaveThread();

   return bRC_OK;
}

/**
 * Plugin called here when it is unloaded, normally when Bareos is going to exit.
 */
bRC DLL_IMP_EXP unloadPlugin()
{
   /*
    * Terminate Python
    */
   PyEval_RestoreThread(mainThreadState);
   Py_Finalize();

   return bRC_OK;
}

#ifdef __cplusplus
}
#endif

/**
 * Called here to make a new instance of the plugin -- i.e. when
 * a new Job is started. There can be multiple instances of
 * each plugin that are running at the same time.  Your
 * plugin instance must be thread safe and keep its own
 * local data.
 */
static bRC newPlugin(bpContext *ctx)
{
   struct plugin_ctx *p_ctx;

   p_ctx = (struct plugin_ctx *)malloc(sizeof(struct plugin_ctx));
   if (!p_ctx) {
      return bRC_Error;
   }
   memset(p_ctx, 0, sizeof(struct plugin_ctx));
   ctx->pContext = (void *)p_ctx;        /* set our context pointer */

   /*
    * For each plugin instance we instantiate a new Python interpreter.
    */
   PyEval_AcquireLock();
   p_ctx->interpreter = Py_NewInterpreter();
   PyEval_ReleaseThread(p_ctx->interpreter);

   /*
    * Always register some events the python plugin itself can register
    * any other events it is interested in.
    */
   bfuncs->registerBareosEvents(ctx,
                                9,
                                bEventLevel,
                                bEventSince,
                                bEventNewPluginOptions,
                                bEventPluginCommand,
                                bEventJobStart,
                                bEventRestoreCommand,
                                bEventEstimateCommand,
                                bEventBackupCommand,
                                bEventRestoreObject);

   return bRC_OK;
}

/**
 * Release everything concerning a particular instance of a
 * plugin. Normally called when the Job terminates.
 */
static bRC freePlugin(bpContext *ctx)
{
   struct plugin_ctx *p_ctx = (struct plugin_ctx *)ctx->pContext;

   if (!p_ctx) {
      return bRC_Error;
   }

   if (p_ctx->plugin_options) {
      free(p_ctx->plugin_options);
   }

   if (p_ctx->module_path) {
      free(p_ctx->module_path);
   }

   if (p_ctx->module_name) {
      free(p_ctx->module_name);
   }

   if (p_ctx->fname) {
      free(p_ctx->fname);
   }

   if (p_ctx->link) {
      free(p_ctx->link);
   }

   if (p_ctx->object_name) {
      free(p_ctx->object_name);
   }

   if (p_ctx->object) {
      free(p_ctx->object);
   }

   /*
    * Stop any sub interpreter started per plugin instance.
    */
   PyEval_AcquireThread(p_ctx->interpreter);

   /*
    * Do python cleanup calls.
    */
   if (p_ctx->bpContext) {
      Py_DECREF(p_ctx->bpContext);
   }

   if (p_ctx->pModule) {
      Py_DECREF(p_ctx->pModule);
   }

   Py_EndInterpreter(p_ctx->interpreter);
   PyEval_ReleaseLock();

   free(p_ctx);
   ctx->pContext = NULL;

   return bRC_OK;
}

/**
 * Called by core code to get a variable from the plugin.
 * Not currently used.
 */
static bRC getPluginValue(bpContext *ctx, pVariable var, void *value)
{
   struct plugin_ctx *p_ctx = (struct plugin_ctx *)ctx->pContext;
   bRC retval = bRC_Error;

   if (!p_ctx) {
      goto bail_out;
   }

   PyEval_AcquireThread(p_ctx->interpreter);
   retval = PyGetPluginValue(ctx, var, value);
   PyEval_ReleaseThread(p_ctx->interpreter);

bail_out:
   return retval;
}

/**
 * Called by core code to set a plugin variable.
 * Not currently used.
 */
static bRC setPluginValue(bpContext *ctx, pVariable var, void *value)
{
   struct plugin_ctx *p_ctx = (struct plugin_ctx *)ctx->pContext;
   bRC retval = bRC_Error;

   if (!p_ctx) {
      return bRC_Error;
   }

   PyEval_AcquireThread(p_ctx->interpreter);
   retval = PySetPluginValue(ctx, var, value);
   PyEval_ReleaseThread(p_ctx->interpreter);

   return retval;
}

/**
 * Called by Bareos when there are certain events that the
 * plugin might want to know.  The value depends on the event.
 */
static bRC handlePluginEvent(bpContext *ctx, bEvent *event, void *value)
{
   bRC retval = bRC_Error;
   bool event_dispatched = false;
   PoolMem plugin_options(PM_FNAME);
   plugin_ctx *p_ctx = (plugin_ctx *)ctx->pContext;

   if (!p_ctx) {
      goto bail_out;
   }

   /*
    * First handle some events internally before calling python if it
    * want to do some special handling on the event triggered.
    */
   switch (event->eventType) {
   case bEventLevel:
      p_ctx->backup_level = (int64_t)value;
      break;
   case bEventSince:
      p_ctx->since = (int64_t)value;
      break;
   case bEventBackupCommand:
     /*
      * Fall-through wanted
      */
   case bEventRestoreCommand:
     /*
      * Fall-through wanted
      */
   case bEventEstimateCommand:
     /*
      * Fall-through wanted
      */
   case bEventPluginCommand:
      event_dispatched = true;
      retval = parse_plugin_definition(ctx, value, plugin_options);
      break;
   case bEventNewPluginOptions:
      /*
       * Free any previous value.
       */
      if (p_ctx->plugin_options) {
         free(p_ctx->plugin_options);
         p_ctx->plugin_options = NULL;
      }

      event_dispatched = true;
      retval = parse_plugin_definition(ctx, value, plugin_options);

      /*
       * Save that we got a plugin override.
       */
      p_ctx->plugin_options = bstrdup((char *)value);
      break;
   case bEventRestoreObject: {
      struct restore_object_pkt *rop;

      rop = (struct restore_object_pkt *)value;

      /*
       * Only use the plugin definition of a restore object if we
       * didn't get any other plugin definition from some other source before.
       */
      if (!p_ctx->python_loaded) {
         if (rop && *rop->plugin_name) {
            event_dispatched = true;
            retval = parse_plugin_definition(ctx, rop->plugin_name, plugin_options);
         }
      }
      break;
   }
   default:
      break;
   }

   /*
    * See if we have been triggered in the previous switch if not we have to
    * always dispatch the event. If we already processed the event internally
    * we only do a dispatch to the python entry point when that internal processing
    * was successfull (e.g. retval == bRC_OK).
    */
   if (!event_dispatched || retval == bRC_OK) {
      PyEval_AcquireThread(p_ctx->interpreter);

      /*
       * Now dispatch the event to Python.
       * First the calls that need special handling.
       */
      switch (event->eventType) {
      case bEventBackupCommand:
        /*
         * Fall-through wanted
         */
      case bEventRestoreCommand:
        /*
         * Fall-through wanted
         */
      case bEventEstimateCommand:
        /*
         * Fall-through wanted
         */
      case bEventPluginCommand:
        /*
         * Fall-through wanted
         */
      case bEventNewPluginOptions:
         /*
          * See if we already loaded the Python modules.
          */
         if (!p_ctx->python_loaded) {
            retval = PyLoadModule(ctx, plugin_options.c_str());
         }

         /*
          * Only try to call when the loading succeeded.
          */
         if (retval == bRC_OK) {
            retval = PyParsePluginDefinition(ctx, plugin_options.c_str());
         }
         break;
      case bEventRestoreObject: {
         struct restore_object_pkt *rop;

         rop = (struct restore_object_pkt *)value;
         if (!rop) {
            /*
             * If rop == NULL this means we got the last restore object.
             * No need to call into python so just return.
             */
            retval = bRC_OK;
         } else {
            /*
             * See if we already loaded the Python modules.
             */
            if (!p_ctx->python_loaded && *rop->plugin_name) {
               retval = PyLoadModule(ctx, plugin_options.c_str());

               /*
                * Only try to call when the loading succeeded.
                */
               if (retval == bRC_OK) {
                  retval = PyParsePluginDefinition(ctx, plugin_options.c_str());
                  if (retval == bRC_OK) {
                     retval = PyRestoreObjectData(ctx, rop);
                  }
               }
            } else {
               retval = PyRestoreObjectData(ctx, rop);
            }
         }
         break;
      }
      case bEventHandleBackupFile:
         retval = PyHandleBackupFile(ctx, (struct save_pkt *)value);
         break;
      default:
         /*
          * Handle the generic events e.g. the ones which are just passed on.
          * We only try to call Python when we loaded the right module until
          * that time we pretend the call succeeded.
          */
         if (p_ctx->python_loaded) {
            retval = PyHandlePluginEvent(ctx, event, value);
         } else {
            retval = bRC_OK;
         }
         break;
      }

      PyEval_ReleaseThread(p_ctx->interpreter);
   }

bail_out:
   return retval;
}

/**
 * Called when starting to backup a file. Here the plugin must
 * return the "stat" packet for the directory/file and provide
 * certain information so that Bareos knows what the file is.
 * The plugin can create "Virtual" files by giving them a
 * name that is not normally found on the file system.
 */
static bRC startBackupFile(bpContext *ctx, struct save_pkt *sp)
{
   bRC retval = bRC_Error;
   struct plugin_ctx *p_ctx = (struct plugin_ctx *)ctx->pContext;

   if (!p_ctx) {
      goto bail_out;
   }

   PyEval_AcquireThread(p_ctx->interpreter);
   retval = PyStartBackupFile(ctx, sp);
   PyEval_ReleaseThread(p_ctx->interpreter);

   /*
    * For Incremental and Differential backups use checkChanges method to
    * see if we need to backup this file.
    */
   switch (p_ctx->backup_level) {
   case L_INCREMENTAL:
   case L_DIFFERENTIAL:
      /*
       * If the plugin didn't set a save_time but we have a since time
       * from the bEventSince event we use that as basis for the actual
       * save_time to check.
       */
      if (sp->save_time == 0 && p_ctx->since) {
         sp->save_time = p_ctx->since;
      }

      switch (bfuncs->checkChanges(ctx, sp)) {
      case bRC_Seen:
         switch (sp->type) {
         case FT_DIRBEGIN:
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

bail_out:
   return retval;
}

/**
 * Done backing up a file.
 */
static bRC endBackupFile(bpContext *ctx)
{
   bRC retval = bRC_Error;
   struct plugin_ctx *p_ctx = (struct plugin_ctx *)ctx->pContext;

   if (!p_ctx) {
      goto bail_out;
   }

   PyEval_AcquireThread(p_ctx->interpreter);
   retval = PyEndBackupFile(ctx);
   PyEval_ReleaseThread(p_ctx->interpreter);

bail_out:
   return retval;
}

/**
 * Do actual I/O. Bareos calls this after startBackupFile
 * or after startRestoreFile to do the actual file
 * input or output.
 */
static bRC pluginIO(bpContext *ctx, struct io_pkt *io)
{
   bRC retval = bRC_Error;
   struct plugin_ctx *p_ctx = (struct plugin_ctx *)ctx->pContext;

   if (!p_ctx) {
      goto bail_out;
   }

   if (!p_ctx->python_loaded) {
      goto bail_out;
   }

   PyEval_AcquireThread(p_ctx->interpreter);
   retval = PyPluginIO(ctx, io);
   PyEval_ReleaseThread(p_ctx->interpreter);

bail_out:
   return retval;
}

/**
 * Start restore of a file.
 */
static bRC startRestoreFile(bpContext *ctx, const char *cmd)
{
   bRC retval = bRC_Error;
   struct plugin_ctx *p_ctx = (struct plugin_ctx *)ctx->pContext;

   if (!p_ctx) {
      goto bail_out;
   }

   PyEval_AcquireThread(p_ctx->interpreter);
   retval = PyStartRestoreFile(ctx, cmd);
   PyEval_ReleaseThread(p_ctx->interpreter);

bail_out:
   return retval;
}

/**
 * Done restoring a file.
 */
static bRC endRestoreFile(bpContext *ctx)
{
   bRC retval = bRC_Error;
   struct plugin_ctx *p_ctx = (struct plugin_ctx *)ctx->pContext;

   if (!p_ctx) {
      goto bail_out;
   }

   PyEval_AcquireThread(p_ctx->interpreter);
   retval = PyEndRestoreFile(ctx);
   PyEval_ReleaseThread(p_ctx->interpreter);

bail_out:
   return retval;
}

/**
 * Called here to give the plugin the information needed to
 * re-create the file on a restore.  It basically gets the
 * stat packet that was created during the backup phase.
 * This data is what is needed to create the file, but does
 * not contain actual file data.
 */
static bRC createFile(bpContext *ctx, struct restore_pkt *rp)
{
   bRC retval = bRC_Error;
   struct plugin_ctx *p_ctx = (struct plugin_ctx *)ctx->pContext;

   if (!p_ctx) {
      goto bail_out;
   }

   PyEval_AcquireThread(p_ctx->interpreter);
   retval = PyCreateFile(ctx, rp);
   PyEval_ReleaseThread(p_ctx->interpreter);

bail_out:
   return retval;
}

/**
 * Called after the file has been restored. This can be used to
 * set directory permissions, ...
 */
static bRC setFileAttributes(bpContext *ctx, struct restore_pkt *rp)
{
   bRC retval = bRC_Error;
   struct plugin_ctx *p_ctx = (struct plugin_ctx *)ctx->pContext;

   if (!p_ctx) {
      goto bail_out;
   }

   PyEval_AcquireThread(p_ctx->interpreter);
   retval = PySetFileAttributes(ctx, rp);
   PyEval_ReleaseThread(p_ctx->interpreter);

bail_out:
   return retval;
}

/**
 * When using Incremental dump, all previous dumps are necessary
 */
static bRC checkFile(bpContext *ctx, char *fname)
{
   bRC retval = bRC_Error;
   struct plugin_ctx *p_ctx = (struct plugin_ctx *)ctx->pContext;

   if (!p_ctx) {
      goto bail_out;
   }

   if (!p_ctx->python_loaded) {
      return bRC_OK;
   }

   PyEval_AcquireThread(p_ctx->interpreter);
   retval = PyCheckFile(ctx, fname);
   PyEval_ReleaseThread(p_ctx->interpreter);

bail_out:
   return retval;
}

/**
 */
static bRC getAcl(bpContext *ctx, acl_pkt *ap)
{
   bRC retval = bRC_Error;
   struct plugin_ctx *p_ctx = (struct plugin_ctx *)ctx->pContext;

   if (!p_ctx) {
      goto bail_out;
   }

   PyEval_AcquireThread(p_ctx->interpreter);
   retval = PyGetAcl(ctx, ap);
   PyEval_ReleaseThread(p_ctx->interpreter);

bail_out:
   return retval;
}

/**
 */
static bRC setAcl(bpContext *ctx, acl_pkt *ap)
{
   bRC retval = bRC_Error;
   struct plugin_ctx *p_ctx = (struct plugin_ctx *)ctx->pContext;

   if (!p_ctx) {
      goto bail_out;
   }

   PyEval_AcquireThread(p_ctx->interpreter);
   retval = PySetAcl(ctx, ap);
   PyEval_ReleaseThread(p_ctx->interpreter);

bail_out:
   return retval;
}

/**
 */
static bRC getXattr(bpContext *ctx, xattr_pkt *xp)
{
   bRC retval = bRC_Error;
   struct plugin_ctx *p_ctx = (struct plugin_ctx *)ctx->pContext;

   if (!p_ctx) {
      goto bail_out;
   }

   PyEval_AcquireThread(p_ctx->interpreter);
   retval = PyGetXattr(ctx, xp);
   PyEval_ReleaseThread(p_ctx->interpreter);

bail_out:
   return retval;
}

/**
 */
static bRC setXattr(bpContext *ctx, xattr_pkt *xp)
{
   bRC retval = bRC_Error;
   struct plugin_ctx *p_ctx = (struct plugin_ctx *)ctx->pContext;

   if (!p_ctx) {
      goto bail_out;
   }

   PyEval_AcquireThread(p_ctx->interpreter);
   retval = PySetXattr(ctx, xp);
   PyEval_ReleaseThread(p_ctx->interpreter);

bail_out:
   return retval;
}

/**
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

/**
 * Parse a boolean value e.g. check if its yes or true anything else translates to false.
 */
static inline bool parse_boolean(const char *argument_value)
{
   if (bstrcasecmp(argument_value, "yes") ||
       bstrcasecmp(argument_value, "true")) {
      return true;
   } else {
      return false;
   }
}

/**
 * Only set destination to value when it has no previous setting.
 */
static inline void set_string_if_null(char **destination, char *value)
{
   if (!*destination) {
      *destination = bstrdup(value);
      strip_back_slashes(*destination);
   }
}

/**
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

/**
 * Parse the plugin definition passed in.
 *
 * The definition is in this form:
 *
 * python:module_path=<path>:module_name=<python_module_name>:...
 */
static bRC parse_plugin_definition(bpContext *ctx, void *value, PoolMem &plugin_options)
{
   bool found;
   int i, cnt;
   bool keep_existing;
   PoolMem plugin_definition(PM_FNAME);
   char *bp, *argument, *argument_value;
   plugin_ctx *p_ctx = (plugin_ctx *)ctx->pContext;

   if (!value) {
      return bRC_Error;
   }

   /*
    * Skip this plugin when getting plugin definition "*all*"
    * This allows to restore a Windows Backup on a Linux FD with
    * Python Plugins enabled.
    */
   if (bstrcmp((char *)value, "*all*")) {
      Dmsg(ctx, debuglevel, "python-fd: Got plugin definition %s, skipping to ignore\n", (char *)value);
      return bRC_Skip;
   }

   keep_existing = (p_ctx->plugin_options) ? true : false;

   /*
    * Parse the plugin definition.
    * Make a private copy of the whole string.
    */
   if (!p_ctx->python_loaded && p_ctx->plugin_options) {
      int len;

      /*
       * We got some option string which got pushed before we actual were able to send
       * it to the python module as the entry point was not instantiated. So we prepend
       * that now in the option string and append the new option string with the first
       * argument being the pluginname removed as that is already part of the other
       * plugin option string.
       */
      len = strlen(p_ctx->plugin_options);
      pm_strcpy(plugin_definition, p_ctx->plugin_options);

      bp = strchr((char *)value, ':');
      if (!bp) {
         Jmsg(ctx, M_FATAL, "python-fd: Illegal plugin definition %s\n", (char *)value);
         Dmsg(ctx, debuglevel, "python-fd: Illegal plugin definition %s\n", (char *)value);
         goto bail_out;
      }

      /*
       * See if option string end with ':'
       */
      if (p_ctx->plugin_options[len - 1] != ':') {
         pm_strcat(plugin_definition, (char *)bp);
      } else {
         pm_strcat(plugin_definition, (char *)bp + 1);
      }
   } else {
      pm_strcpy(plugin_definition, (char *)value);
   }

   bp = strchr(plugin_definition.c_str(), ':');
   if (!bp) {
      Jmsg(ctx, M_FATAL, "python-fd: Illegal plugin definition %s\n", plugin_definition.c_str());
      Dmsg(ctx, debuglevel, "python-fd: Illegal plugin definition %s\n", plugin_definition.c_str());
      goto bail_out;
   }

   /*
    * Skip the first ':'
    */
   bp++;

   cnt = 0;
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
         Jmsg(ctx, M_FATAL, "python-fd: Illegal argument %s without value\n", argument);
         Dmsg(ctx, debuglevel, "python-fd: Illegal argument %s without value\n", argument);
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

      found = false;
      for (i = 0; plugin_arguments[i].name; i++) {
         if (bstrcasecmp(argument, plugin_arguments[i].name)) {
            char **str_destination = NULL;
            bool *bool_destination = NULL;

            switch (plugin_arguments[i].type) {
            case argument_module_path:
               str_destination = &p_ctx->module_path;
               break;
            case argument_module_name:
               str_destination = &p_ctx->module_name;
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
             * Set any boolean variable.
             */
            if (bool_destination) {
               *bool_destination = parse_boolean(argument_value);
            }

            /*
             * When we have a match break the loop.
             */
            found = true;
            break;
         }
      }

      /*
       * If we didn't consume this parameter we add it to the plugin_options list.
       */
      if (!found) {
         PoolMem option(PM_FNAME);

         if (cnt) {
            Mmsg(option, ":%s=%s", argument, argument_value);
            pm_strcat(plugin_options, option.c_str());
         } else {
            Mmsg(option, "%s=%s", argument, argument_value);
            pm_strcat(plugin_options, option.c_str());
         }
         cnt++;
      }
   }

   if (cnt > 0) {
      pm_strcat(plugin_options, ":");
   }

   return bRC_OK;

bail_out:
   return bRC_Error;
}

/**
 * Work around API changes in Python versions.
 * These function abstract the storage and retrieval of the bpContext
 * which is passed to the Python methods and which the method can pass
 * back and which allow the callback function to understand what bpContext
 * its talking about.
 */
#if ((PY_VERSION_HEX < 0x02070000) || \
     ((PY_VERSION_HEX >= 0x03000000) && \
      (PY_VERSION_HEX < 0x03010000)))
/**
 * Python version before 2.7 and 3.0.
 */
static PyObject *PyCreatebpContext(bpContext *ctx)
{
   /*
    * Setup a new CObject which holds the bpContext structure used here internally.
    */
   return PyCObject_FromVoidPtr((void *)ctx, NULL);
}

static bpContext *PyGetbpContext(PyObject *pyCtx)
{
   return (bpContext *)PyCObject_AsVoidPtr(pyCtx);
}
#else
/**
 * Python version after 2.6 and 3.1.
 */
static PyObject *PyCreatebpContext(bpContext *ctx)
{
   /*
    * Setup a new Capsule which holds the bpContext structure used here internally.
    */
   return PyCapsule_New((void *)ctx, "bareos.bpContext", NULL);
}

static bpContext *PyGetbpContext(PyObject *pyCtx)
{
   return (bpContext *)PyCapsule_GetPointer(pyCtx, "bareos.bpContext");
}
#endif

/**
 * Convert a return value into a bRC enum value.
 */
static inline bRC conv_python_retval(PyObject *pRetVal)
{
   return (bRC)PyInt_AsLong(pRetVal);
}

/**
 * Convert a return value from bRC enum value into Python Object.
 */
static inline PyObject *conv_retval_python(bRC retval)
{
   return (PyObject *)PyInt_FromLong((int)retval);
}

/**
 * Handle a Python error.
 *
 * Python equivalent:
 *
 * import traceback, sys
 * return "".join(traceback.format_exception(sys.exc_type,
 *    sys.exc_value, sys.exc_traceback))
 */
static void PyErrorHandler(bpContext *ctx, int msgtype)
{
   PyObject *type, *value, *traceback;
   PyObject *tracebackModule;
   char *error_string;

   PyErr_Fetch(&type, &value, &traceback);

   tracebackModule = PyImport_ImportModule("traceback");
   if (tracebackModule != NULL) {
      PyObject *tbList, *emptyString, *strRetval;

      tbList = PyObject_CallMethod(tracebackModule,
                                   (char *)"format_exception",
                                   (char *)"OOO",
                                   type,
                                   value == NULL ? Py_None : value,
                                   traceback == NULL ? Py_None : traceback);

      emptyString = PyString_FromString("");
      strRetval = PyObject_CallMethod(emptyString,
                                      (char *)"join",
                                      (char *)"O", tbList);

      error_string = bstrdup(PyString_AsString(strRetval));

      Py_DECREF(tbList);
      Py_DECREF(emptyString);
      Py_DECREF(strRetval);
      Py_DECREF(tracebackModule);
   } else {
      error_string = bstrdup("Unable to import traceback module.");
   }

   Py_DECREF(type);
   Py_XDECREF(value);
   Py_XDECREF(traceback);

   Dmsg(ctx, debuglevel, "python-fd: %s\n", error_string);
   if (msgtype) {
      Jmsg(ctx, msgtype, "python-fd: %s\n", error_string);
   }

   free(error_string);
}

/**
 * Initial load of the Python module.
 *
 * Based on the parsed plugin options we set some prerequisits like the
 * module path and the module to load. We also load the dictionary used
 * for looking up the Python methods.
 */
static bRC PyLoadModule(bpContext *ctx, void *value)
{
   bRC retval = bRC_Error;
   struct plugin_ctx *p_ctx = (struct plugin_ctx *)ctx->pContext;
   PyObject *sysPath,
            *mPath,
            *pName,
            *pFunc;

   /*
    * See if we already setup the python search path.
    */
   if (!p_ctx->python_path_set) {
      /*
       * Extend the Python search path with the given module_path.
       */
      if (p_ctx->module_path) {
         sysPath = PySys_GetObject((char *)"path");
         mPath = PyString_FromString(p_ctx->module_path);
         PyList_Append(sysPath, mPath);
         Py_DECREF(mPath);
         p_ctx->python_path_set = true;
      }
   }

   /*
    * See if we already setup the module structure.
    */
   if (!p_ctx->pInstance) {
      /*
       * Make our callback methods available for Python.
       */
      p_ctx->pInstance = Py_InitModule("bareosfd", BareosFDMethods);

      /*
       * Fill in the slots of PyRestoreObject
       */
      PyRestoreObjectType.tp_new = PyType_GenericNew;
      if (PyType_Ready(&PyRestoreObjectType) < 0) {
         goto cleanup;
      }

      /*
       * Fill in the slots of PyStatPacket
       */
      PyStatPacketType.tp_new = PyType_GenericNew;
      if (PyType_Ready(&PyStatPacketType) < 0) {
         goto cleanup;
      }

      /*
       * Fill in the slots of PySavePacket
       */
      PySavePacketType.tp_new = PyType_GenericNew;
      if (PyType_Ready(&PySavePacketType) < 0) {
         goto cleanup;
      }

      /*
       * Fill in the slots of PyRestorePacket
       */
      PyRestorePacketType.tp_new = PyType_GenericNew;
      if (PyType_Ready(&PyRestorePacketType) < 0) {
         goto cleanup;
      }

      /*
       * Fill in the slots of PyIoPacketType
       */
      PyIoPacketType.tp_new = PyType_GenericNew;
      if (PyType_Ready(&PyIoPacketType) < 0) {
         goto cleanup;
      }

      /*
       * Fill in the slots of PyAclPacketType
       */
      PyAclPacketType.tp_new = PyType_GenericNew;
      if (PyType_Ready(&PyAclPacketType) < 0) {
         goto cleanup;
      }

      /*
       * Fill in the slots of PyXattrPacketType
       */
      PyXattrPacketType.tp_new = PyType_GenericNew;
      if (PyType_Ready(&PyXattrPacketType) < 0) {
         goto cleanup;
      }

      /*
       * Add the types to the module
       */
      Py_INCREF(&PyRestoreObjectType);
      PyModule_AddObject(p_ctx->pInstance, "RestoreObject", (PyObject *)&PyRestoreObjectType);

      Py_INCREF(&PyStatPacketType);
      PyModule_AddObject(p_ctx->pInstance, "StatPacket", (PyObject *)&PyStatPacketType);

      Py_INCREF(&PySavePacketType);
      PyModule_AddObject(p_ctx->pInstance, "SavePacket", (PyObject *)&PySavePacketType);

      Py_INCREF(&PyRestorePacketType);
      PyModule_AddObject(p_ctx->pInstance, "RestorePacket", (PyObject *)&PyRestorePacketType);

      Py_INCREF(&PyIoPacketType);
      PyModule_AddObject(p_ctx->pInstance, "IoPacket", (PyObject *)&PyIoPacketType);

      Py_INCREF(&PyAclPacketType);
      PyModule_AddObject(p_ctx->pInstance, "AclPacket", (PyObject *)&PyAclPacketType);

      Py_INCREF(&PyXattrPacketType);
      PyModule_AddObject(p_ctx->pInstance, "XattrPacket", (PyObject *)&PyXattrPacketType);
   }

   /*
    * Try to load the Python module by name.
    */
   if (p_ctx->module_name) {
      Dmsg(ctx, debuglevel, "python-fd: Trying to load module with name %s\n", p_ctx->module_name);
      pName = PyString_FromString(p_ctx->module_name);
      p_ctx->pModule = PyImport_Import(pName);
      Py_DECREF(pName);

      if (!p_ctx->pModule) {
         Dmsg(ctx, debuglevel, "python-fd: Failed to load module with name %s\n", p_ctx->module_name);
         goto bail_out;
      }

      Dmsg(ctx, debuglevel, "python-fd: Successfully loaded module with name %s\n", p_ctx->module_name);

      /*
       * Get the Python dictionary for lookups in the Python namespace.
       */
      p_ctx->pDict = PyModule_GetDict(p_ctx->pModule); /* Borrowed reference */

      /*
       * Encode the bpContext so a Python method can pass it in on calling back.
       */
      p_ctx->bpContext = PyCreatebpContext(ctx);

      /*
       * Lookup the load_bareos_plugin() function in the python module.
       */
      pFunc = PyDict_GetItemString(p_ctx->pDict, "load_bareos_plugin"); /* Borrowed reference */
      if (pFunc && PyCallable_Check(pFunc)) {
         PyObject *pPluginDefinition,
                  *pRetVal;

         pPluginDefinition = PyString_FromString((char *)value);
         if (!pPluginDefinition) {
            goto bail_out;
         }

         pRetVal = PyObject_CallFunctionObjArgs(pFunc, p_ctx->bpContext, pPluginDefinition, NULL);
         Py_DECREF(pPluginDefinition);

         if (!pRetVal) {
            goto bail_out;
         } else {
            retval = conv_python_retval(pRetVal);
            Py_DECREF(pRetVal);
         }
      } else {
         Dmsg(ctx, debuglevel, "python-fd: Failed to find function named load_bareos_plugins()\n");
         goto bail_out;
      }

      /*
       * Keep track we successfully loaded.
       */
      p_ctx->python_loaded = true;
   }

   return retval;

cleanup:
   p_ctx->pInstance = NULL;

bail_out:
   if (PyErr_Occurred()) {
      PyErrorHandler(ctx, M_FATAL);
   }

   return retval;
}

/**
 * Any plugin options which are passed in are dispatched here to a Python method and it
 * can parse the plugin options. This function is also called after PyLoadModule() has
 * loaded the Python module and made sure things are operational. Normally you would only get
 * one set of plugin options but for a restore overrides can be passed in before the actual
 * plugin options are restored as part of the restore stream handling.
 */
static bRC PyParsePluginDefinition(bpContext *ctx, void *value)
{
   bRC retval = bRC_Error;
   struct plugin_ctx *p_ctx = (struct plugin_ctx *)ctx->pContext;
   PyObject *pFunc;

   /*
    * Lookup the parse_plugin_definition() function in the python module.
    */
   pFunc = PyDict_GetItemString(p_ctx->pDict, "parse_plugin_definition"); /* Borrowed reference */
   if (pFunc && PyCallable_Check(pFunc)) {
      PyObject *pPluginDefinition,
               *pRetVal;

      pPluginDefinition = PyString_FromString((char *)value);
      if (!pPluginDefinition) {
         goto bail_out;
      }

      pRetVal = PyObject_CallFunctionObjArgs(pFunc, p_ctx->bpContext, pPluginDefinition, NULL);
      Py_DECREF(pPluginDefinition);

      if (!pRetVal) {
         goto bail_out;
      } else {
         retval = conv_python_retval(pRetVal);
         Py_DECREF(pRetVal);
      }

      return retval;
   } else {
      Dmsg(ctx, debuglevel, "python-fd: Failed to find function named parse_plugin_definition()\n");
      return bRC_Error;
   }

bail_out:
   if (PyErr_Occurred()) {
      PyErrorHandler(ctx, M_FATAL);
   }

   return retval;
}

static bRC PyGetPluginValue(bpContext *ctx, pVariable var, void *value)
{
   return bRC_OK;
}

static bRC PySetPluginValue(bpContext *ctx, pVariable var, void *value)
{
   return bRC_OK;
}

static bRC PyHandlePluginEvent(bpContext *ctx, bEvent *event, void *value)
{
   bRC retval = bRC_Error;
   plugin_ctx *p_ctx = (plugin_ctx *)ctx->pContext;
   PyObject *pFunc;

   /*
    * Lookup the handle_plugin_event() function in the python module.
    */
   pFunc = PyDict_GetItemString(p_ctx->pDict, "handle_plugin_event"); /* Borrowed reference */
   if (pFunc && PyCallable_Check(pFunc)) {
      PyObject *pEventType,
               *pRetVal;

      pEventType = PyInt_FromLong(event->eventType);

      pRetVal = PyObject_CallFunctionObjArgs(pFunc, p_ctx->bpContext, pEventType, NULL);
      Py_DECREF(pEventType);

      if (!pRetVal) {
         goto bail_out;
      } else {
         retval = conv_python_retval(pRetVal);
         Py_DECREF(pRetVal);
      }
   } else {
      Dmsg(ctx, debuglevel, "python-fd: Failed to find function named handle_plugin_event()\n");
   }

   return retval;

bail_out:
   if (PyErr_Occurred()) {
      PyErrorHandler(ctx, M_FATAL);
   }

   return retval;
}

static inline PyStatPacket *NativeToPyStatPacket(struct stat *statp)
{
   PyStatPacket *pStatp = PyObject_New(PyStatPacket, &PyStatPacketType);

   if (pStatp) {
      pStatp->dev = statp->st_dev;
      pStatp->ino = statp->st_ino;
      pStatp->mode = statp->st_mode;
      pStatp->nlink = statp->st_nlink;
      pStatp->uid = statp->st_uid;
      pStatp->gid = statp->st_gid;
      pStatp->rdev = statp->st_rdev;
      pStatp->size = statp->st_size;
      pStatp->atime = statp->st_atime;
      pStatp->mtime = statp->st_mtime;
      pStatp->ctime = statp->st_ctime;
      pStatp->blksize = statp->st_blksize;
      pStatp->blocks = statp->st_blocks;
   }

   return pStatp;
}

static inline void PyStatPacketToNative(PyStatPacket *pStatp, struct stat *statp)
{
   statp->st_dev = pStatp->dev;
   statp->st_ino = pStatp->ino;
   statp->st_mode = pStatp->mode;
   statp->st_nlink = pStatp->nlink;
   statp->st_uid = pStatp->uid;
   statp->st_gid = pStatp->gid;
   statp->st_rdev = pStatp->rdev;
   statp->st_size = pStatp->size;
   statp->st_atime = pStatp->atime;
   statp->st_mtime = pStatp->mtime;
   statp->st_ctime = pStatp->ctime;
   statp->st_blksize = pStatp->blksize;
   statp->st_blocks = pStatp->blocks;
}

static inline PySavePacket *NativeToPySavePacket(struct save_pkt *sp)
{
   PySavePacket *pSavePkt = PyObject_New(PySavePacket, &PySavePacketType);

   if (pSavePkt) {
      /*
       * Initialize the Python SavePkt with the data we got passed in.
       */
      if (sp->fname) {
         pSavePkt->fname = PyString_FromString(sp->fname);
      } else {
         pSavePkt->fname = NULL;
      }

      if (sp->link) {
         pSavePkt->link = PyString_FromString(sp->link);
      } else {
         pSavePkt->link = NULL;
      }

      if (sp->statp.st_mode) {
         pSavePkt->statp = (PyObject *)NativeToPyStatPacket(&sp->statp);
      } else {
         pSavePkt->statp = NULL;
      }

      pSavePkt->type = sp->type;
      pSavePkt->flags = PyByteArray_FromStringAndSize(sp->flags, sizeof(sp->flags));
      pSavePkt->no_read = sp->no_read;
      pSavePkt->portable = sp->portable;
      pSavePkt->accurate_found = sp->accurate_found;
      pSavePkt->cmd = sp->cmd;
      pSavePkt->save_time = sp->save_time;
      pSavePkt->delta_seq = sp->delta_seq;
      pSavePkt->object_name = NULL;
      pSavePkt->object = NULL;
      pSavePkt->object_len = sp->object_len;
      pSavePkt->object_index = sp->index;
   }

   return pSavePkt;
}

static inline bool PySavePacketToNative(PySavePacket *pSavePkt, struct save_pkt *sp,
                                        struct plugin_ctx *p_ctx, bool is_options_plugin)
{
   /*
    * See if this is for an Options Plugin.
    */
   if (!is_options_plugin) {
      /*
       * Only copy back the arguments that are allowed to change.
       */
      if (pSavePkt->fname) {
         /*
          * As this has to linger as long as the backup is running we save it in our plugin context.
          */
         if (PyString_Check(pSavePkt->fname)) {
            if (p_ctx->fname) {
               free(p_ctx->fname);
            }
            p_ctx->fname = bstrdup(PyString_AsString(pSavePkt->fname));
            sp->fname = p_ctx->fname;
         }
      } else {
         goto bail_out;
      }

      /*
       * Optional field.
       */
      if (pSavePkt->link) {
         /*
          * As this has to linger as long as the backup is running we save it in our plugin context.
          */
         if (PyString_Check(pSavePkt->link)) {
            if (p_ctx->link) {
               free(p_ctx->link);
            }
            p_ctx->link = bstrdup(PyString_AsString(pSavePkt->link));
            sp->link = p_ctx->link;
         }
      }

      /*
       * Handle the stat structure.
       */
      if (pSavePkt->statp) {
         PyStatPacketToNative((PyStatPacket *)pSavePkt->statp, &sp->statp);
      } else {
         goto bail_out;
      }

      sp->type = pSavePkt->type;

      if (PyByteArray_Check(pSavePkt->flags)) {
         char *flags;

         if (PyByteArray_Size(pSavePkt->flags) != sizeof(sp->flags)) {
            goto bail_out;
         }

         if ((flags = PyByteArray_AsString(pSavePkt->flags))) {
            memcpy(sp->flags, flags, sizeof(sp->flags));
         } else {
            goto bail_out;
         }
      } else {
         goto bail_out;
      }

      /*
       * Special code for handling restore objects.
       */
      if (IS_FT_OBJECT(sp->type)) {
         /*
          * See if a proper restore object was created.
          */
         if (pSavePkt->object_len > 0) {
            /*
             * As this has to linger as long as the backup is running we save it in our plugin context.
             */
            if (pSavePkt->object_name &&
                pSavePkt->object &&
                PyString_Check(pSavePkt->object_name) &&
                PyByteArray_Check(pSavePkt->object)) {
               char *buf;

               if (p_ctx->object_name) {
                  free(p_ctx->object_name);
               }
               p_ctx->object_name = bstrdup(PyString_AsString(pSavePkt->object_name));
               sp->object_name = p_ctx->object_name;

               sp->object_len = pSavePkt->object_len;
               sp->index = pSavePkt->object_index;

               if ((buf = PyByteArray_AsString(pSavePkt->object))) {
                  if (p_ctx->object) {
                     free(p_ctx->object);
                  }
                  p_ctx->object = (char *)malloc(pSavePkt->object_len);
                  memcpy(p_ctx->object, buf, pSavePkt->object_len);
                  sp->object = p_ctx->object;
               } else {
                  goto bail_out;
               }
            } else {
               goto bail_out;
            }
         } else {
            goto bail_out;
         }
      } else {
         sp->no_read = pSavePkt->no_read;
         sp->delta_seq = pSavePkt->delta_seq;
      }
   } else {
      sp->no_read = pSavePkt->no_read;
      sp->delta_seq = pSavePkt->delta_seq;

      if (PyByteArray_Check(pSavePkt->flags)) {
         char *flags;

         if (PyByteArray_Size(pSavePkt->flags) != sizeof(sp->flags)) {
            goto bail_out;
         }

         if ((flags = PyByteArray_AsString(pSavePkt->flags))) {
            memcpy(sp->flags, flags, sizeof(sp->flags));
         } else {
            goto bail_out;
         }
      } else {
         goto bail_out;
      }
   }

   return true;

bail_out:
   return false;
}

/**
 * Called when starting to backup a file. Here the plugin must
 * return the "stat" packet for the directory/file and provide
 * certain information so that Bareos knows what the file is.
 * The plugin can create "Virtual" files by giving them a
 * name that is not normally found on the file system.
 */
static bRC PyStartBackupFile(bpContext *ctx, struct save_pkt *sp)
{
   bRC retval = bRC_Error;
   struct plugin_ctx *p_ctx = (struct plugin_ctx *)ctx->pContext;
   PyObject *pFunc;

   /*
    * Lookup the start_backup_file() function in the python module.
    */
   pFunc = PyDict_GetItemString(p_ctx->pDict, "start_backup_file"); /* Borrowed reference */
   if (pFunc && PyCallable_Check(pFunc)) {
      PySavePacket *pSavePkt;
      PyObject *pRetVal;

      pSavePkt = NativeToPySavePacket(sp);
      if (!pSavePkt) {
         goto bail_out;
      }

      pRetVal = PyObject_CallFunctionObjArgs(pFunc, p_ctx->bpContext, (PyObject *)pSavePkt, NULL);
      if (!pRetVal) {
         Py_DECREF((PyObject *)pSavePkt);
         goto bail_out;
      } else {
         retval = conv_python_retval(pRetVal);
         Py_DECREF(pRetVal);

         if (!PySavePacketToNative(pSavePkt, sp, p_ctx, false)) {
            Py_DECREF((PyObject *)pSavePkt);
            goto bail_out;
         }
         Py_DECREF((PyObject *)pSavePkt);
      }
   } else {
      Dmsg(ctx, debuglevel, "python-fd: Failed to find function named start_backup_file()\n");
   }

   return retval;

bail_out:
   if (PyErr_Occurred()) {
      PyErrorHandler(ctx, M_FATAL);
   }

   return retval;
}

/**
 * Called at the end of backing up a file for a command plugin.
 * If the plugin's work is done, it should return bRC_OK.
 * If the plugin wishes to create another file and back it up,
 * then it must return bRC_More (not yet implemented).
 */
static bRC PyEndBackupFile(bpContext *ctx)
{
   bRC retval = bRC_Error;
   struct plugin_ctx *p_ctx = (struct plugin_ctx *)ctx->pContext;
   PyObject *pFunc;

   /*
    * Lookup the end_backup_file() function in the python module.
    */
   pFunc = PyDict_GetItemString(p_ctx->pDict, "end_backup_file"); /* Borrowed reference */
   if (pFunc && PyCallable_Check(pFunc)) {
      PyObject *pRetVal;

      pRetVal = PyObject_CallFunctionObjArgs(pFunc, p_ctx->bpContext, NULL);
      if (!pRetVal) {
         goto bail_out;
      } else {
         retval = conv_python_retval(pRetVal);
      }
   } else {
      Dmsg(ctx, debuglevel, "python-fd: Failed to find function named end_backup_file()\n");
   }

   return retval;

bail_out:
   if (PyErr_Occurred()) {
      PyErrorHandler(ctx, M_FATAL);
   }

   return retval;
}

static inline PyIoPacket *NativeToPyIoPacket(struct io_pkt *io)
{
   PyIoPacket *pIoPkt = PyObject_New(PyIoPacket, &PyIoPacketType);

   if (pIoPkt) {
      /*
       * Initialize the Python IoPkt with the data we got passed in.
       */
      pIoPkt->func = io->func;
      pIoPkt->count = io->count;
      pIoPkt->flags = io->flags;
      pIoPkt->mode = io->mode;
      pIoPkt->fname = io->fname;
      pIoPkt->whence = io->whence;
      pIoPkt->offset = io->offset;
      if (io->func == IO_WRITE && io->count > 0) {
         /*
          * Only initialize the buffer with read data when we are writing and there is data.
          */
         pIoPkt->buf = PyByteArray_FromStringAndSize(io->buf, io->count);
         if (!pIoPkt->buf) {
            Py_DECREF((PyObject *)pIoPkt);
            return (PyIoPacket *)NULL;
         }
      } else {
         pIoPkt->buf = NULL;
      }

      /*
       * These must be set by the Python function but we initialize them to zero
       * to be sure they have some valid setting an not random data.
       */
      pIoPkt->io_errno = 0;
      pIoPkt->lerror = 0;
      pIoPkt->win32 = false;
      pIoPkt->status = 0;
   }

   return pIoPkt;
}

static inline bool PyIoPacketToNative(PyIoPacket *pIoPkt, struct io_pkt *io)
{
   /*
    * Only copy back the arguments that are allowed to change.
    */
   io->io_errno = pIoPkt->io_errno;
   io->lerror = pIoPkt->lerror;
   io->win32 = pIoPkt->win32;
   io->status = pIoPkt->status;
   if (io->func == IO_READ && io->status > 0) {
      /*
       * Only copy back the data when doing a read and there is data.
       */
      if (PyByteArray_Check(pIoPkt->buf)) {
         char *buf;

         if (PyByteArray_Size(pIoPkt->buf) > io->count || io->status > io->count) {
            return false;
         }

         if (!(buf = PyByteArray_AsString(pIoPkt->buf))) {
            return false;
         }
         memcpy(io->buf, buf, io->status);
      }
   }

   return true;
}

/**
 * Do actual I/O. Bareos calls this after startBackupFile
 * or after startRestoreFile to do the actual file
 * input or output.
 */
static bRC PyPluginIO(bpContext *ctx, struct io_pkt *io)
{
   bRC retval = bRC_Error;
   struct plugin_ctx *p_ctx = (struct plugin_ctx *)ctx->pContext;
   PyObject *pFunc;

   /*
    * Lookup the plugin_io() function in the python module.
    */
   pFunc = PyDict_GetItemString(p_ctx->pDict, "plugin_io"); /* Borrowed reference */
   if (pFunc && PyCallable_Check(pFunc)) {
      PyIoPacket *pIoPkt;
      PyObject *pRetVal;

      pIoPkt = NativeToPyIoPacket(io);
      if (!pIoPkt) {
         goto bail_out;
      }

      pRetVal = PyObject_CallFunctionObjArgs(pFunc, p_ctx->bpContext, (PyObject *)pIoPkt, NULL);
      if (!pRetVal) {
         Py_DECREF((PyObject *)pIoPkt);
         goto bail_out;
      } else {
         retval = conv_python_retval(pRetVal);
         Py_DECREF(pRetVal);

         if (!PyIoPacketToNative(pIoPkt, io)) {
            Py_DECREF((PyObject *)pIoPkt);
            goto bail_out;
         }
      }
      Py_DECREF((PyObject *)pIoPkt);
   } else {
      Dmsg(ctx, debuglevel, "python-fd: Failed to find function named plugin_io()\n");
   }

   return retval;

bail_out:
   if (PyErr_Occurred()) {
      PyErrorHandler(ctx, M_FATAL);
   }

   io->status = -1;

   return retval;
}

/**
 * Called when the first record is read from the Volume that was previously written by the command plugin.
 */
static bRC PyStartRestoreFile(bpContext *ctx, const char *cmd)
{
   bRC retval = bRC_Error;
   struct plugin_ctx *p_ctx = (struct plugin_ctx *)ctx->pContext;
   PyObject *pFunc;

   /*
    * Lookup the start_restore_file() function in the python module.
    */
   pFunc = PyDict_GetItemString(p_ctx->pDict, "start_restore_file"); /* Borrowed reference */
   if (pFunc && PyCallable_Check(pFunc)) {
      PyObject *pCmd,
               *pRetVal;

      pCmd = PyString_FromString(cmd);
      if (!pCmd) {
         goto bail_out;
      }

      pRetVal = PyObject_CallFunctionObjArgs(pFunc, p_ctx->bpContext, pCmd, NULL);
      Py_DECREF(pCmd);

      if (!pRetVal) {
         goto bail_out;
      } else {
         retval = conv_python_retval(pRetVal);
         Py_DECREF(pRetVal);
      }
   } else {
      Dmsg(ctx, debuglevel, "python-fd: Failed to find function named start_restore_file()\n");
   }

   return retval;

bail_out:
   if (PyErr_Occurred()) {
      PyErrorHandler(ctx, M_FATAL);
   }

   return retval;
}

/**
 * Called when a command plugin is done restoring a file.
 */
static bRC PyEndRestoreFile(bpContext *ctx)
{
   bRC retval = bRC_Error;
   struct plugin_ctx *p_ctx = (struct plugin_ctx *)ctx->pContext;
   PyObject *pFunc;

   /*
    * Lookup the end_restore_file() function in the python module.
    */
   pFunc = PyDict_GetItemString(p_ctx->pDict, "end_restore_file"); /* Borrowed reference */
   if (pFunc && PyCallable_Check(pFunc)) {
      PyObject *pRetVal;

      pRetVal = PyObject_CallFunctionObjArgs(pFunc, p_ctx->bpContext, NULL);
      if (!pRetVal) {
         goto bail_out;
      } else {
         retval = conv_python_retval(pRetVal);
      }
   } else {
      Dmsg(ctx, debuglevel, "python-fd: Failed to find function named end_restore_file()\n");
   }

   return retval;

bail_out:
   if (PyErr_Occurred()) {
      PyErrorHandler(ctx, M_FATAL);
   }

   return retval;
}

static inline PyRestorePacket *NativeToPyRestorePacket(struct restore_pkt *rp)
{
   PyRestorePacket *pRestorePacket = PyObject_New(PyRestorePacket, &PyRestorePacketType);

   if (pRestorePacket) {
      pRestorePacket->stream = rp->stream;
      pRestorePacket->data_stream = rp->data_stream;
      pRestorePacket->type = rp->type;
      pRestorePacket->file_index = rp->file_index;
      pRestorePacket->LinkFI = rp->LinkFI;
      pRestorePacket->uid = rp->uid;
      pRestorePacket->statp = (PyObject *)NativeToPyStatPacket(&rp->statp);
      pRestorePacket->attrEx = rp->attrEx;
      pRestorePacket->ofname = rp->ofname;
      pRestorePacket->olname = rp->olname;
      pRestorePacket->where = rp->where;
      pRestorePacket->RegexWhere = rp->RegexWhere;
      pRestorePacket->replace = rp->replace;
      pRestorePacket->create_status = rp->create_status;
   }

   return pRestorePacket;
}

static inline void PyRestorePacketToNative(PyRestorePacket *pRestorePacket, struct restore_pkt *rp)
{
   /*
    * Only copy back the fields that are allowed to be changed.
    */
   rp->create_status = pRestorePacket->create_status;
}

/**
 * Called for a command plugin to create a file during a Restore job before restoring the data.
 * This entry point is called before any I/O is done on the file. After this call,
 * Bareos will call pluginIO() to open the file for write.
 *
 * We must return in rp->create_status:
 *
 * CF_ERROR    -- error
 * CF_SKIP     -- skip processing this file
 * CF_EXTRACT  -- extract the file (i.e.call i/o routines)
 * CF_CREATED  -- created, but no content to extract (typically directories)
 */
static bRC PyCreateFile(bpContext *ctx, struct restore_pkt *rp)
{
   bRC retval = bRC_Error;
   struct plugin_ctx *p_ctx = (struct plugin_ctx *)ctx->pContext;
   PyObject *pFunc;

   if (!rp) {
      return bRC_Error;
   }

   /*
    * Lookup the create_file() function in the python module.
    */
   pFunc = PyDict_GetItemString(p_ctx->pDict, "create_file"); /* Borrowed reference */
   if (pFunc && PyCallable_Check(pFunc)) {
      PyRestorePacket *pRestorePacket;
      PyObject *pRetVal;

      pRestorePacket = NativeToPyRestorePacket(rp);
      if (!pRestorePacket) {
         goto bail_out;
      }

      pRetVal = PyObject_CallFunctionObjArgs(pFunc, p_ctx->bpContext, pRestorePacket, NULL);
      if (!pRetVal) {
         Py_DECREF(pRestorePacket);
         goto bail_out;
      } else {
         retval = conv_python_retval(pRetVal);
         Py_DECREF(pRetVal);

         PyRestorePacketToNative(pRestorePacket, rp);
         Py_DECREF(pRestorePacket);
      }
   } else {
      Dmsg(ctx, debuglevel, "python-fd: Failed to find function named create_file()\n");
   }

   return retval;

bail_out:
   if (PyErr_Occurred()) {
      PyErrorHandler(ctx, M_FATAL);
   }

   return retval;
}

static bRC PySetFileAttributes(bpContext *ctx, struct restore_pkt *rp)
{
   bRC retval = bRC_Error;
   struct plugin_ctx *p_ctx = (struct plugin_ctx *)ctx->pContext;
   PyObject *pFunc;

   if (!rp) {
      return bRC_Error;
   }

   /*
    * Lookup the set_file_attributes() function in the python module.
    */
   pFunc = PyDict_GetItemString(p_ctx->pDict, "set_file_attributes"); /* Borrowed reference */
   if (pFunc && PyCallable_Check(pFunc)) {
      PyRestorePacket *pRestorePacket;
      PyObject *pRetVal;

      pRestorePacket = NativeToPyRestorePacket(rp);
      if (!pRestorePacket) {
         goto bail_out;
      }

      pRetVal = PyObject_CallFunctionObjArgs(pFunc, p_ctx->bpContext, pRestorePacket, NULL);
      if (!pRetVal) {
         Py_DECREF(pRestorePacket);
         goto bail_out;
      } else {
         retval = conv_python_retval(pRetVal);
         Py_DECREF(pRetVal);
         Py_DECREF(pRestorePacket);
      }
   } else {
      Dmsg(ctx, debuglevel, "python-fd: Failed to find function named set_file_attributes()\n");
   }

   return retval;

bail_out:
   if (PyErr_Occurred()) {
      PyErrorHandler(ctx, M_FATAL);
   }

   return retval;
}

static bRC PyCheckFile(bpContext *ctx, char *fname)
{
   bRC retval = bRC_Error;
   struct plugin_ctx *p_ctx = (struct plugin_ctx *)ctx->pContext;
   PyObject *pFunc;

   if (!fname) {
      return bRC_Error;
   }

   /*
    * Lookup the check_file() function in the python module.
    */
   pFunc = PyDict_GetItemString(p_ctx->pDict, "check_file"); /* Borrowed reference */
   if (pFunc && PyCallable_Check(pFunc)) {
      PyObject *pFname,
               *pRetVal;

      pFname = PyString_FromString(fname);
      pRetVal = PyObject_CallFunctionObjArgs(pFunc, p_ctx->bpContext, pFname, NULL);
      Py_DECREF(pFname);

      if (!pRetVal) {
         goto bail_out;
      } else {
         retval = conv_python_retval(pRetVal);
         Py_DECREF(pRetVal);
      }
   } else {
      Dmsg(ctx, debuglevel, "python-fd: Failed to find function named check_file()\n");
   }

   return retval;

bail_out:
   if (PyErr_Occurred()) {
      PyErrorHandler(ctx, M_FATAL);
   }

   return retval;
}

static inline PyAclPacket *NativeToPyAclPacket(struct acl_pkt *ap)
{
   PyAclPacket *pAclPacket = PyObject_New(PyAclPacket, &PyAclPacketType);

   if (pAclPacket) {
      pAclPacket->fname = ap->fname;

      if (ap->content_length && ap->content) {
         pAclPacket->content = PyByteArray_FromStringAndSize(ap->content, ap->content_length);
      } else {
         pAclPacket->content = NULL;
      }
   }

   return pAclPacket;
}

static inline bool PyAclPacketToNative(PyAclPacket *pAclPacket, struct acl_pkt *ap)
{
   if (!pAclPacket->content) {
      return true;
   }

   if (PyByteArray_Check(pAclPacket->content)) {
      char *buf;

      ap->content_length = PyByteArray_Size(pAclPacket->content);
      if (ap->content_length <= 0 || !(buf = PyByteArray_AsString(pAclPacket->content))) {
         return false;
      }

      if (ap->content) {
         free(ap->content);
      }
      ap->content = (char *)malloc(ap->content_length);
      memcpy(ap->content, buf, ap->content_length);
   }

   return true;
}

static bRC PyGetAcl(bpContext *ctx, acl_pkt *ap)
{
   bRC retval = bRC_Error;
   struct plugin_ctx *p_ctx = (struct plugin_ctx *)ctx->pContext;
   PyObject *pFunc;

   if (!ap) {
      return bRC_Error;
   }

   /*
    * Lookup the get_acl() function in the python module.
    */
   pFunc = PyDict_GetItemString(p_ctx->pDict, "get_acl"); /* Borrowed reference */
   if (pFunc && PyCallable_Check(pFunc)) {
      PyAclPacket *pAclPkt;
      PyObject *pRetVal;

      pAclPkt = NativeToPyAclPacket(ap);
      if (!pAclPkt) {
         goto bail_out;
      }

      pRetVal = PyObject_CallFunctionObjArgs(pFunc, p_ctx->bpContext, pAclPkt, NULL);
      if (!pRetVal) {
         Py_DECREF((PyObject *)pAclPkt);
         goto bail_out;
      } else {
         retval = conv_python_retval(pRetVal);
         Py_DECREF(pRetVal);

         if (!PyAclPacketToNative(pAclPkt, ap)) {
            Py_DECREF((PyObject *)pAclPkt);
            goto bail_out;
         }
         Py_DECREF(pAclPkt);
      }
   } else {
      Dmsg(ctx, debuglevel, "python-fd: Failed to find function named get_acl()\n");
   }

   return retval;

bail_out:
   if (PyErr_Occurred()) {
      PyErrorHandler(ctx, M_FATAL);
   }

   return retval;
}

static bRC PySetAcl(bpContext *ctx, acl_pkt *ap)
{
   bRC retval = bRC_Error;
   struct plugin_ctx *p_ctx = (struct plugin_ctx *)ctx->pContext;
   PyObject *pFunc;

   if (!ap) {
      return bRC_Error;
   }

   /*
    * Lookup the set_acl() function in the python module.
    */
   pFunc = PyDict_GetItemString(p_ctx->pDict, "set_acl"); /* Borrowed reference */
   if (pFunc && PyCallable_Check(pFunc)) {
      PyAclPacket *pAclPkt;
      PyObject *pRetVal;

      pAclPkt = NativeToPyAclPacket(ap);
      if (!pAclPkt) {
         goto bail_out;
      }

      pRetVal = PyObject_CallFunctionObjArgs(pFunc, p_ctx->bpContext, pAclPkt, NULL);
      Py_DECREF(pAclPkt);

      if (!pRetVal) {
         goto bail_out;
      } else {
         retval = conv_python_retval(pRetVal);
         Py_DECREF(pRetVal);
      }
   } else {
      Dmsg(ctx, debuglevel, "python-fd: Failed to find function named set_acl()\n");
   }

   return retval;

bail_out:
   if (PyErr_Occurred()) {
      PyErrorHandler(ctx, M_FATAL);
   }

   return retval;
}

static inline PyXattrPacket *NativeToPyXattrPacket(struct xattr_pkt *xp)
{
   PyXattrPacket *pXattrPacket = PyObject_New(PyXattrPacket, &PyXattrPacketType);

   if (pXattrPacket) {
      pXattrPacket->fname = xp->fname;

      if (xp->name_length && xp->name) {
         pXattrPacket->name = PyByteArray_FromStringAndSize(xp->name, xp->name_length);
      } else {
         pXattrPacket->name = NULL;
      }
      if (xp->value_length && xp->value) {
         pXattrPacket->value = PyByteArray_FromStringAndSize(xp->value, xp->value_length);
      } else {
         pXattrPacket->value = NULL;
      }
   }

   return pXattrPacket;
}

static inline bool PyXattrPacketToNative(PyXattrPacket *pXattrPacket, struct xattr_pkt *xp)
{
   if (!pXattrPacket->name) {
      return true;
   }

   if (PyByteArray_Check(pXattrPacket->name)) {
      char *buf;

      xp->name_length = PyByteArray_Size(pXattrPacket->name);
      if (xp->name_length <= 0 || !(buf = PyByteArray_AsString(pXattrPacket->name))) {
         return false;
      }

      if (xp->name) {
         free(xp->name);
      }
      xp->name = (char *)malloc(xp->name_length);
      memcpy(xp->name, buf, xp->name_length);
   }

   if (pXattrPacket->value && PyByteArray_Check(pXattrPacket->value)) {
      char *buf;

      xp->value_length = PyByteArray_Size(pXattrPacket->value);
      if (xp->name_length <= 0 || !(buf = PyByteArray_AsString(pXattrPacket->name))) {
         return false;
      }

      if (xp->value) {
         free(xp->value);
      }
      xp->value = (char *)malloc(xp->value_length);
      memcpy(xp->value, buf, xp->value_length);
   } else {
      if (xp->value) {
         free(xp->value);
      }
      xp->value = NULL;
   }

   return true;
}

static bRC PyGetXattr(bpContext *ctx, xattr_pkt *xp)
{
   bRC retval = bRC_Error;
   struct plugin_ctx *p_ctx = (struct plugin_ctx *)ctx->pContext;
   PyObject *pFunc;

   if (!xp) {
      return bRC_Error;
   }

   /*
    * Lookup the get_xattr() function in the python module.
    */
   pFunc = PyDict_GetItemString(p_ctx->pDict, "get_xattr"); /* Borrowed reference */
   if (pFunc && PyCallable_Check(pFunc)) {
      PyXattrPacket *pXattrPkt;
      PyObject *pRetVal;

      pXattrPkt = NativeToPyXattrPacket(xp);
      if (!pXattrPkt) {
         goto bail_out;
      }

      pRetVal = PyObject_CallFunctionObjArgs(pFunc, p_ctx->bpContext, pXattrPkt, NULL);
      if (!pRetVal) {
         Py_DECREF((PyObject *)pXattrPkt);
         goto bail_out;
      } else {
         retval = conv_python_retval(pRetVal);
         Py_DECREF(pRetVal);

         if (!PyXattrPacketToNative(pXattrPkt, xp)) {
            Py_DECREF((PyObject *)pXattrPkt);
            goto bail_out;
         }
         Py_DECREF(pXattrPkt);
      }
   } else {
      Dmsg(ctx, debuglevel, "python-fd: Failed to find function named get_xattr()\n");
   }

   return retval;

bail_out:
   if (PyErr_Occurred()) {
      PyErrorHandler(ctx, M_FATAL);
   }

   return retval;
}

static bRC PySetXattr(bpContext *ctx, xattr_pkt *xp)
{
   bRC retval = bRC_Error;
   struct plugin_ctx *p_ctx = (struct plugin_ctx *)ctx->pContext;
   PyObject *pFunc;

   if (!xp) {
      return bRC_Error;
   }

   /*
    * Lookup the set_acl() function in the python module.
    */
   pFunc = PyDict_GetItemString(p_ctx->pDict, "set_xattr"); /* Borrowed reference */
   if (pFunc && PyCallable_Check(pFunc)) {
      PyXattrPacket *pXattrPkt;
      PyObject *pRetVal;

      pXattrPkt = NativeToPyXattrPacket(xp);
      if (!pXattrPkt) {
         goto bail_out;
      }

      pRetVal = PyObject_CallFunctionObjArgs(pFunc, p_ctx->bpContext, pXattrPkt, NULL);
      Py_DECREF(pXattrPkt);

      if (!pRetVal) {
         goto bail_out;
      } else {
         retval = conv_python_retval(pRetVal);
         Py_DECREF(pRetVal);
      }
   } else {
      Dmsg(ctx, debuglevel, "python-fd: Failed to find function named set_xattr()\n");
   }

   return retval;
bail_out:
   if (PyErr_Occurred()) {
      PyErrorHandler(ctx, M_FATAL);
   }

   return retval;
}

static inline PyRestoreObject *NativeToPyRestoreObject(struct restore_object_pkt *rop)
{
   PyRestoreObject *pRestoreObject = PyObject_New(PyRestoreObject, &PyRestoreObjectType);

   if (pRestoreObject) {
      pRestoreObject->object_name = PyString_FromString(rop->object_name);
      pRestoreObject->object = PyByteArray_FromStringAndSize(rop->object, rop->object_len);
      pRestoreObject->plugin_name = rop->plugin_name;
      pRestoreObject->object_type = rop->object_type;
      pRestoreObject->object_len = rop->object_len;
      pRestoreObject->object_full_len = rop->object_full_len;
      pRestoreObject->object_index = rop->object_index;
      pRestoreObject->object_compression = rop->object_compression;
      pRestoreObject->stream = rop->stream;
      pRestoreObject->JobId = rop->JobId;
   }

   return pRestoreObject;
}

static bRC PyRestoreObjectData(bpContext *ctx, struct restore_object_pkt *rop)
{
   bRC retval = bRC_Error;
   struct plugin_ctx *p_ctx = (struct plugin_ctx *)ctx->pContext;
   PyObject *pFunc;

   if (!rop) {
      return bRC_OK;
   }

   /*
    * Lookup the restore_object_data() function in the python module.
    */
   pFunc = PyDict_GetItemString(p_ctx->pDict, "restore_object_data"); /* Borrowed reference */
   if (pFunc && PyCallable_Check(pFunc)) {
      PyRestoreObject *pRestoreObject;
      PyObject *pRetVal;

      pRestoreObject = NativeToPyRestoreObject(rop);
      if (!pRestoreObject) {
         goto bail_out;
      }

      pRetVal = PyObject_CallFunctionObjArgs(pFunc, p_ctx->bpContext, pRestoreObject, NULL);
      Py_DECREF(pRestoreObject);

      if (!pRetVal) {
         goto bail_out;
      } else {
         retval = conv_python_retval(pRetVal);
         Py_DECREF(pRetVal);
      }
   } else {
      Dmsg(ctx, debuglevel, "python-fd: Failed to find function named start_restore_file()\n");
   }

   return retval;

bail_out:
   if (PyErr_Occurred()) {
      PyErrorHandler(ctx, M_FATAL);
   }

   return retval;
}

static bRC PyHandleBackupFile(bpContext *ctx, struct save_pkt *sp)
{
   bRC retval = bRC_Error;
   struct plugin_ctx *p_ctx = (struct plugin_ctx *)ctx->pContext;
   PyObject *pFunc;

   if (!sp) {
      return bRC_Error;
   }

   /*
    * Lookup the handle_backup_file() function in the python module.
    */
   pFunc = PyDict_GetItemString(p_ctx->pDict, "handle_backup_file"); /* Borrowed reference */
   if (pFunc && PyCallable_Check(pFunc)) {
      PySavePacket *pSavePkt;
      PyObject *pRetVal;

      pSavePkt = NativeToPySavePacket(sp);
      if (!pSavePkt) {
         goto bail_out;
      }

      pRetVal = PyObject_CallFunctionObjArgs(pFunc, p_ctx->bpContext, pSavePkt, NULL);
      if (!pRetVal) {
         Py_DECREF((PyObject *)pSavePkt);
         goto bail_out;
      } else {
         retval = conv_python_retval(pRetVal);
         Py_DECREF(pRetVal);

         if (!PySavePacketToNative(pSavePkt, sp, p_ctx, true)) {
            Py_DECREF((PyObject *)pSavePkt);
            goto bail_out;
         }
         Py_DECREF(pSavePkt);
      }
   } else {
      Dmsg(ctx, debuglevel, "python-fd: Failed to find function named handle_backup_file()\n");
   }

   return retval;

bail_out:
   if (PyErr_Occurred()) {
      PyErrorHandler(ctx, M_FATAL);
   }

   return retval;
}

/**
 * Callback function which is exposed as a part of the additional methods which allow
 * a Python plugin to get certain internal values of the current Job.
 */
static PyObject *PyBareosGetValue(PyObject *self, PyObject *args)
{
   int var;
   bpContext *ctx = NULL;
   PyObject *pyCtx;
   PyObject *pRetVal = NULL;

   if (!PyArg_ParseTuple(args, "Oi:BareosGetValue", &pyCtx, &var)) {
      return NULL;
   }

   switch (var) {
   case bVarFDName:
   case bVarWorkingDir:
   case bVarExePath:
   case bVarVersion:
   case bVarDistName: {
      char *value = NULL;

      if (bfuncs->getBareosValue(ctx, (bVariable)var, &value) == bRC_OK) {
         if (value) {
            pRetVal = PyString_FromString(value);
         }
      }
      break;
   }
   case bVarJobId:
   case bVarLevel:
   case bVarType:
   case bVarJobStatus:
   case bVarSinceTime:
   case bVarAccurate:
   case bVarPrefixLinks: {
      int value = 0;

      ctx = PyGetbpContext(pyCtx);
      if (bfuncs->getBareosValue(ctx, (bVariable)var, &value) == bRC_OK) {
         pRetVal = PyInt_FromLong(value);
      }
      break;
   }
   case bVarClient:
   case bVarJobName:
   case bVarPrevJobName:
   case bVarWhere:
   case bVarRegexWhere: {
      char *value = NULL;

      ctx = PyGetbpContext(pyCtx);
      if (bfuncs->getBareosValue(ctx, (bVariable)var, &value) == bRC_OK) {
         if (value) {
            pRetVal = PyString_FromString(value);
         }
      }
      break;
   }
   case bVarFileSeen:
      break;                 /* a write only variable, ignore read request */
   default:
      ctx = PyGetbpContext(pyCtx);
      Dmsg(ctx, debuglevel, "python-fd: PyBareosGetValue unknown variable requested %d\n", var);
      break;
   }

   if (!pRetVal) {
      Py_INCREF(Py_None);
      pRetVal = Py_None;
   }

   return pRetVal;
}

/**
 * Callback function which is exposed as a part of the additional methods which allow
 * a Python plugin to get certain internal values of the current Job.
 */
static PyObject *PyBareosSetValue(PyObject *self, PyObject *args)
{
   int var;
   bpContext *ctx = NULL;
   bRC retval = bRC_Error;
   PyObject *pyCtx, *pyValue;

   if (!PyArg_ParseTuple(args, "OiO:BareosSetValue", &pyCtx, &var, &pyValue)) {
      goto bail_out;
   }

   switch (var) {
   case bVarLevel: {
      int value = 0;

      value = PyInt_AsLong(pyValue);
      if (value) {
         retval = bfuncs->setBareosValue(ctx, (bVariable)var, &value);
      }
      break;
   }
   case bVarFileSeen: {
      char *value;

      value = PyString_AsString(pyValue);
      if (value) {
         retval = bfuncs->setBareosValue(ctx, (bVariable)var, value);
      }
      break;
   }
   default:
      ctx = PyGetbpContext(pyCtx);
      Dmsg(ctx, debuglevel, "python-fd: PyBareosSetValue unknown variable requested %d\n", var);
      break;
   }

bail_out:
   return conv_retval_python(retval);
}

/**
 * Callback function which is exposed as a part of the additional methods which allow
 * a Python plugin to issue debug messages using the Bareos debug message facility.
 */
static PyObject *PyBareosDebugMessage(PyObject *self, PyObject *args)
{
   int level;
   char *dbgmsg = NULL;
   bpContext *ctx;
   PyObject *pyCtx;

   if (!PyArg_ParseTuple(args, "Oi|z:BareosDebugMessage", &pyCtx, &level, &dbgmsg)) {
      return NULL;
   }

   if (dbgmsg) {
      ctx = PyGetbpContext(pyCtx);
      Dmsg(ctx, level, "python-fd: %s", dbgmsg);
   }

   Py_INCREF(Py_None);
   return Py_None;
}

/**
 * Callback function which is exposed as a part of the additional methods which allow
 * a Python plugin to issue Job messages using the Bareos Job message facility.
 */
static PyObject *PyBareosJobMessage(PyObject *self, PyObject *args)
{
   int type;
   char *jobmsg = NULL;
   bpContext *ctx;
   PyObject *pyCtx;

   if (!PyArg_ParseTuple(args, "Oi|z:BareosJobMessage", &pyCtx, &type, &jobmsg)) {
      return NULL;
   }

   if (jobmsg) {
      ctx = PyGetbpContext(pyCtx);
      Jmsg(ctx, type, "python-fd: %s", jobmsg);
   }

   Py_INCREF(Py_None);
   return Py_None;
}

/**
 * Callback function which is exposed as a part of the additional methods which allow
 * a Python plugin to issue a Register Event to register additional events it wants
 * to receive.
 */
static PyObject *PyBareosRegisterEvents(PyObject *self, PyObject *args)
{
   int len, event;
   bpContext *ctx;
   bRC retval = bRC_Error;
   PyObject *pyCtx, *pyEvents, *pySeq, *pyEvent;

   if (!PyArg_ParseTuple(args, "OO:BareosRegisterEvents", &pyCtx, &pyEvents)) {
      goto bail_out;
   }

   pySeq = PySequence_Fast(pyEvents, "Expected a sequence of events");
   if (!pySeq) {
      goto bail_out;
   }

   len = PySequence_Fast_GET_SIZE(pySeq);

   ctx = PyGetbpContext(pyCtx);
   for (int i = 0; i < len; i++) {
      pyEvent = PySequence_Fast_GET_ITEM(pySeq, i);
      event = PyInt_AsLong(pyEvent);

      if (event >= bEventJobStart && event <= FD_NR_EVENTS) {
         Dmsg(ctx, debuglevel, "python-fd: PyBareosRegisterEvents registering event %d\n", event);
         retval = bfuncs->registerBareosEvents(ctx, 1, event);

         if (retval != bRC_OK) {
            break;
         }
      }
   }

   Py_DECREF(pySeq);

bail_out:
   return conv_retval_python(retval);
}

/**
 * Callback function which is exposed as a part of the additional methods which allow
 * a Python plugin to issue an Unregister Event to unregister events it doesn't want to
 * receive anymore.
 */
static PyObject *PyBareosUnRegisterEvents(PyObject *self, PyObject *args)
{
   int len, event;
   bpContext *ctx;
   bRC retval = bRC_Error;
   PyObject *pyCtx, *pyEvents, *pySeq, *pyEvent;

   if (!PyArg_ParseTuple(args, "OO:BareosUnRegisterEvents", &pyCtx, &pyEvents)) {
      goto bail_out;
   }

   pySeq = PySequence_Fast(pyEvents, "Expected a sequence of events");
   if (!pySeq) {
      goto bail_out;
   }

   len = PySequence_Fast_GET_SIZE(pySeq);

   ctx = PyGetbpContext(pyCtx);
   for (int i = 0; i < len; i++) {
      pyEvent = PySequence_Fast_GET_ITEM(pySeq, i);
      event = PyInt_AsLong(pyEvent);

      if (event >= bEventJobStart && event <= FD_NR_EVENTS) {
         Dmsg(ctx, debuglevel, "PyBareosUnRegisterEvents: unregistering event %d\n", event);
         retval = bfuncs->unregisterBareosEvents(ctx, 1, event);
      }
   }

   Py_DECREF(pySeq);

bail_out:
   return conv_retval_python(retval);
}

/**
 * Callback function which is exposed as a part of the additional methods which allow
 * a Python plugin to issue a GetInstanceCount to retrieve the number of instances of
 * the current plugin being loaded into the daemon.
 */
static PyObject *PyBareosGetInstanceCount(PyObject *self, PyObject *args)
{
   int value;
   bpContext *ctx = NULL;
   PyObject *pyCtx;
   PyObject *pRetVal = NULL;

   if (!PyArg_ParseTuple(args, "O:BareosGetInstanceCount", &pyCtx)) {
      return NULL;
   }

   ctx = PyGetbpContext(pyCtx);
   if (bfuncs->getInstanceCount(ctx, &value) == bRC_OK) {
      pRetVal = PyInt_FromLong(value);
   }

   if (!pRetVal) {
      Py_INCREF(Py_None);
      pRetVal = Py_None;
   }

   return pRetVal;
}

/**
 * Callback function which is exposed as a part of the additional methods which allow
 * a Python plugin to issue a Add Exclude pattern to the fileset.
 */
static PyObject *PyBareosAddExclude(PyObject *self, PyObject *args)
{
   char *file = NULL;
   bpContext *ctx;
   PyObject *pyCtx;
   bRC retval = bRC_Error;

   if (!PyArg_ParseTuple(args, "O|z:BareosAddExclude", &pyCtx, &file)) {
      goto bail_out;
   }

   if (file) {
      ctx = PyGetbpContext(pyCtx);
      retval = bfuncs->AddExclude(ctx, file);
   }

bail_out:
   return conv_retval_python(retval);
}

/**
 * Callback function which is exposed as a part of the additional methods which allow
 * a Python plugin to issue a Add Include pattern to the fileset.
 */
static PyObject *PyBareosAddInclude(PyObject *self, PyObject *args)
{
   char *file = NULL;
   bpContext *ctx;
   PyObject *pyCtx;
   bRC retval = bRC_Error;

   if (!PyArg_ParseTuple(args, "O|z:BareosAddInclude", &pyCtx, &file)) {
      goto bail_out;
   }

   if (file) {
      ctx = PyGetbpContext(pyCtx);
      retval = bfuncs->AddInclude(ctx, file);
   }

bail_out:
   return conv_retval_python(retval);
}

/**
 * Callback function which is exposed as a part of the additional methods which allow
 * a Python plugin to issue a Add Include Options to the fileset.
 */
static PyObject *PyBareosAddOptions(PyObject *self, PyObject *args)
{
   char *opts = NULL;
   bpContext *ctx;
   PyObject *pyCtx;
   bRC retval = bRC_Error;

   if (!PyArg_ParseTuple(args, "O|z:BareosAddOptions", &pyCtx, &opts)) {
      goto bail_out;
   }

   if (opts) {
      ctx = PyGetbpContext(pyCtx);
      retval = bfuncs->AddOptions(ctx, opts);
   }

bail_out:
   return conv_retval_python(retval);
}

/**
 * Callback function which is exposed as a part of the additional methods which allow
 * a Python plugin to issue a Add Regex to the fileset.
 */
static PyObject *PyBareosAddRegex(PyObject *self, PyObject *args)
{
   int type;
   char *item = NULL;
   bpContext *ctx;
   PyObject *pyCtx;
   bRC retval = bRC_Error;

   if (!PyArg_ParseTuple(args, "O|zi:BareosAddRegex", &pyCtx, &item, &type)) {
      goto bail_out;
   }

   if (item) {
      ctx = PyGetbpContext(pyCtx);
      retval = bfuncs->AddRegex(ctx, item, type);
   }

bail_out:
   return conv_retval_python(retval);
}

/**
 * Callback function which is exposed as a part of the additional methods which allow
 * a Python plugin to issue a Add Wildcard to the fileset.
 */
static PyObject *PyBareosAddWild(PyObject *self, PyObject *args)
{
   int type;
   char *item = NULL;
   bpContext *ctx;
   PyObject *pyCtx;
   bRC retval = bRC_Error;

   if (!PyArg_ParseTuple(args, "O|zi:BareosAddWild", &pyCtx, &item, &type)) {
      goto bail_out;
   }

   if (item) {
      ctx = PyGetbpContext(pyCtx);
      retval = bfuncs->AddWild(ctx, item, type);
   }

bail_out:
   return conv_retval_python(retval);
}

/**
 * Callback function which is exposed as a part of the additional methods which allow
 * a Python plugin to issue a Add New Option block.
 */
static PyObject *PyBareosNewOptions(PyObject *self, PyObject *args)
{
   bpContext *ctx;
   PyObject *pyCtx;
   bRC retval = bRC_Error;

   if (!PyArg_ParseTuple(args, "O:BareosNewOptions", &pyCtx)) {
      goto bail_out;
   }

   ctx = PyGetbpContext(pyCtx);
   retval = bfuncs->NewOptions(ctx);

bail_out:
   return conv_retval_python(retval);
}

/**
 * Callback function which is exposed as a part of the additional methods which allow
 * a Python plugin to issue a Add New Include block.
 */
static PyObject *PyBareosNewInclude(PyObject *self, PyObject *args)
{
   bpContext *ctx;
   PyObject *pyCtx;
   bRC retval = bRC_Error;

   if (!PyArg_ParseTuple(args, "O:BareosNewInclude", &pyCtx)) {
      goto bail_out;
   }

   ctx = PyGetbpContext(pyCtx);
   retval = bfuncs->NewInclude(ctx);

bail_out:
   return conv_retval_python(retval);
}

/**
 * Callback function which is exposed as a part of the additional methods which allow
 * a Python plugin to issue a Add New Pre Include block.
 */
static PyObject *PyBareosNewPreInclude(PyObject *self, PyObject *args)
{
   bpContext *ctx;
   PyObject *pyCtx;
   bRC retval = bRC_Error;

   if (!PyArg_ParseTuple(args, "O:BareosNewPreInclude", &pyCtx)) {
      goto bail_out;
   }

   ctx = PyGetbpContext(pyCtx);
   retval = bfuncs->NewPreInclude(ctx);

bail_out:
   return conv_retval_python(retval);
}

/**
 * Callback function which is exposed as a part of the additional methods which allow
 * a Python plugin to issue a check if a file have to be backuped using Accurate code.
 */
static PyObject *PyBareosCheckChanges(PyObject *self, PyObject *args)
{
   bpContext *ctx;
   PyObject *pyCtx;
   struct save_pkt sp;
   bRC retval = bRC_Error;
   PySavePacket *pSavePkt;

   if (!PyArg_ParseTuple(args, "OO:BareosCheckChanges", &pyCtx, &pSavePkt)) {
      goto bail_out;
   }

   ctx = PyGetbpContext(pyCtx);

   /*
    * CheckFile only has a need for a limited version of the PySavePacket so we handle
    * that here separately and don't call PySavePacketToNative().
    */
   sp.type = pSavePkt->type;
   if (pSavePkt->fname) {
      if (PyString_Check(pSavePkt->fname)) {
         sp.fname = PyString_AsString(pSavePkt->fname);
      } else {
         goto bail_out;
      }
   } else {
      goto bail_out;
   }
   if (pSavePkt->link) {
      if (PyString_Check(pSavePkt->link)) {
         sp.link = PyString_AsString(pSavePkt->link);
      } else {
         goto bail_out;
      }
   }
   sp.save_time = pSavePkt->save_time;

   retval = bfuncs->checkChanges(ctx, &sp);

   /*
    * Copy back the two fields that are changed by checkChanges().
    */
   pSavePkt->delta_seq = sp.delta_seq;
   pSavePkt->accurate_found = sp.accurate_found;

bail_out:
   return conv_retval_python(retval);
}

/**
 * Callback function which is exposed as a part of the additional methods which allow
 * a Python plugin to issue a check if a file would be saved using current Include/Exclude code.
 */
static PyObject *PyBareosAcceptFile(PyObject *self, PyObject *args)
{
   bpContext *ctx;
   PyObject *pyCtx;
   struct save_pkt sp;
   bRC retval = bRC_Error;
   PySavePacket *pSavePkt;

   if (!PyArg_ParseTuple(args, "OO:BareosAcceptFile", &pyCtx, &pSavePkt)) {
      goto bail_out;
   }

   ctx = PyGetbpContext(pyCtx);

   /*
    * Acceptfile only needs fname and statp from PySavePacket so we handle
    * that here separately and don't call PySavePacketToNative().
    */
   if (pSavePkt->fname) {
      if (PyString_Check(pSavePkt->fname)) {
         sp.fname = PyString_AsString(pSavePkt->fname);
      } else {
         goto bail_out;
      }
   } else {
      goto bail_out;
   }

   if (pSavePkt->statp) {
      PyStatPacketToNative((PyStatPacket *)pSavePkt->statp, &sp.statp);
   } else {
      goto bail_out;
   }

   retval = bfuncs->AcceptFile(ctx, &sp);

bail_out:
   return conv_retval_python(retval);
}

/**
 * Callback function which is exposed as a part of the additional methods which allow
 * a Python plugin to issue a Set bit in the Accurate Seen bitmap.
 */
static PyObject *PyBareosSetSeenBitmap(PyObject *self, PyObject *args)
{
   bool all;
   bpContext *ctx;
   char *fname = NULL;
   bRC retval = bRC_Error;
   PyObject *pyCtx, *pyBool;

   if (!PyArg_ParseTuple(args, "OO|s:BareosSetSeenBitmap", &pyCtx, &pyBool, &fname)) {
      goto bail_out;
   }

   ctx = PyGetbpContext(pyCtx);
   all = PyObject_IsTrue(pyBool);
   retval = bfuncs->SetSeenBitmap(ctx, all, fname);

bail_out:
   return conv_retval_python(retval);
}

/**
 * Callback function which is exposed as a part of the additional methods which allow
 * a Python plugin to issue a Clear bit in the Accurate Seen bitmap.
 */
static PyObject *PyBareosClearSeenBitmap(PyObject *self, PyObject *args)
{
   bool all;
   bpContext *ctx;
   char *fname = NULL;
   bRC retval = bRC_Error;
   PyObject *pyCtx, *pyBool;

   if (!PyArg_ParseTuple(args, "OO|s:BareosClearSeenBitmap", &pyCtx, &pyBool, &fname)) {
      goto bail_out;
   }

   ctx = PyGetbpContext(pyCtx);
   all = PyObject_IsTrue(pyBool);
   retval = bfuncs->ClearSeenBitmap(ctx, all, fname);

bail_out:
   return conv_retval_python(retval);
}

/**
 * Some helper functions.
 */
static inline char *PyGetStringValue(PyObject *object)
{
   if (!object || !PyString_Check(object)) {
      return (char *)"";
   }

   return PyString_AsString(object);
}

static inline char *PyGetByteArrayValue(PyObject *object)
{
   if (!object || !PyByteArray_Check(object)) {
      return (char *)"";
   }

   return PyByteArray_AsString(object);
}

/**
 * Python specific handlers for PyRestoreObject structure mapping.
 */

/**
 * Representation.
 */
static PyObject *PyRestoreObject_repr(PyRestoreObject *self)
{
   PyObject *s;
   PoolMem buf(PM_MESSAGE);

   Mmsg(buf, "RestoreObject(object_name=\"%s\", object=\"%s\", plugin_name=\"%s\", "
             "object_type=%d, object_len=%d, object_full_len=%d, "
             "object_index=%d, object_compression=%d, stream=%d, jobid=%u)",
        PyGetStringValue(self->object_name), PyGetByteArrayValue(self->object), self->plugin_name,
        self->object_type, self->object_len, self->object_full_len,
        self->object_index, self->object_compression, self->stream, self->JobId);
   s = PyString_FromString(buf.c_str());

   return s;
}

/**
 * Initialization.
 */
static int PyRestoreObject_init(PyRestoreObject *self, PyObject *args, PyObject *kwds)
{
   static char *kwlist[] = {
      (char *)"object_name",
      (char *)"object",
      (char *)"plugin_name",
      (char *)"object_type",
      (char *)"object_len",
      (char *)"object_full_len",
      (char *)"object_index",
      (char *)"object_compression",
      (char *)"stream",
      (char *)"jobid",
      NULL
   };

   self->object_name = NULL;
   self->object = NULL;
   self->plugin_name = NULL;
   self->object_type = 0;
   self->object_len = 0;
   self->object_full_len = 0;
   self->object_index = 0;
   self->object_compression = 0;
   self->stream = 0;
   self->JobId = 0;

   if (!PyArg_ParseTupleAndKeywords(args,
                                    kwds,
                                    "|oosiiiiiiI",
                                    kwlist,
                                    &self->object_name,
                                    &self->object,
                                    &self->plugin_name,
                                    &self->object_type,
                                    &self->object_len,
                                    &self->object_full_len,
                                    &self->object_index,
                                    &self->object_compression,
                                    &self->stream,
                                    &self->JobId)) {
      return -1;
   }

   return 0;
}

/**
 * Destructor.
 */
static void PyRestoreObject_dealloc(PyRestoreObject *self)
{
   if (self->object_name) {
      Py_XDECREF(self->object_name);
   }
   if (self->object) {
      Py_XDECREF(self->object);
   }
   PyObject_Del(self);
}

/**
 * Python specific handlers for PyStatPacket structure mapping.
 */

/**
 * Representation.
 */
static PyObject *PyStatPacket_repr(PyStatPacket *self)
{
   PyObject *s;
   PoolMem buf(PM_MESSAGE);

   Mmsg(buf, "StatPacket(dev=%ld, ino=%lld, mode=%04o, nlink=%d, "
             "uid=%ld, gid=%ld, rdev=%ld, size=%lld, "
             "atime=%ld, mtime=%ld, ctime=%ld, blksize=%ld, blocks=%lld)",
        self->dev, self->ino, (self->mode & ~S_IFMT), self->nlink,
        self->uid, self->gid, self->rdev, self->size,
        self->atime, self->mtime, self->ctime, self->blksize, self->blocks);

   s = PyString_FromString(buf.c_str());

   return s;
}

/**
 * Initialization.
 */
static int PyStatPacket_init(PyStatPacket *self, PyObject *args, PyObject *kwds)
{
   time_t now;
   static char *kwlist[] = {
      (char *)"dev",
      (char *)"ino",
      (char *)"mode",
      (char *)"nlink",
      (char *)"uid",
      (char *)"gid",
      (char *)"rdev",
      (char *)"size",
      (char *)"atime",
      (char *)"mtime",
      (char *)"ctime",
      (char *)"blksize",
      (char *)"blocks",
      NULL
   };

   now = time(NULL);
   self->dev = 0;
   self->ino = 0;
   self->mode = 0700 | S_IFREG;
   self->nlink = 0;
   self->uid = 0;
   self->gid = 0;
   self->rdev = 0;
   self->size = -1;
   self->atime = now;
   self->mtime = now;
   self->ctime = now;
   self->blksize = 4096;
   self->blocks = 1;

   if (!PyArg_ParseTupleAndKeywords(args,
                                    kwds,
                                    "|IKHHIIIKIIIIK",
                                    kwlist,
                                    &self->dev,
                                    &self->ino,
                                    &self->mode,
                                    &self->nlink,
                                    &self->uid,
                                    &self->gid,
                                    &self->rdev,
                                    &self->size,
                                    &self->atime,
                                    &self->mtime,
                                    &self->ctime,
                                    &self->blksize,
                                    &self->blocks)) {
      return -1;
   }

   return 0;
}

/**
 * Destructor.
 */
static void PyStatPacket_dealloc(PyStatPacket *self)
{
   PyObject_Del(self);
}

/**
 * Python specific handlers for PySavePacket structure mapping.
 */

/**
 * Representation.
 */
static inline const char *print_flags_bitmap(PyObject *bitmap)
{
   static char visual_bitmap[FO_MAX + 1];

   if (PyByteArray_Check(bitmap)) {
      int cnt;
      char *flags;

      if (PyByteArray_Size(bitmap) == FOPTS_BYTES) {
         if ((flags = PyByteArray_AsString(bitmap))) {
            memset(visual_bitmap, 0, sizeof(visual_bitmap));
            for (cnt = 0; cnt <= FO_MAX; cnt++) {
               if (bit_is_set(cnt, flags)) {
                  visual_bitmap[cnt] = '1';
               } else {
                  visual_bitmap[cnt] = '0';
               }
            }

            return visual_bitmap;
         }
      }
   }

   return "Unknown";
}

static PyObject *PySavePacket_repr(PySavePacket *self)
{
   PyObject *s;
   PoolMem buf(PM_MESSAGE);

   Mmsg(buf, "SavePacket(fname=\"%s\", link=\"%s\", type=%ld, flags=%s, "
             "no_read=%d, portable=%d, accurate_found=%d, "
             "cmd=\"%s\", save_time=%ld, delta_seq=%ld, object_name=\"%s\", "
             "object=\"%s\", object_len=%ld, object_index=%ld)",
        PyGetStringValue(self->fname), PyGetStringValue(self->link),
        self->type, print_flags_bitmap(self->flags), self->no_read, self->portable,
        self->accurate_found, self->cmd, self->save_time, self->delta_seq,
        PyGetStringValue(self->object_name), PyGetByteArrayValue(self->object),
        self->object_len, self->object_index);

   s = PyString_FromString(buf.c_str());

   return s;
}

/**
 * Initialization.
 */
static int PySavePacket_init(PySavePacket *self, PyObject *args, PyObject *kwds)
{
   static char *kwlist[] = {
      (char *)"fname",
      (char *)"link",
      (char *)"type",
      (char *)"flags",
      (char *)"no_read",
      (char *)"portable",
      (char *)"accurate_found",
      (char *)"cmd",
      (char *)"save_time",
      (char *)"delta_seq",
      (char *)"object_name",
      (char *)"object",
      (char *)"object_len",
      (char *)"object_index",
      NULL
   };

   self->fname = NULL;
   self->link = NULL;
   self->type = 0;
   self->flags = NULL;
   self->no_read = false;
   self->portable = false;
   self->accurate_found = false;
   self->cmd = NULL;
   self->save_time = 0;
   self->delta_seq = 0;
   self->object_name = NULL;
   self->object = NULL;
   self->object_len = 0;
   self->object_index = 0;

   if (!PyArg_ParseTupleAndKeywords(args,
                                    kwds,
                                    "|ooiocccsiiooii",
                                    kwlist,
                                    &self->fname,
                                    &self->link,
                                    &self->type,
                                    &self->flags,
                                    &self->no_read,
                                    &self->portable,
                                    &self->accurate_found,
                                    &self->cmd,
                                    &self->save_time,
                                    &self->delta_seq,
                                    &self->object_name,
                                    &self->object,
                                    &self->object_len,
                                    &self->object_index)) {
      return -1;
   }

   return 0;
}

/**
 * Destructor.
 */
static void PySavePacket_dealloc(PySavePacket *self)
{
   if (self->fname) {
      Py_XDECREF(self->fname);
   }
   if (self->link) {
      Py_XDECREF(self->link);
   }
   if (self->flags) {
      Py_XDECREF(self->flags);
   }
   if (self->object_name) {
      Py_XDECREF(self->object_name);
   }
   if (self->object) {
      Py_XDECREF(self->object);
   }
   PyObject_Del(self);
}

/**
 * Python specific handlers for PyRestorePacket structure mapping.
 */

/**
 * Representation.
 */
static PyObject *PyRestorePacket_repr(PyRestorePacket *self)
{
   PyObject *stat_repr, *s;
   PoolMem buf(PM_MESSAGE);

   stat_repr = PyObject_Repr(self->statp);
   Mmsg(buf, "RestorePacket(stream=%d, data_stream=%ld, type=%ld, file_index=%ld, "
             "linkFI=%ld, uid=%ld, statp=\"%s\", attrEx=\"%s\", ofname=\"%s\", "
             "olname=\"%s\", where=\"%s\", RegexWhere=\"%s\", replace=%d, create_status=%d)",
        self->stream, self->data_stream, self->type, self->file_index,
        self->LinkFI, self->uid, PyGetStringValue(stat_repr), self->attrEx, self->ofname,
        self->olname, self->where, self->RegexWhere, self->replace, self->create_status);

   s = PyString_FromString(buf.c_str());
   Py_DECREF(stat_repr);

   return s;
}

/**
 * Initialization.
 */
static int PyRestorePacket_init(PyRestorePacket *self, PyObject *args, PyObject *kwds)
{
   static char *kwlist[] = {
      (char *)"stream",
      (char *)"data_stream",
      (char *)"type",
      (char *)"file_index",
      (char *)"linkFI",
      (char *)"uid",
      (char *)"statp",
      (char *)"attrEX",
      (char *)"ofname",
      (char *)"olname",
      (char *)"where",
      (char *)"regexwhere",
      (char *)"replace",
      (char *)"create_status",
      NULL
   };

   self->stream = 0;
   self->data_stream = 0;
   self->type = 0;
   self->file_index = 0;
   self->LinkFI = 0;
   self->uid = 0;
   self->statp = NULL;
   self->attrEx = NULL;
   self->ofname = NULL;
   self->olname = NULL;
   self->where = NULL;
   self->RegexWhere = NULL;
   self->replace = 0;
   self->create_status = 0;

   if (!PyArg_ParseTupleAndKeywords(args,
                                    kwds,
                                    "|iiiiiIosssssii",
                                    kwlist,
                                    &self->stream,
                                    &self->data_stream,
                                    &self->type,
                                    &self->file_index,
                                    &self->LinkFI,
                                    &self->uid,
                                    &self->statp,
                                    &self->attrEx,
                                    &self->ofname,
                                    &self->olname,
                                    &self->where,
                                    &self->RegexWhere,
                                    &self->replace,
                                    &self->create_status)) {
      return -1;
   }

   return 0;
}

/**
 * Destructor.
 */
static void PyRestorePacket_dealloc(PyRestorePacket *self)
{
   PyObject_Del(self);
}

/**
 * Python specific handlers for PyIoPacket structure mapping.
 */

/**
 * Representation.
 */
static PyObject *PyIoPacket_repr(PyIoPacket *self)
{
   PyObject *s;
   PoolMem buf(PM_MESSAGE);

   Mmsg(buf, "IoPacket(func=%d, count=%ld, flags=%ld, mode=%04o, "
             "buf=\"%s\", fname=\"%s\", status=%ld, io_errno=%ld, lerror=%ld, "
             "whence=%ld, offset=%lld, win32=%d)",
        self->func, self->count, self->flags, (self->mode & ~S_IFMT),
        PyGetByteArrayValue(self->buf), self->fname, self->status,
        self->io_errno, self->lerror,
        self->whence, self->offset, self->win32);
   s = PyString_FromString(buf.c_str());

   return s;
}

/**
 * Initialization.
 */
static int PyIoPacket_init(PyIoPacket *self, PyObject *args, PyObject *kwds)
{
   static char *kwlist[] = {
      (char *)"func",
      (char *)"count",
      (char *)"flags",
      (char *)"mode",
      (char *)"buf",
      (char *)"fname",
      (char *)"status",
      (char *)"io_errno",
      (char *)"lerror",
      (char *)"whence",
      (char *)"offset",
      (char *)"win32",
      NULL
   };

   self->func = 0;
   self->count = 0;
   self->flags = 0;
   self->mode = 0;
   self->buf = NULL;
   self->fname = NULL;
   self->status = 0;
   self->io_errno = 0;
   self->lerror = 0;
   self->whence = 0;
   self->offset = 0;
   self->win32 = false;

   if (!PyArg_ParseTupleAndKeywords(args,
                                    kwds,
                                    "|Hiiiosiiiilc",
                                    kwlist,
                                    &self->func,
                                    &self->count,
                                    &self->flags,
                                    &self->mode,
                                    &self->buf,
                                    &self->fname,
                                    &self->status,
                                    &self->io_errno,
                                    &self->lerror,
                                    &self->whence,
                                    &self->offset,
                                    &self->win32)) {
      return -1;
   }

   return 0;
}

/**
 * Destructor.
 */
static void PyIoPacket_dealloc(PyIoPacket *self)
{
   if (self->buf) {
      Py_XDECREF(self->buf);
   }
   PyObject_Del(self);
}

/**
 * Python specific handlers for PyAclPacket structure mapping.
 */

/**
 * Representation.
 */
static PyObject *PyAclPacket_repr(PyAclPacket *self)
{
   PyObject *s;
   PoolMem buf(PM_MESSAGE);

   Mmsg(buf, "AclPacket(fname=\"%s\", content=\"%s\")",
        self->fname, PyGetByteArrayValue(self->content));
   s = PyString_FromString(buf.c_str());

   return s;
}

/**
 * Initialization.
 */
static int PyAclPacket_init(PyAclPacket *self, PyObject *args, PyObject *kwds)
{
   static char *kwlist[] = {
      (char *)"fname",
      (char *)"content",
      NULL
   };

   self->fname = NULL;
   self->content = NULL;

   if (!PyArg_ParseTupleAndKeywords(args,
                                    kwds,
                                    "|so",
                                    kwlist,
                                    &self->fname,
                                    &self->content)) {
      return -1;
   }

   return 0;
}

/**
 * Destructor.
 */
static void PyAclPacket_dealloc(PyAclPacket *self)
{
   if (self->content) {
      Py_XDECREF(self->content);
   }
   PyObject_Del(self);
}

/**
 * Python specific handlers for PyIOPacket structure mapping.
 */

/**
 * Representation.
 */
static PyObject *PyXattrPacket_repr(PyXattrPacket *self)
{
   PyObject *s;
   PoolMem buf(PM_MESSAGE);

   Mmsg(buf, "XattrPacket(fname=\"%s\", name=\"%s\", value=\"%s\")",
        self->fname, PyGetByteArrayValue(self->name), PyGetByteArrayValue(self->value));
   s = PyString_FromString(buf.c_str());

   return s;
}

/**
 * Initialization.
 */
static int PyXattrPacket_init(PyXattrPacket *self, PyObject *args, PyObject *kwds)
{
   static char *kwlist[] = {
      (char *)"fname",
      (char *)"name",
      (char *)"value",
      NULL
   };

   self->fname = NULL;
   self->name = NULL;
   self->value = NULL;

   if (!PyArg_ParseTupleAndKeywords(args,
                                    kwds,
                                    "|soo",
                                    kwlist,
                                    &self->fname,
                                    &self->name,
                                    &self->value)) {
      return -1;
   }

   return 0;
}

/**
 * Destructor.
 */
static void PyXattrPacket_dealloc(PyXattrPacket *self)
{
   if (self->value) {
      Py_XDECREF(self->value);
   }
   if (self->name) {
      Py_XDECREF(self->name);
   }
   PyObject_Del(self);
}
