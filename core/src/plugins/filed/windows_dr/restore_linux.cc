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

#include "CLI/CLI.hpp"
#include "fmt/base.h"
#include "parser.h"
#include "partitioning.h"

#include "CLI/App.hpp"
#include "CLI/Config.hpp"
#include "CLI/Formatter.hpp"

#include <chrono>
#include <stdexcept>
#include <fmt/format.h>
#include <optional>

#include <gsl/narrow>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cuchar>

#include "logger.h"

bool trace = false;

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
  // if recommended is smaller, we take the next multiple thats bigger than it.
  if (blocksize < default_blocksize) {
    auto factor = (default_blocksize + blocksize - 1) % blocksize;
    blocksize = factor * blocksize;
  }

  return blocksize;
}


template <typename... T>
void trace_msg(fmt::format_string<T...> fmt, T&&... args)
{
  if (trace) { fmt::println(stderr, fmt, std::forward<T>(args)...); }
}

template <typename... T> void err_msg(fmt::format_string<T...> fmt, T&&... args)
{
  fmt::println(stderr, fmt, std::forward<T>(args)...);
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
    if (!directory_fd) {
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

      if (!fd) {
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
    if (fstat(fd, &s) < 0) {
      throw std::runtime_error{libbareos::format(
          "could not stat disk {}: Err={}", current_idx, strerror(errno))};
    }

    if (S_ISREG(s.st_mode) && s.st_size >= 0
        && static_cast<std::size_t>(s.st_size) < disk_size) {
      // file is too small; lets try to enlarge it
      if (ftruncate(fd.get(), disk_size) < 0) {
        logger_->Info("could not expand disk {}: Err={}", current_idx,
                      strerror(errno));
      }
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

std::string guid_to_string(guid id)
{
  std::uint64_t First = {};
  std::uint32_t Second = {};
  std::uint32_t Third = {};
  std::uint64_t Fourth = {};
  std::uint32_t Fifth = {};

  std::size_t offset = 0;
  std::memcpy(&First, id.Data, sizeof(First));
  offset += sizeof(First);
  std::memcpy(&Second, id.Data, sizeof(Second));
  offset += sizeof(Second);
  std::memcpy(&Third, id.Data, sizeof(Third));
  offset += sizeof(Third);
  std::memcpy(&Fourth, id.Data, sizeof(Fourth));
  offset += sizeof(Fourth);
  std::memcpy(&Fifth, id.Data, sizeof(Fifth));
  offset += sizeof(Fifth);

  return fmt::format("{:08X}-{:04X}-{:04X}-{:08X}{:04X}", First, Second, Third,
                     Fourth, Fifth);
}

std::string utf16_to_utf8(std::span<const char16_t> str)
{
  std::string result;
  result.resize((str.size() + 1) * MB_LEN_MAX);

  std::mbstate_t state{};

  char* out = result.data();
  for (auto c : str) {
    std::size_t count = std::c16rtomb(out, c, &state);
    out += count;
  }
  std::size_t count = std::c16rtomb(out, L'\0', &state);
  out += count;

  std::size_t total_size = out - result.data();
  assert(total_size <= result.size());
  result.resize(total_size);
  return result;
}

class ListContents : public GenericHandler {
 public:
  ListContents(GenericLogger* logger) : log{logger} {}

  void BeginRestore(std::size_t num_disks) override
  {
    log->Info("Contains {} Disks", num_disks);
  }
  void EndRestore() override {}
  void BeginDisk(disk_info info) override
  {
    log->Info("Disk {}:", disk_idx_);
    log->Info(" - Size = {}", info.disk_size);
    log->Info(" - Extent Count = {}", info.extent_count);
  }
  void EndDisk() override { disk_idx_ += 1; }

  void BeginMbrTable(const partition_info_mbr&) override
  {
    log->Info(" Mbr Table:");
  }
  void BeginGptTable(const partition_info_gpt& gpt) override
  {
    log->Info(" Gpt Table:");
    log->Info("  - Max Partition Count = {}", gpt.MaxPartitionCount);
  }
  void BeginRawTable(const partition_info_raw&) override
  {
    log->Info(" Raw Table:");
  }
  void MbrEntry(const part_table_entry& entry,
                const part_table_entry_mbr_data& data) override
  {
    log->Info("  Entry:");
    log->Info("   - Offset = {}", entry.partition_offset);
    log->Info("   - Length = {}", entry.partition_length);
    log->Info("   - Number = {}", entry.partition_number);
    log->Info("   - Style = {}", static_cast<uint8_t>(entry.partition_style));

    log->Info("   MBR:");
    log->Info("    - Id = {}", guid_to_string(data.partition_id));
    log->Info("    - Type = {}", data.partition_type);
    log->Info("    - Bootable = {}", data.bootable ? "yes" : "no");
  }
  void GptEntry(const part_table_entry& entry,
                const part_table_entry_gpt_data& data) override
  {
    log->Info("  Entry:");
    log->Info("   - Offset = {}", entry.partition_offset);
    log->Info("   - Length = {}", entry.partition_length);
    log->Info("   - Number = {}", entry.partition_number);
    log->Info("   - Style = {}", static_cast<uint8_t>(entry.partition_style));

    log->Info("   GPT:");
    log->Info("    - Id = {}", guid_to_string(data.partition_id));
    log->Info("    - Type = {}", guid_to_string(data.partition_type));
    log->Info("    - Attributes = {:08X}", data.attributes);

    log->Info("    - Name = {}", utf16_to_utf8(data.name));
  }
  void EndPartTable() override {}

  void BeginExtent(extent_header header) override
  {
    log->Trace("  Extent:");
    log->Trace("   - Length = {}", header.length);
    log->Trace("   - Offset = {}", header.offset);
  }
  void ExtentData(std::span<const char>) override {}
  void EndExtent() override {}

 private:
  std::size_t disk_idx_ = 0;
  GenericLogger* log{};
};

std::vector<auto_fd> open_files(std::span<std::string> filenames)
{
  std::vector<auto_fd> files;
  files.reserve(filenames.size());

  for (auto& filename : filenames) {
    auto_fd fd{open(filename.c_str(), O_WRONLY, 0664)};

    if (!fd) {
      throw std::runtime_error{libbareos::format("could not open '{}': {}",
                                                 filename, strerror(errno))};
    }

    files.emplace_back(std::move(fd));
  }

  return files;
}

int main(int argc, char* argv[])
{
  CLI::App app;

  app.add_flag("--trace", trace, "print additional status information");

  auto* restore = app.add_subcommand("restore");
  std::string filename;
  auto* from = restore
                   ->add_option("--from", filename,
                                "read from this file instead of stdin")
                   ->check(CLI::ExistingFile);

  std::chrono::steady_clock::duration info_frequency = std::chrono::minutes(1);

  auto as_multiplier = [](auto dur) {
    return std::chrono::duration_cast<std::chrono::steady_clock::duration>(dur)
        .count();
  };

  restore
      ->add_option("--freq", info_frequency,
                   "how often should status update occur")
      ->transform(CLI::AsNumberWithUnit(
          std::map<std::string, std::size_t>{
              {"s", as_multiplier(std::chrono::seconds(1))},
              {"m", as_multiplier(std::chrono::minutes(1))},
              {"h", as_multiplier(std::chrono::hours(1))},
          },
          CLI::AsNumberWithUnit::UNIT_REQUIRED
              | CLI::AsNumberWithUnit::CASE_INSENSITIVE,
          "TIME"));

  auto* location = restore->add_option_group(
      "output", "select where the data will be restored");

  auto* stdout = location->add_flag(
      "--stdout", "restore the single disk to stdout (for piping purposes)");

  std::string directory;
  auto* gendir
      = location
            ->add_option(
                "--into", directory,
                "restore the disks to raw files in the given directory")
            ->check(CLI::ExistingDirectory);

  std::vector<std::string> filenames;
  auto* disks = location
                    ->add_option("--onto", filenames,
                                 "write the restored disks into the given "
                                 "files; in the given order")
                    ->check(CLI::ExistingFile);

  location->require_option(1);


  auto* list = app.add_subcommand("list");
  auto* list_from = list->add_option("--from", filename,
                                     "read from this file instead of stdin")
                        ->check(CLI::ExistingFile);

  auto* version = app.add_subcommand("version");

  app.require_subcommand(1, 1);

  CLI11_PARSE(app, argc, argv);

  auto* logger = progressbar::get(trace);

  try {
    if (*restore) {
      std::unique_ptr<GenericHandler> strategy;

      if (*stdout) {
        strategy = std::make_unique<RestoreToStdout>();
      } else if (*gendir) {
        strategy = std::make_unique<RestoreToGeneratedFiles>(directory);
      } else if (*disks) {
        auto files = open_files(filenames);

        strategy = std::make_unique<RestoreToSpecifiedFiles>(std::move(files),
                                                             logger);
      } else {
        throw std::logic_error("i dont know where to restore too!");
      }

      std::ifstream opened_file;
      std::istream* input = std::addressof(std::cin);
      if (*from) {
        logger->Info("using {} as input", filename);
        opened_file.open(filename, std::ios_base::in | std::ios_base::binary);
        input = std::addressof(opened_file);
      }
      parse_file_format(logger, *input, strategy.get());
    } else if (*list) {
      ListContents strategy{logger};

      std::ifstream opened_file;
      std::istream* input = std::addressof(std::cin);
      if (*list_from) {
        logger->Info("using {} as input", filename);
        opened_file.open(filename, std::ios_base::in | std::ios_base::binary);
        input = std::addressof(opened_file);
      }
      parse_file_format(logger, *input, &strategy);
    } else if (*version) {
#if !defined(BARRI_VERSION)
#  warning "no barri version defined"
#  define BARRI_VERSION "unknown"
#endif
#if !defined(BARRI_DATE)
#  warning "no barri date defined"
#  define BARRI_DATE "unknown"
#endif
      std::cout << "barri " << BARRI_VERSION " (" BARRI_DATE ")" << std::endl;
      std::cout << "Copyright (C) 2025-2025 Bareos GmbH & Co. KG" << std::endl;
    } else {
      throw std::logic_error("i dont know what to do");
    }

    return 0;
  } catch (const std::exception& ex) {
    err_msg("{}", ex.what());
    return 1;
  }
}

#define NEW_RESTORE
#include "restore.h"

using handler_ptr = std::unique_ptr<GenericHandler>;

struct data_writer {
  data_writer(GenericLogger* logger_, handler_ptr handler_)
      : logger{logger_}
      , handler{std::move(handler_)}
      , parser{parse_begin(handler.get())}
  {
  }

  std::size_t write(std::span<char> data)
  {
    parse_data(parser, data);
    return data.size();
  }

  ~data_writer() { parse_end(parser); }

  GenericLogger* logger;
  handler_ptr handler;
  restartable_parser* parser;
};

GenericLogger* GetLogger(restore_options options) { return options.logger; }
std::unique_ptr<GenericHandler> GetHandler(restore_options options)
{
  return std::visit(
      [&](auto& val) -> std::unique_ptr<GenericHandler> {
        using T = std::remove_reference_t<decltype(val)>;
        if constexpr (std::is_same_v<T, restore_directory>) {
          return std::make_unique<RestoreToGeneratedFiles>(val.path);
        } else if constexpr (std::is_same_v<T, restore_files>) {
          auto files = open_files(val);
          return std::make_unique<RestoreToSpecifiedFiles>(std::move(files),
                                                           options.logger);
        } else {
          static_assert(!std::is_same_v<T, T>, "case not handled");
        }
      },
      options.location);
}

data_writer* writer_begin(restore_options options)
{
  GenericLogger* logger = GetLogger(options);
  handler_ptr handler = GetHandler(options);
  return new data_writer(logger, std::move(handler));
}
std::size_t writer_write(data_writer* writer, std::span<char> data)
{
  return writer->write(data);
}
void writer_end(data_writer* writer) { delete writer; }
