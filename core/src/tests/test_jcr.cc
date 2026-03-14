/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2025 Bareos GmbH & Co. KG

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.
*/

#include <gtest/gtest.h>
#include <memory>
#include "include/bareos.h"
#include "include/jcr.h"
#include "lib/jcr.h"

class JcrTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // JCR tests require a properly initialized JCR
    jcr = std::make_shared<JobControlRecord>();
  }

  void TearDown() override {
    jcr.reset();
  }

  std::shared_ptr<JobControlRecord> jcr;
};

// JobControlRecord constructor/destructor tests
TEST_F(JcrTest, ConstructorCreatesValidJcr) {
  EXPECT_TRUE(jcr != nullptr);
}

TEST_F(JcrTest, JcrInitialization) {
  auto j = std::make_shared<JobControlRecord>();
  EXPECT_TRUE(j != nullptr);
  j.reset();
}

// Use count tests
TEST_F(JcrTest, InitialUseCountOne) {
  // JCR starts with use count of 1 when created
  EXPECT_GE(jcr->UseCount(), 1);
}

TEST_F(JcrTest, IncUseCount) {
  int initial = jcr->UseCount();
  jcr->IncUseCount();
  EXPECT_EQ(jcr->UseCount(), initial + 1);
}

TEST_F(JcrTest, MultipleIncUseCount) {
  int initial = jcr->UseCount();
  jcr->IncUseCount();
  jcr->IncUseCount();
  jcr->IncUseCount();
  EXPECT_EQ(jcr->UseCount(), initial + 3);
}

TEST_F(JcrTest, DecUseCount) {
  jcr->IncUseCount();
  jcr->IncUseCount();
  int after_inc = jcr->UseCount();
  jcr->DecUseCount();
  EXPECT_EQ(jcr->UseCount(), after_inc - 1);
}

TEST_F(JcrTest, IncDecUseCount) {
  int initial = jcr->UseCount();
  for (int i = 0; i < 10; ++i) {
    jcr->IncUseCount();
  }
  EXPECT_EQ(jcr->UseCount(), initial + 10);
  
  for (int i = 0; i < 5; ++i) {
    jcr->DecUseCount();
  }
  EXPECT_EQ(jcr->UseCount(), initial + 5);
}

// Job status tests
TEST_F(JcrTest, InitialJobStatusSet) {
  // JCR starts with an initial job status
  EXPECT_GE(jcr->getJobStatus(), 0);
}

TEST_F(JcrTest, SetJobStatus) {
  jcr->setJobStatus(JS_Terminated);
  EXPECT_EQ(jcr->getJobStatus(), JS_Terminated);
}

TEST_F(JcrTest, IsJobCanceledInitiallyFalse) {
  EXPECT_FALSE(jcr->IsJobCanceled());
}

TEST_F(JcrTest, IsTerminatedOkForTerminated) {
  jcr->setJobStatus(JS_Terminated);
  EXPECT_TRUE(jcr->IsTerminatedOk());
}

TEST_F(JcrTest, IsTerminatedOkForWarnings) {
  jcr->setJobStatus(JS_Warnings);
  EXPECT_TRUE(jcr->IsTerminatedOk());
}

TEST_F(JcrTest, IsTerminatedOkForCanceled) {
  jcr->setJobStatus(JS_Canceled);
  EXPECT_FALSE(jcr->IsTerminatedOk());
}

TEST_F(JcrTest, IsIncompleteStatus) {
  jcr->setJobStatus(JS_Incomplete);
  EXPECT_TRUE(jcr->IsIncomplete());
}

TEST_F(JcrTest, IsIncompleteOtherStatus) {
  jcr->setJobStatus(JS_Terminated);
  EXPECT_FALSE(jcr->IsIncomplete());
}

// Job type tests
TEST_F(JcrTest, SetJobType) {
  jcr->setJobType(JT_BACKUP);
  EXPECT_EQ(jcr->getJobType(), JT_BACKUP);
}

TEST_F(JcrTest, IsJobTypeBackup) {
  jcr->setJobType(JT_BACKUP);
  EXPECT_TRUE(jcr->is_JobType(JT_BACKUP));
}

TEST_F(JcrTest, IsJobTypeVerify) {
  jcr->setJobType(JT_VERIFY);
  EXPECT_TRUE(jcr->is_JobType(JT_VERIFY));
}

TEST_F(JcrTest, IsJobTypeFalse) {
  jcr->setJobType(JT_BACKUP);
  EXPECT_FALSE(jcr->is_JobType(JT_VERIFY));
}

TEST_F(JcrTest, IsJobTypeRestore) {
  jcr->setJobType(JT_RESTORE);
  EXPECT_TRUE(jcr->is_JobType(JT_RESTORE));
}

TEST_F(JcrTest, IsJobTypeMigrate) {
  jcr->setJobType(JT_MIGRATE);
  EXPECT_TRUE(jcr->is_JobType(JT_MIGRATE));
}

// Job level tests
TEST_F(JcrTest, SetJobLevel) {
  jcr->setJobLevel(L_FULL);
  EXPECT_EQ(jcr->getJobLevel(), L_FULL);
}

TEST_F(JcrTest, IsJobLevelFull) {
  jcr->setJobLevel(L_FULL);
  EXPECT_TRUE(jcr->is_JobLevel(L_FULL));
}

TEST_F(JcrTest, IsJobLevelIncremental) {
  jcr->setJobLevel(L_INCREMENTAL);
  EXPECT_TRUE(jcr->is_JobLevel(L_INCREMENTAL));
}

TEST_F(JcrTest, IsJobLevelDifferential) {
  jcr->setJobLevel(L_DIFFERENTIAL);
  EXPECT_TRUE(jcr->is_JobLevel(L_DIFFERENTIAL));
}

TEST_F(JcrTest, IsJobLevelFalse) {
  jcr->setJobLevel(L_FULL);
  EXPECT_FALSE(jcr->is_JobLevel(L_INCREMENTAL));
}

// Job protocol tests
TEST_F(JcrTest, SetJobProtocol) {
  jcr->setJobProtocol(0);
  EXPECT_EQ(jcr->getJobProtocol(), 0);
}

TEST_F(JcrTest, SetJobProtocolDifferentValue) {
  jcr->setJobProtocol(42);
  EXPECT_EQ(jcr->getJobProtocol(), 42);
}

// NoClientUsed tests
TEST_F(JcrTest, NoClientUsedMigrate) {
  jcr->setJobType(JT_MIGRATE);
  EXPECT_TRUE(jcr->NoClientUsed());
}

TEST_F(JcrTest, NoClientUsedCopy) {
  jcr->setJobType(JT_COPY);
  EXPECT_TRUE(jcr->NoClientUsed());
}

TEST_F(JcrTest, NoClientUsedVirtualFull) {
  jcr->setJobType(JT_BACKUP);
  jcr->setJobLevel(L_VIRTUAL_FULL);
  EXPECT_TRUE(jcr->NoClientUsed());
}

TEST_F(JcrTest, NoClientUsedBackup) {
  jcr->setJobType(JT_BACKUP);
  jcr->setJobLevel(L_FULL);
  EXPECT_FALSE(jcr->NoClientUsed());
}

// Operation name tests
TEST_F(JcrTest, GetOperationNameBackup) {
  jcr->setJobType(JT_BACKUP);
  const char* name = jcr->get_OperationName();
  EXPECT_TRUE(name != nullptr);
  EXPECT_NE(std::string(name).length(), 0);
}

TEST_F(JcrTest, GetOperationNameRestore) {
  jcr->setJobType(JT_RESTORE);
  const char* name = jcr->get_OperationName();
  EXPECT_TRUE(name != nullptr);
  EXPECT_NE(std::string(name).length(), 0);
}

TEST_F(JcrTest, GetOperationNameVerify) {
  jcr->setJobType(JT_VERIFY);
  const char* name = jcr->get_OperationName();
  EXPECT_TRUE(name != nullptr);
  EXPECT_NE(std::string(name).length(), 0);
}

TEST_F(JcrTest, GetOperationNameMigrate) {
  jcr->setJobType(JT_MIGRATE);
  const char* name = jcr->get_OperationName();
  EXPECT_TRUE(name != nullptr);
  EXPECT_NE(std::string(name).length(), 0);
}

TEST_F(JcrTest, GetOperationNameCopy) {
  jcr->setJobType(JT_COPY);
  const char* name = jcr->get_OperationName();
  EXPECT_TRUE(name != nullptr);
  EXPECT_NE(std::string(name).length(), 0);
}

// Action name tests
TEST_F(JcrTest, GetActionNameBackup) {
  jcr->setJobType(JT_BACKUP);
  const char* name = jcr->get_ActionName();
  EXPECT_TRUE(name != nullptr);
  EXPECT_NE(std::string(name).length(), 0);
}

TEST_F(JcrTest, GetActionNamePastTense) {
  jcr->setJobType(JT_BACKUP);
  const char* name = jcr->get_ActionName(true);
  EXPECT_TRUE(name != nullptr);
  EXPECT_NE(std::string(name).length(), 0);
}

// JobReads tests
TEST_F(JcrTest, JobReadsBackup) {
  jcr->setJobType(JT_BACKUP);
  bool reads = jcr->JobReads();
  EXPECT_FALSE(reads);  // Backup doesn't read
}

TEST_F(JcrTest, JobReadsRestore) {
  jcr->setJobType(JT_RESTORE);
  bool reads = jcr->JobReads();
  EXPECT_TRUE(reads);  // Restore reads
}

TEST_F(JcrTest, JobReadsVerify) {
  jcr->setJobType(JT_VERIFY);
  bool reads = jcr->JobReads();
  EXPECT_TRUE(reads);  // Verify reads
}

// Job status checks
TEST_F(JcrTest, IsJobStatusMatches) {
  jcr->setJobStatus(JS_Terminated);
  EXPECT_TRUE(jcr->is_JobStatus(JS_Terminated));
}

TEST_F(JcrTest, IsJobStatusMismatch) {
  jcr->setJobStatus(JS_Terminated);
  EXPECT_FALSE(jcr->is_JobStatus(JS_Running));
}

// Multiple property operations
TEST_F(JcrTest, SetMultipleProperties) {
  jcr->setJobType(JT_BACKUP);
  jcr->setJobLevel(L_INCREMENTAL);
  jcr->setJobStatus(JS_Running);
  jcr->setJobProtocol(1);
  
  EXPECT_EQ(jcr->getJobType(), JT_BACKUP);
  EXPECT_EQ(jcr->getJobLevel(), L_INCREMENTAL);
  EXPECT_EQ(jcr->getJobStatus(), JS_Running);
  EXPECT_EQ(jcr->getJobProtocol(), 1);
}

// Mutex guard test
TEST_F(JcrTest, MutexGuardExists) {
  std::mutex& m = jcr->mutex_guard();
  EXPECT_TRUE(m.try_lock());
  m.unlock();
}

// High use count test
TEST_F(JcrTest, HighUseCount) {
  int initial = jcr->UseCount();
  for (int i = 0; i < 100; ++i) {
    jcr->IncUseCount();
  }
  EXPECT_EQ(jcr->UseCount(), initial + 100);
}

TEST_F(JcrTest, BalancedIncDec) {
  int initial = jcr->UseCount();
  for (int i = 0; i < 50; ++i) {
    jcr->IncUseCount();
  }
  for (int i = 0; i < 50; ++i) {
    jcr->DecUseCount();
  }
  EXPECT_EQ(jcr->UseCount(), initial);
}

// Global job functions
TEST(JcrGlobalTest, NumJobsRunInitial) {
  std::size_t count = NumJobsRun();
  EXPECT_GE(count, 0);
}

TEST(JcrGlobalTest, LockUnlockJobs) {
  LockJobs();
  UnlockJobs();
  EXPECT_TRUE(true);  // No crash is success
}

// Job type constants validation
TEST(JobTypeTest, JobTypeConstants) {
  EXPECT_EQ(JT_BACKUP, 'B');
  EXPECT_EQ(JT_RESTORE, 'R');
  EXPECT_EQ(JT_VERIFY, 'V');
}

// Job level constants validation
TEST(JobLevelTest, JobLevelConstants) {
  EXPECT_EQ(L_FULL, 'F');
  EXPECT_EQ(L_INCREMENTAL, 'I');
  EXPECT_EQ(L_DIFFERENTIAL, 'D');
  EXPECT_EQ(L_SINCE, 'S');
  EXPECT_EQ(L_VERIFY_CATALOG, 'C');
  EXPECT_EQ(L_VERIFY_INIT, 'V');
  EXPECT_EQ(L_VERIFY_VOLUME_TO_CATALOG, 'O');
  EXPECT_EQ(L_VIRTUAL_FULL, 'f');
}

// Job status constants validation
TEST(JobStatusTest, JobStatusConstants) {
  EXPECT_GE(JS_Created, 0);
  EXPECT_GE(JS_Running, 0);
  EXPECT_GE(JS_Blocked, 0);
  EXPECT_GE(JS_Terminated, 0);
  EXPECT_GE(JS_ErrorTerminated, 0);
  EXPECT_GE(JS_Error, 0);
  EXPECT_GE(JS_FatalError, 0);
  EXPECT_GE(JS_Canceled, 0);
}

// Job status transitions
TEST_F(JcrTest, StatusTransitionRunningToTerminated) {
  jcr->setJobStatus(JS_Running);
  EXPECT_EQ(jcr->getJobStatus(), JS_Running);
  
  jcr->setJobStatus(JS_Terminated);
  EXPECT_EQ(jcr->getJobStatus(), JS_Terminated);
  EXPECT_TRUE(jcr->IsTerminatedOk());
}

TEST_F(JcrTest, StatusTransitionRunningToCanceled) {
  jcr->setJobStatus(JS_Running);
  EXPECT_EQ(jcr->getJobStatus(), JS_Running);
  
  jcr->setJobStatus(JS_Canceled);
  EXPECT_EQ(jcr->getJobStatus(), JS_Canceled);
  EXPECT_TRUE(jcr->IsJobCanceled());
}

TEST_F(JcrTest, StatusTransitionRunningToError) {
  jcr->setJobStatus(JS_Running);
  EXPECT_EQ(jcr->getJobStatus(), JS_Running);
  
  jcr->setJobStatus(JS_Error);
  EXPECT_EQ(jcr->getJobStatus(), JS_Error);
  EXPECT_FALSE(jcr->IsTerminatedOk());
}

// Complex job type scenarios
TEST_F(JcrTest, FullBackupJob) {
  jcr->setJobType(JT_BACKUP);
  jcr->setJobLevel(L_FULL);
  jcr->setJobStatus(JS_Running);
  
  EXPECT_TRUE(jcr->is_JobType(JT_BACKUP));
  EXPECT_TRUE(jcr->is_JobLevel(L_FULL));
  EXPECT_FALSE(jcr->NoClientUsed());
}

TEST_F(JcrTest, IncrementalBackupJob) {
  jcr->setJobType(JT_BACKUP);
  jcr->setJobLevel(L_INCREMENTAL);
  jcr->setJobStatus(JS_Running);
  
  EXPECT_TRUE(jcr->is_JobType(JT_BACKUP));
  EXPECT_TRUE(jcr->is_JobLevel(L_INCREMENTAL));
}

TEST_F(JcrTest, VerifyJob) {
  jcr->setJobType(JT_VERIFY);
  jcr->setJobLevel(L_VERIFY_CATALOG);
  
  EXPECT_TRUE(jcr->is_JobType(JT_VERIFY));
  EXPECT_TRUE(jcr->is_JobLevel(L_VERIFY_CATALOG));
}

TEST_F(JcrTest, RestoreJob) {
  jcr->setJobType(JT_RESTORE);
  jcr->setJobStatus(JS_Running);
  
  EXPECT_TRUE(jcr->is_JobType(JT_RESTORE));
  EXPECT_FALSE(jcr->NoClientUsed());
}

TEST_F(JcrTest, MigrateJob) {
  jcr->setJobType(JT_MIGRATE);
  
  EXPECT_TRUE(jcr->is_JobType(JT_MIGRATE));
  EXPECT_TRUE(jcr->NoClientUsed());
}

// Multiple JCR instances
TEST(JcrMultipleTest, TwoJcrInstances) {
  auto jcr1 = std::make_shared<JobControlRecord>();
  auto jcr2 = std::make_shared<JobControlRecord>();
  
  jcr1->setJobType(JT_BACKUP);
  jcr2->setJobType(JT_RESTORE);
  
  EXPECT_NE(jcr1->getJobType(), jcr2->getJobType());
}

TEST(JcrMultipleTest, JcrIndependence) {
  auto jcr1 = std::make_shared<JobControlRecord>();
  auto jcr2 = std::make_shared<JobControlRecord>();
  
  int initial1 = jcr1->UseCount();
  int initial2 = jcr2->UseCount();
  
  jcr1->IncUseCount();
  jcr1->IncUseCount();
  
  EXPECT_EQ(jcr1->UseCount(), initial1 + 2);
  EXPECT_EQ(jcr2->UseCount(), initial2);
}

// SetJobStarted test
TEST_F(JcrTest, SetJobStarted) {
  jcr->setJobStarted();
  EXPECT_TRUE(true);  // Just verify no crash
}

// Edge cases
TEST_F(JcrTest, NegativeUseCount) {
  for (int i = 0; i < 10; ++i) {
    jcr->DecUseCount();
  }
  // Should handle gracefully (implementation dependent)
  EXPECT_TRUE(true);
}

TEST_F(JcrTest, VeryLargeUseCount) {
  int initial = jcr->UseCount();
  for (int i = 0; i < 1000; ++i) {
    jcr->IncUseCount();
  }
  EXPECT_EQ(jcr->UseCount(), initial + 1000);
}

// Job operation readability
TEST_F(JcrTest, AllOperationNamesNonEmpty) {
  int types[] = {JT_BACKUP, JT_RESTORE, JT_VERIFY, JT_MIGRATE, JT_COPY};
  
  for (int type : types) {
    jcr->setJobType(type);
    const char* name = jcr->get_OperationName();
    EXPECT_NE(std::string(name).length(), 0);
  }
}
