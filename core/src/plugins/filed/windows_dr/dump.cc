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

#include <climits>
#include <stdexcept>
#include <iostream>
#include <vector>
#include <chrono>
#include <cstdio>
#include <span>
#include <unordered_map>
#include <variant>
#include <bit>
#include <algorithm>
#include <fstream>
#include <unordered_set>
#include <limits>

#include <Windows.h>
#include <guiddef.h>
#include <Vss.h>
#include <VsWriter.h>
#include <VsBackup.h>

#include <atlbase.h>
#include <atlcomcli.h>

#include "format.h"
#include "file_format.h"
#include "error.h"
#include "common.h"
#include "dump.h"
#include "com.h"
#include "remove_holes.h"

namespace {
std::string FromUtf16(std::wstring_view utf16)
{
  // WideCharToMultiByte does not handle empty strings
  if (utf16.size() == 0) { return {}; }

  // if the buffer is to small (or not supplied) the function returns
  // the number of bytes required
  DWORD required = WideCharToMultiByte(CP_UTF8, 0, utf16.data(), utf16.size(),
                                       nullptr, 0, nullptr, nullptr);
  if (required == 0) { return "<impossible conversion>"; }
  std::string utf8(required, '\0');

  // if the buffer is big enough the function returns the number of
  // bytes written
  DWORD written
      = WideCharToMultiByte(CP_UTF8, 0, utf16.data(), utf16.size(), utf8.data(),
                            utf8.size(), nullptr, nullptr);

  if (written != required) { return "<impossible conversion>"; }

  utf8.resize(written);
  return utf8;
}

std::string FromUtf16(const wchar_t* utf16p)
{
  if (!utf16p) { return {}; }
  return FromUtf16(std::wstring_view{utf16p});
}

bool GetBit(unsigned char* data, std::size_t index)
{
  return (data[index / CHAR_BIT] >> (index % CHAR_BIT)) & 0x1;
}

template <class... Ts> struct overloads : Ts... {
  using Ts::operator()...;
};
};  // namespace

std::size_t compute_plan_size(const insert_plan& plan)
{
  std::size_t computed_size = 0;
  for (auto& step : plan) {
    computed_size += std::visit(
        overloads{
            [](const insert_bytes& bytes) { return bytes.size(); },
            [](const insert_from& from) { return from.length; },
        },
        step);
  }
  return computed_size;
}

struct vec_writer : public writer {
  vec_writer() {}

  void write(const char* input, std::size_t size) override
  {
    buf.insert(std::end(buf), input, input + size);
  }
  virtual ~vec_writer() {}

  std::vector<char> get() { return std::move(buf); }

 private:
  std::vector<char> buf;
};

struct partition_extent {
  std::size_t partition_offset;
  std::size_t handle_offset;
  std::size_t length;

  HANDLE hndl;
};

extent_header header_of(const partition_extent& ext)
{
  extent_header header = {};

  header.offset = ext.partition_offset;
  header.length = ext.length;

  return header;
}

part_type from_win32(PARTITION_STYLE style)
{
  switch (style) {
    case PARTITION_STYLE_MBR: {
      return part_type::Mbr;
    }
    case PARTITION_STYLE_GPT: {
      return part_type::Gpt;
    }
    case PARTITION_STYLE_RAW: {
      return part_type::Raw;
    }
  }

  // TODO: we should report an error here and exit
  return part_type::Raw;
}

part_table_entry from_win32(const PARTITION_INFORMATION_EX& info)
{
  part_table_entry Result = {};
  Result.partition_offset = (uint64_t)info.StartingOffset.QuadPart;
  Result.partition_length = (uint64_t)info.PartitionLength.QuadPart;
  Result.partition_number = info.PartitionNumber;
  Result.partition_style = from_win32(info.PartitionStyle);
  Result.rewrite_partition = info.RewritePartition != 0;
  Result.is_service_partition = info.IsServicePartition != 0;
  return Result;
}

part_table_entry_gpt_data from_win32(const PARTITION_INFORMATION_GPT& gpt)
{
  part_table_entry_gpt_data Result = {};
  Result.partition_type = to_disk_format(gpt.PartitionType);
  Result.partition_id = to_disk_format(gpt.PartitionId);
  Result.attributes = gpt.Attributes;

  static_assert(sizeof(Result.name) == sizeof(gpt.Name));
  std::memcpy(Result.name, gpt.Name, sizeof(gpt.Name));

  return Result;
}

part_table_entry_mbr_data from_win32(const PARTITION_INFORMATION_MBR& mbr)
{
  part_table_entry_mbr_data Result = {};

  Result.partition_id = to_disk_format(mbr.PartitionId);
  Result.num_hidden_sectors = mbr.HiddenSectors;
  Result.partition_type = (uint8_t)mbr.PartitionType;
  Result.bootable = mbr.BootIndicator == TRUE;
  Result.recognized = mbr.RecognizedPartition == TRUE;

  return Result;
}

struct disk_reader {
  // TODO: make this dynamic (?)
  static constexpr std::size_t sector_size = 512;  // volume sector size
  static constexpr std::size_t page_size = 4096;

  GenericLogger* logger;


  disk_reader(std::size_t max_block_size, GenericLogger* logger_)
      : capacity{(max_block_size / page_size) * page_size}
      , buffer{(char*)_aligned_malloc(capacity, page_size)}
      , logger{logger_}
  {
  }

  struct aligned_deleter {
    void operator()(char* ptr) { _aligned_free(ptr); }
  };

  HANDLE current_handle = INVALID_HANDLE_VALUE;
  std::size_t disk_size = 0;
  std::size_t current_offset = std::numeric_limits<std::size_t>::max();
  std::size_t capacity = 0;
  std::size_t size = 0;

  // this buffer is always filled by size bytes
  // starting at current_offset
  std::unique_ptr<char[], aligned_deleter> buffer;

  std::span<char> get_cached(HANDLE hndl, std::size_t offset)
  {
    if (hndl != current_handle) {
      // cached data is from different handle
      return {};
    }

    if (current_offset > offset || current_offset + size < offset) {
      // no valid data available
      return {};
    }

    auto diff = offset - current_offset;

    return std::span{buffer.get() + diff, size - diff};
  }

  void switch_volume(HANDLE hndl)
  {
    current_handle = hndl;
    // invalidate our cache
    current_offset = std::numeric_limits<std::size_t>::max();
    size = 0;

    //

    STORAGE_PROPERTY_QUERY query = {
        .PropertyId = StorageAccessAlignmentProperty,
        .QueryType = PropertyStandardQuery,
    };

    {
      GET_LENGTH_INFORMATION length_info = {0};
      DWORD bytes_returned = 0;

      if (DeviceIoControl(hndl, IOCTL_DISK_GET_LENGTH_INFO, NULL, 0,
                          &length_info, sizeof(length_info), &bytes_returned,
                          NULL)) {
        disk_size = length_info.Length.QuadPart;
        logger->Trace("disk size = {}", disk_size);
      } else {
        disk_size = 0;
        logger->Info("could not determine disk size");
      }
    }
  }

  void refresh_cache(HANDLE hndl, std::size_t offset)
  {
    if (current_handle != hndl) { switch_volume(hndl); }
    if (offset != current_offset + size) {
      // the offset needs to be sector aligned.  We need to round _down_ here
      // to make sure that we read everything

      current_offset = (offset / sector_size) * sector_size;

      // logger->Trace([&] {
      //   return libbareos::format("current offset = {} (wanted: {})",
      //                            current_offset, offset);
      // });

      DWORD off_low = current_offset & 0xFFFFFFFF;
      LONG off_high = (current_offset >> 32) & 0xFFFFFFFF;
      SetFilePointer(current_handle, off_low, &off_high, FILE_BEGIN);
    } else {
      current_offset += size;
    }

    std::size_t bytes_to_read = std::min(capacity, disk_size - current_offset);

    if (current_offset == 0) {
      logger->Trace([&] {
        return libbareos::format("bytes_to_read = {}, buffer.get() = {}",
                                 bytes_to_read, (void*)buffer.get());
      });
    }
    size = 0;
    while (size < bytes_to_read) {
      DWORD bytes_read = 0;
      if (!ReadFile(current_handle, buffer.get() + size, bytes_to_read - size,
                    &bytes_read, NULL)) {
        logger->Trace([&] {
          return libbareos::format("oh oh error: {} ({}: {}, {} | {}, {})",
                                   GetLastError(), current_handle,
                                   buffer.get() + size, bytes_to_read - size,
                                   current_offset, size);
        });
        // throw win_error("ReadFile", GetLastError());
        break;
      }

      if (bytes_read == 0) { break; }

      size += bytes_read;
    }

    // logger->Trace( "wanted: {}, {}, cached: {}, {}-{} ({})",
    //         hndl, offset, current_handle, current_offset, current_offset +
    //         size, size);
  }

  std::size_t do_fill(HANDLE hndl, std::size_t offset, std::span<char> output)
  {
    // logger->Trace( "fill {} {}", hndl, offset);
    std::size_t bytes_written = 0;
    bool refreshed = false;
    for (;;) {
      auto to_write = output.subspan(bytes_written);
      auto cached = get_cached(hndl, offset + bytes_written);
      if (cached.size() < to_write.size()) {
        if (cached.size() > 0) {
          std::memcpy(to_write.data(), cached.data(), cached.size());

          bytes_written += cached.size();
        } else if (refreshed) {
          // there is still no data even after a refresh.
          // something went wrong, so we just return

          logger->Info("no data after refresh offset={} ({} + {}))!!!",
                       offset + bytes_written, offset, bytes_written);
          break;
        }
        refresh_cache(hndl, offset + bytes_written);
        refreshed = true;
      } else {
        std::memcpy(to_write.data(), cached.data(), to_write.size());

        bytes_written += to_write.size();

        // we are done!
        break;
      }
    }

    return bytes_written;
  }
};

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

struct dump_context {
 public:
  bool ignore_data{false};
  bool save_unknown_partitions{false};
  bool save_unknown_disks{false};
  bool save_unknown_extents{false};
  std::unordered_set<std::size_t> ignored_disk_ids;


  dump_context(GenericLogger* logger_) : logger{logger_} {}
  ~dump_context()
  {
    // we need to away to always delete these shadow copies
    // currently you can remove orphaned shadow copies via
    // diskshadow > delete shadows all
    for (auto hndl : open_handles) { CloseHandle(hndl); }
    if (snapshot) { snapshot->delete_snapshot(backup_components); }
  }

  insert_plan create()
  {
    logger->Info(" setting up vss");
    COM_CALL(CreateVssBackupComponents(&backup_components));

    COM_CALL(backup_components->InitializeForBackup());

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
        logger->Trace("=== SYSTEM WRITER DETECTED ===");
      }
      if (identity.writer_id == ASR_WRITER_ID) {
        logger->Trace("=== ASR WRITER DETECTED ===");
      }

      logger->Trace("{}", FromUtf16((BSTR)identity.name));
      {
        wchar_t guid_storage[64] = {};

        StringFromGUID2(identity.instance_id, guid_storage,
                        sizeof(guid_storage));
        logger->Trace("  Instance Id: {}", FromUtf16(guid_storage));
        StringFromGUID2(identity.writer_id, guid_storage, sizeof(guid_storage));
        logger->Trace("  Writer Id: {}", FromUtf16(guid_storage));
        logger->Trace("  Usage Type: {} ({})",
                      vss_usage_as_str(identity.usage_type),
                      (int)identity.usage_type);
        logger->Trace("  Source Type: {} ({})",
                      vss_source_as_str(identity.source_type),
                      (int)identity.source_type);
      }

      COM_CALL(metadata->GetFileCounts(&counts.include, &counts.exclude,
                                       &counts.component));

      logger->Trace("  {} Includes:", counts.include);
      for (UINT i = 0; i < counts.include; ++i) {
        CComPtr<IVssWMFiledesc> file;
        COM_CALL(metadata->GetIncludeFile(i, &file));

        CComBSTR path;
        COM_CALL(file->GetPath(&path));

        logger->Trace("    {}", FromUtf16((BSTR)path));
      }

      logger->Trace("  {} Excludes:", counts.exclude);
      for (UINT i = 0; i < counts.exclude; ++i) {
        CComPtr<IVssWMFiledesc> file;
        COM_CALL(metadata->GetExcludeFile(i, &file));
        CComBSTR path;
        COM_CALL(file->GetPath(&path));

        logger->Trace("    {}", FromUtf16((BSTR)path));
      }

      logger->Trace("  {} Components:", counts.component);
      for (UINT i = 0; i < counts.component; ++i) {
        CComPtr<IVssWMComponent> component;
        COM_CALL(metadata->GetComponent(i, &component));

        PVSSCOMPONENTINFO info{nullptr};
        COM_CALL(component->GetComponentInfo(&info));
        logger->Trace("    {}:", i);
        logger->Trace("      path: {}", FromUtf16(info->bstrLogicalPath));
        logger->Trace("      name: {}", FromUtf16(info->bstrComponentName));
        logger->Trace("      caption: {}", FromUtf16(info->bstrCaption));

        logger->Trace("      files: {}", info->cFileCount);
        for (UINT fileidx = 0; fileidx < info->cFileCount; ++fileidx) {
          logger->Trace("        {}:", fileidx);
          CComPtr<IVssWMFiledesc> file;
          COM_CALL(component->GetFile(fileidx, &file));

          CComBSTR path;
          COM_CALL(file->GetPath(&path));
          CComBSTR spec;
          COM_CALL(file->GetFilespec(&spec));
          bool recursive;
          COM_CALL(file->GetRecursive(&recursive));

          logger->Trace("          path: {}", FromUtf16((BSTR)path));
          logger->Trace("          spec: {}", FromUtf16((BSTR)spec));
          logger->Trace("          recursive: {}",
                        recursive ? "true" : "false");
        }

        logger->Trace("      databases: {}", info->cDatabases);
        for (UINT dbidx = 0; dbidx < info->cDatabases; ++dbidx) {
          logger->Trace("        {}:", dbidx);
          CComPtr<IVssWMFiledesc> file;
          COM_CALL(component->GetDatabaseFile(dbidx, &file));

          CComBSTR path;
          COM_CALL(file->GetPath(&path));
          CComBSTR spec;
          COM_CALL(file->GetFilespec(&spec));
          bool recursive;
          COM_CALL(file->GetRecursive(&recursive));

          logger->Trace("          path: {}", FromUtf16((BSTR)path));
          logger->Trace("          spec: {}", FromUtf16((BSTR)spec));
          logger->Trace("          recursive: {}",
                        recursive ? "true" : "false");
        }

        logger->Trace("      logs: {}", info->cLogFiles);
        for (UINT logidx = 0; logidx < info->cLogFiles; ++logidx) {
          logger->Trace("        {}:", logidx);
          CComPtr<IVssWMFiledesc> file;
          COM_CALL(component->GetDatabaseLogFile(logidx, &file));

          CComBSTR path;
          COM_CALL(file->GetPath(&path));
          CComBSTR spec;
          COM_CALL(file->GetFilespec(&spec));
          bool recursive;
          COM_CALL(file->GetRecursive(&recursive));

          logger->Trace("          path: {}", FromUtf16((BSTR)path));
          logger->Trace("          spec: {}", FromUtf16((BSTR)spec));
          logger->Trace("          recursive: {}",
                        recursive ? "true" : "false");
        }

        logger->Trace("      dependencies: {}", info->cDependencies);
        for (UINT depidx = 0; depidx < info->cDependencies; ++depidx) {
          logger->Trace("        {}:", depidx);
          CComPtr<IVssWMDependency> dep;
          COM_CALL(component->GetDependency(depidx, &dep));

          CComBSTR name;
          COM_CALL(dep->GetComponentName(&name));
          CComBSTR path;
          COM_CALL(dep->GetLogicalPath(&path));
          VSS_ID writer_id;
          COM_CALL(dep->GetWriterId(&writer_id));

          logger->Trace("          name: {}", FromUtf16((BSTR)name));
          logger->Trace("          path: {}", FromUtf16((BSTR)path));

          wchar_t guid_storage[64] = {};
          StringFromGUID2(writer_id, guid_storage, sizeof(guid_storage));
          logger->Trace("          writer: {}", FromUtf16(guid_storage));
        }

        COM_CALL(component->FreeComponentInfo(info));
      }
    }

    auto volumes = list_volumes();


    logger->Info(" creating a vss snapshot");
    snapshot.emplace(VssSnapshot::create(logger, backup_components, volumes));

    {
      wchar_t guid_storage[64] = {};
      StringFromGUID2(snapshot->snapshot_guid, guid_storage,
                      sizeof(guid_storage));
      logger->Info(" ... done! (=> Id = {})", FromUtf16(guid_storage));
    }

    auto paths = snapshot->snapshotted_paths(backup_components);

    disk_map candidate_disks;
    for (auto& [path, copy] : paths) {
      logger->Info(" examining volume {} ({})", FromUtf16(path),
                   FromUtf16(copy));
      std::wstring cpath = path;
      if (cpath.back() == L'\\') { cpath.pop_back(); }

      HANDLE volume
          = CreateFileW(cpath.c_str(), 0,
                        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                        NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);

      if (volume == INVALID_HANDLE_VALUE) {
        logger->Trace("volume {} ({}) -> could not open volume",
                      FromUtf16(path), FromUtf16(copy));
        throw win_error("CreateFileW", GetLastError());
      }

      logger->Trace("volume {} -> {}", FromUtf16(cpath), volume);

      HANDLE shadow = CreateFileW(
          copy.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_ALWAYS,
          FILE_ATTRIBUTE_NORMAL | FILE_FLAG_BACKUP_SEMANTICS
              | FILE_FLAG_SEQUENTIAL_SCAN | FILE_FLAG_OVERLAPPED,
          NULL);

      if (shadow == INVALID_HANDLE_VALUE) {
        logger->Trace("volume {} ({}) -> could not open shadow copy",
                      FromUtf16(path), FromUtf16(copy));
        CloseHandle(volume);
        throw win_error("CreateFileW", GetLastError());
      }
      {
        DWORD bytes_returned;
        if (!OverlappedIoControl(
                shadow,                        // handle to file
                FSCTL_ALLOW_EXTENDED_DASD_IO,  // dwIoControlCode
                NULL,                          // lpInBuffer
                0,                             // nInBufferSize
                NULL,                          // lpOutBuffer
                0,                             // nOutBufferSize
                &bytes_returned)) {
          logger->Trace("---- could not enable extended access (Err={}) ----",
                        GetLastError());
        } else {
          logger->Trace("---- extended access enabled ----");
        }
      }

      logger->Trace([&] {
        return libbareos::format("shadow {} -> {}", FromUtf16(copy), shadow);
      });

      // the operating system will clean up shadow on exit (yes, we leak it)
      GetVolumeExtents(candidate_disks, volume, shadow);
      open_handles.push_back(shadow);

      CloseHandle(volume);
      logger->Info(" ... done!");
    }

    // todo: what happens to volumes that are split between fixed disks/non
    // fixed disks ?
    disk_map disks;

    struct open_disk {
      HANDLE hndl;
      DISK_GEOMETRY_EX geo;
    };

    std::unordered_map<std::size_t, open_disk> disk_info;

    for (auto& [id, disk] : candidate_disks) {
      if (ignored_disk_ids.find(id) != ignored_disk_ids.end()) {
        logger->Info(" skipping ignored \\\\.\\PhysicalDrive{}", id);
        continue;
      }


      std::wstring disk_path = libbareos::format(L"\\\\.\\PhysicalDrive{}", id);

      logger->Info(" collecting info from disk \\\\.\\PhysicalDrive{}", id);

      HANDLE hndl = CreateFileW(
          disk_path.c_str(), GENERIC_READ,
          FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
          OPEN_EXISTING, FILE_ATTRIBUTE_READONLY | FILE_FLAG_BACKUP_SEMANTICS,
          NULL);

      if (hndl == INVALID_HANDLE_VALUE) {
        logger->Info("  could not open disk {}; skipping ...",
                     FromUtf16(disk_path));
        continue;
      }

      auto geo = GetDiskGeometry(hndl);
      if (!geo) {
        CloseHandle(hndl);
        continue;
      }

      if (geo->Geometry.MediaType != FixedMedia) {
        logger->Info("  disk {} has bad media type {}; skipping ...", id,
                     (int)geo->Geometry.MediaType);
        continue;
      }

      disks[id] = std::move(disk);
      disk_info[id] = open_disk{hndl, geo.value()};
      open_handles.push_back(hndl);

      logger->Info(" ... done!");
    }

    logger->Info(" generating backup plan");
    insert_plan plan;
    for (auto& [id, disk] : disks) {
      logger->Trace("disk {} extents:", id);

      std::sort(std::begin(disk.extents), std::end(disk.extents),
                [](auto& l, auto& r) {
                  return l.partition_offset < r.partition_offset;
                });

      auto [hndl, geo] = disk_info[id];

      logger->Trace(
          "disk geometry:\n"
          " - Size: {}\n"
          " - Cylinders: {}\n"
          " - Tracks/C: {}\n"
          " - Sectors/T: {}\n"
          " - Bytes/S: {}",
          geo.DiskSize.QuadPart, geo.Geometry.Cylinders.QuadPart,
          geo.Geometry.TracksPerCylinder, geo.Geometry.SectorsPerTrack,
          geo.Geometry.BytesPerSector);

      auto layout = GetPartitionLayout(hndl);
      if (!layout) { continue; }

      InsertMissingExtents(hndl, layout.value(), disk.extents);

      std::size_t total = 0;
      for (auto& extent : disk.extents) {
        total += extent.length;
        logger->Trace("  {} -> {}", extent.partition_offset,
                      extent.partition_offset + extent.length);
      }

      logger->Trace("  => total = {}", total);

      if (ignore_data) { disk.extents.clear(); }

      WriteDiskHeader(plan, disk, geo);
      WriteDiskPartTable(plan, layout.value());
      WriteDiskData(plan, disk);
    }
    std::size_t payload_size = compute_plan_size(plan);
    PrependFileHeader(plan, disks, payload_size);

    logger->Info(" ... done!");
    return plan;
  }

 private:
  bool OverlappedIoControl(HANDLE handle,
                           DWORD control_code,
                           void* in_buffer,
                           DWORD in_length,
                           void* out_buffer,
                           DWORD out_length,
                           DWORD* bytes_written)
  {
    OVERLAPPED overlapped = {};

    if (DeviceIoControl(handle, control_code, in_buffer, in_length, out_buffer,
                        out_length, bytes_written, &overlapped)) {
      return true;
    }

    auto err = GetLastError();
    if (err != ERROR_IO_PENDING) {
      SetLastError(err);
      return false;
    }

    return GetOverlappedResult(handle, &overlapped, bytes_written, TRUE);
  }

  bool OverlappedRead(HANDLE handle, std::size_t offset, std::span<char> buffer)
  {
    OVERLAPPED overlapped = {};
    overlapped.Offset = static_cast<uint32_t>(offset);
    overlapped.OffsetHigh = static_cast<uint32_t>(offset >> 32);

    std::size_t total_bytes_read = 0;
    while (total_bytes_read < buffer.size()) {
      auto to_read = buffer.subspan(total_bytes_read);
      DWORD bytes_read = 0;
      DWORD bytes_to_read = static_cast<DWORD>(
          std::min(static_cast<std::size_t>(std::numeric_limits<DWORD>::max()),
                   to_read.size()));
      if (!ReadFile(handle, to_read.data(), bytes_to_read, &bytes_read,
                    &overlapped)) {
        auto err = GetLastError();
        if (err == ERROR_IO_PENDING) {
          if (!GetOverlappedResult(handle, &overlapped, bytes_read, TRUE)) {
            err = GetLastError();
          } else {
            err = ERROR_SUCCESS;
          }
        }

        if (err != ERROR_SUCCESS) {
          logger->Trace("could not read from handle {} at {}: Err={}", handle,
                        offset, err);
          return false;
        }
      }

      total_bytes_read += bytes_read;
    }
    return true;
  }


  void InsertMissingExtents(HANDLE disk_handle,
                            const partition_layout& layout,
                            std::vector<partition_extent>& extents)
  {
    /* extents currently contains all extents _from the current disk_ that we
     * wish to save (they are probably all coming from the volumes that we
     * snapshotted) our task is now to insert relevant extents that were not
     * part of the snapshot. */
    // we assume that these extents are sorted

    std::vector<partition_extent> new_extents;
    for (auto& info : layout.partition_infos) {
      std::size_t offset = info.StartingOffset.QuadPart;
      std::size_t length = info.PartitionLength.QuadPart;

      if (info.PartitionStyle == PARTITION_STYLE_MBR
          && info.Mbr.PartitionType == PARTITION_ENTRY_UNUSED) {
        continue;
      }
      if (length == 0) { continue; }

      // first_extent is the index of the first extent whose end
      // is larger than the start of the partition (i.e. offset)
      std::size_t first_extent = extents.size();
      for (std::size_t i = 0; i < extents.size(); ++i) {
        if (extents[i].partition_offset + extents[i].length > offset) {
          first_extent = i;
          break;
        }
      }

      // if the extent we found starts _after_ our partition begins, then
      // their intersection is empty, and we are in the case where the partition
      // contains _no_ snapshotted data.
      // if no first_extent exists, then we are also in this case.
      if (first_extent == extents.size()
          || offset + length <= extents[first_extent].partition_offset) {
        if (save_unknown_partitions) {
          new_extents.push_back({offset, offset, length, disk_handle});
          logger->Trace("saving unknown partition {} ({}-{})",
                        info.PartitionNumber, offset, offset + length);
        }
        continue;
      }

      if (save_unknown_extents) {
        auto current_extent = first_extent;
        auto current_offset = offset;
        // loop invariant:
        // current_offset < end(extents[current_extent])
        while (current_offset < offset + length
               && current_extent < extents.size()) {
          auto& extent = extents[current_extent];
          if (current_offset < extent.partition_offset) {
            std::size_t hole_end
                = std::min(extent.partition_offset, offset + length);

            std::size_t hole_size = hole_end - current_offset;

            logger->Trace("saving unknown extent {}-{}", current_offset,
                          hole_end);

            new_extents.push_back(
                {current_offset, current_offset, hole_size, disk_handle});
          }

          current_offset = extent.partition_offset + extent.length;
          current_extent += 1;
        }
      }
    }
  }

  std::vector<std::wstring> list_volumes()
  {
    std::vector<std::wstring> volumes;
    wchar_t VolumeGUID[MAX_PATH];
    HANDLE iter = FindFirstVolumeW(VolumeGUID, sizeof(VolumeGUID));

    if (iter == INVALID_HANDLE_VALUE) {
      throw win_error("FindFirstVolumeW", GetLastError());
    }

    for (;;) {
      volumes.emplace_back(VolumeGUID);
      logger->Trace("Volume '{}'", FromUtf16(volumes.back()));

      if (!FindNextVolumeW(iter, VolumeGUID, sizeof(VolumeGUID))) {
        auto err = GetLastError();
        if (err == ERROR_NO_MORE_FILES) { break; }
        throw win_error("FindNextVolumeW", err);
      }
    }

    FindVolumeClose(iter);

    return volumes;
  }

  struct VssSnapshot {
    static VssSnapshot create(GenericLogger* logger,
                              IVssBackupComponents* vss,
                              std::span<const std::wstring> volumes)
    {
      wchar_t guid_storage[64] = {};
      GUID snapshot_guid = GUID_NULL;

      COM_CALL(vss->StartSnapshotSet(&snapshot_guid));

      StringFromGUID2(snapshot_guid, guid_storage, sizeof(guid_storage));
      logger->Trace("snapshot set => {}", FromUtf16(guid_storage));

      BackupAborter aborter{vss};

      for (auto& vol : volumes) {
        VSS_ID volume_id = {};
        vss->AddToSnapshotSet(const_cast<VSS_PWSZ>(vol.c_str()), GUID_NULL,
                              &volume_id);

        StringFromGUID2(volume_id, guid_storage, sizeof(guid_storage));
        logger->Trace("{} => {}", FromUtf16(vol), FromUtf16(guid_storage));
      }

      CComPtr<IVssAsync> prepare_job;
      COM_CALL(vss->PrepareForBackup(&prepare_job));
      WaitOnJob(prepare_job);

      CComPtr<IVssAsync> snapshot_job;
      COM_CALL(vss->DoSnapshotSet(&snapshot_job));
      WaitOnJob(snapshot_job);

      aborter.backup_components = nullptr;  // not needed anymore

      return {snapshot_guid, logger};
    }

    GUID snapshot_guid;
    GenericLogger* logger;

    auto snapshotted_paths(CComPtr<IVssBackupComponents> vss)
    {
      std::unordered_map<std::wstring, std::wstring> paths;
      CComPtr<IVssEnumObject> iter;
      COM_CALL(
          vss->Query(GUID_NULL, VSS_OBJECT_NONE, VSS_OBJECT_SNAPSHOT, &iter));

      VSS_OBJECT_PROP prop;

      for (;;) {
        ULONG fetched_count;
        auto err = iter->Next(1, &prop, &fetched_count);

        if (fetched_count == 0) { break; }

        throw_on_error(err, "iter->Next(1, &prop, &fetched_count)");

        wchar_t guid_storage[64] = {};
        StringFromGUID2(prop.Obj.Snap.m_SnapshotSetId, guid_storage,
                        sizeof(guid_storage));
        logger->Trace("found guid => {}", FromUtf16(guid_storage));

        if (prop.Obj.Snap.m_SnapshotSetId == snapshot_guid) {
          logger->Trace("{} => {}",
                        FromUtf16(prop.Obj.Snap.m_pwszOriginalVolumeName),
                        FromUtf16(prop.Obj.Snap.m_pwszSnapshotDeviceObject));

          paths.emplace(prop.Obj.Snap.m_pwszOriginalVolumeName,
                        prop.Obj.Snap.m_pwszSnapshotDeviceObject);
        }

        VssFreeSnapshotProperties(&prop.Obj.Snap);
      }

      return std::move(paths);
    }

    void delete_snapshot(CComPtr<IVssBackupComponents> vss)
    {
      GUID bad_snapshot;
      LONG deleted_count;

      COM_CALL(vss->DeleteSnapshots(snapshot_guid, VSS_OBJECT_SNAPSHOT_SET,
                                    FALSE, &deleted_count, &bad_snapshot));
    }

   private:
    struct BackupAborter {
      ~BackupAborter()
      {
        if (backup_components) { backup_components->AbortBackup(); }
      }

      IVssBackupComponents* backup_components;
    };
  };

  struct part_header {
    uint64_t magic;
    uint64_t size;

    enum part_type : uint8_t
    {
      Mbr = 0,
      Gpt = 1,
      Partition = 2,
    };

    uint8_t type;
  };

  struct disk {
    std::vector<partition_extent> extents;
  };

  using disk_map = std::unordered_map<std::size_t, disk>;

  size_t GetClusterSize(HANDLE volume)
  {
    DWORD bytes_written;
    NTFS_VOLUME_DATA_BUFFER buffer;

    if (!DeviceIoControl(volume, FSCTL_GET_NTFS_VOLUME_DATA, NULL, 0, &buffer,
                         sizeof(buffer), &bytes_written, nullptr)) {
      return 0;
    }

    logger->Trace(
        "{} => sectors: {}, clusters: {}({},{})\nmft: {} "
        "mft-mirror: {} "
        "mft-zone: {}-{}\n",
        volume, (size_t)buffer.NumberSectors.QuadPart,
        (size_t)buffer.TotalClusters.QuadPart,
        (size_t)buffer.FreeClusters.QuadPart,
        (size_t)buffer.TotalReserved.QuadPart,
        (size_t)buffer.MftStartLcn.QuadPart,
        (size_t)buffer.Mft2StartLcn.QuadPart,
        (size_t)buffer.MftZoneStart.QuadPart,
        (size_t)buffer.MftZoneEnd.QuadPart);
    return buffer.BytesPerCluster;
  }

  std::optional<used_bitmap> GetBitmap(std::vector<char>& buffer,
                                       HANDLE disk,
                                       std::size_t start,
                                       std::size_t length,
                                       std::size_t cluster_size)
  {
    // this is the minimum.  Its possible that we actually need to provide
    // more buffer to the ioctl call, as it might not respect the starting lcn
    // (it is allowed to choose an earlier lcn instead!)

    // for some reason, even if the volume size is divisible by the
    // cluster size, the max amount of clusters in a volume will be
    // ceil( size / cluster_size ) - 1.  We have to be careful about this!

    assert(cluster_size > 0);
    auto start_cluster = start / cluster_size;
    auto end_cluster = (start + length + cluster_size - 1) / cluster_size;
    auto length_cluster = end_cluster - start_cluster;

    auto bitmap_bytes = (length_cluster + CHAR_BIT - 1) / CHAR_BIT;
    auto struct_size = offsetof(VOLUME_BITMAP_BUFFER, Buffer[bitmap_bytes]);

    buffer.resize(struct_size);

    DWORD bytes_written = 0;
    STARTING_LCN_INPUT_BUFFER Start
        = {.StartingLcn = {.QuadPart = static_cast<LONGLONG>(start_cluster)}};

    for (;;) {
      if (!OverlappedIoControl(disk, FSCTL_GET_VOLUME_BITMAP, &Start,
                               sizeof(Start), buffer.data(), buffer.size(),
                               &bytes_written)) {
        auto error = GetLastError();

        if (error == ERROR_MORE_DATA) {
          bool enough_data = false;
          if (bytes_written > sizeof(VOLUME_BITMAP_BUFFER)) {
            VOLUME_BITMAP_BUFFER* buf
                = reinterpret_cast<VOLUME_BITMAP_BUFFER*>(buffer.data());

            std::size_t bitmap_count = buf->BitmapSize.QuadPart;
            std::size_t bitmap_start = buf->StartingLcn.QuadPart;

            if (bitmap_start + bitmap_count >= end_cluster) {
              enough_data = true;
            }
          }

          if (!enough_data) {
            std::cerr << libbareos::format(
                "VOLUME_BITMAP returned {} (size = {}, needed = {})",
                bytes_written, buffer.size(), struct_size)
                      << std::endl;

            buffer.resize(buffer.size() * 2);

            continue;
          }
        }

        return std::nullopt;
      }

      VOLUME_BITMAP_BUFFER* buf
          = reinterpret_cast<VOLUME_BITMAP_BUFFER*>(buffer.data());

      used_bitmap map;

      std::size_t bitmap_count = buf->BitmapSize.QuadPart;
      std::size_t bitmap_start = buf->StartingLcn.QuadPart;

      logger->Trace(
          "bitmap:: start: {}, count: {} (bytes: {}, guessed: {})",
          bitmap_start, bitmap_count, bytes_written,
          offsetof(VOLUME_BITMAP_BUFFER, Buffer[(bitmap_count + 7) / 8]));

      map.start = bitmap_start * cluster_size;
      map.unit_size = cluster_size;
      map.bits.resize(bitmap_count);
      for (std::size_t bit = 0; bit < bitmap_count; ++bit) {
        map.bits[bit] = GetBit(buf->Buffer, bit);
      }

      return map;
    }
  }

  void GetVolumeExtents(disk_map& disks, HANDLE volume, HANDLE data_volume)
  {
    size_t cluster_size = GetClusterSize(data_volume);

    logger->Trace("cluster size = {}", cluster_size);

    std::vector<char> extent_buffer;
    extent_buffer.resize(1024 * 1024);

    DWORD bytes_written = 0;
    for (;;) {
      if (!DeviceIoControl(volume, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS,
                           nullptr, 0, extent_buffer.data(),
                           extent_buffer.size(), &bytes_written, nullptr)) {
        auto err = GetLastError();

        if (err != ERROR_MORE_DATA) {
          logger->Info("DISK_EXTENTS returned {} for volume {}", err, volume);
          return;
        }

        extent_buffer.resize(extent_buffer.size() * 2);
      }

      break;
    }

    if (bytes_written == 0) {
      // no extents
      logger->Info("volume {} has no extents ...", volume);
      return;
    }
    auto extents = reinterpret_cast<VOLUME_DISK_EXTENTS*>(extent_buffer.data());

    std::vector<char> bitmap_buffer;

    std::size_t volume_offset = 0;
    std::vector<used_interval> used;

    std::size_t used_clusters = 0;
    std::size_t free_clusters = 0;

    for (size_t i = 0; i < extents->NumberOfDiskExtents; ++i) {
      auto& extent = extents->Extents[i];
      logger->Trace("volume {} extent {} @ {} -> {}", volume, extent.DiskNumber,
                    extent.StartingOffset.QuadPart,
                    extent.ExtentLength.QuadPart);

      auto& disk = disks[extent.DiskNumber];

      auto bits = [&]() -> std::optional<used_bitmap> {
        if (cluster_size == 0) { return std::nullopt; }

        return GetBitmap(bitmap_buffer, data_volume, volume_offset,
                         extent.ExtentLength.QuadPart, cluster_size);
      }();

      if (bits) {
        static constexpr std::size_t min_hole_size = 1;  // 1 mb

        for (auto bit : bits->bits) {
          if (bit) {
            used_clusters += 1;
          } else {
            free_clusters += 1;
          }
        }

        used.clear();
        find_used_data(used, bits.value(), volume_offset,
                       volume_offset + extent.ExtentLength.QuadPart,
                       min_hole_size);

        logger->Trace("{}({}):{} -> ", extent.StartingOffset.QuadPart,
                      volume_offset, extent.ExtentLength.QuadPart);

        std::size_t ext_size
            = static_cast<std::size_t>(extent.ExtentLength.QuadPart);

        std::size_t total_size = 0;
        for (auto [start, length] : used) {
          assert(start >= volume_offset);
          assert(start - volume_offset + length <= ext_size);

          auto disk_offset
              = extent.StartingOffset.QuadPart + (start - volume_offset);

          logger->Trace(" - {}({}):{}", disk_offset, start, length);

          total_size += length;

          disk.extents.push_back({disk_offset, start, length, data_volume});
        }

        assert(total_size <= ext_size);
      } else {
        disk.extents.push_back(
            {(size_t)extent.StartingOffset.QuadPart, volume_offset,
             (size_t)extent.ExtentLength.QuadPart, data_volume});
      }

      volume_offset += (size_t)extent.ExtentLength.QuadPart;
    }

    logger->Trace("volume used: {} free: {} total: {}", used_clusters,
                  free_clusters, used_clusters + free_clusters);
  }

  std::optional<partition_layout> partitioning(
      DRIVE_LAYOUT_INFORMATION_EX* info)
  {
    for (DWORD i = 0; i < info->PartitionCount; ++i) {
      auto& entry = info->PartitionEntry[i];

      logger->Trace("  partition {}", i);

      switch (entry.PartitionStyle) {
        case PARTITION_STYLE_MBR: {
          logger->Trace("    style: mbr");
          auto& mbr = entry.Mbr;
          logger->Trace("      type: {}", mbr.PartitionType);
          logger->Trace("      boot?: {}", mbr.BootIndicator ? "yes" : "no");
          logger->Trace("      recognized?: {}",
                        mbr.RecognizedPartition ? "yes" : "no");
          logger->Trace("      hidden sectors: {}", mbr.HiddenSectors);

          wchar_t guid_storage[64] = {};

          StringFromGUID2(mbr.PartitionId, guid_storage, sizeof(guid_storage));
          logger->Trace("      partition id: {}", FromUtf16(guid_storage));
        } break;
        case PARTITION_STYLE_RAW: {
          logger->Trace("    style: raw");
        } break;
        case PARTITION_STYLE_GPT: {
          logger->Trace("    style: gpt");
          auto& gpt = entry.Gpt;
          wchar_t guid_storage[64] = {};

          StringFromGUID2(gpt.PartitionType, guid_storage,
                          sizeof(guid_storage));
          logger->Trace("      type: {}", FromUtf16(guid_storage));
          StringFromGUID2(gpt.PartitionId, guid_storage, sizeof(guid_storage));
          logger->Trace("      partition id: {}", FromUtf16(guid_storage));
          logger->Trace("      attributes: {}", gpt.Attributes);
          logger->Trace("      name: {}", FromUtf16(std::wstring_view{
                                              gpt.Name, sizeof(gpt.Name)}));
        } break;
        default: {
          logger->Trace("    style: unknown");
        } break;
      }

      logger->Trace("    offset: {}", entry.StartingOffset.QuadPart);
      logger->Trace("    length: {}", entry.PartitionLength.QuadPart);
      logger->Trace("    number: {}", entry.PartitionNumber);
      logger->Trace("    rewrite?: {}", entry.RewritePartition ? "yes" : "no");
      // logger->Trace( "    service?: {}",
      //         entry.IsServicePartition ? "yes" : "no");
    }
    return std::nullopt;
  }

  void read_volume_file(HANDLE hndl, std::span<char> buffer)
  {
    logger->Trace("reading bootstrap (size = {})", buffer.size());

    alignas(4096) char real_buffer[4096];

    std::size_t bytes_to_read = buffer.size();
    std::size_t total_bytes = 0;
    while (total_bytes < bytes_to_read) {
      DWORD bytes_read = 0;

      if (!ReadFile(hndl, real_buffer, (DWORD)std::size(real_buffer),
                    &bytes_read, NULL)) {
        auto err = GetLastError();
        logger->Trace("could not read from {}: Err={}", hndl, err);

        throw win_error("ReadFile", err);
      }

      if (bytes_read != std::size(real_buffer)) {
        logger->Trace(
            "premature reading end.  Only read {} bytes, but still {} "
            "bytes to go...",
            bytes_read, bytes_to_read);
        return;
      }

      DWORD read_size = (DWORD)std::min(std::size(real_buffer),
                                        bytes_to_read - total_bytes);

      std::memcpy(buffer.data() + total_bytes, real_buffer, read_size);

      total_bytes += read_size;
    }


    logger->Trace(
        "bootstrap: {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} ... {:02X} "
        "{:02X} {:02X} {:02X} "
        "{:02X} {:02X}",
        (unsigned char)buffer[0], (unsigned char)buffer[1],
        (unsigned char)buffer[2], (unsigned char)buffer[3],
        (unsigned char)buffer[4], (unsigned char)buffer[5],
        (unsigned char)buffer[buffer.size() - 6],
        (unsigned char)buffer[buffer.size() - 5],
        (unsigned char)buffer[buffer.size() - 4],
        (unsigned char)buffer[buffer.size() - 3],
        (unsigned char)buffer[buffer.size() - 2],
        (unsigned char)buffer[buffer.size() - 1]);
  }

  std::optional<partition_layout> GetPartitionLayout(HANDLE device)
  {
    std::vector<char> buffer;
    buffer.resize(1024 * 1024);  // 1MiB should be more than enough

    for (;;) {
      DWORD bytes_written = 0;
      if (!DeviceIoControl(device, IOCTL_DISK_GET_DRIVE_LAYOUT_EX, nullptr, 0,
                           buffer.data(), (DWORD)buffer.size(), &bytes_written,
                           nullptr)) {
        auto err = GetLastError();
        if (err == ERROR_MORE_DATA) {
          buffer.resize(buffer.size() + (buffer.size() >> 1));
          continue;
        }

        std::cerr << libbareos::format("io control error = {}", err)
                  << std::endl;
        return std::nullopt;
      }

      break;
    }

    auto info = reinterpret_cast<DRIVE_LAYOUT_INFORMATION_EX*>(buffer.data());
    logger->Trace("partition count = {}", info->PartitionCount);

    partition_layout result = {};

    switch (info->PartitionStyle) {
      case PARTITION_STYLE_MBR: {
        partition_info_mbr mbr{
            std::bit_cast<std::uint32_t>(info->Mbr.CheckSum),
            std::bit_cast<std::uint32_t>(info->Mbr.Signature),
        };
        DWORD off_low = 0;
        LONG off_high = 0;
        SetFilePointer(device, off_low, &off_high, FILE_BEGIN);
        read_volume_file(device, mbr.bootstrap);

        result.info = mbr;

        logger->Trace("mbr partitioning");
      } break;
      case PARTITION_STYLE_RAW: {
        result.info = partition_info_raw{};
        logger->Trace("raw partitioning style => nothing to do");
      } break;
      case PARTITION_STYLE_GPT: {
        partition_info_gpt gpt{
            to_disk_format(info->Gpt.DiskId),
            std::bit_cast<std::uint64_t>(info->Gpt.StartingUsableOffset),
            std::bit_cast<std::uint64_t>(info->Gpt.UsableLength),
            std::bit_cast<std::uint32_t>(info->Gpt.MaxPartitionCount),
        };

        DWORD off_low = 0;
        LONG off_high = 0;
        SetFilePointer(device, off_low, &off_high, FILE_BEGIN);
        read_volume_file(device, gpt.bootstrap);

        result.info = gpt;
        logger->Trace("gpt partitioning");
      } break;
      default: {
        logger->Trace("unknown partitioning");
        return std::nullopt;
      } break;
    }

    partitioning(info);

    result.partition_infos.resize(info->PartitionCount);
    std::copy_n(info->PartitionEntry, info->PartitionCount,
                std::begin(result.partition_infos));

    return result;
  }

  void PrependFileHeader(insert_plan& plan,
                         const disk_map& map,
                         std::size_t total_size)
  {
    file_header header(map.size(), total_size);

    vec_writer writer;
    header.write(writer);
    plan.insert(plan.begin(), writer.get());
  }

  void WriteDiskHeader(insert_plan& plan,
                       const disk& Disk,
                       const DISK_GEOMETRY_EX& geo)
  {
    std::size_t total_extent_size = 0;
    for (auto& extent : Disk.extents) { total_extent_size += extent.length; }
    disk_header header(geo.DiskSize.QuadPart, total_extent_size,
                       geo.Geometry.MediaType, geo.Geometry.BytesPerSector,
                       Disk.extents.size());

    vec_writer writer;
    header.write(writer);
    plan.emplace_back(writer.get());
  }

  void WriteDiskPartTable(insert_plan& plan, const partition_layout& layout)
  {
    vec_writer writer;

    {
      auto header = std::visit(
          overloads{
              [](const partition_info_raw&) {
                part_table_header header(0, part_type::Raw, 0, 0, 0);
                return header;
              },
              [](const partition_info_mbr& mbr) {
                part_table_header header(0, part_type::Mbr, mbr.CheckSum,
                                         mbr.Signature, 0, {}, mbr.bootstrap);
                return header;
              },
              [](const partition_info_gpt& gpt) {
                static_assert(sizeof(gpt.DiskId) == 16);
                part_table_header header(
                    0, part_type::Gpt, gpt.MaxPartitionCount,
                    gpt.StartingUsableOffset, gpt.UsableLength,
                    std::span<const char>(
                        reinterpret_cast<const char*>(&gpt.DiskId),
                        sizeof(gpt.DiskId)),
                    gpt.bootstrap);
                return header;
              },
          },
          layout.info);
      header.partition_count = layout.partition_infos.size();

      header.write(writer);
    }


    for (auto& info : layout.partition_infos) {
      part_table_entry ent = from_win32(info);

      ent.write(writer);
      switch (info.PartitionStyle) {
        case PARTITION_STYLE_MBR: {
          part_table_entry_mbr_data data = from_win32(info.Mbr);

          data.write(writer);
        } break;
        case PARTITION_STYLE_GPT: {
          part_table_entry_gpt_data data = from_win32(info.Gpt);

          data.write(writer);
        } break;
        default: { /* intentionally left blank */
        } break;
      }
    }

    plan.emplace_back(writer.get());
  }

  void WriteDiskData(insert_plan& plan, const disk& disk_extents)
  {
    for (auto& extent : disk_extents.extents) {
      vec_writer writer;
      extent_header header = header_of(extent);

      header.write(writer);

      plan.emplace_back(writer.get());

      // logger->Trace( "copying extent ({}, {})", extent.handle_offset,
      //         extent.length);

      plan.emplace_back(
          insert_from{extent.hndl, extent.handle_offset, extent.length});
    }
  }

  // on linux we can get the disk_geometry with the HDIO_GETGEO ioctl
  std::optional<DISK_GEOMETRY_EX> GetDiskGeometry(HANDLE disk)
  {
    DISK_GEOMETRY_EX geo;
    DWORD bytes_written = 0;
    auto res = DeviceIoControl(disk, IOCTL_DISK_GET_DRIVE_GEOMETRY_EX, NULL, 0,
                               &geo, sizeof(geo), &bytes_written, NULL);

    if (res != TRUE) { return std::nullopt; }

    // if (sizeof(geo) != bytes_written) {
    //   logger->Trace( "drive geometry: bad read.  got {} bytes",
    //   bytes_written); return std::nullopt;
    // }

    return geo;
  }

  GenericLogger* logger;
  CComPtr<IVssBackupComponents> backup_components{};
  std::optional<VssSnapshot> snapshot;
  std::vector<HANDLE> open_handles;
};

dump_context* make_context(GenericLogger* logger)
{
  return new dump_context{logger};
}
void destroy_context(dump_context* ctx) { delete ctx; }

void dump_context_save_unknown_disks(dump_context* ctx, bool enable)
{
  ctx->save_unknown_disks = enable;
}
void dump_context_save_unknown_partitions(dump_context* ctx, bool enable)
{
  ctx->save_unknown_partitions = enable;
}
void dump_context_save_unknown_extents(dump_context* ctx, bool enable)
{
  ctx->save_unknown_extents = enable;
}
void dump_context_ignore_all_data(dump_context* ctx, bool ignore_data)
{
  ctx->ignore_data = ignore_data;
}
void dump_context_ignore_disk(dump_context* ctx, std::size_t id)
{
  ctx->ignored_disk_ids.emplace(id);
}
insert_plan dump_context_create_plan(dump_context* ctx)
{
  return ctx->create();
}

struct data_dumper {
  data_dumper(GenericLogger* logger_, insert_plan&& plan_)
      : logger{logger_}, plan{std::move(plan_)}, reader{1 << 20, logger_}
  {
  }

  bool Done() const { return current_index >= plan.size(); }

  std::size_t fill_bytes(const insert_bytes& bytes,
                         std::size_t offset,
                         std::span<char> buffer)
  {
    if (offset == 0) {
      // logger->SetStatus("inserting meta data");
      // logger->Trace("writing {} bytes", bytes.size());
    }

    auto bytes_left = bytes.size() - offset;
    auto bytes_to_write = std::min(bytes_left, buffer.size());
    std::memcpy(buffer.data(), bytes.data(), bytes_to_write);

    logger->Progressed(bytes_to_write);

    return bytes_to_write;
  }

  std::size_t fill_bytes(const insert_from& from,
                         std::size_t offset,
                         std::span<char> buffer)
  {
    if (offset == 0) {
      // logger->SetStatus("inserting from file");
      // logger->Trace("inserting {} bytes", from.length);
    }

    auto bytes_left = from.length - offset;
    auto bytes_to_write = std::min(bytes_left, buffer.size());

    if (bytes_to_write == 0) {
      // logger->Trace("bytes_to_write = 0!!! {} {} {} {}", bytes_left,
      //               buffer.size(), offset, current_offset);
    }

    return reader.do_fill(from.hndl, from.offset + offset,
                          buffer.subspan(0, bytes_to_write));
  }

  std::size_t Write(std::span<char> buffer)
  {
    if (current_index == 0 && current_offset == 0) {
      logger->Begin(compute_plan_size(plan));
    }

    bool wrote_bytes = false;

    auto bytes_written = 0;
    while (bytes_written < buffer.size() && current_index < plan.size()) {
      auto& current_step = plan[current_index];
      auto to_write = buffer.subspan(bytes_written);
      auto write_result = std::visit(
          [this, to_write](auto& elem) {
            return fill_bytes(elem, current_offset, to_write);
          },
          current_step);

      logger->Progressed(write_result);

      bytes_written += write_result;
      if (write_result == 0) {
        // logger->Trace("write done");
        current_index += 1;
        current_offset = 0;
      } else {
        current_offset += write_result;
      }
    }

    if (current_index == plan.size()) {
      current_index = std::numeric_limits<decltype(current_index)>::max();
      logger->End();
    }
    return bytes_written;
  }

  GenericLogger* logger{nullptr};

  std::size_t current_offset{};
  std::size_t current_index{};
  insert_plan plan;
  disk_reader reader;
};

data_dumper* dumper_setup(GenericLogger* logger, insert_plan&& plan)
{
  auto* dumper = new data_dumper{logger, std::move(plan)};
  return dumper;
}

std::size_t dumper_write(data_dumper* dumper, std::span<char> data)
{
  if (dumper->Done()) { return 0; }
  return dumper->Write(data);
}

void dumper_stop(data_dumper* dumper) { delete dumper; }
