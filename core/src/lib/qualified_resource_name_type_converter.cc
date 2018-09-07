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

#include <sstream>
#include <algorithm>
#include <iterator>

template <class T1, class T2>
std::map<T2, T1> swapPairs(std::map<T1, T2> m)
{
  std::map<T2, T1> m1;

  for (auto &&item : m) { m1.emplace(item.second, item.first); }

  return m1;
};

QualifiedResourceNameTypeConverter::QualifiedResourceNameTypeConverter(const std::map<int, std::string> &map)
    : type_name_relation_map_(map), name_type_relation_map_(swapPairs(map))
{
  return;
}

std::string QualifiedResourceNameTypeConverter::ResourceTypeToString(const int &r_type) const
{
  if (type_name_relation_map_.empty()) { return std::string(); }
  if (type_name_relation_map_.find(r_type) == type_name_relation_map_.end()) { return std::string(); }
  return type_name_relation_map_.at(r_type);
}

int QualifiedResourceNameTypeConverter::StringToResourceType(const std::string &r_name) const
{
  if (name_type_relation_map_.empty()) { return -1; }
  if (name_type_relation_map_.find(r_name) == name_type_relation_map_.end()) { return -1; }
  return name_type_relation_map_.at(r_name);
}

bool QualifiedResourceNameTypeConverter::ResourceToString(const std::string &name_of_resource,
                                                          const int &r_type,
                                                          std::string &str_out) const
{
  std::string r_name = ResourceTypeToString(r_type);
  if (r_name.empty()) { return false; }
  str_out = r_name + record_separator_ + name_of_resource;
  return true;
}

bool QualifiedResourceNameTypeConverter::ResourceToString(const std::string &name_of_resource,
                                                          const int &r_type,
                                                          const int &job_id,
                                                          std::string &str_out) const
{
  std::string r_name = ResourceTypeToString(r_type);
  if (r_name.empty()) { return false; }
  str_out = r_name + record_separator_ + name_of_resource + record_separator_ + std::to_string(job_id);
  return true;
}

template <class Container>
static void SplitStringIntoList(const std::string &str_in, Container &cont, char delim, int max_substring)
{
  std::stringstream ss(str_in);
  std::string token;
  int max = max_substring;
  while (std::getline(ss, token, delim) && max) {
    cont.push_back(token);
    max--;
  }
}

bool QualifiedResourceNameTypeConverter::StringToResource(std::string &name_of_resource,
                                                          int &r_type,
                                                          int &job_id,
                                                          const std::string &str_in) const
{
  std::vector<std::string> string_list;
  SplitStringIntoList(str_in, string_list, record_separator_, 3);
  if (string_list.size() < 2) { /* minimum of parameters are name_of_resource and r_type */
    return false;
  }
  std::string r_type_str = string_list.at(0);
  int r_type_temp        = StringToResourceType(r_type_str);
  if (r_type_temp == -1) { return false; }
  r_type           = r_type_temp;
  name_of_resource = string_list.at(1);

  if (string_list.size() == 3) { /* str_in contains probably a job_id */
    int job_id_temp;
    std::string job_id_str = string_list.at(2);
    try {
      job_id_temp = std::stoi(job_id_str);
    } catch (const std::exception &e) {
      return false;
    }
    job_id = job_id_temp;
  }
  return true;
}
