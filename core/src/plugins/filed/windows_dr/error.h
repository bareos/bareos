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

#ifndef BAREOS_PLUGINS_FILED_WINDOWS_DR_ERROR_H_
#define BAREOS_PLUGINS_FILED_WINDOWS_DR_ERROR_H_

#include <stdexcept>
#include <Windows.h>

struct win_error : std::exception {
  win_error(const char*, DWORD) {}

  const char* what() const { return ""; }
};

struct com_error : std::exception {
  com_error(const char*, HRESULT) {}

  const char* what() const { return ""; }
};

#endif  // BAREOS_PLUGINS_FILED_WINDOWS_DR_ERROR_H_
