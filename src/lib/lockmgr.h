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

#ifndef _LOCKMGR_H
#define _LOCKMGR_H 1

#include "mutex_list.h"     /* Manage mutex with priority in a central place */

/*
 * P and V op that don't use the lock manager (for memory allocation or on win32)
 */
DLL_IMP_EXP void lmgr_p(pthread_mutex_t *m);
DLL_IMP_EXP void lmgr_v(pthread_mutex_t *m);

#ifdef _USE_LOCKMGR

typedef struct bthread_mutex_t
{
   pthread_mutex_t mutex;
   int priority;
} bthread_mutex_t;

/*
 * We decide that a thread won't lock more than LMGR_MAX_LOCK at the same time
 */
#define LMGR_MAX_LOCK 32

int bthread_cond_wait_p(pthread_cond_t *cond,
                        bthread_mutex_t *mutex,
                        const char *file="*unknown*", int line=0);

int bthread_cond_timedwait_p(pthread_cond_t *cond,
                             bthread_mutex_t *mutex,
                             const struct timespec * abstime,
                             const char *file="*unknown*", int line=0);

/*
 * Same with real pthread_mutex_t
 */
int bthread_cond_wait_p(pthread_cond_t *cond,
                        pthread_mutex_t *mutex,
                        const char *file="*unknown*", int line=0);

int bthread_cond_timedwait_p(pthread_cond_t *cond,
                             pthread_mutex_t *mutex,
                             const struct timespec * abstime,
                             const char *file="*unknown*", int line=0);

/*
 * Replacement of pthread_mutex_lock()  but with real pthread_mutex_t
 */
int bthread_mutex_lock_p(pthread_mutex_t *m,
                         const char *file="*unknown*", int line=0);

/*
 * Replacement for pthread_mutex_unlock() but with real pthread_mutex_t
 */
int bthread_mutex_unlock_p(pthread_mutex_t *m,
                           const char *file="*unknown*", int line=0);

/*
 * Replacement of pthread_mutex_lock()
 */
int bthread_mutex_lock_p(bthread_mutex_t *m,
                         const char *file="*unknown*", int line=0);

/*
 * Replacement of pthread_mutex_unlock()
 */
int bthread_mutex_unlock_p(bthread_mutex_t *m,
                           const char *file="*unknown*", int line=0);

/*
 * Test if this mutex is locked by the current thread
 * 0 - not locked by the current thread
 * 1 - locked by the current thread
 */
int lmgr_mutex_is_locked(void *m);

/*
 * Use them when you want use your lock yourself (ie rwlock)
 */

/*
 * Call before requesting the lock
 */
void lmgr_pre_lock(void *m, int prio=0,
                   const char *file="*unknown*", int line=0);

/*
 * Call after getting it
 */
void lmgr_post_lock();

/*
 * Same as pre+post lock
 */
void lmgr_do_lock(void *m, int prio=0,
                  const char *file="*unknown*", int line=0);

/*
 * Call just before releasing the lock
 */
void lmgr_do_unlock(void *m);

/*
 * We use C++ mangling to make integration eaysier
 */
int pthread_mutex_init(bthread_mutex_t *m, const pthread_mutexattr_t *attr);
int pthread_mutex_destroy(bthread_mutex_t *m);

void bthread_mutex_set_priority(bthread_mutex_t *m, int prio);

/*
 * Each thread have to call this function to put a lmgr_thread_t object
 * in the stack and be able to call mutex_lock/unlock
 */
void lmgr_init_thread();

/*
 * Call this function at the end of the thread
 */
void lmgr_cleanup_thread();

/*
 * Call this at the end of the program, it will release the
 * global lock manager
 */
void lmgr_cleanup_main();

/*
 * Dump each lmgr_thread_t object to stdout
 */
void lmgr_dump();

/*
 * Search a deadlock
 */
bool lmgr_detect_deadlock();

/*
 * Search a deadlock after a fatal signal no lock are granted, so the program must be stopped.
 */
bool lmgr_detect_deadlock_unlocked();

/*
 * This function will run your thread with lmgr_init_thread() and lmgr_cleanup_thread().
 */
int lmgr_thread_create(pthread_t *thread,
                       const pthread_attr_t *attr,
                       void *(*start_routine)(void*), void *arg);

/*
 * Can use SAFEKILL to check if the argument is a valid threadid
 */
int bthread_kill(pthread_t thread, int sig,
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

#define bthread_mutex_lock(x) bthread_mutex_lock_p(x, __FILE__, __LINE__)
#define bthread_mutex_unlock(x) bthread_mutex_unlock_p(x, __FILE__, __LINE__)
#define bthread_cond_wait(x,y) bthread_cond_wait_p(x,y, __FILE__, __LINE__)
#define bthread_cond_timedwait(x,y,z) bthread_cond_timedwait_p(x,y,z, __FILE__, __LINE__)

/*
 * Define _LOCKMGR_COMPLIANT to use real pthread functions
 */
#ifdef _LOCKMGR_COMPLIANT
#define P(x) lmgr_p(&(x))
#define V(x) lmgr_v(&(x))
#else
#define P(x) bthread_mutex_lock_p(&(x), __FILE__, __LINE__)
#define V(x) bthread_mutex_unlock_p(&(x), __FILE__, __LINE__)
#define pthread_create(a,b,c,d) lmgr_thread_create(a,b,c,d)
#define pthread_mutex_lock(x) bthread_mutex_lock(x)
#define pthread_mutex_unlock(x) bthread_mutex_unlock(x)
#define pthread_cond_wait(x,y) bthread_cond_wait(x,y)
#define pthread_cond_timedwait(x,y,z) bthread_cond_timedwait(x,y,z)
#ifdef USE_LOCKMGR_SAFEKILL
#define pthread_kill(a,b) bthread_kill((a),(b), __FILE__, __LINE__)
#endif
#endif
#else /* _USE_LOCKMGR */
#define lmgr_detect_deadloc()
#define lmgr_dump()
#define lmgr_init_thread()
#define lmgr_cleanup_thread()
#define lmgr_pre_lock(m,prio,f,l)
#define lmgr_post_lock()
#define lmgr_do_lock(m,prio,f,l)
#define lmgr_do_unlock(m)
#define lmgr_cleanup_main()
#define bthread_mutex_set_priority(a,b)
#define bthread_mutex_lock(a) pthread_mutex_lock(a)
#define bthread_mutex_lock_p(a,f,l) pthread_mutex_lock(a)
#define bthread_mutex_unlock(a) pthread_mutex_unlock(a)
#define bthread_mutex_unlock_p(a,f,l) pthread_mutex_unlock(a)
#define lmgr_cond_wait(a,b) pthread_cond_wait(a,b)
#define lmgr_cond_timedwait(a,b,c) pthread_cond_timedwait(a,b,c)
#define bthread_mutex_t pthread_mutex_t
#define P(x) lmgr_p(&(x))
#define V(x) lmgr_v(&(x))
#define BTHREAD_MUTEX_PRIORITY(p) PTHREAD_MUTEX_INITIALIZER
#define BTHREAD_MUTEX_NO_PRIORITY PTHREAD_MUTEX_INITIALIZER
#define BTHREAD_MUTEX_INITIALIZER PTHREAD_MUTEX_INITIALIZER
#define lmgr_mutex_is_locked(m) (1)
#define bthread_cond_wait_p(w,x,y,z) pthread_cond_wait(w,x)
#endif /* _USE_LOCKMGR */
#endif /* _LOCKMGR_H */
