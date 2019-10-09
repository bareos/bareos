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

#include "include/bareos.h"
#include "dird/dird.h"
#include "dird/dird_conf.h"
#include "dird/dird_globals.h"
#include "dird/job.h"
#include "dird/run_hour_validator.h"
#include "dird/scheduler.h"
#include "dird/scheduler_job_item_queue.h"
#include "dird/scheduler_private.h"
#include "dird/scheduler_system_time_source.h"
#include "dird/scheduler_time_adapter.h"
#include "dird/storage.h"
#include "include/make_unique.h"
#include "lib/parse_conf.h"

#include <chrono>
#include <utility>

namespace directordaemon {

static const int debuglevel = 200;

static bool IsAutomaticSchedulerJob(JobResource* job)
{
  if (job->schedule == nullptr) { return false; }
  if (!job->schedule->enabled) { return false; }
  if (!job->enabled) { return false; }
  if ((job->client != nullptr) && !job->client->enabled) { return false; }
  return true;
}

static void SetJcrFromRunResource(JobControlRecord* jcr, RunResource* run)
{
  if (run->level != 0u) {
    jcr->setJobLevel(run->level); /* override run level */
  }

  if (run->pool != nullptr) {
    jcr->res.pool = run->pool; /* override pool */
    jcr->res.run_pool_override = true;
  }

  if (run->full_pool != nullptr) {
    jcr->res.full_pool = run->full_pool; /* override full pool */
    jcr->res.run_full_pool_override = true;
  }

  if (run->vfull_pool != nullptr) {
    jcr->res.vfull_pool = run->vfull_pool; /* override virtual full pool */
    jcr->res.run_vfull_pool_override = true;
  }

  if (run->inc_pool != nullptr) {
    jcr->res.inc_pool = run->inc_pool; /* override inc pool */
    jcr->res.run_inc_pool_override = true;
  }

  if (run->diff_pool != nullptr) {
    jcr->res.diff_pool = run->diff_pool; /* override diff pool */
    jcr->res.run_diff_pool_override = true;
  }

  if (run->next_pool != nullptr) {
    jcr->res.next_pool = run->next_pool; /* override next pool */
    jcr->res.run_next_pool_override = true;
  }

  if (run->storage != nullptr) {
    UnifiedStorageResource store;
    store.store = run->storage;
    PmStrcpy(store.store_source, _("run override"));
    SetRwstorage(jcr, &store); /* override storage */
  }

  if (run->msgs != nullptr) {
    jcr->res.messages = run->msgs; /* override messages */
  }

  if (run->Priority != 0) { jcr->JobPriority = run->Priority; }

  if (run->spool_data_set) { jcr->spool_data = run->spool_data; }

  if (run->accurate_set) {
    jcr->accurate = run->accurate; /* overwrite accurate mode */
  }

  if (run->MaxRunSchedTime_set) { jcr->MaxRunSchedTime = run->MaxRunSchedTime; }
}

JobControlRecord* SchedulerPrivate::TryCreateJobControlRecord(
    const SchedulerJobItem& next_job)
{
  if (next_job.job->client == nullptr) { return nullptr; }
  JobControlRecord* jcr = new_jcr(sizeof(JobControlRecord), DirdFreeJcr);
  SetJcrDefaults(jcr, next_job.job);
  if (next_job.run != nullptr) {
    next_job.run->last_run = time_adapter->time_source_->SystemTime();
    SetJcrFromRunResource(jcr, next_job.run);
  }
  return jcr;
}

void SchedulerPrivate::WaitForJobsToRun()
{
  SchedulerJobItem next_job;

  while (active && !prioritised_job_item_queue.Empty()) {
    next_job = prioritised_job_item_queue.TakeOutTopItem();
    if (!next_job.is_valid) { break; }

    bool job_started = false;
    while (active && !job_started) {
      time_t now = time_adapter->time_source_->SystemTime();

      if (now >= next_job.runtime) {
        JobControlRecord* jcr = TryCreateJobControlRecord(next_job);
        if (jcr != nullptr) { ExecuteJobCallback_(jcr); }
        job_started = true;
      } else {
        time_t wait_interval{std::min(time_adapter->default_wait_interval_,
                                      next_job.runtime - now)};
        time_adapter->time_source_->SleepFor(
            std::chrono::seconds(wait_interval));
      }
    }
  }
}

void SchedulerPrivate::FillSchedulerJobQueueOrSleep()
{
  while (active && prioritised_job_item_queue.Empty()) {
    AddJobsForThisAndNextHourToQueue();
    if (prioritised_job_item_queue.Empty()) {
      time_adapter->time_source_->SleepFor(
          std::chrono::seconds(time_adapter->default_wait_interval_));
    }
  }
}

static time_t CalculateRuntime(time_t time, uint32_t minute)
{
  struct tm tm {
  };
  Blocaltime(&time, &tm);
  tm.tm_min = minute;
  tm.tm_sec = 0;
  return mktime(&tm);
}

void SchedulerPrivate::AddJobsForThisAndNextHourToQueue()
{
  Dmsg0(debuglevel, "Begin AddJobsForThisAndNextHourToQueue\n");

  RunHourValidator this_hour(time_adapter->time_source_->SystemTime());
  this_hour.PrintDebugMessage(debuglevel);

  RunHourValidator next_hour(this_hour.Time() + 3600);
  next_hour.PrintDebugMessage(debuglevel);

  JobResource* job = nullptr;

  LockRes(my_config);
  foreach_res (job, R_JOB) {
    if (!IsAutomaticSchedulerJob(job)) { continue; }

    Dmsg1(debuglevel, "Got job: %s\n", job->resource_name_);

    for (RunResource* run = job->schedule->run; run != nullptr;
         run = run->next) {
      bool run_this_hour = this_hour.TriggersOn(run->date_time_bitfield);
      bool run_next_hour = next_hour.TriggersOn(run->date_time_bitfield);

      Dmsg3(debuglevel, "run@%p: run_now=%d run_next_hour=%d\n", run,
            run_this_hour, run_next_hour);

      if (run_this_hour || run_next_hour) {
        time_t runtime = CalculateRuntime(this_hour.Time(), run->minute);
        if (run_this_hour) {
          AddJobToQueue(job, run, this_hour.Time(), runtime);
        }
        if (run_next_hour) {
          AddJobToQueue(job, run, this_hour.Time(), runtime + 3600);
        }
      }
    }
  }
  UnlockRes(my_config);
  Dmsg0(debuglevel, "Finished AddJobsForThisAndNextHourToQueue\n");
}

void SchedulerPrivate::AddJobToQueue(JobResource* job,
                                     RunResource* run,
                                     time_t now,
                                     time_t runtime)
{
  if (run != nullptr) {
    if ((runtime - run->last_run) < 61) { return; }
  }

  if ((runtime + 59) < now) { return; }

  try {
    prioritised_job_item_queue.EmplaceItem(job, run, runtime);
  } catch (const std::invalid_argument& e) {
    Dmsg1(debuglevel, "Could not emplace job: %s\n", e.what());
  }
}

void SchedulerPrivate::AddJobToQueue(JobResource* job)
{
  time_t now = time_adapter->time_source_->SystemTime();
  AddJobToQueue(job, nullptr, now, now);
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
}

SchedulerPrivate::SchedulerPrivate(
    std::unique_ptr<SchedulerTimeAdapter> time_adapter,
    std::function<void(JobControlRecord*)> ExecuteJobCallback)
    : time_adapter{std::move(time_adapter)}
    , ExecuteJobCallback_{std::move(std::move(ExecuteJobCallback))}
{
}

SchedulerPrivate::~SchedulerPrivate() = default;


}  // namespace directordaemon
