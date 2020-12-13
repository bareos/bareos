/*
 * Copyright (C) 2014 SCALITY SA. All rights reserved.
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

#include <dropletp.h>
#include <droplet/s3/s3.h>

dpl_status_t
dpl_s3_stream_flush(dpl_ctx_t *ctx, dpl_stream_t *stream)
{
  dpl_status_t        ret;
  struct json_object  *obj = NULL;
  const char          *uploadid = NULL;
  unsigned int        nparts = 0;

  DPL_TRACE(ctx, DPL_TRACE_BACKEND, "");

  if (NULL == stream->status)
    {
      ret = DPL_FAILURE;
      goto end;
    }

  if (json_object_object_get_ex(stream->status, "uploadId",  &obj) == 0
      || !json_object_is_type(obj, json_type_string))
    {
      ret = DPL_FAILURE;
      goto end;
    }
  uploadid = json_object_get_string(obj);

  if (json_object_object_get_ex(stream->status, "nparts",  &obj) == 0
      || !json_object_is_type(obj, json_type_int))
    {
      ret = DPL_FAILURE;
      goto end;
    }
  nparts = json_object_get_int64(obj);

  if (json_object_object_get_ex(stream->status, "parts",  &obj) == 0
      || !json_object_is_type(obj, json_type_array))
    {
      ret = DPL_FAILURE;
      goto end;
    }

  ret = dpl_s3_stream_multipart_complete(ctx,
                                         stream->bucket,
                                         stream->locator,
                                         uploadid,
                                         obj, nparts,
                                         stream->md,
                                         stream->sysmd);
  if (DPL_SUCCESS != ret)
    goto end;

  ret = DPL_SUCCESS;

 end:

  DPL_TRACE(ctx, DPL_TRACE_BACKEND, "ret=%d", ret);

  return ret;
}
