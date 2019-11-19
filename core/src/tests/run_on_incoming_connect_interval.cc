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

#include "include/bareos.h"
#include "cats/cats.h"
#include "dird/dird_conf.h"
#include "dird/dird_globals.h"
#include "dird/jcr_private.h"
#include "dird/job.h"
#include "dird/run_on_incoming_connect_interval.h"
#include "dird/scheduler.h"
#include "dird/scheduler_time_adapter.h"
#include "gmock/gmock.h"
#include "include/bareos.h"
#include "include/jcr.h"
#include "include/make_unique.h"
#include "lib/parse_conf.h"
#include "gtest/gtest.h"

#include <algorithm>
#include <chrono>
#include <string>
#include <thread>
#include <vector>

namespace directordaemon {
bool DoReloadConfig() { return false; }
}  // namespace directordaemon

using directordaemon::GetAllJobResourcesByClientName;
using directordaemon::InitDirConfig;
using directordaemon::JobResource;
using directordaemon::my_config;
using directordaemon::RunOnIncomingConnectInterval;
using directordaemon::Scheduler;
using directordaemon::SchedulerTimeAdapter;
using directordaemon::TimeSource;
using std::chrono::hours;
using std::chrono::seconds;
using std::chrono::system_clock;

class RunOnIncomingConnectIntervalTest : public ::testing::Test {
  void SetUp() override;
  void TearDown() override;
};

void RunOnIncomingConnectIntervalTest::SetUp()
{
  InitMsg(nullptr, nullptr);

  std::string path_to_config_file = std::string(
      PROJECT_SOURCE_DIR "/src/tests/configs/client-initiated-reconnect");
  my_config = InitDirConfig(path_to_config_file.c_str(), M_ERROR_TERM);
  my_config->ParseConfig();
}

void RunOnIncomingConnectIntervalTest::TearDown() { delete my_config; }

static bool find(std::vector<JobResource*> jobs, std::string jobname)
{
  return jobs.end() !=
         std::find_if(jobs.begin(), jobs.end(), [&jobname](JobResource* job) {
           return std::string{job->resource_name_} == jobname;
         });
}

TEST_F(RunOnIncomingConnectIntervalTest, find_all_jobs_for_client)
{
  std::vector<JobResource*> jobs{GetAllJobResourcesByClientName("bareos-fd")};

  EXPECT_EQ(jobs.size(), 4);

  EXPECT_TRUE(find(jobs, "backup-bareos-fd"));
  EXPECT_TRUE(find(jobs, "backup-bareos-fd-connect"));
  EXPECT_TRUE(find(jobs, "backup-bareos-fd-connect-2"));
  EXPECT_TRUE(find(jobs, "RestoreFiles"));
}

TEST_F(RunOnIncomingConnectIntervalTest,
       find_all_connect_interval_jobs_for_client)
{
  std::vector<JobResource*> jobs{GetAllJobResourcesByClientName("bareos-fd")};

  auto end = std::remove_if(jobs.begin(), jobs.end(), [](JobResource* job) {
    return job->RunOnIncomingConnectInterval == 0;
  });
  jobs.erase(end, jobs.end());
  EXPECT_EQ(jobs.size(), 2);

  EXPECT_TRUE(find(jobs, "backup-bareos-fd-connect"));
  EXPECT_TRUE(find(jobs, "backup-bareos-fd-connect-2"));
}

class TestTimeSource : public TimeSource {
 public:
  int counter = 0;
  time_t SystemTime() override { return ++counter; }
  void SleepFor(std::chrono::seconds /*wait_interval*/) override {}
  void Terminate() override {}
};

class TimeAdapter : public SchedulerTimeAdapter {
 public:
  explicit TimeAdapter(std::unique_ptr<TestTimeSource> t)
      : SchedulerTimeAdapter(std::move(t))
  {
  }
};

static struct TestResults {
  int job_counter{};
  std::map<std::string, int> job_names;
} test_results;

static void RunAJob(JobControlRecord* jcr)
{
  ++test_results.job_counter;

  // add job-name to map
  test_results.job_names[jcr->impl->res.job->resource_name_]++;
}

class MockDatabase : public BareosDb {
 public:
  enum class Mode
  {
    kFindNoStartTime,
    kFindStartTime
  };
  explicit MockDatabase(Mode mode) : mode_(mode) {}
  bool FindLastStartTimeForJobAndClient(JobControlRecord* /*jcr*/,
                                        std::string /*job_basename*/,
                                        std::string /*client_name*/,
                                        POOLMEM*& stime) override
  {
    switch (mode_) {
      default:
      case Mode::kFindNoStartTime:
        return false;
      case Mode::kFindStartTime: {
        auto now = system_clock::now();
        auto three_hours = seconds(hours(3) + seconds(1));

        utime_t fake_start_time_three_hours_in_the_past =
            system_clock::to_time_t(now - three_hours);

        bstrutime(stime, MAX_NAME_LENGTH,
                  fake_start_time_three_hours_in_the_past);
        return true;
      }
    }
  }
  bool OpenDatabase(JobControlRecord* /*jcr*/) override { return false; }
  void CloseDatabase(JobControlRecord* /*jcr*/) override {}
  bool ValidateConnection() override { return false; }
  void StartTransaction(JobControlRecord* /*jcr*/) override {}
  void EndTransaction(JobControlRecord* /*jcr*/) override {}

 private:
  Mode mode_{};
};

static void scheduler_thread(Scheduler* scheduler) { scheduler->Run(); }

static void RunJob(BareosDb& db)
{
  auto tts{std::make_unique<TestTimeSource>()};
  auto ta{std::make_unique<TimeAdapter>(std::move(tts))};

  Scheduler scheduler(std::move(ta), RunAJob);

  std::thread thread(scheduler_thread, &scheduler);

  test_results = TestResults();

  RunOnIncomingConnectInterval("bareos-fd", scheduler, &db).Run();

  std::this_thread::sleep_for(std::chrono::milliseconds(20));

  scheduler.Terminate();
  thread.join();
}

TEST_F(RunOnIncomingConnectIntervalTest, no_start_time_found_in_database)
{
  MockDatabase db(MockDatabase::Mode::kFindNoStartTime);
  RunJob(db);

  ASSERT_EQ(test_results.job_counter, 2) << "Not all jobs did run";

  EXPECT_NE(test_results.job_names.find("backup-bareos-fd-connect"),
            test_results.job_names.end())
      << "Job backup-bareos-fd-connect did not run";

  EXPECT_NE(test_results.job_names.find("backup-bareos-fd-connect-2"),
            test_results.job_names.end())
      << "Job backup-bareos-fd-connect2 did not run";
}

TEST_F(RunOnIncomingConnectIntervalTest, start_time_found_in_database)
{
  MockDatabase db(MockDatabase::Mode::kFindStartTime);
  RunJob(db);

  ASSERT_EQ(test_results.job_counter, 1)
      << "backup-bareos-fd-connect did not run";

  EXPECT_NE(test_results.job_names.find("backup-bareos-fd-connect"),
            test_results.job_names.end())
      << "Job backup-bareos-fd-connect did not run";
}
