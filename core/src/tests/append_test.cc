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

#include "gtest/gtest.h"

#include "stored/append.h"
#include "include/streams.h"

namespace {
storagedaemon::DeviceRecord MakeRecord(int32_t fileindex, int32_t stream)
{
  storagedaemon::DeviceRecord dr{};
  dr.FileIndex = fileindex;
  dr.Stream = stream;
  static char dummy_data[] = "x";
  dr.data = dummy_data;
  dr.data_len = 1;
  return dr;
}
} /* namespace */

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

TEST(AppendProcessedFileTest, IsUnixAttributeStreamRecognizesAttributeStreams)
{
  EXPECT_TRUE(storagedaemon::IsUnixAttributeStream(STREAM_UNIX_ATTRIBUTES));
  EXPECT_TRUE(storagedaemon::IsUnixAttributeStream(STREAM_UNIX_ATTRIBUTES_EX));
  EXPECT_FALSE(storagedaemon::IsUnixAttributeStream(STREAM_MD5_DIGEST));
}

TEST(AppendProcessedFileTest, SelectAttributesToSendKeepsOnlyLastUnixAttribute)
{
  storagedaemon::DeviceRecord original_attrs
      = MakeRecord(1, STREAM_UNIX_ATTRIBUTES);
  storagedaemon::DeviceRecord digest = MakeRecord(1, STREAM_MD5_DIGEST);
  storagedaemon::DeviceRecord corrected_attrs
      = MakeRecord(1, STREAM_UNIX_ATTRIBUTES);

  std::vector<storagedaemon::ProcessedFileData> attributes;
  attributes.emplace_back(&original_attrs);
  attributes.emplace_back(&digest);
  attributes.emplace_back(&corrected_attrs);

  std::vector<bool> to_send = storagedaemon::SelectAttributesToSend(attributes);

  ASSERT_EQ(to_send.size(), 3U);
  EXPECT_FALSE(to_send[0]); /* superseded original attributes */
  EXPECT_TRUE(to_send[1]);  /* digest is never deduplicated */
  EXPECT_TRUE(to_send[2]);  /* the last (corrected) attributes */
}

TEST(AppendProcessedFileTest,
     SelectAttributesToSendSendsSingleUnixAttributeAsIs)
{
  storagedaemon::DeviceRecord attrs = MakeRecord(1, STREAM_UNIX_ATTRIBUTES);

  std::vector<storagedaemon::ProcessedFileData> attributes;
  attributes.emplace_back(&attrs);

  std::vector<bool> to_send = storagedaemon::SelectAttributesToSend(attributes);

  ASSERT_EQ(to_send.size(), 1U);
  EXPECT_TRUE(to_send[0]);
}
