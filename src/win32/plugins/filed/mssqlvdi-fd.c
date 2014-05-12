/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2010 Zilvinas Krapavickas <zkrapavickas@gmail.com>
   Copyright (C) 2013-2013 Bareos GmbH & Co. KG

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
 * MSSQL backup restore plugin using VDI.
 */
#include "bareos.h"
#include "fd_plugins.h"
#include "fd_common.h"

/*
 * Microsoft® Component Object Model (COM)
 */
#include <comutil.h>

/*
 * Microsoft® MSSQL Virtual Device Interface (VDI)
 */
#include "vdi.h"
#include "vdierror.h"
#include "vdiguid.h"

/*
 * Microsoft® ActiveX® Data Objects
 */
#include <initguid.h>
#include <adoid.h>
#include <adoint.h>

static const int dbglvl = 150;

#define PLUGIN_LICENSE      "Bareos AGPLv3"
#define PLUGIN_AUTHOR       "Zilvinas Krapavickas"
#define PLUGIN_DATE         "July 2013"
#define PLUGIN_VERSION      "1"
#define PLUGIN_DESCRIPTION  "Bareos MSSQL VDI Windows File Daemon Plugin"
#define PLUGIN_USAGE        "\n  mssqlvdi:\n"\
                            "  instance=<instance name>:\n"\
                            "  database=<database name>:\n"\
                            "  username=<database username>:\n"\
                            "  password=<database password>:\n"\
                            "  norecovery=<yes|no>:\n"\
                            "  replace=<yes|no>:\n"\
                            "  recoverafterrestore=<yes|no>:\n"\
                            "  stopbeforemark=<log sequence number specification>:\n"\
                            "  stopatmark=<log sequence number specification>:\n"\
                            "  stopat=<timestamp>\n"\
                            "  \n"\
                            " examples:\n"\
                            "  timestamp: 'Apr 15, 2020 12:00 AM'\n"\
                            "  log sequence number: 'lsn:15000000040000037'"


#define DEFAULT_INSTANCE    "default"
#define DEFAULT_BLOCKSIZE   65536
#define DEFAULT_BUFFERS     10
#define VDS_NAME_LENGTH     50
#define VDI_DEFAULT_WAIT    60000 /* 60 seconds */
#define VDI_WAIT_TIMEOUT    0xFFFFFFFF /* INFINITE */

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

static bRC parse_plugin_definition(bpContext *ctx, void *value);
static bRC end_restore_job(bpContext *ctx, void *value);
static void close_vdi_deviceset(struct plugin_ctx *p_ctx);
static bool adoReportError(bpContext *ctx);

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
   checkFile
};

/*
 * Plugin private context
 */
struct plugin_ctx {
   bool DoNoRecovery;
   bool ForceReplace;
   bool RecoverAfterRestore;
   char *plugin_options;
   char *filename;
   char *instance;
   char *database;
   char *username;
   char *password;
   char *stopbeforemark;
   char *stopatmark;
   char *stopat;
   char *ado_connect_string;
   char *ado_query;
   char *ado_errorstr;
   wchar_t *vdsname;
   int32_t backup_level;
   IClientVirtualDeviceSet2 *VDIDeviceSet;
   IClientVirtualDevice *VDIDevice;
   VDConfig VDIConfig;
   bool AdoThreadStarted;
   pthread_t ADOThread;
};

struct adoThreadContext {
   _ADOConnection *adoConnection;
   BSTR ado_connect_string;
   BSTR ado_query;
};

/*
 * This defines the arguments that the plugin parser understands.
 */
enum plugin_argument_type {
   argument_none,
   argument_instance,
   argument_database,
   argument_username,
   argument_password,
   argument_norecovery,
   argument_replace,
   argument_recover_after_restore,
   argument_stopbeforemark,
   argument_stopatmark,
   argument_stopat
};

struct plugin_argument {
   const char *name;
   enum plugin_argument_type type;
};

static plugin_argument plugin_arguments[] = {
   { "instance", argument_instance },
   { "database", argument_database },
   { "username", argument_username },
   { "password", argument_password },
   { "norecovery", argument_norecovery },
   { "replace", argument_replace },
   { "recoverafterrestore", argument_recover_after_restore },
   { "stopbeforemark", argument_stopbeforemark },
   { "stopatmark", argument_stopatmark },
   { "stopat", argument_stopat },
   { NULL, argument_none }
};

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
   HRESULT hr;
   plugin_ctx *p_ctx;

   /*
    * Initialize COM for this thread.
    */
   hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
   if (!SUCCEEDED (hr)) {
      return bRC_Error;
   }

   p_ctx = (plugin_ctx *)malloc(sizeof(plugin_ctx));
   if (!p_ctx) {
      return bRC_Error;
   }
   memset(p_ctx, 0, sizeof(plugin_ctx));
   ctx->pContext = (void *)p_ctx;        /* set our context pointer */

   /*
    * Only register the events we are really interested in.
    */
   bfuncs->registerBareosEvents(ctx,
                                6,
                                bEventLevel,
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

   Dmsg(ctx, dbglvl, "mssqlvdi-fd: entering freePlugin\n");

   /*
    * Close any open VDI deviceset.
    */
   close_vdi_deviceset(p_ctx);

   /*
    * See if there is any error to report from the ADO layer.
    */
   adoReportError(ctx);

   /*
    * Cleanup the context.
    */
   if (p_ctx->plugin_options) {
      free(p_ctx->plugin_options);
   }

   if (p_ctx->filename) {
      free(p_ctx->filename);
   }

   if (p_ctx->instance) {
      free(p_ctx->instance);
   }

   if (p_ctx->database) {
      free(p_ctx->database);
   }

   if (p_ctx->username) {
      free(p_ctx->username);
   }

   if (p_ctx->password) {
      free(p_ctx->password);
   }

   if (p_ctx->stopbeforemark) {
      free(p_ctx->stopbeforemark);
   }

   if (p_ctx->stopatmark) {
      free(p_ctx->stopatmark);
   }

   if (p_ctx->stopat) {
      free(p_ctx->stopat);
   }

   if (p_ctx->vdsname) {
      free(p_ctx->vdsname);
   }

   if (p_ctx->ado_connect_string) {
      free(p_ctx->ado_connect_string);
   }

   if (p_ctx->ado_query) {
      free(p_ctx->ado_query);
   }

   free(p_ctx);
   p_ctx = NULL;

   /*
    * Tear down COM for this thread.
    */
   CoUninitialize();

   Dmsg(ctx, dbglvl, "mssqlvdi-fd: leaving freePlugin\n");

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
   case bEventRestoreCommand:
     /*
      * Fall-through wanted
      */
   case bEventBackupCommand:
     /*
      * Fall-through wanted
      */
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
      Jmsg(ctx, M_FATAL, "mssqlvdi-fd: unknown event=%d\n", event->eventType);
      Dmsg(ctx, dbglvl, "mssqlvdi-fd: unknown event=%d\n", event->eventType);
      retval = bRC_Error;
      break;
   }

   return retval;
}

/*
 * Start the backup of a specific file
 */
static bRC startBackupFile(bpContext *ctx, struct save_pkt *sp)
{
   time_t now;
   POOL_MEM fname(PM_NAME);
   char dt[MAX_TIME_LENGTH];
   plugin_ctx *p_ctx = (plugin_ctx *)ctx->pContext;

   if (!p_ctx) {
      return bRC_Error;
   }

   /*
    * If no explicit instance name given use the DEFAULT_INSTANCE name.
    */
   if (!p_ctx->instance) {
      p_ctx->instance = bstrdup(DEFAULT_INSTANCE);
   }

   now = time(NULL);
   switch (p_ctx->backup_level) {
   case L_FULL:
      Mmsg(fname, "/@MSSQL/%s/%s/db-full", p_ctx->instance, p_ctx->database);
      break;
   case L_DIFFERENTIAL:
      Mmsg(fname, "/@MSSQL/%s/%s/db-diff", p_ctx->instance, p_ctx->database);
      break;
   case L_INCREMENTAL:
      bstrutime(dt, sizeof(dt), now);
      Mmsg(fname, "/@MSSQL/%s/%s/log-%s", p_ctx->instance, p_ctx->database, dt);
      break;
   default:
      Jmsg(ctx, M_FATAL, "Unsuported backup level (%c).\n", p_ctx->backup_level);
      Dmsg(ctx, dbglvl, "Unsuported backup level (%c).\n", p_ctx->backup_level);
      return bRC_Error;
   }

   p_ctx->filename = bstrdup(fname.c_str());
   Dmsg(ctx, dbglvl, "startBackupFile: Generated filename %s\n", p_ctx->filename);

   sp->fname = p_ctx->filename;
   sp->type = FT_REG;
   sp->statp.st_mode = S_IFREG | S_IREAD | S_IWRITE | S_IEXEC;
   sp->statp.st_ctime = now;
   sp->statp.st_mtime = now;
   sp->statp.st_atime = now;
   sp->statp.st_size = 0;
   sp->statp.st_blksize = DEFAULT_BLOCKSIZE;
   sp->statp.st_blocks = 1;

   return bRC_OK;
}

/*
 * Done with backup of this file
 */
static bRC endBackupFile(bpContext *ctx)
{
   /*
    * We would return bRC_More if we wanted startBackupFile to be called again to backup another file
    */
   return bRC_OK;
}

/*
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

/*
 * Only set destination to value when it has no previous setting.
 */
static inline void set_string_if_null(char **destination, char *value)
{
   if (!*destination) {
      *destination = bstrdup(value);
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
}

/*
 * Parse the plugin definition passed in.
 *
 * The definition is in this form:
 *
 * mssqlvdi:instance=<instance>:database=<databasename>:
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
      Jmsg(ctx, M_FATAL, "Illegal plugin definition %s\n", plugin_definition);
      Dmsg(ctx, dbglvl, "Illegal plugin definition %s\n", plugin_definition);
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
         Jmsg(ctx, M_FATAL, "Illegal argument %s without value\n", argument);
         Dmsg(ctx, dbglvl, "Illegal argument %s without value\n", argument);
         goto bail_out;
      }
      *argument_value++ = '\0';

      /*
       * See if there are more arguments and setup for the next run.
       */
      bp = strchr(argument_value, ':');
      if (bp) {
         *bp++ = '\0';
      }

      for (i = 0; plugin_arguments[i].name; i++) {
         if (bstrcasecmp(argument, plugin_arguments[i].name)) {
            char **str_destination = NULL;
            bool *bool_destination = NULL;

            switch (plugin_arguments[i].type) {
            case argument_instance:
               str_destination = &p_ctx->instance;
               break;
            case argument_database:
               str_destination = &p_ctx->database;
               break;
            case argument_username:
               str_destination = &p_ctx->username;
               break;
            case argument_password:
               str_destination = &p_ctx->password;
               break;
            case argument_norecovery:
               bool_destination = &p_ctx->DoNoRecovery;
               break;
            case argument_replace:
               bool_destination = &p_ctx->ForceReplace;
               break;
            case argument_recover_after_restore:
               bool_destination = &p_ctx->RecoverAfterRestore;
               break;
            case argument_stopbeforemark:
               str_destination = &p_ctx->stopbeforemark;
               break;
            case argument_stopatmark:
               str_destination = &p_ctx->stopatmark;
               break;
            case argument_stopat:
               str_destination = &p_ctx->stopat;
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
            break;
         }
      }

      /*
       * Got an invalid keyword ?
       */
      if (!plugin_arguments[i].name) {
         Jmsg(ctx, M_FATAL, "Illegal argument %s with value %s in plugin definition\n", argument, argument_value);
         Dmsg(ctx, dbglvl, "Illegal argument %s with value %s in plugin definition\n", argument, argument_value);
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
 * Close the VDI deviceset if is is opened.
 */
static void close_vdi_deviceset(plugin_ctx *p_ctx)
{
   /*
    * Close VDI Device.
    */
   if (p_ctx->VDIDevice) {
      p_ctx->VDIDevice->Release();
      p_ctx->VDIDevice = NULL;
   }

   /*
    * Close VDI DeviceSet.
    */
   if (p_ctx->VDIDeviceSet) {
      p_ctx->VDIDeviceSet->Close();
      p_ctx->VDIDeviceSet->Release();
      p_ctx->VDIDeviceSet = NULL;
   }

   /*
    * Cancel the started database thread.
    */
   if (p_ctx->AdoThreadStarted) {
      if (!pthread_equal(p_ctx->ADOThread, pthread_self())) {
         pthread_cancel(p_ctx->ADOThread);
      }
   }
}

/*
 * Generic COM error reporting function.
 */
static void comReportError(bpContext *ctx, HRESULT hrErr)
{
   IErrorInfo *pErrorInfo;
   BSTR pSource = NULL;
   BSTR pDescription = NULL;
   HRESULT hr;
   char *source, *description;

   /*
    * See if there is anything to report.
    */
   hr = GetErrorInfo(0, &pErrorInfo);
   if (hr == S_FALSE) {
      return;
   }

   /*
    * Get the description of the COM error.
    */
   hr = pErrorInfo->GetDescription(&pDescription);
   if (!SUCCEEDED (hr)) {
      pErrorInfo->Release();
      return;
   }

   /*
    * Get the source of the COM error.
    */
   hr = pErrorInfo->GetSource(&pSource);
   if (!SUCCEEDED (hr)) {
      SysFreeString(pDescription);
      pErrorInfo->Release();
      return;
   }

   /*
    * Convert windows BSTR to normal strings.
    */
   source = BSTR_2_str(pSource);
   description = BSTR_2_str(pDescription);
   if (source && description) {
      Jmsg(ctx, M_FATAL, "%s(x%X): %s\n", source, hrErr, description);
      Dmsg(ctx, dbglvl, "%s(x%X): %s\n", source, hrErr, description);
   }

   if (source) {
      free(source);
   }

   if (description) {
      free(description);
   }

   /*
    * Generic cleanup (free the description and source as those are returned in
    * dynamically allocated memory by the COM routines.)
    */
   SysFreeString(pSource);
   SysFreeString(pDescription);

   pErrorInfo->Release();
}

/*
 * Retrieve errors from ADO Connection.
 */
static bool adoGetErrors(bpContext *ctx, _ADOConnection *adoConnection, POOL_MEM &ado_errorstr)
{
   HRESULT hr;
   ADOErrors *adoErrors;
   long errCount;

   /*
    * Get any errors that are reported.
    */
   hr = adoConnection->get_Errors(&adoErrors);
   if (!SUCCEEDED (hr)) {
      comReportError(ctx, hr);
      goto bail_out;
   }

   /*
    * See how many errors there are.
    */
   hr = adoErrors->get_Count(&errCount);
   if (!SUCCEEDED (hr)) {
      comReportError(ctx, hr);
      adoErrors->Release();
      goto bail_out;
   }

   /*
    * Loop over all error and append them into one big error string.
    */
   pm_strcpy(ado_errorstr, "");
   for (long i = 0; i < errCount; i++ ) {
      ADOError *adoError;
      BSTR pDescription = NULL;
      char *description;

      hr = adoErrors->get_Item(_variant_t(i), &adoError);
      if (!SUCCEEDED (hr)) {
         comReportError(ctx, hr);
         adoErrors->Release();
         goto bail_out;
      }

      hr = adoError->get_Description(&pDescription);
      if (!SUCCEEDED (hr)) {
         comReportError(ctx, hr);
         adoError->Release();
         adoErrors->Release();
         goto bail_out;
      }

      description = BSTR_2_str(pDescription);
      if (description) {
         Dmsg(ctx, dbglvl, "adoGetErrors: ADO error %s\n", description);
         pm_strcat(ado_errorstr, description);
         pm_strcat(ado_errorstr, "\n");
         free(description);
      }

      SysFreeString(pDescription);
      adoError->Release();
   }

   /*
    * Generic cleanup.
    */
   adoErrors->Clear();
   adoErrors->Release();

   return true;

bail_out:
   return false;
}

/*
 * Print errors (when available) collected by adoThreadSetError function.
 */
static bool adoReportError(bpContext *ctx)
{
   plugin_ctx *p_ctx = (plugin_ctx *)ctx->pContext;

   if (p_ctx->ado_errorstr) {
      Jmsg(ctx, M_FATAL, "%s\n", p_ctx->ado_errorstr);
      Dmsg(ctx, dbglvl, "%s\n", p_ctx->ado_errorstr);

      free(p_ctx->ado_errorstr);
      p_ctx->ado_errorstr = NULL;

      return true;
   }

   return false;
}

/*
 * Retrieve errors from ADO Connection when running the query in a seperate thread.
 */
static void adoThreadSetError(bpContext *ctx, _ADOConnection *adoConnection)
{
   int cancel_type;
   POOL_MEM ado_errorstr(PM_NAME);
   plugin_ctx *p_ctx = (plugin_ctx *)ctx->pContext;

   if (p_ctx->ado_errorstr) {
      return;
   }

   /*
    * Set the threads cancellation type to defered.
    */
   pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, &cancel_type);

   if (!adoGetErrors(ctx, adoConnection, ado_errorstr)) {
      goto bail_out;
   }

   /*
    * Keep the errors in a buffer these will be printed by the adoReportError function.
    */
   p_ctx->ado_errorstr = bstrdup(ado_errorstr.c_str());

bail_out:
   /*
    * Restore the threads cancellation type.
    */
   pthread_setcanceltype(cancel_type, NULL);

   return;
}

/*
 * Cleanup function called on thread destroy.
 */
static void adoCleanupThread(void *data)
{
   adoThreadContext *ado_ctx;

   ado_ctx = (adoThreadContext *)data;
   if (!data) {
      return;
   }

   /*
    * Generic cleanup.
    */
   if (ado_ctx->ado_connect_string) {
      SysFreeString(ado_ctx->ado_connect_string);
   }

   if (ado_ctx->ado_query) {
      SysFreeString(ado_ctx->ado_query);
   }

   if (ado_ctx->adoConnection) {
      LONG adoState;

      ado_ctx->adoConnection->get_State(&adoState);
      if (adoState & adStateExecuting) {
         ado_ctx->adoConnection->Cancel();
      }

      if (adoState & adStateOpen) {
         ado_ctx->adoConnection->Close();
      }

      ado_ctx->adoConnection->Release();
   }

   /*
    * Tear down COM for this thread.
    */
   CoUninitialize();
}

/*
 * Run a seperate thread that connects to the database server and
 * controls the backup or restore. When we close the VDI device we
 * also tear down this database control thread.
 */
static void *adoThread(void *data)
{
   HRESULT hr;
   adoThreadContext ado_ctx;
   bpContext *ctx = (bpContext *)data;
   plugin_ctx *p_ctx = (struct plugin_ctx *)ctx->pContext;

   memset(&ado_ctx, 0, sizeof(ado_ctx));

   /*
    * When we get canceled make sure we run the cleanup function.
    */
   pthread_cleanup_push(adoCleanupThread, &ado_ctx);

   /*
    * Initialize COM for this thread.
    */
   hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
   if (!SUCCEEDED (hr)) {
      return NULL;
   }

   /*
    * Create a COM instance for an ActiveX® Data Objects connection.
    */
   hr = CoCreateInstance(CLSID_CADOConnection,
                         NULL,
                         CLSCTX_INPROC_SERVER,
                         IID_IADOConnection,
                         (void **)&ado_ctx.adoConnection);
   if (!SUCCEEDED (hr)) {
      goto bail_out;
   }

   /*
    * Make sure the connection doesn't timeout.
    * Default timeout is not long enough most of the time
    * for the backup or restore to finish and when it times
    * out it will abort the action it was performing.
    */
   hr = ado_ctx.adoConnection->put_CommandTimeout(0);
   if (!SUCCEEDED (hr)) {
      adoThreadSetError(ctx, ado_ctx.adoConnection);
      goto bail_out;
   }

   /*
    * Open a connection to the database server with the defined connection string.
    */
   ado_ctx.ado_connect_string = str_2_BSTR(p_ctx->ado_connect_string);
   hr = ado_ctx.adoConnection->Open(ado_ctx.ado_connect_string);
   if (!SUCCEEDED (hr)) {
      adoThreadSetError(ctx, ado_ctx.adoConnection);
      goto bail_out;
   }

   /*
    * Execute the backup or restore command.
    */
   ado_ctx.ado_query = str_2_BSTR(p_ctx->ado_query);
   hr = ado_ctx.adoConnection->Execute(ado_ctx.ado_query, NULL, adExecuteNoRecords, NULL);
   if (!SUCCEEDED (hr)) {
      adoThreadSetError(ctx, ado_ctx.adoConnection);
      goto bail_out;
   }

bail_out:
   /*
    * Run the thread cleanup.
    */
   pthread_cleanup_pop(1);

   return NULL;
}

/*
 * Create a connection string for connecting to the master database.
 */
static void set_ado_connect_string(bpContext *ctx)
{
   POOL_MEM ado_connect_string(PM_NAME);
   plugin_ctx *p_ctx = (plugin_ctx *)ctx->pContext;

   if (bstrcasecmp(p_ctx->instance, DEFAULT_INSTANCE)) {
      pm_strcpy(ado_connect_string,
                "Provider=SQLOLEDB.1;Data Source=localhost;Initial Catalog=master");
   } else {
      Mmsg(ado_connect_string,
           "Provider=SQLOLEDB.1;Data Source=localhost\\%s;Initial Catalog=master",
           p_ctx->instance);
   }

   /*
    * See if we need to use a username/password or a trusted connection.
    */
   if (p_ctx->username && p_ctx->password) {
      POOL_MEM temp(PM_NAME);

      Mmsg(temp, ";User Id=%s;Password=%s;",
            p_ctx->username, p_ctx->password);
      pm_strcat(ado_connect_string, temp.c_str());
   } else {
      pm_strcat(ado_connect_string, ";Integrated Security=SSPI;");
   }

   Dmsg(ctx, dbglvl, "set_ado_connect_string: ADO Connect String '%s'\n", ado_connect_string.c_str());

   if (p_ctx->ado_connect_string) {
      free(p_ctx->ado_connect_string);
   }
   p_ctx->ado_connect_string = bstrdup(ado_connect_string.c_str());
}

/*
 * Generate a valid connect string and the backup command we should execute
 * in the seperate database controling thread.
 */
static inline void perform_ado_backup(bpContext *ctx)
{
   plugin_ctx *p_ctx = (plugin_ctx *)ctx->pContext;
   POOL_MEM ado_connect_string(PM_NAME),
            ado_query(PM_NAME);
   char vdsname[VDS_NAME_LENGTH + 1];

   /*
    * If no explicit instance name given usedthe DEFAULT_INSTANCE name.
    */
   if (!p_ctx->instance) {
      p_ctx->instance = bstrdup(DEFAULT_INSTANCE);
   }

   set_ado_connect_string(ctx);

   sprintf_s(vdsname, sizeof(vdsname), "%S", p_ctx->vdsname);
   switch (p_ctx->backup_level) {
   case L_INCREMENTAL:
      Mmsg(ado_query,
           "BACKUP LOG %s TO VIRTUAL_DEVICE='%s' WITH BLOCKSIZE=%d, BUFFERCOUNT=%d, MAXTRANSFERSIZE=%d",
           p_ctx->database,
           vdsname,
           DEFAULT_BLOCKSIZE,
           DEFAULT_BUFFERS,
           DEFAULT_BLOCKSIZE);
        break;
   case L_DIFFERENTIAL:
      Mmsg(ado_query,
           "BACKUP DATABASE %s TO VIRTUAL_DEVICE='%s' WITH DIFFERENTIAL, BLOCKSIZE=%d, BUFFERCOUNT=%d, MAXTRANSFERSIZE=%d",
           p_ctx->database,
           vdsname,
           DEFAULT_BLOCKSIZE,
           DEFAULT_BUFFERS,
           DEFAULT_BLOCKSIZE);
        break;
   default:
      Mmsg(ado_query,
           "BACKUP DATABASE %s TO VIRTUAL_DEVICE='%s' WITH BLOCKSIZE=%d, BUFFERCOUNT=%d, MAXTRANSFERSIZE=%d",
           p_ctx->database,
           vdsname,
           DEFAULT_BLOCKSIZE,
           DEFAULT_BUFFERS,
           DEFAULT_BLOCKSIZE);
        break;
   }

   Dmsg(ctx, dbglvl, "perform_ado_backup: ADO Query '%s'\n", ado_query.c_str());

   p_ctx->ado_query = bstrdup(ado_query.c_str());
}

/*
 * Generate a valid connect string and the restore command we should execute
 * in the seperate database controling thread.
 */
static inline void perform_ado_restore(bpContext *ctx)
{
   POOL_MEM ado_query(PM_NAME),
            temp(PM_NAME);
   char vdsname[VDS_NAME_LENGTH + 1];
   plugin_ctx *p_ctx = (plugin_ctx *)ctx->pContext;

   /*
    * If no explicit instance name given use the DEFAULT_INSTANCE name.
    */
   if (!p_ctx->instance) {
      p_ctx->instance = bstrdup(DEFAULT_INSTANCE);
   }

   set_ado_connect_string(ctx);

   sprintf_s(vdsname, sizeof(vdsname), "%S", p_ctx->vdsname);
   switch (p_ctx->backup_level) {
   case L_INCREMENTAL:
      Mmsg(ado_query,
           "RESTORE LOG %s FROM VIRTUAL_DEVICE='%s' WITH BLOCKSIZE=%d, BUFFERCOUNT=%d, MAXTRANSFERSIZE=%d, %s",
           p_ctx->database,
           vdsname,
           DEFAULT_BLOCKSIZE,
           DEFAULT_BUFFERS,
           DEFAULT_BLOCKSIZE,
           p_ctx->DoNoRecovery ? "NORECOVERY" : "RECOVERY");
      break;
   default:
      Mmsg(ado_query,
           "RESTORE DATABASE %s FROM VIRTUAL_DEVICE='%s' WITH BLOCKSIZE=%d, BUFFERCOUNT=%d, MAXTRANSFERSIZE=%d, %s",
           p_ctx->database,
           vdsname,
           DEFAULT_BLOCKSIZE,
           DEFAULT_BUFFERS,
           DEFAULT_BLOCKSIZE,
           p_ctx->DoNoRecovery ? "NORECOVERY" : "RECOVERY");
      break;
   }

   /*
    * See if we need to insert any stopbeforemark arguments.
    */
   if (p_ctx->stopbeforemark) {
      Mmsg(temp, ", STOPBEFOREMARK = '%s'", p_ctx->stopbeforemark);
      pm_strcat(ado_query, temp.c_str());
   }

   /*
    * See if we need to insert any stopatmark arguments.
    */
   if (p_ctx->stopatmark) {
      Mmsg(temp, ", STOPATMARK = '%s'", p_ctx->stopatmark);
      pm_strcat(ado_query, temp.c_str());
   }

   /*
    * See if we need to insert any stopat arguments.
    */
   if (p_ctx->stopat) {
      Mmsg(temp, ", STOPAT = '%s'", p_ctx->stopat);
      pm_strcat(ado_query, temp.c_str());
   }

   /*
    * See if we need to insert the REPLACE option.
    */
   if (p_ctx->ForceReplace) {
      pm_strcat(ado_query, ", REPLACE");
   }

   Dmsg(ctx, dbglvl, "perform_ado_restore: ADO Query '%s'\n", ado_query.c_str());

   p_ctx->ado_query = bstrdup(ado_query.c_str());
}


/*
 * Run a query not in a seperate thread.
 */
static inline bool run_ado_query(bpContext *ctx, const char *query)
{
   bool retval = false;
   HRESULT hr;
   BSTR ado_connect_string = NULL;
   BSTR ado_query = NULL;
   _ADOConnection *adoConnection = NULL;
   plugin_ctx *p_ctx = (plugin_ctx *)ctx->pContext;

   Dmsg(ctx, dbglvl, "run_ado_query: ADO Query '%s'\n", query);

   /*
    * Create a COM instance for an ActiveX® Data Objects connection.
    */
   hr = CoCreateInstance(CLSID_CADOConnection,
                         NULL,
                         CLSCTX_INPROC_SERVER,
                         IID_IADOConnection,
                         (void **)&adoConnection);
   if (!SUCCEEDED (hr)) {
      goto cleanup;
   }

   /*
    * Open a connection to the database server with the defined connection string.
    */
   ado_connect_string = str_2_BSTR(p_ctx->ado_connect_string);
   hr = adoConnection->Open(ado_connect_string);
   if (!SUCCEEDED (hr)) {
      POOL_MEM ado_errorstr(PM_NAME);

      adoGetErrors(ctx, adoConnection, ado_errorstr);
      Jmsg(ctx, M_FATAL, "Failed to connect to database, %s\n", ado_errorstr.c_str());
      Dmsg(ctx, dbglvl, "Failed to connect to database, %s\n", ado_errorstr.c_str());
      goto cleanup;
   }

   /*
    * Perform the query.
    */
   ado_query = str_2_BSTR(query);
   hr = adoConnection->Execute(ado_query, NULL, adExecuteNoRecords, NULL);
   if (!SUCCEEDED (hr)) {
      POOL_MEM ado_errorstr(PM_NAME);

      adoGetErrors(ctx, adoConnection, ado_errorstr);
      Jmsg(ctx, M_FATAL, "Failed to execute query %s on database, %s\n", query, ado_errorstr.c_str());
      Dmsg(ctx, dbglvl, "Failed to execute query %s on database, %s\n", query, ado_errorstr.c_str());
      goto cleanup;
   }

   retval = true;

cleanup:
   if (ado_connect_string) {
      SysFreeString(ado_connect_string);
   }

   if (ado_query) {
      SysFreeString(ado_query);
   }

   if (adoConnection) {
      LONG adoState;

      adoConnection->get_State(&adoState);
      if (adoState & adStateExecuting) {
         adoConnection->Cancel();
      }

      if (adoState & adStateOpen) {
         adoConnection->Close();
      }

      adoConnection->Release();
   }

   return retval;
}

/*
 * Automatically recover the database at the end of the whole restore process.
 */
static inline bool perform_ado_recover(bpContext *ctx)
{
   POOL_MEM recovery_query(PM_NAME);
   plugin_ctx *p_ctx = (plugin_ctx *)ctx->pContext;

   set_ado_connect_string(ctx);
   Mmsg(recovery_query, "RESTORE DATABASE %s WITH RECOVERY", p_ctx->database);

   return run_ado_query(ctx, recovery_query.c_str());
}

/*
 * Setup a VDI device for performing a backup or restore operation.
 */
static inline bool setup_vdi_device(bpContext *ctx, struct io_pkt *io)
{
   int status;
   HRESULT hr = NOERROR;
   GUID vdsId;
   plugin_ctx *p_ctx = (plugin_ctx *)ctx->pContext;

   if ((p_ctx->username && !p_ctx->password) ||
       (!p_ctx->username && p_ctx->password)) {
      Jmsg(ctx, M_FATAL, "Illegal plugin definition when using username or password define both\n");
      return false;
   }

   CoCreateGuid(&vdsId);
   p_ctx->vdsname = (wchar_t *)malloc((VDS_NAME_LENGTH * sizeof(wchar_t)) + 2);
   StringFromGUID2(vdsId, p_ctx->vdsname, VDS_NAME_LENGTH);

   /*
    * Get a handle to the device set.
    */
   hr = CoCreateInstance(CLSID_MSSQL_ClientVirtualDeviceSet,
                         NULL,
                         CLSCTX_INPROC_SERVER,
                         IID_IClientVirtualDeviceSet2,
                         (void **)&p_ctx->VDIDeviceSet);
   if (!SUCCEEDED(hr)) {
      comReportError(ctx, hr);
      return false;
   }

   /*
    * Setup the VDI configuration.
    */
   memset(&p_ctx->VDIConfig, 0, sizeof(p_ctx->VDIConfig));
   p_ctx->VDIConfig.deviceCount = 1;
   p_ctx->VDIConfig.features = VDF_LikePipe;

   /*
    * Create the VDI device set.
    */
   hr = p_ctx->VDIDeviceSet->CreateEx(NULL, p_ctx->vdsname, &p_ctx->VDIConfig);
   if (!SUCCEEDED(hr)) {
      comReportError(ctx, hr);
      return false;
   }

   /*
    * Setup the right backup or restore cmdline and connect info.
    */
   if (io->flags & (O_CREAT | O_WRONLY)) {
      perform_ado_restore(ctx);
   } else {
      perform_ado_backup(ctx);
   }

   /*
    * Ask the database server to start a backup or restore via an other thread.
    * We create a new thread that handles the connection to the database.
    */
   status = pthread_create(&p_ctx->ADOThread, NULL, adoThread, (void *)ctx);
   if (status != 0) {
      return false;
   }

   /*
    * Track that we have started the thread and as such need to kill it when
    * we perform a close of the VDI device.
    */
   p_ctx->AdoThreadStarted = true;

   /*
    * Wait for the database server to connect to the VDI deviceset.
    */
   hr = p_ctx->VDIDeviceSet->GetConfiguration(VDI_DEFAULT_WAIT, &p_ctx->VDIConfig);
   if (!SUCCEEDED(hr)) {
      Jmsg(ctx, M_FATAL, "mssqlvdi-fd: IClientVirtualDeviceSet2::GetConfiguration failed\n");
      Dmsg(ctx, dbglvl, "mssqlvdi-fd: IClientVirtualDeviceSet2::GetConfiguration failed\n");
      goto bail_out;
   }

   hr = p_ctx->VDIDeviceSet->OpenDevice(p_ctx->vdsname, &p_ctx->VDIDevice);
   if (!SUCCEEDED(hr)) {
      char vdsname[VDS_NAME_LENGTH + 1];

      sprintf_s(vdsname, sizeof(vdsname), "%S", p_ctx->vdsname);
      Jmsg(ctx, M_FATAL, "mssqlvdi-fd: IClientVirtualDeviceSet2::OpenDevice(%s)\n", vdsname);
      Dmsg(ctx, dbglvl, "mssqlvdi-fd: IClientVirtualDeviceSet2::OpenDevice(%s)\n", vdsname);
      goto bail_out;
   }

   io->status = 0;
   io->io_errno = 0;
   io->lerror = 0;
   io->win32 = false;

   return true;

bail_out:
   /*
    * Report any COM errors.
    */
   comReportError(ctx, hr);

   /*
    * Wait for the adoThread to exit.
    */
   if (p_ctx->AdoThreadStarted) {
      if (!pthread_equal(p_ctx->ADOThread, pthread_self())) {
         pthread_cancel(p_ctx->ADOThread);
         pthread_join(p_ctx->ADOThread, NULL);
         p_ctx->AdoThreadStarted = false;
      }
   }

   return false;
}

/*
 * Perform an I/O operation as part of a backup or restore.
 */
static inline bool perform_vdi_io(bpContext *ctx, struct io_pkt *io, DWORD *completionCode)
{
   HRESULT hr = NOERROR;
   VDC_Command *cmd;
   plugin_ctx *p_ctx = (plugin_ctx *)ctx->pContext;

   /*
    * See what command is available on the VDIDevice.
    */
   hr = p_ctx->VDIDevice->GetCommand(VDI_WAIT_TIMEOUT , &cmd);
   if (!SUCCEEDED(hr)) {
      Jmsg(ctx, M_ERROR, "mssqlvdi-fd: IClientVirtualDevice::GetCommand: x%X\n", hr);
      Dmsg(ctx, dbglvl, "mssqlvdi-fd: IClientVirtualDevice::GetCommand: x%X\n", hr);
      goto bail_out;
   }

   switch (cmd->commandCode) {
   case VDC_Read:
      /*
       * Make sure the write to the VDIDevice will fit e.g. not a to big IO and
       * that we are currently writing to the device.
       */
      if ((DWORD)io->count > cmd->size || io->func != IO_WRITE) {
         *completionCode = ERROR_BAD_ENVIRONMENT;
         goto bail_out;
      } else {
         memcpy(cmd->buffer, io->buf, io->count);
         io->status = io->count;
         *completionCode = ERROR_SUCCESS;
      }
      break;
   case VDC_Write:
      /*
       * Make sure the read from the VDIDevice will fit e.g. not a to big IO and
       * that we are currently reading from the device.
       */
      if (cmd->size > (DWORD)io->count || io->func != IO_READ) {
         *completionCode = ERROR_BAD_ENVIRONMENT;
         goto bail_out;
      } else {
          memcpy(io->buf, cmd->buffer, cmd->size);
          io->status = cmd->size;
          *completionCode = ERROR_SUCCESS;
      }
      break;
   case VDC_Flush:
      io->status = 0;
      *completionCode = ERROR_SUCCESS;
      break;
   case VDC_ClearError:
      *completionCode = ERROR_SUCCESS;
      break;
   default:
      *completionCode = ERROR_NOT_SUPPORTED;
      goto bail_out;
   }

   hr = p_ctx->VDIDevice->CompleteCommand(cmd, *completionCode, io->status, 0);
   if (!SUCCEEDED(hr)) {
      Jmsg(ctx, M_ERROR, "mssqlvdi-fd: IClientVirtualDevice::CompleteCommand: x%X\n", hr);
      Dmsg(ctx, dbglvl, "mssqlvdi-fd: IClientVirtualDevice::CompleteCommand: x%X\n", hr);
      goto bail_out;
   }

   io->io_errno = 0;
   io->lerror = 0;
   io->win32 = false;

   return true;

bail_out:
   /*
    * Report any COM errors.
    */
   comReportError(ctx, hr);

   /*
    * Wait for the adoThread to exit.
    */
   if (p_ctx->AdoThreadStarted) {
      if (!pthread_equal(p_ctx->ADOThread, pthread_self())) {
         pthread_cancel(p_ctx->ADOThread);
         pthread_join(p_ctx->ADOThread, NULL);
         p_ctx->AdoThreadStarted = false;
      }
   }

   return false;
}

/*
 * End of I/O tear down the VDI and check if everything did go to plan.
 */
static inline bool tear_down_vdi_device(bpContext *ctx, struct io_pkt *io)
{
   HRESULT hr = NOERROR;
   VDC_Command *cmd;
   plugin_ctx *p_ctx = (plugin_ctx *)ctx->pContext;

   Dmsg(ctx, dbglvl, "mssqlvdi-fd: entering tear_down_vdi_device\n");

   /*
    * Check if the VDI device is closed.
    */
   if (p_ctx->VDIDevice) {
      hr = p_ctx->VDIDevice->GetCommand(VDI_WAIT_TIMEOUT , &cmd);
      if (hr != VD_E_CLOSE) {
         Jmsg(ctx, M_ERROR, "Abnormal termination, VDIDevice not closed.");
         Dmsg(ctx, dbglvl, "Abnormal termination, VDIDevice not closed.");
         goto bail_out;
      }
   }

   /*
    * Close and release the VDIDevice and VDIDeviceSet.
    */
   close_vdi_deviceset(p_ctx);

   /*
    * See if there is any error to report from the ADO layer.
    */
   if (p_ctx->AdoThreadStarted) {
      if (adoReportError(ctx)) {
         goto bail_out;
      }
   }

   io->status = 0;
   io->io_errno = 0;
   io->lerror = 0;
   io->win32 = false;

   Dmsg(ctx, dbglvl, "mssqlvdi-fd: leaving tear_down_vdi_device\n");

   return true;

bail_out:
   /*
    * Report any COM errors.
    */
   comReportError(ctx, hr);

   Dmsg(ctx, dbglvl, "mssqlvdi-fd: leaving tear_down_vdi_device\n");

   return false;
}

/*
 * Bareos is calling us to do the actual I/O
 */
static bRC pluginIO(bpContext *ctx, struct io_pkt *io)
{
   DWORD completionCode = ERROR_BAD_ENVIRONMENT;
   plugin_ctx *p_ctx = (plugin_ctx *)ctx->pContext;

   if (!p_ctx) {
      return bRC_Error;
   }

   switch(io->func) {
   case IO_OPEN:
      if (!setup_vdi_device(ctx, io)) {
         goto bail_out;
      }
      break;
   case IO_READ:
   case IO_WRITE:
      if (!p_ctx->VDIDevice) {
         return bRC_Error;
      }
      if (!perform_vdi_io(ctx, io, &completionCode)) {
         goto bail_out;
      }
      break;
   case IO_CLOSE:
      if (!tear_down_vdi_device(ctx, io)) {
         goto bail_out;
      }
      break;
   case IO_SEEK:
      Jmsg(ctx, M_ERROR, "Illegal Seek request on VDIDevice.");
      Dmsg(ctx, dbglvl, "Illegal Seek request on VDIDevice.");
      goto bail_out;
   }

   return bRC_OK;

bail_out:
   /*
    * Report any ADO errors.
    */
   adoReportError(ctx);

   /*
    * Generic error handling.
    */
   close_vdi_deviceset(p_ctx);

   io->io_errno = completionCode;
   io->lerror = completionCode;
   io->win32 = true;
   io->status = -1;

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

   Dmsg(ctx, dbglvl, "mssqlvdi-fd: entering end_restore_job\n");

   if (p_ctx->RecoverAfterRestore) {
      if (!perform_ado_recover(ctx)) {
         retval = bRC_Error;
      }
   }

   Dmsg(ctx, dbglvl, "mssqlvdi-fd: leaving end_restore_job\n");

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
   rp->create_status = CF_EXTRACT;
   return bRC_OK;
}

/*
 * We will get here if the File is a directory after everything is written in the directory.
 */
static bRC setFileAttributes(bpContext *ctx, struct restore_pkt *rp)
{
   return bRC_OK;
}

/*
 * When using Incremental dump, all previous dumps are necessary
 */
static bRC checkFile(bpContext *ctx, char *fname)
{
   return bRC_OK;
}
