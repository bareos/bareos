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
 * This code adapted from "Programming with POSIX Threads", by
 * David R. Butenhof
 */

#include "include/bareos.h"
#include "lib/devlock.h"

/*
 * Initialize a read/write lock
 *
 *  Returns: 0 on success
 *           errno on failure
 */

DevLock *DevLock::new_devlock()
{
   DevLock *lock;
   lock = (DevLock *)malloc(sizeof (DevLock));
   lock->mutex = PTHREAD_MUTEX_INITIALIZER;
   lock->read = PTHREAD_COND_INITIALIZER;
   lock->write = PTHREAD_COND_INITIALIZER;
   lock->writer_id = 0;
   lock->priority = 0;
   lock->valid = 0;
   lock->r_active = 0;
   lock->w_active = 0;
   lock->r_wait = 0;
   lock->w_wait = 0;
   lock->reason = 0;
   lock->prev_reason = 0;
   lock->can_take = false;
   return lock;
}

int DevLock::init(int initial_priority)
{
   int status;
   DevLock *rwl = this;

   rwl->r_active = rwl->w_active = 0;
   rwl->r_wait = rwl->w_wait = 0;
   rwl->priority = initial_priority;
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
   rwl->valid = DEVLOCK_VALID;
   return 0;
}

/*
 * Destroy a read/write lock
 *
 * Returns: 0 on success
 *          errno on failure
 */
int DevLock::destroy()
{
   DevLock *rwl = this;
   int status, status1, status2;

   if (rwl->valid != DEVLOCK_VALID) {
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

/*
 * Handle cleanup when the read lock condition variable
 * wait is released.
 */
static void DevlockReadRelease(void *arg)
{
   DevLock *rwl = (DevLock *)arg;
   rwl->ReadRelease();
}

void DevLock::ReadRelease()
{
   r_wait--;
   pthread_mutex_unlock(&mutex);
}

/*
 * Handle cleanup when the write lock condition variable wait
 * is released.
 */
static void DevlockWriteRelease(void *arg)
{
   DevLock *rwl = (DevLock *)arg;
   rwl->WriteRelease();
}

void DevLock::WriteRelease()
{
   w_wait--;
   pthread_mutex_unlock(&mutex);
}

/*
 * Lock for read access, wait until locked (or error).
 */
int DevLock::readlock()
{
   DevLock *rwl = this;
   int status;

   if (rwl->valid != DEVLOCK_VALID) {
      return EINVAL;
   }
   if ((status = pthread_mutex_lock(&rwl->mutex)) != 0) {
      return status;
   }
   if (rwl->w_active) {
      rwl->r_wait++;                  /* indicate that we are waiting */
      pthread_cleanup_push(DevlockReadRelease, (void *)rwl);
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
int DevLock::readtrylock()
{
   DevLock *rwl = this;
   int status, status2;

   if (rwl->valid != DEVLOCK_VALID) {
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
int DevLock::readunlock()
{
   DevLock *rwl = this;
   int status, status2;

   if (rwl->valid != DEVLOCK_VALID) {
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
int DevLock::Writelock(int areason, bool acan_take)
{
   DevLock *rwl = this;
   int status;

   if (rwl->valid != DEVLOCK_VALID) {
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
      rwl->w_wait++;                  /* indicate that we are waiting */
      pthread_cleanup_push(DevlockWriteRelease, (void *)rwl);
      while (rwl->w_active || rwl->r_active > 0) {
         if ((status = pthread_cond_wait(&rwl->write, &rwl->mutex)) != 0) {
            break;                    /* error, bail out */
         }
      }
      pthread_cleanup_pop(0);
      rwl->w_wait--;                  /* we are no longer waiting */
   }
   if (status == 0) {
      rwl->w_active++;                /* we are running */
      rwl->writer_id = pthread_self(); /* save writer thread's id */
   }
   rwl->reason = areason;
   rwl->can_take = acan_take;
   pthread_mutex_unlock(&rwl->mutex);
   return status;
}

/*
 * Attempt to lock for write access, don't wait
 */
int DevLock::writetrylock()
{
   DevLock *rwl = this;
   int status, status2;

   if (rwl->valid != DEVLOCK_VALID) {
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
   }
   status2 = pthread_mutex_unlock(&rwl->mutex);
   return (status == 0 ? status2 : status);
}

/*
 * Unlock write lock
 *  Start any waiting writers in preference to waiting readers
 */
int DevLock::writeunlock()
{
   DevLock *rwl = this;
   int status, status2;

   if (rwl->valid != DEVLOCK_VALID) {
      return EINVAL;
   }
   if ((status = pthread_mutex_lock(&rwl->mutex)) != 0) {
      return status;
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
      status = 0;                       /* writers still active */
   } else {
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

int DevLock::TakeLock(take_lock_t *hold, int areason)
{
   int status;

   if (valid != DEVLOCK_VALID) {
      return EINVAL;
   }
   if ((status = pthread_mutex_lock(&mutex)) != 0) {
      return status;
   }
   hold->reason = reason;
   hold->prev_reason = prev_reason;
   hold->writer_id = writer_id;
   reason = areason;
   writer_id = pthread_self();
   status = pthread_mutex_unlock(&mutex);
   return status;
}

int DevLock::ReturnLock(take_lock_t *hold)
{
   int status, status2;

   if (valid != DEVLOCK_VALID) {
      return EINVAL;
   }
   if ((status = pthread_mutex_lock(&mutex)) != 0) {
      return status;
   }
   reason = hold->reason;
   prev_reason = hold->prev_reason;
   writer_id = hold->writer_id;
   writer_id = pthread_self();
   status2 = pthread_mutex_unlock(&mutex);
   if (w_active || w_wait) {
      status = pthread_cond_broadcast(&write);
   }
   return (status == 0 ? status2 : status);

}
