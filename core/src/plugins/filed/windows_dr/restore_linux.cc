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

#include "CLI/App.hpp"
#include "CLI/Config.hpp"
#include "CLI/Formatter.hpp"

#include <stdexcept>
#include <fmt/format.h>
#include <variant>
#include <optional>
#include <limits>

#include <gsl/narrow>

#include <zlib.h>

#include <fcntl.h>
#include <unistd.h>

struct zlib_crc {
  template <typename T> zlib_crc& update(std::span<const T> data)
  {
    crc = crc32_z(crc, reinterpret_cast<const unsigned char*>(data.data()),
                  data.size() * sizeof(T));
    return *this;
  }

  std::uint32_t get() const { return crc; }

 private:
  std::uint32_t crc = 0;
};

template <typename T> std::span<const char> as_span(const T& elem)
{
  return std::span{reinterpret_cast<const char*>(&elem), sizeof(T)};
}

struct disk_geometry {
  std::size_t cylinders;
  std::size_t tracks_per_cylinder;
  std::size_t sectors_per_track;
  std::size_t bytes_per_sector;

  constexpr std::size_t cylinder_size_in_bytes() const
  {
    return tracks_per_cylinder * sectors_per_track * bytes_per_sector;
  }
};

disk_geometry geometry_for_size(std::size_t size)
{
  static constexpr disk_geometry default_geometry = {
      .cylinders = 0,
      .tracks_per_cylinder = 255,
      .sectors_per_track = 63,
      .bytes_per_sector = 512,
  };

  static constexpr std::size_t cylinder_size
      = default_geometry.cylinder_size_in_bytes();

  auto cylinder_count = size / cylinder_size;

  disk_geometry result = default_geometry;
  result.cylinders = cylinder_count;
  return result;
}

namespace partition_table {
struct chs {
  // chs = cylinder, head, sector
  uint8_t data[3]{};

  struct for_mbr {};
  struct for_gpt {};

  chs() = default;

  chs(std::uint16_t cylinder,  // 10 bits
      std::uint8_t head,       // 8 bits
      std::uint8_t sector      // 6 bits
  )
  {
    data[0] = head;
    data[1] = sector;
    data[1] |= ((cylinder >> 8) & 0b11) << 6;
    data[2] = (cylinder & 0b11111111);
  }
  chs(for_mbr,
      std::uint64_t cylinder,  // 10 bits
      std::uint8_t head,       // 8 bits
      std::uint8_t sector      // 6 bits
      )
      : chs(cylinder >= (1 << 10) ? (1 << 10) - 1 : cylinder, head, sector)
  {
  }
  chs(for_gpt,
      std::uint64_t cylinder,  // 10 bits
      std::uint8_t head,       // 8 bits
      std::uint8_t sector      // 6 bits
      )
      : chs(cylinder >= (1 << 10) ? (cylinder & ((1 << 10) - 1)) : cylinder,
            head,
            sector)
  {
  }

  static chs from_lba(std::uint64_t lba, bool mbr, disk_geometry geo)
  {
    auto make = [mbr](auto c, auto h, auto s) {
      if (mbr) {
        return chs(for_mbr{}, c, h, s);
      } else {
        return chs(for_gpt{}, c, h, s);
      }
    };

    // lba = ((cylinder * geo.TracksPerCylinder) + head) * geo.SectorsPerTrack
    //             + (sector - 1)

    // sectors start at 1!
    std::uint64_t sector = lba % geo.sectors_per_track + 1;
    auto rest = lba / geo.sectors_per_track;

    std::uint64_t head = rest % geo.tracks_per_cylinder;
    std::uint64_t cylinder = rest / geo.tracks_per_cylinder;

    // valid chs is:
    // 0 <= cylinders <  Cylinders
    // 0 <=   head    <  TracksPerCylinder (255)
    // 0 <   sector   <= SectorsPerTrack (63)

    if (sector > geo.sectors_per_track || head >= geo.tracks_per_cylinder
        || cylinder >= (std::uint64_t)geo.cylinders) {
      // offset is too large, so just return the largest one
      return make(geo.cylinders - 1,
                  (std::uint8_t)(geo.tracks_per_cylinder - 1),
                  (std::uint8_t)(geo.sectors_per_track));
    }

    return make(cylinder, (std::uint8_t)head, (std::uint8_t)sector);
  }
};

static_assert(sizeof(chs) == 3);

struct mbr_entry {
  enum attrs : std::uint8_t
  {
    None = 0,
    Bootable = (1 << 7),
  };

  attrs attributes = {};
  chs start_chs = {};
  uint8_t os_type = {};
  chs end_chs = {};
  uint32_t start_lba = {};
  uint32_t lba_count = {};
};

static_assert(sizeof(mbr_entry) == 16);

struct mbr_table {
  using entry_storage = char[sizeof(mbr_entry)];
  char bootstrap[446] = {};
  entry_storage entries[4] = {};
  uint8_t check[2] = {0x55, 0xAA};
};

static_assert(sizeof(mbr_table) == 512);

struct gpt_entry {
  guid partition_type;
  guid id;
  uint64_t start_lba;
  uint64_t end_lba;  // inclusive!
  uint64_t attributes;
  char16_t name[36];
};

static_assert(sizeof(gpt_entry) == 128);

static constexpr std::size_t gpt_header_size = 92;

struct gpt_header {
  char signature[8] = {'E', 'F', 'I', ' ', 'P', 'A', 'R', 'T'};
  uint32_t revision = 1 << 16;
  uint32_t header_size = gpt_header_size;
  uint32_t crc32_sum = 0;
  uint32_t reserved_0 = 0;

  uint64_t lba_location;
  uint64_t backup_lba_location;
  uint64_t first_usable_lba;
  uint64_t last_usable_lba;

  guid disk_guid;

  uint64_t table_lba_location;

  uint32_t partition_count;
  uint32_t entry_size = sizeof(gpt_entry);
  uint32_t table_crc32_sum;

  char reserved[512 - 92] = {};
};

static_assert(sizeof(gpt_header) == 512);

enum class partition_type
{
  Mbr,
  Gpt
};

mbr_table make_protective_mbr(const partition_info_gpt& gpt, disk_geometry geo)
{
  static constexpr uint8_t gpt_type = 0xEE;

  std::size_t end_lba = (gpt.StartingUsableOffset + gpt.UsableLength) / 512 - 1;

  mbr_entry gpt_entry = {
      .attributes = mbr_entry::attrs::None,
      .start_chs = chs::from_lba(1, false, geo),
      .os_type = gpt_type,
      .end_chs = chs::from_lba(end_lba, false, geo),
      .start_lba = 1,

  // windows does not comply with the spec here, so we do it the windows
  // way by default
#ifdef COMPLY_WITH_SPEC
      .lba_count = (std::uint32_t)std::min(
          end_lba, (std::uint64_t)std::numeric_limits<std::uint32_t>::max()),
#else
      .lba_count = std::numeric_limits<std::uint32_t>::max(),
#endif
  };

  mbr_table table = {};


  static_assert(
      std::is_same_v<decltype(gpt.bootstrap), decltype(table.bootstrap)>);

  std::memcpy(table.bootstrap, gpt.bootstrap, sizeof(gpt.bootstrap));

  std::memcpy(table.entries[0], &gpt_entry, sizeof(gpt_entry));

  return table;
}

static constexpr std::size_t protective_mbr_entry_count = 1;


class Parser {
 public:
  struct result {
    std::vector<char> header;
    std::vector<char> footer;
    // this is always equal to disk_size - footer.size()
    std::size_t footer_offset;
  };

  Parser(disk_geometry geometry_, std::size_t disk_size_)
      : geometry{geometry_}, disk_size{disk_size_}
  {
    if (geometry.bytes_per_sector != 512) {
      throw std::logic_error{
          "weird disks are currently not supported (sector size != 512)"};
    }
  }

  void BeginMbrTable(const partition_info_mbr& mbr_info)
  {
    switch_state(state::INITED, state::STARTED);

    type = partition_type::Mbr;

    static_assert(
        std::is_same_v<decltype(mbr.bootstrap), decltype(mbr_info.bootstrap)>);

    std::memcpy(mbr.bootstrap, mbr_info.bootstrap, sizeof(mbr_info.bootstrap));
  }
  void BeginGptTable(const partition_info_gpt& gpt_info)
  {
    switch_state(state::INITED, state::STARTED);

    type = partition_type::Gpt;

    mbr = make_protective_mbr(gpt_info, geometry);
    mbr_entry_count = protective_mbr_entry_count;

    gpt.disk_guid = gpt_info.DiskId;
    gpt.max_part_count = gpt_info.MaxPartitionCount;
    gpt.first_usable_lba
        = gpt_info.StartingUsableOffset / geometry.bytes_per_sector;
    gpt.last_usable_lba
        = (gpt_info.StartingUsableOffset + gpt_info.UsableLength)
              / geometry.bytes_per_sector
          - 1;

    if (gpt.max_part_count % 4 != 0) {
      // gpt entries should fit neatly into sectors
      // one entry is 128bytes, a sector is 512 bytes, so its required
      // that the partition count is divisible by 512/128 = 4
      throw std::logic_error{
          "max partition count of the gpt table needs to be divisible by 4"};
    }
  }
  void BeginRawTable(const partition_info_raw&)
  {
    throw std::invalid_argument{"raw partitioned disks are not supported yet"};
  }
  void MbrEntry(const part_table_entry& entry,
                const part_table_entry_mbr_data& data)
  {
    check_state(state::STARTED);

    if (mbr_entry_count >= 4) {
      throw std::logic_error("cannot have more than 4 mbr partitions");
    }

    static constexpr std::uint8_t PartitionTypeUnused = 0;
    if (data.partition_type != PartitionTypeUnused) {
      auto start_lba = gsl::narrow<std::uint32_t>(entry.partition_offset
                                                  / geometry.bytes_per_sector);
      auto lba_count = gsl::narrow<std::uint32_t>(entry.partition_length
                                                  / geometry.bytes_per_sector);

      if (lba_count == 0) {
        throw std::logic_error{"encountered mbr entry with 0 size."};
      }

      auto last_lba = start_lba + lba_count - 1;

      bool is_mbr = type == partition_type::Mbr;

      mbr_entry entry_data = {
          .attributes
          = data.bootable ? mbr_entry::attrs::Bootable : mbr_entry::attrs::None,
          .start_chs = chs::from_lba(start_lba, is_mbr, geometry),
          .os_type = data.partition_type,
          .end_chs = chs::from_lba(last_lba, is_mbr, geometry),
          .start_lba = start_lba,
          .lba_count = lba_count,
      };

      std::memcpy(mbr.entries[mbr_entry_count], &entry_data,
                  sizeof(entry_data));
    }

    mbr_entry_count += 1;
  }
  void GptEntry(const part_table_entry& entry,
                const part_table_entry_gpt_data& data)
  {
    check_state(state::STARTED);

    if (type != partition_type::Gpt) {
      throw std::logic_error{"I can only add gpt partitions to gpt disks"};
    }

    auto& created = gpt.entries.emplace_back();

    created.attributes = data.attributes;
    created.id = data.partition_id;
    created.partition_type = data.partition_type;

    static_assert(std::is_same_v<decltype(created.name), decltype(data.name)>);
    std::memcpy(created.name, data.name, sizeof(data.name));

    created.start_lba = entry.partition_offset / geometry.bytes_per_sector;
    created.end_lba = (entry.partition_offset + entry.partition_length)
                          / geometry.bytes_per_sector
                      - 1;
  }

  result Make()
  {
    check_state(state::STARTED);
    std::vector<char> header;
    std::vector<char> footer;

    auto append_bytes
        = [](std::vector<char>& buffer, std::span<const char> bytes) {
            buffer.insert(buffer.end(), bytes.begin(), bytes.end());
          };

    // WE _always_ start with the mbr
    append_bytes(header, as_span(mbr));

    if (type == partition_type::Gpt) {
      gpt.entries.resize(gpt.max_part_count);

      zlib_crc table_crc = {};

      auto table_bytes = std::span<const char>(
          reinterpret_cast<const char*>(gpt.entries.data()),
          gpt.entries.size() * sizeof(gpt.entries[0]));

      assert(table_bytes.size() % 512 == 0);

      table_crc.update(table_bytes);

      gpt_header main = main_header(table_crc);
      append_bytes(header, as_span(main));
      append_bytes(header, table_bytes);

      gpt_header backup = backup_header(table_crc);
      append_bytes(footer, table_bytes);
      append_bytes(footer, as_span(backup));
    }


    return {std::move(header), std::move(footer), disk_size - footer.size()};
  }

 private:
  enum class state
  {
    INITED,
    STARTED,
  };

  void check_state(state s)
  {
    if (current_state != s) {
      // TODO: better error message heer
      throw std::logic_error{
          "bad state switch inside part table parsing logic"};
    }
  }

  void switch_state(state from, state to)
  {
    check_state(from);

    current_state = to;
  }

  std::size_t lba_count() const
  {
    return disk_size / geometry.bytes_per_sector;
  }
  std::size_t gpt_table_lba_count() const { return gpt.max_part_count / 4; }

  gpt_header main_header(const zlib_crc& table_crc)
  {
    gpt_header header = {};
    header.table_crc32_sum = table_crc.get();
    header.lba_location = 1;
    header.backup_lba_location = lba_count() - 1;
    header.first_usable_lba = gpt.first_usable_lba;
    header.last_usable_lba = gpt.last_usable_lba;
    header.disk_guid = gpt.disk_guid;
    // the table follows immediatly afterwards
    header.table_lba_location = 2;
    header.partition_count = gpt.max_part_count;
    // the crc member is set to zero while computing the crc sum itself
    header.crc32_sum = 0;

    zlib_crc header_crc = {};
    header_crc.update(
        std::span{reinterpret_cast<const char*>(&header), gpt_header_size});
    header.crc32_sum = header_crc.get();

    return header;
  }

  gpt_header backup_header(const zlib_crc& table_crc)
  {
    gpt_header header = {};
    header.table_crc32_sum = table_crc.get();
    header.lba_location = lba_count() - 1;
    header.backup_lba_location = 1;
    header.first_usable_lba = gpt.first_usable_lba;
    header.last_usable_lba = gpt.last_usable_lba;
    header.disk_guid = gpt.disk_guid;
    header.table_lba_location = lba_count() - 1 - gpt_table_lba_count();
    header.partition_count = gpt.max_part_count;
    header.crc32_sum = 0;

    zlib_crc header_crc = {};
    header_crc.update(
        std::span{reinterpret_cast<const char*>(&header), gpt_header_size});
    header.crc32_sum = header_crc.get();

    return header;
  }

  disk_geometry geometry;
  std::size_t disk_size;
  state current_state = state::INITED;

  partition_type type;

  // MBR data
  std::size_t mbr_entry_count = 0;
  mbr_table mbr = {};

  // GPT data
  struct {
    guid disk_guid;
    std::size_t max_part_count;
    std::size_t first_usable_lba;
    std::size_t last_usable_lba;
    std::vector<gpt_entry> entries;
  } gpt = {};
};
};  // namespace partition_table

class Output {
 public:
  virtual void append(std::span<const char> bytes) = 0;
  virtual void skip_forwards(std::size_t offset) = 0;
  virtual std::size_t current_offset() = 0;
};

class StreamOutput : public Output {
 public:
  // the stream is assumed to be unseekable!
  // this is not efficient if the stream is seekable instead
  StreamOutput(std::ostream& stream) : internal{&stream} {}

  void append(std::span<const char> bytes) override
  {
    internal->write(bytes.data(), bytes.size());
    if (internal->fail() || internal->bad()) {
      throw std::runtime_error{"could not write all data to stream"};
    }
    current_offset_ += bytes.size();
  }
  void skip_forwards(std::size_t offset) override
  {
    if (offset < current_offset_) {
      throw std::logic_error{
          std::format("Trying to skip to offset {}, when already at offset {}",
                      offset, current_offset_)};
    }

    while (current_offset_ < offset) {
      auto diff = offset - current_offset_;
      if (diff > nothing_size) { diff = nothing_size; }
      append(std::span{nothing.get(), diff});
    }
  }

  std::size_t current_offset() override { return current_offset_; }

 private:
  std::size_t current_offset_ = 0;
  std::ostream* internal = nullptr;

  static constexpr std::size_t nothing_size = 1024 * 1024;
  std::unique_ptr<char[]> nothing = std::make_unique<char[]>(nothing_size);
};

struct auto_fd {
  int fd = -1;

  auto_fd() = default;
  explicit auto_fd(int fd_) : fd{fd_} {}

  auto_fd(const auto_fd&) = delete;
  auto_fd& operator=(const auto_fd&) = delete;

  auto_fd(auto_fd&& other) : fd{other.fd} { other.fd = -1; }
  auto_fd& operator=(auto_fd&& other)
  {
    int temp = fd;
    fd = other.fd;
    other.fd = temp;

    return *this;
  }

  operator bool() const { return fd >= 0; }

  int get() { return fd; }

  ~auto_fd()
  {
    if (fd >= 0) { close(fd); }
  }
};

struct FileOutput : public Output {
  FileOutput(int fd, std::size_t size) : fd_{fd}, size_{size}
  {
    internal_offset_ = tell();
  }

  void append(std::span<const char> bytes) override
  {
    if (current_offset_ + bytes.size() > size_) {
      throw std::logic_error{"can not write past the end of the file"};
    }

    write(bytes);
    current_offset_ += bytes.size();
  }
  void skip_forwards(std::size_t offset) override
  {
    if (offset < current_offset_) {
      throw std::logic_error{
          std::format("Trying to skip to offset {}, when already at offset {}",
                      offset, current_offset_)};
    }

    if (offset > size_) {
      throw std::logic_error{"can not seek past the end of the file"};
    }

    seek(internal_offset_ + offset);
  }

  std::size_t current_offset() override { return current_offset_; }

 private:
  std::size_t tell()
  {
    off_t res = lseek(fd_, 0, SEEK_CUR);
    if (res < 0) {
      throw std::runtime_error{std::format(
          "could not determine file position: {}", strerror(errno))};
    }

    return static_cast<size_t>(res);
  }

  void seek(std::size_t offset)
  {
    auto s_offset = gsl::narrow<off_t>(offset);

    off_t res = lseek(fd_, s_offset, SEEK_SET);
    if (res < 0) {
      throw std::runtime_error{
          std::format("could not seek to {}: {}", offset, strerror(errno))};
    }
  }

  void write(std::span<const char> bytes)
  {
    auto towrite = bytes;

    std::size_t bad_count = 0;

    while (towrite.size() > 0) {
      auto res = ::write(fd_, towrite.data(), towrite.size());
      if (res < 0) {
        throw std::runtime_error(
            std::format("write() failed: {}", strerror(errno)));
      }
      if (res == 0) {
        bad_count += 1;
        if (bad_count > 20) {
          throw std::runtime_error(std::format(
              "write() failed to write data too often: {}", strerror(errno)));
        }
      }

      towrite = towrite.subspan(res);
    }
  }

  std::size_t current_offset_ = 0;
  int fd_ = -1;
  std::size_t size_ = 0;
  std::size_t internal_offset_ = 0;
};

namespace disk {
class Parser {
 public:
  Parser(disk_geometry disk_geometry_, std::size_t disk_size_, Output* output_)
      : geometry{disk_geometry_}
      , disk_size{disk_size_}
      , output{output_}
      , part_table{geometry, disk_size}
  {
  }

  void Finish()
  {
    output->skip_forwards(footer_offset);
    output->append(std::span{footer});
  }

  void BeginMbrTable(const partition_info_mbr& mbr)
  {
    part_table.BeginMbrTable(mbr);
  }

  void BeginGptTable(const partition_info_gpt& gpt)
  {
    part_table.BeginGptTable(gpt);
  }

  void BeginRawTable(const partition_info_raw& raw)
  {
    part_table.BeginRawTable(raw);
  }

  void MbrEntry(const part_table_entry& entry,
                const part_table_entry_mbr_data& data)
  {
    part_table.MbrEntry(entry, data);
  }

  void GptEntry(const part_table_entry& entry,
                const part_table_entry_gpt_data& data)
  {
    part_table.GptEntry(entry, data);
  }

  void EndPartTable()
  {
    if (output->current_offset() != 0) {
      throw std::logic_error(
          "I can not output partition table when not at start of output");
    }

    auto result = part_table.Make();

    output->append(result.header);

    footer = std::move(result.footer);
    footer_offset = result.footer_offset;
  }

  void BeginExtent(extent_header header)
  {
    if (current_extent_end != 0) {
      throw std::logic_error{
          "cannot start next extent if current one is not done yet"};
    }
    current_extent_end = header.offset + header.length;
    output->skip_forwards(header.offset);
  }
  void ExtentData(std::span<const char> data) { output->append(data); }
  void EndExtent()
  {
    output->skip_forwards(current_extent_end);
    current_extent_end = 0;
  }

 private:
  disk_geometry geometry;
  std::size_t disk_size;
  Output* output;

  partition_table::Parser part_table;

  std::size_t current_extent_end;

  std::size_t footer_offset;
  std::vector<char> footer;
};
};  // namespace disk


class RestoreToStdout : public GenericHandler {
 public:
  RestoreToStdout(std::ostream& stream) : output{stream} {}

  void BeginRestore(std::size_t num_disks) override
  {
    if (num_disks != 1) {
      throw std::invalid_argument{
          "I can only restore dumps with a single disk to stdout"};
    }
  }

  void EndRestore() override {}
  void BeginDisk(disk_info info) override
  {
    begin_disk(geometry_for_size(info.disk_size), info.disk_size, &output);
  }
  void EndDisk() override { disk().Finish(); }

  void BeginMbrTable(const partition_info_mbr& mbr) override
  {
    disk().BeginMbrTable(mbr);
  }

  void BeginGptTable(const partition_info_gpt& gpt) override
  {
    disk().BeginGptTable(gpt);
  }

  void BeginRawTable(const partition_info_raw& raw) override
  {
    disk().BeginRawTable(raw);
  }

  void MbrEntry(const part_table_entry& entry,
                const part_table_entry_mbr_data& data) override
  {
    disk().MbrEntry(entry, data);
  }

  void GptEntry(const part_table_entry& entry,
                const part_table_entry_gpt_data& data) override
  {
    disk().GptEntry(entry, data);
  }

  void EndPartTable() override { disk().EndPartTable(); }

  void BeginExtent(extent_header header) override
  {
    disk().BeginExtent(header);
  }
  void ExtentData(std::span<const char> data) override
  {
    disk().ExtentData(data);
  }
  void EndExtent() override { disk().EndExtent(); }

 private:
  disk::Parser& disk()
  {
    if (!disk_) {
      throw std::logic_error{"cannot access disk before it is created"};
    }

    return disk_.value();
  }

  template <typename... Args> disk::Parser& begin_disk(Args&&... args)
  {
    if (disk_) {
      throw std::logic_error{"cannot begin disk after one was created"};
    }

    return disk_.emplace(std::forward<Args>(args)...);
  }

  StreamOutput output;
  std::optional<disk::Parser> disk_;
};

class RestoreToFiles : public GenericHandler {
 public:
  RestoreToFiles(std::filesystem::path directory)
  {
    directory_fd = auto_fd{open(directory.c_str(), O_DIRECTORY | O_RDONLY)};
    if (!directory_fd) {
      throw std::runtime_error(std::format("cannot open '{}': {}",
                                           directory.c_str(), strerror(errno)));
    }
  }

  void BeginRestore(std::size_t num_disks) override
  {
    disk_files.reserve(num_disks);
    for (std::size_t idx = 0; idx < num_disks; ++idx) {
      auto disk_name = std::format("disk-{}.raw", idx);

      auto_fd fd{openat(directory_fd.get(), disk_name.c_str(),
                        O_CREAT | O_WRONLY | O_EXCL, 0664)};

      if (!fd) {
        throw std::runtime_error{
            std::format("could not open '{}': {}", disk_name, strerror(errno))};
      }

      disk_files.emplace_back(std::move(fd));
    }
  }

  void EndRestore() override {}
  void BeginDisk(disk_info info) override
  {
    begin_disk(geometry_for_size(info.disk_size), info.disk_size);
  }
  void EndDisk() override
  {
    disk().Finish();
    end_disk();
  }

  void BeginMbrTable(const partition_info_mbr& mbr) override
  {
    disk().BeginMbrTable(mbr);
  }

  void BeginGptTable(const partition_info_gpt& gpt) override
  {
    disk().BeginGptTable(gpt);
  }

  void BeginRawTable(const partition_info_raw& raw) override
  {
    disk().BeginRawTable(raw);
  }

  void MbrEntry(const part_table_entry& entry,
                const part_table_entry_mbr_data& data) override
  {
    disk().MbrEntry(entry, data);
  }

  void GptEntry(const part_table_entry& entry,
                const part_table_entry_gpt_data& data) override
  {
    disk().GptEntry(entry, data);
  }

  void EndPartTable() override { disk().EndPartTable(); }

  void BeginExtent(extent_header header) override
  {
    disk().BeginExtent(header);
  }
  void ExtentData(std::span<const char> data) override
  {
    disk().ExtentData(data);
  }
  void EndExtent() override { disk().EndExtent(); }

 private:
  disk::Parser& disk()
  {
    if (!current_disk) {
      throw std::logic_error{"cannot access disk before it is created"};
    }

    return current_disk->disk;
  }

  template <typename... Args>
  disk::Parser& begin_disk(disk_geometry geometry, std::size_t disk_size)
  {
    if (current_disk) {
      throw std::logic_error{"cannot begin disk after one was created"};
    }

    auto& fd = disk_files[current_idx];

    ftruncate(fd.get(), disk_size);

    auto& res = current_disk.emplace(fd.get(), geometry, disk_size);
    return res.disk;
  }

  void end_disk()
  {
    if (!current_disk) {
      throw std::logic_error{"cannot access disk before it is created"};
    }

    current_disk.reset();
    current_idx += 1;
  }

  struct writing_disk {
    FileOutput output;
    disk::Parser disk;

    writing_disk(int fd, disk_geometry geometry, std::size_t disk_size)
        : output{fd, disk_size}, disk{geometry, disk_size, &output}
    {
    }
  };

  auto_fd directory_fd;
  std::size_t current_idx = 0;
  std::vector<auto_fd> disk_files;
  std::optional<writing_disk> current_disk;
};

#if 0
struct RestoreToFiles : RestoreStrategy {
  void BeginRestore(std::size_t num_disks) override { (void) num_disks; }
  void EndRestore() override {}
  void BeginDisk(disk_info info) override { (void) info; }
  void EndDisk() override {}

  void BeginMbrTable(const partition_info_mbr& mbr) override { }
  void BeginGptTable(const partition_info_gpt& gpt) override {}
  void BeginRawTable(const partition_info_raw& gpt) override {}
  void MbrEntry(const part_table_entry& entry,
                const part_table_entry_mbr_data& data) override
  {
  }
  void GptEntry(const part_table_entry& entry,
                const part_table_entry_gpt_data& data) override
  {
  }
  void EndPartTable() override {}

  void BeginExtent(extent_header header) override {}
  void ExtentData(std::span<const char> data) override {}
  void EndExtent() override {}
};
#endif

void restore_data(std::istream& input, bool use_stdout)
{
  if (use_stdout) {
    RestoreToStdout strategy{std::cout};
    parse_file_format(input, &strategy);
    std::cout.flush();
  } else {
    RestoreToFiles strategy{""};
    parse_file_format(input, &strategy);
  }
}


int main(int argc, char* argv[])
{
  CLI::App app;

  auto* restore = app.add_subcommand("restore");
  std::string filename;
  auto* from = restore
                   ->add_option("--from", filename,
                                "read from this file instead of stdin")
                   ->check(CLI::ExistingFile);

  auto* location = restore->add_option_group(
      "output", "select where the data will be restored");

  auto* stdout = location->add_flag(
      "--stdout", "restore the single disk to stdout (for piping purposes)");

  std::string directory;
  auto* files = location
                    ->add_option(
                        "--into", directory,
                        "restore the disks to raw files in the given directory")
                    ->check(CLI::ExistingDirectory);

  files->excludes(stdout);

  location->require_option();

  app.require_subcommand(1, 1);

  CLI11_PARSE(app, argc, argv);
  try {
    if (*restore) {
      std::unique_ptr<GenericHandler> strategy;

      if (*stdout) {
        strategy = std::make_unique<RestoreToStdout>(std::cout);
      } else if (*from) {
        strategy = std::make_unique<RestoreToFiles>(directory);
      } else {
        throw std::logic_error("i dont know where to restore too!");
      }

      if (*from) {
        fprintf(stderr, "using %s as input\n", filename.c_str());
        std::ifstream infile{filename,
                             std::ios_base::in | std::ios_base::binary};
        parse_file_format(infile, strategy.get());
      } else {
        parse_file_format(std::cin, strategy.get());
      }
    } else {
      throw std::logic_error("i dont know what to do");
    }

    return 0;
  } catch (const std::exception& ex) {
    std::cerr << ex.what() << std::endl;

    return 1;
  }
}
