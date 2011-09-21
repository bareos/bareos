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

//#define DPRINTF(fmt,...) fprintf(stderr, fmt, ##__VA_ARGS__)
#define DPRINTF(fmt,...)

dpl_status_t 
dpl_list_all_my_buckets(dpl_ctx_t *ctx,
                        dpl_vec_t **vecp)
{
  int ret;

  DPL_TRACE(ctx, DPL_TRACE_API, "list_all_my_buckets");

  if (NULL == ctx->backend->list_all_my_buckets)
    {
      ret = DPL_ENOTSUPP;
      goto end;
    }
  
  ret = ctx->backend->list_all_my_buckets(ctx, vecp);

 end:
  
  DPL_TRACE(ctx, DPL_TRACE_API, "ret=%d", ret);
  
  return ret;
}

dpl_status_t 
dpl_list_bucket(dpl_ctx_t *ctx, 
                char *bucket,
                char *prefix,
                char *delimiter,
                dpl_vec_t **objectsp, 
                dpl_vec_t **common_prefixesp)
{
  int ret;

  DPL_TRACE(ctx, DPL_TRACE_API, "list_bucket bucket=%s prefix=%s delimiter=%s", bucket, prefix, delimiter);

  if (NULL == ctx->backend->list_bucket)
    {
      ret = DPL_ENOTSUPP;
      goto end;
    }
  
  ret = ctx->backend->list_bucket(ctx, bucket, prefix, delimiter, objectsp, common_prefixesp);

 end:
  
  DPL_TRACE(ctx, DPL_TRACE_API, "ret=%d", ret);
  
  return ret;
}

dpl_status_t
dpl_make_bucket(dpl_ctx_t *ctx,
                char *bucket, 
                dpl_location_constraint_t location_constraint,
                dpl_canned_acl_t canned_acl)
{
  int ret;

  DPL_TRACE(ctx, DPL_TRACE_API, "makebucket bucket=%s", bucket);

  if (NULL == ctx->backend->make_bucket)
    {
      ret = DPL_ENOTSUPP;
      goto end;
    }
  
  ret = ctx->backend->make_bucket(ctx, bucket, location_constraint, canned_acl);
  
 end:

  DPL_TRACE(ctx, DPL_TRACE_API, "ret=%d", ret);
  
  return ret;
}

dpl_status_t 
dpl_deletebucket(dpl_ctx_t *ctx,
                 char *bucket)
{
  int ret;
  
  DPL_TRACE(ctx, DPL_TRACE_API, "deletebucket bucket=%s", bucket);

  if (NULL == ctx->backend->delete_bucket)
    {
      ret = DPL_ENOTSUPP;
      goto end;
    }
  
  ret = ctx->backend->delete_bucket(ctx, bucket);
  
 end:

  DPL_TRACE(ctx, DPL_TRACE_API, "ret=%d", ret);
  
  return ret;
}

dpl_status_t
dpl_put(dpl_ctx_t *ctx,
        char *bucket,
        char *resource,
        char *subresource,
        dpl_object_type_t object_type,
        dpl_dict_t *metadata,
        dpl_canned_acl_t canned_acl,
        char *data_buf,
        unsigned int data_len)
{
  int ret;

  DPL_TRACE(ctx, DPL_TRACE_API, "put bucket=%s resource=%s subresource=%s", bucket, resource, subresource);

  if (NULL == ctx->backend->put)
    {
      ret = DPL_ENOTSUPP;
      goto end;
    }
  
  ret = ctx->backend->put(ctx, bucket, resource, subresource, object_type, metadata, canned_acl, data_buf, data_len);
  
 end:

  DPL_TRACE(ctx, DPL_TRACE_API, "ret=%d", ret);
  
  return ret;
}

dpl_status_t
dpl_put_buffered(dpl_ctx_t *ctx,
                 char *bucket,
                 char *resource,
                 char *subresource,
                 dpl_dict_t *metadata,
                 dpl_canned_acl_t canned_acl,
                 unsigned int data_len,
                 dpl_conn_t **connp)
{
  int ret;

  DPL_TRACE(ctx, DPL_TRACE_API, "put_buffered bucket=%s resource=%s subresource=%s", bucket, resource, subresource);

  if (NULL == ctx->backend->put_buffered)
    {
      ret = DPL_ENOTSUPP;
      goto end;
    }
  
  ret = ctx->backend->put_buffered(ctx, bucket, resource, subresource, DPL_OBJECT_TYPE_OBJECT, metadata, canned_acl, data_len, connp);
  
 end:

  DPL_TRACE(ctx, DPL_TRACE_API, "ret=%d", ret);
  
  return ret;
}

dpl_status_t
dpl_get(dpl_ctx_t *ctx,
        char *bucket,
        char *resource,
        char *subresource,
        dpl_condition_t *condition,
        char **data_bufp,
        unsigned int *data_lenp,
        dpl_dict_t **metadatap)
{
  int ret;

  DPL_TRACE(ctx, DPL_TRACE_API, "get bucket=%s resource=%s subresource=%s", bucket, resource, subresource);

  if (NULL == ctx->backend->get)
    {
      ret = DPL_ENOTSUPP;
      goto end;
    }
  
  ret = ctx->backend->get(ctx, bucket, resource, subresource, condition, data_bufp, data_lenp, metadatap);
  
 end:

  DPL_TRACE(ctx, DPL_TRACE_API, "ret=%d", ret);
  
  return ret;
}

dpl_status_t
dpl_get_range(dpl_ctx_t *ctx,
              char *bucket,
              char *resource,
              char *subresource,
              dpl_condition_t *condition,
              int start,
              int end,
              char **data_bufp,
              unsigned int *data_lenp,
              dpl_dict_t **metadatap)
{
  int ret;

  DPL_TRACE(ctx, DPL_TRACE_API, "get_range bucket=%s resource=%s subresource=%s", bucket, resource, subresource);

  if (NULL == ctx->backend->get_range)
    {
      ret = DPL_ENOTSUPP;
      goto end;
    }
  
  ret = ctx->backend->get_range(ctx, bucket, resource, subresource, condition, start, end, data_bufp, data_lenp, metadatap);
  
 end:

  DPL_TRACE(ctx, DPL_TRACE_API, "ret=%d", ret);
  
  return ret;
}

dpl_status_t 
dpl_get_buffered(dpl_ctx_t *ctx,
                 char *bucket,
                 char *resource,
                 char *subresource, 
                 dpl_condition_t *condition,
                 dpl_header_func_t header_func, 
                 dpl_buffer_func_t buffer_func,
                 void *cb_arg)
{
  int ret;

  DPL_TRACE(ctx, DPL_TRACE_API, "get_buffered bucket=%s resource=%s subresource=%s", bucket, resource, subresource);

  if (NULL == ctx->backend->get_buffered)
    {
      ret = DPL_ENOTSUPP;
      goto end;
    }
  
  ret = ctx->backend->get_buffered(ctx, bucket, resource, subresource, condition, header_func, buffer_func, cb_arg);
  
 end:

  DPL_TRACE(ctx, DPL_TRACE_API, "ret=%d", ret);
  
  return ret;
}

dpl_status_t
dpl_head(dpl_ctx_t *ctx,
         char *bucket,
         char *resource,
         char *subresource,
         dpl_condition_t *condition,
         dpl_dict_t **metadatap)
{
  int ret;

  DPL_TRACE(ctx, DPL_TRACE_API, "head bucket=%s resource=%s subresource=%s", bucket, resource, subresource);

  if (NULL == ctx->backend->head)
    {
      ret = DPL_ENOTSUPP;
      goto end;
    }
  
  ret = ctx->backend->head(ctx, bucket, resource, subresource, condition, metadatap);
  
 end:

  DPL_TRACE(ctx, DPL_TRACE_API, "ret=%d", ret);
  
  return ret;
}

dpl_status_t
dpl_head_all(dpl_ctx_t *ctx,
             char *bucket,
             char *resource,
             char *subresource,
             dpl_condition_t *condition,
             dpl_dict_t **metadatap)
{
  int ret;

  DPL_TRACE(ctx, DPL_TRACE_API, "head_all bucket=%s resource=%s subresource=%s", bucket, resource, subresource);

  if (NULL == ctx->backend->head_all)
    {
      ret = DPL_ENOTSUPP;
      goto end;
    }
  
  ret = ctx->backend->head_all(ctx, bucket, resource, subresource, condition, metadatap);
  
 end:

  DPL_TRACE(ctx, DPL_TRACE_API, "ret=%d", ret);
  
  return ret;
}

dpl_status_t
dpl_get_metadata_from_headers(dpl_ctx_t *ctx,
                              dpl_dict_t *headers,
                              dpl_dict_t *metadata)
{
  int ret;

  DPL_TRACE(ctx, DPL_TRACE_API, "get_metadata_from_headers");

  if (NULL == ctx->backend->get_metadata_from_headers)
    {
      ret = DPL_ENOTSUPP;
      goto end;
    }
  
  ret = ctx->backend->get_metadata_from_headers(headers, metadata);
  
 end:

  DPL_TRACE(ctx, DPL_TRACE_API, "ret=%d", ret);
  
  return ret;
}

dpl_status_t
dpl_delete(dpl_ctx_t *ctx,
           char *bucket,
           char *resource,
           char *subresource)
{
  int ret;

  DPL_TRACE(ctx, DPL_TRACE_API, "delete bucket=%s resource=%s subresource=%s", bucket, resource, subresource);

  if (NULL == ctx->backend->delete)
    {
      ret = DPL_ENOTSUPP;
      goto end;
    }
  
  ret = ctx->backend->delete(ctx, bucket, resource, subresource);
  
 end:

  DPL_TRACE(ctx, DPL_TRACE_API, "ret=%d", ret);
  
  return ret;
}

dpl_status_t
dpl_genurl(dpl_ctx_t *ctx,
           char *bucket,
           char *resource,
           char *subresource,
           time_t expires,
           char *buf,
           unsigned int len,
           unsigned int *lenp)
{
  int ret;

  DPL_TRACE(ctx, DPL_TRACE_API, "genurl bucket=%s resource=%s subresource=%s", bucket, resource, subresource);

  if (NULL == ctx->backend->genurl)
    {
      ret = DPL_ENOTSUPP;
      goto end;
    }
  
  ret = ctx->backend->genurl(ctx, bucket, resource, subresource, expires, buf, len, lenp);
  
 end:

  DPL_TRACE(ctx, DPL_TRACE_API, "ret=%d", ret);
  
  return ret;
}

dpl_status_t
dpl_copy(dpl_ctx_t *ctx,
         char *src_bucket,
         char *src_resource,
         char *src_subresource,
         char *dst_bucket,
         char *dst_resource,
         char *dst_subresource,
         dpl_object_type_t object_type,
         dpl_metadata_directive_t metadata_directive,
         dpl_dict_t *metadata,
         dpl_canned_acl_t canned_acl,
         dpl_condition_t *condition)
{
  int ret;

  DPL_TRACE(ctx, DPL_TRACE_API, "copy src_bucket=%s src_resource=%s src_subresource=%s dst_bucket=%s dst_resource=%s dst_subresource=%s", src_bucket, src_resource, src_subresource, dst_bucket, dst_resource, dst_subresource);

  if (NULL == ctx->backend->copy)
    {
      ret = DPL_ENOTSUPP;
      goto end;
    }
  
  ret = ctx->backend->copy(ctx, src_bucket, src_resource, src_subresource, dst_bucket, dst_resource, dst_subresource, object_type, metadata_directive, metadata, canned_acl, condition);
  
 end:

  DPL_TRACE(ctx, DPL_TRACE_API, "ret=%d", ret);
  
  return ret;
}
