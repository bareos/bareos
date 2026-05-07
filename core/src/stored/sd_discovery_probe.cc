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
#include <filesystem>
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

bool InList(std::string_view value,
            std::initializer_list<std::string_view> values)
{
  return std::find(values.begin(), values.end(), value) != values.end();
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
  struct addrinfo hints {};
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
      std::unique(filesystems.begin(), filesystems.end(),
                  [](const FilesystemCandidate& lhs,
                     const FilesystemCandidate& rhs) {
                    return lhs.mountpoint == rhs.mountpoint
                           && lhs.filesystem_type == rhs.filesystem_type;
                  }),
      filesystems.end());
}

#if defined(HAVE_LINUX_OS)
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

    struct statvfs stats {};
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
    ULARGE_INTEGER free_bytes_available {};
    ULARGE_INTEGER total_bytes {};
    ULARGE_INTEGER free_bytes {};

    if (!GetVolumeInformationA(drive, nullptr, 0, nullptr, nullptr, nullptr,
                               filesystem_type, sizeof(filesystem_type))) {
      continue;
    }
    if (!GetDiskFreeSpaceExA(drive, &free_bytes_available, &total_bytes,
                             &free_bytes)) {
      continue;
    }

    const bool writable = (GetDriveTypeA(drive) != DRIVE_CDROM);
    filesystems.emplace_back(BuildFilesystemCandidate(
        drive, filesystem_type, total_bytes.QuadPart, free_bytes.QuadPart,
        writable));
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
  json_object_set_new(obj, "total_bytes",
                      json_integer(candidate.total_bytes));
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

}  // namespace

bool IsFilesystemTypeIgnoredForStorageDiscovery(
    std::string_view filesystem_type)
{
  return InList(filesystem_type,
                {"proc", "procfs", "sysfs", "tmpfs", "devtmpfs", "devfs",
                 "devpts", "mqueue", "tracefs", "securityfs", "pstore",
                 "efivarfs", "cgroup", "cgroup2", "rpc_pipefs",
                 "binfmt_misc", "fusectl", "debugfs", "nsfs"});
}

bool IsLocalFilesystemTypeForStorageDiscovery(std::string_view filesystem_type)
{
  return !InList(filesystem_type, {"nfs",   "nfs4",  "cifs",   "smbfs",
                                   "sshfs", "glusterfs", "ceph",   "cephfs",
                                   "gpfs",  "lustre", "afs",    "9p",
                                   "davfs", "davfs2", "coda"});
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
#elif defined(HAVE_FREEBSD_OS) || defined(HAVE_DARWIN_OS)
  report.filesystems = ProbeFilesystemCandidatesBsd();
#elif defined(HAVE_WIN32)
  report.filesystems = ProbeFilesystemCandidatesWindows();
#else
  report.filesystems = ProbeFilesystemCandidatesFallback();
#endif

  return report;
}

std::string StorageDiscoveryReportToJson(const StorageDiscoveryReport& report)
{
  json_t* obj = json_object();
  json_object_set_new(obj, "hostname", json_string(report.hostname.c_str()));
  json_object_set_new(obj, "fqdn", json_string(report.fqdn.c_str()));

  json_t* filesystems = json_array();
  for (const auto& filesystem : report.filesystems) {
    json_array_append_new(filesystems, JsonFilesystemCandidate(filesystem));
  }
  json_object_set_new(obj, "filesystems", filesystems);

  json_object_set_new(obj, "tape_devices", json_array());
  json_object_set_new(obj, "changers", json_array());

  char* raw = json_dumps(obj, JSON_COMPACT);
  json_decref(obj);
  std::string result(raw ? raw : "{}");
  free(raw);
  return result;
}

}  // namespace storagedaemon
