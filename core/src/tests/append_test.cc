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
#  include "gtest/gtest.h"
#else
#  include "gtest/gtest.h"
#  include "gmock/gmock.h"
#endif

#include "stored/append.h"


class AppendProcessedFileTest : public testing::Test {
 public:
  int32_t arbitrary_index{1};
  storagedaemon::ProcessedFile file{};
  void SetUp() override { file.Initialize(arbitrary_index); }
};

TEST_F(AppendProcessedFileTest, ProcessedFileIsEmptyOnInitialization)
{
  file.Initialize(20);
  EXPECT_EQ(file.GetFileIndex(), 20);
  EXPECT_TRUE(file.GetAttributes().empty());
}

TEST_F(AppendProcessedFileTest, AddDeviceRecordCopiesDataContentNotPointer)
{
  POOLMEM* test_msg = GetPoolMemory(PM_MESSAGE);
  PmStrcpy(test_msg, "a random message");

  storagedaemon::DeviceRecord dr;
  dr.data = test_msg;
  dr.data_len = strlen(test_msg) - 1;
  file.AddAttribute(&dr);

  EXPECT_FALSE(file.GetAttributes().empty());
  EXPECT_NE(file.GetAttributes().front().data, dr.data);
  EXPECT_EQ(*file.GetAttributes().front().data, *dr.data);

  FreePoolMemory(test_msg);
}
