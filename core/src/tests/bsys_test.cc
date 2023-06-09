/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2021-2021 Bareos GmbH & Co. KG

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

#include "lib/bsys.h"

TEST(bstrncpy, src_NULL)
{
  char dest[] = "DESTDESTDEST";
  char* src = NULL;

  bstrncpy(dest, src, 10);

  EXPECT_STREQ(dest, "");
}

TEST(bstrncpy, src_empty)
{
  char dest[] = "DESTDESTDEST";
  char src[] = "";

  bstrncpy(dest, src, 10);

  EXPECT_STREQ(dest, "");
}

TEST(bstrncpy, negative_maxlen)
{
  char dest[] = "DESTDESTDEST";
  char src[] = "SRCSRC";

  bstrncpy(dest, src, -99);

  EXPECT_STREQ(dest, "");
}

TEST(bstrncpy, maxlen0)
{
  char dest[] = "DESTDESTDEST";
  char src[] = "SRCSRC";

  bstrncpy(dest, src, 0);

  EXPECT_STREQ(dest, "");
}

TEST(bstrncpy, maxlen1)
{
  char dest[] = "DESTDESTDEST";
  char src[] = "SRCSRC";

  bstrncpy(dest, src, 1);

  EXPECT_STREQ(dest, "");
}

TEST(bstrncpy, maxlen2)
{
  char dest[] = "DESTDESTDEST";
  char src[] = "SRCSRC";

  bstrncpy(dest, src, 2);

  EXPECT_STREQ(dest, "S");
}

TEST(bstrncpy, maxlen3)
{
  char dest[] = "DESTDESTDEST";
  char src[] = "SRCSRC";

  bstrncpy(dest, src, 3);

  EXPECT_STREQ(dest, "SR");
}

TEST(bstrncpy, maxlen_strlen)
{
  char dest[] = "DESTDESTDEST";
  char src[] = "SRCSRC";

  bstrncpy(dest, src, strlen(src));

  EXPECT_STREQ(dest, "SRCSR");
}

TEST(bstrncpy, maxlen_strlen1)
{
  char dest[] = "DESTDESTDEST";
  char src[] = "SRCSRC";

  bstrncpy(dest, src, strlen(src) + 1);

  EXPECT_STREQ(dest, src);
}

TEST(bstrncpy, maxlen_longer_than_src)
{
  char dest[] = "DESTDESTDEST";
  char src[] = "SRCSRC";

  bstrncpy(dest, src, 10);

  EXPECT_STREQ(dest, src);
}

TEST(bstrncpy, overlap4)
{
  char dest[] = "d1d2d3d4d5d6d7d8d9";
  char* src = &dest[4];

  bstrncpy(dest, src, 5);

  EXPECT_STREQ(dest, "d3d4");
}

TEST(bstrncpy, overlap_remaining)
{
  const int start = 4;
  char dest[] = "d1d2d3d4d5d6d7d8d9";
  char* src = &dest[start];

  bstrncpy(dest, src, strlen(dest) + 1 - start);

  EXPECT_STREQ(dest, "d3d4d5d6d7d8d9");
}

TEST(bstrncpy, overlap_maxlen_longer_than_src)
{
  char dest[] = "d1d2d3d4d5d6d7d8d9";
  char* src = &dest[4];

  bstrncpy(dest, src, strlen(dest));

  EXPECT_STREQ(dest, "d3d4d5d6d7d8d9");
}

TEST(bstrncpy, src_without_terminating_null_byte)
{
  char dest[] = "DESTDESTDEST";
  char src[] = "SRCSRC";
  int src_len = strlen(src);
  src[src_len - 1] = 'X';
  src[src_len] = 'Y';

  bstrncpy(dest, src, src_len + 2);

  EXPECT_STREQ(dest, "SRCSRXY");
}

TEST(bstrncat, src_NULL)
{
  char dest[100] = "<OLD>";
  char* src = NULL;

  bstrncat(dest, src, 10);

  EXPECT_STREQ(dest, "<OLD>");
}

TEST(bstrncat, src_empty)
{
  char dest[100] = "<OLD>";
  char src[] = "";

  bstrncat(dest, src, 10);

  EXPECT_STREQ(dest, "<OLD>");
}

TEST(bstrncat, negative_maxlen)
{
  char dest[100] = "<OLD>";
  char src[] = "<NEW>";

  bstrncat(dest, src, -99);

  EXPECT_STREQ(dest, "<OLD>");
}

TEST(bstrncat, maxlen0)
{
  char dest[100] = "<OLD>";
  char src[] = "<NEW>";

  bstrncat(dest, src, 0);

  EXPECT_STREQ(dest, "<OLD>");
}

TEST(bstrncat, maxlen1)
{
  char dest[100] = "<OLD>";
  char src[] = "<NEW>";

  bstrncat(dest, src, 1);

  EXPECT_STREQ(dest, "<OLD>");
}

TEST(bstrncat, maxlen2)
{
  char dest[100] = "<OLD>";
  char src[] = "<NEW>";

  bstrncat(dest, src, 2);

  EXPECT_STREQ(dest, "<OLD>");
}

TEST(bstrncat, maxlen3)
{
  char dest[100] = "<OLD>";
  char src[] = "<NEW>";

  bstrncat(dest, src, 2);

  EXPECT_STREQ(dest, "<OLD>");
}

TEST(bstrncat, maxlen_strlen)
{
  char dest[100] = "<OLD>";
  char src[] = "<NEW>";

  bstrncat(dest, src, strlen(dest) + strlen(src));

  EXPECT_STREQ(dest, "<OLD><NEW");
}

TEST(bstrncat, maxlen_strlen1)
{
  char dest[100] = "<OLD>";
  char src[] = "<NEW>";

  bstrncat(dest, src, strlen(dest) + strlen(src) + 1);

  EXPECT_STREQ(dest, "<OLD><NEW>");
}

TEST(bstrncat, maxlen_longer_than_result)
{
  char dest[100] = "<OLD>";
  char src[] = "<NEW>";

  bstrncat(dest, src, 100);

  EXPECT_STREQ(dest, "<OLD><NEW>");
}

TEST(bstrncat, overlap_maxlen_longer_than_result)
{
  char dest[100] = "<OLD>";
  char* src = &dest[3];

  bstrncat(dest, src, 100);

  EXPECT_STREQ(dest, "<OLD>D>");
}

TEST(bstrncat, overlap_terminating_null_byte)
{
  char dest[100] = "<OLD>";
  char* src = &dest[5];

  bstrncat(dest, src, 100);

  EXPECT_STREQ(dest, "<OLD>");
}

TEST(escapefilename, regularfilename)
{
  std::string filename = "thisisaregularfilename";
  std::string escaped_filename = escape_filename(filename.c_str());

  EXPECT_STREQ(escaped_filename.c_str(), "");

  filename = R"(this\is\aregularfilename)";
  escaped_filename = escape_filename(filename.c_str());

  EXPECT_STREQ(escaped_filename.c_str(), R"(this\\is\\aregularfilename)");

  filename = R"(this"isaregularfile"name)";
  escaped_filename = escape_filename(filename.c_str());

  EXPECT_STREQ(escaped_filename.c_str(), R"(this\"isaregularfile\"name)");

  filename = R"(this"is\aregular\file"name)";
  escaped_filename = escape_filename(filename.c_str());

  EXPECT_STREQ(escaped_filename.c_str(), R"(this\"is\\aregular\\file\"name)");
}
