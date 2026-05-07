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

#include "stored/sd_discovery_probe.h"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string_view>
#include <set>

#include <jansson.h>

#include "include/bareos.h"
#include "findlib/fstype.h"

#if defined(HAVE_LINUX_OS)
#  include <mntent.h>
#  include <netdb.h>
#  include <sys/statvfs.h>
#  include <unistd.h>
#elif defined(HAVE_FREEBSD_OS) || defined(HAVE_DARWIN_OS)
#  include <netdb.h>
#  include <sys/mount.h>
#  include <sys/param.h>
#  include <unistd.h>
#elif defined(HAVE_WIN32)
#  include <fileapi.h>
#else
#  include <unistd.h>
#endif

namespace storagedaemon {

namespace {

constexpr std::string_view kDefaultArchiveRoot = "/var/lib/bareos/storage";

std::optional<std::string> GetEnvironmentVariable(std::string_view name)
{
  if (const char* value = std::getenv(std::string{name}.c_str());
      value && value[0] != '\0') {
    return std::string{value};
  }
  return std::nullopt;
}

bool InList(std::string_view value,
            std::initializer_list<std::string_view> values)
{
  return std::find(values.begin(), values.end(), value) != values.end();
}

std::string TrimWhitespaceAndNuls(std::string value)
{
  while (!value.empty()
         && (value.back() == '\0' || value.back() == ' ' || value.back() == '\n'
             || value.back() == '\r' || value.back() == '\t')) {
    value.pop_back();
  }

  auto first = value.find_first_not_of(" \n\r\t");
  if (first == std::string::npos) { return {}; }
  return value.substr(first);
}

std::optional<std::string> ReadTextFile(const std::filesystem::path& path)
{
  std::ifstream stream(path, std::ios::binary);
  if (!stream.is_open()) { return std::nullopt; }
  std::string content{std::istreambuf_iterator<char>(stream),
                      std::istreambuf_iterator<char>()};
  return TrimWhitespaceAndNuls(std::move(content));
}

std::optional<std::string> ReadBinaryFile(const std::filesystem::path& path)
{
  std::ifstream stream(path, std::ios::binary);
  if (!stream.is_open()) { return std::nullopt; }
  return std::string{std::istreambuf_iterator<char>(stream),
                     std::istreambuf_iterator<char>()};
}

std::string NormalizeMountpoint(std::string mountpoint)
{
  if (mountpoint.empty()) { return "/"; }

#if defined(HAVE_WIN32)
  while (mountpoint.size() > 3 && IsPathSeparator(mountpoint.back())) {
    mountpoint.pop_back();
  }
#else
  while (mountpoint.size() > 1 && IsPathSeparator(mountpoint.back())) {
    mountpoint.pop_back();
  }
#endif

  return mountpoint;
}

bool IsArchiveCandidate(const FilesystemCandidate& candidate)
{
  return candidate.local && candidate.writable && candidate.total_bytes > 0;
}

std::string DetectShortHostname()
{
  char hostname[1024]{};
  if (gethostname(hostname, sizeof(hostname) - 1) != 0) { return {}; }

  hostname[sizeof(hostname) - 1] = '\0';
  return hostname;
}

std::string DetectFqdn(const std::string& hostname)
{
  if (hostname.empty()) { return {}; }

#ifdef HAVE_GETADDRINFO
  struct addrinfo hints{};
  hints.ai_family = AF_UNSPEC;
  hints.ai_flags = AI_CANONNAME;

  struct addrinfo* ai = nullptr;
  if (getaddrinfo(hostname.c_str(), nullptr, &hints, &ai) == 0 && ai) {
    std::string fqdn
        = ai->ai_canonname ? std::string(ai->ai_canonname) : hostname;
    freeaddrinfo(ai);
    return fqdn;
  }
#endif

  return hostname;
}

FilesystemCandidate BuildFilesystemCandidate(std::string mountpoint,
                                             std::string filesystem_type,
                                             uint64_t total_bytes,
                                             uint64_t free_bytes,
                                             bool writable)
{
  FilesystemCandidate candidate;
  candidate.mountpoint = NormalizeMountpoint(std::move(mountpoint));
  candidate.filesystem_type = std::move(filesystem_type);
  candidate.total_bytes = total_bytes;
  candidate.free_bytes = free_bytes;
  candidate.writable = writable;
  candidate.local
      = IsLocalFilesystemTypeForStorageDiscovery(candidate.filesystem_type);
  candidate.suitable_for_archive = IsArchiveCandidate(candidate);
  if (candidate.suitable_for_archive) {
    candidate.recommended_archive_path
        = RecommendedArchivePathForMountpoint(candidate.mountpoint);
  }
  return candidate;
}

void SortAndDeduplicate(std::vector<FilesystemCandidate>& filesystems)
{
  std::sort(filesystems.begin(), filesystems.end(),
            [](const FilesystemCandidate& lhs, const FilesystemCandidate& rhs) {
              if (lhs.mountpoint != rhs.mountpoint) {
                return lhs.mountpoint < rhs.mountpoint;
              }
              return lhs.filesystem_type < rhs.filesystem_type;
            });

  filesystems.erase(
      std::unique(
          filesystems.begin(), filesystems.end(),
          [](const FilesystemCandidate& lhs, const FilesystemCandidate& rhs) {
            return lhs.mountpoint == rhs.mountpoint
                   && lhs.filesystem_type == rhs.filesystem_type;
          }),
      filesystems.end());
}

#if defined(HAVE_LINUX_OS)
std::filesystem::path DiscoverySysfsRoot()
{
  if (auto configured
      = GetEnvironmentVariable("BAREOS_SD_DISCOVERY_SYSFS_ROOT")) {
    return *configured;
  }
  return "/sys";
}

std::filesystem::path DiscoveryDeviceRoot()
{
  if (auto configured
      = GetEnvironmentVariable("BAREOS_SD_DISCOVERY_DEV_ROOT")) {
    return *configured;
  }
  return "/dev";
}

std::optional<std::string> ParseScsiPage80Serial(std::string_view page)
{
  if (page.size() < 4) { return std::nullopt; }

  const auto length
      = (static_cast<uint16_t>(static_cast<unsigned char>(page[2])) << 8)
        | static_cast<uint16_t>(static_cast<unsigned char>(page[3]));
  if (page.size() < static_cast<std::size_t>(4 + length)) {
    return std::nullopt;
  }

  auto serial = TrimWhitespaceAndNuls(std::string{page.substr(4, length)});
  if (serial.empty()) { return std::nullopt; }
  return serial;
}

std::optional<std::string> ReadScsiSerial(
    const std::filesystem::path& device_path)
{
  auto page80 = ReadBinaryFile(device_path / "vpd_pg80");
  if (page80) {
    auto serial = ParseScsiPage80Serial(*page80);
    if (serial) { return serial; }
  }

  auto serial = ReadTextFile(device_path / "serial");
  if (serial && !serial->empty()) { return serial; }

  return std::nullopt;
}

std::string ToLowerHex(std::string_view bytes)
{
  constexpr char kHexDigits[] = "0123456789abcdef";

  std::string encoded;
  encoded.reserve(bytes.size() * 2);
  for (unsigned char byte : bytes) {
    encoded.push_back(kHexDigits[(byte >> 4) & 0x0f]);
    encoded.push_back(kHexDigits[byte & 0x0f]);
  }
  return encoded;
}

std::optional<std::string> FormatScsiDeviceIdentifier(uint8_t designator_type,
                                                      uint8_t code_set,
                                                      std::string_view data)
{
  switch (designator_type) {
    case 0x03:
      return std::string{"naa."} + ToLowerHex(data);
    case 0x02:
      return std::string{"eui."} + ToLowerHex(data);
    case 0x01:
      if (code_set == 0x02 || code_set == 0x03) {
        auto value = TrimWhitespaceAndNuls(std::string{data});
        if (!value.empty()) { return std::string{"t10."} + value; }
      }
      break;
    case 0x08:
      if (code_set == 0x02 || code_set == 0x03) {
        auto value = TrimWhitespaceAndNuls(std::string{data});
        if (!value.empty()) { return std::string{"scsi-name."} + value; }
      }
      break;
    default:
      break;
  }

  return std::nullopt;
}

std::optional<std::string> ParseScsiPage83DeviceIdentifier(
    std::string_view page)
{
  if (page.size() < 4) { return std::nullopt; }

  const auto length
      = (static_cast<uint16_t>(static_cast<unsigned char>(page[2])) << 8)
        | static_cast<uint16_t>(static_cast<unsigned char>(page[3]));
  if (page.size() < static_cast<std::size_t>(4 + length)) {
    return std::nullopt;
  }

  std::optional<std::string> fallback{};
  std::size_t offset = 4;
  const auto end = static_cast<std::size_t>(4 + length);
  while (offset + 4 <= end) {
    const auto descriptor_length = static_cast<std::size_t>(
        static_cast<unsigned char>(page[offset + 3]));
    const auto descriptor_end = offset + 4 + descriptor_length;
    if (descriptor_end > end) { break; }

    const auto code_set
        = static_cast<uint8_t>(static_cast<unsigned char>(page[offset]) & 0x0f);
    const auto association = static_cast<uint8_t>(
        (static_cast<unsigned char>(page[offset + 1]) >> 4) & 0x03);
    const auto designator_type = static_cast<uint8_t>(
        static_cast<unsigned char>(page[offset + 1]) & 0x0f);
    if (association == 0) {
      auto identifier = FormatScsiDeviceIdentifier(
          designator_type, code_set,
          std::string_view{page.data() + offset + 4, descriptor_length});
      if (identifier) {
        if (designator_type == 0x03 || designator_type == 0x02) {
          return identifier;
        }
        if (!fallback) { fallback = std::move(identifier); }
      }
    }

    offset = descriptor_end;
  }

  return fallback;
}

std::optional<std::string> ReadScsiDeviceIdentifier(
    const std::filesystem::path& device_path)
{
  auto page83 = ReadBinaryFile(device_path / "vpd_pg83");
  if (!page83) { return std::nullopt; }
  return ParseScsiPage83DeviceIdentifier(*page83);
}

std::string ResolveGenericDeviceNode(const std::filesystem::path& device_path)
{
  for (const auto& child_directory :
       {device_path / "scsi_generic", device_path / "generic"}) {
    std::error_code error_code;
    if (!std::filesystem::is_directory(child_directory, error_code)) {
      continue;
    }

    for (const auto& entry :
         std::filesystem::directory_iterator(child_directory)) {
      const auto name = entry.path().filename().string();
      if (name.size() >= 3 && name.starts_with("sg")) {
        return (DiscoveryDeviceRoot() / name).string();
      }
    }
  }

  return {};
}

std::vector<TapeDeviceInfo> ProbeTapeDevicesLinux()
{
  std::vector<TapeDeviceInfo> tape_devices;
  const auto class_root = DiscoverySysfsRoot() / "class" / "scsi_tape";
  std::error_code error_code;
  if (!std::filesystem::is_directory(class_root, error_code)) {
    return tape_devices;
  }

  for (const auto& entry : std::filesystem::directory_iterator(class_root)) {
    TapeDeviceInfo tape_device;
    const auto class_name = entry.path().filename().string();
    tape_device.device_node = (DiscoveryDeviceRoot() / class_name).string();

    const auto device_path = entry.path() / "device";
    tape_device.generic_device_node = ResolveGenericDeviceNode(device_path);
    if (auto vendor = ReadTextFile(device_path / "vendor")) {
      tape_device.vendor = *vendor;
    }
    if (auto model = ReadTextFile(device_path / "model")) {
      tape_device.model = *model;
    }
    tape_device.device_identifier = ReadScsiDeviceIdentifier(device_path);
    if (auto serial = ReadScsiSerial(device_path)) {
      tape_device.serial = *serial;
    }

    tape_device.accessible
        = std::filesystem::exists(tape_device.device_node, error_code);
    if (!tape_device.accessible) {
      tape_device.accessibility_error = "device node not accessible";
    }
    tape_devices.emplace_back(std::move(tape_device));
  }

  std::sort(tape_devices.begin(), tape_devices.end(),
            [](const TapeDeviceInfo& lhs, const TapeDeviceInfo& rhs) {
              return lhs.device_node < rhs.device_node;
            });
  return tape_devices;
}

std::vector<ChangerInfo> ProbeChangersLinux()
{
  std::vector<ChangerInfo> changers;
  const auto class_root = DiscoverySysfsRoot() / "class" / "scsi_changer";
  std::error_code error_code;
  if (!std::filesystem::is_directory(class_root, error_code)) {
    return changers;
  }

  for (const auto& entry : std::filesystem::directory_iterator(class_root)) {
    ChangerInfo changer;
    const auto class_name = entry.path().filename().string();
    const auto device_path = entry.path() / "device";
    const auto generic_device_node = ResolveGenericDeviceNode(device_path);
    changer.device_node = !generic_device_node.empty()
                              ? generic_device_node
                              : (DiscoveryDeviceRoot() / class_name).string();

    if (auto vendor = ReadTextFile(device_path / "vendor")) {
      changer.vendor = *vendor;
    }
    if (auto model = ReadTextFile(device_path / "model")) {
      changer.model = *model;
    }
    changer.device_identifier = ReadScsiDeviceIdentifier(device_path);
    if (auto serial = ReadScsiSerial(device_path)) { changer.serial = *serial; }

    changer.accessible
        = std::filesystem::exists(changer.device_node, error_code);
    if (!changer.accessible) {
      changer.accessibility_error = "device node not accessible";
    }
    changers.emplace_back(std::move(changer));
  }

  std::sort(changers.begin(), changers.end(),
            [](const ChangerInfo& lhs, const ChangerInfo& rhs) {
              return lhs.device_node < rhs.device_node;
            });
  return changers;
}

std::vector<FilesystemCandidate> ProbeFilesystemCandidatesLinux()
{
  std::vector<FilesystemCandidate> filesystems;
  std::set<std::string> seen_mountpoints;

  FILE* mounts = setmntent("/proc/mounts", "r");
  if (mounts == nullptr) { mounts = setmntent(_PATH_MOUNTED, "r"); }
  if (mounts == nullptr) { return filesystems; }

  while (auto* entry = getmntent(mounts)) {
    const std::string filesystem_type = entry->mnt_type ? entry->mnt_type : "";
    if (IsFilesystemTypeIgnoredForStorageDiscovery(filesystem_type)) {
      continue;
    }

    const std::string mountpoint = NormalizeMountpoint(
        entry->mnt_dir ? std::string(entry->mnt_dir) : std::string{});
    if (!seen_mountpoints.insert(mountpoint).second) { continue; }

    struct statvfs stats{};
    if (statvfs(mountpoint.c_str(), &stats) != 0) { continue; }

    const uint64_t total_bytes
        = static_cast<uint64_t>(stats.f_blocks) * stats.f_frsize;
    const uint64_t free_bytes
        = static_cast<uint64_t>(stats.f_bavail) * stats.f_frsize;
    const bool writable = access(mountpoint.c_str(), W_OK) == 0;

    filesystems.emplace_back(BuildFilesystemCandidate(
        mountpoint, filesystem_type, total_bytes, free_bytes, writable));
  }

  endmntent(mounts);
  SortAndDeduplicate(filesystems);
  return filesystems;
}
#endif

#if defined(HAVE_FREEBSD_OS) || defined(HAVE_DARWIN_OS)
std::vector<FilesystemCandidate> ProbeFilesystemCandidatesBsd()
{
  std::vector<FilesystemCandidate> filesystems;

  struct statfs* mounts = nullptr;
  const int count = getmntinfo(&mounts, MNT_NOWAIT);
  for (int i = 0; i < count; ++i) {
    const std::string filesystem_type = mounts[i].f_fstypename;
    if (IsFilesystemTypeIgnoredForStorageDiscovery(filesystem_type)) {
      continue;
    }

    const std::string mountpoint = NormalizeMountpoint(mounts[i].f_mntonname);
    const uint64_t total_bytes
        = static_cast<uint64_t>(mounts[i].f_blocks) * mounts[i].f_bsize;
    const uint64_t free_bytes
        = static_cast<uint64_t>(mounts[i].f_bavail) * mounts[i].f_bsize;
    const bool writable = access(mountpoint.c_str(), W_OK) == 0;

    filesystems.emplace_back(BuildFilesystemCandidate(
        mountpoint, filesystem_type, total_bytes, free_bytes, writable));
  }

  SortAndDeduplicate(filesystems);
  return filesystems;
}
#endif

#if defined(HAVE_WIN32)
std::vector<FilesystemCandidate> ProbeFilesystemCandidatesWindows()
{
  std::vector<FilesystemCandidate> filesystems;
  char drives[512]{};
  const DWORD length = GetLogicalDriveStringsA(sizeof(drives), drives);
  if (length == 0 || length >= sizeof(drives)) { return filesystems; }

  for (const char* drive = drives; *drive != '\0';
       drive += std::strlen(drive) + 1) {
    char filesystem_type[MAX_PATH]{};
    ULARGE_INTEGER free_bytes_available{};
    ULARGE_INTEGER total_bytes{};
    ULARGE_INTEGER free_bytes{};

    if (!GetVolumeInformationA(drive, nullptr, 0, nullptr, nullptr, nullptr,
                               filesystem_type, sizeof(filesystem_type))) {
      continue;
    }
    if (!GetDiskFreeSpaceExA(drive, &free_bytes_available, &total_bytes,
                             &free_bytes)) {
      continue;
    }

    const bool writable = (GetDriveTypeA(drive) != DRIVE_CDROM);
    filesystems.emplace_back(
        BuildFilesystemCandidate(drive, filesystem_type, total_bytes.QuadPart,
                                 free_bytes.QuadPart, writable));
  }

  SortAndDeduplicate(filesystems);
  return filesystems;
}
#endif

#if !defined(HAVE_LINUX_OS) && !defined(HAVE_FREEBSD_OS) \
    && !defined(HAVE_DARWIN_OS) && !defined(HAVE_WIN32)
std::vector<FilesystemCandidate> ProbeFilesystemCandidatesFallback()
{
  std::vector<FilesystemCandidate> filesystems;
  constexpr const char* root = "/";
  char filesystem_type[MAX_NAME_LENGTH]{};
  if (!fstype(root, filesystem_type, sizeof(filesystem_type))) {
    bstrncpy(filesystem_type, "unknown", sizeof(filesystem_type));
  }

  std::error_code ec;
  const auto space = std::filesystem::space(root, ec);
  const uint64_t total_bytes = ec ? 0 : space.capacity;
  const uint64_t free_bytes = ec ? 0 : space.available;
  const bool writable = access(root, W_OK) == 0;

  filesystems.emplace_back(BuildFilesystemCandidate(
      root, filesystem_type, total_bytes, free_bytes, writable));
  return filesystems;
}
#endif

json_t* JsonFilesystemCandidate(const FilesystemCandidate& candidate)
{
  auto json_string_or_null = [](const std::optional<std::string>& value) {
    return value ? json_string(value->c_str()) : json_null();
  };
  auto json_integer_or_null = [](const std::optional<uint64_t>& value) {
    return value ? json_integer(*value) : json_null();
  };

  json_t* deduplication = json_object();
  json_object_set_new(deduplication, "support",
                      json_string(candidate.deduplication.support.c_str()));
  json_object_set_new(
      deduplication, "block_size_bytes",
      json_integer_or_null(candidate.deduplication.block_size_bytes));
  json_object_set_new(deduplication, "mode",
                      json_string_or_null(candidate.deduplication.mode));
  json_object_set_new(deduplication, "source",
                      json_string_or_null(candidate.deduplication.source));
  json_object_set_new(deduplication, "note",
                      json_string_or_null(candidate.deduplication.note));

  json_t* obj = json_object();
  json_object_set_new(obj, "mountpoint",
                      json_string(candidate.mountpoint.c_str()));
  json_object_set_new(obj, "filesystem_type",
                      json_string(candidate.filesystem_type.c_str()));
  json_object_set_new(obj, "total_bytes", json_integer(candidate.total_bytes));
  json_object_set_new(obj, "free_bytes", json_integer(candidate.free_bytes));
  json_object_set_new(obj, "writable", json_boolean(candidate.writable));
  json_object_set_new(obj, "local", json_boolean(candidate.local));
  json_object_set_new(obj, "suitable_for_archive",
                      json_boolean(candidate.suitable_for_archive));
  json_object_set_new(obj, "recommended_archive_path",
                      json_string(candidate.recommended_archive_path.c_str()));
  json_object_set_new(obj, "deduplication", deduplication);
  return obj;
}

json_t* JsonTapeDeviceInfo(const TapeDeviceInfo& tape_device)
{
  auto json_string_or_null = [](const std::optional<std::string>& value) {
    return value ? json_string(value->c_str()) : json_null();
  };
  json_t* obj = json_object();
  json_object_set_new(obj, "device_node",
                      json_string(tape_device.device_node.c_str()));
  json_object_set_new(obj, "generic_device_node",
                      json_string(tape_device.generic_device_node.c_str()));
  json_object_set_new(obj, "vendor", json_string(tape_device.vendor.c_str()));
  json_object_set_new(obj, "model", json_string(tape_device.model.c_str()));
  json_object_set_new(obj, "device_identifier",
                      json_string_or_null(tape_device.device_identifier));
  json_object_set_new(obj, "serial", json_string(tape_device.serial.c_str()));
  json_object_set_new(obj, "accessible", json_boolean(tape_device.accessible));
  json_object_set_new(obj, "accessibility_error",
                      json_string(tape_device.accessibility_error.c_str()));
  return obj;
}

json_t* JsonChangerInfo(const ChangerInfo& changer)
{
  auto json_string_or_null = [](const std::optional<std::string>& value) {
    return value ? json_string(value->c_str()) : json_null();
  };
  auto json_integer_or_null = [](const std::optional<uint32_t>& value) {
    return value ? json_integer(*value) : json_null();
  };
  json_t* drive_device_nodes = json_array();
  for (const auto& drive_device_node : changer.drive_device_nodes) {
    json_array_append_new(drive_device_nodes,
                          json_string(drive_device_node.c_str()));
  }

  json_t* drives = json_array();
  for (const auto& drive : changer.drives) {
    json_t* item = json_object();
    json_object_set_new(item, "tape_device_node",
                        json_string(drive.tape_device_node.c_str()));
    json_object_set_new(item, "generic_device_node",
                        json_string(drive.generic_device_node.c_str()));
    json_object_set_new(item, "drive_element_address",
                        json_integer_or_null(drive.drive_element_address));
    json_object_set_new(item, "device_identifier",
                        json_string_or_null(drive.device_identifier));
    json_object_set_new(item, "serial", json_string_or_null(drive.serial));
    json_object_set_new(item, "source", json_string_or_null(drive.source));
    json_array_append_new(drives, item);
  }

  json_t* obj = json_object();
  json_object_set_new(obj, "device_node",
                      json_string(changer.device_node.c_str()));
  json_object_set_new(obj, "vendor", json_string(changer.vendor.c_str()));
  json_object_set_new(obj, "model", json_string(changer.model.c_str()));
  json_object_set_new(obj, "device_identifier",
                      json_string_or_null(changer.device_identifier));
  json_object_set_new(obj, "serial", json_string(changer.serial.c_str()));
  json_object_set_new(obj, "drive_device_nodes", drive_device_nodes);
  json_object_set_new(obj, "drives", drives);
  json_object_set_new(obj, "accessible", json_boolean(changer.accessible));
  json_object_set_new(obj, "accessibility_error",
                      json_string(changer.accessibility_error.c_str()));
  return obj;
}

}  // namespace

bool IsFilesystemTypeIgnoredForStorageDiscovery(
    std::string_view filesystem_type)
{
  return InList(
      filesystem_type,
      {"proc", "procfs", "sysfs", "tmpfs", "devtmpfs", "devfs", "devpts",
       "mqueue", "tracefs", "securityfs", "pstore", "efivarfs", "cgroup",
       "cgroup2", "rpc_pipefs", "binfmt_misc", "fusectl", "debugfs", "nsfs"});
}

bool IsLocalFilesystemTypeForStorageDiscovery(std::string_view filesystem_type)
{
  return !InList(
      filesystem_type,
      {"nfs", "nfs4", "cifs", "smbfs", "sshfs", "glusterfs", "ceph", "cephfs",
       "gpfs", "lustre", "afs", "9p", "davfs", "davfs2", "coda"});
}

std::string RecommendedArchivePathForMountpoint(std::string_view mountpoint)
{
  const auto normalized = NormalizeMountpoint(std::string{mountpoint});
  if (normalized.empty() || normalized == "/") {
    return std::string{kDefaultArchiveRoot};
  }

  return normalized + "/bareos/storage";
}

StorageDiscoveryReport ProbeStorageDiscoveryReport()
{
  StorageDiscoveryReport report;

  report.hostname = DetectShortHostname();
  report.fqdn = DetectFqdn(report.hostname);
  if (report.hostname.empty() && !report.fqdn.empty()) {
    const auto dot = report.fqdn.find('.');
    report.hostname
        = dot == std::string::npos ? report.fqdn : report.fqdn.substr(0, dot);
  }

#if defined(HAVE_LINUX_OS)
  report.filesystems = ProbeFilesystemCandidatesLinux();
  report.tape_devices = ProbeTapeDevicesLinux();
  report.changers = ProbeChangersLinux();
#elif defined(HAVE_FREEBSD_OS) || defined(HAVE_DARWIN_OS)
  report.filesystems = ProbeFilesystemCandidatesBsd();
#elif defined(HAVE_WIN32)
  report.filesystems = ProbeFilesystemCandidatesWindows();
#else
  report.filesystems = ProbeFilesystemCandidatesFallback();
#endif

  return report;
}

StorageDiscoveryReport EnrichChangerDriveSerials(StorageDiscoveryReport report)
{
  for (auto& changer : report.changers) {
    for (auto& drive : changer.drives) {
      if (drive.serial) { continue; }

      for (const auto& tape_device : report.tape_devices) {
        const bool matches_tape_device
            = !drive.tape_device_node.empty()
              && drive.tape_device_node == tape_device.device_node;
        const bool matches_generic_device
            = !drive.generic_device_node.empty()
              && drive.generic_device_node == tape_device.generic_device_node;
        if ((!matches_tape_device && !matches_generic_device)
            || tape_device.serial.empty()) {
          continue;
        }

        drive.serial = tape_device.serial;
        break;
      }
    }
  }

  return report;
}

std::string StorageDiscoveryReportToJson(const StorageDiscoveryReport& report)
{
  auto enriched = EnrichChangerDriveSerials(StorageDiscoveryReport{report});
  json_t* obj = json_object();
  json_object_set_new(obj, "hostname", json_string(enriched.hostname.c_str()));
  json_object_set_new(obj, "fqdn", json_string(enriched.fqdn.c_str()));

  json_t* filesystems = json_array();
  for (const auto& filesystem : enriched.filesystems) {
    json_array_append_new(filesystems, JsonFilesystemCandidate(filesystem));
  }
  json_object_set_new(obj, "filesystems", filesystems);

  json_t* tape_devices = json_array();
  for (const auto& tape_device : enriched.tape_devices) {
    json_array_append_new(tape_devices, JsonTapeDeviceInfo(tape_device));
  }
  json_object_set_new(obj, "tape_devices", tape_devices);

  json_t* changers = json_array();
  for (const auto& changer : enriched.changers) {
    json_array_append_new(changers, JsonChangerInfo(changer));
  }
  json_object_set_new(obj, "changers", changers);

  char* raw = json_dumps(obj, JSON_COMPACT);
  json_decref(obj);
  std::string result(raw ? raw : "{}");
  free(raw);
  return result;
}

}  // namespace storagedaemon
