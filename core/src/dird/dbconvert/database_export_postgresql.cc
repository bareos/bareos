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

DatabaseExportPostgresql::~DatabaseExportPostgresql()
{
  if (transaction_) { db_->SqlQuery("ROLLBACK"); }
}

void DatabaseExportPostgresql::CopyRow(const RowData& data)
{
  std::string query{"INSERT INTO "};
  query += data.table_name;
  query += " (";
  for (const auto& c : data.column_descriptions) {
    query += c->column_name;
    query += ", ";
  }
  query.resize(query.size() - 2);
  query += ")";

  query += " VALUES (";

  for (const auto& r : data.row) {
    query += "'";
    query += r.data_pointer;
    query += "',";
  }

  query.resize(query.size() - 1);
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

void DatabaseExportPostgresql::CopyStart() { SelectSequenceSchema(); }

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
}

void DatabaseExportPostgresql::CursorStartTable(const std::string& table_name)
{
  const DatabaseTableDescriptions::TableDescription* table{
      table_descriptions_->GetTableDescription(table_name)};

  if (table == nullptr) {
    std::string err{
        "DatabaseExportPostgresql: Could not get table description for: "};
    err += table_name;
    throw std::runtime_error(err);
  }

  std::string query{"DECLARE curs1 NO SCROLL CURSOR FOR SELECT "};

  for (const auto& c : table->column_descriptions) {
    query += c->column_name;
    query += ", ";
  }

  query.resize(query.size() - 2);
  query += " FROM ";
  query += table_name;

  if (!db_->SqlQuery(query.c_str())) {
    std::string err{
        "DatabaseExportPostgresql (cursor): Could not execute query: "};
    err += db_->get_errmsg();
    throw std::runtime_error(err);
  }
}

void DatabaseExportPostgresql::StartTable()
{
  if (db_->SqlQuery("BEGIN")) {
    transaction_ = true;
    start_new_table = true;
  }
}

void DatabaseExportPostgresql::EndTable()
{
  if (transaction_) {
    db_->SqlQuery("COMMIT");
    transaction_ = false;
  }
}

void DatabaseExportPostgresql::CompareRow(const RowData& data)
{
  if (start_new_table) {
    CursorStartTable(data.table_name);
    start_new_table = false;
  }

  std::string query{"FETCH NEXT FROM curs1"};

  RowData& rd{const_cast<RowData&>(data)};

  if (!db_->SqlQuery(query.c_str(), ResultHandlerCompare, &rd)) {
    std::string err{
        "DatabaseExportPostgresql (compare): Could not execute query: "};
    err += db_->get_errmsg();
    throw std::runtime_error(err);
  }
}

int DatabaseExportPostgresql::ResultHandlerCompare(void* ctx,
                                                   int fields,
                                                   char** row)
{
  const RowData* rd{static_cast<RowData*>(ctx)};

  for (int i = 0; i < fields; i++) {
    std::string r1{row[i]};
    std::string r2{rd->row[i].data_pointer};
    if (r1 != r2) { throw std::runtime_error("What??"); }
  }
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
