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
    result.emplace_back(std::begin(view), std::end(view));
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
    result.emplace_back(std::begin(view), std::end(view));
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

class WindowsEnvironment : public ::testing::Environment {
  void SetUp() override { InitWinAPIWrapper(); }
};

const testing::Environment* _global_env =
  testing::AddGlobalTestEnvironment(new WindowsEnvironment);

enum class VssStatus {
  Enabled, Disabled
};

std::string_view VssStatusName(VssStatus status) {
  using namespace std::literals;
  switch (status) {
  case VssStatus::Enabled: {
    return "Enabled"sv;
  } break;
  case VssStatus::Disabled: {
    return "Disabled"sv;
  } break;
  }
  return "unreachble"sv;
}

void PrintTo(const VssStatus& status, std::ostream* os) {
  *os << VssStatusName(status);
}

class Vss : public ::testing::TestWithParam<VssStatus> {
  void SetUp() override {
    switch (GetParam()) {
    case VssStatus::Enabled: {

    } break;
    case VssStatus::Disabled: {

    } break;
    }
  }
  void TearDown() override {
    switch (GetParam()) {
    case VssStatus::Enabled: {

    } break;
    case VssStatus::Disabled: {

    } break;
    }
  }
};

TEST_P(Vss, utf8_to_wchar_paths) {
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


TEST_P(Vss, wchar_to_utf8_paths) {
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

INSTANTIATE_TEST_CASE_P(ShadowCopy, Vss, ::testing::Values(VssStatus::Enabled, VssStatus::Disabled),
			[](const testing::TestParamInfo<Vss::ParamType>& info) -> std::string {
			  return std::string(VssStatusName(info.param));
			});
