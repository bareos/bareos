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

/*
 * P and V op that don't use the lock manager (for memory allocation or on win32)
 */
DLL_IMP_EXP void Lmgr_p(pthread_mutex_t *m);
DLL_IMP_EXP void Lmgr_v(pthread_mutex_t *m);

#ifdef BAREOS_INCLUDE_VERSION_H_

typedef struct bthread_mutex_t
{
   pthread_mutex_t mutex;
   int priority;
} bthread_mutex_t;

/*
 * We decide that a thread won't lock more than LMGR_MAX_LOCK at the same time
 */
#define LMGR_MAX_LOCK 32

int BthreadCondWait_p(pthread_cond_t *cond,
                        bthread_mutex_t *mutex,
                        const char *file="*unknown*", int line=0);

int BthreadCondTimedwait_p(pthread_cond_t *cond,
                             bthread_mutex_t *mutex,
                             const struct timespec * abstime,
                             const char *file="*unknown*", int line=0);

/*
 * Same with real pthread_mutex_t
 */
int BthreadCondWait_p(pthread_cond_t *cond,
                        pthread_mutex_t *mutex,
                        const char *file="*unknown*", int line=0);

int BthreadCondTimedwait_p(pthread_cond_t *cond,
                             pthread_mutex_t *mutex,
                             const struct timespec * abstime,
                             const char *file="*unknown*", int line=0);

/*
 * Replacement of pthread_mutex_lock()  but with real pthread_mutex_t
 */
int BthreadMutexLock_p(pthread_mutex_t *m,
                         const char *file="*unknown*", int line=0);

/*
 * Replacement for pthread_mutex_unlock() but with real pthread_mutex_t
 */
int BthreadMutexUnlock_p(pthread_mutex_t *m,
                           const char *file="*unknown*", int line=0);

/*
 * Replacement of pthread_mutex_lock()
 */
int BthreadMutexLock_p(bthread_mutex_t *m,
                         const char *file="*unknown*", int line=0);

/*
 * Replacement of pthread_mutex_unlock()
 */
int BthreadMutexUnlock_p(bthread_mutex_t *m,
                           const char *file="*unknown*", int line=0);

/*
 * Test if this mutex is locked by the current thread
 * 0 - not locked by the current thread
 * 1 - locked by the current thread
 */
int LmgrMutexIsLocked(void *m);

/*
 * Use them when you want use your lock yourself (ie rwlock)
 */

/*
 * Call before requesting the lock
 */
void LmgrPreLock(void *m, int prio=0,
                   const char *file="*unknown*", int line=0);

/*
 * Call after getting it
 */
void LmgrPostLock();

/*
 * Same as pre+post lock
 */
void LmgrDoLock(void *m, int prio=0,
                  const char *file="*unknown*", int line=0);

/*
 * Call just before releasing the lock
 */
void LmgrDoUnlock(void *m);

/*
 * We use C++ mangling to make integration eaysier
 */
int pthread_mutex_init(bthread_mutex_t *m, const pthread_mutexattr_t *attr);
int pthread_mutex_destroy(bthread_mutex_t *m);

void BthreadMutexSetPriority(bthread_mutex_t *m, int prio);

/*
 * Each thread have to call this function to put a lmgr_thread_t object
 * in the stack and be able to call mutex_lock/unlock
 */
void LmgrInitThread();

/*
 * Call this function at the end of the thread
 */
void LmgrCleanupThread();

/*
 * Call this at the end of the program, it will release the
 * global lock manager
 */
void LmgrCleanupMain();

/*
 * Dump each lmgr_thread_t object to stdout
 */
void LmgrDump();

/*
 * Search a deadlock
 */
bool LmgrDetectDeadlock();

/*
 * Search a deadlock after a fatal signal no lock are granted, so the program must be stopped.
 */
bool LmgrDetectDeadlockUnlocked();

/*
 * This function will run your thread with LmgrInitThread() and LmgrCleanupThread().
 */
int LmgrThreadCreate(pthread_t *thread,
                       const pthread_attr_t *attr,
                       void *(*start_routine)(void*), void *arg);

/*
 * Can use SAFEKILL to check if the argument is a valid threadid
 */
int BthreadKill(pthread_t thread, int sig,
                 const char *file="*unknown*", int line=0);

#define BTHREAD_MUTEX_NO_PRIORITY      {PTHREAD_MUTEX_INITIALIZER, 0}
#define BTHREAD_MUTEX_INITIALIZER      BTHREAD_MUTEX_NO_PRIORITY

/*
 * Define USE_LOCKMGR_PRIORITY to detect mutex wrong order
 */
#ifdef USE_LOCKMGR_PRIORITY
#define BTHREAD_MUTEX_PRIORITY(p) { PTHREAD_MUTEX_INITIALIZER, p }
#else
#define BTHREAD_MUTEX_PRIORITY(p) BTHREAD_MUTEX_NO_PRIORITY
#endif

#define BthreadMutexLock(x) BthreadMutexLock_p(x, __FILE__, __LINE__)
#define BthreadMutexUnlock(x) BthreadMutexUnlock_p(x, __FILE__, __LINE__)
#define BthreadCondWait(x,y) BthreadCondWait_p(x,y, __FILE__, __LINE__)
#define BthreadCondTimedwait(x,y,z) BthreadCondTimedwait_p(x,y,z, __FILE__, __LINE__)

/*
 * Define _LOCKMGR_COMPLIANT to use real pthread functions
 */
#ifdef _LOCKMGR_COMPLIANT
#define P(x) Lmgr_p(&(x))
#define V(x) Lmgr_v(&(x))
#else
#define P(x) BthreadMutexLock_p(&(x), __FILE__, __LINE__)
#define V(x) BthreadMutexUnlock_p(&(x), __FILE__, __LINE__)
#define pthread_create(a,b,c,d) LmgrThreadCreate(a,b,c,d)
#define pthread_mutex_lock(x) BthreadMutexLock(x)
#define pthread_mutex_unlock(x) BthreadMutexUnlock(x)
#define pthread_cond_wait(x,y) BthreadCondWait(x,y)
#define pthread_cond_timedwait(x,y,z) BthreadCondTimedwait(x,y,z)
#ifdef USE_LOCKMGR_SAFEKILL
#define pthread_kill(a,b) BthreadKill((a),(b), __FILE__, __LINE__)
#endif
#endif
#else /* BAREOS_INCLUDE_VERSION_H_ */
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
#endif /* BAREOS_INCLUDE_VERSION_H_ */
#endif /* BAREOS_LIB_LOCKMGR_H_ */
