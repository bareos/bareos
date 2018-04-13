/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2016 Bareos GmbH & Co. KG

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

#include "bareos.h"
#include "dird.h"

#if 0
#define SCHED_DEBUG
#define DBGLVL 0
#else
#undef SCHED_DEBUG
#define DBGLVL 200
#endif

const int dbglvl = DBGLVL;

/* Local variables */
struct job_item {
   RunResource *run;
   JobResource *job;
   time_t runtime;
   int Priority;
   dlink link;                        /* link for list */
};

/* List of jobs to be run. They were scheduled in this hour or the next */
static dlist *jobs_to_run;               /* list of jobs to be run */

/* Time interval in secs to sleep if nothing to be run */
static int const next_check_secs = 60;

/* Forward referenced subroutines */
static void find_runs();
static void add_job(JobResource *job, RunResource *run, time_t now, time_t runtime);
static void dump_job(job_item *ji, const char *msg);

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
void invalidate_schedules(void) {
    schedules_invalidated = true;
}

/**
 *
 *         Main Bareos Scheduler
 *
 */
JobControlRecord *wait_for_next_job(char *one_shot_job_to_run)
{
   JobControlRecord *jcr;
   JobResource *job;
   RunResource *run;
   time_t now, prev;
   static bool first = true;
   job_item *next_job = NULL;

   Dmsg0(dbglvl, "Enter wait_for_next_job\n");
   if (first) {
      first = false;
      /* Create scheduled jobs list */
      jobs_to_run = New(dlist(next_job, &next_job->link));
      if (one_shot_job_to_run) {            /* one shot */
         job = (JobResource *)GetResWithName(R_JOB, one_shot_job_to_run);
         if (!job) {
            Emsg1(M_ABORT, 0, _("Job %s not found\n"), one_shot_job_to_run);
         }
         Dmsg1(5, "Found one_shot_job_to_run %s\n", one_shot_job_to_run);
         jcr = new_jcr(sizeof(JobControlRecord), dird_free_jcr);
         set_jcr_defaults(jcr, job);
         return jcr;
      }
   }

   /* Wait until we have something in the
    * next hour or so.
    */
again:
   while (jobs_to_run->empty()) {
      find_runs();
      if (!jobs_to_run->empty()) {
         break;
      }
      bmicrosleep(next_check_secs, 0); /* recheck once per minute */
   }

#ifdef  list_chain
   job_item *je;
   foreach_dlist(je, jobs_to_run) {
      dump_job(je, _("Walk queue"));
   }
#endif
   /*
    * Pull the first job to run (already sorted by runtime and
    *  Priority, then wait around until it is time to run it.
    */
   next_job = (job_item *)jobs_to_run->first();
   jobs_to_run->remove(next_job);

   dump_job(next_job, _("Dequeued job"));

   if (!next_job) {                /* we really should have something now */
      Emsg0(M_ABORT, 0, _("Scheduler logic error\n"));
   }

   /* Now wait for the time to run the job */
   for (;;) {
      time_t twait;
      /* discard scheduled queue and rebuild with new schedule objects. */
      lock_jobs();
      if (schedules_invalidated) {
          dump_job(next_job, "Invalidated job");
          free(next_job);
          while (!jobs_to_run->empty()) {
              next_job = (job_item *)jobs_to_run->first();
              jobs_to_run->remove(next_job);
              dump_job(next_job, "Invalidated job");
              free(next_job);
          }
          schedules_invalidated = false;
          unlock_jobs();
          goto again;
      }
      unlock_jobs();
      prev = now = time(NULL);
      twait = next_job->runtime - now;
      if (twait <= 0) {               /* time to run it */
         break;
      }
      /* Recheck at least once per minute */
      bmicrosleep((next_check_secs < twait)?next_check_secs:twait, 0);
      /* Attempt to handle clock shift (but not daylight savings time changes)
       * we allow a skew of 10 seconds before invalidating everything.
       */
      now = time(NULL);
      if (now < prev-10 || now > (prev+next_check_secs+10)) {
         schedules_invalidated = true;
      }
   }

   jcr = new_jcr(sizeof(JobControlRecord), dird_free_jcr);
   run = next_job->run;               /* pick up needed values */
   job = next_job->job;

   if (job->enabled && (!job->client || job->client->enabled)) {
      dump_job(next_job, _("Run job"));
   }

   free(next_job);

   if (!job->enabled ||
       (job->schedule && !job->schedule->enabled) ||
       (job->client && !job->client->enabled)) {
      free_jcr(jcr);
      goto again;                     /* ignore this job */
   }

   run->last_run = now;               /* mark as run now */

   ASSERT(job);
   set_jcr_defaults(jcr, job);
   if (run->level) {
      jcr->setJobLevel(run->level);  /* override run level */
   }

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
      UnifiedStoreResource store;
      store.store = run->storage;
      pm_strcpy(store.store_source, _("run override"));
      set_rwstorage(jcr, &store); /* override storage */
   }

   if (run->msgs) {
      jcr->res.messages = run->msgs; /* override messages */
   }

   if (run->Priority) {
      jcr->JobPriority = run->Priority;
   }

   if (run->spool_data_set) {
      jcr->spool_data = run->spool_data;
   }

   if (run->accurate_set) {
      jcr->accurate = run->accurate; /* overwrite accurate mode */
   }

   if (run->MaxRunSchedTime_set) {
      jcr->MaxRunSchedTime = run->MaxRunSchedTime;
   }

   Dmsg0(dbglvl, "Leave wait_for_next_job()\n");
   return jcr;
}

/**
 * Shutdown the scheduler
 */
void term_scheduler()
{
   if (jobs_to_run) {
      delete jobs_to_run;
   }
}

/**
 * check if given day of year is in last week of the month in the current year
 * depending if the year is leap year or not, the doy of the last day of the month
 * is varying one day.
 */
bool is_doy_in_last_week(int year, int doy)
{
   int i;
   int *last_dom;
   int last_day_of_month[] = { 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365 };
   int last_day_of_month_leap[] = { 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366 };

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
   RunResource *run;
   JobResource *job;
   ScheduleResource *sched;
   struct tm tm;
   bool is_last_week = false;         /* are we in the last week of a month? */
   bool nh_is_last_week = false;      /* are we in the last week of a month? */
   int hour, mday, wday, month, wom, woy, yday;
   /* Items corresponding to above at the next hour */
   int nh_hour, nh_mday, nh_wday, nh_month, nh_wom, nh_woy, nh_yday;

   Dmsg0(dbglvl, "enter find_runs()\n");

   /*
    * Compute values for time now
    */
   now = time(NULL);
   blocaltime(&now, &tm);
   hour = tm.tm_hour;
   mday = tm.tm_mday - 1;
   wday = tm.tm_wday;
   month = tm.tm_mon;
   wom = mday / 7;
   woy = tm_woy(now);                 /* get week of year */
   yday = tm.tm_yday;                 /* get day of year */

   Dmsg8(dbglvl, "now = %x: h=%d m=%d md=%d wd=%d wom=%d woy=%d yday=%d\n",
         now, hour, month, mday, wday, wom, woy, yday);

   is_last_week = is_doy_in_last_week(tm.tm_year + 1900 , yday);

   /*
    * Compute values for next hour from now.
    * We do this to be sure we don't miss a job while
    * sleeping.
    */
   next_hour = now + 3600;
   blocaltime(&next_hour, &tm);
   nh_hour = tm.tm_hour;
   nh_mday = tm.tm_mday - 1;
   nh_wday = tm.tm_wday;
   nh_month = tm.tm_mon;
   nh_wom = nh_mday / 7;
   nh_woy = tm_woy(next_hour);        /* get week of year */
   nh_yday = tm.tm_yday;              /* get day of year */

   Dmsg8(dbglvl, "nh = %x: h=%d m=%d md=%d wd=%d wom=%d woy=%d yday=%d\n",
         next_hour, nh_hour, nh_month, nh_mday, nh_wday, nh_wom, nh_woy, nh_yday);

   nh_is_last_week = is_doy_in_last_week(tm.tm_year + 1900 , nh_yday);

   /*
    * Loop through all jobs
    */
   LockRes();
   foreach_res(job, R_JOB) {
      sched = job->schedule;
      if (sched == NULL ||
          !sched->enabled ||
          !job->enabled ||
          (job->client && !job->client->enabled)) { /* scheduled? or enabled? */
         continue;                    /* no, skip this job */
      }

      Dmsg1(dbglvl, "Got job: %s\n", job->hdr.name);
      for (run = sched->run; run; run = run->next) {
         bool run_now, run_nh;
         /*
          * Find runs scheduled between now and the next hour.
          */
#ifdef xxxx
         Dmsg0(000, "\n");
         Dmsg7(000, "run h=%d m=%d md=%d wd=%d wom=%d woy=%d yday=%d\n",
            hour, month, mday, wday, wom, woy, yday);
         Dmsg6(000, "bitset bsh=%d bsm=%d bsmd=%d bswd=%d bswom=%d bswoy=%d\n",
               bit_is_set(hour, run->hour),
               bit_is_set(month, run->month),
               bit_is_set(mday, run->mday),
               bit_is_set(wday, run->wday),
               bit_is_set(wom, run->wom),
               bit_is_set(woy, run->woy));

         Dmsg7(000, "nh_run h=%d m=%d md=%d wd=%d wom=%d woy=%d yday=%d\n",
               nh_hour, nh_month, nh_mday, nh_wday, nh_wom, nh_woy, nh_yday);
         Dmsg6(000, "nh_bitset bsh=%d bsm=%d bsmd=%d bswd=%d bswom=%d bswoy=%d\n",
               bit_is_set(nh_hour, run->hour),
               bit_is_set(nh_month, run->month),
               bit_is_set(nh_mday, run->mday),
               bit_is_set(nh_wday, run->wday),
               bit_is_set(nh_wom, run->wom),
               bit_is_set(nh_woy, run->woy));
         Dmsg2(000, "run->last_set:%d, is_last_week:%d\n", run->last_set, is_last_week);
         Dmsg2(000, "run->last_set:%d, nh_is_last_week:%d\n", run->last_set, nh_is_last_week);
#endif

         run_now = bit_is_set(hour, run->hour) &&
                   bit_is_set(mday, run->mday) &&
                   bit_is_set(wday, run->wday) &&
                   bit_is_set(month, run->month) &&
                  (bit_is_set(wom, run->wom) || (run->last_set && is_last_week)) &&
                   bit_is_set(woy, run->woy);

         run_nh = bit_is_set(nh_hour, run->hour) &&
                  bit_is_set(nh_mday, run->mday) &&
                  bit_is_set(nh_wday, run->wday) &&
                  bit_is_set(nh_month, run->month) &&
                 (bit_is_set(nh_wom, run->wom) || (run->last_set && nh_is_last_week)) &&
                  bit_is_set(nh_woy, run->woy);

         Dmsg3(dbglvl, "run@%p: run_now=%d run_nh=%d\n", run, run_now, run_nh);

         if (run_now || run_nh) {
           /*
            * find time (time_t) job is to be run
            */
           blocaltime(&now, &tm);      /* reset tm structure */
           tm.tm_min = run->minute;     /* set run minute */
           tm.tm_sec = 0;               /* zero secs */
           runtime = mktime(&tm);
           if (run_now) {
             add_job(job, run, now, runtime);
           }
           /* If job is to be run in the next hour schedule it */
           if (run_nh) {
             add_job(job, run, now, runtime + 3600);
           }
         }
      }
   }
   UnlockRes();
   Dmsg0(dbglvl, "Leave find_runs()\n");
}

static void add_job(JobResource *job, RunResource *run, time_t now, time_t runtime)
{
   job_item *ji;
   bool inserted = false;
   /*
    * Don't run any job that ran less than a minute ago, but
    *  do run any job scheduled less than a minute ago.
    */
   if (((runtime - run->last_run) < 61) || ((runtime+59) < now)) {
#ifdef SCHED_DEBUG
      Dmsg4(000, "Drop: Job=\"%s\" run=%lld. last_run=%lld. now=%lld\n", job->hdr.name,
            (utime_t)runtime, (utime_t)run->last_run, (utime_t)now);
      fflush(stdout);
#endif
      return;
   }
#ifdef SCHED_DEBUG
   Dmsg4(000, "Add: Job=\"%s\" run=%lld last_run=%lld now=%lld\n", job->hdr.name,
            (utime_t)runtime, (utime_t)run->last_run, (utime_t)now);
#endif
   /* accept to run this job */
   job_item *je = (job_item *)malloc(sizeof(job_item));
   je->run = run;
   je->job = job;
   je->runtime = runtime;
   if (run->Priority) {
      je->Priority = run->Priority;
   } else {
      je->Priority = job->Priority;
   }

   /* Add this job to the wait queue in runtime, priority sorted order */
   foreach_dlist(ji, jobs_to_run) {
      if (ji->runtime > je->runtime ||
          (ji->runtime == je->runtime && ji->Priority > je->Priority)) {
         jobs_to_run->insert_before(je, ji);
         dump_job(je, _("Inserted job"));
         inserted = true;
         break;
      }
   }
   /* If place not found in queue, append it */
   if (!inserted) {
      jobs_to_run->append(je);
      dump_job(je, _("Appended job"));
   }
#ifdef SCHED_DEBUG
   foreach_dlist(ji, jobs_to_run) {
      dump_job(ji, _("Run queue"));
   }
   Dmsg0(000, "End run queue\n");
#endif
}

static void dump_job(job_item *ji, const char *msg)
{
#ifdef SCHED_DEBUG
   char dt[MAX_TIME_LENGTH];
   int save_debug = debug_level;
   if (debug_level < dbglvl) {
      return;
   }
   bstrftime_nc(dt, sizeof(dt), ji->runtime);
   Dmsg4(dbglvl, "%s: Job=%s priority=%d run %s\n", msg, ji->job->hdr.name,
      ji->Priority, dt);
   fflush(stdout);
   debug_level = save_debug;
#endif
}
