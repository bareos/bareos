/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2007-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2019-2021 Bareos GmbH & Co. KG

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

#include "dird/bsr.h"
#include "dird/ua_restore.h"
#include "lib/btime.h"

using namespace directordaemon;

namespace directordaemon {
bool DoReloadConfig() { return false; }
}  // namespace directordaemon

TEST(time_format, correct_time_and_date_format)
{
  // Correct dates
  EXPECT_TRUE(StrToUtime("1999-01-02 22:22:22") != 0);
  EXPECT_TRUE(StrToUtime("2020-11-22 1:5:7") != 0);
  EXPECT_TRUE(StrToUtime("1999-01-02 22:22:") != 0);

  // Correct format, but wrong date
  EXPECT_EQ(StrToUtime("199-01-02 22:22:22"), 0);
  EXPECT_EQ(StrToUtime("1999-13-02 22:22:22"), 0);
  EXPECT_EQ(StrToUtime("1999-01-35 22:22:22"), 0);
  EXPECT_EQ(StrToUtime("1999-01-02 50:22:22"), 0);
  EXPECT_EQ(StrToUtime("1999-01-02 22:90:22"), 0);
  EXPECT_EQ(StrToUtime("1999-31-52 22:22:65"), 0);
  EXPECT_EQ(StrToUtime("1999-31-52 50:72:22"), 0);

  EXPECT_EQ(StrToUtime("199--1--2 22:22:22"), 0);
  EXPECT_EQ(StrToUtime("1999-01-02 22:22:22:10"), 0);


  // leap years
  EXPECT_TRUE(StrToUtime("2000-02-29 10:10:10") != 0);
  EXPECT_TRUE(StrToUtime("2020-02-29 10:10:10") != 0);
  EXPECT_TRUE(StrToUtime("2001-02-29 10:10:10") == 0);
  EXPECT_TRUE(StrToUtime("1900-02-29 10:10:10") == 0);

  // Correct format, but missing entries
  EXPECT_EQ(StrToUtime("1999-01-02 22:"), 0);
  EXPECT_EQ(StrToUtime("1999-01-02"), 0);
  EXPECT_EQ(StrToUtime("1999-"), 0);

  // Wrong format but correct date
  EXPECT_EQ(StrToUtime("1999/01/02 22:22:22"), 0);
  EXPECT_EQ(StrToUtime("01-02-1999 22:22:22"), 0);
  EXPECT_EQ(StrToUtime("1999/01/02 22:22:22"), 0);

  // Random stuff
  EXPECT_EQ(StrToUtime("d-at-ea nd:ti:me"), 0);
  EXPECT_EQ(StrToUtime("-- ::"), 0);
  EXPECT_EQ(StrToUtime(" - -   : : "), 0);
  EXPECT_EQ(StrToUtime("1999-5-----"), 0);

  EXPECT_TRUE(
      StrToUtime("1999-01-02 "
                 "22:22:22adfddfkjlsdjklf;asfdkslfkjasflsdfdkslfjdsfklds;"
                 "lfkjdskkjajkvnkashuiadfvmnknkajnsfkljdnafkljdnafklja")
      == 0);
}

TEST(time_format, short_formats_compensation)
{
  char const* short_date;
  char const* correct_full_format_date;

  // Correct long format
  correct_full_format_date = "1999-05-06 10:10:10";
  EXPECT_EQ(CompensateShortDate(correct_full_format_date),
            correct_full_format_date);

  // Correct short format
  short_date = "1999";
  EXPECT_EQ(CompensateShortDate(short_date), "1999-01-01 00:00:00");

  short_date = "1999-05";
  EXPECT_EQ(CompensateShortDate(short_date), "1999-05-01 00:00:00");

  short_date = "1999-05-";
  EXPECT_EQ(CompensateShortDate(short_date), "1999-05-01 00:00:00");

  short_date = "1999-05-06";
  EXPECT_EQ(CompensateShortDate(short_date), "1999-05-06 00:00:00");

  short_date = "1999-05-31";
  EXPECT_EQ(CompensateShortDate(short_date), "1999-05-31 00:00:00");

  short_date = "1999-05-06 10";
  EXPECT_EQ(CompensateShortDate(short_date), "1999-05-06 10:00:00");

  short_date = "1999-05-06 10:";
  EXPECT_EQ(CompensateShortDate(short_date), "1999-05-06 10:00:00");

  short_date = "1999-05-06 10:10";
  EXPECT_EQ(CompensateShortDate(short_date), "1999-05-06 10:10:00");

  // Random stuff

  short_date = "1999-- ::";
  EXPECT_EQ(CompensateShortDate(short_date), "1999-01-01 00:00:00");

  short_date = "1999--- ::";
  EXPECT_EQ(CompensateShortDate(short_date), short_date);

  short_date = "1999-05- :10:";
  EXPECT_EQ(CompensateShortDate(short_date), "1999-05-01 00:10:00");

  short_date = "1999--06 10:10:";
  EXPECT_EQ(CompensateShortDate(short_date), "1999-01-06 10:10:00");

  short_date = "1999-05-06 10:10:10:10-56";
  EXPECT_EQ(CompensateShortDate(short_date), short_date);

  short_date = "1999asfasdf";
  EXPECT_EQ(CompensateShortDate(short_date), short_date);

  short_date = "1999-05---";
  EXPECT_EQ(CompensateShortDate(short_date), short_date);

  short_date = "hey-05";
  EXPECT_EQ(CompensateShortDate(short_date), short_date);

  short_date = "1999-dd-dd ::";
  EXPECT_EQ(CompensateShortDate(short_date), short_date);

  short_date = "";
  EXPECT_EQ(CompensateShortDate(short_date), short_date);

  short_date = "19999999999999999999999999999999999999999-5-06 10:10:10:10";
  EXPECT_EQ(CompensateShortDate(short_date), short_date);

  short_date
      = "1999-01-02 "
        "22:22:22adfddfkjlsdjklf;asfdkslfkjasflsdfdkslfjdsfklds;"
        "lfkjdskkjajkvnkashuiadfvmnknkajnsfkljdnafkljdnafklja";
  EXPECT_EQ(CompensateShortDate(short_date), short_date);
}
