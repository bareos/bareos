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
#include <jansson.h>

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


std::wstring format_win32_error(DWORD error = GetLastError())
{
  wchar_t* buffer = nullptr;
  // take note: buffer is allocated via LocalAlloc ~> use LocalFree to dealloc
  auto format_success = FormatMessageW(
      FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_IGNORE_INSERTS
          | FORMAT_MESSAGE_FROM_SYSTEM,
      NULL, error, 0, reinterpret_cast<wchar_t*>(&buffer), 0, 0);

  auto result = [&] {
    if (format_success == 0) {
      return std::format(L"non formatable error {:X} (reason = {:X})", error,
                         GetLastError());
    } else {
      return std::format(L"{} ({:X})", buffer, error);
    }
  }();

  LocalFree(buffer);

  return result;
}

std::wstring utf8_to_utf16(std::string_view v)
{
  std::wstring result;
  if (v.size() == 0) { return result; }

  ASSERT(v.size() < static_cast<std::size_t>(std::numeric_limits<int>::max()));

  int in_length = static_cast<int>(v.size());
  int out_length = ::MultiByteToWideChar(
      CP_UTF8,               // Source string is in UTF-8
      MB_ERR_INVALID_CHARS,  // Conversion flags
      v.data(),              // Source UTF-8 string pointer
      in_length,             // Length of the source UTF-8 string, in chars
      nullptr,               // Unused - no conversion done in this step
      0                      // Request size of destination buffer, in wchar_ts
  );


  if (out_length <= 0) {
    // throw error or some such
  }

  result.resize(out_length);


  int conversion
      = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, v.data(),
                              in_length, result.data(), out_length);

  if (conversion != out_length) {
    // throw error, because something went wrong
  }

  return result;
}


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

  template <class... Args>
  void Log(int level, std::format_string<Args...> fmt, Args&&... args)
  {
    if (bareos_core_functions) {
      bareos_core_functions->DebugMessage(
          ctx, loc.function_name(), loc.line(), level, "%s\n",
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
  template <class... Args>
  void Log(int type, std::format_string<Args...> fmt, Args&&... args)
  {
    if (bareos_core_functions) {
      bareos_core_functions->JobMessage(
          ctx, loc.function_name(), loc.line(), type, 0, "%s\n",
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
#define JINFO(ctx, ...)                        \
  do {                                         \
    JobLogger{(ctx)}.Log(M_INFO, __VA_ARGS__); \
  } while (0)
#define JERR(ctx, ...)                          \
  do {                                          \
    JobLogger{(ctx)}.Log(M_ERROR, __VA_ARGS__); \
  } while (0)

const void* fmt_as_ptr(auto x) { return static_cast<const void*>(x); }

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
    Dmsg(ctx, debuglevel, "GetDescription failed\n");
    pErrorInfo->Release();
    return;
  }

  // Get the source of the COM error.
  hr = pErrorInfo->GetSource(&pSource);
  if (!SUCCEEDED(hr)) {
    Dmsg(ctx, debuglevel, "GetSource failed\n");
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
    Dmsg(ctx, debuglevel, "could not print error\n");
  }

  if (source) { free(source); }

  if (description) { free(description); }

  /* Generic cleanup (free the description and source as those are returned in
   * dynamically allocated memory by the COM routines.) */
  SysFreeString(pSource);
  SysFreeString(pDescription);

  pErrorInfo->Release();
}


class win_error : public std::exception {
 public:
  win_error(HRESULT res) noexcept : hres{res} {}

  const char* what() const noexcept override { return "windows error"; }
  std::wstring err_str() const { return format_win32_error(hres); }
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

std::wstring format_error(HRESULT error)
{
  constexpr HRESULT WMI_ERROR_MASK = 0x80041000;
  static HMODULE mod = LoadLibraryW(L"wmiutils.dll");

  if (!mod) { return L"Could not load error module"; }

  bool is_wmi_error
      = error >= WMI_ERROR_MASK && error <= WMI_ERROR_MASK + 0xFFF;
  auto flags = FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_FROM_SYSTEM
               | FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_MAX_WIDTH_MASK
               | (is_wmi_error ? FORMAT_MESSAGE_FROM_HMODULE : 0);

  auto actual_mod = is_wmi_error ? mod : 0;

  wchar_t* output = nullptr;
  auto byte_count = FormatMessageW(flags, actual_mod, error, 0, (LPWSTR)&output,
                                   0, nullptr);

  if (byte_count < 0) { return L"Unformatable error"; }

  std::wstring s{std::wstring_view{output, byte_count}};

  LocalFree(output);

  return s;
}

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

struct variant_type {
  struct i4 {
    static constexpr VARENUM type = VT_I4;
    using c_type = int32_t;

    static c_type from(VARIANT* var)
    {
      if (auto actual_type = V_VT(var); actual_type != type) {
        throw std::runtime_error(std::format("expected variant {}, but got {}",
                                             (int)actual_type, (int)type));
      }

      return V_I4(var);
    }
  };

  struct bstr {
    static constexpr VARENUM type = VT_BSTR;
    using c_type = String;

    static c_type from(VARIANT* var)
    {
      if (auto actual_type = V_VT(var); actual_type != type) {
        throw std::runtime_error(std::format("expected variant {}, but got {}",
                                             (int)actual_type, (int)type));
      }

      return String::wrap(V_BSTR(var));
    }
  };
};

struct cim_type {
#define DEFINE_CIM_TYPE(name, vtype, ctype)        \
  struct name {                                    \
    using v_type = variant_type::##vtype;          \
    static constexpr CIMTYPE c_type = CIM_##ctype; \
  }

  // these are mostly found by experimentation
  DEFINE_CIM_TYPE(uint16, i4, UINT16);
  DEFINE_CIM_TYPE(uint32, i4, UINT32);
  DEFINE_CIM_TYPE(reference, bstr, REFERENCE);
  DEFINE_CIM_TYPE(string, bstr, STRING);

#undef DEFINE_CIM_TYPE
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

  template <typename CimType>
  CimType::v_type::c_type get(const wchar_t* name) const
  {
    VARIANT param;
    CIMTYPE type;

    DBG(L"{}->get({})", fmt_as_ptr(ptr.p), name);
    WMI_CALL(ptr->Get(name, 0, &param, &type, 0));

    if (type != CimType::c_type) {
      throw std::runtime_error(std::format("expected cimtype {}, but got {}",
                                           (int)type, (int)CimType::c_type));
    }

    return CimType::v_type::from(&param);
  }
};

struct ClassObject : BaseObject {
  String path() const { return BaseObject::get<cim_type::string>(L"__PATH"); }
};

struct CallResult : BaseObject {
  std::optional<String> job_name;
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

    DBG(L"{}->put({}, {}):", fmt_as_ptr(ptr.p), name, value);
    WMI_CALL(ptr->Put(name, 0, &param, 0));
  }
  void put(const wchar_t* name, const String& value)
  {
    VARIANT param;

    V_VT(&param) = VT_BSTR;
    V_BSTR(&param) = value.get();

    DBG(L"{}->put({}, {}):", fmt_as_ptr(ptr.p), name, value.as_view());
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

    /* this does not appear in the documentation, but for some reason its
     * part of the sample code given by microsoft
     * see: https://learn.microsoft.com/en-us/windows/win32/hyperv_v2/
     *        common-utilities-for-the-virtualization-samples-v2 */
    CompletedWithWarnings = 32768,
  };

  static bool is_completed(State s)
  {
    return (s == State::Completed) || (s == State::CompletedWithWarnings)
           || (s == State::Terminated) || (s == State::Exception)
           || (s == State::Killed);
  }
  static bool is_successful(State s)
  {
    return (s == State::Completed) || (s == State::CompletedWithWarnings);
  }

  std::wstring format_err() const
  {
    // error description + GetErrorEx
    // uint16   ErrorCode;
    // string   ErrorDescription;
    // string   ErrorSummaryDescription;

    auto code = get<cim_type::uint16>(L"ErrorCode");
    auto desc = get<cim_type::string>(L"ErrorDescription");
    auto sum_desc = get<cim_type::string>(L"ErrorSummaryDescription");

    // TODO: call GetErrorEx

    return std::format(L"JobError({}): {} ({})", code, desc.as_view(),
                       sum_desc.as_view());
  }

  State state() const
  {
    return static_cast<State>(get<cim_type::uint16>(L"JobState"));
  }
};

struct cim_error : public std::exception {
  int32_t error;

  const char* what() const noexcept override { return "cim error"; }

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

  ClassObject get_object_by_path(const String& path) const
  {
    CComPtr<IWbemClassObject> obj;
    WMI_CALL(com_ptr->GetObject(path.get(), 0, NULL, &obj, NULL));

    return ClassObject{std::move(obj)};
  }

  Job get_job_by_name(const String& job_name) const
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

  std::optional<CallResult> exec_method(const ClassObject& obj,
                                        const Method& mthd,
                                        const ParameterPack& params) const
  {
    auto path = obj.path();

    IWbemClassObject* out_params = nullptr;

    TRC(L"calling {}.{}({}) ...", path.as_view(), mthd.name.as_view(),
        params.to_string().as_view());

    WMI_CALL(com_ptr->ExecMethod(path.get(), mthd.name.get(), 0, NULL,
                                 params.get(), &out_params, NULL));

    CallResult result{out_params};

    TRC(L"... returning {}", result.to_string().as_view());

    auto result_value = result.get<cim_type::uint32>(L"ReturnValue");

    if (result_value == CIMReturnValue::OK) { return std::move(result); }

    if (result_value == CIMReturnValue::JobStarted) {
      auto job_name = result.get<cim_type::reference>(L"Job");

      TRC(L"Got job name = {}", job_name.as_view());

      for (;;) {
        auto job = get_job_by_name(job_name);
        TRC(L"Job = {}", job.to_string().as_view());

        auto state = job.state();

        if (Job::is_completed(state)) {
          if (Job::is_successful(state)) {
            result.job_name.emplace(std::move(job_name));
            return std::move(result);
          } else {
            // maybe this should be an exception after all
            std::wstring jn{job_name.as_view()};

            // Jmsg(ctx, M_ERROR, "Job '%ls' did not complete successfully:
            // %ls\n",
            //      jn.c_str(), job.format_err().c_str());
            // write some error message here
            return std::nullopt;
          }
        } else {
          TRC(L"Job {} is still running.  Sleeping...", job_name.as_view());
          std::this_thread::sleep_for(std::chrono::seconds(1));
        }
      }

      // we need to somehow get the type of the return value.
      // this way we could do some things automatically.

      // Maybe one can use IWbemClassObject::GetPropertyQualifierSet for this ?
    }

    throw cim_error(result_value);
  }

  template <typename T>
  T get_result(const CallResult& res, const wchar_t* name) const
  {
    if (res.job_name) {
      auto related
          = get_related_of_class(res.job_name->as_view(), T::class_name);
      ASSERT(related.size() == 1);
      return T{std::move(related[0])};
    } else {
      auto path = res.get<cim_type::reference>(name);
      return T{get_object_by_path(path)};
    }
  }

  // returns std::nullopt if there is not exactly one match
  std::optional<ClassObject> query_single(const String& query) const
  {
    DBG(L"executing query {}", query.as_view());

    CComPtr<IEnumWbemClassObject> iter{};
    WMI_CALL(com_ptr->ExecQuery(
        bstr_t("WQL"), query.get(),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL, &iter));


    CComPtr<IWbemClassObject> target{};
    std::size_t total_count = 0;
    for (;;) {
      CComPtr<IWbemClassObject> found{};
      ULONG found_count = 0;
      WMI_CALL(iter->Next(WBEM_INFINITE, 1, &found, &found_count));
      // found_count is either 0 or 1
      DBG(L"found {} elements matching query", found_count);
      if (found_count == 0) { break; }

      if (total_count == 0) { target = std::move(found); }

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
          fmt_as_ptr(target.p));
      return ClassObject{target};
    }
  }

  std::vector<ClassObject> query_all(const String& query) const
  {
    DBG(L"executing query {}", query.as_view());

    CComPtr<IEnumWbemClassObject> iter{};
    WMI_CALL(com_ptr->ExecQuery(
        bstr_t("WQL"), query.get(),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL, &iter));

    std::vector<ClassObject> result;
    for (;;) {
      ClassObject next;
      ULONG returned_count = 0;
      WMI_CALL(iter->Next(WBEM_INFINITE, 1, &next.ptr, &returned_count));

      if (returned_count == 0) { break; }

      result.emplace_back(std::move(next));
    }

    return result;
  }

  std::vector<ClassObject> get_related_of_class(std::wstring_view associate,
                                                std::wstring_view result_class,
                                                std::wstring_view assoc_class
                                                = {}) const
  {
    std::wstring query = [&] {
      if (assoc_class.empty()) {
        return std::format(L"associators of {{{}}} where ResultClass = {}",
                           associate, result_class);
      } else {
        return std::format(
            L"associators of {{{}}} where ResultClass = {} AssocClass = {}",
            associate, result_class, assoc_class);
      }
    }();

    auto squery = String::copy(query);

    return query_all(squery);
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
  static constexpr std::wstring_view class_name{
      L"CIM_VirtualSystemSettingData"};

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

    if (!result) {
      throw std::runtime_error("exec_method returned std::nullopt");
    }

    auto snapshot
        = srvc.get_result<Snapshot>(result.value(), L"ResultingSnapshot");

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
    if (FAILED(hr)) { throw win_error{hr}; }

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
    TRCC(ctx, L"p_ctx = {}", fmt_as_ptr(p_ctx));
    return p_ctx;
  }

  com_context ctx;
  WMI::Service virt_service;

  std::vector<std::wstring> vm_names;

  WMI::VirtualSystemSnapshotSettingData snapshot_settings;

  json_t* config{nullptr};

  ~plugin_ctx()
  {
    if (config) { json_decref(config); }
  }
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

    DBGC(ctx, L"Locator = {}", fmt_as_ptr(wmi_locator.p));

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

    DBGC(ctx, L"VirtService = {}", fmt_as_ptr(wmi_locator.p));

    JINFO(ctx, L"Successfully connected to 'ROOT\\VIRTUALIZATION\\V2'");

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

const char* event_name(uint32_t event_type)
{
  switch (event_type) {
    case filedaemon::bEventJobStart: {
      return "JobStart";
    } break;
    case filedaemon::bEventJobEnd: {
      return "JobEnd";
    } break;
    case filedaemon::bEventStartBackupJob: {
      return "StartBackupJob";
    } break;
    case filedaemon::bEventEndBackupJob: {
      return "EndBackupJob";
    } break;
    case filedaemon::bEventStartRestoreJob: {
      return "StartRestoreJob";
    } break;
    case filedaemon::bEventEndRestoreJob: {
      return "EndRestoreJob";
    } break;
    case filedaemon::bEventStartVerifyJob: {
      return "StartVerifyJob";
    } break;
    case filedaemon::bEventEndVerifyJob: {
      return "EndVerifyJob";
    } break;
    case filedaemon::bEventBackupCommand: {
      return "BackupCommand";
    } break;
    case filedaemon::bEventRestoreCommand: {
      return "RestoreCommand";
    } break;
    case filedaemon::bEventEstimateCommand: {
      return "EstimateCommand";
    } break;
    case filedaemon::bEventLevel: {
      return "Level";
    } break;
    case filedaemon::bEventSince: {
      return "Since";
    } break;
    case filedaemon::bEventCancelCommand: {
      return "CancelCommand";
    } break;
    case filedaemon::bEventRestoreObject: {
      return "RestoreObject";
    } break;
    case filedaemon::bEventEndFileSet: {
      return "EndFileSet";
    } break;
    case filedaemon::bEventPluginCommand: {
      return "PluginCommand";
    } break;
    case filedaemon::bEventOptionPlugin: {
      return "OptionPlugin";
    } break;
    case filedaemon::bEventHandleBackupFile: {
      return "HandleBackupFile";
    } break;
    case filedaemon::bEventNewPluginOptions: {
      return "NewPluginOptions";
    } break;
    case filedaemon::bEventVssInitializeForBackup: {
      return "VssInitializeForBackup";
    } break;
    case filedaemon::bEventVssInitializeForRestore: {
      return "VssInitializeForRestore";
    } break;
    case filedaemon::bEventVssSetBackupState: {
      return "VssSetBackupState";
    } break;
    case filedaemon::bEventVssPrepareForBackup: {
      return "VssPrepareForBackup";
    } break;
    case filedaemon::bEventVssBackupAddComponents: {
      return "VssBackupAddComponents";
    } break;
    case filedaemon::bEventVssPrepareSnapshot: {
      return "VssPrepareSnapshot";
    } break;
    case filedaemon::bEventVssCreateSnapshots: {
      return "VssCreateSnapshots";
    } break;
    case filedaemon::bEventVssRestoreLoadComponentMetadata: {
      return "VssRestoreLoadComponentMetadata";
    } break;
    case filedaemon::bEventVssRestoreSetComponentsSelected: {
      return "VssRestoreSetComponentsSelected";
    } break;
    case filedaemon::bEventVssCloseRestore: {
      return "VssCloseRestore";
    } break;
    case filedaemon::bEventVssBackupComplete: {
      return "VssBackupComplete";
    } break;
  }

  return "unknown";
}

// Handle an event that was generated in Bareos
static bRC handlePluginEvent(PluginContext* ctx, bEvent* event, void* value)
{
  auto* p_ctx = plugin_ctx::get(ctx);
  if (!p_ctx) { return bRC_Error; }

  DBGC(ctx, "received event {} ({}); value = {}", event_name(event->eventType),
       event->eventType, value);

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
      return parse_plugin_definition(ctx, value);
    } break;
    case bEventPluginCommand: {
      return parse_plugin_definition(ctx, value);
    } break;
    case bEventEndRestoreJob: {
      return bRC_Error;
    } break;
    case bEventNewPluginOptions: {
      return parse_plugin_definition(ctx, value);
    } break;
    case bEventStartBackupJob: {
      // if (!start_backup_job(ctx)) { return bRC_Error; }
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
  auto* p_ctx = plugin_ctx::get(ctx);
  if (!p_ctx) { return bRC_Error; }

  try {
    json_t* val = json_object_get(p_ctx->config, "vmname");
    if (!val) {
      JERR(ctx, "No 'vmname' set in config file");
      return bRC_Error;
    }
    const char* vmname = json_string_value(val);
    if (!val) {
      JERR(ctx, "'vmname' is not a string in config file");
      return bRC_Error;
    }

    std::wstring wname = utf8_to_utf16(vmname);
    p_ctx->vm_names.emplace_back(std::move(wname));

    if (!start_backup_job(ctx)) { return bRC_Error; }

    return bRC_OK;
  } catch (const win_error& err) {
    DBGC(ctx, L"caught win error.  Err={} ({:X})", err.err_str(),
         err.err_num());
    comReportError(ctx, err.err_num());
    return bRC_Error;
  } catch (const WMI::cim_error& err) {
    DBGC(ctx, L"caught cim error.  Err={} ({})", WMI::format_error(err.error),
         static_cast<uint32_t>(err.error));
    return bRC_Error;
  } catch (const std::exception& ex) {
    JERR(ctx, "caught exception: {}", ex.what());
    return bRC_Error;
  } catch (...) {
    // TOOD: use JFATAL here
    JERR(ctx, "detected an internal error.  aborting job ...");
    return bRC_Error;
  }
}

// Done with backup of this file
static bRC endBackupFile(PluginContext*)
{
  /* We would return bRC_More if we wanted startBackupFile to be called again to
   * backup another file */
  return bRC_OK;
}

std::size_t read_file_contents(PluginContext* ctx,
                               HANDLE handle,
                               char* buf,
                               std::size_t bufsize)
{
  std::size_t bytes_read = 0;
  while (bytes_read < bufsize) {
    constexpr std::size_t max_bytes_per_call
        = std::numeric_limits<DWORD>::max();
    DWORD bytes_to_read = static_cast<DWORD>(
        std::min(bufsize - bytes_read, max_bytes_per_call));
    DWORD bytes_actually_read = 0;
    if (!ReadFile(handle, buf + bytes_read, bytes_to_read, &bytes_actually_read,
                  nullptr)) {
      DBGC(ctx, "could not read from {}, bytes read = {}, bytes to read = {}",
           fmt_as_ptr(handle), bytes_read, bytes_to_read);
      return 0;
    }

    if (bytes_actually_read == 0) {
      DBGC(ctx,
           "read 0 bytes from {}, when trying to read {}; already read = {}",
           fmt_as_ptr(handle), bytes_to_read, bytes_read);
      break;
    }

    bytes_read += bytes_actually_read;
  }
  return bytes_read;
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
  DBGC(ctx, "parse_plugin_definition");
  auto* p_ctx = plugin_ctx::get(ctx);
  if (!p_ctx) {
    Jmsg(ctx, M_FATAL, "plugin private context not set\n");
    return bRC_Error;
  }

  std::string_view input = static_cast<const char*>(value);

  DBGC(ctx, "start parsing {}", input);
  constexpr std::string_view plugin_name = "hyper-v";

  if (input.size() < plugin_name.size()) {
    Jmsg(ctx, M_ERROR, "bad plugin definition (its too small): %.*s\n",
         static_cast<int>(input.size()), input.data());
    return bRC_Error;
  }

  if (input.substr(0, plugin_name.size()) != plugin_name) {
    Jmsg(ctx, M_ERROR, "bad plugin definition (wrong plugin name): %.*s\n",
         static_cast<int>(input.size()), input.data());
    return bRC_Error;
  }

  auto rest = input.substr(plugin_name.size());
  DBGC(ctx, "continuing with {}", rest);

  if (rest.size() == 0 || rest[0] != ':') {
    Jmsg(ctx, M_ERROR, "bad plugin definition (expected ':' at %llu): %.*s\n",
         static_cast<long long unsigned>(input.size() - rest.size()),
         static_cast<int>(input.size()), input.data());
    return bRC_Error;
  }

  rest = rest.substr(1);
  DBGC(ctx, "continuing with {}", rest);

  constexpr std::string_view config_path = "config";

  if (rest.size() < config_path.size()) {
    Jmsg(ctx, M_ERROR,
         "bad plugin definition (missing data after ':' at %llu): %.*s\n",
         static_cast<long long unsigned>(input.size() - rest.size()),
         static_cast<int>(input.size()), input.data());
    return bRC_Error;
  }

  if (rest.substr(0, config_path.size()) != config_path) {
    Jmsg(ctx, M_ERROR, "bad plugin definition (unknown option at %llu): %.*s\n",
         static_cast<long long unsigned>(input.size() - rest.size()),
         static_cast<int>(input.size()), input.data());
    return bRC_Error;
  }

  rest = rest.substr(config_path.size());
  DBGC(ctx, "continuing with {}", rest);

  if (rest.size() == 0 || rest[0] != '=') {
    Jmsg(ctx, M_ERROR, "bad plugin definition (expected '=' at %llu): %.*s\n",
         static_cast<long long unsigned>(input.size() - rest.size()),
         static_cast<int>(input.size()), input.data());
    return bRC_Error;
  }

  rest = rest.substr(1);
  DBGC(ctx, "continuing with {}", rest);

  if (rest.size() == 0) {
    Jmsg(ctx, M_ERROR,
         "bad plugin definition (expected config path at %llu): %.*s\n",
         static_cast<long long unsigned>(input.size() - rest.size()),
         static_cast<int>(input.size()), input.data());
    return bRC_Error;
  }

  std::string path{rest};
  DBGC(ctx, "found config path = {}", path);
  std::wstring wpath = utf8_to_utf16(path);
  DBGC(ctx, L"found config path = {}", wpath);

  HANDLE config_handle
      = CreateFileW(wpath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL,
                    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  DBGC(ctx, "opened {} at {}", path, fmt_as_ptr(config_handle));
  if (config_handle == INVALID_HANDLE_VALUE) {
    Jmsg(ctx, M_ERROR, "could not open config file %s: Err=%ls\n", path.c_str(),
         format_win32_error().c_str());
    return bRC_Error;
  }

  LARGE_INTEGER file_size_li = {};

  if (!GetFileSizeEx(config_handle, &file_size_li)) {
    Jmsg(ctx, M_ERROR, "could not determine size of config file %s: Err=%ls\n",
         path.c_str(), format_win32_error().c_str());
    CloseHandle(config_handle);
    return bRC_Error;
  }

  std::size_t file_size = static_cast<std::size_t>(file_size_li.QuadPart);

  auto file_content = std::make_unique<char[]>(file_size);

  auto bytes_read
      = read_file_contents(ctx, config_handle, file_content.get(), file_size);
  if (bytes_read == 0) {
    Jmsg(ctx, M_ERROR, "could read file %s: Err=%ls\n", path.c_str(),
         format_win32_error().c_str());
    CloseHandle(config_handle);
    return bRC_Error;
  } else if (bytes_read != file_size) {
    Jmsg(ctx, M_ERROR,
         "could read complete file %s: only %llu out of expected %llu bytes "
         "were read\n",
         path.c_str(), static_cast<long long unsigned>(bytes_read),
         static_cast<long long unsigned>(file_size));
    CloseHandle(config_handle);
    return bRC_Error;
  }

  CloseHandle(config_handle);
  DBGC(ctx, "sucessfully loaded config file {} into memory at {}", path,
       fmt_as_ptr(file_content.get()));

  std::string_view config_content{file_content.get(), file_size};

  TRCC(ctx, "content = {}", config_content);

  json_error_t jerr = {};
  auto* json
      = json_loadb(config_content.data(), config_content.size(),
                   JSON_REJECT_DUPLICATES | JSON_DISABLE_EOF_CHECK, &jerr);

  if (!json) {
    JERR(ctx, "failed to parse config file {} as json: {} (at {}:{})", path,
         jerr.text, jerr.line, jerr.column);
    return bRC_Error;
  }

  p_ctx->config = json;

  return bRC_OK;
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
