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

#ifndef BAREOS_SRC_DIRD_DBCONVERT_DATABASE_TYPE_H_
#define BAREOS_SRC_DIRD_DBCONVERT_DATABASE_TYPE_H_

class DatabaseType {
 public:
  enum class Enum
  {
    kUndefined,
    kPostgresql,
    kMysql
  };

  static Enum Convert(const std::string& s)
  {
    static const std::map<std::string, DatabaseType::Enum> types{
        {"postgresql", DatabaseType::Enum::kPostgresql},
        {"mysql", DatabaseType::Enum::kMysql}};
    if (types.find(s) == types.cend()) { return Enum::kUndefined; }
    return types.at(s);
  }

  static std::string Convert(Enum e)
  {
    static const std::map<DatabaseType::Enum, std::string> types{
        {DatabaseType::Enum::kPostgresql, "postgresql"},
        {DatabaseType::Enum::kMysql, "mysql"}};
    if (types.find(e) == types.cend()) { return std::string(); }
    return types.at(e);
  }
};

#endif  // BAREOS_SRC_DIRD_DBCONVERT_DATABASE_TYPE_H_
