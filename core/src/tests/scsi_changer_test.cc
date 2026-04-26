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

#include "stored/scsi_changer.h"
#include "stored/drive_tapealert_monitor.h"

#include <gtest/gtest.h>

#include <array>

using namespace storagedaemon;

TEST(sd, NativeScsiChangerCommandSelector)
{
  EXPECT_TRUE(IsNativeScsiChangerCommand("builtin:scsi"));
  EXPECT_FALSE(IsNativeScsiChangerCommand(""));
  EXPECT_FALSE(
      IsNativeScsiChangerCommand("/usr/lib/bareos/scripts/mtx-changer"));
}

TEST(sd, DriveDiagnosticDeviceCandidates)
{
  EXPECT_EQ(GetDriveDiagnosticDeviceCandidates(nullptr, nullptr).size(), 0U);

  auto archive_only = GetDriveDiagnosticDeviceCandidates(nullptr, "/dev/nst0");
  ASSERT_EQ(archive_only.size(), 1U);
  EXPECT_EQ(archive_only[0], "/dev/nst0");

  auto diag_and_archive
      = GetDriveDiagnosticDeviceCandidates("/dev/sg1", "/dev/nst0");
  ASSERT_EQ(diag_and_archive.size(), 2U);
  EXPECT_EQ(diag_and_archive[0], "/dev/sg1");
  EXPECT_EQ(diag_and_archive[1], "/dev/nst0");

  auto deduplicated
      = GetDriveDiagnosticDeviceCandidates("/dev/sg1", "/dev/sg1");
  ASSERT_EQ(deduplicated.size(), 1U);
  EXPECT_EQ(deduplicated[0], "/dev/sg1");
}

TEST(sd, ScsiChangerTransportElementAddresses)
{
  ScsiChangerElementAddressAssignment assignment{};
  assignment.mte_addr = 0x0010;
  assignment.mte_count = 3;

  auto transports = GetScsiChangerTransportElementAddresses(assignment);
  ASSERT_EQ(transports.size(), 3U);
  EXPECT_EQ(transports[0], 0x0010);
  EXPECT_EQ(transports[1], 0x0011);
  EXPECT_EQ(transports[2], 0x0012);

  assignment.mte_count = 0;
  EXPECT_TRUE(GetScsiChangerTransportElementAddresses(assignment).empty());
}

TEST(sd, FormatNativeScsiDiagnosticStatus)
{
  auto native = FormatNativeScsiDiagnosticStatus("builtin:scsi", "/dev/sg0",
                                                 "/dev/sg1", "/dev/nst0", true);
  EXPECT_NE(native.find("Changer backend: native SCSI"), std::string::npos);
  EXPECT_NE(native.find("Changer device: /dev/sg0"), std::string::npos);
  EXPECT_NE(native.find("Drive diagnostics via: /dev/sg1, /dev/nst0"),
            std::string::npos);
  EXPECT_NE(native.find("Drive TapeAlert monitoring: enabled"),
            std::string::npos);

  auto tapealert_only = FormatNativeScsiDiagnosticStatus(
      nullptr, nullptr, nullptr, "/dev/nst0", true);
  EXPECT_EQ(tapealert_only,
            "    Drive diagnostics via: /dev/nst0\n"
            "    Drive TapeAlert monitoring: enabled\n");

  auto empty = FormatNativeScsiDiagnosticStatus(nullptr, nullptr, nullptr,
                                                nullptr, false);
  EXPECT_TRUE(empty.empty());
}

TEST(sd, DriveTapealertPollingEvents)
{
  EXPECT_TRUE(IsDriveTapealertPollingEvent(bSdEventDeviceInit));
  EXPECT_TRUE(IsDriveTapealertPollingEvent(bSdEventVolumeLoad));
  EXPECT_TRUE(IsDriveTapealertPollingEvent(bSdEventLabelVerified));
  EXPECT_TRUE(IsDriveTapealertPollingEvent(bSdEventLabelWrite));
  EXPECT_TRUE(IsDriveTapealertPollingEvent(bSdEventReadError));
  EXPECT_TRUE(IsDriveTapealertPollingEvent(bSdEventWriteError));
  EXPECT_TRUE(IsDriveTapealertPollingEvent(bSdEventVolumeUnload));
  EXPECT_TRUE(IsDriveTapealertPollingEvent(bSdEventDeviceRelease));
  EXPECT_TRUE(IsDriveTapealertPollingEvent(bSdEventDeviceOpen));
  EXPECT_TRUE(IsDriveTapealertPollingEvent(bSdEventDeviceClose));
  EXPECT_FALSE(IsDriveTapealertPollingEvent(bSdEventJobEnd));
  EXPECT_FALSE(IsDriveTapealertPollingEvent(bSdEventChangerLock));
}

TEST(sd, ParseScsiChangerElementAddressAssignment)
{
  std::array<uint8_t, 24> page{};
  page[0] = 0x17;
  page[6] = 0x00;
  page[7] = 0x10;
  page[8] = 0x00;
  page[9] = 0x01;
  page[10] = 0x00;
  page[11] = 0x20;
  page[12] = 0x00;
  page[13] = 0x18;
  page[14] = 0x00;
  page[15] = 0x40;
  page[16] = 0x00;
  page[17] = 0x04;
  page[18] = 0x00;
  page[19] = 0x50;
  page[20] = 0x00;
  page[21] = 0x02;

  ScsiChangerElementAddressAssignment assignment;
  ASSERT_TRUE(ParseScsiChangerElementAddressAssignment(page.data(), page.size(),
                                                       assignment));
  EXPECT_EQ(assignment.mte_addr, 0x0010);
  EXPECT_EQ(assignment.mte_count, 1);
  EXPECT_EQ(assignment.se_addr, 0x0020);
  EXPECT_EQ(assignment.se_count, 0x18);
  EXPECT_EQ(assignment.iee_addr, 0x0040);
  EXPECT_EQ(assignment.iee_count, 4);
  EXPECT_EQ(assignment.dte_addr, 0x0050);
  EXPECT_EQ(assignment.dte_count, 2);
}

TEST(sd, RejectShortScsiChangerElementAddressAssignment)
{
  std::array<uint8_t, 24> page{};
  page[0] = 0x16;

  ScsiChangerElementAddressAssignment assignment;
  EXPECT_FALSE(ParseScsiChangerElementAddressAssignment(
      page.data(), page.size(), assignment));
}

TEST(sd, ParseScsiChangerElementStatus)
{
  std::array<uint8_t, 28> data{};
  data[2] = 0x00;
  data[3] = 0x01;
  data[6] = 0x00;
  data[7] = 0x14;

  data[8] = 0x04;
  data[10] = 0x00;
  data[11] = 0x0c;
  data[13] = 0x00;
  data[14] = 0x00;
  data[15] = 0x0c;

  data[16] = 0x00;
  data[17] = 0x50;
  data[18] = 0x01;
  data[20] = 0x11;
  data[21] = 0x22;
  data[25] = 0x80;
  data[26] = 0x00;
  data[27] = 0x23;

  std::vector<ScsiChangerElementStatus> elements;
  ASSERT_TRUE(
      ParseScsiChangerElementStatusData(data.data(), data.size(), elements));
  ASSERT_EQ(elements.size(), 1U);
  EXPECT_EQ(elements[0].element_type_code, 0x04);
  EXPECT_EQ(elements[0].element_address, 0x0050);
  EXPECT_TRUE(elements[0].full);
  EXPECT_TRUE(elements[0].source_valid);
  EXPECT_EQ(elements[0].asc, 0x11);
  EXPECT_EQ(elements[0].ascq, 0x22);
  EXPECT_EQ(elements[0].src_se_addr, 0x0023);
}

TEST(sd, ParseScsiLogSenseSupportedPages)
{
  std::array<uint8_t, 8> data{};
  data[0] = 0x00;
  data[2] = 0x00;
  data[3] = 0x04;
  data[4] = 0x00;
  data[5] = 0x02;
  data[6] = 0x2e;
  data[7] = 0x3f;

  std::vector<uint8_t> pages;
  ASSERT_TRUE(ParseScsiLogSenseSupportedPages(data.data(), data.size(), pages));
  ASSERT_EQ(pages.size(), 4U);
  EXPECT_EQ(pages[0], 0x00);
  EXPECT_EQ(pages[1], 0x02);
  EXPECT_EQ(pages[2], 0x2e);
  EXPECT_EQ(pages[3], 0x3f);
}

TEST(sd, RejectTruncatedScsiLogSenseSupportedPages)
{
  std::array<uint8_t, 7> data{};
  data[0] = 0x00;
  data[2] = 0x00;
  data[3] = 0x04;
  data[4] = 0x00;
  data[5] = 0x02;
  data[6] = 0x2e;

  std::vector<uint8_t> pages;
  EXPECT_FALSE(
      ParseScsiLogSenseSupportedPages(data.data(), data.size(), pages));
}

TEST(sd, RejectWrongScsiLogSensePage)
{
  std::array<uint8_t, 8> data{};
  data[0] = 0x2e;
  data[2] = 0x00;
  data[3] = 0x04;
  data[4] = 0x00;
  data[5] = 0x02;
  data[6] = 0x2e;
  data[7] = 0x3f;

  std::vector<uint8_t> pages;
  EXPECT_FALSE(
      ParseScsiLogSenseSupportedPages(data.data(), data.size(), pages));
}

TEST(sd, ParseEmptyScsiChangerElementStatus)
{
  std::array<uint8_t, 8> data{};
  std::vector<ScsiChangerElementStatus> elements;

  ASSERT_TRUE(
      ParseScsiChangerElementStatusData(data.data(), data.size(), elements));
  EXPECT_TRUE(elements.empty());
}
