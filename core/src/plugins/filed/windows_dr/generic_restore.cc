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

#include "generic_restore.h"

disk_info ReadDiskHeader(std::istream& stream)
{
  disk_header header;
  header.read(stream);

  std::uint64_t disk_size = header.disk_size;

  return {disk_size, header.extent_count};
}

enum
{
  PARTITION_STYLE_MBR = 0,
  PARTITION_STYLE_GPT = 1,
  PARTITION_STYLE_RAW = 2,
};

void ParseDiskPartTable(std::istream& stream, RestoreStrategy* strategy)
{
  part_table_header header;
  header.read(stream);

  switch (header.part_table_type) {
    case part_type::Raw: {
      strategy->BeginRawTable(partition_info_raw{});
    } break;
    case part_type::Mbr: {
      partition_info_mbr mbr{.CheckSum = header.Datum0,
                             .Signature = (uint32_t)header.Datum1,
                             .bootstrap = {}};
      memcpy(mbr.bootstrap, header.Data2, sizeof(header.Data2));
      strategy->BeginMbrTable(mbr);
    } break;
    case part_type::Gpt: {
      partition_info_gpt gpt{.DiskId = std::bit_cast<guid>(header.Data),
                             .StartingUsableOffset = header.Datum1,
                             .UsableLength = header.Datum2,
                             .MaxPartitionCount = header.Datum0,
                             .bootstrap = {}};
      memcpy(gpt.bootstrap, header.Data2, sizeof(header.Data2));
      strategy->BeginGptTable(gpt);
    } break;
  }

  for (size_t i = 0; i < header.partition_count; ++i) {
    part_table_entry entry;
    entry.read(stream);

    switch (entry.partition_style) {
      case PARTITION_STYLE_MBR: {
        part_table_entry_mbr_data data;
        data.read(stream);

        strategy->MbrEntry(entry, data);
      } break;
      case PARTITION_STYLE_GPT: {
        part_table_entry_gpt_data data;
        data.read(stream);

        strategy->GptEntry(entry, data);
      } break;
    }
  }

  strategy->EndPartTable();
}

std::uint32_t ReadHeader(std::istream& stream)
{
  file_header header;
  header.read(stream);

  assert(header.version == file_header::current_version);

  return header.disk_count;
}

void ParseExtent(std::istream& stream, RestoreStrategy* strategy)
{
  extent_header header;
  header.read(stream);

  strategy->BeginExtent(header);

  std::vector<char> buffer;
  buffer.resize(4 * 1024 * 1024);

  std::size_t bytes_to_read = header.length;

  while (bytes_to_read > 0) {
    auto bytes_this_call = std::min(bytes_to_read, buffer.size());

    stream.read(buffer.data(), bytes_this_call);

    auto read_this_call = stream.gcount();

    if (read_this_call != (std::streamsize)bytes_this_call) {
      assert(read_this_call == (std::streamsize)bytes_this_call);
    }

    strategy->ExtentData({std::begin(buffer), bytes_this_call});

    bytes_to_read -= bytes_this_call;
  }
  strategy->EndExtent();
}

void generic_restore(std::istream& stream, RestoreStrategy* strategy)
{
  auto disk_count = ReadHeader(stream);

  strategy->BeginRestore(disk_count);
  for (std::size_t i = 0; i < disk_count; ++i) {
    auto disk_header = ReadDiskHeader(stream);
    strategy->BeginDisk(disk_header);

    ParseDiskPartTable(stream, strategy);
    for (size_t extent = 0; extent < disk_header.extent_count; ++extent) {
      ParseExtent(stream, strategy);
    }

    strategy->EndDisk();
  }

  strategy->EndRestore();
}
