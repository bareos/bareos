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
struct section_header {
  enum class types : std::uint16_t
  {
    GeneralInfo = 0,
    BlockFile = 1,
    RecordFile = 2,
    DataFile = 3,
  };
  // total section size = header size + data size + padding
  // padding is between header & data
  net_u16 type;
  net_u16 version;
  net_u32
      padding;  // the amount of padding between the header and start of data

  net_u32 data_size;
  net_u32 reserved_0{0};

  net_u64 data_checksum;
  net_u64 checksum;  // checksum for the header; does not include itsef
                     // this _has_ to be the last member!
};

static_assert(sizeof(section_header) == 32);

struct general_section {
  enum class byte_order : std::uint8_t
  {
    little_endian = 1,
    big_endian = 2
  };

  net_u32 block_header_size;
  net_u32 section_header_size;
  net_u8 written_order;  // order of values inside file
};

struct block_file_section {
  net_u32 file_index;
  net_u32 start_block;
  net_u32 end_block;
  net_u32 path_length;
  // the path data follows directly without any padding
  // as if
  // std::byte path_data[]; (in utf8 encoding)
  // was the next member (not legal in C++)
};

struct record_file_section {
  net_u32 file_index;
  net_u32 start_record;
  net_u32 end_record;
  net_u32 path_length;
  // the path data follows directly without any padding
  // as if
  // std::byte path_data[]; (in utf8 encoding)
  // was the next member (not legal in C++)
};

struct data_file_section {
  net_i64 block_size;
  net_u32 file_index;
  net_u32 path_length;
  // the path data follows directly without any padding
  // as if
  // std::byte path_data[]; (in utf8 encoding)
  // was the next member (not legal in C++)
};

}  // namespace dedup::config

#endif  // BAREOS_STORED_BACKENDS_DEDUP_DEDUP_CONFIG_H_
