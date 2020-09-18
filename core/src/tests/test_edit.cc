/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2020-2020 Bareos GmbH & Co. KG

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
#include "include/bareos.h"
#include "gtest/gtest.h"
#else
#include "gtest/gtest.h"
#include "include/bareos.h"
#endif

#include "lib/edit.h"

TEST(edit, convert_number_to_siunits)
{
  ASSERT_STREQ(SizeAsSiPrefixFormat(1).c_str(), "1");
  ASSERT_STREQ(SizeAsSiPrefixFormat(1024).c_str(), "1 k");
  ASSERT_STREQ(SizeAsSiPrefixFormat(1048576).c_str(), "1 m");
  ASSERT_STREQ(SizeAsSiPrefixFormat(1073741824).c_str(), "1 g");
  ASSERT_STREQ(SizeAsSiPrefixFormat(1099511627776).c_str(), "1 t");
  ASSERT_STREQ(SizeAsSiPrefixFormat(1125899906842624).c_str(), "1 p");
  ASSERT_STREQ(SizeAsSiPrefixFormat(1152921504606846976).c_str(), "1 e");

  ASSERT_STREQ(
      SizeAsSiPrefixFormat(1152921504606846976 + 1125899906842624 +
                           1099511627776 + 1073741824 + 1048576 + 1024 + 1)
          .c_str(),
      "1 e 1 p 1 t 1 g 1 m 1 k 1");
}

// kibibyte
TEST(edit, convert_siunits_to_numbers)
{
  {
    char str[] = "1 k";
    uint64_t retvalue = 0;
    size_to_uint64(str, &retvalue);
    ASSERT_EQ(retvalue, 1024);
  }


  // kilobyte
  {
    char str[] = "1 kb";
    uint64_t retvalue = 0;
    size_to_uint64(str, &retvalue);
    ASSERT_EQ(retvalue, 1000);
  }
  {
    char str[] = "1 KB";
    uint64_t retvalue = 0;
    size_to_uint64(str, &retvalue);
    ASSERT_EQ(retvalue, 1000);
  }

  // mebibyte
  {
    char str[] = "1 m";
    uint64_t retvalue = 0;
    size_to_uint64(str, &retvalue);
    ASSERT_EQ(retvalue, 1024 * 1024);
  }

  // megabyte
  {
    char str[] = "1 mb";
    uint64_t retvalue = 0;
    size_to_uint64(str, &retvalue);
    ASSERT_EQ(retvalue, 1000 * 1000);
  }


  // gibibyte
  {
    char str[] = "1 g";
    uint64_t retvalue = 0;
    size_to_uint64(str, &retvalue);
    ASSERT_EQ(retvalue, 1024 * 1024 * 1024);
  }
  // gigabyte
  {
    char str[] = "1 gb";
    uint64_t retvalue = 0;
    size_to_uint64(str, &retvalue);
    ASSERT_EQ(retvalue, 1000 * 1000 * 1000);
  }


  // tebibyte
  {
    char str[] = "1 t";
    uint64_t retvalue = 0;
    size_to_uint64(str, &retvalue);
    ASSERT_EQ(retvalue, 1099511627776);
  }
  // terabyte
  {
    char str[] = "1 tb";
    uint64_t retvalue = 0;
    size_to_uint64(str, &retvalue);
    ASSERT_EQ(retvalue, 1000000000000);
  }

  // pebibyte
  {
    char str[] = "1 p";
    uint64_t retvalue = 0;
    size_to_uint64(str, &retvalue);
    ASSERT_EQ(retvalue, 1125899906842624);
  }
  // petabyte
  {
    char str[] = "1 pb";
    uint64_t retvalue = 0;
    size_to_uint64(str, &retvalue);
    ASSERT_EQ(retvalue, 1000000000000000);
  }

  // exbibyte
  {
    char str[] = "1 e";
    uint64_t retvalue = 0;
    size_to_uint64(str, &retvalue);
    ASSERT_EQ(retvalue, 1152921504606846976);
  }
  // exabyte
  {
    char str[] = "1 eb";
    uint64_t retvalue = 0;
    size_to_uint64(str, &retvalue);
    ASSERT_EQ(retvalue, 1000000000000000000);
  }
  // size_to_uint64 only checks for first modifier so the following does not
  // work
  /*
    {
      char str[] = "1 e 1 p 1 t 1 g 1 m 1 k 1";
      uint64_t retvalue = 0;
      size_to_uint64(str, &retvalue);
      ASSERT_EQ(retvalue, 1152921504606846976 + 1125899906842624 + 1099511627776
    + 1073741824 + 1048576 + 1024 + 1);
    }
  */
}
