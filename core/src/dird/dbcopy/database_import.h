/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2020-2021 Bareos GmbH & Co. KG

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

#ifndef BAREOS_DIRD_DBCOPY_DATABASE_IMPORT_H_
#define BAREOS_DIRD_DBCOPY_DATABASE_IMPORT_H_

class BareosDb;
class DatabaseConnection;
class DatabaseExport;
class DatabaseTableDescriptions;

class DatabaseImport {
 public:
  DatabaseImport(const DatabaseConnection& db_connection);
  virtual ~DatabaseImport();

  virtual void ExportTo(DatabaseExport& exporter) = 0;

  static std::unique_ptr<DatabaseImport> Create(
      const DatabaseConnection& db_connection,
      std::size_t maximum_amount_of_rows);

 protected:
  BareosDb* db_{};

  std::unique_ptr<DatabaseTableDescriptions> table_descriptions_;
};


#endif  // BAREOS_DIRD_DBCOPY_DATABASE_IMPORT_H_
