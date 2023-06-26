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

#ifndef BAREOS_STORED_BACKENDS_DEDUP_DEDUP_VOLUME_H_
#define BAREOS_STORED_BACKENDS_DEDUP_DEDUP_VOLUME_H_

#include "stored/dev.h"
#include "dedup_config.h"
#include "util.h"
#include "lib/network_order.h"

#include <utility>
#include <optional>
#include <algorithm>
#include <utility>
#include <string>

namespace dedup {
using DeviceMode = storagedaemon::DeviceMode;
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

struct block_header {
  bareos_block_header BareosHeader;
  net_u64 RecStart;
  net_u64 RecEnd;

  block_header() = default;

  block_header(const bareos_block_header& base,
               std::uint64_t RecStart,
               std::uint64_t RecEnd)
      : BareosHeader(base)
      , RecStart{network_order::of_native(RecStart)}
      , RecEnd{network_order::of_native(RecEnd)}
  {
  }
};

static_assert(std::is_standard_layout_v<block_header>);
static_assert(std::is_pod_v<block_header>);
static_assert(std::has_unique_object_representations_v<block_header>);

struct record_header {
  bareos_record_header BareosHeader;
  net_u32 reserved_0;
  net_u64 file_index;
  net_u64 DataStart;
  net_u64 DataEnd;

  record_header() = default;

  record_header(const bareos_record_header& base,
                std::uint64_t DataStart,
                std::uint64_t DataEnd,
                std::uint64_t file_index)
      : BareosHeader(base)
      , reserved_0{0}
      , file_index{file_index}
      , DataStart{network_order::of_native(DataStart)}
      , DataEnd{network_order::of_native(DataEnd)}
  {
  }
};

static_assert(std::is_standard_layout_v<record_header>);
static_assert(std::is_pod_v<record_header>);
static_assert(std::has_unique_object_representations_v<record_header>);

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

struct volume_file {
  std::string path{};
  util::raii_fd fd{};

  volume_file() = default;
  volume_file(std::string_view path) : path{path} {}

  bool open_inside(util::raii_fd& dir, int mode, DeviceMode dev_mode)
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
    fd = util::raii_fd(dir.get(), path.c_str(), flags, mode);
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

static util::raii_fd open_inside(util::raii_fd& dir,
                                 const char* path,
                                 int mode,
                                 DeviceMode dev_mode)
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
      return util::raii_fd{};
    }
  }
  return util::raii_fd(dir.get(), path, flags, mode);
}

class block_file {
 public:
  block_file() = default;
  block_file(std::string_view path, std::uint64_t start, std::uint64_t count)
      : start_block{start}, start_size{count}, name{path}
  {
  }
  const char* path() const { return name.c_str(); }

  std::uint32_t begin() const { return start_block; }

  std::uint32_t current() const { return vec.current() + start_block; }

  std::uint32_t end() const { return vec.size() + start_block; }

  bool truncate()
  {
    start_block = 0;
    start_size = 0;
    vec.clear();
    return vec.shrink_to_fit();
  }

  bool goto_end() { return vec.move_to(vec.size()); }

  bool goto_begin() { return vec.move_to(0); }

  bool goto_block(std::uint32_t block) { return vec.move_to(block); }

  bool write(const block_header& header)
  {
    return vec.write(header).has_value();
  }

  std::optional<block_header> read_block()
  {
    if (block_header b; vec.read(&b)) {
      return b;
    } else {
      return std::nullopt;
    }
  }

  bool is_ok() { return is_initialized && vec.is_ok(); }

  bool open_inside(util::raii_fd& dir, int mode, DeviceMode dev_mode)
  {
    if (is_initialized) { return false; }
    is_initialized = true;

    auto file = dedup::open_inside(dir, name.c_str(), mode, dev_mode);

    vec = std::move(util::file_based_vector<block_header>(std::move(file),
                                                          start_size, 1024));
    return vec.is_ok();
  }

 private:
  util::file_based_vector<block_header> vec{};
  std::uint64_t start_block{};
  std::uint64_t start_size{};
  std::string name{};
  bool is_initialized{false};
};

class record_file {
 public:
  record_file() = default;
  record_file(std::string_view path, std::uint64_t start, std::uint64_t count)
      : start_record{start}, start_size{count}, name{path}
  {
  }

  const char* path() const { return name.c_str(); }

  std::uint32_t begin() const { return start_record; }

  std::uint32_t current() const { return vec.current() + start_record; }

  std::uint32_t end() const { return vec.size() + start_record; }

  bool truncate()
  {
    start_record = 0;
    start_size = 0;
    vec.clear();
    return vec.shrink_to_fit();
  }

  bool goto_end() { return vec.move_to(vec.size()); }

  bool goto_begin() { return vec.move_to(0); }

  bool goto_record(std::uint32_t record) { return vec.move_to(record); }

  bool write(const record_header& header)
  {
    return vec.write(header).has_value();
  }

  std::optional<record_header> read_record()
  {
    if (record_header h; vec.read(&h)) {
      return h;
    } else {
      return std::nullopt;
    }
  }

  bool is_ok() { return is_initialized && vec.is_ok(); }

  bool open_inside(util::raii_fd& dir, int mode, DeviceMode dev_mode)
  {
    if (is_initialized) { return false; }
    is_initialized = true;

    auto file = dedup::open_inside(dir, name.c_str(), mode, dev_mode);

    vec = std::move(util::file_based_vector<record_header>(
        std::move(file), start_size, 128 * 1024));
    return vec.is_ok();
  }

 private:
  util::file_based_vector<record_header> vec{};
  std::uint64_t start_record{};
  std::uint64_t start_size{};
  std::string name{};
  bool is_initialized{false};
};

class data_file {
 public:
  data_file() = default;
  data_file(std::string_view path,
            std::uint64_t file_index,
            std::uint64_t block_size,
            std::uint64_t data_used,
            bool read_only = false)
      : file_index{file_index}
      , block_size{block_size}
      , data_used{data_used}
      , name{path}
      , read_only{read_only}
      , error{block_size == 0}
  {
  }

  std::uint64_t index() const { return file_index; }
  std::uint64_t blocksize() const { return block_size; }

  const char* path() const { return name.c_str(); }

  std::uint64_t end() const { return vec.size(); }

  bool truncate()
  {
    vec.clear();
    return vec.shrink_to_fit();
  }

  bool goto_end() { return vec.move_to(vec.size()); }

  bool goto_begin() { return vec.move_to(0); }

  bool open_inside(util::raii_fd& dir, int mode, DeviceMode dev_mode)
  {
    if (is_initialized) { return false; }
    is_initialized = true;

    auto file = dedup::open_inside(dir, name.c_str(), mode, dev_mode);

    vec = std::move(
        util::file_based_vector<char>(std::move(file), data_used, 128 * 1024));
    return vec.is_ok();
  }

  struct written_loc {
    std::uint64_t begin;
    std::uint64_t end;

    written_loc(std::uint64_t begin, std::uint64_t end) : begin(begin), end(end)
    {
    }
  };

  std::optional<written_loc> write(std::uint64_t offset,
                                   const char* data,
                                   std::size_t size)
  {
    if (!accepts_records_of_size(size)) { return std::nullopt; }

    std::optional start = vec.write_at(offset, data, size);

    if (!start) { return std::nullopt; }

    return written_loc{*start, *start + size};
  }
  std::optional<written_loc> reserve(std::size_t size)
  {
    if (!accepts_records_of_size(size)) { return std::nullopt; }

    std::optional start = vec.reserve(size);

    if (!start) { return std::nullopt; }

    return written_loc{*start, *start + size};
  }

  bool accepts_records_of_size(std::uint64_t record_size) const
  {
    if (read_only) { return false; }
    return record_size % block_size == 0;
  }

  bool is_ok() const { return !error && vec.is_ok(); }

  bool read_data(char* buf, std::uint64_t start, std::uint64_t size)
  {
    if (!vec.move_to(start)) { return false; }

    return vec.read(buf, size);
  }

 private:
  util::file_based_vector<char> vec{};
  std::uint64_t file_index{};
  std::uint64_t block_size{};
  std::uint64_t data_used{};
  std::string name{};
  bool read_only{true};
  bool error{true};
  bool is_initialized{false};
};

struct volume_config {
  std::vector<block_file> blockfiles;
  std::vector<record_file> recordfiles;
  std::vector<data_file> datafiles;

  void create_default(std::uint32_t dedup_block_size)
  {
    blockfiles.clear();
    recordfiles.clear();
    datafiles.clear();
    blockfiles.emplace_back("block", 0, 0);
    recordfiles.emplace_back("record", 0, 0);
    if (dedup_block_size != 0) {
      datafiles.emplace_back("full_blocks", 0, dedup_block_size, 0);
    }
    datafiles.emplace_back("data", 1, 1, 0);
  }

  volume_config() = default;
  volume_config(config::loaded_config&& conf)
  {
    for (auto&& blocksection : conf.blockfiles) {
      blockfiles.emplace_back(std::move(blocksection.path),
                              blocksection.start_block,
                              blocksection.num_blocks);
    }
    for (auto&& recordsection : conf.recordfiles) {
      recordfiles.emplace_back(std::move(recordsection.path),
                               recordsection.start_record,
                               recordsection.num_records);
    }
    for (auto&& datasection : conf.datafiles) {
      datafiles.emplace_back(std::move(datasection.path),
                             datasection.file_index, datasection.block_size,
                             datasection.data_used);
    }
  }
};

class volume {
 public:
  volume(const char* path,
         DeviceMode dev_mode,
         int mode,
         std::uint32_t dedup_block_size)
      : path(path), configfile{"config"}
  {
    // to create files inside dir, we need executive permissions
    int dir_mode = mode | 0100;
    if (struct stat st; (dev_mode == DeviceMode::CREATE_READ_WRITE)
                        && (::stat(path, &st) == -1)) {
      if (mkdir(path, mode | 0100) < 0) {
        error = true;
        return;
      }

    } else {
      dev_mode = DeviceMode::OPEN_READ_WRITE;
    }

    dir = util::raii_fd(path, O_RDONLY | O_DIRECTORY, dir_mode);


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
      config.create_default(dedup_block_size);
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
    return blockfile.current() == blockfile.end();
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
      max_block = std::max(max_block, blockfile.end());
      if (blockfile.begin() <= block_num && blockfile.end() < block_num) {
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

  std::optional<block_header> read_block()
  {
    auto& blockfile = get_active_block_file();

    return blockfile.read_block();
  }

  std::optional<record_header> read_record(std::uint64_t record_index)
  {
    // müssen wir hier überhaupt das record bewegen ?
    // wenn eod, bod & reposition das richtige machen, sollte man
    // immer bei der richtigen position sein

    for (auto& record_file : config.recordfiles) {
      if (record_index >= record_file.begin()
          && record_index < record_file.end()) {
        if (!record_file.goto_record(record_index)) { return std::nullopt; }
        return record_file.read_record();
      }
    }
    return std::nullopt;
  }

  bool read_data(std::uint32_t file_index,
                 std::uint64_t start,
                 std::uint64_t end,
                 write_buffer& buf)
  {
    if (start > end) { return false; }
    if (file_index > config.datafiles.size()) { return false; }

    auto& data_file = config.datafiles[file_index];

    // todo: check we are in the right position
    std::uint64_t size = end - start;
    char* data = buf.reserve(size);
    if (!data) { return false; }
    if (!data_file.read_data(data, start, size)) { return false; }
    return true;
  }

  volume(volume&& other) : volume() { *this = std::move(other); }
  volume& operator=(volume&& other)
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

  ~volume()
  {
    // do not write the config if there was an error
    if (error) return;

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
    std::uint64_t file_index;
    std::uint64_t begin;
    std::uint64_t end;
  };

  std::optional<written_loc> write_data(
      const bareos_block_header& blockheader,
      const bareos_record_header& recordheader,
      const char* data,
      std::size_t size)
  {
    if (recordheader.Stream < 0) {
      // this is a continuation record header
      record this_record{blockheader.VolSessionId, blockheader.VolSessionTime,
                         recordheader.FileIndex, -recordheader.Stream};

      if (auto found = unfinished_records.find(this_record);
          found != unfinished_records.end()) {
        write_loc& loc = found->second;

        if (loc.end - loc.current < static_cast<std::uint64_t>(size)) {
          return std::nullopt;
        }

        auto& datafile = config.datafiles[loc.file_index];
        std::optional data_written = datafile.write(loc.current, data, size);
        if (!data_written) { return std::nullopt; }

        loc.current += (size);
        if (loc.current == loc.end) { unfinished_records.erase(found); }

        return written_loc{loc.file_index, data_written->begin,
                           data_written->end};
      } else {
        goto create_new_record;
      }
    } else {
    create_new_record:
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
        loc.file_index = datafile.index();
        loc.current = file_loc->begin;
        loc.end = file_loc->end;

        std::optional data_written = datafile.write(loc.current, data, size);
        if (!data_written) { return std::nullopt; }

        loc.current += size;
        if (loc.current == loc.end) { unfinished_records.erase(iter); }

        return written_loc{loc.file_index, data_written->begin,
                           data_written->end};
      }
    }
  }

  const volume_config& get_config() const { return config; }

 private:
  volume() = default;
  std::string path{};
  util::raii_fd dir{};
  volume_file configfile{};

  volume_config config{};

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
    std::uint64_t file_index;
    std::uint64_t current;
    std::uint64_t end;
  };

  // todo: this should also get serialized!
  std::unordered_map<record, write_loc, record::hash> unfinished_records{};
};

} /* namespace dedup */

#endif  // BAREOS_STORED_BACKENDS_DEDUP_DEDUP_VOLUME_H_
