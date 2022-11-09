/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2022-2022 Bareos GmbH & Co. KG

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

#ifndef BAREOS_STORED_SD_BACKENDS_H_
#define BAREOS_STORED_SD_BACKENDS_H_

#include <lib/implementation_factory.h>

namespace storagedaemon {

class Device;
template <typename T> Device* DeviceFactory(void) { return new T(); }

#if defined(HAVE_DYNAMIC_SD_BACKENDS)
bool LoadStorageBackend(const std::string& device_type,
                        const std::vector<std::string>& backend_directories);
#endif

#define REGISTER_SD_BACKEND(backend_name, backend_class)  \
  [[maybe_unused]] static bool backend_name##_backend_    \
      = ImplementationFactory<Device>::Add(#backend_name, \
                                           DeviceFactory<backend_class>);

}  // namespace storagedaemon

#endif  // BAREOS_STORED_SD_BACKENDS_H_
