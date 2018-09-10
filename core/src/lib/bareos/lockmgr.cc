/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2008-2011 Free Software Foundation Europe e.V.

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

/*
  How to use mutex with bad order usage detection
 ------------------------------------------------

 Note: see file mutex_list.h for current mutexes with
       defined priorities.

 Instead of using:
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    P(mutex);
    ..
    V(mutex);

 use:
    bthread_mutex_t mutex = BTHREAD_MUTEX_PRIORITY(1);
    P(mutex);
    ...
    V(mutex);

 Mutex that doesn't need this extra check can be declared as pthread_mutex_t.
 You can use this object on pthread_mutex_lock/unlock/cond_wait/cond_timewait.

 With dynamic creation, you can use:
    bthread_mutex_t mutex;
    pthread_mutex_init(&mutex);
    BthreadMutexSetPriority(&mutex, 10);
    pthread_mutex_destroy(&mutex);

 */

#define _LOCKMGR_COMPLIANT
#include "include/bareos.h"
#include "lib/edit.h"

#undef ASSERT
#define ASSERT(x) if (!(x)) { \
   char *jcr = NULL; \
   FPmsg3(000, _("ASSERT failed at %s:%i: %s\n"), __FILE__, __LINE__, #x); \
   jcr[0] = 0; }

#define ASSERT_p(x,f,l) if (!(x)) {              \
   char *jcr = NULL; \
   FPmsg3(000, _("ASSERT failed at %s:%i: %s \n"), f, l, #x); \
   jcr[0] = 0; }

/*
  Inspired from
  http://www.cs.berkeley.edu/~kamil/teaching/sp03/041403.pdf

  This lock manager will replace some pthread calls. It can be
  enabled with BAREOS_INCLUDE_VERSION_H_

  Some part of the code can't use this manager, for example the
  rwlock object or the smartalloc lib. To disable LMGR, just add
  _LOCKMGR_COMPLIANT before the inclusion of "include/bareos.h"

  cd build/src/tools
  g++ -g -c lockmgr.c -I.. -I../lib -D_USE_LOCKMGR -D_TEST_IT
  g++ -o lockmgr lockmgr.o -lbareos -L../lib/.libs -lssl -lpthread

*/


/*
 * pthread_mutex_lock for memory allocator and other
 * parts that are _LOCKMGR_COMPLIANT
 */
void Lmgr_p(pthread_mutex_t *m)
{
   int errstat;
   if ((errstat=pthread_mutex_lock(m))) {
      BErrNo be;
      e_msg(__FILE__, __LINE__, M_ABORT, 0, _("Mutex lock failure. ERR=%s\n"),
            be.bstrerror(errstat));
   }
}

void Lmgr_v(pthread_mutex_t *m)
{
   int errstat;
   if ((errstat=pthread_mutex_unlock(m))) {
      BErrNo be;
      e_msg(__FILE__, __LINE__, M_ABORT, 0, _("Mutex unlock failure. ERR=%s\n"),
            be.bstrerror(errstat));
   }
}

#ifdef BAREOS_INCLUDE_VERSION_H_

typedef enum
{
   LMGR_WHITE,                  /* never seen */
   LMGR_BLACK,                  /* no loop */
   LMGR_GREY                    /* seen before */
} lmgr_color_t;

/*
 * Node used by the Lock Manager
 * If the lock is GRANTED, we have mutex -> proc, else it's a proc -> mutex
 * relation.
 *
 * Note, each mutex can be GRANTED once, and each proc can have only one WANTED
 * mutex.
 */
class lmgr_node_t: public SmartAlloc
{
public:
   dlink link;
   void *node;
   void *child;
   lmgr_color_t seen;

   lmgr_node_t() {
      child = node = NULL;
      seen = LMGR_WHITE;
   }

   lmgr_node_t(void *n, void *c) {
      init(n,c);
   }

   void init(void *n, void *c) {
      node = n;
      child = c;
      seen = LMGR_WHITE;
   }

   void MarkAsSeen(lmgr_color_t c) {
      seen = c;
   }

   ~lmgr_node_t() {printf("delete node\n");}
};

typedef enum {
   LMGR_LOCK_EMPTY   = 'E',      /* unused */
   LMGR_LOCK_WANTED  = 'W',      /* before mutex_lock */
   LMGR_LOCK_GRANTED = 'G'       /* after mutex_lock */
} lmgr_state_t;

/*
 * Object associated with each mutex per thread
 */
class lmgr_lock_t: public SmartAlloc
{
public:
   dlink link;
   void *lock;
   lmgr_state_t state;
   int max_priority;
   int priority;

   const char *file;
   int line;

   lmgr_lock_t() {
      lock = NULL;
      state = LMGR_LOCK_EMPTY;
      priority = max_priority = 0;
   }

   lmgr_lock_t(void *l) {
      lock = l;
      state = LMGR_LOCK_WANTED;
   }

   void set_granted() {
      state = LMGR_LOCK_GRANTED;
   }

   ~lmgr_lock_t() {}

};

/*
 * Get the child list, ret must be already allocated
 */
static void SearchAllNode(dlist *g, lmgr_node_t *v, alist *ret)
{
   lmgr_node_t *n;
   foreach_dlist(n, g) {
      if (v->child == n->node) {
         ret->append(n);
      }
   }
}

static bool visite(dlist *g, lmgr_node_t *v)
{
   bool ret=false;
   lmgr_node_t *n;
   v->MarkAsSeen(LMGR_GREY);

   alist *d = New(alist(5, false)); /* use alist because own=false */
   SearchAllNode(g, v, d);

   //foreach_alist(n, d) {
   //   printf("node n=%p c=%p s=%c\n", n->node, n->child, n->seen);
   //}

   foreach_alist(n, d) {
      if (n->seen == LMGR_GREY) { /* already seen this node */
         ret = true;
         goto bail_out;
      } else if (n->seen == LMGR_WHITE) {
         if (visite(g, n)) {
            ret = true;
            goto bail_out;
         }
      }
   }
   v->MarkAsSeen(LMGR_BLACK); /* no loop detected, node is clean */
bail_out:
   delete d;
   return ret;
}

static bool ContainsCycle(dlist *g)
{
   lmgr_node_t *n;
   foreach_dlist(n, g) {
      if (n->seen == LMGR_WHITE) {
         if (visite(g, n)) {
            return true;
         }
      }
   }
   return false;
}

class lmgr_thread_t: public SmartAlloc
{
public:
   dlink link;
   pthread_mutex_t mutex;
   pthread_t thread_id;
   lmgr_lock_t lock_list[LMGR_MAX_LOCK];
   int current;
   int max;
   int max_priority;

   lmgr_thread_t() {
      int status;
      if ((status = pthread_mutex_init(&mutex, NULL)) != 0) {
         BErrNo be;
         FPmsg1(000, _("pthread key create failed: ERR=%s\n"), be.bstrerror(status));
         ASSERT(0);
      }
      thread_id = pthread_self();
      current = -1;
      max = 0;
      max_priority = 0;
   }

   void _dump(FILE *fp) {
      char ed1[50];

      fprintf(fp, "threadid=%s max=%i current=%i\n",
              edit_pthread(thread_id, ed1, sizeof(ed1)), max, current);

      for (int i = 0; i <= current; i++) {
         fprintf(fp, "   lock=%p state=%s priority=%i %s:%i\n",
                 lock_list[i].lock,
                 (lock_list[i].state=='W') ? "Wanted " : "Granted",
                 lock_list[i].priority,
                 lock_list[i].file,
                 lock_list[i].line);
      }
   }

   void dump(FILE *fp) {
      Lmgr_p(&mutex);
      {
         _dump(fp);
      }
      Lmgr_v(&mutex);
   }

   /*
    * Call before a lock operation (mark mutex as WANTED)
    */
   virtual void pre_P(void *m, int priority,
                      const char *f="*unknown*", int l=0)
   {
      int max_prio = max_priority;
      ASSERT_p(current < LMGR_MAX_LOCK, f, l);
      ASSERT_p(current >= -1, f, l);
      Lmgr_p(&mutex);
      {
         current++;
         lock_list[current].lock = m;
         lock_list[current].state = LMGR_LOCK_WANTED;
         lock_list[current].file = f;
         lock_list[current].line = l;
         lock_list[current].priority = priority;
         lock_list[current].max_priority = MAX(priority, max_priority);
         max = MAX(current, max);
         max_priority = MAX(priority, max_priority);
      }
      Lmgr_v(&mutex);
      ASSERT_p(!priority || priority >= max_prio, f, l);
   }

   /*
    * Call after the lock operation (mark mutex as GRANTED)
    */
   virtual void post_P() {
      ASSERT(current >= 0);
      ASSERT(lock_list[current].state == LMGR_LOCK_WANTED);
      lock_list[current].state = LMGR_LOCK_GRANTED;
   }

   /* Using this function is some sort of bug */
   void ShiftList(int i) {
      for(int j=i+1; j<=current; j++) {
         lock_list[i] = lock_list[j];
      }
      if (current >= 0) {
         lock_list[current].lock = NULL;
         lock_list[current].state = LMGR_LOCK_EMPTY;
      }
      /* rebuild the priority list */
      max_priority = 0;
      for(int j=0; j< current; j++) {
         max_priority = MAX(lock_list[j].priority, max_priority);
         lock_list[j].max_priority = max_priority;
      }
   }

   /*
    * Remove the mutex from the list
    */
   virtual void do_V(void *m, const char *f="*unknown*", int l=0) {
      ASSERT_p(current >= 0, f, l);
      Lmgr_p(&mutex);
      {
         if (lock_list[current].lock == m) {
            lock_list[current].lock = NULL;
            lock_list[current].state = LMGR_LOCK_EMPTY;
            current--;
         } else {
            ASSERT(current > 0);
            FPmsg3(0, "ERROR: wrong P/V order search lock=%p %s:%i\n", m, f, l);
            FPmsg4(0, "ERROR: wrong P/V order pos=%i lock=%p %s:%i\n",
                   current, lock_list[current].lock, lock_list[current].file,
                   lock_list[current].line);
            for (int i=current-1; i >= 0; i--) { /* already seen current */
               FPmsg4(0, "ERROR: wrong P/V order pos=%i lock=%p %s:%i\n",
                      i, lock_list[i].lock, lock_list[i].file, lock_list[i].line);
               if (lock_list[i].lock == m) {
                  FPmsg3(0, "ERROR: FOUND P pos=%i %s:%i\n", i, f, l);
                  ShiftList(i);
                  current--;
                  break;
               }
            }
         }
         /* reset max_priority to the last one */
         if (current >= 0) {
            max_priority = lock_list[current].max_priority;
         } else {
            max_priority = 0;
         }
      }
      Lmgr_v(&mutex);
   }

   virtual ~lmgr_thread_t() {destroy();}

   void destroy() {
      pthread_mutex_destroy(&mutex);
   }
} ;

class lmgr_dummy_thread_t: public lmgr_thread_t
{
   void do_V(void *m, const char *file, int l)  {}
   void post_P()                                {}
   void pre_P(void *m, int priority, const char *file, int l) {}
};

/*
 * LMGR - Lock Manager
 *
 *
 *
 */

pthread_once_t key_lmgr_once = PTHREAD_ONCE_INIT;
static pthread_key_t lmgr_key;  /* used to get lgmr_thread_t object */

static dlist *global_mgr = NULL;  /* used to store all lgmr_thread_t objects */
static pthread_mutex_t lmgr_global_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_t undertaker;
static bool use_undertaker=true;

#define lmgr_is_active() (global_mgr != NULL)

/*
 * Add a new lmgr_thread_t object to the global list
 */
void LmgrRegisterThread(lmgr_thread_t *item)
{
   Lmgr_p(&lmgr_global_mutex);
   {
      global_mgr->prepend(item);
   }
   Lmgr_v(&lmgr_global_mutex);
}

/*
 * Call this function to cleanup specific lock thread data
 */
void LmgrUnregisterThread(lmgr_thread_t *item)
{
   if (!lmgr_is_active()) {
      return;
   }
   Lmgr_p(&lmgr_global_mutex);
   {
      global_mgr->remove(item);
   }
   Lmgr_v(&lmgr_global_mutex);
}

/*
 * Search for a deadlock when it's secure to walk across
 * locks list. (after LmgrDetectDeadlock or a fatal signal)
 */
bool LmgrDetectDeadlockUnlocked()
{
   bool ret=false;
   lmgr_node_t *node=NULL;
   lmgr_lock_t *lock;
   lmgr_thread_t *item;
   dlist *g = New(dlist(node, &node->link));

   /* First, get a list of all node */
   foreach_dlist(item, global_mgr) {
      for(int i=0; i<=item->current; i++) {
         node = NULL;
         lock = &item->lock_list[i];
         /* Depending if the lock is granted or not, it's a child or a root
          *  Granted:  Mutex  -> Thread
          *  Wanted:   Thread -> Mutex
          *
          * Note: a Mutex can be locked only once, a thread can request only
          * one mutex.
          *
          */
         if (lock->state == LMGR_LOCK_GRANTED) {
            node = New(lmgr_node_t((void*)lock->lock, (void*)item->thread_id));
         } else if (lock->state == LMGR_LOCK_WANTED) {
            node = New(lmgr_node_t((void*)item->thread_id, (void*)lock->lock));
         }
         if (node) {
            g->append(node);
         }
      }
   }

   //foreach_dlist(node, g) {
   //   printf("g n=%p c=%p\n", node->node, node->child);
   //}

   ret = ContainsCycle(g);
   if (ret) {
      printf("Found a deadlock !!!!\n");
   }

   delete g;
   return ret;
}

/*
 * Search for a deadlock in during the runtime
 * It will lock all thread specific lock manager, nothing
 * can be locked during this check.
 */
bool LmgrDetectDeadlock()
{
   bool ret=false;
   if (!lmgr_is_active()) {
      return ret;
   }

   Lmgr_p(&lmgr_global_mutex);
   {
      lmgr_thread_t *item;
      foreach_dlist(item, global_mgr) {
         Lmgr_p(&item->mutex);
      }

      ret = LmgrDetectDeadlockUnlocked();

      foreach_dlist(item, global_mgr) {
         Lmgr_v(&item->mutex);
      }
   }
   Lmgr_v(&lmgr_global_mutex);

   return ret;
}

/*
 * !!! WARNING !!!
 * Use this function is used only after a fatal signal
 * We don't use locking to display the information
 */
void DbgPrintLock(FILE *fp)
{
   fprintf(fp, "Attempt to dump locks\n");
   if (!lmgr_is_active()) {
      return ;
   }
   lmgr_thread_t *item;
   foreach_dlist(item, global_mgr) {
      item->_dump(fp);
   }
}

/*
 * Dump each lmgr_thread_t object
 */
void LmgrDump()
{
   Lmgr_p(&lmgr_global_mutex);
   {
      lmgr_thread_t *item;
      foreach_dlist(item, global_mgr) {
         item->dump(stderr);
      }
   }
   Lmgr_v(&lmgr_global_mutex);
}

void cln_hdl(void *a)
{
   LmgrCleanupThread();
}

void *check_deadlock(void *)
{
   int old;
   LmgrInitThread();
   pthread_cleanup_push(cln_hdl, NULL);

   while (!Bmicrosleep(30, 0)) {
      pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &old);
      if (LmgrDetectDeadlock()) {
         LmgrDump();
         ASSERT(0);
      }
      pthread_setcancelstate(old, NULL);
      pthread_testcancel();
   }
   pthread_cleanup_pop(1);
   return NULL;
}

/* This object is used when LMGR is not initialized */
static lmgr_dummy_thread_t dummy_lmgr;

/*
 * Retrieve the lmgr_thread_t object from the stack
 */
inline lmgr_thread_t *lmgr_get_thread_info()
{
   if (lmgr_is_active()) {
      return (lmgr_thread_t *)pthread_getspecific(lmgr_key);
   } else {
      return &dummy_lmgr;
   }
}

/*
 * launch once for all threads
 */
void create_lmgr_key()
{
   int status = pthread_key_create(&lmgr_key, NULL);
   if (status != 0) {
      BErrNo be;
      FPmsg1(000, _("pthread key create failed: ERR=%s\n"), be.bstrerror(status));
      ASSERT(0);
   }

   lmgr_thread_t *n=NULL;
   global_mgr = New(dlist(n, &n->link));

   if (use_undertaker) {
      status = pthread_create(&undertaker, NULL, check_deadlock, NULL);
      if (status != 0) {
         BErrNo be;
         FPmsg1(000, _("pthread_create failed: ERR=%s\n"), be.bstrerror(status));
         ASSERT(0);
      }
   }
}

/*
 * Each thread have to call this function to put a lmgr_thread_t object
 * in the stack and be able to call mutex_lock/unlock
 */
void LmgrInitThread()
{
   int status = pthread_once(&key_lmgr_once, create_lmgr_key);
   if (status != 0) {
      BErrNo be;
      FPmsg1(000, _("pthread key create failed: ERR=%s\n"), be.bstrerror(status));
      ASSERT(0);
   }
   lmgr_thread_t *l = New(lmgr_thread_t());
   pthread_setspecific(lmgr_key, l);
   LmgrRegisterThread(l);
}

/*
 * Call this function at the end of the thread
 */
void LmgrCleanupThread()
{
   if (!lmgr_is_active()) {
      return ;
   }
   lmgr_thread_t *self = lmgr_get_thread_info();
   LmgrUnregisterThread(self);
   delete(self);
}

/*
 * This function should be call at the end of the main thread
 * Some thread like the watchdog are already present, so the global_mgr
 * list is never empty. Should carefully clear the memory.
 */
void LmgrCleanupMain()
{
   dlist *temp;

   if (!global_mgr) {
      return;
   }
   if (use_undertaker) {
      pthread_cancel(undertaker);
   }
   LmgrCleanupThread();
   Lmgr_p(&lmgr_global_mutex);
   {
      temp = global_mgr;
      global_mgr = NULL;
      delete temp;
   }
   Lmgr_v(&lmgr_global_mutex);
}

/*
 * Set the priority of the lmgr mutex object
 */
void BthreadMutexSetPriority(bthread_mutex_t *m, int prio)
{
#ifdef USE_LOCKMGR_PRIORITY
   m->priority = prio;
#endif
}

/*
 * Replacement for pthread_mutex_init()
 */
int pthread_mutex_init(bthread_mutex_t *m, const pthread_mutexattr_t *attr)
{
   m->priority = 0;
   return pthread_mutex_init(&m->mutex, attr);
}

/*
 * Replacement for pthread_mutex_destroy()
 */
int pthread_mutex_destroy(bthread_mutex_t *m)
{
   return pthread_mutex_destroy(&m->mutex);
}

/*
 * Replacement for pthread_kill (only with USE_LOCKMGR_SAFEKILL)
 */
int BthreadKill(pthread_t thread, int sig,
                 const char *file, int line)
{
   bool thread_found_in_process=false;

   /* We doesn't allow to send signal to ourself */
   ASSERT(!pthread_equal(thread, pthread_self()));

   /* This loop isn't very efficient with dozens of threads but we don't use
    * signal very much, and this feature is for testing only
    */
   Lmgr_p(&lmgr_global_mutex);
   {
      lmgr_thread_t *item;
      foreach_dlist(item, global_mgr) {
         if (pthread_equal(thread, item->thread_id)) {
            thread_found_in_process=true;
            break;
         }
      }
   }
   Lmgr_v(&lmgr_global_mutex);

   /* Sending a signal to non existing thread can create problem
    * so, we can stop here.
    */
   ASSERT(thread_found_in_process == true);

   return pthread_kill(thread, sig);
}

/*
 * Replacement for pthread_mutex_lock()
 * Returns always ok
 */
int BthreadMutexLock_p(bthread_mutex_t *m, const char *file, int line)
{
   lmgr_thread_t *self = lmgr_get_thread_info();
   self->pre_P(m, m->priority, file, line);
   Lmgr_p(&m->mutex);
   self->post_P();
   return 0;
}

/*
 * Replacement for pthread_mutex_unlock()
 * Returns always ok
 */
int BthreadMutexUnlock_p(bthread_mutex_t *m, const char *file, int line)
{
   lmgr_thread_t *self = lmgr_get_thread_info();
   self->do_V(m, file, line);
   Lmgr_v(&m->mutex);
   return 0;
}

/*
 * Replacement for pthread_mutex_lock() but with real pthread_mutex_t
 * Returns always ok
 */
int BthreadMutexLock_p(pthread_mutex_t *m, const char *file, int line)
{
   lmgr_thread_t *self = lmgr_get_thread_info();
   self->pre_P(m, 0, file, line);
   Lmgr_p(m);
   self->post_P();
   return 0;
}

/*
 * Replacement for pthread_mutex_unlock() but with real pthread_mutex_t
 * Returns always ok
 */
int BthreadMutexUnlock_p(pthread_mutex_t *m, const char *file, int line)
{
   lmgr_thread_t *self = lmgr_get_thread_info();
   self->do_V(m, file, line);
   Lmgr_v(m);
   return 0;
}


/* TODO: check this
 */
int BthreadCondWait_p(pthread_cond_t *cond,
                        pthread_mutex_t *m,
                        const char *file, int line)
{
   int ret;
   lmgr_thread_t *self = lmgr_get_thread_info();
   self->do_V(m, file, line);
   ret = pthread_cond_wait(cond, m);
   self->pre_P(m, 0, file, line);
   self->post_P();
   return ret;
}

/* TODO: check this
 */
int BthreadCondTimedwait_p(pthread_cond_t *cond,
                             pthread_mutex_t *m,
                             const struct timespec * abstime,
                             const char *file, int line)
{
   int ret;
   lmgr_thread_t *self = lmgr_get_thread_info();
   self->do_V(m, file, line);
   ret = pthread_cond_timedwait(cond, m, abstime);
   self->pre_P(m, 0, file, line);
   self->post_P();
   return ret;
}

/* TODO: check this
 */
int BthreadCondWait_p(pthread_cond_t *cond,
                        bthread_mutex_t *m,
                        const char *file, int line)
{
   int ret;
   lmgr_thread_t *self = lmgr_get_thread_info();
   self->do_V(m, file, line);
   ret = pthread_cond_wait(cond, &m->mutex);
   self->pre_P(m, m->priority, file, line);
   self->post_P();
   return ret;
}

/* TODO: check this
 */
int BthreadCondTimedwait_p(pthread_cond_t *cond,
                             bthread_mutex_t *m,
                             const struct timespec * abstime,
                             const char *file, int line)
{
   int ret;
   lmgr_thread_t *self = lmgr_get_thread_info();
   self->do_V(m, file, line);
   ret = pthread_cond_timedwait(cond, &m->mutex, abstime);
   self->pre_P(m, m->priority, file, line);
   self->post_P();
   return ret;
}

/*  Test if this mutex is locked by the current thread
 *  returns:
 *     0 - unlocked
 *     1 - locked by the current thread
 *     2 - locked by another thread
 */
int LmgrMutexIsLocked(void *m)
{
   lmgr_thread_t *self = lmgr_get_thread_info();

   for(int i=0; i <= self->current; i++) {
      if (self->lock_list[i].lock == m) {
         return 1;              /* locked by us */
      }
   }

   return 0;                    /* not locked by us */
}

/*
 * Use this function when the caller handle the mutex directly
 *
 * LmgrPreLock(m, 10);
 * pthread_mutex_lock(m);
 * LmgrPostLock(m);
 */
void LmgrPreLock(void *m, int prio, const char *file, int line)
{
   lmgr_thread_t *self = lmgr_get_thread_info();
   self->pre_P(m, prio, file, line);
}

/*
 * Use this function when the caller handle the mutex directly
 */
void LmgrPostLock()
{
   lmgr_thread_t *self = lmgr_get_thread_info();
   self->post_P();
}

/*
 * Do directly pre_P and post_P (used by trylock)
 */
void LmgrDoLock(void *m, int prio, const char *file, int line)
{
   lmgr_thread_t *self = lmgr_get_thread_info();
   self->pre_P(m, prio, file, line);
   self->post_P();
}

/*
 * Use this function when the caller handle the mutex directly
 */
void LmgrDoUnlock(void *m)
{
   lmgr_thread_t *self = lmgr_get_thread_info();
   self->do_V(m);
}

typedef struct {
   void *(*start_routine)(void*);
   void *arg;
} lmgr_thread_arg_t;

extern "C"
void *lmgr_thread_launcher(void *x)
{
   void *ret=NULL;
   LmgrInitThread();
   pthread_cleanup_push(cln_hdl, NULL);

   lmgr_thread_arg_t arg;
   lmgr_thread_arg_t *a = (lmgr_thread_arg_t *)x;
   arg.start_routine = a->start_routine;
   arg.arg = a->arg;
   free(a);

   ret = arg.start_routine(arg.arg);
   pthread_cleanup_pop(1);
   return ret;
}

int LmgrThreadCreate(pthread_t *thread,
                       const pthread_attr_t *attr,
                       void *(*start_routine)(void*), void *arg)
{
   /* lmgr should be active (LmgrInitThread() call in main()) */
   ASSERT(lmgr_is_active());
   /* Will be freed by the child */
   lmgr_thread_arg_t *a = (lmgr_thread_arg_t*) malloc(sizeof(lmgr_thread_arg_t));
   a->start_routine = start_routine;
   a->arg = arg;
   return pthread_create(thread, attr, lmgr_thread_launcher, a);
}

#else  /* BAREOS_INCLUDE_VERSION_H_ */

/*
 * !!! WARNING !!!
 * Use this function is used only after a fatal signal
 * We don't use locking to display information
 */
void DbgPrintLock(FILE *fp)
{
   FPmsg0(000, "lockmgr disabled\n");
}

#endif  /* BAREOS_INCLUDE_VERSION_H_ */

#ifdef _TEST_IT

#include "lockmgr.h"
#undef P
#undef V
#define P(x) BthreadMutexLock_p(&(x), __FILE__, __LINE__)
#define V(x) BthreadMutexUnlock_p(&(x), __FILE__, __LINE__)
#define pthread_create(a, b, c, d)    LmgrThreadCreate(a,b,c,d)

bthread_mutex_t mutex1 = BTHREAD_MUTEX_NO_PRIORITY;
bthread_mutex_t mutex2 = BTHREAD_MUTEX_NO_PRIORITY;
bthread_mutex_t mutex3 = BTHREAD_MUTEX_NO_PRIORITY;
bthread_mutex_t mutex4 = BTHREAD_MUTEX_NO_PRIORITY;
bthread_mutex_t mutex5 = BTHREAD_MUTEX_NO_PRIORITY;
bthread_mutex_t mutex6 = BTHREAD_MUTEX_NO_PRIORITY;
bthread_mutex_t mutex_p1 = BTHREAD_MUTEX_PRIORITY(1);
bthread_mutex_t mutex_p2 = BTHREAD_MUTEX_PRIORITY(2);
bthread_mutex_t mutex_p3 = BTHREAD_MUTEX_PRIORITY(3);
static const char *my_prog;

void *self_lock(void *temp)
{
   P(mutex1);
   P(mutex1);
   V(mutex1);

   return NULL;
}

void *nolock(void *temp)
{
   P(mutex2);
   sleep(5);
   V(mutex2);
   return NULL;
}

void *locker(void *temp)
{
   bthread_mutex_t *m = (bthread_mutex_t*) temp;
   P(*m);
   V(*m);
   return NULL;
}

void *rwlocker(void *temp)
{
   brwlock_t *m = (brwlock_t*) temp;
   RwlWritelock(m);
   RwlWritelock(m);

   RwlWriteunlock(m);
   RwlWriteunlock(m);
   return NULL;
}

void *mix_rwl_mutex(void *temp)
{
   brwlock_t *m = (brwlock_t*) temp;
   P(mutex1);
   RwlWritelock(m);
   RwlWriteunlock(m);
   V(mutex1);
   return NULL;
}


void *th2(void *temp)
{
   P(mutex2);
   P(mutex1);

   LmgrDump();

   sleep(10);

   V(mutex1);
   V(mutex2);

   LmgrDump();
   return NULL;
}
void *th1(void *temp)
{
   P(mutex1);
   sleep(2);
   P(mutex2);

   LmgrDump();

   sleep(10);

   V(mutex2);
   V(mutex1);

   LmgrDump();
   return NULL;
}

void *thx(void *temp)
{
   int s= 1 + (int) (500.0 * (rand() / (RAND_MAX + 1.0))) + 200;
   P(mutex1);
   Bmicrosleep(0,s);
   P(mutex2);
   Bmicrosleep(0,s);

   V(mutex2);
   V(mutex1);
   return NULL;
}

void *th3(void *a) {
   while (1) {
      fprintf(stderr, "undertaker sleep()\n");
      sleep(10);
      LmgrDump();
      if (LmgrDetectDeadlock()) {
         LmgrDump();
         exit(1);
      }
   }
   return NULL;
}

void *th_prio(void *a) {
   char buf[512];
   bstrncpy(buf, my_prog, sizeof(buf));
   bstrncat(buf, " priority", sizeof(buf));
   int ret = system(buf);
   return (void*) ret;
}

int err=0;
int nb=0;
void _ok(const char *file, int l, const char *op, int value, const char *label)
{
   nb++;
   if (!value) {
      err++;
      printf("ERR %.30s %s:%i on %s\n", label, file, l, op);
   } else {
      printf("OK  %.30s\n", label);
   }
}

#define ok(x, label) _ok(__FILE__, __LINE__, #x, (x), label)

void _nok(const char *file, int l, const char *op, int value, const char *label)
{
   nb++;
   if (value) {
      err++;
      printf("ERR %.30s %s:%i on !%s\n", label, file, l, op);
   } else {
      printf("OK  %.30s\n", label);
   }
}

#define nok(x, label) _nok(__FILE__, __LINE__, #x, (x), label)

int report()
{
   printf("Result %i/%i OK\n", nb - err, nb);
   return err>0;
}

/*
 * TODO:
 *  - Must detect multiple lock
 *  - lock/unlock in wrong order
 *  - deadlock with 2 or 3 threads
 */
int main(int argc, char **argv)
{
   void *ret=NULL;
   lmgr_thread_t *self;
   pthread_t id1, id2, id3, tab[200];
   bthread_mutex_t bmutex1;
   pthread_mutex_t pmutex2;
   my_prog = argv[0];

   use_undertaker = false;
   LmgrInitThread();
   self = lmgr_get_thread_info();

   if (argc == 2) {             /* do priority check */
      P(mutex_p2);                /* not permited */
      P(mutex_p1);
      V(mutex_p1);                /* never goes here */
      V(mutex_p2);
      return 0;
   }

   pthread_mutex_init(&bmutex1, NULL);
   BthreadMutexSetPriority(&bmutex1, 10);

   pthread_mutex_init(&pmutex2, NULL);
   P(bmutex1);
   ok(self->max_priority == 10, "Check self max_priority");
   P(pmutex2);
   ok(bmutex1.priority == 10, "Check bmutex_set_priority()");
   V(pmutex2);
   V(bmutex1);
   ok(self->max_priority == 0, "Check self max_priority");

   pthread_create(&id1, NULL, self_lock, NULL);
   sleep(2);
   ok(LmgrDetectDeadlock(), "Check self deadlock");
   Lmgr_v(&mutex1.mutex);                /* a bit dirty */
   pthread_join(id1, NULL);


   pthread_create(&id1, NULL, nolock, NULL);
   sleep(2);
   nok(LmgrDetectDeadlock(), "Check for nolock");
   pthread_join(id1, NULL);

   P(mutex1);
   pthread_create(&id1, NULL, locker, &mutex1);
   pthread_create(&id2, NULL, locker, &mutex1);
   pthread_create(&id3, NULL, locker, &mutex1);
   sleep(2);
   nok(LmgrDetectDeadlock(), "Check for multiple lock");
   V(mutex1);
   pthread_join(id1, NULL);
   pthread_join(id2, NULL);
   pthread_join(id3, NULL);


   brwlock_t wr;
   RwlInit(&wr);
   RwlWritelock(&wr);
   RwlWritelock(&wr);
   pthread_create(&id1, NULL, rwlocker, &wr);
   pthread_create(&id2, NULL, rwlocker, &wr);
   pthread_create(&id3, NULL, rwlocker, &wr);
   nok(LmgrDetectDeadlock(), "Check for multiple rwlock");
   RwlWriteunlock(&wr);
   nok(LmgrDetectDeadlock(), "Check for simple rwlock");
   RwlWriteunlock(&wr);
   nok(LmgrDetectDeadlock(), "Check for multiple rwlock");

   pthread_join(id1, NULL);
   pthread_join(id2, NULL);
   pthread_join(id3, NULL);

   RwlWritelock(&wr);
   P(mutex1);
   pthread_create(&id1, NULL, mix_rwl_mutex, &wr);
   nok(LmgrDetectDeadlock(), "Check for mix rwlock/mutex");
   V(mutex1);
   nok(LmgrDetectDeadlock(), "Check for mix rwlock/mutex");
   RwlWriteunlock(&wr);
   nok(LmgrDetectDeadlock(), "Check for mix rwlock/mutex");
   pthread_join(id1, NULL);

   P(mutex5);
   P(mutex6);
   V(mutex5);
   V(mutex6);

   nok(LmgrDetectDeadlock(), "Check for wrong order");

   for(int j=0; j<200; j++) {
      pthread_create(&tab[j], NULL, thx, NULL);
   }
   for(int j=0; j<200; j++) {
      pthread_join(tab[j], NULL);
      if (j%3) { LmgrDetectDeadlock();}
   }
   nok(LmgrDetectDeadlock(), "Check 200 lockers");

   P(mutex4);
   P(mutex5);
   P(mutex6);
   ok(LmgrMutexIsLocked(&mutex6) == 1, "Check if mutex is locked");
   V(mutex6);
   ok(LmgrMutexIsLocked(&mutex6) == 0, "Check if mutex is locked");
   V(mutex5);
   V(mutex4);

   pthread_create(&id1, NULL, th1, NULL);
   sleep(1);
   pthread_create(&id2, NULL, th2, NULL);
   sleep(1);
   ok(LmgrDetectDeadlock(), "Check for deadlock");

   pthread_create(&id3, NULL, th_prio, NULL);
   pthread_join(id3, &ret);
   ok(ret != 0, "Check for priority segfault");

   P(mutex_p1);
   ok(self->max_priority == 1, "Check max_priority 1/4");
   P(mutex_p2);
   ok(self->max_priority == 2, "Check max_priority 2/4");
   P(mutex_p3);
   ok(self->max_priority == 3, "Check max_priority 3/4");
   P(mutex6);
   ok(self->max_priority == 3, "Check max_priority 4/4");
   V(mutex6);
   ok(self->max_priority == 3, "Check max_priority 1/5");
   V(mutex_p3);
   ok(self->max_priority == 2, "Check max_priority 4/5");
   V(mutex_p2);
   ok(self->max_priority == 1, "Check max_priority 4/5");
   V(mutex_p1);
   ok(self->max_priority == 0, "Check max_priority 5/5");


   P(mutex_p1);
   P(mutex_p2);
   P(mutex_p3);
   P(mutex6);
   ok(self->max_priority == 3, "Check max_priority mixed");
   V(mutex_p2);
   ok(self->max_priority == 3, "Check max_priority mixed");
   V(mutex_p1);
   ok(self->max_priority == 3, "Check max_priority mixed");
   V(mutex_p3);
   ok(self->max_priority == 0, "Check max_priority mixed");
   V(mutex6);
   ok(self->max_priority == 0, "Check max_priority mixed");

   P(mutex_p1);
   P(mutex_p2);
   V(mutex_p1);
   V(mutex_p2);

//   LmgrDump();
//
//   pthread_create(&id3, NULL, th3, NULL);
//
//   pthread_join(id1, NULL);
//   pthread_join(id2, NULL);
   LmgrCleanupMain();
   sm_check(__FILE__, __LINE__, false);
   return report();
}

#endif
