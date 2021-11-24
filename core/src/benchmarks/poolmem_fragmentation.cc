/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2021-2021 Bareos GmbH & Co. KG

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
#include <vector>
#include <random>
#include <algorithm>

#include <benchmark/benchmark.h>
#include <lib/mem_pool.h>

#include <iostream>

namespace bm = benchmark;

double operator"" _percent(unsigned long long int p) { return p / 100.0; }

double operator"" _percent(long double p) { return p / 100.0; }

static void allocate_memory(size_t count, std::vector<POOLMEM*>& handles)
{
  for (size_t i{0}; i < count; ++i) {
    handles.emplace_back(GetPoolMemory(PM_NAME));
  }
}

static void fragment_pool_mem(size_t allocations,
                              double free_percentage,
                              std::vector<POOLMEM*>& pool_handles)
{
  allocate_memory(allocations, pool_handles);

  /*
   * initialize a mersenne twister RNG with fixed seed, so
   * we get the same "randomness" everywhere
   */
  std::mt19937 stable_rng{1};
  std::shuffle(pool_handles.begin(), pool_handles.end(), stable_rng);

  const auto free_items{static_cast<size_t>(allocations * free_percentage)};
  for (size_t i{0}; i < free_items; ++i) {
    FreePoolMemory(pool_handles.back());
    pool_handles.pop_back();
  }
}


class Fragmentator {
  std::vector<POOLMEM*> pool_handles{};

 public:
  Fragmentator() { fragment_pool_mem(100'000, 50_percent, pool_handles); }

  ~Fragmentator()
  {
    while (pool_handles.size() > 0) {
      FreePoolMemory(pool_handles.back());
      pool_handles.pop_back();
    }
  }
};

// constructed before main() is called
static Fragmentator frag;

static void BM_PoolMem(bm::State& state)
{
  for (auto _ : state) {
    PoolMem string;
    bm::DoNotOptimize(string);
  }
}
BENCHMARK(BM_PoolMem);

static void BM_stdString(bm::State& state)
{
  for (auto _ : state) {
    std::string string;
    bm::DoNotOptimize(string);
  }
}
BENCHMARK(BM_stdString);

BENCHMARK_MAIN();
