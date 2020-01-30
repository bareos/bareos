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
#include "dird/dbcopy/database_export.h"
#include "dird/dbcopy/database_export_postgresql.h"
#include "dird/dbcopy/database_table_descriptions.h"
#include "dird/dbcopy/row_data.h"
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
        std::cout << "Deleting contents of table: " << t.table_name
                  << std::endl;
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

void DatabaseExportPostgresql::CopyRow(RowData& source_data_row)
{
  std::string query_into{"INSERT INTO "};
  query_into += source_data_row.table_name;
  query_into += " (";

  std::string query_values = " VALUES (";

  for (std::size_t i = 0; i < source_data_row.column_descriptions.size(); i++) {
    const ColumnDescription* column_description =
        table_descriptions_->GetColumnDescription(
            source_data_row.table_name,
            source_data_row.column_descriptions[i]->column_name);

    if (!column_description) {
      std::string err{"Could not get column description for: "};
      err += source_data_row.column_descriptions[i]->column_name;
      throw std::runtime_error(err);
    }

    query_into += column_description->column_name;
    query_into += ", ";

    if (i < source_data_row.columns.size()) {
      query_values += "'";
      column_description->db_export_converter(db_, source_data_row.columns[i]);
      query_values += source_data_row.columns[i].data_pointer;
      query_values += "',";
    } else {
      throw std::runtime_error("Row number does not match column description");
    }
  }

  query_values.resize(query_values.size() - 1);
  query_values += ")";

  query_into.resize(query_into.size() - 2);
  query_into += ")";

  query_into += query_values;

  if (!db_->SqlQuery(query_into.c_str())) {
    std::string err{"DatabaseExportPostgresql: Could not execute query: "};
    err += db_->get_errmsg();
    throw std::runtime_error(err);
  }
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
    std::cout << "Updating sequence for table: " << s.table_name << std::endl;
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

static bool TableIsEmtpy(BareosDb* db, const std::string& table_name)
{
  std::string query{"SELECT * FROM "};
  query += table_name;
  query += " LIMIT 1";

  if (!db->SqlQuery(query.c_str())) {
    std::string err{"DatabaseExportPostgresql: Could not select table: "};
    err += table_name;
    throw std::runtime_error(err);
  }

  if (db->SqlNumRows() > 0) { return false; }
  return true;
}

bool DatabaseExportPostgresql::TableExists(const std::string& table_name)
{
  auto found = table_descriptions_->GetTableDescription(table_name);
  if (found == nullptr) { return false; }
  return true;
}

bool DatabaseExportPostgresql::StartTable(const std::string& table_name)
{
  std::cout << "====== table " << table_name << " ======" << std::endl;
  std::cout << "--> checking destination table..." << std::endl;
  if (!TableExists(table_name)) {
    std::cout << "--> destination table does not exist" << std::endl;
    return false;
  }

  if (!TableIsEmtpy(db_, table_name)) {
    std::cout << "--> destination table is not empty" << std::endl;
    return false;
  }

  if (db_->SqlQuery("BEGIN")) {
    transaction_ = true;
    start_new_table = true;
  }

  return true;
}

void DatabaseExportPostgresql::EndTable(const std::string& table_name)
{
  if (table_name == "RestoreObject") {
    std::string query{
        "UPDATE restoreobject SET objectlength=length(restoreobject)"};
    if (!db_->SqlQuery(query.c_str())) {
      std::string err{"Could not update RestoreObjects: "};
      err += db_->get_errmsg();
      throw std::runtime_error(err);
    }
  }

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

  RowData& rd(const_cast<RowData&>(data));

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
    std::string r2{rd->columns[i].data_pointer};
    if (r1 != r2) { throw std::runtime_error("Data is not same"); }
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
