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

DatabaseExport::~DatabaseExport() = default;

void DatabaseExport::operator<<(const RowData& data)
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
    std::string err{"Could not execute query: "};
    err += db_->get_errmsg();
    throw std::runtime_error(err);
  }
#endif
}
