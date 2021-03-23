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
#ifndef BAREOS_DROPLET_LIBDROPLET_INCLUDE_DROPLET_ASYNC_H_
#define BAREOS_DROPLET_LIBDROPLET_INCLUDE_DROPLET_ASYNC_H_

#include <droplet/task.h>

typedef struct
{
  char *ptr;
  u_int size;
  int refcnt;
} dpl_buf_t;

#define dpl_buf_ptr(Buf) ((Buf)->ptr)
#define dpl_buf_size(Buf) ((Buf)->size)

typedef enum
  {
    DPL_TASK_LIST_ALL_MY_BUCKETS,
    DPL_TASK_LIST_BUCKET,
    DPL_TASK_MAKE_BUCKET,
    DPL_TASK_DELETE_BUCKET,
    DPL_TASK_POST,
    DPL_TASK_PUT,
    DPL_TASK_GET,
    DPL_TASK_HEAD,
    DPL_TASK_DELETE,
    DPL_TASK_COPY,
    DPL_TASK_POST_ID,
    DPL_TASK_PUT_ID,
    DPL_TASK_GET_ID,
    DPL_TASK_HEAD_ID,
    DPL_TASK_DELETE_ID,
    DPL_TASK_COPY_ID,
  } dpl_async_task_type_t;

typedef struct
{
  dpl_task_t task; /*!< mandatory */
  dpl_ctx_t *ctx;
  dpl_async_task_type_t type;
  dpl_task_func_t cb_func;
  void *cb_arg;
  dpl_status_t ret;
  union
  {
    struct
    {
      /* input */
      /* output */
      dpl_vec_t *buckets;
    } list_all_my_buckets;
    struct
    {
      /* input */
      char *bucket;
      char *prefix;
      char *delimiter;
      size_t max_keys;
      /* output */
      dpl_vec_t *objects;
      dpl_vec_t *common_prefixes;
    } list_bucket;
    struct
    {
      /* input */
      char *bucket;
      dpl_location_constraint_t location_constraint;
      dpl_canned_acl_t canned_acl;
      /* output */
    } make_bucket;
    struct
    {
      /* input */
      char *bucket;
      /* output */
    } delete_bucket;
    struct
    {
      /* input */
      char *bucket;
      char *resource;
      dpl_option_t *option;
      dpl_ftype_t object_type;
      dpl_condition_t *condition;
      dpl_range_t *range;
      dpl_dict_t *metadata;
      dpl_sysmd_t *sysmd;
      dpl_buf_t *buf;
      dpl_dict_t *query_params;
      /* output */
      dpl_sysmd_t sysmd_returned;
    } post;
    struct
    {
      /* input */
      char *bucket;
      char *resource;
      dpl_option_t *option;
      dpl_ftype_t object_type;
      dpl_condition_t *condition;
      dpl_range_t *range;
      dpl_dict_t *metadata;
      dpl_sysmd_t *sysmd;
      dpl_buf_t *buf;
      /* output */
    } put;
    struct
    {
      /* input */
      char *bucket;
      char *resource;
      dpl_option_t *option;
      dpl_ftype_t object_type;
      dpl_condition_t *condition;
      dpl_range_t *range;
      /* output */
      dpl_buf_t *buf;
      dpl_dict_t *metadata;
      dpl_sysmd_t sysmd;
    } get;
    struct
    {
      /* input */
      char *bucket;
      char *resource;
      dpl_option_t *option;
      dpl_ftype_t object_type;
      dpl_condition_t *condition;
      /* output */
      dpl_dict_t *metadata;
      dpl_sysmd_t sysmd;
    } head;
    struct
    {
      /* input */
      char *bucket;
      char *resource;
      dpl_option_t *option;
      dpl_ftype_t object_type;
      dpl_condition_t *condition;
      /* output */
    } delete;
    struct
    {
      /* input */
      char *src_bucket;
      char *src_resource;
      char *dst_bucket;
      char *dst_resource;
      dpl_option_t *option;
      dpl_ftype_t object_type;
      dpl_copy_directive_t copy_directive;
      dpl_dict_t *metadata;
      dpl_sysmd_t *sysmd;
      dpl_condition_t *condition;
      /* output */
    } copy;
  } u;
} dpl_async_task_t;

dpl_buf_t *dpl_buf_new();
void dpl_buf_acquire(dpl_buf_t *buf);
void dpl_buf_release(dpl_buf_t *buf);
void dpl_async_task_free(dpl_async_task_t *task);
dpl_task_t *dpl_list_all_my_buckets_async_prepare(dpl_ctx_t *ctx);
dpl_task_t *dpl_list_bucket_async_prepare(dpl_ctx_t *ctx, const char *bucket, const char *prefix, const char *delimiter);
dpl_task_t *dpl_make_bucket_async_prepare(dpl_ctx_t *ctx, const char *bucket, dpl_location_constraint_t location_constraint, dpl_canned_acl_t canned_acl);
dpl_task_t *dpl_delete_bucket_prepare(dpl_ctx_t *ctx, const char *bucket);
dpl_task_t *dpl_post_async_prepare(dpl_ctx_t *ctx, const char *bucket, const char *resource, const dpl_option_t *option, dpl_ftype_t object_type, const dpl_condition_t *condition, const dpl_range_t *range, const dpl_dict_t *metadata, const dpl_sysmd_t *sysmd, dpl_buf_t *buf, const dpl_dict_t *query_params);
dpl_task_t *dpl_put_async_prepare(dpl_ctx_t *ctx, const char *bucket, const char *resource, const dpl_option_t *option, dpl_ftype_t object_type, const dpl_condition_t *condition, const dpl_range_t *range, const dpl_dict_t *metadata, const dpl_sysmd_t *sysmd, dpl_buf_t *buf);
dpl_task_t *dpl_get_async_prepare(dpl_ctx_t *ctx, const char *bucket, const char *resource, const dpl_option_t *option, dpl_ftype_t object_type, const dpl_condition_t *condition, const dpl_range_t *range);
dpl_task_t *dpl_head_async_prepare(dpl_ctx_t *ctx, const char *bucket, const char *resource, const dpl_option_t *option, dpl_ftype_t object_type, const dpl_condition_t *condition);
dpl_task_t *dpl_delete_async_prepare(dpl_ctx_t *ctx, const char *bucket, const char *resource, const dpl_option_t *option, dpl_ftype_t object_type, const dpl_condition_t *condition);
dpl_task_t *dpl_copy_async_prepare(dpl_ctx_t *ctx, const char *src_bucket, const char *src_resource, const char *dst_bucket, const char *dst_resource, const dpl_option_t *option, dpl_ftype_t object_type, dpl_copy_directive_t copy_directive, const dpl_dict_t *metadata, const dpl_sysmd_t *sysmd, const dpl_condition_t *condition);
dpl_task_t *dpl_post_id_async_prepare(dpl_ctx_t *ctx, const char *bucket, const char *id, const dpl_option_t *option, dpl_ftype_t object_type, const dpl_condition_t *condition, const dpl_range_t *range, const dpl_dict_t *metadata, const dpl_sysmd_t *sysmd, dpl_buf_t *buf, const dpl_dict_t *query_params);
dpl_task_t *dpl_put_id_async_prepare(dpl_ctx_t *ctx, const char *bucket, const char *resource, const dpl_option_t *option, dpl_ftype_t object_type, const dpl_condition_t *condition, const dpl_range_t *range, const dpl_dict_t *metadata, const dpl_sysmd_t *sysmd, dpl_buf_t *buf);
dpl_task_t *dpl_get_id_async_prepare(dpl_ctx_t *ctx, const char *bucket, const char *resource, const dpl_option_t *option, dpl_ftype_t object_type, const dpl_condition_t *condition, const dpl_range_t *range);
dpl_task_t *dpl_head_id_async_prepare(dpl_ctx_t *ctx, const char *bucket, const char *resource, const dpl_option_t *option, dpl_ftype_t object_type, const dpl_condition_t *condition);
dpl_task_t *dpl_delete_id_async_prepare(dpl_ctx_t *ctx, const char *bucket, const char *resource, const dpl_option_t *option, dpl_ftype_t object_type, const dpl_condition_t *condition);
dpl_task_t *dpl_copy_id_async_prepare(dpl_ctx_t *ctx, const char *src_bucket, const char *src_resource, const char *dst_bucket, const char *dst_resource, const dpl_option_t *option, dpl_ftype_t object_type, dpl_copy_directive_t copy_directive, const dpl_dict_t *metadata, const dpl_sysmd_t *sysmd, const dpl_condition_t *condition);
#endif  // BAREOS_DROPLET_LIBDROPLET_INCLUDE_DROPLET_ASYNC_H_
