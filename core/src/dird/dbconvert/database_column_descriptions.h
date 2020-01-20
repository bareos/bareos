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

#ifndef BAREOS_SRC_DIRD_DBCONVERT_DATABASE_COLUMN_DESCRIPTIONS_H_
#define BAREOS_SRC_DIRD_DBCONVERT_DATABASE_COLUMN_DESCRIPTIONS_H_

#include "include/bareos.h"
#include "dird/dbconvert/column_description.h"
#include "cats/cats.h"

#include <string>

class BareosDb;

class DatabaseColumnDescriptions {
 public:
  DatabaseColumnDescriptions(BareosDb* db);

  std::vector<std::unique_ptr<ColumnDescription>> column_descriptions;

 protected:
  enum RowIndex : int
  {
    kColumnName = 0,
    kDataType = 1,
    kCharMaxLenght = 2
  };
  void SelectTableDescriptions(const std::string& sql_query,
                               DB_RESULT_HANDLER* ResultHandler);

 private:
  BareosDb* db_;
};

class DatabaseColumnDescriptionsPostgresql : public DatabaseColumnDescriptions {
 public:
  DatabaseColumnDescriptionsPostgresql(BareosDb* db,
                                       const std::string& table_name);
  static int ResultHandler(void* ctx, int fields, char** row);
};

class DatabaseColumnDescriptionsMysql : public DatabaseColumnDescriptions {
 public:
  DatabaseColumnDescriptionsMysql(BareosDb* db, const std::string& table_name);
  static int ResultHandler(void* ctx, int fields, char** row);
};

#endif  // BAREOS_SRC_DIRD_DBCONVERT_DATABASE_COLUMN_DESCRIPTIONS_H_
