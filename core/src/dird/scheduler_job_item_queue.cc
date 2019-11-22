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

#include "include/bareos.h"
#include "dird/dird_conf.h"
#include "include/make_unique.h"
#include "scheduler_job_item_queue.h"

#include <iostream>
#include <stdexcept>

namespace directordaemon {

struct PrioritiseJobItems {
  bool operator()(const SchedulerJobItem& a, const SchedulerJobItem& b) const
  {
    bool a_runs_before_b = a.runtime < b.runtime;
    bool a_has_higher_priority_than_b =
        a.runtime == b.runtime && a.priority < b.priority;
    // invert for std::priority_queue sort algorithm
    return !(a_runs_before_b || a_has_higher_priority_than_b);
  }
};

struct SchedulerJobItemQueuePrivate {
  mutable std::mutex mutex;
  std::priority_queue<SchedulerJobItem,
                      std::vector<SchedulerJobItem>,
                      PrioritiseJobItems>
      priority_queue;
};

SchedulerJobItemQueue::SchedulerJobItemQueue()
    : impl_(std::make_unique<SchedulerJobItemQueuePrivate>())
{
}

SchedulerJobItemQueue::~SchedulerJobItemQueue() = default;

SchedulerJobItem SchedulerJobItemQueue::TakeOutTopItem()
{
  SchedulerJobItem job_item_with_highest_priority;

  std::lock_guard<std::mutex> lg(impl_->mutex);

  if (!impl_->priority_queue.empty()) {
    job_item_with_highest_priority = impl_->priority_queue.top();
    impl_->priority_queue.pop();
  }

  return job_item_with_highest_priority;
}

SchedulerJobItem SchedulerJobItemQueue::TopItem() const
{
  SchedulerJobItem job_item_with_highest_priority;

  std::lock_guard<std::mutex> lg(impl_->mutex);

  if (!impl_->priority_queue.empty()) {
    job_item_with_highest_priority = impl_->priority_queue.top();
  }

  return job_item_with_highest_priority;
}

void SchedulerJobItemQueue::EmplaceItem(JobResource* job,
                                        RunResource* run,
                                        time_t runtime)
{
  if (job == nullptr) {
    throw std::invalid_argument("Invalid Argument: JobResource is undefined");
  }
  if (runtime == 0) {
    throw std::invalid_argument("Invalid Argument: runtime is invalid");
  }
  int priority = run != nullptr
                     ? (run->Priority != 0 ? run->Priority : job->Priority)
                     : default_priority;

  std::lock_guard<std::mutex> lg(impl_->mutex);
  impl_->priority_queue.emplace(job, run, runtime, priority);
}

bool SchedulerJobItemQueue::Empty() const
{
  std::lock_guard<std::mutex> lg(impl_->mutex);
  return impl_->priority_queue.empty();
}

void SchedulerJobItemQueue::Clear()
{
  std::lock_guard<std::mutex> lg(impl_->mutex);
  while (!impl_->priority_queue.empty()) { impl_->priority_queue.pop(); }
}

}  // namespace directordaemon
