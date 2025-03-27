/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2025-2025 Bareos GmbH & Co. KG

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   Affero General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
*/

#define _WIN32_DCOM
#include <iostream>
#include <comdef.h>
#include <Wbemidl.h>
#include <atlsafe.h>
#include <initguid.h>  // ask virtdisk.h to include guid definitions
#include <virtdisk.h>
#include <unordered_map>
#include <span>
#include <strsafe.h>

template <class Enum>
constexpr std::underlying_type_t<Enum> to_underlying(Enum e) noexcept
{
  return static_cast<std::underlying_type_t<Enum>>(e);
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


using namespace std;

#pragma comment(lib, "wbemuuid.lib")
#pragma comment(lib, "virtdisk.lib")

#include <string_view>
#include <optional>
#include <format>
#include <source_location>
#include <vector>
#include <string>
#include <chrono>
#include <thread>

struct Logger {
  Logger(std::source_location sloc = std::source_location::current())
      : loc{sloc}
  {
  }

  template <class... Args>
  void Log(std::wformat_string<Args...> fmt, Args&&... args)
  {
    fwprintf(stderr, L"[LOG %hs:%d] %s\n", loc.function_name(), loc.line(),
             std::format(fmt, std::forward<Args>(args)...).c_str());
  }

  std::source_location loc;
};

#define LOG(...)               \
  do {                         \
    Logger{}.Log(__VA_ARGS__); \
  } while (0)

struct snapshot_ref {};
struct exported_snapshot {};

struct WMI {
  template <typename T> using Result = std::optional<T>;

  IWbemServices* service;

  WMI(IWbemServices* service_) : service{service_} {}

  struct SystemString {
    BSTR Value = nullptr;

    SystemString() = default;
    SystemString(BSTR val) : Value{val} {}
    SystemString(std::wstring_view str)
        : SystemString(SysAllocStringLen(str.data(), (UINT)str.size()))
    {
    }

    SystemString(const SystemString&) = delete;
    SystemString& operator=(const SystemString&) = delete;

    SystemString(SystemString&& other) { *this = std::move(other); }
    SystemString& operator=(SystemString&& other)
    {
      std::swap(Value, other.Value);
      return *this;
    }

    BSTR get() const { return Value; }

    std::size_t size() const { return SysStringLen(Value); }

    std::wstring_view as_view() const
    {
      if (!Value) { return std::wstring_view{}; }
      return std::wstring_view{Value, SysStringLen(Value)};
    }

    ~SystemString()
    {
      if (Value) { SysFreeString(Value); }
    }
  };

  struct Variant {
    // https://learn.microsoft.com/en-us/windows/win32/api/wtypes/ne-wtypes-varenum
    // https://learn.microsoft.com/en-us/windows/win32/api/oaidl/ns-oaidl-variant

    VARIANT Value;

    Variant() { VariantInit(&Value); }
    Variant(VARIANT var) : Value{var} {}

    Variant(const Variant&) = delete;
    Variant& operator=(const Variant&) = delete;

    Variant(Variant&& other) { *this = std::move(other); }
    Variant& operator=(Variant&& other)
    {
      std::swap(Value, other.Value);
      return *this;
    }

    VARIANT& get() { return Value; }
    const VARIANT& get() const { return Value; }

    ~Variant() { VariantClear(&Value); }
  };

  struct Class {
    SystemString Name;
    IWbemClassObject* class_ptr{};
  };

  struct ClassObject {
    IWbemClassObject* system;

    WMI::Result<WMI::SystemString> Path() const
    {
      VARIANT Arg;
      CIMTYPE Type;
      auto hres = system->Get(L"__PATH", 0, &Arg, &Type, nullptr);

      if (FAILED(hres)) {
        LOG(L"Get(ComputerSystem, __PATH) failed.  Err={} ({:X})",
            WMI::ErrorString(hres), (uint64_t)hres);
        return std::nullopt;
      } else if (Type != CIM_STRING) {
        LOG(L"Get(ComputerSystem, __PATH) returned bad type detected.  Type={}",
            Type);
        return std::nullopt;
      }
      return WMI::SystemString{V_BSTR(&Arg)};
    }
  };

  struct ComputerSystem : public WMI::ClassObject {};

  struct Method {
    struct Parameter {
      IWbemClassObject* parameter;

      struct throwaway {};

      Result<throwaway> Put(const wchar_t* Name, int32_t Value)
      {
        VARIANT Param;
        V_VT(&Param) = VT_I4;
        V_I4(&Param) = Value;

        auto hres = parameter->Put(Name, 0, &Param, 0);

        if (FAILED(hres)) {
          LOG(L"Put({}, {}) failed.  Err={} ({:X})", Name, Value,
              ErrorString(hres), (uint64_t)hres);
          return std::nullopt;
        }

        return throwaway{};
      }

      Result<throwaway> Put(const wchar_t* Name, const SystemString& Value)
      {
        VARIANT Param;
        V_VT(&Param) = VT_BSTR;
        V_BSTR(&Param) = Value.get();

        auto hres = parameter->Put(Name, 0, &Param, 0);

        if (FAILED(hres)) {
          LOG(L"Put({}, {}) failed.  Err={} ({:X})", Name, Value.as_view(),
              ErrorString(hres), (uint64_t)hres);
          return std::nullopt;
        }

        return throwaway{};
      }

      Result<throwaway> Put(const wchar_t* Name,
                            const std::vector<SystemString>& Value)
      {
        CComSafeArray<BSTR> array;
        auto create_array_res = array.Create(ULONG{0});
        if (FAILED(create_array_res)) {
          LOG(L"Array.Create({}) failed.  Err={} ({:X})", 0,
              ErrorString(create_array_res), (uint64_t)create_array_res);
          return std::nullopt;
        }

        for (auto& str : Value) {
          auto add_res = array.Add(str.get());
          if (FAILED(add_res)) {
            LOG(L"Array.Add({}) failed.  Err={} ({:X})", str.as_view(),
                ErrorString(add_res), (uint64_t)add_res);
            return std::nullopt;
          }
        }

        VARIANT Param;
        V_VT(&Param) = VT_BSTR | VT_ARRAY;
        V_ARRAY(&Param) = array;
        // V_BSTR(&Param) = Value.get();

        auto hres = parameter->Put(Name, 0, &Param, 0);

        if (FAILED(hres)) {
          LOG(L"Put({}, [array]) failed.  Err={} ({:X})", Name,
              ErrorString(hres), (uint64_t)hres);
          return std::nullopt;
        }

        array.Destroy();
        return throwaway{};
      }

      Result<throwaway> Put(const wchar_t* Name, const ComputerSystem& Value)
      {
        VARIANT Param;
        V_VT(&Param) = VT_BSTR;
        auto Path = Value.Path();
        if (!Path) { return std::nullopt; }

        return Put(Name, Path.value());
      }

      Result<throwaway> Put(const wchar_t* Name, const ClassObject& Value)
      {
        VARIANT Param;
        V_VT(&Param) = VT_BSTR;
        auto Path = Value.Path();
        if (!Path) { return std::nullopt; }

        return Put(Name, Path.value());
      }

      Result<throwaway> Put(const wchar_t* Name, VARIANT& Param)
      {
        auto hres = parameter->Put(Name, 0, &Param, 0);

        if (FAILED(hres)) {
          LOG(L"Put({}, VARIANT) failed.  Err={} ({:X})", Name,
              ErrorString(hres), (uint64_t)hres);
          return std::nullopt;
        }

        return throwaway{};
      }
    };

    SystemString Name;
    IWbemClassObject* parameter_def;


    Result<Parameter> CreateParamInstance() const
    {
      IWbemClassObject* params = nullptr;
      auto hres = parameter_def->SpawnInstance(0, &params);

      if (FAILED(hres)) {
        LOG(L"CreateParamInstance failed.  Err={} ({:X})",
            WMI::ErrorString(hres), (uint64_t)hres);
        return std::nullopt;
      }

      return Parameter{params};
    }
  };

  static std::wstring ErrorString(HRESULT error)
  {
    constexpr HRESULT WMI_ERROR_MASK = 0x80041000;
    static HMODULE mod = LoadLibraryW(L"wmiutils.dll");

    if (!mod) { return L"Could not load error module"; }

    bool is_wmi_error
        = error >= WMI_ERROR_MASK && error <= WMI_ERROR_MASK + 0xFFF;
    auto flags = FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_FROM_SYSTEM
                 | FORMAT_MESSAGE_ALLOCATE_BUFFER
                 | FORMAT_MESSAGE_MAX_WIDTH_MASK
                 | (is_wmi_error ? FORMAT_MESSAGE_FROM_HMODULE : 0);

    auto actual_mod = is_wmi_error ? mod : 0;

    wchar_t* output = nullptr;
    auto byte_count = FormatMessageW(flags, actual_mod, error, 0,
                                     (LPWSTR)&output, 0, nullptr);

    if (byte_count < 0) { return L"Unformatable error"; }

    std::wstring s{std::wstring_view{output, byte_count}};

    LocalFree(output);

    return s;
  }

  // template <typename T> class Result {
  //   union {
  //     T success;
  //     HRESULT error;
  //   };
  //   bool ok{false};

  //   Result(T val)
  //     : success{std::move(val)}
  //     , ok{true}
  //   {
  //   }
  //   Result(HRESULT error)
  //     : error{error}
  //     , ok{false}
  //   {
  //   }

  //   operator bool() const { return ok; }

  //   T& value() { return success; }
  //   const T& value() const { return success; }

  //   std::wstring error_string()
  //   {
  //     if (ok) { return L"No error"; }

  //     HMODULE* mod =
  //     GetModuleHandleW(L"C:\\Windows\\System32\\wbem\\wmiutils.dll");

  //     if (!mod) {
  //       return L"Could not load error module";
  //     }

  //     auto byte_count = FormatMessageW(FORMAT_MESSAGE_FROM_HMODULE,
  //                                      mod, error, 0,
  //                                      nullptr, 0, nullptr);

  //     if (byte_count < 0) {
  //       return L"Unformatable error";
  //     }

  //     std::wstring s;
  //     s.resize(byte_count);
  //     FormatMessageW(FORMAT_MESSAGE_FROM_HMODULE,
  //                    mod, error, 0,
  //                    s.data(), s.size(), nullptr);

  //     return s;
  //   }
  // };

  Result<Class> LoadClassByName(std::wstring_view class_name) const
  {
    LOG(L"Loading class {} ...", class_name);
    IWbemClassObject* clz = nullptr;
    WMI::SystemString copy{class_name};

    auto result = service->GetObject(copy.get(), 0, NULL, &clz, NULL);

    if (FAILED(result)) {
      LOG(L"... failed ({}). Err={} ({:X})", class_name, ErrorString(result),
          (uint64_t)result);
      return std::nullopt;
    }

    LOG(L"... succeeded ({}).", class_name);
    return Class{std::move(copy), clz};
  }

  Result<Method> LoadMethodByName(const Class& clz,
                                  std::wstring_view method_name) const
  {
    LOG(L"Loading method {} of class {} ...", method_name, clz.Name.as_view());
    IWbemClassObject* parameter_def = NULL;
    WMI::SystemString copy{method_name};
    auto result = clz.class_ptr->GetMethod(copy.get(), 0, &parameter_def, NULL);

    if (FAILED(result)) {
      LOG(L"... failed ({}). Err={} ({:X})", method_name, ErrorString(result),
          (uint64_t)result);
      return std::nullopt;
    }

    LOG(L"... succeeded ({}).", method_name);
    return Method{std::move(copy), parameter_def};
  }

  IWbemClassObject* ExecMethod(const ClassObject& clz,
                               const Method& method,
                               IWbemClassObject* Parameter) const
  {
    IWbemClassObject* OutParams = nullptr;
    auto Path = clz.Path();
    if (!Path) { return nullptr; }
    LOG(L"Path = {}", Path->as_view());
    LOG(L"Params = {}", ObjectAsString(Parameter).as_view());

    auto result = service->ExecMethod(Path->get(), method.Name.get(), 0, NULL,
                                      Parameter, &OutParams, NULL);

    if (FAILED(result)) {
      LOG(L"{}->{} failed.  Err={} ({:X})", Path->as_view(),
          method.Name.as_view(), ErrorString(result), (uint64_t)result);

      IErrorInfo* Info = nullptr;
      auto error_res = GetErrorInfo(0, &Info);

      if (FAILED(error_res)) {
        LOG(L"Could not retrieve error info.  Err={} ({})",
            ErrorString(error_res), (uint64_t)error_res);
      } else {
        BSTR Desc = nullptr;
        GUID Guid = {};
        DWORD HelpContext = 0;
        BSTR HelpFile = nullptr;
        BSTR Source = nullptr;

        Info->GetSource(&Source);
        Info->GetHelpFile(&HelpFile);
        Info->GetHelpContext(&HelpContext);
        Info->GetGUID(&Guid);
        Info->GetDescription(&Desc);

        auto MakeStrView = [](BSTR val) -> std::wstring_view {
          if (!val) { return L"<empty>"; }
          return {val, SysStringLen(val)};
        };


        LOG(L"{}({:x}:{:x}:{:x}:{:x}{:x}{:x}{:x}{:x}{:x}{:x}{:x}): {}; See {} "
            L"({})",
            MakeStrView(Source), Guid.Data1, Guid.Data2, Guid.Data3,
            Guid.Data4[0], Guid.Data4[1], Guid.Data4[2], Guid.Data4[3],
            Guid.Data4[4], Guid.Data4[5], Guid.Data4[6], Guid.Data4[7],
            MakeStrView(Desc), MakeStrView(HelpFile), HelpContext);

        // if (Desc) { SysFreeString(Desc); }
        // if (HelpFile) { SysFreeString(HelpFile); }
        // if (Source) { SysFreeString(Source); }

        Info->Release();
      }

      return nullptr;
    }

    LOG(L"{}->{} succeeded! OutParams = {}", Path->as_view(),
        method.Name.as_view(), ObjectAsString(OutParams).as_view());
    return OutParams;
  }


  IWbemClassObject* QueryFirst(const SystemString& query) const
  {
    IEnumWbemClassObject* pEnumerator = NULL;
    auto hres = service->ExecQuery(
        bstr_t("WQL"), query.get(),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL,
        &pEnumerator);

    if (FAILED(hres)) {
      LOG(L"QueryFirst::ExecQuery() failed.  Err={} ({:X})",
          WMI::ErrorString(hres), (uint64_t)hres);
      return nullptr;
    }

    {
      IWbemClassObject* pclsObj = NULL;
      ULONG uReturn = 0;
      HRESULT hr = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);

      //      if (0 == uReturn) { break; }

      VARIANT vtProp;

      VariantInit(&vtProp);
      // Get the value of the Name property
      hr = pclsObj->Get(L"Name", 0, &vtProp, 0, 0);
      LOG(L"Name {}", vtProp.bstrVal);
      VariantClear(&vtProp);

      pEnumerator->Release();
      return pclsObj;
    }

    LOG(L"No instance found");
  }

  static SystemString ObjectAsString(IWbemClassObject* obj)
  {
    BSTR representation = nullptr;
    obj->GetObjectText(0, &representation);

    return {representation};
  }

  bool WaitForJobCompletion(IWbemClassObject* result) const
  {
    VARIANT return_val;
    auto hres = result->Get(_bstr_t(L"ReturnValue"), 0, &return_val, NULL, 0);

    if (FAILED(hres)) {
      LOG(L"Could not get return value.  Err={} ({:X})", WMI::ErrorString(hres),
          (uint64_t)hres);
      return false;
    }

    if (V_VT(&return_val) != VT_I4) {
      LOG(L"return val has bad type.  Type = {}", return_val.vt);
      VariantClear(&return_val);
      return false;
    }

    auto actual_return = V_I4(&return_val);

    LOG(L"function returned {}", actual_return);

    if (actual_return == CIMReturnValue::OK) { return true; }
    if (actual_return != CIMReturnValue::JobStarted) { return false; }

    VARIANT JobV;
    CIMTYPE JobType;
    auto job_res = result->Get(_bstr_t(L"Job"), 0, &JobV, &JobType, 0);

    if (FAILED(job_res)) {
      LOG(L"Could not get job.  Err={} ({:X})", WMI::ErrorString(job_res),
          (uint64_t)job_res);

      return false;
    }

    if (V_VT(&JobV) != VT_BSTR) {
      LOG(L"Bad job value type.  Type = {}", JobV.vt);
      VariantClear(&JobV);
      return false;
    }

    if (JobType != CIM_REFERENCE) {
      LOG(L"Bad JobType {}", JobType);
      VariantClear(&JobV);
      return false;
    }

    WMI::SystemString job_name{std::wstring_view{V_BSTR(&JobV)}};
    VariantClear(&JobV);

    LOG(L"Found job = {}", job_name.as_view());

    bool running = true;
    while (running) {
      // NOTE: we have to reacquire the same job over and over
      //       as GetObject basically does a copy:
      //       The state of that copy will never change!
      IWbemClassObject* job = nullptr;
      {
        auto get_res = service->GetObject(job_name.get(), 0, NULL, &job, NULL);

        if (FAILED(get_res)) {
          LOG(L"Could not get job.  Err={} ({:X})", WMI::ErrorString(get_res),
              (uint64_t)get_res);
          return false;
        }
      }

      LOG(L"Got Job = {}", ObjectAsString(job).as_view());

      VARIANT job_state;
      auto state_res = job->Get(L"JobState", 0, &job_state, NULL, 0);

      if (FAILED(state_res)) {
        LOG(L"Could not get job state.  Err={} ({:X})",
            WMI::ErrorString(state_res), (uint64_t)state_res);
        return false;
      }

      if (V_VT(&job_state) != VT_I4) {
        LOG(L"bad job state type = {}", job_state.vt);
        VariantClear(&job_state);
        return false;
      }

      auto state = V_I4(&job_state);

      enum JobState : std::int32_t
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

      switch (state) {
        case JobState::New:
        case JobState::Starting:
        case JobState::Running: {
          LOG(L"Job still running in state {}.  Sleeping ...", state);

          std::this_thread::sleep_for(std::chrono::seconds(1));
        } break;
        default: {
          LOG(L"Job ended in state {}", state);
          running = false;
        } break;
      }

      job->Release();
    }

    return true;
  }

  Result<std::vector<ClassObject>> GetRelatedOfClass(
      std::wstring_view Associate,
      std::wstring_view Class,
      std::wstring_view AssocClass = {}) const
  {
    std::wstring query = [&] {
      if (AssocClass.empty()) {
        return std::format(L"associators of {{{}}} where ResultClass = {}",
                           Associate, Class);
      } else {
        return std::format(
            L"associators of {{{}}} where ResultClass = {} AssocClass = {}",
            Associate, Class, AssocClass);
      }
    }();

    WMI::SystemString squery{query};

    IEnumWbemClassObject* iter;

    HRESULT hres = service->ExecQuery(
        _bstr_t(L"WQL"), squery.get(),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL, &iter);

    if (FAILED(hres)) {
      LOG(L"Could query '{}'.  Err={} ({:X})", squery.as_view(),
          WMI::ErrorString(hres), (uint64_t)hres);
      return std::nullopt;
    }

    std::vector<ClassObject> result;
    for (;;) {
      IWbemClassObject* current = nullptr;
      ULONG returned_count = 0;
      auto hr = iter->Next(WBEM_INFINITE, 1, &current, &returned_count);

      if (FAILED(hr)) {
        LOG(L"Could not get {}th result from query '{}'.  Err={} ({:X})",
            result.size(), squery.as_view(), WMI::ErrorString(hres),
            (uint64_t)hres);
        iter->Release();
        return std::nullopt;
      }

      if (returned_count == 0) { break; }

      result.emplace_back(std::move(ClassObject{current}));
    }

    iter->Release();

    return result;
  }

  static Result<SystemString> ObjectAsXML(const ClassObject& obj)
  {
    IWbemContext* context = nullptr;
    {
      auto hres
          = CoCreateInstance(CLSID_WbemContext, NULL, CLSCTX_INPROC_SERVER,
                             IID_IWbemContext, (void**)&context);

      if (FAILED(hres)) {
        LOG(L"CoCreateInstance(WbemContext) failed.  Err={} ({:X})",
            WMI::ErrorString(hres), (uint32_t)hres);
        return std::nullopt;
      }
    }

    IWbemObjectTextSrc* text_source;
    {
      auto hres = CoCreateInstance(CLSID_WbemObjectTextSrc, NULL,
                                   CLSCTX_INPROC_SERVER, IID_IWbemObjectTextSrc,
                                   (void**)&text_source);

      if (FAILED(hres)) {
        LOG(L"CoCreateInstance(WbemObjectTextSrc) failed.  Err={} ({:X})",
            WMI::ErrorString(hres), (uint32_t)hres);
        context->Release();
        return std::nullopt;
      }
    }

    BSTR text = nullptr;

    auto text_res = text_source->GetText(
        0, obj.system, WMI_OBJ_TEXT_WMI_DTD_2_0, context, &text);

    text_source->Release();
    context->Release();

    if (FAILED(text_res)) {
      LOG(L"GetText() failed.  Err={} ({:X})", WMI::ErrorString(text_res),
          (uint32_t)text_res);
      return std::nullopt;
    }

    return WMI::SystemString{text};
  }


  WMI::Result<WMI::ComputerSystem> GetVMByName(std::wstring_view vm_name) const
  {
#if 0
    WMI::SystemString name{vm_name};
    IWbemClassObject* vm = nullptr;
    auto hres = service->GetObject(name.get(), 0, NULL, &vm, NULL);
    if (FAILED(hres)) {
      LOG(L"GetObject({}) failed.  Err={} ({:X})", name.as_view(),
          WMI::ErrorString(hres), (uint32_t)hres);
      return std::nullopt;
    }

    LOG(L"VM at {}", (void*) vm);

    return WMI::ComputerSystem{ vm };
#else
    std::wstring query = std::format(
        L"SELECT * FROM Msvm_ComputerSystem WHERE ElementName=\"{}\"", vm_name);
    WMI::SystemString system_query{query};
    IEnumWbemClassObject* iter = nullptr;
    LOG(L"Query = {}", system_query.as_view());
    auto hres = service->ExecQuery(
        _bstr_t("WQL"), system_query.get(),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL, &iter);
    if (FAILED(hres)) {
      LOG(L"ExecQuery failed. Err={} ({:X})", WMI::ErrorString(hres),
          (uint64_t)hres);
      return std::nullopt;
    }

    IWbemClassObject* found = nullptr;
    std::size_t count = 0;
    while (iter) {
      IWbemClassObject* object = nullptr;
      ULONG uReturn = 0;
      HRESULT hr = iter->Next(WBEM_INFINITE, 1, &object, &uReturn);

      if (FAILED(hr)) {
        iter->Release();
        LOG(L"iter->Next() failed.  Err={}", WMI::ErrorString(hres), hres);

        return std::nullopt;
      }

      if (0 == uReturn) { break; }

      count += 1;

      LOG(L"Found matching obj at {}", (void*)object);

      if (count == 1) {
        found = object;
      } else {
        object->Release();
      }
    }

    iter->Release();
    if (count == 0) {
      LOG(L"VmName {} not found.  Cannot continue", vm_name);
      return std::nullopt;
    } else if (count > 1) {
      LOG(L"VmName {} is not unique.  Found {} hits.  Using the first one",
          vm_name, count);
    }

    LOG(L"Found vm for VmName {}", vm_name);
    return WMI::ComputerSystem{found};
#endif
  }
};

class VirtualSystemExportSettingData {
 public:
  struct Class {
    static std::optional<Class> load(const WMI& wmi)
    {
      auto clz = wmi.LoadClassByName(L"Msvm_VirtualSystemExportSettingData");

      if (!clz) { return std::nullopt; }

      return Class{std::move(clz.value()), &wmi};
    }

    WMI::Result<WMI::SystemString> GetText(
        const VirtualSystemExportSettingData& settings)
    {
      // BSTR content = nullptr;
      // auto hres = instance->GetObjectText(0, &content);
      // if (FAILED(hres)) {
      //   LOG(L"ExportSettingData::GetText() failed.  Err={} ({:X})",
      //       WMI::ErrorString(hres), (uint64_t)hres);
      //   return std::nullopt;
      // }
      // return content;

      IWbemClassObject* instance = nullptr;
      auto hres = clz.class_ptr->SpawnInstance(0, &instance);

      LOG(L"instance = {}", WMI::ObjectAsString(instance).as_view());

      if (FAILED(hres)) {
        LOG(L"Could not create instance of class {}.  Err={} ({:X})",
            clz.Name.as_view(), WMI::ErrorString(hres), (uint64_t)hres);
        return std::nullopt;
      }

      if (!Set(instance, L"Description", settings.description)) {
        instance->Release();
        return std::nullopt;
      }
      if (!Set(instance, L"SnapshotVirtualSystem",
               settings.snapshot_virtual_system_path)) {
        instance->Release();
        return std::nullopt;
      }
      if (!Set(instance, L"DifferentialBackupBase",
               settings.differential_backup_base_path)) {
        instance->Release();
        return std::nullopt;
      }
      if (!Set(instance, L"DisableDifferentialOfIgnoredStorage",
               settings.disable_differential_of_ignored_storage)) {
        instance->Release();
        return std::nullopt;
      }
      if (!Set(instance, L"ExportForLiveMigration",
               settings.export_for_live_migration)) {
        instance->Release();
        return std::nullopt;
      }
      if (!Set(instance, L"BackupIntent",
               static_cast<std::uint8_t>(settings.backup_intent))) {
        instance->Release();
        return std::nullopt;
      }
      if (!Set(instance, L"SnapshotVirtualSystem",
               settings.snapshot_virtual_system_path)) {
        instance->Release();
        return std::nullopt;
      }
      if (!Set(instance, L"CreateVmExportSubdirectory",
               settings.create_vm_export_subdirectory)) {
        instance->Release();
        return std::nullopt;
      }
      LOG(L"CopyVmStorage = {}", settings.copy_vm_storage);
      if (!Set(instance, L"CopyVmStorage", settings.copy_vm_storage)) {
        instance->Release();
        return std::nullopt;
      }
      if (!Set(instance, L"CopyVmRuntimeInformation",
               settings.copy_vm_runtime_information)) {
        instance->Release();
        return std::nullopt;
      }
      if (!Set(instance, L"CopySnapshotConfiguration",
               static_cast<std::uint8_t>(
                   settings.copy_snapshot_configuration))) {
        instance->Release();
        return std::nullopt;
      }
      if (!Set(instance, L"CaptureLiveState",
               static_cast<std::uint8_t>(settings.capture_live_state))) {
        instance->Release();
        return std::nullopt;
      }

      {
        BSTR text = nullptr;
        auto text_res = instance->GetObjectText(0, &text);
        if (!FAILED(text_res)) {
          LOG(L"Instance = {}", std::wstring_view{text, SysStringLen(text)});
          SysFreeString(text);
        }
      }

      LOG(L"ExportSettings = {}", WMI::ObjectAsString(instance).as_view());


      // we now need to convert this settings instance to xml formatted text
      // this is done via IWbemObjectTextSrc, see:
      // https://learn.microsoft.com/en-us/windows/win32/wmisdk/representing-objects-in-xml

      IWbemContext* context = nullptr;

      auto res3
          = CoCreateInstance(CLSID_WbemContext, NULL, CLSCTX_INPROC_SERVER,
                             IID_IWbemContext, (void**)&context);

      if (FAILED(res3)) {
        LOG(L"CoCreateInstance() failed.  Err={} ({:X})",
            WMI::ErrorString(res3), (uint32_t)res3);
        instance->Release();
        return std::nullopt;
      }

      IWbemObjectTextSrc* text_source;
      auto res = CoCreateInstance(CLSID_WbemObjectTextSrc, NULL,
                                  CLSCTX_INPROC_SERVER, IID_IWbemObjectTextSrc,
                                  (void**)&text_source);

      if (FAILED(res)) {
        LOG(L"CoCreateInstance() failed.  Err={} ({:X})", WMI::ErrorString(res),
            (uint32_t)res);
        context->Release();
        instance->Release();
        return std::nullopt;
      }

      BSTR text = nullptr;

      auto res2 = text_source->GetText(0, instance, WMI_OBJ_TEXT_WMI_DTD_2_0,
                                       context, &text);

      text_source->Release();
      instance->Release();
      context->Release();

      if (FAILED(res2)) {
        LOG(L"GetText() failed.  Err={} ({:X})", WMI::ErrorString(res2),
            (uint32_t)res2);
        return std::nullopt;
      }

      return WMI::SystemString{text};
    }

   private:
    WMI::Class clz;
    const WMI* service;

    struct throwaway {};

    template <typename U>
    WMI::Result<throwaway> Set(IWbemClassObject* instance,
                               const wchar_t* member,
                               const std::optional<U>& value)
    {
      if (value) { return Set(instance, member, value.value()); }

      return throwaway{};
    }

    WMI::Result<throwaway> Set(IWbemClassObject* instance,
                               const wchar_t* member,
                               const std::wstring& value)
    {
      WMI::SystemString svalue{value};
      VARIANT Arg;
      V_VT(&Arg) = VT_BSTR;
      V_BSTR(&Arg) = svalue.get();
      auto result = instance->Put(member, 0, &Arg, 0);
      if (FAILED(result)) {
        LOG(L"Set(ExportSetting, {}, {}) failed.  Err={} ({:X})", member, value,
            WMI::ErrorString(result), (uint64_t)result);
        return std::nullopt;
      }

      return throwaway{};
    }

    WMI::Result<throwaway> Set(IWbemClassObject* instance,
                               const wchar_t* member,
                               bool value)
    {
      VARIANT Arg;
      V_VT(&Arg) = VT_BOOL;
      V_BOOL(&Arg) = value;

      auto result = instance->Put(member, 0, &Arg, 0);
      if (FAILED(result)) {
        LOG(L"Set(ExportSetting, {}, {}) failed.  Err={} ({:X})", member, value,
            WMI::ErrorString(result), (uint64_t)result);
        return std::nullopt;
      }

      return throwaway{};
    }
    WMI::Result<throwaway> Set(IWbemClassObject* instance,
                               const wchar_t* member,
                               std::uint8_t value)
    {
      VARIANT Arg;
      V_VT(&Arg) = VT_UI1;
      V_UI1(&Arg) = value;
      auto result = instance->Put(member, 0, &Arg, 0);
      if (FAILED(result)) {
        LOG(L"Set(ExportSetting, {}, {}) failed.  Err={} ({:X})", member, value,
            WMI::ErrorString(result), (uint64_t)result);
        return std::nullopt;
      }

      return throwaway{};
    }

    Class(WMI::Class clz_, const WMI* service_)
        : clz{std::move(clz_)}, service{service_}
    {
    }
  };


  enum class CaptureLiveState : std::uint8_t
  {
    CrashConsistent = 0,
    Saved = 1,
    AppConsistent = 2,
  };

  // WMI::Result<void> Set_CaptureLiveState(CaptureLiveState val)
  // {
  //   return std::nullopt;
  // }

  // WMI::Result<void> Set_Description(const WMI::SystemString& desc)
  // {
  //   return std::nullopt;
  // }

  enum class CopySnapshotConfiguration : std::uint8_t
  {
    ExportAllSnapshots = 0,
    ExportNoSnapShots = 1,
    ExportOneSnapshot = 2,
    ExportOneSnapshotForBackup = 3,
  };

  // WMI::Result<void> Set_CopySnapshotConfiguration(CaptureSnapshotConfig val)
  // {
  //   return std::nullopt;
  // }
  // WMI::Result<void> CopyVmRuntimeInformation(bool do_copy = true)
  // {
  //   return std::nullopt;
  // }
  // WMI::Result<void> CopyVmStorage(bool do_copy = true)
  // {
  //   return std::nullopt;
  // }
  // WMI::Result<void> CreateVmExportSubdirectory(bool do_create = true)
  // {
  //   return std::nullopt;
  // }
  // WMI::Result<void> SetupForLiveMigration(bool do_setup = true)
  // {
  //   return std::nullopt;
  // }
  // WMI::Result<void> DisableDifferentialOfIgnoredStorage(bool do_ignore =
  // true)
  // {
  //   return std::nullopt;
  // }
  // WMI::Result<void> Set_SnapshotVirtualSystem(const WMI::SystemString& path)
  // {
  //   return std::nullopt;
  // }

  enum class BackupIntent : std::uint8_t
  {
    PreserveChain = 0,  // i.e. we want to store full/diff separately
    Merge = 1,          // i.e. we want to consolidate full/diff
  };
  // WMI::Result<void> Set_BackupIntent(BackupIntent intent)
  // {
  //   return std::nullopt;
  // }
  // WMI::Result<void> Set_ExcludeVirtualHardDisks(const
  // std::vector<WMI::SystemString>& hard_disks)
  // {
  //   // NOT IMPLEMENTED
  //   (void) hard_disks;
  //   return std::nullopt;
  // }
  // WMI::Result<void> Set_DifferentialBackupBase(const WMI::SystemString& base)
  // {
  //   return std::nullopt;
  // }

  // members:
  // uint8   CaptureLiveState; (read/write)
  // string  InstanceID; (read only)
  // string  Caption; (read only)
  // string  Description; (read/write)
  // string  ElementName; (read only)
  // uint8   CopySnapshotConfiguration; (read/write)
  // boolean CopyVmRuntimeInformation; (read/write)
  // boolean CopyVmStorage; (read/write)
  // boolean CreateVmExportSubdirectory; (read/write)
  // string  SnapshotVirtualSystem; (read/write)
  // uint8   BackupIntent; (read/write)
  // boolean ExportForLiveMigration; (read/write)
  // boolean DisableDifferentialOfIgnoredStorage; (read/write)
  // string  ExcludedVirtualHardDisks[]; (read/write)
  // string  DifferentialBackupBase; (read/write)

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

class VirtualSystemManagementService : WMI::ClassObject {
  // Reference:
  // https://learn.microsoft.com/en-us/windows/win32/hyperv_v2/msvm-virtualsystemmanagementservice
 public:
  struct Class {
    friend class VirtualSystemManagementService;

    static WMI::Result<Class> load(const WMI& wmi)
    {
      auto clz = wmi.LoadClassByName(L"Msvm_VirtualSystemManagementService");
      if (!clz) { return std::nullopt; }

      auto export_system
          = wmi.LoadMethodByName(*clz, L"ExportSystemDefinition");
      if (!export_system) { return std::nullopt; }

      auto import_system
          = wmi.LoadMethodByName(*clz, L"ImportSystemDefinition");
      if (!import_system) { return std::nullopt; }

      auto destroy_system = wmi.LoadMethodByName(*clz, L"DestroySystem");
      if (!destroy_system) { return std::nullopt; }

      auto modify_system_settings
          = wmi.LoadMethodByName(*clz, L"ModifySystemSettings");
      if (!modify_system_settings) { return std::nullopt; }

      auto modify_resource_settings
          = wmi.LoadMethodByName(*clz, L"ModifyResourceSettings");
      if (!modify_resource_settings) { return std::nullopt; }

      auto validate_planned_system
          = wmi.LoadMethodByName(*clz, L"ValidatePlannedSystem");
      if (!validate_planned_system) { return std::nullopt; }

      auto realize_planned_system
          = wmi.LoadMethodByName(*clz, L"RealizePlannedSystem");
      if (!realize_planned_system) { return std::nullopt; }

      auto import_snapshot
          = wmi.LoadMethodByName(*clz, L"ImportSnapshotDefinitions");
      if (!import_snapshot) { return std::nullopt; }

      return Class{std::move(clz.value()),
                   std::move(export_system.value()),
                   std::move(import_system.value()),
                   std::move(destroy_system.value()),
                   std::move(modify_system_settings.value()),
                   std::move(modify_resource_settings.value()),
                   std::move(validate_planned_system).value(),
                   std::move(realize_planned_system).value(),
                   std::move(import_snapshot).value(),
                   &wmi};
    }

    WMI::Result<VirtualSystemManagementService> Get() const
    {
      WMI::SystemString query{std::wstring_view{
          L"SELECT * FROM Msvm_VirtualSystemManagementService"}};
      auto* obj = wmi->QueryFirst(query);
      if (!obj) { return std::nullopt; }

      {
        auto str = WMI::ObjectAsString(obj);
        LOG(L"VirtualSystemManagementService = {}", str.as_view());
      }

      WMI::ClassObject service{obj};

      return VirtualSystemManagementService{std::move(service), this};
    }

   private:
    Class(WMI::Class clz_,
          WMI::Method export_system_,
          WMI::Method import_system_,
          WMI::Method destroy_system_,
          WMI::Method modify_system_settings_,
          WMI::Method modify_resource_settings_,
          WMI::Method validate_planned_system_,
          WMI::Method realize_planned_system_,
          WMI::Method import_snapshot_,
          const WMI* service_)
        : clz{std::move(clz_)}
        , export_system{std::move(export_system_)}
        , import_system{std::move(import_system_)}
        , destroy_system{std::move(destroy_system_)}
        , modify_system_settings{std::move(modify_system_settings_)}
        , modify_resource_settings{std::move(modify_resource_settings_)}
        , validate_planned_system{std::move(validate_planned_system_)}
        , realize_planned_system{std::move(realize_planned_system_)}
        , import_snapshot{std::move(import_snapshot_)}
        , wmi{service_}
    {
    }

    WMI::Class clz;
    WMI::Method export_system;
    WMI::Method import_system;
    WMI::Method destroy_system;
    WMI::Method modify_system_settings;
    WMI::Method modify_resource_settings;
    WMI::Method validate_planned_system;
    WMI::Method realize_planned_system;
    WMI::Method import_snapshot;
    const WMI* wmi;
  };

  struct throwaway {};

  WMI::Result<uint32_t> ExportSystemDefinition(
      const WMI::ComputerSystem& TargetSystem,
      const WMI::SystemString& ExportDirectory,
      const WMI::SystemString& ExportSettingData)
  {
    auto params = clz->export_system.CreateParamInstance();
    if (!params) { return std::nullopt; }
    if (!params->Put(L"ExportDirectory", ExportDirectory)
        || !params->Put(L"ExportSettingData", ExportSettingData)
        || !params->Put(L"ComputerSystem", TargetSystem)) {
      return std::nullopt;
    }

    auto result
        = clz->wmi->ExecMethod(*this, clz->export_system, params->parameter);
    if (!result) { return std::nullopt; }

    if (!clz->wmi->WaitForJobCompletion(result)) { return std::nullopt; }

    return 0;
  }

  struct PlannedComputerSystem : public WMI::ComputerSystem {};

  WMI::Result<PlannedComputerSystem> ImportSystemDefinition(
      const WMI::SystemString& SystemDefinitionFile,
      const WMI::SystemString& SnapshotFolder,
      bool GenerateNewSystemIdentifier)
  {
    auto params = clz->import_system.CreateParamInstance();
    if (!params) { return std::nullopt; }
    if (!params->Put(L"SystemDefinitionFile", SystemDefinitionFile)
        //|| !params->Put(L"SnapshotFolder", SnapshotFolder)
        || !params->Put(L"GenerateNewSystemIdentifier",
                        GenerateNewSystemIdentifier)) {
      return std::nullopt;
    }

    auto result
        = clz->wmi->ExecMethod(*this, clz->import_system, params->parameter);
    if (!result) { return std::nullopt; }

    if (!clz->wmi->WaitForJobCompletion(result)) { return std::nullopt; }

#if 0
    VARIANT job_ref;
    CIMTYPE type;
    auto hres = result->Get(L"Job", 0, &job_ref, &type, 0);

    if (FAILED(hres)) {
      LOG(L"Could not get job param.  Err={} ({:X})", WMI::ErrorString(hres),
          (uint64_t)hres);

      return std::nullopt;
    }

    if (V_VT(&job_ref) != VT_BSTR) {
      LOG(L"Bad job ref value type.  Type = {}", job_ref.vt);
      VariantClear(&job_ref);
      return std::nullopt;
    }

    if (type != CIM_REFERENCE) {
      LOG(L"Bad job type {}", type);
      VariantClear(&job_ref);
      return std::nullopt;
    }

    WMI::SystemString job_name{std::wstring_view{V_BSTR(&job_ref)}};
    VariantClear(&job_ref);

    auto related = clz->wmi->GetRelatedOfClass(job_name.as_view(),
                                               L"Msvm_PlannedComputerSystem");
    if (!related) { return std::nullopt; }

    if (related->size() != 1) {
      LOG(L"Result Size != 1");
      return std::nullopt;
    }

    auto& res = related->at(0);
#else
    VARIANT psystem_ref;
    CIMTYPE type;
    auto hres = result->Get(L"ImportedSystem", 0, &psystem_ref, &type, 0);

    if (FAILED(hres)) {
      LOG(L"Could not get ImportedSystem param.  Err={} ({:X})",
          WMI::ErrorString(hres), (uint64_t)hres);

      return std::nullopt;
    }

    if (V_VT(&psystem_ref) != VT_BSTR) {
      LOG(L"Bad ImportedSystem ref value type.  Type = {}", psystem_ref.vt);
      VariantClear(&psystem_ref);
      return std::nullopt;
    }

    if (type != CIM_REFERENCE) {
      LOG(L"Bad ImportedSystem type {}", type);
      VariantClear(&psystem_ref);
      return std::nullopt;
    }

    WMI::SystemString psystem_name{std::wstring_view{V_BSTR(&psystem_ref)}};
    VariantClear(&psystem_ref);

    LOG(L"ImportedSystemPath = {}", psystem_name.as_view());

    IWbemClassObject* imported_system = nullptr;
    auto get_res = clz->wmi->service->GetObject(psystem_name.get(), 0, NULL,
                                                &imported_system, NULL);
    if (FAILED(get_res)) {
      LOG(L"GetObject({}) failed.  Err={} ({:X})", psystem_name.as_view(),
          WMI::ErrorString(get_res), (uint32_t)get_res);
      return std::nullopt;
    }

    LOG(L"ImportedSystem = {}", WMI::ObjectAsString(imported_system).as_view());

    WMI::ClassObject res{imported_system};

#endif

    result->Release();
    return PlannedComputerSystem{std::move(res)};
  }

  WMI::Result<throwaway> DestroySystem(const WMI::ComputerSystem& target)
  {
    auto params = clz->destroy_system.CreateParamInstance();
    if (!params) { return std::nullopt; }

    if (!params->Put(L"AffectedSystem", target)) { return std::nullopt; }

    auto result
        = clz->wmi->ExecMethod(*this, clz->destroy_system, params->parameter);
    if (!result) { return std::nullopt; }

    if (!clz->wmi->WaitForJobCompletion(result)) { return std::nullopt; }

    result->Release();
    return throwaway{};
  }

  WMI::Result<throwaway> ModifySystemSettings(const WMI::SystemString& settings)
  {
    auto params = clz->modify_system_settings.CreateParamInstance();
    if (!params) { return std::nullopt; }

    if (!params->Put(L"SystemSettings", settings)) { return std::nullopt; }

    auto result = clz->wmi->ExecMethod(*this, clz->modify_system_settings,
                                       params->parameter);
    if (!result) { return std::nullopt; }

    if (!clz->wmi->WaitForJobCompletion(result)) { return std::nullopt; }

    result->Release();
    return throwaway{};
  }

  WMI::Result<throwaway> ModifyResourceSettings(
      const std::vector<WMI::SystemString>& resource_settings)
  {
    auto params = clz->modify_resource_settings.CreateParamInstance();
    if (!params) { return std::nullopt; }

    if (!params->Put(L"ResourceSettings", resource_settings)) {
      return std::nullopt;
    }

    auto result = clz->wmi->ExecMethod(*this, clz->modify_resource_settings,
                                       params->parameter);
    if (!result) { return std::nullopt; }

    if (!clz->wmi->WaitForJobCompletion(result)) { return std::nullopt; }

    result->Release();
    return throwaway{};
  }

  WMI::Result<throwaway> ValidatePlannedSystem(
      const PlannedComputerSystem& planned)
  {
    auto params = clz->validate_planned_system.CreateParamInstance();
    if (!params) { return std::nullopt; }

    if (!params->Put(L"PlannedSystem", planned)) { return std::nullopt; }

    auto result = clz->wmi->ExecMethod(*this, clz->validate_planned_system,
                                       params->parameter);
    if (!result) { return std::nullopt; }

    if (!clz->wmi->WaitForJobCompletion(result)) { return std::nullopt; }

    result->Release();
    return throwaway{};
  }
  WMI::Result<throwaway> RealizePlannedSystem(PlannedComputerSystem&& planned)
  {
    auto params = clz->realize_planned_system.CreateParamInstance();
    if (!params) { return std::nullopt; }

    if (!params->Put(L"PlannedSystem", planned)) { return std::nullopt; }

    auto result = clz->wmi->ExecMethod(*this, clz->realize_planned_system,
                                       params->parameter);
    if (!result) { return std::nullopt; }

    if (!clz->wmi->WaitForJobCompletion(result)) { return std::nullopt; }

    result->Release();
    return throwaway{};
  }

  WMI::Result<throwaway> ImportSnapshotDefinitions(
      const PlannedComputerSystem& planned,
      const WMI::SystemString& snapshot_folder)
  {
    auto params = clz->import_snapshot.CreateParamInstance();
    if (!params) { return std::nullopt; }

    if (!params->Put(L"PlannedSystem", planned)
        || !params->Put(L"SnapshotFolder", snapshot_folder)) {
      return std::nullopt;
    }

    auto result
        = clz->wmi->ExecMethod(*this, clz->import_snapshot, params->parameter);
    if (!result) { return std::nullopt; }

    if (!clz->wmi->WaitForJobCompletion(result)) { return std::nullopt; }

    result->Release();
    return throwaway{};
  }

 private:
  VirtualSystemManagementService(ClassObject super, const Class* clz_)
      : ClassObject(std::move(super)), clz{clz_}
  {
  }

  const Class* clz;
};

struct ReferencePoint : public WMI::ClassObject {
  explicit ReferencePoint(WMI::ClassObject obj)
      : WMI::ClassObject{std::move(obj)}
  {
  }
};

class VirtualSystemSnapshotService : WMI::ClassObject {
 public:
  struct Class {
    friend VirtualSystemSnapshotService;

    static WMI::Result<Class> load(const WMI& wmi)
    {
      auto clz = wmi.LoadClassByName(L"Msvm_VirtualSystemSnapshotService");
      if (!clz) { return std::nullopt; }

      auto create_snapshot = wmi.LoadMethodByName(*clz, L"CreateSnapshot");
      if (!create_snapshot) { return std::nullopt; }

      auto destroy_snapshot = wmi.LoadMethodByName(*clz, L"DestroySnapshot");
      if (!destroy_snapshot) { return std::nullopt; }

      auto convert_to_reference_point
          = wmi.LoadMethodByName(*clz, L"ConvertToReferencePoint");
      if (!convert_to_reference_point) { return std::nullopt; }

      return Class{std::move(clz.value()), std::move(create_snapshot.value()),
                   std::move(destroy_snapshot.value()),
                   std::move(convert_to_reference_point.value()), &wmi};
    }

    WMI::Result<VirtualSystemSnapshotService> Get() const
    {
      WMI::SystemString query{std::wstring_view{
          L"SELECT * FROM Msvm_VirtualSystemSnapshotService"}};
      auto* obj = wmi->QueryFirst(query);
      if (!obj) { return std::nullopt; }

      {
        auto str = WMI::ObjectAsString(obj);
        LOG(L"VirtualSystemSnapshotService = {}", str.as_view());
      }

      WMI::ClassObject service{obj};

      return VirtualSystemSnapshotService{std::move(service), this};
    }

   protected:
    WMI::Class clz;
    WMI::Method create_snapshot;
    WMI::Method destroy_snapshot;
    WMI::Method convert_to_reference_point;
    const WMI* wmi;

   private:
    Class(WMI::Class clz_,
          WMI::Method create_snapshot_,
          WMI::Method destroy_snapshot_,
          WMI::Method convert_to_reference_point_,
          const WMI* wmi_)
        : clz{std::move(clz_)}
        , create_snapshot{std::move(create_snapshot_)}
        , destroy_snapshot{std::move(destroy_snapshot_)}
        , convert_to_reference_point{std::move(convert_to_reference_point_)}
        , wmi{wmi_}
    {
    }
  };

  struct Snapshot : public WMI::ClassObject {
    explicit Snapshot(WMI::ClassObject obj) : WMI::ClassObject{std::move(obj)}
    {
    }
  };
  struct throwaway {};

  enum class SnapshotType : std::int32_t
  {
    FullSnapshot = 2,
    DiskSnapshot = 3,
    RecoverySnapshot = 32768,
  };

  WMI::Result<Snapshot> CreateSnapshot(
      const WMI::ComputerSystem& target,
      const WMI::SystemString& snapshot_settings,
      SnapshotType snapshot_type)
  {
    auto params = clz->create_snapshot.CreateParamInstance();
    if (!params) { return std::nullopt; }

    if (!params->Put(L"AffectedSystem", target)
        || !params->Put(L"SnapshotSettings", snapshot_settings)
        || !params->Put(L"SnapshotType", to_underlying(snapshot_type))) {
      return std::nullopt;
    }

    auto result = clz->wmi->ExecMethod(*this, clz->create_snapshot,
                                       params.value().parameter);

    if (!result) { return std::nullopt; }

    if (!clz->wmi->WaitForJobCompletion(result)) { return std::nullopt; }

#if 0
    VARIANT snapshot_ref;
    CIMTYPE type;
    auto hres = result->Get(L"ResultingSnapshot", 0, &snapshot_ref, &type, 0);

    if (FAILED(hres)) {
      LOG(L"Could not get snapshot param.  Err={} ({:X})", WMI::ErrorString(hres),
          (uint64_t)hres);

      return std::nullopt;
    }

    if (V_VT(&snapshot_ref) != VT_BSTR) {
      LOG(L"Bad snapshot ref value type.  Type = {}", snapshot_ref.vt);
      VariantClear(&snapshot_ref);
      return std::nullopt;
    }

    if (type != CIM_REFERENCE) {
      LOG(L"Bad snapshot type {}", type);
      VariantClear(&snapshot_ref);
      return std::nullopt;
    }

    WMI::SystemString snapshot_name{V_BSTR(&snapshot_ref)};
    VariantClear(&snapshot_ref);

    IWbemClassObject* snapshot;
    auto get_snapshot = clz->wmi->service->GetObject(snapshot_name.get(), 0,
                                                     NULL, &snapshot, NULL);

    if (FAILED(get_snapshot)) {
      LOG(L"Could not get snapshot.  Err={} ({:X})",
          WMI::ErrorString(get_snapshot),
          (uint64_t)get_snapshot);

      return std::nullopt;
    }
    WMI::ClassObject res{ snapshot };
#else
    VARIANT job_ref;
    CIMTYPE type;
    auto hres = result->Get(L"Job", 0, &job_ref, &type, 0);

    if (FAILED(hres)) {
      LOG(L"Could not get job param.  Err={} ({:X})", WMI::ErrorString(hres),
          (uint64_t)hres);

      return std::nullopt;
    }

    if (V_VT(&job_ref) != VT_BSTR) {
      LOG(L"Bad job ref value type.  Type = {}", job_ref.vt);
      VariantClear(&job_ref);
      return std::nullopt;
    }

    if (type != CIM_REFERENCE) {
      LOG(L"Bad job type {}", type);
      VariantClear(&job_ref);
      return std::nullopt;
    }

    WMI::SystemString job_name{std::wstring_view{V_BSTR(&job_ref)}};
    VariantClear(&job_ref);

    auto related = clz->wmi->GetRelatedOfClass(job_name.as_view(),
                                               L"CIM_VirtualSystemSettingData");

    if (!related) { return std::nullopt; }

    if (related->size() != 1) {
      LOG(L"Result Size != 1");
      return std::nullopt;
    }

    auto& res = related->at(0);
#endif
    result->Release();


    return Snapshot{std::move(res)};
  }

  WMI::Result<throwaway> DestroySnapshot(Snapshot&& snapshot)
  {
    auto params = clz->destroy_snapshot.CreateParamInstance();
    if (!params) { return std::nullopt; }

    if (!params->Put(L"AffectedSnapshot", snapshot)) { return std::nullopt; }

    auto result = clz->wmi->ExecMethod(*this, clz->destroy_snapshot,
                                       params.value().parameter);
    if (!result) { return std::nullopt; }

    if (!clz->wmi->WaitForJobCompletion(result)) { return std::nullopt; }

    // TODO: check if the job was successful
    return throwaway{};
  }

  WMI::Result<ReferencePoint> ConvertToReferencePoint(Snapshot&& snapshot)
  {
    auto params = clz->convert_to_reference_point.CreateParamInstance();
    if (!params) { return std::nullopt; }

    // this method has an  undocumented ReferencePointSettings parameter.
    // if set, the method will error out, so we ignore it here
    if (!params->Put(L"AffectedSnapshot", snapshot)) { return std::nullopt; }

    auto result = clz->wmi->ExecMethod(*this, clz->convert_to_reference_point,
                                       params.value().parameter);
    if (!result) { return std::nullopt; }

    if (!clz->wmi->WaitForJobCompletion(result)) { return std::nullopt; }

    VARIANT job_ref;
    CIMTYPE type;
    auto hres = result->Get(L"Job", 0, &job_ref, &type, 0);

    if (FAILED(hres)) {
      LOG(L"Could not get job param.  Err={} ({:X})", WMI::ErrorString(hres),
          (uint64_t)hres);

      return std::nullopt;
    }

    if (V_VT(&job_ref) != VT_BSTR) {
      LOG(L"Bad job ref value type.  Type = {}", job_ref.vt);
      VariantClear(&job_ref);
      return std::nullopt;
    }

    if (type != CIM_REFERENCE) {
      LOG(L"Bad job type {}", type);
      VariantClear(&job_ref);
      return std::nullopt;
    }

    WMI::SystemString job_name{std::wstring_view{V_BSTR(&job_ref)}};
    VariantClear(&job_ref);

    auto related = clz->wmi->GetRelatedOfClass(
        job_name.as_view(), L"Msvm_VirtualSystemReferencePoint");

    if (!related) { return std::nullopt; }

    if (related->size() != 1) {
      LOG(L"Result Size != 1");
      return std::nullopt;
    }

    auto& res = related->at(0);
    result->Release();

    return ReferencePoint{std::move(res)};
  }

 private:
  VirtualSystemSnapshotService(ClassObject super, const Class* clz_)
      : ClassObject(std::move(super)), clz{clz_}
  {
  }

  const Class* clz;
};

class VirtualSystemReferencePointService : WMI::ClassObject {
 public:
  struct Class {
    friend VirtualSystemReferencePointService;

    static WMI::Result<Class> load(const WMI& wmi)
    {
      auto clz
          = wmi.LoadClassByName(L"Msvm_VirtualSystemReferencePointService");
      if (!clz) { return std::nullopt; }

      auto destroy_reference_point
          = wmi.LoadMethodByName(*clz, L"DestroyReferencePoint");
      if (!destroy_reference_point) { return std::nullopt; }

      return Class{std::move(clz.value()),
                   std::move(destroy_reference_point.value()), &wmi};
    }

    WMI::Result<VirtualSystemReferencePointService> Get() const
    {
      WMI::SystemString query{std::wstring_view{
          L"SELECT * FROM Msvm_VirtualSystemReferencePointService"}};
      auto* obj = wmi->QueryFirst(query);
      if (!obj) { return std::nullopt; }

      {
        auto str = WMI::ObjectAsString(obj);
        LOG(L"VirtualSystemReferencePointService = {}", str.as_view());
      }

      WMI::ClassObject service{obj};

      return VirtualSystemReferencePointService{std::move(service), this};
    }

   protected:
    WMI::Class clz;
    WMI::Method destroy_reference_point;
    const WMI* wmi;

   private:
    Class(WMI::Class clz_,
          WMI::Method destroy_reference_point_,
          const WMI* wmi_)
        : clz{std::move(clz_)}
        , destroy_reference_point{std::move(destroy_reference_point_)}
        , wmi{wmi_}
    {
    }
  };

  struct throwaway {};

  WMI::Result<throwaway> DestroyReferencePoint(ReferencePoint&& reference_point)
  {
    auto params = clz->destroy_reference_point.CreateParamInstance();
    if (!params) { return std::nullopt; }

    if (!params->Put(L"AffectedReferencePoint", reference_point)) {
      return std::nullopt;
    }

    auto result = clz->wmi->ExecMethod(*this, clz->destroy_reference_point,
                                       params.value().parameter);
    if (!result) { return std::nullopt; }

    if (!clz->wmi->WaitForJobCompletion(result)) { return std::nullopt; }

    // TODO: check if the job was successful
    return throwaway{};
  }

 private:
  VirtualSystemReferencePointService(ClassObject super, const Class* clz_)
      : ClassObject(std::move(super)), clz{clz_}
  {
  }

  const Class* clz;
};

static WMI::Result<WMI::SystemString> HardDiskPath(WMI::ClassObject& setting)
{
  VARIANT host_resource;
  CIMTYPE type;

  auto get_res
      = setting.system->Get(L"HostResource", 0, &host_resource, &type, 0);
  if (FAILED(get_res)) {
    LOG(L"Could not get HostResource.  Err={} ({})", WMI::ErrorString(get_res),
        (uint32_t)get_res);
    return std::nullopt;
  }

  if (type != (CIM_STRING | CIM_FLAG_ARRAY)) {
    LOG(L"Bad type of HostResource.  Type={}", type);
    return std::nullopt;
  }

  if (V_VT(&host_resource) != (VT_BSTR | VT_ARRAY)) {
    LOG(L"Bad type of host_resource Variant.  Type={}", V_VT(&host_resource));
    return std::nullopt;
  }

  SAFEARRAY* arr = V_ARRAY(&host_resource);

  if (auto dim = SafeArrayGetDim(arr); dim != 1) {
    LOG(L"Bad array dim.  Expected 1.  Dim = {}", dim);
    return std::nullopt;
  }

  LONG lbound, ubound;
  auto lbound_res = SafeArrayGetLBound(arr, 1, &lbound);
  if (FAILED(lbound_res)) {
    LOG(L"Could not get lower bound of SAFEARRAY {}.  Err={}", (const void*)arr,
        lbound_res);
    return std::nullopt;
  }
  auto ubound_res = SafeArrayGetUBound(arr, 1, &ubound);
  if (FAILED(ubound_res)) {
    LOG(L"Could not get upper bound of SAFEARRAY {}.  Err={}", (const void*)arr,
        ubound_res);
    return std::nullopt;
  }
  auto size = ubound - lbound + 1;
  if (size != 1) {
    LOG(L"Bad array size.  Expected 1.  Size = {}", size);
    return std::nullopt;
  }

  BSTR disk_path;
  LONG Index = 0;
  auto hres = SafeArrayGetElement(arr, &Index, (void*)&disk_path);
  if (FAILED(hres)) {
    LOG(L"Could not get first element of array.  Err={}", hres);
    return std::nullopt;
  }

  std::wstring_view dpath{disk_path, SysStringLen(disk_path)};

  if (dpath.empty()) {
    LOG(L"Got empty disk path.");
    return std::nullopt;
  }

  return WMI::SystemString{dpath};
}

std::optional<std::wstring> ReplacePath(std::wstring_view path_to_file,
                                        std::wstring_view new_directory)
{
  if (path_to_file.empty()) {
    LOG(L"got empty path");
    return std::nullopt;
  }

  auto pos = path_to_file.find_last_of(L"\\/");
  if (pos == path_to_file.npos) {
    LOG(L"path {} is just a file name.", path_to_file);
    return std::nullopt;
  }

  std::wstring new_disk_path{new_directory};
  new_disk_path += L"\\";
  new_disk_path += path_to_file.substr(pos + 1);

  LOG(L"Updated path {} --> {}", path_to_file, new_disk_path);

  return new_disk_path;
}

static bool UpdatePath(WMI::ClassObject& setting, std::wstring_view new_path)
{
  VARIANT host_resource;
  CIMTYPE type;

  auto get_res
      = setting.system->Get(L"HostResource", 0, &host_resource, &type, 0);
  if (FAILED(get_res)) {
    LOG(L"Could not get HostResource.  Err={} ({})", WMI::ErrorString(get_res),
        (uint32_t)get_res);
    return false;
  }

  if (type != (CIM_STRING | CIM_FLAG_ARRAY)) {
    LOG(L"Bad type of HostResource.  Type={}", type);
    return false;
  }

  if (V_VT(&host_resource) != (VT_BSTR | VT_ARRAY)) {
    LOG(L"Bad type of host_resource Variant.  Type={}", V_VT(&host_resource));
    return false;
  }

  SAFEARRAY* arr = V_ARRAY(&host_resource);

  if (auto dim = SafeArrayGetDim(arr); dim != 1) {
    LOG(L"Bad array dim.  Expected 1.  Dim = {}", dim);
    return false;
  }

  LONG lbound, ubound;
  auto lbound_res = SafeArrayGetLBound(arr, 1, &lbound);
  if (FAILED(lbound_res)) {
    LOG(L"Could not get lower bound of SAFEARRAY {}.  Err={}", (const void*)arr,
        lbound_res);
    return false;
  }
  auto ubound_res = SafeArrayGetUBound(arr, 1, &ubound);
  if (FAILED(ubound_res)) {
    LOG(L"Could not get upper bound of SAFEARRAY {}.  Err={}", (const void*)arr,
        ubound_res);
    return false;
  }
  auto size = ubound - lbound + 1;
  if (size != 1) {
    LOG(L"Bad array size.  Expected 1.  Size = {}", size);
    return false;
  }

  BSTR disk_path;
  LONG Index = 0;
  auto hres = SafeArrayGetElement(arr, &Index, (void*)&disk_path);
  if (FAILED(hres)) {
    LOG(L"Coud not get first element of array.  Err={}", hres);
    return false;
  }

  // disk_path is now the bad path.  We need to combine its filename with the
  // given path.

  std::wstring_view dpath{disk_path, SysStringLen(disk_path)};

  auto new_disk_path = ReplacePath(dpath, new_path);
  if (!new_disk_path) {
    LOG(L"could not replace directory of {} with {}", dpath, new_path);
    return false;
  }

  // TODO: free disk_path; or do we ?

  BSTR to_insert
      = SysAllocStringLen(new_disk_path->data(), (UINT)new_disk_path->size());

  auto put_res = SafeArrayPutElement(arr, &Index, (void*)to_insert);
  if (FAILED(put_res)) {
    LOG(L"Could not store updated path.  Err={}", put_res);
    return false;
  }

  auto wput_res = setting.system->Put(L"HostResource", 0, &host_resource, type);
  if (FAILED(wput_res)) {
    LOG(L"Could not update HostResource.  Err={} ({})",
        WMI::ErrorString(wput_res), (uint32_t)wput_res);
    return false;
  }

  return true;
}

class VirtualSystemSnapshotSettingData {
 public:
  struct Class {
    static WMI::Result<Class> load(const WMI& wmi)
    {
      auto clz = wmi.LoadClassByName(L"Msvm_VirtualSystemSnapshotSettingData");

      if (!clz) { return std::nullopt; }

      return Class{std::move(clz.value()), &wmi};
    }

    WMI::Result<WMI::SystemString> GetText(
        const VirtualSystemSnapshotSettingData& settings)
    {
      IWbemClassObject* inst = nullptr;
      {
        auto hres = clz.class_ptr->SpawnInstance(0, &inst);
        if (FAILED(hres)) {
          LOG(L"Could not create {} instance.  Err={} ({:X})",
              clz.Name.as_view(), WMI::ErrorString(hres), (uint64_t)hres);
          return std::nullopt;
        }
      }

      {
        VARIANT Arg;
        V_VT(&Arg) = VT_I4;
        V_I4(&Arg) = to_underlying(settings.consistency_level);
        auto hres = inst->Put(L"ConsistencyLevel", 0, &Arg, CIM_UINT8);
        if (FAILED(hres)) {
          LOG(L"Set(SnapshotSetting, ConsistencyLevel, {}) failed.  Err={} "
              L"({:X})",
              to_underlying(settings.consistency_level), WMI::ErrorString(hres),
              (uint64_t)hres);
          return std::nullopt;
        }
      }
      {
        VARIANT Arg;
        V_VT(&Arg) = VT_I4;
        V_I4(&Arg) = to_underlying(settings.guest_backup_type);
        auto hres = inst->Put(L"GuestBackupType", 0, &Arg, CIM_UINT8);
        if (FAILED(hres)) {
          LOG(L"Set(SnapshotSetting, GuestBackupType, {}) failed.  Err={} "
              L"({:X})",
              to_underlying(settings.guest_backup_type), WMI::ErrorString(hres),
              (uint64_t)hres);
          return std::nullopt;
        }
      }
      {
        VARIANT Arg;
        V_VT(&Arg) = VT_BOOL;
        V_BOOL(&Arg) = settings.ignore_non_snapshottable_disks;
        auto hres
            = inst->Put(L"IgnoreNonSnapshottableDisks", 0, &Arg, CIM_BOOLEAN);
        if (FAILED(hres)) {
          LOG(L"Set(SnapshotSetting, IgnoreNonSnapshottableDisks, {}) failed.  "
              L"Err={} ({:X})",
              settings.ignore_non_snapshottable_disks, WMI::ErrorString(hres),
              (uint64_t)hres);
          return std::nullopt;
        }
      }

      auto Text = WMI::ObjectAsXML({inst});
      inst->Release();

      return Text;
    }

   protected:
    WMI::Class clz;
    const WMI* wmi;

   private:
    Class(WMI::Class clz_, const WMI* wmi_) : clz{std::move(clz_)}, wmi{wmi_} {}
  };

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

  ConsistencyLevel consistency_level;
  GuestBackupType guest_backup_type;
  bool ignore_non_snapshottable_disks;
};

enum class TestType
{
  Full,
  FullAndIncr,
  Restore
};

WMI::Result<WMI::ClassObject> FullBackup(const WMI& service,
                                         std::wstring_view vm_name,
                                         std::wstring_view backup_directory)
{
  auto vsms_clz = VirtualSystemManagementService::Class::load(service);
  if (!vsms_clz) { return std::nullopt; }
  auto vsss_clz = VirtualSystemSnapshotService::Class::load(service);
  if (!vsss_clz) { return std::nullopt; }
  auto vsesd = VirtualSystemExportSettingData::Class::load(service);
  if (!vsesd) { return std::nullopt; }
  auto vsssd_clz = VirtualSystemSnapshotSettingData::Class::load(service);
  if (!vsssd_clz) { return std::nullopt; }

  auto vsms = vsms_clz->Get();
  if (!vsms) { return std::nullopt; }
  auto vsss = vsss_clz->Get();
  if (!vsss) { return std::nullopt; }

  auto vm = service.GetVMByName(vm_name);

  if (!vm) { return std::nullopt; }

  WMI::SystemString directory{backup_directory};


  VirtualSystemSnapshotSettingData ssettings = {
      .consistency_level
      = VirtualSystemSnapshotSettingData::ConsistencyLevel::CrashConsistent,
      .ignore_non_snapshottable_disks = true,
  };

  auto snapshot_settings = vsssd_clz->GetText(ssettings);

  if (!snapshot_settings) { return std::nullopt; }

  LOG(L"Snapshot Settings = {}", snapshot_settings->as_view());

  auto snapshot = vsss->CreateSnapshot(
      vm.value(), snapshot_settings.value(),
      VirtualSystemSnapshotService::SnapshotType::RecoverySnapshot);
  if (!snapshot) { return std::nullopt; }


  {
    WMI::SystemString new_snapshot_name{
        std::wstring_view{L"Bareos Snapshot - Full"}};

    VARIANT Name;
    V_VT(&Name) = VT_BSTR;
    V_BSTR(&Name) = new_snapshot_name.get();
    snapshot->system->Put(L"ElementName", 0, &Name, 0);
    auto xml = WMI::ObjectAsXML(snapshot.value());
    if (!xml) { return std::nullopt; }
    auto rename_res = vsms->ModifySystemSettings(xml.value());
    if (!rename_res) { return std::nullopt; }
  }

  LOG(L"Snapshot = {}", WMI::ObjectAsString(snapshot->system).as_view());

  auto snapshot_path = snapshot->Path();
  if (!snapshot_path) { return std::nullopt; }

  VirtualSystemExportSettingData settings = {
      .snapshot_virtual_system_path = std::wstring{snapshot_path->as_view()},
      .backup_intent = VirtualSystemExportSettingData::BackupIntent::Merge,
      .copy_snapshot_configuration = VirtualSystemExportSettingData::
          CopySnapshotConfiguration::ExportOneSnapshotForBackup,
      .capture_live_state
      = VirtualSystemExportSettingData::CaptureLiveState::CrashConsistent,
      .copy_vm_runtime_information = false,
      .copy_vm_storage = true,
      .create_vm_export_subdirectory = true,
  };

  WMI::Result options = vsesd->GetText(settings);
  if (!options) { return std::nullopt; }

  auto res = vsms->ExportSystemDefinition(vm.value(), directory, *options);
  if (!res) { return std::nullopt; }

  auto refpoint = vsss->ConvertToReferencePoint(std::move(snapshot).value());
  return refpoint;
}

bool WriteFile_All(HANDLE handle, std::span<const char> Data)
{
  if (Data.size() > std::numeric_limits<DWORD>::max()) {
    LOG(L"WriteFile_All() failed.  Cannot write more than {} bytes at once",
        std::numeric_limits<DWORD>::max());
    return false;
  }
  DWORD BytesToWrite = (DWORD)(Data.size());
  DWORD BytesWritten = 0;
  DWORD BytesWrittenThisCall = 0;

  const char* Current = Data.data();

  while (BytesToWrite > 0) {
    if (!WriteFile(handle, Current, BytesToWrite, &BytesWrittenThisCall,
                   NULL)) {
      auto write_res = GetLastError();
      LOG(L"WriteFile({}, {}) failed.  Err={} ({:X})", Current - Data.data(),
          BytesToWrite, WMI::ErrorString(write_res), (uint32_t)write_res);
      return false;
    }

    BytesToWrite -= BytesWrittenThisCall;
    Current += BytesWrittenThisCall;
  }

  return true;
}

struct range_header {
  std::uint64_t Offset;
  std::uint64_t Size;
};

bool WriteRange(HANDLE /* OVERLAPPED */ in_handle,
                HANDLE /* SYNC */ out_handle,
                std::span<char> Buffer,
                ULONG64 Offset,
                ULONG64 Size)
{
  {
    range_header Header;
    Header.Offset = Offset;
    Header.Size = Size;

    std::span<const char> HeaderData{reinterpret_cast<const char*>(&Header),
                                     sizeof(Header)};

    if (!WriteFile_All(out_handle, HeaderData)) { return false; }
  }

  ULONG64 BytesTransferred = 0;
  while (BytesTransferred < Size) {
    OVERLAPPED Overlapped = {};
    Overlapped.Offset = DWORD{Offset & 0xFFFFFFFF};
    Overlapped.OffsetHigh = DWORD{(Offset >> 32) & 0xFFFFFFFF};
    DWORD BytesRead = 0;
    DWORD BytesToRead
        = (DWORD)(std::min({Buffer.size(), Size - BytesTransferred,
                            ULONG64{std::numeric_limits<DWORD>::max()}}));
    if (!ReadFile(in_handle, Buffer.data(), BytesToRead, &BytesRead,
                  &Overlapped)) {
      auto read_res = GetLastError();

      if (read_res == ERROR_IO_PENDING) {
        if (GetOverlappedResult(in_handle, &Overlapped, &BytesRead, TRUE)) {
          goto read_ok;
        }
      }
      LOG(L"ReadFile({}, {{{}, {}}}) failed.  Err={} ({:X})", BytesToRead,
          Overlapped.Offset, Overlapped.OffsetHigh, WMI::ErrorString(read_res),
          (uint32_t)read_res);
      return false;
    }

  read_ok:
    if (!WriteFile_All(out_handle, Buffer.subspan(0, BytesRead))) {
      return false;
    }

    BytesTransferred += BytesRead;
  }

  return true;
}

bool BackupDisk(const WMI::SystemString& hdd_path,
                const wchar_t* rct_id,
                std::wstring_view dir)
{
  std::optional new_path = ReplacePath(hdd_path.as_view(), dir);

  if (!new_path) {
    LOG(L"could not replace directory of {} with {}", hdd_path.as_view(), dir);
    return false;
  }


  LOG(L"backing up {} to {}", hdd_path.as_view(), new_path.value());

  HANDLE result_handle = CreateFileW(new_path->c_str(), GENERIC_WRITE, 0, NULL,
                                     CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);

  if (result_handle == INVALID_HANDLE_VALUE) {
    auto create_res = GetLastError();
    LOG(L"could not create file {}. Err={} ({})", new_path.value(),
        WMI::ErrorString(create_res), create_res);
    return false;
  }

  HANDLE disk_handle = {};

  VIRTUAL_STORAGE_TYPE vst = {
      .DeviceId = VIRTUAL_STORAGE_TYPE_DEVICE_UNKNOWN,
      .VendorId = VIRTUAL_STORAGE_TYPE_VENDOR_UNKNOWN,
  };

  OPEN_VIRTUAL_DISK_PARAMETERS params = {};
  params.Version = OPEN_VIRTUAL_DISK_VERSION_2;
  params.Version2.ReadOnly = TRUE;
  auto res
      = OpenVirtualDisk(&vst, hdd_path.get(), VIRTUAL_DISK_ACCESS_NONE,
                        OPEN_VIRTUAL_DISK_FLAG_NONE, &params, &disk_handle);
  if (res != ERROR_SUCCESS) {
    LOG(L"OpenVirtualDisk({}) failed.  Err={} ({:X})", hdd_path.as_view(),
        WMI::ErrorString(res), (uint64_t)res);
    return false;
  }
  LOG(L"Opened virual disk at {}", (const void*)disk_handle);
  // APPLY_SNAPSHOT_VHDSET_PARAMETERS snap_params = {
  //   .Version = APPLY_SNAPSHOT_VHDSET_VERSION_1,
  //   .Version1 = {
  //     //.SnapshotId = snapshot_id,
  //   }
  // };
  // APPLY_SNAPSHOT_VHDSET_FLAG snap_flags;
  // auto snap_res = ApplySnapshotVhdSet(disk_handle, &snap_params, snap_flags);

  // {
  //   alignas(GET_VIRTUAL_DISK_INFO) char info_buffer[500] = {};
  //   auto* disk_info = reinterpret_cast<GET_VIRTUAL_DISK_INFO*>(info_buffer);
  //   disk_info->Version = GET_VIRTUAL_DISK_INFO_CHANGE_TRACKING_STATE;
  //   ULONG VirtualDiskInfoSize = sizeof(info_buffer);
  //   ULONG SizeOut = 0;
  //   auto disk_res = GetVirtualDiskInformation(disk_handle,
  //   &VirtualDiskInfoSize,
  //                                             disk_info, &SizeOut);
  //   if (disk_res != ERROR_SUCCESS) {
  //     LOG(L"GetVirtualDiskInfo({}) failed.  Err={} ({:X})",
  //     hdd_path.as_view(),
  //         WMI::ErrorString(disk_res), (uint64_t)disk_res);
  //     return false;
  //   }

  //   LOG(L"Size In = {} ({}), Size Out = {}", sizeof(disk_info),
  //       VirtualDiskInfoSize, SizeOut);

  //   LOG(L"RCT = {{ Enabled = {}, NewerChanges = {}, MostRecentId = {} }}",
  //       disk_info->ChangeTrackingState.Enabled,
  //       disk_info->ChangeTrackingState.NewerChanges,
  //       disk_info->ChangeTrackingState.MostRecentId);
  // }

  GET_VIRTUAL_DISK_INFO disk_info = {
      .Version = GET_VIRTUAL_DISK_INFO_SIZE,
  };
  ULONG VirtualDiskInfoSize = sizeof(disk_info);
  ULONG SizeOut = 0;
  auto disk_res = GetVirtualDiskInformation(disk_handle, &VirtualDiskInfoSize,
                                            &disk_info, &SizeOut);
  if (disk_res != ERROR_SUCCESS) {
    LOG(L"GetVirtualDiskInfo({}) failed.  Err={} ({:X})", hdd_path.as_view(),
        WMI::ErrorString(disk_res), (uint64_t)disk_res);
    return false;
  }

  LOG(L"Size In = {} ({}), Size Out = {}", sizeof(disk_info),
      VirtualDiskInfoSize, SizeOut);

  LOG(L"Size = {{ VirtualSize = {}, PhysicalSize = {}, BlockSize = {}, "
      L"SectorSize = {} }}",
      disk_info.Size.VirtualSize, disk_info.Size.PhysicalSize,
      disk_info.Size.BlockSize, disk_info.Size.SectorSize);

  ATTACH_VIRTUAL_DISK_PARAMETERS attach_params
      = {.Version = ATTACH_VIRTUAL_DISK_VERSION_1, .Version1 = {}};
  auto attach_res = AttachVirtualDisk(disk_handle, NULL,
                                      ATTACH_VIRTUAL_DISK_FLAG_NO_LOCAL_HOST
                                          | ATTACH_VIRTUAL_DISK_FLAG_READ_ONLY,
                                      0, &attach_params, 0);
  if (FAILED(attach_res)) {
    LOG(L"AttachVirtualDisk() failed.  Err={} ({:X})",
        WMI::ErrorString(attach_res), (uint32_t)attach_res);
    return false;
  }


  // std::wstring change_tracking_id = {};

  std::vector<char> Buffer;
  Buffer.resize(1024 * 1024);

  if (rct_id) {
    LOG(L"Doing RCT with rct_id = {}", rct_id);
    std::vector<QUERY_CHANGES_VIRTUAL_DISK_RANGE> ranges;
    ranges.resize(500);
    ULONG64 Start = 0;
    ULONG64 Size = disk_info.Size.VirtualSize;
    std::size_t total_size = 0;
    while (Size > 0) {
      LOG(L"Query Start = {}, Size = {}", Start, Size);
      ULONG64 bytes_processed = 0;
      ULONG range_count = ranges.size();
      auto query_res = QueryChangesVirtualDisk(
          disk_handle, rct_id, Start, Size,
          QUERY_CHANGES_VIRTUAL_DISK_FLAG_NONE, ranges.data(), &range_count,
          &bytes_processed);

      LOG(L"query_res = {}", query_res);

      if (query_res != ERROR_SUCCESS) {
        LOG(L"QueryChangesVirtualDisk({}, {}) failed.  Err={} ({:X})", Start,
            Size, WMI::ErrorString(query_res), (uint32_t)query_res);
        break;
      }

      LOG(L"Got {} ranges", range_count);
      for (ULONG i = 0; i < range_count; ++i) {
        auto& range = ranges[i];
        LOG(L"{}: {}-{} ({} bytes)", i, range.ByteOffset,
            range.ByteOffset + range.ByteLength, range.ByteLength);
        if (!WriteRange(disk_handle, result_handle, Buffer, range.ByteOffset,
                        range.ByteLength)) {
          LOG(L"Could not write range :/");
          return false;
        }
        total_size += range.ByteLength;
      }

      Start += bytes_processed;
      Size -= bytes_processed;
    }
    LOG(L"Total byte length = {}", total_size);
  } else {
    // ???
    // Maybe we just open all virtual disks in the chain, and back them up
    // seperately ??
  }

  if (auto detach_res
      = DetachVirtualDisk(disk_handle, DETACH_VIRTUAL_DISK_FLAG_NONE, 0);
      FAILED(detach_res)) {
    LOG(L"detach failed.  Err={} ({})", WMI::ErrorString(detach_res),
        detach_res);
  }

  CloseHandle(disk_handle);
  CloseHandle(result_handle);
  return true;
}

WMI::Result<WMI::SystemString> GetSnapshotId(const WMI::ClassObject& snapshot)
{
  VARIANT Arg;
  CIMTYPE Type;
  auto hres = snapshot.system->Get(L"ConfigurationID", 0, &Arg, &Type, nullptr);

  if (FAILED(hres)) {
    LOG(L"Get(Snapshot, ConfigurationID) failed.  Err={} ({:X})",
        WMI::ErrorString(hres), (uint64_t)hres);
    return std::nullopt;
  } else if (Type != CIM_STRING) {
    LOG(L"Get(Snapshot, ConfigurationID) returned bad type detected.  Type={}",
        Type);
    return std::nullopt;
  }

  return WMI::SystemString{V_BSTR(&Arg)};
}

WMI::Result<WMI::ClassObject> GetRefPoint(const WMI& wmi,
                                          std::wstring_view ref_path)
{
  WMI::SystemString copy{ref_path};
  IWbemClassObject* ref_point = nullptr;
  auto result = wmi.service->GetObject(copy.get(), 0, NULL, &ref_point, NULL);
  if (FAILED(result)) {
    LOG(L"GetRefPoint({}). Err={} ({:X})", ref_path, WMI::ErrorString(result),
        (uint32_t)result);
    return std::nullopt;
  }

  return WMI::ClassObject{ref_point};
}

WMI::Result<std::vector<std::wstring>> GetStringArray(
    const WMI::ClassObject& ref_point,
    const wchar_t* name)
{
  VARIANT array;
  CIMTYPE array_type;

  auto get_res = ref_point.system->Get(name, 0, &array, &array_type, 0);
  if (FAILED(get_res)) {
    LOG(L"Could not get HostResource.  Err={} ({})", WMI::ErrorString(get_res),
        (uint32_t)get_res);
    return std::nullopt;
  }

  if (array_type != (CIM_STRING | CIM_FLAG_ARRAY)) {
    LOG(L"Bad cim array type of {}.  Type={}", name, array_type);
    return std::nullopt;
  }

  if (V_VT(&array) != (VT_BSTR | VT_ARRAY)) {
    LOG(L"Bad variant type of {}.  Type={}", name, array_type);
    return std::nullopt;
  }

  SAFEARRAY* arr = V_ARRAY(&array);

  if (auto dim = SafeArrayGetDim(arr); dim != 1) {
    LOG(L"Bad array dim in {}.  Expected 1.  Dim = {}", name, dim);
    return std::nullopt;
  }

  LONG lbound, ubound;
  auto lbound_res = SafeArrayGetLBound(arr, 1, &lbound);
  if (FAILED(lbound_res)) {
    LOG(L"Could not get lower bound of SAFEARRAY {}.  Err={} ({})", name,
        WMI::ErrorString(lbound_res), lbound_res);
    return std::nullopt;
  }
  auto ubound_res = SafeArrayGetUBound(arr, 1, &ubound);
  if (FAILED(ubound_res)) {
    LOG(L"Could not get upper bound of SAFEARRAY {}.  Err={} ({})", name,
        WMI::ErrorString(ubound_res), ubound_res);
    return std::nullopt;
  }


  std::vector<std::wstring> vec;
  vec.resize(ubound - lbound + 1);
  for (LONG i = lbound; i <= ubound; ++i) {
    BSTR entry;
    auto hres = SafeArrayGetElement(arr, &i, &entry);
    if (FAILED(hres)) {
      LOG(L"Could not get {}th element of array {}.  Err={} ({})", i, name,
          WMI::ErrorString(hres), hres);
      return std::nullopt;
    }
    vec[i] = {entry};
  }

  VariantClear(&array);

  return vec;
}

WMI::Result<std::unordered_map<std::wstring, std::wstring>> GetRCTsByDiskId(
    const WMI::ClassObject& ref_point)
{
  auto rcti = GetStringArray(ref_point, L"ResilientChangeTrackingIdentifiers");
  if (!rcti) { return std::nullopt; }
  auto vdi = GetStringArray(ref_point, L"VirtualDiskIdentifiers");
  if (!vdi) { return std::nullopt; }

  if (rcti->size() != vdi->size()) {
    LOG(L"Not every disk has rct information: rcti {} != vdi {}", rcti->size(),
        vdi->size());
    return std::nullopt;
  }

  std::unordered_map<std::wstring, std::wstring> result;
  for (std::size_t i = 0; i < rcti->size(); ++i) {
    LOG(L"Found RCT {} -> {}", vdi->at(i), rcti->at(i));
    result.try_emplace(std::move((*vdi)[i]), std::move((*rcti)[i]));
  }
  return result;
}

WMI::Result<WMI::ClassObject> FullBackupWin32(
    const WMI& service,
    std::wstring_view vm_name,
    std::wstring_view backup_directory)
{
  auto vsms_clz = VirtualSystemManagementService::Class::load(service);
  if (!vsms_clz) { return std::nullopt; }
  auto vsss_clz = VirtualSystemSnapshotService::Class::load(service);
  if (!vsss_clz) { return std::nullopt; }
  auto vsesd = VirtualSystemExportSettingData::Class::load(service);
  if (!vsesd) { return std::nullopt; }
  auto vsssd_clz = VirtualSystemSnapshotSettingData::Class::load(service);
  if (!vsssd_clz) { return std::nullopt; }

  auto vsms = vsms_clz->Get();
  if (!vsms) { return std::nullopt; }
  auto vsss = vsss_clz->Get();
  if (!vsss) { return std::nullopt; }

  auto vm = service.GetVMByName(vm_name);
  if (!vm) { return std::nullopt; }

  WMI::SystemString directory{backup_directory};


  VirtualSystemSnapshotSettingData ssettings = {
      .consistency_level
      = VirtualSystemSnapshotSettingData::ConsistencyLevel::CrashConsistent,
      .ignore_non_snapshottable_disks = true,
  };

  auto snapshot_settings = vsssd_clz->GetText(ssettings);
  if (!snapshot_settings) { return std::nullopt; }

  LOG(L"Snapshot Settings = {}", snapshot_settings->as_view());

  auto snapshot = vsss->CreateSnapshot(
      vm.value(), snapshot_settings.value(),
      VirtualSystemSnapshotService::SnapshotType::RecoverySnapshot);
  if (!snapshot) { return std::nullopt; }


  {
    WMI::SystemString new_snapshot_name{
        std::wstring_view{L"Bareos Snapshot - Full"}};

    VARIANT Name;
    V_VT(&Name) = VT_BSTR;
    V_BSTR(&Name) = new_snapshot_name.get();
    snapshot->system->Put(L"ElementName", 0, &Name, 0);
    auto xml = WMI::ObjectAsXML(snapshot.value());
    if (!xml) { return std::nullopt; }
    auto rename_res = vsms->ModifySystemSettings(xml.value());
    if (!rename_res) { return std::nullopt; }
  }

  LOG(L"Snapshot = {}", WMI::ObjectAsString(snapshot->system).as_view());

  auto snapshot_path = snapshot->Path();
  if (!snapshot_path) { return std::nullopt; }

  VirtualSystemExportSettingData settings = {
      .snapshot_virtual_system_path = std::wstring{snapshot_path->as_view()},
      .backup_intent = VirtualSystemExportSettingData::BackupIntent::Merge,
      .copy_snapshot_configuration = VirtualSystemExportSettingData::
          CopySnapshotConfiguration::ExportOneSnapshotForBackup,
      .capture_live_state
      = VirtualSystemExportSettingData::CaptureLiveState::CrashConsistent,
      .copy_vm_runtime_information = false,
      .copy_vm_storage = false,
      .create_vm_export_subdirectory = true,
  };

  WMI::Result options = vsesd->GetText(settings);
  if (!options) { return std::nullopt; }

  auto res = vsms->ExportSystemDefinition(vm.value(), directory, *options);
  if (!res) { return std::nullopt; }

  auto snapshot_id = GetSnapshotId(snapshot.value());
  if (!snapshot_id) { return std::nullopt; }

  LOG(L"SnapshotId = {}", snapshot_id->as_view());

  {
    auto vhd_settings = service.GetRelatedOfClass(
        snapshot_path->as_view(), L"Msvm_StorageAllocationSettingData",
        L"Msvm_VirtualSystemSettingDataComponent");
    if (!vhd_settings) { return std::nullopt; }

    // we only want to look at
    // ResourceSubType = "Microsoft:Hyper-V:Virtual Hard Disk";
    // ResourceType = 31;
    // Resources
    // std::remove_if(std::begin(vhd_settings),
    //                std::end(vhd_settings),
    //                []() {

    //                });

    for (auto& vhd_setting : vhd_settings.value()) {
      LOG(L"vhd setting = {}",
          WMI::ObjectAsString(vhd_setting.system).as_view());

      auto hdd_path = HardDiskPath(vhd_setting);
      if (!hdd_path) { return std::nullopt; }

      if (!BackupDisk(hdd_path.value(), nullptr, backup_directory)) {
        return std::nullopt;
      }
    }
  }

  auto refpoint = vsss->ConvertToReferencePoint(std::move(snapshot).value());
  return refpoint;
}

DWORD
SampleGetVirtualDiskInformation(_In_ LPCWSTR VirtualDiskPath)
{
  OPEN_VIRTUAL_DISK_PARAMETERS openParameters;
  VIRTUAL_STORAGE_TYPE storageType;
  PGET_VIRTUAL_DISK_INFO diskInfo;
  ULONG diskInfoSize;
  DWORD opStatus;

  HANDLE vhdHandle;

  UINT32 driveType;
  UINT32 driveFormat;

  GUID identifier;

  ULONGLONG physicalSize;
  ULONGLONG virtualSize;
  ULONGLONG minInternalSize;
  ULONG blockSize;
  ULONG sectorSize;
  ULONG physicalSectorSize;
  LPCWSTR parentPath;
  size_t parentPathSize;
  size_t parentPathSizeRemaining;
  HRESULT stringLengthResult;
  GUID parentIdentifier;
  ULONGLONG fragmentationPercentage;

  vhdHandle = INVALID_HANDLE_VALUE;
  diskInfo = NULL;
  diskInfoSize = sizeof(GET_VIRTUAL_DISK_INFO);

  diskInfo = (PGET_VIRTUAL_DISK_INFO)malloc(diskInfoSize);
  if (diskInfo == NULL) {
    opStatus = ERROR_NOT_ENOUGH_MEMORY;
    goto Cleanup;
  }

  //
  // Specify UNKNOWN for both device and vendor so the system will use the
  // file extension to determine the correct VHD format.
  //

  storageType.DeviceId = VIRTUAL_STORAGE_TYPE_DEVICE_UNKNOWN;
  storageType.VendorId = VIRTUAL_STORAGE_TYPE_VENDOR_UNKNOWN;

  //
  // Open the VHD for query access.
  //
  // A "GetInfoOnly" handle is a handle that can only be used to query
  // properties or metadata.
  //
  // VIRTUAL_DISK_ACCESS_NONE is the only acceptable access mask for V2 handle
  // opens. OPEN_VIRTUAL_DISK_FLAG_NO_PARENTS indicates the parent chain should
  // not be opened.
  //

  memset(&openParameters, 0, sizeof(openParameters));
  openParameters.Version = OPEN_VIRTUAL_DISK_VERSION_2;
  openParameters.Version2.GetInfoOnly = TRUE;

  opStatus = OpenVirtualDisk(
      &storageType, VirtualDiskPath, VIRTUAL_DISK_ACCESS_NONE,
      OPEN_VIRTUAL_DISK_FLAG_NO_PARENTS, &openParameters, &vhdHandle);

  if (opStatus != ERROR_SUCCESS) { goto Cleanup; }

  //
  // Get the VHD/VHDX type.
  //

  diskInfo->Version = GET_VIRTUAL_DISK_INFO_PROVIDER_SUBTYPE;

  opStatus
      = GetVirtualDiskInformation(vhdHandle, &diskInfoSize, diskInfo, NULL);

  if (opStatus != ERROR_SUCCESS) { goto Cleanup; }

  driveType = diskInfo->ProviderSubtype;

  wprintf(L"driveType = %d", driveType);

  if (driveType == 2) {
    wprintf(L" (fixed)\n");
  } else if (driveType == 3) {
    wprintf(L" (dynamic)\n");
  } else if (driveType == 4) {
    wprintf(L" (differencing)\n");
  } else {
    wprintf(L"\n");
  }

  //
  // Get the VHD/VHDX format.
  //

  diskInfo->Version = GET_VIRTUAL_DISK_INFO_VIRTUAL_STORAGE_TYPE;

  opStatus
      = GetVirtualDiskInformation(vhdHandle, &diskInfoSize, diskInfo, NULL);

  if (opStatus != ERROR_SUCCESS) { goto Cleanup; }

  driveFormat = diskInfo->VirtualStorageType.DeviceId;

  wprintf(L"driveFormat = %d", driveFormat);

  if (driveFormat == VIRTUAL_STORAGE_TYPE_DEVICE_VHD) {
    wprintf(L" (vhd)\n");
  } else if (driveFormat == VIRTUAL_STORAGE_TYPE_DEVICE_VHDX) {
    wprintf(L" (vhdx)\n");
  } else {
    wprintf(L"\n");
  }

  //
  // Get the VHD/VHDX virtual disk size.
  //

  diskInfo->Version = GET_VIRTUAL_DISK_INFO_SIZE;

  opStatus
      = GetVirtualDiskInformation(vhdHandle, &diskInfoSize, diskInfo, NULL);

  if (opStatus != ERROR_SUCCESS) { goto Cleanup; }

  physicalSize = diskInfo->Size.PhysicalSize;
  virtualSize = diskInfo->Size.VirtualSize;
  sectorSize = diskInfo->Size.SectorSize;
  blockSize = diskInfo->Size.BlockSize;

  wprintf(L"physicalSize = %I64u\n", physicalSize);
  wprintf(L"virtualSize = %I64u\n", virtualSize);
  wprintf(L"sectorSize = %u\n", sectorSize);
  wprintf(L"blockSize = %u\n", blockSize);

  //
  // Get the VHD physical sector size.
  //

  diskInfo->Version = GET_VIRTUAL_DISK_INFO_VHD_PHYSICAL_SECTOR_SIZE;

  opStatus
      = GetVirtualDiskInformation(vhdHandle, &diskInfoSize, diskInfo, NULL);

  if (opStatus != ERROR_SUCCESS) { goto Cleanup; }

  physicalSectorSize = diskInfo->VhdPhysicalSectorSize;

  wprintf(L"physicalSectorSize = %u\n", physicalSectorSize);

  //
  // Get the virtual disk ID.
  //

  diskInfo->Version = GET_VIRTUAL_DISK_INFO_IDENTIFIER;

  opStatus
      = GetVirtualDiskInformation(vhdHandle, &diskInfoSize, diskInfo, NULL);

  if (opStatus != ERROR_SUCCESS) { goto Cleanup; }

  identifier = diskInfo->Identifier;

  wprintf(L"identifier = {%08X-%04X-%04X-%02X%02X%02X%02X%02X%02X%02X%02X}\n",
          identifier.Data1, identifier.Data2, identifier.Data3,
          identifier.Data4[0], identifier.Data4[1], identifier.Data4[2],
          identifier.Data4[3], identifier.Data4[4], identifier.Data4[5],
          identifier.Data4[6], identifier.Data4[7]);

  //
  // Get the VHD parent path.
  //

  if (driveType == 0x4) {
    diskInfo->Version = GET_VIRTUAL_DISK_INFO_PARENT_LOCATION;

    opStatus
        = GetVirtualDiskInformation(vhdHandle, &diskInfoSize, diskInfo, NULL);

    if (opStatus != ERROR_SUCCESS) {
      if (opStatus != ERROR_INSUFFICIENT_BUFFER) { goto Cleanup; }

      free(diskInfo);

      diskInfo = (PGET_VIRTUAL_DISK_INFO)malloc(diskInfoSize);
      if (diskInfo == NULL) {
        opStatus = ERROR_NOT_ENOUGH_MEMORY;
        goto Cleanup;
      }

      diskInfo->Version = GET_VIRTUAL_DISK_INFO_PARENT_LOCATION;

      opStatus
          = GetVirtualDiskInformation(vhdHandle, &diskInfoSize, diskInfo, NULL);

      if (opStatus != ERROR_SUCCESS) { goto Cleanup; }
    }

    parentPath = diskInfo->ParentLocation.ParentLocationBuffer;
    parentPathSizeRemaining
        = diskInfoSize
          - FIELD_OFFSET(GET_VIRTUAL_DISK_INFO,
                         ParentLocation.ParentLocationBuffer);

    if (diskInfo->ParentLocation.ParentResolved) {
      wprintf(L"parentPath = '%s'\n", parentPath);
    } else {
      //
      // If the parent is not resolved, the buffer is a MULTI_SZ
      //

      wprintf(L"parentPath:\n");

      while ((parentPathSizeRemaining >= sizeof(parentPath[0]))
             && (*parentPath != 0)) {
        stringLengthResult = StringCbLengthW(
            parentPath, parentPathSizeRemaining, &parentPathSize);

        if (FAILED(stringLengthResult)) { goto Cleanup; }

        wprintf(L"    '%s'\n", parentPath);

        parentPathSize += sizeof(parentPath[0]);
        parentPath = parentPath + (parentPathSize / sizeof(parentPath[0]));
        parentPathSizeRemaining -= parentPathSize;
      }
    }

    //
    // Get parent ID.
    //

    diskInfo->Version = GET_VIRTUAL_DISK_INFO_PARENT_IDENTIFIER;

    opStatus
        = GetVirtualDiskInformation(vhdHandle, &diskInfoSize, diskInfo, NULL);

    if (opStatus != ERROR_SUCCESS) { goto Cleanup; }

    parentIdentifier = diskInfo->ParentIdentifier;

    wprintf(
        L"parentIdentifier = "
        L"{%08X-%04X-%04X-%02X%02X%02X%02X%02X%02X%02X%02X}\n",
        parentIdentifier.Data1, parentIdentifier.Data2, parentIdentifier.Data3,
        parentIdentifier.Data4[0], parentIdentifier.Data4[1],
        parentIdentifier.Data4[2], parentIdentifier.Data4[3],
        parentIdentifier.Data4[4], parentIdentifier.Data4[5],
        parentIdentifier.Data4[6], parentIdentifier.Data4[7]);
  }

  //
  // Get the VHD minimum internal size.
  //

  diskInfo->Version = GET_VIRTUAL_DISK_INFO_SMALLEST_SAFE_VIRTUAL_SIZE;

  opStatus
      = GetVirtualDiskInformation(vhdHandle, &diskInfoSize, diskInfo, NULL);

  if (opStatus == ERROR_SUCCESS) {
    minInternalSize = diskInfo->SmallestSafeVirtualSize;

    wprintf(L"minInternalSize = %I64u\n", minInternalSize);
  } else {
    opStatus = ERROR_SUCCESS;
  }

  //
  // Get the VHD fragmentation percentage.
  //

  diskInfo->Version = GET_VIRTUAL_DISK_INFO_FRAGMENTATION;

  opStatus
      = GetVirtualDiskInformation(vhdHandle, &diskInfoSize, diskInfo, NULL);

  if (opStatus == ERROR_SUCCESS) {
    fragmentationPercentage = diskInfo->FragmentationPercentage;

    wprintf(L"fragmentationPercentage = %I64u\n", fragmentationPercentage);
  } else {
    opStatus = ERROR_SUCCESS;
  }

Cleanup:

  if (opStatus == ERROR_SUCCESS) {
    wprintf(L"success\n");
  } else {
    wprintf(L"error = %u\n", opStatus);
  }

  if (vhdHandle != INVALID_HANDLE_VALUE) { CloseHandle(vhdHandle); }

  if (diskInfo != NULL) { free(diskInfo); }

  return opStatus;
}


WMI::Result<WMI::ClassObject> IncrementalBackup(
    const WMI& service,
    std::wstring_view vm_name,
    std::wstring_view backup_directory,
    std::wstring_view ref_path,
    bool use_win32_api)
{
  auto vsms_clz = VirtualSystemManagementService::Class::load(service);
  if (!vsms_clz) { return std::nullopt; }
  auto vsss_clz = VirtualSystemSnapshotService::Class::load(service);
  if (!vsss_clz) { return std::nullopt; }
  auto vsesd = VirtualSystemExportSettingData::Class::load(service);
  if (!vsesd) { return std::nullopt; }
  auto vsssd_clz = VirtualSystemSnapshotSettingData::Class::load(service);
  if (!vsssd_clz) { return std::nullopt; }

  auto vsms = vsms_clz->Get();
  if (!vsms) { return std::nullopt; }
  auto vsss = vsss_clz->Get();
  if (!vsss) { return std::nullopt; }

  auto vm = service.GetVMByName(vm_name);
  if (!vm) { return std::nullopt; }

  WMI::SystemString directory{backup_directory};

  VirtualSystemSnapshotSettingData ssettings = {
      .consistency_level
      = VirtualSystemSnapshotSettingData::ConsistencyLevel::CrashConsistent,
      .ignore_non_snapshottable_disks = true,
  };

  auto snapshot_settings = vsssd_clz->GetText(ssettings);

  if (!snapshot_settings) { return std::nullopt; }

  LOG(L"Snapshot Settings = {}", snapshot_settings->as_view());

  auto snapshot = vsss->CreateSnapshot(
      vm.value(), snapshot_settings.value(),
      VirtualSystemSnapshotService::SnapshotType::RecoverySnapshot);
  if (!snapshot) { return std::nullopt; }


  {
    WMI::SystemString new_snapshot_name{
        std::wstring_view{L"Bareos Snapshot - Incremental"}};

    VARIANT Name;
    V_VT(&Name) = VT_BSTR;
    V_BSTR(&Name) = new_snapshot_name.get();
    snapshot->system->Put(L"ElementName", 0, &Name, 0);
    auto xml = WMI::ObjectAsXML(snapshot.value());
    if (!xml) { return std::nullopt; }
    auto rename_res = vsms->ModifySystemSettings(xml.value());
    if (!rename_res) { return std::nullopt; }
  }

  LOG(L"Snapshot = {}", WMI::ObjectAsString(snapshot->system).as_view());

  auto snapshot_path = snapshot->Path();
  if (!snapshot_path) { return std::nullopt; }

  // TODO: check if ref_path is actually a referenc point
  VirtualSystemExportSettingData settings = {
      .snapshot_virtual_system_path = std::wstring{snapshot_path->as_view()},
      .differential_backup_base_path = std::wstring{ref_path},
      .backup_intent = VirtualSystemExportSettingData::BackupIntent::Merge,
      .copy_snapshot_configuration = VirtualSystemExportSettingData::
          CopySnapshotConfiguration::ExportOneSnapshotForBackup,
      .capture_live_state
      = VirtualSystemExportSettingData::CaptureLiveState::CrashConsistent,
      .copy_vm_runtime_information = false,
      .copy_vm_storage = !use_win32_api,
      .create_vm_export_subdirectory = true,
  };

  WMI::Result options = vsesd->GetText(settings);
  if (!options) { return std::nullopt; }

  auto res = vsms->ExportSystemDefinition(vm.value(), directory, *options);
  if (!res) { return std::nullopt; }

  if (use_win32_api) {
    auto refpoint = GetRefPoint(service, ref_path);
    if (!refpoint) { return std::nullopt; }
    LOG(L"RefPoint = {}", WMI::ObjectAsString(refpoint->system).as_view());

    auto rct_ids = GetRCTsByDiskId(refpoint.value());
    if (!rct_ids) { return std::nullopt; }

    auto vhd_settings = service.GetRelatedOfClass(
        snapshot_path->as_view(), L"Msvm_StorageAllocationSettingData",
        L"Msvm_VirtualSystemSettingDataComponent");
    if (!vhd_settings) { return std::nullopt; }

    // we only want to look at
    // ResourceSubType = "Microsoft:Hyper-V:Virtual Hard Disk";
    // ResourceType = 31;
    // Resources
    // std::remove_if(std::begin(vhd_settings),
    //                std::end(vhd_settings),
    //                []() {

    //                });

    for (auto& vhd_setting : vhd_settings.value()) {
      LOG(L"vhd setting = {}",
          WMI::ObjectAsString(vhd_setting.system).as_view());

      auto hdd_path = HardDiskPath(vhd_setting);
      if (!hdd_path) { return std::nullopt; }

      // TODO: we somehow have to grep the correct rct_id for the
      // wanted hdd.
      const wchar_t* rct_id = [&] {
        if (rct_ids->size() > 0) { return rct_ids->begin()->second.c_str(); }
        return (const wchar_t*)nullptr;
      }();

      std::wstring stdpath{hdd_path->as_view()};
      SampleGetVirtualDiskInformation(stdpath.c_str());

      if (!BackupDisk(hdd_path.value(), rct_id, backup_directory)) {
        return std::nullopt;
      }
    }
  }

  auto refpoint = vsss->ConvertToReferencePoint(std::move(snapshot).value());
  return refpoint;
}

template <typename T> std::span<char> as_span(T& val)
{
  return std::span<char>{reinterpret_cast<char*>(&val), sizeof(T)};
}

std::optional<std::size_t> ReadOverlapped(HANDLE hnd,
                                          ULONG64 Offset,
                                          std::span<char> Buffer)
{
  std::size_t TotalBytesRead = 0;
  while (TotalBytesRead < Buffer.size()) {
    auto CurrentOffset = Offset + TotalBytesRead;

    OVERLAPPED Overlapped = {};
    Overlapped.Offset = DWORD{CurrentOffset & 0xFFFFFFFF};
    Overlapped.OffsetHigh = DWORD{(CurrentOffset >> 32) & 0xFFFFFFFF};

    DWORD BytesRead = 0;
    char* BufferStart = Buffer.data() + TotalBytesRead;
    std::size_t MaxBytesToRead = Buffer.size() - TotalBytesRead;
    if (MaxBytesToRead >= std::numeric_limits<DWORD>::max()) {
      MaxBytesToRead = std::numeric_limits<DWORD>::max() - 1;
    }
    LOG(L"reading {} bytes from {} handle at offset {}", MaxBytesToRead, hnd,
        CurrentOffset);

    DWORD Error = ERROR_SUCCESS;
    if (!ReadFile(hnd, BufferStart, static_cast<DWORD>(MaxBytesToRead),
                  &BytesRead, &Overlapped)) {
      Error = GetLastError();

      if (Error == ERROR_IO_PENDING) {
        if (GetOverlappedResult(hnd, &Overlapped, &BytesRead, TRUE)) {
          Error = ERROR_SUCCESS;
        } else {
          Error = GetLastError();
        }
      }
    }

    switch (Error) {
      case ERROR_SUCCESS: {
        // intentionally left blank
      } break;
      case ERROR_HANDLE_EOF: {
        // intentionally left blank
      } break;
      default: {
        LOG(L"could not read {} bytes from {} handle at offset {}.  Err={} "
            L"({})",
            MaxBytesToRead, hnd, CurrentOffset, WMI::ErrorString(Error), Error);
        return std::nullopt;
      } break;
    }

    if (BytesRead == 0) {
      // 0 on read => EOF
      LOG(L"could not read {} bytes from {} handle at offset {}: read returned "
          L"0",
          MaxBytesToRead, hnd, CurrentOffset);
      break;
    }

    TotalBytesRead += BytesRead;
  }

  return TotalBytesRead;
}

std::optional<std::size_t> WriteOverlapped(HANDLE hnd,
                                           ULONG64 Offset,
                                           std::span<const char> Buffer)
{
  std::size_t TotalBytesWritten = 0;
  while (TotalBytesWritten < Buffer.size()) {
    auto CurrentOffset = Offset + TotalBytesWritten;

    OVERLAPPED Overlapped = {};
    Overlapped.Offset = DWORD{CurrentOffset & 0xFFFFFFFF};
    Overlapped.OffsetHigh = DWORD{(CurrentOffset >> 32) & 0xFFFFFFFF};

    DWORD BytesWritten = 0;
    const char* BufferStart = Buffer.data() + TotalBytesWritten;
    std::size_t MaxBytesToWrite = Buffer.size() - TotalBytesWritten;
    if (MaxBytesToWrite >= std::numeric_limits<DWORD>::max()) {
      MaxBytesToWrite = std::numeric_limits<DWORD>::max() - 1;
    }
    LOG(L"writing {} bytes to {} handle at offset {}", MaxBytesToWrite, hnd,
        CurrentOffset);
    DWORD Error = ERROR_SUCCESS;
    if (!WriteFile(hnd, BufferStart, static_cast<DWORD>(MaxBytesToWrite),
                   &BytesWritten, &Overlapped)) {
      Error = GetLastError();

      if (Error == ERROR_IO_PENDING) {
        if (GetOverlappedResult(hnd, &Overlapped, &BytesWritten, TRUE)) {
          Error = ERROR_SUCCESS;
        } else {
          Error = GetLastError();
        }
      }
    }

    switch (Error) {
      case ERROR_SUCCESS: {
        // intentionally left blank
      } break;
      default: {
        LOG(L"could not write {} bytes to {} handle at offset {}.  Err={} ({})",
            MaxBytesToWrite, hnd, CurrentOffset, WMI::ErrorString(Error),
            Error);
        return std::nullopt;
      } break;
    }

    if (BytesWritten == 0) {
      LOG(L"could not write {} bytes to {} handle at offset {}: write returned "
          L"0",
          MaxBytesToWrite, hnd, CurrentOffset);
      return std::nullopt;
    }

    TotalBytesWritten += BytesWritten;
  }

  return TotalBytesWritten;
}

HANDLE OpenAndAttachR(const std::wstring& vhd_path)
{
  HANDLE disk_handle = INVALID_HANDLE_VALUE;
  {
    VIRTUAL_STORAGE_TYPE vst = {
        .DeviceId = VIRTUAL_STORAGE_TYPE_DEVICE_UNKNOWN,
        .VendorId = VIRTUAL_STORAGE_TYPE_VENDOR_UNKNOWN,
    };

    OPEN_VIRTUAL_DISK_PARAMETERS params = {};
    params.Version = OPEN_VIRTUAL_DISK_VERSION_2;
    params.Version2.ReadOnly = TRUE;
    auto res
        = OpenVirtualDisk(&vst, vhd_path.c_str(), VIRTUAL_DISK_ACCESS_NONE,
                          OPEN_VIRTUAL_DISK_FLAG_NONE, &params, &disk_handle);
    if (res != ERROR_SUCCESS) {
      LOG(L"OpenVirtualDisk({}) failed.  Err={} ({:X})", vhd_path,
          WMI::ErrorString(res), (uint64_t)res);
      return INVALID_HANDLE_VALUE;
      ;
    }


    ATTACH_VIRTUAL_DISK_PARAMETERS attach_params
        = {.Version = ATTACH_VIRTUAL_DISK_VERSION_1, .Version1 = {}};
    auto attach_res = AttachVirtualDisk(disk_handle, NULL,
                                        ATTACH_VIRTUAL_DISK_FLAG_NO_LOCAL_HOST,
                                        0, &attach_params, 0);
    if (FAILED(attach_res)) {
      LOG(L"AttachVirtualDisk() failed.  Err={} ({:X})",
          WMI::ErrorString(attach_res), (uint32_t)attach_res);
      CloseHandle(disk_handle);
      return INVALID_HANDLE_VALUE;
      ;
    }
  }

  return disk_handle;
}

std::optional<std::size_t> VirtualSize(HANDLE disk_handle)
{
  GET_VIRTUAL_DISK_INFO disk_info = {
      .Version = GET_VIRTUAL_DISK_INFO_SIZE,
  };
  ULONG VirtualDiskInfoSize = sizeof(disk_info);
  ULONG SizeOut = 0;
  auto disk_res = GetVirtualDiskInformation(disk_handle, &VirtualDiskInfoSize,
                                            &disk_info, &SizeOut);
  if (disk_res != ERROR_SUCCESS) {
    LOG(L"GetVirtualDiskInfo({}) failed.  Err={} ({:X})", disk_handle,
        WMI::ErrorString(disk_res), (uint64_t)disk_res);
    return false;
  }

  LOG(L"Size In = {} ({}), Size Out = {}", sizeof(disk_info),
      VirtualDiskInfoSize, SizeOut);

  LOG(L"Size = {{ VirtualSize = {}, PhysicalSize = {}, BlockSize = {}, "
      L"SectorSize = {} }}",
      disk_info.Size.VirtualSize, disk_info.Size.PhysicalSize,
      disk_info.Size.BlockSize, disk_info.Size.SectorSize);

  return disk_info.Size.VirtualSize;
}

bool ComputeDiff(const std::wstring& left_path, const std::wstring& right_path)
{
  HANDLE left = OpenAndAttachR(left_path);
  if (left == INVALID_HANDLE_VALUE) { return false; }
  HANDLE right = OpenAndAttachR(right_path);
  if (right == INVALID_HANDLE_VALUE) {
    if (auto detach_res
        = DetachVirtualDisk(left, DETACH_VIRTUAL_DISK_FLAG_NONE, 0);
        FAILED(detach_res)) {
      LOG(L"detach failed.  Err={} ({})", WMI::ErrorString(detach_res),
          detach_res);
    }
    CloseHandle(left);
    return false;
  }

  bool retval = false;

  std::optional left_size = VirtualSize(left);
  std::optional right_size = VirtualSize(right);
  if (!left_size) { goto cleanup; }
  if (!right_size) { goto cleanup; }

  {
    std::size_t buffer_size = 1024 * 1024;
    std::vector<char> left_buffer;
    left_buffer.resize(buffer_size);
    std::vector<char> right_buffer;
    right_buffer.resize(buffer_size);

    auto read_size = std::min(left_size.value(), right_size.value());
    std::size_t CurrentOffset = 0;

    while (CurrentOffset < read_size) {
      auto current_read_size = std::min(read_size - CurrentOffset, buffer_size);
      std::span left_span{left_buffer.data(), current_read_size};
      std::optional left_bytes = ReadOverlapped(left, CurrentOffset, left_span);
      std::span right_span{right_buffer.data(), current_read_size};
      std::optional right_bytes
          = ReadOverlapped(right, CurrentOffset, right_span);

      if (!left_bytes || left_bytes.value() < current_read_size) {
        goto cleanup;
      }
      if (!right_bytes || right_bytes.value() < current_read_size) {
        goto cleanup;
      }

      if (memcmp(left_span.data(), right_span.data(), current_read_size) != 0) {
        std::wcout << std::format(L"Diff at {}-{}", CurrentOffset,
                                  CurrentOffset + current_read_size)
                   << L"\n";
      }

      CurrentOffset += current_read_size;
    }

    retval = true;
  }
cleanup:
  if (auto detach_res
      = DetachVirtualDisk(left, DETACH_VIRTUAL_DISK_FLAG_NONE, 0);
      FAILED(detach_res)) {
    LOG(L"detach failed.  Err={} ({})", WMI::ErrorString(detach_res),
        detach_res);
  }
  CloseHandle(left);
  if (auto detach_res
      = DetachVirtualDisk(right, DETACH_VIRTUAL_DISK_FLAG_NONE, 0);
      FAILED(detach_res)) {
    LOG(L"detach failed.  Err={} ({})", WMI::ErrorString(detach_res),
        detach_res);
  }
  CloseHandle(right);

  return retval;
}

bool ApplyIncrementalToVhd(const std::wstring& base_vhd_path,
                           const std::wstring& inc_path)
{
  HANDLE disk_handle = INVALID_HANDLE_VALUE;
  {
    VIRTUAL_STORAGE_TYPE vst = {
        .DeviceId = VIRTUAL_STORAGE_TYPE_DEVICE_UNKNOWN,
        .VendorId = VIRTUAL_STORAGE_TYPE_VENDOR_UNKNOWN,
    };
    auto res = OpenVirtualDisk(
        &vst, base_vhd_path.c_str(),
        VIRTUAL_DISK_ACCESS_WRITABLE | VIRTUAL_DISK_ACCESS_ATTACH_RW,
        OPEN_VIRTUAL_DISK_FLAG_NONE, NULL, &disk_handle);
    if (res != ERROR_SUCCESS) {
      LOG(L"OpenVirtualDisk({}) failed.  Err={} ({:X})", base_vhd_path,
          WMI::ErrorString(res), (uint64_t)res);
      return false;
    }


    ATTACH_VIRTUAL_DISK_PARAMETERS attach_params
        = {.Version = ATTACH_VIRTUAL_DISK_VERSION_1, .Version1 = {}};
    auto attach_res = AttachVirtualDisk(disk_handle, NULL,
                                        ATTACH_VIRTUAL_DISK_FLAG_NO_LOCAL_HOST,
                                        0, &attach_params, 0);
    if (FAILED(attach_res)) {
      LOG(L"AttachVirtualDisk() failed.  Err={} ({:X})",
          WMI::ErrorString(attach_res), (uint32_t)attach_res);
      return false;
    }
  }

  HANDLE hnd = CreateFileW(inc_path.c_str(), GENERIC_READ, FILE_SHARE_READ,
                           NULL, OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, NULL);

  if (hnd == INVALID_HANDLE_VALUE) {
    auto hres = GetLastError();
    LOG(L"could not open {}.  Err={} ({})", inc_path, WMI::ErrorString(hres),
        hres);
    return false;
  }

  LOG(L"Handle = {}", hnd);

  struct read_result {
    OVERLAPPED Overlapped{};
    DWORD bytes_transferred{};
    DWORD error_code{};

    read_result(ULONG64 Offset)
    {
      Overlapped.Offset = DWORD{Offset & 0xFFFFFFFF};
      Overlapped.OffsetHigh = DWORD{(Offset >> 32) & 0xFFFFFFFF};
    }

    OVERLAPPED* overlapped() { return &Overlapped; }

    static read_result* get(OVERLAPPED* ptr)
    {
      return reinterpret_cast<read_result*>(ptr);
    }

    static void Callback(DWORD ErrorCode, DWORD BytesRead, OVERLAPPED* In)
    {
      auto* Res = read_result::get(In);
      Res->bytes_transferred = BytesRead;
      Res->error_code = ErrorCode;

      LOG(L"In = {}, Error = {}, Bytes = {}", (const void*)In, ErrorCode,
          BytesRead);
    }
  };

  std::vector<char> buffer;
  buffer.resize(1024 * 1024);

  ULONG64 CurrentOffset = 0;
  std::size_t RangeCount = 0;
  for (;;) {
    range_header Header;
    std::optional header_bytes
        = ReadOverlapped(hnd, CurrentOffset, as_span(Header));
    if (!header_bytes) { return false; }
    if (header_bytes == 0) {
      // reached eof
      break;
    }

    if (header_bytes < sizeof(Header)) {
      LOG(L"expected header but only {} bytes are available",
          header_bytes.value());
      return false;
    }

    LOG(L"Offset {} => Header {{ Offset = {}, Size = {} }}", CurrentOffset,
        Header.Offset, Header.Size);

    CurrentOffset += header_bytes.value();
    RangeCount += 1;

    buffer.resize(Header.Size);
    std::optional body_bytes = ReadOverlapped(hnd, CurrentOffset, buffer);
    if (!body_bytes || body_bytes < Header.Size) {
      LOG(L"could not read payload of {} bytes", Header.Size);
      return false;
    }

    std::optional written_bytes
        = WriteOverlapped(disk_handle, Header.Offset, buffer);
    if (!written_bytes || written_bytes < Header.Size) {
      LOG(L"could not read payload of {} bytes", Header.Size);
      return false;
    }


    // skip the data for now
    CurrentOffset += Header.Size;
  }

  LOG(L"Found {} ranges", RangeCount);

  CloseHandle(hnd);

  if (auto detach_res
      = DetachVirtualDisk(disk_handle, DETACH_VIRTUAL_DISK_FLAG_NONE, 0);
      FAILED(detach_res)) {
    LOG(L"detach failed.  Err={} ({})", WMI::ErrorString(detach_res),
        detach_res);
  }

  CloseHandle(disk_handle);
  return true;
}

bool RestoreBackup(const WMI& service,
                   std::optional<std::wstring_view> new_vm_name,
                   std::wstring_view definition_file,
                   std::wstring_view hard_disk_path)
{
  auto vsms_clz = VirtualSystemManagementService::Class::load(service);
  if (!vsms_clz) { return false; }

  auto vsms = vsms_clz->Get();
  if (!vsms) { return false; }

  WMI::SystemString SystemDefinitionFile{definition_file};
  WMI::SystemString SnapshotPath{std::wstring_view{L""}};
  bool GenerateNewSystemIdentifier = true;

  auto planned_system = vsms->ImportSystemDefinition(
      SystemDefinitionFile, SnapshotPath, GenerateNewSystemIdentifier);
  if (!planned_system) { return false; }

  // We have now imported the system with the correct configuration.
  // That configuration still points to the wrong Hard Disks.  We need to
  // find every associated hard disk and change their paths to the
  // newly restored ones.
  auto planned_system_path = planned_system->Path();
  if (!planned_system_path) { return false; }
  auto related = service.GetRelatedOfClass(planned_system_path->as_view(),
                                           L"Msvm_VirtualSystemSettingData",
                                           L"Msvm_SettingsDefineState");
  if (!related) { return false; }

  WMI::SystemString HardDiskPath{hard_disk_path};
  LOG(L"Found {} settings", related->size());
  for (auto& setting : related.value()) {
    LOG(L"setting = {}", WMI::ObjectAsString(setting.system).as_view());

    auto setting_path = setting.Path();
    if (!setting_path) {
      LOG(L"Could not get path to setting");
      return false;
    }
    auto vhd_settings = service.GetRelatedOfClass(
        setting_path->as_view(), L"Msvm_StorageAllocationSettingData",
        L"Msvm_VirtualSystemSettingDataComponent");
    if (!vhd_settings) { return false; }

    // we only want to look at
    // ResourceSubType = "Microsoft:Hyper-V:Virtual Hard Disk";
    // ResourceType = 31;
    // Resources
    // std::remove_if(std::begin(vhd_settings),
    //                std::end(vhd_settings),
    //                []() {

    //                });

    for (auto& vhd_setting : vhd_settings.value()) {
      LOG(L"vhd setting = {}",
          WMI::ObjectAsString(vhd_setting.system).as_view());

      if (!UpdatePath(vhd_setting, HardDiskPath.as_view())) { return false; }

      LOG(L"updated vhd setting = {}",
          WMI::ObjectAsString(vhd_setting.system).as_view());

      auto xml = WMI::ObjectAsXML(vhd_setting);
      if (!xml) { return false; }

      std::vector<WMI::SystemString> resources;
      resources.emplace_back(std::move(xml.value()));

      auto update_res = vsms->ModifyResourceSettings(resources);
      if (!update_res) { return false; }
    }

    if (new_vm_name) {
      WMI::SystemString s_new_vm_name{new_vm_name.value()};

      VARIANT Name;
      V_VT(&Name) = VT_BSTR;
      V_BSTR(&Name) = s_new_vm_name.get();
      auto put_res = setting.system->Put(L"ElementName", 0, &Name, 0);
      if (FAILED(put_res)) {
        LOG(L"Rename failed.  Err={} ({:X})", WMI::ErrorString(put_res),
            (uint32_t)put_res);
        return false;
      }
      auto xml = WMI::ObjectAsXML(setting);
      if (!xml) { return false; }
      auto rename_res = vsms->ModifySystemSettings(xml.value());
      if (!rename_res) { return false; }
    }
  }

  vsms->ValidatePlannedSystem(planned_system.value());
  auto realize_res
      = vsms->RealizePlannedSystem(std::move(planned_system.value()));
  if (!realize_res) { return false; }


  // auto destroy_res = vsms->DestroySystem(planned_system.value());
  // if (!destroy_res) { return false; }

  return true;
}

bool Test(const WMI& service, TestType tt)
{
  auto vsms_clz = VirtualSystemManagementService::Class::load(service);
  if (!vsms_clz) { return false; }
  auto vsms = vsms_clz->Get();
  if (!vsms) { return false; }

  auto vm = service.GetVMByName(L"Debian");

  if (!vm) { return false; }

  auto f = _bstr_t(L"C:\\Users\\Administrator\\AppData\\Local\\Temp");
  WMI::SystemString directory{f.GetBSTR()};

  auto vsesd = VirtualSystemExportSettingData::Class::load(service);
  if (!vsesd) { return false; }

  switch (tt) {
    case TestType::Full: {
      VirtualSystemExportSettingData settings = {
          .backup_intent = VirtualSystemExportSettingData::BackupIntent::Merge,
          .capture_live_state
          = VirtualSystemExportSettingData::CaptureLiveState::CrashConsistent,
          .copy_vm_runtime_information = false,
          .copy_vm_storage = true,
          .create_vm_export_subdirectory = true,
      };

      WMI::Result options = vsesd->GetText(settings);

      if (!options) { return false; }

      auto res = vsms->ExportSystemDefinition(vm.value(), directory, *options);

      if (!res) { return false; }
    } break;
    case TestType::FullAndIncr: {
      auto vsss_clz = VirtualSystemSnapshotService::Class::load(service);
      if (!vsss_clz) { return false; }

      auto vsss = vsss_clz->Get();
      if (!vsss) { return false; }


      auto vsssd_clz = VirtualSystemSnapshotSettingData::Class::load(service);
      if (!vsssd_clz) { return false; }

      VirtualSystemSnapshotSettingData ssettings = {
          .consistency_level
          = VirtualSystemSnapshotSettingData::ConsistencyLevel::CrashConsistent,
          .ignore_non_snapshottable_disks = true,
      };

      auto snapshot_settings = vsssd_clz->GetText(ssettings);

      if (!snapshot_settings) { return false; }

      LOG(L"Snapshot Settings = {}", snapshot_settings->as_view());

      auto snapshot = vsss->CreateSnapshot(
          vm.value(), snapshot_settings.value(),
          VirtualSystemSnapshotService::SnapshotType::RecoverySnapshot);
      if (!snapshot) { return false; }

      LOG(L"Snapshot = {}", WMI::ObjectAsString(snapshot->system).as_view());

      auto snapshot_path = snapshot->Path();

      if (!snapshot_path) { return false; }

      VirtualSystemExportSettingData settings = {
          .snapshot_virtual_system_path
          = std::wstring{snapshot_path->as_view()},
          .backup_intent = VirtualSystemExportSettingData::BackupIntent::Merge,
          .copy_snapshot_configuration = VirtualSystemExportSettingData::
              CopySnapshotConfiguration::ExportOneSnapshotForBackup,
          .capture_live_state
          = VirtualSystemExportSettingData::CaptureLiveState::CrashConsistent,
          .copy_vm_runtime_information = false,
          .copy_vm_storage = true,
          .create_vm_export_subdirectory = true,
      };

      WMI::Result options = vsesd->GetText(settings);
      if (!options) { return false; }

      {
        auto result
            = vsms->ExportSystemDefinition(vm.value(), directory, *options);
        if (!result) { return false; }
      }

      auto refpoint
          = vsss->ConvertToReferencePoint(std::move(snapshot).value());
      if (!refpoint) { return false; }
      LOG(L"Reference = {}", WMI::ObjectAsString(refpoint->system).as_view());


      auto vsrps_clz = VirtualSystemReferencePointService::Class::load(service);
      if (!vsrps_clz) { return false; }

      auto vsrps = vsrps_clz->Get();
      if (!vsrps) { return false; }

      auto incr_snapshot = vsss->CreateSnapshot(
          vm.value(), snapshot_settings.value(),
          VirtualSystemSnapshotService::SnapshotType::RecoverySnapshot);
      if (!incr_snapshot) { return false; }

      LOG(L"Incr_Snapshot = {}",
          WMI::ObjectAsString(incr_snapshot->system).as_view());

      {
        WMI::SystemString new_snapshot_name{
            std::wstring_view{L"Bareos Snapshot"}};

        VARIANT Name;
        V_VT(&Name) = VT_BSTR;
        V_BSTR(&Name) = new_snapshot_name.get();
        incr_snapshot->system->Put(L"ElementName", 0, &Name, 0);
        auto xml = WMI::ObjectAsXML(incr_snapshot.value());
        if (!xml) { return false; }
        auto rename_res = vsms->ModifySystemSettings(xml.value());
        if (!rename_res) { return false; }
      }

      LOG(L"Incr_Snapshot = {}",
          WMI::ObjectAsString(incr_snapshot->system).as_view());

      auto incr_snapshot_path = incr_snapshot->Path();
      if (!incr_snapshot_path) { return false; }

      auto refpath = refpoint->Path();
      if (!refpath) { return false; }
      VirtualSystemExportSettingData incr_settings = {
          .snapshot_virtual_system_path
          = std::wstring{incr_snapshot_path->as_view()},
          .differential_backup_base_path = std::wstring{refpath->as_view()},
          .backup_intent = VirtualSystemExportSettingData::BackupIntent::Merge,
          .copy_snapshot_configuration = VirtualSystemExportSettingData::
              CopySnapshotConfiguration::ExportOneSnapshotForBackup,
          .capture_live_state
          = VirtualSystemExportSettingData::CaptureLiveState::CrashConsistent,
          .copy_vm_runtime_information = false,
          .copy_vm_storage = true,
          .create_vm_export_subdirectory = true,
      };

      WMI::Result incr_options = vsesd->GetText(incr_settings);
      if (!incr_options) { return false; }

      {
        auto fi = _bstr_t(
            L"C:\\Users\\Administrator\\AppData\\Local\\Temp\\Incremental");
        WMI::SystemString incr_directory{fi.GetBSTR()};
        auto result = vsms->ExportSystemDefinition(vm.value(), incr_directory,
                                                   *incr_options);
        if (!result) { return false; }
      }

      vsrps->DestroyReferencePoint(std::move(refpoint).value());

      // {
      //   auto result = vsss->DestroySnapshot(std::move(snapshot).value());
      //   if (!result) { return false; }
      // }
    } break;
    case TestType::Restore: {
      WMI::SystemString SystemDefinitionFile{std::wstring_view{
          L"C:"
          L"\\Users\\Administrator\\AppData\\Local\\Temp\\Debian\\Vi"
          L"rtual Machines\\BCC1C544-EE85-41FE-9260-60627F74B850.vmcx"}};
      WMI::SystemString SnapshotPath{
          std::wstring_view{L"C:"
                            L"\\Users\\Administrator\\AppData\\Local\\Temp\\Deb"
                            L"ian\\Virtual Machines"}};

      bool GenerateNewSystemIdentifier = true;

      auto planned_system = vsms->ImportSystemDefinition(
          SystemDefinitionFile, SnapshotPath, GenerateNewSystemIdentifier);

      if (!planned_system) { return false; }

      vsms->ValidatePlannedSystem(planned_system.value());
      // // vsms->ImportSnapshotDefinitions(planned_system.value(),
      // //                                 SnapshotPath);
      // WMI::SystemString SnapshotPath2{std::wstring_view{
      //     L"C:"
      //     L"\\Users\\Administrator\\AppData\\Local\\Temp\\Debian\\Virtual
      //     Hard Disks"
      //   }};
      // vsms->ImportSnapshotDefinitions(planned_system.value(),
      //                                 SnapshotPath2);


      // We have now imported the system with the correct configuration.
      // That configuration still points to the wrong Hard Disks.  We need to
      // find every associated hard disk and change their paths to the
      // newly restored ones.
      auto related = service.GetRelatedOfClass(
          planned_system->Path()->as_view(), L"Msvm_VirtualSystemSettingData",
          L"Msvm_SettingsDefineState");
      if (!related) { return false; }

      WMI::SystemString HardDiskPath{
          std::wstring_view{L"C:"
                            L"\\Users\\Administrator\\AppData\\Local\\Temp\\Deb"
                            L"ian\\Virtual Hard Disks"}};
      LOG(L"Found {} settings", related->size());
      for (auto& setting : related.value()) {
        LOG(L"setting = {}", WMI::ObjectAsString(setting.system).as_view());

        auto setting_path = setting.Path();
        if (!setting_path) {
          LOG(L"Could not get path to setting");
          return false;
        }
        auto vhd_settings = service.GetRelatedOfClass(
            setting_path->as_view(), L"Msvm_StorageAllocationSettingData",
            L"Msvm_VirtualSystemSettingDataComponent");
        if (!vhd_settings) { return false; }

        // we only want to look at
        // ResourceSubType = "Microsoft:Hyper-V:Virtual Hard Disk";
        // ResourceType = 31;
        // Resources
        // std::remove_if(std::begin(vhd_settings),
        //                std::end(vhd_settings),
        //                []() {

        //                });

        for (auto& vhd_setting : vhd_settings.value()) {
          LOG(L"vhd setting = {}",
              WMI::ObjectAsString(vhd_setting.system).as_view());

          if (!UpdatePath(vhd_setting, HardDiskPath.as_view())) {
            return false;
          }

          LOG(L"updated vhd setting = {}",
              WMI::ObjectAsString(vhd_setting.system).as_view());

          auto xml = WMI::ObjectAsXML(vhd_setting);
          if (!xml) { return false; }

          std::vector<WMI::SystemString> resources;
          resources.emplace_back(std::move(xml.value()));

          auto update_res = vsms->ModifyResourceSettings(resources);
          if (!update_res) { return false; }
        }

        {
          WMI::SystemString new_vm_name{std::wstring_view{L"Bareos Restored"}};

          VARIANT Name;
          V_VT(&Name) = VT_BSTR;
          V_BSTR(&Name) = new_vm_name.get();
          auto put_res = setting.system->Put(L"ElementName", 0, &Name, 0);
          if (FAILED(put_res)) {
            LOG(L"Rename failed.  Err={} ({:X})", WMI::ErrorString(put_res),
                (uint32_t)put_res);
            return false;
          }
          auto xml = WMI::ObjectAsXML(setting);
          if (!xml) { return false; }
          auto rename_res = vsms->ModifySystemSettings(xml.value());
          if (!rename_res) { return false; }
        }
      }

      vsms->ValidatePlannedSystem(planned_system.value());
      auto realize_res
          = vsms->RealizePlannedSystem(std::move(planned_system.value()));
      if (!realize_res) { return false; }


      // auto destroy_res = vsms->DestroySystem(planned_system.value());
      // if (!destroy_res) { return false; }
    } break;
  }

  return true;
}

#include "CLI/App.hpp"
#include "CLI/Config.hpp"
#include "CLI/Formatter.hpp"

std::optional<std::wstring> MakeFullPath(const std::wstring& path)
{
  auto required_size = GetFullPathNameW(path.c_str(), 0, nullptr, 0);

  if (required_size <= 0) {
    LOG(L"Could not determine full path of dir {}", path);
    return std::nullopt;
  }

  std::wstring full_path;
  full_path.resize(required_size - 1);

  auto required_size_2 = GetFullPathNameW(
      path.c_str(), (DWORD)(full_path.size() + 1), full_path.data(), 0);

  if (required_size_2 <= 0) {
    LOG(L"Somehow could not determine full path of dir {}", path);
    return std::nullopt;
  }

  if (required_size_2 > full_path.size()) {
    LOG(L"Somehow could not determine full path of dir {} ({} != {})", path,
        required_size_2, full_path.size());
    return std::nullopt;
  }

  full_path.resize(required_size_2);

  LOG(L"Full Path of {} is {} ", path, full_path);
  return full_path;
}

int main(int argc, char** argv)
{
  CLI::App app;

  std::wstring vm_name;
  std::wstring backup_dir;
  std::wstring reference_point;
  std::wstring new_vm_name;
  std::wstring system_definition_file;
  std::wstring harddisk_dir;
  bool win32_api = false;
  auto full = app.add_subcommand("full", "create a full backup");
  {
    full->add_option("-V,--vm", vm_name)->required();
    full->add_option("-B,--backup-dir", backup_dir)
        ->required()
        ->check(CLI::ExistingDirectory);
    full->add_flag("-W,--use-win32-api", win32_api,
                   "Use the win32 api for backing up the disks");
  }
  auto incremental
      = app.add_subcommand("incremental", "create an incremental backup");
  {
    incremental->add_option("-V,--vm", vm_name)->required();
    incremental->add_option("-B,--backup-dir", backup_dir)
        ->required()
        ->check(CLI::ExistingDirectory);
    incremental->add_option("-R,--reference-point", reference_point)
        ->required();
    incremental->add_option("-W,--use-win32-api", win32_api);
  }

  auto restore = app.add_subcommand("restore", "restore a backup");
  {
    restore->add_option("-H,--harddisk-dir", harddisk_dir)
        ->required()
        ->check(CLI::ExistingDirectory);
    restore->add_option("-S,--system-definition", system_definition_file)
        ->required()
        ->check(CLI::ExistingFile);
    restore->add_option("-N,--new-name", new_vm_name);
  }

  std::wstring base_vhd_path;
  std::wstring patch_vhd_path;
  auto patch = app.add_subcommand("patch", "patch a vhd");
  {
    patch->add_option("-B,--base", base_vhd_path)
        ->required()
        ->check(CLI::ExistingFile);
    patch->add_option("-P,--patch", patch_vhd_path)
        ->required()
        ->check(CLI::ExistingFile);
  }

  std::wstring vhd1_path;
  std::wstring vhd2_path;
  auto diff = app.add_subcommand("diff", "diff two vhd");
  {
    diff->add_option("-L,--left,left", vhd1_path)
        ->required()
        ->check(CLI::ExistingFile);
    diff->add_option("-R,--right,right", vhd2_path)
        ->required()
        ->check(CLI::ExistingFile);
  }

  app.require_subcommand(1, 1);

  CLI11_PARSE(app, argc, argv);

  if (!backup_dir.empty()) {
    std::optional full_path = MakeFullPath(backup_dir);
    if (!full_path) { return -1; }
    backup_dir = std::move(full_path.value());
  }

  if (!harddisk_dir.empty()) {
    std::optional full_path = MakeFullPath(harddisk_dir);
    if (!full_path) { return -1; }
    harddisk_dir = std::move(full_path.value());
  }

  if (!system_definition_file.empty()) {
    std::optional full_path = MakeFullPath(system_definition_file);
    if (!full_path) { return -1; }
    system_definition_file = std::move(full_path.value());
  }


  HRESULT hres;

  // Step 1: --------------------------------------------------
  // Initialize COM. ------------------------------------------

  hres = CoInitializeEx(0, COINIT_MULTITHREADED);
  if (FAILED(hres)) {
    cout << "Failed to initialize COM library. Error code = 0x" << hex << hres
         << endl;
    return 1;  // Program has failed.
  }

  // Step 2: --------------------------------------------------
  // Set general COM security levels --------------------------

  hres = CoInitializeSecurity(
      NULL,
      -1,                           // COM authentication
      NULL,                         // Authentication services
      NULL,                         // Reserved
      RPC_C_AUTHN_LEVEL_DEFAULT,    // Default authentication
      RPC_C_IMP_LEVEL_IMPERSONATE,  // Default Impersonation
      NULL,                         // Authentication info
      EOAC_NONE,                    // Additional capabilities
      NULL                          // Reserved
  );


  if (FAILED(hres)) {
    cout << "Failed to initialize security. Error code = 0x" << hex << hres
         << endl;
    CoUninitialize();
    return 1;  // Program has failed.
  }

  // Step 3: ---------------------------------------------------
  // Obtain the initial locator to WMI -------------------------

  IWbemLocator* pLoc = NULL;

  hres = CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER,
                          IID_IWbemLocator, (LPVOID*)&pLoc);

  if (FAILED(hres)) {
    cout << "Failed to create IWbemLocator object."
         << " Err code = 0x" << hex << hres << endl;
    CoUninitialize();
    return 1;  // Program has failed.
  }

  IWbemServices* virt_service = nullptr;

  // Connect to the root\cimv2 namespace with
  // the current user and obtain pointer pSvc
  // to make IWbemServices calls.
  hres = pLoc->ConnectServer(
      _bstr_t(L"ROOT\\VIRTUALIZATION\\V2"),  // Object path of WMI namespace
      NULL,                                  // User name. NULL = current user
      NULL,                                  // User password. NULL = current
      0,                                     // Locale. NULL indicates current
      NULL,                                  // Security flags.
      0,             // Authority (for example, Kerberos)
      0,             // Context object
      &virt_service  // pointer to IWbemServices proxy
  );

  if (FAILED(hres)) {
    cout << "Could not connect. Error code = 0x" << hex << hres << endl;
    pLoc->Release();
    CoUninitialize();
    return 1;  // Program has failed.
  }

  cout << "Connected to ROOT\\VIRTUALIZATION\\V2 WMI namespace" << endl;


  // Step 5: --------------------------------------------------
  // Set security levels on the proxy -------------------------

  hres = CoSetProxyBlanket(virt_service,       // Indicates the proxy to set
                           RPC_C_AUTHN_WINNT,  // RPC_C_AUTHN_xxx
                           RPC_C_AUTHZ_NONE,   // RPC_C_AUTHZ_xxx
                           NULL,               // Server principal name
                           RPC_C_AUTHN_LEVEL_CALL,  // RPC_C_AUTHN_LEVEL_xxx
                           RPC_C_IMP_LEVEL_IMPERSONATE,  // RPC_C_IMP_LEVEL_xxx
                           NULL,                         // client identity
                           EOAC_NONE                     // proxy capabilities
  );

  if (FAILED(hres)) {
    cout << "Could not set proxy blanket. Error code = 0x" << hex << hres
         << endl;
    virt_service->Release();
    pLoc->Release();
    CoUninitialize();
    return 1;  // Program has failed.
  }

  WMI wmi{virt_service};
  if (app.got_subcommand(full)) {
    auto refpoint = [&] {
      if (win32_api) {
        return FullBackupWin32(wmi, vm_name, backup_dir);
      } else {
        return FullBackup(wmi, vm_name, backup_dir);
      }
    }();
    if (!refpoint) {
      cout << "Could not create full backup" << std::endl;
      return -1;
    }
    LOG(L"Reference = {}", WMI::ObjectAsString(refpoint->system).as_view());

    auto Path = refpoint->Path();

    if (!Path) {
      std::wcout << L"Count not determine path of created reference point ???"
                 << std::endl;
    }

    std::wcout << L"Reference Point = '" << Path->as_view() << L"'"
               << std::endl;
  }
  if (app.got_subcommand(incremental)) {
    auto refpoint = IncrementalBackup(wmi, vm_name, backup_dir, reference_point,
                                      win32_api);
    if (!refpoint) {
      cout << "Could not create incremental backup" << std::endl;
      return -1;
    }
    LOG(L"Reference = {}", WMI::ObjectAsString(refpoint->system).as_view());

    auto Path = refpoint->Path();

    if (!Path) {
      std::wcout << L"Count not determine path of created reference point ???"
                 << std::endl;
    }

    std::wcout << L"Reference Point = '" << Path->as_view() << L"'"
               << std::endl;
  }
  if (app.got_subcommand(restore)) {
    std::optional<std::wstring_view> new_name = std::nullopt;
    if (!new_vm_name.empty()) { new_name = new_vm_name; }

    if (!RestoreBackup(wmi, new_name, system_definition_file, harddisk_dir)) {
      std::wcout << "Restore failed" << std::endl;
    }
    std::wcout << "Restore succeeded" << std::endl;
  }
  if (app.got_subcommand(patch)) {
    if (!ApplyIncrementalToVhd(base_vhd_path, patch_vhd_path)) {
      std::wcout << "Patch failed" << std::endl;
      return 1;
    }
    std::wcout << "Patch succeeded" << std::endl;
  }
  if (app.got_subcommand(diff)) {
    if (!ComputeDiff(vhd1_path, vhd2_path)) {
      std::wcout << "Diff failed" << std::endl;
      return 1;
    }
    std::wcout << "Diff succeeded" << std::endl;
  }

  // Test(wmi, TestType::FullAndIncr);

  CoUninitialize();

  return 0;  // Program successfully completed.
}
