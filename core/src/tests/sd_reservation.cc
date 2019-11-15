/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2007-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2019 Bareos GmbH & Co. KG

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

#include <chrono>
#include <future>

#define STORAGE_DAEMON 1
#include "gtest/gtest.h"
#include "include/bareos.h"
#include "include/jcr.h"
#include "lib/crypto_cache.h"
#include "lib/edit.h"
#include "lib/parse_conf.h"
#include "stored/jcr_private.h"
#include "stored/job.h"
#include "stored/sd_plugins.h"
#include "stored/sd_stats.h"
#include "stored/stored.h"
#include "stored/stored_globals.h"
#include "stored/wait.h"
#if defined(HAVE_DYNAMIC_SD_BACKENDS)
#include "stored/sd_backends.h"
#endif

#include "bsock_mock.h"
#include "include/make_unique.h"

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
  InitMsg(NULL, NULL);
  OSDependentInit();

  /* configfile is a global char* from stored_globals.h */
  configfile =
      strdup(RELATIVE_PROJECT_SOURCE_DIR "/src/tests/configs/sd_reservation/");
  my_config = InitSdConfig(configfile, M_ERROR_TERM);
  ParseSdConfig(configfile, M_ERROR_TERM);
  /*
   * we do not run CheckResources() here, so take care the test configration
   * is not broken. Also autochangers will not work.
   */

  InitReservationsLock();
  CreateVolumeLists();
}
void ReservationTest::TearDown()
{
  FreeVolumeLists();

  {
    DeviceResource* device;
    foreach_res (device, R_DEVICE) {
      Dmsg1(10, "Term device %s\n", device->device_name);
      if (device->dev) {
        device->dev->ClearVolhdr();
        device->dev->term();
        device->dev = NULL;
      }
    }
  }
#if defined(HAVE_DYNAMIC_SD_BACKENDS)
  DevFlushBackends();
#endif

  if (configfile) { free(configfile); }
  if (my_config) { delete my_config; }

  TermMsg();
  TermReservationsLock();

  CloseMemoryPool();
  debug_level = 0;
  CloseMemoryPool();
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
void WaitThenUnreserve(std::unique_ptr<TestJob>& job)
{
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  job->jcr->impl->dcr->UnreserveDevice();
  ReleaseDeviceCond();
}

/*
 * Test that an illegal command passed to use_cmd will fail gracefully
 */
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

/*
 * Test reserving a device for read works correctly
 */
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

/*
 * Test reserving broken device
 */
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

/*
 * Test reserving device with wrong mediatype
 */
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

/*
 * Test reserving a reserved device and wait for it to become free
 */
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
