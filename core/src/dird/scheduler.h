/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2020 Bareos GmbH & Co. KG

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

#ifndef BAREOS_DIRD_SCHEDULER_H_
#define BAREOS_DIRD_SCHEDULER_H_

#include "dird/job_trigger.h"

#include <memory>
#include <functional>

class JobControlRecord;

namespace directordaemon {

class JobResource;
class SchedulerTimeAdapter;
class SchedulerPrivate;

class Scheduler {
 public:
  Scheduler() noexcept;
  Scheduler(std::unique_ptr<SchedulerTimeAdapter> time_adapter,
            std::function<void(JobControlRecord*)> ExecuteJob) noexcept;
  ~Scheduler();

  void Run();
  void Terminate();
  void ClearQueue();
  void AddJobWithNoRunResourceToQueue(JobResource* job, JobTrigger job_trigger);
  static Scheduler& GetMainScheduler() noexcept;

 private:
  std::unique_ptr<SchedulerPrivate> impl_;
};

} /* namespace directordaemon */

#endif  // BAREOS_DIRD_SCHEDULER_H_
