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
#include "stored/connect_wait.h"

using namespace storagedaemon;

namespace {
constexpr bool kNdmp = true;
constexpr bool kNative = false;
}  // namespace

// A configured value is used as given.
static_assert(SelectConnectWait(kNative, 1800, 180) == 1800);
static_assert(SelectConnectWait(kNative, 60, 180) == 60);
static_assert(SelectConnectWait(kNdmp, 1800, 180) == 180);
static_assert(SelectConnectWait(kNdmp, 1800, 42) == 42);

// NDMP jobs never inherit the much longer client deadline.
static_assert(SelectConnectWait(kNdmp, 1800, 180)
              < SelectConnectWait(kNative, 1800, 180));
static_assert(SelectConnectWait(kNdmp, 99999, 180) == 180);

// Unset values fall back to the respective default, not to each other.
static_assert(SelectConnectWait(kNative, 0, 180) == kDefaultClientConnectWait);
static_assert(SelectConnectWait(kNdmp, 1800, 0) == kDefaultNdmpConnectWait);
static_assert(SelectConnectWait(kNative, 0, 0) == kDefaultClientConnectWait);
static_assert(SelectConnectWait(kNdmp, 0, 0) == kDefaultNdmpConnectWait);

// Negative values are not usable as a deadline and fall back as well.
static_assert(SelectConnectWait(kNative, -1, 180) == kDefaultClientConnectWait);
static_assert(SelectConnectWait(kNdmp, 1800, -1) == kDefaultNdmpConnectWait);

// The defaults themselves keep the intended relation.
static_assert(kDefaultNdmpConnectWait > 0);
static_assert(kDefaultNdmpConnectWait < kDefaultClientConnectWait);

TEST(ConnectWait, UsesConfiguredValue)
{
  EXPECT_EQ(SelectConnectWait(kNative, 1800, 180), 1800);
  EXPECT_EQ(SelectConnectWait(kNdmp, 1800, 180), 180);
}

TEST(ConnectWait, NdmpDeadlineIsIndependentOfClientWait)
{
  // Raising Client Connect Wait must not extend the NDMP deadline.
  EXPECT_EQ(SelectConnectWait(kNdmp, 86400, 180), 180);
  EXPECT_EQ(SelectConnectWait(kNdmp, 1, 180), 180);
}

TEST(ConnectWait, UnsetValuesFallBackToDefaults)
{
  EXPECT_EQ(SelectConnectWait(kNative, 0, 180), kDefaultClientConnectWait);
  EXPECT_EQ(SelectConnectWait(kNdmp, 1800, 0), kDefaultNdmpConnectWait);
}

TEST(ConnectWait, ResultIsAlwaysAUsableDeadline)
{
  for (utime_t client : {utime_t{-1}, utime_t{0}, utime_t{1}, utime_t{1800}}) {
    for (utime_t ndmp : {utime_t{-1}, utime_t{0}, utime_t{1}, utime_t{180}}) {
      EXPECT_GT(SelectConnectWait(kNative, client, ndmp), 0);
      EXPECT_GT(SelectConnectWait(kNdmp, client, ndmp), 0);
    }
  }
}
