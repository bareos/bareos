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
#include <droplet/uks/uks.h>
#include <droplet/scal/gc.h>

void
dpl_scal_gc_gen_key(BIGNUM *id,
		    int cl)
{
  uint32_t hash;
  uint64_t oid;
  uint32_t volid;
  uint8_t serviceid;
  uint32_t specific;
  int ret;

  hash = dpl_rand_u32() & 0xFFFFFFu;
  oid = dpl_rand_u64();
  volid = dpl_rand_u32();
  serviceid = DPL_SCAL_GC_SERVICE_ID;
  specific = dpl_rand_u32();
  (void)dpl_uks_gen_key_raw(id, hash, oid, volid, serviceid, specific);
  ret = dpl_uks_set_class(id, cl);
  assert(0 == ret);
  ret = dpl_uks_set_replica(id, 0);
  assert(0 == ret);
}

dpl_status_t
dpl_scal_gc_index_init(dpl_dbuf_t **indexp)
{
  dpl_dbuf_t *index;
  char magic_ver[DPL_SCAL_GC_IDXMAGICLEN + 1];
  int ret;

  index = dpl_dbuf_new();
  if (NULL == index)
    return DPL_ENOMEM;

  memcpy(magic_ver, DPL_SCAL_GC_IDXMAGIC, DPL_SCAL_GC_IDXMAGICLEN);
  magic_ver[DPL_SCAL_GC_IDXMAGICLEN] = DPL_SCAL_GC_IDXVERSION;

  ret = dpl_dbuf_add(index, magic_ver, DPL_SCAL_GC_IDXMAGICLEN + 1);
  if (0 == ret)
    {
      dpl_dbuf_free(index);
      return DPL_ENOMEM;
    }

  if (NULL != indexp)
    *indexp = index;
  else
    dpl_dbuf_free(index);

  return DPL_SUCCESS;
}

#define CHECKRES(expr) if (0 == (expr)) return DPL_ENOMEM;

/**
 * @brief   Serialize a chunk entry into an index
 *
 * @param   _it     current item
 * @param   cb_arg  output buffer
 *
 * @return  success/failure
 **/
dpl_status_t
dpl_scal_gc_index_serialize(BIGNUM *chunkkey,
			    uint64_t offset,
			    uint64_t size,
			    dpl_dbuf_t *buffer)
{
  uint8_t               b[8];
  uint32_t              *i_p = (uint32_t *) b;
  uint64_t              *l_p = (uint64_t *) b;
  uint8_t               bn_b[DPL_UKS_NBITS/8];
  uint8_t               pad[4] = {0};
 
  assert(NULL != chunkkey);
  assert(NULL != buffer);

  /* add id */
  *i_p = BN_num_bytes(chunkkey);
  assert(*i_p <= (DPL_UKS_NBITS / 8));
  *i_p = (uint32_t) htonl(*i_p);
  CHECKRES(dpl_dbuf_add(buffer, (const void *) b, 4));
  *i_p = BN_bn2bin(chunkkey, bn_b);
  assert(*i_p > 0);
  CHECKRES(dpl_dbuf_add(buffer, (const void *) bn_b, *i_p));
  CHECKRES(dpl_dbuf_add(buffer, (const void *) pad, 4 - (*i_p % 4)));
  /* add offset */
  *i_p = (uint32_t) htonl(8);
  CHECKRES(dpl_dbuf_add(buffer, (const void *) b, 4));
  *l_p = htonll(offset);
  CHECKRES(dpl_dbuf_add(buffer, (const void *) b, 8));
  /* add size */
  *i_p = (uint32_t) htonl(8);
  CHECKRES(dpl_dbuf_add(buffer, (const void *) b, 4));
  *l_p = htonll(size);
  CHECKRES(dpl_dbuf_add(buffer, (const void *) b, 8));

  return DPL_SUCCESS;
}
