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
    blocksections.emplace_back(blockfile.begin(), blockfile.size(),
                               blockfile.path());
  }
  for (auto& recordfile : contents.recordfiles) {
    recordsections.emplace_back(recordfile.begin(), recordfile.size(),
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

class gap_list {
  using interval = std::pair<std::size_t, std::size_t>;

 public:
  bool add(std::size_t start, std::size_t end)
  {
    if (start >= end) return true;

    if (vec.size() == 0) {
      vec.emplace_back(start, end);
      return true;
    }
    auto iter
        = std::lower_bound(vec.begin(), vec.end(), start,
                           [](const auto& interval, const std::size_t& val) {
                             return interval.first < val;
                           });

    if (iter == vec.end()) {
      auto& inter = vec.front();  // we already checked that size > 1!

      if (inter.first < end) {
        // there is some overlap which we do not allow
        return false;
      } else if (inter.first == end) {
        inter.first = start;
        return true;
      } else {
        vec.insert(vec.begin(), interval{start, end});
        return true;
      }
    }

    auto next = iter + 1;

    if (next == vec.end()) {
      auto& inter = *iter;
      if (inter.second > start) {
        // there is some overlap which we do not allow
        return false;
      } else if (inter.second == start) {
        inter.second = end;
        return true;
      } else {
        vec.emplace_back(start, end);
        return true;
      }
    }

    bool combines_with_iter = iter->second == start;
    bool combines_with_next = next->first == end;

    if (combines_with_iter && combines_with_next) {
      iter->second = next->second;
      vec.erase(next);
    } else if (combines_with_iter) {
      iter->second = end;
    } else if (combines_with_next) {
      next->first = start;
    } else {
      vec.insert(next, interval{start, end});
    }

    return true;
  }

  bool has_gaps() const { return vec.size() > 1; }

 private:
  // vec is a sorted vector of half open intervals [first, second)
  std::vector<interval> vec;
};

static bool validate_config(const config::loaded_config& conf)
{
  // at the moment we only support configurations that have
  // at least one block and one record file.
  // This might change in the future
  if (conf.blockfiles.size() == 0) {
    // error: few block files
    return false;
  }

  gap_list block_gaps;
  for (auto& blockfile : conf.blockfiles) {
    if (!block_gaps.add(blockfile.start_block,
                        blockfile.start_block + blockfile.num_blocks)) {
      // todo: print duplicates
      return false;
    }
  }

  if (block_gaps.has_gaps()) {
    // warning: gaps!
  }

  if (conf.recordfiles.size() == 0) {
    // error: few record files
    return false;
  }

  gap_list record_gaps;
  for (auto& recordfile : conf.recordfiles) {
    if (!record_gaps.add(recordfile.start_record,
                         recordfile.start_record + recordfile.num_records)) {
      // todo: print duplicates
      return false;
    }
  }

  gap_list file_indices;
  for (auto& datafile : conf.datafiles) {
    if (!file_indices.add(datafile.file_index, datafile.file_index + 1)) {
      // todo: print duplicates
      return false;
    }
  }

  if (file_indices.has_gaps()) {
    // warning: missing file indices
  }


  if (record_gaps.has_gaps()) {
    // warning: gaps!
  }

  if (conf.info.block_header_size != sizeof(bareos_block_header)
      || conf.info.dedup_block_header_size != sizeof(block_header)) {
    // error: bad block header size
    return false;
  }

  if (conf.info.record_header_size != sizeof(bareos_record_header)
      || conf.info.dedup_record_header_size != sizeof(record_header)) {
    // error: bad record header size
    return false;
  }

  return true;
}

std::optional<volume_layout> volume::load_layout()
{
  std::optional size = config.size();

  if (!size.has_value()) { return std::nullopt; }

  std::vector<std::byte> bytes(size.value());

  if (!config.read_at(0, bytes.data(), bytes.size())) {
    // error: cannot read config file
    return std::nullopt;
  }

  std::optional loaded_config = config::from_bytes(bytes);
  if (!loaded_config) { return std::nullopt; }
  if (!validate_config(*loaded_config)) { return std::nullopt; }

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
