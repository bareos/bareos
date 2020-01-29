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
#include "include/make_unique.h"
#include "cats/cats.h"
#include "dird/dbcopy/database_column_descriptions.h"

#include <algorithm>
#include <iostream>

DatabaseColumnDescriptions::DatabaseColumnDescriptions(BareosDb* db) : db_{db}
{
}

void DatabaseColumnDescriptions::SelectColumnDescriptions(
    const std::string& sql_query,
    DB_RESULT_HANDLER* ResultHandler)
{
  if (!db_->SqlQuery(sql_query.c_str(), ResultHandler, this)) {
    std::string err{"Could not select table names: "};
    err += sql_query;
    throw std::runtime_error(err);
  }
  std::sort(column_descriptions.begin(), column_descriptions.end(),
            [](const std::unique_ptr<ColumnDescription>& v1,
               const std::unique_ptr<ColumnDescription>& v2) {
              return v1->column_name < v2->column_name;
            });
}

int DatabaseColumnDescriptionsPostgresql::ResultHandler(void* ctx,
                                                        int fields,
                                                        char** row)
{
  auto t = static_cast<DatabaseColumnDescriptions*>(ctx);

  t->column_descriptions.emplace_back(
      std::make_unique<ColumnDescriptionPostgresql>(
          row[RowIndex::kColumnName], row[RowIndex::kDataType],
          row[RowIndex::kCharMaxLenght]));

  return 0;
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

  std::cout << "Collecting column descriptions for: " << table_name
            << std::endl;

  SelectColumnDescriptions(query, ResultHandler);
}

int DatabaseColumnDescriptionsMysql::ResultHandler(void* ctx,
                                                   int fields,
                                                   char** row)
{
  auto t = static_cast<DatabaseColumnDescriptions*>(ctx);

  t->column_descriptions.emplace_back(std::make_unique<ColumnDescriptionMysql>(
      row[RowIndex::kColumnName], row[RowIndex::kDataType],
      row[RowIndex::kCharMaxLenght]));

  return 0;
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
  SelectColumnDescriptions(query, ResultHandler);
}
