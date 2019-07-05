/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2019-2019 Bareos GmbH & Co. KG

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
#include "include/bareos.h"

#include "dird/bsr.h"

#include <algorithm>
#include <numeric>
#include <chrono>
#include <iostream>
#include <random>

using namespace std::chrono;
using namespace directordaemon;
using namespace std;

namespace directordaemon {
bool DoReloadConfig() { return false; }
}  // namespace directordaemon

enum
{
  kJobId_1 = 1,
  kJobId_2 = 2
};

// here you can set the size of the fileid vector generated
static const auto kFidCount = 40000;

TEST(fileindex_list, add_filendexes)
{
  RestoreBootstrapRecord bsr;
  AddFindex(&bsr, kJobId_1, 1);
  ASSERT_NE(bsr.fi, nullptr);
  EXPECT_EQ(bsr.JobId, kJobId_1);
  EXPECT_EQ(bsr.fi->getRanges().size(), 1);

  AddFindex(&bsr, kJobId_2, 1);
  AddFindex(&bsr, kJobId_2, 3);
  ASSERT_NE(bsr.next, nullptr);
  EXPECT_EQ(bsr.next->JobId, kJobId_2);
  ASSERT_NE(bsr.next->fi, nullptr);
  EXPECT_EQ(bsr.next->fi->getRanges().size(), 2);
}

template <typename F>
static void TimedLambda(const char* name, F func)
{
  auto start_time = std::chrono::high_resolution_clock::now();
  func();
  auto end_time = std::chrono::high_resolution_clock::now();
  auto elapsed_time = std::chrono::duration<double>{end_time - start_time};
  cout << name << " exec time: " << elapsed_time.count() << "\n";
}

static const std::string RangeStr(int begin, int end)
{
  return begin == end ? to_string(begin)
                      : to_string(begin) + "-" + to_string(end);
}


template <typename itBegin, typename itEnd>
static std::string ToBsrStringLocal(const itBegin& t_begin, const itEnd& t_end)
{
  auto bsr = std::string{""};
  TimedLambda("ToBsrStringLocal", [&]() {
    std::set<int> fileIds{t_begin, t_end};
    int begin{0};
    int end{0};

    for (auto fileId : fileIds) {
      if (begin == 0) { begin = end = fileId; }

      if (fileId > end + 1) {
        bsr += "FileIndex=" + RangeStr(begin, end) + "\n";
        begin = end = fileId;
      } else {
        end = fileId;
      }
    }
    bsr += "FileIndex=" + RangeStr(begin, end) + "\n";
  });
  return bsr;
}

static std::string ToBsrStringLocal(const std::vector<int>& t_fileIds)
{
  return ToBsrStringLocal(t_fileIds.begin(), t_fileIds.end());
}

template <typename itBegin, typename itEnd>
static std::string ToBsrStringBareos(const itBegin& t_begin, const itEnd& t_end)
{
  RestoreBootstrapRecord bsr;
  TimedLambda("AddFindex total", [&]() {
    std::for_each(t_begin, t_end,
                  [&](int fid) { AddFindex(&bsr, kJobId_1, fid); });
  });
  auto maxId = *std::max_element(t_begin, t_end);
  auto buffer = std::string{};
  TimedLambda("write_findex total",
              [&]() { write_findex(bsr.fi.get(), 1, maxId, buffer); });
  return std::string{buffer.c_str()};
}

static std::string ToBsrStringBareos(const std::vector<int>& t_fileIds)
{
  return ToBsrStringBareos(t_fileIds.begin(), t_fileIds.end());
}

TEST(fileindex_list, continous_list)
{
  auto fileIds = std::vector<int>(kFidCount);
  std::iota(fileIds.begin(), fileIds.end(), 1);
  EXPECT_EQ(ToBsrStringLocal(fileIds), ToBsrStringBareos(fileIds));
}

TEST(fileindex_list, continous_list_reverse)
{
  auto fileIds = std::vector<int>(kFidCount);
  std::iota(fileIds.begin(), fileIds.end(), 1);
  std::reverse(fileIds.begin(), fileIds.end());
  EXPECT_EQ(ToBsrStringLocal(fileIds), ToBsrStringBareos(fileIds));
}

TEST(fileindex_list, gap_list)
{
  auto fileIds = std::vector<int>(kFidCount);
  std::generate(fileIds.begin(), fileIds.end(), []() {
    static auto i = 1;
    return i += 2;
  });

  EXPECT_EQ(ToBsrStringLocal(fileIds), ToBsrStringBareos(fileIds));
}

TEST(fileindex_list, gap_list_reverse)
{
  auto fileIds = std::vector<int>(kFidCount);
  std::generate(fileIds.begin(), fileIds.end(), []() {
    static auto i = 0;
    return i += 2;
  });

  std::reverse(fileIds.begin(), fileIds.end());
  EXPECT_EQ(ToBsrStringLocal(fileIds), ToBsrStringBareos(fileIds));
}

TEST(fileindex_list, few_gap_list)
{
  auto fileIds = std::vector<int>(kFidCount);
  std::generate(fileIds.begin(), fileIds.end(), []() {
    static auto i = 0;
    ++i;
    if (i % 100 == 0) ++i;
    return i;
  });

  EXPECT_EQ(ToBsrStringLocal(fileIds), ToBsrStringBareos(fileIds));
}

TEST(fileindex_list, few_gap_list_random_order)
{
  auto fileIds = std::vector<int>(kFidCount);
  std::generate(fileIds.begin(), fileIds.end(), []() {
    static auto i = 0;
    ++i;
    if (i % 100 == 0) ++i;
    return i;
  });

  std::shuffle(fileIds.begin(), fileIds.end(), std::default_random_engine{});
  EXPECT_EQ(ToBsrStringLocal(fileIds), ToBsrStringBareos(fileIds));
}

TEST(fileindex_list, gap_list_duplicates_random_order)
{
  auto fileIds = std::vector<int>(kFidCount);
  std::generate(fileIds.begin(), fileIds.end(), []() {
    static auto i = 0;
    return i += 2;
  });
  std::fill(fileIds.begin() + 10, fileIds.begin() + 20, 55);

  std::shuffle(fileIds.begin(), fileIds.end(), std::default_random_engine{});
  EXPECT_EQ(ToBsrStringLocal(fileIds), ToBsrStringBareos(fileIds));
}
