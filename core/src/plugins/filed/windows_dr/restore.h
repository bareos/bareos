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

#ifndef BAREOS_PLUGINS_FILED_WINDOWS_DR_RESTORE_H_
#define BAREOS_PLUGINS_FILED_WINDOWS_DR_RESTORE_H_

#include "restore_options.h"
#include <span>

struct data_writer;

data_writer* writer_begin(restore_options options);
std::size_t writer_write(data_writer* writer, std::span<char> data);
void writer_end(data_writer* writer);

#endif  // BAREOS_PLUGINS_FILED_WINDOWS_DR_RESTORE_H_
