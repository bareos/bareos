/*
 * Droplet, high performance cloud storage client library
 * Copyright (C) 2010 Scality http://github.com/scality/Droplet
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *  
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "dropletp.h"

//#define DPRINTF(fmt,...) fprintf(stderr, fmt, ##__VA_ARGS__)
#define DPRINTF(fmt,...)

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
        char *bucket,
        char *resource,
        char *subresource,
        dpl_condition_t *condition,
        char **data_bufp,
        u_int *data_lenp,
        dpl_dict_t **metadatap)
{
  char          host[1024];
  int           ret, ret2;
  struct hostent hret, *hresult;
  char          hbuf[1024];
  int           herr;
  struct in_addr addr;
  dpl_conn_t   *conn = NULL;
  char          header[1024];
  u_int         header_len;
  struct iovec  iov[10];
  int           n_iov = 0;
  int           connection_close = 0;
  char          *data_buf = NULL;
  u_int         data_len;
  dpl_vec_t     *vec = NULL;
  dpl_dict_t    *headers_returned = NULL;
  dpl_dict_t    *metadata = NULL;
  dpl_req_t     *req = NULL;

  snprintf(host, sizeof (host), "%s.%s", bucket, ctx->host);

  DPRINTF("dpl_get: host=%s:%d\n", host, ctx->port);

  req = dpl_req_new(ctx);
  if (NULL == req)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  dpl_req_set_method(req, DPL_METHOD_GET);

  ret2 = dpl_req_set_bucket(req, bucket);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
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

  metadata = dpl_dict_new(13);
  if (NULL == metadata)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  ret2 = linux_gethostbyname_r(host, &hret, hbuf, sizeof (hbuf), &hresult, &herr); 
  if (0 != ret2)
    {
      ret = DPL_FAILURE;
      goto end;
    }

  if (AF_INET != hresult->h_addrtype)
    {
      ret = DPL_FAILURE;
      goto end;
    }

  memcpy(&addr, hresult->h_addr_list[0], hresult->h_length);

  conn = dpl_conn_open(ctx, addr, ctx->port);
  if (NULL == conn)
    {
      ret = DPL_FAILURE;
      goto end;
    }

  //build request
  ret2 = dpl_req_build(req, NULL, header, sizeof (header), &header_len);

  if (DPL_SUCCESS != ret2)
    {
      ret = DPL_FAILURE;
      goto end;
    }

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

  ret2 = dpl_read_http_reply(conn, &data_buf, &data_len, &headers_returned);
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
      connection_close = dpl_connection_close(headers_returned);
      
      ret2 = dpl_get_metadata_from_headers(headers_returned, metadata);
      if (DPL_SUCCESS != ret2)
        {
          ret = DPL_FAILURE;
          goto end;
        }
    }

  (void) dpl_log_charged_event(ctx, "DATA", "OUT", data_len);

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

  if (NULL != vec)
    dpl_vec_objects_free(vec);

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

  if (NULL != headers_returned)
    dpl_dict_free(headers_returned);

  if (NULL != req)
    dpl_req_free(req);

  DPRINTF("ret=%d\n", ret);

  return ret;
}

struct get_conven
{
  dpl_dict_t *headers;
  dpl_buffer_func_t buffer_func;
  void *cb_arg;
};

static dpl_status_t
cb_get_header(void *cb_arg,
              char *header,
              char *value)
{
  struct get_conven *gc = (struct get_conven *) cb_arg;
  int ret;
  
  if (NULL == gc->headers)
    {
      gc->headers = dpl_dict_new(13);
      if (NULL == gc->headers)
        return DPL_ENOMEM;
    }

  ret = dpl_dict_add(gc->headers, header, value, 1);
  if (DPL_SUCCESS != ret)
    return DPL_ENOMEM;

  return DPL_SUCCESS;
}

static dpl_status_t
cb_get_buffer(void *cb_arg,
              char *buf,
              u_int len)
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
dpl_get_buffered(dpl_ctx_t *ctx,
                 char *bucket,
                 char *resource,
                 char *subresource,
                 dpl_condition_t *condition,
                 dpl_buffer_func_t buffer_func,
                 void *cb_arg,
                 dpl_dict_t **metadatap)
{
  char          host[1024];
  int           ret, ret2;
  struct hostent hret, *hresult;
  char          hbuf[1024];
  int           herr;
  struct in_addr addr;
  dpl_conn_t   *conn = NULL;
  char          header[1024];
  u_int         header_len;
  struct iovec  iov[10];
  int           n_iov = 0;
  int           connection_close = 0;
  dpl_vec_t     *vec = NULL;
  dpl_dict_t    *headers = NULL;
  dpl_dict_t    *metadata = NULL;
  struct get_conven gc;

  snprintf(host, sizeof (host), "%s.%s", bucket, ctx->host);

  DPRINTF("dpl_list_bucket: host=%s:%d\n", host, ctx->port);

  memset(&gc, 0, sizeof (gc));
  gc.buffer_func = buffer_func;
  gc.cb_arg = cb_arg;

  headers = dpl_dict_new(13);
  if (NULL == headers)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  ret2 = dpl_dict_add(headers, "Host", host, 0);
  if (DPL_SUCCESS != ret2)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  ret2 = dpl_dict_add(headers, "Connection", "keep-alive", 0);
  if (DPL_SUCCESS != ret2)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  metadata = dpl_dict_new(13);
  if (NULL == metadata)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  if (NULL != condition)
    {
      ret2 = dpl_add_condition_to_headers(condition, headers);
      if (DPL_SUCCESS != ret2)
        {
          ret = DPL_ENOMEM;
          goto end;
        }
    }

  ret2 = linux_gethostbyname_r(host, &hret, hbuf, sizeof (hbuf), &hresult, &herr); 
  if (0 != ret2)
    {
      ret = DPL_FAILURE;
      goto end;
    }

  if (AF_INET != hresult->h_addrtype)
    {
      ret = DPL_FAILURE;
      goto end;
    }

  memcpy(&addr, hresult->h_addr_list[0], hresult->h_length);

  conn = dpl_conn_open(ctx, addr, ctx->port);
  if (NULL == conn)
    {
      ret = DPL_FAILURE;
      goto end;
    }

  //build request
  ret2 = dpl_build_s3_request(ctx, 
                              "GET",
                              bucket,
                              resource,
                              subresource,
                              NULL,
                              headers, 
                              header,
                              sizeof (header),
                              &header_len);
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

  ret2 = dpl_read_http_reply_buffered(conn, cb_get_header, cb_get_buffer, &gc);
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

  connection_close = dpl_connection_close(gc.headers);
  
  ret2 = dpl_get_metadata_from_headers(gc.headers, metadata);
  if (DPL_SUCCESS != ret2)
    {
      ret = DPL_FAILURE;
      goto end;
    }

  //caller is responsible for logging the event

  ret = DPL_SUCCESS;

 end:

  if (NULL != vec)
    dpl_vec_objects_free(vec);

  if (NULL != conn)
    {
      if (1 == connection_close)
        dpl_conn_terminate(conn);
      else
        dpl_conn_release(conn);
    }

  if (NULL != metadata)
    dpl_dict_free(metadata);

  if (NULL != headers)
    dpl_dict_free(headers);

  if (NULL != gc.headers)
    dpl_dict_free(gc.headers);

  DPRINTF("ret=%d\n", ret);

  return ret;
}
