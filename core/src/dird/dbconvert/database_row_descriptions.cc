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
#include "cats/cats.h"
#include "database_row_descriptions.h"

#include <iostream>

DatabaseRowDescriptions::DatabaseRowDescriptions(BareosDb* db) : db_{db} {}

void DatabaseRowDescriptions::SelectTableDescriptions(
    const std::string& sql_query)
{
  if (!db_->SqlQuery(sql_query.c_str(), DatabaseRowDescriptions::ResultHandler,
                     this)) {
    std::string err{"Could not select table names: "};
    err += sql_query;
    throw std::runtime_error(err);
  }
}

int DatabaseRowDescriptions::ResultHandler(void* ctx, int fields, char** row)
{
  RowDescription td;
  td.column_name = row[RowIndex::kColumnName];
  td.data_type = row[RowIndex::kDataType];
  std::string field;
  try {
    const char* p{row[RowIndex::kCharMaxLenght]};
    field = p ? p : "";
    if (!field.empty()) { td.character_maximum_length = std::stoul(field); }
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

  auto t = static_cast<DatabaseRowDescriptions*>(ctx);

  t->row_descriptions.emplace_back(td);
  return 0;
}

DatabaseTableDescriptionPostgresl::DatabaseTableDescriptionPostgresl(
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
  SelectTableDescriptions(sql);
}

DatabaseTableDescriptionMysql::DatabaseTableDescriptionMysql(
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
  SelectTableDescriptions(sql);
}
