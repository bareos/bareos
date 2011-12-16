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

//#define DPRINTF(fmt,...) fprintf(stderr, fmt, ##__VA_ARGS__)
#define DPRINTF(fmt,...)

dpl_status_t
dpl_cdmi_make_bucket(dpl_ctx_t *ctx,
                     const char *bucket,
                     const dpl_sysmd_t *sysmd)
{
  return dpl_mkdir(ctx, bucket);
}

dpl_status_t
dpl_cdmi_list_bucket(dpl_ctx_t *ctx,
                     const char *bucket,
                     const char *prefix,
                     const char *delimiter,
                     dpl_vec_t **objectsp,
                     dpl_vec_t **common_prefixesp)
{
  char          *host;
  int           ret, ret2;
  dpl_conn_t   *conn = NULL;
  char          header[1024];
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

  req = dpl_req_new(ctx);
  if (NULL == req)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  dpl_req_set_method(req, DPL_METHOD_GET);

  ret2 = dpl_req_set_resource(req, NULL != prefix ? prefix : "/");
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  //contact default host
  dpl_req_rm_behavior(req, DPL_BEHAVIOR_VIRTUAL_HOSTING);

  dpl_req_set_object_type(req, DPL_FTYPE_DIR);

  //build request
  ret2 = dpl_cdmi_req_build(req, &headers_request, NULL, NULL);
  if (DPL_SUCCESS != ret2)
    {
      ret = DPL_FAILURE;
      goto end;
    }

  host = dpl_dict_get_value(headers_request, "Host");
  if (NULL == host)
    {
      ret = DPL_FAILURE;
      goto end;
    }

  conn = dpl_conn_open_host(ctx, host, ctx->port);
  if (NULL == conn)
    {
      ret = DPL_FAILURE;
      goto end;
    }

  //bucket emulation
  ret2 = dpl_dict_add(headers_request, "X-Scality-Bucket", bucket, 0);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  ret2 = dpl_req_gen_http_request(ctx, req, headers_request, NULL, header, sizeof (header), &header_len);
  if (DPL_SUCCESS != ret2)
    {
      ret = DPL_FAILURE;
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
      DPLERR(1, "writev failed");
      connection_close = 1;
      ret = DPL_ENOENT; //mapped to 404
      goto end;
    }

  ret2 = dpl_read_http_reply(conn, 1, &data_buf, &data_len, &headers_reply);
  if (DPL_SUCCESS != ret2)
    {
      if (DPL_ENOENT == ret2)
        {
          ret = DPL_ENOENT;
          goto end;
        }
      else
        {
          DPLERR(0, "read http answer failed");
          connection_close = 1;
          ret = DPL_ENOENT; //mapped to 404
          goto end;
        }
    }
  else
    {
      connection_close = dpl_connection_close(ctx, headers_reply);
    }

  (void) dpl_log_event(ctx, "DATA", "OUT", data_len);

  objects = dpl_vec_new(2, 2);
  if (NULL == objects)
    {
      ret = DPL_FAILURE;
      goto end;
    }

  common_prefixes = dpl_vec_new(2, 2);
  if (NULL == common_prefixes)
    {
      ret = DPL_FAILURE;
      goto end;
    }

  ret = dpl_cdmi_parse_list_bucket(ctx, data_buf, data_len, prefix, objects, common_prefixes);
  if (DPL_SUCCESS != ret)
    {
      ret = DPL_FAILURE;
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

  DPRINTF("ret=%d\n", ret);

  return ret;
}

dpl_status_t
dpl_cdmi_post(dpl_ctx_t *ctx,
              const char *bucket,
              const char *resource,
              const char *subresource,
              dpl_ftype_t object_type,
              const dpl_dict_t *metadata,
              const dpl_sysmd_t *sysmd,
              const char *data_buf,
              unsigned int data_len,
              const dpl_dict_t *query_params,
              char **resource_idp)
{
  char          *host;
  int           ret, ret2;
  dpl_conn_t   *conn = NULL;
  char          header[1024];
  u_int         header_len;
  struct iovec  iov[10];
  int           n_iov = 0;
  int           connection_close = 0;
  dpl_dict_t    *headers_request = NULL;
  dpl_dict_t    *headers_reply = NULL;
  dpl_req_t     *req = NULL;
  dpl_chunk_t   chunk;
  char          *body_str = NULL;
  int           body_len = 0;

  req = dpl_req_new(ctx);
  if (NULL == req)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  dpl_req_set_method(req, DPL_METHOD_POST);

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

  //contact default host
  dpl_req_rm_behavior(req, DPL_BEHAVIOR_VIRTUAL_HOSTING);

  dpl_req_set_object_type(req, object_type);

  if (NULL != data_buf)
    {
      chunk.buf = data_buf;
      chunk.len = data_len;
      dpl_req_set_chunk(req, &chunk);
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
  ret2 = dpl_cdmi_req_build(req, &headers_request, &body_str, &body_len);
  if (DPL_SUCCESS != ret2)
    {
      ret = DPL_FAILURE;
      goto end;
    }

  host = dpl_dict_get_value(headers_request, "Host");
  if (NULL == host)
    {
      ret = DPL_FAILURE;
      goto end;
    }

  conn = dpl_conn_open_host(ctx, host, ctx->port);
  if (NULL == conn)
    {
      ret = DPL_FAILURE;
      goto end;
    }

  //bucket emulation
  ret2 = dpl_dict_add(headers_request, "X-Scality-Bucket", bucket, 0);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  ret2 = dpl_req_gen_http_request(ctx, req, headers_request, NULL, header, sizeof (header), &header_len);
  if (DPL_SUCCESS != ret2)
    {
      ret = DPL_FAILURE;
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
      DPLERR(1, "writev failed");
      connection_close = 1;
      ret = DPL_ENOENT; //mapped to 404
      goto end;
    }

  ret2 = dpl_read_http_reply(conn, 1, NULL, NULL, &headers_reply);
  if (DPL_SUCCESS != ret2)
    {
      if (DPL_ENOENT == ret2)
        {
          ret = DPL_ENOENT;
          goto end;
        }
      else
        {
          DPLERR(0, "read http answer failed");
          connection_close = 1;
          ret = DPL_ENOENT; //mapped to 404
          goto end;
        }
    }
  else
    {
      connection_close = dpl_connection_close(ctx, headers_reply);
    }

  (void) dpl_log_event(ctx, "DATA", "IN", data_len);

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

  DPRINTF("ret=%d\n", ret);

  return ret;
}

dpl_status_t
dpl_cdmi_post_buffered(dpl_ctx_t *ctx,
                       const char *bucket,
                       const char *resource,
                       const char *subresource,
                       dpl_ftype_t object_type,
                       const dpl_dict_t *metadata,
                       const dpl_sysmd_t *sysmd,
                       unsigned int data_len,
                       const dpl_dict_t *query_params,
                       dpl_conn_t **connp)
{
  char          *host;
  int           ret, ret2;
  dpl_conn_t    *conn = NULL;
  char          header[1024];
  u_int         header_len;
  struct iovec  iov[10];
  int           n_iov = 0;
  int           connection_close = 0;
  dpl_dict_t    *headers_request = NULL;
  dpl_dict_t    *headers_reply = NULL;
  dpl_req_t     *req = NULL;
  dpl_chunk_t   chunk;

  req = dpl_req_new(ctx);
  if (NULL == req)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  dpl_req_set_method(req, DPL_METHOD_POST);

  if (NULL != bucket)
    {
      ret2 = dpl_req_set_bucket(req, bucket);
      if (DPL_SUCCESS != ret2)
        {
          ret = ret2;
          goto end;
        }
    }

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

  chunk.buf = NULL;
  chunk.len = data_len;
  dpl_req_set_chunk(req, &chunk);

  //contact default host
  dpl_req_rm_behavior(req, DPL_BEHAVIOR_VIRTUAL_HOSTING);

  dpl_req_add_behavior(req, DPL_BEHAVIOR_HTTP_COMPAT);

  dpl_req_set_object_type(req, object_type);

  dpl_req_add_behavior(req, DPL_BEHAVIOR_EXPECT);

  if (NULL != metadata)
    {
      ret2 = dpl_req_add_metadata(req, metadata);
      if (DPL_SUCCESS != ret2)
        {
          ret = ret2;
          goto end;
        }
    }

  ret2 = dpl_cdmi_req_build(req, &headers_request, NULL, NULL);
  if (DPL_SUCCESS != ret2)
    {
      ret = DPL_FAILURE;
      goto end;
    }

  host = dpl_dict_get_value(headers_request, "Host");
  if (NULL == host)
    {
      ret = DPL_FAILURE;
      goto end;
    }

  conn = dpl_conn_open_host(ctx, host, ctx->port);
  if (NULL == conn)
    {
      ret = DPL_FAILURE;
      goto end;
    }

  //bucket emulation
  ret2 = dpl_dict_add(headers_request, "X-Scality-Bucket", bucket, 0);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  ret2 = dpl_req_gen_http_request(ctx, req, headers_request, NULL, header, sizeof (header), &header_len);
  if (DPL_SUCCESS != ret2)
    {
      ret = DPL_FAILURE;
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
      DPLERR(1, "writev failed");
      connection_close = 1;
      ret = DPL_ENOENT; //mapped to 404
      goto end;
    }

  ret2 = dpl_read_http_reply(conn, 1, NULL, NULL, &headers_reply);
  if (DPL_SUCCESS != ret2)
    {
      if (DPL_ENOENT == ret2)
        {
          ret = DPL_ENOENT;
          goto end;
        }
      else
        {
          DPLERR(0, "read http answer failed");
          connection_close = 1;
          ret = DPL_ENOENT; //mapped to 404
          goto end;
        }
    }
  else
    {
      if (NULL != headers_reply) //possible if continue succeeded
        connection_close = dpl_connection_close(ctx, headers_reply);
    }

  (void) dpl_log_event(ctx, "DATA", "IN", data_len);

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

  DPRINTF("ret=%d\n", ret);

  return ret;
}

dpl_status_t
dpl_cdmi_put(dpl_ctx_t *ctx,
             const char *bucket,
             const char *resource,
             const char *subresource,
             dpl_ftype_t object_type,
             const dpl_dict_t *metadata,
             const dpl_sysmd_t *sysmd,
             const char *data_buf,
             unsigned int data_len)
{
  char          *host;
  int           ret, ret2;
  dpl_conn_t   *conn = NULL;
  char          header[1024];
  u_int         header_len;
  struct iovec  iov[10];
  int           n_iov = 0;
  int           connection_close = 0;
  dpl_dict_t    *headers_request = NULL;
  dpl_dict_t    *headers_reply = NULL;
  dpl_req_t     *req = NULL;
  dpl_chunk_t   chunk;
  char          *body_str = NULL;
  int           body_len = 0;

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

  //contact default host
  dpl_req_rm_behavior(req, DPL_BEHAVIOR_VIRTUAL_HOSTING);

  dpl_req_set_object_type(req, object_type);

  if (NULL != data_buf)
    {
      chunk.buf = data_buf;
      chunk.len = data_len;
      dpl_req_set_chunk(req, &chunk);
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
  ret2 = dpl_cdmi_req_build(req, &headers_request, &body_str, &body_len);
  if (DPL_SUCCESS != ret2)
    {
      ret = DPL_FAILURE;
      goto end;
    }

  host = dpl_dict_get_value(headers_request, "Host");
  if (NULL == host)
    {
      ret = DPL_FAILURE;
      goto end;
    }

  conn = dpl_conn_open_host(ctx, host, ctx->port);
  if (NULL == conn)
    {
      ret = DPL_FAILURE;
      goto end;
    }

  //bucket emulation
  ret2 = dpl_dict_add(headers_request, "X-Scality-Bucket", bucket, 0);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  ret2 = dpl_req_gen_http_request(ctx, req, headers_request, NULL, header, sizeof (header), &header_len);
  if (DPL_SUCCESS != ret2)
    {
      ret = DPL_FAILURE;
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
      DPLERR(1, "writev failed");
      connection_close = 1;
      ret = DPL_ENOENT; //mapped to 404
      goto end;
    }

  ret2 = dpl_read_http_reply(conn, 1, NULL, NULL, &headers_reply);
  if (DPL_SUCCESS != ret2)
    {
      if (DPL_ENOENT == ret2)
        {
          ret = DPL_ENOENT;
          goto end;
        }
      else
        {
          DPLERR(0, "read http answer failed");
          connection_close = 1;
          ret = DPL_ENOENT; //mapped to 404
          goto end;
        }
    }
  else
    {
      connection_close = dpl_connection_close(ctx, headers_reply);
    }

  (void) dpl_log_event(ctx, "DATA", "IN", data_len);

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

  DPRINTF("ret=%d\n", ret);

  return ret;
}

dpl_status_t
dpl_cdmi_put_buffered(dpl_ctx_t *ctx,
                      const char *bucket,
                      const char *resource,
                      const char *subresource,
                      dpl_ftype_t object_type,
                      const dpl_dict_t *metadata,
                      const dpl_sysmd_t *sysmd,
                      unsigned int data_len,
                      dpl_conn_t **connp)
{
  char          *host;
  int           ret, ret2;
  dpl_conn_t    *conn = NULL;
  char          header[1024];
  u_int         header_len;
  struct iovec  iov[10];
  int           n_iov = 0;
  int           connection_close = 0;
  dpl_dict_t    *headers_request = NULL;
  dpl_dict_t    *headers_reply = NULL;
  dpl_req_t     *req = NULL;
  dpl_chunk_t   chunk;

  req = dpl_req_new(ctx);
  if (NULL == req)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

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

  chunk.buf = NULL;
  chunk.len = data_len;
  dpl_req_set_chunk(req, &chunk);

  //contact default host
  dpl_req_rm_behavior(req, DPL_BEHAVIOR_VIRTUAL_HOSTING);

  dpl_req_add_behavior(req, DPL_BEHAVIOR_HTTP_COMPAT);

  dpl_req_set_object_type(req, object_type);

  dpl_req_add_behavior(req, DPL_BEHAVIOR_EXPECT);

  if (NULL != metadata)
    {
      ret2 = dpl_req_add_metadata(req, metadata);
      if (DPL_SUCCESS != ret2)
        {
          ret = ret2;
          goto end;
        }
    }

  ret2 = dpl_cdmi_req_build(req, &headers_request, NULL, NULL);
  if (DPL_SUCCESS != ret2)
    {
      ret = DPL_FAILURE;
      goto end;
    }

  host = dpl_dict_get_value(headers_request, "Host");
  if (NULL == host)
    {
      ret = DPL_FAILURE;
      goto end;
    }

  conn = dpl_conn_open_host(ctx, host, ctx->port);
  if (NULL == conn)
    {
      ret = DPL_FAILURE;
      goto end;
    }

  //bucket emulation
  ret2 = dpl_dict_add(headers_request, "X-Scality-Bucket", bucket, 0);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  ret2 = dpl_req_gen_http_request(ctx, req, headers_request, NULL, header, sizeof (header), &header_len);
  if (DPL_SUCCESS != ret2)
    {
      ret = DPL_FAILURE;
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
      DPLERR(1, "writev failed");
      connection_close = 1;
      ret = DPL_ENOENT; //mapped to 404
      goto end;
    }

  ret2 = dpl_read_http_reply(conn, 1, NULL, NULL, &headers_reply);
  if (DPL_SUCCESS != ret2)
    {
      if (DPL_ENOENT == ret2)
        {
          ret = DPL_ENOENT;
          goto end;
        }
      else
        {
          DPLERR(0, "read http answer failed");
          connection_close = 1;
          ret = DPL_ENOENT; //mapped to 404
          goto end;
        }
    }
  else
    {
      if (NULL != headers_reply) //possible if continue succeeded
        connection_close = dpl_connection_close(ctx, headers_reply);
    }

  (void) dpl_log_event(ctx, "DATA", "IN", data_len);

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

  DPRINTF("ret=%d\n", ret);

  return ret;
}

dpl_status_t
dpl_cdmi_get(dpl_ctx_t *ctx,
             const char *bucket,
             const char *resource,
             const char *subresource,
             dpl_ftype_t object_type,
             const dpl_condition_t *condition,
             char **data_bufp,
             unsigned int *data_lenp,
             dpl_dict_t **metadatap)
{
  char          *host;
  int           ret, ret2;
  dpl_conn_t   *conn = NULL;
  char          header[1024];
  u_int         header_len;
  struct iovec  iov[10];
  int           n_iov = 0;
  int           connection_close = 0;
  char          *data_buf = NULL;
  u_int         data_len;
  dpl_dict_t    *headers_request = NULL;
  dpl_dict_t    *headers_reply = NULL;
  dpl_dict_t    *metadata = NULL;
  dpl_req_t     *req = NULL;

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

  //contact default host
  dpl_req_rm_behavior(req, DPL_BEHAVIOR_VIRTUAL_HOSTING);

  dpl_req_set_object_type(req, object_type);

  //build request
  ret2 = dpl_cdmi_req_build(req, &headers_request, NULL, NULL);
  if (DPL_SUCCESS != ret2)
    {
      ret = DPL_FAILURE;
      goto end;
    }

  host = dpl_dict_get_value(headers_request, "Host");
  if (NULL == host)
    {
      ret = DPL_FAILURE;
      goto end;
    }

  conn = dpl_conn_open_host(ctx, host, ctx->port);
  if (NULL == conn)
    {
      ret = DPL_FAILURE;
      goto end;
    }

  ret2 = dpl_req_gen_http_request(ctx, req, headers_request, NULL, header, sizeof (header), &header_len);
  if (DPL_SUCCESS != ret2)
    {
      ret = DPL_FAILURE;
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
      DPLERR(1, "writev failed");
      connection_close = 1;
      ret = DPL_ENOENT; //mapped to 404
      goto end;
    }

  ret2 = dpl_read_http_reply(conn, 1, &data_buf, &data_len, &headers_reply);
  if (DPL_SUCCESS != ret2)
    {
      if (DPL_ENOENT == ret2)
        {
          ret = DPL_ENOENT;
          goto end;
        }
      else
        {
          DPLERR(0, "read http answer failed");
          connection_close = 1;
          ret = DPL_ENOENT; //mapped to 404
          goto end;
        }
    }
  else
    {
      connection_close = dpl_connection_close(ctx, headers_reply);
    }

  (void) dpl_log_event(ctx, "DATA", "OUT", data_len);

  if (NULL != data_bufp)
    {
      *data_bufp = data_buf;
      data_buf = NULL; //consume it
    }

  if (NULL != data_lenp)
    *data_lenp = data_len;

  if (NULL != metadatap)
    {
      *metadatap = metadata;
      metadata = NULL; //consume it
    }

  ret = DPL_SUCCESS;

 end:

  if (NULL != data_buf)
    free(data_buf);

  if (NULL != conn)
    {
      if (1 == connection_close)
        dpl_conn_terminate(conn);
      else
        dpl_conn_release(conn);
    }

  if (NULL != metadata)
    dpl_dict_free(metadata);

  if (NULL != headers_reply)
    dpl_dict_free(headers_reply);

  if (NULL != headers_request)
    dpl_dict_free(headers_request);

  if (NULL != req)
    dpl_req_free(req);

  DPRINTF("ret=%d\n", ret);

  return ret;
}

dpl_status_t
dpl_cdmi_get_range(dpl_ctx_t *ctx,
                   const char *bucket,
                   const char *resource,
                   const char *subresource,
                   dpl_ftype_t object_type,
                   const dpl_condition_t *condition,
                   int start,
                   int end,
                   char **data_bufp,
                   unsigned int *data_lenp,
                   dpl_dict_t **metadatap)
{
  return DPL_ENOTSUPP;
}

struct get_conven
{
  dpl_header_func_t header_func;
  dpl_buffer_func_t buffer_func;
  void *cb_arg;
  int connection_close;
};

static dpl_status_t
cb_get_header(void *cb_arg,
              const char *header,
              char *value)
{
  struct get_conven *gc = (struct get_conven *) cb_arg;
  int ret;

  if (NULL != gc->header_func)
    {
      ret = gc->header_func(gc->cb_arg, header, value);
      if (DPL_SUCCESS != ret)
        return ret;
    }

  if (!strcasecmp(header, "connection"))
    {
      if (!strcasecmp(value, "close"))
        gc->connection_close = 1;
    }

  return DPL_SUCCESS;
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
dpl_cdmi_get_buffered(dpl_ctx_t *ctx,
                      const char *bucket,
                      const char *resource,
                      const char *subresource,
                      dpl_ftype_t object_type,
                      const dpl_condition_t *condition,
                      dpl_header_func_t header_func,
                      dpl_buffer_func_t buffer_func,
                      void *cb_arg)
{
  char          *host;
  int           ret, ret2;
  dpl_conn_t   *conn = NULL;
  char          header[1024];
  u_int         header_len;
  struct iovec  iov[10];
  int           n_iov = 0;
  dpl_dict_t    *headers_request = NULL;
  dpl_req_t     *req = NULL;
  struct get_conven gc;

  memset(&gc, 0, sizeof (gc));
  gc.header_func = header_func;
  gc.buffer_func = buffer_func;
  gc.cb_arg = cb_arg;

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

  //contact default host
  dpl_req_rm_behavior(req, DPL_BEHAVIOR_VIRTUAL_HOSTING);

  dpl_req_add_behavior(req, DPL_BEHAVIOR_HTTP_COMPAT);

  //build request
  ret2 = dpl_cdmi_req_build(req, &headers_request, NULL, NULL);
  if (DPL_SUCCESS != ret2)
    {
      ret = DPL_FAILURE;
      goto end;
    }

  host = dpl_dict_get_value(headers_request, "Host");
  if (NULL == host)
    {
      ret = DPL_FAILURE;
      goto end;
    }

  conn = dpl_conn_open_host(ctx, host, ctx->port);
  if (NULL == conn)
    {
      ret = DPL_FAILURE;
      goto end;
    }

  //bucket emulation
  ret2 = dpl_dict_add(headers_request, "X-Scality-Bucket", bucket, 0);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  ret2 = dpl_req_gen_http_request(ctx, req, headers_request, NULL, header, sizeof (header), &header_len);
  if (DPL_SUCCESS != ret2)
    {
      ret = DPL_FAILURE;
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
      DPLERR(1, "writev failed");
      gc.connection_close = 1;
      ret = DPL_ENOENT; //mapped to 404
      goto end;
    }

  ret2 = dpl_read_http_reply_buffered(conn, 1, cb_get_header, cb_get_buffer, &gc);
  if (DPL_SUCCESS != ret2)
    {
      if (DPL_ENOENT == ret2)
        {
          ret = DPL_ENOENT;
          goto end;
        }
      else
        {
          DPLERR(0, "read http answer failed");
          gc.connection_close = 1;
          ret = DPL_ENOENT; //mapped to 404
          goto end;
        }
    }

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

  if (NULL != headers_request)
    dpl_dict_free(headers_request);

  if (NULL != req)
    dpl_req_free(req);

  DPRINTF("ret=%d\n", ret);

  return ret;
}

dpl_status_t
dpl_cdmi_head_all(dpl_ctx_t *ctx,
                  const char *bucket,
                  const char *resource,
                  const char *subresource,
                  dpl_ftype_t object_type,
                  const dpl_condition_t *condition,
                  dpl_dict_t **metadatap)
{
  int ret, ret2;
  char *md_buf = NULL;
  u_int md_len;
  dpl_dict_t *metadata = NULL;
  
  //fetch metadata from JSON content
  ret2 = dpl_cdmi_get(ctx, bucket, resource, NULL != subresource ? subresource : "metadata", object_type, condition, &md_buf, &md_len, NULL);
  if (DPL_SUCCESS != ret2)
    {
      ret = DPL_FAILURE;
      goto end;
    }
  
  metadata = dpl_dict_new(13);
  if (NULL == metadata)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  ret2 = dpl_cdmi_parse_metadata(ctx, md_buf, md_len, metadata);
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

  if (NULL != md_buf)
    free(md_buf);
  
  return ret;
}

dpl_status_t
dpl_cdmi_head(dpl_ctx_t *ctx,
              const char *bucket,
              const char *resource,
              const char *subresource,
              dpl_ftype_t object_type,
              const dpl_condition_t *condition,
              dpl_dict_t **metadatap)
{
  int ret, ret2;
  dpl_dict_t *all_mds = NULL;
  dpl_dict_t *metadata = NULL;

  ret2 = dpl_cdmi_head_all(ctx, bucket, resource, subresource, object_type, condition, &all_mds);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  metadata = dpl_dict_new(13);
  if (NULL == metadata)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  ret2 = dpl_cdmi_get_metadata_from_json_metadata(all_mds, metadata);
  if (DPL_SUCCESS != ret2)
    {
      ret = DPL_FAILURE;
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

  return ret;
}

dpl_status_t
dpl_cdmi_delete(dpl_ctx_t *ctx,
                const char *bucket,
                const char *resource,
                const char *subresource,
                dpl_ftype_t object_type)
{
  char          *host;
  int           ret, ret2;
  dpl_conn_t   *conn = NULL;
  char          header[1024];
  u_int         header_len;
  struct iovec  iov[10];
  int           n_iov = 0;
  int           connection_close = 0;
  dpl_dict_t    *headers_request = NULL;
  dpl_dict_t    *headers_reply = NULL;
  dpl_req_t     *req = NULL;

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

  //contact default host
  dpl_req_rm_behavior(req, DPL_BEHAVIOR_VIRTUAL_HOSTING);

  dpl_req_add_behavior(req, DPL_BEHAVIOR_HTTP_COMPAT);

  //build request
  ret2 = dpl_cdmi_req_build(req, &headers_request, NULL, NULL);
  if (DPL_SUCCESS != ret2)
    {
      ret = DPL_FAILURE;
      goto end;
    }

  host = dpl_dict_get_value(headers_request, "Host");
  if (NULL == host)
    {
      ret = DPL_FAILURE;
      goto end;
    }

  conn = dpl_conn_open_host(ctx, host, ctx->port);
  if (NULL == conn)
    {
      ret = DPL_FAILURE;
      goto end;
    }

  //bucket emulation
  ret2 = dpl_dict_add(headers_request, "X-Scality-Bucket", bucket, 0);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  ret2 = dpl_req_gen_http_request(ctx, req, headers_request, NULL, header, sizeof (header), &header_len);
  if (DPL_SUCCESS != ret2)
    {
      ret = DPL_FAILURE;
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
      DPLERR(1, "writev failed");
      connection_close = 1;
      ret = DPL_ENOENT; //mapped to 404
      goto end;
    }

  ret2 = dpl_read_http_reply(conn, 1, NULL, NULL, &headers_reply);
  if (DPL_SUCCESS != ret2)
    {
      if (DPL_ENOENT == ret2)
        {
          ret = DPL_ENOENT;
          goto end;
        }
      else
        {
          DPLERR(0, "read http answer failed");
          connection_close = 1;
          ret = DPL_ENOENT; //mapped to 404
          goto end;
        }
    }
  else
    {
      connection_close = dpl_connection_close(ctx, headers_reply);
    }

  (void) dpl_log_event(ctx, "REQUEST", "DELETE", 0);

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

  DPRINTF("ret=%d\n", ret);

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
              dpl_ftype_t object_type,
              dpl_metadata_directive_t metadata_directive,
              const dpl_dict_t *metadata,
              const dpl_sysmd_t *sysmd,
              const dpl_condition_t *condition)
{
  int ret, ret2;

  switch (metadata_directive)
    {
    case DPL_METADATA_DIRECTIVE_UNDEF:
      ret = DPL_ENOTSUPP;
      goto end;
    case DPL_METADATA_DIRECTIVE_COPY:
      ret = DPL_ENOTSUPP;
      goto end;
    case DPL_METADATA_DIRECTIVE_REPLACE:

      //replace the metadata
      ret2 = dpl_cdmi_put(ctx, dst_bucket, dst_resource,
                          NULL != dst_subresource ? dst_subresource : "metadata",
                          object_type, metadata, DPL_CANNED_ACL_UNDEF, NULL, 0);
      if (DPL_SUCCESS != ret2)
        {
          ret = ret2;
          goto end;
        }

      break ;
    }

  ret = DPL_SUCCESS;
  
 end:

  return ret;
}

dpl_status_t
dpl_cdmi_get_id_path(dpl_ctx_t *ctx,
                     const char *bucket,
                     char **id_pathp)
{
  char *id_path;

  id_path = strdup("/cdmi_objectid/");
  if (NULL == id_path)
    return DPL_ENOMEM;
  
  *id_pathp = id_path;

  return DPL_SUCCESS;
}
