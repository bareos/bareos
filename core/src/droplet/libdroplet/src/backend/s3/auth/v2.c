/*
 * Copyright (C) 2020-2024 Bareos GmbH & Co. KG
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

static int var_cmp(const void* p1, const void* p2)
{
  dpl_value_t* val1 = *(dpl_value_t**)p1;
  dpl_value_t* val2 = *(dpl_value_t**)p2;
  dpl_dict_var_t* var1 = (dpl_dict_var_t*)val1->ptr;
  dpl_dict_var_t* var2 = (dpl_dict_var_t*)val2->ptr;

  assert(var1->val->type == DPL_VALUE_STRING);
  assert(var2->val->type == DPL_VALUE_STRING);

  return strcasecmp(var1->key, var2->key);
}

static dpl_status_t dpl_s3_make_signature_v2(dpl_ctx_t* ctx,
                                             const char* method,
                                             const char* bucket,
                                             const char* resource,
                                             const char* subresource,
                                             char* date,
                                             dpl_dict_t* headers,
                                             char* buf,
                                             unsigned int len,
                                             unsigned int* lenp)
{
  char* p;
  char* value;
  int ret;

  p = buf;

  // method
  DPL_APPEND_STR(method);
  DPL_APPEND_STR("\n");

  // md5
  if (headers != NULL) {
    value = dpl_dict_get_value(headers, "Content-MD5");
    if (NULL != value) DPL_APPEND_STR(value);
  }
  DPL_APPEND_STR("\n");

  // content type
  if (headers != NULL) {
    value = dpl_dict_get_value(headers, "Content-Type");
    if (NULL != value) DPL_APPEND_STR(value);
  }
  DPL_APPEND_STR("\n");

  // expires or date
  if (date != NULL) DPL_APPEND_STR(date);
  DPL_APPEND_STR("\n");

  // x-amz headers
  if (headers != NULL) {
    dpl_dict_var_t* var;
    dpl_vec_t* vec;

    vec = dpl_vec_new(2, 2);
    if (NULL == vec) return DPL_ENOMEM;

    for (int i = 0; i < headers->n_buckets; i++) {
      for (var = headers->buckets[i]; var; var = var->prev) {
        if (!strncmp(var->key, "x-amz-", 6) && strcmp(var->key, "x-amz-date")) {
          assert(DPL_VALUE_STRING == var->val->type);
          ret = dpl_vec_add(vec, var);
          if (DPL_SUCCESS != ret) {
            dpl_vec_free(vec);
            return DPL_FAILURE;
          }
        }
      }
    }

    dpl_vec_sort(vec, var_cmp);

    for (int i = 0; i < vec->n_items; i++) {
      var = (dpl_dict_var_t*)dpl_vec_get(vec, i);
      if (var == NULL) continue;

      assert(DPL_VALUE_STRING == var->val->type);
      DPL_APPEND_STR(var->key);
      DPL_APPEND_STR(":");
      DPL_APPEND_STR(dpl_sbuf_get_str(var->val->string));
      DPL_APPEND_STR("\n");
    }

    dpl_vec_free(vec);
  }

  // resource
  if (NULL != bucket) {
    DPL_APPEND_STR("/");
    DPL_APPEND_STR(bucket);
  }

  if (NULL != resource) DPL_APPEND_STR(resource);

  if (NULL != subresource) {
    DPL_APPEND_STR("?");
    DPL_APPEND_STR(subresource);
  }

  if (NULL != lenp) *lenp = p - buf;

  return DPL_SUCCESS;
}

dpl_status_t dpl_s3_add_authorization_v2_to_headers(
    const dpl_req_t* req,
    dpl_dict_t* headers,
    const dpl_dict_t UNUSED* query_params,
    struct tm UNUSED* tm)
{
  int ret;
  const char* method = dpl_method_str(req->method);
  char resource_ue[DPL_URL_LENGTH(strlen(req->resource)) + 1];
  char sign_str[1024];
  u_int sign_len;
  char hmac_str[1024];
  u_int hmac_len;
  char base64_str[1024];
  u_int base64_len;
  char auth_str[1024];
  char* date_str = NULL;

  // resource
  if ('/' != req->resource[0]) {
    resource_ue[0] = '/';
    dpl_url_encode_no_slashes(req->resource, resource_ue + 1);
  } else
    dpl_url_encode_no_slashes(req->resource, resource_ue);

  date_str = dpl_dict_get_value(headers, "x-amz-date");
  if (date_str == NULL) date_str = dpl_dict_get_value(headers, "Date");

  ret = dpl_s3_make_signature_v2(req->ctx, method, req->bucket, resource_ue,
                                 req->subresource, date_str, headers, sign_str,
                                 sizeof(sign_str), &sign_len);
  if (DPL_SUCCESS != ret) return DPL_FAILURE;

  /* DPL_TRACE(req->ctx, DPL_TRACE_REQ, "stringtosign=%.*s", sign_len,
   * sign_str); */

  hmac_len = dpl_hmac_sha1(req->ctx->secret_key, strlen(req->ctx->secret_key),
                           sign_str, sign_len, hmac_str);

  base64_len = dpl_base64_encode((const u_char*)hmac_str, hmac_len,
                                 (u_char*)base64_str);

  snprintf(auth_str, sizeof(auth_str), "AWS %s:%.*s", req->ctx->access_key,
           base64_len, base64_str);

  return dpl_dict_add(headers, "Authorization", auth_str, 0);
}

dpl_status_t dpl_s3_get_authorization_v2_params(const dpl_req_t* req,
                                                dpl_dict_t* query_params,
                                                struct tm UNUSED* tm)
{
  dpl_status_t ret;
  char expires_str[128], resource_ue[DPL_URL_LENGTH(strlen(req->resource)) + 1];

  snprintf(expires_str, sizeof(expires_str), "%ld", req->expires);

  if ('/' != req->resource[0]) {
    resource_ue[0] = '/';
    dpl_url_encode_no_slashes(req->resource, resource_ue + 1);
  } else
    dpl_url_encode_no_slashes(req->resource, resource_ue);

  ret = dpl_dict_add(query_params, "AWSAccessKeyId", req->ctx->access_key, 0);
  if (ret != DPL_SUCCESS) return DPL_FAILURE;

  {
    char sign_str[1024];
    u_int sign_len;
    char hmac_str[1024];
    u_int hmac_len;
    char base64_str[1024];
    u_int base64_len;
    char base64_ue_str[1024];
    const char* method = dpl_method_str(req->method);

    ret = dpl_s3_make_signature_v2(req->ctx, method, req->bucket, resource_ue,
                                   NULL, expires_str, NULL, sign_str,
                                   sizeof(sign_str), &sign_len);
    if (ret != DPL_SUCCESS) return DPL_FAILURE;

    DPRINTF("%s\n", sign_str);

    hmac_len = dpl_hmac_sha1(req->ctx->secret_key, strlen(req->ctx->secret_key),
                             sign_str, sign_len, hmac_str);

    base64_len = dpl_base64_encode((const u_char*)hmac_str, hmac_len,
                                   (u_char*)base64_str);
    base64_str[base64_len] = 0;

    dpl_url_encode(base64_str, base64_ue_str);

    ret = dpl_dict_add(query_params, "Signature", base64_ue_str, 0);
    if (ret != DPL_SUCCESS) return DPL_FAILURE;
  }

  ret = dpl_dict_add(query_params, "Expires", expires_str, 0);
  if (ret != DPL_SUCCESS) return DPL_FAILURE;

  return DPL_SUCCESS;
}
