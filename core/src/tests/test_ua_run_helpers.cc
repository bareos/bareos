/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2026-2026 Bareos GmbH & Co. KG

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

#include "dird/ua_run_helpers.h"

#include "gtest/gtest.h"
#include "include/bareos.h"

using namespace directordaemon;

TEST(UaRunHelpers, FitRestoreOptionText)
{
  EXPECT_EQ(FitRestoreOptionText("short", 10), "short");
  EXPECT_EQ(FitRestoreOptionText("exactly10!", 10), "exactly10!");
  EXPECT_EQ(FitRestoreOptionText("a longer text", 10), "a longe...");
  EXPECT_EQ(FitRestoreOptionText("a longer text", 3), "a l");
  EXPECT_EQ(FitRestoreOptionText("abc", 0), "");
}

TEST(UaRunHelpers, ScrollRestoreOptionText)
{
  EXPECT_EQ(ScrollRestoreOptionText("short", 10, 4), "short");
  EXPECT_EQ(ScrollRestoreOptionText("0123456789", 6, 0), "01234>");
  EXPECT_EQ(ScrollRestoreOptionText("0123456789", 6, 2), "<2345>");
  EXPECT_EQ(ScrollRestoreOptionText("0123456789", 6, 99), "<56789");
  EXPECT_EQ(ScrollRestoreOptionText("0123456789", 2, 1), "01");
}

TEST(UaRunHelpers, FormatRunScheduleRelativeTime)
{
  const time_t now = 1000000000;
  EXPECT_EQ(FormatRunScheduleRelativeTime(now, now), "now");
  EXPECT_EQ(FormatRunScheduleRelativeTime(now + 59, now), "now");
  EXPECT_EQ(FormatRunScheduleRelativeTime(now + 60, now), "in 1 minute");
  EXPECT_EQ(FormatRunScheduleRelativeTime(now + 2 * 3600, now), "in 2 hours");
  EXPECT_EQ(FormatRunScheduleRelativeTime(now - 86400, now), "1 day ago");
  EXPECT_EQ(FormatRunScheduleRelativeTime(now - 14 * 86400, now),
            "2 weeks ago");
}

TEST(UaRunHelpers, IsListingTimestamp)
{
  EXPECT_TRUE(IsListingTimestamp("2026-05-04 12:34:56", 0));
  EXPECT_TRUE(IsListingTimestamp("xx 2026-05-04 12:34:56 f", 3));
  EXPECT_FALSE(IsListingTimestamp("2026-05-04 12:34:5", 0));
  EXPECT_FALSE(IsListingTimestamp("2026/05/04 12:34:56", 0));
}

TEST(UaRunHelpers, ParseEstimateListingLine)
{
  std::string path;
  bool directory = false;

  EXPECT_TRUE(ParseEstimateListingLine(
      "drwxr-xr-x 2 root root 4096 2026-05-04 12:34:56  /etc/ssl", &path,
      &directory));
  EXPECT_EQ(path, "/etc/ssl");
  EXPECT_TRUE(directory);

  EXPECT_TRUE(ParseEstimateListingLine(
      "-rw-r--r-- 1 root root 12 2026-05-04 12:34:56 /etc/my file", &path,
      &directory));
  EXPECT_EQ(path, "/etc/my file");
  EXPECT_FALSE(directory);

  EXPECT_TRUE(ParseEstimateListingLine(
      "lrwxrwxrwx 1 root root 3 2026-05-04 12:34:56 /etc/l -> /etc/x", &path,
      &directory));
  EXPECT_EQ(path, "/etc/l");
  EXPECT_FALSE(directory);
}

TEST(UaRunHelpers, ParseEstimateListingLineRejectsNonListingLines)
{
  std::string path;
  bool directory = false;
  EXPECT_FALSE(ParseEstimateListingLine("", &path, &directory));
  EXPECT_FALSE(ParseEstimateListingLine("2000 OK estimate", &path, &directory));
  EXPECT_FALSE(ParseEstimateListingLine("-rw-r--r-- 1 root no timestamp", &path,
                                        &directory));
  EXPECT_FALSE(ParseEstimateListingLine(
      "-rw-r--r-- 1 root root 12 2026-05-04 12:34:56   ", &path, &directory));
}

TEST(UaRunHelpers, NormalizeBrowsePath)
{
  EXPECT_EQ(NormalizeBrowsePath(""), "/");
  EXPECT_EQ(NormalizeBrowsePath("/"), "/");
  EXPECT_EQ(NormalizeBrowsePath("///"), "/");
  EXPECT_EQ(NormalizeBrowsePath("/etc/"), "/etc");
  EXPECT_EQ(NormalizeBrowsePath("/etc"), "/etc");
}

TEST(UaRunHelpers, ParentAndBasename)
{
  EXPECT_EQ(ParentBrowsePath("/"), "/");
  EXPECT_EQ(ParentBrowsePath("/etc"), "/");
  EXPECT_EQ(ParentBrowsePath("/etc/ssl/"), "/etc");
  EXPECT_EQ(ParentBrowsePath("relative"), "/");

  EXPECT_EQ(BrowsePathBasename("/"), "/");
  EXPECT_EQ(BrowsePathBasename("/etc"), "etc");
  EXPECT_EQ(BrowsePathBasename("/etc/ssl/"), "ssl");
  EXPECT_EQ(BrowsePathBasename("relative"), "relative");
}

TEST(UaRunHelpers, IsImmediateBrowseChild)
{
  EXPECT_TRUE(IsImmediateBrowseChild("/", "/etc"));
  EXPECT_FALSE(IsImmediateBrowseChild("/", "/etc/ssl"));
  EXPECT_FALSE(IsImmediateBrowseChild("/", "/"));
  EXPECT_FALSE(IsImmediateBrowseChild("/", "etc"));

  EXPECT_TRUE(IsImmediateBrowseChild("/etc", "/etc/ssl"));
  EXPECT_TRUE(IsImmediateBrowseChild("/etc/", "/etc/ssl"));
  EXPECT_FALSE(IsImmediateBrowseChild("/etc", "/etc"));
  EXPECT_FALSE(IsImmediateBrowseChild("/etc", "/etc/ssl/certs"));
  EXPECT_FALSE(IsImmediateBrowseChild("/etc", "/etcetera"));
  EXPECT_FALSE(IsImmediateBrowseChild("/etc", "/etc/"));
}

TEST(UaRunHelpers, RemoveLastBrowseSearchCharacter)
{
  std::string text = "abc";
  RemoveLastBrowseSearchCharacter(&text);
  EXPECT_EQ(text, "ab");

  text = "caf\xc3\xa9";
  RemoveLastBrowseSearchCharacter(&text);
  EXPECT_EQ(text, "caf");

  text.clear();
  RemoveLastBrowseSearchCharacter(&text);
  EXPECT_TRUE(text.empty());
}

TEST(UaRunHelpers, ContainsBrowseSearchMatch)
{
  EXPECT_TRUE(ContainsBrowseSearchMatch("anything", ""));
  EXPECT_TRUE(ContainsBrowseSearchMatch("/etc/ssl", "ssl"));
  EXPECT_FALSE(ContainsBrowseSearchMatch("/etc/ssl", "SSL"));
  EXPECT_FALSE(ContainsBrowseSearchMatch("", "x"));
}
