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
                         unsigned int *lenp)
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

  if (NULL != req->ctx->base_path)
    {
      DPL_APPEND_STR(req->ctx->base_path);
    }
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
