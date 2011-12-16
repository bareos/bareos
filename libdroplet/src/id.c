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
dpl_post_id(dpl_ctx_t *ctx,
            const char *bucket,
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
  char *id_path = NULL;

  DPL_TRACE(ctx, DPL_TRACE_ID, "post_id bucket=%s subresource=%s", bucket, subresource);

  if (NULL == ctx->backend->get_id_path)
    {
      ret = DPL_ENOTSUPP;
      goto end;
    }

  ret = ctx->backend->get_id_path(ctx, bucket, &id_path);
  if (DPL_SUCCESS != ret)
    {
      goto end;
    }

  if (NULL == ctx->backend->post)
    {
      ret = DPL_ENOTSUPP;
      goto end;
    }
  
  ret = ctx->backend->post(ctx, bucket, id_path, subresource, object_type, metadata, sysmd, data_buf, data_len, query_params, resource_idp);
  
 end:

  if (NULL != id_path)
    free(id_path);

  DPL_TRACE(ctx, DPL_TRACE_ID, "ret=%d", ret);

  return ret;
}

dpl_status_t
dpl_post_buffered_id(dpl_ctx_t *ctx,
                     const char *bucket,
                     const char *subresource,
                     dpl_ftype_t object_type,
                     dpl_dict_t *metadata,
                     dpl_sysmd_t *sysmd,
                     unsigned int data_len,
                     dpl_dict_t *query_params,
                     dpl_conn_t **connp)
{
  int ret;
  char *id_path = NULL;

  DPL_TRACE(ctx, DPL_TRACE_ID, "post_buffered_id bucket=%s subresource=%s", bucket, subresource);

  if (NULL == ctx->backend->get_id_path)
    {
      ret = DPL_ENOTSUPP;
      goto end;
    }

  ret = ctx->backend->get_id_path(ctx, bucket, &id_path);
  if (DPL_SUCCESS != ret)
    {
      goto end;
    }

  if (NULL == ctx->backend->post_buffered)
    {
      ret = DPL_ENOTSUPP;
      goto end;
    }

  ret = ctx->backend->post_buffered(ctx, bucket, id_path, subresource, object_type, metadata, sysmd, data_len, query_params, connp);
  
 end:

  if (NULL != id_path)
    free(id_path);

  DPL_TRACE(ctx, DPL_TRACE_ID, "ret=%d", ret);
  
  return ret;
}

dpl_status_t
dpl_put_id(dpl_ctx_t *ctx,
           const char *bucket,
           const char *id,
           const char *subresource,
           dpl_ftype_t object_type,
           dpl_dict_t *metadata,
           dpl_sysmd_t *sysmd,
           char *data_buf,
           unsigned int data_len)
{
  int ret;
  char *id_path = NULL;
  char resource[DPL_MAXPATHLEN];

  DPL_TRACE(ctx, DPL_TRACE_ID, "put_id bucket=%s id=%s subresource=%s", bucket, id, subresource);

  if (NULL == ctx->backend->get_id_path)
    {
      ret = DPL_ENOTSUPP;
      goto end;
    }

  ret = ctx->backend->get_id_path(ctx, bucket, &id_path);
  if (DPL_SUCCESS != ret)
    {
      goto end;
    }

  snprintf(resource, sizeof (resource), "%s%s", id_path ? id_path : "", id);

  if (NULL == ctx->backend->put)
    {
      ret = DPL_ENOTSUPP;
      goto end;
    }

  ret = ctx->backend->put(ctx, bucket, resource, subresource, object_type, metadata, sysmd, data_buf, data_len);
  
 end:

  if (NULL != id_path)
    free(id_path);

  DPL_TRACE(ctx, DPL_TRACE_ID, "ret=%d", ret);
  
  return ret;
}

dpl_status_t
dpl_put_buffered_id(dpl_ctx_t *ctx,
                    const char *bucket,
                    const char *id,
                    const char *subresource,
                    dpl_ftype_t object_type,
                    dpl_dict_t *metadata,
                    dpl_sysmd_t *sysmd,
                    unsigned int data_len,
                    dpl_conn_t **connp)
{
  int ret;
  char *id_path = NULL;
  char resource[DPL_MAXPATHLEN];

  DPL_TRACE(ctx, DPL_TRACE_ID, "put_buffered_id bucket=%s id=%s subresource=%s", bucket, id, subresource);

  if (NULL == ctx->backend->get_id_path)
    {
      ret = DPL_ENOTSUPP;
      goto end;
    }

  ret = ctx->backend->get_id_path(ctx, bucket, &id_path);
  if (DPL_SUCCESS != ret)
    {
      goto end;
    }

  snprintf(resource, sizeof (resource), "%s%s", id_path ? id_path : "", id);

  if (NULL == ctx->backend->put_buffered)
    {
      ret = DPL_ENOTSUPP;
      goto end;
    }

  ret = ctx->backend->put_buffered(ctx, bucket, resource, subresource, object_type, metadata, sysmd, data_len, connp);
  
 end:

  if (NULL != id_path)
    free(id_path);

  DPL_TRACE(ctx, DPL_TRACE_ID, "ret=%d", ret);
  
  return ret;
}

dpl_status_t
dpl_get_id(dpl_ctx_t *ctx,
           const char *bucket,
           const char *id,
           const char *subresource,
           dpl_ftype_t object_type,
           dpl_condition_t *condition,
           char **data_bufp,
           unsigned int *data_lenp,
           dpl_dict_t **metadatap)
{
  int ret;
  char *id_path = NULL;
  char resource[DPL_MAXPATHLEN];

  DPL_TRACE(ctx, DPL_TRACE_ID, "get_id bucket=%s id=%s subresource=%s", bucket, id, subresource);

  if (NULL == ctx->backend->get_id_path)
    {
      ret = DPL_ENOTSUPP;
      goto end;
    }

  ret = ctx->backend->get_id_path(ctx, bucket, &id_path);
  if (DPL_SUCCESS != ret)
    {
      goto end;
    }

  snprintf(resource, sizeof (resource), "%s%s", id_path ? id_path : "", id);

  if (NULL == ctx->backend->get)
    {
      ret = DPL_ENOTSUPP;
      goto end;
    }
  
  ret = ctx->backend->get(ctx, bucket, resource, subresource, object_type, condition, data_bufp, data_lenp, metadatap);
  
 end:

  if (NULL != id_path)
    free(id_path);

  DPL_TRACE(ctx, DPL_TRACE_ID, "ret=%d", ret);
  
  return ret;
}

dpl_status_t
dpl_get_range_id(dpl_ctx_t *ctx,
                 const char *bucket,
                 const char *id,
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
  char *id_path = NULL;
  char resource[DPL_MAXPATHLEN];

  DPL_TRACE(ctx, DPL_TRACE_ID, "get_range_id bucket=%s id=%s subresource=%s", bucket, id, subresource);

  if (NULL == ctx->backend->get_id_path)
    {
      ret = DPL_ENOTSUPP;
      goto end;
    }

  ret = ctx->backend->get_id_path(ctx, bucket, &id_path);
  if (DPL_SUCCESS != ret)
    {
      goto end;
    }

  snprintf(resource, sizeof (resource), "%s%s", id_path ? id_path : "", id);

  if (NULL == ctx->backend->get_range)
    {
      ret = DPL_ENOTSUPP;
      goto end;
    }
  
  ret = ctx->backend->get_range(ctx, bucket, resource, subresource, object_type, condition, start, end, data_bufp, data_lenp, metadatap);
  
 end:

  if (NULL != id_path)
    free(id_path);

  DPL_TRACE(ctx, DPL_TRACE_ID, "ret=%d", ret);
  
  return ret;
}

dpl_status_t 
dpl_get_buffered_id(dpl_ctx_t *ctx,
                    const char *bucket,
                    const char *id,
                    const char *subresource, 
                    dpl_ftype_t object_type,
                    dpl_condition_t *condition,
                    dpl_header_func_t header_func, 
                    dpl_buffer_func_t buffer_func,
                    void *cb_arg)
{
  int ret;
  char *id_path = NULL;
  char resource[DPL_MAXPATHLEN];

  DPL_TRACE(ctx, DPL_TRACE_ID, "get_buffered_id bucket=%s id=%s subresource=%s", bucket, id, subresource);

  if (NULL == ctx->backend->get_id_path)
    {
      ret = DPL_ENOTSUPP;
      goto end;
    }

  ret = ctx->backend->get_id_path(ctx, bucket, &id_path);
  if (DPL_SUCCESS != ret)
    {
      goto end;
    }

  snprintf(resource, sizeof (resource), "%s%s", id_path ? id_path : "", id);

  if (NULL == ctx->backend->get_buffered)
    {
      ret = DPL_ENOTSUPP;
      goto end;
    }
  
  ret = ctx->backend->get_buffered(ctx, bucket, resource, subresource, object_type, condition, header_func, buffer_func, cb_arg);
  
 end:

  if (NULL != id_path)
    free(id_path);

  DPL_TRACE(ctx, DPL_TRACE_ID, "ret=%d", ret);
  
  return ret;
}

dpl_status_t
dpl_head_id(dpl_ctx_t *ctx,
            const char *bucket,
            const char *id,
            const char *subresource,
            dpl_condition_t *condition,
            dpl_dict_t **metadatap)
{
  int ret;
  char *id_path = NULL;
  char resource[DPL_MAXPATHLEN];

  DPL_TRACE(ctx, DPL_TRACE_ID, "head_id bucket=%s id=%s subresource=%s", bucket, id, subresource);

  if (NULL == ctx->backend->get_id_path)
    {
      ret = DPL_ENOTSUPP;
      goto end;
    }

  ret = ctx->backend->get_id_path(ctx, bucket, &id_path);
  if (DPL_SUCCESS != ret)
    {
      goto end;
    }

  snprintf(resource, sizeof (resource), "%s%s", id_path ? id_path : "", id);

  if (NULL == ctx->backend->head)
    {
      ret = DPL_ENOTSUPP;
      goto end;
    }
  
  ret = ctx->backend->head(ctx, bucket, resource, subresource, DPL_FTYPE_UNDEF, condition, metadatap);
  
 end:

  if (NULL != id_path)
    free(id_path);

  DPL_TRACE(ctx, DPL_TRACE_ID, "ret=%d", ret);
  
  return ret;
}

dpl_status_t
dpl_head_all_id(dpl_ctx_t *ctx,
                const char *bucket,
                const char *id,
                const char *subresource,
                dpl_condition_t *condition,
                dpl_dict_t **metadatap)
{
  int ret;
  char *id_path = NULL;
  char resource[DPL_MAXPATHLEN];

  DPL_TRACE(ctx, DPL_TRACE_ID, "head_all_id bucket=%s id=%s subresource=%s", bucket, id, subresource);

  if (NULL == ctx->backend->get_id_path)
    {
      ret = DPL_ENOTSUPP;
      goto end;
    }

  ret = ctx->backend->get_id_path(ctx, bucket, &id_path);
  if (DPL_SUCCESS != ret)
    {
      goto end;
    }

  snprintf(resource, sizeof (resource), "%s%s", id_path ? id_path : "", id);

  if (NULL == ctx->backend->head_all)
    {
      ret = DPL_ENOTSUPP;
      goto end;
    }
  
  ret = ctx->backend->head_all(ctx, bucket, resource, subresource, DPL_FTYPE_UNDEF, condition, metadatap);
  
 end:

  if (NULL != id_path)
    free(id_path);

  DPL_TRACE(ctx, DPL_TRACE_ID, "ret=%d", ret);
  
  return ret;
}

dpl_status_t
dpl_head_sysmd_id(dpl_ctx_t *ctx,
                  const char *bucket,
                  const char *id,
                  const char *subresource,
                  dpl_condition_t *condition,
                  dpl_sysmd_t *sysmdp,
                  dpl_dict_t **metadatap)
{
  int ret;
  char *id_path = NULL;
  char resource[DPL_MAXPATHLEN];

  DPL_TRACE(ctx, DPL_TRACE_ID, "head_sysmd_id bucket=%s id=%s subresource=%s", bucket, id, subresource);

  if (NULL == ctx->backend->get_id_path)
    {
      ret = DPL_ENOTSUPP;
      goto end;
    }

  ret = ctx->backend->get_id_path(ctx, bucket, &id_path);
  if (DPL_SUCCESS != ret)
    {
      goto end;
    }

  snprintf(resource, sizeof (resource), "%s%s", id_path ? id_path : "", id);

  if (NULL == ctx->backend->head_sysmd)
    {
      ret = DPL_ENOTSUPP;
      goto end;
    }
  
  ret = ctx->backend->head_sysmd(ctx, bucket, resource, subresource, DPL_FTYPE_UNDEF, condition, sysmdp, metadatap);
  
 end:

  if (NULL != id_path)
    free(id_path);

  DPL_TRACE(ctx, DPL_TRACE_ID, "ret=%d", ret);
  
  return ret;
}

dpl_status_t
dpl_delete_id(dpl_ctx_t *ctx,
              const char *bucket,
              const char *id,
              const char *subresource)
{
  int ret;
  char *id_path = NULL;
  char resource[DPL_MAXPATHLEN];

  DPL_TRACE(ctx, DPL_TRACE_ID, "delete bucket=%s id=%s subresource=%s", bucket, id, subresource);

  if (NULL == ctx->backend->get_id_path)
    {
      ret = DPL_ENOTSUPP;
      goto end;
    }

  ret = ctx->backend->get_id_path(ctx, bucket, &id_path);
  if (DPL_SUCCESS != ret)
    {
      goto end;
    }

  snprintf(resource, sizeof (resource), "%s%s", id_path ? id_path : "", id);

  if (NULL == ctx->backend->deletef)
    {
      ret = DPL_ENOTSUPP;
      goto end;
    }
  
  ret = ctx->backend->deletef(ctx, bucket, resource, subresource, DPL_FTYPE_UNDEF);
  
 end:

  if (NULL != id_path)
    free(id_path);

  DPL_TRACE(ctx, DPL_TRACE_ID, "ret=%d", ret);
  
  return ret;
}

dpl_status_t
dpl_copy_id(dpl_ctx_t *ctx,
            const char *src_bucket,
            const char *src_id,
            const char *src_subresource,
            const char *dst_bucket,
            const char *dst_id,
            const char *dst_subresource,
            dpl_ftype_t object_type,
            dpl_metadata_directive_t metadata_directive,
            dpl_dict_t *metadata,
            dpl_sysmd_t *sysmd,
            dpl_condition_t *condition)
{
  int ret;
  char *src_id_path = NULL;
  char *dst_id_path = NULL;
  char src_resource[DPL_MAXPATHLEN];
  char dst_resource[DPL_MAXPATHLEN];

  DPL_TRACE(ctx, DPL_TRACE_ID, "copy src_bucket=%s src_id=%s src_subresource=%s dst_bucket=%s dst_id=%s dst_subresource=%s", src_bucket, src_id, src_subresource, dst_bucket, dst_id, dst_subresource);

  if (NULL == ctx->backend->get_id_path)
    {
      ret = DPL_ENOTSUPP;
      goto end;
    }

  ret = ctx->backend->get_id_path(ctx, src_bucket, &src_id_path);
  if (DPL_SUCCESS != ret)
    {
      goto end;
    }

  snprintf(src_resource, sizeof (src_resource), "%s%s", src_id_path ? src_id_path : "", src_id);

  ret = ctx->backend->get_id_path(ctx, dst_bucket, &dst_id_path);
  if (DPL_SUCCESS != ret)
    {
      goto end;
    }

  snprintf(dst_resource, sizeof (dst_resource), "%s%s", dst_id_path ? dst_id_path : "", dst_id);

  if (NULL == ctx->backend->copy)
    {
      ret = DPL_ENOTSUPP;
      goto end;
    }
  
  ret = ctx->backend->copy(ctx, src_bucket, src_resource, src_subresource, dst_bucket, dst_resource, dst_subresource, object_type, metadata_directive, metadata, sysmd, condition);
  
 end:

  if (NULL != dst_id_path)
    free(dst_id_path);

  if (NULL != src_id_path)
    free(src_id_path);

  DPL_TRACE(ctx, DPL_TRACE_ID, "ret=%d", ret);
  
  return ret;
}

dpl_status_t
dpl_gen_id_from_oid(dpl_ctx_t *ctx,
                    uint64_t oid,
                    dpl_storage_class_t storage_class,
                    char **resource_idp)
{
  int ret;
  char *resource_id = NULL;

  DPL_TRACE(ctx, DPL_TRACE_ID, "gen_id_from_oid oid=%llu\n", (unsigned long long) oid);

  if (NULL == ctx->backend->gen_id_from_oid)
    {
      ret = DPL_ENOTSUPP;
      goto end;
    }

  ret = ctx->backend->gen_id_from_oid(ctx, oid, storage_class, &resource_id);
  if (DPL_SUCCESS != ret)
    {
      goto end;
    }

  if (NULL != resource_idp)
    {
      *resource_idp = resource_id;
      resource_id = NULL;
    }
  
 end:

  if (NULL != resource_id)
    free(resource_id);

  DPL_TRACE(ctx, DPL_TRACE_ID, "ret=%d", ret);
  
  return ret;
}

