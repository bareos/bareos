/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2019-2019 Bareos GmbH & Co. KG

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

#include "stored/autochanger_resource.h"
#include "lib/alist.h"
#include "stored/device_resource.h"
#include "stored/stored_globals.h"

namespace storagedaemon {

AutochangerResource::AutochangerResource()
    : BareosResource()
    , device(nullptr)
    , changer_name(nullptr)
    , changer_command(nullptr)
{
  return;
}

AutochangerResource& AutochangerResource::operator=(
    const AutochangerResource& rhs)
{
  BareosResource::operator=(rhs);
  device = rhs.device;
  changer_name = rhs.changer_name;
  changer_command = rhs.changer_command;
  changer_lock = rhs.changer_lock;
  return *this;
}

bool AutochangerResource::PrintConfigToBuffer(PoolMem& buf)
{
  alist* original_alist = device;
  alist* temp_alist = new alist(original_alist->size(), not_owned_by_alist);
  DeviceResource* dev = nullptr;
  foreach_alist (dev, original_alist) {
    if (dev->multiplied_device_resource) {
      if (dev->multiplied_device_resource == dev) {
        DeviceResource* tmp_dev = new DeviceResource(*dev);
        tmp_dev->MultipliedDeviceRestoreBaseName();
        temp_alist->append(tmp_dev);
      }
    } else {
      DeviceResource* tmp_dev = new DeviceResource(*dev);
      temp_alist->append(tmp_dev);
    }
  }
  device = temp_alist;
  PrintConfig(buf, *my_config);
  device = original_alist;
  foreach_alist (dev, temp_alist) {
    delete dev;
  }
  delete temp_alist;
  return true;
}

} /* namespace storagedaemon */
