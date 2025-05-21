/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2011 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2019-2025 Bareos GmbH & Co. KG

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
#include "stored_conf.h"
#include "lib/alist.h"
#include "stored/device_resource.h"
#include "stored/stored_globals.h"

#include <unordered_set>

namespace storagedaemon {

AutochangerResource::AutochangerResource()
{
  rcode_ = R_AUTOCHANGER;
  rcode_str_ = "Autochanger";
}

std::unique_ptr<AutochangerResource>
AutochangerResource::CreateImplicitAutochanger(const std::string& device_name)
{
  auto autochanger = std::make_unique<AutochangerResource>();
  autochanger->device_resources
      = new alist<DeviceResource*>(10, not_owned_by_alist);
  autochanger->resource_name_ = strdup(device_name.c_str());
  autochanger->changer_name = strdup("/dev/null");
  autochanger->changer_command = strdup("");
  autochanger->implicitly_created_ = true;
  return autochanger;
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
  if (implicitly_created_) { return false; }
  std::unordered_set<DeviceResource*> original_copy_devices;
  alist<DeviceResource*>* original_alist = device_resources;
  alist<DeviceResource*>* temp_alist
      = new alist<DeviceResource*>(original_alist->size(), not_owned_by_alist);
  for (auto* device_resource : original_alist) {
    if (DeviceResource* original_copy_device = device_resource->multiplied_device_resource) {
      if (original_copy_devices.find(original_copy_device) == original_copy_devices.end()) {
        DeviceResource* d = new DeviceResource(*original_copy_device);
        temp_alist->append(d);    
        original_copy_devices.insert(original_copy_device);
      }
    }
    else {
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
