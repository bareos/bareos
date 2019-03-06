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
#include "dird/job.h"
#include "dird/storage.h"
#include "lib/parse_conf.h"

namespace directordaemon {

const int debuglevel = 200;

/* Local variables */
struct job_item {
  RunResource* run;
  JobResource* job;
  time_t runtime;
  int Priority;
  dlink link; /* link for list */
};

/* List of jobs to be run. They were scheduled in this hour or the next */
static dlist* jobs_to_run; /* list of jobs to be run */

/* Time interval in secs to sleep if nothing to be run */
static int const next_check_secs = 60;

/* Forward referenced subroutines */
static void find_runs();
static void add_job(JobResource* job,
                    RunResource* run,
                    time_t now,
                    time_t runtime);

/* Imported subroutines */

/* Imported variables */

/**
 * called by reload_config to tell us that the schedules
 * we may have based our next jobs to run queues have been
 * invalidated.  In fact the schedules may not have changed
 * but the run object that we have recorded the last_run time
 * on are new and no longer have a valid last_run time which
 * causes us to double run schedules that get put into the list
 * because run_nh = 1.
 */
static bool schedules_invalidated = false;
void InvalidateSchedules(void) { schedules_invalidated = true; }

/**
 *
 *         Main Bareos Scheduler
 *
 */
JobControlRecord* wait_for_next_job(char* one_shot_job_to_run)
{
  JobControlRecord* jcr;
  JobResource* job;
  RunResource* run;
  time_t now, prev;
  static bool first = true;
  job_item* next_job = NULL;

  Dmsg0(debuglevel, "Enter wait_for_next_job\n");
  if (first) {
    first = false;
    /* Create scheduled jobs list */
    jobs_to_run = New(dlist(next_job, &next_job->link));
    if (one_shot_job_to_run) { /* one shot */
      job = (JobResource*)my_config->GetResWithName(R_JOB, one_shot_job_to_run);
      if (!job) {
        Emsg1(M_ABORT, 0, _("Job %s not found\n"), one_shot_job_to_run);
      }
      Dmsg1(5, "Found one_shot_job_to_run %s\n", one_shot_job_to_run);
      jcr = new_jcr(sizeof(JobControlRecord), DirdFreeJcr);
      SetJcrDefaults(jcr, job);
      return jcr;
    }
  }

  /* Wait until we have something in the
   * next hour or so.
   */
again:
  while (jobs_to_run->empty()) {
    find_runs();
    if (!jobs_to_run->empty()) { break; }
    Bmicrosleep(next_check_secs, 0); /* recheck once per minute */
  }

  /*
   * Pull the first job to run (already sorted by runtime and
   *  Priority, then wait around until it is time to run it.
   */
  next_job = (job_item*)jobs_to_run->first();
  jobs_to_run->remove(next_job);

  if (!next_job) { /* we really should have something now */
    Emsg0(M_ABORT, 0, _("Scheduler logic error\n"));
  }

  /* Now wait for the time to run the job */
  for (;;) {
    time_t twait;
    /* discard scheduled queue and rebuild with new schedule objects. */
    LockJobs();
    if (schedules_invalidated) {
      free(next_job);
      while (!jobs_to_run->empty()) {
        next_job = (job_item*)jobs_to_run->first();
        jobs_to_run->remove(next_job);
        free(next_job);
      }
      schedules_invalidated = false;
      UnlockJobs();
      goto again;
    }
    UnlockJobs();
    prev = now = time(NULL);
    twait = next_job->runtime - now;
    if (twait <= 0) { /* time to run it */
      break;
    }
    /* Recheck at least once per minute */
    Bmicrosleep((next_check_secs < twait) ? next_check_secs : twait, 0);
    /* Attempt to handle clock shift (but not daylight savings time changes)
     * we allow a skew of 10 seconds before invalidating everything.
     */
    now = time(NULL);
    if (now < prev - 10 || now > (prev + next_check_secs + 10)) {
      schedules_invalidated = true;
    }
  }

  jcr = new_jcr(sizeof(JobControlRecord), DirdFreeJcr);
  run = next_job->run; /* pick up needed values */
  job = next_job->job;

  if (job->enabled && (!job->client || job->client->enabled)) {}

  free(next_job);

  if (!job->enabled || (job->schedule && !job->schedule->enabled) ||
      (job->client && !job->client->enabled)) {
    FreeJcr(jcr);
    goto again; /* ignore this job */
  }

  run->last_run = now; /* mark as run now */

  ASSERT(job);
  SetJcrDefaults(jcr, job);
  if (run->level) { jcr->setJobLevel(run->level); /* override run level */ }

  if (run->pool) {
    jcr->res.pool = run->pool; /* override pool */
    jcr->res.run_pool_override = true;
  }

  if (run->full_pool) {
    jcr->res.full_pool = run->full_pool; /* override full pool */
    jcr->res.run_full_pool_override = true;
  }

  if (run->vfull_pool) {
    jcr->res.vfull_pool = run->vfull_pool; /* override virtual full pool */
    jcr->res.run_vfull_pool_override = true;
  }

  if (run->inc_pool) {
    jcr->res.inc_pool = run->inc_pool; /* override inc pool */
    jcr->res.run_inc_pool_override = true;
  }

  if (run->diff_pool) {
    jcr->res.diff_pool = run->diff_pool; /* override diff pool */
    jcr->res.run_diff_pool_override = true;
  }

  if (run->next_pool) {
    jcr->res.next_pool = run->next_pool; /* override next pool */
    jcr->res.run_next_pool_override = true;
  }

  if (run->storage) {
    UnifiedStorageResource store;
    store.store = run->storage;
    PmStrcpy(store.store_source, _("run override"));
    SetRwstorage(jcr, &store); /* override storage */
  }

  if (run->msgs) { jcr->res.messages = run->msgs; /* override messages */ }

  if (run->Priority) { jcr->JobPriority = run->Priority; }

  if (run->spool_data_set) { jcr->spool_data = run->spool_data; }

  if (run->accurate_set) {
    jcr->accurate = run->accurate; /* overwrite accurate mode */
  }

  if (run->MaxRunSchedTime_set) { jcr->MaxRunSchedTime = run->MaxRunSchedTime; }

  Dmsg0(debuglevel, "Leave wait_for_next_job()\n");
  return jcr;
}

/**
 * Shutdown the scheduler
 */
void TermScheduler()
{
  if (jobs_to_run) { delete jobs_to_run; }
}

/**
 * check if given day of year is in last week of the month in the current year
 * depending if the year is leap year or not, the doy of the last day of the
 * month is varying one day.
 */
bool IsDoyInLastWeek(int year, int doy)
{
  int i;
  int* last_dom;
  int last_day_of_month[] = {31,  59,  90,  120, 151, 181,
                             212, 243, 273, 304, 334, 365};
  int last_day_of_month_leap[] = {31,  60,  91,  121, 152, 182,
                                  213, 244, 274, 305, 335, 366};

  /*
   * Determine if this is a leap year.
   */
  if (year % 400 == 0 || (year % 100 != 0 && year % 4 == 0)) {
    last_dom = last_day_of_month_leap;
  } else {
    last_dom = last_day_of_month;
  }

  for (i = 0; i < 12; i++) {
    /* doy is zero-based */
    if (doy > ((last_dom[i] - 1) - 7) && doy <= (last_dom[i] - 1)) {
      return true;
    }
  }

  return false;
}

/**
 * Find all jobs to be run this hour and the next hour.
 */
static void find_runs()
{
  time_t now, next_hour, runtime;
  RunResource* run;
  JobResource* job;
  ScheduleResource* sched;
  struct tm tm;
  bool is_last_week = false;    /* are we in the last week of a month? */
  bool nh_is_last_week = false; /* are we in the last week of a month? */
  int hour, mday, wday, month, wom, woy, yday;
  /* Items corresponding to above at the next hour */
  int nh_hour, nh_mday, nh_wday, nh_month, nh_wom, nh_woy, nh_yday;

  Dmsg0(debuglevel, "enter find_runs()\n");

  /*
   * Compute values for time now
   */
  now = time(NULL);
  Blocaltime(&now, &tm);
  hour = tm.tm_hour;
  mday = tm.tm_mday - 1;
  wday = tm.tm_wday;
  month = tm.tm_mon;
  wom = mday / 7;
  woy = TmWoy(now);  /* get week of year */
  yday = tm.tm_yday; /* get day of year */

  Dmsg8(debuglevel, "now = %x: h=%d m=%d md=%d wd=%d wom=%d woy=%d yday=%d\n",
        now, hour, month, mday, wday, wom, woy, yday);

  is_last_week = IsDoyInLastWeek(tm.tm_year + 1900, yday);

  /*
   * Compute values for next hour from now.
   * We do this to be sure we don't miss a job while
   * sleeping.
   */
  next_hour = now + 3600;
  Blocaltime(&next_hour, &tm);
  nh_hour = tm.tm_hour;
  nh_mday = tm.tm_mday - 1;
  nh_wday = tm.tm_wday;
  nh_month = tm.tm_mon;
  nh_wom = nh_mday / 7;
  nh_woy = TmWoy(next_hour); /* get week of year */
  nh_yday = tm.tm_yday;      /* get day of year */

  Dmsg8(debuglevel, "nh = %x: h=%d m=%d md=%d wd=%d wom=%d woy=%d yday=%d\n",
        next_hour, nh_hour, nh_month, nh_mday, nh_wday, nh_wom, nh_woy,
        nh_yday);

  nh_is_last_week = IsDoyInLastWeek(tm.tm_year + 1900, nh_yday);

  /*
   * Loop through all jobs
   */
  LockRes(my_config);
  foreach_res (job, R_JOB) {
    sched = job->schedule;
    if (sched == NULL || !sched->enabled || !job->enabled ||
        (job->client && !job->client->enabled)) { /* scheduled? or enabled? */
      continue;                                   /* no, skip this job */
    }

    Dmsg1(debuglevel, "Got job: %s\n", job->hdr.name);
    for (run = sched->run; run; run = run->next) {
      bool run_now, run_nh;
      /*
       * Find runs scheduled between now and the next hour.
       */
      run_now = BitIsSet(hour, run->hour) && BitIsSet(mday, run->mday) &&
                BitIsSet(wday, run->wday) && BitIsSet(month, run->month) &&
                (BitIsSet(wom, run->wom) || (run->last_set && is_last_week)) &&
                BitIsSet(woy, run->woy);

      run_nh =
          BitIsSet(nh_hour, run->hour) && BitIsSet(nh_mday, run->mday) &&
          BitIsSet(nh_wday, run->wday) && BitIsSet(nh_month, run->month) &&
          (BitIsSet(nh_wom, run->wom) || (run->last_set && nh_is_last_week)) &&
          BitIsSet(nh_woy, run->woy);

      Dmsg3(debuglevel, "run@%p: run_now=%d run_nh=%d\n", run, run_now, run_nh);

      if (run_now || run_nh) {
        /*
         * find time (time_t) job is to be run
         */
        Blocaltime(&now, &tm);   /* reset tm structure */
        tm.tm_min = run->minute; /* set run minute */
        tm.tm_sec = 0;           /* zero secs */
        runtime = mktime(&tm);
        if (run_now) { add_job(job, run, now, runtime); }
        /* If job is to be run in the next hour schedule it */
        if (run_nh) { add_job(job, run, now, runtime + 3600); }
      }
    }
  }
  UnlockRes(my_config);
  Dmsg0(debuglevel, "Leave find_runs()\n");
}

static void add_job(JobResource* job,
                    RunResource* run,
                    time_t now,
                    time_t runtime)
{
  job_item* ji;
  bool inserted = false;
  /*
   * Don't run any job that ran less than a minute ago, but
   *  do run any job scheduled less than a minute ago.
   */
  if (((runtime - run->last_run) < 61) || ((runtime + 59) < now)) { return; }
  /* accept to run this job */
  job_item* je = (job_item*)malloc(sizeof(job_item));
  je->run = run;
  je->job = job;
  je->runtime = runtime;
  if (run->Priority) {
    je->Priority = run->Priority;
  } else {
    je->Priority = job->Priority;
  }

  /* Add this job to the wait queue in runtime, priority sorted order */
  foreach_dlist (ji, jobs_to_run) {
    if (ji->runtime > je->runtime ||
        (ji->runtime == je->runtime && ji->Priority > je->Priority)) {
      jobs_to_run->InsertBefore(je, ji);
      inserted = true;
      break;
    }
  }
  /* If place not found in queue, append it */
  if (!inserted) { jobs_to_run->append(je); }
}
} /* namespace directordaemon */
