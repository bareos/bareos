/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2002-2011 Free Software Foundation Europe e.V.

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
 * BAREOS thread watchdog routine. General routine that
 * allows setting a watchdog timer with a callback that is
 * called when the timer goes off.
 *
 * Kern Sibbald, January MMII
 */

#include "bareos.h"
#include "include/jcr.h"

/* Exported globals */
utime_t watchdog_time = 0;            /* this has granularity of SLEEP_TIME */
utime_t watchdog_sleep_time = 60;     /* examine things every 60 seconds */

/* Locals */
static pthread_mutex_t timer_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t timer = PTHREAD_COND_INITIALIZER;

/* Forward referenced functions */
extern "C" void *watchdog_thread(void *arg);

static void wd_lock();
static void wd_unlock();

/* Static globals */
static bool quit = false;
static bool wd_is_init = false;
static brwlock_t lock;                /* watchdog lock */

static pthread_t wd_tid;
static dlist *wd_queue;
static dlist *wd_inactive;

/*
 * Returns: 0 if the current thread is NOT the watchdog
 *          1 if the current thread is the watchdog
 */
bool is_watchdog()
{
   if (wd_is_init && pthread_equal(pthread_self(), wd_tid)) {
      return true;
   } else {
      return false;
   }
}

/*
 * Start watchdog thread
 *
 *  Returns: 0 on success
 *           errno on failure
 */
int start_watchdog(void)
{
   int status;
   watchdog_t *dummy = NULL;
   int errstat;

   if (wd_is_init) {
      return 0;
   }
   Dmsg0(800, "Initialising NicB-hacked watchdog thread\n");
   watchdog_time = time(NULL);

   if ((errstat=rwl_init(&lock)) != 0) {
      berrno be;
      Jmsg1(NULL, M_ABORT, 0, _("Unable to initialize watchdog lock. ERR=%s\n"),
            be.bstrerror(errstat));
   }
   wd_queue = New(dlist(dummy, &dummy->link));
   wd_inactive = New(dlist(dummy, &dummy->link));
   wd_is_init = true;

   if ((status = pthread_create(&wd_tid, NULL, watchdog_thread, NULL)) != 0) {
      return status;
   }
   return 0;
}

/*
 * Wake watchdog timer thread so that it walks the
 *  queue and adjusts its wait time (or exits).
 */
static void ping_watchdog()
{
   P(timer_mutex);
   pthread_cond_signal(&timer);
   V(timer_mutex);
   bmicrosleep(0, 100);
}

/*
 * Terminate the watchdog thread
 *
 * Returns: 0 on success
 *          errno on failure
 */
int stop_watchdog(void)
{
   int status;
   watchdog_t *p;

   if (!wd_is_init) {
      return 0;
   }

   quit = true;                       /* notify watchdog thread to stop */
   ping_watchdog();

   status = pthread_join(wd_tid, NULL);

   while (!wd_queue->empty()) {
      void *item = wd_queue->first();
      wd_queue->remove(item);
      p = (watchdog_t *)item;
      if (p->destructor != NULL) {
         p->destructor(p);
      }
      free(p);
   }
   delete wd_queue;
   wd_queue = NULL;

   while (!wd_inactive->empty()) {
      void *item = wd_inactive->first();
      wd_inactive->remove(item);
      p = (watchdog_t *)item;
      if (p->destructor != NULL) {
         p->destructor(p);
      }
      free(p);
   }
   delete wd_inactive;
   wd_inactive = NULL;
   rwl_destroy(&lock);
   wd_is_init = false;

   return status;
}

watchdog_t *new_watchdog(void)
{
   watchdog_t *wd = (watchdog_t *)malloc(sizeof(watchdog_t));

   if (!wd_is_init) {
      start_watchdog();
   }

   if (wd == NULL) {
      return NULL;
   }
   wd->one_shot = true;
   wd->interval = 0;
   wd->callback = NULL;
   wd->destructor = NULL;
   wd->data = NULL;

   return wd;
}

bool register_watchdog(watchdog_t *wd)
{
   if (!wd_is_init) {
      Jmsg0(NULL, M_ABORT, 0, _("BUG! register_watchdog called before start_watchdog\n"));
   }
   if (wd->callback == NULL) {
      Jmsg1(NULL, M_ABORT, 0, _("BUG! Watchdog %p has NULL callback\n"), wd);
   }
   if (wd->interval == 0) {
      Jmsg1(NULL, M_ABORT, 0, _("BUG! Watchdog %p has zero interval\n"), wd);
   }

   wd_lock();
   wd->next_fire = watchdog_time + wd->interval;
   wd_queue->append(wd);
   Dmsg3(800, "Registered watchdog %p, interval %d%s\n",
         wd, wd->interval, wd->one_shot ? " one shot" : "");
   wd_unlock();
   ping_watchdog();

   return false;
}

bool unregister_watchdog(watchdog_t *wd)
{
   watchdog_t *p;
   bool ok = false;

   if (!wd_is_init) {
      Jmsg0(NULL, M_ABORT, 0, _("BUG! unregister_watchdog_unlocked called before start_watchdog\n"));
   }

   wd_lock();
   foreach_dlist(p, wd_queue) {
      if (wd == p) {
         wd_queue->remove(wd);
         Dmsg1(800, "Unregistered watchdog %p\n", wd);
         ok = true;
         goto get_out;
      }
   }

   foreach_dlist(p, wd_inactive) {
      if (wd == p) {
         wd_inactive->remove(wd);
         Dmsg1(800, "Unregistered inactive watchdog %p\n", wd);
         ok = true;
         goto get_out;
      }
   }

   Dmsg1(800, "Failed to unregister watchdog %p\n", wd);

get_out:
   wd_unlock();
   ping_watchdog();
   return ok;
}

/*
 * This is the thread that walks the watchdog queue
 *  and when a queue item fires, the callback is
 *  invoked.  If it is a one shot, the queue item
 *  is moved to the inactive queue.
 */
extern "C" void *watchdog_thread(void *arg)
{
   struct timespec timeout;
   struct timeval tv;
   struct timezone tz;
   utime_t next_time;

   set_jcr_in_tsd(INVALID_JCR);
   Dmsg0(800, "NicB-reworked watchdog thread entered\n");

   while (!quit) {
      watchdog_t *p;

      /*
       *
       *  NOTE. lock_jcr_chain removed, but the message below
       *   was left until we are sure there are no deadlocks.
       *
       * We lock the jcr chain here because a good number of the
       *   callback routines lock the jcr chain. We need to lock
       *   it here *before* the watchdog lock because the SD message
       *   thread first locks the jcr chain, then when closing the
       *   job locks the watchdog chain. If the two threads do not
       *   lock in the same order, we get a deadlock -- each holds
       *   the other's needed lock.
       */
      wd_lock();

walk_list:
      watchdog_time = time(NULL);
      next_time = watchdog_time + watchdog_sleep_time;
      foreach_dlist(p, wd_queue) {
         if (p->next_fire <= watchdog_time) {
            /* Run the callback */
            Dmsg2(3400, "Watchdog callback p=0x%p fire=%d\n", p, p->next_fire);
            p->callback(p);

            /* Reschedule (or move to inactive list if it's a one-shot timer) */
            if (p->one_shot) {
               wd_queue->remove(p);
               wd_inactive->append(p);
               goto walk_list;
            } else {
               p->next_fire = watchdog_time + p->interval;
            }
         }
         if (p->next_fire <= next_time) {
            next_time = p->next_fire;
         }
      }
      wd_unlock();

      /*
       * Wait sleep time or until someone wakes us
       */
      gettimeofday(&tv, &tz);
      timeout.tv_nsec = tv.tv_usec * 1000;
      timeout.tv_sec = tv.tv_sec + next_time - time(NULL);
      while (timeout.tv_nsec >= 1000000000) {
         timeout.tv_nsec -= 1000000000;
         timeout.tv_sec++;
      }

      Dmsg1(1900, "pthread_cond_timedwait %d\n", timeout.tv_sec - tv.tv_sec);
      /* Note, this unlocks mutex during the sleep */
      P(timer_mutex);
      pthread_cond_timedwait(&timer, &timer_mutex, &timeout);
      V(timer_mutex);
   }

   Dmsg0(800, "NicB-reworked watchdog thread exited\n");
   return NULL;
}

/*
 * Watchdog lock, this can be called multiple times by the same
 *   thread without blocking, but must be unlocked the number of
 *   times it was locked.
 */
static void wd_lock()
{
   int errstat;
   if ((errstat=rwl_writelock(&lock)) != 0) {
      berrno be;
      Jmsg1(NULL, M_ABORT, 0, _("rwl_writelock failure. ERR=%s\n"),
           be.bstrerror(errstat));
   }
}

/*
 * Unlock the watchdog. This can be called multiple times by the
 *   same thread up to the number of times that thread called
 *   wd_ lock()/
 */
static void wd_unlock()
{
   int errstat;
   if ((errstat=rwl_writeunlock(&lock)) != 0) {
      berrno be;
      Jmsg1(NULL, M_ABORT, 0, _("rwl_writeunlock failure. ERR=%s\n"),
           be.bstrerror(errstat));
   }
}
