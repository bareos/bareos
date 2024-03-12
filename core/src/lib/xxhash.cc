/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2023-2024 Bareos GmbH & Co. KG

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   Affero General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
*/
#include <cstring>
#if defined(XXHASH_ENABLE_DISPATCH)
#  include <xxh_x86dispatch.h>
#else
#  include <xxhash.h>
#endif

#include "crypto.h"
#include "xxhash.h"

class XxhashDigestState {
  XXH3_state_t* state;

 public:
  XxhashDigestState() : state(XXH3_createState())
  {
    if (state == nullptr) throw DigestInitException{};
  }
  ~XxhashDigestState() { XXH3_freeState(state); }
  XXH3_state_t* operator*() { return state; }
};

class XxhashDigest : public Digest {
  XxhashDigestState state{};

 public:
  XxhashDigest(JobControlRecord* t_jcr, crypto_digest_t t_type);
  virtual bool Update(const uint8_t* data, uint32_t length) override;
  virtual bool Finalize(uint8_t* data, uint32_t* length) override;
};

XxhashDigest::XxhashDigest(JobControlRecord* t_jcr, crypto_digest_t t_type)
    : Digest(t_jcr, t_type)
{
  if (XXH3_128bits_reset(*state) == XXH_ERROR) { throw DigestInitException{}; }
}

bool XxhashDigest::Update(const uint8_t* data, uint32_t length)
{
  return XXH3_128bits_update(*state, data, length) == XXH_OK;
}

bool XxhashDigest::Finalize(uint8_t* data, uint32_t* length)
{
  const XXH128_hash_t hash = XXH3_128bits_digest(*state);
  XXH128_canonical_t canonical{};
  XXH128_canonicalFromHash(&canonical, hash);
  *length = sizeof(canonical);
  std::memcpy(data, &canonical, sizeof(canonical));
  return true;
}

DIGEST* XxhashDigestNew(JobControlRecord* jcr, crypto_digest_t type)
{
  return new XxhashDigest(jcr, type);
}
