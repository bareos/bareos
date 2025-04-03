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

// polyfill for C++23
template <class Enum>
constexpr std::underlying_type_t<Enum> to_underlying(Enum e) noexcept
{
  return static_cast<std::underlying_type_t<Enum>>(e);
}


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
#define DBGC(ctx, ...)                      \
  do {                                      \
    DbgLogger{(ctx)}.Log(100, __VA_ARGS__); \
  } while (0)
#define TRCC(ctx, ...)                      \
  do {                                      \
    DbgLogger{(ctx)}.Log(200, __VA_ARGS__); \
  } while (0)
#define INFO(ctx, ...)                         \
  do {                                         \
    JobLogger{(ctx)}.Log(M_INFO, __VA_ARGS__); \
  } while (0)

const void* Pointer(const auto* x) { return static_cast<const void*>(x); }

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
      TRC(L"... failed ({:X})", com_hres);                    \
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

struct BaseObject {
  CComPtr<IWbemClassObject> ptr;

  IWbemClassObject* get() const { return ptr.p; }

  String to_string() const
  {
    BSTR repr = nullptr;
    WMI_CALL(ptr->GetObjectText(0, &repr));

    return String::wrap(repr);
  }
};

struct ClassObject : BaseObject {
  String path() const { return String::copy(L""); }
};

struct CallResult : BaseObject {
  void get(std::wstring_view name, VARIANT& var)
  {
    auto cpy = String::copy(name);
    WMI_CALL(ptr->Get(cpy.get(), 0, &var, NULL, 0));
  }

  template <typename T> T get(const wchar_t* name) = delete;
  template <> int32_t get<int32_t>(const wchar_t* name)
  {
    VARIANT param;

    CIMTYPE type;

    WMI_CALL(ptr->Get(name, 0, &param, &type, 0));

    if (V_VT(&param) != VT_I4) {
      // throw error
    }

    // if (type != expected_type) {
    //   // throw error
    // }

    return V_I4(&param);
  }
};

struct ParameterPack : BaseObject {
  struct Proxy {
    ParameterPack& pack;
    const wchar_t* name;

    template <typename T> void operator=(T value)
    {
      pack.put(name, std::forward<T>(value));
    }
  };

  Proxy operator[](const wchar_t* name) { return Proxy{*this, name}; }

  void put(const wchar_t* name, int32_t value)
  {
    VARIANT param;

    V_VT(&param) = VT_I4;
    V_I4(&param) = value;

    WMI_CALL(ptr->Put(name, 0, &param, 0));
  }
  void put(const wchar_t* name, const String& value)
  {
    VARIANT param;

    V_VT(&param) = VT_BSTR;
    V_BSTR(&param) = value.get();

    WMI_CALL(ptr->Put(name, 0, &param, 0));
  }
  void put(const wchar_t* name, const ClassObject& obj)
  {
    auto path = obj.path();
    put(name, path);
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

  Method load_method_by_name(std::wstring_view method_name) const
  {
    TRC(L"Loading method {}::{} ...", name.as_view(), method_name);

    Method mthd{String::copy(method_name)};
    WMI_CALL(ptr->GetMethod(mthd.name.get(), 0, &mthd.parameter_def, NULL));

    TRC(L"... succeeded ({}).", method_name);

    return mthd;
  }
};


struct ComputerSystem : ClassObject {};

struct Job : ClassObject {
  enum State : std::int32_t
  {
    New = 2,
    Starting = 3,
    Running = 4,
    Suspended = 5,
    ShuttingDown = 6,
    Completed = 7,
    Terminated = 8,
    Killed = 9,
    Exception = 10,
    Service = 11,
    QueryPending = 12,
  };

  static bool is_completed(State s)
  {
    return (s == State::Completed)
           //        || (s == State::CompletedWithWarning)
           || (s == State::Terminated) || (s == State::Exception)
           || (s == State::Killed);
  }
  State state() const { return {}; }
};

struct cim_error : public std::exception {
  int32_t error;

  const char* what() noexcept { return "cim error"; }

  cim_error(int32_t err) : error{err} {}
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

  Job get_job_by_name(const String& job_name)
  {
    CComPtr<IWbemClassObject> job;
    WMI_CALL(com_ptr->GetObject(job_name.get(), 0, NULL, &job, NULL));

    return Job{std::move(job)};
  }

  enum CIMReturnValue
  {
    OK = 0,
    JobStarted = 4096,
    Failed = 32768,
    AccessDenied = 32769,
    NotSupported = 32770,
    StatusIsUnknown = 32771,
    Timeout = 32772,
    InvalidParameter = 32773,
    SystemIsInUsed = 32774,
    InvalidStateForThisOperation = 32775,
    IncorrectDataType = 32776,
    SystemIsNotAvailable = 32777,
    OutOfMemory = 32778,
  };

  CallResult exec_method(const ClassObject& obj,
                         const Method& mthd,
                         const ParameterPack& params) const
  {
    auto path = obj.path();

    IWbemClassObject* out_params = nullptr;

    WMI_CALL(com_ptr->ExecMethod(path.get(), mthd.name.get(), 0, NULL,
                                 params.get(), &out_params, NULL));

    CallResult result{out_params};

    auto result_value = result.get<int32_t>(L"ReturnValue");

    if (result_value == CIMReturnValue::OK) { return std::move(result); }

    if (result_value == CIMReturnValue::JobStarted) {
      // TODO: wait for started job

      // we need to somehow get the type of the return value.
      // this way we could do some things automatically.

      // Maybe one can use IWbemClassObject::GetPropertyQualifierSet for this ?
    }

    throw new cim_error(result_value);
  }

  // returns std::nullopt if there is not exactly one match
  std::optional<ClassObject> query_single(const String& query) const
  {
    CComPtr<IEnumWbemClassObject> iter{};
    WMI_CALL(com_ptr->ExecQuery(
        bstr_t("WQL"), query.get(),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL, &iter));


    CComPtr<IWbemClassObject> found{};
    std::size_t total_count = 0;
    for (;;) {
      found.Release();
      ULONG found_count = 0;
      WMI_CALL(iter->Next(WBEM_INFINITE, 1, &found, &found_count));
      // found_count is either 0 or 1
      DBG(L"found {} elements matching query", found_count);
      if (found_count == 0) { break; }

      total_count += found_count;
    }

    if (total_count > 1) {
      TRC(L"{} instances found for query {}, not sure which to choose",
          total_count, query.as_view());
      return std::nullopt;
    } else if (total_count == 0) {
      TRC(L"No instance found for query {}", query.as_view());
      return std::nullopt;
    } else {
      TRC(L"found instance for query {} at {}", query.as_view(),
          Pointer(found.p));
      return ClassObject{found};
    }
  }

  std::optional<ComputerSystem> get_vm_by_name(std::wstring_view vm_name)
  {
    auto query = std::format(
        L"SELECT * FROM Msvm_ComputerSystem WHERE ElementName=\"{}\"", vm_name);
    auto cpy = String::copy(query);

    std::optional system = query_single(cpy);
    if (!system) { return std::nullopt; }
    return ComputerSystem{std::move(system).value()};
  }

 private:
  CComPtr<IWbemServices> com_ptr{};
};

class VirtualSystemManagementService : ClassObject {
 public:
  static constexpr std::wstring_view class_name{
      L"Msvm_VirtualSystemManagementService"};

  static std::optional<VirtualSystemManagementService> find_instance(
      const Service& srvc)
  {
    auto clz = srvc.load_class_by_name(class_name);

    auto query
        = String::copy(L"SELECT * FROM Msvm_VirtualSystemManagementService");
    auto obj = srvc.query_single(query);
    if (!obj) { return std::nullopt; }

    TRC(L"Instance of {} = {}", class_name, obj->to_string().as_view());

    return VirtualSystemManagementService{std::move(obj).value(),
                                          std::move(clz)};
  }

 private:
  VirtualSystemManagementService(ClassObject self, Class clz_)
      : ClassObject{std::move(self)}, clz{std::move(clz_)}
  {
  }

  Class clz;
};

struct ReferencePoint : ClassObject {};

struct Snapshot : ClassObject {
  ~Snapshot()
  {
    // todo: destroy the snapshot here if it is still valid
  }
};

struct VirtualSystemSnapshotSettingData {
  static constexpr std::wstring_view class_name{
      L"Msvm_VirtualSystemSnapshotSettingService"};

  enum class ConsistencyLevel
  {
    Unknown = 0,
    ApplicationConsistent = 1,
    CrashConsistent = 2,
  };

  enum class GuestBackupType
  {
    Undefined = 0,
    Full = 1,
    Copy = 2,
  };

  String as_xml() const { return String::copy(L""); }

  ConsistencyLevel consistency_level;
  GuestBackupType guest_backup_type;
  bool ignore_non_snapshottable_disks;
};

class VirtualSystemSnapshotService : ClassObject {
 public:
  static constexpr std::wstring_view class_name{
      L"Msvm_VirtualSystemSnapshotService"};

  enum class SnapshotType : std::int32_t
  {
    FullSnapshot = 2,
    DiskSnapshot = 3,
    RecoverySnapshot = 32768,
  };

  Snapshot create_snapshot(const Service& srvc,
                           const ComputerSystem& sys,
                           const VirtualSystemSnapshotSettingData& settings,
                           SnapshotType type) const
  {
    auto m_create_snapshot = clz.load_method_by_name(L"CreateSnapshot");
    auto params = m_create_snapshot.create_parameters();

    params[L"AffectedSystem"] = sys;
    params[L"SnapshotSettings"] = settings.as_xml();
    params[L"SnapshotType"] = to_underlying(type);

    auto result = srvc.exec_method(*this, m_create_snapshot, params);

    return {};
  };

  void destroy_snapshot(const Service& srvc, Snapshot snapshot) const
  {
    auto m_destroy_snapshot = clz.load_method_by_name(L"DestroySnapshot");
    auto params = m_destroy_snapshot.create_parameters();

    params[L"AffectedSnapshot"] = snapshot;

    auto result = srvc.exec_method(*this, m_destroy_snapshot, params);
  };

  ReferencePoint convert_to_reference_point(const Service& srvc,
                                            Snapshot snapshot)
  {
    auto m_convert_snapshot
        = clz.load_method_by_name(L"ConvertToReferencePoint");
    auto params = m_convert_snapshot.create_parameters();

    params[L"AffectedSnapshot"] = snapshot;

    auto result = srvc.exec_method(*this, m_convert_snapshot, params);

    return {};
  }

  static std::optional<VirtualSystemSnapshotService> find_instance(
      const Service& srvc)
  {
    auto clz = srvc.load_class_by_name(class_name);

    auto query
        = String::copy(L"SELECT * FROM Msvm_VirtualSystemSnapshotService");
    auto obj = srvc.query_single(query);
    if (!obj) { return std::nullopt; }

    TRC(L"Instance of {} = {}", class_name, obj->to_string().as_view());

    return VirtualSystemSnapshotService{std::move(obj).value(), std::move(clz)};
  }

 private:
  VirtualSystemSnapshotService(ClassObject self, Class clz_)
      : ClassObject{std::move(self)}, clz{std::move(clz_)}
  {
  }

  Class clz;
};

struct VirtualSystemExportSettingData {
  static constexpr std::wstring_view class_name{
      L"Msvm_VirtualSystemExportSettingService"};

  enum class CaptureLiveState : std::uint8_t
  {
    CrashConsistent = 0,
    Saved = 1,
    AppConsistent = 2,
  };

  enum class CopySnapshotConfiguration : std::uint8_t
  {
    ExportAllSnapshots = 0,
    ExportNoSnapShots = 1,
    ExportOneSnapshot = 2,
    ExportOneSnapshotForBackup = 3,
  };

  enum class BackupIntent : std::uint8_t
  {
    PreserveChain = 0,  // i.e. we want to store full/diff separately
    Merge = 1,          // i.e. we want to consolidate full/diff
  };

  std::optional<std::wstring> description{};
  std::optional<std::wstring> snapshot_virtual_system_path;
  std::vector<std::wstring> excluded_virtual_hard_disk_paths;
  std::optional<std::wstring> differential_backup_base_path;
  BackupIntent backup_intent = {};
  CopySnapshotConfiguration copy_snapshot_configuration = {};
  CaptureLiveState capture_live_state = {};
  bool copy_vm_runtime_information = {};
  bool copy_vm_storage = {};
  bool create_vm_export_subdirectory = {};
  bool export_for_live_migration = {};
  bool disable_differential_of_ignored_storage = {};
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
  static plugin_ctx* get(PluginContext* ctx)
  {
    auto p_ctx = static_cast<plugin_ctx*>(ctx->plugin_private_context);
    TRC(L"p_ctx = {}", Pointer(p_ctx));
    return p_ctx;
  }

  com_context ctx;
  WMI::Service virt_service;

  std::vector<std::wstring> vm_names;

  WMI::VirtualSystemSnapshotSettingData snapshot_settings;
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

  TRC(L"loading hyper-v succeeded");

  return bRC_OK;
}

// External entry point to unload the plugin
BAREOS_EXPORT bRC unloadPlugin()
{
  TRC(L"unloading hyper-v succeeded");
  return bRC_OK;
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
  // Initialize COM for this thread.

  try {
    auto com = com_context::init_for_thread();

    if (!InitializeComSecurity()) { return bRC_Error; }

    CComPtr<IWbemLocator> wmi_locator{};

    COM_CALL(wmi_locator.CoCreateInstance(CLSID_WbemLocator, 0,
                                          CLSCTX_INPROC_SERVER));

    DBGC(ctx, L"Locator = {}", Pointer(wmi_locator.p));

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

    DBGC(ctx, L"VirtService = {}", Pointer(wmi_locator.p));

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
    DBGC(ctx, L"could not initialize com.  Err={} ({:X})", err.err_str(),
         err.err_num());
    return bRC_Error;
  }
}

// Free a plugin instance, i.e. release our private storage
static bRC freePlugin(PluginContext* ctx)
{
  auto* p_ctx = plugin_ctx::get(ctx);
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

struct BackupClasses {
  WMI::VirtualSystemManagementService system_management;
  WMI::VirtualSystemSnapshotService system_snapshot;
};

static bool start_backup_job(PluginContext* ctx)
{
  auto* p_ctx = plugin_ctx::get(ctx);
  if (!p_ctx) { return false; }

  auto& srvc = p_ctx->virt_service;

  std::optional system_mgmt
      = WMI::VirtualSystemManagementService::find_instance(srvc);
  if (!system_mgmt) { return false; }
  std::optional system_snap
      = WMI::VirtualSystemSnapshotService::find_instance(srvc);
  if (!system_mgmt) { return false; }

  for (auto& vm_name : p_ctx->vm_names) {
    auto vm = srvc.get_vm_by_name(vm_name);
    if (!vm) { return false; }
    auto snapshot = system_snap->create_snapshot(
        srvc, vm.value(), p_ctx->snapshot_settings,
        WMI::VirtualSystemSnapshotService::SnapshotType::RecoverySnapshot);
  }

  return false;
}

// Handle an event that was generated in Bareos
static bRC handlePluginEvent(PluginContext* ctx, bEvent* event, void* value)
{
  auto* p_ctx = plugin_ctx::get(ctx);
  if (!p_ctx) { return bRC_Error; }

  using filedaemon::bEventBackupCommand;
  using filedaemon::bEventEndRestoreJob;
  using filedaemon::bEventLevel;
  using filedaemon::bEventNewPluginOptions;
  using filedaemon::bEventPluginCommand;
  using filedaemon::bEventRestoreCommand;
  using filedaemon::bEventStartBackupJob;
  using filedaemon::bEventStartRestoreJob;

  switch (event->eventType) {
    case bEventLevel: {
      return bRC_Error;
    } break;
    case bEventRestoreCommand: {
      return bRC_Error;
    } break;
    case bEventBackupCommand: {
      return bRC_Error;
    } break;
    case bEventPluginCommand: {
      return bRC_Error;
    } break;
    case bEventEndRestoreJob: {
      return bRC_Error;
    } break;
    case bEventNewPluginOptions: {
      return bRC_Error;
    } break;
    case bEventStartBackupJob: {
      if (!start_backup_job(ctx)) { return bRC_Error; }
      return bRC_OK;
    } break;
    case bEventStartRestoreJob: {
      return bRC_Error;
    } break;
    default: {
      DBG(L"unknown event type {}", event->eventType);
      return bRC_Error;
    } break;
  }
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
  auto* p_ctx = plugin_ctx::get(ctx);
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
