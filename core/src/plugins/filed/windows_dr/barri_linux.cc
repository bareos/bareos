#include "CLI/CLI.hpp"
#include "CLI/App.hpp"
#include "CLI/Config.hpp"
#include "CLI/Formatter.hpp"

#include <cuchar>

#include "logger.h"

#define NEW_RESTORE
#include "restore.h"

bool trace = false;

// template <typename... T>
// void trace_msg(fmt::format_string<T...> fmt, T&&... args)
// {
//   if (trace) { fmt::println(stderr, fmt, std::forward<T>(args)...); }
// }

template <typename... T> void err_msg(fmt::format_string<T...> fmt, T&&... args)
{
  fmt::println(stderr, fmt, std::forward<T>(args)...);
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

  auto* location = restore->add_option_group(
      "output", "select where the data will be restored to");

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
      // std::unique_ptr<GenericHandler> strategy;


      // parse_file_format(logger, *input, strategy.get());

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
        } else {
          throw std::logic_error("i dont know where to restore too!");
        }
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

        parse_data(parser, buffer.subspan(input->gcount()));

        if (input->eof()) { break; }
      }

      parse_end(parser);
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
