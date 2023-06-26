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

#if defined(HAVE_MINGW)
#  include "include/bareos.h"
#  include "gtest/gtest.h"
#else
#  include "gtest/gtest.h"
#  include "include/bareos.h"
#endif

#include <filesystem>
#include <string_view>
#include "stored/backends/dedup/util.h"

using namespace dedup::util;
namespace stdfs = std::filesystem;


// class WithCleanupFile : public testing::Test {
//  protected:
//   void SetUp() override {
//     dir = raii_fd{path.c_str(), O_CREAT | O_RDWR | O_BINARY, 0666};
//   }

//   void TearDown() override {
//     dir = raii_fd{}; // close dir
//     std::filesystem::remove_all(path.c_str());
//     std::filesystem::remove(path.c_str());
//   }

//   std::string path;
// private:
//   raii_fd dir;
// };

template <typename T> static bool has_value(std::optional<T> val)
{
  return val.has_value();
}

TEST(FileBasedVector, write)
{
  auto path = stdfs::temp_directory_path() / "WriteTest";
  stdfs::remove(path);
  ASSERT_FALSE(stdfs::exists(path));
  raii_fd file{path.c_str(), O_CREAT | O_RDWR | O_BINARY, 0666};
  int fd = file.get();

  ASSERT_GE(fd, 0);

  file_based_vector<char> vec{std::move(file), 0, 1024};

  EXPECT_TRUE(vec.is_ok());

  std::string_view test_view = "Hallo, Welt! Dies ist ein Test!";

  std::optional res = vec.write(test_view.data(), test_view.size());

  EXPECT_PRED1(has_value<std::remove_reference_t<decltype(*res)>>, res);
  struct stat st;
  fstat(fd, &st);

  EXPECT_EQ(st.st_size, 1024);

  EXPECT_EQ(vec.size(), test_view.size());
  EXPECT_EQ(vec.current(), test_view.size());

  bool success = vec.move_to(0);
  EXPECT_EQ(vec.current(), 0);

  EXPECT_TRUE(success);

  auto read = vec.read(test_view.size());

  EXPECT_NE(read, nullptr);

  std::string_view read_view{read.get(), test_view.size()};

  EXPECT_EQ(test_view, read_view);
  EXPECT_TRUE(vec.is_ok());
  stdfs::remove(path);
}

TEST(FileBasedVector, reserve_and_write_at)
{
  auto path = stdfs::temp_directory_path() / "WriteAtTest";
  stdfs::remove(path);
  ASSERT_FALSE(stdfs::exists(path));
  raii_fd file{path.c_str(), O_CREAT | O_RDWR | O_BINARY, 0666};
  int fd = file.get();

  ASSERT_GE(fd, 0);

  file_based_vector<char> vec{std::move(file), 0, 1024};

  EXPECT_TRUE(vec.is_ok());

  std::string_view test_view = "Hallo, Welt! Dies ist ein Test!";

  std::optional where1 = vec.reserve(test_view.size());
  EXPECT_PRED1(has_value<std::remove_reference_t<decltype(*where1)>>, where1);
  EXPECT_EQ(vec.current(), test_view.size());
  EXPECT_EQ(vec.size(), test_view.size());

  struct stat st;

  fstat(fd, &st);
  EXPECT_EQ(st.st_size, 1024);

  std::optional where2 = vec.write(test_view.data(), test_view.size());
  EXPECT_PRED1(has_value<std::remove_reference_t<decltype(*where2)>>, where2);
  EXPECT_EQ(vec.current(), 2 * test_view.size());
  EXPECT_EQ(vec.size(), 2 * test_view.size());

  fstat(fd, &st);
  EXPECT_EQ(st.st_size, 1024);

  std::optional res = vec.write_at(*where1, test_view.data(), test_view.size());
  EXPECT_PRED1(has_value<std::remove_reference_t<decltype(*res)>>, res);
  EXPECT_EQ(vec.current(), 2 * test_view.size());
  EXPECT_EQ(vec.size(), 2 * test_view.size());

  auto read1 = vec.read_at(*where1, test_view.size());
  auto read2 = vec.read_at(*where2, test_view.size());
  EXPECT_EQ(vec.current(), 2 * test_view.size());

  EXPECT_TRUE(vec.is_ok());

  EXPECT_NE(read1, nullptr);
  EXPECT_NE(read2, nullptr);

  std::string_view read1_view{read1.get(), test_view.size()};
  std::string_view read2_view{read2.get(), test_view.size()};

  EXPECT_EQ(test_view, read1_view);
  EXPECT_EQ(test_view, read2_view);
  EXPECT_TRUE(vec.is_ok());
}
