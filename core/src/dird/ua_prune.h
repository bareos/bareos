/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2018-2026 Bareos GmbH & Co. KG

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

#ifndef BAREOS_DIRD_UA_PRUNE_H_
#define BAREOS_DIRD_UA_PRUNE_H_

#include "dird/ua.h"
#include "cats/cats.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace directordaemon {

struct del_ctx;
struct PoolPruneSummary {
  uint32_t prunable_volumes = 0;
  uint32_t prunable_jobs = 0;
  uint64_t prunable_bytes = 0;
};
struct PoolPruneStatusDetail {
  std::string status;
  uint32_t volumes = 0;
};
struct PoolPruneJobDetail {
  JobId_t jobid = 0;
  std::string name;
  uint64_t bytes = 0;
  std::string start_time;
};
struct PoolPruneVolumeDetail {
  std::string volume_name;
  std::string volume_status;
  std::string last_written;
  std::string reason;
  std::vector<JobId_t> job_ids;
  uint32_t prunable_jobs = 0;
  uint64_t prunable_bytes = 0;
};
struct PoolPruneReport {
  std::string reason;
  PoolPruneSummary summary;
  std::vector<PoolPruneStatusDetail> status_breakdown;
  std::vector<PoolPruneVolumeDetail> volumes;
  std::vector<PoolPruneJobDetail> jobs;
};

bool PruneFiles(UaContext* ua, ClientResource* client, PoolResource* pool);
bool PruneJobs(UaContext* ua, ClientResource* client, PoolResource* pool);
bool PruneVolume(UaContext* ua, MediaDbRecord* mr);
bool GetPoolPruneReport(UaContext* ua,
                        PoolDbRecord* pool,
                        PoolPruneReport* report);
bool GetPoolPruneSummary(UaContext* ua,
                         PoolDbRecord* pool,
                         PoolPruneSummary* summary);
PoolPruneReport BuildPoolPruneReport(
    const std::vector<PoolPruneVolumeDetail>& volumes,
    const std::vector<PoolPruneJobDetail>& jobs);
PoolPruneSummary SummarizePoolPruneJobs(
    const std::unordered_map<JobId_t, uint64_t>& prunable_jobs,
    uint32_t prunable_volumes);
int JobDeleteHandler(void* ctx, int num_fields, char** row);
int DelCountHandler(void* ctx, int num_fields, char** row);
int FileDeleteHandler(void* ctx, int num_fields, char** row);
int GetPruneListForVolume(UaContext* ua,
                          MediaDbRecord* mr,
                          std::vector<JobId_t>& prune_list);
int ExcludeRunningJobsFromList(std::vector<JobId_t>& prune_list);

} /* namespace directordaemon */
#endif  // BAREOS_DIRD_UA_PRUNE_H_
