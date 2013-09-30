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
#include <libgen.h>
#include <droplet/swift/swift.h>

/** @file */

//#define DPRINTF(fmt,...) fprintf(stderr, fmt, ##__VA_ARGS__)
#define DPRINTF(fmt,...)

#define DPL_SWIFT_AUTH_RSRC "/auth/v1.0"

dpl_status_t
dpl_swift_get_capabilities(dpl_ctx_t *ctx,
                          dpl_capability_t *maskp)
{
  if (NULL != maskp)
    *maskp = DPL_CAP_FNAMES|
      DPL_CAP_HTTP_COMPAT;
  
  return DPL_SUCCESS;
}

typedef struct dpl_swift_ctx
{
  char *storage_url;
  char *auth_token;
} dpl_swift_ctx_t;

dpl_status_t
dpl_swift_login(dpl_ctx_t *ctx)
{
  int           ret, ret2;
  dpl_conn_t    *conn = NULL;
  dpl_req_t     *req = NULL;
  dpl_dict_t    *headers_request = NULL;
  dpl_dict_t    *headers_reply = NULL;

  char          header[dpl_header_size];
  u_int         header_len;
  struct iovec  iov[10];
  int           n_iov = 0;
  int           connection_close = 0;

  dpl_swift_ctx_t *swift_ctx;
  char *auth_token, *storage_url;
  
  req = dpl_req_new(ctx);
  if (NULL == req)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  dpl_req_set_method(req, DPL_METHOD_GET);
  dpl_req_set_object_type(req, DPL_FTYPE_ANY);
  dpl_req_set_resource(req, DPL_SWIFT_AUTH_RSRC);
  
  dpl_req_rm_behavior(req, DPL_BEHAVIOR_KEEP_ALIVE);

  ret2 = dpl_swift_req_build(ctx, req, 0, &headers_request, NULL, NULL);
  dpl_dict_add(headers_request, "X-Storage-User", ctx->access_key, 0);
  dpl_dict_add(headers_request, "X-Storage-Pass", ctx->secret_key, 0);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

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

  /* WIP dpl_dict_print(headers_reply, stdout, -1); */

  swift_ctx = calloc(1, sizeof(dpl_swift_ctx_t));
  if (NULL == swift_ctx)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  auth_token = dpl_dict_get_value(headers_reply, "x-auth-token");
  storage_url = dpl_dict_get_value(headers_reply, "x-storage-url");
  if (auth_token == NULL || storage_url == NULL)
    {
      ret = DPL_ENOMEM;
      goto end;
    }
  swift_ctx->auth_token = strdup(auth_token);
  swift_ctx->storage_url = strdup(storage_url);

  ctx->backend_ctx = swift_ctx;

  /*  */
  /* printf("X-auth-token: %s\n", swift_ctx->auth_token); */
  /* printf("X-storage-url: %s\n", ((dpl_swift_ctx_t *)(ctx->backend_ctx))->storage_url); */
  /*  */

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

  return ret;
}

static dpl_status_t
dpl_swift_set_directory(dpl_req_t *req,
			dpl_ctx_t *ctx,
			char *bucket)
{
  char rsrc[256];
  char *path = ((dpl_swift_ctx_t *)(ctx->backend_ctx))->storage_url;

  sprintf(rsrc, "/v1/%s/%s", basename(path), bucket ?: "");

  dpl_req_set_resource(req, rsrc);
  /* printf("path: %s\naccess: %s\n", path, rsrc); */
  
  return DPL_SUCCESS;
}

dpl_status_t
dpl_swift_get(dpl_ctx_t *ctx,
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
  dpl_req_set_object_type(req, DPL_FTYPE_ANY);

  dpl_swift_set_directory(req, ctx, resource);
  
  dpl_req_rm_behavior(req, DPL_BEHAVIOR_KEEP_ALIVE);

  ret2 = dpl_swift_req_build(ctx, req, 0, &headers_request, NULL, NULL);  
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  dpl_dict_add(headers_request, "X-Auth-Token", ((dpl_swift_ctx_t *)(ctx->backend_ctx))->auth_token, 0);

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

  ret2 = dpl_read_http_reply(conn, 1, data_bufp, data_lenp, &headers_reply, &connection_close);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  /* /\* WIP *\/ dpl_dict_print(headers_reply, stdout, -1); */
  objects = dpl_vec_new(2, 2);
  if (NULL == objects)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  ret = DPL_SUCCESS;

 end:

  if (NULL != objects)
    dpl_vec_objects_free(objects);

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
dpl_swift_put(dpl_ctx_t *ctx,
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
  char *data_buf_returned = NULL;
  u_int data_len_returned;
  dpl_value_t *val = NULL;
  dpl_swift_req_mask_t req_mask = 0u;

  DPL_TRACE(ctx, DPL_TRACE_BACKEND, "");

  if (option)
    {
      if (option->mask & DPL_OPTION_HTTP_COMPAT)
        req_mask |= DPL_SWIFT_REQ_HTTP_COMPAT;
    }

  req = dpl_req_new(ctx);
  if (NULL == req)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  dpl_req_set_method(req, DPL_METHOD_PUT);

  ret2 = dpl_swift_set_directory(req, ctx, resource);
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

  /* if (range) */
  /*   { */
  /*     ret2 = dpl_swift_req_add_range(req, req_mask, range); */
  /*     if (DPL_SUCCESS != ret2) */
  /*       { */
  /*         ret = ret2; */
  /*         goto end; */
  /*       } */
  /*   } */

  dpl_req_set_object_type(req, object_type);
  dpl_req_set_data(req, data_buf, data_len);

  /* if (NULL != sysmd) */
  /*   { */
  /*     ret2 = dpl_swift_add_sysmd_to_req(sysmd, req); */
  /*     if (DPL_SUCCESS != ret2) */
  /*       { */
  /*         ret = ret2; */
  /*         goto end; */
  /*       } */
  /*   } */

  /* if (NULL != metadata) */
  /*   { */
  /*     ret2 = dpl_swift_req_add_metadata(req, metadata, option ? option->mask & DPL_OPTION_APPEND_METADATA : 0); */
  /*     if (DPL_SUCCESS != ret2) */
  /*       { */
  /*         ret = ret2; */
  /*         goto end; */
  /*       } */
  /*   } */

  //build request
  ret2 = dpl_swift_req_build(ctx, req, 0, &headers_request, &data_buf, &data_len);
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

  dpl_dict_add(headers_request, "X-Auth-Token", ((dpl_swift_ctx_t *)(ctx->backend_ctx))->auth_token, 0);

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
  iov[n_iov].iov_base = data_buf;
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

  ret2 = dpl_read_http_reply(conn, 1, &data_buf_returned, &data_len_returned, &headers_reply, &connection_close);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }
  

  ret = DPL_SUCCESS;

 end:

  if (NULL != val)
    dpl_value_free(val);

  if (NULL != data_buf_returned)
    free(data_buf_returned);

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
dpl_swift_delete(dpl_ctx_t *ctx,
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
  dpl_vec_t     *common_prefixes = NULL;
  dpl_vec_t     *objects = NULL;

  DPL_TRACE(ctx, DPL_TRACE_BACKEND, "");

  req = dpl_req_new(ctx);
  if (NULL == req)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  dpl_req_set_method(req, DPL_METHOD_DELETE);
  dpl_req_set_object_type(req, DPL_FTYPE_ANY);

  dpl_swift_set_directory(req, ctx, resource);
  
  dpl_req_rm_behavior(req, DPL_BEHAVIOR_KEEP_ALIVE);

  ret2 = dpl_swift_req_build(ctx, req, 0, &headers_request, NULL, NULL);  
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  dpl_dict_add(headers_request, "X-Auth-Token", ((dpl_swift_ctx_t *)(ctx->backend_ctx))->auth_token, 0);

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

  /* /\* WIP *\/ dpl_dict_print(headers_reply, stdout, -1); */
  objects = dpl_vec_new(2, 2);
  if (NULL == objects)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  ret = DPL_SUCCESS;

 end:
  if (NULL != objects)
    dpl_vec_objects_free(objects);

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
dpl_swift_head_get(dpl_ctx_t *ctx,
             const char *bucket,
             const char *resource,
             const char *subresource,
             const dpl_option_t *option,
             dpl_ftype_t object_type,
             const dpl_condition_t *condition,
             const dpl_range_t *range,
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
  dpl_dict_t    *headers_request = NULL;
  dpl_dict_t    *headers_reply = NULL;
  dpl_req_t     *req = NULL;
  dpl_vec_t     *common_prefixes = NULL;
  dpl_swift_req_mask_t req_mask = 0u;

  DPL_TRACE(ctx, DPL_TRACE_BACKEND, "");

  if (option)
    {
      if (option->mask & DPL_OPTION_HTTP_COMPAT)
        req_mask |= DPL_SWIFT_REQ_HTTP_COMPAT;
    }

  req = dpl_req_new(ctx);
  if (NULL == req)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  dpl_req_set_method(req, DPL_METHOD_HEAD);
  dpl_req_set_object_type(req, DPL_FTYPE_ANY);

  dpl_swift_set_directory(req, ctx, resource);
  
  dpl_req_rm_behavior(req, DPL_BEHAVIOR_KEEP_ALIVE);

  ret2 = dpl_swift_req_build(ctx, req, 0, &headers_request, NULL, NULL);  
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  dpl_dict_add(headers_request, "X-Auth-Token", ((dpl_swift_ctx_t *)(ctx->backend_ctx))->auth_token, 0);

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

  fprintf(stderr, "read\n");
  ret2 = dpl_read_http_reply(conn, 1, NULL, NULL, &headers_reply, &connection_close);
  fprintf(stderr, "read\n");
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  if (req_mask & DPL_SWIFT_REQ_HTTP_COMPAT)
    {
      //metadata are in headers

      ret2 = dpl_swift_get_metadata_from_headers(headers_reply, metadatap, sysmdp);
      if (DPL_SUCCESS != ret2)
        {
          ret = ret2;
          goto end;
        }
      fprintf(stderr, "TEST\n");
      dpl_dict_print(*metadatap, stderr, -1);
      fprintf(stderr, "TEST\n");
    }


  /* WIP */ dpl_dict_print(headers_reply, stdout, -1);

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
dpl_swift_head_raw(dpl_ctx_t *ctx,
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
  
  memset(&option2, 0, sizeof (option2));
  option2.mask |= DPL_OPTION_HTTP_COMPAT;
  ret2 = dpl_swift_head_get(ctx, bucket, resource, NULL , &option2, object_type, condition, NULL, metadatap, NULL, locationp);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
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
dpl_swift_head(dpl_ctx_t *ctx,
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
  dpl_dict_t *metadata = NULL;

  DPL_TRACE(ctx, DPL_TRACE_BACKEND, "");

  ret2 = dpl_swift_head_raw(ctx, bucket, resource, subresource, option, object_type, condition, &metadata, locationp);
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

  DPL_TRACE(ctx, DPL_TRACE_BACKEND, "ret=%d", ret);

  return ret;
}

dpl_backend_t
dpl_backend_swift = 
  {
    "swift",
    .login	 	= dpl_swift_login,
    .put 		= dpl_swift_put,
    .get 		= dpl_swift_get,
    .head 		= dpl_swift_head,
    .deletef 		= dpl_swift_delete
  };
