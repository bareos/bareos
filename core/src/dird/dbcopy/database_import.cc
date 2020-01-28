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
#include "dird/dbcopy/database_import.h"
#include "dird/dbcopy/database_connection.h"
#include "dird/dbcopy/database_table_descriptions.h"
#include "dird/dbcopy/database_import_mysql.h"

DatabaseImport::DatabaseImport(const DatabaseConnection& db_connection)
    : db_(db_connection.db)
    , table_descriptions_(DatabaseTableDescriptions::Create(db_connection))
{
  return;
}

DatabaseImport::~DatabaseImport() = default;

std::unique_ptr<DatabaseImport> DatabaseImport::Create(
    const DatabaseConnection& db_connection,
    std::size_t maximum_amount_of_rows)
{
  switch (db_connection.db_type) {
    case DatabaseType::Enum::kMysql:
      return std::make_unique<DatabaseImportMysql>(db_connection,
                                                   maximum_amount_of_rows);
    default:
    case DatabaseType::Enum::kPostgresql:
      throw std::runtime_error(
          "DatabaseImport: Database type unknown or not implemented");
  }
}
