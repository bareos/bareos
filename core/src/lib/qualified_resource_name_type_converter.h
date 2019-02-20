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

#ifndef BAREOS_LIB_QUALIFIED_RESOURCE_NAME_TYPE_CONVERTER_H_
#define BAREOS_LIB_QUALIFIED_RESOURCE_NAME_TYPE_CONVERTER_H_ 1

#include "include/bareos.h"

class QualifiedResourceNameTypeConverter {
 public:
  QualifiedResourceNameTypeConverter(const std::map<int, std::string>& map);
  bool ResourceToString(const std::string& name_of_resource,
                        const int& r_type,
                        std::string& out) const;
  bool StringToResource(std::string& name_of_resource,
                        int& r_type,
                        const std::string& in) const;

  std::string ResourceTypeToString(const int& type) const;
  int StringToResourceType(const std::string&) const;

 private:
  const std::map<int, std::string> type_name_relation_map_;
  const std::map<std::string, int> name_type_relation_map_;
};

#endif /* BAREOS_LIB_QUALIFIED_RESOURCE_NAME_TYPE_CONVERTER_H_ */
