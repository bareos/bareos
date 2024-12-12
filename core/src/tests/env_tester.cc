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

#include <include/bareos.h>

#include "bpipe_env_test.h"

#include <iostream>
#include <optional>

std::optional<std::string> GetEnvValue(const char* key)
{
  const char* env = getenv(key);
  if (!env) { return std::nullopt; }

  return std::string{env};
}

int main()
{
  std::optional val = GetEnvValue(environment_key);

  if (!val) { return 1; }

  std::cout << format(*val) << std::endl;
  return 0;
}
