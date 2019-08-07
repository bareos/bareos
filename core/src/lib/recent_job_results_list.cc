/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2019-2019 Bareos GmbH & Co. KG

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
#include "include/jcr.h"
#include "lib/recent_job_results_list.h"
#include "lib/dlist.h"
#include "lib/berrno.h"

#include <vector>
#include <mutex>

static std::vector<RecentJobResultsList::JobResult> recent_job_results_list;
static const int max_count_recent_job_results = 10;
static std::mutex mutex;

void RecentJobResultsList::Cleanup()
{
  if (!recent_job_results_list.empty()) {
    mutex.lock();
    recent_job_results_list.clear();
    mutex.unlock();
  }
}

bool RecentJobResultsList::ImportFromFile(int fd, uint64_t addr)
{
  uint32_t num;
  bool ok = true;

  Dmsg1(100, "read_last_jobs seek to %d\n", (int)addr);
  if (addr == 0 || lseek(fd, (boffset_t)addr, SEEK_SET) < 0) { return false; }
  if (read(fd, &num, sizeof(num)) != sizeof(num)) { return false; }
  Dmsg1(100, "Read num_items=%d\n", num);
  if (num > 4 * max_count_recent_job_results) { /* sanity check */
    return false;
  }
  mutex.lock();
  for (; num; num--) {
    RecentJobResultsList::JobResult job;
    if (read(fd, &job, sizeof(job)) != sizeof(job)) {
      BErrNo be;
      Pmsg1(000, "Read job entry. ERR=%s\n", be.bstrerror());
      ok = false;
      break;
    }
    if (job.JobId > 0) {
      RecentJobResultsList::JobResult je = job;
      recent_job_results_list.push_back(je);
      if (recent_job_results_list.size() > max_count_recent_job_results) {
        recent_job_results_list.erase(recent_job_results_list.begin());
      }
    }
  }
  mutex.unlock();
  return ok;
}

uint64_t RecentJobResultsList::ExportToFile(int fd, uint64_t addr)
{
  Dmsg1(100, "write_last_jobs seek to %d\n", (int)addr);
  if (lseek(fd, (boffset_t)addr, SEEK_SET) < 0) { return 0; }
  if (!recent_job_results_list.empty()) {
    std::lock_guard<std::mutex> m(mutex);
    uint32_t num =
        recent_job_results_list.size();  // always first entry in the file
    if (write(fd, &num, sizeof(num)) != sizeof(num)) {
      BErrNo be;
      Pmsg1(000, "Error writing num_items: ERR=%s\n", be.bstrerror());
      return 0;
    }
    for (auto je : recent_job_results_list) {
      if (write(fd, &je, sizeof(struct RecentJobResultsList::JobResult)) !=
          sizeof(struct RecentJobResultsList::JobResult)) {
        BErrNo be;
        Pmsg1(000, "Error writing job: ERR=%s\n", be.bstrerror());
        return 0;
      }
    }
  }

  ssize_t current_address = lseek(fd, 0, SEEK_CUR);
  if (current_address < 0) { current_address = 0; }
  return current_address;
}

void RecentJobResultsList::Append(JobControlRecord* jcr)
{
  RecentJobResultsList::JobResult je;
  je.Errors = jcr->JobErrors;
  je.JobType = jcr->getJobType();
  je.JobId = jcr->JobId;
  je.VolSessionId = jcr->VolSessionId;
  je.VolSessionTime = jcr->VolSessionTime;
  bstrncpy(je.Job, jcr->Job, sizeof(je.Job));
  je.JobFiles = jcr->JobFiles;
  je.JobBytes = jcr->JobBytes;
  je.JobStatus = jcr->JobStatus;
  je.JobLevel = jcr->getJobLevel();
  je.start_time = jcr->start_time;
  je.end_time = time(nullptr);

  mutex.lock();
  recent_job_results_list.push_back(je);
  if (recent_job_results_list.size() > max_count_recent_job_results) {
    recent_job_results_list.erase(recent_job_results_list.begin());
  }
  mutex.unlock();
}

std::vector<RecentJobResultsList::JobResult> RecentJobResultsList::Get()
{
  std::vector<RecentJobResultsList::JobResult> l;
  mutex.lock();
  l = recent_job_results_list;
  mutex.unlock();
  return l;
}

std::size_t RecentJobResultsList::Count()
{
  int count = 0;
  mutex.lock();
  count = recent_job_results_list.size();
  mutex.unlock();
  return count;
}

bool RecentJobResultsList::IsEmpty()
{
  bool empty;
  mutex.lock();
  empty = recent_job_results_list.empty();
  mutex.unlock();
  return empty;
}


RecentJobResultsList::JobResult RecentJobResultsList::GetMostRecentJobResult()
{
  RecentJobResultsList::JobResult j;
  mutex.lock();
  if (recent_job_results_list.size()) { j = recent_job_results_list.front(); }
  mutex.unlock();
  return j;
}
