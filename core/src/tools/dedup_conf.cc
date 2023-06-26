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
#include "lib/cli.h"
#include "lib/version.h"

#include "stored/backends/dedup/dedup_config.h"
#include "stored/backends/dedup/dedup_volume.h"

#include <iostream>
#include <filesystem>

void print_config(const dedup::config::loaded_config& conf)
{
  std::cout << "{\n";
  std::cout << " \"version\" = " << conf.version << ",\n"
            << " \"file_size\" = " << conf.file_size << ",\n"
            << " \"section_alignment\" = " << conf.section_alignment << ",\n"
            << " \"block_header_size\" = " << conf.info.block_header_size
            << ",\n"
            << " \"section_header_size\" = " << conf.info.record_header_size
            << ",\n";
  {
    std::cout << " \"blockfiles\" = [";
    bool first = true;
    for (auto& blockfile : conf.blockfiles) {
      if (first) {
        first = false;
      } else {
        std::cout << "\n                 ";
      }
      std::cout << "{ "
                << "Name: \"" << blockfile.path << "\", "
                << "Start: " << blockfile.start_block << ", "
                << "End: " << blockfile.end_block << ", "
                << "Index: " << blockfile.file_index << " }";
    }
    std::cout << "],\n";
  }
  {
    std::cout << " \"recordfiles\" = [";
    bool first = true;
    for (auto& recordfile : conf.recordfiles) {
      if (first) {
        first = false;
      } else {
        std::cout << "\n                  ";
      }
      std::cout << "{ "
                << "Name: \"" << recordfile.path << "\", "
                << "Start: " << recordfile.start_record << ", "
                << "End: " << recordfile.end_record << ", "
                << "Index: " << recordfile.file_index << " }";
    }
    std::cout << "],\n";
  }
  {
    std::cout << " \"datafiles\" = [";
    bool first = true;
    for (auto& datafile : conf.datafiles) {
      if (first) {
        first = false;
      } else {
        std::cout << "\n                ";
      }
      std::cout
          << "{ "
          << "Name: \"" << datafile.path << "\", "
          << "BytesUsed: " << datafile.data_used << ", "
          << "BlockSize: " << datafile.block_size
          << ", "
          // << "ReadOnly: " << (datafile.read_only ? "yes" : "no") << ", "
          << "Index: " << datafile.file_index << " }";
    }
    std::cout << "]\n";
  }
  std::cout << "}\n";
}

namespace fs = std::filesystem;

int main(int argc, char* argv[])
{
  CLI::App app;
  std::string desc(1024, '\0');
  kBareosVersionStrings.FormatCopyright(desc.data(), desc.size(), 2023);
  desc += "The Bareos DryDedup Tool.";
  InitCLIApp(app, desc, 0);

  std::string volume;
  app.add_option("-v,--volume,volume", volume)
      ->check(CLI::ExistingDirectory)
      ->required();

  CLI11_PARSE(app, argc, argv);

  fs::path volume_path = volume;
  fs::path config_path = volume_path / "config";

  std::error_code error;
  if (!fs::is_regular_file(config_path, error)) {
    std::cerr << "'" << config_path << "' could not be loaded (ec = " << error
              << ").\n";
    return 1;
  }

  std::vector<std::byte> data;
  if (std::ifstream file{config_path, std::ios::binary | std::ios::ate}) {
    auto size = file.tellg();
    data.resize(size);
    file.seekg(0);
    char* data_ptr = reinterpret_cast<char*>(&*data.begin());
    if (!file.read(data_ptr, size)) {
      std::cerr << "'" << config_path << "' could not be read.\n";
      return 1;
    }
    if (file.gcount() != size) {
      std::cerr << "Bad read from '" << config_path << "'.\n";
      return 1;
    }
  } else {
    std::cerr << "'" << config_path << "' could not be opened.\n";
    return 1;
  }

  auto config = dedup::config::from_bytes(data);
  if (!config.has_value()) {
    std::cerr << "'" << config_path
              << "' does not contain a valid dedup config.\n";
    return 1;
  }

  print_config(*config);

  return 0;
}
