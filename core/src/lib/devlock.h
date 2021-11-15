/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2001-2010 Free Software Foundation Europe e.V.
   Copyright (C) 2016-2021 Bareos GmbH & Co. KG

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
 * Kern Sibbald, January MMI
 * This code adapted from "Programming with POSIX Threads", by
 * David R. Butenhof
 */
/**
 * @file
 * BAREOS Thread Read/Write locking code. It permits
 * multiple readers but only one writer.
 */

#ifndef BAREOS_LIB_DEVLOCK_H_
#define BAREOS_LIB_DEVLOCK_H_

struct take_lock_t {
  pthread_t writer_id; /* id of writer */
  int reason;          /* save reason */
  int prev_reason;     /* previous reason */
};


class DevLock {
 private:
  pthread_mutex_t mutex;
  pthread_cond_t read;  /* wait for read */
  pthread_cond_t write; /* wait for write */
  pthread_t writer_id;  /* writer's thread id */
  int priority;         /* used in deadlock detection */
  int valid;            /* set when valid */
  int r_active;         /* readers active */
  int w_active;         /* writers active */
  int r_wait;           /* readers waiting */
  int w_wait;           /* writers waiting */
  int reason;           /* reason for lock */
  int prev_reason;      /* previous reason */
  bool can_take;        /* can the lock be taken? */


 public:
  DevLock(int reason, bool can_take = false);
  ~DevLock();
  static DevLock* new_devlock();
  int init(int initial_priority);
  int destroy();
  int TakeLock(take_lock_t* hold, int reason);
  int ReturnLock(take_lock_t* hold);
  void NewReason(int nreason)
  {
    prev_reason = reason;
    reason = nreason;
  }
  void restore_reason()
  {
    reason = prev_reason;
    prev_reason = 0;
  }

  int Writelock(int reason, bool can_take = false);
  int writetrylock();
  int writeunlock();
  void WriteRelease();

  int readunlock();
  int readlock();
  int readtrylock();
  void ReadRelease();
};


#define DEVLOCK_VALID 0xfadbec

#define DEVLOCK_INIIALIZER                                 \
  {                                                        \
    PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER,   \
        PTHREAD_COND_INITIALIZER, DEVOCK_VALID, 0, 0, 0, 0 \
  }

#endif  // BAREOS_LIB_DEVLOCK_H_
