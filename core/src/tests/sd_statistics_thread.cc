/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2021-2026 Bareos GmbH & Co. KG

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

#include "testing_sd_common.h"
#include "stored/sd_stats.h"

class SdStatisticsThread : public ::testing::Test {
  void SetUp() override { InitSdGlobals(); }
};

static void test_starting_statistics_thread(std::string path_to_config)
{
  PConfigParser storage_config(
      storagedaemon::InitSdConfig(path_to_config.c_str(), M_INFO));
  storagedaemon::my_config = storage_config.get();
  if (!storage_config) {
    GTEST_SKIP() << "Could not initialize storage config";
  }

  if (!storage_config->ParseConfig()) {
    GTEST_SKIP() << "Could not parse storage config";
  }

  storagedaemon::me
      = (storagedaemon::StorageResource*)storage_config->GetNextRes(
          storagedaemon::R_STORAGE, nullptr);
  storagedaemon::my_config->own_resource_ = storagedaemon::me;

  bool started = storagedaemon::StartStatisticsThread();
  EXPECT_TRUE(started);
  if (started) { storagedaemon::StopStatisticsThread(); }
}

TEST_F(SdStatisticsThread, default_collect_statistics)
{
  std::string path_to_config = std::string(
      "configs/statistics_thread/sd_statistics_thread/default_config");

  test_starting_statistics_thread(path_to_config);
}

TEST_F(SdStatisticsThread, only_interval_set)
{
  std::string path_to_config = std::string(
      "configs/statistics_thread/sd_statistics_thread/only_collect_set");

  test_starting_statistics_thread(path_to_config);
}

TEST_F(SdStatisticsThread, only_collect_set)
{
  std::string path_to_config = std::string(
      "configs/statistics_thread/sd_statistics_thread/only_interval_set");

  test_starting_statistics_thread(path_to_config);
}
