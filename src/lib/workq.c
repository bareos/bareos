/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2001-2012 Free Software Foundation Europe e.V.

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
 * Bacula work queue routines. Permits passing work to
 *  multiple threads.
 *
 *  Kern Sibbald, January MMI
 *
 *  This code adapted from "Programming with POSIX Threads", by
 *    David R. Butenhof
 *
 * Example:
 *
 * static workq_t job_wq;    define work queue
 *
 *  Initialize queue
 *  if ((stat = workq_init(&job_wq, max_workers, job_thread)) != 0) {
 *     berrno be;
 *     Emsg1(M_ABORT, 0, "Could not init job work queue: ERR=%s\n", be.bstrerror(errno));
 *   }
 *
 *  Add an item to the queue
 *  if ((stat = workq_add(&job_wq, (void *)jcr)) != 0) {
 *      berrno be;
 *      Emsg1(M_ABORT, 0, "Could not add job to work queue: ERR=%s\n", be.bstrerror(errno));
 *   }
 *
 *  Terminate the queue
 *  workq_destroy(workq_t *wq);
 *
 */

#include "bacula.h"
#include "jcr.h"

/* Forward referenced functions */
extern "C" void *workq_server(void *arg);

/*
 * Initialize a work queue
 *
 *  Returns: 0 on success
 *           errno on failure
 */
int workq_init(workq_t *wq, int threads, void *(*engine)(void *arg))
{
   int stat;

   if ((stat = pthread_attr_init(&wq->attr)) != 0) {
      return stat;
   }
   if ((stat = pthread_attr_setdetachstate(&wq->attr, PTHREAD_CREATE_DETACHED)) != 0) {
      pthread_attr_destroy(&wq->attr);
      return stat;
   }
   if ((stat = pthread_mutex_init(&wq->mutex, NULL)) != 0) {
      pthread_attr_destroy(&wq->attr);
      return stat;
   }
   if ((stat = pthread_cond_init(&wq->work, NULL)) != 0) {
      pthread_mutex_destroy(&wq->mutex);
      pthread_attr_destroy(&wq->attr);
      return stat;
   }
   wq->quit = 0;
   wq->first = wq->last = NULL;
   wq->max_workers = threads;         /* max threads to create */
   wq->num_workers = 0;               /* no threads yet */
   wq->idle_workers = 0;              /* no idle threads */
   wq->engine = engine;               /* routine to run */
   wq->valid = WORKQ_VALID;
   return 0;
}

/*
 * Destroy a work queue
 *
 * Returns: 0 on success
 *          errno on failure
 */
int workq_destroy(workq_t *wq)
{
   int stat, stat1, stat2;

  if (wq->valid != WORKQ_VALID) {
     return EINVAL;
  }
  P(wq->mutex);
  wq->valid = 0;                      /* prevent any more operations */

  /*
   * If any threads are active, wake them
   */
  if (wq->num_workers > 0) {
     wq->quit = 1;
     if (wq->idle_workers) {
        if ((stat = pthread_cond_broadcast(&wq->work)) != 0) {
           V(wq->mutex);
           return stat;
        }
     }
     while (wq->num_workers > 0) {
        if ((stat = pthread_cond_wait(&wq->work, &wq->mutex)) != 0) {
           V(wq->mutex);
           return stat;
        }
     }
  }
  V(wq->mutex);
  stat  = pthread_mutex_destroy(&wq->mutex);
  stat1 = pthread_cond_destroy(&wq->work);
  stat2 = pthread_attr_destroy(&wq->attr);
  return (stat != 0 ? stat : (stat1 != 0 ? stat1 : stat2));
}


/*
 *  Add work to a queue
 *    wq is a queue that was created with workq_init
 *    element is a user unique item that will be passed to the
 *        processing routine
 *    work_item will get internal work queue item -- if it is not NULL
 *    priority if non-zero will cause the item to be placed on the
 *        head of the list instead of the tail.
 */
int workq_add(workq_t *wq, void *element, workq_ele_t **work_item, int priority)
{
   int stat=0;
   workq_ele_t *item;
   pthread_t id;

   Dmsg0(1400, "workq_add\n");
   if (wq->valid != WORKQ_VALID) {
      return EINVAL;
   }

   if ((item = (workq_ele_t *)malloc(sizeof(workq_ele_t))) == NULL) {
      return ENOMEM;
   }
   item->data = element;
   item->next = NULL;
   P(wq->mutex);

   Dmsg0(1400, "add item to queue\n");
   if (priority) {
      /* Add to head of queue */
      if (wq->first == NULL) {
         wq->first = item;
         wq->last = item;
      } else {
         item->next = wq->first;
         wq->first = item;
      }
   } else {
      /* Add to end of queue */
      if (wq->first == NULL) {
         wq->first = item;
      } else {
         wq->last->next = item;
      }
      wq->last = item;
   }

   /* if any threads are idle, wake one */
   if (wq->idle_workers > 0) {
      Dmsg0(1400, "Signal worker\n");
      if ((stat = pthread_cond_broadcast(&wq->work)) != 0) {
         V(wq->mutex);
         return stat;
      }
   } else if (wq->num_workers < wq->max_workers) {
      Dmsg0(1400, "Create worker thread\n");
      /* No idle threads so create a new one */
      set_thread_concurrency(wq->max_workers + 1);
      if ((stat = pthread_create(&id, &wq->attr, workq_server, (void *)wq)) != 0) {
         V(wq->mutex);
         return stat;
      }
      wq->num_workers++;
   }
   V(wq->mutex);
   Dmsg0(1400, "Return workq_add\n");
   /* Return work_item if requested */
   if (work_item) {
      *work_item = item;
   }
   return stat;
}

/*
 *  Remove work from a queue
 *    wq is a queue that was created with workq_init
 *    work_item is an element of work
 *
 *   Note, it is "removed" by immediately calling a processing routine.
 *    if you want to cancel it, you need to provide some external means
 *    of doing so.
 */
int workq_remove(workq_t *wq, workq_ele_t *work_item)
{
   int stat, found = 0;
   pthread_t id;
   workq_ele_t *item, *prev;

   Dmsg0(1400, "workq_remove\n");
   if (wq->valid != WORKQ_VALID) {
      return EINVAL;
   }

   P(wq->mutex);

   for (prev=item=wq->first; item; item=item->next) {
      if (item == work_item) {
         found = 1;
         break;
      }
      prev = item;
   }
   if (!found) {
      return EINVAL;
   }

   /* Move item to be first on list */
   if (wq->first != work_item) {
      prev->next = work_item->next;
      if (wq->last == work_item) {
         wq->last = prev;
      }
      work_item->next = wq->first;
      wq->first = work_item;
   }

   /* if any threads are idle, wake one */
   if (wq->idle_workers > 0) {
      Dmsg0(1400, "Signal worker\n");
      if ((stat = pthread_cond_broadcast(&wq->work)) != 0) {
         V(wq->mutex);
         return stat;
      }
   } else {
      Dmsg0(1400, "Create worker thread\n");
      /* No idle threads so create a new one */
      set_thread_concurrency(wq->max_workers + 1);
      if ((stat = pthread_create(&id, &wq->attr, workq_server, (void *)wq)) != 0) {
         V(wq->mutex);
         return stat;
      }
      wq->num_workers++;
   }
   V(wq->mutex);
   Dmsg0(1400, "Return workq_remove\n");
   return stat;
}


/*
 * This is the worker thread that serves the work queue.
 * In due course, it will call the user's engine.
 */
extern "C"
void *workq_server(void *arg)
{
   struct timespec timeout;
   workq_t *wq = (workq_t *)arg;
   workq_ele_t *we;
   int stat, timedout;

   Dmsg0(1400, "Start workq_server\n");
   P(wq->mutex);
   set_jcr_in_tsd(INVALID_JCR);

   for (;;) {
      struct timeval tv;
      struct timezone tz;

      Dmsg0(1400, "Top of for loop\n");
      timedout = 0;
      Dmsg0(1400, "gettimeofday()\n");
      gettimeofday(&tv, &tz);
      timeout.tv_nsec = 0;
      timeout.tv_sec = tv.tv_sec + 2;

      while (wq->first == NULL && !wq->quit) {
         /*
          * Wait 2 seconds, then if no more work, exit
          */
         Dmsg0(1400, "pthread_cond_timedwait()\n");
#ifdef xxxxxxxxxxxxxxxx_was_HAVE_CYGWIN
         /* CYGWIN dies with a page fault the second
          * time that pthread_cond_timedwait() is called
          * so fake it out.
          */
         P(wq->mutex);
         stat = ETIMEDOUT;
#else
         stat = pthread_cond_timedwait(&wq->work, &wq->mutex, &timeout);
#endif
         Dmsg1(1400, "timedwait=%d\n", stat);
         if (stat == ETIMEDOUT) {
            timedout = 1;
            break;
         } else if (stat != 0) {
            /* This shouldn't happen */
            Dmsg0(1400, "This shouldn't happen\n");
            wq->num_workers--;
            V(wq->mutex);
            return NULL;
         }
      }
      we = wq->first;
      if (we != NULL) {
         wq->first = we->next;
         if (wq->last == we) {
            wq->last = NULL;
         }
         V(wq->mutex);
         /* Call user's routine here */
         Dmsg0(1400, "Calling user engine.\n");
         wq->engine(we->data);
         Dmsg0(1400, "Back from user engine.\n");
         free(we);                    /* release work entry */
         Dmsg0(1400, "relock mutex\n");
         P(wq->mutex);
         Dmsg0(1400, "Done lock mutex\n");
      }
      /*
       * If no more work request, and we are asked to quit, then do it
       */
      if (wq->first == NULL && wq->quit) {
         wq->num_workers--;
         if (wq->num_workers == 0) {
            Dmsg0(1400, "Wake up destroy routine\n");
            /* Wake up destroy routine if he is waiting */
            pthread_cond_broadcast(&wq->work);
         }
         Dmsg0(1400, "Unlock mutex\n");
         V(wq->mutex);
         Dmsg0(1400, "Return from workq_server\n");
         return NULL;
      }
      Dmsg0(1400, "Check for work request\n");
      /*
       * If no more work requests, and we waited long enough, quit
       */
      Dmsg1(1400, "wq->first==NULL = %d\n", wq->first==NULL);
      Dmsg1(1400, "timedout=%d\n", timedout);
      if (wq->first == NULL && timedout) {
         Dmsg0(1400, "break big loop\n");
         wq->num_workers--;
         break;
      }
      Dmsg0(1400, "Loop again\n");
   } /* end of big for loop */

   Dmsg0(1400, "unlock mutex\n");
   V(wq->mutex);
   Dmsg0(1400, "End workq_server\n");
   return NULL;
}
