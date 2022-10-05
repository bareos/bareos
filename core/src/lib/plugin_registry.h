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

/*
 * generic plugin registry where you can register factory functions based
 * on a name. The registry is templated on the plugin's interface, thus the
 * naming is automatically per plugin interface.
 */

#ifndef BAREOS_LIB_PLUGIN_REGISTRY_H_
#define BAREOS_LIB_PLUGIN_REGISTRY_H_

#include <memory>
#include <functional>
#include <unordered_map>

template <typename T> class PluginRegistry {
  using Factory = std::function<T*()>;
  using Map = std::unordered_map<std::string, Factory>;

  static Map& GetMap()
  {
    // this ensures thread-safe initialization (Meyers' singleton)
    static Map plugin_map;
    return plugin_map;
  }

 public:
  static bool Add(const std::string& plugin_name, Factory plugin_factory)
  {
    GetMap().insert({plugin_name, plugin_factory});
    return true;
  }

  static bool IsRegistered(const std::string& plugin_name)
  {
    auto m = GetMap();
    return m.find(plugin_name) != m.end();
  }

  static T* Create(const std::string& plugin_name)
  {
    Dmsg0(100, "Creating Instance for '%s'\n", plugin_name.c_str());
    return GetMap().at(plugin_name)();
  }

  static void DumpDbg()
  {
    Dmsg0(100, "Start plugin registry dump\n");
    for (auto const& [plugin_name, plugin_factory] : GetMap()) {
      auto type = plugin_factory.target_type().name();
      Dmsg0(100, "Plugin name %s with factory %s\n", plugin_name.c_str(), type);
    }
    Dmsg0(100, "end plugin registry dump\n");
  }
};
#endif  // BAREOS_LIB_PLUGIN_REGISTRY_H_
