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

static std::vector<s_last_job*> last_jobs;
static const int max_last_jobs = 10;

static pthread_mutex_t last_jobs_mutex = PTHREAD_MUTEX_INITIALIZER;

void TermLastJobsList()
{
  if (last_jobs.empty()) { return; }

  LockLastJobsList();
  for (auto je : last_jobs) { free(je); };
  last_jobs.clear();
  UnlockLastJobsList();
}

void LockLastJobsList() { P(last_jobs_mutex); }

void UnlockLastJobsList() { V(last_jobs_mutex); }

bool ReadLastJobsList(int fd, uint64_t addr)
{
  struct s_last_job *je, job;
  uint32_t num;
  bool ok = true;

  Dmsg1(100, "read_last_jobs seek to %d\n", (int)addr);
  if (addr == 0 || lseek(fd, (boffset_t)addr, SEEK_SET) < 0) { return false; }
  if (read(fd, &num, sizeof(num)) != sizeof(num)) { return false; }
  Dmsg1(100, "Read num_items=%d\n", num);
  if (num > 4 * max_last_jobs) { /* sanity check */
    return false;
  }
  LockLastJobsList();
  for (; num; num--) {
    if (read(fd, &job, sizeof(job)) != sizeof(job)) {
      BErrNo be;
      Pmsg1(000, "Read job entry. ERR=%s\n", be.bstrerror());
      ok = false;
      break;
    }
    if (job.JobId > 0) {
      je = (struct s_last_job*)malloc(sizeof(struct s_last_job));
      memcpy((char*)je, (char*)&job, sizeof(job));
      last_jobs.push_back(je);
      if (last_jobs.size() > max_last_jobs) {
        free(last_jobs.front());
        last_jobs.erase(last_jobs.begin());
      }
    }
  }
  UnlockLastJobsList();
  return ok;
}

uint64_t WriteLastJobsList(int fd, uint64_t addr)
{
  uint32_t num;
  ssize_t status;

  Dmsg1(100, "write_last_jobs seek to %d\n", (int)addr);
  if (lseek(fd, (boffset_t)addr, SEEK_SET) < 0) { return 0; }
  if (!last_jobs.empty()) {
    LockLastJobsList();
    /*
     * First record is number of entires
     */
    num = last_jobs.size();
    if (write(fd, &num, sizeof(num)) != sizeof(num)) {
      BErrNo be;
      Pmsg1(000, "Error writing num_items: ERR=%s\n", be.bstrerror());
      goto bail_out;
    }
    for (auto je : last_jobs) {
      if (write(fd, je, sizeof(struct s_last_job)) !=
          sizeof(struct s_last_job)) {
        BErrNo be;
        Pmsg1(000, "Error writing job: ERR=%s\n", be.bstrerror());
        goto bail_out;
      }
    }
    UnlockLastJobsList();
  }

  /*
   * Return current address
   */
  status = lseek(fd, 0, SEEK_CUR);
  if (status < 0) { status = 0; }
  return status;

bail_out:
  UnlockLastJobsList();
  return 0;
}

void SetLastJobsStatistics(JobControlRecord* jcr)
{
  struct s_last_job* je = nullptr;

  switch (jcr->getJobType()) {
    case JT_BACKUP:
    case JT_VERIFY:
    case JT_RESTORE:
    case JT_MIGRATE:
    case JT_COPY:
    case JT_ADMIN:
      /*
       * Keep list of last jobs, but not Console where JobId==0
       */
      if (jcr->JobId > 0) {
        LockLastJobsList();
        num_jobs_run++;
        je = (struct s_last_job*)malloc(sizeof(struct s_last_job));
        new (je) s_last_job();
        je->Errors = jcr->JobErrors;
        je->JobType = jcr->getJobType();
        je->JobId = jcr->JobId;
        je->VolSessionId = jcr->VolSessionId;
        je->VolSessionTime = jcr->VolSessionTime;
        bstrncpy(je->Job, jcr->Job, sizeof(je->Job));
        je->JobFiles = jcr->JobFiles;
        je->JobBytes = jcr->JobBytes;
        je->JobStatus = jcr->JobStatus;
        je->JobLevel = jcr->getJobLevel();
        je->start_time = jcr->start_time;
        je->end_time = time(nullptr);

        last_jobs.push_back(je);
        if (last_jobs.size() > max_last_jobs) {
          free(last_jobs.front());
          last_jobs.erase(last_jobs.begin());
        }
        UnlockLastJobsList();
      }
      break;
    default:
      break;
  }
}

std::vector<s_last_job*> GetLastJobsList()
{
  std::vector<s_last_job*> l;
  LockLastJobsList();
  l = last_jobs;
  UnlockLastJobsList();
  return l;
}

std::size_t LastJobsCount()
{
  int count = 0;
  LockLastJobsList();
  count = last_jobs.size();
  UnlockLastJobsList();
  return count;
}

std::size_t LastJobsEmpty()
{
  bool empty;
  LockLastJobsList();
  empty = last_jobs.empty();
  UnlockLastJobsList();
  return empty;
}


s_last_job GetLastJob()
{
  s_last_job j;
  LockLastJobsList();
  if (last_jobs.size()) { j = *last_jobs.front(); }
  UnlockLastJobsList();
  return j;
}
