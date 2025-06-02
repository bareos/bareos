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
#include <istream>
#include <ostream>
#include <vector>
#include <variant>
#include <cassert>
#include <format>
#include <span>

struct guid {
  char Data[16];
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

template <typename T> static void write_stream(std::ostream& stream, const T& t)
{
  stream.write(reinterpret_cast<const char*>(&t), sizeof(t));
}

template <typename T> static void read_stream(std::istream& stream, T& t)
{
#if 0
  auto read_bytes = stream.sgetn(reinterpret_cast<char*>(&t), sizeof(t));
#else
  stream.read(reinterpret_cast<char*>(&t), sizeof(t));
  auto read_bytes = stream.gcount();
#endif
  fprintf(stderr, "read %lld/%llu bytes\n", read_bytes, sizeof(t));

  if (read_bytes != (std::streamsize)sizeof(t)) {
    fprintf(stderr, "fail: %s | good: %s | eof: %s | bad: %s\n",
            stream.fail() ? "yes" : "no", stream.good() ? "yes" : "no",
            stream.eof() ? "yes" : "no", stream.bad() ? "yes" : "no");
  }


  if constexpr (std::is_same_v<std::uint32_t, T>) {
    fprintf(stderr, "read %s\n", std::format("{}", t).c_str());
  }
}

template <typename T>
static void expect_stream(std::istream& stream, const T& expected)
{
  T value;
  read_stream(stream, value);

  assert(value == expected);
}

// badr <~> BAreos Disaster Recovery

struct file_header {
  static constexpr std::uint64_t magic_value = build_magic("badrfile");
  static constexpr std::uint32_t current_version = 0;

  uint32_t disk_count;
  uint32_t version = current_version;


  file_header() = default;
  file_header(uint32_t disk_count_) : disk_count{disk_count_} {}

  void write(std::ostream& stream) const
  {
    write_stream(stream, magic_value);
    write_stream(stream, disk_count);
    write_stream(stream, version);
  }

  void read(std::istream& stream)
  {
    expect_stream(stream, magic_value);
    read_stream(stream, disk_count);
    read_stream(stream, version);
  }
};

struct disk_header {
  static constexpr std::uint64_t magic_value = build_magic("badrdisk");

  uint64_t disk_size;
  uint32_t media_type;
  uint32_t bytes_per_sector;
  uint32_t extent_count;

  disk_header() = default;
  disk_header(uint64_t disk_size_,
              uint32_t media_type_,
              uint32_t bytes_per_sector_,
              uint32_t extent_count_)
      : disk_size{disk_size_}
      , media_type{media_type_}
      , bytes_per_sector{bytes_per_sector_}
      , extent_count{extent_count_}
  {
  }

  void write(std::ostream& stream) const
  {
    write_stream(stream, magic_value);
    write_stream(stream, disk_size);
    write_stream(stream, media_type);
    write_stream(stream, bytes_per_sector);
    write_stream(stream, extent_count);
  }

  void read(std::istream& stream)
  {
    expect_stream(stream, magic_value);
    read_stream(stream, disk_size);
    read_stream(stream, media_type);
    read_stream(stream, bytes_per_sector);
    read_stream(stream, extent_count);
  }
};

enum part_type : uint8_t
{
  Raw = 0,
  Mbr = 1,
  Gpt = 2,
};

struct part_table_header {
  static constexpr auto magic_value = build_magic("badrtabl");
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
    memcpy(Data, Data_.data(), Data_.size());
    assert(Data2_.size() <= std::size(Data2));
    memcpy(Data2, Data2_.data(), Data2_.size());
  }

  void write(std::ostream& stream) const
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

  void read(std::istream& stream)
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
};

struct part_table_entry {
  static constexpr auto magic_value = build_magic("badrtent");

  uint64_t partition_offset;
  uint64_t partition_length;
  uint32_t partition_number;
  uint8_t partition_style;
  bool rewrite_partition;     // (useless ?)
  bool is_service_partition;  // (useless ?)

  part_table_entry() = default;

  void write(std::ostream& stream) const
  {
    write_stream(stream, magic_value);
    write_stream(stream, partition_offset);
    write_stream(stream, partition_length);
    write_stream(stream, partition_number);
    write_stream(stream, partition_style);
    write_stream(stream, rewrite_partition);
    write_stream(stream, is_service_partition);
  }

  void read(std::istream& stream)
  {
    expect_stream(stream, magic_value);
    read_stream(stream, partition_offset);
    read_stream(stream, partition_length);
    read_stream(stream, partition_number);
    read_stream(stream, partition_style);
    read_stream(stream, rewrite_partition);
    read_stream(stream, is_service_partition);
  }
};

static inline void write_guid(std::ostream& stream, const guid& id)
{
  write_stream(stream, id.Data);
}

static inline void read_guid(std::istream& stream, guid& id)
{
  read_stream(stream, id.Data);
}

struct part_table_entry_gpt_data {
  guid partition_type;
  guid partition_id;
  uint64_t attributes;
  wchar_t name[36];

  part_table_entry_gpt_data() = default;

  void write(std::ostream& stream) const
  {
    write_guid(stream, partition_type);
    write_guid(stream, partition_id);
    write_stream(stream, attributes);
    write_stream(stream, name);
  }

  void read(std::istream& stream)
  {
    read_guid(stream, partition_type);
    read_guid(stream, partition_id);
    read_stream(stream, attributes);
    read_stream(stream, name);
  }
};

struct part_table_entry_mbr_data {
  guid partition_id;
  uint32_t num_hidden_sectors;  // (useless ?)
  uint8_t partition_type;
  bool bootable;
  bool recognized;  // (useless ?)


  part_table_entry_mbr_data() = default;

  void write(std::ostream& stream) const
  {
    write_guid(stream, partition_id);
    write_stream(stream, num_hidden_sectors);
    write_stream(stream, partition_type);
    write_stream(stream, bootable);
    write_stream(stream, recognized);
  }

  void read(std::istream& stream)
  {
    read_guid(stream, partition_id);
    read_stream(stream, num_hidden_sectors);
    read_stream(stream, partition_type);
    read_stream(stream, bootable);
    read_stream(stream, recognized);
  }
};

struct extent_header {
  static constexpr auto magic_value = build_magic("badrxtnt");

  uint64_t offset;
  uint64_t length;

  extent_header() = default;

  void write(std::ostream& stream) const
  {
    write_stream(stream, magic_value);
    write_stream(stream, offset);
    write_stream(stream, length);
  }

  void read(std::istream& stream)
  {
    expect_stream(stream, magic_value);
    read_stream(stream, offset);
    read_stream(stream, length);
  }
};

#endif  // BAREOS_PLUGINS_FILED_WINDOWS_DR_FILE_FORMAT_H_
