/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2024-2026 Bareos GmbH & Co. KG

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
/**
 * @file
 * Shared command builders for the setup wizard steps.
 * Used by both the WebSocket session handler and the TUI wizard.
 */
#ifndef BAREOS_BAREOS_SETUP_SETUP_STEPS_H_
#define BAREOS_BAREOS_SETUP_SETUP_STEPS_H_

#include <cstdint>
#include <string>
#include <vector>

#include "command_runner.h"

struct CapturedCommandOutput {
  int exit_code = 0;
  std::string stdout_text;
  std::string stderr_text;
};

using DeviceIdentifier = std::vector<uint8_t>;

struct TapeChangerDriveIdentifier {
  int element_address = -1;
  std::vector<DeviceIdentifier> device_identifiers;
};

struct TapeDriveInfo {
  std::string path;
  std::string canonical_path;
  std::string display_name;
  std::string identifier;
  std::string serial_number;
  std::string vendor;
  std::string model;
  std::string firmware_version;
  std::vector<std::string> aliases;
  std::vector<DeviceIdentifier> device_identifiers;
};

struct TapeChangerStatus {
  int drives = 0;
  int robot_arms = 0;
  int slots = 0;
  int ie_slots = 0;
  std::string error;
};

struct TapeChangerInfo {
  std::string path;
  std::string canonical_path;
  std::string display_name;
  std::string identifier;
  std::string serial_number;
  std::string vendor;
  std::string model;
  std::string firmware_version;
  std::vector<std::string> aliases;
  TapeChangerStatus status;
  std::vector<TapeChangerDriveIdentifier> drive_identifiers;
};

struct TapeAssignment {
  std::string changer_path;
  std::vector<std::string> drive_paths;
};

struct TapeInventoryDriveEntry {
  int number = -1;
  bool loaded = false;
  std::string source_slot;
  std::string volume_tag;
};

struct TapeInventorySlotEntry {
  int number = -1;
  bool full = false;
  std::string volume_tag;
};

struct TapeChangerInventory {
  std::vector<TapeInventoryDriveEntry> drives;
  std::vector<TapeInventorySlotEntry> slots;
};

struct DiskStorageConfig {
  bool enabled = false;
  std::string storage_path;
  bool dedupable = false;
  int concurrent_jobs = 10;
};

struct TapeStorageConfig {
  bool enabled = false;
  std::vector<TapeAssignment> assignments;
};

struct DirectorStorageDefaults {
  std::string address = "localhost";
  std::string password;
  std::string port = "9103";
};

struct TapeStorageInventory {
  std::vector<TapeChangerInfo> changers;
  std::vector<TapeDriveInfo> drives;
  std::vector<TapeAssignment> suggested_assignments;
};

/** Capitalize the first letter (Bareos repo paths use e.g. "Fedora_43"). */
std::string CapFirst(std::string s);

/** Trim leading and trailing whitespace. */
std::string Trim(std::string value);

/** Build the fixed package list used by the setup wizard. */
std::vector<std::string> BuildDefaultPackageList();

/** Build the repository OS path segment for the detected distribution. */
std::string BuildRepoOsPath(const std::string& distro,
                            const std::string& version);

/**
 * Build the command to download and run add_bareos_repositories.sh.
 * For subscription repos, curl uses -u login:password.
 */
std::vector<std::string> BuildAddRepoCmd(const std::string& distro,
                                         const std::string& version,
                                         const std::string& repo_type,
                                         const std::string& login,
                                         const std::string& password);

// Build the package install command for the detected package manager.
std::vector<std::string> BuildInstallCmd(
    const std::string& pkg_mgr,
    const std::vector<std::string>& packages);

// Build the database initialization command (three Bareos scripts).
std::vector<std::string> BuildDbCmd();

// Build the command to create a Bareos WebUI admin console user.
std::vector<std::string> BuildAdminUserCmd(const std::string& username,
                                           const std::string& password);

/** Discover attached tape changers and tape drives. */
TapeStorageInventory DiscoverTapeStorageInventory();

/** Format a device identifier descriptor for display. */
std::string DescribeDeviceIdentifier(const DeviceIdentifier& identifier);

/** Parse the addressed logical-unit device identifiers from VPD page 0x83. */
std::vector<DeviceIdentifier> ParseDeviceIdentifiersVpdPage(
    const std::vector<uint8_t>& page_data);

/** Parse the changer's data transfer element identifiers from READ ELEMENT
 * STATUS. */
std::vector<TapeChangerDriveIdentifier> ParseTapeLibraryDriveIdentifiers(
    const std::vector<uint8_t>& status_data);

/** Suggest default tape assignments for the discovered inventory. */
std::vector<TapeAssignment> SuggestTapeAssignments(
    const std::vector<TapeChangerInfo>& changers,
    const std::vector<TapeDriveInfo>& drives);

/** Parse mtx status output for a changer inventory listing. */
TapeChangerInventory ParseTapeChangerInventoryStatus(
    const std::string& status_text);

/** Return whether a tape drive status indicates a loaded medium. */
bool TapeDriveHasLoadedMedium(const CapturedCommandOutput& output);

/** Validate the selected disk and tape storage configuration. */
bool ValidateStorageConfig(const DiskStorageConfig& disk,
                           const TapeStorageConfig& tape,
                           std::string& error);

/** Read director defaults from the current File storage configuration. */
DirectorStorageDefaults ReadDirectorStorageDefaults();

/** Build the shell script that applies the selected storage configuration. */
std::string BuildConfigureStorageScript(const DiskStorageConfig& disk,
                                        const TapeStorageConfig& tape,
                                        const DirectorStorageDefaults& defaults
                                        = {});

/** Execute a generated shell script via a temporary file. */
int RunGeneratedScript(const std::string& script,
                       bool use_sudo,
                       OutputCallback cb);

#endif  // BAREOS_BAREOS_SETUP_SETUP_STEPS_H_
