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
#include "database_tables.h"
#include "cats/cats.h"

#include <algorithm>
#include <iostream>

DatabaseTables::DatabaseTables(BareosDb* db) : db_{db} {}

void DatabaseTables::SelectTableNames(const std::string& sql_query)
{
  if (!db_->SqlQuery(sql_query.c_str(), DatabaseTables::ResultHandler, this)) {
    std::string err{"Could not select table names: "};
    err += sql_query;
    throw std::runtime_error(err);
  }
  if (!tables_names.empty()) {
    std::sort(tables_names.begin(), tables_names.end());
  }
}

int DatabaseTables::ResultHandler(void* ctx, int fields, char** row)
{
  auto t = static_cast<DatabaseTables*>(ctx);
  if (row[0] != nullptr) { t->tables_names.emplace_back(row[0]); }
  return 0;
}

DatabaseTablesPostgresql::DatabaseTablesPostgresql(BareosDb* db)
    : DatabaseTables(db)
{
  std::string sql{
      "SELECT table_name FROM INFORMATION_SCHEMA.TABLES WHERE "
      "table_schema='public'"};
  SelectTableNames(sql);
}

DatabaseTablesMysql::DatabaseTablesMysql(BareosDb* db) : DatabaseTables(db)
{
  std::string sql{
      "SELECT TABLE_NAME FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA='"};
  sql += db->get_db_name();
  sql += "'";
  SelectTableNames(sql);
}
