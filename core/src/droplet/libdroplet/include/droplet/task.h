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
#ifndef BAREOS_DROPLET_LIBDROPLET_INCLUDE_DROPLET_TASK_H_
#define BAREOS_DROPLET_LIBDROPLET_INCLUDE_DROPLET_TASK_H_

#define DPL_TASK_DEFAULT_N_WORKERS 10

typedef void (*dpl_task_func_t)(void *handle);

typedef struct dpl_task
{
  struct dpl_task *next;
  dpl_task_func_t func;
} dpl_task_t;

typedef struct dpl_task_pool
{
  dpl_ctx_t *ctx;
  int n_workers;
  int n_workers_needed;
  int n_workers_running;
  dpl_task_t *task_queue;
  dpl_task_t *task_last;
  pthread_mutex_t task_lock;
  pthread_cond_t task_cond;
  pthread_cond_t idle_cond;
  int n_tasks;
  pthread_attr_t joinable_thread_attr;
  int canceled;
  int enable_congestion_logging;
  int congestion_threshold;
  int congestion_log_threshold;
  char *name;
  int worker_id;
} dpl_task_pool_t;

dpl_task_t *dpl_task_get(dpl_task_pool_t *pool);
void dpl_task_pool_put(dpl_task_pool_t *pool, dpl_task_t *task);
dpl_task_pool_t *dpl_task_pool_create(dpl_ctx_t *ctx, char *name, int n_workers);
void dpl_task_pool_cancel(dpl_task_pool_t *pool);
void dpl_task_pool_destroy(dpl_task_pool_t *pool);
int dpl_task_pool_set_workers(dpl_task_pool_t *pool, int n_workers);
int dpl_task_pool_get_workers(dpl_task_pool_t *pool);
void dpl_task_pool_enable_congestion(dpl_task_pool_t *pool, int threshold);
void dpl_task_pool_wait_idle(dpl_task_pool_t *pool);

#endif  // BAREOS_DROPLET_LIBDROPLET_INCLUDE_DROPLET_TASK_H_
