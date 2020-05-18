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
#include "dird/dbcopy/database_import_mysql.h"
#include "dird/dbcopy/database_table_descriptions.h"
#include "dird/dbcopy/progress.h"
#include "dird/dbcopy/row_data.h"
#include "include/make_unique.h"

#include <cassert>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <vector>


static void no_conversion(BareosDb* /*db*/, DatabaseField& f)
{
  f.data_pointer = f.data_pointer != nullptr ? f.data_pointer : "";
}

static const ColumnDescription::DataTypeConverterMap
    db_import_converter_map_mysql{
        {"bigint", no_conversion},   {"binary", no_conversion},
        {"blob", no_conversion},     {"char", no_conversion},
        {"datetime", no_conversion}, {"decimal", no_conversion},
        {"enum", no_conversion},     {"int", no_conversion},
        {"longblob", no_conversion}, {"smallint", no_conversion},
        {"text", no_conversion},     {"timestamp", no_conversion},
        {"tinyblob", no_conversion}, {"tinyint", no_conversion},
        {"varchar", no_conversion}};

DatabaseImportMysql::DatabaseImportMysql(
    const DatabaseConnection& db_connection,
    std::size_t maximum_amount_of_rows_in)
    : DatabaseImport(db_connection)
    , limit_amount_of_rows_(maximum_amount_of_rows_in)
{
  table_descriptions_->SetAllConverterCallbacks(db_import_converter_map_mysql);
}

struct ResultHandlerContext {
  ResultHandlerContext(BareosDb* db_in,
                       RowData& d,
                       DatabaseExport& e,
                       bool is_restore_object_in,
                       size_t data_field_index_of_restore_object_in,
                       Progress& p)
      : row_data(d)
      , exporter(e)
      , db(db_in)
      , is_restore_object(is_restore_object_in)
      , data_field_index_of_restore_object{data_field_index_of_restore_object_in}
      , progress(p)
  {
  }
  RowData& row_data;
  DatabaseExport& exporter;
  BareosDb* db{};
  bool is_restore_object{};
  size_t data_field_index_of_restore_object{};
  Progress& progress;
};

static void PrintCopyStartToStdout(const Progress& progress)
{
  if (progress.FullNumberOfRows() != 0) {
    std::cout << "--> copying " << progress.FullAmount() << " rows..."
              << std::endl;
    std::cout << "--> Start: " << Progress::TimeOfDay() << std::endl;
  } else {
    std::cout << "--> nothing to copy..." << std::endl;
  }
}

auto constexpr invalid = std::numeric_limits<std::size_t>::max();

static size_t CalculateDataFieldIndexOfRestoreobject(
    const DatabaseColumnDescriptions::ColumnDescriptions& column_descriptions)
{
  std::size_t data_field_index_of_restore_object = invalid;

  size_t i = 0;
  for (const auto& c : column_descriptions) {
    if (c.second.data_type == "longblob") {
      data_field_index_of_restore_object = i;
      break;
    }
    ++i;
  }
  return data_field_index_of_restore_object;
}


void DatabaseImportMysql::RunQuerySelectAllRows(
    DB_RESULT_HANDLER* ResultHandler,
    DatabaseExport& exporter)
{
  for (const auto& t : table_descriptions_->tables) {
    const TableDescription& table_description = t.second;

    if (!exporter.StartTable(table_description.table_name_lower_case)) {
      std::cout << "--> *** skipping ***" << std::endl;
      continue;
    }

    Progress progress(db_, table_description.table_name, limit_amount_of_rows_);

    PrintCopyStartToStdout(progress);

    std::string query{"SELECT `"};
    for (const auto& col : table_description.column_descriptions) {
      query += col.second.column_name;
      query += "`, `";
    }
    query.resize(query.size() - 3);

    bool is_restore_object = false;
    size_t data_field_index_of_restore_object = invalid;

    if (table_description.table_name == "RestoreObject") {
      query += ", length(`RestoreObject`) as `size_of_restore_object`";
      is_restore_object = true;

      data_field_index_of_restore_object =
          CalculateDataFieldIndexOfRestoreobject(
              table_description.column_descriptions);

      if (data_field_index_of_restore_object == invalid) {
        throw std::runtime_error("No longblob object found as restore object");
      }
    }

    query += " FROM ";
    query += table_description.table_name;

    if (limit_amount_of_rows_ != 0U) {
      query += " LIMIT ";
      query += std::to_string(limit_amount_of_rows_);
    }

    RowData row_data(table_description.table_name_lower_case,
                     table_description.column_descriptions);

    ResultHandlerContext ctx(db_, row_data, exporter, is_restore_object,
                             data_field_index_of_restore_object, progress);

    if (!db_->SqlQuery(query.c_str(), ResultHandler, &ctx)) {
      std::cout << "Could not import table: " << table_description.table_name
                << std::endl;
      std::string err{"Could not execute select statement: "};
      err += query;
      std::cout << query << std::endl;
    }

    exporter.EndTable(table_description.table_name_lower_case);
    std::cout << "--> success" << std::endl;
  }
}

void DatabaseImportMysql::ExportTo(DatabaseExport& exporter)
{
  exporter.CopyStart();
  RunQuerySelectAllRows(ResultHandlerCopy, exporter);
  exporter.CopyEnd();
}

static void ReadoutSizeOfRestoreObject(ResultHandlerContext* r,
                                       RowData& row_data,
                                       int result_index_longblob,
                                       char** row)
{
  if (r->data_field_index_of_restore_object > row_data.data_fields.size() - 1) {
    throw std::runtime_error(
        "data_field_index_of_restore_object out of bounds");
  }
  auto& f = row_data.data_fields[r->data_field_index_of_restore_object];

  std::istringstream(row[result_index_longblob]) >> f.size_of_restore_object;
}


void DatabaseImportMysql::FillRowWithDatabaseResult(ResultHandlerContext* r,
                                                    int number_of_result_fields,
                                                    char** row)
{
  size_t maximum_number_of_fields = r->row_data.column_descriptions.size();

  if (r->is_restore_object) {
    ++maximum_number_of_fields;  // one more for the size_of_restore_object
  }

  if (number_of_result_fields != static_cast<int>(maximum_number_of_fields)) {
    throw std::runtime_error(
        "Number of database fields does not match description");
  }

  RowData& row_data = r->row_data;

  if (r->is_restore_object) {
    std::size_t result_index_longblob = number_of_result_fields - 1;
    ReadoutSizeOfRestoreObject(r, row_data, result_index_longblob, row);
  }

  size_t i = 0;
  for (const auto& c : r->row_data.column_descriptions) {
    r->row_data.data_fields[i].data_pointer = row[i];
    c.second.converter(r->db, row_data.data_fields[i]);
    ++i;
  }
}

int DatabaseImportMysql::ResultHandlerCopy(void* ctx,
                                           int number_of_result_fields,
                                           char** row)
{
  auto* r = static_cast<ResultHandlerContext*>(ctx);
  FillRowWithDatabaseResult(r, number_of_result_fields, row);

  std::string insert_mode_message;
  r->exporter.CopyRow(r->row_data, insert_mode_message);

  constexpr int width_of_rate_column = 7;

  if (r->progress.Increment()) {
    std::cout << std::setw(width_of_rate_column) << r->progress.Rate() << "%"
              << " ETA:" << r->progress.Eta()  // estimated time of arrival
              << " (running:" << r->progress.RunningHours() << ","
              << " remaining:" << r->progress.RemainingHours()
              << insert_mode_message << ")" << std::endl;
  }
  return 0;
}
