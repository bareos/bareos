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
#include "droplet/s3/s3.h"

dpl_status_t
dpl_s3_head(dpl_ctx_t *ctx,
            const char *bucket,
            const char *resource,
            const char *subresource,
            const dpl_option_t *option,
            dpl_ftype_t object_type,
            const dpl_condition_t *condition,
            dpl_dict_t **metadatap,
            dpl_sysmd_t *sysmdp,
            char **locationp)
{
  dpl_status_t ret, ret2;
  dpl_dict_t *headers_reply = NULL;

  DPL_TRACE(ctx, DPL_TRACE_BACKEND, "");

  /*
   * We need to avoid attempting any access to the directory itself if the
   * empty folder emulation is not enabled (empty folder emulation means that
   * an empty object is used to create directories, instead of simply using the
   * delimiters in the paths)
   */
  if ((resource && resource[strlen(resource)-1] != '/') || ctx->empty_folder_emulation)
    {
      ret2 = dpl_s3_head_raw(ctx, bucket, resource, subresource, NULL,
                             object_type, condition, &headers_reply, locationp);
      if (DPL_SUCCESS != ret2)
        {
          ret = ret2;
          goto end;
        }

      ret2 = dpl_s3_get_metadata_from_headers(headers_reply, metadatap, sysmdp);
      if (DPL_SUCCESS != ret2)
        {
          ret = ret2;
          goto end;
        }
    }
  else if (NULL != sysmdp)
    {
      /*
       * No emulation enabled, but we still need to return a valid minimal set
       * of properties for the Droplet API to be usable, if required.
       */
      memset(sysmdp, 0, sizeof(*sysmdp));
      sysmdp->mask = DPL_SYSMD_MASK_FTYPE | DPL_SYSMD_MASK_SIZE | DPL_SYSMD_MASK_PATH;
      sysmdp->ftype = DPL_FTYPE_DIR;
      snprintf(sysmdp->path, DPL_MAXPATHLEN, "%s", resource);
    }

  ret = DPL_SUCCESS;
  
 end:

  if (NULL != headers_reply)
    dpl_dict_free(headers_reply);

  DPL_TRACE(ctx, DPL_TRACE_BACKEND, "ret=%d", ret);

  return ret;
}
