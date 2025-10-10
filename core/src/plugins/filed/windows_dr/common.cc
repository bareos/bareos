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

#include "common.h"

GUID from_disk_format(const guid& id)
{
  static_assert(sizeof(GUID) == sizeof(guid));
  GUID Result;
  memcpy(&Result, &id, sizeof(Result));
  return Result;
}

guid to_disk_format(const GUID& id)
{
  static_assert(sizeof(GUID) == sizeof(guid));

  guid Result;
  memcpy(&Result, &id, sizeof(id));
  return Result;
}
