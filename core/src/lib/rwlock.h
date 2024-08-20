/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2001-2010 Free Software Foundation Europe e.V.
   Copyright (C) 2016-2024 Bareos GmbH & Co. KG

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
#define BAREOS_LIB_RWLOCK_H_

#include <pthread.h>

struct brwlock_t {
  pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
  pthread_cond_t read = PTHREAD_COND_INITIALIZER;  /* wait for read */
  pthread_cond_t write = PTHREAD_COND_INITIALIZER; /* wait for write */
  pthread_t writer_id{};                           /* writer's thread id */
  int priority{}; /* used in deadlock detection */
  int valid{};    /* set when valid */
  int r_active{}; /* readers active */
  int w_active{}; /* writers active */
  int r_wait{};   /* readers waiting */
  int w_wait{};   /* writers waiting */
};

static constexpr int RWLOCK_VALID{0xfacade};

// read/write lock prototypes
int RwlInit(brwlock_t* rwl, int priority = 0);
int RwlDestroy(brwlock_t* rwl);
bool RwlIsInit(brwlock_t* rwl);
int RwlReadlock(brwlock_t* rwl);
int RwlReadtrylock(brwlock_t* rwl);
int RwlReadunlock(brwlock_t* rwl);
int RwlWritelock(brwlock_t* rwl);
int RwlWritetrylock(brwlock_t* rwl);
int RwlWriteunlock(brwlock_t* rwl);
void RwlAssertWriterIsMe(brwlock_t* rwl);

#endif  // BAREOS_LIB_RWLOCK_H_
