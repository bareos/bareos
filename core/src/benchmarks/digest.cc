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

#include <benchmark/benchmark.h>
#include "include/baconfig.h"
#include "lib/crypto.h"
#include <array>
#include <random>

namespace bm = benchmark;

std::array<uint8_t, DEFAULT_NETWORK_BUFFER_SIZE> corpus;

bool init_corpus()
{
  std::mt19937 gen32;
  std::generate(corpus.begin(), corpus.end(), gen32);
  return true;
}
[[maybe_unused]] static bool corpus_initialized = init_corpus();

static void do_digest(crypto_digest_t t)
{
  DIGEST* digest = crypto_digest_new(nullptr, t);
  for (auto i = 0; i != 1000; i++) {
    CryptoDigestUpdate(digest, corpus.data(), corpus.size());
  }
  uint8_t dest[256];
  uint32_t length = 256;
  CryptoDigestFinalize(digest, dest, &length);
  CryptoDigestFree(digest);
}

static void BM_MD5(bm::State& state)
{
  for (auto _ : state) { do_digest(CRYPTO_DIGEST_MD5); }
}
BENCHMARK(BM_MD5);

static void BM_XXH128(bm::State& state)
{
  for (auto _ : state) { do_digest(CRYPTO_DIGEST_XXH128); }
}
BENCHMARK(BM_XXH128);

static void BM_SHA1(bm::State& state)
{
  for (auto _ : state) { do_digest(CRYPTO_DIGEST_SHA1); }
}
BENCHMARK(BM_SHA1);

static void BM_SHA256(bm::State& state)
{
  for (auto _ : state) { do_digest(CRYPTO_DIGEST_SHA256); }
}
BENCHMARK(BM_SHA256);

static void BM_SHA512(bm::State& state)
{
  for (auto _ : state) { do_digest(CRYPTO_DIGEST_SHA512); }
}
BENCHMARK(BM_SHA512);
