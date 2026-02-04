/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2010 Zilvinas Krapavickas <zkrapavickas@gmail.com>
   Copyright (C) 2013-2014 Planets Communications B.V.
   Copyright (C) 2013-2025 Bareos GmbH & Co. KG

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
 * MSSQL backup restore plugin using VDI.
 */
#define __MIDL_user_allocate_free_DEFINED__ 1
#include "include/bareos.h"
#include "include/filetypes.h"
#include "fd_plugins.h"
#include "plugins/include/common.h"
#include <map>

// Microsoft® Component Object Model (COM)
#include <comutil.h>
#include <array>
// Microsoft® MSSQL Virtual Device Interface (VDI)
#include "vdi.h"
#include "vdierror.h"
#include "vdiguid.h"

// Microsoft® ActiveX® Data Objects
#include <initguid.h>
#include <adoid.h>
#include <adoint.h>
#include <stdexcept>

namespace filedaemon {

static const int debuglevel = 150;

#define PLUGIN_LICENSE "Bareos AGPLv3"
#define PLUGIN_AUTHOR "Zilvinas Krapavickas"
#define PLUGIN_DATE "October 2014"
#define PLUGIN_VERSION "2"
#define PLUGIN_DESCRIPTION "Bareos MSSQL VDI Windows File Daemon Plugin"
#define PLUGIN_USAGE                                        \
  "\n  mssqlvdi:\n"                                         \
  "  serveraddress=<hostname>:\n"                           \
  "  instance=<instance name>:\n"                           \
  "  database=<database name>:\n"                           \
  "  username=<database username>:\n"                       \
  "  password=<database password>:\n"                       \
  "  norecovery=<yes|no>:\n"                                \
  "  replace=<yes|no>:\n"                                   \
  "  recoverafterrestore=<yes|no>:\n"                       \
  "  stopbeforemark=<log sequence number specification>:\n" \
  "  stopatmark=<log sequence number specification>:\n"     \
  "  stopat=<timestamp>\n"                                  \
  "  getconfigurationtimeout=<timeout-seconds>\n"           \
  "  \n"                                                    \
  " examples:\n"                                            \
  "  timestamp: 'Apr 15, 2020 12:00 AM'\n"                  \
  "  log sequence number: 'lsn:15000000040000037'"

#define DEFAULT_SERVER_ADDRESS "localhost"
#define DEFAULT_INSTANCE "default"
#define DEFAULT_BLOCKSIZE 65536
#define DEFAULT_BUFFERS 10
#define VDS_NAME_LENGTH 50
#define VDI_DEFAULT_WAIT 60000      /* 60 seconds */
#define VDI_WAIT_TIMEOUT 0xFFFFFFFF /* INFINITE */

// Forward referenced functions
static bRC newPlugin(PluginContext* ctx);
static bRC freePlugin(PluginContext* ctx);
static bRC getPluginValue(PluginContext* ctx, pVariable var, void* value);
static bRC setPluginValue(PluginContext* ctx, pVariable var, void* value);
static bRC handlePluginEvent(PluginContext* ctx, bEvent* event, void* value);
static bRC startBackupFile(PluginContext* ctx, save_pkt* sp);
static bRC endBackupFile(PluginContext* ctx);
static bRC pluginIO(PluginContext* ctx, io_pkt* io);
static bRC startRestoreFile(PluginContext* ctx, const char* cmd);
static bRC endRestoreFile(PluginContext* ctx);
static bRC createFile(PluginContext* ctx, restore_pkt* rp);
static bRC setFileAttributes(PluginContext* ctx, restore_pkt* rp);
static bRC checkFile(PluginContext* ctx, char* fname);

static bRC parse_plugin_definition(PluginContext* ctx, void* value);
static bRC end_restore_job(PluginContext* ctx, void* value);
static void CloseVdiDeviceset(struct plugin_ctx* p_ctx);
static bool adoReportError(PluginContext* ctx);

// Pointers to Bareos functions
static CoreFunctions* bareos_core_functions = NULL;
static PluginApiDefinition* bareos_plugin_interface_version = NULL;

// Plugin Information block
static PluginInformation pluginInfo
    = {sizeof(pluginInfo), FD_PLUGIN_INTERFACE_VERSION,
       FD_PLUGIN_MAGIC,    PLUGIN_LICENSE,
       PLUGIN_AUTHOR,      PLUGIN_DATE,
       PLUGIN_VERSION,     PLUGIN_DESCRIPTION,
       PLUGIN_USAGE};

// Plugin entry points for Bareos
static PluginFunctions pluginFuncs
    = {sizeof(pluginFuncs), FD_PLUGIN_INTERFACE_VERSION,

       /* Entry points into plugin */
       newPlugin,  /* new plugin instance */
       freePlugin, /* free plugin instance */
       getPluginValue, setPluginValue, handlePluginEvent, startBackupFile,
       endBackupFile, startRestoreFile, endRestoreFile, pluginIO, createFile,
       setFileAttributes, checkFile, nullptr, nullptr, nullptr, nullptr};

enum class Mode
{
  Backup,
  Restore,
};
// Plugin private context
struct plugin_ctx {
  int RestoreFD{};
  bool RestoreToFile{};
  bool DoNoRecovery{};
  bool ForceReplace{};
  bool RecoverAfterRestore{};
  char* plugin_options{};
  char* filename{};
  char* server_address{};
  char* instance{};
  char* database{};
  char* username{};
  char* password{};
  char* stopbeforemark{};
  char* stopatmark{};
  char* stopat{};
  char* ado_connect_string{};
  char* ado_query{};
  char* ado_errorstr{};
  uint32_t get_configuration_timeout{};
  wchar_t* vdsname{};
  int32_t backup_level{};
  IClientVirtualDeviceSet2* VDIDeviceSet{};
  IClientVirtualDevice* VDIDevice{};
  VDConfig VDIConfig{};
  bool AdoThreadStarted{};
  pthread_t ADOThread{};
  bool completion_support{};
  std::optional<Mode> mode{};
};

struct adoThreadContext {
  _ADOConnection* adoConnection;
  BSTR ado_connect_string;
  BSTR ado_query;
};

// This defines the arguments that the plugin parser understands.
enum plugin_argument_type
{
  argument_none,
  argument_server_address,
  argument_instance,
  argument_database,
  argument_username,
  argument_password,
  argument_norecovery,
  argument_replace,
  argument_recover_after_restore,
  argument_stopbeforemark,
  argument_stopatmark,
  argument_stopat,
  argument_getoptions,
  argument_get_configuration_timeout
};

struct plugin_argument {
  const char* name;
  enum plugin_argument_type type;
};

static plugin_argument plugin_arguments[]
    = {{"serveraddress", argument_server_address},
       {"instance", argument_instance},
       {"database", argument_database},
       {"username", argument_username},
       {"password", argument_password},
       {"norecovery", argument_norecovery},
       {"replace", argument_replace},
       {"recoverafterrestore", argument_recover_after_restore},
       {"stopbeforemark", argument_stopbeforemark},
       {"stopatmark", argument_stopatmark},
       {"stopat", argument_stopat},
       {"getconfigurationtimeout", argument_get_configuration_timeout},  // sec
       {NULL, argument_none}};

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
BAREOS_EXPORT bRC
loadPlugin(PluginApiDefinition* lbareos_plugin_interface_version,
           CoreFunctions* lbareos_core_functions,
           PluginInformation** plugin_information,
           PluginFunctions** plugin_functions)
{
  bareos_core_functions
      = lbareos_core_functions; /* set Bareos funct pointers */
  bareos_plugin_interface_version = lbareos_plugin_interface_version;
  *plugin_information = &pluginInfo; /* return pointer to our info */
  *plugin_functions = &pluginFuncs;  /* return pointer to our functions */

  return bRC_OK;
}

// External entry point to unload the plugin
BAREOS_EXPORT bRC unloadPlugin() { return bRC_OK; }

#ifdef __cplusplus
}
#endif

static const char* explain_hr(DWORD hr)
{
  // Return Value	Explanation
  // NOERROR	A command was fetched.
  // VD_E_CLOSE	The device has been closed by the server.
  // VD_E_TIMEOUT	No command was available and the time-out expired.
  // VD_E_ABORT	Either the client or the server has used the SignalAbort
  // to force a shutdown.
  static const std::map<int, const char*> VdiRetVals{
      {VD_E_CLOSE, "VD_E_CLOSE"},
      {VD_E_TIMEOUT, "VD_E_TIMEOUT"},
      {VD_E_ABORT, "VD_E_ABORT"},
      {NO_ERROR, "NO_ERROR"},
  };

  if (auto found = VdiRetVals.find(hr); found != VdiRetVals.end()) {
    return found->second;
  } else {
    return "unknown";
  }
}


/**
 * The following entry points are accessed through the function pointers we
 * supplied to Bareos. Each plugin type (dir, fd, sd) has its own set of entry
 * points that the plugin must define.
 *
 * Create a new instance of the plugin i.e. allocate our private storage
 */
static bRC newPlugin(PluginContext* ctx)
{
  HRESULT hr;
  plugin_ctx* p_ctx;

  // Initialize COM for this thread.
  hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
  if (!SUCCEEDED(hr)) { return bRC_Error; }

  if (!InitializeComSecurity()) { return bRC_Error; }

  p_ctx = new plugin_ctx{};
  if (!p_ctx) { return bRC_Error; }
  ctx->plugin_private_context = (void*)p_ctx; /* set our context pointer */

  // Only register the events we are really interested in.
  bareos_core_functions->registerBareosEvents(
      ctx, 8, bEventLevel, bEventRestoreCommand, bEventBackupCommand,
      bEventPluginCommand, bEventEndRestoreJob, bEventNewPluginOptions,
      bEventStartBackupJob, bEventStartRestoreJob);

  return bRC_OK;
}

// Free a plugin instance, i.e. release our private storage
static bRC freePlugin(PluginContext* ctx)
{
  plugin_ctx* p_ctx = (plugin_ctx*)ctx->plugin_private_context;
  if (!p_ctx) { return bRC_Error; }

  Dmsg(ctx, debuglevel, "mssqlvdi-fd: entering freePlugin\n");

  // Close any open VDI deviceset.
  CloseVdiDeviceset(p_ctx);

  // See if there is any error to report from the ADO layer.
  adoReportError(ctx);

  // Cleanup the context.
  if (p_ctx->plugin_options) { free(p_ctx->plugin_options); }

  if (p_ctx->filename) { free(p_ctx->filename); }

  if (p_ctx->server_address) { free(p_ctx->server_address); }

  if (p_ctx->instance) { free(p_ctx->instance); }

  if (p_ctx->database) { free(p_ctx->database); }

  if (p_ctx->username) { free(p_ctx->username); }

  if (p_ctx->password) { free(p_ctx->password); }

  if (p_ctx->stopbeforemark) { free(p_ctx->stopbeforemark); }

  if (p_ctx->stopatmark) { free(p_ctx->stopatmark); }

  if (p_ctx->stopat) { free(p_ctx->stopat); }

  if (p_ctx->vdsname) { free(p_ctx->vdsname); }

  if (p_ctx->ado_connect_string) { free(p_ctx->ado_connect_string); }

  if (p_ctx->ado_query) { free(p_ctx->ado_query); }

  delete p_ctx;
  p_ctx = NULL;

  // Tear down COM for this thread.
  CoUninitialize();

  Dmsg(ctx, debuglevel, "mssqlvdi-fd: leaving freePlugin\n");

  return bRC_OK;
}

// Return some plugin value (none defined)
static bRC getPluginValue(PluginContext*, pVariable, void*) { return bRC_OK; }

// Set a plugin value (none defined)
static bRC setPluginValue(PluginContext*, pVariable, void*) { return bRC_OK; }

// Handle an event that was generated in Bareos
static bRC handlePluginEvent(PluginContext* ctx, bEvent* event, void* value)
{
  bRC retval;
  plugin_ctx* p_ctx = (plugin_ctx*)ctx->plugin_private_context;

  if (!p_ctx) { return bRC_Error; }

  switch (event->eventType) {
    case bEventStartBackupJob: {
      p_ctx->mode = Mode::Backup;
      retval = bRC_OK;
    } break;
    case bEventStartRestoreJob: {
      p_ctx->mode = Mode::Restore;
      retval = bRC_OK;
    } break;
    case bEventLevel:
      p_ctx->backup_level = (int64_t)value;
      retval = bRC_OK;
      break;
    case bEventRestoreCommand:
      // Fall-through wanted
    case bEventBackupCommand:
      // Fall-through wanted
    case bEventPluginCommand:
      retval = parse_plugin_definition(ctx, value);
      break;
    case bEventNewPluginOptions:
      // Free any previous value.
      if (p_ctx->plugin_options) {
        free(p_ctx->plugin_options);
        p_ctx->plugin_options = NULL;
      }

      retval = parse_plugin_definition(ctx, value);

      // Save that we got a plugin override.
      p_ctx->plugin_options = strdup((char*)value);
      break;
    case bEventEndRestoreJob:
      retval = end_restore_job(ctx, value);
      break;
    default:
      Jmsg(ctx, M_FATAL, "mssqlvdi-fd: unknown event=%d\n", event->eventType);
      Dmsg(ctx, debuglevel, "mssqlvdi-fd: unknown event=%d\n",
           event->eventType);
      retval = bRC_Error;
      break;
  }

  return retval;
}

// Start the backup of a specific file
static bRC startBackupFile(PluginContext* ctx, save_pkt* sp)
{
  time_t now;
  PoolMem fname(PM_NAME);
  char dt[MAX_TIME_LENGTH];
  plugin_ctx* p_ctx = (plugin_ctx*)ctx->plugin_private_context;

  if (!p_ctx) { return bRC_Error; }

  // If no explicit instance name given use the DEFAULT_INSTANCE.
  if (!p_ctx->instance) { p_ctx->instance = strdup(DEFAULT_INSTANCE); }

  // If no explicit server address given use the DEFAULT_SERVER_ADDRESS.
  if (!p_ctx->server_address) {
    p_ctx->server_address = strdup(DEFAULT_SERVER_ADDRESS);
  }

  now = time(NULL);
  bstrftime(dt, sizeof(dt), now, "%Y%m%d-%H%M%S");

  switch (p_ctx->backup_level) {
    case L_FULL:
      Mmsg(fname, "/@MSSQL/%s/%s/db-%s-full.bak", p_ctx->instance,
           p_ctx->database, dt);
      break;
    case L_DIFFERENTIAL:
      Mmsg(fname, "/@MSSQL/%s/%s/db-%s-diff.bak", p_ctx->instance,
           p_ctx->database, dt);
      break;
    case L_INCREMENTAL:
      Mmsg(fname, "/@MSSQL/%s/%s/db-%s-log.trn", p_ctx->instance,
           p_ctx->database, dt);
      break;
    default:
      Jmsg(ctx, M_FATAL, "Unsuported backup level (%c).\n",
           p_ctx->backup_level);
      Dmsg(ctx, debuglevel, "Unsuported backup level (%c).\n",
           p_ctx->backup_level);
      return bRC_Error;
  }

  p_ctx->filename = strdup(fname.c_str());
  Dmsg(ctx, debuglevel, "startBackupFile: Generated filename %s\n",
       p_ctx->filename);

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

// Done with backup of this file
static bRC endBackupFile(PluginContext*)
{
  /* We would return bRC_More if we wanted startBackupFile to be called again to
   * backup another file */
  return bRC_OK;
}

// Strip any backslashes in the string.
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
 * Parse a boolean value e.g. check if its yes or true anything else translates
 * to false.
 */
static inline bool ParseBoolean(const char* argument_value)
{
  if (Bstrcasecmp(argument_value, "yes")
      || Bstrcasecmp(argument_value, "true")) {
    return true;
  } else {
    return false;
  }
}

// Parse a uint32 value
static inline bool ParseUnsignedInt(const char* argument_value,
                                    uint32_t& value_out)
{
  try {
    value_out = std::stoul(argument_value);
    return true;
  } catch (std::invalid_argument& e) {
    return false;
  }
}


// Only set destination to value when it has no previous setting.
static inline void SetStringIfNull(char** destination, char* value)
{
  if (!*destination) {
    *destination = strdup(value);
    StripBackSlashes(*destination);
  }
}

// Always set destination to value and clean any previous one.
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
 * mssqlvdi:instance=<instance>:database=<databasename>:
 */
static bRC parse_plugin_definition(PluginContext* ctx, void* value)
{
  int i;
  bool keep_existing;
  char *plugin_definition, *bp, *argument, *argument_value;
  plugin_ctx* p_ctx = (plugin_ctx*)ctx->plugin_private_context;

  if (!p_ctx || !value) { return bRC_Error; }

  keep_existing = (p_ctx->plugin_options) ? true : false;

  /* Parse the plugin definition.
   * Make a private copy of the whole string. */
  plugin_definition = strdup((char*)value);

  bp = strchr(plugin_definition, ':');
  if (!bp) {
    Jmsg(ctx, M_FATAL, "Illegal plugin definition %s\n", plugin_definition);
    Dmsg(ctx, debuglevel, "Illegal plugin definition %s\n", plugin_definition);
    goto bail_out;
  }

  // Skip the first ':'
  bp++;
  while (bp) {
    if (strlen(bp) == 0) { break; }

    /* Each argument is in the form:
     *    <argument> = <argument_value>
     *
     * So we setup the right pointers here, argument to the beginning
     * of the argument, argument_value to the beginning of the argument_value.
     */
    argument = bp;
    argument_value = strchr(bp, '=');
    if (!argument_value) {
      Jmsg(ctx, M_FATAL, "Illegal argument %s without value\n", argument);
      Dmsg(ctx, debuglevel, "Illegal argument %s without value\n", argument);
      goto bail_out;
    }
    *argument_value++ = '\0';

    // See if there are more arguments and setup for the next run.
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
        bool* bool_destination = NULL;
        uint32_t* uint32_destination = NULL;

        switch (plugin_arguments[i].type) {
          case argument_server_address:
            str_destination = &p_ctx->server_address;
            break;
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
          case argument_get_configuration_timeout:
            uint32_destination = &p_ctx->get_configuration_timeout;
            break;
          default:
            break;
        }

        // Keep the first value, ignore any next setting.
        if (str_destination) {
          if (keep_existing) {
            SetStringIfNull(str_destination, argument_value);
          } else {
            SetString(str_destination, argument_value);
          }
        }

        // Set any boolean variable.
        if (bool_destination) {
          *bool_destination = ParseBoolean(argument_value);
        }

        if (uint32_destination) {
          if (!ParseUnsignedInt(argument_value, *uint32_destination)) {
            Jmsg(ctx, M_FATAL,
                 "Wrong value for argument %s (%s) in plugin definition, "
                 "should be integer number of seconds\n",
                 argument, argument_value);
          }
        }

        // When we have a match break the loop.
        break;
      }
    }

    // Got an invalid keyword ?
    if (!plugin_arguments[i].name) {
      Jmsg(ctx, M_FATAL,
           "Illegal argument %s with value %s in plugin definition\n", argument,
           argument_value);
      Dmsg(ctx, debuglevel,
           "Illegal argument %s with value %s in plugin definition\n", argument,
           argument_value);
      goto bail_out;
    }
  }

  free(plugin_definition);
  return bRC_OK;

bail_out:
  free(plugin_definition);
  return bRC_Error;
}

// Close the VDI deviceset if is is opened.
static void CloseVdiDeviceset(plugin_ctx* p_ctx)
{
  // Close VDI Device.
  if (p_ctx->VDIDevice) {
    p_ctx->VDIDevice->Release();
    p_ctx->VDIDevice = NULL;
  }

  // Close VDI DeviceSet.
  if (p_ctx->VDIDeviceSet) {
    p_ctx->VDIDeviceSet->Close();
    p_ctx->VDIDeviceSet->Release();
    p_ctx->VDIDeviceSet = NULL;
  }

  // Cancel the started database thread.
  if (p_ctx->AdoThreadStarted) {
    if (!pthread_equal(p_ctx->ADOThread, pthread_self())) {
      pthread_cancel(p_ctx->ADOThread);
    }
  }
}

// Generic COM error reporting function.
static void comReportError(PluginContext* ctx, HRESULT hrErr)
{
  IErrorInfo* pErrorInfo;
  BSTR pSource = NULL;
  BSTR pDescription = NULL;
  HRESULT hr;
  char *source, *description;

  // See if there is anything to report.
  hr = GetErrorInfo(0, &pErrorInfo);
  if (hr == S_FALSE) { return; }

  // Get the description of the COM error.
  hr = pErrorInfo->GetDescription(&pDescription);
  if (!SUCCEEDED(hr)) {
    Dmsg(ctx, debuglevel, "mssqlvdi-fd: GetDescription failed\n");
    pErrorInfo->Release();
    return;
  }

  // Get the source of the COM error.
  hr = pErrorInfo->GetSource(&pSource);
  if (!SUCCEEDED(hr)) {
    Dmsg(ctx, debuglevel, "mssqlvdi-fd: GetSource failed\n");
    SysFreeString(pDescription);
    pErrorInfo->Release();
    return;
  }

  // Convert windows BSTR to normal strings.
  source = BSTR_2_str(pSource);
  description = BSTR_2_str(pDescription);
  if (source && description) {
    Jmsg(ctx, M_FATAL, "%s(0x%08lX): %s\n", source, hrErr, description);
    Dmsg(ctx, debuglevel, "%s(0x%08lX): %s\n", source, hrErr, description);
  } else {
    Dmsg(ctx, debuglevel, "mssqlvdi-fd: could not print error\n");
  }

  if (source) { free(source); }

  if (description) { free(description); }

  /* Generic cleanup (free the description and source as those are returned in
   * dynamically allocated memory by the COM routines.) */
  SysFreeString(pSource);
  SysFreeString(pDescription);

  pErrorInfo->Release();
}

// Retrieve errors from ADO Connection.
static bool adoGetErrors(PluginContext* ctx,
                         _ADOConnection* adoConnection,
                         PoolMem& ado_errorstr)
{
  HRESULT hr;
  ADOErrors* adoErrors;
  long errCount;

  // Get any errors that are reported.
  hr = adoConnection->get_Errors(&adoErrors);
  if (!SUCCEEDED(hr)) {
    comReportError(ctx, hr);
    goto bail_out;
  }

  // See how many errors there are.
  hr = adoErrors->get_Count(&errCount);
  if (!SUCCEEDED(hr)) {
    comReportError(ctx, hr);
    adoErrors->Release();
    goto bail_out;
  }

  // Loop over all error and append them into one big error string.
  PmStrcpy(ado_errorstr, "");
  for (long i = 0; i < errCount; i++) {
    ADOError* adoError;
    BSTR pDescription = NULL;
    char* description;

    hr = adoErrors->get_Item(_variant_t(i), &adoError);
    if (!SUCCEEDED(hr)) {
      comReportError(ctx, hr);
      adoErrors->Release();
      goto bail_out;
    }

    hr = adoError->get_Description(&pDescription);
    if (!SUCCEEDED(hr)) {
      comReportError(ctx, hr);
      adoError->Release();
      adoErrors->Release();
      goto bail_out;
    }

    description = BSTR_2_str(pDescription);
    if (description) {
      Dmsg(ctx, debuglevel, "adoGetErrors: ADO error %s\n", description);
      PmStrcat(ado_errorstr, description);
      PmStrcat(ado_errorstr, "\n");
      free(description);
    }

    SysFreeString(pDescription);
    adoError->Release();
  }

  // Generic cleanup.
  adoErrors->Clear();
  adoErrors->Release();

  return true;

bail_out:
  return false;
}

// Print errors (when available) collected by adoThreadSetError function.
static bool adoReportError(PluginContext* ctx)
{
  plugin_ctx* p_ctx = (plugin_ctx*)ctx->plugin_private_context;

  if (p_ctx->ado_errorstr) {
    Jmsg(ctx, M_FATAL, "%s\n", p_ctx->ado_errorstr);
    Dmsg(ctx, debuglevel, "%s\n", p_ctx->ado_errorstr);

    free(p_ctx->ado_errorstr);
    p_ctx->ado_errorstr = NULL;

    return true;
  } else {
    Dmsg(ctx, debuglevel, "Tried reporting error, but none were available\n");
  }

  return false;
}

/**
 * Retrieve errors from ADO Connection when running the query in a separate
 * thread.
 */
static void adoThreadSetError(PluginContext* ctx, _ADOConnection* adoConnection)
{
  int cancel_type;
  PoolMem ado_errorstr(PM_NAME);
  plugin_ctx* p_ctx = (plugin_ctx*)ctx->plugin_private_context;

  if (p_ctx->ado_errorstr) { return; }

  // Set the threads cancellation type to defered.
  pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, &cancel_type);

  if (!adoGetErrors(ctx, adoConnection, ado_errorstr)) { goto bail_out; }

  /* Keep the errors in a buffer these will be printed by the adoReportError
   * function. */
  p_ctx->ado_errorstr = strdup(ado_errorstr.c_str());

bail_out:
  // Restore the threads cancellation type.
  pthread_setcanceltype(cancel_type, NULL);

  return;
}

// Cleanup function called on thread destroy.
static void adoCleanupThread(void* data)
{
  adoThreadContext* ado_ctx;

  ado_ctx = (adoThreadContext*)data;
  if (!data) { return; }

  // Generic cleanup.
  if (ado_ctx->ado_connect_string) {
    SysFreeString(ado_ctx->ado_connect_string);
  }

  if (ado_ctx->ado_query) { SysFreeString(ado_ctx->ado_query); }

  if (ado_ctx->adoConnection) {
    LONG adoState;

    ado_ctx->adoConnection->get_State(&adoState);
    if (adoState & adStateExecuting) { ado_ctx->adoConnection->Cancel(); }

    if (adoState & adStateOpen) { ado_ctx->adoConnection->Close(); }

    ado_ctx->adoConnection->Release();
  }

  // Tear down COM for this thread.
  CoUninitialize();
}

/**
 * Run a separate thread that connects to the database server and
 * controls the backup or restore. When we close the VDI device we
 * also tear down this database control thread.
 */
static void* adoThread(void* data)
{
  HRESULT hr;
  adoThreadContext ado_ctx;
  PluginContext* ctx = (PluginContext*)data;
  plugin_ctx* p_ctx = (struct plugin_ctx*)ctx->plugin_private_context;

  memset(&ado_ctx, 0, sizeof(ado_ctx));

  // When we get canceled make sure we run the cleanup function.
  pthread_cleanup_push(adoCleanupThread, &ado_ctx);

  // Initialize COM for this thread.
  hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
  if (!SUCCEEDED(hr)) { return NULL; }

  // Create a COM instance for an ActiveX® Data Objects connection.
  hr = CoCreateInstance(CLSID_CADOConnection, NULL, CLSCTX_INPROC_SERVER,
                        IID_IADOConnection, (void**)&ado_ctx.adoConnection);
  if (!SUCCEEDED(hr)) { goto bail_out; }

  /* Make sure the connection doesn't timeout.
   * Default timeout is not long enough most of the time
   * for the backup or restore to finish and when it times
   * out it will abort the action it was performing. */
  hr = ado_ctx.adoConnection->put_CommandTimeout(0);
  if (!SUCCEEDED(hr)) {
    adoThreadSetError(ctx, ado_ctx.adoConnection);
    goto bail_out;
  }

  /* Open a connection to the database server with the defined connection
   * string. */
  ado_ctx.ado_connect_string = str_2_BSTR(p_ctx->ado_connect_string);
  hr = ado_ctx.adoConnection->Open(ado_ctx.ado_connect_string);
  if (!SUCCEEDED(hr)) {
    adoThreadSetError(ctx, ado_ctx.adoConnection);
    goto bail_out;
  }

  // Execute the backup or restore command.
  ado_ctx.ado_query = str_2_BSTR(p_ctx->ado_query);
  hr = ado_ctx.adoConnection->Execute(ado_ctx.ado_query, NULL,
                                      adExecuteNoRecords, NULL);
  if (!SUCCEEDED(hr)) {
    adoThreadSetError(ctx, ado_ctx.adoConnection);
    goto bail_out;
  }

bail_out:
  // Run the thread cleanup.
  pthread_cleanup_pop(1);

  return NULL;
}

// Create a connection string for connecting to the master database.
static void SetAdoConnectString(PluginContext* ctx)
{
  PoolMem ado_connect_string(PM_NAME);
  plugin_ctx* p_ctx = (plugin_ctx*)ctx->plugin_private_context;

  // If no explicit instance name given usedthe DEFAULT_INSTANCE name.
  if (!p_ctx->instance) { p_ctx->instance = strdup(DEFAULT_INSTANCE); }

  // If no explicit server address given use the DEFAULT_SERVER_ADDRESS.
  if (!p_ctx->server_address) {
    p_ctx->server_address = strdup(DEFAULT_SERVER_ADDRESS);
  }

  if (Bstrcasecmp(p_ctx->instance, DEFAULT_INSTANCE)) {
    Mmsg(ado_connect_string,
         "Provider=MSOLEDBSQL.1;Data Source=%s;Initial Catalog=master",
         p_ctx->server_address);
  } else {
    Mmsg(ado_connect_string,
         "Provider=MSOLEDBSQL.1;Data Source=%s\\%s;Initial Catalog=master",
         p_ctx->server_address, p_ctx->instance);
  }

  // See if we need to use a username/password or a trusted connection.
  if (p_ctx->username && p_ctx->password) {
    PoolMem temp(PM_NAME);

    Mmsg(temp, ";User Id=%s;Password=%s;", p_ctx->username, p_ctx->password);
    PmStrcat(ado_connect_string, temp.c_str());
  } else {
    PmStrcat(ado_connect_string, ";Integrated Security=SSPI;");
  }

  Dmsg(ctx, debuglevel, "SetAdoConnectString: ADO Connect String '%s'\n",
       ado_connect_string.c_str());

  if (p_ctx->ado_connect_string) { free(p_ctx->ado_connect_string); }
  p_ctx->ado_connect_string = strdup(ado_connect_string.c_str());
}

/**
 * Generate a valid connect string and the backup command we should execute
 * in the separate database controling thread.
 */
static inline void PerformAdoBackup(PluginContext* ctx)
{
  plugin_ctx* p_ctx = (plugin_ctx*)ctx->plugin_private_context;
  PoolMem ado_connect_string(PM_NAME), ado_query(PM_NAME);
  POOLMEM* vdsname;

  SetAdoConnectString(ctx);

  vdsname = GetPoolMemory(PM_NAME);
  wchar_2_UTF8(vdsname, p_ctx->vdsname);

  switch (p_ctx->backup_level) {
    case L_INCREMENTAL:
      Mmsg(ado_query,
           "BACKUP LOG [%s] TO VIRTUAL_DEVICE='%s' WITH BLOCKSIZE=%d, "
           "BUFFERCOUNT=%d, MAXTRANSFERSIZE=%d",
           p_ctx->database, vdsname, DEFAULT_BLOCKSIZE, DEFAULT_BUFFERS,
           DEFAULT_BLOCKSIZE);
      break;
    case L_DIFFERENTIAL:
      Mmsg(ado_query,
           "BACKUP DATABASE [%s] TO VIRTUAL_DEVICE='%s' WITH DIFFERENTIAL, "
           "BLOCKSIZE=%d, BUFFERCOUNT=%d, MAXTRANSFERSIZE=%d",
           p_ctx->database, vdsname, DEFAULT_BLOCKSIZE, DEFAULT_BUFFERS,
           DEFAULT_BLOCKSIZE);
      break;
    default:
      Mmsg(ado_query,
           "BACKUP DATABASE [%s] TO VIRTUAL_DEVICE='%s' WITH BLOCKSIZE=%d, "
           "BUFFERCOUNT=%d, MAXTRANSFERSIZE=%d",
           p_ctx->database, vdsname, DEFAULT_BLOCKSIZE, DEFAULT_BUFFERS,
           DEFAULT_BLOCKSIZE);
      break;
  }

  Dmsg(ctx, debuglevel, "PerformAdoBackup: ADO Query '%s'\n",
       ado_query.c_str());

  p_ctx->ado_query = strdup(ado_query.c_str());
  FreePoolMemory(vdsname);
}

/**
 * Generate a valid connect string and the restore command we should execute
 * in the separate database controlling thread.
 */
static inline void perform_aDoRestore(PluginContext* ctx)
{
  PoolMem ado_query(PM_NAME), temp(PM_NAME);
  POOLMEM* vdsname;
  plugin_ctx* p_ctx = (plugin_ctx*)ctx->plugin_private_context;

  // If no explicit instance name given use the DEFAULT_INSTANCE name.
  if (!p_ctx->instance) { p_ctx->instance = strdup(DEFAULT_INSTANCE); }

  SetAdoConnectString(ctx);

  vdsname = GetPoolMemory(PM_NAME);
  wchar_2_UTF8(vdsname, p_ctx->vdsname);

  switch (p_ctx->backup_level) {
    case L_INCREMENTAL:
      Mmsg(ado_query,
           "RESTORE LOG [%s] FROM VIRTUAL_DEVICE='%s' WITH BLOCKSIZE=%d, "
           "BUFFERCOUNT=%d, MAXTRANSFERSIZE=%d, %s",
           p_ctx->database, vdsname, DEFAULT_BLOCKSIZE, DEFAULT_BUFFERS,
           DEFAULT_BLOCKSIZE, p_ctx->DoNoRecovery ? "NORECOVERY" : "RECOVERY");
      break;
    default:
      Mmsg(ado_query,
           "RESTORE DATABASE [%s] FROM VIRTUAL_DEVICE='%s' WITH BLOCKSIZE=%d, "
           "BUFFERCOUNT=%d, MAXTRANSFERSIZE=%d, %s",
           p_ctx->database, vdsname, DEFAULT_BLOCKSIZE, DEFAULT_BUFFERS,
           DEFAULT_BLOCKSIZE, p_ctx->DoNoRecovery ? "NORECOVERY" : "RECOVERY");
      break;
  }

  // See if we need to insert any stopbeforemark arguments.
  if (p_ctx->stopbeforemark) {
    Mmsg(temp, ", STOPBEFOREMARK = '%s'", p_ctx->stopbeforemark);
    PmStrcat(ado_query, temp.c_str());
  }

  // See if we need to insert any stopatmark arguments.
  if (p_ctx->stopatmark) {
    Mmsg(temp, ", STOPATMARK = '%s'", p_ctx->stopatmark);
    PmStrcat(ado_query, temp.c_str());
  }

  // See if we need to insert any stopat arguments.
  if (p_ctx->stopat) {
    Mmsg(temp, ", STOPAT = '%s'", p_ctx->stopat);
    PmStrcat(ado_query, temp.c_str());
  }

  // See if we need to insert the REPLACE option.
  if (p_ctx->ForceReplace) { PmStrcat(ado_query, ", REPLACE"); }

  Dmsg(ctx, debuglevel, "perform_aDoRestore: ADO Query '%s'\n",
       ado_query.c_str());

  p_ctx->ado_query = strdup(ado_query.c_str());
  FreePoolMemory(vdsname);
}

// Run a query not in a separate thread.
static inline bool RunAdoQuery(PluginContext* ctx, const char* query)
{
  bool retval = false;
  HRESULT hr;
  BSTR ado_connect_string = NULL;
  BSTR ado_query = NULL;
  _ADOConnection* adoConnection = NULL;
  plugin_ctx* p_ctx = (plugin_ctx*)ctx->plugin_private_context;

  Dmsg(ctx, debuglevel, "RunAdoQuery: ADO Query '%s'\n", query);

  // Create a COM instance for an ActiveX® Data Objects connection.
  hr = CoCreateInstance(CLSID_CADOConnection, NULL, CLSCTX_INPROC_SERVER,
                        IID_IADOConnection, (void**)&adoConnection);
  if (!SUCCEEDED(hr)) { goto cleanup; }

  /* Open a connection to the database server with the defined connection
   * string. */
  ado_connect_string = str_2_BSTR(p_ctx->ado_connect_string);
  hr = adoConnection->Open(ado_connect_string);
  if (!SUCCEEDED(hr)) {
    PoolMem ado_errorstr(PM_NAME);

    adoGetErrors(ctx, adoConnection, ado_errorstr);
    Jmsg(ctx, M_FATAL, "Failed to connect to database, %s\n",
         ado_errorstr.c_str());
    Dmsg(ctx, debuglevel, "Failed to connect to database, %s\n",
         ado_errorstr.c_str());
    goto cleanup;
  }

  // Perform the query.
  ado_query = str_2_BSTR(query);
  hr = adoConnection->Execute(ado_query, NULL, adExecuteNoRecords, NULL);
  if (!SUCCEEDED(hr)) {
    PoolMem ado_errorstr(PM_NAME);

    adoGetErrors(ctx, adoConnection, ado_errorstr);
    Jmsg(ctx, M_FATAL, "Failed to execute query %s on database, %s\n", query,
         ado_errorstr.c_str());
    Dmsg(ctx, debuglevel, "Failed to execute query %s on database, %s\n", query,
         ado_errorstr.c_str());
    goto cleanup;
  }

  retval = true;

cleanup:
  if (ado_connect_string) { SysFreeString(ado_connect_string); }

  if (ado_query) { SysFreeString(ado_query); }

  if (adoConnection) {
    LONG adoState;

    adoConnection->get_State(&adoState);
    if (adoState & adStateExecuting) { adoConnection->Cancel(); }

    if (adoState & adStateOpen) { adoConnection->Close(); }

    adoConnection->Release();
  }

  return retval;
}

// Automatically recover the database at the end of the whole restore process.
static inline bool PerformAdoRecover(PluginContext* ctx)
{
  PoolMem recovery_query(PM_NAME);
  plugin_ctx* p_ctx = (plugin_ctx*)ctx->plugin_private_context;

  SetAdoConnectString(ctx);
  Mmsg(recovery_query, "RESTORE DATABASE [%s] WITH RECOVERY", p_ctx->database);

  return RunAdoQuery(ctx, recovery_query.c_str());
}

// Setup a VDI device for performing a backup or restore operation.
static inline bool SetupVdiDevice(PluginContext* ctx, io_pkt* io)
{
  int status;
  HRESULT hr = NOERROR;
  GUID vdsId;
  plugin_ctx* p_ctx = (plugin_ctx*)ctx->plugin_private_context;

  if ((p_ctx->username && !p_ctx->password)
      || (!p_ctx->username && p_ctx->password)) {
    Jmsg(ctx, M_FATAL,
         "Illegal plugin definition when using username or password define "
         "both\n");
    return false;
  }

  if (!p_ctx->mode.has_value()) {
    Jmsg(ctx, M_FATAL, "No Backup/Restore Mode was specified\n");
    return false;
  }

  if (io->flags & (O_CREAT | O_WRONLY)) {
    Dmsg(ctx, debuglevel, "mssqlvdi-fd: creating restore vdi device\n");
    if (p_ctx->mode.value() != Mode::Restore) {
      Jmsg(ctx, M_FATAL, "Wrong [Backup]/Restore Mode was specified\n");
      return false;
    }
  } else {
    Dmsg(ctx, debuglevel, "mssqlvdi-fd: creating backup vdi device\n");
    if (p_ctx->mode.value() != Mode::Backup) {
      Jmsg(ctx, M_FATAL, "Wrong Backup/[Restore] Mode was specified\n");
      return false;
    }
  }

  CoCreateGuid(&vdsId);
  p_ctx->vdsname = (wchar_t*)malloc((VDS_NAME_LENGTH * sizeof(wchar_t)) + 2);
  StringFromGUID2(vdsId, p_ctx->vdsname, VDS_NAME_LENGTH);

  // Get a handle to the device set.
  hr = CoCreateInstance(CLSID_MSSQL_ClientVirtualDeviceSet, NULL,
                        CLSCTX_INPROC_SERVER, IID_IClientVirtualDeviceSet2,
                        (void**)&p_ctx->VDIDeviceSet);
  if (!SUCCEEDED(hr)) {
    Dmsg(ctx, debuglevel, "mssqlvdi-fd: CoCreateInstance failed\n");
    comReportError(ctx, hr);
    return false;
  }

  // Setup the VDI configuration.
  memset(&p_ctx->VDIConfig, 0, sizeof(p_ctx->VDIConfig));
  p_ctx->VDIConfig.deviceCount = 1;
  p_ctx->VDIConfig.features = VDF_LikePipe;
  if (p_ctx->mode.value() == Mode::Backup) {
    // for proper filestream backup support we need to distinguish between
    // a VDC_Flush command and the end of the backup.  We do this
    // by relying on VDC_Complete if its available.
    // If its not available, we are not able to correctly handle a VDC_Flush
    // in the middle of a backup.
    p_ctx->VDIConfig.features |= VDF_RequestComplete;
  }

  // Create the VDI device set.
  if (Bstrcasecmp(p_ctx->instance, DEFAULT_INSTANCE)) {
    hr = p_ctx->VDIDeviceSet->CreateEx(NULL, p_ctx->vdsname, &p_ctx->VDIConfig);
  } else {
    POOLMEM* instance_name;

    instance_name = GetPoolMemory(PM_NAME);
    UTF8_2_wchar(instance_name, p_ctx->instance);
    hr = p_ctx->VDIDeviceSet->CreateEx((LPCWSTR)instance_name, p_ctx->vdsname,
                                       &p_ctx->VDIConfig);
    FreePoolMemory(instance_name);
  }
  if (!SUCCEEDED(hr)) {
    Dmsg(ctx, debuglevel, "mssqlvdi-fd: VDIDeviceSet::CreateEx failed\n");
    comReportError(ctx, hr);
    return false;
  }

  // Setup the right backup or restore cmdline and connect info.
  switch (p_ctx->mode.value()) {
    case Mode::Restore: {
      perform_aDoRestore(ctx);
    } break;
    case Mode::Backup: {
      PerformAdoBackup(ctx);
    } break;
  }

  /* Ask the database server to start a backup or restore via another thread.
   * We create a new thread that handles the connection to the database. */
  status = pthread_create(&p_ctx->ADOThread, NULL, adoThread, (void*)ctx);
  if (status != 0) {
    Dmsg(ctx, debuglevel, "mssqlvdi-fd: pthread_create failed\n");
    return false;
  }

  /* Track that we have started the thread and as such need to kill it when
   * we perform a close of the VDI device. */
  p_ctx->AdoThreadStarted = true;

  // GetConfiguration
  {
    uint32_t timeout = p_ctx->get_configuration_timeout == 0
                           ? VDI_DEFAULT_WAIT
                           : p_ctx->get_configuration_timeout * 1000;  // ms

    Jmsg(ctx, M_INFO, "Calling GetConfiguration with a timeout of %d sec\n.",
         timeout / 1000);

    hr = p_ctx->VDIDeviceSet->GetConfiguration(timeout, &p_ctx->VDIConfig);

    const char* err = "";
    bool success = false;

    switch (hr) {
      case NOERROR:
        err = "NOERROR";
        success = true;
        break;
      case VD_E_ABORT:
        err = "VD_E_ABORT";
        break;
      case VD_E_TIMEOUT:
        err = "VD_E_TIMEOUT, you may want to increase getconfigurationtimeout";
        break;
      default:
        err = "unspecific error";
        break;
    }

    std::array<char, 300> error_msg;
    const char* fmt
        = "mssqlvdi-fd: IClientVirtualDeviceSet2::GetConfiguration "
          "%s: %0#x (%s)\n";

    if (!(p_ctx->VDIConfig.features & VDF_CompleteEnabled)) {
      if (p_ctx->mode.value() == Mode::Backup) {
        Jmsg(ctx, M_INFO,
             "VDI does not support VDC_Complete."
             " Backing up Filestreams are not supported\n.");
      }
      p_ctx->completion_support = false;
    } else {
      p_ctx->completion_support = true;
    }

    Dmsg(ctx, debuglevel, "completion_support = %s\n",
         p_ctx->completion_support ? "Yes" : "No");

    if (success) {
      snprintf(error_msg.data(), error_msg.size(), fmt, "successful",
               static_cast<unsigned int>(hr), err);
      Jmsg(ctx, M_INFO, "%s", error_msg.data());
      Dmsg(ctx, debuglevel, "%s", error_msg.data());
    } else {
      sprintf(error_msg.data(), fmt, "failed", static_cast<unsigned int>(hr),
              err);
      Jmsg(ctx, M_FATAL, "%s", error_msg.data());
      Dmsg(ctx, debuglevel, "%s", error_msg.data());
      goto bail_out;
    }
  }  // GetConfiguration

  hr = p_ctx->VDIDeviceSet->OpenDevice(p_ctx->vdsname, &p_ctx->VDIDevice);
  if (!SUCCEEDED(hr)) {
    char vdsname[VDS_NAME_LENGTH + 1];

    sprintf_s(vdsname, sizeof(vdsname), "%S", p_ctx->vdsname);
    Jmsg(ctx, M_FATAL,
         "mssqlvdi-fd: IClientVirtualDeviceSet2::OpenDevice(%s)\n", vdsname);
    Dmsg(ctx, debuglevel,
         "mssqlvdi-fd: IClientVirtualDeviceSet2::OpenDevice(%s)\n", vdsname);
    goto bail_out;
  }

  io->status = 0;
  io->io_errno = 0;
  io->lerror = 0;
  io->win32 = false;

  return true;

bail_out:
  // Report any COM errors.
  comReportError(ctx, hr);

  // Wait for the adoThread to exit.
  if (p_ctx->AdoThreadStarted) {
    if (!pthread_equal(p_ctx->ADOThread, pthread_self())) {
      pthread_cancel(p_ctx->ADOThread);
      pthread_join(p_ctx->ADOThread, NULL);
      p_ctx->AdoThreadStarted = false;
    }
  }

  return false;
}

// Perform an I/O operation to a file as part of a restore.
static inline bool PerformFileIo(PluginContext* ctx,
                                 io_pkt* io,
                                 DWORD* completionCode)
{
  plugin_ctx* p_ctx = (plugin_ctx*)ctx->plugin_private_context;

  switch (io->func) {
    case IO_OPEN:
      if (p_ctx->RestoreFD == -1) {
        io->status = 0;
        p_ctx->RestoreFD = open(io->fname, io->flags, io->mode);
        if (p_ctx->RestoreFD < 0) { goto bail_out; }
      } else {
        *completionCode = ERROR_BAD_ENVIRONMENT;
        goto bail_out;
      }
      break;
    case IO_READ:
      if (p_ctx->RestoreFD != -1) {
        io->status = read(p_ctx->RestoreFD, io->buf, io->count);
      } else {
        *completionCode = ERROR_BAD_ENVIRONMENT;
        goto bail_out;
      }
      break;
    case IO_WRITE:
      if (p_ctx->RestoreFD != -1) {
        io->status = write(p_ctx->RestoreFD, io->buf, io->count);
      } else {
        *completionCode = ERROR_BAD_ENVIRONMENT;
        goto bail_out;
      }
      break;
    case IO_CLOSE:
      if (p_ctx->RestoreFD != -1) {
        io->status = 0;
        close(p_ctx->RestoreFD);
        p_ctx->RestoreFD = -1;
      } else {
        *completionCode = ERROR_BAD_ENVIRONMENT;
        goto bail_out;
      }
      break;
    case IO_SEEK:
      if (p_ctx->RestoreFD != -1) {
        io->status = lseek(p_ctx->RestoreFD, io->offset, io->whence);
      } else {
        *completionCode = ERROR_BAD_ENVIRONMENT;
        goto bail_out;
      }
      break;
    default:
      goto bail_out;
  }

  if (io->status < 0) { goto bail_out; }

  io->io_errno = 0;
  io->lerror = 0;
  io->win32 = false;
  *completionCode = ERROR_SUCCESS;

  return true;

bail_out:
  return false;
}

const char* command_name(DWORD commandCode)
{
  switch (commandCode) {
    case VDC_Read: {
      return "read";
    }
    case VDC_Write: {
      return "write";
    }
    case VDC_ClearError: {
      return "clear-error";
    }
    case VDC_Rewind: {
      return "rewind";
    }
    case VDC_WriteMark: {
      return "write-mark";
    }
    case VDC_SkipMarks: {
      return "skip-marks";
    }
    case VDC_SkipBlocks: {
      return "skip-blocks";
    }
    case VDC_Load: {
      return "load";
    }
    case VDC_GetPosition: {
      return "get-position";
    }
    case VDC_SetPosition: {
      return "set-position";
    }
    case VDC_Discard: {
      return "discard";
    }
    case VDC_Flush: {
      return "flush";
    }
    case VDC_Snapshot: {
      return "snapshot";
    }
    case VDC_MountSnapshot: {
      return "mount-snapshot";
    }
    case VDC_PrepareToFreeze: {
      return "prepare-to-freeze";
    }
    case VDC_FileInfoBegin: {
      return "file-info-begin";
    }
    case VDC_FileInfoEnd: {
      return "file-info-end";
    }
    case VDC_Complete: {
      return "complete";
    }
    default: {
      return "unknown";
    }
  }
}

// Perform an I/O operation to a virtual device as part of a backup or restore.
static inline bool PerformVdiIo(PluginContext* ctx,
                                io_pkt* io,
                                DWORD* completionCode)
{
  HRESULT hr = NOERROR;
  VDC_Command* cmd;
  plugin_ctx* p_ctx = (plugin_ctx*)ctx->plugin_private_context;

  bool quit = false;
  while (!quit) {
    // See what command is available on the VDIDevice.
    hr = p_ctx->VDIDevice->GetCommand(VDI_WAIT_TIMEOUT, &cmd);
    if (!SUCCEEDED(hr)) {
      auto* explanation = explain_hr(hr);
      Jmsg(ctx, M_ERROR,
           "mssqlvdi-fd: IClientVirtualDevice::GetCommand: Err=%s (0x%08lX)\n",
           explanation, hr);
      Dmsg(ctx, debuglevel,
           "mssqlvdi-fd: IClientVirtualDevice::GetCommand: Err=%s (0x%08lX)\n",
           explanation, hr);
      goto bail_out;
    }

    Dmsg(ctx, debuglevel, "mssqlvdi-fd: Command: %d:%s (size=%d)\n",
         cmd->commandCode, command_name(cmd->commandCode), cmd->size);

    switch (cmd->commandCode) {
      case VDC_Read:
        Dmsg(ctx, debuglevel, "mssqlvdi-fd: Read: %d bytes\n", cmd->size);
        /* Make sure the write to the VDIDevice will fit e.g. not a too big IO
         * and that we are currently writing to the device. */
        if ((DWORD)io->count > cmd->size || io->func != IO_WRITE) {
          *completionCode = ERROR_BAD_ENVIRONMENT;
          goto bail_out;
        } else {
          memcpy(cmd->buffer, io->buf, io->count);
          io->status = io->count;
          *completionCode = ERROR_SUCCESS;
        }
        quit = true;
        break;
      case VDC_Write:
        Dmsg(ctx, debuglevel, "mssqlvdi-fd: Write: %d bytes\n", cmd->size);
        /* Make sure the read from the VDIDevice will fit e.g. not a too big IO
         * and that we are currently reading from the device. */
        if (cmd->size > (DWORD)io->count || io->func != IO_READ) {
          *completionCode = ERROR_BAD_ENVIRONMENT;
          goto bail_out;
        } else {
          memcpy(io->buf, cmd->buffer, cmd->size);
          io->status = cmd->size;
          *completionCode = ERROR_SUCCESS;
        }
        quit = true;
        break;
      case VDC_Flush:
        Dmsg(ctx, debuglevel, "mssqlvdi-fd: Flush\n");
        io->status = 0;
        *completionCode = ERROR_SUCCESS;
        if (!p_ctx->completion_support) { quit = true; }
        break;
      case VDC_ClearError:
        Dmsg(ctx, debuglevel, "mssqlvdi-fd: ClearError\n");
        *completionCode = ERROR_SUCCESS;
        break;
      case VDC_Complete:
        Dmsg(ctx, debuglevel, "mssqlvdi-fd: Complete\n");
        io->status = 0;
        *completionCode = ERROR_SUCCESS;
        quit = true;
        break;
      default:
        Dmsg(ctx, debuglevel, "mssqlvdi-fd: Unknown = %d\n", cmd->commandCode);
        *completionCode = ERROR_NOT_SUPPORTED;
        quit = true;
        goto bail_out;
    }

    hr = p_ctx->VDIDevice->CompleteCommand(cmd, *completionCode, io->status, 0);
    if (!SUCCEEDED(hr)) {
      auto* explanation = explain_hr(hr);
      Jmsg(
          ctx, M_ERROR,
          "mssqlvdi-fd: IClientVirtualDevice::CompleteCommand: Err=%s (0x%X)\n",
          explanation, hr);
      Dmsg(
          ctx, debuglevel,
          "mssqlvdi-fd: IClientVirtualDevice::CompleteCommand: Err=%s (0x%X)\n",
          explanation, hr);
      goto bail_out;
    }
  }

  io->io_errno = 0;
  io->lerror = 0;
  io->win32 = false;

  return true;

bail_out:
  Dmsg(ctx, debuglevel, "mssqlvdi-fd: IO: reached bailout ...\n");
  // Report any COM errors.
  comReportError(ctx, hr);

  // Wait for the adoThread to exit.
  if (p_ctx->AdoThreadStarted) {
    if (!pthread_equal(p_ctx->ADOThread, pthread_self())) {
      pthread_cancel(p_ctx->ADOThread);
      pthread_join(p_ctx->ADOThread, NULL);
      p_ctx->AdoThreadStarted = false;
    }
  }

  return false;
}

// End of I/O tear down the VDI and check if everything did go to plan.
static inline bool TearDownVdiDevice(PluginContext* ctx, io_pkt* io)
{
  HRESULT hr = NOERROR;
  VDC_Command* cmd;
  plugin_ctx* p_ctx = (plugin_ctx*)ctx->plugin_private_context;

  Dmsg(ctx, debuglevel, "mssqlvdi-fd: entering TearDownVdiDevice\n");

  // Check if the VDI device is closed.
  if (p_ctx->VDIDevice) {
  tryagain:
    hr = p_ctx->VDIDevice->GetCommand(VDI_WAIT_TIMEOUT, &cmd);

    if (hr == NO_ERROR) {
      // we got another command for some reason

      const char* type = command_name(cmd->commandCode);
      int status = 0;

      switch (cmd->commandCode) {
        case VDC_Read: {
          status = cmd->size;
        } break;
        case VDC_Write: {
          status = cmd->size;
        } break;
      }

      Jmsg(ctx, M_ERROR,
           "Received command %d:%s (size = %d) when trying to close device\n",
           cmd->commandCode, type, status);

      hr = p_ctx->VDIDevice->CompleteCommand(cmd, ERROR_SUCCESS, status, 0);
      goto tryagain;
    }

    if (hr != VD_E_CLOSE) {
      auto* explanation = explain_hr(hr);

      Jmsg(ctx, M_ERROR,
           "Abnormal termination, VDIDevice not closed. Err=%s (0x%X)\n",
           explanation, hr);
      Dmsg(ctx, debuglevel,
           "Abnormal termination, VDIDevice not closed. Err=%s (0x%X)\n",
           explanation, hr);
      goto bail_out;
    }
  }

  // Close and release the VDIDevice and VDIDeviceSet.
  CloseVdiDeviceset(p_ctx);

  // See if there is any error to report from the ADO layer.
  if (p_ctx->AdoThreadStarted) {
    if (adoReportError(ctx)) { goto bail_out; }
  }

  io->status = 0;
  io->io_errno = 0;
  io->lerror = 0;
  io->win32 = false;

  Dmsg(ctx, debuglevel, "mssqlvdi-fd: leaving TearDownVdiDevice\n");

  return true;

bail_out:
  // Report any COM errors.
  comReportError(ctx, hr);

  Dmsg(ctx, debuglevel, "mssqlvdi-fd: leaving TearDownVdiDevice\n");

  return false;
}

// Bareos is calling us to do the actual I/O
static bRC pluginIO(PluginContext* ctx, io_pkt* io)
{
  DWORD completionCode = ERROR_BAD_ENVIRONMENT;
  plugin_ctx* p_ctx = (plugin_ctx*)ctx->plugin_private_context;

  if (!p_ctx) { return bRC_Error; }

  switch (io->func) {
    case IO_OPEN:
      if (p_ctx->RestoreToFile) {
        if (!PerformFileIo(ctx, io, &completionCode)) { goto bail_out; }
      } else {
        if (!SetupVdiDevice(ctx, io)) { goto bail_out; }
      }
      break;
    case IO_READ:
      if (!p_ctx->VDIDevice) { return bRC_Error; }
      if (!PerformVdiIo(ctx, io, &completionCode)) { goto bail_out; }
      break;
    case IO_WRITE:
      if (p_ctx->RestoreToFile) {
        if (!PerformFileIo(ctx, io, &completionCode)) { goto bail_out; }
      } else {
        if (!p_ctx->VDIDevice) { return bRC_Error; }
        if (!PerformVdiIo(ctx, io, &completionCode)) { goto bail_out; }
      }
      break;
    case IO_CLOSE:
      if (p_ctx->RestoreToFile) {
        if (!PerformFileIo(ctx, io, &completionCode)) { goto bail_out; }
      } else {
        if (!TearDownVdiDevice(ctx, io)) { goto bail_out; }
      }
      break;
    case IO_SEEK:
      if (p_ctx->RestoreToFile) {
        if (!PerformFileIo(ctx, io, &completionCode)) { goto bail_out; }
      } else {
        Jmsg(ctx, M_ERROR, "Illegal Seek request on VDIDevice.");
        Dmsg(ctx, debuglevel, "Illegal Seek request on VDIDevice.");
        goto bail_out;
      }
      break;
  }

  return bRC_OK;

bail_out:
  if (!p_ctx->RestoreToFile) {
    // Report any ADO errors.
    adoReportError(ctx);

    // Generic error handling.
    CloseVdiDeviceset(p_ctx);
  }

  io->io_errno = completionCode;
  io->lerror = completionCode;
  io->win32 = true;
  io->status = -1;

  return bRC_Error;
}

// See if we need to do any postprocessing after the restore.
static bRC end_restore_job(PluginContext* ctx, void*)
{
  bRC retval = bRC_OK;
  plugin_ctx* p_ctx = (plugin_ctx*)ctx->plugin_private_context;

  if (!p_ctx) { return bRC_Error; }

  Dmsg(ctx, debuglevel, "mssqlvdi-fd: entering end_restore_job\n");

  if (!p_ctx->RestoreToFile && p_ctx->RecoverAfterRestore) {
    if (!PerformAdoRecover(ctx)) { retval = bRC_Error; }
  }

  Dmsg(ctx, debuglevel, "mssqlvdi-fd: leaving end_restore_job\n");

  return retval;
}

/**
 * Bareos is notifying us that a plugin name string was found,
 * and passing us the plugin command, so we can prepare for a restore.
 */
static bRC startRestoreFile(PluginContext*, const char*) { return bRC_OK; }

/**
 * Bareos is notifying us that the plugin data has terminated,
 * so the restore for this particular file is done.
 */
static bRC endRestoreFile(PluginContext*) { return bRC_OK; }

/**
 * This is called during restore to create the file (if necessary) We must
 * return in rp->create_status:
 *
 *  CF_ERROR    -- error
 *  CF_SKIP     -- skip processing this file
 *  CF_EXTRACT  -- extract the file (i.e.call i/o routines)
 *  CF_CREATED  -- created, but no content to extract (typically directories)
 *  CF_CORE     -- let bareos core create the file
 */
static bRC createFile(PluginContext* ctx, restore_pkt* rp)
{
  plugin_ctx* p_ctx = (plugin_ctx*)ctx->plugin_private_context;

  if (!p_ctx) { return bRC_Error; }

  if (strlen(rp->where) > 0) {
    Dmsg(ctx, debuglevel, "mssqlvdi-fd: create file -> file io\n");
    p_ctx->RestoreToFile = true;
    p_ctx->RestoreFD = -1;
    rp->create_status = CF_CORE;
  } else {
    Dmsg(ctx, debuglevel, "mssqlvdi-fd: create file -> vdi io\n");
    rp->create_status = CF_EXTRACT;
  }

  return bRC_OK;
}

/**
 * We will get here if the File is a directory after everything is written in
 * the directory.
 */
static bRC setFileAttributes(PluginContext*, restore_pkt*) { return bRC_OK; }

// When using Incremental dump, all previous dumps are necessary
static bRC checkFile(PluginContext*, char*) { return bRC_OK; }

} /* namespace filedaemon */
