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

TEST(DeviceWaitPolicy, ConsumesWholeBudgetInOneWait)
{
  DeviceWaitBudget budget{60s};

  EXPECT_FALSE(budget.Consume(60s));
  EXPECT_EQ(budget.remaining, 0ms);
}

TEST(DeviceWaitPolicy, ConsumesBudgetInSteps)
{
  DeviceWaitBudget budget{60s};

  EXPECT_TRUE(budget.Consume(20s));
  EXPECT_EQ(budget.remaining, 40s);
  EXPECT_TRUE(budget.Consume(20s));
  EXPECT_EQ(budget.remaining, 20s);
  EXPECT_FALSE(budget.Consume(20s));
  EXPECT_EQ(budget.remaining, 0ms);
}

TEST(DeviceWaitPolicy, NeverGoesNegative)
{
  DeviceWaitBudget budget{10s};

  EXPECT_FALSE(budget.Consume(3600s));
  EXPECT_EQ(budget.remaining, 0ms);
}

TEST(DeviceWaitPolicy, SubSecondElapsedTimeIsNotTruncated)
{
  DeviceWaitBudget budget{60s};

  EXPECT_TRUE(budget.Consume(500ms));
  EXPECT_EQ(budget.remaining, 59500ms);
}

TEST(DeviceWaitPolicy, ZeroElapsedTimeStillMakesProgress)
{
  DeviceWaitBudget budget{3ms};

  EXPECT_TRUE(budget.Consume(0ms));
  EXPECT_EQ(budget.remaining, 2ms);
  EXPECT_TRUE(budget.Consume(0ms));
  EXPECT_EQ(budget.remaining, 1ms);
  EXPECT_FALSE(budget.Consume(0ms));
  EXPECT_EQ(budget.remaining, 0ms);
}
