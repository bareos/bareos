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

#ifndef BAREOS_DIRD_DBCOPY_DATABASE_COLUMN_DESCRIPTIONS_H_
#define BAREOS_DIRD_DBCOPY_DATABASE_COLUMN_DESCRIPTIONS_H_

#include "include/bareos.h"
#include "dird/dbcopy/column_description.h"
#include "cats/cats.h"

#include <string>

class BareosDb;

class DatabaseColumnDescriptions {
 public:
  DatabaseColumnDescriptions(BareosDb* db) : db_{db} {};
  virtual ~DatabaseColumnDescriptions() = default;
  void AddToMap(const char* column_name_in,
                const char* data_type_in,
                const char* max_length_in);

  using ColumnDescriptions = std::map<std::string, ColumnDescription>;

  ColumnDescriptions column_descriptions;

 protected:
  enum RowIndex : int
  {
    kColumnName = 0,
    kDataType = 1,
    kCharMaxLenght = 2
  };
  void SelectColumnDescriptionsAndAddToMap(const std::string& sql_query);

 private:
  BareosDb* db_{};
  static int ResultHandler_(void* ctx, int fields, char** row);
};

class DatabaseColumnDescriptionsPostgresql : public DatabaseColumnDescriptions {
 public:
  DatabaseColumnDescriptionsPostgresql(BareosDb* db,
                                       const std::string& table_name);
};

class DatabaseColumnDescriptionsMysql : public DatabaseColumnDescriptions {
 public:
  DatabaseColumnDescriptionsMysql(BareosDb* db, const std::string& table_name);
};

#endif  // BAREOS_DIRD_DBCOPY_DATABASE_COLUMN_DESCRIPTIONS_H_
