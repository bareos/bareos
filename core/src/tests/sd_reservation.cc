/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2007-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2026 Bareos GmbH & Co. KG

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
#include "include/fcntl_def.h"

#include <chrono>
#include <bit>
#include <future>
#include <thread>

#define STORAGE_DAEMON 1
#include "include/jcr.h"
#include "lib/crypto_cache.h"
#include "lib/edit.h"
#include "lib/parse_conf.h"
#include "stored/device_control_record.h"
#include "stored/stored_jcr_impl.h"
#include "stored/job.h"
#include "stored/sd_plugins.h"
#include "stored/sd_stats.h"
#include "stored/stored.h"
#include "stored/stored_globals.h"
#include "stored/wait.h"
#include "stored/sd_backends.h"
#include "stored/autochanger.h"
#include "stored/btape_fill.h"
#include "stored/block.h"
#include "stored/record.h"
#include "stored/spool.h"

#include "bsock_mock.h"

using ::testing::Assign;
using ::testing::DoAll;
using ::testing::Return;
using namespace storagedaemon;

TEST(StorageTools, zero_fill)
{
  for (uint32_t length : {0, 1, 2, 3, 4, 7, 17, 64512}) {
    std::vector<char> buffer(static_cast<size_t>(length) + 2, '\x5a');
    FillZeroBuffer(buffer.data() + 1, length);
    EXPECT_EQ(buffer.front(), '\x5a');
    EXPECT_EQ(buffer.back(), '\x5a');
    EXPECT_TRUE(std::all_of(buffer.begin() + 1, buffer.end() - 1,
                            [](char byte) { return byte == 0; }));
  }
}

namespace storagedaemon {
/* import this to parse the config */
extern bool ParseSdConfig(const char* configfile, int exit_code);
}  // namespace storagedaemon

class ReservationTest : public ::testing::Test {
 protected:
  void SetUp() override;
  void TearDown() override;
};

void ReservationTest::SetUp()
{
  OSDependentInit();

  /* configfile is a global char* from stored_globals.h */
  configfile = strdup("configs/sd_reservation/");
  my_config = InitSdConfig(configfile, M_CONFIG_ERROR);
  ParseSdConfig(configfile, M_CONFIG_ERROR);
  /* we do not run CheckResources() here, so take care the test configuration
   * is not broken. Also autochangers will not work. */

  InitReservationsLock();
  CreateVolumeLists();
}
void ReservationTest::TearDown()
{
  FreeVolumeLists();

  {
    DeviceResource* d = nullptr;
    foreach_res (d, R_DEVICE) {
      Dmsg1(10, "Term device %s\n", d->archive_device_string);
      if (d->dev) {
        d->dev->ClearVolhdr();
        delete d->dev;
        d->dev = nullptr;
      }
    }
  }

  if (configfile) { free(configfile); }
  if (my_config) { delete my_config; }

  TermReservationsLock();
}

/* wrap JobControlRecord into something we can put into a unique_ptr */
struct TestJob {
  JobControlRecord* jcr;

  TestJob() = delete;
  TestJob(uint32_t jobid)
  {
    jcr = NewStoredJcr();
    jcr->JobId = jobid;
    jcr->sd_auth_key = strdup("no key set");
  }

  ~TestJob()
  {
    // set jobid = 0 before Free, so we don't try to
    // write a status file
    jcr->JobId = 0;

    // remove sockets so FreeJcr() doesn't clean up memory it doesn't own
    jcr->dir_bsock = nullptr;
    jcr->store_bsock = nullptr;
    jcr->file_bsock = nullptr;

    FreeJcr(jcr);
  }
};

namespace storagedaemon::detail {
int ReadBlockFromSpoolFile(DeviceControlRecord* dcr);
}  // namespace storagedaemon::detail

static thread_local int injected_wait_status = -1;

TEST_F(ReservationTest, device_wait_results)
{
  TestJob job(111u);
  int retries = 0;
  detail::DeviceWait()
      = [](pthread_cond_t*, pthread_mutex_t* mutex, const timespec* timeout) {
          EXPECT_EQ(pthread_mutex_trylock(mutex), EBUSY);
          EXPECT_GE(timeout->tv_sec, time(nullptr) + 59);
          return injected_wait_status;
        };
  for (int status : {0, ETIMEDOUT, EINVAL, EPERM}) {
    injected_wait_status = status;
    errno = ENOENT;
    EXPECT_EQ(WaitForDevice(job.jcr, retries),
              status == 0 || status == ETIMEDOUT);
  }
  detail::DeviceWait() = pthread_cond_timedwait;
  EXPECT_EQ(retries, 4);
}

TEST_F(ReservationTest, reservation_wait_failure)
{
  auto bsock = std::make_unique<BareosSocketMock>();
  TestJob first(111u);
  TestJob second(222u);
  first.jcr->dir_bsock = second.jcr->dir_bsock = bsock.get();
  EXPECT_CALL(*bsock, recv()).WillRepeatedly(Return(BNET_EOD));
  EXPECT_CALL(*bsock, send()).WillRepeatedly(Return(true));
  for (auto* job : {first.jcr, second.jcr}) {
    EXPECT_CALL(*bsock, recv())
        .WillOnce(
            BSOCK_RECV(bsock.get(),
                       "use storage=sssss media_type=File pool_name=ppppp "
                       "pool_type=ptptp append=0 copy=0 stripe=0"))
        .WillOnce(BSOCK_RECV(bsock.get(), "use device=single3"))
        .WillOnce(Return(BNET_EOD))
        .WillOnce(Return(BNET_EOD));
    bsock->recv();
    detail::DeviceWait() = [](pthread_cond_t*, pthread_mutex_t*,
                              const timespec*) { return EINVAL; };
    bool result = use_cmd(job);
    detail::DeviceWait() = pthread_cond_timedwait;
    EXPECT_EQ(result, job == first.jcr);
  }
}

TEST_F(ReservationTest, autochanger_wait_failure_leaves_changer_unlocked)
{
  TestJob job(111u);
  ASSERT_TRUE(InitAutochangers());
  auto* resource = static_cast<DeviceResource*>(
      my_config->GetResWithName(R_DEVICE, "auto1dev2"));
  auto* other = static_cast<DeviceResource*>(
      my_config->GetResWithName(R_DEVICE, "auto1dev3"));
  ASSERT_NE(resource, nullptr);
  ASSERT_NE(other, nullptr);
  auto* dev = FactoryCreateDevice(job.jcr, resource);
  auto* busy = FactoryCreateDevice(job.jcr, other);
  ASSERT_NE(dev, nullptr);
  ASSERT_NE(busy, nullptr);
  resource->dev = dev;
  other->dev = busy;
  for (auto* res : {resource, other}) {
    free(res->changer_command);
    res->changer_command = strdup("/bin/true");
  }
  dev->SetSlotNumber(1);
  busy->SetSlotNumber(2);
  busy->SetBlocked(BST_WAITING_FOR_SYSOP);
  DeviceControlRecord dcr;
  dcr.jcr = job.jcr;
  dcr.dev = dev;
  dcr.device_resource = resource;
  dcr.VolCatInfo.InChanger = true;
  dcr.VolCatInfo.Slot = 2;
  detail::DeviceWait() = [](pthread_cond_t*, pthread_mutex_t*,
                            const timespec*) { return EINVAL; };
  int result = AutoloadDevice(&dcr, 0, nullptr);
  detail::DeviceWait() = pthread_cond_timedwait;
  EXPECT_EQ(result, -1);
  EXPECT_EQ(resource->changer_res->changer_lock.w_active, 0);
  EXPECT_EQ(RwlWritetrylock(&resource->changer_res->changer_lock), 0);
  EXPECT_EQ(RwlWriteunlock(&resource->changer_res->changer_lock), 0);
}

#if !defined(HAVE_WIN32)
class StorageBlockTest : public ReservationTest {
 protected:
  void SetUp() override
  {
    ReservationTest::SetUp();
    job = std::make_unique<TestJob>(111u);
    auto* resource = static_cast<DeviceResource*>(
        my_config->GetResWithName(R_DEVICE, "single3"));
    ASSERT_NE(resource, nullptr);
    dev = FactoryCreateDevice(job->jcr, resource);
    ASSERT_NE(dev, nullptr);
    resource->dev = dev;
    dev->max_block_size = 512;
    dev->ClearCap(CAP_BLOCKCHECKSUM);
    dev->ClearCap(CAP_ADJWRITESIZE);
    dev->SetAppend();
    filename
        = std::string("block-")
          + ::testing::UnitTest::GetInstance()->current_test_info()->name();
    dev->fd = open(filename.c_str(), O_CREAT | O_TRUNC | O_RDWR, 0600);
    ASSERT_GE(dev->fd, 0);
    dcr.jcr = job->jcr;
    dcr.dev = dev;
    dcr.device_resource = resource;
    dcr.block = new_block(dev);
    dcr.VolFirstIndex = 1;
    record = new_record();
    record->VolSessionId = 0x01020304;
    record->VolSessionTime = 0x05060708;
    record->FileIndex = 1;
    record->Stream = 2;
    record->data = EnsurePoolMemoryCapacity(record->data, 1024);
    memcpy(record->data, "abc", 3);
    record->data_len = 3;
    memset(dcr.block->buf, 0, dcr.block->buf_len);
  }

  void TearDown() override
  {
    FreeRecord(record);
    FreeBlock(dcr.block);
    job.reset();
    ReservationTest::TearDown();
    unlink(filename.c_str());
  }

  std::unique_ptr<TestJob> job;
  Device* dev{};
  DeviceControlRecord dcr;
  DeviceRecord* record{};
  std::string filename;
  const std::vector<unsigned char> archive{
      0, 0, 0, 0, 0, 0, 0, 39, 0, 0, 0, 0, 'B', 'B', '0', '2', 1,   2,   3,  4,
      5, 6, 7, 8, 0, 0, 0, 1,  0, 0, 0, 2, 0,   0,   0,   3,   'a', 'b', 'c'};
};

TEST_F(StorageBlockTest, archive_fixture_and_exhausted_read)
{
  EXPECT_EQ(dcr.block->bytes_used_for_write, WRITE_BLKHDR_LENGTH);
  EXPECT_EQ(dcr.block->bytes_remaining_to_read, 0);
  EXPECT_EQ(BlockWriteNavail(dcr.block), 512 - WRITE_BLKHDR_LENGTH);
  ASSERT_TRUE(WriteRecordToBlock(&dcr, record));
  EXPECT_EQ(BlockWriteNavail(dcr.block), 512 - archive.size());
  ASSERT_TRUE(dcr.WriteBlockToDev());
  std::vector<unsigned char> actual(archive.size());
  ASSERT_EQ(pread(dev->fd, actual.data(), actual.size(), 0),
            static_cast<ssize_t>(actual.size()));
  EXPECT_EQ(actual, archive);
  ASSERT_EQ(lseek(dev->fd, 0, SEEK_SET), 0);
  ASSERT_EQ(dcr.ReadBlockFromDev(false), DeviceControlRecord::ReadStatus::Ok);
  EXPECT_EQ(dcr.block->bytes_used_for_write, 0);
  EXPECT_EQ(dcr.block->bytes_remaining_to_read,
            archive.size() - WRITE_BLKHDR_LENGTH);
  auto* duplicate = dup_block(dcr.block);
  EXPECT_EQ(duplicate->bytes_used_for_write, 0);
  EXPECT_EQ(duplicate->bytes_remaining_to_read,
            dcr.block->bytes_remaining_to_read);
  EXPECT_EQ(memcmp(duplicate->buf, dcr.block->buf, archive.size()), 0);
  FreeBlock(duplicate);
  ASSERT_TRUE(ReadRecordFromBlock(&dcr, record));
  EXPECT_EQ(record->data_len, 3);
  EXPECT_EQ(std::string(record->data, record->data_len), "abc");
  EXPECT_FALSE(ReadRecordFromBlock(&dcr, record));
  EXPECT_FALSE(ReadRecordFromBlock(&dcr, record));
  EmptyBlock(dcr.block);
  EXPECT_EQ(dcr.block->bytes_used_for_write, WRITE_BLKHDR_LENGTH);
  EXPECT_EQ(dcr.block->bytes_remaining_to_read, 0);
  EXPECT_TRUE(WriteRecordToBlock(&dcr, record));
  duplicate = dup_block(dcr.block);
  EXPECT_EQ(duplicate->bytes_used_for_write, archive.size());
  EXPECT_EQ(duplicate->bytes_remaining_to_read, 0);
  FreeBlock(duplicate);
}

TEST_F(StorageBlockTest, spool_fixture_and_replay)
{
  ASSERT_TRUE(WriteRecordToBlock(&dcr, record));
  std::vector<unsigned char> expected{1, 0, 0, 0, 1, 0, 0, 0, 39, 0, 0, 0};
  if constexpr (std::endian::native == std::endian::big) {
    expected = {0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 39};
  }
  expected.resize(12 + WRITE_BLKHDR_LENGTH, 0);
  expected.insert(expected.end(), archive.begin() + WRITE_BLKHDR_LENGTH,
                  archive.end());
  dcr.spool_fd = dev->fd;
  ASSERT_TRUE(WriteBlockToSpoolFile(&dcr));
  EXPECT_EQ(dcr.block->bytes_used_for_write, WRITE_BLKHDR_LENGTH);
  EXPECT_EQ(dcr.block->bytes_remaining_to_read, 0);
  std::vector<unsigned char> actual(expected.size());
  ASSERT_EQ(pread(dev->fd, actual.data(), actual.size(), 0),
            static_cast<ssize_t>(actual.size()));
  EXPECT_EQ(actual, expected);
  ASSERT_EQ(lseek(dev->fd, 0, SEEK_SET), 0);
  job->jcr->VolSessionId = record->VolSessionId;
  job->jcr->VolSessionTime = record->VolSessionTime;
  ASSERT_EQ(detail::ReadBlockFromSpoolFile(&dcr), 3);
  EXPECT_EQ(dcr.block->bytes_used_for_write, archive.size());
  EXPECT_EQ(dcr.block->bytes_remaining_to_read, 0);
  EXPECT_EQ(dcr.block->FirstIndex, 1);
  EXPECT_EQ(dcr.block->LastIndex, 1);
  EXPECT_EQ(BlockWriteNavail(dcr.block), 512 - archive.size());
  ASSERT_TRUE(dcr.WriteBlockToDev());
  actual.resize(archive.size());
  ASSERT_EQ(pread(dev->fd, actual.data(), actual.size(), expected.size()),
            static_cast<ssize_t>(actual.size()));
  EXPECT_EQ(actual, archive);
}

TEST_F(StorageBlockTest, partial_records_and_continuations)
{
  std::string payload(233, 'x');
  for (size_t i = 0; i < payload.size(); ++i) { payload[i] = i % 127; }
  memcpy(record->data, payload.data(), payload.size());
  record->data_len = payload.size();
  dcr.block->buf_len = 64;
  bool complete;
  int blocks = 0;
  do {
    complete = WriteRecordToBlock(&dcr, record);
    EXPECT_EQ(dcr.block->bytes_remaining_to_read, 0);
    EXPECT_EQ(dcr.block->bytes_used_for_write,
              complete ? 36 + payload.size() % 28 : 64);
    ASSERT_TRUE(dcr.WriteBlockToDev());
    EXPECT_EQ(dcr.block->bytes_used_for_write, WRITE_BLKHDR_LENGTH);
    ++blocks;
  } while (!complete);
  EXPECT_GT(blocks, 1);
  ASSERT_EQ(lseek(dev->fd, 0, SEEK_SET), 0);
  for (int i = 0; i < blocks; ++i) {
    ASSERT_EQ(dcr.ReadBlockFromDev(false), DeviceControlRecord::ReadStatus::Ok);
    EXPECT_EQ(dcr.block->bytes_used_for_write, 0);
    ASSERT_TRUE(ReadRecordFromBlock(&dcr, record));
    EXPECT_EQ(dcr.block->bytes_remaining_to_read, 0);
    EXPECT_EQ(record->remainder, i == blocks - 1 ? 0 : 1);
  }
  EXPECT_EQ(std::string(record->data, record->data_len), payload);
  ASSERT_EQ(dcr.ReadBlockFromDev(false),
            DeviceControlRecord::ReadStatus::EndOfFile);
  EXPECT_EQ(dcr.block->bytes_used_for_write, 0);
  EXPECT_EQ(dcr.block->bytes_remaining_to_read, 0);
  ASSERT_EQ(dcr.ReadBlockFromDev(false),
            DeviceControlRecord::ReadStatus::EndOfTape);
  EXPECT_EQ(dcr.block->bytes_used_for_write, 0);
  EXPECT_EQ(dcr.block->bytes_remaining_to_read, 0);
  EmptyBlock(dcr.block);
  EXPECT_FALSE(WriteRecordToBlock(&dcr, record));
  EXPECT_EQ(dcr.block->bytes_used_for_write, 64);
  EXPECT_EQ(dcr.block->bytes_remaining_to_read, 0);
}

TEST_F(StorageBlockTest, record_header_at_block_boundary)
{
  dcr.block->buf_len = WRITE_BLKHDR_LENGTH + WRITE_RECHDR_LENGTH;
  EXPECT_FALSE(WriteRecordToBlock(&dcr, record));
  EXPECT_EQ(record->remainder, 3);
  EXPECT_EQ(BlockWriteNavail(dcr.block), 0);
  ASSERT_TRUE(dcr.WriteBlockToDev());
  dcr.block->buf_len = 512;
  ASSERT_TRUE(WriteRecordToBlock(&dcr, record));
  ASSERT_TRUE(dcr.WriteBlockToDev());
  const std::vector<unsigned char> continuation{0,    0,    0, 1, 0xff, 0xff,
                                                0xff, 0xfe, 0, 0, 0,    3};
  std::vector<unsigned char> actual(continuation.size());
  ASSERT_EQ(pread(dev->fd, actual.data(), actual.size(), 36 + 24),
            static_cast<ssize_t>(actual.size()));
  EXPECT_EQ(actual, continuation);
  ASSERT_EQ(lseek(dev->fd, 0, SEEK_SET), 0);
  ASSERT_EQ(dcr.ReadBlockFromDev(false), DeviceControlRecord::ReadStatus::Ok);
  ASSERT_TRUE(ReadRecordFromBlock(&dcr, record));
  EXPECT_EQ(record->data_len, 0);
  EXPECT_EQ(record->remainder, 1);
  EXPECT_EQ(dcr.block->bytes_remaining_to_read, 0);
  ASSERT_EQ(dcr.ReadBlockFromDev(false), DeviceControlRecord::ReadStatus::Ok);
  ASSERT_TRUE(ReadRecordFromBlock(&dcr, record));
  EXPECT_EQ(std::string(record->data, record->data_len), "abc");
  EXPECT_EQ(record->remainder, 0);
  EXPECT_EQ(dcr.block->bytes_remaining_to_read, 0);
}

TEST_F(StorageBlockTest, fixed_block_padding)
{
  dev->SetCap(CAP_ADJWRITESIZE);
  dev->min_block_size = dev->max_block_size = 512;
  memset(dcr.block->buf, '\x5a', dcr.block->buf_len);
  ASSERT_TRUE(WriteRecordToBlock(&dcr, record));
  ASSERT_TRUE(dcr.WriteBlockToDev());
  std::vector<unsigned char> actual(512);
  ASSERT_EQ(pread(dev->fd, actual.data(), actual.size(), 0), 512);
  EXPECT_TRUE(std::equal(archive.begin(), archive.end(), actual.begin()));
  EXPECT_TRUE(std::all_of(actual.begin() + archive.size(), actual.end(),
                          [](unsigned char byte) { return byte == 0; }));
  ASSERT_EQ(lseek(dev->fd, 0, SEEK_SET), 0);
  ASSERT_EQ(dcr.ReadBlockFromDev(false), DeviceControlRecord::ReadStatus::Ok);
  EXPECT_EQ(dcr.block->read_len, 512);
  EXPECT_EQ(dcr.block->bytes_remaining_to_read, 15);
  ASSERT_TRUE(ReadRecordFromBlock(&dcr, record));
  EXPECT_EQ(dcr.block->bytes_remaining_to_read, 0);
}

TEST_F(StorageBlockTest, failed_reads_clear_directional_counts)
{
  for (auto bytes :
       {std::vector<unsigned char>{}, std::vector<unsigned char>{1, 2, 3},
        std::vector<unsigned char>(24, 0), archive}) {
    ASSERT_EQ(ftruncate(dev->fd, 0), 0);
    ASSERT_EQ(lseek(dev->fd, 0, SEEK_SET), 0);
    if (bytes == archive) { bytes[7] = 45; }
    ASSERT_EQ(write(dev->fd, bytes.data(), bytes.size()),
              static_cast<ssize_t>(bytes.size()));
    ASSERT_EQ(lseek(dev->fd, 0, SEEK_SET), 0);
    dev->ClearEof();
    dcr.block->bytes_used_for_write = 99;
    dcr.block->bytes_remaining_to_read = 88;
    EXPECT_NE(dcr.ReadBlockFromDev(false), DeviceControlRecord::ReadStatus::Ok);
    EXPECT_EQ(dcr.block->read_len, 0);
    EXPECT_EQ(dcr.block->bytes_used_for_write, 0);
    EXPECT_EQ(dcr.block->bytes_remaining_to_read, 0);
  }
  ASSERT_EQ(close(dev->fd), 0);
  dev->fd = -1;
  dcr.block->bytes_used_for_write = 99;
  dcr.block->bytes_remaining_to_read = 88;
  EXPECT_EQ(dcr.ReadBlockFromDev(false),
            DeviceControlRecord::ReadStatus::Error);
  EXPECT_EQ(dcr.block->bytes_used_for_write, 0);
  EXPECT_EQ(dcr.block->bytes_remaining_to_read, 0);
}

TEST_F(StorageBlockTest, legacy_block_fixture)
{
  const std::vector<unsigned char> legacy{
      0, 0, 0, 0, 0, 0, 0, 39, 0, 0, 0, 0, 'B', 'B', '0', '1', 1,   2,   3,  4,
      5, 6, 7, 8, 0, 0, 0, 1,  0, 0, 0, 2, 0,   0,   0,   3,   'a', 'b', 'c'};
  ASSERT_EQ(write(dev->fd, legacy.data(), legacy.size()),
            static_cast<ssize_t>(legacy.size()));
  ASSERT_EQ(lseek(dev->fd, 0, SEEK_SET), 0);
  ASSERT_EQ(dcr.ReadBlockFromDev(false), DeviceControlRecord::ReadStatus::Ok);
  EXPECT_EQ(dcr.block->BlockVer, 1);
  EXPECT_EQ(dcr.block->bytes_used_for_write, 0);
  EXPECT_EQ(dcr.block->bytes_remaining_to_read, 23);
  ASSERT_TRUE(ReadRecordFromBlock(&dcr, record));
  EXPECT_EQ(record->VolSessionId, 0x01020304);
  EXPECT_EQ(record->VolSessionTime, 0x05060708);
  EXPECT_EQ(std::string(record->data, record->data_len), "abc");
  EXPECT_EQ(dcr.block->bytes_remaining_to_read, 0);
}

TEST_F(StorageBlockTest, reread_with_larger_buffer)
{
  ASSERT_EQ(write(dev->fd, archive.data(), archive.size()),
            static_cast<ssize_t>(archive.size()));
  ASSERT_EQ(lseek(dev->fd, 0, SEEK_SET), 0);
  dcr.block->buf_len = 32;
  ASSERT_EQ(dcr.ReadBlockFromDev(false), DeviceControlRecord::ReadStatus::Ok);
  EXPECT_EQ(dcr.block->buf_len, archive.size());
  EXPECT_EQ(dcr.block->bytes_used_for_write, 0);
  EXPECT_EQ(dcr.block->bytes_remaining_to_read, 15);
  ASSERT_TRUE(ReadRecordFromBlock(&dcr, record));
  EXPECT_EQ(std::string(record->data, record->data_len), "abc");
}

TEST_F(StorageBlockTest, checksum_failure_and_cancellation)
{
  ASSERT_EQ(write(dev->fd, archive.data(), archive.size()),
            static_cast<ssize_t>(archive.size()));
  ASSERT_EQ(lseek(dev->fd, 0, SEEK_SET), 0);
  dev->SetCap(CAP_BLOCKCHECKSUM);
  EXPECT_EQ(dcr.ReadBlockFromDev(false),
            DeviceControlRecord::ReadStatus::Error);
  EXPECT_EQ(dcr.block->bytes_used_for_write, 0);
  EXPECT_EQ(dcr.block->bytes_remaining_to_read, 0);
  job->jcr->setJobStatus(JS_Canceled);
  dcr.block->bytes_used_for_write = 99;
  dcr.block->bytes_remaining_to_read = 88;
  EXPECT_EQ(dcr.ReadBlockFromDev(false),
            DeviceControlRecord::ReadStatus::Error);
  EXPECT_EQ(dcr.block->bytes_used_for_write, 0);
  EXPECT_EQ(dcr.block->bytes_remaining_to_read, 0);
}

TEST_F(StorageBlockTest, spool_eof_and_errors_reset_read_state)
{
  dcr.spool_fd = dev->fd;
  for (const auto& bytes :
       {std::vector<unsigned char>{}, std::vector<unsigned char>{1, 2, 3},
        std::vector<unsigned char>{1, 0, 0, 0, 1, 0, 0, 0, 39, 0, 0, 0}}) {
    ASSERT_EQ(ftruncate(dev->fd, 0), 0);
    ASSERT_EQ(lseek(dev->fd, 0, SEEK_SET), 0);
    ASSERT_EQ(write(dev->fd, bytes.data(), bytes.size()),
              static_cast<ssize_t>(bytes.size()));
    ASSERT_EQ(lseek(dev->fd, 0, SEEK_SET), 0);
    dcr.block->bytes_used_for_write = 99;
    dcr.block->bytes_remaining_to_read = 88;
    dcr.block->read_len = 77;
    EXPECT_EQ(detail::ReadBlockFromSpoolFile(&dcr), bytes.empty() ? 1 : 2);
    EXPECT_EQ(dcr.block->bytes_used_for_write, WRITE_BLKHDR_LENGTH);
    EXPECT_EQ(dcr.block->bytes_remaining_to_read, 0);
    EXPECT_EQ(dcr.block->read_len, 0);
  }
}
#endif

void WaitThenUnreserve(std::unique_ptr<TestJob>&);
void WaitThenUnreserve(std::unique_ptr<TestJob>& job)
{
  std::this_thread::sleep_for(std::chrono::seconds(5));
  job->jcr->sd_impl->dcr->UnreserveDevice();
  ReleaseDeviceCond();
}

// Test that an illegal command passed to use_cmd will fail gracefully
TEST_F(ReservationTest, use_cmd_illegal)
{
  auto bsock = std::make_unique<BareosSocketMock>();
  auto job = std::make_unique<TestJob>(111u);

  job->jcr->dir_bsock = bsock.get();

  EXPECT_CALL(*bsock, recv())
      .WillOnce(BSOCK_RECV(bsock.get(), "illegal command"));

  EXPECT_CALL(*bsock, send()).Times(1).WillRepeatedly(Return(true));

  bsock->recv();
  ASSERT_EQ(use_cmd(job->jcr), false);
}

/*
 * Test that an illegal device command passed to use_cmd after the use storage
 * command will also fail gracefully
 */
TEST_F(ReservationTest, use_cmd_illegal_dev)
{
  auto bsock = std::make_unique<BareosSocketMock>();
  auto job = std::make_unique<TestJob>(111u);
  job->jcr->dir_bsock = bsock.get();

  EXPECT_CALL(*bsock, recv())
      .WillOnce(BSOCK_RECV(bsock.get(),
                           "use storage=sssss media_type=mtmtm pool_name=ppppp "
                           "pool_type=ptptp append=1 copy=0 stripe=0"))
      .WillOnce(BSOCK_RECV(bsock.get(), "illegal device command"));

  EXPECT_CALL(*bsock, send()).Times(1).WillRepeatedly(Return(true));

  bsock->recv();
  ASSERT_EQ(use_cmd(job->jcr), false);
}

// Test reserving a device for read works correctly
TEST_F(ReservationTest, use_cmd_reserve_read_twice_success)
{
  auto bsock = std::make_unique<BareosSocketMock>();
  auto job1 = std::make_unique<TestJob>(111u);
  auto job2 = std::make_unique<TestJob>(222u);
  job1->jcr->dir_bsock = job2->jcr->dir_bsock = bsock.get();

  EXPECT_CALL(*bsock, recv())
      .WillOnce(BSOCK_RECV(bsock.get(),
                           "use storage=sssss media_type=File pool_name=ppppp "
                           "pool_type=ptptp append=0 copy=0 stripe=0"))
      .WillOnce(BSOCK_RECV(bsock.get(), "use device=auto1"))
      .WillOnce(Return(BNET_EOD))  // end of device commands
      .WillOnce(Return(BNET_EOD))  // end of storage command
      .WillOnce(BSOCK_RECV(bsock.get(),
                           "use storage=sssss media_type=File pool_name=ppppp "
                           "pool_type=ptptp append=0 copy=0 stripe=0"))
      .WillOnce(BSOCK_RECV(bsock.get(), "use device=auto1"))
      .WillOnce(Return(BNET_EOD))   // end of device commands
      .WillOnce(Return(BNET_EOD));  // end of storage command

  EXPECT_CALL(*bsock, send()).Times(2).WillRepeatedly(Return(true));

  bsock->recv();
  ASSERT_EQ(use_cmd(job1->jcr), true);
  ASSERT_STREQ(bsock->msg, "3000 OK use device device=auto1dev2\n");

  bsock->recv();
  ASSERT_EQ(use_cmd(job2->jcr), true);
  ASSERT_STREQ(bsock->msg, "3000 OK use device device=auto1dev3\n");
}

// Test reserving broken device
TEST_F(ReservationTest, use_cmd_reserve_broken)
{
  auto bsock = std::make_unique<BareosSocketMock>();
  auto job1 = std::make_unique<TestJob>(111u);
  job1->jcr->dir_bsock = bsock.get();

  EXPECT_CALL(*bsock, recv())
      .WillOnce(BSOCK_RECV(bsock.get(),
                           "use storage=sssss media_type=File pool_name=ppppp "
                           "pool_type=ptptp append=0 copy=0 stripe=0"))
      .WillOnce(BSOCK_RECV(bsock.get(), "use device=single1"))
      .WillOnce(Return(BNET_EOD))   // end of device commands
      .WillOnce(Return(BNET_EOD));  // end of storage command

  EXPECT_CALL(*bsock, send()).Times(2).WillRepeatedly(Return(true));

  bsock->recv();
  ASSERT_EQ(use_cmd(job1->jcr), false);
  ASSERT_STREQ(bsock->msg,
               "3924 Device \"single1\" not in SD Device resources or no "
               "matching Media Type.\n");
}

// Test reserving device with wrong mediatype
TEST_F(ReservationTest, use_cmd_reserve_wrong_mediatype)
{
  auto bsock = std::make_unique<BareosSocketMock>();
  auto job1 = std::make_unique<TestJob>(111u);
  job1->jcr->dir_bsock = bsock.get();

  EXPECT_CALL(*bsock, recv())
      .WillOnce(BSOCK_RECV(bsock.get(),
                           "use storage=sssss media_type=File pool_name=ppppp "
                           "pool_type=ptptp append=0 copy=0 stripe=0"))
      .WillOnce(BSOCK_RECV(bsock.get(), "use device=single2"))
      .WillOnce(Return(BNET_EOD))   // end of device commands
      .WillOnce(Return(BNET_EOD));  // end of storage command

  EXPECT_CALL(*bsock, send()).Times(2).WillRepeatedly(Return(true));

  bsock->recv();
  ASSERT_EQ(use_cmd(job1->jcr), false);
  ASSERT_STREQ(bsock->msg,
               "3924 Device \"single2\" not in SD Device resources or no "
               "matching Media Type.\n");
}

// Test reserving a reserved device and wait for it to become free
TEST_F(ReservationTest, use_cmd_reserve_read_twice_wait)
{
  auto bsock = std::make_unique<BareosSocketMock>();
  auto job1 = std::make_unique<TestJob>(111u);
  auto job2 = std::make_unique<TestJob>(222u);
  job1->jcr->dir_bsock = job2->jcr->dir_bsock = bsock.get();

  EXPECT_CALL(*bsock, recv())
      .WillOnce(BSOCK_RECV(bsock.get(),
                           "use storage=sssss media_type=File pool_name=ppppp "
                           "pool_type=ptptp append=0 copy=0 stripe=0"))
      .WillOnce(BSOCK_RECV(bsock.get(), "use device=single3"))
      .WillOnce(Return(BNET_EOD))  // end of device commands
      .WillOnce(Return(BNET_EOD))  // end of storage command
      .WillOnce(BSOCK_RECV(bsock.get(),
                           "use storage=sssss media_type=File pool_name=ppppp "
                           "pool_type=ptptp append=0 copy=0 stripe=0"))
      .WillOnce(BSOCK_RECV(bsock.get(), "use device=single3"))
      .WillOnce(Return(BNET_EOD))   // end of device commands
      .WillOnce(Return(BNET_EOD));  // end of storage command

  EXPECT_CALL(*bsock, send()).WillRepeatedly(Return(true));

  bsock->recv();
  ASSERT_EQ(use_cmd(job1->jcr), true);
  ASSERT_STREQ(bsock->msg, "3000 OK use device device=single3\n");

  bsock->recv();
  auto _ = std::async(std::launch::async, [&job1] { WaitThenUnreserve(job1); });
  ASSERT_EQ(use_cmd(job2->jcr), true);
  ASSERT_STREQ(bsock->msg, "3000 OK use device device=single3\n");
}

// Test append=1 reserving write-only devices only works correctly
TEST_F(ReservationTest, use_cmd_append_reserves_write_only)
{
  auto bsock = std::make_unique<BareosSocketMock>();
  auto job1 = std::make_unique<TestJob>(111u);
  job1->jcr->dir_bsock = bsock.get();

  EXPECT_CALL(*bsock, recv())
      .WillOnce(
          BSOCK_RECV(bsock.get(),
                     "use storage=sssss media_type=FileRWOnly pool_name=ppppp "
                     "pool_type=ptptp append=1 copy=0 stripe=0"))
      .WillOnce(BSOCK_RECV(bsock.get(), "use device=readonly1"))
      .WillOnce(BSOCK_RECV(bsock.get(), "use device=readonly2"))
      .WillOnce(BSOCK_RECV(bsock.get(), "use device=writeonly1"))
      .WillOnce(BSOCK_RECV(bsock.get(), "use device=writeonly2"))
      .WillOnce(Return(BNET_EOD))  // end of device commands
      .WillOnce(Return(BNET_EOD))  // end of storage command
      .WillOnce(BSOCK_RECV(
          bsock.get(),
          "1901 No Media."));  // response to DirFindNextAppendableVolume

  EXPECT_CALL(*bsock, send()).WillRepeatedly(Return(true));

  bsock->recv();
  ASSERT_EQ(use_cmd(job1->jcr), true);
  ASSERT_STREQ(bsock->msg, "3000 OK use device device=writeonly1\n");
}

// Test append=0 reserving read-only devices only works correctly
TEST_F(ReservationTest, use_cmd_non_append_reserves_read_only)
{
  auto bsock = std::make_unique<BareosSocketMock>();
  auto job1 = std::make_unique<TestJob>(111u);
  job1->jcr->dir_bsock = bsock.get();

  EXPECT_CALL(*bsock, recv())
      .WillOnce(
          BSOCK_RECV(bsock.get(),
                     "use storage=sssss media_type=FileRWOnly pool_name=ppppp "
                     "pool_type=ptptp append=0 copy=0 stripe=0"))
      .WillOnce(BSOCK_RECV(bsock.get(), "use device=writeonly1"))
      .WillOnce(BSOCK_RECV(bsock.get(), "use device=writeonly2"))
      .WillOnce(BSOCK_RECV(bsock.get(), "use device=readonly1"))
      .WillOnce(BSOCK_RECV(bsock.get(), "use device=readonly2"))
      .WillOnce(Return(BNET_EOD))   // end of device commands
      .WillOnce(Return(BNET_EOD));  // end of storage command

  EXPECT_CALL(*bsock, send()).WillRepeatedly(Return(true));

  bsock->recv();
  ASSERT_EQ(use_cmd(job1->jcr), true);
  ASSERT_STREQ(bsock->msg, "3000 OK use device device=readonly1\n");
}

/* Test append=0 reserving read-only devices only works correctly when trying to
   reserve more devices than available */
TEST_F(ReservationTest, use_cmd_non_append_reserves_read_only_with_wait)
{
  auto bsock = std::make_unique<BareosSocketMock>();
  auto job1 = std::make_unique<TestJob>(111u);
  auto job2 = std::make_unique<TestJob>(222u);
  auto job3 = std::make_unique<TestJob>(333u);
  job1->jcr->dir_bsock = job2->jcr->dir_bsock = job3->jcr->dir_bsock
      = bsock.get();

  EXPECT_CALL(*bsock, recv())
      .WillOnce(
          BSOCK_RECV(bsock.get(),
                     "use storage=sssss media_type=FileRWOnly pool_name=ppppp "
                     "pool_type=ptptp append=0 copy=0 stripe=0"))
      .WillOnce(BSOCK_RECV(bsock.get(), "use device=readonly1"))
      .WillOnce(BSOCK_RECV(bsock.get(), "use device=readonly2"))
      .WillOnce(BSOCK_RECV(bsock.get(), "use device=writeonly1"))
      .WillOnce(BSOCK_RECV(bsock.get(), "use device=writeonly2"))
      .WillOnce(Return(BNET_EOD))  // end of device commands
      .WillOnce(Return(BNET_EOD))  // end of storage command
      .WillOnce(
          BSOCK_RECV(bsock.get(),
                     "use storage=sssss media_type=FileRWOnly pool_name=ppppp "
                     "pool_type=ptptp append=0 copy=0 stripe=0"))
      .WillOnce(BSOCK_RECV(bsock.get(), "use device=readonly1"))
      .WillOnce(BSOCK_RECV(bsock.get(), "use device=readonly2"))
      .WillOnce(BSOCK_RECV(bsock.get(), "use device=writeonly1"))
      .WillOnce(BSOCK_RECV(bsock.get(), "use device=writeonly2"))
      .WillOnce(Return(BNET_EOD))  // end of device commands
      .WillOnce(Return(BNET_EOD))  // end of storage command
      .WillOnce(
          BSOCK_RECV(bsock.get(),
                     "use storage=sssss media_type=FileRWOnly pool_name=ppppp "
                     "pool_type=ptptp append=0 copy=0 stripe=0"))
      .WillOnce(BSOCK_RECV(bsock.get(), "use device=readonly1"))
      .WillOnce(BSOCK_RECV(bsock.get(), "use device=readonly2"))
      .WillOnce(BSOCK_RECV(bsock.get(), "use device=writeonly1"))
      .WillOnce(BSOCK_RECV(bsock.get(), "use device=writeonly2"))
      .WillOnce(Return(BNET_EOD))   // end of device commands
      .WillOnce(Return(BNET_EOD));  // end of storage command

  EXPECT_CALL(*bsock, send()).Times(4).WillRepeatedly(Return(true));

  bsock->recv();
  ASSERT_EQ(use_cmd(job1->jcr), true);
  ASSERT_STREQ(bsock->msg, "3000 OK use device device=readonly1\n");

  bsock->recv();
  ASSERT_EQ(use_cmd(job2->jcr), true);
  ASSERT_STREQ(bsock->msg, "3000 OK use device device=readonly2\n");

  /* Start reserving another device. Since only writeonly1 and writeonly2
     are free this should wait until readonly1 or readonly2 is unreserved. */
  auto future = std::async(std::launch::async, [&job3, &bsock] {
    bsock->recv();
    ASSERT_EQ(use_cmd(job3->jcr), true);
    ASSERT_STREQ(bsock->msg, "3000 OK use device device=readonly1\n");
  });

  // Unreserve job1 and readonly1 after waiting for a bit
  auto _ = std::async(std::launch::async, [&job1] { WaitThenUnreserve(job1); });

  future.wait();
}
