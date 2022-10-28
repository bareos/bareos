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
 * generic registry where you can register factory functions based on a name.
 * The registry is templated on the implementation's interface, thus the
 * names will be per interface.
 */

#ifndef BAREOS_LIB_IMPLEMENTATION_FACTORY_H_
#define BAREOS_LIB_IMPLEMENTATION_FACTORY_H_

#include <memory>
#include <functional>
#include <unordered_map>

template <typename Interface> class ImplementationFactory {
  using Factory = std::function<Interface*()>;
  using Map = std::unordered_map<std::string, Factory>;

  static Map& GetMap()
  {
    // this ensures thread-safe initialization (Meyers' singleton)
    static Map factory_map;
    return factory_map;
  }

 public:
  // this retuns a bool, so you can initialize a global static variable from it
  // to register your plugin when the program starts or during dl_open()
  static bool Add(const std::string& implementation_name, Factory factory)
  {
    GetMap().insert({implementation_name, factory});
    return true;
  }

  static bool IsRegistered(const std::string& implementation_name)
  {
    auto m = GetMap();
    return m.find(implementation_name) != m.end();
  }

  static Interface* Create(const std::string& implementation_name)
  {
    Dmsg0(100, "Creating Instance for '%s'\n", implementation_name.c_str());
    return GetMap().at(implementation_name)();
  }
};
#endif  // BAREOS_LIB_IMPLEMENTATION_FACTORY_H_
