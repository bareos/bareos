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
#include "droplet/async.h"

/** @file */

//#define DPRINTF(fmt,...) fprintf(stderr, fmt, ##__VA_ARGS__)
#define DPRINTF(fmt,...)

#define DUP_IF_NOT_NULL(Struct, Member)                 \
  if (NULL != Member)                                   \
    {                                                   \
      Struct.Member = strdup(Member);                   \
      if (Struct.Member == NULL)                        \
        goto bad;                                       \
    }

#define FREE_IF_NOT_NULL(StructMember)          \
  if (NULL != StructMember)                     \
    free(StructMember);

dpl_buf_t *
dpl_buf_new()
{
  dpl_buf_t *buf = NULL;

  buf = malloc(sizeof (dpl_buf_t));
  if (NULL == buf)
    return NULL;
  
  buf->ptr = NULL;
  buf->size = 0;
  buf->refcnt = 0;

  return buf;
}

void
dpl_buf_acquire(dpl_buf_t *buf)
{
  buf->refcnt++;
}

void
dpl_buf_release(dpl_buf_t *buf)
{
  buf->refcnt--;
  if (0 == buf->refcnt)
    {
      free(buf->ptr);
      free(buf);
    }
}

void
dpl_async_task_free(dpl_async_task_t *task)
{
  switch (task->type)
    {
    case DPL_TASK_LIST_ALL_MY_BUCKETS:
      /* input */
      /* output */
      if (NULL != task->u.list_all_my_buckets.buckets)
        dpl_vec_buckets_free(task->u.list_all_my_buckets.buckets);
      break ;
    case DPL_TASK_LIST_BUCKET:
      /* input */
      FREE_IF_NOT_NULL(task->u.list_bucket.bucket);
      FREE_IF_NOT_NULL(task->u.list_bucket.prefix);
      FREE_IF_NOT_NULL(task->u.list_bucket.delimiter);
      /* output */
      if (NULL != task->u.list_bucket.objects)
        dpl_vec_objects_free(task->u.list_bucket.objects);
      if (NULL != task->u.list_bucket.common_prefixes)
        dpl_vec_common_prefixes_free(task->u.list_bucket.common_prefixes);
      break ;
    case DPL_TASK_MAKE_BUCKET:
      /* input */
      FREE_IF_NOT_NULL(task->u.make_bucket.bucket);
      /* output */
      break ;
    case DPL_TASK_DELETE_BUCKET:
      /* input */
      FREE_IF_NOT_NULL(task->u.delete_bucket.bucket);
      /* output */
      break ;
    case DPL_TASK_POST:
    case DPL_TASK_POST_ID:
      /* input */
      FREE_IF_NOT_NULL(task->u.post.bucket);
      FREE_IF_NOT_NULL(task->u.post.resource);
      if (NULL != task->u.post.option)
        dpl_option_free(task->u.post.option);
      if (NULL != task->u.post.condition)
        dpl_condition_free(task->u.post.condition);
      if (NULL != task->u.put.range)
        dpl_range_free(task->u.put.range);
      if (NULL != task->u.post.metadata)
        dpl_dict_free(task->u.post.metadata);
      if (NULL != task->u.post.sysmd)
        dpl_sysmd_free(task->u.post.sysmd);
      if (NULL != task->u.post.buf)
        dpl_buf_release(task->u.post.buf);
      if (NULL != task->u.post.query_params)
        dpl_dict_free(task->u.post.query_params);
      /* output */
      break ;
    case DPL_TASK_PUT:
    case DPL_TASK_PUT_ID:
      /* input */
      FREE_IF_NOT_NULL(task->u.put.bucket);
      FREE_IF_NOT_NULL(task->u.put.resource);
      if (NULL != task->u.put.option)
        dpl_option_free(task->u.put.option);
      if (NULL != task->u.put.condition)
        dpl_condition_free(task->u.put.condition);
      if (NULL != task->u.put.range)
        dpl_range_free(task->u.put.range);
      if (NULL != task->u.put.metadata)
        dpl_dict_free(task->u.put.metadata);
      if (NULL != task->u.put.sysmd)
        dpl_sysmd_free(task->u.put.sysmd);
      if (NULL != task->u.put.buf)
        dpl_buf_release(task->u.put.buf);
      /* output */
      break ;
    case DPL_TASK_GET:
    case DPL_TASK_GET_ID:
      /* inget */
      FREE_IF_NOT_NULL(task->u.get.bucket);
      FREE_IF_NOT_NULL(task->u.get.resource);
      if (NULL != task->u.get.option)
        dpl_option_free(task->u.get.option);
      if (NULL != task->u.get.condition)
        dpl_condition_free(task->u.get.condition);
      if (NULL != task->u.get.range)
        dpl_range_free(task->u.get.range);
      /* output */
      if (NULL != task->u.get.metadata)
        dpl_dict_free(task->u.get.metadata);
      if (NULL != task->u.get.buf)
        dpl_buf_release(task->u.get.buf);
      break ;
    case DPL_TASK_HEAD:
    case DPL_TASK_HEAD_ID:
      /* inhead */
      FREE_IF_NOT_NULL(task->u.head.bucket);
      FREE_IF_NOT_NULL(task->u.head.resource);
      if (NULL != task->u.head.option)
        dpl_option_free(task->u.head.option);
      if (NULL != task->u.head.condition)
        dpl_condition_free(task->u.head.condition);
      /* output */
      if (NULL != task->u.head.metadata)
        dpl_dict_free(task->u.head.metadata);
      break ;
    case DPL_TASK_DELETE:
    case DPL_TASK_DELETE_ID:
      /* indelete */
      FREE_IF_NOT_NULL(task->u.delete.bucket);
      FREE_IF_NOT_NULL(task->u.delete.resource);
      if (NULL != task->u.delete.option)
        dpl_option_free(task->u.delete.option);
      if (NULL != task->u.delete.condition)
        dpl_condition_free(task->u.delete.condition);
      /* output */
      break ;
    case DPL_TASK_COPY:
    case DPL_TASK_COPY_ID:
      /* incopy */
      FREE_IF_NOT_NULL(task->u.copy.src_bucket);
      FREE_IF_NOT_NULL(task->u.copy.src_resource);
      FREE_IF_NOT_NULL(task->u.copy.dst_bucket);
      FREE_IF_NOT_NULL(task->u.copy.dst_resource);
      if (NULL != task->u.copy.option)
        dpl_option_free(task->u.copy.option);
      if (NULL != task->u.copy.metadata)
        dpl_dict_free(task->u.copy.metadata);
      if (NULL != task->u.copy.sysmd)
        dpl_sysmd_free(task->u.copy.sysmd);
      if (NULL != task->u.copy.condition)
        dpl_condition_free(task->u.copy.condition);
      /* output */
      break ;
    }
  free(task);
}

static void
async_do(void *arg)
{
  dpl_async_task_t *task = (dpl_async_task_t *) arg;

  switch (task->type)
    {
    case DPL_TASK_LIST_ALL_MY_BUCKETS:
      task->ret = dpl_list_all_my_buckets(task->ctx, 
                                          &task->u.list_all_my_buckets.buckets);
      break ;
    case DPL_TASK_LIST_BUCKET:
      task->ret = dpl_list_bucket(task->ctx, 
                                  task->u.list_bucket.bucket,
                                  task->u.list_bucket.prefix,
                                  task->u.list_bucket.delimiter,
                                  task->u.list_bucket.max_keys,
                                  &task->u.list_bucket.objects, 
                                  &task->u.list_bucket.common_prefixes);
      break ;
    case DPL_TASK_MAKE_BUCKET:
      task->ret = dpl_make_bucket(task->ctx, 
                                  task->u.make_bucket.bucket,
                                  task->u.make_bucket.location_constraint,
                                  task->u.make_bucket.canned_acl);
      break ;
    case DPL_TASK_DELETE_BUCKET:
      task->ret = dpl_delete_bucket(task->ctx, 
                                    task->u.make_bucket.bucket);
      break ;
    case DPL_TASK_POST:
      task->ret = dpl_post(task->ctx, 
                           task->u.post.bucket,
                           task->u.post.resource,
                           task->u.post.option,
                           task->u.post.object_type,
                           task->u.post.condition,
                           task->u.post.range,
                           task->u.post.metadata,
                           task->u.post.sysmd,
                           NULL != task->u.post.buf ? dpl_buf_ptr(task->u.post.buf) : NULL,
                           NULL != task->u.post.buf ? dpl_buf_size(task->u.post.buf) : 0,
                           task->u.post.query_params,
                           &task->u.post.sysmd_returned);
      break ;
    case DPL_TASK_PUT:
      task->ret = dpl_put(task->ctx, 
                          task->u.put.bucket,
                          task->u.put.resource,
                          task->u.put.option,
                          task->u.put.object_type,
                          task->u.put.condition,
                          task->u.put.range,  
                          task->u.put.metadata,
                          task->u.put.sysmd,
                          NULL != task->u.put.buf ? dpl_buf_ptr(task->u.put.buf) : NULL,
                          NULL != task->u.put.buf ? dpl_buf_size(task->u.put.buf) : 0);
      break ;
    case DPL_TASK_GET:
      task->u.get.buf = dpl_buf_new();
      if (NULL == task->u.get.buf)
        {
          task->ret = DPL_ENOMEM;
          break ;
        }
      dpl_buf_acquire(task->u.get.buf);
      task->ret = dpl_get(task->ctx, 
                          task->u.get.bucket,
                          task->u.get.resource,
                          task->u.get.option,
                          task->u.get.object_type,
                          task->u.get.condition,
                          task->u.get.range,  
                          &dpl_buf_ptr(task->u.get.buf),
                          &dpl_buf_size(task->u.get.buf),
                          &task->u.get.metadata,
                          &task->u.get.sysmd);
      break ;
    case DPL_TASK_HEAD:
      task->ret = dpl_head(task->ctx, 
                           task->u.head.bucket,
                           task->u.head.resource,
                           task->u.head.option,
                           task->u.head.object_type,
                           task->u.head.condition,
                           &task->u.head.metadata,
                           &task->u.head.sysmd);
      break ;
    case DPL_TASK_DELETE:
      task->ret = dpl_delete(task->ctx, 
                             task->u.delete.bucket,
                             task->u.delete.resource,
                             task->u.delete.option,
                             task->u.delete.object_type,
                             task->u.delete.condition);
      break ;
    case DPL_TASK_COPY:
      task->ret = dpl_copy(task->ctx, 
                           task->u.copy.src_bucket,
                           task->u.copy.src_resource,
                           task->u.copy.dst_bucket,
                           task->u.copy.dst_resource,
                           task->u.copy.option,
                           task->u.copy.object_type,
                           task->u.copy.copy_directive,
                           task->u.copy.metadata,
                           task->u.copy.sysmd,
                           task->u.copy.condition);
      break ;
    case DPL_TASK_POST_ID:
      task->ret = dpl_post_id(task->ctx, 
                              task->u.post.bucket,
                              task->u.post.resource,
                              task->u.post.option,
                              task->u.post.object_type,
                              task->u.post.condition,
                              task->u.post.range,
                              task->u.post.metadata,
                              task->u.post.sysmd,
                              NULL != task->u.post.buf ? dpl_buf_ptr(task->u.post.buf) : NULL,
                              NULL != task->u.post.buf ? dpl_buf_size(task->u.post.buf) : 0,
                              task->u.post.query_params,
                              &task->u.post.sysmd_returned);
      break ;
    case DPL_TASK_PUT_ID:
      task->ret = dpl_put_id(task->ctx, 
                             task->u.put.bucket,
                             task->u.put.resource,
                             task->u.put.option,
                             task->u.put.object_type,
                             task->u.put.condition,
                             task->u.put.range,  
                             task->u.put.metadata,
                             task->u.put.sysmd,
                             NULL != task->u.put.buf ? dpl_buf_ptr(task->u.put.buf) : NULL,
                             NULL != task->u.put.buf ? dpl_buf_size(task->u.put.buf) : 0);
      break ;
    case DPL_TASK_GET_ID:
      task->u.get.buf = dpl_buf_new();
      if (NULL == task->u.get.buf)
        {
          task->ret = DPL_ENOMEM;
          break ;
        }
      dpl_buf_acquire(task->u.get.buf);
      task->ret = dpl_get_id(task->ctx, 
                             task->u.get.bucket,
                             task->u.get.resource,
                             task->u.get.option,
                             task->u.get.object_type,
                             task->u.get.condition,
                             task->u.get.range,  
                             &dpl_buf_ptr(task->u.get.buf),
                             &dpl_buf_size(task->u.get.buf),
                             &task->u.get.metadata,
                             &task->u.get.sysmd);
      break ;
    case DPL_TASK_HEAD_ID:
      task->ret = dpl_head_id(task->ctx, 
                              task->u.head.bucket,
                              task->u.head.resource,
                              task->u.head.option,
                              task->u.head.object_type,
                              task->u.head.condition,
                              &task->u.head.metadata,
                              &task->u.head.sysmd);
      break ;
    case DPL_TASK_DELETE_ID:
      task->ret = dpl_delete_id(task->ctx, 
                                task->u.delete.bucket,
                                task->u.delete.resource,
                                task->u.delete.option,
                                task->u.delete.object_type,
                                task->u.delete.condition);
      break ;
    case DPL_TASK_COPY_ID:
      task->ret = dpl_copy_id(task->ctx, 
                              task->u.copy.src_bucket,
                              task->u.copy.src_resource,
                              task->u.copy.dst_bucket,
                              task->u.copy.dst_resource,
                              task->u.copy.option,
                              task->u.copy.object_type,
                              task->u.copy.copy_directive,
                              task->u.copy.metadata,
                              task->u.copy.sysmd,
                              task->u.copy.condition);
      break ;
    }
  if (NULL != task->cb_func)
    task->cb_func(task->cb_arg);
}

/**
 * list all buckets
 *
 * @param ctx the droplect context
 * @param cb_func all callback returning a vector of dpl_bucket_t *
 * @param cb_arg closure
 *
 * @return task
 */
dpl_task_t *
dpl_list_all_my_buckets_async_prepare(dpl_ctx_t *ctx)
{
  dpl_async_task_t *task = NULL;

  task = calloc(1, sizeof (*task));
  if (NULL == task)
    goto bad;

  task->ctx = ctx;
  task->type = DPL_TASK_LIST_ALL_MY_BUCKETS;
  task->task.func = async_do;

  return (dpl_task_t *) task;

 bad:

  if (NULL != task)
    dpl_async_task_free(task);

  return NULL;
}

/**
 * list bucket or directory
 *
 * @param ctx the droplet context
 * @param bucket can be NULL
 * @param prefix directory can be NULL
 * @param delimiter e.g. "/" can be NULL
 * @param cb_func
 * @param cb_arg
 *
 * @return task
 */
dpl_task_t *
dpl_list_bucket_async_prepare(dpl_ctx_t *ctx,
                              const char *bucket,
                              const char *prefix,
                              const char *delimiter)
{
  dpl_async_task_t *task = NULL;

  task = calloc(1, sizeof (*task));
  if (NULL == task)
    goto bad;

  task->ctx = ctx;
  task->type = DPL_TASK_LIST_BUCKET;
  task->task.func = async_do;
  DUP_IF_NOT_NULL(task->u.list_bucket, bucket);
  DUP_IF_NOT_NULL(task->u.list_bucket, prefix);
  DUP_IF_NOT_NULL(task->u.list_bucket, delimiter);

  return (dpl_task_t *) task;

 bad:

  if (NULL != task)
    dpl_async_task_free(task);
  
  return NULL;
}

/**
 * make a bucket
 *
 * @param ctx the droplet context
 * @param bucket can be NULL
 * @param location_constraint geographic location
 * @param canned_acl simplified ACL
 *
 * @return task
 */
dpl_task_t *
dpl_make_bucket_async_prepare(dpl_ctx_t *ctx,
                              const char *bucket, 
                              dpl_location_constraint_t location_constraint,
                              dpl_canned_acl_t canned_acl)
{
  dpl_async_task_t *task = NULL;

  task = calloc(1, sizeof (*task));
  if (NULL == task)
    goto bad;

  task->ctx = ctx;
  task->type = DPL_TASK_MAKE_BUCKET;
  task->task.func = async_do;
  DUP_IF_NOT_NULL(task->u.make_bucket, bucket);
  task->u.make_bucket.location_constraint = location_constraint;
  task->u.make_bucket.canned_acl = canned_acl;

  return (dpl_task_t *) task;

 bad:

  if (NULL != task)
    dpl_async_task_free(task);
  
  return NULL;
}

/**
 * delete a resource
 *
 * @param ctx the droplet context
 * @param bucket can be NULL
 *
 * @return task
 */
dpl_task_t  *
dpl_delete_bucket_prepare(dpl_ctx_t *ctx,
                          const char *bucket)
{
  dpl_async_task_t *task = NULL;

  task = calloc(1, sizeof (*task));
  if (NULL == task)
    goto bad;

  task->ctx = ctx;
  task->type = DPL_TASK_DELETE_BUCKET;
  task->task.func = async_do;
  DUP_IF_NOT_NULL(task->u.delete_bucket, bucket);

  return (dpl_task_t *) task;

 bad:

  if (NULL != task)
    dpl_async_task_free(task);
  
  return NULL;
}

/** 
 * create or post data into a resource
 *
 * @note this function is expected to return a newly created object
 * 
 * @param ctx the droplet context
 * @param bucket can be NULL
 * @param resource can be NULL
 * @param option DPL_OPTION_HTTP_COMPAT use if possible the HTTP compat mode
 * @param object_type DPL_FTYPE_REG create a file
 * @param metadata the user metadata. optional
 * @param sysmd the system metadata. optional
 * @param buf the data buffer
 * @param query_params can be NULL
 * @param returned_sysmdp the returned system metadata passed through stack
 * 
 * @return DPL_SUCCESS
 * @return DPL_FAILURE
 */
dpl_task_t *
dpl_post_async_prepare(dpl_ctx_t *ctx,
                       const char *bucket,
                       const char *resource,
                       const dpl_option_t *option,
                       dpl_ftype_t object_type,
                       const dpl_condition_t *condition,
                       const dpl_range_t *range,
                       const dpl_dict_t *metadata,
                       const dpl_sysmd_t *sysmd,
                       dpl_buf_t *buf,
                       const dpl_dict_t *query_params)
{
  dpl_async_task_t *task = NULL;

  task = calloc(1, sizeof (*task));
  if (NULL == task)
    goto bad;

  task->ctx = ctx;
  task->type = DPL_TASK_POST;
  task->task.func = async_do;
  DUP_IF_NOT_NULL(task->u.post, bucket);
  DUP_IF_NOT_NULL(task->u.post, resource);
  if (NULL != option)
    task->u.post.option = dpl_option_dup(option);
  task->u.post.object_type = object_type;
  if (NULL != condition)
    task->u.post.condition = dpl_condition_dup(condition);
  if (NULL != range)
    task->u.post.range = dpl_range_dup(range);
  if (NULL != metadata)
    task->u.post.metadata = dpl_dict_dup(metadata);
  if (NULL != sysmd)
    task->u.post.sysmd = dpl_sysmd_dup(sysmd);
  if (NULL != buf)
    {
      task->u.post.buf = buf;
      dpl_buf_acquire(buf);
    }
  if (NULL != query_params)
    task->u.post.query_params = dpl_dict_dup(query_params);

  return (dpl_task_t *) task;

 bad:

  if (NULL != task)
    dpl_async_task_free(task);
  
  return NULL;
}

/**
 * put a resource
 *
 * @param ctx the droplet context
 * @param bucket optional
 * @param resource mandatory
 * @param option DPL_OPTION_HTTP_COMPAT use if possible the HTTP compat mode
 * @param object_type DPL_FTYPE_REG create a file
 * @param object_type DPL_FTYPE_DIR create a directory
 * @param condition the optional condition
 * @param range optional range
 * @param metadata the optional user metadata
 * @param sysmd the optional system metadata
 * @param buf the data buffer
 *
 * @return DPL_SUCCESS
 * @return DPL_FAILURE
 * @return DPL_EEXIST
 */
dpl_task_t *
dpl_put_async_prepare(dpl_ctx_t *ctx,
                      const char *bucket,
                      const char *resource,
                      const dpl_option_t *option,
                      dpl_ftype_t object_type,
                      const dpl_condition_t *condition,
                      const dpl_range_t *range,
                      const dpl_dict_t *metadata,
                      const dpl_sysmd_t *sysmd,
                      dpl_buf_t *buf)
{
  dpl_async_task_t *task = NULL;

  task = calloc(1, sizeof (*task));
  if (NULL == task)
    goto bad;

  task->ctx = ctx;
  task->type = DPL_TASK_PUT;
  task->task.func = async_do;
  DUP_IF_NOT_NULL(task->u.put, bucket);
  DUP_IF_NOT_NULL(task->u.put, resource);
  if (NULL != option)
    task->u.put.option = dpl_option_dup(option);
  task->u.put.object_type = object_type;
  if (NULL != condition)
    task->u.put.condition = dpl_condition_dup(condition);
  if (NULL != range)
    task->u.put.range = dpl_range_dup(range);
  if (NULL != metadata)
    task->u.put.metadata = dpl_dict_dup(metadata);
  if (NULL != sysmd)
    task->u.put.sysmd = dpl_sysmd_dup(sysmd);
  if (NULL != buf)
    {
      task->u.put.buf = buf;
      dpl_buf_acquire(buf);
    }

  return (dpl_task_t *) task;

 bad:

  if (NULL != task)
    dpl_async_task_free(task);
  
  return NULL;
}

/** 
 * get a resource with range
 * 
 * @param ctx the droplet context
 * @param bucket the optional bucket
 * @param resource the mandat resource
 * @param option DPL_OPTION_HTTP_COMPAT use if possible the HTTP compat mode
 * @param object_type DPL_FTYPE_ANY get any type of resource
 * @param condition the optional condition
 * @param range the optional range
 * 
 * @return DPL_SUCCESS
 * @return DPL_FAILURE
 * @return DPL_ENOENT resource does not exist
 */
dpl_task_t *
dpl_get_async_prepare(dpl_ctx_t *ctx,
                      const char *bucket,
                      const char *resource,
                      const dpl_option_t *option,
                      dpl_ftype_t object_type,
                      const dpl_condition_t *condition,
                      const dpl_range_t *range)
{
  dpl_async_task_t *task = NULL;

  task = calloc(1, sizeof (*task));
  if (NULL == task)
    goto bad;

  task->ctx = ctx;
  task->type = DPL_TASK_GET;
  task->task.func = async_do;
  DUP_IF_NOT_NULL(task->u.get, bucket);
  DUP_IF_NOT_NULL(task->u.get, resource);
  if (NULL != option)
    task->u.get.option = dpl_option_dup(option);
  task->u.get.object_type = object_type;
  if (NULL != condition)
    task->u.get.condition = dpl_condition_dup(condition);
  if (NULL != range)
    task->u.get.range = dpl_range_dup(range);

  return (dpl_task_t *) task;

 bad:

  if (NULL != task)
    dpl_async_task_free(task);
  
  return NULL;
}

/** 
 * get user and system metadata
 * 
 * @param ctx the droplet context
 * @param bucket the optional bucket
 * @param resource the mandat resource
 * @param option DPL_OPTION_HTTP_COMPAT use if possible the HTTP compat mode
 * @param object_type DPL_FTYPE_ANY get any type of resource
 * @param condition the optional condition
 * @param metadatap the returned user metadata client shall free
 * @param sysmdp the returned system metadata passed through stack
 * 
 * @return DPL_SUCCESS
 * @return DPL_FAILURE
 * @return DPL_ENOENT resource does not exist
 */
dpl_task_t *
dpl_head_async_prepare(dpl_ctx_t *ctx,
                       const char *bucket,
                       const char *resource,
                       const dpl_option_t *option,
                       dpl_ftype_t object_type,
                       const dpl_condition_t *condition)
{
  dpl_async_task_t *task = NULL;

  task = calloc(1, sizeof (*task));
  if (NULL == task)
    goto bad;

  task->ctx = ctx;
  task->type = DPL_TASK_HEAD;
  task->task.func = async_do;
  DUP_IF_NOT_NULL(task->u.head, bucket);
  DUP_IF_NOT_NULL(task->u.head, resource);
  if (NULL != option)
    task->u.head.option = dpl_option_dup(option);
  task->u.head.object_type = object_type;
  if (NULL != condition)
    task->u.head.condition = dpl_condition_dup(condition);

  return (dpl_task_t *) task;

 bad:

  if (NULL != task)
    dpl_async_task_free(task);
  
  return NULL;
}

/** 
 * delete a resource
 * 
 * @param ctx the droplet context
 * @param bucket the optional bucket
 * @param resource the mandat resource
 * @param option DPL_OPTION_HTTP_COMPAT use if possible the HTTP compat mode
 * @param condition the optional condition
 * 
 * @return DPL_SUCCESS
 * @return DPL_FAILURE
 * @return DPL_ENOENT resource does not exist
 */
dpl_task_t *
dpl_delete_async_prepare(dpl_ctx_t *ctx,
                         const char *bucket,
                         const char *resource,
                         const dpl_option_t *option,
                         dpl_ftype_t object_type,
                         const dpl_condition_t *condition)
{
  dpl_async_task_t *task = NULL;

  task = calloc(1, sizeof (*task));
  if (NULL == task)
    goto bad;

  task->ctx = ctx;
  task->type = DPL_TASK_DELETE;
  task->task.func = async_do;
  DUP_IF_NOT_NULL(task->u.delete, bucket);
  DUP_IF_NOT_NULL(task->u.delete, resource);
  if (NULL != option)
    task->u.delete.option = dpl_option_dup(option);
  task->u.delete.object_type = object_type;
  if (NULL != condition)
    task->u.delete.condition = dpl_condition_dup(condition);

  return (dpl_task_t *) task;

 bad:

  if (NULL != task)
    dpl_async_task_free(task);
  
  return NULL;
}

/** 
 * perform various flavors of server side copies
 * 
 * @param ctx the droplet context
 * @param src_bucket the optional source bucket
 * @param src_resource the mandat source resource
 * @param dst_bucket the optional destination bucket
 * @param dst_resource the optional destination resource (if dst equals src)
 * @param option unused
 * @param object_type unused
 * @param copy_directive DPL_COPY_DIRECTIVE_COPY server side copy
 * @param copy_directive DPL_COPY_DIRECTIVE_METADATA_REPLACE setattr
 * @param copy_directive DPL_COPY_DIRECTIVE_LINK hard link
 * @param copy_directive DPL_COPY_DIRECTIVE_SYMLINK reference
 * @param copy_directive DPL_COPY_DIRECTIVE_MOVE rename
 * @param copy_directive DPL_COPY_DIRECTIVE_MKDENT create a directory entry
 * @param metadata the optional user metadata
 * @param sysmd the optional system metadata
 * @param condition the optional condition
 *
 * @return DPL_SUCCESS
 * @return DPL_FAILURE
 */
dpl_task_t *
dpl_copy_async_prepare(dpl_ctx_t *ctx,
                       const char *src_bucket,
                       const char *src_resource,
                       const char *dst_bucket,
                       const char *dst_resource,
                       const dpl_option_t *option,
                       dpl_ftype_t object_type,
                       dpl_copy_directive_t copy_directive,
                       const dpl_dict_t *metadata,
                       const dpl_sysmd_t *sysmd,
                       const dpl_condition_t *condition)
{
  dpl_async_task_t *task = NULL;

  task = calloc(1, sizeof (*task));
  if (NULL == task)
    goto bad;

  task->ctx = ctx;
  task->type = DPL_TASK_COPY;
  task->task.func = async_do;
  DUP_IF_NOT_NULL(task->u.copy, src_bucket);
  DUP_IF_NOT_NULL(task->u.copy, src_resource);
  DUP_IF_NOT_NULL(task->u.copy, dst_bucket);
  DUP_IF_NOT_NULL(task->u.copy, dst_resource);
  if (NULL != option)
    task->u.copy.option = dpl_option_dup(option);
  task->u.copy.object_type = object_type;
  task->u.copy.copy_directive = copy_directive;
  if (NULL != metadata)
    task->u.copy.metadata = dpl_dict_dup(metadata);
  if (NULL != sysmd)
    task->u.copy.sysmd = dpl_sysmd_dup(sysmd);
  if (NULL != condition)
    task->u.copy.condition = dpl_condition_dup(condition);

  return (dpl_task_t *) task;

 bad:

  if (NULL != task)
    dpl_async_task_free(task);
  
  return NULL;
}

/** 
 * create or post data into a resource
 *
 * @note this function is expected to return a newly created object
 * 
 * @param ctx the droplet context
 * @param bucket can be NULL
 * @param option DPL_OPTION_HTTP_COMPAT use if possible the HTTP compat mode
 * @param object_type DPL_FTYPE_REG create a file
 * @param metadata the user metadata. optional
 * @param sysmd the system metadata. optional
 * @param buf the data buffer
 * @param query_params can be NULL
 * @param returned_sysmdp the returned system metadata passed through stack
 * 
 * @return DPL_SUCCESS
 * @return DPL_FAILURE
 */
dpl_task_t *
dpl_post_id_async_prepare(dpl_ctx_t *ctx,
                          const char *bucket,
                          const char *id,
                          const dpl_option_t *option,
                          dpl_ftype_t object_type,
                          const dpl_condition_t *condition,
                          const dpl_range_t *range,
                          const dpl_dict_t *metadata,
                          const dpl_sysmd_t *sysmd,
                          dpl_buf_t *buf,
                          const dpl_dict_t *query_params)
{
  dpl_async_task_t *task = NULL;

  task = calloc(1, sizeof (*task));
  if (NULL == task)
    goto bad;

  task->ctx = ctx;
  task->type = DPL_TASK_POST_ID;
  task->task.func = async_do;
  DUP_IF_NOT_NULL(task->u.post, bucket);
  if (NULL != option)
    task->u.post.option = dpl_option_dup(option);
  task->u.post.object_type = object_type;
  if (NULL != condition)
    task->u.post.condition = dpl_condition_dup(condition);
  if (NULL != range)
    task->u.post.range = dpl_range_dup(range);
  if (NULL != metadata)
    task->u.post.metadata = dpl_dict_dup(metadata);
  if (NULL != sysmd)
    task->u.post.sysmd = dpl_sysmd_dup(sysmd);
  if (NULL != buf)
    {
      task->u.post.buf = buf;
      dpl_buf_acquire(buf);
    }
  if (NULL != query_params)
    task->u.post.query_params = dpl_dict_dup(query_params);

  return (dpl_task_t *) task;

 bad:

  if (NULL != task)
    dpl_async_task_free(task);
  
  return NULL;
}

/**
 * put a resource
 *
 * @param ctx the droplet context
 * @param bucket optional
 * @param resource mandatory
 * @param option DPL_OPTION_HTTP_COMPAT use if possible the HTTP compat mode
 * @param object_type DPL_FTYPE_REG create a file
 * @param object_type DPL_FTYPE_DIR create a directory
 * @param condition the optional condition
 * @param range optional range
 * @param metadata the optional user metadata
 * @param sysmd the optional system metadata
 * @param buf the data buffer
 *
 * @return DPL_SUCCESS
 * @return DPL_FAILURE
 * @return DPL_EEXIST
 */
dpl_task_t *
dpl_put_id_async_prepare(dpl_ctx_t *ctx,
                         const char *bucket,
                         const char *resource,
                         const dpl_option_t *option,
                         dpl_ftype_t object_type,
                         const dpl_condition_t *condition,
                         const dpl_range_t *range,
                         const dpl_dict_t *metadata,
                         const dpl_sysmd_t *sysmd,
                         dpl_buf_t *buf)
{
  dpl_async_task_t *task = NULL;

  task = calloc(1, sizeof (*task));
  if (NULL == task)
    goto bad;

  task->ctx = ctx;
  task->type = DPL_TASK_PUT_ID;
  task->task.func = async_do;
  DUP_IF_NOT_NULL(task->u.put, bucket);
  DUP_IF_NOT_NULL(task->u.put, resource);
  if (NULL != option)
    task->u.put.option = dpl_option_dup(option);
  task->u.put.object_type = object_type;
  if (NULL != condition)
    task->u.put.condition = dpl_condition_dup(condition);
  if (NULL != range)
    task->u.put.range = dpl_range_dup(range);
  if (NULL != metadata)
    task->u.put.metadata = dpl_dict_dup(metadata);
  if (NULL != sysmd)
    task->u.put.sysmd = dpl_sysmd_dup(sysmd);
  if (NULL != buf)
    {
      task->u.put.buf = buf;
      dpl_buf_acquire(buf);
    }

  return (dpl_task_t *) task;

 bad:

  if (NULL != task)
    dpl_async_task_free(task);
  
  return NULL;
}

/** 
 * get a resource with range
 * 
 * @param ctx the droplet context
 * @param bucket the optional bucket
 * @param resource the mandat resource
 * @param option DPL_OPTION_HTTP_COMPAT use if possible the HTTP compat mode
 * @param object_type DPL_FTYPE_ANY get any type of resource
 * @param condition the optional condition
 * @param range the optional range
 * 
 * @return DPL_SUCCESS
 * @return DPL_FAILURE
 * @return DPL_ENOENT resource does not exist
 */
dpl_task_t *
dpl_get_id_async_prepare(dpl_ctx_t *ctx,
                         const char *bucket,
                         const char *resource,
                         const dpl_option_t *option,
                         dpl_ftype_t object_type,
                         const dpl_condition_t *condition,
                         const dpl_range_t *range)
{
  dpl_async_task_t *task = NULL;

  task = calloc(1, sizeof (*task));
  if (NULL == task)
    goto bad;

  task->ctx = ctx;
  task->type = DPL_TASK_GET_ID;
  task->task.func = async_do;
  DUP_IF_NOT_NULL(task->u.get, bucket);
  DUP_IF_NOT_NULL(task->u.get, resource);
  if (NULL != option)
    task->u.get.option = dpl_option_dup(option);
  task->u.get.object_type = object_type;
  if (NULL != condition)
    task->u.get.condition = dpl_condition_dup(condition);
  if (NULL != range)
    task->u.get.range = dpl_range_dup(range);

  return (dpl_task_t *) task;

 bad:

  if (NULL != task)
    dpl_async_task_free(task);
  
  return NULL;
}

/** 
 * get user and system metadata
 * 
 * @param ctx the droplet context
 * @param bucket the optional bucket
 * @param resource the mandat resource
 * @param option DPL_OPTION_HTTP_COMPAT use if possible the HTTP compat mode
 * @param object_type DPL_FTYPE_ANY get any type of resource
 * @param condition the optional condition
 * @param metadatap the returned user metadata client shall free
 * @param sysmdp the returned system metadata passed through stack
 * 
 * @return DPL_SUCCESS
 * @return DPL_FAILURE
 * @return DPL_ENOENT resource does not exist
 */
dpl_task_t *
dpl_head_id_async_prepare(dpl_ctx_t *ctx,
                          const char *bucket,
                          const char *resource,
                          const dpl_option_t *option,
                          dpl_ftype_t object_type,
                          const dpl_condition_t *condition)
{
  dpl_async_task_t *task = NULL;

  task = calloc(1, sizeof (*task));
  if (NULL == task)
    goto bad;

  task->ctx = ctx;
  task->type = DPL_TASK_HEAD_ID;
  task->task.func = async_do;
  DUP_IF_NOT_NULL(task->u.head, bucket);
  DUP_IF_NOT_NULL(task->u.head, resource);
  if (NULL != option)
    task->u.head.option = dpl_option_dup(option);
  task->u.head.object_type = object_type;
  if (NULL != condition)
    task->u.head.condition = dpl_condition_dup(condition);

  return (dpl_task_t *) task;

 bad:

  if (NULL != task)
    dpl_async_task_free(task);
  
  return NULL;
}

/** 
 * delete a resource
 * 
 * @param ctx the droplet context
 * @param bucket the optional bucket
 * @param resource the mandat resource
 * @param option DPL_OPTION_HTTP_COMPAT use if possible the HTTP compat mode
 * @param condition the optional condition
 * 
 * @return DPL_SUCCESS
 * @return DPL_FAILURE
 * @return DPL_ENOENT resource does not exist
 */
dpl_task_t *
dpl_delete_id_async_prepare(dpl_ctx_t *ctx,
                            const char *bucket,
                            const char *resource,
                            const dpl_option_t *option,
                            dpl_ftype_t object_type,
                            const dpl_condition_t *condition)
{
  dpl_async_task_t *task = NULL;
  
  task = calloc(1, sizeof (*task));
  if (NULL == task)
    goto bad;

  task->ctx = ctx;
  task->type = DPL_TASK_DELETE_ID;
  task->task.func = async_do;
  DUP_IF_NOT_NULL(task->u.delete, bucket);
  DUP_IF_NOT_NULL(task->u.delete, resource);
  if (NULL != option)
    task->u.delete.option = dpl_option_dup(option);
  task->u.delete.object_type = object_type;
  if (NULL != condition)
    task->u.delete.condition = dpl_condition_dup(condition);

  return (dpl_task_t *) task;

 bad:

  if (NULL != task)
    dpl_async_task_free(task);
  
  return NULL;
}

/** 
 * perform various flavors of server side copies
 * 
 * @param ctx the droplet context
 * @param src_bucket the optional source bucket
 * @param src_resource the mandat source resource
 * @param dst_bucket the optional destination bucket
 * @param dst_resource the optional destination resource (if dst equals src)
 * @param option unused
 * @param object_type unused
 * @param copy_directive DPL_COPY_DIRECTIVE_COPY server side copy
 * @param copy_directive DPL_COPY_DIRECTIVE_METADATA_REPLACE setattr
 * @param copy_directive DPL_COPY_DIRECTIVE_LINK hard link
 * @param copy_directive DPL_COPY_DIRECTIVE_SYMLINK reference
 * @param copy_directive DPL_COPY_DIRECTIVE_MOVE rename
 * @param copy_directive DPL_COPY_DIRECTIVE_MKDENT create a directory entry
 * @param metadata the optional user metadata
 * @param sysmd the optional system metadata
 * @param condition the optional condition
 *
 * @return DPL_SUCCESS
 * @return DPL_FAILURE
 */
dpl_task_t *
dpl_copy_id_async_prepare(dpl_ctx_t *ctx,
                          const char *src_bucket,
                          const char *src_resource,
                          const char *dst_bucket,
                          const char *dst_resource,
                          const dpl_option_t *option,
                          dpl_ftype_t object_type,
                          dpl_copy_directive_t copy_directive,
                          const dpl_dict_t *metadata,
                          const dpl_sysmd_t *sysmd,
                          const dpl_condition_t *condition)
{
  dpl_async_task_t *task = NULL;

  task = calloc(1, sizeof (*task));
  if (NULL == task)
    goto bad;

  task->ctx = ctx;
  task->type = DPL_TASK_COPY_ID;
  task->task.func = async_do;
  DUP_IF_NOT_NULL(task->u.copy, src_bucket);
  DUP_IF_NOT_NULL(task->u.copy, src_resource);
  DUP_IF_NOT_NULL(task->u.copy, dst_bucket);
  DUP_IF_NOT_NULL(task->u.copy, dst_resource);
  if (NULL != option)
    task->u.copy.option = dpl_option_dup(option);
  task->u.copy.object_type = object_type;
  task->u.copy.copy_directive = copy_directive;
  if (NULL != metadata)
    task->u.copy.metadata = dpl_dict_dup(metadata);
  if (NULL != sysmd)
    task->u.copy.sysmd = dpl_sysmd_dup(sysmd);
  if (NULL != condition)
    task->u.copy.condition = dpl_condition_dup(condition);

  return (dpl_task_t *) task;

 bad:

  if (NULL != task)
    dpl_async_task_free(task);
  
  return NULL;
}
