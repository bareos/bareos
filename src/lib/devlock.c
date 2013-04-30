/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2001-2011 Free Software Foundation Europe e.V.

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
 * Bacula Thread Read/Write locking code. It permits
 *  multiple readers but only one writer.  Note, however,
 *  that the writer thread is permitted to make multiple
 *  nested write lock calls.
 *
 *  Kern Sibbald, January MMI
 *
 *  This code adapted from "Programming with POSIX Threads", by
 *    David R. Butenhof
 *
 */

#define _LOCKMGR_COMPLIANT
#include "bacula.h"
#include "devlock.h"

/*
 * Initialize a read/write lock
 *
 *  Returns: 0 on success
 *           errno on failure
 */

devlock *new_devlock()                          
{
   devlock *lock;
   lock = (devlock *)malloc(sizeof (devlock));
   memset(lock, 0, sizeof(devlock));
   return lock;
}

int devlock::init(int priority)                            
{
   int stat;
   devlock *rwl = this;

   rwl->r_active = rwl->w_active = 0;
   rwl->r_wait = rwl->w_wait = 0;
   rwl->priority = priority;
   if ((stat = pthread_mutex_init(&rwl->mutex, NULL)) != 0) {
      return stat;
   }
   if ((stat = pthread_cond_init(&rwl->read, NULL)) != 0) {
      pthread_mutex_destroy(&rwl->mutex);
      return stat;
   }
   if ((stat = pthread_cond_init(&rwl->write, NULL)) != 0) {
      pthread_cond_destroy(&rwl->read);
      pthread_mutex_destroy(&rwl->mutex);
      return stat;
   }
   rwl->valid = DEVLOCK_VALID;
   return 0;
}

/*
 * Destroy a read/write lock
 *
 * Returns: 0 on success
 *          errno on failure
 */
int devlock::destroy()
{
   devlock *rwl = this;
   int stat, stat1, stat2;

   if (rwl->valid != DEVLOCK_VALID) {
      return EINVAL;
   }
   if ((stat = pthread_mutex_lock(&rwl->mutex)) != 0) {
      return stat;
   }

   /*
    * If any threads are active, report EBUSY
    */
   if (rwl->r_active > 0 || rwl->w_active) {
      pthread_mutex_unlock(&rwl->mutex);
      return EBUSY;
   }

   /*
    * If any threads are waiting, report EBUSY
    */
   if (rwl->r_wait > 0 || rwl->w_wait > 0) {
      pthread_mutex_unlock(&rwl->mutex);
      return EBUSY;
   }

   rwl->valid = 0;
   if ((stat = pthread_mutex_unlock(&rwl->mutex)) != 0) {
      return stat;
   }
   stat  = pthread_mutex_destroy(&rwl->mutex);
   stat1 = pthread_cond_destroy(&rwl->read);
   stat2 = pthread_cond_destroy(&rwl->write);
   return (stat != 0 ? stat : (stat1 != 0 ? stat1 : stat2));
}

/*
 * Handle cleanup when the read lock condition variable
 * wait is released.
 */
static void devlock_read_release(void *arg)
{
   devlock *rwl = (devlock *)arg;
   rwl->read_release();
}

void devlock::read_release()
{
   r_wait--;
   pthread_mutex_unlock(&mutex);
}

/*
 * Handle cleanup when the write lock condition variable wait
 * is released.
 */
static void devlock_write_release(void *arg)
{
   devlock *rwl = (devlock *)arg;
   rwl->write_release();
}

void devlock::write_release()
{
   w_wait--;
   pthread_mutex_unlock(&mutex);
}

/*
 * Lock for read access, wait until locked (or error).
 */
int devlock::readlock()
{
   devlock *rwl = this;
   int stat;

   if (rwl->valid != DEVLOCK_VALID) {
      return EINVAL;
   }
   if ((stat = pthread_mutex_lock(&rwl->mutex)) != 0) {
      return stat;
   }
   if (rwl->w_active) {
      rwl->r_wait++;                  /* indicate that we are waiting */
      pthread_cleanup_push(devlock_read_release, (void *)rwl);
      while (rwl->w_active) {
         stat = pthread_cond_wait(&rwl->read, &rwl->mutex);
         if (stat != 0) {
            break;                    /* error, bail out */
         }
      }
      pthread_cleanup_pop(0);
      rwl->r_wait--;                  /* we are no longer waiting */
   }
   if (stat == 0) {
      rwl->r_active++;                /* we are running */
   }
   pthread_mutex_unlock(&rwl->mutex);
   return stat;
}

/*
 * Attempt to lock for read access, don't wait
 */
int devlock::readtrylock()
{
   devlock *rwl = this;
   int stat, stat2;

   if (rwl->valid != DEVLOCK_VALID) {
      return EINVAL;
   }
   if ((stat = pthread_mutex_lock(&rwl->mutex)) != 0) {
      return stat;
   }
   if (rwl->w_active) {
      stat = EBUSY;
   } else {
      rwl->r_active++;                /* we are running */
   }
   stat2 = pthread_mutex_unlock(&rwl->mutex);
   return (stat == 0 ? stat2 : stat);
}

/*
 * Unlock read lock
 */
int devlock::readunlock()
{
   devlock *rwl = this;
   int stat, stat2;

   if (rwl->valid != DEVLOCK_VALID) {
      return EINVAL;
   }
   if ((stat = pthread_mutex_lock(&rwl->mutex)) != 0) {
      return stat;
   }
   rwl->r_active--;
   if (rwl->r_active == 0 && rwl->w_wait > 0) { /* if writers waiting */
      stat = pthread_cond_broadcast(&rwl->write);
   }
   stat2 = pthread_mutex_unlock(&rwl->mutex);
   return (stat == 0 ? stat2 : stat);
}


/*
 * Lock for write access, wait until locked (or error).
 *   Multiple nested write locking is permitted.
 */
int devlock::writelock(int areason, bool acan_take)
{
   devlock *rwl = this;
   int stat;

   if (rwl->valid != DEVLOCK_VALID) {
      return EINVAL;
   }
   if ((stat = pthread_mutex_lock(&rwl->mutex)) != 0) {
      return stat;
   }
   if (rwl->w_active && pthread_equal(rwl->writer_id, pthread_self())) {
      rwl->w_active++;
      pthread_mutex_unlock(&rwl->mutex);
      return 0;
   }
   lmgr_pre_lock(rwl, rwl->priority, __FILE__, __LINE__);
   if (rwl->w_active || rwl->r_active > 0) {
      rwl->w_wait++;                  /* indicate that we are waiting */
      pthread_cleanup_push(devlock_write_release, (void *)rwl);
      while (rwl->w_active || rwl->r_active > 0) {
         if ((stat = pthread_cond_wait(&rwl->write, &rwl->mutex)) != 0) {
            lmgr_do_unlock(rwl);
            break;                    /* error, bail out */
         }
      }
      pthread_cleanup_pop(0);
      rwl->w_wait--;                  /* we are no longer waiting */
   }
   if (stat == 0) {
      rwl->w_active++;                /* we are running */
      rwl->writer_id = pthread_self(); /* save writer thread's id */
      lmgr_post_lock();
   } 
   rwl->reason = areason;
   rwl->can_take = acan_take;
   pthread_mutex_unlock(&rwl->mutex);
   return stat;
}

/*
 * Attempt to lock for write access, don't wait
 */
int devlock::writetrylock()
{
   devlock *rwl = this;
   int stat, stat2;

   if (rwl->valid != DEVLOCK_VALID) {
      return EINVAL;
   }
   if ((stat = pthread_mutex_lock(&rwl->mutex)) != 0) {
      return stat;
   }
   if (rwl->w_active && pthread_equal(rwl->writer_id, pthread_self())) {
      rwl->w_active++;
      pthread_mutex_unlock(&rwl->mutex);
      return 0;
   }
   if (rwl->w_active || rwl->r_active > 0) {
      stat = EBUSY;
   } else {
      rwl->w_active = 1;              /* we are running */
      rwl->writer_id = pthread_self(); /* save writer thread's id */
      lmgr_do_lock(rwl, rwl->priority, __FILE__, __LINE__);
   }
   stat2 = pthread_mutex_unlock(&rwl->mutex);
   return (stat == 0 ? stat2 : stat);
}

/*
 * Unlock write lock
 *  Start any waiting writers in preference to waiting readers
 */
int devlock::writeunlock()
{
   devlock *rwl = this;
   int stat, stat2;

   if (rwl->valid != DEVLOCK_VALID) {
      return EINVAL;
   }
   if ((stat = pthread_mutex_lock(&rwl->mutex)) != 0) {
      return stat;
   }
   if (rwl->w_active <= 0) {
      pthread_mutex_unlock(&rwl->mutex);
      Jmsg0(NULL, M_ABORT, 0, _("writeunlock called too many times.\n"));
   }
   rwl->w_active--;
   if (!pthread_equal(pthread_self(), rwl->writer_id)) {
      pthread_mutex_unlock(&rwl->mutex);
      Jmsg0(NULL, M_ABORT, 0, _("writeunlock by non-owner.\n"));
   }
   if (rwl->w_active > 0) {
      stat = 0;                       /* writers still active */
   } else {
      lmgr_do_unlock(rwl);
      /* No more writers, awaken someone */
      if (rwl->r_wait > 0) {         /* if readers waiting */
         stat = pthread_cond_broadcast(&rwl->read);
      } else if (rwl->w_wait > 0) {
         stat = pthread_cond_broadcast(&rwl->write);
      }
   }
   stat2 = pthread_mutex_unlock(&rwl->mutex);
   return (stat == 0 ? stat2 : stat);
}

int devlock::take_lock(take_lock_t *hold, int areason)
{
   int stat;

   if (valid != DEVLOCK_VALID) {
      return EINVAL;
   }
   if ((stat = pthread_mutex_lock(&mutex)) != 0) {
      return stat;
   }
   hold->reason = reason;
   hold->prev_reason = prev_reason;
   hold->writer_id = writer_id;
   reason = areason;
   writer_id = pthread_self();
   stat = pthread_mutex_unlock(&mutex);
   return stat;
}

int devlock::return_lock(take_lock_t *hold)
{
   int stat, stat2;

   if (valid != DEVLOCK_VALID) {
      return EINVAL;
   }
   if ((stat = pthread_mutex_lock(&mutex)) != 0) {
      return stat;
   }
   reason = hold->reason;
   prev_reason = hold->prev_reason;
   writer_id = hold->writer_id;
   writer_id = pthread_self();
   stat2 = pthread_mutex_unlock(&mutex);
   if (w_active || w_wait) {
      stat = pthread_cond_broadcast(&write);
   }
   return (stat == 0 ? stat2 : stat);

}

#ifdef TEST_RWLOCK

#define THREADS     300
#define DATASIZE   15
#define ITERATIONS 1000000

/*
 * Keep statics for each thread.
 */
typedef struct thread_tag {
   int thread_num;
   pthread_t thread_id;
   int writes;
   int reads;
   int interval;
} thread_t;

/*
 * Read/write lock and shared data.
 */
typedef struct data_tag {
   brwlock_t lock;
   int data;
   int writes;
} data_t;

static thread_t threads[THREADS];
static data_t data[DATASIZE];

/*
 * Thread start routine that uses read/write locks.
 */
void *thread_routine(void *arg)
{
   thread_t *self = (thread_t *)arg;
   int repeats = 0;
   int iteration;
   int element = 0;
   int status;

   for (iteration=0; iteration < ITERATIONS; iteration++) {
      /*
       * Each "self->interval" iterations, perform an
       * update operation (write lock instead of read
       * lock).
       */
//      if ((iteration % self->interval) == 0) {
         status = writelock(&data[element].lock);
         if (status != 0) {
            berrno be;
            printf("Write lock failed. ERR=%s\n", be.bstrerror(status));
            exit(1);
         }
         data[element].data = self->thread_num;
         data[element].writes++;
         self->writes++;
         status = writelock(&data[element].lock);
         if (status != 0) {
            berrno be;
            printf("Write lock failed. ERR=%s\n", be.bstrerror(status));
            exit(1);
         }
         data[element].data = self->thread_num;
         data[element].writes++;
         self->writes++;
         status = writeunlock(&data[element].lock);
         if (status != 0) {
            berrno be;
            printf("Write unlock failed. ERR=%s\n", be.bstrerror(status));
            exit(1);
         }
         status = writeunlock(&data[element].lock);
         if (status != 0) {
            berrno be;
            printf("Write unlock failed. ERR=%s\n", be.bstrerror(status));
            exit(1);
         }

#ifdef xxx
      } else {
         /*
          * Look at the current data element to see whether
          * the current thread last updated it. Count the
          * times to report later.
          */
          status = readlock(&data[element].lock);
          if (status != 0) {
             berrno be;
             printf("Read lock failed. ERR=%s\n", be.bstrerror(status));
             exit(1);
          }
          self->reads++;
          if (data[element].data == self->thread_num)
             repeats++;
          status = readunlock(&data[element].lock);
          if (status != 0) {
             berrno be;
             printf("Read unlock failed. ERR=%s\n", be.bstrerror(status));
             exit(1);
          }
      }
#endif
      element++;
      if (element >= DATASIZE) {
         element = 0;
      }
   }
   if (repeats > 0) {
      Pmsg2(000, _("Thread %d found unchanged elements %d times\n"),
         self->thread_num, repeats);
   }
   return NULL;
}

int main (int argc, char *argv[])
{
    int count;
    int data_count;
    int status;
    unsigned int seed = 1;
    int thread_writes = 0;
    int data_writes = 0;

#ifdef USE_THR_SETCONCURRENCY
    /*
     * On Solaris 2.5,2.6,7 and 8 threads are not timesliced. To ensure
     * that our threads can run concurrently, we need to
     * increase the concurrency level to THREADS.
     */
    thr_setconcurrency (THREADS);
#endif

    /*
     * Initialize the shared data.
     */
    for (data_count = 0; data_count < DATASIZE; data_count++) {
        data[data_count].data = 0;
        data[data_count].writes = 0;
        status = rwl_init(&data[data_count].lock);
        if (status != 0) {
           berrno be;
           printf("Init rwlock failed. ERR=%s\n", be.bstrerror(status));
           exit(1);
        }
    }

    /*
     * Create THREADS threads to access shared data.
     */
    for (count = 0; count < THREADS; count++) {
        threads[count].thread_num = count + 1;
        threads[count].writes = 0;
        threads[count].reads = 0;
        threads[count].interval = rand_r(&seed) % 71;
        if (threads[count].interval <= 0) {
           threads[count].interval = 1;
        }
        status = pthread_create (&threads[count].thread_id,
            NULL, thread_routine, (void*)&threads[count]);
        if (status != 0 || (int)threads[count].thread_id == 0) {
           berrno be;
           printf("Create thread failed. ERR=%s\n", be.bstrerror(status));
           exit(1);
        }
    }

    /*
     * Wait for all threads to complete, and collect
     * statistics.
     */
    for (count = 0; count < THREADS; count++) {
        status = pthread_join (threads[count].thread_id, NULL);
        if (status != 0) {
           berrno be;
           printf("Join thread failed. ERR=%s\n", be.bstrerror(status));
           exit(1);
        }
        thread_writes += threads[count].writes;
        printf (_("%02d: interval %d, writes %d, reads %d\n"),
            count, threads[count].interval,
            threads[count].writes, threads[count].reads);
    }

    /*
     * Collect statistics for the data.
     */
    for (data_count = 0; data_count < DATASIZE; data_count++) {
        data_writes += data[data_count].writes;
        printf (_("data %02d: value %d, %d writes\n"),
            data_count, data[data_count].data, data[data_count].writes);
        rwl_destroy (&data[data_count].lock);
    }

    printf (_("Total: %d thread writes, %d data writes\n"),
        thread_writes, data_writes);
    return 0;
}

#endif

#ifdef TEST_RW_TRY_LOCK
/*
 * brwlock_try_main.c
 *
 * Demonstrate use of non-blocking read-write locks.
 *
 * Special notes: On older Solaris system, call thr_setconcurrency()
 * to allow interleaved thread execution, since threads are not
 * timesliced.
 */
#include <pthread.h>
#include "rwlock.h"
#include "errors.h"

#define THREADS         5
#define ITERATIONS      1000
#define DATASIZE        15

/*
 * Keep statistics for each thread.
 */
typedef struct thread_tag {
    int         thread_num;
    pthread_t   thread_id;
    int         r_collisions;
    int         w_collisions;
    int         updates;
    int         interval;
} thread_t;

/*
 * Read-write lock and shared data
 */
typedef struct data_tag {
    brwlock_t    lock;
    int         data;
    int         updates;
} data_t;

thread_t threads[THREADS];
data_t data[DATASIZE];

/*
 * Thread start routine that uses read-write locks
 */
void *thread_routine (void *arg)
{
    thread_t *self = (thread_t*)arg;
    int iteration;
    int element;
    int status;
    lmgr_init_thread();
    element = 0;                        /* Current data element */

    for (iteration = 0; iteration < ITERATIONS; iteration++) {
        if ((iteration % self->interval) == 0) {
            status = rwl_writetrylock (&data[element].lock);
            if (status == EBUSY)
                self->w_collisions++;
            else if (status == 0) {
                data[element].data++;
                data[element].updates++;
                self->updates++;
                rwl_writeunlock (&data[element].lock);
            } else
                err_abort (status, _("Try write lock"));
        } else {
            status = rwl_readtrylock (&data[element].lock);
            if (status == EBUSY)
                self->r_collisions++;
            else if (status != 0) {
                err_abort (status, _("Try read lock"));
            } else {
                if (data[element].data != data[element].updates)
                    printf ("%d: data[%d] %d != %d\n",
                        self->thread_num, element,
                        data[element].data, data[element].updates);
                rwl_readunlock (&data[element].lock);
            }
        }

        element++;
        if (element >= DATASIZE)
            element = 0;
    }
    lmgr_cleanup_thread();
    return NULL;
}

int main (int argc, char *argv[])
{
    int count, data_count;
    unsigned int seed = 1;
    int thread_updates = 0, data_updates = 0;
    int status;

#ifdef USE_THR_SETCONCURRENCY
    /*
     * On Solaris 2.5,2.6,7 and 8 threads are not timesliced. To ensure
     * that our threads can run concurrently, we need to
     * increase the concurrency level to THREADS.
     */
    DPRINTF (("Setting concurrency level to %d\n", THREADS));
    thr_setconcurrency (THREADS);
#endif

    /*
     * Initialize the shared data.
     */
    for (data_count = 0; data_count < DATASIZE; data_count++) {
        data[data_count].data = 0;
        data[data_count].updates = 0;
        rwl_init(&data[data_count].lock);
    }

    /*
     * Create THREADS threads to access shared data.
     */
    for (count = 0; count < THREADS; count++) {
        threads[count].thread_num = count;
        threads[count].r_collisions = 0;
        threads[count].w_collisions = 0;
        threads[count].updates = 0;
        threads[count].interval = rand_r (&seed) % ITERATIONS;
        status = pthread_create (&threads[count].thread_id,
            NULL, thread_routine, (void*)&threads[count]);
        if (status != 0)
            err_abort (status, _("Create thread"));
    }

    /*
     * Wait for all threads to complete, and collect
     * statistics.
     */
    for (count = 0; count < THREADS; count++) {
        status = pthread_join (threads[count].thread_id, NULL);
        if (status != 0)
            err_abort (status, _("Join thread"));
        thread_updates += threads[count].updates;
        printf (_("%02d: interval %d, updates %d, "
                "r_collisions %d, w_collisions %d\n"),
            count, threads[count].interval,
            threads[count].updates,
            threads[count].r_collisions, threads[count].w_collisions);
    }

    /*
     * Collect statistics for the data.
     */
    for (data_count = 0; data_count < DATASIZE; data_count++) {
        data_updates += data[data_count].updates;
        printf (_("data %02d: value %d, %d updates\n"),
            data_count, data[data_count].data, data[data_count].updates);
        rwl_destroy (&data[data_count].lock);
    }

    return 0;
}

#endif
