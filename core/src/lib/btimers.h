/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2003-2007 Free Software Foundation Europe e.V.
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
 * Nic Bellamy <nic@bellamy.co.nz>, October 2003.
 *
 */
/**
 * @file
 * Process and thread timer routines, built on top of watchdogs.
 */

#ifndef BAREOS_LIB_BTIMERS_H_
#define BAREOS_LIB_BTIMERS_H_

#include "lib/timer_thread.h"
#include "lib/watchdog.h"

class BareosSocket;

struct btimer_t {
  watchdog_t* wd; /**< Parent watchdog */
  int type;
  bool killed;
  pid_t pid;             /**< process id if TYPE_CHILD */
  pthread_t tid;         /**< thread id if TYPE_PTHREAD */
  BareosSocket* bsock;   /**< Pointer to BareosSocket */
  JobControlRecord* jcr; /**< Pointer to job control record */
};

btimer_t* start_child_timer(JobControlRecord* jcr, pid_t pid, uint32_t wait);
void StopChildTimer(btimer_t* wid);
btimer_t* start_thread_timer(JobControlRecord* jcr,
                             pthread_t tid,
                             uint32_t wait);
void StopThreadTimer(btimer_t* wid);
btimer_t* StartBsockTimer(BareosSocket* bs, uint32_t wait);
void StopBsockTimer(btimer_t* wid);

#endif /* BAREOS_LIB_BTIMERS_H_ */
