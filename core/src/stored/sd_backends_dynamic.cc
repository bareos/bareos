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
/**
 * @file
 * Dynamic loading of SD backend plugins.
 */

#include "include/bareos.h"

#include "stored/stored.h"
#include "sd_backends.h"
#include <dlfcn.h>

namespace storagedaemon {

#if defined(HAVE_WIN32)
static const char* kDynLibExtension = ".dll";
#else
static const char* kDynLibExtension = ".so";
#endif

static bool LoadDynamicLibrary(
    const std::string& library_file,
    const std::vector<std::string>& library_directories)
{
  for (const auto& library_dir : library_directories) {
    if (dlopen((library_dir + "/" + library_file).c_str(), RTLD_NOW)) {
      Dmsg0(50, "Loaded dynamic library %s/%s\n", library_dir.c_str(),
            library_file.c_str());
      return true;
    }
    Dmsg0(50, "Could not load library %s/%s: %s\n", library_dir.c_str(),
          library_file.c_str(), dlerror());
  }
  return false;
}

bool LoadStorageBackend(const std::string& dev_type,
                        const std::vector<std::string>& backend_directories)
{
  using namespace std::string_literals;

  if (dev_type.empty() || backend_directories.empty()) { return false; }

  if (!LoadDynamicLibrary("libbareossd-"s + dev_type + kDynLibExtension,
                          backend_directories)) {
    return false;
  }

  if (!ImplementationFactory<Device>::IsRegistered(dev_type)) {
    Jmsg(nullptr, M_ERROR_TERM, 0,
         "Loaded backend library for %s did not register its backend. This is "
         "probably a bug in the backend library.\n",
         dev_type.c_str());
  }

  return true;
}

} /* namespace storagedaemon */
