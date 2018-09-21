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
#ifndef BAREOS_STORED_AUTOCHANGER_H_
#define BAREOS_STORED_AUTOCHANGER_H_

class BareosSocket;

namespace storagedaemon {

class DeviceControlRecord;
class Device;

bool InitAutochangers();
int AutoloadDevice(DeviceControlRecord *dcr, int writing, BareosSocket *dir);
bool AutochangerCmd(DeviceControlRecord *dcr, BareosSocket *dir, const char *cmd);
bool AutochangerTransferCmd(DeviceControlRecord *dcr, BareosSocket *dir,
                              slot_number_t src_slot, slot_number_t dst_slot);
bool UnloadAutochanger(DeviceControlRecord *dcr, slot_number_t loaded, bool lock_set = false);
bool UnloadDev(DeviceControlRecord *dcr, Device *dev, bool lock_set = false);
slot_number_t GetAutochangerLoadedSlot(DeviceControlRecord *dcr, bool lock_set = false);

} /* namespace storagedaemon  */

#endif // BAREOS_STORED_AUTOCHANGER_H_
