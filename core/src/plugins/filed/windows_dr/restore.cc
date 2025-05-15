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
#include <span>
#include <initguid.h>  // ask virtdisk.h to include guid definitions
#include <virtdisk.h>

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

  std::uint64_t disk_size = header.cylinder_count;
  disk_size *= header.tracks_per_cylinder;
  disk_size *= header.sectors_per_track;
  disk_size *= header.bytes_per_sector;

  return {disk_size, header.extent_count};
}

struct partition_table {};

partition_table ReadDiskPartTable(std::istream& stream)
{
  part_table_header header;
  header.read(stream);

  for (size_t i = 0; i < header.partition_count; ++i) {
    part_table_entry entry;
    entry.read(stream);

    switch (entry.partition_style) {
      case PARTITION_STYLE_MBR: {
        part_table_entry_mbr_data data;
        data.read(stream);
      } break;
      case PARTITION_STYLE_GPT: {
        part_table_entry_gpt_data data;
        data.read(stream);
      } break;
    }
  }

  return {};
}

void write_buffer(HANDLE output, std::span<const char> buffer)
{
  std::size_t offset = 0;

  while (offset != buffer.size()) {
    DWORD bytes_written;

    if (!WriteFile(output, buffer.data() + offset,
                   (DWORD)(buffer.size() - offset), &bytes_written, NULL)) {
      throw win_error("WriteFileW", GetLastError());
    }

    offset += bytes_written;
  }
}

struct gpt_header {
  uint64_t signature;
  uint32_t revision;
  uint32_t header_size;
  uint32_t crc32_sum;
  uint32_t reserved_0;

  uint64_t lba_location;
  uint64_t backup_lba_location;
  uint64_t first_usable_lba;

  GUID disk_guid;

  uint64_t table_lba_location;

  uint32_t partition_count;
  uint32_t entry_size;
  uint32_t table_crc32_sum;
};

static_assert(sizeof(gpt_header) == 96);

struct gpt_entry {
  GUID partition_type;
  GUID id;
  uint64_t start;
  uint64_t end;  // inclusive!
  uint64_t attributes;
  wchar_t name[36];
};

static_assert(sizeof(gpt_entry) == 128);

gpt_header MakeGptHeader(const partition_info_gpt&) { return {}; }

gpt_entry MakeGptEntry() { return {}; }

void WritePartitionTable(HANDLE output,
                         const partition_layout& table,
                         std::size_t disk_size)
{
  auto* Gpt = std::get_if<partition_info_gpt>(&table.info);
  if (!Gpt) { return; }

  auto header = MakeGptHeader(*Gpt);

  char sector[512] = {};

  std::memcpy(sector, &header, sizeof(header));

  {
    DWORD off_low = 512;
    LONG off_high = 0;

    SetFilePointer(output, off_low, &off_high, FILE_BEGIN);
  }

  write_buffer(output, sector);
}

void copy_output(std::istream& stream,
                 std::size_t offset,
                 std::size_t length,
                 HANDLE output)
{
#if !defined(DO_DRY)
  std::vector<char> buffer;
  buffer.resize(4 * 1024 * 1024);

  DWORD off_low = offset & 0xFFFFFFFF;
  LONG off_high = (offset >> 32) & 0xFFFFFFFF;
  SetFilePointer(output, off_low, &off_high, FILE_BEGIN);

  std::size_t bytes_to_read = length;

  while (bytes_to_read > 0) {
    stream.read(buffer.data(), buffer.size());

    assert(stream.gcount() == (std::streamsize)buffer.size());

    write_buffer(output, buffer);
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

void restore_data()
{
  fprintf(stderr, "reading header\n");
  auto disk_count = ReadHeader(std::cin);

  fprintf(stderr, "disk count = %lu\n", disk_count);
  for (std::size_t i = 0; i < disk_count; ++i) {
    auto disk_path = std::format(L"disk-{}.vhdx", i);

    fprintf(stderr, "reading disk header\n");
    auto disk_header = ReadDiskHeader(std::cin);
    fprintf(stderr, "reading disk header ... done\n");

    VIRTUAL_STORAGE_TYPE vst
        = {.DeviceId = VIRTUAL_STORAGE_TYPE_DEVICE_VHDX,
           .VendorId = VIRTUAL_STORAGE_TYPE_VENDOR_MICROSOFT};

    HANDLE output = INVALID_HANDLE_VALUE;

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
      fprintf(stderr, "CreateVirtualDisk(%ls) returned %d\n", disk_path.c_str(),
              hres);
      return;
    }

    ATTACH_VIRTUAL_DISK_PARAMETERS attach_params
        = {.Version = ATTACH_VIRTUAL_DISK_VERSION_1, .Version1 = {}};
    auto attach_res = AttachVirtualDisk(output, NULL,
                                        ATTACH_VIRTUAL_DISK_FLAG_NO_LOCAL_HOST,
                                        0, &attach_params, 0);

    if (attach_res != ERROR_SUCCESS) {
      fprintf(stderr, "AttachVirtualDisk(%ls) returned %d\n", disk_path.c_str(),
              attach_res);
      return;
    }


    fprintf(stderr, "reading part table ...\n");
    auto part_table = ReadDiskPartTable(std::cin);
    fprintf(stderr, "reading part table ... done\n");
    for (size_t extent = 0; extent < disk_header.extent_count; ++extent) {
      fprintf(stderr, "reading extent ...\n");
      ReadExtent(std::cin, output);
      fprintf(stderr, "reading extent ... done\n");
    }

    partition_layout layout;
    WritePartitionTable(output, layout, disk_header.disk_size);

    CloseHandle(output);
  }
}
