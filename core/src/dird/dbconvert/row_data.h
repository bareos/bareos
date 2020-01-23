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
#ifndef BAREOS_SRC_DIRD_DBCONVERT_ROW_DATA_H_
#define BAREOS_SRC_DIRD_DBCONVERT_ROW_DATA_H_

#include "include/bareos.h"
#include <vector>

class BareosDb;

struct FieldData {
  const char* data_pointer{};
  std::vector<char> converted_data;
};

struct RowData {
  std::string table_name;
  std::map<std::string, FieldData> row;
};

#endif  // BAREOS_SRC_DIRD_DBCONVERT_ROW_DATA_H_
