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
#include "include/make_unique.h"
#include "lib/thread_specific_data.h"

#include <thread>

TEST(thread_specific_data, setup_and_readout_data_one_thread)
{
  std::unique_ptr<JobControlRecord> jcr(std::make_unique<JobControlRecord>());
  jcr->JobId = 123;

  SetJcrInThreadSpecificData(jcr.get());

  JobControlRecord* jcr_same = GetJcrFromThreadSpecificData();
  EXPECT_EQ(jcr.get(), jcr_same);

  uint32_t jobid = GetJobIdFromThreadSpecificData();
  EXPECT_EQ(jobid, 123);

  RemoveJcrFromThreadSpecificData(jcr.get());
  JobControlRecord* jcr_invalid = GetJcrFromThreadSpecificData();
  EXPECT_EQ(jcr_invalid, nullptr);
}

static std::vector<uint32_t> thread_jobids(100);

static void test_thread(uint32_t index)
{
  std::unique_ptr<JobControlRecord> jcr(std::make_unique<JobControlRecord>());
  jcr->JobId = 123 + index;

  SetJcrInThreadSpecificData(jcr.get());
  std::this_thread::sleep_for(std::chrono::milliseconds(3));

  JobControlRecord* jcr_same = GetJcrFromThreadSpecificData();
  EXPECT_EQ(jcr.get(), jcr_same);

  uint32_t jobid = GetJobIdFromThreadSpecificData();

  thread_jobids[index] = jobid;
}

TEST(thread_specific_data, setup_and_readout_data_multiple_threads)
{
  std::vector<std::thread> threads;

  for (std::size_t i = 0; i < thread_jobids.size(); i++) {
    threads.emplace_back(test_thread, i);
  }

  for (std::size_t i = 0; i < thread_jobids.size(); i++) {
    threads[i].join();
    EXPECT_EQ(thread_jobids[i], 123 + i);
  }
}
