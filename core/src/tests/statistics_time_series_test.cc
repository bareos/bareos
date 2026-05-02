/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2026-2026 Bareos GmbH & Co. KG

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

#include "dird/statistics_time_series.h"

#include <cstdlib>
#include <filesystem>

using directordaemon::BuildDefaultStatisticsTimeSeriesArchives;
using directordaemon::CreateRrdStatisticsTimeSeriesBackend;
using directordaemon::GetDefaultStatisticsTimeSeriesDirectory;
using directordaemon::GetDeviceStatisticsTimeSeriesPath;
using directordaemon::GetJobStatisticsTimeSeriesPath;
using directordaemon::GetPoolStatisticsTimeSeriesPath;
using directordaemon::GetSanitizedStatisticsTimeSeriesComponent;
using directordaemon::GetStatisticsTimeSeriesStepSeconds;
using directordaemon::GetSystemStatisticsTimeSeriesPath;
using directordaemon::PoolStatisticsTimeSeriesSample;
using directordaemon::ReadPoolStatisticsTimeSeries;
using directordaemon::ReadSystemStatisticsTimeSeries;
using directordaemon::StatisticsTimeSeriesConsolidationFunction;
using directordaemon::StatisticsTimeSeriesData;
using directordaemon::SystemStatisticsTimeSeriesSample;

namespace {

std::filesystem::path CreateTemporaryStatisticsDirectory()
{
  std::string pattern = (std::filesystem::temp_directory_path()
                         / "bareos-statistics-time-series-XXXXXX")
                            .string();
  char* path = mkdtemp(pattern.data());
  EXPECT_NE(path, nullptr);
  return path ? std::filesystem::path(path)
              : std::filesystem::temp_directory_path();
}

}  // namespace

TEST(StatisticsTimeSeriesTest, UsesWorkingDirectoryByDefault)
{
  EXPECT_EQ(GetDefaultStatisticsTimeSeriesDirectory("/var/lib/bareos"),
            "/var/lib/bareos/statistics/rrd");
  EXPECT_EQ(GetDefaultStatisticsTimeSeriesDirectory("/var/lib/bareos/"),
            "/var/lib/bareos/statistics/rrd");
  EXPECT_EQ(GetDefaultStatisticsTimeSeriesDirectory(nullptr), "statistics/rrd");
}

TEST(StatisticsTimeSeriesTest, BuildsArchiveLayoutFromStep)
{
  const auto archives = BuildDefaultStatisticsTimeSeriesArchives(60);

  ASSERT_EQ(archives.size(), 4u);
  EXPECT_EQ(archives[0].resolution_seconds, 60u);
  EXPECT_EQ(archives[0].rows, 1440u);
  EXPECT_EQ(archives[1].resolution_seconds, 300u);
  EXPECT_EQ(archives[1].rows, 8640u);
  EXPECT_EQ(archives[2].resolution_seconds, 3600u);
  EXPECT_EQ(archives[2].rows, 8760u);
  EXPECT_EQ(archives[3].resolution_seconds, 86400u);
  EXPECT_EQ(archives[3].rows, 3650u);
}

TEST(StatisticsTimeSeriesTest, BuildsStableSeriesPaths)
{
  EXPECT_EQ(GetDeviceStatisticsTimeSeriesPath("/tmp/rrd", "Main Storage",
                                              "File Device/1"),
            "/tmp/rrd/devices/Main_Storage__File_Device_1.rrd");
  EXPECT_EQ(GetJobStatisticsTimeSeriesPath("/tmp/rrd", 1234),
            "/tmp/rrd/jobs/job-1234.rrd");
  EXPECT_EQ(GetPoolStatisticsTimeSeriesPath("/tmp/rrd", "Full Pool"),
            "/tmp/rrd/pools/Full_Pool.rrd");
  EXPECT_EQ(GetSystemStatisticsTimeSeriesPath("/tmp/rrd"),
            "/tmp/rrd/system/system.rrd");
}

TEST(StatisticsTimeSeriesTest, SanitizesSeriesComponents)
{
  EXPECT_EQ(GetSanitizedStatisticsTimeSeriesComponent(""), "unnamed");
  EXPECT_EQ(GetSanitizedStatisticsTimeSeriesComponent("Daily Full"),
            "Daily_Full");
  EXPECT_EQ(GetSanitizedStatisticsTimeSeriesComponent("a/b::c"), "a_b_c");
}

TEST(StatisticsTimeSeriesTest, UsesSafeMinimumStep)
{
  EXPECT_EQ(GetStatisticsTimeSeriesStepSeconds(0), 60u);
  EXPECT_EQ(GetStatisticsTimeSeriesStepSeconds(15), 60u);
  EXPECT_EQ(GetStatisticsTimeSeriesStepSeconds(120), 120u);
}

TEST(StatisticsTimeSeriesTest, ReadsBackStoredSystemSeries)
{
  const auto directory = CreateTemporaryStatisticsDirectory();
  auto backend = CreateRrdStatisticsTimeSeriesBackend(directory.string(), 60u);

  const time_t sample_one = 1700000040;
  const time_t sample_two = sample_one + 60;
  backend->StoreSystemStatistics(
      nullptr, SystemStatisticsTimeSeriesSample{sample_one, 2, 8, 1024, 10});
  backend->StoreSystemStatistics(
      nullptr, SystemStatisticsTimeSeriesSample{sample_two, 3, 9, 2048, 20});
  backend->Flush(nullptr);

  StatisticsTimeSeriesData series;
  ASSERT_TRUE(ReadSystemStatisticsTimeSeries(
      directory.string(), StatisticsTimeSeriesConsolidationFunction::kLast,
      sample_one - 60, sample_two, &series));
  ASSERT_TRUE(series.available);
  EXPECT_EQ(series.data_source_names,
            std::vector<std::string>(
                {"num_pools", "num_volumes", "total_bytes", "total_files"}));

  ASSERT_FALSE(series.points.empty());
  const auto& point = series.points.back();
  ASSERT_EQ(point.values.size(), 4u);
  EXPECT_DOUBLE_EQ(point.values[0], 3);
  EXPECT_DOUBLE_EQ(point.values[1], 9);
  EXPECT_DOUBLE_EQ(point.values[2], 2048);
  EXPECT_DOUBLE_EQ(point.values[3], 20);

  std::filesystem::remove_all(directory);
}

TEST(StatisticsTimeSeriesTest, ReadsBackStoredPoolSeries)
{
  const auto directory = CreateTemporaryStatisticsDirectory();
  auto backend = CreateRrdStatisticsTimeSeriesBackend(directory.string(), 60u);

  const time_t sample_one = 1700000220;
  const time_t sample_two = sample_one + 60;
  backend->StorePoolStatistics(
      nullptr,
      PoolStatisticsTimeSeriesSample{sample_one, "Full", 4, 4096, 100, 0, 12});
  backend->StorePoolStatistics(
      nullptr,
      PoolStatisticsTimeSeriesSample{sample_two, "Full", 5, 8192, 200, 0, 12});
  backend->Flush(nullptr);

  StatisticsTimeSeriesData series;
  ASSERT_TRUE(ReadPoolStatisticsTimeSeries(
      directory.string(), "Full",
      StatisticsTimeSeriesConsolidationFunction::kLast, sample_one - 60,
      sample_two, &series));
  ASSERT_TRUE(series.available);
  EXPECT_EQ(
      series.data_source_names,
      std::vector<std::string>({"num_volumes", "total_bytes", "total_files",
                                "max_vol_bytes", "max_volumes"}));

  ASSERT_FALSE(series.points.empty());
  const auto& point = series.points.back();
  ASSERT_EQ(point.values.size(), 5u);
  EXPECT_DOUBLE_EQ(point.values[0], 5);
  EXPECT_DOUBLE_EQ(point.values[1], 8192);
  EXPECT_DOUBLE_EQ(point.values[2], 200);
  EXPECT_DOUBLE_EQ(point.values[3], 0);
  EXPECT_DOUBLE_EQ(point.values[4], 12);

  std::filesystem::remove_all(directory);
}
