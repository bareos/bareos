/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2020-2024 Bareos GmbH & Co. KG

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

#include "lib/edit.h"

constexpr std::uint64_t kibi = 1024;
constexpr std::uint64_t mebi = 1024 * kibi;
constexpr std::uint64_t gibi = 1024 * mebi;
constexpr std::uint64_t tebi = 1024 * gibi;
constexpr std::uint64_t pebi = 1024 * tebi;
constexpr std::uint64_t exi = 1024 * pebi;

constexpr std::uint64_t kilo = 1000;
constexpr std::uint64_t mega = 1000 * kilo;
constexpr std::uint64_t giga = 1000 * mega;
constexpr std::uint64_t tera = 1000 * giga;
constexpr std::uint64_t peta = 1000 * tera;
constexpr std::uint64_t exa = 1000 * peta;

TEST(edit, convert_number_to_siunits)
{
  ASSERT_STREQ(SizeAsSiPrefixFormat(0).c_str(), "0");
  ASSERT_STREQ(SizeAsSiPrefixFormat(1).c_str(), "1");
  ASSERT_STREQ(SizeAsSiPrefixFormat(123).c_str(), "123");
  ASSERT_STREQ(SizeAsSiPrefixFormat(kibi).c_str(), "1 k");
  ASSERT_STREQ(SizeAsSiPrefixFormat(2 * kibi + 2).c_str(), "2 k 2");
  ASSERT_STREQ(SizeAsSiPrefixFormat(mebi).c_str(), "1 m");
  ASSERT_STREQ(SizeAsSiPrefixFormat(gibi).c_str(), "1 g");
  ASSERT_STREQ(SizeAsSiPrefixFormat(tebi).c_str(), "1 t");
  ASSERT_STREQ(SizeAsSiPrefixFormat(pebi).c_str(), "1 p");
  ASSERT_STREQ(SizeAsSiPrefixFormat(exi).c_str(), "1 e");
  ASSERT_STREQ(
      SizeAsSiPrefixFormat(exi + pebi + tebi + gibi + mebi + kibi + 1).c_str(),
      "1 e 1 p 1 t 1 g 1 m 1 k 1");
}

// kibibyte
TEST(edit, convert_siunits_to_numbers)
{
  {
    const char* str = "";
    uint64_t retvalue = 0;
    ASSERT_TRUE(size_to_uint64(str, &retvalue));
    ASSERT_EQ(retvalue, 0);
  }

  {
    const char* str = "1";
    uint64_t retvalue = 0;
    ASSERT_TRUE(size_to_uint64(str, &retvalue));
    ASSERT_EQ(retvalue, 1);
  }

  {
    const char* str = "1 k";
    uint64_t retvalue = 0;
    ASSERT_TRUE(size_to_uint64(str, &retvalue));
    ASSERT_EQ(retvalue, kibi);
  }


  // kilobyte
  {
    const char* str = "1 kb";
    uint64_t retvalue = 0;
    ASSERT_TRUE(size_to_uint64(str, &retvalue));
    ASSERT_EQ(retvalue, kilo);
  }
  {
    const char* str = "1 KB";
    uint64_t retvalue = 0;
    ASSERT_TRUE(size_to_uint64(str, &retvalue));
    ASSERT_EQ(retvalue, kilo);
  }

  // mebibyte
  {
    const char* str = "1 m";
    uint64_t retvalue = 0;
    ASSERT_TRUE(size_to_uint64(str, &retvalue));
    ASSERT_EQ(retvalue, mebi);
  }

  // megabyte
  {
    const char* str = "1 mb";
    uint64_t retvalue = 0;
    ASSERT_TRUE(size_to_uint64(str, &retvalue));
    ASSERT_EQ(retvalue, mega);
  }


  // gibibyte
  {
    const char* str = "1 g";
    uint64_t retvalue = 0;
    ASSERT_TRUE(size_to_uint64(str, &retvalue));
    ASSERT_EQ(retvalue, gibi);
  }
  // gigabyte
  {
    const char* str = "1 gb";
    uint64_t retvalue = 0;
    ASSERT_TRUE(size_to_uint64(str, &retvalue));
    ASSERT_EQ(retvalue, giga);
  }


  // tebibyte
  {
    const char* str = "1 t";
    uint64_t retvalue = 0;
    ASSERT_TRUE(size_to_uint64(str, &retvalue));
    ASSERT_EQ(retvalue, tebi);
  }
  // terabyte
  {
    const char* str = "1 tb";
    uint64_t retvalue = 0;
    ASSERT_TRUE(size_to_uint64(str, &retvalue));
    ASSERT_EQ(retvalue, tera);
  }

  // pebibyte
  {
    const char* str = "1 p";
    uint64_t retvalue = 0;
    ASSERT_TRUE(size_to_uint64(str, &retvalue));
    ASSERT_EQ(retvalue, pebi);
  }
  // petabyte
  {
    const char* str = "1 pb";
    uint64_t retvalue = 0;
    ASSERT_TRUE(size_to_uint64(str, &retvalue));
    ASSERT_EQ(retvalue, peta);
  }

  // exbibyte
  {
    const char* str = "1 e";
    uint64_t retvalue = 0;
    ASSERT_TRUE(size_to_uint64(str, &retvalue));
    ASSERT_EQ(retvalue, exi);
  }
  // exabyte
  {
    const char* str = "1 eb";
    uint64_t retvalue = 0;
    ASSERT_TRUE(size_to_uint64(str, &retvalue));
    ASSERT_EQ(retvalue, exa);
  }
  // combined specification
  {
    const char* str = "1 e 1 p 1 t 1 g 1 m 1 k 1";
    uint64_t retvalue = 0;
    ASSERT_TRUE(size_to_uint64(str, &retvalue));
    ASSERT_EQ(retvalue, exi + pebi + tebi + gibi + mebi + kibi + 1);
  }
}

TEST(edit, check_bad_parse)
{
  {
    const char* str = "ei";
    uint64_t retvalue = 0;
    ASSERT_FALSE(size_to_uint64(str, &retvalue));
  }
  {
    const char* str = "-";
    uint64_t retvalue = 0;
    ASSERT_FALSE(size_to_uint64(str, &retvalue));
  }
  {
    const char* str = "+";
    uint64_t retvalue = 0;
    ASSERT_FALSE(size_to_uint64(str, &retvalue));
  }
  {
    const char* str = "M";
    uint64_t retvalue = 0;
    ASSERT_FALSE(size_to_uint64(str, &retvalue));
  }
}
