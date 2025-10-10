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
#include "partitioning.h"

#include <chrono>
#include <stdexcept>
#include <fmt/format.h>
#include <optional>

#include <gsl/narrow>
#include <iostream>
#include <filesystem>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/fs.h>

#include <cuchar>

std::size_t io_block_size(int fd)
{
  static constexpr std::size_t default_blocksize = 256 << 10;

  struct stat st;
  if (fstat(fd, &st) < 0) { return default_blocksize; }

  auto recommended = st.st_blksize;

  // ignore nonsense
  if (recommended <= 0) { recommended = default_blocksize; }

  auto blocksize = static_cast<std::size_t>(recommended);

  // we never want to use less than `default_blocksize` bytes, so
  // if `recommended` is smaller, we take the next multiple that is bigger than
  // it.
  if (blocksize < default_blocksize) {
    auto factor = (default_blocksize + blocksize - 1) % blocksize;
    blocksize = factor * blocksize;
  }

  return blocksize;
}

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
          fmt::format("Trying to skip to offset {}, when already at offset {}",
                      offset, current_offset_)};
    }

    while (current_offset_ < offset) {
      auto diff = offset - current_offset_;
      if (diff > nothing_size) { diff = nothing_size; }
      append(std::span{nothing.get(), diff});
    }
  }

  std::size_t current_offset() const override { return current_offset_; }

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

  bool ok() const { return fd >= 0; }

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
    if (internal_offset_ != 0) {
      throw std::logic_error{libbareos::format(
          "im not starting at offset 0, but {} instead", internal_offset_)};
    }
  }

  void append(std::span<const char> bytes) override
  {
    if (current_offset_ + bytes.size() > size_) {
      throw std::logic_error{
          libbareos::format("can not write past the end of the file; size = "
                            "{}, offset = {}, to_write = {}",
                            size_, current_offset_, bytes.size())};
    }

    write(bytes);
    current_offset_ += bytes.size();
    if (auto offset = tell(); current_offset_ != offset) {
      throw std::logic_error{fmt::format(
          "Wrote {} bytes to offset {} but now im at offset {} (diff = {})",
          bytes.size(), current_offset_ - bytes.size(), offset,
          offset > current_offset_ ? offset - current_offset_
                                   : current_offset_ - offset)};
    }
  }
  void skip_forwards(std::size_t offset) override
  {
    if (internal_offset_ + offset < current_offset_) {
      throw std::logic_error{
          fmt::format("Trying to skip to offset {}, when already at offset {}",
                      offset, current_offset_)};
    }

    if (offset > size_) {
      throw std::logic_error{"can not seek past the end of the file"};
    }

    seek(internal_offset_ + offset);
    current_offset_ = internal_offset_ + offset;
  }

  std::size_t current_offset() const override { return current_offset_; }

 private:
  std::size_t tell()
  {
    off_t res = lseek(fd_, 0, SEEK_CUR);
    if (res < 0) {
      throw std::runtime_error{fmt::format(
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
          fmt::format("could not seek to {}: {}", offset, strerror(errno))};
    }

    if (res != s_offset) {
      throw std::runtime_error(fmt::format(
          "wanted to seek to {}, but got {} instead", s_offset, res));
    }

    if (auto pos = tell(); offset != pos) {
      throw std::runtime_error(
          fmt::format("seeked to {}, but am at {} instead", offset, pos));
    }
  }

  void write(std::span<const char> bytes)
  {
    {
      auto actual_offset = tell();
      if (actual_offset != current_offset_) {
        throw std::runtime_error(
            fmt::format("wanted to write to {}/{}, but got {} instead",
                        current_offset_, internal_offset_, actual_offset));
      }
    }

    auto towrite = bytes;

    std::size_t bad_count = 0;

    while (towrite.size() > 0) {
      auto res = ::write(fd_, towrite.data(), towrite.size());
      if (res < 0) {
        off_t actual_offset = lseek(fd_, 0, SEEK_CUR);
        throw std::runtime_error(
            fmt::format("write() failed: {} (tried writing {} bytes to offset "
                        "{}/{}, {} still to go, {} bad writes)",
                        strerror(errno), bytes.size(), current_offset_,
                        actual_offset, towrite.size(), bad_count));
      }
      if (res == 0) {
        bad_count += 1;
        if (bad_count > 20) {
          throw std::runtime_error(fmt::format(
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

template <typename WrappedOutput> struct BufferedOutput : Output {
  template <typename... WrappedArgs>
  BufferedOutput(std::size_t buffer_size, WrappedArgs&&... args)
      : wrapped{std::forward<WrappedArgs>(args)...}
  {
    buffer.reserve(buffer_size);
  }

 public:
  void flush()
  {
    if (!buffer.empty()) { wrapped.append(buffer); }
  }

  void append(std::span<const char> bytes) override
  {
    while (!bytes.empty()) {
      std::size_t buffer_free = buffer.capacity() - buffer.size();

      if (bytes.size() < buffer_free) {
        // we are done
        buffer.insert(buffer.end(), bytes.begin(), bytes.end());
        break;
      } else {
        // if our buffer is empty, then we "emulate" buffered writes
        // instead of wasting cycles writing stuff into the buffer, just
        // to flush the buffer instead

        if (buffer.size() == 0) {
          while (bytes.size() > buffer.capacity()) {
            auto to_insert = bytes.subspan(0, buffer.capacity());

            wrapped.append(to_insert);

            bytes = bytes.subspan(buffer.capacity());
          }
        } else {
          // our buffer is already used, so we cannot just skip that data
          // since we only want to do "buffered" writes to wrapped*, i.e.
          // writes of a full buffer, we have no choice but to first
          // buffer the data and then flush the buffer
          // [*]: apart from the last one!

          auto to_insert = bytes.subspan(0, buffer_free);
          buffer.insert(buffer.end(), to_insert.begin(), to_insert.end());
          flush_buffer();
          bytes = bytes.subspan(buffer_free);
        }
      }
    }
  }
  void skip_forwards(std::size_t offset) override
  {
    auto wrapped_offset = wrapped.current_offset();
    auto current = wrapped_offset + buffer.size();
    if (current > offset) {
      throw std::logic_error{libbareos::format(
          "Trying to skip to offset {}, when already at offset {} ({} + {})",
          offset, current, wrapped_offset, buffer.size())};
    }

    if (offset < wrapped_offset + buffer.capacity()) {
      // we skip into a different point in our buffer

      // ASSERT: this holds because of the check above !(current > offset)
      assert(offset >= current);
      auto diff = offset - current;
      buffer.insert(buffer.end(), diff, 0);
    } else {
      // we skip outside our buffer, we need to flush the buffer and
      // skip in the wrapped output

      buffer.insert(buffer.end(), buffer.capacity() - buffer.size(), 0);

      flush_buffer();
      wrapped.skip_forwards(offset);
    }
  }
  std::size_t current_offset() const override
  {
    return wrapped.current_offset() + buffer.size();
  }

  const WrappedOutput& internal() const { return wrapped; }

  ~BufferedOutput() { flush(); }

 private:
  void flush_buffer()
  {
    wrapped.append(buffer);
    buffer.clear();
  }

  WrappedOutput wrapped;
  std::vector<char> buffer;
};

class RestoreToStdout : public GenericHandler {
 public:
  RestoreToStdout() : output{io_block_size(fileno(stdout)), std::cout} {}

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
  void EndDisk() override
  {
    disk().Finish();
    output.flush();
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

  BufferedOutput<StreamOutput> output;
  std::optional<disk::Parser> disk_;
};

struct writing_disk {
  BufferedOutput<FileOutput> output;
  disk::Parser disk;

  writing_disk(int fd, disk_geometry geometry, std::size_t disk_size)
      : output{io_block_size(fd), fd, disk_size}
      , disk{geometry, disk_size, &output}
  {
  }
};

class RestoreToGeneratedFiles : public GenericHandler {
 public:
  RestoreToGeneratedFiles(std::filesystem::path directory)
  {
    directory_fd = auto_fd{open(directory.c_str(), O_DIRECTORY | O_RDONLY)};
    if (!directory_fd.ok()) {
      throw std::runtime_error(fmt::format("cannot open '{}': {}",
                                           directory.c_str(), strerror(errno)));
    }
  }

  void BeginRestore(std::size_t num_disks) override
  {
    disk_files.reserve(num_disks);
    for (std::size_t idx = 0; idx < num_disks; ++idx) {
      auto disk_name = fmt::format("disk-{}.raw", idx);

      auto_fd fd{openat(directory_fd.get(), disk_name.c_str(),
                        O_CREAT | O_WRONLY | O_EXCL, 0664)};

      if (!fd.ok()) {
        throw std::runtime_error{
            fmt::format("could not open '{}': {}", disk_name, strerror(errno))};
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
      throw std::logic_error{
          "cannot begin new disk as old one is not finished yet"};
    }

    auto& fd = disk_files[current_idx];

    if (ftruncate(fd.get(), disk_size) < 0) {
      throw std::runtime_error{libbareos::format(
          "could not expand disk {}: Err={}", current_idx, strerror(errno))};
    }

    // TODO: we should use the disk_size of the _target_ disk
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

  auto_fd directory_fd;
  std::size_t current_idx = 0;
  std::vector<auto_fd> disk_files;
  std::optional<writing_disk> current_disk;
};

class RestoreToSpecifiedFiles : public GenericHandler {
 public:
  RestoreToSpecifiedFiles(std::vector<auto_fd> files, GenericLogger* logger)
      : disk_files{std::move(files)}, logger_{logger}
  {
  }

  void BeginRestore(std::size_t num_disks) override
  {
    if (disk_files.size() != num_disks)
      throw std::runtime_error{fmt::format(
          "image contains {} disks, but only {} were specified on the "
          "command line",
          num_disks, disk_files.size())};

    logger_->Info("Restoring {} Disks", num_disks);
  }

  void EndRestore() override {}
  void BeginDisk(disk_info info) override
  {
    logger_->Info("Restoring disk {}", current_idx);
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
    logger_->Trace("writing extent ({}, {})", header.offset, header.length);
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
      throw std::logic_error{
          "cannot begin new disk as old one is not finished yet"};
    }

    auto& fd = disk_files[current_idx];

    struct stat s = {};
    if (fstat(fd.get(), &s) < 0) {
      throw std::runtime_error{libbareos::format(
          "could not stat disk {}: Err={}", current_idx, strerror(errno))};
    }

    if (S_ISREG(s.st_mode)) {
      if (s.st_size >= 0 && static_cast<std::size_t>(s.st_size) < disk_size) {
        // file is too small; lets try to enlarge it
        if (ftruncate(fd.get(), disk_size) < 0) {
          logger_->Info("could not expand disk {}: Err={}", current_idx,
                        strerror(errno));
        }
      }
    } else if (S_ISBLK(s.st_mode)) {
      std::uint64_t blk_size = 0;
      if (auto status = ioctl(fd.get(), BLKGETSIZE64, &blk_size); status < 0) {
        throw std::runtime_error{
            libbareos::format("could not determine disk size of disk {}: {}",
                              current_idx, strerror(errno))};
      }

      if (blk_size < disk_size) {
        throw std::runtime_error{libbareos::format(
            "disk {} is too small: {} < {}", current_idx, blk_size, disk_size)};
      }
    } else {
      logger_->Info(
          "disk {} is neither a block device, nor a regular file.  Continuing "
          "without size check",
          current_idx);
    }

    auto& res = current_disk.emplace(fd.get(), geometry, disk_size);
    return res.disk;
  }

  void end_disk()
  {
    if (!current_disk) {
      throw std::logic_error{"cannot access disk before it is created"};
    }

    logger_->Trace("flushing disk");
    current_disk.reset();
    current_idx += 1;
  }

  std::size_t current_idx = 0;
  std::vector<auto_fd> disk_files;
  std::optional<writing_disk> current_disk;
  GenericLogger* logger_;
};

std::vector<auto_fd> open_files(std::span<std::string> filenames)
{
  std::vector<auto_fd> files;
  files.reserve(filenames.size());

  for (auto& filename : filenames) {
    auto_fd fd{open(filename.c_str(), O_WRONLY, 0664)};

    if (!fd.get()) {
      throw std::runtime_error{libbareos::format("could not open '{}': {}",
                                                 filename, strerror(errno))};
    }

    files.emplace_back(std::move(fd));
  }

  return files;
}

#include "restore.h"

namespace barri::restore {

std::unique_ptr<GenericHandler> GetHandler(GenericLogger* logger,
                                           restore_directory dir)
{
  (void)logger;
  return std::make_unique<RestoreToGeneratedFiles>(dir.path);
}
std::unique_ptr<GenericHandler> GetHandler(GenericLogger* logger,
                                           restore_files paths)
{
  auto files = open_files(paths);
  return std::make_unique<RestoreToSpecifiedFiles>(std::move(files), logger);
}
std::unique_ptr<GenericHandler> GetHandler(GenericLogger* logger,
                                           restore_stdout stream)
{
  (void)logger;
  (void)stream;
  return std::make_unique<RestoreToStdout>();
}

}  // namespace barri::restore
