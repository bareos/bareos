/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2016 Planets Communications B.V.
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
#include "dird/dbconvert/database_row_descriptions.h"

#include <iostream>

DatabaseRowDescriptions::DatabaseRowDescriptions(BareosDb* db) : db_{db} {}

void DatabaseRowDescriptions::SelectTableDescriptions(
    const std::string& sql_query,
    DB_RESULT_HANDLER* ResultHandler)
{
  if (!db_->SqlQuery(sql_query.c_str(), ResultHandler, this)) {
    std::string err{"Could not select table names: "};
    err += sql_query;
    throw std::runtime_error(err);
  }
}

int DatabaseRowDescriptionsPostgresql::ResultHandler(void* ctx,
                                                     int fields,
                                                     char** row)
{
  auto t = static_cast<DatabaseRowDescriptions*>(ctx);

  t->row_descriptions.emplace_back(std::make_unique<RowDescriptionPostgresql>(
      row[RowIndex::kColumnName], row[RowIndex::kDataType],
      row[RowIndex::kCharMaxLenght]));

  return 0;
}

DatabaseRowDescriptionsPostgresql::DatabaseRowDescriptionsPostgresql(
    BareosDb* db,
    const std::string& table_name)
    : DatabaseRowDescriptions(db)
{
  std::string sql{
      "select column_name, data_type, "
      "character_maximum_length from INFORMATION_SCHEMA.COLUMNS where "
      "table_name = '"};
  sql += table_name;
  sql += "'";
  SelectTableDescriptions(sql, ResultHandler);
}

int DatabaseRowDescriptionsMysql::ResultHandler(void* ctx,
                                                int fields,
                                                char** row)
{
  auto t = static_cast<DatabaseRowDescriptions*>(ctx);

  t->row_descriptions.emplace_back(std::make_unique<RowDescriptionMysql>(
      row[RowIndex::kColumnName], row[RowIndex::kDataType],
      row[RowIndex::kCharMaxLenght]));

  return 0;
}

DatabaseRowDescriptionsMysql::DatabaseRowDescriptionsMysql(
    BareosDb* db,
    const std::string& table_name)
    : DatabaseRowDescriptions(db)
{
  std::string sql{
      "select column_name, data_type, "
      "character_maximum_length from INFORMATION_SCHEMA.COLUMNS where "
      "table_name = '"};
  sql += table_name;
  sql += "'";
  SelectTableDescriptions(sql, ResultHandler);
}
