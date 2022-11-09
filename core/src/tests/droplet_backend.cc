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
#include "stored/sd_backends.h"

#define CONFIG_SUBDIR "droplet_backend"
#include "sd_backend_tests.h"

using namespace storagedaemon;

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
}

// write-request crossing the chunk-border by exactly 1 byte
TEST_F(sd, droplet_off_by_one_long)
{
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
}

// write-request hitting chunk-border exactly
TEST_F(sd, droplet_aligned)
{
  using namespace std::string_literals;
  std::vector<std::vector<char>> test_data;
  for (char& c : "0123456789abcdefghijklmnopqrstuvwxyz"s) {
    std::vector<char> tmp(1024 * 1024);
    std::fill(tmp.begin(), tmp.end(), c);
    test_data.push_back(tmp);
  }

  droplet_write_reread_testdata(test_data);
}

// write-request same size as chunk
TEST_F(sd, droplet_fullchunk)
{
  using namespace std::string_literals;
  std::vector<std::vector<char>> test_data;
  for (char& c : "0123"s) {
    std::vector<char> tmp(10 * 1024 * 1024);
    std::fill(tmp.begin(), tmp.end(), c);
    test_data.push_back(tmp);
  }

  droplet_write_reread_testdata(test_data);
}

// write-request larger than chunk
TEST_F(sd, droplet_oversized_write)
{
  using namespace std::string_literals;
  std::vector<std::vector<char>> test_data;
  for (char& c : "0123"s) {
    std::vector<char> tmp(11 * 1024 * 1024);
    std::fill(tmp.begin(), tmp.end(), c);
    test_data.push_back(tmp);
  }

  droplet_write_reread_testdata(test_data);
}

// write-request larger than two chunks
TEST_F(sd, droplet_double_oversized_write)
{
  using namespace std::string_literals;
  std::vector<std::vector<char>> test_data;
  for (char& c : "0123"s) {
    std::vector<char> tmp(21 * 1024 * 1024);
    std::fill(tmp.begin(), tmp.end(), c);
    test_data.push_back(tmp);
  }

  droplet_write_reread_testdata(test_data);
}
