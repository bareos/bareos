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
#include <optional>

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

static std::optional<TimeKeeper> keeper;

static ThreadTimerHandle get_thread_local_timer(std::optional<TimeKeeper>& timer) {
  if (timer.has_value()) {
    if (auto* local = timer->get_thread_local()) {
      return ThreadTimerHandle{ *local };
    }
  }
  return ThreadTimerHandle{};
}

static void SetupKeeper(const bm::State& state) {
  if (state.range(1)) {
    keeper.emplace();
  }
}

static void TeardownKeeper(const bm::State&) {
  keeper.reset();
}

static void SubmitRandomEvents(bm::State& state)
{
  if (keeper.has_value()) {
    keeper->create_thread_local();
  }
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

  auto timer = get_thread_local_timer(keeper);
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
  if (keeper.has_value()) {
    keeper->erase_thread_local();
  }

  benchmark::DoNotOptimize(keeper);

  state.counters["per event"] = benchmark::Counter(numEvents, benchmark::Counter::kIsRate | benchmark::Counter::kInvert);

}

static void SubmitDeepStack(bm::State& state)
{
  if (keeper.has_value()) {
    keeper->create_thread_local();
  }
  auto timer = get_thread_local_timer(keeper);
  auto num_blocks = state.range(0);

  std::size_t numEvents = 0;
  static BlockIdentity block{"deep"};
  for (auto _ : state) {
    for (decltype(num_blocks) i = 0; i < num_blocks; ++i) {
      timer.enter(block);
    }
    for (decltype(num_blocks) i = 0; i < num_blocks; ++i) {
      timer.exit(block);
    }
    numEvents += 2 * num_blocks;
  }

  if (keeper.has_value()) {
    keeper->erase_thread_local();
  }
  benchmark::DoNotOptimize(keeper);
  state.counters["per event"] = benchmark::Counter(numEvents, benchmark::Counter::kIsRate | benchmark::Counter::kInvert);
}
static void SubmitShallowStack(bm::State& state)
{
  if (keeper.has_value()) {
    keeper->create_thread_local();
  }
  auto timer = get_thread_local_timer(keeper);
  auto num_blocks = state.range(0);

  std::size_t numEvents = 0;
  static BlockIdentity block{"deep"};
  for (auto _ : state) {
    for (decltype(num_blocks) i = 0; i < num_blocks; ++i) {
      timer.enter(block);
      timer.exit(block);
    }
    numEvents += 2 * num_blocks;
  }

  if (keeper.has_value()) {
    keeper->erase_thread_local();
  }
  benchmark::DoNotOptimize(keeper);
  state.counters["per event"] = benchmark::Counter(numEvents, benchmark::Counter::kIsRate | benchmark::Counter::kInvert);
}
BENCHMARK(SubmitRandomEvents)->Ranges({{1 << 4, 1 << 12}, {0, 1}})->ThreadRange(1 << 0, 1 << 5)->Setup(SetupKeeper)->Teardown(TeardownKeeper);
BENCHMARK(SubmitDeepStack)->Ranges({{1 << 4, 1 << 12}, {1, 1}})->ThreadRange(1 << 0, 1 << 5)->Setup(SetupKeeper)->Teardown(TeardownKeeper);
BENCHMARK(SubmitShallowStack)->Ranges({{1 << 4, 1 << 12}, {1, 1}})->ThreadRange(1 << 0, 1 << 5)->Setup(SetupKeeper)->Teardown(TeardownKeeper);

static void VectorPush(bm::State& state)
{
  std::vector<event::Event> v{};
  auto num_blocks = state.range(0);

  std::size_t numEvents = 0;
  static BlockIdentity block{"deep"};
  for (auto _ : state) {
    for (decltype(num_blocks) i = 0; i < num_blocks; ++i) {
      v.push_back(event::OpenEvent{block});
      v.push_back(event::CloseEvent{block});
    }
    numEvents += 2 * num_blocks;
  }
  benchmark::DoNotOptimize(v);

  state.counters["per event"] = benchmark::Counter(numEvents, benchmark::Counter::kIsRate | benchmark::Counter::kInvert);
}
BENCHMARK(VectorPush)->Range(1 << 4, 1 << 12)->ThreadRange(1 << 0, 1 << 5);

BENCHMARK_MAIN();
