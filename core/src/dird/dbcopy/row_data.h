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
#ifndef BAREOS_DIRD_DBCOPY_ROW_DATA_H_
#define BAREOS_DIRD_DBCOPY_ROW_DATA_H_

#include "include/bareos.h"
#include "cats/column_data.h"
#include "dird/dbcopy/database_column_descriptions.h"

#include <vector>

class BareosDb;

struct RowData {
  RowData(const std::string& table_name_in,
          const DatabaseColumnDescriptions::ColumnDescriptions&
              column_descriptions_in)
      : table_name(table_name_in), column_descriptions(column_descriptions_in)
  {
    data_fields.resize(column_descriptions.size());
  }

  std::string table_name;

  std::vector<DatabaseField> data_fields;  // same index as column_descriptions
  const DatabaseColumnDescriptions::ColumnDescriptions& column_descriptions;

 public:
  ~RowData() = default;
  RowData(const RowData& other) = delete;
  RowData(RowData&& other) = delete;
  const RowData& operator=(const RowData& rhs) = delete;
  const RowData& operator=(RowData&& rhs) = delete;
};

#endif  // BAREOS_DIRD_DBCOPY_ROW_DATA_H_
