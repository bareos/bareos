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

#ifndef BAREOS_SRC_DIRD_DBCOPY_DATABASE_EXPORT_POSTGRESQL_H_
#define BAREOS_SRC_DIRD_DBCOPY_DATABASE_EXPORT_POSTGRESQL_H_

#include "include/bareos.h"
#include "dird/dbcopy/database_export.h"

class DatabaseConnection;
class DatabaseTableDescriptions;

class DatabaseExportPostgresql : public DatabaseExport {
 public:
  DatabaseExportPostgresql(const DatabaseConnection& db_connection,
                           InsertMode mode = InsertMode::kSqlCopy,
                           bool clear_tables = false);
  ~DatabaseExportPostgresql();

  bool StartTable(const std::string& table_name) override;
  void EndTable(const std::string& table_name) override;

  void CopyStart() override;
  void CopyRow(RowData& source_data_row,
               std::string& insert_mode_message) override;
  void CopyEnd() override;

 public:
  struct SequenceSchema {
    std::string table_name;
    std::string column_name;
    std::string sequence_name;
  };

  using SequenceSchemaVector = std::vector<SequenceSchema>;

 private:
  bool transaction_open_{false};
  InsertMode insert_mode_{};
  std::string insert_mode_message_;
  SequenceSchemaVector sequence_schema_vector_;

  void SelectSequenceSchema();
  bool TableExists(const std::string& table_name,
                   std::vector<std::string>& column_names);
  bool UseCopyInsertion();
  void DoCopyInsertion(RowData& source_data_row);
  void DoInsertInsertion(RowData& source_data_row);
  static int ResultHandlerSequenceSchema(void* ctx, int fields, char** row);
  static int ResultHandlerCompare(void* ctx, int fields, char** row);
};
#endif  // BAREOS_SRC_DIRD_DBCOPY_DATABASE_EXPORT_POSTGRESQL_H_
