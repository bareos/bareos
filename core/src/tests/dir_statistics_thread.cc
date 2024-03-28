/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2021-2024 Bareos GmbH & Co. KG

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

#include "testing_dir_common.h"
#include "dird/stats.h"

#ifdef _MSC_VER
char* optarg {};
#endif


class DirStatisticsThread : public ::testing::Test {
  void SetUp() override { InitDirGlobals(); }
};

static void test_starting_statistics_thread(std::string path_to_config)
{
  PConfigParser director_config(DirectorPrepareResources(path_to_config));
  if (!director_config) { return; }

  EXPECT_FALSE(directordaemon::StartStatisticsThread());
}

TEST_F(DirStatisticsThread, default_collect_statistics)
{
  std::string path_to_config = std::string(
      "configs/statistics_thread/dir_statistics_thread/default_config");

  test_starting_statistics_thread(path_to_config);
}

TEST_F(DirStatisticsThread, only_interval_set)
{
  std::string path_to_config = std::string(
      "configs/statistics_thread/dir_statistics_thread/only_collect_set");

  test_starting_statistics_thread(path_to_config);
}

TEST_F(DirStatisticsThread, only_collect_set)
{
  std::string path_to_config = std::string(
      "configs/statistics_thread/dir_statistics_thread/only_interval_set");

  test_starting_statistics_thread(path_to_config);
}
