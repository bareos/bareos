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
 * This code adapted from "Programming with POSIX Threads", by David R. Butenhof
 */
/**
 * @file
 * BAREOS Thread Read/Write locking code. It permits
 * multiple readers but only one writer.
 */

#ifndef BAREOS_LIB_RWLOCK_H_
#define BAREOS_LIB_RWLOCK_H_ 1

typedef struct s_rwlock_tag {
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
   s_rwlock_tag() {
      mutex = PTHREAD_MUTEX_INITIALIZER;
      read = PTHREAD_COND_INITIALIZER;
      write = PTHREAD_COND_INITIALIZER;
      writer_id = 0;
      priority = 0;
      valid = 0;
      r_active = 0;
      w_active = 0;
      r_wait = 0;
      w_wait = 0;
   };
} brwlock_t;

#define RWLOCK_VALID  0xfacade

#define RwlWritelock(x)     RwlWritelock_p((x), __FILE__, __LINE__)

/**
 * read/write lock prototypes
 */
extern int RwlInit(brwlock_t *rwl, int priority = 0);
extern int RwlDestroy(brwlock_t *rwl);
extern bool RwlIsInit(brwlock_t *rwl);
extern int RwlReadlock(brwlock_t *rwl);
extern int RwlReadtrylock(brwlock_t *rwl);
extern int RwlReadunlock(brwlock_t *rwl);
extern int RwlWritelock_p(brwlock_t *rwl,
                           const char *file = "*unknown*",
                           int line = 0);
extern int RwlWritetrylock(brwlock_t *rwl);
extern int RwlWriteunlock(brwlock_t *rwl);

#endif /* BAREOS_LIB_RWLOCK_H_ */
