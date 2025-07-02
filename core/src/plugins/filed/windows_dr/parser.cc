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
#include <bit>
#include <stack>
#include <fmt/format.h>

struct need_data : std::exception {
  std::size_t _available;
  std::size_t _needed;
  need_data(std::size_t needed, std::size_t available)
      : _available{available}, _needed{needed}
  {
  }
};

struct data_to_read : public reader {
  data_to_read(std::span<const char> data1_, std::span<const char> data2_)
      : data1{data1_}, data2{data2_}
  {
    if (data1.empty()) {
      data1 = data2;
      data2 = {};
    }
  }

  void read(char* buffer, std::size_t size) override
  {
    if (data1.size() + data2.size() < size) {
      throw need_data{size, data1.size() + data2.size()};
    }

    std::size_t from_first = std::min(data1.size(), size);
    std::memcpy(buffer, data1.data(), from_first);

    std::size_t from_second = size - from_first;
    if (from_second) {
      std::memcpy(buffer, data2.data(), from_second);

      // at this point we know that the first buffer is empty, so "swap" them

      data1 = data2.subspan(from_second);
      data2 = {};
    } else {
      data1 = data1.subspan(from_first);
      if (data1.empty()) {
        data1 = data2;
        data2 = {};
      }
    }
  }

  std::span<const char> take_contiguous(std::size_t max_size)
  {
    std::size_t from_first = std::min(data1.size(), max_size);

    if (from_first == data1.size()) {
      auto result = data1;
      data1 = data2;
      data2 = {};
      return result;
    } else {
      auto result = data1.subspan(0, from_first);
      data1 = data1.subspan(from_first);
      return result;
    }
  }

  virtual ~data_to_read() = default;

  std::size_t size() const { return data1.size() + data2.size(); }

  // invariant: if data1.empty(), then data2.empty()
  std::span<const char> data1;
  std::span<const char> data2;
};

struct restartable_parser {
  restartable_parser(GenericHandler* handler) : _handler{handler}
  {
    to_parse.push(init{});
  }

  disk_info ReadDiskHeader(reader& stream)
  {
    disk_header header;
    header.read(stream);

    std::uint64_t disk_size = header.disk_size;

    return {disk_size, header.extent_count, header.total_extent_size};
  }

  file_header ReadFileHeader(reader& stream)
  {
    file_header header;
    header.read(stream);

    if (header.version != file_header::current_version) {
      throw std::runtime_error{
          fmt::format("expected dump version {}, got version {}",
                      file_header::current_version, header.version),
      };
    }

    return header;
  }

  // void ingest_with_leftover(r) {}
  void ingest(std::span<const char> data)
  {
    if (data.size() == 0) { return; }  // nothing to do
    if (to_parse.empty()) {
      throw std::logic_error{"extra data at end of file.  Cannot continue."};
    }

    data_to_read d{left_over, data};

    while (!to_parse.empty()) {
      auto& current = to_parse.top();
      auto snapshot = d;

      try {
        std::visit(
            [this, &d](auto&& x) {
              next_storage.clear();
              parse_element(x, d, next_storage);
              to_parse.pop();
              for (size_t i = 0; i < next_storage.size(); ++i) {
                to_parse.emplace(
                    std::move(next_storage[next_storage.size() - 1 - i]));
              }
            },
            current);
      } catch (const need_data&) {
        d = snapshot;
        break;
      }
    }

    if (to_parse.empty() && d.size() > 0) {
      throw std::runtime_error{"parsing done, but still data left"};
    }
    save_rest(d);
  }

  void save_rest(data_to_read& d)
  {
    if (d.data2.empty()) {
      // if data2 is empty, then data1 (left_over) was completely cleared,
      // so we dont have to be careful now

      left_over.assign(d.data1.begin(), d.data1.end());
    } else {
      // the previous leftovers were not cleared completely
      // we have to be careful now sice d.data1 and leftover are actually
      // the same thing (or at least views into the same thing)
      assert(left_over.size() >= d.data1.size());
      auto parsed_bytes = left_over.size() - d.data1.size();

      if (parsed_bytes) {
        // we need to first remove the used up part
        left_over.erase(left_over.begin(), left_over.begin() + parsed_bytes);
      }

      left_over.insert(left_over.end(), d.data2.begin(), d.data2.end());
    }
  }

  void set_logger(GenericLogger* logger) { _logger = logger; }

  // ~restartable_parser() { }
  bool done() const { return to_parse.empty(); }

  GenericLogger* _logger{nullptr};
  GenericHandler* _handler{nullptr};
  std::vector<char> left_over;

  struct init {};
  struct end {};
  struct file {};
  struct disk {
    std::size_t index;
    std::size_t count;
  };
  struct disk_end {
    std::size_t index;
  };
  struct partition_table {};
  struct partition_table_entry {};
  struct partition_table_end {};
  struct extent {
    std::size_t index;
    std::size_t count;
  };
  struct extent_data {
    std::size_t togo;
  };
  struct extent_end {
    std::size_t index;
    std::size_t count;
  };

  using parsable = std::variant<init,
                                end,
                                file,
                                disk,
                                disk_end,
                                partition_table,
                                partition_table_entry,
                                partition_table_end,
                                extent,
                                extent_data,
                                extent_end>;


  void parse_element(init, reader&, std::vector<parsable>& next)
  {
    next.emplace_back(file{});
    next.emplace_back(end{});
  }
  void parse_element(end, reader&, std::vector<parsable>&)
  {
    _handler->EndRestore();
    Info("restore completed");
    _logger->End();
  }
  void parse_element(file, reader& r, std::vector<parsable>& next)
  {
    file_header header = ReadFileHeader(r);
    _logger->Begin(header.file_size);
    Info("Restoring {} disks", header.disk_count);
    _handler->BeginRestore(header.disk_count);
    for (std::size_t i = 0; i < header.disk_count; ++i) {
      next.push_back(disk{i, header.disk_count});
    }
  }
  void parse_element(disk d, reader& r, std::vector<parsable>& next)
  {
    disk_info header = ReadDiskHeader(r);

    _logger->SetStatus(
        fmt::format("restoring disk {}/{}", d.index + 1, d.count));

    Info("Restoring disk {} of size {}", d.index + 1, header.disk_size);
    next.push_back(partition_table{});
    for (std::size_t i = 0; i < header.extent_count; ++i) {
      next.push_back(extent{i, header.extent_count});
    }
    next.push_back(disk_end{d.index});
  }
  void parse_element(disk_end e, reader&, std::vector<parsable>&)
  {
    _handler->EndDisk();
    Info("disk {} finished", e.index + 1);
  }
  void parse_element(partition_table, reader& r, std::vector<parsable>& next)
  {
    part_table_header header;
    header.read(r);

    switch (header.part_table_type) {
      case part_type::Raw: {
        _handler->BeginRawTable(partition_info_raw{});
      } break;
      case part_type::Mbr: {
        partition_info_mbr mbr{.CheckSum = header.Datum0,
                               .Signature = (uint32_t)header.Datum1,
                               .bootstrap = {}};
        memcpy(mbr.bootstrap, header.Data2, sizeof(header.Data2));
        _handler->BeginMbrTable(mbr);
      } break;
      case part_type::Gpt: {
        partition_info_gpt gpt{.DiskId = std::bit_cast<guid>(header.Data),
                               .StartingUsableOffset = header.Datum1,
                               .UsableLength = header.Datum2,
                               .MaxPartitionCount = header.Datum0,
                               .bootstrap = {}};
        memcpy(gpt.bootstrap, header.Data2, sizeof(header.Data2));
        _handler->BeginGptTable(gpt);
      } break;
    }

    for (std::size_t i = 0; i < header.partition_count; ++i) {
      next.push_back(partition_table_entry{});
    }
    next.push_back(partition_table_end{});
  }
  void parse_element(partition_table_entry, reader& r, std::vector<parsable>&)
  {
    part_table_entry entry;
    entry.read(r);

    switch (entry.partition_style) {
      case Mbr: {
        part_table_entry_mbr_data data;
        data.read(r);

        _handler->MbrEntry(entry, data);
      } break;
      case Gpt: {
        part_table_entry_gpt_data data;
        data.read(r);

        _handler->GptEntry(entry, data);
      } break;
      default: {
        throw std::logic_error{
            fmt::format("unknown partition type ({}) encountered",
                        static_cast<std::uint8_t>(entry.partition_style))};
      }
    }
  }
  void parse_element(partition_table_end, reader&, std::vector<parsable>&)
  {
    _handler->EndPartTable();
  }
  void parse_element(extent e, reader& r, std::vector<parsable>& next)
  {
    Info("Restoring extent {}/{}", e.index + 1, e.count);
    extent_header header;
    header.read(r);

    _handler->BeginExtent(header);

    next.push_back(extent_data{header.length});
    next.push_back(extent_end{e.index, e.count});
  }
  void parse_element(extent_data d,
                     data_to_read& r,
                     std::vector<parsable>& next)
  {
    std::size_t bytes_read = 0;
    while (bytes_read < d.togo) {
      auto span = r.take_contiguous(d.togo - bytes_read);
      if (span.size() == 0) { break; }

      _handler->ExtentData(span);
      bytes_read += span.size();
    }
    if (bytes_read == 0) { throw need_data{d.togo, 0}; }

    if (bytes_read < d.togo) {
      next.push_back(extent_data{d.togo - bytes_read});
    }
  }
  void parse_element(extent_end, reader&, std::vector<parsable>&)
  {
    _handler->EndExtent();
  }

  std::stack<parsable, std::vector<parsable>> to_parse{};
  std::vector<parsable> next_storage;

  template <typename... Args>
  void Info(fmt::format_string<Args...> fmt, Args&&... args)
  {
    _logger->Info(fmt::format(fmt, std::forward<Args>(args)...));
  }
};

restartable_parser* parse_begin(GenericHandler* handler)
{
  return new restartable_parser{handler};
}

void parse_data(restartable_parser* parser, std::span<const char> data)
{
  parser->ingest(data);
}

void parse_end(restartable_parser* parser)
{
  parse_data(parser, {});

  bool done = parser->done();
  delete parser;

  if (!done) {
    throw std::runtime_error{
        "stream reached eof, but parsing was not done yet"};
  }
}


void parse_file_format(GenericLogger* logger,
                       std::istream& stream,
                       GenericHandler* strategy)
{
  std::vector<char> buffer;
  buffer.resize(4 << 20);

  auto parser = parse_begin(strategy);

  parser->set_logger(logger);

  while (!stream.eof()) {
    stream.read(buffer.data(), buffer.size());
    auto count = stream.gcount();

    assert(count >= 0);

    parse_data(parser,
               std::span{buffer.data(), static_cast<std::size_t>(count)});

    logger->Progressed(count);
  }

  parse_end(parser);
}
