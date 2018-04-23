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

bool init_autochangers();
int autoload_device(DeviceControlRecord *dcr, int writing, BareosSocket *dir);
bool autochanger_cmd(DeviceControlRecord *dcr, BareosSocket *dir, const char *cmd);
bool autochanger_transfer_cmd(DeviceControlRecord *dcr, BareosSocket *dir,
                              slot_number_t src_slot, slot_number_t dst_slot);
bool unload_autochanger(DeviceControlRecord *dcr, slot_number_t loaded, bool lock_set = false);
bool unload_dev(DeviceControlRecord *dcr, Device *dev, bool lock_set = false);
slot_number_t get_autochanger_loaded_slot(DeviceControlRecord *dcr, bool lock_set = false);

#endif // BAREOS_STORED_AUTOCHANGER_H_
