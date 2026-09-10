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

#include <gtest/gtest.h>

#include "lib/windows_version.h"

using namespace windows_version;

// compile time tests for the constexpr helpers
static_assert(ArchitectureSuffix(Architecture::kX64) == ", 64-bit");
static_assert(ArchitectureSuffix(Architecture::kX86) == ", 32-bit");
static_assert(ArchitectureSuffix(Architecture::kArm64) == ", ARM64");
static_assert(ArchitectureSuffix(Architecture::kUnknown).empty());

static_assert(IsWindows11(10, 22000, ProductType::kWorkstation));
static_assert(IsWindows11(10, 26100, ProductType::kWorkstation));
static_assert(!IsWindows11(10, 19044, ProductType::kWorkstation));
static_assert(!IsWindows11(10, 26100, ProductType::kServer));
static_assert(!IsWindows11(6, 7601, ProductType::kWorkstation));

static_assert(MarketingName(6, 0, 6002, ProductType::kWorkstation)
              == "Windows Vista");
static_assert(MarketingName(6, 0, 6002, ProductType::kServer)
              == "Windows Server 2008");
static_assert(MarketingName(6, 1, 7601, ProductType::kWorkstation)
              == "Windows 7");
static_assert(MarketingName(6, 1, 7601, ProductType::kServer)
              == "Windows Server 2008 R2");
static_assert(MarketingName(6, 2, 9200, ProductType::kServer)
              == "Windows Server 2012");
static_assert(MarketingName(6, 3, 9600, ProductType::kWorkstation)
              == "Windows 8.1");
static_assert(MarketingName(6, 3, 9600, ProductType::kServer)
              == "Windows Server 2012 R2");
static_assert(MarketingName(10, 0, 19044, ProductType::kWorkstation)
              == "Windows 10");
static_assert(MarketingName(10, 0, 22621, ProductType::kWorkstation)
              == "Windows 11");
static_assert(MarketingName(10, 0, 14393, ProductType::kServer)
              == "Windows Server 2016");
static_assert(MarketingName(10, 0, 17763, ProductType::kServer)
              == "Windows Server 2019");
static_assert(MarketingName(10, 0, 20348, ProductType::kServer)
              == "Windows Server 2022");
static_assert(MarketingName(10, 0, 25398, ProductType::kServer)
              == "Windows Server, version 23H2");
static_assert(MarketingName(10, 0, 26100, ProductType::kServer)
              == "Windows Server 2025");

static_assert(IsLongTermServicingEdition("EnterpriseS"));
static_assert(IsLongTermServicingEdition("IoTEnterpriseS"));
static_assert(!IsLongTermServicingEdition("Enterprise"));
static_assert(!IsLongTermServicingEdition("Professional"));

static_assert(LtscDesignation(10240) == "LTSB 2015");
static_assert(LtscDesignation(14393) == "LTSB 2016");
static_assert(LtscDesignation(17763) == "LTSC 2019");
static_assert(LtscDesignation(19044) == "LTSC 2021");
static_assert(LtscDesignation(26100) == "LTSC 2024");

static_assert(ServicePackShortName("Service Pack 1") == "1");
static_assert(ServicePackShortName("Service Pack 12") == "12");
static_assert(ServicePackShortName("Service Pack ").empty());
static_assert(ServicePackShortName("").empty());
static_assert(ServicePackShortName("Update Rollup 1").empty());

static_assert(IsServerCore("Server Core"));
static_assert(!IsServerCore("Server"));
static_assert(!IsServerCore("Client"));

static_assert(ParseUint32("14393") == 14393);
static_assert(ParseUint32("0") == 0);
static_assert(ParseUint32("1.2") == 1);
static_assert(ParseUint32("") == 0);
static_assert(ParseUint32("abc") == 0);
static_assert(ParseUint32("007") == 7);

static_assert(EditionDisplayName("ServerStandard") == "Standard");
static_assert(EditionDisplayName("ServerDatacenter") == "Datacenter");
static_assert(EditionDisplayName("Professional") == "Professional");
static_assert(EditionDisplayName("Server") == "Server");

namespace {
struct TestCase {
  const char* name;
  VersionInfo info;
  const char* expected;
};

constexpr VersionInfo MakeVersionInfo(std::uint32_t major,
                                      std::uint32_t minor,
                                      std::uint32_t build,
                                      std::uint32_t ubr = 0,
                                      std::string_view product_name = {},
                                      std::string_view display_version = {},
                                      std::string_view edition_id = {},
                                      std::string_view installation_type = {},
                                      std::string_view csd_version = {},
                                      ProductType product_type
                                      = ProductType::kUnknown,
                                      Architecture arch
                                      = Architecture::kUnknown,
                                      bool is_winpe = false)
{
  return VersionInfo{.major = major,
                     .minor = minor,
                     .build = build,
                     .ubr = ubr,
                     .product_name = product_name,
                     .display_version = display_version,
                     .edition_id = edition_id,
                     .installation_type = installation_type,
                     .csd_version = csd_version,
                     .product_type = product_type,
                     .arch = arch,
                     .is_winpe = is_winpe};
}

// clang-format off
const TestCase kCases[] = {
    {"vista_sp2",
     MakeVersionInfo(6, 0, 6002, 0, "Windows Vista (TM) Business", {},
                     "Business", "Client", "Service Pack 2",
                     ProductType::kWorkstation, Architecture::kX86),
     "Microsoft Windows Vista (TM) Business SP2 (build 6002), 32-bit"},

    {"server_2008_sp2",
     MakeVersionInfo(6, 0, 6002, 0, "Windows Server (R) 2008 Standard", {},
                     "ServerStandard", {}, "Service Pack 2",
                     ProductType::kServer, Architecture::kX64),
     "Microsoft Windows Server (R) 2008 Standard SP2 (build 6002), 64-bit"},

    {"windows_7_sp1",
     MakeVersionInfo(6, 1, 7601, 24545, "Windows 7 Professional", {},
                     "Professional", "Client", "Service Pack 1",
                     ProductType::kWorkstation, Architecture::kX64),
     "Microsoft Windows 7 Professional SP1 (build 7601.24545), 64-bit"},

    {"server_2008_r2_sp1",
     MakeVersionInfo(6, 1, 7601, 0, "Windows Server 2008 R2 Standard", {},
                     "ServerStandard", "Server", "Service Pack 1",
                     ProductType::kServer, Architecture::kX64),
     "Microsoft Windows Server 2008 R2 Standard SP1 (build 7601), 64-bit"},

    {"server_2012",
     MakeVersionInfo(6, 2, 9200, 0, "Windows Server 2012 Standard", {},
                     "ServerStandard", "Server", {}, ProductType::kServer,
                     Architecture::kX64),
     "Microsoft Windows Server 2012 Standard (build 9200), 64-bit"},

    {"windows_8_1",
     MakeVersionInfo(6, 3, 9600, 20144, "Windows 8.1 Pro", {},
                     "Professional", "Client", {}, ProductType::kWorkstation,
                     Architecture::kX64),
     "Microsoft Windows 8.1 Pro (build 9600.20144), 64-bit"},

    {"windows_10_1607",
     MakeVersionInfo(10, 0, 14393, 7159, "Windows 10 Pro", "1607",
                     "Professional", "Client", {}, ProductType::kWorkstation,
                     Architecture::kX64),
     "Microsoft Windows 10 Pro 1607 (build 14393.7159), 64-bit"},

    {"windows_10_21h2",
     MakeVersionInfo(10, 0, 19044, 4529, "Windows 10 Pro", "21H2",
                     "Professional", "Client", {}, ProductType::kWorkstation,
                     Architecture::kX64),
     "Microsoft Windows 10 Pro 21H2 (build 19044.4529), 64-bit"},

    // LTSC has no LCUVer and frequently no DisplayVersion either
    {"windows_10_ltsc_2019",
     MakeVersionInfo(10, 0, 17763, 5936, "Windows 10 Enterprise", {},
                     "EnterpriseS", "Client", {}, ProductType::kWorkstation,
                     Architecture::kX64),
     "Microsoft Windows 10 Enterprise LTSC 2019 (build 17763.5936), 64-bit"},

    {"windows_10_ltsc_2021",
     MakeVersionInfo(10, 0, 19044, 4529, "Windows 10 Enterprise LTSC 2021",
                     "21H2", "EnterpriseS", "Client", {},
                     ProductType::kWorkstation, Architecture::kX64),
     "Microsoft Windows 10 Enterprise LTSC 2021 21H2 (build 19044.4529), "
     "64-bit"},

    {"windows_11_22h2",
     MakeVersionInfo(10, 0, 22621, 3737, "Windows 10 Pro", "22H2",
                     "Professional", "Client", {}, ProductType::kWorkstation,
                     Architecture::kX64),
     "Microsoft Windows 11 Pro 22H2 (build 22621.3737), 64-bit"},

    {"windows_11_24h2_arm64",
     MakeVersionInfo(10, 0, 26100, 4652, "Windows 10 Pro", "24H2",
                     "Professional", "Client", {}, ProductType::kWorkstation,
                     Architecture::kArm64),
     "Microsoft Windows 11 Pro 24H2 (build 26100.4652), ARM64"},

    {"server_2016",
     MakeVersionInfo(10, 0, 14393, 7159, "Windows Server 2016 Standard",
                     "1607", "ServerStandard", "Server", {}, ProductType::kServer,
                     Architecture::kX64),
     "Microsoft Windows Server 2016 Standard 1607 (build 14393.7159), 64-bit"},

    {"server_2019",
     MakeVersionInfo(10, 0, 17763, 5936, "Windows Server 2019 Datacenter",
                     "1809", "ServerDatacenter", "Server", {},
                     ProductType::kServer, Architecture::kX64),
     "Microsoft Windows Server 2019 Datacenter 1809 (build 17763.5936), "
     "64-bit"},

    {"server_2022_core",
     MakeVersionInfo(10, 0, 20348, 2527, "Windows Server 2022 Standard",
                     "21H2", "ServerStandard", "Server Core", {},
                     ProductType::kServer, Architecture::kX64),
     "Microsoft Windows Server 2022 Standard (Core installation) 21H2 "
     "(build 20348.2527), 64-bit"},

    {"server_2025",
     MakeVersionInfo(10, 0, 26100, 1742, "Windows Server 2025 Standard",
                     "24H2", "ServerStandard", "Server", {}, ProductType::kServer,
                     Architecture::kX64),
     "Microsoft Windows Server 2025 Standard 24H2 (build 26100.1742), 64-bit"},

    {"winpe",
     MakeVersionInfo(10, 0, 26100, 1, {}, {}, {}, "WindowsPE", {},
                     ProductType::kWorkstation, Architecture::kX64, true),
     "Windows PE (build 26100.1), 64-bit"},

    // WinPE keeps a ProductName of the image it was built from; it must not
    // be reported as a regular installation.
    {"winpe_with_product_name",
     MakeVersionInfo(6, 1, 7601, 0, "Windows 7 Enterprise", {}, {},
                     "WindowsPE", {}, ProductType::kWorkstation,
                     Architecture::kX86, true),
     "Windows PE (build 7601), 32-bit"},

    // registry unreadable: everything is derived from the version numbers
    {"no_registry_values_workstation",
     MakeVersionInfo(6, 1, 7601, 0, {}, {}, {}, {}, {}, ProductType::kWorkstation,
                     Architecture::kX64),
     "Microsoft Windows 7 (build 7601), 64-bit"},

    {"no_registry_values_server",
     MakeVersionInfo(10, 0, 20348, 0, {}, {}, {}, {}, {}, ProductType::kServer,
                     Architecture::kX64),
     "Microsoft Windows Server 2022 (build 20348), 64-bit"},

    {"no_product_name_uses_edition",
     MakeVersionInfo(10, 0, 22631, 4460, {}, "23H2", "Professional", {}, {},
                     ProductType::kWorkstation, Architecture::kX64),
     "Microsoft Windows 11 Professional 23H2 (build 22631.4460), 64-bit"},

    {"unknown_architecture_is_omitted",
     MakeVersionInfo(10, 0, 19045, 4529, "Windows 10 Pro", "22H2", {}, {}, {},
                     ProductType::kWorkstation),
     "Microsoft Windows 10 Pro 22H2 (build 19045.4529)"},

    // registry strings arrive 0-padded/whitespace-padded on some systems
    {"values_are_trimmed",
     MakeVersionInfo(10, 0, 19045, 0, "  Windows 10 Pro  ", " 22H2 ",
                     " Professional ", {}, {}, ProductType::kWorkstation,
                     Architecture::kX64),
     "Microsoft Windows 10 Pro 22H2 (build 19045), 64-bit"},

    {"product_name_already_has_vendor_prefix",
     MakeVersionInfo(10, 0, 19045, 0, "Microsoft Windows 10 Pro", {}, {}, {}, {},
                     ProductType::kWorkstation, Architecture::kX64),
     "Microsoft Windows 10 Pro (build 19045), 64-bit"},

    {"display_version_not_duplicated",
     MakeVersionInfo(10, 0, 19044, 0, "Windows 10 Enterprise LTSC 2021",
                     "21H2", "EnterpriseS", {}, {}, ProductType::kWorkstation,
                     Architecture::kX64),
     "Microsoft Windows 10 Enterprise LTSC 2021 21H2 (build 19044), 64-bit"},
};
// clang-format on

class WindowsVersionFormat : public ::testing::TestWithParam<TestCase> {};

TEST_P(WindowsVersionFormat, MatchesExpectedString)
{
  EXPECT_EQ(FormatVersion(GetParam().info), GetParam().expected);
}

INSTANTIATE_TEST_SUITE_P(
    Systems,
    WindowsVersionFormat,
    ::testing::ValuesIn(kCases),
    [](const ::testing::TestParamInfo<TestCase>& test_info) {
      return std::string{test_info.param.name};
    });

TEST(WindowsVersion, EmptyInfoDoesNotCrash)
{
  VersionInfo info{};
  EXPECT_FALSE(FormatVersion(info).empty());
}
}  // namespace
