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
#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "cats/cats.h"
#include "include/bareos.h"
#include "include/jcr.h"
#include "include/make_unique.h"
#include "lib/parse_conf.h"
#include "dird/dird_globals.h"
#include "dird/dird_conf.h"
#include "dird/scheduler.h"
#include "dird/scheduler_time_adapter.h"
#include "dird/run_on_incoming_connect_interval.h"
#include "dird/job.h"

#include <algorithm>
#include <string>
#include <thread>
#include <vector>

namespace directordaemon {
bool DoReloadConfig() { return false; }
}  // namespace directordaemon

using namespace directordaemon;

class RunOnIncomingConnectIntervalClass : public ::testing::Test {
  void SetUp() override;
  void TearDown() override;
};

void RunOnIncomingConnectIntervalClass::SetUp()
{
  InitMsg(nullptr, nullptr);

  std::string path_to_config_file = std::string(
      PROJECT_SOURCE_DIR "/src/tests/configs/client-initiated-reconnect");
  my_config = InitDirConfig(path_to_config_file.c_str(), M_ERROR_TERM);
  my_config->ParseConfig();
}

void RunOnIncomingConnectIntervalClass::TearDown() { delete my_config; }

static bool find(std::vector<JobResource*> jobs, std::string jobname)
{
  return jobs.end() !=
         std::find_if(jobs.begin(), jobs.end(), [&jobname](JobResource* job) {
           return std::string{job->resource_name_} == jobname;
         });
}

TEST_F(RunOnIncomingConnectIntervalClass, find_all_jobs_for_client)
{
  std::vector<JobResource*> jobs{GetAllJobResourcesByClientName("bareos-fd")};

  EXPECT_EQ(jobs.size(), 4);

  EXPECT_TRUE(find(jobs, "backup-bareos-fd"));
  EXPECT_TRUE(find(jobs, "backup-bareos-fd-connect"));
  EXPECT_TRUE(find(jobs, "backup-bareos-fd-connect-2"));
  EXPECT_TRUE(find(jobs, "RestoreFiles"));
}

TEST_F(RunOnIncomingConnectIntervalClass,
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
  void SleepFor(std::chrono::seconds wait_interval) override { return; }
  void Terminate() override { return; }
};

class TimeAdapter : public SchedulerTimeAdapter {
 public:
  TimeAdapter(std::unique_ptr<TestTimeSource> t)
      : SchedulerTimeAdapter(std::move(t))
  {
  }
};

static int job_counter{};

static void RunAJob(JobControlRecord* jcr)
{
  ++job_counter;
  return;
}

class MockDatabase : public BareosDb {
 public:
  bool FindLastStartTimeForJobAndClient(JobControlRecord* jcr,
                                        std::string job_basename,
                                        std::string client_name,
                                        POOLMEM*& stime) override
  {
    snprintf(stime, MAX_NAME_LENGTH, "bla");
    return true;
  }
  bool OpenDatabase(JobControlRecord* jcr) override { return false; }
  void CloseDatabase(JobControlRecord* jcr) override { return; }
  bool ValidateConnection(void) override { return false; }
  void StartTransaction(JobControlRecord* jcr) override { return; }
  void EndTransaction(JobControlRecord* jcr) override { return; }
};

void scheduler_thread(Scheduler* s) { s->Run(); }

TEST_F(RunOnIncomingConnectIntervalClass, run_on_connect)
{
  Scheduler s(std::make_unique<TimeAdapter>(std::make_unique<TestTimeSource>()),
              RunAJob);

  std::thread st(scheduler_thread, &s);

  MockDatabase db;

  RunOnIncomingConnectInterval("bareos-fd", s, &db).Run();

  std::this_thread::sleep_for(std::chrono::seconds(1));

  EXPECT_EQ(job_counter, 2);
  s.Terminate();
  st.join();
}
