/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2000-2007 Free Software Foundation Europe e.V.

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
 * Definitions for locking and blocking functions in the SD
 *
 * Kern Sibbald, pulled out of dev.h June 2007
 *
 *   Version $Id$
 *
 */


#ifndef __LOCK_H
#define __LOCK_H 1

#ifdef SD_DEBUG_LOCK
#define r_dlock()   _r_dlock(__FILE__, __LINE__);    /* in lock.c */
#define r_dunlock() _r_dunlock(__FILE__, __LINE__);  /* in lock.c */
#define dlock()     _dlock(__FILE__, __LINE__);      /* in lock.c */
#define dunlock()   _dunlock(__FILE__, __LINE__);    /* in lock.c */
#endif

#define block_device(d, s)          _block_device(__FILE__, __LINE__, (d), s)
#define unblock_device(d)           _unblock_device(__FILE__, __LINE__, (d))
#define steal_device_lock(d, p, s)  _steal_device_lock(__FILE__, __LINE__, (d), (p), s)
#define give_back_device_lock(d, p) _give_back_device_lock(__FILE__, __LINE__, (d), (p))

/* m_blocked states (mutually exclusive) */
enum {
   BST_NOT_BLOCKED = 0,               /* not blocked */
   BST_UNMOUNTED,                     /* User unmounted device */
   BST_WAITING_FOR_SYSOP,             /* Waiting for operator to mount tape */
   BST_DOING_ACQUIRE,                 /* Opening/validating/moving tape */
   BST_WRITING_LABEL,                 /* Labeling a tape */
   BST_UNMOUNTED_WAITING_FOR_SYSOP,   /* User unmounted during wait for op */
   BST_MOUNT,                         /* Mount request */
   BST_DESPOOLING,                    /* Despooling -- i.e. multiple writes */
   BST_RELEASING                      /* Releasing the device */
};

typedef struct s_steal_lock {
   pthread_t  no_wait_id;             /* id of no wait thread */
   int        dev_blocked;            /* state */
   int        dev_prev_blocked;       /* previous blocked state */
} bsteal_lock_t;

/*
 * Used in unblock() call
 */
enum {
   DEV_LOCKED = true,
   DEV_UNLOCKED = false
};

#endif
