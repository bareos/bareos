/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2023-2023 Bareos GmbH & Co. KG

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
  XxhashDigest(JobControlRecord* jcr, crypto_digest_t type);
  virtual bool Update(const uint8_t* data, uint32_t length) override;
  virtual bool Finalize(uint8_t* data, uint32_t* length) override;
};

XxhashDigest::XxhashDigest(JobControlRecord* jcr, crypto_digest_t type)
    : Digest(jcr, type)
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

class xxh3_exception : std::exception {
  const char* str{nullptr};

 public:
  xxh3_exception(const char* str) : str{str} {}

  const char* what() const noexcept override { return str; }
};

simple_checksum& simple_checksum::add(const char* begin, const char* end)
{
  if (begin != end) {
    if (state == nullptr) {
      state = XXH3_createState();
      if (state == nullptr) { throw xxh3_exception("xxh3 init error."); }

      XXH3_state_t* real_state = static_cast<XXH3_state_t*>(state);
      if (XXH3_128bits_reset(real_state) == XXH_ERROR) {
        XXH3_freeState(real_state);
        throw xxh3_exception("xxh3 reset error.");
      }
    }

    XXH3_state_t* real_state = static_cast<XXH3_state_t*>(state);
    if (real_state == nullptr) { throw xxh3_exception("xxh3 init error."); }

    if (XXH3_128bits_update(real_state, begin, end - begin) != XXH_OK) {
      throw xxh3_exception("xxh3 update error.");
    }
  }

  return *this;
}

std::vector<char> simple_checksum::finalize()
{
  if (state == nullptr) { state = XXH3_createState(); }

  XXH3_state_t* real_state = static_cast<XXH3_state_t*>(state);
  if (real_state == nullptr) { throw xxh3_exception("xxh3 init error."); }

  const XXH128_hash_t hash = XXH3_128bits_digest(real_state);
  XXH128_canonical_t canonical{};
  XXH128_canonicalFromHash(&canonical, hash);

  std::vector<char> vec;
  vec.resize(sizeof(canonical));
  std::memcpy(vec.data(), &canonical, sizeof(canonical));
  XXH3_freeState(real_state);
  state = nullptr;

  return vec;
}

simple_checksum::~simple_checksum()
{
  if (state) {
    XXH3_state_t* real_state = static_cast<XXH3_state_t*>(state);
    XXH3_freeState(real_state);
  }
}
