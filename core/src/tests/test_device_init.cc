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
#include "stored/device_init.h"

using storagedaemon::DeviceInitInProgress;
using storagedaemon::DeviceInitState;
using storagedaemon::DeviceInitStateToString;
using storagedaemon::SdDirCommandNeedsDeviceInit;

static_assert(DeviceInitStateToString(DeviceInitState::Pending) == "pending");
static_assert(DeviceInitStateToString(DeviceInitState::Initializing)
              == "initializing");
static_assert(DeviceInitStateToString(DeviceInitState::Ready) == "ready");
static_assert(DeviceInitStateToString(DeviceInitState::Failed) == "failed");

static_assert(DeviceInitInProgress(DeviceInitState::Pending));
static_assert(DeviceInitInProgress(DeviceInitState::Initializing));
static_assert(!DeviceInitInProgress(DeviceInitState::Ready));
static_assert(!DeviceInitInProgress(DeviceInitState::Failed));

// Commands that must be answered while devices are still initializing.
static_assert(!SdDirCommandNeedsDeviceInit("status"));
static_assert(!SdDirCommandNeedsDeviceInit(".status"));
static_assert(!SdDirCommandNeedsDeviceInit("setdebug="));
static_assert(!SdDirCommandNeedsDeviceInit("resolve"));
static_assert(!SdDirCommandNeedsDeviceInit("cancel"));
static_assert(!SdDirCommandNeedsDeviceInit("stats"));
static_assert(!SdDirCommandNeedsDeviceInit("getSecureEraseCmd"));

// Commands that use a device and therefore have to wait.
static_assert(SdDirCommandNeedsDeviceInit("autochanger"));
static_assert(SdDirCommandNeedsDeviceInit("JobId="));
static_assert(SdDirCommandNeedsDeviceInit("label"));
static_assert(SdDirCommandNeedsDeviceInit("mount"));
static_assert(SdDirCommandNeedsDeviceInit("unmount"));
static_assert(SdDirCommandNeedsDeviceInit("readlabel"));
static_assert(SdDirCommandNeedsDeviceInit("release"));
static_assert(SdDirCommandNeedsDeviceInit("use storage="));
static_assert(SdDirCommandNeedsDeviceInit("setdevice"));

// An unknown command must never be treated as device independent.
static_assert(SdDirCommandNeedsDeviceInit(""));
static_assert(SdDirCommandNeedsDeviceInit("statuses"));
static_assert(SdDirCommandNeedsDeviceInit("statu"));

TEST(device_init, state_to_string)
{
  EXPECT_EQ(DeviceInitStateToString(DeviceInitState::Pending), "pending");
  EXPECT_EQ(DeviceInitStateToString(DeviceInitState::Initializing),
            "initializing");
  EXPECT_EQ(DeviceInitStateToString(DeviceInitState::Ready), "ready");
  EXPECT_EQ(DeviceInitStateToString(DeviceInitState::Failed), "failed");
}

TEST(device_init, init_in_progress)
{
  EXPECT_TRUE(DeviceInitInProgress(DeviceInitState::Pending));
  EXPECT_TRUE(DeviceInitInProgress(DeviceInitState::Initializing));
  EXPECT_FALSE(DeviceInitInProgress(DeviceInitState::Ready));
  EXPECT_FALSE(DeviceInitInProgress(DeviceInitState::Failed));
}

TEST(device_init, device_independent_commands_do_not_wait)
{
  for (auto cmd : storagedaemon::kDeviceIndependentSdDirCommands) {
    EXPECT_FALSE(SdDirCommandNeedsDeviceInit(cmd)) << cmd;
  }
}

TEST(device_init, device_commands_wait)
{
  EXPECT_TRUE(SdDirCommandNeedsDeviceInit("JobId="));
  EXPECT_TRUE(SdDirCommandNeedsDeviceInit("use storage="));
  EXPECT_TRUE(SdDirCommandNeedsDeviceInit("mount"));
  EXPECT_TRUE(SdDirCommandNeedsDeviceInit("unknown command"));
}
