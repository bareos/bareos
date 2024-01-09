/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2023-2024 Bareos GmbH & Co. KG

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
*/
#include <stdexcept>

#include "device_options.h"
#include "stored/backends/util.h"
#include <cstdint>
#include "lib/edit.h"

namespace dedup {
auto device_option_parser::parse(std::string_view str) -> parse_result
{
  parse_result result;

  auto preparsed = backends::util::parse_options(str);

  if (auto* error = std::get_if<std::string>(&preparsed); error != nullptr) {
    throw std::invalid_argument(*error);
  }

  auto& options = std::get<backends::util::options>(preparsed);

  if (auto iter = options.find("blocksize"); iter != options.end()) {
    auto& val = iter->second;

    std::uint64_t blocksize;
    if (!size_to_uint64(val.data(), &blocksize)) {
      throw std::invalid_argument("bad block size: " + val);
    }

    result.options.blocksize = blocksize;

    options.erase(iter);
  } else {
    result.warnings.emplace_back(
        "Blocksize was not set explicitly;"
        " set to default ("
        + std::to_string(result.options.blocksize) + ").");
  }

  if (options.size() > 0) {
    std::string unknown = "Unknown options: ";
    for (auto [opt, _] : options) {
      unknown += opt;
      unknown += " ";
    }
    result.warnings.emplace_back(std::move(unknown));
  }

  return result;
}
};  // namespace dedup
