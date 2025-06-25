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

#include "parser.h"
#include <bit>
#include <fmt/format.h>

struct parser {
  GenericLogger* logger;
  parser(GenericLogger* logger_) : logger{logger_} {}

  struct logging_reader : public reader {
    logging_reader(GenericLogger* logger_, std::istream* stream_)
        : logger{logger_}, stream{stream_}
    {
    }

    void read(char* buffer, std::size_t size) override
    {
      stream->read(buffer, size);
      auto read_bytes = stream->gcount();
      if (read_bytes != (std::streamsize)size) {
        throw std::runtime_error{fmt::format(
            "wanted to read {} bytes from input stream but only {} were read",
            size, read_bytes)};
      }
      logger->Progressed(size);
    }

    GenericLogger* logger;
    std::istream* stream;
  };


  disk_info ReadDiskHeader(reader& stream)
  {
    disk_header header;
    header.read(stream);

    std::uint64_t disk_size = header.disk_size;

    return {disk_size, header.extent_count, header.total_extent_size};
  }

  void ParseDiskPartTable(reader& stream, GenericHandler* strategy)
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
        case Mbr: {
          part_table_entry_mbr_data data;
          data.read(stream);

          strategy->MbrEntry(entry, data);
        } break;
        case Gpt: {
          part_table_entry_gpt_data data;
          data.read(stream);

          strategy->GptEntry(entry, data);
        } break;
        default: {
          throw std::logic_error{
              fmt::format("unknown partition type ({}) encountered",
                          static_cast<std::uint8_t>(entry.partition_style))};
        }
      }
    }

    strategy->EndPartTable();
  }

  std::uint32_t ReadHeader(reader& stream)
  {
    file_header header;
    header.read(stream);

    assert(header.version == file_header::current_version);

    return header.disk_count;
  }

  void ParseExtent(reader& stream, GenericHandler* strategy)
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

      strategy->ExtentData({std::begin(buffer), bytes_this_call});

      bytes_to_read -= bytes_this_call;
    }
    strategy->EndExtent();
  }

  void parse(std::istream& input_stream, GenericHandler* strategy)
  {
    logging_reader stream{logger, &input_stream};

    auto disk_count = ReadHeader(stream);

    Info("Restoring {} disks", disk_count);
    strategy->BeginRestore(disk_count);
    for (std::size_t disk = 0; disk < disk_count; ++disk) {
      auto disk_header = ReadDiskHeader(stream);
      Info("Restoring disk {} of size {}", disk, disk_header.disk_size);
      strategy->BeginDisk(disk_header);
      logger->Begin(disk_header.disk_size);

      Info("Restoring partition table");
      ParseDiskPartTable(stream, strategy);
      for (size_t extent = 0; extent < disk_header.extent_count; ++extent) {
        Info("Restoring extent {}/{}", extent, disk_header.extent_count);
        ParseExtent(stream, strategy);
      }

      strategy->EndDisk();
      logger->End();
      Info("disk {} finished", disk);
    }

    strategy->EndRestore();
    Info("restore completed");
  }

  template <typename... Args>
  void Info(fmt::format_string<Args...> fmt, Args&&... args)
  {
    logger->Info(fmt::format(fmt, std::forward<Args>(args)...));
  }
};

void parse_file_format(GenericLogger* logger,
                       std::istream& stream,
                       GenericHandler* strategy)
{
  parser FileParser{logger};
  FileParser.parse(stream, strategy);
}
