/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2022-2023 Bareos GmbH & Co. KG

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

#if defined(HAVE_MINGW)
#  include "include/bareos.h"
#  include "gtest/gtest.h"
#  include "gmock/gmock.h"
#else
#  include "gtest/gtest.h"
#  include "gmock/gmock.h"
#  include "include/bareos.h"
#endif

#include <vector>
#include <thread>
#include <random>

#define private public
#include "lib/channel.h"
#undef private


template <typename T>
static void SendDataBlocking(const std::vector<T>& to_send,
                             channel::input<T> in)
{
  for (auto& elem : to_send) {
    ASSERT_TRUE(in.emplace(elem)) << "channel closed unexpectedly";
  }
}

template <typename T>
static void ReceiveDataBlocking(std::vector<T>& to_receive,
                                channel::output<T> out)
{
  for (std::optional elem = out.get(); elem.has_value(); elem = out.get()) {
    to_receive.push_back(elem.value());
  }
}

template <typename T>
static void SendDataNonBlocking(const std::vector<T>& to_send,
                                channel::input<T> in)
{
  for (auto elem : to_send) {
    while (!in.try_emplace(elem)) {
      ASSERT_FALSE(in.closed()) << "channel closed unexpectedly";
    }
  }
}

template <typename T>
static void ReceiveDataNonBlocking(std::vector<T>& to_receive,
                                   channel::output<T> out)
{
  while (!out.closed()) {
    std::optional elem = out.try_get();
    if (elem.has_value()) { to_receive.push_back(*elem); }
  }
}

std::vector<int> RandomData(std::size_t size)
{
  std::vector<int> vec;
  std::minstd_rand rd(size);
  std::uniform_int_distribution dist;

  vec.reserve(size);

  for (std::size_t i = 0; i < size; ++i) { vec.push_back(dist(rd)); }

  return vec;
}

TEST(random, ConsistentRandData)
{
  ASSERT_EQ(RandomData(500), RandomData(500));
}

TEST(channel, BlockingConsistency)
{
  std::size_t size = 100'000;
  std::vector<int> input = RandomData(size);
  std::vector<int> output;
  auto [in, out] = channel::CreateBufferedChannel<int>(40);
  std::thread sender{&SendDataBlocking<int>, std::cref(input), std::move(in)};
  std::thread receiver{&ReceiveDataBlocking<int>, std::ref(output),
                       std::move(out)};

  sender.join();
  receiver.join();

  ASSERT_EQ(input.size(), output.size());
  for (std::size_t i = 0; i < input.size(); ++i) {
    EXPECT_EQ(input[i], output[i]) << "input and output differ at index " << i;
  }
}

TEST(channel, NonBlockingConsistency)
{
  std::size_t size = 100'000;
  std::vector<int> input = RandomData(size);
  std::vector<int> output;
  auto [in, out] = channel::CreateBufferedChannel<int>(40);
  std::thread sender{&SendDataNonBlocking<int>, std::cref(input),
                     std::move(in)};
  std::thread receiver{&ReceiveDataNonBlocking<int>, std::ref(output),
                       std::move(out)};

  sender.join();
  receiver.join();

  ASSERT_EQ(input.size(), output.size());
  for (std::size_t i = 0; i < input.size(); ++i) {
    EXPECT_EQ(input[i], output[i]) << "input and output differ at index " << i;
  }
}

TEST(channel, ConsistentState)
{
  auto [in, out] = channel::CreateBufferedChannel<int>(2);
  ASSERT_FALSE(in.closed());
  ASSERT_FALSE(out.closed());
  ASSERT_TRUE(in.emplace(1));
  ASSERT_TRUE(in.emplace(2));
  ASSERT_FALSE(in.closed());
  ASSERT_FALSE(out.closed());
  in.close();
  EXPECT_TRUE(in.closed());
  ASSERT_FALSE(out.closed());
  EXPECT_TRUE(out.get().has_value());
  ASSERT_FALSE(out.closed());
  EXPECT_TRUE(out.get().has_value());
  EXPECT_FALSE(out.get().has_value());
  EXPECT_TRUE(out.closed());

  EXPECT_FALSE(in.emplace(3));
}

TEST(channel, NoFalsePutsOrGets)
{
  std::size_t capacity = 20;
  auto [in, out] = channel::CreateBufferedChannel<int>(capacity);

  std::vector<int> data_in;

  for (std::size_t i = 0; i < capacity * 2; ++i) { data_in.push_back(i); }

  std::size_t num_writes = 0;
  for (int i : data_in) {
    if (in.try_emplace(i)) { num_writes += 1; }
  }

  EXPECT_EQ(num_writes, capacity);

  // we need to close in or otherwise out will always think
  // that data is still coming, causing the loop below to
  // spin endlessly
  in.close();

  std::vector<int> data_out;
  while (!out.closed()) {
    std::optional i = out.try_get();
    if (i.has_value()) { data_out.push_back(*i); }
  }

  EXPECT_EQ(num_writes, data_out.size());

  for (std::size_t i = 0; i < num_writes; ++i) {
    EXPECT_EQ(data_in[i], data_out[i]);
  }
}
