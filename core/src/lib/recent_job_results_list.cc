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
#include <fstream>

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

bool RecentJobResultsList::ImportFromFile(std::ifstream& file)
{
#if defined HAVE_IS_TRIVIALLY_COPYABLE
  static_assert(
      std::is_trivially_copyable<RecentJobResultsList::JobResult>::value,
      "RecentJobResultsList::JobResult must be trivially copyable");
#endif

  uint32_t num;

  try {
    file.read(reinterpret_cast<char*>(&num), sizeof(num));
  } catch (std::system_error& e) {
    BErrNo be;
    Dmsg3(010, "Could not open and read state file. ERR=%s - %s\n",
          be.bstrerror(), e.code().message().c_str());
    return false;
  } catch (...) {
    Dmsg0(100, "Could not read file. Some error occurred");
    return false;
  }
  Dmsg1(100, "Read num_items=%d\n", num);
  if (num > 4 * max_count_recent_job_results) { /* sanity check */
    return false;
  }

  std::lock_guard<std::mutex> m(mutex);

  for (; num; num--) {
    RecentJobResultsList::JobResult job;
    try {
      file.read(reinterpret_cast<char*>(&job), sizeof(job));
    } catch (std::system_error& e) {
      BErrNo be;
      Dmsg3(010, "Could not read state file. ERR=%s - %s\n", be.bstrerror(),
            e.code().message().c_str());
      return false;
    } catch (...) {
      Dmsg0(100, "Could not read file. Some error occurred");
      return false;
    }
    if (job.JobId > 0) {
      recent_job_results_list.push_back(job);
      if (recent_job_results_list.size() > max_count_recent_job_results) {
        recent_job_results_list.erase(recent_job_results_list.begin());
      }
    }
  }
  return true;
}

bool RecentJobResultsList::ExportToFile(std::ofstream& file)
{
  if (!recent_job_results_list.empty()) {
    std::lock_guard<std::mutex> m(mutex);
    uint32_t num =
        recent_job_results_list.size();  // always first entry in the file

#if defined HAVE_IS_TRIVIALLY_COPYABLE
    static_assert(
        std::is_trivially_copyable<RecentJobResultsList::JobResult>::value,
        "RecentJobResultsList::JobResult must be trivially copyable");
#endif

    try {
      file.write(reinterpret_cast<char*>(&num), sizeof(num));

      for (auto je : recent_job_results_list) {
        file.write(reinterpret_cast<char*>(&je),
                   sizeof(struct RecentJobResultsList::JobResult));
      }
    } catch (std::system_error& e) {
      BErrNo be;
      Dmsg3(010, "Could not write state file. ERR=%s - %s\n", be.bstrerror(),
            e.code().message().c_str());
      return false;
    } catch (...) {
      Dmsg0(100, "Could not write file. Some error occurred");
      return false;
    }
  }
  return true;
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
