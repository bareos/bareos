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

#ifndef BAREOS_SRC_DIRD_DBCOPY_DATABASE_IMPORT_MYSQL_H_
#define BAREOS_SRC_DIRD_DBCOPY_DATABASE_IMPORT_MYSQL_H_

#include "include/bareos.h"
#include "dird/dbcopy/database_import.h"

class BareosDb;
class DatabaseConnection;
class DatabaseExport;
class DatabaseTableDescriptions;
class ResultHandlerContext;

class DatabaseImportMysql : public DatabaseImport {
 public:
  DatabaseImportMysql(const DatabaseConnection& db_connection,
                      std::size_t maximum_amount_of_rows);

  void ExportTo(DatabaseExport& exporter) override;
  void CompareWith(DatabaseExport& exporter) override;

 private:
  static int ResultHandlerCopy(void* ctx, int fields, char** row);
  static int ResultHandlerCompare(void* ctx, int fields, char** row);
  static void FillRowWithDatabaseResult(ResultHandlerContext* r,
                                        int fields,
                                        char** row);

  void RunQuerySelectAllRows(DB_RESULT_HANDLER* result_handler,
                             DatabaseExport& exporter);
  std::size_t maximum_amount_of_rows{};
};


#endif  // BAREOS_SRC_DIRD_DBCOPY_DATABASE_IMPORT_MYSQL_H_
