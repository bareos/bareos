/**
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2018-2026 Bareos GmbH & Co. KG

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

#ifndef BAREOS_LIB_QUALIFIED_RESOURCE_NAME_TYPE_CONVERTER_H_
#define BAREOS_LIB_QUALIFIED_RESOURCE_NAME_TYPE_CONVERTER_H_

#include <map>
#include "include/bareos.h"

class QualifiedResourceNameTypeConverter {
 public:
  QualifiedResourceNameTypeConverter(const std::map<int, std::string>& map);
  bool ResourceToString(const std::string& name_of_resource,
                        const int& r_type,
                        std::string& str_out) const;
  bool ResourceToString(const std::string& name_of_resource,
                        const int& r_type,
                        const std::string separator,
                        std::string& str_out) const;
  bool StringToResource(std::string& name_of_resource,
                        int& r_type,
                        const std::string& in) const;

  std::string ResourceTypeToString(const int& type) const;
  int StringToResourceType(const std::string&) const;

 private:
  const std::map<int, std::string> type_name_relation_map_;
  const std::map<std::string, int> name_type_relation_map_;
};

// this is a list of _all_ resources, for _all_ daemons

namespace global_resource {

enum class Type
{
  Unknown,
  Autochanger,
  Catalog,
  Client,
  Console,
  ConsoleFont,
  Counter,
  Device,
  Director,
  Fileset,
  Job,
  JobDefs,
  Messages,
  Monitor,
  Ndmp,
  Pool,
  Profile,
  Schedule,
  Storage,
  User,
};

struct TypeName {
  Type type;
  std::string_view name;
};

static constexpr TypeName type_names[] = {
    {Type::Autochanger, "R_AUTOCHANGER"},
    {Type::Catalog, "R_CATALOG"},
    {Type::Client, "R_CLIENT"},
    {Type::ConsoleFont, "R_CONSOLE_FONT"},
    {Type::Console, "R_CONSOLE"},
    {Type::Counter, "R_COUNTER"},
    {Type::Device, "R_DEVICE"},
    {Type::Director, "R_DIRECTOR"},
    {Type::Fileset, "R_FILESET"},
    {Type::JobDefs, "R_JOBDEFS"},
    {Type::Job, "R_JOB"},
    {Type::Messages, "R_MSGS"},
    {Type::Monitor, "R_MONITOR"},
    {Type::Ndmp, "R_NDMP"},
    {Type::Pool, "R_POOL"},
    {Type::Profile, "R_PROFILE"},
    {Type::Schedule, "R_SCHEDULE"},
    {Type::Storage, "R_STORAGE"},
    {Type::User, "R_USER"},
};

static constexpr std::string_view GetNameFromType(Type type)
{
  if (auto iter = std::ranges::find_if(
          type_names, [type](auto& item) { return item.type == type; });
      iter != std::end(type_names)) {
    return iter->name;
  }
  return "";
}

static constexpr Type GetTypeFromName(std::string_view name)
{
  if (auto iter = std::ranges::find_if(
          type_names, [name](auto& item) { return item.name == name; });
      iter != std::end(type_names)) {
    return iter->type;
  }
  return Type::Unknown;
}


};  // namespace global_resource

#endif  // BAREOS_LIB_QUALIFIED_RESOURCE_NAME_TYPE_CONVERTER_H_
