/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2020-2021 Bareos GmbH & Co. KG

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

#ifndef BAREOS_DIRD_DBCOPY_DATABASE_IMPORT_MYSQL_H_
#define BAREOS_DIRD_DBCOPY_DATABASE_IMPORT_MYSQL_H_

#include "include/bareos.h"
#include "dird/dbcopy/database_import.h"

class BareosDb;
class DatabaseConnection;
class DatabaseExport;
class DatabaseTableDescriptions;
struct ResultHandlerContext;
class Progress;

class DatabaseImportMysql : public DatabaseImport {
 public:
  DatabaseImportMysql(const DatabaseConnection& db_connection,
                      std::size_t maximum_amount_of_rows);

  void ExportTo(DatabaseExport& exporter) override;

 private:
  static int ResultHandlerCopy(void* ctx, int fields, char** row);
  static void FillRowWithDatabaseResult(ResultHandlerContext* r,
                                        int fields,
                                        char** row);

  void RunQuerySelectAllRows(DB_RESULT_HANDLER* ResultHandler,
                             DatabaseExport& exporter);
  std::size_t limit_amount_of_rows_{};
};


#endif  // BAREOS_DIRD_DBCOPY_DATABASE_IMPORT_MYSQL_H_
