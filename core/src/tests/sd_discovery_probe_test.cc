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

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <jansson.h>

#include "include/bareos.h"
#include "stored/sd_discovery_probe.h"

namespace {

class ScopedEnvironmentVariable {
 public:
  ScopedEnvironmentVariable(const char* name, std::string value) : name_(name)
  {
    if (const char* existing = std::getenv(name_); existing) {
      previous_ = existing;
    }
#if HAVE_WIN32
    _putenv_s(name_, value.c_str());
#else
    setenv(name_, value.c_str(), 1);
#endif
  }

  ~ScopedEnvironmentVariable()
  {
    if (previous_) {
#if HAVE_WIN32
      _putenv_s(name_, previous_->c_str());
#else
      setenv(name_, previous_->c_str(), 1);
#endif
    } else {
#if HAVE_WIN32
      _putenv_s(name_, "");
#else
      unsetenv(name_);
#endif
    }
  }

 private:
  const char* name_;
  std::optional<std::string> previous_{};
};

class ScopedDirectory {
 public:
  ScopedDirectory()
  {
    const auto unique
        = "sd-discovery-probe-test-" + std::to_string(std::rand());
    path_ = std::filesystem::temp_directory_path() / unique;
    std::filesystem::create_directories(path_);
  }

  ~ScopedDirectory()
  {
    std::error_code error_code;
    std::filesystem::remove_all(path_, error_code);
  }

  const std::filesystem::path& path() const { return path_; }

 private:
  std::filesystem::path path_{};
};

#if defined(HAVE_LINUX_OS)
void WriteBinaryFile(const std::filesystem::path& path,
                     std::string_view content)
{
  std::filesystem::create_directories(path.parent_path());
  std::ofstream stream(path, std::ios::binary | std::ios::trunc);
  stream.write(content.data(), static_cast<std::streamsize>(content.size()));
}

std::string BuildPage80(std::string_view serial)
{
  std::string page;
  page.push_back('\0');
  page.push_back(static_cast<char>(0x80));
  page.push_back(static_cast<char>((serial.size() >> 8) & 0xff));
  page.push_back(static_cast<char>(serial.size() & 0xff));
  page.append(serial);
  return page;
}

std::string BuildPage83Naa(std::string_view designator)
{
  std::string page;
  page.push_back('\0');
  page.push_back(static_cast<char>(0x83));
  page.push_back('\0');
  page.push_back(static_cast<char>(4 + designator.size()));
  page.push_back(static_cast<char>(0x01));
  page.push_back(static_cast<char>(0x03));
  page.push_back('\0');
  page.push_back(static_cast<char>(designator.size()));
  page.append(designator);
  return page;
}
#endif

}  // namespace

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
  report.tape_devices.push_back({
      .device_node = "/dev/nst0",
      .generic_device_node = "/dev/sg3",
      .vendor = "IBM",
      .model = "ULTRIUM-HH8",
      .device_identifier = "naa.11223344",
      .serial = "ABC123",
      .accessible = true,
  });
  report.changers.push_back({
      .device_node = "/dev/sg4",
      .vendor = "IBM",
      .model = "3573-TL",
      .device_identifier = "naa.aabbccdd",
      .serial = "CHG1",
      .drive_device_nodes = {"/dev/nst0"},
      .drives = {{
          .tape_device_node = "/dev/nst0",
          .generic_device_node = "/dev/sg3",
          .drive_element_address = 256,
          .device_identifier = "naa.1234",
          .source = "test",
      }},
      .accessible = true,
  });

  const auto json = storagedaemon::StorageDiscoveryReportToJson(report);
  json_error_t error{};
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
  EXPECT_STREQ(json_string_value(json_object_get(deduplication, "support")),
               "unknown");
  EXPECT_TRUE(json_is_null(json_object_get(deduplication, "block_size_bytes")));
  EXPECT_TRUE(json_is_null(json_object_get(deduplication, "mode")));
  EXPECT_TRUE(json_is_null(json_object_get(deduplication, "source")));
  EXPECT_TRUE(json_is_null(json_object_get(deduplication, "note")));

  json_t* tape_devices = json_object_get(parsed, "tape_devices");
  ASSERT_TRUE(json_is_array(tape_devices));
  ASSERT_EQ(json_array_size(tape_devices), 1U);
  json_t* tape_device = json_array_get(tape_devices, 0);
  ASSERT_NE(tape_device, nullptr);
  EXPECT_STREQ(
      json_string_value(json_object_get(tape_device, "device_identifier")),
      "naa.11223344");

  json_t* changers = json_object_get(parsed, "changers");
  ASSERT_TRUE(json_is_array(changers));
  ASSERT_EQ(json_array_size(changers), 1U);

  json_t* changer = json_array_get(changers, 0);
  ASSERT_NE(changer, nullptr);
  EXPECT_STREQ(json_string_value(json_object_get(changer, "device_identifier")),
               "naa.aabbccdd");
  json_t* drives = json_object_get(changer, "drives");
  ASSERT_TRUE(json_is_array(drives));
  ASSERT_EQ(json_array_size(drives), 1U);

  json_t* drive = json_array_get(drives, 0);
  ASSERT_NE(drive, nullptr);
  EXPECT_STREQ(json_string_value(json_object_get(drive, "tape_device_node")),
               "/dev/nst0");
  EXPECT_STREQ(json_string_value(json_object_get(drive, "generic_device_node")),
               "/dev/sg3");
  EXPECT_EQ(json_integer_value(json_object_get(drive, "drive_element_address")),
            256);
  EXPECT_STREQ(json_string_value(json_object_get(drive, "device_identifier")),
               "naa.1234");
  EXPECT_STREQ(json_string_value(json_object_get(drive, "serial")), "ABC123");
  EXPECT_STREQ(json_string_value(json_object_get(drive, "source")), "test");

  json_decref(parsed);
}

TEST(SdDiscoveryProbe, ProbesFilesystemCandidatesForCurrentHost)
{
  const auto report = storagedaemon::ProbeStorageDiscoveryReport();

  EXPECT_FALSE(report.hostname.empty());
  EXPECT_FALSE(report.fqdn.empty());
  EXPECT_FALSE(report.filesystems.empty());
}

#if defined(HAVE_LINUX_OS)
TEST(SdDiscoveryProbe, ProbesTapeAndChangerSerialsFromLinuxSysfs)
{
  ScopedDirectory sysfs_root;
  ScopedDirectory dev_root;

  WriteBinaryFile(sysfs_root.path() / "class/scsi_tape/nst0/device/vendor",
                  "IBM\n");
  WriteBinaryFile(sysfs_root.path() / "class/scsi_tape/nst0/device/model",
                  "ULTRIUM-HH8 \n");
  WriteBinaryFile(sysfs_root.path() / "class/scsi_tape/nst0/device/vpd_pg80",
                  BuildPage80("TAPE123"));
  WriteBinaryFile(sysfs_root.path() / "class/scsi_tape/nst0/device/vpd_pg83",
                  BuildPage83Naa("\x11\x22\x33\x44"));
  std::filesystem::create_directories(
      sysfs_root.path() / "class/scsi_tape/nst0/device/scsi_generic/sg3");
  WriteBinaryFile(dev_root.path() / "nst0", "");
  WriteBinaryFile(dev_root.path() / "sg3", "");

  WriteBinaryFile(sysfs_root.path() / "class/scsi_changer/sch0/device/vendor",
                  "IBM\n");
  WriteBinaryFile(sysfs_root.path() / "class/scsi_changer/sch0/device/model",
                  "3573-TL\n");
  WriteBinaryFile(sysfs_root.path() / "class/scsi_changer/sch0/device/vpd_pg80",
                  BuildPage80("CHANGER42"));
  WriteBinaryFile(sysfs_root.path() / "class/scsi_changer/sch0/device/vpd_pg83",
                  BuildPage83Naa("\xaa\xbb\xcc\xdd"));
  std::filesystem::create_directories(
      sysfs_root.path() / "class/scsi_changer/sch0/device/scsi_generic/sg4");
  WriteBinaryFile(dev_root.path() / "sg4", "");

  ScopedEnvironmentVariable sysfs_override{"BAREOS_SD_DISCOVERY_SYSFS_ROOT",
                                           sysfs_root.path().string()};
  ScopedEnvironmentVariable dev_override{"BAREOS_SD_DISCOVERY_DEV_ROOT",
                                         dev_root.path().string()};

  const auto report = storagedaemon::ProbeStorageDiscoveryReport();

  ASSERT_EQ(report.tape_devices.size(), 1U);
  EXPECT_EQ(report.tape_devices[0].device_node,
            (dev_root.path() / "nst0").string());
  EXPECT_EQ(report.tape_devices[0].generic_device_node,
            (dev_root.path() / "sg3").string());
  EXPECT_EQ(report.tape_devices[0].vendor, "IBM");
  EXPECT_EQ(report.tape_devices[0].model, "ULTRIUM-HH8");
  ASSERT_TRUE(report.tape_devices[0].device_identifier);
  EXPECT_EQ(*report.tape_devices[0].device_identifier, "naa.11223344");
  EXPECT_EQ(report.tape_devices[0].serial, "TAPE123");
  EXPECT_TRUE(report.tape_devices[0].accessible);

  ASSERT_EQ(report.changers.size(), 1U);
  EXPECT_EQ(report.changers[0].device_node, (dev_root.path() / "sg4").string());
  EXPECT_EQ(report.changers[0].vendor, "IBM");
  EXPECT_EQ(report.changers[0].model, "3573-TL");
  ASSERT_TRUE(report.changers[0].device_identifier);
  EXPECT_EQ(*report.changers[0].device_identifier, "naa.aabbccdd");
  EXPECT_EQ(report.changers[0].serial, "CHANGER42");
  EXPECT_TRUE(report.changers[0].accessible);
}
#endif
