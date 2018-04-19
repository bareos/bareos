/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2018-2018 Bareos GmbH & Co. KG

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
#ifndef LIB_TIMERS_H_
#define LIB_TIMERS_H_

DLL_IMP_EXP btimer_t *start_child_timer(JobControlRecord *jcr, pid_t pid, uint32_t wait);
DLL_IMP_EXP void stop_child_timer(btimer_t *wid);
DLL_IMP_EXP btimer_t *start_thread_timer(JobControlRecord *jcr, pthread_t tid, uint32_t wait);
DLL_IMP_EXP void stop_thread_timer(btimer_t *wid);
DLL_IMP_EXP btimer_t *start_bsock_timer(BareosSocket *bs, uint32_t wait);
DLL_IMP_EXP void stop_bsock_timer(btimer_t *wid);

#endif // LIB_TIMERS_H_
