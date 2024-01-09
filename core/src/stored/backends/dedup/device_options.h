/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2023-2024 Bareos GmbH & Co. KG

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation, which is
   listed in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   Affero General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
*/

#ifndef BAREOS_STORED_BACKENDS_DEDUP_DEVICE_OPTIONS_H_
#define BAREOS_STORED_BACKENDS_DEDUP_DEVICE_OPTIONS_H_

#include <string>
#include <string_view>
#include <vector>

namespace dedup {
struct device_options {
  std::size_t blocksize{4096};
};

struct device_option_parser {
  struct parse_result {
    device_options options;
    std::vector<std::string> warnings;
  };

  // throws an exception on error
  static parse_result parse(std::string_view device_option_string);
};
};  // namespace dedup

#endif  // BAREOS_STORED_BACKENDS_DEDUP_DEVICE_OPTIONS_H_
