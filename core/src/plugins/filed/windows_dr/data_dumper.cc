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
#include <span>
#include <unordered_map>
#include <variant>
#include <bit>
#include <cassert>
#include <algorithm>

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

struct win_error : std::exception {
  win_error(const char*, DWORD) {}

  const char* what() const { return ""; }
};

struct com_error : std::exception {
  com_error(const char* where, HRESULT) {}

  const char* what() const { return ""; }
};

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
  static VssSnapshot create(CComPtr<IVssBackupComponents> vss,
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

    CComPtr<IVssBackupComponents>& backup_components;
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

struct mbr_header {};
struct mbr_entry {};

struct gpt_header {};
struct gpt_entry {};

struct partition_info_raw {};
struct partition_info_mbr {
  uint32_t CheckSum;
  uint32_t Signature;
};
struct partition_info_gpt {
  GUID DiskId;
  uint64_t StartingUsableOffset;
  uint64_t UsableLength;
  uint32_t MaxPartitionCount;
};

using partition_info
    = std::variant<partition_info_raw, partition_info_mbr, partition_info_gpt>;

struct partition_layout {
  partition_info info;
  std::vector<PARTITION_INFORMATION_EX> partition_infos;
};

struct partition_extent {
  std::size_t partition_offset;
  std::size_t handle_offset;
  std::size_t length;

  HANDLE hndl;
};

struct disk {
  std::vector<partition_extent> extents;
};

using disk_map = std::unordered_map<std::size_t, disk>;

void GetVolumeExtents(disk_map& disks, HANDLE volume, HANDLE data_volume)
{
  std::vector<char> buffer;
  buffer.resize(1024 * 1024);

  DWORD bytes_written = 0;
  for (;;) {
    if (!DeviceIoControl(volume, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS, nullptr,
                         0, buffer.data(), buffer.size(), &bytes_written,
                         nullptr)) {
      auto err = GetLastError();

      if (err != ERROR_MORE_DATA) {
        std::cerr << std::format("DISK_EXTENTS returned {} for volume {}", err,
                                 volume)
                  << std::endl;
        return;
      }

      buffer.resize(buffer.size() * 2);
    }

    break;
  }

  if (bytes_written == 0) {
    // no extents
    std::cerr << std::format("volume {} has no extents ...", volume)
              << std::endl;
    return;
  }

  auto extents = reinterpret_cast<VOLUME_DISK_EXTENTS*>(buffer.data());

  std::size_t volume_offset = 0;
  for (size_t i = 0; i < extents->NumberOfDiskExtents; ++i) {
    auto& extent = extents->Extents[i];

    std::cerr << std::format("volume {} extent {} @ {} -> {}", volume,
                             extent.DiskNumber, extent.StartingOffset.QuadPart,
                             extent.ExtentLength.QuadPart)
              << std::endl;

    auto& disk = disks[extent.DiskNumber];
    disk.extents.push_back({(size_t)extent.StartingOffset.QuadPart,
                            volume_offset, (size_t)extent.ExtentLength.QuadPart,
                            data_volume});

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
    fprintf(stderr, "    service?: %s\n",
            entry.IsServicePartition ? "yes" : "no");
  }
  return std::nullopt;
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

      std::cerr << std::format("io control error = {}", err) << std::endl;
      return std::nullopt;
    }

    break;
  }

  auto info = reinterpret_cast<DRIVE_LAYOUT_INFORMATION_EX*>(buffer.data());
  fprintf(stderr, "partition count = %d\n", info->PartitionCount);

  partition_layout result = {};

  switch (info->PartitionStyle) {
    case PARTITION_STYLE_MBR: {
      result.info = partition_info_mbr{
          std::bit_cast<std::uint32_t>(info->Mbr.CheckSum),
          std::bit_cast<std::uint32_t>(info->Mbr.Signature),
      };
      fprintf(stderr, "mbr partitioning\n");
    } break;
    case PARTITION_STYLE_RAW: {
      result.info = partition_info_raw{};
      fprintf(stderr, "raw partitioning style => nothing to do\n");
    } break;
    case PARTITION_STYLE_GPT: {
      result.info = partition_info_gpt{
          info->Gpt.DiskId,
          std::bit_cast<std::uint64_t>(info->Gpt.StartingUsableOffset),
          std::bit_cast<std::uint64_t>(info->Gpt.UsableLength),
          std::bit_cast<std::uint32_t>(info->Gpt.MaxPartitionCount),
      };
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

template <std::size_t N>
static constexpr uint64_t build_magic(const char (&str)[N])
{
  static_assert(N <= 9);
  static_assert(N > 0);

  uint64_t value = 0;
  for (std::size_t i = 0; i < (N - 1); ++i) { value = (value << 8) | str[i]; }

  return value;
}

template <typename T> static void write_stream(std::ostream& stream, const T& t)
{
  stream.write(reinterpret_cast<const char*>(&t), sizeof(t));
}

template <typename T, typename F, typename... Is>
void for_each(T& t, F f, std::integer_sequence<Is...>)
{
  auto l = {f(std::get<Is>(t))...};
}

struct writer {
  template <typename T> void operator()(const T& t)
  {
    write_stream(*stream, t);
  }

  std::ostream* stream;
};

struct reader {
  template <typename T> void operator()(T& t) { read_stream(*stream, t); }

  std::ostream* stream;
};

template <typename... Args>
static void write_stream(std::ostream& stream, std::tuple<Args...>& args)
{
  auto Idx = std::make_index_sequence<sizeof...(Args)>;

  for_each(args, writer{&stream}, Idx());
}

template <typename... Args>
static void read_stream(std::ostream& stream, std::tuple<Args...>& args)
{
  auto Idx = std::make_index_sequence<sizeof...(Args)>;

  for_each(args, reader{&stream}, Idx());
}

struct file_header {
  // BAreos Disaster recOvery
  static constexpr std::uint64_t magic_value = build_magic("badrfile");

  uint32_t disk_count;


  file_header(uint32_t disk_count_) : disk_count{disk_count_} {}

  auto data() { return std::tuple{disk_count}; }

  void write(std::ostream& stream)
  {
    write_stream(stream, magic_value);
    write_stream(stream, disk_count);
  }
};

void WriteHeader(const disk_map& map)
{
  file_header header(map.size());

  write_stream(std::cout, header.data());
}

struct disk_header {
  static constexpr std::uint64_t magic_value = build_magic("badrdisk");

  uint64_t cylinder_count;
  uint32_t media_type;
  uint32_t tracks_per_cylinder;
  uint32_t sectors_per_track;
  uint32_t bytes_per_sector;
  uint32_t extent_count;

  disk_header(uint64_t cylinder_count_,
              uint32_t media_type_,
              uint32_t tracks_per_cylinder_,
              uint32_t sectors_per_track_,
              uint32_t bytes_per_sector_,
              uint32_t extent_count_)
      : cylinder_count{cylinder_count_}
      , media_type{media_type_}
      , tracks_per_cylinder{tracks_per_cylinder_}
      , sectors_per_track{sectors_per_track_}
      , bytes_per_sector{bytes_per_sector_}
      , extent_count{extent_count_}
  {
  }

  void write(std::ostream& stream)
  {
    write_stream(stream, magic_value);
    write_stream(stream, cylinder_count);
    write_stream(stream, media_type);
    write_stream(stream, tracks_per_cylinder);
    write_stream(stream, sectors_per_track);
    write_stream(stream, bytes_per_sector);
    write_stream(stream, extent_count);
  }
};

void WriteDiskHeader(const disk& Disk, const DISK_GEOMETRY& geo)
{
  disk_header header(geo.Cylinders.QuadPart, geo.MediaType,
                     geo.TracksPerCylinder, geo.SectorsPerTrack,
                     geo.BytesPerSector, Disk.extents.size());

  header.write(std::cout);
}

enum part_type : uint8_t
{
  Raw = 0,
  Mbr = 1,
  Gpt = 2,
};

struct part_table_header {
  static constexpr auto magic_value = build_magic("badrtabl");
  uint32_t partition_count;
  uint32_t part_table_size_in_bytes;
  uint8_t part_table_type;
  uint32_t Datum0;
  uint64_t Datum1;
  uint64_t Datum2;

  part_table_header(uint32_t partition_count_,
                    uint32_t part_table_size_,
                    part_type part_type,
                    uint32_t Datum0_,
                    uint64_t Datum1_,
                    uint64_t Datum2_)
      : partition_count{partition_count_}
      , part_table_type{part_type}
      , Datum0{Datum0_}
      , Datum1{Datum1_}
      , Datum2{Datum2_}
  {
  }

  void write(std::ostream& stream)
  {
    write_stream(stream, magic_value);
    write_stream(stream, partition_count);
    write_stream(stream, part_table_type);
    write_stream(stream, Datum0);
    write_stream(stream, Datum1);
    write_stream(stream, Datum2);
  }
};

struct part_table_entry {
  static constexpr auto magic_value = build_magic("badrtent");

  uint64_t partition_offset;
  uint64_t partition_length;
  uint32_t partition_number;
  uint8_t partition_style;
  bool rewrite_partition;
  bool is_service_partition;

  part_table_entry(const PARTITION_INFORMATION_EX& info)
      : partition_offset{(uint64_t)info.StartingOffset.QuadPart}
      , partition_length{(uint64_t)info.PartitionLength.QuadPart}
      , partition_number{info.PartitionNumber}
      , partition_style{(uint8_t)info.PartitionStyle}
      , rewrite_partition{info.RewritePartition != 0}
      , is_service_partition{info.IsServicePartition != 0}
  {
  }

  void write(std::ostream& stream)
  {
    write_stream(stream, magic_value);
    write_stream(stream, partition_offset);
    write_stream(stream, partition_length);
    write_stream(stream, partition_number);
    write_stream(stream, partition_style);
    write_stream(stream, rewrite_partition);
    write_stream(stream, is_service_partition);
  }
};

void write_guid(std::ostream& stream, GUID guid)
{
  write_stream(stream, guid.Data1);
  write_stream(stream, guid.Data2);
  write_stream(stream, guid.Data3);
  write_stream(stream, guid.Data4);
}

struct part_table_entry_gpt_data {
  GUID partition_type;
  GUID partition_id;
  uint64_t attributes;
  wchar_t name[36];

  part_table_entry_gpt_data(const PARTITION_INFORMATION_GPT& gpt)
      : partition_type{gpt.PartitionType}
      , partition_id{gpt.PartitionId}
      , attributes{gpt.Attributes}
  {
    static_assert(sizeof(name) == sizeof(gpt.Name));
    std::memcpy(name, gpt.Name, sizeof(gpt.Name));
  }

  void write(std::ostream& stream)
  {
    write_guid(stream, partition_type);
    write_guid(stream, partition_id);
    write_stream(stream, attributes);
    write_stream(stream, name);
  }
};

struct part_table_entry_mbr_data {
  GUID partition_id;
  uint32_t num_hidden_sectors;
  uint8_t partition_type;
  bool bootable;
  bool recognized;


  part_table_entry_mbr_data(const PARTITION_INFORMATION_MBR& mbr)
      : partition_id{mbr.PartitionId}
      , num_hidden_sectors{mbr.HiddenSectors}
      , partition_type{(uint8_t)mbr.PartitionType}
      , bootable{mbr.BootIndicator == TRUE}
      , recognized{mbr.RecognizedPartition == TRUE}
  {
  }

  void write(std::ostream& stream)
  {
    write_guid(stream, partition_id);
    write_stream(stream, num_hidden_sectors);
    write_stream(stream, partition_type);
    write_stream(stream, bootable);
    write_stream(stream, recognized);
  }
};

template <class... Ts> struct overloads : Ts... {
  using Ts::operator()...;
};

void WriteDiskPartTable(const disk& Disk, const partition_layout& layout)
{
  {
    auto header = std::visit(
        overloads{
            [](const partition_info_raw&) {
              part_table_header header(0, 0, part_type::Raw, 0, 0, 0);
              return header;
            },
            [](const partition_info_mbr& mbr) {
              part_table_header header(0, 0, part_type::Mbr, mbr.CheckSum,
                                       mbr.Signature, 0);
              return header;
            },
            [](const partition_info_gpt& gpt) {
              part_table_header header(
                  0, 0, part_type::Gpt, gpt.MaxPartitionCount,
                  gpt.StartingUsableOffset, gpt.UsableLength);
              return header;
            },
        },
        layout.info);
    header.partition_count = layout.partition_infos.size();
    header.write(std::cout);
  }


  for (auto& info : layout.partition_infos) {
    part_table_entry ent(info);

    ent.write(std::cout);
    switch (info.PartitionStyle) {
      case PARTITION_STYLE_MBR: {
        part_table_entry_mbr_data data(info.Mbr);

        data.write(std::cout);
      } break;
      case PARTITION_STYLE_GPT: {
        part_table_entry_gpt_data data(info.Gpt);

        data.write(std::cout);
      } break;
      default: { /* intentionally left blank */
      } break;
    }
  }
}

void copy_stream(HANDLE hndl,
                 std::size_t offset,
                 std::size_t length,
                 std::ostream& stream)
{
  DWORD off_low = offset & 0xFFFFFFFF;
  LONG off_high = (offset >> 32) & 0xFFFFFFFF;
  SetFilePointer(hndl, off_low, &off_high, FILE_BEGIN);

  std::vector<char> buffer(1024 * 1024);

  std::size_t bytes_to_read = length;
  while (bytes_to_read > 0) {
    DWORD bytes_read = 0;

    DWORD buffer_size = std::min(buffer.size(), bytes_to_read);

    if (!ReadFile(hndl, buffer.data(), buffer_size, &bytes_read, NULL)) {
      throw win_error("ReadFile", GetLastError());
    }

    if (bytes_read == 0) {
      fprintf(stderr, "premature reading end.  Still %llu bytes to go...",
              bytes_to_read);
      return;
    }

    stream.write(buffer.data(), bytes_read);

    bytes_to_read -= bytes_read;
  }
}


struct extent_header {
  static constexpr auto magic_value = build_magic("badrxtnt");

  uint64_t offset;
  uint64_t length;

  extent_header(const partition_extent& ex)
      : offset{ex.partition_offset}, length{ex.length}
  {
  }

  void write(std::ostream& stream)
  {
    write_stream(stream, offset);
    write_stream(stream, length);
  }
};

void WriteDiskData(const disk& disk_extents)
{
  for (auto& extent : disk_extents.extents) {
    extent_header header(extent);

    header.write(std::cout);

    copy_stream(extent.hndl, extent.handle_offset, extent.length, std::cout);
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

    auto& cover = covers.emplace_back();

    if (info.PartitionStyle == PARTITION_STYLE_MBR
        && info.Mbr.PartitionType == PARTITION_ENTRY_UNUSED) {
      continue;
    }
    if (length == 0) { continue; }

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

  return covers;
}

std::optional<DISK_GEOMETRY> GetDiskGeometry(HANDLE disk)
{
  DISK_GEOMETRY geo;
  DWORD bytes_written = 0;
  auto res = DeviceIoControl(disk, IOCTL_DISK_GET_DRIVE_GEOMETRY, NULL, 0, &geo,
                             sizeof(geo), &bytes_written, NULL);

  if (sizeof(geo) != sizeof(bytes_written)) {
    fprintf(stderr, "drive geometry: bad read.  got %d bytes\n", bytes_written);
    return std::nullopt;
  }

  return geo;
}

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

      StringFromGUID2(identity.instance_id, guid_storage, sizeof(guid_storage));
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

  auto snapshot = VssSnapshot::create(backup_components, volumes);

  auto paths = snapshot.snapshotted_paths(backup_components);

  std::size_t buffer_size = 64 * 1024;
  auto buffer = std::make_unique<char[]>(buffer_size);

  disk_map disks;
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

    fprintf(stderr, "shadow %ls -> %p\n", copy.c_str(), shadow);

    // the operating system will clean up shadow on exit (yes, we leak it)
    GetVolumeExtents(disks, volume, shadow);

    CloseHandle(volume);
  }

  WriteHeader(disks);

  for (auto& [id, disk] : disks) {
    fprintf(stderr, "disk %zu extents\n", id);
    for (auto& extent : disk.extents) {
      fprintf(stderr, "  %zu -> %zu\n", extent.partition_offset,
              extent.partition_offset + extent.length);
    }


    std::wstring disk_path
        = std::wstring(L"\\\\.\\PhysicalDrive") + std::to_wstring(id);

    HANDLE hndl
        = CreateFileW(disk_path.c_str(), 0,
                      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                      NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);

    if (hndl == INVALID_HANDLE_VALUE) {
      fprintf(stderr, "could not open %ls\n", disk_path.c_str());
      continue;
    }

    auto layout = GetPartitionLayout(hndl);
    if (!layout) { continue; }

    auto disk_extents
        = CrossCheckPartitionsAndExtents(layout.value(), disk.extents);
    if (!disk_extents) {
      // continue;
    }

    auto geo = GetDiskGeometry(hndl);
    if (!geo) { continue; }

    WriteDiskHeader(disk, geo.value());
    WriteDiskPartTable(disk, layout.value());
    WriteDiskData(disk);
  }

  for (auto& [path, copy] : paths) {
    /* we read from the shadow copy, but get information from the live system
     * this is necessary since we can only get some metadata from the live
     * system. */
    HANDLE shadow = CreateFileW(
        copy.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_BACKUP_SEMANTICS
            | FILE_FLAG_SEQUENTIAL_SCAN,
        NULL);

    if (shadow == INVALID_HANDLE_VALUE) {
      fprintf(stderr, "volume %ls (%ls) -> could not shadow copy\n",
              path.c_str(), copy.c_str());
      throw win_error("CreateFileW", GetLastError());
    }

    fprintf(stderr, "shadow %ls -> %p\n", copy.c_str(), shadow);

#if 0
    std::size_t byte_count = 0;

    for (;;) {
      DWORD bytes_read = 0;

      if (!ReadFile(shadow, buffer.get(), buffer_size, &bytes_read, NULL)) {
        throw win_error("ReadFile", GetLastError());
      }

      if (bytes_read == 0) { break; }

      byte_count += bytes_read;
    }

    fprintf(stderr, "%ls => %llu bytes\n", path.c_str(), byte_count);
#endif

    CloseHandle(shadow);
  }

  snapshot.delete_snapshot(backup_components);

  backup_components.Release();
}
