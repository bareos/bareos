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

template <typename T> void write_into(std::vector<char>& vec, T target)
{
  vec_writer w{&vec};
  target.write(w);
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

struct repeat {
  explicit repeat(char byte) : c{byte} {}

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

template <typename... T> void ignore(T...) {}

struct TestHandler : GenericHandler {
  void BeginRestore(std::size_t num_disks) override { ignore(num_disks); }
  void EndRestore() override {}
  void BeginDisk(disk_info info) override { ignore(info); }
  void EndDisk() override {}

  void BeginMbrTable(const partition_info_mbr& mbr) override { ignore(mbr); }
  void BeginGptTable(const partition_info_gpt& gpt) override { ignore(gpt); }
  void BeginRawTable(const partition_info_raw& raw) override { ignore(raw); }
  void MbrEntry(const part_table_entry& entry,
                const part_table_entry_mbr_data& data) override
  {
    ignore(entry, data);
  }
  void GptEntry(const part_table_entry& entry,
                const part_table_entry_gpt_data& data) override
  {
    ignore(entry, data);
  }
  void EndPartTable() override {}

  void BeginExtent(extent_header header) override { ignore(header); }
  void ExtentData(std::span<const char> data) override { ignore(data); }
  void EndExtent() override {}
};

struct TestLogger : GenericLogger {
  TestLogger() : GenericLogger(true) {}

  void Begin(std::size_t FileSize) override { ignore(FileSize); }
  void Progressed(std::size_t Amount) override { ignore(Amount); }
  void End() override {}

  void SetStatus(std::string_view Status) override { ignore(Status); }
  void Output(Message message) override { ignore(message); }
};

TEST(parser, Parse)
{
  TestHandler handler;
  TestLogger logger;
  auto* parser = parse_begin(&handler, &logger);

  std::span<char> data;
  parse_data(parser, data);
  parse_end(parser);
}
