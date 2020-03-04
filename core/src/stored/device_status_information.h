/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2020 Bareos GmbH & Co. KG

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
#ifndef BAREOS_SRC_STORED_DEVICE_STATUS_INFORMATION_H_
#define BAREOS_SRC_STORED_DEVICE_STATUS_INFORMATION_H_

#include "include/bareos.h"

namespace storagedaemon {

class DeviceResource;

/**
 * Device specific status information returned via Device::DeviceStatus()
 * method via bsdEventDriveStatus and bsdEventVolumeStatus plugin events.
 */
struct DeviceStatusInformation {
  DeviceResource* device = nullptr;
  POOLMEM* status = nullptr;
  int status_length = 0;
};

}  // namespace storagedaemon
#endif  // BAREOS_SRC_STORED_DEVICE_STATUS_INFORMATION_H_
