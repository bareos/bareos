/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2003-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2015-2017 Bareos GmbH & Co. KG

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
/* originally was Kern Sibbald, January MMI */
/*
 * extracted the TEST_PROGRAM functionality from the files in ..
 * and adapted for gtest
 *
 * Philipp Storz, November 2017
 */
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>

extern "C" {
#include <cmocka.h>
}
#include "include/bareos.h"

//#define TEST_RW_TRY_LOCK
//#define TEST_RWLOCK

#ifndef TEST_RW_TRY_LOCK
#ifndef TEST_RWLOCK
void test_rwlock(void **state) {
   (void) state; /* unused */
}
#endif
#endif



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
         status = RwlWritelock(&data[element].lock);
         if (status != 0) {
            berrno be;
            printf("Write lock failed. ERR=%s\n", be.bstrerror(status));
            exit(1);
         }
         data[element].data = self->thread_num;
         data[element].writes++;
         self->writes++;
         status = RwlWritelock(&data[element].lock);
         if (status != 0) {
            berrno be;
            printf("Write lock failed. ERR=%s\n", be.bstrerror(status));
            exit(1);
         }
         data[element].data = self->thread_num;
         data[element].writes++;
         self->writes++;
         status = RwlWriteunlock(&data[element].lock);
         if (status != 0) {
            berrno be;
            printf("Write unlock failed. ERR=%s\n", be.bstrerror(status));
            exit(1);
         }
         status = RwlWriteunlock(&data[element].lock);
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
          status = RwlReadlock(&data[element].lock);
          if (status != 0) {
             berrno be;
             printf("Read lock failed. ERR=%s\n", be.bstrerror(status));
             exit(1);
          }
          self->reads++;
          if (data[element].data == self->thread_num)
             repeats++;
          status = RwlReadunlock(&data[element].lock);
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

void test_rwlock(void **state) {
   (void) state; /* unused */
//int main (int argc, char *argv[])
//{
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
    ThrSetconcurrency (THREADS);
#endif

    /*
     * Initialize the shared data.
     */
    for (data_count = 0; data_count < DATASIZE; data_count++) {
        data[data_count].data = 0;
        data[data_count].writes = 0;
        status = RwlInit(&data[data_count].lock);
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
        RwlDestroy (&data[data_count].lock);
    }

    printf (_("Total: %d thread writes, %d data writes\n"),
        thread_writes, data_writes);
}

#endif

#ifdef TEST_RW_TRY_LOCK
/*
 * brwlock_try_main.c
 *
 * Demonstrate use of non-blocking read-write locks.
 *
 * Special notes: On older Solaris system, call ThrSetconcurrency()
 * to allow interleaved thread execution, since threads are not
 * timesliced.
 */
#include <pthread.h>
#include "../rwlock.h"
//#include "errors.h"

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
    LmgrInitThread();
    element = 0;                        /* Current data element */

    for (iteration = 0; iteration < ITERATIONS; iteration++) {
        if ((iteration % self->interval) == 0) {
            status = RwlWritetrylock (&data[element].lock);
            if (status == EBUSY)
                self->w_collisions++;
            else if (status == 0) {
                data[element].data++;
                data[element].updates++;
                self->updates++;
                RwlWriteunlock (&data[element].lock);
            } else
                err_abort (status, _("Try write lock"));
        } else {
            status = RwlReadtrylock (&data[element].lock);
            if (status == EBUSY)
                self->r_collisions++;
            else if (status != 0) {
                err_abort (status, _("Try read lock"));
            } else {
                if (data[element].data != data[element].updates)
                    printf ("%d: data[%d] %d != %d\n",
                        self->thread_num, element,
                        data[element].data, data[element].updates);
                RwlReadunlock (&data[element].lock);
            }
        }

        element++;
        if (element >= DATASIZE)
            element = 0;
    }
    LmgrCleanupThread();
    return NULL;
}



void test_rwlock(void **state) {
   (void) state; /* unused */


//int main (int argc, char *argv[])
//{
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
    ThrSetconcurrency (THREADS);
#endif

    /*
     * Initialize the shared data.
     */
    for (data_count = 0; data_count < DATASIZE; data_count++) {
        data[data_count].data = 0;
        data[data_count].updates = 0;
        RwlInit(&data[data_count].lock);
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
        RwlDestroy (&data[data_count].lock);
    }

    return 0;
}

#endif
