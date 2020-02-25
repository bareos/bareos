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

static void no_conversion(BareosDb* db, DatabaseField& fd)
{
  fd.data_pointer = fd.data_pointer ? fd.data_pointer : "";
}

static void timestamp_conversion_postgresql(BareosDb* db, DatabaseField& fd)
{
  static const char* dummy_timepoint = "1970-01-01 00:00:00";
  if (!fd.data_pointer) {
    fd.data_pointer = dummy_timepoint;
  } else if (fd.data_pointer[0] == '0') {
    fd.data_pointer = dummy_timepoint;
  } else if (strlen(fd.data_pointer) == 0) {
    fd.data_pointer = dummy_timepoint;
  }
}

static void string_conversion_postgresql(BareosDb* db, DatabaseField& fd)
{
  if (fd.data_pointer) {
    std::size_t len{strlen(fd.data_pointer)};
    fd.converted_data.resize(len * 2 + 1);
    db->EscapeString(nullptr, fd.converted_data.data(), fd.data_pointer, len);
    fd.data_pointer = fd.converted_data.data();
  } else {
    fd.data_pointer = "";
  }
}

static void bytea_conversion_postgresql(BareosDb* db, DatabaseField& fd)
{
  std::size_t new_len{};
  std::size_t old_len = fd.size_of_restore_object;

  auto old = reinterpret_cast<const unsigned char*>(fd.data_pointer);

  auto obj = db->EscapeObject(old, old_len, new_len);

  fd.converted_data.resize(new_len);
  memcpy(fd.converted_data.data(), obj, new_len);

  db->FreeEscapedObjectMemory(obj);

  fd.data_pointer = fd.converted_data.data();
}

static const ColumnDescription::DataTypeConverterMap
    db_export_converter_map_postgres_sql_insert{
        {"bigint", no_conversion},
        {"bytea", bytea_conversion_postgresql},
        {"character", string_conversion_postgresql},
        {"integer", no_conversion},
        {"numeric", no_conversion},
        {"smallint", no_conversion},
        {"text", string_conversion_postgresql},
        {"timestamp without time zone", timestamp_conversion_postgresql}};


static const ColumnDescription::DataTypeConverterMap
    db_export_converter_map_postgres_sql_copy{
        {"bigint", no_conversion},
        {"bytea", bytea_conversion_postgresql},
        {"character", no_conversion},
        {"integer", no_conversion},
        {"numeric", no_conversion},
        {"smallint", no_conversion},
        {"text", no_conversion},
        {"timestamp without time zone", timestamp_conversion_postgresql}};

bool DatabaseExportPostgresql::UseCopyInsertion()
{
  switch (insert_mode_) {
    case DatabaseExport::InsertMode::kSqlCopy:
      return true;
    default:
    case DatabaseExport::InsertMode::kSqlInsert:
      return false;
  }
}

static void ClearTable(BareosDb* db, const std::string& table_name)
{
  if (table_name != "version") {
    std::cout << "Deleting contents of table: " << table_name << std::endl;
    std::string query("DELETE ");
    query += " FROM ";
    query += table_name;
    if (!db->SqlQuery(query.c_str())) {
      std::string err{"Could not delete table: "};
      err += table_name;
      err += " ";
      err += db->get_errmsg();
      throw std::runtime_error(err);
    }
  }
}

DatabaseExportPostgresql::DatabaseExportPostgresql(
    const DatabaseConnection& db_connection,
    DatabaseExport::InsertMode mode,
    bool clear_tables)
    : DatabaseExport(db_connection), insert_mode_(mode)
{
  switch (insert_mode_) {
    case DatabaseExport::InsertMode::kSqlCopy:
      insert_mode_message_ = "";
      table_descriptions_->SetAllConverterCallbacks(
          db_export_converter_map_postgres_sql_copy);
      break;
    case DatabaseExport::InsertMode::kSqlInsert:
      insert_mode_message_ = ", using slow insert mode";
      table_descriptions_->SetAllConverterCallbacks(
          db_export_converter_map_postgres_sql_insert);
      break;
    default:
      throw std::runtime_error("Unknown type for export");
  }

  if (clear_tables) {
    for (const auto& t : table_descriptions_->tables) {
      ClearTable(db_, t.table_name);
    }
  }
}

DatabaseExportPostgresql::~DatabaseExportPostgresql()
{
  if (transaction_open_) { db_->SqlQuery("ROLLBACK"); }
}

void DatabaseExportPostgresql::DoCopyInsertion(RowData& source_data_row)
{
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

    if (i < source_data_row.data_fields.size()) {
      column_description->converter(db_, source_data_row.data_fields[i]);
    } else {
      throw std::runtime_error("Row number does not match column description");
    }
  }
  if (!db_->SqlCopyInsert(source_data_row.data_fields)) {
    std::string err{"DatabaseExportPostgresql: Could not execute query: "};
    err += db_->get_errmsg();
    throw std::runtime_error(err);
  }
}

void DatabaseExportPostgresql::DoInsertInsertion(RowData& source_data_row)
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

    if (i < source_data_row.data_fields.size()) {
      query_values += "'";
      column_description->converter(db_, source_data_row.data_fields[i]);
      query_values += source_data_row.data_fields[i].data_pointer;
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

void DatabaseExportPostgresql::CopyRow(RowData& source_data_row,
                                       std::string& insert_mode_message)
{
  insert_mode_message = insert_mode_message_;

  if (UseCopyInsertion()) {
    DoCopyInsertion(source_data_row);
  } else {
    DoInsertInsertion(source_data_row);
  }
}

void DatabaseExportPostgresql::CopyStart()
{
  // runs before first table
  SelectSequenceSchema();
}

static void UpdateSequences(
    BareosDb* db,
    const DatabaseExportPostgresql::SequenceSchemaVector&
        sequence_schema_vector,
    const std::string& table_name)
{
  for (const auto& s : sequence_schema_vector) {
    std::string table_name_lower_case;
    std::transform(table_name.cbegin(), table_name.cend(),
                   std::back_inserter(table_name_lower_case), ::tolower);
    if (s.table_name == table_name_lower_case) {
      std::string sequence_schema_query{"select setval(' "};
      sequence_schema_query += s.sequence_name;
      sequence_schema_query += "', (select max(";
      sequence_schema_query += s.column_name;
      sequence_schema_query += ") from ";
      sequence_schema_query += table_name_lower_case;
      sequence_schema_query += "))";
      std::cout << "--> updating sequence" << std::endl;
      if (!db->SqlQuery(sequence_schema_query.c_str())) {
        throw std::runtime_error(
            "DatabaseExportPostgresql: Could not set sequence");
      }
    }
  }
}

void DatabaseExportPostgresql::CopyEnd() {}

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

bool DatabaseExportPostgresql::TableExists(
    const std::string& table_name,
    std::vector<std::string>& column_names)
{
  auto found = table_descriptions_->GetTableDescription(table_name);
  if (found == nullptr) { return false; }

  for (const auto& col : found->column_descriptions) {
    column_names.push_back(col->column_name);
  }

  return true;
}

bool DatabaseExportPostgresql::StartTable(const std::string& table_name)
{
  std::cout << "====== table " << table_name << " ======" << std::endl;
  std::cout << "--> checking destination table..." << std::endl;

  std::vector<std::string> column_names;

  if (!TableExists(table_name, column_names)) {
    std::cout << "--> destination table does not exist" << std::endl;
    return false;
  }

  if (!TableIsEmtpy(db_, table_name)) {
    std::cout << "--> destination table is not empty" << std::endl;
    return false;
  }

  if (db_->SqlQuery("BEGIN")) { transaction_open_ = true; }

  if (UseCopyInsertion()) {
    if (!db_->SqlCopyStart(table_name, column_names)) {
      std::string err{"Could not start Sql Copy: "};
      err += db_->get_errmsg();
      throw std::runtime_error(err);
    }
  }

  return true;
}

void DatabaseExportPostgresql::EndTable(const std::string& table_name)
{
  if (UseCopyInsertion()) {
    if (!db_->SqlCopyEnd()) {
      std::string err{"Could not end Sql Copy: "};
      err += db_->get_errmsg();
      throw std::runtime_error(err);
    }
  } else if (table_name == "RestoreObject") {
    std::string query{
        "UPDATE restoreobject SET objectlength=length(restoreobject)"};
    if (!db_->SqlQuery(query.c_str())) {
      std::string err{"Could not update RestoreObjects: "};
      err += db_->get_errmsg();
      throw std::runtime_error(err);
    }
  }

  UpdateSequences(db_, sequence_schema_vector_, table_name);

  if (transaction_open_) {
    db_->SqlQuery("COMMIT");
    transaction_open_ = false;
  }
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
