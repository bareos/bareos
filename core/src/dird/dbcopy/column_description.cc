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
#include <vector>

ColumnDescription::ColumnDescription(const char* column_name_in,
                                     const char* data_type_in,
                                     const char* max_length_in)
    : column_name(column_name_in), data_type(data_type_in)
{
  std::string field;
  try {
    field = max_length_in ? max_length_in : "";
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

static void no_conversion(FieldData& fd)
{
  fd.data_pointer = fd.data_pointer ? fd.data_pointer : "";
}

static void timestamp_conversion_postgresql(FieldData& fd)
{
  if (!fd.data_pointer) {
    fd.data_pointer = "1970-01-01 00:00:00";
  } else if (fd.data_pointer[0] == '0') {
    fd.data_pointer = "1970-01-01 00:00:00";
  }
}

static void string_conversion_postgresql(FieldData& fd)
{
  if (fd.data_pointer) {
    std::size_t len{strlen(fd.data_pointer)};
    fd.converted_data.resize(len * 2 + 1);
    char* n = fd.converted_data.data();
    const char* o = fd.data_pointer;

    while (len--) {
      switch (*o) {
        case '\'':
          *n++ = '\'';
          *n++ = '\'';
          o++;
          break;
        case 0:
          *n++ = '\\';
          *n++ = 0;
          o++;
          break;
        case '\"':
          *n++ = '\\';
          *n++ = '\"';
          o++;
          break;
        default:
          *n++ = *o++;
          break;
      }
    }
    *n = 0;
    fd.data_pointer = fd.converted_data.data();
  } else {
    fd.data_pointer = "";
  }
}

const DataTypeConverterMap ColumnDescriptionMysql::db_import_converter_map{
    {"bigint", no_conversion},
    {"binary", no_conversion},
    {"blob", string_conversion_postgresql},
    {"char", no_conversion},
    {"datetime", timestamp_conversion_postgresql},
    {"decimal", no_conversion},
    {"enum", no_conversion},
    {"int", no_conversion},
    {"longblob", no_conversion},
    {"smallint", no_conversion},
    {"text", string_conversion_postgresql},
    {"timestamp", timestamp_conversion_postgresql},
    {"tinyblob", no_conversion},
    {"tinyint", no_conversion},
    {"varchar", string_conversion_postgresql}};

ColumnDescriptionMysql::ColumnDescriptionMysql(const char* column_name_in,
                                               const char* data_type_in,
                                               const char* max_length_in)
    : ColumnDescription(column_name_in, data_type_in, max_length_in)
{
  try {
    db_import_converter = db_import_converter_map.at(data_type_in);
  } catch (const std::out_of_range& e) {
    std::string err{"Mysql: Data type not found in conversion map: "};
    err += data_type_in;
    throw std::runtime_error(err);
  }
}

const DataTypeConverterMap ColumnDescriptionPostgresql::db_export_converter_map{
    {"bigint", no_conversion},
    {"bytea", no_conversion},
    {"character", no_conversion},
    {"integer", no_conversion},
    {"numeric", no_conversion},
    {"smallint", no_conversion},
    {"text", no_conversion},
    {"timestamp without time zone", no_conversion}};

ColumnDescriptionPostgresql::ColumnDescriptionPostgresql(
    const char* column_name_in,
    const char* data_type_in,
    const char* max_length_in)
    : ColumnDescription(column_name_in, data_type_in, max_length_in)
{
  try {
    db_export_converter = db_export_converter_map.at(data_type_in);
  } catch (const std::out_of_range& e) {
    std::string err{"Postgresql: Data type not found in conversion map: "};
    err += data_type_in;
    throw std::runtime_error(err);
  }
}
