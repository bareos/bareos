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

#include <stdexcept>
#include <iostream>
#include <format>
#include <vector>
#include <chrono>
#include <cstdio>

#include <Windows.h>
#include <guiddef.h>
#include <Vss.h>
#include <VsWriter.h>
#include <VsBackup.h>

#include <atlbase.h>
#include <atlcomcli.h>

void dump_data();

int main()
{
  try {
    dump_data();

    return 0;
  } catch (const std::exception& ex) {
    std::cerr << ex.what() << std::endl;

    return 1;
  }
}

const char* hresult_as_str(HRESULT hr)
{
  switch (hr) {
    case E_INVALIDARG:
      return "One of the parameter values is not valid";
      break;
    case E_OUTOFMEMORY:
      return "The caller is out of memory or other system resources";
      break;
    case E_ACCESSDENIED:
      return "The caller does not have sufficient backup privileges or is not "
             "an administrator";
      break;
    case VSS_E_INVALID_XML_DOCUMENT:
      return "The XML document is not valid";
      break;
    case VSS_E_OBJECT_NOT_FOUND:
      return "The specified file does not exist";
      break;
    case VSS_E_BAD_STATE:
      return "Object is not initialized; called during restore or not called "
             "in "
             "correct sequence";
      break;
    case VSS_E_WRITER_INFRASTRUCTURE:
      return "The writer infrastructure is not operating properly. Check that "
             "the "
             "Event Service and VSS have been started, and check for errors "
             "associated with those services in the error log";
      break;
    case VSS_S_ASYNC_CANCELLED:
      return "The asynchronous operation was canceled by a previous call to "
             "IVssAsync::Cancel";
      break;
    case VSS_S_ASYNC_PENDING:
      return "The asynchronous operation is still running";
      break;
    case RPC_E_CHANGED_MODE:
      return "Previous call to CoInitializeEx specified the multithread "
             "apartment "
             "(MTA). This call indicates single-threaded apartment has "
             "occurred";
      break;
    case S_FALSE:
      return "No writer found for the current component";
      break;
    default:
      return "Unknown error";
      break;
  }
}

void throw_on_error(HRESULT hr, const char* callsite)
{
  if (hr == S_OK) { return; }

  throw std::runtime_error(
      std::format("{}: {} ({:X})", callsite, hresult_as_str(hr), hr));
}

#define COM_CALL(...)                            \
  do {                                           \
    throw_on_error((__VA_ARGS__), #__VA_ARGS__); \
  } while (0)

bool WaitOnJob(IVssAsync* job)
{
  for (;;) {
    using namespace std::chrono;
    constexpr seconds wait_time{5};

    COM_CALL(job->Wait(duration_cast<milliseconds>(wait_time).count()));

    HRESULT job_status;
    COM_CALL(job->QueryStatus(&job_status, NULL));

    switch (job_status) {
      case VSS_S_ASYNC_FINISHED: {
        return true;
      }
      case VSS_S_ASYNC_PENDING: {
        continue;
      }

      case VSS_S_ASYNC_CANCELLED:
        [[fallthrough]];
      default: {
        return false;
      }
    }
  }
}

bool GatherWriterMetaData(IVssBackupComponents* comp)
{
  CComPtr<IVssAsync> job;
  COM_CALL(comp->GatherWriterMetadata(&job));

  if (!WaitOnJob(job)) { return false; }

  return true;
}

struct vss_writer {
  VSS_ID id;
  CComPtr<IVssExamineWriterMetadata> metadata;
};

std::vector<vss_writer> get_writers(IVssBackupComponents* comp)
{
  UINT writer_count;
  COM_CALL(comp->GetWriterMetadataCount(&writer_count));

  std::vector<vss_writer> writers;
  writers.reserve(writer_count);

  for (std::size_t i = 0; i < writer_count; ++i) {
    auto& writer = writers.emplace_back();

    COM_CALL(comp->GetWriterMetadata(i, &writer.id, &writer.metadata));
  }

  return writers;
}

const char* vss_usage_as_str(VSS_USAGE_TYPE type)
{
  switch (type) {
    case VSS_UT_UNDEFINED:
      return "Undefined";
    case VSS_UT_BOOTABLESYSTEMSTATE:
      return "BootableSystemState";
    case VSS_UT_SYSTEMSERVICE:
      return "SystemService";
    case VSS_UT_USERDATA:
      return "UserData";
    case VSS_UT_OTHER:
      return "Other";
    default:
      return "Unknown";
  }
}

const char* vss_source_as_str(VSS_SOURCE_TYPE type)
{
  switch (type) {
    case VSS_ST_TRANSACTEDDB:
      return "TransactedDb";
    case VSS_ST_NONTRANSACTEDDB:
      return "NonTransactedDb";
    case VSS_ST_OTHER:
      return "Other";
    case VSS_ST_UNDEFINED:
      return "Undefined";
    default:
      return "Unknown";
  }
}

static constexpr VSS_ID SYSTEM_WRITER_ID
    = {0xE8132975,
       0x6F93,
       0x4464,
       {0xA5, 0x3E, 0x10, 0x50, 0x25, 0x3A, 0xE2, 0x20}};

static constexpr VSS_ID ASR_WRITER_ID
    = {0xBE000CBE,
       0x11FE,
       0x4426,
       {0x9C, 0x58, 0x53, 0x1A, 0xA6, 0x35, 0x5F, 0xC4}};

void dump_data()
{
  COM_CALL(CoInitializeEx(NULL, COINIT_MULTITHREADED));

  struct CoUninitializer {
    ~CoUninitializer() { CoUninitialize(); }
  };

  CoUninitializer _{};

  CComPtr<IVssBackupComponents> backup_components{};
  COM_CALL(CreateVssBackupComponents(&backup_components));

  COM_CALL(backup_components->InitializeForBackup());

  struct BackupAborter {
    ~BackupArborter()
    {
      if (backup_components) { backup_components->AbortBackup(); }
    }

    CComPtr<IVssBackupComponents>& backup_components;
  };

  // TODO: this only needs to be done between StartSnapshotSet/DoSnapshotSet
  BackupAborter _{backup_components};

  bool select_components = true;
  bool backup_bootable_system_state = false;
  VSS_BACKUP_TYPE backup_type = VSS_BT_COPY;
  bool partial_file_support = false;

  COM_CALL(backup_components->SetBackupState(
      select_components, backup_bootable_system_state, backup_type,
      partial_file_support));

  if (!GatherWriterMetaData(backup_components)) {
    throw std::runtime_error("Could not gather writer meta data");
  }

  auto writers = get_writers(backup_components);

  for (auto& writer : writers) {
    auto& metadata = writer.metadata;

    struct {
      VSS_ID instance_id;
      VSS_ID writer_id;
      CComBSTR name;
      VSS_USAGE_TYPE usage_type;
      VSS_SOURCE_TYPE source_type;
    } identity{};
    struct {
      UINT include, exclude, component;
    } counts{};

    COM_CALL(metadata->GetIdentity(&identity.instance_id, &identity.writer_id,
                                   &identity.name, &identity.usage_type,
                                   &identity.source_type));

    // these two writers should be skipped
    if (identity.writer_id == SYSTEM_WRITER_ID) {
      printf("=== SYSTEM WRITER DETECTED ===\n");
    }
    if (identity.writer_id == ASR_WRITER_ID) {
      printf("=== ASR WRITER DETECTED ===\n");
    }

    printf("%ls\n", (BSTR)identity.name);
    {
      wchar_t guid_storage[64] = {};

      StringFromGUID2(identity.instance_id, guid_storage, sizeof(guid_storage));
      printf("  Instance Id: %ls\n", guid_storage);
      StringFromGUID2(identity.writer_id, guid_storage, sizeof(guid_storage));
      printf("  Writer Id: %ls\n", guid_storage);
      printf("  Usage Type: %s (%u)\n", vss_usage_as_str(identity.usage_type),
             identity.usage_type);
      printf("  Source Type: %s (%u)\n",
             vss_source_as_str(identity.source_type), identity.source_type);
    }

    COM_CALL(metadata->GetFileCounts(&counts.include, &counts.exclude,
                                     &counts.component));

    printf("  %u Includes:\n", counts.include);
    for (UINT i = 0; i < counts.include; ++i) {
      CComPtr<IVssWMFiledesc> file;
      COM_CALL(metadata->GetIncludeFile(i, &file));

      CComBSTR path;
      COM_CALL(file->GetPath(&path));

      printf("    %ls\n", (BSTR)path);
    }

    printf("  %u Excludes:\n", counts.exclude);
    for (UINT i = 0; i < counts.exclude; ++i) {
      CComPtr<IVssWMFiledesc> file;
      COM_CALL(metadata->GetExcludeFile(i, &file));
      CComBSTR path;
      COM_CALL(file->GetPath(&path));

      printf("    %ls\n", (BSTR)path);
    }

    printf("  %u Components:\n", counts.component);
    for (UINT i = 0; i < counts.component; ++i) {
      CComPtr<IVssWMComponent> component;
      COM_CALL(metadata->GetComponent(i, &component));

      PVSSCOMPONENTINFO info{nullptr};
      COM_CALL(component->GetComponentInfo(&info));
      printf("    %u:\n", i);
      printf("      path: %ls\n", info->bstrLogicalPath);
      printf("      name: %ls\n", info->bstrComponentName);
      printf("      caption: %ls\n", info->bstrCaption);

      printf("      files: %u\n", info->cFileCount);
      for (UINT fileidx = 0; fileidx < info->cFileCount; ++fileidx) {
        printf("        %u:\n", fileidx);
        CComPtr<IVssWMFiledesc> file;
        COM_CALL(component->GetFile(fileidx, &file));

        CComBSTR path;
        COM_CALL(file->GetPath(&path));
        CComBSTR spec;
        COM_CALL(file->GetFilespec(&spec));
        bool recursive;
        COM_CALL(file->GetRecursive(&recursive));

        printf("          path: %ls\n", (BSTR)path);
        printf("          spec: %ls\n", (BSTR)spec);
        printf("          recursive: %s\n", recursive ? "true" : "false");
      }

      printf("      databases: %u\n", info->cDatabases);
      for (UINT dbidx = 0; dbidx < info->cDatabases; ++dbidx) {
        printf("        %u:\n", dbidx);
        CComPtr<IVssWMFiledesc> file;
        COM_CALL(component->GetDatabaseFile(dbidx, &file));

        CComBSTR path;
        COM_CALL(file->GetPath(&path));
        CComBSTR spec;
        COM_CALL(file->GetFilespec(&spec));
        bool recursive;
        COM_CALL(file->GetRecursive(&recursive));

        printf("          path: %ls\n", (BSTR)path);
        printf("          spec: %ls\n", (BSTR)spec);
        printf("          recursive: %s\n", recursive ? "true" : "false");
      }

      printf("      logs: %u\n", info->cLogFiles);
      for (UINT logidx = 0; logidx < info->cLogFiles; ++logidx) {
        printf("        %u:\n", logidx);
        CComPtr<IVssWMFiledesc> file;
        COM_CALL(component->GetDatabaseLogFile(logidx, &file));

        CComBSTR path;
        COM_CALL(file->GetPath(&path));
        CComBSTR spec;
        COM_CALL(file->GetFilespec(&spec));
        bool recursive;
        COM_CALL(file->GetRecursive(&recursive));

        printf("          path: %ls\n", (BSTR)path);
        printf("          spec: %ls\n", (BSTR)spec);
        printf("          recursive: %s\n", recursive ? "true" : "false");
      }

      printf("      dependencies: %u\n", info->cDependencies);
      for (UINT depidx = 0; depidx < info->cDependencies; ++depidx) {
        printf("        %u:\n", depidx);
        CComPtr<IVssWMDependency> dep;
        COM_CALL(component->GetDependency(depidx, &dep));

        CComBSTR name;
        COM_CALL(dep->GetComponentName(&name));
        CComBSTR path;
        COM_CALL(dep->GetLogicalPath(&path));
        VSS_ID writer_id;
        COM_CALL(dep->GetWriterId(&writer_id));

        printf("          name: %ls\n", (BSTR)name);
        printf("          path: %ls\n", (BSTR)path);

        wchar_t guid_storage[64] = {};
        StringFromGUID2(writer_id, guid_storage, sizeof(guid_storage));
        printf("          writer: %ls\n", guid_storage);
      }

      COM_CALL(component->FreeComponentInfo(info));
    }
  }

  backup_components.Release();
}
