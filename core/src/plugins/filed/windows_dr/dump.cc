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

#if 0
  void read(HANDLE hndl,
            std::size_t offset,
            std::size_t length,
            char* result_buffer)
  {
    DWORD ignore_offset = 0;
    if (hndl != current_handle || offset != current_offset) {
      // the offset needs to be sector aligned.  We need to round _down_ here
      // to make sure that we read everything

      std::size_t rounded_offset = (offset / sector_size) * sector_size;

      ignore_offset = offset - rounded_offset;
      // fprintf(stderr, "offset: %zu => ignore = %d\n", offset, ignore_offset);

      DWORD off_low = rounded_offset & 0xFFFFFFFF;
      LONG off_high = (rounded_offset >> 32) & 0xFFFFFFFF;
      SetFilePointer(hndl, off_low, &off_high, FILE_BEGIN);
      current_offset = offset;
      current_handle = hndl;
    }

    if (ignore_offset) {
      fprintf(stderr, "%zu/%zu => %d\n", offset, sector_size, ignore_offset);
    }

    std::size_t bytes_to_read = length + ignore_offset;
    while (bytes_to_read > 0) {
      // fprintf(stderr, "%s\n", fmt::format("to read: {} (offset: {})",
      // bytes_to_read, offset).c_str());
      DWORD bytes_read = 0;

      DWORD buffer_size = std::min(capacity, bytes_to_read);
      // buffer_size, as computed, may not be divisible by the sector size
      // since we are working with disk handles, we can only read in pages.
      // as such we need to "round up" this value, and overread a bit.
      // we then just simply only copy part of the buffer into the
      // result_buffer.

      DWORD rounded_buffer_size
          = (buffer_size + sector_size - 1) / sector_size * sector_size;

      DWORD diff = rounded_buffer_size - buffer_size;
      if (diff) {
        fprintf(stderr, "%zu/%zu => %d\n", bytes_to_read, sector_size, diff);
      }

      buffer_size = rounded_buffer_size;

      // as the buffer is always sector sized, this should always hold true
      assert(buffer_size <= capacity);

      // fprintf(stderr, "%s\n", fmt::format("buffer size: {}",
      // buffer_size).c_str());

      if (!ReadFile(hndl, buffer.get(), buffer_size, &bytes_read, NULL)) {
        fprintf(stderr, "oh oh error: %d\n", GetLastError());
        throw win_error("ReadFile", GetLastError());
      }

      if (bytes_read == 0) {
        {
          LARGE_INTEGER dist = {};
          dist.QuadPart = 0;
          LARGE_INTEGER new_pos = {};
          if (!SetFilePointerEx(hndl, dist, &new_pos, FILE_CURRENT)) {
            fprintf(stderr, "coud not determine size: Err=%d\n",
                    GetLastError());
          } else {
            fprintf(stderr, "Reading (%llu, %llu) of %p.  Current = %llu\n",
                    offset, length, hndl, new_pos.QuadPart);
          }
        }

        fprintf(stderr,
                "premature reading end (read 0).  Still %llu bytes to go...\n",
                bytes_to_read);
        return;
      }

      std::size_t actual_bytes_read = bytes_read;
      current_offset += bytes_read;

      // fprintf(stderr, "%s\n", fmt::format("read: {}", bytes_read).c_str());

      // make sure to ignore the first ignore_offset bytes
      if (ignore_offset > 0) {
        auto bytes_ignored = std::min(bytes_read, ignore_offset);
        bytes_read -= bytes_ignored;
        ignore_offset -= bytes_ignored;
        bytes_to_read -= bytes_ignored;
      }


      if (bytes_read != 0) {
        if (bytes_read > bytes_to_read) { bytes_read = bytes_to_read; }
        // bytes_read = std::min(bytes_read, bytes_to_read);
        std::memcpy(result_buffer, buffer.get(), bytes_read);
        result_buffer += bytes_read;
        bytes_to_read -= bytes_read;
      }

      std::memset(buffer.get(), 0, actual_bytes_read);
    }
  }
#endif

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

#if 0
    // TODO: this should be done per disk, so that we can use the optimal
    // disk access in here
    std::vector<char> desc_buf;
    desc_buf.resize( sizeof(STORAGE_ACCESS_ALIGNMENT_DESCRIPTOR) );

    std::size_t new_align = 4096;
    for (;;) {
      DWORD bytes_returned;
      auto success = DeviceIoControl(hndl,
                                     IOCTL_STORAGE_QUERY_PROPERTY,
                                     &query,
                                     sizeof(query),
                                     desc_buf.data(),
                                     desc_buf.size(),
                                     &bytes_returned,
                                     NULL);

      if (success) {
        if (bytes_returned >= sizeof(STORAGE_ACCESS_ALIGNMENT_DESCRIPTOR)) {
          STORAGE_ACCESS_ALIGNMENT_DESCRIPTOR desc;
          std::memcpy(&desc, desc_buf.data(), sizeof(desc));

          new_align = desc.BytesPerPhysicalSector;
          fprintf(stderr, "phys sector size: %zu\n", new_align);
        } else {
          fprintf(stderr, "bad size for query storage alignment\n");
        }
      } else {
        auto err = GetLastError();

        if (err == ERROR_MORE_DATA) {
          desc_buf.resize(desc_buf.size() * 2);
          continue;
        } else {
          fprintf(stderr, "could not query storage alignment\n");
        }
      }
      break;
    }
#endif

    {
      GET_LENGTH_INFORMATION length_info = {0};
      DWORD bytes_returned = 0;

      if (DeviceIoControl(hndl, IOCTL_DISK_GET_LENGTH_INFO, NULL, 0,
                          &length_info, sizeof(length_info), &bytes_returned,
                          NULL)) {
        disk_size = length_info.Length.QuadPart;
        logger->Trace(
            [&] { return libbareos::format("disk size = {}", disk_size); });
      } else {
        disk_size = 0;
        logger->Info(libbareos::format("could not determine disk size"));
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

      logger->Trace([&] {
        return libbareos::format("current offset = {} (wanted: {})\n",
                                 current_offset, offset);
      });

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

    // fprintf(stderr, "wanted: %p, %zu, cached: %p, %zu-%zu (%zu)\n",
    //         hndl, offset, current_handle, current_offset, current_offset +
    //         size, size);
  }

  std::size_t do_fill(HANDLE hndl, std::size_t offset, std::span<char> output)
  {
    // fprintf(stderr, "fill %p %zu\n", hndl, offset);
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

          logger->Info(
              libbareos::format("no data after refresh offset={} ({} + {}))!!!",
                                offset + bytes_written, offset, bytes_written));
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
    fprintf(stderr, "Volume '%ls'\n", volumes.back().c_str());

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
  static VssSnapshot create(IVssBackupComponents* vss,
                            std::span<const std::wstring> volumes)
  {
    wchar_t guid_storage[64] = {};
    GUID snapshot_guid = GUID_NULL;

    COM_CALL(vss->StartSnapshotSet(&snapshot_guid));

    StringFromGUID2(snapshot_guid, guid_storage, sizeof(guid_storage));
    fprintf(stderr, "snapshot set => %ls\n", guid_storage);

    BackupAborter aborter{vss};

    for (auto& vol : volumes) {
      VSS_ID volume_id = {};
      vss->AddToSnapshotSet(const_cast<VSS_PWSZ>(vol.c_str()), GUID_NULL,
                            &volume_id);

      StringFromGUID2(volume_id, guid_storage, sizeof(guid_storage));
      fprintf(stderr, "%ls => %ls\n", vol.c_str(), guid_storage);
    }

    CComPtr<IVssAsync> prepare_job;
    COM_CALL(vss->PrepareForBackup(&prepare_job));
    WaitOnJob(prepare_job);

    CComPtr<IVssAsync> snapshot_job;
    COM_CALL(vss->DoSnapshotSet(&snapshot_job));
    WaitOnJob(snapshot_job);

    aborter.backup_components = nullptr;  // not needed anymore

    return {snapshot_guid};
  }

  GUID snapshot_guid;

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
      fprintf(stderr, "found guid => %ls\n", guid_storage);

      if (prop.Obj.Snap.m_SnapshotSetId == snapshot_guid) {
        fprintf(stderr, "%ls => %ls\n", prop.Obj.Snap.m_pwszOriginalVolumeName,
                prop.Obj.Snap.m_pwszSnapshotDeviceObject);

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

    COM_CALL(vss->DeleteSnapshots(snapshot_guid, VSS_OBJECT_SNAPSHOT_SET, FALSE,
                                  &deleted_count, &bad_snapshot));
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

  fprintf(stderr, "\n%p => sectors: %zu, clusters: %zu\n\n", volume,
          (size_t)buffer.NumberSectors.QuadPart,
          (size_t)buffer.TotalClusters.QuadPart);
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
    if (!DeviceIoControl(disk, FSCTL_GET_VOLUME_BITMAP, &Start, sizeof(Start),
                         buffer.data(), buffer.size(), &bytes_written,
                         nullptr)) {
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

  std::cerr << libbareos::format(":::::: cluster size {}", cluster_size)
            << std::endl;


  std::vector<char> extent_buffer;
  extent_buffer.resize(1024 * 1024);

  DWORD bytes_written = 0;
  for (;;) {
    if (!DeviceIoControl(volume, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS, nullptr,
                         0, extent_buffer.data(), extent_buffer.size(),
                         &bytes_written, nullptr)) {
      auto err = GetLastError();

      if (err != ERROR_MORE_DATA) {
        std::cerr << libbareos::format("DISK_EXTENTS returned {} for volume {}",
                                       err, volume)
                  << std::endl;
        return;
      }

      extent_buffer.resize(extent_buffer.size() * 2);
    }

    break;
  }

  if (bytes_written == 0) {
    // no extents
    std::cerr << libbareos::format("volume {} has no extents ...", volume)
              << std::endl;
    return;
  }
  auto extents = reinterpret_cast<VOLUME_DISK_EXTENTS*>(extent_buffer.data());

  std::vector<char> bitmap_buffer;

  std::size_t volume_offset = 0;
  std::vector<used_interval> used;
  for (size_t i = 0; i < extents->NumberOfDiskExtents; ++i) {
    auto& extent = extents->Extents[i];
    std::cerr << libbareos::format(
        "volume {} extent {} @ {} -> {}", volume, extent.DiskNumber,
        extent.StartingOffset.QuadPart, extent.ExtentLength.QuadPart)
              << std::endl;

    auto& disk = disks[extent.DiskNumber];

    auto bits = [&]() -> std::optional<used_bitmap> {
      if (cluster_size == 0) { return std::nullopt; }

      return GetBitmap(bitmap_buffer, data_volume, volume_offset,
                       extent.ExtentLength.QuadPart, cluster_size);
    }();

    if (bits) {
      static constexpr std::size_t min_hole_size = 128 << 10;  // 1 mb

      used.clear();
      find_used_data(used, bits.value(), volume_offset,
                     volume_offset + extent.ExtentLength.QuadPart,
                     min_hole_size);

      for (auto [start, length] : used) {
        assert(start >= volume_offset);
        auto disk_offset
            = extent.StartingOffset.QuadPart + (start - volume_offset);

        disk.extents.push_back({disk_offset, start, length, data_volume});
      }
    } else {
      disk.extents.push_back(
          {(size_t)extent.StartingOffset.QuadPart, volume_offset,
           (size_t)extent.ExtentLength.QuadPart, data_volume});
    }

    volume_offset += (size_t)extent.ExtentLength.QuadPart;
  }
}

std::optional<partition_layout> partitioning(DRIVE_LAYOUT_INFORMATION_EX* info)
{
  for (DWORD i = 0; i < info->PartitionCount; ++i) {
    auto& entry = info->PartitionEntry[i];

    fprintf(stderr, "  partition %d\n", i);

    switch (entry.PartitionStyle) {
      case PARTITION_STYLE_MBR: {
        fprintf(stderr, "    style: mbr\n");
        auto& mbr = entry.Mbr;
        fprintf(stderr, "      type: %d\n", mbr.PartitionType);
        fprintf(stderr, "      boot?: %s\n", mbr.BootIndicator ? "yes" : "no");
        fprintf(stderr, "      recognized?: %s\n",
                mbr.RecognizedPartition ? "yes" : "no");
        fprintf(stderr, "      hidden sectors: %d\n", mbr.HiddenSectors);

        wchar_t guid_storage[64] = {};

        StringFromGUID2(mbr.PartitionId, guid_storage, sizeof(guid_storage));
        fprintf(stderr, "      partition id: %ls\n", guid_storage);
      } break;
      case PARTITION_STYLE_RAW: {
        fprintf(stderr, "    style: raw\n");
      } break;
      case PARTITION_STYLE_GPT: {
        fprintf(stderr, "    style: gpt\n");
        auto& gpt = entry.Gpt;
        wchar_t guid_storage[64] = {};

        StringFromGUID2(gpt.PartitionType, guid_storage, sizeof(guid_storage));
        fprintf(stderr, "      type: %ls\n", guid_storage);
        StringFromGUID2(gpt.PartitionId, guid_storage, sizeof(guid_storage));
        fprintf(stderr, "      partition id: %ls\n", guid_storage);
        fprintf(stderr, "      attributes: %lld\n", gpt.Attributes);
        fprintf(stderr, "      name: %.*ls\n", (int)sizeof(gpt.Name), gpt.Name);
      } break;
      default: {
        fprintf(stderr, "    style: unknown\n");
      } break;
    }

    fprintf(stderr, "    offset: %lld\n", entry.StartingOffset.QuadPart);
    fprintf(stderr, "    length: %lld\n", entry.PartitionLength.QuadPart);
    fprintf(stderr, "    number: %d\n", entry.PartitionNumber);
    fprintf(stderr, "    rewrite?: %s\n",
            entry.RewritePartition ? "yes" : "no");
    // fprintf(stderr, "    service?: %s\n",
    //         entry.IsServicePartition ? "yes" : "no");
  }
  return std::nullopt;
}

void read_volume_file(HANDLE hndl, std::span<char> buffer)
{
  fprintf(stderr, "reading bootstrap (size = %llu)\n", buffer.size());

  alignas(4096) char real_buffer[4096];

  std::size_t bytes_to_read = buffer.size();
  std::size_t total_bytes = 0;
  while (total_bytes < bytes_to_read) {
    DWORD bytes_read = 0;

    if (!ReadFile(hndl, real_buffer, (DWORD)std::size(real_buffer), &bytes_read,
                  NULL)) {
      auto err = GetLastError();
      fprintf(stderr, "could not read from %p: Err=%d\n", hndl, err);

      throw win_error("ReadFile", err);
    }

    if (bytes_read != std::size(real_buffer)) {
      fprintf(stderr,
              "premature reading end.  Only read %d bytes, but still %llu "
              "bytes to go...\n",
              bytes_read, bytes_to_read);
      return;
    }

    DWORD read_size
        = (DWORD)std::min(std::size(real_buffer), bytes_to_read - total_bytes);

    std::memcpy(buffer.data() + total_bytes, real_buffer, read_size);

    total_bytes += read_size;
  }


  fprintf(stderr,
          "bootstrap: %02X %02X %02X %02X %02X %02X ... %02X %02X %02X %02X "
          "%02X %02X\n",
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

      std::cerr << libbareos::format("io control error = {}", err) << std::endl;
      return std::nullopt;
    }

    break;
  }

  auto info = reinterpret_cast<DRIVE_LAYOUT_INFORMATION_EX*>(buffer.data());
  fprintf(stderr, "partition count = %d\n", info->PartitionCount);

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

      fprintf(stderr, "mbr partitioning\n");
    } break;
    case PARTITION_STYLE_RAW: {
      result.info = partition_info_raw{};
      fprintf(stderr, "raw partitioning style => nothing to do\n");
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
      fprintf(stderr, "gpt partitioning\n");
    } break;
    default: {
      fprintf(stderr, "unknown partitioning\n");
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

    fprintf(stderr, "copying extent (%llu, %llu)\n", extent.handle_offset,
            extent.length);

    plan.emplace_back(
        insert_from{extent.hndl, extent.handle_offset, extent.length});
  }
}

// we want to make sure that every partition is covered by some extent
// of a volume.  Because we can only read from (shadow copies) of volumes,
// and not from the devices directly.  If some part of a partition is not
// covered by an extent of a volume, then that part of the partition
// cannot be read!

struct partition_cover {
  std::vector<partition_extent> extents;
};

std::optional<std::vector<partition_cover>> CrossCheckPartitionsAndExtents(
    const partition_layout& layout,
    const std::vector<partition_extent> extents)
{
  // for now we assume that every partition corresponds exactly to one extent.

  std::vector<partition_cover> covers;

  for (auto& info : layout.partition_infos) {
    auto offset = info.StartingOffset.QuadPart;
    auto length = info.PartitionLength.QuadPart;

    if (info.PartitionStyle == PARTITION_STYLE_MBR
        && info.Mbr.PartitionType == PARTITION_ENTRY_UNUSED) {
      continue;
    }
    if (length == 0) { continue; }

    auto& cover = covers.emplace_back();

    bool found = false;
    for (auto extent : extents) {
      if (extent.partition_offset == offset && extent.length == length) {
        found = true;
        cover.extents.push_back(extent);
        break;
      }
    }

    std::sort(std::begin(cover.extents), std::end(cover.extents),
              [](auto& l, auto& r) {
                return l.partition_offset < r.partition_offset;
              });

    if (!found) {
      fprintf(stderr,
              "could not find extent covering partition %d, (%llu, %llu)\n",
              info.PartitionNumber, offset, offset + length);
      return std::nullopt;
    }
  }

  std::sort(std::begin(covers), std::end(covers), [](auto& l, auto& r) {
    // empty collections are always the smallest, so an empty r
    // can never be smaller than l
    if (r.extents.size() == 0) { return false; }
    // if r is not empty, then an empty l is always smaller
    if (l.extents.size() == 0) { return true; }

    return l.extents[0].partition_offset < r.extents[0].partition_offset;
  });

  return covers;
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
  //   fprintf(stderr, "drive geometry: bad read.  got %d bytes\n",
  //   bytes_written); return std::nullopt;
  // }

  return geo;
}

struct dump_context {
  GenericLogger* logger;

  CComPtr<IVssBackupComponents> backup_components{};
  std::optional<VssSnapshot> snapshot;
  std::vector<HANDLE> open_handles;

  ~dump_context()
  {
    // we need to away to always delete these shadow copies
    // currently you can remove orphaned shadow copies via
    // diskshadow > delete shadows all
    for (auto hndl : open_handles) { CloseHandle(hndl); }
    if (snapshot) { snapshot->delete_snapshot(backup_components); }
  }

 public:
  insert_plan create(bool dry)
  {
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
        fprintf(stderr, "=== SYSTEM WRITER DETECTED ===\n");
      }
      if (identity.writer_id == ASR_WRITER_ID) {
        fprintf(stderr, "=== ASR WRITER DETECTED ===\n");
      }

      fprintf(stderr, "%ls\n", (BSTR)identity.name);
      {
        wchar_t guid_storage[64] = {};

        StringFromGUID2(identity.instance_id, guid_storage,
                        sizeof(guid_storage));
        fprintf(stderr, "  Instance Id: %ls\n", guid_storage);
        StringFromGUID2(identity.writer_id, guid_storage, sizeof(guid_storage));
        fprintf(stderr, "  Writer Id: %ls\n", guid_storage);
        fprintf(stderr, "  Usage Type: %s (%u)\n",
                vss_usage_as_str(identity.usage_type), identity.usage_type);
        fprintf(stderr, "  Source Type: %s (%u)\n",
                vss_source_as_str(identity.source_type), identity.source_type);
      }

      COM_CALL(metadata->GetFileCounts(&counts.include, &counts.exclude,
                                       &counts.component));

      fprintf(stderr, "  %u Includes:\n", counts.include);
      for (UINT i = 0; i < counts.include; ++i) {
        CComPtr<IVssWMFiledesc> file;
        COM_CALL(metadata->GetIncludeFile(i, &file));

        CComBSTR path;
        COM_CALL(file->GetPath(&path));

        fprintf(stderr, "    %ls\n", (BSTR)path);
      }

      fprintf(stderr, "  %u Excludes:\n", counts.exclude);
      for (UINT i = 0; i < counts.exclude; ++i) {
        CComPtr<IVssWMFiledesc> file;
        COM_CALL(metadata->GetExcludeFile(i, &file));
        CComBSTR path;
        COM_CALL(file->GetPath(&path));

        fprintf(stderr, "    %ls\n", (BSTR)path);
      }

      fprintf(stderr, "  %u Components:\n", counts.component);
      for (UINT i = 0; i < counts.component; ++i) {
        CComPtr<IVssWMComponent> component;
        COM_CALL(metadata->GetComponent(i, &component));

        PVSSCOMPONENTINFO info{nullptr};
        COM_CALL(component->GetComponentInfo(&info));
        fprintf(stderr, "    %u:\n", i);
        fprintf(stderr, "      path: %ls\n", info->bstrLogicalPath);
        fprintf(stderr, "      name: %ls\n", info->bstrComponentName);
        fprintf(stderr, "      caption: %ls\n", info->bstrCaption);

        fprintf(stderr, "      files: %u\n", info->cFileCount);
        for (UINT fileidx = 0; fileidx < info->cFileCount; ++fileidx) {
          fprintf(stderr, "        %u:\n", fileidx);
          CComPtr<IVssWMFiledesc> file;
          COM_CALL(component->GetFile(fileidx, &file));

          CComBSTR path;
          COM_CALL(file->GetPath(&path));
          CComBSTR spec;
          COM_CALL(file->GetFilespec(&spec));
          bool recursive;
          COM_CALL(file->GetRecursive(&recursive));

          fprintf(stderr, "          path: %ls\n", (BSTR)path);
          fprintf(stderr, "          spec: %ls\n", (BSTR)spec);
          fprintf(stderr, "          recursive: %s\n",
                  recursive ? "true" : "false");
        }

        fprintf(stderr, "      databases: %u\n", info->cDatabases);
        for (UINT dbidx = 0; dbidx < info->cDatabases; ++dbidx) {
          fprintf(stderr, "        %u:\n", dbidx);
          CComPtr<IVssWMFiledesc> file;
          COM_CALL(component->GetDatabaseFile(dbidx, &file));

          CComBSTR path;
          COM_CALL(file->GetPath(&path));
          CComBSTR spec;
          COM_CALL(file->GetFilespec(&spec));
          bool recursive;
          COM_CALL(file->GetRecursive(&recursive));

          fprintf(stderr, "          path: %ls\n", (BSTR)path);
          fprintf(stderr, "          spec: %ls\n", (BSTR)spec);
          fprintf(stderr, "          recursive: %s\n",
                  recursive ? "true" : "false");
        }

        fprintf(stderr, "      logs: %u\n", info->cLogFiles);
        for (UINT logidx = 0; logidx < info->cLogFiles; ++logidx) {
          fprintf(stderr, "        %u:\n", logidx);
          CComPtr<IVssWMFiledesc> file;
          COM_CALL(component->GetDatabaseLogFile(logidx, &file));

          CComBSTR path;
          COM_CALL(file->GetPath(&path));
          CComBSTR spec;
          COM_CALL(file->GetFilespec(&spec));
          bool recursive;
          COM_CALL(file->GetRecursive(&recursive));

          fprintf(stderr, "          path: %ls\n", (BSTR)path);
          fprintf(stderr, "          spec: %ls\n", (BSTR)spec);
          fprintf(stderr, "          recursive: %s\n",
                  recursive ? "true" : "false");
        }

        fprintf(stderr, "      dependencies: %u\n", info->cDependencies);
        for (UINT depidx = 0; depidx < info->cDependencies; ++depidx) {
          fprintf(stderr, "        %u:\n", depidx);
          CComPtr<IVssWMDependency> dep;
          COM_CALL(component->GetDependency(depidx, &dep));

          CComBSTR name;
          COM_CALL(dep->GetComponentName(&name));
          CComBSTR path;
          COM_CALL(dep->GetLogicalPath(&path));
          VSS_ID writer_id;
          COM_CALL(dep->GetWriterId(&writer_id));

          fprintf(stderr, "          name: %ls\n", (BSTR)name);
          fprintf(stderr, "          path: %ls\n", (BSTR)path);

          wchar_t guid_storage[64] = {};
          StringFromGUID2(writer_id, guid_storage, sizeof(guid_storage));
          fprintf(stderr, "          writer: %ls\n", guid_storage);
        }

        COM_CALL(component->FreeComponentInfo(info));
      }
    }

    auto volumes = list_volumes();

    snapshot.emplace(VssSnapshot::create(backup_components, volumes));

    auto paths = snapshot->snapshotted_paths(backup_components);

    disk_map candidate_disks;
    for (auto& [path, copy] : paths) {
      std::wstring cpath = path;
      if (cpath.back() == L'\\') { cpath.pop_back(); }

      HANDLE volume
          = CreateFileW(cpath.c_str(), 0,
                        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                        NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);

      if (volume == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "volume %ls (%ls) -> could not volume\n", path.c_str(),
                copy.c_str());
        throw win_error("CreateFileW", GetLastError());
      }

      fprintf(stderr, "volume %ls -> %p\n", cpath.c_str(), volume);

      HANDLE shadow = CreateFileW(
          copy.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_ALWAYS,
          FILE_ATTRIBUTE_NORMAL | FILE_FLAG_BACKUP_SEMANTICS
              | FILE_FLAG_SEQUENTIAL_SCAN,
          NULL);

      if (shadow == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "volume %ls (%ls) -> could not shadow copy\n",
                path.c_str(), copy.c_str());
        CloseHandle(volume);
        throw win_error("CreateFileW", GetLastError());
      }
      {
        DWORD bytes_returned;
        if (!DeviceIoControl(shadow,                        // handle to file
                             FSCTL_ALLOW_EXTENDED_DASD_IO,  // dwIoControlCode
                             NULL,                          // lpInBuffer
                             0,                             // nInBufferSize
                             NULL,                          // lpOutBuffer
                             0,                             // nOutBufferSize
                             &bytes_returned, NULL)) {
          fprintf(stderr,
                  "---- could not enable extended access (Err=%d) ----\n",
                  GetLastError());
        } else {
          fprintf(stderr, "---- extended access enabled ----\n");
        }
      }

      fprintf(stderr, "shadow %ls -> %p\n", copy.c_str(), shadow);

      // the operating system will clean up shadow on exit (yes, we leak it)
      GetVolumeExtents(candidate_disks, volume, shadow);
      open_handles.push_back(shadow);

      CloseHandle(volume);
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
      std::wstring disk_path
          = std::wstring(L"\\\\.\\PhysicalDrive") + std::to_wstring(id);

      HANDLE hndl = CreateFileW(
          disk_path.c_str(), GENERIC_READ,
          FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
          OPEN_EXISTING, FILE_ATTRIBUTE_READONLY | FILE_FLAG_BACKUP_SEMANTICS,
          NULL);

      if (hndl == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "could not open %ls\n", disk_path.c_str());
        continue;
      }

      auto geo = GetDiskGeometry(hndl);
      if (!geo) {
        CloseHandle(hndl);
        continue;
      }

      if (geo->Geometry.MediaType != FixedMedia) {
        fprintf(stderr, "disk %zu has bad media type (%d); skipping...\n", id,
                geo->Geometry.MediaType);
        continue;
      }

      disks[id] = std::move(disk);
      disk_info[id] = open_disk{hndl, geo.value()};
      open_handles.push_back(hndl);
    }

    insert_plan plan;
    for (auto& [id, disk] : disks) {
      fprintf(stderr, "disk %zu extents\n", id);

      std::sort(std::begin(disk.extents), std::end(disk.extents),
                [](auto& l, auto& r) {
                  return l.partition_offset < r.partition_offset;
                });

      std::size_t total = 0;
      for (auto& extent : disk.extents) {
        total += extent.length;
        fprintf(stderr, "  %zu -> %zu\n", extent.partition_offset,
                extent.partition_offset + extent.length);
      }
      fprintf(stderr, " => total = %zu\n", total);

      auto [hndl, geo] = disk_info[id];

      auto layout = GetPartitionLayout(hndl);
      if (!layout) { continue; }

      auto disk_extents
          = CrossCheckPartitionsAndExtents(layout.value(), disk.extents);
      if (!disk_extents) {
        // continue;
      }

      // for (auto& cover : disk_extents.value()) {
      //   fprintf(stderr, " --- START ---\n");
      //   for (auto& extent : cover) {
      //     fprintf(stderr, "p.offset = %zu, h.offset = %zu, length = %zu\n"
      //             extent.partition_offset, extent.handle_offset,
      //             extent.length
      //            );
      //   }
      //   fprintf(stderr, " --- END ---\n");
      // }

      fprintf(stderr,
              "disk geometry:\n"
              " - Size: %llu\n"
              " - Cylinders: %llu\n"
              " - Tracks/C: %lu\n"
              " - Sectors/T: %lu\n"
              " - Bytes/S: %lu\n",
              geo.DiskSize.QuadPart, geo.Geometry.Cylinders.QuadPart,
              geo.Geometry.TracksPerCylinder, geo.Geometry.SectorsPerTrack,
              geo.Geometry.BytesPerSector);

      if (dry) { disk.extents.clear(); }

      WriteDiskHeader(plan, disk, geo);
      WriteDiskPartTable(plan, layout.value());
      WriteDiskData(plan, disk);
    }
    std::size_t payload_size = compute_plan_size(plan);
    PrependFileHeader(plan, disks, payload_size);
    return plan;
  }

 private:
};

dump_context* make_context(GenericLogger* logger)
{
  return new dump_context{logger};
}
void destroy_context(dump_context* ctx) { delete ctx; }

insert_plan create_insert_plan(dump_context* ctx, bool dry)
{
  return ctx->create(dry);
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
      logger->SetStatus("inserting meta data");
      logger->Trace(
          [&] { return libbareos::format("writing {} bytes", bytes.size()); });
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
      logger->SetStatus("inserting from file");
      logger->Trace(
          [&] { return libbareos::format("inserting {} bytes", from.length); });
    }

    auto bytes_left = from.length - offset;
    auto bytes_to_write = std::min(bytes_left, buffer.size());

    if (bytes_to_write == 0) {
      logger->Trace([&] {
        return libbareos::format("bytes_to_write = 0!!! {} {} {} {}",
                                 bytes_left, buffer.size(), offset,
                                 current_offset);
      });
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
        logger->Trace([&] { return "write done"; });
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
