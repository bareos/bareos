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

std::string BuildReadElementStatusDataTransferDescriptor(
    uint16_t element_address,
    std::string_view designator,
    uint8_t scsi_id = 0,
    uint8_t lun = 0,
    bool scsi_address_valid = false,
    uint8_t code_set = 0x01,
    uint8_t designator_type = 0x03)
{
  std::string descriptor(12, '\0');
  descriptor[0] = static_cast<char>((element_address >> 8) & 0xff);
  descriptor[1] = static_cast<char>(element_address & 0xff);
  descriptor[2] = static_cast<char>(0x09);
  descriptor[6]
      = static_cast<char>((lun & 0x07) | (scsi_address_valid ? 0x30 : 0x00));
  descriptor[7] = static_cast<char>(scsi_id);
  descriptor.append(1, static_cast<char>(code_set));
  descriptor.append(1, static_cast<char>(designator_type));
  descriptor.append(1, '\0');
  descriptor.append(1, static_cast<char>(designator.size()));
  descriptor.append(designator);
  return descriptor;
}

std::string BuildReadElementStatusData(
    std::initializer_list<std::string_view> descriptors)
{
  std::string page(8, '\0');
  page[0] = static_cast<char>(0x04);
  std::size_t descriptor_size = 0;
  std::size_t descriptor_length = 0;
  for (const auto descriptor : descriptors) {
    descriptor_size += descriptor.size();
    descriptor_length = descriptor.size();
    page.append(descriptor);
  }
  page[2] = static_cast<char>((descriptor_length >> 8) & 0xff);
  page[3] = static_cast<char>(descriptor_length & 0xff);
  page[5] = static_cast<char>((descriptor_size >> 16) & 0xff);
  page[6] = static_cast<char>((descriptor_size >> 8) & 0xff);
  page[7] = static_cast<char>(descriptor_size & 0xff);

  std::string payload(8, '\0');
  payload[2] = '\0';
  payload[3] = '\1';
  payload[5] = static_cast<char>((page.size() >> 16) & 0xff);
  payload[6] = static_cast<char>((page.size() >> 8) & 0xff);
  payload[7] = static_cast<char>(page.size() & 0xff);
  payload.append(page);
  return payload;
}

void CreateScsiClassDeviceLink(const std::filesystem::path& sysfs_root,
                               const std::filesystem::path& class_path,
                               std::string_view address)
{
  const auto scsi_device_path
      = sysfs_root / "devices/platform/mock" / std::string{address};
  std::filesystem::create_directories(scsi_device_path);
  std::filesystem::remove(class_path / "device");
  std::filesystem::create_symlink(scsi_device_path, class_path / "device");
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
  ScopedDirectory read_element_status_root;

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
  ScopedEnvironmentVariable read_element_status_override{
      "BAREOS_SD_DISCOVERY_READ_ELEMENT_STATUS_ROOT",
      read_element_status_root.path().string()};
  WriteBinaryFile(
      read_element_status_root.path() / "sg4.bin",
      BuildReadElementStatusData({BuildReadElementStatusDataTransferDescriptor(
          256, "\x11\x22\x33\x44")}));

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
  ASSERT_EQ(report.changers[0].drive_device_nodes.size(), 1U);
  EXPECT_EQ(report.changers[0].drive_device_nodes[0],
            (dev_root.path() / "nst0").string());
  ASSERT_EQ(report.changers[0].drives.size(), 1U);
  EXPECT_EQ(report.changers[0].drives[0].tape_device_node,
            (dev_root.path() / "nst0").string());
  EXPECT_EQ(report.changers[0].drives[0].generic_device_node,
            (dev_root.path() / "sg3").string());
  ASSERT_TRUE(report.changers[0].drives[0].drive_element_address);
  EXPECT_EQ(*report.changers[0].drives[0].drive_element_address, 256U);
  ASSERT_TRUE(report.changers[0].drives[0].device_identifier);
  EXPECT_EQ(*report.changers[0].drives[0].device_identifier, "naa.11223344");
  ASSERT_TRUE(report.changers[0].drives[0].serial);
  EXPECT_EQ(*report.changers[0].drives[0].serial, "TAPE123");
  ASSERT_TRUE(report.changers[0].drives[0].source);
  EXPECT_EQ(*report.changers[0].drives[0].source,
            "read_element_status:identifier");
}

TEST(SdDiscoveryProbe, FallsBackToScsiAddressWhenDriveIdentifierMissing)
{
  ScopedDirectory sysfs_root;
  ScopedDirectory dev_root;
  ScopedDirectory read_element_status_root;

  const auto tape_class = sysfs_root.path() / "class/scsi_tape/nst0";
  std::filesystem::create_directories(tape_class);
  CreateScsiClassDeviceLink(sysfs_root.path(), tape_class, "1:0:3:0");
  WriteBinaryFile(tape_class / "device/vendor", "IBM\n");
  WriteBinaryFile(tape_class / "device/model", "ULTRIUM-HH8 \n");
  WriteBinaryFile(tape_class / "device/vpd_pg80", BuildPage80("TAPE123"));
  std::filesystem::create_directories(tape_class / "device/scsi_generic/sg3");
  WriteBinaryFile(dev_root.path() / "nst0", "");
  WriteBinaryFile(dev_root.path() / "sg3", "");

  const auto changer_class = sysfs_root.path() / "class/scsi_changer/sch0";
  std::filesystem::create_directories(changer_class);
  CreateScsiClassDeviceLink(sysfs_root.path(), changer_class, "1:0:4:0");
  WriteBinaryFile(changer_class / "device/vendor", "IBM\n");
  WriteBinaryFile(changer_class / "device/model", "3573-TL\n");
  std::filesystem::create_directories(changer_class
                                      / "device/scsi_generic/sg4");
  WriteBinaryFile(dev_root.path() / "sg4", "");

  ScopedEnvironmentVariable sysfs_override{"BAREOS_SD_DISCOVERY_SYSFS_ROOT",
                                           sysfs_root.path().string()};
  ScopedEnvironmentVariable dev_override{"BAREOS_SD_DISCOVERY_DEV_ROOT",
                                         dev_root.path().string()};
  ScopedEnvironmentVariable read_element_status_override{
      "BAREOS_SD_DISCOVERY_READ_ELEMENT_STATUS_ROOT",
      read_element_status_root.path().string()};
  WriteBinaryFile(
      read_element_status_root.path() / "sg4.bin",
      BuildReadElementStatusData(
          {BuildReadElementStatusDataTransferDescriptor(256, "", 3, 0, true)}));

  const auto report = storagedaemon::ProbeStorageDiscoveryReport();

  ASSERT_EQ(report.changers.size(), 1U);
  ASSERT_EQ(report.changers[0].drives.size(), 1U);
  EXPECT_EQ(report.changers[0].drives[0].tape_device_node,
            (dev_root.path() / "nst0").string());
  EXPECT_EQ(report.changers[0].drives[0].generic_device_node,
            (dev_root.path() / "sg3").string());
  ASSERT_TRUE(report.changers[0].drives[0].serial);
  EXPECT_EQ(*report.changers[0].drives[0].serial, "TAPE123");
  ASSERT_TRUE(report.changers[0].drives[0].source);
  EXPECT_EQ(*report.changers[0].drives[0].source,
            "read_element_status:scsi_address");
  ASSERT_EQ(report.changers[0].drive_device_nodes.size(), 1U);
  EXPECT_EQ(report.changers[0].drive_device_nodes[0],
            (dev_root.path() / "nst0").string());
}

TEST(SdDiscoveryProbe, KeepsUnmatchedPartialChangerDrivesVisible)
{
  ScopedDirectory sysfs_root;
  ScopedDirectory dev_root;
  ScopedDirectory read_element_status_root;

  std::filesystem::create_directories(sysfs_root.path()
                                      / "class/scsi_changer/sch0/device");
  std::filesystem::create_directories(
      sysfs_root.path() / "class/scsi_changer/sch0/device/scsi_generic/sg4");
  WriteBinaryFile(dev_root.path() / "sg4", "");

  ScopedEnvironmentVariable sysfs_override{"BAREOS_SD_DISCOVERY_SYSFS_ROOT",
                                           sysfs_root.path().string()};
  ScopedEnvironmentVariable dev_override{"BAREOS_SD_DISCOVERY_DEV_ROOT",
                                         dev_root.path().string()};
  ScopedEnvironmentVariable read_element_status_override{
      "BAREOS_SD_DISCOVERY_READ_ELEMENT_STATUS_ROOT",
      read_element_status_root.path().string()};
  WriteBinaryFile(read_element_status_root.path() / "sg4.bin",
                  BuildReadElementStatusData({std::string(12, '\0')}));

  const auto report = storagedaemon::ProbeStorageDiscoveryReport();

  ASSERT_EQ(report.changers.size(), 1U);
  EXPECT_TRUE(report.changers[0].drive_device_nodes.empty());
  ASSERT_EQ(report.changers[0].drives.size(), 1U);
  EXPECT_TRUE(report.changers[0].drives[0].tape_device_node.empty());
  EXPECT_TRUE(report.changers[0].drives[0].generic_device_node.empty());
  ASSERT_TRUE(report.changers[0].drives[0].drive_element_address);
  EXPECT_EQ(*report.changers[0].drives[0].drive_element_address, 0U);
  ASSERT_TRUE(report.changers[0].drives[0].source);
  EXPECT_EQ(*report.changers[0].drives[0].source,
            "read_element_status:unmatched");
}

TEST(SdDiscoveryProbe, ExtractsSerialFromUnmatchedT10DriveIdentifier)
{
  ScopedDirectory sysfs_root;
  ScopedDirectory dev_root;
  ScopedDirectory read_element_status_root;

  std::filesystem::create_directories(sysfs_root.path()
                                      / "class/scsi_changer/sch0/device");
  std::filesystem::create_directories(
      sysfs_root.path() / "class/scsi_changer/sch0/device/scsi_generic/sg4");
  WriteBinaryFile(dev_root.path() / "sg4", "");

  ScopedEnvironmentVariable sysfs_override{"BAREOS_SD_DISCOVERY_SYSFS_ROOT",
                                           sysfs_root.path().string()};
  ScopedEnvironmentVariable dev_override{"BAREOS_SD_DISCOVERY_DEV_ROOT",
                                         dev_root.path().string()};
  ScopedEnvironmentVariable read_element_status_override{
      "BAREOS_SD_DISCOVERY_READ_ELEMENT_STATUS_ROOT",
      read_element_status_root.path().string()};
  WriteBinaryFile(
      read_element_status_root.path() / "sg4.bin",
      BuildReadElementStatusData({BuildReadElementStatusDataTransferDescriptor(
          256, "HPE     Ultrium 9-SCSI  4E77FE415F", 0, 0, false, 0x02,
          0x01)}));

  const auto report = storagedaemon::ProbeStorageDiscoveryReport();

  ASSERT_EQ(report.changers.size(), 1U);
  ASSERT_EQ(report.changers[0].drives.size(), 1U);
  ASSERT_TRUE(report.changers[0].drives[0].device_identifier);
  EXPECT_EQ(*report.changers[0].drives[0].device_identifier,
            "t10.HPE     Ultrium 9-SCSI  4E77FE415F");
  ASSERT_TRUE(report.changers[0].drives[0].serial);
  EXPECT_EQ(*report.changers[0].drives[0].serial, "4E77FE415F");
  ASSERT_TRUE(report.changers[0].drives[0].source);
  EXPECT_EQ(*report.changers[0].drives[0].source,
            "read_element_status:unmatched");
}

TEST(SdDiscoveryProbe, FallsBackToSerialWhenDriveIdentifierUsesT10)
{
  ScopedDirectory sysfs_root;
  ScopedDirectory dev_root;
  ScopedDirectory read_element_status_root;

  WriteBinaryFile(sysfs_root.path() / "class/scsi_tape/nst0/device/vendor",
                  "HPE\n");
  WriteBinaryFile(sysfs_root.path() / "class/scsi_tape/nst0/device/model",
                  "Ultrium 9-SCSI\n");
  WriteBinaryFile(sysfs_root.path() / "class/scsi_tape/nst0/device/vpd_pg80",
                  BuildPage80("4E77FE415F"));
  WriteBinaryFile(sysfs_root.path() / "class/scsi_tape/nst0/device/vpd_pg83",
                  BuildPage83Naa("\x20\x34\x45\x37\x37\x34\x31\x46"));
  std::filesystem::create_directories(
      sysfs_root.path() / "class/scsi_tape/nst0/device/scsi_generic/sg3");
  WriteBinaryFile(dev_root.path() / "nst0", "");
  WriteBinaryFile(dev_root.path() / "sg3", "");

  std::filesystem::create_directories(sysfs_root.path()
                                      / "class/scsi_changer/sch0/device");
  std::filesystem::create_directories(
      sysfs_root.path() / "class/scsi_changer/sch0/device/scsi_generic/sg4");
  WriteBinaryFile(dev_root.path() / "sg4", "");

  ScopedEnvironmentVariable sysfs_override{"BAREOS_SD_DISCOVERY_SYSFS_ROOT",
                                           sysfs_root.path().string()};
  ScopedEnvironmentVariable dev_override{"BAREOS_SD_DISCOVERY_DEV_ROOT",
                                         dev_root.path().string()};
  ScopedEnvironmentVariable read_element_status_override{
      "BAREOS_SD_DISCOVERY_READ_ELEMENT_STATUS_ROOT",
      read_element_status_root.path().string()};
  WriteBinaryFile(
      read_element_status_root.path() / "sg4.bin",
      BuildReadElementStatusData({BuildReadElementStatusDataTransferDescriptor(
          256, "HPE     Ultrium 9-SCSI  4E77FE415F", 0, 0, false, 0x02,
          0x01)}));

  const auto report = storagedaemon::ProbeStorageDiscoveryReport();

  ASSERT_EQ(report.changers.size(), 1U);
  ASSERT_EQ(report.changers[0].drive_device_nodes.size(), 1U);
  EXPECT_EQ(report.changers[0].drive_device_nodes[0],
            (dev_root.path() / "nst0").string());
  ASSERT_EQ(report.changers[0].drives.size(), 1U);
  EXPECT_EQ(report.changers[0].drives[0].tape_device_node,
            (dev_root.path() / "nst0").string());
  EXPECT_EQ(report.changers[0].drives[0].generic_device_node,
            (dev_root.path() / "sg3").string());
  ASSERT_TRUE(report.changers[0].drives[0].device_identifier);
  EXPECT_EQ(*report.changers[0].drives[0].device_identifier,
            "t10.HPE     Ultrium 9-SCSI  4E77FE415F");
  ASSERT_TRUE(report.changers[0].drives[0].serial);
  EXPECT_EQ(*report.changers[0].drives[0].serial, "4E77FE415F");
  ASSERT_TRUE(report.changers[0].drives[0].source);
  EXPECT_EQ(*report.changers[0].drives[0].source, "read_element_status:serial");
}

TEST(SdDiscoveryProbe, PrefersCanonicalNonRewindingTapeNodes)
{
  ScopedDirectory sysfs_root;
  ScopedDirectory dev_root;
  ScopedDirectory read_element_status_root;

  for (const auto* alias : {"st0", "nst0m", "nst0"}) {
    WriteBinaryFile(
        sysfs_root.path() / "class/scsi_tape" / alias / "device/vendor",
        "IBM\n");
    WriteBinaryFile(
        sysfs_root.path() / "class/scsi_tape" / alias / "device/model",
        "ULTRIUM-HH8 \n");
    WriteBinaryFile(
        sysfs_root.path() / "class/scsi_tape" / alias / "device/vpd_pg80",
        BuildPage80("TAPE123"));
    WriteBinaryFile(
        sysfs_root.path() / "class/scsi_tape" / alias / "device/vpd_pg83",
        BuildPage83Naa("\x11\x22\x33\x44"));
    std::filesystem::create_directories(sysfs_root.path() / "class/scsi_tape"
                                        / alias / "device/scsi_generic/sg3");
    WriteBinaryFile(dev_root.path() / alias, "");
  }
  WriteBinaryFile(dev_root.path() / "sg3", "");

  WriteBinaryFile(sysfs_root.path() / "class/scsi_changer/sch0/device/vendor",
                  "IBM\n");
  WriteBinaryFile(sysfs_root.path() / "class/scsi_changer/sch0/device/model",
                  "3573-TL\n");
  std::filesystem::create_directories(
      sysfs_root.path() / "class/scsi_changer/sch0/device/scsi_generic/sg4");
  WriteBinaryFile(dev_root.path() / "sg4", "");

  ScopedEnvironmentVariable sysfs_override{"BAREOS_SD_DISCOVERY_SYSFS_ROOT",
                                           sysfs_root.path().string()};
  ScopedEnvironmentVariable dev_override{"BAREOS_SD_DISCOVERY_DEV_ROOT",
                                         dev_root.path().string()};
  ScopedEnvironmentVariable read_element_status_override{
      "BAREOS_SD_DISCOVERY_READ_ELEMENT_STATUS_ROOT",
      read_element_status_root.path().string()};
  WriteBinaryFile(
      read_element_status_root.path() / "sg4.bin",
      BuildReadElementStatusData({BuildReadElementStatusDataTransferDescriptor(
          256, "\x11\x22\x33\x44")}));

  const auto report = storagedaemon::ProbeStorageDiscoveryReport();

  ASSERT_EQ(report.tape_devices.size(), 1U);
  EXPECT_EQ(report.tape_devices[0].device_node,
            (dev_root.path() / "nst0").string());
  EXPECT_EQ(report.tape_devices[0].generic_device_node,
            (dev_root.path() / "sg3").string());
  ASSERT_EQ(report.changers.size(), 1U);
  ASSERT_EQ(report.changers[0].drive_device_nodes.size(), 1U);
  EXPECT_EQ(report.changers[0].drive_device_nodes[0],
            (dev_root.path() / "nst0").string());
  ASSERT_EQ(report.changers[0].drives.size(), 1U);
  EXPECT_EQ(report.changers[0].drives[0].tape_device_node,
            (dev_root.path() / "nst0").string());
}
#endif
