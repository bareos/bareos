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

#ifndef BAREOS_STORED_SD_DISCOVERY_PROBE_H_
#define BAREOS_STORED_SD_DISCOVERY_PROBE_H_

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace storagedaemon {

struct FilesystemDeduplicationInfo {
  std::string support{"unknown"};
  std::optional<uint64_t> block_size_bytes{};
  std::optional<std::string> mode{};
  std::optional<std::string> source{};
  std::optional<std::string> note{};
};

struct FilesystemCandidate {
  std::string mountpoint{};
  std::string filesystem_type{};
  uint64_t total_bytes{0};
  uint64_t free_bytes{0};
  bool writable{false};
  bool local{false};
  bool suitable_for_archive{false};
  std::string recommended_archive_path{};
  FilesystemDeduplicationInfo deduplication{};
};

struct TapeDeviceInfo {
  std::string device_node{};
  std::string generic_device_node{};
  std::string vendor{};
  std::string model{};
  std::string serial{};
  bool accessible{false};
  std::string accessibility_error{};
};

struct ChangerDriveInfo {
  std::string tape_device_node{};
  std::string generic_device_node{};
  std::optional<uint32_t> drive_element_address{};
  std::optional<std::string> device_identifier{};
  std::optional<std::string> source{};
};

struct ChangerInfo {
  std::string device_node{};
  std::string vendor{};
  std::string model{};
  std::string serial{};
  std::vector<std::string> drive_device_nodes{};
  std::vector<ChangerDriveInfo> drives{};
  bool accessible{false};
  std::string accessibility_error{};
};

struct StorageDiscoveryReport {
  std::string hostname{};
  std::string fqdn{};
  std::vector<FilesystemCandidate> filesystems{};
  std::vector<TapeDeviceInfo> tape_devices{};
  std::vector<ChangerInfo> changers{};
};

bool IsFilesystemTypeIgnoredForStorageDiscovery(
    std::string_view filesystem_type);
bool IsLocalFilesystemTypeForStorageDiscovery(std::string_view filesystem_type);
std::string RecommendedArchivePathForMountpoint(std::string_view mountpoint);

StorageDiscoveryReport ProbeStorageDiscoveryReport();
std::string StorageDiscoveryReportToJson(const StorageDiscoveryReport& report);

}  // namespace storagedaemon

#endif  // BAREOS_STORED_SD_DISCOVERY_PROBE_H_
