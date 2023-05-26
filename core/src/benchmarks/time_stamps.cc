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
#include <string>
#include <random>
#include <cmath>
#include <unordered_map>
#include <array>
#include <unordered_set>

#include "lib/time_stamps.h"

namespace bm = benchmark;

std::default_random_engine engine;

struct block { char start; char end; BlockIdentity id;};

static const std::array<block, 3> blocks{
  block{ '(', ')' , BlockIdentity{"brace"}},
  block{ '{', '}' , BlockIdentity{"curly brace"}},
  block{ '[', ']' , BlockIdentity{"bracket"}},
};


static std::string::iterator RandomBlock(std::string::iterator iter, std::size_t& num_blocks,
					 std::size_t depth = 0)
{
  std::uniform_int_distribution<std::size_t> blockdist(0, blocks.size() - 1);
  block b = blocks[blockdist(engine)];
  *iter = b.start;
  ++iter;
  std::binomial_distribution dist(1, 1.0/std::log2(2 + depth));
  while (num_blocks > 0 && dist(engine) == 1) {
    iter = RandomBlock(iter, --num_blocks, depth + 1);
  }
  *iter = b.end;
  return ++iter;
}

static std::string RandomBlocks(std::size_t num_blocks)
{
  std::string result(2 * num_blocks, ' ');
  std::string::iterator iter = result.begin();
  while (num_blocks > 0) {
    iter = RandomBlock(iter, --num_blocks);
  }
  return result;
}

static TimeKeeper keeper;
static void SubmitRandomEvents(bm::State& state)
{


  std::unordered_map<char, const BlockIdentity*> blockid;
  std::unordered_set<char> open;
  for (auto& block : blocks) {
    open.insert(block.start);
    auto [_1, ins_start] = blockid.try_emplace(block.start, &block.id);
    auto [_2, ins_end] = blockid.try_emplace(block.end, &block.id);
    // check that each block has distinct characters from each other
    // by confirming that each insert did actually create a new entry.
    assert(ins_start && "blocks have to have distinct characters from each other.");
    assert(ins_end && "blocks have to have distinct characters from each outer.");
  }

  auto timer = disabled.get_thread_local();
  auto num_blocks = state.range(0);

  std::string events = RandomBlocks(num_blocks);
  std::size_t numEvents = 0;
  for (auto _ : state) {
    for (char c : events) {
      if (open.find(c) != open.end()) {
	timer.enter(*blockid[c]);
      } else {
	timer.exit(*blockid[c]);
      }
    }
    numEvents += 2 * num_blocks;
  }

  state.counters["per event"] = benchmark::Counter(numEvents, benchmark::Counter::kIsRate | benchmark::Counter::kInvert);

}
BENCHMARK(SubmitRandomEvents)->Range(1 << 10, 1 << 20)->ThreadRange(1 << 0, 1 << 5);

BENCHMARK_MAIN();
