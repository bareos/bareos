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
#include "database_table_descriptions.h"
#include "dird/dbconvert/database_column_descriptions.h"
#include "cats/cats.h"

#include <algorithm>
#include <iostream>
#include <memory>

using TableNames = std::vector<std::string>;

void DatabaseTableDescriptions::SelectTableNames(const std::string& sql_query,
                                                 TableNames& table_names)
{
  if (!db_->SqlQuery(sql_query.c_str(),
                     DatabaseTableDescriptions::ResultHandler,
                     std::addressof(table_names))) {
    std::string err{"Could not select table names: "};
    err += sql_query;
    throw std::runtime_error(err);
  }
  if (!table_names.empty()) {
    std::sort(table_names.begin(), table_names.end());
  }
}

int DatabaseTableDescriptions::ResultHandler(void* ctx, int fields, char** row)
{
  auto table_names = static_cast<TableNames*>(ctx);
  if (row[0] != nullptr) { table_names->emplace_back(row[0]); }
  return 0;
}

DatabaseTablesPostgresql::DatabaseTablesPostgresql(BareosDb* db)
    : DatabaseTableDescriptions(db)
{
  std::string query{
      "SELECT table_name FROM INFORMATION_SCHEMA.TABLES WHERE "
      "table_schema='public'"};

  std::vector<std::string> table_names;
  SelectTableNames(query, table_names);

  for (auto t : table_names) {
    DatabaseColumnDescriptionsPostgresql p(db, t);
    tables.emplace_back(std::move(t), std::move(p.column_descriptions));
  }
}

DatabaseTablesMysql::DatabaseTablesMysql(BareosDb* db)
    : DatabaseTableDescriptions(db)
{
  std::string query{
      "SELECT TABLE_NAME FROM INFORMATION_SCHEMA.TABLES WHERE TABLE_SCHEMA='"};
  query += db->get_db_name();
  query += "'";

  std::vector<std::string> table_names;
  SelectTableNames(query, table_names);

  for (auto t : table_names) {
    DatabaseColumnDescriptionsMysql p(db, t);
    tables.emplace_back(std::move(t), std::move(p.column_descriptions));
  }
}

std::unique_ptr<DatabaseTableDescriptions> DatabaseTableDescriptions::Create(
    const DatabaseConnection& connection)
{
  switch (connection.db_type) {
    case DatabaseType::Enum::kMysql:
      return std::make_unique<DatabaseTablesMysql>(connection.db);
      break;
    case DatabaseType::Enum::kPostgresql:
      return std::make_unique<DatabaseTablesPostgresql>(connection.db);
      break;
    default:
      throw std::runtime_error("Database type unknown");
      return std::unique_ptr<DatabaseTableDescriptions>();
  }
}
