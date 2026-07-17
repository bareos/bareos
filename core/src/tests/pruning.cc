/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2022-2026 Bareos GmbH & Co. KG

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

#include "testing_dir_common.h"

#include "include/jcr.h"
#include "dird/jcr_util.h"
#include "dird/job.h"
#include "dird/ua_prune.h"
#include "dird/ua_purge.h"

#include <unordered_map>

TEST(Pruning, ExcludeRunningJobsFromList)
{
  InitDirGlobals();
  std::string path_to_config = std::string("configs/pruning/");
  PConfigParser director_config(DirectorPrepareResources(path_to_config));
  if (!director_config) { return; }

  JobControlRecord* jcr1 = directordaemon::NewDirectorJcr(
      director_config->GetCurrentConfiguration());
  jcr1->JobId = 1;
  JobControlRecord* jcr2 = directordaemon::NewDirectorJcr(
      director_config->GetCurrentConfiguration());
  jcr2->JobId = 2;
  JobControlRecord* jcr3 = directordaemon::NewDirectorJcr(
      director_config->GetCurrentConfiguration());
  jcr3->JobId = 3;

  std::vector<JobId_t> pruninglist{0, 0, 1, 2, 3, 4, 5};
  int NumJobsToBePruned
      = directordaemon::ExcludeRunningJobsFromList(pruninglist);

  EXPECT_EQ(NumJobsToBePruned, 2);
  EXPECT_EQ(pruninglist[0], 4);
  EXPECT_EQ(pruninglist[1], 5);
}

TEST(Pruning, TransformJobidsTobedeleted)
{
  InitDirGlobals();
  std::string path_to_config = std::string("configs/pruning/");
  PConfigParser director_config(DirectorPrepareResources(path_to_config));
  if (!director_config) { return; }

  JobControlRecord* jcr1 = directordaemon::NewDirectorJcr(
      director_config->GetCurrentConfiguration());
  jcr1->JobId = 1;

  directordaemon::UaContext* ua = directordaemon::new_ua_context(jcr1);
  std::vector<JobId_t> pruninglist{0, 1, 2, 0, 3, 4, 5};
  std::string jobids_to_delete
      = directordaemon::PrepareJobidsTobedeleted(ua, pruninglist);

  EXPECT_EQ(jobids_to_delete, "2,3,4,5");
  EXPECT_EQ(pruninglist.size(), 4);

  FreeUaContext(ua);
}

TEST(Pruning, SummarizePoolPruneJobs)
{
  std::unordered_map<JobId_t, uint64_t> prunable_jobs{
      {10, 1024},
      {11, 2048},
      {42, 4096},
  };

  auto summary = directordaemon::SummarizePoolPruneJobs(prunable_jobs, 2);

  EXPECT_EQ(summary.prunable_volumes, 2);
  EXPECT_EQ(summary.prunable_jobs, 3);
  EXPECT_EQ(summary.prunable_bytes, 7168);
}

TEST(Pruning, BuildPoolPruneReport)
{
  std::vector<directordaemon::PoolPruneVolumeDetail> volumes{
      {
          .volume_name = "Full-0003",
          .volume_status = "Used",
          .last_written = "2026-05-03 09:00:00",
          .reason = "volume_retention_expired",
          .job_ids = {4},
          .prunable_jobs = 1,
          .prunable_bytes = 512,
      },
      {
          .volume_name = "Full-0001",
          .volume_status = "Used",
          .last_written = "2026-05-03 10:00:00",
          .reason = "volume_retention_expired",
          .job_ids = {1, 2},
          .prunable_jobs = 2,
          .prunable_bytes = 3072,
      },
      {
          .volume_name = "Full-0002",
          .volume_status = "Full",
          .last_written = "2026-05-03 11:00:00",
          .reason = "volume_retention_expired",
          .job_ids = {3},
          .prunable_jobs = 1,
          .prunable_bytes = 4096,
      },
  };
  std::vector<directordaemon::PoolPruneJobDetail> jobs{
      {.jobid = 1, .name = "backup-a", .bytes = 1024, .start_time = ""},
      {.jobid = 2, .name = "backup-b", .bytes = 2048, .start_time = ""},
      {.jobid = 3, .name = "backup-c", .bytes = 4096, .start_time = ""},
      {.jobid = 4, .name = "backup-d", .bytes = 512, .start_time = ""},
  };

  auto report = directordaemon::BuildPoolPruneReport(volumes, jobs);

  EXPECT_EQ(report.reason, "volume_retention_expired");
  EXPECT_EQ(report.summary.prunable_volumes, 3);
  EXPECT_EQ(report.summary.prunable_jobs, 4);
  EXPECT_EQ(report.summary.prunable_bytes, 7680);
  ASSERT_EQ(report.volumes.size(), 3u);
  EXPECT_EQ(report.volumes[0].volume_name, "Full-0003");
  EXPECT_EQ(report.volumes[1].volume_name, "Full-0001");
  EXPECT_EQ(report.volumes[2].volume_name, "Full-0002");
  ASSERT_EQ(report.status_breakdown.size(), 2u);
  EXPECT_EQ(report.status_breakdown[0].status, "Used");
  EXPECT_EQ(report.status_breakdown[0].volumes, 2);
  EXPECT_EQ(report.status_breakdown[1].status, "Full");
  EXPECT_EQ(report.status_breakdown[1].volumes, 1);
}
