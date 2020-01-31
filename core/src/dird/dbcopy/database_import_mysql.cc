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
#include "dird/dbcopy/database_table_descriptions.h"
#include "dird/dbcopy/row_data.h"
#include "dird/dbcopy/database_export.h"
#include "dird/dbcopy/database_import_mysql.h"
#include "include/make_unique.h"

#include <cassert>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <vector>

DatabaseImportMysql::DatabaseImportMysql(
    const DatabaseConnection& db_connection,
    std::size_t maximum_amount_of_rows_in)
    : DatabaseImport(db_connection)
    , maximum_amount_of_rows(maximum_amount_of_rows_in)
{
  return;
}

class Stopwatch {
 public:
  void Start() { start_ = std::chrono::steady_clock::now(); }
  void Stop() { end_ = std::chrono::steady_clock::now(); }
  void PrintDurationToStdout()
  {
    auto duration = end_ - start_;
    auto c(std::chrono::duration_cast<std::chrono::milliseconds>(duration)
               .count());
    std::ostringstream oss;
    oss << std::setfill('0')            // set field fill character to '0'
        << (c % 1000000) / 1000 << "s"  // format seconds
        << "::" << std::setw(3)         // set width of milliseconds field
        << (c % 1000) << "ms";          // format milliseconds

    std::cout << "Duration: " << oss.str() << std::endl;
  }

 private:
  std::chrono::time_point<std::chrono::steady_clock> start_;
  std::chrono::time_point<std::chrono::steady_clock> end_;
};

struct ResultHandlerContext {
  ResultHandlerContext(
      const DatabaseColumnDescriptions::VectorOfColumnDescriptions& c,
      RowData& d,
      DatabaseExport& e,
      BareosDb* db_in,
      bool is_restore_object_in)
      : column_descriptions(c)
      , row_data(d)
      , exporter(e)
      , db(db_in)
      , is_restore_object(is_restore_object_in)
  {
  }
  const DatabaseColumnDescriptions::VectorOfColumnDescriptions&
      column_descriptions;
  RowData& row_data;
  DatabaseExport& exporter;
  uint64_t counter{};
  BareosDb* db{};
  bool is_restore_object{};
};

void DatabaseImportMysql::RunQuerySelectAllRows(
    DB_RESULT_HANDLER* ResultHandler,
    DatabaseExport& exporter)
{
  for (const auto& t : table_descriptions_->tables) {
    if (!exporter.StartTable(t.table_name)) {
      std::cout << "--> *** skipping ***" << std::endl;
      continue;
    }

    std::cout << "--> copying..." << std::endl;

    std::string query{"SELECT `"};
    for (const auto& col : t.column_descriptions) {
      query += col->column_name;
      query += "`, `";
    }
    query.resize(query.size() - 3);

    bool is_restore_object = false;
    if (t.table_name == "RestoreObject") {
      query += ", length(`RestoreObject`) as `length_of_restore_object`";
      is_restore_object = true;
    }

    query += " FROM ";
    query += t.table_name;

    if (maximum_amount_of_rows) {
      query += " LIMIT ";
      query += std::to_string(maximum_amount_of_rows);
    }

    RowData row_data(t.column_descriptions, t.table_name, is_restore_object);
    ResultHandlerContext ctx(t.column_descriptions, row_data, exporter, db_,
                             is_restore_object);

    if (!db_->SqlQuery(query.c_str(), ResultHandler, &ctx)) {
      std::cout << "Could not import table: " << t.table_name << std::endl;
      std::string err{"Could not execute select statement: "};
      err += query;
      std::cout << query << std::endl;
    }

    exporter.EndTable(t.table_name);
    std::cout << "--> success" << std::endl;
    // std::cout << query << std::endl << std::endl;
  }
}

void DatabaseImportMysql::ExportTo(DatabaseExport& exporter)
{
  Stopwatch stopwatch;
  stopwatch.Start();

  exporter.CopyStart();

  RunQuerySelectAllRows(ResultHandlerCopy, exporter);

  exporter.CopyEnd();

  stopwatch.Stop();
#if 0
  stopwatch.PrintDurationToStdout();
#endif
}

void DatabaseImportMysql::CompareWith(DatabaseExport& exporter)
{
  RunQuerySelectAllRows(ResultHandlerCompare, exporter);
}

static void ReadoutSizeOfRestoreObject(ResultHandlerContext* r,
                                       RowData& row_data,
                                       int field_index_longblob,
                                       char** row)
{
  auto invalid = std::numeric_limits<std::size_t>::max();
  std::size_t index_of_restore_object = invalid;

  for (std::size_t i = 0; i < r->column_descriptions.size(); i++) {
    if (r->column_descriptions[i]->data_type == "longblob") {
      index_of_restore_object = i;
      break;
    }
  }

  if (index_of_restore_object == invalid) {
    throw std::runtime_error("No longblob object found as restore object");
  }

  std::istringstream(row[field_index_longblob]) >>
      row_data.columns[index_of_restore_object].size;
}


void DatabaseImportMysql::FillRowWithDatabaseResult(ResultHandlerContext* r,
                                                    int fields,
                                                    char** row)
{
  std::size_t number_of_fields = r->column_descriptions.size();

  if (r->is_restore_object) {
    ++number_of_fields;  // one more for length_of_restore_object
  }

  if (fields != static_cast<int>(number_of_fields)) {
    throw std::runtime_error(
        "Number of database fields does not match description");
  }

  RowData& row_data = r->row_data;

  if (r->is_restore_object) {
    std::size_t field_index_longblob = fields - 1;
    ReadoutSizeOfRestoreObject(r, row_data, field_index_longblob, row);
  }

  for (std::size_t i = 0; i < r->column_descriptions.size(); i++) {
    row_data.columns[i].data_pointer = row[i];
    r->column_descriptions[i]->db_import_converter(r->db, row_data.columns[i]);
  }
}

int DatabaseImportMysql::ResultHandlerCopy(void* ctx, int fields, char** row)
{
  ResultHandlerContext* r = static_cast<ResultHandlerContext*>(ctx);
  FillRowWithDatabaseResult(r, fields, row);

  r->exporter.CopyRow(r->row_data);

  if (!(++r->counter % 10000)) { std::cout << "." << std::flush; }
  return 0;
}

int DatabaseImportMysql::ResultHandlerCompare(void* ctx, int fields, char** row)
{
  ResultHandlerContext* r = static_cast<ResultHandlerContext*>(ctx);
  FillRowWithDatabaseResult(r, fields, row);

  r->exporter.CompareRow(r->row_data);

  if (!(++r->counter % 10000)) { std::cout << "." << std::flush; }
  return 0;
}
