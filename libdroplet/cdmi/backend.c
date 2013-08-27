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
#include <droplet/cdmi/cdmi.h>
#include <droplet/cdmi/object_id.h>

/** @file */

//#define DPRINTF(fmt,...) fprintf(stderr, fmt, ##__VA_ARGS__)
#define DPRINTF(fmt,...)

dpl_status_t
dpl_cdmi_get_capabilities(dpl_ctx_t *ctx,
                          dpl_capability_t *maskp)
{
  if (NULL != maskp)
    *maskp = DPL_CAP_FNAMES|
      DPL_CAP_IDS|
      DPL_CAP_HTTP_COMPAT|
      DPL_CAP_RAW|
      DPL_CAP_APPEND_METADATA|
      DPL_CAP_CONDITIONS|
      DPL_CAP_PUT_RANGE;
  
  return DPL_SUCCESS;
}

dpl_status_t
dpl_cdmi_get_id_scheme(dpl_ctx_t *ctx,
                       dpl_id_scheme_t **id_schemep)
{
  if (NULL != id_schemep)
    *id_schemep = DPL_ID_SCHEME_ANY;
  
  return DPL_SUCCESS;
}

dpl_status_t
dpl_cdmi_list_bucket(dpl_ctx_t *ctx,
                     const char *bucket,
                     const char *prefix,
                     const char *delimiter,
                     const int max_keys,
                     dpl_vec_t **objectsp,
                     dpl_vec_t **common_prefixesp,
                     char **locationp)
{
  int           ret, ret2;
  dpl_conn_t   *conn = NULL;
  char          header[dpl_header_size];
  u_int         header_len;
  struct iovec  iov[10];
  int           n_iov = 0;
  int           connection_close = 0;
  char          *data_buf = NULL;
  u_int         data_len;
  dpl_dict_t    *headers_request = NULL;
  dpl_dict_t    *headers_reply = NULL;
  dpl_req_t     *req = NULL;
  dpl_vec_t     *common_prefixes = NULL;
  dpl_vec_t     *objects = NULL;

  DPL_TRACE(ctx, DPL_TRACE_BACKEND, "");

  req = dpl_req_new(ctx);
  if (NULL == req)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  dpl_req_set_method(req, DPL_METHOD_GET);

  ret2 = dpl_cdmi_req_set_resource(req, NULL != prefix ? prefix : "/");
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  dpl_req_set_object_type(req, DPL_FTYPE_DIR);

  //build request
  ret2 = dpl_cdmi_req_build(req, 0, &headers_request, NULL, NULL);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  //contact default host
  dpl_req_rm_behavior(req, DPL_BEHAVIOR_VIRTUAL_HOSTING);

  ret2 = dpl_try_connect(ctx, req, &conn);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  ret2 = dpl_add_host_to_headers(req, headers_request);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  ret2 = dpl_req_gen_http_request(ctx, req, headers_request, NULL, header, sizeof (header), &header_len);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  iov[n_iov].iov_base = header;
  iov[n_iov].iov_len = header_len;
  n_iov++;

  //final crlf
  iov[n_iov].iov_base = "\r\n";
  iov[n_iov].iov_len = 2;
  n_iov++;

  ret2 = dpl_conn_writev_all(conn, iov, n_iov, conn->ctx->write_timeout);
  if (DPL_SUCCESS != ret2)
    {
      DPL_TRACE(conn->ctx, DPL_TRACE_ERR, "writev failed");
      connection_close = 1;
      ret = ret2;
      goto end;
    }

  ret2 = dpl_read_http_reply(conn, 1, &data_buf, &data_len, &headers_reply, &connection_close);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  objects = dpl_vec_new(2, 2);
  if (NULL == objects)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  common_prefixes = dpl_vec_new(2, 2);
  if (NULL == common_prefixes)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  ret2 = dpl_cdmi_parse_list_bucket(ctx, data_buf, data_len, prefix, objects, common_prefixes);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  if (NULL != objectsp)
    {
      *objectsp = objects;
      objects = NULL; //consume it
    }

  if (NULL != common_prefixesp)
    {
      *common_prefixesp = common_prefixes;
      common_prefixes = NULL; //consume it
    }

  ret = DPL_SUCCESS;

 end:

  if (NULL != objects)
    dpl_vec_objects_free(objects);

  if (NULL != common_prefixes)
    dpl_vec_common_prefixes_free(common_prefixes);

  if (NULL != data_buf)
    free(data_buf);

  if (NULL != conn)
    {
      if (1 == connection_close)
        dpl_conn_terminate(conn);
      else
        dpl_conn_release(conn);
    }

  if (NULL != headers_reply)
    dpl_dict_free(headers_reply);

  if (NULL != headers_request)
    dpl_dict_free(headers_request);

  if (NULL != req)
    dpl_req_free(req);

  DPL_TRACE(ctx, DPL_TRACE_BACKEND, "ret=%d", ret);

  return ret;
}

dpl_status_t
dpl_cdmi_put_internal(dpl_ctx_t *ctx,
                      int post,
                      const char *bucket,
                      const char *resource,
                      const char *subresource,
                      const dpl_option_t *option,
                      dpl_ftype_t object_type,
                      const dpl_condition_t *condition,
                      const dpl_range_t *range,
                      const dpl_dict_t *metadata,
                      const dpl_sysmd_t *sysmd,
                      const char *data_buf,
                      unsigned int data_len,
                      const dpl_dict_t *query_params,
                      dpl_sysmd_t *returned_sysmdp,
                      char **locationp)
{
  int           ret, ret2;
  dpl_conn_t   *conn = NULL;
  char          header[dpl_header_size];
  u_int         header_len;
  struct iovec  iov[10];
  int           n_iov = 0;
  int           connection_close = 0;
  dpl_dict_t    *headers_request = NULL;
  dpl_dict_t    *headers_reply = NULL;
  dpl_req_t     *req = NULL;
  char          *body_str = NULL;
  int           body_len = 0;
  char *data_buf_returned = NULL;
  u_int data_len_returned;
  dpl_value_t *val = NULL;
  dpl_cdmi_req_mask_t req_mask = 0u;

  DPL_TRACE(ctx, DPL_TRACE_BACKEND, "");

  if (option)
    {
      if (option->mask & DPL_OPTION_HTTP_COMPAT)
        req_mask |= DPL_CDMI_REQ_HTTP_COMPAT;
    }

  req = dpl_req_new(ctx);
  if (NULL == req)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  if (post)
    dpl_req_set_method(req, DPL_METHOD_POST);
  else
    dpl_req_set_method(req, DPL_METHOD_PUT);

  ret2 = dpl_cdmi_req_set_resource(req, resource);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  if (NULL != subresource)
    {
      ret2 = dpl_req_set_subresource(req, subresource);
      if (DPL_SUCCESS != ret2)
        {
          ret = ret2;
          goto end;
        }
    }

  if (NULL != condition)
    {
      dpl_req_set_condition(req, condition);
    }

  if (range)
    {
      ret2 = dpl_cdmi_req_add_range(req, req_mask, range);
      if (DPL_SUCCESS != ret2)
        {
          ret = ret2;
          goto end;
        }
    }

  dpl_req_set_object_type(req, object_type);

  dpl_req_set_data(req, data_buf, data_len);

  dpl_req_add_behavior(req, DPL_BEHAVIOR_MD5);

  if (NULL != sysmd)
    {
      ret2 = dpl_cdmi_add_sysmd_to_req(sysmd, req);
      if (DPL_SUCCESS != ret2)
        {
          ret = ret2;
          goto end;
        }
    }

  if (NULL != metadata)
    {
      ret2 = dpl_cdmi_req_add_metadata(req, metadata, option ? option->mask & DPL_OPTION_APPEND_METADATA : 0);
      if (DPL_SUCCESS != ret2)
        {
          ret = ret2;
          goto end;
        }
    }

  //build request
  ret2 = dpl_cdmi_req_build(req, 0, &headers_request, &body_str, &body_len);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  //contact default host
  dpl_req_rm_behavior(req, DPL_BEHAVIOR_VIRTUAL_HOSTING);

  ret2 = dpl_try_connect(ctx, req, &conn);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  ret2 = dpl_add_host_to_headers(req, headers_request);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  ret2 = dpl_req_gen_http_request(ctx, req, headers_request, NULL, header, sizeof (header), &header_len);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  iov[n_iov].iov_base = header;
  iov[n_iov].iov_len = header_len;
  n_iov++;

  //final crlf
  iov[n_iov].iov_base = "\r\n";
  iov[n_iov].iov_len = 2;
  n_iov++;

  //buffer
  iov[n_iov].iov_base = body_str;
  iov[n_iov].iov_len = body_len;
  n_iov++;

  ret2 = dpl_conn_writev_all(conn, iov, n_iov, conn->ctx->write_timeout);
  if (DPL_SUCCESS != ret2)
    {
      DPL_TRACE(conn->ctx, DPL_TRACE_ERR, "writev failed");
      connection_close = 1;
      ret = ret2;
      goto end;
    }

  ret2 = dpl_read_http_reply(conn, 1, &data_buf_returned, &data_len_returned, &headers_reply, &connection_close);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  if (post)
    {
      if (req_mask & DPL_CDMI_REQ_HTTP_COMPAT)
        {
          ret2 = dpl_cdmi_get_metadata_from_headers(headers_reply, NULL, returned_sysmdp);
          if (DPL_SUCCESS != ret2)
            {
              ret = ret2;
              goto end;
            }
        }
      else
        {
          ret2 = dpl_cdmi_parse_json_buffer(ctx, data_buf_returned, data_len_returned, &val);
          if (DPL_SUCCESS != ret2)
            {
              ret = ret2;
              goto end;
            }
          
          if (DPL_VALUE_SUBDICT != val->type)
            {
              ret = DPL_EINVAL;
              goto end;
            }
          
          ret2 = dpl_cdmi_get_metadata_from_values(val->subdict, NULL, returned_sysmdp);
          if (DPL_SUCCESS != ret2)
            {
              ret = ret2;
              goto end;
            }
        }
      
      if (NULL != returned_sysmdp)
        {
          dpl_dict_var_t *var;
          int base_path_len = strlen(ctx->base_path);
          
          //location contains the path to new location
          ret2 = dpl_dict_get_lowered(headers_reply, "location", &var);
          if (DPL_SUCCESS != ret2)
            {
              ret = ret2;
              goto end;
            }
          
          if (DPL_VALUE_STRING != var->val->type)
            {
              ret = DPL_EINVAL;
              goto end;
            }
          
          //remove the base_path from the answer
          if (strncmp(dpl_sbuf_get_str(var->val->string),
                      ctx->base_path, base_path_len))
            {
              ret = DPL_EINVAL;
              goto end;
            }
          
          returned_sysmdp->mask |= DPL_SYSMD_MASK_PATH;
          strncpy(returned_sysmdp->path,
                  dpl_sbuf_get_str(var->val->string) + base_path_len,
                  DPL_MAXPATHLEN);
        }
    }
  
  ret = DPL_SUCCESS;

 end:

  if (NULL != val)
    dpl_value_free(val);

  if (NULL != data_buf_returned)
    free(data_buf_returned);

  if (NULL != body_str)
    free(body_str);

  if (NULL != conn)
    {
      if (1 == connection_close)
        dpl_conn_terminate(conn);
      else
        dpl_conn_release(conn);
    }

  if (NULL != headers_reply)
    dpl_dict_free(headers_reply);

  if (NULL != headers_request)
    dpl_dict_free(headers_request);

  if (NULL != req)
    dpl_req_free(req);

  DPL_TRACE(ctx, DPL_TRACE_BACKEND, "ret=%d", ret);

  return ret;
}

dpl_status_t
dpl_cdmi_put_buffered_internal(dpl_ctx_t *ctx,
                               int post,
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
                               const dpl_dict_t *query_params,
                               dpl_conn_t **connp,
                               char **locationp)
{
  int           ret, ret2;
  dpl_conn_t    *conn = NULL;
  char          header[dpl_header_size];
  u_int         header_len;
  struct iovec  iov[10];
  int           n_iov = 0;
  int           connection_close = 0;
  dpl_dict_t    *headers_request = NULL;
  dpl_dict_t    *headers_reply = NULL;
  dpl_req_t     *req = NULL;
  dpl_cdmi_req_mask_t req_mask = 0u;

  DPL_TRACE(ctx, DPL_TRACE_BACKEND, "");

  if (option)
    {
      if (option->mask & DPL_OPTION_HTTP_COMPAT)
        req_mask |= DPL_CDMI_REQ_HTTP_COMPAT;
    }

  req = dpl_req_new(ctx);
  if (NULL == req)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  if (post)
    dpl_req_set_method(req, DPL_METHOD_POST);
  else
    dpl_req_set_method(req, DPL_METHOD_PUT);

  if (NULL != bucket)
    {
      ret2 = dpl_req_set_bucket(req, bucket);
      if (DPL_SUCCESS != ret2)
        {
          ret = ret2;
          goto end;
        }
    }

  ret2 = dpl_cdmi_req_set_resource(req, resource);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  if (NULL != subresource)
    {
      ret2 = dpl_req_set_subresource(req, subresource);
      if (DPL_SUCCESS != ret2)
        {
          ret = ret2;
          goto end;
        }
    }

  dpl_req_set_data(req, NULL, data_len);

  if (NULL != condition)
    {
      dpl_req_set_condition(req, condition);
    }

  if (!(req_mask & DPL_CDMI_REQ_HTTP_COMPAT))
    {
      ret = DPL_ENOTSUPP;
      goto end;
    }

  if (range)
    {
      ret2 = dpl_cdmi_req_add_range(req, req_mask, range);
      if (DPL_SUCCESS != ret2)
        {
          ret = ret2;
          goto end;
        }
    }

  dpl_req_set_object_type(req, object_type);

  dpl_req_add_behavior(req, DPL_BEHAVIOR_EXPECT);

  if (NULL != sysmd)
    {
      ret2 = dpl_cdmi_add_sysmd_to_req(sysmd, req);
      if (DPL_SUCCESS != ret2)
        {
          ret = ret2;
          goto end;
        }
    }

  if (NULL != metadata)
    {
      ret2 = dpl_cdmi_req_add_metadata(req, metadata, option ? option->mask & DPL_OPTION_APPEND_METADATA : 0);
      if (DPL_SUCCESS != ret2)
        {
          ret = ret2;
          goto end;
        }
    }

  ret2 = dpl_cdmi_req_build(req, req_mask, &headers_request, NULL, NULL);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  //contact default host
  dpl_req_rm_behavior(req, DPL_BEHAVIOR_VIRTUAL_HOSTING);

  ret2 = dpl_try_connect(ctx, req, &conn);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  ret2 = dpl_add_host_to_headers(req, headers_request);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  ret2 = dpl_req_gen_http_request(ctx, req, headers_request, NULL, header, sizeof (header), &header_len);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  iov[n_iov].iov_base = header;
  iov[n_iov].iov_len = header_len;
  n_iov++;

  //final crlf
  iov[n_iov].iov_base = "\r\n";
  iov[n_iov].iov_len = 2;
  n_iov++;

  ret2 = dpl_conn_writev_all(conn, iov, n_iov, conn->ctx->write_timeout);
  if (DPL_SUCCESS != ret2)
    {
      DPL_TRACE(conn->ctx, DPL_TRACE_ERR, "writev failed");
      connection_close = 1;
      ret = ret2;
      goto end;
    }

  ret2 = dpl_read_http_reply(conn, 1, NULL, NULL, &headers_reply, &connection_close);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  if (NULL != connp)
    {
      *connp = conn;
      conn = NULL; //consume it
    }

  ret = DPL_SUCCESS;

 end:

  if (NULL != conn)
    {
      if (1 == connection_close)
        dpl_conn_terminate(conn);
      else
        dpl_conn_release(conn);
    }

  if (NULL != headers_reply)
    dpl_dict_free(headers_reply);

  if (NULL != headers_request)
    dpl_dict_free(headers_request);

  if (NULL != req)
    dpl_req_free(req);

  DPL_TRACE(ctx, DPL_TRACE_BACKEND, "ret=%d", ret);

  return ret;
}

dpl_status_t
dpl_cdmi_post(dpl_ctx_t *ctx,
              const char *bucket,
              const char *resource,
              const char *subresource,
              const dpl_option_t *option,
              dpl_ftype_t object_type,
              const dpl_condition_t *condition,
              const dpl_range_t *range,
              const dpl_dict_t *metadata,
              const dpl_sysmd_t *sysmd,
              const char *data_buf,
              unsigned int data_len,
              const dpl_dict_t *query_params,
              dpl_sysmd_t *returned_sysmdp,
              char **locationp)
{
  return dpl_cdmi_put_internal(ctx, 1, bucket, resource, subresource,
                               option, object_type, condition,
                               range, metadata, sysmd, 
                               data_buf, data_len, query_params,
                               returned_sysmdp, locationp);
}

dpl_status_t
dpl_cdmi_post_buffered(dpl_ctx_t *ctx,
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
                       const dpl_dict_t *query_params,
                       dpl_conn_t **connp,
                       char **locationp)
{
  return dpl_cdmi_put_buffered_internal(ctx, 1, bucket, resource, subresource, 
                                        option, object_type, condition, range,
                                        metadata, sysmd, data_len, 
                                        query_params, connp, locationp);
}

dpl_status_t
dpl_cdmi_put(dpl_ctx_t *ctx,
             const char *bucket,
             const char *resource,
             const char *subresource,
             const dpl_option_t *option,
             dpl_ftype_t object_type,
             const dpl_condition_t *condition,
             const dpl_range_t *range,
             const dpl_dict_t *metadata,
             const dpl_sysmd_t *sysmd,
             const char *data_buf,
             unsigned int data_len,
             const dpl_dict_t *query_params,
             dpl_sysmd_t *returned_sysmdp,
             char **locationp)
{
  return dpl_cdmi_put_internal(ctx, 0, bucket, resource, subresource,
                               option, object_type, condition,
                               range, metadata, sysmd, 
                               data_buf, data_len, NULL,
                               NULL, locationp);
}

/** 
 * put range buffered
 *
 * @param ctx 
 * @param bucket 
 * @param resource 
 * @param subresource 
 * @param option DPL_OPTION_HTTP_COMPAT is mandatory
 * @param object_type 
 * @param range 
 * @param metadata 
 * @param sysmd 
 * @param data_len 
 * @param connp 
 * @param locationp 
 * 
 * @return 
 */
dpl_status_t
dpl_cdmi_put_buffered(dpl_ctx_t *ctx,
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
                      const dpl_dict_t *query_params,
                      dpl_conn_t **connp,
                      char **locationp)
{
  return dpl_cdmi_put_buffered_internal(ctx, 0, bucket, resource, subresource, 
                                        option, object_type, condition, range,
                                        metadata, sysmd, data_len, 
                                        query_params, connp, locationp);
}

dpl_status_t
dpl_cdmi_get(dpl_ctx_t *ctx,
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
             dpl_sysmd_t *sysmdp,
             char **locationp)
{
  int           ret, ret2;
  dpl_conn_t   *conn = NULL;
  char          header[dpl_header_size];
  u_int         header_len;
  struct iovec  iov[10];
  int           n_iov = 0;
  int           connection_close = 0;
  char          *data_buf = NULL;
  u_int         data_len;
  dpl_dict_t    *headers_request = NULL;
  dpl_dict_t    *headers_reply = NULL;
  dpl_req_t     *req = NULL;
  int raw = 0;
  dpl_value_t *val = NULL;
  dpl_dict_var_t *var = NULL;
  dpl_dict_var_t *encoding = NULL;
  int value_len;
  int orig_len;
  char *orig_buf = NULL;
  dpl_cdmi_req_mask_t req_mask = 0u;
  char *location;

  DPL_TRACE(ctx, DPL_TRACE_BACKEND, "");

  if (option)
    {
      if (option->mask & DPL_OPTION_HTTP_COMPAT)
        req_mask |= DPL_CDMI_REQ_HTTP_COMPAT;

      if (option->mask & DPL_OPTION_RAW)
        raw = 1;
    }

  req = dpl_req_new(ctx);
  if (NULL == req)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  dpl_req_set_method(req, DPL_METHOD_GET);

  if (NULL != bucket)
    {
      ret2 = dpl_req_set_bucket(req, bucket);
      if (DPL_SUCCESS != ret2)
        {
          ret = ret2;
          goto end;
        }
    }

  ret2 = dpl_cdmi_req_set_resource(req, resource);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  if (NULL == subresource)
    {
      if (DPL_FTYPE_REG == object_type)
        {
          subresource = "valuetransferencoding";
        }
    }

  if (NULL != subresource)
    {
      ret2 = dpl_req_set_subresource(req, subresource);
      if (DPL_SUCCESS != ret2)
        {
          ret = ret2;
          goto end;
        }
    }

  if (NULL != condition)
    {
      dpl_req_set_condition(req, condition);
    }

  if (range)
    {
      ret2 = dpl_cdmi_req_add_range(req, req_mask, range);
      if (DPL_SUCCESS != ret2)
        {
          ret = ret2;
          goto end;
        }
    }

  dpl_req_set_object_type(req, object_type);

  //build request
  ret2 = dpl_cdmi_req_build(req, 0, &headers_request, NULL, NULL);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  //contact default host
  dpl_req_rm_behavior(req, DPL_BEHAVIOR_VIRTUAL_HOSTING);

  ret2 = dpl_try_connect(ctx, req, &conn);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  ret2 = dpl_add_host_to_headers(req, headers_request);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  ret2 = dpl_req_gen_http_request(ctx, req, headers_request, NULL, header, sizeof (header), &header_len);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  iov[n_iov].iov_base = header;
  iov[n_iov].iov_len = header_len;
  n_iov++;

  //final crlf
  iov[n_iov].iov_base = "\r\n";
  iov[n_iov].iov_len = 2;
  n_iov++;

  ret2 = dpl_conn_writev_all(conn, iov, n_iov, conn->ctx->write_timeout);
  if (DPL_SUCCESS != ret2)
    {
      DPL_TRACE(conn->ctx, DPL_TRACE_ERR, "writev failed");
      connection_close = 1;
      ret = ret2;
      goto end;
    }

  if (option && option->mask & DPL_OPTION_NOALLOC)
    {
      data_buf = *data_bufp;
      data_len = *data_lenp;
    }

  ret2 = dpl_read_http_reply_ext(conn, 1, 
                                 (option && option->mask & DPL_OPTION_NOALLOC) ? 1 : 0,
                                 &data_buf, &data_len, &headers_reply, &connection_close);
  if (DPL_SUCCESS != ret2)
    {
      if (DPL_EREDIRECT == ret2)
        {
          if (NULL != locationp)
            {
              location = dpl_location(headers_reply);
              if (NULL == location)
                {
                  DPL_TRACE(conn->ctx, DPL_TRACE_ERR,
                            "missing \"Location\" header in redirect HTTP response");
                  connection_close = 1;
                  ret = DPL_FAILURE;
                  goto end;
                }
              *locationp = strdup(location);
            }
        }
      ret = ret2;
      goto end;
    }

  if (req_mask & DPL_CDMI_REQ_HTTP_COMPAT)
    {
      //metadata are in headers
      ret2 = dpl_cdmi_get_metadata_from_headers(headers_reply, metadatap, sysmdp);
      if (DPL_SUCCESS != ret2)
        {
          ret = ret2;
          goto end;
        }
    }
  else
    {
      if (!raw)
        {
          char *tmp;

          //extract data+metadata from json
          ret2 = dpl_cdmi_parse_json_buffer(ctx, data_buf, data_len, &val);
          if (DPL_SUCCESS != ret2)
            {
              ret = ret2;
              goto end;
            }
          
          if (DPL_VALUE_SUBDICT != val->type)
            {
              ret = DPL_EINVAL;
              goto end;
            }

          ret2 = dpl_cdmi_get_metadata_from_values(val->subdict, metadatap, sysmdp);
          if (DPL_SUCCESS != ret2)
            {
              ret = ret2;
              goto end;
            }
          
          //find the value object
          ret2 = dpl_dict_get_lowered(val->subdict, "value", &var);
          if (DPL_SUCCESS != ret2)
            {
              ret = ret2;
              goto end;
            }
          
          if (DPL_VALUE_STRING != var->val->type)
            {
              ret = DPL_EINVAL;
              goto end;
            }
          
          ret2 = dpl_dict_get_lowered(val->subdict, "valuetransferencoding",
                                      &encoding);
          if (DPL_SUCCESS != ret2)
            {
              ret = ret2;
              goto end;
            }

          if (DPL_VALUE_STRING != encoding->val->type)
            {
              ret = DPL_EINVAL;
              goto end;
            }

          value_len = var->val->string->len;

          if (0 == strcmp(dpl_sbuf_get_str(encoding->val->string), "base64"))
            {
              orig_len = DPL_BASE64_ORIG_LENGTH(value_len);
              orig_buf = malloc(orig_len);
              if (NULL == orig_buf)
                {
                  ret = DPL_ENOMEM;
                  goto end;
                }
          
              orig_len = dpl_base64_decode((u_char *) dpl_sbuf_get_str(var->val->string), value_len, (u_char *) orig_buf);
          
              //swap pointers
              tmp = data_buf;
              data_buf = orig_buf;
              orig_buf = tmp;
              data_len = orig_len;
            }
          else if (0 == strcmp(dpl_sbuf_get_str(encoding->val->string), "utf-8"))
            {
              orig_buf = data_buf;
              orig_len = data_len;
              data_buf = malloc(value_len + 1);
              if (NULL == data_buf)
                {
                  ret = DPL_ENOMEM;
                  goto end;
                }
              memcpy(data_buf, dpl_sbuf_get_str(var->val->string), value_len);
              data_buf[value_len] = '\0';
              data_len = value_len;
            }
          else
            {
              DPL_TRACE(conn->ctx, DPL_TRACE_ERR,
                        "unknown \"valuetransferencoding\" received: \"%s\"",
                        encoding->val->string);
              ret = DPL_EINVAL;
              goto end;
            }
        }
    }

  if (NULL != data_bufp)
    {
      *data_bufp = data_buf;
      data_buf = NULL; //consume it
    }

  if (NULL != data_lenp)
    *data_lenp = data_len;

  ret = DPL_SUCCESS;

 end:

  if (NULL != orig_buf)
    free(orig_buf);

  if (NULL != val)
    dpl_value_free(val);

  if ((option && !(option->mask & DPL_OPTION_NOALLOC)) && NULL != data_buf)
    free(data_buf);

  if (NULL != conn)
    {
      if (1 == connection_close)
        dpl_conn_terminate(conn);
      else
        dpl_conn_release(conn);
    }

  if (NULL != headers_reply)
    dpl_dict_free(headers_reply);

  if (NULL != headers_request)
    dpl_dict_free(headers_request);

  if (NULL != req)
    dpl_req_free(req);

  DPL_TRACE(ctx, DPL_TRACE_BACKEND, "ret=%d", ret);

  return ret;
}

struct get_conven
{
  dpl_dict_t *metadata;
  dpl_sysmd_t *sysmdp;
  dpl_metadatum_func_t metadatum_func;
  dpl_buffer_func_t buffer_func;
  void *cb_arg;
  int http_compat;
  int connection_close;
};

static dpl_status_t
cb_get_header(void *cb_arg,
              const char *header,
              const char *value)
{
  struct get_conven *gc = (struct get_conven *) cb_arg;
  dpl_status_t ret, ret2;

  if (!strcasecmp(header, "connection"))
    {
      if (!strcasecmp(value, "close"))
        gc->connection_close = 1;
    }
  else
    {
      if (gc->http_compat)
        {
          ret2 = dpl_cdmi_get_metadatum_from_header(header,
                                                    value,
                                                    gc->metadatum_func,
                                                    gc->cb_arg,
                                                    gc->metadata,
                                                    gc->sysmdp);
          if (DPL_SUCCESS != ret2)
            {
              ret = ret2;
              goto end;
            }
        }
    }

  ret = DPL_SUCCESS;
  
 end:

  return ret;
}

static dpl_status_t
cb_get_buffer(void *cb_arg,
              char *buf,
              unsigned int len)
{
  struct get_conven *gc = (struct get_conven *) cb_arg;
  int ret;

  if (NULL != gc->buffer_func)
    {
      //XXX transform buffer if JSON

      ret = gc->buffer_func(gc->cb_arg, buf, len);
      if (DPL_SUCCESS != ret)
        return ret;
    }

  return DPL_SUCCESS;
}

/** 
 * get range buffered
 * 
 * @param ctx 
 * @param bucket 
 * @param resource 
 * @param subresource 
 * @param option DPL_OPTION_HTTP_COMPAT is mandatory
 * @param object_type 
 * @param condition 
 * @param range 
 * @param metadatum_func 
 * @param metadatap 
 * @param sysmdp 
 * @param buffer_func 
 * @param cb_arg 
 * @param locationp 
 * 
 * @return 
 */
dpl_status_t
dpl_cdmi_get_buffered(dpl_ctx_t *ctx,
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
                      void *cb_arg,
                      char **locationp)
{
  int           ret, ret2;
  dpl_conn_t   *conn = NULL;
  char          header[dpl_header_size];
  u_int         header_len;
  struct iovec  iov[10];
  int           n_iov = 0;
  dpl_dict_t    *headers_request = NULL;
  dpl_req_t     *req = NULL;
  struct get_conven gc;
  int http_status;
  dpl_cdmi_req_mask_t req_mask = 0u;

  DPL_TRACE(ctx, DPL_TRACE_BACKEND, "");

  memset(&gc, 0, sizeof (gc));
  gc.metadata = dpl_dict_new(13);
  if (NULL == gc.metadata)
    {
      ret = DPL_ENOMEM;
      goto end;
    }
  gc.sysmdp = sysmdp;
  gc.metadatum_func = metadatum_func;
  gc.buffer_func = buffer_func;
  gc.cb_arg = cb_arg;

  if (option)
    {
      if (option->mask & DPL_OPTION_HTTP_COMPAT)
        {
          req_mask |= DPL_CDMI_REQ_HTTP_COMPAT;
          gc.http_compat = 1;
        }
    }

  req = dpl_req_new(ctx);
  if (NULL == req)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  dpl_req_set_method(req, DPL_METHOD_GET);

  if (NULL != bucket)
    {
      ret2 = dpl_req_set_bucket(req, bucket);
      if (DPL_SUCCESS != ret2)
        {
          ret = ret2;
          goto end;
        }
    }

  ret2 = dpl_cdmi_req_set_resource(req, resource);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  if (NULL != subresource)
    {
      ret2 = dpl_req_set_subresource(req, subresource);
      if (DPL_SUCCESS != ret2)
        {
          ret = ret2;
          goto end;
        }
    }

  if (NULL != condition)
    {
      dpl_req_set_condition(req, condition);
    }

  if (!(req_mask & DPL_CDMI_REQ_HTTP_COMPAT))
    {
      ret = DPL_ENOTSUPP;
      goto end;
    }

  //build request
  ret2 = dpl_cdmi_req_build(req, req_mask, &headers_request, NULL, NULL);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  //contact default host
  dpl_req_rm_behavior(req, DPL_BEHAVIOR_VIRTUAL_HOSTING);

  ret2 = dpl_try_connect(ctx, req, &conn);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  ret2 = dpl_add_host_to_headers(req, headers_request);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  ret2 = dpl_req_gen_http_request(ctx, req, headers_request, NULL, header, sizeof (header), &header_len);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  iov[n_iov].iov_base = header;
  iov[n_iov].iov_len = header_len;
  n_iov++;

  //final crlf
  iov[n_iov].iov_base = "\r\n";
  iov[n_iov].iov_len = 2;
  n_iov++;

  ret2 = dpl_conn_writev_all(conn, iov, n_iov, conn->ctx->write_timeout);
  if (DPL_SUCCESS != ret2)
    {
      DPL_TRACE(conn->ctx, DPL_TRACE_ERR, "writev failed");
      gc.connection_close = 1;
      ret = ret2;
      goto end;
    }

  ret2 = dpl_read_http_reply_buffered(conn, 1, &http_status, cb_get_header, cb_get_buffer, &gc);
  if (DPL_SUCCESS != ret2)
    {
      DPL_TRACE(conn->ctx, DPL_TRACE_ERR, "read http answer failed");
      gc.connection_close = 1;
      ret = ret2;
      goto end;
    }

  if (!conn->ctx->keep_alive)
    gc.connection_close = 1;

  if (!gc.http_compat)
    {
      //XXX fetch metadata from overall buffer
      ret = DPL_ENOTSUPP;
      goto end;
    }

  if (NULL != metadatap)
    {
      *metadatap = gc.metadata;
      gc.metadata = NULL;
    }
  
  //map http_status to relevant value
  ret = dpl_map_http_status(http_status);

  //caller is responsible for logging the event

  ret = DPL_SUCCESS;

 end:

  if (NULL != conn)
    {
      if (1 == gc.connection_close)
        dpl_conn_terminate(conn);
      else
        dpl_conn_release(conn);
    }

  if (NULL != gc.metadata)
    dpl_dict_free(gc.metadata);

  if (NULL != headers_request)
    dpl_dict_free(headers_request);

  if (NULL != req)
    dpl_req_free(req);

  DPL_TRACE(ctx, DPL_TRACE_BACKEND, "ret=%d", ret);

  return ret;
}

dpl_status_t
dpl_cdmi_head_raw(dpl_ctx_t *ctx,
                  const char *bucket,
                  const char *resource,
                  const char *subresource,
                  const dpl_option_t *option,
                  dpl_ftype_t object_type,
                  const dpl_condition_t *condition,
                  dpl_dict_t **metadatap,
                  char **locationp)
{
  int ret, ret2;
  char *md_buf = NULL;
  u_int md_len;
  dpl_value_t *val = NULL;
  dpl_option_t option2;

  DPL_TRACE(ctx, DPL_TRACE_BACKEND, "");
  
  //fetch metadata from JSON content
  memset(&option2, 0, sizeof (option2));
  option2.mask |= DPL_OPTION_RAW;
  ret2 = dpl_cdmi_get(ctx, bucket, resource, NULL != subresource ? subresource : "metadata;objectID;parentID;objectType", &option2, object_type, condition, NULL, &md_buf, &md_len, NULL, NULL, locationp);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }
  
  ret2 = dpl_cdmi_parse_json_buffer(ctx, md_buf, md_len, &val);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  if (DPL_VALUE_SUBDICT != val->type)
    {
      ret = DPL_EINVAL;
      goto end;
    }
  
  if (NULL != metadatap)
    {
      *metadatap = val->subdict;
      val->subdict = NULL;
    }
  
  ret = DPL_SUCCESS;
  
 end:

  if (NULL != val)
    dpl_value_free(val);

  if (NULL != md_buf)
    free(md_buf);

  DPL_TRACE(ctx, DPL_TRACE_BACKEND, "ret=%d", ret);
  
  return ret;
}

dpl_status_t
dpl_cdmi_head(dpl_ctx_t *ctx,
              const char *bucket,
              const char *resource,
              const char *subresource,
              const dpl_option_t *option,
              dpl_ftype_t object_type,
              const dpl_condition_t *condition,
              dpl_dict_t **metadatap,
              dpl_sysmd_t *sysmdp,
              char **locationp)
{
  int ret, ret2;
  dpl_dict_t *all_mds = NULL;
  dpl_dict_t *metadata = NULL;

  DPL_TRACE(ctx, DPL_TRACE_BACKEND, "");

  ret2 = dpl_cdmi_head_raw(ctx, bucket, resource, subresource, option, object_type, condition, &all_mds, locationp);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  ret2 = dpl_cdmi_get_metadata_from_values(all_mds, &metadata, sysmdp);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  if (NULL != metadatap)
    {
      *metadatap = metadata;
      metadata = NULL;
    }

  ret = DPL_SUCCESS;
  
 end:

  if (NULL != metadata)
    dpl_dict_free(metadata);

  if (NULL != all_mds)
    dpl_dict_free(all_mds);

  DPL_TRACE(ctx, DPL_TRACE_BACKEND, "ret=%d", ret);

  return ret;
}

dpl_status_t
dpl_cdmi_delete(dpl_ctx_t *ctx,
                const char *bucket,
                const char *resource,
                const char *subresource,
                const dpl_option_t *option,
                dpl_ftype_t object_type,
                const dpl_condition_t *condition,
                char **locationp)
{
  int           ret, ret2;
  dpl_conn_t   *conn = NULL;
  char          header[dpl_header_size];
  u_int         header_len;
  struct iovec  iov[10];
  int           n_iov = 0;
  int           connection_close = 0;
  dpl_dict_t    *headers_request = NULL;
  dpl_dict_t    *headers_reply = NULL;
  dpl_req_t     *req = NULL;
  dpl_cdmi_req_mask_t req_mask = 0u;

  DPL_TRACE(ctx, DPL_TRACE_BACKEND, "");

  if (option)
    {
      if (option->mask & DPL_OPTION_HTTP_COMPAT)
        req_mask |= DPL_CDMI_REQ_HTTP_COMPAT;
    }

  req = dpl_req_new(ctx);
  if (NULL == req)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  dpl_req_set_method(req, DPL_METHOD_DELETE);

  ret2 = dpl_cdmi_req_set_resource(req, resource);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  if (NULL != subresource)
    {
      ret2 = dpl_req_set_subresource(req, subresource);
      if (DPL_SUCCESS != ret2)
        {
          ret = ret2;
          goto end;
        }
    }

#if 0
  if (NULL != condition)
    {
      dpl_req_set_condition(req, condition);
    }
#endif

  //build request
  ret2 = dpl_cdmi_req_build(req, req_mask, &headers_request, NULL, NULL);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  //contact default host
  dpl_req_rm_behavior(req, DPL_BEHAVIOR_VIRTUAL_HOSTING);

  ret2 = dpl_try_connect(ctx, req, &conn);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  ret2 = dpl_add_host_to_headers(req, headers_request);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  ret2 = dpl_req_gen_http_request(ctx, req, headers_request, NULL, header, sizeof (header), &header_len);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  iov[n_iov].iov_base = header;
  iov[n_iov].iov_len = header_len;
  n_iov++;

  //final crlf
  iov[n_iov].iov_base = "\r\n";
  iov[n_iov].iov_len = 2;
  n_iov++;

  ret2 = dpl_conn_writev_all(conn, iov, n_iov, conn->ctx->write_timeout);
  if (DPL_SUCCESS != ret2)
    {
      DPL_TRACE(conn->ctx, DPL_TRACE_ERR, "writev failed");
      connection_close = 1;
      ret = ret2;
      goto end;
    }

  ret2 = dpl_read_http_reply(conn, 1, NULL, NULL, &headers_reply, &connection_close);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  ret = DPL_SUCCESS;

 end:

  if (NULL != conn)
    {
      if (1 == connection_close)
        dpl_conn_terminate(conn);
      else
        dpl_conn_release(conn);
    }

  if (NULL != headers_reply)
    dpl_dict_free(headers_reply);

  if (NULL != headers_request)
    dpl_dict_free(headers_request);

  if (NULL != req)
    dpl_req_free(req);

  DPL_TRACE(ctx, DPL_TRACE_BACKEND, "ret=%d", ret);

  return ret;
}

dpl_status_t
dpl_cdmi_copy(dpl_ctx_t *ctx,
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
              const dpl_condition_t *condition,
              char **locationp)
{
  int           ret, ret2;
  dpl_conn_t   *conn = NULL;
  char          header[dpl_header_size];
  u_int         header_len;
  struct iovec  iov[10];
  int           n_iov = 0;
  int           connection_close = 0;
  dpl_dict_t    *headers_request = NULL;
  dpl_dict_t    *headers_reply = NULL;
  dpl_req_t     *req = NULL;
  char          *body_str = NULL;
  int           body_len = 0;
  int           add_base_path;

  DPL_TRACE(ctx, DPL_TRACE_BACKEND, "");

  if (DPL_COPY_DIRECTIVE_METADATA_REPLACE == copy_directive)
    {
      ret = dpl_cdmi_put(ctx, dst_bucket, dst_resource, NULL, NULL,
                         object_type, condition, NULL, metadata, DPL_CANNED_ACL_UNDEF, NULL, 0, 
                         NULL, NULL, locationp);
      goto end;
    }
      
  req = dpl_req_new(ctx);
  if (NULL == req)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  dpl_req_set_method(req, DPL_METHOD_PUT);

  ret2 = dpl_cdmi_req_set_resource(req, dst_resource);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  if (NULL != dst_subresource)
    {
      ret2 = dpl_req_set_subresource(req, dst_subresource);
      if (DPL_SUCCESS != ret2)
        {
          ret = ret2;
          goto end;
        }
    }

  if (copy_directive == DPL_COPY_DIRECTIVE_MKDENT ||
      copy_directive == DPL_COPY_DIRECTIVE_RMDENT)
    add_base_path = 0;
  else
    add_base_path = 1;

  ret2 = dpl_req_set_src_resource_ext(req, src_resource, add_base_path);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  if (NULL != src_subresource)
    {
      ret2 = dpl_req_set_src_subresource(req, src_subresource);
      if (DPL_SUCCESS != ret2)
        {
          ret = ret2;
          goto end;
        }
    }

  dpl_req_set_copy_directive(req, copy_directive);

  dpl_req_set_object_type(req, object_type);

  dpl_req_add_behavior(req, DPL_BEHAVIOR_MD5);

  if (NULL != sysmd)
    {
      ret2 = dpl_cdmi_add_sysmd_to_req(sysmd, req);
      if (DPL_SUCCESS != ret2)
        {
          ret = ret2;
          goto end;
        }
    }

  if (NULL != metadata)
    {
      ret2 = dpl_cdmi_req_add_metadata(req, metadata, option ? option->mask & DPL_OPTION_APPEND_METADATA : 0);
      if (DPL_SUCCESS != ret2)
        {
          ret = ret2;
          goto end;
        }
    }

  //build request
  ret2 = dpl_cdmi_req_build(req, 0, &headers_request, &body_str, &body_len);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  //contact default host
  dpl_req_rm_behavior(req, DPL_BEHAVIOR_VIRTUAL_HOSTING);

  ret2 = dpl_try_connect(ctx, req, &conn);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  ret2 = dpl_add_host_to_headers(req, headers_request);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  ret2 = dpl_req_gen_http_request(ctx, req, headers_request, NULL, header, sizeof (header), &header_len);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  iov[n_iov].iov_base = header;
  iov[n_iov].iov_len = header_len;
  n_iov++;

  //final crlf
  iov[n_iov].iov_base = "\r\n";
  iov[n_iov].iov_len = 2;
  n_iov++;

  //buffer
  iov[n_iov].iov_base = body_str;
  iov[n_iov].iov_len = body_len;
  n_iov++;

  ret2 = dpl_conn_writev_all(conn, iov, n_iov, conn->ctx->write_timeout);
  if (DPL_SUCCESS != ret2)
    {
      DPL_TRACE(conn->ctx, DPL_TRACE_ERR, "writev failed");
      connection_close = 1;
      ret = ret2;
      goto end;
    }

  ret2 = dpl_read_http_reply(conn, 1, NULL, NULL, &headers_reply, &connection_close);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  ret = DPL_SUCCESS;

 end:

  if (NULL != body_str)
    free(body_str);

  if (NULL != conn)
    {
      if (1 == connection_close)
        dpl_conn_terminate(conn);
      else
        dpl_conn_release(conn);
    }

  if (NULL != headers_reply)
    dpl_dict_free(headers_reply);

  if (NULL != headers_request)
    dpl_dict_free(headers_request);

  if (NULL != req)
    dpl_req_free(req);

  DPL_TRACE(ctx, DPL_TRACE_BACKEND, "ret=%d", ret);

  return ret;
}

dpl_status_t
dpl_cdmi_get_id_path(dpl_ctx_t *ctx,
                     const char *bucket,
                     char **id_pathp)
{
  char *id_path;

  DPL_TRACE(ctx, DPL_TRACE_BACKEND, "");

  id_path = strdup("cdmi_objectid/");
  if (NULL == id_path)
    return DPL_ENOMEM;
  
  *id_pathp = id_path;

  return DPL_SUCCESS;
}

dpl_status_t
dpl_cdmi_convert_id_to_native(dpl_ctx_t *ctx, 
                              const char *id,
                              uint32_t enterprise_number,
                              char **native_idp)
{
  dpl_cdmi_object_id_t obj_id;
  dpl_status_t ret, ret2;
  char opaque[DPL_CDMI_OBJECT_ID_LEN];
  int opaque_len;
  char native_id[DPL_CDMI_OBJECT_ID_LEN];
  char *str = NULL;

  DPL_TRACE(ctx, DPL_TRACE_BACKEND, "");

  ret2 = dpl_cdmi_string_to_opaque(id, opaque, &opaque_len);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  ret2 = dpl_cdmi_object_id_init(&obj_id, enterprise_number, opaque, opaque_len);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  ret2 = dpl_cdmi_object_id_to_string(&obj_id, native_id);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  str = strdup(native_id);
  if (NULL == str)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  if (NULL != native_idp)
    {
      *native_idp = str;
      str = NULL;
    }

  ret = DPL_SUCCESS;
  
 end:

  if (NULL != str)
    free(str);

  DPL_TRACE(ctx, DPL_TRACE_BACKEND, "ret=%d", ret);

  return ret;
}

dpl_status_t
dpl_cdmi_convert_native_to_id(dpl_ctx_t *ctx, 
                              const char *native_id,
                              char **idp, 
                              uint32_t *enterprise_numberp)
{
  dpl_status_t ret, ret2;
  dpl_cdmi_object_id_t obj_id;
  char id[DPL_CDMI_OBJECT_ID_LEN]; //at least
  char *str = NULL;

  DPL_TRACE(ctx, DPL_TRACE_BACKEND, "");

  ret2 = dpl_cdmi_string_to_object_id(native_id, &obj_id);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  ret2 = dpl_cdmi_opaque_to_string(&obj_id, id);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  str = strdup(id);
  if (NULL == str)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  if (NULL != idp)
    {
      *idp = str;
      str = NULL;
    }

  if (NULL != enterprise_numberp)
    *enterprise_numberp = obj_id.enterprise_number;
  
  ret = DPL_SUCCESS;

 end:

  if (NULL != str)
    free(str);

  DPL_TRACE(ctx, DPL_TRACE_BACKEND, "ret=%d", ret);

  return ret;
}

dpl_status_t
dpl_cdmi_post_id(dpl_ctx_t *ctx,
                 const char *bucket,
                 const char *resource,
                 const char *subresource,
                 const dpl_option_t *option,
                 dpl_ftype_t object_type,
                 const dpl_condition_t *condition,
                 const dpl_range_t *range,
                 const dpl_dict_t *metadata,
                 const dpl_sysmd_t *sysmd,
                 const char *data_buf,
                 unsigned int data_len,
                 const dpl_dict_t *query_params,
                 dpl_sysmd_t *returned_sysmdp,
                 char **locationp)
{
  dpl_status_t ret, ret2;
  char *id_path = NULL;

  DPL_TRACE(ctx, DPL_TRACE_ID, "post_id bucket=%s subresource=%s", bucket, subresource);

  ret2 = dpl_cdmi_get_id_path(ctx, bucket, &id_path);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  ret2 = dpl_cdmi_post(ctx, bucket, id_path, subresource, option, object_type, condition, range, metadata, sysmd, data_buf, data_len, query_params, returned_sysmdp, NULL);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  ret = DPL_SUCCESS;
  
 end:

  if (NULL != id_path)
    free(id_path);

  DPL_TRACE(ctx, DPL_TRACE_ID, "ret=%d", ret);

  return ret;
}

dpl_status_t
dpl_cdmi_post_id_buffered(dpl_ctx_t *ctx,
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
                          const dpl_dict_t *query_params,
                          dpl_conn_t **connp,
                          char **locationp)
{
  dpl_status_t ret;
  char *id_path = NULL;

  DPL_TRACE(ctx, DPL_TRACE_ID, "post_buffered_id bucket=%s subresource=%s", bucket, subresource);

  ret = dpl_cdmi_get_id_path(ctx, bucket, &id_path);
  if (DPL_SUCCESS != ret)
    {
      goto end;
    }

  ret = dpl_cdmi_post_buffered(ctx, bucket, id_path, subresource, option, object_type, condition, range, metadata, sysmd, data_len, query_params, connp, locationp);
  
 end:

  if (NULL != id_path)
    free(id_path);

  DPL_TRACE(ctx, DPL_TRACE_ID, "ret=%d", ret);
  
  return ret;
}

dpl_status_t
dpl_cdmi_put_id(dpl_ctx_t *ctx,
                const char *bucket,
                const char *id,
                const char *subresource,
                const dpl_option_t *option,
                dpl_ftype_t object_type,
                const dpl_condition_t *condition,
                const dpl_range_t *range,
                const dpl_dict_t *metadata,
                const dpl_sysmd_t *sysmd,
                const char *data_buf,
                unsigned int data_len,
                const dpl_dict_t *query_params,
                dpl_sysmd_t *returned_sysmdp,
                char **locationp)
{
  dpl_status_t ret, ret2;
  char *id_path = NULL;
  char resource[DPL_MAXPATHLEN];
  char *native_id = NULL;

  DPL_TRACE(ctx, DPL_TRACE_ID, "put_id bucket=%s id=%s subresource=%s", bucket, id, subresource);

  ret = dpl_cdmi_get_id_path(ctx, bucket, &id_path);
  if (DPL_SUCCESS != ret)
    {
      goto end;
    }

  ret2 = dpl_cdmi_convert_id_to_native(ctx, id, ctx->enterprise_number, &native_id);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  snprintf(resource, sizeof (resource), "%s%s", id_path ? id_path : "", native_id);

  ret = dpl_cdmi_put(ctx, bucket, resource, subresource, option, object_type, condition,
                     range, metadata, sysmd, data_buf, data_len, query_params, returned_sysmdp, locationp);

 end:

  if (NULL != native_id)
    free(native_id);

  if (NULL != id_path)
    free(id_path);

  DPL_TRACE(ctx, DPL_TRACE_ID, "ret=%d", ret);
  
  return ret;
}

/** 
 * put range buffered
 *
 * @param ctx 
 * @param bucket 
 * @param resource 
 * @param subresource 
 * @param option DPL_OPTION_HTTP_COMPAT is mandatory
 * @param object_type 
 * @param range 
 * @param metadata 
 * @param sysmd 
 * @param data_len 
 * @param connp 
 * @param locationp 
 * 
 * @return 
 */
dpl_status_t
dpl_cdmi_put_id_buffered(dpl_ctx_t *ctx,
                         const char *bucket,
                         const char *id,
                         const char *subresource,
                         const dpl_option_t *option,
                         dpl_ftype_t object_type,
                         const dpl_condition_t *condition,
                         const dpl_range_t *range,
                         const dpl_dict_t *metadata,
                         const dpl_sysmd_t *sysmd,
                         unsigned int data_len,
                         const dpl_dict_t *query_params,
                         dpl_conn_t **connp,
                         char **locationp)
{
  dpl_status_t ret, ret2;
  char *id_path = NULL;
  char resource[DPL_MAXPATHLEN];
  char *native_id = NULL;

  DPL_TRACE(ctx, DPL_TRACE_ID, "put_id_buffered bucket=%s id=%s subresource=%s", bucket, id, subresource);

  ret = dpl_cdmi_get_id_path(ctx, bucket, &id_path);
  if (DPL_SUCCESS != ret)
    {
      goto end;
    }

  ret2 = dpl_cdmi_convert_id_to_native(ctx, id, ctx->enterprise_number, &native_id);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  snprintf(resource, sizeof (resource), "%s%s", id_path ? id_path : "", native_id);

  ret = dpl_cdmi_put_buffered(ctx, bucket, resource, subresource, option, object_type, condition,
                              range, metadata, sysmd, data_len, query_params, connp, locationp);

 end:

  if (NULL != native_id)
    free(native_id);

  if (NULL != id_path)
    free(id_path);

  DPL_TRACE(ctx, DPL_TRACE_ID, "ret=%d", ret);
  
  return ret;
}

dpl_status_t
dpl_cdmi_get_id(dpl_ctx_t *ctx,
                const char *bucket,
                const char *id,
                const char *subresource,
                const dpl_option_t *option,
                dpl_ftype_t object_type,
                const dpl_condition_t *condition,
                const dpl_range_t *range,
                char **data_bufp,
                unsigned int *data_lenp,
                dpl_dict_t **metadatap,
                dpl_sysmd_t *sysmdp,
                char **locationp)
{
  dpl_status_t ret, ret2;
  char *id_path = NULL;
  char resource[DPL_MAXPATHLEN];
  char *native_id = NULL;

  DPL_TRACE(ctx, DPL_TRACE_ID, "get_id bucket=%s id=%s subresource=%s", bucket, id, subresource);

  ret = dpl_cdmi_get_id_path(ctx, bucket, &id_path);
  if (DPL_SUCCESS != ret)
    {
      goto end;
    }

  ret2 = dpl_cdmi_convert_id_to_native(ctx, id, ctx->enterprise_number, &native_id);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  snprintf(resource, sizeof (resource), "%s%s", id_path ? id_path : "", native_id);

  ret = dpl_cdmi_get(ctx, bucket, resource, subresource, option, object_type, condition, range, data_bufp, data_lenp, metadatap, sysmdp, locationp);
  
 end:

  if (NULL != native_id)
    free(native_id);

  if (NULL != id_path)
    free(id_path);

  DPL_TRACE(ctx, DPL_TRACE_ID, "ret=%d", ret);
  
  return ret;
}

dpl_status_t 
dpl_cdmi_get_id_buffered(dpl_ctx_t *ctx,
                         const char *bucket,
                         const char *id,
                         const char *subresource, 
                         const dpl_option_t *option,
                         dpl_ftype_t object_type,
                         const dpl_condition_t *condition,
                         const dpl_range_t *range,
                         dpl_metadatum_func_t metadatum_func,
                         dpl_dict_t **metadatap,
                         dpl_sysmd_t *sysmdp, 
                         dpl_buffer_func_t buffer_func,
                         void *cb_arg,
                         char **locationp)
{
  dpl_status_t ret, ret2;
  char *id_path = NULL;
  char resource[DPL_MAXPATHLEN];
  char *native_id = NULL;

  DPL_TRACE(ctx, DPL_TRACE_ID, "get_buffered_id bucket=%s id=%s subresource=%s", bucket, id, subresource);

  ret = dpl_cdmi_get_id_path(ctx, bucket, &id_path);
  if (DPL_SUCCESS != ret)
    {
      goto end;
    }

  ret2 = dpl_cdmi_convert_id_to_native(ctx, id, ctx->enterprise_number, &native_id);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  snprintf(resource, sizeof (resource), "%s%s", id_path ? id_path : "", native_id);

  ret = dpl_cdmi_get_buffered(ctx, bucket, resource, subresource, option, object_type, condition, range, metadatum_func, metadatap, sysmdp, buffer_func, cb_arg, locationp);
  
 end:

  if (NULL != native_id)
    free(native_id);

  if (NULL != id_path)
    free(id_path);

  DPL_TRACE(ctx, DPL_TRACE_ID, "ret=%d", ret);
  
  return ret;
}

dpl_status_t
dpl_cdmi_head_id(dpl_ctx_t *ctx,
                 const char *bucket,
                 const char *id,
                 const char *subresource,
                 const dpl_option_t *option,
                 dpl_ftype_t object_type,
                 const dpl_condition_t *condition,
                 dpl_dict_t **metadatap,
                 dpl_sysmd_t *sysmdp,
                 char **locationp)
{
  dpl_status_t ret, ret2;
  char *id_path = NULL;
  char resource[DPL_MAXPATHLEN];
  char *native_id = NULL;

  DPL_TRACE(ctx, DPL_TRACE_ID, "head_id bucket=%s id=%s subresource=%s", bucket, id, subresource);

  ret = dpl_cdmi_get_id_path(ctx, bucket, &id_path);
  if (DPL_SUCCESS != ret)
    {
      goto end;
    }

  ret2 = dpl_cdmi_convert_id_to_native(ctx, id, ctx->enterprise_number, &native_id);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  snprintf(resource, sizeof (resource), "%s%s", id_path ? id_path : "", native_id);

  ret = dpl_cdmi_head(ctx, bucket, resource, subresource, option, object_type, condition, metadatap, sysmdp, locationp);
  
 end:

  if (NULL != native_id)
    free(native_id);

  if (NULL != id_path)
    free(id_path);

  DPL_TRACE(ctx, DPL_TRACE_ID, "ret=%d", ret);
  
  return ret;
}

dpl_status_t
dpl_cdmi_head_id_raw(dpl_ctx_t *ctx,
                     const char *bucket,
                     const char *id,
                     const char *subresource,
                     const dpl_option_t *option,
                     dpl_ftype_t object_type,
                     const dpl_condition_t *condition,
                     dpl_dict_t **metadatap,
                     char **locationp)
{
  dpl_status_t ret, ret2;
  char *id_path = NULL;
  char resource[DPL_MAXPATHLEN];
  char *native_id = NULL;

  DPL_TRACE(ctx, DPL_TRACE_ID, "head_raw_id bucket=%s id=%s subresource=%s", bucket, id, subresource);

  ret = dpl_cdmi_get_id_path(ctx, bucket, &id_path);
  if (DPL_SUCCESS != ret)
    {
      goto end;
    }

  ret2 = dpl_cdmi_convert_id_to_native(ctx, id, ctx->enterprise_number, &native_id);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  snprintf(resource, sizeof (resource), "%s%s", id_path ? id_path : "", native_id);

  ret = dpl_cdmi_head_raw(ctx, bucket, resource, subresource, option, object_type, condition, metadatap, locationp);
  
 end:

  if (NULL != native_id)
    free(native_id);

  if (NULL != id_path)
    free(id_path);

  DPL_TRACE(ctx, DPL_TRACE_ID, "ret=%d", ret);
  
  return ret;
}

dpl_status_t
dpl_cdmi_delete_id(dpl_ctx_t *ctx,
                   const char *bucket,
                   const char *id,
                   const char *subresource,
                   const dpl_option_t *option,
                   dpl_ftype_t object_type,
                   const dpl_condition_t *condition, 
                   char **locationp)
{
  dpl_status_t ret, ret2;
  char *id_path = NULL;
  char resource[DPL_MAXPATHLEN];
  char *native_id = NULL;

  DPL_TRACE(ctx, DPL_TRACE_ID, "delete bucket=%s id=%s subresource=%s", bucket, id, subresource);

  ret = dpl_cdmi_get_id_path(ctx, bucket, &id_path);
  if (DPL_SUCCESS != ret)
    {
      goto end;
    }

  ret2 = dpl_cdmi_convert_id_to_native(ctx, id, ctx->enterprise_number, &native_id);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  snprintf(resource, sizeof (resource), "%s%s", id_path ? id_path : "", native_id);

  ret = dpl_cdmi_delete(ctx, bucket, resource, subresource, option, object_type, condition, locationp);
  
 end:

  if (NULL != native_id)
    free(native_id);

  if (NULL != id_path)
    free(id_path);

  DPL_TRACE(ctx, DPL_TRACE_ID, "ret=%d", ret);
  
  return ret;
}

dpl_status_t
dpl_cdmi_copy_id(dpl_ctx_t *ctx,
                 const char *src_bucket,
                 const char *src_id,
                 const char *src_subresource,
                 const char *dst_bucket,
                 const char *dst_resource,
                 const char *dst_subresource,
                 const dpl_option_t *option,
                 dpl_ftype_t object_type,
                 dpl_copy_directive_t copy_directive,
                 const dpl_dict_t *metadata,
                 const dpl_sysmd_t *sysmd,
                 const dpl_condition_t *condition,
                 char **locationp)
{
  dpl_status_t ret, ret2;
  char *id_path = NULL;
  char src_resource[DPL_MAXPATHLEN];
  char *src_native_id = NULL;

  DPL_TRACE(ctx, DPL_TRACE_ID, "delete src_bucket=%s src_id=%s src_subresource=%s", src_bucket, src_id, src_subresource);

  if (DPL_COPY_DIRECTIVE_MKDENT == copy_directive)
    id_path = NULL;
  else
    {
      ret = dpl_cdmi_get_id_path(ctx, src_bucket, &id_path);
      if (DPL_SUCCESS != ret)
        {
          goto end;
        }
    }

  ret2 = dpl_cdmi_convert_id_to_native(ctx, src_id, ctx->enterprise_number, &src_native_id);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  snprintf(src_resource, sizeof (src_resource), "%s%s", id_path ? id_path : "", src_native_id);

  if (DPL_COPY_DIRECTIVE_METADATA_REPLACE == copy_directive)
    ret = dpl_cdmi_put(ctx, src_bucket, src_resource, NULL, NULL,
                       object_type, condition, NULL, metadata, DPL_CANNED_ACL_UNDEF, NULL, 0, 
                       NULL, NULL, locationp);
  else
    ret = dpl_cdmi_copy(ctx, src_bucket, src_resource, src_subresource, dst_bucket, dst_resource, dst_subresource,
                        option, object_type, copy_directive, metadata, sysmd, condition, locationp);
  
 end:

  if (NULL != src_native_id)
    free(src_native_id);

  if (NULL != id_path)
    free(id_path);

  DPL_TRACE(ctx, DPL_TRACE_ID, "ret=%d", ret);
  
  return ret;
}

