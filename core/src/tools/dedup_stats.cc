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
   Affero General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
*/
/**
 * @file
 * Program to count records in a dedup volume
 */

#define private public
#include "stored/backends/dedup/dedup_volume.h"
#undef private
#include "lib/cli.h"
#include "lib/version.h"

#include <iostream>

int main(int argc, const char* argv[])
{
  CLI::App app;
  std::string desc(1024, '\0');
  kBareosVersionStrings.FormatCopyright(desc.data(), desc.size(), 2023);
  desc += "The Bareos bdedup-stats Tool.";
  InitCLIApp(app, desc, 0);

  std::string volume;
  app.add_option("-v,--volume,volume", volume)
      ->check(CLI::ExistingDirectory)
      ->required();
  std::vector<int> streams;
  app.add_option("-s,--streams", streams)->check(CLI::PositiveNumber);

  CLI11_PARSE(app, argc, argv);

  dedup::volume vol{volume.c_str(), storagedaemon::DeviceMode::OPEN_READ_ONLY,
                    0};

  if (!vol.is_ok()) {
    std::cerr << volume << " is not a valid dedup volume." << std::endl;
  }

  std::vector<dedup::record_header> records;
  for (auto& record_file : vol.config.recordfiles) {
    if (!record_file.goto_begin()) {
      std::cerr << "Error while reading " << record_file.path << std::endl;
    }

    while (std::optional record = record_file.read_record(0)) {
      records.push_back(*record);
    }
  }

  std::unordered_map<std::size_t, std::size_t> sizes;

  for (auto& record : records) {
    if (record.BareosHeader.Stream >= 0) {
      sizes[record.BareosHeader.DataSize] += 1;
    }
  }

  std::vector<std::pair<std::size_t, std::size_t>> sorted{sizes.begin(),
                                                          sizes.end()};

  std::sort(sorted.begin(), sorted.end(), [](const auto& l, const auto& r) {
    if (l.second == r.second) { return l.first > r.first; }
    return l.second > r.second;
  });

  std::cout << "size : num\n";
  for (auto& p : sorted) { std::cout << p.first << " : " << p.second << "\n"; }

  return 0;
}
