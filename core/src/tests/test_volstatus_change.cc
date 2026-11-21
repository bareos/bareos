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

#include "dird/volstatus_change.h"

#include "gtest/gtest.h"

namespace {

using directordaemon::CheckVolStatusChange;
using directordaemon::VolStatusChange;

constexpr const char* kStatuses[]
    = {"Append",   "Archive", "Disabled",  "Full",  "Used",
       "Cleaning", "Recycle", "Read-Only", "Error", "Purged"};

TEST(VolStatusChange, SameStatusIsUnchanged)
{
  for (const char* status : kStatuses) {
    EXPECT_EQ(CheckVolStatusChange(status, status, true),
              VolStatusChange::kUnchanged)
        << status;
  }
}

TEST(VolStatusChange, RecycleOnlyFromPurged)
{
  for (const char* from : kStatuses) {
    std::string_view current{from};
    if (current == "Recycle") { continue; }
    auto expected = current == "Purged"
                        ? VolStatusChange::kAllowed
                        : VolStatusChange::kRecycleRequiresPurged;
    EXPECT_EQ(CheckVolStatusChange(from, "Recycle", false), expected) << from;
  }
}

TEST(VolStatusChange, CleaningOnlyWithoutJobs)
{
  EXPECT_EQ(CheckVolStatusChange("Append", "Cleaning", true),
            VolStatusChange::kCleaningRequiresEmptyVolume);
  EXPECT_EQ(CheckVolStatusChange("Full", "cleaning", true),
            VolStatusChange::kCleaningRequiresEmptyVolume);
  EXPECT_EQ(CheckVolStatusChange("Append", "Cleaning", false),
            VolStatusChange::kAllowed);
}

TEST(VolStatusChange, OtherChangesAreAllowed)
{
  for (const char* to : {"Append", "Archive", "Disabled", "Full", "Used",
                         "Read-Only", "Error"}) {
    for (const char* from : kStatuses) {
      if (std::string_view{from} == to) { continue; }
      EXPECT_EQ(CheckVolStatusChange(from, to, true), VolStatusChange::kAllowed)
          << from << " -> " << to;
    }
  }
}

}  // namespace
