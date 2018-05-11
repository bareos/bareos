/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2001-2012 Free Software Foundation Europe e.V.

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
 * BAREOS work queue routines. Permits passing work to
 * multiple threads.
 *
 * Kern Sibbald, January MMI
 *
 * This code adapted from "Programming with POSIX Threads", by David R. Butenhof
 *
 * Example:
 *
 * static workq_t job_wq;    define work queue
 *
 *  Initialize queue
 *  if ((stat = WorkqInit(&job_wq, max_workers, job_thread)) != 0) {
 *     BErrNo be;
 *     Emsg1(M_ABORT, 0, "Could not init job work queue: ERR=%s\n", be.bstrerror(errno));
 *   }
 *
 *  Add an item to the queue
 *  if ((stat = WorkqAdd(&job_wq, (void *)jcr)) != 0) {
 *      BErrNo be;
 *      Emsg1(M_ABORT, 0, "Could not add job to work queue: ERR=%s\n", be.bstrerror(errno));
 *   }
 *
 *  Terminate the queue
 *  WorkqDestroy(workq_t *wq);
 */

#include "include/bareos.h"
#include "include/jcr.h"

/* Forward referenced functions */
extern "C" void *workq_server(void *arg);

/*
 * Initialize a work queue
 *
 *  Returns: 0 on success
 *           errno on failure
 */
int WorkqInit(workq_t *wq, int threads, void *(*engine)(void *arg))
{
   int status;

   if ((status = pthread_attr_init(&wq->attr)) != 0) {
      return status;
   }
   if ((status = pthread_attr_setdetachstate(&wq->attr, PTHREAD_CREATE_DETACHED)) != 0) {
      pthread_attr_destroy(&wq->attr);
      return status;
   }
   if ((status = pthread_mutex_init(&wq->mutex, NULL)) != 0) {
      pthread_attr_destroy(&wq->attr);
      return status;
   }
   if ((status = pthread_cond_init(&wq->work, NULL)) != 0) {
      pthread_mutex_destroy(&wq->mutex);
      pthread_attr_destroy(&wq->attr);
      return status;
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
int WorkqDestroy(workq_t *wq)
{
   int status, status1, status2;

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
        if ((status = pthread_cond_broadcast(&wq->work)) != 0) {
           V(wq->mutex);
           return status;
        }
     }
     while (wq->num_workers > 0) {
        Dmsg1(1400, "active workers: %d. Waiting for them to finish.\n", wq->num_workers);
        if ((status = pthread_cond_wait(&wq->work, &wq->mutex)) != 0) {
           V(wq->mutex);
           return status;
        }
     }
  }
  V(wq->mutex);
  status = pthread_mutex_destroy(&wq->mutex);
  status1 = pthread_cond_destroy(&wq->work);
  status2 = pthread_attr_destroy(&wq->attr);
  return (status != 0 ? status : (status1 != 0 ? status1 : status2));
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
int WorkqAdd(workq_t *wq, void *element, workq_ele_t **work_item, int priority)
{
   int status = 0;
   workq_ele_t *item;
   pthread_t id;

   Dmsg0(1400, "WorkqAdd\n");
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
      if ((status = pthread_cond_broadcast(&wq->work)) != 0) {
         V(wq->mutex);
         return status;
      }
   } else if (wq->num_workers < wq->max_workers) {
      Dmsg0(1400, "Create worker thread\n");
      /* No idle threads so create a new one */
      SetThreadConcurrency(wq->max_workers + 1);
      if ((status = pthread_create(&id, &wq->attr, workq_server, (void *)wq)) != 0) {
         V(wq->mutex);
         return status;
      }
      wq->num_workers++;
   }
   V(wq->mutex);
   Dmsg0(1400, "Return WorkqAdd\n");
   /* Return work_item if requested */
   if (work_item) {
      *work_item = item;
   }
   return status;
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
int WorkqRemove(workq_t *wq, workq_ele_t *work_item)
{
   int status, found = 0;
   pthread_t id;
   workq_ele_t *item, *prev;

   Dmsg0(1400, "WorkqRemove\n");
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
      if ((status = pthread_cond_broadcast(&wq->work)) != 0) {
         V(wq->mutex);
         return status;
      }
   } else {
      Dmsg0(1400, "Create worker thread\n");
      /* No idle threads so create a new one */
      SetThreadConcurrency(wq->max_workers + 1);
      if ((status = pthread_create(&id, &wq->attr, workq_server, (void *)wq)) != 0) {
         V(wq->mutex);
         return status;
      }
      wq->num_workers++;
   }
   V(wq->mutex);
   Dmsg0(1400, "Return WorkqRemove\n");
   return status;
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
   int status, timedout;

   Dmsg0(1400, "Start workq_server\n");
   P(wq->mutex);
   SetJcrInTsd(INVALID_JCR);

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
         status = ETIMEDOUT;
#else
         status = pthread_cond_timedwait(&wq->work, &wq->mutex, &timeout);
#endif
         Dmsg1(1400, "timedwait=%d\n", status);
         if (status == ETIMEDOUT) {
            timedout = 1;
            break;
         } else if (status != 0) {
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
