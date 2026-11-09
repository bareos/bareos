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

#include <chrono>
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
#include "stored/acquire.h"
#include "stored/lock.h"

#include "bsock_mock.h"

using ::testing::Assign;
using ::testing::DoAll;
using ::testing::Return;
using namespace storagedaemon;

namespace storagedaemon {
/* import this to parse the config */
extern bool ParseSdConfig(const char* configfile, int exit_code);
}  // namespace storagedaemon

class ReservationTest : public ::testing::Test {
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

class ReleaseTestDevice : public SpoolDevice {
 public:
  MOCK_METHOD(int, d_close, (int), (override));
  MOCK_METHOD(bool, d_flush, (DeviceControlRecord*), (override));
};

TEST_F(ReservationTest, ReleaseWaitsForAnotherJobsAcquisition)
{
  // Release must not close a volume while another job is acquiring it.
  for (bool acquired : {false, true}) {
    SCOPED_TRACE(acquired);
    auto job = std::make_unique<TestJob>(111u);
    ReleaseTestDevice device;
    device.prt_name = GetPoolMemory(PM_FNAME);
    PmStrcpy(device.prt_name, "shared acquisition device");
    device.fd = 42;
    device.norewindonclose = true;
    device.num_writers = 1;
    DeviceResource resource{};
    auto* dcr = new DeviceControlRecord;
    dcr->jcr = job->jcr;
    dcr->SetDev(&device);
    dcr->device_resource = &resource;
    dcr->keep_dcr = true;
    EXPECT_CALL(device, d_close(42)).WillOnce(Return(0));
    EXPECT_CALL(device, d_flush(dcr)).WillOnce([&](DeviceControlRecord*) {
      const int status = pthread_mutex_trylock(&device.mutex_);
      EXPECT_EQ(status, 0) << "Flush must not hold the device mutex";
      if (status == 0) {
        EXPECT_EQ(device.blocked(), BST_RELEASING);
        EXPECT_TRUE(pthread_equal(device.no_wait_id, pthread_self()));
        device.Unlock();
      }
      return true;
    });

    device.dblock(BST_DOING_ACQUIRE);
    auto release
        = std::async(std::launch::async, [dcr] { return ReleaseDevice(dcr); });

    int waiting_threads = 0;
    // Wait up to five seconds for release to block; exit early if it completes.
    const auto deadline
        = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    do {
      device.Lock();
      waiting_threads = device.num_waiting;
      device.Unlock();
      if (waiting_threads > 0
          || release.wait_for(std::chrono::milliseconds(0))
                 == std::future_status::ready) {
        break;
      }
      std::this_thread::yield();
    } while (std::chrono::steady_clock::now() < deadline);

    device.Lock();
    const int writers_during_acquisition = device.num_writers;
    const bool open_during_acquisition = device.IsOpen();
    if (acquired) { ++device.num_writers; }
    device.dunblock(true);

    EXPECT_TRUE(release.get());
    EXPECT_EQ(waiting_threads, 1)
        << "Release bypassed another job's acquisition block";
    EXPECT_EQ(writers_during_acquisition, 1);
    EXPECT_TRUE(open_during_acquisition)
        << "Release closed the volume while another job was acquiring it";
    EXPECT_EQ(device.num_writers, acquired ? 1 : 0);
    EXPECT_EQ(device.IsOpen(), acquired);
    device.close(nullptr);
    FreeDeviceControlRecord(dcr);
  }
}

TEST_F(ReservationTest, ReleaseCanFinishItsOwnDespoolingBlock)
{
  // A canceled job still owns its despooling block and must release it cleanly.
  auto job = std::make_unique<TestJob>(111u);
  job->jcr->setJobStatus(JS_Canceled);
  SpoolDevice device;
  device.prt_name = GetPoolMemory(PM_FNAME);
  PmStrcpy(device.prt_name, "despooling device");
  device.num_writers = 1;
  auto* dcr = new DeviceControlRecord;
  dcr->jcr = job->jcr;
  dcr->SetDev(&device);
  dcr->keep_dcr = true;

  device.dblock(BST_DESPOOLING);
  EXPECT_TRUE(ReleaseDevice(dcr));
  EXPECT_FALSE(device.IsBlocked());
  EXPECT_EQ(device.num_writers, 0);
  FreeDeviceControlRecord(dcr);
}

TEST_F(ReservationTest, ReleaseWaitsForAnotherJobsFlush)
{
  // The first release holds its release block throughout the flush.
  auto job = std::make_unique<TestJob>(111u);
  // Cancellation skips the second job's own flush to isolate the wait.
  auto canceled_job = std::make_unique<TestJob>(112u);
  canceled_job->jcr->setJobStatus(JS_Canceled);
  ReleaseTestDevice device;
  device.prt_name = GetPoolMemory(PM_FNAME);
  PmStrcpy(device.prt_name, "flushing device");
  device.fd = 42;
  device.norewindonclose = true;
  device.num_writers = 2;
  DeviceResource resource{};
  auto* dcr = new DeviceControlRecord;
  dcr->jcr = job->jcr;
  dcr->SetDev(&device);
  dcr->device_resource = &resource;
  dcr->keep_dcr = true;
  auto* canceled_dcr = new DeviceControlRecord;
  canceled_dcr->jcr = canceled_job->jcr;
  canceled_dcr->SetDev(&device);
  canceled_dcr->keep_dcr = true;

  std::promise<void> flushing;
  std::promise<void> finish_flush;
  auto finish = finish_flush.get_future();
  EXPECT_CALL(device, d_close(42)).WillOnce(Return(0));
  EXPECT_CALL(device, d_flush(dcr)).WillOnce([&](DeviceControlRecord*) {
    flushing.set_value();
    finish.wait();
    return true;
  });
  auto release
      = std::async(std::launch::async, [dcr] { return ReleaseDevice(dcr); });
  flushing.get_future().wait();
  auto canceled_release = std::async(std::launch::async, [canceled_dcr] {
    return ReleaseDevice(canceled_dcr);
  });

  int waiting_threads = 0;
  // Wait for the second release to block, bounded to prevent a hung test.
  const auto deadline
      = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  do {
    device.Lock();
    waiting_threads = device.num_waiting;
    device.Unlock();
    if (waiting_threads > 0
        || canceled_release.wait_for(std::chrono::milliseconds(0))
               == std::future_status::ready) {
      break;
    }
    std::this_thread::yield();
  } while (std::chrono::steady_clock::now() < deadline);
  device.Lock();
  const int writers_during_flush = device.num_writers;
  device.Unlock();
  finish_flush.set_value();

  EXPECT_TRUE(release.get());
  EXPECT_TRUE(canceled_release.get());
  EXPECT_EQ(waiting_threads, 1) << "Release bypassed another job's flush";
  EXPECT_EQ(writers_during_flush, 2);
  EXPECT_EQ(device.num_writers, 0);
  EXPECT_FALSE(device.IsOpen());
  FreeDeviceControlRecord(canceled_dcr);
  FreeDeviceControlRecord(dcr);
}

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
