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
 * are those of the authors and should not be interpreted as representi
ng
 * official policies, either expressed or implied, of SCALITY SA.
 *
 * https://github.com/scality/Droplet
 */
#include "dropletp.h"
#include <droplet/swift/reqbuilder.h>
#include <droplet/swift/replyparser.h>
#include <droplet/swift/backend.h>

/** @file */

//#define DPRINTF(fmt,...) fprintf(stderr, fmt, ##__VA_ARGS__)
#define DPRINTF(fmt,...)

/*
  ret2 = dpl_swift_req_set_resource(req, NULL != prefix ? prefix : "/");
  ret2 = dpl_swift_req_build(req, 0, &headers_request, NULL, NULL);
*/


dpl_status_t
dpl_swift_req_set_resource(dpl_req_t *req,
                          const char *resource)
{
  char *nstr;
  int len;
  dpl_status_t ret;

  nstr = strdup(resource);
  if (NULL == nstr)
    return DPL_ENOMEM;

  len = strlen(nstr);

  //suppress special chars
  if (len > 0u && '?' == nstr[len - 1])
    nstr[len - 1] = 0;

  ret = dpl_req_set_resource(req, nstr);

  free(nstr);

  return ret;
}

dpl_status_t
dpl_swift_req_build(dpl_ctx_t *ctx,
		    const dpl_req_t *req,
		    dpl_swift_req_mask_t req_mask,
		    dpl_dict_t **headersp,
		    char **body_strp,
		    int *body_lenp)
{
  dpl_dict_t *headers = NULL;
  int ret, ret2;
  char *method = dpl_method_str(req->method);
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
      switch (req->object_type)
        {
        case DPL_FTYPE_ANY:
          ret2 = dpl_dict_add(headers, "Accept", DPL_SWIFT_CONTENT_TYPE_ANY, 0);
          if (DPL_SUCCESS != ret2)
            {
              ret = ret2;
              goto end;
            }
          break ;
        }
    }
  else if (DPL_METHOD_PUT == req->method)
    {
      if (body_strp != NULL)
	{
	  snprintf(buf, sizeof (buf), "%u", *body_lenp);
	  ret2 = dpl_dict_add(headers, "Content-Length", buf, 0);
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
  else if (DPL_METHOD_HEAD == req->method)
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
