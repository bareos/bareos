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

#ifndef BAREOS_SRC_DIRD_DBCONVERT_DATABASE_EXPORT_POSTGRESQL_H_
#define BAREOS_SRC_DIRD_DBCONVERT_DATABASE_EXPORT_POSTGRESQL_H_

class DatabaseConnection;
class DatabaseTableDescriptions;

class DatabaseExportPostgresql : public DatabaseExport {
 public:
  DatabaseExportPostgresql(const DatabaseConnection& db_connection,
                           bool clear_tables = false);
  ~DatabaseExportPostgresql();

  void CopyStart() override;
  void CopyRow(const RowData& data) override;
  void CopyEnd() override;

  virtual void CompareStart() override;
  virtual void CompareRow(const RowData& data) override;
  virtual void CompareEnd() override;

 private:
  struct SequenceSchema {
    std::string table_name;
    std::string column_name;
    std::string sequence_name;
  };
  using SequenceSchemaVector = std::vector<SequenceSchema>;
  SequenceSchemaVector sequence_schema_vector_;

  void SelectSequenceSchema();
  static int ResultHandlerSequenceSchema(void* ctx, int fields, char** row);
  static int ResultHandlerCompare(void* ctx, int fields, char** row);
};
#endif  // BAREOS_SRC_DIRD_DBCONVERT_DATABASE_EXPORT_POSTGRESQL_H_
