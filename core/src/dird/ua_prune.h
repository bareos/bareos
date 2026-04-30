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

namespace directordaemon {

struct del_ctx;
struct JobContentHistoryRecord {
  JobId_t JobId = 0;
  DBId_t BaseId = 0;
  DBId_t ContentId = 0;
};
struct JobKeepCountRecord {
  JobId_t JobId = 0;
  std::string Name;
  DBId_t ClientId = 0;
  DBId_t FileSetId = 0;
  utime_t JobTDate = 0;
  int32_t KeepNumber = 0;
};

bool PruneFiles(UaContext* ua, ClientResource* client, PoolResource* pool);
bool PruneJobs(UaContext* ua, ClientResource* client, PoolResource* pool);
bool PruneVolume(UaContext* ua, MediaDbRecord* mr);
int JobDeleteHandler(void* ctx, int num_fields, char** row);
int DelCountHandler(void* ctx, int num_fields, char** row);
int FileDeleteHandler(void* ctx, int num_fields, char** row);
int GetPruneListForVolume(UaContext* ua,
                          MediaDbRecord* mr,
                          std::vector<JobId_t>& prune_list);
int ExcludeRunningJobsFromList(std::vector<JobId_t>& prune_list);
int ExcludeDependentJobsFromList(
    std::vector<JobId_t>& prune_list,
    const std::vector<JobContentHistoryRecord>& jobs);
int ExcludeJobsByKeepCount(std::vector<JobId_t>& prune_list,
                           const std::vector<JobKeepCountRecord>& jobs);

} /* namespace directordaemon */
#endif  // BAREOS_DIRD_UA_PRUNE_H_
