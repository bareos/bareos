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

#include "compat_old_conversion.h"

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

class Regression : public ::testing::TestWithParam<VssStatus> {
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

using namespace std::literals;
const std::vector<std::string_view> paths{
    "/"sv,
    "C:"sv,
    "C:\\"sv,
    "C:/./.."sv,
    "c:\\..\\\\/test//ab\\.x"sv,
};

std::string OldU2U(const char* name) {
  PoolMem win32_name;
  old_path_conversion::unix_name_to_win32(win32_name.addr(), name);
  return std::string(win32_name.c_str());
}

std::wstring OldU2W(const char* name) {
  PoolMem win32_name;
  old_path_conversion::make_win32_path_UTF8_2_wchar(win32_name.addr(),
						    name);
  return std::wstring((wchar_t*)win32_name.c_str());
}

extern void unix_name_to_win32(POOLMEM*& win32_name, const char* name);

TEST_P(Regression, utf8_to_utf8_paths) {
  using namespace std::literals;

  for (auto path : paths) {
    PoolMem converted;
    unix_name_to_win32(converted.addr(), path.data());
    std::string new_str{converted.c_str()};
    std::string old_str = OldU2U(path.data());
    EXPECT_EQ(new_str, old_str) << "During Conversion of " << path << ".";
  }
}


TEST_P(Regression, utf8_to_wchar_paths) {
  using namespace std::literals;
  for (auto path : paths) {
    std::wstring new_str = make_win32_path_UTF8_2_wchar(path);
    std::wstring old_str = OldU2W(path.data());
    EXPECT_EQ(new_str, old_str) << "during conversion of '" << path << "'";
  }
}

INSTANTIATE_TEST_CASE_P(ShadowCopy, Regression, ::testing::Values(VssStatus::Enabled, VssStatus::Disabled),
			[](const testing::TestParamInfo<Regression::ParamType>& info) -> std::string {
			  return std::string(VssStatusName(info.param));
			});
