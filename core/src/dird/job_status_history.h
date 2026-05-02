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
#ifndef BAREOS_DIRD_JOB_STATUS_HISTORY_H_
#define BAREOS_DIRD_JOB_STATUS_HISTORY_H_

#include <cstdint>
#include <ctime>
#include <string>
#include <vector>

#include "include/jcr.h"

namespace directordaemon {

enum class JobStatusTransitionSource
{
  kDirector,
  kStorageDaemon,
  kFileDaemon,
};

struct JobStatusTransitionEvent {
  time_t timestamp = 0;
  JobStatusTransitionSource source = JobStatusTransitionSource::kDirector;
  int32_t previous_status = 0;
  int32_t new_status = 0;
  int32_t director_status = 0;
  int32_t storage_daemon_status = 0;
  int32_t file_daemon_status = 0;
  uint32_t job_files = 0;
  uint64_t job_bytes = 0;
  std::string current_file{};
};

struct JobStatusTransitionHistory {
  bool available = false;
  std::vector<JobStatusTransitionEvent> events{};
};

const char* JobStatusTransitionSourceToString(JobStatusTransitionSource source);

void EnsureJobStatusHistoryHookRegistered();

void RecordDirectorJobStatusTransition(JobControlRecord* jcr,
                                       int32_t previous_status,
                                       int32_t new_status);
void RecordStorageDaemonJobStatusTransition(JobControlRecord* jcr,
                                            int32_t previous_status,
                                            int32_t new_status);
void RecordFileDaemonJobStatusTransition(JobControlRecord* jcr,
                                         int32_t previous_status,
                                         int32_t new_status);

void StoreJobStatusTransition(JobId_t job_id,
                              const JobStatusTransitionEvent& event);
bool GetJobStatusTransitionHistory(JobId_t job_id,
                                   time_t start_time,
                                   time_t end_time,
                                   JobStatusTransitionHistory* result);
void ClearJobStatusTransitionHistory();

}  // namespace directordaemon

#endif  // BAREOS_DIRD_JOB_STATUS_HISTORY_H_
