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
#include "droplet/sproxyd/sproxyd.h"

dpl_status_t dpl_sproxyd_copy_id(dpl_ctx_t* ctx,
                                 const char* src_bucket,
                                 const char* src_resource,
                                 const char* src_subresource,
                                 const char* dst_bucket,
                                 const char* dst_resource,
                                 const char* dst_subresource,
                                 const dpl_option_t* option,
                                 dpl_ftype_t object_type,
                                 dpl_copy_directive_t copy_directive,
                                 const dpl_dict_t* metadata,
                                 const dpl_sysmd_t* sysmd,
                                 const dpl_condition_t* condition,
                                 char** locationp)
{
  int ret, ret2;

  DPL_TRACE(ctx, DPL_TRACE_BACKEND, "");

  switch (copy_directive) {
    case DPL_COPY_DIRECTIVE_UNDEF:
      ret = DPL_ENOTSUPP;
      goto end;
    case DPL_COPY_DIRECTIVE_COPY:
      ret = DPL_ENOTSUPP;
      goto end;
    case DPL_COPY_DIRECTIVE_METADATA_REPLACE:

      // replace the metadata
      ret2 = dpl_sproxyd_put_internal(
          ctx, dst_bucket, dst_resource, dst_subresource, option, object_type,
          condition, NULL, metadata, DPL_CANNED_ACL_UNDEF, NULL, 0, 1,
          locationp);
      if (DPL_SUCCESS != ret2) {
        ret = ret2;
        goto end;
      }

      break;
    case DPL_COPY_DIRECTIVE_LINK:
    case DPL_COPY_DIRECTIVE_SYMLINK:
    case DPL_COPY_DIRECTIVE_MOVE:
    case DPL_COPY_DIRECTIVE_MKDENT:
    case DPL_COPY_DIRECTIVE_RMDENT:
    case DPL_COPY_DIRECTIVE_MVDENT:
      ret = DPL_ENOTSUPP;
      goto end;
  }

  ret = DPL_SUCCESS;

end:

  DPL_TRACE(ctx, DPL_TRACE_BACKEND, "ret=%d", ret);

  return ret;
}
