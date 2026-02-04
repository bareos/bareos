/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2025 Bareos GmbH & Co. KG

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
#include "dird/dird.h"
#include "dird/dird_conf.h"
#include "dird/dird_globals.h"
#include "dird/director_jcr_impl.h"
#include "dird/job.h"
#include "dird/date_time.h"
#include "dird/scheduler.h"
#include "dird/scheduler_job_item_queue.h"
#include "dird/scheduler_private.h"
#include "dird/scheduler_system_time_source.h"
#include "dird/scheduler_time_adapter.h"
#include "dird/storage.h"
#include "lib/parse_conf.h"
#include "dird/jcr_util.h"

#include <chrono>
#include <utility>

namespace directordaemon {

using std::chrono::seconds;

static constexpr int local_debuglevel = 200;

static bool IsAutomaticSchedulerJob(JobResource* job)
{
  Dmsg1(local_debuglevel + 100,
        "Scheduler: Check if job IsAutomaticSchedulerJob %s.\n",
        job->resource_name_);
  if (job->schedule == nullptr) { return false; }
  if (!job->schedule->enabled) { return false; }
  if (!job->enabled) { return false; }
  if ((job->client != nullptr) && !job->client->enabled) { return false; }
  Dmsg1(local_debuglevel + 100,
        "Scheduler: Check if job IsAutomaticSchedulerJob %s: Yes.\n",
        job->resource_name_);
  return true;
}

static void SetJcrFromRunResource(JobControlRecord* jcr, RunResource* run)
{
  if (run->level != 0U) {
    jcr->setJobLevel(run->level); /* override run level */
  }

  if (run->pool != nullptr) {
    jcr->dir_impl->res.pool = run->pool; /* override pool */
    jcr->dir_impl->res.run_pool_override = true;
  }

  if (run->full_pool != nullptr) {
    jcr->dir_impl->res.full_pool = run->full_pool; /* override full pool */
    jcr->dir_impl->res.run_full_pool_override = true;
  }

  if (run->vfull_pool != nullptr) {
    jcr->dir_impl->res.vfull_pool
        = run->vfull_pool; /* override virtual full pool */
    jcr->dir_impl->res.run_vfull_pool_override = true;
  }

  if (run->inc_pool != nullptr) {
    jcr->dir_impl->res.inc_pool = run->inc_pool; /* override inc pool */
    jcr->dir_impl->res.run_inc_pool_override = true;
  }

  if (run->diff_pool != nullptr) {
    jcr->dir_impl->res.diff_pool = run->diff_pool; /* override diff pool */
    jcr->dir_impl->res.run_diff_pool_override = true;
  }

  if (run->next_pool != nullptr) {
    jcr->dir_impl->res.next_pool = run->next_pool; /* override next pool */
    jcr->dir_impl->res.run_next_pool_override = true;
  }

  if (run->storage != nullptr) {
    UnifiedStorageResource store;
    store.store = run->storage;
    PmStrcpy(store.store_source, T_("run override"));
    SetRwstorage(jcr, &store); /* override storage */
  }

  if (run->msgs != nullptr) {
    jcr->dir_impl->res.messages = run->msgs; /* override messages */
  }

  if (run->Priority != 0) { jcr->JobPriority = run->Priority; }

  if (run->spool_data_set) { jcr->dir_impl->spool_data = run->spool_data; }

  if (run->accurate_set) {
    jcr->accurate = run->accurate; /* overwrite accurate mode */
  }

  if (run->MaxRunSchedTime_set) {
    jcr->dir_impl->MaxRunSchedTime = run->MaxRunSchedTime;
  }
}

JobControlRecord* SchedulerPrivate::TryCreateJobControlRecord(
    const SchedulerJobItem& next_job)
{
  JobControlRecord* jcr = NewDirectorJcr(DirdFreeJcr);
  SetJcrDefaults(jcr, next_job.job);
  if (next_job.run != nullptr) {
    next_job.run->scheduled_last = time_adapter->time_source_->SystemTime();
    SetJcrFromRunResource(jcr, next_job.run);
  }
  return jcr;
}

void SchedulerPrivate::WaitForJobsToRun()
{
  while (active && !prioritised_job_item_queue.Empty()) {
    auto next_job = prioritised_job_item_queue.TopItem();
    if (!next_job.is_valid) { break; }

    time_t now = time_adapter->time_source_->SystemTime();

    if (now >= next_job.runtime) {
      auto run_job = prioritised_job_item_queue.TakeOutTopItem();
      if (!run_job.is_valid || !run_job.job->enabled) {
        continue;  // check queue again
      }
      JobControlRecord* jcr = TryCreateJobControlRecord(run_job);
      if (jcr != nullptr) {
        Dmsg1(local_debuglevel, "Scheduler: Running job %s.\n",
              run_job.job->resource_name_);
        jcr->dir_impl->job_trigger = next_job.job_trigger;
        ExecuteJobCallback_(jcr);
      }

    } else {
      time_t wait_interval{std::min(time_adapter->default_wait_interval_,
                                    next_job.runtime - now)};
      Dmsg2(local_debuglevel,
            "Scheduler: WaitForJobsToRun is sleeping for %lld seconds. Next "
            "job: %s.",
            static_cast<long long>(wait_interval),
            next_job.job->resource_name_);

      time_adapter->time_source_->SleepFor(seconds(wait_interval));
    }
  }
}

void SchedulerPrivate::FillSchedulerJobQueueOrSleep()
{
  while (active && prioritised_job_item_queue.Empty()) {
    AddJobsForThisAndNextHourToQueue();
    if (prioritised_job_item_queue.Empty()) {
      time_adapter->time_source_->SleepFor(
          seconds(time_adapter->default_wait_interval_));
    }
  }
}

void SchedulerPrivate::AddJobsForThisAndNextHourToQueue()
{
  Dmsg0(local_debuglevel, "Begin AddJobsForThisAndNextHourToQueue\n");

  time_t now = time_adapter->time_source_->SystemTime();
  DateTime(now).PrintDebugMessage(local_debuglevel);
  DateTime(now + kSecondsPerHour).PrintDebugMessage(local_debuglevel);

  JobResource* job = nullptr;

  foreach_res (job, R_JOB) {
    if (!IsAutomaticSchedulerJob(job)) { continue; }

    Dmsg1(local_debuglevel, "Got job: %s\n", job->resource_name_);

    for (RunResource* run = job->schedule->run; run != nullptr;
         run = run->next) {
      for (auto runtime : run->schedule.GetMatchingTimes(now, now + kSecondsPerHour)) {
        AddJobToQueue(job, run, now, runtime, JobTrigger::kScheduler);
      }
    }
  }
  Dmsg0(local_debuglevel, "Finished AddJobsForThisAndNextHourToQueue\n");
}

void SchedulerPrivate::AddJobToQueue(JobResource* job,
                                     RunResource* run,
                                     time_t now,
                                     time_t runtime,
                                     JobTrigger job_trigger)
{
  Dmsg1(local_debuglevel + 100, "Scheduler: Try AddJobToQueue %s.\n",
        job->resource_name_);

  if (run != nullptr) {
    if ((runtime - run->scheduled_last) <= 60) { return; }
  }

  if ((runtime + 60) <= now) { return; }

  try {
    Dmsg1(local_debuglevel + 100, "Scheduler: Put job %s into queue.\n",
          job->resource_name_);

    prioritised_job_item_queue.EmplaceItem(job, run, runtime, job_trigger);

  } catch (const std::invalid_argument& e) {
    Dmsg1(local_debuglevel + 100, "Could not emplace job: %s\n", e.what());
  }
}

void SchedulerPrivate::AddJobWithNoRunResourceToQueue(JobResource* job,
                                                      JobTrigger job_trigger)
{
  time_t now = time_adapter->time_source_->SystemTime();
  AddJobToQueue(job, nullptr, now, now, job_trigger);
}

class DefaultSchedulerTimeAdapter : public SchedulerTimeAdapter {
 public:
  DefaultSchedulerTimeAdapter()
      : SchedulerTimeAdapter(std::make_unique<SystemTimeSource>())
  {
    default_wait_interval_ = 60;
  }
};

SchedulerPrivate::SchedulerPrivate()
    : time_adapter{std::make_unique<DefaultSchedulerTimeAdapter>()}
    , ExecuteJobCallback_{ExecuteJob}
{
  // this is the default director scheduler
}

SchedulerPrivate::SchedulerPrivate(
    std::unique_ptr<SchedulerTimeAdapter> t_time_adapter,
    std::function<void(JobControlRecord*)> ExecuteJobCallback)
    : time_adapter{std::move(t_time_adapter)}
    , ExecuteJobCallback_{std::move(std::move(ExecuteJobCallback))}
{
  // constructor used for tests to inject mocked time adapter and callbacks
}

SchedulerPrivate::~SchedulerPrivate() = default;


}  // namespace directordaemon
