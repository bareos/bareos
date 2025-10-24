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

#ifndef BAREOS_PLUGINS_FILED_WINDOWS_DR_FILE_FORMAT_H_
#define BAREOS_PLUGINS_FILED_WINDOWS_DR_FILE_FORMAT_H_

#include <cstdint>

#include <vector>
#include <variant>
#include <cassert>
#include <cstring>
#include <span>
#include <cstdint>
#include <limits>

#include <lib/source_location.h>

#include "format.h"

struct reader {
  // reads exactly size bytes into buffer
  virtual void read(char* buffer, std::size_t size) = 0;
  virtual ~reader() {}
};

struct writer {
  virtual void write(const char* buffer, std::size_t size) = 0;
  virtual ~writer() {};
};

static_assert(sizeof(char16_t) == 2);

// TODO: we want to change the format slightly:
//   instead of storing the data depth first, we want to store it breadth first
//   i.e. first should come _all_ meta data, and only then there should be
//   actual data.
//   this makes it easier for tools to understand what they are supposed to do!

struct guid {
  char Data[16];

  constexpr auto operator<=>(const guid&) const = default;
};

struct partition_info_raw {};
struct partition_info_mbr {
  uint32_t CheckSum;
  uint32_t Signature;
  char bootstrap[446];
};
struct partition_info_gpt {
  guid DiskId;
  uint64_t StartingUsableOffset;
  uint64_t UsableLength;
  uint32_t MaxPartitionCount;
  char bootstrap[446];
};

using partition_info
    = std::variant<partition_info_raw, partition_info_mbr, partition_info_gpt>;

template <std::size_t N>
static constexpr uint64_t build_magic(const char (&str)[N])
{
  static_assert(N <= 9);
  static_assert(N > 0);

  uint64_t value = 0;
  for (std::size_t i = 0; i < (N - 1); ++i) {
    value = (value << 8) | str[(N - 2) - i];
  }

  return value;
}

struct decoded_version {
  uint16_t major{};
  uint8_t minor{};
  uint8_t patch{};

  constexpr decoded_version() = default;

  constexpr decoded_version(uint16_t major_, uint8_t minor_, uint8_t patch_)
      : major{major_}, minor{minor_}, patch{patch_}
  {
  }

  constexpr decoded_version(std::uint32_t encoded)
      : major{static_cast<std::uint16_t>(encoded >> 16)}
      , minor{static_cast<std::uint8_t>(encoded >> 8)}
      , patch{static_cast<std::uint8_t>(encoded >> 0)}
  {
  }

  constexpr std::uint32_t encode() const
  {
    uint32_t major32 = major, minor32 = minor, patch32 = patch;
    return major32 << 16 | minor32 << 8 | patch32;
  }

  constexpr operator uint32_t() { return encode(); }

  constexpr auto operator<=>(const decoded_version&) const = default;
};

static constexpr bool test_decode(decoded_version v)
{
  return v == decoded_version{static_cast<std::uint32_t>(v)};
}

static constexpr bool test_encode(std::uint32_t v)
{
  return v == decoded_version{v}.encode();
}

static_assert(test_encode(15));
static_assert(test_encode(std::numeric_limits<std::uint32_t>::max()));
static_assert(test_decode(decoded_version{1, 0, 0}));
static_assert(test_decode(decoded_version{1, 1, 0}));
static_assert(test_decode(decoded_version{1, 0, 1}));
static_assert(test_decode(decoded_version{1, 1, 1}));
static_assert(test_decode(decoded_version{255, 0, 0}));
static_assert(test_decode(decoded_version{255, 255, 0}));
static_assert(test_decode(decoded_version{255, 0, 255}));
static_assert(test_decode(decoded_version{255, 255, 255}));

template <typename T> static void write_stream(writer& stream, const T& t)
{
  stream.write(reinterpret_cast<const char*>(&t), sizeof(t));
}

template <typename T> static void read_stream(reader& stream, T& t)
{
  stream.read(reinterpret_cast<char*>(&t), sizeof(t));
}

template <typename T>
static void expect_stream(reader& stream,
                          const T& expected,
                          libbareos::source_location expected_at
                          = libbareos::source_location::current())
{
  T value;
  read_stream(stream, value);

  if (value != expected) {
    throw std::runtime_error{libbareos::format(
        "{}:{} dump format error: expected constant {:0x}, got {:0x}",
        expected_at.file_name(), expected_at.line(), expected, value)};
  }
}

struct file_header {
  // barri - file
  static constexpr auto magic_value = build_magic("barrifil");
  static constexpr auto current_version = decoded_version{1, 0, 0}.encode();

  uint32_t disk_count{};
  uint32_t version{};
  // NOTE: this is the size of everything _but_ this header!
  uint64_t file_size{};
  // todo: we should add _all_ disk sizes here
  // this makes it easier for the restore code to decide to which disk
  // it should restore to!


  file_header() = default;
  file_header(uint32_t disk_count_, uint64_t file_size_)
      : disk_count{disk_count_}, version{current_version}, file_size{file_size_}
  {
  }

  void write(writer& stream) const
  {
    write_stream(stream, magic_value);
    write_stream(stream, version);
    write_stream(stream, disk_count);
    write_stream(stream, file_size);
  }

  void read(reader& stream)
  {
    expect_stream(stream, magic_value);
    read_stream(stream, version);
    read_stream(stream, disk_count);
    read_stream(stream, file_size);
  }

  constexpr auto operator<=>(const file_header&) const = default;
};

struct disk_header {
  // barri - disk
  static constexpr auto magic_value = build_magic("barridsk");

  uint64_t disk_size;
  uint64_t total_extent_size;
  uint32_t media_type;
  uint32_t bytes_per_sector;
  uint32_t extent_count;

  disk_header() = default;
  disk_header(uint64_t disk_size_,
              uint64_t total_extent_size_,
              uint32_t media_type_,
              uint32_t bytes_per_sector_,
              uint32_t extent_count_)
      : disk_size{disk_size_}
      , total_extent_size{total_extent_size_}
      , media_type{media_type_}
      , bytes_per_sector{bytes_per_sector_}
      , extent_count{extent_count_}
  {
  }

  void write(writer& stream) const
  {
    write_stream(stream, magic_value);
    write_stream(stream, disk_size);
    write_stream(stream, total_extent_size);
    write_stream(stream, media_type);
    write_stream(stream, bytes_per_sector);
    write_stream(stream, extent_count);
  }

  void read(reader& stream)
  {
    expect_stream(stream, magic_value);
    read_stream(stream, disk_size);
    read_stream(stream, total_extent_size);
    read_stream(stream, media_type);
    read_stream(stream, bytes_per_sector);
    read_stream(stream, extent_count);
  }

  constexpr auto operator<=>(const disk_header&) const = default;
};

enum part_type : uint8_t
{
  Raw = 0,
  Mbr = 1,
  Gpt = 2,
};

struct part_table_header {
  // barri - table
  static constexpr auto magic_value = build_magic("barritbl");
  uint32_t partition_count;
  uint8_t part_table_type;
  // at this point we should just divide this into header + data ...
  uint32_t Datum0;
  uint64_t Datum1;
  uint64_t Datum2;
  char Data[16];
  char Data2[446];

  part_table_header() = default;
  part_table_header(uint32_t partition_count_,
                    part_type part_type,
                    uint32_t Datum0_,
                    uint64_t Datum1_,
                    uint64_t Datum2_,
                    std::span<const char> Data_ = {},
                    std::span<const char> Data2_ = {})
      : partition_count{partition_count_}
      , part_table_type{part_type}
      , Datum0{Datum0_}
      , Datum1{Datum1_}
      , Datum2{Datum2_}
      , Data{}
      , Data2{}
  {
    assert(Data_.size() <= std::size(Data));
    if (Data_.size() > 0) { memcpy(Data, Data_.data(), Data_.size()); }
    assert(Data2_.size() <= std::size(Data2));
    if (Data2_.size() > 0) { memcpy(Data2, Data2_.data(), Data2_.size()); }
  }

  void write(writer& stream) const
  {
    write_stream(stream, magic_value);
    write_stream(stream, partition_count);
    write_stream(stream, part_table_type);
    write_stream(stream, Datum0);
    write_stream(stream, Datum1);
    write_stream(stream, Datum2);
    write_stream(stream, Data);
    write_stream(stream, Data2);
  }

  void read(reader& stream)
  {
    expect_stream(stream, magic_value);
    read_stream(stream, partition_count);
    read_stream(stream, part_table_type);
    read_stream(stream, Datum0);
    read_stream(stream, Datum1);
    read_stream(stream, Datum2);
    read_stream(stream, Data);
    read_stream(stream, Data2);
  }

  constexpr auto operator<=>(const part_table_header&) const = default;
};

struct part_table_entry {
  // barri - table entry
  static constexpr auto magic_value = build_magic("barritnt");

  uint64_t partition_offset;
  uint64_t partition_length;
  uint32_t partition_number;
  part_type partition_style;
  bool rewrite_partition;     // (useless ?)
  bool is_service_partition;  // (useless ?)

  part_table_entry() = default;
  part_table_entry(uint64_t partition_offset_,
                   uint64_t partition_length_,
                   uint32_t partition_number_,
                   part_type partition_style_,
                   bool rewrite_partition_,
                   bool is_service_partition_)
      : partition_offset{partition_offset_}
      , partition_length{partition_length_}
      , partition_number{partition_number_}
      , partition_style{partition_style_}
      , rewrite_partition{rewrite_partition_}
      , is_service_partition{is_service_partition_}
  {
  }

  void write(writer& stream) const
  {
    write_stream(stream, magic_value);
    write_stream(stream, partition_offset);
    write_stream(stream, partition_length);
    write_stream(stream, partition_number);
    write_stream(stream, partition_style);
    write_stream(stream, rewrite_partition);
    write_stream(stream, is_service_partition);
  }

  void read(reader& stream)
  {
    expect_stream(stream, magic_value);
    read_stream(stream, partition_offset);
    read_stream(stream, partition_length);
    read_stream(stream, partition_number);
    read_stream(stream, partition_style);
    read_stream(stream, rewrite_partition);
    read_stream(stream, is_service_partition);
  }

  constexpr auto operator<=>(const part_table_entry&) const = default;
};

static inline void write_guid(writer& stream, const guid& id)
{
  write_stream(stream, id.Data);
}

static inline void read_guid(reader& stream, guid& id)
{
  read_stream(stream, id.Data);
}

struct part_table_entry_gpt_data {
  struct part_name {
    char16_t data[36];

    constexpr auto operator<=>(const part_name&) const = default;
  };

  guid partition_type;
  guid partition_id;
  uint64_t attributes;
  part_name name;

  part_table_entry_gpt_data() = default;
  part_table_entry_gpt_data(guid partition_type_,
                            guid partition_id_,
                            uint64_t attributes_,
                            part_name name_)
      : partition_type{partition_type_}
      , partition_id{partition_id_}
      , attributes{attributes_}
      , name{name_}
  {
  }

  void write(writer& stream) const
  {
    write_guid(stream, partition_type);
    write_guid(stream, partition_id);
    write_stream(stream, attributes);
    write_stream(stream, name.data);
  }

  void read(reader& stream)
  {
    read_guid(stream, partition_type);
    read_guid(stream, partition_id);
    read_stream(stream, attributes);
    read_stream(stream, name.data);
  }

  constexpr auto operator<=>(const part_table_entry_gpt_data&) const = default;
};

struct part_table_entry_mbr_data {
  guid partition_id;
  uint32_t num_hidden_sectors;  // (useless ?)
  uint8_t partition_type;
  bool bootable;
  bool recognized;  // (useless ?)


  part_table_entry_mbr_data() = default;
  part_table_entry_mbr_data(guid partition_id_,
                            uint32_t num_hidden_sectors_,
                            uint8_t partition_type_,
                            bool bootable_,
                            bool recognized_)
      : partition_id{partition_id_}
      , num_hidden_sectors{num_hidden_sectors_}
      , partition_type{partition_type_}
      , bootable{bootable_}
      , recognized{recognized_}
  {
  }

  void write(writer& stream) const
  {
    write_guid(stream, partition_id);
    write_stream(stream, num_hidden_sectors);
    write_stream(stream, partition_type);
    write_stream(stream, bootable);
    write_stream(stream, recognized);
  }

  void read(reader& stream)
  {
    read_guid(stream, partition_id);
    read_stream(stream, num_hidden_sectors);
    read_stream(stream, partition_type);
    read_stream(stream, bootable);
    read_stream(stream, recognized);
  }

  constexpr auto operator<=>(const part_table_entry_mbr_data&) const = default;
};

struct extent_header {
  // barri - extent
  static constexpr auto magic_value = build_magic("barrixtn");

  uint64_t offset;
  uint64_t length;

  extent_header() = default;
  extent_header(uint64_t offset_, uint64_t length_)
      : offset{offset_}, length{length_}
  {
  }

  void write(writer& stream) const
  {
    write_stream(stream, magic_value);
    write_stream(stream, offset);
    write_stream(stream, length);
  }

  void read(reader& stream)
  {
    expect_stream(stream, magic_value);
    read_stream(stream, offset);
    read_stream(stream, length);
  }

  constexpr auto operator<=>(const extent_header&) const = default;
};

#endif  // BAREOS_PLUGINS_FILED_WINDOWS_DR_FILE_FORMAT_H_
