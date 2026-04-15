/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2026 Bareos GmbH & Co. KG

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
#include "setup_steps.h"

#include <gtest/gtest.h>

TEST(BareosSetupStepsShared, BuildsDefaultPackageList)
{
  EXPECT_EQ(BuildDefaultPackageList(),
            (std::vector<std::string>{"bareos-filedaemon",
                                      "bareos-director",
                                      "bareos-storage",
                                      "bareos-storage-tape",
                                      "bareos-storage-dedupable",
                                      "bareos-database-tools",
                                      "bareos-tools",
                                      "bareos-webui-vue"}));
}

TEST(BareosSetupStepsShared, SuggestsSingleChangerAssignments)
{
  const std::vector<TapeChangerInfo> changers = {
      {"/dev/tape/by-id/changer-0", "/dev/sg4", "Changer", "", "", "", "", "",
       {}, {}, {}},
  };
  const std::vector<TapeDriveInfo> drives = {
      {"/dev/tape/by-id/drive-0-nst", "/dev/nst0", "Drive0", "", "", "", "",
       "", {}, {}},
      {"/dev/tape/by-id/drive-1-nst", "/dev/nst1", "Drive1", "", "", "", "",
       "", {}, {}},
  };

  const auto assignments = SuggestTapeAssignments(changers, drives);

  ASSERT_EQ(assignments.size(), 1U);
  EXPECT_EQ(assignments.front().changer_path, "/dev/tape/by-id/changer-0");
  EXPECT_EQ(assignments.front().drive_paths,
            (std::vector<std::string>{"/dev/tape/by-id/drive-0-nst",
                                      "/dev/tape/by-id/drive-1-nst"}));
}

TEST(BareosSetupStepsShared, SuggestsAllDrivesForEveryChangerWithoutMatches)
{
  const std::vector<TapeChangerInfo> changers = {
      {"/dev/tape/by-id/changer-0", "/dev/sg4", "Changer0", "", "", "", "", "",
       {}, {}, {}},
      {"/dev/tape/by-id/changer-1", "/dev/sg5", "Changer1", "", "", "", "", "",
       {}, {}, {}},
  };
  const std::vector<TapeDriveInfo> drives = {
      {"/dev/tape/by-id/drive-0-nst", "/dev/nst0", "Drive0", "", "", "", "", "",
       {}, {}},
      {"/dev/tape/by-id/drive-1-nst", "/dev/nst1", "Drive1", "", "", "", "", "",
       {}, {}},
  };

  const auto assignments = SuggestTapeAssignments(changers, drives);

  ASSERT_EQ(assignments.size(), 2U);
  EXPECT_EQ(assignments[0].drive_paths,
            (std::vector<std::string>{"/dev/tape/by-id/drive-0-nst",
                                      "/dev/tape/by-id/drive-1-nst"}));
  EXPECT_EQ(assignments[1].drive_paths,
            (std::vector<std::string>{"/dev/tape/by-id/drive-0-nst",
                                      "/dev/tape/by-id/drive-1-nst"}));
}

TEST(BareosSetupStepsShared, ParsesVpdPageDeviceIdentifiers)
{
  const std::vector<uint8_t> page = {
      0x01, 0x83, 0x00, 0x42, 0x02, 0x01, 0x00, 0x22, 0x48, 0x50, 0x45, 0x20,
      0x20, 0x20, 0x20, 0x20, 0x55, 0x6c, 0x74, 0x72, 0x69, 0x75, 0x6d, 0x20,
      0x39, 0x2d, 0x53, 0x43, 0x53, 0x49, 0x20, 0x20, 0x30, 0x31, 0x46, 0x41,
      0x42, 0x31, 0x32, 0x38, 0x31, 0x32, 0x01, 0x03, 0x00, 0x08, 0x20, 0x30,
      0x31, 0x46, 0x41, 0x32, 0x38, 0x32, 0x01, 0x94, 0x00, 0x04, 0x00, 0x00,
      0x00, 0x01, 0x01, 0x93, 0x00, 0x08, 0x21, 0x30, 0x31, 0x46, 0x41, 0x32,
      0x38, 0x32,
  };

  const auto identifiers = ParseDeviceIdentifiersVpdPage(page);

  ASSERT_EQ(identifiers.size(), 2U);
  EXPECT_EQ(identifiers[0].front(), 0x02);
  EXPECT_EQ(identifiers[0][1], 0x01);
  EXPECT_EQ(identifiers[0].back(), 0x32);
  EXPECT_EQ(DescribeDeviceIdentifier(identifiers[0]),
            "HPE Ultrium 9-SCSI 01FAB12812");
}

TEST(BareosSetupStepsShared, ParsesLibraryDriveIdentifiers)
{
  const std::vector<uint8_t> status = {
      0x00, 0x01, 0x00, 0x04, 0x00, 0x00, 0x00, 0xd0, 0x04, 0x00, 0x00, 0x32,
      0x00, 0x00, 0x00, 0xc8, 0x01, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x02, 0x01, 0x00, 0x22, 0x48, 0x50, 0x45, 0x20,
      0x20, 0x20, 0x20, 0x20, 0x55, 0x6c, 0x74, 0x72, 0x69, 0x75, 0x6d, 0x20,
      0x39, 0x2d, 0x53, 0x43, 0x53, 0x49, 0x20, 0x20, 0x34, 0x45, 0x37, 0x37,
      0x46, 0x45, 0x34, 0x31, 0x35, 0x46, 0x01, 0x01, 0x08, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x01, 0x00, 0x22, 0x48, 0x50,
      0x45, 0x20, 0x20, 0x20, 0x20, 0x20, 0x55, 0x6c, 0x74, 0x72, 0x69, 0x75,
      0x6d, 0x20, 0x39, 0x2d, 0x53, 0x43, 0x53, 0x49, 0x20, 0x20, 0x34, 0x33,
      0x35, 0x42, 0x30, 0x30, 0x39, 0x42, 0x34, 0x33, 0x01, 0x02, 0x08, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x01, 0x00, 0x22,
      0x48, 0x50, 0x45, 0x20, 0x20, 0x20, 0x20, 0x20, 0x55, 0x6c, 0x74, 0x72,
      0x69, 0x75, 0x6d, 0x20, 0x39, 0x2d, 0x53, 0x43, 0x53, 0x49, 0x20, 0x20,
      0x34, 0x37, 0x43, 0x31, 0x33, 0x35, 0x41, 0x31, 0x34, 0x37, 0x01, 0x03,
      0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x01,
      0x00, 0x22, 0x48, 0x50, 0x45, 0x20, 0x20, 0x20, 0x20, 0x20, 0x55, 0x6c,
      0x74, 0x72, 0x69, 0x75, 0x6d, 0x20, 0x39, 0x2d, 0x53, 0x43, 0x53, 0x49,
      0x20, 0x20, 0x30, 0x31, 0x46, 0x41, 0x42, 0x31, 0x32, 0x38, 0x31, 0x32,
  };

  const auto identifiers = ParseTapeLibraryDriveIdentifiers(status);

  ASSERT_EQ(identifiers.size(), 4U);
  EXPECT_EQ(identifiers[0].element_address, 256);
  ASSERT_EQ(identifiers[0].device_identifiers.size(), 1U);
  EXPECT_EQ(identifiers[0].device_identifiers[0][0], 0x02);
  EXPECT_EQ(identifiers[3].device_identifiers[0].back(), 0x32);
}

TEST(BareosSetupStepsShared, MatchesLibraryDrivesByIdentifier)
{
  const auto drive_page = ParseDeviceIdentifiersVpdPage({
      0x01, 0x83, 0x00, 0x2a, 0x02, 0x01, 0x00, 0x22, 0x48, 0x50, 0x45, 0x20,
      0x20, 0x20, 0x20, 0x20, 0x55, 0x6c, 0x74, 0x72, 0x69, 0x75, 0x6d, 0x20,
      0x39, 0x2d, 0x53, 0x43, 0x53, 0x49, 0x20, 0x20, 0x30, 0x31, 0x46, 0x41,
      0x42, 0x31, 0x32, 0x38, 0x31, 0x32,
  });
  const auto library = ParseTapeLibraryDriveIdentifiers({
      0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x3a, 0x04, 0x00, 0x00, 0x32,
      0x00, 0x00, 0x00, 0x32, 0x01, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x02, 0x01, 0x00, 0x22, 0x48, 0x50, 0x45, 0x20,
      0x20, 0x20, 0x20, 0x20, 0x55, 0x6c, 0x74, 0x72, 0x69, 0x75, 0x6d, 0x20,
      0x39, 0x2d, 0x53, 0x43, 0x53, 0x49, 0x20, 0x20, 0x30, 0x31, 0x46, 0x41,
      0x42, 0x31, 0x32, 0x38, 0x31, 0x32,
  });

  ASSERT_EQ(drive_page.size(), 1U);
  ASSERT_EQ(library.size(), 1U);

  const std::vector<TapeChangerInfo> changers = {
      {"/dev/tape/by-id/changer-0",
       "/dev/sg4",
       "Changer",
       "",
       "",
       "",
       "",
       "",
       {},
       {},
       library},
  };
  const std::vector<TapeDriveInfo> drives = {
      {"/dev/tape/by-id/drive-0-nst",
       "/dev/nst0",
       "Drive0",
       "",
       "",
       "",
       "",
       "",
       {},
       drive_page},
  };

  const auto assignments = SuggestTapeAssignments(changers, drives);

  ASSERT_EQ(assignments.size(), 1U);
  EXPECT_EQ(assignments[0].drive_paths,
            (std::vector<std::string>{"/dev/tape/by-id/drive-0-nst"}));
}

TEST(BareosSetupStepsShared, AllowsReusedTapeDriveAcrossLibraries)
{
  DiskStorageConfig disk_config;
  TapeStorageConfig tape_config;
  tape_config.enabled = true;
  tape_config.assignments = {
      {"/dev/changer0", {"/dev/nst0"}},
      {"/dev/changer1", {"/dev/nst0"}},
  };

  std::string error;
  EXPECT_TRUE(ValidateStorageConfig(disk_config, tape_config, error));
  EXPECT_TRUE(error.empty());
}

TEST(BareosSetupStepsShared, BuildsTapeStorageScript)
{
  DiskStorageConfig disk_config;
  TapeStorageConfig tape_config;
  tape_config.enabled = true;
  tape_config.assignments = {
      {"/dev/tape/by-id/changer-0",
       {"/dev/tape/by-id/drive-0-nst", "/dev/tape/by-id/drive-1-nst"}},
  };

  const auto script = BuildConfigureStorageScript(disk_config, tape_config);

  EXPECT_NE(script.find("Changer Device = /dev/tape/by-id/changer-0"),
            std::string::npos);
  EXPECT_NE(script.find("ArchiveDevice = /dev/tape/by-id/drive-0-nst"),
            std::string::npos);
  EXPECT_NE(script.find("ArchiveDevice = /dev/tape/by-id/drive-1-nst"),
            std::string::npos);
}

TEST(BareosSetupStepsShared, EscapesAdminUserCommandArguments)
{
  const auto cmd
      = BuildAdminUserCmd("admin/user", "pa\"ss\\word and spaces");

  ASSERT_EQ(cmd.size(), 3U);
  EXPECT_EQ(cmd[0], "bash");
  EXPECT_EQ(cmd[1], "-c");
  EXPECT_NE(cmd[2].find("console/admin_user.conf"), std::string::npos);
  EXPECT_NE(cmd[2].find("Name = \"admin/user\""), std::string::npos);
  EXPECT_NE(cmd[2].find("Password = \"pa\\\"ss\\\\word and spaces\""),
            std::string::npos);
  EXPECT_NE(cmd[2].find("cat > '/etc/bareos/bareos-dir.d/console/admin_user.conf'"),
            std::string::npos);
}
