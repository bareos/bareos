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
#include "dird/dbconvert/database_importer.h"
#include "dird/dbconvert/database_table_descriptions.h"

#include <iostream>
#include <cassert>

struct ResultHandlerContext {
  ResultHandlerContext(
      const DatabaseColumnDescriptions::VectorOfColumnDescriptions& c,
      DatabaseImporter::RowDataMap& r)
      : cols(c), one_row_of_data(r)
  {
  }
  const DatabaseColumnDescriptions::VectorOfColumnDescriptions& cols;
  DatabaseImporter::RowDataMap& one_row_of_data;
};

DatabaseImporter::DatabaseImporter(const DatabaseConnection& db_connection)
    : table_descriptions(DatabaseTableDescriptions::Create(db_connection))
{
  for (const auto& t : table_descriptions->tables) {
    std::string query{"SELECT "};
    for (const auto& col : t.column_descriptions) {
      query += col->column_name;
      query += ", ";
    }
    query.erase(query.cend() - 2);
    query += "FROM ";
    query += t.table_name;
    one_row_of_data.clear();
    ResultHandlerContext ctx(t.column_descriptions, one_row_of_data);
    if (!db_connection.db->SqlQuery(query.c_str(), ResultHandler, &ctx)) {
      std::cout << "Could not import table: " << t.table_name << std::endl;
      std::string err{"Could not execute select statement: "};
      err += query;
      std::cout << query << std::endl;
    }
  }
}

int DatabaseImporter::ResultHandler(void* ctx, int fields, char** row)
{
  ResultHandlerContext* r = static_cast<ResultHandlerContext*>(ctx);
  assert(fields == (int)r->cols.size());
  for (int i = 0; i < fields; i++) {
    const std::string& column_name = r->cols[i]->column_name;
    r->one_row_of_data[column_name] = row[i];
  }
  return 0;
}


DatabaseImporter::~DatabaseImporter() = default;
