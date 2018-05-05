/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2001-2011 Free Software Foundation Europe e.V.

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
 * BAREOS Thread Read/Write locking code. It permits
 * multiple readers but only one writer.  Note, however,
 * that the writer thread is permitted to make multiple
 * nested write lock calls.
 *
 * Kern Sibbald, January MMI
 *
 * This code adapted from "Programming with POSIX Threads", by David R. Butenhof
 */

#define _LOCKMGR_COMPLIANT
#include "include/bareos.h"

/*
 * Initialize a read/write lock
 *
 *  Returns: 0 on success
 *           errno on failure
 */
int RwlInit(brwlock_t *rwl, int priority)
{
   int status;

   rwl->r_active = rwl->w_active = 0;
   rwl->r_wait = rwl->w_wait = 0;
   rwl->priority = priority;
   if ((status = pthread_mutex_init(&rwl->mutex, NULL)) != 0) {
      return status;
   }
   if ((status = pthread_cond_init(&rwl->read, NULL)) != 0) {
      pthread_mutex_destroy(&rwl->mutex);
      return status;
   }
   if ((status = pthread_cond_init(&rwl->write, NULL)) != 0) {
      pthread_cond_destroy(&rwl->read);
      pthread_mutex_destroy(&rwl->mutex);
      return status;
   }
   rwl->valid = RWLOCK_VALID;
   return 0;
}

/*
 * Destroy a read/write lock
 *
 * Returns: 0 on success
 *          errno on failure
 */
int RwlDestroy(brwlock_t *rwl)
{
   int status, status1, status2;

   if (rwl->valid != RWLOCK_VALID) {
      return EINVAL;
   }
   if ((status = pthread_mutex_lock(&rwl->mutex)) != 0) {
      return status;
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
   if ((status = pthread_mutex_unlock(&rwl->mutex)) != 0) {
      return status;
   }
   status = pthread_mutex_destroy(&rwl->mutex);
   status1 = pthread_cond_destroy(&rwl->read);
   status2 = pthread_cond_destroy(&rwl->write);
   return (status != 0 ? status : (status1 != 0 ? status1 : status2));
}

bool RwlIsInit(brwlock_t *rwl)
{
   return (rwl->valid == RWLOCK_VALID);
}

/*
 * Handle cleanup when the read lock condition variable
 * wait is released.
 */
static void RwlReadRelease(void *arg)
{
   brwlock_t *rwl = (brwlock_t *)arg;

   rwl->r_wait--;
   pthread_mutex_unlock(&rwl->mutex);
}

/*
 * Handle cleanup when the write lock condition variable wait
 * is released.
 */
static void RwlWriteRelease(void *arg)
{
   brwlock_t *rwl = (brwlock_t *)arg;

   rwl->w_wait--;
   pthread_mutex_unlock(&rwl->mutex);
}

/*
 * Lock for read access, wait until locked (or error).
 */
int RwlReadlock(brwlock_t *rwl)
{
   int status;

   if (rwl->valid != RWLOCK_VALID) {
      return EINVAL;
   }
   if ((status = pthread_mutex_lock(&rwl->mutex)) != 0) {
      return status;
   }
   if (rwl->w_active) {
      rwl->r_wait++;                  /* indicate that we are waiting */
      pthread_cleanup_push(RwlReadRelease, (void *)rwl);
      while (rwl->w_active) {
         status = pthread_cond_wait(&rwl->read, &rwl->mutex);
         if (status != 0) {
            break;                    /* error, bail out */
         }
      }
      pthread_cleanup_pop(0);
      rwl->r_wait--;                  /* we are no longer waiting */
   }
   if (status == 0) {
      rwl->r_active++;                /* we are running */
   }
   pthread_mutex_unlock(&rwl->mutex);
   return status;
}

/*
 * Attempt to lock for read access, don't wait
 */
int RwlReadtrylock(brwlock_t *rwl)
{
   int status, status2;

   if (rwl->valid != RWLOCK_VALID) {
      return EINVAL;
   }
   if ((status = pthread_mutex_lock(&rwl->mutex)) != 0) {
      return status;
   }
   if (rwl->w_active) {
      status = EBUSY;
   } else {
      rwl->r_active++;                /* we are running */
   }
   status2 = pthread_mutex_unlock(&rwl->mutex);
   return (status == 0 ? status2 : status);
}

/*
 * Unlock read lock
 */
int RwlReadunlock(brwlock_t *rwl)
{
   int status, status2;

   if (rwl->valid != RWLOCK_VALID) {
      return EINVAL;
   }
   if ((status = pthread_mutex_lock(&rwl->mutex)) != 0) {
      return status;
   }
   rwl->r_active--;
   if (rwl->r_active == 0 && rwl->w_wait > 0) { /* if writers waiting */
      status = pthread_cond_broadcast(&rwl->write);
   }
   status2 = pthread_mutex_unlock(&rwl->mutex);
   return (status == 0 ? status2 : status);
}


/*
 * Lock for write access, wait until locked (or error).
 *   Multiple nested write locking is permitted.
 */
int RwlWritelock_p(brwlock_t *rwl, const char *file, int line)
{
   int status;

   if (rwl->valid != RWLOCK_VALID) {
      return EINVAL;
   }
   if ((status = pthread_mutex_lock(&rwl->mutex)) != 0) {
      return status;
   }
   if (rwl->w_active && pthread_equal(rwl->writer_id, pthread_self())) {
      rwl->w_active++;
      pthread_mutex_unlock(&rwl->mutex);
      return 0;
   }
   LmgrPreLock(rwl, rwl->priority, file, line);
   if (rwl->w_active || rwl->r_active > 0) {
      rwl->w_wait++;                  /* indicate that we are waiting */
      pthread_cleanup_push(RwlWriteRelease, (void *)rwl);
      while (rwl->w_active || rwl->r_active > 0) {
         if ((status = pthread_cond_wait(&rwl->write, &rwl->mutex)) != 0) {
            LmgrDoUnlock(rwl);
            break;                    /* error, bail out */
         }
      }
      pthread_cleanup_pop(0);
      rwl->w_wait--;                  /* we are no longer waiting */
   }
   if (status == 0) {
      rwl->w_active++;                /* we are running */
      rwl->writer_id = pthread_self(); /* save writer thread's id */
      LmgrPostLock();
   }
   pthread_mutex_unlock(&rwl->mutex);
   return status;
}

/*
 * Attempt to lock for write access, don't wait
 */
int RwlWritetrylock(brwlock_t *rwl)
{
   int status, status2;

   if (rwl->valid != RWLOCK_VALID) {
      return EINVAL;
   }
   if ((status = pthread_mutex_lock(&rwl->mutex)) != 0) {
      return status;
   }
   if (rwl->w_active && pthread_equal(rwl->writer_id, pthread_self())) {
      rwl->w_active++;
      pthread_mutex_unlock(&rwl->mutex);
      return 0;
   }
   if (rwl->w_active || rwl->r_active > 0) {
      status = EBUSY;
   } else {
      rwl->w_active = 1;              /* we are running */
      rwl->writer_id = pthread_self(); /* save writer thread's id */
      LmgrDoLock(rwl, rwl->priority, __FILE__, __LINE__);
   }
   status2 = pthread_mutex_unlock(&rwl->mutex);
   return (status == 0 ? status2 : status);
}

/*
 * Unlock write lock
 *  Start any waiting writers in preference to waiting readers
 */
int RwlWriteunlock(brwlock_t *rwl)
{
   int status, status2;

   if (rwl->valid != RWLOCK_VALID) {
      return EINVAL;
   }
   if ((status = pthread_mutex_lock(&rwl->mutex)) != 0) {
      return status;
   }
   if (rwl->w_active <= 0) {
      pthread_mutex_unlock(&rwl->mutex);
      Jmsg0(NULL, M_ABORT, 0, _("RwlWriteunlock called too many times.\n"));
   }
   rwl->w_active--;
   if (!pthread_equal(pthread_self(), rwl->writer_id)) {
      pthread_mutex_unlock(&rwl->mutex);
      Jmsg0(NULL, M_ABORT, 0, _("RwlWriteunlock by non-owner.\n"));
   }
   if (rwl->w_active > 0) {
      status = 0;                       /* writers still active */
   } else {
      LmgrDoUnlock(rwl);
      /* No more writers, awaken someone */
      if (rwl->r_wait > 0) {         /* if readers waiting */
         status = pthread_cond_broadcast(&rwl->read);
      } else if (rwl->w_wait > 0) {
         status = pthread_cond_broadcast(&rwl->write);
      }
   }
   status2 = pthread_mutex_unlock(&rwl->mutex);
   return (status == 0 ? status2 : status);
}
