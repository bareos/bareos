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

#include "gtest/gtest.h"

#include "dird/job_status_history.h"

namespace {

using directordaemon::ClearJobStatusTransitionHistory;
using directordaemon::GetJobStatusTransitionHistory;
using directordaemon::JobStatusTransitionEvent;
using directordaemon::JobStatusTransitionHistory;
using directordaemon::JobStatusTransitionSource;
using directordaemon::StoreJobStatusTransition;

TEST(JobStatusHistoryTest, FiltersTransitionsByRequestedRange)
{
  ClearJobStatusTransitionHistory();

  StoreJobStatusTransition(
      42, JobStatusTransitionEvent{100, JobStatusTransitionSource::kDirector,
                                   JS_Created, JS_Running, JS_Running});
  StoreJobStatusTransition(
      42, JobStatusTransitionEvent{200, JobStatusTransitionSource::kStorageDaemon,
                                   JS_WaitSD, JS_Running, JS_Running});
  StoreJobStatusTransition(
      42, JobStatusTransitionEvent{300, JobStatusTransitionSource::kFileDaemon,
                                   0, JS_Terminated, JS_Terminated});

  JobStatusTransitionHistory history;
  ASSERT_TRUE(GetJobStatusTransitionHistory(42, 150, 250, &history));
  ASSERT_TRUE(history.available);
  ASSERT_EQ(history.events.size(), 1u);
  EXPECT_EQ(history.events[0].timestamp, 200);
  EXPECT_EQ(history.events[0].source, JobStatusTransitionSource::kStorageDaemon);
}

TEST(JobStatusHistoryTest, KeepsOnlyMostRecentTransitionsPerJob)
{
  ClearJobStatusTransitionHistory();

  for (int index = 0; index < 300; ++index) {
    StoreJobStatusTransition(
        7, JobStatusTransitionEvent{1000 + index,
                                    JobStatusTransitionSource::kDirector,
                                    JS_Running, JS_Running + 1});
  }

  JobStatusTransitionHistory history;
  ASSERT_TRUE(GetJobStatusTransitionHistory(7, 0, 0, &history));
  ASSERT_TRUE(history.available);
  ASSERT_EQ(history.events.size(), 256u);
  EXPECT_EQ(history.events.front().timestamp, 1044);
  EXPECT_EQ(history.events.back().timestamp, 1299);
}

}  // namespace
