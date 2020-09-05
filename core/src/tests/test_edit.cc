/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2018-2020 Bareos GmbH & Co. KG

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

// kibibyte
TEST(edit, k)
{
  char str[] = "1 k";
  uint64_t retvalue = 0;
  size_to_uint64(str, &retvalue);
  ASSERT_EQ(retvalue, 1024);
}


// kilobyte
TEST(edit, kb)
{
  char str[] = "1 kb";
  uint64_t retvalue = 0;
  size_to_uint64(str, &retvalue);
  ASSERT_EQ(retvalue, 1000);
}
TEST(edit, kB)
{
  char str[] = "1 KB";
  uint64_t retvalue = 0;
  size_to_uint64(str, &retvalue);
  ASSERT_EQ(retvalue, 1000);
}

// mebibyte
TEST(edit, m)
{
  char str[] = "1 m";
  uint64_t retvalue = 0;
  size_to_uint64(str, &retvalue);
  ASSERT_EQ(retvalue, 1024 * 1024);
}

// megabyte
TEST(edit, mb)
{
  char str[] = "1 mb";
  uint64_t retvalue = 0;
  size_to_uint64(str, &retvalue);
  ASSERT_EQ(retvalue, 1000 * 1000);
}


// gibibyte
TEST(edit, g)
{
  char str[] = "1 g";
  uint64_t retvalue = 0;
  size_to_uint64(str, &retvalue);
  ASSERT_EQ(retvalue, 1024 * 1024 * 1024);
}
// gigabyte
TEST(edit, gb)
{
  char str[] = "1 gb";
  uint64_t retvalue = 0;
  size_to_uint64(str, &retvalue);
  ASSERT_EQ(retvalue, 1000 * 1000 * 1000);
}


// tebibyte
TEST(edit, t)
{
  char str[] = "1 t";
  uint64_t retvalue = 0;
  size_to_uint64(str, &retvalue);
  ASSERT_EQ(retvalue, 1099511627776);
}
// terabyte
TEST(edit, tb)
{
  char str[] = "1 tb";
  uint64_t retvalue = 0;
  size_to_uint64(str, &retvalue);
  ASSERT_EQ(retvalue, 1000000000000);
}

// pebibyte
TEST(edit, p)
{
  char str[] = "1 p";
  uint64_t retvalue = 0;
  size_to_uint64(str, &retvalue);
  ASSERT_EQ(retvalue, 1125899906842624);
}
// petabyte
TEST(edit, pb)
{
  char str[] = "1 pb";
  uint64_t retvalue = 0;
  size_to_uint64(str, &retvalue);
  ASSERT_EQ(retvalue, 1000000000000000);
}

// exbibyte
TEST(edit, e)
{
  char str[] = "1 e";
  uint64_t retvalue = 0;
  size_to_uint64(str, &retvalue);
  ASSERT_EQ(retvalue, 1152921504606846976);
}
// exabyte
TEST(edit, eb)
{
  char str[] = "1 eb";
  uint64_t retvalue = 0;
  size_to_uint64(str, &retvalue);
  ASSERT_EQ(retvalue, 1000000000000000000);
}
