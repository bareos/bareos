/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2019-2024 Bareos GmbH & Co. KG

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

  // unsupported w
  EXPECT_EQ(Bsnprintf(dest, 100, "%w%xyz"), 3);
  EXPECT_STREQ(dest, "xyz");

  // invalid ! (ignored)
  EXPECT_EQ(Bsnprintf(dest, 100, "%!xyz"), 3);
  EXPECT_STREQ(dest, "xyz");
}

TEST(bsnprintf, pointer)
{
  char dest[100];
  void* null = nullptr;
  void* ones = reinterpret_cast<void*>(UINTPTR_MAX);

  EXPECT_EQ(Bsnprintf(dest, 100, "%p", null), 1);
  EXPECT_STREQ(dest, "0");
  if constexpr (sizeof(void*) == 4) {
    EXPECT_EQ(Bsnprintf(dest, 100, "%p", ones), 8);
    EXPECT_STREQ(dest, "ffffffff");
  } else {
    EXPECT_EQ(Bsnprintf(dest, 100, "%p", ones), 16);
    EXPECT_STREQ(dest, "ffffffffffffffff");
  }
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

  EXPECT_EQ(Bsnprintf(dest, 100, "%qi", llint), 4);
  EXPECT_STREQ(dest, "-123");

  ssize_t ss_int = -123;
  EXPECT_EQ(Bsnprintf(dest, 100, "%zi", ss_int), 4);
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

  EXPECT_EQ(Bsnprintf(dest, 100, "%qu", ullint), 3);
  EXPECT_STREQ(dest, "123");

  size_t s_int = 123;
  EXPECT_EQ(Bsnprintf(dest, 100, "%zu", s_int), 3);
  EXPECT_STREQ(dest, "123");

  EXPECT_EQ(Bsnprintf(dest, 100, "%o", 8), 2);
  EXPECT_STREQ(dest, "10");

  EXPECT_EQ(Bsnprintf(dest, 100, "%x", 255), 2);
  EXPECT_STREQ(dest, "ff");

  EXPECT_EQ(Bsnprintf(dest, 100, "%X", 255), 2);
  EXPECT_STREQ(dest, "FF");
}
