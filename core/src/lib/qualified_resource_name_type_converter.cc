/**
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

#include "qualified_resource_name_type_converter.h"

template <class T1, class T2>
std::map<T2, T1> swapPairs(std::map<T1, T2> m)
{
  std::map<T2, T1> m1;

  for (auto &&item : m) {
    m1.emplace(item.second, item.first);
  }

  return m1;
};

QualifiedResourceNameTypeConverter::QualifiedResourceNameTypeConverter(std::map<int, std::string> map)
{
  type_name_relation_map_ = map;
  name_type_relation_map_ = swapPairs(map);
}

std::string QualifiedResourceNameTypeConverter::ResourceTypeToString(const int &r_type) const
{
  if (type_name_relation_map_.empty()) {
    return std::string();
  }
  if (type_name_relation_map_.find(r_type) == type_name_relation_map_.end()) {
    return std::string();
  }
  return type_name_relation_map_.at(r_type);
}

int QualifiedResourceNameTypeConverter::StringToResourceType(const std::string &r_name) const
{
  if (name_type_relation_map_.empty()) {
    return -1;
  }
  if (name_type_relation_map_.find(r_name) == name_type_relation_map_.end()) {
    return -1;
  }
  return name_type_relation_map_.at(r_name);
}

bool QualifiedResourceNameTypeConverter::ResourceToString(const std::string &name_of_resource,
                                                          const int &r_type,
                                                          std::string &out) const
{
  std::string r_name = ResourceTypeToString(r_type);
  if (r_name.empty()) {
    return false;
  }
  out = r_name + std::to_string(':') + name_of_resource;
  return true;
}

bool QualifiedResourceNameTypeConverter::StringToResource(std::string &name_of_resource,
                                                          int &r_type,
                                                          const std::string &in) const
{
  return true;
}
