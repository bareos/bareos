/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2026-2026 Bareos GmbH & Co. KG

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

#include "gtest/gtest.h"
#include "stored/device_wait_policy.h"

using namespace storagedaemon;

namespace {
// The values InitJcrDeviceWaitTimers() sets up for a job.
constexpr DeviceWaitTimes DefaultTimes()
{
  DeviceWaitTimes times{};

  times.min_wait = 60 * 60;
  times.max_wait = 24 * 60 * 60;
  times.max_num_wait = 9;
  times.wait_sec = times.min_wait;
  times.rem_wait_sec = times.wait_sec;
  times.num_wait = 0;

  return times;
}

// Waiting the given number of seconds, reports whether waiting may continue.
constexpr bool WaitFor(DeviceWaitTimes& times, int32_t seconds)
{
  return ConsumeDeviceWaitTime(times, seconds);
}

// Total seconds a job may wait before it gives up.
constexpr int32_t TotalBudget()
{
  DeviceWaitTimes times = DefaultTimes();
  int32_t total = 0;

  // Consume in whole rounds until the budget is exhausted.
  while (true) {
    int32_t round = times.rem_wait_sec;
    total += round;
    if (!ConsumeDeviceWaitTime(times, round)) { return total; }
  }
}
}  // namespace

// Time inside the current round does not end the wait.
static_assert([] {
  DeviceWaitTimes times = DefaultTimes();
  return WaitFor(times, 60);
}());

// Using up a round starts the next one, which is twice as long.
static_assert([] {
  DeviceWaitTimes times = DefaultTimes();
  WaitFor(times, times.rem_wait_sec);
  return times.wait_sec == 2 * 60 * 60 && times.num_wait == 1;
}());

// Rounds never grow beyond max_wait.
static_assert([] {
  DeviceWaitTimes times = DefaultTimes();
  for (int i = 0; i < 8; ++i) { WaitFor(times, times.rem_wait_sec); }
  return times.wait_sec == times.max_wait;
}());

// The budget is finite, so a job that never gets a device gives up.
static_assert([] {
  DeviceWaitTimes times = DefaultTimes();
  for (int i = 0; i < 1000; ++i) {
    if (!WaitFor(times, times.rem_wait_sec)) { return true; }
  }
  return false;
}());

// Exactly max_num_wait rounds are granted.
static_assert([] {
  DeviceWaitTimes times = DefaultTimes();
  int rounds = 0;
  while (WaitFor(times, times.rem_wait_sec)) { ++rounds; }
  return rounds == times.max_num_wait - 1;
}());

/* A round that grants no time must not stall the budget, otherwise a wait
 * that returns immediately would spin forever. */
static_assert([] {
  DeviceWaitTimes times{};
  times.max_num_wait = 3;
  for (int i = 0; i < 1000; ++i) {
    if (!ConsumeDeviceWaitTime(times, 0)) { return true; }
  }
  return false;
}());

// Waiting longer than the round grants still only consumes one round.
static_assert([] {
  DeviceWaitTimes times = DefaultTimes();
  WaitFor(times, times.rem_wait_sec * 100);
  return times.num_wait == 1;
}());

// The default budget is finite and reaches into the range of days.
static_assert(TotalBudget() > 0);
static_assert(TotalBudget() < 7 * 24 * 60 * 60);

TEST(DeviceWaitPolicy, WaitingWithinRoundContinues)
{
  DeviceWaitTimes times = DefaultTimes();

  EXPECT_TRUE(ConsumeDeviceWaitTime(times, 60));
  EXPECT_EQ(times.num_wait, 0);
  EXPECT_EQ(times.rem_wait_sec, times.min_wait - 60);
}

TEST(DeviceWaitPolicy, RoundsDoubleUpToMaximum)
{
  DeviceWaitTimes times = DefaultTimes();

  ConsumeDeviceWaitTime(times, times.rem_wait_sec);
  EXPECT_EQ(times.wait_sec, 2 * 60 * 60);

  ConsumeDeviceWaitTime(times, times.rem_wait_sec);
  EXPECT_EQ(times.wait_sec, 4 * 60 * 60);

  for (int i = 0; i < 10; ++i) {
    ConsumeDeviceWaitTime(times, times.rem_wait_sec);
  }
  EXPECT_EQ(times.wait_sec, times.max_wait);
}

// The regression this guards: waiting used to never end.
TEST(DeviceWaitPolicy, WaitingEventuallyGivesUp)
{
  DeviceWaitTimes times = DefaultTimes();

  int rounds = 0;
  while (ConsumeDeviceWaitTime(times, times.rem_wait_sec)) {
    ++rounds;
    ASSERT_LT(rounds, 1000) << "waiting for a device never ends";
  }

  EXPECT_EQ(rounds, times.max_num_wait - 1);
}

TEST(DeviceWaitPolicy, ZeroLengthWaitsStillExhaustBudget)
{
  DeviceWaitTimes times{};
  times.max_num_wait = 3;

  int rounds = 0;
  while (ConsumeDeviceWaitTime(times, 0)) {
    ++rounds;
    ASSERT_LT(rounds, 1000) << "zero length waits never end";
  }

  EXPECT_EQ(rounds, times.max_num_wait - 1);
}

TEST(DeviceWaitPolicy, NegativeDurationDoesNotExtendBudget)
{
  DeviceWaitTimes times = DefaultTimes();

  // A clock that appears to go backwards must not grant extra time.
  ConsumeDeviceWaitTime(times, -100);
  EXPECT_EQ(times.rem_wait_sec, times.min_wait);
}
