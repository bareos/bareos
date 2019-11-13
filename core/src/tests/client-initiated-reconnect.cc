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
#include "include/bareos.h"
#include "include/jcr.h"
#include "lib/parse_conf.h"
#include "dird/dird_globals.h"
#include "dird/dird_conf.h"
#include "dird/scheduler.h"
#include "dird/job.h"

#include <algorithm>
#include <string>
#include <vector>

namespace directordaemon {
bool DoReloadConfig() { return false; }
}  // namespace directordaemon

using namespace directordaemon;

class ClientInitiatedReconnect : public ::testing::Test {
  void SetUp() override;
  void TearDown() override;
};

void ClientInitiatedReconnect::SetUp()
{
  InitMsg(nullptr, nullptr);

  std::string path_to_config_file = std::string(
      PROJECT_SOURCE_DIR "/src/tests/configs/client-initiated-reconnect");
  my_config = InitDirConfig(path_to_config_file.c_str(), M_ERROR_TERM);
  my_config->ParseConfig();
}

void ClientInitiatedReconnect::TearDown() { delete my_config; }

static bool find(std::vector<JobResource*> jobs, std::string jobname)
{
  return jobs.end() !=
         std::find_if(jobs.begin(), jobs.end(), [&jobname](JobResource* job) {
           return std::string{job->resource_name_} == jobname;
         });
}

TEST_F(ClientInitiatedReconnect, find_all_jobs_for_client)
{
  std::vector<JobResource*> jobs{GetAllJobResourcesByClientName("bareos-fd")};

  EXPECT_EQ(jobs.size(), 4);

  EXPECT_TRUE(find(jobs, "backup-bareos-fd"));
  EXPECT_TRUE(find(jobs, "backup-bareos-fd-connect"));
  EXPECT_TRUE(find(jobs, "backup-bareos-fd-connect-2"));
  EXPECT_TRUE(find(jobs, "RestoreFiles"));
}

TEST_F(ClientInitiatedReconnect, find_all_connect_interval_jobs_for_client)
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
