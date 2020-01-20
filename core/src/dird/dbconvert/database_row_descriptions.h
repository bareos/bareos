/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2016 Planets Communications B.V.
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

#ifndef BAREOS_SRC_DIRD_DBCONVERT_DATABASE_ROW_DESCRIPTIONS_H_
#define BAREOS_SRC_DIRD_DBCONVERT_DATABASE_ROW_DESCRIPTIONS_H_

#include "dird/dbconvert/row_description.h"

#include <string>

class BareosDb;

class DatabaseRowDescriptions {
 public:
  DatabaseRowDescriptions(BareosDb* db);

  std::vector<RowDescription> row_descriptions;

 protected:
  enum RowIndex : int
  {
    kColumnName = 0,
    kDataType = 1,
    kCharMaxLenght = 2
  };
  void SelectTableDescriptions(const std::string& sql_query);

 private:
  BareosDb* db_;
  static int ResultHandler(void* ctx, int fields, char** row);
};

class DatabaseTableDescriptionPostgresl : public DatabaseRowDescriptions {
 public:
  DatabaseTableDescriptionPostgresl(BareosDb* db,
                                    const std::string& table_name);
};

class DatabaseTableDescriptionMysql : public DatabaseRowDescriptions {
 public:
  DatabaseTableDescriptionMysql(BareosDb* db, const std::string& table_name);
};

#endif  // BAREOS_SRC_DIRD_DBCONVERT_DATABASE_ROW_DESCRIPTIONS_H_
