/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2022-2022 Bareos GmbH & Co. KG

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

#include <algorithm>
#include <cstdarg>
#include <cstdio>

#include "lib/bsnprintf.h"

int Bsnprintf(char* str, int32_t size, const char* format, ...)
{
  va_list arg_ptr;
  int len;

  va_start(arg_ptr, format);
  len = Bvsnprintf(str, size, format, arg_ptr);
  va_end(arg_ptr);
  return len;
}

int Bvsnprintf(char* str, int32_t size, const char* format, va_list args)
{
  return std::min(size, vsnprintf(str, size, format, args));
}
