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
#include "droplet/uks/uks.h"

//#define DPRINTF(fmt,...) fprintf(stderr, fmt, ##__VA_ARGS__)
#define DPRINTF(fmt,...)

/**
 * list all buckets
 *
 * @param ctx the droplect context
 * @param storage_class DPL_STORAGE_CLASS_STANDARD
 * @param storage_class DPL_STORAGE_CLASS_REDUCED_REDUNDANCY
 * @param storage_class DPL_STORAGE_CLASS_CUSTOM
 * @param custom if DPL_STORAGE_CLASS_CUSTOM, specify the number of replicas
 * @param id_buf id buffer
 * @param max_len max length of id
 *
 * @return DPL_SUCCESS
 * @return other failure codes
 */
dpl_status_t 
dpl_gen_random_key(dpl_ctx_t *ctx,
                   dpl_storage_class_t storage_class,
                   char *custom,
                   char *id_buf,
                   int max_len)
{
  dpl_status_t ret, ret2;
  dpl_id_scheme_t *id_scheme;

  DPL_TRACE(ctx, DPL_TRACE_ID_SCHEME, "gen_random_key");

  if (NULL == ctx->backend->get_id_scheme)
    {
      ret = DPL_ENOTSUPP;
      goto end;
    }
  
  ret2 = ctx->backend->get_id_scheme(ctx, &id_scheme);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  if (DPL_ID_SCHEME_ANY == id_scheme)
    {
      //choose UKS
      id_scheme = &dpl_id_scheme_uks;
    }

  ret2 = id_scheme->gen_random_key(ctx, storage_class, custom, id_buf, max_len);
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }
  
  ret = DPL_SUCCESS;
  
 end:
  
  DPL_TRACE(ctx, DPL_TRACE_ID_SCHEME, "ret=%d", ret);
  
  return ret;
}
