/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2022 Bareos GmbH & Co. KG

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

#ifndef BAREOS_STORED_SD_DEVICE_CONTROL_RECORD_H_
#define BAREOS_STORED_SD_DEVICE_CONTROL_RECORD_H_

#include "stored/device_control_record.h"

namespace storagedaemon {

class StorageDaemonDeviceControlRecord : public DeviceControlRecord {
 public:
  bool DirFindNextAppendableVolume() override;
  bool DirUpdateVolumeInfo(bool label, bool update_LastWritten) override;
  bool DirCreateJobmediaRecord(bool zero) override;
  bool DirUpdateFileAttributes(DeviceRecord* record) override;
  bool DirAskSysopToMountVolume(int mode) override;
  bool DirAskSysopToCreateAppendableVolume() override;
  bool DirGetVolumeInfo(enum get_vol_info_rw writing) override;
  bool DirAskToUpdateFileList() override;
  bool DirAskToUpdateJobRecord() override;
  DeviceControlRecord* get_new_spooling_dcr() override;
};


}  // namespace storagedaemon

#endif  // BAREOS_STORED_SD_DEVICE_CONTROL_RECORD_H_
