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
add_metadata_to_headers(dpl_dict_t *metadata,
                        dpl_dict_t *headers)

{
  int bucket;
  dpl_dict_var_t *var;
  char header[dpl_header_size];
  int ret;

  for (bucket = 0;bucket < metadata->n_buckets;bucket++)
    {
      for (var = metadata->buckets[bucket];var;var = var->prev)
        {
          snprintf(header, sizeof (header), "%s%s", DPL_X_AMZ_META_PREFIX, var->key);

          assert(DPL_VALUE_STRING == var->val->type);
          ret = dpl_dict_add(headers, header, var->val->string, 0);
          if (DPL_SUCCESS != ret)
            {
              return DPL_FAILURE;
            }
        }
    }

  return DPL_SUCCESS;
}

static dpl_status_t
add_condition_to_headers(const dpl_condition_one_t *condition,
                         dpl_dict_t *headers,
                         int copy_source)
{
  int ret;
  char *header;

  if (condition->type == DPL_CONDITION_IF_MODIFIED_SINCE ||
      condition->type == DPL_CONDITION_IF_UNMODIFIED_SINCE)
    {
      char date_str[128];
      struct tm tm_buf;

      ret = strftime(date_str, sizeof (date_str), "%a, %d %b %Y %H:%M:%S GMT", gmtime_r(&condition->time, &tm_buf));
      if (0 == ret)
        return DPL_FAILURE;

      if (condition->type == DPL_CONDITION_IF_MODIFIED_SINCE)
        {
          if (1 == copy_source)
            header = "x-amz-copy-source-if-modified-since";
          else
            header = "If-Modified-Since";
          ret = dpl_dict_add(headers, header, date_str, 0);
          if (DPL_SUCCESS != ret)
            {
              return DPL_FAILURE;
            }
        }

      if (condition->type == DPL_CONDITION_IF_UNMODIFIED_SINCE)
        {
          if (1 == copy_source)
            header = "x-amz-copy-source-if-unmodified-since";
          else
            header = "If-Unodified-Since";
          ret = dpl_dict_add(headers, header, date_str, 0);
          if (DPL_SUCCESS != ret)
            {
              return DPL_FAILURE;
            }
        }
    }

  if (condition->type == DPL_CONDITION_IF_MATCH)
    {
      if (1 == copy_source)
        header = "x-amz-copy-source-if-match";
      else
        header = "If-Match";
      ret = dpl_dict_add(headers, header, condition->etag, 0);
      if (DPL_SUCCESS != ret)
        {
          return DPL_FAILURE;
        }
    }

  if (condition->type == DPL_CONDITION_IF_NONE_MATCH)
    {
      if (1 == copy_source)
        header = "x-amz-copy-source-if-none-match";
      else
        header = "If-None-Match";
      ret = dpl_dict_add(headers, header, condition->etag, 0);
      if (DPL_SUCCESS != ret)
        {
          return DPL_FAILURE;
        }
    }

  return DPL_SUCCESS;
}

static dpl_status_t
add_conditions_to_headers(const dpl_condition_t *condition,
                          dpl_dict_t *headers,
                          int copy_source)
{
  int i;
  dpl_status_t ret, ret2;

  for (i = 0;i < condition->n_conds;i++)
    {
      ret2 = add_condition_to_headers(&condition->conds[i], headers, copy_source);
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

static int
var_cmp(const void *p1,
        const void *p2)
{
  dpl_value_t *val1 = *(dpl_value_t **) p1;
  dpl_value_t *val2 = *(dpl_value_t **) p2;
  dpl_dict_var_t *var1 = (dpl_dict_var_t *) val1->ptr;
  dpl_dict_var_t *var2 = (dpl_dict_var_t *) val2->ptr;

  assert(var1->val->type == DPL_VALUE_STRING);
  assert(var2->val->type == DPL_VALUE_STRING);

  return strcmp(var1->key, var2->key);
}

static dpl_status_t
make_signature(dpl_ctx_t *ctx,
               const char *method,
               const char *bucket,
               const char *resource,
               const char *subresource,
               dpl_dict_t *headers,
               char *buf,
               unsigned int len,
               unsigned int *lenp)
{
  char *p;
  //char *tmp_str;
  char *value;
  int ret;

  p = buf;

  //method
  DPL_APPEND_STR(method);
  DPL_APPEND_STR("\n");

  //md5
  value = dpl_dict_get_value(headers, "Content-MD5");
  if (NULL != value)
    DPL_APPEND_STR(value);
  DPL_APPEND_STR("\n");

  //content type
  value = dpl_dict_get_value(headers, "Content-Type");
  if (NULL != value)
    DPL_APPEND_STR(value);
  DPL_APPEND_STR("\n");

  //expires or date
  value = dpl_dict_get_value(headers, "Expires");
  if (NULL != value)
    {
      DPL_APPEND_STR(value);
    }
  else
    {
      value = dpl_dict_get_value(headers, "Date");
      if (NULL != value)
        DPL_APPEND_STR(value);
    }
  DPL_APPEND_STR("\n");

  //x-amz headers
  {
    int bucket;
    dpl_dict_var_t *var;
    dpl_vec_t *vec;
    int i;

    vec = dpl_vec_new(2, 2);
    if (NULL == vec)
      return DPL_ENOMEM;

    for (bucket = 0;bucket < headers->n_buckets;bucket++)
      {
        for (var = headers->buckets[bucket];var;var = var->prev)
          {
            if (!strncmp(var->key, "x-amz-", 6))
              {
                assert(DPL_VALUE_STRING == var->val->type);
                DPRINTF("%s: %s\n", var->key, var->val->string);
                ret = dpl_vec_add(vec, var);
                if (DPL_SUCCESS != ret)
                  {
                    dpl_vec_free(vec);
                    return DPL_FAILURE;
                  }
              }
          }
      }

    dpl_vec_sort(vec, var_cmp);

    for (i = 0;i < vec->n_items;i++)
      {
        var = (dpl_dict_var_t *) dpl_vec_get(vec, i);

        assert(DPL_VALUE_STRING == var->val->type);
        DPRINTF("%s:%s\n", var->key, var->val->string);
        DPL_APPEND_STR(var->key);
        DPL_APPEND_STR(":");
        DPL_APPEND_STR(var->val->string);
        DPL_APPEND_STR("\n");
      }

    dpl_vec_free(vec);
  }

  //resource

  if (NULL != bucket)
    {
      DPL_APPEND_STR("/");
      DPL_APPEND_STR(bucket);
    }

  if (NULL != resource)
    {
      //DPL_APPEND_STR("/");
      DPL_APPEND_STR(resource);
    }

  if (NULL != subresource)
    {
      DPL_APPEND_STR("?");
      DPL_APPEND_STR(subresource);
    }

  if (NULL != lenp)
    *lenp = p - buf;

  return DPL_SUCCESS;
}

static dpl_status_t
add_date_to_headers(dpl_dict_t *headers)
{
  int ret, ret2;
  time_t t;
  struct tm tm_buf;
  char date_str[128];

  (void) time(&t);
  ret = strftime(date_str, sizeof (date_str), "%a, %d %b %Y %H:%M:%S GMT", gmtime_r(&t, &tm_buf));
  if (0 == ret)
    return DPL_FAILURE;

  ret2 = dpl_dict_add(headers, "Date", date_str, 0);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  ret = DPL_SUCCESS;

 end:

  return ret;
}

static dpl_status_t
add_authorization_to_headers(const dpl_req_t *req,
                             dpl_dict_t *headers)
{
  int ret, ret2;
  const char *method = dpl_method_str(req->method);
  char resource_ue[DPL_URL_LENGTH(strlen(req->resource)) + 1];
  char sign_str[1024];
  u_int sign_len;
  char hmac_str[1024];
  u_int hmac_len;
  char base64_str[1024];
  u_int base64_len;
  char auth_str[1024];

  //resource
  if ('/' != req->resource[0])
    {
      resource_ue[0] = '/';
      dpl_url_encode_no_slashes(req->resource, resource_ue + 1);
    }
  else
    {
      dpl_url_encode_no_slashes(req->resource, resource_ue);
    }

  ret = make_signature(req->ctx, method, req->bucket, resource_ue, req->subresource, headers, sign_str, sizeof (sign_str), &sign_len);
  if (DPL_SUCCESS != ret)
    return DPL_FAILURE;

  DPL_TRACE(req->ctx, DPL_TRACE_REQ, "stringtosign=%.*s", sign_len, sign_str);

  hmac_len = dpl_hmac_sha1(req->ctx->secret_key, strlen(req->ctx->secret_key), sign_str, sign_len, hmac_str);

  base64_len = dpl_base64_encode((const u_char *) hmac_str, hmac_len, (u_char *) base64_str);

  snprintf(auth_str, sizeof (auth_str), "AWS %s:%.*s", req->ctx->access_key, base64_len, base64_str);

  ret2 = dpl_dict_add(headers, "Authorization", auth_str, 0);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  ret = DPL_SUCCESS;

 end:

  return ret;
}

static dpl_status_t
add_source_to_headers(const dpl_req_t *req,
                      dpl_dict_t *headers)
{
  int ret, ret2;
  char buf[1024];
  u_int len = sizeof (buf);
  char *p;
  char src_resource_ue[DPL_URL_LENGTH(strlen(req->src_resource)) + 1];

  //resource
  if ('/' != req->src_resource[0])
    {
      src_resource_ue[0] = '/';
      dpl_url_encode(req->src_resource, src_resource_ue + 1);
    }
  else
    {
      src_resource_ue[0] = '/'; //some servers do not like encoded slash
      dpl_url_encode(req->src_resource + 1, src_resource_ue + 1);
    }

  p = buf;

  DPL_APPEND_STR("/");
  DPL_APPEND_STR(req->src_bucket);
  DPL_APPEND_STR(src_resource_ue);

  //subresource and query params
  if (NULL != req->src_subresource)
    {
      DPL_APPEND_STR("?");
      DPL_APPEND_STR(req->src_subresource);
    }

  DPL_APPEND_CHAR(0);

  ret2 = dpl_dict_add(headers, "x-amz-copy-source", buf, 0);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  ret = DPL_SUCCESS;

 end:

  return ret;
}

/**
 * build headers from request
 *
 * @param req
 * @param headersp
 *
 * @return
 */
dpl_status_t
dpl_s3_req_build(const dpl_req_t *req,
                 dpl_s3_req_mask_t req_mask,
                 dpl_dict_t **headersp)
{
  dpl_dict_t *headers = NULL;
  int ret, ret2;
  const char *method = dpl_method_str(req->method);
  char resource_ue[DPL_URL_LENGTH(strlen(req->resource)) + 1];

  DPL_TRACE(req->ctx, DPL_TRACE_REQ, "req_build method=%s bucket=%s resource=%s subresource=%s", method, req->bucket, req->resource, req->subresource);

  //resource
  if ('/' != req->resource[0])
    {
      resource_ue[0] = '/';
      dpl_url_encode(req->resource, resource_ue + 1);
    }
  else
    {
      resource_ue[0] = '/'; //some servers do not like encoded slash
      dpl_url_encode(req->resource + 1, resource_ue + 1);
    }

  headers = dpl_dict_new(13);
  if (NULL == headers)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  /*
   * per method headers
   */
  if (DPL_METHOD_GET == req->method ||
      DPL_METHOD_HEAD == req->method)
    {
      if (req->range_enabled)
        {
          ret2 = dpl_add_range_to_headers(&req->range, headers);
          if (DPL_SUCCESS != ret2)
            {
              ret = ret2;
              goto end;
            }
        }

      ret2 = add_conditions_to_headers(&req->condition, headers, 0);
      if (DPL_SUCCESS != ret2)
        {
          ret = ret2;
          goto end;
        }
    }
  else if (DPL_METHOD_PUT == req->method)
    {
      char buf[64];

      if (NULL != req->cache_control)
        {
          ret2 = dpl_dict_add(headers, "Cache-Control", req->cache_control, 0);
          if (DPL_SUCCESS != ret2)
            {
              ret = ret2;
              goto end;
            }
        }

      if (NULL != req->content_disposition)
        {
          ret2 = dpl_dict_add(headers, "Content-Disposition", req->content_disposition, 0);
          if (DPL_SUCCESS != ret2)
            {
              ret = ret2;
              goto end;
            }
        }

      if (NULL != req->content_encoding)
        {
          ret2 = dpl_dict_add(headers, "Content-Encoding", req->content_encoding, 0);
          if (DPL_SUCCESS != ret2)
            {
              ret = ret2;
              goto end;
            }
        }

      if (req->behavior_flags & DPL_BEHAVIOR_MD5)
        {
          MD5_CTX ctx;
          u_char digest[MD5_DIGEST_LENGTH];
          char b64_digest[DPL_BASE64_LENGTH(MD5_DIGEST_LENGTH) + 1];
          u_int b64_digest_len;

          if (!req->data_enabled)
            {
              ret = DPL_EINVAL;
              goto end;
            }

          MD5_Init(&ctx);
          MD5_Update(&ctx, req->data_buf, req->data_len);
          MD5_Final(digest, &ctx);

          b64_digest_len = dpl_base64_encode(digest, MD5_DIGEST_LENGTH, (u_char *) b64_digest);
          b64_digest[b64_digest_len] = 0;

          ret2 = dpl_dict_add(headers, "Content-MD5", b64_digest, 0);
          if (DPL_SUCCESS != ret2)
            {
              ret = ret2;
              goto end;
            }
        }

      if (req->data_enabled)
        {
          snprintf(buf, sizeof (buf), "%u", req->data_len);
          ret2 = dpl_dict_add(headers, "Content-Length", buf, 0);
          if (DPL_SUCCESS != ret2)
            {
              ret = DPL_ENOMEM;
              goto end;
            }
        }

      if (NULL != req->content_type)
        {
          ret2 = dpl_dict_add(headers, "Content-Type", req->content_type, 0);
          if (DPL_SUCCESS != ret2)
            {
              ret = ret2;
              goto end;
            }
        }

      if (req->behavior_flags & DPL_BEHAVIOR_EXPECT)
        {
          ret2 = dpl_dict_add(headers, "Expect", "100-continue", 0);
          if (DPL_SUCCESS != ret2)
            {
              ret = DPL_ENOMEM;
              goto end;
            }
        }

      if (DPL_CANNED_ACL_UNDEF != req->canned_acl)
        {
          char *str;

          str = dpl_canned_acl_str(req->canned_acl);
          if (NULL == str)
            {
              ret = DPL_FAILURE;
              goto end;
            }

          ret2 = dpl_dict_add(headers, "x-amz-acl", str, 0);
          if (DPL_SUCCESS != ret2)
            {
              ret = DPL_ENOMEM;
              goto end;
            }
        }

      ret2 = add_metadata_to_headers(req->metadata, headers);
      if (DPL_SUCCESS != ret2)
        {
          ret = ret2;
          goto end;
        }

      if (DPL_STORAGE_CLASS_UNDEF != req->storage_class)
        {
          char *str;

          str = dpl_storage_class_str(req->storage_class);
          if (NULL == str)
            {
              ret = DPL_FAILURE;
              goto end;
            }

          ret2 = dpl_dict_add(headers, "x-amz-storage-class", str, 0);
          if (DPL_SUCCESS != ret2)
            {
              ret = DPL_ENOMEM;
              goto end;
            }
        }

      /*
       * copy
       */
      if (req_mask & DPL_S3_REQ_COPY)
        {
          ret2 = add_source_to_headers(req, headers);
          if (DPL_SUCCESS != ret2)
            {
              ret = ret2;
              goto end;
            }

          if (DPL_COPY_DIRECTIVE_UNDEF != req->copy_directive)
            {
              char *str;

              switch (req->copy_directive)
                {
                case DPL_COPY_DIRECTIVE_UNDEF:
                case DPL_COPY_DIRECTIVE_COPY:
                case DPL_COPY_DIRECTIVE_LINK:
                case DPL_COPY_DIRECTIVE_SYMLINK:
                case DPL_COPY_DIRECTIVE_MOVE:
                case DPL_COPY_DIRECTIVE_MKDENT:
                case DPL_COPY_DIRECTIVE_RMDENT:
                case DPL_COPY_DIRECTIVE_MVDENT:
                  ret = DPL_ENOTSUPP;
                  goto end;
                case DPL_COPY_DIRECTIVE_METADATA_REPLACE:
                  str = "REPLACE";
                  break ;
                }

              ret2 = dpl_dict_add(headers, "x-amz-metadata-directive", str, 0);
              if (DPL_SUCCESS != ret2)
                {
                  ret = DPL_ENOMEM;
                  goto end;
                }
            }

          ret2 = add_conditions_to_headers(&req->copy_source_condition, headers, 1);
          if (DPL_SUCCESS != ret2)
            {
              ret = ret2;
              goto end;
            }
        }
    }
  else if (DPL_METHOD_DELETE == req->method)
    {
      //XXX todo x-amz-mfa
    }
  else
    {
      ret = DPL_EINVAL;
      goto end;
    }

  /*
   * common headers
   */
  if (req->behavior_flags & DPL_BEHAVIOR_KEEP_ALIVE)
    {
      ret2 = dpl_dict_add(headers, "Connection", "keep-alive", 0);
      if (DPL_SUCCESS != ret2)
        {
          ret = DPL_ENOMEM;
          goto end;
        }
    }

  if (req->behavior_flags & DPL_BEHAVIOR_QUERY_STRING)
    {
      char str[128];

      snprintf(str, sizeof (str), "%ld", req->expires);
      ret2 = dpl_dict_add(headers, "Expires", str, 0);
      if (DPL_SUCCESS != ret2)
        {
          ret = DPL_ENOMEM;
          goto end;
        }
    }
  else
    {
      ret2 = add_date_to_headers(headers);
      if (DPL_SUCCESS != ret2)
        {
          ret = ret2;
          goto end;
        }
    }

  ret2 = add_authorization_to_headers(req, headers);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  if (NULL != headersp)
    {
      *headersp = headers;
      headers = NULL; //consume it
    }

  ret = DPL_SUCCESS;

 end:

  if (NULL != headers)
    dpl_dict_free(headers);

  return ret;
}

dpl_status_t
dpl_s3_req_gen_url(const dpl_req_t *req,
                   dpl_dict_t *headers,
                   char *buf,
                   int len,
                   unsigned int *lenp)
{
  int ret, ret2;
  char *p;
  char *host;
  char *method = dpl_method_str(req->method);
  char resource_ue[DPL_URL_LENGTH(strlen(req->resource)) + 1];
  char sign_str[1024];
  u_int sign_len;
  char hmac_str[1024];
  u_int hmac_len;
  char base64_str[1024];
  u_int base64_len;
  char base64_ue_str[1024];
  char str[128];

  DPL_TRACE(req->ctx, DPL_TRACE_REQ, "req_gen_query_string");

  p = buf;

  if (1 == req->ctx->use_https)
    {
      DPL_APPEND_STR("https");
    }
  else
    {
      DPL_APPEND_STR("http");
    }

  DPL_APPEND_STR("://");

  host = dpl_dict_get_value(headers, "Host");
  if (NULL == host)
    {
      ret = DPL_FAILURE;
      goto end;
    }

  DPL_APPEND_STR(host);

  if (((1 == req->ctx->use_https) && (strcmp(req->port, "443"))) ||
      ((0 == req->ctx->use_https) && (strcmp(req->port, "80"))))
    {
      snprintf(str, sizeof(str), ":%s", req->port);
      DPL_APPEND_STR(str);
    }
  
  //resource
  if ('/' != req->resource[0])
    {
      resource_ue[0] = '/';
      dpl_url_encode(req->resource, resource_ue + 1);
    }
  else
    {
      resource_ue[0] = '/'; //some servers do not like encoded slash
      dpl_url_encode(req->resource + 1, resource_ue + 1);
    }

  DPL_APPEND_STR(resource_ue);

  DPL_APPEND_STR("?");

  DPL_APPEND_STR("AWSAccessKeyId=");
  DPL_APPEND_STR(req->ctx->access_key);

  DPL_APPEND_STR("&");

  DPL_APPEND_STR("Signature=");
  ret2 = make_signature(req->ctx, method, req->bucket, resource_ue, req->subresource, headers, sign_str, sizeof (sign_str), &sign_len);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  DPL_TRACE(req->ctx, DPL_TRACE_REQ, "stringtosign=%.*s", sign_len, sign_str);

  hmac_len = dpl_hmac_sha1(req->ctx->secret_key, strlen(req->ctx->secret_key), sign_str, sign_len, hmac_str);

  base64_len = dpl_base64_encode((const u_char *) hmac_str, hmac_len, (u_char *) base64_str);

  base64_str[base64_len] = 0; //XXX

  dpl_url_encode(base64_str, base64_ue_str);

  DPL_APPEND_STR(base64_ue_str);

  DPL_APPEND_STR("&");

  DPL_APPEND_STR("Expires=");
  snprintf(str, sizeof (str), "%ld", req->expires);
  DPL_APPEND_STR(str);

  if (NULL != lenp)
    *lenp = (p - buf);

  ret = DPL_SUCCESS;

 end:

  return ret;
}
