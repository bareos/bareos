/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2019-2019 Free Software Foundation Europe e.V.

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

#ifndef BAREOS_SRC_DIRD_SCHEDULER_JOB_ITEM_QUEUE_H_
#define BAREOS_SRC_DIRD_SCHEDULER_JOB_ITEM_QUEUE_H_

#include "include/bareos.h"

#include <queue>
#include <vector>

namespace directordaemon {

class RunResource;
class JobResource;
struct SchedulerJobItemQueuePrivate;

struct SchedulerJobItem {
  ~SchedulerJobItem() = default;

  SchedulerJobItem() = default;
  SchedulerJobItem(const SchedulerJobItem& other) = default;
  SchedulerJobItem(SchedulerJobItem&& other) = default;
  SchedulerJobItem& operator=(const SchedulerJobItem& other) = default;
  SchedulerJobItem& operator=(SchedulerJobItem&& other) = default;

  SchedulerJobItem(JobResource* job_in,
                   RunResource* run_in,
                   time_t runtime_in,
                   int priority_in) noexcept
      : job(job_in), run(run_in), runtime(runtime_in), priority(priority_in)
  {
    is_valid = job && runtime;
  };

  bool operator==(const SchedulerJobItem& rhs) const
  {
    return runtime == rhs.runtime && job == rhs.job &&
           priority == rhs.priority && run == rhs.run;
  }

  bool operator!=(const SchedulerJobItem& rhs) const { return !(*this == rhs); }

  JobResource* job{nullptr};
  RunResource* run{nullptr};
  time_t runtime{0};
  int priority{10};
  bool is_valid{false};
};

class SchedulerJobItemQueue {
 public:
  SchedulerJobItemQueue();
  ~SchedulerJobItemQueue();

  SchedulerJobItem TopItem() const;
  SchedulerJobItem TakeOutTopItem();
  void EmplaceItem(JobResource* job, RunResource* run, time_t runtime);
  bool Empty() const;
  void Clear();

  SchedulerJobItemQueue(const SchedulerJobItemQueue& other) = delete;
  SchedulerJobItemQueue(SchedulerJobItemQueue&& other) = delete;
  SchedulerJobItemQueue& operator=(const SchedulerJobItemQueue& other) = delete;
  SchedulerJobItemQueue& operator=(SchedulerJobItemQueue&& other) = delete;

 private:
  static constexpr int default_priority{10};
  std::unique_ptr<SchedulerJobItemQueuePrivate> impl_;
};

}  // namespace directordaemon

#endif  // BAREOS_SRC_DIRD_SCHEDULER_JOB_ITEM_QUEUE_H_
