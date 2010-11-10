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

void
dpl_req_free(dpl_req_t *req)
{
  if (NULL != req->bucket)
    free(req->bucket);

  if (NULL != req->resource)
    free(req->resource);

  if (NULL != req->subresource)
    free(req->subresource);

  if (NULL != req->cache_control)
    free(req->cache_control);

  if (NULL != req->content_disposition)
    free(req->content_disposition);

  if (NULL != req->content_encoding)
    free(req->content_encoding);

  if (NULL != req->content_type)
    free(req->content_type);

  if (NULL != req->metadata)
    dpl_dict_free(req->metadata);

  if (NULL != req->src_bucket)
    free(req->src_bucket);

  if (NULL != req->src_resource)
    free(req->src_resource);

  if (NULL != req->src_subresource)
    free(req->src_subresource);

  free(req);
}

dpl_req_t *
dpl_req_new(dpl_ctx_t *ctx)
{
  dpl_req_t *req = NULL;
  
  req = malloc(sizeof (*req));
  if (NULL == req)
    goto bad;

  memset(req, 0, sizeof (*req));

  req->ctx = ctx;

  req->metadata = dpl_dict_new(13);
  if (NULL == req->metadata)
    goto bad;

  //virtual hosting is prefered since it "disperses" connections
  req->behavior_flags = DPL_BEHAVIOR_KEEP_ALIVE|DPL_BEHAVIOR_VIRTUAL_HOSTING;

  return req;

 bad:

  if (NULL != req)
    dpl_req_free(req);

  return NULL;
}

void
dpl_req_set_method(dpl_req_t *req,
                   dpl_method_t method)
{
  req->method = method;
}

dpl_status_t 
dpl_req_set_bucket(dpl_req_t *req,
                   char *bucket)
{
  char *nstr;

  nstr = strdup(bucket);
  if (NULL == nstr)
    return DPL_ENOMEM;

  if (NULL != req->bucket)
    free(req->bucket);
  
  req->bucket = nstr;

  return DPL_SUCCESS;
}

dpl_status_t 
dpl_req_set_resource(dpl_req_t *req,
                     char *resource)
{
  char *nstr;

  nstr = strdup(resource);
  if (NULL == nstr)
    return DPL_ENOMEM;

  if (NULL != req->resource)
    free(req->resource);
  
  req->resource = nstr;

  return DPL_SUCCESS;
}

dpl_status_t 
dpl_req_set_subresource(dpl_req_t *req,
                        char *subresource)
{
  char *nstr;

  nstr = strdup(subresource);
  if (NULL == nstr)
    return DPL_ENOMEM;

  if (NULL != req->subresource)
    free(req->subresource);
  
  req->subresource = nstr;

  return DPL_SUCCESS;
}

void
dpl_req_add_behavior(dpl_req_t *req,
                     u_int flags)
{
  req->behavior_flags |= flags;
}

void
dpl_req_rm_behavior(dpl_req_t *req,
                    u_int flags)
{
  req->behavior_flags &= ~flags;
}

void
dpl_req_set_location_constraint(dpl_req_t *req,
                                dpl_location_constraint_t location_constraint)
{
  req->location_constraint = location_constraint;
}

void
dpl_req_set_canned_acl(dpl_req_t *req,
                       dpl_canned_acl_t canned_acl)
{
  req->canned_acl = canned_acl;
}

void
dpl_req_set_storage_class(dpl_req_t *req,
                          dpl_storage_class_t storage_class)
{
  req->storage_class = storage_class;
}

void
dpl_req_set_condition(dpl_req_t *req,
                      dpl_condition_t *condition)
{
  req->condition = *condition;
}

dpl_status_t 
dpl_req_set_cache_control(dpl_req_t *req,
                        char *cache_control)
{
  char *nstr;

  nstr = strdup(cache_control);
  if (NULL == nstr)
    return DPL_ENOMEM;

  if (NULL != req->cache_control)
    free(req->cache_control);
  
  req->cache_control = nstr;

  return DPL_SUCCESS;
}

dpl_status_t 
dpl_req_set_content_disposition(dpl_req_t *req,
                                char *content_disposition)
{
  char *nstr;

  nstr = strdup(content_disposition);
  if (NULL == nstr)
    return DPL_ENOMEM;

  if (NULL != req->content_disposition)
    free(req->content_disposition);
  
  req->content_disposition = nstr;

  return DPL_SUCCESS;
}

dpl_status_t 
dpl_req_set_content_encoding(dpl_req_t *req,
                             char *content_encoding)
{
  char *nstr;

  nstr = strdup(content_encoding);
  if (NULL == nstr)
    return DPL_ENOMEM;

  if (NULL != req->content_encoding)
    free(req->content_encoding);
  
  req->content_encoding = nstr;

  return DPL_SUCCESS;
}

void
dpl_req_set_chunk(dpl_req_t *req,
                  dpl_chunk_t *chunk)
{
  req->chunk = chunk;
}

dpl_status_t
dpl_req_add_metadatum(dpl_req_t *req,
                      char *key,
                      char *value)
{
  return dpl_dict_add(req->metadata, key, value, 0);
}

dpl_status_t
dpl_req_add_metadata(dpl_req_t *req,
                     dpl_dict_t *metadata)
{
  int bucket;
  dpl_var_t *var;
  int ret;

  for (bucket = 0;bucket < metadata->n_buckets;bucket++)
    {
      for (var = metadata->buckets[bucket];var;var = var->prev)
        {
          ret = dpl_dict_add(req->metadata, var->key, var->value, 0);
          if (DPL_SUCCESS != ret)
            {
              return DPL_FAILURE;
            }
        }
    }

  return DPL_SUCCESS;
}

dpl_status_t 
dpl_req_set_content_type(dpl_req_t *req,
                         char *content_type)
{
  char *nstr;

  nstr = strdup(content_type);
  if (NULL == nstr)
    return DPL_ENOMEM;

  if (NULL != req->content_type)
    free(req->content_type);
  
  req->content_type = nstr;

  return DPL_SUCCESS;
}

dpl_status_t
dpl_req_add_range(dpl_req_t *req,
                  int start,
                  int end)
{
  if (req->n_ranges < DPL_RANGE_MAX)
    {
      req->ranges[req->n_ranges].start = start;
      req->ranges[req->n_ranges].end = end;

      req->n_ranges++;

      return DPL_SUCCESS;
    }

  return DPL_EINVAL;
}

void
dpl_req_set_expires(dpl_req_t *req,
                    time_t expires)
{
  req->expires = expires;
}

dpl_status_t 
dpl_req_set_src_bucket(dpl_req_t *req,
                       char *src_bucket)
{
  char *nstr;

  nstr = strdup(src_bucket);
  if (NULL == nstr)
    return DPL_ENOMEM;

  if (NULL != req->src_bucket)
    free(req->src_bucket);
  
  req->src_bucket = nstr;

  return DPL_SUCCESS;
}

dpl_status_t 
dpl_req_set_src_resource(dpl_req_t *req,
                         char *src_resource)
{
  char *nstr;

  nstr = strdup(src_resource);
  if (NULL == nstr)
    return DPL_ENOMEM;

  if (NULL != req->src_resource)
    free(req->src_resource);
  
  req->src_resource = nstr;

  return DPL_SUCCESS;
}

dpl_status_t 
dpl_req_set_src_subresource(dpl_req_t *req,
                            char *src_subresource)
{
  char *nstr;

  nstr = strdup(src_subresource);
  if (NULL == nstr)
    return DPL_ENOMEM;

  if (NULL != req->src_subresource)
    free(req->src_subresource);
  
  req->src_subresource = nstr;

  return DPL_SUCCESS;
}

void
dpl_req_set_metadata_directive(dpl_req_t *req,
                               dpl_metadata_directive_t metadata_directive)
{
  req->metadata_directive = metadata_directive;
}

void
dpl_req_set_copy_source_condition(dpl_req_t *req,
                                  dpl_condition_t *condition)
{
  req->copy_source_condition = *condition;
}

/*
 * builder
 */

static dpl_status_t
dpl_add_metadata_to_headers(dpl_dict_t *metadata,
                            dpl_dict_t *headers)
                            
{
  int bucket;
  dpl_var_t *var;
  char header[1024];
  int ret;

  for (bucket = 0;bucket < metadata->n_buckets;bucket++)
    {
      for (var = metadata->buckets[bucket];var;var = var->prev)
        {
          snprintf(header, sizeof (header), "x-amz-meta-%s", var->key);

          ret = dpl_dict_add(headers, header, var->value, 0);
          if (DPL_SUCCESS != ret)
            {
              return DPL_FAILURE;
            }
        }
    }

  return DPL_SUCCESS;
}

static dpl_status_t
dpl_add_condition_to_headers(dpl_condition_t *condition,
                             dpl_dict_t *headers,
                             int copy_source)
{
  int ret;
  char *header;

  if (condition->mask & (DPL_CONDITION_IF_MODIFIED_SINCE|
                         DPL_CONDITION_IF_UNMODIFIED_SINCE))
    {
      char date_str[128];
      struct tm tm_buf;

      ret = strftime(date_str, sizeof (date_str), "%a, %d %b %Y %H:%M:%S GMT", gmtime_r(&condition->time, &tm_buf));
      if (0 == ret)
        return DPL_FAILURE;
      
      if (condition->mask & DPL_CONDITION_IF_MODIFIED_SINCE)
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
      
      if (condition->mask & DPL_CONDITION_IF_UNMODIFIED_SINCE)
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

  if (condition->mask & DPL_CONDITION_IF_MATCH)
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

  if (condition->mask & DPL_CONDITION_IF_NONE_MATCH)
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
dpl_add_ranges_to_headers(dpl_range_t *ranges, 
                          int n_ranges,
                          dpl_dict_t *headers)
{
  int ret;
  int i;
  char buf[1024];
  int len = sizeof (buf);
  char *p;
  int first = 1;

  p = buf;

  if (0 != n_ranges)
    {
      DPL_APPEND_STR("bytes=");
      
      for (i = 0;i < n_ranges;i++)
        {
          char str[128];

          if (1 == first)
            first = 0;
          else
            DPL_APPEND_STR(",");

          if (DPL_UNDEF == ranges[i].start && DPL_UNDEF == ranges[i].end)
            return DPL_EINVAL;
          else if (DPL_UNDEF == ranges[i].start)
            {
              snprintf(str, sizeof (str), "-%d", ranges[i].end);
              DPL_APPEND_STR(str);
            }
          else if (DPL_UNDEF == ranges[i].end)
            {
              snprintf(str, sizeof (str), "%d-", ranges[i].start);
              DPL_APPEND_STR(str);
            }
          else
            {
              snprintf(str, sizeof (str), "%d-%d", ranges[i].start, ranges[i].end);
              DPL_APPEND_STR(str);
            }
        }

      DPL_APPEND_CHAR(0);
        
      ret = dpl_dict_add(headers, "Range", buf, 0);
      if (DPL_SUCCESS != ret)
        {
          return DPL_FAILURE;
        }
    }
  
  return DPL_SUCCESS;
}

static int
var_cmp(const void *p1,
        const void *p2)
{
  dpl_var_t *var1 = *(dpl_var_t **) p1;
  dpl_var_t *var2 = *(dpl_var_t **) p2;

  return strcmp(var1->key, var2->key);
}

static dpl_status_t
dpl_make_signature(dpl_ctx_t *ctx,
                   char *method,
                   char *bucket,
                   char *resource, 
                   char *subresource,
                   dpl_dict_t *headers,
                   char *buf, 
                   u_int len,
                   u_int *lenp)
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
        DPL_APPEND_STR(var->key);
        DPL_APPEND_STR(":");
        DPL_APPEND_STR(var->value);
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
dpl_add_date_to_headers(dpl_dict_t *headers)
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
dpl_add_authorization_to_headers(dpl_req_t *req,
                                 dpl_dict_t *headers)
{
  int ret, ret2;
  char *method = dpl_method_str(req->method);
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
      dpl_url_encode(req->resource, resource_ue + 1);
    }
  else
    {
      resource_ue[0] = '/'; //some servers do not like encoded slash
      dpl_url_encode(req->resource + 1, resource_ue + 1);
    }

  ret = dpl_make_signature(req->ctx, method, req->bucket, resource_ue, req->subresource, headers, sign_str, sizeof (sign_str), &sign_len);
  if (DPL_SUCCESS != ret)
    return DPL_FAILURE;
  
  DPL_TRACE(req->ctx, DPL_TRACE_REQ, "stringtosign=%.*s", sign_len, sign_str);
  
  hmac_len = dpl_hmac_sha1(req->ctx->secret_key, strlen(req->ctx->secret_key), sign_str, sign_len, hmac_str);
  
  base64_len = dpl_base64_encode((u_char *) hmac_str, hmac_len, base64_str);
  
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
dpl_add_source_to_headers(dpl_req_t *req,
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
dpl_req_build(dpl_req_t *req,
              dpl_dict_t **headersp)
{
  dpl_dict_t *headers = NULL;
  int ret, ret2;
  char *method = dpl_method_str(req->method);
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
      ret2 = dpl_add_ranges_to_headers(req->ranges, req->n_ranges, headers);
      if (DPL_SUCCESS != ret2)
        {
          ret = ret2;
          goto end;
        }

      ret2 = dpl_add_condition_to_headers(&req->condition, headers, 0);
      if (DPL_SUCCESS != ret2)
        {
          ret = ret2;
          goto end;
        }
    }
  else if (DPL_METHOD_PUT == req->method)
    {
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

          if (NULL == req->chunk)
            {
              ret = DPL_EINVAL;
              goto end;
            }

          MD5_Init(&ctx);
          MD5_Update(&ctx, req->chunk->buf, req->chunk->len);
          MD5_Final(digest, &ctx);
          
          b64_digest_len = dpl_base64_encode(digest, MD5_DIGEST_LENGTH, b64_digest);
          b64_digest[b64_digest_len] = 0;

          ret2 = dpl_dict_add(headers, "Content-MD5", b64_digest, 0);
          if (DPL_SUCCESS != ret2)
            {
              ret = ret2;
              goto end;
            }          
        }

      if (NULL != req->chunk)
        {
          char buf[64];
          
          snprintf(buf, sizeof (buf), "%u", req->chunk->len);
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

      ret2 = dpl_add_metadata_to_headers(req->metadata, headers);
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
      if (req->behavior_flags & DPL_BEHAVIOR_COPY)
        {
          ret2 = dpl_add_source_to_headers(req, headers);
          if (DPL_SUCCESS != ret2)
            {
              ret = ret2;
              goto end;
            }
          
          if (DPL_METADATA_DIRECTIVE_UNDEF != req->metadata_directive)
            {
              char *str;
              
              str = dpl_metadata_directive_str(req->metadata_directive);
              if (NULL == str)
                {
                  ret = DPL_FAILURE;
                  goto end;
                }
              
              ret2 = dpl_dict_add(headers, "x-amz-metadata-directive", str, 0);
              if (DPL_SUCCESS != ret2)
                {
                  ret = DPL_ENOMEM;
                  goto end;
                }
            }
          
          ret2 = dpl_add_condition_to_headers(&req->copy_source_condition, headers, 1);
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
  if (req->behavior_flags & DPL_BEHAVIOR_VIRTUAL_HOSTING)
    {
      char host[1024];

      snprintf(host, sizeof (host), "%s.%s", req->bucket, req->ctx->host);
      
      ret2 = dpl_dict_add(headers, "Host", host, 0);
      if (DPL_SUCCESS != ret2)
        {
          ret = DPL_ENOMEM;
          goto end;
        }
    }
  else
    {
      ret2 = dpl_dict_add(headers, "Host", req->ctx->host, 0);
      if (DPL_SUCCESS != ret2)
        {
          ret = DPL_ENOMEM;
          goto end;
        }
    }

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
      ret2 = dpl_add_date_to_headers(headers);
      if (DPL_SUCCESS != ret2)
        {
          ret = ret2;
          goto end;
        }
    }

  ret2 = dpl_add_authorization_to_headers(req, headers);
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

/** 
 * generate HTTP request
 * 
 * @param req 
 * @param headers 
 * @param query_params 
 * @param buf 
 * @param len 
 * @param lenp 
 * 
 * @return 
 */
dpl_status_t
dpl_req_gen_http_request(dpl_req_t *req,
                         dpl_dict_t *headers,
                         dpl_dict_t *query_params,
                         char *buf,
                         int len,
                         u_int *lenp)
{
  char *p;
  char *method = dpl_method_str(req->method);
  char resource_ue[DPL_URL_LENGTH(strlen(req->resource)) + 1];
  
  DPL_TRACE(req->ctx, DPL_TRACE_REQ, "req_gen_http_request");
    
  p = buf;

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
  
  //method
  DPL_APPEND_STR(method);
  
  DPL_APPEND_STR(" ");
  
  DPL_APPEND_STR(resource_ue);
  
  //subresource and query params
  if (NULL != req->subresource || NULL != query_params)
    DPL_APPEND_STR("?");
  
  if (NULL != req->subresource)
    DPL_APPEND_STR(req->subresource);
  
  if (NULL != query_params)
    {
      int bucket;
      dpl_var_t *var;
      int amp = 0;
      
      if (NULL != req->subresource)
        amp = 1;
      
      for (bucket = 0;bucket < query_params->n_buckets;bucket++)
        {
          for (var = query_params->buckets[bucket];var;var = var->prev)
            {
              if (amp)
                DPL_APPEND_STR("&");
              DPL_APPEND_STR(var->key);
              DPL_APPEND_STR("=");
              DPL_APPEND_STR(var->value);
              amp = 1;
            }
        }
    }
  
  DPL_APPEND_STR(" ");
  
  //version
  DPL_APPEND_STR("HTTP/1.1");
  DPL_APPEND_STR("\r\n");
  
  //headers
  if (NULL != headers)
    {
      int bucket;
      dpl_var_t *var;
      
      for (bucket = 0;bucket < headers->n_buckets;bucket++)
        {
          for (var = headers->buckets[bucket];var;var = var->prev)
            {
              DPL_TRACE(req->ctx, DPL_TRACE_REQ, "header='%s' value='%s'", var->key, var->value);

              DPL_APPEND_STR(var->key);
              DPL_APPEND_STR(": ");
              DPL_APPEND_STR(var->value);
              DPL_APPEND_STR("\r\n");
            }
        }
    }
  
  //final crlf managed by caller
  
  if (NULL != lenp)
    *lenp = (p - buf);

  return DPL_SUCCESS;
}

dpl_status_t
dpl_req_gen_url(dpl_req_t *req,
                dpl_dict_t *headers,
                char *buf,
                int len,
                u_int *lenp)
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
  ret2 = dpl_make_signature(req->ctx, method, req->bucket, resource_ue, req->subresource, headers, sign_str, sizeof (sign_str), &sign_len);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }
  
  DPL_TRACE(req->ctx, DPL_TRACE_REQ, "stringtosign=%.*s", sign_len, sign_str);
  
  hmac_len = dpl_hmac_sha1(req->ctx->secret_key, strlen(req->ctx->secret_key), sign_str, sign_len, hmac_str);

  base64_len = dpl_base64_encode((u_char *) hmac_str, hmac_len, base64_str);

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
