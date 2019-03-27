/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2003-2011 Free Software Foundation Europe e.V.
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
 * Kern Sibbald, July MMIII
 *
 *
 * This code was adapted from the Bareos workq, which was
 * adapted from "Programming with POSIX Threads", by
 * David R. Butenhof
 */
/**
 * BAREOS job queue routines.
 *
 * This code consists of three queues, the waiting_jobs
 * queue, where jobs are initially queued, the ready_jobs
 * queue, where jobs are placed when all the resources are
 * allocated and they can immediately be run, and the
 * running queue where jobs are placed when they are
 * running.
 */

#include "include/bareos.h"
#include "dird.h"
#include "dird/job.h"
#include "dird/jobq.h"
#include "dird/storage.h"
#include "lib/berrno.h"

namespace directordaemon {

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

/* Forward referenced functions */
extern "C" void* jobq_server(void* arg);
extern "C" void* sched_wait(void* arg);

static int StartServer(jobq_t* jq);
static bool AcquireResources(JobControlRecord* jcr);
static bool RescheduleJob(JobControlRecord* jcr, jobq_t* jq, jobq_item_t* je);
static bool IncClientConcurrency(JobControlRecord* jcr);
static void DecClientConcurrency(JobControlRecord* jcr);
static bool IncJobConcurrency(JobControlRecord* jcr);
static void DecJobConcurrency(JobControlRecord* jcr);
static bool IncWriteStore(JobControlRecord* jcr);
static void DecWriteStore(JobControlRecord* jcr);

/*
 * Initialize a job queue
 *
 * Returns: 0 on success
 *          errno on failure
 */
int JobqInit(jobq_t* jq, int max_workers, void* (*engine)(void* arg))
{
  int status;
  jobq_item_t* item = NULL;

  if ((status = pthread_attr_init(&jq->attr)) != 0) {
    BErrNo be;
    Jmsg1(NULL, M_ERROR, 0, _("pthread_attr_init: ERR=%s\n"),
          be.bstrerror(status));
    return status;
  }
  if ((status = pthread_attr_setdetachstate(&jq->attr,
                                            PTHREAD_CREATE_DETACHED)) != 0) {
    pthread_attr_destroy(&jq->attr);
    return status;
  }
  if ((status = pthread_mutex_init(&jq->mutex, NULL)) != 0) {
    BErrNo be;
    Jmsg1(NULL, M_ERROR, 0, _("pthread_mutex_init: ERR=%s\n"),
          be.bstrerror(status));
    pthread_attr_destroy(&jq->attr);
    return status;
  }
  if ((status = pthread_cond_init(&jq->work, NULL)) != 0) {
    BErrNo be;
    Jmsg1(NULL, M_ERROR, 0, _("pthread_cond_init: ERR=%s\n"),
          be.bstrerror(status));
    pthread_mutex_destroy(&jq->mutex);
    pthread_attr_destroy(&jq->attr);
    return status;
  }
  jq->quit = false;
  jq->max_workers = max_workers; /* max threads to create */
  jq->num_workers = 0;           /* no threads yet */
  jq->engine = engine;           /* routine to run */
  jq->valid = JOBQ_VALID;

  /*
   * Initialize the job queues
   */
  jq->waiting_jobs = New(dlist(item, &item->link));
  jq->running_jobs = New(dlist(item, &item->link));
  jq->ready_jobs = New(dlist(item, &item->link));

  return 0;
}

/**
 * Destroy the job queue
 *
 * Returns: 0 on success
 *          errno on failure
 */
int JobqDestroy(jobq_t* jq)
{
  int status, status1, status2;

  if (jq->valid != JOBQ_VALID) { return EINVAL; }
  P(jq->mutex);
  jq->valid = 0; /* prevent any more operations */

  /*
   * If any threads are active, wake them
   */
  if (jq->num_workers > 0) {
    jq->quit = true;
    while (jq->num_workers > 0) {
      if ((status = pthread_cond_wait(&jq->work, &jq->mutex)) != 0) {
        BErrNo be;
        Jmsg1(NULL, M_ERROR, 0, _("pthread_cond_wait: ERR=%s\n"),
              be.bstrerror(status));
        V(jq->mutex);
        return status;
      }
    }
  }
  V(jq->mutex);
  status = pthread_mutex_destroy(&jq->mutex);
  status1 = pthread_cond_destroy(&jq->work);
  status2 = pthread_attr_destroy(&jq->attr);
  delete jq->waiting_jobs;
  delete jq->running_jobs;
  delete jq->ready_jobs;
  return (status != 0 ? status : (status1 != 0 ? status1 : status2));
}

struct wait_pkt {
  JobControlRecord* jcr;
  jobq_t* jq;
};

/**
 * Wait until schedule time arrives before starting. Normally
 * this routine is only used for jobs started from the console
 * for which the user explicitly specified a start time. Otherwise
 * most jobs are put into the job queue only when their
 * scheduled time arives.
 */
extern "C" void* sched_wait(void* arg)
{
  JobControlRecord* jcr = ((wait_pkt*)arg)->jcr;
  jobq_t* jq = ((wait_pkt*)arg)->jq;

  SetJcrInTsd(INVALID_JCR);
  Dmsg0(2300, "Enter sched_wait.\n");
  free(arg);
  time_t wtime = jcr->sched_time - time(NULL);
  jcr->setJobStatus(JS_WaitStartTime);

  /*
   * Wait until scheduled time arrives
   */
  if (wtime > 0) {
    Jmsg(jcr, M_INFO, 0,
         _("Job %s waiting %d seconds for scheduled start time.\n"), jcr->Job,
         wtime);
  }

  /*
   * Check every 30 seconds if canceled
   */
  while (wtime > 0) {
    Dmsg3(2300, "Waiting on sched time, jobid=%d secs=%d use=%d\n", jcr->JobId,
          wtime, jcr->UseCount());
    if (wtime > 30) { wtime = 30; }
    Bmicrosleep(wtime, 0);
    if (JobCanceled(jcr)) { break; }
    wtime = jcr->sched_time - time(NULL);
  }
  Dmsg1(200, "resched use=%d\n", jcr->UseCount());
  JobqAdd(jq, jcr);
  FreeJcr(jcr); /* we are done with jcr */
  Dmsg0(2300, "Exit sched_wait\n");

  return NULL;
}

/**
 * Add a job to the queue
 * jq is a queue that was created with jobq_init
 */
int JobqAdd(jobq_t* jq, JobControlRecord* jcr)
{
  int status;
  jobq_item_t *item, *li;
  bool inserted = false;
  time_t wtime = jcr->sched_time - time(NULL);
  pthread_t id;
  wait_pkt* sched_pkt;

  if (!jcr->term_wait_inited) {
    /*
     * Initialize termination condition variable
     */
    if ((status = pthread_cond_init(&jcr->term_wait, NULL)) != 0) {
      BErrNo be;
      Jmsg1(jcr, M_FATAL, 0, _("Unable to init job cond variable: ERR=%s\n"),
            be.bstrerror(status));
      return status;
    }
    jcr->term_wait_inited = true;
  }

  Dmsg3(2300, "JobqAdd jobid=%d jcr=0x%x UseCount=%d\n", jcr->JobId, jcr,
        jcr->UseCount());
  if (jq->valid != JOBQ_VALID) {
    Jmsg0(jcr, M_ERROR, 0, "Jobq_add queue not initialized.\n");
    return EINVAL;
  }

  jcr->IncUseCount(); /* mark jcr in use by us */
  Dmsg3(2300, "JobqAdd jobid=%d jcr=0x%x UseCount=%d\n", jcr->JobId, jcr,
        jcr->UseCount());
  if (!JobCanceled(jcr) && wtime > 0) {
    SetThreadConcurrency(jq->max_workers + 2);
    sched_pkt = (wait_pkt*)malloc(sizeof(wait_pkt));
    sched_pkt->jcr = jcr;
    sched_pkt->jq = jq;
    status = pthread_create(&id, &jq->attr, sched_wait, (void*)sched_pkt);
    if (status != 0) { /* thread not created */
      BErrNo be;
      Jmsg1(jcr, M_ERROR, 0, _("pthread_thread_create: ERR=%s\n"),
            be.bstrerror(status));
    }
    return status;
  }

  P(jq->mutex);

  if ((item = (jobq_item_t*)malloc(sizeof(jobq_item_t))) == NULL) {
    FreeJcr(jcr); /* release jcr */
    return ENOMEM;
  }
  item->jcr = jcr;

  /*
   * While waiting in a queue this job is not attached to a thread
   */
  SetJcrInTsd(INVALID_JCR);
  if (JobCanceled(jcr)) {
    /*
     * Add job to ready queue so that it is canceled quickly
     */
    jq->ready_jobs->prepend(item);
    Dmsg1(2300, "Prepended job=%d to ready queue\n", jcr->JobId);
  } else {
    /*
     * Add this job to the wait queue in priority sorted order
     */
    foreach_dlist (li, jq->waiting_jobs) {
      Dmsg2(2300, "waiting item jobid=%d priority=%d\n", li->jcr->JobId,
            li->jcr->JobPriority);
      if (li->jcr->JobPriority > jcr->JobPriority) {
        jq->waiting_jobs->InsertBefore(item, li);
        Dmsg2(2300, "InsertBefore jobid=%d before waiting job=%d\n",
              li->jcr->JobId, jcr->JobId);
        inserted = true;
        break;
      }
    }

    /*
     * If not jobs in wait queue, append it
     */
    if (!inserted) {
      jq->waiting_jobs->append(item);
      Dmsg1(2300, "Appended item jobid=%d to waiting queue\n", jcr->JobId);
    }
  }

  /*
   * Ensure that at least one server looks at the queue.
   */
  status = StartServer(jq);

  V(jq->mutex);
  Dmsg0(2300, "Return JobqAdd\n");
  return status;
}

/**
 * Remove a job from the job queue. Used only by CancelJob().
 * jq is a queue that was created with jobq_init
 * work_item is an element of work
 *
 * Note, it is "removed" from the job queue.
 * If you want to cancel it, you need to provide some external means
 * of doing so (e.g. pthread_kill()).
 */
int JobqRemove(jobq_t* jq, JobControlRecord* jcr)
{
  int status;
  bool found = false;
  jobq_item_t* item;

  Dmsg2(2300, "JobqRemove jobid=%d jcr=0x%x\n", jcr->JobId, jcr);
  if (jq->valid != JOBQ_VALID) { return EINVAL; }

  P(jq->mutex);
  foreach_dlist (item, jq->waiting_jobs) {
    if (jcr == item->jcr) {
      found = true;
      break;
    }
  }
  if (!found) {
    V(jq->mutex);
    Dmsg2(2300, "JobqRemove jobid=%d jcr=0x%x not in wait queue\n", jcr->JobId,
          jcr);
    return EINVAL;
  }

  /*
   * Move item to be the first on the list
   */
  jq->waiting_jobs->remove(item);
  jq->ready_jobs->prepend(item);
  Dmsg2(2300, "JobqRemove jobid=%d jcr=0x%x moved to ready queue\n", jcr->JobId,
        jcr);

  status = StartServer(jq);

  V(jq->mutex);
  Dmsg0(2300, "Return JobqRemove\n");
  return status;
}

/**
 * Start the server thread if it isn't already running
 */
static int StartServer(jobq_t* jq)
{
  int status = 0;
  pthread_t id;

  if (jq->num_workers < jq->max_workers) {
    Dmsg0(2300, "Create worker thread\n");
    SetThreadConcurrency(jq->max_workers + 1);
    if ((status = pthread_create(&id, &jq->attr, jobq_server, (void*)jq)) !=
        0) {
      BErrNo be;
      Jmsg1(NULL, M_ERROR, 0, _("pthread_create: ERR=%s\n"),
            be.bstrerror(status));
      return status;
    }
    jq->num_workers++;
  }
  return status;
}

/**
 * This is the worker thread that serves the job queue.
 * When all the resources are acquired for the job,
 * it will call the user's engine.
 */
extern "C" void* jobq_server(void* arg)
{
  struct timespec timeout;
  jobq_t* jq = (jobq_t*)arg;
  jobq_item_t* je; /* job entry in queue */
  int status;
  bool timedout = false;
  bool work = true;

  SetJcrInTsd(INVALID_JCR);
  Dmsg0(2300, "Start jobq_server\n");
  P(jq->mutex);

  for (;;) {
    struct timeval tv;
    struct timezone tz;

    Dmsg0(2300, "Top of for loop\n");
    if (!work && !jq->quit) {
      gettimeofday(&tv, &tz);
      timeout.tv_nsec = 0;
      timeout.tv_sec = tv.tv_sec + 4;

      while (!jq->quit) {
        /*
         * Wait 4 seconds, then if no more work, exit
         */
        Dmsg0(2300, "pthread_cond_timedwait()\n");
        status = pthread_cond_timedwait(&jq->work, &jq->mutex, &timeout);
        if (status == ETIMEDOUT) {
          Dmsg0(2300, "timedwait timedout.\n");
          timedout = true;
          break;
        } else if (status != 0) {
          /*
           * This shouldn't happen
           */
          Dmsg0(2300, "This shouldn't happen\n");
          jq->num_workers--;
          V(jq->mutex);
          return NULL;
        }
        break;
      }
    }

    /*
     * If anything is in the ready queue, run it
     */
    Dmsg0(2300, "Checking ready queue.\n");
    while (!jq->ready_jobs->empty() && !jq->quit) {
      JobControlRecord* jcr;

      je = (jobq_item_t*)jq->ready_jobs->first();
      jcr = je->jcr;
      jq->ready_jobs->remove(je);
      if (!jq->ready_jobs->empty()) {
        Dmsg0(2300, "ready queue not empty start server\n");
        if (StartServer(jq) != 0) {
          jq->num_workers--;
          V(jq->mutex);
          return NULL;
        }
      }
      jq->running_jobs->append(je);

      /*
       * Attach jcr to this thread while we run the job
       */
      jcr->SetKillable(true);
      SetJcrInTsd(jcr);
      Dmsg1(2300, "Took jobid=%d from ready and appended to run\n", jcr->JobId);

      /*
       * Release job queue lock
       */
      V(jq->mutex);

      /*
       * Call user's routine here
       */
      Dmsg3(2300, "Calling user engine for jobid=%d use=%d stat=%c\n",
            jcr->JobId, jcr->UseCount(), jcr->JobStatus);
      jq->engine(je->jcr);

      /*
       * Job finished detach from thread
       */
      RemoveJcrFromTsd(je->jcr);
      je->jcr->SetKillable(false);

      Dmsg2(2300, "Back from user engine jobid=%d use=%d.\n", jcr->JobId,
            jcr->UseCount());

      /*
       * Reacquire job queue lock
       */
      P(jq->mutex);
      Dmsg0(200, "Done lock mutex after running job. Release locks.\n");
      jq->running_jobs->remove(je);

      /*
       * Release locks if acquired. Note, they will not have
       * been acquired for jobs canceled before they were put into the ready
       * queue.
       */
      if (jcr->acquired_resource_locks) {
        DecReadStore(jcr);
        DecWriteStore(jcr);
        DecClientConcurrency(jcr);
        DecJobConcurrency(jcr);
        jcr->acquired_resource_locks = false;
      }

      if (RescheduleJob(jcr, jq, je)) { continue; /* go look for more work */ }

      /*
       * Clean up and release old jcr
       */
      Dmsg2(2300, "====== Termination job=%d use_cnt=%d\n", jcr->JobId,
            jcr->UseCount());
      jcr->SDJobStatus = 0;
      V(jq->mutex); /* release internal lock */
      FreeJcr(jcr);
      free(je);     /* release job entry */
      P(jq->mutex); /* reacquire job queue lock */
    }

    /*
     * If any job in the wait queue can be run, move it to the ready queue
     */
    Dmsg0(2300, "Done check ready, now check wait queue.\n");
    if (!jq->waiting_jobs->empty() && !jq->quit) {
      int Priority;
      bool running_allow_mix = false;
      je = (jobq_item_t*)jq->waiting_jobs->first();
      jobq_item_t* re = (jobq_item_t*)jq->running_jobs->first();
      if (re) {
        Priority = re->jcr->JobPriority;
        Dmsg2(2300, "JobId %d is running. Look for pri=%d\n", re->jcr->JobId,
              Priority);
        running_allow_mix = true;

        for (; re;) {
          Dmsg2(2300, "JobId %d is also running with %s\n", re->jcr->JobId,
                re->jcr->res.job->allow_mixed_priority ? "mix" : "no mix");
          if (!re->jcr->res.job->allow_mixed_priority) {
            running_allow_mix = false;
            break;
          }
          re = (jobq_item_t*)jq->running_jobs->next(re);
        }
        Dmsg1(2300, "The running job(s) %s mixing priorities.\n",
              running_allow_mix ? "allow" : "don't allow");
      } else {
        Priority = je->jcr->JobPriority;
        Dmsg1(2300, "No job running. Look for Job pri=%d\n", Priority);
      }

      /*
       * Walk down the list of waiting jobs and attempt to acquire the resources
       * it needs.
       */
      for (; je;) {
        /*
         * je is current job item on the queue, jn is the next one
         */
        JobControlRecord* jcr = je->jcr;
        jobq_item_t* jn = (jobq_item_t*)jq->waiting_jobs->next(je);

        Dmsg4(2300, "Examining Job=%d JobPri=%d want Pri=%d (%s)\n", jcr->JobId,
              jcr->JobPriority, Priority,
              jcr->res.job->allow_mixed_priority ? "mix" : "no mix");

        /*
         * Take only jobs of correct Priority
         */
        if (!(jcr->JobPriority == Priority ||
              (jcr->JobPriority < Priority &&
               jcr->res.job->allow_mixed_priority && running_allow_mix))) {
          jcr->setJobStatus(JS_WaitPriority);
          break;
        }

        if (!AcquireResources(jcr)) {
          /*
           * If resource conflict, job is canceled
           */
          if (!JobCanceled(jcr)) {
            je = jn; /* point to next waiting job */
            continue;
          }
        }

        /*
         * Got all locks, now remove it from wait queue and append it
         * to the ready queue.  Note, we may also get here if the
         * job was canceled.  Once it is "run", it will quickly Terminate.
         */
        jq->waiting_jobs->remove(je);
        jq->ready_jobs->append(je);
        Dmsg1(2300, "moved JobId=%d from wait to ready queue\n",
              je->jcr->JobId);
        je = jn; /* Point to next waiting job */
      }          /* end for loop */
    }            /* end if */

    Dmsg0(2300, "Done checking wait queue.\n");

    /*
     * If no more ready work and we are asked to quit, then do it
     */
    if (jq->ready_jobs->empty() && jq->quit) {
      jq->num_workers--;
      if (jq->num_workers == 0) {
        Dmsg0(2300, "Wake up destroy routine\n");

        /*
         * Wake up destroy routine if he is waiting
         */
        pthread_cond_broadcast(&jq->work);
      }
      break;
    }

    Dmsg0(2300, "Check for work request\n");

    /*
     * If no more work requests, and we waited long enough, quit
     */
    Dmsg2(2300, "timedout=%d read empty=%d\n", timedout,
          jq->ready_jobs->empty());

    if (jq->ready_jobs->empty() && timedout) {
      Dmsg0(2300, "break big loop\n");
      jq->num_workers--;
      break;
    }

    work = !jq->ready_jobs->empty() || !jq->waiting_jobs->empty();
    if (work) {
      /*
       * If a job is waiting on a Resource, don't consume all
       * the CPU time looping looking for work, and even more
       * important, release the lock so that a job that has
       * terminated can give us the resource.
       */
      V(jq->mutex);
      Bmicrosleep(2, 0); /* pause for 2 seconds */
      P(jq->mutex);

      /*
       * Recompute work as something may have changed in last 2 secs
       */
      work = !jq->ready_jobs->empty() || !jq->waiting_jobs->empty();
    }
    Dmsg1(2300, "Loop again. work=%d\n", work);
  } /* end of big for loop */

  Dmsg0(200, "unlock mutex\n");
  V(jq->mutex);
  Dmsg0(2300, "End jobq_server\n");

  return NULL;
}

/**
 * Returns true if cleanup done and we should look for more work
 */
static bool RescheduleJob(JobControlRecord* jcr, jobq_t* jq, jobq_item_t* je)
{
  bool resched = false, retval = false;

  /*
   * Reschedule the job if requested and possible
   */

  /*
   * Basic condition is that more reschedule times remain
   */
  if (jcr->res.job->RescheduleTimes == 0 ||
      jcr->reschedule_count < jcr->res.job->RescheduleTimes) {
    resched =
        /*
         * Check for incomplete jobs
         */
        (jcr->res.job->RescheduleIncompleteJobs && jcr->IsIncomplete() &&
         jcr->is_JobType(JT_BACKUP) && !jcr->is_JobLevel(L_BASE)) ||
        /*
         * Check for failed jobs
         */
        (jcr->res.job->RescheduleOnError && !jcr->IsTerminatedOk() &&
         !jcr->is_JobStatus(JS_Canceled) && jcr->is_JobType(JT_BACKUP));
  }

  if (resched) {
    char dt[50], dt2[50];
    time_t now;

    /*
     * Reschedule this job by cleaning it up, but reuse the same JobId if
     * possible.
     */
    now = time(NULL);
    jcr->reschedule_count++;
    jcr->sched_time = now + jcr->res.job->RescheduleInterval;
    bstrftime(dt, sizeof(dt), now);
    bstrftime(dt2, sizeof(dt2), jcr->sched_time);
    Dmsg4(2300, "Rescheduled Job %s to re-run in %d seconds.(now=%u,then=%u)\n",
          jcr->Job, (int)jcr->res.job->RescheduleInterval, now,
          jcr->sched_time);
    Jmsg(jcr, M_INFO, 0,
         _("Rescheduled Job %s at %s to re-run in %d seconds (%s).\n"),
         jcr->Job, dt, (int)jcr->res.job->RescheduleInterval, dt2);
    DirdFreeJcrPointers(jcr); /* partial cleanup old stuff */
    jcr->JobStatus = -1;
    jcr->SDJobStatus = 0;
    jcr->JobErrors = 0;
    if (!AllowDuplicateJob(jcr)) { return false; }

    /*
     * Only jobs with no output or Incomplete jobs can run on same
     * JobControlRecord
     */
    if (jcr->JobBytes == 0) {
      UpdateJobEnd(jcr, JS_WaitStartTime);
      Dmsg2(2300, "Requeue job=%d use=%d\n", jcr->JobId, jcr->UseCount());
      V(jq->mutex);
      jcr->jr.RealEndTime = 0;
      JobqAdd(jq, jcr); /* queue the job to run again */
      P(jq->mutex);
      FreeJcr(jcr);  /* release jcr */
      free(je);      /* free the job entry */
      retval = true; /* we already cleaned up */
    } else {
      JobControlRecord* njcr;

      /*
       * Something was actually backed up, so we cannot reuse
       * the old JobId or there will be database record
       * conflicts.  We now create a new job, copying the
       * appropriate fields.
       */
      jcr->setJobStatus(JS_WaitStartTime);
      njcr = new_jcr(sizeof(JobControlRecord), DirdFreeJcr);
      SetJcrDefaults(njcr, jcr->res.job);
      njcr->reschedule_count = jcr->reschedule_count;
      njcr->sched_time = jcr->sched_time;
      njcr->initial_sched_time = jcr->initial_sched_time;

      njcr->setJobLevel(jcr->getJobLevel());
      njcr->res.pool = jcr->res.pool;
      njcr->res.run_pool_override = jcr->res.run_pool_override;
      njcr->res.full_pool = jcr->res.full_pool;
      njcr->res.run_full_pool_override = jcr->res.run_full_pool_override;
      njcr->res.inc_pool = jcr->res.inc_pool;
      njcr->res.run_inc_pool_override = jcr->res.run_inc_pool_override;
      njcr->res.diff_pool = jcr->res.diff_pool;
      njcr->res.run_diff_pool_override = jcr->res.run_diff_pool_override;
      njcr->res.next_pool = jcr->res.next_pool;
      njcr->res.run_next_pool_override = jcr->res.run_next_pool_override;
      njcr->JobStatus = -1;
      njcr->setJobStatus(jcr->JobStatus);
      if (jcr->res.read_storage) {
        CopyRstorage(njcr, jcr->res.read_storage_list, _("previous Job"));
      } else {
        FreeRstorage(njcr);
      }
      if (jcr->res.write_storage) {
        CopyWstorage(njcr, jcr->res.write_storage_list, _("previous Job"));
      } else {
        FreeWstorage(njcr);
      }
      njcr->res.messages = jcr->res.messages;
      njcr->spool_data = jcr->spool_data;
      Dmsg0(2300, "Call to run new job\n");
      V(jq->mutex);
      RunJob(njcr);  /* This creates a "new" job */
      FreeJcr(njcr); /* release "new" jcr */
      P(jq->mutex);
      Dmsg0(2300, "Back from running new job.\n");
    }
  }

  return retval;
}

/**
 * See if we can acquire all the necessary resources for the job
 * (JobControlRecord)
 *
 *  Returns: true  if successful
 *           false if resource failure
 */
static bool AcquireResources(JobControlRecord* jcr)
{
  /*
   * Set that we didn't acquire any resourse locks yet.
   */
  jcr->acquired_resource_locks = false;

  /*
   * Some Job Types are excluded from the client and storage concurrency
   * as they have no interaction with the client or storage at all.
   */
  switch (jcr->getJobType()) {
    case JT_MIGRATE:
    case JT_COPY:
    case JT_CONSOLIDATE:
      /*
       * Migration/Copy and Consolidation jobs are not counted for client
       * concurrency as they do not touch the client at all
       */
      jcr->IgnoreClientConcurrency = true;
      Dmsg1(200, "Skipping migrate/copy Job %s for client concurrency\n",
            jcr->Job);

      if (jcr->MigrateJobId == 0) {
        /*
         * Migration/Copy control jobs are not counted for storage concurrency
         * as they do not touch the storage at all
         */
        Dmsg1(200,
              "Skipping migrate/copy Control Job %s for storage concurrency\n",
              jcr->Job);
        jcr->IgnoreStorageConcurrency = true;
      }
      break;
    default:
      break;
  }

  if (jcr->res.read_storage) {
    if (!IncReadStore(jcr)) {
      jcr->setJobStatus(JS_WaitStoreRes);

      return false;
    }
  }

  if (jcr->res.write_storage) {
    if (!IncWriteStore(jcr)) {
      DecReadStore(jcr);
      jcr->setJobStatus(JS_WaitStoreRes);

      return false;
    }
  }

  if (!IncClientConcurrency(jcr)) {
    /*
     * Back out previous locks
     */
    DecWriteStore(jcr);
    DecReadStore(jcr);
    jcr->setJobStatus(JS_WaitClientRes);

    return false;
  }

  if (!IncJobConcurrency(jcr)) {
    /*
     * Back out previous locks
     */
    DecWriteStore(jcr);
    DecReadStore(jcr);
    DecClientConcurrency(jcr);
    jcr->setJobStatus(JS_WaitJobRes);

    return false;
  }

  jcr->acquired_resource_locks = true;

  return true;
}

static bool IncClientConcurrency(JobControlRecord* jcr)
{
  if (!jcr->res.client || jcr->IgnoreClientConcurrency) { return true; }

  P(mutex);
  if (jcr->res.client->rcs->NumConcurrentJobs <
      jcr->res.client->MaxConcurrentJobs) {
    jcr->res.client->rcs->NumConcurrentJobs++;
    Dmsg2(50, "Inc Client=%s rncj=%d\n", jcr->res.client->resource_name_,
          jcr->res.client->rcs->NumConcurrentJobs);
    V(mutex);

    return true;
  }

  V(mutex);

  return false;
}

static void DecClientConcurrency(JobControlRecord* jcr)
{
  if (jcr->IgnoreClientConcurrency) { return; }

  P(mutex);
  if (jcr->res.client) {
    jcr->res.client->rcs->NumConcurrentJobs--;
    Dmsg2(50, "Dec Client=%s rncj=%d\n", jcr->res.client->resource_name_,
          jcr->res.client->rcs->NumConcurrentJobs);
  }
  V(mutex);
}

static bool IncJobConcurrency(JobControlRecord* jcr)
{
  P(mutex);
  if (jcr->res.job->rjs->NumConcurrentJobs < jcr->res.job->MaxConcurrentJobs) {
    jcr->res.job->rjs->NumConcurrentJobs++;
    Dmsg2(50, "Inc Job=%s rncj=%d\n", jcr->res.job->resource_name_,
          jcr->res.job->rjs->NumConcurrentJobs);
    V(mutex);

    return true;
  }

  V(mutex);

  return false;
}

static void DecJobConcurrency(JobControlRecord* jcr)
{
  P(mutex);
  jcr->res.job->rjs->NumConcurrentJobs--;
  Dmsg2(50, "Dec Job=%s rncj=%d\n", jcr->res.job->resource_name_,
        jcr->res.job->rjs->NumConcurrentJobs);
  V(mutex);
}

/**
 * Note: IncReadStore() and DecReadStore() are
 * called from SelectNextRstore() in src/dird/job.c
 */
bool IncReadStore(JobControlRecord* jcr)
{
  if (jcr->IgnoreStorageConcurrency) { return true; }

  P(mutex);
  if (jcr->res.read_storage->runtime_storage_status->NumConcurrentJobs <
      jcr->res.read_storage->MaxConcurrentJobs) {
    jcr->res.read_storage->runtime_storage_status->NumConcurrentReadJobs++;
    jcr->res.read_storage->runtime_storage_status->NumConcurrentJobs++;
    Dmsg2(50, "Inc Rstore=%s rncj=%d\n", jcr->res.read_storage->resource_name_,
          jcr->res.read_storage->runtime_storage_status->NumConcurrentJobs);
    V(mutex);

    return true;
  }
  V(mutex);

  Dmsg2(50, "Fail to acquire Rstore=%s rncj=%d\n",
        jcr->res.read_storage->resource_name_,
        jcr->res.read_storage->runtime_storage_status->NumConcurrentJobs);

  return false;
}

void DecReadStore(JobControlRecord* jcr)
{
  if (jcr->res.read_storage && !jcr->IgnoreStorageConcurrency) {
    P(mutex);
    jcr->res.read_storage->runtime_storage_status->NumConcurrentReadJobs--;
    jcr->res.read_storage->runtime_storage_status->NumConcurrentJobs--;
    Dmsg2(50, "Dec Rstore=%s rncj=%d\n", jcr->res.read_storage->resource_name_,
          jcr->res.read_storage->runtime_storage_status->NumConcurrentJobs);

    if (jcr->res.read_storage->runtime_storage_status->NumConcurrentReadJobs < 0) {
      Jmsg(jcr, M_FATAL, 0, _("NumConcurrentReadJobs Dec Rstore=%s rncj=%d\n"),
           jcr->res.read_storage->resource_name_,
           jcr->res.read_storage->runtime_storage_status->NumConcurrentReadJobs);
    }

    if (jcr->res.read_storage->runtime_storage_status->NumConcurrentJobs < 0) {
      Jmsg(jcr, M_FATAL, 0, _("NumConcurrentJobs Dec Rstore=%s rncj=%d\n"),
           jcr->res.read_storage->resource_name_,
           jcr->res.read_storage->runtime_storage_status->NumConcurrentJobs);
    }
    V(mutex);
  }
}

static bool IncWriteStore(JobControlRecord* jcr)
{
  if (jcr->IgnoreStorageConcurrency) { return true; }

  P(mutex);
  if (jcr->res.write_storage->runtime_storage_status->NumConcurrentJobs <
      jcr->res.write_storage->MaxConcurrentJobs) {
    jcr->res.write_storage->runtime_storage_status->NumConcurrentJobs++;
    Dmsg2(50, "Inc Wstore=%s wncj=%d\n", jcr->res.write_storage->resource_name_,
          jcr->res.write_storage->runtime_storage_status->NumConcurrentJobs);
    V(mutex);

    return true;
  }
  V(mutex);

  Dmsg2(50, "Fail to acquire Wstore=%s wncj=%d\n",
        jcr->res.write_storage->resource_name_,
        jcr->res.write_storage->runtime_storage_status->NumConcurrentJobs);

  return false;
}

static void DecWriteStore(JobControlRecord* jcr)
{
  if (jcr->res.write_storage && !jcr->IgnoreStorageConcurrency) {
    P(mutex);
    jcr->res.write_storage->runtime_storage_status->NumConcurrentJobs--;
    Dmsg2(50, "Dec Wstore=%s wncj=%d\n", jcr->res.write_storage->resource_name_,
          jcr->res.write_storage->runtime_storage_status->NumConcurrentJobs);

    if (jcr->res.write_storage->runtime_storage_status->NumConcurrentJobs < 0) {
      Jmsg(jcr, M_FATAL, 0, _("NumConcurrentJobs Dec Wstore=%s wncj=%d\n"),
           jcr->res.write_storage->resource_name_,
           jcr->res.write_storage->runtime_storage_status->NumConcurrentJobs);
    }
    V(mutex);
  }
}
} /* namespace directordaemon */
