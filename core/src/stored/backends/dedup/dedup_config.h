/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2023-2023 Bareos GmbH & Co. KG

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
*/

#ifndef BAREOS_STORED_BACKENDS_DEDUP_DEDUP_CONFIG_H_
#define BAREOS_STORED_BACKENDS_DEDUP_DEDUP_CONFIG_H_

#include "lib/network_order.h"
#include <array>
#include <cstdint>
#include <string>
#include <vector>
#include <optional>

namespace dedup::config {


using net_u32 = network_order::network_value<std::uint32_t>;
using net_i32 = network_order::network_value<std::int32_t>;
using net_u64 = network_order::network_value<std::uint64_t>;
using net_u16 = network_order::network_value<std::uint16_t>;
using net_u8 = std::uint8_t;
using net_i64 = network_order::network_value<std::int64_t>;

struct file_header {
  // since char8_t only exists from C++20, we use std::byte here
  // In the future we should switch to char8_t
  static constexpr std::array<std::byte, 8> correct_identifier = {
      static_cast<std::byte>(u8'D'), static_cast<std::byte>(u8'D'),
      static_cast<std::byte>(u8'C'), static_cast<std::byte>(u8'O'),
      static_cast<std::byte>(u8'N'), static_cast<std::byte>(u8'F'),
      static_cast<std::byte>(u8'I'), static_cast<std::byte>(u8'G'),
  };

  // the file header is always written in big endian!

  std::array<std::byte, 8> identifier;  // will say "DDCONFIG" in ascii
  net_u64 file_size;
  net_u32 version;
  net_u32 section_alignment;
  net_u64 file_checksum;  // checksum does not include this header
  net_u64 checksum;       // checksum for the header; does not include itsef
                          // this _has_ to be the last member!
};

static_assert(sizeof(file_header) == 40);

// sections are 16byte aligned
enum class byte_order : std::uint8_t
{
  little_endian = 1,
  big_endian = 2
};

struct section_header {
  enum class types : std::uint16_t
  {
    GeneralInfo = 0,
    BlockFile = 1,
    RecordFile = 2,
    DataFile = 3,
    UnfinishedRecord = 4,
  };
  // total section size = header size + data size + padding
  // padding is between header & data
  net_u16 type;
  net_u16 version;
  net_u32
      padding;  // the amount of padding between the header and start of data

  net_u32 data_size;
  net_u8 written_order;  // order of bytes inside section
                         // currently only big endian is supported
  net_u8 reserved_0{0};
  net_u16 reserved_1{0};

  net_u64 data_checksum;
  net_u64 checksum;  // checksum for the header; does not include itsef
                     // this _has_ to be the last member!
};

static_assert(sizeof(section_header) == 32);

struct general_section {
  net_u32 block_header_size;
  net_u32 record_header_size;
  net_u32 dedup_block_header_size;
  net_u32 dedup_record_header_size;
};

struct block_file_section {
  net_u64 start_block;
  net_u64 num_blocks;
  net_u32 path_length;
  // the path data follows directly without any padding
  // as if
  // std::byte path_data[]; (in utf8 encoding)
  // was the next member (not legal in C++)
};

struct record_file_section {
  net_u64 start_record;
  net_u64 num_records;
  net_u32 path_length;
  // the path data follows directly without any padding
  // as if
  // std::byte path_data[]; (in utf8 encoding)
  // was the next member (not legal in C++)
};

struct data_file_section {
  net_u64 data_used;
  net_u64 block_size;
  net_u64 file_index;
  net_u32 path_length;
  // the path data follows directly without any padding
  // as if
  // std::byte path_data[]; (in utf8 encoding)
  // was the next member (not legal in C++)
};

struct unfinished_record_section {
  // record data of unfinished record
  net_u32 VolSessionId;
  net_u32 VolSessionTime;
  net_i32 FileIndex;
  net_i32 Stream;

  // location of allocated section in data file
  net_u64 file_offset;
  net_u64 DataIdx;
  net_u64 size;
};

struct loaded_data_section {
  std::int32_t block_size;
  std::uint32_t file_index;
  std::string path;
  std::uint64_t data_used;

  loaded_data_section() = default;
  loaded_data_section(std::uint32_t file_index,
                      std::int32_t block_size,
                      std::string path,
                      std::uint64_t data_used)
      : block_size(block_size)
      , file_index(file_index)
      , path(std::move(path))
      , data_used{data_used}
  {
  }
};

struct loaded_block_section {
  std::uint64_t start_block;
  std::uint64_t num_blocks;
  std::string path;

  loaded_block_section() = default;
  loaded_block_section(std::uint64_t start_block,
                       std::uint64_t num_blocks,
                       std::string path)
      : start_block(start_block), num_blocks(num_blocks), path(std::move(path))
  {
  }
};

struct loaded_record_section {
  std::uint64_t start_record;
  std::uint64_t num_records;
  std::string path;

  loaded_record_section() = default;
  loaded_record_section(std::uint64_t start_record,
                        std::uint64_t num_records,
                        std::string path)
      : start_record(start_record)
      , num_records(num_records)
      , path(std::move(path))
  {
  }
};

struct loaded_general_info {
  std::uint32_t block_header_size;
  std::uint32_t record_header_size;
  std::uint32_t dedup_block_header_size;
  std::uint32_t dedup_record_header_size;
};

struct loaded_unfinished_record {
  // record data of unfinished record
  std::uint32_t VolSessionId;
  std::uint32_t VolSessionTime;
  std::int32_t FileIndex;
  std::int32_t Stream;

  // location of allocated section in data file
  std::uint64_t DataIdx;
  std::uint64_t file_offset;
  std::uint64_t size;

  loaded_unfinished_record() = default;
  loaded_unfinished_record(std::uint32_t VolSessionId,
                           std::uint32_t VolSessionTime,
                           std::int32_t FileIndex,
                           std::int32_t Stream,
                           std::uint64_t DataIdx,
                           std::uint64_t file_offset,
                           std::uint64_t size)
      : VolSessionId{VolSessionId}
      , VolSessionTime{VolSessionTime}
      , FileIndex{FileIndex}
      , Stream{Stream}
      , DataIdx{DataIdx}
      , file_offset{file_offset}
      , size{size}
  {
  }
};

struct loaded_config {
  std::vector<loaded_data_section> datafiles;
  std::vector<loaded_record_section> recordfiles;
  std::vector<loaded_block_section> blockfiles;
  std::vector<loaded_unfinished_record> unfinished;
  loaded_general_info info;
  std::uint64_t file_size;
  std::uint64_t version;
  std::uint32_t section_alignment;
};

std::vector<std::byte> to_bytes(
    loaded_general_info info,
    const std::vector<loaded_data_section>& datafiles,
    const std::vector<loaded_record_section>& recordfiles,
    const std::vector<loaded_block_section>& blockfiles,
    const std::vector<loaded_unfinished_record>& unfinished);

std::optional<loaded_config> from_bytes(const std::vector<std::byte>& bytes);
}  // namespace dedup::config

#endif  // BAREOS_STORED_BACKENDS_DEDUP_DEDUP_CONFIG_H_
