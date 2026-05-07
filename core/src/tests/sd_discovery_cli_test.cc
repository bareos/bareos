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

#include <jansson.h>

#include "stored/sd_discovery_probe.h"
#include "tools/sd_discovery_cli.h"

namespace {

storagedaemon::StorageDiscoveryReport ExampleReport()
{
  storagedaemon::StorageDiscoveryReport report;
  report.hostname = "sdhost";
  report.fqdn = "sdhost.example.com";
  report.filesystems.push_back({
      .mountpoint = "/srv/storage",
      .filesystem_type = "xfs",
      .total_bytes = 4096,
      .free_bytes = 2048,
      .writable = true,
      .local = true,
      .suitable_for_archive = true,
      .recommended_archive_path = "/srv/storage/bareos/storage",
  });
  report.tape_devices.push_back({
      .device_node = "/dev/nst0",
      .generic_device_node = "/dev/sg3",
      .vendor = "IBM",
      .model = "ULTRIUM-HH8",
      .serial = "ABC123",
      .accessible = true,
  });
  report.changers.push_back({
      .device_node = "/dev/sg4",
      .vendor = "IBM",
      .model = "3573-TL",
      .serial = "CHG1",
      .drive_device_nodes = {"/dev/nst0"},
      .accessible = true,
  });
  return report;
}

}  // namespace

TEST(SdDiscoveryCli, ParsesReportSections)
{
  EXPECT_EQ(storagedaemon::discoverycli::ParseReportSection("full"),
            storagedaemon::discoverycli::ReportSection::kFull);
  EXPECT_EQ(storagedaemon::discoverycli::ParseReportSection("filesystems"),
            storagedaemon::discoverycli::ReportSection::kFilesystems);
  EXPECT_EQ(storagedaemon::discoverycli::ParseReportSection("tape"),
            storagedaemon::discoverycli::ReportSection::kTape);
  EXPECT_EQ(storagedaemon::discoverycli::ParseReportSection("bogus"),
            std::nullopt);
}

TEST(SdDiscoveryCli, FiltersFilesystemSection)
{
  auto filtered = storagedaemon::discoverycli::FilterDiscoveryReport(
      ExampleReport(), storagedaemon::discoverycli::ReportSection::kFilesystems);

  EXPECT_EQ(filtered.filesystems.size(), 1U);
  EXPECT_TRUE(filtered.tape_devices.empty());
  EXPECT_TRUE(filtered.changers.empty());
}

TEST(SdDiscoveryCli, FiltersTapeSection)
{
  auto filtered = storagedaemon::discoverycli::FilterDiscoveryReport(
      ExampleReport(), storagedaemon::discoverycli::ReportSection::kTape);

  EXPECT_TRUE(filtered.filesystems.empty());
  EXPECT_EQ(filtered.tape_devices.size(), 1U);
  EXPECT_EQ(filtered.changers.size(), 1U);
}

TEST(SdDiscoveryCli, RendersFilteredJson)
{
  const auto json = storagedaemon::discoverycli::RenderDiscoveryReportJson(
      ExampleReport(), storagedaemon::discoverycli::ReportSection::kTape);
  json_error_t error {};
  json_t* parsed = json_loads(json.c_str(), 0, &error);

  ASSERT_NE(parsed, nullptr) << error.text;
  EXPECT_TRUE(json_is_array(json_object_get(parsed, "filesystems")));
  EXPECT_EQ(json_array_size(json_object_get(parsed, "filesystems")), 0U);
  EXPECT_EQ(json_array_size(json_object_get(parsed, "tape_devices")), 1U);
  EXPECT_EQ(json_array_size(json_object_get(parsed, "changers")), 1U);

  json_decref(parsed);
}
