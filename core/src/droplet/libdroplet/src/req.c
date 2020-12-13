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

//#define DPRINTF(fmt,...) fprintf(stderr, fmt, ##__VA_ARGS__)
#define DPRINTF(fmt,...)

void
dpl_req_free(dpl_req_t *req)
{
  if (NULL != req->host)
    free(req->host);

  if (NULL != req->port)
    free(req->port);

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

dpl_status_t
dpl_req_set_host(dpl_req_t *req,
                 const char *host)
{
  char *nstr;

  nstr = strdup(host);
  if (NULL == nstr)
    return DPL_ENOMEM;

  if (NULL != req->host)
    free(req->host);

  req->host = nstr;

  return DPL_SUCCESS;
}

dpl_status_t
dpl_req_set_port(dpl_req_t *req,
                 const char *port)
{
  char *nstr;

  nstr = strdup(port);
  if (NULL == nstr)
    return DPL_ENOMEM;

  if (NULL != req->port)
    free(req->port);

  req->port = nstr;

  return DPL_SUCCESS;
}

void
dpl_req_set_method(dpl_req_t *req,
                   dpl_method_t method)
{
  req->method = method;
}

dpl_status_t
dpl_req_set_bucket(dpl_req_t *req,
                   const char *bucket)
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
                     const char *resource)
{
  char *nstr;
  char npath[DPL_MAXPATHLEN];

  if (resource == NULL || *resource == '\0' || !strcmp(resource, "/")) {
    if (!strcmp(req->ctx->base_path, "/")) {
      if (!req->ctx->preserve_root_path)
        npath[0] = '\0';
      else
        strncpy(npath, resource, sizeof (npath));
    }
    else {
      if (!req->ctx->preserve_root_path)
        strncpy(npath, req->ctx->base_path, sizeof (npath));
      else {
        snprintf(npath, sizeof (npath), "%s%s", req->ctx->base_path, resource);
      }
    }
  } else {
    if (!strcmp(req->ctx->base_path, "/"))
      snprintf(npath, sizeof (npath), "%s", resource);
    else
      snprintf(npath, sizeof (npath), "%s/%s", req->ctx->base_path, resource);
  }

  nstr = strdup(npath);
  if (NULL == nstr)
    return DPL_ENOMEM;

  if (NULL != req->resource)
    free(req->resource);

  req->resource = nstr;
  return DPL_SUCCESS;
}

dpl_status_t
dpl_req_set_subresource(dpl_req_t *req,
                        const char *subresource)
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

dpl_status_t
dpl_req_add_subresource(dpl_req_t *req,
                        const char *subresource)
{
  char *tmp = NULL;

  if (! req->subresource)
    return dpl_req_set_subresource(req, subresource);

  tmp = realloc(req->subresource,
                strlen(req->subresource) + strlen(subresource) + 2);
  if (! tmp)
    return DPL_ENOMEM;

  tmp = strcat(tmp, ";");
  tmp = strcat(tmp, subresource);
  req->subresource = tmp;

  return DPL_SUCCESS;
}

void
dpl_req_add_behavior(dpl_req_t *req,
                     unsigned int flags)
{
  req->behavior_flags |= flags;
}

void
dpl_req_rm_behavior(dpl_req_t *req,
                    unsigned int flags)
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
                      const dpl_condition_t *condition)
{
  req->condition = *condition;
}

dpl_status_t
dpl_req_set_cache_control(dpl_req_t *req,
                        const char *cache_control)
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
                                const char *content_disposition)
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
                             const char *content_encoding)
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
dpl_req_set_data(dpl_req_t *req,
                 const char *data_buf,
                 u_int data_len)
{
  req->data_buf = data_buf;
  req->data_len = data_len;
  req->data_enabled = 1;
}

dpl_status_t
dpl_req_add_metadatum(dpl_req_t *req,
                      const char *key,
                      const char *value)
{
  return dpl_dict_add(req->metadata, key, value, 0);
}

dpl_status_t
dpl_req_add_metadata(dpl_req_t *req,
                     const dpl_dict_t *metadata)
{
  int ret;

  ret = dpl_dict_copy(req->metadata, metadata);
  if (DPL_SUCCESS != ret)
    {
      return DPL_FAILURE;
    }

  return DPL_SUCCESS;
}

dpl_status_t
dpl_req_set_content_type(dpl_req_t *req,
                         const char *content_type)
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
dpl_req_set_object_type(dpl_req_t *req,
                        dpl_ftype_t object_type)
{
  req->object_type = object_type;

  return DPL_SUCCESS;
}

dpl_status_t
dpl_req_add_range(dpl_req_t *req,
                  uint64_t start,
                  uint64_t end)
{
  req->range.start = start;
  req->range.end = end;
  req->range_enabled = 1;

  return DPL_SUCCESS;
}

void
dpl_req_set_expires(dpl_req_t *req,
                    time_t expires)
{
  req->expires = expires;
}

dpl_status_t
dpl_req_set_src_bucket(dpl_req_t *req,
                       const char *src_bucket)
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
dpl_req_set_src_resource_ext(dpl_req_t *req,
                             const char *src_resource,
                             int add_base_path)
{
  char *nstr;
  char npath[DPL_MAXPATHLEN];

  if (add_base_path)
    {
      if (!strcmp(req->ctx->base_path, "/"))
        snprintf(npath, sizeof (npath), "%s", src_resource);
      else
        snprintf(npath, sizeof (npath), "%s/%s", req->ctx->base_path, src_resource);
    }
  else
    snprintf(npath, sizeof (npath), "%s", src_resource);

  nstr = strdup(npath);
  if (NULL == nstr)
    return DPL_ENOMEM;

  if (NULL != req->src_resource)
    free(req->src_resource);

  req->src_resource = nstr;

  return DPL_SUCCESS;
}

dpl_status_t
dpl_req_set_src_resource(dpl_req_t *req,
                         const char *src_resource)
{
  return dpl_req_set_src_resource_ext(req, src_resource, 1);
}

dpl_status_t
dpl_req_set_src_subresource(dpl_req_t *req,
                            const char *src_subresource)
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
dpl_req_set_copy_directive(dpl_req_t *req,
                               dpl_copy_directive_t copy_directive)
{
  req->copy_directive = copy_directive;
}

void
dpl_req_set_copy_source_condition(dpl_req_t *req,
                                  const dpl_condition_t *condition)
{
  req->copy_source_condition = *condition;
}
