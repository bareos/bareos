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

#if defined(HAVE_MINGW)
#  include "include/bareos.h"
#  include "gtest/gtest.h"
#else
#  include "gtest/gtest.h"
#  include "include/bareos.h"
#endif


#include <chrono>
#include <future>
#include <thread>

#define STORAGE_DAEMON 1
#include "include/jcr.h"
#include "lib/crypto_cache.h"
#include "lib/edit.h"
#include "lib/parse_conf.h"
#include "stored/device_control_record.h"
#include "stored/sd_device_control_record.h"
#include "stored/acquire.h"
#include "stored/stored_jcr_impl.h"
#include "stored/job.h"
#include "stored/sd_plugins.h"
#include "stored/sd_stats.h"
#include "stored/stored.h"
#include "stored/stored_globals.h"
#include "stored/wait.h"
#include "stored/sd_backends.h"

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
  ResetWaitForDeviceTimeoutForTesting();
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

void WaitThenUnreserve(std::unique_ptr<TestJob>&);
void WaitThenUnreserve(std::unique_ptr<TestJob>&, std::chrono::milliseconds);
void WaitThenUnreserve(std::unique_ptr<TestJob>& job)
{
  WaitThenUnreserve(job, std::chrono::seconds(5));
}

void WaitThenUnreserve(std::unique_ptr<TestJob>& job,
                       std::chrono::milliseconds delay)
{
  std::this_thread::sleep_for(delay);
  job->jcr->sd_impl->dcr->UnreserveDevice();
  ReleaseDeviceCond();
}

static constexpr char kMountedAppendVolumeInfo[]
    = "1000 OK VolName=MountedVolume VolJobs=0 VolFiles=0 VolBlocks=0 "
      "VolBytes=0 VolMounts=0 VolErrors=0 VolWrites=0 MaxVolBytes=0 "
      "VolCapacityBytes=0 VolStatus=Append Slot=0 MaxVolJobs=0 "
      "MaxVolFiles=0 InChanger=1 VolReadTime=0 VolWriteTime=0 EndFile=0 "
      "EndBlock=0 LabelType=0 MediaId=1 EncryptionKey=dummy "
      "MinBlocksize=0 MaxBlocksize=0\n";

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

  EXPECT_CALL(*bsock, send()).WillRepeatedly(Return(true));

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

  EXPECT_CALL(*bsock, send()).WillRepeatedly(Return(true));

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

  EXPECT_CALL(*bsock, send()).WillRepeatedly(Return(true));

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

TEST_F(ReservationTest, wait_for_device_times_out)
{
  auto job = std::make_unique<TestJob>(111u);
  int retries = 0;

  job->jcr->sd_impl->device_wait_times.max_num_wait = 1;
  job->jcr->sd_impl->device_wait_times.wait_sec = 0;
  job->jcr->sd_impl->device_wait_times.rem_wait_sec = 0;
  SetWaitForDeviceTimeoutForTesting(0);

  ASSERT_EQ(WaitForDevice(job->jcr, retries), false);
  ASSERT_EQ(retries, 1);
}

TEST_F(ReservationTest, wait_for_device_uses_total_wait_budget)
{
  auto job = std::make_unique<TestJob>(111u);
  int retries = 0;

  job->jcr->sd_impl->device_wait_times.max_num_wait = 2;
  job->jcr->sd_impl->device_wait_times.wait_sec = 0;
  job->jcr->sd_impl->device_wait_times.rem_wait_sec = 0;
  SetWaitForDeviceTimeoutForTesting(0);

  ASSERT_EQ(WaitForDevice(job->jcr, retries), true);
  ASSERT_EQ(WaitForDevice(job->jcr, retries), false);
  ASSERT_EQ(retries, 2);
}

TEST_F(ReservationTest, use_cmd_reserve_read_retries_before_waiting)
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

  SetWaitForDeviceTimeoutForTesting(0);
  bsock->recv();
  auto future
      = std::async(std::launch::async, [&job2] { return use_cmd(job2->jcr); });
  std::this_thread::sleep_for(std::chrono::milliseconds(5));
  job1->jcr->sd_impl->dcr->UnreserveDevice();
  ReleaseDeviceCond();

  ASSERT_EQ(future.wait_for(std::chrono::seconds(1)),
            std::future_status::ready);
  ASSERT_EQ(future.get(), true);
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

TEST_F(ReservationTest, append_found_in_use_releases_retry_reservation)
{
  auto bsock = std::make_unique<BareosSocketMock>();
  auto job1 = std::make_unique<TestJob>(111u);
  auto job2 = std::make_unique<TestJob>(222u);
  job1->jcr->dir_bsock = job2->jcr->dir_bsock = bsock.get();

  auto configure_append_storage
      = [](TestJob& job, std::initializer_list<const char*> devices) {
          auto& stores = job.jcr->sd_impl->dirstores;
          stores.clear();
          stores.emplace_back(true, "sssss", "File", "ppppp", "ptptp");
          for (const auto* device : devices) {
            stores.front().device_names.push_back(device);
          }
        };

  configure_append_storage(*job1, {"auto1dev2"});
  configure_append_storage(*job2, {"auto1dev3", "auto1dev2"});

  EXPECT_CALL(*bsock, recv())
      .WillOnce(BSOCK_RECV(
          bsock.get(),
          kMountedAppendVolumeInfo))  // DirFindNextAppendableVolume for job1
      .WillOnce(BSOCK_RECV(
          bsock.get(),
          kMountedAppendVolumeInfo))  // mounted volume is in use on auto1dev2
      .WillOnce(
          BSOCK_RECV(bsock.get(), "1901 No Media."));  // stop free-drive search

  EXPECT_CALL(*bsock, send()).WillRepeatedly(Return(true));

  ASSERT_TRUE(TryReserveAfterUse(job1->jcr, true));
  ASSERT_STREQ(job1->jcr->sd_impl->dcr->dev->device_resource->resource_name_,
               "auto1dev2");
  ASSERT_STREQ(job1->jcr->sd_impl->dcr->VolumeName, "MountedVolume");

  auto* job2_dcr = new StorageDaemonDeviceControlRecord;
  SetupNewDcrDevice(job2->jcr, job2_dcr, nullptr, nullptr);
  job2_dcr->SetWillWrite();
  job2->jcr->sd_impl->dcr = job2_dcr;

  ReserveContext rctx{};
  rctx.append = true;
  rctx.store = &job2->jcr->sd_impl->dirstores.front();
  rctx.device_name
      = job2->jcr->sd_impl->dirstores.front().device_names.front().c_str();

  ASSERT_EQ(SearchResForDevice(job2->jcr, rctx), -1);
  ASSERT_TRUE(rctx.PreferMountedVols);
  ASSERT_FALSE(job2_dcr->IsReserved());
  ASSERT_EQ(job2_dcr->dev->NumReserved(), 0);
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

  EXPECT_CALL(*bsock, send()).WillRepeatedly(Return(true));

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
