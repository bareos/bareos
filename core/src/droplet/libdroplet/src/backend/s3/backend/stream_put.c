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

static dpl_status_t _status_write_get(struct json_object* status,
                                      unsigned int* offsetp,
                                      unsigned int* npartsp)
{
  dpl_status_t ret = DPL_FAILURE;
  struct json_object* json_mode = NULL;
  struct json_object* json_off = NULL;
  struct json_object* json_nparts = NULL;
  struct json_object* json_parts = NULL;

  if (json_object_object_get_ex(status, "direction", &json_mode) == 0) {
    json_mode = json_object_new_string("write");
    if (NULL == json_mode) {
      ret = DPL_ENOMEM;
      goto end;
    }
    json_object_object_add(status, "direction", json_mode);
    json_mode = NULL;
  } else if (!json_object_is_type(json_mode, json_type_string)
             || strcmp(json_object_get_string(json_mode), "write") != 0) {
    ret = DPL_EINVAL;
    goto end;
  }

  if (json_object_object_get_ex(status, "offset", &json_off) == 0) {
    json_off = json_object_new_int64(0);
    if (NULL == json_off) {
      ret = DPL_ENOMEM;
      goto end;
    }
    json_object_object_add(status, "offset", json_off);
  }
  *offsetp = (unsigned int)json_object_get_int64(json_off);

  if (json_object_object_get_ex(status, "nparts", &json_nparts) == 0) {
    json_nparts = json_object_new_int64(0);
    if (NULL == json_nparts) {
      ret = DPL_ENOMEM;
      goto end;
    }
    json_object_object_add(status, "nparts", json_nparts);
  }
  *npartsp = (unsigned int)json_object_get_int64(json_nparts);

  if (json_object_object_get_ex(status, "parts", &json_parts) == 0) {
    json_parts = json_object_new_array();
    if (NULL == json_parts) {
      ret = DPL_ENOMEM;
      goto end;
    }
    json_object_object_add(status, "parts", json_parts);
  }

  ret = DPL_SUCCESS;

end:
  return ret;
}

static dpl_status_t _status_write_set(struct json_object* status,
                                      unsigned int offset,
                                      unsigned int nparts,
                                      unsigned int part_idx,
                                      const char* etag)
{
  dpl_status_t ret = DPL_FAILURE;
  struct json_object* json_etag = NULL;
  struct json_object* json_off = NULL;
  struct json_object* json_nparts = NULL;
  struct json_object* json_parts = NULL;

  if (json_object_object_get_ex(status, "parts", &json_parts) == 0) {
    ret = DPL_FAILURE;
    goto end;
  }

  json_etag = json_object_new_string(etag);
  if (json_etag == NULL) {
    ret = DPL_ENOMEM;
    goto end;
  }

  json_off = json_object_new_int64(offset);
  if (NULL == json_off) {
    ret = DPL_ENOMEM;
    goto end;
  }

  json_nparts = json_object_new_int64(nparts);
  if (NULL == json_nparts) {
    ret = DPL_ENOMEM;
    goto end;
  }

  json_object_object_del(status, "offset");
  json_object_object_add(status, "offset", json_off);
  json_off = NULL;
  json_object_object_del(status, "nparts");
  json_object_object_add(status, "nparts", json_nparts);
  json_nparts = NULL;
  json_object_array_put_idx(json_parts, part_idx, json_etag);
  json_etag = NULL;

  ret = DPL_SUCCESS;

end:
  if (NULL != json_off) json_object_put(json_off);
  if (NULL != json_nparts) json_object_put(json_nparts);
  if (NULL != json_etag) json_object_put(json_etag);

  return ret;
}


dpl_status_t dpl_s3_stream_putmd(dpl_ctx_t* ctx,
                                 dpl_stream_t* stream,
                                 dpl_dict_t* metadata,
                                 dpl_sysmd_t* sysmd)
{
  dpl_status_t ret = DPL_FAILURE;
  dpl_dict_t* md = NULL;
  dpl_sysmd_t* sys = NULL;

  DPL_TRACE(ctx, DPL_TRACE_BACKEND, "");

  if (metadata) {
    md = dpl_dict_dup(metadata);
    if (NULL == md) {
      ret = DPL_ENOMEM;
      goto end;
    }
  }

  if (sysmd) {
    sys = dpl_sysmd_dup(sysmd);
    if (NULL == sysmd) {
      ret = DPL_ENOMEM;
      goto end;
    }
  }

  if (metadata) {
    if (stream->md) dpl_dict_free(stream->md);
    stream->md = md;
    md = NULL;
  }

  if (sysmd) {
    if (stream->sysmd) dpl_sysmd_free(stream->sysmd);
    stream->sysmd = sys;
    sys = NULL;
  }

  ret = DPL_SUCCESS;

end:
  if (md) dpl_dict_free(md);
  if (sys) dpl_sysmd_free(sys);

  DPL_TRACE(ctx, DPL_TRACE_BACKEND, "ret=%d", ret);

  return ret;
}

dpl_status_t dpl_s3_stream_put(dpl_ctx_t* ctx,
                               dpl_stream_t* stream,
                               char* buf,
                               unsigned int len,
                               struct json_object** statusp)
{
  dpl_status_t ret;
  struct json_object* obj = NULL;
  const char** uploadidp = NULL;
  const char* uploadid = NULL;
  const char* etag = NULL;
  unsigned int offset = 0;
  unsigned int part_idx;
  unsigned int nparts;

  DPL_TRACE(ctx, DPL_TRACE_BACKEND, "");

  /*
   * Check state of multipart upload:
   *  1 -> Not started : Initialize multipart upload + Upload chunk.
   *  2 -> Ongoing : Upload chunk
   */
  if (NULL == stream->status) {
    stream->status = json_object_new_object();
    if (NULL == stream->status) {
      ret = DPL_ENOMEM;
      goto end;
    }
  }

  if (json_object_object_get_ex(stream->status, "uploadId", &obj) == 0) {
    uploadidp = &uploadid;
    ret = dpl_s3_stream_multipart_init(ctx, stream->bucket, stream->locator,
                                       uploadidp);
    if (DPL_SUCCESS != ret) { goto end; }
    obj = json_object_new_string(uploadid);
    if (NULL == obj) {
      ret = DPL_ENOMEM;
      goto end;
    }
    json_object_object_add(stream->status, "uploadId", obj);
    free((void*)uploadid);
    uploadidp = NULL;
  }
  uploadid = json_object_get_string(obj);

  /*
   * Get important status bits, Upload part, and update status.
   * Functions are used to simplify+represent the "update atomically" idiom.
   */
  ret = _status_write_get(stream->status, &offset, &nparts);
  if (DPL_SUCCESS != ret) goto end;

  part_idx = nparts;

  ret = dpl_s3_stream_multipart_put(
      ctx, stream->bucket, stream->locator, uploadid,
      part_idx + 1 /* we want the number not the index */, buf, len, &etag);
  if (DPL_SUCCESS != ret) goto end;

  ret = _status_write_set(stream->status, offset, nparts + 1, part_idx, etag);
  if (DPL_SUCCESS != ret) goto end;

  json_object_get(stream->status);
  *statusp = stream->status;

  ret = DPL_SUCCESS;

end:
  if (NULL != uploadidp) free((void*)uploadid);
  if (NULL != etag) free((void*)etag);

  DPL_TRACE(ctx, DPL_TRACE_BACKEND, "ret=%d", ret);

  return ret;
}
