/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2008-2010 Free Software Foundation Europe e.V.

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation, which is
   listed in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   Affero General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
*/

#ifndef BAREOS_LIB_LOCKMGR_H_
#define BAREOS_LIB_LOCKMGR_H_ 1

#include "mutex_list.h"     /* Manage mutex with priority in a central place */
#include <mutex>

/*
 * P and V op that don't use the lock manager (for memory allocation or on win32)
 */
void Lmgr_p(pthread_mutex_t *m);
void Lmgr_v(pthread_mutex_t *m);

#define lmgr_detect_deadloc()
#define LmgrDump()
#define LmgrInitThread()
#define LmgrCleanupThread()
#define LmgrPreLock(m,prio,f,l)
#define LmgrPostLock()
#define LmgrDoLock(m,prio,f,l)
#define LmgrDoUnlock(m)
#define LmgrCleanupMain()
#define BthreadMutexSetPriority(a,b)
#define BthreadMutexLock(a) pthread_mutex_lock(a)
#define BthreadMutexLock_p(a,f,l) pthread_mutex_lock(a)
#define BthreadMutexUnlock(a) pthread_mutex_unlock(a)
#define BthreadMutexUnlock_p(a,f,l) pthread_mutex_unlock(a)
#define lmgr_cond_wait(a,b) pthread_cond_wait(a,b)
#define lmgr_cond_timedwait(a,b,c) pthread_cond_timedwait(a,b,c)
#define bthread_mutex_t pthread_mutex_t
#define P(x) Lmgr_p(&(x))
#define V(x) Lmgr_v(&(x))
#define BTHREAD_MUTEX_PRIORITY(p) PTHREAD_MUTEX_INITIALIZER
#define BTHREAD_MUTEX_NO_PRIORITY PTHREAD_MUTEX_INITIALIZER
#define BTHREAD_MUTEX_INITIALIZER PTHREAD_MUTEX_INITIALIZER
#define LmgrMutexIsLocked(m) (1)
#define BthreadCondWait_p(w,x,y,z) pthread_cond_wait(w,x)
#endif /* BAREOS_LIB_LOCKMGR_H_ */
