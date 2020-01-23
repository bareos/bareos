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
#include "dird/dbconvert/database_table_descriptions.h"
#include "dird/dbconvert/row_data.h"
#include "lib/bstringlist.h"

#include <iostream>
#include <memory>

DatabaseExport::DatabaseExport(const DatabaseConnection& db_connection,
                               bool clear_tables)
    : db_(db_connection.db)
    , table_descriptions_(DatabaseTableDescriptions::Create(db_connection))
{
  if (clear_tables) {
    for (const auto& t : table_descriptions_->tables) {
      if (t.table_name != "version") {
        std::string sequenc_schema_query("DELETE ");
        sequenc_schema_query += " FROM ";
        sequenc_schema_query += t.table_name;
        if (!db_->SqlQuery(sequenc_schema_query.c_str())) {
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

DatabaseExport::~DatabaseExport() = default;

void DatabaseExport::operator<<(const RowData& data)
{
  std::string sequenc_schema_query{"INSERT INTO "};
  sequenc_schema_query += data.table_name;
  sequenc_schema_query += " (";
  for (const auto& r : data.row) {
    sequenc_schema_query += r.first;
    sequenc_schema_query += ", ";
  }
  sequenc_schema_query.erase(sequenc_schema_query.end() - 2);
  sequenc_schema_query += ")";

  sequenc_schema_query += " VALUES (";

  for (const auto& r : data.row) {
    sequenc_schema_query += "'";
    sequenc_schema_query += r.second.data_pointer;
    sequenc_schema_query += "',";
  }

  sequenc_schema_query.erase(sequenc_schema_query.end() - 1);
  sequenc_schema_query += ")";
#if 0
  std::cout << sequenc_schema_query << std::endl;
#else
  if (!db_->SqlQuery(sequenc_schema_query.c_str())) {
    std::string err{"Could not execute sequenc_schema_query: "};
    err += db_->get_errmsg();
    throw std::runtime_error(err);
  }
#endif
}

void DatabaseExport::Start()
{
  //
}

struct SequenceSchema {
  std::string table_name;
  std::string column_name;
  std::string sequence_name;
};


using SequenceSchemaVector = std::vector<SequenceSchema>;

int DatabaseExport::ResultHandler(void* ctx, int fields, char** row)
{
  if (fields != 3) {
    throw std::runtime_error("DatabaseExport: Wrong number of rows");
  }

  SequenceSchema s;
  s.table_name = row[0];
  s.column_name = row[1];

  BStringList l(row[2], "'");
  if (l.size() != 3) {
    throw std::runtime_error("DatabaseExport: Wrong column_default syntax");
  }
  s.sequence_name = l[1];

  SequenceSchemaVector* v = static_cast<SequenceSchemaVector*>(ctx);
  v->push_back(s);
  return 0;
}

void DatabaseExport::End()
{
  const std::string sequenc_schema_query{
      "select table_name, column_name,"
      " column_default from information_schema.columns where table_schema ="
      " 'public' and column_default like 'nextval(%';"};

  SequenceSchemaVector v;

  if (!db_->SqlQuery(sequenc_schema_query.c_str(), ResultHandler, &v)) {
    std::cout << sequenc_schema_query << std::endl;
  }

  for (const auto& s : v) {
    std::string sequence_schema_query{"select setval(' "};
    sequence_schema_query += s.sequence_name;
    sequence_schema_query += "', (select max(";
    sequence_schema_query += s.column_name;
    sequence_schema_query += ") from ";
    sequence_schema_query += s.table_name;
    sequence_schema_query += "))";
    if (!db_->SqlQuery(sequence_schema_query.c_str())) {
      throw std::runtime_error("DatabaseExport: Could not set sequence");
    }
  }
}
