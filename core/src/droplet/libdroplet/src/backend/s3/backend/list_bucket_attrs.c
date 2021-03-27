/*
 * Copyright (C) 2020-2021 Bareos GmbH & Co. KG
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

/* WARNING, UNTESTED */
dpl_status_t dpl_s3_list_bucket_attrs(dpl_ctx_t* ctx,
                                      const char* bucket,
                                      const char* prefix,
                                      const char* delimiter,
                                      const int max_keys,
                                      dpl_dict_t** metadatap,
                                      dpl_sysmd_t* sysmdp,
                                      dpl_vec_t** objectsp,
                                      dpl_vec_t** common_prefixesp,
                                      char** locationp)
{
  dpl_status_t status;

  status = dpl_s3_head(ctx, bucket, prefix, NULL, NULL, DPL_FTYPE_UNDEF, NULL,
                       metadatap, sysmdp, locationp);
  if (DPL_SUCCESS != status) { goto end; }

  status = dpl_s3_list_bucket(ctx, bucket, prefix, delimiter, max_keys,
                              objectsp, common_prefixesp, locationp);
  if (DPL_SUCCESS != status) {
    if (NULL != metadatap && NULL != *metadatap) {
      dpl_dict_free(*metadatap);
      *metadatap = NULL;
    }
    goto end;
  }

end:
  return status;
}
