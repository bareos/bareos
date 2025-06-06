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

#include "CLI/App.hpp"
#include "CLI/Config.hpp"
#include "CLI/Formatter.hpp"

#include <stdexcept>
#include <fmt/format.h>
#include <variant>
#include <optional>
#include <limits>
#include <cstdint>

#include <gsl/narrow>

#include <fcntl.h>
#include <unistd.h>

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
          fmt::format("Trying to skip to offset {}, when already at offset {}",
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
  }

  void write(std::span<const char> bytes)
  {
    auto towrite = bytes;

    std::size_t bad_count = 0;

    while (towrite.size() > 0) {
      auto res = ::write(fd_, towrite.data(), towrite.size());
      if (res < 0) {
        throw std::runtime_error(
            fmt::format("write() failed: {}", strerror(errno)));
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
      throw std::logic_error{"cannot begin disk after one was created"};
    }

    auto& fd = disk_files[current_idx];

    if (ftruncate(fd.get(), disk_size) < 0) {
      throw std::runtime_error{
          fmt::format("could not expand disk: Err={}", strerror(errno))};
    }

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
      } else if (*files) {
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
