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

#include <json.h>

#include <dropletp.h>
#include <droplet/s3/s3.h>

static dpl_status_t _status_read_get(struct json_object* status,
                                     unsigned int* offsetp)
{
  dpl_status_t ret = DPL_FAILURE;
  struct json_object* json_mode = NULL;
  struct json_object* json_offset = NULL;

  if (json_object_object_get_ex(status, "direction", &json_mode) == 0) {
    json_mode = json_object_new_string("read");
    if (NULL == json_mode) {
      ret = DPL_ENOMEM;
      goto end;
    }
    json_object_object_add(status, "direction", json_mode);
    json_mode = NULL;
  } else if (!json_object_is_type(json_mode, json_type_string)
             || strcmp(json_object_get_string(json_mode), "read") != 0) {
    ret = DPL_EINVAL;
    goto end;
  }

  if (json_object_object_get_ex(status, "offset", &json_offset) == 0) {
    json_offset = json_object_new_int64(0);
    if (NULL == json_offset) {
      ret = DPL_ENOMEM;
      goto end;
    }
    json_object_object_add(status, "offset", json_offset);
  }

  *offsetp = json_object_get_int64(json_offset);

  ret = DPL_SUCCESS;

end:

  return ret;
}

static dpl_status_t _status_read_set(struct json_object* status,
                                     unsigned int offset)
{
  dpl_status_t ret = DPL_FAILURE;
  struct json_object* json_offset = NULL;

  json_offset = json_object_new_int64(offset);
  if (NULL == json_offset) {
    ret = DPL_ENOMEM;
    goto end;
  }
  json_object_object_del(status, "offset");
  json_object_object_add(status, "offset", json_offset);

  ret = DPL_SUCCESS;

end:
  return ret;
}

dpl_status_t dpl_s3_stream_getmd(dpl_ctx_t* ctx,
                                 dpl_stream_t* stream,
                                 dpl_dict_t** metadatap,
                                 dpl_sysmd_t** sysmdp)
{
  dpl_status_t ret = DPL_FAILURE;
  dpl_dict_t* md = NULL;
  dpl_sysmd_t* sys = NULL;

  DPL_TRACE(ctx, DPL_TRACE_BACKEND, "");

  if (metadatap && stream->md) {
    md = dpl_dict_dup(stream->md);
    if (NULL == md) {
      ret = DPL_ENOMEM;
      goto end;
    }
  }

  if (sysmdp && stream->sysmd) {
    sys = dpl_sysmd_dup(stream->sysmd);
    if (NULL == sys) {
      ret = DPL_ENOMEM;
      goto end;
    }
  }

  if (metadatap) {
    *metadatap = md;
    md = NULL;
  }

  if (sysmdp) {
    *sysmdp = sys;
    sys = NULL;
  }

  ret = DPL_SUCCESS;

end:
  if (md) dpl_dict_free(md);
  if (sys) dpl_sysmd_free(sys);

  DPL_TRACE(ctx, DPL_TRACE_BACKEND, "ret=%d", ret);

  return ret;
}

dpl_status_t dpl_s3_stream_get(dpl_ctx_t* ctx,
                               dpl_stream_t* stream,
                               unsigned int len,
                               char** bufp,
                               unsigned int* lenp,
                               struct json_object** statusp)
{
  dpl_status_t ret = DPL_FAILURE;
  dpl_dict_t** metadatap = NULL;
  dpl_sysmd_t* sysmdp = NULL;
  dpl_range_t range;
  unsigned int offset = 0;

  if (NULL == stream->status) {
    stream->status = json_object_new_object();
    if (NULL == stream->status) {
      ret = DPL_ENOMEM;
      goto end;
    }
  }

  ret = _status_read_get(stream->status, &offset);
  if (DPL_SUCCESS != ret) goto end;

  range.start = offset;
  range.end = range.start + len;

  ret = dpl_s3_get(ctx, stream->bucket, stream->locator, NULL /*sub resource*/,
                   stream->options, DPL_FTYPE_REG, stream->condition, &range,
                   bufp, lenp, metadatap, sysmdp, NULL);
  if (DPL_SUCCESS != ret) goto end;

  ret = _status_read_set(stream->status, offset + *lenp);
  if (DPL_SUCCESS != ret) goto end;

  ret = DPL_SUCCESS;

end:

  return ret;
}
