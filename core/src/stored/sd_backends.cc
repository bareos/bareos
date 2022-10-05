/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2014-2014 Planets Communications B.V.
   Copyright (C) 2014-2022 Bareos GmbH & Co. KG

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
// Marco van Wieringen, June 2014
/**
 * @file
 * Dynamic loading of SD backend plugins.
 */

#include "include/bareos.h"

#if defined(HAVE_DYNAMIC_SD_BACKENDS)

#  include "stored/stored.h"
#  include "sd_backends.h"

#  include <cstdlib>
#  include <dlfcn.h>
#  include <map>
#  include <memory>
#  include <stdexcept>
#  include <vector>

namespace storagedaemon {

struct BackendDeviceLibraryDescriptor {
 public:
  DeviceType device_type{DeviceType::B_UNKNOWN_DEV};

 private:
  void* dynamic_library_handle{};
  BackendInterface* backend_interface{};

 public:
  BackendDeviceLibraryDescriptor(DeviceType t_device_type,
                                 void* t_dynamic_library_handle,
                                 BackendInterface* t_backend_interface)
      : device_type(t_device_type)
      , dynamic_library_handle(t_dynamic_library_handle)
      , backend_interface(t_backend_interface)
  {
  }

  ~BackendDeviceLibraryDescriptor()
  {
    delete backend_interface;
    dlclose(dynamic_library_handle);
  }

  BackendDeviceLibraryDescriptor(const BackendDeviceLibraryDescriptor& other)
      = delete;
  BackendDeviceLibraryDescriptor& operator=(
      const BackendDeviceLibraryDescriptor& other)
      = delete;
  BackendDeviceLibraryDescriptor(BackendDeviceLibraryDescriptor&& other)
  {
    device_type = other.device_type;
    std::swap(dynamic_library_handle, other.dynamic_library_handle);
    std::swap(backend_interface, other.backend_interface);
  }
  BackendDeviceLibraryDescriptor& operator=(
      BackendDeviceLibraryDescriptor&& other)
  {
    device_type = other.device_type;
    std::swap(dynamic_library_handle, other.dynamic_library_handle);
    std::swap(backend_interface, other.backend_interface);
    return *this;
  }

  Device* GetDevice() { return backend_interface->GetDevice(); }
};

const std::map<DeviceType, const char*> device_type_to_name_mapping
    = {{DeviceType::B_FIFO_DEV, "fifo"},
       {DeviceType::B_FILE_DEV, "file"},
       {DeviceType::B_TAPE_DEV, "tape"},
       {DeviceType::B_GFAPI_DEV, "gfapi"},
       {DeviceType::B_DROPLET_DEV, "droplet"},
       {DeviceType::B_UNKNOWN_DEV, nullptr}};

static std::vector<std::unique_ptr<BackendDeviceLibraryDescriptor>>
    loaded_backends;
static std::vector<std::string> backend_directories;

void SetBackendDeviceDirectories(
    std::vector<std::string>&& new_backend_directories)
{
  backend_directories = new_backend_directories;
}

static inline const char* get_dlerror()
{
  const char* error = dlerror();
  return error != nullptr ? error : "";
}

Device* InitBackendDevice(JobControlRecord* jcr, DeviceType device_type)
{
  if (backend_directories.empty()) {
    Jmsg(jcr, M_ERROR_TERM, 0, _("Catalog Backends Dir not configured.\n"));
    // does not return
  }

  const char* interface_name = nullptr;

  try {
    interface_name = device_type_to_name_mapping.at(device_type);
  } catch (const std::out_of_range&) {
    return nullptr;
  }

  for (const auto& b : loaded_backends) {
    if (b->device_type == device_type) { return b->GetDevice(); }
  }

  t_backend_base GetBackend{nullptr};
  void* dynamic_library_handle{nullptr};

  for (const auto& backend_dir : backend_directories) {
    if (dynamic_library_handle != nullptr) { break; }

    std::string shared_library_name = backend_dir + "/libbareossd-";
    shared_library_name += interface_name;
    shared_library_name += DYN_LIB_EXTENSION;

    Dmsg3(100, "InitBackendDevice: checking backend %s/libbareossd-%s%s\n",
          backend_dir.c_str(), interface_name, DYN_LIB_EXTENSION);

    struct stat st;
    if (stat(shared_library_name.c_str(), &st) != 0) {
      Dmsg3(100,
            "InitBackendDevice: backend does not exist %s/libbareossd-%s%s\n",
            backend_dir.c_str(), interface_name, DYN_LIB_EXTENSION);
      return nullptr;
    }

    dynamic_library_handle = dlopen(shared_library_name.c_str(), RTLD_NOW);

    if (dynamic_library_handle == nullptr) {
      const char* error = get_dlerror();
      Jmsg(jcr, M_ERROR, 0, _("Unable to load shared library: %s ERR=%s\n"),
           shared_library_name.c_str(), error);
      Dmsg2(100, _("Unable to load shared library: %s ERR=%s\n"),
            shared_library_name.c_str(), error);
      continue;
    }

    GetBackend = reinterpret_cast<t_backend_base>(
        dlsym(dynamic_library_handle, "GetBackend"));

    if (GetBackend == nullptr) {
      const char* error = get_dlerror();
      Jmsg(jcr, M_ERROR, 0,
           _("Lookup of GetBackend in shared library %s failed: "
             "ERR=%s\n"),
           shared_library_name.c_str(), error);
      Dmsg2(100,
            _("Lookup of GetBackend in shared library %s failed: "
              "ERR=%s\n"),
            shared_library_name.c_str(), error);
      dlclose(dynamic_library_handle);
      dynamic_library_handle = nullptr;
      continue;
    }
  }

  if (dynamic_library_handle == nullptr) {
    Jmsg(jcr, M_ERROR_TERM, 0,
         _("Unable to load any shared library for libbareossd-%s%s\n"),
         interface_name, DYN_LIB_EXTENSION);
    return nullptr;
  }
  if (!GetBackend) {
    Jmsg(jcr, M_ERROR_TERM, 0,
         _("Unable to locate GetBackend() function in shared backend library "
           "for libbareossd-%s%s\n"),
         interface_name, DYN_LIB_EXTENSION);
    return nullptr;
  }

  auto b = std::make_unique<BackendDeviceLibraryDescriptor>(
      device_type, dynamic_library_handle, GetBackend());

  Device* d = b->GetDevice();
  loaded_backends.push_back(std::move(b));
  return d;
}

void FlushAndCloseBackendDevices() { loaded_backends.clear(); }

} /* namespace storagedaemon */

#endif /* HAVE_DYNAMIC_SD_BACKENDS */
