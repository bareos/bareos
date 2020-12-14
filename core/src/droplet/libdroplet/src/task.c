/*
 * Copyright (C) 2010 SCALITY SA. All rights reserved.
 * http://www.scality.com
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY SCALITY SA ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL SCALITY SA OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation
 * are those of the authors and should not be interpreted as representing
 * official policies, either expressed or implied, of SCALITY SA.
 *
 * https://github.com/scality/Droplet
 */
#include "dropletp.h"

dpl_task_t *
dpl_task_get(dpl_task_pool_t *pool)
{
  dpl_task_t *task = NULL;

  pthread_mutex_lock(&pool->task_lock);

  while (1) 
    {
      if (NULL != pool->task_queue)
	{
	  task = pool->task_queue;
	  pool->task_queue = task->next;
	  pool->n_tasks--;
	  if (NULL == pool->task_queue)
	    pool->task_last = NULL;

	  if (pool->enable_congestion_logging
	      && pool->congestion_log_threshold > pool->congestion_threshold
	      && pool->n_tasks < MAX(pool->congestion_log_threshold / 2,
				     pool->congestion_threshold))
	    {
	      pool->congestion_log_threshold =
		MAX(pool->congestion_log_threshold * 2 / 3,
		    pool->congestion_threshold);
	      if (pool->n_tasks < pool->congestion_threshold)
		{
		  DPL_TRACE(pool->ctx, DPL_TRACE_WARN,
			     "pool %s end of congestion n_tasks %d threshold %d",
			     pool->name,
			     pool->n_tasks,
			     pool->congestion_threshold);
		}
	    }

	  break;
	}

      pool->n_workers_running--;
      if (pool->n_workers_running == 0
	  && pool->n_tasks == 0)
	pthread_cond_broadcast(&pool->idle_cond);

      if ( pool->canceled
	   || pool->n_workers > pool->n_workers_needed)
	{
	  pool->n_workers--;
	  pthread_cond_broadcast(&pool->task_cond);
	  pthread_mutex_unlock(&pool->task_lock);
	  pthread_exit(NULL);
	}

      pthread_cond_wait(&pool->task_cond, &pool->task_lock);

      pool->n_workers_running++;
    }

  pthread_mutex_unlock(&pool->task_lock);

  return task;
}

static void *worker_main(void *arg)
{
  dpl_task_pool_t *pool = arg;
  int my_id;
  char my_name[16];

  pthread_mutex_lock(&pool->task_lock);
  my_id = ++pool->worker_id;
  pool->n_workers_running++;
  pthread_mutex_unlock(&pool->task_lock);

  snprintf(my_name, sizeof(my_name), "%s%d", pool->name, my_id);
  prctl(PR_SET_NAME, my_name);
 
  while (1)
    {
      dpl_task_t *task;

      task = dpl_task_get(pool);
      assert(NULL != task); 

      task->func(task);
    }

  return NULL;
}

void
dpl_task_pool_put(dpl_task_pool_t *pool, dpl_task_t *task)
{
  task->next = NULL;
  
  pthread_mutex_lock(&pool->task_lock);

  if (NULL == pool->task_queue)
    {
      pool->task_queue = pool->task_last = task;
    }
  else
    {
      pool->task_last->next = task;
      pool->task_last = task;
    }

  pool->n_tasks++;

  if (pool->enable_congestion_logging
      && pool->n_tasks >= pool->congestion_log_threshold)
    {
      DPL_TRACE(pool->ctx, DPL_TRACE_WARN,
		 "pool %s congestion reached n_tasks %d threshold %d",
		 pool->name, pool->n_tasks, pool->congestion_threshold);
      pool->congestion_log_threshold = pool->congestion_log_threshold * 3 / 2;
    }

  pthread_cond_signal(&pool->task_cond);
  pthread_mutex_unlock(&pool->task_lock);
}


void dpl_task_pool_cancel(dpl_task_pool_t *pool)
{
  pthread_mutex_lock(&pool->task_lock);

  pool->canceled = 1;
  pthread_cond_broadcast(&pool->task_cond);

  while (pool->n_workers != 0)
    pthread_cond_wait(&pool->task_cond, &pool->task_lock);

  pthread_mutex_unlock(&pool->task_lock);
}

void dpl_task_pool_destroy(dpl_task_pool_t *pool)
{
  dpl_task_pool_wait_idle(pool);

  if (0 == pool->canceled)
    dpl_task_pool_cancel(pool);

  pthread_attr_destroy(&pool->joinable_thread_attr);
  pthread_mutex_destroy(&pool->task_lock);
  pthread_cond_destroy(&pool->task_cond);
  pthread_cond_destroy(&pool->idle_cond);
  free(pool->name);
  free(pool);
}

dpl_task_pool_t *
dpl_task_pool_create(dpl_ctx_t *ctx, char *name, int n_workers)
{
  int ret;
  dpl_task_pool_t *pool = NULL;

  /* name + id should fit in 16 chars (prctl limit) */
  if (NULL == name || strlen(name) > 13)
    {
      DPL_TRACE(ctx, DPL_TRACE_ERR, "invalid pool name: %s",
		 name ? name : "NULL");
      goto bad;
    }

  if (0 >= n_workers)
    n_workers = DPL_TASK_DEFAULT_N_WORKERS;

  pool = malloc(sizeof (*pool));
  if (NULL == pool)
    goto bad;

  memset(pool, 0, sizeof(*pool));
  pool->ctx = ctx;
  pool->n_workers = 0;
  pool->n_workers_needed = n_workers;
  pool->task_queue = NULL;
  pool->task_last = NULL;
  pool->n_tasks = 0;
  pool->canceled = 0;
  pool->worker_id = 0;
  pool->name = strdup(name);
  if (NULL == pool->name)
    {
      DPL_TRACE(ctx, DPL_TRACE_ERR, "malloc");
      goto bad;
    }

  pthread_mutex_init(&pool->task_lock, NULL);
  pthread_cond_init(&pool->task_cond, NULL);
  pthread_cond_init(&pool->idle_cond, NULL);

  ret = pthread_attr_init(&pool->joinable_thread_attr);
  if (0 != ret)
    {
      DPL_TRACE(ctx, DPL_TRACE_ERR, "pthread_attr_init failed %d", ret);
      goto bad;
    }
  
  pthread_attr_setdetachstate(&pool->joinable_thread_attr,
			      PTHREAD_CREATE_DETACHED);

  ret = dpl_task_pool_set_workers(pool, n_workers);
  if (0 != ret)
    {
      DPL_TRACE(ctx, DPL_TRACE_ERR, "could not create threads");
      goto bad;
    }

  return pool;

 bad:

  if (NULL != pool)
    dpl_task_pool_destroy(pool);
  
  return NULL;
}

int dpl_task_pool_set_workers(dpl_task_pool_t *pool, int n_workers_needed)
{
  int i, ret = 0;

  if (n_workers_needed < 0)
    {
      DPL_TRACE(pool->ctx, DPL_TRACE_ERR, "invalid number of workers %d",
		 n_workers_needed);
      return -1;
    }

  pthread_mutex_lock(&pool->task_lock);

  pool->n_workers_needed = n_workers_needed;

  if (n_workers_needed < pool->n_workers)
    pthread_cond_broadcast(&pool->task_cond);
  else
    {
      for (i = pool->n_workers; i < n_workers_needed; i++)
	{
	  pthread_t thread;
	  ret = pthread_create(&thread, &pool->joinable_thread_attr,
			       worker_main, pool);
	  if (ret == 0)
	    pool->n_workers++;
	  else
	    break;
	}
    }

  pthread_mutex_unlock(&pool->task_lock);

  if (0 != ret)
    {
      DPL_TRACE(pool->ctx, DPL_TRACE_ERR, "pthread_create %d (%s)",
		 ret, strerror(ret));
      return -1;
    }

  return 0;
}

int dpl_task_pool_get_workers(dpl_task_pool_t *pool)
{
  int n_workers;

  pthread_mutex_lock(&pool->task_lock);
  n_workers = pool->n_workers;
  pthread_mutex_unlock(&pool->task_lock);

  return n_workers;
}

void dpl_task_pool_enable_congestion(dpl_task_pool_t *pool, int threshold)
{
  pool->enable_congestion_logging = 1;
  pool->congestion_threshold = threshold;
  pool->congestion_log_threshold = threshold;
}

void dpl_task_pool_wait_idle(dpl_task_pool_t *pool)
{
  pthread_mutex_lock(&pool->task_lock);

  while (pool->n_tasks != 0
	 || pool->n_workers_running != 0)
    pthread_cond_wait(&pool->idle_cond, &pool->task_lock);

  pthread_mutex_unlock(&pool->task_lock);
}
