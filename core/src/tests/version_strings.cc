/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2019-2019 Bareos GmbH & Co. KG

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
#include <algorithm>
#include <vector>

#if defined(HAVE_MINGW)
#define _S_IFDIR S_IFDIR
#define _stat stat
#include "minwindef.h"
#include "include/bareos.h"
#include "gtest/gtest.h"
#else
#include "gtest/gtest.h"
#endif

#include "lib/version.h"

TEST(version_strings, version)
{
  std::string ver_full{kBareosVersionStrings.Full};

  // the full version is not a nullstring
  EXPECT_GT(strlen(kBareosVersionStrings.Full), 0);

  // the full version contains at least two dots (.)
  EXPECT_GE(std::count(ver_full.begin(), ver_full.end(), '.'), 2);

  // the version starts with a digit
  EXPECT_TRUE(isdigit(kBareosVersionStrings.Full[0]));

  // version + date is set correctly
  std::string full_with_date{kBareosVersionStrings.FullWithDate};
  std::string ref_full_with_date =
      ver_full + " (" + kBareosVersionStrings.Date + ")";
  EXPECT_EQ(full_with_date, ref_full_with_date);
}

TEST(version_strings, dates)
{
  const size_t date_len{strlen(kBareosVersionStrings.Date)};
  const std::string date{kBareosVersionStrings.Date};
  const std::string short_date{kBareosVersionStrings.ShortDate};

  // shortest possible is a date in May like "01 May 2019"
  EXPECT_GT(date_len, 10);

  // check format of date
  EXPECT_TRUE(isdigit(kBareosVersionStrings.Date[0]));
  EXPECT_TRUE(isdigit(kBareosVersionStrings.Date[1]));
  EXPECT_EQ(kBareosVersionStrings.Date[2], ' ');
  EXPECT_EQ(kBareosVersionStrings.Date[date_len - 5], ' ');
  EXPECT_TRUE(isdigit(kBareosVersionStrings.Date[date_len - 4]));
  EXPECT_TRUE(isdigit(kBareosVersionStrings.Date[date_len - 3]));
  EXPECT_TRUE(isdigit(kBareosVersionStrings.Date[date_len - 2]));
  EXPECT_TRUE(isdigit(kBareosVersionStrings.Date[date_len - 1]));

  const int day = std::stoi(date.substr(0, 2));
  EXPECT_GT(day, 0);
  EXPECT_LT(day, 32);

  // check that our month is a real month
  const std::vector<std::string> months{
      "January", "February", "March",     "April",   "May",      "June",
      "July",    "August",   "September", "October", "November", "December"};

  EXPECT_NE(std::find(std::begin(months), std::end(months),
                      date.substr(3, date_len - 8)),
            std::end(months));

  // year should have a sane value
  const int year = std::stoi(date.substr(date_len - 4, 4));
  EXPECT_GT(year, 2018);
  EXPECT_LT(year, 2100);

  // year is exactly 4 chars long
  EXPECT_EQ(strlen(kBareosVersionStrings.Year), 4);
  // all these chars are digits
  EXPECT_TRUE(isdigit(kBareosVersionStrings.Year[0]));
  EXPECT_TRUE(isdigit(kBareosVersionStrings.Year[1]));
  EXPECT_TRUE(isdigit(kBareosVersionStrings.Year[2]));
  EXPECT_TRUE(isdigit(kBareosVersionStrings.Year[3]));

  // year should be consistent across variables
  EXPECT_EQ(year, atoi(kBareosVersionStrings.Year));
  EXPECT_EQ(year % 1000, atoi(kBareosVersionStrings.ShortDate + 5));
  EXPECT_EQ(year, atoi(kBareosVersionStrings.ProgDateTime));

  // construct shortdate from date and check if it matches
  std::string ref_short_date =
      date.substr(0, 2) + date.substr(3, 3) + date.substr(date_len - 2, 2);
  EXPECT_EQ(short_date, ref_short_date);
}

TEST(version_strings, messages)
{
  // just make sure these strings contain at least some text
  EXPECT_GT(strlen(kBareosVersionStrings.BinaryInfo), 10);
  EXPECT_GT(strlen(kBareosVersionStrings.ServicesMessage), 10);
  EXPECT_GT(strlen(kBareosVersionStrings.JoblogMessage), 10);
}
