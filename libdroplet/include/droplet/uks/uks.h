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
#ifndef __DROPLET_UKS_UKS_H__
#define __DROPLET_UKS_UKS_H__ 1

#define DPL_UKS_CLASS_NBITS            4
#define DPL_UKS_REPLICA_NBITS          4
#define DPL_UKS_EXTRA_NBITS            (DPL_UKS_CLASS_NBITS+DPL_UKS_REPLICA_NBITS)

#define DPL_UKS_SPECIFIC_NBITS       24
#define DPL_UKS_SERVICEID_NBITS      8
#define DPL_UKS_VOLID_NBITS          32
#define DPL_UKS_OID_NBITS            64
#define DPL_UKS_HASH_NBITS           24 /*!< dispersion */

#define DPL_UKS_PAYLOAD_NBITS        (DPL_UKS_SPECIFIC_NBITS+DPL_UKS_SERVICEID_NBITS+DPL_UKS_VOLID_NBITS+DPL_UKS_OID_NBITS)

#define DPL_UKS_NBITS   160
#define DPL_UKS_BCH_LEN 40

typedef enum
  {
    DPL_UKS_MASK_OID = (1u<<0),
    DPL_UKS_MASK_VOLID = (1u<<1),
    DPL_UKS_MASK_SERVICEID = (1u<<2),
    DPL_UKS_MASK_SPECIFIC = (1u<<3),
  } dpl_uks_mask_t;

dpl_status_t dpl_uks_gen_key_raw(BIGNUM *id, uint32_t hash, uint64_t oid, uint32_t volid, uint8_t serviceid, uint32_t specific);
dpl_status_t dpl_uks_gen_key_ext(BIGNUM *id, dpl_uks_mask_t mask, uint64_t oid, uint32_t volid, uint8_t serviceid, uint32_t specific);
dpl_status_t dpl_uks_gen_key(BIGNUM *id, uint64_t oid, uint32_t volid, uint8_t serviceid, uint32_t specific);
dpl_status_t dpl_uks_set_class(BIGNUM *k, int cl);
dpl_status_t dpl_uks_set_replica(BIGNUM *k, int replica);
dpl_status_t dpl_uks_bn2hex(const BIGNUM *id, char *id_str);

extern dpl_id_scheme_t dpl_id_scheme_uks;

#endif
