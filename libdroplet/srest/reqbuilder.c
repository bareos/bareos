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
#include <droplet/srest/reqbuilder.h>

//#define DPRINTF(fmt,...) fprintf(stderr, fmt, ##__VA_ARGS__)
#define DPRINTF(fmt,...)

/**
 * build headers from request
 *
 * @param req
 * @param headersp
 *
 * @return
 */
dpl_status_t
dpl_srest_req_build(const dpl_req_t *req,
                    dpl_dict_t **headersp)
{
  dpl_dict_t *headers = NULL;
  int ret, ret2;
  const char *method = dpl_method_str(req->method);
  char buf[256];

  DPL_TRACE(req->ctx, DPL_TRACE_REQ, "req_build method=%s bucket=%s resource=%s subresource=%s", method, req->bucket, req->resource, req->subresource);

  headers = dpl_dict_new(13);
  if (NULL == headers)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  /*
   * per method headers
   */
  if (DPL_METHOD_GET == req->method)
    {
      //XXX ranges, conditions
    }
  else if (DPL_METHOD_POST == req->method)
    {
      snprintf(buf, sizeof (buf), "%u", req->chunk->len);
      ret2 = dpl_dict_add(headers, "Content-Length", buf, 0);
      if (DPL_SUCCESS != ret2)
        {
          ret = DPL_ENOMEM;
          goto end;
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

    }
  else if (DPL_METHOD_DELETE == req->method)
    {
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
