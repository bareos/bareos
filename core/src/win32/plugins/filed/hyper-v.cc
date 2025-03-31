/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2025-2025 Bareos GmbH & Co. KG

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

#include "include/bareos.h"
#include "include/filetypes.h"
#include "fd_plugins.h"
#include "plugins/include/common.h"

#include <format>
#include <source_location>
#include <string_view>
#include <variant>
#include <exception>

#include <comdef.h>
#include <Wbemidl.h>
#include <atlsafe.h>
#include <initguid.h>  // ask virtdisk.h to include guid definitions
#include <virtdisk.h>


static const int debuglevel = 150;

#define PLUGIN_LICENSE "Bareos AGPLv3"
#define PLUGIN_AUTHOR "Sebastian Sura"
#define PLUGIN_DATE "April 2025"
#define PLUGIN_VERSION "1"
#define PLUGIN_DESCRIPTION "Bareos Hyper-V Windows File Daemon Plugin"
#define PLUGIN_USAGE \
  "\n  hyper-v:\n"   \
  ""

using filedaemon::bEvent;
using filedaemon::CoreFunctions;
using filedaemon::io_pkt;
using filedaemon::PluginApiDefinition;
using filedaemon::PluginFunctions;
using filedaemon::pVariable;
using filedaemon::restore_pkt;
using filedaemon::save_pkt;

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


struct DbgLogger {
  DbgLogger(PluginContext* ctx = nullptr,
            std::source_location sloc = std::source_location::current())
      : loc{sloc}, ctx{ctx}
  {
  }

  template <class... Args>
  void Log(int level, std::wformat_string<Args...> fmt, Args&&... args)
  {
    if (bareos_core_functions) {
      bareos_core_functions->DebugMessage(
          ctx, loc.function_name(), loc.line(), level, "%ls\n",
          std::format(fmt, std::forward<Args>(args)...).c_str());
    }
  }

  std::source_location loc;
  PluginContext* ctx;
};

struct JobLogger {
  JobLogger(PluginContext* ctx,
            std::source_location sloc = std::source_location::current())
      : loc{sloc}, ctx{ctx}
  {
  }

  template <class... Args>
  void Log(int type, std::wformat_string<Args...> fmt, Args&&... args)
  {
    if (bareos_core_functions) {
      bareos_core_functions->JobMessage(
          ctx, loc.function_name(), loc.line(), type, 0, "%ls\n",
          std::format(fmt, std::forward<Args>(args)...).c_str());
    }
  }

  std::source_location loc;
  PluginContext* ctx;
};

#define DBG(...)                       \
  do {                                 \
    DbgLogger{}.Log(100, __VA_ARGS__); \
  } while (0)
#define TRC(...)                       \
  do {                                 \
    DbgLogger{}.Log(200, __VA_ARGS__); \
  } while (0)
#define INFO(ctx, ...)                         \
  do {                                         \
    JobLogger{(ctx)}.Log(M_INFO, __VA_ARGS__); \
  } while (0)


class win_error : public std::exception {
 public:
  win_error(HRESULT res) noexcept : hres{res} {}

  const char* what() noexcept { return "windows error"; }
  std::wstring err_str() const { return {}; }
  HRESULT err_num() const { return hres; }

 private:
  HRESULT hres;
};

#define COM_CALL(...)                                         \
  do {                                                        \
    TRC(L"Calling {} ...", L## #__VA_ARGS__);                 \
    if (HRESULT com_hres = (__VA_ARGS__); FAILED(com_hres)) { \
      TRC(L"... failed ({})", com_hres);                      \
      throw win_error{com_hres};                              \
    }                                                         \
    TRC(L"... succeeded");                                    \
  } while (0)


namespace WMI {
#define WMI_CALL(...) COM_CALL(__VA_ARGS__)

class String {
 public:
  static String copy(std::wstring_view str)
  {
    if (str.size() > std::numeric_limits<UINT>::max()) {
      throw std::runtime_error("Cannot copy string that is too big");
    }
    return String{SysAllocStringLen(str.data(), static_cast<UINT>(str.size()))};
  }

  static String wrap(BSTR str) { return String{str}; }

  const BSTR get() const { return value; }

  std::size_t size() const
  {
    if (!value) { return 0; }
    return SysStringLen(value);
  }

  std::wstring_view as_view() const
  {
    if (!value) { return {}; }
    return std::wstring_view{value, SysStringLen(value)};
  }

  String(const String&) = delete;
  String& operator=(const String&) = delete;
  String(String&& other) { *this = std::move(other); };
  String& operator=(String&& other)
  {
    std::swap(value, other.value);
    return *this;
  }

  ~String()
  {
    if (value) { SysFreeString(value); }
  }

 private:
  String(BSTR val) : value{val} {}

  BSTR value = nullptr;
};

struct WMIBaseObject {
  CComPtr<IWbemClassObject> ptr;

  IWbemClassObject* get() const { return ptr.p; }

  String to_string() const
  {
    BSTR repr = nullptr;
    WMI_CALL(ptr->GetObjectText(0, &repr));

    return String::wrap(repr);
  }
};

struct ParameterPack : WMIBaseObject {
  void Put(const wchar_t* name, int32_t value)
  {
    VARIANT param;

    V_VT(&param) = VT_I4;
    V_I4(&param) = value;

    WMI_CALL(ptr->Put(name, 0, &param, 0));
  }
  void Put(const wchar_t* name, const String& value)
  {
    VARIANT param;

    V_VT(&param) = VT_BSTR;
    V_BSTR(&param) = value.get();

    WMI_CALL(ptr->Put(name, 0, &param, 0));
  }
};

struct Method {
  String name;
  CComPtr<IWbemClassObject> parameter_def;

  ParameterPack create_parameters() const
  {
    CComPtr<IWbemClassObject> params;

    WMI_CALL(parameter_def->SpawnInstance(0, &params));

    return ParameterPack{params};
  };
};

struct Class {
  String name;
  CComPtr<IWbemClassObject> ptr{};

  Method load_method_by_name(std::wstring_view method_name)
  {
    TRC(L"Loading method {}::{} ...", name.as_view(), method_name);

    Method mthd{String::copy(method_name)};
    WMI_CALL(ptr->GetMethod(mthd.name.get(), 0, &mthd.parameter_def, NULL));

    TRC(L"... succeeded ({}).", method_name);

    return mthd;
  }
};

struct ClassObject : WMIBaseObject {
  String path() const { return String::copy(L""); }
};

struct CallResult : WMIBaseObject {
  void get(std::wstring_view name, VARIANT& var)
  {
    auto cpy = String::copy(name);
    WMI_CALL(ptr->Get(cpy.get(), 0, &var, NULL, 0));
  }
};


struct Service {
  Service(CComPtr<IWbemServices> ptr) : com_ptr{std::move(ptr)} {}

  Class load_class_by_name(std::wstring_view class_name) const
  {
    TRC(L"Loading class {} ...", class_name);

    Class clz{String::copy(class_name)};

    WMI_CALL(com_ptr->GetObject(clz.name.get(), 0, NULL, &clz.ptr, NULL));

    TRC(L"... succeeded ({})", class_name);

    return std::move(clz);
  }

  CallResult exec_method(const ClassObject& obj,
                         const Method& mthd,
                         const ParameterPack& params)
  {
    auto path = obj.path();

    IWbemClassObject* out_params = nullptr;

    WMI_CALL(com_ptr->ExecMethod(path.get(), mthd.name.get(), 0, NULL,
                                 params.get(), &out_params, NULL));

    return CallResult{out_params};
  }

 private:
  CComPtr<IWbemServices> com_ptr{};
};
};  // namespace WMI

struct com_context {
  static com_context init_for_thread()
  {
    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr)) { throw win_error{bRC_Error}; }

    return com_context{};
  }

  com_context(const com_context&) = delete;
  com_context& operator=(const com_context&) = delete;
  com_context(com_context&& other) : active{other.active}
  {
    other.active = false;
  }
  com_context& operator=(com_context&& other)
  {
    active = other.active;
    other.active = false;
  }

  ~com_context()
  {
    if (active) { CoUninitialize(); }
  }

 private:
  com_context() = default;

  bool active = true;
};

// Plugin private context
struct plugin_ctx {
  com_context ctx;
  WMI::Service virt_service;
};


extern "C" {
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
  // Initialize COM for this thread.

  try {
    auto com = com_context::init_for_thread();

    if (!InitializeComSecurity()) { return bRC_Error; }

    CComPtr<IWbemLocator> wmi_locator{};

    COM_CALL(wmi_locator.CoCreateInstance(CLSID_WbemLocator, 0,
                                          CLSCTX_INPROC_SERVER));

    CComPtr<IWbemServices> virt_service{};

    COM_CALL(wmi_locator->ConnectServer(
        _bstr_t(L"ROOT\\VIRTUALIZATION\\V2"),  // Object path of WMI namespace
        NULL,                                  // User name. NULL = current user
        NULL,                                  // User password. NULL = current
        0,                                     // Locale. NULL indicates current
        NULL,                                  // Security flags.
        0,             // Authority (for example, Kerberos)
        0,             // Context object
        &virt_service  // pointer to IWbemServices proxy
        ));

    INFO(ctx, L"Successfully connected to 'ROOT\\VIRTUALIZATION\\V2'");

    COM_CALL(CoSetProxyBlanket(
        virt_service,
        RPC_C_AUTHN_WINNT,            // RPC_C_AUTHN_xxx
        RPC_C_AUTHZ_NONE,             // RPC_C_AUTHZ_xxx
        NULL,                         // Server principal name
        RPC_C_AUTHN_LEVEL_CALL,       // RPC_C_AUTHN_LEVEL_xxx
        RPC_C_IMP_LEVEL_IMPERSONATE,  // RPC_C_IMP_LEVEL_xxx
        NULL,                         // client identity
        EOAC_NONE                     // proxy capabilities
        ));

    WMI::Service srvc{std::move(virt_service)};

    plugin_ctx* p_ctx = new plugin_ctx{std::move(com), std::move(srvc)};

    /* set our context pointer */
    ctx->plugin_private_context = static_cast<void*>(p_ctx);

    using filedaemon::bEventBackupCommand;
    using filedaemon::bEventEndRestoreJob;
    using filedaemon::bEventLevel;
    using filedaemon::bEventNewPluginOptions;
    using filedaemon::bEventPluginCommand;
    using filedaemon::bEventRestoreCommand;
    using filedaemon::bEventStartBackupJob;
    using filedaemon::bEventStartRestoreJob;

    // Only register the events we are really interested in.
    bareos_core_functions->registerBareosEvents(
        ctx, 8, bEventLevel, bEventRestoreCommand, bEventBackupCommand,
        bEventPluginCommand, bEventEndRestoreJob, bEventNewPluginOptions,
        bEventStartBackupJob, bEventStartRestoreJob);

    return bRC_OK;
  } catch (const win_error& err) {
    DBG(L"could not initialize com.  Err={} ({})", err.err_str(),
        err.err_num());
    return bRC_Error;
  }
}

// Free a plugin instance, i.e. release our private storage
static bRC freePlugin(PluginContext* ctx)
{
  plugin_ctx* p_ctx = (plugin_ctx*)ctx->plugin_private_context;
  if (!p_ctx) { return bRC_Error; }

  delete p_ctx;

  return bRC_OK;
}

// Return some plugin value (none defined)
static bRC getPluginValue(PluginContext*, pVariable, void*)
{
  return bRC_Error;
}

// Set a plugin value (none defined)
static bRC setPluginValue(PluginContext*, pVariable, void*)
{
  return bRC_Error;
}

// Handle an event that was generated in Bareos
static bRC handlePluginEvent(PluginContext* ctx, bEvent* event, void* value)
{
  plugin_ctx* p_ctx = (plugin_ctx*)ctx->plugin_private_context;

  if (!p_ctx) { return bRC_Error; }

  return bRC_Error;
}

// Start the backup of a specific file
static bRC startBackupFile(PluginContext* ctx, save_pkt* sp)
{
  return bRC_Error;
}

// Done with backup of this file
static bRC endBackupFile(PluginContext*)
{
  /* We would return bRC_More if we wanted startBackupFile to be called again to
   * backup another file */
  return bRC_OK;
}
/**
 * Parse the plugin definition passed in.
 *
 * The definition is in this form:
 *
 * hyper-v:...
 */
static bRC parse_plugin_definition(PluginContext* ctx, void* value)
{
  return bRC_Error;
}

// Generic COM error reporting function.
static void comReportError(PluginContext* ctx, HRESULT hrErr)
{
#if 0
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
    Jmsg(ctx, M_FATAL, "%s(0x%X): %s\n", source, hrErr, description);
    Dmsg(ctx, debuglevel, "%s(0x%X): %s\n", source, hrErr, description);
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
#endif
}

static bRC pluginIO(PluginContext* ctx, io_pkt* io) { return bRC_Error; }

static bRC end_restore_job(PluginContext* ctx, void*) { return bRC_Error; }

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

  return bRC_Error;
}

/**
 * We will get here if the File is a directory after everything is written in
 * the directory.
 */
static bRC setFileAttributes(PluginContext*, restore_pkt*) { return bRC_OK; }

// When using Incremental dump, all previous dumps are necessary
static bRC checkFile(PluginContext*, char*) { return bRC_OK; }
