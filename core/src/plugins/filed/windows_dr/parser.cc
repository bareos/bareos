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

#include "format.h"

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

  template <typename Parsable> struct generic_context {
    void BeginRestore(std::size_t num_disks)
    {
      _handler->BeginRestore(num_disks);
    }
    void EndRestore() { _handler->EndRestore(); }
    void BeginDisk(disk_info info) { _handler->BeginDisk(info); }
    void EndDisk() { _handler->EndDisk(); }

    void BeginMbrTable(const partition_info_mbr& mbr)
    {
      _handler->BeginMbrTable(mbr);
    }
    void BeginGptTable(const partition_info_gpt& gpt)
    {
      _handler->BeginGptTable(gpt);
    }
    void BeginRawTable(const partition_info_raw& raw)
    {
      _handler->BeginRawTable(raw);
    }
    void MbrEntry(const part_table_entry& entry,
                  const part_table_entry_mbr_data& data)
    {
      _handler->MbrEntry(entry, data);
    }
    void GptEntry(const part_table_entry& entry,
                  const part_table_entry_gpt_data& data)
    {
      _handler->GptEntry(entry, data);
    }
    void EndPartTable() { _handler->EndPartTable(); }

    void BeginExtent(extent_header header) { _handler->BeginExtent(header); }
    void ExtentData(std::span<const char> data) { _handler->ExtentData(data); }
    void EndExtent() { _handler->EndExtent(); }

    void Begin(std::size_t FileSize) { _logger->Begin(FileSize); }
    void Progressed(std::size_t Amount) { _logger->Progressed(Amount); }
    void End() { _logger->End(); }

    void SetStatus(std::string_view Status) { _logger->SetStatus(Status); }

    void Info(std::string_view Message) { _logger->Info(Message); }

    template <typename... Args>
    void Info(libbareos::format_string<Args...> fmt, Args&&... args)
    {
      _logger->Info(libbareos::format(fmt, std::forward<Args>(args)...));
    }

    data_to_read& stream() { return *_data; }

    template <typename T, typename... Args> void enqueue(Args&&... args)
    {
      next->push_back(T{std::forward<Args>(args)...});
    }

    GenericLogger* _logger;
    GenericHandler* _handler;
    data_to_read* _data;
    std::vector<Parsable>* next;
  };

  static disk_info ReadDiskHeader(reader& stream)
  {
    disk_header header;
    header.read(stream);

    std::uint64_t disk_size = header.disk_size;

    return {disk_size, header.extent_count, header.total_extent_size};
  }

  static file_header ReadFileHeader(reader& stream)
  {
    file_header header;
    header.read(stream);

    if (header.version != file_header::current_version) {
      throw std::runtime_error{
          libbareos::format("expected dump version {}, got version {}",
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

      context ctx{_logger, _handler, &d, &next_storage};

      try {
        std::visit(
            [this, &d, &ctx](auto&& x) {
              next_storage.clear();
              x.parse(ctx);
            },
            current);
      } catch (const need_data&) {
        d = snapshot;
        break;
      }

      to_parse.pop();
      for (size_t i = 0; i < next_storage.size(); ++i) {
        to_parse.emplace(std::move(next_storage[next_storage.size() - 1 - i]));
      }
    }

    if (to_parse.empty() && d.size() > 0) {
      throw std::runtime_error{"parsing done, but still data left"};
    }

    // we need to save the left over data, so that the next time we are called
    // we can continue where we left off
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

  struct end;
  struct file;
  struct init;
  struct disk;
  struct disk_end;
  struct partition_table;
  struct partition_table_entry;
  struct partition_table_end;
  struct extent;
  struct extent_data;
  struct extent_end;

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

  using context = generic_context<parsable>;

  struct end {
    end() = default;
    void parse(context& ctx)
    {
      ctx.EndRestore();
      ctx.Info("restore completed");
      ctx.End();
    }
  };
  struct file {
    file() = default;
    void parse(context& ctx)
    {
      file_header header = ReadFileHeader(ctx.stream());
      ctx.Begin(header.file_size);
      ctx.Info("Restoring {} disks", header.disk_count);
      ctx.BeginRestore(header.disk_count);
      for (std::size_t i = 0; i < header.disk_count; ++i) {
        ctx.enqueue<disk>(i, header.disk_count);
      }
    }
  };
  struct init {
    init() = default;
    void parse(context& ctx)
    {
      ctx.enqueue<file>();
      ctx.enqueue<end>();
    }
  };
  struct disk {
    disk(std::size_t index, std::size_t count) : _count{count}, _index{index} {}

    void parse(context& ctx)
    {
      disk_info header = ReadDiskHeader(ctx.stream());

      ctx.SetStatus(
          libbareos::format("restoring disk {}/{}", _index + 1, _count));

      ctx.Info("Restoring disk {} of size {}", _index + 1, header.disk_size);
      ctx.BeginDisk(header);
      ctx.enqueue<partition_table>();
      for (std::size_t i = 0; i < header.extent_count; ++i) {
        ctx.enqueue<extent>(i, header.extent_count);
      }
      ctx.enqueue<disk_end>(_index);
    }

    std::size_t _count;
    std::size_t _index;
  };
  struct disk_end {
    disk_end(std::size_t index) : _index{index} {}

    void parse(context& ctx)
    {
      ctx.EndDisk();
      ctx.Info("disk {} finished", _index + 1);
    }

    std::size_t _index;
  };
  struct partition_table {
    partition_table() = default;
    void parse(context& ctx)
    {
      part_table_header header;
      header.read(ctx.stream());

      switch (header.part_table_type) {
        case part_type::Raw: {
          ctx.BeginRawTable(partition_info_raw{});
        } break;
        case part_type::Mbr: {
          partition_info_mbr mbr{.CheckSum = header.Datum0,
                                 .Signature = (uint32_t)header.Datum1,
                                 .bootstrap = {}};
          memcpy(mbr.bootstrap, header.Data2, sizeof(header.Data2));
          ctx.BeginMbrTable(mbr);
        } break;
        case part_type::Gpt: {
          partition_info_gpt gpt{.DiskId = std::bit_cast<guid>(header.Data),
                                 .StartingUsableOffset = header.Datum1,
                                 .UsableLength = header.Datum2,
                                 .MaxPartitionCount = header.Datum0,
                                 .bootstrap = {}};
          memcpy(gpt.bootstrap, header.Data2, sizeof(header.Data2));
          ctx.BeginGptTable(gpt);
        } break;
      }

      for (std::size_t i = 0; i < header.partition_count; ++i) {
        ctx.enqueue<partition_table_entry>();
      }
      ctx.enqueue<partition_table_end>();
    }
  };
  struct partition_table_entry {
    partition_table_entry() = default;

    void parse(context& ctx)
    {
      part_table_entry entry;
      entry.read(ctx.stream());

      switch (entry.partition_style) {
        case Mbr: {
          part_table_entry_mbr_data data;
          data.read(ctx.stream());

          ctx.MbrEntry(entry, data);
        } break;
        case Gpt: {
          part_table_entry_gpt_data data;
          data.read(ctx.stream());

          ctx.GptEntry(entry, data);
        } break;
        default: {
          throw std::logic_error{libbareos::format(
              "unknown partition type ({}) encountered",
              static_cast<std::uint8_t>(entry.partition_style))};
        }
      }
    }
  };
  struct partition_table_end {
    partition_table_end() = default;

    void parse(context& ctx) { ctx.EndPartTable(); }
  };
  struct extent {
    extent(std::size_t index, std::size_t count) : _index{index}, _count{count}
    {
    }

    void parse(context& ctx)
    {
      ctx.Info("Restoring extent {}/{}", _index + 1, _count);
      extent_header header;
      header.read(ctx.stream());

      ctx.BeginExtent(header);

      ctx.enqueue<extent_data>(header.length);
      ctx.enqueue<extent_end>(_index, _count);
    }

    std::size_t _index;
    std::size_t _count;
  };
  struct extent_data {
    extent_data(std::size_t togo) : _togo{togo} {}

    void parse(context& ctx)
    {
      std::size_t bytes_read = 0;
      while (bytes_read < _togo) {
        auto span = ctx.stream().take_contiguous(_togo - bytes_read);
        if (span.size() == 0) { break; }

        ctx.ExtentData(span);
        bytes_read += span.size();
      }
      if (bytes_read == 0) { throw need_data{_togo, 0}; }

      if (bytes_read < _togo) { ctx.enqueue<extent_data>(_togo - bytes_read); }
    }

    std::size_t _togo;
  };
  struct extent_end {
    extent_end(std::size_t index, std::size_t count)
        : _index{index}, _count{count}
    {
    }

    void parse(context& ctx) { ctx.EndExtent(); }

    std::size_t _index;
    std::size_t _count;
  };

  std::stack<parsable, std::vector<parsable>> to_parse{};
  std::vector<parsable> next_storage;
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
