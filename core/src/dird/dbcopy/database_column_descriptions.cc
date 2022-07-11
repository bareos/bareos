/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2020-2022 Bareos GmbH & Co. KG

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
#include "cats/cats.h"
#include "dird/dbcopy/database_column_descriptions.h"
#include "lib/util.h"

#include <algorithm>
#include <iostream>

void DatabaseColumnDescriptions::SelectColumnDescriptionsAndAddToMap(
    const std::string& sql_query)
{
  if (!db_->SqlQuery(sql_query.c_str(), ResultHandler_, this)) {
    std::string err{"Could not select table names: "};
    err += sql_query;
    throw std::runtime_error(err);
  }
}

int DatabaseColumnDescriptions::ResultHandler_(void* ctx,
                                               int /*fields*/,
                                               char** row)
{
  auto t = static_cast<DatabaseColumnDescriptions*>(ctx);

  t->AddToMap(row[RowIndex::kColumnName], row[RowIndex::kDataType],
              row[RowIndex::kCharMaxLenght]);

  return 0;
}

void DatabaseColumnDescriptions::AddToMap(const char* column_name_in,
                                          const char* data_type_in,
                                          const char* max_length_in)

{
  std::string column_name_lower_case;
  std::string column_name{column_name_in};

  StringToLowerCase(column_name_lower_case, column_name_in);

  column_descriptions.emplace(  // std::map sorts keys ascending by default
      std::piecewise_construct, std::forward_as_tuple(column_name_lower_case),
      std::forward_as_tuple(column_name, column_name_lower_case, data_type_in,
                            max_length_in));
}

DatabaseColumnDescriptionsPostgresql::DatabaseColumnDescriptionsPostgresql(
    BareosDb* db,
    const std::string& table_name)
    : DatabaseColumnDescriptions(db)
{
  std::string query{
      "select column_name, data_type, "
      "character_maximum_length from INFORMATION_SCHEMA.COLUMNS where "
      "table_name = '"};
  query += table_name;
  query += "'";

  std::cout << "--> " << table_name << std::endl;

  SelectColumnDescriptionsAndAddToMap(query);
}

DatabaseColumnDescriptionsMysql::DatabaseColumnDescriptionsMysql(
    BareosDb* db,
    const std::string& table_name)
    : DatabaseColumnDescriptions(db)
{
  std::string query{
      "select column_name, data_type, "
      "character_maximum_length from INFORMATION_SCHEMA.COLUMNS where "
      "table_name = '"};
  query += table_name;
  query += "'";
  query += " AND";
  query += " table_schema='";
  query += db->get_db_name();
  query += "'";

  SelectColumnDescriptionsAndAddToMap(query);
}
