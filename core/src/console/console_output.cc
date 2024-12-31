/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2018-2024 Bareos GmbH & Co. KG

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

#include "console_output.h"

#include <stdarg.h>

#include "include/bareos.h"

static FILE* output_file_ = stdout;
static bool teeout_enabled_ = false;

void ConsoleOutputFormat(const char* fmt, ...)
{
  PoolMem buf;
  va_list arg_ptr;

  va_start(arg_ptr, fmt);
  auto res = buf.Bvsprintf(fmt, arg_ptr);
  va_end(arg_ptr);
  ASSERT(res >= 0);
  ConsoleOutput(buf.c_str());
}

void ConsoleOutput(const char* buf)
{
  fputs(buf, output_file_);
  fflush(output_file_);
  if (teeout_enabled_) {
    fputs(buf, stdout);
    fflush(stdout);
  }
}

void EnableTeeOut() { teeout_enabled_ = true; }

void DisableTeeOut() { teeout_enabled_ = false; }

void SetTeeFile(FILE* f) { output_file_ = f; }

void CloseTeeFile()
{
  if (output_file_ != stdout) {
    fclose(output_file_);
    SetTeeFile(stdout);
    DisableTeeOut();
  }
}
