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

#include "stored/scsi_changer.h"

#include "include/bareos.h"
#include "lib/bsock.h"
#include "lib/scsi_lli.h"
#include "lib/scsi_tapealert.h"
#include "stored/device_control_record.h"
#include "stored/sd_stats.h"
#include "stored/stored.h"

#include <algorithm>
#include <array>
#include <optional>
#include <string_view>
#include <thread>
#include <vector>

namespace storagedaemon {
namespace {

constexpr char kNativeScsiChangerCommand[] = "builtin:scsi";
constexpr uint8_t kScsiInquiry = 0x12;
constexpr uint8_t kScsiLogSense = 0x4D;
constexpr uint8_t kScsiModeSense6 = 0x1A;
constexpr uint8_t kScsiReadElementStatus = 0xB8;
constexpr uint8_t kScsiMoveMedium = 0xA5;
constexpr uint8_t kScsiTestUnitReady = 0x00;
constexpr uint8_t kLogSenseSupportedPages = 0x00;
constexpr uint8_t kLogSenseTapeAlert = 0x2E;

constexpr uint8_t kElementTypeAll = 0;
constexpr uint8_t kElementTypeStorage = 2;
constexpr uint8_t kElementTypeImportExport = 3;
constexpr uint8_t kElementTypeDrive = 4;

constexpr std::size_t kVolumeTagLength = 36;

uint16_t Get2(const uint8_t* data)
{
  return static_cast<uint16_t>((data[0] << 8) | data[1]);
}

uint32_t Get3(const uint8_t* data)
{
  return (static_cast<uint32_t>(data[0]) << 16)
         | (static_cast<uint32_t>(data[1]) << 8) | data[2];
}

void Put2(uint8_t* data, uint16_t value)
{
  data[0] = static_cast<uint8_t>(value >> 8);
  data[1] = static_cast<uint8_t>(value & 0xff);
}

void Put3(uint8_t* data, uint32_t value)
{
  data[0] = static_cast<uint8_t>((value >> 16) & 0xff);
  data[1] = static_cast<uint8_t>((value >> 8) & 0xff);
  data[2] = static_cast<uint8_t>(value & 0xff);
}

std::string TrimAscii(const uint8_t* data, std::size_t size)
{
  std::string result(reinterpret_cast<const char*>(data), size);
  auto first = result.find_first_not_of(' ');
  if (first == std::string::npos) { return {}; }
  auto last = result.find_last_not_of(' ');
  return result.substr(first, last - first + 1);
}

const char* GetDriveDiagnosticDevice(const DeviceControlRecord* dcr)
{
  if (dcr->device_resource->diag_device_name
      && dcr->device_resource->diag_device_name[0] != '\0') {
    return dcr->device_resource->diag_device_name;
  }

  return dcr->dev->archive_device_string;
}

bool TestUnitReady(const char* device_name)
{
  std::array<uint8_t, 6> cdb{};

  cdb[0] = kScsiTestUnitReady;
  return DoScsiCmd(-1, device_name, cdb.data(), cdb.size());
}

bool WaitForDriveReady(DeviceControlRecord* dcr)
{
  const char* device_name = GetDriveDiagnosticDevice(dcr);
  if (!device_name || device_name[0] == '\0') { return true; }

  auto timeout = std::max<utime_t>(1, dcr->device_resource->max_changer_wait);
  for (utime_t second = 0; second < timeout; ++second) {
    if (TestUnitReady(device_name)) { return true; }
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }

  Jmsg(dcr->jcr, M_WARNING, 0,
       T_("Native SCSI changer: drive %s did not become ready within %u seconds "
          "after media move.\n"),
       device_name, static_cast<unsigned>(timeout));
  return false;
}

bool ReadInquiry(const char* device_name, std::array<uint8_t, 96>& data)
{
  std::array<uint8_t, 6> cdb{};
  cdb[0] = kScsiInquiry;
  cdb[4] = static_cast<uint8_t>(data.size());

  data.fill(0);
  return RecvScsiCmdPage(-1, device_name, cdb.data(), cdb.size(), data.data(),
                         data.size());
}

bool ReadElementAddressAssignmentPage(
    const char* device_name, std::array<uint8_t, 255>& data)
{
  std::array<uint8_t, 6> cdb{};
  cdb[0] = kScsiModeSense6;
  cdb[1] = 0x08; /* DBD */
  cdb[2] = 0x1D;
  cdb[4] = static_cast<uint8_t>(data.size());

  data.fill(0);
  return RecvScsiCmdPage(-1, device_name, cdb.data(), cdb.size(), data.data(),
                         data.size());
}

bool ReadElementStatus(const char* device_name,
                       uint8_t element_type,
                       uint16_t number_of_elements,
                       bool with_volume_tags,
                       std::vector<uint8_t>& data)
{
  std::array<uint8_t, 12> cdb{};
  cdb[0] = kScsiReadElementStatus;
  cdb[1] = static_cast<uint8_t>((with_volume_tags ? 0x10 : 0x00)
                                | (element_type & 0x0f));
  Put2(&cdb[4], number_of_elements);
  Put3(&cdb[7], static_cast<uint32_t>(data.size()));

  std::fill(data.begin(), data.end(), 0);
  if (!RecvScsiCmdPage(-1, device_name, cdb.data(), cdb.size(), data.data(),
                       data.size())) {
    if (with_volume_tags) {
      cdb[1] = static_cast<uint8_t>(element_type & 0x0f);
      std::fill(data.begin(), data.end(), 0);
      return RecvScsiCmdPage(-1, device_name, cdb.data(), cdb.size(),
                             data.data(), data.size());
    }

    return false;
  }

  return true;
}

bool ReadLogSensePage(const char* device_name,
                      uint8_t page_code,
                      std::vector<uint8_t>& data)
{
  std::array<uint8_t, 10> cdb{};
  cdb[0] = kScsiLogSense;
  cdb[2] = page_code & 0x3f;
  Put2(&cdb[7], static_cast<uint16_t>(data.size()));

  std::fill(data.begin(), data.end(), 0);
  return RecvScsiCmdPage(-1, device_name, cdb.data(), cdb.size(), data.data(),
                         data.size());
}

bool MoveMedium(const char* device_name,
                uint16_t medium_transport_element,
                uint16_t source,
                uint16_t destination)
{
  std::array<uint8_t, 12> cdb{};
  cdb[0] = kScsiMoveMedium;
  Put2(&cdb[2], medium_transport_element);
  Put2(&cdb[4], source);
  Put2(&cdb[6], destination);

  return DoScsiCmd(-1, device_name, cdb.data(), cdb.size());
}

bool PrepareDriveForMediumMove(DeviceControlRecord* dcr)
{
  auto* dev = dcr->dev;
  if (!dev->IsOpen()) { return true; }

  bool ok = false;
  if (dev->HasCap(CAP_OFFLINEUNMOUNT)) {
    ok = dev->offline();
    if (!ok) {
      Jmsg(dcr->jcr, M_WARNING, 0,
           T_("Native SCSI changer: failed to offline drive %s before media "
              "move: %s"),
           dev->print_name(), NPRT(dev->print_errmsg()));
    }
  } else {
    ok = dev->close(dcr);
    if (!ok) {
      Jmsg(dcr->jcr, M_WARNING, 0,
           T_("Native SCSI changer: failed to close drive %s before media move: "
              "%s"),
           dev->print_name(), NPRT(dev->print_errmsg()));
    }
  }

  return ok;
}

void ReportNativeScsiDriveDiagnostics(DeviceControlRecord* dcr)
{
  auto devices = GetDriveDiagnosticDeviceCandidates(
      dcr->device_resource->diag_device_name, dcr->dev->archive_device_string);

  for (const auto& device_name : devices) {
    std::array<uint8_t, 96> inquiry{};
    if (ReadInquiry(device_name.c_str(), inquiry)) {
      Dmsg4(100, "Native SCSI drive inquiry: vendor=%s product=%s revision=%s "
                 "device=%s\n",
            TrimAscii(inquiry.data() + 8, 8).c_str(),
            TrimAscii(inquiry.data() + 16, 16).c_str(),
            TrimAscii(inquiry.data() + 32, 4).c_str(), device_name.c_str());
    }

    if (!TestUnitReady(device_name.c_str())) {
      Jmsg(dcr->jcr, M_WARNING, 0,
           T_("Native SCSI drive diagnostic: TEST UNIT READY failed on %s\n"),
           device_name.c_str());
    }
  }

  if (!dcr->device_resource->drive_tapealert_enabled) { return; }

  uint64_t flags = 0;
  for (const auto& device_name : devices) {
    if (GetTapealertFlags(-1, device_name.c_str(), &flags) && flags != 0) {
      UpdateDeviceTapealert(dcr->device_resource->resource_name_, flags,
                            static_cast<utime_t>(time(nullptr)));
      Jmsg(dcr->jcr, M_WARNING, 0,
           T_("Native SCSI drive diagnostic: TapeAlert flags=0x%llx on %s\n"),
           static_cast<unsigned long long>(flags), device_name.c_str());
      break;
    }
  }
}

std::optional<ScsiChangerElementAddressAssignment> ReadAssignment(
    const char* device_name)
{
  std::array<uint8_t, 255> data{};
  ScsiChangerElementAddressAssignment assignment;

  if (!ReadElementAddressAssignmentPage(device_name, data)
      || !ParseScsiChangerElementAddressAssignment(data.data(), data.size(),
                                                  assignment)) {
    return std::nullopt;
  }

  return assignment;
}

std::optional<std::vector<ScsiChangerElementStatus>> ReadStatuses(
    const char* device_name,
    uint8_t element_type,
    uint16_t number_of_elements,
    bool with_volume_tags)
{
  std::vector<uint8_t> data(32768);
  std::vector<ScsiChangerElementStatus> elements;

  if (!ReadElementStatus(device_name, element_type, number_of_elements,
                         with_volume_tags, data)
      || !ParseScsiChangerElementStatusData(data.data(), data.size(), elements)) {
    return std::nullopt;
  }

  return elements;
}

std::optional<std::vector<uint8_t>> ReadSupportedLogPages(const char* device_name)
{
  std::vector<uint8_t> data(256);
  std::vector<uint8_t> pages;

  if (!ReadLogSensePage(device_name, kLogSenseSupportedPages, data)
      || !ParseScsiLogSenseSupportedPages(data.data(), data.size(), pages)) {
    return std::nullopt;
  }

  return pages;
}

slot_number_t ElementAddressToSlot(
    const ScsiChangerElementAddressAssignment& assignment,
    uint16_t element_address,
    uint8_t element_type_code)
{
  switch (element_type_code) {
    case kElementTypeStorage:
      if (element_address >= assignment.se_addr) {
        return static_cast<slot_number_t>(element_address - assignment.se_addr
                                          + 1);
      }
      break;
    case kElementTypeImportExport:
      if (element_address >= assignment.iee_addr) {
        return static_cast<slot_number_t>(
            assignment.se_count + (element_address - assignment.iee_addr) + 1);
      }
      break;
    default:
      break;
  }

  return kInvalidSlotNumber;
}

uint16_t SlotToElementAddress(const ScsiChangerElementAddressAssignment& assignment,
                              slot_number_t slot)
{
  if (!IsSlotNumberValid(slot)) { return 0; }

  if (slot <= assignment.se_count) {
    return static_cast<uint16_t>(assignment.se_addr + slot - 1);
  }

  auto import_slot
      = static_cast<uint16_t>(slot - assignment.se_count);
  if (import_slot >= 1 && import_slot <= assignment.iee_count) {
    return static_cast<uint16_t>(assignment.iee_addr + import_slot - 1);
  }

  return 0;
}

uint16_t DriveToElementAddress(const ScsiChangerElementAddressAssignment& assignment,
                               const Device* dev)
{
  return static_cast<uint16_t>(assignment.dte_addr + dev->drive_index);
}

std::string FormatBarcode(const std::string& barcode)
{
  return barcode.empty() ? std::string{"E"} : std::string{"F:"} + barcode;
}

bool MoveMediumWithRetry(DeviceControlRecord* dcr,
                         const ScsiChangerElementAddressAssignment& assignment,
                         uint16_t source,
                         uint16_t destination)
{
  auto transports = GetScsiChangerTransportElementAddresses(assignment);
  for (const auto transport : transports) {
    if (MoveMedium(dcr->device_resource->changer_name, transport, source,
                   destination)) {
      return true;
    }

    Dmsg4(100,
          "Native SCSI changer: MOVE MEDIUM with transport %u failed from %u to "
          "%u on %s\n",
          transport, source, destination, dcr->device_resource->changer_name);
  }

  return false;
}

void SendListLine(BareosSocket* dir, const std::string& line)
{
  dir->fsend("%s\n", line.c_str());
}

void ReportElementExceptions(JobControlRecord* jcr,
                             const std::vector<ScsiChangerElementStatus>& elements)
{
  for (const auto& element : elements) {
    if (!element.except) { continue; }

    Jmsg(jcr, M_WARNING, 0,
         T_("Native SCSI changer diagnostic: element type=%u address=%u "
            "reports ASC=0x%02x ASCQ=0x%02x\n"),
         element.element_type_code, element.element_address, element.asc,
         element.ascq);
  }
}

bool HasLogPage(const std::vector<uint8_t>& pages, uint8_t page)
{
  return std::find(pages.begin(), pages.end(), page) != pages.end();
}

std::string FormatLogPages(const std::vector<uint8_t>& pages)
{
  std::string formatted;
  char page[8];

  for (std::size_t i = 0; i < pages.size(); ++i) {
    if (!formatted.empty()) { formatted += ' '; }
    Bsnprintf(page, sizeof(page), "0x%02x", pages[i]);
    formatted += page;
  }

  return formatted;
}

}  // namespace

bool IsNativeScsiChangerCommand(const char* changer_command)
{
  return changer_command != nullptr
         && bstrcmp(changer_command, kNativeScsiChangerCommand);
}

bool ParseScsiChangerElementAddressAssignment(
    const uint8_t* data,
    std::size_t size,
    ScsiChangerElementAddressAssignment& assignment)
{
  if (size < 22 || data[0] < 0x12) { return false; }

  const auto* page = data + 4;
  assignment.mte_addr = Get2(page + 2);
  assignment.mte_count = Get2(page + 4);
  assignment.se_addr = Get2(page + 6);
  assignment.se_count = Get2(page + 8);
  assignment.iee_addr = Get2(page + 10);
  assignment.iee_count = Get2(page + 12);
  assignment.dte_addr = Get2(page + 14);
  assignment.dte_count = Get2(page + 16);
  return true;
}

bool ParseScsiChangerElementStatusData(
    const uint8_t* data,
    std::size_t size,
    std::vector<ScsiChangerElementStatus>& elements)
{
  if (size < 8) { return false; }

  auto byte_count = Get3(data + 5);
  auto effective_size = std::min<std::size_t>(size, byte_count + 8);
  std::size_t offset = 8;
  elements.clear();

  while (offset + 8 <= effective_size) {
    const auto* page = data + offset;
    auto element_type = page[0];
    auto descriptor_length = Get2(page + 2);
    auto page_byte_count = Get3(page + 5);
    auto page_end = std::min<std::size_t>(effective_size, offset + 8
                                                              + page_byte_count);
    bool primary_volume_tag = (page[1] & 0x80) != 0;
    offset += 8;

    while (offset + descriptor_length <= page_end && descriptor_length >= 12) {
      const auto* descriptor = data + offset;
      ScsiChangerElementStatus status;
      status.element_type_code = element_type;
      status.element_address = Get2(descriptor);
      status.full = (descriptor[2] & 0x01) != 0;
      status.import_export = (descriptor[2] & 0x02) != 0;
      status.except = (descriptor[2] & 0x04) != 0;
      status.access = (descriptor[2] & 0x08) != 0;
      status.export_enabled = (descriptor[2] & 0x10) != 0;
      status.import_enabled = (descriptor[2] & 0x20) != 0;
      status.asc = descriptor[4];
      status.ascq = descriptor[5];
      status.scsi_lun = static_cast<uint8_t>(descriptor[6] & 0x07);
      status.lun_valid = (descriptor[6] & 0x10) != 0;
      status.id_valid = (descriptor[6] & 0x20) != 0;
      status.not_bus = (descriptor[6] & 0x80) != 0;
      status.scsi_id = descriptor[7];
      status.invert = (descriptor[9] & 0x40) != 0;
      status.source_valid = (descriptor[9] & 0x80) != 0;
      status.src_se_addr = Get2(descriptor + 10);

      if (primary_volume_tag && descriptor_length >= 12 + kVolumeTagLength) {
        status.primary_volume_tag
            = TrimAscii(descriptor + 12, kVolumeTagLength - 4);
      }

      elements.emplace_back(std::move(status));
      offset += descriptor_length;
    }

    offset = page_end;
  }

  return !elements.empty() || effective_size == 8;
}

bool ParseScsiLogSenseSupportedPages(const uint8_t* data,
                                     std::size_t size,
                                     std::vector<uint8_t>& pages)
{
  if (size < 4 || (data[0] & 0x3f) != kLogSenseSupportedPages) { return false; }

  auto page_length = static_cast<std::size_t>(Get2(data + 2));
  if (size < page_length + 4) { return false; }
  auto effective_size = page_length + 4;

  pages.clear();
  for (std::size_t offset = 4; offset < effective_size; ++offset) {
    pages.push_back(static_cast<uint8_t>(data[offset] & 0x3f));
  }

  return true;
}

std::vector<uint16_t> GetScsiChangerTransportElementAddresses(
    const ScsiChangerElementAddressAssignment& assignment)
{
  std::vector<uint16_t> transports;
  transports.reserve(assignment.mte_count);

  for (uint16_t index = 0; index < assignment.mte_count; ++index) {
    transports.push_back(static_cast<uint16_t>(assignment.mte_addr + index));
  }

  return transports;
}

std::vector<std::string> GetDriveDiagnosticDeviceCandidates(
    const char* diag_device_name,
    const char* archive_device_name)
{
  std::vector<std::string> devices;

  if (diag_device_name && diag_device_name[0] != '\0') {
    devices.emplace_back(diag_device_name);
  }

  if (archive_device_name && archive_device_name[0] != '\0'
      && (devices.empty() || devices.front() != archive_device_name)) {
    devices.emplace_back(archive_device_name);
  }

  return devices;
}

std::string FormatNativeScsiDiagnosticStatus(const char* changer_command,
                                             const char* changer_name,
                                             const char* diag_device_name,
                                             const char* archive_device_name,
                                             bool drive_tapealert_enabled)
{
  std::string status;
  char line[512];
  auto native_changer = IsNativeScsiChangerCommand(changer_command);
  auto drive_devices = GetDriveDiagnosticDeviceCandidates(diag_device_name,
                                                          archive_device_name);

  if (!native_changer && !drive_tapealert_enabled && drive_devices.empty()) {
    return status;
  }

  if (native_changer) {
    status += T_("    Changer backend: native SCSI\n");
    if (changer_name && changer_name[0] != '\0') {
      Bsnprintf(line, sizeof(line), T_("    Changer device: %s\n"), changer_name);
      status += line;
    }
  }

  if (!drive_devices.empty()) {
    status += T_("    Drive diagnostics via: ");
    for (std::size_t i = 0; i < drive_devices.size(); ++i) {
      if (i != 0) { status += ", "; }
      status += drive_devices[i];
    }
    status += '\n';
  }

  if (drive_tapealert_enabled) {
    status += T_("    Drive TapeAlert monitoring: enabled\n");
  }

  return status;
}

slot_number_t NativeScsiGetLoadedSlot(DeviceControlRecord* dcr)
{
  auto assignment = ReadAssignment(dcr->device_resource->changer_name);
  if (!assignment) { return kInvalidSlotNumber; }

  auto statuses = ReadStatuses(dcr->device_resource->changer_name, kElementTypeDrive,
                               assignment->dte_count, true);
  if (!statuses) { return kInvalidSlotNumber; }

  auto drive_element_address = DriveToElementAddress(*assignment, dcr->dev);
  for (const auto& element : *statuses) {
    if (element.element_address != drive_element_address) { continue; }
    if (!element.full) { return 0; }
    if (!element.source_valid) { return kInvalidSlotNumber; }
    return ElementAddressToSlot(*assignment, element.src_se_addr,
                                kElementTypeStorage);
  }

  return kInvalidSlotNumber;
}

bool NativeScsiLoadSlot(DeviceControlRecord* dcr, slot_number_t slot)
{
  auto assignment = ReadAssignment(dcr->device_resource->changer_name);
  if (!assignment) { return false; }

  auto source = SlotToElementAddress(*assignment, slot);
  auto destination = DriveToElementAddress(*assignment, dcr->dev);
  if (source == 0 || destination == 0) { return false; }

  if (!PrepareDriveForMediumMove(dcr)) { return false; }

  auto moved = MoveMediumWithRetry(dcr, *assignment, source, destination);
  if (moved) {
    moved = WaitForDriveReady(dcr);
    if (!moved) {
      ReportNativeScsiDriveDiagnostics(dcr);
      ReportNativeScsiChangerDiagnostics(dcr);
    }
  }

  return moved;
}

bool NativeScsiUnloadSlot(DeviceControlRecord* dcr, slot_number_t slot)
{
  auto assignment = ReadAssignment(dcr->device_resource->changer_name);
  if (!assignment) { return false; }

  auto source = DriveToElementAddress(*assignment, dcr->dev);
  auto destination = SlotToElementAddress(*assignment, slot);
  if (source == 0 || destination == 0) { return false; }

  if (!PrepareDriveForMediumMove(dcr)) { return false; }

  return MoveMediumWithRetry(dcr, *assignment, source, destination);
}

bool NativeScsiUnloadDrive(DeviceControlRecord* dcr, Device* dev)
{
  auto save_dev = dcr->dev;
  auto save_slot = dcr->VolCatInfo.Slot;
  dcr->SetDev(dev);
  dcr->VolCatInfo.Slot = dev->GetSlot();

  auto unloaded = NativeScsiUnloadSlot(dcr, dev->GetSlot());
  dcr->VolCatInfo.Slot = save_slot;
  dcr->SetDev(save_dev);
  return unloaded;
}

bool NativeScsiAutochangerCmd(DeviceControlRecord* dcr,
                              BareosSocket* dir,
                              const char* cmd)
{
  auto assignment = ReadAssignment(dcr->device_resource->changer_name);
  if (!assignment) {
    dir->fsend(T_("3998 Native SCSI autochanger error: could not read element "
                  "address assignment.\n"));
    return false;
  }

  auto statuses
      = ReadStatuses(dcr->device_resource->changer_name, kElementTypeAll,
                     static_cast<uint16_t>(assignment->mte_count
                                           + assignment->se_count
                                           + assignment->iee_count
                                           + assignment->dte_count),
                     true);
  if (!statuses) {
    dir->fsend(
        T_("3998 Native SCSI autochanger error: could not read element status.\n"));
    return false;
  }

  ReportElementExceptions(dcr->jcr, *statuses);

  if (bstrcmp(cmd, "slots")) {
    dir->fsend("slots=%hd", assignment->se_count + assignment->iee_count);
    return true;
  }

  if (bstrcmp(cmd, "list")) {
    for (const auto& element : *statuses) {
      if (!element.full) { continue; }
      if (element.element_type_code == kElementTypeStorage) {
        auto slot = ElementAddressToSlot(*assignment, element.element_address,
                                         element.element_type_code);
        SendListLine(dir, std::to_string(slot) + ":" + element.primary_volume_tag);
      } else if (element.element_type_code == kElementTypeDrive
                 && element.source_valid) {
        auto slot = ElementAddressToSlot(*assignment, element.src_se_addr,
                                         kElementTypeStorage);
        if (IsSlotNumberValid(slot)) {
          SendListLine(dir,
                       std::to_string(slot) + ":" + element.primary_volume_tag);
        }
      }
    }
    return true;
  }

  if (bstrcmp(cmd, "listall")) {
    for (const auto& element : *statuses) {
      switch (element.element_type_code) {
        case kElementTypeDrive: {
          auto drive = element.element_address - assignment->dte_addr;
          if (element.full && element.source_valid) {
            auto slot = ElementAddressToSlot(*assignment, element.src_se_addr,
                                             kElementTypeStorage);
            SendListLine(dir, "D:" + std::to_string(drive) + ":F:"
                                  + std::to_string(slot) + ":"
                                  + element.primary_volume_tag);
          } else {
            SendListLine(dir, "D:" + std::to_string(drive) + ":E");
          }
          break;
        }
        case kElementTypeStorage: {
          auto slot = ElementAddressToSlot(*assignment, element.element_address,
                                           element.element_type_code);
          SendListLine(dir, "S:" + std::to_string(slot) + ":"
                                + FormatBarcode(element.primary_volume_tag));
          break;
        }
        case kElementTypeImportExport: {
          auto slot = ElementAddressToSlot(*assignment, element.element_address,
                                           element.element_type_code);
          SendListLine(dir, "I:" + std::to_string(slot) + ":"
                                + FormatBarcode(element.primary_volume_tag));
          break;
        }
        default:
          break;
      }
    }
    return true;
  }

  return false;
}

bool NativeScsiAutochangerTransferCmd(DeviceControlRecord* dcr,
                                      BareosSocket* dir,
                                      slot_number_t src_slot,
                                      slot_number_t dst_slot)
{
  auto assignment = ReadAssignment(dcr->device_resource->changer_name);
  if (!assignment) {
    dir->fsend(T_("3998 Native SCSI autochanger error: could not read element "
                  "address assignment.\n"));
    return false;
  }

  auto source = SlotToElementAddress(*assignment, src_slot);
  auto destination = SlotToElementAddress(*assignment, dst_slot);
  if (source == 0 || destination == 0) {
    dir->fsend(T_("3998 Native SCSI autochanger error: invalid slot for "
                  "transfer.\n"));
    return false;
  }

  if (!MoveMediumWithRetry(dcr, *assignment, source, destination)) {
    dir->fsend(T_("3998 Native SCSI autochanger error: transfer failed.\n"));
    return false;
  }

  dir->fsend(T_("3308 Successfully transferred volume from slot %hd to %hd.\n"),
             src_slot, dst_slot);
  return true;
}

void ReportNativeScsiChangerDiagnostics(DeviceControlRecord* dcr)
{
  const char* device_name = dcr->device_resource->changer_name;
  if (!device_name || device_name[0] == '\0') { return; }

  std::array<uint8_t, 96> inquiry{};
  if (ReadInquiry(device_name, inquiry)) {
    Dmsg4(100, "Native SCSI changer inquiry: vendor=%s product=%s revision=%s "
               "device=%s\n",
          TrimAscii(inquiry.data() + 8, 8).c_str(),
          TrimAscii(inquiry.data() + 16, 16).c_str(),
          TrimAscii(inquiry.data() + 32, 4).c_str(), device_name);
  }

  if (!TestUnitReady(device_name)) {
    Jmsg(dcr->jcr, M_WARNING, 0,
         T_("Native SCSI changer diagnostic: TEST UNIT READY failed on %s\n"),
         device_name);
  }

  auto supported_log_pages = ReadSupportedLogPages(device_name);
  if (supported_log_pages) {
    Dmsg2(100, "Native SCSI changer diagnostic: supported LOG SENSE pages on %s: "
               "%s\n",
          device_name, FormatLogPages(*supported_log_pages).c_str());
  }

  uint64_t flags = 0;
  if ((!supported_log_pages || HasLogPage(*supported_log_pages, kLogSenseTapeAlert))
      && GetTapealertFlags(-1, device_name, &flags) && flags != 0) {
    Jmsg(dcr->jcr, M_WARNING, 0,
         T_("Native SCSI changer diagnostic: TapeAlert flags=0x%llx on %s\n"),
         static_cast<unsigned long long>(flags), device_name);
  }

  auto assignment = ReadAssignment(device_name);
  if (!assignment) { return; }

  auto statuses
      = ReadStatuses(device_name, kElementTypeAll,
                     static_cast<uint16_t>(assignment->mte_count
                                           + assignment->se_count
                                           + assignment->iee_count
                                           + assignment->dte_count),
                     false);
  if (!statuses) { return; }

  ReportElementExceptions(dcr->jcr, *statuses);
}

} /* namespace storagedaemon */
