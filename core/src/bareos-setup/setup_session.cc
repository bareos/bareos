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
#include "setup_session.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <unistd.h>

// Shared WebSocket codec used by bareos-setup and bareos-webui-proxy.
#include "ws_codec.h"

#include <jansson.h>

#include "command_runner.h"
#include "os_detector.h"
#include "setup_steps.h"

namespace {

struct CapturedCommandOutput {
  int exit_code = 0;
  std::string stdout_text;
  std::string stderr_text;
};

CapturedCommandOutput RunCommandCapture(const std::vector<std::string>& argv,
                                       bool use_sudo = false)
{
  CapturedCommandOutput output;
  output.exit_code = RunCommand(
      argv, use_sudo,
      [&output](const std::string& line, const std::string& stream) {
        std::string& target
            = (stream == "stderr") ? output.stderr_text : output.stdout_text;
        if (!target.empty()) target += '\n';
        target += line;
      });
  return output;
}

std::string Trim(std::string value)
{
  auto is_space = [](unsigned char ch) { return std::isspace(ch) != 0; };
  while (!value.empty() && is_space(value.front())) value.erase(0, 1);
  while (!value.empty() && is_space(value.back())) value.pop_back();
  return value;
}

[[maybe_unused]] std::string ReadUdevProperty(const std::string& output,
                                              const std::string& key)
{
  std::istringstream stream(output);
  std::string line;
  const std::string prefix = key + "=";
  while (std::getline(stream, line)) {
    if (line.rfind(prefix, 0) == 0) return line.substr(prefix.size());
  }
  return "";
}

std::string StableTapePath(const std::filesystem::path& path)
{
  try {
    return std::filesystem::canonical(path).string();
  } catch (...) {
    return path.string();
  }
}

[[maybe_unused]] bool IsTapeByIdPath(const std::string& path)
{
  return path.rfind("/dev/tape/by-id/", 0) == 0;
}

[[maybe_unused]] std::string SelectShortestTapeByIdPath(
    const std::vector<std::string>& devlinks,
    const std::string& fallback)
{
  std::string selected;
  for (const auto& link : devlinks) {
    if (!IsTapeByIdPath(link)) continue;
    if (selected.empty() || link.size() < selected.size()
        || (link.size() == selected.size() && link < selected)) {
      selected = link;
    }
  }
  return selected.empty() ? fallback : selected;
}

[[maybe_unused]] std::string ExtractSerialFromDevlinks(
    const std::vector<std::string>& devlinks)
{
  std::string serial;
  const std::regex by_id_re(
      R"(/dev/tape/by-id/scsi-([^-\/]+)(?:-(?:nst|st|changer))?$)");
  std::smatch match;
  for (const auto& link : devlinks) {
    if (!std::regex_match(link, match, by_id_re) || match.size() != 2) continue;
    const std::string candidate = match[1].str();
    if (candidate.empty()) continue;
    if (serial.empty() || candidate.size() < serial.size()
        || (candidate.size() == serial.size() && candidate < serial)) {
      serial = candidate;
    }
  }
  return serial;
}

struct TapeDeviceInfo {
  std::string path;
  std::string canonical_path;
  std::string display_name;
  std::string identifier;
  std::string serial_number;
  std::string vendor;
  std::string model;
  std::string firmware_version;
  std::vector<std::string> aliases;
};

struct TapeChangerStatus {
  int drives = 0;
  int robot_arms = 0;
  int slots = 0;
  int ie_slots = 0;
  std::string error;
};

struct TapeChangerInfo {
  TapeDeviceInfo device;
  TapeChangerStatus status;
};

struct TapeAssignmentRequest {
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

struct DiskStorageRequest {
  bool enabled = false;
  std::string storage_path;
  bool dedupable = false;
  int concurrent_jobs = 10;
};

struct TapeStorageRequest {
  bool enabled = false;
  std::vector<TapeAssignmentRequest> assignments;
};

struct DirectorStorageDefaults {
  std::string address = "localhost";
  std::string password;
  std::string port = "9103";
};

[[maybe_unused]] TapeDeviceInfo ReadTapeDeviceInfo(
    const std::filesystem::directory_entry& entry)
{
  const auto name = entry.path().filename().string();
  const std::string path = entry.path().string();
  const std::string canonical_path = StableTapePath(entry.path());
  auto udev = RunCommandCapture(
      {"udevadm", "info", "--query=property", "--name", canonical_path}, false);

  std::vector<std::string> devlinks;
  std::istringstream stream(ReadUdevProperty(udev.stdout_text, "DEVLINKS"));
  std::string link;
  while (stream >> link) devlinks.push_back(link);
  std::sort(devlinks.begin(), devlinks.end());
  devlinks.erase(std::unique(devlinks.begin(), devlinks.end()), devlinks.end());

  const std::string identifier = [&]() {
    std::string value = ReadUdevProperty(udev.stdout_text, "ID_SERIAL_SHORT");
    if (value.empty()) value = ReadUdevProperty(udev.stdout_text, "ID_SERIAL");
    return value;
  }();
  const std::string serial_number = [&]() {
    std::string value = ReadUdevProperty(udev.stdout_text, "ID_SCSI_SERIAL");
    if (value.empty())
      value = ReadUdevProperty(udev.stdout_text, "SCSI_IDENT_SERIAL");
    if (value.empty()) value = ExtractSerialFromDevlinks(devlinks);
    return value;
  }();
  const std::string preferred_path = SelectShortestTapeByIdPath(devlinks, path);

  TapeDeviceInfo info;
  info.path = preferred_path;
  info.canonical_path = canonical_path;
  info.identifier = identifier;
  info.serial_number = serial_number;
  info.vendor = [&]() {
    std::string value = ReadUdevProperty(udev.stdout_text, "ID_VENDOR");
    if (value.empty())
      value = ReadUdevProperty(udev.stdout_text, "ID_VENDOR_FROM_DATABASE");
    return value;
  }();
  info.model = [&]() {
    std::string value = ReadUdevProperty(udev.stdout_text, "ID_MODEL");
    if (value.empty()) value = ReadUdevProperty(udev.stdout_text, "ID_MODEL_ENC");
    return value;
  }();
  info.firmware_version = [&]() {
    std::string value = ReadUdevProperty(udev.stdout_text, "ID_REVISION");
    if (value.empty())
      value = ReadUdevProperty(udev.stdout_text, "SCSI_IDENT_REVISION");
    return value;
  }();
  info.display_name = !identifier.empty() ? identifier
                                          : (!serial_number.empty() ? serial_number
                                                                    : name);
  for (const auto& devlink : devlinks) {
    if (devlink != info.path) info.aliases.push_back(devlink);
  }
  if (info.canonical_path != info.path
      && std::find(info.aliases.begin(), info.aliases.end(), info.canonical_path)
             == info.aliases.end()) {
    info.aliases.insert(info.aliases.begin(), info.canonical_path);
  }
  return info;
}

[[maybe_unused]] TapeChangerStatus QueryTapeChangerStatus(
    const std::string& changer_path)
{
  TapeChangerStatus status;
  auto result
      = RunCommandCapture({"mtx", "-f", StableTapePath(changer_path), "status"},
                          false);
  if (result.exit_code != 0) {
    status.error = !result.stderr_text.empty() ? result.stderr_text
                                               : result.stdout_text;
    return status;
  }

  std::smatch match;
  std::regex summary(
      R"(.*:([0-9]+)\s+Drives,\s+([0-9]+)\s+Slots\s+\(\s*([0-9]+)\s+Import\/Export\s*\).*)");
  std::istringstream stream(result.stdout_text);
  std::string line;
  while (std::getline(stream, line)) {
    if (std::regex_match(line, match, summary) && match.size() == 4) {
      status.drives = std::stoi(match[1].str());
      status.slots = std::stoi(match[2].str());
      status.ie_slots = std::stoi(match[3].str());
    }
    if (line.rfind("Medium Transport Element", 0) == 0) ++status.robot_arms;
  }
  return status;
}

[[maybe_unused]] TapeChangerInventory ParseTapeChangerInventoryStatus(
    const std::string& status_text)
{
  TapeChangerInventory inventory;

  const std::regex drive_re(
      R"(^Data Transfer Element\s+([0-9]+):(Full|Empty)(?:\s+\(Storage Element\s+([0-9]+)\s+Loaded\))?(?::VolumeTag\s*=\s*(.+))?$)");
  const std::regex slot_re(
      R"(^\s*Storage Element\s+([0-9]+):(Full|Empty)(?:\s*:VolumeTag\s*=\s*(.+))?$)");

  std::istringstream stream(status_text);
  std::string line;
  std::smatch match;
  while (std::getline(stream, line)) {
    if (std::regex_match(line, match, drive_re) && match.size() == 5) {
      TapeInventoryDriveEntry drive;
      drive.number = std::stoi(match[1].str());
      drive.loaded = match[2].str() == "Full";
      drive.source_slot = match[3].str();
      drive.volume_tag = Trim(match[4].str());
      inventory.drives.push_back(std::move(drive));
      continue;
    }

    if (std::regex_match(line, match, slot_re) && match.size() == 4) {
      TapeInventorySlotEntry slot;
      slot.number = std::stoi(match[1].str());
      slot.full = match[2].str() == "Full";
      slot.volume_tag = Trim(match[3].str());
      inventory.slots.push_back(std::move(slot));
    }
  }

  std::sort(inventory.drives.begin(), inventory.drives.end(),
            [](const auto& lhs, const auto& rhs) { return lhs.number < rhs.number; });
  std::sort(inventory.slots.begin(), inventory.slots.end(),
            [](const auto& lhs, const auto& rhs) { return lhs.number < rhs.number; });
  return inventory;
}

bool ContainsInsensitive(std::string haystack, const std::string& needle)
{
  std::transform(haystack.begin(), haystack.end(), haystack.begin(),
                 [](unsigned char ch) { return std::tolower(ch); });
  std::string lowered_needle = needle;
  std::transform(lowered_needle.begin(), lowered_needle.end(), lowered_needle.begin(),
                 [](unsigned char ch) { return std::tolower(ch); });
  return haystack.find(lowered_needle) != std::string::npos;
}

bool TapeDriveHasLoadedMedium(const CapturedCommandOutput& output)
{
  std::string combined = output.stdout_text;
  if (!combined.empty() && !output.stderr_text.empty()) combined += '\n';
  combined += output.stderr_text;

  if (ContainsInsensitive(combined, "no medium")
      || ContainsInsensitive(combined, "medium not present")
      || ContainsInsensitive(combined, "not ready")
      || ContainsInsensitive(combined, "dr_open")
      || ContainsInsensitive(combined, "door open")
      || ContainsInsensitive(combined, "offline")) {
    return false;
  }

  return ContainsInsensitive(combined, "online")
         || ContainsInsensitive(combined, "im_rep_en")
         || ContainsInsensitive(combined, "file number=");
}

std::vector<TapeAssignmentRequest> ParseTapeAssignments(json_t* msg)
{
  std::vector<TapeAssignmentRequest> assignments;
  json_t* array = json_object_get(msg, "tape_assignments");
  if (!array || !json_is_array(array)) return assignments;

  size_t idx = 0;
  json_t* item = nullptr;
  json_array_foreach(array, idx, item)
  {
    if (!json_is_object(item)) continue;
    TapeAssignmentRequest assignment;
    if (json_t* changer = json_object_get(item, "changer_path");
        changer && json_is_string(changer)) {
      assignment.changer_path = json_string_value(changer);
    }
    json_t* drive_array = json_object_get(item, "drive_paths");
    if (drive_array && json_is_array(drive_array)) {
      size_t drive_idx = 0;
      json_t* drive = nullptr;
      json_array_foreach(drive_array, drive_idx, drive)
      {
        if (drive && json_is_string(drive)) {
          assignment.drive_paths.emplace_back(json_string_value(drive));
        }
      }
    }
    if (!assignment.changer_path.empty() && !assignment.drive_paths.empty()) {
      assignments.push_back(std::move(assignment));
    }
  }

  return assignments;
}

int JsonInt(json_t* obj, const char* key, int fallback = 0)
{
  json_t* value = json_object_get(obj, key);
  if (!value) return fallback;
  if (json_is_integer(value)) return static_cast<int>(json_integer_value(value));
  if (json_is_string(value)) {
    try {
      return std::stoi(json_string_value(value));
    } catch (...) {
      return fallback;
    }
  }
  return fallback;
}

std::string StripInlineComment(std::string value)
{
  const auto hash = value.find('#');
  if (hash != std::string::npos) value.erase(hash);
  return Trim(value);
}

std::string Unquote(std::string value)
{
  value = Trim(value);
  if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
    return value.substr(1, value.size() - 2);
  }
  return value;
}

[[maybe_unused]] DirectorStorageDefaults ReadDirectorStorageDefaults()
{
  DirectorStorageDefaults defaults;
  std::ifstream input("/etc/bareos/bareos-dir.d/storage/File.conf");
  if (!input) return defaults;

  std::string line;
  while (std::getline(input, line)) {
    line = StripInlineComment(line);
    if (line.empty()) continue;
    const auto equal = line.find('=');
    if (equal == std::string::npos) continue;
    const std::string key = Trim(line.substr(0, equal));
    const std::string value = Unquote(line.substr(equal + 1));
    if (key == "Address" && !value.empty()) {
      defaults.address = value;
    } else if (key == "Password") {
      defaults.password = value;
    } else if (key == "Port" && !value.empty()) {
      defaults.port = value;
    }
  }

  return defaults;
}

std::string ShellQuote(const std::string& value)
{
  std::string quoted = "'";
  for (char ch : value) {
    if (ch == '\'') {
      quoted += "'\"'\"'";
    } else {
      quoted += ch;
    }
  }
  quoted += "'";
  return quoted;
}

std::string BuildDiskStorageSdConfig(const DiskStorageRequest& disk)
{
  std::ostringstream conf;
  if (disk.dedupable) {
    conf << "Device {\n"
         << "  Name = DedupStorage\n"
         << "  Media Type = Dedup\n"
         << "  Device Type = dedupable\n"
         << "  Device Options = \"Block Size = 16k\"\n"
         << "  Archive Device = " << disk.storage_path << "\n"
         << "  LabelMedia = yes\n"
         << "  Random Access = yes\n"
         << "  AutomaticMount = yes\n"
         << "  RemovableMedia = no\n"
         << "  AlwaysOpen = no\n"
         << "}\n";
  } else {
    conf << "Device {\n"
         << "  Name = FileStorage\n"
         << "  Media Type = File\n"
         << "  Archive Device = " << disk.storage_path << "\n"
         << "  Label Media = yes\n"
         << "  Random Access = yes\n"
         << "  Automatic Mount = yes\n"
         << "  Removable Media = no\n"
         << "  Always Open = no\n"
         << "  Description = \"File device configured by bareos-setup.\"\n"
         << "  Count = " << disk.concurrent_jobs << "\n"
         << "}\n";
  }
  return conf.str();
}

std::string BuildDiskStorageDirConfig(const DiskStorageRequest& disk,
                                      const DirectorStorageDefaults& defaults)
{
  std::ostringstream conf;
  conf << "Storage {\n"
       << "  Name = " << (disk.dedupable ? "Dedup" : "File") << "\n"
       << "  Address = " << defaults.address << "\n"
       << "  Password = \"" << defaults.password << "\"\n"
       << "  Device = " << (disk.dedupable ? "DedupStorage" : "FileStorage")
       << "\n"
       << "  Media Type = " << (disk.dedupable ? "Dedup" : "File") << "\n"
       << "  Port = " << defaults.port << "\n"
       << "  Maximum Concurrent Jobs = " << disk.concurrent_jobs << "\n"
       << "}\n";
  return conf.str();
}

std::string BuildTapeDriveName(size_t changer_index, size_t drive_index)
{
  return "tapedrive-" + std::to_string(changer_index) + "-" + std::to_string(drive_index);
}

std::string BuildTapeDevicesSdConfig(const TapeStorageRequest& tape)
{
  std::ostringstream conf;
  for (size_t changer_index = 0; changer_index < tape.assignments.size();
       ++changer_index) {
    const auto& assignment = tape.assignments[changer_index];
    for (size_t drive_index = 0; drive_index < assignment.drive_paths.size();
         ++drive_index) {
      conf << "Device {\n"
           << "  Name = \"" << BuildTapeDriveName(changer_index, drive_index)
           << "\"\n"
           << "  DeviceType = tape\n\n"
           << "  DriveIndex = " << drive_index << "\n\n"
           << "  ArchiveDevice = " << assignment.drive_paths[drive_index] << "\n\n"
           << "  MediaType = LTO\n"
           << "  Drive Crypto Enabled = Yes\n"
           << "  Query Crypto Status = Yes\n"
           << "  AutoChanger = yes\n"
           << "  AutomaticMount = yes\n"
           << "  MaximumFileSize = 10GB\n"
           << "  AlwaysOpen = Yes\n"
           << "  Maximum Concurrent Jobs = 1\n"
           << "}\n\n";
    }
  }
  return conf.str();
}

std::string BuildAutochangerSdConfig(const TapeStorageRequest& tape)
{
  std::ostringstream conf;
  for (size_t changer_index = 0; changer_index < tape.assignments.size();
       ++changer_index) {
    const auto& assignment = tape.assignments[changer_index];
    conf << "Autochanger {\n"
         << "  Name = \"autochanger-" << changer_index << "\"\n"
         << "  Changer Device = " << assignment.changer_path << "\n\n"
         << "  Changer Command = \"/usr/lib/bareos/scripts/mtx-changer %c %o %S %a %d\"\n";
    for (size_t drive_index = 0; drive_index < assignment.drive_paths.size();
         ++drive_index) {
      conf << "  Device=" << BuildTapeDriveName(changer_index, drive_index) << "\n";
    }
    conf << "\n}\n\n";
  }
  return conf.str();
}

std::string BuildTapeStorageDirConfig(const TapeStorageRequest& tape,
                                      const DirectorStorageDefaults& defaults)
{
  std::ostringstream conf;
  for (size_t changer_index = 0; changer_index < tape.assignments.size();
       ++changer_index) {
    const auto& assignment = tape.assignments[changer_index];
    conf << "Storage {\n"
         << "  Name = Tape-" << changer_index << "\n"
         << "  Address = " << defaults.address << "\n"
         << "  Password = \"" << defaults.password << "\"\n"
         << "  Device = autochanger-" << changer_index << "\n"
         << "  Port = " << defaults.port << "\n"
         << "  Media Type = LTO\n"
         << "  Auto Changer = yes\n"
         << "  Maximum Concurrent Jobs = " << assignment.drive_paths.size()
         << "\n"
         << "}\n\n";
  }
  return conf.str();
}

[[maybe_unused]] bool ValidateStorageConfig(const DiskStorageRequest& disk,
                                            const TapeStorageRequest& tape,
                                            std::string& error)
{
  if (!disk.enabled && !tape.enabled) {
    error = "Select at least one storage target.";
    return false;
  }

  if (disk.enabled) {
    if (Trim(disk.storage_path).empty()) {
      error = "Select a disk storage path.";
      return false;
    }
    if (disk.concurrent_jobs < 1) {
      error = "Disk storage requires at least one concurrent job.";
      return false;
    }
  }

  if (tape.enabled) {
    if (tape.assignments.empty()) {
      error = "Assign at least one tape drive to a tape changer.";
      return false;
    }

    for (const auto& assignment : tape.assignments) {
      if (assignment.changer_path.empty() || assignment.drive_paths.empty()) {
        error = "Each tape changer needs at least one assigned drive.";
        return false;
      }
    }
  }

  return true;
}

[[maybe_unused]] std::string BuildConfigureStorageScript(
    const DiskStorageRequest& disk,
    const TapeStorageRequest& tape,
    const DirectorStorageDefaults& defaults)
{
  std::ostringstream script;
  script << "#!/bin/bash\n"
         << "set -euo pipefail\n\n"
         << "echo \"Preparing Bareos storage configuration directories...\"\n"
         << "install -d /etc/bareos/bareos-sd.d/device "
            "/etc/bareos/bareos-sd.d/autochanger /etc/bareos/bareos-dir.d/storage\n\n";

  if (disk.enabled) {
    script << "echo \"Ensuring disk storage path exists: " << disk.storage_path
           << "\"\n"
           << "install -d " << ShellQuote(disk.storage_path) << "\n"
           << "cat <<'EOF' > /etc/bareos/bareos-sd.d/device/FileStorage.conf\n"
           << BuildDiskStorageSdConfig(disk)
           << "EOF\n"
           << "cat <<'EOF' > /etc/bareos/bareos-dir.d/storage/File.conf\n"
           << BuildDiskStorageDirConfig(disk, defaults)
           << "EOF\n\n";
  } else {
    script << "echo \"Removing disk storage configuration...\"\n"
           << "rm -f /etc/bareos/bareos-sd.d/device/FileStorage.conf "
              "/etc/bareos/bareos-dir.d/storage/File.conf\n\n";
  }

  if (tape.enabled) {
    script << "echo \"Writing tape drive configuration...\"\n"
           << "cat <<'EOF' > /etc/bareos/bareos-sd.d/device/tape_devices.conf\n"
           << BuildTapeDevicesSdConfig(tape)
           << "EOF\n"
           << "cat <<'EOF' > /etc/bareos/bareos-sd.d/autochanger/autochanger.conf\n"
           << BuildAutochangerSdConfig(tape)
           << "EOF\n"
           << "cat <<'EOF' > /etc/bareos/bareos-dir.d/storage/Tape.conf\n"
           << BuildTapeStorageDirConfig(tape, defaults)
           << "EOF\n\n";
  } else {
    script << "echo \"Removing tape storage configuration...\"\n"
           << "rm -f /etc/bareos/bareos-sd.d/device/tape_devices.conf "
              "/etc/bareos/bareos-sd.d/autochanger/autochanger.conf "
              "/etc/bareos/bareos-dir.d/storage/Tape.conf\n\n";
  }

  script << "if command -v bareos-sd >/dev/null 2>&1; then\n"
         << "  echo \"Checking bareos-sd configuration...\"\n"
         << "  bareos-sd -t\n"
         << "else\n"
         << "  echo \"Skipping bareos-sd config check: bareos-sd not found.\"\n"
         << "fi\n\n"
         << "if command -v bareos-dir >/dev/null 2>&1; then\n"
         << "  echo \"Checking bareos-dir configuration...\"\n"
         << "  bareos-dir -t\n"
         << "else\n"
         << "  echo \"Skipping bareos-dir config check: bareos-dir not found.\"\n"
         << "fi\n\n"
         << "if command -v systemctl >/dev/null 2>&1 "
            "&& systemctl list-unit-files bareos-sd.service >/dev/null 2>&1; then\n"
         << "  echo \"Restarting bareos-sd...\"\n"
         << "  systemctl restart bareos-sd\n"
         << "else\n"
         << "  echo \"Skipping bareos-sd restart: systemd unit not found.\"\n"
         << "fi\n\n"
         << "if command -v systemctl >/dev/null 2>&1 "
            "&& systemctl list-unit-files bareos-dir.service >/dev/null 2>&1; then\n"
         << "  echo \"Restarting bareos-dir...\"\n"
         << "  systemctl restart bareos-dir\n"
         << "else\n"
         << "  echo \"Skipping bareos-dir restart: systemd unit not found.\"\n"
         << "fi\n";

  return script.str();
}

[[maybe_unused]] int RunGeneratedScript(
    WsCodec& ws,
    [[maybe_unused]] const std::string& step_id,
    const std::string& script,
    bool use_sudo)
{
  char path_template[] = "/tmp/bareos-setup-storage-XXXXXX";
  const int fd = mkstemp(path_template);
  if (fd < 0) throw std::runtime_error("Failed to create temporary script file.");

  const std::string script_path(path_template);
  close(fd);

  try {
    std::ofstream output(script_path);
    if (!output) {
      throw std::runtime_error("Failed to open temporary script file for writing.");
    }
    output << script;
    output.close();
    if (!output) {
      throw std::runtime_error("Failed to write temporary script file.");
    }

    const int exit_code = RunCommand(
        {"bash", script_path}, use_sudo,
        [&ws](const std::string& line, const std::string& stream) {
          json_t* obj = json_pack("{s:s, s:s, s:s}", "type", "output", "line",
                                  line.c_str(), "stream", stream.c_str());
          char* serialized = json_dumps(obj, JSON_COMPACT);
          json_decref(obj);
          ws.SendText(serialized ? serialized : "{}");
          free(serialized);
        });
    std::filesystem::remove(script_path);
    return exit_code;
  } catch (...) {
    std::filesystem::remove(script_path);
    throw;
  }
}

json_t* DiscoverDiskInventoryJson()
{
  json_t* disks = json_array();
  auto output = RunCommandCapture(
      {"lsblk", "-J", "-o",
       "NAME,PATH,TYPE,SIZE,FSTYPE,MOUNTPOINTS,MODEL,VENDOR,TRAN"},
      false);
  if (output.exit_code != 0) return disks;

  json_error_t err{};
  json_t* root = json_loads(output.stdout_text.c_str(), 0, &err);
  if (!root) return disks;

  auto append_devices = [&](auto&& self, json_t* devices) -> void {
    if (!devices || !json_is_array(devices)) return;
    size_t i = 0;
    json_t* value = nullptr;
    json_array_foreach(devices, i, value)
    {
      if (!json_is_object(value)) continue;
      auto json_string = [&](const char* key) {
        json_t* field = json_object_get(value, key);
        return (field && json_is_string(field)) ? std::string(json_string_value(field))
                                                : std::string{};
      };
      std::string mountpoints;
      if (json_t* mounts = json_object_get(value, "mountpoints");
          mounts && json_is_array(mounts)) {
        size_t mount_idx = 0;
        json_t* mount = nullptr;
        json_array_foreach(mounts, mount_idx, mount)
        {
          if (!mount || !json_is_string(mount)) continue;
          const std::string current = json_string_value(mount);
          if (current.empty()) continue;
          if (!mountpoints.empty()) mountpoints += ", ";
          mountpoints += current;
        }
      }

      const std::string path = json_string("path");
      if (!path.empty()) {
        json_array_append_new(
            disks,
            json_pack("{s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s}",
                      "name", json_string("name").c_str(), "path", path.c_str(),
                      "type", json_string("type").c_str(), "size",
                      json_string("size").c_str(), "filesystem",
                      json_string("fstype").c_str(), "mountpoints",
                      mountpoints.c_str(), "model",
                      Trim(json_string("model")).c_str(), "vendor",
                      Trim(json_string("vendor")).c_str(), "transport",
                      json_string("tran").c_str()));
      }

      self(self, json_object_get(value, "children"));
    }
  };

  append_devices(append_devices, json_object_get(root, "blockdevices"));
  json_decref(root);
  return disks;
}

json_t* BuildStorageInventoryJson()
{
  const auto inventory = ::DiscoverTapeStorageInventory();

  json_t* changers_json = json_array();
  for (const auto& changer : inventory.changers) {
    json_t* aliases = json_array();
    for (const auto& alias : changer.aliases) {
      json_array_append_new(aliases, json_string(alias.c_str()));
    }
    json_t* status = json_pack(
        "{s:i, s:i, s:i, s:i, s:s}", "drives", changer.status.drives, "robot_arms",
        changer.status.robot_arms, "slots", changer.status.slots, "ie_slots",
        changer.status.ie_slots, "error", changer.status.error.c_str());
    json_t* drive_identifiers = json_array();
    for (const auto& drive : changer.drive_identifiers) {
      json_t* identifiers = json_array();
      for (const auto& identifier : drive.device_identifiers) {
        json_array_append_new(
            identifiers,
            json_string(::DescribeDeviceIdentifier(identifier).c_str()));
      }
      json_array_append_new(
          drive_identifiers,
          json_pack("{s:i, s:o}", "element_address", drive.element_address,
                    "identifiers", identifiers));
    }
    json_array_append_new(
        changers_json,
        json_pack(
            "{s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:o, s:o, s:o}",
            "path", changer.path.c_str(), "canonical_path",
            changer.canonical_path.c_str(), "display_name", changer.display_name.c_str(),
            "identifier", changer.identifier.c_str(), "serial_number",
            changer.serial_number.c_str(), "vendor", changer.vendor.c_str(), "model",
            changer.model.c_str(), "firmware_version",
            changer.firmware_version.c_str(), "aliases", aliases, "status", status,
            "drive_identifiers", drive_identifiers));
  }

  json_t* drives_json = json_array();
  for (const auto& drive : inventory.drives) {
    json_t* aliases = json_array();
    for (const auto& alias : drive.aliases) {
      json_array_append_new(aliases, json_string(alias.c_str()));
    }
    json_array_append_new(
        drives_json,
        json_pack(
            "{s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:s, s:o}", "path",
            drive.path.c_str(), "canonical_path", drive.canonical_path.c_str(),
            "display_name", drive.display_name.c_str(), "identifier",
            drive.identifier.c_str(), "serial_number",
            drive.serial_number.c_str(), "vendor", drive.vendor.c_str(),
            "model", drive.model.c_str(), "firmware_version",
             drive.firmware_version.c_str(), "aliases", aliases));
  }

  json_t* assignments = json_array();
  for (const auto& assignment : inventory.suggested_assignments) {
    json_t* drive_paths = json_array();
    for (const auto& drive_path : assignment.drive_paths) {
      json_array_append_new(drive_paths, json_string(drive_path.c_str()));
    }
    json_array_append_new(
        assignments,
        json_pack("{s:s, s:o}", "changer_path",
                  assignment.changer_path.c_str(), "drive_paths", drive_paths));
  }

  return json_pack("{s:s, s:o, s:o, s:o, s:o}", "type", "storage_inventory",
                   "disks", DiscoverDiskInventoryJson(), "tape_changers",
                   changers_json, "tape_drives", drives_json,
                   "suggested_tape_assignments", assignments);
}

}  // namespace

/** Serialize a json_t* to a std::string and decref it. */
static std::string Dump(json_t* obj)
{
  char* s = json_dumps(obj, JSON_COMPACT);
  json_decref(obj);
  std::string result(s ? s : "{}");
  free(s);
  return result;
}

/** Send a JSON text frame. */
static void SendJson(WsCodec& ws, json_t* obj) { ws.SendText(Dump(obj)); }

static void SendOutputLine(WsCodec& ws, const std::string& line,
                           const std::string& stream = "stdout")
{
  json_t* obj = json_pack("{s:s, s:s, s:s}", "type", "output", "line",
                          line.c_str(), "stream", stream.c_str());
  ws.SendText(Dump(obj));
}

/** Send {"type":"error","message":"..."} */
static void SendError(WsCodec& ws,
                      const std::string& step,
                      const std::string& msg)
{
  json_t* obj = json_pack("{s:s, s:s, s:s}", "type", "error", "step",
                          step.c_str(), "message", msg.c_str());
  SendJson(ws, obj);
}

/** Get a string field from a JSON object (or "" if missing). */
static std::string JStr(json_t* obj, const char* key)
{
  json_t* v = json_object_get(obj, key);
  return (v && json_is_string(v)) ? json_string_value(v) : "";
}

/** Get a boolean field from a JSON object (or false if missing). */
static bool JBool(json_t* obj, const char* key)
{
  json_t* v = json_object_get(obj, key);
  return v && json_is_true(v);
}

// ---------------------------------------------------------------------------
// Action handlers
// ---------------------------------------------------------------------------

static void HandleDetectOs(WsCodec& ws)
{
  try {
    OsInfo info = DetectOs();
    json_t* obj = json_pack(
        "{s:s, s:s, s:s, s:s, s:s, s:s, s:s}", "type", "os_info", "distro",
        info.distro.c_str(), "version", info.version.c_str(), "codename",
        info.codename.c_str(), "pretty_name", info.pretty_name.c_str(), "arch",
        info.arch.c_str(), "pkg_mgr", info.pkg_mgr.c_str());
    SendJson(ws, obj);
  } catch (const std::exception& e) {
    SendError(ws, "detect_os", e.what());
  }
}

static void HandleScanStorage(WsCodec& ws)
{
  try {
    SendJson(ws, BuildStorageInventoryJson());
  } catch (const std::exception& e) {
    SendError(ws, "scan_storage", e.what());
  }
}

static void HandleTestTapeAssignment(WsCodec& ws, json_t* msg)
{
  const bool use_sudo = JBool(msg, "sudo");
  const auto assignments = ParseTapeAssignments(msg);
  if (assignments.empty()) {
    SendError(ws, "test_tape_assignment",
              "Assign at least one tape drive to a tape changer first.");
    return;
  }

  auto report_done = [&](int exit_code) {
    SendJson(ws, json_pack("{s:s, s:s, s:i}", "type", "done", "step",
                           "test_tape_assignment", "exit_code", exit_code));
  };

  try {
    SendOutputLine(ws, "Checking that all assigned drives are empty...");
    for (const auto& assignment : assignments) {
      for (const auto& drive_path : assignment.drive_paths) {
        auto status
            = RunCommandCapture({"mt", "-f", StableTapePath(drive_path), "status"},
                                use_sudo);
        if (TapeDriveHasLoadedMedium(status)) {
          SendError(ws, "test_tape_assignment",
                    "Drive " + drive_path
                        + " already has a tape loaded. Empty all assigned drives first.");
          return;
        }
      }
    }

    for (const auto& assignment : assignments) {
      auto changer_status = RunCommandCapture(
          {"mtx", "-f", StableTapePath(assignment.changer_path), "status"}, use_sudo);
      if (changer_status.exit_code != 0) {
        SendError(ws, "test_tape_assignment",
                  "Failed to query changer status for " + assignment.changer_path
                      + ": "
                      + ::Trim(!changer_status.stderr_text.empty()
                                   ? changer_status.stderr_text
                                   : changer_status.stdout_text));
        return;
      }

      const auto inventory
          = ::ParseTapeChangerInventoryStatus(changer_status.stdout_text);
      auto full_slot_it
          = std::find_if(inventory.slots.begin(), inventory.slots.end(),
                         [](const auto& slot) { return slot.full; });
      if (full_slot_it == inventory.slots.end()) {
        SendError(ws, "test_tape_assignment",
                  "No tape is available in changer " + assignment.changer_path
                      + " for assignment testing.");
        return;
      }

      for (size_t drive_index = 0; drive_index < assignment.drive_paths.size();
           ++drive_index) {
        const auto& expected_drive = assignment.drive_paths[drive_index];
        const int changer_drive_number = static_cast<int>(drive_index);

        SendOutputLine(
            ws, "Loading a tape from slot " + std::to_string(full_slot_it->number)
                    + " into drive " + std::to_string(changer_drive_number) + "...");
        auto load_result = RunCommandCapture(
            std::vector<std::string>{"mtx", "-f",
                                     StableTapePath(assignment.changer_path),
                                     "load",
                                     std::to_string(full_slot_it->number),
                                     std::to_string(changer_drive_number)},
            use_sudo);
        if (load_result.exit_code != 0) {
          SendError(ws, "test_tape_assignment",
                    "Failed to load a tape into drive "
                        + std::to_string(changer_drive_number) + " of changer "
                        + assignment.changer_path + ": "
                        + ::Trim(!load_result.stderr_text.empty()
                                     ? load_result.stderr_text
                                     : load_result.stdout_text));
          return;
        }

        SendOutputLine(ws, "Checking which drive now reports a loaded tape...");
        std::vector<std::string> loaded_drives;
        for (const auto& drive_path : assignment.drive_paths) {
          auto status
              = RunCommandCapture({"mt", "-f", StableTapePath(drive_path), "status"},
                                  use_sudo);
          if (TapeDriveHasLoadedMedium(status)) loaded_drives.push_back(drive_path);
        }

        if (loaded_drives.size() != 1 || loaded_drives.front() != expected_drive) {
          RunCommandCapture(
              std::vector<std::string>{"mtx", "-f",
                                       StableTapePath(assignment.changer_path),
                                       "unload",
                                       std::to_string(full_slot_it->number),
                                       std::to_string(changer_drive_number)},
              use_sudo);
          std::ostringstream message;
          message << "Expected tape to load into " << expected_drive;
          if (loaded_drives.empty()) {
            message << ", but no assigned drive reported a loaded tape.";
          } else {
            message << ", but ";
            for (size_t i = 0; i < loaded_drives.size(); ++i) {
              if (i > 0) message << ", ";
              message << loaded_drives[i];
            }
            message << (loaded_drives.size() == 1 ? " reported" : " reported")
                    << " a loaded tape.";
          }
          SendError(ws, "test_tape_assignment", message.str());
          return;
        }

        SendOutputLine(ws, "Unloading the tape again...");
        auto unload_result = RunCommandCapture(
            std::vector<std::string>{"mtx", "-f",
                                     StableTapePath(assignment.changer_path),
                                     "unload",
                                     std::to_string(full_slot_it->number),
                                     std::to_string(changer_drive_number)},
            use_sudo);
        if (unload_result.exit_code != 0) {
          SendError(ws, "test_tape_assignment",
                    "Failed to unload drive "
                        + std::to_string(changer_drive_number) + " of changer "
                        + assignment.changer_path + ": "
                        + ::Trim(!unload_result.stderr_text.empty()
                                     ? unload_result.stderr_text
                                     : unload_result.stdout_text));
          return;
        }

        SendOutputLine(ws, "Checking that the drive is empty again...");
        auto final_status
            = RunCommandCapture({"mt", "-f", StableTapePath(expected_drive), "status"},
                                use_sudo);
        if (TapeDriveHasLoadedMedium(final_status)) {
          SendError(ws, "test_tape_assignment",
                    "Drive " + expected_drive
                        + " still reports a loaded tape after unloading.");
          return;
        }
      }
    }

    report_done(0);
  } catch (const std::exception& e) {
    SendError(ws, "test_tape_assignment", e.what());
  }
}

/** Capitalize the first letter of s (Bareos repo paths use e.g. "Fedora_43").
 */

static void HandleRunStep(WsCodec& ws, json_t* msg, bool dry_run)
{
  std::string step_id = JStr(msg, "id");
  bool use_sudo = JBool(msg, "sudo");

  if (step_id == "configure_storage") {
    ::DiskStorageConfig disk;
    disk.enabled = JBool(msg, "disk_enabled");
    disk.storage_path = ::Trim(JStr(msg, "disk_path"));
    disk.dedupable = JBool(msg, "disk_dedupable");
    disk.concurrent_jobs = JsonInt(msg, "disk_concurrent_jobs", 1);

    ::TapeStorageConfig tape;
    tape.enabled = JBool(msg, "tape_enabled");
    for (const auto& assignment : ParseTapeAssignments(msg)) {
      tape.assignments.push_back(
          {assignment.changer_path, assignment.drive_paths});
    }

    std::string error;
    if (!ValidateStorageConfig(disk, tape, error)) {
      SendError(ws, step_id, error);
      return;
    }

    const auto defaults = ::ReadDirectorStorageDefaults();
    const auto script = BuildConfigureStorageScript(disk, tape, defaults);
    int exit_code = 0;

    if (dry_run) {
      std::istringstream stream(script);
      std::string line;
      while (std::getline(stream, line)) {
        SendOutputLine(ws, "[dry-run] " + line);
      }
    } else {
      try {
        exit_code = RunGeneratedScript(ws, step_id, script, use_sudo);
      } catch (const std::exception& e) {
        SendError(ws, step_id, e.what());
        return;
      }
    }

    SendJson(ws, json_pack("{s:s, s:s, s:i}", "type", "done", "step",
                           step_id.c_str(), "exit_code", exit_code));
    return;
  }

  std::vector<std::string> cmd;

  if (step_id == "add_repo") {
    cmd = BuildAddRepoCmd(JStr(msg, "distro"), JStr(msg, "version"),
                          JStr(msg, "repo_type"), JStr(msg, "repo_login"),
                          JStr(msg, "repo_password"));

  } else if (step_id == "install_packages") {
    std::vector<std::string> pkgs;
    json_t* jarr = json_object_get(msg, "packages");
    if (jarr && json_is_array(jarr)) {
      size_t i;
      json_t* v;
      json_array_foreach(jarr, i, v)
      {
        if (json_is_string(v)) pkgs.push_back(json_string_value(v));
      }
    }
    if (pkgs.empty()) pkgs = BuildDefaultPackageList();
    cmd = BuildInstallCmd(JStr(msg, "pkg_mgr"), pkgs);

  } else if (step_id == "setup_db") {
    cmd = BuildDbCmd();

  } else if (step_id == "create_admin_user") {
    std::string username = JStr(msg, "username");
    std::string password = JStr(msg, "password");
    if (username.empty()) username = "admin";
    cmd = BuildAdminUserCmd(username, password);

  } else {
    SendError(ws, step_id, "Unknown step id: " + step_id);
    return;
  }

  int exit_code = 0;

  if (dry_run) {
    // Build the full command line as it would be executed and print it.
    std::string line = "[dry-run] ";
    if (use_sudo) line += "sudo ";
    for (size_t i = 0; i < cmd.size(); ++i) {
      if (i > 0) line += ' ';
      // Quote arguments that contain spaces
      if (cmd[i].find(' ') != std::string::npos) {
        line += '\'' + cmd[i] + '\'';
      } else {
        line += cmd[i];
      }
    }
    json_t* obj = json_pack("{s:s, s:s, s:s}", "type", "output", "line",
                            line.c_str(), "stream", "stdout");
    ws.SendText(Dump(obj));
  } else {
    try {
      exit_code = RunCommand(
          cmd, use_sudo,
          [&ws](const std::string& line, const std::string& stream) {
            json_t* obj = json_pack("{s:s, s:s, s:s}", "type", "output", "line",
                                    line.c_str(), "stream", stream.c_str());
            ws.SendText(Dump(obj));
          });
    } catch (const std::exception& e) {
      SendError(ws, step_id, e.what());
      return;
    }
  }

  json_t* done = json_pack("{s:s, s:s, s:i}", "type", "done", "step",
                           step_id.c_str(), "exit_code", exit_code);
  SendJson(ws, done);
}

static void HandleGenerateScript(WsCodec& ws)
{
  // Emit a minimal setup script template
  std::ostringstream script;
  script << "#!/bin/bash\n"
         << "# Bareos Setup Script\n"
         << "# Generated by bareos-setup\n"
         << "set -euo pipefail\n\n"
         << "# Add Bareos repository\n"
         << "# (edit repo URL as needed)\n"
         << "# apt-get install -y bareos-director bareos-storage "
            "bareos-filedaemon\n"
         << "# /usr/lib/bareos/scripts/create_bareos_database\n"
         << "# /usr/lib/bareos/scripts/make_bareos_tables\n"
         << "# /usr/lib/bareos/scripts/grant_bareos_privileges\n";

  json_t* obj = json_pack("{s:s, s:s}", "type", "script", "content",
                          script.str().c_str());
  SendJson(ws, obj);
}

// ---------------------------------------------------------------------------
// Session entry point
// ---------------------------------------------------------------------------

void RunSetupSession(int fd, bool dry_run)
{
  WsCodec ws(fd);
  try {
    while (!ws.IsClosed()) {
      std::string text = ws.RecvMessage();
      if (text.empty()) break;

      json_error_t err{};
      json_t* msg = json_loads(text.c_str(), 0, &err);
      if (!msg) {
        std::cerr << "Invalid JSON from browser: " << err.text << "\n";
        continue;
      }

      std::string action = JStr(msg, "action");

      if (action == "detect_os") {
        HandleDetectOs(ws);
      } else if (action == "scan_storage") {
        HandleScanStorage(ws);
      } else if (action == "test_tape_assignment") {
        HandleTestTapeAssignment(ws, msg);
      } else if (action == "run_step") {
        HandleRunStep(ws, msg, dry_run);
      } else if (action == "generate_script") {
        HandleGenerateScript(ws);
      } else {
        SendError(ws, action, "Unknown action: " + action);
      }

      json_decref(msg);
    }
  } catch (const std::exception& e) {
    std::cerr << "Setup session error: " << e.what() << "\n";
  }
}
