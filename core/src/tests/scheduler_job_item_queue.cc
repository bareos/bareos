/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2019-2026 Bareos GmbH & Co. KG

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

#include "lib/parse_conf.h"
#include "gtest/gtest.h"
#include "include/bareos.h"

#include "dird/dird_globals.h"
#include "dird/job_trigger.h"
#include "dird/scheduler.h"
#include "dird/scheduler_job_item_queue.h"
#include "dird/date_time.h"
#include "dird/dird_conf.h"

using namespace directordaemon;

static SchedulerJobItemQueue scheduler_job_item_queue;

TEST(scheduler_job_item_queue, job_item)
{
  SchedulerJobItem item;
  EXPECT_FALSE(item.is_valid);

  auto conf = std::make_shared<LoadedConfiguration>();
  JobResource job;
  RunResource run;

  SchedulerJobItem item_unitialised(conf, &job, &run, time(nullptr), 0,
                                    JobTrigger::kUndefined);
  EXPECT_TRUE(item_unitialised.is_valid);
}

TEST(scheduler_job_item_queue, compare_job_items)
{
  auto conf = std::make_shared<LoadedConfiguration>();
  JobResource job[2];
  RunResource run[2];

  SchedulerJobItem item1(conf, &job[0], &run[0], time(nullptr), 10,
                         JobTrigger::kUndefined);
  SchedulerJobItem item2 = item1;
  SchedulerJobItem item3(conf, &job[1], &run[1], time(nullptr) + 3600, 11,
                         JobTrigger::kUndefined);

  EXPECT_EQ(item1, item2);
  EXPECT_NE(item1, item3);
}

TEST(scheduler_job_item_queue, priority_and_time)
{
  time_t now = time(nullptr);

  auto unused = std::make_shared<LoadedConfiguration>();
  std::vector<JobResource> job_resources(4);
  std::vector<RunResource> run_resources(job_resources.size());

  /* JobResource::selection_type is used here as a helper variable
   * to store the expected place in the run order*/

  time_t runtime = now;
  run_resources[0].Priority = 10;
  job_resources[0].selection_type = 1;  // runs first (see above)
  scheduler_job_item_queue.EmplaceItem(unused, &job_resources[0],
                                       &run_resources[0], runtime,
                                       JobTrigger::kUndefined);
  runtime = now + 1;
  run_resources[1].Priority = 10;
  job_resources[1].selection_type = 3;
  scheduler_job_item_queue.EmplaceItem(unused, &job_resources[1],
                                       &run_resources[1], runtime,
                                       JobTrigger::kUndefined);
  runtime = now + 1;
  run_resources[2].Priority = 11;
  job_resources[2].selection_type = 4;  // runs last
  scheduler_job_item_queue.EmplaceItem(unused, &job_resources[2],
                                       &run_resources[2], runtime,
                                       JobTrigger::kUndefined);
  runtime = now + 1;
  run_resources[3].Priority = 9;
  job_resources[3].selection_type = 2;
  scheduler_job_item_queue.EmplaceItem(unused, &job_resources[3],
                                       &run_resources[3], runtime,
                                       JobTrigger::kUndefined);

  int item_position = 1;
  while (!scheduler_job_item_queue.Empty()) {
    auto job_item = scheduler_job_item_queue.TakeOutTopItem();
    ASSERT_TRUE(job_item.is_valid);
    ASSERT_EQ(job_item.job->selection_type, item_position)
        << "selection_type is used as "
           "position parameter in this "
           "test";
    item_position++;
  }
}

TEST(scheduler_job_item_queue, job_resource_undefined)
{
  auto unused = std::make_shared<LoadedConfiguration>();
  bool failed{false};
  RunResource run;
  try {
    scheduler_job_item_queue.EmplaceItem(unused, nullptr, &run, 123,
                                         JobTrigger::kUndefined);
  } catch (const std::invalid_argument& e) {
    EXPECT_STREQ(e.what(), "Invalid Argument: JobResource is undefined");
    failed = true;
  }
  EXPECT_TRUE(failed);
}

TEST(scheduler_job_item_queue, runtime_undefined)
{
  auto unused = std::make_shared<LoadedConfiguration>();
  bool failed{false};
  JobResource job;
  RunResource run;
  try {
    scheduler_job_item_queue.EmplaceItem(unused, &job, &run, 0,
                                         JobTrigger::kUndefined);
  } catch (const std::invalid_argument& e) {
    EXPECT_STREQ(e.what(), "Invalid Argument: runtime is invalid");
    failed = true;
  }
  EXPECT_TRUE(failed);
}

TEST(RunResource, NextScheduleTimeSkipsPastMinutesInCurrentHour)
{
  RunResource run;
  // Trigger on all days, months, weeks, weekdays
  for (int i = 0; i < 31; ++i) { SetBit(i, run.date_time_mask.mday); }
  for (int i = 0; i < 7; ++i) { SetBit(i, run.date_time_mask.wday); }
  for (int i = 0; i < 12; ++i) { SetBit(i, run.date_time_mask.month); }
  for (int i = 0; i < 5; ++i) { SetBit(i, run.date_time_mask.wom); }
  for (int i = 0; i < 54; ++i) { SetBit(i, run.date_time_mask.woy); }
  // Only trigger at 14:00
  SetBit(14, run.date_time_mask.hour);
  run.minute = 0;

  // Set start time to 14:30 today
  struct tm tm_start = {};
  time_t now = time(nullptr);
  Blocaltime(&now, &tm_start);
  tm_start.tm_hour = 14;
  tm_start.tm_min = 30;
  tm_start.tm_sec = 0;
  tm_start.tm_isdst = -1;
  time_t start_1430 = mktime(&tm_start);

  // 14:00 today is in the past; next run must be tomorrow at 14:00 (not today
  // at 14:00)
  auto next = run.NextScheduleTime(start_1430, 2);
  ASSERT_TRUE(next.has_value());
  EXPECT_GE(next.value(), start_1430);

  struct tm tm_res = {};
  Blocaltime(&next.value(), &tm_res);
  EXPECT_EQ(tm_res.tm_hour, 14);
  EXPECT_EQ(tm_res.tm_min, 0);

  // Check that if start is 14:00 or earlier, today's 14:00 IS returned
  tm_start.tm_min = 0;
  time_t start_1400 = mktime(&tm_start);
  auto next_exact = run.NextScheduleTime(start_1400, 1);
  ASSERT_TRUE(next_exact.has_value());
  EXPECT_EQ(next_exact.value(), start_1400);
}
