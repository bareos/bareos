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
  char basic_str[1024];
  int basic_len;
  char base64_str[1024];
  int base64_len;
  char auth_str[1024];

  snprintf(basic_str, sizeof (basic_str), "%s:%s", req->ctx->access_key, req->ctx->secret_key);
  basic_len = strlen(basic_str);

  base64_len = dpl_base64_encode((u_char *) basic_str, basic_len, base64_str);

  snprintf(auth_str, sizeof (auth_str), "Basic %.*s", base64_len, base64_str);

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

/**
 * build headers from request
 *
 * @param req
 * @param headersp
 *
 * @return
 */
dpl_status_t
dpl_cdmi_req_build(dpl_req_t *req,
                   dpl_dict_t **headersp)
{
  dpl_dict_t *headers = NULL;
  int ret, ret2;
  char *method = dpl_method_str(req->method);

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
  if (DPL_METHOD_GET == req->method ||
      DPL_METHOD_HEAD == req->method)
    {
      //XXX ranges, conditions
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
  //CDMI content-type is required even for GET
  if (NULL != req->content_type)
    {
      ret2 = dpl_dict_add(headers, "Content-Type", req->content_type, 0);
      if (DPL_SUCCESS != ret2)
        {
          ret = ret2;
          goto end;
        }
    }

  if (!(req->behavior_flags & DPL_BEHAVIOR_HTTP_COMPAT))
    {
      ret2 = dpl_dict_add(headers, "X-CDMI-Specification-Version", "1.0.1", 0);
      if (DPL_SUCCESS != ret2)
        {
          ret = ret2;
          goto end;
        }
    }

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

dpl_status_t
dpl_cdmi_req_gen_url(dpl_req_t *req,
                   dpl_dict_t *headers,
                   char *buf,
                   int len,
                   unsigned int *lenp)
{
  //XXX no spec in CDMI

  return DPL_ENOTSUPP;
}
