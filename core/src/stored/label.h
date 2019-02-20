
/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2018-2018 Bareos GmbH & Co. KG

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
#ifndef BAREOS_STORED_LABEL_H_
#define BAREOS_STORED_LABEL_H_

namespace storagedaemon {

int ReadDevVolumeLabel(DeviceControlRecord* dcr);
void CreateVolumeLabel(Device* dev, const char* VolName, const char* PoolName);

#define ANSI_VOL_LABEL 0
#define ANSI_EOF_LABEL 1
#define ANSI_EOV_LABEL 2

bool WriteAnsiIbmLabels(DeviceControlRecord* dcr,
                        int type,
                        const char* VolName);
int ReadAnsiIbmLabel(DeviceControlRecord* dcr);
bool WriteSessionLabel(DeviceControlRecord* dcr, int label);
void DumpVolumeLabel(Device* dev);
void DumpLabelRecord(Device* dev, DeviceRecord* rec, bool verbose);
bool UnserVolumeLabel(Device* dev, DeviceRecord* rec);
bool UnserSessionLabel(SESSION_LABEL* label, DeviceRecord* rec);
bool WriteNewVolumeLabelToDev(DeviceControlRecord* dcr,
                              const char* VolName,
                              const char* PoolName,
                              bool relabel);

} /* namespace storagedaemon */

#endif  // BAREOS_STORED_LABEL_H_
