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

#ifndef BAREOS_DIRD_DBCOPY_DATABASE_TABLE_DESCRIPTIONS_H_
#define BAREOS_DIRD_DBCOPY_DATABASE_TABLE_DESCRIPTIONS_H_

#include "dird/dbcopy/database_column_descriptions.h"
#include "dird/dbcopy/database_connection.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

class BareosDb;

struct TableDescription {
  TableDescription() = delete;
  TableDescription(const std::string& tn,
                   const std::string& tn_lc,
                   DatabaseColumnDescriptions::ColumnDescriptions&& c)
      : table_name{tn}, table_name_lower_case{tn_lc}, column_descriptions{c} {};

  std::string table_name;
  std::string table_name_lower_case;
  DatabaseColumnDescriptions::ColumnDescriptions column_descriptions;
};

class DatabaseTableDescriptions {
 public:
  std::map<std::string, TableDescription> tables;

 public:
  static std::unique_ptr<DatabaseTableDescriptions> Create(
      const DatabaseConnection& connection);
  virtual ~DatabaseTableDescriptions() = default;

 public:
  void SetAllConverterCallbacks(const ColumnDescription::DataTypeConverterMap&);

  const TableDescription* GetTableDescription(
      const std::string& table_name_lower_case) const;

  const ColumnDescription* GetColumnDescription(
      const std::string& table_name_lower_case,
      const std::string& column_name_lower_case) const;

 protected:
  DatabaseTableDescriptions(BareosDb* db) : db_{db} {}

  void SelectTableNames(const std::string& sql_query,
                        std::vector<std::string>& table_names_out);

 private:
  BareosDb* db_{};
  static int ResultHandler(void* ctx, int fields, char** row);
};

class DatabaseTablesPostgresql : public DatabaseTableDescriptions {
 public:
  DatabaseTablesPostgresql(BareosDb* db);
};

class DatabaseTablesMysql : public DatabaseTableDescriptions {
 public:
  DatabaseTablesMysql(BareosDb* db);
};

#endif  // BAREOS_DIRD_DBCOPY_DATABASE_TABLE_DESCRIPTIONS_H_
