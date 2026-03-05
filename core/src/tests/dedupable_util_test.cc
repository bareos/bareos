/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2023-2025 Bareos GmbH & Co. KG

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

//#if defined(HAVE_MINGW)
#include "include/bareos.h"
//#endif

#include "gtest/gtest.h"
#include "stored/backends/dedupable/util.h"
#include <vector>
#include <cstring>

using namespace dedup;

// Tests for chunked_writer
TEST(ChunkedWriterTest, Construction)
{
  std::vector<char> buffer(100);
  chunked_writer writer(buffer.data(), buffer.size());
  EXPECT_EQ(writer.leftover(), 100);
  EXPECT_FALSE(writer.finished());
}

TEST(ChunkedWriterTest, WriteEntireBuffer)
{
  std::vector<char> buffer(10);
  chunked_writer writer(buffer.data(), buffer.size());

  const char* data = "0123456789";
  EXPECT_TRUE(writer.write(data, 10));
  EXPECT_EQ(writer.leftover(), 0);
  EXPECT_TRUE(writer.finished());
  EXPECT_EQ(std::memcmp(buffer.data(), data, 10), 0);
}

TEST(ChunkedWriterTest, WritePartialBuffer)
{
  std::vector<char> buffer(10);
  chunked_writer writer(buffer.data(), buffer.size());

  const char* data = "12345";
  EXPECT_TRUE(writer.write(data, 5));
  EXPECT_EQ(writer.leftover(), 5);
  EXPECT_FALSE(writer.finished());
  EXPECT_EQ(std::memcmp(buffer.data(), data, 5), 0);
}

TEST(ChunkedWriterTest, MultipleWrites)
{
  std::vector<char> buffer(10);
  chunked_writer writer(buffer.data(), buffer.size());

  EXPECT_TRUE(writer.write("abc", 3));
  EXPECT_EQ(writer.leftover(), 7);

  EXPECT_TRUE(writer.write("def", 3));
  EXPECT_EQ(writer.leftover(), 4);

  EXPECT_TRUE(writer.write("ghij", 4));
  EXPECT_EQ(writer.leftover(), 0);
  EXPECT_TRUE(writer.finished());

  EXPECT_EQ(std::string(buffer.data(), 10), "abcdefghij");
}

TEST(ChunkedWriterTest, WriteExceedsBuffer)
{
  std::vector<char> buffer(5);
  chunked_writer writer(buffer.data(), buffer.size());

  const char* data = "0123456789";
  EXPECT_FALSE(writer.write(data, 10));
  EXPECT_EQ(writer.leftover(), 5);
}

TEST(ChunkedWriterTest, WriteToFullBuffer)
{
  std::vector<char> buffer(5);
  chunked_writer writer(buffer.data(), buffer.size());

  EXPECT_TRUE(writer.write("12345", 5));
  EXPECT_TRUE(writer.finished());

  // Try to write more
  EXPECT_FALSE(writer.write("6", 1));
}

TEST(ChunkedWriterTest, WriteZeroBytes)
{
  std::vector<char> buffer(10);
  chunked_writer writer(buffer.data(), buffer.size());

  EXPECT_TRUE(writer.write("test", 0));
  EXPECT_EQ(writer.leftover(), 10);
  EXPECT_FALSE(writer.finished());
}

TEST(ChunkedWriterTest, EmptyBuffer)
{
  std::vector<char> buffer(0);
  chunked_writer writer(buffer.data(), buffer.size());

  EXPECT_EQ(writer.leftover(), 0);
  EXPECT_TRUE(writer.finished());
  EXPECT_FALSE(writer.write("a", 1));
}

// Tests for chunked_reader
TEST(ChunkedReaderTest, Construction)
{
  std::vector<char> buffer(100, 'x');
  chunked_reader reader(buffer.data(), buffer.size());
  EXPECT_EQ(reader.leftover(), 100);
  EXPECT_FALSE(reader.finished());
}

TEST(ChunkedReaderTest, ReadEntireBuffer)
{
  const char* data = "0123456789";
  chunked_reader reader(data, 10);

  char output[10];
  EXPECT_TRUE(reader.read(output, 10));
  EXPECT_EQ(reader.leftover(), 0);
  EXPECT_TRUE(reader.finished());
  EXPECT_EQ(std::memcmp(output, data, 10), 0);
}

TEST(ChunkedReaderTest, ReadPartialBuffer)
{
  const char* data = "0123456789";
  chunked_reader reader(data, 10);

  char output[5];
  EXPECT_TRUE(reader.read(output, 5));
  EXPECT_EQ(reader.leftover(), 5);
  EXPECT_FALSE(reader.finished());
  EXPECT_EQ(std::memcmp(output, data, 5), 0);
}

TEST(ChunkedReaderTest, MultipleReads)
{
  const char* data = "abcdefghij";
  chunked_reader reader(data, 10);

  char buf1[3], buf2[3], buf3[4];

  EXPECT_TRUE(reader.read(buf1, 3));
  EXPECT_EQ(reader.leftover(), 7);
  EXPECT_EQ(std::string(buf1, 3), "abc");

  EXPECT_TRUE(reader.read(buf2, 3));
  EXPECT_EQ(reader.leftover(), 4);
  EXPECT_EQ(std::string(buf2, 3), "def");

  EXPECT_TRUE(reader.read(buf3, 4));
  EXPECT_EQ(reader.leftover(), 0);
  EXPECT_TRUE(reader.finished());
  EXPECT_EQ(std::string(buf3, 4), "ghij");
}

TEST(ChunkedReaderTest, ReadExceedsBuffer)
{
  const char* data = "12345";
  chunked_reader reader(data, 5);

  char output[10];
  EXPECT_FALSE(reader.read(output, 10));
  EXPECT_EQ(reader.leftover(), 5);
}

TEST(ChunkedReaderTest, ReadFromEmptyBuffer)
{
  const char* data = "12345";
  chunked_reader reader(data, 5);

  char buf[5];
  EXPECT_TRUE(reader.read(buf, 5));
  EXPECT_TRUE(reader.finished());

  // Try to read more
  EXPECT_FALSE(reader.read(buf, 1));
}

TEST(ChunkedReaderTest, ReadZeroBytes)
{
  const char* data = "test";
  chunked_reader reader(data, 4);

  char output[10];
  EXPECT_TRUE(reader.read(output, 0));
  EXPECT_EQ(reader.leftover(), 4);
  EXPECT_FALSE(reader.finished());
}

TEST(ChunkedReaderTest, GetPointer)
{
  const char* data = "0123456789";
  chunked_reader reader(data, 10);

  const char* ptr = reader.get(5);
  EXPECT_NE(ptr, nullptr);
  EXPECT_EQ(reader.leftover(), 5);
  EXPECT_EQ(std::string(ptr, 5), "01234");

  ptr = reader.get(5);
  EXPECT_NE(ptr, nullptr);
  EXPECT_EQ(reader.leftover(), 0);
  EXPECT_TRUE(reader.finished());
  EXPECT_EQ(std::string(ptr, 5), "56789");
}

TEST(ChunkedReaderTest, GetExceedsBuffer)
{
  const char* data = "12345";
  chunked_reader reader(data, 5);

  const char* ptr = reader.get(10);
  EXPECT_EQ(ptr, nullptr);
  EXPECT_EQ(reader.leftover(), 5);
}

TEST(ChunkedReaderTest, GetZeroBytes)
{
  const char* data = "test";
  chunked_reader reader(data, 4);

  const char* ptr = reader.get(0);
  EXPECT_NE(ptr, nullptr);
  EXPECT_EQ(reader.leftover(), 4);
}

TEST(ChunkedReaderTest, EmptyBuffer)
{
  const char* data = "";
  chunked_reader reader(data, 0);

  EXPECT_EQ(reader.leftover(), 0);
  EXPECT_TRUE(reader.finished());

  char buf[1];
  EXPECT_FALSE(reader.read(buf, 1));
  EXPECT_EQ(reader.get(1), nullptr);
}

// Tests for block_header
TEST(BlockHeaderTest, SizeCalculation)
{
  block_header header;
  header.BlockSize = 100;

  EXPECT_EQ(header.size(), 100 - sizeof(block_header));
}

TEST(BlockHeaderTest, NetworkByteOrder)
{
  block_header header;
  header.CheckSum = 0x12345678;
  header.BlockSize = 1024;
  header.BlockNumber = 42;

  // Verify that network_order types are being used
  // (they should convert to/from network byte order automatically)
  EXPECT_EQ(header.CheckSum.load(), 0x12345678u);
  EXPECT_EQ(header.BlockSize.load(), 1024u);
  EXPECT_EQ(header.BlockNumber.load(), 42u);
}

// Tests for record_header
TEST(RecordHeaderTest, SizeCalculation)
{
  record_header header;
  header.DataSize = 256;

  EXPECT_EQ(header.size(), 256);
}

TEST(RecordHeaderTest, NetworkByteOrder)
{
  record_header header;
  header.FileIndex = 100;
  header.Stream = -200;
  header.DataSize = 512;

  EXPECT_EQ(header.FileIndex.load(), 100);
  EXPECT_EQ(header.Stream.load(), -200);
  EXPECT_EQ(header.DataSize.load(), 512u);
}

// Tests for raii_fd
TEST(RaiiFdTest, DefaultConstruction)
{
  raii_fd fd;
  EXPECT_FALSE(fd);
  EXPECT_EQ(fd.fileno(), -1);
}

TEST(RaiiFdTest, ConstructionWithFd)
{
  raii_fd fd(5);
  EXPECT_TRUE(fd);
  EXPECT_EQ(fd.fileno(), 5);
}

TEST(RaiiFdTest, MoveConstruction)
{
  raii_fd fd1(5);
  raii_fd fd2(std::move(fd1));

  EXPECT_FALSE(fd1);
  EXPECT_EQ(fd1.fileno(), -1);
  EXPECT_TRUE(fd2);
  EXPECT_EQ(fd2.fileno(), 5);
}

TEST(RaiiFdTest, MoveAssignment)
{
  raii_fd fd1(5);
  raii_fd fd2(10);

  fd2 = std::move(fd1);

  EXPECT_FALSE(fd1);
  EXPECT_EQ(fd1.fileno(), -1);
  EXPECT_TRUE(fd2);
  EXPECT_EQ(fd2.fileno(), 5);
}

TEST(RaiiFdTest, Release)
{
  raii_fd fd(5);
  EXPECT_TRUE(fd);

  int released = fd.release();
  EXPECT_EQ(released, 5);
  EXPECT_FALSE(fd);
  EXPECT_EQ(fd.fileno(), -1);
}

TEST(RaiiFdTest, BoolConversion)
{
  raii_fd invalid_fd(-1);
  raii_fd valid_fd(5);

  EXPECT_FALSE(invalid_fd);
  EXPECT_TRUE(valid_fd);
}

// Integration test: Write and Read
TEST(ChunkedIOIntegrationTest, WriteAndRead)
{
  std::vector<char> buffer(20);

  // Write phase
  {
    chunked_writer writer(buffer.data(), buffer.size());
    EXPECT_TRUE(writer.write("Hello ", 6));
    EXPECT_TRUE(writer.write("World!", 6));
    EXPECT_TRUE(writer.write("12345678", 8));
    EXPECT_TRUE(writer.finished());
  }

  // Read phase
  {
    chunked_reader reader(buffer.data(), buffer.size());
    char buf1[6], buf2[6], buf3[8];

    EXPECT_TRUE(reader.read(buf1, 6));
    EXPECT_EQ(std::string(buf1, 6), "Hello ");

    EXPECT_TRUE(reader.read(buf2, 6));
    EXPECT_EQ(std::string(buf2, 6), "World!");

    EXPECT_TRUE(reader.read(buf3, 8));
    EXPECT_EQ(std::string(buf3, 8), "12345678");

    EXPECT_TRUE(reader.finished());
  }
}

// Boundary test: Single byte operations
TEST(ChunkedIOBoundaryTest, SingleByteOperations)
{
  char buffer[3];

  // Write single bytes
  {
    chunked_writer writer(buffer, 3);
    EXPECT_TRUE(writer.write("a", 1));
    EXPECT_TRUE(writer.write("b", 1));
    EXPECT_TRUE(writer.write("c", 1));
    EXPECT_TRUE(writer.finished());
  }

  // Read single bytes
  {
    chunked_reader reader(buffer, 3);
    char a, b, c;
    EXPECT_TRUE(reader.read(&a, 1));
    EXPECT_TRUE(reader.read(&b, 1));
    EXPECT_TRUE(reader.read(&c, 1));
    EXPECT_EQ(a, 'a');
    EXPECT_EQ(b, 'b');
    EXPECT_EQ(c, 'c');
    EXPECT_TRUE(reader.finished());
  }
}
