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
#include <sys/param.h>

//#define DPRINTF(fmt,...) fprintf(stderr, fmt, ##__VA_ARGS__)
#define DPRINTF(fmt,...)

#define BIT_SET(bit)   (entropy[(bit)/NBBY] |= (1<<((bit)%NBBY)))
#define BIT_CLEAR(bit) (entropy[(bit)/NBBY] &= ~(1<<((bit)%NBBY)))

dpl_status_t
dpl_uks_gen_key_raw(BIGNUM *id,
		    uint32_t hash,
		    uint64_t oid,
		    uint32_t volid,
		    uint8_t serviceid,
		    uint32_t specific)
{
  int off, i;
  MD5_CTX ctx;

  BN_zero(id);
  
  off = DPL_UKS_REPLICA_NBITS +
    DPL_UKS_CLASS_NBITS;

  for (i = 0;i < DPL_UKS_SPECIFIC_NBITS;i++)
    {
      if (specific & 1<<i)
        {
          BN_set_bit(id, off + i);
        }
      else
        {
          BN_clear_bit(id, off + i);
        }
    }

  off += DPL_UKS_SPECIFIC_NBITS;

  for (i = 0;i < DPL_UKS_SERVICEID_NBITS;i++)
    {
      if (serviceid & 1<<i)
        {
          BN_set_bit(id, off + i);
        }
      else
        {
          BN_clear_bit(id, off + i);
        }
    }

  off += DPL_UKS_SERVICEID_NBITS;

  for (i = 0;i < DPL_UKS_VOLID_NBITS;i++)
    {
      if (volid & 1<<i)
        {
          BN_set_bit(id, off + i);
        }
      else
        {
          BN_clear_bit(id, off + i);
        }
    }

  off += DPL_UKS_VOLID_NBITS;

  for (i = 0;i < DPL_UKS_OID_NBITS;i++)
    {
      if (oid & (1ULL<<i))
        {
          BN_set_bit(id, off + i);
        }
      else
        {
          BN_clear_bit(id, off + i);
        }
    }

  off += DPL_UKS_OID_NBITS;

  for (i = 0;i < DPL_UKS_HASH_NBITS;i++)
    {
      if (hash & (1ULL<<i))
        {
          BN_set_bit(id, off + i);
        }
      else
        {
          BN_clear_bit(id, off + i);
        }
    }

  return DPL_SUCCESS;
}

dpl_status_t
dpl_uks_gen_key_ext(BIGNUM *id,
		    dpl_uks_mask_t mask,
		    uint64_t oid,
		    uint32_t volid,
		    uint8_t serviceid,
		    uint32_t specific)
{
  int off, i;
  MD5_CTX ctx;
  char entropy[DPL_UKS_PAYLOAD_NBITS/NBBY];
  u_char hash[MD5_DIGEST_LENGTH];

  off = DPL_UKS_REPLICA_NBITS +
    DPL_UKS_CLASS_NBITS;

  if (!(mask & DPL_UKS_MASK_SPECIFIC))
    {
      specific = 0;
      for (i = 0;i < DPL_UKS_SPECIFIC_NBITS;i++)
	{
	  if (BN_is_bit_set(id, off + i))
	    {
	      specific |= (1<<i);
	    }
	}
    }

  for (i = 0;i < DPL_UKS_SPECIFIC_NBITS;i++)
    {
      if (specific & 1<<i)
        {
          BN_set_bit(id, off + i);
          BIT_SET(off - DPL_UKS_EXTRA_NBITS + i);
        }
      else
        {
          BN_clear_bit(id, off + i);
          BIT_CLEAR(off - DPL_UKS_EXTRA_NBITS + i);
        }
    }

  off += DPL_UKS_SPECIFIC_NBITS;

  if (!(mask & DPL_UKS_MASK_SERVICEID))
    {
      serviceid = 0;
      for (i = 0;i < DPL_UKS_SERVICEID_NBITS;i++)
	{
	  if (BN_is_bit_set(id, off + i))
	    {
	      serviceid |= (1<<i);
	    }
	}
    }

  for (i = 0;i < DPL_UKS_SERVICEID_NBITS;i++)
    {
      if (serviceid & 1<<i)
        {
          BN_set_bit(id, off + i);
          BIT_SET(off - DPL_UKS_EXTRA_NBITS + i);
        }
      else
        {
          BN_clear_bit(id, off + i);
          BIT_CLEAR(off - DPL_UKS_EXTRA_NBITS + i);
        }
    }

  off += DPL_UKS_SERVICEID_NBITS;

  if (!(mask & DPL_UKS_MASK_VOLID))
    {
      volid = 0;
      for (i = 0;i < DPL_UKS_VOLID_NBITS;i++)
	{
	  if (BN_is_bit_set(id, off + i))
	    {
	      volid |= (1<<i);
	    }
	}
    }

  for (i = 0;i < DPL_UKS_VOLID_NBITS;i++)
    {
      if (volid & 1<<i)
        {
          BN_set_bit(id, off + i);
          BIT_SET(off - DPL_UKS_EXTRA_NBITS + i);
        }
      else
        {
          BN_clear_bit(id, off + i);
          BIT_CLEAR(off - DPL_UKS_EXTRA_NBITS + i);
        }
    }

  off += DPL_UKS_VOLID_NBITS;

  if (!(mask & DPL_UKS_MASK_OID))
    {
      oid = 0;
      for (i = 0;i < DPL_UKS_OID_NBITS;i++)
	{
	  if (BN_is_bit_set(id, off + i))
	    {
	      oid |= (1<<i);
	    }
	}
    }

  for (i = 0;i < DPL_UKS_OID_NBITS;i++)
    {
      if (oid & (1ULL<<i))
        {
          BN_set_bit(id, off + i);
          BIT_SET(off - DPL_UKS_EXTRA_NBITS + i);
        }
      else
        {
          BN_clear_bit(id, off + i);
          BIT_CLEAR(off  - DPL_UKS_EXTRA_NBITS + i);
        }
    }

  off += DPL_UKS_OID_NBITS;

  MD5_Init(&ctx);
  MD5_Update(&ctx, entropy, sizeof (entropy));
  MD5_Final(hash, &ctx);

  for (i = 0;i < DPL_UKS_HASH_NBITS;i++)
    {
      if (hash[i/8] & 1<<(i%8))
        BN_set_bit(id, off + i);
      else
        BN_clear_bit(id, off + i);
    }

  return DPL_SUCCESS;
}

dpl_status_t
dpl_uks_gen_key(BIGNUM *id,
                uint64_t oid,
                uint32_t volid,
                uint8_t serviceid,
                uint32_t specific)
{
  return dpl_uks_gen_key_ext(id, ~0, oid, volid, serviceid, specific);
}

dpl_status_t
dpl_uks_set_class(BIGNUM *k,
                  int cl)
{
  int i;
  
  if (cl < 0 ||
      cl >= 1<<DPL_UKS_CLASS_NBITS)
    return DPL_FAILURE;
  
  for (i = 0;i < DPL_UKS_CLASS_NBITS;i++)
    if (cl & 1<<i)
      BN_set_bit(k, DPL_UKS_REPLICA_NBITS + i);
    else
      BN_clear_bit(k, DPL_UKS_REPLICA_NBITS + i);
  
  return DPL_SUCCESS;
}

dpl_status_t
dpl_uks_set_replica(BIGNUM *k,
		    int replica)
{
  int i;

  if (replica < 0 ||
      replica >= 6)
    return DPL_FAILURE;

  for (i = 0; i < DPL_UKS_REPLICA_NBITS; i++)
    {
      if (replica & 1<<i)
        BN_set_bit(k, i);
      else
        BN_clear_bit(k, i);
    }

  return DPL_SUCCESS;
}

dpl_status_t
dpl_uks_gen_random_key(dpl_ctx_t *ctx,
                       dpl_storage_class_t storage_class,
                       char *custom,
                       char *id_buf,
                       int max_len)
{
  BIGNUM *bn = NULL;
  char *id_str = NULL;
  dpl_status_t ret, ret2;
  int len;
  int class = 0;

  bn = BN_new();
  if (NULL == bn)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  ret2 = dpl_uks_gen_key(bn,
                         dpl_rand_u64(),
                         dpl_rand_u32(),
                         0,
                         dpl_rand_u32());
  if (DPL_SUCCESS != ret2)
    {
      ret = ret2;
      goto end;
    }

  switch (storage_class)
    {
    case DPL_STORAGE_CLASS_UNDEF:
    case DPL_STORAGE_CLASS_STANDARD:
      class = 2;
      break ;
    case DPL_STORAGE_CLASS_REDUCED_REDUNDANCY:
      class = 1;
      break ;
    case DPL_STORAGE_CLASS_CUSTOM:
      
      if (NULL == custom)
        {
          ret = DPL_EINVAL;
          goto end;
        }

      class = atoi(custom);
      
      if (class < 0 || class > 15)
        {
          ret = DPL_EINVAL;
          goto end;
        }
      break ;
    }

  dpl_uks_set_class(bn, class);

  id_str = BN_bn2hex(bn);
  if (NULL == id_str)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  len = snprintf(id_buf, max_len, "%s", id_str);

  if (len >= max_len)
    {
      ret = DPL_ENAMETOOLONG;
      goto end;
    }

  ret = DPL_SUCCESS;
  
 end:

  free(id_str);
  BN_free(bn);

  return ret;
}

dpl_status_t
dpl_uks_bn2hex(const BIGNUM *id,
	       char *id_str)
{
  int ret;
  int tmp_str_len = 0;
  char *tmp_str = BN_bn2hex(id);

  if (! tmp_str)
    {
      ret = DPL_ENOMEM;
      goto end;
    }

  tmp_str_len = strlen(tmp_str);
  memset(id_str, '0', DPL_UKS_BCH_LEN);
  memcpy(id_str + DPL_UKS_BCH_LEN - tmp_str_len, tmp_str, tmp_str_len);
  id_str[DPL_UKS_BCH_LEN] = 0;

  ret = DPL_SUCCESS;

 end:

  if (tmp_str)
    free(tmp_str);

  return ret;
}

dpl_id_scheme_t dpl_id_scheme_uks = 
  {
    .name = "uks",
    .gen_random_key = dpl_uks_gen_random_key,
  };
