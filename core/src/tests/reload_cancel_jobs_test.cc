/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2026-2026 Bareos GmbH & Co. KG

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

#include "dird/dird_globals.h"
#include "dird/dird_conf.h"
#include "dird/reload.h"
#include "dird/job.h"
#include "dird/jcr_util.h"
#include "dird/jobq.h"

using namespace directordaemon;

class ReloadCancelJobsTest : public ::testing::Test {
 protected:
  void SetUp() override { InitDirGlobals(); }
  void TearDown() override
  {
    if (my_config) {
      delete my_config;
      my_config = nullptr;
    }
  }
};

TEST_F(ReloadCancelJobsTest, CancelsJobsReferencingRemovedResources)
{
  // Initialize configuration from test fixtures
  std::string path_to_config_file
      = std::string("configs/bareos-configparser-tests");
  ConfigurationParser* dir_conf
      = InitDirConfig(path_to_config_file.c_str(), M_ERROR_TERM);
  ASSERT_NE(nullptr, dir_conf);
  my_config = dir_conf;
  ASSERT_TRUE(my_config->ParseConfig());
  me = (DirectorResource*)my_config->GetNextRes(R_DIRECTOR, nullptr);
  my_config->own_resource_ = me;
  ASSERT_NE(nullptr, me);

  // Find a job resource present in the test config
  JobResource* job = (JobResource*)my_config->GetResWithName(R_JOB, "job1");
  ASSERT_NE(nullptr, job) << "Test config must contain job1";

  // Initialize job server (job queue)
  InitJobServer(1);

  // Create a JCR referencing the job resource and enqueue it
  JobControlRecord* jcr = NewDirectorJcr(DirdFreeJcr);
  ASSERT_NE(nullptr, jcr);
  SetJcrDefaults(jcr, job);

  int status = JobqAdd(&job_queue, jcr);
  ASSERT_EQ(status, 0);

  // Simulate removal of the job resource from configuration (as during reload)
  bool removed = my_config->RemoveResource(R_JOB, "job1");
  ASSERT_TRUE(removed);

  // Call the validation routine that should cancel invalid queued jobs
  CancelInvalidQueuedJobs();

  // The jcr should now be marked canceled
  EXPECT_TRUE(jcr->IsJobCanceled() || jcr->getJobStatus() == JS_Canceled);

  // Cleanup: attempt to remove and free job
  // If still queued, remove it
  JobqRemove(&job_queue, jcr);
  FreeJcr(jcr);
  TermJobServer();
}
