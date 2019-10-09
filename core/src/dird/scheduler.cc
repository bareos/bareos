/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2018 Bareos GmbH & Co. KG

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
/*
 * Kern Sibbald, May MM, major revision December MMIII
 */
/**
 * @file
 * BAREOS scheduler
 *
 * It looks at what jobs are to be run and when
 * and waits around until it is time to
 * fire them up.
 */

#include "include/bareos.h"
#include "dird.h"
#include "dird/dird_globals.h"
#include "dird/jcr_private.h"
#include "dird/job.h"
#include "dird/storage.h"
#include "lib/parse_conf.h"

class JobControlRecord;

namespace directordaemon {

class JobResource;
class SchedulerTimeAdapter;

static const int debuglevel = 200;

Scheduler& Scheduler::GetMainScheduler() noexcept
{
  static Scheduler scheduler;
  return scheduler;
  }

Scheduler::Scheduler() noexcept : impl_(std::make_unique<SchedulerPrivate>()){};

Scheduler::Scheduler(std::unique_ptr<SchedulerTimeAdapter> time_adapter,
                     std::function<void(JobControlRecord*)> ExecuteJob) noexcept
    : impl_(std::make_unique<SchedulerPrivate>(
          std::forward<std::unique_ptr<SchedulerTimeAdapter>>(time_adapter),
          std::forward<std::function<void(JobControlRecord*)>>(ExecuteJob)))
{
  }

Scheduler::~Scheduler() = default;

void Scheduler::AddJobWithNoRunResourceToQueue(JobResource* job)
{
  impl_->AddJobToQueue(job);
}

void Scheduler::Run()
{
  while (impl_->active) {
    Dmsg0(debuglevel, "Scheduler Cycle\n");
    impl_->FillSchedulerJobQueueOrSleep();
    impl_->WaitForJobsToRun();
    }
  }

void Scheduler::Terminate()
{
  impl_->active = false;
  impl_->time_adapter->time_source_->Terminate();
    }

void Scheduler::ClearQueue() { impl_->prioritised_job_item_queue.Clear(); }

} /* namespace directordaemon */
