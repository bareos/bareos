/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2016-2016 Bareos GmbH & Co. KG

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
 * Kern Sibbald, June 2007
 */
/*+
 * @file
 * Collection of Bacula Storage daemon locking software
 */

#include "include/bareos.h"                   /* pull in global headers */
#include "stored/stored.h"                   /* pull in Storage Daemon headers */
#include "lib/edit.h"

namespace storagedaemon {

/**
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
 * Device::Lock()   does P(mutex_)     (in dev.h)
 * Device::Unlock() does V(mutex_)
 *
 * Device::rLock(locked) allows locking the device when this thread
 *                       already has the device blocked.
 * - if (!locked)
 *   Lock()
 * - if blocked and not same thread that locked
 *   pthread_cond_wait
 * - leaves device locked
 *
 * Device::rUnlock() Unlocks but does not unblock same as Unlock();
 *
 * Device::dblock(why) does
 * - rLock();         (recursive device lock)
 * - BlockDevice(this, why)
 * - rUnlock()
 *
 * Device::dunblock does
 * - Lock()
 * - UnblockDevice()
 * - Unlock()
 *
 * BlockDevice() does (must be locked and not blocked at entry)
 * - set blocked status
 * - set our pid
 *
 * UnblockDevice() does (must be blocked at entry)
 * - (locked on entry)
 * - (locked on exit)
 * - set unblocked status
 * - clear pid
 * - if waiting threads
 *   pthread_cond_broadcast
 *
 * StealDeviceLock() does (must be locked and blocked at entry)
 * - save status
 * - set new blocked status
 * - set new pid
 * - Unlock()
 *
 * GiveBackDeviceLock() does (must be blocked but not locked)
 * - Lock()
 * - reset blocked status
 * - save previous blocked
 * - reset pid
 * - if waiting threads
 *   pthread_cond_broadcast
 */
void Device::dblock(int why)
{
   rLock(false);              /* need recursive lock to block */
   BlockDevice(this, why);
   rUnlock();
}

void Device::dunblock(bool locked)
{
   if (!locked) {
      Lock();
   }
   UnblockDevice(this);
   Unlock();
}

#ifdef SD_DEBUG_LOCK
/**
 * Debug DeviceControlRecord locks  N.B.
 *
 */
void DeviceControlRecord::dbg_mLock(const char *file, int line, bool locked)
{
   real_P(r_mutex);
   if (IsDevLocked()) {
      real_V(r_mutex);
      return;
   }
   Dmsg3(sd_debuglevel, "mLock %d from %s:%d\n", locked, file, line);
   dev->dbg_rLock(file,line,locked);
   IncDevLock();
   real_V(r_mutex);
   return;
}

void DeviceControlRecord::dbg_mUnlock(const char *file, int line)
{
   Dmsg2(sd_debuglevel, "mUnlock from %s:%d\n", file, line);
   real_P(r_mutex);
   if (!IsDevLocked()) {
      real_P(r_mutex);
      ASSERT2(0, "Call on dcr mUnlock when not locked");
      return;
   }
   DecDevLock();
   /* When the count goes to zero, unlock it */
   if (!IsDevLocked()) {
      dev->dbg_rUnlock(file,line);
   }
   real_V(r_mutex);
   return;
}

/**
 * Debug Device locks  N.B.
 *
 */
void Device::dbg_Lock(const char *file, int line)
{
   Dmsg3(sd_debuglevel, "Lock from %s:%d precnt=%d\n", file, line, count_);
   /* Note, this *really* should be protected by a mutex, but
    *  since it is only debug code we don't worry too much.
    */
   if (count_ > 0 && pthread_equal(pid_, pthread_self())) {
      Dmsg4(sd_debuglevel, "Possible DEADLOCK!! lock held by JobId=%u from %s:%d count_=%d\n",
            GetJobidFromTid(pid_),
            file, line, count_);
   }
   pthread_mutex_lock(&mutex_);
   pid_ = pthread_self();
   count_++;
}

void Device::dbg_Unlock(const char *file, int line)
{
   count_--;
   Dmsg3(sd_debuglevel, "Unlock from %s:%d postcnt=%d\n", file, line, count_);
   pthread_mutex_unlock(&mutex_);
}

void Device::dbg_rUnlock(const char *file, int line)
{
   Dmsg2(sd_debuglevel, "rUnlock from %s:%d\n", file, line);
   dbg_Unlock(file, line);
}

void Device::dbg_Lock_acquire(const char *file, int line)
{
   Dmsg2(sd_debuglevel, "Lock_acquire from %s:%d\n", file, line);
   pthread_mutex_lock(&acquire_mutex);
}

void Device::dbg_Unlock_acquire(const char *file, int line)
{
   Dmsg2(sd_debuglevel, "Unlock_acquire from %s:%d\n", file, line);
   pthread_mutex_unlock(&acquire_mutex);
}

void Device::dbg_Lock_read_acquire(const char *file, int line)
{
   Dmsg2(sd_debuglevel, "Lock_read_acquire from %s:%d\n", file, line);
   pthread_mutex_lock(&read_acquire_mutex);
}

void Device::dbg_Unlock_read_acquire(const char *file, int line)
{
   Dmsg2(sd_debuglevel, "Unlock_read_acquire from %s:%d\n", file, line);
   pthread_mutex_unlock(&read_acquire_mutex);
}

#else

/**
 * DeviceControlRecord locks N.B.
 */
/**
 * Multiple rLock implementation
 */
void DeviceControlRecord::mLock(bool locked)
{
   P(r_mutex);
   if (IsDevLocked()) {
      V(r_mutex);
      return;
   }
   dev->rLock(locked);
   IncDevLock();
   V(r_mutex);
   return;
}

/**
 * Multiple rUnlock implementation
 */
void DeviceControlRecord::mUnlock()
{
   P(r_mutex);
   if (!IsDevLocked()) {
      V(r_mutex);
      ASSERT2(0, "Call on dcr mUnlock when not locked");
      return;
   }
   DecDevLock();
   /*
    * When the count goes to zero, unlock it
    */
   if (!IsDevLocked()) {
      dev->rUnlock();
   }
   V(r_mutex);
   return;
}

/**
 * Device locks N.B.
 */
void Device::rUnlock()
{
   Unlock();
}

void Device::Lock()
{
   P(mutex_);
}

void Device::Unlock()
{
   V(mutex_);
}

void Device::Lock_acquire()
{
   P(acquire_mutex);
}

void Device::Unlock_acquire()
{
   V(acquire_mutex);
}

void Device::Lock_read_acquire()
{
   P(read_acquire_mutex);
}

void Device::Unlock_read_acquire()
{
   V(read_acquire_mutex);
}

#endif

/**
 * Main device access control
 */
int Device::InitMutex()
{
   return pthread_mutex_init(&mutex_, NULL);
}

/**
 * Write device acquire mutex
 */
int Device::InitAcquireMutex()
{
   return pthread_mutex_init(&acquire_mutex, NULL);
}

/**
 * Read device acquire mutex
 */
int Device::InitReadAcquireMutex()
{
   return pthread_mutex_init(&read_acquire_mutex, NULL);
}

int Device::NextVolTimedwait(const struct timespec *timeout)
{
   return pthread_cond_timedwait(&wait_next_vol, &mutex_, timeout);
}

/**
 * This is a recursive lock that checks if the device is blocked.
 *
 * When blocked is set, all threads EXCEPT thread with id no_wait_id
 * must wait. The no_wait_id thread is out obtaining a new volume
 * and preparing the label.
 */
#ifdef SD_DEBUG_LOCK
void Device::dbg_rLock(const char *file, int line, bool locked)
{
   Dmsg3(sd_debuglevel, "rLock blked=%s from %s:%d\n", print_blocked(),
         file, line);
   if (!locked) {
      pthread_mutex_lock(&mutex);
      count_++;
   }
#else
void Device::rLock(bool locked)
{
   if (!locked) {
      Lock();
      count_++;
   }
#endif

   if (blocked() && !pthread_equal(no_wait_id, pthread_self())) {
      num_waiting++;             /* indicate that I am waiting */
      while (blocked()) {
         int status;
         char ed1[50], ed2[50];

         Dmsg3(sd_debuglevel, "rLock blked=%s no_wait=%s me=%s\n",
               print_blocked(),
               edit_pthread(no_wait_id, ed1, sizeof(ed1)),
               edit_pthread(pthread_self(), ed2, sizeof(ed2)));
         if ((status = pthread_cond_wait(&wait, &mutex_)) != 0) {
            BErrNo be;
            this->Unlock();
            Emsg1(M_ABORT, 0, _("pthread_cond_wait failure. ERR=%s\n"),
                  be.bstrerror(status));
         }
      }
      num_waiting--;             /* no longer waiting */
   }
}

/**
 * Block all other threads from using the device
 *
 * Device must already be locked.  After this call,
 * the device is blocked to any thread calling dev->rLock(),
 * but the device is not locked (i.e. no P on device).  Also,
 * the current thread can do slip through the dev->rLock()
 * calls without blocking.
 */
void _blockDevice(const char *file, int line, Device *dev, int state)
{
   ASSERT(dev->blocked() == BST_NOT_BLOCKED);
   dev->SetBlocked(state);           /* make other threads wait */
   dev->no_wait_id = pthread_self();  /* allow us to continue */
   Dmsg3(sd_debuglevel, "set blocked=%s from %s:%d\n", dev->print_blocked(), file, line);
}

/**
 * Unblock the device, and wake up anyone who went to sleep.
 * Enter: device locked
 * Exit:  device locked
 */
void _unBlockDevice(const char *file, int line, Device *dev)
{
   Dmsg3(sd_debuglevel, "unblock %s from %s:%d\n", dev->print_blocked(), file, line);
   ASSERT(dev->blocked());
   dev->SetBlocked(BST_NOT_BLOCKED);
   ClearThreadId(dev->no_wait_id);
   if (dev->num_waiting > 0) {
      pthread_cond_broadcast(&dev->wait); /* wake them up */
   }
}

/**
 * Enter with device locked and blocked
 * Exit with device unlocked and blocked by us.
 */
void _stealDeviceLock(const char *file, int line, Device *dev, bsteal_lock_t *hold, int state)
{
   Dmsg3(sd_debuglevel, "steal lock. old=%s from %s:%d\n", dev->print_blocked(),
      file, line);
   hold->dev_blocked = dev->blocked();
   hold->dev_prev_blocked = dev->dev_prev_blocked;
   hold->no_wait_id = dev->no_wait_id;
   dev->SetBlocked(state);
   Dmsg1(sd_debuglevel, "steal lock. new=%s\n", dev->print_blocked());
   dev->no_wait_id = pthread_self();
   dev->Unlock();
}

/**
 * Enter with device blocked by us but not locked
 * Exit with device locked, and blocked by previous owner
 */
void _giveBackDeviceLock(const char *file, int line, Device *dev, bsteal_lock_t *hold)
{
   Dmsg3(sd_debuglevel, "return lock. old=%s from %s:%d\n",
      dev->print_blocked(), file, line);
   dev->Lock();
   dev->SetBlocked(hold->dev_blocked);
   dev->dev_prev_blocked = hold->dev_prev_blocked;
   dev->no_wait_id = hold->no_wait_id;
   Dmsg1(sd_debuglevel, "return lock. new=%s\n", dev->print_blocked());
   if (dev->num_waiting > 0) {
      pthread_cond_broadcast(&dev->wait); /* wake them up */
   }
}

const char *Device::print_blocked() const
{
   switch (blocked_) {
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

/**
 * Check if the device is blocked or not
 */
bool Device::IsDeviceUnmounted()
{
   bool status;
   int blk = blocked();

   status = (blk == BST_UNMOUNTED) ||
            (blk == BST_UNMOUNTED_WAITING_FOR_SYSOP);

   return status;
}

} /* namespace storagedaemon */
