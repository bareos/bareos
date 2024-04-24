/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2024-2024 Bareos GmbH & Co. KG

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

#ifndef BAREOS_STORED_BACKENDS_CRUD_STORAGE_H_
#define BAREOS_STORED_BACKENDS_CRUD_STORAGE_H_

#include <map>
#include <optional>
#include <string_view>
#include <gsl/span>

class CrudStorage {
  struct Stat {
    size_t size{0};
  };
  std::string m_program{"/bin/false"};

 public:
  bool set_program(const std::string& program);
  bool test_connection();
  std::optional<Stat> stat(std::string_view obj_name,
                           std::string_view obj_part);
  std::map<std::string, Stat> list(std::string_view /* obj_name */);
  bool upload(std::string_view obj_name,
              std::string_view obj_part,
              gsl::span<char> obj_data);
  std::optional<gsl::span<char>> download(std::string_view obj_name,
                                          std::string_view obj_part,
                                          gsl::span<char> buffer);
  bool remove(std::string_view obj_name, std::string_view obj_part);
};
#endif  // BAREOS_STORED_BACKENDS_CRUD_STORAGE_H_
