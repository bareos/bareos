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

QualifiedResourceNameTypeConverter::QualifiedResourceNameTypeConverter(std::map<std::string, int> map)
{
  name_type_relation_map_ = map;
}

std::string QualifiedResourceNameTypeConverter::ResourceTypeToString(int type)
{
  if (name_type_relation_map_.empty()) {
    return std::string();
  }
  return std::string();
}

int QualifiedResourceNameTypeConverter::StringToResourceType(const std::string &)
{
  if (name_type_relation_map_.empty()) {
    return -1;
  }
  return -1;
}

std::string QualifiedResourceNameTypeConverter::ResourceNameToFullyQualifiedString(
    std::string name_of_resource,
    int type_of_resource)
{
  return std::string();
}

#if 0
template <class T1, class T2>
map<T2, T1> swapPairs(map<T1, T2> m) {
    map<T2, T1> m1;

    for (auto&& item : m) {
        m1.emplace(item.second, item.first);
    }

    return m1;
};
#endif
