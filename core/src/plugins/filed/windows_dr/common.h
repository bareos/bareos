/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2025-2025 Bareos GmbH & Co. KG

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

#ifndef BAREOS_PLUGINS_FILED_WINDOWS_DR_COMMON_H_
#define BAREOS_PLUGINS_FILED_WINDOWS_DR_COMMON_H_

#include "file_format.h"

#include <guiddef.h>
#include <Windows.h>

struct partition_layout {
  partition_info info;
  std::vector<PARTITION_INFORMATION_EX> partition_infos;
};

GUID from_disk_format(const guid& id);
guid to_disk_format(const GUID& id);

// #define DO_DRY 1

#endif  // BAREOS_PLUGINS_FILED_WINDOWS_DR_COMMON_H_
