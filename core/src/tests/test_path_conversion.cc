/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2018-2024 Bareos GmbH & Co. KG

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
  void SetUp() override { InitWinAPIWrapper(); }
};

const testing::Environment* _global_env
    = testing::AddGlobalTestEnvironment(new WindowsEnvironment);

enum class VssStatus
{
  Enabled,
  Disabled
};

std::string_view VssStatusName(VssStatus status)
{
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
void PrintTo(const VssStatus& status, std::ostream* os)
{
  *os << VssStatusName(status);
}

static bool MockVSSConvert_OldApi(const char* path, char* buf, int len)
{
  using namespace std::literals;
  constexpr std::string_view s = "\\\\?\\vss\\"sv;
  if (len <= 0) return false;
  std::size_t ulen = static_cast<std::size_t>(len);
  buf[0] = '\0';
  // strncat can write up to n+1 bytes!
  // using strncat ensures that we end up with proper strings, that is
  // we ensure that we always have a zero terminator
  strncat(buf, s.data(), ulen - 1);
  strncat(buf, path, ulen - 1 - strlen(buf));
  return true;
}

static bool MockVSSConvertW_OldApi(const wchar_t* path, wchar_t* buf, int len)
{
  using namespace std::literals;
  constexpr std::wstring_view s = L"\\\\?\\vss\\"sv;
  if (len <= 0) return false;
  std::size_t ulen = static_cast<std::size_t>(len);
  buf[0] = L'\0';

  // wcsncat can write up to n+1 bytes!
  // using wcsncat ensures that we end up with proper strings, that is
  // we ensure that we always have a zero terminator
  wcsncat(buf, s.data(), ulen - 1);
  wcsncat(buf, path, ulen - 1 - wcslen(buf));
  return true;
}

// do the same thing as MockVSSConvert* except with the new required api
static char* MockVSSConvert_NewApi(const char* path)
{
  // use a len thats big enough to fit path without
  // relying on strlen(path) so as to not trigger
  // a false strncat 'bound depends on source' warning
  int len = 40000;
  ASSERT(static_cast<std::size_t>(len) > strlen(path));
  char* buf = (char*)malloc(len);
  MockVSSConvert_OldApi(path, buf, len);
  return buf;
}

static wchar_t* MockVSSConvertW_NewApi(const wchar_t* path)
{
  // use a len thats big enough to fit path without
  // relying on strlen(path) so as to not trigger
  // a false strncat 'bound depends on source' warning
  int len = 40000;
  ASSERT(static_cast<std::size_t>(len) > wcslen(path));
  wchar_t* buf = (wchar_t*)malloc(len * sizeof(*buf));
  MockVSSConvertW_OldApi(path, buf, len);
  return buf;
}

class Regression : public ::testing::TestWithParam<VssStatus> {
  void SetUp() override
  {
    switch (GetParam()) {
      case VssStatus::Enabled: {
        old_path_conversion::Win32SetPathConvert(MockVSSConvert_OldApi,
                                                 MockVSSConvertW_OldApi);
        SetVSSPathConvert(MockVSSConvert_NewApi, MockVSSConvertW_NewApi);
      } break;
      case VssStatus::Disabled: {
      } break;
    }
  }
  void TearDown() override
  {
    switch (GetParam()) {
      case VssStatus::Enabled: {
        old_path_conversion::Win32SetPathConvert(nullptr, nullptr);
        SetVSSPathConvert(nullptr, nullptr);
      } break;
      case VssStatus::Disabled: {
      } break;
    }
    // reset the conversion cache!
    Win32ResetConversionCache();
  }
};

using namespace std::literals;

std::string make_long_file_name()
{
  std::string file_name(30000, 'a');
  file_name[0] = 'Z';
  file_name[1] = ':';
  file_name[2] = '/';
  return file_name;
}
std::string long_file_name = make_long_file_name();
const std::vector<std::string_view> paths{
    "/"sv,
    "/aaa"sv,
    "C:\\"sv,
    "\\\\?\\C:"sv,
    "\\\\?\\C:\\"sv,
    "\\\\?\\d:"sv,
    "\\\\?\\d:\\"sv,
    "C:/test"sv,
    "\\\\?\\literal_path"sv,
    "\\\\?\\literal_path\\.\\..\\\test"sv,
    long_file_name,

};

std::string OldU2U(const char* name)
{
  PoolMem win32_name;
  old_path_conversion::unix_name_to_win32(win32_name.addr(), name);
  return std::string(win32_name.c_str());
}

std::wstring OldU2W(const char* name)
{
  PoolMem win32_name;
  old_path_conversion::make_win32_path_UTF8_2_wchar(win32_name.addr(), name);
  return std::wstring((wchar_t*)win32_name.c_str());
}

extern void unix_name_to_win32(POOLMEM*& win32_name, const char* name);

TEST_P(Regression, utf8_to_utf8_paths)
{
  using namespace std::literals;

  for (auto path : paths) {
    PoolMem converted;
    unix_name_to_win32(converted.addr(), path.data());
    std::string new_str{converted.c_str()};
    std::string old_str = OldU2U(path.data());
    EXPECT_EQ(new_str, old_str) << "During Conversion of " << path << ".";
  }
}


TEST_P(Regression, utf8_to_wchar_paths)
{
  using namespace std::literals;
  for (auto path : paths) {
    std::wstring new_str = make_win32_path_UTF8_2_wchar(path);
    std::wstring old_str = OldU2W(path.data());
    EXPECT_EQ(new_str, old_str) << "during conversion of '" << path << "'";
  }
}

TEST_P(Regression, utf8_long_path)
{
  // path bigger than 4K
  std::string path = "C:/" + std::string(5000, 'a') + 'b';

  PoolMem converted;
  unix_name_to_win32(converted.addr(), path.data());
  std::string new_str{converted.c_str()};
  std::string old_str = OldU2U(path.data());
  EXPECT_EQ(new_str, old_str);
}

TEST_P(Regression, wchar_long_path)
{
  // path bigger than 4K
  std::string path = "C:/" + std::string(5000, 'a') + 'b';

  std::wstring new_str = make_win32_path_UTF8_2_wchar(path);
  std::wstring old_str = OldU2W(path.data());
  EXPECT_EQ(new_str, old_str);
}

auto invalid_paths = {
    "C:/dir./",        "C:/dir</",        "C:/dir>/",
    "C:/dir*/",        "C:/dir?/",        "C:/dir /",

    "C:/dir./rest/",   "C:/dir</rest/",   "C:/dir>/rest/",
    "C:/dir*/rest/",   "C:/dir?/rest/",   "C:/dir /rest/",

    "C:/file.",        "C:/file<",        "C:/file>",
    "C:/file*",        "C:/file?",        "C:/file ",
};

TEST_P(Regression, utf8_invalid_paths)
{
  using namespace std::literals;

  for (auto path : invalid_paths) {
    PoolMem converted;
    unix_name_to_win32(converted.addr(), path);
    std::string new_str{converted.c_str()};
    std::string old_str = OldU2U(path);
    EXPECT_EQ(new_str, old_str) << "During Conversion of " << path << ".";
  }
}

TEST_P(Regression, wchar_invalid_paths)
{
  using namespace std::literals;

  for (auto path : invalid_paths) {
    std::wstring new_str = make_win32_path_UTF8_2_wchar(path);
    std::wstring old_str = OldU2W(path);
    EXPECT_EQ(new_str, old_str) << "During Conversion of " << path << ".";
  }
}

INSTANTIATE_TEST_CASE_P(
    ShadowCopy,
    Regression,
    ::testing::Values(VssStatus::Enabled, VssStatus::Disabled),
    [](const testing::TestParamInfo<Regression::ParamType>& t_info)
        -> std::string { return std::string(VssStatusName(t_info.param)); });

extern std::wstring ReplaceSlashes(std::wstring_view path);

TEST(ReplaceSlashes, literal_paths)
{
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
    auto& expected
        = (replaced.back() == L'\\') ? expected_with_slash : expected_no_slash;
    ASSERT_EQ(replaced, expected)
        << "replacing slashes in " << FromUtf16(view).c_str();
  }
}

TEST(ReplaceSlashes, normalized_paths)
{
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
    auto& expected
        = (replaced.back() == L'\\') ? expected_with_slash : expected_no_slash;
    ASSERT_EQ(replaced, expected)
        << "replacing slashes in " << FromUtf16(view).c_str();
  }
}

TEST(ReplaceSlashes, full_paths)
{
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
    auto& expected
        = (replaced.back() == L'\\') ? expected_with_slash : expected_no_slash;
    ASSERT_EQ(replaced, expected)
        << "replacing slashes in " << FromUtf16(view).c_str();
  }
}
