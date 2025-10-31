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

#include <cuchar>

#include "logger.h"
#include "barri_cli.h"
#include "format.h"

#include "restore.h"

bool trace = false;

template <typename... T>
void err_msg(libbareos::format_string<T...> fmt, T&&... args)
{
  std::cerr << libbareos::format(fmt, std::forward<T>(args)...) << '\n';
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

  return libbareos::format("{:08X}-{:04X}-{:04X}-{:08X}{:04X}", First, Second,
                           Third, Fourth, Fifth);
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

    log->Info("    - Name = {}", utf16_to_utf8(data.name.data));
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

int main(int argc, char* argv[])
{
  CLI::App app;

  app.description(version_text() + "\n");

  app.add_flag("--trace", trace, "print additional status information");

  auto name = argc > 0 ? argv[0] : "barri-cli";

  auto* restore
      = app.add_subcommand("restore",
                           "restore a barri backup to the given output(s)")
            ->formatter(std::make_shared<SubcommandFormatter>(
                R"(This command restores a barri backup to some output.
The backup is read from stdin; alternatively you can specify a file to read from via the --file option.

When output to a terminal, the progress will be displayed in a progress bar.)"))
            ->fallthrough()
            ->footer(libbareos::format(R"(Examples:
  # restore
  #   - the first disk to /dev/sda,
  #   - the second disk to /dev/null (i.e. it is not restored), and
  #   - the third disk to /dev/nvme1n0
  {0} restore --from backup.barri --to-disks /dev/sda /dev/null /dev/nvme1n0

  # restore to some network block device
  # here `get-backup` is any command that outputs the barri image to stdout
  get-backup | {0} restore --stdout | nbdcopy ...

  # restore the disks into /tmp/disks, and save the restore debug trace
  cat backup.barri | {0} restore --to-raw-directory /tmp/disks --trace 2>/tmp/trace.log)",
                                       name));
  std::string filename;
  auto* from = restore
                   ->add_option("--from", filename,
                                "read from this file instead of stdin")
                   ->check(CLI::ExistingFile);

  auto* location = restore->add_option_group(
      "output", "select where the data will be restored to");

  // this is only used to disambiguate an overload
  bool stdout_unused = false;
  auto* stdout = location->add_flag(
      "--stdout", stdout_unused,
      "restore the single disk to stdout (for piping purposes)");

  std::string directory;
  auto* gendir
      = location
            ->add_option(
                "--to-raw-directory", directory,
                "restore the disks to raw files in the given directory")
            ->check(CLI::ExistingDirectory);

  std::vector<std::string> filenames;
  auto* disks = location
                    ->add_option("--to-disks", filenames,
                                 "write the restored disks into the given "
                                 "disk devices; in the given order")
                    ->check(CLI::ExistingFile);

  location->require_option(1);


  auto* list
      = app.add_subcommand("list", "list the contents of a barri backup")
            ->formatter(std::make_shared<SubcommandFormatter>(libbareos::format(
                R"(This command lists the contents of a barri backup.
This information may be useful if you need to match up which backed up drive should get restored to which path)",
                name)))
            ->fallthrough()
            ->footer(libbareos::format(R"(Examples:
  # list the contents of a backup from disk
  {0} list --from backup.barri

  # list the contents of a backup from stdin
  # here `get-backup` is any command that outputs the barri image to stdout
  get-backup | {0} list)",
                                       name));
  auto* list_from = list->add_option("--from", filename,
                                     "read from this file instead of stdin")
                        ->check(CLI::ExistingFile);

  auto* version
      = app.add_subcommand("version", "print the version information")
            ->formatter(std::make_shared<SubcommandFormatter>(libbareos::format(
                R"(This command outputs the version of {0} to stdout)", name)))
            ->fallthrough();

  app.require_subcommand(1, 1);

  CLI11_PARSE(app, argc, argv);

  auto* logger = progressbar::get(trace);

  try {
    if (*restore) {
      logger->Info("{}", version_text());

      std::ifstream opened_file;
      std::istream* input = std::addressof(std::cin);
      if (*from) {
        logger->Info("using {} as input", filename);
        opened_file.open(filename, std::ios_base::in | std::ios_base::binary);
        input = std::addressof(opened_file);
      }

      auto handler = [&] {
        using namespace barri::restore;
        if (*stdout) {
          return GetHandler(logger, restore_stdout{});
        } else if (*gendir) {
          return GetHandler(logger, restore_directory{directory});
        } else if (*disks) {
          return GetHandler(logger, restore_files{filenames});
        }
        throw std::logic_error("I dont know where to restore too!");
      }();

      auto* parser = parse_begin(handler.get(), logger);

      std::vector<char> buffer_storage;
      buffer_storage.resize(64 << 10);
      std::span<char> buffer = std::span{buffer_storage};

      for (;;) {
        input->read(buffer.data(), buffer.size());

        if (input->bad() || input->exceptions()) {
          err_msg("could not read from input: bad = {}, exception = {}",
                  input->bad() ? "yes" : "no",
                  input->exceptions() ? "yes" : "no");
          break;
        }

        parse_data(parser, buffer.subspan(0, input->gcount()));

        if (input->eof()) { break; }
      }

      parse_end(parser);
    } else if (*list) {
      ListContents strategy{logger};

      logger->Info("{}", version_text());

      std::ifstream opened_file;
      std::istream* input = std::addressof(std::cin);
      if (*list_from) {
        logger->Info("using {} as input", filename);
        opened_file.open(filename, std::ios_base::in | std::ios_base::binary);
        input = std::addressof(opened_file);
      }
      parse_file_format(logger, *input, &strategy);
    } else if (*version) {
      std::cout << version_text() << std::endl;
    } else {
      // this should never happen, as we tell cli11 that we need at least
      // one subcommand
      throw std::logic_error("i dont know what to do");
    }

    return 0;
  } catch (const std::exception& ex) {
    err_msg("{}", ex.what());
    return 1;
  }
}
