/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2019-2024 Bareos GmbH & Co. KG

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
    , device_resources(nullptr)
    , changer_name(nullptr)
    , changer_command(nullptr)
{
  return;
}

AutochangerResource& AutochangerResource::operator=(
    const AutochangerResource& rhs)
{
  BareosResource::operator=(rhs);
  device_resources = rhs.device_resources;
  changer_name = rhs.changer_name;
  changer_command = rhs.changer_command;
  changer_lock = rhs.changer_lock;
  return *this;
}

bool AutochangerResource::PrintConfig(OutputFormatterResource& send,
                                      const ConfigurationParser&,
                                      bool hide_sensitive_data,
                                      bool verbose)
{
  alist<DeviceResource*>* original_alist = device_resources;
  alist<DeviceResource*>* temp_alist
      = new alist<DeviceResource*>(original_alist->size(), not_owned_by_alist);
  for (auto* device_resource : original_alist) {
    if (device_resource->multiplied_device_resource) {
      if (device_resource->multiplied_device_resource == device_resource) {
        DeviceResource* d = new DeviceResource(*device_resource);
        d->MultipliedDeviceRestoreBaseName();
        temp_alist->append(d);
      }
    } else {
      DeviceResource* d = new DeviceResource(*device_resource);
      temp_alist->append(d);
    }
  }
  device_resources = temp_alist;
  BareosResource::PrintConfig(send, *my_config, hide_sensitive_data, verbose);
  device_resources = original_alist;
  for (auto* device_resource : temp_alist) { delete device_resource; }
  delete temp_alist;
  return true;
}

} /* namespace storagedaemon */
