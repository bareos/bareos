/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2022-2022 Bareos GmbH & Co. KG

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
#if defined(HAVE_MINGW)
#  include "include/bareos.h"
#endif

#include "gtest/gtest.h"

#include "stored/append.h"

TEST(AppendProcessedFileTest, ProcessedFileIsEmptyOnInitialization)
{
  storagedaemon::ProcessedFile file{20};
  EXPECT_EQ(file.GetFileIndex(), 20);
  EXPECT_TRUE(file.GetAttributes().empty());
}

TEST(AppendProcessedFileTest, AddDeviceRecordCopiesDataContentNotPointer)
{
  POOLMEM* test_msg = GetPoolMemory(PM_MESSAGE);
  PmStrcpy(test_msg, "data\0more data");

  storagedaemon::DeviceRecord dr;
  dr.data_len = 14;
  dr.data = test_msg;

  int32_t arbitrary_index{1234};
  storagedaemon::ProcessedFile file{arbitrary_index};
  file.AddAttribute(&dr);

  EXPECT_FALSE(file.GetAttributes().empty());

  storagedaemon::ProcessedFileData processedfiledata
      = file.GetAttributes().front();

  EXPECT_NE(processedfiledata.GetData().data, dr.data);
  EXPECT_EQ(memcmp(processedfiledata.GetData().data, dr.data, dr.data_len), 0);

  FreePoolMemory(test_msg);
}
