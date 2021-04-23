/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2021-2021 Bareos GmbH & Co. KG

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

#define STORAGE_DAEMON 1
#include "include/jcr.h"
#include "lib/crypto_cache.h"
#include "lib/edit.h"
#include "lib/parse_conf.h"
#include "stored/butil.h"
#include "stored/device_control_record.h"
#include "stored/jcr_private.h"
#include "stored/job.h"
#include "stored/sd_plugins.h"
#include "stored/sd_stats.h"
#include "stored/stored.h"
#include "stored/stored_globals.h"
#include "stored/wait.h"
#if defined(HAVE_DYNAMIC_SD_BACKENDS)
#  include "stored/sd_backends.h"
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

class sd : public ::testing::Test {
 protected:
  void SetUp() override;
  void TearDown() override;
};

void sd::SetUp()
{
  OSDependentInit();
  InitMsg(NULL, NULL);

  debug_level = 1000;

  /* configfile is a global char* from stored_globals.h */
  configfile = strdup(RELATIVE_PROJECT_SOURCE_DIR "/configs/sd_backend/");
  my_config = InitSdConfig(configfile, M_ERROR_TERM);
  ParseSdConfig(configfile, M_ERROR_TERM);
  /*
   * we do not run CheckResources() here, so take care the test configration
   * is not broken. Also autochangers will not work.
   */
}
void sd::TearDown()
{
  Dmsg0(100, "TearDown start\n");

  {
    DeviceResource* d = nullptr;
    foreach_res (d, R_DEVICE) {
      Dmsg1(10, "Term device %s (%s)\n", d->resource_name_,
            d->archive_device_string);
      if (d->dev) {
        d->dev->ClearVolhdr();
        delete d->dev;
        d->dev = nullptr;
      }
    }
  }
#if defined(HAVE_DYNAMIC_SD_BACKENDS)
  FlushAndCloseBackendDevices();
#endif

  if (configfile) { free(configfile); }
  if (my_config) { delete my_config; }

  TermMsg();
  TermReservationsLock();

  CloseMemoryPool();
  debug_level = 0;
  CloseMemoryPool();
}

// Test that load and unloads a tape device.
TEST_F(sd, backend_load_unload)
{
  const char* name = "sd_backend_test";
  char dev_name[10] = "tape1";

  JobControlRecord* jcr = SetupDummyJcr(name, nullptr, nullptr);
  ASSERT_TRUE(jcr);

  DeviceResource* device_resource
      = (DeviceResource*)my_config->GetResWithName(R_DEVICE, dev_name);

  Device* dev = FactoryCreateDevice(jcr, device_resource);
  ASSERT_TRUE(dev);

  Dmsg0(100, "open\n");
  /*
   * Open device. Calling d_open directly,
   * because otherwise OpenDevice()/open()
   * would also try IOCTLs on the tape device,
   * which will fail on our dummy device.
   */
  dev->fd = dev->d_open("/dev/null", 0, 0640);
  ASSERT_TRUE(dev->fd > 0);

  /*
   * always true on generic (disk) device.
   * On /dev/null used as tape it will fail.
   */
  ASSERT_FALSE(dev->offline());

  Dmsg0(100, "cleanup dev \n");
  delete dev;

  Dmsg0(100, "cleanup\n");
  FreeJcr(jcr);
}
