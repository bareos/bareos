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

#ifndef BAREOS_SRC_DIRD_DBCONVERT_DATABASE_TABLES_H_
#define BAREOS_SRC_DIRD_DBCONVERT_DATABASE_TABLES_H_

#include "dird/dbconvert/database_row_descriptions.h"
#include "dird/dbconvert/database_connection.h"
#include "include/make_unique.h"

#include <string>
#include <vector>

class BareosDb;

class DatabaseTables {
 public:
  class TableDescription {
   public:
    TableDescription(std::string&& t, std::vector<RowDescription>&& r)
        : table_name(t), row_descriptions(r){};

    std::string table_name;
    std::vector<RowDescription> row_descriptions;
  };

  std::vector<TableDescription> tables;

  static std::unique_ptr<DatabaseTables> Create(
      const DatabaseConnection& connection);

 protected:
  DatabaseTables(BareosDb* db) : db_{db} {}

  void SelectTableNames(const std::string& sql_query,
                        std::vector<std::string>& tables_names);

 private:
  BareosDb* db_{};
  static int ResultHandler(void* ctx, int fields, char** row);
};

class DatabaseTablesPostgresql : public DatabaseTables {
 public:
  DatabaseTablesPostgresql(BareosDb* db);
};

class DatabaseTablesMysql : public DatabaseTables {
 public:
  DatabaseTablesMysql(BareosDb* db);
};

#endif  // BAREOS_SRC_DIRD_DBCONVERT_DATABASE_TABLES_H_
