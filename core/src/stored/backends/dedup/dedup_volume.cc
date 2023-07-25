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

#include <unordered_map>

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

  auto layout = contents.make_layout();

  for (auto& blockfile : layout.blockfiles) {
    blocksections.emplace_back(blockfile.start, blockfile.count,
                               std::move(blockfile.path));
  }
  for (auto& recordfile : layout.recordfiles) {
    recordsections.emplace_back(recordfile.start, recordfile.count,
                                std::move(recordfile.path));
  }
  for (auto& datafile : layout.datafiles) {
    datasections.emplace_back(datafile.file_index, datafile.chunk_size,
                              std::move(datafile.path), datafile.bytes_used);
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
    auto next
        = std::lower_bound(vec.begin(), vec.end(), start,
                           [](const auto& interval, const std::size_t& val) {
                             return interval.first < val;
                           });

    // next points to the first interval with first >= val
    if (next == vec.end()) {
      auto& inter = vec.back();  // we already checked that size > 1!

      if (inter.second > start) {
        // there is some overlap which we do not allow
        return false;
      } else if (inter.second == start) {
        inter.second = end;
      } else {
        vec.emplace_back(start, end);
      }
      return true;
    }

    if (next == vec.begin()) {
      auto& inter = vec.front();
      if (inter.first < end) {
        // there is some overlap which we do not allow
        return false;
      } else if (inter.first == end) {
        inter.first = start;
      } else {
        vec.insert(vec.begin(), interval{start, end});
      }
      return true;
    }

    auto prev = next - 1;

    bool combines_with_next = next->first == end;
    bool combines_with_prev = prev->second == start;

    if (combines_with_next && combines_with_prev) {
      prev->second = next->second;
      vec.erase(next);
    } else if (combines_with_next) {
      next->first = start;
    } else if (combines_with_prev) {
      prev->second = end;
    } else {
      vec.insert(next, interval{start, end});
    }

    return true;
  }

  std::vector<interval> gaps() const
  {
    std::vector<interval> gaps{};

    for (std::size_t i = 0; i + 1 < vec.size(); ++i) {
      gaps.emplace_back(vec[i].second, vec[i + 1].first);
    }

    return gaps;
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
    Emsg0(M_ERROR, 0, "no block files found.\n");
    return false;
  }

  gap_list block_gaps;
  for (auto& blockfile : conf.blockfiles) {
    if (!block_gaps.add(blockfile.start_block,
                        blockfile.start_block + blockfile.num_blocks)) {
      Emsg0(M_ERROR, 0, "duplicate blocks detected:\n");
      auto begin = blockfile.start_block;
      auto end = blockfile.start_block + blockfile.num_blocks;
      std::size_t current_index = &blockfile - conf.blockfiles.data();

      for (std::size_t i = 0; i < current_index; ++i) {
        auto& other = conf.blockfiles[i];
        auto begin_i = other.start_block;
        auto end_i = other.start_block + other.num_blocks;
        if (begin < end_i && begin_i < end) {
          Emsg0(M_ERROR, 0,
                "blockfiles %s and %s each contain blocks %zu-%zu\n",
                blockfile.path.c_str(), other.path.c_str(),
                std::max(begin_i, begin), std::min(end_i, end) - 1);
        }
      }

      return false;
    }
  }

  if (block_gaps.has_gaps()) {
    Dmsg0(150, "block gaps detected:\n");
    for (auto gap : block_gaps.gaps()) {
      Dmsg0(150, "blocks %zu-%zu are missing.\n", gap.first, gap.second - 1);
    }
  }

  if (conf.recordfiles.size() == 0) {
    Emsg0(M_ERROR, 0, "no record files found.\n");
    return false;
  }

  gap_list record_gaps;
  for (auto& recordfile : conf.recordfiles) {
    if (!record_gaps.add(recordfile.start_record,
                         recordfile.start_record + recordfile.num_records)) {
      Emsg0(M_ERROR, 0, "duplicate records detected:\n");
      auto begin = recordfile.start_record;
      auto end = recordfile.start_record + recordfile.num_records;
      std::size_t current_index = &recordfile - conf.recordfiles.data();

      for (std::size_t i = 0; i < current_index; ++i) {
        auto& other = conf.recordfiles[i];
        auto begin_i = other.start_record;
        auto end_i = other.start_record + other.num_records;
        if (begin < end_i && begin_i < end) {
          Emsg0(M_ERROR, 0,
                "recordfiles %s and %s each contain records %zu-%zu\n",
                recordfile.path.c_str(), other.path.c_str(),
                std::max(begin_i, begin), std::min(end_i, end) - 1);
        }
      }
      return false;
    }
  }

  if (record_gaps.has_gaps()) {
    Dmsg0(150, "record gaps detected:\n");
    for (auto gap : record_gaps.gaps()) {
      Dmsg0(150, "records %zu-%zu are missing.\n", gap.first, gap.second - 1);
    }
  }

  gap_list file_indices;
  for (auto& datafile : conf.datafiles) {
    if (!file_indices.add(datafile.file_index, datafile.file_index + 1)) {
      Emsg0(M_ERROR, 0, "duplicate data file indices detected:\n");
      auto index = datafile.file_index;
      std::size_t current_index = &datafile - conf.datafiles.data();

      for (std::size_t i = 0; i < current_index; ++i) {
        auto& other = conf.datafiles[i];
        auto index_i = other.file_index;
        if (index == index_i) {
          Emsg0(M_ERROR, 0, "datafiles %s and %s have the same index %zu\n",
                datafile.path.c_str(), other.path.c_str(), index);
        }
      }
      return false;
    }
  }

  if (file_indices.has_gaps()) {
    // this is not as bad as the previous errors, so display it only
    // with a much higher debug level

    Dmsg0(500, "data file index gaps detected:\n");
    for (auto gap : file_indices.gaps()) {
      Dmsg0(500, "data files with indices %zu-%zu are missing.\n", gap.first,
            gap.second - 1);
    }
  }

  if (conf.info.block_header_size != sizeof(bareos_block_header)) {
    Emsg0(M_ERROR, 0,
          "bareos block header differ in size: Got: %d, Expected: %d\n",
          conf.info.block_header_size, sizeof(bareos_block_header));
    return false;
  }

  if (conf.info.dedup_block_header_size != sizeof(block_header)) {
    Emsg0(M_ERROR, 0,
          "dedup block header differ in size: Got: %d, Expected: %d\n",
          conf.info.dedup_block_header_size, sizeof(block_header));
    return false;
  }

  if (conf.info.record_header_size != sizeof(bareos_record_header)) {
    Emsg0(M_ERROR, 0,
          "bareos record header differ in size: Got: %d, Expected: %d\n",
          conf.info.record_header_size, sizeof(bareos_record_header));
    return false;
  }

  if (conf.info.dedup_record_header_size != sizeof(record_header)) {
    Emsg0(M_ERROR, 0,
          "dedup record header differ in size: Got: %d, Expected: %d\n",
          conf.info.dedup_record_header_size, sizeof(record_header));
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
    Emsg0(M_ERROR, 0, "Could not read config file %s correctly.\n",
          config.relative_path());
    return std::nullopt;
  }

  std::optional loaded_config = config::from_bytes(bytes);
  if (!loaded_config) { return std::nullopt; }
  if (!validate_config(*loaded_config)) { return std::nullopt; }

  std::unordered_map<std::size_t, std::size_t> file_size;
  std::unordered_map<std::size_t, const char*> file_name;

  for (auto& datafile : loaded_config->datafiles) {
    file_size[datafile.file_index] = datafile.data_used;
    file_name[datafile.file_index] = datafile.path.c_str();
  }

  for (auto& rec : loaded_config->unfinished) {
    auto found = file_size.find(rec.FileIndex);
    if (found == file_size.end()) {
      Emsg0(M_ERROR, 0,
            "Unfinished record points to non existent file index %ld\n",
            rec.FileIndex);
      return std::nullopt;
    }
    if (found->second < rec.file_offset + rec.size) {
      Emsg0(M_ERROR, 0,
            "Unfinished record points to unallocated file memory [%ld,%ld] in "
            "file %s (index: %ld) of size %zu\n",
            rec.file_offset, rec.file_offset + rec.size - 1,
            file_name[rec.FileIndex], rec.FileIndex, found->second);
      return std::nullopt;
    }

    record unfinished_record{rec.VolSessionId, rec.VolSessionTime,
                             rec.FileIndex, rec.Stream};
    write_loc loc{rec.DataIdx, rec.file_offset, rec.file_offset + rec.size};
    auto [_, inserted] = unfinished_records.emplace(unfinished_record, loc);
    if (!inserted) {
      Emsg0(M_ERROR, 0, "Found duplicate unfinished records.\n");
      return std::nullopt;
    }
  }

  return volume_layout(std::move(loaded_config.value()));
}
} /* namespace dedup */
