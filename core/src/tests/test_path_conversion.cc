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

class WindowsEnvironment : public ::testing::Environment {
  void SetUp() override {
    InitWinAPIWrapper();
  }
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

// used by gtest to print VssStatus;
// this is needed to be able to use VssStatus as test param
void PrintTo(const VssStatus& status, std::ostream* os) {
  *os << VssStatusName(status);
}

bool OldConvert(const char* path, char* buf, int len) {
  using namespace std::literals;
  constexpr std::string_view s = "\\\\?\\vss\\"sv;
  strncpy(buf, s.data(), len);
  len -= s.size();
  strncat(buf, path, len);
  return true;
}

bool OldConvertW(const wchar_t* path, wchar_t *buf, int len) {
  using namespace std::literals;
  constexpr std::wstring_view s = L"\\\\?\\vss\\"sv;
  wcsncpy(buf, s.data(), len);
  len -= s.size();
  wcsncat(buf, path, len);
  return true;
}

char* NewConvert(const char* path) {
  int len = strlen(path) + 100;
  char* buf = (char*) malloc(len);
  OldConvert(path, buf, len);
  return buf;
}

wchar_t* NewConvertW(const wchar_t* path) {
  int len = wcslen(path) + 100;
  wchar_t* buf = (wchar_t*) malloc(len * sizeof(*path));
  OldConvertW(path, buf, len);
  return buf;
}

class Regression : public ::testing::TestWithParam<VssStatus> {
  void SetUp() override {
    switch (GetParam()) {
    case VssStatus::Enabled: {
      old_path_conversion::Win32SetPathConvert(OldConvert, OldConvertW);
      SetVSSPathConvert(NewConvert, NewConvertW);
    } break;
    case VssStatus::Disabled: {

    } break;
    }
  }
  void TearDown() override {
    switch (GetParam()) {
    case VssStatus::Enabled: {
      old_path_conversion::Win32SetPathConvert(nullptr, nullptr);
      SetVSSPathConvert(nullptr, nullptr);
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
    ""sv,
    "\\\\.\\C:"sv,
    "\\\\.\\C:\\"sv,
    "\\\\?\\C:"sv,
    "\\\\?\\C:\\"sv,
    "\\\\.\\d:"sv,
    "\\\\.\\d:\\"sv,
    "\\\\?\\d:"sv,
    "\\\\?\\d:\\"sv,
    "C:/test"sv,
    "//./normalized_path"sv,
    "\\\\.\\normalized_path"sv,
    "//?/literal_path"sv,
    "\\\\?\\literal_path"sv,
    "//./normalized_path/.\\..\\/test"sv,
    "\\\\.\\normalized_path/.\\..\\/test"sv,
    "//?/literal_path/.\\..\\/test"sv,
    "\\\\?\\literal_path/.\\..\\/test"sv,
    "//localhast/C$/dir"sv,
    "\\\\localhost\\C$\\dir"sv,
    "//localhost/d$"sv,
    "\\\\localhost\\d$"sv,
    "//localhost/d$/"sv,
    "\\\\localhost\\d$\\"sv,
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

TEST_P(Regression, utf8_long_path) {
  // path bigger than 4K
  std::string path = "C:/" + std::string(5000, 'a') + 'b';

  PoolMem converted;
  unix_name_to_win32(converted.addr(), path.data());
  std::string new_str{converted.c_str()};
  std::string old_str = OldU2U(path.data());
  EXPECT_EQ(new_str, old_str);
}

TEST_P(Regression, wchar_long_path) {
  // path bigger than 4K
  std::string path = "C:/" + std::string(5000, 'a') + 'b';

  std::wstring new_str = make_win32_path_UTF8_2_wchar(path);
  std::wstring old_str = OldU2W(path.data());
  EXPECT_EQ(new_str, old_str);
}

INSTANTIATE_TEST_CASE_P(ShadowCopy, Regression, ::testing::Values(VssStatus::Enabled, VssStatus::Disabled),
			[](const testing::TestParamInfo<Regression::ParamType>& info) -> std::string {
			  return std::string(VssStatusName(info.param));
			});

extern std::wstring ReplaceSlashes(std::wstring_view path);

TEST(ReplaceSlashes, literal_paths) {
  using namespace std::literals;
  auto literal_paths = {
    L"\\\\?\\path\\to\\somewhere"sv,
    L"//?/path\\to\\somewhere"sv,
    L"//?/path/to/somewhere"sv,
    L"//?/path\\\\//\\to\\\\somewhere//////"sv,
    L"//?/path\\\\//\\to\\\\somewhere///\\///"sv,
  };

  // strings have prettier output in gtest so they are used
  // here instead of string_views
  auto expected_no_slash = L"\\\\?\\path\\to\\somewhere"s;
  auto expected_with_slash = L"\\\\?\\path\\to\\somewhere\\"s;

  for (auto view : literal_paths) {
    auto replaced = ReplaceSlashes(view);
    auto& expected = (replaced.back() == L'\\') ? expected_with_slash
      : expected_no_slash;
    ASSERT_EQ(replaced, expected)
      << "replacing slashes in " << FromUtf16(view).c_str();
  }
}

TEST(ReplaceSlashes, normalized_paths) {
  using namespace std::literals;

  auto normalized_paths = {
    L"\\\\.\\path\\to\\somewhere"sv,
    L"//./path\\to\\somewhere"sv,
    L"//./path/to/somewhere"sv,
    L"//./path\\\\//\\to\\\\somewhere//////"sv,
    L"//./path\\\\//\\to\\\\somewhere///\\///"sv,
  };

  auto expected_no_slash = L"\\\\.\\path\\to\\somewhere"s;
  auto expected_with_slash = L"\\\\.\\path\\to\\somewhere\\"s;

  for (auto view : normalized_paths) {
    auto replaced = ReplaceSlashes(view);
    auto& expected = (replaced.back() == L'\\') ? expected_with_slash
      : expected_no_slash;
    ASSERT_EQ(replaced, expected)
      << "replacing slashes in " << FromUtf16(view).c_str();
  }
}

TEST(ReplaceSlashes, full_paths) {
  using namespace std::literals;

  auto normalized_paths = {
    L"C:\\path\\to\\somewhere"sv,
    L"C:/path\\to\\somewhere"sv,
    L"C:/path/to/somewhere"sv,
    L"C:\\/\\/path\\to\\somewhere"sv,
    L"C:/path\\\\//\\to\\\\somewhere//////"sv,
    L"C:/path\\\\//\\to\\\\somewhere///\\///"sv,
  };

  auto expected_no_slash = L"C:\\path\\to\\somewhere"s;
  auto expected_with_slash = L"C:\\path\\to\\somewhere\\"s;

  for (auto view : normalized_paths) {
    auto replaced = ReplaceSlashes(view);
    auto& expected = (replaced.back() == L'\\') ? expected_with_slash
      : expected_no_slash;
    ASSERT_EQ(replaced, expected)
      << "replacing slashes in " << FromUtf16(view).c_str();
  }
}
