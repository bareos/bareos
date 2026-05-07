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

TEST(SdDiscoveryProbe, ClassifiesFilesystemTypes)
{
  EXPECT_TRUE(
      storagedaemon::IsFilesystemTypeIgnoredForStorageDiscovery("tmpfs"));
  EXPECT_FALSE(
      storagedaemon::IsFilesystemTypeIgnoredForStorageDiscovery("xfs"));

  EXPECT_TRUE(storagedaemon::IsLocalFilesystemTypeForStorageDiscovery("xfs"));
  EXPECT_TRUE(
      storagedaemon::IsLocalFilesystemTypeForStorageDiscovery("overlay"));
  EXPECT_FALSE(storagedaemon::IsLocalFilesystemTypeForStorageDiscovery("nfs"));
}

TEST(SdDiscoveryProbe, RecommendsArchivePathsFromMountpoints)
{
  EXPECT_EQ(storagedaemon::RecommendedArchivePathForMountpoint("/"),
            "/var/lib/bareos/storage");
  EXPECT_EQ(storagedaemon::RecommendedArchivePathForMountpoint("/srv/storage"),
            "/srv/storage/bareos/storage");
  EXPECT_EQ(storagedaemon::RecommendedArchivePathForMountpoint("/srv/storage/"),
            "/srv/storage/bareos/storage");
}

TEST(SdDiscoveryProbe, SerializesStableJsonShape)
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

  const auto json = storagedaemon::StorageDiscoveryReportToJson(report);
  json_error_t error {};
  json_t* parsed = json_loads(json.c_str(), 0, &error);

  ASSERT_NE(parsed, nullptr) << error.text;
  EXPECT_STREQ(json_string_value(json_object_get(parsed, "hostname")),
               "sdhost");
  EXPECT_STREQ(json_string_value(json_object_get(parsed, "fqdn")),
               "sdhost.example.com");

  json_t* filesystems = json_object_get(parsed, "filesystems");
  ASSERT_TRUE(json_is_array(filesystems));
  ASSERT_EQ(json_array_size(filesystems), 1U);

  json_t* filesystem = json_array_get(filesystems, 0);
  ASSERT_NE(filesystem, nullptr);
  EXPECT_STREQ(json_string_value(json_object_get(filesystem, "mountpoint")),
               "/srv/storage");

  json_t* deduplication = json_object_get(filesystem, "deduplication");
  ASSERT_TRUE(json_is_object(deduplication));
  EXPECT_STREQ(
      json_string_value(json_object_get(deduplication, "support")), "unknown");
  EXPECT_TRUE(
      json_is_null(json_object_get(deduplication, "block_size_bytes")));
  EXPECT_TRUE(json_is_null(json_object_get(deduplication, "mode")));
  EXPECT_TRUE(json_is_null(json_object_get(deduplication, "source")));
  EXPECT_TRUE(json_is_null(json_object_get(deduplication, "note")));

  EXPECT_TRUE(json_is_array(json_object_get(parsed, "tape_devices")));
  EXPECT_TRUE(json_is_array(json_object_get(parsed, "changers")));

  json_decref(parsed);
}

TEST(SdDiscoveryProbe, ProbesFilesystemCandidatesForCurrentHost)
{
  const auto report = storagedaemon::ProbeStorageDiscoveryReport();

  EXPECT_FALSE(report.hostname.empty());
  EXPECT_FALSE(report.fqdn.empty());
  EXPECT_FALSE(report.filesystems.empty());
}
