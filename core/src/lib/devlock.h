/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2001-2010 Free Software Foundation Europe e.V.
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
 * Kern Sibbald, January MMI
 * This code adapted from "Programming with POSIX Threads", by
 * David R. Butenhof
 */
/**
 * @file
 * BAREOS Thread Read/Write locking code. It permits
 * multiple readers but only one writer.
 */

#ifndef __DEVLOCK_H
#define __DEVLOCK_H 1

struct take_lock_t {
   pthread_t  writer_id;              /* id of writer */
   int        reason;                 /* save reason */
   int        prev_reason;            /* previous reason */
};


class devlock {
private:
   pthread_mutex_t   mutex;
   pthread_cond_t    read;            /* wait for read */
   pthread_cond_t    write;           /* wait for write */
   pthread_t         writer_id;       /* writer's thread id */
   int               priority;        /* used in deadlock detection */
   int               valid;           /* set when valid */
   int               r_active;        /* readers active */
   int               w_active;        /* writers active */
   int               r_wait;          /* readers waiting */
   int               w_wait;          /* writers waiting */
   int               reason;          /* reason for lock */
   int               prev_reason;     /* previous reason */
   bool              can_take;        /* can the lock be taken? */


public:
   devlock(int reason, bool can_take=false);
   ~devlock();
   int init(int initial_priority);
   int destroy();
   int take_lock(take_lock_t *hold, int reason);
   int return_lock(take_lock_t *hold);
   void new_reason(int nreason) { prev_reason = reason; reason = nreason; }
   void restore_reason() { reason = prev_reason; prev_reason = 0; }

   int writelock(int reason, bool can_take=false);
   int writetrylock();
   int writeunlock();
   void write_release();

   int readunlock();
   int readlock();
   int readtrylock();
   void read_release();

};


#define DEVLOCK_VALID  0xfadbec

#define DEVLOCK_INIIALIZER \
   {PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER, \
    PTHREAD_COND_INITIALIZER, DEVOCK_VALID, 0, 0, 0, 0}

#endif /* __DEVLOCK_H */
