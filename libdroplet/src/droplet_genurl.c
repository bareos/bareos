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

/** 
 * get metadata
 * 
 * @param ctx 
 * @param bucket 
 * @param resource 
 * @param subresource can be NULL
 * @param condition can be NULL
 * @param metadatap must be freed by caller
 * 
 * @return 
 */
dpl_status_t
dpl_genurl(dpl_ctx_t *ctx,
           char *bucket,
           char *resource,
           char *subresource,
           time_t expires,
           char *buf,
           u_int len,
           u_int *lenp)
{
  int           ret, ret2;
  dpl_dict_t    *headers_request = NULL;
  dpl_req_t     *req = NULL;

  DPL_TRACE(ctx, DPL_TRACE_CONV, "genurl bucket=%s resource=%s", bucket, resource);

  req = dpl_req_new(ctx);
  if (NULL == req)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  dpl_req_set_method(req, DPL_METHOD_GET);

  dpl_req_add_behavior(req, DPL_BEHAVIOR_QUERY_STRING);

  ret2 = dpl_req_set_bucket(req, bucket);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  ret2 = dpl_req_set_resource(req, resource);
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

  dpl_req_set_expires(req, expires);

  //build request
  ret2 = dpl_req_build(req, &headers_request);
  if (DPL_SUCCESS != ret2)
    {
      ret = DPL_FAILURE;
      goto end;
    }

  ret2 = dpl_req_gen_url(req, headers_request, buf, len, lenp);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  ret = DPL_SUCCESS;

 end:

  if (NULL != headers_request)
    dpl_dict_free(headers_request);

  if (NULL != req)
    dpl_req_free(req);

  DPRINTF("ret=%d\n", ret);

  return ret;
}
