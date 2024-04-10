/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2022-2024 Bareos GmbH & Co. KG

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

#ifndef BAREOS_WIN32_INCLUDE_FILL_PROC_ADDRESS_H_
#define BAREOS_WIN32_INCLUDE_FILL_PROC_ADDRESS_H_

#include <windows.h>

/*
 * wrap GetProcAddress so that the type of the pointer is deduced and the
 * compiler warning is suppressed. We also return the pointer, so the caller
 * can simply do the following:
 *
 * WinAPI my_func
 * if (!BareosFillProcAddress(my_func, my_module, "my_function_name")) {
 *   // error handling
 * }
 */

template <typename T>
T BareosFillProcAddress(T& func_ptr, HMODULE hModule, LPCSTR lpProcName)
{
  func_ptr = reinterpret_cast<T>(GetProcAddress(hModule, lpProcName));
  return func_ptr;
}

#endif  // BAREOS_WIN32_INCLUDE_FILL_PROC_ADDRESS_H_
