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
#include <unistd.h>

#include <cuchar>

bool trace = false;


template <typename... T>
void trace_msg(fmt::format_string<T...> fmt, T&&... args)
{
  if (trace) { fmt::println(stderr, fmt, std::forward<T>(args)...); }
}

template <typename... T> void err_msg(fmt::format_string<T...> fmt, T&&... args)
{
  fmt::println(stderr, fmt, std::forward<T>(args)...);
}

template <typename... T>
void info_msg(fmt::format_string<T...> fmt, T&&... args)
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

class RestoreToSpecifiedFiles : public GenericHandler {
 public:
  RestoreToSpecifiedFiles(std::vector<auto_fd> files)
      : disk_files{std::move(files)}
  {
  }

  void BeginRestore(std::size_t num_disks) override
  {
    if (disk_files.size() != num_disks)
      throw std::runtime_error{fmt::format(
          "image contains {} disks, but only {} were specified on the "
          "command line",
          num_disks, disk_files.size())};
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
    RestoreToGeneratedFiles strategy{""};
    parse_file_format(input, &strategy);
  }
}


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
  void BeginRestore(std::size_t num_disks) override
  {
    fmt::println(stderr, "Contains {} Disks", num_disks);
  }
  void EndRestore() override {}
  void BeginDisk(disk_info info) override
  {
    fmt::println(stderr, "Disk {}:", disk_idx_);
    fmt::println(stderr, " - Size = {}", info.disk_size);
    fmt::println(stderr, " - Extent Count = {}", info.extent_count);
  }
  void EndDisk() override { disk_idx_ += 1; }

  void BeginMbrTable(const partition_info_mbr&) override
  {
    fmt::println(stderr, " Mbr Table:");
  }
  void BeginGptTable(const partition_info_gpt& gpt) override
  {
    fmt::println(stderr, " Gpt Table:");
    fmt::println(stderr, "  - Max Partition Count = {}", gpt.MaxPartitionCount);
  }
  void BeginRawTable(const partition_info_raw&) override
  {
    fmt::println(stderr, " Raw Table:");
  }
  void MbrEntry(const part_table_entry& entry,
                const part_table_entry_mbr_data& data) override
  {
    fmt::println(stderr, "  Entry:");
    fmt::println(stderr, "   - Offset = {}", entry.partition_offset);
    fmt::println(stderr, "   - Length = {}", entry.partition_length);
    fmt::println(stderr, "   - Number = {}", entry.partition_number);
    fmt::println(stderr, "   - Style = {}",
                 static_cast<uint8_t>(entry.partition_style));

    fmt::println(stderr, "   MBR:");
    fmt::println(stderr, "    - Id = {}", guid_to_string(data.partition_id));
    fmt::println(stderr, "    - Type = {}", data.partition_type);
    fmt::println(stderr, "    - Bootable = {}", data.bootable ? "yes" : "no");
  }
  void GptEntry(const part_table_entry& entry,
                const part_table_entry_gpt_data& data) override
  {
    fmt::println(stderr, "  Entry:");
    fmt::println(stderr, "   - Offset = {}", entry.partition_offset);
    fmt::println(stderr, "   - Length = {}", entry.partition_length);
    fmt::println(stderr, "   - Number = {}", entry.partition_number);
    fmt::println(stderr, "   - Style = {}",
                 static_cast<uint8_t>(entry.partition_style));

    fmt::println(stderr, "   GPT:");
    fmt::println(stderr, "    - Id = {}", guid_to_string(data.partition_id));
    fmt::println(stderr, "    - Type = {}",
                 guid_to_string(data.partition_type));
    fmt::println(stderr, "    - Attributes = {:08X}", data.attributes);

    fmt::println(stderr, "    - Name = {}", utf16_to_utf8(data.name));
  }
  void EndPartTable() override {}

  void BeginExtent(extent_header header) override
  {
    fmt::println(stderr, "  Extent:");
    fmt::println(stderr, "   - Length = {}", header.length);
    fmt::println(stderr, "   - Offset = {}", header.offset);
  }
  void ExtentData(std::span<const char>) override {}
  void EndExtent() override {}

 private:
  std::size_t disk_idx_ = 0;
};

std::vector<auto_fd> open_files(std::span<std::string> filenames)
{
  std::vector<auto_fd> files;
  files.reserve(filenames.size());

  for (auto& filename : filenames) {
    auto_fd fd{open(filename.c_str(), O_WRONLY, 0664)};

    if (!fd) {
      throw std::runtime_error{
          fmt::format("could not open '{}': {}", filename, strerror(errno))};
    }

    files.emplace_back(std::move(fd));
  }

  return files;
}

template <typename BaseHandler> struct WithInfo : GenericHandler {
  using Clock = typename std::chrono::steady_clock;


  template <typename... Args>
  WithInfo(Clock::duration info_frequency_, Args&&... args)
      : handler{std::forward<Args>(args)...}, info_frequency{info_frequency_}
  {
  }

  void BeginRestore(std::size_t num_disks) override
  {
    info_msg("restoring {} disks", num_disks);
    handler.BeginRestore(num_disks);
  }
  void EndRestore() override { handler.EndRestore(); }
  void BeginDisk(disk_info info) override
  {
    current_disk += 1;
    current_disk_size = info.disk_size;
    current_disk_offset = 0;
    last_info_at = Clock::now();
    info_msg("restoring disk {} of size {}", current_disk, current_disk_size);
    handler.BeginDisk(info);
  }
  void EndDisk() override
  {
    AdvanceTo(current_disk_size);
    handler.EndDisk();
    info_msg("finished restoring disk {}", current_disk);
  }

  void BeginMbrTable(const partition_info_mbr& mbr) override
  {
    info_msg("start restoring mbr table");
    handler.BeginMbrTable(mbr);
  }
  void BeginGptTable(const partition_info_gpt& gpt) override
  {
    info_msg("start restoring gpt table");
    handler.BeginGptTable(gpt);
  }
  void BeginRawTable(const partition_info_raw& raw) override
  {
    handler.BeginRawTable(raw);
  }
  void MbrEntry(const part_table_entry& entry,
                const part_table_entry_mbr_data& data) override
  {
    handler.MbrEntry(entry, data);
  }
  void GptEntry(const part_table_entry& entry,
                const part_table_entry_gpt_data& data) override
  {
    handler.GptEntry(entry, data);
  }
  void EndPartTable() override
  {
    info_msg("start restoring disk data");
    handler.EndPartTable();
  }

  void BeginExtent(extent_header header) override
  {
    AdvanceTo(header.offset);
    handler.BeginExtent(header);
  }

  void ExtentData(std::span<const char> data) override
  {
    static constexpr std::size_t block_size = std::size_t{4} << 20;

    while (data.size() > block_size) {
      auto block = data.subspan(0, block_size);
      Write(block);
      data = data.subspan(block.size());
    }
    Write(data);
  }

  void EndExtent() override { handler.EndExtent(); }

  void AdvanceTo(std::size_t next_disk_offset)
  {
    if (next_disk_offset == current_disk_size) {
      info_msg(". 100% ({0}/{0})", current_disk_size);
      current_disk_offset = next_disk_offset;
      return;
    }

    Clock::time_point now = Clock::now();
    if (now - last_info_at >= info_frequency) {
      info_msg(". {:3}% ({}/{})", (100 * next_disk_offset) / current_disk_size,
               next_disk_offset, current_disk_size);

      last_info_at = now;
    }
    current_disk_offset = next_disk_offset;
  }
  void Write(std::span<char const> data)
  {
    handler.ExtentData(data);
    AdvanceTo(current_disk_offset + data.size());
  }

  virtual ~WithInfo() = default;

  BaseHandler handler;
  std::size_t current_disk{};
  std::size_t current_disk_size{};
  std::size_t current_disk_offset{};
  Clock::time_point last_info_at;
  Clock::duration info_frequency;
};

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

  app.require_subcommand(1, 1);

  CLI11_PARSE(app, argc, argv);
  try {
    if (*restore) {
      std::unique_ptr<GenericHandler> strategy;

      if (*stdout) {
        strategy = std::make_unique<WithInfo<RestoreToStdout>>(info_frequency,
                                                               std::cout);
      } else if (*gendir) {
        strategy = std::make_unique<WithInfo<RestoreToGeneratedFiles>>(
            info_frequency, directory);
      } else if (*disks) {
        auto files = open_files(filenames);

        strategy = std::make_unique<WithInfo<RestoreToSpecifiedFiles>>(
            info_frequency, std::move(files));
      } else {
        throw std::logic_error("i dont know where to restore too!");
      }

      std::ifstream opened_file;
      std::istream* input = std::addressof(std::cin);
      if (*from) {
        trace_msg("using {} as input", filename);
        opened_file.open(filename, std::ios_base::in | std::ios_base::binary);
        input = std::addressof(opened_file);
      }
      parse_file_format(*input, strategy.get());
    } else if (*list) {
      ListContents strategy;

      std::ifstream opened_file;
      std::istream* input = std::addressof(std::cin);
      if (*list_from) {
        trace_msg("using {} as input", filename);
        opened_file.open(filename, std::ios_base::in | std::ios_base::binary);
        input = std::addressof(opened_file);
      }
      parse_file_format(*input, &strategy);
    } else {
      throw std::logic_error("i dont know what to do");
    }

    return 0;
  } catch (const std::exception& ex) {
    err_msg("{}", ex.what());
    return 1;
  }
}
