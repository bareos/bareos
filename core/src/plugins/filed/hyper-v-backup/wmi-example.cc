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
#include <vector>
#include <string>

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

      if (FAILED(hres)) {
        LOG(L"Could not create instance of class {}.  Err={} ({:X})",
            clz.Name.as_view(), WMI::ErrorString(hres), (uint64_t)hres);
        return std::nullopt;
      }

      if (!Set(instance, L"Description", settings.description)) {
        instance->Release();
        return std::nullopt;
      }
      if (!Set(instance, L"SnapshotVirtualSystemPath",
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
    BackupIntentPreserveChain
    = 0,                    // i.e. we want to store full/diff separately
    BackupIntentMerge = 1,  // i.e. we want to consolidate full/diff
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

      // BSTR system_str = nullptr;
      // auto name_res =
      // TargetSystem.system->GetObjectText(WBEM_FLAG_NO_FLAVORS, &system_str);
      // if (FAILED(name_res)) {
      //   LOG(L"GetObjectText(ComputerSystem) failed.  Err={} ({:X})",
      //       WMI::ErrorString(name_res), (uint64_t)name_res);
      //   return std::nullopt;
      // }
      // Arg.vt = VT_BSTR;
      // Arg.bstrVal = system_str;
      // {
      //   VARIANT Arg2;
      //   CIMTYPE Type2;
      //   auto name_res
      //     = TargetSystem.system->Get(L"Path", 0, &Arg2, &Type2, nullptr);

      //   if (FAILED(name_res)) {
      //     LOG(L"Get(ComputerSystem, Path) failed.  Err={} ({:X})",
      //         WMI::ErrorString(name_res), (uint64_t)name_res);
      //   } else {
      //     VariantClear(&Arg2);
      //   }
      // }
      // {
      //   VARIANT Arg2;
      //   CIMTYPE Type2;
      //   auto name_res
      //     = TargetSystem.system->Get(L"Path_", 0, &Arg2, &Type2, nullptr);

      //   if (FAILED(name_res)) {
      //     LOG(L"Get(ComputerSystem, Path_) failed.  Err={} ({:X})",
      //         WMI::ErrorString(name_res), (uint64_t)name_res);
      //   } else {
      //     VariantClear(&Arg2);
      //   }
      // }
      // {
      //   VARIANT Arg2;
      //   CIMTYPE Type2;
      //   auto name_res
      //     = TargetSystem.system->Get(L"__PATH", 0, &Arg2, &Type2, nullptr);

      //   if (FAILED(name_res)) {
      //     LOG(L"Get(ComputerSystem, __PATH) failed.  Err={} ({:X})",
      //         WMI::ErrorString(name_res), (uint64_t)name_res);
      //   } else {
      //     if (Type2 != CIM_STRING) { LOG(L"Bad type detected.  Type={}",
      //     Type2); } else { LOG(L"__PATH = {}",
      //                WMI::SystemString{V_BSTR(&Arg2)}.as_view()
      //               ); }
      //     VariantClear(&Arg2);
      //   }
      // }
      // {
      //   VARIANT Arg2;
      //   CIMTYPE Type2;
      //   auto name_res
      //     = TargetSystem.system->Get(L"__RELPATH", 0, &Arg2, &Type2,
      //     nullptr);

      //   if (FAILED(name_res)) {
      //     LOG(L"Get(ComputerSystem, __RELPATH) failed.  Err={} ({:X})",
      //         WMI::ErrorString(name_res), (uint64_t)name_res);
      //   } else {
      //     if (Type2 != CIM_STRING) { LOG(L"Bad type detected.  Type={}",
      //     Type2); } else { LOG(L"__RELPATH = {}",
      //                WMI::SystemString{V_BSTR(&Arg2)}.as_view()
      //               ); }
      //     VariantClear(&Arg2);
      //   }
      // }


      // CIMTYPE Type;
      // auto name_res
      //     = TargetSystem.system->Get(L"Name", 0, &Arg, &Type, nullptr);

      // if (FAILED(name_res)) {
      //   LOG(L"Get(ComputerSystem, Name) failed.  Err={} ({:X})",
      //       WMI::ErrorString(name_res), (uint64_t)name_res);
      //   return std::nullopt;
      // }

      // if (Type != CIM_STRING) { LOG(L"Bad type detected.  Type={}", Type); }


      WMI::Result Path = TargetSystem.Path();

      if (!Path) { return std::nullopt; }

      V_VT(&Arg) = VT_BSTR;
      V_BSTR(&Arg) = Path->get();

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

  auto vsesd = VirtualSystemExportSettingData::Class::load(service);

  VirtualSystemExportSettingData settings = {
      .capture_live_state
      = VirtualSystemExportSettingData::CaptureLiveState::CrashConsistent,
      .copy_vm_storage = false,
      .create_vm_export_subdirectory = true,
      .copy_vm_runtime_information = true,
  };

  WMI::Result options = vsesd->GetText(settings);

  if (!options) { return false; }

  auto res = vsms->ExportSystemDefinition(vm.value(), directory, *options);

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
