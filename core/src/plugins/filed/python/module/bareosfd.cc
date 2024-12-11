/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2020-2024 Bareos GmbH & Co. KG

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
 * Python module for the Bareos filedaemon plugin
 */
#define PY_SSIZE_T_CLEAN
#define BUILD_PLUGIN

#if defined(HAVE_WIN32)
#  include "include/bareos.h"
#  include <Python.h>
#else
#  include <Python.h>
#  include "include/bareos.h"
#endif

#include "include/version_hex.h"

#define LOGPREFIX "python3-fd-mod: "

#include "filed/fd_plugins.h"

#define BAREOSFD_MODULE
#include "bareosfd.h"
#include "include/filetypes.h"
#include "lib/edit.h"

namespace filedaemon {

static const int debuglevel = 150;

static bRC set_bareos_core_functions(CoreFunctions* new_bareos_core_functions);
static bRC set_plugin_context(PluginContext* new_plugin_context);
static bRC PyParsePluginDefinition(PluginContext* plugin_ctx, void* value);

static bRC PyGetPluginValue(PluginContext* plugin_ctx,
                            pVariable var,
                            void* value);
static bRC PySetPluginValue(PluginContext* plugin_ctx,
                            pVariable var,
                            void* value);
static bRC PyHandlePluginEvent(PluginContext* plugin_ctx,
                               bEvent* event,
                               void* value);
static bRC PyStartBackupFile(PluginContext* plugin_ctx, save_pkt* sp);
static bRC PyEndBackupFile(PluginContext* plugin_ctx);
static bRC PyPluginIO(PluginContext* plugin_ctx, io_pkt* io);
static bRC PyStartRestoreFile(PluginContext* plugin_ctx, const char* cmd);
static bRC PyEndRestoreFile(PluginContext* plugin_ctx);
static bRC PyCreateFile(PluginContext* plugin_ctx, restore_pkt* rp);
static bRC PySetFileAttributes(PluginContext* plugin_ctx, restore_pkt* rp);
static bRC PyCheckFile(PluginContext* plugin_ctx, char* fname);
static bRC PyGetAcl(PluginContext* plugin_ctx, acl_pkt* ap);
static bRC PySetAcl(PluginContext* plugin_ctx, acl_pkt* ap);
static bRC PyGetXattr(PluginContext* plugin_ctx, xattr_pkt* xp);
static bRC PySetXattr(PluginContext* plugin_ctx, xattr_pkt* xp);
static bRC PyRestoreObjectData(PluginContext* plugin_ctx,
                               restore_object_pkt* rop);
static bRC PyHandleBackupFile(PluginContext* plugin_ctx, save_pkt* sp);

/* Pointers to Bareos functions */
static CoreFunctions* bareos_core_functions = NULL;

#include "plugins/filed/python/plugin_private_context.h"

#define NOPLUGINSETGETVALUE 1
/* functions common to all plugins */
#include "plugins/include/python_plugins_common.inc"


/* set the bareos_core_functions pointer to the given value */
static bRC set_bareos_core_functions(CoreFunctions* new_bareos_core_functions)
{
  bareos_core_functions = new_bareos_core_functions;
  return bRC_OK;
}

/* set the plugin context pointer to the given value */
static bRC set_plugin_context(PluginContext* new_plugin_context)
{
  plugin_context = new_plugin_context;
  return bRC_OK;
}

/**
 * Any plugin options which are passed in are dispatched here to a Python
 * method and it can parse the plugin options. This function is also called
 * after PyLoadModule() has loaded the Python module and made sure things are
 * operational. Normally you would only get one set of plugin options but for
 * a restore overrides can be passed in before the actual plugin options are
 * restored as part of the restore stream handling.
 */
static bRC PyParsePluginDefinition(PluginContext* plugin_ctx, void* value)
{
  bRC retval = bRC_Error;
  struct plugin_private_context* plugin_priv_ctx
      = (struct plugin_private_context*)plugin_ctx->plugin_private_context;
  PyObject* pFunc;

  // Lookup the parse_plugin_definition() function in the python module.
  pFunc = PyDict_GetItemString(
      plugin_priv_ctx->pyModuleFunctionsDict,
      "parse_plugin_definition"); /* Borrowed reference */
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

    return retval;
  } else {
    Dmsg(plugin_ctx, debuglevel,
         LOGPREFIX
         "Failed to find function named "
         "parse_plugin_definition()\n");
    return bRC_Error;
  }

bail_out:
  if (PyErr_Occurred()) { PyErrorHandler(plugin_ctx, M_FATAL); }

  return retval;
}

static bRC PyGetPluginValue(PluginContext*, pVariable, void*) { return bRC_OK; }

static bRC PySetPluginValue(PluginContext*, pVariable, void*) { return bRC_OK; }

static bRC PyHandlePluginEvent(PluginContext* plugin_ctx, bEvent* event, void*)
{
  bRC retval = bRC_Error;
  plugin_private_context* plugin_priv_ctx
      = (plugin_private_context*)plugin_ctx->plugin_private_context;
  PyObject* pFunc;

  // Lookup the handle_plugin_event() function in the python module.
  pFunc = PyDict_GetItemString(plugin_priv_ctx->pyModuleFunctionsDict,
                               "handle_plugin_event"); /* Borrowed reference */
  if (pFunc && PyCallable_Check(pFunc)) {
    PyObject *pEventType, *pRetVal;

    pEventType = PyLong_FromLong(event->eventType);

    pRetVal = PyObject_CallFunctionObjArgs(pFunc, pEventType, NULL);
    Py_DECREF(pEventType);

    if (!pRetVal) {
      goto bail_out;
    } else {
      retval = ConvertPythonRetvalTobRCRetval(pRetVal);
      Py_DECREF(pRetVal);
    }
  } else {
    Dmsg(plugin_ctx, debuglevel,
         LOGPREFIX "Failed to find function named handle_plugin_event()\n");
  }

  return retval;

bail_out:
  if (PyErr_Occurred()) { PyErrorHandler(plugin_ctx, M_FATAL); }

  return retval;
}

static inline PyStatPacket* NativeToPyStatPacket(struct stat* statp)
{
  PyStatPacket* pStatp = PyObject_New(PyStatPacket, &PyStatPacketType);

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

static inline void PyStatPacketToNative(PyStatPacket* pStatp,
                                        struct stat* statp)
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

static inline PySavePacket* NativeToPySavePacket(save_pkt* sp)
{
  PySavePacket* pSavePkt = PyObject_New(PySavePacket, &PySavePacketType);

  if (pSavePkt) {
    pSavePkt->fname = PyUnicode_FromString(sp->fname ? sp->fname : "");
    pSavePkt->link = PyUnicode_FromString(sp->link ? sp->link : "");
    if (sp->statp.st_mode) {
      pSavePkt->statp = (PyObject*)NativeToPyStatPacket(&sp->statp);
    } else {
      pSavePkt->statp = NULL;
    }

    pSavePkt->type = sp->type;
    pSavePkt->flags
        = PyByteArray_FromStringAndSize(sp->flags, sizeof(sp->flags));
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
}  // namespace filedaemon

static inline bool PySavePacketToNative(
    PySavePacket* pSavePkt,
    save_pkt* sp,
    struct plugin_private_context* plugin_priv_ctx,
    bool is_options_plugin)
{
  // See if this is for an Options Plugin.
  if (!is_options_plugin) {
    // Only copy back the arguments that are allowed to change.
    if (pSavePkt->fname) {
      /* As this has to linger as long as the backup is running we save it in
       * our plugin context. */
      if (PyUnicode_Check(pSavePkt->fname)) {
        if (plugin_priv_ctx->fname) { free(plugin_priv_ctx->fname); }

        const char* fileName_AsUTF8 = PyUnicode_AsUTF8(pSavePkt->fname);
        if (!fileName_AsUTF8) return false;

        plugin_priv_ctx->fname = strdup(fileName_AsUTF8);
        sp->fname = plugin_priv_ctx->fname;
      } else {
        PyErr_SetString(PyExc_TypeError,
                        "fname needs to be of type string \"utf-8\"");
        return false;
      }
    } else {
      PyErr_SetString(PyExc_RuntimeError, "fname is empty");
      return false;
    }

    // Optional field.
    if (pSavePkt->link) {
      /* As this has to linger as long as the backup is running we save it in
       * our plugin context. */
      if (PyUnicode_Check(pSavePkt->link)) {
        if (plugin_priv_ctx->link) { free(plugin_priv_ctx->link); }
        plugin_priv_ctx->link = strdup(PyUnicode_AsUTF8(pSavePkt->link));
        sp->link = plugin_priv_ctx->link;
      } else {
        PyErr_SetString(PyExc_TypeError,
                        "if given, link needs to be of type string \"utf-8\"");
        return false;
      }
    }

    // Handle the stat structure.
    if (pSavePkt->statp) {
      PyStatPacketToNative((PyStatPacket*)pSavePkt->statp, &sp->statp);
    } else {
      PyErr_SetString(PyExc_RuntimeError, "PyStatPacketToNative() failed");
      return false;
    }

    sp->type = pSavePkt->type;

    if (PyByteArray_Check(pSavePkt->flags)) {
      char* flags;

      if (PyByteArray_Size(pSavePkt->flags) != sizeof(sp->flags)) {
        PyErr_SetString(PyExc_RuntimeError, "PyByteArray_Size(flags) failed");
        return false;
      }
      if ((flags = PyByteArray_AsString(pSavePkt->flags))) {
        memcpy(sp->flags, flags, sizeof(sp->flags));
      } else {
        PyErr_SetString(PyExc_RuntimeError,
                        "PyByteArray_AsString(flags) failed");
        return false;
      }
    } else {
      PyErr_SetString(PyExc_TypeError, "flags need to be of type bytearray");
      return false;
    }

    // Special code for handling restore objects.
    if (IS_FT_OBJECT(sp->type)) {
      // See if a proper restore object was created.
      if (pSavePkt->object_len > 0) {
        /* As this has to linger as long as the backup is running we save it
         * in our plugin context. */
        bool object_name_exists = pSavePkt->object_name;
        bool object_name_is_unicode = PyUnicode_Check(pSavePkt->object_name);
        bool object_exists = pSavePkt->object;
        bool object_is_bytearray = PyByteArray_Check(pSavePkt->object);
        if (object_name_exists && object_name_is_unicode && object_exists
            && object_is_bytearray) {
          char* buf;

          if (plugin_priv_ctx->object_name) {
            free(plugin_priv_ctx->object_name);
          }
          plugin_priv_ctx->object_name
              = strdup(PyUnicode_AsUTF8(pSavePkt->object_name));
          sp->object_name = plugin_priv_ctx->object_name;

          sp->object_len = pSavePkt->object_len;
          sp->index = pSavePkt->object_index;

          if ((buf = PyByteArray_AsString(pSavePkt->object))) {
            if (plugin_priv_ctx->object) { free(plugin_priv_ctx->object); }
            plugin_priv_ctx->object = (char*)malloc(pSavePkt->object_len);
            memcpy(plugin_priv_ctx->object, buf, pSavePkt->object_len);
            sp->object = plugin_priv_ctx->object;
          } else {
            return false;
          }
        } else {
          std::string err_string{};
          if (!object_name_exists) { err_string = "object name missing"; };
          if (!object_name_is_unicode) {
            err_string = "object name must be unicode type";
          };
          if (!object_exists) { err_string = "object missing"; };
          if (!object_is_bytearray) {
            err_string = "object needs to be of type bytearray";
          };

          PyErr_SetString(PyExc_RuntimeError, err_string.c_str());
          return false;
        }
      } else {
        PyErr_SetString(PyExc_RuntimeError, "pSavePkt->object_len is <=0");
        return false;
      }
    } else {
      sp->no_read = pSavePkt->no_read;
      sp->delta_seq = pSavePkt->delta_seq;
      sp->save_time = pSavePkt->save_time;
    }
  } else {
    sp->no_read = pSavePkt->no_read;
    sp->delta_seq = pSavePkt->delta_seq;
    sp->save_time = pSavePkt->save_time;

    if (PyByteArray_Check(pSavePkt->flags)) {
      char* flags;

      if (PyByteArray_Size(pSavePkt->flags) != sizeof(sp->flags)) {
        PyErr_SetString(PyExc_RuntimeError, "PyByteArray_Size(flags) failed");
        return false;
      }

      if ((flags = PyByteArray_AsString(pSavePkt->flags))) {
        memcpy(sp->flags, flags, sizeof(sp->flags));
      } else {
        PyErr_SetString(PyExc_RuntimeError,
                        "PyByteArray_AsString(flags) failed");
        return false;
      }
    } else {
      PyErr_SetString(PyExc_TypeError, "flags need to be of type bytearray");
      return false;
    }
  }
  return true;
}

/**
 * Called when starting to backup a file. Here the plugin must
 * return the "stat" packet for the directory/file and provide
 * certain information so that Bareos knows what the file is.
 * The plugin can create "Virtual" files by giving them a
 * name that is not normally found on the file system.
 */
static bRC PyStartBackupFile(PluginContext* plugin_ctx, save_pkt* sp)
{
  bRC retval = bRC_Error;
  struct plugin_private_context* plugin_priv_ctx
      = (struct plugin_private_context*)plugin_ctx->plugin_private_context;
  PyObject* pFunc;

  // Lookup the start_backup_file() function in the python module.
  pFunc = PyDict_GetItemString(plugin_priv_ctx->pyModuleFunctionsDict,
                               "start_backup_file"); /* Borrowed reference */
  if (pFunc && PyCallable_Check(pFunc)) {
    PySavePacket* pSavePkt;
    PyObject* pRetVal;

    pSavePkt = NativeToPySavePacket(sp);
    if (!pSavePkt) {
      Dmsg(plugin_ctx, debuglevel,
           LOGPREFIX "Failed to convert save packet to python.\n");
      goto bail_out;
    }

    pRetVal = PyObject_CallFunctionObjArgs(pFunc, (PyObject*)pSavePkt, NULL);
    if (!pRetVal) {
      Py_DECREF((PyObject*)pSavePkt);
      goto bail_out;
    } else {
      retval = ConvertPythonRetvalTobRCRetval(pRetVal);
      Py_DECREF(pRetVal);

      if (retval == bRC_OK
          && !PySavePacketToNative(pSavePkt, sp, plugin_priv_ctx, false)) {
        Dmsg(plugin_ctx, debuglevel,
             LOGPREFIX "Failed to convert save packet to native.\n");
        Py_DECREF((PyObject*)pSavePkt);
        goto bail_out;
      }
      Py_DECREF((PyObject*)pSavePkt);
    }
  } else {
    Dmsg(plugin_ctx, debuglevel,
         LOGPREFIX "Failed to find function named start_backup_file()\n");
  }

  return retval;

bail_out:
  if (PyErr_Occurred()) { PyErrorHandler(plugin_ctx, M_FATAL); }

  return bRC_Error;
}

/**
 * Called at the end of backing up a file for a command plugin.
 * If the plugin's work is done, it should return bRC_OK.
 * If the plugin wishes to create another file and back it up,
 * then it must return bRC_More (not yet implemented).
 */
static bRC PyEndBackupFile(PluginContext* plugin_ctx)
{
  bRC retval = bRC_Error;
  struct plugin_private_context* plugin_priv_ctx
      = (struct plugin_private_context*)plugin_ctx->plugin_private_context;
  PyObject* pFunc;

  // Lookup the end_backup_file() function in the python module.
  pFunc = PyDict_GetItemString(plugin_priv_ctx->pyModuleFunctionsDict,
                               "end_backup_file"); /* Borrowed reference */
  if (pFunc && PyCallable_Check(pFunc)) {
    PyObject* pRetVal;

    pRetVal = PyObject_CallFunctionObjArgs(pFunc, NULL);
    if (!pRetVal) {
      goto bail_out;
    } else {
      retval = ConvertPythonRetvalTobRCRetval(pRetVal);
    }
  } else {
    Dmsg(plugin_ctx, debuglevel,
         LOGPREFIX "Failed to find function named end_backup_file()\n");
  }

  return retval;

bail_out:
  if (PyErr_Occurred()) { PyErrorHandler(plugin_ctx, M_FATAL); }

  return retval;
}

static inline PyIoPacket* NativeToPyIoPacket(io_pkt* io)
{
  PyIoPacket* pIoPkt = PyObject_New(PyIoPacket, &PyIoPacketType);

  if (pIoPkt) {
    // Initialize the Python IoPkt with the data we got passed in.
    pIoPkt->func = io->func;
    pIoPkt->count = io->count;
    pIoPkt->flags = io->flags;
    pIoPkt->mode = io->mode;
    pIoPkt->fname = io->fname;
    pIoPkt->whence = io->whence;
    pIoPkt->offset = io->offset;
    pIoPkt->filedes = io->filedes;

    if (io->func == IO_WRITE && io->count > 0) {
      /* Only initialize the buffer with read data when we are writing and
       * there is data.*/
      pIoPkt->buf = PyByteArray_FromStringAndSize(io->buf, io->count);
      if (!pIoPkt->buf) {
        Py_DECREF((PyObject*)pIoPkt);
        return (PyIoPacket*)NULL;
      }
    } else {
      pIoPkt->buf = NULL;
    }
    /* These must be set by the Python function but we initialize them to zero
     * to be sure they have some valid setting an not random data.  */
    pIoPkt->io_errno = 0;
    pIoPkt->lerror = 0;
    pIoPkt->win32 = false;
    pIoPkt->status = 0;
  }

  return pIoPkt;
}

static inline bool PyIoPacketToNative(PyIoPacket* pIoPkt, io_pkt* io)
{
  // Only copy back the arguments that are allowed to change.
  io->io_errno = pIoPkt->io_errno;
  io->lerror = pIoPkt->lerror;
  io->win32 = pIoPkt->win32;
  io->status = pIoPkt->status;
  io->filedes = pIoPkt->filedes;

  if (io->func == IO_READ && io->status > 0) {
    // Only copy back the data when doing a read and there is data.
    if (PyByteArray_Check(pIoPkt->buf)) {
      char* buf;

      if (PyByteArray_Size(pIoPkt->buf) > io->count || io->status > io->count) {
        return false;
      }

      if (!(buf = PyByteArray_AsString(pIoPkt->buf))) { return false; }
      memcpy(io->buf, buf, io->status);
    } else if (PyBytes_Check(pIoPkt->buf)) {
      char* buf;

      if (PyBytes_Size(pIoPkt->buf) > io->count || io->status > io->count) {
        return false;
      }

      if (!(buf = PyBytes_AsString(pIoPkt->buf))) { return false; }
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
static bRC PyPluginIO(PluginContext* plugin_ctx, io_pkt* io)
{
  bRC retval = bRC_Error;
  struct plugin_private_context* plugin_priv_ctx
      = (struct plugin_private_context*)plugin_ctx->plugin_private_context;
  PyObject* pFunc;

  // Lookup the plugin_io() function in the python module.
  pFunc = PyDict_GetItemString(plugin_priv_ctx->pyModuleFunctionsDict,
                               "plugin_io"); /* Borrowed reference */
  if (pFunc && PyCallable_Check(pFunc)) {
    PyIoPacket* pIoPkt;
    PyObject* pRetVal;

    pIoPkt = NativeToPyIoPacket(io);
    if (!pIoPkt) { goto bail_out; }

    pRetVal = PyObject_CallFunctionObjArgs(pFunc, (PyObject*)pIoPkt, NULL);
    if (!pRetVal) {
      Py_DECREF((PyObject*)pIoPkt);
      goto bail_out;
    } else {
      retval = ConvertPythonRetvalTobRCRetval(pRetVal);
      Py_DECREF(pRetVal);

      if (!PyIoPacketToNative(pIoPkt, io)) {
        Py_DECREF((PyObject*)pIoPkt);
        goto bail_out;
      }
    }
    Py_DECREF((PyObject*)pIoPkt);
  } else {
    Dmsg(plugin_ctx, debuglevel,
         LOGPREFIX "Failed to find function named plugin_io()\n");
  }

  return retval;

bail_out:
  if (PyErr_Occurred()) { PyErrorHandler(plugin_ctx, M_FATAL); }

  io->status = -1;

  return retval;
}

/**
 * Called when the first record is read from the Volume that was previously
 * written by the command plugin.
 */
static bRC PyStartRestoreFile(PluginContext* plugin_ctx, const char* cmd)
{
  bRC retval = bRC_Error;
  struct plugin_private_context* plugin_priv_ctx
      = (struct plugin_private_context*)plugin_ctx->plugin_private_context;
  PyObject* pFunc;

  // Lookup the start_restore_file() function in the python module.
  pFunc = PyDict_GetItemString(plugin_priv_ctx->pyModuleFunctionsDict,
                               "start_restore_file"); /* Borrowed reference */
  if (pFunc && PyCallable_Check(pFunc)) {
    PyObject *pCmd, *pRetVal;

    pCmd = PyUnicode_FromString(cmd);
    if (!pCmd) { goto bail_out; }

    pRetVal = PyObject_CallFunctionObjArgs(pFunc, pCmd, NULL);
    Py_DECREF(pCmd);

    if (!pRetVal) {
      goto bail_out;
    } else {
      retval = ConvertPythonRetvalTobRCRetval(pRetVal);
      Py_DECREF(pRetVal);
    }
  } else {
    Dmsg(plugin_ctx, debuglevel,
         LOGPREFIX "Failed to find function named start_restore_file()\n");
  }

  return retval;

bail_out:
  if (PyErr_Occurred()) { PyErrorHandler(plugin_ctx, M_FATAL); }

  return retval;
}

// Called when a command plugin is done restoring a file.
static bRC PyEndRestoreFile(PluginContext* plugin_ctx)
{
  bRC retval = bRC_Error;
  struct plugin_private_context* plugin_priv_ctx
      = (struct plugin_private_context*)plugin_ctx->plugin_private_context;
  PyObject* pFunc;

  // Lookup the end_restore_file() function in the python module.
  pFunc = PyDict_GetItemString(plugin_priv_ctx->pyModuleFunctionsDict,
                               "end_restore_file"); /* Borrowed reference */
  if (pFunc && PyCallable_Check(pFunc)) {
    PyObject* pRetVal;

    pRetVal = PyObject_CallFunctionObjArgs(pFunc, NULL);
    if (!pRetVal) {
      goto bail_out;
    } else {
      retval = ConvertPythonRetvalTobRCRetval(pRetVal);
    }
  } else {
    Dmsg(plugin_ctx, debuglevel,
         LOGPREFIX "Failed to find function named end_restore_file()\n");
  }

  return retval;

bail_out:
  if (PyErr_Occurred()) { PyErrorHandler(plugin_ctx, M_FATAL); }

  return retval;
}

static inline PyRestorePacket* NativeToPyRestorePacket(restore_pkt* rp)
{
  PyRestorePacket* pRestorePacket
      = PyObject_New(PyRestorePacket, &PyRestorePacketType);

  if (pRestorePacket) {
    pRestorePacket->stream = rp->stream;
    pRestorePacket->data_stream = rp->data_stream;
    pRestorePacket->type = rp->type;
    pRestorePacket->file_index = rp->file_index;
    pRestorePacket->LinkFI = rp->LinkFI;
    pRestorePacket->uid = rp->uid;
    pRestorePacket->statp = (PyObject*)NativeToPyStatPacket(&rp->statp);
    pRestorePacket->attrEx = rp->attrEx;
    pRestorePacket->ofname = rp->ofname;
    pRestorePacket->olname = rp->olname;
    pRestorePacket->where = rp->where;
    pRestorePacket->RegexWhere = rp->RegexWhere;
    pRestorePacket->replace = rp->replace;
    pRestorePacket->create_status = rp->create_status;
    pRestorePacket->filedes = rp->filedes;
  }

  return pRestorePacket;
}

static inline void PyRestorePacketToNative(PyRestorePacket* pRestorePacket,
                                           restore_pkt* rp)
{
  // Only copy back the fields that are allowed to be changed.
  rp->create_status = pRestorePacket->create_status;
  rp->filedes = pRestorePacket->filedes;
}

/**
 * Called for a command plugin to create a file during a Restore job before
 * restoring the data. This entry point is called before any I/O is done on
 * the file. After this call, Bareos will call pluginIO() to open the file for
 * write.
 *
 * We must return in rp->create_status:
 *
 * CF_ERROR    -- error
 * CF_SKIP     -- skip processing this file
 * CF_EXTRACT  -- extract the file (i.e.call i/o routines)
 * CF_CREATED  -- created, but no content to extract (typically directories)
 */
static bRC PyCreateFile(PluginContext* plugin_ctx, restore_pkt* rp)
{
  bRC retval = bRC_Error;
  struct plugin_private_context* plugin_priv_ctx
      = (struct plugin_private_context*)plugin_ctx->plugin_private_context;
  PyObject* pFunc;

  if (!rp) { return bRC_Error; }

  // Lookup the create_file() function in the python module.
  pFunc = PyDict_GetItemString(plugin_priv_ctx->pyModuleFunctionsDict,
                               "create_file"); /* Borrowed reference */
  if (pFunc && PyCallable_Check(pFunc)) {
    PyRestorePacket* pRestorePacket;
    PyObject* pRetVal;

    pRestorePacket = NativeToPyRestorePacket(rp);
    if (!pRestorePacket) { goto bail_out; }

    pRetVal = PyObject_CallFunctionObjArgs(pFunc, pRestorePacket, NULL);
    if (!pRetVal) {
      Py_DECREF(pRestorePacket);
      goto bail_out;
    } else {
      retval = ConvertPythonRetvalTobRCRetval(pRetVal);
      Py_DECREF(pRetVal);

      PyRestorePacketToNative(pRestorePacket, rp);
      Py_DECREF(pRestorePacket);
    }
  } else {
    Dmsg(plugin_ctx, debuglevel,
         LOGPREFIX "Failed to find function named create_file()\n");
  }

  return retval;

bail_out:
  if (PyErr_Occurred()) { PyErrorHandler(plugin_ctx, M_FATAL); }

  return retval;
}

static bRC PySetFileAttributes(PluginContext* plugin_ctx, restore_pkt* rp)
{
  bRC retval = bRC_Error;
  struct plugin_private_context* plugin_priv_ctx
      = (struct plugin_private_context*)plugin_ctx->plugin_private_context;
  PyObject* pFunc;

  if (!rp) { return bRC_Error; }

  // Lookup the set_file_attributes() function in the python module.
  pFunc = PyDict_GetItemString(plugin_priv_ctx->pyModuleFunctionsDict,
                               "set_file_attributes"); /* Borrowed reference */
  if (pFunc && PyCallable_Check(pFunc)) {
    PyRestorePacket* pRestorePacket;
    PyObject* pRetVal;

    pRestorePacket = NativeToPyRestorePacket(rp);
    if (!pRestorePacket) { goto bail_out; }

    pRetVal = PyObject_CallFunctionObjArgs(pFunc, pRestorePacket, NULL);
    if (!pRetVal) {
      Py_DECREF(pRestorePacket);
      goto bail_out;
    } else {
      retval = ConvertPythonRetvalTobRCRetval(pRetVal);
      Py_DECREF(pRetVal);
      Py_DECREF(pRestorePacket);
    }
  } else {
    Dmsg(plugin_ctx, debuglevel,
         LOGPREFIX "Failed to find function named set_file_attributes()\n");
  }

  return retval;

bail_out:
  if (PyErr_Occurred()) { PyErrorHandler(plugin_ctx, M_FATAL); }

  return retval;
}

static bRC PyCheckFile(PluginContext* plugin_ctx, char* fname)
{
  bRC retval = bRC_Error;
  struct plugin_private_context* plugin_priv_ctx
      = (struct plugin_private_context*)plugin_ctx->plugin_private_context;
  PyObject* pFunc;

  if (!fname) { return bRC_Error; }

  // Lookup the check_file() function in the python module.
  pFunc = PyDict_GetItemString(plugin_priv_ctx->pyModuleFunctionsDict,
                               "check_file"); /* Borrowed reference */
  if (pFunc && PyCallable_Check(pFunc)) {
    PyObject *pFname, *pRetVal;

    pFname = PyUnicode_FromString(fname);
    pRetVal = PyObject_CallFunctionObjArgs(pFunc, pFname, NULL);
    Py_DECREF(pFname);

    if (!pRetVal) {
      goto bail_out;
    } else {
      retval = ConvertPythonRetvalTobRCRetval(pRetVal);
      Py_DECREF(pRetVal);
    }
  } else {
    Dmsg(plugin_ctx, debuglevel,
         LOGPREFIX "Failed to find function named check_file()\n");
  }

  return retval;

bail_out:
  if (PyErr_Occurred()) { PyErrorHandler(plugin_ctx, M_FATAL); }

  return retval;
}

static inline PyAclPacket* NativeToPyAclPacket(acl_pkt* ap)
{
  PyAclPacket* pAclPacket = PyObject_New(PyAclPacket, &PyAclPacketType);

  if (pAclPacket) {
    pAclPacket->fname = ap->fname;

    if (ap->content_length && ap->content) {
      pAclPacket->content
          = PyByteArray_FromStringAndSize(ap->content, ap->content_length);
    } else {
      pAclPacket->content = NULL;
    }
  }

  return pAclPacket;
}

static inline bool PyAclPacketToNative(PyAclPacket* pAclPacket, acl_pkt* ap)
{
  if (!pAclPacket->content) { return true; }

  if (PyByteArray_Check(pAclPacket->content)) {
    char* buf;

    ap->content_length = PyByteArray_Size(pAclPacket->content);
    if (ap->content_length <= 0
        || !(buf = PyByteArray_AsString(pAclPacket->content))) {
      return false;
    }

    if (ap->content) { free(ap->content); }
    ap->content = (char*)malloc(ap->content_length + 1);
    memcpy(ap->content, buf, ap->content_length + 1);
  } else {
    PyErr_SetString(PyExc_TypeError,
                    "acl packet content needs to be of bytearray type");
    return false;
  }

  return true;
}

static bRC PyGetAcl(PluginContext* plugin_ctx, acl_pkt* ap)
{
  bRC retval = bRC_Error;
  struct plugin_private_context* plugin_priv_ctx
      = (struct plugin_private_context*)plugin_ctx->plugin_private_context;
  PyObject* pFunc;

  if (!ap) { return bRC_Error; }

  // Lookup the get_acl() function in the python module.
  pFunc = PyDict_GetItemString(plugin_priv_ctx->pyModuleFunctionsDict,
                               "get_acl"); /* Borrowed reference */
  if (pFunc && PyCallable_Check(pFunc)) {
    PyAclPacket* pAclPkt;
    PyObject* pRetVal;

    pAclPkt = NativeToPyAclPacket(ap);
    if (!pAclPkt) { goto bail_out; }

    pRetVal = PyObject_CallFunctionObjArgs(pFunc, pAclPkt, NULL);
    if (!pRetVal) {
      Py_DECREF((PyObject*)pAclPkt);
      goto bail_out;
    } else {
      retval = ConvertPythonRetvalTobRCRetval(pRetVal);
      Py_DECREF(pRetVal);

      if (!PyAclPacketToNative(pAclPkt, ap)) {
        Py_DECREF((PyObject*)pAclPkt);
        goto bail_out;
      }
      Py_DECREF(pAclPkt);
    }
  } else {
    Dmsg(plugin_ctx, debuglevel,
         LOGPREFIX "Failed to find function named get_acl()\n");
  }

  return retval;

bail_out:
  if (PyErr_Occurred()) { PyErrorHandler(plugin_ctx, M_FATAL); }

  return retval;
}

static bRC PySetAcl(PluginContext* plugin_ctx, acl_pkt* ap)
{
  bRC retval = bRC_Error;
  struct plugin_private_context* plugin_priv_ctx
      = (struct plugin_private_context*)plugin_ctx->plugin_private_context;
  PyObject* pFunc;

  if (!ap) { return bRC_Error; }

  // Lookup the set_acl() function in the python module.
  pFunc = PyDict_GetItemString(plugin_priv_ctx->pyModuleFunctionsDict,
                               "set_acl"); /* Borrowed reference */
  if (pFunc && PyCallable_Check(pFunc)) {
    PyAclPacket* pAclPkt;
    PyObject* pRetVal;

    pAclPkt = NativeToPyAclPacket(ap);
    if (!pAclPkt) { goto bail_out; }

    pRetVal = PyObject_CallFunctionObjArgs(pFunc, pAclPkt, NULL);
    Py_DECREF(pAclPkt);

    if (!pRetVal) {
      goto bail_out;
    } else {
      retval = ConvertPythonRetvalTobRCRetval(pRetVal);
      Py_DECREF(pRetVal);
    }
  } else {
    Dmsg(plugin_ctx, debuglevel,
         LOGPREFIX "Failed to find function named set_acl()\n");
  }

  return retval;

bail_out:
  if (PyErr_Occurred()) { PyErrorHandler(plugin_ctx, M_FATAL); }

  return retval;
}

static inline PyXattrPacket* NativeToPyXattrPacket(xattr_pkt* xp)
{
  PyXattrPacket* pXattrPacket = PyObject_New(PyXattrPacket, &PyXattrPacketType);

  if (pXattrPacket) {
    pXattrPacket->fname = xp->fname;

    if (xp->name_length && xp->name) {
      pXattrPacket->name
          = PyByteArray_FromStringAndSize(xp->name, xp->name_length);
    } else {
      pXattrPacket->name = NULL;
    }
    if (xp->value_length && xp->value) {
      pXattrPacket->value
          = PyByteArray_FromStringAndSize(xp->value, xp->value_length);
    } else {
      pXattrPacket->value = NULL;
    }
  }

  return pXattrPacket;
}

static inline bool PyXattrPacketToNative(PyXattrPacket* pXattrPacket,
                                         xattr_pkt* xp)
{
  if (!pXattrPacket->name) { return true; }

  if (PyByteArray_Check(pXattrPacket->name)) {
    char* buf;

    xp->name_length = PyByteArray_Size(pXattrPacket->name);
    if (xp->name_length <= 0
        || !(buf = PyByteArray_AsString(pXattrPacket->name))) {
      return false;
    }

    if (xp->name) { free(xp->name); }
    xp->name = (char*)malloc(xp->name_length);
    memcpy(xp->name, buf, xp->name_length);
  }

  if (pXattrPacket->value && PyByteArray_Check(pXattrPacket->value)) {
    char* buf;

    xp->value_length = PyByteArray_Size(pXattrPacket->value);
    if (xp->value_length <= 0
        || !(buf = PyByteArray_AsString(pXattrPacket->value))) {
      return false;
    }

    if (xp->value) { free(xp->value); }
    xp->value = (char*)malloc(xp->value_length);
    memcpy(xp->value, buf, xp->value_length);
  } else {
    if (xp->value) { free(xp->value); }
    xp->value = NULL;
  }

  return true;
}

static bRC PyGetXattr(PluginContext* plugin_ctx, xattr_pkt* xp)
{
  bRC retval = bRC_Error;
  struct plugin_private_context* plugin_priv_ctx
      = (struct plugin_private_context*)plugin_ctx->plugin_private_context;
  PyObject* pFunc;

  if (!xp) { return bRC_Error; }

  // Lookup the get_xattr() function in the python module.
  pFunc = PyDict_GetItemString(plugin_priv_ctx->pyModuleFunctionsDict,
                               "get_xattr"); /* Borrowed reference */
  if (pFunc && PyCallable_Check(pFunc)) {
    PyXattrPacket* pXattrPkt;
    PyObject* pRetVal;

    pXattrPkt = NativeToPyXattrPacket(xp);
    if (!pXattrPkt) { goto bail_out; }

    pRetVal = PyObject_CallFunctionObjArgs(pFunc, pXattrPkt, NULL);
    if (!pRetVal) {
      Py_DECREF((PyObject*)pXattrPkt);
      goto bail_out;
    } else {
      retval = ConvertPythonRetvalTobRCRetval(pRetVal);
      Py_DECREF(pRetVal);

      if (!PyXattrPacketToNative(pXattrPkt, xp)) {
        Py_DECREF((PyObject*)pXattrPkt);
        goto bail_out;
      }
      Py_DECREF(pXattrPkt);
    }
  } else {
    Dmsg(plugin_ctx, debuglevel,
         LOGPREFIX "Failed to find function named get_xattr()\n");
  }

  return retval;

bail_out:
  if (PyErr_Occurred()) { PyErrorHandler(plugin_ctx, M_FATAL); }

  return retval;
}

static bRC PySetXattr(PluginContext* plugin_ctx, xattr_pkt* xp)
{
  bRC retval = bRC_Error;
  struct plugin_private_context* plugin_priv_ctx
      = (struct plugin_private_context*)plugin_ctx->plugin_private_context;
  PyObject* pFunc;

  if (!xp) { return bRC_Error; }

  // Lookup the set_acl() function in the python module.
  pFunc = PyDict_GetItemString(plugin_priv_ctx->pyModuleFunctionsDict,
                               "set_xattr"); /* Borrowed reference */
  if (pFunc && PyCallable_Check(pFunc)) {
    PyXattrPacket* pXattrPkt;
    PyObject* pRetVal;

    pXattrPkt = NativeToPyXattrPacket(xp);
    if (!pXattrPkt) { goto bail_out; }

    pRetVal = PyObject_CallFunctionObjArgs(pFunc, pXattrPkt, NULL);
    Py_DECREF(pXattrPkt);

    if (!pRetVal) {
      goto bail_out;
    } else {
      retval = ConvertPythonRetvalTobRCRetval(pRetVal);
      Py_DECREF(pRetVal);
    }
  } else {
    Dmsg(plugin_ctx, debuglevel,
         LOGPREFIX "Failed to find function named set_xattr()\n");
  }

  return retval;
bail_out:
  if (PyErr_Occurred()) { PyErrorHandler(plugin_ctx, M_FATAL); }

  return retval;
}

static inline PyRestoreObject* NativeToPyRestoreObject(restore_object_pkt* rop)
{
  PyRestoreObject* pRestoreObject
      = PyObject_New(PyRestoreObject, &PyRestoreObjectType);

  if (pRestoreObject) {
    pRestoreObject->object_name = PyUnicode_FromString(rop->object_name);
    pRestoreObject->object
        = PyByteArray_FromStringAndSize(rop->object, rop->object_len);
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

static bRC PyRestoreObjectData(PluginContext* plugin_ctx,
                               restore_object_pkt* rop)
{
  bRC retval = bRC_Error;
  struct plugin_private_context* plugin_priv_ctx
      = (struct plugin_private_context*)plugin_ctx->plugin_private_context;
  PyObject* pFunc;

  if (!rop) { return bRC_OK; }

  // Lookup the restore_object_data() function in the python module.
  pFunc = PyDict_GetItemString(plugin_priv_ctx->pyModuleFunctionsDict,
                               "restore_object_data"); /* Borrowed reference */
  if (pFunc && PyCallable_Check(pFunc)) {
    PyRestoreObject* pRestoreObject;
    PyObject* pRetVal;

    pRestoreObject = NativeToPyRestoreObject(rop);
    if (!pRestoreObject) { goto bail_out; }

    pRetVal = PyObject_CallFunctionObjArgs(pFunc, pRestoreObject, NULL);
    Py_DECREF(pRestoreObject);

    if (!pRetVal) {
      goto bail_out;
    } else {
      retval = ConvertPythonRetvalTobRCRetval(pRetVal);
      Py_DECREF(pRetVal);
    }
  } else {
    Dmsg(plugin_ctx, debuglevel,
         LOGPREFIX "Failed to find function named start_restore_file()\n");
  }

  return retval;

bail_out:
  if (PyErr_Occurred()) { PyErrorHandler(plugin_ctx, M_FATAL); }

  return retval;
}

static bRC PyHandleBackupFile(PluginContext* plugin_ctx, save_pkt* sp)
{
  bRC retval = bRC_Error;
  struct plugin_private_context* plugin_priv_ctx
      = (struct plugin_private_context*)plugin_ctx->plugin_private_context;
  PyObject* pFunc;

  if (!sp) { return bRC_Error; }

  // Lookup the handle_backup_file() function in the python module.
  pFunc = PyDict_GetItemString(plugin_priv_ctx->pyModuleFunctionsDict,
                               "handle_backup_file"); /* Borrowed reference */
  if (pFunc && PyCallable_Check(pFunc)) {
    PySavePacket* pSavePkt;
    PyObject* pRetVal;

    pSavePkt = NativeToPySavePacket(sp);
    if (!pSavePkt) { goto bail_out; }

    pRetVal = PyObject_CallFunctionObjArgs(pFunc, pSavePkt, NULL);
    if (!pRetVal) {
      Py_DECREF((PyObject*)pSavePkt);
      goto bail_out;
    } else {
      retval = ConvertPythonRetvalTobRCRetval(pRetVal);
      Py_DECREF(pRetVal);

      if (!PySavePacketToNative(pSavePkt, sp, plugin_priv_ctx, true)) {
        Py_DECREF((PyObject*)pSavePkt);
        goto bail_out;
      }
      Py_DECREF(pSavePkt);
    }
  } else {
    Dmsg(plugin_ctx, debuglevel,
         LOGPREFIX "Failed to find function named handle_backup_file()\n");
  }

  return retval;

bail_out:
  if (PyErr_Occurred()) { PyErrorHandler(plugin_ctx, M_FATAL); }

  return retval;
}

/**
 * Callback function which is exposed as a part of the additional methods
 * which allow a Python plugin to get certain internal values of the current
 * Job.
 */
static PyObject* PyBareosGetValue(PyObject*, PyObject* args)
{
  bVariable var;
  PluginContext* plugin_ctx = plugin_context;
  PyObject* pRetVal = NULL;

  if (!PyArg_ParseTuple(args, "i:BareosGetValue", &var)) { return NULL; }
  RETURN_RUNTIME_ERROR_IF_BFUNC_OR_BAREOS_PLUGIN_CTX_UNSET()

  switch (var) {
    case bVarFDName:
    case bVarWorkingDir:
    case bVarUsedConfig:
    case bVarExePath:
    case bVarVersion:
    case bVarDistName: {
      char* value = NULL;

      if (bareos_core_functions->getBareosValue(plugin_ctx, var, &value)
          == bRC_OK) {
        if (value) { pRetVal = PyUnicode_FromString(value); }
      }
      break;
    }
    case bVarJobId:
    case bVarLevel:
    case bVarType:
    case bVarJobStatus:
    case bVarAccurate:
    case bVarPrefixLinks: {
      int value = 0;

      if (bareos_core_functions->getBareosValue(plugin_ctx, var, &value)
          == bRC_OK) {
        pRetVal = PyLong_FromLong(value);
      }
      break;
    }
    case bVarClient:
    case bVarJobName:
    case bVarPrevJobName:
    case bVarWhere:
    case bVarRegexWhere: {
      char* value = NULL;

      if (bareos_core_functions->getBareosValue(plugin_ctx, var, &value)
          == bRC_OK) {
        if (value) { pRetVal = PyUnicode_FromString(value); }
      }
      break;
    }
    case bVarSinceTime: {
      pRetVal = PyLong_FromLong(static_cast<plugin_private_context*>(
                                    plugin_ctx->plugin_private_context)
                                    ->since);
      break;
    }
    case bVarCheckChanges: {
      bool value{false};
      if (bareos_core_functions->getBareosValue(plugin_ctx, var, &value)
          == bRC_OK) {
        pRetVal = value ? Py_True : Py_False;
      }
      break;
    }
    case bVarFileSeen:
      break; /* a write only variable, ignore read request */
    default:
      Dmsg(plugin_ctx, debuglevel,
           LOGPREFIX "PyBareosGetValue unknown variable requested %d\n", var);
      break;
  }

  if (!pRetVal) { Py_RETURN_NONE; }

  return pRetVal;
}

/**
 * Callback function which is exposed as a part of the additional methods
 * which allow a Python plugin to get certain internal values of the current
 * Job.
 */
static PyObject* PyBareosSetValue(PyObject*, PyObject* args)
{
  bVariable var;
  PluginContext* plugin_ctx = plugin_context;
  bRC retval = bRC_Error;
  PyObject* pyValue;

  if (!PyArg_ParseTuple(args, "iO:BareosSetValue", &var, &pyValue)) {
    goto bail_out;
  }
  RETURN_RUNTIME_ERROR_IF_BFUNC_OR_BAREOS_PLUGIN_CTX_UNSET()

  switch (var) {
    case bVarSinceTime: {
      static_cast<plugin_private_context*>(plugin_ctx->plugin_private_context)
          ->since
          = PyLong_AsLong(pyValue);
      retval = bRC_OK;
      break;
    }
    case bVarCheckChanges: {
      bool value = (pyValue == Py_True);
      retval = bareos_core_functions->setBareosValue(plugin_ctx, var, &value);
      break;
    }
    case bVarFileSeen: {
      const char* value = PyUnicode_AsUTF8(pyValue);
      if (value) {
        retval = bareos_core_functions->setBareosValue(plugin_ctx, var, value);
      }
      break;
    }
    default:
      Dmsg(plugin_ctx, debuglevel,
           LOGPREFIX "PyBareosSetValue unknown variable requested %d\n", var);
      break;
  }

bail_out:
  return ConvertbRCRetvalToPythonRetval(retval);
}

/**
 * Callback function which is exposed as a part of the additional methods
 * which allow a Python plugin to issue debug messages using the Bareos debug
 * message facility.
 */
static PyObject* PyBareosDebugMessage(PyObject*, PyObject* args)
{
  int level;
  char* dbgmsg = NULL;
  PluginContext* plugin_ctx = plugin_context;

  if (!PyArg_ParseTuple(args, "i|z:BareosDebugMessage", &level, &dbgmsg)) {
    return NULL;
  }
  RETURN_RUNTIME_ERROR_IF_BAREOS_PLUGIN_CTX_UNSET()

  if (dbgmsg) { Dmsg(plugin_ctx, level, LOGPREFIX "%s", dbgmsg); }

  Py_RETURN_NONE;
}

/**
 * Callback function which is exposed as a part of the additional methods
 * which allow a Python plugin to issue Job messages using the Bareos Job
 * message facility.
 */
static PyObject* PyBareosJobMessage(PyObject*, PyObject* args)
{
  int level;
  char* jobmsg = NULL;
  PluginContext* plugin_ctx = plugin_context;

  if (!PyArg_ParseTuple(args, "i|z:BareosJobMessage", &level, &jobmsg)) {
    return NULL;
  }
  RETURN_RUNTIME_ERROR_IF_BFUNC_OR_BAREOS_PLUGIN_CTX_UNSET()

  if (jobmsg) { Jmsg(plugin_ctx, level, LOGPREFIX "%s", jobmsg); }

  Py_RETURN_NONE;
}

/**
 * Callback function which is exposed as a part of the additional methods
 * which allow a Python plugin to issue a Register Event to register
 * additional events it wants to receive.
 */
static PyObject* PyBareosRegisterEvents(PyObject*, PyObject* args)
{
  int len, event;
  PluginContext* plugin_ctx = plugin_context;
  bRC retval = bRC_Error;
  PyObject *pyEvents, *pySeq, *pyEvent;

  if (!PyArg_ParseTuple(args, "O:BareosRegisterEvents", &pyEvents)) {
    goto bail_out;
  }
  RETURN_RUNTIME_ERROR_IF_BFUNC_OR_BAREOS_PLUGIN_CTX_UNSET()

  pySeq = PySequence_Fast(pyEvents, "Expected a sequence of events");
  if (!pySeq) { goto bail_out; }
  len = PySequence_Fast_GET_SIZE(pySeq);

  for (int i = 0; i < len; i++) {
    pyEvent = PySequence_Fast_GET_ITEM(pySeq, i);
    event = PyLong_AsLong(pyEvent);

    if (event >= bEventJobStart && event <= FD_NR_EVENTS) {
      Dmsg(plugin_ctx, debuglevel,
           LOGPREFIX "PyBareosRegisterEvents registering event %d\n", event);
      retval
          = bareos_core_functions->registerBareosEvents(plugin_ctx, 1, event);

      if (retval != bRC_OK) { break; }
    }
  }

  Py_DECREF(pySeq);

bail_out:
  return ConvertbRCRetvalToPythonRetval(retval);
}

/**
 * Callback function which is exposed as a part of the additional methods
 * which allow a Python plugin to issue an Unregister Event to unregister
 * events it doesn't want to receive anymore.
 */
static PyObject* PyBareosUnRegisterEvents(PyObject*, PyObject* args)
{
  int len, event;
  PluginContext* plugin_ctx = plugin_context;
  bRC retval = bRC_Error;
  PyObject *pyEvents, *pySeq, *pyEvent;

  if (!PyArg_ParseTuple(args, "O:BareosUnRegisterEvents", &pyEvents)) {
    goto bail_out;
  }
  RETURN_RUNTIME_ERROR_IF_BFUNC_OR_BAREOS_PLUGIN_CTX_UNSET()

  pySeq = PySequence_Fast(pyEvents, "Expected a sequence of events");
  if (!pySeq) { goto bail_out; }

  len = PySequence_Fast_GET_SIZE(pySeq);
  for (int i = 0; i < len; i++) {
    pyEvent = PySequence_Fast_GET_ITEM(pySeq, i);
    event = PyLong_AsLong(pyEvent);

    if (event >= bEventJobStart && event <= FD_NR_EVENTS) {
      Dmsg(plugin_ctx, debuglevel,
           "PyBareosUnRegisterEvents: unregistering event %d\n", event);
      retval
          = bareos_core_functions->unregisterBareosEvents(plugin_ctx, 1, event);
    }
  }

  Py_DECREF(pySeq);

bail_out:
  return ConvertbRCRetvalToPythonRetval(retval);
}

/**
 * Callback function which is exposed as a part of the additional methods
 * which allow a Python plugin to issue a GetInstanceCount to retrieve the
 * number of instances of the current plugin being loaded into the daemon.
 */
static PyObject* PyBareosGetInstanceCount(PyObject*, PyObject* args)
{
  int value;
  PluginContext* plugin_ctx = plugin_context;
  PyObject* pRetVal = NULL;

  if (!PyArg_ParseTuple(args, ":BareosGetInstanceCount")) { return NULL; }
  RETURN_RUNTIME_ERROR_IF_BFUNC_OR_BAREOS_PLUGIN_CTX_UNSET()

  if (bareos_core_functions->getInstanceCount(plugin_ctx, &value) == bRC_OK) {
    pRetVal = PyLong_FromLong(value);
  }

  if (!pRetVal) { Py_RETURN_NONE; }

  return pRetVal;
}

/**
 * Callback function which is exposed as a part of the additional methods
 * which allow a Python plugin to issue a Add Exclude pattern to the fileset.
 */
static PyObject* PyBareosAddExclude(PyObject*, PyObject* args)
{
  char* file = NULL;
  bRC retval = bRC_Error;
  PluginContext* plugin_ctx = plugin_context;

  if (!PyArg_ParseTuple(args, "|z:BareosAddExclude", &file)) { goto bail_out; }
  RETURN_RUNTIME_ERROR_IF_BFUNC_OR_BAREOS_PLUGIN_CTX_UNSET()

  if (file) { retval = bareos_core_functions->AddExclude(plugin_ctx, file); }

bail_out:
  return ConvertbRCRetvalToPythonRetval(retval);
}

/**
 * Callback function which is exposed as a part of the additional methods
 * which allow a Python plugin to issue a Add Include pattern to the fileset.
 */
static PyObject* PyBareosAddInclude(PyObject*, PyObject* args)
{
  char* file = NULL;
  bRC retval = bRC_Error;
  PluginContext* plugin_ctx = plugin_context;

  if (!PyArg_ParseTuple(args, "|z:BareosAddInclude", &file)) { goto bail_out; }
  RETURN_RUNTIME_ERROR_IF_BFUNC_OR_BAREOS_PLUGIN_CTX_UNSET()

  if (file) { retval = bareos_core_functions->AddInclude(plugin_ctx, file); }

bail_out:
  return ConvertbRCRetvalToPythonRetval(retval);
}

/**
 * Callback function which is exposed as a part of the additional methods
 * which allow a Python plugin to issue a Add Include Options to the fileset.
 */
static PyObject* PyBareosAddOptions(PyObject*, PyObject* args)
{
  char* opts = NULL;
  bRC retval = bRC_Error;
  PluginContext* plugin_ctx = plugin_context;

  if (!PyArg_ParseTuple(args, "|z:BareosAddOptions", &opts)) { goto bail_out; }
  RETURN_RUNTIME_ERROR_IF_BFUNC_OR_BAREOS_PLUGIN_CTX_UNSET()

  if (opts) { retval = bareos_core_functions->AddOptions(plugin_ctx, opts); }

bail_out:
  return ConvertbRCRetvalToPythonRetval(retval);
}

/**
 * Callback function which is exposed as a part of the additional methods
 * which allow a Python plugin to issue a Add Regex to the fileset.
 */
static PyObject* PyBareosAddRegex(PyObject*, PyObject* args)
{
  int type;
  char* item = NULL;
  bRC retval = bRC_Error;
  PluginContext* plugin_ctx = plugin_context;
  if (!PyArg_ParseTuple(args, "|zi:BareosAddRegex", &item, &type)) {
    goto bail_out;
  }
  RETURN_RUNTIME_ERROR_IF_BFUNC_OR_BAREOS_PLUGIN_CTX_UNSET()

  if (item) {
    retval = bareos_core_functions->AddRegex(plugin_ctx, item, type);
  }

bail_out:
  return ConvertbRCRetvalToPythonRetval(retval);
}

/**
 * Callback function which is exposed as a part of the additional methods
 * which allow a Python plugin to issue a Add Wildcard to the fileset.
 */
static PyObject* PyBareosAddWild(PyObject*, PyObject* args)
{
  int type;
  char* item = NULL;
  bRC retval = bRC_Error;
  PluginContext* plugin_ctx = plugin_context;

  if (!PyArg_ParseTuple(args, "|zi:BareosAddWild", &item, &type)) {
    goto bail_out;
  }
  RETURN_RUNTIME_ERROR_IF_BFUNC_OR_BAREOS_PLUGIN_CTX_UNSET()

  if (item) { retval = bareos_core_functions->AddWild(plugin_ctx, item, type); }

bail_out:
  return ConvertbRCRetvalToPythonRetval(retval);
}

/**
 * Callback function which is exposed as a part of the additional methods
 * which allow a Python plugin to issue a Add New Option block.
 */
static PyObject* PyBareosNewOptions(PyObject*, PyObject* args)
{
  PluginContext* plugin_ctx = plugin_context;
  bRC retval = bRC_Error;

  if (!PyArg_ParseTuple(args, ":BareosNewOptions")) { goto bail_out; }
  RETURN_RUNTIME_ERROR_IF_BFUNC_OR_BAREOS_PLUGIN_CTX_UNSET()

  retval = bareos_core_functions->NewOptions(plugin_ctx);

bail_out:
  return ConvertbRCRetvalToPythonRetval(retval);
}

/**
 * Callback function which is exposed as a part of the additional methods
 * which allow a Python plugin to issue a Add New Include block.
 */
static PyObject* PyBareosNewInclude(PyObject*, PyObject* args)
{
  PluginContext* plugin_ctx = plugin_context;
  bRC retval = bRC_Error;

  if (!PyArg_ParseTuple(args, ":BareosNewInclude")) { goto bail_out; }
  RETURN_RUNTIME_ERROR_IF_BFUNC_OR_BAREOS_PLUGIN_CTX_UNSET()

  retval = bareos_core_functions->NewInclude(plugin_ctx);

bail_out:
  return ConvertbRCRetvalToPythonRetval(retval);
}

/**
 * Callback function which is exposed as a part of the additional methods
 * which allow a Python plugin to issue a Add New Pre Include block.
 */
static PyObject* PyBareosNewPreInclude(PyObject*, PyObject* args)
{
  PluginContext* plugin_ctx = plugin_context;
  bRC retval = bRC_Error;

  if (!PyArg_ParseTuple(args, ":BareosNewPreInclude")) { goto bail_out; }
  RETURN_RUNTIME_ERROR_IF_BFUNC_OR_BAREOS_PLUGIN_CTX_UNSET()

  retval = bareos_core_functions->NewPreInclude(plugin_ctx);

bail_out:
  return ConvertbRCRetvalToPythonRetval(retval);
}

/**
 * Callback function which is exposed as a part of the additional methods
 * which allow a Python plugin to issue a check if a file have to be backed up
 * using Accurate code.
 */
static PyObject* PyBareosCheckChanges(PyObject*, PyObject* args)
{
  PluginContext* plugin_ctx = plugin_context;

  save_pkt sp;
  bRC retval = bRC_Error;
  PySavePacket* pSavePkt;

  if (!PyArg_ParseTuple(args, "O:BareosCheckChanges", &pSavePkt)) {
    goto bail_out;
  }
  RETURN_RUNTIME_ERROR_IF_BFUNC_OR_BAREOS_PLUGIN_CTX_UNSET()


  /* CheckFile only has a need for a limited version of the PySavePacket so we
   * handle that here separately and don't call PySavePacketToNative(). */
  sp.type = pSavePkt->type;
  if (pSavePkt->fname) {
    if (PyUnicode_Check(pSavePkt->fname)) {
      sp.fname = const_cast<char*>(PyUnicode_AsUTF8(pSavePkt->fname));
    } else {
      goto bail_out;
    }
  } else {
    goto bail_out;
  }
  if (pSavePkt->link) {
    if (PyUnicode_Check(pSavePkt->link)) {
      sp.link = const_cast<char*>(PyUnicode_AsUTF8(pSavePkt->link));
    } else {
      goto bail_out;
    }
  }
  sp.save_time = pSavePkt->save_time;

  /* TODO: this is incorrect!  We need to set a lot more!
   * AccurateCheckChanges uses the stat block to determine the file type! */
  retval = bareos_core_functions->checkChanges(plugin_ctx, &sp);

  // Copy back the two fields that are changed by checkChanges().
  pSavePkt->delta_seq = sp.delta_seq;
  pSavePkt->accurate_found = sp.accurate_found;

bail_out:
  return ConvertbRCRetvalToPythonRetval(retval);
}

/**
 * Callback function which is exposed as a part of the additional methods
 * which allow a Python plugin to issue a check if a file would be saved using
 * current Include/Exclude code.
 */
static PyObject* PyBareosAcceptFile(PyObject*, PyObject* args)
{
  PluginContext* plugin_ctx = plugin_context;
  save_pkt sp;
  bRC retval = bRC_Error;
  PySavePacket* pSavePkt;

  if (!PyArg_ParseTuple(args, "O:BareosAcceptFile", &pSavePkt)) {
    goto bail_out;
  }
  RETURN_RUNTIME_ERROR_IF_BFUNC_OR_BAREOS_PLUGIN_CTX_UNSET()

  /* Acceptfile only needs fname and statp from PySavePacket so we handle
   * that here separately and don't call PySavePacketToNative(). */
  if (pSavePkt->fname) {
    if (PyUnicode_Check(pSavePkt->fname)) {
      sp.fname = const_cast<char*>(PyUnicode_AsUTF8(pSavePkt->fname));
    } else {
      goto bail_out;
    }
  } else {
    goto bail_out;
  }

  if (pSavePkt->statp) {
    PyStatPacketToNative((PyStatPacket*)pSavePkt->statp, &sp.statp);
  } else {
    goto bail_out;
  }

  retval = bareos_core_functions->AcceptFile(plugin_ctx, &sp);

bail_out:
  return ConvertbRCRetvalToPythonRetval(retval);
}

/**
 * Callback function which is exposed as a part of the additional methods
 * which allow a Python plugin to issue a Set bit in the Accurate Seen bitmap.
 */
static PyObject* PyBareosSetSeenBitmap(PyObject*, PyObject* args)
{
  bool all;
  PluginContext* plugin_ctx = plugin_context;
  char* fname = NULL;
  bRC retval = bRC_Error;
  PyObject* pyBool;

  if (!PyArg_ParseTuple(args, "O|s:BareosSetSeenBitmap", &pyBool, &fname)) {
    goto bail_out;
  }
  RETURN_RUNTIME_ERROR_IF_BFUNC_OR_BAREOS_PLUGIN_CTX_UNSET()

  all = PyObject_IsTrue(pyBool);
  retval = bareos_core_functions->SetSeenBitmap(plugin_ctx, all, fname);

bail_out:
  return ConvertbRCRetvalToPythonRetval(retval);
}

/**
 * Callback function which is exposed as a part of the additional methods
 * which allow a Python plugin to issue a Clear bit in the Accurate Seen
 * bitmap.
 */
static PyObject* PyBareosClearSeenBitmap(PyObject*, PyObject* args)
{
  bool all;
  PluginContext* plugin_ctx = plugin_context;
  char* fname = NULL;
  bRC retval = bRC_Error;
  PyObject* pyBool;

  if (!PyArg_ParseTuple(args, "O|s:BareosClearSeenBitmap", &pyBool, &fname)) {
    goto bail_out;
  }
  RETURN_RUNTIME_ERROR_IF_BFUNC_OR_BAREOS_PLUGIN_CTX_UNSET()

  all = PyObject_IsTrue(pyBool);
  retval = bareos_core_functions->ClearSeenBitmap(plugin_ctx, all, fname);

bail_out:
  return ConvertbRCRetvalToPythonRetval(retval);
}

// Some helper functions.
static inline char* PyGetStringValue(PyObject* object)
{
  if (!object || !PyUnicode_Check(object)) { return (char*)""; }

  return const_cast<char*>(PyUnicode_AsUTF8(object));
}

static inline char* PyGetByteArrayValue(PyObject* object)
{
  if (!object || !PyByteArray_Check(object)) { return (char*)""; }

  return PyByteArray_AsString(object);
}

// Python specific handlers for PyRestoreObject structure mapping.

// Representation.
static PyObject* PyRestoreObject_repr(PyRestoreObject* self)
{
  PyObject* s;
  PoolMem buf(PM_MESSAGE);

  Mmsg(buf,
       "RestoreObject(object_name=\"%s\", object=\"%s\", plugin_name=\"%s\", "
       "object_type=%d, object_len=%d, object_full_len=%d, "
       "object_index=%d, object_compression=%d, stream=%d, jobid=%u)",
       PyGetStringValue(self->object_name), PyGetByteArrayValue(self->object),
       self->plugin_name, self->object_type, self->object_len,
       self->object_full_len, self->object_index, self->object_compression,
       self->stream, self->JobId);
  s = PyUnicode_FromString(buf.c_str());

  return s;
}

// Initialization.
static int PyRestoreObject_init(PyRestoreObject* self,
                                PyObject* args,
                                PyObject* kwds)
{
  static char* kwlist[] = {(char*)"object_name",
                           (char*)"object",
                           (char*)"plugin_name",
                           (char*)"object_type",
                           (char*)"object_len",
                           (char*)"object_full_len",
                           (char*)"object_index",
                           (char*)"object_compression",
                           (char*)"stream",
                           (char*)"jobid",
                           NULL};

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

  if (!PyArg_ParseTupleAndKeywords(
          args, kwds, "|oosiiiiiiI", kwlist, &self->object_name, &self->object,
          &self->plugin_name, &self->object_type, &self->object_len,
          &self->object_full_len, &self->object_index,
          &self->object_compression, &self->stream, &self->JobId)) {
    return -1;
  }

  return 0;
}

// Destructor.
static void PyRestoreObject_dealloc(PyRestoreObject* self)
{
  if (self->object_name) { Py_XDECREF(self->object_name); }
  if (self->object) { Py_XDECREF(self->object); }
  PyObject_Del(self);
}

// Python specific handlers for PyStatPacket structure mapping.

// Representation.
static PyObject* PyStatPacket_repr(PyStatPacket* self)
{
  PyObject* s;
  PoolMem buf(PM_MESSAGE);

  Mmsg(buf,
       "StatPacket(dev=%ld, ino=%lld, mode=%04o, nlink=%d, "
       "uid=%ld, gid=%ld, rdev=%ld, size=%lld, "
       "atime=%ld, mtime=%ld, ctime=%ld, blksize=%ld, blocks=%lld)",
       self->dev, self->ino, (self->mode & ~S_IFMT), self->nlink, self->uid,
       self->gid, self->rdev, self->size, self->atime, self->mtime, self->ctime,
       self->blksize, self->blocks);

  s = PyUnicode_FromString(buf.c_str());

  return s;
}

// Initialization.
static int PyStatPacket_init(PyStatPacket* self, PyObject* args, PyObject* kwds)
{
  time_t now;
  static char* kwlist[] = {(char*)"dev",    (char*)"ino",
                           (char*)"mode",   (char*)"nlink",
                           (char*)"uid",    (char*)"gid",
                           (char*)"rdev",   (char*)"size",
                           (char*)"atime",  (char*)"mtime",
                           (char*)"ctime",  (char*)"blksize",
                           (char*)"blocks", NULL};

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

  if (!PyArg_ParseTupleAndKeywords(
          args, kwds, "|IKHHIIILIIIIK", kwlist, &self->dev, &self->ino,
          &self->mode, &self->nlink, &self->uid, &self->gid, &self->rdev,
          &self->size, &self->atime, &self->mtime, &self->ctime, &self->blksize,
          &self->blocks)) {
    return -1;
  }

  return 0;
}

// Destructor.
static void PyStatPacket_dealloc(PyStatPacket* self) { PyObject_Del(self); }

// Python specific handlers for PySavePacket structure mapping.

// Representation.
static inline const char* print_flags_bitmap(PyObject* bitmap)
{
  static char visual_bitmap[FO_MAX + 1];
  if (!bitmap) { return "<NULL>"; }
  if (PyByteArray_Check(bitmap)) {
    int cnt;
    char* flags;

    if (PyByteArray_Size(bitmap) == FOPTS_BYTES) {
      if ((flags = PyByteArray_AsString(bitmap))) {
        memset(visual_bitmap, 0, sizeof(visual_bitmap));
        for (cnt = 0; cnt <= FO_MAX; cnt++) {
          if (BitIsSet(cnt, flags)) {
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

static PyObject* PySavePacket_repr(PySavePacket* self)
{
  PyObject* s;
  PoolMem buf(PM_MESSAGE);

  Mmsg(buf,
       "SavePacket(fname=\"%s\", link=\"%s\", type=%ld, flags=%s, "
       "no_read=%d, portable=%d, accurate_found=%d, "
       "cmd=\"%s\", save_time=%ld, delta_seq=%ld, object_name=\"%s\", "
       "object=\"%s\", object_len=%ld, object_index=%ld)",
       PyGetStringValue(self->fname), PyGetStringValue(self->link), self->type,
       print_flags_bitmap(self->flags), self->no_read, self->portable,
       self->accurate_found, self->cmd, self->save_time, self->delta_seq,
       PyGetStringValue(self->object_name), PyGetByteArrayValue(self->object),
       self->object_len, self->object_index);

  s = PyUnicode_FromString(buf.c_str());

  return s;
}

// Initialization.
static int PySavePacket_init(PySavePacket* self, PyObject* args, PyObject* kwds)
{
  static char* kwlist[]
      = {(char*)"fname",          (char*)"link",         (char*)"type",
         (char*)"flags",          (char*)"no_read",      (char*)"portable",
         (char*)"accurate_found", (char*)"cmd",          (char*)"save_time",
         (char*)"delta_seq",      (char*)"object_name",  (char*)"object",
         (char*)"object_len",     (char*)"object_index", NULL};
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

  if (!PyArg_ParseTupleAndKeywords(
          args, kwds, "|OOiOpppsiiOOii", kwlist, &self->fname, &self->link,
          &self->type, &self->flags, &self->no_read, &self->portable,
          &self->accurate_found, &self->cmd, &self->save_time, &self->delta_seq,
          &self->object_name, &self->object, &self->object_len,
          &self->object_index)) {
    return -1;
  }
  return 0;
}

// Destructor.
static void PySavePacket_dealloc(PySavePacket* self)
{
  if (self->fname) { Py_XDECREF(self->fname); }
  if (self->link) { Py_XDECREF(self->link); }
  if (self->flags) { Py_XDECREF(self->flags); }
  if (self->object_name) { Py_XDECREF(self->object_name); }
  if (self->object) { Py_XDECREF(self->object); }
  if (self->statp) { Py_XDECREF(self->statp); }
  PyObject_Del(self);
}

// Python specific handlers for PyRestorePacket structure mapping.

// Representation.
static PyObject* PyRestorePacket_repr(PyRestorePacket* self)
{
  PyObject *stat_repr, *s;
  PoolMem buf(PM_MESSAGE);

  stat_repr = PyObject_Repr(self->statp);
  Mmsg(buf,
       "RestorePacket(stream=%d, data_stream=%ld, type=%ld, file_index=%ld, "
       "linkFI=%ld, uid=%ld, statp=\"%s\", attrEx=\"%s\", ofname=\"%s\", "
       "olname=\"%s\", where=\"%s\", RegexWhere=\"%s\", replace=%d, "
       "create_status=%d)",
       self->stream, self->data_stream, self->type, self->file_index,
       self->LinkFI, self->uid, PyGetStringValue(stat_repr), self->attrEx,
       self->ofname, self->olname, self->where, self->RegexWhere, self->replace,
       self->create_status);

  s = PyUnicode_FromString(buf.c_str());
  Py_DECREF(stat_repr);

  return s;
}

// Initialization.
static int PyRestorePacket_init(PyRestorePacket* self,
                                PyObject* args,
                                PyObject* kwds)
{
  static char* kwlist[]
      = {(char*)"stream",     (char*)"data_stream",   (char*)"type",
         (char*)"file_index", (char*)"linkFI",        (char*)"uid",
         (char*)"statp",      (char*)"attrEX",        (char*)"ofname",
         (char*)"olname",     (char*)"where",         (char*)"regexwhere",
         (char*)"replace",    (char*)"create_status", NULL};

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

  if (!PyArg_ParseTupleAndKeywords(
          args, kwds, "|iiiiiIosssssii", kwlist, &self->stream,
          &self->data_stream, &self->type, &self->file_index, &self->LinkFI,
          &self->uid, &self->statp, &self->attrEx, &self->ofname, &self->olname,
          &self->where, &self->RegexWhere, &self->replace,
          &self->create_status)) {
    return -1;
  }

  return 0;
}

// Destructor.
static void PyRestorePacket_dealloc(PyRestorePacket* self)
{
  PyObject_Del(self);
}

// Python specific handlers for PyIoPacket structure mapping.

// Representation.
static PyObject* PyIoPacket_repr(PyIoPacket* self)
{
  PyObject* s;
  PoolMem buf(PM_MESSAGE);

  Mmsg(buf,
       "IoPacket(func=%d, count=%ld, flags=%ld, mode=%04o, "
       "buf=\"%s\", fname=\"%s\", status=%ld, io_errno=%ld, lerror=%ld, "
       "whence=%ld, offset=%lld, win32=%d, filedes=%d)",
       self->func, self->count, self->flags, (self->mode & ~S_IFMT),
       PyGetByteArrayValue(self->buf), self->fname, self->status,
       self->io_errno, self->lerror, self->whence, self->offset, self->win32,
       self->filedes);
  s = PyUnicode_FromString(buf.c_str());

  return s;
}

// Initialization.
static int PyIoPacket_init(PyIoPacket* self, PyObject* args, PyObject* kwds)
{
  static char* kwlist[] = {(char*)"func",    (char*)"count",
                           (char*)"flags",   (char*)"mode",
                           (char*)"buf",     (char*)"fname",
                           (char*)"status",  (char*)"io_errno",
                           (char*)"lerror",  (char*)"whence",
                           (char*)"offset",  (char*)"win32",
                           (char*)"filedes", NULL};

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
  self->filedes = kInvalidFiledescriptor;

  if (!PyArg_ParseTupleAndKeywords(
          args, kwds, "|Hiiiosiiiilci", kwlist, &self->func, &self->count,
          &self->flags, &self->mode, &self->buf, &self->fname, &self->status,
          &self->io_errno, &self->lerror, &self->whence, &self->offset,
          &self->win32, &self->filedes)) {
    return -1;
  }

  return 0;
}

// Destructor.
static void PyIoPacket_dealloc(PyIoPacket* self)
{
  if (self->buf) { Py_XDECREF(self->buf); }
  PyObject_Del(self);
}

// Python specific handlers for PyAclPacket structure mapping.

// Representation.
static PyObject* PyAclPacket_repr(PyAclPacket* self)
{
  PyObject* s;
  PoolMem buf(PM_MESSAGE);

  Mmsg(buf, "AclPacket(fname=\"%s\", content=\"%s\")", self->fname,
       PyGetByteArrayValue(self->content));
  s = PyUnicode_FromString(buf.c_str());

  return s;
}

// Initialization.
static int PyAclPacket_init(PyAclPacket* self, PyObject* args, PyObject* kwds)
{
  static char* kwlist[] = {(char*)"fname", (char*)"content", NULL};

  self->fname = NULL;
  self->content = NULL;

  if (!PyArg_ParseTupleAndKeywords(args, kwds, "|so", kwlist, &self->fname,
                                   &self->content)) {
    return -1;
  }

  return 0;
}

// Destructor.
static void PyAclPacket_dealloc(PyAclPacket* self)
{
  if (self->content) { Py_XDECREF(self->content); }
  PyObject_Del(self);
}

// Python specific handlers for PyIOPacket structure mapping.

// Representation.
static PyObject* PyXattrPacket_repr(PyXattrPacket* self)
{
  PyObject* s;
  PoolMem buf(PM_MESSAGE);

  Mmsg(buf, "XattrPacket(fname=\"%s\", name=\"%s\", value=\"%s\")", self->fname,
       PyGetByteArrayValue(self->name), PyGetByteArrayValue(self->value));
  s = PyUnicode_FromString(buf.c_str());

  return s;
}

// Initialization.
static int PyXattrPacket_init(PyXattrPacket* self,
                              PyObject* args,
                              PyObject* kwds)
{
  static char* kwlist[] = {(char*)"fname", (char*)"name", (char*)"value", NULL};

  self->fname = NULL;
  self->name = NULL;
  self->value = NULL;

  if (!PyArg_ParseTupleAndKeywords(args, kwds, "|soo", kwlist, &self->fname,
                                   &self->name, &self->value)) {
    return -1;
  }

  return 0;
}

// Destructor.
static void PyXattrPacket_dealloc(PyXattrPacket* self)
{
  if (self->value) { Py_XDECREF(self->value); }
  if (self->name) { Py_XDECREF(self->name); }
  PyObject_Del(self);
}

} /* namespace filedaemon */
