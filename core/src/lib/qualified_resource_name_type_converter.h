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

#include <bareos.h>

class QualifiedResourceNameTypeConverter {
 public:
  QualifiedResourceNameTypeConverter(std::map<std::string, int> map = std::map<std::string, int>());
  std::string ResourceTypeToString(int type);
  std::string ResourceNameToFullyQualifiedString(std::string name_of_resource, int type_of_resource);
  int StringToResourceType(const std::string &);

 private:
  std::map<std::string, int> name_type_relation_map_;
};

#endif /* BAREOS_LIB_QUALIFIED_RESOURCE_NAME_TYPE_CONVERTER_H_ */
