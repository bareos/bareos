/*
 * Copyright (C) 2020-2022 Bareos GmbH & Co. KG
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

/**
 * @defgroup scality Scality specific functions
 * @addtogroup scality
 * @{
 * Functions specific to Scality backends
 *
 * This module contains utility functions for dealing with the Scality
 * backend cloud provider.
 *
 * Several of these functions deal with Scality's UKS (Universal Key Scheme)
 * which is the binary object ID format used in Scality's RING software.
 * UKS keys are 160 bits long and are divided into several fixed-length
 * fields which encode specific information.  Fields include:
 *
 * @arg @b hashing/dispersion (24b) used to help distribute objects
 * around multiple servers in a ring, normally an MD5 hash of the
 * payload field.
 *
 * @arg @b payload (128b) Further information, broken out below.
 *
 * @arg @b class (4b).  Classes from 0-5 specify the number of *additional*
 * copies to be stored, i.e. class 2 means 3 copies will be stored.
 * Some of the class numbers 6-15 are used for other purposes.
 *
 * @arg @b replica (4b).  The replica number for classes 0-5, i.e. 0 for
 * the first copy, 1 for the second copy.
 *
 * You can store anything you like in the @b payload field, but a
 * recommended format is to use the following bit fields.
 *
 * @arg @b Object @b ID (64b).  Identifies an object, e.g. an inode number.
 *
 * @arg @b Volume @b ID (32b).  Identifies a virtual volume, e.g. a
 * filesystem.
 *
 * @arg @b Application @b ID (8b).  Also known as @b Service @b ID.  A fixed
 * field identifying your application, to allow multiple applications to use
 * the same RING storage.  To avoid clashing with future Scality software,
 * use a number greater than 0xc0.
 *
 * @arg @b Application @b Specific (24b), e.g. block offset within
 * the file.
 */


//#define DPRINTF(fmt,...) fprintf(stderr, fmt, ##__VA_ARGS__)
#define DPRINTF(fmt, ...)

#define BIT_SET(bit) (entropy[(bit) / NBBY] |= (1 << ((bit) % NBBY)))
#define BIT_CLEAR(bit) (entropy[(bit) / NBBY] &= ~(1 << ((bit) % NBBY)))

/**
 * Generate a UKS key from raw data
 *
 * Generates a binary UKS key in @a id using raw data for each of the
 * bitfields in the generic format.  Note the class and replica fields
 * should be set separately using `dpl_uks_set_class()` and
 * `dpl_uks_set_replica()`.  The binary key @a id is stored in a
 * `BIGNUM` structure, which you should create with `BN_new()` and
 * free with `BN_free()`.
 *
 * Only use this function if you know what you are doing.  Note
 * particularly that this function requires you to calculate and set the
 * hashing/dispersion field yourself.  For most applications you should
 * use either `dpl_uks_gen_key_ext()` or `dpl_uks_gen_key()` which will
 * calculate the hashing/dispersion field for you.
 *
 * @param id the binary UKS key will be generated here
 * @param hash will be used as the hashing/dispersion field
 * @param oid will be used as the Object ID field
 * @param volid will be used as the Volume ID field
 * @param serviceid will be used as the Service ID field
 * @param specific will be used as the Application Specific field
 * @retval DPL_SUCCESS this function currently cannot fail
 */
dpl_status_t dpl_uks_gen_key_raw(BIGNUM* id,
                                 uint32_t hash,
                                 uint64_t oid,
                                 uint32_t volid,
                                 uint8_t serviceid,
                                 uint32_t specific)
{
  int off, i;

  BN_zero(id);

  off = DPL_UKS_REPLICA_NBITS + DPL_UKS_CLASS_NBITS;

  for (i = 0; i < DPL_UKS_SPECIFIC_NBITS; i++) {
    if (specific & 1 << i) {
      BN_set_bit(id, off + i);
    } else {
      BN_clear_bit(id, off + i);
    }
  }

  off += DPL_UKS_SPECIFIC_NBITS;

  for (i = 0; i < DPL_UKS_SERVICEID_NBITS; i++) {
    if (serviceid & 1 << i) {
      BN_set_bit(id, off + i);
    } else {
      BN_clear_bit(id, off + i);
    }
  }

  off += DPL_UKS_SERVICEID_NBITS;

  for (i = 0; i < DPL_UKS_VOLID_NBITS; i++) {
    if (volid & 1 << i) {
      BN_set_bit(id, off + i);
    } else {
      BN_clear_bit(id, off + i);
    }
  }

  off += DPL_UKS_VOLID_NBITS;

  for (i = 0; i < DPL_UKS_OID_NBITS; i++) {
    if (oid & (1ULL << i)) {
      BN_set_bit(id, off + i);
    } else {
      BN_clear_bit(id, off + i);
    }
  }

  off += DPL_UKS_OID_NBITS;

  for (i = 0; i < DPL_UKS_HASH_NBITS; i++) {
    if (hash & (1ULL << i)) {
      BN_set_bit(id, off + i);
    } else {
      BN_clear_bit(id, off + i);
    }
  }

  return DPL_SUCCESS;
}

/**
 * Set some fields in a binary UKS key.
 *
 * Sets some fields in a binary UKS key, according to a mask of which
 * fields to set.  Fields not specified in the mask are preserved.
 * Automatically recalculates the hashing/dispersion field.  You should
 * also call `dpl_uks_set_class()` to set the class field. The binary
 * key @a id is stored in a `BIGNUM` structure, which you should create
 * with `BN_new()` and free with `BN_free()`.
 *
 * @param id the binary UKS key
 * @param mask a bitmask of the following values indicating which fields to set
 * @arg `DPL_UKS_MASK_OID` use the @a oid parameter as the Object ID field
 * @arg `DPL_UKS_MASK_VOLID` use the @a volid parameter as the Volume ID field
 * @arg `DPL_UKS_MASK_SERVICEID` use the @serviceid parameter as the
 * Service ID field
 * @arg `DPL_UKS_MASK_SPECIFIC` use the @specific parameter as the
 * Application Specific field.
 * @param oid will be used as the Object ID field if `DPL_UKS_MASK_OID`
 * is present in @a mask
 * @param volid will be used as the Volume ID field if
 * `DPL_UKS_MASK_VOLID` is present in @a mask
 * @param serviceid will be used as the Service ID field if
 * `DPL_UKS_MASK_SERVICEID` is present in @a mask
 * @param specific will be used as the Application Specific field if
 * `DPL_UKS_MASK_SPECIFIC` is present in @a mask
 * @retval DPL_SUCCESS on success, or
 * @retval DPL_* a Droplet error code on failure
 */
dpl_status_t dpl_uks_gen_key_ext(BIGNUM* id,
                                 dpl_uks_mask_t mask,
                                 uint64_t oid,
                                 uint32_t volid,
                                 uint8_t serviceid,
                                 uint32_t specific)
{
  int off, i;
  MD5_CTX ctx;
  char entropy[DPL_UKS_PAYLOAD_NBITS / NBBY];
  u_char hash[MD5_DIGEST_LENGTH];

  off = DPL_UKS_REPLICA_NBITS + DPL_UKS_CLASS_NBITS;

  memset(entropy, 0, sizeof(entropy));
  if (!(mask & DPL_UKS_MASK_SPECIFIC)) {
    specific = 0;
    for (i = 0; i < DPL_UKS_SPECIFIC_NBITS; i++) {
      if (BN_is_bit_set(id, off + i)) { specific |= (1 << i); }
    }
  }

  for (i = 0; i < DPL_UKS_SPECIFIC_NBITS; i++) {
    if (specific & 1 << i) {
      BN_set_bit(id, off + i);
      BIT_SET(off - DPL_UKS_EXTRA_NBITS + i);
    } else {
      BN_clear_bit(id, off + i);
      BIT_CLEAR(off - DPL_UKS_EXTRA_NBITS + i);
    }
  }

  off += DPL_UKS_SPECIFIC_NBITS;

  if (!(mask & DPL_UKS_MASK_SERVICEID)) {
    serviceid = 0;
    for (i = 0; i < DPL_UKS_SERVICEID_NBITS; i++) {
      if (BN_is_bit_set(id, off + i)) { serviceid |= (1 << i); }
    }
  }

  for (i = 0; i < DPL_UKS_SERVICEID_NBITS; i++) {
    if (serviceid & 1 << i) {
      BN_set_bit(id, off + i);
      BIT_SET(off - DPL_UKS_EXTRA_NBITS + i);
    } else {
      BN_clear_bit(id, off + i);
      BIT_CLEAR(off - DPL_UKS_EXTRA_NBITS + i);
    }
  }

  off += DPL_UKS_SERVICEID_NBITS;

  if (!(mask & DPL_UKS_MASK_VOLID)) {
    volid = 0;
    for (i = 0; i < DPL_UKS_VOLID_NBITS; i++) {
      if (BN_is_bit_set(id, off + i)) { volid |= (1 << i); }
    }
  }

  for (i = 0; i < DPL_UKS_VOLID_NBITS; i++) {
    if (volid & 1 << i) {
      BN_set_bit(id, off + i);
      BIT_SET(off - DPL_UKS_EXTRA_NBITS + i);
    } else {
      BN_clear_bit(id, off + i);
      BIT_CLEAR(off - DPL_UKS_EXTRA_NBITS + i);
    }
  }

  off += DPL_UKS_VOLID_NBITS;

  if (!(mask & DPL_UKS_MASK_OID)) {
    oid = 0;
    for (i = 0; i < DPL_UKS_OID_NBITS; i++) {
      if (BN_is_bit_set(id, off + i)) { oid |= (1 << i); }
    }
  }

  for (i = 0; i < DPL_UKS_OID_NBITS; i++) {
    if (oid & (1ULL << i)) {
      BN_set_bit(id, off + i);
      BIT_SET(off - DPL_UKS_EXTRA_NBITS + i);
    } else {
      BN_clear_bit(id, off + i);
      BIT_CLEAR(off - DPL_UKS_EXTRA_NBITS + i);
    }
  }

  off += DPL_UKS_OID_NBITS;

  MD5_Init(&ctx);
  MD5_Update(&ctx, entropy, sizeof(entropy));
  MD5_Final(hash, &ctx);

  for (i = 0; i < DPL_UKS_HASH_NBITS; i++) {
    if (hash[i / 8] & 1 << (i % 8))
      BN_set_bit(id, off + i);
    else
      BN_clear_bit(id, off + i);
  }

  return DPL_SUCCESS;
}

/**
 * Generate a binary UKS key.
 *
 * Sets all the fields in a binary UKS key and automatically calculates
 * the hashing/dispersion field.  You should also call `dpl_uks_set_class()`
 * to set the class field.  The binary key @a id is stored in a `BIGNUM`
 * structure, which you should create with `BN_new()` and free with `BN_free()`.
 *
 * @param id the binary UKS key
 * @param oid will be used as the Object ID field
 * @param volid will be used as the Volume ID field
 * @param serviceid will be used as the Service ID field
 * @param specific will be used as the Application Specific field
 * @retval DPL_SUCCESS on success, or
 * @retval DPL_* a Droplet error code on failure
 */
dpl_status_t dpl_uks_gen_key(BIGNUM* id,
                             uint64_t oid,
                             uint32_t volid,
                             uint8_t serviceid,
                             uint32_t specific)
{
  return dpl_uks_gen_key_ext(id, ~0, oid, volid, serviceid, specific);
}

/**
 * Get the hash field from a UKS key.
 *
 * Get the hash field from a UKS key. The binary key @a id is stored in a
 * `BIGNUM` structure, which you should create with `BN_new()` and free with
 * `BN_free()`.
 *
 * @param k the binary UKS key
 * @retval the value of the hash
 */
uint32_t dpl_uks_hash_get(BIGNUM* k)
{
  int i;
  int hash = 0;

  for (i = 0; i < DPL_UKS_HASH_NBITS; i++) {
    if (BN_is_bit_set(k, DPL_UKS_PAYLOAD_NBITS + i)) hash |= 1 << i;
  }

  return hash;
}


/**
 * Set the hash field in a UKS key.
 *
 * Set the hash field in a UKS key. The binary key @a id is stored in a
 * `BIGNUM` structure, which you should create with `BN_new()` and free with
 * `BN_free()`.
 *
 * @param k the binary UKS key
 * @param hash will be used as the class field
 * @retval DPL_SUCCESS on success, or
 * @retval DPL_* a Droplet error code on failure
 */
dpl_status_t dpl_uks_hash_set(BIGNUM* k, uint32_t hash)
{
  int i;

  if (hash >= (1 << DPL_UKS_HASH_NBITS)) return DPL_FAILURE;

  for (i = 0; i < DPL_UKS_HASH_NBITS; i++) {
    if (hash & 1 << i)
      BN_set_bit(k, DPL_UKS_PAYLOAD_NBITS + i);
    else
      BN_clear_bit(k, DPL_UKS_PAYLOAD_NBITS + i);
  }

  return DPL_SUCCESS;
}

/**
 * Set the class field in a UKS key.
 *
 * Set the class field in a UKS key.  The binary key @a id is stored in a
 * `BIGNUM` structure, which you should create with `BN_new()` and free
 * with `BN_free()`.
 *
 * @param k the binary UKS key
 * @param cl will be used as the class field
 * @retval DPL_SUCCESS on success, or
 * @retval DPL_* a Droplet error code on failure
 */
dpl_status_t dpl_uks_set_class(BIGNUM* k, int cl)
{
  int i;

  if (cl < 0 || cl >= 1 << DPL_UKS_CLASS_NBITS) return DPL_FAILURE;

  for (i = 0; i < DPL_UKS_CLASS_NBITS; i++)
    if (cl & 1 << i)
      BN_set_bit(k, DPL_UKS_REPLICA_NBITS + i);
    else
      BN_clear_bit(k, DPL_UKS_REPLICA_NBITS + i);

  return DPL_SUCCESS;
}

/**
 * Set the replica field in a UKS key.
 *
 * Set the replica field in a UKS key.  The binary key @a id is stored in a
 * `BIGNUM` structure, which you should create with `BN_new()` and free
 * with `BN_free()`.
 *
 * @param k the binary UKS key
 * @param replica will be used as the replica field
 * @retval DPL_SUCCESS on success, or
 * @retval DPL_* a Droplet error code on failure
 */
dpl_status_t dpl_uks_set_replica(BIGNUM* k, int replica)
{
  int i;

  if (replica < 0 || replica >= 6) return DPL_FAILURE;

  for (i = 0; i < DPL_UKS_REPLICA_NBITS; i++) {
    if (replica & 1 << i)
      BN_set_bit(k, i);
    else
      BN_clear_bit(k, i);
  }

  return DPL_SUCCESS;
}

dpl_status_t dpl_uks_gen_random_key(dpl_ctx_t* ctx,
                                    dpl_storage_class_t storage_class,
                                    char* custom,
                                    char* id_buf,
                                    int max_len)
{
  BIGNUM* bn = NULL;
  char* id_str = NULL;
  dpl_status_t ret, ret2;
  int len, padding;
  int class = 0;

  bn = BN_new();
  if (NULL == bn) {
    ret = DPL_ENOMEM;
    goto end;
  }

  ret2 = dpl_uks_gen_key(bn, dpl_rand_u64(), dpl_rand_u32(), 0, dpl_rand_u32());
  if (DPL_SUCCESS != ret2) {
    ret = ret2;
    goto end;
  }

  switch (storage_class) {
    case DPL_STORAGE_CLASS_ERROR:
    case DPL_STORAGE_CLASS_UNDEF:
    case DPL_STORAGE_CLASS_STANDARD:
    case DPL_STORAGE_CLASS_STANDARD_IA:
      class = 2;
      break;
    case DPL_STORAGE_CLASS_REDUCED_REDUNDANCY:
      class = 1;
      break;
    case DPL_STORAGE_CLASS_CUSTOM:

      if (NULL == custom) {
        ret = DPL_EINVAL;
        goto end;
      }

      class = atoi(custom);

      if (class < 0 || class > 15) {
        ret = DPL_EINVAL;
        goto end;
      }
      break;
  }

  dpl_uks_set_class(bn, class);

  id_str = BN_bn2hex(bn);
  if (NULL == id_str) {
    ret = DPL_ENOMEM;
    goto end;
  }

  len = snprintf(id_buf, max_len, "%s", id_str);
  if (len >= max_len) {
    ret = DPL_ENAMETOOLONG;
    goto end;
  }

  padding = DPL_UKS_BCH_LEN - strlen(id_buf);
  if (padding > 0) {
    int i;

    memmove(id_buf + padding, id_buf, strlen(id_buf));
    for (i = 0; i < padding; i++) id_buf[i] = '0';
  }

  ret = DPL_SUCCESS;

end:

  free(id_str);
  BN_free(bn);

  return ret;
}

/**
 * Convert a binary UKS key to string form.
 *
 * Converts the binary UKS key in @a id to a string form in
 * @a id_str.  The string form is suitable for use as the @a id
 * parameter of the RESTful functions such as `dpl_put_id()`.
 * The binary key @a id is stored in a `BIGNUM` structure, which
 * you should create with `BN_new()` and free with `BN_free()`.
 *
 * @param id the binary UKS key
 * @param[out] id_str on success the string form is written here, must
 *  be at least `DPL_UKS_BCH_LEN+1` bytes long
 * @retval DPL_SUCCESS on success, or
 * @retval DPL_* a Droplet error code on failure
 */
dpl_status_t dpl_uks_bn2hex(const BIGNUM* id, char* id_str)
{
  int ret;
  int tmp_str_len = 0;
  char* tmp_str = BN_bn2hex(id);

  if (!tmp_str) {
    ret = DPL_ENOMEM;
    goto end;
  }

  tmp_str_len = strlen(tmp_str);
  memset(id_str, '0', DPL_UKS_BCH_LEN);
  memcpy(id_str + DPL_UKS_BCH_LEN - tmp_str_len, tmp_str, tmp_str_len);
  id_str[DPL_UKS_BCH_LEN] = 0;

  ret = DPL_SUCCESS;

end:

  if (tmp_str) free(tmp_str);

  return ret;
}

dpl_id_scheme_t dpl_id_scheme_uks = {
    .name = "uks",
    .gen_random_key = dpl_uks_gen_random_key,
};

/** @} */
