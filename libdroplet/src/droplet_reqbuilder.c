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
dpl_req_compute_md5(dpl_req_t *req,
                    char *buf,
                    u_int len)
{
  MD5_CTX ctx;

  MD5_Init(&ctx);
  MD5_Update(&ctx, buf, len);
  MD5_Final(req->digest, &ctx);
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
dpl_req_set_condition(dpl_req_t *req,
                      dpl_condition_t *condition)
{
  req->condition = *condition; //XXX etag
}
