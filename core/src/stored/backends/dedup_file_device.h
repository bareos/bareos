/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2023-2023 Bareos GmbH & Co. KG

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation, which is
   listed in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   Affero General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
*/

#ifndef BAREOS_STORED_BACKENDS_DEDUP_FILE_DEVICE_H_
#define BAREOS_STORED_BACKENDS_DEDUP_FILE_DEVICE_H_

#include "stored/dev.h"
#include "lib/network_order.h"

#include <unordered_map>
#include <fcntl.h>
#include <optional>
#include <algorithm>
#include <utility>

namespace storagedaemon {
using net_u32 = network_order::network_value<std::uint32_t>;
using net_i32 = network_order::network_value<std::int32_t>;
using net_u64 = network_order::network_value<std::uint64_t>;
using net_u16 = network_order::network_value<std::uint16_t>;
using net_u8 = std::uint8_t;
using net_i64 = network_order::network_value<std::int64_t>;

struct bareos_block_header {
  net_u32 CheckSum;       /* Block check sum */
  net_u32 BlockSize;      /* Block byte size including the header */
  net_u32 BlockNumber;    /* Block number */
  char ID[4];             /* Identification and block level */
  net_u32 VolSessionId;   /* Session Id for Job */
  net_u32 VolSessionTime; /* Session Time for Job */
};

struct bareos_record_header {
  net_i32 FileIndex; /* File index supplied by File daemon */
  net_i32 Stream;    /* Stream number supplied by File daemon */
  net_u32 DataSize;  /* size of following data record in bytes */
};

struct write_buffer {
  char* current;
  char* end;

  write_buffer(char* data, std::size_t size)
      : current{static_cast<char*>(data)}, end{current + size}
  {
  }


  bool write(std::size_t size, const char* data)
  {
    if (current + size > end) { return false; }

    current = std::copy(data, data + size, current);
    return true;
  }

  char* reserve(std::size_t size)
  {
    if (current + size > end) { return nullptr; }

    return std::exchange(current, current + size);
  }

  template <typename F> inline bool write(const F& val)
  {
    return write(sizeof(F), reinterpret_cast<const char*>(&val));
  }
};

struct dedup_block_header {
  bareos_block_header BareosHeader;
  // change to 64bit start!
  net_u32 RecStart;
  net_u32 RecEnd;
  net_u32 file_index;

  dedup_block_header() = default;

  dedup_block_header(const bareos_block_header& base,
                     std::uint32_t RecStart,
                     std::uint32_t RecEnd,
                     std::uint32_t file_index)
      : BareosHeader(base)
      , RecStart{network_order::of_native(RecStart)}
      , RecEnd{network_order::of_native(RecEnd)}
      , file_index{network_order::of_native(file_index)}
  {
  }
};

static_assert(std::is_standard_layout_v<dedup_block_header>);
static_assert(std::is_pod_v<dedup_block_header>);
static_assert(std::has_unique_object_representations_v<dedup_block_header>);

struct dedup_record_header {
  bareos_record_header BareosHeader;
  net_u32 file_index;
  net_u64 DataStart;
  net_u64 DataEnd;

  dedup_record_header() = default;

  dedup_record_header(const bareos_record_header& base,
                      std::uint64_t DataStart,
                      std::uint64_t DataEnd,
                      std::uint32_t file_index)
      : BareosHeader(base)
      , file_index{file_index}
      , DataStart{network_order::of_native(DataStart)}
      , DataEnd{network_order::of_native(DataEnd)}
  {
  }
};

static_assert(std::is_standard_layout_v<dedup_record_header>);
static_assert(std::is_pod_v<dedup_record_header>);
static_assert(std::has_unique_object_representations_v<dedup_record_header>);

class raii_fd {
 public:
  raii_fd(const char* path, int flags, int mode) : flags{flags}, mode{mode}
  {
    fd = open(path, flags, mode);
  }

  raii_fd(int dird, const char* path, int flags, int mode)
      : flags{flags}, mode{mode}
  {
    fd = openat(dird, path, flags, mode);
  }
  raii_fd(int fd = -1, int flags = 0, int mode = 0)
      : fd{fd}, flags{flags}, mode{mode}
  {
  }

  raii_fd(raii_fd&& move_from) : raii_fd{} { *this = std::move(move_from); }

  raii_fd& operator=(raii_fd&& move_from)
  {
    std::swap(fd, move_from.fd);
    std::swap(flags, move_from.flags);
    std::swap(mode, move_from.mode);
    return *this;
  }

  bool is_ok() const { return !(fd < 0); }

  int get() const { return fd; }

  int get_flags() const { return flags; }

  int get_mode() const { return mode; }

  ~raii_fd()
  {
    if (is_ok()) { close(fd); }
  }

 private:
  int fd{-1};
  int flags;
  int mode;
};

struct dedup_volume_file {
  std::string path{};
  raii_fd fd{};

  dedup_volume_file() = default;
  dedup_volume_file(std::string_view path) : path{path} {}

  bool open_inside(raii_fd& dir, int mode, DeviceMode dev_mode)
  {
    int flags;
    switch (dev_mode) {
      case DeviceMode::CREATE_READ_WRITE: {
        flags = O_CREAT | O_RDWR | O_BINARY;
      } break;
      case DeviceMode::OPEN_READ_WRITE: {
        flags = O_RDWR | O_BINARY;
      } break;
      case DeviceMode::OPEN_READ_ONLY: {
        flags = O_RDONLY | O_BINARY;
      } break;
      case DeviceMode::OPEN_WRITE_ONLY: {
        flags = O_WRONLY | O_BINARY;
      } break;
      default: {
        return false;
      }
    }
    fd = raii_fd(dir.get(), path.c_str(), flags, mode);
    return is_ok();
  }

  bool truncate(std::size_t size = 0)
  {
    if (ftruncate(fd.get(), size) != 0) { return false; }
    return true;
  }

  bool goto_end()
  {
    if (lseek(fd.get(), 0, SEEK_END) < 0) { return false; }
    return true;
  }

  bool goto_begin(ssize_t offset = 0)
  {
    if (offset < 0) { return false; }
    if (lseek(fd.get(), offset, SEEK_SET) != offset) { return false; }
    return true;
  }

  std::optional<std::size_t> current_pos()
  {
    auto pos = lseek(fd.get(), 0, SEEK_CUR);
    if (pos < 0) return std::nullopt;
    return static_cast<std::size_t>(pos);
  }

  bool write(const void* data, std::size_t length)
  {
    return ::write(fd.get(), data, length) == static_cast<ssize_t>(length);
  }

  bool read(void* data, std::size_t length)
  {
    return ::read(fd.get(), data, length) == static_cast<ssize_t>(length);
  }

  bool is_ok() const { return fd.is_ok(); }
};

struct block_file : public dedup_volume_file {
  block_file() = default;
  block_file(std::string_view path,
             std::uint32_t file_index,
             std::uint32_t s,
             std::uint32_t e)
      : dedup_volume_file(path)
      , file_index{file_index}
      , start_block{s}
      , end_block{e}
      , current_block{0}
  {
  }
  std::uint32_t file_index{};
  std::uint32_t start_block{};
  std::uint32_t end_block{};
  std::uint32_t current_block{};

  bool truncate()
  {
    start_block = 0;
    end_block = 0;
    current_block = 0;
    return dedup_volume_file::truncate();
  }

  bool goto_end()
  {
    current_block = end_block;
    return dedup_volume_file::goto_end();
  }

  bool goto_begin()
  {
    current_block = 0;
    return dedup_volume_file::goto_begin();
  }

  bool goto_block(std::int64_t block)
  {
    if (current_block == block) { return true; }
    if (dedup_volume_file::goto_begin(block * sizeof(dedup_block_header))) {
      current_block = block;
      return true;
    }
    return false;
  }

  bool write(const bareos_block_header&,
             std::uint32_t,
             std::uint32_t,
             std::uint32_t);

  std::optional<dedup_block_header> read_block()
  {
    if (current_block == end_block) { return std::nullopt; }

    dedup_block_header result;
    if (!read(static_cast<void*>(&result), sizeof(result))) {
      return std::nullopt;
    }

    current_block += 1;
    return result;
  }
};

struct record_file : public dedup_volume_file {
  record_file() = default;
  record_file(std::string_view path,
              std::uint32_t file_index,
              std::uint32_t s,
              std::uint32_t e)
      : dedup_volume_file(path)
      , file_index{file_index}
      , start_record{s}
      , end_record{e}
      , current_record{0}
  {
  }
  std::uint32_t file_index{};
  std::uint32_t start_record{};
  std::uint32_t end_record{};
  std::uint32_t current_record{};

  bool truncate()
  {
    start_record = 0;
    end_record = 0;
    current_record = 0;
    return dedup_volume_file::truncate();
  }

  bool goto_end()
  {
    current_record = end_record;
    return dedup_volume_file::goto_end();
  }

  bool goto_begin()
  {
    current_record = 0;
    return dedup_volume_file::goto_begin();
  }

  bool goto_record(std::uint64_t record_idx)
  {
    if (current_record == record_idx) { return true; }

    if (dedup_volume_file::goto_begin(record_idx
                                      * sizeof(dedup_record_header))) {
      current_record = record_idx;
      return true;
    }
    return false;
  }

  std::uint32_t current() const { return current_record; }

  // should probably return std::optional<record idx>
  bool write(const bareos_record_header&,
             std::uint64_t,
             std::uint64_t,
             std::uint32_t);

  std::optional<dedup_record_header> read_record(std::int64_t)
  {
    // if (!goto_record(index)) {
    //   return std::nullopt;
    // }

    dedup_record_header record;
    if (!read(static_cast<void*>(&record), sizeof(record))) {
      return std::nullopt;
    }

    current_record += 1;
    return record;
  }
};

struct data_file : public dedup_volume_file {
  data_file() = default;
  data_file(std::string_view path,
            std::uint32_t file_index,
            std::int64_t block_size)
      : dedup_volume_file{path}, file_index{file_index}, block_size{block_size}
  {
  }
  std::uint32_t file_index;

  static constexpr std::int64_t read_only_size = -1;
  static constexpr std::int64_t any_size = 0;
  std::int64_t block_size;

  struct written_loc {
    std::uint64_t begin;
    std::uint64_t end;

    written_loc(std::uint64_t begin, std::uint64_t end) : begin(begin), end(end)
    {
    }
  };

  std::optional<written_loc> write(std::uint64_t offset,
                                   const char* begin,
                                   const char* end)
  {
    // first check that the space was reserved:

    std::uint64_t end_offset = offset + static_cast<std::uint64_t>(end - begin);

    if (!goto_begin(end_offset)) { return std::nullopt; }

    if (!goto_begin(offset)) { return std::nullopt; }

    if (!dedup_volume_file::write(begin, end - begin)) { return std::nullopt; }

    return std::make_optional<written_loc>(offset, end_offset);
  }

  bool accepts_records_of_size(std::uint32_t record_size) const
  {
    if (block_size == any_size) {
      return true;
    } else if (block_size == read_only_size) {
      return false;
    } else {
      return record_size == block_size;
    }
  }

  std::optional<written_loc> reserve(std::size_t size)
  {
    if (!accepts_records_of_size(size)) { return std::nullopt; }

    // todo: this is wrong; we need to track the current "virtual" length
    // as well, so that two calls to reserve() work (important for multiplexing)

    std::optional current = current_pos();
    if (!current) { return std::nullopt; }

    std::size_t end = *current + size;

    if (!truncate(end)) { return std::nullopt; }

    goto_begin(end);

    return written_loc{*current, end};
  }

  bool read_data(char* buf, std::uint64_t start, std::uint64_t end)
  {
    if (!dedup_volume_file::goto_begin(start)) { return false; }

    return dedup_volume_file::read(buf, end - start);
  }
};

struct dedup_volume_config {
  std::vector<block_file> blockfiles;
  std::vector<record_file> recordfiles;
  std::vector<data_file> datafiles;

  void create_default()
  {
    blockfiles.clear();
    recordfiles.clear();
    datafiles.clear();
    blockfiles.emplace_back("block", 0, 0, 0);
    recordfiles.emplace_back("record", 0, 0, 0);
    datafiles.emplace_back("64KiB", 0, 65536);
    datafiles.emplace_back("data", 1, data_file::any_size);
  }
};

class dedup_volume {
 public:
  dedup_volume(const char* path, DeviceMode dev_mode, int mode)
      : path(path), configfile{"config"}
  {
    int dir_mode = mode | 0100;
    if (dev_mode == DeviceMode::CREATE_READ_WRITE) {
      if (struct stat st; ::stat(path, &st) != -1) {
        error = true;
        return;
      }

      if (mkdir(path, mode | 0100) < 0) {
        error = true;
        return;
      }

      // to create files inside dir, we need executive permissions
    }

    dir = raii_fd(path, O_RDONLY | O_DIRECTORY, dir_mode);


    if (!dir.is_ok()) {
      error = true;
      return;
    }

    if (dev_mode == DeviceMode::OPEN_WRITE_ONLY) {
      // we always need to read the config file
      configfile.open_inside(dir, mode, DeviceMode::OPEN_READ_WRITE);
    } else {
      configfile.open_inside(dir, mode, dev_mode);
    }

    if (!configfile.is_ok()) {
      error = true;
      return;
    }

    if (dev_mode == DeviceMode::CREATE_READ_WRITE) {
      config.create_default();
      volume_changed = true;
    } else {
      if (!load_config()) {
        error = true;
        return;
      }
    }

    for (auto& blockfile : config.blockfiles) {
      if (!blockfile.open_inside(dir, mode, dev_mode)) {
        error = true;
        return;
      }
    }

    for (auto& recordfile : config.recordfiles) {
      if (!recordfile.open_inside(dir, mode, dev_mode)) {
        error = true;
        return;
      }
    }

    for (auto& datafile : config.datafiles) {
      if (!datafile.open_inside(dir, mode, dev_mode)) {
        error = true;
        return;
      }
    }
  }

  record_file& get_active_record_file()
  {
    // currently only one record file is supported
    return config.recordfiles[0];
  }

  record_file& get_record_file(std::uint32_t)
  {
    return get_active_record_file();
  }

  block_file& get_active_block_file()
  {
    // currently only one block file is supported
    return config.blockfiles[0];
  }

  bool is_at_end()
  {
    auto& blockfile = get_active_block_file();
    return blockfile.current_block == blockfile.end_block;
  }

  data_file& get_data_file_by_size(std::uint32_t record_size)
  {
    // we have to do this smarter
    // if datafile::any_size is first, we should ignore it until the end!
    // maybe split into _one_ any_size + map size -> file
    // + vector of read_only ?
    for (auto& datafile : config.datafiles) {
      if (datafile.accepts_records_of_size(record_size)) { return datafile; }
    }

    ASSERT(!"dedup volume has no datafile for this record.\n");
    return config.datafiles[0];
  }

  bool reset()
  {
    // TODO: look at unix_file_device for "secure_erase_cmdline"
    for (auto& blockfile : config.blockfiles) {
      if (!blockfile.truncate()) { return false; }
    }

    for (auto& recordfile : config.recordfiles) {
      if (!recordfile.truncate()) { return false; }
    }

    for (auto& datafile : config.datafiles) {
      if (!datafile.truncate()) { return false; }
    }
    return true;
  }

  bool goto_begin()
  {
    for (auto& blockfile : config.blockfiles) {
      if (!blockfile.goto_begin()) { return false; }
    }

    for (auto& recordfile : config.recordfiles) {
      if (!recordfile.goto_begin()) { return false; }
    }

    for (auto& datafile : config.datafiles) {
      if (!datafile.goto_begin()) { return false; }
    }
    return true;
  }

  bool goto_block(std::size_t block_num)
  {
    // todo: if we are not at the end of the device
    //       we should read the block header and position
    //       the record and data files as well
    //       otherwise set the record and data files to their respective end

    std::uint32_t max_block = 0;
    for (auto& blockfile : config.blockfiles) {
      max_block = std::max(max_block, blockfile.end_block);
      if (blockfile.start_block <= block_num
          && blockfile.end_block < block_num) {
        return blockfile.goto_block(block_num);
      }
    }

    if (block_num == max_block) { return goto_end(); }
    return false;
  }

  bool goto_end()
  {
    for (auto& blockfile : config.blockfiles) {
      if (!blockfile.goto_end()) { return false; }
    }
    for (auto& recordfile : config.recordfiles) {
      if (!recordfile.goto_end()) { return false; }
    }
    for (auto& datafile : config.datafiles) {
      if (!datafile.goto_end()) { return false; }
    }
    return true;
  }

  std::optional<dedup_block_header> read_block()
  {
    auto& blockfile = get_active_block_file();

    return blockfile.read_block();
  }

  std::optional<dedup_record_header> read_record(std::uint32_t file_index,
                                                 std::uint32_t record_index)
  {
    // müssen wir hier überhaupt das record bewegen ?
    // wenn eod, bod & reposition das richtige machen, sollte man
    // immer bei der richtigen position sein
    if (file_index > config.recordfiles.size()) { return std::nullopt; }
    auto& record_file = config.recordfiles[file_index];
    return record_file.read_record(record_index);
  }

  bool read_data(std::uint32_t file_index,
                 std::uint64_t start,
                 std::uint64_t end,
                 write_buffer& buf)
  {
    if (file_index > config.datafiles.size()) { return false; }

    auto& data_file = config.datafiles[file_index];

    // todo: check we are in the right position
    char* data = buf.reserve(end - start);
    if (!data) { return false; }
    if (!data_file.read_data(data, start, end)) { return false; }
    return true;
  }

  dedup_volume(dedup_volume&& other) : dedup_volume()
  {
    *this = std::move(other);
  }
  dedup_volume& operator=(dedup_volume&& other)
  {
    std::swap(path, other.path);
    std::swap(dir, other.dir);
    std::swap(path, other.path);
    std::swap(configfile, other.configfile);
    std::swap(config, other.config);
    std::swap(error, other.error);
    std::swap(volume_changed, other.volume_changed);

    return *this;
  }

  ~dedup_volume()
  {
    if (volume_changed) { write_current_config(); }

    if (error) {
      Emsg1(M_FATAL, 0,
            _("Error while writing dedup config.  Volume '%s' may be damaged."),
            path.c_str());
    }
  }

  bool is_ok() const { return !error && dir.is_ok() && configfile.is_ok(); }

  void write_current_config();
  bool load_config();
  void changed_volume() { volume_changed = true; };

  struct written_loc {
    std::uint32_t file_index;
    std::uint64_t begin;
    std::uint64_t end;
  };

  std::optional<written_loc> write_data(
      const bareos_block_header& blockheader,
      const bareos_record_header& recordheader,
      const char* data_start,
      const char* data_end)
  {
    if (recordheader.Stream < 0) {
      // this is a continuation record header
      record this_record{blockheader.VolSessionId, blockheader.VolSessionTime,
                         recordheader.FileIndex, -recordheader.Stream};

      if (auto found = unfinished_records.find(this_record);
          found != unfinished_records.end()) {
        write_loc& loc = found->second;

        if (!(data_start < data_end)) { return std::nullopt; }
        if (loc.end - loc.current
            < static_cast<std::uint64_t>(data_end - data_start)) {
          return std::nullopt;
        }

        auto& datafile = config.datafiles[loc.file_index];
        std::optional data_written
            = datafile.write(loc.current, data_start, data_end);
        if (!data_written) { return std::nullopt; }

        loc.current += (data_end - data_start);
        if (loc.current == loc.end) { unfinished_records.erase(found); }

        return written_loc{loc.file_index, data_written->begin,
                           data_written->end};
      } else {
        return std::nullopt;
      }
    } else {
      record this_record{blockheader.VolSessionId, blockheader.VolSessionTime,
                         recordheader.FileIndex, recordheader.Stream};
      // this is a real record header
      if (auto found = unfinished_records.find(this_record);
          found != unfinished_records.end()) {
        return std::nullopt;
      } else {
        auto [iter, inserted]
            = unfinished_records.emplace(this_record, write_loc{});

        if (!inserted) { return std::nullopt; }

        auto& datafile = get_data_file_by_size(recordheader.DataSize);
        std::optional file_loc = datafile.reserve(recordheader.DataSize);

        if (!file_loc) {
          unfinished_records.erase(iter);
          return std::nullopt;
        }

        write_loc& loc = iter->second;
        loc.file_index = datafile.file_index;
        loc.current = file_loc->begin;
        loc.end = file_loc->end;

        std::optional data_written
            = datafile.write(loc.current, data_start, data_end);
        if (!data_written) { return std::nullopt; }

        loc.current += (data_end - data_start);
        if (loc.current == loc.end) { unfinished_records.erase(iter); }

        return written_loc{loc.file_index, data_written->begin,
                           data_written->end};
      }
    }
  }

 private:
  dedup_volume() = default;
  std::string path{};
  raii_fd dir{};
  dedup_volume_file configfile{};

  dedup_volume_config config{};

  bool error{false};
  bool volume_changed{false};

  struct record {
    std::uint32_t VolSessionId;
    std::uint32_t VolSessionTime;
    std::int32_t FileIndex;
    std::int32_t Stream;

    friend bool operator==(const record& l, const record& r)
    {
      return (l.VolSessionId == r.VolSessionId)
             && (l.VolSessionTime == r.VolSessionTime)
             && (l.FileIndex == r.FileIndex) && (l.Stream == r.Stream);
    }

    struct hash {
      std::size_t operator()(const record& rec) const
      {
        std::size_t hash = 0;

        hash *= 101;
        hash += std::hash<std::uint32_t>{}(rec.VolSessionId);
        hash *= 101;
        hash += std::hash<std::uint32_t>{}(rec.VolSessionTime);

        hash *= 101;
        hash += std::hash<std::int32_t>{}(rec.FileIndex);
        hash *= 101;
        hash += std::hash<std::int32_t>{}(rec.Stream);

        return hash;
      }
    };
  };

  struct write_loc {
    std::uint32_t file_index;
    std::uint64_t current;
    std::uint64_t end;
  };

  // todo: this should also get serialized!
  std::unordered_map<record, write_loc, record::hash> unfinished_records{};
};

class dedup_file_device : public Device {
 public:
  dedup_file_device() = default;
  ~dedup_file_device() { close(nullptr); }

  // Interface from Device
  SeekMode GetSeekMode() const override { return SeekMode::FILE_BLOCK; }
  bool CanReadConcurrently() const override { return false; }
  bool MountBackend(DeviceControlRecord* dcr, int timeout) override;
  bool UnmountBackend(DeviceControlRecord* dcr, int timeout) override;
  bool ScanForVolumeImpl(DeviceControlRecord* dcr) override;
  int d_close(int) override;
  int d_open(const char* pathname, int flags, int mode) override;
  int d_ioctl(int fd, ioctl_req_t request, char* mt = NULL) override;
  boffset_t d_lseek(DeviceControlRecord* dcr,
                    boffset_t offset,
                    int whence) override;
  ssize_t d_read(int fd, void* buffer, size_t count) override;
  ssize_t d_write(int fd, const void* buffer, size_t count) override;
  bool d_truncate(DeviceControlRecord* dcr) override;
  bool rewind(DeviceControlRecord* dcr) override;
  bool UpdatePos(DeviceControlRecord* dcr) override;
  bool Reposition(DeviceControlRecord* dcr,
                  uint32_t rfile,
                  uint32_t rblock) override;
  bool eod(DeviceControlRecord* dcr) override;

 private:
  bool mounted{false};

  int fd_ctr;
  std::unordered_map<int, dedup_volume> open_volumes;
};

} /* namespace storagedaemon */

#endif  // BAREOS_STORED_BACKENDS_DEDUP_FILE_DEVICE_H_
