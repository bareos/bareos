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
#include "dird/dbconvert/database_import.h"
#include "dird/dbconvert/database_table_descriptions.h"
#include "dird/dbconvert/row_data.h"
#include "dird/dbconvert/database_export.h"

#include <iostream>
#include <cassert>

DatabaseImport::DatabaseImport(const DatabaseConnection& db_connection)
    : db_(db_connection.db)
    , table_descriptions_(DatabaseTableDescriptions::Create(db_connection))
{
  return;
}

DatabaseImport::~DatabaseImport() = default;

struct ResultHandlerContext {
  ResultHandlerContext(
      const DatabaseColumnDescriptions::VectorOfColumnDescriptions& c,
      RowData& d,
      DatabaseExport& e)
      : column_descriptions(c), row_data(d), exporter(e)
  {
  }
  const DatabaseColumnDescriptions::VectorOfColumnDescriptions&
      column_descriptions;
  RowData& row_data;
  DatabaseExport& exporter;
};

void DatabaseImport::ExportTo(DatabaseExport& exporter)
{
  exporter.Start();

  for (const auto& t : table_descriptions_->tables) {
    std::cout << "Converting table: " << t.table_name << std::endl;
    std::string query{"SELECT "};
    for (const auto& col : t.column_descriptions) {
      query += col->column_name;
      query += ", ";
    }
    query.erase(query.cend() - 2);
    query += "FROM ";
    query += t.table_name;
    query += " LIMIT 1000";

    RowData row_data;
    row_data.table_name = t.table_name;
    ResultHandlerContext ctx(t.column_descriptions, row_data, exporter);

    if (!db_->SqlQuery(query.c_str(), ResultHandler, &ctx)) {
      std::cout << "Could not import table: " << t.table_name << std::endl;
      std::string err{"Could not execute select statement: "};
      err += query;
      std::cout << query << std::endl;
    }
  }
  exporter.End();
}

int DatabaseImport::ResultHandler(void* ctx, int fields, char** row)
{
  ResultHandlerContext* r = static_cast<ResultHandlerContext*>(ctx);

  if (fields != static_cast<int>(r->column_descriptions.size())) {
    throw std::runtime_error(
        "Number of database fields does not match description");
  }

  for (int i = 0; i < fields; i++) {
    const std::string& column_name = r->column_descriptions[i]->column_name;

    RowData& row_data = r->row_data;
    row_data.row[column_name].data_pointer = row[i];
    r->column_descriptions[i]->db_import_converter(row_data.row[column_name]);
  }

  r->exporter << r->row_data;

  return 0;
}
