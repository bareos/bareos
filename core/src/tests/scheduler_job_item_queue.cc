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

#include "gtest/gtest.h"
#include "dird/dird_globals.h"
#include "dird/scheduler.h"
#include "dird/scheduler_job_item_queue.h"
#include "dird/run_hour_validator.h"
#include "dird/dird_conf.h"

using namespace directordaemon;

static SchedulerJobItemQueue scheduler_job_item_queue;

TEST(scheduler_job_item_queue, job_item)
{
  SchedulerJobItem item;
  EXPECT_FALSE(item.is_valid);

  JobResource job;
  RunResource run;

  SchedulerJobItem item_unitialised(&job, &run, time(nullptr), 0);
  EXPECT_TRUE(item_unitialised.is_valid);
}

TEST(scheduler_job_item_queue, compare_job_items)
{
  JobResource job[2];
  RunResource run[2];

  SchedulerJobItem item1(&job[0], &run[0], time(nullptr), 10);
  SchedulerJobItem item2 = item1;
  SchedulerJobItem item3(&job[1], &run[1], time(nullptr) + 3600, 11);

  EXPECT_EQ(item1, item2);
  EXPECT_NE(item1, item3);
}

TEST(scheduler_job_item_queue, priority_and_time)
{
  time_t now = time(nullptr);

  std::vector<JobResource> job_resources(4);
  std::vector<RunResource> run_resources(job_resources.size());

  /* JobResource::selection_type is used here as a helper variable
   * to store the expected place in the run order*/

  time_t runtime = now;
  run_resources[0].Priority = 10;
  job_resources[0].selection_type = 1;  // runs first (see above)
  scheduler_job_item_queue.EmplaceItem(&job_resources[0], &run_resources[0],
                                       runtime);
  runtime = now + 1;
  run_resources[1].Priority = 10;
  job_resources[1].selection_type = 3;
  scheduler_job_item_queue.EmplaceItem(&job_resources[1], &run_resources[1],
                                       runtime);
  runtime = now + 1;
  run_resources[2].Priority = 11;
  job_resources[2].selection_type = 4;  // runs last
  scheduler_job_item_queue.EmplaceItem(&job_resources[2], &run_resources[2],
                                       runtime);
  runtime = now + 1;
  run_resources[3].Priority = 9;
  job_resources[3].selection_type = 2;
  scheduler_job_item_queue.EmplaceItem(&job_resources[3], &run_resources[3],
                                       runtime);

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
  bool failed{false};
  RunResource run;
  try {
    scheduler_job_item_queue.EmplaceItem(nullptr, &run, 123);
  } catch (const std::invalid_argument& e) {
    EXPECT_STREQ(e.what(), "Invalid Argument: JobResource is undefined");
    failed = true;
  }
  EXPECT_TRUE(failed);
}

TEST(scheduler_job_item_queue, runtime_undefined)
{
  bool failed{false};
  JobResource job;
  RunResource run;
  try {
    scheduler_job_item_queue.EmplaceItem(&job, &run, 0);
  } catch (const std::invalid_argument& e) {
    EXPECT_STREQ(e.what(), "Invalid Argument: runtime is invalid");
    failed = true;
  }
  EXPECT_TRUE(failed);
}
