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

#ifndef BAREOS_STORED_SCSI_CHANGER_H_
#define BAREOS_STORED_SCSI_CHANGER_H_

#include "include/bareos.h"

#include <cstdint>
#include <string>
#include <vector>

class BareosSocket;

namespace storagedaemon {

class Device;
class DeviceControlRecord;

struct ScsiChangerElementAddressAssignment {
  uint16_t mte_addr{};
  uint16_t mte_count{};
  uint16_t se_addr{};
  uint16_t se_count{};
  uint16_t iee_addr{};
  uint16_t iee_count{};
  uint16_t dte_addr{};
  uint16_t dte_count{};
};

struct ScsiChangerElementStatus {
  uint8_t element_type_code{};
  uint16_t element_address{};
  bool full{};
  bool import_export{};
  bool except{};
  bool access{};
  bool export_enabled{};
  bool import_enabled{};
  bool lun_valid{};
  bool id_valid{};
  bool not_bus{};
  bool invert{};
  bool source_valid{};
  uint8_t asc{};
  uint8_t ascq{};
  uint8_t scsi_id{};
  uint8_t scsi_lun{};
  uint16_t src_se_addr{};
  std::string primary_volume_tag{};
};

bool IsNativeScsiChangerCommand(const char* changer_command);
bool ParseScsiChangerElementAddressAssignment(
    const uint8_t* data,
    std::size_t size,
    ScsiChangerElementAddressAssignment& assignment);
bool ParseScsiChangerElementStatusData(
    const uint8_t* data,
    std::size_t size,
    std::vector<ScsiChangerElementStatus>& elements);
bool ParseScsiLogSenseSupportedPages(const uint8_t* data,
                                     std::size_t size,
                                     std::vector<uint8_t>& pages);
std::vector<uint16_t> GetScsiChangerTransportElementAddresses(
    const ScsiChangerElementAddressAssignment& assignment);
std::vector<std::string> GetDriveDiagnosticDeviceCandidates(
    const char* diag_device_name,
    const char* archive_device_name);
std::string FormatNativeScsiDiagnosticStatus(const char* changer_command,
                                             const char* changer_name,
                                             const char* diag_device_name,
                                             const char* archive_device_name,
                                             bool drive_tapealert_enabled);

slot_number_t NativeScsiGetLoadedSlot(DeviceControlRecord* dcr);
bool NativeScsiLoadSlot(DeviceControlRecord* dcr, slot_number_t slot);
bool NativeScsiUnloadSlot(DeviceControlRecord* dcr, slot_number_t slot);
bool NativeScsiUnloadDrive(DeviceControlRecord* dcr, Device* dev);
bool NativeScsiAutochangerCmd(DeviceControlRecord* dcr,
                              BareosSocket* dir,
                              const char* cmd);
bool NativeScsiAutochangerTransferCmd(DeviceControlRecord* dcr,
                                      BareosSocket* dir,
                                      slot_number_t src_slot,
                                      slot_number_t dst_slot);
void ReportNativeScsiChangerDiagnostics(DeviceControlRecord* dcr);

} /* namespace storagedaemon */

#endif  // BAREOS_STORED_SCSI_CHANGER_H_
