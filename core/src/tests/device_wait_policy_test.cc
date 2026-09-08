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

#include <chrono>

#include "gtest/gtest.h"
#include "stored/device_wait_policy.h"

using namespace storagedaemon;
using namespace std::chrono_literals;

namespace {
constexpr bool ConsumesWholeBudgetInOneWait()
{
  DeviceWaitBudget budget{60s};

  return !ConsumeDeviceWait(budget, 60s) && budget.remaining == 0s;
}

constexpr bool ConsumesBudgetInSteps()
{
  DeviceWaitBudget budget{60s};

  if (!ConsumeDeviceWait(budget, 20s)) { return false; }
  if (budget.remaining != 40s) { return false; }
  if (!ConsumeDeviceWait(budget, 20s)) { return false; }
  if (budget.remaining != 20s) { return false; }

  return !ConsumeDeviceWait(budget, 20s) && budget.remaining == 0s;
}

constexpr bool NeverGoesNegative()
{
  DeviceWaitBudget budget{10s};

  return !ConsumeDeviceWait(budget, 3600s) && budget.remaining == 0s;
}

/* A wakeup shorter than a second must still cost what it took. Truncating it
 * to zero would let repeated wakeups keep a job in the reservation loop for
 * good. */
constexpr bool SubSecondWaitIsNotTruncated()
{
  DeviceWaitBudget budget{60s};

  return ConsumeDeviceWait(budget, 500ms) && budget.remaining == 59500ms;
}

// Even a wait that measures as no time at all has to make progress.
constexpr bool WaitOfNoTimeStillCostsSomething()
{
  DeviceWaitBudget budget{60s};

  return ConsumeDeviceWait(budget, 0ms) && budget.remaining < 60s;
}

// So that a job woken up over and over eventually gives up.
constexpr bool RepeatedWakeupsExhaustBudget()
{
  DeviceWaitBudget budget{3 * kMinSignalledWaitCharge};

  if (!ConsumeDeviceWait(budget, 0ms)) { return false; }
  if (!ConsumeDeviceWait(budget, 0ms)) { return false; }

  return !ConsumeDeviceWait(budget, 0ms) && budget.remaining == 0ms;
}

// A wait that ran out of its granted time counts as a full round.
constexpr bool TimedOutWaitCostsAtLeastASecond()
{
  DeviceWaitBudget budget{60s};

  return ConsumeDeviceWait(budget, 0ms, kMinTimedOutWaitCharge)
         && budget.remaining == 59s;
}

constexpr bool NextWaitIsCappedByBudget()
{
  const DeviceWaitBudget budget{10s};

  return NextDeviceWait(budget, 60s) == 10s;
}

constexpr bool NextWaitIsCappedByInterval()
{
  const DeviceWaitBudget budget{24h};

  return NextDeviceWait(budget, 60s) == 60s;
}

constexpr bool ExhaustedBudgetGrantsNoWait()
{
  const DeviceWaitBudget budget{0s};

  return NextDeviceWait(budget, 60s) == 0s;
}

// A job starts out with a budget it can actually spend.
constexpr bool DefaultBudgetIsPositive()
{
  return DeviceWaitBudget{}.remaining == kMaxTotalDeviceWait
         && kMaxTotalDeviceWait > 0s && kDefaultDeviceWait > 0s;
}

static_assert(ConsumesWholeBudgetInOneWait());
static_assert(ConsumesBudgetInSteps());
static_assert(NeverGoesNegative());
static_assert(SubSecondWaitIsNotTruncated());
static_assert(WaitOfNoTimeStillCostsSomething());
static_assert(RepeatedWakeupsExhaustBudget());
static_assert(TimedOutWaitCostsAtLeastASecond());
static_assert(NextWaitIsCappedByBudget());
static_assert(NextWaitIsCappedByInterval());
static_assert(ExhaustedBudgetGrantsNoWait());
static_assert(DefaultBudgetIsPositive());
}  // namespace

TEST(DeviceWaitPolicy, ConsumesWholeBudgetInOneWait)
{
  EXPECT_TRUE(ConsumesWholeBudgetInOneWait());
}

TEST(DeviceWaitPolicy, ConsumesBudgetInSteps)
{
  EXPECT_TRUE(ConsumesBudgetInSteps());
}

TEST(DeviceWaitPolicy, NeverGoesNegative) { EXPECT_TRUE(NeverGoesNegative()); }

TEST(DeviceWaitPolicy, SubSecondWaitIsNotTruncated)
{
  EXPECT_TRUE(SubSecondWaitIsNotTruncated());
}

TEST(DeviceWaitPolicy, WaitOfNoTimeStillCostsSomething)
{
  EXPECT_TRUE(WaitOfNoTimeStillCostsSomething());
}

TEST(DeviceWaitPolicy, RepeatedWakeupsExhaustBudget)
{
  EXPECT_TRUE(RepeatedWakeupsExhaustBudget());
}

TEST(DeviceWaitPolicy, TimedOutWaitCostsAtLeastASecond)
{
  EXPECT_TRUE(TimedOutWaitCostsAtLeastASecond());
}

TEST(DeviceWaitPolicy, NextWaitIsCappedByBudget)
{
  EXPECT_TRUE(NextWaitIsCappedByBudget());
}

TEST(DeviceWaitPolicy, NextWaitIsCappedByInterval)
{
  EXPECT_TRUE(NextWaitIsCappedByInterval());
}

TEST(DeviceWaitPolicy, ExhaustedBudgetGrantsNoWait)
{
  EXPECT_TRUE(ExhaustedBudgetGrantsNoWait());
}

TEST(DeviceWaitPolicy, DefaultBudgetIsPositive)
{
  EXPECT_TRUE(DefaultBudgetIsPositive());
}
