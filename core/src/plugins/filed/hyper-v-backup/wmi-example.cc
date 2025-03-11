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

using namespace std;

#pragma comment(lib, "wbemuuid.lib")

#include <string_view>
#include <optional>
#include <format>
#include <source_location>

struct Logger {
  Logger(std::source_location sloc = std::source_location::current())
      : loc{sloc}
  {
  }

  template <class... Args>
  void Log(std::wformat_string<Args...> fmt, Args&&... args)
  {
    wprintf(L"[LOG %hs:%d] %s\n", loc.function_name(), loc.line(),
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

  struct Method {
    SystemString Name;
    IWbemClassObject* parameter_def;
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

  template <typename T> using Result = std::optional<T>;

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

  IWbemClassObject* ExecMethod(const Class& clz,
                               const Method& method,
                               IWbemClassObject* Parameter) const
  {
    IWbemClassObject* OutParams = nullptr;
    auto result = service->ExecMethod(clz.Name.get(), method.Name.get(), 0,
                                      NULL, Parameter, &OutParams, NULL);

    if (FAILED(result)) {
      LOG(L"{}->{} failed.  Err={} ({:X})", clz.Name.as_view(),
          method.Name.as_view(), ErrorString(result), (uint64_t)result);
      return nullptr;
    }

    return OutParams;
  }
};

class VirtualSystemExportSettingData {
 public:
  struct Class {
    static std::optional<Class> load(const WMI& wmi)
    {
      auto clz = wmi.LoadClassByName(L"Msvm_VirtualSystemExportSettingData");

      if (!clz) { return std::nullopt; }

      auto get_text = wmi.LoadMethodByName(*clz, L"GetText");

      if (!get_text) { return std::nullopt; }

      return Class{std::move(clz.value()), std::move(get_text.value()), &wmi};
    }

    WMI::Result<VirtualSystemExportSettingData> instance() const
    {
      IWbemClassObject* instance = nullptr;
      auto hres = clz.class_ptr->SpawnInstance(0, &instance);

      if (FAILED(hres)) {
        LOG(L"Could not create instance of class {}.  Err={} ({:X})",
            clz.Name.as_view(), WMI::ErrorString(hres), (uint64_t)hres);
        return std::nullopt;
      }

      return VirtualSystemExportSettingData{*this, instance};
    }

   private:
    WMI::Class clz;
    WMI::Method get_text;
    const WMI* service;

    Class(WMI::Class clz_, WMI::Method get_text_, const WMI* service_)
        : clz{std::move(clz_)}
        , get_text{std::move(get_text_)}
        , service{service_}
    {
    }
  };

  WMI::Result<WMI::SystemString> GetText() { return {}; }

 private:
  const Class* clz;
  IWbemClassObject* instance;

  VirtualSystemExportSettingData(const Class& clz_, IWbemClassObject* instance_)
      : clz{&clz_}, instance{instance_}
  {
  }
};

class VirtualSystemManagementService {
  // Reference:
  // https://learn.microsoft.com/en-us/windows/win32/hyperv_v2/msvm-virtualsystemmanagementservice
 public:
  static std::optional<VirtualSystemManagementService> load(const WMI& wmi)
  {
    auto clz = wmi.LoadClassByName(L"Msvm_VirtualSystemManagementService");

    if (!clz) { return std::nullopt; }

    auto export_system = wmi.LoadMethodByName(*clz, L"ExportSystemDefinition");

    if (!export_system) { return std::nullopt; }

    return VirtualSystemManagementService{
        std::move(clz.value()), std::move(export_system.value()), &wmi};
  }

  struct ComputerSystem {
    IWbemClassObject* system;
  };

  WMI::Result<ComputerSystem> GetVMByName(std::wstring_view vm_name)
  {
#if 0
    WMI::SystemString name{vm_name};
    IWbemClassObject* vm = nullptr;
    auto hres = service->service->GetObject(name.get(), 0, NULL, &vm, NULL);
    if (FAILED(hres)) {
      LOG(L"GetObject({}) failed.  Err={} ({:X})", name.as_view(),
          WMI::ErrorString(hres), (uint32_t)hres);
      return std::nullopt;
    }

    LOG(L"VM at {}", (void*) vm);

    return ComputerSystem{ vm };
#else
    std::wstring query = std::format(
        L"SELECT * FROM Msvm_ComputerSystem WHERE ElementName=\"{}\"", vm_name);
    WMI::SystemString system_query{query};
    IEnumWbemClassObject* iter = nullptr;
    LOG(L"Query = {}", system_query.as_view());
    auto hres = service->service->ExecQuery(
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
    return ComputerSystem{found};
#endif
  }

  WMI::Result<uint32_t> ExportSystemDefinition(
      ComputerSystem& TargetSystem,
      const WMI::SystemString& ExportDirectory,
      const WMI::SystemString& ExportSettingData)
  {
    IWbemClassObject* params = nullptr;

    {
      auto hres = export_system.parameter_def->SpawnInstance(0, &params);

      if (FAILED(hres)) {
        LOG(L"SpawnInstance failed.  Err={} ({:X})", WMI::ErrorString(hres),
            (uint64_t)hres);
        return std::nullopt;
      }
    }
    {
      VARIANT Arg;
      Arg.vt = VT_BSTR;
      Arg.bstrVal = ExportDirectory.get();

      auto hres = params->Put(L"ExportDirectory", 0, &Arg, 0);

      if (FAILED(hres)) {
        LOG(L"Put(ExportDirectory) failed.  Err={} ({:X})",
            WMI::ErrorString(hres), (uint64_t)hres);
        return std::nullopt;
      } else {
        LOG(L"ExportDirectory = {}", ExportDirectory.as_view());
      }
    }
    {
      VARIANT Arg;
      Arg.vt = VT_BSTR;
      Arg.bstrVal = ExportSettingData.get();

      auto hres = params->Put(L"ExportSettingData", 0, &Arg, 0);

      if (FAILED(hres)) {
        LOG(L"Put(ExportSettingData) failed.  Err={} ({:X})",
            WMI::ErrorString(hres), (uint64_t)hres);
        return std::nullopt;
      }
    }
    {
      {
        auto* s = TargetSystem.system;
        BSTR class_name = nullptr;
        s->GetObjectText(0, &class_name);

        LOG(L"ClassName = {}",
            std::wstring_view{class_name, SysStringLen(class_name)});
        SysFreeString(class_name);
      }
      VARIANT Arg;
      // VariantInit(&Arg);
      //  Arg.vt = VT_UNKNOWN;
      //  {
      //    LOG(L"TargetSystem at {}", (void*) TargetSystem.system);
      //    IUnknown* ptr = nullptr;
      //    auto res = TargetSystem.system->QueryInterface(IID_IUnknown,
      //    (void**)&ptr); if (FAILED(res)) {
      //      LOG(L"QueryInterface failed.  Err=({:X})", (uint32_t) res);
      //      return std::nullopt;
      //    }
      //    LOG(L"Got ptr {}", (void*)ptr);
      //    Arg.punkVal = ptr;
      //  }

      BSTR system_name = nullptr;
      CIMTYPE Type;
      auto name_res
          = TargetSystem.system->Get(L"Name", 0, &Arg, &Type, nullptr);

      if (FAILED(name_res)) {
        LOG(L"Get(ComputerSystem, Name) failed.  Err={} ({:X})",
            WMI::ErrorString(name_res), (uint64_t)name_res);
        return std::nullopt;
      }

      if (Type != CIM_STRING) { LOG(L"Bad type detected.  Type={}", Type); }

      auto hres = params->Put(L"ComputerSystem", 0, &Arg, 0);

      VariantClear(&Arg);

      if (FAILED(hres)) {
        LOG(L"Put(ComputerSystem) failed.  Err={} ({:X})",
            WMI::ErrorString(hres), (uint64_t)hres);
        return std::nullopt;
      }
    }

    {
      auto* s = params;
      BSTR class_name = nullptr;
      s->GetObjectText(0, &class_name);

      LOG(L"Params = {}",
          std::wstring_view{class_name, SysStringLen(class_name)});
      SysFreeString(class_name);
    }


    auto result = service->ExecMethod(clz, export_system, params);

    if (!result) { return std::nullopt; }

    {
      VARIANT return_val;
      auto hres = result->Get(_bstr_t(L"ReturnValue"), 0, &return_val, NULL, 0);

      if (FAILED(hres)) {
        LOG(L"Could not get return value.  Err={} ({:X})",
            WMI::ErrorString(hres), (uint64_t)hres);
        return std::nullopt;
      }

      if (return_val.vt != VT_UI4) {
        VariantClear(&return_val);
        return std::nullopt;
      }

      return return_val.uintVal;
    }


    return 0;
  }

 private:
  WMI::Class clz;
  WMI::Method export_system;
  const WMI* service;

  VirtualSystemManagementService(WMI::Class clz_,
                                 WMI::Method export_system_,
                                 const WMI* service_)
      : clz{std::move(clz_)}
      , export_system{std::move(export_system_)}
      , service{service_}
  {
  }
};

// struct WMI_Proxy {
//
//   bool Stuff()
//   {
//     // Set up to call the Win32_Process::Create method
//     BSTR MethodName = SysAllocString(L"Create");
//     BSTR ClassName = SysAllocString(L"Win32_Process");
//
//     IWbemClassObject* pClass = NULL;
//     hres = pSvc->GetObject(ClassName, 0, NULL, &pClass, NULL);
//
//     IWbemClassObject* pInParamsDefinition = NULL;
//     hres = pClass->GetMethod(MethodName, 0, &pInParamsDefinition, NULL);
//
//     IWbemClassObject* pClassInstance = NULL;
//     hres = pInParamsDefinition->SpawnInstance(0, &pClassInstance);
//
//     // Create the values for the in-parameters
//     VARIANT varCommand;
//     varCommand.vt = VT_BSTR;
//     varCommand.bstrVal = _bstr_t(L"notepad.exe");
//
//     // Store the value for the in-parameters
//     hres = pClassInstance->Put(L"CommandLine", 0, &varCommand, 0);
//     wprintf(L"The command is: %s\n", V_BSTR(&varCommand));
//
//     IWbemClassObject* pOutParams = NULL;
//     hres = pSvc->ExecMethod(ClassName, MethodName, 0, NULL, pClassInstance,
//                             &pOutParams, NULL);
//
//     if (!FAILED(hres)) {
//       VARIANT varReturnValue;
//       hres = pOutParams->Get(_bstr_t(L"ReturnValue"), 0, &varReturnValue,
//       NULL,
//                              0);
//       wprintf(L"The command succeeded.\n");
//       VariantClear(&varReturnValue);
//     } else {
//       wprintf(L"The command failed.\n");
//     }
//
//     VariantClear(&varCommand);
//     SysFreeString(ClassName);
//     SysFreeString(MethodName);
//     pClass->Release();
//     pClassInstance->Release();
//     pInParamsDefinition->Release();
//     pOutParams->Release();
//   }
// };

// bool BusinessLogic(WMI_Proxy& proxy)
//{
//// maybe the name of the last snapshot is stored inside a restore object ?
// snapshot_ref last_snapshot;
// if (!proxy.GetLastSnapshot(last_snapshot)) {
// return false;
// }
//
// snapshot_ref current_snapshot;
// if (!proxy.CreateSnapshot(current_snapshot)) {
// return false;
// }
//
// exported_snapshot sn;
// if (!proxy.ExportSnapshot(sn, { .snapshot = current_snapshot })) {
// return false;
// }
//
// return false;
// }


bool Test(const WMI& service)
{
  auto vsms = VirtualSystemManagementService::load(service);
  if (!vsms) { return false; }

  auto vm = vsms->GetVMByName(L"Debian");

  if (!vm) { return false; }

  auto f = _bstr_t(L"C:\\Users\\Administrator\\AppData\\Local\\Temp");
  WMI::SystemString directory{f.GetBSTR()};
  WMI::SystemString options{_bstr_t(L"")};

  auto res = vsms->ExportSystemDefinition(vm.value(), directory, options);

  return true;
}

int main(int argc, char** argv)
{
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

  // Step 4: -----------------------------------------------------
  // Connect to WMI through the IWbemLocator::ConnectServer method

  IWbemServices* pSvc = NULL;

  // Connect to the root\cimv2 namespace with
  // the current user and obtain pointer pSvc
  // to make IWbemServices calls.
  hres = pLoc->ConnectServer(
      _bstr_t(L"ROOT\\CIMV2"),  // Object path of WMI namespace
      NULL,                     // User name. NULL = current user
      NULL,                     // User password. NULL = current
      0,                        // Locale. NULL indicates current
      NULL,                     // Security flags.
      0,                        // Authority (for example, Kerberos)
      0,                        // Context object
      &pSvc                     // pointer to IWbemServices proxy
  );

  if (FAILED(hres)) {
    cout << "Could not connect. Error code = 0x" << hex << hres << endl;
    pLoc->Release();
    CoUninitialize();
    return 1;  // Program has failed.
  }

  cout << "Connected to ROOT\\CIMV2 WMI namespace" << endl;


  // Step 5: --------------------------------------------------
  // Set security levels on the proxy -------------------------

  hres = CoSetProxyBlanket(pSvc,               // Indicates the proxy to set
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
    pSvc->Release();
    pLoc->Release();
    CoUninitialize();
    return 1;  // Program has failed.
  }

  // Step 6: --------------------------------------------------
  // Use the IWbemServices pointer to make requests of WMI ----

  // For example, get the name of the operating system
  IEnumWbemClassObject* pEnumerator = NULL;
  hres = pSvc->ExecQuery(bstr_t("WQL"),
                         bstr_t("SELECT * FROM Win32_OperatingSystem"),
                         WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
                         NULL, &pEnumerator);

  if (FAILED(hres)) {
    cout << "Query for operating system name failed."
         << " Error code = 0x" << hex << hres << endl;
    pSvc->Release();
    pLoc->Release();
    CoUninitialize();
    return 1;  // Program has failed.
  }

  // Step 7: -------------------------------------------------
  // Get the data from the query in step 6 -------------------

  IWbemClassObject* pclsObj = NULL;
  ULONG uReturn = 0;

  while (pEnumerator) {
    HRESULT hr = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);

    if (0 == uReturn) { break; }

    VARIANT vtProp;

    VariantInit(&vtProp);
    // Get the value of the Name property
    hr = pclsObj->Get(L"Name", 0, &vtProp, 0, 0);
    wcout << " OS Name : " << vtProp.bstrVal << endl;
    VariantClear(&vtProp);

    pclsObj->Release();
  }
  if (0) {
    // Set up to call the Win32_Process::Create method
    BSTR MethodName = SysAllocString(L"Create");
    BSTR ClassName = SysAllocString(L"Win32_Process");

    IWbemClassObject* pClass = NULL;
    hres = pSvc->GetObject(ClassName, 0, NULL, &pClass, NULL);

    IWbemClassObject* pInParamsDefinition = NULL;
    hres = pClass->GetMethod(MethodName, 0, &pInParamsDefinition, NULL);

    IWbemClassObject* pClassInstance = NULL;
    hres = pInParamsDefinition->SpawnInstance(0, &pClassInstance);

    // Create the values for the in-parameters
    VARIANT varCommand;
    varCommand.vt = VT_BSTR;
    varCommand.bstrVal = _bstr_t(L"notepad.exe");

    // Store the value for the in-parameters
    hres = pClassInstance->Put(L"CommandLine", 0, &varCommand, 0);
    wprintf(L"The command is: %s\n", V_BSTR(&varCommand));

    IWbemClassObject* pOutParams = NULL;
    hres = pSvc->ExecMethod(ClassName, MethodName, 0, NULL, pClassInstance,
                            &pOutParams, NULL);

    if (!FAILED(hres)) {
      VARIANT varReturnValue;
      hres = pOutParams->Get(_bstr_t(L"ReturnValue"), 0, &varReturnValue, NULL,
                             0);
      wprintf(L"The command succeeded.\n");
      VariantClear(&varReturnValue);
    } else {
      wprintf(L"The command failed.\n");
    }

    VariantClear(&varCommand);
    SysFreeString(ClassName);
    SysFreeString(MethodName);
    pClass->Release();
    pClassInstance->Release();
    pInParamsDefinition->Release();
    pOutParams->Release();
  }

  // Cleanup
  // ========

  pSvc->Release();
  pLoc->Release();
  pEnumerator->Release();


  {
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

    hres
        = CoSetProxyBlanket(virt_service,       // Indicates the proxy to set
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

    if (!Test({virt_service})) { wprintf(L"Business logic does not work!\n"); }
  }

  CoUninitialize();

  return 0;  // Program successfully completed.
}
