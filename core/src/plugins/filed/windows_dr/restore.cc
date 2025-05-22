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

#include "file_format.h"
#include "error.h"

#include <iostream>
#include <string>
#include <format>
#include <stdexcept>
#include <span>
#include <initguid.h>  // ask virtdisk.h to include guid definitions
#include <virtdisk.h>
#include <zlib.h>

DISK_GEOMETRY hardcoded_geo = {.Cylinders = {4177},
                               .MediaType = {},
                               .TracksPerCylinder = 255,
                               .SectorsPerTrack = 64,
                               .BytesPerSector = 512};

std::uint32_t ReadHeader(std::istream& stream)
{
  file_header header;
  header.read(stream);
  return header.disk_count;
}

struct disk_info {
  std::uint64_t disk_size;
  std::uint32_t extent_count;
};

disk_info ReadDiskHeader(std::istream& stream)
{
  disk_header header;
  header.read(stream);

  std::uint64_t disk_size = header.disk_size;

  return {disk_size, header.extent_count};
}


partition_layout ReadDiskPartTable(std::istream& stream)
{
  partition_layout layout;

  fprintf(stderr, "read part header ... \n");
  part_table_header header;
  header.read(stream);
  fprintf(stderr, "read part header ... done\n");

  switch (header.part_table_type) {
    case part_type::Raw: {
      layout.info = partition_info_raw{};
    } break;
    case part_type::Mbr: {
      partition_info_mbr mbr{.CheckSum = header.Datum0,
                             .Signature = (uint32_t)header.Datum1};
      memcpy(mbr.bootstrap, header.Data2, sizeof(header.Data2));
      layout.info = mbr;
    } break;
    case part_type::Gpt: {
      partition_info_gpt gpt{
          .DiskId = std::bit_cast<GUID>(header.Data),
          .StartingUsableOffset = header.Datum1,
          .UsableLength = header.Datum2,
          .MaxPartitionCount = header.Datum0,
      };

      memcpy(gpt.bootstrap, header.Data2, sizeof(header.Data2));
      layout.info = gpt;

    } break;
  }

  fprintf(stderr, "found %u partitions\n", header.partition_count);

  layout.partition_infos.resize(header.partition_count);

  for (auto& info : layout.partition_infos) {
    fprintf(stderr, "read entry ...\n");
    part_table_entry entry;
    entry.read(stream);
    fprintf(stderr, "read entry ... done\n");

    info.PartitionStyle = (PARTITION_STYLE)entry.partition_style;
    info.StartingOffset.QuadPart = entry.partition_offset;
    info.PartitionLength.QuadPart = entry.partition_length;
    info.PartitionNumber = entry.partition_number;

    switch (entry.partition_style) {
      case PARTITION_STYLE_MBR: {
        fprintf(stderr, "read mbr ...\n");
        part_table_entry_mbr_data data;
        data.read(stream);
        fprintf(stderr, "read mbr ... done\n");

        info.Mbr.PartitionType = data.partition_type;
        info.Mbr.BootIndicator = data.bootable ? TRUE : FALSE;
        info.Mbr.RecognizedPartition = data.recognized ? TRUE : FALSE;
        info.Mbr.HiddenSectors = data.num_hidden_sectors;
        info.Mbr.PartitionId = data.partition_id;
      } break;
      case PARTITION_STYLE_GPT: {
        fprintf(stderr, "read gpt ...\n");
        part_table_entry_gpt_data data;
        data.read(stream);
        fprintf(stderr, "read gpt ... done\n");

        fprintf(stderr, "gpt name = '%.36ls'\n", data.name);

        info.Gpt.PartitionType = data.partition_type;
        info.Gpt.PartitionId = data.partition_id;
        info.Gpt.Attributes = data.attributes;
        static_assert(sizeof(data.name) == sizeof(info.Gpt.Name));
        memcpy(info.Gpt.Name, data.name, sizeof(data.name));
      } break;
    }
  }

  return layout;
}

void write_buffer(HANDLE output,
                  std::size_t start,
                  std::span<const char> buffer)
{
  std::size_t offset = 0;

  fprintf(
      stderr,
      "%llu: %02X %02X %02X %02X %02X %02X ... %02X %02X %02X %02X %02X %02X\n",
      start, (unsigned char)buffer[0], (unsigned char)buffer[1],
      (unsigned char)buffer[2], (unsigned char)buffer[3],
      (unsigned char)buffer[4], (unsigned char)buffer[5],
      (unsigned char)buffer[buffer.size() - 6],
      (unsigned char)buffer[buffer.size() - 5],
      (unsigned char)buffer[buffer.size() - 4],
      (unsigned char)buffer[buffer.size() - 3],
      (unsigned char)buffer[buffer.size() - 2],
      (unsigned char)buffer[buffer.size() - 1]);

  while (offset != buffer.size()) {
    OVERLAPPED overlapped = {};
    overlapped.Offset = (DWORD)(start + offset);
    overlapped.OffsetHigh = (DWORD)((start + offset) >> 32);

    DWORD bytes_written = 0;

    assert(!WriteFile(output, buffer.data() + offset,
                      (DWORD)(buffer.size() - offset), NULL, &overlapped));
    auto err = GetLastError();

    if (err != ERROR_IO_PENDING) {
      fprintf(stderr, "WriteFile(no-async): could not write (%llu, %llu): %d\n",
              offset, buffer.size() - offset, err);
      assert(err == ERROR_IO_PENDING);
    }

    if (!GetOverlappedResult(output, &overlapped, &bytes_written, TRUE)) {
      err = GetLastError();
      fprintf(stderr, "WriteFile: could not write (%llu, %llu): %d\n", offset,
              buffer.size() - offset, err);

      throw win_error("WriteFileW", err);
    }

    offset += bytes_written;
  }
}

#pragma pack(push, 1)
struct gpt_header {
  char signature[8] = {'E', 'F', 'I', ' ', 'P', 'A', 'R', 'T'};
  uint32_t revision = 1 << 16;
  uint32_t header_size = sizeof(gpt_header);
  uint32_t crc32_sum = 0;
  uint32_t reserved_0 = 0;

  uint64_t lba_location;
  uint64_t backup_lba_location;
  uint64_t first_usable_lba;
  uint64_t last_usable_lba;

  GUID disk_guid;

  uint64_t table_lba_location;

  uint32_t partition_count;
  uint32_t entry_size;
  uint32_t table_crc32_sum;
};
#pragma pack(pop)

static_assert(sizeof(gpt_header) == 92);

struct gpt_entry {
  GUID partition_type;
  GUID id;
  uint64_t start_lba;
  uint64_t end_lba;  // inclusive!
  uint64_t attributes;
  wchar_t name[36];
};

static_assert(sizeof(gpt_entry) == 128);

struct ccitt_crc {
  ccitt_crc& update(std::span<const char> data)
  {
    crc = crc32_z(crc, reinterpret_cast<const unsigned char*>(data.data()),
                  data.size());
    return *this;
  }

  std::uint32_t get() const { return crc; }

 private:
  std::uint32_t crc = 0;
};


template <typename T> std::span<const char> as_span(const T& val)
{
  return {reinterpret_cast<const char*>(&val), sizeof(T)};
}

template <typename T> std::span<const char> byte_span(const std::vector<T>& val)
{
  std::span<const char> result{reinterpret_cast<const char*>(val.data()),
                               sizeof(T) * val.size()};

  fprintf(stderr, "first bytes: %02X %02X %02X %02X %02X %02X\n",
          (unsigned char)result[0], (unsigned char)result[1],
          (unsigned char)result[2], (unsigned char)result[3],
          (unsigned char)result[4], (unsigned char)result[5]);
  return result;
}

gpt_header MakeGptHeader(const partition_info_gpt& info,
                         ccitt_crc table_crc,
                         uint64_t disk_size)
{
  gpt_header header;

  header.lba_location = 1;
  std::uint64_t lba_count = disk_size / 512;
  header.backup_lba_location = lba_count - 1;

  {
    wchar_t guid_storage[64] = {};

    StringFromGUID2(info.DiskId, guid_storage, sizeof(guid_storage));
    fprintf(stderr, "disk id: %ls\n", guid_storage);
  }

  fprintf(stderr, "disk size = %llu, lbas = %llu, loc = %llx\n", disk_size,
          lba_count, header.backup_lba_location);

  fprintf(stderr, "usable start = %llu, usable length = %llu\n",
          info.StartingUsableOffset, info.UsableLength);
  header.first_usable_lba = info.StartingUsableOffset / 512;
  header.last_usable_lba
      = header.first_usable_lba + info.UsableLength / 512 - 1;

  header.disk_guid = info.DiskId;
  header.table_lba_location = 2;

  header.partition_count = info.MaxPartitionCount;
  header.entry_size = sizeof(gpt_entry);
  header.table_crc32_sum = table_crc.get();

  header.crc32_sum = ccitt_crc{}.update(as_span(header)).get();

  return header;
}

#pragma pack(push, 1)
struct chs {
  // chs = cylinder, head, sector
  uint8_t data[3]{};

  chs() = default;
  chs(std::uint16_t cylinder,  // 10 bits
      std::uint8_t head,       // 8 bits
      std::uint8_t sector      // 6 bits
  )
  {
    data[0] = head;
    data[1] = sector | (cylinder << 6) & 0b11;
    data[2] = (cylinder & 0b11111111);
  }

  static chs from_lba(std::uint64_t lba, DISK_GEOMETRY geo)
  {
    // lba = ((cylinder * geo.TracksPerCylinder) + head) * geo.SectorsPerTrack
    //             + (sector - 1)

    std::uint64_t sector = lba % geo.SectorsPerTrack + 1;
    auto rest = lba / geo.SectorsPerTrack;

    std::uint64_t head = rest % geo.TracksPerCylinder;
    std::uint64_t cylinder = rest / geo.TracksPerCylinder;


    if (sector > 63 || head > 255 || cylinder > 1023) {
      // offset is too large, so just return the largest one
      return chs((std::uint16_t)(geo.Cylinders.QuadPart - 1),
                 (std::uint8_t)(geo.TracksPerCylinder - 1),
                 (std::uint8_t)(geo.SectorsPerTrack - 1));
    }

    return chs((std::uint16_t)cylinder, (std::uint8_t)head,
               (std::uint8_t)sector);
  }
};

static_assert(sizeof(chs) == 3);

struct mbr_entry {
  enum attrs : std::uint8_t
  {
    None = 0,
    Bootable = (1 << 7),
  };

  attrs attributes;
  chs start_chs;
  uint8_t os_type;
  chs end_chs;
  uint32_t start_lba;
  uint32_t end_lba;
};

static_assert(sizeof(mbr_entry) == 16);

struct mbr {
  char bootstrap[446];
  mbr_entry entries[4];
  uint8_t check[2];
};

static_assert(sizeof(mbr) == 512);

#pragma pack(pop)

mbr_entry MakeMbrEntry(const PARTITION_INFORMATION_EX& info)
{
  if (info.PartitionStyle != PARTITION_STYLE_MBR) {
    throw std::invalid_argument{
        "cannot create mbr partition from non mbr partition"};
  }

  mbr_entry entry = {};

  if (info.Mbr.BootIndicator) { entry.attributes = mbr_entry::attrs::Bootable; }

  wchar_t guid_storage[64] = {};
  StringFromGUID2(info.Mbr.PartitionId, guid_storage, sizeof(guid_storage));
  fprintf(stderr, "mbr partition guid: %ls\n", guid_storage);

  entry.os_type = info.Mbr.PartitionType;
  entry.start_lba = info.StartingOffset.QuadPart / 512;
  entry.end_lba = entry.start_lba + info.PartitionLength.QuadPart / 512 - 1;
  entry.start_chs = chs::from_lba(entry.start_lba, hardcoded_geo);
  entry.end_chs = chs::from_lba(entry.end_lba, hardcoded_geo);

  return entry;
}

gpt_entry MakeGptEntry(const PARTITION_INFORMATION_EX& info)
{
  if (info.PartitionStyle != PARTITION_STYLE_GPT) {
    throw std::invalid_argument{
        "cannot create gpt partition from non gpt partition"};
  }

  gpt_entry entry;
  entry.partition_type = info.Gpt.PartitionType;

  wchar_t guid_storage[64] = {};

  StringFromGUID2(info.Gpt.PartitionType, guid_storage, sizeof(guid_storage));
  fprintf(stderr, "Partition-Type: %ls\n", guid_storage);
  StringFromGUID2(entry.partition_type, guid_storage, sizeof(guid_storage));
  fprintf(stderr, "Partition-Type: %ls\n", guid_storage);

  entry.id = info.Gpt.PartitionId;
  entry.start_lba = info.StartingOffset.QuadPart / 512;
  entry.end_lba = entry.start_lba + info.PartitionLength.QuadPart / 512 - 1;
  entry.attributes = info.Gpt.Attributes;
  std::memcpy(entry.name, info.Gpt.Name, sizeof(entry.name));
  return entry;
}

void WriteProtectionMBR(HANDLE output,
                        std::uint64_t end_lba,
                        std::span<const char> bootstrap)
{
  static constexpr uint8_t gpt_type = 0xEE;

  mbr_entry gpt_entry = {
      .attributes
      = mbr_entry::attrs::None,  // if you dont understand gpt, dont boot this
      .start_chs = chs::from_lba(1, hardcoded_geo),
      .os_type = gpt_type,
      .end_chs = chs::from_lba(end_lba, hardcoded_geo),
      .start_lba = 1,
      .end_lba = (std::uint32_t)std::max(
          end_lba, (std::uint64_t)std::numeric_limits<std::uint32_t>::max()),
  };

  mbr protective_mbr = {};
  protective_mbr.entries[0] = gpt_entry;

  assert(bootstrap.size() <= std::size(protective_mbr.bootstrap));
  memcpy(protective_mbr.bootstrap, bootstrap.data(), bootstrap.size());

  protective_mbr.check[0] = 0x55;
  protective_mbr.check[1] = 0xAA;

  static_assert(sizeof(mbr) == 512);

  write_buffer(output, 0, as_span(protective_mbr));
}

void WritePartitionTable(HANDLE output,
                         const partition_layout& table,
                         std::size_t disk_size)
{
  if (auto* Gpt = std::get_if<partition_info_gpt>(&table.info)) {
    // we still have to add support for shared gpt/mbr partitions
    WriteProtectionMBR(output, disk_size / 512 - 1, Gpt->bootstrap);

    std::size_t table_offset = 1024;

    ccitt_crc table_crc;

    std::size_t partition_count = table.partition_infos.size();
    std::size_t rounded_size = (partition_count + 3) / 4 * 4;
    // we can only write in 512byte bursts, so make sure
    // that we always write in multiple of 4 entries at once
    std::vector<gpt_entry> entries;
    entries.reserve(rounded_size);
    for (auto& part : table.partition_infos) {
      auto& entry = entries.emplace_back(MakeGptEntry(part));
      table_crc.update(as_span(entry));
    }

    entries.resize(rounded_size);
    write_buffer(output, table_offset, byte_span(entries));
    table_offset += sizeof(gpt_entry) * partition_count;

    for (size_t i = 0;
         i < Gpt->MaxPartitionCount - table.partition_infos.size(); ++i) {
      gpt_entry entry;
      memset(&entry, 0, sizeof(entry));
      table_crc.update(as_span(entry));
      // write_buffer(output, table_offset, as_span(entry));
      table_offset += sizeof(entry);
    }

    auto header = MakeGptHeader(*Gpt, table_crc, disk_size);

    char sector[512] = {};

    std::memcpy(sector, &header, sizeof(header));

    write_buffer(output, 512, as_span(sector));
  } else if (auto* Mbr = std::get_if<partition_info_mbr>(&table.info)) {
    // todo
    fprintf(stderr, "cant restore mbr yet\n");

    mbr GeneratedMbr = {};

    static_assert(sizeof(GeneratedMbr.bootstrap) == sizeof(Mbr->bootstrap));
    memcpy(GeneratedMbr.bootstrap, Mbr->bootstrap, sizeof(Mbr->bootstrap));

    if (table.partition_infos.size() > 4) {
      throw std::runtime_error(
          "cannot restore mbr with more than 4 partitions");
    }

    for (std::size_t i = 0; i < table.partition_infos.size(); ++i) {
      GeneratedMbr.entries[i] = MakeMbrEntry(table.partition_infos[i]);
    }

    write_buffer(output, 0, as_span(GeneratedMbr));
  } else {
    // todo
    fprintf(stderr, "cant restore raw yet\n");
  }
}

void copy_output(std::istream& stream,
                 std::size_t offset,
                 std::size_t length,
                 HANDLE output)
{
#if !defined(DO_DRY)
  std::vector<char> buffer;
  buffer.resize(4 * 1024 * 1024);

  std::size_t bytes_to_read = length;

  while (bytes_to_read > 0) {
    auto bytes_this_call = std::min(bytes_to_read, buffer.size());

    stream.read(buffer.data(), bytes_this_call);

    auto read_this_call = stream.gcount();

    if (read_this_call != (std::streamsize)bytes_this_call) {
      fprintf(stderr, "read %llu/%llu, got %llu\n", bytes_this_call,
              bytes_to_read, read_this_call);

      assert(read_this_call == (std::streamsize)bytes_this_call);
    }

    write_buffer(output, offset + (length - bytes_to_read),
                 {std::begin(buffer), bytes_this_call});

    bytes_to_read -= bytes_this_call;
  }
#endif
}

void ReadExtent(std::istream& stream, HANDLE output)
{
  extent_header header;
  header.read(stream);

  fprintf(stderr, "Extent: Offset=%llu Length=%llu\n", header.offset,
          header.length);

  copy_output(stream, header.offset, header.length, output);
}

void restore_data(std::istream& stream, bool raw_file)
{
  fprintf(stderr, "reading header\n");
  auto disk_count = ReadHeader(stream);

  fprintf(stderr, "disk count = %lu\n", disk_count);
  for (std::size_t i = 0; i < disk_count; ++i) {
    fprintf(stderr, "reading disk header\n");
    auto disk_header = ReadDiskHeader(stream);
    fprintf(stderr, "reading disk header ... done\n");

    HANDLE output = INVALID_HANDLE_VALUE;

    if (raw_file) {
      auto disk_path = std::format(L"disk-{}.raw", i);

      output = CreateFileW(disk_path.c_str(), GENERIC_WRITE, FILE_SHARE_WRITE,
                           NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);

      if (output == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "could not open %ls: Err=%d\n", disk_path.c_str(),
                GetLastError());

        return;
      }
    } else {
      auto disk_path = std::format(L"disk-{}.vhdx", i);

      VIRTUAL_STORAGE_TYPE vst
          = {.DeviceId = VIRTUAL_STORAGE_TYPE_DEVICE_VHDX,
             .VendorId = VIRTUAL_STORAGE_TYPE_VENDOR_MICROSOFT};

      CREATE_VIRTUAL_DISK_PARAMETERS params = {
        .Version = CREATE_VIRTUAL_DISK_VERSION_2,
        .Version2 = {
          .UniqueId = {},
          .MaximumSize = disk_header.disk_size,
          .BlockSizeInBytes = {},
          .SectorSizeInBytes = {},
          .PhysicalSectorSizeInBytes = {},
          .ParentPath = nullptr,
          .SourcePath = nullptr,
        },
      };

      fprintf(stderr, "creating disk\n");
      DWORD hres = CreateVirtualDisk(
          &vst, disk_path.c_str(), VIRTUAL_DISK_ACCESS_NONE, NULL,
          CREATE_VIRTUAL_DISK_FLAG_NONE, 0, &params, NULL, &output);

      if (hres != ERROR_SUCCESS) {
        fprintf(stderr, "CreateVirtualDisk(%ls) returned %d\n",
                disk_path.c_str(), hres);
        return;
      }

      ATTACH_VIRTUAL_DISK_PARAMETERS attach_params
          = {.Version = ATTACH_VIRTUAL_DISK_VERSION_1, .Version1 = {}};
      auto attach_res = AttachVirtualDisk(
          output, NULL, ATTACH_VIRTUAL_DISK_FLAG_NO_LOCAL_HOST, 0,
          &attach_params, 0);

      if (attach_res != ERROR_SUCCESS) {
        fprintf(stderr, "AttachVirtualDisk(%ls) returned %d\n",
                disk_path.c_str(), attach_res);
        return;
      }
    }


    fprintf(stderr, "reading part table ...\n");
    auto part_table = ReadDiskPartTable(stream);
    fprintf(stderr, "reading part table ... done\n");
    for (size_t extent = 0; extent < disk_header.extent_count; ++extent) {
      fprintf(stderr, "reading extent ...\n");
      ReadExtent(stream, output);
      fprintf(stderr, "reading extent ... done\n");
    }

    WritePartitionTable(output, part_table, disk_header.disk_size);

    CloseHandle(output);
  }
}
