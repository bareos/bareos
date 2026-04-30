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

TEST(Pruning, ExcludeRunningJobsFromList)
{
  InitDirGlobals();
  std::string path_to_config = std::string("configs/pruning/");
  PConfigParser director_config(DirectorPrepareResources(path_to_config));
  if (!director_config) { return; }

  JobControlRecord* jcr1
      = directordaemon::NewDirectorJcr(directordaemon::DirdFreeJcr);
  jcr1->JobId = 1;
  JobControlRecord* jcr2
      = directordaemon::NewDirectorJcr(directordaemon::DirdFreeJcr);
  jcr2->JobId = 2;
  JobControlRecord* jcr3
      = directordaemon::NewDirectorJcr(directordaemon::DirdFreeJcr);
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

  JobControlRecord* jcr1
      = directordaemon::NewDirectorJcr(directordaemon::DirdFreeJcr);
  jcr1->JobId = 1;

  directordaemon::UaContext* ua = directordaemon::new_ua_context(jcr1);
  std::vector<JobId_t> pruninglist{0, 1, 2, 0, 3, 4, 5};
  std::string jobids_to_delete
      = directordaemon::PrepareJobidsTobedeleted(ua, pruninglist);

  EXPECT_EQ(jobids_to_delete, "2,3,4,5");
  EXPECT_EQ(pruninglist.size(), 4);

  FreeUaContext(ua);
}

TEST(Pruning, ExcludeDependentJobsFromList)
{
  std::vector<JobId_t> pruninglist{1, 2};
  std::vector<directordaemon::JobContentHistoryRecord> jobs{
      {.JobId = 1, .BaseId = 0, .ContentId = 1},
      {.JobId = 2, .BaseId = 1, .ContentId = 2},
      {.JobId = 3, .BaseId = 2, .ContentId = 3},
  };

  int NumJobsToBePruned
      = directordaemon::ExcludeDependentJobsFromList(pruninglist, jobs);

  EXPECT_EQ(NumJobsToBePruned, 0);
  EXPECT_TRUE(pruninglist.empty());
}

TEST(Pruning, ExcludeDependentJobsAllowsCopies)
{
  std::vector<JobId_t> pruninglist{1};
  std::vector<directordaemon::JobContentHistoryRecord> jobs{
      {.JobId = 1, .BaseId = 0, .ContentId = 6},
      {.JobId = 2, .BaseId = 0, .ContentId = 6},
      {.JobId = 3, .BaseId = 6, .ContentId = 7},
  };

  int NumJobsToBePruned
      = directordaemon::ExcludeDependentJobsFromList(pruninglist, jobs);

  EXPECT_EQ(NumJobsToBePruned, 1);
  EXPECT_EQ(pruninglist[0], 1);
}

TEST(Pruning, ExcludeJobsByKeepCountProtectsNewestCandidates)
{
  std::vector<JobId_t> pruninglist{1, 2, 3};
  std::vector<directordaemon::JobKeepCountRecord> jobs{
      {.JobId = 1,
       .Name = "backup",
       .ClientId = 1,
       .FileSetId = 1,
       .JobTDate = 10,
       .KeepNumber = 2},
      {.JobId = 2,
       .Name = "backup",
       .ClientId = 1,
       .FileSetId = 1,
       .JobTDate = 20,
       .KeepNumber = 2},
      {.JobId = 3,
       .Name = "backup",
       .ClientId = 1,
       .FileSetId = 1,
       .JobTDate = 30,
       .KeepNumber = 2},
      {.JobId = 4,
       .Name = "backup",
       .ClientId = 1,
       .FileSetId = 1,
       .JobTDate = 40,
       .KeepNumber = 2},
  };

  int NumJobsToBePruned
      = directordaemon::ExcludeJobsByKeepCount(pruninglist, jobs);

  EXPECT_EQ(NumJobsToBePruned, 2);
  ASSERT_EQ(pruninglist.size(), 2U);
  EXPECT_EQ(pruninglist[0], 1);
  EXPECT_EQ(pruninglist[1], 2);
}

TEST(Pruning, ExcludeJobsByKeepCountSeparatesGroups)
{
  std::vector<JobId_t> pruninglist{1, 2, 5};
  std::vector<directordaemon::JobKeepCountRecord> jobs{
      {.JobId = 1,
       .Name = "backup-a",
       .ClientId = 1,
       .FileSetId = 1,
       .JobTDate = 10,
       .KeepNumber = 2},
      {.JobId = 2,
       .Name = "backup-a",
       .ClientId = 1,
       .FileSetId = 1,
       .JobTDate = 20,
       .KeepNumber = 2},
      {.JobId = 3,
       .Name = "backup-a",
       .ClientId = 1,
       .FileSetId = 1,
       .JobTDate = 30,
       .KeepNumber = 2},
      {.JobId = 5,
       .Name = "backup-b",
       .ClientId = 1,
       .FileSetId = 1,
       .JobTDate = 15,
       .KeepNumber = 1},
      {.JobId = 6,
       .Name = "backup-b",
       .ClientId = 1,
       .FileSetId = 1,
       .JobTDate = 25,
       .KeepNumber = 1},
  };

  int NumJobsToBePruned
      = directordaemon::ExcludeJobsByKeepCount(pruninglist, jobs);

  EXPECT_EQ(NumJobsToBePruned, 2);
  ASSERT_EQ(pruninglist.size(), 2U);
  EXPECT_EQ(pruninglist[0], 1);
  EXPECT_EQ(pruninglist[1], 5);
}

TEST(Pruning, ExcludeJobsByKeepCountProtectsAtFloor)
{
  std::vector<JobId_t> pruninglist{2};
  std::vector<directordaemon::JobKeepCountRecord> jobs{
      {.JobId = 2,
       .Name = "backup",
       .ClientId = 1,
       .FileSetId = 1,
       .JobTDate = 20,
       .KeepNumber = 2},
      {.JobId = 3,
       .Name = "backup",
       .ClientId = 1,
       .FileSetId = 1,
       .JobTDate = 30,
       .KeepNumber = 2},
  };

  int NumJobsToBePruned
      = directordaemon::ExcludeJobsByKeepCount(pruninglist, jobs);

  EXPECT_EQ(NumJobsToBePruned, 0);
  EXPECT_TRUE(pruninglist.empty());
}
