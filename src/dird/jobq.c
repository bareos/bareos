/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2003-2011 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from
   many others, a complete list can be found in the file AUTHORS.
   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.

   Bacula® is a registered trademark of Kern Sibbald.
   The licensor of Bacula is the Free Software Foundation Europe
   (FSFE), Fiduciary Program, Sumatrastrasse 25, 8006 Zürich,
   Switzerland, email:ftf@fsfeurope.org.
*/
/*
 * Bacula job queue routines.
 *
 *  This code consists of three queues, the waiting_jobs
 *  queue, where jobs are initially queued, the ready_jobs
 *  queue, where jobs are placed when all the resources are
 *  allocated and they can immediately be run, and the
 *  running queue where jobs are placed when they are
 *  running.
 *
 *  Kern Sibbald, July MMIII
 *
 *
 *  This code was adapted from the Bacula workq, which was
 *    adapted from "Programming with POSIX Threads", by
 *    David R. Butenhof
 *
 */

#include "bacula.h"
#include "dird.h"

extern JCR *jobs;

/* Forward referenced functions */
extern "C" void *jobq_server(void *arg);
extern "C" void *sched_wait(void *arg);

static int  start_server(jobq_t *jq);
static bool acquire_resources(JCR *jcr);
static bool reschedule_job(JCR *jcr, jobq_t *jq, jobq_item_t *je);
static void dec_write_store(JCR *jcr);

/*
 * Initialize a job queue
 *
 *  Returns: 0 on success
 *           errno on failure
 */
int jobq_init(jobq_t *jq, int threads, void *(*engine)(void *arg))
{
   int stat;
   jobq_item_t *item = NULL;

   if ((stat = pthread_attr_init(&jq->attr)) != 0) {
      berrno be;
      Jmsg1(NULL, M_ERROR, 0, _("pthread_attr_init: ERR=%s\n"), be.bstrerror(stat));
      return stat;
   }
   if ((stat = pthread_attr_setdetachstate(&jq->attr, PTHREAD_CREATE_DETACHED)) != 0) {
      pthread_attr_destroy(&jq->attr);
      return stat;
   }
   if ((stat = pthread_mutex_init(&jq->mutex, NULL)) != 0) {
      berrno be;
      Jmsg1(NULL, M_ERROR, 0, _("pthread_mutex_init: ERR=%s\n"), be.bstrerror(stat));
      pthread_attr_destroy(&jq->attr);
      return stat;
   }
   if ((stat = pthread_cond_init(&jq->work, NULL)) != 0) {
      berrno be;
      Jmsg1(NULL, M_ERROR, 0, _("pthread_cond_init: ERR=%s\n"), be.bstrerror(stat));
      pthread_mutex_destroy(&jq->mutex);
      pthread_attr_destroy(&jq->attr);
      return stat;
   }
   jq->quit = false;
   jq->max_workers = threads;         /* max threads to create */
   jq->num_workers = 0;               /* no threads yet */
   jq->idle_workers = 0;              /* no idle threads */
   jq->engine = engine;               /* routine to run */
   jq->valid = JOBQ_VALID;
   /* Initialize the job queues */
   jq->waiting_jobs = New(dlist(item, &item->link));
   jq->running_jobs = New(dlist(item, &item->link));
   jq->ready_jobs = New(dlist(item, &item->link));
   return 0;
}

/*
 * Destroy the job queue
 *
 * Returns: 0 on success
 *          errno on failure
 */
int jobq_destroy(jobq_t *jq)
{
   int stat, stat1, stat2;

   if (jq->valid != JOBQ_VALID) {
      return EINVAL;
   }
   P(jq->mutex);
   jq->valid = 0;                      /* prevent any more operations */

   /* 
    * If any threads are active, wake them 
    */
   if (jq->num_workers > 0) {
      jq->quit = true;
      if (jq->idle_workers) {
         if ((stat = pthread_cond_broadcast(&jq->work)) != 0) {
            berrno be;
            Jmsg1(NULL, M_ERROR, 0, _("pthread_cond_broadcast: ERR=%s\n"), be.bstrerror(stat));
            V(jq->mutex);
            return stat;
         }
      }
      while (jq->num_workers > 0) {
         if ((stat = pthread_cond_wait(&jq->work, &jq->mutex)) != 0) {
            berrno be;
            Jmsg1(NULL, M_ERROR, 0, _("pthread_cond_wait: ERR=%s\n"), be.bstrerror(stat));
            V(jq->mutex);
            return stat;
         }
      }
   }
   V(jq->mutex);
   stat  = pthread_mutex_destroy(&jq->mutex);
   stat1 = pthread_cond_destroy(&jq->work);
   stat2 = pthread_attr_destroy(&jq->attr);
   delete jq->waiting_jobs;
   delete jq->running_jobs;
   delete jq->ready_jobs;
   return (stat != 0 ? stat : (stat1 != 0 ? stat1 : stat2));
}

struct wait_pkt {
   JCR *jcr;
   jobq_t *jq;
};

/*
 * Wait until schedule time arrives before starting. Normally
 *  this routine is only used for jobs started from the console
 *  for which the user explicitly specified a start time. Otherwise
 *  most jobs are put into the job queue only when their
 *  scheduled time arives.
 */
extern "C"
void *sched_wait(void *arg)
{
   JCR *jcr = ((wait_pkt *)arg)->jcr;
   jobq_t *jq = ((wait_pkt *)arg)->jq;

   set_jcr_in_tsd(INVALID_JCR);
   Dmsg0(2300, "Enter sched_wait.\n");
   free(arg);
   time_t wtime = jcr->sched_time - time(NULL);
   jcr->setJobStatus(JS_WaitStartTime);
   /* Wait until scheduled time arrives */
   if (wtime > 0) {
      Jmsg(jcr, M_INFO, 0, _("Job %s waiting %d seconds for scheduled start time.\n"),
         jcr->Job, wtime);
   }
   /* Check every 30 seconds if canceled */
   while (wtime > 0) {
      Dmsg3(2300, "Waiting on sched time, jobid=%d secs=%d use=%d\n", 
         jcr->JobId, wtime, jcr->use_count());
      if (wtime > 30) {
         wtime = 30;
      }
      bmicrosleep(wtime, 0);
      if (job_canceled(jcr)) {
         break;
      }
      wtime = jcr->sched_time - time(NULL);
   }
   Dmsg1(200, "resched use=%d\n", jcr->use_count());
   jobq_add(jq, jcr);
   free_jcr(jcr);                     /* we are done with jcr */
   Dmsg0(2300, "Exit sched_wait\n");
   return NULL;
}

/*
 *  Add a job to the queue
 *    jq is a queue that was created with jobq_init
 */
int jobq_add(jobq_t *jq, JCR *jcr)
{
   int stat;
   jobq_item_t *item, *li;
   bool inserted = false;
   time_t wtime = jcr->sched_time - time(NULL);
   pthread_t id;
   wait_pkt *sched_pkt;

   if (!jcr->term_wait_inited) { 
      /* Initialize termination condition variable */
      if ((stat = pthread_cond_init(&jcr->term_wait, NULL)) != 0) {
         berrno be;
         Jmsg1(jcr, M_FATAL, 0, _("Unable to init job cond variable: ERR=%s\n"), be.bstrerror(stat));
         return stat;
      }
      jcr->term_wait_inited = true;
   }                           
                             
   Dmsg3(2300, "jobq_add jobid=%d jcr=0x%x use_count=%d\n", jcr->JobId, jcr, jcr->use_count());
   if (jq->valid != JOBQ_VALID) {
      Jmsg0(jcr, M_ERROR, 0, "Jobq_add queue not initialized.\n");
      return EINVAL;
   }

   jcr->inc_use_count();                 /* mark jcr in use by us */
   Dmsg3(2300, "jobq_add jobid=%d jcr=0x%x use_count=%d\n", jcr->JobId, jcr, jcr->use_count());
   if (!job_canceled(jcr) && wtime > 0) {
      set_thread_concurrency(jq->max_workers + 2);
      sched_pkt = (wait_pkt *)malloc(sizeof(wait_pkt));
      sched_pkt->jcr = jcr;
      sched_pkt->jq = jq;
      stat = pthread_create(&id, &jq->attr, sched_wait, (void *)sched_pkt);        
      if (stat != 0) {                /* thread not created */
         berrno be;
         Jmsg1(jcr, M_ERROR, 0, _("pthread_thread_create: ERR=%s\n"), be.bstrerror(stat));
      }
      return stat;
   }

   P(jq->mutex);

   if ((item = (jobq_item_t *)malloc(sizeof(jobq_item_t))) == NULL) {
      free_jcr(jcr);                    /* release jcr */
      return ENOMEM;
   }
   item->jcr = jcr;

   /* While waiting in a queue this job is not attached to a thread */
   set_jcr_in_tsd(INVALID_JCR);
   if (job_canceled(jcr)) {
      /* Add job to ready queue so that it is canceled quickly */
      jq->ready_jobs->prepend(item);
      Dmsg1(2300, "Prepended job=%d to ready queue\n", jcr->JobId);
   } else {
      /* Add this job to the wait queue in priority sorted order */
      foreach_dlist(li, jq->waiting_jobs) {
         Dmsg2(2300, "waiting item jobid=%d priority=%d\n",
            li->jcr->JobId, li->jcr->JobPriority);
         if (li->jcr->JobPriority > jcr->JobPriority) {
            jq->waiting_jobs->insert_before(item, li);
            Dmsg2(2300, "insert_before jobid=%d before waiting job=%d\n",
               li->jcr->JobId, jcr->JobId);
            inserted = true;
            break;
         }
      }
      /* If not jobs in wait queue, append it */
      if (!inserted) {
         jq->waiting_jobs->append(item);
         Dmsg1(2300, "Appended item jobid=%d to waiting queue\n", jcr->JobId);
      }
   }

   /* Ensure that at least one server looks at the queue. */
   stat = start_server(jq);

   V(jq->mutex);
   Dmsg0(2300, "Return jobq_add\n");
   return stat;
}

/*
 *  Remove a job from the job queue. Used only by cancel_job().
 *    jq is a queue that was created with jobq_init
 *    work_item is an element of work
 *
 *   Note, it is "removed" from the job queue.
 *    If you want to cancel it, you need to provide some external means
 *    of doing so (e.g. pthread_kill()).
 */
int jobq_remove(jobq_t *jq, JCR *jcr)
{
   int stat;
   bool found = false;
   jobq_item_t *item;

   Dmsg2(2300, "jobq_remove jobid=%d jcr=0x%x\n", jcr->JobId, jcr);
   if (jq->valid != JOBQ_VALID) {
      return EINVAL;
   }

   P(jq->mutex);
   foreach_dlist(item, jq->waiting_jobs) {
      if (jcr == item->jcr) {
         found = true;
         break;
      }
   }
   if (!found) {
      V(jq->mutex);
      Dmsg2(2300, "jobq_remove jobid=%d jcr=0x%x not in wait queue\n", jcr->JobId, jcr);
      return EINVAL;
   }

   /* Move item to be the first on the list */
   jq->waiting_jobs->remove(item);
   jq->ready_jobs->prepend(item);
   Dmsg2(2300, "jobq_remove jobid=%d jcr=0x%x moved to ready queue\n", jcr->JobId, jcr);

   stat = start_server(jq);

   V(jq->mutex);
   Dmsg0(2300, "Return jobq_remove\n");
   return stat;
}


/*
 * Start the server thread if it isn't already running
 */
static int start_server(jobq_t *jq)
{
   int stat = 0;
   pthread_t id;

   /*
    * if any threads are idle, wake one.
    *   Actually we do a broadcast because on /lib/tls 
    *   these signals seem to get lost from time to time.
    */
   if (jq->idle_workers > 0) {
      Dmsg0(2300, "Signal worker to wake up\n");
      if ((stat = pthread_cond_broadcast(&jq->work)) != 0) {
         berrno be;
         Jmsg1(NULL, M_ERROR, 0, _("pthread_cond_signal: ERR=%s\n"), be.bstrerror(stat));
         return stat;
      }
   } else if (jq->num_workers < jq->max_workers) {
      Dmsg0(2300, "Create worker thread\n");
      /* No idle threads so create a new one */
      set_thread_concurrency(jq->max_workers + 1);
      jq->num_workers++;
      if ((stat = pthread_create(&id, &jq->attr, jobq_server, (void *)jq)) != 0) {
         berrno be;
         jq->num_workers--;
         Jmsg1(NULL, M_ERROR, 0, _("pthread_create: ERR=%s\n"), be.bstrerror(stat));
         return stat;
      }
   }
   return stat;
}


/*
 * This is the worker thread that serves the job queue.
 * When all the resources are acquired for the job,
 *  it will call the user's engine.
 */
extern "C"
void *jobq_server(void *arg)
{
   struct timespec timeout;
   jobq_t *jq = (jobq_t *)arg;
   jobq_item_t *je;                   /* job entry in queue */
   int stat;
   bool timedout = false;
   bool work = true;

   set_jcr_in_tsd(INVALID_JCR);
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
            stat = pthread_cond_timedwait(&jq->work, &jq->mutex, &timeout);
            if (stat == ETIMEDOUT) {
               Dmsg0(2300, "timedwait timedout.\n");
               timedout = true;
               break;
            } else if (stat != 0) {
               /* This shouldn't happen */
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
         JCR *jcr;
         je = (jobq_item_t *)jq->ready_jobs->first();
         jcr = je->jcr;
         jq->ready_jobs->remove(je);
         if (!jq->ready_jobs->empty()) {
            Dmsg0(2300, "ready queue not empty start server\n");
            if (start_server(jq) != 0) {
               jq->num_workers--;
               V(jq->mutex);
               return NULL;
            }
         }
         jq->running_jobs->append(je);

         /* Attach jcr to this thread while we run the job */
         jcr->set_killable(true);
         set_jcr_in_tsd(jcr);
         Dmsg1(2300, "Took jobid=%d from ready and appended to run\n", jcr->JobId);

         /* Release job queue lock */
         V(jq->mutex);

         /* Call user's routine here */
         Dmsg3(2300, "Calling user engine for jobid=%d use=%d stat=%c\n", jcr->JobId,
            jcr->use_count(), jcr->JobStatus);
         jq->engine(je->jcr);

         /* Job finished detach from thread */
         remove_jcr_from_tsd(je->jcr);
         je->jcr->set_killable(false);

         Dmsg2(2300, "Back from user engine jobid=%d use=%d.\n", jcr->JobId,
            jcr->use_count());

         /* Reacquire job queue lock */
         P(jq->mutex);
         Dmsg0(200, "Done lock mutex after running job. Release locks.\n");
         jq->running_jobs->remove(je);
         /*
          * Release locks if acquired. Note, they will not have
          *  been acquired for jobs canceled before they were
          *  put into the ready queue.
          */
         if (jcr->acquired_resource_locks) {
            dec_read_store(jcr);
            dec_write_store(jcr);
            jcr->client->NumConcurrentJobs--;
            jcr->job->NumConcurrentJobs--;
            jcr->acquired_resource_locks = false;
         }

         if (reschedule_job(jcr, jq, je)) {
            continue;              /* go look for more work */
         }

         /* Clean up and release old jcr */
         Dmsg2(2300, "====== Termination job=%d use_cnt=%d\n", jcr->JobId, jcr->use_count());
         jcr->SDJobStatus = 0;
         V(jq->mutex);                /* release internal lock */
         free_jcr(jcr);
         free(je);                    /* release job entry */
         P(jq->mutex);                /* reacquire job queue lock */
      }
      /*
       * If any job in the wait queue can be run,
       *  move it to the ready queue
       */
      Dmsg0(2300, "Done check ready, now check wait queue.\n");
      if (!jq->waiting_jobs->empty() && !jq->quit) {
         int Priority;
         bool running_allow_mix = false;
         je = (jobq_item_t *)jq->waiting_jobs->first();
         jobq_item_t *re = (jobq_item_t *)jq->running_jobs->first();
         if (re) {
            Priority = re->jcr->JobPriority;
            Dmsg2(2300, "JobId %d is running. Look for pri=%d\n",
                  re->jcr->JobId, Priority);
            running_allow_mix = true;
            for ( ; re; ) {
               Dmsg2(2300, "JobId %d is also running with %s\n",
                     re->jcr->JobId, 
                     re->jcr->job->allow_mixed_priority ? "mix" : "no mix");
               if (!re->jcr->job->allow_mixed_priority) {
                  running_allow_mix = false;
                  break;
               }
               re = (jobq_item_t *)jq->running_jobs->next(re);
            }
            Dmsg1(2300, "The running job(s) %s mixing priorities.\n",
                  running_allow_mix ? "allow" : "don't allow");
         } else {
            Priority = je->jcr->JobPriority;
            Dmsg1(2300, "No job running. Look for Job pri=%d\n", Priority);
         }
         /*
          * Walk down the list of waiting jobs and attempt
          *   to acquire the resources it needs.
          */
         for ( ; je;  ) {
            /* je is current job item on the queue, jn is the next one */
            JCR *jcr = je->jcr;
            jobq_item_t *jn = (jobq_item_t *)jq->waiting_jobs->next(je);

            Dmsg4(2300, "Examining Job=%d JobPri=%d want Pri=%d (%s)\n",
                  jcr->JobId, jcr->JobPriority, Priority,
                  jcr->job->allow_mixed_priority ? "mix" : "no mix");

            /* Take only jobs of correct Priority */
            if (!(jcr->JobPriority == Priority
                  || (jcr->JobPriority < Priority &&
                      jcr->job->allow_mixed_priority && running_allow_mix))) {
               jcr->setJobStatus(JS_WaitPriority);
               break;
            }

            if (!acquire_resources(jcr)) {
               /* If resource conflict, job is canceled */
               if (!job_canceled(jcr)) {
                  je = jn;            /* point to next waiting job */
                  continue;
               }
            }

            /*
             * Got all locks, now remove it from wait queue and append it
             *   to the ready queue.  Note, we may also get here if the
             *    job was canceled.  Once it is "run", it will quickly
             *    terminate.
             */
            jq->waiting_jobs->remove(je);
            jq->ready_jobs->append(je);
            Dmsg1(2300, "moved JobId=%d from wait to ready queue\n", je->jcr->JobId);
            je = jn;                  /* Point to next waiting job */
         } /* end for loop */

      } /* end if */

      Dmsg0(2300, "Done checking wait queue.\n");
      /*
       * If no more ready work and we are asked to quit, then do it
       */
      if (jq->ready_jobs->empty() && jq->quit) {
         jq->num_workers--;
         if (jq->num_workers == 0) {
            Dmsg0(2300, "Wake up destroy routine\n");
            /* Wake up destroy routine if he is waiting */
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
          *   the CPU time looping looking for work, and even more
          *   important, release the lock so that a job that has
          *   terminated can give us the resource.
          */
         V(jq->mutex);
         bmicrosleep(2, 0);              /* pause for 2 seconds */
         P(jq->mutex);
         /* Recompute work as something may have changed in last 2 secs */
         work = !jq->ready_jobs->empty() || !jq->waiting_jobs->empty();
      }
      Dmsg1(2300, "Loop again. work=%d\n", work);
   } /* end of big for loop */

   Dmsg0(200, "unlock mutex\n");
   V(jq->mutex);
   Dmsg0(2300, "End jobq_server\n");
   return NULL;
}

/*
 * Returns true if cleanup done and we should look for more work
 */
static bool reschedule_job(JCR *jcr, jobq_t *jq, jobq_item_t *je)
{
   bool resched = false;
   /*
    * Reschedule the job if requested and possible
    */
   /* Basic condition is that more reschedule times remain */
   if (jcr->job->RescheduleTimes == 0 ||
       jcr->reschedule_count < jcr->job->RescheduleTimes) {
      resched = 
         /* Check for incomplete jobs */
         (jcr->job->RescheduleIncompleteJobs && 
          jcr->is_incomplete() && jcr->is_JobType(JT_BACKUP) &&
          !jcr->is_JobLevel(L_BASE)) ||
         /* Check for failed jobs */
         (jcr->job->RescheduleOnError &&
          !jcr->is_JobStatus(JS_Terminated) &&
          !jcr->is_JobStatus(JS_Canceled) &&
          jcr->is_JobType(JT_BACKUP));
   }
   if (resched) {
       char dt[50], dt2[50];

       /*
        * Reschedule this job by cleaning it up, but
        *  reuse the same JobId if possible.
        */
      time_t now = time(NULL);
      jcr->reschedule_count++;
      jcr->sched_time = now + jcr->job->RescheduleInterval;
      bstrftime(dt, sizeof(dt), now);
      bstrftime(dt2, sizeof(dt2), jcr->sched_time);
      Dmsg4(2300, "Rescheduled Job %s to re-run in %d seconds.(now=%u,then=%u)\n", jcr->Job,
            (int)jcr->job->RescheduleInterval, now, jcr->sched_time);
      Jmsg(jcr, M_INFO, 0, _("Rescheduled Job %s at %s to re-run in %d seconds (%s).\n"),
           jcr->Job, dt, (int)jcr->job->RescheduleInterval, dt2);
      dird_free_jcr_pointers(jcr);     /* partial cleanup old stuff */
      jcr->JobStatus = -1;
      jcr->setJobStatus(JS_WaitStartTime);
      jcr->SDJobStatus = 0;
      jcr->JobErrors = 0;
      if (!allow_duplicate_job(jcr)) {
         return false;
      }
      /* Only jobs with no output or Incomplete jobs can run on same JCR */
      if (jcr->JobBytes == 0) {
         Dmsg2(2300, "Requeue job=%d use=%d\n", jcr->JobId, jcr->use_count());
         V(jq->mutex);
         /*
          * Special test here since a Virtual Full gets marked
          *  as a Full, so we look at the resource record
          */
         if (jcr->wasVirtualFull) {
            jcr->setJobLevel(L_VIRTUAL_FULL);
         }
         jobq_add(jq, jcr);     /* queue the job to run again */
         P(jq->mutex);
         free_jcr(jcr);         /* release jcr */
         free(je);              /* free the job entry */
         return true;           /* we already cleaned up */
      }
      /*
       * Something was actually backed up, so we cannot reuse
       *   the old JobId or there will be database record
       *   conflicts.  We now create a new job, copying the
       *   appropriate fields.
       */           
      JCR *njcr = new_jcr(sizeof(JCR), dird_free_jcr);
      set_jcr_defaults(njcr, jcr->job);
      njcr->reschedule_count = jcr->reschedule_count;
      njcr->sched_time = jcr->sched_time;
      /*
       * Special test here since a Virtual Full gets marked
       *  as a Full, so we look at the resource record
       */
      if (jcr->wasVirtualFull) {
         njcr->setJobLevel(L_VIRTUAL_FULL);
      } else {
         njcr->setJobLevel(jcr->getJobLevel());
      }
      njcr->pool = jcr->pool;
      njcr->run_pool_override = jcr->run_pool_override;
      njcr->full_pool = jcr->full_pool;
      njcr->run_full_pool_override = jcr->run_full_pool_override;
      njcr->inc_pool = jcr->inc_pool;
      njcr->run_inc_pool_override = jcr->run_inc_pool_override;
      njcr->diff_pool = jcr->diff_pool;
      njcr->JobStatus = -1;
      njcr->setJobStatus(jcr->JobStatus);
      if (jcr->rstore) {
         copy_rstorage(njcr, jcr->rstorage, _("previous Job"));
      } else {
         free_rstorage(njcr);
      }
      if (jcr->wstore) {
         copy_wstorage(njcr, jcr->wstorage, _("previous Job"));
      } else {
         free_wstorage(njcr);
      }
      njcr->messages = jcr->messages;
      njcr->spool_data = jcr->spool_data;
      njcr->write_part_after_job = jcr->write_part_after_job;
      Dmsg0(2300, "Call to run new job\n");
      V(jq->mutex);
      run_job(njcr);            /* This creates a "new" job */
      free_jcr(njcr);           /* release "new" jcr */
      P(jq->mutex);
      Dmsg0(2300, "Back from running new job.\n");
   }
   return false;
}

/*
 * See if we can acquire all the necessary resources for the job (JCR)
 *
 *  Returns: true  if successful
 *           false if resource failure
 */
static bool acquire_resources(JCR *jcr)
{
   bool skip_this_jcr = false;

   jcr->acquired_resource_locks = false;
/*
 * Turning this code off is likely to cause some deadlocks,
 *   but we do not really have enough information here to
 *   know if this is really a deadlock (it may be a dual drive
 *   autochanger), and in principle, the SD reservation system
 *   should detect these deadlocks, so push the work off on it.
 */
#ifdef xxx
   if (jcr->rstore && jcr->rstore == jcr->wstore) {    /* possible deadlock */
      Jmsg(jcr, M_FATAL, 0, _("Job canceled. Attempt to read and write same device.\n"
         "    Read storage \"%s\" (From %s) -- Write storage \"%s\" (From %s)\n"), 
         jcr->rstore->name(), jcr->rstore_source, jcr->wstore->name(), jcr->wstore_source);
      jcr->setJobStatus(JS_Canceled);
      return false;
   }
#endif
   if (jcr->rstore) {
      Dmsg1(200, "Rstore=%s\n", jcr->rstore->name());
      if (!inc_read_store(jcr)) {
         Dmsg1(200, "Fail rncj=%d\n", jcr->rstore->NumConcurrentJobs);
         jcr->setJobStatus(JS_WaitStoreRes);
         return false;
      }
   }
   
   if (jcr->wstore) {
      Dmsg1(200, "Wstore=%s\n", jcr->wstore->name());
      if (jcr->wstore->NumConcurrentJobs < jcr->wstore->MaxConcurrentJobs) {
         jcr->wstore->NumConcurrentJobs++;
         Dmsg1(200, "Inc wncj=%d\n", jcr->wstore->NumConcurrentJobs);
      } else if (jcr->rstore) {
         dec_read_store(jcr);
         skip_this_jcr = true;
      } else {
         Dmsg1(200, "Fail wncj=%d\n", jcr->wstore->NumConcurrentJobs);
         skip_this_jcr = true;
      }
   }
   if (skip_this_jcr) {
      jcr->setJobStatus(JS_WaitStoreRes);
      return false;
   }

   if (jcr->client->NumConcurrentJobs < jcr->client->MaxConcurrentJobs) {
      jcr->client->NumConcurrentJobs++;
   } else {
      /* Back out previous locks */
      dec_write_store(jcr);
      dec_read_store(jcr);
      jcr->setJobStatus(JS_WaitClientRes);
      return false;
   }
   if (jcr->job->NumConcurrentJobs < jcr->job->MaxConcurrentJobs) {
      jcr->job->NumConcurrentJobs++;
   } else {
      /* Back out previous locks */
      dec_write_store(jcr);
      dec_read_store(jcr);
      jcr->client->NumConcurrentJobs--;
      jcr->setJobStatus(JS_WaitJobRes);
      return false;
   }

   jcr->acquired_resource_locks = true;
   return true;
}

static pthread_mutex_t rstore_mutex = PTHREAD_MUTEX_INITIALIZER;

/* 
 * Note: inc_read_store() and dec_read_store() are
 *   called from select_rstore() in src/dird/restore.c
 */
bool inc_read_store(JCR *jcr)
{
   P(rstore_mutex);
   if (jcr->rstore->NumConcurrentJobs < jcr->rstore->MaxConcurrentJobs &&
       (jcr->getJobType() == JT_RESTORE ||
        jcr->rstore->MaxConcurrentReadJobs == 0 ||
        jcr->rstore->NumConcurrentReadJobs < jcr->rstore->MaxConcurrentReadJobs)) {
      jcr->rstore->NumConcurrentReadJobs++;
      jcr->rstore->NumConcurrentJobs++;
      Dmsg1(200, "Inc rncj=%d\n", jcr->rstore->NumConcurrentJobs);
      V(rstore_mutex);
      return true;
   }
   V(rstore_mutex);
   return false;
}

void dec_read_store(JCR *jcr)
{
   if (jcr->rstore) {
      P(rstore_mutex);
      jcr->rstore->NumConcurrentReadJobs--;    /* back out rstore */
      jcr->rstore->NumConcurrentJobs--;        /* back out rstore */
      Dmsg1(200, "Dec rncj=%d\n", jcr->rstore->NumConcurrentJobs);
      V(rstore_mutex);
      ASSERT(jcr->rstore->NumConcurrentReadJobs >= 0);
      ASSERT(jcr->rstore->NumConcurrentJobs >= 0);
   }
}

static void dec_write_store(JCR *jcr)
{
   if (jcr->wstore) {
      jcr->wstore->NumConcurrentJobs--;
      Dmsg1(200, "Dec wncj=%d\n", jcr->wstore->NumConcurrentJobs);
      ASSERT(jcr->wstore->NumConcurrentJobs >= 0);
   }
}
