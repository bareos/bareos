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

#include "include/bareos.h"
#include "stored/autochanger.h"
#include "stored/autochanger_resource.h"
#include "stored/device_resource.h"
#include "stored/scsi_changer.h"
#include "testing_sd_common.h"

using namespace storagedaemon;

TEST(sd, NativeScsiAutochangerInitCopiesCommandAndDriveNumbers)
{
  InitSdGlobals();

  auto storage_config = StoragePrepareResources("configs/native_scsi_autochanger/");
  ASSERT_TRUE(storage_config);

  auto* changer = dynamic_cast<AutochangerResource*>(
      storage_config->GetResWithName(R_AUTOCHANGER, "NativeScsiAutochanger", false));
  ASSERT_TRUE(changer);

  auto* drive0 = dynamic_cast<DeviceResource*>(
      storage_config->GetResWithName(R_DEVICE, "NativeScsiDrive0", false));
  auto* drive1 = dynamic_cast<DeviceResource*>(
      storage_config->GetResWithName(R_DEVICE, "NativeScsiDrive1", false));
  ASSERT_TRUE(drive0);
  ASSERT_TRUE(drive1);

  ASSERT_TRUE(InitAutochangers());

  EXPECT_EQ(drive0->changer_res, changer);
  EXPECT_EQ(drive1->changer_res, changer);
  ASSERT_NE(drive0->changer_command, nullptr);
  ASSERT_NE(drive1->changer_command, nullptr);
  EXPECT_TRUE(IsNativeScsiChangerCommand(drive0->changer_command));
  EXPECT_TRUE(IsNativeScsiChangerCommand(drive1->changer_command));
  EXPECT_STREQ(drive0->changer_name, "/dev/null");
  EXPECT_STREQ(drive1->changer_name, "/dev/null");
  EXPECT_TRUE(drive0->drive_tapealert_enabled);
  EXPECT_FALSE(drive1->drive_tapealert_enabled);
  EXPECT_EQ(drive0->drive, 0);
  EXPECT_EQ(drive1->drive, 1);
}

TEST(sd, ScriptAutochangerInitKeepsFallbackCommand)
{
  InitSdGlobals();

  auto storage_config = StoragePrepareResources("configs/native_scsi_autochanger/");
  ASSERT_TRUE(storage_config);

  auto* changer = dynamic_cast<AutochangerResource*>(
      storage_config->GetResWithName(R_AUTOCHANGER, "ScriptAutochanger", false));
  ASSERT_TRUE(changer);

  auto* drive = dynamic_cast<DeviceResource*>(
      storage_config->GetResWithName(R_DEVICE, "ScriptChangerDrive0", false));
  ASSERT_TRUE(drive);

  ASSERT_TRUE(InitAutochangers());

  EXPECT_EQ(drive->changer_res, changer);
  ASSERT_NE(drive->changer_command, nullptr);
  EXPECT_FALSE(IsNativeScsiChangerCommand(drive->changer_command));
  EXPECT_STREQ(drive->changer_command,
               "/usr/lib/bareos/scripts/mtx-changer %c %o %S %a %d");
  EXPECT_STREQ(drive->changer_name, "/dev/null");
  EXPECT_EQ(drive->drive, 0);
}
