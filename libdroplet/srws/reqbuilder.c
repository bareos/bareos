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
#include <droplet/srws/reqbuilder.h>
#include <droplet/srws/replyparser.h>

//#define DPRINTF(fmt,...) fprintf(stderr, fmt, ##__VA_ARGS__)
#define DPRINTF(fmt,...)

static dpl_status_t
add_metadata_to_headers(dpl_dict_t *metadata,
                        dpl_dict_t *headers)

{
  int bucket;
  dpl_dict_var_t *var;
  int ret;
  dpl_sbuf_t *sbuf = NULL;
  char *usermd = NULL;
  int usermd_len;

  sbuf = dpl_sbuf_new(2);
  if (NULL == sbuf)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  for (bucket = 0;bucket < metadata->n_buckets;bucket++)
    {
      for (var = metadata->buckets[bucket];var;var = var->prev)
        {
          assert(var->val->type == DPL_VALUE_STRING);
          ret = dpl_ntinydb_set(sbuf, var->key,
                                dpl_sbuf_get_str(var->val->string),
                                strlen(dpl_sbuf_get_str(var->val->string)));
          if (DPL_SUCCESS != ret)
            {
              ret = DPL_FAILURE;
              goto end;
            }
        }
    }
  
  usermd = alloca(DPL_BASE64_LENGTH(sbuf->len) + 1);

  usermd_len = dpl_base64_encode((const u_char *) sbuf->buf, sbuf->len, (u_char *) usermd);
  usermd[usermd_len] = 0;

  ret = dpl_dict_add(headers, DPL_SRWS_X_BIZ_USERMD, usermd, 0);
  if (DPL_SUCCESS != ret)
    {
      return DPL_FAILURE;
    }

  ret = DPL_SUCCESS;
  
 end:
  
  if (NULL != sbuf)
    dpl_sbuf_free(sbuf);

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
dpl_srws_req_build(const dpl_req_t *req,
                   dpl_srws_req_mask_t req_mask,
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

      if (req_mask & DPL_SRWS_REQ_LAZY)
        {
          ret2 = dpl_dict_add(headers, DPL_SRWS_X_BIZ_REPLICA_POLICY, DPL_SRWS_LAZY, 0);
          if (DPL_SUCCESS != ret2)
            {
              ret = DPL_ENOMEM;
              goto end;
            }
        }

    }
  else if (DPL_METHOD_PUT == req->method)
    {
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

      if (req->behavior_flags & DPL_BEHAVIOR_EXPECT)
        {
          ret2 = dpl_dict_add(headers, "Expect", "100-continue", 0);
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

      if (req_mask & DPL_SRWS_REQ_MD_ONLY)
        {
          ret2 = dpl_dict_add(headers, DPL_SRWS_X_BIZ_CMD, DPL_SRWS_UPDATEUSERMD, 0);
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
