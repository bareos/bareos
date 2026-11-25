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

#include "include/bareos.h"
#include "include/streams.h"
#include "stored/record.h"

#include <gtest/gtest.h>
#include <string>

TEST(RecordToStringTest, DistinguishesReusedFileIndexesAcrossSessions)
{
  storagedaemon::DeviceRecord record;
  record.FileIndex = 1;
  record.Stream = STREAM_FILE_DATA;
  record.maskedStream = STREAM_FILE_DATA;
  record.VolSessionId = 17;
  record.VolSessionTime = 4000000000U;

  PoolMem output(PM_MESSAGE);
  const std::string first
      = storagedaemon::record_to_str(output, nullptr, &record);
  EXPECT_NE(first.find("VolSessionId=17 VolSessionTime=4000000000"),
            std::string::npos);

  record.VolSessionId = 18;
  const std::string different_id
      = storagedaemon::record_to_str(output, nullptr, &record);
  EXPECT_NE(first, different_id);

  record.VolSessionId = 17;
  record.VolSessionTime++;
  const std::string different_time
      = storagedaemon::record_to_str(output, nullptr, &record);
  EXPECT_NE(first, different_time);
}
