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
#include "droplet/s3/replyparser.h"
#include "droplet/s3/reqbuilder.h"

//#define DPRINTF(fmt,...) fprintf(stderr, fmt, ##__VA_ARGS__)
#define DPRINTF(fmt,...)

static dpl_status_t
add_payload_signature_to_headers(const dpl_req_t *req, dpl_dict_t *headers)
{
  uint8_t       digest[SHA256_DIGEST_LENGTH];
  char          digest_hex[DPL_HEX_LENGTH(sizeof digest) + 1];
  int           i;

  if (req->data_enabled)
    dpl_sha256((uint8_t *) req->data_buf, req->data_len, digest);
  else
    dpl_sha256((uint8_t *) "", 0, digest);

  for (i = 0; i < sizeof digest; i++)
    sprintf(digest_hex + 2 * i, "%02x", (u_char) digest[i]);

  return dpl_dict_add(headers, "x-amz-content-sha256", digest_hex, 0);
}

static int
var_cmp(const void *p1,
        const void *p2)
{
  dpl_value_t *val1 = *(dpl_value_t **) p1;
  dpl_value_t *val2 = *(dpl_value_t **) p2;
  dpl_dict_var_t *var1 = (dpl_dict_var_t *) val1->ptr;
  dpl_dict_var_t *var2 = (dpl_dict_var_t *) val2->ptr;

  return strcasecmp(var1->key, var2->key);
}

static dpl_status_t
create_canonical_request(const dpl_req_t *req,
                         dpl_dict_t *headers,
                         dpl_vec_t *canonical_headers,
                         dpl_vec_t *canonical_params,
                         char *p, unsigned int len)
{
  // Method
  {
    const char    *method = dpl_method_str(req->method);

    DPL_APPEND_STR(method);
    DPL_APPEND_STR("\n");
  }

  // Resource
  if (req->resource != NULL) {
    char        resource_ue[DPL_URL_LENGTH(strlen(req->resource)) + 1];

    if (req->resource[0] != '/')
      DPL_APPEND_STR("/");

    dpl_url_encode_no_slashes(req->resource, resource_ue);
    DPL_APPEND_STR(resource_ue);
  }
  DPL_APPEND_STR("\n");

  // Query params
  {
    int item;

    for (item = 0; item < canonical_params->n_items; item++) {
      dpl_dict_var_t    *param = (dpl_dict_var_t *) dpl_vec_get(canonical_params, item);

      if (item > 0)
        DPL_APPEND_STR("&");

      DPL_APPEND_STR(param->key);
      DPL_APPEND_STR("=");
      DPL_APPEND_STR(dpl_sbuf_get_str(param->val->string));
    }
    DPL_APPEND_STR("\n");
  }

  // Headers
  {
    int                 item;
    char                *c;
    dpl_dict_var_t      *header;

    for (item = 0; item < canonical_headers->n_items; item++) {
      header = (dpl_dict_var_t *) dpl_vec_get(canonical_headers, item);

      for (c = header->key; *c != '\0'; c++)
        DPL_APPEND_CHAR(tolower(*c));
      DPL_APPEND_STR(":");
      DPL_APPEND_STR(dpl_sbuf_get_str(header->val->string));
      DPL_APPEND_STR("\n");
    }
    DPL_APPEND_STR("\n");

    for (item = 0; item < canonical_headers->n_items; item++) {
      header = (dpl_dict_var_t *) dpl_vec_get(canonical_headers, item);

      if (item > 0)
        DPL_APPEND_STR(";");

      for (c = header->key; *c != '\0'; c++)
        DPL_APPEND_CHAR(tolower(*c));
    }
    DPL_APPEND_STR("\n");
  }

  // Hashed payload
  {
    char        *value = dpl_dict_get_value(headers, "x-amz-content-sha256");
    if (value != NULL)
      DPL_APPEND_STR(value);
  }

  return DPL_SUCCESS;
}

static dpl_status_t
get_current_utc_date(struct tm *tm, char *date_str, size_t date_size)
{
  int           ret;
  time_t        t;

  time(&t);
  gmtime_r(&t, tm);

  ret = strftime(date_str, date_size, "%Y%m%dT%H%M%SZ", tm);

  return (ret == 0 ? DPL_FAILURE : DPL_SUCCESS);
}

static dpl_status_t
create_sign_request(const dpl_req_t *req,
                    char *canonical_request, struct tm *tm, char *date_str,
                    char *p, unsigned int len)
{
  DPL_APPEND_STR("AWS4-HMAC-SHA256\n");

  DPL_APPEND_STR(date_str);
  DPL_APPEND_STR("\n");

  {
    int         ret;
    char        date_buf[9];

    ret = strftime(date_buf, sizeof(date_buf), "%Y%m%d", tm);
    if (ret == 0)
      return DPL_FAILURE;

    DPL_APPEND_STR(date_buf);
    DPL_APPEND_STR("/");
    DPL_APPEND_STR(req->ctx->aws_region);
    DPL_APPEND_STR("/s3/aws4_request\n");
  }

  {
    uint8_t     digest[SHA256_DIGEST_LENGTH];
    char        digest_hex[DPL_HEX_LENGTH(sizeof digest) + 1];
    int         i;

    dpl_sha256((uint8_t *) canonical_request, strlen(canonical_request), digest);
    for (i = 0; i < sizeof digest; i++)
      sprintf(digest_hex + 2 * i, "%02x", (u_char) digest[i]);

    DPL_APPEND_STR(digest_hex);
  }

  return DPL_SUCCESS;
}

static dpl_status_t
create_signature(const dpl_req_t *req, struct tm *tm,
                 char *sign_request, char *signature)
{
  char          digest[64];
  char          date_buf[9];
  unsigned int  digest_len, i;
  int           ret;

  ret = strftime(date_buf, sizeof(date_buf), "%Y%m%d", tm);
  if (ret == 0)
    return DPL_FAILURE;

  digest_len = snprintf(digest, sizeof(digest), "AWS4%s", req->ctx->secret_key);

#define H(d) \
  digest_len = dpl_hmac_sha256(digest, digest_len, d, strlen(d), digest)

  H(date_buf);
  H(req->ctx->aws_region);
  H("s3");
  H("aws4_request");
  H(sign_request);

  for (i = 0; i < SHA256_DIGEST_LENGTH; i++)
    sprintf(signature + 2 * i, "%02x", (u_char) digest[i]);

  return DPL_SUCCESS;
}

static dpl_vec_t *
get_canonical_headers(dpl_dict_t *headers)
{
  int                 bucket;
  dpl_vec_t           *canonical_headers;
  dpl_dict_var_t      *header;

  canonical_headers = dpl_vec_new(2, 2);
  if (canonical_headers == NULL)
    return NULL;

  for (bucket = 0; bucket < headers->n_buckets; bucket++) {
    header = headers->buckets[bucket];
    while (header != NULL) {
      dpl_status_t    ret;

      assert(header->val->type == DPL_VALUE_STRING);
      
      ret = dpl_vec_add(canonical_headers, header);
      if (ret != DPL_SUCCESS) {
        dpl_vec_free(canonical_headers);
        return NULL;
      }

      header = header->prev;
    }
  }

  dpl_vec_sort(canonical_headers, var_cmp);

  return canonical_headers;
}

static dpl_status_t
create_authorization(const dpl_req_t *req, struct tm *tm,
                     dpl_vec_t *canonical_headers, char *signature,
                     char *p, unsigned int len)
{
  int   ret;
  char  date_buf[9];

  ret = strftime(date_buf, sizeof(date_buf), "%Y%m%d", tm);
  if (ret == 0)
    return DPL_FAILURE;

  DPL_APPEND_STR("AWS4-HMAC-SHA256");

  DPL_APPEND_STR(" ");

  DPL_APPEND_STR("Credential=");
  DPL_APPEND_STR(req->ctx->access_key);
  DPL_APPEND_STR("/");
  DPL_APPEND_STR(date_buf);
  DPL_APPEND_STR("/");
  DPL_APPEND_STR(req->ctx->aws_region);
  DPL_APPEND_STR("/s3/aws4_request");

  DPL_APPEND_STR(",");

  DPL_APPEND_STR("SignedHeaders=");
  {
    int                 item;
    char                *c;
    dpl_dict_var_t      *header;

    for (item = 0; item < canonical_headers->n_items; item++) {
      header = (dpl_dict_var_t *) dpl_vec_get(canonical_headers, item);

      if (item > 0)
        DPL_APPEND_STR(";");

      for (c = header->key; *c != '\0'; c++)
        DPL_APPEND_CHAR(tolower(*c));
    }
  }

  DPL_APPEND_STR(",");

  DPL_APPEND_STR("Signature=");
  DPL_APPEND_STR(signature);

  return DPL_SUCCESS;
}

static dpl_status_t
insert_query_params_in_vec(dpl_vec_t *params, const dpl_dict_t *query_params)
{
  int   bucket;

  for (bucket = 0; bucket < query_params->n_buckets; bucket++) {
    dpl_dict_var_t  *param = query_params->buckets[bucket];

    while (param != NULL) {
      size_t            new_size;
      dpl_dict_var_t    *new_param;
      dpl_status_t      ret;

      new_param = (dpl_dict_var_t *) malloc (sizeof (dpl_dict_var_t));
      if (new_param == NULL)
        return DPL_FAILURE;

      new_size = DPL_URL_LENGTH(strlen(param->key)) + 1;
      new_param->key = (char *) malloc(new_size);
      if (new_param->key == NULL) {
        free(new_param);
        return DPL_FAILURE;
      }

      dpl_url_encode(param->key, new_param->key);

      new_param->val = dpl_value_dup(param->val);
      if (new_param->val == NULL) {
        free(new_param->key);
        free(new_param);
        return DPL_FAILURE;
      }

      ret = dpl_sbuf_url_encode(new_param->val->string);
      if (ret != DPL_SUCCESS) {
        dpl_value_free(new_param->val);
        free(new_param->key);
        free(new_param);
        return DPL_FAILURE;
      }

      dpl_vec_add(params, new_param);

      param = param->prev;
    }
  }

  return DPL_SUCCESS;
}

static dpl_status_t
parse_query_params_from_subresource(dpl_vec_t *params, const char *subresource)
{
  // TODO: Parse subresource
  return DPL_SUCCESS;
}

static dpl_vec_t *
get_canonical_params(const dpl_req_t *req, const dpl_dict_t *query_params)
{
  dpl_status_t  ret = DPL_SUCCESS;
  dpl_vec_t     *params;

  params = dpl_vec_new(2, 2);
  if (params == NULL)
    return NULL;

  if (req->subresource != NULL)
    ret = parse_query_params_from_subresource(params, req->subresource);

  if (ret == DPL_SUCCESS && query_params != NULL)
    ret = insert_query_params_in_vec(params, query_params);

  if (ret != DPL_SUCCESS) {
    int item;

    for (item = 0; item < params->n_items; item++) {
      dpl_dict_var_t    *param = (dpl_dict_var_t *) dpl_vec_get(params, item);

      free(param->key);
      dpl_dict_var_free(param);
    }
    dpl_vec_free(params);
    
    return NULL;
  }

  dpl_vec_sort(params, var_cmp);

  return params;
}

dpl_status_t
dpl_s3_add_authorization_v4_to_headers(const dpl_req_t *req,
                                       dpl_dict_t *headers,
                                       const dpl_dict_t *query_params)
{
  int           item;
  dpl_status_t  ret;
  char          canonical_request[4096] = "";
  char          sign_request[1024] = "";
  char          signature[DPL_HEX_LENGTH(SHA256_DIGEST_LENGTH) + 1];
  char          authorization[1024] = "";
  char          date_str[32] = "";
  struct tm     tm;
  dpl_vec_t     *canonical_headers;
  dpl_vec_t     *canonical_params;

  ret = add_payload_signature_to_headers(req, headers);
  if (ret != DPL_SUCCESS)
    return ret;

  ret = get_current_utc_date(&tm, date_str, sizeof(date_str));
  if (ret != DPL_SUCCESS)
    return ret;

  ret = dpl_dict_add(headers, "x-amz-date", date_str, 0);
  if (ret != DPL_SUCCESS)
    return ret;

  canonical_headers = get_canonical_headers(headers);
  if (canonical_headers == NULL)
    return DPL_FAILURE;

  canonical_params = get_canonical_params(req, query_params);
  if (canonical_params == NULL) {
    dpl_vec_free(canonical_headers);
    return DPL_FAILURE;
  }

  ret = create_canonical_request(req, headers,
                                 canonical_headers, canonical_params,
                                 canonical_request, sizeof(canonical_request));

  if (ret == DPL_SUCCESS) {
    DPRINTF("Canonical request:\n%s\n", canonical_request);
    ret = create_sign_request(req, canonical_request, &tm, date_str,
                              sign_request, sizeof(sign_request));
  }

  if (ret == DPL_SUCCESS) {
    DPRINTF("Signing request:\n%s\n", sign_request);
    ret = create_signature(req, &tm, sign_request, signature);
  }

  if (ret == DPL_SUCCESS) {
    DPRINTF("Signature: %s\n", signature);
    ret = create_authorization(req, &tm, canonical_headers, signature,
                               authorization, sizeof(authorization));
  }

  if (ret == DPL_SUCCESS)
    ret = dpl_dict_add(headers, "Authorization", authorization, 0);

  for (item = 0; item < canonical_params->n_items; item++) {
    dpl_dict_var_t    *param = (dpl_dict_var_t *) dpl_vec_get(canonical_params, item);
    free(param->key);
    dpl_dict_var_free(param);
  }
  dpl_vec_free(canonical_params);
  dpl_vec_free(canonical_headers);

  return ret;
}
