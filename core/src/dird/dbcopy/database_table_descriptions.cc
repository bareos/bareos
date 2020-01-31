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
#include "dird/dbcopy/database_column_descriptions.h"
#include "cats/cats.h"

#include <algorithm>
#include <iostream>
#include <memory>
#include <set>

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

  std::cout << "getting column descriptions..." << std::endl;
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

static void ToLowerCase(const std::string& i1,
                        const std::string& i2,
                        std::string& o1,
                        std::string& o2)
{
  o1.clear();
  o2.clear();
  std::transform(i1.cbegin(), i1.cend(), std::back_inserter(o1), ::tolower);
  std::transform(i2.cbegin(), i2.cend(), std::back_inserter(o2), ::tolower);
}

const DatabaseTableDescriptions::TableDescription*
DatabaseTableDescriptions::GetTableDescription(
    const std::string& table_name) const
{
  auto tr = std::find_if(tables.begin(), tables.end(),
                         [&table_name](const TableDescription& t) {
                           std::string l1, l2;
                           ToLowerCase(table_name, t.table_name, l1, l2);
                           return l1 == l2;
                         });
  return tr == tables.end() ? nullptr : std::addressof(*tr);
}

const ColumnDescription* DatabaseTableDescriptions::GetColumnDescription(
    const std::string& table_name,
    const std::string& column_name) const
{
  const DatabaseTableDescriptions::TableDescription* table =
      GetTableDescription(table_name);
  if (table == nullptr) {
    std::cout << "Could not get table description for table: " << table_name
              << std::endl;
    return nullptr;
  }
  auto c = std::find_if(
      table->column_descriptions.cbegin(), table->column_descriptions.cend(),
      [&column_name](const std::unique_ptr<ColumnDescription>& c) {
        std::string l1, l2;
        ToLowerCase(column_name, c->column_name, l1, l2);
        if (l1 == l2) { return true; }
        return false;
      });
  return (c == table->column_descriptions.cend()) ? nullptr : c->get();
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
