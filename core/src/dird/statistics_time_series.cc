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

#include <mutex>
#include <cstdlib>
#include <string>
#include <unordered_map>

#include "dird/dird_globals.h"
#include "dird/director_jcr_impl.h"
#include "dird/statistics_time_series.h"

namespace directordaemon {

namespace {

struct StatisticsTimeSeriesState {
  std::mutex mutex{};
  std::unique_ptr<StatisticsTimeSeriesBackend> backend{};
  bool backend_initialized = false;
  time_t last_catalog_sample_time = 0;
  std::unordered_map<std::string, DeviceRuntimeStatisticsSample>
      pending_device_samples{};
};

StatisticsTimeSeriesState& GetStatisticsTimeSeriesState()
{
  static StatisticsTimeSeriesState state;
  return state;
}

bool IsStatisticsTimeSeriesEnabled()
{
  return me != nullptr && me->statistics_time_series;
}

std::string GetConfiguredBaseDirectory()
{
  if (me == nullptr) {
    return GetDefaultStatisticsTimeSeriesDirectory(nullptr);
  }

  const char* configured_directory = me->statistics_time_series_directory;
  return (configured_directory && configured_directory[0])
             ? configured_directory
             : GetDefaultStatisticsTimeSeriesDirectory(me->working_directory);
}

const char* GetStorageNameForRuntimeSnapshot(const JobControlRecord* jcr)
{
  if (jcr->dir_impl->res.write_storage) {
    return jcr->dir_impl->res.write_storage->resource_name_;
  }
  if (jcr->dir_impl->res.paired_read_write_storage) {
    return jcr->dir_impl->res.paired_read_write_storage->resource_name_;
  }
  if (jcr->dir_impl->res.read_storage) {
    return jcr->dir_impl->res.read_storage->resource_name_;
  }
  return "";
}

bool EnsureBackend(StatisticsTimeSeriesState& state, JobControlRecord* jcr)
{
  if (state.backend_initialized) { return state.backend != nullptr; }

  state.backend_initialized = true;
  state.backend = CreateStatisticsTimeSeriesBackend(jcr);
  return state.backend != nullptr;
}

uint32_t ParseUnsigned32(const char* value)
{
  return value ? static_cast<uint32_t>(std::strtoull(value, nullptr, 10)) : 0;
}

uint64_t ParseUnsigned64(const char* value)
{
  return value ? std::strtoull(value, nullptr, 10) : 0;
}

struct PoolQueryContext {
  StatisticsTimeSeriesBackend* backend;
  JobControlRecord* jcr;
  time_t sample_time;
};

int PoolStatisticsHandler(void* ctx, int nb_col, char** row)
{
  if (nb_col != 6 || !row[0]) { return 0; }

  auto* query_context = static_cast<PoolQueryContext*>(ctx);
  PoolStatisticsTimeSeriesSample sample;
  sample.sample_time = query_context->sample_time;
  sample.pool_name = row[0];
  sample.num_volumes = ParseUnsigned32(row[1]);
  sample.total_bytes = ParseUnsigned64(row[2]);
  sample.total_files = ParseUnsigned64(row[3]);
  sample.max_vol_bytes = ParseUnsigned64(row[4]);
  sample.max_volumes = ParseUnsigned32(row[5]);

  query_context->backend->StorePoolStatistics(query_context->jcr, sample);
  return 0;
}

struct SystemQueryContext {
  StatisticsTimeSeriesBackend* backend;
  JobControlRecord* jcr;
  time_t sample_time;
};

int SystemStatisticsHandler(void* ctx, int nb_col, char** row)
{
  if (nb_col != 4) { return 0; }

  auto* query_context = static_cast<SystemQueryContext*>(ctx);
  SystemStatisticsTimeSeriesSample sample;
  sample.sample_time = query_context->sample_time;
  sample.num_pools = ParseUnsigned32(row[0]);
  sample.num_volumes = ParseUnsigned32(row[1]);
  sample.total_bytes = ParseUnsigned64(row[2]);
  sample.total_files = ParseUnsigned64(row[3]);

  query_context->backend->StoreSystemStatistics(query_context->jcr, sample);
  return 0;
}

void SampleCatalogStatistics(StatisticsTimeSeriesState& state,
                             JobControlRecord* jcr,
                             time_t sample_time)
{
  if (!jcr->db) { return; }

  static constexpr char kPoolQuery[]
      = "SELECT Pool.Name,"
        " COUNT(Media.MediaId),"
        " COALESCE(SUM(Media.VolBytes), 0),"
        " COALESCE(SUM(Media.VolFiles), 0),"
        " Pool.MaxVolBytes,"
        " Pool.MaxVols"
        " FROM Pool"
        " LEFT JOIN Media ON Media.PoolId = Pool.PoolId"
        " GROUP BY Pool.PoolId, Pool.Name, Pool.MaxVolBytes, Pool.MaxVols"
        " ORDER BY Pool.PoolId";
  static constexpr char kSystemQuery[]
      = "SELECT"
        " (SELECT COUNT(*) FROM Pool),"
        " (SELECT COUNT(*) FROM Media),"
        " COALESCE((SELECT SUM(VolBytes) FROM Media), 0),"
        " COALESCE((SELECT SUM(VolFiles) FROM Media), 0)";

  PoolQueryContext pool_context{state.backend.get(), jcr, sample_time};
  if (!jcr->db->SqlQuery(kPoolQuery, PoolStatisticsHandler, &pool_context)) {
    Jmsg(jcr, M_WARNING, 0,
         T_("Could not sample pool statistics for time-series storage: %s\n"),
         jcr->db->strerror());
  }

  SystemQueryContext system_context{state.backend.get(), jcr, sample_time};
  if (!jcr->db->SqlQuery(kSystemQuery, SystemStatisticsHandler,
                         &system_context)) {
    Jmsg(jcr, M_WARNING, 0,
         T_("Could not sample system statistics for time-series storage: %s\n"),
         jcr->db->strerror());
  }
}

void AccumulateRuntimeDeviceSample(StatisticsTimeSeriesState& state,
                                   JobControlRecord* jcr,
                                   const std::string& storage_name,
                                   const std::string& device_name,
                                   const ::StorageRuntimeSnapshot& snapshot)
{
  if (device_name.empty()) { return; }

  const std::string key = storage_name + '\x1f' + device_name;
  auto& pending = state.pending_device_samples[key];
  if (pending.sample_time != 0 && pending.sample_time != snapshot.sample_time) {
    state.backend->StoreDeviceStatistics(jcr, pending);
    pending = DeviceRuntimeStatisticsSample{};
  }

  pending.sample_time = snapshot.sample_time;
  pending.storage_name = storage_name;
  pending.device_name = device_name;
  pending.running_jobs += 1;
  pending.job_files += snapshot.job_files;
  pending.job_bytes += snapshot.job_bytes;
  pending.average_rate += snapshot.average_rate;
  pending.last_rate += snapshot.last_rate;
  pending.spooling = pending.spooling || snapshot.spooling;
  pending.despooling = pending.despooling || snapshot.despooling;
  pending.despool_wait = pending.despool_wait || snapshot.despool_wait;
}

}  // namespace

std::unique_ptr<StatisticsTimeSeriesBackend> CreateStatisticsTimeSeriesBackend(
    JobControlRecord* /*jcr*/)
{
  if (!IsStatisticsTimeSeriesEnabled()) { return nullptr; }

  return CreateRrdStatisticsTimeSeriesBackend(
      GetConfiguredBaseDirectory(),
      GetStatisticsTimeSeriesStepSeconds(me->stats_collect_interval));
}

void StoreStorageRuntimeSnapshot(JobControlRecord* jcr,
                                 const ::StorageRuntimeSnapshot& snapshot)
{
  if (!IsStatisticsTimeSeriesEnabled() || !snapshot.valid) { return; }

  auto& state = GetStatisticsTimeSeriesState();
  std::lock_guard<std::mutex> lock(state.mutex);
  if (!EnsureBackend(state, jcr)) { return; }

  JobRuntimeStatisticsSample job_sample;
  job_sample.sample_time = snapshot.sample_time;
  job_sample.job_id = jcr->JobId;
  job_sample.job_files = snapshot.job_files;
  job_sample.job_bytes = snapshot.job_bytes;
  job_sample.average_rate = snapshot.average_rate;
  job_sample.last_rate = snapshot.last_rate;
  job_sample.job_status = snapshot.job_status;
  job_sample.spooling = snapshot.spooling;
  job_sample.despooling = snapshot.despooling;
  job_sample.despool_wait = snapshot.despool_wait;
  state.backend->StoreJobStatistics(jcr, job_sample);

  const std::string storage_name = GetStorageNameForRuntimeSnapshot(jcr);
  AccumulateRuntimeDeviceSample(state, jcr, storage_name, snapshot.read_device,
                                snapshot);
  if (snapshot.write_device != snapshot.read_device) {
    AccumulateRuntimeDeviceSample(state, jcr, storage_name,
                                  snapshot.write_device, snapshot);
  }

  state.backend->Flush(jcr);
}

void SampleCatalogStatisticsTimeSeries(JobControlRecord* jcr)
{
  if (!IsStatisticsTimeSeriesEnabled() || !jcr || !jcr->db) { return; }

  auto& state = GetStatisticsTimeSeriesState();
  std::lock_guard<std::mutex> lock(state.mutex);
  if (!EnsureBackend(state, jcr)) { return; }

  const time_t sample_time = time(nullptr);
  if (sample_time == 0 || sample_time == state.last_catalog_sample_time) {
    return;
  }

  SampleCatalogStatistics(state, jcr, sample_time);
  state.last_catalog_sample_time = sample_time;
  state.backend->Flush(jcr);
}

void FlushStatisticsTimeSeries(JobControlRecord* jcr)
{
  auto& state = GetStatisticsTimeSeriesState();
  std::lock_guard<std::mutex> lock(state.mutex);
  if (!EnsureBackend(state, jcr)) { return; }

  for (auto& [key, pending] : state.pending_device_samples) {
    static_cast<void>(key);
    if (pending.sample_time == 0) { continue; }
    state.backend->StoreDeviceStatistics(jcr, pending);
  }
  state.pending_device_samples.clear();

  state.backend->Flush(jcr);
}

std::string GetConfiguredStatisticsTimeSeriesDirectory()
{
  return GetConfiguredBaseDirectory();
}

}  // namespace directordaemon
