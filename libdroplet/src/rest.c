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

/**
 * list all buckets
 *
 * @param ctx
 * @param vecp
 *
 * @return
 */
dpl_status_t 
dpl_list_all_my_buckets(dpl_ctx_t *ctx,
                        dpl_vec_t **vecp)
{
  int ret;

  DPL_TRACE(ctx, DPL_TRACE_REST, "list_all_my_buckets");

  if (NULL == ctx->backend->list_all_my_buckets)
    {
      ret = DPL_ENOTSUPP;
      goto end;
    }
  
  ret = ctx->backend->list_all_my_buckets(ctx, vecp);

 end:
  
  DPL_TRACE(ctx, DPL_TRACE_REST, "ret=%d", ret);
  
  return ret;
}

/**
 * list bucket
 *
 * @param ctx
 * @param bucket
 * @param prefix can be NULL
 * @param delimiter can be NULL
 * @param objectsp
 * @param prefixesp
 *
 * @return
 */
dpl_status_t 
dpl_list_bucket(dpl_ctx_t *ctx, 
                const char *bucket,
                const char *prefix,
                const char *delimiter,
                dpl_vec_t **objectsp, 
                dpl_vec_t **common_prefixesp)
{
  int ret;

  DPL_TRACE(ctx, DPL_TRACE_REST, "list_bucket bucket=%s prefix=%s delimiter=%s", bucket, prefix, delimiter);

  if (NULL == ctx->backend->list_bucket)
    {
      ret = DPL_ENOTSUPP;
      goto end;
    }
  
  ret = ctx->backend->list_bucket(ctx, bucket, prefix, delimiter, objectsp, common_prefixesp);

 end:
  
  DPL_TRACE(ctx, DPL_TRACE_REST, "ret=%d", ret);
  
  return ret;
}

/**
 * make a bucket
 *
 * @param ctx
 * @param bucket
 * @param location_constraint
 * @param canned_acl
 *
 * @return
 */
dpl_status_t
dpl_make_bucket(dpl_ctx_t *ctx,
                const char *bucket, 
                dpl_location_constraint_t location_constraint,
                dpl_canned_acl_t canned_acl)
{
  int ret;
  dpl_sysmd_t sysmd;

  DPL_TRACE(ctx, DPL_TRACE_REST, "makebucket bucket=%s", bucket);

  if (NULL == ctx->backend->make_bucket)
    {
      ret = DPL_ENOTSUPP;
      goto end;
    }
  
  memset(&sysmd, 0, sizeof (sysmd));
  sysmd.mask = DPL_SYSMD_MASK_CANNED_ACL|DPL_SYSMD_MASK_LOCATION_CONSTRAINT;
  sysmd.canned_acl = canned_acl;
  sysmd.location_constraint = location_constraint;

  ret = ctx->backend->make_bucket(ctx, bucket, &sysmd);
  
 end:

  DPL_TRACE(ctx, DPL_TRACE_REST, "ret=%d", ret);
  
  return ret;
}

/**
 * delete a resource
 *
 * @param ctx
 * @param bucket
 * @param resource
 * @param subresource can be NULL
 *
 * @return
 */
dpl_status_t 
dpl_deletebucket(dpl_ctx_t *ctx,
                 const char *bucket)
{
  int ret;
  
  DPL_TRACE(ctx, DPL_TRACE_REST, "deletebucket bucket=%s", bucket);

  if (NULL == ctx->backend->delete_bucket)
    {
      ret = DPL_ENOTSUPP;
      goto end;
    }
  
  ret = ctx->backend->delete_bucket(ctx, bucket);
  
 end:

  DPL_TRACE(ctx, DPL_TRACE_REST, "ret=%d", ret);
  
  return ret;
}

/** 
 * create or post data into a resource
 *
 * @note this function is expected to return a newly created object
 * 
 * @param ctx 
 * @param bucket 
 * @param resource can be NULL
 * @param subresource can be NULL
 * @param object_type 
 * @param metadata 
 * @param canned_acl 
 * @param data_buf 
 * @param data_len 
 * @param query_params can be NULL
 * @param resource_idp ID of newly created object. caller must free it
 * 
 * @return 
 */
dpl_status_t
dpl_post(dpl_ctx_t *ctx,
         const char *bucket,
         const char *resource,
         const char *subresource,
         dpl_ftype_t object_type,
         dpl_dict_t *metadata,
         dpl_sysmd_t *sysmd,
         char *data_buf,
         unsigned int data_len,
         dpl_dict_t *query_params,
         char **resource_idp)
{
  int ret;

  DPL_TRACE(ctx, DPL_TRACE_REST, "put bucket=%s resource=%s subresource=%s", bucket, resource, subresource);

  if (NULL == ctx->backend->post)
    {
      ret = DPL_ENOTSUPP;
      goto end;
    }
  
ret = ctx->backend->post(ctx, bucket, resource, subresource, object_type, metadata, sysmd, data_buf, data_len, query_params, resource_idp);
  
 end:

  DPL_TRACE(ctx, DPL_TRACE_REST, "ret=%d", ret);
  
  return ret;
}

dpl_status_t
dpl_post_buffered(dpl_ctx_t *ctx,
                  const char *bucket,
                  const char *resource,
                  const char *subresource,
                  dpl_ftype_t object_type,
                  dpl_dict_t *metadata,
                  dpl_sysmd_t *sysmd,
                  unsigned int data_len,
                  dpl_dict_t *query_params,
                  dpl_conn_t **connp)
{
  int ret;

  DPL_TRACE(ctx, DPL_TRACE_REST, "post bucket=%s resource=%s subresource=%s", bucket, resource, subresource);

  if (NULL == ctx->backend->post_buffered)
    {
      ret = DPL_ENOTSUPP;
      goto end;
    }

  ret = ctx->backend->post_buffered(ctx, bucket, resource, subresource, object_type, metadata, sysmd, data_len, query_params, connp);
  
 end:

  DPL_TRACE(ctx, DPL_TRACE_REST, "ret=%d", ret);
  
  return ret;
}


/**
 * put a resource
 *
 * @param ctx
 * @param bucket
 * @param resource
 * @param subresource can be NULL
 * @param metadata can be NULL
 * @param canned_acl
 * @param data_buf
 * @param data_len
 *
 * @return
 */
dpl_status_t
dpl_put(dpl_ctx_t *ctx,
        const char *bucket,
        const char *resource,
        const char *subresource,
        dpl_ftype_t object_type,
        dpl_dict_t *metadata,
        dpl_sysmd_t *sysmd,
        char *data_buf,
        unsigned int data_len)
{
  int ret;

  DPL_TRACE(ctx, DPL_TRACE_REST, "put bucket=%s resource=%s subresource=%s", bucket, resource, subresource);

  if (NULL == ctx->backend->put)
    {
      ret = DPL_ENOTSUPP;
      goto end;
    }

  ret = ctx->backend->put(ctx, bucket, resource, subresource, object_type, metadata, sysmd, data_buf, data_len);
  
 end:

  DPL_TRACE(ctx, DPL_TRACE_REST, "ret=%d", ret);
  
  return ret;
}

dpl_status_t
dpl_put_buffered(dpl_ctx_t *ctx,
                 const char *bucket,
                 const char *resource,
                 const char *subresource,
                 dpl_ftype_t object_type,
                 dpl_dict_t *metadata,
                 dpl_sysmd_t *sysmd,
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

  ret = ctx->backend->put_buffered(ctx, bucket, resource, subresource, object_type, metadata, sysmd, data_len, connp);
  
 end:

  DPL_TRACE(ctx, DPL_TRACE_REST, "ret=%d", ret);
  
  return ret;
}

/**
 * get a resource
 *
 * @param ctx
 * @param bucket
 * @param resource
 * @param subresource can be NULL
 * @param condition can be NULL
 * @param data_bufp must be freed by caller
 * @param data_lenp
 * @param metadatap must be freed by caller
 *
 * @return
 */
dpl_status_t
dpl_get(dpl_ctx_t *ctx,
        const char *bucket,
        const char *resource,
        const char *subresource,
        dpl_ftype_t object_type,
        dpl_condition_t *condition,
        char **data_bufp,
        unsigned int *data_lenp,
        dpl_dict_t **metadatap)
{
  int ret;

  DPL_TRACE(ctx, DPL_TRACE_REST, "get bucket=%s resource=%s subresource=%s", bucket, resource, subresource);

  if (NULL == ctx->backend->get)
    {
      ret = DPL_ENOTSUPP;
      goto end;
    }
  
  ret = ctx->backend->get(ctx, bucket, resource, subresource, object_type, condition, data_bufp, data_lenp, metadatap);
  
 end:

  DPL_TRACE(ctx, DPL_TRACE_REST, "ret=%d", ret);
  
  return ret;
}

/**
 * get a resource
 *
 * @param ctx
 * @param bucket
 * @param resource
 * @param subresource can be NULL
 * @param condition can be NULL
 * @param data_bufp must be freed by caller
 * @param data_lenp
 * @param metadatap must be freed by caller
 *
 * @return
 */
dpl_status_t
dpl_get_range(dpl_ctx_t *ctx,
              const char *bucket,
              const char *resource,
              const char *subresource,
              dpl_ftype_t object_type,
              dpl_condition_t *condition,
              int start,
              int end,
              char **data_bufp,
              unsigned int *data_lenp,
              dpl_dict_t **metadatap)
{
  int ret;

  DPL_TRACE(ctx, DPL_TRACE_REST, "get_range bucket=%s resource=%s subresource=%s", bucket, resource, subresource);

  if (NULL == ctx->backend->get_range)
    {
      ret = DPL_ENOTSUPP;
      goto end;
    }
  
  ret = ctx->backend->get_range(ctx, bucket, resource, subresource, object_type, condition, start, end, data_bufp, data_lenp, metadatap);
  
 end:

  DPL_TRACE(ctx, DPL_TRACE_REST, "ret=%d", ret);
  
  return ret;
}

dpl_status_t 
dpl_get_buffered(dpl_ctx_t *ctx,
                 const char *bucket,
                 const char *resource,
                 const char *subresource, 
                 dpl_ftype_t object_type,
                 dpl_condition_t *condition,
                 dpl_header_func_t header_func, 
                 dpl_buffer_func_t buffer_func,
                 void *cb_arg)
{
  int ret;

  DPL_TRACE(ctx, DPL_TRACE_REST, "get_buffered bucket=%s resource=%s subresource=%s", bucket, resource, subresource);

  if (NULL == ctx->backend->get_buffered)
    {
      ret = DPL_ENOTSUPP;
      goto end;
    }
  
  ret = ctx->backend->get_buffered(ctx, bucket, resource, subresource, object_type, condition, header_func, buffer_func, cb_arg);
  
 end:

  DPL_TRACE(ctx, DPL_TRACE_REST, "ret=%d", ret);
  
  return ret;
}

dpl_status_t
dpl_head(dpl_ctx_t *ctx,
         const char *bucket,
         const char *resource,
         const char *subresource,
         dpl_condition_t *condition,
         dpl_dict_t **metadatap)
{
  int ret;

  DPL_TRACE(ctx, DPL_TRACE_REST, "head bucket=%s resource=%s subresource=%s", bucket, resource, subresource);

  if (NULL == ctx->backend->head)
    {
      ret = DPL_ENOTSUPP;
      goto end;
    }
  
  ret = ctx->backend->head(ctx, bucket, resource, subresource, DPL_FTYPE_UNDEF, condition, metadatap);
  
 end:

  DPL_TRACE(ctx, DPL_TRACE_REST, "ret=%d", ret);
  
  return ret;
}


/**
 * get metadata
 *
 * @param ctx
 * @param bucket
 * @param resource
 * @param subresource can be NULL
 * @param condition can be NULL
 * @param all_headers tells us if we grab all the headers or just metadata
 * @param metadatap must be freed by caller
 *
 * @return
 */
dpl_status_t
dpl_head_all(dpl_ctx_t *ctx,
             const char *bucket,
             const char *resource,
             const char *subresource,
             dpl_condition_t *condition,
             dpl_dict_t **metadatap)
{
  int ret;

  DPL_TRACE(ctx, DPL_TRACE_REST, "head_all bucket=%s resource=%s subresource=%s", bucket, resource, subresource);

  if (NULL == ctx->backend->head_all)
    {
      ret = DPL_ENOTSUPP;
      goto end;
    }
  
  ret = ctx->backend->head_all(ctx, bucket, resource, subresource, DPL_FTYPE_UNDEF, condition, metadatap);
  
 end:

  DPL_TRACE(ctx, DPL_TRACE_REST, "ret=%d", ret);
  
  return ret;
}

dpl_status_t
dpl_head_sysmd(dpl_ctx_t *ctx,
               const char *bucket,
               const char *resource,
               const char *subresource,
               dpl_condition_t *condition,
               dpl_sysmd_t *sysmdp,
               dpl_dict_t **metadatap)
{
  int ret;

  DPL_TRACE(ctx, DPL_TRACE_REST, "head_sysmd bucket=%s resource=%s subresource=%s", bucket, resource, subresource);

  if (NULL == ctx->backend->head_sysmd)
    {
      ret = DPL_ENOTSUPP;
      goto end;
    }

  ret = ctx->backend->head_sysmd(ctx, bucket, resource, subresource, DPL_FTYPE_UNDEF, condition, sysmdp, metadatap);
  
 end:

  DPL_TRACE(ctx, DPL_TRACE_REST, "ret=%d", ret);
  
  return ret;
}

dpl_status_t
dpl_get_metadata_from_headers(dpl_ctx_t *ctx,
                              const dpl_dict_t *headers,
                              dpl_dict_t *metadata)
{
  int ret;

  DPL_TRACE(ctx, DPL_TRACE_REST, "get_metadata_from_headers");

  if (NULL == ctx->backend->get_metadata_from_headers)
    {
      ret = DPL_ENOTSUPP;
      goto end;
    }
  
  ret = ctx->backend->get_metadata_from_headers(headers, metadata);
  
 end:

  DPL_TRACE(ctx, DPL_TRACE_REST, "ret=%d", ret);
  
  return ret;
}


/**
 * delete a resource
 *
 * @param ctx
 * @param bucket
 * @param resource
 * @param subresource can be NULL
 *
 * @return
 */
dpl_status_t
dpl_delete(dpl_ctx_t *ctx,
           const char *bucket,
           const char *resource,
           const char *subresource)
{
  int ret;

  DPL_TRACE(ctx, DPL_TRACE_REST, "delete bucket=%s resource=%s subresource=%s", bucket, resource, subresource);

  if (NULL == ctx->backend->deletef)
    {
      ret = DPL_ENOTSUPP;
      goto end;
    }
  
  ret = ctx->backend->deletef(ctx, bucket, resource, subresource, DPL_FTYPE_UNDEF);
  
 end:

  DPL_TRACE(ctx, DPL_TRACE_REST, "ret=%d", ret);
  
  return ret;
}

/**
 * generate url
 *
 * @param ctx
 * @param bucket
 * @param resource
 * @param subresource can be NULL
 * @param condition can be NULL
 * @param metadatap must be freed by caller
 *
 * @return
 */
dpl_status_t
dpl_genurl(dpl_ctx_t *ctx,
           const char *bucket,
           const char *resource,
           const char *subresource,
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
  
  ret = ctx->backend->genurl(ctx, bucket, resource, subresource, expires, buf, len, lenp);
  
 end:

  DPL_TRACE(ctx, DPL_TRACE_REST, "ret=%d", ret);
  
  return ret;
}

/**
 * copy a resource
 *
 * @param ctx
 * @param src_bucket
 * @param src_resource
 * @param src_subresource
 * @param dst_bucket
 * @param dst_resource
 * @param dst_subresource
 * @param metadata_directive
 * @param metadata
 * @param canned_acl
 * @param condition
 *
 * @return
 */
dpl_status_t
dpl_copy(dpl_ctx_t *ctx,
         const char *src_bucket,
         const char *src_resource,
         const char *src_subresource,
         const char *dst_bucket,
         const char *dst_resource,
         const char *dst_subresource,
         dpl_ftype_t object_type,
         dpl_metadata_directive_t metadata_directive,
         dpl_dict_t *metadata,
         dpl_sysmd_t *sysmd,
         dpl_condition_t *condition)
{
  int ret;

  DPL_TRACE(ctx, DPL_TRACE_REST, "copy src_bucket=%s src_resource=%s src_subresource=%s dst_bucket=%s dst_resource=%s dst_subresource=%s", src_bucket, src_resource, src_subresource, dst_bucket, dst_resource, dst_subresource);

  if (NULL == ctx->backend->copy)
    {
      ret = DPL_ENOTSUPP;
      goto end;
    }
  
  ret = ctx->backend->copy(ctx, src_bucket, src_resource, src_subresource, dst_bucket, dst_resource, dst_subresource, object_type, metadata_directive, metadata, sysmd, condition);
  
 end:

  DPL_TRACE(ctx, DPL_TRACE_REST, "ret=%d", ret);
  
  return ret;
}
