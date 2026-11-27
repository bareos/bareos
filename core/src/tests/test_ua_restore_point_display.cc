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

#include "dird/ua_restore_point_display.h"

#include "gtest/gtest.h"
#include "include/bareos.h"

namespace {

using directordaemon::FormatHumanReadableAge;
using directordaemon::FormatRestorePointCandidate;
using directordaemon::RestorePointCandidate;

TEST(FormatHumanReadableAgeTest, RoundsDownToTheLargestApplicableUnit)
{
  EXPECT_EQ(FormatHumanReadableAge(0), "just now");
  EXPECT_EQ(FormatHumanReadableAge(30), "just now");
  EXPECT_EQ(FormatHumanReadableAge(60), "1 minute ago");
  EXPECT_EQ(FormatHumanReadableAge(3599), "59 minutes ago");
  EXPECT_EQ(FormatHumanReadableAge(3600), "1 hour ago");
  EXPECT_EQ(FormatHumanReadableAge(86400), "1 day ago");
  EXPECT_EQ(FormatHumanReadableAge(3 * 86400), "3 days ago");
  EXPECT_EQ(FormatHumanReadableAge(604800), "1 week ago");
  EXPECT_EQ(FormatHumanReadableAge(2592000), "1 month ago");
  EXPECT_EQ(FormatHumanReadableAge(31536000), "1 year ago");
}

TEST(FormatHumanReadableAgeTest, ClampsNegativeDurationsToJustNow)
{
  EXPECT_EQ(FormatHumanReadableAge(-5), "just now");
}

TEST(FormatRestorePointCandidateTest, IncludesAllExpectedFields)
{
  RestorePointCandidate candidate;
  candidate.JobId = 42;
  candidate.RestorePoint = 1000;
  candidate.JobCount = 3;
  candidate.JobNames = "FullBackup, IncrBackup";
  candidate.JobFiles = 12345;
  candidate.JobBytes = 1234567890;

  utime_t now = 1000 + 3 * 86400;  // 3 days later
  std::string line = FormatRestorePointCandidate(candidate, now);

  EXPECT_NE(line.find("JobId=42"), std::string::npos);
  EXPECT_NE(line.find("3 days ago"), std::string::npos);
  EXPECT_NE(line.find("Files=12,345"), std::string::npos);
  EXPECT_NE(line.find("Jobs: FullBackup, IncrBackup"), std::string::npos);
}

}  // namespace
