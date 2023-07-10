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

#include <algorithm>
#include <optional>
#include <string>
#include <unordered_set>
#include <utility>

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
  net_u64 start;
  net_u64 count;

  block_header() = default;

  block_header(const bareos_block_header& base,
               std::uint64_t start,
               std::uint64_t count)
      : BareosHeader(base)
      , start{network_order::of_native(start)}
      , count{network_order::of_native(count)}
  {
  }
};

static_assert(std::is_standard_layout_v<block_header>);
static_assert(std::is_pod_v<block_header>);
static_assert(std::has_unique_object_representations_v<block_header>);

struct record_header {
  bareos_record_header BareosHeader;
  // the weird order is for padding reasons
  net_u32 size;   // real payload size
  net_u64 start;  // data start
  net_u64 file_index;

  record_header() = default;

  record_header(const bareos_record_header& base,
                std::uint64_t start,
                std::uint32_t size,
                std::uint64_t file_index)
      : BareosHeader(base)
      , size{network_order::of_native(size)}
      , start{network_order::of_native(start)}
      , file_index{file_index}
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
  block_file(util::raii_fd&& file, std::uint64_t start, std::uint64_t count)
      : start_block{start}, vec{std::move(file), count}
  {
  }
  const char* path() const { return vec.backing_file().relative_path(); }

  std::uint32_t begin() const { return start_block; }

  std::uint32_t current() const { return vec.current() + start_block; }

  std::uint32_t end() const { return vec.size() + start_block; }

  std::uint32_t capacity() const { return vec.capacity(); }

  bool truncate()
  {
    start_block = 0;
    vec.clear();
    return true;
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

  bool is_ok() { return vec.is_ok(); }

 private:
  std::uint64_t start_block{};
  util::file_based_array<block_header> vec{};
};

class record_file {
 public:
  record_file() = default;
  record_file(util::raii_fd&& file, std::uint64_t start, std::uint64_t count)
      : start_record{start}, vec{std::move(file), count, 128 * 1024}
  {
  }

  const char* path() const { return vec.backing_file().relative_path(); }

  std::uint64_t begin() const { return start_record; }

  std::uint64_t end() const { return vec.size() + start_record; }

  std::uint64_t size() const { return vec.size(); }

  bool truncate()
  {
    start_record = 0;
    vec.clear();
    return vec.shrink_to_fit();
  }

  std::optional<std::uint64_t> append(const record_header* headers,
                                      std::uint64_t count)
  {
    return vec.push_back(headers, count);
  }

  bool read_at(std::uint32_t record,
               record_header* headers,
               std::uint64_t count) const
  {
    return vec.read_at(record, headers, count);
  }

  bool is_ok() { return vec.is_ok(); }

 private:
  std::uint64_t start_record{};
  util::file_based_vector<record_header> vec{};
};

class data_file {
 public:
  data_file() = default;
  data_file(util::raii_fd&& file,
            std::uint64_t file_index,
            std::uint64_t block_size,
            std::uint64_t data_used,
            bool read_only = false)
      : file_index{file_index}
      , block_size{block_size}
      , read_only{read_only}
      , vec{std::move(file), data_used, 128 * 1024}
      , error{block_size == 0 || !vec.is_ok()}
  {
  }

  std::uint64_t index() const { return file_index; }
  std::uint64_t blocksize() const { return block_size; }

  const char* path() const { return vec.backing_file().relative_path(); }

  std::uint64_t end() const { return vec.size(); }

  bool truncate()
  {
    vec.clear();
    return vec.shrink_to_fit();
  }

  struct written_loc {
    std::uint64_t begin;
    std::uint64_t end;

    written_loc(std::uint64_t begin, std::uint64_t end) : begin(begin), end(end)
    {
    }
  };

  std::optional<written_loc> write_at(std::uint64_t offset,
                                      const char* data,
                                      std::size_t size)
  {
    if (!accepts_records_of_size(size)) { return std::nullopt; }

    bool success = vec.write_at(offset, data, size);

    if (!success) { return std::nullopt; }

    return written_loc{offset, offset + size};
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

  bool read_at(char* buf, std::uint64_t start, std::uint64_t size) const
  {
    return vec.read_at(start, buf, size);
  }

 private:
  std::uint64_t file_index{};
  std::uint64_t block_size{};
  bool read_only{true};
  util::file_based_vector<char> vec{};
  bool error{true};
};

struct volume_layout {
  struct block_file {
    std::string path;
    std::size_t start;
    std::size_t count;

    block_file() = default;
    block_file(std::string_view path, std::size_t start, std::size_t count)
        : path{path}, start{start}, count{count}
    {
    }
  };
  struct record_file {
    std::string path;
    std::size_t start;
    std::size_t count;
    record_file() = default;
    record_file(std::string_view path, std::size_t start, std::size_t count)
        : path{path}, start{start}, count{count}
    {
    }
  };
  struct data_file {
    std::string path;
    std::size_t file_index;
    std::size_t chunk_size;
    std::size_t bytes_used;
    data_file() = default;
    data_file(std::string_view path,
              std::size_t file_index,
              std::size_t chunk_size,
              std::size_t bytes_used)
        : path{path}
        , file_index{file_index}
        , chunk_size{chunk_size}
        , bytes_used{bytes_used}
    {
    }
  };
  std::vector<block_file> blockfiles;
  std::vector<record_file> recordfiles;
  std::vector<data_file> datafiles;

  static volume_layout create_default(std::uint32_t dedup_block_size)
  {
    volume_layout layout;
    layout.blockfiles.emplace_back("block", 0, 0);
    layout.recordfiles.emplace_back("record", 0, 0);
    if (dedup_block_size != 0) {
      layout.datafiles.emplace_back("full_blocks", layout.datafiles.size(),
                                    dedup_block_size, 0);
    }
    layout.datafiles.emplace_back("data", layout.datafiles.size(), 1, 0);
    return layout;
  }

  volume_layout(config::loaded_config&& conf)
  {
    for (auto&& blocksection : conf.blockfiles) {
      blockfiles.emplace_back(std::move(blocksection.path),
                              blocksection.start_block,
                              blocksection.num_blocks);
    }
    // todo: we need to check wether the start blocks are unique or not!
    std::sort(blockfiles.begin(), blockfiles.end(),
              [](const auto& l, const auto& r) { return l.start < r.start; });

    for (auto&& recordsection : conf.recordfiles) {
      recordfiles.emplace_back(std::move(recordsection.path),
                               recordsection.start_record,
                               recordsection.num_records);
    }
    // todo: we need to check wether the start blocks are unique or not!
    std::sort(recordfiles.begin(), recordfiles.end(),
              [](const auto& l, const auto& r) { return l.start < r.start; });

    for (auto&& datasection : conf.datafiles) {
      datafiles.emplace_back(std::move(datasection.path),
                             datasection.file_index, datasection.block_size,
                             datasection.data_used);
    }
    // todo: we need to check whether the indices are unique or not!
    std::sort(datafiles.begin(), datafiles.end(),
              [](const auto& l, const auto& r) {
                return l.file_index < r.file_index;
              });
  }

  volume_layout() = default;

  bool validate()
  {
    for (std::size_t i = 0; i + 1 < blockfiles.size(); ++i) {
      auto& current = blockfiles[i];
      auto& next = blockfiles[i + 1];
      if (current.start + current.count > next.start) {
        // error: blocks are not unique
        return false;
      } else if (current.start + current.count < next.start) {
        // warning: missing blocks
        return false;
      }
    }

    for (std::size_t i = 0; i + 1 < recordfiles.size(); ++i) {
      auto& current = recordfiles[i];
      auto& next = recordfiles[i + 1];
      if (current.start + current.count > next.start) {
        // error: records are not unique
        return false;
      } else if (current.start + current.count < next.start) {
        // warning: missing records
        return false;
      }
    }
    return true;
  }

 private:
};

struct volume_data {
  volume_data() = default;
  volume_data(volume_layout&& layout,
              util::raii_fd& dir,
              int mode,
              DeviceMode dev_mode)
      : error{false}
  {
    for (auto& blockfile : layout.blockfiles) {
      auto file = open_inside(dir, blockfile.path.c_str(), mode, dev_mode);
      auto& result = blockfiles.emplace_back(std::move(file), blockfile.start,
                                             blockfile.count);
      if (!result.is_ok()) {
        error = true;
        return;
      }
    }

    for (auto& recordfile : layout.recordfiles) {
      auto file = open_inside(dir, recordfile.path.c_str(), mode, dev_mode);
      auto& result = recordfiles.emplace_back(std::move(file), recordfile.start,
                                              recordfile.count);
      if (!result.is_ok()) {
        error = true;
        return;
      }
    }

    for (auto& datafile : layout.datafiles) {
      auto file = open_inside(dir, datafile.path.c_str(), mode, dev_mode);
      auto& result
          = datafiles.emplace_back(std::move(file), datafile.file_index,
                                   datafile.chunk_size, datafile.bytes_used);
      if (!result.is_ok()) {
        error = true;
        return;
      }
    }
  }

  volume_layout make_layout() const
  {
    volume_layout layout;

    for (auto& blockfile : blockfiles) { (void)blockfile; }

    for (auto& recordfile : recordfiles) { (void)recordfile; }

    for (auto& datafile : datafiles) { (void)datafile; }

    return layout;
  }

  bool is_ok() const { return !error; }

  std::vector<block_file> blockfiles{};
  std::vector<record_file> recordfiles{};
  std::vector<data_file> datafiles{};
  bool error{true};
};

class volume {
 public:
  volume(const char* path,
         DeviceMode dev_mode,
         int mode,
         std::uint32_t dedup_block_size)
      : path(path)
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
      config = open_inside(dir, default_config_path, mode,
                           DeviceMode::OPEN_READ_WRITE);
    } else {
      config = open_inside(dir, default_config_path, mode, dev_mode);
    }

    if (!config.is_ok()) {
      error = true;
      return;
    }

    std::optional<volume_layout> layout;
    if (dev_mode == DeviceMode::CREATE_READ_WRITE) {
      volume_changed = true;
      layout = volume_layout::create_default(dedup_block_size);
    } else {
      layout = load_layout();
    }

    if (!layout) {
      error = true;
      return;
    }

    contents = volume_data{std::move(*layout), dir, mode, dev_mode};

    if (!contents.is_ok()) {
      error = true;
      return;
    }
  }

  bool append_block(const block_header& block)
  {
    auto& blockfile = contents.blockfiles.back();

    if (blockfile.capacity() == blockfile.end()) {}

    auto result = blockfile.write(block);
    if (result) { changed_volume(); }
    return result;
  }

  std::uint64_t size() const { return contents.blockfiles.back().end(); }

  data_file* get_data_file_by_size(std::uint32_t record_size)
  {
    // we have to do this smarter
    // if datafile::any_size is first, we should ignore it until the end!
    // maybe split into _one_ any_size + map size -> file
    // + vector of read_only ?
    data_file* selected = nullptr;
    ;
    for (auto& datafile : contents.datafiles) {
      if (datafile.accepts_records_of_size(record_size)) {
        if (!selected || selected->blocksize() < datafile.blocksize()) {
          selected = &datafile;
        }
      }
    }

    return selected;
  }

  bool reset()
  {
    changed_volume();

    // TODO: look at unix_file_device for "secure_erase_cmdline"
    // TODO: delete all but one record/block file
    //       and all unneeded datafiles

    for (auto& blockfile : contents.blockfiles) {
      if (!blockfile.truncate()) { return false; }
    }
    contents.blockfiles.resize(1);

    for (auto& recordfile : contents.recordfiles) {
      if (!recordfile.truncate()) { return false; }
    }
    contents.recordfiles.resize(1);

    std::unordered_set<uint64_t> blocksizes;
    for (auto& datafile : contents.datafiles) {
      if (!datafile.truncate()) { return false; }
    }

    contents.datafiles.erase(
        std::remove_if(contents.datafiles.begin(), contents.datafiles.end(),
                       [&blocksizes](const auto& datafile) {
                         auto [_, inserted]
                             = blocksizes.insert(datafile.blocksize());
                         return !inserted;
                       }),
        contents.datafiles.end());

    return true;
  }

  std::optional<block_header> read_block(std::uint64_t block_num)
  {
    auto& files = contents.blockfiles;
    if (files.size() == 0) { return std::nullopt; }

    // iter points to the first block file for which file.begin() <=
    // block_num
    auto iter = std::lower_bound(files.rbegin(), files.rend(), block_num,
                                 [](const auto& file, const auto& block_num) {
                                   return file.begin() > block_num;
                                 });
    // this only happens if something weird is going on since
    // block_num >= 0 is true and we should always have a block
    // with begin() == 0
    if (iter == files.rend()) {
      // warning: some blocks are missing
      return std::nullopt;
    }

    if (!iter->goto_block(block_num)) { return std::nullopt; }
    return iter->read_block();
  }

  std::optional<std::uint64_t> append_records(record_header* headers,
                                              std::uint64_t count)
  {
    auto result = contents.recordfiles.back().append(headers, count);
    if (result) { changed_volume(); }
    return result;
  }

  bool read_records(std::uint64_t record_index,
                    record_header* headers,
                    std::uint64_t count)
  {
    // müssen wir hier überhaupt das record bewegen ?
    // wenn eod, bod & reposition das richtige machen, sollte man
    // immer bei der richtigen position sein

    auto& files = contents.recordfiles;

    // iter points to the first record file for which file.begin() <=
    // record_index
    auto iter
        = std::lower_bound(files.rbegin(), files.rend(), record_index,
                           [](const auto& file, const auto& record_index) {
                             return file.begin() > record_index;
                           });
    // this only happens if something weird is going on since
    // record_index >= 0 is true and we should always have a record
    // with begin() == 0
    if (iter == files.rend()) {
      // warning: some records are missing
      return false;
    }


    for (;;) {
      std::uint64_t num_records = std::min(iter->end() - record_index, count);

      if (!iter->read_at(record_index, headers, num_records)) { return false; }

      count -= num_records;
      // this is a reverse iterator, so we need to go backwards!
      if (count > 0) {
        headers += num_records;
        record_index += num_records;

        if (iter == files.rbegin()) {
          return false;
        } else {
          // since iter is a reverse iterator
          // `--' actually lets it go forward!
          --iter;
        }
      } else {
        break;
      }
    }

    return true;
  }

  bool read_data(std::uint32_t file_index,
                 std::uint64_t start,
                 std::uint32_t size,
                 write_buffer& buf)
  {
    if (file_index >= contents.datafiles.size()) { return false; }

    auto& data_file = contents.datafiles[file_index];

    char* data = buf.reserve(size);
    if (!data) { return false; }
    if (!data_file.read_at(data, start, size)) { return false; }
    return true;
  }

  volume(volume&& other) : volume() { *this = std::move(other); }
  volume& operator=(volume&& other)
  {
    std::swap(path, other.path);
    std::swap(dir, other.dir);
    std::swap(path, other.path);
    std::swap(config, other.config);
    std::swap(contents, other.contents);
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

  bool is_ok() const { return !error && dir.is_ok() && config.is_ok(); }

  void write_current_config();
  std::optional<volume_layout> load_layout();

  struct written_loc {
    std::uint64_t file_index;
    std::uint64_t begin;
    std::uint64_t end;
  };

  std::optional<written_loc> append_data(
      const bareos_block_header& blockheader,
      const bareos_record_header& recordheader,
      const char* data,
      std::size_t size)
  {
    // continuation record headers have a negative stream number
    // whereas "starting" record headers have a nonnegative one.
    // We always use the positive stream number for lookups!
    record this_record{
        blockheader.VolSessionId, blockheader.VolSessionTime,
        recordheader.FileIndex,
        recordheader.Stream >= 0 ? +recordheader.Stream : -recordheader.Stream};
    if (recordheader.Stream < 0) {
      // this is a continuation record header

      if (auto found = unfinished_records.find(this_record);
          found != unfinished_records.end()) {
        write_loc& loc = found->second;

        if (loc.end - loc.current < static_cast<std::uint64_t>(size)) {
          return std::nullopt;
        }

        auto& datafile = contents.datafiles[loc.file_index];
        std::optional data_written = datafile.write_at(loc.current, data, size);
        if (!data_written) { return std::nullopt; }
        changed_volume();

        loc.current += (size);
        if (loc.current == loc.end) { unfinished_records.erase(found); }

        return written_loc{loc.file_index, data_written->begin,
                           data_written->end};
      }

      // if we did not find the start record on this volume, we instead
      // go on as if this was a normal record
    }
    // this is a real record header; or an unfinished one that was not started
    // in this volume; either way we need to use the real stream
    if (auto found = unfinished_records.find(this_record);
        found != unfinished_records.end()) {
      return std::nullopt;
    } else {
      auto [iter, inserted]
          = unfinished_records.emplace(this_record, write_loc{});

      if (!inserted) { return std::nullopt; }

      auto* datafile = get_data_file_by_size(recordheader.DataSize);
      if (!datafile) {
        // dmesg: no appropriate data file present
        return std::nullopt;
      }
      std::optional file_loc = datafile->reserve(recordheader.DataSize);

      if (!file_loc) {
        unfinished_records.erase(iter);
        return std::nullopt;
      }

      write_loc& loc = iter->second;
      loc.file_index = datafile->index();
      loc.current = file_loc->begin;
      loc.end = file_loc->end;

      std::optional data_written = datafile->write_at(loc.current, data, size);
      if (!data_written) {
        unfinished_records.erase(iter);
        return std::nullopt;
      }
      changed_volume();

      loc.current += size;
      if (loc.current == loc.end) { unfinished_records.erase(iter); }

      return written_loc{loc.file_index, data_written->begin,
                         data_written->end};
    }
  }

  volume_layout layout() const { return contents.make_layout(); }

  const std::vector<block_file>& blockfiles() const
  {
    return contents.blockfiles;
  }
  const std::vector<record_file>& recordfiles() const
  {
    return contents.recordfiles;
  }
  const std::vector<data_file>& datafiles() const { return contents.datafiles; }

 private:
  volume() = default;
  std::string path{};
  util::raii_fd dir{};
  static constexpr const char* default_config_path = "config";
  util::raii_fd config{};

  volume_data contents{};

  bool error{false};
  bool volume_changed{false};

  void changed_volume() { volume_changed = true; };

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
