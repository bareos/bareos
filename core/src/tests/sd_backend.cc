/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2021-2022 Bareos GmbH & Co. KG

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

  debug_level = 900;

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

  if (configfile) { free(configfile); }
  if (my_config) { delete my_config; }

  TermReservationsLock();
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

void droplet_write_reread_testdata(std::vector<std::vector<char>>& test_data)
{
  const char* name = "sd_backend_test";
  const char* dev_name = "droplet";
  const char* volname
      = ::testing::UnitTest::GetInstance()->current_test_info()->name();

  JobControlRecord* jcr = SetupDummyJcr(name, nullptr, nullptr);
  ASSERT_TRUE(jcr);

  DeviceResource* device_resource
      = (DeviceResource*)my_config->GetResWithName(R_DEVICE, dev_name);

  Device* dev = FactoryCreateDevice(jcr, device_resource);
  ASSERT_TRUE(dev);

  // write to device
  {
    dev->setVolCatName(volname);
    auto fd = dev->d_open(volname, O_CREAT | O_RDWR | O_BINARY, 0640);
    dev->d_truncate(
        nullptr);  // dcr parameter is unused, so nullptr should be fine

    for (auto& buf : test_data) { dev->d_write(fd, buf.data(), buf.size()); }
    dev->d_close(fd);
  }

  // read from device
  {
    dev->setVolCatName(volname);
    auto fd = dev->d_open(volname, O_CREAT | O_RDWR | O_BINARY, 0640);

    for (auto& buf : test_data) {
      std::vector<char> tmp(buf.size());
      dev->d_read(fd, tmp.data(), buf.size());
      ASSERT_EQ(buf, tmp);
    }
    dev->d_close(fd);
  }
  delete dev;
  FreeJcr(jcr);
}

// write-request writing 1 byte to this chunk and rest to next chunk
TEST_F(sd, droplet_off_by_one_short)
{
#if !defined HAVE_DROPLET
  std::cerr << "\nThis test requires droplet backend, which has not been "
               "built. Skipping.\n";
  exit(77);
#else
  using namespace std::string_literals;
  std::vector<std::vector<char>> test_data;

  {
    std::vector<char> tmp(1024 * 1024 - 1);
    std::fill(tmp.begin(), tmp.end(), '0');
    test_data.push_back(tmp);
  }
  for (char& c : "123456789abcdefghijklmnopqrstuvwxyz"s) {
    std::vector<char> tmp(1024 * 1024);
    std::fill(tmp.begin(), tmp.end(), c);
    test_data.push_back(tmp);
  }

  droplet_write_reread_testdata(test_data);
#endif
}

// write-request crossing the chunk-border by exactly 1 byte
TEST_F(sd, droplet_off_by_one_long)
{
#if !defined HAVE_DROPLET
  std::cerr << "\nThis test requires droplet backend, which has not been "
               "built. Skipping.\n";
  exit(77);
#else
  using namespace std::string_literals;
  std::vector<std::vector<char>> test_data;

  {
    std::vector<char> tmp(1024 * 1024 + 1);
    std::fill(tmp.begin(), tmp.end(), '0');
    test_data.push_back(tmp);
  }
  for (char& c : "123456789abcdefghijklmnopqrstuvwxyz"s) {
    std::vector<char> tmp(1024 * 1024);
    std::fill(tmp.begin(), tmp.end(), c);
    test_data.push_back(tmp);
  }

  droplet_write_reread_testdata(test_data);
#endif
}

// write-request hitting chunk-border exactly
TEST_F(sd, droplet_aligned)
{
#if !defined HAVE_DROPLET
  std::cerr << "\nThis test requires droplet backend, which has not been "
               "built. Skipping.\n";
  exit(77);
#else
  using namespace std::string_literals;
  std::vector<std::vector<char>> test_data;
  for (char& c : "0123456789abcdefghijklmnopqrstuvwxyz"s) {
    std::vector<char> tmp(1024 * 1024);
    std::fill(tmp.begin(), tmp.end(), c);
    test_data.push_back(tmp);
  }

  droplet_write_reread_testdata(test_data);
#endif
}

// write-request same size as chunk
TEST_F(sd, droplet_fullchunk)
{
#if !defined HAVE_DROPLET
  std::cerr << "\nThis test requires droplet backend, which has not been "
               "built. Skipping.\n";
  exit(77);
#else
  using namespace std::string_literals;
  std::vector<std::vector<char>> test_data;
  for (char& c : "0123"s) {
    std::vector<char> tmp(10 * 1024 * 1024);
    std::fill(tmp.begin(), tmp.end(), c);
    test_data.push_back(tmp);
  }

  droplet_write_reread_testdata(test_data);
#endif
}

// write-request larger than chunk
TEST_F(sd, droplet_oversized_write)
{
#if !defined HAVE_DROPLET
  std::cerr << "\nThis test requires droplet backend, which has not been "
               "built. Skipping.\n";
  exit(77);
#else
  using namespace std::string_literals;
  std::vector<std::vector<char>> test_data;
  for (char& c : "0123"s) {
    std::vector<char> tmp(11 * 1024 * 1024);
    std::fill(tmp.begin(), tmp.end(), c);
    test_data.push_back(tmp);
  }

  droplet_write_reread_testdata(test_data);
#endif
}

// write-request larger than two chunks
TEST_F(sd, droplet_double_oversized_write)
{
#if !defined HAVE_DROPLET
  std::cerr << "\nThis test requires droplet backend, which has not been "
               "built. Skipping.\n";
  exit(77);
#else
  using namespace std::string_literals;
  std::vector<std::vector<char>> test_data;
  for (char& c : "0123"s) {
    std::vector<char> tmp(21 * 1024 * 1024);
    std::fill(tmp.begin(), tmp.end(), c);
    test_data.push_back(tmp);
  }

  droplet_write_reread_testdata(test_data);
#endif
}
