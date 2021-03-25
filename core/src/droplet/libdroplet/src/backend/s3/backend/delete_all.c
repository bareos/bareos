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
#include "droplet/s3/s3.h"

struct dall_req {
  dpl_req_t* req;
  dpl_dict_t* headers;
  dpl_dict_t* query_params;
  dpl_conn_t* conn;
  dpl_sbuf_t* data;
  char* answer_data;
  unsigned int answer_len;
  int connection_close;
  dpl_vec_t* objects;
};

static dpl_status_t get_delete_data_content(dpl_ctx_t* ctx,
                                            dpl_locators_t* locators,
                                            dpl_sbuf_t* buffer)
{
  int i;
  dpl_status_t ret;

#define DPL_FN(fn, args...)             \
  ({                                    \
    ret = fn(args);                     \
    if (ret != DPL_SUCCESS) return ret; \
  })
#define ADD_STR(str) DPL_FN(dpl_sbuf_add_str, buffer, str)
#define ADD_STR_FMT(fmt, args...) \
  DPL_FN(dpl_sbuf_add_str_fmt, buffer, fmt, args)

  ADD_STR(
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      "<Delete>\n"
      "    <Quiet>false</Quiet>\n");

  for (i = 0; i < locators->size; i++) {
    dpl_locator_t* locator = &locators->tab[i];

    ADD_STR("    <Object>\n");
    ADD_STR_FMT("        <Key>%s</Key>\n", locator->name);
    if (locator->version_id != NULL)
      ADD_STR_FMT("        <VersionId>%s</VersionId>\n", locator->version_id);
    ADD_STR("    </Object>\n");
  }

  ADD_STR("</Delete>");

#undef DPL_FN
#undef ADD_STR
#undef ADD_STR_FMT

  return DPL_SUCCESS;
}

static dpl_status_t delete_all(dpl_ctx_t* ctx,
                               const char* bucket,
                               dpl_locators_t* locators,
                               struct dall_req* dctx,
                               dpl_vec_t** objectsp)
{
  dpl_status_t ret;
  struct iovec iov[10];
  int n_iov = 0;
  char header[dpl_header_size];
  u_int header_len;

  dctx->req = dpl_req_new(ctx);
  if (dctx->req == NULL) return DPL_ENOMEM;

  dpl_req_set_method(dctx->req, DPL_METHOD_POST);

  if (bucket == NULL) return DPL_EINVAL;

  ret = dpl_req_set_bucket(dctx->req, bucket);
  if (ret != DPL_SUCCESS) return ret;

  ret = dpl_req_set_resource(dctx->req, "/");
  if (ret != DPL_SUCCESS) return ret;

  ret = dpl_req_set_subresource(dctx->req, "delete");
  if (ret != DPL_SUCCESS) return ret;

  dctx->query_params = dpl_dict_new(1);
  if (dctx->query_params == NULL) return ENOMEM;

  /* Note: Juste for authentification signature calcul */
  ret = dpl_dict_add(dctx->query_params, "delete", "", 0);
  if (DPL_SUCCESS != ret) return ret;

  dctx->data = dpl_sbuf_new(4096);
  if (dctx->data == NULL) return DPL_ENOMEM;

  ret = get_delete_data_content(ctx, locators, dctx->data);
  if (ret != DPL_SUCCESS) return ret;

  dpl_req_set_data(dctx->req, dctx->data->buf, dctx->data->len);
  dpl_req_add_behavior(dctx->req, DPL_BEHAVIOR_MD5);

  ret = dpl_s3_req_build(dctx->req, 0, &dctx->headers);
  if (ret != DPL_SUCCESS) return ret;

  ret = dpl_try_connect(ctx, dctx->req, &dctx->conn);
  if (ret != DPL_SUCCESS) return ret;

  ret = dpl_add_host_to_headers(dctx->req, dctx->headers);
  if (ret != DPL_SUCCESS) return ret;

  ret = dpl_s3_add_authorization_to_headers(dctx->req, dctx->headers,
                                            dctx->query_params, NULL);
  if (ret != DPL_SUCCESS) return ret;

  ret = dpl_req_gen_http_request(ctx, dctx->req, dctx->headers, NULL, header,
                                 sizeof(header), &header_len);
  if (ret != DPL_SUCCESS) return ret;

  // Headers
  iov[n_iov].iov_base = header;
  iov[n_iov].iov_len = header_len;
  n_iov++;

  // Final crlf
  iov[n_iov].iov_base = "\r\n";
  iov[n_iov].iov_len = 2;
  n_iov++;

  // Data
  iov[n_iov].iov_base = (void*)dctx->data->buf;
  iov[n_iov].iov_len = dctx->data->len;
  n_iov++;

  ret = dpl_conn_writev_all(dctx->conn, iov, n_iov, ctx->write_timeout);
  if (ret != DPL_SUCCESS) {
    dctx->connection_close = 1;
    return ret;
  }

  ret = dpl_read_http_reply(dctx->conn, 1, &dctx->answer_data,
                            &dctx->answer_len, NULL, &dctx->connection_close);

  if (dctx->answer_len > 0) {
    dpl_status_t parse_ret;

    dctx->objects = dpl_vec_new(2, 2);
    if (dctx->objects == NULL) return DPL_ENOMEM;

    parse_ret = dpl_s3_parse_delete_all(ctx, dctx->answer_data,
                                        dctx->answer_len, dctx->objects);
    if (parse_ret == DPL_SUCCESS && objectsp != NULL) {
      *objectsp = dctx->objects;
      dctx->objects = NULL;
    }

    if (ret == DPL_SUCCESS) ret = parse_ret;
  }

  return ret;
}

dpl_status_t dpl_s3_delete_all(dpl_ctx_t* ctx,
                               const char* bucket,
                               dpl_locators_t* locators,
                               UNUSED const dpl_option_t* option,
                               UNUSED const dpl_condition_t* condition,
                               dpl_vec_t** objectsp)
{
  dpl_status_t ret;
  struct dall_req dctx = {
      .req = NULL,
      .headers = NULL,
      .conn = NULL,
      .data = NULL,
      .answer_data = NULL,
      .answer_len = 0,
      .connection_close = 0,
      .objects = NULL,
  };

  ret = delete_all(ctx, bucket, locators, &dctx, objectsp);

  if (dctx.conn != NULL) {
    if (dctx.connection_close)
      dpl_conn_terminate(dctx.conn);
    else
      dpl_conn_release(dctx.conn);
  }

  if (dctx.headers != NULL) dpl_dict_free(dctx.headers);

  if (dctx.query_params) dpl_dict_free(dctx.query_params);

  if (dctx.req != NULL) dpl_req_free(dctx.req);

  if (dctx.data != NULL) dpl_sbuf_free(dctx.data);

  if (dctx.answer_data != NULL) free(dctx.answer_data);

  if (dctx.objects != NULL) dpl_vec_delete_objects_free(dctx.objects);

  return ret;
}
