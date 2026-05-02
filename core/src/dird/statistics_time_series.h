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
#ifndef BAREOS_DIRD_STATISTICS_TIME_SERIES_H_
#define BAREOS_DIRD_STATISTICS_TIME_SERIES_H_

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <limits>
#include <memory>
#include <string>
#include <vector>

struct StorageRuntimeSnapshot;
class JobControlRecord;

namespace directordaemon {
struct StatisticsTimeSeriesArchive {
  uint32_t resolution_seconds = 0;
  uint32_t rows = 0;
};

enum class StatisticsTimeSeriesConsolidationFunction
{
  kAverage,
  kMin,
  kMax,
  kLast,
};

inline const char* StatisticsTimeSeriesConsolidationFunctionToString(
    StatisticsTimeSeriesConsolidationFunction cf)
{
  switch (cf) {
    case StatisticsTimeSeriesConsolidationFunction::kAverage:
      return "AVERAGE";
    case StatisticsTimeSeriesConsolidationFunction::kMin:
      return "MIN";
    case StatisticsTimeSeriesConsolidationFunction::kMax:
      return "MAX";
    case StatisticsTimeSeriesConsolidationFunction::kLast:
      return "LAST";
  }

  return "LAST";
}

inline bool ParseStatisticsTimeSeriesConsolidationFunction(
    const std::string& raw_value,
    StatisticsTimeSeriesConsolidationFunction* result)
{
  std::string value;
  value.reserve(raw_value.size());
  for (unsigned char ch : raw_value) {
    value.push_back(static_cast<char>(std::tolower(ch)));
  }

  if (value == "average" || value == "avg") {
    *result = StatisticsTimeSeriesConsolidationFunction::kAverage;
    return true;
  }
  if (value == "min") {
    *result = StatisticsTimeSeriesConsolidationFunction::kMin;
    return true;
  }
  if (value == "max") {
    *result = StatisticsTimeSeriesConsolidationFunction::kMax;
    return true;
  }
  if (value == "last") {
    *result = StatisticsTimeSeriesConsolidationFunction::kLast;
    return true;
  }

  return false;
}

inline std::string GetDefaultStatisticsTimeSeriesDirectory(
    const char* working_directory)
{
  if (!working_directory || !working_directory[0]) { return "statistics/rrd"; }

  std::string result{working_directory};
  if (result.back() != '/') { result += '/'; }
  result += "statistics/rrd";
  return result;
}

inline std::vector<StatisticsTimeSeriesArchive>
BuildDefaultStatisticsTimeSeriesArchives(uint32_t step_seconds)
{
  static constexpr struct {
    uint32_t target_resolution;
    uint32_t retention_seconds;
  } archive_targets[] = {
      {60, 24u * 60u * 60u},
      {5u * 60u, 30u * 24u * 60u * 60u},
      {60u * 60u, 365u * 24u * 60u * 60u},
      {24u * 60u * 60u, 3650u * 24u * 60u * 60u},
  };

  std::vector<StatisticsTimeSeriesArchive> archives;
  const uint32_t step = std::max(step_seconds, 1u);

  for (const auto& target : archive_targets) {
    const uint32_t pdp_steps
        = std::max(1u, (target.target_resolution + step - 1u) / step);
    const uint32_t resolution_seconds = pdp_steps * step;
    const uint32_t rows
        = std::max(1u, target.retention_seconds / resolution_seconds);

    if (!archives.empty()
        && archives.back().resolution_seconds == resolution_seconds) {
      archives.back().rows = std::max(archives.back().rows, rows);
      continue;
    }

    archives.push_back({resolution_seconds, rows});
  }

  return archives;
}

inline uint32_t GetStatisticsTimeSeriesStepSeconds(uint32_t configured_seconds)
{
  return std::max(configured_seconds, 60u);
}

inline std::string GetSanitizedStatisticsTimeSeriesComponent(
    const std::string& component)
{
  std::string result;
  result.reserve(component.size());

  bool previous_was_separator = false;
  for (unsigned char ch : component) {
    const bool allowed
        = std::isalnum(ch) || ch == '-' || ch == '_' || ch == '.';
    if (allowed) {
      result.push_back(static_cast<char>(ch));
      previous_was_separator = false;
      continue;
    }

    if (!previous_was_separator) {
      result.push_back('_');
      previous_was_separator = true;
    }
  }

  if (result.empty()) { return "unnamed"; }
  return result;
}

inline std::string GetDeviceStatisticsTimeSeriesPath(
    const std::string& base_directory,
    const std::string& storage_name,
    const std::string& device_name)
{
  return base_directory + "/devices/"
         + GetSanitizedStatisticsTimeSeriesComponent(storage_name) + "__"
         + GetSanitizedStatisticsTimeSeriesComponent(device_name) + ".rrd";
}

inline std::string GetJobStatisticsTimeSeriesPath(
    const std::string& base_directory,
    uint32_t job_id)
{
  return base_directory + "/jobs/job-" + std::to_string(job_id) + ".rrd";
}

inline std::string GetPoolStatisticsTimeSeriesPath(
    const std::string& base_directory,
    const std::string& pool_name)
{
  return base_directory + "/pools/"
         + GetSanitizedStatisticsTimeSeriesComponent(pool_name) + ".rrd";
}

inline std::string GetSystemStatisticsTimeSeriesPath(
    const std::string& base_directory)
{
  return base_directory + "/system/system.rrd";
}

struct JobRuntimeStatisticsSample {
  time_t sample_time = 0;
  uint32_t job_id = 0;
  uint32_t job_files = 0;
  uint64_t job_bytes = 0;
  uint32_t average_rate = 0;
  uint32_t last_rate = 0;
  int32_t job_status = 0;
  bool spooling = false;
  bool despooling = false;
  bool despool_wait = false;
};

struct DeviceRuntimeStatisticsSample {
  time_t sample_time = 0;
  std::string storage_name{};
  std::string device_name{};
  uint32_t running_jobs = 0;
  uint64_t job_files = 0;
  uint64_t job_bytes = 0;
  uint64_t average_rate = 0;
  uint64_t last_rate = 0;
  bool spooling = false;
  bool despooling = false;
  bool despool_wait = false;
};

struct PoolStatisticsTimeSeriesSample {
  time_t sample_time = 0;
  std::string pool_name{};
  uint32_t num_volumes = 0;
  uint64_t total_bytes = 0;
  uint64_t total_files = 0;
  uint64_t max_vol_bytes = 0;
  uint32_t max_volumes = 0;
};

struct SystemStatisticsTimeSeriesSample {
  time_t sample_time = 0;
  uint32_t num_pools = 0;
  uint32_t num_volumes = 0;
  uint64_t total_bytes = 0;
  uint64_t total_files = 0;
};

struct StatisticsTimeSeriesPoint {
  time_t sample_time = 0;
  std::vector<double> values{};
};

struct StatisticsTimeSeriesData {
  bool available = false;
  time_t start_time = 0;
  time_t end_time = 0;
  uint32_t step_seconds = 0;
  std::vector<std::string> data_source_names{};
  std::vector<StatisticsTimeSeriesPoint> points{};
};

class StatisticsTimeSeriesBackend {
 public:
  virtual ~StatisticsTimeSeriesBackend() = default;

  virtual void StoreDeviceStatistics(
      JobControlRecord* jcr,
      const DeviceRuntimeStatisticsSample& sample)
      = 0;
  virtual void StoreJobStatistics(JobControlRecord* jcr,
                                  const JobRuntimeStatisticsSample& sample)
      = 0;
  virtual void StorePoolStatistics(JobControlRecord* jcr,
                                   const PoolStatisticsTimeSeriesSample& sample)
      = 0;
  virtual void StoreSystemStatistics(
      JobControlRecord* jcr,
      const SystemStatisticsTimeSeriesSample& sample)
      = 0;
  virtual void Flush(JobControlRecord* jcr) = 0;
};

std::unique_ptr<StatisticsTimeSeriesBackend> CreateStatisticsTimeSeriesBackend(
    JobControlRecord* jcr);

std::unique_ptr<StatisticsTimeSeriesBackend>
CreateRrdStatisticsTimeSeriesBackend(const std::string& base_directory,
                                     uint32_t step_seconds);

void StoreStorageRuntimeSnapshot(JobControlRecord* jcr,
                                 const ::StorageRuntimeSnapshot& snapshot);
void SampleCatalogStatisticsTimeSeries(JobControlRecord* jcr);
void FlushStatisticsTimeSeries(JobControlRecord* jcr);

std::string GetConfiguredStatisticsTimeSeriesDirectory();

bool ReadJobStatisticsTimeSeries(const std::string& base_directory,
                                 uint32_t job_id,
                                 StatisticsTimeSeriesConsolidationFunction cf,
                                 time_t start_time,
                                 time_t end_time,
                                 StatisticsTimeSeriesData* result);
bool ReadDeviceStatisticsTimeSeries(
    const std::string& base_directory,
    const std::string& storage_name,
    const std::string& device_name,
    StatisticsTimeSeriesConsolidationFunction cf,
    time_t start_time,
    time_t end_time,
    StatisticsTimeSeriesData* result);
bool ReadPoolStatisticsTimeSeries(const std::string& base_directory,
                                  const std::string& pool_name,
                                  StatisticsTimeSeriesConsolidationFunction cf,
                                  time_t start_time,
                                  time_t end_time,
                                  StatisticsTimeSeriesData* result);
bool ReadSystemStatisticsTimeSeries(
    const std::string& base_directory,
    StatisticsTimeSeriesConsolidationFunction cf,
    time_t start_time,
    time_t end_time,
    StatisticsTimeSeriesData* result);

std::string ConsumeStatisticsTimeSeriesError();

}  // namespace directordaemon

#endif  // BAREOS_DIRD_STATISTICS_TIME_SERIES_H_
