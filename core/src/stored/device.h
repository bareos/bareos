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

#ifndef BAREOS_STORED_DEVICE_H_
#define BAREOS_STORED_DEVICE_H_

bool open_device(DeviceControlRecord *dcr);
bool first_open_device(DeviceControlRecord *dcr);
bool fixup_device_block_write_error(DeviceControlRecord *dcr, int retries = 4);
void set_start_vol_position(DeviceControlRecord *dcr);
void set_new_volume_parameters(DeviceControlRecord *dcr);
void set_new_file_parameters(DeviceControlRecord *dcr);
BootStrapRecord *position_device_to_first_file(JobControlRecord *jcr, DeviceControlRecord *dcr);
bool try_device_repositioning(JobControlRecord *jcr, DeviceRecord *rec, DeviceControlRecord *dcr);

#endif // BAREOS_STORED_DEVICE_H_
