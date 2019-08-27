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

static bool callback_called_from_destructor = false;
static void callback(JobControlRecord* jcr)
{
  callback_called_from_destructor = true;
}

TEST(job_control_record, constructor_destructor)
{
  std::shared_ptr<JobControlRecord> jcr(std::make_shared<JobControlRecord>());
  InitJcr(jcr, callback);

  callback_called_from_destructor = false;
  jcr.reset();
  EXPECT_TRUE(callback_called_from_destructor);
}

class JobControlRecordTest : public ::testing::Test {
 public:
  std::vector<std::shared_ptr<JobControlRecord>> jobs;

 private:
  void SetUp() override;
  void TearDown() override;
};

void JobControlRecordTest::SetUp()
{
  for (int i = 0; i < 3; i++) {
    jobs.push_back(std::make_shared<JobControlRecord>());
    InitJcr(jobs[i], callback);
    sprintf(jobs[i]->Job, "%d-test", 123 + i);
    jobs[i]->JobId = 123 + i;
    jobs[i]->VolSessionId = 10 + i;
    jobs[i]->VolSessionTime = 100 + i;
  }
}

void JobControlRecordTest::TearDown()
{
  // explicitly release memory
  jobs.clear();
}

TEST_F(JobControlRecordTest, get_job_by_id)
{
  auto found_jcr = GetJcrById(125);
  EXPECT_EQ(jobs[2].get(), found_jcr.get());
}

TEST_F(JobControlRecordTest, get_job_by_id_expired)
{
  {
    std::shared_ptr<JobControlRecord> jcr(std::make_shared<JobControlRecord>());
    InitJcr(jcr, callback);
    jcr->JobId = 777;

    EXPECT_EQ(GetJcrCount(), 4);
  }

  // check if the above jcr has been deleted from list
  EXPECT_EQ(GetJcrCount(), 3);
  auto jcr_found = GetJcrById(777);
  EXPECT_FALSE(jcr_found.get());
}

TEST_F(JobControlRecordTest, get_job_by_full_name)
{
  char name[12]{"124-test"};
  auto found_jcr = GetJcrByFullName(name);
  EXPECT_EQ(jobs[1].get(), found_jcr.get());
}

TEST_F(JobControlRecordTest, get_job_by_full_name_not_found)
{
  char name[12]{"127"};
  auto found_jcr = GetJcrByFullName(name);
  EXPECT_FALSE(found_jcr.get());
}

TEST_F(JobControlRecordTest, get_job_by_partial_name)
{
  char name[12]{"124"};
  auto found_jcr = GetJcrByPartialName(name);
  EXPECT_EQ(jobs[1].get(), found_jcr.get());
}

TEST_F(JobControlRecordTest, get_job_by_partial_name_not_found)
{
  std::shared_ptr<JobControlRecord> jcr(std::make_shared<JobControlRecord>());
  sprintf(jcr->Job, "987-654-321");
  InitJcr(jcr, callback);

  char name[12]{"654"};
  auto found_jcr = GetJcrByPartialName(name);
  EXPECT_FALSE(found_jcr.get());
}

TEST_F(JobControlRecordTest, get_job_by_session)
{
  auto found_jcr = GetJcrBySession(11, 101);
  EXPECT_EQ(jobs[1].get(), found_jcr.get());
}

TEST_F(JobControlRecordTest, get_job_by_session_not_found)
{
  auto found_jcr = GetJcrBySession(12, 101);
  EXPECT_FALSE(found_jcr.get());

  found_jcr = GetJcrBySession(11, 103);
  EXPECT_FALSE(found_jcr.get());
}
