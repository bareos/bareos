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

#define APPEND_STR(Str)                                         \
  do                                                            \
    {                                                           \
    tmp_len = strlen((Str));                                    \
    if (len < tmp_len)                                          \
      return DPL_FAILURE;                                       \
    memcpy(p, (Str), tmp_len); p += tmp_len; len -= tmp_len;    \
    } while (0);

static int
var_cmp(const void *p1,
        const void *p2)
{
  dpl_var_t *var1 = *(dpl_var_t **) p1;
  dpl_var_t *var2 = *(dpl_var_t **) p2;

  return strcmp(var1->key, var2->key);
}

dpl_status_t
dpl_build_s3_signature(dpl_ctx_t *ctx,
                       char *method,
                       char *bucket,
                       char *resource_ue, 
                       char *subresource_ue,
                       dpl_dict_t *headers,
                       char *buf, 
                       u_int len,
                       u_int *lenp)
{
  char *p;
  //char *tmp_str;
  u_int tmp_len;
  char *value;
  int ret;
  
  p = buf;

  //method
  APPEND_STR(method);
  APPEND_STR("\n");

  //md5
  value = dpl_dict_get_value(headers, "Content-MD5");
  if (NULL != value)
    APPEND_STR(value);
  APPEND_STR("\n");

  //content type
  value = dpl_dict_get_value(headers, "Content-Type");
  if (NULL != value)
    APPEND_STR(value);
  APPEND_STR("\n");

  //date
  value = dpl_dict_get_value(headers, "Date");
  if (NULL != value)
    APPEND_STR(value);
  APPEND_STR("\n");

  //x-amz headers
  {
    int bucket;
    dpl_var_t *var;
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
                DPRINTF("%s: %s\n", var->key, var->value);
                ret = dpl_vec_add(vec, var);
                if (DPL_SUCCESS != ret)
                  {
                    dpl_vec_free(vec);
                    return DPL_FAILURE;
                  }
              }
          }
      }

    qsort(vec->array, vec->n_items, sizeof (var), var_cmp);

    for (i = 0;i < vec->n_items;i++)
      {
        var = (dpl_var_t *) vec->array[i];
        
        DPRINTF("%s:%s\n", var->key, var->value);
        APPEND_STR(var->key);
        APPEND_STR(":");
        APPEND_STR(var->value);
        APPEND_STR("\n");
      }

    dpl_vec_free(vec);
  }

  //resource

  if (NULL != bucket)
    {
      APPEND_STR("/");
      APPEND_STR(bucket);
    }

  if (NULL != resource_ue)
    APPEND_STR(resource_ue);

  if (NULL != subresource_ue)
    {
      APPEND_STR("?");
      APPEND_STR(subresource_ue);
    }

  if (NULL != lenp)
    *lenp = p - buf;

  return DPL_SUCCESS;
}

/** 
 * build a valid S3 request
 * 
 * @param ctx 
 * @param method "GET", "PUT", "DELETE", etc
 * @param bucket some hosters do not like '_' in bucket names
 * @param resource object name
 * @param subresource e.g. "acl", "location", ...
 * @param query_params e.g. "prefix"
 * @param headers additional headers
 * @param buf buffer where to store build request
 * @param len maximum length
 * @param lenp len returned, might be NULL
 * 
 * @return 
 */
dpl_status_t
dpl_build_s3_request(dpl_ctx_t *ctx,
                     char *method,
                     char *bucket,
                     char *resource,
                     char *subresource,
                     dpl_dict_t *query_params,
                     dpl_dict_t *headers,
                     char *buf,
                     u_int len,
                     u_int *lenp)
{
  char *p;
  u_int tmp_len;
  int ret;
  char *resource_ue = NULL;
  char *subresource_ue = NULL;

  DPL_TRACE(ctx, DPL_TRACE_S3, "method=%s bucket=%s resource=%s subresource=%s", method, bucket, resource, subresource);

  p = buf;

  //method
  APPEND_STR(method);

  APPEND_STR(" ");

  //resource
  if ('/' != resource[0])
    {
      resource_ue = alloca(DPL_URL_LENGTH(strlen(resource)) + 1);
      resource_ue[0] = '/';
      dpl_url_encode(resource, resource_ue + 1);
    }
  else
    {
      resource_ue = alloca(DPL_URL_LENGTH(strlen(resource)) + 1);
      resource_ue[0] = '/'; //some servers do not like encoded slash
      dpl_url_encode(resource + 1, resource_ue + 1);
    }
  APPEND_STR(resource_ue);

  //subresource and query params
  if (NULL != subresource || NULL != query_params)
    APPEND_STR("?");

  if (NULL != subresource)
    {
      subresource_ue = alloca(DPL_URL_LENGTH(strlen(subresource)));
      dpl_url_encode(subresource, subresource_ue);
      APPEND_STR(subresource);
    }

  if (NULL != query_params)
    {
      int bucket;
      dpl_var_t *var;
      int amp = 0;

      if (NULL != subresource)
        amp = 1;
      
      for (bucket = 0;bucket < query_params->n_buckets;bucket++)
        {
          for (var = query_params->buckets[bucket];var;var = var->prev)
            {
              if (amp)
                APPEND_STR("&");
              APPEND_STR(var->key);
              APPEND_STR("=");
              APPEND_STR(var->value);
              amp = 1;
            }
        }
    }

  APPEND_STR(" ");

  //version
  APPEND_STR("HTTP/1.1");
  APPEND_STR("\r\n");

  //Host, Expect, Connection are set by caller

  //date
  {
    time_t t;
    struct tm tm_buf;
    char date_str[128];

    (void) time(&t);
    ret = strftime(date_str, sizeof (date_str), "%a, %d %b %Y %H:%M:%S GMT", gmtime_r(&t, &tm_buf));
    if (0 == ret)
      return DPL_FAILURE;
    ret = dpl_dict_add(headers, "Date", date_str, 0);
    if (DPL_SUCCESS != ret)
      return DPL_FAILURE;
  }

  //authorization
  {
    char sign_str[1024];
    u_int sign_len;
    char hmac_str[1024];
    u_int hmac_len;
    char base64_str[1024];
    u_int base64_len;
    char auth_str[1024];

    ret = dpl_build_s3_signature(ctx, method, bucket, resource_ue, subresource_ue, headers, sign_str, sizeof (sign_str), &sign_len);
    if (DPL_SUCCESS != ret)
      return DPL_FAILURE;

    if (ctx->trace_level & DPL_TRACE_S3)
      dpl_dump_simple(sign_str, sign_len);
    
    hmac_len = dpl_hmac_sha1(ctx->secret_key, strlen(ctx->secret_key), sign_str, sign_len, hmac_str);
    
    base64_len = dpl_base64_encode((u_char *) hmac_str, hmac_len, base64_str);
    
    snprintf(auth_str, sizeof (auth_str), "AWS %s:%.*s", ctx->access_key, base64_len, base64_str);
    ret = dpl_dict_add(headers, "Authorization", auth_str, 0);
    if (DPL_SUCCESS != ret)
      return DPL_FAILURE;
  }

  //headers
  if (NULL != headers)
    {
      int bucket;
      dpl_var_t *var;
      
      for (bucket = 0;bucket < headers->n_buckets;bucket++)
        {
          for (var = headers->buckets[bucket];var;var = var->prev)
            {
              APPEND_STR(var->key);
              APPEND_STR(": ");
              APPEND_STR(var->value);
              APPEND_STR("\r\n");
            }
        }
    }
  
  //final crlf managed by caller

  if (NULL != lenp)
    *lenp = (p - buf);

  return DPL_SUCCESS;
}
