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

#include "include/bareos.h"
#include "dird.h"

#include "dird/statistics_time_series.h"
#include "lib/bsys.h"

#include <array>
#include <cmath>
#include <limits>
#include <mutex>
#include <sys/stat.h>
#include <unordered_set>
#include <vector>

#if HAVE_LIBRRD
#  include <rrd.h>
#endif

namespace directordaemon {

#if HAVE_LIBRRD
namespace {

bool FileExists(const std::string& path)
{
  struct stat st;
  return stat(path.c_str(), &st) == 0;
}

bool EnsureDirectory(JobControlRecord* jcr, const std::string& path)
{
  if (PathCreate(path.c_str())) { return true; }

  Jmsg(jcr, M_WARNING, 0, T_("Could not create statistics directory \"%s\".\n"),
       path.c_str());
  return false;
}

void AppendCreateArgument(std::vector<std::string>& args,
                          const std::string& arg)
{
  args.emplace_back(arg);
}

std::string MakeGaugeDefinition(const char* name, uint32_t heartbeat_seconds)
{
  return std::string{"DS:"} + name
         + ":GAUGE:" + std::to_string(heartbeat_seconds) + ":0:U";
}

std::string MakeArchiveDefinition(const char* cf,
                                  uint32_t pdp_steps,
                                  uint32_t rows)
{
  return std::string{"RRA:"} + cf + ":0.5:" + std::to_string(pdp_steps) + ":"
         + std::to_string(rows);
}

std::vector<const char*> MakeArgv(const std::vector<std::string>& args)
{
  std::vector<const char*> argv;
  argv.reserve(args.size());
  for (const auto& arg : args) { argv.push_back(arg.c_str()); }
  return argv;
}

const char* BoolToRrdValue(bool value) { return value ? "1" : "0"; }

bool FetchRrdStatisticsTimeSeries(const std::string& path,
                                  StatisticsTimeSeriesConsolidationFunction cf,
                                  time_t start_time,
                                  time_t end_time,
                                  StatisticsTimeSeriesData* result)
{
  *result = {};

  if (!FileExists(path)) { return true; }
  if (end_time <= start_time) { return true; }

  time_t fetch_start = start_time;
  time_t fetch_end
      = end_time < std::numeric_limits<time_t>::max() ? end_time + 1 : end_time;
  unsigned long fetch_step = 0;
  unsigned long data_source_count = 0;
  char** data_source_names = nullptr;
  rrd_value_t* values = nullptr;

  if (rrd_fetch_r(path.c_str(),
                  StatisticsTimeSeriesConsolidationFunctionToString(cf),
                  &fetch_start, &fetch_end, &fetch_step, &data_source_count,
                  &data_source_names, &values)
      != 0) {
    return false;
  }

  result->available = true;
  result->start_time = fetch_start;
  result->end_time = fetch_end;
  result->step_seconds = fetch_step;
  result->data_source_names.reserve(data_source_count);
  for (unsigned long index = 0; index < data_source_count; ++index) {
    result->data_source_names.emplace_back(data_source_names[index]);
    rrd_freemem(data_source_names[index]);
  }
  rrd_freemem(data_source_names);

  const unsigned long row_count
      = fetch_step == 0 ? 0 : (fetch_end - fetch_start) / fetch_step;
  result->points.reserve(row_count);
  for (unsigned long row = 0; row < row_count; ++row) {
    StatisticsTimeSeriesPoint point;
    point.sample_time
        = fetch_start + static_cast<time_t>((row + 1) * fetch_step);
    point.values.reserve(data_source_count);
    bool has_value = false;
    for (unsigned long column = 0; column < data_source_count; ++column) {
      const auto value = values[row * data_source_count + column];
      has_value = has_value || !std::isnan(value);
      point.values.push_back(value);
    }
    if (has_value) { result->points.emplace_back(std::move(point)); }
  }

  rrd_freemem(values);
  return true;
}

class RrdStatisticsTimeSeriesBackend final
    : public StatisticsTimeSeriesBackend {
 public:
  RrdStatisticsTimeSeriesBackend(std::string base_directory,
                                 uint32_t step_seconds)
      : base_directory_{std::move(base_directory)}
      , step_seconds_{std::max(step_seconds, 1u)}
      , archives_{BuildDefaultStatisticsTimeSeriesArchives(step_seconds_)}
  {
    static std::once_flag init_flag;
    std::call_once(init_flag, []() { rrd_thread_init(); });
  }

  void StoreDeviceStatistics(
      JobControlRecord*,
      const DeviceRuntimeStatisticsSample& sample) override
  {
    device_samples_.push_back(sample);
  }

  void StoreJobStatistics(JobControlRecord*,
                          const JobRuntimeStatisticsSample& sample) override
  {
    job_samples_.push_back(sample);
  }

  void StorePoolStatistics(
      JobControlRecord*,
      const PoolStatisticsTimeSeriesSample& sample) override
  {
    pool_samples_.push_back(sample);
  }

  void StoreSystemStatistics(
      JobControlRecord*,
      const SystemStatisticsTimeSeriesSample& sample) override
  {
    system_samples_.push_back(sample);
  }

  void Flush(JobControlRecord* jcr) override
  {
    const std::string devices_directory = base_directory_ + "/devices";
    const std::string jobs_directory = base_directory_ + "/jobs";
    const std::string pools_directory = base_directory_ + "/pools";
    const std::string system_directory = base_directory_ + "/system";

    if (!EnsureDirectory(jcr, base_directory_)
        || !EnsureDirectory(jcr, devices_directory)
        || !EnsureDirectory(jcr, jobs_directory)
        || !EnsureDirectory(jcr, pools_directory)
        || !EnsureDirectory(jcr, system_directory)) {
      device_samples_.clear();
      job_samples_.clear();
      pool_samples_.clear();
      system_samples_.clear();
      return;
    }

    for (const auto& sample : device_samples_) {
      FlushDeviceSample(jcr, sample);
    }
    device_samples_.clear();

    for (const auto& sample : job_samples_) { FlushJobSample(jcr, sample); }
    job_samples_.clear();

    for (const auto& sample : pool_samples_) { FlushPoolSample(jcr, sample); }
    pool_samples_.clear();

    for (const auto& sample : system_samples_) {
      FlushSystemSample(jcr, sample);
    }
    system_samples_.clear();
  }

 private:
  void FlushDeviceSample(JobControlRecord* jcr,
                         const DeviceRuntimeStatisticsSample& sample)
  {
    const std::string path = GetDeviceStatisticsTimeSeriesPath(
        base_directory_, sample.storage_name, sample.device_name);
    if (!EnsureDeviceRrd(jcr, path, sample.sample_time)) { return; }

    const std::string template_string
        = "running_jobs:job_files:job_bytes:average_rate:last_rate:spooling:"
          "despooling:despool_wait";
    const std::string sample_string
        = std::to_string(sample.sample_time) + ":"
          + std::to_string(sample.running_jobs) + ":"
          + std::to_string(sample.job_files) + ":"
          + std::to_string(sample.job_bytes) + ":"
          + std::to_string(sample.average_rate) + ":"
          + std::to_string(sample.last_rate) + ":"
          + BoolToRrdValue(sample.spooling) + ":"
          + BoolToRrdValue(sample.despooling) + ":"
          + BoolToRrdValue(sample.despool_wait);

    const char* argv[] = {sample_string.c_str()};
    if (rrd_update_r(path.c_str(), template_string.c_str(), 1, argv) != 0) {
      ReportRrdFailureOnce(jcr, path, "update");
    }
  }

  void FlushJobSample(JobControlRecord* jcr,
                      const JobRuntimeStatisticsSample& sample)
  {
    const std::string path
        = GetJobStatisticsTimeSeriesPath(base_directory_, sample.job_id);
    if (!EnsureJobRrd(jcr, path, sample.sample_time)) { return; }

    const std::string template_string
        = "job_files:job_bytes:average_rate:last_rate:job_status:spooling:"
          "despooling:despool_wait";
    const std::string sample_string = std::to_string(sample.sample_time) + ":"
                                      + std::to_string(sample.job_files) + ":"
                                      + std::to_string(sample.job_bytes) + ":"
                                      + std::to_string(sample.average_rate)
                                      + ":" + std::to_string(sample.last_rate)
                                      + ":" + std::to_string(sample.job_status)
                                      + ":" + BoolToRrdValue(sample.spooling)
                                      + ":" + BoolToRrdValue(sample.despooling)
                                      + ":"
                                      + BoolToRrdValue(sample.despool_wait);

    const char* argv[] = {sample_string.c_str()};
    if (rrd_update_r(path.c_str(), template_string.c_str(), 1, argv) != 0) {
      ReportRrdFailureOnce(jcr, path, "update");
    }
  }

  void FlushPoolSample(JobControlRecord* jcr,
                       const PoolStatisticsTimeSeriesSample& sample)
  {
    const std::string path
        = GetPoolStatisticsTimeSeriesPath(base_directory_, sample.pool_name);
    if (!EnsurePoolRrd(jcr, path, sample.sample_time)) { return; }

    const std::string template_string
        = "num_volumes:total_bytes:total_files:max_vol_bytes:max_volumes";
    const std::string sample_string = std::to_string(sample.sample_time) + ":"
                                      + std::to_string(sample.num_volumes) + ":"
                                      + std::to_string(sample.total_bytes) + ":"
                                      + std::to_string(sample.total_files) + ":"
                                      + std::to_string(sample.max_vol_bytes)
                                      + ":"
                                      + std::to_string(sample.max_volumes);

    const char* argv[] = {sample_string.c_str()};
    if (rrd_update_r(path.c_str(), template_string.c_str(), 1, argv) != 0) {
      ReportRrdFailureOnce(jcr, path, "update");
    }
  }

  void FlushSystemSample(JobControlRecord* jcr,
                         const SystemStatisticsTimeSeriesSample& sample)
  {
    const std::string path = GetSystemStatisticsTimeSeriesPath(base_directory_);
    if (!EnsureSystemRrd(jcr, path, sample.sample_time)) { return; }

    const std::string template_string
        = "num_pools:num_volumes:total_bytes:total_files";
    const std::string sample_string = std::to_string(sample.sample_time) + ":"
                                      + std::to_string(sample.num_pools) + ":"
                                      + std::to_string(sample.num_volumes) + ":"
                                      + std::to_string(sample.total_bytes) + ":"
                                      + std::to_string(sample.total_files);

    const char* argv[] = {sample_string.c_str()};
    if (rrd_update_r(path.c_str(), template_string.c_str(), 1, argv) != 0) {
      ReportRrdFailureOnce(jcr, path, "update");
    }
  }

  bool EnsureDeviceRrd(JobControlRecord* jcr,
                       const std::string& path,
                       time_t sample_time)
  {
    static constexpr std::array<const char*, 8> ds_names
        = {"running_jobs", "job_files", "job_bytes",  "average_rate",
           "last_rate",    "spooling",  "despooling", "despool_wait"};
    return EnsureRrd(jcr, path, sample_time, ds_names);
  }

  bool EnsureJobRrd(JobControlRecord* jcr,
                    const std::string& path,
                    time_t sample_time)
  {
    static constexpr std::array<const char*, 8> ds_names
        = {"job_files",  "job_bytes", "average_rate", "last_rate",
           "job_status", "spooling",  "despooling",   "despool_wait"};
    return EnsureRrd(jcr, path, sample_time, ds_names);
  }

  bool EnsurePoolRrd(JobControlRecord* jcr,
                     const std::string& path,
                     time_t sample_time)
  {
    static constexpr std::array<const char*, 5> ds_names
        = {"num_volumes", "total_bytes", "total_files", "max_vol_bytes",
           "max_volumes"};
    return EnsureRrd(jcr, path, sample_time, ds_names);
  }

  bool EnsureSystemRrd(JobControlRecord* jcr,
                       const std::string& path,
                       time_t sample_time)
  {
    static constexpr std::array<const char*, 4> ds_names
        = {"num_pools", "num_volumes", "total_bytes", "total_files"};
    return EnsureRrd(jcr, path, sample_time, ds_names);
  }

  template <size_t N>
  bool EnsureRrd(JobControlRecord* jcr,
                 const std::string& path,
                 time_t sample_time,
                 const std::array<const char*, N>& ds_names)
  {
    if (FileExists(path)) { return true; }

    std::vector<std::string> create_args;
    create_args.reserve(N + archives_.size() * 4u);

    const uint32_t heartbeat_seconds = std::max(step_seconds_ * 2u, 2u);
    for (const auto* name : ds_names) {
      AppendCreateArgument(create_args,
                           MakeGaugeDefinition(name, heartbeat_seconds));
    }

    for (const auto& archive : archives_) {
      const uint32_t pdp_steps
          = std::max(1u, archive.resolution_seconds / step_seconds_);
      AppendCreateArgument(
          create_args,
          MakeArchiveDefinition("AVERAGE", pdp_steps, archive.rows));
      AppendCreateArgument(
          create_args, MakeArchiveDefinition("MIN", pdp_steps, archive.rows));
      AppendCreateArgument(
          create_args, MakeArchiveDefinition("MAX", pdp_steps, archive.rows));
      AppendCreateArgument(
          create_args, MakeArchiveDefinition("LAST", pdp_steps, archive.rows));
    }

    auto argv = MakeArgv(create_args);
    if (rrd_create_r(path.c_str(), step_seconds_,
                     sample_time > 0 ? sample_time - 1 : 0,
                     static_cast<int>(argv.size()), argv.data())
        != 0) {
      ReportRrdFailureOnce(jcr, path, "create");
      return false;
    }

    return true;
  }

  void ReportRrdFailureOnce(JobControlRecord* jcr,
                            const std::string& path,
                            const char* operation)
  {
    const std::string key = std::string{operation} + ":" + path;
    if (!reported_failures_.insert(key).second) {
      rrd_clear_error();
      return;
    }

    Jmsg(jcr, M_WARNING, 0, T_("RRD %s failed for \"%s\": %s\n"), operation,
         path.c_str(), rrd_get_error());
    rrd_clear_error();
  }

  std::string base_directory_;
  uint32_t step_seconds_;
  std::vector<StatisticsTimeSeriesArchive> archives_;
  std::vector<DeviceRuntimeStatisticsSample> device_samples_;
  std::vector<JobRuntimeStatisticsSample> job_samples_;
  std::vector<PoolStatisticsTimeSeriesSample> pool_samples_;
  std::vector<SystemStatisticsTimeSeriesSample> system_samples_;
  std::unordered_set<std::string> reported_failures_;
};

}  // namespace
#endif

std::unique_ptr<StatisticsTimeSeriesBackend>
CreateRrdStatisticsTimeSeriesBackend(const std::string& base_directory,
                                     uint32_t step_seconds)
{
#if HAVE_LIBRRD
  return std::make_unique<RrdStatisticsTimeSeriesBackend>(base_directory,
                                                          step_seconds);
#else
  static_cast<void>(base_directory);
  static_cast<void>(step_seconds);
  return nullptr;
#endif
}

bool ReadJobStatisticsTimeSeries(const std::string& base_directory,
                                 uint32_t job_id,
                                 StatisticsTimeSeriesConsolidationFunction cf,
                                 time_t start_time,
                                 time_t end_time,
                                 StatisticsTimeSeriesData* result)
{
#if HAVE_LIBRRD
  return FetchRrdStatisticsTimeSeries(
      GetJobStatisticsTimeSeriesPath(base_directory, job_id), cf, start_time,
      end_time, result);
#else
  static_cast<void>(base_directory);
  static_cast<void>(job_id);
  static_cast<void>(cf);
  static_cast<void>(start_time);
  static_cast<void>(end_time);
  *result = {};
  return false;
#endif
}

bool ReadDeviceStatisticsTimeSeries(
    const std::string& base_directory,
    const std::string& storage_name,
    const std::string& device_name,
    StatisticsTimeSeriesConsolidationFunction cf,
    time_t start_time,
    time_t end_time,
    StatisticsTimeSeriesData* result)
{
#if HAVE_LIBRRD
  return FetchRrdStatisticsTimeSeries(
      GetDeviceStatisticsTimeSeriesPath(base_directory, storage_name,
                                        device_name),
      cf, start_time, end_time, result);
#else
  static_cast<void>(base_directory);
  static_cast<void>(storage_name);
  static_cast<void>(device_name);
  static_cast<void>(cf);
  static_cast<void>(start_time);
  static_cast<void>(end_time);
  *result = {};
  return false;
#endif
}

bool ReadPoolStatisticsTimeSeries(const std::string& base_directory,
                                  const std::string& pool_name,
                                  StatisticsTimeSeriesConsolidationFunction cf,
                                  time_t start_time,
                                  time_t end_time,
                                  StatisticsTimeSeriesData* result)
{
#if HAVE_LIBRRD
  return FetchRrdStatisticsTimeSeries(
      GetPoolStatisticsTimeSeriesPath(base_directory, pool_name), cf,
      start_time, end_time, result);
#else
  static_cast<void>(base_directory);
  static_cast<void>(pool_name);
  static_cast<void>(cf);
  static_cast<void>(start_time);
  static_cast<void>(end_time);
  *result = {};
  return false;
#endif
}

bool ReadSystemStatisticsTimeSeries(
    const std::string& base_directory,
    StatisticsTimeSeriesConsolidationFunction cf,
    time_t start_time,
    time_t end_time,
    StatisticsTimeSeriesData* result)
{
#if HAVE_LIBRRD
  return FetchRrdStatisticsTimeSeries(
      GetSystemStatisticsTimeSeriesPath(base_directory), cf, start_time,
      end_time, result);
#else
  static_cast<void>(base_directory);
  static_cast<void>(cf);
  static_cast<void>(start_time);
  static_cast<void>(end_time);
  *result = {};
  return false;
#endif
}

std::string ConsumeStatisticsTimeSeriesError()
{
#if HAVE_LIBRRD
  const char* error = rrd_get_error();
  std::string result = error ? error : "";
  rrd_clear_error();
  return result;
#else
  return "";
#endif
}

}  // namespace directordaemon
