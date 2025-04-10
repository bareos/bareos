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
#include <algorithm>
#include <span>

#include <shlobj_core.h>
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
#define PLUGIN_USAGE            \
  "\n  hyper-v:config=<path>\n" \
  ""

using filedaemon::bEvent;
using filedaemon::CoreFunctions;
using filedaemon::io_pkt;
using filedaemon::PluginApiDefinition;
using filedaemon::PluginFunctions;
using filedaemon::pVariable;
using filedaemon::restore_pkt;
using filedaemon::save_pkt;


std::string format_win32_error_2(DWORD error = GetLastError())
{
  char* buffer = nullptr;
  // take note: buffer is allocated via LocalAlloc ~> use LocalFree to dealloc
  auto format_success = FormatMessageA(
      FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_IGNORE_INSERTS
          | FORMAT_MESSAGE_FROM_SYSTEM,
      NULL, error, 0, reinterpret_cast<char*>(&buffer), 0, 0);

  auto result = [&] {
    if (format_success == 0) {
      return std::format("non formatable error {:X} (reason = {:X})", error,
                         GetLastError());
    } else {
      return std::format("{} ({:X})", buffer, error);
    }
  }();

  LocalFree(buffer);

  return result;
}

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

struct werror : std::exception {
  werror(const char* where, DWORD err_num = GetLastError())
  {
    err_msg = std::format("caught error {} while trying to {}",
                          format_win32_error_2(err_num), where);
  }

  const char* what() const noexcept override { return err_msg.c_str(); }

  std::string err_msg;
};

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
    auto err = GetLastError();
    throw std::runtime_error(
        std::format("could not convert '{}' to utf16. Err={} ({})", v,
                    format_win32_error_2(err), err));
  }

  result.resize(out_length);


  int conversion
      = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, v.data(),
                              in_length, result.data(), out_length);

  if (conversion != out_length) {
    // throw error, because something went wrong
    throw std::runtime_error(
        std::format("could not convert '{}' to utf16. Err=One call returned "
                    "{}, but the next returned {}",
                    v, out_length, conversion));
  }

  return result;
}

std::string utf16_to_utf8(std::wstring_view v)
{
  std::string result;
  if (v.size() == 0) { return result; }

  ASSERT(v.size() < static_cast<std::size_t>(std::numeric_limits<int>::max()));

  int in_length = static_cast<int>(v.size());
  int out_length = ::WideCharToMultiByte(
      CP_UTF8,               // Convert to UTF-8
      WC_ERR_INVALID_CHARS,  // Conversion flags
      v.data(),              // Source UTF-16 string pointer
      in_length,             // Length of the source UTF-16 string, in wchar_ts
      nullptr,               // Unused - no conversion done in this step
      0,                     // Request size of destination buffer, in bytes
      nullptr, nullptr);

  if (out_length <= 0) {
    auto err = GetLastError();
    throw std::runtime_error(
        std::format("could not convert input to utf8. Err={} ({})",
                    format_win32_error_2(err), err));
  }

  result.resize(out_length);

  int conversion = ::WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
                                         v.data(), in_length, result.data(),
                                         out_length, nullptr, nullptr);

  if (conversion != out_length) {
    // throw error, because something went wrong
    throw std::runtime_error(
        std::format("could not convert input to utf8. Err=One call returned "
                    "{}, but the next returned {}",
                    out_length, conversion));
  }

  return result;
}


// Forward referenced functions
static bRC newPlugin(PluginContext* ctx);
static bRC freePlugin(PluginContext* ctx);
static bRC getPluginValue(PluginContext* ctx, pVariable var, void* value);
static bRC setPluginValue(PluginContext* ctx, pVariable var, void* value);
static bRC handlePluginEvent(PluginContext* ctx, bEvent* event, void* value);
static bRC start_backup_file(PluginContext* ctx, save_pkt* sp);
static bRC endBackupFile(PluginContext* ctx);
static bRC pluginIO(PluginContext* ctx, io_pkt* io);
static bRC startRestoreFile(PluginContext* ctx, const char* cmd);
static bRC endRestoreFile(PluginContext* ctx);
static bRC createFile(PluginContext* ctx, restore_pkt* rp);
static bRC setFileAttributes(PluginContext* ctx, restore_pkt* rp);
static bRC checkFile(PluginContext* ctx, char* fname);

static bRC parse_plugin_definition(PluginContext* ctx, void* value);
static bRC end_restore_job(PluginContext* ctx);
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
       getPluginValue, setPluginValue, handlePluginEvent, start_backup_file,
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
      auto formatted = std::format(fmt, std::forward<Args>(args)...);
      bareos_core_functions->DebugMessage(ctx, loc.function_name(), loc.line(),
                                          200, "%ls\n", formatted.c_str());
      bareos_core_functions->JobMessage(ctx, loc.function_name(), loc.line(),
                                        type, 0, "%ls\n", formatted.c_str());
    }
  }
  template <class... Args>
  void Log(int type, std::format_string<Args...> fmt, Args&&... args)
  {
    if (bareos_core_functions) {
      auto formatted = std::format(fmt, std::forward<Args>(args)...);
      bareos_core_functions->DebugMessage(ctx, loc.function_name(), loc.line(),
                                          200, "%s\n", formatted.c_str());
      bareos_core_functions->JobMessage(ctx, loc.function_name(), loc.line(),
                                        type, 0, "%s\n", formatted.c_str());
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
#define JFATAL(ctx, ...)                        \
  do {                                          \
    JobLogger{(ctx)}.Log(M_FATAL, __VA_ARGS__); \
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
  // see here: https://learn.microsoft.com/en-us/windows/win32/wmisdk/numbers

  struct ui1 {
    static constexpr VARENUM type = VT_UI1;
    using c_type = uint8_t;

    static c_type from(VARIANT* var)
    {
      if (auto actual_type = V_VT(var); actual_type != type) {
        throw std::runtime_error(std::format("expected variant {}, but got {}",
                                             (int)actual_type, (int)type));
      }

      return V_UI1(var);
    }

    static std::vector<c_type> from_array(SAFEARRAY* arr,
                                          LONG lower_bound,
                                          LONG upper_bound)
    {
      auto size = upper_bound - lower_bound + 1;
      std::vector<c_type> result;
      result.reserve(size);

      for (LONG idx = lower_bound; idx <= upper_bound; ++idx) {
        uint8_t element;
        auto get_res = SafeArrayGetElement(arr, &idx, (void*)&element);
        if (FAILED(get_res)) {
          throw std::runtime_error(
              std::format("could not access safe array at pos {} when trying "
                          "to get {}->{} ({:X})",
                          idx, lower_bound, upper_bound, get_res));
        }

        result.push_back(element);
      }

      return result;
    }

    static VARIANT into(const c_type& var)
    {
      VARIANT result;
      V_VT(&result) = VT_UI1;
      V_UI1(&result) = var;
      return result;
    }
  };

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

    static std::vector<c_type> from_array(SAFEARRAY* arr,
                                          LONG lower_bound,
                                          LONG upper_bound)
    {
      auto size = upper_bound - lower_bound + 1;
      std::vector<c_type> result;
      result.reserve(size);

      for (LONG idx = lower_bound; idx <= upper_bound; ++idx) {
        int32_t element;
        auto get_res = SafeArrayGetElement(arr, &idx, (void*)&element);
        if (FAILED(get_res)) {
          throw std::runtime_error(
              std::format("could not access safe array at pos {} when trying "
                          "to get {}->{} ({:X})",
                          idx, lower_bound, upper_bound, get_res));
        }

        result.push_back(element);
      }

      return result;
    }

    static VARIANT into(const c_type& var)
    {
      VARIANT result;
      V_VT(&result) = VT_I4;
      V_I4(&result) = var;
      return result;
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

    static std::vector<c_type> from_array(SAFEARRAY* arr,
                                          LONG lower_bound,
                                          LONG upper_bound)
    {
      auto size = upper_bound - lower_bound + 1;
      std::vector<c_type> result;
      result.reserve(size);

      for (LONG idx = lower_bound; idx <= upper_bound; ++idx) {
        BSTR element;
        auto get_res = SafeArrayGetElement(arr, &idx, (void*)&element);
        if (FAILED(get_res)) {
          throw std::runtime_error(
              std::format("could not access safe array at pos {} when trying "
                          "to get {}->{} ({:X})",
                          idx, lower_bound, upper_bound, get_res));
        }

        result.emplace_back(String::copy(element));
      }

      return result;
    }

    static VARIANT into(const c_type& var)
    {
      VARIANT result;
      V_VT(&result) = VT_BSTR;
      V_BSTR(&result) = var.get();
      return result;
    }

    static VARIANT into_array(std::span<const c_type> vals)
    {
      CComSafeArray<BSTR> array(vals.size());
      for (size_t i = 0; i < vals.size(); ++i) {
        array.SetAt(i, vals[i].get(), TRUE);
      }

      VARIANT var = {};
      V_VT(&var) = type | VT_ARRAY;
      V_ARRAY(&var) = array.Detach();

      return var;
    }
  };

  struct boolean {
    static constexpr VARENUM type = VT_BOOL;
    using c_type = bool;

    static c_type from(VARIANT* var)
    {
      if (auto actual_type = V_VT(var); actual_type != type) {
        throw std::runtime_error(std::format("expected variant {}, but got {}",
                                             (int)actual_type, (int)type));
      }

      return V_BOOL(var) == VARIANT_TRUE;
    }

    static std::vector<c_type> from_array(SAFEARRAY* arr,
                                          LONG lower_bound,
                                          LONG upper_bound)
    {
      auto size = upper_bound - lower_bound + 1;
      std::vector<c_type> result;
      result.reserve(size);

      for (LONG idx = lower_bound; idx <= upper_bound; ++idx) {
        BOOL element;
        auto get_res = SafeArrayGetElement(arr, &idx, (void*)&element);
        if (FAILED(get_res)) {
          throw std::runtime_error(
              std::format("could not access safe array at pos {} when trying "
                          "to get {}->{} ({:X})",
                          idx, lower_bound, upper_bound, get_res));
        }

        result.push_back(element == VARIANT_TRUE);
      }

      return result;
    }

    static VARIANT into(const c_type& var)
    {
      VARIANT result;
      V_VT(&result) = VT_BOOL;
      V_BOOL(&result) = var ? VARIANT_TRUE : VARIANT_FALSE;
      return result;
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
  DEFINE_CIM_TYPE(uint8, ui1, UINT8);
  DEFINE_CIM_TYPE(uint16, i4, UINT16);
  DEFINE_CIM_TYPE(uint32, i4, UINT32);
  DEFINE_CIM_TYPE(reference, bstr, REFERENCE);
  DEFINE_CIM_TYPE(string, bstr, STRING);
  DEFINE_CIM_TYPE(boolean, boolean, BOOLEAN);

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
                                           (int)CimType::c_type, (int)type));
    }

    return CimType::v_type::from(&param);
  }

  template <typename CimType>
  std::vector<typename CimType::v_type::c_type> get_array(
      const wchar_t* name) const
  {
    VARIANT param;
    CIMTYPE type;

    DBG(L"{}->get({})", fmt_as_ptr(ptr.p), name);
    WMI_CALL(ptr->Get(name, 0, &param, &type, 0));

    if (type != (CimType::c_type | CIM_FLAG_ARRAY)) {
      VariantClear(&param);
      throw std::runtime_error(std::format(
          "when accessing {}->get(??): expected cimtype {} | {}, but got {}",
          fmt_as_ptr(ptr.p), (int)CimType::c_type, (int)CIM_FLAG_ARRAY,
          (int)type));
    }

    if (V_VT(&param) != (CimType::v_type::type | VT_ARRAY)) {
      VariantClear(&param);
      throw std::runtime_error(std::format(
          "when accessing {}->get(??): expected vtype {} | {}, but got {}",
          fmt_as_ptr(ptr.p), (int)CimType::v_type::type, (int)VT_ARRAY,
          (int)V_VT(&param)));
    }

    SAFEARRAY* arr = V_ARRAY(&param);

    if (auto dim = SafeArrayGetDim(arr); dim != 1) {
      VariantClear(&param);
      throw std::runtime_error(std::format(
          "when accessing {}->get(??): expected dim 1, but got dim {}",
          fmt_as_ptr(ptr.p), dim));
    }

    LONG lbound, ubound;
    auto lbound_res = SafeArrayGetLBound(arr, 1, &lbound);
    if (FAILED(lbound_res)) {
      VariantClear(&param);
      throw std::runtime_error(std::format(
          "when accessing {}->get(??): could not get lower bound ({})",
          fmt_as_ptr(ptr.p), lbound_res));
    }
    auto ubound_res = SafeArrayGetUBound(arr, 1, &ubound);
    if (FAILED(ubound_res)) {
      VariantClear(&param);
      throw std::runtime_error(std::format(
          "when accessing {}->get(??): could not get upper bound ({})",
          fmt_as_ptr(ptr.p), ubound_res));
    }

    auto result = CimType::v_type::from_array(arr, lbound, ubound);
    VariantClear(&param);
    return result;
  }

  template <typename CimType>
  void put(const wchar_t* name, const CimType::v_type::c_type& value)
  {
    VARIANT param = CimType::v_type::into(value);
    CIMTYPE type = CimType::c_type;

    DBG(L"{}->put({}):", fmt_as_ptr(ptr.p), name);
    WMI_CALL(ptr->Put(name, 0, &param, type));
  }

  template <typename CimType>
  void put_array(const wchar_t* name,
                 std::span<const typename CimType::v_type::c_type> arr) const
  {
    VARIANT param = CimType::v_type::into_array(arr);

    CIMTYPE type = CimType::c_type | CIM_FLAG_ARRAY;

    DBG(L"{}->put_array({}):", fmt_as_ptr(ptr.p), name);
    WMI_CALL(ptr->Put(name, 0, &param, type));

    // TODO: leak on throw
    VariantClear(&param);
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

    template <typename T> void operator=(T&& value)
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

  ClassObject create_instance() const
  {
    CComPtr<IWbemClassObject> instance;

    TRC(L"{}->SpawnInstance()", name.as_view());
    WMI_CALL(ptr->SpawnInstance(0, &instance));

    return ClassObject{instance};
  }
};

String format_as_xml(const BaseObject& obj)
{
  CComPtr<IWbemContext> context{};
  COM_CALL(
      context.CoCreateInstance(CLSID_WbemContext, NULL, CLSCTX_INPROC_SERVER));

  CComPtr<IWbemObjectTextSrc> text_source{};
  COM_CALL(text_source.CoCreateInstance(CLSID_WbemObjectTextSrc, NULL,
                                        CLSCTX_INPROC_SERVER));
  BSTR text = nullptr;

  COM_CALL(text_source->GetText(0, obj.ptr, WMI_OBJ_TEXT_WMI_DTD_2_0, context,
                                &text));

  return String::wrap(text);
}

struct ComputerSystem : ClassObject {
  static constexpr std::wstring_view class_name{L"CIM_ComputerSystem"};
};

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

  State state() const
  {
    return static_cast<State>(get<cim_type::uint16>(L"JobState"));
  }
};


enum CIMReturnValue : std::int32_t
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

enum ResourceType : std::int32_t
{
  LogicalDisk = 31,
};

static const wchar_t* cim_return_value_name(std::int32_t ret)
{
  switch (ret) {
    case OK: {
      return L"OK";
    } break;
    case JobStarted: {
      return L"JobStarted";
    } break;
    case Failed: {
      return L"Failed";
    } break;
    case AccessDenied: {
      return L"AccessDenied";
    } break;
    case NotSupported: {
      return L"NotSupported";
    } break;
    case StatusIsUnknown: {
      return L"StatusIsUnknown";
    } break;
    case Timeout: {
      return L"Timeout";
    } break;
    case InvalidParameter: {
      return L"InvalidParameter";
    } break;
    case SystemIsInUsed: {
      return L"SystemIsInUsed";
    } break;
    case InvalidStateForThisOperation: {
      return L"InvalidStateForThisOperation";
    } break;
    case IncorrectDataType: {
      return L"IncorrectDataType";
    } break;
    case SystemIsNotAvailable: {
      return L"SystemIsNotAvailable";
    } break;
    case OutOfMemory: {
      return L"OutOfMemory";
    } break;
  }

  return L"Unknown";
}

struct cim_error : public std::exception {
  int32_t error;

  const char* what() const noexcept override { return "cim error"; }
  const wchar_t* known_name() const noexcept
  {
    return cim_return_value_name(error);
  }

  cim_error(int32_t err) : error{err} {}
};

struct Service {
  Service(CComPtr<IWbemServices> ptr) : com_ptr{std::move(ptr)} {}

  std::wstring format_job_err(const Job& job) const
  {
    // error description + GetErrorEx
    // uint16   ErrorCode;
    // string   ErrorDescription;
    // string   ErrorSummaryDescription;

    auto code = job.get<cim_type::uint16>(L"ErrorCode");
    auto desc = job.get<cim_type::string>(L"ErrorDescription");
    auto sum_desc = job.get<cim_type::string>(L"ErrorSummaryDescription");


    auto clz = load_class_by_name(L"Msvm_ConcreteJob");
    auto m_geterr = clz.load_method_by_name(L"GetErrorEx");

    auto result = exec_method(job, m_geterr);

    auto err_msg = std::format(L"JobError({}): {} ({})", code, desc.as_view(),
                               sum_desc.as_view());
    if (result) {
      auto errors = result->get_array<cim_type::string>(L"Errors");

      for (auto& err : errors) {
        err_msg += L"error xml: ---\n\n";
        err_msg += err.as_view();
        err_msg += L"\n---\n";
      }
    }

    return err_msg;
  }

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

  std::optional<CallResult> exec_method(const ClassObject& obj,
                                        const Method& mthd) const
  {
    auto path = obj.path();

    IWbemClassObject* out_params = nullptr;

    TRC(L"calling {}.{}() ...", path.as_view(), mthd.name.as_view());

    WMI_CALL(com_ptr->ExecMethod(path.get(), mthd.name.get(), 0, NULL, NULL,
                                 &out_params, NULL));

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
            DBG(L"Job {} did not complete successfully: Err={} ({:X})",
                job_name.as_view(), format_job_err(job), int32_t{state});
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
            DBG(L"Job {} did not complete successfully: Err={} ({:X})",
                job_name.as_view(), format_job_err(job), int32_t{state});
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
      DBG(L"found {} related entities of class {}", related.size(),
          T::class_name);
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

  std::optional<ComputerSystem> get_vm_by_name(std::wstring_view vm_name) const
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

struct VirtualSystemExportSettingData {
  static constexpr std::wstring_view class_name{
      L"Msvm_VirtualSystemExportSettingData"};

  String as_xml(const Service& srvc) const
  {
    auto clz = srvc.load_class_by_name(class_name);
    auto inst = clz.create_instance();
    // this isnt really correct, but i dont want to move the put() code
    // to a base class

    if (description) {
      inst.put<cim_type::string>(L"Description", description.value());
    }
    if (snapshot_virtual_system_path) {
      inst.put<cim_type::string>(L"SnapshotVirtualSystem",
                                 snapshot_virtual_system_path.value());
    }

    // TODO: excluded virtual hard disk paths

    if (differential_backup_base_path) {
      inst.put<cim_type::string>(L"DifferentialBackupBase",
                                 differential_backup_base_path.value());
    }
    inst.put<cim_type::boolean>(L"DisableDifferentialOfIgnoredStorage",
                                disable_differential_of_ignored_storage);
    inst.put<cim_type::boolean>(L"ExportForLiveMigration",
                                export_for_live_migration);
    inst.put<cim_type::uint8>(L"BackupIntent",
                              static_cast<std::uint8_t>(backup_intent));
    inst.put<cim_type::boolean>(L"CreateVmExportSubdirectory",
                                create_vm_export_subdirectory);
    inst.put<cim_type::boolean>(L"CopyVmStorage", copy_vm_storage);
    inst.put<cim_type::boolean>(L"CopyVmRuntimeInformation",
                                copy_vm_runtime_information);
    inst.put<cim_type::uint8>(L"CopySnapshotConfiguration",
                              to_underlying(copy_snapshot_configuration));
    inst.put<cim_type::uint8>(L"CaptureLiveState",
                              to_underlying(capture_live_state));

    TRC(L"ExportSettings = {}", inst.to_string().as_view());

    return format_as_xml(inst);
  }

  std::optional<String> description{};
  std::optional<String> snapshot_virtual_system_path;
  std::vector<String> excluded_virtual_hard_disk_paths;
  std::optional<String> differential_backup_base_path;
  BackupIntent backup_intent = {};
  CopySnapshotConfiguration copy_snapshot_configuration = {};
  CaptureLiveState capture_live_state = {};
  bool copy_vm_runtime_information = {};
  bool copy_vm_storage = {};
  bool create_vm_export_subdirectory = {};
  bool export_for_live_migration = {};
  bool disable_differential_of_ignored_storage = {};
};

struct PlannedSystem : ComputerSystem {
  static constexpr std::wstring_view class_name{L"Msvm_PlannedComputerSystem"};
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

  void modify_system_settings(const Service& srvc, String new_settings) const
  {
    auto m_modify = clz.load_method_by_name(L"ModifySystemSettings");
    auto params = m_modify.create_parameters();

    params[L"SystemSettings"] = std::move(new_settings);

    auto result = srvc.exec_method(*this, m_modify, params);

    if (!result) {
      throw std::runtime_error("exec_method returned std::nullopt");
    }
  }


  void modify_resource_settings(const Service& srvc,
                                std::span<const String> resource_settings) const
  {
    auto m_modify = clz.load_method_by_name(L"ModifyResourceSettings");
    auto params = m_modify.create_parameters();

    params.put_array<cim_type::string>(L"ResourceSettings", resource_settings);

    auto result = srvc.exec_method(*this, m_modify, params);

    if (!result) {
      throw std::runtime_error("exec_method returned std::nullopt");
    }
  }

  void rename(const Service& srvc,
              ClassObject& obj,
              const String& new_name) const
  {
    obj.put<cim_type::string>(L"ElementName", new_name);
    auto xml = format_as_xml(obj);
    modify_system_settings(srvc, std::move(xml));
  }

  void export_system_definition(
      const Service& srvc,
      const ComputerSystem& vm,
      std::wstring_view directory,
      const VirtualSystemExportSettingData& settings) const
  {
    DBG(L"Exporting {} to {}", vm.path().as_view(), directory);

    auto settings_xml = settings.as_xml(srvc);

    auto m_export = clz.load_method_by_name(L"ExportSystemDefinition");
    auto params = m_export.create_parameters();

    params[L"ExportDirectory"] = String::copy(directory);
    params[L"ExportSettingData"] = settings_xml;
    params[L"ComputerSystem"] = vm;

    auto result = srvc.exec_method(*this, m_export, params);

    if (!result) {
      throw std::runtime_error("exec_method returned std::nullopt");
    }
  }

  PlannedSystem import_system_definition(const Service& srvc,
                                         const String& path_to_def,
                                         bool generate_new_id) const
  {
    DBG(L"Importing {}", path_to_def.as_view());

    auto m_import = clz.load_method_by_name(L"ImportSystemDefinition");
    auto params = m_import.create_parameters();

    params[L"SystemDefinitionFile"] = path_to_def;
    params[L"GenerateNewSystemIdentifier"] = generate_new_id;

    auto result = srvc.exec_method(*this, m_import, params);

    if (!result) {
      throw std::runtime_error("exec_method returned std::nullopt");
    }

    auto system
        = srvc.get_result<PlannedSystem>(result.value(), L"ImportedSystem");

    DBG(L"imported system = {}", system.to_string().as_view());

    return system;
  }

  void validate_planned_system(const Service& srvc,
                               const PlannedSystem& sys) const
  {
    DBG(L"Validating {}", sys.path().as_view());

    auto m_validate = clz.load_method_by_name(L"ValidatePlannedSystem");
    auto params = m_validate.create_parameters();

    params[L"PlannedSystem"] = sys;

    auto result = srvc.exec_method(*this, m_validate, params);

    if (!result) {
      throw std::runtime_error("exec_method returned std::nullopt");
    }
  }

  ComputerSystem realize_planned_system(const Service& srvc,
                                        PlannedSystem system) const
  {
    DBG(L"Realizing {}", system.path().as_view());

    auto m_realize = clz.load_method_by_name(L"RealizePlannedSystem");
    auto params = m_realize.create_parameters();

    params[L"PlannedSystem"] = system;

    auto result = srvc.exec_method(*this, m_realize, params);

    if (!result) {
      throw std::runtime_error("exec_method returned std::nullopt");
    }

    auto new_system
        = srvc.get_result<ComputerSystem>(result.value(), L"ResultingSystem");

    return new_system;
  }

  // NOTE: technically you can destroy any system with this function,
  //  but i dont see a situation currently where we would want to destroy
  //  a live system, so we restrict ourselves to just planned systems for now.
  void destroy_system(const Service& srvc, PlannedSystem system) const
  {
    DBG(L"Destroying {}", system.path().as_view());

    auto m_destroy = clz.load_method_by_name(L"DestroySystem");
    auto params = m_destroy.create_parameters();

    params[L"AffectedSystem"] = system;

    if (!srvc.exec_method(*this, m_destroy, params)) {
      throw std::runtime_error("exec_method returned std::nullopt");
    }
  }

 private:
  VirtualSystemManagementService(ClassObject self, Class clz_)
      : ClassObject{std::move(self)}, clz{std::move(clz_)}
  {
  }

  Class clz;
};

struct ReferencePoint : ClassObject {
  static constexpr std::wstring_view class_name{
      L"Msvm_VirtualSystemReferencePoint"};
};

class VirtualSystemSnapshotService;

struct Snapshot : ClassObject {
  static constexpr std::wstring_view class_name{
      L"CIM_VirtualSystemSettingData"};

  // const Service* srvc;
  // const VirtualSystemSnapshotService* snapshot_srvc;

  ~Snapshot()
  {
    // todo: destroy the snapshot here if it is still valid
    // if (still_exists) {
    //   snapshot_srvc->destroy_snapshot(*srvc, std::move(this));
    // }
  }
};

struct VirtualSystemSnapshotSettingData {
  static constexpr std::wstring_view class_name{
      L"Msvm_VirtualSystemSnapshotSettingData"};

  enum class ConsistencyLevel : std::int32_t
  {
    Unknown = 0,
    ApplicationConsistent = 1,
    CrashConsistent = 2,
  };

  enum class GuestBackupType : std::int32_t
  {
    Undefined = 0,
    Full = 1,
    Copy = 2,
  };

  String as_xml(const Service& srvc) const
  {
    auto clz = srvc.load_class_by_name(class_name);
    auto inst = clz.create_instance();
    // this isnt really correct, but i dont want to move the put() code
    // to a base class
    auto as_pack = ParameterPack{std::move(inst)};

    as_pack[L"ConsistencyLevel"] = to_underlying(consistency_level);
    as_pack[L"GuestBackupType"] = to_underlying(guest_backup_type);
    as_pack[L"IgnoreNonSnapshottableDisks"] = ignore_non_snapshottable_disks;

    return WMI::format_as_xml(as_pack);
  }

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
    params[L"SnapshotSettings"] = settings.as_xml(srvc);
    params[L"SnapshotType"] = to_underlying(type);

    auto result = srvc.exec_method(*this, m_create_snapshot, params);

    if (!result) {
      throw std::runtime_error("exec_method returned std::nullopt");
    }

    auto snapshot
        = srvc.get_result<Snapshot>(result.value(), L"ResultingSnapshot");

    return snapshot;
  };

  void destroy_snapshot(const Service& srvc, Snapshot snapshot) const
  {
    auto m_destroy_snapshot = clz.load_method_by_name(L"DestroySnapshot");
    auto params = m_destroy_snapshot.create_parameters();

    params[L"AffectedSnapshot"] = snapshot;

    auto result = srvc.exec_method(*this, m_destroy_snapshot, params);
  };

  ReferencePoint convert_to_reference_point(const Service& srvc,
                                            Snapshot snapshot) const
  {
    auto m_convert_snapshot
        = clz.load_method_by_name(L"ConvertToReferencePoint");
    auto params = m_convert_snapshot.create_parameters();

    params[L"AffectedSnapshot"] = snapshot;

    auto result = srvc.exec_method(*this, m_convert_snapshot, params);

    if (!result) {
      throw std::runtime_error("exec_method returned std::nullopt");
    }

    // snapshot.release();

    auto refpoint = srvc.get_result<ReferencePoint>(result.value(),
                                                    L"ResultingReferencePoint");

    return refpoint;
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

struct restore_object {
  std::string name;
  int index;
  std::string content;
};

struct disk_info {
  std::wstring path;
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

  struct backup {
    WMI::VirtualSystemManagementService system_srvc;
    WMI::VirtualSystemSnapshotService snapshot_srvc;


    struct prepared_backup {
      std::wstring vm_name;
      std::wstring tmp_dir;
      WMI::Snapshot vm_snapshot;

      // prepared_backup(std::wstring name,
      //                 std::wstring dir,
      //                 WMI::Snapshot snapshot)
      //   : vm_name{std::move(name)}
      //   , tmp_dir{std::move(dir)}
      //   , vm_snapshot{std::move(snapshot)}
      // {}

      std::vector<std::wstring> files_to_backup{};
      std::vector<std::wstring> disks_to_backup{};
      std::vector<restore_object> restore_objects{};
      bool error = false;

      std::optional<HANDLE> disk_to_backup;
      std::size_t disk_offset;
      std::size_t disk_full_size;

      std::optional<restore_object> current_object{};

      bool finished() const
      {
        // the last thing we do is convert the snapshot into
        // a reference point, deleting the snapshot in the process
        // so while this snapshot is still legal, we need to keep going
        return vm_snapshot.ptr.p == nullptr;
      }
    };

    std::variant<std::monostate, prepared_backup> state;
  };

  struct restore {
    WMI::VirtualSystemManagementService system_srvc;

    std::wstring tmp_dir;

    std::optional<HANDLE> hndl;

    std::vector<WMI::String> restored_definition_files;

    std::optional<HANDLE> disk_handle;
    std::size_t current_offset;

    std::unordered_map<std::string, disk_info> disk_map;
  };

  int jobid;
  std::variant<std::monostate, backup, restore> current_state{};

  std::wstring directory;

  std::string current_path;

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

    int jobid;
    bareos_core_functions->getBareosValue(ctx, filedaemon::bVarJobId, &jobid);
    p_ctx->jobid = jobid;

    // Only register the events we are really interested in.
    bareos_core_functions->registerBareosEvents(
        ctx, 8, bEventLevel, bEventRestoreCommand, bEventBackupCommand,
        bEventPluginCommand, bEventEndRestoreJob, bEventNewPluginOptions,
        bEventStartBackupJob, bEventStartRestoreJob, bEventEndRestoreJob);

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

  p_ctx->current_state.emplace<plugin_ctx::backup>(
      std::move(system_mgmt.value()), std::move(system_snap.value()));

  return true;
}

static std::wstring make_temp_dir(uint64_t jobid)
{
  auto path_len = GetTempPathW(0, nullptr);

  if (path_len == 0) { throw werror("retrieve the users temp path"); }

  std::wstring path;
  path.resize(path_len);

  auto written_len = GetTempPathW(path.size(), path.data());
  path.resize(written_len);

  if (path.back() != L'\\') { path += L'\\'; }

  path += L"Bareos-";
  path += std::to_wstring(jobid);

  if (!CreateDirectoryW(path.c_str(), 0)) {
    throw werror("create a temporary directory");
  }

  DBG(L"temp path = {}", path);

  return path;
}

static bool start_restore_job(PluginContext* ctx)
{
  auto* p_ctx = plugin_ctx::get(ctx);
  if (!p_ctx) { return false; }

  auto& srvc = p_ctx->virt_service;

  std::optional system_mgmt
      = WMI::VirtualSystemManagementService::find_instance(srvc);
  if (!system_mgmt) { return false; }

  std::wstring dir = make_temp_dir(p_ctx->jobid);
  p_ctx->current_state.emplace<plugin_ctx::restore>(
      std::move(system_mgmt.value()), std::move(dir));

  return true;
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
      return parse_plugin_definition(ctx, value);
    } break;
    case bEventBackupCommand: {
      return parse_plugin_definition(ctx, value);
    } break;
    case bEventPluginCommand: {
      return parse_plugin_definition(ctx, value);
    } break;
    case bEventEndRestoreJob: {
      return end_restore_job(ctx);
    } break;
    case bEventNewPluginOptions: {
      return parse_plugin_definition(ctx, value);
    } break;
    case bEventStartBackupJob: {
      DBGC(ctx, "start backup job");
      if (!start_backup_job(ctx)) { return bRC_Error; }
      return bRC_OK;
    } break;
    case bEventStartRestoreJob: {
      DBGC(ctx, "start restore job");
      if (!start_restore_job(ctx)) { return bRC_Error; }
      return bRC_OK;
    } break;
    default: {
      DBGC(ctx, L"unknown event type {}", event->eventType);
      return bRC_Error;
    } break;
  }
}

static std::wstring extract_disk_path(const WMI::ClassObject& vhd_setting)
{
  auto paths = vhd_setting.get_array<WMI::cim_type::string>(L"HostResource");
  if (paths.size() != 1) {
    // todo: we should include the format of vhd_setting here
    //   I.e. this should be a different type!
    throw std::runtime_error(
        std::format("bad HostResource: expected exactly one string, but got {}",
                    paths.size()));
  }

  DBG(L"found harddisk path {}", paths[0].as_view());

  return std::wstring{paths[0].as_view()};
}

static void set_disk_path(const WMI::ClassObject& vhd_setting,
                          std::wstring_view path)
{
  WMI::String args[] = {WMI::String::copy(path)};
  vhd_setting.put_array<WMI::cim_type::string>(L"HostResource", args);
  DBG(L"updated harddisk path to {}", path);
}

static std::vector<std::wstring> get_disk_paths_of_snapshot(
    const WMI::Service& srvc,
    const WMI::Snapshot& snap)
{
  auto settings = srvc.get_related_of_class(
      snap.path().as_view(), L"Msvm_StorageAllocationSettingData",
      L"Msvm_VirtualSystemSettingDataComponent");

  std::vector<std::wstring> paths;
  paths.reserve(settings.size());

  for (auto& setting : settings) {
    if (setting.get<WMI::cim_type::uint16>(L"ResourceType")
        != WMI::ResourceType::LogicalDisk) {
      DBG(L"Skipping {} because it has a bad resource type",
          setting.path().as_view());
      continue;
    }
    if (setting.get<WMI::cim_type::string>(L"ResourceSubType").as_view()
        != std::wstring_view{L"Microsoft:Hyper-V:Virtual Hard Disk"}) {
      DBG(L"Skipping {} because it has a bad resource sub type",
          setting.path().as_view());
      continue;
    }

    TRC(L"found setting {}", setting.to_string().as_view());

    paths.emplace_back(extract_disk_path(setting));
  }

  return paths;
}

static std::vector<std::wstring> get_files_in_dir(std::wstring_view dir)
{
  std::vector<std::wstring> files;
  std::vector<std::wstring> directories;

  directories.emplace_back(dir);

  while (directories.size() > 0) {
    std::wstring current_dir = std::move(directories.back());
    directories.pop_back();

    TRC(L"entering directory '{}' ...", current_dir);

    std::wstring search_path = current_dir + L"\\*";

    WIN32_FIND_DATAW data;
    HANDLE finder = FindFirstFileW(search_path.c_str(), &data);

    if (finder == INVALID_HANDLE_VALUE) {
      throw std::runtime_error(
          std::format("could not enter generated directory {}",
                      utf16_to_utf8(current_dir)));
    }

    do {
      std::wstring_view entry = data.cFileName;
      TRC(L"found '{}' ...", entry);

      if (entry == L"." || entry == L"..") { continue; }

      std::wstring full_path{current_dir};
      full_path += L"\\";
      full_path += entry;

      if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
        directories.emplace_back(std::move(full_path));
      } else {
        files.emplace_back(std::move(full_path));
      }
    } while (FindNextFileW(finder, &data));

    FindClose(finder);
  }

  return files;
}

restore_object get_info(PluginContext* ctx,
                        std::wstring_view vm_name,
                        const std::vector<std::wstring>& found_disks)
{
  restore_object obj;
  obj.name = utf16_to_utf8(vm_name);
  obj.index = 0;

  json_t* content = json_object();
  {
    auto* disks = json_array();
    for (auto& disk : found_disks) {
      json_array_append_new(disks, json_string(utf16_to_utf8(disk).c_str()));
    }
    json_object_set_new(content, "disks", disks);
  }

  char* formatted = json_dumps(content, JSON_COMPACT);
  obj.content = formatted;
  free(formatted);
  json_decref(content);
  return std::move(obj);
}

static bool prepare_backup(PluginContext* ctx, std::wstring_view vm_name)
{
  TRCC(ctx, L"start backup file");
  auto* p_ctx = plugin_ctx::get(ctx);

  auto& bstate = std::get<plugin_ctx::backup>(p_ctx->current_state);

  const auto& srvc = p_ctx->virt_service;
  const auto& system_srvc = bstate.system_srvc;
  const auto& snapshot_srvc = bstate.snapshot_srvc;

  // do this first, so we dont create a snapshot for nothing
  std::wstring dir = make_temp_dir(p_ctx->jobid);

  auto vm = srvc.get_vm_by_name(vm_name);
  if (!vm) {
    JFATAL(ctx, L"there are mulitple vms named {}.  Cannot continue.", vm_name);
    return false;
  }
  auto snapshot = snapshot_srvc.create_snapshot(
      srvc, vm.value(), p_ctx->snapshot_settings,
      WMI::VirtualSystemSnapshotService::SnapshotType::RecoverySnapshot);

  system_srvc.rename(
      srvc, snapshot,
      WMI::String::copy(std::format(L"Bareos - {}", p_ctx->jobid)));

  WMI::VirtualSystemExportSettingData export_settings = {
      .snapshot_virtual_system_path = snapshot.path(),
      .backup_intent = WMI::BackupIntent::Merge,
      .copy_snapshot_configuration
      = WMI::CopySnapshotConfiguration::ExportOneSnapshotForBackup,
      .capture_live_state = WMI::CaptureLiveState::CrashConsistent,
      .copy_vm_runtime_information = true,
      .copy_vm_storage = false,
      .create_vm_export_subdirectory = false,
  };

  system_srvc.export_system_definition(srvc, vm.value(), dir, export_settings);

  auto& prepared = bstate.state.emplace<plugin_ctx::backup::prepared_backup>(
      std::wstring{vm_name}, dir, std::move(snapshot));

  prepared.files_to_backup = get_files_in_dir(dir);
  prepared.disks_to_backup
      = get_disk_paths_of_snapshot(srvc, prepared.vm_snapshot);

  // if (full) {
  prepared.restore_objects.emplace_back(
      get_info(ctx, vm_name, prepared.disks_to_backup));
  // }

  return true;
}

static void prepare_restore_object(PluginContext* ctx)
{
  TRCC(ctx, L"prepare restore object");
  auto* p_ctx = plugin_ctx::get(ctx);
  auto& bstate = std::get<plugin_ctx::backup>(p_ctx->current_state);
  auto& prepared = std::get<plugin_ctx::backup::prepared_backup>(bstate.state);

  const auto& srvc = p_ctx->virt_service;
  const auto& system_srvc = bstate.system_srvc;
  const auto& snapshot_srvc = bstate.snapshot_srvc;

  auto refpoint = snapshot_srvc.convert_to_reference_point(
      srvc, std::move(prepared.vm_snapshot));
  DBGC(ctx, L"refpoint = {}", refpoint.path().as_view());
}

// maybe we should add the cluster name here somehow ?
std::string create_vm_path(std::wstring_view vm_name,
                           std::wstring_view sub_dir,
                           std::wstring_view path)
{
  auto last_slash = path.find_last_of(L"\\/");
  if (last_slash == path.npos) {
    // if there somehow is no slash in the path, then we just take
    // everything.
    last_slash = 0;
  }

  std::wstring new_path = std::format(L"@hyper-v@/{}/{}/{}", vm_name, sub_dir,
                                      path.substr(last_slash + 1));

  std::replace(std::begin(new_path), std::end(new_path), L'\\', L'/');
  return utf16_to_utf8(new_path);
}

// Start the backup of a specific file
static bRC start_backup_file(PluginContext* ctx, save_pkt* sp)
{
  TRCC(ctx, L"start backup file");
  auto* p_ctx = plugin_ctx::get(ctx);
  if (!p_ctx) { return bRC_Error; }

  try {
    if (std::get_if<std::monostate>(&p_ctx->current_state)) {
      DBG(L"Backup is not started.  Doing so now...");
      if (!start_backup_job(ctx)) { return bRC_Error; }
    }

    auto& bstate = std::get<plugin_ctx::backup>(p_ctx->current_state);
    if (!std::get_if<plugin_ctx::backup::prepared_backup>(&bstate.state)) {
      DBG(L"Backup is not prepared.  Doing so now...");

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

      // {
      //   json_t* val = json_object_get(p_ctx->config, "directory");
      //   if (!val) {
      //     JERR(ctx, "No 'directory' set in config file");
      //     return bRC_Error;
      //   }
      //   const char* directory = json_string_value(val);
      //   if (!val) {
      //     JERR(ctx, "'directory' is not a string in config file");
      //     return bRC_Error;
      //   }

      //   p_ctx->directory = utf8_to_utf16(directory);
      // }

      if (!prepare_backup(ctx, wname)) { return bRC_Error; }
    }

    auto& prepared
        = std::get<plugin_ctx::backup::prepared_backup>(bstate.state);

    if (prepared.files_to_backup.size() > 0) {
      auto& path = prepared.files_to_backup.back();
      DBGC(ctx, L"transforming {} ...", path);
      p_ctx->current_path = create_vm_path(prepared.vm_name, L"config", path);
      DBGC(ctx, "into {}!", p_ctx->current_path);
      auto now = time(NULL);
      sp->fname = p_ctx->current_path.data();
      sp->type = FT_REG;

      // todo: fix these
      sp->statp.st_mode = 0700 | S_IFREG;
      sp->statp.st_ctime = now;
      sp->statp.st_mtime = now;
      sp->statp.st_atime = now;
      sp->statp.st_size = -1;
      sp->statp.st_blksize = 4096;
      sp->statp.st_blocks = 1;

      return bRC_OK;
    }

    DBGC(ctx, L"All files were backed up");

    if (prepared.disks_to_backup.size() > 0) {
      auto& path = prepared.disks_to_backup.back();
      DBGC(ctx, L"transforming {} ...", path);
      p_ctx->current_path = create_vm_path(prepared.vm_name, L"disks", path);
      DBGC(ctx, "into {}!", p_ctx->current_path);

      {
        HANDLE disk_handle = INVALID_HANDLE_VALUE;

        VIRTUAL_STORAGE_TYPE vst = {
            .DeviceId = VIRTUAL_STORAGE_TYPE_DEVICE_UNKNOWN,
            .VendorId = VIRTUAL_STORAGE_TYPE_VENDOR_UNKNOWN,
        };

        OPEN_VIRTUAL_DISK_PARAMETERS params = {};
        params.Version = OPEN_VIRTUAL_DISK_VERSION_2;
        params.Version2.ReadOnly = TRUE;

        auto res = OpenVirtualDisk(&vst, path.c_str(), VIRTUAL_DISK_ACCESS_NONE,
                                   OPEN_VIRTUAL_DISK_FLAG_NONE, &params,
                                   &disk_handle);

        if (res != ERROR_SUCCESS) {
          JERR(ctx, L"could not open disk {}: Err={} ({:X})", path,
               format_win32_error(res), res);
          return bRC_Error;
        }

        prepared.disk_to_backup = disk_handle;

        GET_VIRTUAL_DISK_INFO disk_info = {
            .Version = GET_VIRTUAL_DISK_INFO_SIZE,
        };
        ULONG VirtualDiskInfoSize = sizeof(disk_info);
        ULONG SizeOut = 0;
        auto disk_res = GetVirtualDiskInformation(
            disk_handle, &VirtualDiskInfoSize, &disk_info, &SizeOut);
        if (disk_res != ERROR_SUCCESS) {
          CloseHandle(disk_handle);
          // LOG(L"GetVirtualDiskInfo({}) failed.  Err={} ({:X})",
          // hdd_path.as_view(),
          //     WMI::ErrorString(disk_res), (uint64_t)disk_res);
          JERR(ctx, L"could not retrieve info for disk {}", path);
          return bRC_Error;
        }

        DBGC(ctx,
             L"Size = {{ VirtualSize = {}, PhysicalSize = {}, BlockSize = {}, "
             L"SectorSize = {} }}",
             disk_info.Size.VirtualSize, disk_info.Size.PhysicalSize,
             disk_info.Size.BlockSize, disk_info.Size.SectorSize);
        sp->statp.st_size = disk_info.Size.VirtualSize;
        sp->statp.st_blksize = disk_info.Size.BlockSize;
        sp->statp.st_blocks = disk_info.Size.SectorSize;

        prepared.disk_full_size = disk_info.Size.VirtualSize;
        prepared.disk_offset = 0;
      }

      auto now = time(NULL);
      sp->fname = p_ctx->current_path.data();
      sp->type = FT_REG;

      // todo: fix these
      sp->statp.st_mode = 0700 | S_IFREG;
      sp->statp.st_ctime = now;
      sp->statp.st_mtime = now;
      sp->statp.st_atime = now;

      return bRC_OK;
    }

    DBGC(ctx, L"All disks were backed up");

    if (prepared.restore_objects.size() > 0) {
      prepared.current_object = std::move(prepared.restore_objects.back());
      prepared.restore_objects.pop_back();

      sp->type = FT_RESTORE_FIRST;
      sp->object = prepared.current_object->content.data();
      sp->object_len = prepared.current_object->content.size();

      sp->object_name = prepared.current_object->name.data();
      sp->index = prepared.current_object->index;

      return bRC_OK;
    }


    if (prepared.vm_snapshot.ptr.p) {
      // the snapshot still exists, so we need to create a reference point now
      // and send it via a restore object

      prepare_restore_object(ctx);
    }

    DBGC(ctx, L"reference point was already created");

    return bRC_Stop;

  } catch (const win_error& err) {
    DBGC(ctx, L"caught win error.  Err={} ({:X})", err.err_str(),
         err.err_num());
    comReportError(ctx, err.err_num());
    return bRC_Error;
  } catch (const WMI::cim_error& err) {
    DBGC(ctx, L"caught cim error.  Err={}/{} ({})", err.known_name(),
         WMI::format_error(err.error), static_cast<uint32_t>(err.error));
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
static bRC endBackupFile(PluginContext* ctx)
{
  TRCC(ctx, "end backup file");
  auto* p_ctx = plugin_ctx::get(ctx);
  if (!p_ctx) { return bRC_Error; }

  auto* bstate = std::get_if<plugin_ctx::backup>(&p_ctx->current_state);
  if (!bstate) {
    DBGC(ctx, "no backup was started, so there is nothing to do here");
    return bRC_OK;
  }
  if (auto* prepared
      = std::get_if<plugin_ctx::backup::prepared_backup>(&bstate->state)) {
    if (prepared->error) {
      // delete the snapshot here ?
      // The destructor should just do it on its own...
      TRCC(ctx, "prepared->error");
      bstate->state.emplace<std::monostate>();
      return bRC_Error;
    }

    if (!prepared->finished()) {
      if (prepared->files_to_backup.size() > 0) {
        DBGC(ctx, L"finishing up {}", prepared->files_to_backup.back());
        prepared->files_to_backup.pop_back();
      } else if (prepared->disks_to_backup.size() > 0) {
        DBGC(ctx, L"finishing up {}", prepared->disks_to_backup.back());
        prepared->disks_to_backup.pop_back();
      }
      TRCC(ctx, "more work");
      return bRC_More;
    } else {
      TRCC(ctx, "prepared files done");
      bstate->state.emplace<std::monostate>();
    }
  }
  /* We would return bRC_More if we wanted start_backup_file to be called again
   * to backup another file */

  TRCC(ctx, "no more files left");
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

static bRC pluginBackupIO(PluginContext* ctx,
                          plugin_ctx* p_ctx,
                          plugin_ctx::backup& bstate,
                          io_pkt* io)
{
  TRCC(ctx, "backup plugin io ({})", io->func);
  auto& prepared = std::get<plugin_ctx::backup::prepared_backup>(bstate.state);

  switch (io->func) {
    case filedaemon::IO_OPEN: {
      if (prepared.files_to_backup.size() > 0) {
        // we are backing up a normal file
        auto& current_file = prepared.files_to_backup.back();

        HANDLE h = [&] {
          if (io->flags & O_WRONLY) {
            return CreateFileW(current_file.c_str(), GENERIC_WRITE,
                               FILE_SHARE_WRITE, NULL, OPEN_ALWAYS,
                               FILE_ATTRIBUTE_NORMAL, NULL);
          } else {
            return CreateFileW(current_file.c_str(), GENERIC_READ,
                               FILE_SHARE_READ, NULL, OPEN_EXISTING,
                               FILE_ATTRIBUTE_NORMAL, NULL);
          }
        }();

        io->hndl = h;
        io->status = IoStatus::do_io_in_core;
        return bRC_OK;
      }

      if (prepared.disks_to_backup.size() > 0) {
        // we are backing up a disk
        if (!prepared.disk_to_backup) {
          JERR(ctx,
               L"can not open disk {} because it was not properly prepared",
               prepared.disks_to_backup.back());
          return bRC_Error;
        }

        ATTACH_VIRTUAL_DISK_PARAMETERS attach_params
            = {.Version = ATTACH_VIRTUAL_DISK_VERSION_1, .Version1 = {}};
        auto attach_res
            = AttachVirtualDisk(prepared.disk_to_backup.value(), NULL,
                                ATTACH_VIRTUAL_DISK_FLAG_NO_LOCAL_HOST
                                    | ATTACH_VIRTUAL_DISK_FLAG_READ_ONLY,
                                0, &attach_params, 0);
        if (FAILED(attach_res)) {
          JERR(ctx, L"could not attach disk {} for reading",
               prepared.disks_to_backup.back());
          // LOG(L"AttachVirtualDisk() failed.  Err={} ({:X})",
          //     WMI::ErrorString(attach_res), (uint32_t)attach_res);
          return bRC_Error;
        }

        io->status = 0;
        return bRC_OK;
      }
      return bRC_Error;
    } break;
    case filedaemon::IO_READ: {
      // we only get here when backing up a disk
      if (!prepared.disk_to_backup) {
        DBGC(ctx, L"bad read!");
        return bRC_Error;
      }

      OVERLAPPED overlapped = {};

      auto current_offset = prepared.disk_offset;
      overlapped.Offset = DWORD{current_offset & 0xFFFFFFFF};
      overlapped.OffsetHigh = DWORD{(current_offset >> 32) & 0xFFFFFFFF};

      size_t possible_end = current_offset + io->count;
      if (possible_end > prepared.disk_full_size) {
        possible_end = prepared.disk_full_size;
      }

      auto diff = possible_end - current_offset;

      if (diff == 0) {
        TRCC(ctx, "disk is done");
        io->status = 0;
        return bRC_OK;
      }

      DWORD bytes_read = 0;
      if (!ReadFile(prepared.disk_to_backup.value(), io->buf, diff, &bytes_read,
                    &overlapped)) {
        DWORD error = GetLastError();

        if (error == ERROR_IO_PENDING) {
          if (GetOverlappedResult(prepared.disk_to_backup.value(), &overlapped,
                                  &bytes_read, TRUE)) {
            goto read_ok;
          }

          error = GetLastError();
        }

        JERR(ctx, L"encountered error while trying to read disk {}: {}",
             prepared.disks_to_backup.back(), format_win32_error(error));
        return bRC_Error;
      }
    read_ok:

      TRCC(ctx, L"read {} bytes from offset {}", bytes_read,
           prepared.disk_offset);
      prepared.disk_offset += bytes_read;
      io->status = bytes_read;
      return bRC_OK;

    } break;
    case filedaemon::IO_WRITE: {
      return bRC_Error;
    } break;
    case filedaemon::IO_CLOSE: {
      if (prepared.files_to_backup.size() > 0) {
        auto& current_file = prepared.files_to_backup.back();

        auto res = CloseHandle(io->hndl);

        io->hndl = INVALID_HANDLE_VALUE;
        if (FAILED(res)) {
          io->status = -1;
        } else {
          io->status = 0;
        }

        // we dont need these files anymore, so we just delete them
        DeleteFileW(current_file.c_str());

        return bRC_OK;
      }
      if (prepared.disks_to_backup.size() > 0) {
        // we are backing up a disk
        if (prepared.disk_to_backup) {
          DetachVirtualDisk(prepared.disk_to_backup.value(),
                            DETACH_VIRTUAL_DISK_FLAG_NONE, 0);
          CloseHandle(prepared.disk_to_backup.value());
          prepared.disk_to_backup.reset();
        }
      }
      return bRC_Error;
    } break;
    default: {
      JERR(ctx, "unknown plugin io command {}", io->func);
      return bRC_Error;
    } break;
  }
}

static bool WriteFile_All(HANDLE handle,
                          std::size_t offset,
                          std::span<const char> to_write)
{
  if (to_write.size() > std::numeric_limits<DWORD>::max()) {
    DBG(L"WriteFile_All() failed.  Cannot write more than {} bytes at once",
        std::numeric_limits<DWORD>::max());
    return false;
  }
  DWORD bytes_to_write = static_cast<DWORD>(to_write.size());
  DWORD bytes_written = 0;

  const char* current = to_write.data();

  while (bytes_to_write > 0) {
    OVERLAPPED overlapped = {};
    overlapped.Offset = DWORD{offset & 0xFF'FF'FF'FF};
    overlapped.OffsetHigh = DWORD{(offset >> 32) & 0xFF'FF'FF'FF};

    DWORD bytes_written_this_call = 0;
    if (!WriteFile(handle, current, bytes_to_write, &bytes_written_this_call,
                   &overlapped)) {
      auto write_res = GetLastError();

      if (write_res == ERROR_IO_PENDING) {
        TRC(L"WriteFile({}, {}, {}) -> async", fmt_as_ptr(handle),
            offset + bytes_written, to_write.size() - bytes_written);

        if (GetOverlappedResult(handle, &overlapped, &bytes_written_this_call,
                                TRUE)) {
          goto write_ok;
        } else {
          write_res = GetLastError();
        }
      }

      DBG(L"WriteFile({}, {}) failed.  Err={} ({:X})",
          current - to_write.data(), bytes_to_write,
          format_win32_error(write_res), (uint32_t)write_res);
      return false;
    }

  write_ok:

    bytes_to_write -= bytes_written_this_call;
    current += bytes_written_this_call;
  }

  return true;
}

static bRC pluginRestoreIO(PluginContext* ctx,
                           plugin_ctx* p_ctx,
                           plugin_ctx::restore& restore_ctx,
                           io_pkt* io)
{
  TRCC(ctx, "restore plugin io ({})", io->func);

  switch (io->func) {
    case filedaemon::IO_OPEN: {
      if (restore_ctx.hndl) {
        DBGC(ctx, "preparing file handle {}", restore_ctx.hndl.value());
        io->hndl = restore_ctx.hndl.value();
        io->status = IoStatus::do_io_in_core;
        restore_ctx.hndl.reset();
        return bRC_OK;
      } else if (restore_ctx.disk_handle) {
        DBGC(ctx, "preparing disk handle {}", restore_ctx.disk_handle.value());

        ATTACH_VIRTUAL_DISK_PARAMETERS attach_params
            = {.Version = ATTACH_VIRTUAL_DISK_VERSION_1, .Version1 = {}};
        auto attach_res = AttachVirtualDisk(
            restore_ctx.disk_handle.value(), NULL,
            ATTACH_VIRTUAL_DISK_FLAG_NO_LOCAL_HOST, 0, &attach_params, 0);
        if (FAILED(attach_res)) {
          DBGC(ctx, L"bad attach ");
          JERR(ctx, "could not attach disk {} ({}) for writing", io->fname,
               restore_ctx.disk_handle.value());
          io->status = IoStatus::error;
          return bRC_Error;
        }

        DBGC(ctx, L"good attach");

        io->hndl = INVALID_HANDLE_VALUE;
        io->status = IoStatus::success;
        return bRC_OK;
      } else {
        JERR(ctx, "could not open file {}, as no file was prepared", io->fname);
        io->status = -1;
        return bRC_Error;
      }
    } break;
    case filedaemon::IO_READ: {
      DBGC(ctx, L"bad read!");
      return bRC_Error;
    } break;
    case filedaemon::IO_WRITE: {
      if (!restore_ctx.disk_handle) {
        JERR(ctx, "can not write to disk {} as it was not prepared", io->fname);
        io->status = IoStatus::error;
        return bRC_Error;
      }

      if (io->count < 0) {
        JERR(ctx, "can not write {} bytes to disk {}", io->count, io->fname);
        io->status = IoStatus::error;
        return bRC_Error;
      }

      if (!WriteFile_All(restore_ctx.disk_handle.value(),
                         restore_ctx.current_offset,
                         {io->buf, static_cast<size_t>(io->count)})) {
        io->status = IoStatus::error;
        return bRC_Error;
      }

      restore_ctx.current_offset += io->count;

      io->status = io->count;
      return bRC_OK;
    } break;
    case filedaemon::IO_CLOSE: {
      if (restore_ctx.disk_handle) {
        auto hndl = restore_ctx.disk_handle.value();
        DBGC(ctx, "closing disk handle {}", hndl);

        DetachVirtualDisk(hndl, DETACH_VIRTUAL_DISK_FLAG_NONE, 0);
        CloseHandle(hndl);
        restore_ctx.disk_handle.reset();
      } else {
        DBGC(ctx, "closing file handle {}", io->hndl);
        CloseHandle(io->hndl);
      }
      io->hndl = INVALID_HANDLE_VALUE;
    } break;
    default: {
      JERR(ctx, "unknown plugin io command {}", io->func);
      return bRC_Error;
    } break;
  }
  return bRC_Error;
}

static bRC pluginIO(PluginContext* ctx, io_pkt* io)
{
  auto* p_ctx = plugin_ctx::get(ctx);
  if (!p_ctx) { return bRC_Error; }

  TRCC(ctx, "plugin io ({})", io->func);

  return std::visit(
      [&](auto&& val) {
        using T = std::decay_t<decltype(val)>;

        if constexpr (std::is_same_v<T, plugin_ctx::restore>) {
          return pluginRestoreIO(ctx, p_ctx, val, io);
        } else if constexpr (std::is_same_v<T, plugin_ctx::backup>) {
          return pluginBackupIO(ctx, p_ctx, val, io);
        } else if constexpr (std::is_same_v<T, std::monostate>) {
          JERR(ctx, "instructed to do plugin io, but plugin is not setup");
          return bRC_Error;
        } else {
          static_assert(!std::is_same_v<T, T>, "type not handled");
        }
      },
      p_ctx->current_state);
}

static bRC end_restore_job(PluginContext* ctx)
{
  TRCC(ctx, "end restore job");
  auto* p_ctx = plugin_ctx::get(ctx);
  if (!p_ctx) { return bRC_Error; }

  auto* restore_ctx = std::get_if<plugin_ctx::restore>(&p_ctx->current_state);
  if (!restore_ctx) {
    JERR(ctx, "instructed to end restore job while not in restore mode");
    return bRC_Error;
  }

  const auto& srvc = p_ctx->virt_service;
  const auto& system_srvc = restore_ctx->system_srvc;

  for (auto& definition_file : restore_ctx->restored_definition_files) {
    bool generate_new_id = true;

    try {
      auto planned_system = system_srvc.import_system_definition(
          srvc, definition_file, generate_new_id);

      // TODO: this isnt great,  it would be better if this was somehow
      //  build into the type.
      struct SystemDestroyer {
        const WMI::Service& srvc;
        const WMI::VirtualSystemManagementService& system_srvc;
        WMI::PlannedSystem* system;

        void release() { system = nullptr; }

        ~SystemDestroyer()
        {
          if (system) { system_srvc.destroy_system(srvc, std::move(*system)); }
        }
      };

      SystemDestroyer destroyer{srvc, system_srvc, &planned_system};


      auto settings = srvc.get_related_of_class(
          planned_system.path().as_view(), L"Msvm_VirtualSystemSettingData",
          L"Msvm_SettingsDefineState");

      DBGC(ctx, "found {} settings", settings.size());

      for (auto& setting : settings) {
        TRCC(ctx, L"setting = {}", setting.to_string().as_view());

        auto disk_settings = srvc.get_related_of_class(
            setting.path().as_view(), L"Msvm_StorageAllocationSettingData",
            L"Msvm_VirtualSystemSettingDataComponent");


        DBGC(ctx, "found {} disk settings", disk_settings.size());

        for (auto& disk_setting : disk_settings) {
          TRCC(ctx, L"disk setting = {}", disk_setting.to_string().as_view());

          if (disk_setting.get<WMI::cim_type::uint16>(L"ResourceType")
              != WMI::ResourceType::LogicalDisk) {
            DBGC(ctx, L"Skipping {} because it has a bad resource type",
                 disk_setting.path().as_view());
            continue;
          }
          if (disk_setting.get<WMI::cim_type::string>(L"ResourceSubType")
                  .as_view()
              != std::wstring_view{L"Microsoft:Hyper-V:Virtual Hard Disk"}) {
            DBGC(ctx, L"Skipping {} because it has a bad resource sub type",
                 disk_setting.path().as_view());
            continue;
          }

          // we need to rewire the paths in the vm config, so that they point
          // to the paths where we restored the volumes in.

          auto backed_up_disk_path = extract_disk_path(disk_setting);

          TRCC(ctx, L"disk path = {}", backed_up_disk_path);

          auto disk_name = [&] {
            auto last_slash = backed_up_disk_path.find_last_of(L"\\/");
            if (last_slash == backed_up_disk_path.npos) { last_slash = -1; }

            std::wstring name = backed_up_disk_path.substr(last_slash + 1,
                                                           std::wstring::npos);

            return utf16_to_utf8(name);
          }();

          TRCC(ctx, "disk name = {}", disk_name);

          auto found = restore_ctx->disk_map.find(disk_name);
          if (found == restore_ctx->disk_map.end()) {
            // if we did not restore the volume, then just ignore it
            DBGC(ctx, "{} was not restored, so we leave it as is", disk_name);
            continue;
          }

          TRCC(ctx, L"pre update xml = {}",
               format_as_xml(disk_setting).as_view());


          auto& found_path = found->second.path;

          set_disk_path(disk_setting, found_path);

          TRCC(ctx, L"updated disk setting = {}",
               disk_setting.to_string().as_view());

          auto xml = format_as_xml(disk_setting);

          std::vector<WMI::String> xmls;
          xmls.emplace_back(std::move(xml));

          system_srvc.modify_resource_settings(srvc, xmls);
        }
      }

      system_srvc.validate_planned_system(srvc, planned_system);

      destroyer.release();
      system_srvc.realize_planned_system(srvc, std::move(planned_system));

    } catch (const std::exception& ex) {
      JERR(ctx, "plugin was not able to restore the vm");
      return bRC_Error;
    } catch (...) {
      return bRC_Error;
    }
  }
  return bRC_OK;
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

static void ensure_paths(std::wstring_view path)
{
  std::wstring cpy{path};
  std::replace(std::begin(cpy), std::end(cpy), '/', '\\');

  DBG(L"Trying to create directories {}", cpy);

  auto err = SHCreateDirectoryExW(NULL, cpy.c_str(), NULL);
  switch (err) {
    case ERROR_SUCCESS: {
      DBG(L"path '{}' already exists", path);
    } break;
    case ERROR_FILE_EXISTS:
      [[fallthrough]];
    case ERROR_ALREADY_EXISTS: {
      DBG(L"path '{}' already exists", path);
    } break;
    default: {
      throw werror("trying to create paths", err);
    }
  }
}

static void create_dirs(std::wstring_view path)
{
  TRC(L"creating dirs in path {}", path);
  auto last_slash = path.find_last_of(L"\\/");
  if (last_slash != path.npos) { ensure_paths(path.substr(0, last_slash)); }
}

// creates the file at path and all directories above it
static HANDLE create_file(std::wstring_view path)
{
  create_dirs(path);

  TRC(L"creating file '{}' ...", path);

  auto dwaccess = GENERIC_WRITE | FILE_ALL_ACCESS | WRITE_OWNER | WRITE_DAC
                  | ACCESS_SYSTEM_SECURITY;
  auto dwflags = FILE_FLAG_BACKUP_SEMANTICS;
  return CreateFileW(std::wstring{path}.c_str(), dwaccess, FILE_SHARE_WRITE,
                     NULL, CREATE_NEW, dwflags, NULL);
}

static bool is_volume_file(std::string_view path)
{
  static constexpr std::string_view disks_path_start = "disks/";
  if (path.size() < disks_path_start.size()) { return false; }

  auto first_chars = path.substr(0, disks_path_start.size());

  return first_chars == disks_path_start;
}

static bool is_definition_file(std::string_view path)
{
  static constexpr std::string_view definition_file_ending = ".vmcx";
  static constexpr auto min_size = definition_file_ending.size();
  if (path.size() < min_size) { return false; }

  auto last_chars = path.substr(path.size() - min_size, min_size);

  return last_chars == definition_file_ending;
}

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
  TRCC(ctx, "create file '{}'", rp->ofname);
  auto* p_ctx = plugin_ctx::get(ctx);
  if (!p_ctx) { return bRC_Error; }

  auto* restore_ctx = std::get_if<plugin_ctx::restore>(&p_ctx->current_state);
  if (!restore_ctx) {
    JERR(ctx, "instructed to create file '{}' while not in restore mode",
         rp->ofname);
    return bRC_Error;
  }

  if (restore_ctx->hndl) {
    JERR(ctx,
         "instructed to create file '{}' while another file is still being "
         "worked on",
         rp->ofname);

    return bRC_Error;
  }

  auto actual_name = [&] {
    std::string_view output = rp->ofname;
    if (rp->where) {
      std::string_view where = rp->where;
      if (output.size() >= where.size()
          && output.substr(0, where.size()) == where) {
        return output.substr(where.size());
      } else {
        return output;
      }
    } else {
      return output;
    }
  }();

  TRCC(ctx, "actual name = {}", actual_name);

  // paths that we create have the following structure:
  // <prefix>/<vm name>/path
  // we need to check that the path given conforms to this and extract
  // the vm name/path.
  constexpr std::string_view prefix = "/@hyper-v@/";

  if (actual_name.size() <= prefix.size()
      || actual_name.substr(0, prefix.size()) != prefix) {
    JERR(ctx, "File {} was not created by this plugin (missing prefix {})",
         actual_name, prefix);
    return bRC_Error;
  }

  std::string_view without_prefix = actual_name.substr(prefix.size());

  TRCC(ctx, "without prefix = {}", without_prefix);

  auto pos = without_prefix.find_first_of("/");
  if (pos == without_prefix.npos) {
    JERR(ctx, "File {} was not created by this plugin (missing vm name)",
         actual_name);
    return bRC_Error;
  }
  if (pos == without_prefix.size()) {
    JERR(ctx, "File {} was not created by this plugin (missing path)",
         actual_name);
    return bRC_Error;
  }

  std::string_view vm_name = without_prefix.substr(0, pos);
  std::string_view actual_path = without_prefix.substr(pos + 1);

  TRCC(ctx, "vm_name = {}, path = {}", vm_name, actual_path);

  // we need to include the vm name to support restoring multiple vms at once
  std::wstring tmp_path
      = std::format(L"{}\\{}\\{}", restore_ctx->tmp_dir, utf8_to_utf16(vm_name),
                    utf8_to_utf16(actual_path));

  std::replace(std::begin(tmp_path), std::end(tmp_path), L'/', L'\\');
  // std::replace(std::begin(tmp_path),
  //              std::end(tmp_path),
  //              L"\\\\"; L"\\");

  TRCC(ctx, L"tmp_path = {}", tmp_path);

  if (is_definition_file(actual_path)) {
    DBGC(ctx, L"found definitions file at {}", tmp_path);
    restore_ctx->restored_definition_files.emplace_back(
        WMI::String::copy(tmp_path));
  }

  try {
    if (is_volume_file(actual_path)) {
      auto last_slash = actual_path.find_last_of("\\/");
      // we know that there is at least one slash, because
      // it contains disks/
      ASSERT(last_slash != actual_path.npos);
      if (last_slash == actual_path.size()) {
        JERR(ctx, "bad disk name {} (trailing slash)", actual_name);
        rp->create_status = CF_ERROR;
        return bRC_Error;
      }

      if (rp->delta_seq > 0) {
        // TODO:
        // we need to check that the file already exists
        // and open it.   We should also let the write routines know,
        // that a diff will be destored instead of a full.
      } else {
        std::string disk_name{
            actual_path.substr(last_slash + 1, actual_path.npos)};

        restore_ctx->disk_map[disk_name] = disk_info{tmp_path};
      }

      auto virtual_size = rp->statp.st_size;
      auto block_size = rp->statp.st_blksize;
      auto sector_size = rp->statp.st_blocks;

      create_dirs(tmp_path);

      const VIRTUAL_STORAGE_TYPE vst_vhd = {
          .DeviceId = VIRTUAL_STORAGE_TYPE_DEVICE_VHD,
          .VendorId = VIRTUAL_STORAGE_TYPE_VENDOR_MICROSOFT,
      };

      const VIRTUAL_STORAGE_TYPE vst_vhdx = {
          .DeviceId = VIRTUAL_STORAGE_TYPE_DEVICE_VHDX,
          .VendorId = VIRTUAL_STORAGE_TYPE_VENDOR_MICROSOFT,
      };


      VIRTUAL_STORAGE_TYPE vst = [&] {
        if (actual_path.back() == 'x') {
          DBGC(ctx, "{} -> vhdx", actual_path);
          return vst_vhdx;
        } else {
          DBGC(ctx, "{} -> vhd", actual_path);
          return vst_vhd;
        }
      }();

      TRCC(ctx, "--- 1");

      CREATE_VIRTUAL_DISK_PARAMETERS params;
      params.Version = CREATE_VIRTUAL_DISK_VERSION_1;
      params.Version1 = {
          .UniqueId = 0,  // let the system try to create a unique id
          .MaximumSize = virtual_size,
          .BlockSizeInBytes = block_size,
          .SectorSizeInBytes = static_cast<ULONG>(sector_size),
          .ParentPath = NULL,
          .SourcePath = NULL,
      };

      TRCC(ctx, "--- 2");

      auto disk_handle = INVALID_HANDLE_VALUE;

      auto result = CreateVirtualDisk(
          &vst, tmp_path.c_str(), VIRTUAL_DISK_ACCESS_ALL, NULL,
          CREATE_VIRTUAL_DISK_FLAG_NONE, 0, &params, NULL, &disk_handle);

      TRCC(ctx, "--- 3");

      if (result != ERROR_SUCCESS) {
        TRCC(ctx, "--- 4");

        JERR(ctx, L"could not create initial hard disk at {}.  Err={} ({:X})",
             tmp_path, format_win32_error(result), result);
        return bRC_Error;
      }

      TRCC(ctx, "--- 5");

      TRCC(ctx, L"created hard disk {} at {}", tmp_path.c_str(),
           fmt_as_ptr(disk_handle));

      restore_ctx->disk_handle = disk_handle;
      restore_ctx->current_offset = 0;
      rp->create_status = CF_EXTRACT;

      return bRC_OK;
    }

    HANDLE h = create_file(tmp_path);
    if (h == INVALID_HANDLE_VALUE) {
      JERR(ctx, L"could not create file {}: Err={}", tmp_path,
           format_win32_error(GetLastError()));
      return bRC_Error;
    }

    restore_ctx->hndl = h;

    DBGC(ctx, L"created file '{}'", tmp_path);
    rp->create_status = CF_EXTRACT;
    return bRC_OK;
  } catch (const std::exception& ex) {
    JERR(ctx, L"could not create file '{}'. Err={}", tmp_path,
         utf8_to_utf16(ex.what()));
    return bRC_Error;
  } catch (...) {
    JERR(ctx, L"could not create file '{}'.", tmp_path);
    return bRC_Error;
  }
}

/**
 * We will get here if the File is a directory after everything is written in
 * the directory.
 */
static bRC setFileAttributes(PluginContext*, restore_pkt*) { return bRC_OK; }

// When using Incremental dump, all previous dumps are necessary
static bRC checkFile(PluginContext*, char*) { return bRC_OK; }
