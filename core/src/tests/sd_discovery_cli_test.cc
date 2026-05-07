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
#include <jansson.h>
#include <optional>

#include "include/bareos.h"
#include "stored/sd_discovery_probe.h"
#include "tools/sd_discovery_cli.h"

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
    const auto unique = "sd-discovery-cli-test-" + std::to_string(std::rand());
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

std::string ReadTextFile(const std::filesystem::path& path)
{
  std::ifstream stream(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(stream),
          std::istreambuf_iterator<char>()};
}

std::string QuoteShellArgument(std::string_view value)
{
#  if HAVE_WIN32
  return "\"" + std::string{value} + "\"";
#  else
  std::string quoted{"'"};
  for (char ch : value) {
    if (ch == '\'') {
      quoted += "'\"'\"'";
    } else {
      quoted.push_back(ch);
    }
  }
  quoted += "'";
  return quoted;
#  endif
}

int ExtractExitStatus(int result)
{
#  if HAVE_WIN32
  return result;
#  else
  if (result == -1) { return -1; }
  if (WIFEXITED(result)) { return WEXITSTATUS(result); }
  return -1;
#  endif
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

void CreateTapeByIdLink(const std::filesystem::path& dev_root,
                        std::string_view link_name,
                        std::string_view target_name)
{
  const auto link_path = dev_root / "tape" / "by-id" / std::string{link_name};
  std::filesystem::create_directories(link_path.parent_path());
  std::filesystem::remove(link_path);
  std::filesystem::create_symlink(dev_root / std::string{target_name},
                                  link_path);
}
#endif

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
  return report;
}

storagedaemon::StorageDiscoveryReport ExampleFallbackReport()
{
  auto report = ExampleReport();
  report.changers[0].drives = {{
                                   .tape_device_node = "/dev/nst0",
                                   .generic_device_node = "/dev/sg3",
                                   .drive_element_address = 256,
                                   .device_identifier = "naa.11223344",
                                   .serial = "ABC123",
                                   .source = "read_element_status:identifier",
                               },
                               {
                                   .drive_element_address = 257,
                                   .source = "read_element_status:unmatched",
                               }};
  return report;
}

storagedaemon::StorageDiscoveryReport ExampleByIdReport()
{
  auto report = ExampleReport();
  report.tape_devices[0].device_node = "/dev/tape/by-id/scsi-123456-nst";
  report.changers[0].drive_device_nodes = {"/dev/tape/by-id/scsi-123456-nst"};
  report.changers[0].drives[0].tape_device_node
      = "/dev/tape/by-id/scsi-123456-nst";
  report.changers[0].drives[0].source = "read_element_status:identifier";
  return report;
}

storagedaemon::StorageDiscoveryReport ExampleByIdSerialFallbackReport()
{
  auto report = ExampleByIdReport();
  report.tape_devices[0].vendor = "HPE";
  report.tape_devices[0].model = "Ultrium 9-SCSI";
  report.tape_devices[0].device_identifier = "naa.2034453737343146";
  report.tape_devices[0].serial = "4E77FE415F";
  report.changers[0].drives[0].device_identifier
      = "t10.HPE     Ultrium 9-SCSI  4E77FE415F";
  report.changers[0].drives[0].serial = "4E77FE415F";
  report.changers[0].drives[0].source = "read_element_status:serial";
  return report;
}

storagedaemon::StorageDiscoveryReport ExampleByIdScsiFallbackReport()
{
  auto report = ExampleByIdReport();
  report.changers[0].drives[0].device_identifier = "naa.55667788";
  report.changers[0].drives[0].source = "read_element_status:scsi_address";
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
      ExampleReport(),
      storagedaemon::discoverycli::ReportSection::kFilesystems);

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

TEST(SdDiscoveryCli, FiltersFullSectionWithByIdTapeNodes)
{
  auto filtered = storagedaemon::discoverycli::FilterDiscoveryReport(
      ExampleByIdReport(), storagedaemon::discoverycli::ReportSection::kFull);

  EXPECT_EQ(filtered.filesystems.size(), 1U);
  ASSERT_EQ(filtered.tape_devices.size(), 1U);
  EXPECT_EQ(filtered.tape_devices[0].device_node,
            "/dev/tape/by-id/scsi-123456-nst");
  ASSERT_EQ(filtered.changers.size(), 1U);
  ASSERT_EQ(filtered.changers[0].drive_device_nodes.size(), 1U);
  EXPECT_EQ(filtered.changers[0].drive_device_nodes[0],
            "/dev/tape/by-id/scsi-123456-nst");
  ASSERT_EQ(filtered.changers[0].drives.size(), 1U);
  EXPECT_EQ(filtered.changers[0].drives[0].tape_device_node,
            "/dev/tape/by-id/scsi-123456-nst");
}

TEST(SdDiscoveryCli, FiltersFullSectionWithByIdSerialFallback)
{
  auto filtered = storagedaemon::discoverycli::FilterDiscoveryReport(
      ExampleByIdSerialFallbackReport(),
      storagedaemon::discoverycli::ReportSection::kFull);

  EXPECT_EQ(filtered.filesystems.size(), 1U);
  ASSERT_EQ(filtered.tape_devices.size(), 1U);
  EXPECT_EQ(filtered.tape_devices[0].device_node,
            "/dev/tape/by-id/scsi-123456-nst");
  ASSERT_EQ(filtered.changers.size(), 1U);
  ASSERT_EQ(filtered.changers[0].drive_device_nodes.size(), 1U);
  EXPECT_EQ(filtered.changers[0].drive_device_nodes[0],
            "/dev/tape/by-id/scsi-123456-nst");
  ASSERT_EQ(filtered.changers[0].drives.size(), 1U);
  EXPECT_EQ(filtered.changers[0].drives[0].tape_device_node,
            "/dev/tape/by-id/scsi-123456-nst");
  ASSERT_TRUE(filtered.changers[0].drives[0].source);
  EXPECT_EQ(*filtered.changers[0].drives[0].source,
            "read_element_status:serial");
  ASSERT_TRUE(filtered.changers[0].drives[0].device_identifier);
  EXPECT_EQ(*filtered.changers[0].drives[0].device_identifier,
            "t10.HPE     Ultrium 9-SCSI  4E77FE415F");
}

TEST(SdDiscoveryCli, FiltersFullSectionWithByIdScsiFallback)
{
  auto filtered = storagedaemon::discoverycli::FilterDiscoveryReport(
      ExampleByIdScsiFallbackReport(),
      storagedaemon::discoverycli::ReportSection::kFull);

  EXPECT_EQ(filtered.filesystems.size(), 1U);
  ASSERT_EQ(filtered.tape_devices.size(), 1U);
  EXPECT_EQ(filtered.tape_devices[0].device_node,
            "/dev/tape/by-id/scsi-123456-nst");
  ASSERT_EQ(filtered.changers.size(), 1U);
  ASSERT_EQ(filtered.changers[0].drive_device_nodes.size(), 1U);
  EXPECT_EQ(filtered.changers[0].drive_device_nodes[0],
            "/dev/tape/by-id/scsi-123456-nst");
  ASSERT_EQ(filtered.changers[0].drives.size(), 1U);
  EXPECT_EQ(filtered.changers[0].drives[0].tape_device_node,
            "/dev/tape/by-id/scsi-123456-nst");
  ASSERT_TRUE(filtered.changers[0].drives[0].source);
  EXPECT_EQ(*filtered.changers[0].drives[0].source,
            "read_element_status:scsi_address");
  ASSERT_TRUE(filtered.changers[0].drives[0].device_identifier);
  EXPECT_EQ(*filtered.changers[0].drives[0].device_identifier, "naa.55667788");
}

TEST(SdDiscoveryCli, PreservesFallbackAndUnmatchedDriveMetadata)
{
  auto filtered = storagedaemon::discoverycli::FilterDiscoveryReport(
      ExampleFallbackReport(),
      storagedaemon::discoverycli::ReportSection::kTape);

  ASSERT_EQ(filtered.changers.size(), 1U);
  ASSERT_EQ(filtered.changers[0].drives.size(), 2U);
  EXPECT_EQ(filtered.changers[0].drives[0].tape_device_node, "/dev/nst0");
  ASSERT_TRUE(filtered.changers[0].drives[0].source);
  EXPECT_EQ(*filtered.changers[0].drives[0].source,
            "read_element_status:identifier");

  EXPECT_TRUE(filtered.changers[0].drives[1].tape_device_node.empty());
  EXPECT_TRUE(filtered.changers[0].drives[1].generic_device_node.empty());
  ASSERT_TRUE(filtered.changers[0].drives[1].drive_element_address);
  EXPECT_EQ(*filtered.changers[0].drives[1].drive_element_address, 257U);
  ASSERT_TRUE(filtered.changers[0].drives[1].source);
  EXPECT_EQ(*filtered.changers[0].drives[1].source,
            "read_element_status:unmatched");
}

TEST(SdDiscoveryCli, PreservesByIdTapeNodesInFilteredTapeSection)
{
  auto filtered = storagedaemon::discoverycli::FilterDiscoveryReport(
      ExampleByIdReport(), storagedaemon::discoverycli::ReportSection::kTape);

  ASSERT_EQ(filtered.tape_devices.size(), 1U);
  EXPECT_EQ(filtered.tape_devices[0].device_node,
            "/dev/tape/by-id/scsi-123456-nst");
  ASSERT_EQ(filtered.changers.size(), 1U);
  ASSERT_EQ(filtered.changers[0].drive_device_nodes.size(), 1U);
  EXPECT_EQ(filtered.changers[0].drive_device_nodes[0],
            "/dev/tape/by-id/scsi-123456-nst");
  ASSERT_EQ(filtered.changers[0].drives.size(), 1U);
  EXPECT_EQ(filtered.changers[0].drives[0].tape_device_node,
            "/dev/tape/by-id/scsi-123456-nst");
  ASSERT_TRUE(filtered.changers[0].drives[0].source);
  EXPECT_EQ(*filtered.changers[0].drives[0].source,
            "read_element_status:identifier");
}

TEST(SdDiscoveryCli, PreservesByIdSerialFallbackInFilteredTapeSection)
{
  auto filtered = storagedaemon::discoverycli::FilterDiscoveryReport(
      ExampleByIdSerialFallbackReport(),
      storagedaemon::discoverycli::ReportSection::kTape);

  ASSERT_EQ(filtered.tape_devices.size(), 1U);
  EXPECT_EQ(filtered.tape_devices[0].device_node,
            "/dev/tape/by-id/scsi-123456-nst");
  ASSERT_EQ(filtered.changers.size(), 1U);
  ASSERT_EQ(filtered.changers[0].drive_device_nodes.size(), 1U);
  EXPECT_EQ(filtered.changers[0].drive_device_nodes[0],
            "/dev/tape/by-id/scsi-123456-nst");
  ASSERT_EQ(filtered.changers[0].drives.size(), 1U);
  EXPECT_EQ(filtered.changers[0].drives[0].tape_device_node,
            "/dev/tape/by-id/scsi-123456-nst");
  ASSERT_TRUE(filtered.changers[0].drives[0].source);
  EXPECT_EQ(*filtered.changers[0].drives[0].source,
            "read_element_status:serial");
  ASSERT_TRUE(filtered.changers[0].drives[0].device_identifier);
  EXPECT_EQ(*filtered.changers[0].drives[0].device_identifier,
            "t10.HPE     Ultrium 9-SCSI  4E77FE415F");
}

TEST(SdDiscoveryCli, PreservesByIdScsiFallbackInFilteredTapeSection)
{
  auto filtered = storagedaemon::discoverycli::FilterDiscoveryReport(
      ExampleByIdScsiFallbackReport(),
      storagedaemon::discoverycli::ReportSection::kTape);

  ASSERT_EQ(filtered.tape_devices.size(), 1U);
  EXPECT_EQ(filtered.tape_devices[0].device_node,
            "/dev/tape/by-id/scsi-123456-nst");
  ASSERT_EQ(filtered.changers.size(), 1U);
  ASSERT_EQ(filtered.changers[0].drive_device_nodes.size(), 1U);
  EXPECT_EQ(filtered.changers[0].drive_device_nodes[0],
            "/dev/tape/by-id/scsi-123456-nst");
  ASSERT_EQ(filtered.changers[0].drives.size(), 1U);
  EXPECT_EQ(filtered.changers[0].drives[0].tape_device_node,
            "/dev/tape/by-id/scsi-123456-nst");
  ASSERT_TRUE(filtered.changers[0].drives[0].source);
  EXPECT_EQ(*filtered.changers[0].drives[0].source,
            "read_element_status:scsi_address");
  ASSERT_TRUE(filtered.changers[0].drives[0].device_identifier);
  EXPECT_EQ(*filtered.changers[0].drives[0].device_identifier, "naa.55667788");
}

TEST(SdDiscoveryCli, RendersFilteredJson)
{
  const auto json = storagedaemon::discoverycli::RenderDiscoveryReportJson(
      ExampleReport(), storagedaemon::discoverycli::ReportSection::kTape);
  json_error_t error{};
  json_t* parsed = json_loads(json.c_str(), 0, &error);

  ASSERT_NE(parsed, nullptr) << error.text;
  EXPECT_TRUE(json_is_array(json_object_get(parsed, "filesystems")));
  EXPECT_EQ(json_array_size(json_object_get(parsed, "filesystems")), 0U);
  EXPECT_EQ(json_array_size(json_object_get(parsed, "tape_devices")), 1U);
  EXPECT_EQ(json_array_size(json_object_get(parsed, "changers")), 1U);
  json_t* tape_device
      = json_array_get(json_object_get(parsed, "tape_devices"), 0);
  ASSERT_NE(tape_device, nullptr);
  EXPECT_STREQ(
      json_string_value(json_object_get(tape_device, "device_identifier")),
      "naa.11223344");
  json_t* changer = json_array_get(json_object_get(parsed, "changers"), 0);
  ASSERT_NE(changer, nullptr);
  EXPECT_STREQ(json_string_value(json_object_get(changer, "device_identifier")),
               "naa.aabbccdd");
  json_t* drives = json_object_get(changer, "drives");
  ASSERT_TRUE(json_is_array(drives));
  EXPECT_EQ(json_array_size(drives), 1U);
  json_t* drive = json_array_get(drives, 0);
  ASSERT_NE(drive, nullptr);
  EXPECT_STREQ(json_string_value(json_object_get(drive, "serial")), "ABC123");

  json_decref(parsed);
}

TEST(SdDiscoveryCli, RendersFullSectionWithByIdTapeNodesInJson)
{
  const auto json = storagedaemon::discoverycli::RenderDiscoveryReportJson(
      ExampleByIdReport(), storagedaemon::discoverycli::ReportSection::kFull);
  json_error_t error{};
  json_t* parsed = json_loads(json.c_str(), 0, &error);

  ASSERT_NE(parsed, nullptr) << error.text;
  EXPECT_EQ(json_array_size(json_object_get(parsed, "filesystems")), 1U);
  auto* tape_device
      = json_array_get(json_object_get(parsed, "tape_devices"), 0);
  ASSERT_NE(tape_device, nullptr);
  EXPECT_STREQ(json_string_value(json_object_get(tape_device, "device_node")),
               "/dev/tape/by-id/scsi-123456-nst");

  auto* changer = json_array_get(json_object_get(parsed, "changers"), 0);
  ASSERT_NE(changer, nullptr);
  auto* drive_device_nodes = json_object_get(changer, "drive_device_nodes");
  ASSERT_TRUE(json_is_array(drive_device_nodes));
  ASSERT_EQ(json_array_size(drive_device_nodes), 1U);
  EXPECT_STREQ(json_string_value(json_array_get(drive_device_nodes, 0)),
               "/dev/tape/by-id/scsi-123456-nst");

  auto* drives = json_object_get(changer, "drives");
  ASSERT_TRUE(json_is_array(drives));
  ASSERT_EQ(json_array_size(drives), 1U);
  auto* drive = json_array_get(drives, 0);
  ASSERT_NE(drive, nullptr);
  EXPECT_STREQ(json_string_value(json_object_get(drive, "tape_device_node")),
               "/dev/tape/by-id/scsi-123456-nst");
  EXPECT_STREQ(json_string_value(json_object_get(drive, "source")),
               "read_element_status:identifier");

  json_decref(parsed);
}

TEST(SdDiscoveryCli, RendersByIdTapeNodesInJson)
{
  const auto json = storagedaemon::discoverycli::RenderDiscoveryReportJson(
      ExampleByIdReport(), storagedaemon::discoverycli::ReportSection::kTape);
  json_error_t error{};
  json_t* parsed = json_loads(json.c_str(), 0, &error);

  ASSERT_NE(parsed, nullptr) << error.text;
  auto* tape_device
      = json_array_get(json_object_get(parsed, "tape_devices"), 0);
  ASSERT_NE(tape_device, nullptr);
  EXPECT_STREQ(json_string_value(json_object_get(tape_device, "device_node")),
               "/dev/tape/by-id/scsi-123456-nst");

  auto* changer = json_array_get(json_object_get(parsed, "changers"), 0);
  ASSERT_NE(changer, nullptr);
  auto* drive_device_nodes = json_object_get(changer, "drive_device_nodes");
  ASSERT_TRUE(json_is_array(drive_device_nodes));
  ASSERT_EQ(json_array_size(drive_device_nodes), 1U);
  EXPECT_STREQ(json_string_value(json_array_get(drive_device_nodes, 0)),
               "/dev/tape/by-id/scsi-123456-nst");

  auto* drives = json_object_get(changer, "drives");
  ASSERT_TRUE(json_is_array(drives));
  ASSERT_EQ(json_array_size(drives), 1U);
  auto* drive = json_array_get(drives, 0);
  ASSERT_NE(drive, nullptr);
  EXPECT_STREQ(json_string_value(json_object_get(drive, "tape_device_node")),
               "/dev/tape/by-id/scsi-123456-nst");
  EXPECT_STREQ(json_string_value(json_object_get(drive, "source")),
               "read_element_status:identifier");

  json_decref(parsed);
}

TEST(SdDiscoveryCli, RendersByIdSerialFallbackInJson)
{
  const auto json = storagedaemon::discoverycli::RenderDiscoveryReportJson(
      ExampleByIdSerialFallbackReport(),
      storagedaemon::discoverycli::ReportSection::kTape);
  json_error_t error{};
  json_t* parsed = json_loads(json.c_str(), 0, &error);

  ASSERT_NE(parsed, nullptr) << error.text;
  auto* tape_device
      = json_array_get(json_object_get(parsed, "tape_devices"), 0);
  ASSERT_NE(tape_device, nullptr);
  EXPECT_STREQ(json_string_value(json_object_get(tape_device, "device_node")),
               "/dev/tape/by-id/scsi-123456-nst");

  auto* changer = json_array_get(json_object_get(parsed, "changers"), 0);
  ASSERT_NE(changer, nullptr);
  auto* drive_device_nodes = json_object_get(changer, "drive_device_nodes");
  ASSERT_TRUE(json_is_array(drive_device_nodes));
  ASSERT_EQ(json_array_size(drive_device_nodes), 1U);
  EXPECT_STREQ(json_string_value(json_array_get(drive_device_nodes, 0)),
               "/dev/tape/by-id/scsi-123456-nst");

  auto* drives = json_object_get(changer, "drives");
  ASSERT_TRUE(json_is_array(drives));
  ASSERT_EQ(json_array_size(drives), 1U);
  auto* drive = json_array_get(drives, 0);
  ASSERT_NE(drive, nullptr);
  EXPECT_STREQ(json_string_value(json_object_get(drive, "tape_device_node")),
               "/dev/tape/by-id/scsi-123456-nst");
  EXPECT_STREQ(json_string_value(json_object_get(drive, "source")),
               "read_element_status:serial");
  EXPECT_STREQ(json_string_value(json_object_get(drive, "device_identifier")),
               "t10.HPE     Ultrium 9-SCSI  4E77FE415F");

  json_decref(parsed);
}

TEST(SdDiscoveryCli, RendersByIdScsiFallbackInJson)
{
  const auto json = storagedaemon::discoverycli::RenderDiscoveryReportJson(
      ExampleByIdScsiFallbackReport(),
      storagedaemon::discoverycli::ReportSection::kTape);
  json_error_t error{};
  json_t* parsed = json_loads(json.c_str(), 0, &error);

  ASSERT_NE(parsed, nullptr) << error.text;
  auto* tape_device
      = json_array_get(json_object_get(parsed, "tape_devices"), 0);
  ASSERT_NE(tape_device, nullptr);
  EXPECT_STREQ(json_string_value(json_object_get(tape_device, "device_node")),
               "/dev/tape/by-id/scsi-123456-nst");

  auto* changer = json_array_get(json_object_get(parsed, "changers"), 0);
  ASSERT_NE(changer, nullptr);
  auto* drive_device_nodes = json_object_get(changer, "drive_device_nodes");
  ASSERT_TRUE(json_is_array(drive_device_nodes));
  ASSERT_EQ(json_array_size(drive_device_nodes), 1U);
  EXPECT_STREQ(json_string_value(json_array_get(drive_device_nodes, 0)),
               "/dev/tape/by-id/scsi-123456-nst");

  auto* drives = json_object_get(changer, "drives");
  ASSERT_TRUE(json_is_array(drives));
  ASSERT_EQ(json_array_size(drives), 1U);
  auto* drive = json_array_get(drives, 0);
  ASSERT_NE(drive, nullptr);
  EXPECT_STREQ(json_string_value(json_object_get(drive, "tape_device_node")),
               "/dev/tape/by-id/scsi-123456-nst");
  EXPECT_STREQ(json_string_value(json_object_get(drive, "source")),
               "read_element_status:scsi_address");
  EXPECT_STREQ(json_string_value(json_object_get(drive, "device_identifier")),
               "naa.55667788");

  json_decref(parsed);
}

TEST(SdDiscoveryCli, RendersUnmatchedDriveMetadataInJson)
{
  const auto json = storagedaemon::discoverycli::RenderDiscoveryReportJson(
      ExampleFallbackReport(),
      storagedaemon::discoverycli::ReportSection::kTape);
  json_error_t error{};
  json_t* parsed = json_loads(json.c_str(), 0, &error);

  ASSERT_NE(parsed, nullptr) << error.text;
  json_t* changer = json_array_get(json_object_get(parsed, "changers"), 0);
  ASSERT_NE(changer, nullptr);
  json_t* drives = json_object_get(changer, "drives");
  ASSERT_TRUE(json_is_array(drives));
  ASSERT_EQ(json_array_size(drives), 2U);

  json_t* matched = json_array_get(drives, 0);
  ASSERT_NE(matched, nullptr);
  EXPECT_STREQ(json_string_value(json_object_get(matched, "source")),
               "read_element_status:identifier");

  json_t* unmatched = json_array_get(drives, 1);
  ASSERT_NE(unmatched, nullptr);
  EXPECT_STREQ(
      json_string_value(json_object_get(unmatched, "tape_device_node")), "");
  EXPECT_STREQ(
      json_string_value(json_object_get(unmatched, "generic_device_node")), "");
  EXPECT_EQ(
      json_integer_value(json_object_get(unmatched, "drive_element_address")),
      257);
  EXPECT_TRUE(json_is_null(json_object_get(unmatched, "device_identifier")));
  EXPECT_TRUE(json_is_null(json_object_get(unmatched, "serial")));
  EXPECT_STREQ(json_string_value(json_object_get(unmatched, "source")),
               "read_element_status:unmatched");

  json_decref(parsed);
}

#if defined(HAVE_LINUX_OS)
TEST(SdDiscoveryCli, InvokesExecutableWithTapeSection)
{
  ScopedDirectory sysfs_root;
  ScopedDirectory dev_root;
  ScopedDirectory read_element_status_root;
  ScopedDirectory output_root;

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
  WriteBinaryFile(
      read_element_status_root.path() / "sg4.bin",
      BuildReadElementStatusData({BuildReadElementStatusDataTransferDescriptor(
          256, "\x11\x22\x33\x44")}));

  ScopedEnvironmentVariable sysfs_override{"BAREOS_SD_DISCOVERY_SYSFS_ROOT",
                                           sysfs_root.path().string()};
  ScopedEnvironmentVariable dev_override{"BAREOS_SD_DISCOVERY_DEV_ROOT",
                                         dev_root.path().string()};
  ScopedEnvironmentVariable read_element_status_override{
      "BAREOS_SD_DISCOVERY_READ_ELEMENT_STATUS_ROOT",
      read_element_status_root.path().string()};

  const auto output_path = output_root.path() / "discovery.json";
  const auto command = QuoteShellArgument(BAREOS_SD_DISCOVER_BINARY)
                       + " --section tape > "
                       + QuoteShellArgument(output_path.string());
  EXPECT_EQ(ExtractExitStatus(std::system(command.c_str())), 0);

  json_error_t error{};
  json_t* parsed = json_loads(ReadTextFile(output_path).c_str(), 0, &error);
  ASSERT_NE(parsed, nullptr) << error.text;
  EXPECT_EQ(json_array_size(json_object_get(parsed, "filesystems")), 0U);
  EXPECT_EQ(json_array_size(json_object_get(parsed, "tape_devices")), 1U);
  EXPECT_EQ(json_array_size(json_object_get(parsed, "changers")), 1U);

  auto* tape_device
      = json_array_get(json_object_get(parsed, "tape_devices"), 0);
  ASSERT_NE(tape_device, nullptr);
  EXPECT_STREQ(
      json_string_value(json_object_get(tape_device, "device_identifier")),
      "naa.11223344");

  auto* changer = json_array_get(json_object_get(parsed, "changers"), 0);
  ASSERT_NE(changer, nullptr);
  auto* drives = json_object_get(changer, "drives");
  ASSERT_TRUE(json_is_array(drives));
  ASSERT_EQ(json_array_size(drives), 1U);
  auto* drive = json_array_get(drives, 0);
  ASSERT_NE(drive, nullptr);
  EXPECT_EQ(json_integer_value(json_object_get(drive, "drive_element_address")),
            256);
  EXPECT_STREQ(json_string_value(json_object_get(drive, "device_identifier")),
               "naa.11223344");
  EXPECT_STREQ(json_string_value(json_object_get(drive, "serial")), "TAPE123");
  EXPECT_STREQ(json_string_value(json_object_get(drive, "source")),
               "read_element_status:identifier");

  json_decref(parsed);
}

TEST(SdDiscoveryCli, InvokesExecutableWithShortTapeSectionOption)
{
  ScopedDirectory sysfs_root;
  ScopedDirectory dev_root;
  ScopedDirectory read_element_status_root;
  ScopedDirectory output_root;

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
  WriteBinaryFile(
      read_element_status_root.path() / "sg4.bin",
      BuildReadElementStatusData({BuildReadElementStatusDataTransferDescriptor(
          256, "\x11\x22\x33\x44")}));

  ScopedEnvironmentVariable sysfs_override{"BAREOS_SD_DISCOVERY_SYSFS_ROOT",
                                           sysfs_root.path().string()};
  ScopedEnvironmentVariable dev_override{"BAREOS_SD_DISCOVERY_DEV_ROOT",
                                         dev_root.path().string()};
  ScopedEnvironmentVariable read_element_status_override{
      "BAREOS_SD_DISCOVERY_READ_ELEMENT_STATUS_ROOT",
      read_element_status_root.path().string()};

  const auto output_path = output_root.path() / "discovery.json";
  const auto command = QuoteShellArgument(BAREOS_SD_DISCOVER_BINARY)
                       + " -s tape > "
                       + QuoteShellArgument(output_path.string());
  EXPECT_EQ(ExtractExitStatus(std::system(command.c_str())), 0);

  json_error_t error{};
  json_t* parsed = json_loads(ReadTextFile(output_path).c_str(), 0, &error);
  ASSERT_NE(parsed, nullptr) << error.text;
  EXPECT_EQ(json_array_size(json_object_get(parsed, "filesystems")), 0U);
  EXPECT_EQ(json_array_size(json_object_get(parsed, "tape_devices")), 1U);
  EXPECT_EQ(json_array_size(json_object_get(parsed, "changers")), 1U);

  auto* drives = json_object_get(
      json_array_get(json_object_get(parsed, "changers"), 0), "drives");
  ASSERT_TRUE(json_is_array(drives));
  ASSERT_EQ(json_array_size(drives), 1U);
  auto* drive = json_array_get(drives, 0);
  ASSERT_NE(drive, nullptr);
  EXPECT_STREQ(json_string_value(json_object_get(drive, "source")),
               "read_element_status:identifier");

  json_decref(parsed);
}

TEST(SdDiscoveryCli, InvokesExecutableWithScsiFallbackTapeSection)
{
  ScopedDirectory sysfs_root;
  ScopedDirectory dev_root;
  ScopedDirectory read_element_status_root;
  ScopedDirectory output_root;

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
  WriteBinaryFile(
      read_element_status_root.path() / "sg4.bin",
      BuildReadElementStatusData({BuildReadElementStatusDataTransferDescriptor(
          256, "\x55\x66\x77\x88", 3, 0, true)}));

  ScopedEnvironmentVariable sysfs_override{"BAREOS_SD_DISCOVERY_SYSFS_ROOT",
                                           sysfs_root.path().string()};
  ScopedEnvironmentVariable dev_override{"BAREOS_SD_DISCOVERY_DEV_ROOT",
                                         dev_root.path().string()};
  ScopedEnvironmentVariable read_element_status_override{
      "BAREOS_SD_DISCOVERY_READ_ELEMENT_STATUS_ROOT",
      read_element_status_root.path().string()};

  const auto output_path = output_root.path() / "discovery.json";
  const auto command = QuoteShellArgument(BAREOS_SD_DISCOVER_BINARY)
                       + " --section tape > "
                       + QuoteShellArgument(output_path.string());
  EXPECT_EQ(ExtractExitStatus(std::system(command.c_str())), 0);

  json_error_t error{};
  json_t* parsed = json_loads(ReadTextFile(output_path).c_str(), 0, &error);
  ASSERT_NE(parsed, nullptr) << error.text;
  EXPECT_EQ(json_array_size(json_object_get(parsed, "changers")), 1U);

  auto* changer = json_array_get(json_object_get(parsed, "changers"), 0);
  ASSERT_NE(changer, nullptr);
  auto* drive_device_nodes = json_object_get(changer, "drive_device_nodes");
  ASSERT_TRUE(json_is_array(drive_device_nodes));
  ASSERT_EQ(json_array_size(drive_device_nodes), 1U);
  EXPECT_STREQ(json_string_value(json_array_get(drive_device_nodes, 0)),
               (dev_root.path() / "nst0").string().c_str());

  auto* drives = json_object_get(changer, "drives");
  ASSERT_TRUE(json_is_array(drives));
  ASSERT_EQ(json_array_size(drives), 1U);
  auto* drive = json_array_get(drives, 0);
  ASSERT_NE(drive, nullptr);
  EXPECT_EQ(json_integer_value(json_object_get(drive, "drive_element_address")),
            256);
  EXPECT_STREQ(json_string_value(json_object_get(drive, "tape_device_node")),
               (dev_root.path() / "nst0").string().c_str());
  EXPECT_STREQ(json_string_value(json_object_get(drive, "generic_device_node")),
               (dev_root.path() / "sg3").string().c_str());
  EXPECT_STREQ(json_string_value(json_object_get(drive, "device_identifier")),
               "naa.55667788");
  EXPECT_STREQ(json_string_value(json_object_get(drive, "serial")), "TAPE123");
  EXPECT_STREQ(json_string_value(json_object_get(drive, "source")),
               "read_element_status:scsi_address");

  json_decref(parsed);
}

TEST(SdDiscoveryCli, InvokesExecutableWithScsiFallbackTapeSectionViaTapeById)
{
  ScopedDirectory sysfs_root;
  ScopedDirectory dev_root;
  ScopedDirectory read_element_status_root;
  ScopedDirectory output_root;

  const auto tape_class = sysfs_root.path() / "class/scsi_tape/nst0";
  std::filesystem::create_directories(tape_class);
  CreateScsiClassDeviceLink(sysfs_root.path(), tape_class, "1:0:3:0");
  WriteBinaryFile(tape_class / "device/vendor", "IBM\n");
  WriteBinaryFile(tape_class / "device/model", "ULTRIUM-HH8 \n");
  WriteBinaryFile(tape_class / "device/vpd_pg80", BuildPage80("TAPE123"));
  std::filesystem::create_directories(tape_class / "device/scsi_generic/sg3");
  WriteBinaryFile(dev_root.path() / "nst0", "");
  WriteBinaryFile(dev_root.path() / "sg3", "");
  CreateTapeByIdLink(dev_root.path(), "scsi-tape123-nst", "nst0");

  const auto changer_class = sysfs_root.path() / "class/scsi_changer/sch0";
  std::filesystem::create_directories(changer_class);
  CreateScsiClassDeviceLink(sysfs_root.path(), changer_class, "1:0:4:0");
  WriteBinaryFile(changer_class / "device/vendor", "IBM\n");
  WriteBinaryFile(changer_class / "device/model", "3573-TL\n");
  std::filesystem::create_directories(changer_class
                                      / "device/scsi_generic/sg4");
  WriteBinaryFile(dev_root.path() / "sg4", "");
  WriteBinaryFile(
      read_element_status_root.path() / "sg4.bin",
      BuildReadElementStatusData({BuildReadElementStatusDataTransferDescriptor(
          256, "\x55\x66\x77\x88", 3, 0, true)}));

  ScopedEnvironmentVariable sysfs_override{"BAREOS_SD_DISCOVERY_SYSFS_ROOT",
                                           sysfs_root.path().string()};
  ScopedEnvironmentVariable dev_override{"BAREOS_SD_DISCOVERY_DEV_ROOT",
                                         dev_root.path().string()};
  ScopedEnvironmentVariable read_element_status_override{
      "BAREOS_SD_DISCOVERY_READ_ELEMENT_STATUS_ROOT",
      read_element_status_root.path().string()};

  const auto output_path = output_root.path() / "discovery.json";
  const auto command = QuoteShellArgument(BAREOS_SD_DISCOVER_BINARY)
                       + " --section tape > "
                       + QuoteShellArgument(output_path.string());
  EXPECT_EQ(ExtractExitStatus(std::system(command.c_str())), 0);

  json_error_t error{};
  json_t* parsed = json_loads(ReadTextFile(output_path).c_str(), 0, &error);
  ASSERT_NE(parsed, nullptr) << error.text;
  EXPECT_EQ(json_array_size(json_object_get(parsed, "changers")), 1U);
  const auto expected_device_node
      = (dev_root.path() / "tape/by-id/scsi-tape123-nst").string();

  auto* changer = json_array_get(json_object_get(parsed, "changers"), 0);
  ASSERT_NE(changer, nullptr);
  auto* drive_device_nodes = json_object_get(changer, "drive_device_nodes");
  ASSERT_TRUE(json_is_array(drive_device_nodes));
  ASSERT_EQ(json_array_size(drive_device_nodes), 1U);
  EXPECT_STREQ(json_string_value(json_array_get(drive_device_nodes, 0)),
               expected_device_node.c_str());

  auto* drives = json_object_get(changer, "drives");
  ASSERT_TRUE(json_is_array(drives));
  ASSERT_EQ(json_array_size(drives), 1U);
  auto* drive = json_array_get(drives, 0);
  ASSERT_NE(drive, nullptr);
  EXPECT_EQ(json_integer_value(json_object_get(drive, "drive_element_address")),
            256);
  EXPECT_STREQ(json_string_value(json_object_get(drive, "tape_device_node")),
               expected_device_node.c_str());
  EXPECT_STREQ(json_string_value(json_object_get(drive, "generic_device_node")),
               (dev_root.path() / "sg3").string().c_str());
  EXPECT_STREQ(json_string_value(json_object_get(drive, "device_identifier")),
               "naa.55667788");
  EXPECT_STREQ(json_string_value(json_object_get(drive, "serial")), "TAPE123");
  EXPECT_STREQ(json_string_value(json_object_get(drive, "source")),
               "read_element_status:scsi_address");

  json_decref(parsed);
}

TEST(SdDiscoveryCli, InvokesExecutableWithUnmatchedTapeSection)
{
  ScopedDirectory sysfs_root;
  ScopedDirectory dev_root;
  ScopedDirectory read_element_status_root;
  ScopedDirectory output_root;

  WriteBinaryFile(sysfs_root.path() / "class/scsi_tape/nst0/device/vendor",
                  "IBM\n");
  WriteBinaryFile(sysfs_root.path() / "class/scsi_tape/nst0/device/model",
                  "ULTRIUM-HH8 \n");
  WriteBinaryFile(sysfs_root.path() / "class/scsi_tape/nst0/device/vpd_pg80",
                  BuildPage80("TAPE123"));
  std::filesystem::create_directories(
      sysfs_root.path() / "class/scsi_tape/nst0/device/scsi_generic/sg3");
  WriteBinaryFile(dev_root.path() / "nst0", "");
  WriteBinaryFile(dev_root.path() / "sg3", "");

  WriteBinaryFile(sysfs_root.path() / "class/scsi_changer/sch0/device/vendor",
                  "IBM\n");
  WriteBinaryFile(sysfs_root.path() / "class/scsi_changer/sch0/device/model",
                  "3573-TL\n");
  std::filesystem::create_directories(
      sysfs_root.path() / "class/scsi_changer/sch0/device/scsi_generic/sg4");
  WriteBinaryFile(dev_root.path() / "sg4", "");
  WriteBinaryFile(
      read_element_status_root.path() / "sg4.bin",
      BuildReadElementStatusData({BuildReadElementStatusDataTransferDescriptor(
          257, "\x55\x66\x77\x88")}));

  ScopedEnvironmentVariable sysfs_override{"BAREOS_SD_DISCOVERY_SYSFS_ROOT",
                                           sysfs_root.path().string()};
  ScopedEnvironmentVariable dev_override{"BAREOS_SD_DISCOVERY_DEV_ROOT",
                                         dev_root.path().string()};
  ScopedEnvironmentVariable read_element_status_override{
      "BAREOS_SD_DISCOVERY_READ_ELEMENT_STATUS_ROOT",
      read_element_status_root.path().string()};

  const auto output_path = output_root.path() / "discovery.json";
  const auto command = QuoteShellArgument(BAREOS_SD_DISCOVER_BINARY)
                       + " --section tape > "
                       + QuoteShellArgument(output_path.string());
  EXPECT_EQ(ExtractExitStatus(std::system(command.c_str())), 0);

  json_error_t error{};
  json_t* parsed = json_loads(ReadTextFile(output_path).c_str(), 0, &error);
  ASSERT_NE(parsed, nullptr) << error.text;
  EXPECT_EQ(json_array_size(json_object_get(parsed, "changers")), 1U);

  auto* changer = json_array_get(json_object_get(parsed, "changers"), 0);
  ASSERT_NE(changer, nullptr);
  auto* drive_device_nodes = json_object_get(changer, "drive_device_nodes");
  ASSERT_TRUE(json_is_array(drive_device_nodes));
  ASSERT_EQ(json_array_size(drive_device_nodes), 0U);

  auto* drives = json_object_get(changer, "drives");
  ASSERT_TRUE(json_is_array(drives));
  ASSERT_EQ(json_array_size(drives), 1U);
  auto* drive = json_array_get(drives, 0);
  ASSERT_NE(drive, nullptr);
  EXPECT_EQ(json_integer_value(json_object_get(drive, "drive_element_address")),
            257);
  EXPECT_STREQ(json_string_value(json_object_get(drive, "tape_device_node")),
               "");
  EXPECT_STREQ(json_string_value(json_object_get(drive, "generic_device_node")),
               "");
  EXPECT_STREQ(json_string_value(json_object_get(drive, "device_identifier")),
               "naa.55667788");
  EXPECT_TRUE(json_is_null(json_object_get(drive, "serial")));
  EXPECT_STREQ(json_string_value(json_object_get(drive, "source")),
               "read_element_status:unmatched");

  json_decref(parsed);
}

TEST(SdDiscoveryCli, ExtractsSerialFromUnmatchedT10TapeSection)
{
  ScopedDirectory sysfs_root;
  ScopedDirectory dev_root;
  ScopedDirectory read_element_status_root;
  ScopedDirectory output_root;

  WriteBinaryFile(sysfs_root.path() / "class/scsi_changer/sch0/device/vendor",
                  "IBM\n");
  WriteBinaryFile(sysfs_root.path() / "class/scsi_changer/sch0/device/model",
                  "3573-TL\n");
  std::filesystem::create_directories(
      sysfs_root.path() / "class/scsi_changer/sch0/device/scsi_generic/sg4");
  WriteBinaryFile(dev_root.path() / "sg4", "");
  WriteBinaryFile(
      read_element_status_root.path() / "sg4.bin",
      BuildReadElementStatusData({BuildReadElementStatusDataTransferDescriptor(
          256, "HPE     Ultrium 9-SCSI  4E77FE415F", 0, 0, false, 0x02,
          0x01)}));

  ScopedEnvironmentVariable sysfs_override{"BAREOS_SD_DISCOVERY_SYSFS_ROOT",
                                           sysfs_root.path().string()};
  ScopedEnvironmentVariable dev_override{"BAREOS_SD_DISCOVERY_DEV_ROOT",
                                         dev_root.path().string()};
  ScopedEnvironmentVariable read_element_status_override{
      "BAREOS_SD_DISCOVERY_READ_ELEMENT_STATUS_ROOT",
      read_element_status_root.path().string()};

  const auto output_path = output_root.path() / "discovery.json";
  const auto command = QuoteShellArgument(BAREOS_SD_DISCOVER_BINARY)
                       + " --section tape > "
                       + QuoteShellArgument(output_path.string());
  EXPECT_EQ(ExtractExitStatus(std::system(command.c_str())), 0);

  json_error_t error{};
  json_t* parsed = json_loads(ReadTextFile(output_path).c_str(), 0, &error);
  ASSERT_NE(parsed, nullptr) << error.text;
  auto* drives = json_object_get(
      json_array_get(json_object_get(parsed, "changers"), 0), "drives");
  ASSERT_TRUE(json_is_array(drives));
  ASSERT_EQ(json_array_size(drives), 1U);
  auto* drive = json_array_get(drives, 0);
  ASSERT_NE(drive, nullptr);
  EXPECT_STREQ(json_string_value(json_object_get(drive, "device_identifier")),
               "t10.HPE     Ultrium 9-SCSI  4E77FE415F");
  EXPECT_STREQ(json_string_value(json_object_get(drive, "serial")),
               "4E77FE415F");
  EXPECT_STREQ(json_string_value(json_object_get(drive, "source")),
               "read_element_status:unmatched");

  json_decref(parsed);
}

TEST(SdDiscoveryCli, MatchesT10TapeSectionBySerial)
{
  ScopedDirectory sysfs_root;
  ScopedDirectory dev_root;
  ScopedDirectory read_element_status_root;
  ScopedDirectory output_root;

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

  WriteBinaryFile(sysfs_root.path() / "class/scsi_changer/sch0/device/vendor",
                  "IBM\n");
  WriteBinaryFile(sysfs_root.path() / "class/scsi_changer/sch0/device/model",
                  "3573-TL\n");
  std::filesystem::create_directories(
      sysfs_root.path() / "class/scsi_changer/sch0/device/scsi_generic/sg4");
  WriteBinaryFile(dev_root.path() / "sg4", "");
  WriteBinaryFile(
      read_element_status_root.path() / "sg4.bin",
      BuildReadElementStatusData({BuildReadElementStatusDataTransferDescriptor(
          256, "HPE     Ultrium 9-SCSI  4E77FE415F", 0, 0, false, 0x02,
          0x01)}));

  ScopedEnvironmentVariable sysfs_override{"BAREOS_SD_DISCOVERY_SYSFS_ROOT",
                                           sysfs_root.path().string()};
  ScopedEnvironmentVariable dev_override{"BAREOS_SD_DISCOVERY_DEV_ROOT",
                                         dev_root.path().string()};
  ScopedEnvironmentVariable read_element_status_override{
      "BAREOS_SD_DISCOVERY_READ_ELEMENT_STATUS_ROOT",
      read_element_status_root.path().string()};

  const auto output_path = output_root.path() / "discovery.json";
  const auto command = QuoteShellArgument(BAREOS_SD_DISCOVER_BINARY)
                       + " --section tape > "
                       + QuoteShellArgument(output_path.string());
  EXPECT_EQ(ExtractExitStatus(std::system(command.c_str())), 0);

  json_error_t error{};
  json_t* parsed = json_loads(ReadTextFile(output_path).c_str(), 0, &error);
  ASSERT_NE(parsed, nullptr) << error.text;
  auto* changer = json_array_get(json_object_get(parsed, "changers"), 0);
  ASSERT_NE(changer, nullptr);
  auto* drive_device_nodes = json_object_get(changer, "drive_device_nodes");
  ASSERT_TRUE(json_is_array(drive_device_nodes));
  ASSERT_EQ(json_array_size(drive_device_nodes), 1U);
  EXPECT_STREQ(json_string_value(json_array_get(drive_device_nodes, 0)),
               (dev_root.path() / "nst0").string().c_str());

  auto* drives = json_object_get(changer, "drives");
  ASSERT_TRUE(json_is_array(drives));
  ASSERT_EQ(json_array_size(drives), 1U);
  auto* drive = json_array_get(drives, 0);
  ASSERT_NE(drive, nullptr);
  EXPECT_STREQ(json_string_value(json_object_get(drive, "tape_device_node")),
               (dev_root.path() / "nst0").string().c_str());
  EXPECT_STREQ(json_string_value(json_object_get(drive, "generic_device_node")),
               (dev_root.path() / "sg3").string().c_str());
  EXPECT_STREQ(json_string_value(json_object_get(drive, "device_identifier")),
               "t10.HPE     Ultrium 9-SCSI  4E77FE415F");
  EXPECT_STREQ(json_string_value(json_object_get(drive, "serial")),
               "4E77FE415F");
  EXPECT_STREQ(json_string_value(json_object_get(drive, "source")),
               "read_element_status:serial");

  json_decref(parsed);
}

TEST(SdDiscoveryCli, MatchesT10TapeSectionBySerialViaTapeById)
{
  ScopedDirectory sysfs_root;
  ScopedDirectory dev_root;
  ScopedDirectory read_element_status_root;
  ScopedDirectory output_root;

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
  CreateTapeByIdLink(dev_root.path(), "scsi-4e77fe415f", "nst0");

  WriteBinaryFile(sysfs_root.path() / "class/scsi_changer/sch0/device/vendor",
                  "IBM\n");
  WriteBinaryFile(sysfs_root.path() / "class/scsi_changer/sch0/device/model",
                  "3573-TL\n");
  std::filesystem::create_directories(
      sysfs_root.path() / "class/scsi_changer/sch0/device/scsi_generic/sg4");
  WriteBinaryFile(dev_root.path() / "sg4", "");
  WriteBinaryFile(
      read_element_status_root.path() / "sg4.bin",
      BuildReadElementStatusData({BuildReadElementStatusDataTransferDescriptor(
          256, "HPE     Ultrium 9-SCSI  4E77FE415F", 0, 0, false, 0x02,
          0x01)}));

  ScopedEnvironmentVariable sysfs_override{"BAREOS_SD_DISCOVERY_SYSFS_ROOT",
                                           sysfs_root.path().string()};
  ScopedEnvironmentVariable dev_override{"BAREOS_SD_DISCOVERY_DEV_ROOT",
                                         dev_root.path().string()};
  ScopedEnvironmentVariable read_element_status_override{
      "BAREOS_SD_DISCOVERY_READ_ELEMENT_STATUS_ROOT",
      read_element_status_root.path().string()};

  const auto output_path = output_root.path() / "discovery.json";
  const auto command = QuoteShellArgument(BAREOS_SD_DISCOVER_BINARY)
                       + " --section tape > "
                       + QuoteShellArgument(output_path.string());
  EXPECT_EQ(ExtractExitStatus(std::system(command.c_str())), 0);

  json_error_t error{};
  json_t* parsed = json_loads(ReadTextFile(output_path).c_str(), 0, &error);
  ASSERT_NE(parsed, nullptr) << error.text;
  const auto expected_device_node
      = (dev_root.path() / "tape/by-id/scsi-4e77fe415f").string();

  auto* changer = json_array_get(json_object_get(parsed, "changers"), 0);
  ASSERT_NE(changer, nullptr);
  auto* drive_device_nodes = json_object_get(changer, "drive_device_nodes");
  ASSERT_TRUE(json_is_array(drive_device_nodes));
  ASSERT_EQ(json_array_size(drive_device_nodes), 1U);
  EXPECT_STREQ(json_string_value(json_array_get(drive_device_nodes, 0)),
               expected_device_node.c_str());

  auto* drives = json_object_get(changer, "drives");
  ASSERT_TRUE(json_is_array(drives));
  ASSERT_EQ(json_array_size(drives), 1U);
  auto* drive = json_array_get(drives, 0);
  ASSERT_NE(drive, nullptr);
  EXPECT_STREQ(json_string_value(json_object_get(drive, "tape_device_node")),
               expected_device_node.c_str());
  EXPECT_STREQ(json_string_value(json_object_get(drive, "generic_device_node")),
               (dev_root.path() / "sg3").string().c_str());
  EXPECT_STREQ(json_string_value(json_object_get(drive, "device_identifier")),
               "t10.HPE     Ultrium 9-SCSI  4E77FE415F");
  EXPECT_STREQ(json_string_value(json_object_get(drive, "serial")),
               "4E77FE415F");
  EXPECT_STREQ(json_string_value(json_object_get(drive, "source")),
               "read_element_status:serial");

  json_decref(parsed);
}

TEST(SdDiscoveryCli, PrefersCanonicalNonRewindingTapeNodes)
{
  ScopedDirectory sysfs_root;
  ScopedDirectory dev_root;
  ScopedDirectory read_element_status_root;
  ScopedDirectory output_root;

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
  WriteBinaryFile(
      read_element_status_root.path() / "sg4.bin",
      BuildReadElementStatusData({BuildReadElementStatusDataTransferDescriptor(
          256, "\x11\x22\x33\x44")}));

  ScopedEnvironmentVariable sysfs_override{"BAREOS_SD_DISCOVERY_SYSFS_ROOT",
                                           sysfs_root.path().string()};
  ScopedEnvironmentVariable dev_override{"BAREOS_SD_DISCOVERY_DEV_ROOT",
                                         dev_root.path().string()};
  ScopedEnvironmentVariable read_element_status_override{
      "BAREOS_SD_DISCOVERY_READ_ELEMENT_STATUS_ROOT",
      read_element_status_root.path().string()};

  const auto output_path = output_root.path() / "discovery.json";
  const auto command = QuoteShellArgument(BAREOS_SD_DISCOVER_BINARY)
                       + " --section tape > "
                       + QuoteShellArgument(output_path.string());
  EXPECT_EQ(ExtractExitStatus(std::system(command.c_str())), 0);

  json_error_t error{};
  json_t* parsed = json_loads(ReadTextFile(output_path).c_str(), 0, &error);
  ASSERT_NE(parsed, nullptr) << error.text;
  auto* tape_devices = json_object_get(parsed, "tape_devices");
  ASSERT_TRUE(json_is_array(tape_devices));
  ASSERT_EQ(json_array_size(tape_devices), 1U);
  auto* tape_device = json_array_get(tape_devices, 0);
  ASSERT_NE(tape_device, nullptr);
  EXPECT_STREQ(json_string_value(json_object_get(tape_device, "device_node")),
               (dev_root.path() / "nst0").string().c_str());

  auto* changer = json_array_get(json_object_get(parsed, "changers"), 0);
  ASSERT_NE(changer, nullptr);
  auto* drive_device_nodes = json_object_get(changer, "drive_device_nodes");
  ASSERT_TRUE(json_is_array(drive_device_nodes));
  ASSERT_EQ(json_array_size(drive_device_nodes), 1U);
  EXPECT_STREQ(json_string_value(json_array_get(drive_device_nodes, 0)),
               (dev_root.path() / "nst0").string().c_str());
  auto* drives = json_object_get(changer, "drives");
  ASSERT_TRUE(json_is_array(drives));
  ASSERT_EQ(json_array_size(drives), 1U);
  auto* drive = json_array_get(drives, 0);
  ASSERT_NE(drive, nullptr);
  EXPECT_STREQ(json_string_value(json_object_get(drive, "tape_device_node")),
               (dev_root.path() / "nst0").string().c_str());

  json_decref(parsed);
}

TEST(SdDiscoveryCli, PrefersTapeByIdLinksWhenAvailable)
{
  ScopedDirectory sysfs_root;
  ScopedDirectory dev_root;
  ScopedDirectory read_element_status_root;
  ScopedDirectory output_root;

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
  CreateTapeByIdLink(dev_root.path(), "scsi-123456", "st0");
  CreateTapeByIdLink(dev_root.path(), "scsi-123456-nst", "nst0");

  WriteBinaryFile(sysfs_root.path() / "class/scsi_changer/sch0/device/vendor",
                  "IBM\n");
  WriteBinaryFile(sysfs_root.path() / "class/scsi_changer/sch0/device/model",
                  "3573-TL\n");
  std::filesystem::create_directories(
      sysfs_root.path() / "class/scsi_changer/sch0/device/scsi_generic/sg4");
  WriteBinaryFile(dev_root.path() / "sg4", "");
  WriteBinaryFile(
      read_element_status_root.path() / "sg4.bin",
      BuildReadElementStatusData({BuildReadElementStatusDataTransferDescriptor(
          256, "\x11\x22\x33\x44")}));

  ScopedEnvironmentVariable sysfs_override{"BAREOS_SD_DISCOVERY_SYSFS_ROOT",
                                           sysfs_root.path().string()};
  ScopedEnvironmentVariable dev_override{"BAREOS_SD_DISCOVERY_DEV_ROOT",
                                         dev_root.path().string()};
  ScopedEnvironmentVariable read_element_status_override{
      "BAREOS_SD_DISCOVERY_READ_ELEMENT_STATUS_ROOT",
      read_element_status_root.path().string()};

  const auto output_path = output_root.path() / "discovery.json";
  const auto command = QuoteShellArgument(BAREOS_SD_DISCOVER_BINARY)
                       + " --section tape > "
                       + QuoteShellArgument(output_path.string());
  EXPECT_EQ(ExtractExitStatus(std::system(command.c_str())), 0);

  json_error_t error{};
  json_t* parsed = json_loads(ReadTextFile(output_path).c_str(), 0, &error);
  ASSERT_NE(parsed, nullptr) << error.text;
  const auto expected_device_node
      = (dev_root.path() / "tape/by-id/scsi-123456-nst").string();

  auto* tape_devices = json_object_get(parsed, "tape_devices");
  ASSERT_TRUE(json_is_array(tape_devices));
  ASSERT_EQ(json_array_size(tape_devices), 1U);
  auto* tape_device = json_array_get(tape_devices, 0);
  ASSERT_NE(tape_device, nullptr);
  EXPECT_STREQ(json_string_value(json_object_get(tape_device, "device_node")),
               expected_device_node.c_str());

  auto* changer = json_array_get(json_object_get(parsed, "changers"), 0);
  ASSERT_NE(changer, nullptr);
  auto* drive_device_nodes = json_object_get(changer, "drive_device_nodes");
  ASSERT_TRUE(json_is_array(drive_device_nodes));
  ASSERT_EQ(json_array_size(drive_device_nodes), 1U);
  EXPECT_STREQ(json_string_value(json_array_get(drive_device_nodes, 0)),
               expected_device_node.c_str());
  auto* drives = json_object_get(changer, "drives");
  ASSERT_TRUE(json_is_array(drives));
  ASSERT_EQ(json_array_size(drives), 1U);
  auto* drive = json_array_get(drives, 0);
  ASSERT_NE(drive, nullptr);
  EXPECT_STREQ(json_string_value(json_object_get(drive, "tape_device_node")),
               expected_device_node.c_str());

  json_decref(parsed);
}

TEST(SdDiscoveryCli, InvokesExecutableWithFilesystemSection)
{
  ScopedDirectory sysfs_root;
  ScopedDirectory dev_root;
  ScopedDirectory read_element_status_root;
  ScopedDirectory output_root;

  ScopedEnvironmentVariable sysfs_override{"BAREOS_SD_DISCOVERY_SYSFS_ROOT",
                                           sysfs_root.path().string()};
  ScopedEnvironmentVariable dev_override{"BAREOS_SD_DISCOVERY_DEV_ROOT",
                                         dev_root.path().string()};
  ScopedEnvironmentVariable read_element_status_override{
      "BAREOS_SD_DISCOVERY_READ_ELEMENT_STATUS_ROOT",
      read_element_status_root.path().string()};

  const auto output_path = output_root.path() / "discovery.json";
  const auto command = QuoteShellArgument(BAREOS_SD_DISCOVER_BINARY)
                       + " --section filesystems > "
                       + QuoteShellArgument(output_path.string());
  EXPECT_EQ(ExtractExitStatus(std::system(command.c_str())), 0);

  json_error_t error{};
  json_t* parsed = json_loads(ReadTextFile(output_path).c_str(), 0, &error);
  ASSERT_NE(parsed, nullptr) << error.text;
  auto* filesystems = json_object_get(parsed, "filesystems");
  ASSERT_TRUE(json_is_array(filesystems));
  EXPECT_GT(json_array_size(filesystems), 0U);
  EXPECT_EQ(json_array_size(json_object_get(parsed, "tape_devices")), 0U);
  EXPECT_EQ(json_array_size(json_object_get(parsed, "changers")), 0U);

  json_decref(parsed);
}

TEST(SdDiscoveryCli, InvokesExecutableWithFullSection)
{
  ScopedDirectory sysfs_root;
  ScopedDirectory dev_root;
  ScopedDirectory read_element_status_root;
  ScopedDirectory output_root;

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
  WriteBinaryFile(
      read_element_status_root.path() / "sg4.bin",
      BuildReadElementStatusData({BuildReadElementStatusDataTransferDescriptor(
          256, "\x11\x22\x33\x44")}));

  ScopedEnvironmentVariable sysfs_override{"BAREOS_SD_DISCOVERY_SYSFS_ROOT",
                                           sysfs_root.path().string()};
  ScopedEnvironmentVariable dev_override{"BAREOS_SD_DISCOVERY_DEV_ROOT",
                                         dev_root.path().string()};
  ScopedEnvironmentVariable read_element_status_override{
      "BAREOS_SD_DISCOVERY_READ_ELEMENT_STATUS_ROOT",
      read_element_status_root.path().string()};

  const auto output_path = output_root.path() / "discovery.json";
  const auto command = QuoteShellArgument(BAREOS_SD_DISCOVER_BINARY)
                       + " --section full > "
                       + QuoteShellArgument(output_path.string());
  EXPECT_EQ(ExtractExitStatus(std::system(command.c_str())), 0);

  json_error_t error{};
  json_t* parsed = json_loads(ReadTextFile(output_path).c_str(), 0, &error);
  ASSERT_NE(parsed, nullptr) << error.text;
  auto* filesystems = json_object_get(parsed, "filesystems");
  ASSERT_TRUE(json_is_array(filesystems));
  EXPECT_GT(json_array_size(filesystems), 0U);
  EXPECT_EQ(json_array_size(json_object_get(parsed, "tape_devices")), 1U);
  EXPECT_EQ(json_array_size(json_object_get(parsed, "changers")), 1U);

  auto* tape_device
      = json_array_get(json_object_get(parsed, "tape_devices"), 0);
  ASSERT_NE(tape_device, nullptr);
  EXPECT_STREQ(
      json_string_value(json_object_get(tape_device, "device_identifier")),
      "naa.11223344");

  auto* changer = json_array_get(json_object_get(parsed, "changers"), 0);
  ASSERT_NE(changer, nullptr);
  auto* drives = json_object_get(changer, "drives");
  ASSERT_TRUE(json_is_array(drives));
  ASSERT_EQ(json_array_size(drives), 1U);
  auto* drive = json_array_get(drives, 0);
  ASSERT_NE(drive, nullptr);
  EXPECT_STREQ(json_string_value(json_object_get(drive, "source")),
               "read_element_status:identifier");

  json_decref(parsed);
}

TEST(SdDiscoveryCli, InvokesExecutableWithFullSectionViaScsiFallback)
{
  ScopedDirectory sysfs_root;
  ScopedDirectory dev_root;
  ScopedDirectory read_element_status_root;
  ScopedDirectory output_root;

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
  WriteBinaryFile(
      read_element_status_root.path() / "sg4.bin",
      BuildReadElementStatusData({BuildReadElementStatusDataTransferDescriptor(
          256, "\x55\x66\x77\x88", 3, 0, true)}));

  ScopedEnvironmentVariable sysfs_override{"BAREOS_SD_DISCOVERY_SYSFS_ROOT",
                                           sysfs_root.path().string()};
  ScopedEnvironmentVariable dev_override{"BAREOS_SD_DISCOVERY_DEV_ROOT",
                                         dev_root.path().string()};
  ScopedEnvironmentVariable read_element_status_override{
      "BAREOS_SD_DISCOVERY_READ_ELEMENT_STATUS_ROOT",
      read_element_status_root.path().string()};

  const auto output_path = output_root.path() / "discovery.json";
  const auto command = QuoteShellArgument(BAREOS_SD_DISCOVER_BINARY)
                       + " --section full > "
                       + QuoteShellArgument(output_path.string());
  EXPECT_EQ(ExtractExitStatus(std::system(command.c_str())), 0);

  json_error_t error{};
  json_t* parsed = json_loads(ReadTextFile(output_path).c_str(), 0, &error);
  ASSERT_NE(parsed, nullptr) << error.text;
  auto* filesystems = json_object_get(parsed, "filesystems");
  ASSERT_TRUE(json_is_array(filesystems));
  EXPECT_GT(json_array_size(filesystems), 0U);
  EXPECT_EQ(json_array_size(json_object_get(parsed, "tape_devices")), 1U);
  EXPECT_EQ(json_array_size(json_object_get(parsed, "changers")), 1U);

  auto* changer = json_array_get(json_object_get(parsed, "changers"), 0);
  ASSERT_NE(changer, nullptr);
  auto* drive_device_nodes = json_object_get(changer, "drive_device_nodes");
  ASSERT_TRUE(json_is_array(drive_device_nodes));
  ASSERT_EQ(json_array_size(drive_device_nodes), 1U);
  EXPECT_STREQ(json_string_value(json_array_get(drive_device_nodes, 0)),
               (dev_root.path() / "nst0").string().c_str());

  auto* drives = json_object_get(changer, "drives");
  ASSERT_TRUE(json_is_array(drives));
  ASSERT_EQ(json_array_size(drives), 1U);
  auto* drive = json_array_get(drives, 0);
  ASSERT_NE(drive, nullptr);
  EXPECT_STREQ(json_string_value(json_object_get(drive, "tape_device_node")),
               (dev_root.path() / "nst0").string().c_str());
  EXPECT_STREQ(json_string_value(json_object_get(drive, "generic_device_node")),
               (dev_root.path() / "sg3").string().c_str());
  EXPECT_STREQ(json_string_value(json_object_get(drive, "serial")), "TAPE123");
  EXPECT_STREQ(json_string_value(json_object_get(drive, "source")),
               "read_element_status:scsi_address");

  json_decref(parsed);
}

TEST(SdDiscoveryCli, InvokesExecutableWithFullSectionViaScsiFallbackAndTapeById)
{
  ScopedDirectory sysfs_root;
  ScopedDirectory dev_root;
  ScopedDirectory read_element_status_root;
  ScopedDirectory output_root;

  const auto tape_class = sysfs_root.path() / "class/scsi_tape/nst0";
  std::filesystem::create_directories(tape_class);
  CreateScsiClassDeviceLink(sysfs_root.path(), tape_class, "1:0:3:0");
  WriteBinaryFile(tape_class / "device/vendor", "IBM\n");
  WriteBinaryFile(tape_class / "device/model", "ULTRIUM-HH8 \n");
  WriteBinaryFile(tape_class / "device/vpd_pg80", BuildPage80("TAPE123"));
  std::filesystem::create_directories(tape_class / "device/scsi_generic/sg3");
  WriteBinaryFile(dev_root.path() / "nst0", "");
  WriteBinaryFile(dev_root.path() / "sg3", "");
  CreateTapeByIdLink(dev_root.path(), "scsi-tape123-nst", "nst0");

  const auto changer_class = sysfs_root.path() / "class/scsi_changer/sch0";
  std::filesystem::create_directories(changer_class);
  CreateScsiClassDeviceLink(sysfs_root.path(), changer_class, "1:0:4:0");
  WriteBinaryFile(changer_class / "device/vendor", "IBM\n");
  WriteBinaryFile(changer_class / "device/model", "3573-TL\n");
  std::filesystem::create_directories(changer_class
                                      / "device/scsi_generic/sg4");
  WriteBinaryFile(dev_root.path() / "sg4", "");
  WriteBinaryFile(
      read_element_status_root.path() / "sg4.bin",
      BuildReadElementStatusData({BuildReadElementStatusDataTransferDescriptor(
          256, "\x55\x66\x77\x88", 3, 0, true)}));

  ScopedEnvironmentVariable sysfs_override{"BAREOS_SD_DISCOVERY_SYSFS_ROOT",
                                           sysfs_root.path().string()};
  ScopedEnvironmentVariable dev_override{"BAREOS_SD_DISCOVERY_DEV_ROOT",
                                         dev_root.path().string()};
  ScopedEnvironmentVariable read_element_status_override{
      "BAREOS_SD_DISCOVERY_READ_ELEMENT_STATUS_ROOT",
      read_element_status_root.path().string()};

  const auto output_path = output_root.path() / "discovery.json";
  const auto command = QuoteShellArgument(BAREOS_SD_DISCOVER_BINARY)
                       + " --section full > "
                       + QuoteShellArgument(output_path.string());
  EXPECT_EQ(ExtractExitStatus(std::system(command.c_str())), 0);

  json_error_t error{};
  json_t* parsed = json_loads(ReadTextFile(output_path).c_str(), 0, &error);
  ASSERT_NE(parsed, nullptr) << error.text;
  auto* filesystems = json_object_get(parsed, "filesystems");
  ASSERT_TRUE(json_is_array(filesystems));
  EXPECT_GT(json_array_size(filesystems), 0U);
  EXPECT_EQ(json_array_size(json_object_get(parsed, "tape_devices")), 1U);
  EXPECT_EQ(json_array_size(json_object_get(parsed, "changers")), 1U);

  const auto expected_device_node
      = (dev_root.path() / "tape/by-id/scsi-tape123-nst").string();

  auto* changer = json_array_get(json_object_get(parsed, "changers"), 0);
  ASSERT_NE(changer, nullptr);
  auto* drive_device_nodes = json_object_get(changer, "drive_device_nodes");
  ASSERT_TRUE(json_is_array(drive_device_nodes));
  ASSERT_EQ(json_array_size(drive_device_nodes), 1U);
  EXPECT_STREQ(json_string_value(json_array_get(drive_device_nodes, 0)),
               expected_device_node.c_str());

  auto* drives = json_object_get(changer, "drives");
  ASSERT_TRUE(json_is_array(drives));
  ASSERT_EQ(json_array_size(drives), 1U);
  auto* drive = json_array_get(drives, 0);
  ASSERT_NE(drive, nullptr);
  EXPECT_STREQ(json_string_value(json_object_get(drive, "tape_device_node")),
               expected_device_node.c_str());
  EXPECT_STREQ(json_string_value(json_object_get(drive, "generic_device_node")),
               (dev_root.path() / "sg3").string().c_str());
  EXPECT_STREQ(json_string_value(json_object_get(drive, "serial")), "TAPE123");
  EXPECT_STREQ(json_string_value(json_object_get(drive, "source")),
               "read_element_status:scsi_address");

  json_decref(parsed);
}

TEST(SdDiscoveryCli, InvokesExecutableWithFullSectionViaTapeById)
{
  ScopedDirectory sysfs_root;
  ScopedDirectory dev_root;
  ScopedDirectory read_element_status_root;
  ScopedDirectory output_root;

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
  CreateTapeByIdLink(dev_root.path(), "scsi-123456", "st0");
  CreateTapeByIdLink(dev_root.path(), "scsi-123456-nst", "nst0");

  WriteBinaryFile(sysfs_root.path() / "class/scsi_changer/sch0/device/vendor",
                  "IBM\n");
  WriteBinaryFile(sysfs_root.path() / "class/scsi_changer/sch0/device/model",
                  "3573-TL\n");
  std::filesystem::create_directories(
      sysfs_root.path() / "class/scsi_changer/sch0/device/scsi_generic/sg4");
  WriteBinaryFile(dev_root.path() / "sg4", "");
  WriteBinaryFile(
      read_element_status_root.path() / "sg4.bin",
      BuildReadElementStatusData({BuildReadElementStatusDataTransferDescriptor(
          256, "\x11\x22\x33\x44")}));

  ScopedEnvironmentVariable sysfs_override{"BAREOS_SD_DISCOVERY_SYSFS_ROOT",
                                           sysfs_root.path().string()};
  ScopedEnvironmentVariable dev_override{"BAREOS_SD_DISCOVERY_DEV_ROOT",
                                         dev_root.path().string()};
  ScopedEnvironmentVariable read_element_status_override{
      "BAREOS_SD_DISCOVERY_READ_ELEMENT_STATUS_ROOT",
      read_element_status_root.path().string()};

  const auto output_path = output_root.path() / "discovery.json";
  const auto command = QuoteShellArgument(BAREOS_SD_DISCOVER_BINARY)
                       + " --section full > "
                       + QuoteShellArgument(output_path.string());
  EXPECT_EQ(ExtractExitStatus(std::system(command.c_str())), 0);

  json_error_t error{};
  json_t* parsed = json_loads(ReadTextFile(output_path).c_str(), 0, &error);
  ASSERT_NE(parsed, nullptr) << error.text;
  auto* filesystems = json_object_get(parsed, "filesystems");
  ASSERT_TRUE(json_is_array(filesystems));
  EXPECT_GT(json_array_size(filesystems), 0U);
  EXPECT_EQ(json_array_size(json_object_get(parsed, "tape_devices")), 1U);
  EXPECT_EQ(json_array_size(json_object_get(parsed, "changers")), 1U);

  const auto expected_device_node
      = (dev_root.path() / "tape/by-id/scsi-123456-nst").string();

  auto* tape_device
      = json_array_get(json_object_get(parsed, "tape_devices"), 0);
  ASSERT_NE(tape_device, nullptr);
  EXPECT_STREQ(json_string_value(json_object_get(tape_device, "device_node")),
               expected_device_node.c_str());

  auto* changer = json_array_get(json_object_get(parsed, "changers"), 0);
  ASSERT_NE(changer, nullptr);
  auto* drive_device_nodes = json_object_get(changer, "drive_device_nodes");
  ASSERT_TRUE(json_is_array(drive_device_nodes));
  ASSERT_EQ(json_array_size(drive_device_nodes), 1U);
  EXPECT_STREQ(json_string_value(json_array_get(drive_device_nodes, 0)),
               expected_device_node.c_str());

  auto* drives = json_object_get(changer, "drives");
  ASSERT_TRUE(json_is_array(drives));
  ASSERT_EQ(json_array_size(drives), 1U);
  auto* drive = json_array_get(drives, 0);
  ASSERT_NE(drive, nullptr);
  EXPECT_STREQ(json_string_value(json_object_get(drive, "tape_device_node")),
               expected_device_node.c_str());
  EXPECT_STREQ(json_string_value(json_object_get(drive, "generic_device_node")),
               (dev_root.path() / "sg3").string().c_str());
  EXPECT_STREQ(json_string_value(json_object_get(drive, "source")),
               "read_element_status:identifier");

  json_decref(parsed);
}

TEST(SdDiscoveryCli, InvokesExecutableWithFullSectionViaSerialFallback)
{
  ScopedDirectory sysfs_root;
  ScopedDirectory dev_root;
  ScopedDirectory read_element_status_root;
  ScopedDirectory output_root;

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

  WriteBinaryFile(sysfs_root.path() / "class/scsi_changer/sch0/device/vendor",
                  "IBM\n");
  WriteBinaryFile(sysfs_root.path() / "class/scsi_changer/sch0/device/model",
                  "3573-TL\n");
  std::filesystem::create_directories(
      sysfs_root.path() / "class/scsi_changer/sch0/device/scsi_generic/sg4");
  WriteBinaryFile(dev_root.path() / "sg4", "");
  WriteBinaryFile(
      read_element_status_root.path() / "sg4.bin",
      BuildReadElementStatusData({BuildReadElementStatusDataTransferDescriptor(
          256, "HPE     Ultrium 9-SCSI  4E77FE415F", 0, 0, false, 0x02,
          0x01)}));

  ScopedEnvironmentVariable sysfs_override{"BAREOS_SD_DISCOVERY_SYSFS_ROOT",
                                           sysfs_root.path().string()};
  ScopedEnvironmentVariable dev_override{"BAREOS_SD_DISCOVERY_DEV_ROOT",
                                         dev_root.path().string()};
  ScopedEnvironmentVariable read_element_status_override{
      "BAREOS_SD_DISCOVERY_READ_ELEMENT_STATUS_ROOT",
      read_element_status_root.path().string()};

  const auto output_path = output_root.path() / "discovery.json";
  const auto command = QuoteShellArgument(BAREOS_SD_DISCOVER_BINARY)
                       + " --section full > "
                       + QuoteShellArgument(output_path.string());
  EXPECT_EQ(ExtractExitStatus(std::system(command.c_str())), 0);

  json_error_t error{};
  json_t* parsed = json_loads(ReadTextFile(output_path).c_str(), 0, &error);
  ASSERT_NE(parsed, nullptr) << error.text;
  auto* filesystems = json_object_get(parsed, "filesystems");
  ASSERT_TRUE(json_is_array(filesystems));
  EXPECT_GT(json_array_size(filesystems), 0U);
  EXPECT_EQ(json_array_size(json_object_get(parsed, "tape_devices")), 1U);
  EXPECT_EQ(json_array_size(json_object_get(parsed, "changers")), 1U);

  auto* changer = json_array_get(json_object_get(parsed, "changers"), 0);
  ASSERT_NE(changer, nullptr);
  auto* drive_device_nodes = json_object_get(changer, "drive_device_nodes");
  ASSERT_TRUE(json_is_array(drive_device_nodes));
  ASSERT_EQ(json_array_size(drive_device_nodes), 1U);
  EXPECT_STREQ(json_string_value(json_array_get(drive_device_nodes, 0)),
               (dev_root.path() / "nst0").string().c_str());

  auto* drives = json_object_get(changer, "drives");
  ASSERT_TRUE(json_is_array(drives));
  ASSERT_EQ(json_array_size(drives), 1U);
  auto* drive = json_array_get(drives, 0);
  ASSERT_NE(drive, nullptr);
  EXPECT_STREQ(json_string_value(json_object_get(drive, "tape_device_node")),
               (dev_root.path() / "nst0").string().c_str());
  EXPECT_STREQ(json_string_value(json_object_get(drive, "generic_device_node")),
               (dev_root.path() / "sg3").string().c_str());
  EXPECT_STREQ(json_string_value(json_object_get(drive, "device_identifier")),
               "t10.HPE     Ultrium 9-SCSI  4E77FE415F");
  EXPECT_STREQ(json_string_value(json_object_get(drive, "serial")),
               "4E77FE415F");
  EXPECT_STREQ(json_string_value(json_object_get(drive, "source")),
               "read_element_status:serial");

  json_decref(parsed);
}

TEST(SdDiscoveryCli,
     InvokesExecutableWithFullSectionViaSerialFallbackAndTapeById)
{
  ScopedDirectory sysfs_root;
  ScopedDirectory dev_root;
  ScopedDirectory read_element_status_root;
  ScopedDirectory output_root;

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
  CreateTapeByIdLink(dev_root.path(), "scsi-4e77fe415f", "nst0");

  WriteBinaryFile(sysfs_root.path() / "class/scsi_changer/sch0/device/vendor",
                  "IBM\n");
  WriteBinaryFile(sysfs_root.path() / "class/scsi_changer/sch0/device/model",
                  "3573-TL\n");
  std::filesystem::create_directories(
      sysfs_root.path() / "class/scsi_changer/sch0/device/scsi_generic/sg4");
  WriteBinaryFile(dev_root.path() / "sg4", "");
  WriteBinaryFile(
      read_element_status_root.path() / "sg4.bin",
      BuildReadElementStatusData({BuildReadElementStatusDataTransferDescriptor(
          256, "HPE     Ultrium 9-SCSI  4E77FE415F", 0, 0, false, 0x02,
          0x01)}));

  ScopedEnvironmentVariable sysfs_override{"BAREOS_SD_DISCOVERY_SYSFS_ROOT",
                                           sysfs_root.path().string()};
  ScopedEnvironmentVariable dev_override{"BAREOS_SD_DISCOVERY_DEV_ROOT",
                                         dev_root.path().string()};
  ScopedEnvironmentVariable read_element_status_override{
      "BAREOS_SD_DISCOVERY_READ_ELEMENT_STATUS_ROOT",
      read_element_status_root.path().string()};

  const auto output_path = output_root.path() / "discovery.json";
  const auto command = QuoteShellArgument(BAREOS_SD_DISCOVER_BINARY)
                       + " --section full > "
                       + QuoteShellArgument(output_path.string());
  EXPECT_EQ(ExtractExitStatus(std::system(command.c_str())), 0);

  json_error_t error{};
  json_t* parsed = json_loads(ReadTextFile(output_path).c_str(), 0, &error);
  ASSERT_NE(parsed, nullptr) << error.text;
  auto* filesystems = json_object_get(parsed, "filesystems");
  ASSERT_TRUE(json_is_array(filesystems));
  EXPECT_GT(json_array_size(filesystems), 0U);
  EXPECT_EQ(json_array_size(json_object_get(parsed, "tape_devices")), 1U);
  EXPECT_EQ(json_array_size(json_object_get(parsed, "changers")), 1U);

  const auto expected_device_node
      = (dev_root.path() / "tape/by-id/scsi-4e77fe415f").string();

  auto* changer = json_array_get(json_object_get(parsed, "changers"), 0);
  ASSERT_NE(changer, nullptr);
  auto* drive_device_nodes = json_object_get(changer, "drive_device_nodes");
  ASSERT_TRUE(json_is_array(drive_device_nodes));
  ASSERT_EQ(json_array_size(drive_device_nodes), 1U);
  EXPECT_STREQ(json_string_value(json_array_get(drive_device_nodes, 0)),
               expected_device_node.c_str());

  auto* drives = json_object_get(changer, "drives");
  ASSERT_TRUE(json_is_array(drives));
  ASSERT_EQ(json_array_size(drives), 1U);
  auto* drive = json_array_get(drives, 0);
  ASSERT_NE(drive, nullptr);
  EXPECT_STREQ(json_string_value(json_object_get(drive, "tape_device_node")),
               expected_device_node.c_str());
  EXPECT_STREQ(json_string_value(json_object_get(drive, "generic_device_node")),
               (dev_root.path() / "sg3").string().c_str());
  EXPECT_STREQ(json_string_value(json_object_get(drive, "device_identifier")),
               "t10.HPE     Ultrium 9-SCSI  4E77FE415F");
  EXPECT_STREQ(json_string_value(json_object_get(drive, "serial")),
               "4E77FE415F");
  EXPECT_STREQ(json_string_value(json_object_get(drive, "source")),
               "read_element_status:serial");

  json_decref(parsed);
}

TEST(SdDiscoveryCli, InvokesExecutableWithFullSectionUnmatched)
{
  ScopedDirectory sysfs_root;
  ScopedDirectory dev_root;
  ScopedDirectory read_element_status_root;
  ScopedDirectory output_root;

  WriteBinaryFile(sysfs_root.path() / "class/scsi_tape/nst0/device/vendor",
                  "IBM\n");
  WriteBinaryFile(sysfs_root.path() / "class/scsi_tape/nst0/device/model",
                  "ULTRIUM-HH8 \n");
  WriteBinaryFile(sysfs_root.path() / "class/scsi_tape/nst0/device/vpd_pg80",
                  BuildPage80("TAPE123"));
  std::filesystem::create_directories(
      sysfs_root.path() / "class/scsi_tape/nst0/device/scsi_generic/sg3");
  WriteBinaryFile(dev_root.path() / "nst0", "");
  WriteBinaryFile(dev_root.path() / "sg3", "");

  WriteBinaryFile(sysfs_root.path() / "class/scsi_changer/sch0/device/vendor",
                  "IBM\n");
  WriteBinaryFile(sysfs_root.path() / "class/scsi_changer/sch0/device/model",
                  "3573-TL\n");
  std::filesystem::create_directories(
      sysfs_root.path() / "class/scsi_changer/sch0/device/scsi_generic/sg4");
  WriteBinaryFile(dev_root.path() / "sg4", "");
  WriteBinaryFile(
      read_element_status_root.path() / "sg4.bin",
      BuildReadElementStatusData({BuildReadElementStatusDataTransferDescriptor(
          257, "\x55\x66\x77\x88")}));

  ScopedEnvironmentVariable sysfs_override{"BAREOS_SD_DISCOVERY_SYSFS_ROOT",
                                           sysfs_root.path().string()};
  ScopedEnvironmentVariable dev_override{"BAREOS_SD_DISCOVERY_DEV_ROOT",
                                         dev_root.path().string()};
  ScopedEnvironmentVariable read_element_status_override{
      "BAREOS_SD_DISCOVERY_READ_ELEMENT_STATUS_ROOT",
      read_element_status_root.path().string()};

  const auto output_path = output_root.path() / "discovery.json";
  const auto command = QuoteShellArgument(BAREOS_SD_DISCOVER_BINARY)
                       + " --section full > "
                       + QuoteShellArgument(output_path.string());
  EXPECT_EQ(ExtractExitStatus(std::system(command.c_str())), 0);

  json_error_t error{};
  json_t* parsed = json_loads(ReadTextFile(output_path).c_str(), 0, &error);
  ASSERT_NE(parsed, nullptr) << error.text;
  auto* filesystems = json_object_get(parsed, "filesystems");
  ASSERT_TRUE(json_is_array(filesystems));
  EXPECT_GT(json_array_size(filesystems), 0U);
  EXPECT_EQ(json_array_size(json_object_get(parsed, "tape_devices")), 1U);
  EXPECT_EQ(json_array_size(json_object_get(parsed, "changers")), 1U);

  auto* changer = json_array_get(json_object_get(parsed, "changers"), 0);
  ASSERT_NE(changer, nullptr);
  auto* drive_device_nodes = json_object_get(changer, "drive_device_nodes");
  ASSERT_TRUE(json_is_array(drive_device_nodes));
  ASSERT_EQ(json_array_size(drive_device_nodes), 0U);

  auto* drives = json_object_get(changer, "drives");
  ASSERT_TRUE(json_is_array(drives));
  ASSERT_EQ(json_array_size(drives), 1U);
  auto* drive = json_array_get(drives, 0);
  ASSERT_NE(drive, nullptr);
  EXPECT_EQ(json_integer_value(json_object_get(drive, "drive_element_address")),
            257);
  EXPECT_STREQ(json_string_value(json_object_get(drive, "tape_device_node")),
               "");
  EXPECT_STREQ(json_string_value(json_object_get(drive, "generic_device_node")),
               "");
  EXPECT_STREQ(json_string_value(json_object_get(drive, "device_identifier")),
               "naa.55667788");
  EXPECT_TRUE(json_is_null(json_object_get(drive, "serial")));
  EXPECT_STREQ(json_string_value(json_object_get(drive, "source")),
               "read_element_status:unmatched");

  json_decref(parsed);
}

TEST(SdDiscoveryCli, InvokesExecutableWithDefaultSection)
{
  ScopedDirectory sysfs_root;
  ScopedDirectory dev_root;
  ScopedDirectory read_element_status_root;
  ScopedDirectory output_root;

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
  WriteBinaryFile(
      read_element_status_root.path() / "sg4.bin",
      BuildReadElementStatusData({BuildReadElementStatusDataTransferDescriptor(
          256, "\x11\x22\x33\x44")}));

  ScopedEnvironmentVariable sysfs_override{"BAREOS_SD_DISCOVERY_SYSFS_ROOT",
                                           sysfs_root.path().string()};
  ScopedEnvironmentVariable dev_override{"BAREOS_SD_DISCOVERY_DEV_ROOT",
                                         dev_root.path().string()};
  ScopedEnvironmentVariable read_element_status_override{
      "BAREOS_SD_DISCOVERY_READ_ELEMENT_STATUS_ROOT",
      read_element_status_root.path().string()};

  const auto output_path = output_root.path() / "discovery.json";
  const auto command = QuoteShellArgument(BAREOS_SD_DISCOVER_BINARY) + " > "
                       + QuoteShellArgument(output_path.string());
  EXPECT_EQ(ExtractExitStatus(std::system(command.c_str())), 0);

  json_error_t error{};
  json_t* parsed = json_loads(ReadTextFile(output_path).c_str(), 0, &error);
  ASSERT_NE(parsed, nullptr) << error.text;
  auto* filesystems = json_object_get(parsed, "filesystems");
  ASSERT_TRUE(json_is_array(filesystems));
  EXPECT_GT(json_array_size(filesystems), 0U);
  EXPECT_EQ(json_array_size(json_object_get(parsed, "tape_devices")), 1U);
  EXPECT_EQ(json_array_size(json_object_get(parsed, "changers")), 1U);

  auto* changer = json_array_get(json_object_get(parsed, "changers"), 0);
  ASSERT_NE(changer, nullptr);
  auto* drives = json_object_get(changer, "drives");
  ASSERT_TRUE(json_is_array(drives));
  ASSERT_EQ(json_array_size(drives), 1U);
  auto* drive = json_array_get(drives, 0);
  ASSERT_NE(drive, nullptr);
  EXPECT_STREQ(json_string_value(json_object_get(drive, "source")),
               "read_element_status:identifier");

  json_decref(parsed);
}

TEST(SdDiscoveryCli, InvokesExecutableWithExplicitJsonFormat)
{
  ScopedDirectory sysfs_root;
  ScopedDirectory dev_root;
  ScopedDirectory read_element_status_root;
  ScopedDirectory output_root;

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
  WriteBinaryFile(
      read_element_status_root.path() / "sg4.bin",
      BuildReadElementStatusData({BuildReadElementStatusDataTransferDescriptor(
          256, "\x11\x22\x33\x44")}));

  ScopedEnvironmentVariable sysfs_override{"BAREOS_SD_DISCOVERY_SYSFS_ROOT",
                                           sysfs_root.path().string()};
  ScopedEnvironmentVariable dev_override{"BAREOS_SD_DISCOVERY_DEV_ROOT",
                                         dev_root.path().string()};
  ScopedEnvironmentVariable read_element_status_override{
      "BAREOS_SD_DISCOVERY_READ_ELEMENT_STATUS_ROOT",
      read_element_status_root.path().string()};

  const auto output_path = output_root.path() / "discovery.json";
  const auto command = QuoteShellArgument(BAREOS_SD_DISCOVER_BINARY)
                       + " --format json > "
                       + QuoteShellArgument(output_path.string());
  EXPECT_EQ(ExtractExitStatus(std::system(command.c_str())), 0);

  json_error_t error{};
  json_t* parsed = json_loads(ReadTextFile(output_path).c_str(), 0, &error);
  ASSERT_NE(parsed, nullptr) << error.text;
  auto* filesystems = json_object_get(parsed, "filesystems");
  ASSERT_TRUE(json_is_array(filesystems));
  EXPECT_GT(json_array_size(filesystems), 0U);
  EXPECT_EQ(json_array_size(json_object_get(parsed, "tape_devices")), 1U);
  EXPECT_EQ(json_array_size(json_object_get(parsed, "changers")), 1U);

  auto* changer = json_array_get(json_object_get(parsed, "changers"), 0);
  ASSERT_NE(changer, nullptr);
  auto* drives = json_object_get(changer, "drives");
  ASSERT_TRUE(json_is_array(drives));
  ASSERT_EQ(json_array_size(drives), 1U);
  auto* drive = json_array_get(drives, 0);
  ASSERT_NE(drive, nullptr);
  EXPECT_STREQ(json_string_value(json_object_get(drive, "source")),
               "read_element_status:identifier");

  json_decref(parsed);
}

TEST(SdDiscoveryCli, RejectsInvalidSectionOption)
{
  ScopedDirectory output_root;

  const auto stdout_path = output_root.path() / "stdout.txt";
  const auto stderr_path = output_root.path() / "stderr.txt";
  const auto command = QuoteShellArgument(BAREOS_SD_DISCOVER_BINARY)
                       + " --section bogus > "
                       + QuoteShellArgument(stdout_path.string()) + " 2> "
                       + QuoteShellArgument(stderr_path.string());
  EXPECT_NE(ExtractExitStatus(std::system(command.c_str())), 0);
  EXPECT_TRUE(ReadTextFile(stdout_path).empty());

  const auto stderr = ReadTextFile(stderr_path);
  EXPECT_NE(stderr.find("ValidationError"), std::string::npos);
  EXPECT_NE(stderr.find("--section"), std::string::npos);
  EXPECT_NE(stderr.find("bogus"), std::string::npos);
}

TEST(SdDiscoveryCli, RejectsInvalidFormatOption)
{
  ScopedDirectory output_root;

  const auto stdout_path = output_root.path() / "stdout.txt";
  const auto stderr_path = output_root.path() / "stderr.txt";
  const auto command = QuoteShellArgument(BAREOS_SD_DISCOVER_BINARY)
                       + " --format bogus > "
                       + QuoteShellArgument(stdout_path.string()) + " 2> "
                       + QuoteShellArgument(stderr_path.string());
  EXPECT_NE(ExtractExitStatus(std::system(command.c_str())), 0);
  EXPECT_TRUE(ReadTextFile(stdout_path).empty());

  const auto stderr = ReadTextFile(stderr_path);
  EXPECT_NE(stderr.find("ValidationError"), std::string::npos);
  EXPECT_NE(stderr.find("--format"), std::string::npos);
  EXPECT_NE(stderr.find("bogus"), std::string::npos);
}

TEST(SdDiscoveryCli, PrintsHelp)
{
  ScopedDirectory output_root;

  const auto stdout_path = output_root.path() / "stdout.txt";
  const auto stderr_path = output_root.path() / "stderr.txt";
  const auto command = QuoteShellArgument(BAREOS_SD_DISCOVER_BINARY)
                       + " --help > " + QuoteShellArgument(stdout_path.string())
                       + " 2> " + QuoteShellArgument(stderr_path.string());
  EXPECT_EQ(ExtractExitStatus(std::system(command.c_str())), 0);
  EXPECT_TRUE(ReadTextFile(stderr_path).empty());

  const auto stdout = ReadTextFile(stdout_path);
  EXPECT_NE(stdout.find("Print storage discovery information as JSON."),
            std::string::npos);
  EXPECT_NE(stdout.find("[OPTIONS]"), std::string::npos);
  EXPECT_NE(stdout.find("--format"), std::string::npos);
  EXPECT_NE(stdout.find("--section"), std::string::npos);
}

TEST(SdDiscoveryCli, PrintsVersion)
{
  ScopedDirectory output_root;

  const auto stdout_path = output_root.path() / "stdout.txt";
  const auto stderr_path = output_root.path() / "stderr.txt";
  const auto command = QuoteShellArgument(BAREOS_SD_DISCOVER_BINARY)
                       + " --version > "
                       + QuoteShellArgument(stdout_path.string()) + " 2> "
                       + QuoteShellArgument(stderr_path.string());
  EXPECT_EQ(ExtractExitStatus(std::system(command.c_str())), 0);
  EXPECT_TRUE(ReadTextFile(stderr_path).empty());

  const auto stdout = ReadTextFile(stdout_path);
  EXPECT_FALSE(stdout.empty());
  EXPECT_NE(stdout.find('\n'), std::string::npos);
}
#endif
