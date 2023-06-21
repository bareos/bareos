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

#include "dedup_config.h"
#include <algorithm>

namespace dedup::config {
using iter = std::vector<std::byte>::const_iterator;

std::uint64_t CalculateCheckSum(const std::byte* begin, const std::byte* end)
{
  std::uint64_t checksum = 1103;

  for (const std::byte* current = begin; current != end; ++current) {
    checksum *= 101;
    checksum += static_cast<std::uint64_t>(*current);
  }

  return checksum;
}

std::vector<std::byte> serialize_string(const std::string& s)
{
  // we always write in utf-8
  // we assume that the given string is in utf-8 already
  // in situations where this is not the case, we need to convert
  // the string to utf-8 here
  std::vector<std::byte> bytes{reinterpret_cast<const std::byte*>(&*s.begin()),
                               reinterpret_cast<const std::byte*>(&*s.end())};
  return bytes;
}

std::string deserialize_string(iter begin, iter end)
{
  // the bytes we are given represent a utf-8 string
  // we assume that std::string has an utf-8 encoding here
  // otherwise we would need to convert the data first.
  // We could use std::u8string in C++20 here but there
  // is no builtin way to convert u8string <-> string
  // in C++20 (not in 23 either)
  std::string string{reinterpret_cast<const char*>(&*begin),
                     reinterpret_cast<const char*>(&*end)};
  return string;
}

std::size_t alignment_diff(std::size_t current, std::size_t alignment)
{
  auto diff = current % alignment;
  auto offset = (alignment - diff) % alignment;

  return offset;
}

void make_aligned(iter& current, iter begin, std::size_t alignment)
{
  current += alignment_diff(current - begin, alignment);
}

std::vector<std::byte> serialize_with_header(std::uint16_t version,
                                             config::section_header::types type,
                                             std::size_t data_align,
                                             const std::vector<std::byte>& data)
{
  config::section_header header;

  header.type = static_cast<std::uint16_t>(type);
  header.version = version;
  header.padding = alignment_diff(sizeof(header), data_align);
  header.written_order = static_cast<std::uint8_t>(byte_order::big_endian);
  header.data_size = data.size();
  header.data_checksum = CalculateCheckSum(&*data.begin(), &*data.end());
  header.checksum
      = CalculateCheckSum(reinterpret_cast<std::byte*>(&header),
                          reinterpret_cast<std::byte*>(&header.checksum));

  std::vector<std::byte> bytes{
      reinterpret_cast<const std::byte*>(&header),
      reinterpret_cast<const std::byte*>(&header) + sizeof(header)};
  bytes.reserve(sizeof(header) + header.padding + data.size());
  bytes.resize(bytes.size() + header.padding);
  bytes.insert(bytes.end(), data.begin(), data.end());

  return bytes;
}

std::vector<std::byte> serialize_general_info(const loaded_general_info& info)
{
  config::general_section general;

  general.block_header_size = info.block_header_size;
  general.record_header_size = info.record_header_size;
  general.dedup_block_header_size = info.dedup_block_header_size;
  general.dedup_record_header_size = info.dedup_record_header_size;

  std::vector<std::byte> data{
      reinterpret_cast<std::byte*>(&general),
      reinterpret_cast<std::byte*>(&general) + sizeof(general)};

  return serialize_with_header(0, config::section_header::types::GeneralInfo,
                               alignof(general), data);
}

std::vector<std::byte> serialize_block_file(loaded_block_section blockfile)
{
  config::block_file_section network;

  network.file_index = blockfile.file_index;
  network.start_block = blockfile.start_block;
  network.end_block = blockfile.end_block;
  network.path_length = blockfile.path.size();

  std::vector<std::byte> data{
      reinterpret_cast<std::byte*>(&network),
      reinterpret_cast<std::byte*>(&network) + sizeof(network)};
  std::vector<std::byte> string = serialize_string(blockfile.path);
  data.insert(data.end(), string.begin(), string.end());

  return serialize_with_header(0, config::section_header::types::BlockFile,
                               alignof(network), data);
}

std::vector<std::byte> serialize_record_file(loaded_record_section recordfile)
{
  config::record_file_section network;

  network.file_index = recordfile.file_index;
  network.start_record = recordfile.start_record;
  network.end_record = recordfile.end_record;
  network.path_length = recordfile.path.size();

  std::vector<std::byte> data{
      reinterpret_cast<std::byte*>(&network),
      reinterpret_cast<std::byte*>(&network) + sizeof(network)};
  std::vector<std::byte> string = serialize_string(recordfile.path);
  data.insert(data.end(), string.begin(), string.end());

  return serialize_with_header(0, config::section_header::types::RecordFile,
                               alignof(network), data);
}

std::vector<std::byte> serialize_data_file(const loaded_data_section& datafile)
{
  config::data_file_section network;

  network.block_size = datafile.block_size;
  network.file_index = datafile.file_index;
  network.path_length = datafile.path.size();

  std::vector<std::byte> data{
      reinterpret_cast<std::byte*>(&network),
      reinterpret_cast<std::byte*>(&network) + sizeof(network)};
  std::vector<std::byte> string = serialize_string(datafile.path);
  data.insert(data.end(), string.begin(), string.end());

  return serialize_with_header(0, config::section_header::types::DataFile,
                               alignof(network), data);
}


std::vector<std::byte> to_bytes(
    loaded_general_info info,
    const std::vector<loaded_data_section>& datafiles,
    const std::vector<loaded_record_section>& recordfiles,
    const std::vector<loaded_block_section>& blockfiles)
{
  std::vector<std::byte> bytes;
  static constexpr std::size_t section_alignment = 16;

  config::file_header file_header;

  bytes.resize(sizeof(file_header));  // reserve space
  bytes.resize(bytes.size() + alignment_diff(bytes.size(), section_alignment));

  std::vector<std::byte> general_info_bytes = serialize_general_info(info);

  bytes.insert(bytes.end(), general_info_bytes.begin(),
               general_info_bytes.end());

  for (auto& blockfile : blockfiles) {
    bytes.resize(bytes.size()
                 + alignment_diff(bytes.size(), section_alignment));
    std::vector<std::byte> blockfile_bytes = serialize_block_file(blockfile);

    bytes.insert(bytes.end(), blockfile_bytes.begin(), blockfile_bytes.end());
  }

  for (auto& recordfile : recordfiles) {
    bytes.resize(bytes.size()
                 + alignment_diff(bytes.size(), section_alignment));
    std::vector<std::byte> recordfile_bytes = serialize_record_file(recordfile);

    bytes.insert(bytes.end(), recordfile_bytes.begin(), recordfile_bytes.end());
  }

  for (auto& datafile : datafiles) {
    bytes.resize(bytes.size()
                 + alignment_diff(bytes.size(), section_alignment));
    std::vector<std::byte> datafile_bytes = serialize_data_file(datafile);

    bytes.insert(bytes.end(), datafile_bytes.begin(), datafile_bytes.end());
  }

  file_header.file_size = bytes.size();
  file_header.section_alignment = section_alignment;
  file_header.version = 0;
  file_header.identifier = config::file_header::correct_identifier;
  file_header.file_checksum
      = CalculateCheckSum(&bytes.front() + sizeof(file_header), &*bytes.end());
  file_header.checksum = CalculateCheckSum(
      reinterpret_cast<const std::byte*>(&file_header),
      reinterpret_cast<const std::byte*>(&file_header.checksum));

  std::copy_n(reinterpret_cast<std::byte*>(&file_header), sizeof(file_header),
              bytes.begin());

  return bytes;
}

std::optional<const config::file_header*> try_read_file_header(iter& current,
                                                               iter end)
{
  if (static_cast<std::size_t>(end - current) < sizeof(config::file_header)) {
    return std::nullopt;
  }

  // this is ub; hopefully c++ somedays will get support for memory mapping/etc.
  auto* file_header = reinterpret_cast<const config::file_header*>(&*current);

  if (file_header->identifier != config::file_header::correct_identifier) {
    // error: bad file id
    return std::nullopt;
  }

  if (file_header->checksum
      != CalculateCheckSum(
          reinterpret_cast<const std::byte*>(file_header),
          reinterpret_cast<const std::byte*>(&file_header->checksum))) {
    // error: file header checksum does not match
    return std::nullopt;
  }

  current += sizeof(config::file_header);

  if (file_header->file_checksum != CalculateCheckSum(&*current, &*end)) {
    // error: file checksum does not match
    return std::nullopt;
  }

  return file_header;
}

std::optional<const config::section_header*> try_read_section_header(
    iter& current,
    iter end)
{
  if (static_cast<std::size_t>(end - current)
      < sizeof(config::section_header)) {
    return std::nullopt;
  }

  // this is ub; hopefully c++ somedays will get support for memory mapping/etc.
  auto* section_header
      = reinterpret_cast<const config::section_header*>(&*current);

  if (section_header->version != 0) {
    // error: bad file id
    return std::nullopt;
  }

  current += sizeof(config::section_header);

  if (section_header->checksum
      != CalculateCheckSum(
          reinterpret_cast<const std::byte*>(section_header),
          reinterpret_cast<const std::byte*>(&section_header->checksum))) {
    // error: header checksum does not match
    return std::nullopt;
  }

  return section_header;
}

std::optional<loaded_config> from_bytes(const std::vector<std::byte>& bytes)
{
  auto begin = bytes.begin();
  auto current = begin;
  auto end = bytes.end();

  if (std::optional file_header_opt = try_read_file_header(current, end)) {
    auto* file_header = file_header_opt.value();
    loaded_config config;
    if (file_header->version != 0) {
      // error: unrecognized version
      return std::nullopt;
    }

    if (file_header->file_size != static_cast<std::size_t>(end - begin)) {
      // error: file size missmatch
      return std::nullopt;
    }

    config.version = file_header->version;
    config.file_size = file_header->file_size;
    config.section_alignment = file_header->section_alignment;

    bool encountered_general_info = false;

    while (current < end) {
      make_aligned(current, begin, file_header->section_alignment);
      std::optional section_header_opt = try_read_section_header(current, end);

      if (!section_header_opt) { return std::nullopt; }

      auto* section_header = section_header_opt.value();

      if (end - current < section_header->padding + section_header->data_size) {
        // error: eof reached
        return std::nullopt;
      }

      current += section_header->padding;

      // todo: compute checksum
      if (section_header->data_checksum
          != CalculateCheckSum(&*current,
                               &*(current + section_header->data_size))) {
        // error: bad checksum
        return std::nullopt;
      }

      if (section_header->written_order
          != static_cast<std::uint8_t>(byte_order::big_endian)) {
        // error: bad byte order
        // FIXME: in the future we could support little endian as well
        return std::nullopt;
      }

      switch (config::section_header::types{section_header->type.as_native()}) {
        case config::section_header::types::GeneralInfo: {
          if (encountered_general_info) {
            // error: two general info
            return std::nullopt;
          }

          if (section_header->data_size < sizeof(config::general_section)) {
            // error: bad section header
            return std::nullopt;
          }

          encountered_general_info = true;

          auto* general_section
              = reinterpret_cast<const config::general_section*>(&*current);
          current += sizeof(*general_section);

          config.info.block_header_size = general_section->block_header_size;
          config.info.record_header_size = general_section->record_header_size;
          config.info.dedup_block_header_size
              = general_section->dedup_block_header_size;
          config.info.dedup_record_header_size
              = general_section->dedup_record_header_size;
        } break;
        case config::section_header::types::BlockFile: {
          if (section_header->data_size < sizeof(config::block_file_section)) {
            // error: bad section header
            return std::nullopt;
          }

          auto* block
              = reinterpret_cast<const config::block_file_section*>(&*current);
          current += sizeof(*block);

          if (end - current < block->path_length) {
            // error: reached eof
            return std::nullopt;
          }

          loaded_block_section block_file;
          block_file.file_index = block->file_index;
          block_file.start_block = block->start_block;
          block_file.end_block = block->end_block;
          block_file.path
              = deserialize_string(current, current + block->path_length);

          current += block->path_length;
          config.blockfiles.emplace_back(std::move(block_file));
        } break;
        case config::section_header::types::RecordFile: {
          if (section_header->data_size < sizeof(config::record_file_section)) {
            // error: bad section header
            return std::nullopt;
          }

          auto* record
              = reinterpret_cast<const config::record_file_section*>(&*current);
          current += sizeof(*record);

          if (end - current < record->path_length) {
            // error: reached eof
            return std::nullopt;
          }

          loaded_record_section record_file;
          record_file.file_index = record->file_index;
          record_file.start_record = record->start_record;
          record_file.end_record = record->end_record;
          record_file.path
              = deserialize_string(current, current + record->path_length);

          current += record->path_length;
          config.recordfiles.emplace_back(std::move(record_file));
        } break;
        case config::section_header::types::DataFile: {
          if (section_header->data_size < sizeof(config::data_file_section)) {
            // error: bad section header
            return std::nullopt;
          }

          auto* data
              = reinterpret_cast<const config::data_file_section*>(&*current);
          current += sizeof(*data);

          if (end - current < data->path_length) {
            // error: reached eof
            return std::nullopt;
          }

          loaded_data_section data_file;
          data_file.file_index = data->file_index;
          data_file.block_size = data->block_size;
          data_file.path
              = deserialize_string(current, current + data->path_length);

          current += data->path_length;
          config.datafiles.emplace_back(std::move(data_file));
        } break;
        default: {
          // error: unrecognized section header
          return std::nullopt;
        }
      }
    }

    if (!encountered_general_info) {
      // error: no general info
      return std::nullopt;
    }

    return config;
  } else {
    // error: file to small
    return std::nullopt;
  }
}

} /* namespace dedup::config */
