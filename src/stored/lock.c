/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.

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
 * Collection of Bacula Storage daemon locking software
 *
 * Kern Sibbald, June 2007
 */

#include "bareos.h"                   /* pull in global headers */
#include "stored.h"                   /* pull in Storage Deamon headers */

#ifdef SD_DEBUG_LOCK
const int dbglvl = 0;
#else
const int dbglvl = 500;
#endif

/*
 * The Storage daemon has three locking concepts that must be understood:
 *
 * 1. dblock    blocking the device, which means that the device
 *              is "marked" in use.  When setting and removing the
 *              block, the device is locked, but after dblock is
 *              called the device is unlocked.
 * 2. Lock()    simple mutex that locks the device structure. A Lock
 *              can be acquired while a device is blocked if it is not
 *              locked.
 * 3. rLock(locked) "recursive" Lock, when means that a Lock (mutex)
 *                  will be acquired on the device if it is not blocked
 *                  by some other thread. If the device was blocked by
 *                  the current thread, it will acquire the lock.
 *                  If some other thread has set a block on the device,
 *                  this call will wait until the device is unblocked.
 *                  Can be called with locked true, which means the
 *                  Lock is already set
 *
 * - A lock is normally set when modifying the device structure.
 * - A rLock is normally acquired when you want to block the device
 *   i.e. it will wait until the device is not blocked.
 * - A block is normally set during long operations like writing to the device.
 * - If you are writing the device, you will normally block and lock it.
 * - A lock cannot be violated. No other thread can touch the
 *   device while a lock is set.
 * - When a block is set, every thread accept the thread that set
 *   the block will block if rLock is called.
 * - A device can be blocked for multiple reasons, labeling, writing,
 *   acquiring (opening) the device, waiting for the operator, unmounted,
 *   ...
 *
 * Under certain conditions the block that is set on a device can be
 * stolen and the device can be used by another thread. For example,
 * a device is blocked because it is waiting for the operator to
 * mount a tape.  The operator can then unmount the device, and label
 * a tape, re-mount it, give back the block, and the job will continue.
 *
 * Functions:
 *
 * DEVICE::Lock()   does P(m_mutex)     (in dev.h)
 * DEVICE::Unlock() does V(m_mutex)
 *
 * DEVICE::rLock(locked) allows locking the device when this thread
 *                       already has the device blocked.
 * - if (!locked)
 *   Lock()
 * - if blocked and not same thread that locked
 *   pthread_cond_wait
 * - leaves device locked
 *
 * DEVICE::rUnlock() Unlocks but does not unblock same as Unlock();
 *
 * DEVICE::dblock(why) does
 * - rLock();         (recursive device lock)
 * - block_device(this, why)
 * - rUnlock()
 *
 * DEVICE::dunblock does
 * - Lock()
 * - unblock_device()
 * - Unlock()
 *
 * block_device() does (must be locked and not blocked at entry)
 * - set blocked status
 * - set our pid
 *
 * unblock_device() does (must be blocked at entry)
 * - (locked on entry)
 * - (locked on exit)
 * - set unblocked status
 * - clear pid
 * - if waiting threads
 *   pthread_cond_broadcast
 *
 * steal_device_lock() does (must be locked and blocked at entry)
 * - save status
 * - set new blocked status
 * - set new pid
 * - Unlock()
 *
 * give_back_device_lock() does (must be blocked but not locked)
 * - Lock()
 * - reset blocked status
 * - save previous blocked
 * - reset pid
 * - if waiting threads
 *   pthread_cond_broadcast
 */
void DEVICE::dblock(int why)
{
   rLock(false);              /* need recursive lock to block */
   block_device(this, why);
   rUnlock();
}

void DEVICE::dunblock(bool locked)
{
   if (!locked) {
      Lock();
   }
   unblock_device(this);
   Unlock();
}

#ifdef SD_DEBUG_LOCK
/*
 * Debug DCR locks  N.B.
 *
 */
void DCR::dbg_mLock(const char *file, int line, bool locked)
{
   real_P(r_mutex);
   if (is_dev_locked()) {
      real_V(r_mutex);
      return;
   }
   Dmsg3(sd_dbglvl, "mLock %d from %s:%d\n", locked, file, line);
   dev->dbg_rLock(file,line,locked);
   inc_dev_lock();
   real_V(r_mutex);
   return;
}

void DCR::dbg_mUnlock(const char *file, int line)
{
   Dmsg2(sd_dbglvl, "mUnlock from %s:%d\n", file, line);
   real_P(r_mutex);
   if (!is_dev_locked()) {
      real_P(r_mutex);
      ASSERT2(0, "Call on dcr mUnlock when not locked");
      return;
   }
   dec_dev_lock();
   /* When the count goes to zero, unlock it */
   if (!is_dev_locked()) {
      dev->dbg_rUnlock(file,line);
   }
   real_V(r_mutex);
   return;
}

/*
 * Debug DEVICE locks  N.B.
 *
 */
void DEVICE::dbg_Lock(const char *file, int line)
{
   Dmsg3(sd_dbglvl, "Lock from %s:%d precnt=%d\n", file, line, m_count);
   /* Note, this *really* should be protected by a mutex, but
    *  since it is only debug code we don't worry too much.
    */
   if (m_count > 0 && pthread_equal(m_pid, pthread_self())) {
      Dmsg4(sd_dbglvl, "Possible DEADLOCK!! lock held by JobId=%u from %s:%d m_count=%d\n",
            get_jobid_from_tid(m_pid),
            file, line, m_count);
   }
   bthread_mutex_lock_p(&m_mutex, file, line);
   m_pid = pthread_self();
   m_count++;
}

void DEVICE::dbg_Unlock(const char *file, int line)
{
   m_count--;
   Dmsg3(sd_dbglvl, "Unlock from %s:%d postcnt=%d\n", file, line, m_count);
   bthread_mutex_unlock_p(&m_mutex, file, line);
}

void DEVICE::dbg_rUnlock(const char *file, int line)
{
   Dmsg2(sd_dbglvl, "rUnlock from %s:%d\n", file, line);
   dbg_Unlock(file, line);
}

void DEVICE::dbg_Lock_acquire(const char *file, int line)
{
   Dmsg2(sd_dbglvl, "Lock_acquire from %s:%d\n", file, line);
   bthread_mutex_lock_p(&acquire_mutex, file, line);
}

void DEVICE::dbg_Unlock_acquire(const char *file, int line)
{
   Dmsg2(sd_dbglvl, "Unlock_acquire from %s:%d\n", file, line);
   bthread_mutex_unlock_p(&acquire_mutex, file, line);
}

void DEVICE::dbg_Lock_read_acquire(const char *file, int line)
{
   Dmsg2(sd_dbglvl, "Lock_read_acquire from %s:%d\n", file, line);
   bthread_mutex_lock_p(&read_acquire_mutex, file, line);
}

void DEVICE::dbg_Unlock_read_acquire(const char *file, int line)
{
   Dmsg2(sd_dbglvl, "Unlock_read_acquire from %s:%d\n", file, line);
   bthread_mutex_unlock_p(&read_acquire_mutex, file, line);
}

#else

/*
 * DCR locks N.B.
 */
/*
 * Multiple rLock implementation
 */
void DCR::mLock(bool locked)
{
   P(r_mutex);
   if (is_dev_locked()) {
      V(r_mutex);
      return;
   }
   dev->rLock(locked);
   inc_dev_lock();
   V(r_mutex);
   return;
}

/*
 * Multiple rUnlock implementation
 */
void DCR::mUnlock()
{
   P(r_mutex);
   if (!is_dev_locked()) {
      V(r_mutex);
      ASSERT2(0, "Call on dcr mUnlock when not locked");
      return;
   }
   dec_dev_lock();
   /*
    * When the count goes to zero, unlock it
    */
   if (!is_dev_locked()) {
      dev->rUnlock();
   }
   V(r_mutex);
   return;
}

/*
 * DEVICE locks N.B.
 */
void DEVICE::rUnlock()
{
   Unlock();
}

void DEVICE::Lock()
{
   P(m_mutex);
}

void DEVICE::Unlock()
{
   V(m_mutex);
}

void DEVICE::Lock_acquire()
{
   P(acquire_mutex);
}

void DEVICE::Unlock_acquire()
{
   V(acquire_mutex);
}

void DEVICE::Lock_read_acquire()
{
   P(read_acquire_mutex);
}

void DEVICE::Unlock_read_acquire()
{
   V(read_acquire_mutex);
}

#endif

/*
 * Main device access control
 */
int DEVICE::init_mutex()
{
   return pthread_mutex_init(&m_mutex, NULL);
}

/*
 * Write device acquire mutex
 */
int DEVICE::init_acquire_mutex()
{
   return pthread_mutex_init(&acquire_mutex, NULL);
}

/*
 * Read device acquire mutex
 */
int DEVICE::init_read_acquire_mutex()
{
   return pthread_mutex_init(&read_acquire_mutex, NULL);
}

/*
 * Set order in which device locks must be acquired
 */
void DEVICE::set_mutex_priorities()
{
   /*
    * Ensure that we respect this order in P/V operations
    */
   bthread_mutex_set_priority(&m_mutex, PRIO_SD_DEV_ACCESS);
   bthread_mutex_set_priority(&spool_mutex, PRIO_SD_DEV_SPOOL);
   bthread_mutex_set_priority(&acquire_mutex, PRIO_SD_DEV_ACQUIRE);
}

int DEVICE::next_vol_timedwait(const struct timespec *timeout)
{
   return pthread_cond_timedwait(&wait_next_vol, &m_mutex, timeout);
}

/*
 * This is a recursive lock that checks if the device is blocked.
 *
 * When blocked is set, all threads EXCEPT thread with id no_wait_id
 * must wait. The no_wait_id thread is out obtaining a new volume
 * and preparing the label.
 */
#ifdef SD_DEBUG_LOCK
void DEVICE::dbg_rLock(const char *file, int line, bool locked)
{
   Dmsg3(sd_dbglvl, "rLock blked=%s from %s:%d\n", print_blocked(),
         file, line);
   if (!locked) {
      /*
       * lockmgr version of P(m_mutex)
       */
      bthread_mutex_lock_p(&m_mutex, file, line);
      m_count++;
   }
#else
void DEVICE::rLock(bool locked)
{
   if (!locked) {
      Lock();
      m_count++;
   }
#endif

   if (blocked() && !pthread_equal(no_wait_id, pthread_self())) {
      num_waiting++;             /* indicate that I am waiting */
      while (blocked()) {
         int status;
#ifndef HAVE_WIN32
         /*
          * thread id on Win32 may be a struct
          */
         Dmsg3(sd_dbglvl, "rLock blked=%s no_wait=%p me=%p\n", print_blocked(),
               no_wait_id, pthread_self());
#endif
         if ((status = pthread_cond_wait(&this->wait, &m_mutex)) != 0) {
            berrno be;
            this->Unlock();
            Emsg1(M_ABORT, 0, _("pthread_cond_wait failure. ERR=%s\n"),
                  be.bstrerror(status));
         }
      }
      num_waiting--;             /* no longer waiting */
   }
}

/*
 * Block all other threads from using the device
 *
 * Device must already be locked.  After this call,
 * the device is blocked to any thread calling dev->rLock(),
 * but the device is not locked (i.e. no P on device).  Also,
 * the current thread can do slip through the dev->rLock()
 * calls without blocking.
 */
void _block_device(const char *file, int line, DEVICE *dev, int state)
{
// ASSERT(lmgr_mutex_is_locked(&dev->m_mutex) == 1);
   ASSERT(dev->blocked() == BST_NOT_BLOCKED);
   dev->set_blocked(state);           /* make other threads wait */
   dev->no_wait_id = pthread_self();  /* allow us to continue */
   Dmsg3(sd_dbglvl, "set blocked=%s from %s:%d\n", dev->print_blocked(), file, line);
}

/*
 * Unblock the device, and wake up anyone who went to sleep.
 * Enter: device locked
 * Exit:  device locked
 */
void _unblock_device(const char *file, int line, DEVICE *dev)
{
   Dmsg3(sd_dbglvl, "unblock %s from %s:%d\n", dev->print_blocked(), file, line);
// ASSERT(lmgr_mutex_is_locked(&dev->m_mutex) == 1);
   ASSERT(dev->blocked());
   dev->set_blocked(BST_NOT_BLOCKED);
   clear_thread_id(dev->no_wait_id);
   if (dev->num_waiting > 0) {
      pthread_cond_broadcast(&dev->wait); /* wake them up */
   }
}

/*
 * Enter with device locked and blocked
 * Exit with device unlocked and blocked by us.
 */
void _steal_device_lock(const char *file, int line, DEVICE *dev, bsteal_lock_t *hold, int state)
{
   Dmsg3(sd_dbglvl, "steal lock. old=%s from %s:%d\n", dev->print_blocked(),
      file, line);
   hold->dev_blocked = dev->blocked();
   hold->dev_prev_blocked = dev->dev_prev_blocked;
   hold->no_wait_id = dev->no_wait_id;
   dev->set_blocked(state);
   Dmsg1(sd_dbglvl, "steal lock. new=%s\n", dev->print_blocked());
   dev->no_wait_id = pthread_self();
   dev->Unlock();
}

/*
 * Enter with device blocked by us but not locked
 * Exit with device locked, and blocked by previous owner
 */
void _give_back_device_lock(const char *file, int line, DEVICE *dev, bsteal_lock_t *hold)
{
   Dmsg3(sd_dbglvl, "return lock. old=%s from %s:%d\n",
      dev->print_blocked(), file, line);
   dev->Lock();
   dev->set_blocked(hold->dev_blocked);
   dev->dev_prev_blocked = hold->dev_prev_blocked;
   dev->no_wait_id = hold->no_wait_id;
   Dmsg1(sd_dbglvl, "return lock. new=%s\n", dev->print_blocked());
   if (dev->num_waiting > 0) {
      pthread_cond_broadcast(&dev->wait); /* wake them up */
   }
}

const char *DEVICE::print_blocked() const
{
   switch (m_blocked) {
   case BST_NOT_BLOCKED:
      return "BST_NOT_BLOCKED";
   case BST_UNMOUNTED:
      return "BST_UNMOUNTED";
   case BST_WAITING_FOR_SYSOP:
      return "BST_WAITING_FOR_SYSOP";
   case BST_DOING_ACQUIRE:
      return "BST_DOING_ACQUIRE";
   case BST_WRITING_LABEL:
      return "BST_WRITING_LABEL";
   case BST_UNMOUNTED_WAITING_FOR_SYSOP:
      return "BST_UNMOUNTED_WAITING_FOR_SYSOP";
   case BST_MOUNT:
      return "BST_MOUNT";
   case BST_DESPOOLING:
      return "BST_DESPOOLING";
   case BST_RELEASING:
      return "BST_RELEASING";
   default:
      return _("unknown blocked code");
   }
}

/*
 * Check if the device is blocked or not
 */
bool DEVICE::is_device_unmounted()
{
   bool status;
   int blk = blocked();

   status = (blk == BST_UNMOUNTED) ||
            (blk == BST_UNMOUNTED_WAITING_FOR_SYSOP);

   return status;
}
