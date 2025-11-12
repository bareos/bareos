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
#include "gtest/gtest.h"

#include "parser.h"
#include "file_format.h"

#include <span>
#include <stack>

struct vec_writer : public writer {
  vec_writer(std::vector<char>* buffer) : buf{buffer} {}

  void write(const char* input, std::size_t size) override
  {
    buf->insert(std::end(*buf), input, input + size);
  }

 private:
  std::vector<char>* buf;
};

struct span_reader : public reader {
  span_reader(std::span<char>* buffer) : buf{buffer} {}

  void read(char* output, std::size_t size) override
  {
    memcpy(output, buf->data(), size);
    *buf = buf->subspan(size);
  }

 private:
  std::span<char>* buf;
};

template <typename T> void write_into(std::vector<char>& vec, const T& target)
{
  vec_writer w{&vec};
  target.write(w);
}

template <>
void write_into<std::vector<char>>(std::vector<char>& vec,
                                   const std::vector<char>& target)
{
  vec.insert(vec.end(), target.begin(), target.end());
}

template <typename T> T read_from(std::span<char>* span)
{
  T val;
  span_reader reader{span};
  val.read(reader);
  return val;
}

template <typename EntryType>
struct EntryTypeTestSuite : public ::testing::Test {};

using EntryTypes = ::testing::Types<file_header,
                                    disk_header,
                                    part_table_header,
                                    part_table_entry,
                                    part_table_entry_gpt_data,
                                    part_table_entry_mbr_data,
                                    extent_header>;

TYPED_TEST_SUITE(EntryTypeTestSuite, EntryTypes);

template <typename EntryType>
struct MagicEntryTypeTestSuite : public ::testing::Test {};

using EntryTypesWithMagic = ::testing::Types<file_header,
                                             disk_header,
                                             part_table_header,
                                             part_table_entry,
                                             extent_header>;

TYPED_TEST_SUITE(MagicEntryTypeTestSuite, EntryTypesWithMagic);


#if !defined(HAVE_WIN32)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wsuggest-override"
#endif
TYPED_TEST(MagicEntryTypeTestSuite, MagicMismatch)
{
  TypeParam orig{};

  std::vector<char> data;
  write_into(data, orig);

  // modify the magic number
  memset(data.data(), 0, sizeof(TypeParam::magic_value));

  std::span to_be_read{data};

  ASSERT_THROW(read_from<TypeParam>(&to_be_read), std::runtime_error);
}
#if !defined(HAVE_WIN32)
#  pragma GCC diagnostic pop
#endif

struct repeat {
  explicit repeat(char byte) : c{byte} {}

  operator bool() const
  {
    // needed because asan/ubsan are too smart
    return c % 2 == 0;
  }

  template <typename T> operator T() const
  {
    static_assert(std::is_trivial_v<T>);

    T val;
    memset(&val, c, sizeof(T));
    return val;
  }

  char c;
};

void insert_test_value(file_header& val) { val = {repeat{0x01}, repeat{0x02}}; }
void insert_test_value(disk_header& val)
{
  val = {repeat{0x01}, repeat{0x02}, repeat{0x03}, repeat{0x04}, repeat{0x05}};
}
void insert_test_value(part_table_header& val)
{
  val = {repeat{0x01}, repeat{0x02}, repeat{0x03}, repeat{0x04}, repeat{0x05}};
}
void insert_test_value(part_table_entry& val)
{
  val = {repeat{0x01}, repeat{0x02}, repeat{0x03},
         repeat{0x04}, repeat{0x05}, repeat{0x06}};
}
void insert_test_value(part_table_entry_gpt_data& val)
{
  val = {repeat{0x01}, repeat{0x02}, repeat{0x03}, repeat{0x04}};
}
void insert_test_value(part_table_entry_mbr_data& val)
{
  val = {repeat{0x01}, repeat{0x02}, repeat{0x03}, repeat{0x04}, repeat{0x05}};
}
void insert_test_value(extent_header& val)
{
  val = {repeat{0x01}, repeat{0x02}};
}

#if !defined(HAVE_WIN32)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wsuggest-override"
#endif
TYPED_TEST(EntryTypeTestSuite, PingPong)
{
  TypeParam orig;
  insert_test_value(orig);

  std::vector<char> data;
  write_into(data, orig);

  EXPECT_GT(data.size(), 0);

  std::span to_be_read{data};

  auto copy = read_from<TypeParam>(&to_be_read);

  EXPECT_EQ(to_be_read.size(), 0) << "not all data was consumed";

  EXPECT_EQ(orig, copy);
}
#if !defined(HAVE_WIN32)
#  pragma GCC diagnostic pop
#endif

template <typename... T> void ignore(T...) {}

using Element = std::variant<file_header,
                             disk_header,
                             part_table_header,
                             part_table_entry,
                             part_table_entry_gpt_data,
                             part_table_entry_mbr_data,
                             extent_header,
                             std::vector<char>>;

struct TestHandler
    : public GenericHandler
    , public GenericLogger {
  std::vector<Element> parsed_elements;
  std::stack<std::size_t> open_elements;

  TestHandler() : GenericLogger{true} {}

  template <typename ElementType> std::size_t push(ElementType el)
  {
    parsed_elements.emplace_back(std::move(el));

    auto idx = parsed_elements.size() - 1;
    open_elements.push(idx);

    return idx;
  }

  template <typename ElementType> ElementType* try_peek()
  {
    if (open_elements.empty()) {
      throw std::runtime_error("cannot pop from empty stack");
    }

    return std::get_if<ElementType>(&parsed_elements[open_elements.top()]);
  }

  template <typename ElementType> ElementType* peek()
  {
    if (auto* element = try_peek<ElementType>()) {
      return element;
    } else {
      throw std::runtime_error{"unexpected element type on top"};
    }
  }

  template <typename ElementType> void pop()
  {
    // make sure the top element has the right type
    ignore(peek<ElementType>());

    open_elements.pop();
  }

  void BeginRestore(std::size_t num_disks) override
  {
    switch (open_elements.size()) {
      case 0: {
        push(file_header{static_cast<uint32_t>(num_disks), 0});
      } break;
      case 1: {
        auto* hdr = peek<file_header>();
        hdr->disk_count = num_disks;
      } break;
      default: {
        throw std::runtime_error{"Begin: bad state"};
      } break;
    }
  }
  void EndRestore() override
  {
    if (open_elements.size() != 1) {
      throw std::runtime_error{"EndRestore: bad state"};
    }

    pop<file_header>();
  }
  void BeginDisk(disk_info info) override
  {
    push(disk_header{info.disk_size, info.total_extent_size, 0, 0,
                     info.extent_count});
  }
  void EndDisk() override { pop<disk_header>(); }

  void BeginMbrTable(const partition_info_mbr& mbr) override
  {
    push(part_table_header{
        0,
        part_type::Mbr,
        mbr.CheckSum,
        mbr.Signature,
        0,
        {},
        mbr.bootstrap,
    });
  }
  void BeginGptTable(const partition_info_gpt& gpt) override
  {
    push(part_table_header{
        0,
        part_type::Gpt,
        gpt.MaxPartitionCount,
        gpt.StartingUsableOffset,
        gpt.UsableLength,
        gpt.DiskId.Data,
        gpt.bootstrap,
    });
  }
  void BeginRawTable(const partition_info_raw& raw) override { ignore(raw); }
  void MbrEntry(const part_table_entry& entry,
                const part_table_entry_mbr_data& data) override
  {
    auto* table = peek<part_table_header>();
    table->partition_count += 1;
    ignore(entry, data);
  }
  void GptEntry(const part_table_entry& entry,
                const part_table_entry_gpt_data& data) override
  {
    auto* table = peek<part_table_header>();
    table->partition_count += 1;
    ignore(entry, data);
  }
  void EndPartTable() override { pop<part_table_header>(); }

  void BeginExtent(extent_header header) override { push(header); }
  void ExtentData(std::span<const char> data) override
  {
    auto* extent_data = try_peek<std::vector<char>>();
    if (extent_data) {
      extent_data->insert(extent_data->end(), data.begin(), data.end());
    } else {
      push(std::vector<char>(data.begin(), data.end()));
    }
  }
  void EndExtent() override
  {
    // if we read data, we need to pop it
    if (try_peek<std::vector<char>>()) { pop<std::vector<char>>(); }
    pop<extent_header>();
  }

  void Begin(std::size_t FileSize) override
  {
    switch (open_elements.size()) {
      case 0: {
        push(file_header{0, FileSize});
      } break;
      case 1: {
        auto* hdr = peek<file_header>();
        hdr->file_size = FileSize;
      } break;
      default: {
        throw std::runtime_error{"Begin: bad state"};
      } break;
    }
  }
  void Progressed(std::size_t Amount) override { ignore(Amount); }
  void End() override {}

  void SetStatus(std::string_view Status) override { ignore(Status); }
  void Output(Message message) override { ignore(message); }
};

std::vector<Element> GenerateTestData()
{
  std::vector<Element> elements;

  {
    // disk 1

    elements.emplace_back(disk_header{1234512, 0, 0, 0, 5});
    elements.emplace_back(part_table_header{0, part_type::Mbr, 0, 0, 0});
    elements.emplace_back(extent_header{10, 0});
    elements.emplace_back(extent_header{20, 0});
    elements.emplace_back(extent_header{30, 0});
    elements.emplace_back(extent_header{40, 0});
    elements.emplace_back(extent_header{50, 0});
  }

  {
    // disk 2

    elements.emplace_back(disk_header{1234512, 0, 0, 0, 3});
    elements.emplace_back(part_table_header{0, part_type::Mbr, 0, 0, 0});
    elements.emplace_back(extent_header{1000, 50});
    elements.emplace_back(std::vector<char>(50));
    elements.emplace_back(extent_header{2000, 150});
    elements.emplace_back(std::vector<char>(150));
    elements.emplace_back(extent_header{3000, 250});
    elements.emplace_back(std::vector<char>(250));
  }
  return elements;
}

std::vector<char> WriteTestData(std::vector<Element>& elements,
                                std::vector<std::size_t>& sizes)
{
  // at this points elements is still lacking the file header

  sizes.clear();
  sizes.reserve(elements.size() + 1);


  std::vector<char> data;
  std::uint32_t disk_count = 0;
  for (auto& elem : elements) {
    auto current = data.size();
    std::visit(
        [&](auto&& x) {
          if constexpr (std::is_same_v<std::remove_reference_t<decltype(x)>,
                                       disk_header>) {
            disk_count += 1;
          }
          write_into(data, x);
        },
        elem);
    sizes.push_back(data.size() - current);
  }

  file_header hdr{disk_count, data.size()};
  std::vector<char> header_data;
  write_into(header_data, hdr);
  elements.emplace(elements.begin(), hdr);
  data.insert(data.begin(), header_data.begin(), header_data.end());
  sizes.emplace(sizes.begin(), header_data.size());


  return data;
}

TEST(parser, Parse_PerfectSize)
{
  auto test_data = GenerateTestData();
  std::vector<std::size_t> test_data_sizes;
  auto bytes = WriteTestData(test_data, test_data_sizes);

  TestHandler handler;
  auto* parser = parse_begin(&handler, &handler);

  std::size_t current = 0;
  for (size_t i = 0; i < test_data.size(); ++i) {
    std::span data{bytes.begin() + (current),
                   bytes.begin() + (current + test_data_sizes[i])};
    parse_data(parser, data);
    current += test_data_sizes[i];

    EXPECT_EQ(handler.parsed_elements.size(), i + 1);
  }
  ASSERT_EQ(current, bytes.size());
  EXPECT_TRUE(handler.open_elements.empty());

  parse_end(parser);

  EXPECT_EQ(test_data, handler.parsed_elements);
}

TEST(parser, Parse_Halfs)
{
  auto test_data = GenerateTestData();
  std::vector<std::size_t> test_data_sizes;
  auto bytes = WriteTestData(test_data, test_data_sizes);

  TestHandler handler;
  auto* parser = parse_begin(&handler, &handler);

  std::size_t current = 0;
  for (size_t i = 0; i < test_data.size(); ++i) {
    auto middle = current + test_data_sizes[i] / 2;
    std::span data1{bytes.begin() + (current), bytes.begin() + (middle)};
    parse_data(parser, data1);
    // nothing parsed yet
    if (std::get_if<std::vector<char>>(&test_data[i]) == nullptr) {
      // if we are parsing a vector then it is expected for there
      // to be partial successes, so we dont check this in that case
      EXPECT_EQ(handler.parsed_elements.size(), i);
    }
    std::span data2{bytes.begin() + (middle),
                    bytes.begin() + (current + test_data_sizes[i])};
    parse_data(parser, data2);
    current += test_data_sizes[i];

    EXPECT_EQ(handler.parsed_elements.size(), i + 1);
  }
  ASSERT_EQ(current, bytes.size());
  EXPECT_TRUE(handler.open_elements.empty());

  parse_end(parser);

  EXPECT_EQ(test_data, handler.parsed_elements);
}

TEST(parser, Parse_100Bytes)
{
  auto test_data = GenerateTestData();
  std::vector<std::size_t> test_data_sizes;
  auto bytes = WriteTestData(test_data, test_data_sizes);
  ignore(test_data_sizes);

  TestHandler handler;
  auto* parser = parse_begin(&handler, &handler);

  for (size_t i = 0; i < bytes.size(); i += 100) {
    auto end = [&]() -> std::size_t {
      if (bytes.size() < i + 100) { return bytes.size(); }
      return i + 100;
    }();
    std::span data{bytes.begin() + (i), bytes.begin() + (end)};
    parse_data(parser, data);
  }
  EXPECT_TRUE(handler.open_elements.empty());

  parse_end(parser);

  EXPECT_EQ(test_data, handler.parsed_elements);
}

TEST(parser, Parse_AllAtOnce)
{
  auto test_data = GenerateTestData();
  std::vector<std::size_t> test_data_sizes;
  auto bytes = WriteTestData(test_data, test_data_sizes);
  ignore(test_data_sizes);

  TestHandler handler;
  auto* parser = parse_begin(&handler, &handler);

  parse_data(parser, bytes);
  EXPECT_TRUE(handler.open_elements.empty());

  parse_end(parser);

  EXPECT_EQ(test_data, handler.parsed_elements);
}
