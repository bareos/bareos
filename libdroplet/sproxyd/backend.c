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
#include <droplet/sproxyd/sproxyd.h>
#include <droplet/uks/uks.h>
#include <sys/param.h>

//#define DPRINTF(fmt,...) fprintf(stderr, fmt, ##__VA_ARGS__)
#define DPRINTF(fmt,...)

dpl_status_t
dpl_sproxyd_get_capabilities(dpl_ctx_t *ctx,
                             dpl_capability_t *maskp)
{
  if (NULL != maskp)
    *maskp = DPL_CAP_IDS|
      DPL_CAP_CONSISTENCY|
      DPL_CAP_VERSIONING|
      DPL_CAP_CONDITIONS|
      DPL_CAP_PUT_RANGE;
  
  return DPL_SUCCESS;
}

dpl_status_t
dpl_sproxyd_get_id_scheme(dpl_ctx_t *ctx,
                          dpl_id_scheme_t **id_schemep)
{
  if (NULL != id_schemep)
    *id_schemep = &dpl_id_scheme_uks;
  
  return DPL_SUCCESS;
}

dpl_status_t
dpl_sproxyd_put_internal(dpl_ctx_t *ctx,
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
                         int mdonly,
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
  dpl_sproxyd_req_mask_t req_mask = 0u;
  dpl_dict_t    *query_params = NULL;
  uint32_t      force_version = -1;

  DPL_TRACE(ctx, DPL_TRACE_BACKEND, "");

  req = dpl_req_new(ctx);
  if (NULL == req)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  dpl_req_set_method(req, DPL_METHOD_PUT);

  ret2 = dpl_req_set_resource(req, resource);
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

  dpl_req_set_object_type(req, object_type);

  if (NULL != condition)
    {
      dpl_req_set_condition(req, condition);
    }

  if (option)
    {
      if (option->mask & DPL_OPTION_CONSISTENT)
        req_mask |= DPL_SPROXYD_REQ_CONSISTENT;

      if (option->mask & DPL_OPTION_EXPECT_VERSION)
        {
          req_mask = DPL_SPROXYD_REQ_EXPECT_VERSION;

          query_params = dpl_dict_new(13);
          if (NULL == query_params)
            {
              ret = DPL_ENOMEM;
              goto end;
            }

          ret2 = dpl_dict_add(query_params, "version", option->expect_version, 0);
          if (DPL_SUCCESS != ret2)
            {
              ret = ret2;
              goto end;
            }
        }

      if (option->mask & DPL_OPTION_FORCE_VERSION)
        {
          req_mask = DPL_SPROXYD_REQ_FORCE_VERSION;
          
          force_version = strtoul(option->force_version, NULL, 0);
        }
    }

  if (mdonly)
    {
      req_mask |= DPL_SPROXYD_REQ_MD_ONLY;
    }
  else
    {
      dpl_req_set_data(req, data_buf, data_len);
    }

  dpl_req_add_behavior(req, DPL_BEHAVIOR_MD5);

  if (NULL != metadata)
    {
      ret2 = dpl_req_add_metadata(req, metadata);
      if (DPL_SUCCESS != ret2)
        {
          ret = ret2;
          goto end;
        }
    }

  //build request
  ret2 = dpl_sproxyd_req_build(req, req_mask, force_version, &headers_request);
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

  ret2 = dpl_req_gen_http_request(ctx, req, headers_request, query_params, header, sizeof (header), &header_len);
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
  iov[n_iov].iov_base = (void *)data_buf;
  iov[n_iov].iov_len = data_len;
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

  if (NULL != query_params)
    dpl_dict_free(query_params);

  if (NULL != req)
    dpl_req_free(req);

  DPL_TRACE(ctx, DPL_TRACE_BACKEND, "ret=%d", ret);

  return ret;
}

dpl_status_t
dpl_sproxyd_put_id(dpl_ctx_t *ctx,
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
  return dpl_sproxyd_put_internal(ctx, bucket, resource, subresource, option,
                                  object_type, condition, range,
                                  metadata, sysmd, data_buf, data_len, 0, locationp);
}

dpl_status_t
dpl_sproxyd_get_id(dpl_ctx_t *ctx,
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
  dpl_sproxyd_req_mask_t req_mask = 0u;

  DPL_TRACE(ctx, DPL_TRACE_BACKEND, "");

  req = dpl_req_new(ctx);
  if (NULL == req)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  dpl_req_set_method(req, DPL_METHOD_GET);

  ret2 = dpl_req_set_resource(req, resource);
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
      ret2 = dpl_req_add_range(req, range->start, range->end);
      if (DPL_SUCCESS != ret2)
	{
	  ret = ret2;
	  goto end;
	}
    }

  if (option)
    {
      if (option->mask & DPL_OPTION_CONSISTENT)
        req_mask |= DPL_SPROXYD_REQ_CONSISTENT;
    }

  dpl_req_set_object_type(req, object_type);

  //build request
  ret2 = dpl_sproxyd_req_build(req, req_mask, -1, &headers_request);
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
      ret = ret2;
      goto end;
    }

  ret2 = dpl_sproxyd_get_metadata_from_headers(headers_reply, metadatap, sysmdp);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
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
      ret2 = dpl_sproxyd_get_metadatum_from_header(header,
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
      ret = gc->buffer_func(gc->cb_arg, buf, len);
      if (DPL_SUCCESS != ret)
        return ret;
    }

  return DPL_SUCCESS;
}

dpl_status_t 
dpl_sproxyd_head_id_raw(dpl_ctx_t *ctx, 
                        const char *bucket, 
                        const char *resource,
                        const char *subresource,
                        const dpl_option_t *option,
                        dpl_ftype_t object_type,
                        const dpl_condition_t *condition,
                        dpl_dict_t **metadatap,
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
  dpl_sproxyd_req_mask_t req_mask = 0u;

  DPL_TRACE(ctx, DPL_TRACE_BACKEND, "");

  req = dpl_req_new(ctx);
  if (NULL == req)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  dpl_req_set_method(req, DPL_METHOD_HEAD);

  ret2 = dpl_req_set_resource(req, resource);
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

  if (option)
    {
      if (option->mask & DPL_OPTION_CONSISTENT)
        req_mask |= DPL_SPROXYD_REQ_CONSISTENT;
    }

  //build request
  ret2 = dpl_sproxyd_req_build(req, req_mask, -1, &headers_request);
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

  ret2 = dpl_read_http_reply(conn, 0, NULL, NULL, &headers_reply, &connection_close);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  if (NULL != metadatap)
    {
      *metadatap = headers_reply;
      headers_reply = NULL;
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
dpl_sproxyd_head_id(dpl_ctx_t *ctx,
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
  dpl_status_t ret, ret2;
  dpl_dict_t *headers_reply = NULL;

  DPL_TRACE(ctx, DPL_TRACE_BACKEND, "");

  ret2 = dpl_sproxyd_head_id_raw(ctx, bucket, resource, subresource, option,
                                 object_type, condition, 
                                 &headers_reply, locationp);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  ret2 = dpl_sproxyd_get_metadata_from_headers(headers_reply, metadatap, sysmdp);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  ret = DPL_SUCCESS;
  
 end:

  if (NULL != headers_reply)
    dpl_dict_free(headers_reply);

  DPL_TRACE(ctx, DPL_TRACE_BACKEND, "ret=%d", ret);

  return ret;
}

dpl_status_t
dpl_sproxyd_delete_id(dpl_ctx_t *ctx,
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
  dpl_sproxyd_req_mask_t req_mask = 0u;
  dpl_dict_t    *query_params = NULL;
  uint32_t      force_version = -1;

  DPL_TRACE(ctx, DPL_TRACE_BACKEND, "");

  req = dpl_req_new(ctx);
  if (NULL == req)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  dpl_req_set_method(req, DPL_METHOD_DELETE);

  ret2 = dpl_req_set_resource(req, resource);
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

  if (option)
    {
      if (option->mask & DPL_OPTION_CONSISTENT)
        req_mask |= DPL_SPROXYD_REQ_CONSISTENT;
      
      if (option->mask & DPL_OPTION_EXPECT_VERSION)
        {
          req_mask = DPL_SPROXYD_REQ_EXPECT_VERSION;

          query_params = dpl_dict_new(13);
          if (NULL == query_params)
            {
              ret = DPL_ENOMEM;
              goto end;
            }

          ret2 = dpl_dict_add(query_params, "version", option->expect_version, 0);
          if (DPL_SUCCESS != ret2)
            {
              ret = ret2;
              goto end;
            }
        }

      if (option->mask & DPL_OPTION_FORCE_VERSION)
        {
          req_mask = DPL_SPROXYD_REQ_FORCE_VERSION;
          
          force_version = strtoul(option->force_version, NULL, 0);
        }
    }

  //build request
  ret2 = dpl_sproxyd_req_build(req, req_mask, force_version, &headers_request);
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

  if (NULL != query_params)
    dpl_dict_free(query_params);

  if (NULL != req)
    dpl_req_free(req);

  DPL_TRACE(ctx, DPL_TRACE_BACKEND, "ret=%d", ret);

  return ret;
}

dpl_status_t
dpl_sproxyd_copy_id(dpl_ctx_t *ctx,
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
  int ret, ret2;

  DPL_TRACE(ctx, DPL_TRACE_BACKEND, "");

  switch (copy_directive)
    {
    case DPL_COPY_DIRECTIVE_UNDEF:
      ret = DPL_ENOTSUPP;
      goto end;
    case DPL_COPY_DIRECTIVE_COPY:
      ret = DPL_ENOTSUPP;
      goto end;
    case DPL_COPY_DIRECTIVE_METADATA_REPLACE:

      //replace the metadata
      ret2 = dpl_sproxyd_put_internal(ctx, dst_bucket, dst_resource,
                                      dst_subresource, option, object_type, condition, NULL, metadata, 
                                      DPL_CANNED_ACL_UNDEF, NULL, 0, 1, locationp);
      if (DPL_SUCCESS != ret2)
        {
          ret = ret2;
          goto end;
        }

      break ;
    case DPL_COPY_DIRECTIVE_LINK:
    case DPL_COPY_DIRECTIVE_SYMLINK:
    case DPL_COPY_DIRECTIVE_MOVE:
    case DPL_COPY_DIRECTIVE_MKDENT:
    case DPL_COPY_DIRECTIVE_RMDENT:
    case DPL_COPY_DIRECTIVE_MVDENT:
      ret = DPL_ENOTSUPP;
      goto end;
    }

  ret = DPL_SUCCESS;
  
 end:

  DPL_TRACE(ctx, DPL_TRACE_BACKEND, "ret=%d", ret);

  return ret;
}

dpl_backend_t
dpl_backend_sproxyd = 
  {
    "sproxyd",
    .get_capabilities   = dpl_sproxyd_get_capabilities,
    .get_id_scheme      = dpl_sproxyd_get_id_scheme,
    .put_id 		= dpl_sproxyd_put_id,
    .get_id 		= dpl_sproxyd_get_id,
    .head_id 		= dpl_sproxyd_head_id,
    .head_id_raw	= dpl_sproxyd_head_id_raw,
    .delete_id 		= dpl_sproxyd_delete_id,
    .copy_id            = dpl_sproxyd_copy_id,
  };
