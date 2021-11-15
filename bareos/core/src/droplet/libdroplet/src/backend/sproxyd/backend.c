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
#include "droplet/sproxyd/sproxyd.h"
#include "droplet/uks/uks.h"

/* #include <sys/param.h> */

dpl_status_t dpl_sproxyd_get_capabilities(dpl_ctx_t* ctx,
                                          dpl_capability_t* maskp)
{
  if (NULL != maskp) {
    *maskp = DPL_CAP_IDS | DPL_CAP_CONSISTENCY | DPL_CAP_VERSIONING
             | DPL_CAP_CONDITIONS | DPL_CAP_PUT_RANGE;
  }

  return DPL_SUCCESS;
}

dpl_status_t dpl_sproxyd_get_id_scheme(dpl_ctx_t* ctx,
                                       dpl_id_scheme_t** id_schemep)
{
  if (NULL != id_schemep) *id_schemep = &dpl_id_scheme_uks;

  return DPL_SUCCESS;
}

dpl_backend_t dpl_backend_sproxyd
    = {"sproxyd",
       .get_capabilities = dpl_sproxyd_get_capabilities,
       .get_id_scheme = dpl_sproxyd_get_id_scheme,
       .put_id = dpl_sproxyd_put_id,
       .get_id = dpl_sproxyd_get_id,
       .head_id = dpl_sproxyd_head_id,
       .head_id_raw = dpl_sproxyd_head_id_raw,
       .delete_id = dpl_sproxyd_delete_id,
       .delete_all_id = dpl_sproxyd_delete_all_id,
       .copy_id = dpl_sproxyd_copy_id};
