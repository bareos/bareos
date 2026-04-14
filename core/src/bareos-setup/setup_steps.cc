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
#include "setup_steps.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <unistd.h>

namespace {

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

std::string ReadUdevProperty(const std::string& output, const std::string& key)
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

bool IsTapeByIdPath(const std::string& path)
{
  return path.rfind("/dev/tape/by-id/", 0) == 0;
}

std::string SelectShortestTapeByIdPath(const std::vector<std::string>& devlinks,
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

std::string ExtractSerialFromDevlinks(const std::vector<std::string>& devlinks)
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

TapeDriveInfo ReadTapeDeviceInfo(const std::filesystem::directory_entry& entry)
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

  TapeDriveInfo info;
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

TapeChangerStatus QueryTapeChangerStatus(const std::string& changer_path)
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

bool ContainsInsensitive(std::string haystack, const std::string& needle)
{
  std::transform(haystack.begin(), haystack.end(), haystack.begin(),
                 [](unsigned char ch) { return std::tolower(ch); });
  std::string lowered_needle = needle;
  std::transform(lowered_needle.begin(), lowered_needle.end(),
                 lowered_needle.begin(),
                 [](unsigned char ch) { return std::tolower(ch); });
  return haystack.find(lowered_needle) != std::string::npos;
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

uint16_t ReadBe16(const std::vector<uint8_t>& data, size_t offset)
{
  return static_cast<uint16_t>((static_cast<uint16_t>(data[offset]) << 8)
                               | data[offset + 1]);
}

uint32_t ReadBe24(const std::vector<uint8_t>& data, size_t offset)
{
  return (static_cast<uint32_t>(data[offset]) << 16)
         | (static_cast<uint32_t>(data[offset + 1]) << 8)
         | data[offset + 2];
}

bool SameDeviceIdentifier(const DeviceIdentifier& lhs,
                          const DeviceIdentifier& rhs)
{
  return lhs == rhs;
}

std::string CollapseWhitespace(std::string value)
{
  std::string collapsed;
  bool previous_space = false;
  for (unsigned char ch : value) {
    if (std::isspace(ch) != 0) {
      if (!previous_space && !collapsed.empty()) collapsed += ' ';
      previous_space = true;
    } else {
      collapsed += static_cast<char>(ch);
      previous_space = false;
    }
  }
  return Trim(collapsed);
}

std::string BytesToHex(const std::vector<uint8_t>& bytes)
{
  static constexpr char kHex[] = "0123456789ABCDEF";
  std::string hex = "0x";
  hex.reserve(2 + bytes.size() * 2);
  for (const auto byte : bytes) {
    hex += kHex[(byte >> 4) & 0x0F];
    hex += kHex[byte & 0x0F];
  }
  return hex;
}

std::optional<std::vector<uint8_t>> ReadBinaryStdoutCommand(
    const std::string& command,
    bool use_sudo)
{
  char path_template[] = "/tmp/bareos-setup-query-XXXXXX";
  const int fd = mkstemp(path_template);
  if (fd < 0) return std::nullopt;

  const std::string output_path(path_template);
  close(fd);

  try {
    const auto result = RunCommandCapture(
        {"bash", "-c", command + " > " + ShellQuote(output_path)}, use_sudo);
    if (result.exit_code != 0) {
      std::remove(output_path.c_str());
      return std::nullopt;
    }

    std::ifstream input(output_path, std::ios::binary);
    if (!input) {
      std::remove(output_path.c_str());
      return std::nullopt;
    }

    std::vector<uint8_t> data((std::istreambuf_iterator<char>(input)),
                              std::istreambuf_iterator<char>());
    std::remove(output_path.c_str());
    return data;
  } catch (...) {
    std::remove(output_path.c_str());
    return std::nullopt;
  }
}

std::optional<std::vector<uint8_t>> QueryDriveIdentifiersRaw(
    const std::string& drive_path)
{
  const std::string command = "sg_inq --page=0x83 --raw "
                              + ShellQuote(StableTapePath(drive_path));
  if (auto data = ReadBinaryStdoutCommand(command, false)) return data;
  return ReadBinaryStdoutCommand(command, true);
}

std::optional<std::vector<uint8_t>> QueryLibraryDriveIdentifiersRaw(
    const std::string& changer_path)
{
  const std::string command
      = "sg_raw --binary --readonly --request=65535 "
        + ShellQuote(StableTapePath(changer_path))
        + " b8 04 00 00 ff ff 01 00 ff ff 00 00";
  if (auto data = ReadBinaryStdoutCommand(command, false)) return data;
  return ReadBinaryStdoutCommand(command, true);
}

std::string BuildDiskStorageSdConfig(const DiskStorageConfig& disk)
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

std::string BuildDiskStorageDirConfig(const DiskStorageConfig& disk,
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
  return "tapedrive-" + std::to_string(changer_index) + "-"
         + std::to_string(drive_index);
}

std::string BuildTapeDevicesSdConfig(const TapeStorageConfig& tape)
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
           << "  ArchiveDevice = " << assignment.drive_paths[drive_index]
           << "\n\n"
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

std::string BuildAutochangerSdConfig(const TapeStorageConfig& tape)
{
  std::ostringstream conf;
  for (size_t changer_index = 0; changer_index < tape.assignments.size();
       ++changer_index) {
    const auto& assignment = tape.assignments[changer_index];
    conf << "Autochanger {\n"
         << "  Name = \"autochanger-" << changer_index << "\"\n"
         << "  Changer Device = " << assignment.changer_path << "\n\n"
         << "  Changer Command = "
            "\"/usr/lib/bareos/scripts/mtx-changer %c %o %S %a %d\"\n";
    for (size_t drive_index = 0; drive_index < assignment.drive_paths.size();
         ++drive_index) {
      conf << "  Device=" << BuildTapeDriveName(changer_index, drive_index)
           << "\n";
    }
    conf << "\n}\n\n";
  }
  return conf.str();
}

std::string BuildTapeStorageDirConfig(const TapeStorageConfig& tape,
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

std::string MajorVersion(const std::string& version)
{
  const auto dot = version.find('.');
  return dot == std::string::npos ? version : version.substr(0, dot);
}

bool IsElDistro(const std::string& distro)
{
  return distro == "almalinux" || distro == "centos" || distro == "ol"
         || distro == "openela" || distro == "oracle" || distro == "rocky";
}

}  // namespace

std::string CapFirst(std::string s)
{
  if (!s.empty()) s[0] = static_cast<char>(std::toupper(s[0]));
  return s;
}

std::string Trim(std::string value)
{
  auto is_space = [](unsigned char ch) { return std::isspace(ch) != 0; };
  while (!value.empty() && is_space(value.front())) value.erase(0, 1);
  while (!value.empty() && is_space(value.back())) value.pop_back();
  return value;
}

std::vector<std::string> BuildDefaultPackageList()
{
  return {"bareos-filedaemon",      "bareos-director",
          "bareos-storage",         "bareos-storage-tape",
          "bareos-storage-dedupable", "bareos-database-tools",
          "bareos-tools",           "bareos-webui-vue"};
}

std::string BuildRepoOsPath(const std::string& distro, const std::string& version)
{
  if (IsElDistro(distro)) return "EL_" + MajorVersion(version);
  return CapFirst(distro) + "_" + version;
}

std::vector<std::string> BuildAddRepoCmd(const std::string& distro,
                                         const std::string& version,
                                         const std::string& repo_type,
                                         const std::string& login,
                                         const std::string& password)
{
  const std::string base
      = (repo_type == "subscription")
            ? "https://download.bareos.com/bareos/release/latest"
            : "https://download.bareos.org/current";

  const std::string script_url
      = base + "/" + BuildRepoOsPath(distro, version)
        + "/add_bareos_repositories.sh";

  std::string curl_auth;
  if (repo_type == "subscription" && !login.empty()) {
    curl_auth = " -u '" + login + ":" + password + "'";
  }

  return {"bash", "-c",
          "curl -fsSL" + curl_auth + " " + script_url + " | bash"};
}

std::vector<std::string> BuildInstallCmd(
    const std::string& pkg_mgr,
    const std::vector<std::string>& packages)
{
  const auto& selected_packages
      = packages.empty() ? BuildDefaultPackageList() : packages;

  if (pkg_mgr == "apt") {
    std::vector<std::string> cmd = {"apt-get", "install", "-y"};
    cmd.insert(cmd.end(), selected_packages.begin(), selected_packages.end());
    return cmd;
  } else if (pkg_mgr == "dnf") {
    std::vector<std::string> cmd = {"dnf", "install", "-y"};
    cmd.insert(cmd.end(), selected_packages.begin(), selected_packages.end());
    return cmd;
  } else if (pkg_mgr == "yum") {
    std::vector<std::string> cmd = {"yum", "install", "-y"};
    cmd.insert(cmd.end(), selected_packages.begin(), selected_packages.end());
    return cmd;
  } else if (pkg_mgr == "zypper") {
    std::vector<std::string> cmd = {"zypper", "--non-interactive", "install"};
    cmd.insert(cmd.end(), selected_packages.begin(), selected_packages.end());
    return cmd;
  }
  return {"echo", "Unsupported package manager: " + pkg_mgr};
}

std::vector<std::string> BuildDbCmd()
{
  return {"bash", "-c",
          "/usr/lib/bareos/scripts/create_bareos_database && "
          "/usr/lib/bareos/scripts/make_bareos_tables && "
          "/usr/lib/bareos/scripts/grant_bareos_privileges"};
}

std::vector<std::string> BuildAdminUserCmd(const std::string& username,
                                           const std::string& password)
{
  const std::string console_conf
      = "/etc/bareos/bareos-dir.d/console/" + username + ".conf";
  return {"bash",
          "-c",
          "cat > " + console_conf + " << 'BAREOS_EOF'\n"
          "Console {\n"
          "  Name = " + username + "\n"
          "  Password = \"" + password + "\"\n"
          "  Profile = \"webui-admin\"\n"
          "  TLS Enable = No\n"
          "}\n"
          "BAREOS_EOF\n"
          "chmod 640 " + console_conf + " && "
          "chown root:bareos " + console_conf + " && "
          "systemctl reload bareos-dir 2>/dev/null || true"};
}

std::string DescribeDeviceIdentifier(const DeviceIdentifier& identifier)
{
  if (identifier.size() <= 4) return BytesToHex(identifier);

  const uint8_t code_set = identifier[0] & 0x0F;
  const std::vector<uint8_t> payload(identifier.begin() + 4, identifier.end());

  if ((code_set == 0x02 || code_set == 0x03) && !payload.empty()) {
    std::string text;
    text.reserve(payload.size());
    for (const auto byte : payload) {
      text += std::isprint(byte) != 0 ? static_cast<char>(byte) : '.';
    }
    text = CollapseWhitespace(text);
    if (!text.empty()) return text;
  }

  return BytesToHex(payload);
}

std::vector<DeviceIdentifier> ParseDeviceIdentifiersVpdPage(
    const std::vector<uint8_t>& page_data)
{
  std::vector<DeviceIdentifier> identifiers;
  if (page_data.size() < 4 || page_data[1] != 0x83) return identifiers;

  const size_t total_length
      = std::min(page_data.size(), static_cast<size_t>(4 + ReadBe16(page_data, 2)));
  for (size_t offset = 4; offset + 4 <= total_length;) {
    const size_t descriptor_length = 4 + page_data[offset + 3];
    if (offset + descriptor_length > total_length) break;

    const uint8_t association = (page_data[offset + 1] >> 4) & 0x3;
    if (association == 0) {
      identifiers.emplace_back(page_data.begin() + static_cast<long>(offset),
                               page_data.begin()
                                   + static_cast<long>(offset + descriptor_length));
    }
    offset += descriptor_length;
  }

  return identifiers;
}

std::vector<TapeChangerDriveIdentifier> ParseTapeLibraryDriveIdentifiers(
    const std::vector<uint8_t>& status_data)
{
  std::vector<TapeChangerDriveIdentifier> entries;
  if (status_data.size() < 8) return entries;

  for (size_t page_offset = 8; page_offset + 8 <= status_data.size();) {
    const uint8_t element_type = status_data[page_offset];
    const uint8_t flags = status_data[page_offset + 1];
    const size_t descriptor_length = ReadBe16(status_data, page_offset + 2);
    const size_t byte_count = ReadBe24(status_data, page_offset + 5);
    const size_t descriptors_offset = page_offset + 8;
    const size_t page_end
        = std::min(status_data.size(), descriptors_offset + byte_count);

    if (descriptor_length == 0 || descriptors_offset > page_end) break;

    if (element_type == 0x04) {
      for (size_t offset = descriptors_offset;
           offset + descriptor_length <= page_end; offset += descriptor_length) {
        if (descriptor_length < 12) continue;

        size_t ident_offset = offset + 12;
        if (flags & 0x80) ident_offset += 36;
        if (flags & 0x40) ident_offset += 36;
        if (ident_offset + 4 > offset + descriptor_length) continue;

        const size_t identifier_length = 4 + status_data[ident_offset + 3];
        if (identifier_length <= 4
            || ident_offset + identifier_length > offset + descriptor_length) {
          continue;
        }

        TapeChangerDriveIdentifier entry;
        entry.element_address = ReadBe16(status_data, offset);
        entry.device_identifiers.emplace_back(
            status_data.begin() + static_cast<long>(ident_offset),
            status_data.begin()
                + static_cast<long>(ident_offset + identifier_length));
        entries.push_back(std::move(entry));
      }
    }

    if (page_end <= page_offset) break;
    page_offset = page_end;
  }

  return entries;
}

std::vector<TapeAssignment> SuggestTapeAssignments(
    const std::vector<TapeChangerInfo>& changers,
    const std::vector<TapeDriveInfo>& drives)
{
  if (changers.empty() || drives.empty()) return {};

  std::vector<TapeAssignment> assignments;
  std::set<std::string> claimed_drives;
  bool matched_any_identifier = false;

  for (const auto& changer : changers) {
    TapeAssignment assignment;
    assignment.changer_path = changer.path;

    for (const auto& changer_drive : changer.drive_identifiers) {
      for (const auto& drive : drives) {
        if (claimed_drives.contains(drive.path)) continue;

        const auto matches = std::any_of(
            changer_drive.device_identifiers.begin(),
            changer_drive.device_identifiers.end(),
            [&drive](const auto& library_identifier) {
              return std::any_of(
                  drive.device_identifiers.begin(), drive.device_identifiers.end(),
                  [&library_identifier](const auto& drive_identifier) {
                    return SameDeviceIdentifier(library_identifier, drive_identifier);
                  });
            });
        if (!matches) continue;

        assignment.drive_paths.push_back(drive.path);
        claimed_drives.insert(drive.path);
        matched_any_identifier = true;
        break;
      }
    }

    if (!assignment.drive_paths.empty()) assignments.push_back(std::move(assignment));
  }

  if (matched_any_identifier) return assignments;

  if (changers.size() == 1) {
    TapeAssignment assignment;
    assignment.changer_path = changers.front().path;
    for (const auto& drive : drives) assignment.drive_paths.push_back(drive.path);
    return {assignment};
  }

  return {};
}

TapeStorageInventory DiscoverTapeStorageInventory()
{
  TapeStorageInventory inventory;
  if (!std::filesystem::exists("/dev/tape/by-id")) return inventory;

  std::map<std::string, TapeDriveInfo> drive_map;
  for (const auto& entry : std::filesystem::directory_iterator("/dev/tape/by-id")) {
    if (!entry.is_symlink()) continue;
    const auto name = entry.path().filename().string();
    if (name.ends_with("-changer")) {
      auto device = ReadTapeDeviceInfo(entry);
      TapeChangerInfo changer;
      changer.path = device.path;
      changer.canonical_path = device.canonical_path;
      changer.display_name = device.display_name;
      changer.identifier = device.identifier;
      changer.serial_number = device.serial_number;
      changer.vendor = device.vendor;
      changer.model = device.model;
      changer.firmware_version = device.firmware_version;
      changer.aliases = device.aliases;
      changer.status = QueryTapeChangerStatus(changer.path);
      inventory.changers.push_back(std::move(changer));
    } else if (name.ends_with("-nst") || name.ends_with("-st")) {
      auto info = ReadTapeDeviceInfo(entry);
      auto& drive = drive_map[info.canonical_path];
      if (drive.canonical_path.empty()) {
        drive = std::move(info);
      } else {
        for (const auto& alias : info.aliases) drive.aliases.push_back(alias);
      }
    }
  }

  for (auto& [_, drive] : drive_map) {
    std::sort(drive.aliases.begin(), drive.aliases.end());
    drive.aliases.erase(std::unique(drive.aliases.begin(), drive.aliases.end()),
                        drive.aliases.end());
    drive.aliases.erase(std::remove(drive.aliases.begin(), drive.aliases.end(),
                                    drive.path),
                        drive.aliases.end());
    if (const auto identifiers = QueryDriveIdentifiersRaw(drive.path)) {
      drive.device_identifiers = ParseDeviceIdentifiersVpdPage(*identifiers);
    }
    inventory.drives.push_back(std::move(drive));
  }

  for (auto& changer : inventory.changers) {
    if (const auto identifiers = QueryLibraryDriveIdentifiersRaw(changer.path)) {
      changer.drive_identifiers = ParseTapeLibraryDriveIdentifiers(*identifiers);
    }
  }

  std::sort(inventory.changers.begin(), inventory.changers.end(),
            [](const auto& lhs, const auto& rhs) { return lhs.path < rhs.path; });
  std::sort(inventory.drives.begin(), inventory.drives.end(),
            [](const auto& lhs, const auto& rhs) { return lhs.path < rhs.path; });
  inventory.suggested_assignments
      = SuggestTapeAssignments(inventory.changers, inventory.drives);
  return inventory;
}

TapeChangerInventory ParseTapeChangerInventoryStatus(
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

bool ValidateStorageConfig(const DiskStorageConfig& disk,
                           const TapeStorageConfig& tape,
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

    std::set<std::string> seen_drives;
    for (const auto& assignment : tape.assignments) {
      if (assignment.changer_path.empty() || assignment.drive_paths.empty()) {
        error = "Each tape changer needs at least one assigned drive.";
        return false;
      }

      for (const auto& drive_path : assignment.drive_paths) {
        if (!seen_drives.insert(drive_path).second) {
          error = "A tape drive can only be assigned to one changer.";
          return false;
        }
      }
    }
  }

  return true;
}

DirectorStorageDefaults ReadDirectorStorageDefaults()
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

std::string BuildConfigureStorageScript(const DiskStorageConfig& disk,
                                        const TapeStorageConfig& tape,
                                        const DirectorStorageDefaults& defaults)
{
  std::ostringstream script;
  script << "#!/bin/bash\n"
         << "set -euo pipefail\n\n"
         << "echo \"Preparing Bareos storage configuration directories...\"\n"
         << "install -d /etc/bareos/bareos-sd.d/device "
            "/etc/bareos/bareos-sd.d/autochanger "
            "/etc/bareos/bareos-dir.d/storage\n\n";

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
           << "cat <<'EOF' > /etc/bareos/bareos-sd.d/device/"
              "tape_devices.conf\n"
           << BuildTapeDevicesSdConfig(tape)
           << "EOF\n"
           << "cat <<'EOF' > /etc/bareos/bareos-sd.d/autochanger/"
              "autochanger.conf\n"
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
            "&& systemctl list-unit-files bareos-sd.service >/dev/null 2>&1; "
            "then\n"
         << "  echo \"Restarting bareos-sd...\"\n"
         << "  systemctl restart bareos-sd\n"
         << "else\n"
         << "  echo \"Skipping bareos-sd restart: systemd unit not found.\"\n"
         << "fi\n\n"
         << "if command -v systemctl >/dev/null 2>&1 "
            "&& systemctl list-unit-files bareos-dir.service >/dev/null 2>&1; "
            "then\n"
         << "  echo \"Restarting bareos-dir...\"\n"
         << "  systemctl restart bareos-dir\n"
         << "else\n"
         << "  echo \"Skipping bareos-dir restart: systemd unit not found.\"\n"
         << "fi\n";

  return script.str();
}

int RunGeneratedScript(const std::string& script, bool use_sudo, OutputCallback cb)
{
  char path_template[] = "/tmp/bareos-setup-storage-XXXXXX";
  const int fd = mkstemp(path_template);
  if (fd < 0) throw std::runtime_error("Failed to create temporary script file.");

  const std::string script_path(path_template);
  close(fd);

  try {
    std::ofstream output(script_path);
    if (!output) {
      throw std::runtime_error(
          "Failed to open temporary script file for writing.");
    }
    output << script;
    output.close();
    if (!output) {
      throw std::runtime_error("Failed to write temporary script file.");
    }

    const int exit_code = RunCommand({"bash", script_path}, use_sudo, cb);
    std::remove(script_path.c_str());
    return exit_code;
  } catch (...) {
    std::remove(script_path.c_str());
    throw;
  }
}
