#include <pthread.h>
#include <check.h>
#include <droplet.h>
#include <droplet/task.h>
#include <unistd.h>

#include "utest_main.h"


//
// Retrieve a fake ctx for task pool initialization
//
    static dpl_ctx_t *
get_ctx (void)
{
  static pthread_once_t once = PTHREAD_ONCE_INIT;
  static dpl_ctx_t      t;

  void init (void)
    {
      bzero(&t, sizeof(t));
      pthread_mutex_init(&t.lock, NULL);
      return;
    }
  (void) pthread_once(&once, init);
  return &t;
}


START_TEST(taskpool_test)
{
  dpl_task_pool_t    * p;
  p = dpl_task_pool_create(get_ctx(), "taskpool_test", 100);
  fail_if(NULL == p);
  dpl_task_pool_destroy(p);

  p = dpl_task_pool_create(get_ctx(), "taskpool_test", 100);
  fail_if(NULL == p);
  dpl_task_pool_cancel(p);

  p = dpl_task_pool_create(get_ctx(), "taskpool_test", 100);
  fail_if(NULL == p);
    //
    // Increase and decrease the number of workers
    //
  int   wn[] = { 1,  100, 50, 200, 100};
  for (unsigned int i = 0; i < sizeof(wn) / sizeof(wn[0]); i++)
    {
      int   ret;
      ret = dpl_task_pool_set_workers(p, wn[i]);
      fail_unless(0 == ret, NULL);
      usleep(100000); // give a chance to the threads to stop themselves
      ret = dpl_task_pool_get_workers(p);
      fail_unless(wn[i] == ret, "Fail at loop iteration %d: expected workers %d, retrieved workers %d", i, wn[i], ret);
    }

    //
    // Start thread, wait, and cancel combo
    //
  static int                incr = 0;
  static pthread_mutex_t    mtx = PTHREAD_MUTEX_INITIALIZER;
  void * tf (void * arg)
    {
      unsigned int  i = 0;
      for (i = 0; i < 10000; i++)
        {
          pthread_mutex_lock(&mtx);
          incr++;
          pthread_mutex_unlock(&mtx);
        }
      return NULL;
    }

  dpl_task_t    * tasks = calloc(1000, sizeof(*tasks));
  fail_if(NULL == tasks, NULL);
  for (unsigned int i = 0; i < 100; i++)
    {
      tasks[i].func = tf;
      dpl_task_pool_put(p, &tasks[i]);
    }
  dpl_task_pool_wait_idle(p);
  fail_unless(100 * 10000 == incr, NULL);

  incr = 0;
  for (unsigned int i = 0; i < 100; i++)
    {
      tasks[i].func = tf;
      dpl_task_pool_put(p, &tasks[i]);
    }
  dpl_task_pool_cancel(p);
  fail_unless(100 * 10000 == incr, NULL);
  dpl_task_pool_destroy(p);
}
END_TEST


    Suite *
taskpool_suite ()
{
  Suite * s = suite_create("taskpool");
  TCase * t = tcase_create("base");
  tcase_add_test(t, taskpool_test);
  suite_add_tcase(s, t);
  return s;
}
