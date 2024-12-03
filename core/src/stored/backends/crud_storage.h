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
#include <string_view>
#include <unordered_map>
#include <gsl/span>
#include <cstdint>
#include "lib/bstringlist.h"
#include "tl/expected.hpp"

class CrudStorage {
  struct Stat {
    size_t size{0};
  };
  std::string m_program{"/bin/false"};
  uint32_t m_program_timeout{30};
  std::unordered_map<std::string, std::string> m_env_vars{};

 public:
  tl::expected<void, std::string> set_program(const std::string& program);
  void set_program_timeout(uint32_t timeout);
  tl::expected<BStringList, std::string> get_supported_options();
  tl::expected<void, std::string> set_option(const std::string& name,
                                             const std::string& value);
  tl::expected<void, std::string> test_connection();
  tl::expected<Stat, std::string> stat(std::string_view obj_name,
                                       std::string_view obj_part);
  tl::expected<std::map<std::string, Stat>, std::string> list(
      std::string_view obj_name);
  tl::expected<void, std::string> upload(std::string_view obj_name,
                                         std::string_view obj_part,
                                         gsl::span<char> obj_data);
  tl::expected<gsl::span<char>, std::string> download(std::string_view obj_name,
                                                      std::string_view obj_part,
                                                      gsl::span<char> buffer);
  tl::expected<void, std::string> remove(std::string_view obj_name,
                                         std::string_view obj_part);
};
#endif  // BAREOS_STORED_BACKENDS_CRUD_STORAGE_H_
