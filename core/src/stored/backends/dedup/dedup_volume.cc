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

#include "dedup_volume.h"

namespace dedup {
constexpr config::loaded_general_info my_general_info = {
    .block_header_size = sizeof(bareos_block_header),
    .record_header_size = sizeof(bareos_record_header),
    .dedup_block_header_size = sizeof(block_header),
    .dedup_record_header_size = sizeof(record_header),
};

void volume::write_current_config()
{
  std::vector<config::loaded_block_section> blocksections;
  std::vector<config::loaded_record_section> recordsections;
  std::vector<config::loaded_data_section> datasections;
  std::vector<config::loaded_unfinished_record> unfinished;

  for (auto& blockfile : contents.blockfiles) {
    blocksections.emplace_back(blockfile.begin(), blockfile.end(),
                               blockfile.path());
  }
  for (auto& recordfile : contents.recordfiles) {
    recordsections.emplace_back(recordfile.begin(), recordfile.end(),
                                recordfile.path());
  }
  for (auto& datafile : contents.datafiles) {
    datasections.emplace_back(datafile.index(), datafile.blocksize(),
                              datafile.path(), datafile.end());
  }

  for (auto& [record, loc] : unfinished_records) {
    unfinished.emplace_back(record.VolSessionId, record.VolSessionTime,
                            record.FileIndex, record.Stream, loc.file_index,
                            loc.current, loc.end - loc.current);
  }


  std::vector<std::byte> bytes = config::to_bytes(
      my_general_info, datasections, recordsections, blocksections, unfinished);
  if (!config.resize(0)) {
    error = true;
  } else if (!config.write_at(0, bytes.data(), bytes.size())) {
    error = true;
  }
}

std::optional<volume_layout> volume::load_layout()
{
  std::optional size = config.size_then_reset();

  if (!size.has_value()) { return std::nullopt; }

  std::vector<std::byte> bytes(size.value());

  if (!config.read(bytes.data(), bytes.size())) {
    // error: cannot read config file
    return std::nullopt;
  }

  std::optional loaded_config = config::from_bytes(bytes);
  if (!loaded_config) { return std::nullopt; }

  // at the moment we only support configurations that have
  // exactly one block and one record file.
  // This might change in the future
  if (loaded_config->blockfiles.size() != 1) {
    // error: to many/few block files
    return std::nullopt;
  }

  if (loaded_config->recordfiles.size() != 1) {
    // error: to many/few record files
    return std::nullopt;
  }

  if (loaded_config->info.block_header_size != sizeof(bareos_block_header)
      || loaded_config->info.dedup_block_header_size != sizeof(block_header)) {
    // error: bad block header size
    return std::nullopt;
  }

  if (loaded_config->info.record_header_size != sizeof(bareos_record_header)
      || loaded_config->info.dedup_record_header_size
             != sizeof(record_header)) {
    // error: bad record header size
    return std::nullopt;
  }

  for (auto& rec : loaded_config->unfinished) {
    // FIXME: check wether this write_loc makes any sense!
    record unfinished_record{rec.VolSessionId, rec.VolSessionTime,
                             rec.FileIndex, rec.Stream};
    write_loc loc{rec.DataIdx, rec.file_offset, rec.file_offset + rec.size};
    auto [_, inserted] = unfinished_records.emplace(unfinished_record, loc);
    if (!inserted) {
      // error: bad unfinished record
      return std::nullopt;
    }
  }

  return volume_layout(std::move(loaded_config.value()));
}
} /* namespace dedup */
