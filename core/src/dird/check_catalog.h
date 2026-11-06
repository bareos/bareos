/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2016 Planets Communications B.V.
   Copyright (C) 2019-2026 Bareos GmbH & Co. KG

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
#ifndef BAREOS_DIRD_CHECK_CATALOG_H_
#define BAREOS_DIRD_CHECK_CATALOG_H_

#include <cstdint>
#include <vector>
#include <string>
#include <optional>

#include "include/compiler_macro.h"

namespace directordaemon {

typedef enum
{
  CHECK_CONNECTION, /* Check catalog connection */
  UPDATE_CATALOG,   /* Ensure that catalog is ok with conf */
  UPDATE_AND_FIX    /* Ensure that catalog is ok, and fix old jobs */
} cat_op;

struct CatalogUtility {
  cat_op operation;
  CatalogUtility(cat_op operation_to_do = CHECK_CONNECTION)
      : operation{operation_to_do}
  {
  }

  void warn(const char* fmt, ...) PRINTF_LIKE(2, 3);
  void err(const char* fmt, ...) PRINTF_LIKE(2, 3);

  std::vector<std::string> warnings;
  std::optional<std::string> error;
};

bool CheckAllCatalogs(CatalogUtility& util);

}  // namespace directordaemon

#endif  // BAREOS_DIRD_CHECK_CATALOG_H_
