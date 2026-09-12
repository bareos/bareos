/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2026 Bareos GmbH & Co. KG

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

#ifndef BAREOS_LIB_PROTOCOL_TOKEN_H_
#define BAREOS_LIB_PROTOCOL_TOKEN_H_

#include <optional>
#include <string_view>

constexpr std::optional<std::string_view> GetProtocolToken(
    std::string_view message,
    std::string_view key)
{
  const auto key_position = message.find(key);
  if (key_position == std::string_view::npos) { return std::nullopt; }

  const auto token_start = key_position + key.size();
  if (token_start >= message.size()) { return std::nullopt; }

  const auto token_end = message.find_first_of(" \t\r\n", token_start);
  const auto token = message.substr(token_start, token_end - token_start);
  if (token.empty()) { return std::nullopt; }

  return token;
}

#endif  // BAREOS_LIB_PROTOCOL_TOKEN_H_
