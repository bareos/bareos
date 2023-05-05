/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2018-2023 Bareos GmbH & Co. KG

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

#include <vector>
#include <string>
#include <cstring>

#include <compat.h>
#include <lib/mem_pool.h>

template<typename CharT>
bool ByteEq(std::basic_string_view<CharT> lhs,
	    std::basic_string_view<CharT> rhs) {
  return lhs.size() == rhs.size() &&
    std::memcmp(lhs.data(), rhs.data(), rhs.size() * sizeof(CharT)) == 0;
}

std::vector<std::wstring> OldU2W(std::vector<std::string_view> strings) {
  std::vector<std::wstring> result;
  for (auto view : strings) {
    PoolMem mem(PM_FNAME);
    UTF8_2_wchar(mem.addr(), view.data());
    result.emplace_back((wchar_t*)(mem.c_str()));
  }
  return result;
}

std::vector<std::wstring> NewU2W(std::vector<std::string_view> strings) {
  std::vector<std::wstring> result;
  for (auto view : strings) {
    result.emplace_back(std::move(FromUtf8(view)));
  }
  return result;
}

std::vector<std::string> OldW2U(std::vector<std::wstring_view> strings) {
  std::vector<std::string> result;
  for (auto view : strings) {
    PoolMem mem(PM_FNAME);
    wchar_2_UTF8(mem.addr(), view.data());
    result.emplace_back(mem.c_str());
  }
  return result;
}

std::vector<std::string> NewW2U(std::vector<std::wstring_view> strings) {
  std::vector<std::string> result;
  for (auto view : strings) {
    result.emplace_back(std::move(FromUtf16(view)));
  }
  return result;
}

class Conversions : public ::testing::Test {
  void SetUp() override { InitWinAPIWrapper(); }
};

TEST_F(Conversions, utf8_to_wchar_paths) {
  using namespace std::literals;
  std::vector<std::string_view> paths{
    "/"sv, "C:"sv, "D:\\"sv, "D:/./.."sv,
    "D:\\.."sv,
  };

  auto old_ver = OldU2W(paths);
  auto new_ver = NewU2W(paths);
  ASSERT_EQ(old_ver.size(), paths.size());
  ASSERT_EQ(new_ver.size(), paths.size());

  for (std::size_t i = 0; i < paths.size(); ++i) {
    EXPECT_PRED2(ByteEq<wchar_t>, old_ver[i], new_ver[i]);
  }
}


TEST_F(Conversions, utf8_to_wchar_phrases) {
  using namespace std::literals;
  std::vector<std::string_view> strings{
    "Döner macht schöner."sv,
    "烤肉串使你更加美丽。"sv,
    "Döner kebab gør dig smukkere."sv,
    "도너 케밥은 당신을 더 아름답게 만듭니다."sv,
    "Донер-кебаб робить вас красивішими."sv
  };

  auto old_ver = OldU2W(strings);
  auto new_ver = NewU2W(strings);
  ASSERT_EQ(old_ver.size(), strings.size());
  ASSERT_EQ(new_ver.size(), strings.size());

  for (std::size_t i = 0; i < strings.size(); ++i) {
    EXPECT_PRED2(ByteEq<wchar_t>, old_ver[i], new_ver[i]);
  }
}

TEST_F(Conversions, wchar_to_utf8_paths) {
  using namespace std::literals;
  std::vector<std::wstring_view> paths{
    L"/"sv, L"C:"sv, L"D:\\"sv, L"D:/./.."sv,
    L"D:\\.."sv,
  };

  auto old_ver = OldW2U(paths);
  auto new_ver = NewW2U(paths);
  ASSERT_EQ(old_ver.size(), paths.size());
  ASSERT_EQ(new_ver.size(), paths.size());

  for (std::size_t i = 0; i < paths.size(); ++i) {
    EXPECT_PRED2(ByteEq<char>, old_ver[i], new_ver[i]);
  }
}


TEST_F(Conversions, wchar_to_utf8_phrases) {
  using namespace std::literals;
  std::vector<std::wstring_view> strings{
    L"Döner macht schöner."sv,
    L"烤肉串使你更加美丽。"sv,
    L"Döner kebab gør dig smukkere."sv,
    L"도너 케밥은 당신을 더 아름답게 만듭니다."sv,
    L"Донер-кебаб робить вас красивішими."sv
  };

  auto old_ver = OldW2U(strings);
  auto new_ver = NewW2U(strings);
  ASSERT_EQ(old_ver.size(), strings.size());
  ASSERT_EQ(new_ver.size(), strings.size());

  for (std::size_t i = 0; i < strings.size(); ++i) {
    EXPECT_PRED2(ByteEq<char>, old_ver[i], new_ver[i]);
  }
}
