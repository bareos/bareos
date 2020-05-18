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
#include "cats/cats.h"
#include "database_table_descriptions.h"
#include "dird/dbcopy/database_column_descriptions.h"
#include "lib/util.h"

#include <algorithm>
#include <iostream>
#include <memory>
#include <set>

using TableNames = std::vector<std::string>;

void DatabaseTableDescriptions::SelectTableNames(const std::string& sql_query,
                                                 TableNames& table_names_out)
{
  if (!db_->SqlQuery(sql_query.c_str(),
                     DatabaseTableDescriptions::ResultHandler,
                     std::addressof(table_names_out))) {
    std::string err{"Could not select table names: "};
    err += sql_query;
    throw std::runtime_error(err);
  }
}

int DatabaseTableDescriptions::ResultHandler(void* ctx,
                                             int /*fields*/,
                                             char** row)
{
  auto table_names_out = static_cast<TableNames*>(ctx);
  if (row[0] != nullptr) { table_names_out->emplace_back(row[0]); }
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
  for (const auto& table_name : table_names) {
    DatabaseColumnDescriptionsPostgresql cdesc(db, table_name);

    std::string table_name_lower_case;
    StringToLowerCase(table_name_lower_case, table_name);

    tables.emplace(  // std::map sorts keys ascending by default
        std::piecewise_construct, std::forward_as_tuple(table_name_lower_case),
        std::forward_as_tuple(table_name, table_name_lower_case,
                              std::move(cdesc.column_descriptions)));
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

  for (const auto& table_name : table_names) {
    DatabaseColumnDescriptionsMysql cdesc(db, table_name);

    std::string table_name_lower_case;
    StringToLowerCase(table_name_lower_case, table_name);

    tables.emplace(  // std::map sorts keys ascending by default
        std::piecewise_construct, std::forward_as_tuple(table_name_lower_case),
        std::forward_as_tuple(table_name, table_name_lower_case,
                              std::move(cdesc.column_descriptions)));
  }
}

const TableDescription* DatabaseTableDescriptions::GetTableDescription(
    const std::string& table_name_lower_case) const
{
  try {
    const auto& t = tables.at(table_name_lower_case);
    return std::addressof(t);
  } catch (std::out_of_range&) {
    return nullptr;
  }
}

const ColumnDescription* DatabaseTableDescriptions::GetColumnDescription(
    const std::string& table_name_lower_case,
    const std::string& column_name_lower_case) const
{
  const auto table_description = GetTableDescription(table_name_lower_case);

  if (table_description == nullptr) {
    std::cout << "Could not get table description for table: "
              << table_name_lower_case << std::endl;
    return nullptr;
  }
  try {
    const auto& c =
        table_description->column_descriptions.at(column_name_lower_case);
    return std::addressof(c);
  } catch (std::out_of_range&) {
    std::cout << "Could not get columnd description for column: "
              << column_name_lower_case << std::endl;
    return nullptr;
  }
}

void DatabaseTableDescriptions::SetAllConverterCallbacks(
    const ColumnDescription::DataTypeConverterMap& map)
{
  for (auto& t : tables) {
    for (auto& c : t.second.column_descriptions) {
      c.second.RegisterConverterCallbackFromMap(map);
    }
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
