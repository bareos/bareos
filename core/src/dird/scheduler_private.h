/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2019 Bareos GmbH & Co. KG

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

#ifndef BAREOS_SRC_DIRD_SCHEDULER_PRIVATE_H_
#define BAREOS_SRC_DIRD_SCHEDULER_PRIVATE_H_

#include "dird/scheduler_job_item_queue.h"

#include <atomic>
#include <functional>
#include <memory>

namespace directordaemon {

class SchedulerTimeAdapter;

struct SchedulerPrivate {
  SchedulerPrivate();
  SchedulerPrivate(std::unique_ptr<SchedulerTimeAdapter> time_adapter,
                   std::function<void(JobControlRecord*)> ExecuteJobCallback);
  ~SchedulerPrivate();

  void WaitForJobsToRun();
  void FillSchedulerJobQueueOrSleep();
  void AddJobToQueue(JobResource* job);

  std::unique_ptr<SchedulerTimeAdapter> time_adapter;
  SchedulerJobItemQueue prioritised_job_item_queue;
  std::atomic<bool> active{true};

 private:
  std::function<void(JobControlRecord*)> ExecuteJobCallback_;
  JobControlRecord* TryCreateJobControlRecord(const SchedulerJobItem& next_job);
  void AddJobsForThisAndNextHourToQueue();
  void AddJobToQueue(JobResource* job,
                     RunResource* run,
                     time_t now,
                     time_t runtime);
};

}  // namespace directordaemon

#endif  // BAREOS_SRC_DIRD_SCHEDULER_PRIVATE_H_
