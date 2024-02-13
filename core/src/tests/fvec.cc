/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2023-2024 Bareos GmbH & Co. KG

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
#endif

#include "gtest/gtest.h"
#include "stored/backends/dedup/fvec.h"

#include <cstdio>
#include <memory>
#include <vector>
#include <random>
#include <cerrno>

using namespace dedup;

struct file_closer {
  void operator()(FILE* f) const { std::fclose(f); }
};

std::vector<int> gen_rand_ints(std::size_t count)
{
  std::random_device rd;
  std::default_random_engine gen(rd());
  std::uniform_int_distribution<int> dist{};

  std::vector<int> data;
  for (std::size_t i = 0; i < count; ++i) { data.push_back(dist(gen)); }

  return data;
}

struct fvec_fixture : public testing::Test {
 protected:
  std::unique_ptr<FILE, file_closer> backing;
  std::vector<int> rand_ints;

  void SetUp() override
  {
    backing = std::unique_ptr<FILE, file_closer>(std::tmpfile());
    rand_ints = gen_rand_ints(100);
  }
  void TearDown() override { rand_ints.clear(); }
};

std::size_t file_size(int fd)
{
  struct stat s;
  if (fstat(fd, &s) != 0) { return 0; }

  return s.st_size;
}

TEST_F(fvec_fixture, Creation)
{
  int fd = fileno(backing.get());

  try {
    fvec<int> v(access::rdwr, fd);
  } catch (const std::system_error& ec) {
    FAIL() << "Error: " << ec.code() << " - " << ec.what() << "\n";
  } catch (const std::exception& ec) {
    FAIL() << "Error: " << ec.what() << "\n";
  }
}

TEST_F(fvec_fixture, Pushing)
{
  try {
    int fd = fileno(backing.get());
    fvec<int> v(access::rdwr, fd);

    for (auto i : rand_ints) { v.push_back(i); }

    EXPECT_EQ(v.size(), rand_ints.size());
  } catch (const std::system_error& ec) {
    FAIL() << "Error: " << ec.code() << " - " << ec.what() << "\n";
  } catch (const std::exception& ec) {
    FAIL() << "Error: " << ec.what() << "\n";
  }
}

TEST_F(fvec_fixture, PushConsistency)
{
  try {
    int fd = fileno(backing.get());

    {
      fvec<int> v(access::rdwr, fd, 0);
      for (auto i : rand_ints) { v.push_back(i); }
    }

    fvec<int> v(access::rdonly, fd, rand_ints.size());

    EXPECT_EQ(v.size(), rand_ints.size());

    for (std::size_t i = 0; i < v.size(); ++i) {
      EXPECT_EQ(v[i], rand_ints[i]);
    }

  } catch (const std::system_error& ec) {
    FAIL() << "Error: " << ec.code() << " - " << ec.what() << "\n";
  } catch (const std::exception& ec) {
    FAIL() << "Error: " << ec.what() << "\n";
  }
}

TEST_F(fvec_fixture, Clear)
{
  try {
    int fd = fileno(backing.get());

    {
      fvec<int> v(access::rdwr, fd, 0);
      for (auto i : rand_ints) { v.push_back(i); }
    }

    fvec<int> v(access::rdwr, fd, rand_ints.size());
    EXPECT_EQ(v.size(), rand_ints.size());
    v.clear();
    EXPECT_EQ(v.size(), 0);

    v.resize_to_fit();

    EXPECT_EQ(file_size(fd), v.size() * sizeof(int));
  } catch (const std::system_error& ec) {
    FAIL() << "Error: " << ec.code() << " - " << ec.what() << "\n";
  } catch (const std::exception& ec) {
    FAIL() << "Error: " << ec.what() << "\n";
  }
}

TEST_F(fvec_fixture, PushWithChecks)
{
  int fd = fileno(backing.get());

  try {
    for (std::size_t i = 0; i < rand_ints.size(); ++i) {
      fvec<int> v(access::rdwr, fd, i);
      EXPECT_EQ(v.size(), i);
      v.push_back(i);
      EXPECT_EQ(v.size(), i + 1);
      EXPECT_LE(v.size(), v.capacity());
      EXPECT_LE(v.capacity() * v.element_size, file_size(fd));
      v.resize_to_fit();
      EXPECT_EQ(v.capacity(), v.size());
      EXPECT_EQ(v.capacity() * v.element_size, file_size(fd));
    }

  } catch (const std::system_error& ec) {
    FAIL() << "Error: " << ec.code() << " - " << ec.what() << "\n";
  } catch (const std::exception& ec) {
    FAIL() << "Error: " << ec.what() << "\n";
  }
}
