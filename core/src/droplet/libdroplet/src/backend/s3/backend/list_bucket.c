/*
 * Copyright (C) 2020-2021 Bareos GmbH & Co. KG
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
#include "droplet/s3/s3.h"

dpl_status_t dpl_s3_list_bucket(dpl_ctx_t* ctx,
                                const char* bucket,
                                const char* prefix,
                                const char* delimiter,
                                const int max_keys,
                                dpl_vec_t** objectsp,
                                dpl_vec_t** common_prefixesp,
                                char** locationp)
{
  int ret, ret2;
  dpl_conn_t* conn = NULL;
  char header[dpl_header_size];
  u_int header_len;
  struct iovec iov[10];
  int n_iov = 0;
  int connection_close = 0;
  char* data_buf = NULL;
  u_int data_len;
  dpl_vec_t* objects = NULL;
  dpl_vec_t* common_prefixes = NULL;
  dpl_dict_t* query_params = NULL;
  dpl_dict_t* headers_request = NULL;
  dpl_dict_t* headers_reply = NULL;
  dpl_req_t* req = NULL;
  dpl_s3_req_mask_t req_mask = 0u;

  DPL_TRACE(ctx, DPL_TRACE_BACKEND, "");

  req = dpl_req_new(ctx);
  if (NULL == req) {
    ret = DPL_ENOMEM;
    goto end;
  }

  dpl_req_set_method(req, DPL_METHOD_GET);

  if (NULL == bucket) {
    ret = DPL_EINVAL;
    goto end;
  }

  ret2 = dpl_req_set_bucket(req, bucket);
  if (DPL_SUCCESS != ret2) {
    ret = ret2;
    goto end;
  }

  ret2 = dpl_req_set_resource(req, "/");
  if (DPL_SUCCESS != ret2) {
    ret = ret2;
    goto end;
  }

  query_params = dpl_dict_new(13);
  if (NULL == query_params) {
    ret = DPL_ENOMEM;
    goto end;
  }

  if (NULL != prefix) {
    ret2 = dpl_dict_add(query_params, "prefix", prefix, 0);
    if (DPL_SUCCESS != ret2) {
      ret = DPL_ENOMEM;
      goto end;
    }
  }

  if (NULL != delimiter) {
    ret2 = dpl_dict_add(query_params, "delimiter", delimiter, 0);
    if (DPL_SUCCESS != ret2) {
      ret = DPL_ENOMEM;
      goto end;
    }
  }

  if (-1 != max_keys) {
    char tmp[32] = "";

    snprintf(tmp, sizeof tmp, "%d", max_keys);
    ret2 = dpl_dict_add(query_params, "max-keys", tmp, 0);
    if (DPL_SUCCESS != ret2) {
      ret = DPL_ENOMEM;
      goto end;
    }
  }

  ret2 = dpl_s3_req_build(req, req_mask, &headers_request);
  if (DPL_SUCCESS != ret2) {
    ret = ret2;
    goto end;
  }

  ret2 = dpl_try_connect(ctx, req, &conn);
  if (DPL_SUCCESS != ret2) {
    ret = ret2;
    goto end;
  }

  ret2 = dpl_add_host_to_headers(req, headers_request);
  if (DPL_SUCCESS != ret2) {
    ret = ret2;
    goto end;
  }

  ret2 = dpl_s3_add_authorization_to_headers(req, headers_request, query_params,
                                             NULL);
  if (DPL_SUCCESS != ret2) {
    ret = ret2;
    goto end;
  }

  ret2 = dpl_req_gen_http_request(ctx, req, headers_request, query_params,
                                  header, sizeof(header), &header_len);
  if (DPL_SUCCESS != ret2) {
    ret = ret2;
    goto end;
  }

  iov[n_iov].iov_base = header;
  iov[n_iov].iov_len = header_len;
  n_iov++;

  // final crlf
  iov[n_iov].iov_base = "\r\n";
  iov[n_iov].iov_len = 2;
  n_iov++;

  ret2 = dpl_conn_writev_all(conn, iov, n_iov, conn->ctx->write_timeout);
  if (DPL_SUCCESS != ret2) {
    DPL_TRACE(conn->ctx, DPL_TRACE_ERR, "writev failed");
    connection_close = 1;
    ret = ret2;
    goto end;
  }

  ret2 = dpl_read_http_reply(conn, 1, &data_buf, &data_len, &headers_reply,
                             &connection_close);
  if (DPL_SUCCESS != ret2) {
    ret = ret2;
    goto end;
  }

  objects = dpl_vec_new(2, 2);
  if (NULL == objects) {
    ret = DPL_ENOMEM;
    goto end;
  }

  common_prefixes = dpl_vec_new(2, 2);
  if (NULL == common_prefixes) {
    ret = DPL_ENOMEM;
    goto end;
  }

  ret = dpl_s3_parse_list_bucket(ctx, data_buf, data_len, objects,
                                 common_prefixes);
  if (DPL_SUCCESS != ret) {
    ret = ret2;
    goto end;
  }

  if (NULL != objectsp) {
    *objectsp = objects;
    objects = NULL;  // consume it
  }

  if (NULL != common_prefixesp) {
    *common_prefixesp = common_prefixes;
    common_prefixes = NULL;  // consume it
  }

  ret = DPL_SUCCESS;

end:

  if (NULL != objects) dpl_vec_objects_free(objects);

  if (NULL != common_prefixes) dpl_vec_common_prefixes_free(common_prefixes);

  if (NULL != data_buf) free(data_buf);

  if (NULL != conn) {
    if (1 == connection_close)
      dpl_conn_terminate(conn);
    else
      dpl_conn_release(conn);
  }

  if (NULL != query_params) dpl_dict_free(query_params);

  if (NULL != headers_reply) dpl_dict_free(headers_reply);

  if (NULL != headers_request) dpl_dict_free(headers_request);

  if (NULL != req) dpl_req_free(req);

  DPL_TRACE(ctx, DPL_TRACE_BACKEND, "ret=%d", ret);

  return ret;
}
