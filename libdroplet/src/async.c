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
dpl_buf_new(size_t size)
{
  dpl_buf_t *buf = NULL;

  buf = malloc(sizeof (dpl_buf_t) + size);
  if (NULL == buf)
    return NULL;
  
  buf->size = size;
  buf->refcnt = 0;

  return buf;
}

size_t dpl_buf_size(dpl_buf_t *buf)
{
  if (buf)
    return buf->size;
  return 0;
}

char *
dpl_buf_ptr(dpl_buf_t *buf)
{
  if (buf)
    return ((char *) buf) + sizeof (dpl_buf_t);
  return NULL;
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
    free(buf);
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
      /* input */
      FREE_IF_NOT_NULL(task->u.post.bucket);
      FREE_IF_NOT_NULL(task->u.post.resource);
      FREE_IF_NOT_NULL(task->u.post.subresource);
      if (NULL != task->u.post.option)
        dpl_option_free(task->u.post.option);
      if (NULL != task->u.post.metadata)
        dpl_dict_free(task->u.post.metadata);
      if (NULL != task->u.post.sysmd)
        dpl_sysmd_free(task->u.post.sysmd);
      if (NULL != task->u.post.buf)
        dpl_buf_release(task->u.post.buf);
      if (NULL != task->u.post.query_params)
        dpl_dict_free(task->u.post.query_params);
      /* output */
       FREE_IF_NOT_NULL(task->u.post.location);
      break ;
    case DPL_TASK_PUT:
      /* input */
      FREE_IF_NOT_NULL(task->u.put.bucket);
      FREE_IF_NOT_NULL(task->u.put.resource);
      FREE_IF_NOT_NULL(task->u.put.subresource);
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
                           task->u.post.subresource,
                           task->u.post.option,
                           task->u.post.object_type,
                           task->u.post.metadata,
                           task->u.post.sysmd,
                           dpl_buf_ptr(task->u.post.buf),
                           dpl_buf_size(task->u.post.buf),
                           task->u.post.query_params,
                           &task->u.post.sysmd_returned,
                           &task->u.post.location);
      break ;
    case DPL_TASK_PUT:
      task->ret = dpl_put(task->ctx, 
                          task->u.put.bucket,
                          task->u.put.resource,
                          task->u.put.subresource,
                          task->u.put.option,
                          task->u.put.object_type,
                          task->u.put.condition,
                          task->u.put.range,  
                          task->u.put.metadata,
                          task->u.put.sysmd,
                          dpl_buf_ptr(task->u.put.buf),
                          dpl_buf_size(task->u.put.buf));
      break ;
    }
  if (NULL != task->cb_func)
    task->cb_func(task);
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
 * @param resource the resource
 * @param subresource can be NULL
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
 * @param subresource can be NULL
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
                       const char *subresource,
                       const dpl_option_t *option,
                       dpl_ftype_t object_type,
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
  DUP_IF_NOT_NULL(task->u.post, subresource);
  if (NULL != option)
    task->u.post.option = dpl_option_dup(option);
  task->u.post.object_type = object_type;
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
 * @param subresource can be NULL
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
                      const char *subresource,
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
  DUP_IF_NOT_NULL(task->u.put, subresource);
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

#if 0
/** 
 * put a resource with bufferization
 * 
 * @param ctx the droplet context
 * @param bucket the optional bucket
 * @param resource mandatory resource
 * @param subresource optional
 * @param option DPL_OPTION_HTTP_COMPAT use if possible the HTTP compat mode
 * @param object_type DPL_FTYPE_REG create a file
 * @param condition optional condition
 * @param range optional range
 * @param metadata optional user metadata
 * @param sysmd optional system metadata
 * @param data_len advertise the length
 * @param connp the connection object
 * 
 * @return DPL_SUCCESS
 * @return DPL_FAILURE
 */
dpl_status_t
dpl_put_buffered(dpl_ctx_t *ctx,
                 const char *bucket,
                 const char *resource,
                 const char *subresource,
                 const dpl_option_t *option,
                 dpl_ftype_t object_type,
                 const dpl_condition_t *condition,
                 const dpl_range_t *range,
                 const dpl_dict_t *metadata,
                 const dpl_sysmd_t *sysmd,
                 unsigned int data_len,
                 dpl_conn_t **connp)
{
  int ret;

  DPL_TRACE(ctx, DPL_TRACE_REST, "put_buffered bucket=%s resource=%s subresource=%s", bucket, resource, subresource);

  if (NULL == ctx->backend->put_buffered)
    {
      ret = DPL_ENOTSUPP;
      goto end;
    }

  ret = ctx->backend->put_buffered(ctx, bucket, resource, subresource, option, object_type, condition, range, metadata, sysmd, data_len, connp, NULL);
  
 end:

  DPL_TRACE(ctx, DPL_TRACE_REST, "ret=%d", ret);
  
  return ret;
}

/** 
 * get a resource with range
 * 
 * @param ctx the droplet context
 * @param bucket the optional bucket
 * @param resource the mandat resource
 * @param subresource the optional subresource
 * @param option DPL_OPTION_HTTP_COMPAT use if possible the HTTP compat mode
 * @param object_type DPL_FTYPE_ANY get any type of resource
 * @param condition the optional condition
 * @param range the optional range
 * @param data_bufp the returned data buffer client shall free
 * @param data_lenp the returned data length
 * @param metadatap the returned user metadata client shall free
 * @param sysmdp the returned system metadata passed through stack
 * 
 * @return DPL_SUCCESS
 * @return DPL_FAILURE
 * @return DPL_ENOENT resource does not exist
 */
dpl_status_t
dpl_get(dpl_ctx_t *ctx,
        const char *bucket,
        const char *resource,
        const char *subresource,
        const dpl_option_t *option,
        dpl_ftype_t object_type,
        const dpl_condition_t *condition,
        const dpl_range_t *range, 
        char **data_bufp,
        unsigned int *data_lenp,
        dpl_dict_t **metadatap,
        dpl_sysmd_t *sysmdp)
{
  int ret;

  DPL_TRACE(ctx, DPL_TRACE_REST, "get bucket=%s resource=%s subresource=%s", bucket, resource, subresource);

  if (NULL == ctx->backend->get)
    {
      ret = DPL_ENOTSUPP;
      goto end;
    }
  
  ret = ctx->backend->get(ctx, bucket, resource, subresource, option, object_type, condition, range, data_bufp, data_lenp, metadatap, sysmdp, NULL);
  
 end:

  DPL_TRACE(ctx, DPL_TRACE_REST, "ret=%d", ret);
  
  return ret;
}

/** 
 * get range with bufferization
 * 
 * @param ctx the droplet context
 * @param bucket the optional bucket
 * @param resource the mandat resource
 * @param subresource the optional subresource
 * @param option DPL_OPTION_HTTP_COMPAT use if possible the HTTP compat mode
 * @param object_type DPL_FTYPE_ANY get any type of resource
 * @param condition the optional condition
 * @param range the optional range
 * @param metadatum_func each time a metadata is discovered into the stream it is called back
 * @param metadatap the returned user metadata client shall free
 * @param sysmdp the returned system metadata passed through stack
 * @param buffer_func the function called each time a buffer is discovered
 * @param cb_arg the callback argument
 * 
 * @return DPL_SUCCESS
 * @return DPL_FAILURE
 */
dpl_status_t 
dpl_get_buffered(dpl_ctx_t *ctx,
                 const char *bucket,
                 const char *resource,
                 const char *subresource, 
                 const dpl_option_t *option,
                 dpl_ftype_t object_type,
                 const dpl_condition_t *condition,
                 const dpl_range_t *range,
                 dpl_metadatum_func_t metadatum_func,
                 dpl_dict_t **metadatap,
                 dpl_sysmd_t *sysmdp, 
                 dpl_buffer_func_t buffer_func,
                 void *cb_arg)
{
  int ret;

  DPL_TRACE(ctx, DPL_TRACE_ID, "get_buffered bucket=%s resource=%s subresource=%s", bucket, resource, subresource);

  if (NULL == ctx->backend->get_buffered)
    {
      ret = DPL_ENOTSUPP;
      goto end;
    }
  
  ret = ctx->backend->get_buffered(ctx, bucket, resource, subresource, option, object_type, condition, range, metadatum_func, metadatap, sysmdp, buffer_func, cb_arg, NULL);
  
 end:

  DPL_TRACE(ctx, DPL_TRACE_ID, "ret=%d", ret);
  
  return ret;
}

/** 
 * get user and system metadata
 * 
 * @param ctx the droplet context
 * @param bucket the optional bucket
 * @param resource the mandat resource
 * @param subresource the optional subresource
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
dpl_status_t
dpl_head(dpl_ctx_t *ctx,
         const char *bucket,
         const char *resource,
         const char *subresource,
         const dpl_option_t *option,
         const dpl_condition_t *condition,
         dpl_dict_t **metadatap,
         dpl_sysmd_t *sysmdp)
{
  int ret;

  DPL_TRACE(ctx, DPL_TRACE_REST, "head bucket=%s resource=%s subresource=%s", bucket, resource, subresource);

  if (NULL == ctx->backend->head)
    {
      ret = DPL_ENOTSUPP;
      goto end;
    }
  
  ret = ctx->backend->head(ctx, bucket, resource, subresource, option, DPL_FTYPE_UNDEF, condition, metadatap, sysmdp, NULL);
  
 end:

  DPL_TRACE(ctx, DPL_TRACE_REST, "ret=%d", ret);
  
  return ret;
}

/** 
 * get raw metadata
 * 
 * @param ctx the droplet context
 * @param bucket the optional bucket
 * @param resource the mandat resource
 * @param subresource the optional subresource
 * @param option DPL_OPTION_HTTP_COMPAT use if possible the HTTP compat mode
 * @param condition the optional condition
 * @param metadatap the returned metadata client shall free
 * 
 * @return DPL_SUCCESS
 * @return DPL_FAILURE
 * @return DPL_ENOENT
 */
dpl_status_t
dpl_head_raw(dpl_ctx_t *ctx,
             const char *bucket,
             const char *resource,
             const char *subresource,
             const dpl_option_t *option,
             const dpl_condition_t *condition,
             dpl_dict_t **metadatap)
{
  int ret;

  DPL_TRACE(ctx, DPL_TRACE_REST, "head_raw bucket=%s resource=%s subresource=%s", bucket, resource, subresource);

  if (NULL == ctx->backend->head_raw)
    {
      ret = DPL_ENOTSUPP;
      goto end;
    }
  
  ret = ctx->backend->head_raw(ctx, bucket, resource, subresource, option, DPL_FTYPE_UNDEF, condition, metadatap, NULL);
  
 end:

  DPL_TRACE(ctx, DPL_TRACE_REST, "ret=%d", ret);
  
  return ret;
}

/** 
 * delete a resource
 * 
 * @param ctx the droplet context
 * @param bucket the optional bucket
 * @param resource the mandat resource
 * @param subresource the optional subresource
 * @param option DPL_OPTION_HTTP_COMPAT use if possible the HTTP compat mode
 * @param condition the optional condition
 * 
 * @return DPL_SUCCESS
 * @return DPL_FAILURE
 * @return DPL_ENOENT resource does not exist
 */
dpl_status_t
dpl_delete(dpl_ctx_t *ctx,
           const char *bucket,
           const char *resource,
           const char *subresource,
           const dpl_option_t *option,
           const dpl_condition_t *condition)
{
  int ret;

  DPL_TRACE(ctx, DPL_TRACE_REST, "delete bucket=%s resource=%s subresource=%s", bucket, resource, subresource);

  if (NULL == ctx->backend->deletef)
    {
      ret = DPL_ENOTSUPP;
      goto end;
    }
  
  ret = ctx->backend->deletef(ctx, bucket, resource, subresource, option, DPL_FTYPE_UNDEF, NULL);
  
 end:

  DPL_TRACE(ctx, DPL_TRACE_REST, "ret=%d", ret);
  
  return ret;
}

/** 
 * generate a valid URL for sharing object
 * 
 * @param ctx the droplet context
 * @param bucket the optional bucket
 * @param resource the mandat resource
 * @param subresource the optional subresource
 * @param option unused
 * @param expires expire time of URL
 * @param buf URL is created in this buffer
 * @param len buffer length
 * @param lenp real buffer returned
 * 
 * @return DPL_SUCCESS
 * @return DPL_FAILURE
 */
dpl_status_t
dpl_genurl(dpl_ctx_t *ctx,
           const char *bucket,
           const char *resource,
           const char *subresource,
           const dpl_option_t *option,
           time_t expires,
           char *buf,
           unsigned int len,
           unsigned int *lenp)
{
  int ret;

  DPL_TRACE(ctx, DPL_TRACE_REST, "genurl bucket=%s resource=%s subresource=%s", bucket, resource, subresource);

  if (NULL == ctx->backend->genurl)
    {
      ret = DPL_ENOTSUPP;
      goto end;
    }
  
  ret = ctx->backend->genurl(ctx, bucket, resource, subresource, option, expires, buf, len, lenp, NULL);
  
 end:

  DPL_TRACE(ctx, DPL_TRACE_REST, "ret=%d", ret);
  
  return ret;
}

/** 
 * perform various flavors of server side copies
 * 
 * @param ctx the droplet context
 * @param src_bucket the optional source bucket
 * @param src_resource the mandat source resource
 * @param src_subresource the optional src subresource
 * @param dst_bucket the optional destination bucket
 * @param dst_resource the optional destination resource (if dst equals src)
 * @param dst_subresource the optional dest subresource
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
dpl_status_t
dpl_copy(dpl_ctx_t *ctx,
         const char *src_bucket,
         const char *src_resource,
         const char *src_subresource,
         const char *dst_bucket,
         const char *dst_resource,
         const char *dst_subresource,
         const dpl_option_t *option,
         dpl_ftype_t object_type,
         dpl_copy_directive_t copy_directive,
         const dpl_dict_t *metadata,
         const dpl_sysmd_t *sysmd,
         const dpl_condition_t *condition)
{
  int ret;

  DPL_TRACE(ctx, DPL_TRACE_REST, "copy src_bucket=%s src_resource=%s src_subresource=%s dst_bucket=%s dst_resource=%s dst_subresource=%s", src_bucket, src_resource, src_subresource, dst_bucket, dst_resource, dst_subresource);

  if (NULL == ctx->backend->copy)
    {
      ret = DPL_ENOTSUPP;
      goto end;
    }
  
  ret = ctx->backend->copy(ctx, src_bucket, src_resource, src_subresource, dst_bucket, dst_resource, dst_subresource, option, object_type, copy_directive, metadata, sysmd, condition, NULL);
  
 end:

  DPL_TRACE(ctx, DPL_TRACE_REST, "ret=%d", ret);
  
  return ret;
}
#endif
