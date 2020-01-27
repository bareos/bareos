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
#include "dird/dbconvert/database_export.h"
#include "dird/dbconvert/database_export_postgresql.h"
#include "dird/dbconvert/database_table_descriptions.h"
#include "dird/dbconvert/row_data.h"
#include "lib/bstringlist.h"

#include <iostream>
#include <memory>

DatabaseExportPostgresql::DatabaseExportPostgresql(
    const DatabaseConnection& db_connection,
    bool clear_tables)
    : DatabaseExport(db_connection, clear_tables)
{
  if (clear_tables) {
    for (const auto& t : table_descriptions_->tables) {
      if (t.table_name != "version") {
        std::string query("DELETE ");
        query += " FROM ";
        query += t.table_name;
        if (!db_->SqlQuery(query.c_str())) {
          std::string err{"Could not delete table: "};
          err += t.table_name;
          err += " ";
          err += db_->get_errmsg();
          throw std::runtime_error(err);
        }
      }
    }
  }
}

DatabaseExportPostgresql::~DatabaseExportPostgresql() = default;

void DatabaseExportPostgresql::CopyRow(const RowData& data)
{
  std::string query{"INSERT INTO "};
  query += data.table_name;
  query += " (";
  for (const auto& r : data.row) {
    query += r.first;
    query += ", ";
  }
  query.erase(query.end() - 2);
  query += ")";

  query += " VALUES (";

  for (const auto& r : data.row) {
    query += "'";
    query += r.second.data_pointer;
    query += "',";
  }

  query.erase(query.end() - 1);
  query += ")";
#if 0
  std::cout << query << std::endl;
#else
  if (!db_->SqlQuery(query.c_str())) {
    std::string err{"DatabaseExportPostgresql: Could not execute query: "};
    err += db_->get_errmsg();
    throw std::runtime_error(err);
  }
#endif
}

void DatabaseExportPostgresql::CopyStart()
{
  SelectSequenceSchema();

  if (!db_->SqlQuery("BEGIN")) { throw std::runtime_error(db_->get_errmsg()); }
}

struct SequenceSchema {
  std::string table_name;
  std::string column_name;
  std::string sequence_name;
};
using SequenceSchemaVector = std::vector<SequenceSchema>;

int DatabaseExportPostgresql::ResultHandlerSequenceSchema(void* ctx,
                                                          int fields,
                                                          char** row)
{
  if (fields != 3) {
    throw std::runtime_error("DatabaseExportPostgresql: Wrong number of rows");
  }

  SequenceSchema s;
  s.table_name = row[0];
  s.column_name = row[1];

  BStringList l(row[2], "'");
  if (l.size() != 3) {
    throw std::runtime_error(
        "DatabaseExportPostgresql: Wrong column_default syntax");
  }
  s.sequence_name = l[1];

  SequenceSchemaVector* v = static_cast<SequenceSchemaVector*>(ctx);
  v->push_back(s);
  return 0;
}

void DatabaseExportPostgresql::SelectSequenceSchema()
{
  const std::string query{
      "select table_name, column_name,"
      " column_default from information_schema.columns where table_schema ="
      " 'public' and column_default like 'nextval(%';"};

  if (!db_->SqlQuery(query.c_str(), ResultHandlerSequenceSchema,
                     &sequence_schema_vector_)) {
    std::string err{"DatabaseExportPostgresql: Could not change sequence: "};
    err += db_->get_errmsg();
    throw std::runtime_error(err);
  }
}

static void UpdateSequences(
    BareosDb* db,
    const DatabaseExportPostgresql::SequenceSchemaVector&
        sequence_schema_vector)
{
  for (const auto& s : sequence_schema_vector) {
    std::string sequence_schema_query{"select setval(' "};
    sequence_schema_query += s.sequence_name;
    sequence_schema_query += "', (select max(";
    sequence_schema_query += s.column_name;
    sequence_schema_query += ") from ";
    sequence_schema_query += s.table_name;
    sequence_schema_query += "))";
    if (!db->SqlQuery(sequence_schema_query.c_str())) {
      throw std::runtime_error(
          "DatabaseExportPostgresql: Could not set sequence");
    }
#if 0
    std::cout << "Updating sequence for table: " << s.table_name <<
    std::endl;
#endif
  }
}

void DatabaseExportPostgresql::CopyEnd()
{
  UpdateSequences(db_, sequence_schema_vector_);

  if (!db_->SqlQuery("COMMIT")) { throw std::runtime_error(db_->get_errmsg()); }
}

void DatabaseExportPostgresql::CompareStart() {}

void DatabaseExportPostgresql::CompareRow(const RowData& data)
{
  std::string query{"SELECT "};
  for (const auto& r : data.row) {
    query += r.first;
    query += ", ";
  }
  query.resize(query.size() - 2);

  query += " FROM ";
  query += data.table_name;

#if 0
  std::cout << query << std::endl;
#else
  RowData& no_const_rowdata{const_cast<RowData&>(data)};
  if (!db_->SqlQuery(query.c_str(), ResultHandlerCompare, &no_const_rowdata)) {
    std::string err{
        "DatabaseExportPostgresql (compare): Could not execute query: "};
    err += db_->get_errmsg();
    throw std::runtime_error(err);
  }
#endif
}

int DatabaseExportPostgresql::ResultHandlerCompare(void* ctx,
                                                   int fields,
                                                   char** row)
{
  const RowData* data = static_cast<RowData*>(ctx);
  return 1;
}

void DatabaseExportPostgresql::CompareEnd() {}
