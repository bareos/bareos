/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2011-2015 Planets Communications B.V.
   Copyright (C) 2013-2020 Bareos GmbH & Co. KG

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
 * Python plugin for the Bareos File Daemon
 */
#define PY_SSIZE_T_CLEAN
#define BUILD_PLUGIN

#if defined(HAVE_WIN32)
#include "include/bareos.h"
#include <Python.h>
#else
#include <Python.h>
#include "plugins/include/capsulethunk.h"
#include "include/bareos.h"
#endif

#define PLUGIN_DAEMON "fd"

#if PY_VERSION_HEX < 0x03000000
#define PLUGIN_NAME "python"
#define PLUGIN_DIR PY2MODDIR
#else
#define PLUGIN_NAME "python3"
#define PLUGIN_DIR PY3MODDIR
#endif
#define LOGPREFIX PLUGIN_NAME "-" PLUGIN_DAEMON ": "

#include "filed/fd_plugins.h"
#include "plugins/include/python3compat.h"

#include "python-fd.h"
#include "module/bareosfd.h"
#include "lib/edit.h"

namespace filedaemon {

static const int debuglevel = 150;

#define PLUGIN_LICENSE "Bareos AGPLv3"
#define PLUGIN_AUTHOR "Bareos GmbH & Co. KG"
#define PLUGIN_DATE "May 2020"
#define PLUGIN_VERSION "4"
#define PLUGIN_DESCRIPTION "Python File Daemon Plugin"
#define PLUGIN_USAGE                                                     \
  PLUGIN_NAME                                                            \
  ":module_path=<path-to-python-modules>:module_name=<python-module-to-" \
  "load>:..."

/* Forward referenced functions */
static bRC newPlugin(PluginContext* plugin_ctx);
static bRC freePlugin(PluginContext* plugin_ctx);

static bRC getPluginValue(PluginContext* plugin_ctx,
                          pVariable var,
                          void* value);
static bRC setPluginValue(PluginContext* plugin_ctx,
                          pVariable var,
                          void* value);
static bRC handlePluginEvent(PluginContext* plugin_ctx,
                             bEvent* event,
                             void* value);
static bRC startBackupFile(PluginContext* plugin_ctx, struct save_pkt* sp);
static bRC endBackupFile(PluginContext* plugin_ctx);
static bRC pluginIO(PluginContext* plugin_ctx, struct io_pkt* io);
static bRC startRestoreFile(PluginContext* plugin_ctx, const char* cmd);
static bRC endRestoreFile(PluginContext* plugin_ctx);
static bRC createFile(PluginContext* plugin_ctx, struct restore_pkt* rp);
static bRC setFileAttributes(PluginContext* plugin_ctx, struct restore_pkt* rp);
static bRC checkFile(PluginContext* plugin_ctx, char* fname);
static bRC getAcl(PluginContext* plugin_ctx, acl_pkt* ap);
static bRC setAcl(PluginContext* plugin_ctx, acl_pkt* ap);
static bRC getXattr(PluginContext* plugin_ctx, xattr_pkt* xp);
static bRC setXattr(PluginContext* plugin_ctx, xattr_pkt* xp);
static bRC parse_plugin_definition(PluginContext* plugin_ctx,
                                   void* value,
                                   PoolMem& plugin_options);

static void PyErrorHandler(PluginContext* plugin_ctx, int msgtype);
static bRC PyLoadModule(PluginContext* plugin_ctx, void* value);

/* Pointers to Bareos functions */
static CoreFunctions* bareos_core_functions = NULL;
static PluginApiDefinition* bareos_plugin_interface_version = NULL;

static PluginInformation pluginInfo = {
    sizeof(pluginInfo), FD_PLUGIN_INTERFACE_VERSION,
    FD_PLUGIN_MAGIC,    PLUGIN_LICENSE,
    PLUGIN_AUTHOR,      PLUGIN_DATE,
    PLUGIN_VERSION,     PLUGIN_DESCRIPTION,
    PLUGIN_USAGE};

static PluginFunctions pluginFuncs = {
    sizeof(pluginFuncs), FD_PLUGIN_INTERFACE_VERSION,

    /* Entry points into plugin */
    newPlugin,  /* new plugin instance */
    freePlugin, /* free plugin instance */
    getPluginValue, setPluginValue, handlePluginEvent, startBackupFile,
    endBackupFile, startRestoreFile, endRestoreFile, pluginIO, createFile,
    setFileAttributes, checkFile, getAcl, setAcl, getXattr, setXattr};


#include "plugin_private_context.h"
/**
 * We don't actually use this but we need it to tear down the
 * final python interpreter on unload of the plugin. Each instance of
 * the plugin get its own interpreter.
 */
static PyThreadState* mainThreadState;

/* functions common to all plugins */
#include "plugins/include/python_plugins_common.inc"

#ifdef __cplusplus
extern "C" {
#endif

static void PyErrorHandler()
{
  PyObject *type, *value, *traceback;
  PyObject* tracebackModule;
  char* error_string;

  PyErr_Fetch(&type, &value, &traceback);
  PyErr_NormalizeException(&type, &value, &traceback);

  tracebackModule = PyImport_ImportModule("traceback");
  if (tracebackModule != NULL) {
    PyObject *tbList, *emptyString, *strRetval;

    tbList =
        PyObject_CallMethod(tracebackModule, (char*)"format_exception",
                            (char*)"OOO", type, value == NULL ? Py_None : value,
                            traceback == NULL ? Py_None : traceback);

    emptyString = PyUnicode_FromString("");
    strRetval =
        PyObject_CallMethod(emptyString, (char*)"join", (char*)"O", tbList);

    error_string = strdup(PyUnicode_AsUTF8(strRetval));

    Py_DECREF(tbList);
    Py_DECREF(emptyString);
    Py_DECREF(strRetval);
    Py_DECREF(tracebackModule);
  } else {
    error_string = strdup("Unable to import traceback module.");
  }
  Py_DECREF(type);
  Py_XDECREF(value);
  Py_XDECREF(traceback);
  // printf("%s", error_string);

  free(error_string);
  exit(1);
}


/**
 * Plugin called here when it is first loaded
 */
bRC loadPlugin(PluginApiDefinition* lbareos_plugin_interface_version,
               CoreFunctions* lbareos_core_functions,
               PluginInformation** plugin_information,
               PluginFunctions** plugin_functions)
{
  if (!Py_IsInitialized()) {
    Py_InitializeEx(0);

    // add bareos plugin path to python module search path
    PyObject* sysPath = PySys_GetObject((char*)"path");
    PyObject* pluginPath = PyUnicode_FromString(PLUGIN_DIR);
    PyList_Append(sysPath, pluginPath);
    Py_DECREF(pluginPath);

    /* import the bareosfd module */
    PyObject* bareosfdModule = PyImport_ImportModule("bareosfd");
    if (!bareosfdModule) {
      printf("loading of bareosfd failed\n");
      if (PyErr_Occurred()) { PyErrorHandler(); }
    }

    /* import the CAPI from the bareosfd python module
     * afterwards, Bareosfd_* macros are initialized to
     * point to the corresponding functions in the bareosfd python
     * module */
    import_bareosfd();

    /* set bareos_core_functions inside of barosfd module */
    Bareosfd_set_bareos_core_functions(lbareos_core_functions);

    bareos_core_functions =
        lbareos_core_functions; /* Set Bareos funct pointers */
    bareos_plugin_interface_version = lbareos_plugin_interface_version;

    *plugin_information = &pluginInfo; /* Return pointer to our info */
    *plugin_functions = &pluginFuncs;  /* Return pointer to our functions */

    PyEval_InitThreads();
    mainThreadState = PyEval_SaveThread();
  }
  return bRC_OK;
}

/**
 * Plugin called here when it is unloaded, normally when Bareos is going to
 * exit.
 */
bRC unloadPlugin()
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
static bRC newPlugin(PluginContext* plugin_ctx)
{
  struct plugin_private_context* plugin_priv_ctx =
      (struct plugin_private_context*)malloc(
          sizeof(struct plugin_private_context));
  if (!plugin_priv_ctx) { return bRC_Error; }
  memset(plugin_priv_ctx, 0, sizeof(struct plugin_private_context));
  plugin_ctx->plugin_private_context =
      (void*)plugin_priv_ctx; /* set our context pointer */


  /* set bareos_plugin_context inside of barosfd module */
  Bareosfd_set_plugin_context(plugin_ctx);

  /* For each plugin instance we instantiate a new Python interpreter. */
  PyEval_AcquireThread(mainThreadState);
  plugin_priv_ctx->interpreter = Py_NewInterpreter();
  PyEval_ReleaseThread(plugin_priv_ctx->interpreter);

  /* Always register some events the python plugin itself can register
     any other events it is interested in. */
  bareos_core_functions->registerBareosEvents(
      plugin_ctx, 9, bEventLevel, bEventSince, bEventNewPluginOptions,
      bEventPluginCommand, bEventJobStart, bEventRestoreCommand,
      bEventEstimateCommand, bEventBackupCommand, bEventRestoreObject);

  return bRC_OK;
}

/**
 * Release everything concerning a particular instance of a
 * plugin. Normally called when the Job terminates.
 */
static bRC freePlugin(PluginContext* plugin_ctx)
{
  struct plugin_private_context* plugin_priv_ctx =
      (struct plugin_private_context*)plugin_ctx->plugin_private_context;

  if (!plugin_priv_ctx) { return bRC_Error; }

  if (plugin_priv_ctx->plugin_options) {
    free(plugin_priv_ctx->plugin_options);
  }

  if (plugin_priv_ctx->module_path) { free(plugin_priv_ctx->module_path); }

  if (plugin_priv_ctx->module_name) { free(plugin_priv_ctx->module_name); }

  if (plugin_priv_ctx->fname) { free(plugin_priv_ctx->fname); }

  if (plugin_priv_ctx->link) { free(plugin_priv_ctx->link); }

  if (plugin_priv_ctx->object_name) { free(plugin_priv_ctx->object_name); }

  if (plugin_priv_ctx->object) { free(plugin_priv_ctx->object); }

  /*
   * Stop any sub interpreter started per plugin instance.
   */
  PyEval_AcquireThread(plugin_priv_ctx->interpreter);


  if (plugin_priv_ctx->pModule) { Py_DECREF(plugin_priv_ctx->pModule); }

  Py_EndInterpreter(plugin_priv_ctx->interpreter);
  PyEval_ReleaseLock();

  free(plugin_priv_ctx);
  plugin_ctx->plugin_private_context = NULL;

  return bRC_OK;
}


static bRC handlePluginEvent(PluginContext* plugin_ctx,
                             bEvent* event,
                             void* value)
{
  bRC retval = bRC_Error;
  bool event_dispatched = false;
  PoolMem plugin_options(PM_FNAME);
  plugin_private_context* plugin_priv_ctx =
      (plugin_private_context*)plugin_ctx->plugin_private_context;

  if (!plugin_priv_ctx) { goto bail_out; }

  /*
   * First handle some events internally before calling python if it
   * want to do some special handling on the event triggered.
   */
  switch (event->eventType) {
    case bEventLevel:
      plugin_priv_ctx->backup_level = (int64_t)value;
      break;
    case bEventSince:
      plugin_priv_ctx->since = (int64_t)value;
      break;
    case bEventBackupCommand:
      /* Fall-through wanted */
    case bEventRestoreCommand:
      /* Fall-through wanted */
    case bEventEstimateCommand:
      /* Fall-through wanted */
    case bEventPluginCommand:
      event_dispatched = true;
      retval = parse_plugin_definition(plugin_ctx, value, plugin_options);
      break;
    case bEventNewPluginOptions:
      /* Free any previous value.  */
      if (plugin_priv_ctx->plugin_options) {
        free(plugin_priv_ctx->plugin_options);
        plugin_priv_ctx->plugin_options = NULL;
      }

      event_dispatched = true;
      retval = parse_plugin_definition(plugin_ctx, value, plugin_options);

      /* Save that we got a plugin override.  */
      plugin_priv_ctx->plugin_options = strdup((char*)value);
      break;
    case bEventRestoreObject: {
      struct restore_object_pkt* rop;

      rop = (struct restore_object_pkt*)value;

      /* Only use the plugin definition of a restore object if we
       * didn't get any other plugin definition from some other source before.*/
      if (!plugin_priv_ctx->python_loaded) {
        if (rop && *rop->plugin_name) {
          event_dispatched = true;
          retval = parse_plugin_definition(plugin_ctx, rop->plugin_name,
                                           plugin_options);
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
   * we only do a dispatch to the python entry point when that internal
   * processing was successful (e.g. retval == bRC_OK).
   */
  if (!event_dispatched || retval == bRC_OK) {
    PyEval_AcquireThread(plugin_priv_ctx->interpreter);

    /*
     * Now dispatch the event to Python.
     * First the calls that need special handling.
     */
    switch (event->eventType) {
      case bEventBackupCommand:
        /* Fall-through wanted */
      case bEventRestoreCommand:
        /* Fall-through wanted */
      case bEventEstimateCommand:
        /* Fall-through wanted */
      case bEventPluginCommand:
        /* Fall-through wanted */
      case bEventNewPluginOptions:
        /* See if we already loaded the Python modules. */
        if (!plugin_priv_ctx->python_loaded) {
          retval = PyLoadModule(plugin_ctx, plugin_options.c_str());
        }

        /* Only try to call when the loading succeeded. */
        if (retval == bRC_OK) {
          retval = Bareosfd_PyParsePluginDefinition(plugin_ctx,
                                                    plugin_options.c_str());
        }
        break;
      case bEventRestoreObject: {
        struct restore_object_pkt* rop;

        rop = (struct restore_object_pkt*)value;
        if (!rop) {
          /*
           * If rop == NULL this means we got the last restore object.
           * No need to call into python so just return.
           */
          retval = bRC_OK;
        } else {
          /* See if we already loaded the Python modules. */
          if (!plugin_priv_ctx->python_loaded && *rop->plugin_name) {
            retval = PyLoadModule(plugin_ctx, plugin_options.c_str());

            /* Only try to call when the loading succeeded. */
            if (retval == bRC_OK) {
              retval = Bareosfd_PyParsePluginDefinition(plugin_ctx,
                                                        plugin_options.c_str());
              if (retval == bRC_OK) {
                retval = Bareosfd_PyRestoreObjectData(plugin_ctx, rop);
              }
            }
          } else {
            retval = Bareosfd_PyRestoreObjectData(plugin_ctx, rop);
          }
        }
        break;
      }
      case bEventHandleBackupFile:
        retval =
            Bareosfd_PyHandleBackupFile(plugin_ctx, (struct save_pkt*)value);
        break;
      default:
        /*
         * Handle the generic events e.g. the ones which are just passed on.
         * We only try to call Python when we loaded the right module until
         * that time we pretend the call succeeded.
         */
        if (plugin_priv_ctx->python_loaded) {
          retval = Bareosfd_PyHandlePluginEvent(plugin_ctx, event, value);
        } else {
          retval = bRC_OK;
        }
        break;
    }

    PyEval_ReleaseThread(plugin_priv_ctx->interpreter);
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
static bRC startBackupFile(PluginContext* plugin_ctx, struct save_pkt* sp)
{
  bRC retval = bRC_Error;
  struct plugin_private_context* plugin_priv_ctx =
      (struct plugin_private_context*)plugin_ctx->plugin_private_context;

  if (!plugin_priv_ctx) { goto bail_out; }

  PyEval_AcquireThread(plugin_priv_ctx->interpreter);
  retval = Bareosfd_PyStartBackupFile(plugin_ctx, sp);
  PyEval_ReleaseThread(plugin_priv_ctx->interpreter);

  /*
   * For Incremental and Differential backups use checkChanges method to
   * see if we need to backup this file.
   */
  switch (plugin_priv_ctx->backup_level) {
    case L_INCREMENTAL:
    case L_DIFFERENTIAL:
      /*
       * If the plugin didn't set a save_time but we have a since time
       * from the bEventSince event we use that as basis for the actual
       * save_time to check.
       */
      if (sp->save_time == 0 && plugin_priv_ctx->since) {
        sp->save_time = plugin_priv_ctx->since;
      }

      switch (bareos_core_functions->checkChanges(plugin_ctx, sp)) {
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
static bRC endBackupFile(PluginContext* plugin_ctx)
{
  bRC retval = bRC_Error;
  struct plugin_private_context* plugin_priv_ctx =
      (struct plugin_private_context*)plugin_ctx->plugin_private_context;

  if (!plugin_priv_ctx) { goto bail_out; }

  PyEval_AcquireThread(plugin_priv_ctx->interpreter);
  retval = Bareosfd_PyEndBackupFile(plugin_ctx);
  PyEval_ReleaseThread(plugin_priv_ctx->interpreter);

bail_out:
  return retval;
}

/**
 * Do actual I/O. Bareos calls this after startBackupFile
 * or after startRestoreFile to do the actual file
 * input or output.
 */
static bRC pluginIO(PluginContext* plugin_ctx, struct io_pkt* io)
{
  bRC retval = bRC_Error;
  struct plugin_private_context* plugin_priv_ctx =
      (struct plugin_private_context*)plugin_ctx->plugin_private_context;

  if (!plugin_priv_ctx) { goto bail_out; }

  if (!plugin_priv_ctx->python_loaded) { goto bail_out; }

  PyEval_AcquireThread(plugin_priv_ctx->interpreter);
  retval = Bareosfd_PyPluginIO(plugin_ctx, io);
  PyEval_ReleaseThread(plugin_priv_ctx->interpreter);

bail_out:
  return retval;
}

/**
 * Start restore of a file.
 */
static bRC startRestoreFile(PluginContext* plugin_ctx, const char* cmd)
{
  bRC retval = bRC_Error;
  struct plugin_private_context* plugin_priv_ctx =
      (struct plugin_private_context*)plugin_ctx->plugin_private_context;

  if (!plugin_priv_ctx) { goto bail_out; }

  PyEval_AcquireThread(plugin_priv_ctx->interpreter);
  retval = Bareosfd_PyStartRestoreFile(plugin_ctx, cmd);
  PyEval_ReleaseThread(plugin_priv_ctx->interpreter);

bail_out:
  return retval;
}

/**
 * Done restoring a file.
 */
static bRC endRestoreFile(PluginContext* plugin_ctx)
{
  bRC retval = bRC_Error;
  struct plugin_private_context* plugin_priv_ctx =
      (struct plugin_private_context*)plugin_ctx->plugin_private_context;

  if (!plugin_priv_ctx) { goto bail_out; }

  PyEval_AcquireThread(plugin_priv_ctx->interpreter);
  retval = Bareosfd_PyEndRestoreFile(plugin_ctx);
  PyEval_ReleaseThread(plugin_priv_ctx->interpreter);

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
static bRC createFile(PluginContext* plugin_ctx, struct restore_pkt* rp)
{
  bRC retval = bRC_Error;
  struct plugin_private_context* plugin_priv_ctx =
      (struct plugin_private_context*)plugin_ctx->plugin_private_context;

  if (!plugin_priv_ctx) { goto bail_out; }

  PyEval_AcquireThread(plugin_priv_ctx->interpreter);
  retval = Bareosfd_PyCreateFile(plugin_ctx, rp);
  PyEval_ReleaseThread(plugin_priv_ctx->interpreter);

bail_out:
  return retval;
}

/**
 * Called after the file has been restored. This can be used to
 * set directory permissions, ...
 */
static bRC setFileAttributes(PluginContext* plugin_ctx, struct restore_pkt* rp)
{
  bRC retval = bRC_Error;
  struct plugin_private_context* plugin_priv_ctx =
      (struct plugin_private_context*)plugin_ctx->plugin_private_context;

  if (!plugin_priv_ctx) { goto bail_out; }

  PyEval_AcquireThread(plugin_priv_ctx->interpreter);
  retval = Bareosfd_PySetFileAttributes(plugin_ctx, rp);
  PyEval_ReleaseThread(plugin_priv_ctx->interpreter);

bail_out:
  return retval;
}

/**
 * When using Incremental dump, all previous dumps are necessary
 */
static bRC checkFile(PluginContext* plugin_ctx, char* fname)
{
  bRC retval = bRC_Error;
  struct plugin_private_context* plugin_priv_ctx =
      (struct plugin_private_context*)plugin_ctx->plugin_private_context;

  if (!plugin_priv_ctx) { goto bail_out; }

  if (!plugin_priv_ctx->python_loaded) { return bRC_OK; }

  PyEval_AcquireThread(plugin_priv_ctx->interpreter);
  retval = Bareosfd_PyCheckFile(plugin_ctx, fname);
  PyEval_ReleaseThread(plugin_priv_ctx->interpreter);

bail_out:
  return retval;
}

/**
 */
static bRC getAcl(PluginContext* plugin_ctx, acl_pkt* ap)
{
  bRC retval = bRC_Error;
  struct plugin_private_context* plugin_priv_ctx =
      (struct plugin_private_context*)plugin_ctx->plugin_private_context;

  if (!plugin_priv_ctx) { goto bail_out; }

  PyEval_AcquireThread(plugin_priv_ctx->interpreter);
  retval = Bareosfd_PyGetAcl(plugin_ctx, ap);
  PyEval_ReleaseThread(plugin_priv_ctx->interpreter);

bail_out:
  return retval;
}

/**
 */
static bRC setAcl(PluginContext* plugin_ctx, acl_pkt* ap)
{
  bRC retval = bRC_Error;
  struct plugin_private_context* plugin_priv_ctx =
      (struct plugin_private_context*)plugin_ctx->plugin_private_context;

  if (!plugin_priv_ctx) { goto bail_out; }

  PyEval_AcquireThread(plugin_priv_ctx->interpreter);
  retval = Bareosfd_PySetAcl(plugin_ctx, ap);
  PyEval_ReleaseThread(plugin_priv_ctx->interpreter);

bail_out:
  return retval;
}

/**
 */
static bRC getXattr(PluginContext* plugin_ctx, xattr_pkt* xp)
{
  bRC retval = bRC_Error;
  struct plugin_private_context* plugin_priv_ctx =
      (struct plugin_private_context*)plugin_ctx->plugin_private_context;

  if (!plugin_priv_ctx) { goto bail_out; }

  PyEval_AcquireThread(plugin_priv_ctx->interpreter);
  retval = Bareosfd_PyGetXattr(plugin_ctx, xp);
  PyEval_ReleaseThread(plugin_priv_ctx->interpreter);

bail_out:
  return retval;
}

/**
 */
static bRC setXattr(PluginContext* plugin_ctx, xattr_pkt* xp)
{
  bRC retval = bRC_Error;
  struct plugin_private_context* plugin_priv_ctx =
      (struct plugin_private_context*)plugin_ctx->plugin_private_context;

  if (!plugin_priv_ctx) { goto bail_out; }

  PyEval_AcquireThread(plugin_priv_ctx->interpreter);
  retval = Bareosfd_PySetXattr(plugin_ctx, xp);
  PyEval_ReleaseThread(plugin_priv_ctx->interpreter);

bail_out:
  return retval;
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
 * Parse the plugin definition passed in.
 *
 * The definition is in this form:
 *
 * python:module_path=<path>:module_name=<python_module_name>:...
 */
static bRC parse_plugin_definition(PluginContext* plugin_ctx,
                                   void* value,
                                   PoolMem& plugin_options)
{
  bool found;
  int i, cnt;
  bool keep_existing;
  PoolMem plugin_definition(PM_FNAME);
  char *bp, *argument, *argument_value;
  plugin_private_context* plugin_priv_ctx =
      (plugin_private_context*)plugin_ctx->plugin_private_context;

  if (!value) { return bRC_Error; }

  /*
   * Skip this plugin when getting plugin definition "*all*"
   * This allows to restore a Windows Backup on a Linux FD with
   * Python Plugins enabled.
   */
  if (bstrcmp((char*)value, "*all*")) {
    Dmsg(plugin_ctx, debuglevel,
         LOGPREFIX "Got plugin definition %s, skipping to ignore\n",
         (char*)value);
    return bRC_Skip;
  }

  keep_existing = (plugin_priv_ctx->plugin_options) ? true : false;

  /*
   * Parse the plugin definition.
   * Make a private copy of the whole string.
   */
  if (!plugin_priv_ctx->python_loaded && plugin_priv_ctx->plugin_options) {
    int len;

    /*
     * We got some option string which got pushed before we actual were able to
     * send it to the python module as the entry point was not instantiated. So
     * we prepend that now in the option string and append the new option string
     * with the first argument being the pluginname removed as that is already
     * part of the other plugin option string.
     */
    len = strlen(plugin_priv_ctx->plugin_options);
    PmStrcpy(plugin_definition, plugin_priv_ctx->plugin_options);

    bp = strchr((char*)value, ':');
    if (!bp) {
      Jmsg(plugin_ctx, M_FATAL, LOGPREFIX "Illegal plugin definition %s\n",
           (char*)value);
      Dmsg(plugin_ctx, debuglevel, LOGPREFIX "Illegal plugin definition %s\n",
           (char*)value);
      goto bail_out;
    }

    /*
     * See if option string end with ':'
     */
    if (plugin_priv_ctx->plugin_options[len - 1] != ':') {
      PmStrcat(plugin_definition, (char*)bp);
    } else {
      PmStrcat(plugin_definition, (char*)bp + 1);
    }
  } else {
    PmStrcpy(plugin_definition, (char*)value);
  }

  bp = strchr(plugin_definition.c_str(), ':');
  if (!bp) {
    Jmsg(plugin_ctx, M_FATAL, LOGPREFIX "Illegal plugin definition %s\n",
         plugin_definition.c_str());
    Dmsg(plugin_ctx, debuglevel, LOGPREFIX "Illegal plugin definition %s\n",
         plugin_definition.c_str());
    goto bail_out;
  }

  /*
   * Skip the first ':'
   */
  bp++;

  cnt = 0;
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
      Jmsg(plugin_ctx, M_FATAL, LOGPREFIX "Illegal argument %s without value\n",
           argument);
      Dmsg(plugin_ctx, debuglevel,
           LOGPREFIX "Illegal argument %s without value\n", argument);
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
      if (Bstrcasecmp(argument, plugin_arguments[i].name)) {
        char** str_destination = NULL;
        bool* bool_destination = NULL;

        switch (plugin_arguments[i].type) {
          case argument_module_path:
            str_destination = &plugin_priv_ctx->module_path;
            break;
          case argument_module_name:
            str_destination = &plugin_priv_ctx->module_name;
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
         * Set any boolean variable.
         */
        if (bool_destination) {
          *bool_destination = ParseBoolean(argument_value);
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
        PmStrcat(plugin_options, option.c_str());
      } else {
        Mmsg(option, "%s=%s", argument, argument_value);
        PmStrcat(plugin_options, option.c_str());
      }
      cnt++;
    }
  }

  if (cnt > 0) { PmStrcat(plugin_options, ":"); }

  return bRC_OK;

bail_out:
  return bRC_Error;
}


/**
 * Initial load of the Python module.
 *
 * Based on the parsed plugin options we set some prerequisites like the
 * module path and the module to load. We also load the dictionary used
 * for looking up the Python methods.
 */
static bRC PyLoadModule(PluginContext* plugin_ctx, void* value)
{
  bRC retval = bRC_Error;
  struct plugin_private_context* plugin_priv_ctx =
      (struct plugin_private_context*)plugin_ctx->plugin_private_context;
  PyObject *sysPath, *mPath, *pName, *pFunc;

  /* See if we already setup the python search path.  */
  if (!plugin_priv_ctx->python_path_set) {
    /* Extend the Python search path with the given module_path.  */
    if (plugin_priv_ctx->module_path) {
      sysPath = PySys_GetObject((char*)"path");
      mPath = PyUnicode_FromString(plugin_priv_ctx->module_path);
      PyList_Append(sysPath, mPath);
      Py_DECREF(mPath);
      plugin_priv_ctx->python_path_set = true;
    }
  }

  /* Try to load the Python module by name. */
  if (plugin_priv_ctx->module_name) {
    Dmsg(plugin_ctx, debuglevel,
         LOGPREFIX "Trying to load module with name %s\n",
         plugin_priv_ctx->module_name);
    pName = PyUnicode_FromString(plugin_priv_ctx->module_name);
    plugin_priv_ctx->pModule = PyImport_Import(pName);
    Py_DECREF(pName);

    if (!plugin_priv_ctx->pModule) {
      Dmsg(plugin_ctx, debuglevel,
           LOGPREFIX "Failed to load module with name %s\n",
           plugin_priv_ctx->module_name);
      goto bail_out;
    }

    Dmsg(plugin_ctx, debuglevel,
         LOGPREFIX "Successfully loaded module with name %s\n",
         plugin_priv_ctx->module_name);

    /* Get the Python dictionary for lookups in the Python namespace.  */
    plugin_priv_ctx->pyModuleFunctionsDict =
        PyModule_GetDict(plugin_priv_ctx->pModule); /* Borrowed reference */


    /* Lookup the load_bareos_plugin() function in the python module.  */
    pFunc = PyDict_GetItemString(plugin_priv_ctx->pyModuleFunctionsDict,
                                 "load_bareos_plugin"); /* Borrowed reference */
    if (pFunc && PyCallable_Check(pFunc)) {
      PyObject *pPluginDefinition, *pRetVal;

      pPluginDefinition = PyUnicode_FromString((char*)value);
      if (!pPluginDefinition) { goto bail_out; }

      pRetVal = PyObject_CallFunctionObjArgs(pFunc, pPluginDefinition, NULL);
      Py_DECREF(pPluginDefinition);

      if (!pRetVal) {
        goto bail_out;
      } else {
        retval = ConvertPythonRetvalTobRCRetval(pRetVal);
        Py_DECREF(pRetVal);
      }
    } else {
      Dmsg(plugin_ctx, debuglevel,
           LOGPREFIX "Failed to find function named load_bareos_plugin()\n");
      goto bail_out;
    }

    /*
     * Keep track we successfully loaded.
     */
    plugin_priv_ctx->python_loaded = true;
  }

  return retval;

bail_out:
  if (PyErr_Occurred()) { PyErrorHandler(plugin_ctx, M_FATAL); }

  return retval;
}

static bRC getPluginValue(PluginContext* bareos_plugin_ctx,
                          pVariable var,
                          void* value)
{
  struct plugin_private_context* plugin_priv_ctx =
      (struct plugin_private_context*)bareos_plugin_ctx->plugin_private_context;
  bRC retval = bRC_Error;

  if (!plugin_priv_ctx) { goto bail_out; }

  PyEval_AcquireThread(plugin_priv_ctx->interpreter);
  retval = Bareosfd_PyGetPluginValue(bareos_plugin_ctx, var, value);
  PyEval_ReleaseThread(plugin_priv_ctx->interpreter);

bail_out:
  return retval;
}

static bRC setPluginValue(PluginContext* bareos_plugin_ctx,
                          pVariable var,
                          void* value)
{
  struct plugin_private_context* plugin_priv_ctx =
      (struct plugin_private_context*)bareos_plugin_ctx->plugin_private_context;
  bRC retval = bRC_Error;

  if (!plugin_priv_ctx) { return bRC_Error; }

  PyEval_AcquireThread(plugin_priv_ctx->interpreter);
  retval = Bareosfd_PySetPluginValue(bareos_plugin_ctx, var, value);
  PyEval_ReleaseThread(plugin_priv_ctx->interpreter);

  return retval;
}

} /* namespace filedaemon */
