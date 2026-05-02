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

#include "dird/director_jcr_impl.h"
#include "dird/job_status_history.h"

#include <algorithm>
#include <deque>
#include <mutex>
#include <unordered_map>

namespace directordaemon {

namespace {

constexpr size_t kMaxTransitionsPerJob = 256;
constexpr size_t kMaxTrackedJobs = 4096;

struct JobStatusTransitionState {
  std::mutex mutex{};
  std::deque<JobId_t> job_order{};
  std::unordered_map<JobId_t, std::deque<JobStatusTransitionEvent>>
      transitions_by_job{};
};

JobStatusTransitionState& GetJobStatusTransitionState()
{
  static JobStatusTransitionState state;
  return state;
}

JobStatusTransitionEvent BuildTransitionEvent(JobControlRecord* jcr,
                                              JobStatusTransitionSource source,
                                              int32_t previous_status,
                                              int32_t new_status)
{
  JobStatusTransitionEvent event;
  event.timestamp = time(nullptr);
  event.source = source;
  event.previous_status = previous_status;
  event.new_status = new_status;

  if (jcr == nullptr || jcr->dir_impl == nullptr) { return event; }

  event.director_status = jcr->getJobStatus();
  event.storage_daemon_status = jcr->dir_impl->SDJobStatus.load();
  event.file_daemon_status = jcr->dir_impl->FDJobStatus.load();

  if (jcr->dir_impl->sd_runtime_snapshot.valid) {
    event.job_files = jcr->dir_impl->sd_runtime_snapshot.job_files;
    event.job_bytes = jcr->dir_impl->sd_runtime_snapshot.job_bytes;
    event.current_file = jcr->dir_impl->sd_runtime_snapshot.current_file;
  } else {
    event.job_files = jcr->dir_impl->SDJobFiles;
    event.job_bytes = jcr->dir_impl->SDJobBytes;
  }

  return event;
}

void RecordJobStatusTransition(JobControlRecord* jcr,
                               JobStatusTransitionSource source,
                               int32_t previous_status,
                               int32_t new_status)
{
  if (jcr == nullptr || jcr->JobId == 0 || previous_status == new_status) {
    return;
  }

  StoreJobStatusTransition(jcr->JobId,
                           BuildTransitionEvent(jcr, source, previous_status,
                                                new_status));
}

}  // namespace

const char* JobStatusTransitionSourceToString(JobStatusTransitionSource source)
{
  switch (source) {
    case JobStatusTransitionSource::kDirector:
      return "director";
    case JobStatusTransitionSource::kStorageDaemon:
      return "storage-daemon";
    case JobStatusTransitionSource::kFileDaemon:
      return "file-daemon";
  }

  return "director";
}

void EnsureJobStatusHistoryHookRegistered()
{
  static std::once_flag hook_registered;
  std::call_once(hook_registered, []() {
    SetJobStatusChangeHook(&RecordDirectorJobStatusTransition);
  });
}

void RecordDirectorJobStatusTransition(JobControlRecord* jcr,
                                       int32_t previous_status,
                                       int32_t new_status)
{
  RecordJobStatusTransition(jcr, JobStatusTransitionSource::kDirector,
                            previous_status, new_status);
}

void RecordStorageDaemonJobStatusTransition(JobControlRecord* jcr,
                                            int32_t previous_status,
                                            int32_t new_status)
{
  RecordJobStatusTransition(jcr, JobStatusTransitionSource::kStorageDaemon,
                            previous_status, new_status);
}

void RecordFileDaemonJobStatusTransition(JobControlRecord* jcr,
                                         int32_t previous_status,
                                         int32_t new_status)
{
  RecordJobStatusTransition(jcr, JobStatusTransitionSource::kFileDaemon,
                            previous_status, new_status);
}

void StoreJobStatusTransition(JobId_t job_id,
                              const JobStatusTransitionEvent& event)
{
  if (job_id == 0) { return; }

  auto& state = GetJobStatusTransitionState();
  std::lock_guard<std::mutex> lock(state.mutex);

  auto it = state.transitions_by_job.find(job_id);
  if (it == state.transitions_by_job.end()) {
    while (state.job_order.size() >= kMaxTrackedJobs) {
      const auto oldest_job_id = state.job_order.front();
      state.job_order.pop_front();
      state.transitions_by_job.erase(oldest_job_id);
    }

    state.job_order.push_back(job_id);
    it = state.transitions_by_job
             .emplace(job_id, std::deque<JobStatusTransitionEvent>{})
             .first;
  }

  it->second.push_back(event);
  while (it->second.size() > kMaxTransitionsPerJob) { it->second.pop_front(); }
}

bool GetJobStatusTransitionHistory(JobId_t job_id,
                                   time_t start_time,
                                   time_t end_time,
                                   JobStatusTransitionHistory* result)
{
  if (result == nullptr) { return false; }

  *result = {};
  if (job_id == 0) { return true; }

  auto& state = GetJobStatusTransitionState();
  std::lock_guard<std::mutex> lock(state.mutex);

  const auto it = state.transitions_by_job.find(job_id);
  if (it == state.transitions_by_job.end()) { return true; }

  for (const auto& event : it->second) {
    if (start_time > 0 && event.timestamp < start_time) { continue; }
    if (end_time > 0 && event.timestamp > end_time) { continue; }
    result->events.push_back(event);
  }

  result->available = !result->events.empty();
  return true;
}

void ClearJobStatusTransitionHistory()
{
  auto& state = GetJobStatusTransitionState();
  std::lock_guard<std::mutex> lock(state.mutex);
  state.job_order.clear();
  state.transitions_by_job.clear();
}

}  // namespace directordaemon
