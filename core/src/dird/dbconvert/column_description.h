/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2020-2020 Bareos GmbH & Co. KG

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

#ifndef BAREOS_SRC_DIRD_DBCONVERT_COLUMN_DESCRIPTION_H_
#define BAREOS_SRC_DIRD_DBCONVERT_COLUMN_DESCRIPTION_H_

#include <functional>
#include <map>
#include <string>

class FieldData;

class ColumnDescription {
 public:
  ColumnDescription(const char* column_name_in,
                    const char* data_type_in,
                    const char* max_length_in);
  virtual ~ColumnDescription() = default;

  std::string column_name;
  std::string data_type;
  std::size_t character_maximum_length{};
  std::function<void(FieldData&)> db_export_converter{};
  std::function<void(FieldData&)> db_import_converter{};
};

using DataTypeConverterMap =
    std::map<std::string, std::function<void(FieldData&)>>;

class ColumnDescriptionMysql : public ColumnDescription {
 public:
  ColumnDescriptionMysql(const char* column_name_in,
                         const char* data_type_in,
                         const char* max_length_in);

 private:
  static const DataTypeConverterMap db_import_converter_map;
};

class ColumnDescriptionPostgresql : public ColumnDescription {
 public:
  ColumnDescriptionPostgresql(const char* column_name_in,
                              const char* data_type_in,
                              const char* max_length_in);

 private:
  static const DataTypeConverterMap db_export_converter_map;
};


#endif  // BAREOS_SRC_DIRD_DBCONVERT_COLUMN_DESCRIPTION_H_
