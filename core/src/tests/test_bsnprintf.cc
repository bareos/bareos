/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2019-2025 Bareos GmbH & Co. KG

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation, which is
   listed in the file LICENSE.

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
#include "lib/bsnprintf.h"
#include <string.h>

// #define Bsnprintf snprintf

TEST(bsnprintf, truncate)
{
  char dest[100];

  EXPECT_EQ(Bsnprintf(dest, 100, "Some String"), 11);
  EXPECT_STREQ(dest, "Some String");

  EXPECT_EQ(Bsnprintf(dest, 10, "Some String"), 10);
  EXPECT_STREQ(dest, "Some Stri");
}

TEST(bsnprintf, vararg)
{
  char dest[100];

  EXPECT_EQ(Bsnprintf(dest, 100, "%s", "Some String"), 11);
  EXPECT_STREQ(dest, "Some String");

  EXPECT_EQ(Bsnprintf(dest, 100, "%s %s", "Some", "String"), 11);
  EXPECT_STREQ(dest, "Some String");
}

TEST(bsnprintf, len_param)
{
  char dest[100];

  // align the string by value
  EXPECT_EQ(Bsnprintf(dest, 100, "-%20s-", "Some String"), 22);
  EXPECT_STREQ(dest, "-         Some String-");

  // align the string by value
  EXPECT_EQ(Bsnprintf(dest, 100, "-%-20s-", "Some String"), 22);
  EXPECT_STREQ(dest, "-Some String         -");

  // truncate the string by value
  EXPECT_EQ(Bsnprintf(dest, 100, "-%.5s-", "Some String"), 7);
  EXPECT_STREQ(dest, "-Some -");

  // align the string by parameter
  EXPECT_EQ(Bsnprintf(dest, 100, "-%*s-", 20, "Some String"), 22);
  EXPECT_STREQ(dest, "-         Some String-");

  // truncate the string by parameter
  EXPECT_EQ(Bsnprintf(dest, 100, "-%.*s-", 5, "Some String"), 7);
  EXPECT_STREQ(dest, "-Some -");

  // fixed length string value with no null-terminator
  const char fixed_str[] = {'a', 'b', 'c', 'd', 'e', 'f'};
  EXPECT_EQ(Bsnprintf(dest, 100, "-%.*s-", 6, fixed_str), 8);
  EXPECT_STREQ(dest, "-abcdef-");
}

TEST(bsnprintf, char)
{
  char dest[100];
  char a = 'A';
  char b = 'b';

  EXPECT_EQ(Bsnprintf(dest, 100, "%c %c", a, b), 3);
  EXPECT_STREQ(dest, "A b");

  // literal %
  EXPECT_EQ(Bsnprintf(dest, 100, "%%"), 1);
  EXPECT_STREQ(dest, "%");
}

TEST(bsnprintf, integers)
{
  char dest[100];
  int pos = 123;
  int neg = -321;

  // unaligned
  EXPECT_EQ(Bsnprintf(dest, 100, "%i %i", pos, neg), 8);
  EXPECT_STREQ(dest, "123 -321");

  // unaligned with space for sign
  EXPECT_EQ(Bsnprintf(dest, 100, "% i % i", pos, neg), 9);
  EXPECT_STREQ(dest, " 123 -321");

  // right aligned to 5 chars with sign
  EXPECT_EQ(Bsnprintf(dest, 100, "%+5i %+5i", pos, neg), 11);
  EXPECT_STREQ(dest, " +123  -321");

  // left aligned to 5 chars with sign
  EXPECT_EQ(Bsnprintf(dest, 100, "%-5i %-5i", pos, neg), 11);
  EXPECT_STREQ(dest, "123   -321 ");

  // aligned to 5 chars with zeros and sign
  EXPECT_EQ(Bsnprintf(dest, 100, "%+05i %+05i", pos, neg), 11);
  EXPECT_STREQ(dest, "+0123 -0321");

  // unaligned with zerofill
  EXPECT_EQ(Bsnprintf(dest, 100, "%05i %05i", pos, neg), 11);
  EXPECT_STREQ(dest, "00123 -0321");

  short sint = -123;
  EXPECT_EQ(Bsnprintf(dest, 100, "%hi", sint), 4);
  EXPECT_STREQ(dest, "-123");

  long lint = -123;
  EXPECT_EQ(Bsnprintf(dest, 100, "%li", lint), 4);
  EXPECT_STREQ(dest, "-123");

  long long llint = -123;
  EXPECT_EQ(Bsnprintf(dest, 100, "%lli", llint), 4);
  EXPECT_STREQ(dest, "-123");

  ssize_t ss_int = -123;
  EXPECT_EQ(Bsnprintf(dest, 100, "%" PRIiz, ss_int), 4);
  EXPECT_STREQ(dest, "-123");

  unsigned int uns_int = 123;
  EXPECT_EQ(Bsnprintf(dest, 100, "%u", uns_int), 3);
  EXPECT_STREQ(dest, "123");

  unsigned short usint = 123;
  EXPECT_EQ(Bsnprintf(dest, 100, "%hu", usint), 3);
  EXPECT_STREQ(dest, "123");

  unsigned long ulint = 123;
  EXPECT_EQ(Bsnprintf(dest, 100, "%lu", ulint), 3);
  EXPECT_STREQ(dest, "123");

  unsigned long long ullint = 123;
  EXPECT_EQ(Bsnprintf(dest, 100, "%llu", ullint), 3);
  EXPECT_STREQ(dest, "123");

  size_t s_int = 123;
  EXPECT_EQ(Bsnprintf(dest, 100, "%" PRIuz, s_int), 3);
  EXPECT_STREQ(dest, "123");

  EXPECT_EQ(Bsnprintf(dest, 100, "%o", 8), 2);
  EXPECT_STREQ(dest, "10");

  EXPECT_EQ(Bsnprintf(dest, 100, "%x", 255), 2);
  EXPECT_STREQ(dest, "ff");

  EXPECT_EQ(Bsnprintf(dest, 100, "%X", 255), 2);
  EXPECT_STREQ(dest, "FF");
}

template <typename T> struct specifier;

template <typename T> constexpr auto specifier_v = specifier<T>::value;

template <> struct specifier<std::uint64_t> {
  static constexpr const char* value = "%" PRIu64;
};
template <> struct specifier<std::int64_t> {
  static constexpr const char* value = "%" PRIi64;
};
template <> struct specifier<std::uint32_t> {
  static constexpr const char* value = "%" PRIu32;
};
template <> struct specifier<std::int32_t> {
  static constexpr const char* value = "%" PRIi32;
};
template <> struct specifier<std::uint16_t> {
  static constexpr const char* value = "%" PRIu16;
};
template <> struct specifier<std::int16_t> {
  static constexpr const char* value = "%" PRIi16;
};
template <> struct specifier<std::uint8_t> {
  static constexpr const char* value = "%" PRIu8;
};
template <> struct specifier<std::int8_t> {
  static constexpr const char* value = "%" PRIi8;
};

#include <charconv>

template <typename T> void test_format(T value)
{
  char buffer[100];

  auto res = std::to_chars(std::begin(buffer), std::end(buffer), value);

  ASSERT_EQ(res.ec, std::errc());
  ASSERT_NE(res.ptr, std::end(buffer));
  *res.ptr = '\0';

  PoolMem formatted;
  auto size = formatted.bsprintf(specifier_v<T>, value);
  ASSERT_GE(size, 0);
  ASSERT_LT(size, sizeof(buffer));

  EXPECT_STREQ(buffer, formatted.c_str());
}

template <typename T> void test_limits()
{
  test_format(std::numeric_limits<T>::min());
  test_format(std::numeric_limits<T>::max());
}

TEST(bsnprintf, integer_limits)
{
  test_limits<std::uint64_t>();
  test_limits<std::int64_t>();
  test_limits<std::uint32_t>();
  test_limits<std::int32_t>();
  test_limits<std::uint16_t>();
  test_limits<std::int16_t>();
  test_limits<std::uint8_t>();
  test_limits<std::int8_t>();
}

static std::string repeat(char c, std::size_t count)
{
  std::string s;

  s.resize(count);
  for (char& current : s) { current = c; }

  return s;
}


static void test_buffer(char* buffer, size_t size, const std::string& s)
{
  size_t expected_result = std::min(size, s.size());

  EXPECT_EQ(Bsnprintf(buffer, size, "%s", s.c_str()), expected_result);

  size_t expected_size = std::min(size - 1, s.size());

  EXPECT_EQ(std::string_view(buffer, expected_size),
            std::string_view(s.c_str(), expected_size));

  EXPECT_EQ(buffer[expected_size], '\0');
}

TEST(bsnprintf, memory_limits)
{
  EXPECT_EQ(Bsnprintf(nullptr, -1, "%s", "Hello, World!"), -1);
  EXPECT_EQ(Bsnprintf(nullptr, 0, "%s", "Hello, World!"), 0);


  char buffer[100];

  {
    memset(buffer, 'a', sizeof(buffer));
    test_buffer(buffer, 1, repeat('b', 9).c_str());
  }
  {
    memset(buffer, 'a', sizeof(buffer));
    test_buffer(buffer, 10, repeat('b', 9).c_str());
  }
  {
    memset(buffer, 'a', sizeof(buffer));
    test_buffer(buffer, 10, repeat('b', 10).c_str());
  }
  {
    memset(buffer, 'a', sizeof(buffer));
    test_buffer(buffer, 100, repeat('b', 10).c_str());
  }
  {
    memset(buffer, 'a', sizeof(buffer));
    test_buffer(buffer, 100, repeat('b', 100).c_str());
  }
}
