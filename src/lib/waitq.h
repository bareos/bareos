/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2001-2009 Free Software Foundation Europe e.V.
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
 * Kern Sibbald, March MMI
 * This code inspired from "Programming with POSIX Threads", by
 * David R. Butenhof
 */
/**
 * @file
 * BAREOS wait queue routines. Permits waiting for something
 * to be done. I.e. for operator to mount new volume.
 */

#ifndef __WAITQ_H
#define __WAITQ_H 1

/**
 * Structure to keep track of wait queue request
 */
typedef struct waitq_ele_tag {
   struct waitq_ele_tag *next;
   int               done_flag;       /* predicate for wait */
   pthread_cont_t    done;            /* wait for completion */
   void             *msg;             /* message to be passed */
} waitq_ele_t;

/**
 * Structure describing a wait queue
 */
typedef struct workq_tag {
   pthread_mutex_t   mutex;           /* queue access control */
   pthread_cond_t    wait_req;        /* wait for OK */
   int               num_msgs;        /* number of waiters */
   waitq_ele_t       *first;          /* wait queue first item */
   waitq_ele_t       *last;           /* wait queue last item */
} workq_t;

extern int waitq_init(waitq_t *wq);
extern int waitq_destroy(waitq_t *wq);
extern int waitq_add(waitq_t *wq, void *msg);

#endif /* __WAITQ_H */
