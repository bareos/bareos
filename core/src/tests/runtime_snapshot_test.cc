/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2026-2026 Bareos GmbH & Co. KG

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

#include "gtest/gtest.h"

#include "dird/runtime_snapshot.h"

#include <string>

using directordaemon::ParseStorageRuntimeSnapshotRecord;

TEST(RuntimeSnapshotTest, ParsesBashedRuntimeRecord)
{
  JobId_t jobid = 0;
  StorageRuntimeSnapshot snapshot;
  std::string record
      = "JobId=42 JobStatus=R SampleTime=1700000000 JobFiles=11 JobBytes=1234 "
        "AverageRate=222 LastRate=333 Spooling=1 Despooling=0 DespoolWait=0 "
        "ReadDevice=*None* WriteDevice=File";
  record.push_back('\x01');
  record += "Storage ReadVolume=*None* WriteVolume=Vol001 Pool=Full";
  record.push_back('\x01');
  record += "Pool CurrentFile=/srv/data/file";
  record.push_back('\x01');
  record += "1";

  ASSERT_TRUE(
      ParseStorageRuntimeSnapshotRecord(record.c_str(), &jobid, &snapshot));

  EXPECT_EQ(jobid, 42u);
  EXPECT_TRUE(snapshot.valid);
  EXPECT_EQ(snapshot.job_status, 'R');
  EXPECT_EQ(snapshot.sample_time, 1700000000);
  EXPECT_EQ(snapshot.job_files, 11u);
  EXPECT_EQ(snapshot.job_bytes, 1234u);
  EXPECT_EQ(snapshot.average_rate, 222u);
  EXPECT_EQ(snapshot.last_rate, 333u);
  EXPECT_TRUE(snapshot.spooling);
  EXPECT_FALSE(snapshot.despooling);
  EXPECT_FALSE(snapshot.despool_wait);
  EXPECT_EQ(snapshot.read_device, "*None*");
  EXPECT_EQ(snapshot.write_device, "File Storage");
  EXPECT_EQ(snapshot.write_volume, "Vol001");
  EXPECT_EQ(snapshot.pool_name, "Full Pool");
  EXPECT_EQ(snapshot.current_file, "/srv/data/file 1");
}

TEST(RuntimeSnapshotTest, RejectsMalformedRuntimeRecord)
{
  JobId_t jobid = 0;
  StorageRuntimeSnapshot snapshot;

  EXPECT_FALSE(ParseStorageRuntimeSnapshotRecord("JobId=42 broken",
                                                 &jobid, &snapshot));
}
