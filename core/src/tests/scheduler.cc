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

#if defined(HAVE_MINGW)
#include "include/bareos.h"
#include "gtest/gtest.h"
#else
#include "gtest/gtest.h"
#include "include/bareos.h"
#endif


#include "dird/dird_globals.h"
#include "dird/jcr_private.h"
#include "dird/scheduler.h"
#include "dird/dird_conf.h"
#define DIRECTOR_DAEMON
#include "include/jcr.h"
#include "include/make_unique.h"
#include "lib/parse_conf.h"
#include "tests/scheduler_time_source.h"
#include "dird/scheduler_system_time_source.h"

#include <atomic>
#include <chrono>
#include <thread>
#include <numeric>
#include <iterator>
#include <sstream>
#include <string>
#include <iostream>

using namespace directordaemon;

namespace directordaemon {
bool DoReloadConfig() { return false; }
}  // namespace directordaemon

class SimulatedTimeAdapter : public SchedulerTimeAdapter {
 public:
  SimulatedTimeAdapter()
      : SchedulerTimeAdapter(std::make_unique<SimulatedTimeSource>())
  {
    default_wait_interval_ = 60;
  }
};

static std::unique_ptr<Scheduler> scheduler;
static SimulatedTimeAdapter* time_adapter;

class SchedulerTest : public ::testing::Test {
  void SetUp() override
  {
    std::unique_ptr<SimulatedTimeAdapter> ta =
        std::make_unique<SimulatedTimeAdapter>();
    time_adapter = ta.get();

    scheduler = std::make_unique<Scheduler>(std::move(ta),
                                            SimulatedTimeSource::ExecuteJob);
  }
  void TearDown() override { scheduler.reset(); }
};

static void StopScheduler(std::chrono::milliseconds timeout)
{
  std::this_thread::sleep_for(timeout);
  scheduler->Terminate();
}

TEST_F(SchedulerTest, terminate)
{
  InitMsg(NULL, NULL); /* initialize message handler */

  std::string path_to_config_file =
      std::string(RELATIVE_PROJECT_SOURCE_DIR "/configs/scheduler-hourly");

  my_config = InitDirConfig(path_to_config_file.c_str(), M_ERROR_TERM);
  ASSERT_TRUE(my_config);

  my_config->ParseConfig();

  std::thread scheduler_canceler(StopScheduler, std::chrono::milliseconds(200));

  scheduler->Run();

  scheduler_canceler.join();
  delete my_config;
}

TEST_F(SchedulerTest, system_time_source)
{
  SystemTimeSource s;
  time_t start = s.SystemTime();
  s.SleepFor(std::chrono::seconds(1));
  time_t end = s.SystemTime();
  EXPECT_GT(end - start, 0);
  EXPECT_LT(end - start, 2);
}

TEST_F(SchedulerTest, system_time_source_canceled)
{
  SystemTimeSource s;

  time_t start = s.SystemTime();

  std::thread terminate_wait_for_thread([&s]() {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    s.Terminate();
  });

  s.SleepFor(std::chrono::seconds(1));
  time_t end = s.SystemTime();

  terminate_wait_for_thread.join();

  bool end_equals_start = (end == start);
  bool end_wrapped_to_next_second_while_waiting = (end == start + 1);

  EXPECT_TRUE(end_equals_start || end_wrapped_to_next_second_while_waiting);
}

static std::vector<time_t> list_of_job_execution_time_stamps;
static std::vector<time_t> list_of_time_gaps_between_adjacent_jobs;
static int maximum_number_of_jobs_run{0};
static int counter_of_number_of_jobs_run{0};

void SimulatedTimeSource::ExecuteJob(JobControlRecord* jcr)
{
  // called by the scheduler to simulate a job run
  counter_of_number_of_jobs_run++;

  list_of_job_execution_time_stamps.emplace_back(
      time_adapter->time_source_->SystemTime());

  if (debug) { std::cout << jcr->impl->res.job->resource_name_ << std::endl; }

  if (counter_of_number_of_jobs_run == maximum_number_of_jobs_run) {
    scheduler->Terminate();
  }

  // auto tm = *std::gmtime(&t);
  // std::cout << "Executing job: " << jcr->Job << std::endl;
  // std::cout << std::put_time(&tm, "%d-%m-%Y %H:%M:%S") << std::endl;
}

static int CalculateAverage()
{
  std::adjacent_difference(
      list_of_job_execution_time_stamps.begin(),
      list_of_job_execution_time_stamps.end(),
      std::back_inserter(list_of_time_gaps_between_adjacent_jobs));

  list_of_time_gaps_between_adjacent_jobs.erase(
      list_of_time_gaps_between_adjacent_jobs.begin());

  int sum{std::accumulate(list_of_time_gaps_between_adjacent_jobs.begin(),
                          list_of_time_gaps_between_adjacent_jobs.end(), 0)};

  return sum / list_of_time_gaps_between_adjacent_jobs.size();
}

TEST_F(SchedulerTest, hourly)
{
  InitMsg(NULL, NULL);

  if (debug) { std::cout << "Start test" << std::endl; }

  std::string path_to_config_file{
      std::string(RELATIVE_PROJECT_SOURCE_DIR "/configs/scheduler-hourly")};

  my_config = InitDirConfig(path_to_config_file.c_str(), M_ERROR_TERM);
  ASSERT_TRUE(my_config);

  if (debug) { std::cout << "Parse config" << std::endl; }

  my_config->ParseConfig();
  ASSERT_TRUE(PopulateDefs());

  list_of_job_execution_time_stamps.clear();
  list_of_time_gaps_between_adjacent_jobs.clear();
  counter_of_number_of_jobs_run = 0;
  maximum_number_of_jobs_run = 25;

  if (debug) { std::cout << "Run scheduler" << std::endl; }
  scheduler->Run();

  if (debug) { std::cout << "End" << std::endl; }
  delete my_config;

  int average{CalculateAverage()};

  bool average_time_between_adjacent_jobs_is_too_low = average < 3600 - 36;
  bool average_time_between_adjacent_jobs_is_too_high = average > 3600 + 36;

  if (debug || average_time_between_adjacent_jobs_is_too_low ||
      average_time_between_adjacent_jobs_is_too_high) {
    int hour = 0;
    for (const auto& t : list_of_job_execution_time_stamps) {
      auto tm = *std::gmtime(&t);
      std::cout << "Hour " << hour << ": "
                << std::put_time(&tm, "%d-%m-%Y %H:%M:%S");
      if (hour) {
        std::cout << " - " << list_of_time_gaps_between_adjacent_jobs[hour]
                  << " sec";
      }
      std::cout << std::endl;
      hour++;
    }
  }

  if (debug) {
    std::cout << "Average simulated time between two jobs: " << average
              << " seconds" << std::endl;
  }

  EXPECT_FALSE(average_time_between_adjacent_jobs_is_too_low);
  EXPECT_FALSE(average_time_between_adjacent_jobs_is_too_high);
}

TEST_F(SchedulerTest, on_time)
{
  InitMsg(NULL, NULL);

  if (debug) { std::cout << "Start test" << std::endl; }

  std::string path_to_config_file{
      std::string(RELATIVE_PROJECT_SOURCE_DIR "/configs/scheduler-on-time")};

  my_config = InitDirConfig(path_to_config_file.c_str(), M_ERROR_TERM);
  ASSERT_TRUE(my_config);

  if (debug) { std::cout << "Parse config" << std::endl; }

  my_config->ParseConfig();
  ASSERT_TRUE(PopulateDefs());

  list_of_job_execution_time_stamps.clear();
  maximum_number_of_jobs_run = 3;
  counter_of_number_of_jobs_run = 0;

  if (debug) { std::cout << "Run scheduler" << std::endl; }
  scheduler->Run();

  for (const auto& t : list_of_job_execution_time_stamps) {
    auto tm = *std::localtime(&t);
    bool is_two_o_clock = tm.tm_hour == 2;
    bool is_monday = tm.tm_wday == 1;
    EXPECT_TRUE(is_two_o_clock);
    EXPECT_TRUE(is_monday);
    if (debug || !is_two_o_clock || !is_monday) {
      std::cout << std::put_time(&tm, "%A, %d-%m-%Y %H:%M:%S ") << std::endl;
    }
  }

  if (debug) { std::cout << "End" << std::endl; }
  delete my_config;
}

TEST_F(SchedulerTest, add_job_with_no_run_resource_to_queue)
{
  OSDependentInit();
  InitMsg(NULL, NULL);

  if (debug) { std::cout << "Start test" << std::endl; }

  std::string path_to_config_file{std::string(
      RELATIVE_PROJECT_SOURCE_DIR "/configs/bareos-configparser-tests")};

  my_config = InitDirConfig(path_to_config_file.c_str(), M_ERROR_TERM);
  ASSERT_TRUE(my_config);

  if (debug) { std::cout << "Parse config" << std::endl; }

  my_config->ParseConfig();
  ASSERT_TRUE(PopulateDefs());

  counter_of_number_of_jobs_run = 0;
  maximum_number_of_jobs_run = 1;

  auto scheduler_thread{std::thread([]() { scheduler->Run(); })};

  JobResource* job{dynamic_cast<JobResource*>(
      my_config->GetResWithName(R_JOB, "backup-bareos-fd"))};
  ASSERT_TRUE(job) << "Job Resource \"backup-bareos-fd\" not found";

  scheduler->AddJobWithNoRunResourceToQueue(job);

  scheduler_thread.join();
  ASSERT_EQ(counter_of_number_of_jobs_run, 1);

  delete my_config;
}
