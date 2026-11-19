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

#include <string>
#include <vector>

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

  std::vector<std::size_t> to_send
      = storagedaemon::SelectAttributesToSend(attributes);

  /* The digest must arrive after the attribute record that creates the
   * file row, so the corrected attributes are emitted in the first
   * attribute's slot: [corrected_attrs, digest]. */
  ASSERT_EQ(to_send.size(), 2U);
  EXPECT_EQ(to_send[0], 2U); /* the last (corrected) attributes */
  EXPECT_EQ(to_send[1], 1U); /* digest is never deduplicated */
}

TEST(AppendProcessedFileTest, SelectAttributesToSendKeepsDigestsInOriginalOrder)
{
  storagedaemon::DeviceRecord attrs = MakeRecord(1, STREAM_UNIX_ATTRIBUTES);
  storagedaemon::DeviceRecord md5 = MakeRecord(1, STREAM_MD5_DIGEST);
  storagedaemon::DeviceRecord sha1 = MakeRecord(1, STREAM_SHA1_DIGEST);
  storagedaemon::DeviceRecord corrected_attrs
      = MakeRecord(1, STREAM_UNIX_ATTRIBUTES);

  std::vector<storagedaemon::ProcessedFileData> attributes;
  attributes.emplace_back(&attrs);
  attributes.emplace_back(&md5);
  attributes.emplace_back(&sha1);
  attributes.emplace_back(&corrected_attrs);

  std::vector<std::size_t> to_send
      = storagedaemon::SelectAttributesToSend(attributes);

  /* Both digests survive in their original relative order, and the
   * corrected attributes are emitted first. */
  ASSERT_EQ(to_send.size(), 3U);
  EXPECT_EQ(to_send[0], 3U); /* corrected attributes */
  EXPECT_EQ(to_send[1], 1U); /* md5 stays where it was */
  EXPECT_EQ(to_send[2], 2U); /* sha1 stays where it was */
}

TEST(AppendProcessedFileTest,
     SelectAttributesToSendSendsSingleUnixAttributeAsIs)
{
  storagedaemon::DeviceRecord attrs = MakeRecord(1, STREAM_UNIX_ATTRIBUTES);

  std::vector<storagedaemon::ProcessedFileData> attributes;
  attributes.emplace_back(&attrs);

  std::vector<std::size_t> to_send
      = storagedaemon::SelectAttributesToSend(attributes);

  ASSERT_EQ(to_send.size(), 1U);
  EXPECT_EQ(to_send[0], 0U);
}

TEST(AppendProcessedFileTest,
     SelectAttributesToSendPassesThroughSingleAttributeWithDigest)
{
  /* The common (non-resend) case: exactly one attribute record followed
   * by a digest, as read from the medium for a regular file. This is the
   * baseline bscan/bextract/bls must not disturb when no plugin
   * corrected-attributes resend is involved. */
  storagedaemon::DeviceRecord attrs = MakeRecord(1, STREAM_UNIX_ATTRIBUTES);
  storagedaemon::DeviceRecord md5 = MakeRecord(1, STREAM_MD5_DIGEST);

  std::vector<storagedaemon::ProcessedFileData> attributes;
  attributes.emplace_back(&attrs);
  attributes.emplace_back(&md5);

  std::vector<std::size_t> to_send
      = storagedaemon::SelectAttributesToSend(attributes);

  ASSERT_EQ(to_send.size(), 2U);
  EXPECT_EQ(to_send[0], 0U); /* attributes */
  EXPECT_EQ(to_send[1], 1U); /* digest */
}

TEST(AppendProcessedFileTest, AttributeGroupBufferFlushesPerFileIndex)
{
  std::vector<std::pair<int32_t, int32_t>> applied;  // FileIndex, stream
  storagedaemon::AttributeGroupBuffer buffer(
      [&applied](storagedaemon::DeviceRecord& rec) {
        applied.emplace_back(rec.FileIndex, rec.maskedStream);
      });

  auto add = [&buffer](int32_t fileindex, int32_t stream) {
    storagedaemon::DeviceRecord rec = MakeRecord(fileindex, stream);
    rec.maskedStream = stream;
    buffer.Add(&rec);
  };

  add(1, STREAM_UNIX_ATTRIBUTES);
  add(1, STREAM_MD5_DIGEST);
  add(1, STREAM_UNIX_ATTRIBUTES);
  EXPECT_TRUE(applied.empty());

  // A new FileIndex flushes the previous file: corrected attributes first.
  add(2, STREAM_UNIX_ATTRIBUTES);
  ASSERT_EQ(applied.size(), 2U);
  EXPECT_EQ(applied[0], std::make_pair(1, int32_t{STREAM_UNIX_ATTRIBUTES}));
  EXPECT_EQ(applied[1], std::make_pair(1, int32_t{STREAM_MD5_DIGEST}));

  buffer.FlushAll();
  ASSERT_EQ(applied.size(), 3U);
  EXPECT_EQ(applied[2].first, 2);
}

TEST(AppendProcessedFileTest, AttributeGroupBufferSelectsLatestAttributes)
{
  std::vector<std::pair<int32_t, char>> applied;
  storagedaemon::AttributeGroupBuffer buffer(
      [&applied](storagedaemon::DeviceRecord& rec) {
        applied.emplace_back(rec.maskedStream, rec.data[0]);
      });

  char original_data[] = "original";
  char digest_data[] = "digest";
  char corrected_data[] = "corrected";
  auto add = [&buffer](storagedaemon::DeviceRecord& record, char* data,
                       uint32_t data_len) {
    record.data = data;
    record.data_len = data_len;
    record.maskedStream = record.Stream;
    buffer.Add(&record);
  };

  auto original = MakeRecord(1, STREAM_UNIX_ATTRIBUTES);
  auto digest = MakeRecord(1, STREAM_MD5_DIGEST);
  auto corrected = MakeRecord(1, STREAM_UNIX_ATTRIBUTES);
  auto next_file = MakeRecord(2, STREAM_UNIX_ATTRIBUTES);
  add(original, original_data, sizeof(original_data));
  add(digest, digest_data, sizeof(digest_data));
  add(corrected, corrected_data, sizeof(corrected_data));
  add(next_file, original_data, sizeof(original_data));
  buffer.FlushAll();

  ASSERT_EQ(applied.size(), 3U);
  EXPECT_EQ(applied[0], std::make_pair(int32_t{STREAM_UNIX_ATTRIBUTES}, 'c'));
  EXPECT_EQ(applied[1], std::make_pair(int32_t{STREAM_MD5_DIGEST}, 'd'));
  EXPECT_EQ(applied[2], std::make_pair(int32_t{STREAM_UNIX_ATTRIBUTES}, 'o'));
}

TEST(AppendProcessedFileTest,
     AttributeGroupBufferFlushesBeforeRestoreObjectForSession)
{
  std::vector<std::string> processed;
  storagedaemon::AttributeGroupBuffer buffer(
      [&processed](storagedaemon::DeviceRecord& rec) {
        processed.emplace_back("attributes:" + std::to_string(rec.FileIndex));
      });

  auto same_session_attrs = MakeRecord(1, STREAM_UNIX_ATTRIBUTES);
  same_session_attrs.VolSessionId = 7;
  same_session_attrs.VolSessionTime = 11;
  same_session_attrs.maskedStream = same_session_attrs.Stream;
  buffer.Add(&same_session_attrs);

  auto other_session_attrs = MakeRecord(2, STREAM_UNIX_ATTRIBUTES);
  other_session_attrs.VolSessionId = 8;
  other_session_attrs.VolSessionTime = 12;
  other_session_attrs.maskedStream = other_session_attrs.Stream;
  buffer.Add(&other_session_attrs);

  auto restore_object = MakeRecord(3, STREAM_RESTORE_OBJECT);
  restore_object.VolSessionId = same_session_attrs.VolSessionId;
  restore_object.VolSessionTime = same_session_attrs.VolSessionTime;
  buffer.Flush(restore_object.VolSessionId, restore_object.VolSessionTime);
  processed.emplace_back("restore-object");

  ASSERT_EQ(processed.size(), 2U);
  EXPECT_EQ(processed[0], "attributes:1");
  EXPECT_EQ(processed[1], "restore-object");

  buffer.FlushAll();
  ASSERT_EQ(processed.size(), 3U);
  EXPECT_EQ(processed[2], "attributes:2");
}

TEST(AppendProcessedFileTest, AttributeGroupBufferIsBufferedStreams)
{
  storagedaemon::DeviceRecord rec{};
  rec.maskedStream = STREAM_UNIX_ATTRIBUTES_EX;
  EXPECT_TRUE(storagedaemon::AttributeGroupBuffer::IsBuffered(&rec));
  rec.maskedStream = STREAM_SHA1_DIGEST;
  EXPECT_TRUE(storagedaemon::AttributeGroupBuffer::IsBuffered(&rec));
  rec.maskedStream = STREAM_RESTORE_OBJECT;
  EXPECT_FALSE(storagedaemon::AttributeGroupBuffer::IsBuffered(&rec));
  rec.maskedStream = STREAM_FILE_DATA;
  EXPECT_FALSE(storagedaemon::AttributeGroupBuffer::IsBuffered(&rec));
}

TEST(AppendProcessedFileTest, AttributeResendFilterDetectsSameFileIndex)
{
  storagedaemon::AttributeResendFilter filter;
  storagedaemon::DeviceRecord rec = MakeRecord(1, STREAM_UNIX_ATTRIBUTES);
  EXPECT_FALSE(filter.IsResend(&rec));
  EXPECT_TRUE(filter.IsResend(&rec));

  rec.FileIndex = 2;
  EXPECT_FALSE(filter.IsResend(&rec));

  rec.VolSessionId = 7;
  EXPECT_FALSE(filter.IsResend(&rec));
}
