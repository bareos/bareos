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
#include "include/bareos.h"
#include "dird/dbcopy/column_description.h"
#include "dird/dbcopy/row_data.h"

#include <cstring>
#include <iostream>
#include <vector>

ColumnDescription::ColumnDescription(const char* column_name_in,
                                     const char* data_type_in,
                                     const char* max_length_in)
    : column_name(column_name_in), data_type(data_type_in)
{
  std::string field;
  try {
    field = max_length_in != nullptr ? max_length_in : "";
    if (!field.empty()) { character_maximum_length = std::stoul(field); }
  } catch (const std::exception& e) {
    std::string err{__FILE__};
    err += ":";
    err += std::to_string(__LINE__);
    err += ": Could not convert number ";
    err += "<";
    err += field;
    err += ">";
    throw std::runtime_error(err);
  }
}

void ColumnDescription::RegisterConverterCallbackFromMap(
    const DataTypeConverterMap& converter_map)
{
  try {
    converter = converter_map.at(data_type);
  } catch (const std::out_of_range& e) {
    std::string err{"Postgresql: Data type not found in conversion map: "};
    err += data_type;
    throw std::runtime_error(err);
  }
}
