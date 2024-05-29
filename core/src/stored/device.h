/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2018-2024 Bareos GmbH & Co. KG

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

#ifndef BAREOS_STORED_DEVICE_H_
#define BAREOS_STORED_DEVICE_H_

namespace storagedaemon {

bool FirstOpenDevice(DeviceControlRecord* dcr);
bool FixupDeviceBlockWriteError(DeviceControlRecord* dcr, int retries = 4);
void SetStartVolPosition(DeviceControlRecord* dcr);
void SetNewVolumeParameters(DeviceControlRecord* dcr);
void SetNewFileParameters(DeviceControlRecord* dcr);
BootStrapRecord* PositionDeviceToFirstFile(JobControlRecord* jcr,
                                           BootStrapRecord** current,
                                           DeviceControlRecord* dcr);
bool TryDeviceRepositioning(JobControlRecord* jcr,
                            BootStrapRecord** current,
                            DeviceRecord* rec,
                            DeviceControlRecord* dcr);

} /* namespace storagedaemon */

#endif  // BAREOS_STORED_DEVICE_H_
